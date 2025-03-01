/*
 *  Copyright (c) Huawei Technologies Co., Ltd. 2015-2019. All rights reserved.
 *  Description: eFuse driver
 *  Create : 2015/12/21
 */
#include <linux/version.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <linux/arm-smccc.h>
#else
#include <asm/compiler.h>
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
#include <linux/compat.h>
#else
#include <asm/compat.h>
#endif
#include <linux/compiler.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/cpumask.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <asm/memory.h>
#include <linux/proc_fs.h>
#include <linux/semaphore.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/smp.h>
#include <platform_include/see/efuse_driver.h>
#include <linux/dma-mapping.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <global_ddr_map.h>
#include <securec.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/kconfig.h>
#include <linux/ctype.h>
#if defined(CONFIG_GENERAL_SEE) || defined(CONFIG_CRYPTO_CORE)
#include "platform_include/see/flash_general_see_otp.h"
#endif
#ifdef CONFIG_GENERAL_SEE_SUPPORT_OVERSEA
#include "../general_see/general_see.h"
#endif
#include "socid.h"

#define EFUSE_DEV_NAME                    "efuse"

#define EFUSE_GROUP_MAX_STRING            "efuse_mem_attr_group_max"
#define EFUSE_MEM_START_ADDRESS           "vendor,share-memory-efuse"
#define EFUSE_ATTR_STRING                 "vendor, efuse"

#define EFUSE_FN_GET_KCE                  0xCA000005
#define EFUSE_FN_GET_DIEID                0xCA000020
#define EFUSE_FN_GET_CHIPID               0xCA000021
#define EFUSE_FN_SET_CHIPID               0xCA000022
#define EFUSE_FN_GET_AUTHKEY              0xCA000023
#define EFUSE_FN_SET_AUTHKEY              0xCA000024
#define EFUSE_FN_GET_SECDBG               0xCA000025
#define EFUSE_FN_SET_SECDBG               0xCA000026
#define EFUSE_FN_SET_KCE                  0xCA000029

#ifdef CONFIG_DFX_DEBUG_FS
#define EFUSE_FN_GET_SLTFINISHFLAG        0xCA00002C
#define EFUSE_FN_SET_SLTFINISHFLAG        0xCA00002D
#endif

#if defined(CONFIG_GENERAL_SEE) || defined(CONFIG_CRYPTO_CORE)
#define EFUSE_HISEE_LENGTH_BYTES          8
#define EFUSE_FN_SET_GENERAL_SEE          0xCA00002E
#define EFUSE_FN_GET_GENERAL_SEE          0xCA00002F
#endif
#define EFUSE_FN_GET_AVS                  0xCA000030
#define EFUSE_FN_GET_PARTIAL_PASS_INFO    0xCA000031
#define EFUSE_FN_GET_NVCNT                0xCA000032
#define EFUSE_FN_SET_NVCNT                0xCA000033
#define EFUSE_FN_GET_DESKEW               0xCA000035
#define EFUSE_FN_UPDATE_NVCNT             0xCA000038

/*
 * define for r/w func verification
 * we don't support health level r/w anymore in all platform
 * In order to keep a write template, we still hold the func_id here
 * there is no security problem cause we have already del the atf code
 */
u32 g_wr_check_tbl[] = {
#ifdef CONFIG_DFX_DEBUG_FS
			 EFUSE_FN_ENABLE_DEBUG,
			 EFUSE_FN_DISABLE_DEBUG,
#endif
			 EFUSE_FN_WR_HEALTH_LEVEL };

u32 g_rd_check_tbl[] = {
			 EFUSE_FN_RD_PARTIAL_PASSP2,
			 EFUSE_FN_RD_CHIPLEVEL_TESTFLAG,
			 EFUSE_FN_RD_PARTIALGOOD_FLAG };
/*
 * 40 bytes be enough for these efuse field now, need to make
 * larger if the length of efuse field more than 40!
 */
#define EFUSE_BUFFER_MAX_BYTES            40

/* define SECURE DEBUG MODE value */
#define ARMDEBUG_CTRL_ENABLE_MODE         0x0
#define ARMDEBUG_CTRL_PASSWD_MODE         0x1
#define ARMDEBUG_CTRL_CERT_MODE           0x2
#define ARMDEBUG_CTRL_EFUSE_MODE          0x3

#ifndef ARRAY_LEN
#define ARRAY_LEN(a)                      (sizeof(a) / (sizeof(*(a))))
#endif

#define GET_GROUP_NUM(width_bit)          (((width_bit) + 31) / 32)
#define GET_BYTE_NUM(width_bit)           (GET_GROUP_NUM(width_bit) * 4)

#define GET_BITS_WIDTH(attr_id) \
	g_efusec.efuse_attrs_from_dts[(u32)(attr_id)].bits_width

struct ioctl_efuse_func_t {
	u32 func_id;
	s32 (*rw_func)(u8 *buffer, void __user *argp);
};

struct ioctl_efuse_func_const_t {
	u32 func_id;
	s32 (*rw_func)(u8 *buffer, const void __user *argp);
};

static struct tag_efusec_data g_efusec;

static char *g_efusec_attr[] = {
	"efuse_mem_attr_huk",
	"efuse_mem_attr_scp",
	"efuse_mem_attr_authkey",
	"efuse_mem_attr_chipid",
	"efuse_mem_attr_tsensor_calibration",
	"efuse_mem_attr_huk_rd_disable",
	"efuse_mem_attr_authkey_rd_disable",
	"efuse_mem_attr_dbg_class_ctrl",
	"efuse_mem_attr_dieid",
#ifdef CONFIG_DFX_DEBUG_FS
	"efuse_mem_attr_sltfinishflag",
#endif
	"efuse_mem_attr_modem_avs",
	"efuse_mem_attr_max"
};

static s32 atfd_service_efusec_smc(u64 _function_id, u64 _arg0, u64 _arg1, u64 _arg2)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	struct arm_smccc_res res;
	arm_smccc_smc(_function_id, _arg0, _arg1, _arg2,
			0, 0, 0, 0, &res);
	return (int)res.a0;
#else
	register u64 function_id asm("x0") = _function_id;
	register u64 arg0 asm("x1") = _arg0;
	register u64 arg1 asm("x2") = _arg1;
	register u64 arg2 asm("x3") = _arg2;

	asm volatile(
		__asmeq("%0", "x0")
		__asmeq("%1", "x1")
		__asmeq("%2", "x2")
		__asmeq("%3", "x3")
		"smc    #0\n"
		: "+r" (function_id)
		: "r" (arg0), "r" (arg1), "r" (arg2));

	return (s32)function_id;
#endif
}

static s32 smc_read_efuse(u32 fid, u8 *buf, u32 size, u32 timeout)
{
	s32 ret;

	if (g_efusec.is_init_success != EFUSE_MODULE_INIT_SUCCESS) {
		pr_err("%s: efuse module is not ready now.\n", __func__);
		return -ENODEV;
	}

	mutex_lock(&g_efusec.efuse_mutex);
	ret = g_efusec.atf_fn((u64)fid, (u64)g_efusec.paddr,
			      (u64)size, (u64)timeout);
	if (ret == OK) {
		if (memmove_s((void *)buf, size, (void *)g_efusec.vaddr, size) != EOK) {
			pr_err("memove_s error\n");
			mutex_unlock(&g_efusec.efuse_mutex);
			return -EFAULT;
		}
	}
	mutex_unlock(&g_efusec.efuse_mutex);

	return ret;
}

static s32 smc_write_efuse(u32 fid, u8 *buf, u32 size, u32 timeout)
{
	s32 ret;

	if (g_efusec.is_init_success != EFUSE_MODULE_INIT_SUCCESS) {
		pr_err("%s: efuse module is not ready now.\n", __func__);
		return -ENODEV;
	}

	mutex_lock(&g_efusec.efuse_mutex);
	if (memmove_s((void *)g_efusec.vaddr, size, (void *)buf, size) != EOK) {
		pr_err("memove_s error\n");
		mutex_unlock(&g_efusec.efuse_mutex);
		return -EFAULT;
	}
	ret = g_efusec.atf_fn((u64)fid, (u64)g_efusec.paddr,
			      (u64)size, (u64)timeout);
	mutex_unlock(&g_efusec.efuse_mutex);

	return ret;
}

#if defined(CONFIG_GENERAL_SEE) || defined(CONFIG_CRYPTO_CORE)

s32 get_efuse_hisee_value(u8 *buf, u32 size, u32 timeout)
{
	s32 ret;

	if (!buf || size > EFUSE_HISEE_LENGTH_BYTES || size == 0) {
		pr_err("%s: buf is NULL.\n", __func__);
		return -EFAULT;
	}

	ret = smc_read_efuse(EFUSE_FN_GET_GENERAL_SEE, buf, size, timeout);
	pr_info("%s: ret=%d.\n", __func__, ret);
	return ret;
}

s32 set_efuse_hisee_value(u8 *buf, u32 size, u32 timeout)
{
	s32 ret;

	if (!buf || size > EFUSE_HISEE_LENGTH_BYTES || size == 0) {
		pr_err("%s: buf is NULL.\n", __func__);
		return -EFAULT;
	}

	ret = smc_write_efuse(EFUSE_FN_SET_GENERAL_SEE, buf, size, timeout);
	pr_info("%s: ret=%d.\n", __func__, ret);
	return ret;
}

#endif

s32 get_efuse_dieid_value(u8 *buf, u32 size, u32 timeout)
{
	s32 ret;

	if (!buf || size > EFUSE_DIEID_LENGTH_BYTES || size == 0) {
		pr_err("%s: buf is NULL.\n", __func__);
		return -EFAULT;
	}

	ret = smc_read_efuse(EFUSE_FN_GET_DIEID, buf, size, timeout);
	pr_info("%s: ret=%d.\n", __func__, ret);
	return ret;
}

/* func : get_efuse_deskew_value, provide isp with bit1756 */
s32 get_efuse_deskew_value(u8 *buf, u32 size, u32 timeout)
{
	s32 ret;

	if (!buf || size > EFUSE_DESKEW_LENGTH_BYTES || size == 0) {
		pr_err("%s: buf is NULL.\n", __func__);
		return -EFAULT;
	}

	ret = smc_read_efuse(EFUSE_FN_GET_DESKEW, buf, size, timeout);
	pr_info("%s: ret=%d.\n", __func__, ret);
	return ret;
}

static s32 efuse_param_check(const u32 *buf, const u32 buf_size)
{
	if (!buf) {
		pr_err("%s: buf is NULL.\n", __func__);
		return -EFAULT;
	}
	if (buf_size > EFUSE_BUFFER_MAX_NUM || buf_size == 0) {
		pr_err("%s: buf size is invalid.\n", __func__);
		return -EFAULT;
	}
	return OK;
}

/*
 * general used func for efuse read
 * param:
 *   u32 *buf    :    the source data
 *   u32 buf_size:    is the num of u32
 *   u32 func_id :    the id of efuse item
 */
s32 efuse_read_value(u32 *buf, u32 buf_size, u32 func_id)
{
	s32 ret;
	u32 i, buf_len;
	u8 temp_buf[EFUSE_BUFFER_MAX_BYTES] = {0};
	u8 *p_src = NULL;
	u8 *p_dst = NULL;
	u32 chk_flg = 0;

	if (efuse_param_check(buf, buf_size) != OK) {
		pr_err("%s: params check failed\n", __func__);
		return -EFAULT;
	}
	for (i = 0; i < ARRAY_LEN(g_rd_check_tbl); i++) {
		if (func_id == g_rd_check_tbl[i])
			chk_flg += 1;
	}
	if (chk_flg == 0) {
		pr_err("%s: params check failed\n", __func__);
		return -EFAULT;
	}

	if (g_efusec.is_init_success != EFUSE_MODULE_INIT_SUCCESS) {
		pr_err("%s: efuse module is not ready now.\n", __func__);
		return -ENODEV;
	}

	mutex_lock(&g_efusec.efuse_mutex);
	p_src = g_efusec.vaddr;
	p_dst = (u8 *)buf;
	buf_len = buf_size * sizeof(u32);

	ret = g_efusec.atf_fn(func_id, (u64)g_efusec.paddr,
			      (u64)buf_size, EFUSE_TIMEOUT_SECOND);
	if (ret != OK)
		goto exit;
	for (i = 0; i < buf_len; i++)
		temp_buf[i] = p_src[i];
	for (i = 0; i < buf_len; i++)
		p_dst[i] = temp_buf[i];
exit:
	/* clr shmem */
	(void)memset_s(g_efusec.vaddr, buf_len, 0, buf_len);

	mutex_unlock(&g_efusec.efuse_mutex);
	/* clr temp mem */
	(void)memset_s(temp_buf, sizeof(temp_buf), 0, sizeof(temp_buf));
	pr_info("%s: ret=%d.\n", __func__, ret);
	return ret;
}
EXPORT_SYMBOL(efuse_read_value);

/*
 * general used func for efuse write
 * param:
 *   u32 *buf    :    the source data
 *   u32 buf_size:    the num of u32
 *   u32 func_id :    the id of efuse item
 */
s32 efuse_write_value(u32 *buf, u32 buf_size, u32 func_id)
{
	s32 ret;
	u8 temp_buf[EFUSE_BUFFER_MAX_BYTES] = {0};
	u8 *p_src = NULL;
	u8 *p_dst = NULL;
	u32 i, buf_len;
	u32 chk_flg = 0;

	if (efuse_param_check(buf, buf_size) != OK) {
		pr_err("%s: params check failed\n", __func__);
		return -EFAULT;
	}
	for (i = 0; i < ARRAY_LEN(g_wr_check_tbl); i++) {
		if (func_id == g_wr_check_tbl[i])
			chk_flg += 1;
	}
	if (chk_flg == 0) {
		pr_err("%s: params check failed\n", __func__);
		return -EFAULT;
	}

	if (g_efusec.is_init_success != EFUSE_MODULE_INIT_SUCCESS) {
		pr_err("%s: efuse module is not ready now.\n", __func__);
		return -ENODEV;
	}

	mutex_lock(&g_efusec.efuse_mutex);
	p_src = (u8 *)buf;
	buf_len = buf_size * sizeof(u32);
	p_dst = g_efusec.vaddr;
	for (i = 0; i < buf_len; i++)
		temp_buf[i] = p_src[i];
	for (i = 0; i < buf_len; i++)
		p_dst[i] = temp_buf[i];

	ret = g_efusec.atf_fn(func_id, (u64)g_efusec.paddr,
			      (u64)buf_size, EFUSE_TIMEOUT_SECOND);
	mutex_unlock(&g_efusec.efuse_mutex);
	/* clr temp mem */
	(void)memset_s(temp_buf, sizeof(temp_buf), 0, sizeof(temp_buf));
	/* clr shmem */
	(void)memset_s(g_efusec.vaddr, buf_len, 0, buf_len);
	pr_info("%s: ret=%d.\n", __func__, ret);
	return ret;
}

s32 get_efuse_chipid_value(u8 *buf, u32 size, u32 timeout)
{
	s32 ret;

	if (!buf || size > EFUSE_CHIPID_LENGTH_BYTES || size == 0) {
		pr_err("%s: invalid params.\n", __func__);
		return -EFAULT;
	}

	ret = smc_read_efuse(EFUSE_FN_GET_CHIPID, buf, size, timeout);
	pr_info("%s: ret=%d.\n", __func__, ret);
	return ret;
}

s32 get_partial_pass_info(u8 *buf, u32 size, u32 timeout)
{
	s32 ret;

	if (!buf || size > EFUSE_PARTIAL_PASS_LENGTH_BYTES || size == 0) {
		pr_err("%s: invalid params.\n", __func__);
		return -EFAULT;
	}

	ret = smc_read_efuse(EFUSE_FN_GET_PARTIAL_PASS_INFO, buf, size, timeout);
	pr_info("%s: ret=%d.\n", __func__, ret);
	return ret;
}

static s32 efuse_get_nvcnt(u8 *buf, u32 size)
{
	s32 ret;

	if (!buf) {
		pr_err("%s: buf is NULL.\n", __func__);
		return -EFAULT;
	}

	ret = smc_read_efuse(EFUSE_FN_GET_NVCNT, buf, size, EFUSE_TIMEOUT_SECOND);
	pr_info("%s: ret=%d\n", __func__, ret);
	return ret;
}

static s32 efuse_set_nvcnt(u8 *buf, u32 size)
{
	s32 ret;

	if (!buf) {
		pr_err("%s: buf is NULL.\n", __func__);
		return -EFAULT;
	}

	ret = smc_write_efuse(EFUSE_FN_SET_NVCNT, buf, size, EFUSE_TIMEOUT_SECOND);
	pr_info("%s: ret=%d.\n", __func__, ret);
	return ret;
}
/* write the software verion of xloader's image into both nvcount1 and nvcount2 in efuse,
 * the return value is nvcount1 in efuse
 */
s32 efuse_update_nvcnt(u8 *buf, u32 size)
{
	s32 ret;

	if (!buf) {
		pr_err("%s: buf is NULL.\n", __func__);
		return -EFAULT;
	}

	ret = smc_read_efuse(EFUSE_FN_UPDATE_NVCNT, buf, size, EFUSE_TIMEOUT_SECOND);
	pr_info("%s: nv=%d.\n", __func__, *(uint32_t *)buf);
	return ret;
}

static s32 set_efuse_chipid_value(u8 *buf, u32 size, u32 timeout)
{
	s32 ret;

	if (!buf || size > EFUSE_CHIPID_LENGTH_BYTES || size == 0) {
		pr_err("%s: buf is NULL.\n", __func__);
		return -EFAULT;
	}

	ret = smc_write_efuse(EFUSE_FN_SET_CHIPID, buf, size, timeout);
	pr_info("%s: ret=%d.\n", __func__, ret);
	return ret;
}

static s32 get_efuse_authkey_value(u8 *buf, u32 size, u32 timeout)
{
	s32 ret;

	if (!buf || size > EFUSE_BUFFER_MAX_BYTES || size == 0) {
		pr_err("%s: buf is NULL.\n", __func__);
		return -EFAULT;
	}

	ret = smc_read_efuse(EFUSE_FN_GET_AUTHKEY, buf, size, timeout);
	pr_info("%s: ret=%d.\n", __func__, ret);
	return ret;
}

static s32 set_efuse_authkey_value(u8 *buf, u32 size, u32 timeout)
{
	s32 ret;

	if (!buf || size > EFUSE_BUFFER_MAX_BYTES || size == 0) {
		pr_err("%s: buf is NULL.\n", __func__);
		return -EFAULT;
	}

	ret = smc_write_efuse(EFUSE_FN_SET_AUTHKEY, buf, size, timeout);
	pr_info("%s: ret=%d.\n", __func__, ret);
	return ret;
}

static s32 get_efuse_securitydebug_value(u8 *buf, u32 size, u32 timeout)
{
	s32 ret;

	if (!buf || size > EFUSE_SECDBG_LENGTH_BYTES || size == 0) {
		pr_err("%s: buf is NULL.\n", __func__);
		return -EFAULT;
	}

	ret = smc_read_efuse(EFUSE_FN_GET_SECDBG, buf, size, timeout);
	pr_info("%s: ret=%d\n", __func__, ret);
	return ret;
}

static s32 ioctl_get_nvcnt(u8 *buffer, void __user *argp)
{
	s32 size = EFUSE_NVCNT_LENGTH_BYTES;
	s32 ret;
	if (!argp) {
		pr_err("%s: argp NULL", __func__);
		return ERROR;
	}
	ret = efuse_get_nvcnt(buffer, size);
	if (ret != OK) {
		pr_err("%s: efuse_get_nvcnt failed,ret=%d\n",
		       __func__, ret);
		return ERROR;
	}

	if (copy_to_user(argp, buffer, size))
		return ERROR;
	return OK;
}

/* update both trb_nv and ntrb_nv */
static s32 ioctl_update_nvcnt(u8 *buffer, void __user *argp)
{
	s32 size = EFUSE_NVCNT_LENGTH_BYTES;
	s32 ret;

	if (!argp) {
		pr_err("%s: argp NULL", __func__);
		return ERROR;
	}
	ret = efuse_update_nvcnt(buffer, size);
	if (ret != OK) {
		pr_err("%s: update nvcnt failed,ret=%d\n",
		       __func__, ret);
		return ERROR;
	}

	if (copy_to_user(argp, buffer, size))
		return ERROR;
	return OK;
}

/* only update trb_nv */
static s32 ioctl_set_nvcnt(u8 *buffer, const void __user *argp)
{
	s32 size = EFUSE_NVCNT_LENGTH_BYTES;
	s32 ret;
	if (!argp) {
		pr_err("%s: argp NULL", __func__);
		return ERROR;
	}

	if (copy_from_user(buffer, argp, size))
		return ERROR;

	ret = efuse_set_nvcnt(buffer, size);
	if (ret != OK) {
		pr_err("%s: efuse_set_nvcnt failed,ret=%d\n",
		       __func__, ret);
		return ERROR;
	}

	return OK;
}

/* check secret_mode is disabled mode. Mode 0 will also write general see otp1 */
static bool efuse_secdebug_is_disabled(u32 secret_mode)
{
	if (secret_mode == ARMDEBUG_CTRL_ENABLE_MODE ||
	    secret_mode == ARMDEBUG_CTRL_PASSWD_MODE ||
	    secret_mode == ARMDEBUG_CTRL_CERT_MODE ||
	    secret_mode == ARMDEBUG_CTRL_EFUSE_MODE)
		return true;
	return false;
}

/* check secure debug efuse is disabled */
s32 efuse_check_secdebug_disable(bool *b_disabled)
{
	u32 sec_debug_value;
	s32 ret;

	if (!b_disabled)
		return -EINVAL;

	sec_debug_value = ARMDEBUG_CTRL_ENABLE_MODE;

	ret = get_efuse_securitydebug_value((u8 *)&sec_debug_value,
					    sizeof(sec_debug_value),
					    EFUSE_TIMEOUT_SECOND);
	if (ret != OK)
		return ERROR;

	*b_disabled = efuse_secdebug_is_disabled(sec_debug_value);
	return OK;
}

static s32 set_efuse_securitydebug_value(u8 *buf, u32 size, u32 timeout)
{
	s32 ret;

	if (!buf || size > EFUSE_SECDBG_LENGTH_BYTES || size == 0) {
		pr_err("%s: buf is NULL.\n", __func__);
		return -EFAULT;
	}

	if (g_efusec.is_init_success != EFUSE_MODULE_INIT_SUCCESS) {
		pr_err("%s: efuse module is not ready now.\n", __func__);
		return -ENODEV;
	}

	mutex_lock(&g_efusec.efuse_mutex);

#ifdef CONFIG_GENERAL_SEE_SUPPORT_OVERSEA
	/*
	 * set smx flag in atf if is oversea version(has cos1)
	 * so atf will write efuse to disable hieps smx later
	 */
	ret = hisee_write_smx_cfg();
	if (ret != HISEE_OK) {
		pr_info("%s: write smx failed, ret=%d.\n", __func__, ret);
		goto end;
	}
#endif

	if (memcpy_s((void *)g_efusec.vaddr, size, (void *)buf, size) != EOK) {
		pr_err("[%s]: memove_s error\n", __func__);
		mutex_unlock(&g_efusec.efuse_mutex);
		return -EFAULT;
	}

	ret = g_efusec.atf_fn(EFUSE_FN_SET_SECDBG, (u64)g_efusec.paddr,
			      (u64)size, (u64)timeout);
#if defined(CONFIG_GENERAL_SEE) || defined(CONFIG_CRYPTO_CORE)
	/* start hisee otp1 write task, if secure debug is disabled */
	if (ret == OK && efuse_secdebug_is_disabled(*(u32 *)buf) == true) {
		pr_err("debug efuse set %x start otp\n", (*(u32 *)buf));
		/* start otp1 flash stask for hisee pinstall task */
		creat_flash_otp_thread();
	}
#endif

#ifdef CONFIG_GENERAL_SEE_SUPPORT_OVERSEA
end:
#endif
	mutex_unlock(&g_efusec.efuse_mutex);
	pr_info("%s: ret=%d.\n", __func__, ret);

	return ret;
}

s32 get_efuse_avs_value(u8 *buf, u32 size, u32 timeout)
{
	u32 bits_width, bytes;
	s32 ret;

	bits_width = GET_BITS_WIDTH(EFUSE_MEM_ATTR_MODEM_AVS);
	bytes = EFUSE_AVS_LENGTH_BYTES(bits_width);
	if (!buf || bytes > EFUSE_AVS_MAX_LENGTH_BYTES || size < bytes) {
		pr_err("%s: avs bytes is too large or buff is NULL\n",
		       __func__);
		return -EFAULT;
	}

	ret = smc_read_efuse(EFUSE_FN_GET_AVS, buf, bytes, timeout);
	pr_info("%s: ret=%d.\n", __func__, ret);
	return ret;
}
EXPORT_SYMBOL(get_efuse_avs_value);

/* not delete here to make sure someone else may call */
s32 get_efuse_kce_value(u8 *buf, u32 size,
			u32 timeout)
{
	s32 ret = OK;

	if (g_efusec.is_init_success != EFUSE_MODULE_INIT_SUCCESS) {
		pr_err("%s: efuse module is not ready now.\n", __func__);
		return -ENODEV;
	}
	pr_info("%s: ret=%d.\n", __func__, ret);
	return ret;
}

s32 set_efuse_kce_value(u8 *buf, u32 size, u32 timeout)
{
	s32 ret;

	if (!buf || size != EFUSE_KCE_LENGTH_BYTES) {
		pr_err("%s: buf is NULL.\n", __func__);
		return -EFAULT;
	}

	ret = smc_write_efuse(EFUSE_FN_SET_KCE, buf, size, timeout);
	pr_info("%s: ret=%d.\n", __func__, ret);
	return ret;
}

#ifdef CONFIG_DFX_DEBUG_FS
static s32 get_efuse_sltfinishflag_value(u8 *buf, u32 size, u32 timeout)
{
	s32 ret;

	if (!buf || size > EFUSE_SLTFINISHFLAG_LENGTH_BYTES || size == 0) {
		pr_err("%s: buf is NULL.\n", __func__);
		return -EFAULT;
	}

	ret = smc_read_efuse(EFUSE_FN_GET_SLTFINISHFLAG, buf, size, timeout);
	pr_info("%s: ret=%d.\n", __func__, ret);
	return ret;
}

static s32 set_efuse_sltfinishflag_value(u8 *buf, u32 size, u32 timeout)
{
	s32 ret;

	if (!buf || size > EFUSE_SLTFINISHFLAG_LENGTH_BYTES || size == 0) {
		pr_err("%s: buf is NULL.\n", __func__);
		return -EFAULT;
	}

	ret = smc_write_efuse(EFUSE_FN_SET_SLTFINISHFLAG, buf, size, timeout);
	pr_info("%s: ret=%d.\n", __func__, ret);
	return ret;
}
#endif

/* buffer is a stack array pointer in this static func */
static s32 ioctl_read_chipid(u8 *buffer, void __user *argp)
{
	u32 bits_width, bytes;
	s32 ret;

	bits_width = GET_BITS_WIDTH(EFUSE_MEM_ATTR_CHIPID);
	bytes = GET_BYTE_NUM(bits_width);

	if (bytes > EFUSE_CHIPID_LENGTH_BYTES || !argp) {
		pr_err("%s: chipid bytes is too large or argp NULL", __func__);
		return ERROR;
	}

	ret = get_efuse_chipid_value(buffer, bytes,
				     EFUSE_TIMEOUT_SECOND);
	if (ret != OK) {
		pr_err("%s: get_efuse_chipid_value failed,ret=%d\n",
		       __func__, ret);
		return ret;
	}

	if (copy_to_user(argp, buffer, bytes))
		return ERROR;
	return OK;
}

static s32 ioctl_write_chipid(u8 *buffer, const void __user *argp)
{
	u32 bits_width, bytes;
	s32 ret;

	bits_width = GET_BITS_WIDTH(EFUSE_MEM_ATTR_CHIPID);
	bytes = GET_BYTE_NUM(bits_width);

	if (bytes > EFUSE_CHIPID_LENGTH_BYTES || !argp) {
		pr_err("%s: chipid bytes is too large or argp NULL", __func__);
		return ERROR;
	}

	if (copy_from_user(buffer, argp, bytes))
		return ERROR;

	ret = set_efuse_chipid_value(buffer, bytes,
				     EFUSE_TIMEOUT_SECOND);
	if (ret != OK) {
		pr_err("%s: efuse_set_chipID,ret=%d\n", __func__, ret);
		return ERROR;
	}
	return OK;
}

static s32 ioctl_read_chipid_len(u8 *buffer, void __user *argp)
{
	u32 bits_width;
	(void)buffer;

	if (!argp) {
		pr_err("%s: Invalid param argp", __func__);
		return ERROR;
	}

	bits_width = GET_BITS_WIDTH(EFUSE_MEM_ATTR_CHIPID);

	if (copy_to_user(argp, &bits_width, sizeof(bits_width)))
		return ERROR;
	return OK;
}

static s32 ioctl_read_authkey(u8 *buffer, void __user *argp)
{
	u32 bits_width, bytes;
	s32 ret;

	bits_width = GET_BITS_WIDTH(EFUSE_MEM_ATTR_AUTHKEY);
	bytes = GET_BYTE_NUM(bits_width);

	/* bytes depends on DTS. It's external module thus is untrusted. */
	if (bytes > EFUSE_BUFFER_MAX_BYTES || !argp) {
		pr_err("%s:authkey bytes is too large or argp NULL", __func__);
		return ERROR;
	}

	ret = get_efuse_authkey_value(buffer, bytes,
				      EFUSE_TIMEOUT_SECOND);
	if (ret != OK) {
		pr_err("%s: get_efuse_authkey_value failed, ret=%d\n",
		       __func__, ret);
		return ERROR;
	}

	if (copy_to_user(argp, buffer, bytes))
		return ERROR;
	return OK;
}

static s32 ioctl_write_authkey(u8 *buffer, const void __user *argp)
{
	u32 bits_width, bytes;
	s32 ret;

	bits_width = GET_BITS_WIDTH(EFUSE_MEM_ATTR_AUTHKEY);
	bytes = GET_BYTE_NUM(bits_width);

	if (bytes > EFUSE_BUFFER_MAX_BYTES || !argp) {
		pr_err("%s:authkey bytes is too large or argp NULL", __func__);
		return ERROR;
	}

	if (copy_from_user(buffer, argp, bytes))
		return ERROR;

	ret = set_efuse_authkey_value(buffer, bytes,
				      EFUSE_TIMEOUT_SECOND);
	if (ret != OK) {
		pr_err("%s: set_efuse_authkey_value failed,ret=%d\n",
		       __func__, ret);
		return ERROR;
	}
	return OK;
}

static s32 ioctl_read_dbgmode(u8 *buffer, void __user *argp)
{
	u32 bits_width, bytes;
	s32 ret;

	bits_width = GET_BITS_WIDTH(EFUSE_MEM_ATTR_HUK_RD_DISABLE);
	bytes = GET_BYTE_NUM(bits_width);

	if (bytes > EFUSE_SECDBG_LENGTH_BYTES || !argp) {
		pr_err("%s: secdbg bytes is too large or argp NULL", __func__);
		return ERROR;
	}

	ret = get_efuse_securitydebug_value(buffer, bytes,
					    EFUSE_TIMEOUT_SECOND);
	if (ret != OK) {
		pr_err("%s: efuse_get_SecureDebugMode failed,ret=%d\n",
		       __func__, ret);
		return ERROR;
	}

	if (copy_to_user(argp, buffer, bytes))
		return ERROR;
	return OK;
}

static s32 ioctl_write_dbgmode(u8 *buffer, const void __user *argp)
{
	u32 bits_width, bytes;
	s32 ret;

	bits_width = GET_BITS_WIDTH(EFUSE_MEM_ATTR_HUK_RD_DISABLE);
	bytes = GET_BYTE_NUM(bits_width);

	if (bytes > EFUSE_SECDBG_LENGTH_BYTES || !argp) {
		pr_err("%s: secdbg bytes is too large or argp NULL", __func__);
		return ERROR;
	}

	if (copy_from_user(buffer, argp, bytes))
		return ERROR;

	ret = set_efuse_securitydebug_value(buffer, bytes,
					    EFUSE_TIMEOUT_SECOND);
	if (ret != OK) {
		pr_err("%s: efuse_set_SecureDebugMode failed,ret=%d\n",
		       __func__, ret);
		return ERROR;
	}
	return OK;
}

static s32 ioctl_read_dieid(u8 *buffer, void __user *argp)
{
	u32 bits_width, bytes;
	s32 ret;

	bits_width = GET_BITS_WIDTH(EFUSE_MEM_ATTR_DIEID);
	bytes = GET_BYTE_NUM(bits_width);

	if (bytes > EFUSE_DIEID_LENGTH_BYTES || !argp) {
		pr_err("%s: dieid bytes is too large or argp NULL", __func__);
		return ERROR;
	}

	ret = get_efuse_dieid_value(buffer, bytes,
				    EFUSE_TIMEOUT_SECOND);
	if (ret != OK) {
		pr_err("%s: get_efuse_dieid_value failed,ret=%d\n",
		       __func__, ret);
		return ERROR;
	}

	if (copy_to_user(argp, buffer, bytes))
		return ERROR;
	return OK;
}

static s32 ioctl_read_socid(u8 *buffer, void __user *argp)
{
	s32 ret;
	u8 buf[EFUSE_SOCID_LENGTH_BYTES] = {0};

	if (!argp) {
		pr_err("%s: argp NULL", __func__);
		return ERROR;
	}

	ret = get_socid(buf, sizeof(buf));
	if (ret != OK) {
		pr_err("%s: get_efuse_socid_value failed,ret=%d\n",
		       __func__, ret);
		return ERROR;
	}

	if (copy_to_user(argp, buf, EFUSE_SOCID_LENGTH_BYTES))
		return ERROR;
	return OK;
}

#ifdef CONFIG_DFX_DEBUG_FS
static s32 ioctl_read_sltfinishflag(u8 *buffer, void __user *argp)
{
	u32 i, bits_width, bytes;
	s32 ret;

	bits_width = GET_BITS_WIDTH(EFUSE_MEM_ATTR_SLTFINISHFLAG);
	bytes = GET_BYTE_NUM(bits_width);

	if (bytes > EFUSE_SLTFINISHFLAG_LENGTH_BYTES || !argp) {
		pr_err("%s:sltfinishflag bytes is too large or argp NULL",
		       __func__);
		return ERROR;
	}

	ret = get_efuse_sltfinishflag_value(buffer, bytes,
					    EFUSE_TIMEOUT_SECOND);
	if (ret != OK) {
		pr_err("%s: get_efuse_sltfinishflag_value failed, ret=%d.\n",
		       __func__, ret);
		return ERROR;
	}

	for (i = 0; i < bytes; i++) {
		pr_err("%s: get_efuse_sltfinishflag_value[%d] = %2x.\n",
		       __func__, i, buffer[i]);
	}

	if (copy_to_user(argp, buffer, bytes))
		return ERROR;
	return OK;
}

static s32 ioctl_write_sltfinishflag(u8 *buffer, const void __user *argp)
{
	u32 i, bits_width, bytes;
	s32 ret;

	bits_width = GET_BITS_WIDTH(EFUSE_MEM_ATTR_SLTFINISHFLAG);
	bytes = GET_BYTE_NUM(bits_width);

	if (bytes > EFUSE_SLTFINISHFLAG_LENGTH_BYTES || !argp) {
		pr_err("%s:sltfinishflag bytes is too large or argp NULL",
		       __func__);
		return ERROR;
	}

	if (copy_from_user(buffer, argp, bytes))
		return ERROR;

	for (i = 0; i < bytes; i++) {
		pr_err("%s:set_efuse_sltfinishflag_value[%d] = %2x.\n",
		       __func__, i, buffer[i]);
	}

	ret = set_efuse_sltfinishflag_value(buffer, bytes,
					    EFUSE_TIMEOUT_SECOND);
	if (ret != OK) {
		pr_err("%s: set_efuse_sltfinishflag_value failed, ret=%d.\n",
		       __func__, ret);
		return ERROR;
	}
	return OK;
}
#endif

static const struct ioctl_efuse_func_t g_ioctl_func_table[] = {
	{(u32)EFUSE_READ_CHIPID, ioctl_read_chipid},
	{(u32)EFUSE_READ_CHIPIDLEN, ioctl_read_chipid_len},
	{(u32)EFUSE_READ_DIEID, ioctl_read_dieid},
	{(u32)EFUSE_READ_AUTHKEY, ioctl_read_authkey},
	{(u32)EFUSE_READ_DEBUGMODE, ioctl_read_dbgmode},
#ifdef CONFIG_DFX_DEBUG_FS
	{(u32)EFUSE_READ_SLTFINISHFLAG, ioctl_read_sltfinishflag},
#endif
	{(u32)EFUSE_GET_NVCNT, ioctl_get_nvcnt},
	{(u32)EFUSE_READ_SOCID, ioctl_read_socid},
	{(u32)EFUSE_UPDATE_NVCNT, ioctl_update_nvcnt},
};

static const struct ioctl_efuse_func_const_t g_ioctl_func_const_tbl[] = {
	{(u32)EFUSE_WRITE_CHIPID, ioctl_write_chipid},
	{(u32)EFUSE_WRITE_AUTHKEY, ioctl_write_authkey},
	{(u32)EFUSE_WRITE_DEBUGMODE, ioctl_write_dbgmode},
#ifdef CONFIG_DFX_DEBUG_FS
	{(u32)EFUSE_WRITE_SLTFINISHFLAG, ioctl_write_sltfinishflag},
#endif
	{(u32)EFUSE_SET_NVCNT, ioctl_set_nvcnt},
};

/*
 * Function name:efusec_ioctl.
 * Discription:complement read/write efuse by terms of sending command-words.
 * return value:
 *          @ 0 - success.
 *          @ -1- failure.
 */
static long efusec_ioctl(struct file *file, u32 cmd, unsigned long arg)
{
	s32 ret = OK;
	void __user *argp = (void __user *)(uintptr_t)arg;
	u32 i, j;
	u32 table_size = ARRAY_LEN(g_ioctl_func_table);
	u32 tbl_const_size = ARRAY_LEN(g_ioctl_func_const_tbl);

	u32 read_buf[EFUSE_BUFFER_MAX_BYTES / sizeof(u32)] = { 0 };
	u8 *buffer = (u8 *)read_buf;

	pr_info("%s: cmd=0x%x.\n", __func__, cmd);
	if (!argp)
		return -EFAULT;

	/* find out efuse read func */
	for (i = 0; i < table_size; i++) {
		if (g_ioctl_func_table[i].func_id == cmd &&
		    g_ioctl_func_table[i].rw_func) {
			if (g_ioctl_func_table[i].rw_func(buffer, argp) != OK) {
				pr_err("[EFUSE][%s] ioctl failed, id is %d\n",
				       __func__, g_ioctl_func_table[i].func_id);
				return -EFAULT;
			}
			return OK;
		}
	}

	/* find out efuse write func */
	for (j = 0; j < tbl_const_size; j++) {
		if (g_ioctl_func_const_tbl[j].func_id == cmd &&
		    g_ioctl_func_const_tbl[j].rw_func) {
			if (g_ioctl_func_const_tbl[j].rw_func(buffer, argp) !=
				OK) {
				pr_err("[EFUSE][%s] ioctl failed, id is %d\n",
				       __func__,
				       g_ioctl_func_const_tbl[j].func_id);
				return -EFAULT;
			}
			return OK;
		}
	}

	if (i + j == table_size + tbl_const_size) {
		pr_err("[EFUSE][%s] Unknown command!\n", __func__);
		ret = -ENOTTY;
	}
	return ret;
}

static long efusec_compat_ioctl(struct file *file, u32 cmd, unsigned long arg)
{
	void __user *arg64 = compat_ptr(arg);

	if (!file->f_op) {
		pr_err("%s,file->f_op is NULL\n", __func__);
		return -ENOTTY;
	}

	if (!file->f_op->unlocked_ioctl) {
		pr_err("%s,file->f_op->unlocked_ioctl is NULL\n", __func__);
		return -ENOTTY;
	}

	return file->f_op->unlocked_ioctl(file, cmd,
					  (unsigned long)(uintptr_t)arg64);
}

/*
 * 32bit app & 32bit kernel, app ioctl will callback unlocked_ioctl
 * 64bit app & 64bit kernel, app ioctl will callback unlocked_ioctl
 * 32bit app & 64bit kernel, app ioctl will callback compat_ioctl
 * 64bit app & 32bit kernel, illegal
 */
static const struct file_operations g_efusec_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = efusec_ioctl,
	.compat_ioctl = efusec_compat_ioctl,
};

/*
 * currently, parse the dts configurations follow the strategy:
 * 1.Assume the values in correspondings attribution is correct,
 *   so the values in DTS must be filled correctly;
 * 2.Assume to parse the attributions one by one according to
 *   g_efusec_attr[] until one does not exist, treat it as failure,
 *   so these attributions contained in g_efusec_attr[] must be
 *   configured in dts;
 * 3.if the value are 0xffffffff, indicate the attribution is
 *   reserved for future use, it's process depend on software.
 *   A example is the dieid attribution.
 */
static s32 efuse_get_attr_from_dts(void)
{
	u32 i = 0;
	s32 ret;
	u32 bit_width = 0;
	struct device_node *np = NULL;

	np = of_find_compatible_node(NULL, NULL, EFUSE_ATTR_STRING);
	if (!np) {
		pr_err("%s: No efusec compatible node found.\n", __func__);
		ret = -ENODEV;
		goto error0;
	}

	ret = of_property_read_u32(np, EFUSE_GROUP_MAX_STRING,
				   &g_efusec.efuse_group_max);
	if (ret != OK) {
		pr_err("%s %s in efusec compatible node\n",
		       EFUSE_GROUP_MAX_STRING,
		       (ret == -EINVAL) ? "not exist" : "has invalid value");
		goto error0;
	}

	/* ARRAY_LEN(g_efusec_attr) - 1 : the -1 is for  EFUSE_MEM_ATTR_MAX */
	for (i = 0; i < ARRAY_LEN(g_efusec_attr) - 1; i++) {
		ret = of_property_read_u32(np, g_efusec_attr[i], &bit_width);
		if (ret != OK) {
			pr_err("%s %s in efusec compatible node\n",
			       g_efusec_attr[i],
			       (ret == -EINVAL) ?
			       "does not exist" : "has invalid value");
			goto error0;
		}

		if (i < ARRAY_LEN(g_efusec.efuse_attrs_from_dts)) {
			g_efusec.efuse_attrs_from_dts[i].bits_width = bit_width;
		} else {
			pr_err("%s: i = 0x%x is out-of-bound\n", __func__, i);
			ret = -EFAULT;
			goto error0;
		}

		pr_info("%d: %s in efusec compatible node value::%d\n",
			i, g_efusec_attr[i], bit_width);
	}

	pr_info("%s success\n", __func__);

error0:
	return ret;
}

/* get share memory between kernel and atf */
static s32 efuse_get_share_mem_from_dts(void)
{
	s32 ret = -EFAULT;
	struct device_node *np = NULL;
	/*
	 * data[0] is the offset of physical address
	 * data[1] is the size of share memory
	 */
	u32 data[2] = { 0 };
	phys_addr_t shm_base = ATF_SUB_RESERVED_BL31_SHARE_MEM_PHYMEM_BASE;

	np = of_find_compatible_node(NULL, NULL, EFUSE_MEM_START_ADDRESS);
	if (!np) {
		pr_err("%s: No share-memory-efuse compatible node found.\n",
		       __func__);
		goto error0;
	}

	ret = of_property_read_u32_array(np, "reg", &data[0], 2);
	if (ret != OK) {
		pr_err("reg %s in share-memory-efuse compatible node.\n",
		       (ret == -EINVAL) ? "not exist" : "has invalid value");
		goto error0;
	}

	g_efusec.paddr = shm_base + data[0];
	if (data[1] < EFUSE_BUFFER_MAX_BYTES) {
		ret = -EFAULT;
		pr_err("%s: share memory is too small.\n", __func__);
		goto error0;
	}

	g_efusec.vaddr = (u8 *)ioremap(shm_base + data[0],
				       data[1]);
	if (!g_efusec.vaddr) {
		ret = -EFAULT;
		pr_err("%s: memory for g_efusec.vaddr failed.\n", __func__);
		goto error0;
	}

	pr_info("%s success\n", __func__);
error0:
	return ret;
}

static void efuse_set_atf_invoke(void)
{
	g_efusec.atf_fn = atfd_service_efusec_smc;
}

static s32 efuse_create_device_node(void)
{
	s32 ret = OK;
	s32 major;
	struct class *pclass = NULL;
	struct device *pdevice = NULL;

	major = register_chrdev(0, EFUSE_DEV_NAME, &g_efusec_fops);
	if (major <= 0) {
		ret = -EFAULT;
		pr_err("%s: unable to get major.\n", __func__);
		goto error0;
	}

	pclass = class_create(THIS_MODULE, EFUSE_DEV_NAME);
	if (IS_ERR(pclass)) {
		ret = -EFAULT;
		pr_err("%s: class_create error.\n", __func__);
		goto error1;
	}

	pdevice = device_create(pclass, NULL, MKDEV((u32)major, 0),
				NULL, EFUSE_DEV_NAME);
	if (IS_ERR(pdevice)) {
		ret = -EFAULT;
		pr_err("%s: device_create error.\n", __func__);
		goto error2;
	}

	pr_info("%s success\n", __func__);
	return ret;

error2:
	class_destroy(pclass);
error1:
	unregister_chrdev(major, EFUSE_DEV_NAME);
error0:
	return ret;
}

static s32 __init efuse_driver_init(void)
{
	s32 ret = 0;

	ret = memset_s(&g_efusec, sizeof(g_efusec), 0, sizeof(g_efusec));
	if (ret != EOK) {
		pr_err("memset_s error\n");
		return -EFAULT;
	}
	ret = efuse_get_attr_from_dts();
	if (ret != OK)
		return ret;

	ret = efuse_get_share_mem_from_dts();
	if (ret != OK)
		return ret;

	efuse_set_atf_invoke();

	ret = efuse_create_device_node();
	if (ret != OK)
		return ret;

	mutex_init(&g_efusec.efuse_mutex);

	g_efusec.is_init_success = EFUSE_MODULE_INIT_SUCCESS;

	pr_info("%s success\n", __func__);
	return ret;
}

rootfs_initcall(efuse_driver_init);

MODULE_DESCRIPTION("efuse module");
MODULE_LICENSE("GPL");
