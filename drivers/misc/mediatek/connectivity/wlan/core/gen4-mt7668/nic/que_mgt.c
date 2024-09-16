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
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/nic/que_mgt.c#3
*/

/*! \file   "que_mgt.c"
*    \brief  TX/RX queues management
*
*    The main tasks of queue management include TC-based HIF TX flow control,
*    adaptive TC quota adjustment, HIF TX grant scheduling, Power-Save
*    forwarding control, RX packet reordering, and RX BA agreement management.
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
#include "queue.h"

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
OS_SYSTIME g_arMissTimeout[CFG_STA_REC_NUM][CFG_RX_MAX_BA_TID_NUM];

const UINT_8 aucTid2ACI[TX_DESC_TID_NUM] = {
	WMM_AC_BE_INDEX,	/* TID0 */
	WMM_AC_BK_INDEX,	/* TID1 */
	WMM_AC_BK_INDEX,	/* TID2 */
	WMM_AC_BE_INDEX,	/* TID3 */
	WMM_AC_VI_INDEX,	/* TID4 */
	WMM_AC_VI_INDEX,	/* TID5 */
	WMM_AC_VO_INDEX,	/* TID6 */
	WMM_AC_VO_INDEX		/* TID7 */
};

const UINT_8 aucACI2TxQIdx[WMM_AC_INDEX_NUM] = {
	TX_QUEUE_INDEX_AC1,	/* WMM_AC_BE_INDEX */
	TX_QUEUE_INDEX_AC0,	/* WMM_AC_BK_INDEX */
	TX_QUEUE_INDEX_AC2,	/* WMM_AC_VI_INDEX */
	TX_QUEUE_INDEX_AC3	/* WMM_AC_VO_INDEX */
};

const PUINT_8 apucACI2Str[WMM_AC_INDEX_NUM] = {
	"BE", "BK", "VI", "VO"
};

const UINT_8 arNetwork2TcResource[HW_BSSID_NUM + 1][NET_TC_NUM] = {
	/* HW Queue Set 1 */
	/* AC_BE, AC_BK, AC_VI, AC_VO, MGMT, BMC */
	{TC1_INDEX, TC0_INDEX, TC2_INDEX, TC3_INDEX, TC4_INDEX, BMC_TC_INDEX},	/* AIS */
	{TC1_INDEX, TC0_INDEX, TC2_INDEX, TC3_INDEX, TC4_INDEX, BMC_TC_INDEX},	/* P2P/BoW */
	{TC1_INDEX, TC0_INDEX, TC2_INDEX, TC3_INDEX, TC4_INDEX, BMC_TC_INDEX},	/* P2P/BoW */
	{TC1_INDEX, TC0_INDEX, TC2_INDEX, TC3_INDEX, TC4_INDEX, BMC_TC_INDEX},	/* P2P/BoW */
	{TC1_INDEX, TC0_INDEX, TC2_INDEX, TC3_INDEX, TC4_INDEX, BMC_TC_INDEX},	/* P2P_DEV */


	/* HW Queue Set 2 */
	/* {TC7_INDEX, TC6_INDEX, TC8_INDEX, TC9_INDEX, TC4_INDEX, TC10_INDEX}, */
};

const UINT_8 aucWmmAC2TcResourceSet1[WMM_AC_INDEX_NUM] = {
	TC1_INDEX,
	TC0_INDEX,
	TC2_INDEX,
	TC3_INDEX
};

#if NIC_TX_ENABLE_SECOND_HW_QUEUE
const UINT_8 aucWmmAC2TcResourceSet2[WMM_AC_INDEX_NUM] = {
	TC7_INDEX,
	TC6_INDEX,
	TC8_INDEX,
	TC9_INDEX
};
#endif
/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

#if CFG_RX_REORDERING_ENABLED
#define qmHandleRxPackets_AOSP_1 \
do { \
	/* ToDo[6630]: duplicate removal */ \
	if (!fgIsBMC && nicRxIsDuplicateFrame(prCurrSwRfb) == TRUE) { \
		DBGLOG(QM, TRACE, "Duplicated packet is detected\n"); \
		RX_INC_CNT(&prAdapter->rRxCtrl, RX_DUPICATE_DROP_COUNT); \
		prCurrSwRfb->eDst = RX_PKT_DESTINATION_NULL; \
	} \
	/* ToDo[6630]: defragmentation */ \
	if (prCurrSwRfb->fgFragFrame) { \
		prCurrSwRfb = incRxDefragMPDU(prAdapter, prCurrSwRfb, prReturnedQue); \
		if (prCurrSwRfb) { \
			prRxStatus = prCurrSwRfb->prRxStatus; \
			DBGLOG(QM, TRACE, "defragmentation RxStatus=%p\n", \
			prRxStatus); \
		} \
	} \
	if (prCurrSwRfb) { \
		fgMicErr = FALSE; \
		if (HAL_RX_STATUS_GET_SEC_MODE(prRxStatus) == CIPHER_SUITE_TKIP_WO_MIC) { \
			if (prCurrSwRfb->prStaRec) { \
				UINT_8 ucBssIndex; \
				P_BSS_INFO_T prBssInfo = NULL; \
				PUINT_8      pucMicKey = NULL; \
				ucBssIndex = prCurrSwRfb->prStaRec->ucBssIndex; \
				ASSERT(ucBssIndex < BSS_INFO_NUM); \
				prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex); \
				ASSERT(prBssInfo); \
				if (prBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE) { \
					pucMicKey = &(prAdapter->rWifiVar.rAisSpecificBssInfo.aucRxMicKey[0]); \
					prCurrSwRfb->ucTid = (UINT_8) (HAL_RX_STATUS_GET_TID(prRxStatus)); \
				} \
				else { \
					ASSERT(FALSE); \
					/* pucMicKey = &prCurrSwRfb->prStaRec->aucRxMicKey[0]; */ \
				} \
				/* SW TKIP MIC verify, adopt new function call for MIC calculation */ \
				/* TODO:[6630] Need to Check Header Translation Case */ \
				if (pucMicKey == NULL) { \
					DBGLOG(RX, ERROR, "Mark NULL the Packet for TKIP Key Error\n"); \
					fgMicErr = TRUE; \
				} \
				else if (tkipMicDecapsulateInRxHdrTransMode(prCurrSwRfb, pucMicKey) == FALSE) { \
					fgMicErr = TRUE; \
				} \
			} \
			if (fgMicErr) { \
				/* bypass tkip frag */ \
				if (!prCurrSwRfb->fgFragFrame) { \
					DBGLOG(RX, ERROR, "Mark NULL the Packet for TKIP Mic Error\n"); \
					RX_INC_CNT(&prAdapter->rRxCtrl, RX_MIC_ERROR_DROP_COUNT); \
					prCurrSwRfb->eDst = RX_PKT_DESTINATION_NULL; \
				} \
			} \
		} \
		QUEUE_INSERT_TAIL(prReturnedQue, (P_QUE_ENTRY_T)prCurrSwRfb); \
	} \
} while (0)
#endif

#define RX_DIRECT_REORDER_LOCK(pad, dbg) \
do { \
	P_GLUE_INFO_T _glue = pad->prGlueInfo; \
	if (!HAL_IS_RX_DIRECT(pad) || !_glue) \
		break; \
	if (dbg) \
		DBGLOG(QM, EVENT, "RX_DIRECT_REORDER_LOCK %d\n", __LINE__); \
	spin_lock_bh(&_glue->rSpinLock[SPIN_LOCK_RX_DIRECT_REORDER]);\
} while (0)

#define RX_DIRECT_REORDER_UNLOCK(pad, dbg) \
do { \
	P_GLUE_INFO_T _glue = pad->prGlueInfo; \
	if (!HAL_IS_RX_DIRECT(pad) || !_glue) \
		break; \
	if (dbg) \
		DBGLOG(QM, EVENT, "RX_DIRECT_REORDER_UNLOCK %u\n", __LINE__); \
	spin_unlock_bh(&_glue->rSpinLock[SPIN_LOCK_RX_DIRECT_REORDER]); \
} while (0)

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

/*----------------------------------------------------------------------------*/
/*!
* \brief Init Queue Management for TX
*
* \param[in] (none)
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID qmInit(IN P_ADAPTER_T prAdapter, IN BOOLEAN isTxResrouceControlEn)
{
	UINT_32 u4Idx;
#if QM_ADAPTIVE_TC_RESOURCE_CTRL
	UINT_32 u4TotalMinReservedTcResource = 0;
	UINT_32 u4TotalTcResource = 0;
	UINT_32 u4TotalGurantedTcResource = 0;
#endif

	P_QUE_MGT_T prQM = &prAdapter->rQM;

	/* DbgPrint("QM: Enter qmInit()\n"); */

	/* 4 <2> Initialize other TX queues (queues not in STA_RECs) */
	for (u4Idx = 0; u4Idx < NUM_OF_PER_TYPE_TX_QUEUES; u4Idx++)
		QUEUE_INITIALIZE(&(prQM->arTxQueue[u4Idx]));

	/* 4 <3> Initialize the RX BA table and RX queues */
	/* Initialize the RX Reordering Parameters and Queues */
	for (u4Idx = 0; u4Idx < CFG_NUM_OF_RX_BA_AGREEMENTS; u4Idx++) {
		prQM->arRxBaTable[u4Idx].fgIsValid = FALSE;
		QUEUE_INITIALIZE(&(prQM->arRxBaTable[u4Idx].rReOrderQue));
		prQM->arRxBaTable[u4Idx].u2WinStart = 0xFFFF;
		prQM->arRxBaTable[u4Idx].u2WinEnd = 0xFFFF;

		prQM->arRxBaTable[u4Idx].fgIsWaitingForPktWithSsn = FALSE;
		prQM->arRxBaTable[u4Idx].fgHasBubble = FALSE;
#if CFG_SUPPORT_RX_AMSDU
		/* RX reorder for one MSDU in AMSDU issue */
		prQM->arRxBaTable[u4Idx].u8LastAmsduSubIdx = RX_PAYLOAD_FORMAT_MSDU;
		prQM->arRxBaTable[u4Idx].fgAmsduNeedLastFrame = FALSE;
		prQM->arRxBaTable[u4Idx].fgIsAmsduDuplicated = FALSE;
#endif
		prQM->arRxBaTable[u4Idx].fgFirstSnToWinStart = FALSE;
		cnmTimerInitTimer(prAdapter,
				  &(prQM->arRxBaTable[u4Idx].rReorderBubbleTimer),
				  (PFN_MGMT_TIMEOUT_FUNC) qmHandleReorderBubbleTimeout,
				  (ULONG) (&prQM->arRxBaTable[u4Idx]));

	}
	prQM->ucRxBaCount = 0;

	kalMemSet(&g_arMissTimeout, 0, sizeof(g_arMissTimeout));

	prQM->fgIsTxResrouceControlEn = isTxResrouceControlEn;

#if QM_ADAPTIVE_TC_RESOURCE_CTRL
	/* 4 <4> Initialize TC resource control variables */
	for (u4Idx = 0; u4Idx < TC_NUM; u4Idx++)
		prQM->au4AverageQueLen[u4Idx] = 0;

	ASSERT(prQM->u4TimeToAdjustTcResource && prQM->u4TimeToUpdateQueLen);

	for (u4Idx = 0; u4Idx < TC_NUM; u4Idx++) {
		prQM->au4CurrentTcResource[u4Idx] = prAdapter->rTxCtrl.rTc.au4MaxNumOfBuffer[u4Idx];

		if (u4Idx != TC4_INDEX) {
			u4TotalTcResource += prQM->au4CurrentTcResource[u4Idx];
			u4TotalGurantedTcResource += prQM->au4GuaranteedTcResource[u4Idx];
			u4TotalMinReservedTcResource += prQM->au4MinReservedTcResource[u4Idx];
		}
	}

	/* Sanity Check */
	if (u4TotalMinReservedTcResource > u4TotalTcResource)
		kalMemZero(prQM->au4MinReservedTcResource, sizeof(prQM->au4MinReservedTcResource));

	if (u4TotalGurantedTcResource > u4TotalTcResource)
		kalMemZero(prQM->au4GuaranteedTcResource, sizeof(prQM->au4GuaranteedTcResource));

	u4TotalGurantedTcResource = 0;

	/* Initialize Residual TC resource */
	for (u4Idx = 0; u4Idx < TC_NUM; u4Idx++) {
		if (prQM->au4GuaranteedTcResource[u4Idx] < prQM->au4MinReservedTcResource[u4Idx])
			prQM->au4GuaranteedTcResource[u4Idx] = prQM->au4MinReservedTcResource[u4Idx];

		if (u4Idx != TC4_INDEX)
			u4TotalGurantedTcResource += prQM->au4GuaranteedTcResource[u4Idx];
	}

	prQM->u4ResidualTcResource = u4TotalTcResource - u4TotalGurantedTcResource;

	prQM->fgTcResourcePostAnnealing = FALSE;

#if QM_FAST_TC_RESOURCE_CTRL
	prQM->fgTcResourceFastReaction = FALSE;
#endif

#endif

#if QM_TEST_MODE
	prQM->u4PktCount = 0;

#if QM_TEST_FAIR_FORWARDING

	prQM->u4CurrentStaRecIndexToEnqueue = 0;
	{
		UINT_8 aucMacAddr[MAC_ADDR_LEN];
		P_STA_RECORD_T prStaRec;

		/* Irrelevant in case this STA is an AIS AP (see qmDetermineStaRecIndex()) */
		aucMacAddr[0] = 0x11;
		aucMacAddr[1] = 0x22;
		aucMacAddr[2] = 0xAA;
		aucMacAddr[3] = 0xBB;
		aucMacAddr[4] = 0xCC;
		aucMacAddr[5] = 0xDD;

		prStaRec = &prAdapter->arStaRec[1];
		ASSERT(prStaRec);

		prStaRec->fgIsValid = TRUE;
		prStaRec->fgIsQoS = TRUE;
		prStaRec->fgIsInPS = FALSE;
		prStaRec->ucNetTypeIndex = NETWORK_TYPE_AIS_INDEX;
		COPY_MAC_ADDR((prStaRec)->aucMacAddr, aucMacAddr);

	}

#endif

#endif

#if QM_FORWARDING_FAIRNESS
	for (u4Idx = 0; u4Idx < NUM_OF_PER_STA_TX_QUEUES; u4Idx++) {
		prQM->au4ResourceUsedCount[u4Idx] = 0;
		prQM->au4HeadStaRecIndex[u4Idx] = 0;
	}

	prQM->u4GlobalResourceUsedCount = 0;
#endif

	prQM->u4TxAllowedStaCount = 0;

	prQM->rLastTxPktDumpTime = (OS_SYSTIME) kalGetTimeTick();

}

#if QM_TEST_MODE
VOID qmTestCases(IN P_ADAPTER_T prAdapter)
{
	P_QUE_MGT_T prQM = &prAdapter->rQM;

	DbgPrint("QM: ** TEST MODE **\n");

	if (QM_TEST_STA_REC_DETERMINATION) {
		if (prAdapter->arStaRec[0].fgIsValid) {
			prAdapter->arStaRec[0].fgIsValid = FALSE;
			DbgPrint("QM: (Test) Deactivate STA_REC[0]\n");
		} else {
			prAdapter->arStaRec[0].fgIsValid = TRUE;
			DbgPrint("QM: (Test) Activate STA_REC[0]\n");
		}
	}

	if (QM_TEST_STA_REC_DEACTIVATION) {
		/* Note that QM_STA_REC_HARD_CODING shall be set to 1 for this test */

		if (prAdapter->arStaRec[0].fgIsValid) {

			DbgPrint("QM: (Test) Deactivate STA_REC[0]\n");
			qmDeactivateStaRec(prAdapter, &prAdapter->arStaRec[0]);
		} else {

			UINT_8 aucMacAddr[MAC_ADDR_LEN];

			/* Irrelevant in case this STA is an AIS AP (see qmDetermineStaRecIndex()) */
			aucMacAddr[0] = 0x11;
			aucMacAddr[1] = 0x22;
			aucMacAddr[2] = 0xAA;
			aucMacAddr[3] = 0xBB;
			aucMacAddr[4] = 0xCC;
			aucMacAddr[5] = 0xDD;

			DbgPrint("QM: (Test) Activate STA_REC[0]\n");
			qmActivateStaRec(prAdapter,	/* Adapter pointer */
					 0,	/* STA_REC index from FW */
					 TRUE,	/* fgIsQoS */
					 NETWORK_TYPE_AIS_INDEX,	/* Network type */
					 TRUE,	/* fgIsAp */
					 aucMacAddr	/* MAC address */
			    );
		}
	}

	if (QM_TEST_FAIR_FORWARDING) {
		if (prAdapter->arStaRec[1].fgIsValid) {
			prQM->u4CurrentStaRecIndexToEnqueue++;
			prQM->u4CurrentStaRecIndexToEnqueue %= 2;
			DbgPrint("QM: (Test) Switch to STA_REC[%ld]\n", prQM->u4CurrentStaRecIndexToEnqueue);
		}
	}

}
#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief Update a STA_REC
*
* \param[in] prAdapter  Pointer to the Adapter instance
* \param[in] prStaRec The pointer of the STA_REC
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID qmUpdateStaRec(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec)
{
	P_BSS_INFO_T prBssInfo;
	BOOLEAN fgIsTxAllowed = FALSE;

	if (!prStaRec)
		return;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);

	/* 4 <1> Ensure STA is valid */
	if (prStaRec->fgIsValid) {
		/* 4 <2.1> STA/BSS is protected */
		if (secIsProtectedBss(prAdapter, prBssInfo)) {
			if (prStaRec->fgIsTxKeyReady)
				fgIsTxAllowed = TRUE;
			else	/* whsu test for 1x */
				fgIsTxAllowed = FALSE;
		}
		/* 4 <2.2> OPEN security */
		else
			fgIsTxAllowed = TRUE;
	}
	/* 4 <x> Update StaRec */
	qmSetStaRecTxAllowed(prAdapter, prStaRec, fgIsTxAllowed);

#if CFG_SUPPORT_BFER
		if ((IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucStaVhtBfer) ||
			IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucStaHtBfer)) &&
			prAdapter->fgIsHwSupportBfer && fgIsTxAllowed &&
			(prStaRec->ucStaState == STA_STATE_3)) {
			rlmBfStaRecPfmuUpdate(prAdapter, prStaRec);
			rlmETxBfTriggerPeriodicSounding(prAdapter);
		}
#endif
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Activate a STA_REC
*
* \param[in] prAdapter  Pointer to the Adapter instance
* \param[in] prStaRec The pointer of the STA_REC
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID qmActivateStaRec(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec)
{
	/* 4 <1> Deactivate first */
	if (!prStaRec)
		return;

	if (prStaRec->fgIsValid) {	/* The STA_REC has been activated */
		DBGLOG(QM, WARN, "QM: (WARNING) Activating a STA_REC which has been activated\n");
		DBGLOG(QM, WARN, "QM: (WARNING) Deactivating a STA_REC before re-activating\n");
		qmDeactivateStaRec(prAdapter, prStaRec);	/* To flush TX/RX queues and del RX BA agreements */
	}
	/* 4 <2> Activate the STA_REC */
	/* Reset buffer count  */
	prStaRec->ucFreeQuota = 0;
	prStaRec->ucFreeQuotaForDelivery = 0;
	prStaRec->ucFreeQuotaForNonDelivery = 0;

	/* Init the STA_REC */
	prStaRec->fgIsValid = TRUE;
	prStaRec->fgIsInPS = FALSE;

	/* Default setting of TX/RX AMPDU */
	prStaRec->fgTxAmpduEn = IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucAmpduTx);
	prStaRec->fgRxAmpduEn = IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucAmpduRx);

	nicTxGenerateDescTemplate(prAdapter, prStaRec);

	qmUpdateStaRec(prAdapter, prStaRec);

	/* Done in qmInit() or qmDeactivateStaRec() */
#if 0
	/* At the beginning, no RX BA agreements have been established */
	for (i = 0; i < CFG_RX_MAX_BA_TID_NUM; i++)
		(prStaRec->aprRxReorderParamRefTbl)[i] = NULL;
#endif

	DBGLOG(QM, INFO, "QM: +STA[%d]\n", prStaRec->ucIndex);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Deactivate a STA_REC
*
* \param[in] prAdapter  Pointer to the Adapter instance
* \param[in] u4StaRecIdx The index of the STA_REC
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID qmDeactivateStaRec(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec)
{
	UINT_32 i;

	if (!prStaRec)
		return;
	/* 4 <1> Flush TX queues */
	if (HAL_IS_TX_DIRECT(prAdapter)) {
		nicTxDirectClearStaPsQ(prAdapter, prStaRec->ucIndex);
	} else {
		P_MSDU_INFO_T prFlushedTxPacketList = NULL;

		prFlushedTxPacketList = qmFlushStaTxQueues(prAdapter, prStaRec->ucIndex);

		if (prFlushedTxPacketList)
			wlanProcessQueuedMsduInfo(prAdapter, prFlushedTxPacketList);
	}

	/* 4 <2> Flush RX queues and delete RX BA agreements */
	for (i = 0; i < CFG_RX_MAX_BA_TID_NUM; i++) {
		/* Delete the RX BA entry with TID = i */
		qmDelRxBaEntry(prAdapter, prStaRec->ucIndex, (UINT_8) i, FALSE);
	}

	/* 4 <3> Deactivate the STA_REC */
	prStaRec->fgIsValid = FALSE;
	prStaRec->fgIsInPS = FALSE;
	prStaRec->fgIsTxKeyReady = FALSE;

	/* Reset buffer count  */
	prStaRec->ucFreeQuota = 0;
	prStaRec->ucFreeQuotaForDelivery = 0;
	prStaRec->ucFreeQuotaForNonDelivery = 0;

	nicTxFreeDescTemplate(prAdapter, prStaRec);

	qmUpdateStaRec(prAdapter, prStaRec);

	DBGLOG(QM, INFO, "QM: -STA[%u]\n", prStaRec->ucIndex);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Deactivate a STA_REC
*
* \param[in] prAdapter  Pointer to the Adapter instance
* \param[in] ucBssIndex The index of the BSS
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID qmFreeAllByBssIdx(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex)
{

	P_QUE_MGT_T prQM;
	P_QUE_T prQue;
	QUE_T rNeedToFreeQue;
	QUE_T rTempQue;
	P_QUE_T prNeedToFreeQue;
	P_QUE_T prTempQue;
	P_MSDU_INFO_T prMsduInfo;

	prQM = &prAdapter->rQM;
	prQue = &prQM->arTxQueue[TX_QUEUE_INDEX_BMCAST];

	QUEUE_INITIALIZE(&rNeedToFreeQue);
	QUEUE_INITIALIZE(&rTempQue);

	prNeedToFreeQue = &rNeedToFreeQue;
	prTempQue = &rTempQue;

	QUEUE_MOVE_ALL(prTempQue, prQue);

	QUEUE_REMOVE_HEAD(prTempQue, prMsduInfo, P_MSDU_INFO_T);
	while (prMsduInfo) {

		if (prMsduInfo->ucBssIndex == ucBssIndex) {
			/* QUEUE_INSERT_TAIL */
			QUEUE_INSERT_TAIL(prNeedToFreeQue, (P_QUE_ENTRY_T) prMsduInfo);
		} else {
			/* QUEUE_INSERT_TAIL */
			QUEUE_INSERT_TAIL(prQue, (P_QUE_ENTRY_T) prMsduInfo);
		}

		QUEUE_REMOVE_HEAD(prTempQue, prMsduInfo, P_MSDU_INFO_T);
	}
	if (QUEUE_IS_NOT_EMPTY(prNeedToFreeQue))
		wlanProcessQueuedMsduInfo(prAdapter, (P_MSDU_INFO_T) QUEUE_GET_HEAD(prNeedToFreeQue));

}

/*----------------------------------------------------------------------------*/
/*!
* \brief Flush all TX queues
*
* \param[in] (none)
*
* \return The flushed packets (in a list of MSDU_INFOs)
*/
/*----------------------------------------------------------------------------*/
P_MSDU_INFO_T qmFlushTxQueues(IN P_ADAPTER_T prAdapter)
{
	UINT_8 ucStaArrayIdx;
	UINT_8 ucQueArrayIdx;

	P_QUE_MGT_T prQM = &prAdapter->rQM;

	QUE_T rTempQue;
	P_QUE_T prTempQue = &rTempQue;
	P_QUE_T prQue;

	DBGLOG(QM, TRACE, "QM: Enter qmFlushTxQueues()\n");

	QUEUE_INITIALIZE(prTempQue);

	/* Concatenate all MSDU_INFOs in per-STA queues */
	for (ucStaArrayIdx = 0; ucStaArrayIdx < CFG_STA_REC_NUM; ucStaArrayIdx++) {
		for (ucQueArrayIdx = 0; ucQueArrayIdx < NUM_OF_PER_STA_TX_QUEUES; ucQueArrayIdx++) {
			prQue = &(prAdapter->arStaRec[ucStaArrayIdx].arPendingTxQueue[ucQueArrayIdx]);
			QUEUE_CONCATENATE_QUEUES(prTempQue, prQue);
			prQue = &(prAdapter->arStaRec[ucStaArrayIdx].arTxQueue[ucQueArrayIdx]);
			QUEUE_CONCATENATE_QUEUES(prTempQue, prQue);
		}
	}

	/* Flush per-Type queues */
	for (ucQueArrayIdx = 0; ucQueArrayIdx < NUM_OF_PER_TYPE_TX_QUEUES; ucQueArrayIdx++) {
		prQue = &(prQM->arTxQueue[ucQueArrayIdx]);
		QUEUE_CONCATENATE_QUEUES(prTempQue, prQue);
	}

	return (P_MSDU_INFO_T)QUEUE_GET_HEAD(prTempQue);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Flush TX packets for a particular STA
*
* \param[in] u4StaRecIdx STA_REC index
*
* \return The flushed packets (in a list of MSDU_INFOs)
*/
/*----------------------------------------------------------------------------*/
P_MSDU_INFO_T qmFlushStaTxQueues(IN P_ADAPTER_T prAdapter, IN UINT_32 u4StaRecIdx)
{
	UINT_8 ucQueArrayIdx;
	P_STA_RECORD_T prStaRec;
	P_QUE_T prQue;
	QUE_T rTempQue;
	P_QUE_T prTempQue = &rTempQue;

	DBGLOG(QM, TRACE, "QM: Enter qmFlushStaTxQueues(%u)\n", u4StaRecIdx);

	ASSERT(u4StaRecIdx < CFG_STA_REC_NUM);

	prStaRec = &prAdapter->arStaRec[u4StaRecIdx];
	ASSERT(prStaRec);

	QUEUE_INITIALIZE(prTempQue);

	/* Concatenate all MSDU_INFOs in TX queues of this STA_REC */
	for (ucQueArrayIdx = 0; ucQueArrayIdx < NUM_OF_PER_STA_TX_QUEUES; ucQueArrayIdx++) {
		prQue = &(prStaRec->arPendingTxQueue[ucQueArrayIdx]);
		QUEUE_CONCATENATE_QUEUES(prTempQue, prQue);
		prQue = &(prStaRec->arTxQueue[ucQueArrayIdx]);
		QUEUE_CONCATENATE_QUEUES(prTempQue, prQue);
	}

	return (P_MSDU_INFO_T)QUEUE_GET_HEAD(prTempQue);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Flush RX packets
*
* \param[in] (none)
*
* \return The flushed packets (in a list of SW_RFBs)
*/
/*----------------------------------------------------------------------------*/
P_SW_RFB_T qmFlushRxQueues(IN P_ADAPTER_T prAdapter)
{
	UINT_32 i;
	P_SW_RFB_T prSwRfbListHead;
	P_SW_RFB_T prSwRfbListTail;
	P_QUE_MGT_T prQM = &prAdapter->rQM;

	prSwRfbListHead = prSwRfbListTail = NULL;

	DBGLOG(QM, TRACE, "QM: Enter qmFlushRxQueues()\n");

	RX_DIRECT_REORDER_LOCK(prAdapter, 0);
	for (i = 0; i < CFG_NUM_OF_RX_BA_AGREEMENTS; i++) {
		if (QUEUE_IS_NOT_EMPTY(&(prQM->arRxBaTable[i].rReOrderQue))) {
			if (!prSwRfbListHead) {

				/* The first MSDU_INFO is found */
				prSwRfbListHead = (P_SW_RFB_T)
				    QUEUE_GET_HEAD(&(prQM->arRxBaTable[i].rReOrderQue));
				prSwRfbListTail = (P_SW_RFB_T)
				    QUEUE_GET_TAIL(&(prQM->arRxBaTable[i].rReOrderQue));
			} else {
				/* Concatenate the MSDU_INFO list with the existing list */
				QM_TX_SET_NEXT_MSDU_INFO(prSwRfbListTail,
							 QUEUE_GET_HEAD(&(prQM->arRxBaTable[i].rReOrderQue)));

				prSwRfbListTail = (P_SW_RFB_T)
				    QUEUE_GET_TAIL(&(prQM->arRxBaTable[i].rReOrderQue));
			}

			QUEUE_INITIALIZE(&(prQM->arRxBaTable[i].rReOrderQue));
			if (QM_RX_GET_NEXT_SW_RFB(prSwRfbListTail)) {
				DBGLOG(QM, ERROR,
					"QM: non-null tail->next at arRxBaTable[%u]\n",
					i);
			}
		} else {
			continue;
		}
	}
	RX_DIRECT_REORDER_UNLOCK(prAdapter, 0);

	if (prSwRfbListTail) {
		/* Terminate the MSDU_INFO list with a NULL pointer */
		QM_TX_SET_NEXT_SW_RFB(prSwRfbListTail, NULL);
	}
	return prSwRfbListHead;

}

/*----------------------------------------------------------------------------*/
/*!
* \brief Flush RX packets with respect to a particular STA
*
* \param[in] u4StaRecIdx STA_REC index
* \param[in] u4Tid TID
*
* \return The flushed packets (in a list of SW_RFBs)
*/
/*----------------------------------------------------------------------------*/
P_SW_RFB_T qmFlushStaRxQueue(IN P_ADAPTER_T prAdapter, IN UINT_32 u4StaRecIdx, IN UINT_32 u4Tid)
{
	/* UINT_32 i; */
	P_SW_RFB_T prSwRfbListHead;
	P_SW_RFB_T prSwRfbListTail;
	P_RX_BA_ENTRY_T prReorderQueParm;
	P_STA_RECORD_T prStaRec;

	DBGLOG(QM, TRACE, "QM: Enter qmFlushStaRxQueues(%u)\n", u4StaRecIdx);

	prSwRfbListHead = prSwRfbListTail = NULL;

	prStaRec = &prAdapter->arStaRec[u4StaRecIdx];
	ASSERT(prStaRec);

	/* No matter whether this is an activated STA_REC, do flush */
#if 0
	if (!prStaRec->fgIsValid)
		return NULL;
#endif

	/* Obtain the RX BA Entry pointer */
	prReorderQueParm = ((prStaRec->aprRxReorderParamRefTbl)[u4Tid]);

	/* Note: For each queued packet, prCurrSwRfb->eDst equals RX_PKT_DESTINATION_HOST */
	if (prReorderQueParm) {
		RX_DIRECT_REORDER_LOCK(prAdapter, 0);
		if (QUEUE_IS_NOT_EMPTY(&(prReorderQueParm->rReOrderQue))) {

			prSwRfbListHead = (P_SW_RFB_T)
			    QUEUE_GET_HEAD(&(prReorderQueParm->rReOrderQue));
			prSwRfbListTail = (P_SW_RFB_T)
			    QUEUE_GET_TAIL(&(prReorderQueParm->rReOrderQue));

			QUEUE_INITIALIZE(&(prReorderQueParm->rReOrderQue));
		}
		RX_DIRECT_REORDER_UNLOCK(prAdapter, 0);
	}

	if (prSwRfbListTail) {
		if (QM_RX_GET_NEXT_SW_RFB(prSwRfbListTail)) {
			DBGLOG(QM, ERROR,
				"QM: non-empty tail->next at STA %u TID %u\n",
				u4StaRecIdx, u4Tid);
		}

		/* Terminate the MSDU_INFO list with a NULL pointer */
		QM_TX_SET_NEXT_SW_RFB(prSwRfbListTail, NULL);
	}
	return prSwRfbListHead;

}

P_QUE_T qmDetermineStaTxQueue(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo, OUT PUINT_8 pucTC)
{
	P_QUE_T prTxQue = NULL;
	P_STA_RECORD_T prStaRec;
	ENUM_WMM_ACI_T eAci = WMM_AC_BE_INDEX;
	BOOLEAN fgCheckACMAgain;
	UINT_8 ucTC, ucQueIdx = WMM_AC_BE_INDEX;
	P_BSS_INFO_T prBssInfo;
	UINT_8 aucNextUP[WMM_AC_INDEX_NUM] = { 1 /* BEtoBK */,
		1 /* na */,
		0 /* VItoBE */,
		4		/* VOtoVI */
	};

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prMsduInfo->ucBssIndex);
	prStaRec = QM_GET_STA_REC_PTR_FROM_INDEX(prAdapter, prMsduInfo->ucStaRecIndex);

	if (!prStaRec) {
		DBGLOG(QM, ERROR, "prStaRec is null.\n");
		return NULL;
	}
	if (prMsduInfo->ucUserPriority < 8) {
		QM_DBG_CNT_INC(&prAdapter->rQM, prMsduInfo->ucUserPriority + 15);
		/* QM_DBG_CNT_15 *//* QM_DBG_CNT_16 *//* QM_DBG_CNT_17 *//* QM_DBG_CNT_18 */
		/* QM_DBG_CNT_19 *//* QM_DBG_CNT_20 *//* QM_DBG_CNT_21 *//* QM_DBG_CNT_22 */
	}

	eAci = WMM_AC_BE_INDEX;
	do {
		fgCheckACMAgain = FALSE;
		if (prStaRec->fgIsQoS) {
			if (prMsduInfo->ucUserPriority < TX_DESC_TID_NUM) {
				eAci = aucTid2ACI[prMsduInfo->ucUserPriority];
				ucQueIdx = aucACI2TxQIdx[eAci];
				ucTC = arNetwork2TcResource[prMsduInfo->ucBssIndex][eAci];
			} else {
				ucQueIdx = TX_QUEUE_INDEX_AC1;
				ucTC = TC1_INDEX;
				eAci = WMM_AC_BE_INDEX;
				DBGLOG(QM, WARN, "Packet TID is not in [0~7]\n");
				ASSERT(0);
			}
			if ((prBssInfo->arACQueParms[eAci].ucIsACMSet) && (eAci != WMM_AC_BK_INDEX)) {
				prMsduInfo->ucUserPriority = aucNextUP[eAci];
				fgCheckACMAgain = TRUE;
			}
		} else {
			ucQueIdx = TX_QUEUE_INDEX_NON_QOS;
			ucTC = arNetwork2TcResource[prMsduInfo->ucBssIndex][NET_TC_WMM_AC_BE_INDEX];
		}

		if (prAdapter->rWifiVar.ucTcRestrict < TC_NUM) {
			ucTC = prAdapter->rWifiVar.ucTcRestrict;
			ucQueIdx = ucTC;
		}

	} while (fgCheckACMAgain);

	if (ucQueIdx >= NUM_OF_PER_STA_TX_QUEUES) {
		DBGLOG(QM, ERROR,
		       "ucQueIdx = %u, needs 0~3 to avoid out-of-bounds.\n", ucQueIdx);
		return NULL;
	}
	if (prStaRec->fgIsTxAllowed) {
		/* non protected BSS or protected BSS with key set */
		prTxQue = prStaRec->aprTargetQueue[ucQueIdx];
	} else if (secIsProtectedBss(prAdapter, prBssInfo) && prMsduInfo->fgIs802_1x
		&& prMsduInfo->fgIs802_1x_NonProtected) {
		/* protected BSS without key set */
		/* Tx pairwise EAPOL 1x packet (non-protected frame) */
		prTxQue = &prStaRec->arTxQueue[ucQueIdx];
	} else {
		/* protected BSS without key set */
		/* Enqueue protected frame into pending queue */
		prTxQue = prStaRec->aprTargetQueue[ucQueIdx];
	}

	*pucTC = ucTC;

	return prTxQue;
}

VOID qmSetTxPacketDescTemplate(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo)
{
	P_STA_RECORD_T prStaRec = QM_GET_STA_REC_PTR_FROM_INDEX(prAdapter, prMsduInfo->ucStaRecIndex);

	/* Check the Tx descriptor template is valid */
	if (prStaRec && prStaRec->aprTxDescTemplate[prMsduInfo->ucUserPriority]) {
		prMsduInfo->fgIsTXDTemplateValid = TRUE;
	} else {
		if (prStaRec) {
			DBGLOG(QM, TRACE,
			       "Cannot get TXD template for STA[%u] QoS[%u] MSDU UP[%u]\n",
			       prStaRec->ucIndex, prStaRec->fgIsQoS, prMsduInfo->ucUserPriority);
		}
		prMsduInfo->fgIsTXDTemplateValid = FALSE;
	}
}

/*----------------------------------------------------------------------------*/
/*!
* \brief : To StaRec, function to stop TX
*
* \param[in] :
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID qmSetStaRecTxAllowed(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec, IN BOOLEAN fgIsTxAllowed)
{
	UINT_8 ucIdx;
	P_QUE_T prSrcQ, prDstQ;

	/* Update Tx queue */
	for (ucIdx = 0; ucIdx < NUM_OF_PER_STA_TX_QUEUES; ucIdx++) {
		if (fgIsTxAllowed) {
			prSrcQ = &prStaRec->arPendingTxQueue[ucIdx];
			prDstQ = &prStaRec->arTxQueue[ucIdx];
		} else {
			prSrcQ = &prStaRec->arTxQueue[ucIdx];
			prDstQ = &prStaRec->arPendingTxQueue[ucIdx];
		}

		QUEUE_CONCATENATE_QUEUES_HEAD(prDstQ, prSrcQ);
		prStaRec->aprTargetQueue[ucIdx] = prDstQ;
	}

	if (prStaRec->fgIsTxAllowed != fgIsTxAllowed) {
		if (fgIsTxAllowed)
			prAdapter->rQM.u4TxAllowedStaCount++;
		else
			prAdapter->rQM.u4TxAllowedStaCount--;
	}
	prStaRec->fgIsTxAllowed = fgIsTxAllowed;

	DBGLOG(QM, INFO, "Set Sta[%u] TxAllowed[%u] %s TxQ\n", prStaRec->ucIndex, fgIsTxAllowed,
		fgIsTxAllowed ? "normal":"pending");
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Enqueue TX packets
*
* \param[in] prMsduInfoListHead Pointer to the list of TX packets
*
* \return The freed packets, which are not enqueued
*/
/*----------------------------------------------------------------------------*/
P_MSDU_INFO_T qmEnqueueTxPackets(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfoListHead)
{
	P_MSDU_INFO_T prMsduInfoReleaseList;
	P_MSDU_INFO_T prCurrentMsduInfo;
	P_MSDU_INFO_T prNextMsduInfo;

	P_QUE_T prTxQue;
	QUE_T rNotEnqueuedQue;

	UINT_8 ucTC;
	P_QUE_MGT_T prQM = &prAdapter->rQM;
	P_BSS_INFO_T prBssInfo;
	BOOLEAN fgDropPacket;

	DBGLOG(QM, LOUD, "Enter qmEnqueueTxPackets\n");

	ASSERT(prMsduInfoListHead);

	prMsduInfoReleaseList = NULL;
	prCurrentMsduInfo = NULL;
	QUEUE_INITIALIZE(&rNotEnqueuedQue);
	prNextMsduInfo = prMsduInfoListHead;

	do {
		prCurrentMsduInfo = prNextMsduInfo;
		prNextMsduInfo = QM_TX_GET_NEXT_MSDU_INFO(prCurrentMsduInfo);
		ucTC = TC1_INDEX;

		/* 4 <0> Sanity check of BSS_INFO */
		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prCurrentMsduInfo->ucBssIndex);

		if (!prBssInfo) {
			/* No BSS_INFO */
			fgDropPacket = TRUE;
		} else if (IS_BSS_ACTIVE(prBssInfo)) {
			/* BSS active */
			fgDropPacket = FALSE;
		} else {
			/* BSS inactive */
			fgDropPacket = TRUE;
		}

		if (!fgDropPacket) {
			/* 4 <1> Lookup the STA_REC index */
			/* The ucStaRecIndex will be set in this function */
			qmDetermineStaRecIndex(prAdapter, prCurrentMsduInfo);

			wlanUpdateTxStatistics(prAdapter, prCurrentMsduInfo, FALSE);	/*get per-AC Tx packets */

			DBGLOG(QM, LOUD, "Enqueue MSDU by StaRec[%u]!\n", prCurrentMsduInfo->ucStaRecIndex);

			switch (prCurrentMsduInfo->ucStaRecIndex) {
			case STA_REC_INDEX_BMCAST:
				prTxQue = &prQM->arTxQueue[TX_QUEUE_INDEX_BMCAST];
				ucTC = arNetwork2TcResource[prCurrentMsduInfo->ucBssIndex][NET_TC_BMC_INDEX];

				/* Always set BMC packet retry limit to unlimited */
				if (!(prCurrentMsduInfo->u4Option & MSDU_OPT_MANUAL_RETRY_LIMIT))
					nicTxSetPktRetryLimit(prCurrentMsduInfo, TX_DESC_TX_COUNT_NO_LIMIT);

				QM_DBG_CNT_INC(prQM, QM_DBG_CNT_23);
				break;

			case STA_REC_INDEX_NOT_FOUND:
				/* Drop packet if no STA_REC is found */
				DBGLOG(QM, TRACE, "Drop the Packet for no STA_REC\n");

				prTxQue = &rNotEnqueuedQue;

				TX_INC_CNT(&prAdapter->rTxCtrl, TX_INACTIVE_STA_DROP);
				QM_DBG_CNT_INC(prQM, QM_DBG_CNT_24);
				break;

			default:
				prTxQue = qmDetermineStaTxQueue(prAdapter, prCurrentMsduInfo, &ucTC);
				break;	/*default */
			}	/* switch (prCurrentMsduInfo->ucStaRecIndex) */

			if (prCurrentMsduInfo->eSrc == TX_PACKET_FORWARDING) {
				DBGLOG(QM, TRACE, "Forward Pkt to STA[%u] BSS[%u]\n",
				       prCurrentMsduInfo->ucStaRecIndex, prCurrentMsduInfo->ucBssIndex);

				if (prTxQue->u4NumElem >= prQM->u4MaxForwardBufferCount) {
					DBGLOG(QM, INFO,
					       "Drop the Packet for full Tx queue (forwarding) Bss %u\n",
					       prCurrentMsduInfo->ucBssIndex);
					prTxQue = &rNotEnqueuedQue;
					TX_INC_CNT(&prAdapter->rTxCtrl, TX_FORWARD_OVERFLOW_DROP);
				}
			}

		} else {
			DBGLOG(QM, TRACE, "Drop the Packet for inactive Bss %u\n", prCurrentMsduInfo->ucBssIndex);
			QM_DBG_CNT_INC(prQM, QM_DBG_CNT_31);
			prTxQue = &rNotEnqueuedQue;
			TX_INC_CNT(&prAdapter->rTxCtrl, TX_INACTIVE_BSS_DROP);
		}

		/* 4 <3> Fill the MSDU_INFO for constructing HIF TX header */
		/* Note that the BSS Index and STA_REC index are determined in
		 *  qmDetermineStaRecIndex(prCurrentMsduInfo).
		 */
		prCurrentMsduInfo->ucTC = ucTC;

		/* Check the Tx descriptor template is valid */
		qmSetTxPacketDescTemplate(prAdapter, prCurrentMsduInfo);

		/* Set Tx rate */
		switch (prAdapter->rWifiVar.ucDataTxRateMode) {
		case DATA_RATE_MODE_BSS_LOWEST:
			nicTxSetPktLowestFixedRate(prAdapter, prCurrentMsduInfo);
			break;

		case DATA_RATE_MODE_MANUAL:
			prCurrentMsduInfo->u4FixedRateOption = prAdapter->rWifiVar.u4DataTxRateCode;

			prCurrentMsduInfo->ucRateMode = MSDU_RATE_MODE_MANUAL_DESC;
			break;

		case DATA_RATE_MODE_AUTO:
		default:
			if (prCurrentMsduInfo->ucRateMode == MSDU_RATE_MODE_LOWEST_RATE)
				nicTxSetPktLowestFixedRate(prAdapter, prCurrentMsduInfo);
			break;
		}

		/* BMC pkt need limited rate according to coex report*/
		if (prCurrentMsduInfo->ucStaRecIndex == STA_REC_INDEX_BMCAST)
			nicTxSetPktLowestFixedRate(prAdapter,
						prCurrentMsduInfo);

		/* 4 <4> Enqueue the packet */
		QUEUE_INSERT_TAIL(prTxQue, (P_QUE_ENTRY_T) prCurrentMsduInfo);

#if QM_FAST_TC_RESOURCE_CTRL && QM_ADAPTIVE_TC_RESOURCE_CTRL
		if (prTxQue != &rNotEnqueuedQue) {
			/* Check and trigger fast TC resource adjustment for queued packets */
			qmCheckForFastTcResourceCtrl(prAdapter, ucTC);
		}
#endif

#if QM_TEST_MODE
		if (++prQM->u4PktCount == QM_TEST_TRIGGER_TX_COUNT) {
			prQM->u4PktCount = 0;
			qmTestCases(prAdapter);
		}
#endif

		DBGLOG(QM, LOUD, "Current queue length = %u\n", prTxQue->u4NumElem);
	} while (prNextMsduInfo);

	if (QUEUE_IS_NOT_EMPTY(&rNotEnqueuedQue)) {
		QM_TX_SET_NEXT_MSDU_INFO((P_MSDU_INFO_T) QUEUE_GET_TAIL(&rNotEnqueuedQue), NULL);
		prMsduInfoReleaseList = (P_MSDU_INFO_T) QUEUE_GET_HEAD(&rNotEnqueuedQue);
	}
#if QM_ADAPTIVE_TC_RESOURCE_CTRL
	/* 4 <x> Update TC resource control related variables */
	/* Keep track of the queue length */
	qmDoAdaptiveTcResourceCtrl(prAdapter);
#endif

	return prMsduInfoReleaseList;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Determine the STA_REC index for a packet
*
* \param[in] prMsduInfo Pointer to the packet
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID qmDetermineStaRecIndex(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo)
{
	UINT_32 i;

	P_STA_RECORD_T prTempStaRec;
	P_BSS_INFO_T prBssInfo;

	GLUE_SPIN_LOCK_DECLARATION();

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prMsduInfo->ucBssIndex);
	prTempStaRec = NULL;

	ASSERT(prMsduInfo);

	DBGLOG(QM, LOUD, "Msdu BSS Idx[%u] OpMode[%u] StaRecOfApExist[%u]\n",
	       prMsduInfo->ucBssIndex, prBssInfo->eCurrentOPMode, prBssInfo->prStaRecOfAP ? TRUE : FALSE);

	switch (prBssInfo->eCurrentOPMode) {
	case OP_MODE_IBSS:
	case OP_MODE_ACCESS_POINT:
		/* 4 <1> DA = BMCAST */
		if (IS_BMCAST_MAC_ADDR(prMsduInfo->aucEthDestAddr)) {
			prMsduInfo->ucStaRecIndex = STA_REC_INDEX_BMCAST;
			DBGLOG(QM, LOUD, "TX with DA = BMCAST\n");
			return;
		}
		break;

		/* Infra Client/GC */
	case OP_MODE_INFRASTRUCTURE:
	case OP_MODE_BOW:
		if (prBssInfo->prStaRecOfAP) {
#if CFG_SUPPORT_TDLS

			prTempStaRec =
			    cnmGetTdlsPeerByAddress(prAdapter, prBssInfo->ucBssIndex, prMsduInfo->aucEthDestAddr);
			if (IS_DLS_STA(prTempStaRec) && prTempStaRec->ucStaState == STA_STATE_3) {
				if (g_arTdlsLink[prTempStaRec->ucTdlsIndex]) {
					prMsduInfo->ucStaRecIndex = prTempStaRec->ucIndex;
					return;
				}
			}
#endif
			/* 4 <2> Check if an AP STA is present */
			GLUE_ACQUIRE_SPIN_LOCK(prAdapter->prGlueInfo,
				SPIN_LOCK_STA_RECOFAP);
			prTempStaRec = prBssInfo->prStaRecOfAP;

			DBGLOG(QM, LOUD,
			       "StaOfAp Idx[%u] WIDX[%u] Valid[%u] TxAllowed[%u] InUse[%u] Type[%u]\n",
			       prTempStaRec->ucIndex, prTempStaRec->ucWlanIndex,
			       prTempStaRec->fgIsValid, prTempStaRec->fgIsTxAllowed,
			       prTempStaRec->fgIsInUse, prTempStaRec->eStaType);

			if (prTempStaRec && prTempStaRec->fgIsInUse) {
				prMsduInfo->ucStaRecIndex = prTempStaRec->ucIndex;
				GLUE_RELEASE_SPIN_LOCK(prAdapter->prGlueInfo,
					SPIN_LOCK_STA_RECOFAP);
				DBGLOG(QM, LOUD, "TX with AP_STA[%u]\n", prTempStaRec->ucIndex);
				return;
			}
			GLUE_RELEASE_SPIN_LOCK(prAdapter->prGlueInfo,
				SPIN_LOCK_STA_RECOFAP);
		}
		break;

	case OP_MODE_P2P_DEVICE:
		break;

	default:
		break;
	}

	/* 4 <3> Not BMCAST, No AP --> Compare DA (i.e., to see whether this is a unicast frame to a client) */
	for (i = 0; i < CFG_STA_REC_NUM; i++) {
		prTempStaRec = &(prAdapter->arStaRec[i]);
		if (prTempStaRec->fgIsInUse) {
			if (EQUAL_MAC_ADDR(prTempStaRec->aucMacAddr, prMsduInfo->aucEthDestAddr)) {
				prMsduInfo->ucStaRecIndex = prTempStaRec->ucIndex;
				DBGLOG(QM, LOUD, "TX with STA[%u]\n", prTempStaRec->ucIndex);
				return;
			}
		}
	}

	/* 4 <4> No STA found, Not BMCAST --> Indicate NOT_FOUND to FW */
	prMsduInfo->ucStaRecIndex = STA_REC_INDEX_NOT_FOUND;
	DBGLOG(QM, LOUD, "QM: TX with STA_REC_INDEX_NOT_FOUND\n");

#if (QM_TEST_MODE && QM_TEST_FAIR_FORWARDING)
	prMsduInfo->ucStaRecIndex = (UINT_8) prQM->u4CurrentStaRecIndexToEnqueue;
#endif
}

P_STA_RECORD_T qmDetermineStaToBeDequeued(IN P_ADAPTER_T prAdapter, IN UINT_32 u4StartStaRecIndex)
{

	return NULL;
}

P_QUE_T qmDequeueStaTxPackets(IN P_ADAPTER_T prAdapter)
{

	return NULL;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Dequeue TX packets from a STA_REC for a particular TC
*
* \param[out] prQue The queue to put the dequeued packets
* \param[in] ucTC The TC index (TC0_INDEX to TC5_INDEX)
* \param[in] ucMaxNum The maximum amount of dequeued packets
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
UINT_32
qmDequeueTxPacketsFromPerStaQueues(IN P_ADAPTER_T prAdapter, OUT P_QUE_T prQue,
	IN UINT_8 ucTC, IN UINT_32 u4CurrentQuota, IN UINT_32 u4TotalQuota)
{
	UINT_32 ucLoop;		/* Loop for */

	UINT_32 u4CurStaIndex = 0;
	UINT_32 u4CurStaUsedResource = 0;

	P_STA_RECORD_T prStaRec;	/* The current focused STA */
	P_BSS_INFO_T prBssInfo;	/* The Bss for current focused STA */
	P_QUE_T prCurrQueue;	/* The current TX queue to dequeue */
	P_MSDU_INFO_T prDequeuedPkt;	/* The dequeued packet */

	UINT_32 u4CurStaForwardFrameCount;	/* To remember the total forwarded packets for a STA */
	UINT_32 u4MaxForwardFrameCountLimit;	/* The maximum number of packets a STA can forward */
	UINT_32 u4AvaliableResource;	/* The TX resource amount */
	UINT_32 u4MaxResourceLimit;

	BOOLEAN fgEndThisRound;
	P_QUE_MGT_T prQM = &prAdapter->rQM;

	PUINT_8 pucPsStaFreeQuota;

	/* Sanity Check */
	if (!u4CurrentQuota) {
		DBGLOG(TX, LOUD, "(Fairness) Skip TC = %u u4CurrentQuota = %u\n", ucTC, u4CurrentQuota);
		return u4CurrentQuota;
	}
	/* 4 <1> Assign init value */
	u4AvaliableResource = u4CurrentQuota;
	u4MaxResourceLimit = u4TotalQuota;

#if QM_FORWARDING_FAIRNESS
	u4CurStaIndex = prQM->au4HeadStaRecIndex[ucTC];
	u4CurStaUsedResource = prQM->au4ResourceUsedCount[ucTC];
#endif

	fgEndThisRound = FALSE;
	ucLoop = 0;
	u4CurStaForwardFrameCount = 0;

	DBGLOG(QM, LOUD, "(Fairness) TC[%u] Init Head STA[%u] Resource[%u]\n",
	       ucTC, u4CurStaIndex, u4AvaliableResource);

	/* 4 <2> Traverse STA array from Head STA */
	/* From STA[x] to STA[x+1] to STA[x+2] to ... to STA[x] */
	while (ucLoop < CFG_STA_REC_NUM) {
		prStaRec = &prAdapter->arStaRec[u4CurStaIndex];
		prCurrQueue = &prStaRec->arTxQueue[ucTC];

		/* 4 <2.1> Find a Tx allowed STA */
		/* Only Data frame will be queued in */
		/* if (prStaRec->fgIsTxAllowed) { */
		if (QUEUE_IS_NOT_EMPTY(prCurrQueue)) {
			prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);

			/* prCurrQueue = &prStaRec->aprTxQueue[ucTC]; */
			prDequeuedPkt = NULL;
			pucPsStaFreeQuota = NULL;
			/* Set default forward count limit to unlimited */
			u4MaxForwardFrameCountLimit = QM_STA_FORWARD_COUNT_UNLIMITED;

			/* 4 <2.2> Update forward frame/page count limit for this STA */
			/* AP mode: STA in PS buffer handling */
			if (prStaRec->fgIsInPS) {
				if (prStaRec->fgIsQoS && prStaRec->fgIsUapsdSupported &&
					(prStaRec->ucBmpTriggerAC & BIT(ucTC))) {
					u4MaxForwardFrameCountLimit = prStaRec->ucFreeQuotaForDelivery;
					pucPsStaFreeQuota = &prStaRec->ucFreeQuotaForDelivery;
				} else {
					/* ASSERT(prStaRec->ucFreeQuotaForDelivery == 0); */
					u4MaxForwardFrameCountLimit = prStaRec->ucFreeQuotaForNonDelivery;
					pucPsStaFreeQuota = &prStaRec->ucFreeQuotaForNonDelivery;
				}
			}

			/* fgIsInPS */
			/* Absent BSS handling */
			if (prBssInfo->fgIsNetAbsent) {
				if (u4MaxForwardFrameCountLimit > prBssInfo->ucBssFreeQuota)
					u4MaxForwardFrameCountLimit = prBssInfo->ucBssFreeQuota;
			}

#if CFG_SUPPORT_DBDC
			if (prAdapter->rWifiVar.fgDbDcModeEn)
				u4MaxResourceLimit = gmGetDequeueQuota(prAdapter, prStaRec, prBssInfo, u4TotalQuota);
#endif

			/* 4 <2.3> Dequeue packet */
			/* Three cases to break: (1) No resource (2) No packets (3) Fairness */
			while (!QUEUE_IS_EMPTY(prCurrQueue)) {
				prDequeuedPkt = (P_MSDU_INFO_T) QUEUE_GET_HEAD(prCurrQueue);

				if ((u4CurStaForwardFrameCount >= u4MaxForwardFrameCountLimit) ||
				    (u4CurStaUsedResource >= u4MaxResourceLimit)) {
					/* Exceeds Limit */

					break;
				} else if (prDequeuedPkt->u4PageCount > u4AvaliableResource) {
					/* Available Resource is not enough */
					if (!(prAdapter->rWifiVar.ucAlwaysResetUsedRes & BIT(0)))
						fgEndThisRound = TRUE;
					break;
				} else if (!prStaRec->fgIsValid) {
					/* In roaming, if the sta_rec doesn't
					 * active by event 0x0C, it can't
					 * dequeue data.
					 */
					DBGLOG(QM, WARN,
						"sta_rec is not valid\n");
					break;
				}
				/* Available to be Tx */

				QUEUE_REMOVE_HEAD(prCurrQueue, prDequeuedPkt, P_MSDU_INFO_T);

				if (!QUEUE_IS_EMPTY(prCurrQueue)) {
					/* XXX: check all queues for STA */
					prDequeuedPkt->ucPsForwardingType = PS_FORWARDING_MORE_DATA_ENABLED;
				}
				prDequeuedPkt->ucWmmQueSet = prBssInfo->ucWmmQueSet; /* to record WMM Set */
				QUEUE_INSERT_TAIL(prQue, (P_QUE_ENTRY_T) prDequeuedPkt);

				u4AvaliableResource -= prDequeuedPkt->u4PageCount;
				u4CurStaUsedResource += prDequeuedPkt->u4PageCount;
				u4CurStaForwardFrameCount++;
			}

			/* AP mode: Update STA in PS Free quota */
			if (prStaRec->fgIsInPS && pucPsStaFreeQuota) {
				if ((*pucPsStaFreeQuota) >= u4CurStaForwardFrameCount)
					(*pucPsStaFreeQuota) -= u4CurStaForwardFrameCount;
				else
					(*pucPsStaFreeQuota) = 0;
			}

			if (prBssInfo->fgIsNetAbsent) {
				if (prBssInfo->ucBssFreeQuota >= u4CurStaForwardFrameCount)
					prBssInfo->ucBssFreeQuota -= u4CurStaForwardFrameCount;
				else
					prBssInfo->ucBssFreeQuota = 0;
			}
		}

		if (fgEndThisRound) {
			/* End this round */
			break;
		}

		/* Prepare for next STA */
		ucLoop++;
		u4CurStaIndex++;
		u4CurStaIndex %= CFG_STA_REC_NUM;
		u4CurStaUsedResource = 0;
		u4CurStaForwardFrameCount = 0;
	}

	/* 4 <3> Store Head Sta information to QM */
	/* No need to count used resource if thers is only one STA */
	if ((prQM->u4TxAllowedStaCount == 1) || (prAdapter->rWifiVar.ucAlwaysResetUsedRes & BIT(1)))
		u4CurStaUsedResource = 0;

#if QM_FORWARDING_FAIRNESS
	prQM->au4HeadStaRecIndex[ucTC] = u4CurStaIndex;
	prQM->au4ResourceUsedCount[ucTC] = u4CurStaUsedResource;
#endif

	DBGLOG(QM, LOUD, "(Fairness) TC[%u] Scheduled Head STA[%u] Left Resource[%u]\n",
	       ucTC, u4CurStaIndex, u4AvaliableResource);

	return u4AvaliableResource;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Dequeue TX packets from a per-Type-based Queue for a particular TC
*
* \param[out] prQue The queue to put the dequeued packets
* \param[in] ucTC The TC index (Shall always be TC5_INDEX)
* \param[in] ucMaxNum The maximum amount of available resource
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID
qmDequeueTxPacketsFromPerTypeQueues(IN P_ADAPTER_T prAdapter, OUT P_QUE_T prQue,
	IN UINT_8 ucTC, IN UINT_32 u4CurrentQuota, IN UINT_32 u4TotalQuota)
{
	UINT_32 u4AvaliableResource, u4LeftResource;
	UINT_32 u4MaxResourceLimit;
	UINT_32 u4TotalUsedResource = 0;
	P_QUE_MGT_T prQM;
	PFN_DEQUEUE_FUNCTION pfnDeQFunc[2];
	BOOLEAN fgChangeDeQFunc = TRUE;
	BOOLEAN fgGlobalQueFirst = TRUE;

	DBGLOG(QM, LOUD, "Enter %s (TC = %d, quota = %u)\n", __func__, ucTC, u4CurrentQuota);

	/* Broadcast/Multicast data packets */
	if (u4CurrentQuota == 0)
		return;

	prQM = &prAdapter->rQM;

	u4AvaliableResource = u4CurrentQuota;
	u4MaxResourceLimit = u4TotalQuota;
#if QM_FORWARDING_FAIRNESS
	u4TotalUsedResource = prQM->u4GlobalResourceUsedCount;
	fgGlobalQueFirst = prQM->fgGlobalQFirst;
#endif

	/* Dequeue function selection */
	if (fgGlobalQueFirst) {
		pfnDeQFunc[0] = qmDequeueTxPacketsFromGlobalQueue;
		pfnDeQFunc[1] = qmDequeueTxPacketsFromPerStaQueues;
	} else {
		pfnDeQFunc[0] = qmDequeueTxPacketsFromPerStaQueues;
		pfnDeQFunc[1] = qmDequeueTxPacketsFromGlobalQueue;
	}

	/* 1st dequeue function */
	u4LeftResource = pfnDeQFunc[0](prAdapter, prQue, ucTC, u4AvaliableResource,
		(u4MaxResourceLimit - u4TotalUsedResource));

	/* dequeue function comsumes no resource, change */
	if ((u4LeftResource >= u4AvaliableResource) && (u4AvaliableResource >= NIC_TX_MAX_PAGE_PER_FRAME)) {
		fgChangeDeQFunc = TRUE;
	} else {
		u4TotalUsedResource += (u4AvaliableResource - u4LeftResource);
		/* Used resource exceeds limit, change */
		if (u4TotalUsedResource >= u4MaxResourceLimit)
			fgChangeDeQFunc = TRUE;
	}

	if (fgChangeDeQFunc) {
		fgGlobalQueFirst = !fgGlobalQueFirst;
		u4TotalUsedResource = 0;
	}

	/* 2nd dequeue function */
	u4LeftResource = pfnDeQFunc[1](prAdapter, prQue, ucTC, u4LeftResource, u4MaxResourceLimit);

#if QM_FORWARDING_FAIRNESS
	prQM->fgGlobalQFirst = fgGlobalQueFirst;
	prQM->u4GlobalResourceUsedCount = u4TotalUsedResource;
#endif

}				/* qmDequeueTxPacketsFromPerTypeQueues */

/*----------------------------------------------------------------------------*/
/*!
* \brief Dequeue TX packets from a QM global Queue for a particular TC
*
* \param[out] prQue The queue to put the dequeued packets
* \param[in] ucTC The TC index (Shall always be TC5_INDEX)
* \param[in] ucMaxNum The maximum amount of available resource
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
UINT_32
qmDequeueTxPacketsFromGlobalQueue(IN P_ADAPTER_T prAdapter, OUT P_QUE_T prQue,
	IN UINT_8 ucTC, IN UINT_32 u4CurrentQuota, IN UINT_32 u4TotalQuota)
{
	P_BSS_INFO_T prBssInfo;
	P_QUE_T prCurrQueue;
	UINT_32 u4AvaliableResource;
	P_MSDU_INFO_T prDequeuedPkt;
	P_MSDU_INFO_T prBurstEndPkt;
	QUE_T rMergeQue;
	P_QUE_T prMergeQue;
	P_QUE_MGT_T prQM;

	DBGLOG(QM, LOUD, "Enter %s (TC = %d, quota = %u)\n", __func__, ucTC, u4CurrentQuota);

	/* Broadcast/Multicast data packets */
	if (u4CurrentQuota == 0)
		return u4CurrentQuota;

	prQM = &prAdapter->rQM;

	/* 4 <1> Determine the queue */
	prCurrQueue = &prQM->arTxQueue[TX_QUEUE_INDEX_BMCAST];
	u4AvaliableResource = u4CurrentQuota;
	prDequeuedPkt = NULL;
	prBurstEndPkt = NULL;

	QUEUE_INITIALIZE(&rMergeQue);
	prMergeQue = &rMergeQue;

	/* 4 <2> Dequeue packets */
	while (!QUEUE_IS_EMPTY(prCurrQueue)) {
		prDequeuedPkt = (P_MSDU_INFO_T) QUEUE_GET_HEAD(prCurrQueue);
		if (prDequeuedPkt->u4PageCount > u4AvaliableResource)
			break;

		QUEUE_REMOVE_HEAD(prCurrQueue, prDequeuedPkt, P_MSDU_INFO_T);

		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prDequeuedPkt->ucBssIndex);

		if (IS_BSS_ACTIVE(prBssInfo)) {
			if (!prBssInfo->fgIsNetAbsent) {
				prDequeuedPkt->ucWmmQueSet = prBssInfo->ucWmmQueSet; /* to record WMM Set */
				QUEUE_INSERT_TAIL(prQue, (P_QUE_ENTRY_T) prDequeuedPkt);
				prBurstEndPkt = prDequeuedPkt;
				u4AvaliableResource -= prDequeuedPkt->u4PageCount;
				QM_DBG_CNT_INC(prQM, QM_DBG_CNT_26);
			} else {
				QUEUE_INSERT_TAIL(prMergeQue, (P_QUE_ENTRY_T) prDequeuedPkt);
			}
		} else {
			QM_TX_SET_NEXT_MSDU_INFO(prDequeuedPkt, NULL);
			wlanProcessQueuedMsduInfo(prAdapter, prDequeuedPkt);
		}
	}

	if (QUEUE_IS_NOT_EMPTY(prMergeQue)) {
		QUEUE_CONCATENATE_QUEUES(prMergeQue, prCurrQueue);
		QUEUE_MOVE_ALL(prCurrQueue, prMergeQue);
		QM_TX_SET_NEXT_MSDU_INFO((P_MSDU_INFO_T) QUEUE_GET_TAIL(prCurrQueue), NULL);
	}

	return u4AvaliableResource;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Dequeue TX packets to send to HIF TX
*
* \param[in] prTcqStatus Info about the maximum amount of dequeued packets
*
* \return The list of dequeued TX packets
*/
/*----------------------------------------------------------------------------*/
P_MSDU_INFO_T qmDequeueTxPackets(IN P_ADAPTER_T prAdapter, IN P_TX_TCQ_STATUS_T prTcqStatus)
{
	INT_32 i;
	P_MSDU_INFO_T prReturnedPacketListHead;
	QUE_T rReturnedQue;
	UINT_32 u4MaxQuotaLimit;

	DBGLOG(QM, LOUD, "Enter qmDequeueTxPackets\n");

	QUEUE_INITIALIZE(&rReturnedQue);

	prReturnedPacketListHead = NULL;

	/* TC0 to TC3: AC0~AC3 (commands packets are not handled by QM) */
	for (i = TC3_INDEX; i >= TC0_INDEX; i--) {
		DBGLOG(QM, LOUD, "Dequeue packets from Per-STA queue[%u]\n", i);

		/* If only one STA is Tx allowed, no need to restrict Max quota */
		if (prAdapter->rWifiVar.u4MaxTxDeQLimit)
			u4MaxQuotaLimit = prAdapter->rWifiVar.u4MaxTxDeQLimit;
		else if (prAdapter->rQM.u4TxAllowedStaCount == 1)
			u4MaxQuotaLimit = QM_STA_FORWARD_COUNT_UNLIMITED;
		else
			u4MaxQuotaLimit = (UINT_32) prTcqStatus->au4MaxNumOfPage[i];

		if (i == BMC_TC_INDEX)
			qmDequeueTxPacketsFromPerTypeQueues(prAdapter, &rReturnedQue, (UINT_8)i,
				prTcqStatus->au4FreePageCount[i], u4MaxQuotaLimit);
		else
			qmDequeueTxPacketsFromPerStaQueues(prAdapter, &rReturnedQue, (UINT_8)i,
				prTcqStatus->au4FreePageCount[i], u4MaxQuotaLimit);

		/* The aggregate number of dequeued packets */
		DBGLOG(QM, LOUD, "DQA)[%u](%u)\n", i, rReturnedQue.u4NumElem);
	}

	if (QUEUE_IS_NOT_EMPTY(&rReturnedQue)) {
		prReturnedPacketListHead = (P_MSDU_INFO_T) QUEUE_GET_HEAD(&rReturnedQue);
		QM_TX_SET_NEXT_MSDU_INFO((P_MSDU_INFO_T) QUEUE_GET_TAIL(&rReturnedQue), NULL);
	}

	return prReturnedPacketListHead;
}

#if CFG_SUPPORT_MULTITHREAD
/*----------------------------------------------------------------------------*/
/*!
* \brief Dequeue TX packets to send to HIF TX
*
* \param[in] prTcqStatus Info about the maximum amount of dequeued packets
*
* \return The list of dequeued TX packets
*/
/*----------------------------------------------------------------------------*/
P_MSDU_INFO_T qmDequeueTxPacketsMthread(IN P_ADAPTER_T prAdapter, IN P_TX_TCQ_STATUS_T prTcqStatus)
{

	/* INT_32 i; */
	P_MSDU_INFO_T prReturnedPacketListHead;
	/* QUE_T rReturnedQue; */
	/* UINT_32 u4MaxQuotaLimit; */
	P_MSDU_INFO_T prMsduInfo, prNextMsduInfo;

	KAL_SPIN_LOCK_DECLARATION();

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_RESOURCE);

	prReturnedPacketListHead = qmDequeueTxPackets(prAdapter, prTcqStatus);

	/* require the resource first to prevent from unsync */
	prMsduInfo = prReturnedPacketListHead;
	while (prMsduInfo) {
		prNextMsduInfo = (P_MSDU_INFO_T) QUEUE_GET_NEXT_ENTRY((P_QUE_ENTRY_T) prMsduInfo);
		nicTxAcquireResource(prAdapter, prMsduInfo->ucTC,
			nicTxGetPageCount(prMsduInfo->u2FrameLength, FALSE), FALSE);
		prMsduInfo = prNextMsduInfo;
	}

	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_RESOURCE);

	return prReturnedPacketListHead;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Adjust the TC quotas according to traffic demands
*
* \param[out] prTcqAdjust The resulting adjustment
* \param[in] prTcqStatus Info about the current TC quotas and counters
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
BOOLEAN
qmAdjustTcQuotasMthread(IN P_ADAPTER_T prAdapter, OUT P_TX_TCQ_ADJUST_T prTcqAdjust, IN P_TX_TCQ_STATUS_T prTcqStatus)
{
#if QM_ADAPTIVE_TC_RESOURCE_CTRL
	UINT_32 i;
	P_QUE_MGT_T prQM = &prAdapter->rQM;

	KAL_SPIN_LOCK_DECLARATION();

	/* Must initialize */
	for (i = 0; i < QM_ACTIVE_TC_NUM; i++)
		prTcqAdjust->ai4Variation[i] = 0;

	/* 4 <1> If TC resource is not just adjusted, exit directly */
	if (!prQM->fgTcResourcePostAnnealing)
		return FALSE;
	/* 4 <2> Adjust TcqStatus according to the updated prQM->au4CurrentTcResource */
	else {
		INT_32 i4TotalExtraQuota = 0;
		INT_32 ai4ExtraQuota[QM_ACTIVE_TC_NUM];
		BOOLEAN fgResourceRedistributed = TRUE;

		/* Must initialize */
		for (i = 0; i < TC_NUM; i++)
			prTcqAdjust->ai4Variation[i] = 0;

		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_RESOURCE);

		/* Obtain the free-to-distribute resource */
		for (i = 0; i < QM_ACTIVE_TC_NUM; i++) {
			ai4ExtraQuota[i] =
			    (INT_32) prTcqStatus->au4MaxNumOfBuffer[i] - (INT_32) prQM->au4CurrentTcResource[i];

			if (ai4ExtraQuota[i] > 0) {	/* The resource shall be reallocated to other TCs */
				if (ai4ExtraQuota[i] > prTcqStatus->au4FreeBufferCount[i]) {
					ai4ExtraQuota[i] = prTcqStatus->au4FreeBufferCount[i];
					fgResourceRedistributed = FALSE;
				}

				i4TotalExtraQuota += ai4ExtraQuota[i];
				prTcqAdjust->ai4Variation[i] = (-ai4ExtraQuota[i]);
			}
		}

		/* Distribute quotas to TCs which need extra resource according to prQM->au4CurrentTcResource */
		for (i = 0; i < QM_ACTIVE_TC_NUM; i++) {
			if (ai4ExtraQuota[i] < 0) {
				if ((-ai4ExtraQuota[i]) > i4TotalExtraQuota) {
					ai4ExtraQuota[i] = (-i4TotalExtraQuota);
					fgResourceRedistributed = FALSE;
				}

				i4TotalExtraQuota += ai4ExtraQuota[i];
				prTcqAdjust->ai4Variation[i] = (-ai4ExtraQuota[i]);
			}
		}

		/* In case some TC is waiting for TX Done, continue to adjust TC quotas upon TX Done */
		prQM->fgTcResourcePostAnnealing = (!fgResourceRedistributed);

		for (i = 0; i < TC_NUM; i++) {
			prTcqStatus->au4FreePageCount[i] += (prTcqAdjust->ai4Variation[i] * NIC_TX_MAX_PAGE_PER_FRAME);
			prTcqStatus->au4MaxNumOfPage[i] += (prTcqAdjust->ai4Variation[i] * NIC_TX_MAX_PAGE_PER_FRAME);

			prTcqStatus->au4FreeBufferCount[i] += prTcqAdjust->ai4Variation[i];
			prTcqStatus->au4MaxNumOfBuffer[i] += prTcqAdjust->ai4Variation[i];

			ASSERT(prTcqStatus->au4FreeBufferCount[i] >= 0);
			ASSERT(prTcqStatus->au4MaxNumOfBuffer[i] >= 0);
		}

#if QM_FAST_TC_RESOURCE_CTRL
		prQM->fgTcResourceFastReaction = FALSE;
#endif
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_RESOURCE);
	}

	return TRUE;
#else
	return FALSE;
#endif
}
#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief Adjust the TC quotas according to traffic demands
*
* \param[out] prTcqAdjust The resulting adjustment
* \param[in] prTcqStatus Info about the current TC quotas and counters
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
BOOLEAN qmAdjustTcQuotas(IN P_ADAPTER_T prAdapter, OUT P_TX_TCQ_ADJUST_T prTcqAdjust, IN P_TX_TCQ_STATUS_T prTcqStatus)
{
#if QM_ADAPTIVE_TC_RESOURCE_CTRL
	UINT_32 i;
	P_QUE_MGT_T prQM = &prAdapter->rQM;

	/* Must initialize */
	for (i = 0; i < QM_ACTIVE_TC_NUM; i++)
		prTcqAdjust->ai4Variation[i] = 0;

	/* 4 <1> If TC resource is not just adjusted, exit directly */
	if (!prQM->fgTcResourcePostAnnealing)
		return FALSE;
	/* 4 <2> Adjust TcqStatus according to the updated prQM->au4CurrentTcResource */
	else {
		INT_32 i4TotalExtraQuota = 0;
		INT_32 ai4ExtraQuota[QM_ACTIVE_TC_NUM];
		BOOLEAN fgResourceRedistributed = TRUE;

		/* Obtain the free-to-distribute resource */
		for (i = 0; i < QM_ACTIVE_TC_NUM; i++) {
			ai4ExtraQuota[i] =
			    (INT_32) prTcqStatus->au4MaxNumOfBuffer[i] - (INT_32) prQM->au4CurrentTcResource[i];

			if (ai4ExtraQuota[i] > 0) {	/* The resource shall be reallocated to other TCs */
				if (ai4ExtraQuota[i] > prTcqStatus->au4FreeBufferCount[i]) {
					ai4ExtraQuota[i] = prTcqStatus->au4FreeBufferCount[i];
					fgResourceRedistributed = FALSE;
				}

				i4TotalExtraQuota += ai4ExtraQuota[i];
				prTcqAdjust->ai4Variation[i] = (-ai4ExtraQuota[i]);
			}
		}

		/* Distribute quotas to TCs which need extra resource according to prQM->au4CurrentTcResource */
		for (i = 0; i < QM_ACTIVE_TC_NUM; i++) {
			if (ai4ExtraQuota[i] < 0) {
				if ((-ai4ExtraQuota[i]) > i4TotalExtraQuota) {
					ai4ExtraQuota[i] = (-i4TotalExtraQuota);
					fgResourceRedistributed = FALSE;
				}

				i4TotalExtraQuota += ai4ExtraQuota[i];
				prTcqAdjust->ai4Variation[i] = (-ai4ExtraQuota[i]);
			}
		}

		/* In case some TC is waiting for TX Done, continue to adjust TC quotas upon TX Done */
		prQM->fgTcResourcePostAnnealing = (!fgResourceRedistributed);

#if QM_FAST_TC_RESOURCE_CTRL
		prQM->fgTcResourceFastReaction = FALSE;
#endif

#if QM_PRINT_TC_RESOURCE_CTRL
		DBGLOG(QM, LOUD, "QM: Curr Quota [0]=%u [1]=%u [2]=%u [3]=%u [4]=%u [5]=%u\n",
		       prTcqStatus->au4FreeBufferCount[0],
		       prTcqStatus->au4FreeBufferCount[1],
		       prTcqStatus->au4FreeBufferCount[2],
		       prTcqStatus->au4FreeBufferCount[3],
		       prTcqStatus->au4FreeBufferCount[4], prTcqStatus->au4FreeBufferCount[5]);
#endif
	}

	return TRUE;
#else
	return FALSE;
#endif
}

#if QM_ADAPTIVE_TC_RESOURCE_CTRL
/*----------------------------------------------------------------------------*/
/*!
* \brief Update the average TX queue length for the TC resource control mechanism
*
* \param (none)
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID qmUpdateAverageTxQueLen(IN P_ADAPTER_T prAdapter)
{
	INT_32 u4CurrQueLen, u4Tc, u4StaRecIdx;
	P_STA_RECORD_T prStaRec;
	P_QUE_MGT_T prQM = &prAdapter->rQM;
	P_BSS_INFO_T prBssInfo;

	/* 4 <1> Update the queue lengths for TC0 to TC3 (skip TC4) and TC5 */
	for (u4Tc = 0; u4Tc < QM_ACTIVE_TC_NUM; u4Tc++) {
		u4CurrQueLen = 0;

		/* Calculate per-STA queue length */
		if (u4Tc < NUM_OF_PER_STA_TX_QUEUES) {
			for (u4StaRecIdx = 0; u4StaRecIdx < CFG_STA_REC_NUM; u4StaRecIdx++) {
				prStaRec = cnmGetStaRecByIndex(prAdapter, u4StaRecIdx);
				if (prStaRec) {
					prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);

					/* If the STA is activated, get the queue length */
					if ((prStaRec->fgIsValid) && (!prBssInfo->fgIsNetAbsent))
						u4CurrQueLen += (prStaRec->arTxQueue[u4Tc].u4NumElem);
				}
			}
		}

		if (u4Tc == BMC_TC_INDEX) {
			/* Update the queue length for (BMCAST) */
			u4CurrQueLen += prQM->arTxQueue[TX_QUEUE_INDEX_BMCAST].u4NumElem;
		}

		if (prQM->au4AverageQueLen[u4Tc] == 0) {
			prQM->au4AverageQueLen[u4Tc] = (u4CurrQueLen << prQM->u4QueLenMovingAverage);
		} else {
			prQM->au4AverageQueLen[u4Tc] -= (prQM->au4AverageQueLen[u4Tc] >> prQM->u4QueLenMovingAverage);
			prQM->au4AverageQueLen[u4Tc] += (u4CurrQueLen);
		}
	}
#if 0
	/* Update the queue length for TC5 (BMCAST) */
	u4CurrQueLen = prQM->arTxQueue[TX_QUEUE_INDEX_BMCAST].u4NumElem;

	if (prQM->au4AverageQueLen[TC5_INDEX] == 0) {
		prQM->au4AverageQueLen[TC5_INDEX] = (u4CurrQueLen << QM_QUE_LEN_MOVING_AVE_FACTOR);
	} else {
		prQM->au4AverageQueLen[TC5_INDEX] -=
		    (prQM->au4AverageQueLen[TC5_INDEX] >> QM_QUE_LEN_MOVING_AVE_FACTOR);
		prQM->au4AverageQueLen[TC5_INDEX] += (u4CurrQueLen);
	}
#endif
}

VOID qmAllocateResidualTcResource(IN P_ADAPTER_T prAdapter, IN PINT_32 ai4TcResDemand,
	IN PUINT_32 pu4ResidualResource, IN PUINT_32 pu4ShareCount)
{
	P_QUE_MGT_T prQM = &prAdapter->rQM;
	UINT_32 u4Share = 0;
	UINT_32 u4TcIdx;
	UINT_8 ucIdx;
	UINT_32 au4AdjTc[] = { TC3_INDEX, TC2_INDEX, TC5_INDEX, TC1_INDEX, TC0_INDEX };
	UINT_32 u4AdjTcSize = (sizeof(au4AdjTc) / sizeof(UINT_32));
	UINT_32 u4ResidualResource = *pu4ResidualResource;
	UINT_32 u4ShareCount = *pu4ShareCount;

	/* If there is no resource left, exit directly */
	if (u4ResidualResource == 0)
		return;

	/* This shall not happen */
	if (u4ShareCount == 0) {
		prQM->au4CurrentTcResource[TC1_INDEX] += u4ResidualResource;
		DBGLOG(QM, ERROR, "QM: (Error) u4ShareCount = 0\n");
		return;
	}

	/* Share the residual resource evenly */
	u4Share = (u4ResidualResource / u4ShareCount);
	if (u4Share) {
		for (u4TcIdx = 0; u4TcIdx < QM_ACTIVE_TC_NUM; u4TcIdx++) {
			/* Skip TC4 (not adjustable) */
			if (u4TcIdx == TC4_INDEX)
				continue;

			if (ai4TcResDemand[u4TcIdx] > 0) {
				if (ai4TcResDemand[u4TcIdx] > u4Share) {
					prQM->au4CurrentTcResource[u4TcIdx] += u4Share;
					u4ResidualResource -= u4Share;
					ai4TcResDemand[u4TcIdx] -= u4Share;
				} else {
					prQM->au4CurrentTcResource[u4TcIdx] += ai4TcResDemand[u4TcIdx];
					u4ResidualResource -= ai4TcResDemand[u4TcIdx];
					ai4TcResDemand[u4TcIdx] = 0;
				}
			}
		}
	}

	/* By priority, allocate the left resource that is not divisible by u4Share */
	ucIdx = 0;
	while (u4ResidualResource) {
		u4TcIdx = au4AdjTc[ucIdx];

		if (ai4TcResDemand[u4TcIdx]) {
			prQM->au4CurrentTcResource[u4TcIdx]++;
			u4ResidualResource--;
			ai4TcResDemand[u4TcIdx]--;

			if (ai4TcResDemand[u4TcIdx] == 0)
				u4ShareCount--;
		}

		if (u4ShareCount <= 0)
			break;

		ucIdx++;
		ucIdx %= u4AdjTcSize;
	}

	/* Allocate the left resource */
	prQM->au4CurrentTcResource[TC3_INDEX] += u4ResidualResource;

	*pu4ResidualResource = u4ResidualResource;
	*pu4ShareCount = u4ShareCount;

}

/*----------------------------------------------------------------------------*/
/*!
* \brief Assign TX resource for each TC according to TX queue length and current assignment
*
* \param (none)
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID qmReassignTcResource(IN P_ADAPTER_T prAdapter)
{
	INT_32 i4TotalResourceDemand = 0;
	UINT_32 u4ResidualResource = 0;
	UINT_32 u4TcIdx;
	INT_32 ai4TcResDemand[QM_ACTIVE_TC_NUM];
	UINT_32 u4ShareCount = 0;
	UINT_32 u4Share = 0;
	P_QUE_MGT_T prQM = &prAdapter->rQM;
	UINT_32 u4ActiveTcCount = 0;
	UINT_32 u4LastActiveTcIdx = TC3_INDEX;

	/* Note: After the new assignment is obtained, set prQM->fgTcResourcePostAnnealing to TRUE to
	 *  start the TC-quota adjusting procedure, which will be invoked upon every TX Done
	 */

	/* 4 <1> Determine the demands */
	/* Determine the amount of extra resource to fulfill all of the demands */
	for (u4TcIdx = 0; u4TcIdx < QM_ACTIVE_TC_NUM; u4TcIdx++) {
		/* Skip TC4, which is not adjustable */
		if (u4TcIdx == TC4_INDEX)
			continue;

		/* Define: extra_demand = que_length + min_reserved_quota - current_quota */
		ai4TcResDemand[u4TcIdx] = ((INT_32)(QM_GET_TX_QUEUE_LEN(prAdapter, u4TcIdx) +
			prQM->au4MinReservedTcResource[u4TcIdx]) - (INT_32)prQM->au4CurrentTcResource[u4TcIdx]);

		/* If there are queued packets, allocate extra resource for the TC (for TCP consideration) */
		if (QM_GET_TX_QUEUE_LEN(prAdapter, u4TcIdx)) {
			ai4TcResDemand[u4TcIdx] += prQM->u4ExtraReservedTcResource;
			u4ActiveTcCount++;
		}

		i4TotalResourceDemand += ai4TcResDemand[u4TcIdx];
	}

	/* 4 <2> Case 1: Demand <= Total Resource */
	if (i4TotalResourceDemand <= 0) {

		/* 4 <2.1> Calculate the residual resource evenly */
		if (u4ActiveTcCount == 0)
			u4ShareCount = (QM_ACTIVE_TC_NUM - 1);	/* excluding TC4 */
		else
			u4ShareCount = u4ActiveTcCount;
		u4ResidualResource = (UINT_32) (-i4TotalResourceDemand);
		u4Share = (u4ResidualResource / u4ShareCount);

		/* 4 <2.2> Satisfy every TC and share the residual resource evenly */
		for (u4TcIdx = 0; u4TcIdx < QM_ACTIVE_TC_NUM; u4TcIdx++) {
			/* Skip TC4 (not adjustable) */
			if (u4TcIdx == TC4_INDEX)
				continue;

			prQM->au4CurrentTcResource[u4TcIdx] += ai4TcResDemand[u4TcIdx];

			/* Every TC is fully satisfied */
			ai4TcResDemand[u4TcIdx] = 0;

			/* The left resource will be allocated */
			if (QM_GET_TX_QUEUE_LEN(prAdapter, u4TcIdx) || (u4ActiveTcCount == 0)) {
				prQM->au4CurrentTcResource[u4TcIdx] += u4Share;
				u4ResidualResource -= u4Share;
				u4LastActiveTcIdx = u4TcIdx;
			}
		}

		/* 4 <2.3> Allocate the left resource to last active TC */
		prQM->au4CurrentTcResource[u4LastActiveTcIdx] += (u4ResidualResource);

	}
	/* 4 <3> Case 2: Demand > Total Resource --> Guarantee a minimum amount of resource for each TC */
	else {
		u4ShareCount = 0;
		u4ResidualResource = prQM->u4ResidualTcResource;

		/* 4 <3.1> Allocated resouce amount  = minimum of (guaranteed, total demand) */
		for (u4TcIdx = 0; u4TcIdx < QM_ACTIVE_TC_NUM; u4TcIdx++) {
			/* Skip TC4 (not adjustable) */
			if (u4TcIdx == TC4_INDEX)
				continue;

			/* The demand can be fulfilled with the guaranteed resource amount */
			if ((prQM->au4CurrentTcResource[u4TcIdx] + ai4TcResDemand[u4TcIdx]) <=
				prQM->au4GuaranteedTcResource[u4TcIdx]) {

				prQM->au4CurrentTcResource[u4TcIdx] += ai4TcResDemand[u4TcIdx];
				u4ResidualResource +=
					(prQM->au4GuaranteedTcResource[u4TcIdx] - prQM->au4CurrentTcResource[u4TcIdx]);
				ai4TcResDemand[u4TcIdx] = 0;
			}

			/* The demand can not be fulfilled with the guaranteed resource amount */
			else {
				ai4TcResDemand[u4TcIdx] -=
					(prQM->au4GuaranteedTcResource[u4TcIdx] - prQM->au4CurrentTcResource[u4TcIdx]);

				prQM->au4CurrentTcResource[u4TcIdx] = prQM->au4GuaranteedTcResource[u4TcIdx];
				u4ShareCount++;
			}
		}

		/* 4 <3.2> Allocate the residual resource */
		qmAllocateResidualTcResource(prAdapter, ai4TcResDemand, &u4ResidualResource, &u4ShareCount);
	}

	prQM->fgTcResourcePostAnnealing = TRUE;

#if QM_PRINT_TC_RESOURCE_CTRL
	/* Debug print */
	DBGLOG(QM, INFO, "QM: TC Rsc adjust to [%03u:%03u:%03u:%03u:%03u:%03u]\n",
	       prQM->au4CurrentTcResource[0], prQM->au4CurrentTcResource[1],
	       prQM->au4CurrentTcResource[2], prQM->au4CurrentTcResource[3],
	       prQM->au4CurrentTcResource[4], prQM->au4CurrentTcResource[5]);
#endif

}

/*----------------------------------------------------------------------------*/
/*!
* \brief Adjust TX resource for each TC according to TX queue length and current assignment
*
* \param (none)
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID qmDoAdaptiveTcResourceCtrl(IN P_ADAPTER_T prAdapter)
{
	P_QUE_MGT_T prQM = &prAdapter->rQM;

	/* 4 <0> Check to update queue length or not */
	if (--prQM->u4TimeToUpdateQueLen)
		return;
	/* 4 <1> Update TC queue length */
	prQM->u4TimeToUpdateQueLen = QM_INIT_TIME_TO_UPDATE_QUE_LEN;
	qmUpdateAverageTxQueLen(prAdapter);

	/* 4 <2> Adjust TC resource assignment */
	/* Check whether it is time to adjust the TC resource assignment */
	if (--prQM->u4TimeToAdjustTcResource == 0) {
		/* The last assignment has not been completely applied */
		if (prQM->fgTcResourcePostAnnealing) {
			/* Upon the next qmUpdateAverageTxQueLen function call, do this check again */
			prQM->u4TimeToAdjustTcResource = 1;
		}

		/* The last assignment has been applied */
		else {
			prQM->u4TimeToAdjustTcResource = QM_INIT_TIME_TO_ADJUST_TC_RSC;
			qmReassignTcResource(prAdapter);
#if QM_FAST_TC_RESOURCE_CTRL
			if (prQM->fgTcResourceFastReaction) {
				prQM->fgTcResourceFastReaction = FALSE;
				nicTxAdjustTcq(prAdapter);
			}
#endif
		}
	}

	/* Debug */
#if QM_PRINT_TC_RESOURCE_CTRL
	do {
		UINT_32 u4Tc;

		for (u4Tc = 0; u4Tc < QM_ACTIVE_TC_NUM; u4Tc++) {
			if (QM_GET_TX_QUEUE_LEN(prAdapter, u4Tc) >= 100) {
				DBGLOG(QM, LOUD, "QM: QueLen [%ld %ld %ld %ld %ld %ld]\n",
				       QM_GET_TX_QUEUE_LEN(prAdapter, 0),
				       QM_GET_TX_QUEUE_LEN(prAdapter, 1),
				       QM_GET_TX_QUEUE_LEN(prAdapter, 2),
				       QM_GET_TX_QUEUE_LEN(prAdapter, 3),
				       QM_GET_TX_QUEUE_LEN(prAdapter, 4), QM_GET_TX_QUEUE_LEN(prAdapter, 5)
				    );
				break;
			}
		}
	} while (FALSE);
#endif

}

#if QM_FAST_TC_RESOURCE_CTRL
VOID qmCheckForFastTcResourceCtrl(IN P_ADAPTER_T prAdapter, IN UINT_8 ucTc)
{
	P_QUE_MGT_T prQM = &prAdapter->rQM;
	BOOLEAN fgTrigger = FALSE;

	/* Trigger TC resource adjustment if there is a requirement coming for a empty TC */
	if (!prAdapter->rTxCtrl.rTc.au4FreeBufferCount[ucTc]) {
		if (!prQM->au4CurrentTcResource[ucTc] ||
			nicTxGetAdjustableResourceCnt(prAdapter))
			fgTrigger = TRUE;
	}

	if (fgTrigger) {
		prQM->u4TimeToUpdateQueLen = 1;
		prQM->u4TimeToAdjustTcResource = 1;
		prQM->fgTcResourceFastReaction = TRUE;

		DBGLOG(QM, LOUD, "Trigger TC Resource adjustment for TC[%u]\n", ucTc);
	}
}
#endif

#endif

UINT_32
gmGetDequeueQuota(
	IN P_ADAPTER_T		prAdapter,
	IN P_STA_RECORD_T	prStaRec,
	IN P_BSS_INFO_T		prBssInfo,
	IN UINT_32			u4TotalQuota
	)
{
	UINT_32	u4Weight = 100;
	UINT_32	u4Quota;

	P_QUE_MGT_T prQM = &prAdapter->rQM;

	if ((prAdapter->rWifiVar.uDeQuePercentEnable == FALSE) ||
		(prQM->fgIsTxResrouceControlEn == FALSE))
		return u4TotalQuota;

	if (prStaRec->ucDesiredPhyTypeSet & PHY_TYPE_BIT_VHT) {
		if (prBssInfo->ucVhtChannelWidth > VHT_OP_CHANNEL_WIDTH_20_40) {
			/* BW80 NSS1 rate: MCS9 433 Mbps */
			u4Weight = prAdapter->rWifiVar.u4DeQuePercentVHT80Nss1;
		} else if (prBssInfo->eBssSCO != CHNL_EXT_SCN) {
			/* BW40 NSS1 Max rate: 200 Mbps */
			u4Weight = prAdapter->rWifiVar.u4DeQuePercentVHT40Nss1;
		} else {
			/* BW20 NSS1 Max rate: 72.2Mbps (MCS8 86.7Mbps) */
			u4Weight = prAdapter->rWifiVar.u4DeQuePercentVHT20Nss1;
		}
	} else if (prStaRec->ucDesiredPhyTypeSet & PHY_TYPE_BIT_HT) {
		if (prBssInfo->ucHtOpInfo1 & HT_OP_INFO1_STA_CHNL_WIDTH) {
			/* BW40 NSS1 Max rate: 150 Mbps (MCS9 200Mbps)*/
			u4Weight = prAdapter->rWifiVar.u4DeQuePercentHT40Nss1;
		} else {
			/* BW20 NSS1 Max rate: 72.2Mbps (MCS8 86.7Mbps)*/
			u4Weight = prAdapter->rWifiVar.u4DeQuePercentHT20Nss1;
		}
	}

	u4Quota = u4TotalQuota * u4Weight / 100;

	if (u4Quota > u4TotalQuota || u4Quota <= 0)
		return u4TotalQuota;

	return u4Quota;
}

/*----------------------------------------------------------------------------*/
/* RX-Related Queue Management                                                */
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
/*!
* \brief Init Queue Management for RX
*
* \param[in] (none)
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID qmInitRxQueues(IN P_ADAPTER_T prAdapter)
{
	/* DbgPrint("QM: Enter qmInitRxQueues()\n"); */
	/* TODO */
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Handle RX packets (buffer reordering)
*
* \param[in] prSwRfbListHead The list of RX packets
*
* \return The list of packets which are not buffered for reordering
*/
/*----------------------------------------------------------------------------*/
P_SW_RFB_T qmHandleRxPackets(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfbListHead)
{

#if CFG_RX_REORDERING_ENABLED
	/* UINT_32 i; */
	P_SW_RFB_T prCurrSwRfb;
	P_SW_RFB_T prNextSwRfb;
	P_HW_MAC_RX_DESC_T prRxStatus;
	QUE_T rReturnedQue;
	P_QUE_T prReturnedQue;
	PUINT_8 pucEthDestAddr;
	BOOLEAN fgIsBMC, fgIsHTran;
	BOOLEAN fgMicErr;
#if CFG_SUPPORT_REPLAY_DETECTION
	UINT_8 ucBssIndexRly = 0;
	P_BSS_INFO_T prBssInfoRly = NULL;
#endif

	/* DbgPrint("QM: Enter qmHandleRxPackets()\n"); */

	DEBUGFUNC("qmHandleRxPackets");

	ASSERT(prSwRfbListHead);

	prReturnedQue = &rReturnedQue;

	QUEUE_INITIALIZE(prReturnedQue);
	prNextSwRfb = prSwRfbListHead;

	do {
		prCurrSwRfb = prNextSwRfb;
		prNextSwRfb = QM_RX_GET_NEXT_SW_RFB(prCurrSwRfb);

		/* prHifRxHdr = prCurrSwRfb->prHifRxHdr; // TODO: (Tehuang) Use macro to obtain the pointer */
		prRxStatus = prCurrSwRfb->prRxStatus;

		/* TODO: (Tehuang) Check if relaying */
		prCurrSwRfb->eDst = RX_PKT_DESTINATION_HOST;

		/* Decide the Destination */
#if CFG_RX_PKTS_DUMP
		if (prAdapter->rRxCtrl.u4RxPktsDumpTypeMask & BIT(HIF_RX_PKT_TYPE_DATA)) {
			DBGLOG(SW4, INFO, "QM RX DATA: net _u sta idx %u wlan idx %u ssn _u tid %u ptype %u 11 %u\n",
			       /* HIF_RX_HDR_GET_NETWORK_IDX(prHifRxHdr), */
			       prCurrSwRfb->ucStaRecIdx, prRxStatus->ucWlanIdx,
			       /* HIF_RX_HDR_GET_SN(prHifRxHdr), *//* The new SN of the frame */
			       HAL_RX_STATUS_GET_TID(prRxStatus),
			       prCurrSwRfb->ucPacketType, prCurrSwRfb->fgReorderBuffer);

			DBGLOG_MEM8(SW4, TRACE, (PUINT_8) prCurrSwRfb->pvHeader, prCurrSwRfb->u2PacketLen);
		}
#endif

		fgIsBMC = HAL_RX_STATUS_IS_BC(prRxStatus) | HAL_RX_STATUS_IS_MC(prRxStatus);
		fgIsHTran = FALSE;
		if (HAL_RX_STATUS_GET_HEADER_TRAN(prRxStatus) == TRUE) {
			/* (!HIF_RX_HDR_GET_80211_FLAG(prHifRxHdr)){ */

			UINT_8 ucBssIndex;
			P_BSS_INFO_T prBssInfo;
			UINT_8 aucTaAddr[MAC_ADDR_LEN];

			fgIsHTran = TRUE;
			pucEthDestAddr = prCurrSwRfb->pvHeader;

			if (prCurrSwRfb->prRxStatusGroup4 == NULL) {
				DBGLOG(QM, ERROR, "RxStatusGroup4 is NULL!!\n");
				DBGLOG_MEM8(QM, ERROR, prCurrSwRfb->pucRecvBuff,
					HAL_RX_STATUS_GET_RX_BYTE_CNT(
					prRxStatus));
				prCurrSwRfb->eDst = RX_PKT_DESTINATION_NULL;
				QUEUE_INSERT_TAIL(prReturnedQue,
					(P_QUE_ENTRY_T) prCurrSwRfb);
				DBGLOG_MEM8(RX, WARN, (uint8_t *) prRxStatus,
					sizeof(*prRxStatus));
				if (prCurrSwRfb->pvHeader)
					DBGLOG_MEM8(RX, WARN,
						prCurrSwRfb->pvHeader,
						prCurrSwRfb->u2PacketLen >
						 32 ? 32 :
						prCurrSwRfb->u2PacketLen);
				continue;
			}

			if (prCurrSwRfb->prStaRec == NULL) {
				/* Workaround WTBL Issue */

				HAL_RX_STATUS_GET_TA(prCurrSwRfb->prRxStatusGroup4, aucTaAddr);
				prCurrSwRfb->ucStaRecIdx = secLookupStaRecIndexFromTA(prAdapter, aucTaAddr);
				if (prCurrSwRfb->ucStaRecIdx < CFG_STA_REC_NUM) {
					prCurrSwRfb->prStaRec =
					    cnmGetStaRecByIndex(prAdapter, prCurrSwRfb->ucStaRecIdx);
					DBGLOG(QM, TRACE,
					       "Re-search the staRec = %d, mac = " MACSTR ", byteCnt= %d\n",
					       prCurrSwRfb->ucStaRecIdx, MAC2STR(aucTaAddr), prRxStatus->u2RxByteCount);
				}

				if (prCurrSwRfb->prStaRec == NULL) {
					DBGLOG(QM, TRACE,
					       "Mark NULL Packet,StaRec=NULL,wlanIdx:%d,but via Header Translation\n",
					       prRxStatus->ucWlanIdx);
					/* DBGLOG_MEM8(SW4, TRACE, (PUINT_8)prRxStatus, prRxStatus->u2RxByteCount); */
					RX_INC_CNT(&prAdapter->rRxCtrl, RX_NO_STA_DROP_COUNT);
					prCurrSwRfb->eDst = RX_PKT_DESTINATION_NULL;
					QUEUE_INSERT_TAIL(prReturnedQue, (P_QUE_ENTRY_T) prCurrSwRfb);
					continue;
				}

				prCurrSwRfb->ucWlanIdx = prCurrSwRfb->prStaRec->ucWlanIndex;
				GLUE_SET_PKT_BSS_IDX(prCurrSwRfb->pvPacket,
						     secGetBssIdxByWlanIdx(prAdapter, prCurrSwRfb->ucWlanIdx));
			}
			/* ASSERT(prAdapter->rWifiVar.arWtbl[prCurrSwRfb->ucWlanIdx].ucUsed); */
			if (prAdapter->rRxCtrl.rFreeSwRfbList.u4NumElem
			    > (CFG_RX_MAX_PKT_NUM - CFG_NUM_OF_QM_RX_PKT_NUM) || TRUE) {

				ucBssIndex = prCurrSwRfb->prStaRec->ucBssIndex;
				prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);
				/* DBGLOG_MEM8(QM, TRACE,prCurrSwRfb->pvHeader, 16); */
				/*  */

				/* if ((OP_MODE_ACCESS_POINT != prBssInfo->eCurrentOPMode)) { */
				/* fgIsBMC = HAL_RX_STATUS_IS_BC(prRxStatus) | HAL_RX_STATUS_IS_MC(prRxStatus); */
				/* } */

				if (!IS_BSS_ACTIVE(prBssInfo)) {
					DBGLOG(QM, TRACE, "Mark NULL the Packet for inactive Bss %u\n", ucBssIndex);
					RX_INC_CNT(&prAdapter->rRxCtrl, RX_INACTIVE_BSS_DROP_COUNT);
					prCurrSwRfb->eDst = RX_PKT_DESTINATION_NULL;
					QUEUE_INSERT_TAIL(prReturnedQue, (P_QUE_ENTRY_T) prCurrSwRfb);
					continue;
				}

				if (prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT) {
					if (IS_BMCAST_MAC_ADDR(pucEthDestAddr)) {
						prCurrSwRfb->eDst = RX_PKT_DESTINATION_HOST_WITH_FORWARD;
					} else if (secLookupStaRecIndexFromTA(prAdapter, pucEthDestAddr)
						!= STA_REC_INDEX_NOT_FOUND) {

						prCurrSwRfb->eDst = RX_PKT_DESTINATION_FORWARD;
						/* TODO : need to check the dst mac is valid */
						/* If src mac is invalid, the packet will be freed in fw */
					}
				}
#if CFG_SUPPORT_PASSPOINT
				else if (hs20IsFrameFilterEnabled(prAdapter, prBssInfo) &&
					 hs20IsUnsecuredFrame(prAdapter, prBssInfo, prCurrSwRfb)) {
					DBGLOG(QM, WARN, "Mark NULL the Packet for Dropped Packet %u\n", ucBssIndex);
					RX_INC_CNT(&prAdapter->rRxCtrl, RX_HS20_DROP_COUNT);
					prCurrSwRfb->eDst = RX_PKT_DESTINATION_NULL;
					QUEUE_INSERT_TAIL(prReturnedQue, (P_QUE_ENTRY_T) prCurrSwRfb);
					continue;
				}
#endif /* CFG_SUPPORT_PASSPOINT */

			} else {
				/* Dont not occupy other SW RFB */
				DBGLOG(QM, TRACE, "Mark NULL the Packet for less Free Sw Rfb\n");
				RX_INC_CNT(&prAdapter->rRxCtrl, RX_LESS_SW_RFB_DROP_COUNT);
				prCurrSwRfb->eDst = RX_PKT_DESTINATION_NULL;
				QUEUE_INSERT_TAIL(prReturnedQue, (P_QUE_ENTRY_T) prCurrSwRfb);
				continue;
			}

		}

#if CFG_SUPPORT_WAPI
		if (prCurrSwRfb->u2PacketLen > ETHER_HEADER_LEN) {
			PUINT_8 pc = (PUINT_8) prCurrSwRfb->pvHeader;
			UINT_16 u2Etype = 0;

			u2Etype = (pc[ETHER_TYPE_LEN_OFFSET] << 8) | (pc[ETHER_TYPE_LEN_OFFSET + 1]);
			/* for wapi integrity test. WPI_1x packet should be always in non-encrypted mode.
			 * if we received any WPI(0x88b4) packet that is encrypted, drop here.
			 */
			if (u2Etype == ETH_WPI_1X &&
				HAL_RX_STATUS_GET_SEC_MODE(prRxStatus) != 0 &&
				HAL_RX_STATUS_IS_CIPHER_MISMATCH(prRxStatus) == 0) {
				DBGLOG(QM, INFO, "drop wpi packet with sec mode\n");
				prCurrSwRfb->eDst = RX_PKT_DESTINATION_NULL;
				QUEUE_INSERT_TAIL(prReturnedQue, (P_QUE_ENTRY_T) prCurrSwRfb);
				continue;
			}
		}
#endif


		/* Todo:: Move the data class error check here */

#if CFG_SUPPORT_REPLAY_DETECTION
		if (prCurrSwRfb->prStaRec) {
			ucBssIndexRly = prCurrSwRfb->prStaRec->ucBssIndex;
			prBssInfoRly = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndexRly);
			if (!IS_BSS_ACTIVE(prBssInfoRly)) {
				DBGLOG(QM, INFO,
					"Mark NULL the Packet for inactive Bss %u\n",
					ucBssIndexRly);
				prCurrSwRfb->eDst = RX_PKT_DESTINATION_NULL;
				QUEUE_INSERT_TAIL(prReturnedQue, (P_QUE_ENTRY_T) prCurrSwRfb);
				continue;
			}
			if (fgIsBMC
				&& prBssInfoRly
				&& (IS_BSS_AIS(prBssInfoRly) || IS_BSS_P2P(prBssInfoRly))
				&& qmHandleRxReplay(prAdapter, prCurrSwRfb)) {
				prCurrSwRfb->eDst = RX_PKT_DESTINATION_NULL;
				QUEUE_INSERT_TAIL(prReturnedQue, (P_QUE_ENTRY_T) prCurrSwRfb);
				continue;
			}
		}
#endif

		if (prCurrSwRfb->fgReorderBuffer && !fgIsBMC && fgIsHTran) {
			/* If this packet should dropped or indicated to the host immediately,
			 *  it should be enqueued into the rReturnedQue with specific flags. If
			 *  this packet should be buffered for reordering, it should be enqueued
			 *  into the reordering queue in the STA_REC rather than into the
			 *  rReturnedQue.
			 */
			qmProcessPktWithReordering(prAdapter, prCurrSwRfb, prReturnedQue);

		} else if (prCurrSwRfb->fgDataFrame) {
			/* Check Class Error */
			if (secCheckClassError(prAdapter, prCurrSwRfb, prCurrSwRfb->prStaRec) == TRUE) {
				P_RX_BA_ENTRY_T prReorderQueParm = NULL;

				/* Invalid BA aggrement */
				if (fgIsHTran) {
					UINT_16 u2FrameCtrl = 0;

					u2FrameCtrl = HAL_RX_STATUS_GET_FRAME_CTL_FIELD(prCurrSwRfb->prRxStatusGroup4);
					/* Check FC type, if DATA, then no-reordering */
					if ((u2FrameCtrl & MASK_FRAME_TYPE) == MAC_FRAME_DATA) {
						DBGLOG(QM, TRACE, "FC [0x%04X], no-reordering...\n", u2FrameCtrl);
					} else {
						prReorderQueParm =
						    ((prCurrSwRfb->prStaRec->
						      aprRxReorderParamRefTbl)[prCurrSwRfb->ucTid]);
					}
				}

				if (prReorderQueParm && prReorderQueParm->fgIsValid && !fgIsBMC)
					qmProcessPktWithReordering(prAdapter, prCurrSwRfb, prReturnedQue);
				else
					qmHandleRxPackets_AOSP_1;
			} else {
				DBGLOG(QM, TRACE, "Mark NULL the Packet for class error\n");
				RX_INC_CNT(&prAdapter->rRxCtrl, RX_CLASS_ERR_DROP_COUNT);
				prCurrSwRfb->eDst = RX_PKT_DESTINATION_NULL;
				QUEUE_INSERT_TAIL(prReturnedQue, (P_QUE_ENTRY_T) prCurrSwRfb);
			}
		} else {
			P_WLAN_MAC_HEADER_T prWlanMacHeader;

			ASSERT(prCurrSwRfb->pvHeader);

			prWlanMacHeader = (P_WLAN_MAC_HEADER_T) prCurrSwRfb->pvHeader;
			prCurrSwRfb->eDst = RX_PKT_DESTINATION_NULL;

			switch (prWlanMacHeader->u2FrameCtrl & MASK_FRAME_TYPE) {
				/* BAR frame */
			case MAC_FRAME_BLOCK_ACK_REQ:
				qmProcessBarFrame(prAdapter, prCurrSwRfb, prReturnedQue);
				RX_INC_CNT(&prAdapter->rRxCtrl, RX_BAR_DROP_COUNT);
				break;
			default:
				DBGLOG(QM, TRACE, "Mark NULL the Packet for non-interesting type\n");
				RX_INC_CNT(&prAdapter->rRxCtrl, RX_NO_INTEREST_DROP_COUNT);
				QUEUE_INSERT_TAIL(prReturnedQue, (P_QUE_ENTRY_T) prCurrSwRfb);
				break;
			}
		}

	} while (prNextSwRfb);

	/* The returned list of SW_RFBs must end with a NULL pointer */
	if (QUEUE_IS_NOT_EMPTY(prReturnedQue))
		QM_TX_SET_NEXT_MSDU_INFO((P_SW_RFB_T) QUEUE_GET_TAIL(prReturnedQue), NULL);

	return (P_SW_RFB_T) QUEUE_GET_HEAD(prReturnedQue);

#else

	/* DbgPrint("QM: Enter qmHandleRxPackets()\n"); */
	return prSwRfbListHead;

#endif

}

/*----------------------------------------------------------------------------*/
/*!
* \brief Reorder the received packet
*
* \param[in] prSwRfb The RX packet to process
* \param[out] prReturnedQue The queue for indicating packets
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID qmProcessPktWithReordering(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb, OUT P_QUE_T prReturnedQue)
{

	P_STA_RECORD_T prStaRec;
	P_HW_MAC_RX_DESC_T prRxStatus;
	P_RX_BA_ENTRY_T prReorderQueParm;

	/* P_SW_RFB_T prReorderedSwRfb; */
#if CFG_SUPPORT_RX_AMSDU
	UINT_8 u8AmsduSubframeIdx;
	UINT_32 u4SeqNo;
#endif
	DEBUGFUNC("qmProcessPktWithReordering");

	ASSERT(prSwRfb);
	ASSERT(prReturnedQue);
	ASSERT(prSwRfb->prRxStatus);

	/* Incorrect STA_REC index */
	if (prSwRfb->ucStaRecIdx >= CFG_STA_REC_NUM) {
		RX_INC_CNT(&prAdapter->rRxCtrl, RX_NO_STA_DROP_COUNT);
		prSwRfb->eDst = RX_PKT_DESTINATION_NULL;
		QUEUE_INSERT_TAIL(prReturnedQue, (P_QUE_ENTRY_T) prSwRfb);
		DBGLOG(QM, WARN, "Reordering for a NULL STA_REC, ucStaRecIdx = %d\n", prSwRfb->ucStaRecIdx);
		authSendDeauthFrame(prAdapter, NULL, NULL, prSwRfb, REASON_CODE_CLASS_3_ERR,
			(PFN_TX_DONE_HANDLER)NULL);
		/* ASSERT(0); */
		return;
	}

	/* Check whether the STA_REC is activated */
	prStaRec = prSwRfb->prStaRec;
	ASSERT(prStaRec);

	prRxStatus = prSwRfb->prRxStatus;
	/* prSwRfb->eDst = RX_PKT_DESTINATION_HOST; */

#if 0
	if (!(prStaRec->fgIsValid)) {
		/* TODO: (Tehuang) Handle the Host-FW sync issue. */
		prSwRfb->eDst = RX_PKT_DESTINATION_NULL;
		QUEUE_INSERT_TAIL(prReturnedQue, (P_QUE_ENTRY_T) prSwRfb);
		DBGLOG(QM, WARN, "Reordering for an invalid STA_REC\n");
		/* ASSERT(0); */
		return;
	}
#endif

	RX_INC_CNT(&prAdapter->rRxCtrl, RX_DATA_REORDER_TOTAL_COUINT);

	/* Check whether the BA agreement exists */
	prReorderQueParm = ((prStaRec->aprRxReorderParamRefTbl)[prSwRfb->ucTid]);
	if (!prReorderQueParm || !(prReorderQueParm->fgIsValid)) {
		/* TODO: (Tehuang) Handle the Host-FW sync issue. */
		prSwRfb->eDst = RX_PKT_DESTINATION_HOST;
		QUEUE_INSERT_TAIL(prReturnedQue, (P_QUE_ENTRY_T) prSwRfb);
		DBGLOG(QM, TRACE, "Reordering for a NULL ReorderQueParm\n");
		return;
	}

#if CFG_SUPPORT_RX_AMSDU
	/* RX reorder for one MSDU in AMSDU issue */
	/* QUEUE_INITIALIZE(&prSwRfb->rAmsduQue); */

	u8AmsduSubframeIdx = HAL_RX_STATUS_GET_PAYLOAD_FORMAT(prRxStatus);

	/* prMpduSwRfb = prReorderQueParm->prMpduSwRfb; */
	u4SeqNo = (UINT_32)prSwRfb->u2SSN;

	switch (u8AmsduSubframeIdx) {
	case RX_PAYLOAD_FORMAT_FIRST_SUB_AMSDU:
		if (prReorderQueParm->fgAmsduNeedLastFrame) {
			RX_INC_CNT(&prAdapter->rRxCtrl, RX_DATA_AMSDU_MISS_COUNT);
			prReorderQueParm->fgAmsduNeedLastFrame = FALSE;
		}
		RX_INC_CNT(&prAdapter->rRxCtrl, RX_DATA_MSDU_IN_AMSDU_COUNT);
		RX_INC_CNT(&prAdapter->rRxCtrl, RX_DATA_AMSDU_COUNT);
		break;

	case RX_PAYLOAD_FORMAT_MIDDLE_SUB_AMSDU:
		prReorderQueParm->fgAmsduNeedLastFrame = TRUE;
		RX_INC_CNT(&prAdapter->rRxCtrl, RX_DATA_MSDU_IN_AMSDU_COUNT);
		if (prReorderQueParm->u4SeqNo != u4SeqNo) {
			RX_INC_CNT(&prAdapter->rRxCtrl, RX_DATA_AMSDU_MISS_COUNT);
			RX_INC_CNT(&prAdapter->rRxCtrl, RX_DATA_AMSDU_COUNT);
		}
		break;
	case RX_PAYLOAD_FORMAT_LAST_SUB_AMSDU:
		prReorderQueParm->fgAmsduNeedLastFrame = FALSE;
		RX_INC_CNT(&prAdapter->rRxCtrl, RX_DATA_MSDU_IN_AMSDU_COUNT);
		if (prReorderQueParm->u4SeqNo != u4SeqNo) {
			RX_INC_CNT(&prAdapter->rRxCtrl, RX_DATA_AMSDU_MISS_COUNT);
			RX_INC_CNT(&prAdapter->rRxCtrl, RX_DATA_AMSDU_COUNT);
		}
		break;

	case RX_PAYLOAD_FORMAT_MSDU:
		if (prReorderQueParm->fgAmsduNeedLastFrame) {
			RX_INC_CNT(&prAdapter->rRxCtrl, RX_DATA_AMSDU_MISS_COUNT);
			prReorderQueParm->fgAmsduNeedLastFrame = FALSE;
		}
		break;
	default:
		break;
	}

	prReorderQueParm->u4SeqNo = u4SeqNo;
#endif

	RX_DIRECT_REORDER_LOCK(prAdapter, 0);
	/* After resuming, WinStart and WinEnd are obsolete and unsync
	 * with AP's SN. So assign the SN of first packet to WinStart
	 * as "Fall Within" case.
	 */
	if (prReorderQueParm->fgFirstSnToWinStart) {
		DBGLOG(QM, INFO,
		       "[%u] First resumed SN(%u) reset Window{%u,%u}\n",
		       prSwRfb->ucTid, prSwRfb->u2SSN,
		       prReorderQueParm->u2WinStart,
		       prReorderQueParm->u2WinEnd);

		prReorderQueParm->u2WinStart = prSwRfb->u2SSN;
		prReorderQueParm->u2WinEnd =
		    ((prReorderQueParm->u2WinStart) +
		     (prReorderQueParm->u2WinSize) - 1) % MAX_SEQ_NO_COUNT;
		prReorderQueParm->fgFirstSnToWinStart = FALSE;
	}

	/* Insert reorder packet */
	qmInsertReorderPkt(prAdapter, prSwRfb, prReorderQueParm, prReturnedQue);
	RX_DIRECT_REORDER_UNLOCK(prAdapter, 0);
}

VOID qmProcessBarFrame(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb, OUT P_QUE_T prReturnedQue)
{

	P_STA_RECORD_T prStaRec;
	P_HW_MAC_RX_DESC_T prRxStatus;
	P_RX_BA_ENTRY_T prReorderQueParm;
	P_CTRL_BAR_FRAME_T prBarCtrlFrame;

	UINT_32 u4SSN;
	UINT_32 u4WinStart;
	UINT_32 u4WinEnd;

	ASSERT(prSwRfb);
	ASSERT(prReturnedQue);
	ASSERT(prSwRfb->prRxStatus);
	ASSERT(prSwRfb->pvHeader);

	prRxStatus = prSwRfb->prRxStatus;

	prBarCtrlFrame = (P_CTRL_BAR_FRAME_T) prSwRfb->pvHeader;

	prSwRfb->ucTid =
	    (*((PUINT_16) ((PUINT_8) prBarCtrlFrame + CTRL_BAR_BAR_CONTROL_OFFSET))) >> BAR_CONTROL_TID_INFO_OFFSET;
	prSwRfb->u2SSN =
	    (*((PUINT_16) ((PUINT_8) prBarCtrlFrame + CTRL_BAR_BAR_INFORMATION_OFFSET))) >> OFFSET_BAR_SSC_SN;

	prSwRfb->eDst = RX_PKT_DESTINATION_NULL;
	QUEUE_INSERT_TAIL(prReturnedQue, (P_QUE_ENTRY_T) prSwRfb);

	/* Incorrect STA_REC index */
	prSwRfb->ucStaRecIdx = secLookupStaRecIndexFromTA(prAdapter, prBarCtrlFrame->aucSrcAddr);
	if (prSwRfb->ucStaRecIdx >= CFG_STA_REC_NUM) {
		DBGLOG(QM, WARN, "QM: (Warning) BAR for a NULL STA_REC, ucStaRecIdx = %d\n", prSwRfb->ucStaRecIdx);
		/* ASSERT(0); */
		return;
	}

	/* Check whether the STA_REC is activated */
	prSwRfb->prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);
	prStaRec = prSwRfb->prStaRec;
	if (prStaRec == NULL) {
		/* ASSERT(prStaRec); */
		return;
	}
#if 0
	if (!(prStaRec->fgIsValid)) {
		/* TODO: (Tehuang) Handle the Host-FW sync issue. */
		DbgPrint("QM: (Warning) BAR for an invalid STA_REC\n");
		/* ASSERT(0); */
		return;
	}
#endif

	/* Check whether the BA agreement exists */
	prReorderQueParm = prStaRec->aprRxReorderParamRefTbl[prSwRfb->ucTid];
	if (!prReorderQueParm) {
		/* TODO: (Tehuang) Handle the Host-FW sync issue. */
		DBGLOG(QM, WARN, "QM: (Warning) BAR for a NULL ReorderQueParm\n");
		/* ASSERT(0); */
		return;
	}

	RX_DIRECT_REORDER_LOCK(prAdapter, 0);

	u4SSN = (UINT_32) (prSwRfb->u2SSN);
	u4WinStart = (UINT_32) (prReorderQueParm->u2WinStart);
	u4WinEnd = (UINT_32) (prReorderQueParm->u2WinEnd);

	if (qmCompareSnIsLessThan(u4WinStart, u4SSN)) {
		prReorderQueParm->u2WinStart = (UINT_16) u4SSN;
		prReorderQueParm->u2WinEnd =
		    ((prReorderQueParm->u2WinStart) + (prReorderQueParm->u2WinSize) - 1) % MAX_SEQ_NO_COUNT;
#if CFG_SUPPORT_RX_AMSDU
		/* RX reorder for one MSDU in AMSDU issue */
		prReorderQueParm->u8LastAmsduSubIdx = RX_PAYLOAD_FORMAT_MSDU;
#endif
		DBGLOG(QM, TRACE, "QM:(BAR)[%u](%u){%u,%u}\n", prSwRfb->ucTid,
			u4SSN, prReorderQueParm->u2WinStart,
			prReorderQueParm->u2WinEnd);
		qmPopOutDueToFallAhead(prAdapter, prReorderQueParm, prReturnedQue);
	} else {
		DBGLOG(QM, TRACE, "QM:(BAR)(%u)(%u){%u,%u}\n", prSwRfb->ucTid,
			u4SSN, u4WinStart, u4WinEnd);
	}
	RX_DIRECT_REORDER_UNLOCK(prAdapter, 0);
}

VOID qmInsertReorderPkt(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb,
	IN P_RX_BA_ENTRY_T prReorderQueParm, OUT P_QUE_T prReturnedQue)
{
	UINT_32 u4SeqNo;
	UINT_32 u4WinStart;
	UINT_32 u4WinEnd;

	/* Start to reorder packets */
	u4SeqNo = (UINT_32) (prSwRfb->u2SSN);
	u4WinStart = (UINT_32) (prReorderQueParm->u2WinStart);
	u4WinEnd = (UINT_32) (prReorderQueParm->u2WinEnd);

	/* Debug */
	DBGLOG(QM, LOUD, "QM:(R)[%u](%u){%u,%u}\n", prSwRfb->ucTid, u4SeqNo,
		u4WinStart, u4WinEnd);

	/* Case 1: Fall within */
	if			/* 0 - start - sn - end - 4095 */
	    (((u4WinStart <= u4SeqNo) && (u4SeqNo <= u4WinEnd))
	     /* 0 - end - start - sn - 4095 */
	     || ((u4WinEnd < u4WinStart) && (u4WinStart <= u4SeqNo))
	     /* 0 - sn - end - start - 4095 */
	     || ((u4SeqNo <= u4WinEnd) && (u4WinEnd < u4WinStart))) {

		qmInsertFallWithinReorderPkt(prAdapter, prSwRfb, prReorderQueParm, prReturnedQue);

#if QM_RX_WIN_SSN_AUTO_ADVANCING
		if (prReorderQueParm->fgIsWaitingForPktWithSsn) {
			/* Let the first received packet pass the reorder check */
			DBGLOG(QM, LOUD, "QM:(A)[%u](%u){%u,%u}\n",
				prSwRfb->ucTid, u4SeqNo, u4WinStart, u4WinEnd);

			prReorderQueParm->u2WinStart = (UINT_16) u4SeqNo;
			prReorderQueParm->u2WinEnd =
			    ((prReorderQueParm->u2WinStart) + (prReorderQueParm->u2WinSize) - 1) % MAX_SEQ_NO_COUNT;
			prReorderQueParm->fgIsWaitingForPktWithSsn = FALSE;
#if CFG_SUPPORT_RX_AMSDU
			/* RX reorder for one MSDU in AMSDU issue */
			prReorderQueParm->u8LastAmsduSubIdx = RX_PAYLOAD_FORMAT_MSDU;
#endif
		}
#endif

		qmPopOutDueToFallWithin(prAdapter, prReorderQueParm, prReturnedQue);
	}
	/* Case 2: Fall ahead */
	else if
	    /* 0 - start - end - sn - (start+2048) - 4095 */
	    (((u4WinStart < u4WinEnd)
	      && (u4WinEnd < u4SeqNo)
	      && (u4SeqNo < (u4WinStart + HALF_SEQ_NO_COUNT)))
	     /* 0 - sn - (start+2048) - start - end - 4095 */
	     || ((u4SeqNo < u4WinStart)
		 && (u4WinStart < u4WinEnd)
		 && ((u4SeqNo + MAX_SEQ_NO_COUNT) < (u4WinStart + HALF_SEQ_NO_COUNT)))
	     /* 0 - end - sn - (start+2048) - start - 4095 */
	     || ((u4WinEnd < u4SeqNo)
		 && (u4SeqNo < u4WinStart)
		 && ((u4SeqNo + MAX_SEQ_NO_COUNT) < (u4WinStart + HALF_SEQ_NO_COUNT)))) {

		UINT_16 u2Delta, u2BeforeWinEnd;
		UINT_32 u4BeforeCount, u4MissingCount;

#if QM_RX_WIN_SSN_AUTO_ADVANCING
		if (prReorderQueParm->fgIsWaitingForPktWithSsn)
			prReorderQueParm->fgIsWaitingForPktWithSsn = FALSE;
#endif

		qmInsertFallAheadReorderPkt(prAdapter, prSwRfb, prReorderQueParm, prReturnedQue);

		u2BeforeWinEnd = prReorderQueParm->u2WinEnd;

		/* Advance the window after inserting a new tail */
		prReorderQueParm->u2WinEnd = (UINT_16) u4SeqNo;
		prReorderQueParm->u2WinStart =
			(prReorderQueParm->u2WinEnd + MAX_SEQ_NO_COUNT -
			 prReorderQueParm->u2WinSize + 1) % MAX_SEQ_NO_COUNT;
#if CFG_SUPPORT_RX_AMSDU
		/* RX reorder for one MSDU in AMSDU issue */
		prReorderQueParm->u8LastAmsduSubIdx = RX_PAYLOAD_FORMAT_MSDU;
#endif
		u4BeforeCount = prReorderQueParm->rReOrderQue.u4NumElem;
		qmPopOutDueToFallAhead(prAdapter, prReorderQueParm, prReturnedQue);

		if (prReorderQueParm->u2WinEnd >= u2BeforeWinEnd)
			u2Delta = prReorderQueParm->u2WinEnd - u2BeforeWinEnd;
		else
			u2Delta = MAX_SEQ_NO_COUNT - (u2BeforeWinEnd - prReorderQueParm->u2WinEnd);

		u4MissingCount = u2Delta - (u4BeforeCount - prReorderQueParm->rReOrderQue.u4NumElem);

		RX_ADD_CNT(&prAdapter->rRxCtrl, RX_DATA_REORDER_MISS_COUNT, u4MissingCount);
	}
	/* Case 3: Fall behind */
	else {
#if QM_RX_WIN_SSN_AUTO_ADVANCING && QM_RX_INIT_FALL_BEHIND_PASS
		if (prReorderQueParm->fgIsWaitingForPktWithSsn) {
			DBGLOG(QM, LOUD, "QM:(P)[%u](%u){%u,%u}\n",
				prSwRfb->ucTid, u4SeqNo, u4WinStart, u4WinEnd);
			qmPopOutReorderPkt(prAdapter, prSwRfb, prReturnedQue,
				RX_DATA_REORDER_BEHIND_COUNT);
			return;
		}
#endif

		/* An erroneous packet */
		DBGLOG(QM, LOUD, "QM:(D)[%u](%u){%u,%u}\n", prSwRfb->ucTid,
			u4SeqNo, u4WinStart, u4WinEnd);
		prSwRfb->eDst = RX_PKT_DESTINATION_NULL;
		qmPopOutReorderPkt(prAdapter, prSwRfb, prReturnedQue,
			RX_DATA_REORDER_BEHIND_COUNT);
		return;
	}
}

VOID qmInsertFallWithinReorderPkt(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb,
	IN P_RX_BA_ENTRY_T prReorderQueParm, OUT P_QUE_T prReturnedQue)
{
	P_SW_RFB_T prExaminedQueuedSwRfb;
	P_QUE_T prReorderQue;
	P_HW_MAC_RX_DESC_T prRxStatus;
	UINT_8 u8AmsduSubframeIdx; /* RX reorder for one MSDU in AMSDU issue */

	ASSERT(prSwRfb);
	ASSERT(prReorderQueParm);
	ASSERT(prReturnedQue);

	prReorderQue = &(prReorderQueParm->rReOrderQue);
	prExaminedQueuedSwRfb = (P_SW_RFB_T) QUEUE_GET_HEAD(prReorderQue);

	prRxStatus = prSwRfb->prRxStatus;
	u8AmsduSubframeIdx = HAL_RX_STATUS_GET_PAYLOAD_FORMAT(prRxStatus);

	/* There are no packets queued in the Reorder Queue */
	if (prExaminedQueuedSwRfb == NULL) {
		((P_QUE_ENTRY_T) prSwRfb)->prPrev = NULL;
		((P_QUE_ENTRY_T) prSwRfb)->prNext = NULL;
		prReorderQue->prHead = (P_QUE_ENTRY_T) prSwRfb;
		prReorderQue->prTail = (P_QUE_ENTRY_T) prSwRfb;
		prReorderQue->u4NumElem++;
	}

	/* Determine the insert position */
	else {
		do {
			/* Case 1: Terminate. A duplicate packet */
			if ((prExaminedQueuedSwRfb->u2SSN)
				== (prSwRfb->u2SSN)) {
#if CFG_SUPPORT_RX_AMSDU
				/* RX reorder for one MSDU in AMSDU issue */
				/* if middle or last and first is not duplicated, not a duplicat packet */
				if (!prReorderQueParm->fgIsAmsduDuplicated &&
					(u8AmsduSubframeIdx == RX_PAYLOAD_FORMAT_MIDDLE_SUB_AMSDU ||
					u8AmsduSubframeIdx == RX_PAYLOAD_FORMAT_LAST_SUB_AMSDU)) {

					prExaminedQueuedSwRfb =
					(P_SW_RFB_T) (((P_QUE_ENTRY_T) prExaminedQueuedSwRfb)->prNext);
					while (prExaminedQueuedSwRfb &&
					((prExaminedQueuedSwRfb->u2SSN) == (prSwRfb->u2SSN)))
						prExaminedQueuedSwRfb =
						(P_SW_RFB_T) (((P_QUE_ENTRY_T) prExaminedQueuedSwRfb)->prNext);

					break;
				}
				/* if first is duplicated, drop subsequent middle and last frames */
				if (u8AmsduSubframeIdx == RX_PAYLOAD_FORMAT_FIRST_SUB_AMSDU)
					prReorderQueParm->fgIsAmsduDuplicated = TRUE;
#endif
				prSwRfb->eDst = RX_PKT_DESTINATION_NULL;
				qmPopOutReorderPkt(prAdapter, prSwRfb, prReturnedQue, RX_DUPICATE_DROP_COUNT);
				return;
			}

			/* Case 2: Terminate. The insert point is found */
			else if (qmCompareSnIsLessThan((prSwRfb->u2SSN), (prExaminedQueuedSwRfb->u2SSN)))
				break;

			/* Case 3: Insert point not found. Check the next SW_RFB in the Reorder Queue */
			else
				prExaminedQueuedSwRfb =
				(P_SW_RFB_T) (((P_QUE_ENTRY_T) prExaminedQueuedSwRfb)->prNext);
		} while (prExaminedQueuedSwRfb);
#if CFG_SUPPORT_RX_AMSDU
		prReorderQueParm->fgIsAmsduDuplicated = FALSE;
#endif

		/* Update the Reorder Queue Parameters according to the found insert position */
		if (prExaminedQueuedSwRfb == NULL) {
			/* The received packet shall be placed at the tail */
			((P_QUE_ENTRY_T) prSwRfb)->prPrev = prReorderQue->prTail;
			((P_QUE_ENTRY_T) prSwRfb)->prNext = NULL;
			(prReorderQue->prTail)->prNext = (P_QUE_ENTRY_T) (prSwRfb);
			prReorderQue->prTail = (P_QUE_ENTRY_T) (prSwRfb);
		} else {
			((P_QUE_ENTRY_T) prSwRfb)->prPrev = ((P_QUE_ENTRY_T) prExaminedQueuedSwRfb)->prPrev;
			((P_QUE_ENTRY_T) prSwRfb)->prNext = (P_QUE_ENTRY_T) prExaminedQueuedSwRfb;
			if (((P_QUE_ENTRY_T) prExaminedQueuedSwRfb) == (prReorderQue->prHead)) {
				/* The received packet will become the head */
				prReorderQue->prHead = (P_QUE_ENTRY_T) prSwRfb;
			} else {
				(((P_QUE_ENTRY_T) prExaminedQueuedSwRfb)->prPrev)->prNext = (P_QUE_ENTRY_T) prSwRfb;
			}
			((P_QUE_ENTRY_T) prExaminedQueuedSwRfb)->prPrev = (P_QUE_ENTRY_T) prSwRfb;
		}

		prReorderQue->u4NumElem++;
	}

}

VOID qmInsertFallAheadReorderPkt(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb,
	IN P_RX_BA_ENTRY_T prReorderQueParm, OUT P_QUE_T prReturnedQue)
{
	P_QUE_T prReorderQue;

	ASSERT(prSwRfb);
	ASSERT(prReorderQueParm);
	ASSERT(prReturnedQue);
#if CFG_SUPPORT_RX_AMSDU
	/* RX reorder for one MSDU in AMSDU issue */
	prReorderQueParm->fgIsAmsduDuplicated = FALSE;
#endif
	prReorderQue = &(prReorderQueParm->rReOrderQue);
	/* There are no packets queued in the Reorder Queue */
	if (QUEUE_IS_EMPTY(prReorderQue)) {
		((P_QUE_ENTRY_T) prSwRfb)->prPrev = NULL;
		((P_QUE_ENTRY_T) prSwRfb)->prNext = NULL;
		prReorderQue->prHead = (P_QUE_ENTRY_T) prSwRfb;
	} else {
		((P_QUE_ENTRY_T) prSwRfb)->prPrev = prReorderQue->prTail;
		((P_QUE_ENTRY_T) prSwRfb)->prNext = NULL;
		(prReorderQue->prTail)->prNext = (P_QUE_ENTRY_T) (prSwRfb);
	}
	prReorderQue->prTail = (P_QUE_ENTRY_T) prSwRfb;
	prReorderQue->u4NumElem++;
}

VOID qmPopOutReorderPkt(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb, OUT P_QUE_T prReturnedQue,
	IN ENUM_RX_STATISTIC_COUNTER_T eRxCounter)
{
	UINT_32 u4PktCnt = 0;
	/* RX reorder for one MSDU in AMSDU issue */
#if 0
	P_SW_RFB_T prAmsduSwRfb;
#endif

	u4PktCnt++;
	QUEUE_INSERT_TAIL(prReturnedQue, (P_QUE_ENTRY_T)prSwRfb);

#if 0
	u4PktCnt += prSwRfb->rAmsduQue.u4NumElem;
	QUEUE_REMOVE_HEAD(&prSwRfb->rAmsduQue, prAmsduSwRfb, P_SW_RFB_T);
	while (prAmsduSwRfb) {
		/* Update MSDU destination of AMSDU */
		prAmsduSwRfb->eDst = prSwRfb->eDst;
		QUEUE_INSERT_TAIL(prReturnedQue, (P_QUE_ENTRY_T)prAmsduSwRfb);
		QUEUE_REMOVE_HEAD(&prSwRfb->rAmsduQue, prAmsduSwRfb, P_SW_RFB_T);
	}
#endif

	RX_ADD_CNT(&prAdapter->rRxCtrl, eRxCounter, u4PktCnt);
}

VOID qmPopOutDueToFallWithin(IN P_ADAPTER_T prAdapter, IN P_RX_BA_ENTRY_T prReorderQueParm, OUT P_QUE_T prReturnedQue)
{
	P_SW_RFB_T prReorderedSwRfb;
	P_QUE_T prReorderQue;
	BOOLEAN fgDequeuHead, fgMissing;
	OS_SYSTIME rCurrentTime, *prMissTimeout;
	P_HW_MAC_RX_DESC_T prRxStatus;
	UINT_8 fgIsAmsduSubframe;/* RX reorder for one MSDU in AMSDU issue */

	prReorderQue = &(prReorderQueParm->rReOrderQue);

	fgMissing = FALSE;
	rCurrentTime = 0;
	prMissTimeout = &g_arMissTimeout[prReorderQueParm->ucStaRecIdx][prReorderQueParm->ucTid];
	if (*prMissTimeout) {
		fgMissing = TRUE;
		GET_CURRENT_SYSTIME(&rCurrentTime);
	}

	/* Check whether any packet can be indicated to the higher layer */
	while (TRUE) {
		if (QUEUE_IS_EMPTY(prReorderQue))
			break;

		/* Always examine the head packet */
		prReorderedSwRfb = (P_SW_RFB_T) QUEUE_GET_HEAD(prReorderQue);
		fgDequeuHead = FALSE;

		/* RX reorder for one MSDU in AMSDU issue */
		prRxStatus = prReorderedSwRfb->prRxStatus;
		fgIsAmsduSubframe = HAL_RX_STATUS_GET_PAYLOAD_FORMAT(prRxStatus);
#if CFG_SUPPORT_RX_AMSDU
		/* If SN + 1 come and last frame is first or middle, update winstart */
		if ((qmCompareSnIsLessThan((prReorderQueParm->u2WinStart), (prReorderedSwRfb->u2SSN)))
			&& (prReorderQueParm->u4SeqNo != prReorderQueParm->u2WinStart)) {
			if (prReorderQueParm->u8LastAmsduSubIdx == RX_PAYLOAD_FORMAT_FIRST_SUB_AMSDU
				|| prReorderQueParm->u8LastAmsduSubIdx == RX_PAYLOAD_FORMAT_MIDDLE_SUB_AMSDU) {

				prReorderQueParm->u2WinStart =
					(((prReorderQueParm->u2WinStart) + 1) % MAX_SEQ_NO_COUNT);
				prReorderQueParm->u8LastAmsduSubIdx = RX_PAYLOAD_FORMAT_MSDU;
			}
		}
#endif
		/* SN == WinStart, so the head packet shall be indicated (advance the window) */
		if ((prReorderedSwRfb->u2SSN) == (prReorderQueParm->u2WinStart)) {

			fgDequeuHead = TRUE;
			/* RX reorder for one MSDU in AMSDU issue */
			/* if last frame, winstart++. Otherwise, keep winstart */
			if (fgIsAmsduSubframe == RX_PAYLOAD_FORMAT_LAST_SUB_AMSDU
				|| fgIsAmsduSubframe == RX_PAYLOAD_FORMAT_MSDU)
				prReorderQueParm->u2WinStart = (((prReorderedSwRfb->u2SSN) + 1) % MAX_SEQ_NO_COUNT);
#if CFG_SUPPORT_RX_AMSDU
			prReorderQueParm->u8LastAmsduSubIdx = fgIsAmsduSubframe;
#endif
		}
		/* SN > WinStart, break to update WinEnd */
		else {
			/* Start bubble timer */
			if (!prReorderQueParm->fgHasBubble) {
				cnmTimerStartTimer(prAdapter, &(prReorderQueParm->rReorderBubbleTimer),
					QM_RX_BA_ENTRY_MISS_TIMEOUT_MS);
				prReorderQueParm->fgHasBubble = TRUE;
				prReorderQueParm->u2FirstBubbleSn = prReorderQueParm->u2WinStart;

				DBGLOG(QM, TRACE,
					"QM:(Bub Timer) STA[%u] TID[%u] BubSN[%u] Win{%u, %u}\n",
					prReorderQueParm->ucStaRecIdx,
					prReorderedSwRfb->ucTid,
					prReorderQueParm->u2FirstBubbleSn,
					prReorderQueParm->u2WinStart,
					prReorderQueParm->u2WinEnd);
			}

			if (fgMissing && CHECK_FOR_TIMEOUT(rCurrentTime, *prMissTimeout,
				MSEC_TO_SYSTIME(QM_RX_BA_ENTRY_MISS_TIMEOUT_MS))) {

				DBGLOG(QM, TRACE,
					"QM:RX BA Timout Next Tid %u SSN %u\n",
					prReorderQueParm->ucTid,
					prReorderedSwRfb->u2SSN);
				fgDequeuHead = TRUE;
				prReorderQueParm->u2WinStart = (((prReorderedSwRfb->u2SSN) + 1) % MAX_SEQ_NO_COUNT);
#if CFG_SUPPORT_RX_AMSDU
				/* RX reorder for one MSDU in AMSDU issue */
				prReorderQueParm->u8LastAmsduSubIdx = RX_PAYLOAD_FORMAT_MSDU;
#endif
				fgMissing = FALSE;
			} else
				break;
		}

		/* Dequeue the head packet */
		if (fgDequeuHead) {
			if (((P_QUE_ENTRY_T) prReorderedSwRfb)->prNext == NULL) {
				prReorderQue->prHead = NULL;
				prReorderQue->prTail = NULL;
			} else {
				prReorderQue->prHead = ((P_QUE_ENTRY_T) prReorderedSwRfb)->prNext;
				(((P_QUE_ENTRY_T) prReorderedSwRfb)->prNext)->prPrev = NULL;
			}
			prReorderQue->u4NumElem--;
			DBGLOG(QM, LOUD, "QM: [%u] %u (%u)\n",
				prReorderQueParm->ucTid,
				prReorderedSwRfb->u2PacketLen,
				prReorderedSwRfb->u2SSN);
			qmPopOutReorderPkt(prAdapter, prReorderedSwRfb, prReturnedQue, RX_DATA_REORDER_WITHIN_COUNT);
		}
	}

	if (QUEUE_IS_EMPTY(prReorderQue))
		*prMissTimeout = 0;
	else {
		if (fgMissing == FALSE)
			GET_CURRENT_SYSTIME(prMissTimeout);
	}

	/* After WinStart has been determined, update the WinEnd */
	prReorderQueParm->u2WinEnd =
	    (((prReorderQueParm->u2WinStart) + (prReorderQueParm->u2WinSize) - 1) % MAX_SEQ_NO_COUNT);

}

VOID qmPopOutDueToFallAhead(IN P_ADAPTER_T prAdapter, IN P_RX_BA_ENTRY_T prReorderQueParm, OUT P_QUE_T prReturnedQue)
{
	P_SW_RFB_T prReorderedSwRfb;
	P_QUE_T prReorderQue;
	BOOLEAN fgDequeuHead;
	P_HW_MAC_RX_DESC_T prRxStatus;
	UINT_8 fgIsAmsduSubframe;/* RX reorder for one MSDU in AMSDU issue */

	prReorderQue = &(prReorderQueParm->rReOrderQue);

	/* Check whether any packet can be indicated to the higher layer */
	while (TRUE) {
		if (QUEUE_IS_EMPTY(prReorderQue))
			break;

		/* Always examine the head packet */
		prReorderedSwRfb = (P_SW_RFB_T) QUEUE_GET_HEAD(prReorderQue);
		fgDequeuHead = FALSE;

		/* RX reorder for one MSDU in AMSDU issue */
		prRxStatus = prReorderedSwRfb->prRxStatus;
		fgIsAmsduSubframe = HAL_RX_STATUS_GET_PAYLOAD_FORMAT(prRxStatus);
#if CFG_SUPPORT_RX_AMSDU
		/* If SN + 1 come and last frame is first or middle, update winstart */
		if ((qmCompareSnIsLessThan((prReorderQueParm->u2WinStart), (prReorderedSwRfb->u2SSN)))
			&& (prReorderQueParm->u4SeqNo != prReorderQueParm->u2WinStart)) {
			if (prReorderQueParm->u8LastAmsduSubIdx == RX_PAYLOAD_FORMAT_FIRST_SUB_AMSDU
				|| prReorderQueParm->u8LastAmsduSubIdx == RX_PAYLOAD_FORMAT_MIDDLE_SUB_AMSDU) {

				prReorderQueParm->u2WinStart =
					(((prReorderQueParm->u2WinStart) + 1) % MAX_SEQ_NO_COUNT);
				prReorderQueParm->u8LastAmsduSubIdx = RX_PAYLOAD_FORMAT_MSDU;
			}
		}
#endif
		/* SN == WinStart, so the head packet shall be indicated (advance the window) */
		if ((prReorderedSwRfb->u2SSN) == (prReorderQueParm->u2WinStart)) {

			fgDequeuHead = TRUE;
			/* RX reorder for one MSDU in AMSDU issue */
			/* if last frame, winstart++. Otherwise, keep winstart */
			if (fgIsAmsduSubframe == RX_PAYLOAD_FORMAT_LAST_SUB_AMSDU
				|| fgIsAmsduSubframe == RX_PAYLOAD_FORMAT_MSDU)
				prReorderQueParm->u2WinStart = (((prReorderedSwRfb->u2SSN) + 1) % MAX_SEQ_NO_COUNT);
#if CFG_SUPPORT_RX_AMSDU
			prReorderQueParm->u8LastAmsduSubIdx = fgIsAmsduSubframe;
#endif
		}

		/* SN < WinStart, so the head packet shall be indicated (do not advance the window) */
		else if (qmCompareSnIsLessThan((UINT_32)(prReorderedSwRfb->u2SSN),
			(UINT_32)(prReorderQueParm->u2WinStart)))
			fgDequeuHead = TRUE;

		/* SN > WinStart, break to update WinEnd */
		else {
			/* Start bubble timer */
			if (!prReorderQueParm->fgHasBubble) {
				cnmTimerStartTimer(prAdapter, &(prReorderQueParm->rReorderBubbleTimer),
					QM_RX_BA_ENTRY_MISS_TIMEOUT_MS);
				prReorderQueParm->fgHasBubble = TRUE;
				prReorderQueParm->u2FirstBubbleSn = prReorderQueParm->u2WinStart;

				DBGLOG(QM, TRACE,
					"QM:(Bub Timer) STA[%u] TID[%u] BubSN[%u] Win{%u,%u}\n",
					prReorderQueParm->ucStaRecIdx,
					prReorderedSwRfb->ucTid,
					prReorderQueParm->u2FirstBubbleSn,
					prReorderQueParm->u2WinStart,
					prReorderQueParm->u2WinEnd);
			}
			break;
		}

		/* Dequeue the head packet */
		if (fgDequeuHead) {
			if (((P_QUE_ENTRY_T) prReorderedSwRfb)->prNext == NULL) {
				prReorderQue->prHead = NULL;
				prReorderQue->prTail = NULL;
			} else {
				prReorderQue->prHead = ((P_QUE_ENTRY_T) prReorderedSwRfb)->prNext;
				(((P_QUE_ENTRY_T) prReorderedSwRfb)->prNext)->prPrev = NULL;
			}
			prReorderQue->u4NumElem--;
			DBGLOG(QM, TRACE, "QM: [%u] %u (%u)\n",
				prReorderQueParm->ucTid,
				prReorderedSwRfb->u2PacketLen,
				prReorderedSwRfb->u2SSN);

			qmPopOutReorderPkt(prAdapter, prReorderedSwRfb, prReturnedQue, RX_DATA_REORDER_AHEAD_COUNT);
		}
	}

	/* After WinStart has been determined, update the WinEnd */
	prReorderQueParm->u2WinEnd =
	    (((prReorderQueParm->u2WinStart) + (prReorderQueParm->u2WinSize) - 1) % MAX_SEQ_NO_COUNT);

}

VOID qmHandleReorderBubbleTimeout(IN P_ADAPTER_T prAdapter, IN ULONG ulParamPtr)
{
	P_RX_BA_ENTRY_T prReorderQueParm = (P_RX_BA_ENTRY_T) ulParamPtr;
	P_SW_RFB_T prSwRfb = (P_SW_RFB_T) NULL;
	P_WIFI_EVENT_T prEvent = NULL;
	P_EVENT_CHECK_REORDER_BUBBLE_T prCheckReorderEvent;

	KAL_SPIN_LOCK_DECLARATION();

	if (!prReorderQueParm->fgIsValid) {
		DBGLOG(QM, TRACE, "QM:(Bub Check Cancel) STA[%u] TID[%u], No Rx BA entry\n",
		       prReorderQueParm->ucStaRecIdx, prReorderQueParm->ucTid);
		return;
	}

	if (!prReorderQueParm->fgHasBubble) {
		DBGLOG(QM, TRACE,
		       "QM:(Bub Check Cancel) STA[%u] TID[%u], Bubble has been filled\n",
		       prReorderQueParm->ucStaRecIdx, prReorderQueParm->ucTid);
		return;
	}

	DBGLOG(QM, TRACE, "QM:(Bub Timeout) STA[%u] TID[%u] BubSN[%u]\n",
	       prReorderQueParm->ucStaRecIdx, prReorderQueParm->ucTid, prReorderQueParm->u2FirstBubbleSn);

	/* Generate a self-inited event to Rx path */
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);
	QUEUE_REMOVE_HEAD(&prAdapter->rRxCtrl.rFreeSwRfbList, prSwRfb, P_SW_RFB_T);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);

	if (prSwRfb) {
		prEvent = (P_WIFI_EVENT_T) (prSwRfb->pucRecvBuff);
		prEvent->ucEID = EVENT_ID_CHECK_REORDER_BUBBLE;
		prEvent->ucSeqNum = 0;
		prEvent->u2PacketLength = sizeof(WIFI_EVENT_T) + sizeof(EVENT_CHECK_REORDER_BUBBLE_T);

		prCheckReorderEvent = (P_EVENT_CHECK_REORDER_BUBBLE_T) prEvent->aucBuffer;
		prCheckReorderEvent->ucStaRecIdx = prReorderQueParm->ucStaRecIdx;
		prCheckReorderEvent->ucTid = prReorderQueParm->ucTid;

		prSwRfb->ucPacketType = RX_PKT_TYPE_SW_DEFINED;
		prSwRfb->prRxStatus->u2PktTYpe = RXM_RXD_PKT_TYPE_SW_EVENT;

		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
		QUEUE_INSERT_TAIL(&prAdapter->rRxCtrl.rReceivedRfbList, &prSwRfb->rQueEntry);
		RX_INC_CNT(&prAdapter->rRxCtrl, RX_MPDU_TOTAL_COUNT);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);

		DBGLOG(QM, LOUD, "QM:(Bub Check Event Sent) STA[%u] TID[%u]\n",
		       prReorderQueParm->ucStaRecIdx, prReorderQueParm->ucTid);

		nicRxProcessRFBs(prAdapter);

		DBGLOG(QM, LOUD, "QM:(Bub Check Event Handled) STA[%u] TID[%u]\n",
		       prReorderQueParm->ucStaRecIdx, prReorderQueParm->ucTid);
	} else {
		DBGLOG(QM, TRACE,
		       "QM:(Bub Check Cancel) STA[%u] TID[%u], Bub check event alloc failed\n",
		       prReorderQueParm->ucStaRecIdx, prReorderQueParm->ucTid);

		cnmTimerStartTimer(prAdapter, &(prReorderQueParm->rReorderBubbleTimer), QM_RX_BA_ENTRY_MISS_TIMEOUT_MS);

		DBGLOG(QM, TRACE,
			"QM:(Bub Timer Restart) STA[%u] TID[%u] BubSN[%u] Win{%u,%u}\n",
		       prReorderQueParm->ucStaRecIdx, prReorderQueParm->ucTid,
		       prReorderQueParm->u2FirstBubbleSn,
		       prReorderQueParm->u2WinStart,
		       prReorderQueParm->u2WinEnd);
	}

}

VOID qmHandleEventCheckReorderBubble(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent)
{
	P_EVENT_CHECK_REORDER_BUBBLE_T prCheckReorderEvent;
	P_RX_BA_ENTRY_T prReorderQueParm;
	P_QUE_T prReorderQue;
	QUE_T rReturnedQue;
	P_QUE_T prReturnedQue = &rReturnedQue;
	P_SW_RFB_T prReorderedSwRfb, prSwRfb;
	OS_SYSTIME *prMissTimeout;

	prCheckReorderEvent = (P_EVENT_CHECK_REORDER_BUBBLE_T) (prEvent->aucBuffer);

	QUEUE_INITIALIZE(prReturnedQue);

	/* Get target Rx BA entry */
	prReorderQueParm = qmLookupRxBaEntry(prAdapter, prCheckReorderEvent->ucStaRecIdx, prCheckReorderEvent->ucTid);

	/* Sanity Check */
	if (!prReorderQueParm) {
		DBGLOG(QM, TRACE, "QM:(Bub Check Cancel) STA[%u] TID[%u], No Rx BA entry\n",
		       prCheckReorderEvent->ucStaRecIdx, prCheckReorderEvent->ucTid);
		return;
	}

	if (!prReorderQueParm->fgIsValid) {
		DBGLOG(QM, TRACE, "QM:(Bub Check Cancel) STA[%u] TID[%u], No Rx BA entry\n",
		       prReorderQueParm->ucStaRecIdx, prReorderQueParm->ucTid);
		return;
	}

	if (!prReorderQueParm->fgHasBubble) {
		DBGLOG(QM, TRACE,
		       "QM:(Bub Check Cancel) STA[%u] TID[%u], Bubble has been filled\n",
		       prReorderQueParm->ucStaRecIdx, prReorderQueParm->ucTid);
		return;
	}

	prReorderQue = &(prReorderQueParm->rReOrderQue);

	RX_DIRECT_REORDER_LOCK(prAdapter, 0);

	if (QUEUE_IS_EMPTY(prReorderQue)) {
		prReorderQueParm->fgHasBubble = FALSE;

		DBGLOG(QM, TRACE,
		       "QM:(Bub Check Cancel) STA[%u] TID[%u], Bubble has been filled\n",
		       prReorderQueParm->ucStaRecIdx, prReorderQueParm->ucTid);
		RX_DIRECT_REORDER_UNLOCK(prAdapter, 0);
		return;
	}

	DBGLOG(QM, TRACE, "QM:(Bub Check Event Got) STA[%u] TID[%u]\n",
	       prReorderQueParm->ucStaRecIdx, prReorderQueParm->ucTid);

	/* Expected bubble timeout => pop out packets before win_end */
	if (prReorderQueParm->u2FirstBubbleSn == prReorderQueParm->u2WinStart) {
		prReorderedSwRfb = (P_SW_RFB_T)QUEUE_GET_TAIL(prReorderQue);

		prReorderQueParm->u2WinStart = prReorderedSwRfb->u2SSN + 1;
		if (prReorderQueParm->u2WinStart >= MAX_SEQ_NO_COUNT)
			prReorderQueParm->u2WinStart %= MAX_SEQ_NO_COUNT;

		prReorderQueParm->u2WinEnd =
		    ((prReorderQueParm->u2WinStart) + (prReorderQueParm->u2WinSize) - 1) % MAX_SEQ_NO_COUNT;
#if CFG_SUPPORT_RX_AMSDU
		prReorderQueParm->u8LastAmsduSubIdx = RX_PAYLOAD_FORMAT_MSDU;
#endif
		qmPopOutDueToFallAhead(prAdapter, prReorderQueParm, prReturnedQue);

		DBGLOG(QM, TRACE,
			"QM:(Bub Flush) STA[%u] TID[%u] BubSN[%u] Win{%u,%u}\n",
			prReorderQueParm->ucStaRecIdx, prReorderQueParm->ucTid,
			prReorderQueParm->u2FirstBubbleSn,
			prReorderQueParm->u2WinStart,
			prReorderQueParm->u2WinEnd);

		prReorderQueParm->fgHasBubble = FALSE;
		RX_DIRECT_REORDER_UNLOCK(prAdapter, 0);

		/* process prReturnedQue after unlock prReturnedQue */
		if (QUEUE_IS_NOT_EMPTY(prReturnedQue)) {
			P_QUE_ENTRY_T prTail = QUEUE_GET_TAIL(prReturnedQue);

			QM_RX_SET_NEXT_SW_RFB((P_SW_RFB_T)prTail, NULL);

			prSwRfb = (P_SW_RFB_T) QUEUE_GET_HEAD(prReturnedQue);
			while (prSwRfb) {
				DBGLOG(QM, TRACE,
				       "QM:(Bub Flush) STA[%u] TID[%u] Pop Out SN[%u]\n",
				       prReorderQueParm->ucStaRecIdx, prReorderQueParm->ucTid, prSwRfb->u2SSN);

				prSwRfb = (P_SW_RFB_T) QUEUE_GET_NEXT_ENTRY((P_QUE_ENTRY_T) prSwRfb);
			}

			wlanProcessQueuedSwRfb(prAdapter, (P_SW_RFB_T) QUEUE_GET_HEAD(prReturnedQue));
		} else {
			DBGLOG(QM, TRACE, "QM:(Bub Flush) STA[%u] TID[%u] Pop Out 0 packet\n",
			       prReorderQueParm->ucStaRecIdx, prReorderQueParm->ucTid);
		}
	}
	/* First bubble has been filled but others exist */
	else {
		prReorderQueParm->u2FirstBubbleSn = prReorderQueParm->u2WinStart;
		DBGLOG(QM, TRACE,
			"QM:(Bub Timer) STA[%u] TID[%u] BubSN[%u] Win{%u,%u}\n",
			prReorderQueParm->ucStaRecIdx, prReorderQueParm->ucTid,
			prReorderQueParm->u2FirstBubbleSn,
			prReorderQueParm->u2WinStart,
			prReorderQueParm->u2WinEnd);
		RX_DIRECT_REORDER_UNLOCK(prAdapter, 0);

		cnmTimerStartTimer(prAdapter,
			&(prReorderQueParm->rReorderBubbleTimer),
			QM_RX_BA_ENTRY_MISS_TIMEOUT_MS);
	}

	prMissTimeout = &g_arMissTimeout[prReorderQueParm->ucStaRecIdx][prReorderQueParm->ucTid];
	if (QUEUE_IS_EMPTY(prReorderQue)) {
		DBGLOG(QM, TRACE, "QM:(Bub Check) Reset prMissTimeout to zero\n");
		*prMissTimeout = 0;
	} else {
		DBGLOG(QM, TRACE, "QM:(Bub Check) Reset prMissTimeout to current time\n");
		GET_CURRENT_SYSTIME(prMissTimeout);
	}
}

BOOLEAN qmCompareSnIsLessThan(IN UINT_32 u4SnLess, IN UINT_32 u4SnGreater)
{
	/* 0 <--->  SnLess   <--(gap>2048)--> SnGreater : SnLess > SnGreater */
	if ((u4SnLess + HALF_SEQ_NO_COUNT) <= u4SnGreater)	/* Shall be <= */
		return FALSE;

	/* 0 <---> SnGreater <--(gap>2048)--> SnLess    : SnLess < SnGreater */
	else if ((u4SnGreater + HALF_SEQ_NO_COUNT) < u4SnLess)
		return TRUE;

	/* 0 <---> SnGreater <--(gap<2048)--> SnLess    : SnLess > SnGreater */
	/* 0 <--->  SnLess   <--(gap<2048)--> SnGreater : SnLess < SnGreater */
	else
		return u4SnLess < u4SnGreater;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Handle Mailbox RX messages
*
* \param[in] prMailboxRxMsg The received Mailbox message from the FW
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID qmHandleMailboxRxMessage(IN MAILBOX_MSG_T prMailboxRxMsg)
{
	/* DbgPrint("QM: Enter qmHandleMailboxRxMessage()\n"); */
	/* TODO */
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Handle ADD RX BA Event from the FW
*
* \param[in] prAdapter Adapter pointer
* \param[in] prEvent The event packet from the FW
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID qmHandleEventRxAddBa(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent)
{
	P_EVENT_RX_ADDBA_T prEventRxAddBa;
	P_STA_RECORD_T prStaRec;
	UINT_32 u4Tid;
	UINT_32 u4WinSize;

	DBGLOG(QM, INFO, "QM:Event +RxBa\n");

	prEventRxAddBa = (P_EVENT_RX_ADDBA_T) (prEvent->aucBuffer);
	prStaRec = QM_GET_STA_REC_PTR_FROM_INDEX(prAdapter, prEventRxAddBa->ucStaRecIdx);

	if (!prStaRec) {
		/* Invalid STA_REC index, discard the event packet */
		/* ASSERT(0); */
		DBGLOG(QM, INFO, "QM: (Warning) RX ADDBA Event for a NULL STA_REC\n");
		return;
	}
#if 0
	if (!(prStaRec->fgIsValid)) {
		/* TODO: (Tehuang) Handle the Host-FW synchronization issue */
		DBGLOG(QM, WARN, "QM: (Warning) RX ADDBA Event for an invalid STA_REC\n");
		/* ASSERT(0); */
		/* return; */
	}
#endif

	u4Tid = (((prEventRxAddBa->u2BAParameterSet) & BA_PARAM_SET_TID_MASK)
		 >> BA_PARAM_SET_TID_MASK_OFFSET);

	u4WinSize = (((prEventRxAddBa->u2BAParameterSet) & BA_PARAM_SET_BUFFER_SIZE_MASK)
		     >> BA_PARAM_SET_BUFFER_SIZE_MASK_OFFSET);

	if (!qmAddRxBaEntry(prAdapter,
			    prStaRec->ucIndex,
			    (UINT_8) u4Tid,
			    (prEventRxAddBa->u2BAStartSeqCtrl >> OFFSET_BAR_SSC_SN), (UINT_16) u4WinSize)) {

		/* FW shall ensure the availabiilty of the free-to-use BA entry */
		DBGLOG(QM, ERROR, "QM: (Error) qmAddRxBaEntry() failure\n");
		ASSERT(0);
	}

}

/*----------------------------------------------------------------------------*/
/*!
* \brief Handle DEL RX BA Event from the FW
*
* \param[in] prAdapter Adapter pointer
* \param[in] prEvent The event packet from the FW
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID qmHandleEventRxDelBa(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent)
{
	P_EVENT_RX_DELBA_T prEventRxDelBa;
	P_STA_RECORD_T prStaRec;

	/* DbgPrint("QM:Event -RxBa\n"); */

	prEventRxDelBa = (P_EVENT_RX_DELBA_T) (prEvent->aucBuffer);
	prStaRec = QM_GET_STA_REC_PTR_FROM_INDEX(prAdapter, prEventRxDelBa->ucStaRecIdx);

	if (!prStaRec)
		/* Invalid STA_REC index, discard the event packet */
		/* ASSERT(0); */
		return;
#if 0
	if (!(prStaRec->fgIsValid))
		/* TODO: (Tehuang) Handle the Host-FW synchronization issue */
		/* ASSERT(0); */
		return;
#endif

	qmDelRxBaEntry(prAdapter, prStaRec->ucIndex, prEventRxDelBa->ucTid, TRUE);

}

P_RX_BA_ENTRY_T qmLookupRxBaEntry(IN P_ADAPTER_T prAdapter, UINT_8 ucStaRecIdx, UINT_8 ucTid)
{
	int i;
	P_QUE_MGT_T prQM = &prAdapter->rQM;

	/* DbgPrint("QM: Enter qmLookupRxBaEntry()\n"); */

	for (i = 0; i < CFG_NUM_OF_RX_BA_AGREEMENTS; i++) {
		if (prQM->arRxBaTable[i].fgIsValid) {
			if ((prQM->arRxBaTable[i].ucStaRecIdx == ucStaRecIdx) && (prQM->arRxBaTable[i].ucTid == ucTid))
				return &prQM->arRxBaTable[i];
		}
	}
	return NULL;
}

BOOL
qmAddRxBaEntry(IN P_ADAPTER_T prAdapter,
	       IN UINT_8 ucStaRecIdx, IN UINT_8 ucTid, IN UINT_16 u2WinStart, IN UINT_16 u2WinSize)
{
	int i;
	P_RX_BA_ENTRY_T prRxBaEntry = NULL;
	P_STA_RECORD_T prStaRec;
	P_QUE_MGT_T prQM = &prAdapter->rQM;

	ASSERT(ucStaRecIdx < CFG_STA_REC_NUM);

	if (ucStaRecIdx >= CFG_STA_REC_NUM) {
		/* Invalid STA_REC index, discard the event packet */
		DBGLOG(QM, WARN, "QM: (WARNING) RX ADDBA Event for a invalid ucStaRecIdx = %d\n", ucStaRecIdx);
		return FALSE;
	}

	prStaRec = &prAdapter->arStaRec[ucStaRecIdx];
	ASSERT(prStaRec);

	/* if(!(prStaRec->fgIsValid)){ */
	/* DbgPrint("QM: (WARNING) Invalid STA when adding an RX BA\n"); */
	/* return FALSE; */
	/* } */

	/* 4 <1> Delete before adding */
	/* Remove the BA entry for the same (STA, TID) tuple if it exists */
	if (qmLookupRxBaEntry(prAdapter, ucStaRecIdx, ucTid))
		qmDelRxBaEntry(prAdapter, ucStaRecIdx, ucTid, TRUE);	/* prQM->ucRxBaCount-- */
	/* 4 <2> Add a new BA entry */
	/* No available entry to store the BA agreement info. Retrun FALSE. */
	if (prQM->ucRxBaCount >= CFG_NUM_OF_RX_BA_AGREEMENTS) {
		DBGLOG(QM, ERROR, "QM: **failure** (limited resource, ucRxBaCount=%d)\n", prQM->ucRxBaCount);
		return FALSE;
	}
		/* Find the free-to-use BA entry */
		for (i = 0; i < CFG_NUM_OF_RX_BA_AGREEMENTS; i++) {
			if (!prQM->arRxBaTable[i].fgIsValid) {
				prRxBaEntry = &(prQM->arRxBaTable[i]);
				prQM->ucRxBaCount++;
				DBGLOG(QM, LOUD, "QM: ucRxBaCount=%d\n", prQM->ucRxBaCount);
				break;
			}
		}

		/* If a free-to-use entry is found, configure it and associate it with the STA_REC */
		u2WinSize += CFG_RX_BA_INC_SIZE;
		if (prRxBaEntry) {
			prRxBaEntry->ucStaRecIdx = ucStaRecIdx;
			prRxBaEntry->ucTid = ucTid;
			prRxBaEntry->u2WinStart = u2WinStart;
			prRxBaEntry->u2WinSize = u2WinSize;
			prRxBaEntry->u2WinEnd = ((u2WinStart + u2WinSize - 1) % MAX_SEQ_NO_COUNT);
#if CFG_SUPPORT_RX_AMSDU
			/* RX reorder for one MSDU in AMSDU issue */
			prRxBaEntry->u8LastAmsduSubIdx = RX_PAYLOAD_FORMAT_MSDU;
			prRxBaEntry->fgAmsduNeedLastFrame = FALSE;
			prRxBaEntry->fgIsAmsduDuplicated = FALSE;
#endif
			prRxBaEntry->fgIsValid = TRUE;
			prRxBaEntry->fgIsWaitingForPktWithSsn = TRUE;
			prRxBaEntry->fgHasBubble = FALSE;

			g_arMissTimeout[ucStaRecIdx][ucTid] = 0;

			DBGLOG(QM, INFO,
			       "QM: +RxBA(STA=%u TID=%u WinStart=%u WinEnd=%u WinSize=%u)\n",
			       ucStaRecIdx, ucTid, prRxBaEntry->u2WinStart,
			       prRxBaEntry->u2WinEnd, prRxBaEntry->u2WinSize);

			/* Update the BA entry reference table for per-packet lookup */
			prStaRec->aprRxReorderParamRefTbl[ucTid] = prRxBaEntry;
		} else {
			/* This shall not happen because FW should keep track of the usage of RX BA entries */
			DBGLOG(QM, ERROR, "QM: **AddBA Error** (ucRxBaCount=%d)\n", prQM->ucRxBaCount);
			return FALSE;
		}


	return TRUE;
}

VOID qmDelRxBaEntry(IN P_ADAPTER_T prAdapter, IN UINT_8 ucStaRecIdx, IN UINT_8 ucTid, IN BOOLEAN fgFlushToHost)
{
	P_RX_BA_ENTRY_T prRxBaEntry;
	P_STA_RECORD_T prStaRec;
	P_SW_RFB_T prFlushedPacketList = NULL;
	P_QUE_MGT_T prQM = &prAdapter->rQM;

	ASSERT(ucStaRecIdx < CFG_STA_REC_NUM);

	prStaRec = &prAdapter->arStaRec[ucStaRecIdx];
	ASSERT(prStaRec);

#if 0
	if (!(prStaRec->fgIsValid)) {
		DbgPrint("QM: (WARNING) Invalid STA when deleting an RX BA\n");
		return;
	}
#endif

	/* Remove the BA entry for the same (STA, TID) tuple if it exists */
	prRxBaEntry = prStaRec->aprRxReorderParamRefTbl[ucTid];

	if (prRxBaEntry) {

		prFlushedPacketList = qmFlushStaRxQueue(prAdapter, ucStaRecIdx, ucTid);

		if (prFlushedPacketList) {

			if (fgFlushToHost) {
				wlanProcessQueuedSwRfb(prAdapter, prFlushedPacketList);
			} else {

				P_SW_RFB_T prSwRfb;
				P_SW_RFB_T prNextSwRfb;

				prSwRfb = prFlushedPacketList;

				do {
					prNextSwRfb = (P_SW_RFB_T) QUEUE_GET_NEXT_ENTRY((P_QUE_ENTRY_T)
											prSwRfb);
					nicRxReturnRFB(prAdapter, prSwRfb);
					prSwRfb = prNextSwRfb;
				} while (prSwRfb);

			}

		}

		if (prRxBaEntry->fgHasBubble) {
			DBGLOG(QM, TRACE, "QM:(Bub Check Cancel) STA[%u] TID[%u], DELBA\n",
			       prRxBaEntry->ucStaRecIdx, prRxBaEntry->ucTid);

			cnmTimerStopTimer(prAdapter, &prRxBaEntry->rReorderBubbleTimer);
			prRxBaEntry->fgHasBubble = FALSE;
		}
#if ((QM_TEST_MODE == 0) && (QM_TEST_STA_REC_DEACTIVATION == 0))
		/* Update RX BA entry state. Note that RX queue flush is not done here */
		prRxBaEntry->fgIsValid = FALSE;
		prQM->ucRxBaCount--;

		/* Debug */
#if 0
		DbgPrint("QM: ucRxBaCount=%d\n", prQM->ucRxBaCount);
#endif

		/* Update STA RX BA table */
		prStaRec->aprRxReorderParamRefTbl[ucTid] = NULL;
#endif

		DBGLOG(QM, INFO, "QM: -RxBA(STA=%d,TID=%d)\n", ucStaRecIdx, ucTid);

	}

	/* Debug */
#if CFG_HIF_RX_STARVATION_WARNING
	{
		P_RX_CTRL_T prRxCtrl;

		prRxCtrl = &prAdapter->rRxCtrl;
		DBGLOG(QM, TRACE,
		       "QM: (RX DEBUG) Enqueued: %d / Dequeued: %d\n", prRxCtrl->u4QueuedCnt, prRxCtrl->u4DequeuedCnt);
	}
#endif
}

VOID mqmParseAssocReqWmmIe(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucIE, IN P_STA_RECORD_T prStaRec)
{
	P_IE_WMM_INFO_T prIeWmmInfo;
	UINT_8 ucQosInfo;
	UINT_8 ucQosInfoAC;
	UINT_8 ucBmpAC;
	UINT_8 aucWfaOui[] = VENDOR_OUI_WFA;

	if ((WMM_IE_OUI_TYPE(pucIE) == VENDOR_OUI_TYPE_WMM) && (!kalMemCmp(WMM_IE_OUI(pucIE), aucWfaOui, 3))) {

		switch (WMM_IE_OUI_SUBTYPE(pucIE)) {
		case VENDOR_OUI_SUBTYPE_WMM_INFO:
			if (IE_LEN(pucIE) != 7)
				break;	/* WMM Info IE with a wrong length */

			prStaRec->fgIsQoS = TRUE;
			prStaRec->fgIsWmmSupported = TRUE;

			prIeWmmInfo = (P_IE_WMM_INFO_T) pucIE;
			ucQosInfo = prIeWmmInfo->ucQosInfo;
			ucQosInfoAC = ucQosInfo & BITS(0, 3);

			if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucUapsd))
				prStaRec->fgIsUapsdSupported = (ucQosInfoAC) ? TRUE : FALSE;
			else
				prStaRec->fgIsUapsdSupported = FALSE;

			ucBmpAC = 0;

			if (ucQosInfoAC & WMM_QOS_INFO_VO_UAPSD)
				ucBmpAC |= BIT(ACI_VO);

			if (ucQosInfoAC & WMM_QOS_INFO_VI_UAPSD)
				ucBmpAC |= BIT(ACI_VI);

			if (ucQosInfoAC & WMM_QOS_INFO_BE_UAPSD)
				ucBmpAC |= BIT(ACI_BE);

			if (ucQosInfoAC & WMM_QOS_INFO_BK_UAPSD)
				ucBmpAC |= BIT(ACI_BK);
			prStaRec->ucBmpTriggerAC = prStaRec->ucBmpDeliveryAC = ucBmpAC;
			prStaRec->ucUapsdSp = (ucQosInfo & WMM_QOS_INFO_MAX_SP_LEN_MASK) >> 5;
			break;

		default:
			/* Other WMM QoS IEs. Ignore any */
			break;
		}
	}
}

/*----------------------------------------------------------------------------*/
/*!
* \brief To process WMM related IEs in ASSOC_RSP
*
* \param[in] prAdapter Adapter pointer
* \param[in] prSwRfb            The received frame
* \param[in] pucIE              The pointer to the first IE in the frame
* \param[in] u2IELength         The total length of IEs in the frame
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID mqmProcessAssocReq(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb, IN PUINT_8 pucIE, IN UINT_16 u2IELength)
{
	P_STA_RECORD_T prStaRec;
	UINT_16 u2Offset;
	PUINT_8 pucIEStart;
	UINT_32 u4Flags;

	DEBUGFUNC("mqmProcessAssocReq");

	ASSERT(prSwRfb);
	ASSERT(pucIE);

	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);
	ASSERT(prStaRec);

	if (prStaRec == NULL)
		return;

	prStaRec->fgIsQoS = FALSE;
	prStaRec->fgIsWmmSupported = prStaRec->fgIsUapsdSupported = FALSE;

	pucIEStart = pucIE;

	/* If the device does not support QoS or if WMM is not supported by the peer, exit. */
	if (IS_FEATURE_DISABLED(prAdapter->rWifiVar.ucQoS))
		return;

	/* Determine whether QoS is enabled with the association */
	else {
		prStaRec->u4Flags = 0;
		IE_FOR_EACH(pucIE, u2IELength, u2Offset) {
			switch (IE_ID(pucIE)) {
			case ELEM_ID_VENDOR:
				mqmParseAssocReqWmmIe(prAdapter, pucIE, prStaRec);

#if CFG_SUPPORT_MTK_SYNERGY
				if (rlmParseCheckMTKOuiIE(prAdapter, pucIE, &u4Flags))
					prStaRec->u4Flags = u4Flags;
#endif

				break;

			case ELEM_ID_HT_CAP:
				/* Some client won't put the WMM IE if client is 802.11n */
				if (IE_LEN(pucIE) == (sizeof(IE_HT_CAP_T) - 2))
					prStaRec->fgIsQoS = TRUE;
				break;
			default:
				break;
			}
		}

		DBGLOG(QM, TRACE, "MQM: Assoc_Req Parsing (QoS Enabled=%d)\n", prStaRec->fgIsQoS);

	}
}

VOID mqmParseAssocRspWmmIe(IN PUINT_8 pucIE, IN P_STA_RECORD_T prStaRec)
{
	UINT_8 aucWfaOui[] = VENDOR_OUI_WFA;

	if ((WMM_IE_OUI_TYPE(pucIE) == VENDOR_OUI_TYPE_WMM) && (!kalMemCmp(WMM_IE_OUI(pucIE), aucWfaOui, 3))) {

		switch (WMM_IE_OUI_SUBTYPE(pucIE)) {
		case VENDOR_OUI_SUBTYPE_WMM_PARAM:
			if (IE_LEN(pucIE) != 24)
				break;	/* WMM Info IE with a wrong length */
			prStaRec->fgIsQoS = TRUE;
			break;

		case VENDOR_OUI_SUBTYPE_WMM_INFO:
			if (IE_LEN(pucIE) != 7)
				break;	/* WMM Info IE with a wrong length */
			prStaRec->fgIsQoS = TRUE;
			break;

		default:
			/* Other WMM QoS IEs. Ignore any */
			break;
		}
	}
}

/*----------------------------------------------------------------------------*/
/*!
* \brief To process WMM related IEs in ASSOC_RSP
*
* \param[in] prAdapter Adapter pointer
* \param[in] prSwRfb            The received frame
* \param[in] pucIE              The pointer to the first IE in the frame
* \param[in] u2IELength         The total length of IEs in the frame
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID mqmProcessAssocRsp(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb, IN PUINT_8 pucIE, IN UINT_16 u2IELength)
{
	P_STA_RECORD_T prStaRec;
	UINT_16 u2Offset;
	PUINT_8 pucIEStart;
	UINT_32 u4Flags;

	DEBUGFUNC("mqmProcessAssocRsp");

	ASSERT(prSwRfb);
	ASSERT(pucIE);

	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);
	ASSERT(prStaRec);

	if (prStaRec == NULL)
		return;

	prStaRec->fgIsQoS = FALSE;

	pucIEStart = pucIE;

	DBGLOG(QM, TRACE, "QM: (fgIsWmmSupported=%d, fgSupportQoS=%d)\n",
	       prStaRec->fgIsWmmSupported, prAdapter->rWifiVar.ucQoS);

	/* If the device does not support QoS or if WMM is not supported by the peer, exit. */
	/* if((!prAdapter->rWifiVar.fgSupportQoS) || (!prStaRec->fgIsWmmSupported)) */
	if (IS_FEATURE_DISABLED(prAdapter->rWifiVar.ucQoS))
		return;

	/* Determine whether QoS is enabled with the association */
	else {
		prStaRec->u4Flags = 0;
		IE_FOR_EACH(pucIE, u2IELength, u2Offset) {
			switch (IE_ID(pucIE)) {
			case ELEM_ID_VENDOR:
				/* Process WMM related IE */
				mqmParseAssocRspWmmIe(pucIE, prStaRec);

#if CFG_SUPPORT_MTK_SYNERGY
				if (rlmParseCheckMTKOuiIE(prAdapter, pucIE, &u4Flags))
					prStaRec->u4Flags = u4Flags;
#endif

				break;

			case ELEM_ID_HT_CAP:
				/* Some AP won't put the WMM IE if client is 802.11n */
				if (IE_LEN(pucIE) == (sizeof(IE_HT_CAP_T) - 2))
					prStaRec->fgIsQoS = TRUE;
				break;
			default:
				break;
			}
		}

		/* Parse AC parameters and write to HW CRs */
		if ((prStaRec->fgIsQoS) && (prStaRec->eStaType == STA_TYPE_LEGACY_AP))
			mqmParseEdcaParameters(prAdapter, prSwRfb, pucIEStart, u2IELength, TRUE);

		DBGLOG(QM, TRACE, "MQM: Assoc_Rsp Parsing (QoS Enabled=%d)\n", prStaRec->fgIsQoS);
		if (prStaRec->fgIsWmmSupported)
			nicQmUpdateWmmParms(prAdapter, prStaRec->ucBssIndex);
	}
}

/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID mqmProcessBcn(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb, IN PUINT_8 pucIE, IN UINT_16 u2IELength)
{
	P_BSS_INFO_T prBssInfo;
	BOOLEAN fgNewParameter;
	UINT_8 i;

	ASSERT(prAdapter);
	ASSERT(prSwRfb);
	ASSERT(pucIE);

	DBGLOG(QM, TRACE, "Enter %s\n", __func__);

	fgNewParameter = FALSE;

	for (i = 0; i < BSS_INFO_NUM; i++) {
		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, i);

		if (IS_BSS_ACTIVE(prBssInfo)) {
			if (prBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE &&
			    prBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED) {
				/* P2P client or AIS infra STA */
				if (EQUAL_MAC_ADDR(prBssInfo->aucBSSID, ((P_WLAN_MAC_MGMT_HEADER_T)
									 (prSwRfb->pvHeader))->aucBSSID)) {

					fgNewParameter = mqmParseEdcaParameters(prAdapter,
										prSwRfb, pucIE, u2IELength, FALSE);
				}
			}

			/* Appy new parameters if necessary */
			if (fgNewParameter) {
				/* DBGLOG(QM, INFO, ("Update EDCA parameter for BSS[%u]\n", prBssInfo->ucBssIndex)); */
				nicQmUpdateWmmParms(prAdapter, prBssInfo->ucBssIndex);
				fgNewParameter = FALSE;
			}
		}		/* end of IS_BSS_ACTIVE() */
	}
}

BOOLEAN mqmUpdateEdcaParameters(IN P_BSS_INFO_T prBssInfo, IN PUINT_8 pucIE, IN BOOLEAN fgForceOverride)
{
	P_AC_QUE_PARMS_T prAcQueParams;
	P_IE_WMM_PARAM_T prIeWmmParam;
	ENUM_WMM_ACI_T eAci;
	BOOLEAN fgNewParameter = FALSE;

	do {
		if (IE_LEN(pucIE) != 24)
			break;	/* WMM Param IE with a wrong length */

		prIeWmmParam = (P_IE_WMM_PARAM_T) pucIE;

		/* Check the Parameter Set Count to determine whether EDCA parameters have been changed */
		if (!fgForceOverride) {
			if (mqmCompareEdcaParameters(prIeWmmParam, prBssInfo)) {
				fgNewParameter = FALSE;
				break;
			}
		}

		fgNewParameter = TRUE;
		/* Update Parameter Set Count */
		prBssInfo->ucWmmParamSetCount = (prIeWmmParam->ucQosInfo & WMM_QOS_INFO_PARAM_SET_CNT);
		/* Update EDCA parameters */
		for (eAci = 0; eAci < WMM_AC_INDEX_NUM; eAci++) {
			prAcQueParams = &prBssInfo->arACQueParms[eAci];
			mqmFillAcQueParam(prIeWmmParam, eAci, prAcQueParams);
			DBGLOG(QM, INFO,
			       "BSS[%u]: eAci[%d] ACM[%d] Aifsn[%d] CWmin/max[%d/%d] TxopLimit[%d] NewParameter[%d]\n",
			       prBssInfo->ucBssIndex, eAci, prAcQueParams->ucIsACMSet,
			       prAcQueParams->u2Aifsn, prAcQueParams->u2CWmin, prAcQueParams->u2CWmax,
			       prAcQueParams->u2TxopLimit, fgNewParameter);
		}
	} while (FALSE);

	return fgNewParameter;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief To parse WMM Parameter IE (in BCN or Assoc_Rsp)
*
* \param[in] prAdapter          Adapter pointer
* \param[in] prSwRfb            The received frame
* \param[in] pucIE              The pointer to the first IE in the frame
* \param[in] u2IELength         The total length of IEs in the frame
* \param[in] fgForceOverride    TRUE: If EDCA parameters are found, always set to HW CRs.
*
* \return none
*/
/*----------------------------------------------------------------------------*/
BOOLEAN
mqmParseEdcaParameters(IN P_ADAPTER_T prAdapter,
		       IN P_SW_RFB_T prSwRfb, IN PUINT_8 pucIE, IN UINT_16 u2IELength, IN BOOLEAN fgForceOverride)
{
	P_STA_RECORD_T prStaRec;
	UINT_16 u2Offset;
	UINT_8 aucWfaOui[] = VENDOR_OUI_WFA;
	P_BSS_INFO_T prBssInfo;
	BOOLEAN fgNewParameter = FALSE;

	DEBUGFUNC("mqmParseEdcaParameters");

	if (!prSwRfb)
		return FALSE;

	if (!pucIE)
		return FALSE;

	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);
	/* ASSERT(prStaRec); */

	if (prStaRec == NULL)
		return FALSE;

	DBGLOG(QM, TRACE, "QM: (fgIsWmmSupported=%d, fgIsQoS=%d)\n", prStaRec->fgIsWmmSupported, prStaRec->fgIsQoS);

	if (IS_FEATURE_DISABLED(prAdapter->rWifiVar.ucQoS) || (!prStaRec->fgIsWmmSupported)
	    || (!prStaRec->fgIsQoS))
		return FALSE;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);

	/* Goal: Obtain the EDCA parameters */
	IE_FOR_EACH(pucIE, u2IELength, u2Offset) {
		switch (IE_ID(pucIE)) {
		case ELEM_ID_WMM:
			if (!((WMM_IE_OUI_TYPE(pucIE) == VENDOR_OUI_TYPE_WMM) &&
			      (!kalMemCmp(WMM_IE_OUI(pucIE), aucWfaOui, 3))))
				break;

			switch (WMM_IE_OUI_SUBTYPE(pucIE)) {
			case VENDOR_OUI_SUBTYPE_WMM_PARAM:
				fgNewParameter = mqmUpdateEdcaParameters(prBssInfo, pucIE, fgForceOverride);
				break;

			default:
				/* Other WMM QoS IEs. Ignore */
				break;
			}

			/* else: VENDOR_OUI_TYPE_WPA, VENDOR_OUI_TYPE_WPS, ... (not cared) */
			break;
		default:
			break;
		}
	}

	return fgNewParameter;
}

BOOLEAN mqmCompareEdcaParameters(IN P_IE_WMM_PARAM_T prIeWmmParam, IN P_BSS_INFO_T prBssInfo)
{
	P_AC_QUE_PARMS_T prAcQueParams;
	P_WMM_AC_PARAM_T prWmmAcParams;
	ENUM_WMM_ACI_T eAci;

	/* return FALSE; */

	/* Check Set Count */
	if (prBssInfo->ucWmmParamSetCount != (prIeWmmParam->ucQosInfo & WMM_QOS_INFO_PARAM_SET_CNT))
		return FALSE;

	for (eAci = 0; eAci < WMM_AC_INDEX_NUM; eAci++) {
		prAcQueParams = &prBssInfo->arACQueParms[eAci];
		prWmmAcParams = &prIeWmmParam->arAcParam[eAci];

		/* ACM */
		if (prAcQueParams->ucIsACMSet != ((prWmmAcParams->ucAciAifsn & WMM_ACIAIFSN_ACM) ? TRUE : FALSE))
			return FALSE;

		/* AIFSN */
		if (prAcQueParams->u2Aifsn != (prWmmAcParams->ucAciAifsn & WMM_ACIAIFSN_AIFSN))
			return FALSE;

		/* CW Max */
		if (prAcQueParams->u2CWmax !=
		    (BIT((prWmmAcParams->ucEcw & WMM_ECW_WMAX_MASK) >> WMM_ECW_WMAX_OFFSET) - 1))
			return FALSE;

		/* CW Min */
		if (prAcQueParams->u2CWmin != (BIT(prWmmAcParams->ucEcw & WMM_ECW_WMIN_MASK) - 1))
			return FALSE;

		if (prAcQueParams->u2TxopLimit != prWmmAcParams->u2TxopLimit)
			return FALSE;
	}

	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is used for parsing EDCA parameters specified in the WMM Parameter IE
*
* \param[in] prAdapter           Adapter pointer
* \param[in] prIeWmmParam        The pointer to the WMM Parameter IE
* \param[in] u4AcOffset          The offset specifying the AC queue for parsing
* \param[in] prHwAcParams        The parameter structure used to configure the HW CRs
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID mqmFillAcQueParam(IN P_IE_WMM_PARAM_T prIeWmmParam, IN UINT_32 u4AcOffset, OUT P_AC_QUE_PARMS_T prAcQueParams)
{
	P_WMM_AC_PARAM_T prAcParam = &prIeWmmParam->arAcParam[u4AcOffset];

	prAcQueParams->ucIsACMSet = (prAcParam->ucAciAifsn & WMM_ACIAIFSN_ACM) ? TRUE : FALSE;

	prAcQueParams->u2Aifsn = (prAcParam->ucAciAifsn & WMM_ACIAIFSN_AIFSN);

	prAcQueParams->u2CWmax = BIT((prAcParam->ucEcw & WMM_ECW_WMAX_MASK) >> WMM_ECW_WMAX_OFFSET) - 1;

	prAcQueParams->u2CWmin = BIT(prAcParam->ucEcw & WMM_ECW_WMIN_MASK) - 1;

	WLAN_GET_FIELD_16(&prAcParam->u2TxopLimit, &prAcQueParams->u2TxopLimit);

	prAcQueParams->ucGuradTime = TXM_DEFAULT_FLUSH_QUEUE_GUARD_TIME;

}

/*----------------------------------------------------------------------------*/
/*!
* \brief To parse WMM/11n related IEs in scan results (only for AP peers)
*
* \param[in] prAdapter       Adapter pointer
* \param[in]  prScanResult   The scan result which shall be parsed to obtain needed info
* \param[out] prStaRec       The obtained info is stored in the STA_REC
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID mqmProcessScanResult(IN P_ADAPTER_T prAdapter, IN P_BSS_DESC_T prScanResult, OUT P_STA_RECORD_T prStaRec)
{
	PUINT_8 pucIE;
	UINT_16 u2IELength;
	UINT_16 u2Offset;
	UINT_8 aucWfaOui[] = VENDOR_OUI_WFA;
	BOOLEAN fgIsHtVht;

	DEBUGFUNC("mqmProcessScanResult");

	ASSERT(prScanResult);
	ASSERT(prStaRec);

	/* Reset the flag before parsing */
	prStaRec->fgIsWmmSupported = FALSE;
	prStaRec->fgIsUapsdSupported = FALSE;
	prStaRec->fgIsQoS = FALSE;

	fgIsHtVht = FALSE;

	if (IS_FEATURE_DISABLED(prAdapter->rWifiVar.ucQoS))
		return;

	u2IELength = prScanResult->u2IELength;
	pucIE = prScanResult->aucIEBuf;

	/* <1> Determine whether the peer supports WMM/QoS and UAPSDU */
	IE_FOR_EACH(pucIE, u2IELength, u2Offset) {
		switch (IE_ID(pucIE)) {

		case ELEM_ID_EXTENDED_CAP:
#if CFG_SUPPORT_TDLS
			TdlsBssExtCapParse(prStaRec, pucIE);
#endif /* CFG_SUPPORT_TDLS */
			break;

		case ELEM_ID_WMM:
			if ((WMM_IE_OUI_TYPE(pucIE) == VENDOR_OUI_TYPE_WMM) &&
			    (!kalMemCmp(WMM_IE_OUI(pucIE), aucWfaOui, 3))) {

				switch (WMM_IE_OUI_SUBTYPE(pucIE)) {
				case VENDOR_OUI_SUBTYPE_WMM_PARAM:
					if (IE_LEN(pucIE) != 24)
						break;	/* WMM Param IE with a wrong length */

					prStaRec->fgIsWmmSupported = TRUE;
					prStaRec->fgIsUapsdSupported =
					((((P_IE_WMM_PARAM_T) pucIE)->ucQosInfo)
					& WMM_QOS_INFO_UAPSD) ? TRUE : FALSE;
					break;

				case VENDOR_OUI_SUBTYPE_WMM_INFO:
					if (IE_LEN(pucIE) != 7)
						break;	/* WMM Info IE with a wrong length */

					prStaRec->fgIsWmmSupported = TRUE;
					prStaRec->fgIsUapsdSupported =
					((((P_IE_WMM_INFO_T) pucIE)->ucQosInfo)
					& WMM_QOS_INFO_UAPSD) ? TRUE : FALSE;
					break;

				default:
					/* A WMM QoS IE that doesn't matter. Ignore it. */
					break;
				}
			}
			/* else: VENDOR_OUI_TYPE_WPA, VENDOR_OUI_TYPE_WPS, ... (not cared) */

			break;

		default:
			/* A WMM IE that doesn't matter. Ignore it. */
			break;
		}
	}

	/* <1> Determine QoS */
	if (prStaRec->ucDesiredPhyTypeSet & (PHY_TYPE_SET_802_11N | PHY_TYPE_SET_802_11AC))
		fgIsHtVht = TRUE;

	if (fgIsHtVht || prStaRec->fgIsWmmSupported)
		prStaRec->fgIsQoS = TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief Generate the WMM Info IE by Param
*
* \param[in] prAdapter  Adapter pointer
* @param prMsduInfo The TX MMPDU
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
UINT_32
mqmFillWmmInfoIE(P_UINT_8 pucOutBuf,
		 BOOLEAN fgSupportUAPSD, UINT_8 ucBmpDeliveryAC, UINT_8 ucBmpTriggerAC, UINT_8 ucUapsdSp)
{
	P_IE_WMM_INFO_T prIeWmmInfo;
	UINT_32 ucUapsd[] = {
		WMM_QOS_INFO_BE_UAPSD,
		WMM_QOS_INFO_BK_UAPSD,
		WMM_QOS_INFO_VI_UAPSD,
		WMM_QOS_INFO_VO_UAPSD
	};
	UINT_8 aucWfaOui[] = VENDOR_OUI_WFA;

	ASSERT(pucOutBuf);

	prIeWmmInfo = (P_IE_WMM_INFO_T) pucOutBuf;

	prIeWmmInfo->ucId = ELEM_ID_WMM;
	prIeWmmInfo->ucLength = ELEM_MAX_LEN_WMM_INFO;

	/* WMM-2.2.1 WMM Information Element Field Values */
	prIeWmmInfo->aucOui[0] = aucWfaOui[0];
	prIeWmmInfo->aucOui[1] = aucWfaOui[1];
	prIeWmmInfo->aucOui[2] = aucWfaOui[2];
	prIeWmmInfo->ucOuiType = VENDOR_OUI_TYPE_WMM;
	prIeWmmInfo->ucOuiSubtype = VENDOR_OUI_SUBTYPE_WMM_INFO;

	prIeWmmInfo->ucVersion = VERSION_WMM;
	prIeWmmInfo->ucQosInfo = 0;

	/* UAPSD initial queue configurations (delivery and trigger enabled) */
	if (fgSupportUAPSD) {
		UINT_8 ucQosInfo = 0;
		UINT_8 i;

		/* Static U-APSD setting */
		for (i = ACI_BE; i <= ACI_VO; i++) {
			if (ucBmpDeliveryAC & ucBmpTriggerAC & BIT(i))
				ucQosInfo |= (UINT_8) ucUapsd[i];
		}

		if (ucBmpDeliveryAC & ucBmpTriggerAC) {
			switch (ucUapsdSp) {
			case WMM_MAX_SP_LENGTH_ALL:
				ucQosInfo |= WMM_QOS_INFO_MAX_SP_ALL;
				break;

			case WMM_MAX_SP_LENGTH_2:
				ucQosInfo |= WMM_QOS_INFO_MAX_SP_2;
				break;

			case WMM_MAX_SP_LENGTH_4:
				ucQosInfo |= WMM_QOS_INFO_MAX_SP_4;
				break;

			case WMM_MAX_SP_LENGTH_6:
				ucQosInfo |= WMM_QOS_INFO_MAX_SP_6;
				break;

			default:
				DBGLOG(QM, INFO, "MQM: Incorrect SP length\n");
				ucQosInfo |= WMM_QOS_INFO_MAX_SP_2;
				break;
			}
		}
		prIeWmmInfo->ucQosInfo = ucQosInfo;

	}

	/* Increment the total IE length for the Element ID and Length fields. */
	return IE_SIZE(prIeWmmInfo);
}

/*----------------------------------------------------------------------------*/
/*!
* @brief Generate the WMM Info IE
*
* \param[in] prAdapter  Adapter pointer
* @param prMsduInfo The TX MMPDU
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
UINT_32
mqmGenerateWmmInfoIEByStaRec(P_ADAPTER_T prAdapter, P_BSS_INFO_T prBssInfo, P_STA_RECORD_T prStaRec, P_UINT_8 pucOutBuf)
{
	P_PM_PROFILE_SETUP_INFO_T prPmProfSetupInfo;
	BOOLEAN fgSupportUapsd;

	ASSERT(pucOutBuf);

	/* In case QoS is not turned off, exit directly */
	if (IS_FEATURE_DISABLED(prAdapter->rWifiVar.ucQoS))
		return 0;

	if (prStaRec == NULL)
		return 0;

	if (!prStaRec->fgIsWmmSupported)
		return 0;

	prPmProfSetupInfo = &prBssInfo->rPmProfSetupInfo;

	fgSupportUapsd = (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucUapsd)
			  && prStaRec->fgIsUapsdSupported);

	return mqmFillWmmInfoIE(pucOutBuf,
				fgSupportUapsd,
				prPmProfSetupInfo->ucBmpDeliveryAC,
				prPmProfSetupInfo->ucBmpTriggerAC, prPmProfSetupInfo->ucUapsdSp);
}

/*----------------------------------------------------------------------------*/
/*!
* @brief Generate the WMM Info IE
*
* \param[in] prAdapter  Adapter pointer
* @param prMsduInfo The TX MMPDU
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID mqmGenerateWmmInfoIE(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo)
{
	P_BSS_INFO_T prBssInfo;
	P_STA_RECORD_T prStaRec;
	UINT_32 u4Length;

	DEBUGFUNC("mqmGenerateWmmInfoIE");

	ASSERT(prMsduInfo);

	/* In case QoS is not turned off, exit directly */
	if (IS_FEATURE_DISABLED(prAdapter->rWifiVar.ucQoS))
		return;

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);
	ASSERT(prStaRec);

	if (prStaRec == NULL)
		return;

	if (!prStaRec->fgIsWmmSupported)
		return;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);

	u4Length = mqmGenerateWmmInfoIEByStaRec(prAdapter,
						prBssInfo,
						prStaRec, ((PUINT_8) prMsduInfo->prPacket + prMsduInfo->u2FrameLength));

	prMsduInfo->u2FrameLength += u4Length;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief Generate the WMM Param IE
*
* \param[in] prAdapter  Adapter pointer
* @param prMsduInfo The TX MMPDU
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID mqmGenerateWmmParamIE(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo)
{
	P_IE_WMM_PARAM_T prIeWmmParam;

	UINT_8 aucWfaOui[] = VENDOR_OUI_WFA;

	UINT_8 aucACI[] = {
		WMM_ACI_AC_BE,
		WMM_ACI_AC_BK,
		WMM_ACI_AC_VI,
		WMM_ACI_AC_VO
	};

	P_BSS_INFO_T prBssInfo;
	P_STA_RECORD_T prStaRec;
	ENUM_WMM_ACI_T eAci;
	P_WMM_AC_PARAM_T prAcParam;

	DEBUGFUNC("mqmGenerateWmmParamIE");
	DBGLOG(QM, LOUD, "\n");

	ASSERT(prMsduInfo);

	/* In case QoS is not turned off, exit directly */
	if (IS_FEATURE_DISABLED(prAdapter->rWifiVar.ucQoS))
		return;

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

	if (prStaRec) {
		if (!prStaRec->fgIsQoS)
			return;
	}

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prMsduInfo->ucBssIndex);

	if (!prBssInfo->fgIsQBSS)
		return;

	prIeWmmParam = (P_IE_WMM_PARAM_T)
	    ((PUINT_8) prMsduInfo->prPacket + prMsduInfo->u2FrameLength);

	prIeWmmParam->ucId = ELEM_ID_WMM;
	prIeWmmParam->ucLength = ELEM_MAX_LEN_WMM_PARAM;

	/* WMM-2.2.1 WMM Information Element Field Values */
	prIeWmmParam->aucOui[0] = aucWfaOui[0];
	prIeWmmParam->aucOui[1] = aucWfaOui[1];
	prIeWmmParam->aucOui[2] = aucWfaOui[2];
	prIeWmmParam->ucOuiType = VENDOR_OUI_TYPE_WMM;
	prIeWmmParam->ucOuiSubtype = VENDOR_OUI_SUBTYPE_WMM_PARAM;

	prIeWmmParam->ucVersion = VERSION_WMM;
	prIeWmmParam->ucQosInfo = (prBssInfo->ucWmmParamSetCount & WMM_QOS_INFO_PARAM_SET_CNT);

	/* UAPSD initial queue configurations (delivery and trigger enabled) */
	if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucUapsd))
		prIeWmmParam->ucQosInfo |= WMM_QOS_INFO_UAPSD;

	/* EDCA parameter */

	for (eAci = 0; eAci < WMM_AC_INDEX_NUM; eAci++) {
		prAcParam = &prIeWmmParam->arAcParam[eAci];

		/* DBGLOG(QM, LOUD, ("MQM: eAci=%d, ACM = %d, Aifsn = %d, CWmin = %d, CWmax = %d, TxopLimit = %d\n", */
		/* eAci,prBssInfo->arACQueParmsForBcast[eAci].ucIsACMSet , */
		/* prBssInfo->arACQueParmsForBcast[eAci].u2Aifsn, */
		/* prBssInfo->arACQueParmsForBcast[eAci].u2CWmin, */
		/* prBssInfo->arACQueParmsForBcast[eAci].u2CWmax, */
		/* prBssInfo->arACQueParmsForBcast[eAci].u2TxopLimit)); */

		/* ACI */
		prAcParam->ucAciAifsn = aucACI[eAci];
		/* ACM */
		if (prBssInfo->arACQueParmsForBcast[eAci].ucIsACMSet)
			prAcParam->ucAciAifsn |= WMM_ACIAIFSN_ACM;
		/* AIFSN */
		prAcParam->ucAciAifsn |= (prBssInfo->arACQueParmsForBcast[eAci].u2Aifsn & WMM_ACIAIFSN_AIFSN);

		/* ECW Min */
		prAcParam->ucEcw = (prBssInfo->aucCWminLog2ForBcast[eAci] & WMM_ECW_WMIN_MASK);
		/* ECW Max */
		prAcParam->ucEcw |=
		    ((prBssInfo->aucCWmaxLog2ForBcast[eAci] << WMM_ECW_WMAX_OFFSET) & WMM_ECW_WMAX_MASK);

		/* Txop limit */
		WLAN_SET_FIELD_16(&prAcParam->u2TxopLimit, prBssInfo->arACQueParmsForBcast[eAci].u2TxopLimit);

	}

	/* Increment the total IE length for the Element ID and Length fields. */
	prMsduInfo->u2FrameLength += IE_SIZE(prIeWmmParam);

}

#if CFG_SUPPORT_TDLS
/*----------------------------------------------------------------------------*/
/*!
* @brief Generate the WMM Param IE
*
* \param[in] prAdapter  Adapter pointer
* @param prMsduInfo The TX MMPDU
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
UINT_32 mqmGenerateWmmParamIEByParam(P_ADAPTER_T prAdapter, P_BSS_INFO_T prBssInfo, PUINT_8 pOutBuf)
{
	P_IE_WMM_PARAM_T prIeWmmParam;

	UINT_8 aucWfaOui[] = VENDOR_OUI_WFA;

	UINT_8 aucACI[] = {
		WMM_ACI_AC_BE,
		WMM_ACI_AC_BK,
		WMM_ACI_AC_VI,
		WMM_ACI_AC_VO
	};

	P_AC_QUE_PARMS_T prACQueParms;
	UINT_8 auCWminLog2ForBcast[WMM_AC_INDEX_NUM] = { 4 /*BE*/, 4 /*BK*/, 3 /*VO*/, 2 /*VI*/ };
	UINT_8 auCWmaxLog2ForBcast[WMM_AC_INDEX_NUM] = { 10, 10, 4, 3 };
	UINT_8 auAifsForBcast[WMM_AC_INDEX_NUM] = { 3, 7, 2, 2 };
	UINT_8 auTxopForBcast[WMM_AC_INDEX_NUM] = { 0, 0, 94, 47 };	/* If the AP is OFDM */

	ENUM_WMM_ACI_T eAci;
	P_WMM_AC_PARAM_T prAcParam;

	DEBUGFUNC("mqmGenerateWmmParamIE");
	DBGLOG(QM, LOUD, "\n");

	ASSERT(pOutBuf);

	/* In case QoS is not turned off, exit directly */
	if (IS_FEATURE_DISABLED(prAdapter->rWifiVar.ucQoS))
		return WLAN_STATUS_SUCCESS;

	if (!prBssInfo->fgIsQBSS)
		return WLAN_STATUS_SUCCESS;

	if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.fgTdlsBufferSTASleep)) {
		prACQueParms = prBssInfo->arACQueParmsForBcast;

		for (eAci = 0; eAci < WMM_AC_INDEX_NUM; eAci++) {
			prACQueParms[eAci].ucIsACMSet = FALSE;
			prACQueParms[eAci].u2Aifsn = auAifsForBcast[eAci];
			prACQueParms[eAci].u2CWmin = BIT(auCWminLog2ForBcast[eAci]) - 1;
			prACQueParms[eAci].u2CWmax = BIT(auCWmaxLog2ForBcast[eAci]) - 1;
			prACQueParms[eAci].u2TxopLimit = auTxopForBcast[eAci];

			/* used to send WMM IE */
			prBssInfo->aucCWminLog2ForBcast[eAci] = auCWminLog2ForBcast[eAci];
			prBssInfo->aucCWmaxLog2ForBcast[eAci] = auCWmaxLog2ForBcast[eAci];

			/* DBGLOG(TDLS, INFO, "eAci = %d, ACM = %d, Aifsn = %d, CWmin = %d, CWmax = %d,
			*	TxopLimit = %d\n",
			*	eAci, prACQueParms[eAci].ucIsACMSet, prACQueParms[eAci].u2Aifsn,
			*	prACQueParms[eAci].u2CWmin, prACQueParms[eAci].u2CWmax, prACQueParms[eAci].u2TxopLimit);
			*/
		}
	}

	prIeWmmParam = (P_IE_WMM_PARAM_T) pOutBuf;

	prIeWmmParam->ucId = ELEM_ID_WMM;
	prIeWmmParam->ucLength = ELEM_MAX_LEN_WMM_PARAM;

	/* WMM-2.2.1 WMM Information Element Field Values */
	prIeWmmParam->aucOui[0] = aucWfaOui[0];
	prIeWmmParam->aucOui[1] = aucWfaOui[1];
	prIeWmmParam->aucOui[2] = aucWfaOui[2];
	prIeWmmParam->ucOuiType = VENDOR_OUI_TYPE_WMM;
	prIeWmmParam->ucOuiSubtype = VENDOR_OUI_SUBTYPE_WMM_PARAM;

	prIeWmmParam->ucVersion = VERSION_WMM;
	/* STAUT Buffer STA, also sleeps (optional)
	*	The STAUT sends a TDLS Setup/Response/Confirm Frame, to STA 2, via the AP,
	*	with all four AC flags set to 1 in QoS Info Field
	*/
	if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.fgTdlsBufferSTASleep))
		prIeWmmParam->ucQosInfo = (0x0F & WMM_QOS_INFO_PARAM_SET_CNT);
	else
		prIeWmmParam->ucQosInfo = (prBssInfo->ucWmmParamSetCount & WMM_QOS_INFO_PARAM_SET_CNT);

	/* UAPSD initial queue configurations (delivery and trigger enabled) */
	if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucUapsd))
		prIeWmmParam->ucQosInfo |= WMM_QOS_INFO_UAPSD;

	/* EDCA parameter */

	for (eAci = 0; eAci < WMM_AC_INDEX_NUM; eAci++) {
		prAcParam = &prIeWmmParam->arAcParam[eAci];

		/* DBGLOG(QM, LOUD, ("MQM: eAci=%d, ACM = %d, Aifsn = %d, CWmin = %d, CWmax = %d, TxopLimit = %d\n", */
		/* eAci,prBssInfo->arACQueParmsForBcast[eAci].ucIsACMSet , */
		/* prBssInfo->arACQueParmsForBcast[eAci].u2Aifsn, */
		/* prBssInfo->arACQueParmsForBcast[eAci].u2CWmin, */
		/* prBssInfo->arACQueParmsForBcast[eAci].u2CWmax, */
		/* prBssInfo->arACQueParmsForBcast[eAci].u2TxopLimit)); */

		/* ACI */
		prAcParam->ucAciAifsn = aucACI[eAci];

		if (IS_FEATURE_ENABLED(
				prAdapter->rWifiVar.fgTdlsBufferSTASleep)) {
			/* ACM */
			if (prBssInfo->arACQueParmsForBcast[eAci].ucIsACMSet)
				prAcParam->ucAciAifsn |= WMM_ACIAIFSN_ACM;
			/* AIFSN */
			prAcParam->ucAciAifsn |=
				(prBssInfo->arACQueParmsForBcast[eAci].u2Aifsn
				& WMM_ACIAIFSN_AIFSN);

			/* ECW Min */
			prAcParam->ucEcw =
				(prBssInfo->aucCWminLog2ForBcast[eAci]
				& WMM_ECW_WMIN_MASK);
			/* ECW Max */
			prAcParam->ucEcw |=
			    ((prBssInfo->aucCWmaxLog2ForBcast[eAci]
				<< WMM_ECW_WMAX_OFFSET) & WMM_ECW_WMAX_MASK);

			/* Txop limit */
			WLAN_SET_FIELD_16(&prAcParam->u2TxopLimit,
				prBssInfo->arACQueParmsForBcast[eAci].
					u2TxopLimit);
		} else {
			/* ACM */
			if (prBssInfo->arACQueParms[eAci].ucIsACMSet)
				prAcParam->ucAciAifsn |= WMM_ACIAIFSN_ACM;
			/* AIFSN */
			prAcParam->ucAciAifsn |=
				(prBssInfo->arACQueParms[eAci].u2Aifsn
				& WMM_ACIAIFSN_AIFSN);

			/* ECW Min */
			prAcParam->ucEcw =
				(prBssInfo->arACQueParms[eAci].u2CWmin
				& WMM_ECW_WMIN_MASK);
			/* ECW Max */
			prAcParam->ucEcw |=
				((prBssInfo->arACQueParms[eAci].u2CWmax
				<< WMM_ECW_WMAX_OFFSET) & WMM_ECW_WMAX_MASK);

			/* Txop limit */
			WLAN_SET_FIELD_16(&prAcParam->u2TxopLimit,
				prBssInfo->arACQueParms[eAci].u2TxopLimit);
		}
	}

	/* Increment the total IE length for the Element ID and Length fields. */
	return IE_SIZE(prIeWmmParam);

}

#endif

BOOLEAN isProbeResponse(IN P_MSDU_INFO_T prMgmtTxMsdu)
{
	P_WLAN_MAC_HEADER_T prWlanHdr = (P_WLAN_MAC_HEADER_T) NULL;

	prWlanHdr = (P_WLAN_MAC_HEADER_T) ((ULONG) prMgmtTxMsdu->prPacket +
			MAC_TX_RESERVED_FIELD);

	return (prWlanHdr->u2FrameCtrl & MASK_FRAME_TYPE) == MAC_FRAME_PROBE_RSP ?
		TRUE : FALSE;
}


ENUM_FRAME_ACTION_T
qmGetFrameAction(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex,
		 IN UINT_8 ucStaRecIdx, IN P_MSDU_INFO_T prMsduInfo,
		 IN ENUM_FRAME_TYPE_IN_CMD_Q_T eFrameType, IN UINT_16 u2FrameLength)
{
	ENUM_FRAME_ACTION_T eFrameAction = FRAME_ACTION_TX_PKT;
	P_BSS_INFO_T prBssInfo;
	P_STA_RECORD_T prStaRec;
	UINT_8 ucTC = nicTxGetFrameResourceType(eFrameType, prMsduInfo);
	UINT_16 u2FreeResource = nicTxGetResource(prAdapter, ucTC);
	UINT_8 ucReqResource;
	P_WIFI_VAR_T prWifiVar = &prAdapter->rWifiVar;

	DEBUGFUNC("qmGetFrameAction");

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);
	prStaRec = QM_GET_STA_REC_PTR_FROM_INDEX(prAdapter, ucStaRecIdx);

	do {
		/* 4 <1> Tx, if FORCE_TX is set */
		if (prMsduInfo) {
			if (prMsduInfo->ucControlFlag & MSDU_CONTROL_FLAG_FORCE_TX) {
				eFrameAction = FRAME_ACTION_TX_PKT;
				break;
			}
		}
		/* 4 <2> Drop, if BSS is inactive */
		if (!IS_BSS_ACTIVE(prBssInfo)) {
			DBGLOG(QM, TRACE, "Drop packets (BSS[%u] is INACTIVE)\n", prBssInfo->ucBssIndex);
			eFrameAction = FRAME_ACTION_DROP_PKT;
			break;
		}

		/* 4 <3> Queue, if BSS is absent, drop probe response */
		if (prBssInfo->fgIsNetAbsent) {
			if (prMsduInfo && isProbeResponse(prMsduInfo)) {
				DBGLOG(TX, TRACE, "Drop probe response (BSS[%u] Absent)\n",
					prBssInfo->ucBssIndex);

				eFrameAction = FRAME_ACTION_DROP_PKT;
			} else {
				DBGLOG(TX, TRACE, "Queue packets (BSS[%u] Absent)\n",
					prBssInfo->ucBssIndex);
				eFrameAction = FRAME_ACTION_QUEUE_PKT;
			}
			break;
		}

		/* 4 <4> Check based on StaRec */
		if (prStaRec) {
			/* 4 <4.1> Drop, if StaRec is not in use */
			if (!prStaRec->fgIsInUse) {
				DBGLOG(QM, TRACE, "Drop packets (Sta[%u] not in USE)\n", prStaRec->ucIndex);
				eFrameAction = FRAME_ACTION_DROP_PKT;
				break;
			}
			/* 4 <4.2> Sta in PS */
			if (prStaRec->fgIsInPS) {
				ucReqResource = nicTxGetPageCount(u2FrameLength, FALSE) +
				    prWifiVar->ucCmdRsvResource + QM_MGMT_QUEUED_THRESHOLD;

				/* 4 <4.2.1> Tx, if resource is enough */
				if (u2FreeResource > ucReqResource) {
					eFrameAction = FRAME_ACTION_TX_PKT;
					break;
				}
				/* 4 <4.2.2> Queue, if resource is not enough */
				else {
					DBGLOG(QM, INFO, "Queue packets (Sta[%u] in PS)\n", prStaRec->ucIndex);
					eFrameAction = FRAME_ACTION_QUEUE_PKT;
					break;
				}
			}
		}

	} while (FALSE);

	/* <5> Resource CHECK! */
	/* <5.1> Reserve resource for CMD & 1X */
	if (eFrameType == FRAME_TYPE_MMPDU) {
		ucReqResource = nicTxGetPageCount(u2FrameLength, FALSE) + prWifiVar->ucCmdRsvResource;

		if (u2FreeResource < ucReqResource) {
			eFrameAction = FRAME_ACTION_QUEUE_PKT;
			DBGLOG(QM, INFO, "Queue MGMT (MSDU[0x%p] Req/Rsv/Free[%u/%u/%u])\n",
			       prMsduInfo,
			       nicTxGetPageCount(u2FrameLength, FALSE), prWifiVar->ucCmdRsvResource, u2FreeResource);
		}

		/* <6> Timeout check! */
#if CFG_ENABLE_PKT_LIFETIME_PROFILE
		if ((eFrameAction == FRAME_ACTION_QUEUE_PKT) && prMsduInfo) {
			OS_SYSTIME rCurrentTime, rEnqTime;

			GET_CURRENT_SYSTIME(&rCurrentTime);
			rEnqTime = prMsduInfo->rPktProfile.rEnqueueTimestamp;

			if (CHECK_FOR_TIMEOUT(rCurrentTime, rEnqTime,
					      MSEC_TO_SYSTIME(prWifiVar->u4MgmtQueueDelayTimeout))) {
				eFrameAction = FRAME_ACTION_DROP_PKT;
				DBGLOG(QM, INFO, "Drop MGMT (MSDU[0x%p] timeout[%ums])\n",
				       prMsduInfo, prWifiVar->u4MgmtQueueDelayTimeout);
			}
		}
#endif
	}

	return eFrameAction;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Handle BSS change operation Event from the FW
*
* \param[in] prAdapter Adapter pointer
* \param[in] prEvent The event packet from the FW
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID qmHandleEventBssAbsencePresence(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent)
{
	P_EVENT_BSS_ABSENCE_PRESENCE_T prEventBssStatus;
	P_BSS_INFO_T prBssInfo;
	BOOLEAN fgIsNetAbsentOld;
#if (CFG_SUPPORT_DFS_MASTER == 1)
	P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo = (P_P2P_ROLE_FSM_INFO_T) NULL;
#endif
	prEventBssStatus = (P_EVENT_BSS_ABSENCE_PRESENCE_T) (prEvent->aucBuffer);
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prEventBssStatus->ucBssIndex);
	fgIsNetAbsentOld = prBssInfo->fgIsNetAbsent;
	prBssInfo->fgIsNetAbsent = prEventBssStatus->ucIsAbsent;
	prBssInfo->ucBssFreeQuota = prEventBssStatus->ucBssFreeQuota;
#if (CFG_SUPPORT_DFS_MASTER == 1)
	prP2pRoleFsmInfo = P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
						prBssInfo->u4PrivateData);
#endif
	/* DBGLOG(QM, TRACE, ("qmHandleEventBssAbsencePresence (ucNetTypeIdx=%d, fgIsAbsent=%d, FreeQuota=%d)\n", */
	/* prEventBssStatus->ucNetTypeIdx, prBssInfo->fgIsNetAbsent, prBssInfo->ucBssFreeQuota)); */

	DBGLOG(QM, INFO, "NAF=%d,%d,%d\n",
	       prEventBssStatus->ucBssIndex, prBssInfo->fgIsNetAbsent, prBssInfo->ucBssFreeQuota);

	if (!prBssInfo->fgIsNetAbsent) {
		/* ToDo:: QM_DBG_CNT_INC */
		QM_DBG_CNT_INC(&(prAdapter->rQM), QM_DBG_CNT_27);
	} else {
		/* ToDo:: QM_DBG_CNT_INC */
		QM_DBG_CNT_INC(&(prAdapter->rQM), QM_DBG_CNT_28);
	}
	/* From Absent to Present */
	if ((fgIsNetAbsentOld) && (!prBssInfo->fgIsNetAbsent)) {
		if (HAL_IS_TX_DIRECT(prAdapter))
			nicTxDirectStartCheckQTimer(prAdapter);
		else
			kalSetEvent(prAdapter->prGlueInfo);
#if (CFG_SUPPORT_DFS_MASTER == 1)
		if (prP2pRoleFsmInfo && prAdapter->prGlueInfo->
			prP2PInfo[prP2pRoleFsmInfo->ucRoleIndex]
				->fgIsNetDevDetach) {
			netif_device_attach(prAdapter->prGlueInfo->
				prP2PInfo[prP2pRoleFsmInfo->ucRoleIndex]
					->prDevHandler);
			prAdapter->prGlueInfo
				->prP2PInfo[prP2pRoleFsmInfo->ucRoleIndex]
					->fgIsNetDevDetach = FALSE;
		}
#endif
	}
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Handle STA change PS mode Event from the FW
*
* \param[in] prAdapter Adapter pointer
* \param[in] prEvent The event packet from the FW
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID qmHandleEventStaChangePsMode(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent)
{
	P_EVENT_STA_CHANGE_PS_MODE_T prEventStaChangePsMode;
	P_STA_RECORD_T prStaRec;
	BOOLEAN fgIsInPSOld;

	/* DbgPrint("QM:Event -RxBa\n"); */

	prEventStaChangePsMode = (P_EVENT_STA_CHANGE_PS_MODE_T) (prEvent->aucBuffer);
	prStaRec = QM_GET_STA_REC_PTR_FROM_INDEX(prAdapter, prEventStaChangePsMode->ucStaRecIdx);
	/* ASSERT(prStaRec); */

	if (prStaRec) {

		fgIsInPSOld = prStaRec->fgIsInPS;
		prStaRec->fgIsInPS = prEventStaChangePsMode->ucIsInPs;

		qmUpdateFreeQuota(prAdapter,
				  prStaRec, prEventStaChangePsMode->ucUpdateMode, prEventStaChangePsMode->ucFreeQuota);

		/* DBGLOG(QM, TRACE, ("qmHandleEventStaChangePsMode (ucStaRecIdx=%d, fgIsInPs=%d)\n", */
		/* prEventStaChangePsMode->ucStaRecIdx, prStaRec->fgIsInPS)); */

		DBGLOG(QM, INFO, "PS=%d,%d\n", prEventStaChangePsMode->ucStaRecIdx, prStaRec->fgIsInPS);

		/* From PS to Awake */
		if ((fgIsInPSOld) && (!prStaRec->fgIsInPS)) {
			if (HAL_IS_TX_DIRECT(prAdapter))
				nicTxDirectStartCheckQTimer(prAdapter);
			else
				kalSetEvent(prAdapter->prGlueInfo);
		}
	}
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Update STA free quota Event from FW
*
* \param[in] prAdapter Adapter pointer
* \param[in] prEvent The event packet from the FW
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID qmHandleEventStaUpdateFreeQuota(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent)
{
	P_EVENT_STA_UPDATE_FREE_QUOTA_T prEventStaUpdateFreeQuota;
	P_STA_RECORD_T prStaRec;

	prEventStaUpdateFreeQuota = (P_EVENT_STA_UPDATE_FREE_QUOTA_T) (prEvent->aucBuffer);
	prStaRec = QM_GET_STA_REC_PTR_FROM_INDEX(prAdapter, prEventStaUpdateFreeQuota->ucStaRecIdx);
	/* 2013/08/30
	 * Station Record possible been freed.
	 */
	/* ASSERT(prStaRec); */

	if (prStaRec) {
		if (prStaRec->fgIsInPS) {
			qmUpdateFreeQuota(prAdapter,
					  prStaRec,
					  prEventStaUpdateFreeQuota->ucUpdateMode,
					  prEventStaUpdateFreeQuota->ucFreeQuota);

			if (HAL_IS_TX_DIRECT(prAdapter))
				nicTxDirectStartCheckQTimer(prAdapter);
			else
				kalSetEvent(prAdapter->prGlueInfo);
		}
#if 0
		DBGLOG(QM, TRACE,
		       "qmHandleEventStaUpdateFreeQuota (ucStaRecIdx=%d, ucUpdateMode=%d, ucFreeQuota=%d)\n",
		       prEventStaUpdateFreeQuota->ucStaRecIdx,
		       prEventStaUpdateFreeQuota->ucUpdateMode, prEventStaUpdateFreeQuota->ucFreeQuota);
#endif

		DBGLOG(QM, TRACE, "UFQ=%d,%d,%d\n",
		       prEventStaUpdateFreeQuota->ucStaRecIdx,
		       prEventStaUpdateFreeQuota->ucUpdateMode, prEventStaUpdateFreeQuota->ucFreeQuota);

	}

}

/*----------------------------------------------------------------------------*/
/*!
* \brief Update STA free quota
*
* \param[in] prStaRec the STA
* \param[in] ucUpdateMode the method to update free quota
* \param[in] ucFreeQuota  the value for update
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID
qmUpdateFreeQuota(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec, IN UINT_8 ucUpdateMode, IN UINT_8 ucFreeQuota)
{

	UINT_8 ucFreeQuotaForNonDelivery;
	UINT_8 ucFreeQuotaForDelivery;

	ASSERT(prStaRec);
	DBGLOG(QM, LOUD, "qmUpdateFreeQuota orig ucFreeQuota=%d Mode %u New %u\n",
	       prStaRec->ucFreeQuota, ucUpdateMode, ucFreeQuota);

	if (!prStaRec->fgIsInPS)
		return;

	switch (ucUpdateMode) {
	case FREE_QUOTA_UPDATE_MODE_INIT:
	case FREE_QUOTA_UPDATE_MODE_OVERWRITE:
		prStaRec->ucFreeQuota = ucFreeQuota;
		break;
	case FREE_QUOTA_UPDATE_MODE_INCREASE:
		prStaRec->ucFreeQuota += ucFreeQuota;
		break;
	case FREE_QUOTA_UPDATE_MODE_DECREASE:
		prStaRec->ucFreeQuota -= ucFreeQuota;
		break;
	default:
		ASSERT(0);
	}

	DBGLOG(QM, LOUD, "qmUpdateFreeQuota new ucFreeQuota=%d)\n", prStaRec->ucFreeQuota);

	ucFreeQuota = prStaRec->ucFreeQuota;

	ucFreeQuotaForNonDelivery = 0;
	ucFreeQuotaForDelivery = 0;

	if (ucFreeQuota > 0) {
		if (prStaRec->fgIsQoS && prStaRec->fgIsUapsdSupported
		    /* && prAdapter->rWifiVar.fgSupportQoS */
		    /* && prAdapter->rWifiVar.fgSupportUAPSD */) {
			/* XXX We should assign quota to aucFreeQuotaPerQueue[NUM_OF_PER_STA_TX_QUEUES]  */

			if (prStaRec->ucFreeQuotaForNonDelivery > 0 && prStaRec->ucFreeQuotaForDelivery > 0) {
				ucFreeQuotaForNonDelivery = ucFreeQuota >> 1;
				ucFreeQuotaForDelivery = ucFreeQuota - ucFreeQuotaForNonDelivery;
			} else if (prStaRec->ucFreeQuotaForNonDelivery == 0 && prStaRec->ucFreeQuotaForDelivery == 0) {
				ucFreeQuotaForNonDelivery = ucFreeQuota >> 1;
				ucFreeQuotaForDelivery = ucFreeQuota - ucFreeQuotaForNonDelivery;
			} else if (prStaRec->ucFreeQuotaForNonDelivery > 0) {
				/* NonDelivery is not busy */
				if (ucFreeQuota >= 3) {
					ucFreeQuotaForNonDelivery = 2;
					ucFreeQuotaForDelivery = ucFreeQuota - ucFreeQuotaForNonDelivery;
				} else {
					ucFreeQuotaForDelivery = ucFreeQuota;
					ucFreeQuotaForNonDelivery = 0;
				}
			} else if (prStaRec->ucFreeQuotaForDelivery > 0) {
				/* Delivery is not busy */
				if (ucFreeQuota >= 3) {
					ucFreeQuotaForDelivery = 2;
					ucFreeQuotaForNonDelivery = ucFreeQuota - ucFreeQuotaForDelivery;
				} else {
					ucFreeQuotaForNonDelivery = ucFreeQuota;
					ucFreeQuotaForDelivery = 0;
				}
			}

		} else {
			/* !prStaRec->fgIsUapsdSupported */
			ucFreeQuotaForNonDelivery = ucFreeQuota;
			ucFreeQuotaForDelivery = 0;
		}
	}
	/* ucFreeQuota > 0 */
	prStaRec->ucFreeQuotaForDelivery = ucFreeQuotaForDelivery;
	prStaRec->ucFreeQuotaForNonDelivery = ucFreeQuotaForNonDelivery;

	DBGLOG(QM, LOUD, "new QuotaForDelivery = %d  QuotaForNonDelivery = %d\n",
	       prStaRec->ucFreeQuotaForDelivery, prStaRec->ucFreeQuotaForNonDelivery);

}

/*----------------------------------------------------------------------------*/
/*!
* \brief Return the reorder queued RX packets
*
* \param[in] (none)
*
* \return The number of queued RX packets
*/
/*----------------------------------------------------------------------------*/
UINT_32 qmGetRxReorderQueuedBufferCount(IN P_ADAPTER_T prAdapter)
{
	UINT_32 i, u4Total;
	P_QUE_MGT_T prQM = &prAdapter->rQM;

	u4Total = 0;
	/* XXX The summation may impact the performance */
	for (i = 0; i < CFG_NUM_OF_RX_BA_AGREEMENTS; i++) {
		u4Total += prQM->arRxBaTable[i].rReOrderQue.u4NumElem;
#if DBG && 0
		if (QUEUE_IS_EMPTY(&(prQM->arRxBaTable[i].rReOrderQue)))
			ASSERT(prQM->arRxBaTable[i].rReOrderQue == 0);
#endif
	}
	ASSERT(u4Total <= (CFG_NUM_OF_QM_RX_PKT_NUM * 2));
	return u4Total;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Dump current queue status
*
* \param[in] (none)
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
UINT_32 qmDumpQueueStatus(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucBuf, IN UINT_32 u4Max)
{
	P_TX_CTRL_T prTxCtrl;
	P_QUE_MGT_T prQM;
	P_GLUE_INFO_T prGlueInfo;
	UINT_32 i, u4TotalBufferCount, u4TotalPageCount;
	UINT_32 u4CurBufferCount, u4CurPageCount;
	UINT_32 u4Len = 0;

	DEBUGFUNC(("%s", __func__));

	prTxCtrl = &prAdapter->rTxCtrl;
	prQM = &prAdapter->rQM;
	prGlueInfo = prAdapter->prGlueInfo;
	u4TotalBufferCount = 0;
	u4TotalPageCount = 0;
	u4CurBufferCount = 0;
	u4CurPageCount = 0;

	LOGBUF(pucBuf, u4Max, u4Len, "\n");
	LOGBUF(pucBuf, u4Max, u4Len, "------<Dump QUEUE Status>------\n");

	for (i = TC0_INDEX; i < TC_NUM; i++) {
		LOGBUF(pucBuf, u4Max, u4Len, "TC%u ResCount: Max[%02u/%03u] Free[%02u/%03u] PreUsed[%03u]\n",
			i, prTxCtrl->rTc.au4MaxNumOfBuffer[i], prTxCtrl->rTc.au4MaxNumOfPage[i],
			prTxCtrl->rTc.au4FreeBufferCount[i], prTxCtrl->rTc.au4FreePageCount[i],
			prTxCtrl->rTc.au4PreUsedPageCount[i]);

		u4TotalBufferCount += prTxCtrl->rTc.au4MaxNumOfBuffer[i];
		u4TotalPageCount += prTxCtrl->rTc.au4MaxNumOfPage[i];
		u4CurBufferCount += prTxCtrl->rTc.au4FreeBufferCount[i];
		u4CurPageCount += prTxCtrl->rTc.au4FreePageCount[i];
	}

	LOGBUF(pucBuf, u4Max, u4Len, "ToT ResCount: Max[%02u/%03u] Free[%02u/%03u]\n",
		u4TotalBufferCount, u4TotalPageCount, u4CurBufferCount, u4CurPageCount);

	LOGBUF(pucBuf, u4Max, u4Len, "---------------------------------\n");

#if QM_ADAPTIVE_TC_RESOURCE_CTRL
	for (i = TC0_INDEX; i < TC_NUM; i++) {
		LOGBUF(pucBuf, u4Max, u4Len, "TC%u AvgQLen[%04u] minRsv[%02u] CurTcRes[%02u] GrtdTcRes[%02u]\n",
			i, QM_GET_TX_QUEUE_LEN(prAdapter, i), prQM->au4MinReservedTcResource[i],
			prQM->au4CurrentTcResource[i], prQM->au4GuaranteedTcResource[i]);
	}

	LOGBUF(pucBuf, u4Max, u4Len, "Resource Residual[%u] ExtraRsv[%u]\n",
		prQM->u4ResidualTcResource, prQM->u4ExtraReservedTcResource);
	LOGBUF(pucBuf, u4Max, u4Len, "QueLenMovingAvg[%u] Time2AdjResource[%u] Time2UpdateQLen[%u]\n",
		prQM->u4QueLenMovingAverage, prQM->u4TimeToAdjustTcResource, prQM->u4TimeToUpdateQueLen);
#endif

	DBGLOG(SW4, INFO, "---------------------------------\n");

#if QM_FORWARDING_FAIRNESS
	for (i = 0; i < NUM_OF_PER_STA_TX_QUEUES; i++) {
		LOGBUF(pucBuf, u4Max, u4Len, "TC%u HeadSta[%u] ResourceUsedCount[%u]\n",
			i, prQM->au4HeadStaRecIndex[i], prQM->au4ResourceUsedCount[i]);
	}
#endif

	LOGBUF(pucBuf, u4Max, u4Len, "BMC or unknown TxQueue Len[%u]\n", prQM->arTxQueue[0].u4NumElem);
	LOGBUF(pucBuf, u4Max, u4Len, "Pending QLen Normal[%u] Sec[%u] Cmd[%u]\n",
		GLUE_GET_REF_CNT(prGlueInfo->i4TxPendingFrameNum),
		GLUE_GET_REF_CNT(prGlueInfo->i4TxPendingSecurityFrameNum),
		GLUE_GET_REF_CNT(prGlueInfo->i4TxPendingCmdNum));

#if defined(LINUX)
	for (i = 0; i < HW_BSSID_NUM; i++) {
		LOGBUF(pucBuf, u4Max, u4Len, "Pending BSS[%u] QLen[%u:%u:%u:%u]\n", i,
			prGlueInfo->ai4TxPendingFrameNumPerQueue[i][0],
			prGlueInfo->ai4TxPendingFrameNumPerQueue[i][1],
			prGlueInfo->ai4TxPendingFrameNumPerQueue[i][2],
			prGlueInfo->ai4TxPendingFrameNumPerQueue[i][3]);
	}
#endif
	LOGBUF(pucBuf, u4Max, u4Len, "Pending FWD CNT[%d]\n", prTxCtrl->i4PendingFwdFrameCount);
	LOGBUF(pucBuf, u4Max, u4Len, "Pending MGMT CNT[%d]\n", prTxCtrl->i4TxMgmtPendingNum);

	LOGBUF(pucBuf, u4Max, u4Len, "---------------------------------\n");

	LOGBUF(pucBuf, u4Max, u4Len, "Total RFB[%u]\n", CFG_RX_MAX_PKT_NUM);
	LOGBUF(pucBuf, u4Max, u4Len, "rFreeSwRfbList[%u]\n", prAdapter->rRxCtrl.rFreeSwRfbList.u4NumElem);
	LOGBUF(pucBuf, u4Max, u4Len, "rReceivedRfbList[%u]\n", prAdapter->rRxCtrl.rReceivedRfbList.u4NumElem);
	LOGBUF(pucBuf, u4Max, u4Len, "rIndicatedRfbList[%u]\n", prAdapter->rRxCtrl.rIndicatedRfbList.u4NumElem);
	LOGBUF(pucBuf, u4Max, u4Len, "ucNumIndPacket[%u]\n", prAdapter->rRxCtrl.ucNumIndPacket);
	LOGBUF(pucBuf, u4Max, u4Len, "ucNumRetainedPacket[%u]\n", prAdapter->rRxCtrl.ucNumRetainedPacket);

	LOGBUF(pucBuf, u4Max, u4Len, "---------------------------------\n");
	LOGBUF(pucBuf, u4Max, u4Len, "CMD: Free[%u/%u] Cmd[%u] ToTx[%u]\n",
		prAdapter->rFreeCmdList.u4NumElem, CFG_TX_MAX_CMD_PKT_NUM,
		prAdapter->rPendingCmdQueue.u4NumElem, prGlueInfo->rCmdQueue.u4NumElem);
	LOGBUF(pucBuf, u4Max, u4Len, "MSDU: Free[%u/%u] Pending[%u] Done[%u]\n",
		prAdapter->rTxCtrl.rFreeMsduInfoList.u4NumElem, CFG_TX_MAX_PKT_NUM,
		prAdapter->rTxCtrl.rTxMgmtTxingQueue.u4NumElem,
		prAdapter->rTxDataDoneQueue.u4NumElem);

	LOGBUF(pucBuf, u4Max, u4Len, "---------------------------------\n");

	return u4Len;
}

#if CFG_M0VE_BA_TO_DRIVER
/*----------------------------------------------------------------------------*/
/*!
* @brief Send DELBA Action frame
*
* @param fgIsInitiator DELBA_ROLE_INITIATOR or DELBA_ROLE_RECIPIENT
* @param prStaRec Pointer to the STA_REC of the receiving peer
* @param u4Tid TID of the BA entry
* @param u4ReasonCode The reason code carried in the Action frame
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID
mqmSendDelBaFrame(IN P_ADAPTER_T prAdapter,
		  IN BOOLEAN fgIsInitiator, IN P_STA_RECORD_T prStaRec, IN UINT_32 u4Tid, IN UINT_32 u4ReasonCode)
{

	P_MSDU_INFO_T prTxMsduInfo;
	P_ACTION_DELBA_FRAME_T prDelBaFrame;
	P_BSS_INFO_T prBssInfo;

	DBGLOG(QM, WARN, "[Puff]: Enter mqmSendDelBaFrame()\n");

	ASSERT(prStaRec);

	/* 3 <1> Block the message in case of invalid STA */
	if (!prStaRec->fgIsInUse) {
		DBGLOG(QM, WARN, "[Puff][%s]: (Warning) sta_rec is not inuse\n", __func__);
		return;
	}
	/* Check HT-capabale STA */
	if (!(prStaRec->ucDesiredPhyTypeSet & PHY_TYPE_BIT_HT)) {
		DBGLOG(QM, WARN,
		       "[Puff][%s]: (Warning) sta is NOT HT-capable(0x%08X)\n", __func__,
		       prStaRec->ucDesiredPhyTypeSet);
		return;
	}
	/* 4 <2> Construct the DELBA frame */
	prTxMsduInfo = (P_MSDU_INFO_T) cnmMgtPktAlloc(prAdapter, ACTION_DELBA_FRAME_LEN);

	if (!prTxMsduInfo) {
		DBGLOG(QM, WARN,
		       "[Puff][%s]: (Warning) DELBA for TID=%ld was not sent (MSDU_INFO alloc failure)\n",
		       __func__, u4Tid);
		return;
	}

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);

	/* Fill the Action frame */
	prDelBaFrame = (P_ACTION_DELBA_FRAME_T) ((UINT_32) (prTxMsduInfo->prPacket) + MAC_TX_RESERVED_FIELD);
	prDelBaFrame->u2FrameCtrl = MAC_FRAME_ACTION;
#if CFG_SUPPORT_802_11W
	if (rsnCheckBipKeyInstalled(prAdapter, prStaRec)) {
		DBGLOG(QM, WARN, "[Puff][%s]: (Warning) DELBA is 80211w enabled\n", __func__);
		prDelBaFrame->u2FrameCtrl |= MASK_FC_PROTECTED_FRAME;
	}
#endif

	prDelBaFrame->u2DurationID = 0;
	prDelBaFrame->ucCategory = CATEGORY_BLOCK_ACK_ACTION;
	prDelBaFrame->ucAction = ACTION_DELBA;

	prDelBaFrame->u2DelBaParameterSet = 0;
	prDelBaFrame->u2DelBaParameterSet |= ((fgIsInitiator ? ACTION_DELBA_INITIATOR_MASK : 0));
	prDelBaFrame->u2DelBaParameterSet |= ((u4Tid << ACTION_DELBA_TID_OFFSET) & ACTION_DELBA_TID_MASK);
	prDelBaFrame->u2ReasonCode = u4ReasonCode;

	COPY_MAC_ADDR(prDelBaFrame->aucDestAddr, prStaRec->aucMacAddr);
	COPY_MAC_ADDR(prDelBaFrame->aucSrcAddr, prBssInfo->aucOwnMacAddr);
	COPY_MAC_ADDR(prDelBaFrame->aucBSSID, prBssInfo->aucBSSID);

	/* 4 <3> Configure the MSDU_INFO and forward it to TXM */
	TX_SET_MMPDU(prAdapter,
		     prTxMsduInfo,
		     prStaRec->ucBssIndex,
		     (prStaRec != NULL) ? (prStaRec->ucIndex) : (STA_REC_INDEX_NOT_FOUND),
		     WLAN_MAC_HEADER_LEN, ACTION_DELBA_FRAME_LEN, NULL, MSDU_RATE_MODE_AUTO);

#if CFG_SUPPORT_802_11W
	if (rsnCheckBipKeyInstalled(prAdapter, prStaRec)) {
		DBGLOG(RSN, INFO, "Set MSDU_OPT_PROTECTED_FRAME\n");
		nicTxConfigPktOption(prTxMsduInfo, MSDU_OPT_PROTECTED_FRAME, TRUE);
	}
#endif

	/* TID and fgIsInitiator are needed when processing TX Done of the DELBA frame */
	prTxMsduInfo->ucTID = (UINT_8) u4Tid;
	prTxMsduInfo->ucControlFlag = (fgIsInitiator ? 1 : 0);

	nicTxEnqueueMsdu(prAdapter, prTxMsduInfo);

	DBGLOG(QM, WARN, "[Puff][%s]: Send DELBA for TID=%ld Initiator=%d\n", __func__, u4Tid, fgIsInitiator);
}

/*----------------------------------------------------------------------------*/
/*!
* @brief Callback function for the TX Done event for an ADDBA_RSP
*
* @param prMsduInfo The TX packet
* @param rWlanStatus WLAN_STATUS_SUCCESS if TX is successful
*
* @return WLAN_STATUS_BUFFER_RETAINED is returned if the buffer shall not be freed by TXM
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
mqmCallbackAddBaRspSent(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_RESULT_CODE_T rTxDoneStatus)
{
	P_RX_BA_ENTRY_T prRxBaEntry;
	P_STA_RECORD_T prStaRec;
	P_QUE_MGT_T prQM;

	UINT_32 u4Tid = 0;

	/* ASSERT(prMsduInfo); */
	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);
	ASSERT(prStaRec);

	prQM = &prAdapter->rQM;

	DBGLOG(QM, WARN, "[Puff]: Enter mqmCallbackAddBaRspSent()\n");

	/* 4 <0> Check STA_REC status */
	/* Check STA_REC is inuse */
	if (!prStaRec->fgIsInUse) {
		DBGLOG(QM, WARN, "[Puff][%s]: (Warning) sta_rec is not inuse\n", __func__);
		return WLAN_STATUS_SUCCESS;
	}
	/* Check HT-capabale STA */
	if (!(prStaRec->ucDesiredPhyTypeSet & PHY_TYPE_BIT_HT)) {
		DBGLOG(QM, WARN,
		       "[Puff][%s]: (Warning) sta is NOT HT-capable(0x%08X)\n", __func__,
		       prStaRec->ucDesiredPhyTypeSet);
		return WLAN_STATUS_SUCCESS;	/* To free the received ADDBA_REQ directly */
	}
	/* 4 <1> Find the corresponding BA entry */
	u4Tid = prMsduInfo->ucTID;	/* TID is stored in MSDU_INFO when composing the ADDBA_RSP frame */
	prRxBaEntry = &prQM->arRxBaTable[u4Tid];

	/* Note: Due to some reason, for example, receiving a DELBA, the BA entry may not be in state NEGO */
	/* 4 <2> INVALID state */
	if (!prRxBaEntry) {
		DBGLOG(QM, WARN,
		       "[Puff][%s]: (RX_BA) ADDBA_RSP ---> peer (STA=%d TID=%d)(TX successful)(invalid BA)\n",
		       __func__, prStaRec->ucIndex, u4Tid);
	}
	/* 4 <3> NEGO, ACTIVE, or DELETING state */
	else {
		switch (rTxDoneStatus) {
			/* 4 <Case 1> TX Success */
		case TX_RESULT_SUCCESS:

			DBGLOG(QM, WARN,
			       "[Puff][%s]: (RX_BA) ADDBA_RSP ---> peer (STA=%d TID=%d)(TX successful)\n",
			       __func__, prStaRec->ucIndex, u4Tid);

			/* 4 <Case 1.1> NEGO or ACTIVE state */
			if (prRxBaEntry->ucStatus != BA_ENTRY_STATUS_DELETING)
				mqmRxModifyBaEntryStatus(prAdapter, prRxBaEntry, BA_ENTRY_STATUS_ACTIVE);
			/* 4 <Case 1.2> DELETING state */
			/* else */
			/* Deleting is on-going, so do nothing and wait for TX done of the DELBA frame */

			break;

			/* 4 <Case 2> TX Failure */
		default:

			DBGLOG(QM, WARN,
			       "[Puff][%s]: (RX_BA) ADDBA_RSP ---> peer (STA=%d TID=%ld Entry_Status=%d)(TX failed)\n",
			       __func__, prStaRec->ucIndex, u4Tid, prRxBaEntry->ucStatus);

			/* 4 <Case 2.1> NEGO or ACTIVE state */
			/* Notify the host to delete the agreement */
			if (prRxBaEntry->ucStatus != BA_ENTRY_STATUS_DELETING) {
				mqmRxModifyBaEntryStatus(prAdapter, prRxBaEntry, BA_ENTRY_STATUS_DELETING);

				/* Send DELBA to the peer to ensure the BA state is synchronized */
				mqmSendDelBaFrame(prAdapter, DELBA_ROLE_RECIPIENT, prStaRec, u4Tid,
						  STATUS_CODE_UNSPECIFIED_FAILURE);
			}
			/* 4 <Case 2.2> DELETING state */
			/* else */
			/* Deleting is on-going, so do nothing and wait for the TX done of the DELBA frame */

			break;
		}

	}

	return WLAN_STATUS_SUCCESS;	/* TXM shall release the packet */

}

/*----------------------------------------------------------------------------*/
/*!
* @brief Check if there is any idle RX BA
*
* @param u4Param (not used)
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID mqmTimeoutCheckIdleRxBa(IN P_ADAPTER_T prAdapter, IN ULONG ulParamPtr)
{
	INT_8 i;
	P_RX_BA_ENTRY_T prRxBa;
	UINT_32 u4IdleCountThreshold = 0;
	P_STA_RECORD_T prStaRec;
	P_QUE_MGT_T prQM;

	DBGLOG(QM, WARN, "[Puff]: Enter mqmTimeoutIdleRxBaDetection()\n");

	prQM = &prAdapter->rQM;

	/* 4 <1> Restart the timer */
	cnmTimerStopTimer(prAdapter, &prAdapter->rMqmIdleRxBaDetectionTimer);
	cnmTimerStartTimer(prAdapter, &prAdapter->rMqmIdleRxBaDetectionTimer, MQM_IDLE_RX_BA_CHECK_INTERVAL);

	/* 4 <2> Increment the idle count for each idle BA */
	for (i = 0; i < CFG_NUM_OF_RX_BA_AGREEMENTS; i++) {

		prRxBa = &prQM->arRxBaTable[i];

		if (prRxBa->ucStatus == BA_ENTRY_STATUS_ACTIVE) {

			prStaRec = cnmGetStaRecByIndex(prAdapter, prRxBa->ucStaRecIdx);

			if (!prStaRec->fgIsInUse) {
				DBGLOG(QM, WARN, "[Puff][%s]: (Warning) sta_rec is not inuse\n", __func__);
				ASSERT(0);
			}
			/* Check HT-capabale STA */
			if (!(prStaRec->ucDesiredPhyTypeSet & PHY_TYPE_BIT_HT)) {
				DBGLOG(QM, WARN,
				       "[Puff][%s]: (Warning) sta is NOT HT-capable(0x%08X)\n",
				       __func__, prStaRec->ucDesiredPhyTypeSet);
				ASSERT(0);
			}
			/* 4 <2.1>  Idle detected, increment idle count and see if a DELBA should be sent */
			if (prRxBa->u2SnapShotSN == prStaRec->au2CachedSeqCtrl[prRxBa->ucTid]) {

				prRxBa->ucIdleCount++;

				ASSERT(prRxBa->ucTid < 8);
				switch (aucTid2ACI[prRxBa->ucTid]) {
				case 0:	/* BK */
					u4IdleCountThreshold = MQM_DEL_IDLE_RXBA_THRESHOLD_BK;
					break;
				case 1:	/* BE */
					u4IdleCountThreshold = MQM_DEL_IDLE_RXBA_THRESHOLD_BE;
					break;
				case 2:	/* VI */
					u4IdleCountThreshold = MQM_DEL_IDLE_RXBA_THRESHOLD_VI;
					break;
				case 3:	/* VO */
					u4IdleCountThreshold = MQM_DEL_IDLE_RXBA_THRESHOLD_VO;
					break;
				}

				if (prRxBa->ucIdleCount >= u4IdleCountThreshold) {
					mqmRxModifyBaEntryStatus(prAdapter, prRxBa, BA_ENTRY_STATUS_INVALID);
					mqmSendDelBaFrame(prAdapter, DELBA_ROLE_RECIPIENT, prStaRec,
							  (UINT_32) prRxBa->ucTid, REASON_CODE_PEER_TIME_OUT);
					qmDelRxBaEntry(prAdapter, prStaRec->ucIndex, prRxBa->ucTid, TRUE);
				}
			}
			/* 4 <2.2> Activity detected */
			else {
				prRxBa->u2SnapShotSN = prStaRec->au2CachedSeqCtrl[prRxBa->ucTid];
				prRxBa->ucIdleCount = 0;
				continue;	/* check the next BA entry */
			}
		}
	}

}

/*----------------------------------------------------------------------------*/
/*!
* @brief Do RX BA entry state transition
*
* @param prRxBaEntry The BA entry pointer
* @param eStatus The state to transition to
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID
mqmRxModifyBaEntryStatus(IN P_ADAPTER_T prAdapter, IN P_RX_BA_ENTRY_T prRxBaEntry, IN ENUM_BA_ENTRY_STATUS_T eStatus)
{
	P_STA_RECORD_T prStaRec;
	P_QUE_MGT_T prQM;

	BOOLEAN fgResetScoreBoard = FALSE;

	ASSERT(prRxBaEntry);

	prStaRec = cnmGetStaRecByIndex(prAdapter, prRxBaEntry->ucStaRecIdx);
	ASSERT(prStaRec);
	prQM = &prAdapter->rQM;

	if (prRxBaEntry->ucStatus == (UINT_8) eStatus) {
		DBGLOG(QM, WARN, "[Puff][%s]: eStatus are identical...\n", __func__, prRxBaEntry->ucStatus);
		return;
	}
	/* 4 <1> State transition from state X */
	switch (prRxBaEntry->ucStatus) {

		/* 4 <1.1> From (X = INVALID) to (ACTIVE or NEGO or DELETING) */
	case BA_ENTRY_STATUS_INVALID:

		/* Associate the BA entry with the STA_REC when leaving INVALID state */
		kalMemCopy(&prQM->arRxBaTable[prRxBaEntry->ucTid], prRxBaEntry, sizeof(RX_BA_ENTRY_T));

		/* Increment the RX BA counter */
		prQM->ucRxBaCount++;
		ASSERT(prQM->ucRxBaCount <= CFG_NUM_OF_RX_BA_AGREEMENTS);

		/* Since AMPDU may be received during INVALID state */
		fgResetScoreBoard = TRUE;

		/* Reset Idle Count since this BA entry is being activated now.
		 *  Note: If there is no ACTIVE entry, the idle detection timer will not be started.
		 */
		prRxBaEntry->ucIdleCount = 0;
		break;

		/* 4 <1.2> Other cases */
	default:
		break;
	}

	/* 4 <2> State trasition to state Y */
	switch (eStatus) {

		/* 4 <2.1> From  (NEGO, ACTIVE, DELETING) to (Y=INVALID) */
	case BA_ENTRY_STATUS_INVALID:

		/* Disassociate the BA entry with the STA_REC */
		kalMemZero(&prQM->arRxBaTable[prRxBaEntry->ucTid], sizeof(RX_BA_ENTRY_T));

		/* Decrement the RX BA counter */
		prQM->ucRxBaCount--;
		ASSERT(prQM->ucRxBaCount < CFG_NUM_OF_RX_BA_AGREEMENTS);

		/* (TBC) */
		fgResetScoreBoard = TRUE;

		/* If there is not any BA agreement, stop doing idle detection  */
		if (prQM->ucRxBaCount == 0) {
			if (MQM_CHECK_FLAG(prAdapter->u4FlagBitmap, MQM_FLAG_IDLE_RX_BA_TIMER_STARTED)) {
				cnmTimerStopTimer(prAdapter, &prAdapter->rMqmIdleRxBaDetectionTimer);
				MQM_CLEAR_FLAG(prAdapter->u4FlagBitmap, MQM_FLAG_IDLE_RX_BA_TIMER_STARTED);
			}
		}

		break;

		/* 4 <2.2> From  (any) to (Y=ACTIVE) */
	case BA_ENTRY_STATUS_ACTIVE:

		/* If there is at least one BA going into ACTIVE, start idle detection */
		if (!MQM_CHECK_FLAG(prAdapter->u4FlagBitmap, MQM_FLAG_IDLE_RX_BA_TIMER_STARTED)) {
			cnmTimerInitTimer(prAdapter, &prAdapter->rMqmIdleRxBaDetectionTimer,
					(PFN_MGMT_TIMEOUT_FUNC) mqmTimeoutCheckIdleRxBa, (ULONG) NULL);
			/* No parameter */

			cnmTimerStopTimer(prAdapter, &prAdapter->rMqmIdleRxBaDetectionTimer);

#if MQM_IDLE_RX_BA_DETECTION
			cnmTimerStartTimer(prAdapter, &prAdapter->rMqmIdleRxBaDetectionTimer,
					   MQM_IDLE_RX_BA_CHECK_INTERVAL);
			MQM_SET_FLAG(prAdapter->u4FlagBitmap, MQM_FLAG_IDLE_RX_BA_TIMER_STARTED);
#endif
		}

		break;

	case BA_ENTRY_STATUS_NEGO:
	default:
		break;
	}

	if (fgResetScoreBoard) {
		P_CMD_RESET_BA_SCOREBOARD_T prCmdBody;

		prCmdBody = (P_CMD_RESET_BA_SCOREBOARD_T)
		    cnmMemAlloc(prAdapter, RAM_TYPE_BUF, sizeof(CMD_RESET_BA_SCOREBOARD_T));
		ASSERT(prCmdBody);

		prCmdBody->ucflag = MAC_ADDR_TID_MATCH;
		prCmdBody->ucTID = prRxBaEntry->ucTid;
		kalMemCopy(prCmdBody->aucMacAddr, prStaRec->aucMacAddr, PARAM_MAC_ADDR_LEN);

		wlanoidResetBAScoreboard(prAdapter, prCmdBody, sizeof(CMD_RESET_BA_SCOREBOARD_T));

	}

	DBGLOG(QM, WARN, "[Puff]QM: (RX_BA) [STA=%d TID=%d] status from %d to %d\n",
	       prRxBaEntry->ucStaRecIdx, prRxBaEntry->ucTid, prRxBaEntry->ucStatus, eStatus);

	prRxBaEntry->ucStatus = (UINT_8) eStatus;

}

/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID mqmHandleAddBaReq(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
	P_STA_RECORD_T prStaRec;
	P_BSS_INFO_T prBssInfo;
	P_ACTION_ADDBA_REQ_FRAME_T prAddBaReq;
	ACTION_ADDBA_REQ_BODY_T rAddBaReqBody;
	P_ACTION_ADDBA_RSP_FRAME_T prAddBaRsp;
	ACTION_ADDBA_RSP_BODY_T rAddBaRspBody;
	P_RX_BA_ENTRY_T prRxBaEntry;
	P_MSDU_INFO_T prTxMsduInfo;
	P_QUE_MGT_T prQM;

	BOOLEAN fgIsReqAccepted = TRUE;	/* Reject or accept the ADDBA_REQ */
	BOOLEAN fgIsNewEntryAdded = FALSE;	/* Indicator: Whether a new RX BA entry will be added */

	UINT_32 u4Tid;
	UINT_32 u4StaRecIdx;
	UINT_16 u2WinStart;
	UINT_16 u2WinSize;
	UINT_32 u4BuffSize;

#if CFG_SUPPORT_BCM
	UINT_32 u4BuffSizeBT;
#endif

	ASSERT(prSwRfb);

	prStaRec = prSwRfb->prStaRec;
	prQM = &prAdapter->rQM;

	do {

		/* 4 <0> Check if this is an active HT-capable STA */
		/* Check STA_REC is inuse */
		if (!prStaRec->fgIsInUse) {
			DBGLOG(QM, WARN, "[Puff][%s]: (Warning) sta_rec is not inuse\n", __func__);
			break;
		}
		/* Check HT-capabale STA */
		if (!(prStaRec->ucDesiredPhyTypeSet & PHY_TYPE_BIT_HT)) {
			DBGLOG(QM, WARN,
			       "[Puff][%s]: (Warning) sta is NOT HT-capable(0x%08X)\n", __func__,
			       prStaRec->ucDesiredPhyTypeSet);
			break;	/* To free the received ADDBA_REQ directly */
		}
		/* 4 <1> Check user configurations and HW capabilities */
		/* Check configurations (QoS support, AMPDU RX support) */
		if ((!prAdapter->rWifiVar.fgSupportQoS) ||
		    (!prAdapter->rWifiVar.fgSupportAmpduRx) || (!prStaRec->fgRxAmpduEn)) {
			DBGLOG(QM, WARN,
			       "[Puff][%s]: (Warning) BA ACK Policy not supported fgSupportQoS(%d)",
			       __func__, prAdapter->rWifiVar.fgSupportQoS);
			DBGLOG(QM, WARN,
			       "fgSupportAmpduRx(%d), fgRxAmpduEn(%d)\n",
			       prAdapter->rWifiVar.fgSupportAmpduRx, prStaRec->fgRxAmpduEn);
			fgIsReqAccepted = FALSE;	/* Will send an ADDBA_RSP with DECLINED */
		}
		/* Check capability */
		prAddBaReq = ((P_ACTION_ADDBA_REQ_FRAME_T) (prSwRfb->pvHeader));
		kalMemCopy((PUINT_8) (&rAddBaReqBody), (PUINT_8) (&(prAddBaReq->aucBAParameterSet[0])), 6);
		if ((((rAddBaReqBody.u2BAParameterSet) & BA_PARAM_SET_ACK_POLICY_MASK) >>
		     BA_PARAM_SET_ACK_POLICY_MASK_OFFSET)
		    != BA_PARAM_SET_ACK_POLICY_IMMEDIATE_BA) {	/* Only Immediate_BA is supported */
			DBGLOG(QM, WARN,
			       "[Puff][%s]: (Warning) BA ACK Policy not supported (0x%08X)\n",
			       __func__, rAddBaReqBody.u2BAParameterSet);
			fgIsReqAccepted = FALSE;	/* Will send an ADDBA_RSP with DECLINED */
		}

		/* 4 <2> Determine the RX BA entry (existing or to be added) */
		/* Note: BA entry index = (TID, STA_REC index) */
		u4Tid = (((rAddBaReqBody.u2BAParameterSet) & BA_PARAM_SET_TID_MASK) >> BA_PARAM_SET_TID_MASK_OFFSET);
		u4StaRecIdx = prStaRec->ucIndex;
		DBGLOG(QM, WARN,
		       "[Puff][%s]: BA entry index = [TID(%d), STA_REC index(%d)]\n", __func__, u4Tid, u4StaRecIdx);

		u2WinStart = ((rAddBaReqBody.u2BAStartSeqCtrl) >> OFFSET_BAR_SSC_SN);
		u2WinSize = (((rAddBaReqBody.u2BAParameterSet) & BA_PARAM_SET_BUFFER_SIZE_MASK)
			     >> BA_PARAM_SET_BUFFER_SIZE_MASK_OFFSET);
		DBGLOG(QM, WARN,
		       "[Puff][%s]: BA entry info = [WinStart(%d), WinSize(%d)]\n", __func__, u2WinStart, u2WinSize);

		if (fgIsReqAccepted) {

			prRxBaEntry = &prQM->arRxBaTable[u4Tid];

			if (!prRxBaEntry) {

				/* 4 <Case 2.1> INVALID state && BA entry available --> Add a new entry and accept */
				if (prQM->ucRxBaCount < CFG_NUM_OF_RX_BA_AGREEMENTS) {

					fgIsNewEntryAdded = qmAddRxBaEntry(prAdapter,
									   (UINT_8) u4StaRecIdx,
									   (UINT_8) u4Tid, u2WinStart, u2WinSize);

					if (!fgIsNewEntryAdded) {
						DBGLOG(QM, ERROR,
						       "[Puff][%s]: (Error) Free RX BA entry alloc failure\n");
						fgIsReqAccepted = FALSE;
					} else {
						DBGLOG(QM, WARN, "[Puff][%s]: Create a new BA Entry\n");
					}
				}
				/* 4 <Case 2.2> INVALID state && BA entry unavailable --> Reject the ADDBA_REQ */
				else {
					DBGLOG(QM, WARN,
					       "[Puff][%s]: (Warning) Free RX BA entry unavailable(req: %d)\n",
					       __func__, prQM->ucRxBaCount);
					fgIsReqAccepted = FALSE;	/* Will send an ADDBA_RSP with DECLINED */
				}
			} else {

				/* 4 <Case 2.3> NEGO or DELETING  state --> Ignore the ADDBA_REQ */
				/* For NEGO: do nothing. Wait for TX Done of ADDBA_RSP */
				/* For DELETING: do nothing. Wait for TX Done of DELBA */
				if (prRxBaEntry->ucStatus != BA_ENTRY_STATUS_ACTIVE) {
					DBGLOG(QM, WARN,
					       "[Puff][%s]:(Warning)ADDBA_REQ for TID=%ld is received, status:%d)\n",
					       __func__, u4Tid, prRxBaEntry->ucStatus);
					break;	/* Ignore the ADDBA_REQ since the current state is NEGO */
				}
				/* 4 <Case 2.4> ACTIVE state --> Accept */
				/* Send an ADDBA_RSP to accept the request again */
				/* else */
			}
		}
		/* 4 <3> Construct the ADDBA_RSP frame */
		prTxMsduInfo = (P_MSDU_INFO_T) cnmMgtPktAlloc(prAdapter, ACTION_ADDBA_RSP_FRAME_LEN);
		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);

		if (!prTxMsduInfo) {

			/* The peer may send an ADDBA_REQ message later.
			 *  Do nothing to the BA entry. No DELBA will be sent (because cnmMgtPktAlloc() may fail again).
			 *  No BA deletion event will be sent to the host (because cnmMgtPktAlloc() may fail again).
			 */
			DBGLOG(QM, WARN, "[Puff][%s]: (Warning) ADDBA_RSP alloc failure\n", __func__);

			if (fgIsNewEntryAdded) {
				/* If a new entry has been created due to this ADDBA_REQ, delete it */
				ASSERT(prRxBaEntry);
				mqmRxModifyBaEntryStatus(prAdapter, prRxBaEntry, BA_ENTRY_STATUS_INVALID);
			}

			break;	/* Exit directly to free the ADDBA_REQ */
		}

		/* Fill the ADDBA_RSP message */
		prAddBaRsp = (P_ACTION_ADDBA_RSP_FRAME_T) ((UINT_32) (prTxMsduInfo->prPacket) + MAC_TX_RESERVED_FIELD);
		prAddBaRsp->u2FrameCtrl = MAC_FRAME_ACTION;

#if CFG_SUPPORT_802_11W
		if (rsnCheckBipKeyInstalled(prAdapter, prStaRec)) {
			DBGLOG(QM, WARN, "[Puff][%s]: (Warning) ADDBA_RSP is 80211w enabled\n", __func__);
			prAddBaReq->u2FrameCtrl |= MASK_FC_PROTECTED_FRAME;
		}
#endif
		prAddBaRsp->u2DurationID = 0;
		prAddBaRsp->ucCategory = CATEGORY_BLOCK_ACK_ACTION;
		prAddBaRsp->ucAction = ACTION_ADDBA_RSP;
		prAddBaRsp->ucDialogToken = prAddBaReq->ucDialogToken;

		DBGLOG(QM, WARN,
		       "[Puff][%s]: (Warning) ADDBA_RSP DurationID(%d) Category(%d) Action(%d) DialogToken(%d)\n",
		       __func__, prAddBaRsp->u2DurationID, prAddBaRsp->ucCategory,
		       prAddBaRsp->ucAction, prAddBaRsp->ucDialogToken);

		if (fgIsReqAccepted)
			rAddBaRspBody.u2StatusCode = STATUS_CODE_SUCCESSFUL;
		else
			rAddBaRspBody.u2StatusCode = STATUS_CODE_REQ_DECLINED;

		/* WinSize = min(WinSize in ADDBA_REQ, CFG_RX_BA_MAX_WINSIZE) */
		u4BuffSize = (((rAddBaReqBody.u2BAParameterSet) & BA_PARAM_SET_BUFFER_SIZE_MASK)
			      >> BA_PARAM_SET_BUFFER_SIZE_MASK_OFFSET);

		/*If ADDBA req WinSize<=0 => use default WinSize(16) */
		if ((u4BuffSize > CFG_RX_BA_MAX_WINSIZE) || (u4BuffSize <= 0))
			u4BuffSize = CFG_RX_BA_MAX_WINSIZE;
#if CFG_SUPPORT_BCM
		/* TODO: Call BT coexistence function to limit the winsize */
		u4BuffSizeBT = bcmRequestBaWinSize();
		DBGLOG(QM, WARN, "[Puff][%s]: (Warning) bcmRequestBaWinSize(%d)\n", __func__, u4BuffSizeBT);

		if (u4BuffSize > u4BuffSizeBT)
			u4BuffSize = u4BuffSizeBT;
#endif /* CFG_SUPPORT_BCM */

		rAddBaRspBody.u2BAParameterSet = (BA_POLICY_IMMEDIATE |
						  (u4Tid << BA_PARAM_SET_TID_MASK_OFFSET) |
						  (u4BuffSize << BA_PARAM_SET_BUFFER_SIZE_MASK_OFFSET));

		/* TODO: Determine the BA timeout value according to the default preference */
		rAddBaRspBody.u2BATimeoutValue = rAddBaReqBody.u2BATimeoutValue;

		DBGLOG(QM, WARN,
		       "[Puff][%s]: (Warning) ADDBA_RSP u4BuffSize(%d) StatusCode(%d)",
		       __func__, u4BuffSize, rAddBaRspBody.u2StatusCode);
		DBGLOG(QM, WARN,
		       "BAParameterSet(0x%08X) BATimeoutValue(%d)\n",
		       rAddBaRspBody.u2BAParameterSet, rAddBaRspBody.u2BATimeoutValue);
		kalMemCopy((PUINT_8) (&(prAddBaRsp->aucStatusCode[0])), (PUINT_8) (&rAddBaRspBody), 6);

		COPY_MAC_ADDR(prAddBaRsp->aucDestAddr, prStaRec->aucMacAddr);
		COPY_MAC_ADDR(prAddBaRsp->aucSrcAddr, prBssInfo->aucOwnMacAddr);
		/* COPY_MAC_ADDR(prAddBaRsp->aucBSSID,g_aprBssInfo[prStaRec->ucNetTypeIndex]->aucBSSID); */
		COPY_MAC_ADDR(prAddBaRsp->aucBSSID, prAddBaReq->aucBSSID);

		/* 4 <4> Forward the ADDBA_RSP to TXM */
		TX_SET_MMPDU(prAdapter,
			     prTxMsduInfo,
			     prStaRec->ucBssIndex,
			     (prStaRec != NULL) ? (prStaRec->ucIndex) : (STA_REC_INDEX_NOT_FOUND),
			     WLAN_MAC_HEADER_LEN,
			     ACTION_ADDBA_RSP_FRAME_LEN, mqmCallbackAddBaRspSent, MSDU_RATE_MODE_AUTO);

#if CFG_SUPPORT_802_11W
		if (rsnCheckBipKeyInstalled(prAdapter, prStaRec)) {
			DBGLOG(RSN, INFO, "Set MSDU_OPT_PROTECTED_FRAME\n");
			nicTxConfigPktOption(prTxMsduInfo, MSDU_OPT_PROTECTED_FRAME, TRUE);
		}
#endif

		/* Note: prTxMsduInfo->ucTID is not used for transmitting the ADDBA_RSP.
		 *  However, when processing TX Done of this ADDBA_RSP, the TID value is needed, so
		 *  store the TID value in advance to prevent parsing the ADDBA_RSP frame
		 */
		prTxMsduInfo->ucTID = (UINT_8) u4Tid;

		nicTxEnqueueMsdu(prAdapter, prTxMsduInfo);

		DBGLOG(QM, WARN,
		       "[Puff][%s]: (RX_BA) ADDBA_RSP ---> peer (STA=%d TID=%ld)\n", __func__,
		       prStaRec->ucIndex, u4Tid);

#if 0
		/* 4 <5> Notify the host to start buffer reordering */
		if (fgIsNewEntryAdded) {	/* Only when a new BA entry is indeed added will the host be notified */
			ASSERT(fgIsReqAccepted);

			prSwRfbEventToHost = (P_SW_RFB_T) cnmMgtPktAlloc(EVENT_RX_ADDBA_PACKET_LEN);

			if (!prSwRfbEventToHost) {

				/* Note: DELBA will not be sent since cnmMgtPktAlloc() may fail again. However,
				 * it does not matter because upon receipt of AMPDUs without a RX BA agreement,
				 * MQM will send DELBA frames
				 */

				DBGLOG(MQM, WARN, "MQM: (Warning) EVENT packet alloc failed\n");

				/* Ensure that host and FW are synchronized */
				mqmRxModifyBaEntryStatus(prRxBaEntry, BA_ENTRY_STATUS_INVALID);

				break;	/* Free the received ADDBA_REQ */
			}
				prEventRxAddBa = (P_EVENT_RX_ADDBA_T) prSwRfbEventToHost->pucBuffer;
				prEventRxAddBa->ucStaRecIdx = (UINT_8) u4StaRecIdx;
				prEventRxAddBa->u2Length = EVENT_RX_ADDBA_PACKET_LEN;
				prEventRxAddBa->ucEID = EVENT_ID_RX_ADDBA;
				prEventRxAddBa->ucSeqNum = 0;	/* Unsolicited event packet */
				prEventRxAddBa->u2BAParameterSet = rAddBaRspBody.u2BAParameterSet;
				prEventRxAddBa->u2BAStartSeqCtrl = rAddBaReqBody.u2BAStartSeqCtrl;
				prEventRxAddBa->u2BATimeoutValue = rAddBaReqBody.u2BATimeoutValue;
				prEventRxAddBa->ucDialogToken = prAddBaReq->ucDialogToken;

				DBGLOG(MQM, INFO,
				       "MQM: (RX_BA) Event ADDBA ---> driver (STA=%ld TID=%ld WinStart=%d)\n",
				       u4StaRecIdx, u4Tid, (prEventRxAddBa->u2BAStartSeqCtrl >> 4));

				/* Configure the SW_RFB for the Event packet */
				RXM_SET_EVENT_PACKET(
							    /* P_SW_RFB_T */ (P_SW_RFB_T)
							    prSwRfbEventToHost,
							    /* HIF RX Packet pointer */
							    (PUINT_8) prEventRxAddBa,
							    /* HIF RX port number */ HIF_RX0_INDEX
				    );

				rxmSendEventToHost(prSwRfbEventToHost);


		}
#endif

	} while (FALSE);

}

/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID mqmHandleAddBaRsp(IN P_SW_RFB_T prSwRfb)
{

}

/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID mqmHandleDelBa(IN P_SW_RFB_T prSwRfb)
{

}

/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID mqmHandleBaActionFrame(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
	P_WLAN_ACTION_FRAME prRxFrame;

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prRxFrame = (P_WLAN_ACTION_FRAME) prSwRfb->pvHeader;
	DBGLOG(RLM, WARN, "[Puff][%s] Action(%d)\n", __func__, prRxFrame->ucAction);

	switch (prRxFrame->ucAction) {

	case ACTION_ADDBA_REQ:
		DBGLOG(RLM, WARN, "[Puff][%s] (RX_BA) ADDBA_REQ <--- peer\n", __func__);
		mqmHandleAddBaReq(prAdapter, prSwRfb);
		break;

	case ACTION_ADDBA_RSP:
		DBGLOG(RLM, WARN, "[Puff][%s] (RX_BA) ADDBA_RSP <--- peer\n", __func__);
		mqmHandleAddBaRsp(prSwRfb);
		break;

	case ACTION_DELBA:
		DBGLOG(RLM, WARN, "[Puff][%s] (RX_BA) DELBA <--- peer\n", __func__);
		mqmHandleDelBa(prSwRfb);
		break;

	default:
		DBGLOG(RLM, WARN, "[Puff][%s] Unknown BA Action Frame\n", __func__);
		break;
	}

}

#endif

#if QM_ADAPTIVE_TC_RESOURCE_CTRL
VOID qmResetTcControlResource(IN P_ADAPTER_T prAdapter)
{
	UINT_32 u4Idx;
	UINT_32 u4TotalMinReservedTcResource = 0;
	UINT_32 u4TotalTcResource = 0;
	UINT_32 u4TotalGurantedTcResource = 0;
	P_QUE_MGT_T prQM = &prAdapter->rQM;

	/* Initialize TC resource control variables */
	for (u4Idx = 0; u4Idx < TC_NUM; u4Idx++)
		prQM->au4AverageQueLen[u4Idx] = 0;

	ASSERT(prQM->u4TimeToAdjustTcResource && prQM->u4TimeToUpdateQueLen);

	for (u4Idx = 0; u4Idx < TC_NUM; u4Idx++) {
		prQM->au4CurrentTcResource[u4Idx] = prAdapter->rTxCtrl.rTc.au4MaxNumOfBuffer[u4Idx];

		if (u4Idx != TC4_INDEX) {
			u4TotalTcResource += prQM->au4CurrentTcResource[u4Idx];
			u4TotalGurantedTcResource += prQM->au4GuaranteedTcResource[u4Idx];
			u4TotalMinReservedTcResource += prQM->au4MinReservedTcResource[u4Idx];
		}
	}

	/* Sanity Check */
	if (u4TotalMinReservedTcResource > u4TotalTcResource)
		kalMemZero(prQM->au4MinReservedTcResource, sizeof(prQM->au4MinReservedTcResource));

	if (u4TotalGurantedTcResource > u4TotalTcResource)
		kalMemZero(prQM->au4GuaranteedTcResource, sizeof(prQM->au4GuaranteedTcResource));

	u4TotalGurantedTcResource = 0;

	/* Initialize Residual TC resource */
	for (u4Idx = 0; u4Idx < TC_NUM; u4Idx++) {
		if (prQM->au4GuaranteedTcResource[u4Idx] < prQM->au4MinReservedTcResource[u4Idx])
			prQM->au4GuaranteedTcResource[u4Idx] = prQM->au4MinReservedTcResource[u4Idx];

		if (u4Idx != TC4_INDEX)
			u4TotalGurantedTcResource += prQM->au4GuaranteedTcResource[u4Idx];
	}

	prQM->u4ResidualTcResource = u4TotalTcResource - u4TotalGurantedTcResource;

}
#endif


#if CFG_SUPPORT_REPLAY_DETECTION
/* To change PN number to UINT64 */
#define CCMPTSCPNNUM	6
BOOLEAN qmRxPNtoU64(PUINT_8 pucPN, UINT_8 uPNNum, PUINT_64 pu8Rets)
{
	UINT_8 ucCount = 0;
	UINT_64 u8Data = 0;
	UINT_64 ucTmp = 0;

	if (!pu8Rets) {
		DBGLOG(QM, ERROR, "Please input valid pu8Rets\n");
		return FALSE;
	}

	if (uPNNum > CCMPTSCPNNUM) {
		DBGLOG(QM, ERROR, "Please input valid uPNNum:%d\n", uPNNum);
		return FALSE;
	}

	*pu8Rets = 0;
	for (; ucCount < uPNNum; ucCount++) {
		ucTmp = pucPN[ucCount];
		u8Data = ucTmp << 8*ucCount;
		*pu8Rets +=  u8Data;
	}
	return TRUE;
}

/* To check PN/TSC between RxStatus and local record. return TRUE if PNS is not bigger than PNT */
BOOLEAN qmRxDetectReplay(PUINT_8 pucPNS, PUINT_8 pucPNT)
{
	UINT_64 u8RxNum = 0;
	UINT_64 u8LocalRec = 0;

	if (!pucPNS || !pucPNT) {
		DBGLOG(QM, ERROR, "Please input valid PNS:%p and PNT:%p\n", pucPNS, pucPNT);
		return TRUE;
	}

	if (!qmRxPNtoU64(pucPNS, CCMPTSCPNNUM, &u8RxNum)
		|| !qmRxPNtoU64(pucPNT, CCMPTSCPNNUM, &u8LocalRec)) {
		DBGLOG(QM, ERROR, "PN2U64 failed\n");
		return TRUE;
	}
	/* PN overflow ? */

	return !(u8RxNum > u8LocalRec);
}

/* TO filter broadcast and multicast data packet replay issue. */
BOOLEAN qmHandleRxReplay(P_ADAPTER_T prAdapter, P_SW_RFB_T prSwRfb)
{
	PUINT_8 pucPN = NULL;
	UINT_8 ucKeyID = 0;				/* 0~4 */
	UINT_8 ucSecMode = CIPHER_SUITE_NONE;		/* CIPHER_SUITE_NONE~CIPHER_SUITE_GCMP */
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_GL_WPA_INFO_T prWpaInfo = NULL;
	struct SEC_DETECT_REPLAY_INFO *prDetRplyInfo = NULL;
	P_HW_MAC_RX_DESC_T prRxStatus = NULL;
	UINT_8 ucBssIndex = 0;
	P_BSS_INFO_T prBssInfo = NULL;
	UINT_8 ucCheckZeroPN;
	UINT_8 i;

	if (!prAdapter)
		return TRUE;
	if (prSwRfb->u2PacketLen <= ETHER_HEADER_LEN)
		return TRUE;

	ucBssIndex = prSwRfb->prStaRec->ucBssIndex;
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

	ASSERT(prBssInfo);

	prGlueInfo = prAdapter->prGlueInfo;
	prWpaInfo = &prGlueInfo->rWpaInfo;

	if (!(prSwRfb->ucGroupVLD & BIT(RX_GROUP_VLD_1)))
		return FALSE;

	/* BMC only need check CCMP and TKIP Cipher suite */
	prRxStatus = prSwRfb->prRxStatus;
	ucSecMode = HAL_RX_STATUS_GET_SEC_MODE(prRxStatus);
	if (ucSecMode != CIPHER_SUITE_CCMP
		&& ucSecMode != CIPHER_SUITE_TKIP) {
		DBGLOG(QM, TRACE, "SecMode: %d and CipherGroup: %d, no need check replay\n",
				ucSecMode, prWpaInfo->u4CipherGroup);
#if 0
		if (!(prSwRfb->ucGroupVLD & BIT(RX_GROUP_VLD_1))) {
			DBGLOG(QM, ERROR, "Group 1 invalid\n");
			return TRUE;
		}
#endif
		return FALSE;
	}

	ucKeyID = HAL_RX_STATUS_GET_KEY_ID(prRxStatus);
	if (ucKeyID >= MAX_KEY_NUM) {
		DBGLOG(QM, ERROR, "KeyID: %d error\n", ucKeyID);
		return TRUE;
	}

	prDetRplyInfo = &prBssInfo->rDetRplyInfo;

#if 0
	if (prDetRplyInfo->arReplayPNInfo[ucKeyID].fgFirstPkt) {
		prDetRplyInfo->arReplayPNInfo[ucKeyID].fgFirstPkt = FALSE;
		HAL_RX_STATUS_GET_PN(prSwRfb->prRxStatusGroup1, prDetRplyInfo->arReplayPNInfo[ucKeyID].auPN);
		DBGLOG(QM, INFO,
			"First check packet. Key ID:0x%x\n", ucKeyID);
		return FALSE;
	}
#endif

	pucPN = prSwRfb->prRxStatusGroup1->aucPN;
	DBGLOG(QM, TRACE,
			"BC packet 0x%x:0x%x:0x%x:0x%x:0x%x:0x%x--0x%x:0x%x:0x%x:0x%x:0x%x:0x%x\n",
			pucPN[0], pucPN[1], pucPN[2], pucPN[3], pucPN[4], pucPN[5],
			prDetRplyInfo->arReplayPNInfo[ucKeyID].auPN[0],
			prDetRplyInfo->arReplayPNInfo[ucKeyID].auPN[1],
			prDetRplyInfo->arReplayPNInfo[ucKeyID].auPN[2],
			prDetRplyInfo->arReplayPNInfo[ucKeyID].auPN[3],
			prDetRplyInfo->arReplayPNInfo[ucKeyID].auPN[4],
			prDetRplyInfo->arReplayPNInfo[ucKeyID].auPN[5]);

	if (prDetRplyInfo->fgKeyRscFresh == TRUE) {

		/* PN non-fresh setting */
		prDetRplyInfo->fgKeyRscFresh = FALSE;
		ucCheckZeroPN = 0;

		for (i = 0; i < 8; i++) {
			if (prSwRfb->prRxStatusGroup1->aucPN[i] == 0x0)
				ucCheckZeroPN++;
		}

		/* for AP start PN from 0, bypass PN check and update */
		if (ucCheckZeroPN == 8) {
			DBGLOG(QM, WARN, "Fresh BC_PN with AP PN=0\n");
			return FALSE;
		}
	}

	if (qmRxDetectReplay(pucPN, prDetRplyInfo->arReplayPNInfo[ucKeyID].auPN)) {
		DBGLOG(QM, WARN, "Drop BC replay packet!\n");
		return TRUE;
	}

	HAL_RX_STATUS_GET_PN(prSwRfb->prRxStatusGroup1, prDetRplyInfo->arReplayPNInfo[ucKeyID].auPN);

	return FALSE;
}

#endif
