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
#include "precomp.h"

APPEND_VAR_ATTRI_ENTRY_T txAssocRspAttributesTable[] = {
	{(P2P_ATTRI_HDR_LEN + P2P_ATTRI_MAX_LEN_STATUS), NULL, p2pFuncAppendAttriStatusForAssocRsp}
	/* 0 *//* Status */
	, {(P2P_ATTRI_HDR_LEN + P2P_ATTRI_MAX_LEN_EXT_LISTEN_TIMING), NULL, p2pFuncAppendAttriExtListenTiming}	/* 8 */
};

APPEND_VAR_IE_ENTRY_T txProbeRspIETable[] = {
	{(ELEM_HDR_LEN + (RATE_NUM_SW - ELEM_MAX_LEN_SUP_RATES)), NULL, bssGenerateExtSuppRate_IE}	/* 50 */
	, {(ELEM_HDR_LEN + ELEM_MAX_LEN_ERP), NULL, rlmRspGenerateErpIE}	/* 42 */
	, {(ELEM_HDR_LEN + ELEM_MAX_LEN_HT_CAP), NULL, rlmRspGenerateHtCapIE}	/* 45 */
	, {(ELEM_HDR_LEN + ELEM_MAX_LEN_HT_OP), NULL, rlmRspGenerateHtOpIE}	/* 61 */
	, {(ELEM_HDR_LEN + ELEM_MAX_LEN_RSN), NULL, rsnGenerateRSNIE}	/* 48 */
	, {(ELEM_HDR_LEN + ELEM_MAX_LEN_OBSS_SCAN), NULL, rlmRspGenerateObssScanIE}	/* 74 */
	, {(ELEM_HDR_LEN + ELEM_MAX_LEN_EXT_CAP), NULL, rlmRspGenerateExtCapIE}	/* 127 */
	, {(ELEM_HDR_LEN + ELEM_MAX_LEN_WPA), NULL, rsnGenerateWpaNoneIE}	/* 221 */
	, {(ELEM_HDR_LEN + ELEM_MAX_LEN_WMM_PARAM), NULL, mqmGenerateWmmParamIE}	/* 221 */
#if CFG_SUPPORT_802_11AC
	, {(ELEM_HDR_LEN + ELEM_MAX_LEN_VHT_CAP), NULL, rlmRspGenerateVhtCapIE}	/*191 */
	, {(ELEM_HDR_LEN + ELEM_MAX_LEN_VHT_OP), NULL, rlmRspGenerateVhtOpIE}	/*192 */
	, {(ELEM_HDR_LEN + ELEM_MAX_LEN_VHT_OP_MODE_NOTIFICATION), NULL, rlmRspGenerateVhtOpNotificationIE}	/*199 */
#endif
#if CFG_SUPPORT_MTK_SYNERGY
	, {(ELEM_HDR_LEN + ELEM_MIN_LEN_MTK_OUI), NULL, rlmGenerateMTKOuiIE}	/* 221 */
#endif

};

#if (CFG_SUPPORT_DFS_MASTER == 1)
BOOLEAN g_fgManualCac = FALSE;
UINT_32 g_u4DriverCacTime;
UINT_32 g_u4CacStartBootTime;
UINT_8 g_ucRadarDetectMode = FALSE;
struct P2P_RADAR_INFO g_rP2pRadarInfo;
UINT_8 g_ucDfsState = DFS_STATE_INACTIVE;
static PUINT_8 apucDfsState[DFS_STATE_NUM] = {
	(PUINT_8) DISP_STRING("DFS_STATE_INACTIVE"),
	(PUINT_8) DISP_STRING("DFS_STATE_CHECKING"),
	(PUINT_8) DISP_STRING("DFS_STATE_ACTIVE"),
	(PUINT_8) DISP_STRING("DFS_STATE_DETECTED")
};

PUINT_8 apucW53RadarType[3] = {
	(PUINT_8) DISP_STRING("Unknown Type"),
	(PUINT_8) DISP_STRING("Type 1 (short pulse)"),
	(PUINT_8) DISP_STRING("Type 2 (short pulse)")
};
PUINT_8 apucW56RadarType[12] = {
	(PUINT_8) DISP_STRING("Unknown Type"),
	(PUINT_8) DISP_STRING("Type 1 (short pulse)"),
	(PUINT_8) DISP_STRING("Type 2 (short pulse)"),
	(PUINT_8) DISP_STRING("Type 3 (short pulse)"),
	(PUINT_8) DISP_STRING("Type 4 (short pulse)"),
	(PUINT_8) DISP_STRING("Type 5 (short pulse)"),
	(PUINT_8) DISP_STRING("Type 6 (short pulse)"),
	(PUINT_8) DISP_STRING("Type 7 (long pulse)"),
	(PUINT_8) DISP_STRING("Type 8 (short pulse)"),
	(PUINT_8) DISP_STRING("Type 4 or Type 5 or Type 6 (short pulse)"),
	(PUINT_8) DISP_STRING("Type 5 or Type 6 or Type 8 (short pulse)"),
	(PUINT_8) DISP_STRING("Type 5 or Type 6 (short pulse)")
};
#endif

static VOID
p2pFuncParseBeaconVenderId(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucIE,
			   IN P_P2P_SPECIFIC_BSS_INFO_T prP2pSpecificBssInfo, IN UINT_8 ucRoleIndex);
#if 0
static VOID
p2pFuncGetAttriListAction(IN P_ADAPTER_T prAdapter,
			  IN P_IE_P2P_T prIe, IN UINT_8 ucOuiType,
			  OUT PUINT_8 *pucAttriListStart, OUT UINT_16 *u2AttriListLen,
			  OUT BOOLEAN *fgIsAllocMem, OUT BOOLEAN *fgBackupAttributes, OUT UINT_16 *u2BufferSize);
#endif

static VOID
p2pFuncProcessP2pProbeRspAction(IN P_ADAPTER_T prAdapter,
				IN PUINT_8 pucIEBuf, IN UINT_8 ucElemIdType,
				OUT UINT_8 *ucBssIdx, OUT P_BSS_INFO_T *prP2pBssInfo, OUT BOOLEAN *fgIsWSCIE,
				OUT BOOLEAN *fgIsP2PIE, OUT BOOLEAN *fgIsWFDIE);
static VOID
p2pFuncGetSpecAttriAction(IN P_IE_P2P_T prP2pIE,
			  IN UINT_8 ucOuiType, IN UINT_8 ucAttriID, OUT P_ATTRIBUTE_HDR_T *prTargetAttri);
/*----------------------------------------------------------------------------*/
/*!
* @brief Function for requesting scan. There is an option to do ACTIVE or PASSIVE scan.
*
* @param eScanType - Specify the scan type of the scan request. It can be an ACTIVE/PASSIVE
*                                  Scan.
*              eChannelSet - Specify the preferred channel set.
*                                    A FULL scan would request a legacy full channel normal scan.(usually ACTIVE).
*                                    A P2P_SOCIAL scan would scan 1+6+11 channels.(usually ACTIVE)
*                                    A SPECIFIC scan would only 1/6/11 channels scan. (Passive Listen/Specific Search)
*               ucChannelNum - A specific channel number. (Only when channel is specified)
*               eBand - A specific band. (Only when channel is specified)
*
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID p2pFuncRequestScan(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex, IN P_P2P_SCAN_REQ_INFO_T prScanReqInfo)
{
	P_MSG_SCN_SCAN_REQ_V2 prScanReqV2 = (P_MSG_SCN_SCAN_REQ_V2) NULL;

#ifdef CFG_SUPPORT_BEAM_PLUS
	/*NFC Beam + Indication */
	P_P2P_FSM_INFO_T prP2pFsmInfo = (P_P2P_FSM_INFO_T) NULL;
#endif

	DEBUGFUNC("p2pFuncRequestScan()");

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prScanReqInfo != NULL));

		if (prScanReqInfo->eChannelSet == SCAN_CHANNEL_SPECIFIED) {
			ASSERT_BREAK(prScanReqInfo->ucNumChannelList > 0);
			DBGLOG(P2P, LOUD,
			       "P2P Scan Request Channel:%d\n", prScanReqInfo->arScanChannelList[0].ucChannelNum);
		}

		prScanReqV2 =
		    (P_MSG_SCN_SCAN_REQ_V2) cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
							(sizeof(MSG_SCN_SCAN_REQ_V2) +
							 (sizeof(PARAM_SSID_T) * prScanReqInfo->ucSsidNum)));
		if (!prScanReqV2) {
			ASSERT(0);	/* Can't trigger SCAN FSM */
			DBGLOG(P2P, ERROR,
			       "p2pFuncRequestScan: Memory allocation fail, can not send SCAN MSG to scan module\n");
			break;
		}

		prScanReqV2->rMsgHdr.eMsgId = MID_P2P_SCN_SCAN_REQ_V2;
		prScanReqV2->ucSeqNum = ++prScanReqInfo->ucSeqNumOfScnMsg;
		prScanReqV2->ucBssIndex = ucBssIndex;
		prScanReqV2->eScanType = prScanReqInfo->eScanType;
		prScanReqV2->eScanChannel = prScanReqInfo->eChannelSet;
		prScanReqV2->u2IELen = 0;
		prScanReqV2->prSsid = (P_PARAM_SSID_T) ((ULONG) prScanReqV2 + sizeof(MSG_SCN_SCAN_REQ_V2));

		/* Copy IE for Probe Request. */
		kalMemCopy(prScanReqV2->aucIE, prScanReqInfo->aucIEBuf, prScanReqInfo->u4BufLength);
		prScanReqV2->u2IELen = (UINT_16) prScanReqInfo->u4BufLength;

		prScanReqV2->u2ChannelDwellTime = prScanReqInfo->u2PassiveDewellTime;
		prScanReqV2->u2TimeoutValue = 0;
		prScanReqV2->u2ProbeDelay = 0;

		switch (prScanReqInfo->eChannelSet) {
		case SCAN_CHANNEL_SPECIFIED:
			{
				UINT_32 u4Idx = 0;
				P_RF_CHANNEL_INFO_T prDomainInfo =
				    (P_RF_CHANNEL_INFO_T) prScanReqInfo->arScanChannelList;

				if (prScanReqInfo->ucNumChannelList > MAXIMUM_OPERATION_CHANNEL_LIST)
					prScanReqInfo->ucNumChannelList = MAXIMUM_OPERATION_CHANNEL_LIST;

				for (u4Idx = 0; u4Idx < prScanReqInfo->ucNumChannelList; u4Idx++) {
					prScanReqV2->arChnlInfoList[u4Idx].ucChannelNum = prDomainInfo->ucChannelNum;
					prScanReqV2->arChnlInfoList[u4Idx].eBand = prDomainInfo->eBand;
					prDomainInfo++;
				}

				prScanReqV2->ucChannelListNum = prScanReqInfo->ucNumChannelList;
			}
			/* fallthrough */
		case SCAN_CHANNEL_FULL:
			/* fallthrough */
		case SCAN_CHANNEL_2G4:
			/* fallthrough */
		case SCAN_CHANNEL_P2P_SOCIAL:
			{
				/* UINT_8 aucP2pSsid[] = P2P_WILDCARD_SSID; */
				P_PARAM_SSID_T prParamSsid = (P_PARAM_SSID_T) NULL;

				prParamSsid = prScanReqV2->prSsid;

				for (prScanReqV2->ucSSIDNum = 0;
				     prScanReqV2->ucSSIDNum < prScanReqInfo->ucSsidNum; prScanReqV2->ucSSIDNum++) {
					COPY_SSID(prParamSsid->aucSsid,
						  prParamSsid->u4SsidLen,
						  prScanReqInfo->arSsidStruct[prScanReqV2->ucSSIDNum].aucSsid,
						  prScanReqInfo->arSsidStruct[prScanReqV2->ucSSIDNum].ucSsidLen);

					prParamSsid++;
				}

				/* For compatible. (in FW?) need to check. */
				if (prScanReqV2->ucSSIDNum == 0)
					prScanReqV2->ucSSIDType = SCAN_REQ_SSID_P2P_WILDCARD;
				else
					prScanReqV2->ucSSIDType = SCAN_REQ_SSID_SPECIFIED;
			}
			break;
		default:
			/* Currently there is no other scan channel set. */
			ASSERT(FALSE);
			break;
		}

		prScanReqInfo->fgIsScanRequest = TRUE;

		mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prScanReqV2, MSG_SEND_METHOD_BUF);

	} while (FALSE);
}				/* p2pFuncRequestScan */

VOID p2pFuncCancelScan(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex, IN P_P2P_SCAN_REQ_INFO_T prScanInfo)
{
	P_MSG_SCN_SCAN_CANCEL prScanCancelMsg = (P_MSG_SCN_SCAN_CANCEL) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prScanInfo != NULL));

		if (!prScanInfo->fgIsScanRequest)
			break;

		if (prScanInfo->ucSeqNumOfScnMsg) {
			/* There is a channel privilege on hand. */
			DBGLOG(P2P, TRACE, "P2P Cancel Scan\n");

			prScanCancelMsg =
			    (P_MSG_SCN_SCAN_CANCEL) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_SCN_SCAN_CANCEL));
			if (!prScanCancelMsg) {
				/* Buffer not enough, can not cancel scan request. */
				DBGLOG(P2P, TRACE, "Buffer not enough, can not cancel scan.\n");
				ASSERT(FALSE);
				break;
			}

			prScanCancelMsg->rMsgHdr.eMsgId = MID_P2P_SCN_SCAN_CANCEL;
			prScanCancelMsg->ucBssIndex = ucBssIndex;
			prScanCancelMsg->ucSeqNum = prScanInfo->ucSeqNumOfScnMsg++;
			prScanCancelMsg->fgIsChannelExt = FALSE;
			prScanInfo->fgIsScanRequest = FALSE;

			mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prScanCancelMsg, MSG_SEND_METHOD_BUF);

		}

	} while (FALSE);
}				/* p2pFuncCancelScan */

VOID p2pFuncGCJoin(IN P_ADAPTER_T prAdapter, IN P_BSS_INFO_T prP2pBssInfo, IN P_P2P_JOIN_INFO_T prP2pJoinInfo)
{
	P_MSG_JOIN_REQ_T prJoinReqMsg = (P_MSG_JOIN_REQ_T) NULL;
	P_STA_RECORD_T prStaRec = (P_STA_RECORD_T) NULL;
	P_BSS_DESC_T prBssDesc = (P_BSS_DESC_T) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prP2pBssInfo != NULL) && (prP2pJoinInfo != NULL));

		prBssDesc = prP2pJoinInfo->prTargetBssDesc;
		if ((prBssDesc) == NULL) {
			DBGLOG(P2P, ERROR, "p2pFuncGCJoin: NO Target BSS Descriptor\n");
			ASSERT(FALSE);
			break;
		}

		if (prBssDesc->ucSSIDLen) {
			COPY_SSID(prP2pBssInfo->aucSSID,
				  prP2pBssInfo->ucSSIDLen, prBssDesc->aucSSID, prBssDesc->ucSSIDLen);
		}

		/* 2 <1> We are goin to connect to this BSS */
		prBssDesc->fgIsConnecting = TRUE;

		/* 2 <2> Setup corresponding STA_RECORD_T */
		prStaRec = bssCreateStaRecFromBssDesc(prAdapter, (prBssDesc->fgIsP2PPresent ? (STA_TYPE_P2P_GO)
								  : (STA_TYPE_LEGACY_AP)),
						      prP2pBssInfo->ucBssIndex, prBssDesc);

		if (prStaRec == NULL) {
			DBGLOG(P2P, TRACE, "Create station record fail\n");
			ASSERT(FALSE);
			break;
		}

		prP2pJoinInfo->prTargetStaRec = prStaRec;
		prP2pJoinInfo->fgIsJoinComplete = FALSE;
		prP2pJoinInfo->u4BufLength = 0;

		/* 2 <2.1> Sync. to FW domain */
		cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_1);

		if (prP2pBssInfo->eConnectionState == PARAM_MEDIA_STATE_DISCONNECTED) {
			prStaRec->fgIsReAssoc = FALSE;
			prP2pJoinInfo->ucAvailableAuthTypes = (UINT_8) AUTH_TYPE_OPEN_SYSTEM;
			prStaRec->ucTxAuthAssocRetryLimit = TX_AUTH_ASSOCI_RETRY_LIMIT;
		} else {
			DBGLOG(P2P, ERROR, "JOIN INIT: Join Request when connected.\n");
			/* TODO: Shall we considering ROAMIN case for P2P Device?. */
			break;
		}

		/* 2 <4> Use an appropriate Authentication Algorithm Number among the ucAvailableAuthTypes. */
		if (prP2pJoinInfo->ucAvailableAuthTypes & (UINT_8) AUTH_TYPE_OPEN_SYSTEM) {

			DBGLOG(P2P, TRACE, "JOIN INIT: Try to do Authentication with AuthType == OPEN_SYSTEM.\n");

			prP2pJoinInfo->ucAvailableAuthTypes &= ~(UINT_8) AUTH_TYPE_OPEN_SYSTEM;

			prStaRec->ucAuthAlgNum = (UINT_8) AUTH_ALGORITHM_NUM_OPEN_SYSTEM;
		} else {
			DBGLOG(P2P, ERROR, "JOIN INIT: ucAvailableAuthTypes Error.\n");
			ASSERT(FALSE);
			break;
		}

		/* 4 <5> Overwrite Connection Setting for eConnectionPolicy == ANY (Used by Assoc Req) */

		/* 2 <5> Backup desired channel. */

		/* 2 <6> Send a Msg to trigger SAA to start JOIN process. */
		prJoinReqMsg = (P_MSG_JOIN_REQ_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_JOIN_REQ_T));

		if (!prJoinReqMsg) {
			DBGLOG(P2P, TRACE, "Allocation Join Message Fail\n");
			ASSERT(FALSE);
			return;
		}

		prJoinReqMsg->rMsgHdr.eMsgId = MID_P2P_SAA_FSM_START;
		prJoinReqMsg->ucSeqNum = ++prP2pJoinInfo->ucSeqNumOfReqMsg;
		prJoinReqMsg->prStaRec = prStaRec;

		/* TODO: Consider fragmentation info in station record. */

		mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prJoinReqMsg, MSG_SEND_METHOD_BUF);

	} while (FALSE);

}				/* p2pFuncGCJoin */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will update the contain of BSS_INFO_T for AIS network once
*        the association was completed.
*
* @param[in] prStaRec               Pointer to the STA_RECORD_T
* @param[in] prAssocRspSwRfb        Pointer to SW RFB of ASSOC RESP FRAME.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID
p2pFuncUpdateBssInfoForJOIN(IN P_ADAPTER_T prAdapter,
			    IN P_BSS_DESC_T prBssDesc,
			    IN P_STA_RECORD_T prStaRec, IN P_BSS_INFO_T prP2pBssInfo, IN P_SW_RFB_T prAssocRspSwRfb)
{
	P_WLAN_ASSOC_RSP_FRAME_T prAssocRspFrame = (P_WLAN_ASSOC_RSP_FRAME_T) NULL;
	UINT_16 u2IELength;
	PUINT_8 pucIE;

	DEBUGFUNC("p2pUpdateBssInfoForJOIN()");

	do {
		ASSERT_BREAK((prAdapter != NULL) &&
			     (prStaRec != NULL) && (prP2pBssInfo != NULL) && (prAssocRspSwRfb != NULL));

		prAssocRspFrame = (P_WLAN_ASSOC_RSP_FRAME_T) prAssocRspSwRfb->pvHeader;

		if (prBssDesc == NULL) {
			/* Target BSS NULL. */
			DBGLOG(P2P, TRACE, "Target BSS NULL\n");
			break;
		}

		DBGLOG(P2P, INFO, "Update P2P_BSS_INFO_T and apply settings to MAC\n");

		/* 3 <1> Update BSS_INFO_T from AIS_FSM_INFO_T or User Settings */
		/* 4 <1.1> Setup Operation Mode */
		ASSERT_BREAK(prP2pBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE);

		if (UNEQUAL_MAC_ADDR(prBssDesc->aucBSSID, prAssocRspFrame->aucBSSID))
			ASSERT(FALSE);
		/* 4 <1.2> Setup SSID */
		COPY_SSID(prP2pBssInfo->aucSSID, prP2pBssInfo->ucSSIDLen, prBssDesc->aucSSID, prBssDesc->ucSSIDLen);

		/* 4 <1.3> Setup Channel, Band */
		prP2pBssInfo->ucPrimaryChannel = prBssDesc->ucChannelNum;
		prP2pBssInfo->eBand = prBssDesc->eBand;

		/* 3 <2> Update BSS_INFO_T from STA_RECORD_T */
		/* 4 <2.1> Save current AP's STA_RECORD_T and current AID */
		prP2pBssInfo->prStaRecOfAP = prStaRec;
		prP2pBssInfo->u2AssocId = prStaRec->u2AssocId;

		/* 4 <2.2> Setup Capability */
		prP2pBssInfo->u2CapInfo = prStaRec->u2CapInfo;	/* Use AP's Cap Info as BSS Cap Info */

		if (prP2pBssInfo->u2CapInfo & CAP_INFO_SHORT_PREAMBLE)
			prP2pBssInfo->fgIsShortPreambleAllowed = TRUE;
		else
			prP2pBssInfo->fgIsShortPreambleAllowed = FALSE;

		/* 4 <2.3> Setup PHY Attributes and Basic Rate Set/Operational Rate Set */
		prP2pBssInfo->ucPhyTypeSet = prStaRec->ucDesiredPhyTypeSet;

		prP2pBssInfo->ucNonHTBasicPhyType = prStaRec->ucNonHTBasicPhyType;

		prP2pBssInfo->u2OperationalRateSet = prStaRec->u2OperationalRateSet;
		prP2pBssInfo->u2BSSBasicRateSet = prStaRec->u2BSSBasicRateSet;

		nicTxUpdateBssDefaultRate(prP2pBssInfo);

		/* 3 <3> Update BSS_INFO_T from SW_RFB_T (Association Resp Frame) */
		/* 4 <3.1> Setup BSSID */
		COPY_MAC_ADDR(prP2pBssInfo->aucBSSID, prAssocRspFrame->aucBSSID);

		u2IELength =
		    (UINT_16) ((prAssocRspSwRfb->u2PacketLen - prAssocRspSwRfb->u2HeaderLen) -
			       (OFFSET_OF(WLAN_ASSOC_RSP_FRAME_T, aucInfoElem[0]) - WLAN_MAC_MGMT_HEADER_LEN));
		pucIE = prAssocRspFrame->aucInfoElem;

		/* 4 <3.2> Parse WMM and setup QBSS flag */
		/* Parse WMM related IEs and configure HW CRs accordingly */
		mqmProcessAssocRsp(prAdapter, prAssocRspSwRfb, pucIE, u2IELength);

		prP2pBssInfo->fgIsQBSS = prStaRec->fgIsQoS;

		/* 3 <4> Update BSS_INFO_T from BSS_DESC_T */

		prBssDesc->fgIsConnecting = FALSE;
		prBssDesc->fgIsConnected = TRUE;

		/* 4 <4.1> Setup MIB for current BSS */
		prP2pBssInfo->u2BeaconInterval = prBssDesc->u2BeaconInterval;
		/* NOTE: Defer ucDTIMPeriod updating to when beacon is received after connection */
		prP2pBssInfo->ucDTIMPeriod = 0;
		prP2pBssInfo->u2ATIMWindow = 0;

		prP2pBssInfo->ucBeaconTimeoutCount = AIS_BEACON_TIMEOUT_COUNT_INFRA;

		/* 4 <4.2> Update HT information and set channel */
		/* Record HT related parameters in rStaRec and rBssInfo
		 * Note: it shall be called before nicUpdateBss()
		 */
		rlmProcessAssocRsp(prAdapter, prAssocRspSwRfb, pucIE, u2IELength);

		/* 4 <4.3> Sync with firmware for BSS-INFO */
		nicUpdateBss(prAdapter, prP2pBssInfo->ucBssIndex);

		/* 4 <4.4> *DEFER OPERATION* nicPmIndicateBssConnected() will be invoked */
		/* inside scanProcessBeaconAndProbeResp() after 1st beacon is received */

	} while (FALSE);
}				/* end of p2pUpdateBssInfoForJOIN() */

WLAN_STATUS
p2pFunMgmtFrameTxDone(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_RESULT_CODE_T rTxDoneStatus)
{
	BOOLEAN fgIsSuccess = FALSE;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsduInfo != NULL));

		if (rTxDoneStatus != TX_RESULT_SUCCESS) {
			DBGLOG(P2P, TRACE, "Mgmt Frame TX Fail, Status:%d.\n", rTxDoneStatus);
		} else {
			fgIsSuccess = TRUE;
			DBGLOG(P2P, TRACE, "Mgmt Frame TX Done.\n");
		}

		kalP2PIndicateMgmtTxStatus(prAdapter->prGlueInfo, prMsduInfo, fgIsSuccess);

	} while (FALSE);

	return WLAN_STATUS_SUCCESS;

}				/* p2pFunMgmtFrameTxDone */

WLAN_STATUS
p2pFuncTxMgmtFrame(IN P_ADAPTER_T prAdapter,
		   IN UINT_8 ucBssIndex, IN P_MSDU_INFO_T prMgmtTxMsdu, IN BOOLEAN fgNonCckRate)
{
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;
	/* P_MSDU_INFO_T prTxMsduInfo = (P_MSDU_INFO_T)NULL; */
	P_WLAN_MAC_HEADER_T prWlanHdr = (P_WLAN_MAC_HEADER_T) NULL;
	P_STA_RECORD_T prStaRec = (P_STA_RECORD_T) NULL;
	UINT_8 ucRetryLimit = 30;	/* TX_DESC_TX_COUNT_NO_LIMIT; */
	BOOLEAN fgDrop = FALSE;
	P_BSS_INFO_T prBssInfo;

	do {
		ASSERT_BREAK(prAdapter != NULL);

		/* Drop this frame if BSS inactive */
		if (!IS_NET_ACTIVE(prAdapter, ucBssIndex)) {
			p2pDevFsmRunEventMgmtFrameTxDone(prAdapter, prMgmtTxMsdu, TX_RESULT_DROPPED_IN_DRIVER);
			cnmMgtPktFree(prAdapter, prMgmtTxMsdu);
			fgDrop = TRUE;

			break;
		}

		prWlanHdr = (P_WLAN_MAC_HEADER_T) ((ULONG) prMgmtTxMsdu->prPacket + MAC_TX_RESERVED_FIELD);
		prStaRec = cnmGetStaRecByAddress(prAdapter, ucBssIndex, prWlanHdr->aucAddr1);
		/* prMgmtTxMsdu->ucBssIndex = ucBssIndex; */

		switch (prWlanHdr->u2FrameCtrl & MASK_FRAME_TYPE) {
		case MAC_FRAME_PROBE_RSP:
			DBGLOG(P2P, TRACE, "TX Probe Resposne Frame\n");
			prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);
			if ((!nicTxIsMgmtResourceEnough(prAdapter)) || (prBssInfo->fgIsNetAbsent)) {
				DBGLOG(P2P, TRACE, "Drop Tx probe response due to resource issue\n");
				fgDrop = TRUE;

				break;
			}
			prMgmtTxMsdu->ucStaRecIndex =
			    (prStaRec != NULL) ? (prStaRec->ucIndex) : (STA_REC_INDEX_NOT_FOUND);
			/* Modifiy Lie time to 100 mS due to the STA only wait 30-50mS */
			/* and AP do not need send it after STA left */
			nicTxSetPktLifeTime(prMgmtTxMsdu, 100);
			prMgmtTxMsdu = p2pFuncProcessP2pProbeRsp(prAdapter, ucBssIndex, prMgmtTxMsdu);
			ucRetryLimit = 6;
			break;
		default:
			prMgmtTxMsdu->ucBssIndex = ucBssIndex;
			break;
		}

		if (fgDrop) {
			/* Drop this frame */
			p2pDevFsmRunEventMgmtFrameTxDone(prAdapter, prMgmtTxMsdu, TX_RESULT_DROPPED_IN_DRIVER);
			cnmMgtPktFree(prAdapter, prMgmtTxMsdu);

			break;
		}

		TX_SET_MMPDU(prAdapter,
			     prMgmtTxMsdu,
			     prMgmtTxMsdu->ucBssIndex,
			     (prStaRec != NULL) ? (prStaRec->ucIndex) : (STA_REC_INDEX_NOT_FOUND),
			     WLAN_MAC_MGMT_HEADER_LEN,
			     prMgmtTxMsdu->u2FrameLength, p2pDevFsmRunEventMgmtFrameTxDone, MSDU_RATE_MODE_AUTO);

		nicTxSetPktRetryLimit(prMgmtTxMsdu, ucRetryLimit);

		nicTxConfigPktControlFlag(prMgmtTxMsdu, MSDU_CONTROL_FLAG_FORCE_TX, TRUE);

		nicTxEnqueueMsdu(prAdapter, prMgmtTxMsdu);

	} while (FALSE);

	return rWlanStatus;
}				/* p2pFuncTxMgmtFrame */

VOID p2pFuncStopComplete(IN P_ADAPTER_T prAdapter, IN P_BSS_INFO_T prP2pBssInfo)
{
	do {
		ASSERT_BREAK((prAdapter != NULL) && (prP2pBssInfo != NULL));

		DBGLOG(P2P, TRACE, "p2pFuncStopComplete\n");

		/* GO: It would stop Beacon TX. GC: Stop all BSS related PS function. */
		nicPmIndicateBssAbort(prAdapter, prP2pBssInfo->ucBssIndex);
		/* Reset RLM related field of BSSINFO. */
		rlmBssAborted(prAdapter, prP2pBssInfo);

		UNSET_NET_ACTIVE(prAdapter, prP2pBssInfo->ucBssIndex);
		nicDeactivateNetwork(prAdapter, prP2pBssInfo->ucBssIndex);
		/* Release CNM channel */
		nicUpdateBss(prAdapter, prP2pBssInfo->ucBssIndex);

		/* Reset current OPMode */
		/* 20170628, remove reset opmode, otherwise we cannot free P2P beacon buffer */
		/* prP2pBssInfo->eCurrentOPMode = OP_MODE_INFRASTRUCTURE; */

		/* Point StaRecOfAP to NULL when GC role stop Complete */
		prP2pBssInfo->prStaRecOfAP = NULL;
	} while (FALSE);

}				/* p2pFuncStopComplete */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will start a P2P Group Owner and send Beacon Frames.
*
* @param (none)
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID
p2pFuncStartGO(IN P_ADAPTER_T prAdapter,
	       IN P_BSS_INFO_T prBssInfo,
	       IN P_P2P_CONNECTION_REQ_INFO_T prP2pConnReqInfo, IN P_P2P_CHNL_REQ_INFO_T prP2pChnlReqInfo)
{
#if (CFG_SUPPORT_DFS_MASTER == 1)
	P_CMD_RDD_ON_OFF_CTRL_T prCmdRddOnOffCtrl;
#endif

#ifdef CFG_SUPPORT_P2P_OPEN_SECURITY
		BOOLEAN fgIsOpenP2P = TRUE;
#else
		BOOLEAN fgIsOpenP2P = FALSE;
#endif

#ifdef CFG_SUPPORT_P2P_GO_11B_RATE
		BOOLEAN fgIs11bRate = TRUE;
#else
		BOOLEAN fgIs11bRate = FALSE;
#endif

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prBssInfo != NULL));

		if (prBssInfo->ucBssIndex >= MAX_BSS_INDEX) {
			DBGLOG(P2P, ERROR, "P2P BSS exceed the number of P2P interface number.");
			ASSERT(FALSE);
			break;
		}

		DBGLOG(P2P, TRACE, "p2pFuncStartGO:\n");

#if (CFG_SUPPORT_DFS_MASTER == 1)
		prCmdRddOnOffCtrl = (P_CMD_RDD_ON_OFF_CTRL_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
						sizeof(*prCmdRddOnOffCtrl));

		if (prCmdRddOnOffCtrl == NULL) {
			DBGLOG(P2P, ERROR, "Allocate memory for prCmdRddOnOffCtrl failed.");
			return;
		}

		prCmdRddOnOffCtrl->ucDfsCtrl = RDD_START_TXQ;
		prCmdRddOnOffCtrl->ucRddIdx = prAdapter->
				aprBssInfo[prBssInfo->ucBssIndex]->eDBDCBand;

		DBGLOG(P2P, INFO, "p2pFuncStartGO: Start TXQ - DFS ctrl: %.d\n", prCmdRddOnOffCtrl->ucDfsCtrl);

		wlanSendSetQueryCmd(prAdapter,
					CMD_ID_RDD_ON_OFF_CTRL,
					TRUE,
					FALSE,
					FALSE,
					NULL,
					NULL,
					sizeof(*prCmdRddOnOffCtrl), (PUINT_8) prCmdRddOnOffCtrl, NULL, 0);

		cnmMemFree(prAdapter, prCmdRddOnOffCtrl);
#endif

		/* Re-start AP mode.  */
		p2pFuncSwitchOPMode(prAdapter, prBssInfo, prBssInfo->eIntendOPMode, FALSE);

		prBssInfo->eIntendOPMode = OP_MODE_NUM;

		/* 4 <1.1> Assign SSID */
		COPY_SSID(prBssInfo->aucSSID,
			  prBssInfo->ucSSIDLen,
			  prP2pConnReqInfo->rSsidStruct.aucSsid, prP2pConnReqInfo->rSsidStruct.ucSsidLen);

		DBGLOG(P2P, TRACE, "GO SSID:%s\n", prBssInfo->aucSSID);

		/* 4 <1.2> Clear current AP's STA_RECORD_T and current AID */
		prBssInfo->prStaRecOfAP = (P_STA_RECORD_T) NULL;
		prBssInfo->u2AssocId = 0;

		/* 4 <1.3> Setup Channel, Band and Phy Attributes */
		prBssInfo->ucPrimaryChannel = prP2pChnlReqInfo->ucReqChnlNum;
		prBssInfo->eBand = prP2pChnlReqInfo->eBand;
		prBssInfo->eBssSCO = prP2pChnlReqInfo->eChnlSco;

		DBGLOG(P2P, TRACE, "GO Channel:%d\n", prBssInfo->ucPrimaryChannel);

		if (prBssInfo->eBand == BAND_5G) {
			/* Depend on eBand */
			prBssInfo->ucPhyTypeSet = (prAdapter->rWifiVar.ucAvailablePhyTypeSet & PHY_TYPE_SET_802_11AN);
			prBssInfo->ucConfigAdHocAPMode = AP_MODE_11A;	/* Depend on eCurrentOPMode and ucPhyTypeSet */
		} else if ((prP2pConnReqInfo->eConnRequest == P2P_CONNECTION_TYPE_PURE_AP) ||
			   fgIs11bRate) {
			/* Depend on eBand */
			prBssInfo->ucPhyTypeSet = (prAdapter->rWifiVar.ucAvailablePhyTypeSet & PHY_TYPE_SET_802_11BGN);
			/* Depend on eCurrentOPMode and ucPhyTypeSet */
			prBssInfo->ucConfigAdHocAPMode = AP_MODE_MIXED_11BG;
		} else {
			ASSERT(prP2pConnReqInfo->eConnRequest == P2P_CONNECTION_TYPE_GO);
			/* Depend on eBand */
			prBssInfo->ucPhyTypeSet = (prAdapter->rWifiVar.ucAvailablePhyTypeSet & PHY_TYPE_SET_802_11GN);
			/* Depend on eCurrentOPMode and ucPhyTypeSet */
			prBssInfo->ucConfigAdHocAPMode = AP_MODE_11G_P2P;
		}

		/* Overwrite BSS PHY type set by Feature Options */
		bssDetermineApBssInfoPhyTypeSet(prAdapter,
						(prP2pConnReqInfo->eConnRequest ==
						 P2P_CONNECTION_TYPE_PURE_AP) ? TRUE : FALSE, prBssInfo);

		prBssInfo->ucNonHTBasicPhyType = (UINT_8)
		    rNonHTApModeAttributes[prBssInfo->ucConfigAdHocAPMode].ePhyTypeIndex;
		prBssInfo->u2BSSBasicRateSet = rNonHTApModeAttributes[prBssInfo->ucConfigAdHocAPMode].u2BSSBasicRateSet;
		prBssInfo->u2OperationalRateSet =
		    rNonHTPhyAttributes[prBssInfo->ucNonHTBasicPhyType].u2SupportedRateSet;

		if ((prBssInfo->ucAllSupportedRatesLen == 0) || fgIs11bRate) {
			rateGetDataRatesFromRateSet(prBssInfo->u2OperationalRateSet,
						    prBssInfo->u2BSSBasicRateSet,
						    prBssInfo->aucAllSupportedRates,
						    &prBssInfo->ucAllSupportedRatesLen);
		}
		/* 4 <1.5> Setup MIB for current BSS */
		prBssInfo->u2ATIMWindow = 0;
		prBssInfo->ucBeaconTimeoutCount = 0;

		/* 3 <2> Update BSS_INFO_T common part */
#if CFG_SUPPORT_AAA
		prBssInfo->fgIsProtection = FALSE;
		/* Always enable protection at P2P GO But OOBE AP */
		if ((prP2pConnReqInfo->eConnRequest == P2P_CONNECTION_TYPE_GO) && (!fgIsOpenP2P)) {
			prBssInfo->fgIsProtection = TRUE;
		} else {
			if (!fgIsOpenP2P)
				ASSERT(prP2pConnReqInfo->eConnRequest == P2P_CONNECTION_TYPE_PURE_AP);
			if (kalP2PGetCipher(prAdapter->prGlueInfo, (UINT_8) prBssInfo->u4PrivateData))
				prBssInfo->fgIsProtection = TRUE;
		}

		bssInitForAP(prAdapter, prBssInfo, TRUE);

#if 0
		if (prBssInfo->ucBMCWlanIndex >= WTBL_SIZE) {
			prBssInfo->ucBMCWlanIndex =
			    secPrivacySeekForBcEntry(prAdapter, prBssInfo->ucBssIndex,
						     prBssInfo->aucBSSID, 0xff, CIPHER_SUITE_NONE, 0xff);
		}
#endif
		nicQmUpdateWmmParms(prAdapter, prBssInfo->ucBssIndex);
#endif /* CFG_SUPPORT_AAA */

		/* 3 <3> Set MAC HW */
		/* 4 <3.1> Setup channel and bandwidth */
		rlmBssInitForAPandIbss(prAdapter, prBssInfo);

		/* 4 <3.2> Reset HW TSF Update Mode and Beacon Mode */
		nicUpdateBss(prAdapter, prBssInfo->ucBssIndex);

		/* 4 <3.3> Update Beacon again for network phy type confirmed. */
		bssUpdateBeaconContent(prAdapter, prBssInfo->ucBssIndex);

		/* 4 <3.4> Setup BSSID */
		nicPmIndicateBssCreated(prAdapter, prBssInfo->ucBssIndex);

	} while (FALSE);
}				/* p2pFuncStartGO() */

VOID p2pFuncStopGO(IN P_ADAPTER_T prAdapter, IN P_BSS_INFO_T prP2pBssInfo)
{
	UINT_32 u4ClientCount = 0;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prP2pBssInfo != NULL));

		DBGLOG(P2P, TRACE, "p2pFuncStopGO\n");

		u4ClientCount = bssGetClientCount(prAdapter, prP2pBssInfo);

		if ((prP2pBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT)
		    && (prP2pBssInfo->eIntendOPMode == OP_MODE_NUM)) {
			/* AP is created, Beacon Updated. */
			p2pFuncDissolve(prAdapter, prP2pBssInfo, TRUE, REASON_CODE_DEAUTH_LEAVING_BSS);
			prP2pBssInfo->eIntendOPMode = OP_MODE_P2P_DEVICE;
		}

		/* Do not Deactivate Network if any Client existed, we'll deactive it after Deauth Tx done */
		if (u4ClientCount == 0) {
			DBGLOG(P2P, INFO, "No client! Deactive GO immediately.\n");
			p2pChangeMediaState(prAdapter, prP2pBssInfo, PARAM_MEDIA_STATE_DISCONNECTED);
			p2pFuncStopComplete(prAdapter, prP2pBssInfo);
		}

	} while (FALSE);

}				/* p2pFuncStopGO */

WLAN_STATUS p2pFuncRoleToBssIdx(IN P_ADAPTER_T prAdapter, IN UINT_8 ucRoleIdx, OUT PUINT_8 pucBssIdx)
{
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (pucBssIdx != NULL));

		if (ucRoleIdx >= BSS_P2P_NUM) {
			rWlanStatus = WLAN_STATUS_FAILURE;
			break;
		}
		if (!prAdapter->rWifiVar.aprP2pRoleFsmInfo[ucRoleIdx]) {
			DBGLOG(P2P, WARN, "%s, invalid aprP2pRoleFsmInfo, ignore\n", __func__);
			rWlanStatus = WLAN_STATUS_FAILURE;
		} else
			*pucBssIdx = prAdapter->rWifiVar.aprP2pRoleFsmInfo[ucRoleIdx]->ucBssIndex;

	} while (FALSE);

	return rWlanStatus;
}				/* p2pFuncRoleToBssIdx */

P_P2P_ROLE_FSM_INFO_T p2pFuncGetRoleByBssIdx(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex)
{
	INT_32 i = 0;
	P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo = (P_P2P_ROLE_FSM_INFO_T)NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL));

		for (i = 0 ; i < BSS_P2P_NUM; i++) {
			if (!prAdapter->rWifiVar.aprP2pRoleFsmInfo[i])
				continue;

			if (prAdapter->rWifiVar.aprP2pRoleFsmInfo[i]->ucBssIndex == ucBssIndex)
				break;
		}
		if (i < BSS_P2P_NUM)
			prP2pRoleFsmInfo = prAdapter->rWifiVar.aprP2pRoleFsmInfo[i];

	} while (FALSE);

	return prP2pRoleFsmInfo;
}

/* /////////////////////////////////   MT6630 CODE END //////////////////////////////////////////////// */

VOID
p2pFuncSwitchOPMode(IN P_ADAPTER_T prAdapter,
		    IN P_BSS_INFO_T prP2pBssInfo, IN ENUM_OP_MODE_T eOpMode, IN BOOLEAN fgSyncToFW)
{
	do {
		ASSERT_BREAK((prAdapter != NULL) && (prP2pBssInfo != NULL) && (eOpMode < OP_MODE_NUM));

		if (prP2pBssInfo->eCurrentOPMode != eOpMode) {
			DBGLOG(P2P, TRACE,
			       "p2pFuncSwitchOPMode: Switch to from %d, to %d.\n",
			       prP2pBssInfo->eCurrentOPMode, eOpMode);

			switch (prP2pBssInfo->eCurrentOPMode) {
			case OP_MODE_ACCESS_POINT:
				/* p2pFuncDissolve will be done in p2pFuncStopGO(). */
				/* p2pFuncDissolve(prAdapter, prP2pBssInfo, TRUE, REASON_CODE_DEAUTH_LEAVING_BSS); */
				if (prP2pBssInfo->eIntendOPMode != OP_MODE_P2P_DEVICE) {
					p2pFuncStopGO(prAdapter, prP2pBssInfo);

					SET_NET_PWR_STATE_IDLE(prAdapter, prP2pBssInfo->ucBssIndex);
				}
				break;
			default:
				break;
			}

			prP2pBssInfo->eIntendOPMode = eOpMode;

			/* The state is in disconnecting and can not change any BSS status */
			if (IS_NET_PWR_STATE_IDLE(prAdapter, prP2pBssInfo->ucBssIndex) &&
				IS_NET_ACTIVE(prAdapter, prP2pBssInfo->ucBssIndex)) {
				DBGLOG(P2P, TRACE, "under deauth procedure, Quit.\n");
				break;
			}

			prP2pBssInfo->eCurrentOPMode = eOpMode;
			switch (eOpMode) {
			case OP_MODE_INFRASTRUCTURE:
				DBGLOG(P2P, TRACE, "p2pFuncSwitchOPMode: Switch to Client.\n");
				/* fall through */
			case OP_MODE_ACCESS_POINT:
				/* Change interface address. */
				if (eOpMode == OP_MODE_ACCESS_POINT) {
					DBGLOG(P2P, TRACE, "p2pFuncSwitchOPMode: Switch to AP.\n");
					prP2pBssInfo->ucSSIDLen = 0;
				}

#if CFG_DUAL_P2PLIKE_INTERFACE
				/*avoid ap1 Bss  have diff A2 & A3, */
				/*ToDo :  fix for P2P case*/

#else
				COPY_MAC_ADDR(prP2pBssInfo->aucOwnMacAddr, prAdapter->rWifiVar.aucInterfaceAddress);
				COPY_MAC_ADDR(prP2pBssInfo->aucBSSID, prAdapter->rWifiVar.aucInterfaceAddress);
#endif
				break;
			case OP_MODE_P2P_DEVICE:
				{
					/* Change device address. */
					DBGLOG(P2P, TRACE, "p2pFuncSwitchOPMode: Switch back to P2P Device.\n");

					p2pChangeMediaState(prAdapter, prP2pBssInfo, PARAM_MEDIA_STATE_DISCONNECTED);

					COPY_MAC_ADDR(prP2pBssInfo->aucOwnMacAddr,
						      prAdapter->rWifiVar.aucDeviceAddress);
					COPY_MAC_ADDR(prP2pBssInfo->aucBSSID, prAdapter->rWifiVar.aucDeviceAddress);

				}
				break;
			default:
				ASSERT(FALSE);
				break;
			}

			if (1) {
				P2P_DISCONNECT_INFO rP2PDisInfo;

				rP2PDisInfo.ucRole = 2;
				wlanSendSetQueryCmd(prAdapter,
						    CMD_ID_P2P_ABORT,
						    TRUE,
						    FALSE,
						    FALSE,
						    NULL,
						    NULL,
						    sizeof(P2P_DISCONNECT_INFO), (PUINT_8) &rP2PDisInfo, NULL, 0);
			}

			DBGLOG(P2P, TRACE,
			       "The device address is changed to " MACSTR "\n", MAC2STR(prP2pBssInfo->aucOwnMacAddr));
			DBGLOG(P2P, TRACE, "The BSSID is changed to " MACSTR "\n", MAC2STR(prP2pBssInfo->aucBSSID));

			/* Update BSS INFO to FW. */
			if ((fgSyncToFW) && (eOpMode != OP_MODE_ACCESS_POINT))
				nicUpdateBss(prAdapter, prP2pBssInfo->ucBssIndex);
		}

	} while (FALSE);
}				/* p2pFuncSwitchOPMode */

/*----------------------------------------------------------------------------*/
/*!
* \brief    This function is to inform CNM that channel privilege
*           has been released
*
* \param[in] prAdapter  Pointer of ADAPTER_T
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID p2pFuncReleaseCh(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIdx, IN P_P2P_CHNL_REQ_INFO_T prChnlReqInfo)
{
	P_MSG_CH_ABORT_T prMsgChRelease = (P_MSG_CH_ABORT_T) NULL;

	DEBUGFUNC("p2pFuncReleaseCh()");

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prChnlReqInfo != NULL));

		if (!prChnlReqInfo->fgIsChannelRequested)
			break;
		DBGLOG(P2P, TRACE, "P2P Release Channel\n");
		prChnlReqInfo->fgIsChannelRequested = FALSE;

		/* 1. return channel privilege to CNM immediately */
		prMsgChRelease = (P_MSG_CH_ABORT_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_CH_ABORT_T));
		if (!prMsgChRelease) {
			ASSERT(0);	/* Can't release Channel to CNM */
			break;
		}

		prMsgChRelease->rMsgHdr.eMsgId = MID_MNY_CNM_CH_ABORT;
		prMsgChRelease->ucBssIndex = ucBssIdx;
		prMsgChRelease->ucTokenID = prChnlReqInfo->ucSeqNumOfChReq++;
#if CFG_SUPPORT_DBDC
		prMsgChRelease->eDBDCBand = ENUM_BAND_AUTO;

		DBGLOG(P2P, INFO,
			"p2pFuncReleaseCh: P2P abort channel on band %u.\n",
			prMsgChRelease->eDBDCBand);
#endif /*CFG_SUPPORT_DBDC*/
		mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prMsgChRelease, MSG_SEND_METHOD_BUF);

	} while (FALSE);
}				/* p2pFuncReleaseCh */

/*----------------------------------------------------------------------------*/
/*!
* @brief Process of CHANNEL_REQ_JOIN Initial. Enter CHANNEL_REQ_JOIN State.
*
* @param (none)
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID p2pFuncAcquireCh(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIdx, IN P_P2P_CHNL_REQ_INFO_T prChnlReqInfo)
{
	P_MSG_CH_REQ_T prMsgChReq = (P_MSG_CH_REQ_T) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prChnlReqInfo != NULL));

		p2pFuncReleaseCh(prAdapter, ucBssIdx, prChnlReqInfo);

		/* send message to CNM for acquiring channel */
		prMsgChReq = (P_MSG_CH_REQ_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_CH_REQ_T));

		if (!prMsgChReq) {
			ASSERT(0);	/* Can't indicate CNM for channel acquiring */
			break;
		}

		prMsgChReq->rMsgHdr.eMsgId = MID_MNY_CNM_CH_REQ;
		prMsgChReq->ucBssIndex = ucBssIdx;
		prMsgChReq->ucTokenID = ++prChnlReqInfo->ucSeqNumOfChReq;
		prMsgChReq->eReqType = prChnlReqInfo->eChnlReqType;
		prMsgChReq->u4MaxInterval = prChnlReqInfo->u4MaxInterval;
		prMsgChReq->ucPrimaryChannel = prChnlReqInfo->ucReqChnlNum;
		prMsgChReq->eRfSco = prChnlReqInfo->eChnlSco;
		prMsgChReq->eRfBand = prChnlReqInfo->eBand;
		prMsgChReq->eRfChannelWidth = prChnlReqInfo->eChannelWidth;
		prMsgChReq->ucRfCenterFreqSeg1 = prChnlReqInfo->ucCenterFreqS1;
		prMsgChReq->ucRfCenterFreqSeg2 = prChnlReqInfo->ucCenterFreqS2;
#if CFG_SUPPORT_DBDC
		prMsgChReq->eDBDCBand = ENUM_BAND_AUTO;

		DBGLOG(P2P, INFO,
		   "p2pFuncAcquireCh: P2P Request channel on band %u.\n",
		   prMsgChReq->eDBDCBand);
#endif /*CFG_SUPPORT_DBDC*/
		/* Channel request join BSSID. */

		mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prMsgChReq, MSG_SEND_METHOD_BUF);

		prChnlReqInfo->fgIsChannelRequested = TRUE;

	} while (FALSE);
}				/* p2pFuncAcquireCh */

#if (CFG_SUPPORT_DFS_MASTER == 1)
VOID p2pFuncStartRdd(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIdx)
{
	P_CMD_RDD_ON_OFF_CTRL_T prCmdRddOnOffCtrl;
	P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo = (P_P2P_ROLE_FSM_INFO_T) NULL;
	UINT_8 ucReqChnlNum;

	DEBUGFUNC("p2pFuncStartRdd()");


	prP2pRoleFsmInfo = P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
			prAdapter->aprBssInfo[ucBssIdx]->u4PrivateData);

	ucReqChnlNum = prP2pRoleFsmInfo->rChnlReqInfo.ucReqChnlNum;

	prCmdRddOnOffCtrl = (P_CMD_RDD_ON_OFF_CTRL_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
				sizeof(*prCmdRddOnOffCtrl));

	if (!prCmdRddOnOffCtrl) {
		DBGLOG(P2P, ERROR, "cnmMemAlloc for prCmdRddOnOffCtrl failed!\n");
		return;
	}

	prCmdRddOnOffCtrl->ucDfsCtrl = RDD_START;

	prCmdRddOnOffCtrl->ucRddIdx = prAdapter->aprBssInfo[ucBssIdx]->eDBDCBand;

	if (rlmDomainGetDfsRegion() == NL80211_DFS_JP) {
		if (ucReqChnlNum >= 52 && ucReqChnlNum <= 64)
			prCmdRddOnOffCtrl->ucRegDomain = REG_JP_53;
		else if (ucReqChnlNum >= 100 && ucReqChnlNum <= 140)
			prCmdRddOnOffCtrl->ucRegDomain = REG_JP_56;
	} else {
		prCmdRddOnOffCtrl->ucRegDomain = REG_DEFAULT;
	}

	if (prCmdRddOnOffCtrl->ucRddIdx)
		prCmdRddOnOffCtrl->ucRddInSel = RDD_IN_SEL_1;
	else
		prCmdRddOnOffCtrl->ucRddInSel = RDD_IN_SEL_0;

	DBGLOG(P2P, INFO, "p2pFuncStartRdd: Start Radar detection - DFS ctrl: %d, RDD index: %d\n",
			prCmdRddOnOffCtrl->ucDfsCtrl, prCmdRddOnOffCtrl->ucRddIdx);

	wlanSendSetQueryCmd(prAdapter,
				CMD_ID_RDD_ON_OFF_CTRL,
				TRUE,
				FALSE,
				FALSE,
				NULL,
				NULL,
				sizeof(*prCmdRddOnOffCtrl), (PUINT_8) prCmdRddOnOffCtrl, NULL, 0);

	cnmMemFree(prAdapter, prCmdRddOnOffCtrl);
}				/* p2pFuncStartRdd */

VOID p2pFuncStopRdd(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIdx)
{
	P_CMD_RDD_ON_OFF_CTRL_T prCmdRddOnOffCtrl;

	DEBUGFUNC("p2pFuncStopRdd()");

	prCmdRddOnOffCtrl = (P_CMD_RDD_ON_OFF_CTRL_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
				sizeof(*prCmdRddOnOffCtrl));

	if (!prCmdRddOnOffCtrl) {
		DBGLOG(P2P, ERROR, "cnmMemAlloc for prCmdRddOnOffCtrl failed!\n");
		return;
	}

	prCmdRddOnOffCtrl->ucDfsCtrl = RDD_STOP;

	prCmdRddOnOffCtrl->ucRddIdx = prAdapter->aprBssInfo[ucBssIdx]->eDBDCBand;

	if (prCmdRddOnOffCtrl->ucRddIdx)
		prCmdRddOnOffCtrl->ucRddInSel = RDD_IN_SEL_1;
	else
		prCmdRddOnOffCtrl->ucRddInSel = RDD_IN_SEL_0;

	DBGLOG(P2P, INFO, "p2pFuncStopRdd: Stop Radar detection - DFS ctrl: %d, RDD index: %d\n",
			prCmdRddOnOffCtrl->ucDfsCtrl, prCmdRddOnOffCtrl->ucRddIdx);

	wlanSendSetQueryCmd(prAdapter,
				CMD_ID_RDD_ON_OFF_CTRL,
				TRUE,
				FALSE,
				FALSE,
				NULL,
				NULL,
				sizeof(*prCmdRddOnOffCtrl), (PUINT_8) prCmdRddOnOffCtrl, NULL, 0);

	cnmMemFree(prAdapter, prCmdRddOnOffCtrl);

}				/* p2pFuncStopRdd */

VOID p2pFuncDfsSwitchCh(IN P_ADAPTER_T prAdapter, IN P_BSS_INFO_T prBssInfo, IN P2P_CHNL_REQ_INFO_T rP2pChnlReqInfo)
{

	P_GLUE_INFO_T prGlueInfo;
	P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo = (P_P2P_ROLE_FSM_INFO_T) NULL;
	P_CMD_RDD_ON_OFF_CTRL_T prCmdRddOnOffCtrl;
	UINT_8 role_idx;

	DEBUGFUNC("p2pFuncDfsSwitchCh()");

	if (!prBssInfo) {
		DBGLOG(P2P, ERROR, "prBssInfo shouldn't be NULL!\n");
		return;
	}

	/*  Setup Channel, Band */
	prBssInfo->ucPrimaryChannel = rP2pChnlReqInfo.ucReqChnlNum;
	prBssInfo->eBand = rP2pChnlReqInfo.eBand;
	prBssInfo->eBssSCO = rP2pChnlReqInfo.eChnlSco;

	/* Setup channel and bandwidth */
	rlmBssInitForAPandIbss(prAdapter, prBssInfo);

	/* Update Beacon again for network phy type confirmed. */
	bssUpdateBeaconContent(prAdapter, prBssInfo->ucBssIndex);

	/* Reset HW TSF Update Mode and Beacon Mode */
	nicUpdateBss(prAdapter, prBssInfo->ucBssIndex);

	prCmdRddOnOffCtrl = (P_CMD_RDD_ON_OFF_CTRL_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
					sizeof(*prCmdRddOnOffCtrl));

	if (!prCmdRddOnOffCtrl) {
		DBGLOG(P2P, ERROR, "cnmMemAlloc for prCmdRddOnOffCtrl failed!\n");
		return;
	}

	prCmdRddOnOffCtrl->ucDfsCtrl = RDD_START_TXQ;
	prCmdRddOnOffCtrl->ucRddIdx = prAdapter->
				aprBssInfo[prBssInfo->ucBssIndex]->eDBDCBand;

	DBGLOG(P2P, INFO, "p2pFuncDfsSwitchCh: Start TXQ - DFS ctrl: %.d\n", prCmdRddOnOffCtrl->ucDfsCtrl);

	wlanSendSetQueryCmd(prAdapter,
				CMD_ID_RDD_ON_OFF_CTRL,
				TRUE,
				FALSE,
				FALSE,
				NULL,
				NULL,
				sizeof(*prCmdRddOnOffCtrl), (PUINT_8) prCmdRddOnOffCtrl, NULL, 0);

	cnmMemFree(prAdapter, prCmdRddOnOffCtrl);

	prP2pRoleFsmInfo = P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter, prBssInfo->u4PrivateData);

	prGlueInfo = prAdapter->prGlueInfo;

	DBGLOG(P2P, INFO, "p2pFuncDfsSwitchCh: Update to OS\n");

	role_idx = prP2pRoleFsmInfo->ucRoleIndex;
	/* NL80211 event should send to p2p group netdevice.
	 * Otherwise wpa_supplicant wouldn't perform beacon update.
	 * Hostapd case: prDevHandler same with aprRoleHandler
	 * P2P GO case: p2p0=>prDevHandler, p2p-xxx-x=> aprRoleHandler
	 */
	cfg80211_ch_switch_notify(
		prGlueInfo->prP2PInfo[role_idx]->aprRoleHandler,
		prGlueInfo->prP2PInfo[role_idx]->chandef);
	DBGLOG(P2P, INFO, "p2pFuncDfsSwitchCh: Update to OS Done\n");

} /* p2pFuncDfsSwitchCh */

BOOLEAN p2pFuncCheckWeatherRadarBand(IN P_P2P_CHNL_REQ_INFO_T prChnlReqInfo)
{
	UINT_8 ucReqChnlNum;
	UINT_8 ucCenterFreqS1;
	ENUM_CHANNEL_WIDTH_T eChannelWidth;
	ENUM_CHNL_EXT_T eChnlSco;


	ucReqChnlNum = prChnlReqInfo->ucReqChnlNum;
	ucCenterFreqS1 = prChnlReqInfo->ucCenterFreqS1;
	eChannelWidth = prChnlReqInfo->eChannelWidth;
	eChnlSco = prChnlReqInfo->eChnlSco;

	if (rlmDomainGetDfsRegion() == NL80211_DFS_ETSI) {
		if (eChannelWidth == VHT_OP_CHANNEL_WIDTH_80) {
			if (ucCenterFreqS1 >= 120 && ucCenterFreqS1 <= 128)
				return TRUE;
		} else {
			if ((ucReqChnlNum >= 120 && ucReqChnlNum <= 128))
				return TRUE;
			else if (ucReqChnlNum == 116 && eChnlSco == CHNL_EXT_SCA) /* ch116, 120 BW40 */
				return TRUE;
		}
	}

	return FALSE;
}

INT_32 p2pFuncSetDriverCacTime(IN UINT_32 u4CacTime)
{
	WLAN_STATUS i4Status = WLAN_STATUS_SUCCESS;

	g_u4DriverCacTime = u4CacTime;

	DBGLOG(P2P, INFO, "p2pFuncSetDriverCacTime: g_u4ManualCacTime = %dsec\n", g_u4DriverCacTime);

	return i4Status;
}

VOID p2pFuncEnableManualCac(VOID)
{
	g_fgManualCac = TRUE;
}

UINT_32 p2pFuncGetDriverCacTime(VOID)
{
	return g_u4DriverCacTime;
}

BOOLEAN p2pFuncIsManualCac(VOID)
{
	return g_fgManualCac;
}

VOID p2pFuncRadarInfoInit(VOID)
{
	kalMemZero(&g_rP2pRadarInfo, sizeof(g_rP2pRadarInfo));
}

VOID p2pFuncShowRadarInfo(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIdx)
{
	UINT_8 ucCnt = 0;
	P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo = (P_P2P_ROLE_FSM_INFO_T) NULL;
	UINT_8 ucReqChnlNum;

	if (g_rP2pRadarInfo.ucRadarReportMode == 1) {

		prP2pRoleFsmInfo = P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
				prAdapter->aprBssInfo[ucBssIdx]->u4PrivateData);

		ucReqChnlNum = prP2pRoleFsmInfo->rChnlReqInfo.ucReqChnlNum;

		DBGLOG(P2P, INFO, "-----Radar Detected Event-----\n");
		DBGLOG(P2P, INFO, "Radar detected in DBDC band%d\n", g_rP2pRadarInfo.ucRddIdx);

		switch (rlmDomainGetDfsRegion()) {
		case NL80211_DFS_FCC:
			DBGLOG(P2P, INFO, "Regulation domain: FCC\n");
			break;
		case NL80211_DFS_ETSI:
			DBGLOG(P2P, INFO, "Regulation domain: ETSI\n");
			break;
		case NL80211_DFS_JP:
			DBGLOG(P2P, INFO, "Regulation domain: JP\n");

			if (ucReqChnlNum >= 52 && ucReqChnlNum <= 64)
				DBGLOG(P2P, INFO, "Radar type: W53 - %s\n", p2pFuncJpW53RadarType());
			else if (ucReqChnlNum >= 100 && ucReqChnlNum <= 140)
				DBGLOG(P2P, INFO, "Radar type: W56 - %s\n", p2pFuncJpW56RadarType());
			break;
		default:
			break;
		}

		DBGLOG(P2P, INFO, "Radar Content:\n");

		DBGLOG(P2P, INFO, "start time    pulse width    PRI\n");

		if (g_rP2pRadarInfo.ucPeriodicDetected) {
			DBGLOG(P2P, INFO, "%-10d    %-11d    -\n"
				, g_rP2pRadarInfo.arPpbContent[ucCnt].u4PeriodicStartTime
				, g_rP2pRadarInfo.arPpbContent[ucCnt].u2PeriodicPulseWidth);

			for (ucCnt = 1; ucCnt < g_rP2pRadarInfo.ucPPBNum; ucCnt++) {
				DBGLOG(P2P, INFO, "%-10d    %-11d    %d\n"
					, g_rP2pRadarInfo.arPpbContent[ucCnt].u4PeriodicStartTime
					, g_rP2pRadarInfo.arPpbContent[ucCnt].u2PeriodicPulseWidth
					, (g_rP2pRadarInfo.arPpbContent[ucCnt].u4PeriodicStartTime
						- g_rP2pRadarInfo.arPpbContent[ucCnt-1].u4PeriodicStartTime) * 2 / 5);
			}
		} else if (g_rP2pRadarInfo.ucLongDetected) {
			DBGLOG(P2P, INFO, "%-10d    %-11d    -\n"
				, g_rP2pRadarInfo.arLpbContent[ucCnt].u4LongStartTime
				, g_rP2pRadarInfo.arLpbContent[ucCnt].u2LongPulseWidth);

			for (ucCnt = 1; ucCnt < g_rP2pRadarInfo.ucLPBNum; ucCnt++) {
				DBGLOG(P2P, INFO, "%-10d    %-11d    %d\n"
					, g_rP2pRadarInfo.arLpbContent[ucCnt].u4LongStartTime
					, g_rP2pRadarInfo.arLpbContent[ucCnt].u2LongPulseWidth
					, (g_rP2pRadarInfo.arLpbContent[ucCnt].u4LongStartTime
						- g_rP2pRadarInfo.arLpbContent[ucCnt-1].u4LongStartTime) * 2 / 5);
			}
		}
	}
}

VOID p2pFuncGetRadarInfo(IN struct P2P_RADAR_INFO *prP2pRadarInfo)
{
	kalMemCopy(prP2pRadarInfo, &g_rP2pRadarInfo, sizeof(*prP2pRadarInfo));
}

PUINT_8 p2pFuncJpW53RadarType(VOID)
{
	UINT_32 u4Type1Diff;
	UINT_32 u4Type2Diff;

	if (g_rP2pRadarInfo.u4PRI1stUs >= 1428)
		u4Type1Diff = g_rP2pRadarInfo.u4PRI1stUs - 1428;
	else
		u4Type1Diff = 1428 - g_rP2pRadarInfo.u4PRI1stUs;

	if (g_rP2pRadarInfo.u4PRI1stUs >= 3846)
		u4Type2Diff = g_rP2pRadarInfo.u4PRI1stUs - 3846;
	else
		u4Type2Diff = 3846 - g_rP2pRadarInfo.u4PRI1stUs;

	if (u4Type1Diff < u4Type2Diff)
		return apucW53RadarType[1];
	else
		return apucW53RadarType[2];
}

PUINT_8 p2pFuncJpW56RadarType(VOID)
{
	UINT_32 u4Type1Diff;
	UINT_32 u4Type2Diff;

	if (g_rP2pRadarInfo.ucLongDetected)
		return apucW56RadarType[7];

	if (g_rP2pRadarInfo.u4PRI1stUs >= 3980 && g_rP2pRadarInfo.u4PRI1stUs <= 4020)
		return apucW56RadarType[3];

	if (g_rP2pRadarInfo.u4PRI1stUs >= 1368 && g_rP2pRadarInfo.u4PRI1stUs <= 1448) {

		if (g_rP2pRadarInfo.u4PRI1stUs >= 1388)
			u4Type1Diff = g_rP2pRadarInfo.u4PRI1stUs - 1388;
		else
			u4Type1Diff = 1388 - g_rP2pRadarInfo.u4PRI1stUs;

		if (g_rP2pRadarInfo.u4PRI1stUs >= 1428)
			u4Type2Diff = g_rP2pRadarInfo.u4PRI1stUs - 1428;
		else
			u4Type2Diff = 1428 - g_rP2pRadarInfo.u4PRI1stUs;

		if (u4Type1Diff < u4Type2Diff)
			return apucW56RadarType[1];
		else
			return apucW56RadarType[2];

	}

	if (g_rP2pRadarInfo.u4PRI1stUs >= 130 && g_rP2pRadarInfo.u4PRI1stUs < 200)
		return apucW56RadarType[4];

	if (g_rP2pRadarInfo.u4PRI1stUs >= 200 && g_rP2pRadarInfo.u4PRI1stUs <= 520) {

		if (g_rP2pRadarInfo.u4PRI1stUs <= 230)
		return apucW56RadarType[9];

		if (g_rP2pRadarInfo.u4PRI1stUs >= 323 && g_rP2pRadarInfo.u4PRI1stUs <= 343)
			return apucW56RadarType[10];

		return apucW56RadarType[11];
	}

	return apucW56RadarType[0];
}

VOID p2pFuncSetRadarDetectMode(IN UINT_8 ucRadarDetectMode)
{
	g_ucRadarDetectMode = ucRadarDetectMode;

	DBGLOG(P2P, INFO, "p2pFuncSetRadarDetectMode: g_ucRadarDetectMode: %d\n", g_ucRadarDetectMode);
}

UINT_8 p2pFuncGetRadarDetectMode(VOID)
{
	return g_ucRadarDetectMode;
}

VOID p2pFuncSetDfsState(IN UINT_8 ucDfsState)
{
	DBGLOG(P2P, INFO, "[DFS_STATE] TRANSITION: [%s] -> [%s]\n",
		apucDfsState[g_ucDfsState], apucDfsState[ucDfsState]);

	g_ucDfsState = ucDfsState;
}

UINT_8 p2pFuncGetDfsState(VOID)
{
	return g_ucDfsState;
}

PUINT_8 p2pFuncShowDfsState(VOID)
{
	return apucDfsState[g_ucDfsState];
}

VOID p2pFuncRecordCacStartBootTime(VOID)
{
	g_u4CacStartBootTime = kalGetBootTime();
}

UINT_32 p2pFuncGetCacRemainingTime(VOID)
{
	UINT_32 u4CurrentBootTime;
	UINT_32 u4CacRemainingTime;

	u4CurrentBootTime = kalGetBootTime();

	u4CacRemainingTime = g_u4DriverCacTime - (u4CurrentBootTime - g_u4CacStartBootTime)/1000000;

	return u4CacRemainingTime;
}
#endif

#if 0
WLAN_STATUS
p2pFuncBeaconUpdate(IN P_ADAPTER_T prAdapter,
		    IN PUINT_8 pucBcnHdr,
		    IN UINT_32 u4HdrLen,
		    IN PUINT_8 pucBcnBody, IN UINT_32 u4BodyLen, IN UINT_32 u4DtimPeriod, IN UINT_32 u4BcnInterval)
{
	WLAN_STATUS rResultStatus = WLAN_STATUS_INVALID_DATA;
	P_WLAN_BEACON_FRAME_T prBcnFrame = (P_WLAN_BEACON_FRAME_T) NULL;
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;
	P_MSDU_INFO_T prBcnMsduInfo = (P_MSDU_INFO_T) NULL;
	PUINT_8 pucTIMBody = (PUINT_8) NULL;
	UINT_16 u2FrameLength = 0, UINT_16 u2OldBodyLen = 0;
	UINT_8 aucIEBuf[MAX_IE_LENGTH];

	do {
		ASSERT_BREAK(prAdapter != NULL);

		prP2pBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX]);
		prBcnMsduInfo = prP2pBssInfo->prBeacon ASSERT_BREAK(prBcnMsduInfo != NULL);

		/* TODO: Find TIM IE pointer. */
		prBcnFrame = prBcnMsduInfo->prPacket;

		ASSERT_BREAK(prBcnFrame != NULL);

		do {
			/* Ori header. */
			UINT_16 u2IELength = 0, u2Offset = 0;
			PUINT_8 pucIEBuf = prBcnFrame->aucInfoElem;

			u2IELength = prBcnMsduInfo->u2FrameLength - prBcnMsduInfo->ucMacHeaderLength;

			IE_FOR_EACH(pucIEBuf, u2IELength, u2Offset) {
				if ((IE_ID(pucIEBuf) == ELEM_ID_TIM) || ((IE_ID(pucIEBuf) > ELEM_ID_IBSS_PARAM_SET))) {
					pucTIMBody = pucIEBuf;
					break;
				}
				u2FrameLength += IE_SIZE(pucIEBuf);
			}

			if (pucTIMBody == NULL)
				pucTIMBody = pucIEBuf;

			/* Body not change. */
			u2OldBodyLen = (UINT_16) ((UINT_32) pucTIMBody - (UINT_32) prBcnFrame->aucInfoElem);
			/* Move body. */
			kalMemCmp(aucIEBuf, pucTIMBody, u2OldBodyLen);
		} while (FALSE);
		if (pucBcnHdr) {
			kalMemCopy(prBcnMsduInfo->prPacket, pucBcnHdr, u4HdrLen);
			pucTIMBody = (PUINT_8) ((UINT_32) prBcnMsduInfo->prPacket + u4HdrLen);
			prBcnMsduInfo->ucMacHeaderLength =
			    (WLAN_MAC_MGMT_HEADER_LEN +
			     (TIMESTAMP_FIELD_LEN + BEACON_INTERVAL_FIELD_LEN + CAP_INFO_FIELD_LEN));
			u2FrameLength = u4HdrLen;	/* Header + Partial Body. */
		} else {
			/* Header not change. */
			u2FrameLength += prBcnMsduInfo->ucMacHeaderLength;
		}

		if (pucBcnBody) {
			kalMemCopy(pucTIMBody, pucBcnBody, u4BodyLen);
			u2FrameLength += (UINT_16) u4BodyLen;
		} else {
			kalMemCopy(pucTIMBody, aucIEBuf, u2OldBodyLen);
			u2FrameLength += u2OldBodyLen;
		}

		/* Frame Length */
		prBcnMsduInfo->u2FrameLength = u2FrameLength;
		prBcnMsduInfo->fgIs802_11 = TRUE;
		prBcnMsduInfo->ucNetworkType = NETWORK_TYPE_P2P_INDEX;
		prP2pBssInfo->u2BeaconInterval = (UINT_16) u4BcnInterval;
		prP2pBssInfo->ucDTIMPeriod = (UINT_8) u4DtimPeriod;
		prP2pBssInfo->u2CapInfo = prBcnFrame->u2CapInfo;
		prBcnMsduInfo->ucPacketType = 3;
		rResultStatus = nicUpdateBeaconIETemplate(prAdapter,
							  IE_UPD_METHOD_UPDATE_ALL,
							  NETWORK_TYPE_P2P_INDEX,
							  prP2pBssInfo->u2CapInfo,
							  (PUINT_8) prBcnFrame->aucInfoElem,
							  prBcnMsduInfo->u2FrameLength -
							  OFFSET_OF(WLAN_BEACON_FRAME_T, aucInfoElem));
		if (prP2pBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT) {
			/* AP is created, Beacon Update. */
			nicPmIndicateBssAbort(prAdapter, NETWORK_TYPE_P2P_INDEX);
			nicPmIndicateBssCreated(prAdapter, NETWORK_TYPE_P2P_INDEX);
		}

	} while (FALSE);
	return rResultStatus;
}				/* p2pFuncBeaconUpdate */

#else
WLAN_STATUS
p2pFuncBeaconUpdate(IN P_ADAPTER_T prAdapter,
		    IN P_BSS_INFO_T prP2pBssInfo,
		    IN P_P2P_BEACON_UPDATE_INFO_T prBcnUpdateInfo,
		    IN PUINT_8 pucNewBcnHdr, IN UINT_32 u4NewHdrLen, IN PUINT_8 pucNewBcnBody, IN UINT_32 u4NewBodyLen)
{
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;
	P_WLAN_BEACON_FRAME_T prBcnFrame = (P_WLAN_BEACON_FRAME_T) NULL;
	P_MSDU_INFO_T prBcnMsduInfo = (P_MSDU_INFO_T) NULL;
	PUINT_8 pucIEBuf = (PUINT_8) NULL;
	UINT_8 aucIEBuf[MAX_IE_LENGTH];

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prP2pBssInfo != NULL) && (prBcnUpdateInfo != NULL));

		prBcnMsduInfo = prP2pBssInfo->prBeacon;

#if DBG
		if (prBcnUpdateInfo->pucBcnHdr != NULL) {
			ASSERT((UINT_32) prBcnUpdateInfo->pucBcnHdr ==
			       ((UINT_32) prBcnMsduInfo->prPacket + MAC_TX_RESERVED_FIELD));
		}

		if (prBcnUpdateInfo->pucBcnBody != NULL) {
			ASSERT((UINT_32) prBcnUpdateInfo->pucBcnBody ==
			       ((UINT_32) prBcnUpdateInfo->pucBcnHdr + (UINT_32) prBcnUpdateInfo->u4BcnHdrLen));
		}
#endif
		prBcnFrame = (P_WLAN_BEACON_FRAME_T) ((ULONG) prBcnMsduInfo->prPacket + MAC_TX_RESERVED_FIELD);

		if (!pucNewBcnBody) {
			/* Old body. */
			pucNewBcnBody = prBcnUpdateInfo->pucBcnBody;
			ASSERT(u4NewBodyLen == 0);
			u4NewBodyLen = prBcnUpdateInfo->u4BcnBodyLen;
		} else {
			prBcnUpdateInfo->u4BcnBodyLen = u4NewBodyLen;
		}

		/* Temp buffer body part. */
		kalMemCopy(aucIEBuf, pucNewBcnBody, u4NewBodyLen);

		if (pucNewBcnHdr) {
			kalMemCopy(prBcnFrame, pucNewBcnHdr, u4NewHdrLen);
			prBcnUpdateInfo->pucBcnHdr = (PUINT_8) prBcnFrame;
			prBcnUpdateInfo->u4BcnHdrLen = u4NewHdrLen;
		}

		pucIEBuf = (PUINT_8) ((ULONG) prBcnUpdateInfo->pucBcnHdr + (ULONG) prBcnUpdateInfo->u4BcnHdrLen);
		kalMemCopy(pucIEBuf, aucIEBuf, u4NewBodyLen);
		prBcnUpdateInfo->pucBcnBody = pucIEBuf;

		/* Frame Length */
		prBcnMsduInfo->u2FrameLength = (UINT_16) (prBcnUpdateInfo->u4BcnHdrLen + prBcnUpdateInfo->u4BcnBodyLen);

		prBcnMsduInfo->ucPacketType = TX_PACKET_TYPE_MGMT;
		prBcnMsduInfo->fgIs802_11 = TRUE;
		prBcnMsduInfo->ucBssIndex = prP2pBssInfo->ucBssIndex;

		/* Update BSS INFO related information. */
		COPY_MAC_ADDR(prP2pBssInfo->aucOwnMacAddr, prBcnFrame->aucSrcAddr);
		COPY_MAC_ADDR(prP2pBssInfo->aucBSSID, prBcnFrame->aucBSSID);
		prP2pBssInfo->u2CapInfo = prBcnFrame->u2CapInfo;

		p2pFuncParseBeaconContent(prAdapter,
					  prP2pBssInfo,
					  (PUINT_8) prBcnFrame->aucInfoElem,
					  (prBcnMsduInfo->u2FrameLength - OFFSET_OF(WLAN_BEACON_FRAME_T, aucInfoElem)));

#if 1
		/* bssUpdateBeaconContent(prAdapter, NETWORK_TYPE_P2P_INDEX); */
#else
		nicUpdateBeaconIETemplate(prAdapter,
					  IE_UPD_METHOD_UPDATE_ALL,
					  NETWORK_TYPE_P2P_INDEX,
					  prBcnFrame->u2CapInfo,
					  (PUINT_8) prBcnFrame->aucInfoElem,
					  (prBcnMsduInfo->u2FrameLength - OFFSET_OF(WLAN_BEACON_FRAME_T, aucInfoElem)));
#endif
	} while (FALSE);

	return rWlanStatus;
}				/* p2pFuncBeaconUpdate */

/*----------------------------------------------------------------------------*/
/*!
* \brief    This function is to update extra IEs (ex: WPS) for assoc resp.
*           Caller should sanity check the params.
*
* \param[in] prAdapter      Pointer of ADAPTER_T
* \param[in] prP2pBssInfo   Pointer to BSS_INFO_T structure
* \param[in] AssocRespIE    Pointer to extra IEs for assoc resp
* \param[in] u4AssocRespLen Length of extra IEs for assoc resp
*
* \return WLAN_STATUS
*/
/*----------------------------------------------------------------------------*/

WLAN_STATUS
p2pFuncAssocRespUpdate(IN P_ADAPTER_T prAdapter,
		    IN P_BSS_INFO_T prP2pBssInfo,
		    IN PUINT_8 AssocRespIE, IN UINT_32 u4AssocRespLen)
{
	UINT_8 ucOuiType = 0;
	UINT_16 u2SubTypeVersion = 0;

	if (!rsnParseCheckForWFAInfoElem(prAdapter, AssocRespIE, &ucOuiType, &u2SubTypeVersion))
		return WLAN_STATUS_FAILURE;

	if (ucOuiType == VENDOR_OUI_TYPE_WPS) {
		kalP2PUpdateWSC_IE(prAdapter->prGlueInfo, 3, (PUINT_8)AssocRespIE, IE_SIZE(AssocRespIE),
			(UINT_8) (prP2pBssInfo->u4PrivateData));
	}

	return WLAN_STATUS_SUCCESS;
}

#endif

#if 0
/* TODO: We do not apply IE in deauth frame set from upper layer now. */
WLAN_STATUS
p2pFuncDeauth(IN P_ADAPTER_T prAdapter,
	      IN PUINT_8 pucPeerMacAddr,
	      IN UINT_16 u2ReasonCode, IN PUINT_8 pucIEBuf, IN UINT_16 u2IELen, IN BOOLEAN fgSendDeauth)
{
	WLAN_STATUS rWlanStatus = WLAN_STATUS_FAILURE;
	P_STA_RECORD_T prCliStaRec = (P_STA_RECORD_T) NULL;
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;
	BOOLEAN fgIsStaFound = FALSE;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (pucPeerMacAddr != NULL));

		prP2pBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX]);

		prCliStaRec = cnmGetStaRecByAddress(prAdapter, NETWORK_TYPE_P2P_INDEX, pucPeerMacAddr);

		switch (prP2pBssInfo->eCurrentOPMode) {
		case OP_MODE_ACCESS_POINT:
			{
				P_LINK_T prStaRecOfClientList = (P_LINK_T) NULL;
				P_LINK_ENTRY_T prLinkEntry = (P_LINK_ENTRY_T) NULL;

				prStaRecOfClientList = &(prP2pBssInfo->rStaRecOfClientList);

				LINK_FOR_EACH(prLinkEntry, prStaRecOfClientList) {
					if ((UINT_32) prCliStaRec == (UINT_32) prLinkEntry) {
						LINK_REMOVE_KNOWN_ENTRY(prStaRecOfClientList, &prCliStaRec->rLinkEntry);
						fgIsStaFound = TRUE;
						break;
					}
				}

			}
			break;
		case OP_MODE_INFRASTRUCTURE:
			ASSERT(prCliStaRec == prP2pBssInfo->prStaRecOfAP);
			if (prCliStaRec != prP2pBssInfo->prStaRecOfAP)
				break;
			prP2pBssInfo->prStaRecOfAP = NULL;
			fgIsStaFound = TRUE;
			break;
		default:
			break;
		}

		if (fgIsStaFound)
			p2pFuncDisconnect(prAdapter, prCliStaRec, fgSendDeauth, u2ReasonCode);

		rWlanStatus = WLAN_STATUS_SUCCESS;
	} while (FALSE);

	return rWlanStatus;
}				/* p2pFuncDeauth */

/* TODO: We do not apply IE in disassoc frame set from upper layer now. */
WLAN_STATUS
p2pFuncDisassoc(IN P_ADAPTER_T prAdapter,
		IN PUINT_8 pucPeerMacAddr,
		IN UINT_16 u2ReasonCode, IN PUINT_8 pucIEBuf, IN UINT_16 u2IELen, IN BOOLEAN fgSendDisassoc)
{
	WLAN_STATUS rWlanStatus = WLAN_STATUS_FAILURE;
	P_STA_RECORD_T prCliStaRec = (P_STA_RECORD_T) NULL;
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;
	BOOLEAN fgIsStaFound = FALSE;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (pucPeerMacAddr != NULL));

		prP2pBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX]);

		prCliStaRec = cnmGetStaRecByAddress(prAdapter, NETWORK_TYPE_P2P_INDEX, pucPeerMacAddr);

		switch (prP2pBssInfo->eCurrentOPMode) {
		case OP_MODE_ACCESS_POINT:
			{
				P_LINK_T prStaRecOfClientList = (P_LINK_T) NULL;
				P_LINK_ENTRY_T prLinkEntry = (P_LINK_ENTRY_T) NULL;

				prStaRecOfClientList = &(prP2pBssInfo->rStaRecOfClientList);

				LINK_FOR_EACH(prLinkEntry, prStaRecOfClientList) {
					if ((UINT_32) prCliStaRec == (UINT_32) prLinkEntry) {
						LINK_REMOVE_KNOWN_ENTRY(prStaRecOfClientList, &prCliStaRec->rLinkEntry);
						fgIsStaFound = TRUE;
						/* p2pFuncDisconnect(prAdapter, prCliStaRec, */
						/* fgSendDisassoc, u2ReasonCode); */
						break;
					}
				}

			}
			break;
		case OP_MODE_INFRASTRUCTURE:
			ASSERT(prCliStaRec == prP2pBssInfo->prStaRecOfAP);
			if (prCliStaRec != prP2pBssInfo->prStaRecOfAP)
				break;
			/* p2pFuncDisconnect(prAdapter, prCliStaRec, fgSendDisassoc, u2ReasonCode); */
			prP2pBssInfo->prStaRecOfAP = NULL;
			fgIsStaFound = TRUE;
			break;
		default:
			break;
		}

		if (fgIsStaFound) {

			p2pFuncDisconnect(prAdapter, prCliStaRec, fgSendDisassoc, u2ReasonCode);
			/* 20120830 moved into p2pFuncDisconnect(). */
			/* cnmStaRecFree(prAdapter, prCliStaRec); */

		}

		rWlanStatus = WLAN_STATUS_SUCCESS;
	} while (FALSE);

	return rWlanStatus;
}				/* p2pFuncDisassoc */

#endif

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to dissolve from group or one group. (Would not change P2P FSM.)
*              1. GC: Disconnect from AP. (Send Deauth)
*              2. GO: Disconnect all STA
*
* @param[in] prAdapter   Pointer to the adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID
p2pFuncDissolve(IN P_ADAPTER_T prAdapter,
		IN P_BSS_INFO_T prP2pBssInfo, IN BOOLEAN fgSendDeauth, IN UINT_16 u2ReasonCode)
{
	P_STA_RECORD_T prCurrStaRec, prStaRecNext;
	P_LINK_T prClientList;

	DEBUGFUNC("p2pFuncDissolve()");

	do {

		ASSERT_BREAK((prAdapter != NULL) && (prP2pBssInfo != NULL));

		switch (prP2pBssInfo->eCurrentOPMode) {
		case OP_MODE_INFRASTRUCTURE:
			/* Reset station record status. */
			if (prP2pBssInfo->prStaRecOfAP) {
#if CFG_WPS_DISCONNECT || (KERNEL_VERSION(4, 4, 0) <= CFG80211_VERSION_CODE)
				kalP2PGCIndicateConnectionStatus(prAdapter->prGlueInfo,
								 (UINT_8) prP2pBssInfo->u4PrivateData, NULL, NULL, 0,
								 REASON_CODE_DEAUTH_LEAVING_BSS,
								 WLAN_STATUS_MEDIA_DISCONNECT);
#else
				kalP2PGCIndicateConnectionStatus(prAdapter->prGlueInfo,
								 (UINT_8) prP2pBssInfo->u4PrivateData, NULL, NULL, 0,
								 REASON_CODE_DEAUTH_LEAVING_BSS);
#endif

				/* 2012/02/14 frog: After formation before join group, prStaRecOfAP is NULL. */
				p2pFuncDisconnect(prAdapter,
						  prP2pBssInfo, prP2pBssInfo->prStaRecOfAP, fgSendDeauth, u2ReasonCode);
			}

			/* Fix possible KE when RX Beacon & call nicPmIndicateBssConnected().
			* hit prStaRecOfAP == NULL.
			*/
			p2pChangeMediaState(prAdapter, prP2pBssInfo, PARAM_MEDIA_STATE_DISCONNECTED);

			prP2pBssInfo->prStaRecOfAP = NULL;

			break;
		case OP_MODE_ACCESS_POINT:
			/* Under AP mode, we would net send deauthentication frame to each STA.
			 * We only stop the Beacon & let all stations timeout.
			 */
			/* Send deauth. */
			authSendDeauthFrame(prAdapter,
					    prP2pBssInfo,
					    NULL, (P_SW_RFB_T) NULL, u2ReasonCode, (PFN_TX_DONE_HANDLER) NULL);

			prClientList = &prP2pBssInfo->rStaRecOfClientList;

			LINK_FOR_EACH_ENTRY_SAFE(prCurrStaRec, prStaRecNext,
				prClientList, rLinkEntry, STA_RECORD_T) {
				ASSERT(prCurrStaRec);
				p2pFuncDisconnect(prAdapter, prP2pBssInfo, prCurrStaRec, TRUE, u2ReasonCode);
			}
			break;
		default:
			return;	/* 20110420 -- alreay in Device Mode. */
		}

		/* Make the deauth frame send to FW ASAP. */
#if !CFG_SUPPORT_MULTITHREAD
		wlanAcquirePowerControl(prAdapter);
#endif
		wlanProcessCommandQueue(prAdapter, &prAdapter->prGlueInfo->rCmdQueue);
#if !CFG_SUPPORT_MULTITHREAD
		wlanReleasePowerControl(prAdapter);
#endif

		/*kalMdelay(100);*/

		/* Change Connection Status. *//* 20161025, can not set DISCONNECTED if clientcount > 0 */
		/*p2pChangeMediaState(prAdapter, prP2pBssInfo, PARAM_MEDIA_STATE_DISCONNECTED);*/

	} while (FALSE);
}				/* p2pFuncDissolve */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to dissolve from group or one group. (Would not change P2P FSM.)
*              1. GC: Disconnect from AP. (Send Deauth)
*              2. GO: Disconnect all STA
*
* @param[in] prAdapter   Pointer to the adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID
p2pFuncDisconnect(IN P_ADAPTER_T prAdapter,
		  IN P_BSS_INFO_T prP2pBssInfo,
		  IN P_STA_RECORD_T prStaRec, IN BOOLEAN fgSendDeauth, IN UINT_16 u2ReasonCode)
{
	ENUM_PARAM_MEDIA_STATE_T eOriMediaStatus;

	DBGLOG(P2P, INFO, "p2pFuncDisconnect()");

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prStaRec != NULL) && (prP2pBssInfo != NULL));

		ASSERT_BREAK(prP2pBssInfo->eNetworkType == NETWORK_TYPE_P2P);

		ASSERT_BREAK(prP2pBssInfo->ucBssIndex < P2P_DEV_BSS_INDEX);

		eOriMediaStatus = prP2pBssInfo->eConnectionState;

		/* Indicate disconnect. */
		if (prP2pBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT) {
			P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo =
			    P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter, prP2pBssInfo->u4PrivateData);

			kalP2PGOStationUpdate(prAdapter->prGlueInfo, prP2pRoleFsmInfo->ucRoleIndex, prStaRec, FALSE);
		} else {
			scanRemoveConnFlagOfBssDescByBssid(prAdapter, prP2pBssInfo->aucBSSID);
		}

		if (fgSendDeauth) {
			/* Send deauth. */
			authSendDeauthFrame(prAdapter,
					    prP2pBssInfo,
					    prStaRec,
					    (P_SW_RFB_T) NULL,
					    u2ReasonCode, (PFN_TX_DONE_HANDLER) p2pRoleFsmRunEventDeauthTxDone);

			/* Make the deauth frame send to FW ASAP. */
#if !CFG_SUPPORT_MULTITHREAD
			wlanAcquirePowerControl(prAdapter);
#endif
			wlanProcessCommandQueue(prAdapter, &prAdapter->prGlueInfo->rCmdQueue);
#if !CFG_SUPPORT_MULTITHREAD
			wlanReleasePowerControl(prAdapter);
#endif
		} else {
			/* Change station state. */
			cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_1);

			/* Reset Station Record Status. */
			p2pFuncResetStaRecStatus(prAdapter, prStaRec);

			cnmStaRecFree(prAdapter, prStaRec);

			if ((prP2pBssInfo->eCurrentOPMode != OP_MODE_ACCESS_POINT) ||
			    (bssGetClientCount(prAdapter, prP2pBssInfo) == 0)) {
				DBGLOG(P2P, TRACE, "No More Client, Media Status DISCONNECTED\n");
				p2pChangeMediaState(prAdapter, prP2pBssInfo, PARAM_MEDIA_STATE_DISCONNECTED);
			}

			if (eOriMediaStatus != prP2pBssInfo->eConnectionState) {
				/* Update Disconnected state to FW. */
				nicUpdateBss(prAdapter, prP2pBssInfo->ucBssIndex);
			}

		}
	} while (FALSE);

	return;

}				/* p2pFuncDisconnect */

VOID p2pFuncSetChannel(IN P_ADAPTER_T prAdapter, IN UINT_8 ucRoleIdx, IN P_RF_CHANNEL_INFO_T prRfChannelInfo)
{
	P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo = (P_P2P_ROLE_FSM_INFO_T) NULL;
	P_P2P_CONNECTION_REQ_INFO_T prP2pConnReqInfo = (P_P2P_CONNECTION_REQ_INFO_T) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prRfChannelInfo != NULL));

		prP2pRoleFsmInfo = P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter, ucRoleIdx);
		if (!prP2pRoleFsmInfo)
			break;
		prP2pConnReqInfo = &(prP2pRoleFsmInfo->rConnReqInfo);

		prP2pConnReqInfo->rChannelInfo.ucChannelNum = prRfChannelInfo->ucChannelNum;
		prP2pConnReqInfo->rChannelInfo.eBand = prRfChannelInfo->eBand;
		prP2pConnReqInfo->eChnlBw = prRfChannelInfo->ucChnlBw;
		prP2pConnReqInfo->u2PriChnlFreq = prRfChannelInfo->u2PriChnlFreq;
		prP2pConnReqInfo->u4CenterFreq1 = prRfChannelInfo->u4CenterFreq1;
		prP2pConnReqInfo->u4CenterFreq2 = prRfChannelInfo->u4CenterFreq2;

	} while (FALSE);
}				/* p2pFuncSetChannel */

/*----------------------------------------------------------------------------*/
/*!
* @brief Retry JOIN for AUTH_MODE_AUTO_SWITCH
*
* @param[in] prStaRec       Pointer to the STA_RECORD_T
*
* @retval TRUE      We will retry JOIN
* @retval FALSE     We will not retry JOIN
*/
/*----------------------------------------------------------------------------*/
BOOLEAN p2pFuncRetryJOIN(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec, IN P_P2P_JOIN_INFO_T prJoinInfo)
{
	P_MSG_JOIN_REQ_T prJoinReqMsg = (P_MSG_JOIN_REQ_T) NULL;
	BOOLEAN fgRetValue = FALSE;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prStaRec != NULL) && (prJoinInfo != NULL));

		/* Retry other AuthType if possible */
		if (!prJoinInfo->ucAvailableAuthTypes)
			break;

		if (prJoinInfo->ucAvailableAuthTypes & (UINT_8) AUTH_TYPE_SHARED_KEY) {

			DBGLOG(P2P, INFO, "RETRY JOIN INIT: Retry Authentication with AuthType == SHARED_KEY.\n");

			prJoinInfo->ucAvailableAuthTypes &= ~(UINT_8) AUTH_TYPE_SHARED_KEY;

			prStaRec->ucAuthAlgNum = (UINT_8) AUTH_ALGORITHM_NUM_SHARED_KEY;
		} else {
			DBGLOG(P2P, ERROR, "RETRY JOIN INIT: Retry Authentication with Unexpected AuthType.\n");
			ASSERT(0);
			break;
		}

		prJoinInfo->ucAvailableAuthTypes = 0;	/* No more available Auth Types */

		/* Trigger SAA to start JOIN process. */
		prJoinReqMsg = (P_MSG_JOIN_REQ_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_JOIN_REQ_T));
		if (!prJoinReqMsg) {
			ASSERT(0);	/* Can't trigger SAA FSM */
			break;
		}

		prJoinReqMsg->rMsgHdr.eMsgId = MID_P2P_SAA_FSM_START;
		prJoinReqMsg->ucSeqNum = ++prJoinInfo->ucSeqNumOfReqMsg;
		prJoinReqMsg->prStaRec = prStaRec;

		mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prJoinReqMsg, MSG_SEND_METHOD_BUF);

		fgRetValue = TRUE;
	} while (FALSE);

	return fgRetValue;

}				/* end of p2pFuncRetryJOIN() */

P_BSS_INFO_T p2pFuncBSSIDFindBssInfo(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucBSSID)
{
	P_BSS_INFO_T prBssInfo = (P_BSS_INFO_T) NULL;
	UINT_8 ucBssIdx = 0;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (pucBSSID != NULL));

		for (ucBssIdx = 0; ucBssIdx < BSS_INFO_NUM; ucBssIdx++) {
			if (!IS_NET_ACTIVE(prAdapter, ucBssIdx))
				continue;

			prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIdx);

			if (EQUAL_MAC_ADDR(prBssInfo->aucBSSID, pucBSSID) && IS_BSS_P2P(prBssInfo))
				break;

			prBssInfo = NULL;
		}

	} while (FALSE);

	return prBssInfo;
}				/* p2pFuncBSSIDFindBssInfo */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will validate the Rx Auth Frame and then return
*        the status code to AAA to indicate if need to perform following actions
*        when the specified conditions were matched.
*
* @param[in] prAdapter          Pointer to the Adapter structure.
* @param[in] prSwRfb            Pointer to SW RFB data structure.
* @param[in] pprStaRec          Pointer to pointer of STA_RECORD_T structure.
* @param[out] pu2StatusCode     The Status Code of Validation Result
*
* @retval TRUE      Reply the Auth
* @retval FALSE     Don't reply the Auth
*/
/*----------------------------------------------------------------------------*/
BOOLEAN
p2pFuncValidateAuth(IN P_ADAPTER_T prAdapter,
		    IN P_BSS_INFO_T prP2pBssInfo,
		    IN P_SW_RFB_T prSwRfb, IN PP_STA_RECORD_T pprStaRec, OUT PUINT_16 pu2StatusCode)
{
	BOOLEAN fgPmfConn = FALSE;
	P_STA_RECORD_T prStaRec = (P_STA_RECORD_T) NULL;
	P_WLAN_AUTH_FRAME_T prAuthFrame = (P_WLAN_AUTH_FRAME_T) NULL;

	DBGLOG(P2P, TRACE, "p2pValidate Authentication Frame\n");


	/* P2P 3.2.8 */
	*pu2StatusCode = STATUS_CODE_REQ_DECLINED;
	prAuthFrame = (P_WLAN_AUTH_FRAME_T) prSwRfb->pvHeader;

	if ((prP2pBssInfo->eCurrentOPMode != OP_MODE_ACCESS_POINT) ||
	    (prP2pBssInfo->eIntendOPMode != OP_MODE_NUM)) {
		/* We are not under AP Mode yet. */
		DBGLOG(P2P, WARN, "Current OP mode is not under AP mode. (%d)\n", prP2pBssInfo->eCurrentOPMode);
		return FALSE;
	}

	prStaRec = cnmGetStaRecByAddress(prAdapter, prP2pBssInfo->ucBssIndex, prAuthFrame->aucSrcAddr);

	if (!prStaRec) {
		prStaRec = cnmStaRecAlloc(prAdapter, STA_TYPE_P2P_GC,
					  prP2pBssInfo->ucBssIndex, prAuthFrame->aucSrcAddr);

		/* TODO(Kevin): Error handling of allocation of STA_RECORD_T for
		 * exhausted case and do removal of unused STA_RECORD_T.
		 */
		/* Sent a message event to clean un-used STA_RECORD_T. */
		ASSERT(prStaRec);

		prSwRfb->ucStaRecIdx = prStaRec->ucIndex;

		prStaRec->u2BSSBasicRateSet = prP2pBssInfo->u2BSSBasicRateSet;

		prStaRec->u2DesiredNonHTRateSet = RATE_SET_ERP_P2P;

		prStaRec->u2OperationalRateSet = RATE_SET_ERP_P2P;
		prStaRec->ucPhyTypeSet = PHY_TYPE_SET_802_11GN;

		/* Update default Tx rate */
		nicTxUpdateStaRecDefaultRate(prStaRec);

		/* NOTE(Kevin): Better to change state here, not at TX Done */
		cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_1);
	} else {
#if CFG_SUPPORT_802_11W
		/* AP PMF. if PMF connection, do not reset state & FSM */
		fgPmfConn = rsnCheckBipKeyInstalled(prAdapter, prStaRec);
		if (fgPmfConn) {
			DBGLOG(P2P, WARN, "PMF Connction, return false\n");
			return FALSE;
		}
#endif

		prSwRfb->ucStaRecIdx = prStaRec->ucIndex;

		if ((prStaRec->ucStaState > STA_STATE_1) && (IS_STA_IN_P2P(prStaRec))) {

			cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_1);

			p2pFuncResetStaRecStatus(prAdapter, prStaRec);

			bssRemoveClient(prAdapter, prP2pBssInfo, prStaRec);
		}

	}

	if (bssGetClientCount(prAdapter, prP2pBssInfo) >= P2P_MAXIMUM_CLIENT_COUNT
		|| !p2pRoleProcessACLInspection(prAdapter, prStaRec->aucMacAddr, prP2pBssInfo->ucBssIndex)
#if CFG_SUPPORT_HOTSPOT_WPS_MANAGER
		|| kalP2PMaxClients(prAdapter->prGlueInfo, bssGetClientCount(prAdapter, prP2pBssInfo),
		(UINT_8) prP2pBssInfo->u4PrivateData)
#endif
	) {
		/* GROUP limit full. */
		/* P2P 3.2.8 */
		DBGLOG(P2P, WARN, "Group Limit Full. (%d)\n", bssGetClientCount(prAdapter, prP2pBssInfo));
		cnmStaRecFree(prAdapter, prStaRec);
		return TRUE;
	}
#if CFG_SUPPORT_HOTSPOT_WPS_MANAGER
	else {
		/* Hotspot Blacklist */
		if (kalP2PCmpBlackList(prAdapter->prGlueInfo, prAuthFrame->aucSrcAddr,
			(UINT_8) prP2pBssInfo->u4PrivateData)) {
			return FALSE;
		}

	}
#endif
	/* prStaRec->eStaType = STA_TYPE_INFRA_CLIENT; */
	prStaRec->eStaType = STA_TYPE_P2P_GC;

	/* Update Station Record - Status/Reason Code */
	prStaRec->u2StatusCode = STATUS_CODE_SUCCESSFUL;

	prStaRec->ucJoinFailureCount = 0;

	*pprStaRec = prStaRec;

	*pu2StatusCode = STATUS_CODE_SUCCESSFUL;


	return TRUE;

}				/* p2pFuncValidateAuth */

VOID p2pFuncResetStaRecStatus(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec)
{
	do {
		if ((prAdapter == NULL) || (prStaRec == NULL)) {
			ASSERT(FALSE);
			break;
		}

		prStaRec->u2StatusCode = STATUS_CODE_SUCCESSFUL;
		prStaRec->u2ReasonCode = REASON_CODE_RESERVED;
		prStaRec->ucJoinFailureCount = 0;
		prStaRec->fgTransmitKeyExist = FALSE;

		prStaRec->fgSetPwrMgtBit = FALSE;

	} while (FALSE);
}				/* p2pFuncResetStaRecStatus */

/*----------------------------------------------------------------------------*/
/*!
* @brief The function is used to initialize the value of the connection settings for
*        P2P network
*
* @param (none)
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID
p2pFuncInitConnectionSettings(IN P_ADAPTER_T prAdapter,
			      IN P_P2P_CONNECTION_SETTINGS_T prP2PConnSettings, IN BOOLEAN fgIsApMode)
{
	P_WIFI_VAR_T prWifiVar = NULL;

	ASSERT(prP2PConnSettings);

	prWifiVar = &(prAdapter->rWifiVar);
	ASSERT(prWifiVar);

	prP2PConnSettings->fgIsApMode = fgIsApMode;

#if CFG_SUPPORT_HOTSPOT_WPS_MANAGER
	prP2PConnSettings->fgIsWPSMode = prWifiVar->ucApWpsMode;
#endif
}				/* p2pFuncInitConnectionSettings */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will validate the Rx Assoc Req Frame and then return
*        the status code to AAA to indicate if need to perform following actions
*        when the specified conditions were matched.
*
* @param[in] prAdapter          Pointer to the Adapter structure.
* @param[in] prSwRfb            Pointer to SW RFB data structure.
* @param[out] pu2StatusCode     The Status Code of Validation Result
*
* @retval TRUE      Reply the Assoc Resp
* @retval FALSE     Don't reply the Assoc Resp
*/
/*----------------------------------------------------------------------------*/
BOOLEAN p2pFuncValidateAssocReq(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb, OUT PUINT_16 pu2StatusCode)
{
	BOOLEAN fgReplyAssocResp = TRUE;
	P_WLAN_ASSOC_REQ_FRAME_T prAssocReqFrame = (P_WLAN_ASSOC_REQ_FRAME_T) NULL;
	P_STA_RECORD_T prStaRec = (P_STA_RECORD_T) NULL;
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;

	/* TODO(Kevin): Call P2P functions to check ..
	 *  2. Check we can accept connection from thsi peer
	 *  a. If we are in PROVISION state, only accept the peer we do the GO formation previously.
	 *  b. If we are in OPERATION state, only accept the other peer when P2P_GROUP_LIMIT is 0.
	 *  3. Check Black List here.
	 */

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prSwRfb != NULL) && (pu2StatusCode != NULL));

		*pu2StatusCode = STATUS_CODE_REQ_DECLINED;
		prAssocReqFrame = (P_WLAN_ASSOC_REQ_FRAME_T) prSwRfb->pvHeader;

		prP2pBssInfo = p2pFuncBSSIDFindBssInfo(prAdapter, prAssocReqFrame->aucBSSID);

		if (prP2pBssInfo == NULL) {
			DBGLOG(P2P, ERROR, "RX ASSOC frame without BSS active / BSSID match\n");
			ASSERT(FALSE);
			break;
		}

		prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);

		if (prStaRec == NULL) {
			/* Station record should be ready while RX AUTH frame. */
			fgReplyAssocResp = FALSE;
			ASSERT(FALSE);
			break;
		}
		ASSERT(prSwRfb->prRxStatusGroup3);
		prStaRec->ucRCPI = nicRxGetRcpiValueFromRxv(RCPI_MODE_WF0, prSwRfb);

		prStaRec->u2DesiredNonHTRateSet &= prP2pBssInfo->u2OperationalRateSet;
		prStaRec->ucDesiredPhyTypeSet = prStaRec->ucPhyTypeSet & prP2pBssInfo->ucPhyTypeSet;

		if (prStaRec->ucDesiredPhyTypeSet == 0) {
			/* The station only support 11B rate. */
			*pu2StatusCode = STATUS_CODE_ASSOC_DENIED_RATE_NOT_SUPPORTED;
			break;
		}

		*pu2StatusCode = STATUS_CODE_SUCCESSFUL;

	} while (FALSE);

	return fgReplyAssocResp;

}				/* p2pFuncValidateAssocReq */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is used to check the TKIP IE
*
*
* @return none
*/
/*----------------------------------------------------------------------------*/
BOOLEAN p2pFuncParseCheckForTKIPInfoElem(IN PUINT_8 pucBuf)
{
	UINT_8 aucWfaOui[] = VENDOR_OUI_WFA;
	P_WPA_INFO_ELEM_T prWpaIE = (P_WPA_INFO_ELEM_T) NULL;
	UINT_32 u4GroupKeyCipher = 0;

	if (pucBuf == NULL)
		return FALSE;

	prWpaIE = (P_WPA_INFO_ELEM_T) pucBuf;

	if (prWpaIE->ucLength <= ELEM_MIN_LEN_WFA_OUI_TYPE_SUBTYPE)
		return FALSE;

	if (kalMemCmp(prWpaIE->aucOui, aucWfaOui, sizeof(aucWfaOui)))
		return FALSE;

	WLAN_GET_FIELD_32(&prWpaIE->u4GroupKeyCipherSuite, &u4GroupKeyCipher);

	if (prWpaIE->ucOuiType == VENDOR_OUI_TYPE_WPA &&
		u4GroupKeyCipher == WPA_CIPHER_SUITE_TKIP)
		return TRUE;
	else
		return FALSE;
}				/* p2pFuncParseCheckForP2PInfoElem */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is used to check the P2P IE
*
*
* @return none
*/
/*----------------------------------------------------------------------------*/
BOOLEAN p2pFuncParseCheckForP2PInfoElem(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucBuf, OUT PUINT_8 pucOuiType)
{
	UINT_8 aucWfaOui[] = VENDOR_OUI_WFA_SPECIFIC;
	P_IE_WFA_T prWfaIE = (P_IE_WFA_T) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (pucBuf != NULL) && (pucOuiType != NULL));

		prWfaIE = (P_IE_WFA_T) pucBuf;

		if (IE_LEN(pucBuf) <= ELEM_MIN_LEN_WFA_OUI_TYPE_SUBTYPE) {
			break;
		} else if (prWfaIE->aucOui[0] != aucWfaOui[0] ||
			   prWfaIE->aucOui[1] != aucWfaOui[1] || prWfaIE->aucOui[2] != aucWfaOui[2]) {
			break;
		}

		*pucOuiType = prWfaIE->ucOuiType;

		return TRUE;
	} while (FALSE);

	return FALSE;
}				/* p2pFuncParseCheckForP2PInfoElem */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will validate the Rx Probe Request Frame and then return
*        result to BSS to indicate if need to send the corresponding Probe Response
*        Frame if the specified conditions were matched.
*
* @param[in] prAdapter          Pointer to the Adapter structure.
* @param[in] prSwRfb            Pointer to SW RFB data structure.
* @param[out] pu4ControlFlags   Control flags for replying the Probe Response
*
* @retval TRUE      Reply the Probe Response
* @retval FALSE     Don't reply the Probe Response
*/
/*----------------------------------------------------------------------------*/
BOOLEAN
p2pFuncValidateProbeReq(IN P_ADAPTER_T prAdapter,
			IN P_SW_RFB_T prSwRfb,
			OUT PUINT_32 pu4ControlFlags, IN BOOLEAN fgIsDevInterface, IN UINT_8 ucRoleIdx)
{
	BOOLEAN fgIsReplyProbeRsp = FALSE;
	BOOLEAN fgApplyp2PDevFilter = FALSE;
	P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo = (P_P2P_ROLE_FSM_INFO_T) NULL;
	DEBUGFUNC("p2pFuncValidateProbeReq");

	do {

		ASSERT_BREAK((prAdapter != NULL) && (prSwRfb != NULL));

		prP2pRoleFsmInfo = prAdapter->rWifiVar.aprP2pRoleFsmInfo[ucRoleIdx];

		/* Process both cases that with amd without add p2p interface */
		if (fgIsDevInterface)
			fgApplyp2PDevFilter = TRUE;
		else {
			if (prAdapter->prGlueInfo->prP2PInfo[0]->prDevHandler ==
				prAdapter->prGlueInfo->prP2PInfo[ucRoleIdx]->aprRoleHandler)
				fgApplyp2PDevFilter = TRUE;
			else
				fgApplyp2PDevFilter = FALSE;
		}
		/* TODO: */
		if ((fgApplyp2PDevFilter &&
			(prAdapter->u4OsPacketFilter & PARAM_PACKET_FILTER_PROBE_REQ))
			|| (!fgApplyp2PDevFilter &&
			(prP2pRoleFsmInfo->u4P2pPacketFilter & PARAM_PACKET_FILTER_PROBE_REQ))) {
			/* Leave the probe response to p2p_supplicant. */
			kalP2PIndicateRxMgmtFrame(prAdapter->prGlueInfo, prSwRfb, fgIsDevInterface, ucRoleIdx);
		}

	} while (FALSE);

	return fgIsReplyProbeRsp;

}				/* end of p2pFuncValidateProbeReq() */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will validate the Rx Probe Request Frame and then return
*        result to BSS to indicate if need to send the corresponding Probe Response
*        Frame if the specified conditions were matched.
*
* @param[in] prAdapter          Pointer to the Adapter structure.
* @param[in] prSwRfb            Pointer to SW RFB data structure.
* @param[out] pu4ControlFlags   Control flags for replying the Probe Response
*
* @retval TRUE      Reply the Probe Response
* @retval FALSE     Don't reply the Probe Response
*/
/*----------------------------------------------------------------------------*/
VOID p2pFuncValidateRxActionFrame(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb, IN BOOLEAN fgIsDevInterface,
					IN UINT_8 ucRoleIdx)
{
	DEBUGFUNC("p2pFuncValidateRxActionFrame");

	do {

		ASSERT_BREAK((prAdapter != NULL) && (prSwRfb != NULL));

		/* TODO: */

		if (PARAM_PACKET_FILTER_ACTION_FRAME) {
			/* Leave the probe response to p2p_supplicant. */
			kalP2PIndicateRxMgmtFrame(prAdapter->prGlueInfo, prSwRfb, fgIsDevInterface, ucRoleIdx);
		}

	} while (FALSE);

	return;

}				/* p2pFuncValidateRxMgmtFrame */

BOOLEAN p2pFuncIsAPMode(IN P_P2P_CONNECTION_SETTINGS_T prP2pConnSettings)
{
	if (prP2pConnSettings) {
		if (prP2pConnSettings->fgIsWPSMode == 1)
			return FALSE;
		return prP2pConnSettings->fgIsApMode;
	} else {
		return FALSE;
	}
}

/* p2pFuncIsAPMode */

VOID
p2pFuncParseBeaconContent(IN P_ADAPTER_T prAdapter,
			  IN P_BSS_INFO_T prP2pBssInfo, IN PUINT_8 pucIEInfo, IN UINT_32 u4IELen)
{
	PUINT_8 pucIE = (PUINT_8) NULL;
	UINT_16 u2Offset = 0;
	P_P2P_SPECIFIC_BSS_INFO_T prP2pSpecificBssInfo = (P_P2P_SPECIFIC_BSS_INFO_T) NULL;
	UINT_8 i = 0;
	RSN_INFO_T rRsnIe;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prP2pBssInfo != NULL));

		if (u4IELen == 0)
			break;

		prP2pSpecificBssInfo = prAdapter->rWifiVar.prP2pSpecificBssInfo[prP2pBssInfo->u4PrivateData];
		prP2pSpecificBssInfo->u2AttributeLen = 0;

		ASSERT_BREAK(pucIEInfo != NULL);

		pucIE = pucIEInfo;

		if (prP2pBssInfo->u2CapInfo & CAP_INFO_PRIVACY)
			kalP2PSetCipher(prAdapter->prGlueInfo, IW_AUTH_CIPHER_WEP40,
			(UINT_8) prP2pBssInfo->u4PrivateData);
		else
			kalP2PSetCipher(prAdapter->prGlueInfo, IW_AUTH_CIPHER_NONE,
			(UINT_8) prP2pBssInfo->u4PrivateData);

		IE_FOR_EACH(pucIE, u4IELen, u2Offset) {
			switch (IE_ID(pucIE)) {
			case ELEM_ID_SSID:	/* 0 *//* V *//* Done */
				{

					/* DBGLOG(P2P, TRACE, ("SSID update\n")); */
					/* SSID is saved when start AP/GO */
					/* SSID IE set in beacon from supplicant will not always be
					 * the true since hidden SSID case
					 */
#if 0
					   COPY_SSID(prP2pBssInfo->aucSSID,
					   prP2pBssInfo->ucSSIDLen,
					   SSID_IE(pucIE)->aucSSID,
					   SSID_IE(pucIE)->ucLength);

					   COPY_SSID(prP2pSpecificBssInfo->aucGroupSsid,
					   prP2pSpecificBssInfo->u2GroupSsidLen,
					   SSID_IE(pucIE)->aucSSID,
					   SSID_IE(pucIE)->ucLength);
#endif

				}
				break;
			case ELEM_ID_SUP_RATES:	/* 1 *//* V *//* Done */
				{
#ifndef CFG_SUPPORT_P2P_GO_KEEP_RATE_SETTING
					DBGLOG(P2P, TRACE, "Support Rate IE\n");
					if ((SUP_RATES_IE(pucIE)->ucLength) > ELEM_MAX_LEN_SUP_RATES)
						SUP_RATES_IE(pucIE)->ucLength = ELEM_MAX_LEN_SUP_RATES;
					kalMemCopy(prP2pBssInfo->aucAllSupportedRates,
						   SUP_RATES_IE(pucIE)->aucSupportedRates,
						   SUP_RATES_IE(pucIE)->ucLength);
					prP2pBssInfo->ucAllSupportedRatesLen = SUP_RATES_IE(pucIE)->ucLength;
					DBGLOG_MEM8(P2P, TRACE,
						    SUP_RATES_IE(pucIE)->aucSupportedRates,
						    SUP_RATES_IE(pucIE)->ucLength);
#endif
				}
				break;
			case ELEM_ID_DS_PARAM_SET:	/* 3 *//* V *//* Done */
				{
					DBGLOG(P2P, TRACE, "DS PARAM IE: %d.\n", DS_PARAM_IE(pucIE)->ucCurrChnl);

					/* prP2pBssInfo->ucPrimaryChannel = DS_PARAM_IE(pucIE)->ucCurrChnl; */

					/* prP2pBssInfo->eBand = BAND_2G4; */
				}
				break;
			case ELEM_ID_TIM:	/* 5 *//* V */
				TIM_IE(pucIE)->ucDTIMPeriod = prP2pBssInfo->ucDTIMPeriod;
				DBGLOG(P2P, TRACE,
				       "TIM IE, Len:%d, DTIM:%d\n", IE_LEN(pucIE), TIM_IE(pucIE)->ucDTIMPeriod);
				break;
			case ELEM_ID_ERP_INFO:	/* 42 *//* V */
				{
#if 1
					/* This IE would dynamic change due to FW detection change is required. */
					DBGLOG(P2P, TRACE, "ERP IE will be over write by driver\n");
					DBGLOG(P2P, TRACE, "    ucERP: %x.\n", ERP_INFO_IE(pucIE)->ucERP);

#else
					/* This IE would dynamic change due to FW detection change is required. */
					DBGLOG(P2P, TRACE, "ERP IE.\n");

					prP2pBssInfo->ucPhyTypeSet |= PHY_TYPE_SET_802_11GN;

					ASSERT(prP2pBssInfo->eBand == BAND_2G4);

					prP2pBssInfo->fgObssErpProtectMode =
					    ((ERP_INFO_IE(pucIE)->ucERP & ERP_INFO_USE_PROTECTION) ? TRUE : FALSE);

					prP2pBssInfo->fgErpProtectMode =
					    ((ERP_INFO_IE(pucIE)->ucERP & (ERP_INFO_USE_PROTECTION |
									   ERP_INFO_NON_ERP_PRESENT)) ? TRUE : FALSE);
#endif

				}
				break;
			case ELEM_ID_HT_CAP:	/* 45 *//* V */
				{
#if 1
					DBGLOG(P2P, TRACE, "HT CAP IE would be overwritten by driver\n");

					DBGLOG(P2P, TRACE,
					       "HT Cap Info:%x, AMPDU Param:%x\n",
					       HT_CAP_IE(pucIE)->u2HtCapInfo, HT_CAP_IE(pucIE)->ucAmpduParam);

					DBGLOG(P2P, TRACE,
					       "HT Extended Cap:%x, TX Beamforming Cap:%x, Ant Selection Cap:%x\n",
					       HT_CAP_IE(pucIE)->u2HtExtendedCap,
					       HT_CAP_IE(pucIE)->u4TxBeamformingCap, HT_CAP_IE(pucIE)->ucAselCap);
#else
					prP2pBssInfo->ucPhyTypeSet |= PHY_TYPE_SET_802_11N;

					/* u2HtCapInfo */
					if ((HT_CAP_IE(pucIE)->u2HtCapInfo &
					     (HT_CAP_INFO_SUP_CHNL_WIDTH | HT_CAP_INFO_SHORT_GI_40M
					      | HT_CAP_INFO_DSSS_CCK_IN_40M)) == 0) {
						prP2pBssInfo->fgAssoc40mBwAllowed = FALSE;
					} else {
						prP2pBssInfo->fgAssoc40mBwAllowed = TRUE;
					}

					if ((HT_CAP_IE(pucIE)->u2HtCapInfo &
					     (HT_CAP_INFO_SHORT_GI_20M | HT_CAP_INFO_SHORT_GI_40M))
					    == 0) {
						prAdapter->rWifiVar.rConnSettings.fgRxShortGIDisabled = TRUE;
					} else {
						prAdapter->rWifiVar.rConnSettings.fgRxShortGIDisabled = FALSE;
					}

					/* ucAmpduParam */
					DBGLOG(P2P, TRACE,
					       "AMPDU setting from supplicant:0x%x, & default value:0x%x\n",
					       (UINT_8) HT_CAP_IE(pucIE)->ucAmpduParam,
					       (UINT_8) AMPDU_PARAM_DEFAULT_VAL);

					/* rSupMcsSet */
					/* Can do nothing. the field is default value from other configuration. */
					/* HT_CAP_IE(pucIE)->rSupMcsSet; */

					/* u2HtExtendedCap */
					ASSERT(HT_CAP_IE(pucIE)->u2HtExtendedCap ==
					       (HT_EXT_CAP_DEFAULT_VAL &
						~(HT_EXT_CAP_PCO | HT_EXT_CAP_PCO_TRANS_TIME_NONE)));

					/* u4TxBeamformingCap */
					ASSERT(HT_CAP_IE(pucIE)->u4TxBeamformingCap == TX_BEAMFORMING_CAP_DEFAULT_VAL);

					/* ucAselCap */
					ASSERT(HT_CAP_IE(pucIE)->ucAselCap == ASEL_CAP_DEFAULT_VAL);
#endif
				}
				break;
			case ELEM_ID_RSN:	/* 48 *//* V */

				DBGLOG(P2P, TRACE, "RSN IE\n");
				kalP2PSetCipher(prAdapter->prGlueInfo, IW_AUTH_CIPHER_CCMP,
					(UINT_8) prP2pBssInfo->u4PrivateData);

				if (rsnParseRsnIE(prAdapter, RSN_IE(pucIE), &rRsnIe)) {
					prP2pBssInfo->u4RsnSelectedGroupCipher = RSN_CIPHER_SUITE_CCMP;
					prP2pBssInfo->u4RsnSelectedPairwiseCipher = RSN_CIPHER_SUITE_CCMP;
					prP2pBssInfo->u4RsnSelectedAKMSuite = RSN_AKM_SUITE_PSK;
					prP2pBssInfo->u2RsnSelectedCapInfo = rRsnIe.u2RsnCap;
					DBGLOG(RSN, TRACE, "RsnIe CAP:0x%x\n", rRsnIe.u2RsnCap);
				}

#if CFG_SUPPORT_802_11W
				/* AP PMF */
				prP2pBssInfo->rApPmfCfg.fgMfpc = (rRsnIe.u2RsnCap & ELEM_WPA_CAP_MFPC) ? 1 : 0;
				prP2pBssInfo->rApPmfCfg.fgMfpr = (rRsnIe.u2RsnCap & ELEM_WPA_CAP_MFPR) ? 1 : 0;

				for (i = 0; i < rRsnIe.u4AuthKeyMgtSuiteCount; i++) {
					if ((rRsnIe.au4AuthKeyMgtSuite[i] == RSN_AKM_SUITE_PSK_SHA256) ||
						(rRsnIe.au4AuthKeyMgtSuite[i] == RSN_AKM_SUITE_802_1X_SHA256)) {
						DBGLOG(RSN, INFO, "SHA256 support\n");
						/* over-write u4RsnSelectedAKMSuite by SHA256 AKM */
						prP2pBssInfo->u4RsnSelectedAKMSuite =
							rRsnIe.au4AuthKeyMgtSuite[i];
						prP2pBssInfo->rApPmfCfg.fgSha256 = TRUE;
						break;
					}
				}
				DBGLOG(RSN, ERROR, "bcn mfpc:%d, mfpr:%d, sha256:%d\n",
					prP2pBssInfo->rApPmfCfg.fgMfpc,
					prP2pBssInfo->rApPmfCfg.fgMfpr,
					prP2pBssInfo->rApPmfCfg.fgSha256);
#endif

				break;
			case ELEM_ID_EXTENDED_SUP_RATES:	/* 50 *//* V */
#ifndef CFG_SUPPORT_P2P_GO_KEEP_RATE_SETTING
				/* ELEM_ID_SUP_RATES should be placed before ELEM_ID_EXTENDED_SUP_RATES. */
				DBGLOG(P2P, TRACE, "Ex Support Rate IE\n");
				kalMemCopy(&
					   (prP2pBssInfo->aucAllSupportedRates[prP2pBssInfo->ucAllSupportedRatesLen]),
					   EXT_SUP_RATES_IE(pucIE)->aucExtSupportedRates,
					   EXT_SUP_RATES_IE(pucIE)->ucLength);

				DBGLOG_MEM8(P2P, TRACE,
					    EXT_SUP_RATES_IE(pucIE)->aucExtSupportedRates,
					    EXT_SUP_RATES_IE(pucIE)->ucLength);

				prP2pBssInfo->ucAllSupportedRatesLen += EXT_SUP_RATES_IE(pucIE)->ucLength;
#endif
				break;
			case ELEM_ID_HT_OP:
				/* 61 *//* V *//* TODO: */
				{
#if 1
					DBGLOG(P2P, TRACE, "HT OP IE would be overwritten by driver\n");

					DBGLOG(P2P, TRACE,
					       "    Primary Channel: %x, Info1: %x, Info2: %x, Info3: %x\n",
					       HT_OP_IE(pucIE)->ucPrimaryChannel,
					       HT_OP_IE(pucIE)->ucInfo1, HT_OP_IE(pucIE)->u2Info2,
					       HT_OP_IE(pucIE)->u2Info3);
#else
					UINT_16 u2Info2 = 0;

					prP2pBssInfo->ucPhyTypeSet |= PHY_TYPE_SET_802_11N;

					DBGLOG(P2P, TRACE, "HT OP IE\n");

					/* ucPrimaryChannel. */
					ASSERT(HT_OP_IE(pucIE)->ucPrimaryChannel == prP2pBssInfo->ucPrimaryChannel);

					/* ucInfo1 */
					prP2pBssInfo->ucHtOpInfo1 = HT_OP_IE(pucIE)->ucInfo1;

					/* u2Info2 */
					u2Info2 = HT_OP_IE(pucIE)->u2Info2;

					if (u2Info2 & HT_OP_INFO2_NON_GF_HT_STA_PRESENT) {
						ASSERT(prP2pBssInfo->eGfOperationMode != GF_MODE_NORMAL);
						u2Info2 &= ~HT_OP_INFO2_NON_GF_HT_STA_PRESENT;
					}

					if (u2Info2 & HT_OP_INFO2_OBSS_NON_HT_STA_PRESENT) {
						prP2pBssInfo->eObssHtProtectMode = HT_PROTECT_MODE_NON_MEMBER;
						u2Info2 &= ~HT_OP_INFO2_OBSS_NON_HT_STA_PRESENT;
					}

					switch (u2Info2 & HT_OP_INFO2_HT_PROTECTION) {
					case HT_PROTECT_MODE_NON_HT:
						prP2pBssInfo->eHtProtectMode = HT_PROTECT_MODE_NON_HT;
						break;
					case HT_PROTECT_MODE_NON_MEMBER:
						prP2pBssInfo->eHtProtectMode = HT_PROTECT_MODE_NONE;
						prP2pBssInfo->eObssHtProtectMode = HT_PROTECT_MODE_NON_MEMBER;
						break;
					default:
						prP2pBssInfo->eHtProtectMode = HT_OP_IE(pucIE)->u2Info2;
						break;
					}

					/* u2Info3 */
					prP2pBssInfo->u2HtOpInfo3 = HT_OP_IE(pucIE)->u2Info3;

					/* aucBasicMcsSet */
					DBGLOG_MEM8(P2P, TRACE, HT_OP_IE(pucIE)->aucBasicMcsSet, 16);
#endif
				}
				break;
			case ELEM_ID_OBSS_SCAN_PARAMS:	/* 74 *//* V */
				{
					DBGLOG(P2P, TRACE, "ELEM_ID_OBSS_SCAN_PARAMS IE would be replaced by driver\n");
				}
				break;
			case ELEM_ID_EXTENDED_CAP:	/* 127 *//* V */
				{
					DBGLOG(P2P, TRACE, "ELEM_ID_EXTENDED_CAP IE would be replaced by driver\n");
				}
				break;
			case ELEM_ID_VENDOR:	/* 221 *//* V */
				DBGLOG(P2P, TRACE, "Vender Specific IE\n");
				{
					p2pFuncParseBeaconVenderId(prAdapter, pucIE, prP2pSpecificBssInfo,
						(UINT_8) prP2pBssInfo->u4PrivateData);
					/* TODO: Store other Vender IE except for WMM Param. */
				}
				break;
			default:
				DBGLOG(P2P, TRACE, "Unprocessed element ID:%d\n", IE_ID(pucIE));
				break;
			}
		}

	} while (FALSE);
}				/* p2pFuncParseBeaconContent */

/* Code refactoring for AOSP */
static VOID
p2pFuncParseBeaconVenderId(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucIE,
			   IN P_P2P_SPECIFIC_BSS_INFO_T prP2pSpecificBssInfo, IN UINT_8 ucRoleIndex)
{
	do {
		UINT_8 ucOuiType;
		UINT_16 u2SubTypeVersion;

		if (rsnParseCheckForWFAInfoElem(prAdapter, pucIE, &ucOuiType, &u2SubTypeVersion)) {
			if ((ucOuiType == VENDOR_OUI_TYPE_WPA) && (u2SubTypeVersion == VERSION_WPA)) {
				if (!kalP2PGetCcmpCipher(prAdapter->prGlueInfo, ucRoleIndex))
					kalP2PSetCipher(prAdapter->prGlueInfo, IW_AUTH_CIPHER_TKIP, ucRoleIndex);
				kalMemCopy(prP2pSpecificBssInfo->aucWpaIeBuffer, pucIE, IE_SIZE(pucIE));
				prP2pSpecificBssInfo->u2WpaIeLen = IE_SIZE(pucIE);
				DBGLOG(P2P, TRACE, "WPA IE in supplicant\n");
			} else if (ucOuiType == VENDOR_OUI_TYPE_WPS) {
				kalP2PUpdateWSC_IE(prAdapter->prGlueInfo, 0, pucIE, IE_SIZE(pucIE), ucRoleIndex);
				DBGLOG(P2P, TRACE, "WPS IE in supplicant\n");
			} else if (ucOuiType == VENDOR_OUI_TYPE_WMM) {
				DBGLOG(P2P, TRACE, "WMM IE in supplicant\n");
			}
			/* WMM here. */
		} else if (p2pFuncParseCheckForP2PInfoElem(prAdapter, pucIE, &ucOuiType)) {
			/* TODO Store the whole P2P IE & generate later. */
			/* Be aware that there may be one or more P2P IE. */
			if (ucOuiType == VENDOR_OUI_TYPE_P2P) {
				kalMemCopy(&prP2pSpecificBssInfo->aucAttributesCache
					   [prP2pSpecificBssInfo->u2AttributeLen], pucIE, IE_SIZE(pucIE));
				prP2pSpecificBssInfo->u2AttributeLen += IE_SIZE(pucIE);
				DBGLOG(P2P, TRACE, "P2P IE in supplicant\n");
			} else if (ucOuiType == VENDOR_OUI_TYPE_WFD) {

				kalMemCopy(&prP2pSpecificBssInfo->aucAttributesCache
					   [prP2pSpecificBssInfo->u2AttributeLen], pucIE, IE_SIZE(pucIE));

				prP2pSpecificBssInfo->u2AttributeLen += IE_SIZE(pucIE);
			} else {
				DBGLOG(P2P, TRACE, "Unknown 50-6F-9A-%d IE.\n", ucOuiType);
			}
		} else {
			kalMemCopy(&prP2pSpecificBssInfo->aucAttributesCache[prP2pSpecificBssInfo->u2AttributeLen],
				   pucIE, IE_SIZE(pucIE));

			prP2pSpecificBssInfo->u2AttributeLen += IE_SIZE(pucIE);
			DBGLOG(P2P, TRACE, "Driver unprocessed Vender Specific IE\n");
			ASSERT(FALSE);
		}
	} while (0);
}

P_BSS_DESC_T
p2pFuncKeepOnConnection(IN P_ADAPTER_T prAdapter,
			IN P_BSS_INFO_T prBssInfo,
			IN P_P2P_CONNECTION_REQ_INFO_T prConnReqInfo,
			IN P_P2P_CHNL_REQ_INFO_T prChnlReqInfo, IN P_P2P_SCAN_REQ_INFO_T prScanReqInfo)
{
	P_BSS_DESC_T prTargetBss = (P_BSS_DESC_T) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prBssInfo != NULL) &&
			     (prConnReqInfo != NULL) && (prChnlReqInfo != NULL) && (prScanReqInfo != NULL));

		if (prBssInfo->eCurrentOPMode != OP_MODE_INFRASTRUCTURE)
			break;
		/* Update connection request information. */
		ASSERT(prConnReqInfo->eConnRequest == P2P_CONNECTION_TYPE_GC);

		/* Find BSS Descriptor first. */
		prTargetBss = scanP2pSearchDesc(prAdapter, prConnReqInfo);

		if (prTargetBss == NULL) {
			/* Update scan parameter... to scan target device. */
			/* TODO: Need refine. */
			prScanReqInfo->ucNumChannelList = 1;
			prScanReqInfo->eScanType = SCAN_TYPE_ACTIVE_SCAN;
			prScanReqInfo->eChannelSet = SCAN_CHANNEL_FULL;
			prScanReqInfo->u4BufLength = 0;	/* Prevent other P2P ID in IE. */
			prScanReqInfo->fgIsAbort = TRUE;
		} else {
			prChnlReqInfo->u8Cookie = 0;
			prChnlReqInfo->ucReqChnlNum = prTargetBss->ucChannelNum;
			prChnlReqInfo->eBand = prTargetBss->eBand;
			prChnlReqInfo->eChnlSco = prTargetBss->eSco;
			prChnlReqInfo->u4MaxInterval = AIS_JOIN_CH_REQUEST_INTERVAL;
			prChnlReqInfo->eChnlReqType = CH_REQ_TYPE_JOIN;

			prChnlReqInfo->eChannelWidth = prTargetBss->eChannelWidth;
			prChnlReqInfo->ucCenterFreqS1 = prTargetBss->ucCenterFreqS1;
			prChnlReqInfo->ucCenterFreqS2 = prTargetBss->ucCenterFreqS2;
		}

	} while (FALSE);

	return prTargetBss;
}				/* p2pFuncKeepOnConnection */

/* Currently Only for ASSOC Response Frame. */
VOID p2pFuncStoreAssocRspIEBuffer(IN P_ADAPTER_T prAdapter, IN P_P2P_JOIN_INFO_T prP2pJoinInfo, IN P_SW_RFB_T prSwRfb)
{
	P_WLAN_ASSOC_RSP_FRAME_T prAssocRspFrame = (P_WLAN_ASSOC_RSP_FRAME_T) NULL;
	INT_16 i2IELen = 0;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prP2pJoinInfo != NULL) && (prSwRfb != NULL));

		prAssocRspFrame = (P_WLAN_ASSOC_RSP_FRAME_T) prSwRfb->pvHeader;

		if (prAssocRspFrame->u2FrameCtrl != MAC_FRAME_ASSOC_RSP)
			break;

		i2IELen = prSwRfb->u2PacketLen - (WLAN_MAC_HEADER_LEN +
						  CAP_INFO_FIELD_LEN + STATUS_CODE_FIELD_LEN + AID_FIELD_LEN);

		if (i2IELen <= 0)
			break;

		prP2pJoinInfo->u4BufLength = (UINT_32) i2IELen;

		kalMemCopy(prP2pJoinInfo->aucIEBuf, prAssocRspFrame->aucInfoElem, prP2pJoinInfo->u4BufLength);

	} while (FALSE);
}				/* p2pFuncStoreAssocRspIEBuffer */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set Packet Filter.
*
* \param[in] prAdapter      Pointer to the Adapter structure.
* \param[in] pvSetBuffer    Pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                           bytes read from the set buffer. If the call failed
*                           due to invalid length of the set buffer, returns
*                           the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
* \retval WLAN_STATUS_NOT_SUPPORTED
* \retval WLAN_STATUS_ADAPTER_NOT_READY
*/
/*----------------------------------------------------------------------------*/
VOID
p2pFuncMgmtFrameRegister(IN P_ADAPTER_T prAdapter,
			 IN UINT_16 u2FrameType, IN BOOLEAN fgIsRegistered, OUT PUINT_32 pu4P2pPacketFilter)
{
	UINT_32 u4NewPacketFilter = 0;
	CMD_RX_PACKET_FILTER rSetRxPacketFilter;

	DEBUGFUNC("p2pFuncMgmtFrameRegister");

	do {
		ASSERT_BREAK(prAdapter != NULL);

		if (pu4P2pPacketFilter)
			u4NewPacketFilter = *pu4P2pPacketFilter;

		switch (u2FrameType) {
		case MAC_FRAME_PROBE_REQ:
			if (fgIsRegistered) {
				u4NewPacketFilter |= PARAM_PACKET_FILTER_PROBE_REQ;
				DBGLOG(P2P, TRACE, "Open packet filer probe request\n");
			} else {
				u4NewPacketFilter &= ~PARAM_PACKET_FILTER_PROBE_REQ;
				DBGLOG(P2P, TRACE, "Close packet filer probe request\n");
			}
			break;
		case MAC_FRAME_ACTION:
			if (fgIsRegistered) {
				u4NewPacketFilter |= PARAM_PACKET_FILTER_ACTION_FRAME;
				DBGLOG(P2P, TRACE, "Open packet filer action frame.\n");
			} else {
				u4NewPacketFilter &= ~PARAM_PACKET_FILTER_ACTION_FRAME;
				DBGLOG(P2P, TRACE, "Close packet filer action frame.\n");
			}
			break;
		default:
			DBGLOG(P2P, TRACE, "Ask frog to add code for mgmt:%x\n", u2FrameType);
			break;
		}

		if (pu4P2pPacketFilter)
			*pu4P2pPacketFilter = u4NewPacketFilter;

		/* u4NewPacketFilter |= prAdapter->u4OsPacketFilter; */

		prAdapter->u4OsPacketFilter &= ~PARAM_PACKET_FILTER_P2P_MASK;
		prAdapter->u4OsPacketFilter |= u4NewPacketFilter;

		DBGLOG(P2P, TRACE, "P2P Set PACKET filter:0x%x\n",
				prAdapter->u4OsPacketFilter);

		rSetRxPacketFilter.u4RxPacketFilter = prAdapter->u4OsPacketFilter;

		wlanSendSetQueryCmd(prAdapter,
				    CMD_ID_SET_RX_FILTER,
				    TRUE,
				    FALSE,
				    FALSE,
				    nicCmdEventSetCommon,
				    nicOidCmdTimeoutCommon,
				    sizeof(CMD_RX_PACKET_FILTER),
				    (PUINT_8) &rSetRxPacketFilter,
					&u4NewPacketFilter,
					sizeof(u4NewPacketFilter)
		);

		prAdapter->u4OsPacketFilter = rSetRxPacketFilter.u4RxPacketFilter;

	} while (FALSE);
}				/* p2pFuncMgmtFrameRegister */

VOID p2pFuncUpdateMgmtFrameRegister(IN P_ADAPTER_T prAdapter, IN UINT_32 u4OsFilter)
{
	CMD_RX_PACKET_FILTER rSetRxPacketFilter;

	do {

		/* TODO: Filter need to be done. */
		/* prAdapter->rWifiVar.prP2pFsmInfo->u4P2pPacketFilter = u4OsFilter; */

		if ((prAdapter->u4OsPacketFilter & PARAM_PACKET_FILTER_P2P_MASK) ^ u4OsFilter) {

			prAdapter->u4OsPacketFilter &= ~PARAM_PACKET_FILTER_P2P_MASK;

			prAdapter->u4OsPacketFilter |= (u4OsFilter & PARAM_PACKET_FILTER_P2P_MASK);

			rSetRxPacketFilter.u4RxPacketFilter = prAdapter->u4OsPacketFilter;

			wlanSendSetQueryCmd(prAdapter,
					    CMD_ID_SET_RX_FILTER,
					    TRUE,
					    FALSE,
					    FALSE,
					    nicCmdEventSetCommon,
					    nicOidCmdTimeoutCommon,
					    sizeof(CMD_RX_PACKET_FILTER),
					    (PUINT_8)&rSetRxPacketFilter,
					    &u4OsFilter,
					    sizeof(u4OsFilter)
			);

			prAdapter->u4OsPacketFilter = rSetRxPacketFilter.u4RxPacketFilter;
			DBGLOG(P2P, TRACE, "P2P Set PACKET filter:0x%x\n",
					prAdapter->u4OsPacketFilter);
		}

	} while (FALSE);
}				/* p2pFuncUpdateMgmtFrameRegister */

VOID p2pFuncGetStationInfo(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucMacAddr, OUT P_P2P_STATION_INFO_T prStaInfo)
{

	do {
		ASSERT_BREAK((prAdapter != NULL) && (pucMacAddr != NULL) && (prStaInfo != NULL));

		prStaInfo->u4InactiveTime = 0;
		prStaInfo->u4RxBytes = 0;
		prStaInfo->u4TxBytes = 0;
		prStaInfo->u4RxPackets = 0;
		prStaInfo->u4TxPackets = 0;
		/* TODO: */

	} while (FALSE);
}				/* p2pFuncGetStationInfo */
#if 0
BOOLEAN
p2pFuncGetAttriList(IN P_ADAPTER_T prAdapter,
		    IN UINT_8 ucOuiType,
		    IN PUINT_8 pucIE, IN UINT_16 u2IELength, OUT PPUINT_8 ppucAttriList, OUT PUINT_16 pu2AttriListLen)
{
	BOOLEAN fgIsAllocMem = FALSE;
	UINT_8 aucWfaOui[] = VENDOR_OUI_WFA_SPECIFIC;
	UINT_16 u2Offset = 0;
	P_IE_P2P_T prIe = (P_IE_P2P_T) NULL;
	PUINT_8 pucAttriListStart = (PUINT_8) NULL;
	UINT_16 u2AttriListLen = 0, u2BufferSize;
	BOOLEAN fgBackupAttributes = FALSE;

	u2BufferSize = 0;

	do {
		ASSERT_BREAK((prAdapter != NULL) &&
			     (pucIE != NULL) &&
			     (u2IELength != 0) && (ppucAttriList != NULL) && (pu2AttriListLen != NULL));

		if (ppucAttriList)
			*ppucAttriList = NULL;
		if (pu2AttriListLen)
			*pu2AttriListLen = 0;

		if (ucOuiType == VENDOR_OUI_TYPE_WPS) {
			aucWfaOui[0] = 0x00;
			aucWfaOui[1] = 0x50;
			aucWfaOui[2] = 0xF2;
		} else if ((ucOuiType != VENDOR_OUI_TYPE_P2P)
#if CFG_SUPPORT_WFD
			   && (ucOuiType != VENDOR_OUI_TYPE_WFD)
#endif
		    ) {
			DBGLOG(P2P, INFO, "Not supported OUI Type to parsing 0x%x\n", ucOuiType);
			break;
		}

		IE_FOR_EACH(pucIE, u2IELength, u2Offset) {
			if (IE_ID(pucIE) == ELEM_ID_VENDOR) {
				prIe = (P_IE_P2P_T) pucIE;

				if (prIe->ucLength <= P2P_OUI_TYPE_LEN)
					continue;

				if ((prIe->aucOui[0] == aucWfaOui[0]) &&
				    (prIe->aucOui[1] == aucWfaOui[1]) &&
				    (prIe->aucOui[2] == aucWfaOui[2]) && (ucOuiType == prIe->ucOuiType)) {
					p2pFuncGetAttriListAction(prAdapter,
								  prIe, ucOuiType,
								  &pucAttriListStart, &u2AttriListLen,
								  &fgIsAllocMem, &fgBackupAttributes, &u2BufferSize);
				}	/* prIe->aucOui */
			}	/* ELEM_ID_VENDOR */
		}		/* IE_FOR_EACH */

	} while (FALSE);

	if (pucAttriListStart) {
		PUINT_8 pucAttribute = pucAttriListStart;

		DBGLOG(P2P, LOUD, "Checking Attribute Length.\n");
		if (ucOuiType == VENDOR_OUI_TYPE_P2P) {
			P2P_ATTRI_FOR_EACH(pucAttribute, u2AttriListLen, u2Offset);
		} else if (ucOuiType == VENDOR_OUI_TYPE_WFD) {
			/* Todo:: Nothing */
		} else if (ucOuiType == VENDOR_OUI_TYPE_WPS) {
			/* Big Endian: WSC, WFD. */
			WSC_ATTRI_FOR_EACH(pucAttribute, u2AttriListLen, u2Offset) {
				DBGLOG(P2P, LOUD, "Attribute ID:%d, Length:%d.\n",
				       WSC_ATTRI_ID(pucAttribute), WSC_ATTRI_LEN(pucAttribute));
			}
		} else {
		}

		ASSERT(u2Offset == u2AttriListLen);

		*ppucAttriList = pucAttriListStart;
		*pu2AttriListLen = u2AttriListLen;

	} else {
		*ppucAttriList = (PUINT_8) NULL;
		*pu2AttriListLen = 0;
	}

	return fgIsAllocMem;
}				/* p2pFuncGetAttriList */

/* Code refactoring for AOSP */
static VOID
p2pFuncGetAttriListAction(IN P_ADAPTER_T prAdapter,
			  IN P_IE_P2P_T prIe, IN UINT_8 ucOuiType,
			  OUT PUINT_8 *pucAttriListStart, OUT UINT_16 *u2AttriListLen,
			  OUT BOOLEAN *fgIsAllocMem, OUT BOOLEAN *fgBackupAttributes, OUT UINT_16 *u2BufferSize)
{
	do {
		if (!(*pucAttriListStart)) {
			*pucAttriListStart = &prIe->aucP2PAttributes[0];
			if (prIe->ucLength > P2P_OUI_TYPE_LEN)
				*u2AttriListLen = (UINT_16) (prIe->ucLength - P2P_OUI_TYPE_LEN);
			else
				ASSERT(FALSE);
		} else {
			/* More than 2 attributes. */
			UINT_16 u2CopyLen;

			if (*fgBackupAttributes == FALSE) {
				P_P2P_SPECIFIC_BSS_INFO_T prP2pSpecificBssInfo =
				    prAdapter->rWifiVar.prP2pSpecificBssInfo;

				*fgBackupAttributes = TRUE;
				if (ucOuiType == VENDOR_OUI_TYPE_P2P) {
					kalMemCopy(&prP2pSpecificBssInfo->aucAttributesCache[0],
						   *pucAttriListStart, *u2AttriListLen);

					*pucAttriListStart = &prP2pSpecificBssInfo->aucAttributesCache[0];
					*u2BufferSize = P2P_MAXIMUM_ATTRIBUTE_LEN;
				} else if (ucOuiType == VENDOR_OUI_TYPE_WPS) {
					kalMemCopy(&prP2pSpecificBssInfo->aucWscAttributesCache[0],
						   *pucAttriListStart, *u2AttriListLen);
					*pucAttriListStart = &prP2pSpecificBssInfo->aucWscAttributesCache[0];
					*u2BufferSize = WPS_MAXIMUM_ATTRIBUTES_CACHE_SIZE;
				}
#if CFG_SUPPORT_WFD
				else if (ucOuiType == VENDOR_OUI_TYPE_WFD) {
					PUINT_8 pucTmpBuf = (PUINT_8) NULL;

					pucTmpBuf = (PUINT_8) kalMemAlloc
					    (WPS_MAXIMUM_ATTRIBUTES_CACHE_SIZE, VIR_MEM_TYPE);

					if (pucTmpBuf != NULL) {
						*fgIsAllocMem = TRUE;
					} else {
						/* Can't alloca memory for WFD IE relocate. */
						ASSERT(FALSE);
						break;
					}

					kalMemCopy(pucTmpBuf, *pucAttriListStart, *u2AttriListLen);
					*pucAttriListStart = pucTmpBuf;
					*u2BufferSize = WPS_MAXIMUM_ATTRIBUTES_CACHE_SIZE;
				}
#endif
				else
					*fgBackupAttributes = FALSE;
			}
			u2CopyLen = (UINT_16) (prIe->ucLength - P2P_OUI_TYPE_LEN);

			if (((*u2AttriListLen) + u2CopyLen) > (*u2BufferSize)) {
				u2CopyLen = (*u2BufferSize) - (*u2AttriListLen);
				DBGLOG(P2P, WARN, "Length of received P2P attributes > maximum cache size.\n");
			}

			if (u2CopyLen) {
				kalMemCopy((PUINT_8) ((ULONG) (*pucAttriListStart) + (ULONG) (*u2AttriListLen)),
					   &prIe->aucP2PAttributes[0], u2CopyLen);
				*u2AttriListLen += u2CopyLen;
			}

		}
	} while (0);
}
#endif

P_MSDU_INFO_T p2pFuncProcessP2pProbeRsp(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIdx, IN P_MSDU_INFO_T prMgmtTxMsdu)
{
	P_MSDU_INFO_T prRetMsduInfo = prMgmtTxMsdu;
	P_WLAN_PROBE_RSP_FRAME_T prProbeRspFrame = (P_WLAN_PROBE_RSP_FRAME_T) NULL;
	PUINT_8 pucIEBuf = (PUINT_8) NULL;
	UINT_16 u2Offset = 0, u2IELength = 0, u2ProbeRspHdrLen = 0;
	BOOLEAN fgIsP2PIE = FALSE, fgIsWSCIE = FALSE;
	BOOLEAN fgIsWFDIE = FALSE;
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;
	UINT_16 u2EstimateSize = 0, u2EstimatedExtraIELen = 0;
	UINT_32 u4IeArraySize = 0, u4Idx = 0, i = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMgmtTxMsdu != NULL));

		prP2pBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIdx);
		prGlueInfo = prAdapter->prGlueInfo;
		if (!prGlueInfo) {
			DBGLOG(P2P, ERROR, "NULL prGlueInfo\n");
			break;
		}

		/* 3 Make sure this is probe response frame. */
		prProbeRspFrame = (P_WLAN_PROBE_RSP_FRAME_T) ((ULONG) prMgmtTxMsdu->prPacket + MAC_TX_RESERVED_FIELD);
		ASSERT_BREAK((prProbeRspFrame->u2FrameCtrl & MASK_FRAME_TYPE) == MAC_FRAME_PROBE_RSP);

		/* 3 Get the importent P2P IE. */
		u2ProbeRspHdrLen =
		    (WLAN_MAC_MGMT_HEADER_LEN + TIMESTAMP_FIELD_LEN + BEACON_INTERVAL_FIELD_LEN + CAP_INFO_FIELD_LEN);
		pucIEBuf = prProbeRspFrame->aucInfoElem;
		u2IELength = prMgmtTxMsdu->u2FrameLength - u2ProbeRspHdrLen;

#if CFG_SUPPORT_WFD
		/* prAdapter->prGlueInfo->prP2PInfo[0]->u2VenderIELen = 0; */
		/* Reset in each time ?? */
		prAdapter->prGlueInfo->prP2PInfo[prP2pBssInfo->u4PrivateData]->u2WFDIELen = 0;
#endif
		kalP2PResetP2P_IE(prGlueInfo,
			(UINT_8) prP2pBssInfo->u4PrivateData);
		IE_FOR_EACH(pucIEBuf, u2IELength, u2Offset) {
			switch (IE_ID(pucIEBuf)) {
			case ELEM_ID_SSID:
				{
					p2pFuncProcessP2pProbeRspAction(prAdapter, pucIEBuf, ELEM_ID_SSID,
									&ucBssIdx, &prP2pBssInfo, &fgIsWSCIE,
									&fgIsP2PIE, &fgIsWFDIE);
				}
				break;
			case ELEM_ID_VENDOR:
				{
					p2pFuncProcessP2pProbeRspAction(prAdapter, pucIEBuf, ELEM_ID_VENDOR,
									&ucBssIdx, &prP2pBssInfo, &fgIsWSCIE,
									&fgIsP2PIE, &fgIsWFDIE);
				}
				break;
			default:
				break;
			}

		}

		/* 3 Check the total size & current frame. */
		u2EstimateSize = WLAN_MAC_MGMT_HEADER_LEN +
		    TIMESTAMP_FIELD_LEN +
		    BEACON_INTERVAL_FIELD_LEN +
		    CAP_INFO_FIELD_LEN +
		    (ELEM_HDR_LEN + ELEM_MAX_LEN_SSID) +
		    (ELEM_HDR_LEN + ELEM_MAX_LEN_SUP_RATES) + (ELEM_HDR_LEN + ELEM_MAX_LEN_DS_PARAMETER_SET);

		u2EstimatedExtraIELen = 0;

		u4IeArraySize = sizeof(txProbeRspIETable) / sizeof(APPEND_VAR_IE_ENTRY_T);
		for (u4Idx = 0; u4Idx < u4IeArraySize; u4Idx++) {
			if (txProbeRspIETable[u4Idx].u2EstimatedFixedIELen) {
				u2EstimatedExtraIELen += txProbeRspIETable[u4Idx].u2EstimatedFixedIELen;
			}

			else {
				ASSERT(txProbeRspIETable[u4Idx].pfnCalculateVariableIELen);

				u2EstimatedExtraIELen +=
				    (UINT_16) (txProbeRspIETable[u4Idx].pfnCalculateVariableIELen(prAdapter, ucBssIdx,
												  NULL));
			}

		}

		if (fgIsWSCIE)
			u2EstimatedExtraIELen += kalP2PCalWSC_IELen(prAdapter->prGlueInfo, 2,
			(UINT_8) prP2pBssInfo->u4PrivateData);

		if (fgIsP2PIE) {
			for (i = 0; i < MAX_MULTI_P2P_IE_COUNT; i++) {
				u2EstimatedExtraIELen +=
					kalP2PCalP2P_IELen(prGlueInfo, i,
					(UINT_8) prP2pBssInfo->u4PrivateData);
			}
			u2EstimatedExtraIELen += p2pFuncCalculateP2P_IE_NoA(prAdapter, ucBssIdx, NULL);
		}
#if CFG_SUPPORT_WFD
		ASSERT(sizeof(prAdapter->prGlueInfo->prP2PInfo[prP2pBssInfo->u4PrivateData]->aucWFDIE) >=
	       prAdapter->prGlueInfo->prP2PInfo[prP2pBssInfo->u4PrivateData]->u2WFDIELen);
		if (fgIsWFDIE)
			u2EstimatedExtraIELen +=
				prAdapter->prGlueInfo->prP2PInfo[prP2pBssInfo->u4PrivateData]->u2WFDIELen;
#if 0
		u2EstimatedExtraIELen += prAdapter->prGlueInfo->prP2PInfo[0]->u2VenderIELen;
#endif
#endif

		u2EstimateSize += u2EstimatedExtraIELen;
		if ((u2EstimateSize) > (prRetMsduInfo->u2FrameLength)) {
			prRetMsduInfo = cnmMgtPktAlloc(prAdapter, u2EstimateSize);

			if (prRetMsduInfo == NULL) {
				DBGLOG(P2P, WARN, "No packet for sending new probe response, use original one\n");
				prRetMsduInfo = prMgmtTxMsdu;
				break;
			}

		}

		prRetMsduInfo->ucBssIndex = ucBssIdx;

		/* 3 Compose / Re-compose probe response frame. */
		bssComposeBeaconProbeRespFrameHeaderAndFF((PUINT_8)
							  ((ULONG) (prRetMsduInfo->prPacket) +
							   MAC_TX_RESERVED_FIELD),
							  prProbeRspFrame->aucDestAddr,
							  prProbeRspFrame->aucSrcAddr,
							  prProbeRspFrame->aucBSSID,
							  prProbeRspFrame->u2BeaconInterval,
							  prProbeRspFrame->u2CapInfo);

		prRetMsduInfo->u2FrameLength =
		    (WLAN_MAC_MGMT_HEADER_LEN + TIMESTAMP_FIELD_LEN + BEACON_INTERVAL_FIELD_LEN + CAP_INFO_FIELD_LEN);

		bssBuildBeaconProbeRespFrameCommonIEs(prRetMsduInfo, prP2pBssInfo, prProbeRspFrame->aucDestAddr);

		prRetMsduInfo->ucStaRecIndex = prMgmtTxMsdu->ucStaRecIndex;

		for (u4Idx = 0; u4Idx < u4IeArraySize; u4Idx++) {
			if (txProbeRspIETable[u4Idx].pfnAppendIE)
				txProbeRspIETable[u4Idx].pfnAppendIE(prAdapter, prRetMsduInfo);

		}

		if (fgIsWSCIE) {
			kalP2PGenWSC_IE(prAdapter->prGlueInfo,
					2,
					(PUINT_8) ((ULONG) prRetMsduInfo->prPacket +
						   (ULONG) prRetMsduInfo->u2FrameLength),
						   (UINT_8) prP2pBssInfo->u4PrivateData);

			prRetMsduInfo->u2FrameLength += (UINT_16) kalP2PCalWSC_IELen(prAdapter->prGlueInfo, 2,
				(UINT_8) prP2pBssInfo->u4PrivateData);
		}

		if (fgIsP2PIE) {
			for (i = 0; i < MAX_MULTI_P2P_IE_COUNT; i++) {
				kalP2PGenP2P_IE(prGlueInfo, i,
				(PUINT_8) ((ULONG) prRetMsduInfo->prPacket +
					(ULONG) prRetMsduInfo->u2FrameLength),
					(UINT_8) prP2pBssInfo->u4PrivateData);
				prRetMsduInfo->u2FrameLength +=
					kalP2PCalP2P_IELen(prGlueInfo, i,
					(UINT_8) prP2pBssInfo->u4PrivateData);
			}

			p2pFuncGenerateP2P_IE_NoA(prAdapter, prRetMsduInfo);
		}
#if CFG_SUPPORT_WFD

		if (fgIsWFDIE > 0) {
			ASSERT(prAdapter->prGlueInfo->prP2PInfo[prP2pBssInfo->u4PrivateData]->u2WFDIELen > 0);
			kalMemCopy((PUINT_8)
				   ((ULONG) prRetMsduInfo->prPacket +
				    (ULONG) prRetMsduInfo->u2FrameLength),
				   prAdapter->prGlueInfo->prP2PInfo[prP2pBssInfo->u4PrivateData]->aucWFDIE,
				   prAdapter->prGlueInfo->prP2PInfo[prP2pBssInfo->u4PrivateData]->u2WFDIELen);
			prRetMsduInfo->u2FrameLength +=
			(UINT_16) prAdapter->prGlueInfo->prP2PInfo[prP2pBssInfo->u4PrivateData]->u2WFDIELen;

		}
#if 0
		if (prAdapter->prGlueInfo->prP2PInfo[0]->u2VenderIELen > 0) {
			kalMemCopy((PUINT_8)
				   ((UINT_32) prRetMsduInfo->prPacket +
				    (UINT_32) prRetMsduInfo->u2FrameLength),
				   prAdapter->prGlueInfo->prP2PInfo[0]->aucVenderIE,
				   prAdapter->prGlueInfo->prP2PInfo[0]->u2VenderIELen);
			prRetMsduInfo->u2FrameLength += (UINT_16) prAdapter->prGlueInfo->prP2PInfo[0]->u2VenderIELen;
		}
#endif
#endif /* CFG_SUPPORT_WFD */

	} while (FALSE);

	if (prRetMsduInfo != prMgmtTxMsdu)
		cnmMgtPktFree(prAdapter, prMgmtTxMsdu);

	return prRetMsduInfo;
}				/* p2pFuncProcessP2pProbeRsp */

/* Code refactoring for AOSP */
static VOID
p2pFuncProcessP2pProbeRspAction(IN P_ADAPTER_T prAdapter,
				IN PUINT_8 pucIEBuf, IN UINT_8 ucElemIdType,
				OUT UINT_8 *ucBssIdx, OUT P_BSS_INFO_T *prP2pBssInfo, OUT BOOLEAN *fgIsWSCIE,
				OUT BOOLEAN *fgIsP2PIE, OUT BOOLEAN *fgIsWFDIE)
{
	UINT_8 ucOuiType = 0;
	UINT_16 u2SubTypeVersion = 0;

	switch (ucElemIdType) {
	case ELEM_ID_SSID:
		{
			if (SSID_IE(pucIEBuf)->ucLength > 7) {
				for ((*ucBssIdx) = 0; (*ucBssIdx) < MAX_BSS_INDEX; (*ucBssIdx)++) {
					*prP2pBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, *ucBssIdx);
					if (!(*prP2pBssInfo))
						continue;
					if (EQUAL_SSID((*prP2pBssInfo)->aucSSID,
						       (*prP2pBssInfo)->ucSSIDLen,
						       SSID_IE(pucIEBuf)->aucSSID, SSID_IE(pucIEBuf)->ucLength)) {
						break;
					}
				}
				if ((*ucBssIdx) == P2P_DEV_BSS_INDEX)
					*prP2pBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, *ucBssIdx);
			} else {
				*prP2pBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, P2P_DEV_BSS_INDEX);
				COPY_SSID((*prP2pBssInfo)->aucSSID,
					  (*prP2pBssInfo)->ucSSIDLen,
					  SSID_IE(pucIEBuf)->aucSSID, SSID_IE(pucIEBuf)->ucLength);

			}
		}
		break;
	case ELEM_ID_VENDOR:
		if (rsnParseCheckForWFAInfoElem(prAdapter, pucIEBuf, &ucOuiType, &u2SubTypeVersion)) {
			if (ucOuiType == VENDOR_OUI_TYPE_WPS) {
				kalP2PUpdateWSC_IE(prAdapter->prGlueInfo, 2, pucIEBuf, IE_SIZE(pucIEBuf),
					(UINT_8) ((P_BSS_INFO_T)*prP2pBssInfo)->u4PrivateData);
				*fgIsWSCIE = TRUE;
			}

		} else if (p2pFuncParseCheckForP2PInfoElem(prAdapter, pucIEBuf, &ucOuiType)) {
			if (ucOuiType == VENDOR_OUI_TYPE_P2P) {
				kalP2PUpdateP2P_IE(prAdapter->prGlueInfo,
					pucIEBuf, IE_SIZE(pucIEBuf),
					(UINT_8) ((P_BSS_INFO_T)*prP2pBssInfo)->u4PrivateData);
				*fgIsP2PIE = TRUE;
			}
#if CFG_SUPPORT_WFD
			else if (ucOuiType == VENDOR_OUI_TYPE_WFD) {
				DBGLOG(P2P, INFO,
				       "WFD IE is found in probe resp (supp). Len %u\n", IE_SIZE(pucIEBuf));
				if ((sizeof(
				prAdapter->prGlueInfo->prP2PInfo[((P_BSS_INFO_T)*prP2pBssInfo)->u4PrivateData]
				->aucWFDIE) >= (prAdapter->prGlueInfo->prP2PInfo[((P_BSS_INFO_T)*prP2pBssInfo)
				->u4PrivateData]->u2WFDIELen + IE_SIZE(pucIEBuf)))) {
					*fgIsWFDIE = TRUE;
					kalMemCopy(prAdapter->prGlueInfo->prP2PInfo[((P_BSS_INFO_T)*prP2pBssInfo)
						->u4PrivateData]->aucWFDIE, pucIEBuf, IE_SIZE(pucIEBuf));
					prAdapter->prGlueInfo->prP2PInfo[((P_BSS_INFO_T)*prP2pBssInfo)
						->u4PrivateData]->u2WFDIELen +=	IE_SIZE(pucIEBuf);
				}
			}	/*  VENDOR_OUI_TYPE_WFD */
#endif
		} else {
			DBGLOG(P2P, INFO,
			       "Other vender IE is found in probe resp (supp). Len %u\n", IE_SIZE(pucIEBuf));
		}
		break;
	default:
		break;
	}
}

#if 0				/* LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 0) */
UINT_32
p2pFuncCalculateExtra_IELenForBeacon(IN P_ADAPTER_T prAdapter,
				     IN ENUM_NETWORK_TYPE_INDEX_T eNetTypeIndex, IN P_STA_RECORD_T prStaRec)
{

	P_P2P_SPECIFIC_BSS_INFO_T prP2pSpeBssInfo = (P_P2P_SPECIFIC_BSS_INFO_T) NULL;
	UINT_32 u4IELen = 0;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (eNetTypeIndex == NETWORK_TYPE_P2P_INDEX));

		if (p2pFuncIsAPMode(prAdapter->rWifiVar.prP2pFsmInfo))
			break;

		prP2pSpeBssInfo = prAdapter->rWifiVar.prP2pSpecificBssInfo;

		u4IELen = prP2pSpeBssInfo->u2IELenForBCN;

	} while (FALSE);

	return u4IELen;
}				/* p2pFuncCalculateP2p_IELenForBeacon */

VOID p2pFuncGenerateExtra_IEForBeacon(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo)
{
	P_P2P_SPECIFIC_BSS_INFO_T prP2pSpeBssInfo = (P_P2P_SPECIFIC_BSS_INFO_T) NULL;
	PUINT_8 pucIEBuf = (PUINT_8) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsduInfo != NULL));

		prP2pSpeBssInfo = prAdapter->rWifiVar.prP2pSpecificBssInfo;

		if (p2pFuncIsAPMode(prAdapter->rWifiVar.prP2pFsmInfo))
			break;

		pucIEBuf = (PUINT_8) ((UINT_32) prMsduInfo->prPacket + (UINT_32) prMsduInfo->u2FrameLength);

		kalMemCopy(pucIEBuf, prP2pSpeBssInfo->aucBeaconIECache, prP2pSpeBssInfo->u2IELenForBCN);

		prMsduInfo->u2FrameLength += prP2pSpeBssInfo->u2IELenForBCN;

	} while (FALSE);
}				/* p2pFuncGenerateExtra_IEForBeacon */

#else
UINT_32 p2pFuncCalculateP2p_IELenForBeacon(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIdx, IN P_STA_RECORD_T prStaRec)
{
	P_P2P_SPECIFIC_BSS_INFO_T prP2pSpeBssInfo = (P_P2P_SPECIFIC_BSS_INFO_T) NULL;
	UINT_32 u4IELen = 0;
	P_BSS_INFO_T prBssInfo;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (ucBssIdx < BSS_INFO_NUM));

		prBssInfo = prAdapter->aprBssInfo[ucBssIdx];

		if (!prAdapter->fgIsP2PRegistered)
			break;

		if (p2pFuncIsAPMode(prAdapter->rWifiVar.prP2PConnSettings[prBssInfo->u4PrivateData]))
			break;

		prP2pSpeBssInfo = prAdapter->rWifiVar.prP2pSpecificBssInfo[prBssInfo->u4PrivateData];

		u4IELen = prP2pSpeBssInfo->u2AttributeLen;

	} while (FALSE);

	return u4IELen;
}				/* p2pFuncCalculateP2p_IELenForBeacon */

VOID p2pFuncGenerateP2p_IEForBeacon(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo)
{
	P_P2P_SPECIFIC_BSS_INFO_T prP2pSpeBssInfo = (P_P2P_SPECIFIC_BSS_INFO_T) NULL;
	PUINT_8 pucIEBuf = (PUINT_8) NULL;
	P_BSS_INFO_T prBssInfo;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsduInfo != NULL));

		if (!prAdapter->fgIsP2PRegistered)
			break;

		prBssInfo = prAdapter->aprBssInfo[prMsduInfo->ucBssIndex];

		prP2pSpeBssInfo = prAdapter->rWifiVar.prP2pSpecificBssInfo[prBssInfo->u4PrivateData];

		if (p2pFuncIsAPMode(prAdapter->rWifiVar.prP2PConnSettings[prBssInfo->u4PrivateData]))
			break;

		pucIEBuf = (PUINT_8) ((ULONG) prMsduInfo->prPacket + (ULONG) prMsduInfo->u2FrameLength);

		kalMemCopy(pucIEBuf, prP2pSpeBssInfo->aucAttributesCache, prP2pSpeBssInfo->u2AttributeLen);

		prMsduInfo->u2FrameLength += prP2pSpeBssInfo->u2AttributeLen;

	} while (FALSE);
}				/* p2pFuncGenerateP2p_IEForBeacon */

UINT_32 p2pFuncCalculateWSC_IELenForBeacon(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIdx, IN P_STA_RECORD_T prStaRec)
{
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;

	prP2pBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIdx);

	if (prP2pBssInfo->eNetworkType != NETWORK_TYPE_P2P)
		return 0;

	return kalP2PCalWSC_IELen(prAdapter->prGlueInfo, 0, (UINT_8) prP2pBssInfo->u4PrivateData);
}				/* p2pFuncCalculateP2p_IELenForBeacon */

VOID p2pFuncGenerateWSC_IEForBeacon(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo)
{
	PUINT_8 pucBuffer;
	UINT_16 u2IELen = 0;
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

	prP2pBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prMsduInfo->ucBssIndex);

	if (prP2pBssInfo->eNetworkType != NETWORK_TYPE_P2P)
		return;

	u2IELen = (UINT_16) kalP2PCalWSC_IELen(prAdapter->prGlueInfo, 0, (UINT_8) prP2pBssInfo->u4PrivateData);

	pucBuffer = (PUINT_8) ((ULONG) prMsduInfo->prPacket + (ULONG) prMsduInfo->u2FrameLength);

	ASSERT(pucBuffer);

	/* TODO: Check P2P FSM State. */
	kalP2PGenWSC_IE(prAdapter->prGlueInfo, 0, pucBuffer, (UINT_8) prP2pBssInfo->u4PrivateData);

	prMsduInfo->u2FrameLength += u2IELen;
}				/* p2pFuncGenerateP2p_IEForBeacon */

#endif
/*----------------------------------------------------------------------------*/
/*!
* @brief This function is used to calculate P2P IE length for Beacon frame.
*
* @param[in] eNetTypeIndex      Specify which network
* @param[in] prStaRec           Pointer to the STA_RECORD_T
*
* @return The length of P2P IE added
*/
/*----------------------------------------------------------------------------*/
UINT_32 p2pFuncCalculateP2p_IELenForAssocRsp(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex, IN P_STA_RECORD_T prStaRec)
{
	P_BSS_INFO_T prBssInfo = (P_BSS_INFO_T) NULL;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

	if (prBssInfo->eNetworkType != NETWORK_TYPE_P2P)
		return 0;

	return p2pFuncCalculateP2P_IELen(prAdapter,
					 ucBssIndex,
					 prStaRec,
					 txAssocRspAttributesTable,
					 sizeof(txAssocRspAttributesTable) / sizeof(APPEND_VAR_ATTRI_ENTRY_T));

}				/* p2pFuncCalculateP2p_IELenForAssocRsp */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is used to generate P2P IE for Beacon frame.
*
* @param[in] prMsduInfo             Pointer to the composed MSDU_INFO_T.
*
* @return none
*/
/*----------------------------------------------------------------------------*/
VOID p2pFuncGenerateP2p_IEForAssocRsp(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo)
{
	P_STA_RECORD_T prStaRec = (P_STA_RECORD_T) NULL;


	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

	if (!prStaRec) {
		DBGLOG(P2P, ERROR, "prStaRec of ucStaRecIndex %d is NULL!\n",
			prMsduInfo->ucStaRecIndex);
		return;
	}

	if (IS_STA_IN_P2P(prStaRec)) {
		DBGLOG(P2P, TRACE, "Generate NULL P2P IE for Assoc Rsp.\n");

		p2pFuncGenerateP2P_IE(prAdapter,
				      prMsduInfo->ucBssIndex,
				      TRUE,
				      &prMsduInfo->u2FrameLength,
				      prMsduInfo->prPacket,
				      1500,
				      txAssocRspAttributesTable,
				      sizeof(txAssocRspAttributesTable) / sizeof(APPEND_VAR_ATTRI_ENTRY_T));
	} else {

		DBGLOG(P2P, TRACE, "Legacy device, no P2P IE.\n");
	}
}				/* p2pFuncGenerateP2p_IEForAssocRsp */

UINT_32
p2pFuncCalculateP2P_IELen(IN P_ADAPTER_T prAdapter,
			  IN UINT_8 ucBssIndex,
			  IN P_STA_RECORD_T prStaRec,
			  IN APPEND_VAR_ATTRI_ENTRY_T arAppendAttriTable[], IN UINT_32 u4AttriTableSize)
{

	UINT_32 u4OverallAttriLen, u4Dummy;
	UINT_16 u2EstimatedFixedAttriLen;
	UINT_32 i;

	/* Overall length of all Attributes */
	u4OverallAttriLen = 0;

	for (i = 0; i < u4AttriTableSize; i++) {
		u2EstimatedFixedAttriLen = arAppendAttriTable[i].u2EstimatedFixedAttriLen;

		if (u2EstimatedFixedAttriLen) {
			u4OverallAttriLen += u2EstimatedFixedAttriLen;
		} else {
			ASSERT(arAppendAttriTable[i].pfnCalculateVariableAttriLen);

			u4OverallAttriLen += arAppendAttriTable[i].pfnCalculateVariableAttriLen(prAdapter, prStaRec);
		}
	}

	u4Dummy = u4OverallAttriLen;
	u4OverallAttriLen += P2P_IE_OUI_HDR;

	for (; (u4Dummy > P2P_MAXIMUM_ATTRIBUTE_LEN);) {
		u4OverallAttriLen += P2P_IE_OUI_HDR;
		u4Dummy -= P2P_MAXIMUM_ATTRIBUTE_LEN;
	}

	return u4OverallAttriLen;
}				/* p2pFuncCalculateP2P_IELen */

VOID
p2pFuncGenerateP2P_IE(IN P_ADAPTER_T prAdapter,
		      IN UINT_8 ucBssIndex,
		      IN BOOLEAN fgIsAssocFrame,
		      IN PUINT_16 pu2Offset,
		      IN PUINT_8 pucBuf,
		      IN UINT_16 u2BufSize,
		      IN APPEND_VAR_ATTRI_ENTRY_T arAppendAttriTable[], IN UINT_32 u4AttriTableSize)
{
	PUINT_8 pucBuffer = (PUINT_8) NULL;
	P_IE_P2P_T prIeP2P = (P_IE_P2P_T) NULL;
	UINT_32 u4OverallAttriLen;
	UINT_32 u4AttriLen;
	UINT_8 aucWfaOui[] = VENDOR_OUI_WFA_SPECIFIC;
	UINT_8 aucTempBuffer[P2P_MAXIMUM_ATTRIBUTE_LEN];
	UINT_32 i;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (pucBuf != NULL));

		pucBuffer = (PUINT_8) ((ULONG) pucBuf + (*pu2Offset));

		ASSERT_BREAK(pucBuffer != NULL);

		/* Check buffer length is still enough. */
		ASSERT_BREAK((u2BufSize - (*pu2Offset)) >= P2P_IE_OUI_HDR);

		prIeP2P = (P_IE_P2P_T) pucBuffer;

		prIeP2P->ucId = ELEM_ID_P2P;

		prIeP2P->aucOui[0] = aucWfaOui[0];
		prIeP2P->aucOui[1] = aucWfaOui[1];
		prIeP2P->aucOui[2] = aucWfaOui[2];
		prIeP2P->ucOuiType = VENDOR_OUI_TYPE_P2P;

		(*pu2Offset) += P2P_IE_OUI_HDR;

		/* Overall length of all Attributes */
		u4OverallAttriLen = 0;

		for (i = 0; i < u4AttriTableSize; i++) {

			if (arAppendAttriTable[i].pfnAppendAttri) {
				u4AttriLen =
				    arAppendAttriTable[i].pfnAppendAttri(prAdapter, ucBssIndex, fgIsAssocFrame,
									 pu2Offset, pucBuf, u2BufSize);

				u4OverallAttriLen += u4AttriLen;

				if (u4OverallAttriLen > P2P_MAXIMUM_ATTRIBUTE_LEN) {
					u4OverallAttriLen -= P2P_MAXIMUM_ATTRIBUTE_LEN;

					prIeP2P->ucLength = (VENDOR_OUI_TYPE_LEN + P2P_MAXIMUM_ATTRIBUTE_LEN);

					pucBuffer =
					    (PUINT_8) ((ULONG) prIeP2P +
						       (VENDOR_OUI_TYPE_LEN + P2P_MAXIMUM_ATTRIBUTE_LEN));

					prIeP2P = (P_IE_P2P_T) ((ULONG) prIeP2P +
								(ELEM_HDR_LEN +
								 (VENDOR_OUI_TYPE_LEN + P2P_MAXIMUM_ATTRIBUTE_LEN)));

					kalMemCopy(aucTempBuffer, pucBuffer, u4OverallAttriLen);

					prIeP2P->ucId = ELEM_ID_P2P;

					prIeP2P->aucOui[0] = aucWfaOui[0];
					prIeP2P->aucOui[1] = aucWfaOui[1];
					prIeP2P->aucOui[2] = aucWfaOui[2];
					prIeP2P->ucOuiType = VENDOR_OUI_TYPE_P2P;

					kalMemCopy(prIeP2P->aucP2PAttributes, aucTempBuffer, u4OverallAttriLen);
					(*pu2Offset) += P2P_IE_OUI_HDR;
				}

			}

		}

		prIeP2P->ucLength = (UINT_8) (VENDOR_OUI_TYPE_LEN + u4OverallAttriLen);

	} while (FALSE);
}				/* p2pFuncGenerateP2P_IE */

UINT_32
p2pFuncAppendAttriStatusForAssocRsp(IN P_ADAPTER_T prAdapter,
				    IN UINT_8 ucBssIndex,
				    IN BOOLEAN fgIsAssocFrame,
				    IN PUINT_16 pu2Offset, IN PUINT_8 pucBuf, IN UINT_16 u2BufSize)
{
	PUINT_8 pucBuffer;
	P_P2P_ATTRI_STATUS_T prAttriStatus;
	UINT_32 u4AttriLen = 0;

	ASSERT(prAdapter);
	ASSERT(pucBuf);

	if (fgIsAssocFrame)
		return u4AttriLen;
	/* TODO: For assoc request P2P IE check in driver & return status in P2P IE. */

	pucBuffer = (PUINT_8) ((ULONG) pucBuf + (ULONG) (*pu2Offset));

	ASSERT(pucBuffer);
	prAttriStatus = (P_P2P_ATTRI_STATUS_T) pucBuffer;

	ASSERT(u2BufSize >= ((*pu2Offset) + (UINT_16) u4AttriLen));

	prAttriStatus->ucId = P2P_ATTRI_ID_STATUS;
	WLAN_SET_FIELD_16(&prAttriStatus->u2Length, P2P_ATTRI_MAX_LEN_STATUS);

	prAttriStatus->ucStatusCode = P2P_STATUS_SUCCESS;

	u4AttriLen = (P2P_ATTRI_HDR_LEN + P2P_ATTRI_MAX_LEN_STATUS);

	(*pu2Offset) += (UINT_16) u4AttriLen;

	return u4AttriLen;
}				/* p2pFuncAppendAttriStatusForAssocRsp */

UINT_32
p2pFuncAppendAttriExtListenTiming(IN P_ADAPTER_T prAdapter,
				  IN UINT_8 ucBssIndex,
				  IN BOOLEAN fgIsAssocFrame,
				  IN PUINT_16 pu2Offset, IN PUINT_8 pucBuf, IN UINT_16 u2BufSize)
{
	UINT_32 u4AttriLen = 0;
	P_P2P_ATTRI_EXT_LISTEN_TIMING_T prP2pExtListenTiming = (P_P2P_ATTRI_EXT_LISTEN_TIMING_T) NULL;
	P_P2P_SPECIFIC_BSS_INFO_T prP2pSpecificBssInfo = (P_P2P_SPECIFIC_BSS_INFO_T) NULL;
	PUINT_8 pucBuffer = NULL;
	P_BSS_INFO_T prBssInfo = NULL;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

	ASSERT(prAdapter);
	ASSERT(pucBuf);
	ASSERT(prBssInfo);

	if (fgIsAssocFrame)
		return u4AttriLen;
	/* TODO: For extend listen timing. */

	prP2pSpecificBssInfo = prAdapter->rWifiVar.prP2pSpecificBssInfo[prBssInfo->u4PrivateData];

	u4AttriLen = (P2P_ATTRI_HDR_LEN + P2P_ATTRI_MAX_LEN_EXT_LISTEN_TIMING);

	ASSERT(u2BufSize >= ((*pu2Offset) + (UINT_16) u4AttriLen));

	pucBuffer = (PUINT_8) ((ULONG) pucBuf + (ULONG) (*pu2Offset));

	ASSERT(pucBuffer);

	prP2pExtListenTiming = (P_P2P_ATTRI_EXT_LISTEN_TIMING_T) pucBuffer;

	prP2pExtListenTiming->ucId = P2P_ATTRI_ID_EXT_LISTEN_TIMING;
	WLAN_SET_FIELD_16(&prP2pExtListenTiming->u2Length, P2P_ATTRI_MAX_LEN_EXT_LISTEN_TIMING);
	WLAN_SET_FIELD_16(&prP2pExtListenTiming->u2AvailInterval, prP2pSpecificBssInfo->u2AvailabilityInterval);
	WLAN_SET_FIELD_16(&prP2pExtListenTiming->u2AvailPeriod, prP2pSpecificBssInfo->u2AvailabilityPeriod);

	(*pu2Offset) += (UINT_16) u4AttriLen;

	return u4AttriLen;
}				/* p2pFuncAppendAttriExtListenTiming */

P_IE_HDR_T
p2pFuncGetSpecIE(IN P_ADAPTER_T prAdapter,
		 IN PUINT_8 pucIEBuf, IN UINT_16 u2BufferLen, IN UINT_8 ucElemID, IN PBOOLEAN pfgIsMore)
{
	P_IE_HDR_T prTargetIE = (P_IE_HDR_T) NULL;
	PUINT_8 pucIE = (PUINT_8) NULL;
	UINT_16 u2Offset = 0;

	do {
		ASSERT_BREAK((prAdapter != NULL)
			     && (pucIEBuf != NULL));

		pucIE = pucIEBuf;

		if (pfgIsMore)
			*pfgIsMore = FALSE;

		IE_FOR_EACH(pucIE, u2BufferLen, u2Offset) {
			if (IE_ID(pucIE) == ucElemID) {
				if ((prTargetIE) && (pfgIsMore)) {

					*pfgIsMore = TRUE;
					break;
				}
				prTargetIE = (P_IE_HDR_T) pucIE;

				if (pfgIsMore == NULL)
					break;

			}
		}

	} while (FALSE);

	return prTargetIE;
}				/* p2pFuncGetSpecIE */

P_ATTRIBUTE_HDR_T
p2pFuncGetSpecAttri(IN P_ADAPTER_T prAdapter,
		    IN UINT_8 ucOuiType, IN PUINT_8 pucIEBuf, IN UINT_16 u2BufferLen, IN UINT_8 ucAttriID)
{
	P_IE_P2P_T prP2pIE = (P_IE_P2P_T) NULL;
	P_ATTRIBUTE_HDR_T prTargetAttri = (P_ATTRIBUTE_HDR_T) NULL;
	BOOLEAN fgIsMore = FALSE;
	PUINT_8 pucIE = (PUINT_8) NULL;
	UINT_16 u2BufferLenLeft = 0;

	DBGLOG(P2P, INFO, "Check AssocReq Oui type %u attri %u for len %u\n", ucOuiType, ucAttriID, u2BufferLen);

	do {
		ASSERT_BREAK((prAdapter != NULL)
			     && (pucIEBuf != NULL));

		u2BufferLenLeft = u2BufferLen;
		pucIE = pucIEBuf;

		do {
			fgIsMore = FALSE;
			prP2pIE = (P_IE_P2P_T) p2pFuncGetSpecIE(prAdapter,
								pucIE, u2BufferLenLeft, ELEM_ID_VENDOR, &fgIsMore);
			if (prP2pIE) {
				ASSERT((ULONG) prP2pIE >= (ULONG) pucIE);
				u2BufferLenLeft = u2BufferLen - (UINT_16) (((ULONG) prP2pIE) - ((ULONG) pucIEBuf));

				DBGLOG(P2P, INFO, "Find vendor id %u len %u oui %u more %u LeftLen %u\n",
				       IE_ID(prP2pIE), IE_LEN(prP2pIE), prP2pIE->ucOuiType, fgIsMore, u2BufferLenLeft);

				if (IE_LEN(prP2pIE) > P2P_OUI_TYPE_LEN)
					p2pFuncGetSpecAttriAction(prP2pIE, ucOuiType, ucAttriID, &prTargetAttri);
				/* P2P_OUI_TYPE_LEN */
				pucIE = (PUINT_8) (((ULONG) prP2pIE) + IE_SIZE(prP2pIE));
			}
			/* prP2pIE */
		} while (prP2pIE && fgIsMore && u2BufferLenLeft);

	} while (FALSE);

	return prTargetAttri;
}

/* p2pFuncGetSpecAttri */

/* Code refactoring for AOSP */
static VOID
p2pFuncGetSpecAttriAction(IN P_IE_P2P_T prP2pIE,
			  IN UINT_8 ucOuiType, IN UINT_8 ucAttriID, OUT P_ATTRIBUTE_HDR_T *prTargetAttri)
{
	PUINT_8 pucAttri = (PUINT_8) NULL;
	UINT_16 u2OffsetAttri = 0;
	UINT_8 aucWfaOui[] = VENDOR_OUI_WFA_SPECIFIC;

	if (prP2pIE->ucOuiType == ucOuiType) {
		switch (ucOuiType) {
		case VENDOR_OUI_TYPE_WPS:
			aucWfaOui[0] = 0x00;
			aucWfaOui[1] = 0x50;
			aucWfaOui[2] = 0xF2;
			break;
		case VENDOR_OUI_TYPE_P2P:
			break;
		case VENDOR_OUI_TYPE_WPA:
		case VENDOR_OUI_TYPE_WMM:
		case VENDOR_OUI_TYPE_WFD:
		default:
			break;
		}

		if ((prP2pIE->aucOui[0] == aucWfaOui[0]) &&
		    (prP2pIE->aucOui[1] == aucWfaOui[1]) && (prP2pIE->aucOui[2] == aucWfaOui[2])) {

			u2OffsetAttri = 0;
			pucAttri = prP2pIE->aucP2PAttributes;

			if (ucOuiType == VENDOR_OUI_TYPE_WPS) {
				WSC_ATTRI_FOR_EACH(pucAttri, (IE_LEN(prP2pIE) - P2P_IE_OUI_HDR), u2OffsetAttri) {
					if (WSC_ATTRI_ID(pucAttri) == ucAttriID) {
						*prTargetAttri = (P_ATTRIBUTE_HDR_T) pucAttri;
						break;
					}

				}

			} else if (ucOuiType == VENDOR_OUI_TYPE_P2P) {
				P2P_ATTRI_FOR_EACH(pucAttri, (IE_LEN(prP2pIE) - P2P_IE_OUI_HDR), u2OffsetAttri) {
					if (ATTRI_ID(pucAttri) == ucAttriID) {
						*prTargetAttri = (P_ATTRIBUTE_HDR_T) pucAttri;
						break;
					}
				}

			}
#if CFG_SUPPORT_WFD
			else if (ucOuiType == VENDOR_OUI_TYPE_WFD) {
				WFD_ATTRI_FOR_EACH(pucAttri, (IE_LEN(prP2pIE) - P2P_IE_OUI_HDR), u2OffsetAttri) {
					if (ATTRI_ID(pucAttri) == (UINT_8) ucAttriID) {
						*prTargetAttri = (P_ATTRIBUTE_HDR_T) pucAttri;
						break;
					}
				}
			}
#endif
			else {
				/* Todo:: Nothing */
				/* Possible or else. */
			}
		}
	}			/* ucOuiType */
}

WLAN_STATUS
p2pFuncGenerateBeaconProbeRsp(IN P_ADAPTER_T prAdapter,
			      IN P_BSS_INFO_T prBssInfo, IN P_MSDU_INFO_T prMsduInfo, IN BOOLEAN fgIsProbeRsp)
{
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;
	P_WLAN_BEACON_FRAME_T prBcnFrame = (P_WLAN_BEACON_FRAME_T) NULL;
/* P_APPEND_VAR_IE_ENTRY_T prAppendIeTable = (P_APPEND_VAR_IE_ENTRY_T)NULL; */

	do {

		ASSERT_BREAK((prAdapter != NULL) && (prBssInfo != NULL) && (prMsduInfo != NULL));

/* txBcnIETable */

/* txProbeRspIETable */

		prBcnFrame = (P_WLAN_BEACON_FRAME_T) prMsduInfo->prPacket;

		return nicUpdateBeaconIETemplate(prAdapter,
						 IE_UPD_METHOD_UPDATE_ALL,
						 prBssInfo->ucBssIndex,
						 prBssInfo->u2CapInfo,
						 (PUINT_8) prBcnFrame->aucInfoElem,
						 prMsduInfo->u2FrameLength -
						 OFFSET_OF(WLAN_BEACON_FRAME_T, aucInfoElem));

	} while (FALSE);

	return rWlanStatus;
}				/* p2pFuncGenerateBeaconProbeRsp */

WLAN_STATUS
p2pFuncComposeBeaconProbeRspTemplate(IN P_ADAPTER_T prAdapter,
				     IN P_BSS_INFO_T prP2pBssInfo,
				     IN PUINT_8 pucBcnBuffer,
				     IN UINT_32 u4BcnBufLen,
				     IN BOOLEAN fgIsProbeRsp,
				     IN P_P2P_PROBE_RSP_UPDATE_INFO_T prP2pProbeRspInfo, IN BOOLEAN fgSynToFW)
{
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;
	P_MSDU_INFO_T prMsduInfo = (P_MSDU_INFO_T) NULL;
	P_WLAN_MAC_HEADER_T prWlanBcnFrame = (P_WLAN_MAC_HEADER_T) NULL;

	PUINT_8 pucBuffer = (PUINT_8) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (pucBcnBuffer != NULL)
			     && (prP2pBssInfo != NULL));

		prWlanBcnFrame = (P_WLAN_MAC_HEADER_T) pucBcnBuffer;

		if ((prWlanBcnFrame->u2FrameCtrl != MAC_FRAME_BEACON) && (!fgIsProbeRsp)) {
			rWlanStatus = WLAN_STATUS_INVALID_DATA;
			break;
		}

		else if (prWlanBcnFrame->u2FrameCtrl != MAC_FRAME_PROBE_RSP) {
			rWlanStatus = WLAN_STATUS_INVALID_DATA;
			break;
		}

		if (fgIsProbeRsp) {
			ASSERT_BREAK(prP2pProbeRspInfo != NULL);

			if (!prP2pProbeRspInfo->prProbeRspMsduTemplate)
				cnmMgtPktFree(prAdapter, prP2pProbeRspInfo->prProbeRspMsduTemplate);

			prP2pProbeRspInfo->prProbeRspMsduTemplate = cnmMgtPktAlloc(prAdapter, u4BcnBufLen);
			if (!prP2pProbeRspInfo->prProbeRspMsduTemplate) {
				DBGLOG(P2P, ERROR, "cnmMgtPktAlloc fail!\n");
				rWlanStatus = WLAN_STATUS_FAILURE;
				break;
			}

			prMsduInfo = prP2pProbeRspInfo->prProbeRspMsduTemplate;
			prMsduInfo->eSrc = TX_PACKET_MGMT;
			prMsduInfo->ucStaRecIndex = 0xFF;
			prMsduInfo->ucBssIndex = prP2pBssInfo->ucBssIndex;

		} else {
			prMsduInfo = prP2pBssInfo->prBeacon;

			if (prMsduInfo == NULL) {
				rWlanStatus = WLAN_STATUS_FAILURE;
				break;
			}

			if (u4BcnBufLen > (OFFSET_OF(WLAN_BEACON_FRAME_T, aucInfoElem[0]) + MAX_IE_LENGTH)) {
				/* Unexpected error, buffer overflow. */
				ASSERT(FALSE);
				break;
			}

		}

		pucBuffer = (PUINT_8) ((ULONG) (prMsduInfo->prPacket) + MAC_TX_RESERVED_FIELD);

		kalMemCopy(pucBuffer, pucBcnBuffer, u4BcnBufLen);

		prMsduInfo->fgIs802_11 = TRUE;
		prMsduInfo->u2FrameLength = (UINT_16) u4BcnBufLen;

		if (fgSynToFW)
			rWlanStatus = p2pFuncGenerateBeaconProbeRsp(prAdapter, prP2pBssInfo, prMsduInfo, fgIsProbeRsp);

	} while (FALSE);

	return rWlanStatus;

}				/* p2pFuncComposeBeaconTemplate */

UINT_32 wfdFuncCalculateWfdIELenForAssocRsp(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex, IN P_STA_RECORD_T prStaRec)
{
#if CFG_SUPPORT_WFD_COMPOSE_IE
	UINT_16 u2EstimatedExtraIELen = 0;
	P_BSS_INFO_T prBssInfo = (P_BSS_INFO_T) NULL;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);
	if (prBssInfo->eNetworkType != NETWORK_TYPE_P2P)
		return 0;

	if (!IS_STA_P2P_TYPE(prStaRec))
		return 0;

	u2EstimatedExtraIELen = prAdapter->prGlueInfo->prP2PInfo[0]->u2WFDIELen;

	if (u2EstimatedExtraIELen < VENDOR_SPECIFIC_IE_LENGTH)
		return u2EstimatedExtraIELen;
	else
		return 0;
#else
	return 0;
#endif
}				/* wfdFuncCalculateWfdIELenForAssocRsp */

VOID wfdFuncGenerateWfdIEForAssocRsp(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo)
{
#if CFG_SUPPORT_WFD_COMPOSE_IE
	P_STA_RECORD_T prStaRec;
	UINT_16 u2EstimatedExtraIELen;
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;
	P_GLUE_INFO_T prGlueInfo;
	P_GL_P2P_INFO_T prP2PInfo;

	if (!prAdapter)
		return;

	if (!prMsduInfo)
		return;

	prGlueInfo = prAdapter->prGlueInfo;
	if (!prGlueInfo)
		return;

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);
	if (!prStaRec)
		return;

	if (!IS_STA_P2P_TYPE(prStaRec))
		return;

	prP2pBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prMsduInfo->ucBssIndex);
	prP2PInfo = prGlueInfo->prP2PInfo[prP2pBssInfo->u4PrivateData];
	if (!prP2PInfo)
		return;

	u2EstimatedExtraIELen = prP2PInfo->u2WFDIELen;
	if (u2EstimatedExtraIELen > 0
		&& u2EstimatedExtraIELen < VENDOR_SPECIFIC_IE_LENGTH) {
		kalMemCopy((prMsduInfo->prPacket + prMsduInfo->u2FrameLength),
			prP2PInfo->aucWFDIE, u2EstimatedExtraIELen);
		prMsduInfo->u2FrameLength += u2EstimatedExtraIELen;
	}
	return;
#else
	return;
#endif
}				/* wfdFuncGenerateWfdIEForAssocRsp */

VOID
p2pFuncComposeNoaAttribute(IN P_ADAPTER_T prAdapter,
			   IN UINT_8 ucBssIndex, OUT PUINT_8 aucNoaAttrArray, OUT PUINT_32 pu4Len)
{
	P_BSS_INFO_T prBssInfo = NULL;
	P_P2P_ATTRI_NOA_T prNoaAttr = NULL;
	P_P2P_SPECIFIC_BSS_INFO_T prP2pSpecificBssInfo = NULL;
	P_NOA_DESCRIPTOR_T prNoaDesc = NULL;
	UINT_32 u4NumOfNoaDesc = 0;
	UINT_32 i = 0;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);
	prP2pSpecificBssInfo = prAdapter->rWifiVar.prP2pSpecificBssInfo[prBssInfo->u4PrivateData];

	prNoaAttr = (P_P2P_ATTRI_NOA_T) aucNoaAttrArray;

	prNoaAttr->ucId = P2P_ATTRI_ID_NOTICE_OF_ABSENCE;
	prNoaAttr->ucIndex = prP2pSpecificBssInfo->ucNoAIndex;

	if (prP2pSpecificBssInfo->fgEnableOppPS) {
		prNoaAttr->ucCTWOppPSParam = P2P_CTW_OPPPS_PARAM_OPPPS_FIELD |
		    (prP2pSpecificBssInfo->u2CTWindow & P2P_CTW_OPPPS_PARAM_CTWINDOW_MASK);
	} else {
		prNoaAttr->ucCTWOppPSParam = 0;
	}

	for (i = 0; i < prP2pSpecificBssInfo->ucNoATimingCount; i++) {
		if (prP2pSpecificBssInfo->arNoATiming[i].fgIsInUse) {
			prNoaDesc = (P_NOA_DESCRIPTOR_T) &prNoaAttr->aucNoADesc[u4NumOfNoaDesc];

			prNoaDesc->ucCountType = prP2pSpecificBssInfo->arNoATiming[i].ucCount;
			prNoaDesc->u4Duration = prP2pSpecificBssInfo->arNoATiming[i].u4Duration;
			prNoaDesc->u4Interval = prP2pSpecificBssInfo->arNoATiming[i].u4Interval;
			prNoaDesc->u4StartTime = prP2pSpecificBssInfo->arNoATiming[i].u4StartTime;

			u4NumOfNoaDesc++;
		}
	}

	/* include "index" + "OppPs Params" + "NOA descriptors" */
	prNoaAttr->u2Length = 2 + u4NumOfNoaDesc * sizeof(NOA_DESCRIPTOR_T);

	/* include "Attribute ID" + "Length" + "index" + "OppPs Params" + "NOA descriptors" */
	*pu4Len = P2P_ATTRI_HDR_LEN + prNoaAttr->u2Length;
}

UINT_32 p2pFuncCalculateP2P_IE_NoA(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIdx, IN P_STA_RECORD_T prStaRec)
{
	P_P2P_SPECIFIC_BSS_INFO_T prP2pSpecificBssInfo = NULL;
	UINT_8 ucIdx;
	UINT_32 u4NumOfNoaDesc = 0;
	P_BSS_INFO_T prBssInfo;

	prBssInfo = prAdapter->aprBssInfo[ucBssIdx];

	if (p2pFuncIsAPMode(prAdapter->rWifiVar.prP2PConnSettings[prBssInfo->u4PrivateData]))
		return 0;

	prP2pSpecificBssInfo = prAdapter->rWifiVar.prP2pSpecificBssInfo[prBssInfo->u4PrivateData];

	for (ucIdx = 0; ucIdx < prP2pSpecificBssInfo->ucNoATimingCount; ucIdx++) {
		if (prP2pSpecificBssInfo->arNoATiming[ucIdx].fgIsInUse)
			u4NumOfNoaDesc++;
	}

	/* include "index" + "OppPs Params" + "NOA descriptors" */
	/* include "Attribute ID" + "Length" + "index" + "OppPs Params" + "NOA descriptors" */
	return P2P_ATTRI_HDR_LEN + 2 + (u4NumOfNoaDesc * sizeof(NOA_DESCRIPTOR_T));
}

VOID p2pFuncGenerateP2P_IE_NoA(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo)
{
	P_IE_P2P_T prIeP2P;
	UINT_8 aucWfaOui[] = VENDOR_OUI_WFA_SPECIFIC;
	UINT_32 u4AttributeLen;
	P_BSS_INFO_T prBssInfo;

	prBssInfo = prAdapter->aprBssInfo[prMsduInfo->ucBssIndex];

	if (p2pFuncIsAPMode(prAdapter->rWifiVar.prP2PConnSettings[prBssInfo->u4PrivateData]))
		return;

	prIeP2P = (P_IE_P2P_T) ((ULONG) prMsduInfo->prPacket + (UINT_32) prMsduInfo->u2FrameLength);

	prIeP2P->ucId = ELEM_ID_P2P;
	prIeP2P->aucOui[0] = aucWfaOui[0];
	prIeP2P->aucOui[1] = aucWfaOui[1];
	prIeP2P->aucOui[2] = aucWfaOui[2];
	prIeP2P->ucOuiType = VENDOR_OUI_TYPE_P2P;

	/* Compose NoA attribute */
	p2pFuncComposeNoaAttribute(prAdapter, prMsduInfo->ucBssIndex, prIeP2P->aucP2PAttributes, &u4AttributeLen);

	prIeP2P->ucLength = VENDOR_OUI_TYPE_LEN + u4AttributeLen;

	prMsduInfo->u2FrameLength += (ELEM_HDR_LEN + prIeP2P->ucLength);

}
