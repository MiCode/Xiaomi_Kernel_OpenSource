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
 ** Id:
 */

/*! \file   "roaming_fsm.c"
 *    \brief  This file defines the FSM for Roaming MODULE.
 *
 *    This file defines the FSM for Roaming MODULE.
 */


/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include "precomp.h"

#if CFG_SUPPORT_ROAMING
/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */
static uint8_t *apucDebugRoamingState[ROAMING_STATE_NUM] = {
	(uint8_t *) DISP_STRING("IDLE"),
	(uint8_t *) DISP_STRING("DECISION"),
	(uint8_t *) DISP_STRING("DISCOVERY"),
	(uint8_t *) DISP_STRING("REQ_CAND_LIST"),
	(uint8_t *) DISP_STRING("ROAM")
};


/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
static void roamingWaitCandidateTimeout(IN struct ADAPTER *prAdapter,
					unsigned long ulParamPtr)
{
	DBGLOG(ROAMING, INFO,
	       "Time out, Waiting for neighbor response");

	roamingFsmSteps(prAdapter, ROAMING_STATE_DISCOVERY);
}
/*----------------------------------------------------------------------------*/
/*!
 * @brief Initialize the value in ROAMING_FSM_INFO_T for ROAMING FSM operation
 *
 * @param [IN P_ADAPTER_T] prAdapter
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void roamingFsmInit(IN struct ADAPTER *prAdapter)
{
	struct ROAMING_INFO *prRoamingFsmInfo;
	struct CONNECTION_SETTINGS *prConnSettings;

	DBGLOG(ROAMING, LOUD,
	       "->roamingFsmInit(): Current Time = %d\n",
	       kalGetTimeTick());

	prRoamingFsmInfo = (struct ROAMING_INFO *) &
			   (prAdapter->rWifiVar.rRoamingInfo);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	/* 4 <1> Initiate FSM */
	prRoamingFsmInfo->fgIsEnableRoaming =
		prConnSettings->fgIsEnableRoaming;
	prRoamingFsmInfo->eCurrentState = ROAMING_STATE_IDLE;
	prRoamingFsmInfo->rRoamingDiscoveryUpdateTime = 0;
	prRoamingFsmInfo->fgDrvRoamingAllow = TRUE;
	cnmTimerInitTimer(prAdapter, &prRoamingFsmInfo->rWaitCandidateTimer,
			  (PFN_MGMT_TIMEOUT_FUNC)roamingWaitCandidateTimeout,
			  (unsigned long)NULL);
}				/* end of roamingFsmInit() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief Uninitialize the value in AIS_FSM_INFO_T for AIS FSM operation
 *
 * @param [IN P_ADAPTER_T] prAdapter
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void roamingFsmUninit(IN struct ADAPTER *prAdapter)
{
	struct ROAMING_INFO *prRoamingFsmInfo;

	DBGLOG(ROAMING, LOUD,
	       "->roamingFsmUninit(): Current Time = %d\n",
	       kalGetTimeTick());

	prRoamingFsmInfo = (struct ROAMING_INFO *) &
			   (prAdapter->rWifiVar.rRoamingInfo);

	prRoamingFsmInfo->eCurrentState = ROAMING_STATE_IDLE;
	cnmTimerStopTimer(prAdapter, &prRoamingFsmInfo->rWaitCandidateTimer);
}				/* end of roamingFsmUninit() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief Send commands to firmware
 *
 * @param [IN P_ADAPTER_T]       prAdapter
 *        [IN P_ROAMING_PARAM_T] prParam
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void roamingFsmSendCmd(IN struct ADAPTER *prAdapter,
		       IN struct CMD_ROAMING_TRANSIT *prTransit)
{
	struct ROAMING_INFO *prRoamingFsmInfo;
	uint32_t rStatus;

	DBGLOG(ROAMING, LOUD,
	       "->roamingFsmSendCmd(): Current Time = %d\n",
	       kalGetTimeTick());

	prRoamingFsmInfo = (struct ROAMING_INFO *) &
			   (prAdapter->rWifiVar.rRoamingInfo);

	rStatus = wlanSendSetQueryCmd(prAdapter,	/* prAdapter */
				      CMD_ID_ROAMING_TRANSIT,	/* ucCID */
				      TRUE,	/* fgSetQuery */
				      FALSE,	/* fgNeedResp */
				      FALSE,	/* fgIsOid */
				      NULL,	/* pfCmdDoneHandler */
				      NULL,	/* pfCmdTimeoutHandler */
				      sizeof(struct CMD_ROAMING_TRANSIT),
				      /* u4SetQueryInfoLen */
				      (uint8_t *)prTransit, /* pucInfoBuffer */
				      NULL,	/* pvSetQueryBuffer */
				      0	/* u4SetQueryBufferLen */
				     );

	/* ASSERT(rStatus == WLAN_STATUS_PENDING); */
}				/* end of roamingFsmSendCmd() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief Update the recent time when ScanDone occurred
 *
 * @param [IN P_ADAPTER_T] prAdapter
 *
 * @return none
 */
/*----------------------------------------------------------------------------*/
void roamingFsmScanResultsUpdate(IN struct ADAPTER
				 *prAdapter)
{
	struct ROAMING_INFO *prRoamingFsmInfo;

	prRoamingFsmInfo = (struct ROAMING_INFO *) &
			   (prAdapter->rWifiVar.rRoamingInfo);

	/* Check Roaming Conditions */
	if (!(prRoamingFsmInfo->fgIsEnableRoaming))
		return;


	DBGLOG(ROAMING, LOUD,
	       "->roamingFsmScanResultsUpdate(): Current Time = %d\n",
	       kalGetTimeTick());

	GET_CURRENT_SYSTIME(
		&prRoamingFsmInfo->rRoamingDiscoveryUpdateTime);
}				/* end of roamingFsmScanResultsUpdate() */

#if CFG_SUPPORT_ROAMING_SKIP_ONE_AP
/*----------------------------------------------------------------------------*/
/*
 * @brief Check if need to do scan for roaming
 *
 * @param[out] fgIsNeedScan Set to TRUE if need to scan since
 *		there is roaming candidate in current scan result
 *		or skip roaming times > limit times
 * @return
 */
/*----------------------------------------------------------------------------*/
static u_int8_t roamingFsmIsNeedScan(IN struct ADAPTER
				     *prAdapter)
{
	struct SCAN_INFO *prScanInfo;
	struct LINK *prRoamBSSDescList;
	struct ROAM_BSS_DESC *prRoamBssDesc;
	struct BSS_INFO *prAisBssInfo;
	struct BSS_DESC *prBssDesc;
	/*CMD_SW_DBG_CTRL_T rCmdSwCtrl;*/
	struct CMD_ROAMING_SKIP_ONE_AP rCmdRoamingSkipOneAP;
	u_int8_t fgIsNeedScan, fgIsRoamingSSID;

	fgIsNeedScan = FALSE;

	/*Whether there's roaming candidate in RoamBssDescList*/
	fgIsRoamingSSID = FALSE;

	kalMemZero(&rCmdRoamingSkipOneAP,
		   sizeof(struct CMD_ROAMING_SKIP_ONE_AP));

	prAisBssInfo = prAdapter->prAisBssInfo;
	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prRoamBSSDescList = &prScanInfo->rRoamBSSDescList;
	/* <1> Count same BSS Desc from current SCAN result list. */
	LINK_FOR_EACH_ENTRY(prRoamBssDesc, prRoamBSSDescList,
			    rLinkEntry, struct ROAM_BSS_DESC) {
		if (EQUAL_SSID(prRoamBssDesc->aucSSID,
			       prRoamBssDesc->ucSSIDLen,
			       prAisBssInfo->aucSSID,
			       prAisBssInfo->ucSSIDLen)) {
			fgIsRoamingSSID = TRUE;
			fgIsNeedScan = TRUE;
			DBGLOG(ROAMING, INFO,
				"roamingFsmSteps: IsRoamingSSID:%d\n",
			       fgIsRoamingSSID);
			break;
		}
	}

	/* <2> Start skip roaming scan mechanism
	 *	if there is no candidate in current SCAN result list
	 */
	if (!fgIsRoamingSSID) {
		/* Get current BssDesc */
		prBssDesc = prAdapter->rWifiVar.rAisFsmInfo.prTargetBssDesc;
		if (prBssDesc) {

			/*rCmdSwCtrl.u4Id = 0xa0280000;*/
			/*rCmdSwCtrl.u4Data = 0x1;*/
			rCmdRoamingSkipOneAP.fgIsRoamingSkipOneAP = 1;

			DBGLOG(ROAMING, INFO,
			       "roamingFsmSteps: RCPI:%d RoamSkipTimes:%d\n",
			       prBssDesc->ucRCPI,
			       prAisBssInfo->ucRoamSkipTimes);
			if (prBssDesc->ucRCPI >
			    90) { /* Set parameters related to Good Area */
				prAisBssInfo->ucRoamSkipTimes = 3;
				prAisBssInfo->fgGoodRcpiArea = TRUE;
				prAisBssInfo->fgPoorRcpiArea = FALSE;
			} else {
				if (prAisBssInfo->fgGoodRcpiArea) {
					prAisBssInfo->ucRoamSkipTimes--;
				} else if (prBssDesc->ucRCPI > 67) {
					if (!prAisBssInfo->fgPoorRcpiArea) {
					/*Set parameters related to Poor Area*/
						prAisBssInfo->ucRoamSkipTimes
							= 2;
						prAisBssInfo->fgPoorRcpiArea
							= TRUE;
						prAisBssInfo->fgGoodRcpiArea
							= FALSE;
					} else {
						prAisBssInfo->ucRoamSkipTimes--;
					}
				} else {
					prAisBssInfo->fgPoorRcpiArea = FALSE;
					prAisBssInfo->fgGoodRcpiArea = FALSE;
					prAisBssInfo->ucRoamSkipTimes--;
				}
			}

			if (prAisBssInfo->ucRoamSkipTimes == 0) {
				prAisBssInfo->ucRoamSkipTimes = 3;
				prAisBssInfo->fgPoorRcpiArea = FALSE;
				prAisBssInfo->fgGoodRcpiArea = FALSE;
				DBGLOG(ROAMING, INFO,
					"roamingFsmSteps: Need Scan\n");
				fgIsNeedScan = TRUE;
			} else
				wlanSendSetQueryCmd(prAdapter,
				    CMD_ID_SET_ROAMING_SKIP,
				    TRUE,
				    FALSE,
				    FALSE, NULL, NULL,
				    sizeof(struct CMD_ROAMING_SKIP_ONE_AP),
				    (uint8_t *)&rCmdRoamingSkipOneAP, NULL, 0);
		} else
			DBGLOG(ROAMING, WARN,
			       "Target BssDesc in AisFsmInfo is NULL\n");
	}

	return fgIsNeedScan;
}
#endif

/*----------------------------------------------------------------------------*/
/*!
 * @brief The Core FSM engine of ROAMING for AIS Infra.
 *
 * @param [IN P_ADAPTER_T]          prAdapter
 *        [IN ENUM_ROAMING_STATE_T] eNextState Enum value of next AIS STATE
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void roamingFsmSteps(IN struct ADAPTER *prAdapter,
		     IN enum ENUM_ROAMING_STATE eNextState)
{
	struct ROAMING_INFO *prRoamingFsmInfo;
	enum ENUM_ROAMING_STATE ePreviousState;
	u_int8_t fgIsTransition = (u_int8_t) FALSE;
	u_int8_t fgIsNeedScan = FALSE;
#if CFG_SUPPORT_NCHO
	uint32_t u4ScnResultsTimeout =
		ROAMING_DISCOVERY_TIMEOUT_SEC;
	uint32_t u4ReqScan = FALSE;
#endif

	prRoamingFsmInfo = (struct ROAMING_INFO *) &
			   (prAdapter->rWifiVar.rRoamingInfo);

	do {

		/* Do entering Next State */
		DBGLOG(ROAMING, STATE,
		       "[ROAMING]TRANSITION: [%s] -> [%s]\n",
		       apucDebugRoamingState[prRoamingFsmInfo->eCurrentState],
		       apucDebugRoamingState[eNextState]);

		/* NOTE(Kevin): This is the only place to
		 *    change the eCurrentState(except initial)
		 */
		ePreviousState = prRoamingFsmInfo->eCurrentState;
		prRoamingFsmInfo->eCurrentState = eNextState;

		fgIsTransition = (u_int8_t) FALSE;

		/* Do tasks of the State that we just entered */
		switch (prRoamingFsmInfo->eCurrentState) {
		/* NOTE(Kevin): we don't have to rearrange the sequence of
		 *   following switch case. Instead I would like to use a common
		 *   lookup table of array of function pointer
		 *   to speed up state search.
		 */
		case ROAMING_STATE_IDLE:
		case ROAMING_STATE_DECISION:
			break;

		case ROAMING_STATE_DISCOVERY: {
#if CFG_SUPPORT_NCHO
			if (prAdapter->rNchoInfo.fgECHOEnabled == TRUE) {
				u4ScnResultsTimeout =
					prAdapter->rNchoInfo.u4RoamScanPeriod;
				DBGLOG(ROAMING, TRACE,
					"NCHO u4ScnResultsTimeout is %d\n",
				       u4ScnResultsTimeout);
			}

			if (CHECK_FOR_TIMEOUT(kalGetTimeTick(),
			      prRoamingFsmInfo->rRoamingDiscoveryUpdateTime,
			      SEC_TO_SYSTIME(u4ScnResultsTimeout))) {
				DBGLOG(ROAMING, LOUD,
					"DiscoveryUpdateTime Timeout");
				u4ReqScan =  TRUE;
			} else {
				DBGLOG(ROAMING, LOUD,
					"DiscoveryUpdateTime Updated");
				u4ReqScan = FALSE;
			}
			aisFsmRunEventRoamingDiscovery(prAdapter, u4ReqScan);
#else
			OS_SYSTIME rCurrentTime;
#if CFG_SUPPORT_ROAMING_SKIP_ONE_AP
			fgIsNeedScan = roamingFsmIsNeedScan(prAdapter);
#else
			fgIsNeedScan = TRUE;
#endif

			cnmTimerStopTimer(
				prAdapter,
				&prRoamingFsmInfo->rWaitCandidateTimer);

			GET_CURRENT_SYSTIME(&rCurrentTime);
			if (CHECK_FOR_TIMEOUT(rCurrentTime,
			      prRoamingFsmInfo->rRoamingDiscoveryUpdateTime,
			      SEC_TO_SYSTIME(ROAMING_DISCOVERY_TIMEOUT_SEC))
				    && fgIsNeedScan) {
				DBGLOG(ROAMING, LOUD,
			     "roamingFsmSteps: DiscoveryUpdateTime Timeout\n");
				aisFsmRunEventRoamingDiscovery(prAdapter,
								TRUE);
			} else {
				DBGLOG(ROAMING, LOUD,
			     "roamingFsmSteps: DiscoveryUpdateTime Updated\n");
				aisFsmRunEventRoamingDiscovery(prAdapter,
								FALSE);
			}
#endif /* CFG_SUPPORT_NCHO */
		}
		break;
		case ROAMING_STATE_REQ_CAND_LIST:
		{
#if CFG_SUPPORT_802_11K
			struct BSS_INFO *prBssInfo = prAdapter->prAisBssInfo;
			struct BSS_DESC *prBssDesc =
				prAdapter->rWifiVar.rAisFsmInfo.prTargetBssDesc;
			/* if AP supports Neighbor AP report, then it can used
			** to assist roaming candicate selection
			*/
			if (prBssInfo && prBssInfo->prStaRecOfAP) {
				if (prBssDesc &&
				    (prBssDesc->aucRrmCap[0] &
				     BIT(RRM_CAP_INFO_NEIGHBOR_REPORT_BIT))) {
					aisSendNeighborRequest(prAdapter);
					cnmTimerStartTimer(
						prAdapter,
						&prRoamingFsmInfo
							 ->rWaitCandidateTimer,
						100);
				}
			}
#endif
			fgIsTransition = TRUE;
			eNextState = ROAMING_STATE_DISCOVERY;
			break;
		}
		case ROAMING_STATE_ROAM:
			break;

		default:
			ASSERT(0); /* Make sure we have handle all STATEs */
		}
	} while (fgIsTransition);

	return;

}				/* end of roamingFsmSteps() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief Transit to Decision state after join completion
 *
 * @param [IN P_ADAPTER_T] prAdapter
 *
 * @return none
 */
/*----------------------------------------------------------------------------*/
void roamingFsmRunEventStart(IN struct ADAPTER *prAdapter)
{
	struct ROAMING_INFO *prRoamingFsmInfo;
	enum ENUM_ROAMING_STATE eNextState;
	struct BSS_INFO *prAisBssInfo;
	struct CMD_ROAMING_TRANSIT rTransit;

	prRoamingFsmInfo = (struct ROAMING_INFO *) &
			   (prAdapter->rWifiVar.rRoamingInfo);

	/* Check Roaming Conditions */
	if (!(prRoamingFsmInfo->fgIsEnableRoaming))
		return;


	prAisBssInfo = prAdapter->prAisBssInfo;
	if (prAisBssInfo->eCurrentOPMode != OP_MODE_INFRASTRUCTURE)
		return;

	DBGLOG(ROAMING, EVENT,
	       "EVENT-ROAMING START: Current Time = %d\n",
	       kalGetTimeTick());

	/* IDLE, ROAM -> DECISION */
	/* Errors as DECISION, DISCOVERY -> DECISION */
	if (!(prRoamingFsmInfo->eCurrentState == ROAMING_STATE_IDLE
	      || prRoamingFsmInfo->eCurrentState == ROAMING_STATE_ROAM))
		return;

	eNextState = ROAMING_STATE_DECISION;
	if (eNextState != prRoamingFsmInfo->eCurrentState) {
		rTransit.u2Event = ROAMING_EVENT_START;
		rTransit.u2Data = prAisBssInfo->ucBssIndex;
		roamingFsmSendCmd(prAdapter,
				  (struct CMD_ROAMING_TRANSIT *) &rTransit);

		/* Step to next state */
		roamingFsmSteps(prAdapter, eNextState);
	}
}				/* end of roamingFsmRunEventStart() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief Transit to Discovery state when deciding to find a candidate
 *
 * @param [IN P_ADAPTER_T] prAdapter
 *
 * @return none
 */
/*----------------------------------------------------------------------------*/
void roamingFsmRunEventDiscovery(IN struct ADAPTER *prAdapter,
			IN struct CMD_ROAMING_TRANSIT *prTransit)
{
	struct ROAMING_INFO *prRoamingFsmInfo;
	enum ENUM_ROAMING_STATE eNextState;

	prRoamingFsmInfo = (struct ROAMING_INFO *) &
			   (prAdapter->rWifiVar.rRoamingInfo);

	/* Check Roaming Conditions */
	if (!(prRoamingFsmInfo->fgIsEnableRoaming))
		return;

	DBGLOG(ROAMING, EVENT,
	       "EVENT-ROAMING DISCOVERY: Current Time = %d\n",
	       kalGetTimeTick());

	/* DECISION -> DISCOVERY */
	/* Errors as IDLE, DISCOVERY, ROAM -> DISCOVERY */
	if (prRoamingFsmInfo->eCurrentState !=
	    ROAMING_STATE_DECISION)
		return;

	eNextState = ROAMING_STATE_REQ_CAND_LIST;
	/* DECISION -> DISCOVERY */
	if (eNextState != prRoamingFsmInfo->eCurrentState) {
		struct BSS_INFO *prAisBssInfo;
		struct BSS_DESC *prBssDesc;
		uint8_t arBssid[PARAM_MAC_ADDR_LEN];
		struct PARAM_SSID rSsid;
		struct AIS_FSM_INFO *prAisFsmInfo;
		struct CONNECTION_SETTINGS *prConnSettings;

		kalMemZero(&rSsid, sizeof(struct PARAM_SSID));
		prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
		prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

		/* sync. rcpi with firmware */
		prAisBssInfo =
			&(prAdapter->rWifiVar.arBssInfoPool[NETWORK_TYPE_AIS]);
		prBssDesc = prAisFsmInfo->prTargetBssDesc;
		if (prBssDesc) {
			COPY_SSID(rSsid.aucSsid, rSsid.u4SsidLen,
				  prBssDesc->aucSSID, prBssDesc->ucSSIDLen);
			COPY_MAC_ADDR(arBssid, prBssDesc->aucBSSID);
		} else  {
			COPY_SSID(rSsid.aucSsid, rSsid.u4SsidLen,
				  prConnSettings->aucSSID,
				  prConnSettings->ucSSIDLen);
			COPY_MAC_ADDR(arBssid, prConnSettings->aucBSSID);
		}
		prBssDesc = scanSearchBssDescByBssidAndSsid(prAdapter,
				arBssid, TRUE, &rSsid);
		if (prBssDesc) {
			prBssDesc->ucRCPI = (uint8_t)(prTransit->u2Data & 0xff);
			DBGLOG(ROAMING, INFO, "ucRCPI %u\n",
				prBssDesc->ucRCPI);
		}
		roamingFsmSteps(prAdapter, eNextState);
	}
}				/* end of roamingFsmRunEventDiscovery() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief Transit to Roam state after Scan Done
 *
 * @param [IN P_ADAPTER_T] prAdapter
 *
 * @return none
 */
/*----------------------------------------------------------------------------*/
void roamingFsmRunEventRoam(IN struct ADAPTER *prAdapter)
{
	struct ROAMING_INFO *prRoamingFsmInfo;
	enum ENUM_ROAMING_STATE eNextState;
	struct CMD_ROAMING_TRANSIT rTransit;

	prRoamingFsmInfo = (struct ROAMING_INFO *) &
			   (prAdapter->rWifiVar.rRoamingInfo);

	/* Check Roaming Conditions */
	if (!(prRoamingFsmInfo->fgIsEnableRoaming))
		return;


	DBGLOG(ROAMING, EVENT,
	       "EVENT-ROAMING ROAM: Current Time = %d\n",
	       kalGetTimeTick());

	/* IDLE, ROAM -> DECISION */
	/* Errors as IDLE, DECISION, ROAM -> ROAM */
	if (prRoamingFsmInfo->eCurrentState !=
	    ROAMING_STATE_DISCOVERY)
		return;

	eNextState = ROAMING_STATE_ROAM;
	/* DISCOVERY -> ROAM */
	if (eNextState != prRoamingFsmInfo->eCurrentState) {
		rTransit.u2Event = ROAMING_EVENT_ROAM;
		roamingFsmSendCmd(prAdapter,
				  (struct CMD_ROAMING_TRANSIT *) &rTransit);

		/* Step to next state */
		roamingFsmSteps(prAdapter, eNextState);
	}
}				/* end of roamingFsmRunEventRoam() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief Transit to Decision state as being failed to find out any candidate
 *
 * @param [IN P_ADAPTER_T] prAdapter
 *
 * @return none
 */
/*----------------------------------------------------------------------------*/
void roamingFsmRunEventFail(IN struct ADAPTER *prAdapter,
			    IN uint32_t u4Param)
{
	struct ROAMING_INFO *prRoamingFsmInfo;
	enum ENUM_ROAMING_STATE eNextState;
	struct CMD_ROAMING_TRANSIT rTransit;

	prRoamingFsmInfo = (struct ROAMING_INFO *) &
			   (prAdapter->rWifiVar.rRoamingInfo);

	/* Check Roaming Conditions */
	if (!(prRoamingFsmInfo->fgIsEnableRoaming))
		return;


	DBGLOG(ROAMING, STATE,
	       "EVENT-ROAMING FAIL: reason %x Current Time = %d\n",
	       u4Param, kalGetTimeTick());

	/* IDLE, ROAM -> DECISION */
	/* Errors as IDLE, DECISION, DISCOVERY -> DECISION */
	if (prRoamingFsmInfo->eCurrentState != ROAMING_STATE_ROAM)
		return;

	eNextState = ROAMING_STATE_DECISION;
	/* ROAM -> DECISION */
	if (eNextState != prRoamingFsmInfo->eCurrentState) {
		rTransit.u2Event = ROAMING_EVENT_FAIL;
		rTransit.u2Data = (uint16_t) (u4Param & 0xffff);
		roamingFsmSendCmd(prAdapter,
				  (struct CMD_ROAMING_TRANSIT *) &rTransit);

		/* Step to next state */
		roamingFsmSteps(prAdapter, eNextState);
	}
}				/* end of roamingFsmRunEventFail() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief Transit to Idle state as beging aborted by other moduels, AIS
 *
 * @param [IN P_ADAPTER_T] prAdapter
 *
 * @return none
 */
/*----------------------------------------------------------------------------*/
void roamingFsmRunEventAbort(IN struct ADAPTER *prAdapter)
{
	struct ROAMING_INFO *prRoamingFsmInfo;
	enum ENUM_ROAMING_STATE eNextState;
	struct CMD_ROAMING_TRANSIT rTransit;

	prRoamingFsmInfo = (struct ROAMING_INFO *) &
			   (prAdapter->rWifiVar.rRoamingInfo);

	/* Check Roaming Conditions */
	if (!(prRoamingFsmInfo->fgIsEnableRoaming))
		return;


	DBGLOG(ROAMING, EVENT,
	       "EVENT-ROAMING ABORT: Current Time = %d\n",
	       kalGetTimeTick());

	eNextState = ROAMING_STATE_IDLE;
	/* IDLE, DECISION, DISCOVERY, ROAM -> IDLE */
	if (eNextState != prRoamingFsmInfo->eCurrentState) {
		rTransit.u2Event = ROAMING_EVENT_ABORT;
		roamingFsmSendCmd(prAdapter,
				  (struct CMD_ROAMING_TRANSIT *) &rTransit);

		/* Step to next state */
		roamingFsmSteps(prAdapter, eNextState);
	}
}				/* end of roamingFsmRunEventAbort() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief Process events from firmware
 *
 * @param [IN P_ADAPTER_T]       prAdapter
 *        [IN P_ROAMING_PARAM_T] prParam
 *
 * @return none
 */
/*----------------------------------------------------------------------------*/
uint32_t roamingFsmProcessEvent(IN struct ADAPTER *prAdapter,
			IN struct CMD_ROAMING_TRANSIT *prTransit)
{
	DBGLOG(ROAMING, LOUD,
	       "ROAMING Process Events: Current Time = %d\n",
	       kalGetTimeTick());

	if (prTransit->u2Event == ROAMING_EVENT_DISCOVERY) {
		roamingFsmRunEventDiscovery(prAdapter, prTransit);

#if 0
		DBGLOG(ROAMING, INFO,
		       "RX ROAMING_EVENT_DISCOVERY RCPI[%d] Thr[%d] Reason[%d] Time[%ld]\n",
		       prTransit->u2Data,
		       prTransit->u2RcpiLowThreshold,
		       prTransit->eReason,
		       prTransit->u4RoamingTriggerTime);
#endif
	}

	return WLAN_STATUS_SUCCESS;
}

#endif
