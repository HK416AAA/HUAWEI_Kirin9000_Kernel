/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2012-2015. All rights reserved.
 * foss@huawei.com
 *
 * If distributed as part of the Linux kernel, the following license terms
 * apply:
 *
 * * This program is free software; you can redistribute it and/or modify
 * * it under the terms of the GNU General Public License version 2 and
 * * only version 2 as published by the Free Software Foundation.
 * *
 * * This program is distributed in the hope that it will be useful,
 * * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * * GNU General Public License for more details.
 * *
 * * You should have received a copy of the GNU General Public License
 * * along with this program; if not, write to the Free Software
 * * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA
 *
 * Otherwise, the following license terms apply:
 *
 * * Redistribution and use in source and binary forms, with or without
 * * modification, are permitted provided that the following conditions
 * * are met:
 * * 1) Redistributions of source code must retain the above copyright
 * *    notice, this list of conditions and the following disclaimer.
 * * 2) Redistributions in binary form must reproduce the above copyright
 * *    notice, this list of conditions and the following disclaimer in the
 * *    documentation and/or other materials provided with the distribution.
 * * 3) Neither the name of Huawei nor the names of its contributors may
 * *    be used to endorse or promote products derived from this software
 * *    without specific prior written permission.
 *
 * * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * 1 头文件包
 */
#include "scm_ind_src.h"
#include <product_config.h>
#include <osl_sem.h>
#include <soc_socp_adapter.h>
#include <bsp_socp.h>
#include <securec.h>
#include "ppm_common.h"
#include "scm_common.h"
#include "diag_system_debug.h"


/*
 * 2 全局变量定义
 */
scm_coder_src_cfg_s g_scm_ind_coder_src_cfg = { SCM_CHANNEL_UNINIT,
    SOCP_CODER_SRC_PS_IND,
    SOCP_CODER_DST_OM_IND,
    SOCP_DATA_TYPE_0,
    SOCP_ENCSRC_CHNMODE_CTSPACKET,
#ifdef DIAG_SYSTEM_5G
    SOCP_CHAN_PRIORITY_1,
    SOCP_TRANS_ID_EN,
    SOCP_PTR_IMG_DIS,
#else
    SOCP_CHAN_PRIORITY_2,
    SOCP_TRANS_ID_DIS,
    SOCP_PTR_IMG_DIS,
#endif
    SCM_CODER_SRC_IND_BUFFER_SIZE,
    SCM_CODER_SRC_RDSIZE,
    NULL,
    NULL,
    NULL,
    NULL,
    0,
    0 };

u32 scm_init_ind_src_buff(void)
{
    u32 ret;

    ret = scm_create_ind_src_buff(&g_scm_ind_coder_src_cfg.src_virt_buff, &g_scm_ind_coder_src_cfg.src_phy_buff,
        g_scm_ind_coder_src_cfg.src_buff_len);
    if (ret != BSP_OK) {
        g_scm_ind_coder_src_cfg.init_state = SCM_CHANNEL_MEM_FAIL;
        return (u32)BSP_ERROR;
    }
    g_scm_ind_coder_src_cfg.init_state = SCM_CHANNEL_INIT_SUCC;

    return BSP_OK;
}

/*
 * 功能描述: 申请编码源buffer空间
 * 修改历史:
 */
u32 scm_create_ind_src_buff(u8 **virt_addr, u8 **phy_addr, u32 len)
{
    unsigned long addr;

    /* 申请uncache的动态内存区 */
    *virt_addr = (u8 *)scm_uncache_mem_alloc(len, &addr);

    /* 分配内存失败 */
    if (*virt_addr == NULL) {
        return (u32)BSP_ERROR;
    }

    *phy_addr = (u8 *)(uintptr_t)addr;

    return BSP_OK;
}

u32 scm_ind_src_chan_init(void)
{
    if (scm_ind_src_chan_cfg(&g_scm_ind_coder_src_cfg) != BSP_OK) {
        diag_error("cfg ind src fail\n");
        g_scm_ind_coder_src_cfg.init_state = SCM_CHANNEL_CFG_FAIL;

        return (u32)BSP_ERROR; /* 返回失败 */
    }
#ifndef DIAG_SYSTEM_5G
    if (bsp_socp_start(g_scm_ind_coder_src_cfg.chan_id) != BSP_OK) {
        diag_error("start ind src fail\n");
        g_scm_ind_coder_src_cfg.init_state = SCM_CHANNEL_START_FAIL;

        return ERR_MSP_SCM_START_SOCP_FAIL; /* 返回失败 */
    }
#endif
    g_scm_ind_coder_src_cfg.init_state = SCM_CHANNEL_INIT_SUCC;

    return BSP_OK; /* 返回成功 */
}

unsigned long *scm_ind_src_phy_to_virt(u8 *phy_addr)
{
    if ((phy_addr < g_scm_ind_coder_src_cfg.src_phy_buff) ||
        (phy_addr >= (g_scm_ind_coder_src_cfg.src_virt_buff + g_scm_ind_coder_src_cfg.src_buff_len))) {
        return NULL;
    }

    return (unsigned long *)((phy_addr - g_scm_ind_coder_src_cfg.src_phy_buff) + g_scm_ind_coder_src_cfg.src_virt_buff);
}

u32 scm_ind_src_chan_cfg(scm_coder_src_cfg_s *chan_attr)
{
    socp_src_chan_cfg_s chan_info;

    (void)memset_s(&chan_info, sizeof(chan_info), 0, sizeof(chan_info));
    chan_info.dest_chan_id = chan_attr->dst_chan_id; /*  目标通道ID */
    chan_info.data_type = chan_attr->data_type;
    chan_info.mode = chan_attr->chan_mode; /*  通道数据模式 */
    chan_info.priority = chan_attr->chan_prio;
    chan_info.trans_id_en = chan_attr->Trans_id_en;
    chan_info.rptr_img_en = chan_attr->rptr_img_en;
    chan_info.bypass_en = SOCP_HDLC_ENABLE; /*  通道bypass使能 */
    chan_info.data_type_en = SOCP_DATA_TYPE_EN;
    chan_info.debug_en = SOCP_ENC_DEBUG_DIS;
    chan_info.chan_group = 1; /* socp rate group, this is DRV  */

    chan_info.coder_src_setbuf.input_start = chan_attr->src_phy_buff; /*  输入通道起始地址 */
    chan_info.coder_src_setbuf.input_end =
        (chan_attr->src_phy_buff + chan_attr->src_buff_len) - 1;        /*  输入通道结束地址 */
    chan_info.coder_src_setbuf.rd_input_start = chan_attr->rd_phy_buff; /* RD buffer起始地址 */
    /*  RD buffer结束地址 */
    chan_info.coder_src_setbuf.rd_input_end = (chan_attr->rd_phy_buff + chan_attr->rd_buff_len) - 1;
    chan_info.coder_src_setbuf.rd_threshold = SCM_CODER_SRC_RD_THRESHOLD;

    if (chan_attr->rptr_img_en) {
        chan_attr->rptr_img_virt_addr = (uintptr_t)scm_uncache_mem_alloc(sizeof(u64), &(chan_attr->rptr_img_phy_addr));
        if (chan_attr->rptr_img_virt_addr == 0) {
            return ERR_MSP_NOT_FREEE_SPACE;
        }

        chan_info.read_ptr_img_phy_addr = chan_attr->rptr_img_phy_addr;
        chan_info.read_ptr_img_vir_addr = chan_attr->rptr_img_virt_addr;
    }

    if (bsp_socp_coder_set_src_chan(chan_attr->chan_id, &chan_info) != BSP_OK) {
        diag_error("Channel ID(0x%x) Error\n", chan_attr->chan_id);

        return (u32)BSP_ERROR; /* 返回错误 */
    }

    chan_attr->init_state = SCM_CHANNEL_INIT_SUCC;

    return BSP_OK; /* 返回成功 */
}

