/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _P2P_FUNC_H
#define _P2P_FUNC_H

#include "precomp.h"

#define P2P_OFF_CHNL_TX_DEFAULT_TIME_MS                      1000

/*******************************************************************************
 *                                 M A C R O S
 ********************************************************************************
 */
#define ATTRI_ID(_fp)       (((P_P2P_ATTRIBUTE_T)_fp)->ucId)
#define ATTRI_LEN(_fp)      \
	(((UINT_16)((PUINT_8)&((P_P2P_ATTRIBUTE_T)_fp)->u2Length)[0]) | \
	 ((UINT_16)((PUINT_8)&((P_P2P_ATTRIBUTE_T)_fp)->u2Length)[1] << 8))

#define ATTRI_SIZE(_fp)     (P2P_ATTRI_HDR_LEN + ATTRI_LEN(_fp))

#define P2P_ATTRI_FOR_EACH(_pucAttriBuf, _u2AttriBufLen, _u2Offset) \
	for ((_u2Offset) = 0; ((_u2Offset) < (_u2AttriBufLen)); \
	     (_u2Offset) += ATTRI_SIZE(_pucAttriBuf), ((_pucAttriBuf) += ATTRI_SIZE(_pucAttriBuf)))

#define P2P_IE(_fp)          ((P_IE_P2P_T)_fp)

#define WSC_ATTRI_ID(_fp)     \
	(((UINT_16)((PUINT_8)&((P_WSC_ATTRIBUTE_T)_fp)->u2Id)[0] << 8) | \
	 ((UINT_16)((PUINT_8)&((P_WSC_ATTRIBUTE_T)_fp)->u2Id)[1]))

#define WSC_ATTRI_LEN(_fp)      \
	(((UINT_16)((PUINT_8)&((P_WSC_ATTRIBUTE_T)_fp)->u2Length)[0] << 8) | \
	 ((UINT_16)((PUINT_8)&((P_WSC_ATTRIBUTE_T)_fp)->u2Length)[1]))

#define WSC_ATTRI_SIZE(_fp)     (WSC_ATTRI_HDR_LEN + WSC_ATTRI_LEN(_fp))

#define WSC_ATTRI_FOR_EACH(_pucAttriBuf, _u2AttriBufLen, _u2Offset) \
	for ((_u2Offset) = 0; ((_u2Offset) < (_u2AttriBufLen)); \
	     (_u2Offset) += WSC_ATTRI_SIZE(_pucAttriBuf), ((_pucAttriBuf) += WSC_ATTRI_SIZE(_pucAttriBuf)))

#define WSC_IE(_fp)          ((P_IE_P2P_T)_fp)

#define WFD_ATTRI_SIZE(_fp)     (P2P_ATTRI_HDR_LEN + WSC_ATTRI_LEN(_fp))

#define WFD_ATTRI_FOR_EACH(_pucAttriBuf, _u2AttriBufLen, _u2Offset) \
	for ((_u2Offset) = 0; ((_u2Offset) < (_u2AttriBufLen)); \
	     (_u2Offset) += WFD_ATTRI_SIZE(_pucAttriBuf), ((_pucAttriBuf) += WFD_ATTRI_SIZE(_pucAttriBuf)))

/*******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
 ********************************************************************************
 */

VOID p2pFuncRequestScan(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex, IN P_P2P_SCAN_REQ_INFO_T prScanReqInfo);

VOID p2pFuncCancelScan(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex, IN P_P2P_SCAN_REQ_INFO_T prScanReqInfo);

VOID
p2pFuncUpdateBssInfoForJOIN(IN P_ADAPTER_T prAdapter, IN P_BSS_DESC_T prBssDesc, IN P_STA_RECORD_T prStaRec, IN
			    P_BSS_INFO_T prP2pBssInfo, IN P_SW_RFB_T prAssocRspSwRfb);

VOID p2pFuncAcquireCh(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIdx, IN P_P2P_CHNL_REQ_INFO_T prChnlReqInfo);

VOID
p2pFuncDisconnect(IN P_ADAPTER_T prAdapter, IN P_BSS_INFO_T prP2pBssInfo, IN P_STA_RECORD_T prStaRec, IN BOOLEAN
		  fgSendDeauth, IN UINT_16 u2ReasonCode);

P_BSS_INFO_T p2pFuncBSSIDFindBssInfo(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucBSSID);

/* //////////////////////////////// MT6630 CODE //////////////////////////////////// */

VOID p2pFuncGCJoin(IN P_ADAPTER_T prAdapter, IN P_BSS_INFO_T prP2pBssInfo, IN P_P2P_JOIN_INFO_T prP2pJoinInfo);

VOID
p2pFuncStartGO(IN P_ADAPTER_T prAdapter,
	       IN P_BSS_INFO_T prBssInfo,
	       IN P_P2P_CONNECTION_REQ_INFO_T prP2pConnReqInfo,
	       IN P_P2P_CHNL_REQ_INFO_T prP2pChnlReqInfo);

VOID p2pFuncStopGO(IN P_ADAPTER_T prAdapter, IN P_BSS_INFO_T prP2pBssInfo);

WLAN_STATUS p2pFuncRoleToBssIdx(IN P_ADAPTER_T prAdapter, IN UINT_8 ucRoleIdx, OUT PUINT_8 pucBssIdx);

VOID
p2pFuncSwitchOPMode(IN P_ADAPTER_T prAdapter, IN P_BSS_INFO_T prP2pBssInfo, IN ENUM_OP_MODE_T eOpMode, IN BOOLEAN
		    fgSyncToFW);

VOID p2pFuncReleaseCh(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIdx, IN P_P2P_CHNL_REQ_INFO_T prChnlReqInfo);

VOID p2pFuncSetChannel(IN P_ADAPTER_T prAdapter, IN UINT_8 ucRoleIdx, IN P_RF_CHANNEL_INFO_T prRfChannelInfo);

BOOLEAN p2pFuncRetryJOIN(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec, IN P_P2P_JOIN_INFO_T prJoinInfo);

WLAN_STATUS
p2pFuncTxMgmtFrame(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex, IN P_MSDU_INFO_T prMgmtTxMsdu, IN BOOLEAN
		   fgNonCckRate);

WLAN_STATUS
p2pFuncProcessBeacon(IN P_ADAPTER_T prAdapter,
		     IN P_BSS_INFO_T prP2pBssInfo,
		     IN P_P2P_BEACON_UPDATE_INFO_T prBcnUpdateInfo,
		     IN PUINT_8 pucNewBcnHdr,
		     IN UINT_32 u4NewHdrLen,
		     IN PUINT_8 pucNewBcnBody,
		     IN UINT_32 u4NewBodyLen);

BOOLEAN
p2pFuncValidateAuth(IN P_ADAPTER_T prAdapter, IN P_BSS_INFO_T prBssInfo, IN P_SW_RFB_T prSwRfb, IN PP_STA_RECORD_T
		    pprStaRec, OUT PUINT_16 pu2StatusCode);

BOOLEAN p2pFuncValidateAssocReq(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb, OUT PUINT_16 pu2StatusCode);

VOID p2pFuncResetStaRecStatus(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec);

VOID
p2pFuncInitConnectionSettings(IN P_ADAPTER_T prAdapter, IN P_P2P_CONNECTION_SETTINGS_T prP2PConnSettings, IN BOOLEAN
			      fgIsApMode);

BOOLEAN p2pFuncParseCheckForP2PInfoElem(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucBuf, OUT PUINT_8 pucOuiType);

BOOLEAN
p2pFuncValidateProbeReq(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb, OUT PUINT_32 pu4ControlFlags, IN BOOLEAN
			fgIsDevInterface, IN UINT_8 ucRoleIdx);

VOID p2pFuncValidateRxActionFrame(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb);

BOOLEAN p2pFuncIsAPMode(IN P_P2P_CONNECTION_SETTINGS_T prP2pConnSettings);

VOID
p2pFuncParseBeaconIEs(IN P_ADAPTER_T prAdapter,
		      IN P_BSS_INFO_T prP2pBssInfo,
		      IN PUINT_8 pucIEInfo,
		      IN UINT_32 u4IELen);

P_BSS_DESC_T
p2pFuncKeepOnConnection(IN P_ADAPTER_T prAdapter, IN P_BSS_INFO_T prBssInfo, IN P_P2P_CONNECTION_REQ_INFO_T
			prConnReqInfo, IN P_P2P_CHNL_REQ_INFO_T prChnlReqInfo, IN P_P2P_SCAN_REQ_INFO_T prScanReqInfo);

VOID p2pFuncStoreAssocRspIEBuffer(IN P_ADAPTER_T prAdapter, IN P_P2P_JOIN_INFO_T prP2pJoinInfo, IN P_SW_RFB_T prSwRfb);

VOID
p2pFuncMgmtFrameRegister(IN P_ADAPTER_T prAdapter, IN UINT_16 u2FrameType, IN BOOLEAN fgIsRegistered, OUT PUINT_32
			 pu4P2pPacketFilter);

VOID p2pFuncUpdateMgmtFrameRegister(IN P_ADAPTER_T prAdapter, IN UINT_32 u4OsFilter);

VOID p2pFuncGetStationInfo(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucMacAddr, OUT P_P2P_STATION_INFO_T prStaInfo);

P_MSDU_INFO_T p2pFuncProcessP2pProbeRsp(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIdx, IN P_MSDU_INFO_T prMgmtTxMsdu);

#if 0				/* LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 0) */
UINT_32
p2pFuncCalculateExtra_IELenForBeacon(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex, IN P_STA_RECORD_T prStaRec);

VOID p2pFuncGenerateExtra_IEForBeacon(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo);

#else
UINT_32 p2pFuncCalculateP2p_IELenForBeacon(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex, IN P_STA_RECORD_T prStaRec);

VOID p2pFuncGenerateP2p_IEForBeacon(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo);

UINT_32 p2pFuncCalculateWSC_IELenForBeacon(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex, IN P_STA_RECORD_T prStaRec);

VOID p2pFuncGenerateWSC_IEForBeacon(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo);
#endif
UINT_32
p2pFuncCalculateP2p_IELenForAssocRsp(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex, IN P_STA_RECORD_T prStaRec);

VOID p2pFuncGenerateP2p_IEForAssocRsp(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo);

UINT_32
p2pFuncCalculateP2P_IELen(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex, IN P_STA_RECORD_T prStaRec, IN
			  APPEND_VAR_ATTRI_ENTRY_T arAppendAttriTable[], IN UINT_32 u4AttriTableSize);

VOID
p2pFuncGenerateP2P_IE(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgIsAssocFrame, IN PUINT_16 pu2Offset, IN PUINT_8 pucBuf, IN
		      UINT_16 u2BufSize, IN APPEND_VAR_ATTRI_ENTRY_T arAppendAttriTable[], IN UINT_32 u4AttriTableSize);

UINT_32
p2pFuncAppendAttriStatusForAssocRsp(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgIsAssocFrame, IN PUINT_16 pu2Offset, IN
				    PUINT_8 pucBuf, IN UINT_16 u2BufSize);

UINT_32
p2pFuncAppendAttriExtListenTiming(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgIsAssocFrame, IN PUINT_16 pu2Offset, IN PUINT_8
				  pucBuf, IN UINT_16 u2BufSize);

VOID
p2pFuncDissolve(IN P_ADAPTER_T prAdapter, IN P_BSS_INFO_T prP2pBssInfo, IN BOOLEAN fgSendDeauth, IN UINT_16
		u2ReasonCode);

P_IE_HDR_T
p2pFuncGetSpecIE(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucIEBuf, IN UINT_16 u2BufferLen, IN UINT_8 ucElemID, IN PBOOLEAN
		 pfgIsMore);

P_ATTRIBUTE_HDR_T
p2pFuncGetSpecAttri(IN P_ADAPTER_T prAdapter, IN UINT_8 ucOuiType, IN PUINT_8 pucIEBuf, IN UINT_16 u2BufferLen, IN
		    UINT_8 ucAttriID);

UINT_32 wfdFuncCalculateWfdIELenForAssocRsp(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex, IN P_STA_RECORD_T prStaRec);

VOID wfdFuncGenerateWfdIEForAssocRsp(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo);

UINT_32 p2pFuncCalculateP2P_IE_NoA(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIdx, IN P_STA_RECORD_T prStaRec);

VOID p2pFuncGenerateP2P_IE_NoA(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo);

UINT_8 p2pFunGetAcsBestCh(IN P_ADAPTER_T prAdapter,
		IN ENUM_BAND_T eBand,
		IN ENUM_MAX_BANDWIDTH_SETTING eChnlBw,
		IN UINT_32 u4LteSafeChnMask_2G,
		IN UINT_32 u4LteSafeChnMask_5G_1,
		IN UINT_32 u4LteSafeChnMask_5G_2);

void p2pFunProcessAcsReport(IN P_ADAPTER_T prAdapter,
		IN UINT_8 ucRoleIndex,
		IN P_PARAM_GET_CHN_INFO prLteSafeChnInfo,
		IN struct P2P_ACS_REQ_INFO *prAcsReqInfo);

void p2pFunIndicateAcsResult(IN P_GLUE_INFO_T prGlueInfo,
		IN struct P2P_ACS_REQ_INFO *prAcsReqInfo);

void p2pFunCalAcsChnScores(IN P_ADAPTER_T prAdapter,
		IN ENUM_BAND_T eBand);

#endif
