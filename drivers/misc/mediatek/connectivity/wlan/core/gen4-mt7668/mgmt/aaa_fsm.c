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
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/mgmt/aaa_fsm.c#3 $
*/

/*! \file   "aaa_fsm.c"
*    \brief  This file defines the FSM for AAA MODULE.
*
*    This file defines the FSM for AAA MODULE.
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
#if 0
/*----------------------------------------------------------------------------*/
/*!
* @brief This function will send Event to AIS/BOW/P2P
*
* @param[in] rJoinStatus        To indicate JOIN success or failure.
* @param[in] prStaRec           Pointer to the STA_RECORD_T
* @param[in] prSwRfb            Pointer to the SW_RFB_T

* @return none
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS aaaFsmSendEventJoinComplete(WLAN_STATUS rJoinStatus, P_STA_RECORD_T prStaRec, P_SW_RFB_T prSwRfb)
{
	P_MSG_SAA_JOIN_COMP_T prJoinCompMsg;

	ASSERT(prStaRec);

	prJoinCompMsg = cnmMemAlloc(RAM_TYPE_TCM, sizeof(MSG_SAA_JOIN_COMP_T));
	if (!prJoinCompMsg)
		return WLAN_STATUS_RESOURCES;

	if (IS_STA_IN_AIS(prStaRec))
		prJoinCompMsg->rMsgHdr.eMsgId = MID_SAA_AIS_JOIN_COMPLETE;
	else if (IS_STA_IN_P2P(prStaRec))
		prJoinCompMsg->rMsgHdr.eMsgId = MID_SAA_P2P_JOIN_COMPLETE;
	else if (IS_STA_IN_BOW(prStaRec))
		prJoinCompMsg->rMsgHdr.eMsgId = MID_SAA_BOW_JOIN_COMPLETE;
	else
		ASSERT(0);

	prJoinCompMsg->rJoinStatus = rJoinStatus;
	prJoinCompMsg->prStaRec = prStaRec;
	prJoinCompMsg->prSwRfb = prSwRfb;

	mboxSendMsg(MBOX_ID_0, (P_MSG_HDR_T) prJoinCompMsg, MSG_SEND_METHOD_BUF);

	return WLAN_STATUS_SUCCESS;

}				/* end of saaFsmSendEventJoinComplete() */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will handle the Start Event to AAA FSM.
*
* @param[in] prMsgHdr   Message of Join Request for a particular STA.
*
* @return none
*/
/*----------------------------------------------------------------------------*/
VOID aaaFsmRunEventStart(IN P_MSG_HDR_T prMsgHdr)
{
	P_MSG_SAA_JOIN_REQ_T prJoinReqMsg;
	P_STA_RECORD_T prStaRec;
	P_AIS_BSS_INFO_T prAisBssInfo;

	ASSERT(prMsgHdr);

	prJoinReqMsg = (P_MSG_SAA_JOIN_REQ_T) prMsgHdr;
	prStaRec = prJoinReqMsg->prStaRec;

	ASSERT(prStaRec);

	DBGLOG(SAA, LOUD, "EVENT-START: Trigger SAA FSM\n");

	cnmMemFree(prMsgHdr);

	/* 4 <1> Validation of SAA Start Event */
	if (!IS_AP_STA(prStaRec->eStaType)) {

		DBGLOG(SAA, ERROR, "EVENT-START: STA Type - %d was not supported.\n", prStaRec->eStaType);

		/* Ignore the return value because don't care the prSwRfb */
		saaFsmSendEventJoinComplete(WLAN_STATUS_FAILURE, prStaRec, NULL);

		return;
	}
	/* 4 <2> The previous JOIN process is not completed ? */
	if (prStaRec->eAuthAssocState != AA_STATE_IDLE) {
		DBGLOG(SAA, ERROR, "EVENT-START: Reentry of SAA Module.\n");
		prStaRec->eAuthAssocState = AA_STATE_IDLE;
	}
	/* 4 <3> Reset Status Code and Time */
	/* Update Station Record - Status/Reason Code */
	prStaRec->u2StatusCode = STATUS_CODE_SUCCESSFUL;

	/* Update the record join time. */
	GET_CURRENT_SYSTIME(&prStaRec->rLastJoinTime);

	prStaRec->ucTxAuthAssocRetryCount = 0;

	if (prStaRec->prChallengeText) {
		cnmMemFree(prStaRec->prChallengeText);
		prStaRec->prChallengeText = (P_IE_CHALLENGE_TEXT_T) NULL;
	}

	cnmTimerStopTimer(&prStaRec->rTxReqDoneOrRxRespTimer);

	prStaRec->ucStaState = STA_STATE_1;

	/* Trigger SAA MODULE */
	saaFsmSteps(prStaRec, SAA_STATE_SEND_AUTH1, (P_SW_RFB_T) NULL);
}				/* end of saaFsmRunEventStart() */
#endif

#if CFG_SUPPORT_AAA

VOID aaaFsmRunEventTxReqTimeOut(IN P_ADAPTER_T prAdapter, IN ULONG plParamPtr)
{
	P_STA_RECORD_T prStaRec = (P_STA_RECORD_T) plParamPtr;
	P_BSS_INFO_T prBssInfo;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);

	ASSERT(prStaRec);
	if (!prStaRec)
		return;

	DBGLOG(AAA, LOUD, "EVENT-TIMER: TX REQ TIMEOUT, Current Time = %d\n", kalGetTimeTick());

	/* Trigger statistics log if Auth/Assoc Tx timeout */
	wlanTriggerStatsLog(prAdapter, prAdapter->rWifiVar.u4StatsLogDuration);

	switch (prStaRec->eAuthAssocState) {
	case AAA_STATE_SEND_AUTH2:
		DBGLOG(AAA, ERROR,
			       "LOST EVENT ,Auth Tx done disappear for (%d)Ms\n",
			TU_TO_MSEC(TX_AUTHENTICATION_RESPONSE_TIMEOUT_TU));

		prStaRec->eAuthAssocState = AA_STATE_IDLE;

		/* NOTE(Kevin): Change to STATE_1 */
		cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_1);

#if CFG_ENABLE_WIFI_DIRECT
			if (prBssInfo->eNetworkType == NETWORK_TYPE_P2P)
				p2pRoleFsmRunEventAAATxFail(prAdapter, prStaRec, prBssInfo);
#endif /* CFG_ENABLE_WIFI_DIRECT */
		break;
#if 0
	/*state 2 to state 3 only check Assoc_req valid, no need for time out
	 *the fail case already handle at aaaFsmRunEventRxAssoc
	 */
	case AAA_STATE_SEND_ASSOC2:
		DBGLOG(AAA, ERROR,
			       "LOST EVENT ,Assoc Tx done disappear for (%d)Ms\n",
			TU_TO_MSEC(TX_AUTHENTICATION_RESPONSE_TIMEOUT_TU));


		prStaRec->eAuthAssocState = AAA_STATE_SEND_AUTH2;

		/* NOTE(Kevin): Change to STATE_2 */
		cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_2);

#if CFG_ENABLE_WIFI_DIRECT
		if (prBssInfo->eNetworkType == NETWORK_TYPE_P2P)
			p2pRoleFsmRunEventAAATxFail(prAdapter, prStaRec, prBssInfo);
#endif /* CFG_ENABLE_WIFI_DIRECT */
		break;
#endif

	default:
		return;
	}


}				/* end of saaFsmRunEventTxReqTimeOut() */





/*----------------------------------------------------------------------------*/
/*!
* @brief This function will process the Rx Auth Request Frame and then
*        trigger AAA FSM.
*
* @param[in] prAdapter          Pointer to the Adapter structure.
* @param[in] prSwRfb            Pointer to the SW_RFB_T structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aaaFsmRunEventRxAuth(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
	P_BSS_INFO_T prBssInfo = (P_BSS_INFO_T) NULL;
	P_STA_RECORD_T prStaRec = (P_STA_RECORD_T) NULL;
	UINT_16 u2StatusCode;
	BOOLEAN fgReplyAuth = FALSE;
	P_WLAN_AUTH_FRAME_T prAuthFrame = (P_WLAN_AUTH_FRAME_T) NULL;

	ASSERT(prAdapter);

	do {
		prAuthFrame = (P_WLAN_AUTH_FRAME_T) prSwRfb->pvHeader;

#if CFG_ENABLE_WIFI_DIRECT
		prBssInfo = p2pFuncBSSIDFindBssInfo(prAdapter, prAuthFrame->aucBSSID);

		/* 4 <1> Check P2P network conditions */

		/* if (prBssInfo && prAdapter->fgIsP2PRegistered) */
		/* modify coding sytle to reduce indent */

		if (!prAdapter->fgIsP2PRegistered)
			goto bow_proc;

		if (prBssInfo && prBssInfo->fgIsNetActive) {

			/* 4 <1.1> Validate Auth Frame by Auth Algorithm/Transation Seq */
			if (WLAN_STATUS_SUCCESS ==
				authProcessRxAuth1Frame(prAdapter,
					prSwRfb,
					prBssInfo->aucBSSID,
					AUTH_ALGORITHM_NUM_OPEN_SYSTEM,
					AUTH_TRANSACTION_SEQ_1, &u2StatusCode)) {

				if (u2StatusCode == STATUS_CODE_SUCCESSFUL) {
					DBGLOG(AAA, TRACE, "process RxAuth status success\n");
					/* 4 <1.2> Validate Auth Frame for Network Specific Conditions */
					fgReplyAuth = p2pFuncValidateAuth(prAdapter,
									  prBssInfo,
									  prSwRfb, &prStaRec, &u2StatusCode);

#if CFG_SUPPORT_802_11W
					/* AP PMF, if PMF connection, ignore Rx auth */
					/* Certification 4.3.3.4 */
					if (rsnCheckBipKeyInstalled(prAdapter, prStaRec)) {
						DBGLOG(AAA, INFO, "Drop RxAuth\n");
						return;
					}
#endif
				} else {
					fgReplyAuth = TRUE;
				}
				break;
			}
		}
#endif /* CFG_ENABLE_WIFI_DIRECT */

bow_proc:

		/* 4 <2> Check BOW network conditions */
#if CFG_ENABLE_BT_OVER_WIFI
		{
			P_BOW_FSM_INFO_T prBowFsmInfo = (P_BOW_FSM_INFO_T) NULL;

			prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);
			prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prBowFsmInfo->ucBssIndex);

			if ((prBssInfo->fgIsNetActive) && (prBssInfo->eCurrentOPMode == OP_MODE_BOW)) {

				/* 4 <2.1> Validate Auth Frame by Auth Algorithm/Transation Seq */
				/* Check if for this BSSID */
				if (WLAN_STATUS_SUCCESS ==
				    authProcessRxAuth1Frame(prAdapter,
							    prSwRfb,
							    prBssInfo->aucBSSID,
							    AUTH_ALGORITHM_NUM_OPEN_SYSTEM,
							    AUTH_TRANSACTION_SEQ_1, &u2StatusCode)) {

					if (u2StatusCode == STATUS_CODE_SUCCESSFUL) {

						/* 4 <2.2> Validate Auth Frame for Network Specific Conditions */
						fgReplyAuth =
						    bowValidateAuth(prAdapter, prSwRfb, &prStaRec, &u2StatusCode);

					} else {

						fgReplyAuth = TRUE;
					}
					/* TODO(Kevin): Allocate a STA_RECORD_T for new client */
					break;
				}
			}
		}
#endif /* CFG_ENABLE_BT_OVER_WIFI */

		return;
	} while (FALSE);

	if (prStaRec) {
		/* update RCPI */
		ASSERT(prSwRfb->prRxStatusGroup3);
		prStaRec->ucRCPI = nicRxGetRcpiValueFromRxv(RCPI_MODE_WF0, prSwRfb);
	}
	/* 4 <3> Update STA_RECORD_T and reply Auth_2(Response to Auth_1) Frame */
	if (fgReplyAuth) {

		if (prStaRec) {

			if (u2StatusCode == STATUS_CODE_SUCCESSFUL) {
				if (prStaRec->eAuthAssocState != AA_STATE_IDLE) {
					DBGLOG(AAA, WARN,
					       "Previous AuthAssocState (%d) != IDLE.\n", prStaRec->eAuthAssocState);
				}
				if (prStaRec->eAuthAssocState
					== AAA_STATE_SEND_AUTH2)
				return;

				prStaRec->eAuthAssocState = AAA_STATE_SEND_AUTH2;
			} else {
				prStaRec->eAuthAssocState = AA_STATE_IDLE;

				/* NOTE(Kevin): Change to STATE_1 */
				cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_1);
			}

			/* Update the record join time. */
			GET_CURRENT_SYSTIME(&prStaRec->rUpdateTime);

			/* Update Station Record - Status/Reason Code */
			prStaRec->u2StatusCode = u2StatusCode;

			prStaRec->ucAuthAlgNum = AUTH_ALGORITHM_NUM_OPEN_SYSTEM;
		} else {
			/* NOTE(Kevin): We should have STA_RECORD_T if the status code was successful */
			ASSERT(!(u2StatusCode == STATUS_CODE_SUCCESSFUL));
		}

		/* NOTE: Ignore the return status for AAA */
		/* 4 <4> Reply  Auth */
		authSendAuthFrame(prAdapter,
				  prStaRec, prBssInfo->ucBssIndex, prSwRfb, AUTH_TRANSACTION_SEQ_2, u2StatusCode);


		/*sta_rec might be removed when client list full, skip timer setting*/
		if (prStaRec && prStaRec->fgIsInUse == TRUE) {
			cnmTimerStopTimer(prAdapter, &prStaRec->rTxReqDoneOrRxRespTimer);
			/*ToDo:Init Timer to check get Auth Txdone avoid sta_rec not clear*/
			cnmTimerInitTimer(prAdapter,
					  &prStaRec->rTxReqDoneOrRxRespTimer, (PFN_MGMT_TIMEOUT_FUNC)
					  aaaFsmRunEventTxReqTimeOut, (ULONG) prStaRec);

			cnmTimerStartTimer(prAdapter,
					   &prStaRec->rTxReqDoneOrRxRespTimer,
					   TU_TO_MSEC(TX_AUTHENTICATION_RESPONSE_TIMEOUT_TU));
		}



	} else if (prStaRec)
		cnmStaRecFree(prAdapter, prStaRec);
}				/* end of aaaFsmRunEventRxAuth() */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will process the Rx (Re)Association Request Frame and then
*        trigger AAA FSM.
*
* @param[in] prAdapter          Pointer to the Adapter structure.
* @param[in] prSwRfb            Pointer to the SW_RFB_T structure.
*
* @retval WLAN_STATUS_SUCCESS           Always return success
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS aaaFsmRunEventRxAssoc(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
	P_BSS_INFO_T prBssInfo;
	P_STA_RECORD_T prStaRec = (P_STA_RECORD_T) NULL;
	UINT_16 u2StatusCode = STATUS_CODE_RESERVED;
	BOOLEAN fgReplyAssocResp = FALSE;
	BOOLEAN fgSendSAQ = FALSE;

	ASSERT(prAdapter);
	DBGLOG(AAA, INFO, "aaaFsmRunEventRxAssoc\n");

	do {

		/* 4 <1> Check if we have the STA_RECORD_T for incoming Assoc Req */
		prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);

		/* We should have the corresponding Sta Record. */
		if ((!prStaRec) || (!prStaRec->fgIsInUse)) {
			/* Not to reply association response with failure code due to lack of STA_REC */
			break;
		}

		if (!IS_CLIENT_STA(prStaRec))
			break;

		DBGLOG(AAA, TRACE, "RxAssoc enter ucStaState:%d, eAuthassocState:%d\n",
			prStaRec->ucStaState, prStaRec->eAuthAssocState);

		if (prStaRec->ucStaState == STA_STATE_3) {
			/* Do Reassocation */
		} else if ((prStaRec->ucStaState == STA_STATE_2) &&
				(prStaRec->eAuthAssocState == AAA_STATE_SEND_AUTH2)) {
			/* Normal case */
		} else {
			DBGLOG(AAA, WARN, "Previous AuthAssocState (%d) != SEND_AUTH2.\n", prStaRec->eAuthAssocState);

			/* Maybe Auth Response TX fail, but actually it success. */
			cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_2);
		}

		/* update RCPI */
		ASSERT(prSwRfb->prRxStatusGroup3);
		prStaRec->ucRCPI = nicRxGetRcpiValueFromRxv(RCPI_MODE_WF0, prSwRfb);

		/* 4 <2> Check P2P network conditions */
#if CFG_ENABLE_WIFI_DIRECT
		if ((prAdapter->fgIsP2PRegistered) && (IS_STA_IN_P2P(prStaRec))) {

			prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);

			if (prBssInfo->fgIsNetActive) {

				/* 4 <2.1> Validate Assoc Req Frame and get Status Code */
				/* Check if for this BSSID */
				if (WLAN_STATUS_SUCCESS ==
				    assocProcessRxAssocReqFrame(prAdapter, prSwRfb, &u2StatusCode)) {

					if (u2StatusCode == STATUS_CODE_SUCCESSFUL) {
						/* 4 <2.2> Validate Assoc Req  Frame for Network Specific Conditions */
						fgReplyAssocResp =
						    p2pFuncValidateAssocReq(prAdapter, prSwRfb,
									    (PUINT_16)&u2StatusCode);
					} else {
						fgReplyAssocResp = TRUE;
					}

					break;
				}
			}
		}
#endif /* CFG_ENABLE_WIFI_DIRECT */

		/* 4 <3> Check BOW network conditions */
#if CFG_ENABLE_BT_OVER_WIFI
		if (IS_STA_BOW_TYPE(prStaRec)) {

			prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);

			if ((prBssInfo->fgIsNetActive) && (prBssInfo->eCurrentOPMode == OP_MODE_BOW)) {

				/* 4 <3.1> Validate Auth Frame by Auth Algorithm/Transation Seq */
				/* Check if for this BSSID */
				if (WLAN_STATUS_SUCCESS ==
				    assocProcessRxAssocReqFrame(prAdapter, prSwRfb, &u2StatusCode)) {

					if (u2StatusCode == STATUS_CODE_SUCCESSFUL) {

						/* 4 <3.2> Validate Auth Frame for Network Specific Conditions */
						fgReplyAssocResp =
						    bowValidateAssocReq(prAdapter, prSwRfb, &u2StatusCode);

					} else {

						fgReplyAssocResp = TRUE;
					}

					/* TODO(Kevin): Allocate a STA_RECORD_T for new client */
					break;
				}
			}
		}
#endif /* CFG_ENABLE_BT_OVER_WIFI */

		return WLAN_STATUS_SUCCESS;	/* To release the SW_RFB_T */
	} while (FALSE);

	/* 4 <4> Update STA_RECORD_T and reply Assoc Resp Frame */
	if (fgReplyAssocResp) {
		UINT_16 u2IELength;
		PUINT_8 pucIE;

		cnmTimerStopTimer(prAdapter, &prStaRec->rTxReqDoneOrRxRespTimer);

		if ((((P_WLAN_ASSOC_REQ_FRAME_T) (prSwRfb->pvHeader))->u2FrameCtrl & MASK_FRAME_TYPE) ==
		    MAC_FRAME_REASSOC_REQ) {

			u2IELength = prSwRfb->u2PacketLen -
			    (UINT_16) OFFSET_OF(WLAN_REASSOC_REQ_FRAME_T, aucInfoElem[0]);

			pucIE = ((P_WLAN_REASSOC_REQ_FRAME_T) (prSwRfb->pvHeader))->aucInfoElem;
		} else {
			u2IELength = prSwRfb->u2PacketLen - (UINT_16) OFFSET_OF(WLAN_ASSOC_REQ_FRAME_T, aucInfoElem[0]);

			pucIE = ((P_WLAN_ASSOC_REQ_FRAME_T) (prSwRfb->pvHeader))->aucInfoElem;
		}

		rlmProcessAssocReq(prAdapter, prSwRfb, pucIE, u2IELength);

		/* 4 <4.1> Assign Association ID */
		if (u2StatusCode == STATUS_CODE_SUCCESSFUL) {

#if CFG_ENABLE_WIFI_DIRECT
			if ((prAdapter->fgIsP2PRegistered) && (IS_STA_IN_P2P(prStaRec))) {
				prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);
				if (p2pRoleFsmRunEventAAAComplete(prAdapter, prStaRec, prBssInfo) ==
				    WLAN_STATUS_SUCCESS) {
					prStaRec->u2AssocId = bssAssignAssocID(prStaRec);
					/* prStaRec->eAuthAssocState = AA_STATE_IDLE; */
					/* NOTE(Kevin): for TX done */
					prStaRec->eAuthAssocState = AAA_STATE_SEND_ASSOC2;
					/* NOTE(Kevin): Method A: Change to STATE_3 before handle TX Done */
					/* cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_3); */
				} else {
					/* Client List FULL. */
					u2StatusCode = STATUS_CODE_REQ_DECLINED;

					prStaRec->u2AssocId = 0;	/* Invalid Association ID */

					/* If(Re)association fail,remove sta record and use class error to handle sta */
					prStaRec->eAuthAssocState = AA_STATE_IDLE;

					/* NOTE(Kevin): Better to change state here, not at TX Done */
					cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_2);
				}
			}
#endif

#if CFG_ENABLE_BT_OVER_WIFI
			if ((IS_STA_BOW_TYPE(prStaRec))) {
				/* if (bowRunEventAAAComplete(prAdapter, prStaRec) == WLAN_STATUS_SUCCESS) { */
				prStaRec->u2AssocId = bssAssignAssocID(prStaRec);
				prStaRec->eAuthAssocState = AAA_STATE_SEND_ASSOC2;	/* NOTE(Kevin): for TX done */

				/* NOTE(Kevin): Method A: Change to STATE_3 before handle TX Done */
				/* cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_3); */
			}
#endif
		} else {

#if CFG_SUPPORT_802_11W
			/* AP PMF */
			/* don't change state, just send assoc resp (NO need TX done, TIE + code30) and then SAQ */
			if (u2StatusCode == STATUS_CODE_ASSOC_REJECTED_TEMPORARILY) {
				DBGLOG(AAA, INFO, "AP send SAQ\n");
				fgSendSAQ = TRUE;
			} else
#endif
			{
				prStaRec->u2AssocId = 0;	/* Invalid Association ID */

				/* If (Re)association fail, remove sta record and use class error to handle sta */
				prStaRec->eAuthAssocState = AA_STATE_IDLE;
				/* Remove from client list if it was previously associated */
				if ((prStaRec->ucStaState > STA_STATE_1) && prAdapter->fgIsP2PRegistered
					&& (IS_STA_IN_P2P(prStaRec))) {
					P_BSS_INFO_T prBssInfo = NULL;

					prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);
					if (prBssInfo) {
						DBGLOG(AAA, INFO, "Remove client!\n");
						bssRemoveClient(prAdapter, prBssInfo, prStaRec);
					}
				}
				/* NOTE(Kevin): Better to change state here, not at TX Done */
				cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_2);
			}
		}

		/* Update the record join time. */
		GET_CURRENT_SYSTIME(&prStaRec->rUpdateTime);

		/* Update Station Record - Status/Reason Code */
		prStaRec->u2StatusCode = u2StatusCode;

		/* NOTE: Ignore the return status for AAA */
		/* 4 <4.2> Reply  Assoc Resp */
		assocSendReAssocRespFrame(prAdapter, prStaRec);

#if CFG_SUPPORT_802_11W
		/* AP PMF */
		if (fgSendSAQ) {
			/* if PMF connection, and return code 30, send SAQ */
			rsnApStartSaQuery(prAdapter, prStaRec);
		}
#endif

	}

	return WLAN_STATUS_SUCCESS;

}				/* end of aaaFsmRunEventRxAssoc() */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will handle TxDone(Auth2/AssocReq) Event of AAA FSM.
*
* @param[in] prAdapter      Pointer to the Adapter structure.
* @param[in] prMsduInfo     Pointer to the MSDU_INFO_T.
* @param[in] rTxDoneStatus  Return TX status of the Auth1/Auth3/AssocReq frame.
*
* @retval WLAN_STATUS_SUCCESS
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
aaaFsmRunEventTxDone(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_RESULT_CODE_T rTxDoneStatus)
{
	P_STA_RECORD_T prStaRec;
	P_BSS_INFO_T prBssInfo;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

	DBGLOG(AAA, LOUD, "EVENT-TX DONE: Current Time = %d\n",
			kalGetTimeTick());

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

	if ((!prStaRec) || (!prStaRec->fgIsInUse))
		return WLAN_STATUS_SUCCESS;	/* For the case of replying ERROR STATUS CODE */

	ASSERT(prStaRec->ucBssIndex <= MAX_BSS_INDEX);

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);

	DBGLOG(AAA, LOUD, "TxDone ucStaState:%d, eAuthAssocState:%d\n",
		prStaRec->ucStaState, prStaRec->eAuthAssocState);

	/* Trigger statistics log if Auth/Assoc Tx failed */
	if (rTxDoneStatus != TX_RESULT_SUCCESS)
		wlanTriggerStatsLog(prAdapter, prAdapter->rWifiVar.u4StatsLogDuration);

	switch (prStaRec->eAuthAssocState) {
	case AAA_STATE_SEND_AUTH2:
		{
			/* Strictly check the outgoing frame is matched with current AA STATE */
			if (authCheckTxAuthFrame(prAdapter, prMsduInfo, AUTH_TRANSACTION_SEQ_2) != WLAN_STATUS_SUCCESS)
				break;

			cnmTimerStopTimer(prAdapter, &prStaRec->rTxReqDoneOrRxRespTimer);

			if (prStaRec->u2StatusCode == STATUS_CODE_SUCCESSFUL) {
				if (rTxDoneStatus == TX_RESULT_SUCCESS) {

					/* NOTE(Kevin): Change to STATE_2 at TX Done */
					cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_2);
					/* Error handle if can not complete the ASSOC flow */
					cnmTimerStartTimer(prAdapter,
							   &prStaRec->rTxReqDoneOrRxRespTimer,
							   TU_TO_MSEC(TX_ASSOCIATE_TIMEOUT_TU));
				} else {

					prStaRec->eAuthAssocState = AA_STATE_IDLE;

					/* NOTE(Kevin): Change to STATE_1 */
					cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_1);

#if CFG_ENABLE_WIFI_DIRECT
					if (prBssInfo->eNetworkType == NETWORK_TYPE_P2P)
						p2pRoleFsmRunEventAAATxFail(prAdapter, prStaRec, prBssInfo);
#endif /* CFG_ENABLE_WIFI_DIRECT */
#if CFG_ENABLE_BT_OVER_WIFI
					if (IS_STA_BOW_TYPE(prStaRec))
						bowRunEventAAATxFail(prAdapter, prStaRec);

#endif /* CFG_ENABLE_BT_OVER_WIFI */
				}

			}
			/* NOTE(Kevin): Ignore the TX Done Event of Auth Frame with Error Status Code */

		}
		break;

	case AAA_STATE_SEND_ASSOC2:
		{
			/* Strictly check the outgoing frame is matched with current SAA STATE */
			if (assocCheckTxReAssocRespFrame(prAdapter, prMsduInfo) != WLAN_STATUS_SUCCESS)
				break;

			if (prStaRec->u2StatusCode == STATUS_CODE_SUCCESSFUL) {
				if (rTxDoneStatus == TX_RESULT_SUCCESS) {

					prStaRec->eAuthAssocState = AA_STATE_IDLE;

					/* NOTE(Kevin): Change to STATE_3 at TX Done */
#if CFG_ENABLE_WIFI_DIRECT
					if (prBssInfo->eNetworkType == NETWORK_TYPE_P2P)
						p2pRoleFsmRunEventAAASuccess(prAdapter, prStaRec, prBssInfo);
#endif /* CFG_ENABLE_WIFI_DIRECT */

#if CFG_ENABLE_BT_OVER_WIFI

					if (IS_STA_BOW_TYPE(prStaRec))
						bowRunEventAAAComplete(prAdapter, prStaRec);

#endif /* CFG_ENABLE_BT_OVER_WIFI */

				} else {

					prStaRec->eAuthAssocState = AAA_STATE_SEND_AUTH2;

					/* NOTE(Kevin): Change to STATE_2 */
					cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_2);

#if CFG_ENABLE_WIFI_DIRECT
					if (prBssInfo->eNetworkType == NETWORK_TYPE_P2P)
						p2pRoleFsmRunEventAAATxFail(prAdapter, prStaRec, prBssInfo);
#endif /* CFG_ENABLE_WIFI_DIRECT */

#if CFG_ENABLE_BT_OVER_WIFI
					if (IS_STA_BOW_TYPE(prStaRec))
						bowRunEventAAATxFail(prAdapter, prStaRec);

#endif /* CFG_ENABLE_BT_OVER_WIFI */

				}
			}
			/* NOTE(Kevin): Ignore the TX Done Event of Auth Frame with Error Status Code */
		}
		break;

	case AA_STATE_IDLE:
		/* 2013-08-27 frog:  Do nothing.
		 * Somtimes we may send Assoc Resp twice. (Rx Assoc Req before the first Assoc TX Done)
		 * The AssocState is changed to IDLE after first TX done.
		 * Free station record when IDLE is seriously wrong.
		 */
		/* 2017-01-12 Do nothing only when STA is in state 3 */
		/* Free the StaRec if found any unexpected status */
		if (prStaRec->ucStaState != STA_STATE_3)
			cnmStaRecFree(prAdapter, prStaRec);
		break;

	default:
		break;		/* Ignore other cases */
	}

	DBGLOG(AAA, LOUD, "TxDone end ucStaState:%d, eAuthAssocState:%d\n",
		prStaRec->ucStaState, prStaRec->eAuthAssocState);

	return WLAN_STATUS_SUCCESS;

}				/* end of aaaFsmRunEventTxDone() */
#endif /* CFG_SUPPORT_AAA */

#if 0				/* TODO(Kevin): for abort event, just reset the STA_RECORD_T. */
/*----------------------------------------------------------------------------*/
/*!
* \brief This function will send ABORT Event to JOIN FSM.
*
* \param[in] prAdapter  Pointer to the Adapter structure.
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID saaFsmRunEventAbort(IN P_MSG_HDR_T prMsgHdr)
{
	P_JOIN_INFO_T prJoinInfo;
	P_STA_RECORD_T prStaRec;

	DEBUGFUNC("joinFsmRunEventAbort");

	ASSERT(prAdapter);
	prJoinInfo = &prAdapter->rJoinInfo;

	DBGLOG(JOIN, EVENT, "JOIN EVENT: ABORT\n");

	/* NOTE(Kevin): when reach here, the ARB_STATE should be in ARB_STATE_JOIN. */
	ASSERT(prJoinInfo->prBssDesc);

	/* 4 <1> Update Flags and Elements of JOIN Module. */
	/* Reset Send Auth/(Re)Assoc Frame Count */
	prJoinInfo->ucTxAuthAssocRetryCount = 0;

	/* Cancel all JOIN relative Timer */
	ARB_CANCEL_TIMER(prAdapter, prJoinInfo->rTxRequestTimer);

	ARB_CANCEL_TIMER(prAdapter, prJoinInfo->rRxResponseTimer);

	ARB_CANCEL_TIMER(prAdapter, prJoinInfo->rJoinTimer);

	/* 4 <2> Update the associated STA_RECORD_T during JOIN. */
	/* Get a Station Record if possible, TA == BSSID for AP */
	prStaRec = staRecGetStaRecordByAddr(prAdapter, prJoinInfo->prBssDesc->aucBSSID);
	if (prStaRec)
		prStaRec->ucStaState = STA_STATE_1;	/* Update Station Record - Class 1 Flag */
#if DBG
	else
		ASSERT(0);	/* Shouldn't happened, because we already add this STA_RECORD_T at JOIN_STATE_INIT */

#endif /* DBG */

	/* 4 <3> Pull back to IDLE. */
	joinFsmSteps(prAdapter, JOIN_STATE_IDLE);

	/* 4 <4> If we are in Roaming, recover the settings of previous BSS. */
	/* NOTE: JOIN FAIL -
	 * Restore original setting from current BSS_INFO_T.
	 */
	if (prAdapter->eConnectionState == MEDIA_STATE_CONNECTED)
		joinAdoptParametersFromCurrentBss(prAdapter);
}				/* end of joinFsmRunEventAbort() */
#endif

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
		 * <AssocReq>     <AssocResp w/i ERP>     <BARKER(Long)>  Final Driver Setting(Short)
		 * TRUE            FALSE                  FALSE           FALSE(shouldn't have such case,
		 *                                                        use the AssocResp)
		 * TRUE            FALSE                  TRUE            FALSE
		 * FALSE           FALSE                  FALSE           FALSE(shouldn't have such case,
		 *                                                        use the AssocResp)
		 * FALSE           FALSE                  TRUE            FALSE
		 * TRUE            TRUE                   FALSE           TRUE(follow ERP)
		 * TRUE            TRUE                   TRUE            FALSE(follow ERP)
		 * FALSE           TRUE                   FALSE           FALSE(shouldn't have such case,
		 *                                                        and we should set to FALSE)
		 * FALSE           TRUE                   TRUE            FALSE(we should set to FALSE)
		 */
		if ((prPeerBssInfo->fgIsShortPreambleAllowed) &&
		    ((prConnSettings->ePreambleType == PREAMBLE_TYPE_SHORT) ||
		     /* Short Preamble Option Enable is TRUE */
		     ((prConnSettings->ePreambleType == PREAMBLE_TYPE_AUTO)
		      && (prBssDesc->u2CapInfo & CAP_INFO_SHORT_PREAMBLE)))) {

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
