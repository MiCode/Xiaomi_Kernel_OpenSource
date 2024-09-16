/******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * BSD LICENSE
 *
 * Copyright(C) 2016 MediaTek Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
#ifndef _P2P_ROLE_STATE_H
#define _P2P_ROLE_STATE_H

void
p2pRoleStateInit_IDLE(IN struct ADAPTER *prAdapter,
		IN struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo,
		IN struct BSS_INFO *prP2pBssInfo);

void
p2pRoleStateAbort_IDLE(IN struct ADAPTER *prAdapter,
		IN struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo,
		IN struct P2P_CHNL_REQ_INFO *prP2pChnlReqInfo);

void p2pRoleStateInit_SCAN(IN struct ADAPTER *prAdapter,
		IN uint8_t ucBssIndex,
		IN struct P2P_SCAN_REQ_INFO *prScanReqInfo);

void p2pRoleStateAbort_SCAN(IN struct ADAPTER *prAdapter,
		IN struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo);

void
p2pRoleStateInit_REQING_CHANNEL(IN struct ADAPTER *prAdapter,
		IN uint8_t ucBssIdx,
		IN struct P2P_CHNL_REQ_INFO *prChnlReqInfo);

void
p2pRoleStateAbort_REQING_CHANNEL(IN struct ADAPTER *prAdapter,
		IN struct BSS_INFO *prP2pRoleBssInfo,
		IN struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo,
		IN enum ENUM_P2P_ROLE_STATE eNextState);

void
p2pRoleStateInit_AP_CHNL_DETECTION(IN struct ADAPTER *prAdapter,
		IN uint8_t ucBssIndex,
		IN struct P2P_SCAN_REQ_INFO *prScanReqInfo,
		IN struct P2P_CONNECTION_REQ_INFO *prConnReqInfo);

void
p2pRoleStateAbort_AP_CHNL_DETECTION(IN struct ADAPTER *prAdapter,
		IN uint8_t ucBssIndex,
		IN struct P2P_CONNECTION_REQ_INFO *prP2pConnReqInfo,
		IN struct P2P_CHNL_REQ_INFO *prChnlReqInfo,
		IN struct P2P_SCAN_REQ_INFO *prP2pScanReqInfo,
		IN enum ENUM_P2P_ROLE_STATE eNextState);

void
p2pRoleStateInit_GC_JOIN(IN struct ADAPTER *prAdapter,
		IN struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo,
		IN struct P2P_CHNL_REQ_INFO *prChnlReqInfo);

void
p2pRoleStateAbort_GC_JOIN(IN struct ADAPTER *prAdapter,
		IN struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo,
		IN struct P2P_JOIN_INFO *prJoinInfo,
		IN enum ENUM_P2P_ROLE_STATE eNextState);

#if (CFG_SUPPORT_DFS_MASTER == 1)
void
p2pRoleStateInit_DFS_CAC(IN struct ADAPTER *prAdapter,
		IN uint8_t ucBssIdx,
		IN struct P2P_CHNL_REQ_INFO *prChnlReqInfo);

void
p2pRoleStateAbort_DFS_CAC(IN struct ADAPTER *prAdapter,
		IN struct BSS_INFO *prP2pRoleBssInfo,
		IN struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo,
		IN enum ENUM_P2P_ROLE_STATE eNextState);

void
p2pRoleStateInit_SWITCH_CHANNEL(IN struct ADAPTER *prAdapter,
		IN uint8_t ucBssIdx,
		IN struct P2P_CHNL_REQ_INFO *prChnlReqInfo);

void
p2pRoleStateAbort_SWITCH_CHANNEL(IN struct ADAPTER *prAdapter,
		IN struct BSS_INFO *prP2pRoleBssInfo,
		IN struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo,
		IN enum ENUM_P2P_ROLE_STATE eNextState);

void
p2pRoleStatePrepare_To_DFS_CAC_STATE(IN struct ADAPTER *prAdapter,
		IN struct BSS_INFO *prBssInfo,
		IN enum ENUM_CHANNEL_WIDTH rChannelWidth,
		IN struct P2P_CONNECTION_REQ_INFO *prConnReqInfo,
		OUT struct P2P_CHNL_REQ_INFO *prChnlReqInfo);

#endif

void
p2pRoleStatePrepare_To_REQING_CHANNEL_STATE(IN struct ADAPTER *prAdapter,
		IN struct BSS_INFO *prBssInfo,
		IN struct P2P_CONNECTION_REQ_INFO *prConnReqInfo,
		OUT struct P2P_CHNL_REQ_INFO *prChnlReqInfo);

u_int8_t
p2pRoleStateInit_OFF_CHNL_TX(IN struct ADAPTER *prAdapter,
		IN struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo,
		IN struct P2P_CHNL_REQ_INFO *prChnlReqInfo,
		IN struct P2P_MGMT_TX_REQ_INFO *prP2pMgmtTxInfo,
		OUT enum ENUM_P2P_ROLE_STATE *peNextState);

void
p2pRoleStateAbort_OFF_CHNL_TX(IN struct ADAPTER *prAdapter,
		IN struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo,
		IN struct P2P_MGMT_TX_REQ_INFO *prP2pMgmtTxInfo,
		IN struct P2P_CHNL_REQ_INFO *prChnlReqInfo,
		IN enum ENUM_P2P_ROLE_STATE eNextState);

#endif
