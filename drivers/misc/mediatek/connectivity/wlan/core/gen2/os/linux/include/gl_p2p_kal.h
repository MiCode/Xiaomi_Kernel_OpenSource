/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
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

extern BOOLEAN
wextSrchDesiredWPAIE(IN PUINT_8 pucIEStart,
		     IN INT_32 i4TotalIeLen, IN UINT_8 ucDesiredElemId, OUT unsigned char **ppucDesiredIE);

#if CFG_SUPPORT_WPS
extern BOOLEAN
wextSrchDesiredWPSIE(IN PUINT_8 pucIEStart,
		     IN INT_32 i4TotalIeLen, IN UINT_8 ucDesiredElemId, OUT unsigned char **ppucDesiredIE);
#endif

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
BOOLEAN kalP2pFuncGetChannelType(IN ENUM_CHNL_EXT_T rChnlSco, OUT enum nl80211_channel_type *channel_type);
struct ieee80211_channel *kalP2pFuncGetChannelEntry(IN P_GL_P2P_INFO_T prP2pInfo, IN P_RF_CHANNEL_INFO_T prChannelInfo);

#if CFG_SUPPORT_P2P_ECSA
VOID kalP2pUpdateECSA(IN P_ADAPTER_T prAdapter, IN P_EVENT_ECSA_RESULT prECSA);
#endif

/* Service Discovery */
VOID kalP2PIndicateSDRequest(IN P_GLUE_INFO_T prGlueInfo, IN PARAM_MAC_ADDRESS rPeerAddr, IN UINT_8 ucSeqNum);

void kalP2PIndicateSDResponse(IN P_GLUE_INFO_T prGlueInfo, IN PARAM_MAC_ADDRESS rPeerAddr, IN UINT_8 ucSeqNum);

VOID kalP2PIndicateTXDone(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucSeqNum, IN UINT_8 ucStatus);

/*----------------------------------------------------------------------------*/
/* Wi-Fi Direct handling                                                      */
/*----------------------------------------------------------------------------*/
ENUM_PARAM_MEDIA_STATE_T kalP2PGetState(IN P_GLUE_INFO_T prGlueInfo);

VOID
kalP2PSetState(IN P_GLUE_INFO_T prGlueInfo,
	       IN ENUM_PARAM_MEDIA_STATE_T eState, IN PARAM_MAC_ADDRESS rPeerAddr, IN UINT_8 ucRole);

VOID
kalP2PUpdateAssocInfo(IN P_GLUE_INFO_T prGlueInfo,
		      IN PUINT_8 pucFrameBody, IN UINT_32 u4FrameBodyLen, IN BOOLEAN fgReassocRequest);

UINT_32 kalP2PGetFreqInKHz(IN P_GLUE_INFO_T prGlueInfo);

UINT_8 kalP2PGetRole(IN P_GLUE_INFO_T prGlueInfo);

VOID
kalP2PSetRole(IN P_GLUE_INFO_T prGlueInfo,
	      IN UINT_8 ucResult, IN PUINT_8 pucSSID, IN UINT_8 ucSSIDLen, IN UINT_8 ucRole);

VOID kalP2PSetCipher(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Cipher);

BOOLEAN kalP2PGetCipher(IN P_GLUE_INFO_T prGlueInfo);

BOOLEAN kalP2PGetTkipCipher(IN P_GLUE_INFO_T prGlueInfo);

BOOLEAN kalP2PGetCcmpCipher(IN P_GLUE_INFO_T prGlueInfo);

VOID kalP2PSetWscMode(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucWscMode);

UINT_8 kalP2PGetWscMode(IN P_GLUE_INFO_T prGlueInfo);

UINT_16 kalP2PCalWSC_IELen(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucType);

VOID kalP2PGenWSC_IE(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucType, IN PUINT_8 pucBuffer);

VOID kalP2PUpdateWSC_IE(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucType, IN PUINT_8 pucBuffer, IN UINT_16 u2BufferLength);

UINT_16 kalP2PCalP2P_IELen(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucIndex);

VOID kalP2PGenP2P_IE(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucIndex, IN PUINT_8 pucBuffer);

VOID kalP2PUpdateP2P_IE(IN P_GLUE_INFO_T prGlueInfo,
		IN UINT_8 ucIndex, IN PUINT_8 pucBuffer, IN UINT_16 u2BufferLength);

BOOLEAN kalP2PIndicateFound(IN P_GLUE_INFO_T prGlueInfo);

VOID kalP2PIndicateConnReq(IN P_GLUE_INFO_T prGlueInfo, IN PUINT_8 pucDevName, IN INT_32 u4NameLength,
			   IN PARAM_MAC_ADDRESS rPeerAddr, IN UINT_8 ucDevType,	/* 0: P2P Device / 1: GC / 2: GO */
			   IN INT_32 i4ConfigMethod, IN INT_32 i4ActiveConfigMethod);

VOID kalP2PInvitationStatus(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4InvStatus);

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

VOID kalP2PIndicateScanDone(IN P_GLUE_INFO_T prGlueInfo, IN BOOLEAN fgIsAbort);

VOID
kalP2PIndicateBssInfo(IN P_GLUE_INFO_T prGlueInfo,
		      IN PUINT_8 pucFrameBuf,
		      IN UINT_32 u4BufLen, IN P_RF_CHANNEL_INFO_T prChannelInfo, IN INT_32 i4SignalStrength);

VOID
kalP2PIndicateCompleteBssInfo(IN P_GLUE_INFO_T prGlueInfo, IN P_BSS_DESC_T prSpecificBssDesc);

VOID kalP2PIndicateRxMgmtFrame(IN P_GLUE_INFO_T prGlueInfo, IN P_SW_RFB_T prSwRfb);

VOID
kalP2PIndicateMgmtTxStatus(IN P_GLUE_INFO_T prGlueInfo,
			   IN UINT_64 u8Cookie, IN BOOLEAN fgIsAck, IN PUINT_8 pucFrameBuf, IN UINT_32 u4FrameLen);

VOID kalP2PIndicateChannelExpired(IN P_GLUE_INFO_T prGlueInfo, IN P_P2P_CHNL_REQ_INFO_T prChnlReqInfo);

VOID
kalP2PGCIndicateConnectionStatus(IN P_GLUE_INFO_T prGlueInfo,
				 IN P_P2P_CONNECTION_REQ_INFO_T prP2pConnInfo,
				 IN PUINT_8 pucRxIEBuf, IN UINT_16 u2RxIELen, IN UINT_16 u2StatusReason,
				 IN WLAN_STATUS eStatus);

VOID kalP2PGOStationUpdate(IN P_GLUE_INFO_T prGlueInfo, IN P_STA_RECORD_T prCliStaRec, IN BOOLEAN fgIsNew);

INT_32 kalP2PSetBlackList(IN P_GLUE_INFO_T prGlueInfo, IN PARAM_MAC_ADDRESS bssid, IN BOOLEAN block);

BOOLEAN kalP2PCmpBlackList(IN P_GLUE_INFO_T prGlueInfo, IN PARAM_MAC_ADDRESS bssid);

VOID kalP2PSetMaxClients(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4MaxClient);

BOOLEAN kalP2PReachMaxClients(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4NumClient);

VOID kalP2pUnlinkBss(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 aucBSSID[]);

void kalP2pIndicateQueuedMgmtFrame(IN P_GLUE_INFO_T prGlueInfo,
		IN struct P2P_QUEUED_ACTION_FRAME *prFrame);

void kalP2pIndicateAcsResult(IN P_GLUE_INFO_T prGlueInfo,
		IN uint8_t ucPrimaryCh,
		IN uint8_t ucSecondCh,
		IN uint8_t ucSeg0Ch,
		IN uint8_t ucSeg1Ch,
		IN enum ENUM_MAX_BANDWIDTH_SETTING eChnlBw);

#endif /* _GL_P2P_KAL_H */
