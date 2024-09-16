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
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/mgmt/saa_fsm.c#2
*/

/*! \file   "saa_fsm.c"
*    \brief  This file defines the FSM for SAA MODULE.
*
*    This file defines the FSM for SAA MODULE.
*/


/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "precomp.h"

/*******************************************************************************
*                              C O N S T A N T S
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
static PUINT_8 apucDebugAAState[AA_STATE_NUM] = {
	(PUINT_8) DISP_STRING("AA_IDLE"),
	(PUINT_8) DISP_STRING("SAA_SEND_AUTH1"),
	(PUINT_8) DISP_STRING("SAA_WAIT_AUTH2"),
	(PUINT_8) DISP_STRING("SAA_SEND_AUTH3"),
	(PUINT_8) DISP_STRING("SAA_WAIT_AUTH4"),
	(PUINT_8) DISP_STRING("SAA_SEND_ASSOC1"),
	(PUINT_8) DISP_STRING("SAA_WAIT_ASSOC2"),
	(PUINT_8) DISP_STRING("AAA_SEND_AUTH2"),
	(PUINT_8) DISP_STRING("AAA_SEND_AUTH4"),
	(PUINT_8) DISP_STRING("AAA_SEND_ASSOC2"),
	(PUINT_8) DISP_STRING("AA_RESOURCE")
};

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
#if CFG_SUPPORT_CFG80211_AUTH
/*----------------------------------------------------------------------------*/
/*
 * @brief prepare to send authentication or association frame
 *
 * @param[in] prStaRec		Pointer to the STA_RECORD_T
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void saaSendAuthAssoc(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec)
{
	/* This function do the things like
	 * "case SAA_STATE_SEND_AUTH1/ASSOC1" in SAA FSM steps
	 */

	UINT_32 rStatus = WLAN_STATUS_FAILURE;
	P_CONNECTION_SETTINGS_T prConnSettings = NULL;
	UINT_16 u2AuthTransSN = AUTH_TRANSACTION_SEQ_1; /* default for OPEN */
	P_BSS_DESC_T prBssDesc = NULL;
	P_AIS_SPECIFIC_BSS_INFO_T prAisSpecBssInfo = NULL;
	PARAM_SSID_T rParamSsid;

	ASSERT(prAdapter);
	ASSERT(prStaRec);

	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	DBGLOG(SAA, INFO, "[SAA]saaSendAuthAssoc, StaState:%d\n",
		prStaRec->ucStaState);

	if (prStaRec->ucTxAuthAssocRetryCount >=
		prStaRec->ucTxAuthAssocRetryLimit) {

		/* Record the Status Code of Authentication Request */
		prStaRec->u2StatusCode = (prConnSettings->fgIsSendAssoc) ?
			STATUS_CODE_ASSOC_TIMEOUT : STATUS_CODE_AUTH_TIMEOUT;

		if (saaFsmSendEventJoinComplete(prAdapter,
			WLAN_STATUS_FAILURE,
			prStaRec, NULL) ==
			WLAN_STATUS_RESOURCES) {
			/* can set a timer and retry later */
			DBGLOG(SAA, WARN,
			"[SAA]can't alloc msg for inform AIS join complete\n");
		}
	} else {
		prStaRec->ucTxAuthAssocRetryCount++;
		/* Prepare to send authentication frame */
		if (!prConnSettings->fgIsSendAssoc) {
			/* Fill authentication transaction sequence number
			 * depends on auth type
			 */
			if ((prAdapter->prGlueInfo->rWpaInfo.u4AuthAlg &
				AUTH_TYPE_SAE) ||
				(prAdapter->prGlueInfo->rWpaInfo.u4AuthAlg &
				AUTH_TYPE_SHARED_KEY)) {
				kalMemCopy(&u2AuthTransSN,
					prConnSettings->aucAuthData,
					AUTH_TRANSACTION_SEQENCE_NUM_FIELD_LEN);
				DBGLOG(SAA, INFO,
				"[SAA]Get auth SN = %d from Conn Settings\n",
				u2AuthTransSN);
			}
			/* Update Station Record - Class 1 Flag */
			if (prStaRec->ucStaState != STA_STATE_1) {
				DBGLOG(SAA, WARN,
				"[SAA]Rx send auth CMD at unexpect state:%d\n",
				prStaRec->ucStaState);
				cnmStaRecChangeState(prAdapter,
					prStaRec, STA_STATE_1);
			}
#if !CFG_SUPPORT_AAA
			rStatus = authSendAuthFrame(prAdapter,
						prStaRec, u2AuthTransSN);
#else
			rStatus = authSendAuthFrame(prAdapter,
						prStaRec,
						prStaRec->ucBssIndex,
						NULL,
						u2AuthTransSN,
						STATUS_CODE_RESERVED);
#endif /* CFG_SUPPORT_AAA */
			prStaRec->eAuthAssocSent = u2AuthTransSN;
		} else { /* Prepare to send association frame */
#if 1
			/* Fill Cipher/AKM before sending association request,
			 * copy fro m AIS search step
			 */
			if (prConnSettings->ucSSIDLen) {
				rParamSsid.u4SsidLen =
					prConnSettings->ucSSIDLen;
				COPY_SSID(rParamSsid.aucSsid,
					rParamSsid.u4SsidLen,
					prConnSettings->aucSSID,
					prConnSettings->ucSSIDLen);
				prBssDesc = scanSearchBssDescByBssidAndSsid(
						prAdapter,
						prStaRec->aucMacAddr,
						TRUE,
						&rParamSsid);
				DBGLOG(RSN, INFO, "[RSN] prBssDesc["
					MACSTR" ,%s] Searched by BSSID["
					MACSTR"] & SSID %s.\n",
					MAC2STR(prBssDesc->aucBSSID),
					prBssDesc->aucSSID,
					MAC2STR(prStaRec->aucMacAddr),
					prConnSettings->aucSSID);
			} else {
				prBssDesc = scanSearchBssDescByBssidAndChanNum(
					prAdapter,
					prStaRec->aucMacAddr,
					TRUE,
					prConnSettings->ucChannelNum);
				DBGLOG(RSN, INFO, "[RSN] prBssDesc["
					MACSTR" ,%s] Searched by BSSID["
					MACSTR"] & ChanNum %d.\n",
					MAC2STR(prBssDesc->aucBSSID),
					prBssDesc->aucSSID,
					MAC2STR(prStaRec->aucMacAddr),
					prConnSettings->ucChannelNum);
			}

			prAisSpecBssInfo =
				&(prAdapter->rWifiVar.rAisSpecificBssInfo);
			if (rsnPerformPolicySelection(prAdapter, prBssDesc)) {
				if (prAisSpecBssInfo->fgCounterMeasure)
					DBGLOG(RSN, WARN,
					"Skip whle at counter measure perid\n");
				else {
					DBGLOG(RSN, INFO, "Bss RSN matched!\n");
					prAdapter->prAisBssInfo
						->u4RsnSelectedGroupCipher =
						prBssDesc
						->u4RsnSelectedGroupCipher;
					prAdapter->prAisBssInfo
						->u4RsnSelectedPairwiseCipher =
						prBssDesc
						->u4RsnSelectedPairwiseCipher;
					prAdapter->prAisBssInfo
						->u4RsnSelectedAKMSuite =
						prBssDesc
						->u4RsnSelectedAKMSuite;
				}
			} else
				DBGLOG(RSN, WARN, "Bss fail for RSN check\n");
#endif
			if (prStaRec->ucStaState == STA_STATE_1) {
				/* don't change to state2 for reassociation */
				/* Update Station Record - Class 2 Flag */
				cnmStaRecChangeState(prAdapter,
						prStaRec, STA_STATE_2);
			}

			rStatus = assocSendReAssocReqFrame(prAdapter, prStaRec);
			prStaRec->eAuthAssocSent = AA_SENT_ASSOC1;
		}

		if (rStatus != WLAN_STATUS_SUCCESS) {
			/* maybe can't alloc msdu info, retry after timeout */
			cnmTimerInitTimer(prAdapter,
					  &prStaRec->rTxReqDoneOrRxRespTimer,
					  (PFN_MGMT_TIMEOUT_FUNC)
					  saaFsmRunEventTxReqTimeOut,
					  (unsigned long) prStaRec);

			cnmTimerStartTimer(prAdapter,
				&prStaRec->rTxReqDoneOrRxRespTimer,
				TU_TO_MSEC(TX_AUTHENTICATION_RETRY_TIMEOUT_TU));
		}

	}
}
#endif

/*----------------------------------------------------------------------------*/
/*!
* @brief The Core FSM engine of SAA Module.
*
* @param[in] prStaRec           Pointer to the STA_RECORD_T
* @param[in] eNextState         The value of Next State
* @param[in] prRetainedSwRfb    Pointer to the retained SW_RFB_T for JOIN Success
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID
saaFsmSteps(IN P_ADAPTER_T prAdapter,
	    IN P_STA_RECORD_T prStaRec, IN ENUM_AA_STATE_T eNextState, IN P_SW_RFB_T prRetainedSwRfb)
{
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;
	ENUM_AA_STATE_T ePreviousState;
	BOOLEAN fgIsTransition;

	ASSERT(prStaRec);
	if (!prStaRec || g_u4HaltFlag)
		return;

	do {

		DBGLOG(SAA, STATE, "[SAA]TRANSITION: [%s] -> [%s]\n",
			apucDebugAAState[prStaRec->eAuthAssocState], apucDebugAAState[eNextState]);

		ePreviousState = prStaRec->eAuthAssocState;

		/* NOTE(Kevin): This is the only place to change the eAuthAssocState(except initial) */
		prStaRec->eAuthAssocState = eNextState;

		fgIsTransition = (BOOLEAN) FALSE;
		switch (prStaRec->eAuthAssocState) {
		case AA_STATE_IDLE:

			if (ePreviousState != prStaRec->eAuthAssocState) {	/* Only trigger this event once */

				if (prRetainedSwRfb) {
					if (saaFsmSendEventJoinComplete(prAdapter,
									WLAN_STATUS_SUCCESS,
									prStaRec,
									prRetainedSwRfb) == WLAN_STATUS_SUCCESS) {
						/* ToDo:: Nothing */
					} else {
						eNextState = AA_STATE_RESOURCE;
						fgIsTransition = TRUE;
					}
				} else {
					if (saaFsmSendEventJoinComplete(prAdapter,
									WLAN_STATUS_FAILURE,
									prStaRec, NULL) == WLAN_STATUS_RESOURCES) {
						eNextState = AA_STATE_RESOURCE;
						fgIsTransition = TRUE;
					}
				}

			}

			/* Free allocated TCM memory */
			if (prStaRec->prChallengeText) {
				cnmMemFree(prAdapter, prStaRec->prChallengeText);
				prStaRec->prChallengeText = (P_IE_CHALLENGE_TEXT_T) NULL;
			}

			break;

		case SAA_STATE_SEND_AUTH1:

			/* Do tasks in INIT STATE */
			if (prStaRec->ucTxAuthAssocRetryCount >= prStaRec->ucTxAuthAssocRetryLimit) {

				/* Record the Status Code of Authentication Request */
				prStaRec->u2StatusCode = STATUS_CODE_AUTH_TIMEOUT;

				eNextState = AA_STATE_IDLE;
				fgIsTransition = TRUE;
			} else {
				prStaRec->ucTxAuthAssocRetryCount++;

				/* Update Station Record - Class 1 Flag */
				cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_1);

#if !CFG_SUPPORT_AAA
				rStatus = authSendAuthFrame(prAdapter, prStaRec, AUTH_TRANSACTION_SEQ_1);
#else
				rStatus = authSendAuthFrame(prAdapter,
						      prStaRec,
						      prStaRec->ucBssIndex,
						      NULL,
						      AUTH_TRANSACTION_SEQ_1,
						      STATUS_CODE_RESERVED);
#endif /* CFG_SUPPORT_AAA */
				if (rStatus != WLAN_STATUS_SUCCESS) {
					cnmTimerInitTimer(prAdapter,
							  &prStaRec->rTxReqDoneOrRxRespTimer, (PFN_MGMT_TIMEOUT_FUNC)
							  saaFsmRunEventTxReqTimeOut, (ULONG) prStaRec);

					cnmTimerStartTimer(prAdapter,
							   &prStaRec->rTxReqDoneOrRxRespTimer,
							   TU_TO_MSEC(TX_AUTHENTICATION_RETRY_TIMEOUT_TU));
				}
			}

			break;

		case SAA_STATE_WAIT_AUTH2:
			break;

		case SAA_STATE_SEND_AUTH3:

			/* Do tasks in INIT STATE */
			if (prStaRec->ucTxAuthAssocRetryCount >= prStaRec->ucTxAuthAssocRetryLimit) {

				/* Record the Status Code of Authentication Request */
				prStaRec->u2StatusCode = STATUS_CODE_AUTH_TIMEOUT;

				eNextState = AA_STATE_IDLE;
				fgIsTransition = TRUE;
			} else {
				prStaRec->ucTxAuthAssocRetryCount++;

#if !CFG_SUPPORT_AAA
				rStatus = authSendAuthFrame(prAdapter, prStaRec, AUTH_TRANSACTION_SEQ_3);
#else
				rStatus = authSendAuthFrame(prAdapter,
						      prStaRec,
						      prStaRec->ucBssIndex,
						      NULL,
						      AUTH_TRANSACTION_SEQ_3,
						      STATUS_CODE_RESERVED);
#endif /* CFG_SUPPORT_AAA */
				if (rStatus != WLAN_STATUS_SUCCESS) {
					cnmTimerInitTimer(prAdapter,
							  &prStaRec->rTxReqDoneOrRxRespTimer, (PFN_MGMT_TIMEOUT_FUNC)
							  saaFsmRunEventTxReqTimeOut, (ULONG) prStaRec);

					cnmTimerStartTimer(prAdapter,
							   &prStaRec->rTxReqDoneOrRxRespTimer,
							   TU_TO_MSEC(TX_AUTHENTICATION_RETRY_TIMEOUT_TU));
				}
			}

			break;

		case SAA_STATE_WAIT_AUTH4:
			break;

		case SAA_STATE_SEND_ASSOC1:
			/* Do tasks in INIT STATE */
			if (prStaRec->ucTxAuthAssocRetryCount >= prStaRec->ucTxAuthAssocRetryLimit) {

				/* Record the Status Code of Authentication Request */
				prStaRec->u2StatusCode = STATUS_CODE_ASSOC_TIMEOUT;

				eNextState = AA_STATE_IDLE;
				fgIsTransition = TRUE;
			} else {
				prStaRec->ucTxAuthAssocRetryCount++;

				rStatus = assocSendReAssocReqFrame(prAdapter, prStaRec);
				if (rStatus != WLAN_STATUS_SUCCESS) {
					cnmTimerInitTimer(prAdapter,
							  &prStaRec->rTxReqDoneOrRxRespTimer, (PFN_MGMT_TIMEOUT_FUNC)
							  saaFsmRunEventTxReqTimeOut, (ULONG) prStaRec);

					cnmTimerStartTimer(prAdapter,
							   &prStaRec->rTxReqDoneOrRxRespTimer,
							   TU_TO_MSEC(TX_ASSOCIATION_RETRY_TIMEOUT_TU));
				}
			}

			break;

		case SAA_STATE_WAIT_ASSOC2:
			break;

		case AA_STATE_RESOURCE:
			/* TODO(Kevin) Can setup a timer and send message later */
			break;

		default:
			DBGLOG(SAA, ERROR, "Unknown AA STATE\n");
			ASSERT(0);
			break;
		}

	} while (fgIsTransition);

	return;

}				/* end of saaFsmSteps() */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will send Event to AIS/BOW/P2P
*
* @param[in] rJoinStatus        To indicate JOIN success or failure.
* @param[in] prStaRec           Pointer to the STA_RECORD_T
* @param[in] prSwRfb            Pointer to the SW_RFB_T

* @return (none)
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
saaFsmSendEventJoinComplete(IN P_ADAPTER_T prAdapter,
			    IN WLAN_STATUS rJoinStatus, IN P_STA_RECORD_T prStaRec, IN P_SW_RFB_T prSwRfb)
{
	P_BSS_INFO_T prBssInfo;

	ASSERT(prStaRec);
	if (!prStaRec) {
		DBGLOG(SAA, ERROR, "[%s]prStaRec is NULL\n", __func__);
		return WLAN_STATUS_INVALID_PACKET;
	}
	if (!prAdapter) {
		DBGLOG(SAA, ERROR, "[%s]prAdapter is NULL\n", __func__);
		return WLAN_STATUS_INVALID_PACKET;
	}
	if (!prAdapter->prAisBssInfo) {
		DBGLOG(SAA, ERROR, "[%s]prAdapter->prAisBssInfo is NULL\n", __func__);
		return WLAN_STATUS_INVALID_PACKET;
	}

	/* Store limitation about 40Mhz bandwidth capability during association */
	if (prStaRec->ucBssIndex < BSS_INFO_NUM) {
		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);

		if (rJoinStatus == WLAN_STATUS_SUCCESS) {
			prBssInfo->fg40mBwAllowed = prBssInfo->fgAssoc40mBwAllowed;
			/* Initialize OpMode Channel Width change indicator */
			prBssInfo->fgIsOpChangeChannelWidth = FALSE;
		}
		prBssInfo->fgAssoc40mBwAllowed = FALSE;
	}

	if (prStaRec->ucBssIndex == prAdapter->prAisBssInfo->ucBssIndex) {
		P_MSG_SAA_FSM_COMP_T prSaaFsmCompMsg;

		prSaaFsmCompMsg = cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_SAA_FSM_COMP_T));
		if (!prSaaFsmCompMsg)
			return WLAN_STATUS_RESOURCES;
#if CFG_SUPPORT_CFG80211_AUTH
		if (rJoinStatus == WLAN_STATUS_SUCCESS)
			prStaRec->u2StatusCode = STATUS_CODE_SUCCESSFUL;
#endif
		prSaaFsmCompMsg->rMsgHdr.eMsgId = MID_SAA_AIS_JOIN_COMPLETE;
		prSaaFsmCompMsg->ucSeqNum = prStaRec->ucAuthAssocReqSeqNum;
		prSaaFsmCompMsg->rJoinStatus = rJoinStatus;
		prSaaFsmCompMsg->prStaRec = prStaRec;
		prSaaFsmCompMsg->prSwRfb = prSwRfb;

		/* NOTE(Kevin): Set to UNBUF for immediately JOIN complete */
		mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prSaaFsmCompMsg, MSG_SEND_METHOD_UNBUF);

		return WLAN_STATUS_SUCCESS;
	}
#if CFG_ENABLE_WIFI_DIRECT
	else if ((prAdapter->fgIsP2PRegistered) && (IS_STA_IN_P2P(prStaRec))) {
		P_MSG_SAA_FSM_COMP_T prSaaFsmCompMsg;

		prSaaFsmCompMsg = cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_SAA_FSM_COMP_T));
		if (!prSaaFsmCompMsg)
			return WLAN_STATUS_RESOURCES;
#if CFG_SUPPORT_CFG80211_AUTH
		if (rJoinStatus == WLAN_STATUS_SUCCESS)
			prStaRec->u2StatusCode = STATUS_CODE_SUCCESSFUL;
#endif
		prSaaFsmCompMsg->rMsgHdr.eMsgId = MID_SAA_P2P_JOIN_COMPLETE;
		prSaaFsmCompMsg->ucSeqNum = prStaRec->ucAuthAssocReqSeqNum;
		prSaaFsmCompMsg->rJoinStatus = rJoinStatus;
		prSaaFsmCompMsg->prStaRec = prStaRec;
		prSaaFsmCompMsg->prSwRfb = prSwRfb;

		/* NOTE(Kevin): Set to UNBUF for immediately JOIN complete */
		mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prSaaFsmCompMsg, MSG_SEND_METHOD_UNBUF);

		return WLAN_STATUS_SUCCESS;
	}
#endif
#if CFG_ENABLE_BT_OVER_WIFI
	else if (IS_STA_BOW_TYPE(prStaRec)) {
		/* @TODO: BOW handler */

		P_MSG_SAA_FSM_COMP_T prSaaFsmCompMsg;

		prSaaFsmCompMsg = cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_SAA_FSM_COMP_T));
		if (!prSaaFsmCompMsg)
			return WLAN_STATUS_RESOURCES;

		prSaaFsmCompMsg->rMsgHdr.eMsgId = MID_SAA_BOW_JOIN_COMPLETE;
		prSaaFsmCompMsg->ucSeqNum = prStaRec->ucAuthAssocReqSeqNum;
		prSaaFsmCompMsg->rJoinStatus = rJoinStatus;
		prSaaFsmCompMsg->prStaRec = prStaRec;
		prSaaFsmCompMsg->prSwRfb = prSwRfb;

		/* NOTE(Kevin): Set to UNBUF for immediately JOIN complete */
		mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prSaaFsmCompMsg, MSG_SEND_METHOD_UNBUF);

		return WLAN_STATUS_SUCCESS;
	}
#endif
	else {
		DBGLOG(SAA, ERROR, "Invalid case in %s.\n", __func__);
		return WLAN_STATUS_FAILURE;
	}

}				/* end of saaFsmSendEventJoinComplete() */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will handle the Start Event to SAA FSM.
*
* @param[in] prMsgHdr   Message of Join Request for a particular STA.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID saaFsmRunEventStart(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_MSG_SAA_FSM_START_T prSaaFsmStartMsg;
	P_STA_RECORD_T prStaRec;
	P_BSS_INFO_T prBssInfo;

	ASSERT(prAdapter);
	ASSERT(prMsgHdr);

	prSaaFsmStartMsg = (P_MSG_SAA_FSM_START_T) prMsgHdr;
	prStaRec = prSaaFsmStartMsg->prStaRec;

	if ((!prStaRec) || (prStaRec->fgIsInUse == FALSE)) {
		cnmMemFree(prAdapter, prMsgHdr);
		return;
	}

	ASSERT(prStaRec);

	DBGLOG(SAA, LOUD, "EVENT-START: Trigger SAA FSM.\n");

	/* record sequence number of request message */
	prStaRec->ucAuthAssocReqSeqNum = prSaaFsmStartMsg->ucSeqNum;

	cnmMemFree(prAdapter, prMsgHdr);

	/* 4 <1> Validation of SAA Start Event */
	if (!IS_AP_STA(prStaRec)) {

		DBGLOG(SAA, ERROR, "EVENT-START: STA Type - %d was not supported.\n", prStaRec->eStaType);

		/* Ignore the return value because don't care the prSwRfb */
		saaFsmSendEventJoinComplete(prAdapter, WLAN_STATUS_FAILURE, prStaRec, NULL);

		return;
	}
	/* 4 <2> The previous JOIN process is not completed ? */
	if (prStaRec->eAuthAssocState != AA_STATE_IDLE) {
		DBGLOG(SAA, ERROR, "EVENT-START: Reentry of SAA Module.\n");
		prStaRec->eAuthAssocState = AA_STATE_IDLE;
	}

	/* 4 <3> Reset Status Code and Time */
	/* Update Station Record - Status/Reason Code */
#if CFG_SUPPORT_CFG80211_AUTH
	prStaRec->u2StatusCode = STATUS_CODE_UNSPECIFIED_FAILURE;
#else
	prStaRec->u2StatusCode = STATUS_CODE_SUCCESSFUL;
#endif

	/* Update the record join time. */
	GET_CURRENT_SYSTIME(&prStaRec->rLastJoinTime);

	prStaRec->ucTxAuthAssocRetryCount = 0;

	if (prStaRec->prChallengeText) {
		cnmMemFree(prAdapter, prStaRec->prChallengeText);
		prStaRec->prChallengeText = (P_IE_CHALLENGE_TEXT_T) NULL;
	}

	cnmTimerStopTimer(prAdapter, &prStaRec->rTxReqDoneOrRxRespTimer);

	/* 4 <4> Init the sec fsm */
	/* secFsmInit(prAdapter, prStaRec); */

	/* 4 <5> Reset the STA STATE */
	/* Update Station Record - Class 1 Flag */
	/* NOTE(Kevin): Moved to AIS FSM for Reconnect issue -
	 * We won't deactivate the same STA_RECORD_T and then activate it again for the
	 * case of reconnection.
	 */
	/* cnmStaRecChangeState(prStaRec, STA_STATE_1); */

	/* 4 <6> Decide if this BSS 20/40M bandwidth is allowed */
	if (prStaRec->ucBssIndex < BSS_INFO_NUM) {
		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);

		if ((prAdapter->rWifiVar.ucAvailablePhyTypeSet & PHY_TYPE_SET_802_11N)
		    && (prStaRec->ucPhyTypeSet & PHY_TYPE_SET_802_11N)) {
			prBssInfo->fgAssoc40mBwAllowed = cnmBss40mBwPermitted(prAdapter, prBssInfo->ucBssIndex);
		} else {
			prBssInfo->fgAssoc40mBwAllowed = FALSE;
		}
		DBGLOG(RLM, TRACE, "STA 40mAllowed=%d\n", prBssInfo->fgAssoc40mBwAllowed);
	}

#if CFG_SUPPORT_CFG80211_AUTH
	if (!IS_STA_IN_P2P(prStaRec)) {
		/* skip SAA FSM */
		prStaRec->eAuthAssocSent = AA_SENT_NONE;
		saaSendAuthAssoc(prAdapter, prStaRec);
	} else {
#endif
	/* 4 <7> Trigger SAA FSM */
	if (prStaRec->ucStaState == STA_STATE_1)
		saaFsmSteps(prAdapter, prStaRec, SAA_STATE_SEND_AUTH1, (P_SW_RFB_T) NULL);
	else if (prStaRec->ucStaState == STA_STATE_2 || prStaRec->ucStaState == STA_STATE_3)
		saaFsmSteps(prAdapter, prStaRec, SAA_STATE_SEND_ASSOC1, (P_SW_RFB_T) NULL);
#if CFG_SUPPORT_CFG80211_AUTH
	}
#endif
}				/* end of saaFsmRunEventStart() */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will handle TxDone(Auth1/Auth3/AssocReq) Event of SAA FSM.
*
* @param[in] prMsduInfo     Pointer to the MSDU_INFO_T.
* @param[in] rTxDoneStatus  Return TX status of the Auth1/Auth3/AssocReq frame.
*
* @retval WLAN_STATUS_SUCCESS
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
saaFsmRunEventTxDone(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_RESULT_CODE_T rTxDoneStatus)
{

	P_STA_RECORD_T prStaRec;
	ENUM_AA_STATE_T eNextState;

	ASSERT(prMsduInfo);

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

	if (!prStaRec)
		return WLAN_STATUS_INVALID_PACKET;

	if ((rTxDoneStatus == TX_RESULT_DROPPED_IN_DRIVER) ||
		(rTxDoneStatus == TX_RESULT_QUEUE_CLEARANCE))
		return WLAN_STATUS_SUCCESS;

	ASSERT(prStaRec);

	DBGLOG(SAA, LOUD, "EVENT-TX DONE: Current Time = %d\n", kalGetTimeTick());

	/* Trigger statistics log if Auth/Assoc Tx failed */
	if (rTxDoneStatus != TX_RESULT_SUCCESS)
		wlanTriggerStatsLog(prAdapter, prAdapter->rWifiVar.u4StatsLogDuration);

#if CFG_SUPPORT_CFG80211_AUTH
	if (!IS_STA_IN_P2P(prStaRec)) {
		/* check the outgoing frame is matched with
		 * the last sent frame, ignore the unmatched txdone
		 */
		if ((prStaRec->eAuthAssocSent >= AA_SENT_AUTH1) &&
			(prStaRec->eAuthAssocSent <= AA_SENT_AUTH4)) {
			if (authCheckTxAuthFrame(prAdapter, prMsduInfo,
				prStaRec->eAuthAssocSent) !=
				WLAN_STATUS_SUCCESS)
				return WLAN_STATUS_SUCCESS;
		} else if (prStaRec->eAuthAssocSent == AA_SENT_ASSOC1) {
			if (assocCheckTxReAssocReqFrame(prAdapter,
				prMsduInfo) != WLAN_STATUS_SUCCESS)
				return WLAN_STATUS_SUCCESS;
		} else
			DBGLOG(SAA, WARN, "unexpected sent frame = %d\n",
				prStaRec->eAuthAssocSent);

		cnmTimerStopTimer(prAdapter,
			&prStaRec->rTxReqDoneOrRxRespTimer);

		if (rTxDoneStatus == TX_RESULT_SUCCESS) {
			cnmTimerInitTimer(prAdapter,
				&prStaRec->rTxReqDoneOrRxRespTimer,
				(PFN_MGMT_TIMEOUT_FUNC)
				saaFsmRunEventRxRespTimeOut,
				(unsigned long) prStaRec);
#if CFG_SUPPORT_CFG80211_AUTH
			if (prAdapter->prGlueInfo->rWpaInfo.u4AuthAlg &
				AUTH_TYPE_SAE)
				cnmTimerStartTimer(prAdapter,
					&prStaRec->rTxReqDoneOrRxRespTimer,
					TU_TO_MSEC(
					DOT11_RSNA_SAE_RETRANS_PERIOD_TU));
			else
#endif
				cnmTimerStartTimer(prAdapter,
				&prStaRec->rTxReqDoneOrRxRespTimer,
				TU_TO_MSEC(
				DOT11_AUTHENTICATION_RESPONSE_TIMEOUT_TU));
		} else /* Tx failed, do retry if possible */
			saaSendAuthAssoc(prAdapter, prStaRec);
	} else {
#endif
	eNextState = prStaRec->eAuthAssocState;

	switch (prStaRec->eAuthAssocState) {
	case SAA_STATE_SEND_AUTH1:
		{
			/* Strictly check the outgoing frame is matched with current AA STATE */
			if (authCheckTxAuthFrame(prAdapter, prMsduInfo, AUTH_TRANSACTION_SEQ_1) != WLAN_STATUS_SUCCESS)
				break;

			if (rTxDoneStatus == TX_RESULT_SUCCESS) {
				eNextState = SAA_STATE_WAIT_AUTH2;

				cnmTimerStopTimer(prAdapter, &prStaRec->rTxReqDoneOrRxRespTimer);

				cnmTimerInitTimer(prAdapter, &prStaRec->rTxReqDoneOrRxRespTimer, (PFN_MGMT_TIMEOUT_FUNC)
						  saaFsmRunEventRxRespTimeOut, (ULONG) prStaRec);
#if CFG_SUPPORT_CFG80211_AUTH
			if (prAdapter->prGlueInfo
				->rWpaInfo.u4AuthAlg & AUTH_TYPE_SAE)
				cnmTimerStartTimer(prAdapter,
					&prStaRec->rTxReqDoneOrRxRespTimer,
				TU_TO_MSEC(DOT11_RSNA_SAE_RETRANS_PERIOD_TU));
			else
#endif
				cnmTimerStartTimer(prAdapter,
						   &prStaRec->rTxReqDoneOrRxRespTimer,
						   TU_TO_MSEC(DOT11_AUTHENTICATION_RESPONSE_TIMEOUT_TU));
			}

			/* if TX was successful, change to next state.
			 * if TX was failed, do retry if possible.
			 */
			saaFsmSteps(prAdapter, prStaRec, eNextState, (P_SW_RFB_T) NULL);
		}
		break;

	case SAA_STATE_SEND_AUTH3:
		{
			/* Strictly check the outgoing frame is matched with current JOIN STATE */
			if (authCheckTxAuthFrame(prAdapter, prMsduInfo, AUTH_TRANSACTION_SEQ_3) != WLAN_STATUS_SUCCESS)
				break;

			if (rTxDoneStatus == TX_RESULT_SUCCESS) {
				eNextState = SAA_STATE_WAIT_AUTH4;

				cnmTimerStopTimer(prAdapter, &prStaRec->rTxReqDoneOrRxRespTimer);

				cnmTimerInitTimer(prAdapter, &prStaRec->rTxReqDoneOrRxRespTimer, (PFN_MGMT_TIMEOUT_FUNC)
						  saaFsmRunEventRxRespTimeOut, (ULONG) prStaRec);

				cnmTimerStartTimer(prAdapter,
						   &prStaRec->rTxReqDoneOrRxRespTimer,
						   TU_TO_MSEC(DOT11_AUTHENTICATION_RESPONSE_TIMEOUT_TU));
			}

			/* if TX was successful, change to next state.
			 * if TX was failed, do retry if possible.
			 */
			saaFsmSteps(prAdapter, prStaRec, eNextState, (P_SW_RFB_T) NULL);
		}
		break;

	case SAA_STATE_SEND_ASSOC1:
		{
			/* Strictly check the outgoing frame is matched with current SAA STATE */
			if (assocCheckTxReAssocReqFrame(prAdapter, prMsduInfo) != WLAN_STATUS_SUCCESS)
				break;

			if (rTxDoneStatus == TX_RESULT_SUCCESS) {
				eNextState = SAA_STATE_WAIT_ASSOC2;

				cnmTimerStopTimer(prAdapter, &prStaRec->rTxReqDoneOrRxRespTimer);

				cnmTimerInitTimer(prAdapter, &prStaRec->rTxReqDoneOrRxRespTimer, (PFN_MGMT_TIMEOUT_FUNC)
						  saaFsmRunEventRxRespTimeOut, (ULONG) prStaRec);

				cnmTimerStartTimer(prAdapter,
						   &(prStaRec->rTxReqDoneOrRxRespTimer),
						   TU_TO_MSEC(DOT11_ASSOCIATION_RESPONSE_TIMEOUT_TU));
			}

			/* if TX was successful, change to next state.
			 * if TX was failed, do retry if possible.
			 */
			saaFsmSteps(prAdapter, prStaRec, eNextState, (P_SW_RFB_T) NULL);
		}
		break;

	default:
		break;		/* Ignore other cases */
	}
#if CFG_SUPPORT_CFG80211_AUTH
	}
#endif
	return WLAN_STATUS_SUCCESS;

}				/* end of saaFsmRunEventTxDone() */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will send Tx Request Timeout Event to SAA FSM.
*
* @param[in] prStaRec           Pointer to the STA_RECORD_T
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID saaFsmRunEventTxReqTimeOut(IN P_ADAPTER_T prAdapter, IN ULONG plParamPtr)
{
	P_STA_RECORD_T prStaRec = (P_STA_RECORD_T) plParamPtr;

	ASSERT(prStaRec);
	if (!prStaRec)
		return;

	DBGLOG(SAA, LOUD, "EVENT-TIMER: TX REQ TIMEOUT, Current Time = %d\n", kalGetTimeTick());

	/* Trigger statistics log if Auth/Assoc Tx timeout */
	wlanTriggerStatsLog(prAdapter, prAdapter->rWifiVar.u4StatsLogDuration);
#if CFG_SUPPORT_CFG80211_AUTH
	if (!IS_STA_IN_P2P(prStaRec)) {
		saaSendAuthAssoc(prAdapter, prStaRec);
	} else {
#endif
	switch (prStaRec->eAuthAssocState) {
	case SAA_STATE_SEND_AUTH1:
	case SAA_STATE_SEND_AUTH3:
	case SAA_STATE_SEND_ASSOC1:
		saaFsmSteps(prAdapter, prStaRec, prStaRec->eAuthAssocState, (P_SW_RFB_T) NULL);
		break;

	default:
		return;
	}
#if CFG_SUPPORT_CFG80211_AUTH
	}
#endif
}				/* end of saaFsmRunEventTxReqTimeOut() */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will send Rx Response Timeout Event to SAA FSM.
*
* @param[in] prStaRec           Pointer to the STA_RECORD_T
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID saaFsmRunEventRxRespTimeOut(IN P_ADAPTER_T prAdapter, IN ULONG ulParamPtr)
{
	P_STA_RECORD_T prStaRec = (P_STA_RECORD_T) ulParamPtr;
	ENUM_AA_STATE_T eNextState;
	DBGLOG(SAA, LOUD, "EVENT-TIMER: RX RESP TIMEOUT, Current Time = %d\n", kalGetTimeTick());

	ASSERT(prStaRec);
	if (!prStaRec)
		return;

#if CFG_SUPPORT_CFG80211_AUTH
	if (!IS_STA_IN_P2P(prStaRec)) {
		/* Retry the last sent frame if possible */
		saaSendAuthAssoc(prAdapter, prStaRec);
	} else {
#endif
	eNextState = prStaRec->eAuthAssocState;

	switch (prStaRec->eAuthAssocState) {
	case SAA_STATE_WAIT_AUTH2:
		/* Record the Status Code of Authentication Request */
		prStaRec->u2StatusCode = STATUS_CODE_AUTH_TIMEOUT;

		/* Pull back to earlier state to do retry */
		eNextState = SAA_STATE_SEND_AUTH1;
		break;

	case SAA_STATE_WAIT_AUTH4:
		/* Record the Status Code of Authentication Request */
		prStaRec->u2StatusCode = STATUS_CODE_AUTH_TIMEOUT;

		/* Pull back to earlier state to do retry */
		eNextState = SAA_STATE_SEND_AUTH3;
		break;

	case SAA_STATE_WAIT_ASSOC2:
		/* Record the Status Code of Authentication Request */
		prStaRec->u2StatusCode = STATUS_CODE_ASSOC_TIMEOUT;

		/* Pull back to earlier state to do retry */
		eNextState = SAA_STATE_SEND_ASSOC1;
		break;

	default:
		break;		/* Ignore other cases */
	}

	if (eNextState != prStaRec->eAuthAssocState)
		saaFsmSteps(prAdapter, prStaRec, eNextState, (P_SW_RFB_T) NULL);
#if CFG_SUPPORT_CFG80211_AUTH
	}
#endif
}				/* end of saaFsmRunEventRxRespTimeOut() */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will process the Rx Auth Response Frame and then
*        trigger SAA FSM.
*
* @param[in] prSwRfb            Pointer to the SW_RFB_T structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID saaFsmRunEventRxAuth(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
	P_STA_RECORD_T prStaRec;
	UINT_16 u2StatusCode;
	ENUM_AA_STATE_T eNextState;
	UINT_8 ucWlanIdx;
#if CFG_SUPPORT_CFG80211_AUTH
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_WLAN_AUTH_FRAME_T prAuthFrame = (P_WLAN_AUTH_FRAME_T) NULL;
#endif

	ASSERT(prSwRfb);
	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);
	ucWlanIdx = (UINT_8) HAL_RX_STATUS_GET_WLAN_IDX(prSwRfb->prRxStatus);
#if CFG_SUPPORT_CFG80211_AUTH
	prGlueInfo = prAdapter->prGlueInfo;
	ASSERT(prGlueInfo);
#endif

	/* We should have the corresponding Sta Record. */
	 if (!prStaRec) {
		DBGLOG(SAA, WARN, "Received a AuthResp: wlanIdx[%d] w/o corresponding staRec\n", ucWlanIdx);
		return;
	}

	if (!IS_AP_STA(prStaRec))
		return;

#if CFG_SUPPORT_CFG80211_AUTH
	/* check received auth frame */
	if ((authCheckRxAuthFrameStatus(prAdapter, prSwRfb,
		prStaRec->eAuthAssocSent, &u2StatusCode) ==
		WLAN_STATUS_SUCCESS) && (!(IS_STA_IN_P2P(prStaRec)))) {

		cnmTimerStopTimer(prAdapter,
			&prStaRec->rTxReqDoneOrRxRespTimer);

		/* Record the Status Code of Authentication Request */
		prStaRec->u2StatusCode = u2StatusCode;

		/*Report Rx auth frame to upper layer*/
		prAuthFrame = (P_WLAN_AUTH_FRAME_T) prSwRfb->pvHeader;

		DBGLOG(INIT, INFO, "Dump rx auth data\n");
		DBGLOG_MEM8(REQ, INFO, prAuthFrame, prSwRfb->u2PacketLen);

		DBGLOG(SAA, INFO,
		"Report RX auth to upper layer with alg:%d, SN:%d, status:%d\n",
		prAuthFrame->u2AuthAlgNum, prAuthFrame->aucAuthData[0],
		prAuthFrame->aucAuthData[2]);

		cfg80211_rx_mlme_mgmt(prGlueInfo->prDevHandler,
			(const u8 *)prAuthFrame, (size_t)prSwRfb->u2PacketLen);
		DBGLOG(SAA, INFO, "notification of RX Authentication Done\n");

		/* Reset Send Auth/(Re)Assoc Frame Count */
		prStaRec->ucTxAuthAssocRetryCount = 0;
		if (u2StatusCode == STATUS_CODE_SUCCESSFUL) {

			authProcessRxAuth2_Auth4Frame(prAdapter, prSwRfb);
		} else {
			DBGLOG(SAA, INFO,
				"Auth Req was rejected by [" MACSTR
				"], Status Code = %d\n",
				MAC2STR(prStaRec->aucMacAddr), u2StatusCode);

		/* AIS retry JOIN or indicate JOIN FAILURE to upper layer*/
			if (saaFsmSendEventJoinComplete(prAdapter,
				WLAN_STATUS_FAILURE, prStaRec, NULL) ==
				WLAN_STATUS_RESOURCES) {
			/* can set a timer and retry later */
			DBGLOG(SAA, WARN,
			"[SAA]can't alloc msg for inform AIS join complete\n");
			}
		}
	} else {
#endif
	switch (prStaRec->eAuthAssocState) {
	case SAA_STATE_SEND_AUTH1:
	case SAA_STATE_WAIT_AUTH2:
		/* Check if the incoming frame is what we are waiting for */
		if (authCheckRxAuthFrameStatus(prAdapter,
					       prSwRfb, AUTH_TRANSACTION_SEQ_2, &u2StatusCode) == WLAN_STATUS_SUCCESS) {

			cnmTimerStopTimer(prAdapter, &prStaRec->rTxReqDoneOrRxRespTimer);

			/* Record the Status Code of Authentication Request */
			prStaRec->u2StatusCode = u2StatusCode;

			if (u2StatusCode == STATUS_CODE_SUCCESSFUL) {

				authProcessRxAuth2_Auth4Frame(prAdapter, prSwRfb);

				if (prStaRec->ucAuthAlgNum == (UINT_8) AUTH_ALGORITHM_NUM_SHARED_KEY) {

					eNextState = SAA_STATE_SEND_AUTH3;
				} else {
					/* Update Station Record - Class 2 Flag */
					cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_2);

					eNextState = SAA_STATE_SEND_ASSOC1;
				}
			} else {
				DBGLOG(SAA, INFO,
				       "Auth Req was rejected by [" MACSTR "], Status Code = %d\n",
				       MAC2STR(prStaRec->aucMacAddr), u2StatusCode);

				eNextState = AA_STATE_IDLE;
			}

			/* Reset Send Auth/(Re)Assoc Frame Count */
			prStaRec->ucTxAuthAssocRetryCount = 0;

			saaFsmSteps(prAdapter, prStaRec, eNextState, (P_SW_RFB_T) NULL);
		}
		break;

	case SAA_STATE_SEND_AUTH3:
	case SAA_STATE_WAIT_AUTH4:
		/* Check if the incoming frame is what we are waiting for */
		if (authCheckRxAuthFrameStatus(prAdapter,
					       prSwRfb, AUTH_TRANSACTION_SEQ_4, &u2StatusCode) == WLAN_STATUS_SUCCESS) {

			cnmTimerStopTimer(prAdapter, &prStaRec->rTxReqDoneOrRxRespTimer);

			/* Record the Status Code of Authentication Request */
			prStaRec->u2StatusCode = u2StatusCode;

			if (u2StatusCode == STATUS_CODE_SUCCESSFUL) {

				authProcessRxAuth2_Auth4Frame(prAdapter, prSwRfb);	/* Add for 802.11r handling */

				/* Update Station Record - Class 2 Flag */
				cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_2);

				eNextState = SAA_STATE_SEND_ASSOC1;
			} else {
				DBGLOG(SAA, INFO,
				       "Auth Req was rejected by [" MACSTR "], Status Code = %d\n",
				       MAC2STR(prStaRec->aucMacAddr), u2StatusCode);

				eNextState = AA_STATE_IDLE;
			}

			/* Reset Send Auth/(Re)Assoc Frame Count */
			prStaRec->ucTxAuthAssocRetryCount = 0;

			saaFsmSteps(prAdapter, prStaRec, eNextState, (P_SW_RFB_T) NULL);
		}
		break;

	default:
		break;		/* Ignore other cases */
	}
#if CFG_SUPPORT_CFG80211_AUTH
	}
#endif
}				/* end of saaFsmRunEventRxAuth() */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will process the Rx (Re)Association Response Frame and then
*        trigger SAA FSM.
*
* @param[in] prSwRfb            Pointer to the SW_RFB_T structure.
*
* @retval WLAN_STATUS_SUCCESS           if the status code was not success
* @retval WLAN_STATUS_BUFFER_RETAINED   if the status code was success
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS saaFsmRunEventRxAssoc(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
	P_STA_RECORD_T prStaRec;
	UINT_16 u2StatusCode;
	ENUM_AA_STATE_T eNextState;
	P_SW_RFB_T prRetainedSwRfb = (P_SW_RFB_T) NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_8 ucWlanIdx;
#if CFG_SUPPORT_CFG80211_AUTH
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_WLAN_ASSOC_RSP_FRAME_T prAssocRspFrame = NULL;
	P_CONNECTION_SETTINGS_T prConnSettings = NULL;
#endif

	ASSERT(prSwRfb);
	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);
	ucWlanIdx = (UINT_8) HAL_RX_STATUS_GET_WLAN_IDX(prSwRfb->prRxStatus);
#if CFG_SUPPORT_CFG80211_AUTH
	prGlueInfo = prAdapter->prGlueInfo;
	ASSERT(prGlueInfo);
	prConnSettings = &prGlueInfo->prAdapter->rWifiVar.rConnSettings;
#endif
	DBGLOG(SAA, INFO, "RX Assoc Resp\n");

	/* We should have the corresponding Sta Record. */
	if (!prStaRec) {
		/* ASSERT(0); */
		DBGLOG(SAA, WARN, "Received a AssocResp: wlanIdx[%d] w/o corresponding staRec\n", ucWlanIdx);
		return rStatus;
	}

	if (!IS_AP_STA(prStaRec))
		return rStatus;

#if CFG_SUPPORT_CFG80211_AUTH
	/* TRUE if the incoming frame is what we are waiting for */
	if ((assocCheckRxReAssocRspFrameStatus(
		prAdapter, prSwRfb, &u2StatusCode) == WLAN_STATUS_SUCCESS) &&
		(!(IS_STA_IN_P2P(prStaRec)))) {

		cnmTimerStopTimer(prAdapter,
			&prStaRec->rTxReqDoneOrRxRespTimer);

		/* Record the Status Code of Authentication Request */
		prStaRec->u2StatusCode = u2StatusCode;

		/*Report Rx assoc frame to upper layer*/
		prAssocRspFrame = (P_WLAN_ASSOC_RSP_FRAME_T) prSwRfb->pvHeader;


		/* The BSS from cfg80211_ops.assoc must give back to
		 * cfg80211_send_rx_assoc() or to cfg80211_assoc_timeout().
		 * To ensure proper refcounting, new association requests
		 * while already associating must be rejected.
		 */
		DBGLOG(SAA, INFO, "Report RX Assoc to upper layer, %s\n",
			prConnSettings->bss ? "DO IT" : "Oops");
		if (prConnSettings->bss) {
#if KERNEL_VERSION(3, 18, 0) <= CFG80211_VERSION_CODE
			/* [TODO] Set uapsd_queues field to zero first,
			 * fill it if needed
			 */
			cfg80211_rx_assoc_resp(prGlueInfo->prDevHandler,
				prConnSettings->bss,
				(const u8 *)prAssocRspFrame,
				(size_t)prSwRfb->u2PacketLen, 0);
#else
			cfg80211_rx_assoc_resp(prGlueInfo->prDevHandler,
				prConnSettings->bss,
				(const u8 *)prAssocRspFrame,
				(size_t)prSwRfb->u2PacketLen);
#endif
			DBGLOG(SAA, INFO,
				"Report RX Assoc to upper layer, Done\n");
			prConnSettings->bss = NULL;
		} else
			DBGLOG(SAA, WARN,
				"Rx Assoc Resp without specific BSS\n");
		/* Reset Send Auth/(Re)Assoc Frame Count */
		prStaRec->ucTxAuthAssocRetryCount = 0;

		/* update RCPI */
		ASSERT(prSwRfb->prRxStatusGroup3);
		prStaRec->ucRCPI =
			nicRxGetRcpiValueFromRxv(RCPI_MODE_WF0, prSwRfb);

		if (u2StatusCode == STATUS_CODE_SUCCESSFUL) {

			/* Update Station Record - Class 3 Flag */
			/* NOTE(Kevin): Moved to AIS FSM for roaming issue
			 * We should deactivate the struct STA_RECORD of
			 * previous AP before activate new one in Driver.
			 */
			/* cnmStaRecChangeState(prStaRec, STA_STATE_3); */
			/* Clear history. */
			prStaRec->ucJoinFailureCount = 0;

			if (saaFsmSendEventJoinComplete(prAdapter,
				WLAN_STATUS_SUCCESS,
				prStaRec, prSwRfb) == WLAN_STATUS_RESOURCES) {
			/* can set a timer and retry later */
			DBGLOG(SAA, WARN,
			"[SAA]can't alloc msg for inform AIS join complete\n");
			}

			rStatus = WLAN_STATUS_PENDING;
		} else {
			DBGLOG(SAA, INFO,
			       "Assoc Req was rejected by [" MACSTR
			       "], Status Code = %d\n",
			       MAC2STR(prStaRec->aucMacAddr), u2StatusCode);

			if (saaFsmSendEventJoinComplete(prAdapter,
				WLAN_STATUS_FAILURE,
				prStaRec, NULL) == WLAN_STATUS_RESOURCES) {
			/* can set a timer and retry later */
			DBGLOG(SAA, WARN,
			"[SAA]can't alloc msg for inform AIS join complete\n");
			}
		}
	} else {
#endif
	switch (prStaRec->eAuthAssocState) {
	case SAA_STATE_SEND_ASSOC1:
	case SAA_STATE_WAIT_ASSOC2:
		/* TRUE if the incoming frame is what we are waiting for */
		if (assocCheckRxReAssocRspFrameStatus(prAdapter, prSwRfb, &u2StatusCode) == WLAN_STATUS_SUCCESS) {

			cnmTimerStopTimer(prAdapter, &prStaRec->rTxReqDoneOrRxRespTimer);

			/* Record the Status Code of Authentication Request */
			prStaRec->u2StatusCode = u2StatusCode;

			if (u2StatusCode == STATUS_CODE_SUCCESSFUL) {

				/* Update Station Record - Class 3 Flag */
				/* NOTE(Kevin): Moved to AIS FSM for roaming issue -
				 * We should deactivate the STA_RECORD_T of previous AP before
				 * activate new one in Driver.
				 */
				/* cnmStaRecChangeState(prStaRec, STA_STATE_3); */

				prStaRec->ucJoinFailureCount = 0;	/* Clear history. */

				prRetainedSwRfb = prSwRfb;
				rStatus = WLAN_STATUS_PENDING;
			} else {
				DBGLOG(SAA, INFO,
				       "Assoc Req was rejected by [" MACSTR
				       "], Status Code = %d\n", MAC2STR(prStaRec->aucMacAddr), u2StatusCode);
			}

			/* Reset Send Auth/(Re)Assoc Frame Count */
			prStaRec->ucTxAuthAssocRetryCount = 0;

			/* update RCPI */
			ASSERT(prSwRfb->prRxStatusGroup3);
			prStaRec->ucRCPI = nicRxGetRcpiValueFromRxv(RCPI_MODE_WF0, prSwRfb);

			eNextState = AA_STATE_IDLE;

			saaFsmSteps(prAdapter, prStaRec, eNextState, prRetainedSwRfb);
		}
		break;

	default:
		break;		/* Ignore other cases */
	}
#if CFG_SUPPORT_CFG80211_AUTH
	}
#endif
	return rStatus;

}				/* end of saaFsmRunEventRxAssoc() */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will check the incoming Deauth Frame.
*
* @param[in] prSwRfb            Pointer to the SW_RFB_T structure.
*
* @retval WLAN_STATUS_SUCCESS   Always not retain deauthentication frames
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS saaFsmRunEventRxDeauth(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
	P_STA_RECORD_T prStaRec;
	P_WLAN_DEAUTH_FRAME_T prDeauthFrame;
	UINT_8 ucWlanIdx;

	ASSERT(prSwRfb);
	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);
	prDeauthFrame = (P_WLAN_DEAUTH_FRAME_T) prSwRfb->pvHeader;
	ucWlanIdx = (UINT_8) HAL_RX_STATUS_GET_WLAN_IDX(prSwRfb->prRxStatus);

	DBGLOG(SAA, EVENT,
		"Rx Deauth frame ,DA[" MACSTR "] SA[" MACSTR "] BSSID["
		MACSTR "] ReasonCode[0x%x]\n",
	       MAC2STR(prDeauthFrame->aucDestAddr), MAC2STR(prDeauthFrame->aucSrcAddr),
	       MAC2STR(prDeauthFrame->aucBSSID), prDeauthFrame->u2ReasonCode);

	do {

		/* We should have the corresponding Sta Record. */
		if (!prStaRec) {
			DBGLOG(SAA, WARN, "Received a Deauth: wlanIdx[%d] w/o corresponding staRec\n", ucWlanIdx);
			break;
		}

		if (IS_STA_IN_AIS(prStaRec)) {
			P_BSS_INFO_T prAisBssInfo;

			if (!IS_AP_STA(prStaRec))
				break;

			prAisBssInfo = prAdapter->prAisBssInfo;

			if (prStaRec->ucStaState > STA_STATE_1) {

				/* Check if this is the AP we are associated or associating with */
				if (authProcessRxDeauthFrame(prSwRfb,
							     prStaRec->aucMacAddr,
							     &prStaRec->u2ReasonCode) == WLAN_STATUS_SUCCESS) {

#if CFG_SUPPORT_802_11W
					P_AIS_SPECIFIC_BSS_INFO_T prAisSpecBssInfo;

					prAisSpecBssInfo = &(prAdapter->rWifiVar.rAisSpecificBssInfo);

					DBGLOG(RSN, INFO,
					       "QM RX MGT: Deauth frame, P=%d Sec=%d CM=%d BC=%d fc=%02x\n",
					       prAisSpecBssInfo->
						fgMgmtProtection, (uint8_t)
						HAL_RX_STATUS_GET_SEC_MODE
						(prSwRfb->prRxStatus),
						HAL_RX_STATUS_IS_CIPHER_MISMATCH
						(prSwRfb->prRxStatus),
						IS_BMCAST_MAC_ADDR
						(prDeauthFrame->aucDestAddr),
						prDeauthFrame->u2FrameCtrl);
					if (prAisSpecBssInfo->fgMgmtProtection
					    && HAL_RX_STATUS_IS_CIPHER_MISMATCH(prSwRfb->prRxStatus)
					    /* HAL_RX_STATUS_GET_SEC_MODE(prSwRfb->prRxStatus) != CIPHER_SUITE_BIP */
					    ) {
						saaChkDeauthfrmParamHandler(prAdapter, prSwRfb, prStaRec);
						return WLAN_STATUS_SUCCESS;
					}
#endif
#if CFG_SUPPORT_CFG80211_AUTH
		DBGLOG(SAA, INFO, "notification of RX deauthentication %d\n",
			prSwRfb->u2PacketLen);
		cfg80211_rx_mlme_mgmt(prAdapter->prGlueInfo->prDevHandler,
			(PUINT_8)prDeauthFrame, (size_t)prSwRfb->u2PacketLen);
		DBGLOG(SAA, INFO, "notification of RX deauthentication Done\n");
#endif
#if CFG_DISCONN_DEBUG_FEATURE
					g_rDisconnInfoTemp.u2DisassocSeqNum =
						(prDeauthFrame->u2SeqCtrl &
						MASK_SC_SEQ_NUM) >>
						MASK_SC_SEQ_NUM_OFFSET;
#endif
					saaSendDisconnectMsgHandler(prAdapter, prStaRec, prAisBssInfo, FRM_DEAUTH);
				}
			}
		}
#if CFG_ENABLE_WIFI_DIRECT
		else if (prAdapter->fgIsP2PRegistered && IS_STA_IN_P2P(prStaRec)) {
			/* TODO(Kevin) */
#if CFG_SUPPORT_CFG80211_AUTH
		DBGLOG(SAA, INFO, "notification of RX deauthentication %d\n",
			prSwRfb->u2PacketLen);
		cfg80211_rx_mlme_mgmt(prAdapter->prGlueInfo->prDevHandler,
			(PUINT_8)prDeauthFrame, (size_t)prSwRfb->u2PacketLen);
		DBGLOG(SAA, INFO, "notification of RX deauthentication Done\n");
#endif
			p2pRoleFsmRunEventRxDeauthentication(prAdapter, prStaRec, prSwRfb);
		}
#endif
#if CFG_ENABLE_BT_OVER_WIFI
		else if (IS_STA_BOW_TYPE(prStaRec))
			bowRunEventRxDeAuth(prAdapter, prStaRec, prSwRfb);
#endif
		else
			ASSERT(0);

	} while (FALSE);

	return WLAN_STATUS_SUCCESS;

}				/* end of saaFsmRunEventRxDeauth() */

/* for AOSP */
/*----------------------------------------------------------------------------*/
/*!
* @brief This function will check param of deauth frame and reson code for deauth
*
* @param[in]
*
* @retval
*/
/*----------------------------------------------------------------------------*/

VOID saaChkDeauthfrmParamHandler(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb, IN P_STA_RECORD_T prStaRec)
{
	P_WLAN_DEAUTH_FRAME_T prDeauthFrame;

	do {
		prDeauthFrame = (P_WLAN_DEAUTH_FRAME_T) prSwRfb->pvHeader;
		if (!IS_BMCAST_MAC_ADDR(prDeauthFrame->aucDestAddr) &&
		    (prStaRec->u2ReasonCode == REASON_CODE_CLASS_2_ERR
		     || prStaRec->u2ReasonCode == REASON_CODE_CLASS_3_ERR)) {
			DBGLOG(RSN, INFO, "QM RX MGT: rsnStartSaQuery\n");
			/* MFP test plan 5.3.3.5 */
			rsnStartSaQuery(prAdapter);
		} else {
			DBGLOG(RSN, INFO, "RXM: Drop unprotected Mgmt frame\n");
			DBGLOG(RSN, INFO,
			       "RXM: (MAC RX Done) RX (u2StatusFlag=0x%x) (ucKIdxSecMode=0x%x) (ucWlanIdx=0x%x)\n",
			       prSwRfb->prRxStatus->u2StatusFlag,
			       prSwRfb->prRxStatus->ucTidSecMode, prSwRfb->prRxStatus->ucWlanIdx);
		}
	} while (0);
}

/* for AOSP */
/*----------------------------------------------------------------------------*/
/*!
* @brief This function will check and send disconnect message to AIS module
*
* @param[in]
*
* @retval
*/
/*----------------------------------------------------------------------------*/
VOID
saaSendDisconnectMsgHandler(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec, IN P_BSS_INFO_T prAisBssInfo,
			    IN ENUM_AA_FRM_TYPE_T eFrmType)
{
	do {
		if (eFrmType == FRM_DEAUTH) {
			if (prStaRec->ucStaState == STA_STATE_3) {
				P_MSG_AIS_ABORT_T prAisAbortMsg;

				/* NOTE(Kevin): Change state immediately to avoid starvation of
				 * MSG buffer because of too many deauth frames before changing
				 * the STA state.
				 */
				cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_1);

				prAisAbortMsg =
				    (P_MSG_AIS_ABORT_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_AIS_ABORT_T));
				if (!prAisAbortMsg)
					break;

				prAisAbortMsg->rMsgHdr.eMsgId = MID_SAA_AIS_FSM_ABORT;
				prAisAbortMsg->ucReasonOfDisconnect = DISCONNECT_REASON_CODE_DEAUTHENTICATED;
				prAisAbortMsg->fgDelayIndication = FALSE;
				mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prAisAbortMsg, MSG_SEND_METHOD_BUF);
			} else {
				/* TODO(Kevin): Joining Abort */
			}
		} else {	/* FRM_DISASSOC */
			if (prStaRec->ucStaState == STA_STATE_3) {
				P_MSG_AIS_ABORT_T prAisAbortMsg;

				prAisAbortMsg = (P_MSG_AIS_ABORT_T) cnmMemAlloc(prAdapter,
										RAM_TYPE_MSG, sizeof(MSG_AIS_ABORT_T));
				if (!prAisAbortMsg)
					break;

				prAisAbortMsg->rMsgHdr.eMsgId = MID_SAA_AIS_FSM_ABORT;
				prAisAbortMsg->ucReasonOfDisconnect = DISCONNECT_REASON_CODE_DISASSOCIATED;
				prAisAbortMsg->fgDelayIndication = FALSE;
				mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prAisAbortMsg, MSG_SEND_METHOD_BUF);
			} else {
				/* TODO(Kevin): Joining Abort */
			}
		}
		if (prAisBssInfo)
			prAisBssInfo->u2DeauthReason = prStaRec->u2ReasonCode;
#if CFG_DISCONN_DEBUG_FEATURE
			g_rDisconnInfoTemp.ucTrigger =
				DISCONNECT_TRIGGER_PASSIVE;
			g_rDisconnInfoTemp.ucDisassocReason =
				prStaRec->u2ReasonCode;
#endif
	} while (0);
}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will check the incoming Disassociation Frame.
*
* @param[in] prSwRfb            Pointer to the SW_RFB_T structure.
*
* @retval WLAN_STATUS_SUCCESS   Always not retain disassociation frames
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS saaFsmRunEventRxDisassoc(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
	P_STA_RECORD_T prStaRec;
	P_WLAN_DISASSOC_FRAME_T prDisassocFrame;
	UINT_8 ucWlanIdx;

	ASSERT(prSwRfb);
	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);
	prDisassocFrame = (P_WLAN_DISASSOC_FRAME_T) prSwRfb->pvHeader;
	ucWlanIdx = (UINT_8) HAL_RX_STATUS_GET_WLAN_IDX(prSwRfb->prRxStatus);

	DBGLOG(SAA, EVENT,
	       "Rx Disassoc frame from BSSID[" MACSTR "] DA[" MACSTR "] ReasonCode[0x%x]\n",
	       MAC2STR(prDisassocFrame->aucBSSID), MAC2STR(prDisassocFrame->aucDestAddr),
	       prDisassocFrame->u2ReasonCode);

	do {

		/* We should have the corresponding Sta Record. */
		if (!prStaRec) {
			DBGLOG(SAA, WARN, "Received a DisAssoc: wlanIdx[%d] w/o corresponding staRec\n", ucWlanIdx);
			break;
		}

		if (IS_STA_IN_AIS(prStaRec)) {
			P_BSS_INFO_T prAisBssInfo;

			if (!IS_AP_STA(prStaRec))
				break;

			prAisBssInfo = prAdapter->prAisBssInfo;

			if (prStaRec->ucStaState > STA_STATE_1) {

				/* Check if this is the AP we are associated or associating with */
				if (assocProcessRxDisassocFrame(prAdapter,
								prSwRfb,
								prStaRec->aucMacAddr,
								&prStaRec->u2ReasonCode) == WLAN_STATUS_SUCCESS) {

#if CFG_SUPPORT_802_11W
					P_AIS_SPECIFIC_BSS_INFO_T prAisSpecBssInfo;

					prAisSpecBssInfo = &(prAdapter->rWifiVar.rAisSpecificBssInfo);

					DBGLOG(RSN, INFO,
					       "QM RX MGT: Disassoc frame, P=%d Sec=%d CM=%d BC=%d fc=%02x\n",
					       prAisSpecBssInfo->
						fgMgmtProtection, (uint8_t)
						HAL_RX_STATUS_GET_SEC_MODE
						(prSwRfb->prRxStatus),
						HAL_RX_STATUS_IS_CIPHER_MISMATCH
						(prSwRfb->prRxStatus),
						IS_BMCAST_MAC_ADDR
						(prDisassocFrame->aucDestAddr),
						prDisassocFrame->u2FrameCtrl);

					if (IS_STA_IN_AIS(prStaRec)
					    && prAisSpecBssInfo->fgMgmtProtection
					    && HAL_RX_STATUS_IS_CIPHER_MISMATCH(prSwRfb->prRxStatus)
					    /* HAL_RX_STATUS_GET_SEC_MODE(prSwRfb->prRxStatus) != CIPHER_SUITE_CCMP */
					    ) {
						/* prDisassocFrame = (P_WLAN_DISASSOC_FRAME_T) prSwRfb->pvHeader; */
						saaChkDisassocfrmParamHandler(prAdapter, prDisassocFrame, prStaRec,
									      prSwRfb);
						return WLAN_STATUS_SUCCESS;
					}
#endif
#if CFG_SUPPORT_CFG80211_AUTH
		DBGLOG(SAA, INFO, "notification of RX disassociation %d\n",
			prSwRfb->u2PacketLen);
		cfg80211_rx_mlme_mgmt(prAdapter->prGlueInfo->prDevHandler,
			(PUINT_8)prDisassocFrame, (size_t)prSwRfb->u2PacketLen);
		DBGLOG(SAA, INFO, "notification of RX disassociation Done\n");
#endif
#if CFG_DISCONN_DEBUG_FEATURE
					g_rDisconnInfoTemp.u2DisassocSeqNum =
						(prDisassocFrame->u2SeqCtrl &
						MASK_SC_SEQ_NUM) >>
						MASK_SC_SEQ_NUM_OFFSET;
#endif
					saaSendDisconnectMsgHandler(prAdapter, prStaRec, prAisBssInfo, FRM_DISASSOC);
				}
			}
		}
#if CFG_ENABLE_WIFI_DIRECT
		else if (prAdapter->fgIsP2PRegistered && (IS_STA_IN_P2P(prStaRec))) {
			/* TODO(Kevin) */
#if CFG_SUPPORT_CFG80211_AUTH
		DBGLOG(SAA, INFO, "notification of RX disassociation %d\n",
			prSwRfb->u2PacketLen);
		cfg80211_rx_mlme_mgmt(prAdapter->prGlueInfo->prDevHandler,
			(PUINT_8)prDisassocFrame, (size_t)prSwRfb->u2PacketLen);
		DBGLOG(SAA, INFO, "notification of RX disassociation Done\n");
#endif
			p2pRoleFsmRunEventRxDisassociation(prAdapter, prStaRec, prSwRfb);
		}
#endif
#if CFG_ENABLE_BT_OVER_WIFI
		else if (IS_STA_BOW_TYPE(prStaRec)) {
			/* ToDo:: nothing */
			/* TODO(Kevin) */
#if CFG_SUPPORT_CFG80211_AUTH
		DBGLOG(SAA, INFO, "notification of RX disassociation %d\n",
			prSwRfb->u2PacketLen);
		cfg80211_rx_mlme_mgmt(prAdapter->prGlueInfo->prDevHandler,
			(PUINT_8)prDisassocFrame, (size_t)prSwRfb->u2PacketLen);
		DBGLOG(SAA, INFO, "notification of RX disassociation Done\n");
#endif
		}
#endif
		else
			ASSERT(0);

	} while (FALSE);

	return WLAN_STATUS_SUCCESS;

}				/* end of saaFsmRunEventRxDisassoc() */

/* for AOSP */
/*----------------------------------------------------------------------------*/
/*!
* @brief This function will check param of Disassoc frame and reson code for Disassoc
*
* @param[in]
*
* @retval
*/
/*----------------------------------------------------------------------------*/

VOID
saaChkDisassocfrmParamHandler(IN P_ADAPTER_T prAdapter,
			      IN P_WLAN_DISASSOC_FRAME_T prDisassocFrame, IN P_STA_RECORD_T prStaRec,
			      IN P_SW_RFB_T prSwRfb)
{
	if (!IS_BMCAST_MAC_ADDR(prDisassocFrame->aucDestAddr) &&
	    (prStaRec->u2ReasonCode == REASON_CODE_CLASS_2_ERR || prStaRec->u2ReasonCode == REASON_CODE_CLASS_3_ERR)) {
		/* MFP test plan 5.3.3.5 */
		DBGLOG(RSN, INFO, "QM RX MGT: rsnStartSaQuery\n");
		rsnStartSaQuery(prAdapter);
	} else {
		DBGLOG(RSN, INFO, "RXM: Drop unprotected Mgmt frame\n");
		DBGLOG(RSN, INFO,
		       "RXM: (MAC RX Done) RX (u2StatusFlag=0x%x) (ucKIdxSecMode=0x%x) (ucWlanIdx=0x%x)\n",
		       prSwRfb->prRxStatus->u2StatusFlag,
		       prSwRfb->prRxStatus->ucTidSecMode, prSwRfb->prRxStatus->ucWlanIdx);
	}
}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will handle the Abort Event to SAA FSM.
*
* @param[in] prMsgHdr   Message of Abort Request for a particular STA.
*
* @return none
*/
/*----------------------------------------------------------------------------*/
VOID saaFsmRunEventAbort(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_MSG_SAA_FSM_ABORT_T prSaaFsmAbortMsg;
	P_STA_RECORD_T prStaRec;

	ASSERT(prMsgHdr);

	prSaaFsmAbortMsg = (P_MSG_SAA_FSM_ABORT_T) prMsgHdr;
	prStaRec = prSaaFsmAbortMsg->prStaRec;

	ASSERT(prStaRec);
	if (!prStaRec) {
		cnmMemFree(prAdapter, prMsgHdr);
		return;
	}

	DBGLOG(SAA, LOUD, "EVENT-ABORT: Stop SAA FSM.\n");

	cnmMemFree(prAdapter, prMsgHdr);

	/* Reset Send Auth/(Re)Assoc Frame Count */
	prStaRec->ucTxAuthAssocRetryCount = 0;

	/* Cancel JOIN relative Timer */
	cnmTimerStopTimer(prAdapter, &prStaRec->rTxReqDoneOrRxRespTimer);

	if (prStaRec->eAuthAssocState != AA_STATE_IDLE) {
		DBGLOG(SAA, LOUD, "EVENT-ABORT: Previous Auth/Assoc State == %s.\n",
			apucDebugAAState[prStaRec->eAuthAssocState]);
	}
#if 0
	/* For the Auth/Assoc State to IDLE */
	prStaRec->eAuthAssocState = AA_STATE_IDLE;
#else
	/* Free this StaRec */
	cnmStaRecFree(prAdapter, prStaRec);
#endif
}				/* end of saaFsmRunEventAbort() */

/* TODO(Kevin): following code will be modified and move to AIS FSM */
#if 0
/*----------------------------------------------------------------------------*/
/*!
* \brief This function will send Join Timeout Event to JOIN FSM.
*
* \param[in] prAdapter      Pointer to the Adapter structure.
*
* \retval WLAN_STATUS_FAILURE   Fail because of Join Timeout
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS joinFsmRunEventJoinTimeOut(IN P_ADAPTER_T prAdapter)
{
	P_JOIN_INFO_T prJoinInfo;
	P_STA_RECORD_T prStaRec;

	DEBUGFUNC("joinFsmRunEventJoinTimeOut");

	ASSERT(prAdapter);
	prJoinInfo = &prAdapter->rJoinInfo;

	DBGLOG(JOIN, EVENT, "JOIN EVENT: JOIN TIMEOUT\n");

	/* Get a Station Record if possible, TA == BSSID for AP */
	prStaRec = staRecGetStaRecordByAddr(prAdapter, prJoinInfo->prBssDesc->aucBSSID);

	/* We have renew this Sta Record when in JOIN_STATE_INIT */
	ASSERT(prStaRec);

	/* Record the Status Code of Authentication Request */
	prStaRec->u2StatusCode = STATUS_CODE_JOIN_TIMEOUT;

	/* Increase Failure Count */
	prStaRec->ucJoinFailureCount++;

	/* Reset Send Auth/(Re)Assoc Frame Count */
	prJoinInfo->ucTxAuthAssocRetryCount = 0;

	/* Cancel other JOIN relative Timer */
	ARB_CANCEL_TIMER(prAdapter, prJoinInfo->rTxRequestTimer);

	ARB_CANCEL_TIMER(prAdapter, prJoinInfo->rRxResponseTimer);

	/* Restore original setting from current BSS_INFO_T */
	if (prAdapter->eConnectionState == MEDIA_STATE_CONNECTED)
		joinAdoptParametersFromCurrentBss(prAdapter);

	/* Pull back to IDLE */
	joinFsmSteps(prAdapter, JOIN_STATE_IDLE);

	return WLAN_STATUS_FAILURE;

}				/* end of joinFsmRunEventJoinTimeOut() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This function will adopt the parameters from Peer BSS.
*
* \param[in] prAdapter      Pointer to the Adapter structure.
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID joinAdoptParametersFromPeerBss(IN P_ADAPTER_T prAdapter)
{
	P_JOIN_INFO_T prJoinInfo;
	P_BSS_DESC_T prBssDesc;

	DEBUGFUNC("joinAdoptParametersFromPeerBss");

	ASSERT(prAdapter);
	prJoinInfo = &prAdapter->rJoinInfo;
	prBssDesc = prJoinInfo->prBssDesc;

	/* 4 <1> Adopt Peer BSS' PHY TYPE */
	prAdapter->eCurrentPhyType = prBssDesc->ePhyType;

	DBGLOG(JOIN, INFO, "Target BSS[%s]'s PhyType = %s\n",
	       prBssDesc->aucSSID, (prBssDesc->ePhyType == PHY_TYPE_ERP_INDEX) ? "ERP" : "HR_DSSS");

	/* 4 <2> Adopt Peer BSS' Frequency(Band/Channel) */
	DBGLOG(JOIN, INFO, "Target BSS's Channel = %d, Band = %d\n", prBssDesc->ucChannelNum, prBssDesc->eBand);

	nicSwitchChannel(prAdapter, prBssDesc->eBand, prBssDesc->ucChannelNum, 10);

	prJoinInfo->fgIsParameterAdopted = TRUE;
}				/* end of joinAdoptParametersFromPeerBss() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This function will adopt the parameters from current associated BSS.
*
* \param[in] prAdapter      Pointer to the Adapter structure.
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID joinAdoptParametersFromCurrentBss(IN P_ADAPTER_T prAdapter)
{
	/* P_JOIN_INFO_T prJoinInfo = &prAdapter->rJoinInfo; */
	P_BSS_INFO_T prBssInfo;

	ASSERT(prAdapter);
	prBssInfo = &prAdapter->rBssInfo;

	/* 4 <1> Adopt current BSS' PHY TYPE */
	prAdapter->eCurrentPhyType = prBssInfo->ePhyType;

	/* 4 <2> Adopt current BSS' Frequency(Band/Channel) */
	DBGLOG(JOIN, INFO, "Current BSS's Channel = %d, Band = %d\n", prBssInfo->ucChnl, prBssInfo->eBand);

	nicSwitchChannel(prAdapter, prBssInfo->eBand, prBssInfo->ucChnl, 10);
}				/* end of joinAdoptParametersFromCurrentBss() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This function will update all the SW variables and HW MCR registers after
*        the association with target BSS.
*
* \param[in] prAdapter      Pointer to the Adapter structure.
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID joinComplete(IN P_ADAPTER_T prAdapter)
{
	P_JOIN_INFO_T prJoinInfo;
	P_BSS_DESC_T prBssDesc;
	P_PEER_BSS_INFO_T prPeerBssInfo;
	P_BSS_INFO_T prBssInfo;
	P_CONNECTION_SETTINGS_T prConnSettings;
	P_STA_RECORD_T prStaRec;
	P_TX_CTRL_T prTxCtrl;
#if CFG_SUPPORT_802_11D
	P_IE_COUNTRY_T prIECountry;
#endif

	DEBUGFUNC("joinComplete");

	ASSERT(prAdapter);
	prJoinInfo = &prAdapter->rJoinInfo;
	prBssDesc = prJoinInfo->prBssDesc;
	prPeerBssInfo = &prAdapter->rPeerBssInfo;
	prBssInfo = &prAdapter->rBssInfo;
	prConnSettings = &prAdapter->rConnSettings;
	prTxCtrl = &prAdapter->rTxCtrl;

/* 4 <1> Update Connecting & Connected Flag of BSS_DESC_T. */
	/* Remove previous AP's Connection Flags if have */
	scanRemoveConnectionFlagOfBssDescByBssid(prAdapter, prBssInfo->aucBSSID);

	prBssDesc->fgIsConnected = TRUE;	/* Mask as Connected */

	if (prBssDesc->fgIsHiddenSSID) {
		/* NOTE(Kevin): This is for the case of Passive Scan and the target BSS didn't
		 * broadcast SSID on its Beacon Frame.
		 */
		COPY_SSID(prBssDesc->aucSSID,
			  prBssDesc->ucSSIDLen, prAdapter->rConnSettings.aucSSID, prAdapter->rConnSettings.ucSSIDLen);

		if (prBssDesc->ucSSIDLen)
			prBssDesc->fgIsHiddenSSID = FALSE;
#if DBG
		else
			ASSERT(0);
#endif /* DBG */

		DBGLOG(JOIN, INFO, "Hidden SSID! - Update SSID : %s\n", prBssDesc->aucSSID);
	}

/* 4 <2> Update BSS_INFO_T from BSS_DESC_T */
	/* 4 <2.A> PHY Type */
	prBssInfo->ePhyType = prBssDesc->ePhyType;

	/* 4 <2.B> BSS Type */
	prBssInfo->eBSSType = BSS_TYPE_INFRASTRUCTURE;

	/* 4 <2.C> BSSID */
	COPY_MAC_ADDR(prBssInfo->aucBSSID, prBssDesc->aucBSSID);

	DBGLOG(JOIN, INFO, "JOIN to BSSID: [" MACSTR "]\n", MAC2STR(prBssDesc->aucBSSID));

	/* 4 <2.D> SSID */
	COPY_SSID(prBssInfo->aucSSID, prBssInfo->ucSSIDLen, prBssDesc->aucSSID, prBssDesc->ucSSIDLen);

	/* 4 <2.E> Channel / Band information. */
	prBssInfo->eBand = prBssDesc->eBand;
	prBssInfo->ucChnl = prBssDesc->ucChannelNum;

	/* 4 <2.F> RSN/WPA information. */
	secFsmRunEventStart(prAdapter);
	prBssInfo->u4RsnSelectedPairwiseCipher = prBssDesc->u4RsnSelectedPairwiseCipher;
	prBssInfo->u4RsnSelectedGroupCipher = prBssDesc->u4RsnSelectedGroupCipher;
	prBssInfo->u4RsnSelectedAKMSuite = prBssDesc->u4RsnSelectedAKMSuite;

	if (secRsnKeyHandshakeEnabled())
		prBssInfo->fgIsWPAorWPA2Enabled = TRUE;
	else
		prBssInfo->fgIsWPAorWPA2Enabled = FALSE;

	/* 4 <2.G> Beacon interval. */
	prBssInfo->u2BeaconInterval = prBssDesc->u2BeaconInterval;

	/* 4 <2.H> DTIM period. */
	prBssInfo->ucDtimPeriod = prBssDesc->ucDTIMPeriod;

	/* 4 <2.I> ERP Information */
	if ((prBssInfo->ePhyType == PHY_TYPE_ERP_INDEX) &&	/* Our BSS's PHY_TYPE is ERP now. */
	    (prBssDesc->fgIsERPPresent)) {

		prBssInfo->fgIsERPPresent = TRUE;
		prBssInfo->ucERP = prBssDesc->ucERP;	/* Save the ERP for later check */
	} else {
		/* Some AP, may send ProbeResp without ERP IE. Thus prBssDesc->fgIsERPPresent is FALSE. */
		prBssInfo->fgIsERPPresent = FALSE;
		prBssInfo->ucERP = 0;
	}

#if CFG_SUPPORT_802_11D
	/* 4 <2.J> Country inforamtion of the associated AP */
	if (prConnSettings->fgMultiDomainCapabilityEnabled) {
		DOMAIN_INFO_ENTRY rDomainInfo;

		if (domainGetDomainInfoByScanResult(prAdapter, &rDomainInfo)) {
			if (prBssDesc->prIECountry) {
				prIECountry = prBssDesc->prIECountry;

				domainParseCountryInfoElem(prIECountry, &prBssInfo->rDomainInfo);

				/* use the domain get from the BSS info */
				prBssInfo->fgIsCountryInfoPresent = TRUE;
				nicSetupOpChnlList(prAdapter, prBssInfo->rDomainInfo.u2CountryCode, FALSE);
			} else {
				/* use the domain get from the scan result */
				prBssInfo->fgIsCountryInfoPresent = TRUE;
				nicSetupOpChnlList(prAdapter, rDomainInfo.u2CountryCode, FALSE);
			}
		}
	}
#endif

	/* 4 <2.K> Signal Power of the associated AP */
	prBssInfo->rRcpi = prBssDesc->rRcpi;
	prBssInfo->rRssi = RCPI_TO_dBm(prBssInfo->rRcpi);
	GET_CURRENT_SYSTIME(&prBssInfo->rRssiLastUpdateTime);

	/* 4 <2.L> Capability Field of the associated AP */
	prBssInfo->u2CapInfo = prBssDesc->u2CapInfo;

	DBGLOG(JOIN, INFO,
	       "prBssInfo-> fgIsERPPresent = %d, ucERP = %02x, rRcpi = %d, rRssi = %ld\n",
	       prBssInfo->fgIsERPPresent, prBssInfo->ucERP, prBssInfo->rRcpi, prBssInfo->rRssi);

/* 4 <3> Update BSS_INFO_T from PEER_BSS_INFO_T & NIC RATE FUNC */
	/* 4 <3.A> Association ID */
	prBssInfo->u2AssocId = prPeerBssInfo->u2AssocId;

	/* 4 <3.B> WMM Information */
	if (prAdapter->fgIsEnableWMM && (prPeerBssInfo->rWmmInfo.ucWmmFlag & WMM_FLAG_SUPPORT_WMM)) {

		prBssInfo->fgIsWmmAssoc = TRUE;
		prTxCtrl->rTxQForVoipAccess = TXQ_AC3;

		qosWmmInfoInit(&prBssInfo->rWmmInfo, (prBssInfo->ePhyType == PHY_TYPE_HR_DSSS_INDEX) ? TRUE : FALSE);

		if (prPeerBssInfo->rWmmInfo.ucWmmFlag & WMM_FLAG_AC_PARAM_PRESENT) {
			kalMemCopy(&prBssInfo->rWmmInfo, &prPeerBssInfo->rWmmInfo, sizeof(WMM_INFO_T));
		} else {
			kalMemCopy(&prBssInfo->rWmmInfo,
				   &prPeerBssInfo->rWmmInfo,
				   sizeof(WMM_INFO_T) - sizeof(prPeerBssInfo->rWmmInfo.arWmmAcParams));
		}
	} else {
		prBssInfo->fgIsWmmAssoc = FALSE;
		prTxCtrl->rTxQForVoipAccess = TXQ_AC1;

		kalMemZero(&prBssInfo->rWmmInfo, sizeof(WMM_INFO_T));
	}

	/* 4 <3.C> Operational Rate Set & BSS Basic Rate Set */
	prBssInfo->u2OperationalRateSet = prPeerBssInfo->u2OperationalRateSet;
	prBssInfo->u2BSSBasicRateSet = prPeerBssInfo->u2BSSBasicRateSet;

	/* 4 <3.D> Short Preamble */
	if (prBssInfo->fgIsERPPresent) {

		/* NOTE(Kevin 2007/12/24): Truth Table.
		 * Short Preamble Bit in
		 * <AssocReq> <AssocResp w/i ERP> <BARKER(Long)>  Final Driver Setting(Short)
		 * TRUE        FALSE              FALSE      FALSE(shouldn't have such case, use the AssocResp)
		 * TRUE        FALSE              TRUE       FALSE
		 * FALSE       FALSE              FALSE      FALSE(shouldn't have such case, use the AssocResp)
		 * FALSE       FALSE              TRUE       FALSE
		 * TRUE        TRUE               FALSE      TRUE(follow ERP)
		 * TRUE        TRUE               TRUE       FALSE(follow ERP)
		 * FALSE       TRUE               FALSE      FALSE(shouldn't have such case, and we should set to FALSE)
		 * FALSE       TRUE               TRUE       FALSE(we should set to FALSE)
		 */
		if ((prPeerBssInfo->fgIsShortPreambleAllowed) &&
			((prConnSettings->ePreambleType == PREAMBLE_TYPE_SHORT) ||
			 /* Short Preamble Option Enable is TRUE */
			 ((prConnSettings->ePreambleType == PREAMBLE_TYPE_AUTO) &&
			  (prBssDesc->u2CapInfo & CAP_INFO_SHORT_PREAMBLE)))) {

			prBssInfo->fgIsShortPreambleAllowed = TRUE;

			if (prBssInfo->ucERP & ERP_INFO_BARKER_PREAMBLE_MODE)
				prBssInfo->fgUseShortPreamble = FALSE;
			else
				prBssInfo->fgUseShortPreamble = TRUE;
		} else {
			prBssInfo->fgIsShortPreambleAllowed = FALSE;
			prBssInfo->fgUseShortPreamble = FALSE;
		}
	} else {
		/* NOTE(Kevin 2007/12/24): Truth Table.
		 * Short Preamble Bit in
		 * <AssocReq>     <AssocResp w/o ERP>     Final Driver Setting(Short)
		 * TRUE            FALSE                  FALSE
		 * FALSE           FALSE                  FALSE
		 * TRUE            TRUE                   TRUE
		 * FALSE           TRUE(status success)   TRUE
		 * --> Honor the result of prPeerBssInfo.
		 */

		prBssInfo->fgIsShortPreambleAllowed = prBssInfo->fgUseShortPreamble =
		    prPeerBssInfo->fgIsShortPreambleAllowed;
	}

	DBGLOG(JOIN, INFO,
	       "prBssInfo->fgIsShortPreambleAllowed = %d, prBssInfo->fgUseShortPreamble = %d\n",
	       prBssInfo->fgIsShortPreambleAllowed, prBssInfo->fgUseShortPreamble);

	/* 4 <3.E> Short Slot Time */
	prBssInfo->fgUseShortSlotTime = prPeerBssInfo->fgUseShortSlotTime;	/* AP support Short Slot Time */

	DBGLOG(JOIN, INFO, "prBssInfo->fgUseShortSlotTime = %d\n", prBssInfo->fgUseShortSlotTime);

	nicSetSlotTime(prAdapter,
		       prBssInfo->ePhyType,
		       ((prConnSettings->fgIsShortSlotTimeOptionEnable &&
			 prBssInfo->fgUseShortSlotTime) ? TRUE : FALSE));

	/* 4 <3.F> Update Tx Rate for Control Frame */
	bssUpdateTxRateForControlFrame(prAdapter);

	/* 4 <3.G> Save the available Auth Types during Roaming (Design for Fast BSS Transition). */
	/* if (prAdapter->fgIsEnableRoaming) *//* NOTE(Kevin): Always prepare info for roaming */
	{

		if (prJoinInfo->ucCurrAuthAlgNum == AUTH_ALGORITHM_NUM_OPEN_SYSTEM)
			prJoinInfo->ucRoamingAuthTypes |= AUTH_TYPE_OPEN_SYSTEM;
		else if (prJoinInfo->ucCurrAuthAlgNum == AUTH_ALGORITHM_NUM_SHARED_KEY)
			prJoinInfo->ucRoamingAuthTypes |= AUTH_TYPE_SHARED_KEY;

		prBssInfo->ucRoamingAuthTypes = prJoinInfo->ucRoamingAuthTypes;

		/* Set the stable time of the associated BSS. We won't do roaming decision
		 * during the stable time.
		 */
		SET_EXPIRATION_TIME(prBssInfo->rRoamingStableExpirationTime,
				    SEC_TO_SYSTIME(ROAMING_STABLE_TIMEOUT_SEC));
	}

	/* 4 <3.H> Update Parameter for TX Fragmentation Threshold */
#if CFG_TX_FRAGMENT
	txFragInfoUpdate(prAdapter);
#endif /* CFG_TX_FRAGMENT */

/* 4 <4> Update STA_RECORD_T */
	/* Get a Station Record if possible */
	prStaRec = staRecGetStaRecordByAddr(prAdapter, prBssDesc->aucBSSID);

	if (prStaRec) {
		UINT_16 u2OperationalRateSet, u2DesiredRateSet;

		/* 4 <4.A> Desired Rate Set */
		u2OperationalRateSet = (rPhyAttributes[prBssInfo->ePhyType].u2SupportedRateSet &
					prBssInfo->u2OperationalRateSet);

		u2DesiredRateSet = (u2OperationalRateSet & prConnSettings->u2DesiredRateSet);
		if (u2DesiredRateSet) {
			prStaRec->u2DesiredRateSet = u2DesiredRateSet;
		} else {
			/* For Error Handling - The Desired Rate Set is not covered in Operational Rate Set. */
			prStaRec->u2DesiredRateSet = u2OperationalRateSet;
		}

		/* Try to set the best initial rate for this entry */
		if (!rateGetBestInitialRateIndex(prStaRec->u2DesiredRateSet,
						 prStaRec->rRcpi, &prStaRec->ucCurrRate1Index)) {

			if (!rateGetLowestRateIndexFromRateSet(prStaRec->u2DesiredRateSet, &prStaRec->ucCurrRate1Index))
				ASSERT(0);
		}

		DBGLOG(JOIN, INFO, "prStaRec->ucCurrRate1Index = %d\n", prStaRec->ucCurrRate1Index);

		/* 4 <4.B> Preamble Mode */
		prStaRec->fgIsShortPreambleOptionEnable = prBssInfo->fgUseShortPreamble;

		/* 4 <4.C> QoS Flag */
		prStaRec->fgIsQoS = prBssInfo->fgIsWmmAssoc;
	}
#if DBG
	else
		ASSERT(0);
#endif /* DBG */

/* 4 <5> Update NIC */
	/* 4 <5.A> Update BSSID & Operation Mode */
	nicSetupBSS(prAdapter, prBssInfo);

	/* 4 <5.B> Update WLAN Table. */
	if (nicSetHwBySta(prAdapter, prStaRec) == FALSE)
		ASSERT(FALSE);
	/* 4 <5.C> Update Desired Rate Set for BT. */
#if CFG_TX_FRAGMENT
	if (prConnSettings->fgIsEnableTxAutoFragmentForBT)
		txRateSetInitForBT(prAdapter, prStaRec);
#endif /* CFG_TX_FRAGMENT */

	/* 4 <5.D> TX AC Parameter and TX/RX Queue Control */
	if (prBssInfo->fgIsWmmAssoc) {

#if CFG_TX_AGGREGATE_HW_FIFO
		nicTxAggregateTXQ(prAdapter, FALSE);
#endif /* CFG_TX_AGGREGATE_HW_FIFO */

		qosUpdateWMMParametersAndAssignAllowedACI(prAdapter, &prBssInfo->rWmmInfo);
	} else {

#if CFG_TX_AGGREGATE_HW_FIFO
		nicTxAggregateTXQ(prAdapter, TRUE);
#endif /* CFG_TX_AGGREGATE_HW_FIFO */

		nicTxNonQoSAssignDefaultAdmittedTXQ(prAdapter);

		nicTxNonQoSUpdateTXQParameters(prAdapter, prBssInfo->ePhyType);
	}

#if CFG_TX_STOP_WRITE_TX_FIFO_UNTIL_JOIN
	{
		prTxCtrl->fgBlockTxDuringJoin = FALSE;

#if !CFG_TX_AGGREGATE_HW_FIFO	/* TX FIFO AGGREGATE already do flush once */
		nicTxFlushStopQueues(prAdapter, (UINT_8) TXQ_DATA_MASK, (UINT_8) NULL);
#endif /* CFG_TX_AGGREGATE_HW_FIFO */

		nicTxRetransmitOfSendWaitQue(prAdapter);

		if (prTxCtrl->fgIsPacketInOsSendQueue)
			nicTxRetransmitOfOsSendQue(prAdapter);
#if CFG_SDIO_TX_ENHANCE
		halTxLeftClusteredMpdu(prAdapter);
#endif /* CFG_SDIO_TX_ENHANCE */

	}
#endif /* CFG_TX_STOP_WRITE_TX_FIFO_UNTIL_JOIN */

/* 4 <6> Setup CONNECTION flag. */
	prAdapter->eConnectionState = MEDIA_STATE_CONNECTED;
	prAdapter->eConnectionStateIndicated = MEDIA_STATE_CONNECTED;

	if (prJoinInfo->fgIsReAssoc)
		prAdapter->fgBypassPortCtrlForRoaming = TRUE;
	else
		prAdapter->fgBypassPortCtrlForRoaming = FALSE;

	kalIndicateStatusAndComplete(prAdapter->prGlueInfo, WLAN_STATUS_MEDIA_CONNECT, (PVOID) NULL, 0);
}				/* end of joinComplete() */
#endif
