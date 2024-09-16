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
** Id: //Department/DaVinci/TRUNK/WiFi_P2P_Driver/os/linux/include/gl_p2p_kal.h#2
*/

/*! \file   gl_p2p_kal.h
*    \brief  Declaration of KAL functions for Wi-Fi Direct support
*	    - kal*() which is provided by GLUE Layer.
*
*    Any definitions in this file will be shared among GLUE Layer and internal Driver Stack.
*/


#ifndef _GL_P2P_KAL_H
#define _GL_P2P_KAL_H

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "config.h"
#include "gl_typedef.h"
#include "gl_os.h"
#include "wlan_lib.h"
#include "wlan_oid.h"
#include "wlan_p2p.h"
#include "gl_kal.h"
#include "gl_wext_priv.h"
#include "gl_p2p_ioctl.h"
#include "nic/p2p.h"

#if DBG
extern int allocatedMemSize;
#endif

BOOLEAN kalP2pFuncGetChannelType(IN ENUM_CHNL_EXT_T rChnlSco, OUT enum nl80211_channel_type *channel_type);

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/* Service Discovery */
VOID kalP2PIndicateSDRequest(IN P_GLUE_INFO_T prGlueInfo, IN PARAM_MAC_ADDRESS rPeerAddr, IN UINT_8 ucSeqNum);

void kalP2PIndicateSDResponse(IN P_GLUE_INFO_T prGlueInfo, IN PARAM_MAC_ADDRESS rPeerAddr, IN UINT_8 ucSeqNum);

VOID kalP2PIndicateTXDone(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucSeqNum, IN UINT_8 ucStatus);

/*----------------------------------------------------------------------------*/
/* Wi-Fi Direct handling                                                      */
/*----------------------------------------------------------------------------*/
/*ENUM_PARAM_MEDIA_STATE_T kalP2PGetState(IN P_GLUE_INFO_T prGlueInfo);*/

/*VOID
*kalP2PSetState(IN P_GLUE_INFO_T prGlueInfo,
*	       IN ENUM_PARAM_MEDIA_STATE_T eState, IN PARAM_MAC_ADDRESS rPeerAddr, IN UINT_8 ucRole);
*/

VOID
kalP2PUpdateAssocInfo(IN P_GLUE_INFO_T prGlueInfo,
	IN PUINT_8 pucFrameBody, IN UINT_32 u4FrameBodyLen, IN BOOLEAN fgReassocRequest, IN UINT_8 ucBssIndex);

/*UINT_32 kalP2PGetFreqInKHz(IN P_GLUE_INFO_T prGlueInfo);*/

INT_32 mtk_Netdev_To_RoleIdx(P_GLUE_INFO_T prGlueInfo, struct net_device *ndev, PUINT_8 pucRoleIdx);

UINT_8 kalP2PGetRole(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucRoleIdx);

#if 1
VOID kalP2PSetRole(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucRole, IN UINT_8 ucRoleIdx);

#else
VOID
kalP2PSetRole(IN P_GLUE_INFO_T prGlueInfo,
	      IN UINT_8 ucResult, IN PUINT_8 pucSSID, IN UINT_8 ucSSIDLen, IN UINT_8 ucRole);
#endif

VOID kalP2PSetCipher(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Cipher, IN UINT_8 ucRoleIdx);

BOOLEAN kalP2PGetCipher(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucRoleIdx);

BOOLEAN kalP2PGetWepCipher(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucRoleIdx);

BOOLEAN kalP2PGetTkipCipher(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucRoleIdx);

BOOLEAN kalP2PGetCcmpCipher(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucRoleIdx);

#if CFG_SUPPORT_SUITB
BOOLEAN kalP2PGetGcmp256Cipher(IN P_GLUE_INFO_T prGlueInfo,
				IN UINT_8 ucRoleIdx);
#endif
VOID kalP2PSetWscMode(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucWscMode);

UINT_8 kalP2PGetWscMode(IN P_GLUE_INFO_T prGlueInfo);

UINT_16 kalP2PCalWSC_IELen(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucType, IN UINT_8 ucRoleIdx);

VOID kalP2PGenWSC_IE(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucType, IN PUINT_8 pucBuffer, IN UINT_8 ucRoleIdx);

VOID kalP2PUpdateWSC_IE(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucType, IN PUINT_8 pucBuffer,
	IN UINT_16 u2BufferLength, IN UINT_8 ucRoleIdx);

UINT_16 kalP2PCalP2P_IELen(IN P_GLUE_INFO_T prGlueInfo,
	IN UINT_32 u4IEIdx, IN UINT_8 ucRoleIdx);
VOID kalP2PGenP2P_IE(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4IEIdx,
	IN PUINT_8 pucBuffer, IN UINT_8 ucRoleIdx);
VOID kalP2PResetP2P_IE(IN P_GLUE_INFO_T prGlueInfo,
	IN UINT_8 ucRoleIdx);
VOID kalP2PUpdateP2P_IE(IN P_GLUE_INFO_T prGlueInfo,
	IN PUINT_8 pucBuffer, IN UINT_16 u2BufferLength, IN UINT_8 ucRoleIdx);

BOOLEAN kalP2PIndicateFound(IN P_GLUE_INFO_T prGlueInfo);

VOID kalP2PIndicateConnReq(IN P_GLUE_INFO_T prGlueInfo, IN PUINT_8 pucDevName, IN INT_32 u4NameLength,
	IN PARAM_MAC_ADDRESS rPeerAddr, IN UINT_8 ucDevType,	/* 0: P2P Device / 1: GC / 2: GO */
			   IN INT_32 i4ConfigMethod, IN INT_32 i4ActiveConfigMethod);

/*VOID kalP2PInvitationStatus(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4InvStatus);*/

VOID
kalP2PInvitationIndication(IN P_GLUE_INFO_T prGlueInfo,
			   IN P_P2P_DEVICE_DESC_T prP2pDevDesc,
			   IN PUINT_8 pucSsid,
			   IN UINT_8 ucSsidLen,
			   IN UINT_8 ucOperatingChnl, IN UINT_8 ucInvitationType, IN PUINT_8 pucGroupBssid);

struct net_device *kalP2PGetDevHdlr(P_GLUE_INFO_T prGlueInfo);

VOID
kalGetChnlList(IN P_GLUE_INFO_T prGlueInfo,
	       IN ENUM_BAND_T eSpecificBand,
	       IN UINT_8 ucMaxChannelNum, IN PUINT_8 pucNumOfChannel, IN P_RF_CHANNEL_INFO_T paucChannelList);

#if CFG_SUPPORT_ANTI_PIRACY
VOID kalP2PIndicateSecCheckRsp(IN P_GLUE_INFO_T prGlueInfo, IN PUINT_8 pucRsp, IN UINT_16 u2RspLen);
#endif

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

VOID
kalP2PIndicateChannelReady(IN P_GLUE_INFO_T prGlueInfo,
			   IN UINT_64 u8SeqNum,
			   IN UINT_32 u4ChannelNum,
			   IN ENUM_BAND_T eBand, IN ENUM_CHNL_EXT_T eSco, IN UINT_32 u4Duration);

VOID kalP2PIndicateScanDone(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucRoleIndex, IN BOOLEAN fgIsAbort);

VOID
kalP2PIndicateBssInfo(IN P_GLUE_INFO_T prGlueInfo,
		      IN PUINT_8 pucFrameBuf,
		      IN UINT_32 u4BufLen, IN P_RF_CHANNEL_INFO_T prChannelInfo, IN INT_32 i4SignalStrength);

VOID
kalP2PIndicateRxMgmtFrame(IN P_GLUE_INFO_T prGlueInfo,
			  IN P_SW_RFB_T prSwRfb, IN BOOLEAN fgIsDevInterface, IN UINT_8 ucRoleIdx);

VOID kalP2PIndicateMgmtTxStatus(IN P_GLUE_INFO_T prGlueInfo, IN P_MSDU_INFO_T prMsduInfo, IN BOOLEAN fgIsAck);

VOID
kalP2PIndicateChannelExpired(IN P_GLUE_INFO_T prGlueInfo,
			     IN UINT_64 u8SeqNum,
			     IN UINT_32 u4ChannelNum, IN ENUM_BAND_T eBand, IN ENUM_CHNL_EXT_T eSco);

#if CFG_WPS_DISCONNECT  || (KERNEL_VERSION(4, 4, 0) <= CFG80211_VERSION_CODE)
VOID
kalP2PGCIndicateConnectionStatus(IN P_GLUE_INFO_T prGlueInfo,
				 IN UINT_8 ucRoleIndex,
				 IN P_P2P_CONNECTION_REQ_INFO_T prP2pConnInfo,
				 IN PUINT_8 pucRxIEBuf, IN UINT_16 u2RxIELen, IN UINT_16 u2StatusReason,
				 IN WLAN_STATUS eStatus);
#else
VOID
kalP2PGCIndicateConnectionStatus(IN P_GLUE_INFO_T prGlueInfo,
				 IN UINT_8 ucRoleIndex,
				 IN P_P2P_CONNECTION_REQ_INFO_T prP2pConnInfo,
				 IN PUINT_8 pucRxIEBuf, IN UINT_16 u2RxIELen, IN UINT_16 u2StatusReason);

#endif
VOID
kalP2PGOStationUpdate(IN P_GLUE_INFO_T prGlueInfo,
		      IN UINT_8 ucRoleIndex, IN P_STA_RECORD_T prCliStaRec, IN BOOLEAN fgIsNew);

#if (CFG_SUPPORT_DFS_MASTER == 1)
VOID
kalP2PRddDetectUpdate(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucRoleIndex);

VOID
kalP2PCacFinishedUpdate(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucRoleIndex);
#endif

#if CFG_SUPPORT_HOTSPOT_WPS_MANAGER

BOOLEAN kalP2PSetBlackList(IN P_GLUE_INFO_T prGlueInfo, IN PARAM_MAC_ADDRESS rbssid, IN BOOLEAN fgIsblock,
	IN UINT_8 ucRoleIndex);

BOOLEAN kalP2PCmpBlackList(IN P_GLUE_INFO_T prGlueInfo, IN PARAM_MAC_ADDRESS rbssid, IN UINT_8 ucRoleIndex);

VOID kalP2PSetMaxClients(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4MaxClient, IN UINT_8 ucRoleIndex);

BOOLEAN kalP2PMaxClients(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4NumClient, IN UINT_8 ucRoleIndex);

#endif

#endif /* _GL_P2P_KAL_H */
