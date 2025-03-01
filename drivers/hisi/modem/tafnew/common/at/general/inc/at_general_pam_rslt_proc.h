/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2020. All rights reserved.
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

#ifndef _ATGENERALPAMRSLTPROC_H_
#define _ATGENERALPAMRSLTPROC_H_

#include "vos.h"
#include "si_app_pih.h"
#include "at_mn_interface.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#pragma pack(push, 4)

extern VOS_UINT32 At_ProcPihCimiQryCnf(VOS_UINT8 indexNum, SI_PIH_EventInfo *event, VOS_UINT16 *length);
extern VOS_UINT32 At_ProcPihCglaSetCnf(VOS_UINT8 indexNum, SI_PIH_EventInfo *event, VOS_UINT16 *length);

extern TAF_VOID At_QryParaRspIccidProc(TAF_UINT8 indexNum, TAF_UINT8 opId, TAF_UINT8 *para);
extern TAF_VOID At_QryParaRspCimiProc(TAF_UINT8 indexNum, TAF_UINT8 opId, TAF_UINT8 *para);

extern TAF_VOID At_QryParaRspCgclassProc(TAF_UINT8 indexNum, TAF_UINT8 opId, TAF_UINT8 *para);
extern TAF_VOID At_QryParaRspPnnProc(TAF_UINT8 indexNum, TAF_UINT8 opId, TAF_UINT8 *para);
extern TAF_VOID At_QryParaRspCPnnProc(TAF_UINT8 indexNum, TAF_UINT8 opId, TAF_UINT8 *para);

extern TAF_VOID At_QryParaRspOplProc(TAF_UINT8 indexNum, TAF_UINT8 opId, TAF_UINT8 *para);
extern TAF_VOID At_QryRspUsimRangeProc(TAF_UINT8 indexNum, TAF_UINT8 opId, TAF_UINT8 *para);
extern TAF_VOID   At_QryMsgProc(TAF_QryRslt *qryRslt);
extern VOS_UINT32 AT_RcvDrvAgentSetAdcRsp(struct MsgCB *msg);
VOS_UINT32 AT_RcvDrvAgentQryAdcRsp(struct MsgCB *msg);

/*
 * 功能描述: 收到UE信息上报的处理
 */
VOS_UINT32 AT_RcvMtaCgsnQryCnf(VOS_VOID *msg, VOS_UINT8 indexNum);
#pragma pack(pop)

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif
