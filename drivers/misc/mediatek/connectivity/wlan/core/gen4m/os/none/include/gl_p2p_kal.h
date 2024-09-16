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
 ** Id: //Department/DaVinci/TRUNK/WiFi_P2P_Driver/
 *        os/linux/include/gl_p2p_kal.h#2
 */

/*! \file   gl_p2p_kal.h
 *    \brief  Declaration of KAL functions for Wi-Fi Direct support
 *	    - kal*() which is provided by GLUE Layer.
 *
 *    Any definitions in this file will be shared among GLUE Layer
 *    and internal Driver Stack.
 */


#ifndef _GL_P2P_KAL_H
#define _GL_P2P_KAL_H

/******************************************************************************
 *                         C O M P I L E R   F L A G S
 ******************************************************************************
 */

/******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 ******************************************************************************
 */
#include "config.h"
#include "gl_typedef.h"
#include "gl_os.h"
#include "wlan_lib.h"
#include "wlan_oid.h"
#include "wlan_p2p.h"
#include "gl_kal.h"
/* for some structure in p2p_ioctl.h */
#include "gl_p2p_ioctl.h"
#include "nic/p2p.h"

#if DBG
extern int allocatedMemSize;
#endif


#define kalP2pFuncGetChannelType(_rChnlSco, _prChannelType)

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

/* Service Discovery */
void kalP2PIndicateSDRequest(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t rPeerAddr[PARAM_MAC_ADDR_LEN],
		IN uint8_t ucSeqNum);

void kalP2PIndicateSDResponse(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t rPeerAddr[PARAM_MAC_ADDR_LEN],
		IN uint8_t ucSeqNum);

void kalP2PIndicateTXDone(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t ucSeqNum, IN uint8_t ucStatus);

/*------------------------------------------------------------------------*/
/* Wi-Fi Direct handling                                                      */
/*------------------------------------------------------------------------*/
/*ENUM_PARAM_MEDIA_STATE_T kalP2PGetState(IN P_GLUE_INFO_T prGlueInfo);*/

/*VOID
 *kalP2PSetState(IN P_GLUE_INFO_T prGlueInfo,
 *		IN ENUM_PARAM_MEDIA_STATE_T eState,
 *		IN PARAM_MAC_ADDRESS rPeerAddr,
 *		IN UINT_8 ucRole);
 */

/*UINT_32 kalP2PGetFreqInKHz(IN P_GLUE_INFO_T prGlueInfo);*/

int32_t mtk_Netdev_To_RoleIdx(struct GLUE_INFO *prGlueInfo,
		struct net_device *ndev,
		uint8_t *pucRoleIdx);

#ifdef CFG_REMIND_IMPLEMENT
#define kalP2PUpdateAssocInfo(_prGlueInfo, _pucFrameBody, _u4FrameBodyLen, \
	_fgReassocRequest, _ucBssIndex) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalP2PGetRole(_prGlueInfo, _ucRoleIdx) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalP2PSetRole(_prGlueInfo, _ucRole, _ucRoleIdx) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalP2PSetCipher(_prGlueInfo, _u4Cipher, _ucRoleIdx) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalP2PGetCipher(_prGlueInfo, _ucRoleIdx) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalP2PGetWepCipher(_prGlueInfo, _ucRoleIdx) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalP2PGetTkipCipher(_prGlueInfo, _ucRoleIdx) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalP2PGetCcmpCipher(_prGlueInfo, _ucRoleIdx) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)
#else
void
kalP2PUpdateAssocInfo(IN struct GLUE_INFO *prGlueInfo,
	IN uint8_t *pucFrameBody,
	IN uint32_t u4FrameBodyLen,
	IN u_int8_t fgReassocRequest,
	IN uint8_t ucBssIndex);

uint8_t kalP2PGetRole(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t ucRoleIdx);

void kalP2PSetRole(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t ucRole,
		IN uint8_t ucRoleIdx);

void kalP2PSetCipher(IN struct GLUE_INFO *prGlueInfo,
		IN uint32_t u4Cipher,
		IN uint8_t ucRoleIdx);

u_int8_t kalP2PGetCipher(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t ucRoleIdx);

u_int8_t kalP2PGetWepCipher(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t ucRoleIdx);

u_int8_t kalP2PGetTkipCipher(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t ucRoleIdx);

u_int8_t kalP2PGetCcmpCipher(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t ucRoleIdx);
#endif

void kalP2PSetWscMode(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t ucWscMode);

uint8_t kalP2PGetWscMode(IN struct GLUE_INFO *prGlueInfo);

#ifdef CFG_REMIND_IMPLEMENT
#define kalP2PCalWSC_IELen(_prGlueInfo, _ucType, _ucRoleIdx) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalP2PGenWSC_IE(_prGlueInfo, _ucType, _pucBuffer, _ucRoleIdx) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalP2PUpdateWSC_IE(_prGlueInfo, _ucType, _pucBuffer, \
	_u2BufferLength, _ucRoleIdx) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalP2PCalP2P_IELen(_prGlueInfo, _ucIndex, _ucRoleIdx) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalP2PGenP2P_IE(_prGlueInfo, _ucIndex, _pucBuffer, _ucRoleIdx) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalP2PUpdateP2P_IE(_prGlueInfo, _ucIndex, _pucBuffer, \
	_u2BufferLength, _ucRoleIdx) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)
#else
uint16_t kalP2PCalWSC_IELen(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t ucType,
		IN uint8_t ucRoleIdx);

void kalP2PGenWSC_IE(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t ucType,
		IN uint8_t *pucBuffer,
		IN uint8_t ucRoleIdx);

void kalP2PUpdateWSC_IE(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t ucType,
		IN uint8_t *pucBuffer,
		IN uint16_t u2BufferLength,
		IN uint8_t ucRoleIdx);

uint16_t kalP2PCalP2P_IELen(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t ucIndex,
		IN uint8_t ucRoleIdx);

void kalP2PGenP2P_IE(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t ucIndex,
		IN uint8_t *pucBuffer,
		IN uint8_t ucRoleIdx);

void kalP2PUpdateP2P_IE(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t ucIndex,
		IN uint8_t *pucBuffer,
		IN uint16_t u2BufferLength,
		IN uint8_t ucRoleIdx);
#endif

u_int8_t kalP2PIndicateFound(IN struct GLUE_INFO *prGlueInfo);

void kalP2PIndicateConnReq(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t *pucDevName,
		IN int32_t u4NameLength,
		IN uint8_t rPeerAddr[PARAM_MAC_ADDR_LEN],
		IN uint8_t ucDevType,	/* 0: P2P Device / 1: GC / 2: GO */
		IN int32_t i4ConfigMethod,
		IN int32_t i4ActiveConfigMethod);

/*VOID kalP2PInvitationStatus(IN P_GLUE_INFO_T prGlueInfo,
 *		IN UINT_32 u4InvStatus);
 */

void
kalP2PInvitationIndication(IN struct GLUE_INFO *prGlueInfo,
		IN struct P2P_DEVICE_DESC *prP2pDevDesc,
		IN uint8_t *pucSsid,
		IN uint8_t ucSsidLen,
		IN uint8_t ucOperatingChnl,
		IN uint8_t ucInvitationType,
		IN uint8_t *pucGroupBssid);

struct net_device *kalP2PGetDevHdlr(struct GLUE_INFO *prGlueInfo);

void
kalGetChnlList(IN struct GLUE_INFO *prGlueInfo,
		IN enum ENUM_BAND eSpecificBand,
		IN uint8_t ucMaxChannelNum,
		IN uint8_t *pucNumOfChannel,
		IN struct RF_CHANNEL_INFO *paucChannelList);

#if CFG_SUPPORT_ANTI_PIRACY
void kalP2PIndicateSecCheckRsp(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t *pucRsp,
		IN uint16_t u2RspLen);
#endif

/******************************************************************************
 *                              F U N C T I O N S
 ******************************************************************************
 */

#ifdef CFG_REMIND_IMPLEMENT
#define kalP2PIndicateChannelReady(_prGlueInfo, _u8SeqNum, _u4ChannelNum, \
	_eBand, _eSco, _u4Duration) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalP2PIndicateScanDone(_prGlueInfo, _ucRoleIndex, _fgIsAbort) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalP2PIndicateBssInfo(_prGlueInfo, _pucFrameBuf, _u4BufLen, \
	_prChannelInfo, _i4SignalStrength) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalP2PIndicateRxMgmtFrame(_prGlueInfo, _prSwRfb, \
	_fgIsDevInterface, _ucRoleIdx) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalP2PIndicateMgmtTxStatus(_prGlueInfo, _prMsduInfo, _fgIsAck) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalP2PIndicateChannelExpired(_prGlueInfo, _u8SeqNum, \
	_u4ChannelNum, _eBand, _eSco) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalP2PGCIndicateConnectionStatus(_prGlueInfo, _ucRoleIndex, \
	_prP2pConnInfo, _pucRxIEBuf, _u2RxIELen, _u2StatusReason) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalP2PGOStationUpdate(_pr, _ucRoleIndex, _prCliStaRec, _fgIsNew) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _pr)
#else
void
kalP2PIndicateChannelReady(IN struct GLUE_INFO *prGlueInfo,
		IN uint64_t u8SeqNum,
		IN uint32_t u4ChannelNum,
		IN enum ENUM_BAND eBand,
		IN enum ENUM_CHNL_EXT eSco,
		IN uint32_t u4Duration);

void kalP2PIndicateScanDone(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t ucRoleIndex,
		IN u_int8_t fgIsAbort);

void
kalP2PIndicateBssInfo(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t *pucFrameBuf,
		IN uint32_t u4BufLen,
		IN struct RF_CHANNEL_INFO *prChannelInfo,
		IN int32_t i4SignalStrength);

void
kalP2PIndicateRxMgmtFrame(IN struct GLUE_INFO *prGlueInfo,
		IN struct SW_RFB *prSwRfb,
		IN u_int8_t fgIsDevInterface,
		IN uint8_t ucRoleIdx);

void kalP2PIndicateMgmtTxStatus(IN struct GLUE_INFO *prGlueInfo,
		IN struct MSDU_INFO *prMsduInfo,
		IN u_int8_t fgIsAck);

void
kalP2PIndicateChannelExpired(IN struct GLUE_INFO *prGlueInfo,
		IN uint64_t u8SeqNum,
		IN uint32_t u4ChannelNum,
		IN enum ENUM_BAND eBand,
		IN enum ENUM_CHNL_EXT eSco);

void
kalP2PGCIndicateConnectionStatus(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t ucRoleIndex,
		IN struct P2P_CONNECTION_REQ_INFO *prP2pConnInfo,
		IN uint8_t *pucRxIEBuf,
		IN uint16_t u2RxIELen,
		IN uint16_t u2StatusReason);

void
kalP2PGOStationUpdate(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t ucRoleIndex,
		IN struct STA_RECORD *prCliStaRec,
		IN u_int8_t fgIsNew);
#endif

#if (CFG_SUPPORT_DFS_MASTER == 1)
#ifdef CFG_REMIND_IMPLEMENT
#define kalP2PRddDetectUpdate(_prGlueInfo, _ucRoleIndex) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalP2PCacFinishedUpdate(_prGlueInfo, _ucRoleIndex) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)
#else
void
kalP2PRddDetectUpdate(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t ucRoleIndex);

void
kalP2PCacFinishedUpdate(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t ucRoleIndex);
#endif
#endif

#if CFG_SUPPORT_HOTSPOT_WPS_MANAGER

u_int8_t kalP2PSetBlackList(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t rbssid[PARAM_MAC_ADDR_LEN],
		IN u_int8_t fgIsblock,
		IN uint8_t ucRoleIndex);

u_int8_t kalP2PResetBlackList(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t ucRoleIndex);

void kalP2PSetMaxClients(IN struct GLUE_INFO *prGlueInfo,
		IN uint32_t u4MaxClient,
		IN uint8_t ucRoleIndex);

#ifdef CFG_REMIND_IMPLEMENT
#define kalP2PCmpBlackList(_prGlueInfo, _rbssid, _ucRoleIndex) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalP2PMaxClients(_prGlueInfo, _u4NumClient, _ucRoleIndex) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)
#else
u_int8_t kalP2PCmpBlackList(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t rbssid[PARAM_MAC_ADDR_LEN],
		IN uint8_t ucRoleIndex);

u_int8_t kalP2PMaxClients(IN struct GLUE_INFO *prGlueInfo,
		IN uint32_t u4NumClient,
		IN uint8_t ucRoleIndex);
#endif
#endif

#ifdef CFG_REMIND_IMPLEMENT
#define kalP2pUnlinkBss(_prGlueInfo, _aucBSSID) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalP2pIndicateQueuedMgmtFrame(_prGlueInfo, _prFrame) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalP2pIndicateAcsResult(_prGlueInfo, _ucRoleIndex, _ucPrimaryCh,\
	_ucSecondCh, _ucSeg0Ch, _ucSeg1Ch, _eChnlBw) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalP2pNotifyStopApComplete(_prAdapter, _ucRoleIndex) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

#define kalP2pIndicateChnlSwitch(_prAdapter, _prBssInfo) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
#else
void kalP2pUnlinkBss(IN struct GLUE_INFO *prGlueInfo, IN uint8_t aucBSSID[]);

void kalP2pIndicateQueuedMgmtFrame(IN struct GLUE_INFO *prGlueInfo,
		IN struct P2P_QUEUED_ACTION_FRAME *prFrame);

void kalP2pIndicateAcsResult(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t ucRoleIndex,
		IN uint8_t ucPrimaryCh,
		IN uint8_t ucSecondCh,
		IN uint8_t ucSeg0Ch,
		IN uint8_t ucSeg1Ch,
		IN enum ENUM_MAX_BANDWIDTH_SETTING eChnlBw);

void kalP2pNotifyStopApComplete(IN struct ADAPTER *prAdapter,
		IN uint8_t ucRoleIndex);

void kalP2pIndicateChnlSwitch(IN struct ADAPTER *prAdapter,
		IN struct BSS_INFO *prBssInfo);
#endif
#endif /* _GL_P2P_KAL_H */
