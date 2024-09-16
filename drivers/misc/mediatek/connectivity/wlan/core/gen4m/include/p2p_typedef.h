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
/*
 ** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/
 *   include/p2p_typedef.h#1
 */

/*! \file   p2p_typedef.h
 *    \brief  Declaration of data type and return values of
 *    internal protocol stack.
 *
 *    In this file we declare the data type and return values
 *    which will be exported to all MGMT Protocol Stack.
 */

#ifndef _P2P_TYPEDEF_H
#define _P2P_TYPEDEF_H

#if CFG_ENABLE_WIFI_DIRECT

/******************************************************************************
 *                         C O M P I L E R   F L A G S
 ******************************************************************************
 */

/******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 ******************************************************************************
 */

/******************************************************************************
 *                              C O N S T A N T S
 ******************************************************************************
 */

/******************************************************************************
 *                                 M A C R O S
 ******************************************************************************
 */

/******************************************************************************
 *                             D A T A   T Y P E S
 ******************************************************************************
 */

/*
 * type definition of pointer to p2p structure
 */
/* typedef struct GL_P2P_INFO   GL_P2P_INFO_T, *P_GL_P2P_INFO_T; */
struct P2P_INFO;	/* declare P2P_INFO_T */

struct P2P_FSM_INFO;	/* declare P2P_FSM_INFO_T */

struct P2P_DEV_FSM_INFO;	/* declare P2P_DEV_FSM_INFO_T */

struct P2P_ROLE_FSM_INFO;	/* declare P2P_ROLE_FSM_INFO_T */

struct P2P_CONNECTION_SETTINGS;	/* declare P2P_CONNECTION_SETTINGS_T */

/* Type definition for function pointer to p2p function*/
typedef u_int8_t(*P2P_LAUNCH) (struct GLUE_INFO *prGlueInfo);

typedef u_int8_t(*P2P_REMOVE) (struct GLUE_INFO *prGlueInfo,
					  u_int8_t fgIsWlanLaunched);

typedef u_int8_t(*KAL_P2P_GET_CIPHER) (IN struct GLUE_INFO *prGlueInfo);

typedef u_int8_t(*KAL_P2P_GET_TKIP_CIPHER) (IN struct GLUE_INFO *prGlueInfo);

typedef u_int8_t(*KAL_P2P_GET_CCMP_CIPHER) (IN struct GLUE_INFO *prGlueInfo);

typedef u_int8_t(*KAL_P2P_GET_WSC_MODE) (IN struct GLUE_INFO *prGlueInfo);

typedef struct net_device *(*KAL_P2P_GET_DEV_HDLR) (
		struct GLUE_INFO *prGlueInfo);

typedef void(*KAL_P2P_SET_MULTICAST_WORK_ITEM) (struct GLUE_INFO *prGlueInfo);

typedef void(*P2P_NET_REGISTER) (struct GLUE_INFO *prGlueInfo);

typedef void(*P2P_NET_UNREGISTER) (struct GLUE_INFO *prGlueInfo);

typedef void(*KAL_P2P_UPDATE_ASSOC_INFO) (IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t *pucFrameBody,
		IN uint32_t u4FrameBodyLen,
		IN u_int8_t fgReassocRequest);

typedef u_int8_t(*P2P_VALIDATE_AUTH) (IN struct ADAPTER *prAdapter,
		IN struct SW_RFB *prSwRfb,
		IN struct STA_RECORD **pprStaRec,
		OUT uint16_t *pu2StatusCode);

typedef u_int8_t(*P2P_VALIDATE_ASSOC_REQ) (IN struct ADAPTER *prAdapter,
		IN struct SW_RFB *prSwRfb,
		OUT uint16_t *pu4ControlFlags);

typedef void(*P2P_RUN_EVENT_AAA_TX_FAIL) (IN struct ADAPTER *prAdapter,
		IN struct STA_RECORD *prStaRec);

typedef u_int8_t(*P2P_PARSE_CHECK_FOR_P2P_INFO_ELEM) (
		IN struct ADAPTER *prAdapter,
		IN uint8_t *pucBuf,
		OUT uint8_t *pucOuiType);

typedef uint32_t(*P2P_RUN_EVENT_AAA_COMPLETE) (IN struct ADAPTER *prAdapter,
		IN struct STA_RECORD *prStaRec);

typedef void(*P2P_PROCESS_EVENT_UPDATE_NOA_PARAM) (
		IN struct ADAPTER *prAdapter,
		uint8_t ucNetTypeIndex,
		struct EVENT_UPDATE_NOA_PARAMS *prEventUpdateNoaParam);

typedef void(*SCAN_P2P_PROCESS_BEACON_AND_PROBE_RESP) (
		IN struct ADAPTER *prAdapter,
		IN struct SW_RFB *prSwRfb,
		IN uint32_t *prStatus,
		IN struct BSS_DESC *prBssDesc,
		IN struct WLAN_BEACON_FRAME *prWlanBeaconFrame);

typedef void(*P2P_RX_PUBLIC_ACTION_FRAME) (struct ADAPTER *prAdapter,
		IN struct SW_RFB *prSwRfb);

typedef void(*RLM_RSP_GENERATE_OBSS_SCAN_IE) (struct ADAPTER *prAdapter,
		struct MSDU_INFO *prMsduInfo);

typedef void(*RLM_UPDATE_BW_BY_CH_LIST_FOR_AP) (struct ADAPTER *prAdapter,
		struct BSS_INFO *prBssInfo);

typedef void(*RLM_PROCESS_PUBLIC_ACTION) (struct ADAPTER *prAdapter,
		struct SW_RFB *prSwRfb);

typedef void(*RLM_PROCESS_HT_ACTION) (struct ADAPTER *prAdapter,
		struct SW_RFB *prSwRfb);

typedef void(*RLM_UPDATE_PARAMS_FOR_AP) (struct ADAPTER *prAdapter,
		struct BSS_INFO *prBssInfo, u_int8_t fgUpdateBeacon);

typedef void(*RLM_HANDLE_OBSS_STATUS_EVENT_PKT) (struct ADAPTER *prAdapter,
		struct EVENT_AP_OBSS_STATUS *prObssStatus);

typedef u_int8_t(*P2P_FUNC_VALIDATE_PROBE_REQ) (IN struct ADAPTER *prAdapter,
		IN struct SW_RFB *prSwRfb,
		OUT uint32_t *pu4ControlFlags);

typedef void(*RLM_BSS_INIT_FOR_AP) (struct ADAPTER *prAdapter,
		struct BSS_INFO *prBssInfo);

typedef uint32_t(*P2P_GET_PROB_RSP_IE_TABLE_SIZE) (void);

typedef uint8_t *(*P2P_BUILD_REASSOC_REQ_FRAME_COMMON_IES) (
		IN struct ADAPTER *prAdapter,
		IN struct MSDU_INFO *prMsduInfo, IN uint8_t *pucBuffer);

typedef void(*P2P_FUNC_DISCONNECT) (IN struct ADAPTER *prAdapter,
		IN struct STA_RECORD *prStaRec,
		IN u_int8_t fgSendDeauth, IN uint16_t u2ReasonCode);

typedef void(*P2P_FSM_RUN_EVENT_RX_DEAUTH) (IN struct ADAPTER *prAdapter,
		IN struct STA_RECORD *prStaRec,
		IN struct SW_RFB *prSwRfb);

typedef void(*P2P_FSM_RUN_EVENT_RX_DISASSOC) (IN struct ADAPTER *prAdapter,
		IN struct STA_RECORD *prStaRec,
		IN struct SW_RFB *prSwRfb);

typedef u_int8_t(*P2P_FUN_IS_AP_MODE) (IN struct P2P_FSM_INFO *prP2pFsmInfo);

typedef void(*P2P_FSM_RUN_EVENT_BEACON_TIMEOUT) (IN struct ADAPTER *prAdapter);

typedef void(*P2P_FUNC_STORE_ASSOC_RSP_IE_BUFFER) (
		IN struct ADAPTER *prAdapter,
		IN struct SW_RFB *prSwRfb);

typedef void(*P2P_GENERATE_P2P_IE) (IN struct ADAPTER *prAdapter,
		IN struct MSDU_INFO *prMsduInfo);

typedef uint32_t(*P2P_CALCULATE_P2P_IE_LEN) (IN struct ADAPTER *prAdapter,
		IN uint8_t ucBssIndex, IN struct STA_RECORD *prStaRec);

/******************************************************************************
 *                            P U B L I C   D A T A
 ******************************************************************************
 */

/******************************************************************************
 *                           P R I V A T E   D A T A
 ******************************************************************************
 */

/******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
 ******************************************************************************
 */

#endif /*CFG_ENABLE_WIFI_DIRECT */

#endif /* _P2P_TYPEDEF_H */
