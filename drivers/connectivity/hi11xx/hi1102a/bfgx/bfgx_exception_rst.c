

/* 头文件包含 */
#include "bfgx_exception_rst.h"
#include <linux/delay.h>
#include <linux/rtc.h>
#include <linux/tty.h>

#include "plat_type.h"
#include "plat_debug.h"
#include "hw_bfg_ps.h"
#include "plat_pm.h"
#include "plat_pm_wlan.h"
#include "bfgx_exception_rst.h"
#include "plat_firmware.h"
#include "plat_uart.h"
#include "oal_sdio_host_if.h"
#include "oal_hcc_host_if.h"
#include "oal_ext_if.h"
#include <linux/ktime.h>
#include "chr_errno.h"
#include "oam_rdr.h"
#include "securec.h"

/* 全局变量定义 */
struct st_exception_info *g_exception_info = NULL;
struct sdio_dump_bcpu_buff g_bcpu_dump_buff = { NULL, 0, 0 };
oal_netbuf_stru *g_bcpu_dump_netbuf = NULL;

#if ((_PRE_PRODUCT_ID != _PRE_PRODUCT_ID_HI1103_HOST) && (_PRE_PRODUCT_ID != _PRE_PRODUCT_ID_HI1102A_HOST))
uint8 *g_bfgx_mem_file_name[BFGX_MEM_DUMP_BLOCK_COUNT] = {
    "plat_nfc_stack",
    "fgb_stack",
    "bt_extra_mem",
};

struct st_exception_mem_info g_bfgx_mem_dump[BFGX_MEM_DUMP_BLOCK_COUNT] = {{0}, {0}, {0}};
#else
uint8 *g_bfgx_mem_file_name[BFGX_MEM_DUMP_BLOCK_COUNT] = {
    "bcpu_itcm_mem",
    "bcpu_dtcm_mem",
};

struct st_exception_mem_info g_bfgx_mem_dump[BFGX_MEM_DUMP_BLOCK_COUNT] = {{0}, {0}};
#endif
uint32 g_recvd_block_count = 0;

#define WIFI_PUB_REG_BLOCKS   12
#define WIFI_PRIV_REG_BLOCKS  9
#define WIFI_MEM_BLOCKS       3

#define WIFI_PUB_GLB_CTL_LEN       (4 * 1024)
#define WIFI_PUB_PMU_CMU_CTL_LEN   (4 * 1024)
#define WIFI_PUB_RF_WB_CTL_LEN     (2 * 1024)
#define WIFI_PUB_RF_WB_TRX_REG_LEN (2 * 1024)
#define WIFI_PUB_RF_WB_PLL_REG_LEN (2 * 1024)
#define WIFI_PUB_RF_FG_CTL_LEN     (2 * 1024)
#define WIFI_PUB_RF_FG_TRX_REG_LEN (2 * 1024)
#define WIFI_PUB_RF_FG_PLL_REG_LEN (2 * 1024)
#define WIFI_PUB_COEX_CTL_LEN      (1 * 1024)
#define WIFI_PUB_DIAG_CTL_LEN      (1 * 1024)
#define WIFI_PUB_COM_CTL_LEN       (2 * 1024)
#define WIFI_PUB_AON_GPIO_RTC_LEN  (16 * 1024)

#define WIFI_PUB_REG_TOTAL_LEN (WIFI_PUB_GLB_CTL_LEN + WIFI_PUB_PMU_CMU_CTL_LEN + \
                                WIFI_PUB_RF_WB_CTL_LEN + WIFI_PUB_RF_WB_TRX_REG_LEN + \
                                WIFI_PUB_RF_WB_PLL_REG_LEN + WIFI_PUB_RF_FG_CTL_LEN + \
                                WIFI_PUB_RF_FG_TRX_REG_LEN + WIFI_PUB_RF_FG_PLL_REG_LEN + \
                                WIFI_PUB_COEX_CTL_LEN + WIFI_PUB_DIAG_CTL_LEN + \
                                WIFI_PUB_COM_CTL_LEN + WIFI_PUB_AON_GPIO_RTC_LEN)

#define WIFI_PRIV_W_CTL_LEN       (1 * 1024)
#define WIFI_PRIV_W_WDT_LEN       (1 * 1024)
#define WIFI_PRIV_W_TIMER_LEN     (1 * 1024)
#define WIFI_PRIV_W_CPU_CTL_LEN   (1 * 1024)
#define WIFI_PRIV_W_PHY_BANK1_LEN (1 * 1024)
#define WIFI_PRIV_W_PHY_BANK2_LEN (1 * 1024)
#define WIFI_PRIV_W_PHY_BANK3_LEN (1 * 1024)
#define WIFI_PRIV_W_PHY_BANK4_LEN (1 * 1024)
#define WIFI_PRIV_W_MAC_BANK_LEN  (2 * 1024 + 512)
#define WIFI_PRIV_W_DMA_LEN       (1 * 1024)

#define WIFI_PRIV_REG_TOTAL_LEN (WIFI_PRIV_W_CTL_LEN \
                                           + WIFI_PRIV_W_WDT_LEN \
                                           + WIFI_PRIV_W_TIMER_LEN \
                                           + WIFI_PRIV_W_CPU_CTL_LEN \
                                           + WIFI_PRIV_W_PHY_BANK1_LEN \
                                           + WIFI_PRIV_W_PHY_BANK2_LEN \
                                           + WIFI_PRIV_W_PHY_BANK3_LEN \
                                           + WIFI_PRIV_W_PHY_BANK4_LEN \
                                           + WIFI_PRIV_W_MAC_BANK_LEN \
                                           + WIFI_PRIV_W_DMA_LEN)

#define WIFI_MEM_W_TCM_WRAM_LEN      (512 * 1024)
#define WIFI_MEM_W_PKT_SHARE_RAM_LEN (256 * 1024)
#define WIFI_MEM_B_SHARE_RAM_LEN     (64 * 1024)

#define WIFI_MEM_TOTAL_LEN (WIFI_MEM_W_TCM_WRAM_LEN \
                                           + WIFI_MEM_W_PKT_SHARE_RAM_LEN \
                                           + WIFI_MEM_B_SHARE_RAM_LEN)

#define BFGX_PUB_REG_NUM    5
#define BFGX_PRIV_REG_NUM   2
#define BFGX_SHARE_RAM_NUM  2

#define BFGX_GLB_PMU_CMU_CTL_ADDR                 0x50000000
#define BFGX_RF_WB_CTL_ADDR                       0x5000C000
#define BFGX_RF_FG_CTL_ADDR                       0x5000E000
#define BFGX_COEX_DIAG_COM_CTL_ADDR               0x50010000
#define BFGX_AON_WB_GPIO_RTC_ADDR                 0x50006000
#define BFGX_PATCH_PLAT_NFC_BFGNI_SHARE_RAM_ADDR  0x300E4000
#define BFGX_W_SHARE_RAM_ADDR                     0x30178000
#define BFGX_GNSS_SUB_ADDR                        0x38000000
#define BFGX_B_CTL_WDT_TIMER_UART_ADDR            0x40000000
#define BFGX_IR_SUB_ADDR                          0x40007000
#define BFGX_B_DMA_CFG_ADDR                       0x40010000
#define BFGX_BT_FM_SUB_ADDR                       0x41040000
#define BFGX_NFC_APB_ADDR                         0x60000000

#define BFGX_GLB_PMU_CMU_CTL_LEN                (1024 * 12)
#define BFGX_RF_WB_CTL_LEN                      (1024 * 8)
#define BFGX_RF_FG_CTL_LEN                      (1024 * 8)
#define BFGX_COEX_DIAG_COM_CTL_LEN              (1024 * 20)
#define BFGX_AON_WB_GPIO_RTC_LEN                (1024 * 16)
#define BFGX_PATCH_PLAT_NFC_BFGNI_SHARE_RAM_LEN (1024 * 592)
#define BFGX_W_SHARE_RAM_LEN                    (1024 * 32)
#define BFGX_GNSS_SUB_LEN                       (1024 * 128)
#define BFGX_B_CTL_WDT_TIMER_UART_LEN           (1024 * 20)
#define BFGX_IR_SUB_LEN                         (1024 * 4)
#define BFGX_B_DMA_CFG_LEN                      (1024 * 4)
#define BFGX_BT_FM_SUB_LEN                      (1024 * 140)
#define BFGX_NFC_APB_LEN                        (1024 * 20)

#define GLB_PMU_CMU_CTL_FILE_NAME                "glb_pmu_cmu_ctl"
#define RF_WB_CTL_FILE_NAME                      "rf_wb_ctl"
#define RF_FG_CTL_FILE_NAME                      "rf_fg_ctl"
#define COEX_DIAG_COM_CTL_FILE_NAME              "coex_diag_com_ctl"
#define AON_WB_GPIO_RTC_FILE_NAME                "aon_wb_gpio_rtc"
#define PATCH_PLAT_NFC_BFGNI_SHARE_RAM_FILE_NAME "patch_plat_nfc_bfgni_share_ram"
#define W_SHARE_RAM_FILE_NAME                    "w_share_ram"
#define GNSS_SUB_FILE_NAME                       "gnss_sub"
#define B_CTL_WDT_TIMER_UART_FILE_NAME           "b_ctl_wdt_timer_uart"
#define IR_SUB_FILE_NAME                         "ir_sub"
#define B_DMA_CFG_FILE_NAME                      "b_dma_cfg"
#define BT_FM_SUB_FILE_NAME                      "bt_fm_sub"
#define NFC_APB_FILE_NAME                        "nfc_apb"

#define EXCEPTION_FILE_NAME_LEN                  100

OAL_STATIC struct st_uart_dump_wifi_mem_info g_wifi_pub_reg_info[WIFI_PUB_REG_BLOCKS] = {

    { "glb_ctl",       WIFI_PUB_GLB_CTL_LEN },
    { "pmu_cmu_ctl",   WIFI_PUB_PMU_CMU_CTL_LEN },
    { "rf_wb_ctl",     WIFI_PUB_RF_WB_CTL_LEN },
    { "rf_wb_trx_reg", WIFI_PUB_RF_WB_TRX_REG_LEN },
    { "rf_wb_pll_reg", WIFI_PUB_RF_WB_PLL_REG_LEN },
    { "rf_fg_ctl",     WIFI_PUB_RF_FG_CTL_LEN },
    { "rf_fg_trx_reg", WIFI_PUB_RF_FG_TRX_REG_LEN },
    { "rf_fg_pll_reg", WIFI_PUB_RF_FG_PLL_REG_LEN },
    { "coex_ctl",      WIFI_PUB_COEX_CTL_LEN },
    { "diag_ctl",      WIFI_PUB_DIAG_CTL_LEN },
    { "com_ctl",       WIFI_PUB_COM_CTL_LEN },
    { "aon_gpio_rtc",  WIFI_PUB_AON_GPIO_RTC_LEN },
};

OAL_STATIC struct st_uart_dump_wifi_mem_info g_wifi_priv_reg_info[WIFI_PRIV_REG_BLOCKS] = {
    { "w_ctl",       WIFI_PRIV_W_CTL_LEN },
    { "w_wdt",       WIFI_PRIV_W_WDT_LEN },
    { "w_timer",     WIFI_PRIV_W_TIMER_LEN },
    { "w_cpu_ctl",   WIFI_PRIV_W_CPU_CTL_LEN },
    { "w_phy_bank1", WIFI_PRIV_W_PHY_BANK1_LEN },
    { "w_phy_bank2", WIFI_PRIV_W_PHY_BANK2_LEN },
    { "w_phy_bank3", WIFI_PRIV_W_PHY_BANK3_LEN },
    { "w_phy_bank4", WIFI_PRIV_W_PHY_BANK4_LEN },
    { "w_mac_bank",  WIFI_PRIV_W_MAC_BANK_LEN },
};

OAL_STATIC struct st_uart_dump_wifi_mem_info g_wifi_mem_info[WIFI_MEM_BLOCKS] = {
    { "w_tcm_wram",      WIFI_MEM_W_TCM_WRAM_LEN },
    { "w_pkt_share_ram", WIFI_MEM_W_PKT_SHARE_RAM_LEN },
    { "b_share_ram",     WIFI_MEM_B_SHARE_RAM_LEN },
};

OAL_STATIC struct st_uart_dump_wifi_info g_uart_read_wifi_mem_info[UART_WIFI_MEM_DUMP_BOTTOM] = {
    { SYS_CFG_READ_WLAN_PUB_REG,  WIFI_PUB_REG_TOTAL_LEN,  WIFI_PUB_REG_BLOCKS,  g_wifi_pub_reg_info },
    { SYS_CFG_READ_WLAN_PRIV_REG, WIFI_PRIV_REG_TOTAL_LEN, WIFI_PRIV_REG_BLOCKS, g_wifi_priv_reg_info },
    { SYS_CFG_READ_WLAN_MEM,      WIFI_MEM_TOTAL_LEN,      WIFI_MEM_BLOCKS,      g_wifi_mem_info },
};

OAL_STATIC struct st_exception_mem_info g_uart_wifi_mem_dump[UART_WIFI_MEM_DUMP_BOTTOM] = {{0}, {0}, {0}};
OAL_STATIC uint32 g_recvd_wifi_block_index = UART_WIFI_MEM_DUMP_BOTTOM;

OAL_STATIC struct st_bfgx_reset_cmd g_bfgx_reset_msg[BFGX_BUTT] = {
    { BT_RESET_CMD_LEN,   { 0x04, 0xff, 0x01, 0xc7 }},
    { FM_RESET_CMD_LEN,   { 0xfb }},
    /* seperate_tag equal GNSS_SEPER_TAG_LAST */
    { GNSS_RESET_CMD_LEN, { 0x8, 0x0, 0x0, 0x0, 0xa1, 0xb4, 0xc7, 0x51, GNSS_SEPER_TAG_LAST }},
    { IR_RESET_CMD_LEN,   {0}},
    { NFC_RESET_CMD_LEN,  {0}},
};

OAL_STATIC exception_bcpu_dump_msg g_sdio_read_bcpu_pub_reg_info[BFGX_PUB_REG_NUM] = {
    { GLB_PMU_CMU_CTL_FILE_NAME,   BFGX_GLB_PMU_CMU_CTL_ADDR,   ALIGN_2_BYTE, BFGX_GLB_PMU_CMU_CTL_LEN },
    { RF_WB_CTL_FILE_NAME,         BFGX_RF_WB_CTL_ADDR,         ALIGN_2_BYTE, BFGX_RF_WB_CTL_LEN },
    { RF_FG_CTL_FILE_NAME,         BFGX_RF_FG_CTL_ADDR,         ALIGN_2_BYTE, BFGX_RF_FG_CTL_LEN },
    { COEX_DIAG_COM_CTL_FILE_NAME, BFGX_COEX_DIAG_COM_CTL_ADDR, ALIGN_2_BYTE, BFGX_COEX_DIAG_COM_CTL_LEN },
    { AON_WB_GPIO_RTC_FILE_NAME,   BFGX_AON_WB_GPIO_RTC_ADDR,   ALIGN_2_BYTE, BFGX_AON_WB_GPIO_RTC_LEN },
};

OAL_STATIC exception_bcpu_dump_msg g_sdio_read_bcpu_mem_info[BFGX_SHARE_RAM_NUM] = {
    { PATCH_PLAT_NFC_BFGNI_SHARE_RAM_FILE_NAME, BFGX_PATCH_PLAT_NFC_BFGNI_SHARE_RAM_ADDR,
      ALIGN_1_BYTE, BFGX_PATCH_PLAT_NFC_BFGNI_SHARE_RAM_LEN },
    { W_SHARE_RAM_FILE_NAME, BFGX_W_SHARE_RAM_ADDR, ALIGN_1_BYTE, BFGX_W_SHARE_RAM_LEN },
};

OAL_STATIC exception_bcpu_dump_msg g_sdio_read_bcpu_priv_reg_info[BFGX_PRIV_REG_NUM] = {
    /* 私有寄存器只能拷贝2个 */
    { B_CTL_WDT_TIMER_UART_FILE_NAME, BFGX_B_CTL_WDT_TIMER_UART_ADDR, ALIGN_2_BYTE, BFGX_B_CTL_WDT_TIMER_UART_LEN },
    { IR_SUB_FILE_NAME,               BFGX_IR_SUB_ADDR,               ALIGN_2_BYTE, BFGX_IR_SUB_LEN },
};

OAL_STATIC exception_bcpu_dump_msg g_sdio_read_all[BFGX_PUB_REG_NUM + BFGX_SHARE_RAM_NUM + BFGX_PRIV_REG_NUM] = {
    { GLB_PMU_CMU_CTL_FILE_NAME,   BFGX_GLB_PMU_CMU_CTL_ADDR,   ALIGN_2_BYTE, BFGX_GLB_PMU_CMU_CTL_LEN },
    { RF_WB_CTL_FILE_NAME,         BFGX_RF_WB_CTL_ADDR,         ALIGN_2_BYTE, BFGX_RF_WB_CTL_LEN },
    { RF_FG_CTL_FILE_NAME,         BFGX_RF_FG_CTL_ADDR,         ALIGN_2_BYTE, BFGX_RF_FG_CTL_LEN },
    { COEX_DIAG_COM_CTL_FILE_NAME, BFGX_COEX_DIAG_COM_CTL_ADDR, ALIGN_2_BYTE, BFGX_COEX_DIAG_COM_CTL_LEN },
    { AON_WB_GPIO_RTC_FILE_NAME,   BFGX_AON_WB_GPIO_RTC_ADDR,   ALIGN_2_BYTE, BFGX_AON_WB_GPIO_RTC_LEN },
    { PATCH_PLAT_NFC_BFGNI_SHARE_RAM_FILE_NAME, BFGX_PATCH_PLAT_NFC_BFGNI_SHARE_RAM_ADDR,
      ALIGN_1_BYTE, BFGX_PATCH_PLAT_NFC_BFGNI_SHARE_RAM_LEN },
    { W_SHARE_RAM_FILE_NAME,          BFGX_W_SHARE_RAM_ADDR,          ALIGN_1_BYTE, BFGX_W_SHARE_RAM_LEN },
    { B_CTL_WDT_TIMER_UART_FILE_NAME, BFGX_B_CTL_WDT_TIMER_UART_ADDR, ALIGN_2_BYTE, BFGX_B_CTL_WDT_TIMER_UART_LEN },
    { IR_SUB_FILE_NAME,               BFGX_IR_SUB_ADDR,               ALIGN_2_BYTE, BFGX_IR_SUB_LEN },
};

OAL_STATIC uint8 g_plat_beattimer_timeout_reset_cfg = 0;
#ifdef HI110X_HAL_MEMDUMP_ENABLE
memdump_info_t g_bcpu_memdump_cfg;
memdump_info_t g_wcpu_memdump_cfg;
#endif

/* 函数实现 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 18, 0))
void bfgx_beat_timer_expire(uint64 data);
#else
void bfgx_beat_timer_expire(struct timer_list *t);
#endif
int32 get_exception_info_reference(struct st_exception_info **exception_data);
int32 plat_exception_handler(uint32 subsys_type, uint32 thread_type, uint32 exception_type);
void plat_exception_reset_work(struct work_struct *work);
int32 wifi_exception_handler(void);
int32 wifi_subsystem_reset(void);
int32 wifi_system_reset(void);
int32 wifi_status_recovery(void);
int32 wifi_exception_mem_dump(struct st_wifi_dump_mem_info *pst_mem_dump_info, uint32 count, oal_int32 excep_type);
int32 bfgx_exception_handler(void);
int32 bfgx_subthread_reset(void);
int32 bfgx_subsystem_reset(void);
int32 bfgx_system_reset(void);
int32 bfgx_recv_dev_mem(uint8 *buf_ptr, uint16 count);
void bfgx_store_stack_mem_to_file(void);
void bfgx_dump_stack(void);
int32 bfgx_status_recovery(void);
int32 plat_bfgx_exception_rst_register(struct ps_plat_s *data);
int32 plat_exception_reset_init(void);
int32 plat_exception_reset_exit(void);

/*
 * 函 数 名  : plat_dfr_cfg_set
 * 功能描述  : dfr全局配置
 */
void plat_dfr_cfg_set(uint64 arg)
{
    struct st_exception_info *pst_exception_data = NULL;

    get_exception_info_reference(&pst_exception_data);
    if (pst_exception_data == NULL) {
        ps_print_err("get exception info reference is error\n");
        return;
    }

    pst_exception_data->exception_reset_enable = arg ? (PLAT_EXCEPTION_ENABLE) : (PLAT_EXCEPTION_DISABLE);

    ps_print_info("plat dfr cfg set value = %ld\n", arg);
}

/*
 * 函 数 名  : plat_beattimer_timeout_reset_cfg_set
 * 功能描述  : beat_timer全局配置
 * 输入参数  : uint64 arg，没有使用。
 */
void plat_beattimer_timeout_reset_cfg_set(uint64 arg)
{
    g_plat_beattimer_timeout_reset_cfg = arg ? (PLAT_EXCEPTION_ENABLE) : (PLAT_EXCEPTION_DISABLE);
    ps_print_info("plat beat timer timeOut reset cfg set value = %ld\n", arg);
}

int32 mod_beat_timer(uint8 on)
{
    struct st_exception_info *pst_exception_data = NULL;

    get_exception_info_reference(&pst_exception_data);
    if (pst_exception_data == NULL) {
        ps_print_err("get exception info reference is error\n");
        return -EXCEPTION_FAIL;
    }

    if (on) {
        ps_print_info("reset beat timer\n");
        mod_timer(&pst_exception_data->bfgx_beat_timer, jiffies + BFGX_BEAT_TIME * HZ);
        atomic_set(&pst_exception_data->bfgx_beat_flag, BFGX_NOT_RECV_BEAT_INFO);
    } else {
        ps_print_info("delete beat timer\n");
        del_timer_sync(&pst_exception_data->bfgx_beat_timer);
    }

    return 0;
}

/*
 * 函 数 名  : bfgx_beat_timer_expire
 * 功能描述  : bfgx心跳超时处理函数，该函数运行在软中断上下文中，不能有引起睡眠的操作
 * 输入参数  : uint64 data，不需要。加上是为了遵守内核的函数声明
 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 18, 0))
void bfgx_beat_timer_expire(uint64 data)
#else
void bfgx_beat_timer_expire(struct timer_list *t)
#endif
{
    struct ps_core_s *ps_core_d = NULL;
    struct st_exception_info *pst_exception_data = NULL;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35))
    struct tty_buffer *thead;
    struct tty_bufhead *buf = NULL;
#endif

    if (bfgx_is_shutdown()) {
        ps_print_warning("bfgx is closed\n");
        return;
    }
    get_exception_info_reference(&pst_exception_data);
    if (pst_exception_data == NULL) {
        ps_print_err("get exception info reference is error\n");
        return;
    }

    if (unlikely(pst_exception_data->ps_plat_d == NULL)) {
        ps_print_err("pst_exception_data->ps_plat_d is NULL\n");
        return;
    }

    ps_core_d = pst_exception_data->ps_plat_d->core_data;

    /* bfgx睡眠时，没有心跳消息上报 */
    if (ps_core_d->ps_pm->bfgx_dev_state_get() == BFGX_SLEEP) {
        ps_print_info("bfgx has sleep!\n");
        return;
    }

    if (atomic_read(&pst_exception_data->bfgx_beat_flag) == BFGX_NOT_RECV_BEAT_INFO) {
        if (OAL_IS_ERR_OR_NULL(ps_core_d->tty)) {
            ps_print_err("tty is NULL\n");
            return;
        }

        ps_print_err("###########host can not recvive bfgx beat info@@@@@@@@@@@@@@!\n");
        ps_uart_state_pre(ps_core_d->tty);
        ps_uart_state_dump(ps_core_d->tty);

        if (ps_core_d->tty->port == NULL) {
            ps_print_err("tty->port is null, bfgx download patch maybe failed!\n");
            return;
        }

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35))
        buf = &(ps_core_d->tty->port->buf);
        thead = buf->head;
        while (thead != NULL) {
            ps_print_info("tty rx buf:used=0x%x,size=0x%x,commit=0x%x,read=0x%x\n",
                          thead->used, thead->size, thead->commit, thead->read);
            thead = thead->next;
        }
#endif

        declare_dft_trace_key_info("bfgx beat timeout", OAL_DFT_TRACE_EXCEP);

        if (g_ssi_dump_en) {
            ssi_dump_device_regs(SSI_MODULE_MASK_COMM | SSI_MODULE_MASK_BCTRL | SSI_MODULE_MASK_WCTRL);
        }

        chr_exception_report(CHR_PLATFORM_EXCEPTION_EVENTID, CHR_SYSTEM_PLAT, CHR_LAYER_DRV,
                             CHR_PLT_DRV_EVENT_DEV, CHR_PLAT_DRV_ERROR_BEAT_TIMEOUT);

        if (g_plat_beattimer_timeout_reset_cfg == PLAT_EXCEPTION_ENABLE) {
            ps_print_err("bfgx beat timer bring to reset work!\n");
            plat_exception_handler(SUBSYS_BFGX, THREAD_IDLE, BFGX_BEATHEART_TIMEOUT);
            return;
        }
    }

    atomic_set(&pst_exception_data->bfgx_beat_flag, BFGX_NOT_RECV_BEAT_INFO);

    ps_print_info("reset beat timer\n");
    mod_timer(&pst_exception_data->bfgx_beat_timer, jiffies + BFGX_BEAT_TIME * HZ);

    bfg_check_timer_work();

    return;
}

/*
 * 函 数 名  : get_exception_info_reference
 * 功能描述  : 获得保存异常信息的全局变量
 * 输入参数  : st_exception_info **exception结构体指针，保存全局变量地址的指针
 * 返 回 值  : 成功返回0，否则返回-1
 */
int32 get_exception_info_reference(struct st_exception_info **exception_data)
{
    if (exception_data == NULL) {
        ps_print_err("%s parm exception_data is NULL\n", __func__);
        return -EXCEPTION_FAIL;
    }

    if (g_exception_info == NULL) {
        *exception_data = NULL;
        ps_print_err("%s g_exception_info is NULL\n", __func__);
        return -EXCEPTION_FAIL;
    }

    *exception_data = g_exception_info;

    return EXCEPTION_SUCCESS;
}

int32 uart_reset_wcpu(void)
{
    uint64 timeleft;
    struct ps_core_s *ps_core_d = NULL;
    struct st_exception_info *pst_exception_data = NULL;

    get_exception_info_reference(&pst_exception_data);
    if (pst_exception_data == NULL) {
        ps_print_err("get exception info reference is error\n");
        return -EXCEPTION_FAIL;
    }

    ps_get_core_reference(&ps_core_d);
    if (unlikely(ps_core_d == NULL)) {
        ps_print_err("ps_core_d is err\n");
        return -EXCEPTION_FAIL;
    }

    if (pst_exception_data->subsys_type >= SUBSYS_BOTTOM) {
        ps_print_err("subsys [%d] is error!\n", pst_exception_data->subsys_type);
        return -EXCEPTION_FAIL;
    }

    if (prepare_to_visit_node(ps_core_d) != EXCEPTION_SUCCESS) {
        ps_print_err("prepare work FAIL\n");
        return -EXCEPTION_FAIL;
    }

    oal_reinit_completion(ps_core_d->wait_wifi_opened);
    ps_uart_state_pre(ps_core_d->tty);

    if (pst_exception_data->subsys_type == SUBSYS_WIFI) {
        ps_print_info("uart reset WCPU\n");
        ps_tx_sys_cmd(ps_core_d, SYS_MSG, SYS_CFG_EXCEP_RESET_WCPU);
    } else {
        ps_print_info("uart open WCPU\n");
        ps_tx_sys_cmd(ps_core_d, SYS_MSG, SYS_CFG_OPEN_WIFI);
    }

    timeleft = wait_for_completion_timeout(&ps_core_d->wait_wifi_opened, msecs_to_jiffies(WAIT_WIFI_OPEN_TIME));
    if (!timeleft) {
        ps_uart_state_dump(ps_core_d->tty);
        ps_print_err("wait wifi open ack timeout\n");
        post_to_visit_node(ps_core_d);
        chr_exception_report(CHR_PLATFORM_EXCEPTION_EVENTID, CHR_SYSTEM_PLAT, CHR_LAYER_DRV,
                             CHR_PLT_DRV_EVENT_RST, CHR_PLAT_DRV_ERROR_UART_RST_WCPU);
        return -EXCEPTION_FAIL;
    }

    post_to_visit_node(ps_core_d);

    return EXCEPTION_SUCCESS;
}

void plat_dfr_sysrst_type_cnt_inc(enum DFR_RST_SYSTEM_TYPE_E rst_type, enum SUBSYSTEM_ENUM subs)
{
    struct st_exception_info *pst_exception_data = NULL;
    if ((rst_type >= DFR_SYSTEM_RST_TYPE_BOTT) || (subs >= SUBSYS_BOTTOM)) {
        goto exit;
    }
    get_exception_info_reference(&pst_exception_data);
    if (pst_exception_data == NULL) {
        ps_print_err("get exception info reference is error\n");
        goto exit;
    }
    if (pst_exception_data->subsys_type == subs) {
        pst_exception_data->etype_info[pst_exception_data->excetion_type].rst_type_cnt[rst_type]++;
    }
    return;
exit:
    ps_print_err("dfr rst type cnt inc fail\n");
}

OAL_STATIC excp_info_str_t g_excp_info_str_tab[] = {
    { .id = BFGX_BEATHEART_TIMEOUT, .name = "bfgx_beatheart_timeout" },
    { .id = BFGX_LASTWORD_PANIC,    .name = "bfgx_lastword_panic" },
    { .id = BFGX_TIMER_TIMEOUT,     .name = "bfgx_timer_timeout" },
    { .id = BFGX_ARP_TIMEOUT,       .name = "bfgx_arp_timeout" },
    { .id = BFGX_POWERON_FAIL,      .name = "bfgx_poweron_fail" },
    { .id = BFGX_WAKEUP_FAIL,       .name = "bfgx_wakeup_fail" },
    { .id = WIFI_WATCHDOG_TIMEOUT,  .name = "wifi_watchdog_timeout" },
    { .id = WIFI_POWERON_FAIL,      .name = "wifi_poweron_fail" },
    { .id = WIFI_WAKEUP_FAIL,       .name = "wifi_wakeup_fail" },
    { .id = WIFI_DEVICE_PANIC,      .name = "wifi_device_panic" },
    { .id = WIFI_TRANS_FAIL,        .name = "wifi_trans_fail" },
    { .id = SDIO_DUMPBCPU_FAIL,     .name = "sdio_dumpbcpu_fail" },
};

OAL_STATIC char *excp_info_str_get(uint32 id)
{
    int32 i = 0;
    for (i = 0; i < sizeof(g_excp_info_str_tab) / sizeof(excp_info_str_t); i++) {
        if (id == g_excp_info_str_tab[i].id) {
            return g_excp_info_str_tab[i].name;
        }
    }
    return NULL;
}

int32 plat_get_dfr_sinfo(char *buf, int32 index)
{
    struct st_exception_info *pst_exception_data = NULL;
    int i = index;
    uint etype;
    int ret;
    get_exception_info_reference(&pst_exception_data);
    if (pst_exception_data == NULL) {
        ps_print_err("get exception info reference is error\n");
        return -EXCEPTION_FAIL;
    }
    ret = snprintf_s(buf + i, PAGE_SIZE - i, PAGE_SIZE - i - 1, "==========dfr info:=========\n");
    if (ret < 0) {
        return i;
    }
    i += ret;

    ret = snprintf_s(buf + i, PAGE_SIZE - i, PAGE_SIZE - i - 1, "total cnt:%-10d\n", plat_get_excp_total_cnt());
    if (ret < 0) {
        return i;
    }
    i += ret;

    for (etype = 0; etype < EXCEPTION_TYPE_BOTTOM; etype++) {
        ret = snprintf_s(buf + i, PAGE_SIZE - i, PAGE_SIZE - i - 1,
                         "id:%-30scnt:%-10dfail:%-10dsingle_sysrst:%-10dall_sysrst:%-10dmaxtime:%lldms\n",
                         excp_info_str_get(etype),
                         pst_exception_data->etype_info[etype].excp_cnt,
                         pst_exception_data->etype_info[etype].fail_cnt,
                         pst_exception_data->etype_info[etype].rst_type_cnt[DFR_SINGLE_SYS_RST],
                         pst_exception_data->etype_info[etype].rst_type_cnt[DFR_ALL_SYS_RST],
                         pst_exception_data->etype_info[etype].maxtime);
        if (ret < 0) {
            return i;
        }
        i += ret;
    }
    return i;
}

void plat_print_dfr_info(void)
{
    struct st_exception_info *pst_exception_data = NULL;
    uint etype;
    get_exception_info_reference(&pst_exception_data);
    if (pst_exception_data == NULL) {
        ps_print_err("get exception info reference is error\n");
        return;
    }
    ps_print_info("========== dfr info:+++++++++++++++++++++++++++++++\n");
    ps_print_info("total cnt:%-10d\n", plat_get_excp_total_cnt());
    for (etype = 0; etype < EXCEPTION_TYPE_BOTTOM; etype++) {
        ps_print_info("type:%-30scnt:%-10dfail:%-10dsingle_sysrst:%-10dall_sysrst:%-10dmaxtime:%lldms\n",
                      excp_info_str_get(etype),
                      pst_exception_data->etype_info[etype].excp_cnt,
                      pst_exception_data->etype_info[etype].fail_cnt,
                      pst_exception_data->etype_info[etype].rst_type_cnt[DFR_SINGLE_SYS_RST],
                      pst_exception_data->etype_info[etype].rst_type_cnt[DFR_ALL_SYS_RST],
                      pst_exception_data->etype_info[etype].maxtime);
    }
    ps_print_info("========== dfr info:-----------------------------------\n");
}

int32 plat_get_excp_total_cnt()
{
    struct st_exception_info *pst_exception_data = NULL;
    int i;
    int total_cnt = 0;
    get_exception_info_reference(&pst_exception_data);
    if (pst_exception_data == NULL) {
        ps_print_err("get exception info reference is error\n");
        return -EXCEPTION_FAIL;
    }
    for (i = 0; i < EXCEPTION_TYPE_BOTTOM; i++) {
        total_cnt += (int)pst_exception_data->etype_info[i].excp_cnt;
    }
    return total_cnt;
}

/*
 * 函 数 名  : plat_power_fail_exception_info_set
 * 功能描述  : bfgx open失败时，调用该函数设置exception info
 * 输入参数  : subsys_type:调用异常处理的子系统，WIFI或者BFGX
 *             thread_type:子系统中的子线程
 *             exception_type:异常的类型
 * 返 回 值  : 异常处理成功返回0，否则返回1
 */
int32 plat_power_fail_exception_info_set(uint32 subsys_type, uint32 thread_type, uint32 exception_type)
{
    struct st_exception_info *pst_exception_data = NULL;
    uint64 flag;

    if (subsys_type >= SUBSYS_BOTTOM) {
        ps_print_err("para subsys_type %u is error!\n", subsys_type);
        return -EXCEPTION_FAIL;
    }

    if (((subsys_type == SUBSYS_WIFI) && (thread_type >= WIFI_THREAD_BOTTOM)) ||
        ((subsys_type == SUBSYS_BFGX) && (thread_type >= BFGX_THREAD_BOTTOM))) {
        ps_print_err("para thread_type %u is error! subsys_type is %u\n", thread_type, subsys_type);
        return -EXCEPTION_FAIL;
    }

    if (exception_type >= EXCEPTION_TYPE_BOTTOM) {
        ps_print_err("para exception_type %u is error!\n", exception_type);
        return -EXCEPTION_FAIL;
    }

    get_exception_info_reference(&pst_exception_data);
    if (pst_exception_data == NULL) {
        ps_print_err("get exception info reference is error\n");
        return -EXCEPTION_FAIL;
    }

    if (pst_exception_data->exception_reset_enable != PLAT_EXCEPTION_ENABLE) {
        ps_print_warning("palt exception reset not enable!");
        return -EXCEPTION_FAIL;
    }

    spin_lock_irqsave(&pst_exception_data->exception_spin_lock, flag);

    if (atomic_read(&pst_exception_data->is_reseting_device) == PLAT_EXCEPTION_RESET_IDLE) {
        pst_exception_data->subsys_type = subsys_type;
        pst_exception_data->thread_type = thread_type;
        pst_exception_data->excetion_type = exception_type;

        /* 当前异常没有处理完成之前，不允许处理新的异常 */
        atomic_set(&pst_exception_data->is_reseting_device, PLAT_EXCEPTION_RESET_BUSY);
    } else {
        ps_print_info("plat is processing exception! subsys=%d, exception type=%d\n",
                      pst_exception_data->subsys_type, pst_exception_data->excetion_type);
        spin_unlock_irqrestore(&pst_exception_data->exception_spin_lock, flag);
        return -EXCEPTION_FAIL;
    }

    /* 增加统计信息 */
    pst_exception_data->etype_info[exception_type].excp_cnt += 1;

    spin_unlock_irqrestore(&pst_exception_data->exception_spin_lock, flag);

    return EXCEPTION_SUCCESS;
}

/*
 * 函 数 名  : plat_power_fail_process_done
 * 功能描述  : bfgx open失败时，异常处理完成
 */
void plat_power_fail_process_done(void)
{
    struct st_exception_info *pst_exception_data = NULL;

    get_exception_info_reference(&pst_exception_data);
    if (pst_exception_data == NULL) {
        ps_print_err("get exception info reference is error\n");
        return;
    }

    atomic_set(&pst_exception_data->is_reseting_device, PLAT_EXCEPTION_RESET_IDLE);

    ps_print_suc("bfgx open fail process done\n");

    return;
}

void plat_bfgx_subtype_process(uint32 subsys_type, uint32 exception_type, struct pm_drv_data *pm_data)
{
    if (subsys_type == SUBSYS_BFGX) {
        /* timer time out 中断调用时，不能再这里删除，死锁 */
        if (exception_type != BFGX_BEATHEART_TIMEOUT) {
            pm_data->ps_pm_interface->operate_beat_timer(BEAT_TIMER_DELETE);
        }
    }
}
/*
 * 函 数 名  : plat_exception_handler
 * 功能描述  : 平台host异常处理总入口函数，填充异常信息，并触发异常处理work
 * 输入参数  : subsys_type:调用异常处理的子系统，WIFI或者BFGX
 *             thread_type:子系统中的子线程
 *             exception_type:异常的类型
 * 返 回 值  : 异常处理成功返回0，否则返回1
 */
int32 plat_exception_handler(uint32 subsys_type, uint32 thread_type, uint32 exception_type)
{
    struct st_exception_info *pst_exception_data = NULL;
    uint64 flag;
    int32 timeout;
    struct pm_drv_data *pm_data = pm_get_drvdata();

    if (pm_data == NULL) {
        ps_print_err("pm_data is NULL!\n");
        return -EXCEPTION_FAIL;
    }
    if (exception_type != RF_CALI_FAIL) {
        chr_exception_report(CHR_PLATFORM_EXCEPTION_EVENTID, CHR_SYSTEM_PLAT, CHR_LAYER_DRV,
                             CHR_PLT_DRV_EVENT_DFR, CHR_PLAT_DRV_ERROR_EXCEPTION_HANDLE);
    }
    if (subsys_type >= SUBSYS_BOTTOM) {
        ps_print_err("para subsys_type %u is error!\n", subsys_type);
        return -EXCEPTION_FAIL;
    }

    if (((subsys_type == SUBSYS_WIFI) && (thread_type >= WIFI_THREAD_BOTTOM)) ||
        ((subsys_type == SUBSYS_BFGX) && (thread_type >= BFGX_THREAD_BOTTOM))) {
        ps_print_err("para thread_type %u is error! subsys_type is %u\n", thread_type, subsys_type);
        return -EXCEPTION_FAIL;
    }

    if (exception_type >= EXCEPTION_TYPE_BOTTOM) {
        ps_print_err("para exception_type %u is error!\n", exception_type);
        return -EXCEPTION_FAIL;
    }

    get_exception_info_reference(&pst_exception_data);
    if (pst_exception_data == NULL) {
        ps_print_err("get exception info reference is error\n");
        return -EXCEPTION_FAIL;
    }

    if (pst_exception_data->exception_reset_enable != PLAT_EXCEPTION_ENABLE) {
        ps_print_info("plat exception reset not enable!");
        return EXCEPTION_SUCCESS;
    }

    /* 这里只能用spin lock，因为该函数会被心跳超时函数调用，心跳超时函数属于软中断，不允许睡眠 */
    spin_lock_irqsave(&pst_exception_data->exception_spin_lock, flag);
    if (atomic_read(&pst_exception_data->is_reseting_device) == PLAT_EXCEPTION_RESET_IDLE) {
        pst_exception_data->subsys_type = subsys_type;
        pst_exception_data->thread_type = thread_type;
        pst_exception_data->excetion_type = exception_type;

        /* 当前异常没有处理完成之前，不允许处理新的异常 */
        atomic_set(&pst_exception_data->is_reseting_device, PLAT_EXCEPTION_RESET_BUSY);
    } else {
        ps_print_info("plat is processing exception! subsys=%d, exception type=%d\n",
                      pst_exception_data->subsys_type, pst_exception_data->excetion_type);
        spin_unlock_irqrestore(&pst_exception_data->exception_spin_lock, flag);
        return EXCEPTION_SUCCESS;
    }
    spin_unlock_irqrestore(&pst_exception_data->exception_spin_lock, flag);

#ifdef _PRE_CONFIG_GPIO_TO_SSI_DEBUG
    /* 等待SSI操作完成，防止NOC */
    timeout = wait_for_ssi_idle_timeout(HI110X_DFR_WAIT_SSI_IDLE_MS);
    if (timeout <= 0) {
        ps_print_err("[HI110X_DFR]wait for ssi idle failed\n");
        atomic_set(&pst_exception_data->is_reseting_device, PLAT_EXCEPTION_RESET_IDLE);
        return EXCEPTION_FAIL;
    }
    ps_print_info("[HI110X_DFR]wait for ssi idle cost time:%dms\n", HI110X_DFR_WAIT_SSI_IDLE_MS - timeout);
#endif

    plat_bfgx_subtype_process(subsys_type, exception_type, pm_data);
    pst_exception_data->etype_info[pst_exception_data->excetion_type].stime = ktime_get();
    ps_print_warning("[HI110X_DFR]plat start doing exception! subsys=%d, exception type=%d\n",
                     pst_exception_data->subsys_type, pst_exception_data->excetion_type);

    oal_wake_lock_timeout(&pst_exception_data->plat_exception_rst_wlock, PLAT_EXCEP_TIMEOUT); /* 处理异常，持锁10秒 */
    /* 触发异常处理worker */
    queue_work(pst_exception_data->plat_exception_rst_workqueue, &pst_exception_data->plat_exception_rst_work);

    return EXCEPTION_SUCCESS;
}

EXPORT_SYMBOL(plat_exception_handler);

/*
 * 函 数 名  : plat_exception_reset_work
 * 功能描述  : 平台host异常处理work，判断产生异常的系统，调用相应的处理函数。
 *             这个函数会获取mutex，以避免并发处理。
 * 返 回 值  : 异常处理成功返回0，否则返回1
 */
void plat_exception_reset_work(struct work_struct *work)
{
    int32 ret = -EXCEPTION_FAIL;
    struct st_exception_info *pst_exception_data = NULL;
    struct pm_drv_data *pm_data = pm_get_drvdata();
    ktime_t trans_time;
    unsigned long long total_time;

    if (pm_data == NULL) {
        ps_print_err("[HI110X_DFR]pm_data is NULL!\n");
        return;
    }

    get_exception_info_reference(&pst_exception_data);
    if (pst_exception_data == NULL) {
        ps_print_err("[HI110X_DFR]get exception info reference is error\n");
        return;
    }

    ps_print_warning("[HI110X_DFR] enter plat_exception_reset_work\n");
    mutex_lock(&pm_data->host_mutex);

    if (pst_exception_data->subsys_type == SUBSYS_WIFI) {
        if (wlan_is_shutdown()) {
            ps_print_warning("[HI110X_DFR]wifi is closed,stop wifi dfr\n");
            goto exit_exception;
        }
        ret = wifi_exception_handler();
    } else {
        if (bfgx_is_shutdown()) {
            ps_print_warning("[HI110X_DFR]bfgx is closed,stop bfgx dfr\n");
            goto exit_exception;
        }
        ret = bfgx_exception_handler();
    }

    pst_exception_data->etype_info[pst_exception_data->excetion_type].excp_cnt += 1;

    if (ret != EXCEPTION_SUCCESS) {
        ps_print_err("[HI110X_DFR]plat execption recovery fail! subsys_type = [%u], exception_type = [%u]\n",
                     pst_exception_data->subsys_type, pst_exception_data->excetion_type);
        pst_exception_data->etype_info[pst_exception_data->excetion_type].fail_cnt++;
    } else {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35))
        trans_time = ktime_sub(ktime_get(), pst_exception_data->etype_info[pst_exception_data->excetion_type].stime);
        total_time = (unsigned long long)ktime_to_ms(trans_time);
        if (pst_exception_data->etype_info[pst_exception_data->excetion_type].maxtime < total_time) {
            pst_exception_data->etype_info[pst_exception_data->excetion_type].maxtime = total_time;
            ps_print_warning("[HI110X_DFR]time update:%llu,exception_type:%d \n",
                             total_time, pst_exception_data->excetion_type);
        }
#endif
        ps_print_warning("[HI110X_DFR]plat execption recovery success, current time [%llu]ms, max time [%llu]ms\n",
                         total_time, pst_exception_data->etype_info[pst_exception_data->excetion_type].maxtime);
    }
    /* 每次dfr完成，显示历史dfr处理结果 */
    plat_print_dfr_info();

exit_exception:
    plat_bbox_msg_hander(pst_exception_data->subsys_type, pst_exception_data->excetion_type);
    atomic_set(&pst_exception_data->is_reseting_device, PLAT_EXCEPTION_RESET_IDLE);
    mutex_unlock(&pm_data->host_mutex);
    return;
}

int32 is_subsystem_rst_enable(void)
{
    struct st_exception_info *pst_exception_data = NULL;
    get_exception_info_reference(&pst_exception_data);
    if (pst_exception_data == NULL) {
        ps_print_err("get exception info reference is error\n");
        return DFR_TEST_DISABLE;
    }
    ps_print_info("#########is_subsystem_rst_enable:%d\n", pst_exception_data->subsystem_rst_en);
    return pst_exception_data->subsystem_rst_en;
}

/*
 * 函 数 名  : wifi_exception_handler
 * 功能描述  : wifi异常处理入口函数，判断BFGN是否打开，如果打开则调用单系统复位流程，
 *             BFGN没开则调用全系统复位流程(单系统复位需要用到uart)。如果单系统复位
 *             失败，则会进行全系统复位。
 * 返 回 值  : 异常处理成功返回0，否则返回1
 */
int32 wifi_exception_handler(void)
{
    int32 ret = -EXCEPTION_FAIL;
    uint32 exception_type;
    struct st_exception_info *pst_exception_data = NULL;

    chr_exception_report(CHR_PLATFORM_EXCEPTION_EVENTID, CHR_SYSTEM_PLAT, CHR_LAYER_DRV,
                         CHR_PLT_DRV_EVENT_DFR, CHR_PLAT_DRV_ERROR_EXCP_WIFI_HANDLE);

    get_exception_info_reference(&pst_exception_data);
    if (pst_exception_data == NULL) {
        ps_print_err("get exception info reference is error\n");
        return -EXCEPTION_FAIL;
    }

    exception_type = pst_exception_data->excetion_type;

    /* 如果bfgx已经打开，执行wifi子系统复位流程，否则执行wifi全系统复位流程 */
    if (!bfgx_is_shutdown()) {
        ps_print_info("bfgx is opened, start wifi subsystem reset!\n");
        bfgx_print_subsys_state();
        if (!is_subsystem_rst_enable()) {
            ret = wifi_subsystem_reset();
            if (ret != EXCEPTION_SUCCESS) {
                ps_print_err("wifi subsystem reset failed, start wifi system reset!\n");
                ret = wifi_system_reset();
            }
        } else {
            ps_print_err("in debug mode,skip subsystem rst,do wifi system reset!\n");
            ret = wifi_system_reset();
        }
    } else {
        ps_print_info("bfgx is not opened, start wifi system reset!\n");
        ret = wifi_system_reset();
    }

    if (ret != EXCEPTION_SUCCESS) {
        ps_print_err("wifi execption recovery fail!\n");
        return ret;
    }

    if (pst_exception_data->wifi_callback->wifi_recovery_complete == NULL) {
        ps_print_err("wifi recovery complete callback not regist\n");
        return -EXCEPTION_FAIL;
    }

    pst_exception_data->wifi_callback->wifi_recovery_complete();

    ps_print_info("wifi recovery complete\n");

    return EXCEPTION_SUCCESS;
}

/*
 * 函 数 名  : wifi_subsystem_reset
 * 功能描述  : wifi子系统复位，通过uart复位解复位WCPU,重新加载wifi firmware
 * 返 回 值  : 异常处理成功返回0，否则返回1
 */
int32 wifi_subsystem_reset(void)
{
    oal_int32 ret;
    struct pm_drv_data *pm_data = pm_get_drvdata();
    int32 l_subchip_type = get_hi110x_subchip_type();

    if (pm_data == NULL) {
        ps_print_err("pm_data is NULL!\n");
        return -EXCEPTION_FAIL;
    }

    switch (l_subchip_type) {
        case BOARD_VERSION_HI1102A:
            if (uart_reset_wcpu() != EXCEPTION_SUCCESS) {
                ps_print_err("uart reset wcpu failed\n");
                return -EXCEPTION_FAIL;
            }
            if (hi1102a_wifi_subsys_reset() != EXCEPTION_SUCCESS) {
                ps_print_err("wifi reset failed\n");
                return -EXCEPTION_FAIL;
            }
            break;
        default:
            ps_print_err("wifi subsys reset error! subchip=%d\n", l_subchip_type);
            return -EXCEPTION_FAIL;
    }

    hcc_bus_power_action(hcc_get_current_110x_bus(), HCC_BUS_POWER_PATCH_LOAD_PREPARE);

    if (firmware_download_function(WIFI_CFG) != EXCEPTION_SUCCESS) {
        ps_print_err("wifi firmware download failed\n");
        return -EXCEPTION_FAIL;
    }

    ret = hcc_bus_power_action(hcc_get_current_110x_bus(), HCC_BUS_POWER_PATCH_LAUCH);
    if (ret != 0) {
        ps_print_err("HCC_BUS_POWER_PATCH_LAUCH failed, ret=%d", ret);
        chr_exception_report(CHR_PLATFORM_EXCEPTION_EVENTID, CHR_SYSTEM_PLAT, CHR_LAYER_DRV,
                             CHR_PLT_DRV_EVENT_DFR, CHR_PLAT_DRV_ERROR_WIFI_SUBSYS_WCPU_BOOTUP);

        return -EXCEPTION_FAIL;
    }

#ifdef _PRE_PLAT_FEATURE_CUSTOMIZE
    // 下发定制化参数到device去
#if (_PRE_PRODUCT_ID == _PRE_PRODUCT_ID_HI1103_HOST)
    hwifi_hcc_customize_h2d_data_cfg();
#endif
#endif

    plat_dfr_sysrst_type_cnt_inc(DFR_SINGLE_SYS_RST, SUBSYS_WIFI);
    chr_exception_report(CHR_PLATFORM_EXCEPTION_EVENTID, CHR_SYSTEM_PLAT, CHR_LAYER_DRV,
                         CHR_PLT_DRV_EVENT_DFR, CHR_PLAT_DRV_ERROR_WIFI_SUBSYS_DFR_SUCC);

    return EXCEPTION_SUCCESS;
}

OAL_STATIC int32_t wifi_system_reset_recovery_bfgx(void)
{
    uint64_t timeleft;
    struct ps_core_s *ps_core_d = NULL;
    struct pm_drv_data *pm_data = pm_get_drvdata();

    ps_get_core_reference(&ps_core_d);
    if (unlikely(ps_core_d == NULL)) {
        ps_print_err("ps_core_d is NULL\n");
        return -EXCEPTION_FAIL;
    }

    if (unlikely(pm_data == NULL)) {
        ps_print_err("pm_data is NULL!\n");
        return -EXCEPTION_FAIL;
    }

    oal_reinit_completion(pm_data->dev_bootok_ack_comp);
    atomic_set(&pm_data->bfg_needwait_devboot_flag, NEED_SET_FLAG);

    if (open_tty_drv(ps_core_d->pm_data) != 0) {
        ps_print_err("open_tty_drv failed\n");
        return -EXCEPTION_FAIL;
    }

    if (wlan_pm_open_bcpu() != EXCEPTION_SUCCESS) {
        ps_print_err("wifi reset bcpu fail\n");
        if (release_tty_drv(ps_core_d->pm_data) != 0) {
            ps_print_warning("wifi_system_reset, release_tty_drv fail, line = %d\n", __LINE__);
        }
        atomic_set(&pm_data->bfg_needwait_devboot_flag, NONEED_SET_FLAG);
        chr_exception_report(CHR_PLATFORM_EXCEPTION_EVENTID, CHR_SYSTEM_PLAT, CHR_LAYER_DRV,
                             CHR_PLT_DRV_EVENT_DFR, CHR_PLAT_DRV_ERROR_WIFI_SYSTEM_RST_OPEN_BCPU);

        return -EXCEPTION_FAIL;
    }

    timeleft = wait_for_completion_timeout(&pm_data->dev_bootok_ack_comp, msecs_to_jiffies(WAIT_BFGX_BOOTOK_TIME));
    if (!timeleft) {
        ps_print_err("wait bfgx boot ok timeout\n");
        atomic_set(&pm_data->bfg_needwait_devboot_flag, NONEED_SET_FLAG);
        chr_exception_report(CHR_PLATFORM_EXCEPTION_EVENTID, CHR_SYSTEM_PLAT, CHR_LAYER_DRV,
                             CHR_PLT_DRV_EVENT_DFR, CHR_PLAT_DRV_ERROR_WIFI_SYSTEM_RST_BCPU_BOOTUP);

        return -EXCEPTION_FAIL;
    }

    atomic_set(&pm_data->bfg_needwait_devboot_flag, NONEED_SET_FLAG);

    bfgx_pm_feature_set();

    /* 恢复bfgx业务状态 */
    if (bfgx_status_recovery() != EXCEPTION_SUCCESS) {
        ps_print_err("bfgx status revocery failed!\n");
        chr_exception_report(CHR_PLATFORM_EXCEPTION_EVENTID, CHR_SYSTEM_PLAT, CHR_LAYER_DRV,
                             CHR_PLT_DRV_EVENT_RST, CHR_PLAT_DRV_ERROR_WIFI_SYSTEM_RST_BFGX_RECOVERY);

        return -EXCEPTION_FAIL;
    }

    return EXCEPTION_SUCCESS;
}

OAL_STATIC void wifi_system_reset_pre_bfgx(void)
{
    struct ps_core_s *ps_core_d = NULL;
    struct pm_drv_data *pm_data = pm_get_drvdata();

    ps_get_core_reference(&ps_core_d);
    if (unlikely(ps_core_d == NULL)) {
        ps_print_err("ps_core_d is NULL\n");
        return;
    }

    if (unlikely(pm_data == NULL)) {
        ps_print_err("pm_data is NULL!\n");
        return;
    }

    bfgx_print_subsys_state();
    del_timer_sync(&ps_core_d->ps_pm->pm_priv_data->bfg_timer);
    pm_data->bfg_timer_mod_cnt = 0;
    pm_data->bfg_timer_mod_cnt_pre = 0;

    ps_core_d->ps_pm->operate_beat_timer(BEAT_TIMER_DELETE);
    if (release_tty_drv(ps_core_d->pm_data) != 0) {
        ps_print_warning("wifi_system_reset, release_tty_drv fail, line = %d\n", __LINE__);
    }
}

/*
 * 函 数 名  : wifi_system_reset
 * 功能描述  : wifi全系统复位，对device重新上电，bfgn和wifi firmware重新加载，
 *              恢复全系统复位前bfgn的业务
 * 返 回 值  : 异常处理成功返回0，否则返回1
 */
int32 wifi_system_reset(void)
{
    int32_t ret;
    uint8_t dfr_type = DFR_SINGLE_SYS_RST;

    /* 重新上电，firmware重新加载 */
    hcc_bus_power_action(hcc_get_current_110x_bus(), HCC_BUS_POWER_PATCH_LOAD_PREPARE);

    if (!bfgx_is_shutdown()) {
        dfr_type = DFR_ALL_SYS_RST;
        wifi_system_reset_pre_bfgx();
    }

    ps_print_info("wifi system reset, board power on\n");
    ret = board_power_reset(WLAN_POWER);
    if (ret) {
        ps_print_err("board_power_reset wlan failed=%d\n", ret);
        return -EXCEPTION_FAIL;
    }

    if (firmware_download_function(BFGX_AND_WIFI_CFG) != EXCEPTION_SUCCESS) {
        ps_print_err("hi110x system power reset failed!\n");
        return -EXCEPTION_FAIL;
    }

    ret = hcc_bus_power_action(hcc_get_current_110x_bus(), HCC_BUS_POWER_PATCH_LAUCH);
    if (ret != 0) {
        ps_print_err("HCC_BUS_POWER_PATCH_LAUCH failed ret=%d !!!!!!", ret);
        chr_exception_report(CHR_PLATFORM_EXCEPTION_EVENTID, CHR_SYSTEM_PLAT, CHR_LAYER_DRV,
                             CHR_PLT_DRV_EVENT_RST, CHR_PLAT_DRV_ERROR_WIFI_SYSTEM_RST_WIFI_BOOTUP);

        return -EXCEPTION_FAIL;
    }

#ifdef _PRE_PLAT_FEATURE_CUSTOMIZE
    // 下发定制化参数到device去
#if (_PRE_PRODUCT_ID == _PRE_PRODUCT_ID_HI1103_HOST)
    hwifi_hcc_customize_h2d_data_cfg();
#endif
#endif
    if (!bfgx_is_shutdown()) {
        if (wifi_system_reset_recovery_bfgx() != EXCEPTION_SUCCESS) {
            return -EXCEPTION_FAIL;
        }
    } else {
        /* 单wifi情况下，需要拉低bcpu en */
        hi1102a_bfgx_disable();
    }

    plat_dfr_sysrst_type_cnt_inc(dfr_type, SUBSYS_WIFI);
    chr_exception_report(CHR_PLATFORM_EXCEPTION_EVENTID, CHR_SYSTEM_PLAT, CHR_LAYER_DRV,
                         CHR_PLT_DRV_EVENT_RST, CHR_PLAT_DRV_ERROR_WIFI_SYSTEM_DFR_SUCC);

    return EXCEPTION_SUCCESS;
}

/*
 * 函 数 名  : wifi_status_recovery
 * 功能描述  : 全系统复位以后，恢复wifi业务函数
 * 输入参数  : st_exception_info *exception结构体指针，保存了异常产生的系统wifi
 *             还是BFGN，对于BFGN还保存了产生异常的子业务，以及异常的类型
 * 返 回 值  : 异常处理成功返回0，否则返回1
 */
int32 wifi_status_recovery(void)
{
    struct st_exception_info *pst_exception_data = NULL;

    get_exception_info_reference(&pst_exception_data);
    if (pst_exception_data == NULL) {
        ps_print_err("get exception info reference is error\n");
        return -EXCEPTION_FAIL;
    }

    if (wifi_subsystem_reset() != EXCEPTION_SUCCESS) {
        ps_print_err("wifi subsystem reset failed\n");
        return -EXCEPTION_FAIL;
    }

    if (pst_exception_data->wifi_callback->wifi_recovery_complete == NULL) {
        ps_print_err("wifi recovery complete callback not regist\n");
        return -EXCEPTION_FAIL;
    }

    pst_exception_data->wifi_callback->wifi_recovery_complete();

    ps_print_info("wifi recovery complete\n");

    return EXCEPTION_SUCCESS;
}

int32 wifi_open_bcpu_set(uint8 enable)
{
    struct st_exception_info *pst_exception_data = NULL;
    uint64 timeleft;
    struct ps_core_s *ps_core_d = NULL;
    struct pm_drv_data *pm_data = pm_get_drvdata();

    if (pm_data == NULL) {
        ps_print_err("pm_data is NULL!\n");
        return -EXCEPTION_FAIL;
    }

    ps_get_core_reference(&ps_core_d);
    if (unlikely(ps_core_d == NULL)) {
        ps_print_err("ps_core_d is err\n");
        return -EXCEPTION_FAIL;
    }

    get_exception_info_reference(&pst_exception_data);
    if (pst_exception_data == NULL) {
        ps_print_err("get exception info reference is error\n");
        return -EXCEPTION_FAIL;
    }

    if (!enable) {
        ps_print_info("wifi_open_bcpu_enable flag disable\n");
        pst_exception_data->wifi_open_bcpu_enable = false;
        return EXCEPTION_SUCCESS;
    }

    ps_print_info("wifi_open_bcpu_enable flag enable\n");
    pst_exception_data->wifi_open_bcpu_enable = true;

    mutex_lock(&pm_data->host_mutex);

    if (release_tty_drv(ps_core_d->pm_data) != SUCCESS) {
        ps_print_err("close tty is err!");
        mutex_unlock(&pm_data->host_mutex);
        return -EXCEPTION_FAIL;
    }

    if (open_tty_drv(ps_core_d->pm_data) != SUCCESS) {
        ps_print_err("open tty is err!\n");
        mutex_unlock(&pm_data->host_mutex);
        return -EXCEPTION_FAIL;
    }

    if (wlan_pm_open_bcpu() != OAL_SUCC) {
        mutex_unlock(&pm_data->host_mutex);

        chr_exception_report(CHR_PLATFORM_EXCEPTION_EVENTID, CHR_SYSTEM_PLAT, CHR_LAYER_DRV,
                             CHR_PLT_DRV_EVENT_OPEN, CHR_PLAT_DRV_ERROR_WIFI_OPEN_BCPU);

        return -EXCEPTION_FAIL;
    }

    timeleft = wait_for_completion_timeout(&pm_data->dev_bootok_ack_comp, msecs_to_jiffies(WAIT_BFGX_BOOTOK_TIME));
    if (!timeleft) {
        ps_print_err("wait BFGX boot ok timeout\n");
        mutex_unlock(&pm_data->host_mutex);
        return -EXCEPTION_FAIL;
    }

    mutex_unlock(&pm_data->host_mutex);

    return EXCEPTION_SUCCESS;
}

EXPORT_SYMBOL(wifi_open_bcpu_set);

int32 wifi_device_reset(void)
{
    struct ps_core_s *ps_core_d = NULL;
    int32 l_subchip_type = get_hi110x_subchip_type();

    ps_get_core_reference(&ps_core_d);
    if (ps_core_d == NULL) {
        ps_print_err("ps_core_d is NULL\n");
        return -EXCEPTION_FAIL;
    }

    if (l_subchip_type == BOARD_VERSION_HI1102A) { /* 1102a 通过w_en复位 */
        ps_print_info("reset wifi device by w_en gpio\n");
        hi1102a_wifi_disable();
        if (hi1102a_wifi_enable()) {
            ps_print_err("hi1102a wifi enable failed\n");
            return -EXCEPTION_FAIL;
        }
        return EXCEPTION_SUCCESS;
    } else {
        ps_print_err("subchip_type is error : %u.\n", l_subchip_type);
        return -EXCEPTION_FAIL;
    }
}

/*
 * 函 数 名  : wifi_exception_mem_dump
 * 功能描述  : 全系统复位，firmware重新加载的时候，导出device指定地址的内存
 * 输入参数  : pst_mem_dump_info  : 需要读取的内存信息
 *             count              : 需要读取的内存块个数
 * 返 回 值  : 异常处理成功返回0，否则返回1
 */
int32 wifi_exception_mem_dump(struct st_wifi_dump_mem_info *pst_mem_dump_info, uint32 count, oal_int32 excep_type)
{
    int32 ret;
    int32 reset_flag = 0;
    hcc_bus_dev *pst_bus_dev = NULL;
    struct pm_drv_data *pm_data = pm_get_drvdata();

    if (pm_data == NULL) {
        ps_print_err("pm_data is NULL!\n");
        return -EXCEPTION_FAIL;
    }

    if (excep_type != WIFI_DEVICE_PANIC && excep_type != WIFI_TRANS_FAIL) {
        ps_print_err("unsupport exception type :%d\n", excep_type);
        return -EXCEPTION_FAIL;
    }

    mutex_lock(&pm_data->host_mutex);

    if (wlan_is_shutdown()) {
        ps_print_err("[E]dfr ignored, wifi shutdown before memdump\n");
        mutex_unlock(&pm_data->host_mutex);
        return -EXCEPTION_FAIL;
    }

retry:
    if (excep_type != WIFI_DEVICE_PANIC) {
        if (wifi_device_reset() != EXCEPTION_SUCCESS) {
            ps_print_err("wifi device reset fail, exception type :%d\n", excep_type);
            mutex_unlock(&pm_data->host_mutex);
            return -EXCEPTION_FAIL;
        }
        reset_flag = 1;
        hcc_change_state_exception();
    }

    pst_bus_dev = hcc_get_bus_dev(HCC_CHIP_110X_DEV);
    if (pst_bus_dev != NULL) {
        ret = hcc_bus_reinit(pst_bus_dev->cur_bus);
    } else {
        ret = -OAL_EIO;
    }

    if (ret != OAL_SUCC) {
        ps_print_err("wifi mem dump:current bus reinit failed, ret=[%d]\n", ret);
        if (!reset_flag) {
            ps_print_err("reinit failed, try to reset wifi\n");
            excep_type = WIFI_TRANS_FAIL;
            goto retry;
        }
        mutex_unlock(&pm_data->host_mutex);
        return -EXCEPTION_FAIL;
    }

    ret = wifi_device_mem_dump(pst_mem_dump_info, count);
    hcc_bus_disable_state(pm_data->pst_wlan_pm_info->pst_bus, OAL_BUS_STATE_ALL);
    if (ret < 0) {
        ps_print_err("wifi_device_mem_dump failed, ret=[%d]\n", ret);
        if (!reset_flag) {
            ps_print_err("memdump failed, try to reset wifi\n");
            excep_type = WIFI_TRANS_FAIL;
            goto retry;
        }
        mutex_unlock(&pm_data->host_mutex);
        return -EXCEPTION_FAIL;
    }

    mutex_unlock(&pm_data->host_mutex);

    return EXCEPTION_SUCCESS;
}

EXPORT_SYMBOL(wifi_exception_mem_dump);

/*
 * 函 数 名  : wifi_exception_work_submit
 * 功能描述  : 异常恢复动作触发接口
 */
int32 wifi_exception_work_submit(uint32 wifi_excp_type)
{
    oal_int32 ret;
    struct st_exception_info *pst_exception_data = NULL;
    hcc_bus *hi_bus = hcc_get_current_110x_bus();

    get_exception_info_reference(&pst_exception_data);
    if ((pst_exception_data == NULL) || (hi_bus == NULL)) {
        ps_print_err("get exception info reference is error\n");
        return -EXCEPTION_FAIL;
    }

    if (pst_exception_data->exception_reset_enable != PLAT_EXCEPTION_ENABLE) {
        ps_print_info("palt exception reset not enable!");
        return EXCEPTION_SUCCESS;
    }

#ifdef _PRE_WLAN_FEATURE_DFR
    if (oal_work_is_busy(&g_exception_info->wifi_excp_worker)) {
        ps_print_warning("WIFI DFR Worker is Processing:doing wifi excp_work...need't submit\n");
        return -OAL_EBUSY;
    } else if (oal_work_is_busy(&g_exception_info->wifi_excp_recovery_worker)) {
        ps_print_warning(
            "WIFI DFR Recovery_Worker is Processing:doing wifi wifi_excp_recovery_worker ...need't submit\n");
        return -OAL_EBUSY;
    } else if ((atomic_read(&pst_exception_data->is_reseting_device) != PLAT_EXCEPTION_RESET_IDLE) &&
               (pst_exception_data->subsys_type == SUBSYS_WIFI)) {
        ps_print_warning("WIFI DFR Recovery_Worker is Processing:doing plat wifi recovery ...need't submit\n");
        return -OAL_EBUSY;
    } else {
        ret = wifi_exception_mem_dump(hi_bus->mem_info, hi_bus->mem_size, hi_bus->bus_excp_type);
        if (ret < 0) {
            ps_print_err("Panic File Save Failed!");
        } else {
            ps_print_info("Panic File Save OK!");
        }

        ps_print_err("WiFi DFR Worker Submit, excp_type[%d]\n", wifi_excp_type);
        g_exception_info->wifi_excp_type = wifi_excp_type;
        hcc_bus_disable_state(hi_bus, OAL_BUS_STATE_ALL);
        queue_work(g_exception_info->wifi_exception_workqueue, &g_exception_info->wifi_excp_worker);
    }
#else
    ps_print_warning("Geting WIFI DFR, but _PRE_WLAN_FEATURE_DFR not open!");
#endif  // _PRE_WLAN_FEATURE_DFR
    return OAL_SUCC;
}
EXPORT_SYMBOL(g_exception_info);
EXPORT_SYMBOL(wifi_exception_work_submit);

oal_workqueue_stru *wifi_get_exception_workqueue(oal_void)
{
    if (g_exception_info == NULL) {
        return NULL;
    }
    return g_exception_info->wifi_exception_workqueue;
}
EXPORT_SYMBOL(wifi_get_exception_workqueue);

int32 prepare_to_recv_bfgx_stack(uint32 len)
{
    if (g_recvd_block_count > BFGX_MEM_DUMP_BLOCK_COUNT - 1) {
        ps_print_err("bfgx mem dump have recvd [%d] blocks\n", g_recvd_block_count);
        return -EXCEPTION_FAIL;
    }

    if (g_bfgx_mem_dump[g_recvd_block_count].exception_mem_addr == NULL) {
        g_bfgx_mem_dump[g_recvd_block_count].exception_mem_addr = (uint8 *)os_vmalloc_gfp(len);
        if (g_bfgx_mem_dump[g_recvd_block_count].exception_mem_addr == NULL) {
            ps_print_err("prepare mem to recv bfgx stack failed\n");
            return -EXCEPTION_FAIL;
        } else {
            g_bfgx_mem_dump[g_recvd_block_count].recved_size = 0;
            g_bfgx_mem_dump[g_recvd_block_count].total_size = len;
            g_bfgx_mem_dump[g_recvd_block_count].file_name = g_bfgx_mem_file_name[g_recvd_block_count];
            ps_print_info("prepare mem [%d] to recv bfgx stack\n", len);
        }
    }

    return EXCEPTION_SUCCESS;
}

void free_bfgx_stack_dump_mem(void)
{
    uint32 i = 0;

    for (i = 0; i < BFGX_MEM_DUMP_BLOCK_COUNT; i++) {
        if (g_bfgx_mem_dump[i].exception_mem_addr != NULL) {
            os_mem_vfree(g_bfgx_mem_dump[i].exception_mem_addr);
            g_bfgx_mem_dump[i].recved_size = 0;
            g_bfgx_mem_dump[i].total_size = 0;
            g_bfgx_mem_dump[i].file_name = NULL;
            g_bfgx_mem_dump[i].exception_mem_addr = NULL;
        }
    }

    g_recvd_block_count = 0;
}

/*
 * 函 数 名  : bfgx_exception_handler
 * 功能描述  : bfgx异常处理入口函数，根据异常类型，调用不同的处理函数
 * 返 回 值  : 异常处理成功返回0，否则返回1
 */
int32 bfgx_exception_handler(void)
{
    int32 ret = -EXCEPTION_FAIL;
    uint32 exception_type;
    struct st_exception_info *pst_exception_data = NULL;
    struct pm_drv_data *pm_data = pm_get_drvdata();

    get_exception_info_reference(&pst_exception_data);
    if (pst_exception_data == NULL) {
        ps_print_err("pst_exception_data is NULL!\n");
        return -EXCEPTION_FAIL;
    }

    if (pm_data == NULL) {
        ps_print_err("pm_data is NULL!\n");
        return -EXCEPTION_FAIL;
    }

    cancel_work_sync(&pm_data->send_allow_sleep_work);
    del_timer_sync(&pm_data->bfg_timer);
    pm_data->bfg_timer_mod_cnt = 0;
    pm_data->bfg_timer_mod_cnt_pre = 0;

    if (pst_exception_data->excetion_type == BFGX_BEATHEART_TIMEOUT) {
        /* bfgx异常，先删除bfg timer和心跳timer，防止重复触发bfgx异常 */
        pm_data->ps_pm_interface->operate_beat_timer(BEAT_TIMER_DELETE);
    }

    exception_type = pst_exception_data->excetion_type;

    /* ioctl下来的异常执行线程复位流程,当前执行系统全复位 */
    if (exception_type == BFGX_TIMER_TIMEOUT || exception_type == BFGX_ARP_TIMEOUT) {
        ret = bfgx_subthread_reset();
    } else {
        /* 异常恢复之前，尝试用平台命令读栈，不能保证一定成功，因为此时uart可能不通 */
        bfgx_dump_stack();

        ret = bfgx_subsystem_reset();
    }

    if (ret != EXCEPTION_SUCCESS) {
        ps_print_err("bfgx exception recovery fail!, exception_type = [%u]\n", exception_type);
        del_timer_sync(&pm_data->bfg_timer);
        pm_data->bfg_timer_mod_cnt = 0;
        pm_data->bfg_timer_mod_cnt_pre = 0;

        pm_data->ps_pm_interface->operate_beat_timer(BEAT_TIMER_DELETE);
        return ret;
    }

    return EXCEPTION_SUCCESS;
}

/*
 * 函 数 名  : bfgx_subthread_reset
 * 功能描述  : bfgx子线程复位函数，通过uart下发复位子线程命令，并等待复位
 *             成功ACK，收到ACK，异常处理完成，否则进入BFGX子系统复位流程
 * 返 回 值  : 异常处理成功返回0，否则返回1
 */
int32 bfgx_subthread_reset(void)
{
    /* 这里执行芯片全系统复位，保证芯片下电 */
    return bfgx_system_reset();
}

int32 wifi_reset_bfgx(void)
{
    uint64 timeleft;
    struct pm_drv_data *pm_data = pm_get_drvdata();

    if (pm_data == NULL) {
        ps_print_err("pm_data is NULL!\n");
        return -FAILURE;
    }

    if (wlan_pm_shutdown_bcpu_cmd() != EXCEPTION_SUCCESS) {
        ps_print_err("wifi shutdown bcpu fail\n");
        chr_exception_report(CHR_PLATFORM_EXCEPTION_EVENTID, CHR_SYSTEM_PLAT, CHR_LAYER_DRV,
                             CHR_PLT_DRV_EVENT_RST, CHR_PLAT_DRV_ERROR_WIFI_RST_BCPU_CLOSE_BCPU);

        return -EXCEPTION_FAIL;
    }

    oal_reinit_completion(pm_data->dev_bootok_ack_comp);

    if (wlan_pm_open_bcpu() != EXCEPTION_SUCCESS) {
        ps_print_err("wifi reset bcpu fail\n");
        chr_exception_report(CHR_PLATFORM_EXCEPTION_EVENTID, CHR_SYSTEM_PLAT, CHR_LAYER_DRV,
                             CHR_PLT_DRV_EVENT_RST, CHR_PLAT_DRV_ERROR_WIFI_RST_BCPU_OPEN_BCPU);

        return -EXCEPTION_FAIL;
    }
    hi1102a_bfgx_subsys_reset();
    timeleft = wait_for_completion_timeout(&pm_data->dev_bootok_ack_comp, msecs_to_jiffies(WAIT_BFGX_BOOTOK_TIME));
    if (!timeleft) {
        ps_print_err("wait bfgx boot ok timeout\n");
        chr_exception_report(CHR_PLATFORM_EXCEPTION_EVENTID, CHR_SYSTEM_PLAT, CHR_LAYER_DRV,
                             CHR_PLT_DRV_EVENT_RST, CHR_PLAT_DRV_ERROR_WIFI_RST_BCPU_BOOTUP);

        return -EXCEPTION_FAIL;
    }

    return EXCEPTION_SUCCESS;
}

/*
 * 函 数 名  : __bfgx_subsystem_reset
 * 功能描述  : bfgx子系统复位函数，进入该函数，说明wifi已开，通过sdio下发复位
 *             解复位BCPU命令，进行bfgx子系统复位。子系统复位完成以后需要恢复
 *             复位前bfgin的业务
 * 返 回 值  : 异常处理成功返回0，否则返回1
 */
int32 __bfgx_subsystem_reset(void)
{
    struct ps_core_s *ps_core_d = NULL;
    struct pm_drv_data *pm_data = pm_get_drvdata();
    int32 l_subchip_type = get_hi110x_subchip_type();

    if (pm_data == NULL) {
        ps_print_err("pm_data is NULL!\n");
        return -FAILURE;
    }

    ps_get_core_reference(&ps_core_d);
    if (unlikely(ps_core_d == NULL)) {
        ps_print_err("ps_core_d is err\n");
        return -EXCEPTION_FAIL;
    }

    if (ps_chk_bfg_active(ps_core_d) == false) {
        ps_print_err("bfgx no subsys is opened\n");
        return EXCEPTION_SUCCESS;
    }

    if (release_tty_drv(ps_core_d->pm_data) != EXCEPTION_SUCCESS) {
        ps_print_err("close tty is err!\n");
        if (!is_tty_open(ps_core_d->pm_data)) {
            return -EXCEPTION_FAIL;
        }
    }

    if (open_tty_drv(ps_core_d->pm_data) != EXCEPTION_SUCCESS) {
        ps_print_err("open tty is err!\n");
        return -EXCEPTION_FAIL;
    }

    atomic_set(&pm_data->bfg_needwait_devboot_flag, NEED_SET_FLAG);

    switch (l_subchip_type) {
        case BOARD_VERSION_HI1102A:
            if (wifi_reset_bfgx() != EXCEPTION_SUCCESS) {
                ps_print_err("wifi reset bfgx fail\n");
                atomic_set(&pm_data->bfg_needwait_devboot_flag, NONEED_SET_FLAG);
                return -EXCEPTION_FAIL;
            }
            break;
        default:
            ps_print_err("bfgx subsys reset error! subchip=%d\n", l_subchip_type);
            atomic_set(&pm_data->bfg_needwait_devboot_flag, NONEED_SET_FLAG);
            return -EXCEPTION_FAIL;
    }

    atomic_set(&pm_data->bfg_needwait_devboot_flag, NONEED_SET_FLAG);

    bfgx_pm_feature_set();

    if (bfgx_status_recovery() != EXCEPTION_SUCCESS) {
        ps_print_err("bfgx recovery status failed\n");

        chr_exception_report(CHR_PLATFORM_EXCEPTION_EVENTID, CHR_SYSTEM_PLAT, CHR_LAYER_DRV,
                             CHR_PLT_DRV_EVENT_RST, CHR_PLAT_DRV_ERROR_BFGX_SUB_SYSTEM_RST_RECOVERY);
        return -EXCEPTION_FAIL;
    }

    plat_dfr_sysrst_type_cnt_inc(DFR_SINGLE_SYS_RST, SUBSYS_BFGX);

    return EXCEPTION_SUCCESS;
}

/*
 * 函 数 名  : bfgx_subsystem_reset
 * 功能描述  : bfgx子系统复位函数，如果wifi打开，则通过sdio下发复位解复位BCPU命令，
 *             进行bfgx子系统复位，wifi没开，进入bfgx全系统复位流程。如果子系统
 *             复位不能将异常恢复也会进入全系统复位流程。子系统复位完成以后需要
 *             恢复复位前bfgin的业务
 * 返 回 值  : 异常处理成功返回0，否则返回1
 */
int32 bfgx_subsystem_reset(void)
{
    chr_exception_report(CHR_PLATFORM_EXCEPTION_EVENTID, CHR_SYSTEM_PLAT, CHR_LAYER_DRV,
                         CHR_PLT_DRV_EVENT_RST, CHR_PLAT_DRV_ERROR_BFGX_SUB_SYSTEM_RESET);

    if (!wlan_is_shutdown()) {
        ps_print_info("wifi is opened, start bfgx subsystem reset!\n");

        if (!is_subsystem_rst_enable()) {
            if (__bfgx_subsystem_reset() != EXCEPTION_SUCCESS) {
                ps_print_err("bfgx subsystem reset failed, start bfgx system reset!\n");
                return bfgx_system_reset();
            }
        } else {
            ps_print_info("in debug mode,skip subsystem rst,do bfgx system reset!\n");
            return bfgx_system_reset();
        }

        return EXCEPTION_SUCCESS;
    } else {
        ps_print_info("wifi is not opened, start bfgx system reset!\n");
        return bfgx_system_reset();
    }
}

int32 bfgx_power_reset(void)
{
    int32 ret;
    uint64 timeleft;
    struct ps_core_s *ps_core_d = NULL;
    struct pm_drv_data *pm_data = pm_get_drvdata();

    ps_get_core_reference(&ps_core_d);
    if (unlikely(ps_core_d == NULL)) {
        ps_print_err("ps_core_d is err\n");
        return -FAILURE;
    }

    if (pm_data == NULL) {
        ps_print_err("pm_data is NULL!\n");
        return -FAILURE;
    }

    if (release_tty_drv(ps_core_d->pm_data) != 0) {
        ps_print_warning("bfgx_power_reset, release_tty_drv fail\n");
    }

    ps_print_info("bfgx system reset, board power on\n");
    ret = board_power_reset(BFGX_POWER);
    if (ret) {
        ps_print_err("board_power_reset bfgx failed=%d\n", ret);
        return -EXCEPTION_FAIL;
    }

    if (open_tty_drv(ps_core_d->pm_data) != 0) {
        ps_print_warning("bfgx_power_reset, open_tty_drv fail\n");
    }

    oal_reinit_completion(pm_data->dev_bootok_ack_comp);
    atomic_set(&pm_data->bfg_needwait_devboot_flag, NEED_SET_FLAG);

    if (firmware_download_function(BFGX_CFG) != EXCEPTION_SUCCESS) {
        hcc_bus_disable_state(pm_data->pst_wlan_pm_info->pst_bus, OAL_BUS_STATE_ALL);
        ps_print_err("bfgx power reset failed!\n");
        atomic_set(&pm_data->bfg_needwait_devboot_flag, NONEED_SET_FLAG);
        return -EXCEPTION_FAIL;
    }
    hcc_bus_disable_state(pm_data->pst_wlan_pm_info->pst_bus, OAL_BUS_STATE_ALL);

    timeleft = wait_for_completion_timeout(&pm_data->dev_bootok_ack_comp, msecs_to_jiffies(WAIT_BFGX_BOOTOK_TIME));
    if (!timeleft) {
        ps_print_err("wait bfgx boot ok timeout\n");
        atomic_set(&pm_data->bfg_needwait_devboot_flag, NONEED_SET_FLAG);
        chr_exception_report(CHR_PLATFORM_EXCEPTION_EVENTID, CHR_SYSTEM_PLAT, CHR_LAYER_DRV,
                             CHR_PLT_DRV_EVENT_RST, CHR_PLAT_DRV_ERROR_BCPU_PWR_RST_BOOTUP);

        return -EXCEPTION_FAIL;
    }

    atomic_set(&pm_data->bfg_needwait_devboot_flag, NONEED_SET_FLAG);

    bfgx_pm_feature_set();

    return EXCEPTION_SUCCESS;
}

/*
 * 函 数 名  : bfgx_system_reset
 * 功能描述  : bfgin全系统复位函数，对device重新上电，wifi和bfgin firmware重新
 *             加载，恢复wifi，恢复bfgin业务。
 * 返 回 值  : 异常处理成功返回0，否则返回1
 */
int32 bfgx_system_reset(void)
{
    struct st_wifi_dfr_callback *wifi_dfr_callback = NULL;
    struct st_exception_info *pst_exception_data = NULL;

    chr_exception_report(CHR_PLATFORM_EXCEPTION_EVENTID, CHR_SYSTEM_PLAT, CHR_LAYER_DRV,
                         CHR_PLT_DRV_EVENT_RST, CHR_PLAT_DRV_ERROR_BFGX_SYSTEM_RESET);

    get_exception_info_reference(&pst_exception_data);
    if (pst_exception_data == NULL) {
        ps_print_err("get exception info reference is error\n");
        return -EXCEPTION_FAIL;
    }

    wifi_dfr_callback = pst_exception_data->wifi_callback;
    if (!wlan_is_shutdown()) {
        if (wifi_dfr_callback->wifi_finish_recovery() == false) {
            ps_print_err("wifi already in recovery status\n");
            return -EXCEPTION_FAIL;
        }

        if (wifi_dfr_callback->notify_wifi_to_recovery != NULL) {
            ps_print_info("notify wifi bfgx start to power reset\n");
            pst_exception_data->wifi_callback->notify_wifi_to_recovery();
        }
    }

    /* 重新上电，firmware重新加载 */
    if (bfgx_power_reset() != EXCEPTION_SUCCESS) {
        ps_print_err("bfgx power reset failed!\n");
        return -EXCEPTION_FAIL;
    }

    if (bfgx_status_recovery() != EXCEPTION_SUCCESS) {
        ps_print_err("bfgx status revocery failed!\n");

        chr_exception_report(CHR_PLATFORM_EXCEPTION_EVENTID, CHR_SYSTEM_PLAT, CHR_LAYER_DRV,
                             CHR_PLT_DRV_EVENT_RST, CHR_PLAT_DRV_ERROR_BFGX_SYSTEM_RESET_BFGX_RECOVERY);
        return -EXCEPTION_FAIL;
    }

    if (!wlan_is_shutdown()) {
        if (wifi_status_recovery() != EXCEPTION_SUCCESS) {
            ps_print_err("wifi status revocery failed!\n");

            chr_exception_report(CHR_PLATFORM_EXCEPTION_EVENTID, CHR_SYSTEM_PLAT, CHR_LAYER_DRV,
                                 CHR_PLT_DRV_EVENT_RST, CHR_PLAT_DRV_ERROR_BFGX_SYSTEM_RESET_WIFI_RECOVERY);

            return -EXCEPTION_FAIL;
        }
        plat_dfr_sysrst_type_cnt_inc(DFR_ALL_SYS_RST, SUBSYS_BFGX);
    } else {
        plat_dfr_sysrst_type_cnt_inc(DFR_SINGLE_SYS_RST, SUBSYS_BFGX);
    }

    return EXCEPTION_SUCCESS;
}

/*
 * 函 数 名  : bfgx_recv_dev_mem
 * 功能描述  : 接收bfgx异常时，device上报的栈内存
 * 输入参数  : buf_ptr :uart接收到的占内存bufer首地址
 *             count   :buffer长度
 * 输出参数  : 0表示成功，-1表示失败
 */
int32 bfgx_recv_dev_mem(uint8 *buf_ptr, uint16 count)
{
    struct st_exception_mem_info *pst_mem_info = NULL;
    uint32 offset;

    if (buf_ptr == NULL) {
        ps_print_err("buf_ptr is NULL\n");
        return -EFAIL;
    }

    if (g_recvd_block_count >= BFGX_MEM_DUMP_BLOCK_COUNT) {
        ps_print_err("g_recvd_block_count =[%d]\n", g_recvd_block_count);
        return -EFAIL;
    }

    pst_mem_info = &(g_bfgx_mem_dump[g_recvd_block_count]);

    if (pst_mem_info->exception_mem_addr == NULL) {
        ps_print_err("mem addr is null, g_recvd_block_count=[%d]\n", g_recvd_block_count);
        return -EXCEPTION_FAIL;
    }

    offset = pst_mem_info->recved_size;
    if ((uint32)offset + (uint32)count > pst_mem_info->total_size) {
        ps_print_err("outof buf total size, recved size is [%d], curr recved size is [%d], total size is [%d]\n",
                     offset, count, pst_mem_info->total_size);
        return -EXCEPTION_FAIL;
    } else {
        ps_print_dbg("cpy stack size [%d] to exception mem\n", count);
        if (memcpy_s(pst_mem_info->exception_mem_addr + offset, pst_mem_info->total_size - offset,
                     buf_ptr, count) != EOK) {
            ps_print_err("memcpy failed\n ");
            return -EXCEPTION_FAIL;
        }

        pst_mem_info->recved_size += count;
    }

    if (pst_mem_info->recved_size == pst_mem_info->total_size) {
        g_recvd_block_count++;
        ps_print_info("mem block [%d] recvd done\n", g_recvd_block_count);
    }

    return EXCEPTION_SUCCESS;
}

#ifndef HI110X_HAL_MEMDUMP_ENABLE
/*
 * 函 数 名  : bfgx_store_stack_mem_to_file
 * 功能描述  : 将接收到的bfgx内存保存到文件中
 */
void bfgx_store_stack_mem_to_file(void)
{
    os_kernel_file_stru *fp = NULL;
    char filename[EXCEPTION_FILE_NAME_LEN] = {0};
    uint32 i;
    int32 ret;
    mm_segment_t fs;
    struct st_exception_mem_info *pst_mem_info = NULL;

    fs = get_fs();
    set_fs(KERNEL_DS);
    for (i = 0; i < BFGX_MEM_DUMP_BLOCK_COUNT; i++) {
        pst_mem_info = &(g_bfgx_mem_dump[i]);
        if (pst_mem_info->exception_mem_addr == NULL) {
            ps_print_err("mem addr is null, i=[%d]\n", i);
            continue;
        }
        memset_s(filename, sizeof(filename), 0, sizeof(filename));
        ret = snprintf_s(filename, sizeof(filename), sizeof(filename) - 1, BFGX_DUMP_PATH "/%s_%s.bin",
                         UART_STORE_BFGX_STACK, pst_mem_info->file_name);
        if (ret < 0) {
            ps_print_err("bfgx_store_stack_mem_to_file snprintf_s error, please check!\n");
        }

        /* 打开文件，准备保存接收到的内存 */
        fp = filp_open(filename, O_RDWR | O_CREAT, 0664);
        if (OAL_IS_ERR_OR_NULL(fp)) {
            ps_print_err("create file error,fp = 0x%p,filename:%s\n", fp, filename);
            continue;
        }

        /* 将接收到的内存写入到文件中 */
        ps_print_info("pos = %d\n", (int)fp->f_pos);
        vfs_write(fp, pst_mem_info->exception_mem_addr, pst_mem_info->recved_size, &fp->f_pos);
        filp_close(fp, NULL);
    }
    set_fs(fs);

    /* send cmd to oam_hisi to rotate file */
    plat_send_rotate_cmd_2_app(CMD_READM_BFGX_UART);

    free_bfgx_stack_dump_mem();
}
#endif

/*
 * 函 数 名  : bfgx_dump_stack
 * 功能描述  : 心跳超时时，尝试通过uart读栈，不保证一定能成功，因为此时uart可能不通
 */
void bfgx_dump_stack(void)
{
    uint64 timeleft;
    uint32 exception_type;
    struct ps_core_s *ps_core_d = NULL;
    struct st_exception_info *pst_exception_data = NULL;

    get_exception_info_reference(&pst_exception_data);
    if (pst_exception_data == NULL) {
        ps_print_err("get exception info reference is error\n");
        return;
    }

    exception_type = pst_exception_data->excetion_type;

    if (exception_type != BFGX_LASTWORD_PANIC) {
        return;
    }

    if (unlikely(pst_exception_data->ps_plat_d == NULL)) {
        ps_print_err("pst_exception_data->ps_plat_d is NULL\n");
        return;
    }
    ps_core_d = pst_exception_data->ps_plat_d->core_data;

    oal_reinit_completion(pst_exception_data->wait_read_bfgx_stack);

    /* 等待读栈操作完成 */
    timeleft = wait_for_completion_timeout(&pst_exception_data->wait_read_bfgx_stack,
                                           msecs_to_jiffies(WAIT_BFGX_READ_STACK_TIME));
    if (!timeleft) {
        ps_uart_state_dump(ps_core_d->tty);
        atomic_set(&pst_exception_data->is_memdump_runing, 0);
        ps_print_err("read bfgx stack failed!\n");
    } else {
        ps_print_info("read bfgx stack success!\n");
    }
#ifndef HI110X_HAL_MEMDUMP_ENABLE
    plat_wait_last_rotate_finish();

    bfgx_store_stack_mem_to_file();
#endif
    return;
}

int32 prepare_to_recv_wifi_mem(void)
{
    uint32 malloc_mem_len;
    uint32 index;

    ps_print_info("%s\n", __func__);

    index = g_recvd_wifi_block_index;
    if (index >= UART_WIFI_MEM_DUMP_BOTTOM) {
        ps_print_err("g_recvd_wifi_block_index is error [%d]\n", index);
        return -EXCEPTION_FAIL;
    }

    if (g_uart_wifi_mem_dump[index].exception_mem_addr == NULL) {
        malloc_mem_len = g_uart_read_wifi_mem_info[index].total_size;
        g_uart_wifi_mem_dump[index].exception_mem_addr = (uint8 *)os_vmalloc_gfp(malloc_mem_len);
        if (g_uart_wifi_mem_dump[index].exception_mem_addr == NULL) {
            ps_print_err("prepare mem to recv wifi mem failed\n");
            return -EXCEPTION_FAIL;
        } else {
            g_uart_wifi_mem_dump[index].recved_size = 0;
            g_uart_wifi_mem_dump[index].total_size = malloc_mem_len;
            g_uart_wifi_mem_dump[index].file_name = NULL;
            ps_print_info("prepare mem [%d] to recv wifi mem, index = [%d]\n", malloc_mem_len, index);
        }
    }

    return EXCEPTION_SUCCESS;
}

int32 free_uart_read_wifi_mem(void)
{
    uint32 index;

    ps_print_info("%s\n", __func__);

    index = g_recvd_wifi_block_index;
    if (index >= UART_WIFI_MEM_DUMP_BOTTOM) {
        ps_print_err("g_recvd_wifi_block_index is error [%d]\n", index);
        return -EXCEPTION_FAIL;
    }

    if (g_uart_wifi_mem_dump[index].exception_mem_addr != NULL) {
        os_mem_vfree(g_uart_wifi_mem_dump[index].exception_mem_addr);
        g_uart_wifi_mem_dump[index].recved_size = 0;
        g_uart_wifi_mem_dump[index].total_size = 0;
        g_uart_wifi_mem_dump[index].exception_mem_addr = NULL;
        ps_print_info("vfree uart read wifi mem [%d] success\n", index);
    }

    g_recvd_wifi_block_index = UART_WIFI_MEM_DUMP_BOTTOM;

    return EXCEPTION_SUCCESS;
}

int32 __store_wifi_mem_to_file(void)
{
    os_kernel_file_stru *fp = NULL;
    char filename[EXCEPTION_FILE_NAME_LEN] = {0};
    mm_segment_t fs;
    uint32 index;
    uint32 i;
    int32 ret;
    uint32 block_count;
    uint8 *block_file_name = NULL;
    uint32 block_size;
    uint32 offset = 0;
    struct st_exception_mem_info *pst_mem_info = NULL;

#ifdef PLATFORM_DEBUG_ENABLE
    struct rtc_time tm = {0};
    oal_timespec_stru tv;
    oal_do_gettimeofday(&tv);
    oal_rtc_ktime_to_tm(tv, &tm);
    ps_print_info("%4d-%02d-%02d  %02d:%02d:%02d\n",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec); /* 转换成当前时间 */
#endif
    ps_print_info("%s\n", __func__);

    index = g_recvd_wifi_block_index;
    if (index >= UART_WIFI_MEM_DUMP_BOTTOM) {
        ps_print_err("g_recvd_wifi_block_index [%d] is error\n", index);
        return -EXCEPTION_FAIL;
    }

    pst_mem_info = &(g_uart_wifi_mem_dump[index]);
    if (pst_mem_info->exception_mem_addr == NULL) {
        ps_print_err("mem addr is null, g_recvd_wifi_block_index=[%d]", index);
        return -EXCEPTION_FAIL;
    }

    block_count = g_uart_read_wifi_mem_info[index].block_count;

    fs = get_fs();
    set_fs(KERNEL_DS);
    for (i = 0; i < block_count; i++) {
        block_size = g_uart_read_wifi_mem_info[index].block_info[i].size;
        block_file_name = g_uart_read_wifi_mem_info[index].block_info[i].file_name;
        ret = snprintf_s(filename, sizeof(filename), sizeof(filename) - 1, BFGX_DUMP_PATH "/%s_%s.bin",
                         UART_STORE_WIFI_MEM, block_file_name);
        if (ret < 0) {
            ps_print_err("__store_wifi_mem_to_file snprintf_s error, please check!\n");
        }

        /* 打开文件，准备保存接收到的内存 */
        fp = filp_open(filename, O_RDWR | O_CREAT, 0664);
        if (OAL_IS_ERR_OR_NULL(fp)) {
            ps_print_err("create file error, errno:%ld, filename:%s\n", PTR_ERR(fp), filename);
            set_fs(fs);
            return -EXCEPTION_FAIL;
        }

        /* 将接收到的内存写入到文件中 */
        ps_print_info("pos = %d\n", (int)fp->f_pos);
        vfs_write(fp, pst_mem_info->exception_mem_addr + offset, block_size, &fp->f_pos);
        filp_close(fp, NULL);

        offset += block_size;
    }
    set_fs(fs);

    return EXCEPTION_SUCCESS;
}

void store_wifi_mem_to_file_work(struct work_struct *work)
{
    struct st_exception_info *pst_exception_data = NULL;

    ps_print_info("%s\n", __func__);

    get_exception_info_reference(&pst_exception_data);
    if (pst_exception_data == NULL) {
        ps_print_err("get exception info reference is error\n");
        return;
    }

    __store_wifi_mem_to_file();

    free_uart_read_wifi_mem();

    complete(&pst_exception_data->wait_uart_read_wifi_mem);

    return;
}

void store_wifi_mem_to_file(void)
{
    struct st_exception_info *pst_exception_data = NULL;

    ps_print_info("%s\n", __func__);

    get_exception_info_reference(&pst_exception_data);
    if (pst_exception_data == NULL) {
        ps_print_err("get exception info reference is error\n");
        return;
    }

    queue_work(pst_exception_data->plat_exception_rst_workqueue, &pst_exception_data->uart_store_wifi_mem_to_file_work);
    return;
}

int32 uart_halt_wcpu(void)
{
    uint64 timeleft;
    hcc_bus *pst_bus = NULL;
    hcc_bus_dev *pst_bus_dev = NULL;
    struct ps_core_s *ps_core_d = NULL;
    struct st_exception_info *pst_exception_data = NULL;

    ps_print_info("%s\n", __func__);

    get_exception_info_reference(&pst_exception_data);
    if (pst_exception_data == NULL) {
        ps_print_err("get exception info reference is error\n");
        return -EXCEPTION_FAIL;
    }

    ps_get_core_reference(&ps_core_d);
    if (unlikely(ps_core_d == NULL)) {
        ps_print_err("ps_core_d is err\n");
        return -EINVAL;
    }

    oal_reinit_completion(pst_exception_data->wait_uart_halt_wcpu);
    ps_uart_state_pre(ps_core_d->tty);
    ps_tx_sys_cmd(ps_core_d, SYS_MSG, SYS_CFG_HALT_WCPU);
    timeleft = wait_for_completion_timeout(&pst_exception_data->wait_uart_halt_wcpu,
                                           msecs_to_jiffies(UART_HALT_WCPU_TIMEOUT));
    if (!timeleft) {
        ps_uart_state_dump(ps_core_d->tty);
        ps_print_err("wait uart halt wcpu ack timeout\n");
        return -ETIMEDOUT;
    }

    pst_bus_dev = hcc_get_bus_dev(HCC_CHIP_110X_DEV);
    if (pst_bus_dev == NULL) {
        ps_print_err("110x dev is null\n");
        return -ENODEV;
    }

    pst_bus = hdev_to_hbus(pst_bus_dev);
    if (pst_bus == NULL) {
        ps_print_err("110x bus is null\n");
        return -ENODEV;
    }

    hcc_bus_wakeup_request(pst_bus);
    return EXCEPTION_SUCCESS;
}

/*
 * 函 数 名  : exception_bcpu_dump_recv
 * 功能描述  : 回调数据处理函数
 * 返 回 值  : 异常处理成功返回0，否则返回1
 */
int32 exception_bcpu_dump_recv(uint8 *str, oal_netbuf_stru *netbuf)
{
    exception_bcpu_dump_header *cmd_header = {0};
    struct st_exception_info *pst_exception_data = NULL;
    int ret;

    get_exception_info_reference(&pst_exception_data);
    if (pst_exception_data == NULL) {
        ps_print_err("get exception info reference is error\n");
        return -EXCEPTION_FAIL;
    }

    if (g_bcpu_dump_buff.mem_addr == NULL) {
        ps_print_err("g_bcpu_dump_buff addr is null\n");
        return -EXCEPTION_FAIL;
    }

    cmd_header = (exception_bcpu_dump_header *)(str);
    ret = memcpy_s(g_bcpu_dump_buff.mem_addr + g_bcpu_dump_buff.data_len,
                   g_bcpu_dump_buff.data_limit - g_bcpu_dump_buff.data_len,
                   str + sizeof(exception_bcpu_dump_header), cmd_header->men_len);
    if (ret != EOK) {
        ps_print_err("memcopy_s error, destlen=%d, srclen=%d\n ",
                     g_bcpu_dump_buff.data_limit - g_bcpu_dump_buff.data_len, cmd_header->men_len);
        return -EXCEPTION_FAIL;
    }
    g_bcpu_dump_buff.data_len += cmd_header->men_len;

    complete(&pst_exception_data->wait_sdio_d2h_dump_ack);

    return EXCEPTION_SUCCESS;
}

void free_buffer_and_netbuf(void)
{
    if (g_bcpu_dump_buff.mem_addr != NULL) {
        os_mem_vfree(g_bcpu_dump_buff.mem_addr);
        g_bcpu_dump_buff.data_limit = 0;
        g_bcpu_dump_buff.data_len = 0;
        g_bcpu_dump_buff.mem_addr = NULL;
    }

    if (g_bcpu_dump_netbuf != NULL) {
        g_bcpu_dump_netbuf = NULL;
    }
}

/*
 * 函 数 名  : sdio_halt_bcpu
 * 功能描述  : sdio send halt bcpu cmd
 * 返 回 值  : 异常处理成功返回0，否则返回1
 */
int32 sdio_halt_bcpu(void)
{
    int32 ret;
    uint64 timeleft;
    int i;
    struct wlan_pm_s *pst_wlan_pm = wlan_pm_get_drv();

    hcc_tx_transfer_lock(hcc_get_110x_handler());

    // wake up wifi
    for (i = 0; i < WAKEUP_RETRY_TIMES; i++) {
        ret = (int32)wlan_pm_wakeup_dev();
        if (ret == OAL_SUCC) {
            break;
        }
    }

    if (ret != EXCEPTION_SUCCESS) {
        ps_print_err("wlan wake up fail!");
        hcc_tx_transfer_unlock(hcc_get_110x_handler());
        return -OAL_FAIL;
    }

    ret = hcc_bus_send_message(pst_wlan_pm->pst_bus, H2D_MSG_HALT_BCPU);
    if (ret == 0) {
        /* 等待device执行命令 */
        timeleft = wait_for_completion_timeout(&pst_wlan_pm->st_halt_bcpu_done,
                                               msecs_to_jiffies(WLAN_HALT_BCPU_TIMEOUT));
        if (timeleft == 0) {
            ps_print_err("sdio halt bcpu failed!\n");
            hcc_tx_transfer_unlock(hcc_get_110x_handler());
            return -OAL_FAIL;
        }
    }
    ps_print_info("halt bcpu sucess!");
    hcc_tx_transfer_unlock(hcc_get_110x_handler());
    return OAL_SUCC;
}

/*
 * 函 数 名  : allocate_data_save_buffer
 * 功能描述  : 分配存储数据buffer
 * 返 回 值  : 异常处理成功返回0，否则返回1
 */
int32 allocate_data_save_buffer(uint32 len)
{
    // 临时buff配置,用于传送数据
    g_bcpu_dump_buff.mem_addr = os_vmalloc_gfp(len);
    if (g_bcpu_dump_buff.mem_addr == NULL) {
        ps_print_err("g_bcpu_dump_buff allocate fail!\n");
        return -EXCEPTION_FAIL;
    }
    g_bcpu_dump_buff.data_limit = len;
    g_bcpu_dump_buff.data_len = 0;
    return EXCEPTION_SUCCESS;
}

/*
 * 函 数 名  : allocate_send_netbuf
 * 功能描述  : 分配netbuf
 * 返 回 值  : 异常处理成功返回0，否则返回1
 */
int32 allocate_send_netbuf(uint32 len)
{
    g_bcpu_dump_netbuf = hcc_netbuf_alloc(len);
    if (g_bcpu_dump_netbuf == NULL) {
        ps_print_err("g_bcpu_dump_netbuf allocate fail !\n");
        return -EXCEPTION_FAIL;
    }

    return EXCEPTION_SUCCESS;
}

void dump_header_init(exception_bcpu_dump_header *header, uint32 align_type, uint32 addr, uint32 send_len)
{
    /* cmd 初始化 */
    header->align_type = align_type;
    header->start_addr = addr;
    header->men_len = send_len;
}

/*
 * 函 数 名  : init_hcc_head_and_send
 * 功能描述  : 初始化hcc发送header
 * 返 回 值  : 异常处理成功返回0，否则返回1
 */
int32 init_hcc_head_and_send(struct hcc_transfer_param st_hcc_transfer_param,
                             struct st_exception_info *pst_exception_data, uint32 wait_time)
{
    uint64 timeleft;
    int32 ret;
    // 发送
    oal_reinit_completion(pst_exception_data->wait_sdio_d2h_dump_ack);
    ret = hcc_tx(hcc_get_110x_handler(), g_bcpu_dump_netbuf, &st_hcc_transfer_param);
    if (ret != OAL_SUCC) {
        ps_print_err("send tx  failed!\n");
        oal_netbuf_free(g_bcpu_dump_netbuf);
        g_bcpu_dump_netbuf = NULL;
        return -EXCEPTION_FAIL;
    }
    /* 等待SDIO读数据完成 */
    timeleft = wait_for_completion_timeout(&pst_exception_data->wait_sdio_d2h_dump_ack, msecs_to_jiffies(wait_time));
    if (!timeleft) {
        ps_print_err("sdio read  failed!\n");

        g_bcpu_dump_netbuf = NULL;
        return -EXCEPTION_FAIL;
    }
    return EXCEPTION_SUCCESS;
}

static int32 sdio_get_data_pre(struct st_exception_info **exception_data)
{
    get_exception_info_reference(exception_data);
    if (*exception_data == NULL) {
        ps_print_err("get exception info reference is error\n");
        return -EXCEPTION_FAIL;
    }

    if (sdio_halt_bcpu() != OAL_SUCC) {
        ps_print_err("halt bcpu error!\n");
        return -EXCEPTION_FAIL;
    }

    return EXCEPTION_SUCCESS;
}

/*
 * 函 数 名  : sdio_get_and_save_data
 * 功能描述  : sdio send cmd save data
 * 返 回 值  : 异常处理成功返回0，否则返回1
 */
int32 sdio_get_and_save_data(exception_bcpu_dump_msg *sdio_read_info, uint32 count)
{
    uint32 header_len;
    uint32 netbuf_len;
    uint32 send_len;
    uint32 index;
    uint32 i = 0;
    int32 ret;
    uint32 buffer_len;
    uint32 max_buff_len;
    uint32 send_total_len;
    uint32 align_type;
    int32 error = EXCEPTION_SUCCESS;
    int8 filename[EXCEPTION_FILE_NAME_LEN] = {0};

    mm_segment_t fs = {0};
    os_kernel_file_stru *fp = {0};

    struct hcc_transfer_param st_hcc_transfer_param = {0};
    struct st_exception_info *pst_exception_data = NULL;
    exception_bcpu_dump_header dump_header = {0};

    ps_print_info("%s\n", __func__);

    if (sdio_get_data_pre(&pst_exception_data) != EXCEPTION_SUCCESS) {
        return -EXCEPTION_FAIL;
    }

    header_len = sizeof(exception_bcpu_dump_header);
    fs = get_fs();
    set_fs(KERNEL_DS);
    for (i = 0; i < count; i++) {
        index = 0;
        ret = snprintf_s(filename, sizeof(filename), sizeof(filename) - 1, BFGX_DUMP_PATH "/%s_%s.bin",
                         SDIO_STORE_BFGX_REGMEM, sdio_read_info[i].file_name);
        if (ret < 0) {
            ps_print_err("sdio_get_and_save_data snprintf_s error, please check!\n");
        }

        /* 准备文件空间 */
        fp = filp_open(filename, O_RDWR | O_CREAT, 0664);
        if (OAL_IS_ERR_OR_NULL(fp)) {
            set_fs(fs);
            ps_print_err("create file error, errno:%ld\n", PTR_ERR(fp));
            return -EXCEPTION_FAIL;
        }
        ps_print_info("%s is dumping...,pos = %d\n", sdio_read_info[i].file_name, (int)fp->f_pos);

        // prepare data buffer
        send_total_len = sdio_read_info[i].men_len;
        align_type = sdio_read_info[i].align_type;
        max_buff_len = (align_type == ALIGN_1_BYTE) ? DUMP_BCPU_MEM_BUFF_LEN : DUMP_BCPU_REG_BUFF_LEN;
        buffer_len = (send_total_len > max_buff_len) ? max_buff_len : sdio_read_info[i].men_len;

        if (allocate_data_save_buffer(buffer_len) != EXCEPTION_SUCCESS) {
            error = -EXCEPTION_FAIL;
            goto exit;
        }

        // send cmd and save data
        while (index < send_total_len) {
            send_len = send_total_len - index;

            if (align_type == ALIGN_1_BYTE) {
                // dump mem set
                if (send_len > DUMP_BCPU_MEM_BUFF_LEN) {
                    send_len = DUMP_BCPU_MEM_BUFF_LEN;
                }
                hcc_hdr_param_init(&st_hcc_transfer_param, HCC_ACTION_TYPE_OAM, DUMP_MEM, 0, 0, DATA_HI_QUEUE);
            } else {  // dump reg set
                if (send_len > DUMP_BCPU_REG_BUFF_LEN) {
                    send_len = DUMP_BCPU_REG_BUFF_LEN;
                }
                hcc_hdr_param_init(&st_hcc_transfer_param, HCC_ACTION_TYPE_OAM, DUMP_REG, 0, 0, DATA_HI_QUEUE);
            }

            netbuf_len = header_len + send_len;
            if (allocate_send_netbuf(netbuf_len) != EXCEPTION_SUCCESS) {
                error = -EXCEPTION_FAIL;
                goto exit;
            }

            dump_header_init(&dump_header, sdio_read_info[i].align_type,
                             sdio_read_info[i].start_addr + index, send_len);
            if (g_bcpu_dump_netbuf == NULL) {
                goto exit;
            }

            if (memcpy_s(oal_netbuf_put(g_bcpu_dump_netbuf, netbuf_len), netbuf_len,
                         &dump_header, sizeof(exception_bcpu_dump_header)) != EOK) {
                ps_print_err("memcpy_s error, destlen=%u, srclen=%lu\n ",
                             netbuf_len, sizeof(exception_bcpu_dump_header));
                goto exit;
            }

            // 发送
            if (init_hcc_head_and_send(st_hcc_transfer_param, pst_exception_data, WIFI_DUMP_BCPU_TIMEOUT) !=
                EXCEPTION_SUCCESS) {
                error = -EXCEPTION_FAIL;
                goto exit;
            }

            vfs_write(fp, g_bcpu_dump_buff.mem_addr, g_bcpu_dump_buff.data_len, &fp->f_pos);

            index += send_len;

            // prepare for next data
            g_bcpu_dump_buff.data_len = 0;
            g_bcpu_dump_netbuf = NULL;
        }
        filp_close(fp, NULL);
        free_buffer_and_netbuf();
    }
    set_fs(fs);
    return EXCEPTION_SUCCESS;
exit:
    set_fs(fs);
    filp_close(fp, NULL);
    free_buffer_and_netbuf();
    complete(&pst_exception_data->wait_sdio_d2h_dump_ack);

    return error;
}

/*
 * 函 数 名  : debug_sdio_read_bfgx_reg_and_mem
 * 功能描述  : sdio read bfgx reg and mem
 * 返 回 值  : 异常处理成功返回0，否则返回1
 */
int32 debug_sdio_read_bfgx_reg_and_mem(uint32 which_mem)
{
    struct ps_core_s *ps_core_d = NULL;
    struct st_exception_info *pst_exception_data = NULL;

    get_exception_info_reference(&pst_exception_data);

    if (pst_exception_data == NULL) {
        ps_print_err("get exception info reference is error!\n");
        return -EXCEPTION_FAIL;
    }

    if (pst_exception_data->exception_reset_enable != PLAT_EXCEPTION_ENABLE) {
        ps_print_err("plat dfr is not enable ,can not dump info");
        return -EXCEPTION_FAIL;
    }

    if (bfgx_is_shutdown()) {
        ps_print_warning("bfgx is off can not dump bfgx msg !\n");
        return -EXCEPTION_FAIL;
    }

    ps_get_core_reference(&ps_core_d);
    if (unlikely(ps_core_d == NULL)) {
        ps_print_err("ps_core_d is err\n");
        return -EXCEPTION_FAIL;
    }

    ps_print_info("sdio dump bfgx msg begin\n");

    // prepare the wlan state
    if (wlan_is_shutdown()) {
        ps_print_warning("wifi is closed, can not dump bcpu info!");
        return -EXCEPTION_FAIL;
    } else {
        ps_print_info("wifi is open!\n");
    }

    // 去能exception设置,halt bcpu不引发DFR
    pst_exception_data->exception_reset_enable = PLAT_EXCEPTION_DISABLE;

    // dump data
    switch (which_mem) {
        case BFGX_PUB_REG:
            sdio_get_and_save_data(g_sdio_read_bcpu_pub_reg_info, BFGX_PUB_REG_NUM);
            break;
        case BFGX_PRIV_REG:
            sdio_get_and_save_data(g_sdio_read_bcpu_priv_reg_info, BFGX_PRIV_REG_NUM);
            break;
        case BFGX_MEM:
            sdio_get_and_save_data(g_sdio_read_bcpu_mem_info, BFGX_SHARE_RAM_NUM);
            break;
        case SDIO_BFGX_MEM_DUMP_BOTTOM:
            sdio_get_and_save_data(g_sdio_read_all, sizeof(g_sdio_read_all) / sizeof(exception_bcpu_dump_msg));
            break;
        default:
            ps_print_warning("input param error , which_mem is %d\n", which_mem);
            pst_exception_data->exception_reset_enable = PLAT_EXCEPTION_ENABLE;
            return -EXCEPTION_FAIL;
    }

    /* send cmd to oam_hisi to rotate file */
    ps_print_info("dump complete, recovery begin\n");

    // 使能DFR, recovery
    pst_exception_data->exception_reset_enable = PLAT_EXCEPTION_ENABLE;
    plat_exception_handler(SUBSYS_BFGX, THREAD_IDLE, SDIO_DUMPBCPU_FAIL);

    return EXCEPTION_SUCCESS;
}

int32 uart_read_wifi_mem(uint32 which_mem)
{
    uint64 timeleft;
    struct ps_core_s *ps_core_d = NULL;
    struct st_exception_info *pst_exception_data = NULL;

    ps_print_info("%s\n", __func__);

    get_exception_info_reference(&pst_exception_data);
    if (pst_exception_data == NULL) {
        ps_print_err("get exception info reference is error\n");
        return -EXCEPTION_FAIL;
    }

    ps_get_core_reference(&ps_core_d);
    if (unlikely(ps_core_d == NULL)) {
        ps_print_err("ps_core_d is err\n");
        return -EINVAL;
    }

    if (which_mem >= UART_WIFI_MEM_DUMP_BOTTOM) {
        ps_print_err("param which_mem [%d] is err\n", which_mem);
        return -EINVAL;
    }

    g_recvd_wifi_block_index = which_mem;

    if (prepare_to_recv_wifi_mem() < 0) {
        ps_print_err("prepare mem to recv wifi mem fail, which_mem = [%d]\n", which_mem);
        g_recvd_wifi_block_index = UART_WIFI_MEM_DUMP_BOTTOM;
        return -EINVAL;
    }

    oal_reinit_completion(pst_exception_data->wait_uart_read_wifi_mem);
    ps_uart_state_pre(ps_core_d->tty);
    ps_tx_sys_cmd(ps_core_d, SYS_MSG, g_uart_read_wifi_mem_info[which_mem].cmd);
    timeleft = wait_for_completion_timeout(&pst_exception_data->wait_uart_read_wifi_mem,
                                           msecs_to_jiffies(UART_READ_WIFI_MEM_TIMEOUT));
    if (!timeleft) {
        ps_uart_state_dump(ps_core_d->tty);
        ps_print_err("wait uart read wifi mem [%d] timeout\n", which_mem);
        free_uart_read_wifi_mem();
        return -ETIMEDOUT;
    }

    return EXCEPTION_SUCCESS;
}

int32 debug_uart_read_wifi_mem(uint32 ul_lock)
{
    uint32 i;
    uint32 read_mem_succ_count = 0;
    struct ps_core_s *ps_core_d = NULL;
    int32 l_subchip_type = get_hi110x_subchip_type();
    struct pm_drv_data *pm_data = pm_get_drvdata();

    ps_print_info("%s\n", __func__);

    if (pm_data == NULL) {
        ps_print_err("pm_data is NULL!\n");
        return -EINVAL;
    }

    ps_get_core_reference(&ps_core_d);
    if (unlikely(ps_core_d == NULL)) {
        ps_print_err("ps_core_d is err\n");
        return -EINVAL;
    }

    if (l_subchip_type == BOARD_VERSION_HI1102) {
        if (ul_lock) {
            mutex_lock(&pm_data->host_mutex);
        }

        if (prepare_to_visit_node(ps_core_d) != EXCEPTION_SUCCESS) {
            ps_print_err("prepare work FAIL\n");
            goto fail_return;
        }

        if (uart_halt_wcpu() != EXCEPTION_SUCCESS) {
            ps_print_err("uart halt wcpu fail!\n");
            post_to_visit_node(ps_core_d);
            goto fail_return;
        }

        for (i = 0; i < UART_WIFI_MEM_DUMP_BOTTOM; i++) {
            if (uart_read_wifi_mem(i) != EXCEPTION_SUCCESS) {
                ps_print_err("uart read wifi mem [%d] fail!", i);
                break;
            }
            read_mem_succ_count++;
        }

        post_to_visit_node(ps_core_d);

        if (ul_lock) {
            mutex_unlock(&pm_data->host_mutex);
        }

        return EXCEPTION_SUCCESS;
    } else {
        ps_print_err("hi1103, ignore uart read wifi mem\n");
        return EXCEPTION_SUCCESS;
    }

fail_return:
    if (ul_lock) {
        mutex_unlock(&pm_data->host_mutex);
    }

    return -EXCEPTION_FAIL;
}

int32 bfgx_reset_cmd_send(uint32 subsys)
{
    int32 ret;
    struct ps_core_s *ps_core_d = NULL;
    struct st_bfgx_data *pst_bfgx_data = NULL;

    ps_get_core_reference(&ps_core_d);
    if (unlikely(ps_core_d == NULL)) {
        ps_print_err("ps_core_d is null\n");
        return -EINVAL;
    }

    pst_bfgx_data = &ps_core_d->bfgx_info[subsys];

    ret = ps_push_skb_queue(ps_core_d, g_bfgx_reset_msg[subsys].cmd,
                            g_bfgx_reset_msg[subsys].len,
                            g_bfgx_rx_queue[subsys]);
    if (ret != EXCEPTION_SUCCESS) {
        ps_print_err("push %s reset cmd to skb fail\n", g_bfgx_subsys_name[subsys]);
        return -EXCEPTION_FAIL;
    }

    wake_up_interruptible(&pst_bfgx_data->rx_wait);

    return EXCEPTION_SUCCESS;
}

/*
 * 函 数 名  : bfgx_status_recovery
 * 功能描述  : 全系统复位以后，恢复bfgin业务函数
 * 输入参数  : st_exception_info *exception结构体指针，保存了异常产生的系统wifi
 *             还是BFGN，对于BFGN还保存了产生异常的子业务，以及异常的类型
 * 返 回 值  : 异常处理成功返回0，否则返回1
 */
int32 bfgx_status_recovery(void)
{
    uint32 i;
    struct st_exception_info *pst_exception_data = NULL;
    struct ps_core_s *ps_core_d = NULL;
    struct st_bfgx_data *pst_bfgx_data = NULL;

    get_exception_info_reference(&pst_exception_data);
    if (pst_exception_data == NULL) {
        ps_print_err("get exception info reference is error\n");
        return -EXCEPTION_FAIL;
    }

    ps_get_core_reference(&ps_core_d);
    if (unlikely(ps_core_d == NULL)) {
        ps_print_err("ps_core_d is null\n");
        return -EXCEPTION_FAIL;
    }

    if (prepare_to_visit_node(ps_core_d) != EXCEPTION_SUCCESS) {
        ps_print_err("prepare work FAIL\n");
        return -EXCEPTION_FAIL;
    }

    for (i = 0; i < BFGX_BUTT; i++) {
        pst_bfgx_data = &ps_core_d->bfgx_info[i];
        if (atomic_read(&pst_bfgx_data->subsys_state) == POWER_STATE_SHUTDOWN) {
            continue;
        }

        ps_kfree_skb(ps_core_d, g_bfgx_rx_queue[i]);

        if (bfgx_open_cmd_send(i) != EXCEPTION_SUCCESS) {
            ps_print_err("bfgx open cmd fail\n");
            post_to_visit_node(ps_core_d);
            return -EXCEPTION_FAIL;
        }

        if (bfgx_reset_cmd_send(i) != EXCEPTION_SUCCESS) {
            ps_print_err("bfgx reset cmd send fail\n");
            post_to_visit_node(ps_core_d);
            return -EXCEPTION_FAIL;
        }
    }

    post_to_visit_node(ps_core_d);

    /* 仅调试使用 */
    ps_print_info("exception: set debug beat flag to 1\n");
    pst_exception_data->debug_beat_flag = 1;

    return EXCEPTION_SUCCESS;
}

/*
 * 函 数 名  : is_bfgx_exception
 * 功能描述  : 判断bfgx是否发生了ecxception
 * 返 回 值  : 0没有异常，1发生异常
 */
int32 is_bfgx_exception(void)
{
    struct st_exception_info *pst_exception_data = NULL;
    int32 is_exception;
    uint64 flag;

    get_exception_info_reference(&pst_exception_data);
    if (pst_exception_data == NULL) {
        ps_print_err("get exception info reference is error\n");
        return 0;
    }

    spin_lock_irqsave(&pst_exception_data->exception_spin_lock, flag);
    if (atomic_read(&pst_exception_data->is_reseting_device) == PLAT_EXCEPTION_RESET_BUSY) {
        is_exception = PLAT_EXCEPTION_RESET_BUSY;
    } else {
        is_exception = PLAT_EXCEPTION_RESET_IDLE;
    }
    spin_unlock_irqrestore(&pst_exception_data->exception_spin_lock, flag);

    return is_exception;
}

int32 plat_bfgx_exception_rst_register(struct ps_plat_s *data)
{
    struct st_exception_info *pst_exception_data = NULL;

    get_exception_info_reference(&pst_exception_data);
    if (pst_exception_data == NULL) {
        ps_print_err("get exception info reference is error\n");
        return -EXCEPTION_FAIL;
    }

    if (data == NULL) {
        ps_print_err("para data is NULL!\n");
        return -EXCEPTION_FAIL;
    }

    pst_exception_data->ps_plat_d = data;

    return EXCEPTION_SUCCESS;
}

EXPORT_SYMBOL_GPL(plat_bfgx_exception_rst_register);

int32 plat_wifi_exception_rst_register(struct st_wifi_dfr_callback *data)
{
    struct st_exception_info *pst_exception_data = NULL;

    get_exception_info_reference(&pst_exception_data);
    if (pst_exception_data == NULL) {
        ps_print_err("get exception info reference is error\n");
        return -EXCEPTION_FAIL;
    }

    if (data == NULL) {
        ps_print_err("param data is null\n");
        return -EXCEPTION_FAIL;
    }

    /* wifi异常回调函数注册 */
    pst_exception_data->wifi_callback = data;

    return EXCEPTION_SUCCESS;
}

EXPORT_SYMBOL_GPL(plat_wifi_exception_rst_register);

/*
 * 函 数 名  : plat_exception_reset_init
 * 功能描述  : 平台异常处理模块初始化函数
 * 返 回 值  : 成功返回0，否则返回-1
 */
int32 plat_exception_reset_init(void)
{
    struct st_exception_info *p_exception_data = NULL;

    p_exception_data = (struct st_exception_info *)kzalloc(sizeof(struct st_exception_info), GFP_KERNEL);
    if (p_exception_data == NULL) {
        ps_print_err("kzalloc p_exception_data is failed!\n");
        return -EXCEPTION_FAIL;
    }

    p_exception_data->wifi_callback = NULL;

    p_exception_data->subsys_type = SUBSYS_BOTTOM;
    p_exception_data->thread_type = BFGX_THREAD_BOTTOM;
    p_exception_data->excetion_type = EXCEPTION_TYPE_BOTTOM;

    p_exception_data->exception_reset_enable = PLAT_EXCEPTION_DISABLE;
    p_exception_data->subsystem_rst_en = DFR_TEST_ENABLE;

    p_exception_data->ps_plat_d = NULL;

    atomic_set(&p_exception_data->bfgx_beat_flag, BFGX_NOT_RECV_BEAT_INFO);
    atomic_set(&p_exception_data->is_reseting_device, PLAT_EXCEPTION_RESET_IDLE);

    /* 初始化异常处理workqueue和work */
    p_exception_data->plat_exception_rst_workqueue = create_singlethread_workqueue("plat_exception_reset_queue");
    INIT_WORK(&p_exception_data->plat_exception_rst_work, plat_exception_reset_work);
    oal_wake_lock_init(&p_exception_data->plat_exception_rst_wlock, "hi11xx_excep_rst_wlock");
    INIT_WORK(&p_exception_data->uart_store_wifi_mem_to_file_work, store_wifi_mem_to_file_work);

    /* 初始化心跳timer */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 18, 0))
    init_timer(&p_exception_data->bfgx_beat_timer);
    p_exception_data->bfgx_beat_timer.function = bfgx_beat_timer_expire;
    p_exception_data->bfgx_beat_timer.data = 0;
#else
    timer_setup(&p_exception_data->bfgx_beat_timer, (void *)bfgx_beat_timer_expire, 0);
#endif
    p_exception_data->bfgx_beat_timer.expires = jiffies + BFGX_BEAT_TIME * HZ;

    /* 初始化异常处理自旋锁 */
    spin_lock_init(&p_exception_data->exception_spin_lock);

    /* 初始化bfgx读栈完成量 */
    init_completion(&p_exception_data->wait_read_bfgx_stack);
    /* 初始化sdio读取bcpu完成量 */
    init_completion(&p_exception_data->wait_sdio_d2h_dump_ack);

    /* 调试使用的变量初始化 */
    p_exception_data->debug_beat_flag = 1;
    atomic_set(&p_exception_data->is_memdump_runing, 0);
    p_exception_data->wifi_open_bcpu_enable = false;

    init_completion(&p_exception_data->wait_uart_read_wifi_mem);
    init_completion(&p_exception_data->wait_uart_halt_wcpu);

    g_exception_info = p_exception_data;

    /* 初始化dump文件轮替模块 */
    plat_exception_dump_file_rotate_init();

    hisi_conn_rdr_init();

    ps_print_suc("plat exception reset init success\n");

    return EXCEPTION_SUCCESS;
}

EXPORT_SYMBOL_GPL(plat_exception_reset_init);

/*
 * 函 数 名  : plat_exception_reset_exit
 * 功能描述  : 平台异常处理模块退出清理函数
 * 返 回 值  : 成功返回0，否则返回-1
 */
int32 plat_exception_reset_exit(void)
{
    struct st_exception_info *p_exception_data = NULL;

    p_exception_data = g_exception_info;
    if (p_exception_data == NULL) {
        ps_print_err("g_exception_info is NULL!\n");
        return -EXCEPTION_FAIL;
    }

    destroy_workqueue(p_exception_data->plat_exception_rst_workqueue);
    del_timer_sync(&p_exception_data->bfgx_beat_timer);

    oal_wake_lock_exit(&p_exception_data->plat_exception_rst_wlock);
    kfree(p_exception_data);
    g_exception_info = NULL;

    ps_print_suc("plat exception reset exit success\n");

    hisi_conn_rdr_exit();

    return EXCEPTION_SUCCESS;
}

EXPORT_SYMBOL_GPL(plat_exception_reset_exit);

#ifdef PLATFORM_DEBUG_ENABLE
OAL_STATIC uint8 g_gst_excp_test_cfg[EXCP_TEST_CFG_BOTT] = { DFR_TEST_DISABLE, DFR_TEST_DISABLE, DFR_TEST_DISABLE };

int32 is_dfr_test_en(enum excp_test_cfg_em excp_cfg)
{
    if (excp_cfg >= EXCP_TEST_CFG_BOTT) {
        return -1;
    }

    if (g_gst_excp_test_cfg[excp_cfg] == DFR_TEST_ENABLE) {
        g_gst_excp_test_cfg[excp_cfg] = DFR_TEST_DISABLE;
        return 0;
    }
    return -1;
}

EXPORT_SYMBOL_GPL(is_dfr_test_en);

void set_excp_test_en(enum excp_test_cfg_em excp_cfg)
{
    if (excp_cfg >= EXCP_TEST_CFG_BOTT) {
        return;
    }

    g_gst_excp_test_cfg[excp_cfg] = DFR_TEST_ENABLE;
}
#endif

#ifdef HI110X_HAL_MEMDUMP_ENABLE
/*
 * Prototype    : plat_exception_dump_file_rotate_init
 * Description  : driver init rotate resource
 */
void plat_exception_dump_file_rotate_init(void)
{
    init_waitqueue_head(&g_bcpu_memdump_cfg.dump_type_wait);
    skb_queue_head_init(&g_bcpu_memdump_cfg.dump_type_queue);
    skb_queue_head_init(&g_bcpu_memdump_cfg.quenue);
    init_waitqueue_head(&g_wcpu_memdump_cfg.dump_type_wait);
    skb_queue_head_init(&g_wcpu_memdump_cfg.dump_type_queue);
    skb_queue_head_init(&g_wcpu_memdump_cfg.quenue);
    ps_print_info("plat exception dump file rotate init success\n");
}

void excp_memdump_quenue_clear(memdump_info_t *memdump_t)
{
    struct sk_buff *skb = NULL;
    while ((skb = skb_dequeue(&memdump_t->quenue)) != NULL) {
        kfree_skb(skb);
    }
}

int32 bfgx_memdump_quenue_clear(void)
{
    ps_print_dbg("bfgx_memdump_quenue_clear\n");
    excp_memdump_quenue_clear(&g_bcpu_memdump_cfg);
    return 0;
}

void wifi_memdump_quenue_clear(void)
{
    ps_print_dbg("wifi_memdump_quenue_clear\n");
    excp_memdump_quenue_clear(&g_wcpu_memdump_cfg);
}

void bfgx_memdump_finish(void)
{
    g_bcpu_memdump_cfg.is_working = 0;
}

void wifi_memdump_finish(void)
{
    g_wcpu_memdump_cfg.is_working = 0;
}

/*
 * Prototype    : plat_send_rotate_cmd_2_app
 * Description  : driver send rotate cmd to app for rotate file
 * input        : uint32 which_dump, memdump_info_t *memdump_inf
 */
int32 plat_excp_send_rotate_cmd_2_app(uint32 which_dump, memdump_info_t *memdump_info)
{
    struct sk_buff *skb = NULL;

    if (which_dump >= CMD_DUMP_BUFF) {
        ps_print_warning("which dump:%d error\n", which_dump);
        return -EINVAL;
    }
    if (skb_queue_len(&memdump_info->dump_type_queue) > MEMDUMP_ROTATE_QUEUE_MAX_LEN) {
        ps_print_warning("too many dump type in queue,dispose type:%d", which_dump);
        return -EINVAL;
    }

    if (oal_in_interrupt() || oal_in_atomic() || irqs_disabled()) {
        skb = alloc_skb(sizeof(which_dump), GFP_ATOMIC);
    } else {
        skb = alloc_skb(sizeof(which_dump), GFP_KERNEL);
    }

    if (skb == NULL) {
        ps_print_err("alloc errno skbuff failed! len=%d, errno=%x\n", (int32)sizeof(which_dump), which_dump);
        return -EINVAL;
    }
    skb_put(skb, sizeof(which_dump));
    *(uint32 *)skb->data = which_dump;
    skb_queue_tail(&memdump_info->dump_type_queue, skb);
    ps_print_info("save rotate cmd [%d] in queue\n", which_dump);

    wake_up_interruptible(&memdump_info->dump_type_wait);

    return 0;
}

int32 notice_hal_memdump(memdump_info_t *memdump_t, uint32 which_dump)
{
    PS_PRINT_FUNCTION_NAME;
    if (memdump_t->is_working) {
        ps_print_err("is doing memdump\n");
        return -1;
    }
    excp_memdump_quenue_clear(memdump_t);
    plat_excp_send_rotate_cmd_2_app(which_dump, memdump_t);
    memdump_t->is_working = 1;
    return 0;
}

int32 bfgx_notice_hal_memdump(void)
{
    return notice_hal_memdump(&g_bcpu_memdump_cfg, CMD_READM_BFGX_UART);
}

int32 wifi_notice_hal_memdump(void)
{
    return notice_hal_memdump(&g_wcpu_memdump_cfg, CMD_READM_WIFI_SDIO);
}

int32 excp_memdump_queue(uint8 *buf_ptr, uint16 count, memdump_info_t *memdump_t)
{
    struct sk_buff *skb = NULL;

    ps_print_dbg("[send] len:%d\n", count);
    if (!memdump_t->is_working) {
        ps_print_err("excp_memdump_queue not allow\n");
        return -EINVAL;
    }
    if (buf_ptr == NULL) {
        ps_print_err("buf_ptr is NULL\n");
        return -EINVAL;
    }
    if (in_atomic() || in_softirq() || in_interrupt() || irqs_disabled()) {
        skb = alloc_skb(count, GFP_ATOMIC);
    } else {
        skb = alloc_skb(count, GFP_KERNEL);
    }

    if (skb == NULL) {
        ps_print_err("can't allocate mem for new debug skb, len=%d\n", count);
        return -EINVAL;
    }

    if (memcpy_s(skb_tail_pointer(skb), count, buf_ptr, count) != EOK) {
        ps_print_err("memcpy_s error\n");
        return -EINVAL;
    }
    skb_put(skb, count);
    skb_queue_tail(&memdump_t->quenue, skb);
    ps_print_warning("[excp_memdump_queue]qlen:%d,count:%d\n", memdump_t->quenue.qlen, count);
    return 0;
}

int32 bfgx_memdump_enquenue(uint8 *buf_ptr, uint16 count)
{
    return excp_memdump_queue(buf_ptr, count, &g_bcpu_memdump_cfg);
}

int32 wifi_memdump_enquenue(uint8 *buf_ptr, uint16 count)
{
    return excp_memdump_queue(buf_ptr, count, &g_wcpu_memdump_cfg);
}
#endif
