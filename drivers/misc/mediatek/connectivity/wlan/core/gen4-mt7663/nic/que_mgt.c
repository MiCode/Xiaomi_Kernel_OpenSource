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
/*! \file   "que_mgt.c"
 *    \brief  TX/RX queues management
 *
 *    The main tasks of queue management include TC-based HIF TX flow control,
 *    adaptive TC quota adjustment, HIF TX grant scheduling, Power-Save
 *    forwarding control, RX packet reordering, and RX BA agreement management.
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
#include "queue.h"

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
OS_SYSTIME g_arMissTimeout[CFG_STA_REC_NUM][CFG_RX_MAX_BA_TID_NUM];

const uint8_t aucTid2ACI[TX_DESC_TID_NUM] = {
	WMM_AC_BE_INDEX,	/* TID0 */
	WMM_AC_BK_INDEX,	/* TID1 */
	WMM_AC_BK_INDEX,	/* TID2 */
	WMM_AC_BE_INDEX,	/* TID3 */
	WMM_AC_VI_INDEX,	/* TID4 */
	WMM_AC_VI_INDEX,	/* TID5 */
	WMM_AC_VO_INDEX,	/* TID6 */
	WMM_AC_VO_INDEX		/* TID7 */
};

const uint8_t aucACI2TxQIdx[WMM_AC_INDEX_NUM] = {
	TX_QUEUE_INDEX_AC1,	/* WMM_AC_BE_INDEX */
	TX_QUEUE_INDEX_AC0,	/* WMM_AC_BK_INDEX */
	TX_QUEUE_INDEX_AC2,	/* WMM_AC_VI_INDEX */
	TX_QUEUE_INDEX_AC3	/* WMM_AC_VO_INDEX */
};

const uint8_t *apucACI2Str[WMM_AC_INDEX_NUM] = {
	"BE", "BK", "VI", "VO"
};

const uint8_t arNetwork2TcResource[MAX_BSSID_NUM + 1][NET_TC_NUM] = {
	/* HW Queue Set 1 */
	/* AC_BE, AC_BK, AC_VI, AC_VO, MGMT, BMC */
	/* AIS */
	{TC1_INDEX, TC0_INDEX, TC2_INDEX, TC3_INDEX, TC4_INDEX, BMC_TC_INDEX},
	/* P2P/BoW */
	{TC1_INDEX, TC0_INDEX, TC2_INDEX, TC3_INDEX, TC4_INDEX, BMC_TC_INDEX},
	/* P2P/BoW */
	{TC1_INDEX, TC0_INDEX, TC2_INDEX, TC3_INDEX, TC4_INDEX, BMC_TC_INDEX},
	/* P2P/BoW */
	{TC1_INDEX, TC0_INDEX, TC2_INDEX, TC3_INDEX, TC4_INDEX, BMC_TC_INDEX},
	/* P2P_DEV */
	{TC1_INDEX, TC0_INDEX, TC2_INDEX, TC3_INDEX, TC4_INDEX, BMC_TC_INDEX},
};

const uint8_t aucWmmAC2TcResourceSet1[WMM_AC_INDEX_NUM] = {
	TC1_INDEX,
	TC0_INDEX,
	TC2_INDEX,
	TC3_INDEX
};

#if NIC_TX_ENABLE_SECOND_HW_QUEUE
const uint8_t aucWmmAC2TcResourceSet2[WMM_AC_INDEX_NUM] = {
	TC7_INDEX,
	TC6_INDEX,
	TC8_INDEX,
	TC9_INDEX
};
#endif
/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */
#if ARP_MONITER_ENABLE
static uint16_t arpMoniter;
static uint8_t apIp[4];
static uint8_t gatewayIp[4];
#endif
/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
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
		prCurrSwRfb = nicRxDefragMPDU(prAdapter, \
			prCurrSwRfb, prReturnedQue); \
		if (prCurrSwRfb) { \
			prRxStatus = prCurrSwRfb->prRxStatus; \
			DBGLOG(QM, TRACE, \
				"defragmentation RxStatus=%p\n", prRxStatus); \
		} \
	} \
	if (prCurrSwRfb) { \
		fgMicErr = FALSE; \
		if (HAL_RX_STATUS_GET_SEC_MODE(prRxStatus) == \
			CIPHER_SUITE_TKIP_WO_MIC) { \
			if (prCurrSwRfb->prStaRec) { \
				uint8_t ucBssIndex; \
				struct BSS_INFO *prBssInfo = NULL; \
				uint8_t *pucMicKey = NULL; \
				ucBssIndex = \
					prCurrSwRfb->prStaRec->ucBssIndex; \
				ASSERT(ucBssIndex < prAdapter->ucHwBssIdNum); \
				prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, \
					ucBssIndex); \
				ASSERT(prBssInfo); \
				if (prBssInfo->eCurrentOPMode == \
					OP_MODE_INFRASTRUCTURE) \
					pucMicKey = \
					&(prAdapter->rWifiVar.\
					rAisSpecificBssInfo.aucRxMicKey[0]); \
				else { \
					ASSERT(FALSE); \
				} \
				/* SW TKIP MIC verify */ \
				if (pucMicKey == NULL) { \
					DBGLOG(RX, ERROR, \
						"No TKIP Mic Key\n"); \
					fgMicErr = TRUE; \
				} \
				else if (tkipMicDecapsulateInRxHdrTransMode( \
					prCurrSwRfb, pucMicKey) == FALSE) { \
					fgMicErr = TRUE; \
				} \
			} \
			if (fgMicErr) { \
				/* bypass tkip frag */ \
				if (!prCurrSwRfb->fgFragFrame) { \
					log_dbg(RX, ERROR, \
					"Mark NULL for TKIP Mic Error\n"); \
					RX_INC_CNT(&prAdapter->rRxCtrl, \
					RX_MIC_ERROR_DROP_COUNT); \
					prCurrSwRfb->eDst = \
						RX_PKT_DESTINATION_NULL; \
				} \
			} \
		} \
		QUEUE_INSERT_TAIL(prReturnedQue, \
			(struct QUE_ENTRY *)prCurrSwRfb); \
	} \
} while (0)
#endif

#define RX_DIRECT_REORDER_LOCK(pad, dbg) \
do { \
	struct GLUE_INFO *_glue = pad->prGlueInfo; \
	if (!HAL_IS_RX_DIRECT(pad) || !_glue) \
		break; \
	if (dbg) \
		DBGLOG(QM, EVENT, "RX_DIRECT_REORDER_LOCK %d\n", __LINE__); \
	if (irqs_disabled()) \
		spin_lock(&_glue->rSpinLock[SPIN_LOCK_RX_DIRECT_REORDER]); \
	else \
		spin_lock_bh(&_glue->rSpinLock[SPIN_LOCK_RX_DIRECT_REORDER]); \
} while (0)

#define RX_DIRECT_REORDER_UNLOCK(pad, dbg) \
do { \
	struct GLUE_INFO *_glue = pad->prGlueInfo; \
	if (!HAL_IS_RX_DIRECT(pad) || !_glue) \
		break; \
	if (dbg) \
		DBGLOG(QM, EVENT, "RX_DIRECT_REORDER_UNLOCK %u\n", __LINE__); \
	if (irqs_disabled()) \
		spin_unlock(&_glue->rSpinLock[SPIN_LOCK_RX_DIRECT_REORDER]); \
	else \
		spin_unlock_bh(&_glue->rSpinLock[SPIN_LOCK_RX_DIRECT_REORDER]);\
} while (0)

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
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
void qmInit(IN struct ADAPTER *prAdapter,
	IN u_int8_t isTxResrouceControlEn)
{
	uint32_t u4Idx;
#if QM_ADAPTIVE_TC_RESOURCE_CTRL
	uint32_t u4TotalMinReservedTcResource = 0;
	uint32_t u4TotalTcResource = 0;
	uint32_t u4TotalGurantedTcResource = 0;
#endif

	struct QUE_MGT *prQM = &prAdapter->rQM;

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
		prQM->arRxBaTable[u4Idx].u8LastAmsduSubIdx =
			RX_PAYLOAD_FORMAT_MSDU;
		prQM->arRxBaTable[u4Idx].fgAmsduNeedLastFrame = FALSE;
		prQM->arRxBaTable[u4Idx].fgIsAmsduDuplicated = FALSE;
#endif
		prQM->arRxBaTable[u4Idx].fgFirstSnToWinStart = FALSE;
		cnmTimerInitTimer(prAdapter,
			&(prQM->arRxBaTable[u4Idx].rReorderBubbleTimer),
			(PFN_MGMT_TIMEOUT_FUNC) qmHandleReorderBubbleTimeout,
			(unsigned long) (&prQM->arRxBaTable[u4Idx]));

	}
	prQM->ucRxBaCount = 0;

	kalMemSet(&g_arMissTimeout, 0, sizeof(g_arMissTimeout));

	prQM->fgIsTxResrouceControlEn = isTxResrouceControlEn;

#if QM_ADAPTIVE_TC_RESOURCE_CTRL
	/* 4 <4> Initialize TC resource control variables */
	for (u4Idx = 0; u4Idx < TC_NUM; u4Idx++)
		prQM->au4AverageQueLen[u4Idx] = 0;

	ASSERT(prQM->u4TimeToAdjustTcResource
	       && prQM->u4TimeToUpdateQueLen);

	for (u4Idx = 0; u4Idx < TC_NUM; u4Idx++) {
		prQM->au4CurrentTcResource[u4Idx] =
			prAdapter->rTxCtrl.rTc.au4MaxNumOfBuffer[u4Idx];

		if (u4Idx != TC4_INDEX) {
			u4TotalTcResource += prQM->au4CurrentTcResource[u4Idx];
			u4TotalGurantedTcResource +=
				prQM->au4GuaranteedTcResource[u4Idx];
			u4TotalMinReservedTcResource +=
				prQM->au4MinReservedTcResource[u4Idx];
		}
	}

	/* Sanity Check */
	if (u4TotalMinReservedTcResource > u4TotalTcResource)
		kalMemZero(prQM->au4MinReservedTcResource,
			   sizeof(prQM->au4MinReservedTcResource));

	if (u4TotalGurantedTcResource > u4TotalTcResource)
		kalMemZero(prQM->au4GuaranteedTcResource,
			   sizeof(prQM->au4GuaranteedTcResource));

	u4TotalGurantedTcResource = 0;

	/* Initialize Residual TC resource */
	for (u4Idx = 0; u4Idx < TC_NUM; u4Idx++) {
		if (prQM->au4GuaranteedTcResource[u4Idx] <
		    prQM->au4MinReservedTcResource[u4Idx])
			prQM->au4GuaranteedTcResource[u4Idx] =
				prQM->au4MinReservedTcResource[u4Idx];

		if (u4Idx != TC4_INDEX)
			u4TotalGurantedTcResource +=
				prQM->au4GuaranteedTcResource[u4Idx];
	}

	prQM->u4ResidualTcResource = u4TotalTcResource -
				     u4TotalGurantedTcResource;

	prQM->fgTcResourcePostAnnealing = FALSE;
	prQM->fgForceReassign = FALSE;
#if QM_FAST_TC_RESOURCE_CTRL
	prQM->fgTcResourceFastReaction = FALSE;
#endif
#endif

#if QM_TEST_MODE
	prQM->u4PktCount = 0;

#if QM_TEST_FAIR_FORWARDING

	prQM->u4CurrentStaRecIndexToEnqueue = 0;
	{
		uint8_t aucMacAddr[MAC_ADDR_LEN];
		struct STA_RECORD *prStaRec;

		/* Irrelevant in case this STA is an AIS AP
		 * (see qmDetermineStaRecIndex())
		 */
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
void qmTestCases(IN struct ADAPTER *prAdapter)
{
	struct QUE_MGT *prQM = &prAdapter->rQM;

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
		/* Note that QM_STA_REC_HARD_CODING
		 * shall be set to 1 for this test
		 */

		if (prAdapter->arStaRec[0].fgIsValid) {

			DbgPrint("QM: (Test) Deactivate STA_REC[0]\n");
			qmDeactivateStaRec(prAdapter, &prAdapter->arStaRec[0]);
		} else {

			uint8_t aucMacAddr[MAC_ADDR_LEN];

			/* Irrelevant in case this STA is an AIS AP
			 * (see qmDetermineStaRecIndex())
			 */
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
				NETWORK_TYPE_AIS_INDEX, /* Network type */
				TRUE,	/* fgIsAp */
				aucMacAddr	/* MAC address */
			);
		}
	}

	if (QM_TEST_FAIR_FORWARDING) {
		if (prAdapter->arStaRec[1].fgIsValid) {
			prQM->u4CurrentStaRecIndexToEnqueue++;
			prQM->u4CurrentStaRecIndexToEnqueue %= 2;
			DbgPrint("QM: (Test) Switch to STA_REC[%ld]\n",
				 prQM->u4CurrentStaRecIndexToEnqueue);
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
void qmUpdateStaRec(IN struct ADAPTER *prAdapter,
	IN struct STA_RECORD *prStaRec)
{
	struct BSS_INFO *prBssInfo;
	u_int8_t fgIsTxAllowed = FALSE;

	if (!prStaRec)
		return;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);

	ASSERT(prBssInfo);

	/* 4 <1> Ensure STA is valid */
	if (prStaRec->fgIsValid) {
		/* 4 <2.1> STA/BSS is protected */
		if (secIsProtectedBss(prAdapter, prBssInfo)) {
			if (prStaRec->fgIsTxKeyReady ||
				secIsWepBss(prAdapter, prBssInfo))
				fgIsTxAllowed = TRUE;
			else
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
		fgIsTxAllowed && (prStaRec->ucStaState == STA_STATE_3)) {
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
void qmActivateStaRec(IN struct ADAPTER *prAdapter,
		      IN struct STA_RECORD *prStaRec)
{
	/* 4 <1> Deactivate first */
	if (!prStaRec)
		return;

	if (prStaRec->fgIsValid) {	/* The STA_REC has been activated */
		DBGLOG(QM, WARN,
			"QM: (WARNING) Activating a STA_REC which has been activated\n");
		DBGLOG(QM, WARN,
			"QM: (WARNING) Deactivating a STA_REC before re-activating\n");
		/* To flush TX/RX queues and del RX BA agreements */
		qmDeactivateStaRec(prAdapter, prStaRec);
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
	prStaRec->fgTxAmpduEn = IS_FEATURE_ENABLED(
		prAdapter->rWifiVar.ucAmpduTx);
	prStaRec->fgRxAmpduEn = IS_FEATURE_ENABLED(
		prAdapter->rWifiVar.ucAmpduRx);

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
void qmDeactivateStaRec(IN struct ADAPTER *prAdapter,
	IN struct STA_RECORD *prStaRec)
{
	uint32_t i;

	if (!prStaRec)
		return;
	/* 4 <1> Flush TX queues */
	if (HAL_IS_TX_DIRECT(prAdapter)) {
		nicTxDirectClearStaPsQ(prAdapter, prStaRec->ucIndex);
	} else {
		struct MSDU_INFO *prFlushedTxPacketList = NULL;

		prFlushedTxPacketList = qmFlushStaTxQueues(prAdapter,
			prStaRec->ucIndex);

		if (prFlushedTxPacketList)
			wlanProcessQueuedMsduInfo(prAdapter,
				prFlushedTxPacketList);
	}

	/* 4 <2> Flush RX queues and delete RX BA agreements */
	for (i = 0; i < CFG_RX_MAX_BA_TID_NUM; i++) {
		/* Delete the RX BA entry with TID = i */
		qmDelRxBaEntry(prAdapter, prStaRec->ucIndex, (uint8_t) i,
			FALSE);
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
void qmFreeAllByBssIdx(IN struct ADAPTER *prAdapter, IN uint8_t ucBssIndex)
{

	struct QUE_MGT *prQM;
	struct QUE *prQue;
	struct QUE rNeedToFreeQue;
	struct QUE rTempQue;
	struct QUE *prNeedToFreeQue;
	struct QUE *prTempQue;
	struct MSDU_INFO *prMsduInfo;

	prQM = &prAdapter->rQM;
	prQue = &prQM->arTxQueue[TX_QUEUE_INDEX_BMCAST];

	QUEUE_INITIALIZE(&rNeedToFreeQue);
	QUEUE_INITIALIZE(&rTempQue);

	prNeedToFreeQue = &rNeedToFreeQue;
	prTempQue = &rTempQue;

	QUEUE_MOVE_ALL(prTempQue, prQue);

	QUEUE_REMOVE_HEAD(prTempQue, prMsduInfo,
		struct MSDU_INFO *);
	while (prMsduInfo) {

		if (prMsduInfo->ucBssIndex == ucBssIndex) {
			/* QUEUE_INSERT_TAIL */
			QUEUE_INSERT_TAIL(prNeedToFreeQue,
				(struct QUE_ENTRY *) prMsduInfo);
		} else {
			/* QUEUE_INSERT_TAIL */
			QUEUE_INSERT_TAIL(prQue,
				(struct QUE_ENTRY *) prMsduInfo);
		}

		QUEUE_REMOVE_HEAD(prTempQue, prMsduInfo,
			struct MSDU_INFO *);
	}
	if (QUEUE_IS_NOT_EMPTY(prNeedToFreeQue))
		wlanProcessQueuedMsduInfo(prAdapter,
			(struct MSDU_INFO *)
			QUEUE_GET_HEAD(prNeedToFreeQue));

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
struct MSDU_INFO *qmFlushTxQueues(IN struct ADAPTER *prAdapter)
{
	uint8_t ucStaArrayIdx;
	uint8_t ucQueArrayIdx;

	struct QUE_MGT *prQM = &prAdapter->rQM;

	struct QUE rTempQue;
	struct QUE *prTempQue = &rTempQue;
	struct QUE *prQue;

	DBGLOG(QM, TRACE, "QM: Enter qmFlushTxQueues()\n");

	QUEUE_INITIALIZE(prTempQue);

	/* Concatenate all MSDU_INFOs in per-STA queues */
	for (ucStaArrayIdx = 0; ucStaArrayIdx < CFG_STA_REC_NUM;
		ucStaArrayIdx++) {
		for (ucQueArrayIdx = 0;
			ucQueArrayIdx < NUM_OF_PER_STA_TX_QUEUES;
			ucQueArrayIdx++) {
			prQue = &(prAdapter->arStaRec[ucStaArrayIdx].
			arPendingTxQueue[ucQueArrayIdx]);
			QUEUE_CONCATENATE_QUEUES(prTempQue, prQue);
			prQue = &(prAdapter->arStaRec[ucStaArrayIdx].
			arTxQueue[ucQueArrayIdx]);
			QUEUE_CONCATENATE_QUEUES(prTempQue, prQue);
		}
	}

	/* Flush per-Type queues */
	for (ucQueArrayIdx = 0;
		ucQueArrayIdx < NUM_OF_PER_TYPE_TX_QUEUES;
		ucQueArrayIdx++) {
		prQue = &(prQM->arTxQueue[ucQueArrayIdx]);
		QUEUE_CONCATENATE_QUEUES(prTempQue, prQue);
	}

	return (struct MSDU_INFO *)QUEUE_GET_HEAD(prTempQue);
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
struct MSDU_INFO *qmFlushStaTxQueues(IN struct ADAPTER *prAdapter,
	IN uint32_t u4StaRecIdx)
{
	uint8_t ucQueArrayIdx;
	struct STA_RECORD *prStaRec;
	struct QUE *prQue;
	struct QUE rTempQue;
	struct QUE *prTempQue = &rTempQue;

	DBGLOG(QM, TRACE, "QM: Enter qmFlushStaTxQueues(%u)\n", u4StaRecIdx);

	ASSERT(u4StaRecIdx < CFG_STA_REC_NUM);

	prStaRec = &prAdapter->arStaRec[u4StaRecIdx];
	ASSERT(prStaRec);

	QUEUE_INITIALIZE(prTempQue);

	/* Concatenate all MSDU_INFOs in TX queues of this STA_REC */
	for (ucQueArrayIdx = 0;
		ucQueArrayIdx < NUM_OF_PER_STA_TX_QUEUES; ucQueArrayIdx++) {
		prQue = &(prStaRec->arPendingTxQueue[ucQueArrayIdx]);
		QUEUE_CONCATENATE_QUEUES(prTempQue, prQue);
		prQue = &(prStaRec->arTxQueue[ucQueArrayIdx]);
		QUEUE_CONCATENATE_QUEUES(prTempQue, prQue);
	}

	return (struct MSDU_INFO *)QUEUE_GET_HEAD(prTempQue);
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
struct SW_RFB *qmFlushRxQueues(IN struct ADAPTER *prAdapter)
{
	uint32_t i;
	struct SW_RFB *prSwRfbListHead;
	struct SW_RFB *prSwRfbListTail;
	struct QUE_MGT *prQM = &prAdapter->rQM;

	prSwRfbListHead = prSwRfbListTail = NULL;

	DBGLOG(QM, TRACE, "QM: Enter qmFlushRxQueues()\n");

	RX_DIRECT_REORDER_LOCK(prAdapter, 0);
	for (i = 0; i < CFG_NUM_OF_RX_BA_AGREEMENTS; i++) {
		if (QUEUE_IS_NOT_EMPTY(&
			(prQM->arRxBaTable[i].rReOrderQue))) {
			if (!prSwRfbListHead) {

				/* The first MSDU_INFO is found */
				prSwRfbListHead = (struct SW_RFB *)
					QUEUE_GET_HEAD(
						&(prQM->arRxBaTable[i].
						rReOrderQue));
				prSwRfbListTail = (struct SW_RFB *)
					QUEUE_GET_TAIL(
						&(prQM->arRxBaTable[i].
						rReOrderQue));
			} else {
				/* Concatenate the MSDU_INFO list with
				 * the existing list
				 */
				QM_TX_SET_NEXT_MSDU_INFO(prSwRfbListTail,
					QUEUE_GET_HEAD(
						&(prQM->arRxBaTable[i].
						rReOrderQue)));

				prSwRfbListTail = (struct SW_RFB *)
					QUEUE_GET_TAIL(
						&(prQM->arRxBaTable[i].
						rReOrderQue));
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
struct SW_RFB *qmFlushStaRxQueue(IN struct ADAPTER *prAdapter,
	IN uint32_t u4StaRecIdx, IN uint32_t u4Tid)
{
	/* UINT_32 i; */
	struct SW_RFB *prSwRfbListHead;
	struct SW_RFB *prSwRfbListTail;
	struct RX_BA_ENTRY *prReorderQueParm;
	struct STA_RECORD *prStaRec;

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

	/* Note: For each queued packet,
	 * prCurrSwRfb->eDst equals RX_PKT_DESTINATION_HOST
	 */
	if (prReorderQueParm) {
		RX_DIRECT_REORDER_LOCK(prAdapter, 0);
		if (QUEUE_IS_NOT_EMPTY(&(prReorderQueParm->rReOrderQue))) {

			prSwRfbListHead = (struct SW_RFB *)
				QUEUE_GET_HEAD(
					&(prReorderQueParm->rReOrderQue));
			prSwRfbListTail = (struct SW_RFB *)
				QUEUE_GET_TAIL(
					&(prReorderQueParm->rReOrderQue));

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

struct QUE *qmDetermineStaTxQueue(IN struct ADAPTER *prAdapter,
				  IN struct MSDU_INFO *prMsduInfo,
				  IN uint8_t ucActiveTs, OUT uint8_t *pucTC)
{
	struct QUE *prTxQue = NULL;
	struct STA_RECORD *prStaRec;
	enum ENUM_WMM_ACI eAci = WMM_AC_BE_INDEX;
	u_int8_t fgCheckACMAgain;
	uint8_t ucTC, ucQueIdx = WMM_AC_BE_INDEX;
	struct BSS_INFO *prBssInfo;
	/* BEtoBK, na, VItoBE, VOtoVI */
	uint8_t aucNextUP[WMM_AC_INDEX_NUM] = {1, 1, 0, 4};

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
		prMsduInfo->ucBssIndex);
	prStaRec = QM_GET_STA_REC_PTR_FROM_INDEX(prAdapter,
		prMsduInfo->ucStaRecIndex);
	if (prStaRec == NULL)
		return prTxQue;

	if (!prStaRec) {
		DBGLOG(QM, ERROR, "prStaRec is null.\n");
		return NULL;
	}
	if (prMsduInfo->ucUserPriority < 8) {
		QM_DBG_CNT_INC(&prAdapter->rQM,
			prMsduInfo->ucUserPriority + 15);
	}

	eAci = WMM_AC_BE_INDEX;
	do {
		fgCheckACMAgain = FALSE;
		if (prStaRec->fgIsQoS) {
			if (prMsduInfo->ucUserPriority < TX_DESC_TID_NUM) {
				eAci = aucTid2ACI[prMsduInfo->ucUserPriority];
				ucQueIdx = aucACI2TxQIdx[eAci];
				ucTC =
					arNetwork2TcResource[
					prMsduInfo->ucBssIndex][eAci];
			} else {
				ucQueIdx = TX_QUEUE_INDEX_AC1;
				ucTC = TC1_INDEX;
				eAci = WMM_AC_BE_INDEX;
				DBGLOG(QM, WARN,
					"Packet TID is not in [0~7]\n");
				ASSERT(0);
			}
			if ((prBssInfo->arACQueParms[eAci].ucIsACMSet) &&
			    !(ucActiveTs & BIT(eAci)) &&
			    (eAci != WMM_AC_BK_INDEX)) {
				DBGLOG(WMM, TRACE,
					"ucUserPriority: %d, aucNextUP[eAci]: %d",
					prMsduInfo->ucUserPriority,
					aucNextUP[eAci]);
				prMsduInfo->ucUserPriority = aucNextUP[eAci];
				fgCheckACMAgain = TRUE;
			}
		} else {
			ucQueIdx = TX_QUEUE_INDEX_NON_QOS;
			ucTC = arNetwork2TcResource[prMsduInfo->ucBssIndex][
				NET_TC_WMM_AC_BE_INDEX];
		}

		if (prAdapter->rWifiVar.ucTcRestrict < TC_NUM) {
			ucTC = prAdapter->rWifiVar.ucTcRestrict;
			ucQueIdx = ucTC;
		}

	} while (fgCheckACMAgain);

	if (ucQueIdx >= NUM_OF_PER_STA_TX_QUEUES) {
		DBGLOG(QM, ERROR,
			"ucQueIdx = %u, needs 0~3 to avoid out-of-bounds.\n",
			ucQueIdx);
		return NULL;
	}
	if (prStaRec->fgIsTxAllowed) {
		/* non protected BSS or protected BSS with key set */
		prTxQue = prStaRec->aprTargetQueue[ucQueIdx];
	} else if (secIsProtectedBss(prAdapter, prBssInfo) &&
		prMsduInfo->fgIs802_1x &&
		prMsduInfo->fgIs802_1x_NonProtected) {
		/* protected BSS without key set */
		/* Tx pairwise EAPOL 1x packet (non-protected frame) */
		prTxQue = &prStaRec->arTxQueue[ucQueIdx];
	} else {
		/* protected BSS without key set */
		/* Enqueue protected frame into pending queue */
		prTxQue = prStaRec->aprTargetQueue[ucQueIdx];
	}

	*pucTC = ucTC;
	/*
	 * Record how many packages enqueue this STA
	 * to TX during statistic intervals
	 */
	prStaRec->u4EnqueueCounter++;

	return prTxQue;
}

void qmSetTxPacketDescTemplate(IN struct ADAPTER *prAdapter,
	IN struct MSDU_INFO *prMsduInfo)
{
	struct STA_RECORD *prStaRec = QM_GET_STA_REC_PTR_FROM_INDEX(
		prAdapter, prMsduInfo->ucStaRecIndex);

	/* Check the Tx descriptor template is valid */
	if (prStaRec &&
		prStaRec->aprTxDescTemplate[prMsduInfo->ucUserPriority]) {
		prMsduInfo->fgIsTXDTemplateValid = TRUE;
	} else {
		if (prStaRec) {
			DBGLOG(QM, TRACE,
				"Cannot get TXD template for STA[%u] QoS[%u] MSDU UP[%u]\n",
				prStaRec->ucIndex, prStaRec->fgIsQoS,
				prMsduInfo->ucUserPriority);
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
void qmSetStaRecTxAllowed(IN struct ADAPTER *prAdapter,
	IN struct STA_RECORD *prStaRec, IN u_int8_t fgIsTxAllowed)
{
	uint8_t ucIdx;
	struct QUE *prSrcQ, *prDstQ;

	DBGLOG(QM, INFO, "Set Sta[%u] TxAllowed from [%u] to [%u] %s TxQ\n",
		prStaRec->ucIndex,
		prStaRec->fgIsTxAllowed,
		fgIsTxAllowed,
		fgIsTxAllowed ? "normal" : "pending");

	/* Update Tx queue when allowed state change*/
	if (prStaRec->fgIsTxAllowed != fgIsTxAllowed) {
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

		if (fgIsTxAllowed)
			prAdapter->rQM.u4TxAllowedStaCount++;
		else
			prAdapter->rQM.u4TxAllowedStaCount--;

	}
	prStaRec->fgIsTxAllowed = fgIsTxAllowed;
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
struct MSDU_INFO *qmEnqueueTxPackets(IN struct ADAPTER *prAdapter,
	IN struct MSDU_INFO *prMsduInfoListHead)
{
	struct MSDU_INFO *prMsduInfoReleaseList;
	struct MSDU_INFO *prCurrentMsduInfo;
	struct MSDU_INFO *prNextMsduInfo;

	struct QUE *prTxQue;
	struct QUE rNotEnqueuedQue;
	struct STA_RECORD *prStaRec;
	uint8_t ucTC;
	struct TX_CTRL *prTxCtrl = &prAdapter->rTxCtrl;
	struct QUE_MGT *prQM = &prAdapter->rQM;
	struct BSS_INFO *prBssInfo;
	u_int8_t fgDropPacket;
	uint8_t ucActivedTspec = 0;

	DBGLOG(QM, LOUD, "Enter qmEnqueueTxPackets\n");

	ASSERT(prMsduInfoListHead);

	prMsduInfoReleaseList = NULL;
	prCurrentMsduInfo = NULL;
	QUEUE_INITIALIZE(&rNotEnqueuedQue);
	prNextMsduInfo = prMsduInfoListHead;
	ucActivedTspec = wmmHasActiveTspec(&prAdapter->rWifiVar.rWmmInfo);

	do {
		prCurrentMsduInfo = prNextMsduInfo;
		prNextMsduInfo = QM_TX_GET_NEXT_MSDU_INFO(
			prCurrentMsduInfo);
		ucTC = TC1_INDEX;

		/* 4 <0> Sanity check of BSS_INFO */
		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
			prCurrentMsduInfo->ucBssIndex);

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

			/*get per-AC Tx packets */
			wlanUpdateTxStatistics(prAdapter, prCurrentMsduInfo,
				FALSE);

			DBGLOG(QM, LOUD, "Enqueue MSDU by StaRec[%u]!\n",
			       prCurrentMsduInfo->ucStaRecIndex);

			switch (prCurrentMsduInfo->ucStaRecIndex) {
			case STA_REC_INDEX_BMCAST:
				prTxQue =
					&prQM->arTxQueue[TX_QUEUE_INDEX_BMCAST];
				ucTC =
					arNetwork2TcResource[prCurrentMsduInfo->
					ucBssIndex][NET_TC_BMC_INDEX];

				/* Always set BMC packet retry limit
				 * to unlimited
				 */
				if (!(prCurrentMsduInfo->u4Option &
				      MSDU_OPT_MANUAL_RETRY_LIMIT))
					nicTxSetPktRetryLimit(prCurrentMsduInfo,
						TX_DESC_TX_COUNT_NO_LIMIT);

				QM_DBG_CNT_INC(prQM, QM_DBG_CNT_23);
				break;

			case STA_REC_INDEX_NOT_FOUND:
				/* Drop packet if no STA_REC is found */
				DBGLOG(QM, TRACE,
					"Drop the Packet for no STA_REC\n");

				prTxQue = &rNotEnqueuedQue;

				TX_INC_CNT(&prAdapter->rTxCtrl,
					TX_INACTIVE_STA_DROP);
				QM_DBG_CNT_INC(prQM, QM_DBG_CNT_24);
				break;

			default:
				prTxQue = qmDetermineStaTxQueue(
					prAdapter, prCurrentMsduInfo,
					ucActivedTspec, &ucTC);
				if (!prTxQue) {
					DBGLOG(QM, INFO,
						"Drop the Packet for TxQue is NULL\n");
					prTxQue = &rNotEnqueuedQue;
					TX_INC_CNT(&prAdapter->rTxCtrl,
						TX_INACTIVE_STA_DROP);
					QM_DBG_CNT_INC(prQM, QM_DBG_CNT_24);
				}
#if ARP_MONITER_ENABLE
				prStaRec =
					QM_GET_STA_REC_PTR_FROM_INDEX(prAdapter,
						prCurrentMsduInfo->
						ucStaRecIndex);
				if (prStaRec && IS_STA_IN_AIS(prStaRec) &&
					prCurrentMsduInfo->eSrc == TX_PACKET_OS)
					qmDetectArpNoResponse(prAdapter,
						prCurrentMsduInfo);
#endif
				break;	/*default */
			}	/* switch (prCurrentMsduInfo->ucStaRecIndex) */

			if (prCurrentMsduInfo->eSrc == TX_PACKET_FORWARDING) {
				DBGLOG(QM, TRACE,
					"Forward Pkt to STA[%u] BSS[%u]\n",
					prCurrentMsduInfo->ucStaRecIndex,
					prCurrentMsduInfo->ucBssIndex);

				if (prTxQue->u4NumElem >=
					prQM->u4MaxForwardBufferCount) {
					DBGLOG(QM, INFO,
					       "Drop the Packet for full Tx queue (forwarding) Bss %u\n",
					       prCurrentMsduInfo->ucBssIndex);
					prTxQue = &rNotEnqueuedQue;
					TX_INC_CNT(&prAdapter->rTxCtrl,
						TX_FORWARD_OVERFLOW_DROP);
				}
			}

		} else {
			DBGLOG(QM, TRACE,
				"Drop the Packet for inactive Bss %u\n",
				prCurrentMsduInfo->ucBssIndex);
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
			nicTxSetPktLowestFixedRate(prAdapter,
				prCurrentMsduInfo);
			break;

		case DATA_RATE_MODE_MANUAL:
			prCurrentMsduInfo->u4FixedRateOption =
				prAdapter->rWifiVar.u4DataTxRateCode;

			prCurrentMsduInfo->ucRateMode =
				MSDU_RATE_MODE_MANUAL_DESC;
			break;

		case DATA_RATE_MODE_AUTO:
		default:
			if (prCurrentMsduInfo->ucRateMode ==
			    MSDU_RATE_MODE_LOWEST_RATE)
				nicTxSetPktLowestFixedRate(prAdapter,
					prCurrentMsduInfo);
			break;
		}

		/* 4 <4> Enqueue the packet */
		QUEUE_INSERT_TAIL(prTxQue,
			(struct QUE_ENTRY *) prCurrentMsduInfo);
		wlanFillTimestamp(prAdapter, prCurrentMsduInfo->prPacket,
				  PHASE_ENQ_QM);
		/*
		 * Record how many packages enqueue
		 * to TX during statistic intervals
		 */
		if (prTxQue != &rNotEnqueuedQue) {
			prQM->u4EnqueueCounter++;
			/* how many page count this frame wanted */
			prQM->au4QmTcWantedPageCounter[ucTC] +=
				prCurrentMsduInfo->u4PageCount;
		}
#if QM_TC_RESOURCE_EMPTY_COUNTER
		if (prCurrentMsduInfo->u4PageCount >
			prTxCtrl->rTc.au4FreePageCount[ucTC])
			prQM->au4QmTcResourceEmptyCounter[
			prCurrentMsduInfo->ucBssIndex][ucTC]++;
#endif

#if QM_FAST_TC_RESOURCE_CTRL && QM_ADAPTIVE_TC_RESOURCE_CTRL
		if (prTxQue != &rNotEnqueuedQue) {
			/* Check and trigger fast TC resource
			 * adjustment for queued packets
			 */
			qmCheckForFastTcResourceCtrl(prAdapter, ucTC);
		}
#endif

#if QM_TEST_MODE
		if (++prQM->u4PktCount == QM_TEST_TRIGGER_TX_COUNT) {
			prQM->u4PktCount = 0;
			qmTestCases(prAdapter);
		}
#endif
	} while (prNextMsduInfo);

	if (QUEUE_IS_NOT_EMPTY(&rNotEnqueuedQue)) {
		QM_TX_SET_NEXT_MSDU_INFO((struct MSDU_INFO *)
			QUEUE_GET_TAIL(&rNotEnqueuedQue), NULL);
		prMsduInfoReleaseList = (struct MSDU_INFO *) QUEUE_GET_HEAD(
			&rNotEnqueuedQue);
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
void qmDetermineStaRecIndex(IN struct ADAPTER *prAdapter,
	IN struct MSDU_INFO *prMsduInfo)
{
	uint32_t i;

	struct STA_RECORD *prTempStaRec;
	struct BSS_INFO *prBssInfo;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prMsduInfo->ucBssIndex);
	prTempStaRec = NULL;

	ASSERT(prMsduInfo);

	DBGLOG(QM, LOUD,
		"Msdu BSS Idx[%u] OpMode[%u] StaRecOfApExist[%u]\n",
		prMsduInfo->ucBssIndex, prBssInfo->eCurrentOPMode,
		prBssInfo->prStaRecOfAP ? TRUE : FALSE);

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
#if CFG_SUPPORT_TDLS
		prTempStaRec =
			cnmGetTdlsPeerByAddress(prAdapter,
				prBssInfo->ucBssIndex,
				prMsduInfo->aucEthDestAddr);
		if (IS_DLS_STA(prTempStaRec)
		    && prTempStaRec->ucStaState == STA_STATE_3) {
			if (g_arTdlsLink[prTempStaRec->ucTdlsIndex]) {
				prMsduInfo->ucStaRecIndex =
					prTempStaRec->ucIndex;
				return;
			}
		}
#endif
		/* 4 <2> Check if an AP STA is present */
		prTempStaRec = prBssInfo->prStaRecOfAP;
		if (prTempStaRec) {
			DBGLOG(QM, LOUD,
				"StaOfAp Idx[%u] WIDX[%u] Valid[%u] TxAllowed[%u] InUse[%u] Type[%u]\n",
				prTempStaRec->ucIndex,
				prTempStaRec->ucWlanIndex,
				prTempStaRec->fgIsValid,
				prTempStaRec->fgIsTxAllowed,
				prTempStaRec->fgIsInUse,
				prTempStaRec->eStaType);

			if (prTempStaRec->fgIsInUse) {
				prMsduInfo->ucStaRecIndex =
					prTempStaRec->ucIndex;
				DBGLOG(QM, LOUD, "TX with AP_STA[%u]\n",
					prTempStaRec->ucIndex);
				return;
			}
		}
		break;

	case OP_MODE_P2P_DEVICE:
		break;

	default:
		break;
	}

	/* 4 <3> Not BMCAST, No AP --> Compare DA
	 * (i.e., to see whether this is a unicast frame to a client)
	 */
	for (i = 0; i < CFG_STA_REC_NUM; i++) {
		prTempStaRec = &(prAdapter->arStaRec[i]);
		if (prTempStaRec->fgIsInUse) {
			if (EQUAL_MAC_ADDR(prTempStaRec->aucMacAddr,
				prMsduInfo->aucEthDestAddr)) {
				prMsduInfo->ucStaRecIndex =
					prTempStaRec->ucIndex;
				DBGLOG(QM, LOUD, "TX with STA[%u]\n",
					prTempStaRec->ucIndex);
				return;
			}
		}
	}

	/* 4 <4> No STA found, Not BMCAST --> Indicate NOT_FOUND to FW */
	prMsduInfo->ucStaRecIndex = STA_REC_INDEX_NOT_FOUND;
	DBGLOG(QM, LOUD, "QM: TX with STA_REC_INDEX_NOT_FOUND\n");

#if (QM_TEST_MODE && QM_TEST_FAIR_FORWARDING)
	prMsduInfo->ucStaRecIndex =
		(uint8_t)	prQM->u4CurrentStaRecIndexToEnqueue;
#endif
}

struct STA_RECORD *qmDetermineStaToBeDequeued(
	IN struct ADAPTER *prAdapter,
	IN uint32_t u4StartStaRecIndex)
{

	return NULL;
}

struct QUE *qmDequeueStaTxPackets(IN struct ADAPTER *prAdapter)
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
uint32_t
qmDequeueTxPacketsFromPerStaQueues(IN struct ADAPTER *prAdapter,
	OUT struct QUE *prQue, IN uint8_t ucTC,
	IN uint32_t u4CurrentQuota,
	IN uint32_t *prPleCurrentQuota,
	IN uint32_t u4TotalQuota)
{
	uint32_t ucLoop;		/* Loop for */

	uint32_t u4CurStaIndex = 0;
	uint32_t u4CurStaUsedResource = 0;

	/* The current focused STA */
	struct STA_RECORD *prStaRec;
	/* The Bss for current focused STA */
	struct BSS_INFO	*prBssInfo;
	/* The current TX queue to dequeue */
	struct QUE *prCurrQueue;
	/* The dequeued packet */
	struct MSDU_INFO *prDequeuedPkt;

	/* To remember the total forwarded packets for a STA */
	uint32_t u4CurStaForwardFrameCount;
	/* The maximum number of packets a STA can forward */
	uint32_t u4MaxForwardFrameCountLimit;
	uint32_t u4AvaliableResource;	/* The TX resource amount */
	uint32_t u4MaxResourceLimit;

	u_int8_t fgEndThisRound;
	struct QUE_MGT *prQM = &prAdapter->rQM;

	uint8_t *pucPsStaFreeQuota;
#if CFG_SUPPORT_SOFT_ACM
	uint8_t ucAc;
	u_int8_t fgAcmFlowCtrl = FALSE;
	static const uint8_t aucTc2Ac[] = {ACI_BK, ACI_BE, ACI_VI, ACI_VO};
#endif

	/* 4 <1> Assign init value */
	/* Post resource handling, give infinity resource*/
	if (prQM->fgTcResourcePostHandle) {
		u4AvaliableResource = QM_STA_FORWARD_COUNT_UNLIMITED;
		u4MaxResourceLimit = QM_STA_FORWARD_COUNT_UNLIMITED;
		*prPleCurrentQuota = QM_STA_FORWARD_COUNT_UNLIMITED;
	} else {
		/* Sanity Check */
		if (!u4CurrentQuota) {
			DBGLOG(TX, LOUD,
				"(Fairness) Skip TC = %u u4CurrentQuota = %u\n",
				ucTC, u4CurrentQuota);
			prQM->au4DequeueNoTcResourceCounter[ucTC]++;
			return u4CurrentQuota;
		}
		/* Check PLE resource */
		if (!(*prPleCurrentQuota))
			return u4CurrentQuota;
		u4AvaliableResource = u4CurrentQuota;
		u4MaxResourceLimit = u4TotalQuota;
	}

#if QM_FORWARDING_FAIRNESS
	u4CurStaIndex = prQM->au4HeadStaRecIndex[ucTC];
	u4CurStaUsedResource = prQM->au4ResourceUsedCount[ucTC];
#endif

	fgEndThisRound = FALSE;
	ucLoop = 0;
	u4CurStaForwardFrameCount = 0;

	DBGLOG(QM, TEMP,
		"(Fairness) TC[%u] Init Head STA[%u] Resource[%u]\n",
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
			prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
				prStaRec->ucBssIndex);

			/* prCurrQueue = &prStaRec->aprTxQueue[ucTC]; */
			prDequeuedPkt = NULL;
			pucPsStaFreeQuota = NULL;
			/* Set default forward count limit to unlimited */
			u4MaxForwardFrameCountLimit =
				QM_STA_FORWARD_COUNT_UNLIMITED;

			/* 4 <2.2> Update forward frame/page count
			 * limit for this STA
			 */
			/* AP mode: STA in PS buffer handling */
			if (prStaRec->fgIsInPS) {
				if (prStaRec->fgIsQoS &&
					prStaRec->fgIsUapsdSupported &&
					(prStaRec->ucBmpTriggerAC &
						BIT(ucTC))) {
					u4MaxForwardFrameCountLimit =
						prStaRec->
						ucFreeQuotaForDelivery;
					pucPsStaFreeQuota =
						&prStaRec->
						ucFreeQuotaForDelivery;
				} else {
					u4MaxForwardFrameCountLimit =
						prStaRec->
						ucFreeQuotaForNonDelivery;
					pucPsStaFreeQuota =
						&prStaRec->
						ucFreeQuotaForNonDelivery;
				}
			}

			/* fgIsInPS */
			/* Absent BSS handling */
			if (prBssInfo->fgIsNetAbsent) {
				if (u4MaxForwardFrameCountLimit >
					prBssInfo->ucBssFreeQuota)
					u4MaxForwardFrameCountLimit =
						prBssInfo->ucBssFreeQuota;
			}
#if CFG_SUPPORT_DBDC
			if (prAdapter->rWifiVar.fgDbDcModeEn)
				u4MaxResourceLimit =
					gmGetDequeueQuota(prAdapter,
						prStaRec, prBssInfo,
						u4TotalQuota);
#endif
#if CFG_SUPPORT_SOFT_ACM
			if (ucTC <= TC3_INDEX &&
			    prStaRec->afgAcmRequired[aucTc2Ac[ucTC]]) {
				ucAc = aucTc2Ac[ucTC];
				DBGLOG(QM, TRACE, "AC %d Pending Pkts %u\n",
				       ucAc, prCurrQueue->u4NumElem);
				/* Quick check remain medium time and pending
				** packets
				*/
				if (QUEUE_IS_EMPTY(prCurrQueue) ||
				    !wmmAcmCanDequeue(prAdapter, ucAc, 0))
					goto skip_dequeue;
				fgAcmFlowCtrl = TRUE;
			} else
				fgAcmFlowCtrl = FALSE;
#endif
			/* 4 <2.3> Dequeue packet */
			/* Three cases to break: (1) No resource
			 * (2) No packets (3) Fairness
			 */
			while (!QUEUE_IS_EMPTY(prCurrQueue)) {
				prDequeuedPkt = (struct MSDU_INFO *)
					QUEUE_GET_HEAD(prCurrQueue);

				if ((u4CurStaForwardFrameCount >=
				     u4MaxForwardFrameCountLimit) ||
				    (u4CurStaUsedResource >=
				    u4MaxResourceLimit)) {
					/* Exceeds Limit */
					prQM->
					au4DequeueNoTcResourceCounter[ucTC]++;
					break;
				} else if (prDequeuedPkt->u4PageCount >
					   u4AvaliableResource) {
					/* Available Resource is not enough */
					prQM->
					au4DequeueNoTcResourceCounter[ucTC]++;
					if (!(prAdapter->rWifiVar.
						ucAlwaysResetUsedRes & BIT(0)))
						fgEndThisRound = TRUE;
					break;
				} else if ((*prPleCurrentQuota) <
					   NIX_TX_PLE_PAGE_CNT_PER_FRAME) {
					if (!(prAdapter->rWifiVar.
						ucAlwaysResetUsedRes & BIT(0)))
						fgEndThisRound = TRUE;
					break;
				} else if (!prStaRec->fgIsValid) {
					/* In roaming, if the sta_rec doesn't
					 * active by event 0x0C, it can't
					 * dequeue data.
					 */
					DBGLOG_LIMITED(QM, WARN,
						"sta_rec is not valid\n");
					break;
				}
#if CFG_SUPPORT_SOFT_ACM
				if (fgAcmFlowCtrl) {
					uint32_t u4PktTxTime = 0;

					u4PktTxTime = wmmCalculatePktUsedTime(
						prBssInfo, prStaRec,
						prDequeuedPkt->u2FrameLength -
							ETH_HLEN);
					if (!wmmAcmCanDequeue(prAdapter, ucAc,
							      u4PktTxTime))
						break;
				}
#endif
				/* Available to be Tx */

				QUEUE_REMOVE_HEAD(prCurrQueue, prDequeuedPkt,
						  struct MSDU_INFO *);

				if (!QUEUE_IS_EMPTY(prCurrQueue)) {
					/* XXX: check all queues for STA */
					prDequeuedPkt->ucPsForwardingType =
						PS_FORWARDING_MORE_DATA_ENABLED;
				}
				/* to record WMM Set */
				prDequeuedPkt->ucWmmQueSet =
					prBssInfo->ucWmmQueSet;
				QUEUE_INSERT_TAIL(prQue,
					(struct QUE_ENTRY *)
					prDequeuedPkt);
				prStaRec->u4DeqeueuCounter++;
				prQM->u4DequeueCounter++;

				u4AvaliableResource -=
					prDequeuedPkt->u4PageCount;
				u4CurStaUsedResource +=
					prDequeuedPkt->u4PageCount;
				u4CurStaForwardFrameCount++;
				(*prPleCurrentQuota) -=
					NIX_TX_PLE_PAGE_CNT_PER_FRAME;
			}
#if CFG_SUPPORT_SOFT_ACM
skip_dequeue:
#endif
			/* AP mode: Update STA in PS Free quota */
			if (prStaRec->fgIsInPS && pucPsStaFreeQuota) {
				if ((*pucPsStaFreeQuota) >=
					u4CurStaForwardFrameCount)
					(*pucPsStaFreeQuota) -=
						u4CurStaForwardFrameCount;
				else
					(*pucPsStaFreeQuota) = 0;
			}

			if (prBssInfo->fgIsNetAbsent) {
				if (prBssInfo->ucBssFreeQuota >=
					u4CurStaForwardFrameCount)
					prBssInfo->ucBssFreeQuota -=
						u4CurStaForwardFrameCount;
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
	if ((prQM->u4TxAllowedStaCount == 1) ||
		(prAdapter->rWifiVar.ucAlwaysResetUsedRes & BIT(1)))
		u4CurStaUsedResource = 0;

#if QM_FORWARDING_FAIRNESS
	prQM->au4HeadStaRecIndex[ucTC] = u4CurStaIndex;
	prQM->au4ResourceUsedCount[ucTC] = u4CurStaUsedResource;
#endif

	DBGLOG(QM, TEMP,
		"(Fairness) TC[%u] Scheduled Head STA[%u] Left Resource[%u]\n",
		ucTC, u4CurStaIndex, u4AvaliableResource);

	return u4AvaliableResource;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Dequeue TX packets from a per-Type-based Queue for a particular TC
 *
 * \param[out] prQue The queue to put the dequeued packets
 * \param[in] ucTC The TC index(Shall always be BMC_TC_INDEX)
 * \param[in] ucMaxNum The maximum amount of available resource
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
void
qmDequeueTxPacketsFromPerTypeQueues(IN struct ADAPTER *prAdapter,
	OUT struct QUE *prQue, IN uint8_t ucTC,
	IN uint32_t u4CurrentQuota,
	IN uint32_t *prPleCurrentQuota,
	IN uint32_t u4TotalQuota)
{
	uint32_t u4AvaliableResource, u4LeftResource;
	uint32_t u4MaxResourceLimit;
	uint32_t u4TotalUsedResource = 0;
	struct QUE_MGT *prQM;
	PFN_DEQUEUE_FUNCTION pfnDeQFunc[2];
	u_int8_t fgChangeDeQFunc = TRUE;
	u_int8_t fgGlobalQueFirst = TRUE;

	DBGLOG(QM, TEMP, "Enter %s (TC = %d, quota = %u)\n",
		__func__, ucTC, u4CurrentQuota);

	prQM = &prAdapter->rQM;

	/* Post resource handling, give infinity resource*/
	if (prQM->fgTcResourcePostHandle) {
		u4AvaliableResource = QM_STA_FORWARD_COUNT_UNLIMITED;
		u4MaxResourceLimit = QM_STA_FORWARD_COUNT_UNLIMITED;
		*prPleCurrentQuota = QM_STA_FORWARD_COUNT_UNLIMITED;
	} else {
		/* Broadcast/Multicast data packets */
		if (u4CurrentQuota == 0)
			return;
		/* Check PLE resource */
		if (!(*prPleCurrentQuota))
			return;
		u4AvaliableResource = u4CurrentQuota;
		u4MaxResourceLimit = u4TotalQuota;
	}

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
	u4LeftResource = pfnDeQFunc[0](prAdapter, prQue, ucTC,
		u4AvaliableResource,
		prPleCurrentQuota,
		(u4MaxResourceLimit - u4TotalUsedResource));

	/* dequeue function comsumes no resource, change */
	if ((u4LeftResource >= u4AvaliableResource) &&
		(u4AvaliableResource >=
		prAdapter->rTxCtrl.u4MaxPageCntPerFrame)) {
		fgChangeDeQFunc = TRUE;
	} else {
		u4TotalUsedResource +=
			(u4AvaliableResource - u4LeftResource);
		/* Used resource exceeds limit, change */
		if (u4TotalUsedResource >= u4MaxResourceLimit)
			fgChangeDeQFunc = TRUE;
	}

	if (fgChangeDeQFunc) {
		fgGlobalQueFirst = !fgGlobalQueFirst;
		u4TotalUsedResource = 0;
	}

	/* 2nd dequeue function */
	u4LeftResource = pfnDeQFunc[1](prAdapter, prQue, ucTC,
		u4LeftResource, prPleCurrentQuota, u4MaxResourceLimit);

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
 * \param[in] ucTC The TC index(Shall always be BMC_TC_INDEX)
 * \param[in] ucMaxNum The maximum amount of available resource
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
uint32_t
qmDequeueTxPacketsFromGlobalQueue(IN struct ADAPTER *prAdapter,
	OUT struct QUE *prQue,
	IN uint8_t ucTC, IN uint32_t u4CurrentQuota,
	IN uint32_t *prPleCurrentQuota,
	IN uint32_t u4TotalQuota)
{
	struct BSS_INFO *prBssInfo;
	struct QUE *prCurrQueue;
	uint32_t u4AvaliableResource;
	struct MSDU_INFO *prDequeuedPkt;
	struct MSDU_INFO *prBurstEndPkt;
	struct QUE rMergeQue;
	struct QUE *prMergeQue;
	struct QUE_MGT *prQM;

	DBGLOG(QM, TEMP, "Enter %s (TC = %d, quota = %u)\n",
		__func__, ucTC, u4CurrentQuota);

	/* Broadcast/Multicast data packets */
	if (u4CurrentQuota == 0)
		return u4CurrentQuota;
	/* Check PLE resource */
	if (!(*prPleCurrentQuota))
		return u4CurrentQuota;

	prQM = &prAdapter->rQM;

	/* 4 <1> Determine the queue */
	prCurrQueue = &prQM->arTxQueue[TX_QUEUE_INDEX_BMCAST];
	u4AvaliableResource = u4CurrentQuota;
	prDequeuedPkt = NULL;
	prBurstEndPkt = NULL;

	/* Post resource handling, give infinity resource*/
	if (prQM->fgTcResourcePostHandle) {
		u4AvaliableResource = QM_STA_FORWARD_COUNT_UNLIMITED;
		*prPleCurrentQuota = QM_STA_FORWARD_COUNT_UNLIMITED;
	}

	QUEUE_INITIALIZE(&rMergeQue);
	prMergeQue = &rMergeQue;

	/* 4 <2> Dequeue packets */
	while (!QUEUE_IS_EMPTY(prCurrQueue)) {
		prDequeuedPkt = (struct MSDU_INFO *) QUEUE_GET_HEAD(
					prCurrQueue);
		if (prDequeuedPkt->u4PageCount > u4AvaliableResource)
			break;
		if ((*prPleCurrentQuota) < NIX_TX_PLE_PAGE_CNT_PER_FRAME)
			break;

		QUEUE_REMOVE_HEAD(prCurrQueue, prDequeuedPkt,
			struct MSDU_INFO *);

		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
			prDequeuedPkt->ucBssIndex);

		if (IS_BSS_ACTIVE(prBssInfo)) {
			if (!prBssInfo->fgIsNetAbsent) {
				/* to record WMM Set */
				prDequeuedPkt->ucWmmQueSet =
					prBssInfo->ucWmmQueSet;
				QUEUE_INSERT_TAIL(prQue,
					(struct QUE_ENTRY *)
					prDequeuedPkt);
				prBurstEndPkt = prDequeuedPkt;
				prQM->u4DequeueCounter++;
				u4AvaliableResource -=
					prDequeuedPkt->u4PageCount;
				(*prPleCurrentQuota) -=
					NIX_TX_PLE_PAGE_CNT_PER_FRAME;
				QM_DBG_CNT_INC(prQM, QM_DBG_CNT_26);
			} else {
				QUEUE_INSERT_TAIL(prMergeQue,
					(struct QUE_ENTRY *)
					prDequeuedPkt);
			}
		} else {
			QM_TX_SET_NEXT_MSDU_INFO(prDequeuedPkt, NULL);
			wlanProcessQueuedMsduInfo(prAdapter, prDequeuedPkt);
		}
	}

	if (QUEUE_IS_NOT_EMPTY(prMergeQue)) {
		QUEUE_CONCATENATE_QUEUES(prMergeQue, prCurrQueue);
		QUEUE_MOVE_ALL(prCurrQueue, prMergeQue);
		QM_TX_SET_NEXT_MSDU_INFO((struct MSDU_INFO *)
			QUEUE_GET_TAIL(prCurrQueue), NULL);
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
struct MSDU_INFO *qmDequeueTxPackets(IN struct ADAPTER *prAdapter,
	IN struct TX_TCQ_STATUS *prTcqStatus)
{
	int32_t i;
	struct MSDU_INFO *prReturnedPacketListHead;
	struct QUE rReturnedQue;
	uint32_t u4MaxQuotaLimit;
	uint32_t u4AvailableResourcePLE;

	DBGLOG(QM, TEMP, "Enter qmDequeueTxPackets\n");

	QUEUE_INITIALIZE(&rReturnedQue);

	prReturnedPacketListHead = NULL;

	/* TC0 to TC3: AC0~AC3 (commands packets are not handled by QM) */
	for (i = TC3_INDEX; i >= TC0_INDEX; i--) {
		DBGLOG(QM, TEMP, "Dequeue packets from Per-STA queue[%u]\n", i);

		/* If only one STA is Tx allowed,
		 * no need to restrict Max quota
		 */
		if (prAdapter->rWifiVar.u4MaxTxDeQLimit)
			u4MaxQuotaLimit = prAdapter->rWifiVar.u4MaxTxDeQLimit;
		else if (prAdapter->rQM.u4TxAllowedStaCount == 1)
			u4MaxQuotaLimit = QM_STA_FORWARD_COUNT_UNLIMITED;
		else
			u4MaxQuotaLimit =
				(uint32_t) prTcqStatus->au4MaxNumOfPage[i];

		u4AvailableResourcePLE = nicTxResourceGetPleFreeCount(
			prAdapter, i);

		if (i == BMC_TC_INDEX)
			qmDequeueTxPacketsFromPerTypeQueues(prAdapter,
				&rReturnedQue, (uint8_t)i,
				prTcqStatus->au4FreePageCount[i],
				&u4AvailableResourcePLE,
				u4MaxQuotaLimit);
		else
			qmDequeueTxPacketsFromPerStaQueues(prAdapter,
				&rReturnedQue,
				(uint8_t)i,
				prTcqStatus->au4FreePageCount[i],
				&u4AvailableResourcePLE,
				u4MaxQuotaLimit);

		/* The aggregate number of dequeued packets */
		DBGLOG(QM, TEMP, "DQA)[%u](%u)\n", i,
			rReturnedQue.u4NumElem);
	}

	if (QUEUE_IS_NOT_EMPTY(&rReturnedQue)) {
		prReturnedPacketListHead = (struct MSDU_INFO *)
			QUEUE_GET_HEAD(&rReturnedQue);
		QM_TX_SET_NEXT_MSDU_INFO((struct MSDU_INFO *)
			QUEUE_GET_TAIL(&rReturnedQue), NULL);
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
struct MSDU_INFO *qmDequeueTxPacketsMthread(
	IN struct ADAPTER *prAdapter,
	IN struct TX_TCQ_STATUS *prTcqStatus)
{

	/* INT_32 i; */
	struct MSDU_INFO *prReturnedPacketListHead;
	/* QUE_T rReturnedQue; */
	/* UINT_32 u4MaxQuotaLimit; */
	struct MSDU_INFO *prMsduInfo, *prNextMsduInfo;
	KAL_SPIN_LOCK_DECLARATION();

#if QM_ADAPTIVE_TC_RESOURCE_CTRL
	if (prAdapter->rQM.fgForceReassign)
		qmDoAdaptiveTcResourceCtrl(prAdapter);
#endif

	if (!prAdapter->rQM.fgTcResourcePostHandle) {
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_RESOURCE);

		prReturnedPacketListHead = qmDequeueTxPackets(prAdapter,
		prTcqStatus);

		/* require the resource first to prevent from unsync */
		prMsduInfo = prReturnedPacketListHead;
		while (prMsduInfo) {
			prNextMsduInfo = (struct MSDU_INFO *)
				QUEUE_GET_NEXT_ENTRY
				((struct QUE_ENTRY *) prMsduInfo);
			nicTxAcquireResource(prAdapter, prMsduInfo->ucTC,
				nicTxGetPageCount(prAdapter,
				prMsduInfo->u2FrameLength, FALSE), FALSE);

			prMsduInfo = prNextMsduInfo;
		}
	} else {
		prReturnedPacketListHead = qmDequeueTxPackets(prAdapter,
		prTcqStatus);
	}
	if (prReturnedPacketListHead)
		wlanTxProfilingTagMsdu(prAdapter, prReturnedPacketListHead,
			TX_PROF_TAG_DRV_DEQUE);

	if (!prAdapter->rQM.fgTcResourcePostHandle)
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
u_int8_t
qmAdjustTcQuotasMthread(IN struct ADAPTER *prAdapter,
	OUT struct TX_TCQ_ADJUST *prTcqAdjust,
	IN struct TX_TCQ_STATUS *prTcqStatus)
{
#if QM_ADAPTIVE_TC_RESOURCE_CTRL
	uint32_t i;
	struct QUE_MGT *prQM = &prAdapter->rQM;

	KAL_SPIN_LOCK_DECLARATION();

	/* Must initialize */
	for (i = 0; i < QM_ACTIVE_TC_NUM; i++)
		prTcqAdjust->ai4Variation[i] = 0;

	/* 4 <1> If TC resource is not just adjusted, exit directly */
	if (!prQM->fgTcResourcePostAnnealing)
		return FALSE;

	/* 4 <2> Adjust TcqStatus according to
	 * the updated prQM->au4CurrentTcResource
	 */
	else {
		int32_t i4TotalExtraQuota = 0;
		int32_t ai4ExtraQuota[QM_ACTIVE_TC_NUM];
		u_int8_t fgResourceRedistributed = TRUE;

		/* Must initialize */
		for (i = 0; i < TC_NUM; i++)
			prTcqAdjust->ai4Variation[i] = 0;

		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_RESOURCE);

		/* Obtain the free-to-distribute resource */
		for (i = 0; i < QM_ACTIVE_TC_NUM; i++) {
			ai4ExtraQuota[i] =
				(int32_t) prTcqStatus->au4MaxNumOfBuffer[i] -
				(int32_t) prQM->au4CurrentTcResource[i];

			if (ai4ExtraQuota[i] > 0) {
				/* The resource shall be reallocated
				 * to other TCs
				 */
				if (ai4ExtraQuota[i] >
					prTcqStatus->au4FreeBufferCount[
					i]) {
					ai4ExtraQuota[i] =
						prTcqStatus->
						au4FreeBufferCount[i];
					fgResourceRedistributed = FALSE;
				}

				i4TotalExtraQuota += ai4ExtraQuota[i];
				prTcqAdjust->ai4Variation[i] =
					(-ai4ExtraQuota[i]);
			}
		}

		/* Distribute quotas to TCs which need extra resource
		 * according to prQM->au4CurrentTcResource
		 */
		for (i = 0; i < QM_ACTIVE_TC_NUM; i++) {
			if (ai4ExtraQuota[i] < 0) {
				if ((-ai4ExtraQuota[i]) > i4TotalExtraQuota) {
					ai4ExtraQuota[i] = (-i4TotalExtraQuota);
					fgResourceRedistributed = FALSE;
				}

				i4TotalExtraQuota += ai4ExtraQuota[i];
				prTcqAdjust->ai4Variation[i] =
					(-ai4ExtraQuota[i]);
			}
		}

		/* In case some TC is waiting for TX Done,
		 * continue to adjust TC quotas upon TX Done
		 */
		prQM->fgTcResourcePostAnnealing = (!fgResourceRedistributed);

		for (i = 0; i < TC_NUM; i++) {
			prTcqStatus->au4FreePageCount[i] +=
				(prTcqAdjust->ai4Variation[i] *
				prAdapter->rTxCtrl.u4MaxPageCntPerFrame);
			prTcqStatus->au4MaxNumOfPage[i] +=
				(prTcqAdjust->ai4Variation[i] *
				prAdapter->rTxCtrl.u4MaxPageCntPerFrame);

			prTcqStatus->au4FreeBufferCount[i] +=
				prTcqAdjust->ai4Variation[i];
			prTcqStatus->au4MaxNumOfBuffer[i] +=
				prTcqAdjust->ai4Variation[i];
		}


		/* PLE */
		qmAdjustTcQuotaPle(prAdapter, prTcqAdjust, prTcqStatus);


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

/*----------------------------------------------------------------------------*/
/*!
 * \brief Adjust the TC PLE quotas according to traffic demands
 *
 * \param[out] prTcqAdjust The resulting adjustment
 * \param[in] prTcqStatus Info about the current TC quotas and counters
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
void qmAdjustTcQuotaPle(IN struct ADAPTER *prAdapter,
	OUT struct TX_TCQ_ADJUST *prTcqAdjust,
	IN struct TX_TCQ_STATUS *prTcqStatus)
{
	uint8_t i;
	int32_t i4pages;
	struct TX_CTRL *prTxCtrl;
	struct TX_TCQ_STATUS *prTc;
	int32_t i4TotalExtraQuota = 0;

	ASSERT(prAdapter);

	prTxCtrl = &prAdapter->rTxCtrl;
	prTc = &prTxCtrl->rTc;

	/* no PLE resource control */
	if (!prTc->fgNeedPleCtrl)
		return;

	/* collect free PLE resource */
	for (i = TC0_INDEX; i < TC_NUM; i++) {

		if (!nicTxResourceIsPleCtrlNeeded(prAdapter, i))
			continue;

		/* adjust ple resource */
		i4pages = prTcqAdjust->ai4Variation[i] *
				NIX_TX_PLE_PAGE_CNT_PER_FRAME;

		if (i4pages < 0) {
			/* donate resource to other TC */
			if (prTcqStatus->au4FreePageCount_PLE[i] < (-i4pages)) {
				/* not enough to give */
				i4pages =
					-(prTcqStatus->au4FreePageCount_PLE[i]);
			}
			i4TotalExtraQuota += -i4pages;

			prTcqStatus->au4FreePageCount_PLE[i] += i4pages;
			prTcqStatus->au4MaxNumOfPage_PLE[i] += i4pages;

			prTcqStatus->au4FreeBufferCount_PLE[i] +=
				(i4pages / NIX_TX_PLE_PAGE_CNT_PER_FRAME);
			prTcqStatus->au4MaxNumOfBuffer_PLE[i] +=
				(i4pages / NIX_TX_PLE_PAGE_CNT_PER_FRAME);
		}
	}

	/* distribute PLE resource */
	for (i = TC0_INDEX; i < TC_NUM; i++) {
		if (!nicTxResourceIsPleCtrlNeeded(prAdapter, i))
			continue;

		/* adjust ple resource */
		i4pages = prTcqAdjust->ai4Variation[i] *
				NIX_TX_PLE_PAGE_CNT_PER_FRAME;

		if (i4pages > 0) {
			if (i4TotalExtraQuota >= i4pages) {
				i4TotalExtraQuota -= i4pages;
			} else {
				i4pages = i4TotalExtraQuota;
				i4TotalExtraQuota = 0;
			}
			prTcqStatus->au4FreePageCount_PLE[i] += i4pages;
			prTcqStatus->au4MaxNumOfPage_PLE[i] += i4pages;

			prTcqStatus->au4FreeBufferCount_PLE[i] =
				(prTcqStatus->au4FreePageCount_PLE[i] /
					NIX_TX_PLE_PAGE_CNT_PER_FRAME);
			prTcqStatus->au4MaxNumOfBuffer_PLE[i] =
				(prTcqStatus->au4MaxNumOfBuffer_PLE[i] /
					NIX_TX_PLE_PAGE_CNT_PER_FRAME);
		}
	}

	/* distribute remaining PLE resource */
	while (i4TotalExtraQuota != 0) {
		DBGLOG(QM, INFO,
				"distribute remaining PLE resource[%u]\n",
				i4TotalExtraQuota);
		for (i = TC0_INDEX; i < TC_NUM; i++) {
			if (!nicTxResourceIsPleCtrlNeeded(prAdapter, i))
				continue;

			if (i4TotalExtraQuota >=
				NIX_TX_PLE_PAGE_CNT_PER_FRAME) {
				prTcqStatus->au4FreePageCount_PLE[i] +=
					NIX_TX_PLE_PAGE_CNT_PER_FRAME;
				prTcqStatus->au4MaxNumOfPage_PLE[i] +=
					NIX_TX_PLE_PAGE_CNT_PER_FRAME;

				prTcqStatus->au4FreeBufferCount_PLE[i] += 1;
				prTcqStatus->au4MaxNumOfBuffer_PLE[i] += 1;

				i4TotalExtraQuota -=
					NIX_TX_PLE_PAGE_CNT_PER_FRAME;
			} else {
				/* remaining PLE pages are
				 * not enough for a package
				 */
				prTcqStatus->au4FreePageCount_PLE[i] +=
					i4TotalExtraQuota;
				prTcqStatus->au4MaxNumOfPage_PLE[i] +=
					i4TotalExtraQuota;

				prTcqStatus->au4FreeBufferCount_PLE[i] =
					(prTcqStatus->au4FreePageCount_PLE[i] /
						NIX_TX_PLE_PAGE_CNT_PER_FRAME);
				prTcqStatus->au4MaxNumOfBuffer_PLE[i] =
					(prTcqStatus->au4MaxNumOfPage_PLE[i] /
						NIX_TX_PLE_PAGE_CNT_PER_FRAME);

				i4TotalExtraQuota = 0;
			}
		}
	}

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
u_int8_t qmAdjustTcQuotas(IN struct ADAPTER *prAdapter,
	OUT struct TX_TCQ_ADJUST *prTcqAdjust,
	IN struct TX_TCQ_STATUS *prTcqStatus)
{
#if QM_ADAPTIVE_TC_RESOURCE_CTRL
	uint32_t i;
	struct QUE_MGT *prQM = &prAdapter->rQM;

	/* Must initialize */
	for (i = 0; i < QM_ACTIVE_TC_NUM; i++)
		prTcqAdjust->ai4Variation[i] = 0;

	/* 4 <1> If TC resource is not just adjusted, exit directly */
	if (!prQM->fgTcResourcePostAnnealing)
		return FALSE;
	/* 4 <2> Adjust TcqStatus according to
	 * the updated prQM->au4CurrentTcResource
	 */
	else {
		int32_t i4TotalExtraQuota = 0;
		int32_t ai4ExtraQuota[QM_ACTIVE_TC_NUM];
		u_int8_t fgResourceRedistributed = TRUE;

		/* Obtain the free-to-distribute resource */
		for (i = 0; i < QM_ACTIVE_TC_NUM; i++) {
			ai4ExtraQuota[i] =
				(int32_t) prTcqStatus->au4MaxNumOfBuffer[i] -
				(int32_t) prQM->au4CurrentTcResource[i];

			if (ai4ExtraQuota[i] > 0) {
				/* The resource shall be
				 * reallocated to other TCs
				 */
				if (ai4ExtraQuota[i] >
					prTcqStatus->au4FreeBufferCount[i]) {
					ai4ExtraQuota[i] =
						prTcqStatus->
						au4FreeBufferCount[i];
					fgResourceRedistributed = FALSE;
				}

				i4TotalExtraQuota += ai4ExtraQuota[i];
				prTcqAdjust->ai4Variation[i] =
					(-ai4ExtraQuota[i]);
			}
		}

		/* Distribute quotas to TCs which need extra resource
		 * according to prQM->au4CurrentTcResource
		 */
		for (i = 0; i < QM_ACTIVE_TC_NUM; i++) {
			if (ai4ExtraQuota[i] < 0) {
				if ((-ai4ExtraQuota[i]) > i4TotalExtraQuota) {
					ai4ExtraQuota[i] = (-i4TotalExtraQuota);
					fgResourceRedistributed = FALSE;
				}

				i4TotalExtraQuota += ai4ExtraQuota[i];
				prTcqAdjust->ai4Variation[i] =
					(-ai4ExtraQuota[i]);
			}
		}

		/* In case some TC is waiting for TX Done,
		 * continue to adjust TC quotas upon TX Done
		 */
		prQM->fgTcResourcePostAnnealing = (!fgResourceRedistributed);

#if QM_FAST_TC_RESOURCE_CTRL
		prQM->fgTcResourceFastReaction = FALSE;
#endif

#if QM_PRINT_TC_RESOURCE_CTRL
		DBGLOG(QM, LOUD,
			"QM: Curr Quota [0]=%u [1]=%u [2]=%u [3]=%u [4]=%u [5]=%u\n",
			prTcqStatus->au4FreeBufferCount[0],
			prTcqStatus->au4FreeBufferCount[1],
			prTcqStatus->au4FreeBufferCount[2],
			prTcqStatus->au4FreeBufferCount[3],
			prTcqStatus->au4FreeBufferCount[4],
			prTcqStatus->au4FreeBufferCount[5]);
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
 * \brief Update the average TX queue length for the TC resource control
 *        mechanism
 *
 * \param (none)
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
void qmUpdateAverageTxQueLen(IN struct ADAPTER *prAdapter)
{
	int32_t u4CurrQueLen, u4Tc, u4StaRecIdx;
	struct STA_RECORD *prStaRec;
	struct QUE_MGT *prQM = &prAdapter->rQM;
	struct BSS_INFO *prBssInfo;

	/* 4 <1> Update the queue lengths for TC0 to TC3 (skip TC4) and TC5 */
	for (u4Tc = 0; u4Tc < QM_ACTIVE_TC_NUM; u4Tc++) {
		u4CurrQueLen = 0;

		/* Calculate per-STA queue length */
		if (u4Tc < NUM_OF_PER_STA_TX_QUEUES) {
			for (u4StaRecIdx = 0; u4StaRecIdx < CFG_STA_REC_NUM;
			     u4StaRecIdx++) {
				prStaRec = cnmGetStaRecByIndex(prAdapter,
					u4StaRecIdx);
				if (prStaRec) {
					prBssInfo = GET_BSS_INFO_BY_INDEX(
						prAdapter,
						prStaRec->ucBssIndex);

					/* If the STA is activated,
					 * get the queue length
					 */
					if ((prStaRec->fgIsValid) &&
						(!prBssInfo->fgIsNetAbsent))
						u4CurrQueLen +=
							(prStaRec->
							arTxQueue[u4Tc].
							u4NumElem);
				}
			}
		}

		if (u4Tc == BMC_TC_INDEX) {
			/* Update the queue length for (BMCAST) */
			u4CurrQueLen += prQM->arTxQueue[
			TX_QUEUE_INDEX_BMCAST].u4NumElem;
		}

		if (prQM->au4AverageQueLen[u4Tc] == 0) {
			prQM->au4AverageQueLen[u4Tc] = (u4CurrQueLen <<
				prQM->u4QueLenMovingAverage);
		} else {
			prQM->au4AverageQueLen[u4Tc] -=
				(prQM->au4AverageQueLen[u4Tc] >>
				 prQM->u4QueLenMovingAverage);
			prQM->au4AverageQueLen[u4Tc] += (u4CurrQueLen);
		}
	}
#if 0
	/* Update the queue length for TC5 (BMCAST) */
	u4CurrQueLen =
		prQM->arTxQueue[TX_QUEUE_INDEX_BMCAST].u4NumElem;

	if (prQM->au4AverageQueLen[TC5_INDEX] == 0) {
		prQM->au4AverageQueLen[TC5_INDEX] = (u4CurrQueLen <<
			QM_QUE_LEN_MOVING_AVE_FACTOR);
	} else {
		prQM->au4AverageQueLen[TC5_INDEX] -=
			(prQM->au4AverageQueLen[TC5_INDEX] >>
			QM_QUE_LEN_MOVING_AVE_FACTOR);
		prQM->au4AverageQueLen[TC5_INDEX] += (u4CurrQueLen);
	}
#endif
}

void qmAllocateResidualTcResource(IN struct ADAPTER *prAdapter,
	IN int32_t *ai4TcResDemand,
	IN uint32_t *pu4ResidualResource,
	IN uint32_t *pu4ShareCount)
{
	struct QUE_MGT *prQM = &prAdapter->rQM;
	uint32_t u4Share = 0;
	uint32_t u4TcIdx;
	uint8_t ucIdx;
	uint32_t au4AdjTc[] = { TC3_INDEX, TC2_INDEX, TC1_INDEX, TC0_INDEX };
	uint32_t u4AdjTcSize = (sizeof(au4AdjTc) / sizeof(uint32_t));
	uint32_t u4ResidualResource = *pu4ResidualResource;
	uint32_t u4ShareCount = *pu4ShareCount;

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
					prQM->au4CurrentTcResource[u4TcIdx] +=
						u4Share;
					u4ResidualResource -= u4Share;
					ai4TcResDemand[u4TcIdx] -= u4Share;
				} else {
					prQM->au4CurrentTcResource[u4TcIdx] +=
						ai4TcResDemand[u4TcIdx];
					u4ResidualResource -=
						ai4TcResDemand[u4TcIdx];
					ai4TcResDemand[u4TcIdx] = 0;
				}
			}
		}
	}

	/* By priority, allocate the left resource
	 * that is not divisible by u4Share
	 */
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
 * \brief Assign TX resource for each TC according to TX queue length and
 *        current assignment
 *
 * \param (none)
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
void qmReassignTcResource(IN struct ADAPTER *prAdapter)
{
	int32_t i4TotalResourceDemand = 0;
	uint32_t u4ResidualResource = 0;
	uint32_t u4TcIdx;
	int32_t ai4TcResDemand[QM_ACTIVE_TC_NUM];
	uint32_t u4ShareCount = 0;
	uint32_t u4Share = 0;
	struct QUE_MGT *prQM = &prAdapter->rQM;
	uint32_t u4ActiveTcCount = 0;
	uint32_t u4LastActiveTcIdx = TC3_INDEX;

	/* Note: After the new assignment is obtained,
	 * set prQM->fgTcResourcePostAnnealing to TRUE to
	 * start the TC-quota adjusting procedure,
	 * which will be invoked upon every TX Done
	 */

	/* 4 <1> Determine the demands */
	/* Determine the amount of extra resource
	 * to fulfill all of the demands
	 */
	for (u4TcIdx = 0; u4TcIdx < QM_ACTIVE_TC_NUM; u4TcIdx++) {
		/* Skip TC4, which is not adjustable */
		if (u4TcIdx == TC4_INDEX)
			continue;

		/* Define: extra_demand = que_length +
		 * min_reserved_quota - current_quota
		 */
		ai4TcResDemand[u4TcIdx] = ((int32_t)(QM_GET_TX_QUEUE_LEN(
			prAdapter, u4TcIdx) +
			prQM->au4MinReservedTcResource[u4TcIdx]) -
			(int32_t)prQM->au4CurrentTcResource[u4TcIdx]);

		/* If there are queued packets, allocate extra resource
		 * for the TC (for TCP consideration)
		 */
		if (QM_GET_TX_QUEUE_LEN(prAdapter, u4TcIdx)) {
			ai4TcResDemand[u4TcIdx] +=
				prQM->u4ExtraReservedTcResource;
			u4ActiveTcCount++;
		}

		i4TotalResourceDemand += ai4TcResDemand[u4TcIdx];
	}

	/* 4 <2> Case 1: Demand <= Total Resource */
	if (i4TotalResourceDemand <= 0) {

		/* 4 <2.1> Calculate the residual resource evenly */
		/* excluding TC4 */
		if (u4ActiveTcCount == 0)
			u4ShareCount = (QM_ACTIVE_TC_NUM - 1);
		else
			u4ShareCount = u4ActiveTcCount;

		u4ResidualResource = (uint32_t) (-i4TotalResourceDemand);
		u4Share = (u4ResidualResource / u4ShareCount);

		/* 4 <2.2> Satisfy every TC and share
		 * the residual resource evenly
		 */
		for (u4TcIdx = 0; u4TcIdx < QM_ACTIVE_TC_NUM; u4TcIdx++) {
			/* Skip TC4 (not adjustable) */
			if (u4TcIdx == TC4_INDEX)
				continue;

			prQM->au4CurrentTcResource[u4TcIdx] +=
				ai4TcResDemand[u4TcIdx];

			/* Every TC is fully satisfied */
			ai4TcResDemand[u4TcIdx] = 0;

			/* The left resource will be allocated */
			if (QM_GET_TX_QUEUE_LEN(prAdapter, u4TcIdx) ||
				(u4ActiveTcCount == 0)) {
				prQM->au4CurrentTcResource[u4TcIdx] += u4Share;
				u4ResidualResource -= u4Share;
				u4LastActiveTcIdx = u4TcIdx;
			}
		}

		/* 4 <2.3> Allocate the left resource to last active TC */
		prQM->au4CurrentTcResource[u4LastActiveTcIdx] +=
			(u4ResidualResource);

	}
	/* 4 <3> Case 2: Demand > Total Resource --> Guarantee
	 * a minimum amount of resource for each TC
	 */
	else {
		u4ShareCount = 0;
		u4ResidualResource = prQM->u4ResidualTcResource;

		/* 4 <3.1> Allocated resouce amount  = minimum
		 * of (guaranteed, total demand)
		 */
		for (u4TcIdx = 0; u4TcIdx < QM_ACTIVE_TC_NUM; u4TcIdx++) {
			/* Skip TC4 (not adjustable) */
			if (u4TcIdx == TC4_INDEX)
				continue;

			/* The demand can be fulfilled with
			 * the guaranteed resource amount
			 */
			if ((prQM->au4CurrentTcResource[u4TcIdx] +
			     ai4TcResDemand[u4TcIdx]) <=
			    prQM->au4GuaranteedTcResource[u4TcIdx]) {

				prQM->au4CurrentTcResource[u4TcIdx] +=
					ai4TcResDemand[u4TcIdx];
				u4ResidualResource +=
					(prQM->au4GuaranteedTcResource[u4TcIdx]
					- prQM->au4CurrentTcResource[u4TcIdx]);
				ai4TcResDemand[u4TcIdx] = 0;
			}

			/* The demand can not be fulfilled with
			 * the guaranteed resource amount
			 */
			else {
				ai4TcResDemand[u4TcIdx] -=
					(prQM->au4GuaranteedTcResource[u4TcIdx]
					- prQM->au4CurrentTcResource[u4TcIdx]);

				prQM->au4CurrentTcResource[u4TcIdx] =
					prQM->au4GuaranteedTcResource[u4TcIdx];
				u4ShareCount++;
			}
		}

		/* 4 <3.2> Allocate the residual resource */
		qmAllocateResidualTcResource(prAdapter, ai4TcResDemand,
			&u4ResidualResource, &u4ShareCount);
	}

	prQM->fgTcResourcePostAnnealing = TRUE;

#if QM_PRINT_TC_RESOURCE_CTRL
	/* Debug print */
	DBGLOG(QM, INFO,
		"QM: TC Rsc adjust to [%03u:%03u:%03u:%03u:%03u:%03u]\n",
		prQM->au4CurrentTcResource[0],
		prQM->au4CurrentTcResource[1],
		prQM->au4CurrentTcResource[2],
		prQM->au4CurrentTcResource[3],
		prQM->au4CurrentTcResource[4],
		prQM->au4CurrentTcResource[5]);
#endif

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Adjust TX resource for each TC according to TX queue length and
 *        current assignment
 *
 * \param (none)
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
void qmDoAdaptiveTcResourceCtrl(IN struct ADAPTER	*prAdapter)
{
	struct QUE_MGT *prQM = &prAdapter->rQM;

	if (prQM->fgForceReassign) {
		prQM->u4TimeToUpdateQueLen = 1;
		prQM->u4TimeToAdjustTcResource = 1;
		prQM->fgTcResourceFastReaction = TRUE;

		prQM->fgForceReassign = FALSE;
	}

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
			/* Upon the next qmUpdateAverageTxQueLen function call,
			 * do this check again
			 */
			prQM->u4TimeToAdjustTcResource = 1;
		}

		/* The last assignment has been applied */
		else {
			prQM->u4TimeToAdjustTcResource =
				QM_INIT_TIME_TO_ADJUST_TC_RSC;
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
		uint32_t u4Tc;

		for (u4Tc = 0; u4Tc < QM_ACTIVE_TC_NUM; u4Tc++) {
			if (QM_GET_TX_QUEUE_LEN(prAdapter, u4Tc) >= 100) {
				log_dbg(QM, LOUD, "QM: QueLen [%ld %ld %ld %ld %ld %ld]\n",
					QM_GET_TX_QUEUE_LEN(prAdapter, 0),
					QM_GET_TX_QUEUE_LEN(prAdapter, 1),
					QM_GET_TX_QUEUE_LEN(prAdapter, 2),
					QM_GET_TX_QUEUE_LEN(prAdapter, 3),
					QM_GET_TX_QUEUE_LEN(prAdapter, 4),
					QM_GET_TX_QUEUE_LEN(prAdapter, 5));
				break;
			}
		}
	} while (FALSE);
#endif
}

#if QM_FAST_TC_RESOURCE_CTRL
void qmCheckForFastTcResourceCtrl(IN struct ADAPTER *prAdapter,
	IN uint8_t ucTc)
{
	struct QUE_MGT *prQM = &prAdapter->rQM;
	u_int8_t fgTrigger = FALSE;

	if (!prAdapter->rTxCtrl.rTc.au4FreeBufferCount[ucTc]
	|| ((prAdapter->rTxCtrl.rTc.fgNeedPleCtrl) &&
	(!prAdapter->rTxCtrl.rTc.au4FreeBufferCount_PLE[ucTc]))) {
		if (!prQM->au4CurrentTcResource[ucTc] ||
			nicTxGetAdjustableResourceCnt(prAdapter))
			fgTrigger = TRUE;
	}

	/* Trigger TC resource adjustment
	 * if there is a requirement coming for a empty TC
	 */
	if (fgTrigger) {
		prQM->fgForceReassign = TRUE;

		DBGLOG(QM, LOUD,
			"Trigger TC Resource adjustment for TC[%u]\n", ucTc);
	}
}
#endif

#endif

uint32_t gmGetDequeueQuota(
	IN struct ADAPTER *prAdapter,
	IN struct STA_RECORD *prStaRec,
	IN struct BSS_INFO *prBssInfo,
	IN uint32_t			u4TotalQuota
)
{
	uint32_t	u4Weight = 100;
	uint32_t	u4Quota;

	struct QUE_MGT *prQM = &prAdapter->rQM;

	if ((prAdapter->rWifiVar.uDeQuePercentEnable == FALSE) ||
		(prQM->fgIsTxResrouceControlEn == FALSE))
		return u4TotalQuota;

	if (prStaRec->ucDesiredPhyTypeSet & PHY_TYPE_BIT_VHT) {
		if (prBssInfo->ucVhtChannelWidth >
			VHT_OP_CHANNEL_WIDTH_20_40) {
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
void qmInitRxQueues(IN struct ADAPTER *prAdapter)
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
struct SW_RFB *qmHandleRxPackets(IN struct ADAPTER *prAdapter,
	IN struct SW_RFB *prSwRfbListHead)
{

#if CFG_RX_REORDERING_ENABLED
	struct SW_RFB *prCurrSwRfb;
	struct SW_RFB *prNextSwRfb;
	struct HW_MAC_RX_DESC *prRxStatus;
	struct QUE rReturnedQue;
	struct QUE *prReturnedQue;
	uint8_t *pucEthDestAddr;
	u_int8_t fgIsBMC, fgIsHTran;
	u_int8_t fgMicErr;
#if CFG_SUPPORT_REPLAY_DETECTION
	u_int8_t ucBssIndexRly = 0;
	struct BSS_INFO *prBssInfoRly = NULL;
#endif

	DEBUGFUNC("qmHandleRxPackets");

	ASSERT(prSwRfbListHead);

	prReturnedQue = &rReturnedQue;

	QUEUE_INITIALIZE(prReturnedQue);
	prNextSwRfb = prSwRfbListHead;

	do {
		prCurrSwRfb = prNextSwRfb;
		prNextSwRfb = QM_RX_GET_NEXT_SW_RFB(prCurrSwRfb);

		prRxStatus = prCurrSwRfb->prRxStatus;
		if (prRxStatus->u2RxByteCount > CFG_RX_MAX_PKT_SIZE) {
			prCurrSwRfb->eDst = RX_PKT_DESTINATION_NULL;
			QUEUE_INSERT_TAIL(prReturnedQue,
				(struct QUE_ENTRY *) prCurrSwRfb);
			DBGLOG(QM, ERROR,
				"Drop packet when packet length is larger than CFG_RX_MAX_PKT_SIZE. Packet length=%d\n",
				prRxStatus->u2RxByteCount);
			continue;
		}
		/* TODO: (Tehuang) Check if relaying */
		prCurrSwRfb->eDst = RX_PKT_DESTINATION_HOST;

		/* Decide the Destination */
#if CFG_RX_PKTS_DUMP
		if (prAdapter->rRxCtrl.u4RxPktsDumpTypeMask & BIT(
			    HIF_RX_PKT_TYPE_DATA)) {
			log_dbg(SW4, INFO, "QM RX DATA: net _u sta idx %u wlan idx %u ssn _u tid %u ptype %u 11 %u\n",
				prCurrSwRfb->ucStaRecIdx, prRxStatus->ucWlanIdx,
				HAL_RX_STATUS_GET_TID(prRxStatus),
				prCurrSwRfb->ucPacketType,
				prCurrSwRfb->fgReorderBuffer);

			DBGLOG_MEM8(SW4, TRACE,
				(uint8_t *) prCurrSwRfb->pvHeader,
				prCurrSwRfb->u2PacketLen);
		}
#endif

		fgIsBMC = HAL_RX_STATUS_IS_BC(prRxStatus) |
			HAL_RX_STATUS_IS_MC(prRxStatus);
		fgIsHTran = FALSE;
		if (HAL_RX_STATUS_GET_HEADER_TRAN(prRxStatus) == TRUE) {
			/* (!HIF_RX_HDR_GET_80211_FLAG(prHifRxHdr)){ */

			uint8_t ucBssIndex;
			struct BSS_INFO *prBssInfo;
			uint8_t aucTaAddr[MAC_ADDR_LEN];

			fgIsHTran = TRUE;
			pucEthDestAddr = prCurrSwRfb->pvHeader;
			if (prCurrSwRfb->prRxStatusGroup4 == NULL) {
				DBGLOG(QM, ERROR,
					"H/W did Header Trans but prRxStatusGroup4 is NULL !!!\n");
				DBGLOG_MEM8(QM, ERROR, prCurrSwRfb->pucRecvBuff,
					HAL_RX_STATUS_GET_RX_BYTE_CNT(
					prRxStatus));
				prCurrSwRfb->eDst = RX_PKT_DESTINATION_NULL;
				QUEUE_INSERT_TAIL(prReturnedQue,
					(struct QUE_ENTRY *)
					prCurrSwRfb);
				DBGLOG(RX, WARN,
				       "rxStatusGroup4 for data packet is NULL, drop this packet, and dump RXD and Packet\n");
				DBGLOG_MEM8(RX, WARN, (uint8_t *) prRxStatus,
					sizeof(*prRxStatus));
				if (prCurrSwRfb->pvHeader)
					DBGLOG_MEM8(RX, WARN,
						prCurrSwRfb->pvHeader,
						prCurrSwRfb->u2PacketLen >
						 32 ? 32 :
						prCurrSwRfb->u2PacketLen);
#if 0
				glGetRstReason(RST_GROUP4_NULL);
				GL_RESET_TRIGGER(prAdapter,
					RST_FLAG_DO_CORE_DUMP);
#endif
				continue;
			}

			if (prCurrSwRfb->prStaRec == NULL) {
				/* Workaround WTBL Issue */
				HAL_RX_STATUS_GET_TA(
					prCurrSwRfb->prRxStatusGroup4,
					aucTaAddr);
				prCurrSwRfb->ucStaRecIdx =
					secLookupStaRecIndexFromTA(
					prAdapter, aucTaAddr);
				if (prCurrSwRfb->ucStaRecIdx <
					CFG_STA_REC_NUM) {
					prCurrSwRfb->prStaRec =
						cnmGetStaRecByIndex(prAdapter,
							prCurrSwRfb->
							ucStaRecIdx);
#define __STR_FMT__ \
	"Re-search the staRec = %d, mac = " MACSTR ", byteCnt= %d\n"
					log_dbg(QM, TRACE,
						__STR_FMT__,
						prCurrSwRfb->ucStaRecIdx,
						MAC2STR(aucTaAddr),
						prRxStatus->u2RxByteCount);
#undef __STR_FMT__
				}

				if (prCurrSwRfb->prStaRec == NULL) {
					DBGLOG(QM, TRACE,
						"Mark NULL the Packet for no STA_REC, wlanIdx=%d\n",
						prRxStatus->ucWlanIdx);
					RX_INC_CNT(&prAdapter->rRxCtrl,
						RX_NO_STA_DROP_COUNT);
					prCurrSwRfb->eDst =
						RX_PKT_DESTINATION_NULL;
					QUEUE_INSERT_TAIL(prReturnedQue,
						(struct QUE_ENTRY *)
						prCurrSwRfb);
					continue;
				}

				prCurrSwRfb->ucWlanIdx =
					prCurrSwRfb->prStaRec->ucWlanIndex;
				GLUE_SET_PKT_BSS_IDX(prCurrSwRfb->pvPacket,
					secGetBssIdxByWlanIdx(prAdapter,
						prCurrSwRfb->ucWlanIdx));
			}

			if (prCurrSwRfb->ucTid >= CFG_RX_MAX_BA_TID_NUM) {
				log_dbg(QM, ERROR, "TID from RXD = %d, out of range !!!\n",
					prCurrSwRfb->ucTid);
				DBGLOG_MEM8(QM, ERROR,
					prCurrSwRfb->pucRecvBuff,
					HAL_RX_STATUS_GET_RX_BYTE_CNT(
					prRxStatus));
				prCurrSwRfb->eDst = RX_PKT_DESTINATION_NULL;
				QUEUE_INSERT_TAIL(prReturnedQue,
					(struct QUE_ENTRY *) prCurrSwRfb);
				continue;
			}

			if (prAdapter->rRxCtrl.rFreeSwRfbList.u4NumElem >
				(CFG_RX_MAX_PKT_NUM -
					CFG_NUM_OF_QM_RX_PKT_NUM) || TRUE) {

				ucBssIndex = prCurrSwRfb->prStaRec->ucBssIndex;
				prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
					ucBssIndex);

				if (!IS_BSS_ACTIVE(prBssInfo)) {
					log_dbg(QM, TRACE, "Mark NULL the Packet for inactive Bss %u\n",
						ucBssIndex);
					RX_INC_CNT(&prAdapter->rRxCtrl,
						RX_INACTIVE_BSS_DROP_COUNT);
					prCurrSwRfb->eDst =
						RX_PKT_DESTINATION_NULL;
					QUEUE_INSERT_TAIL(prReturnedQue,
						(struct QUE_ENTRY *)
						prCurrSwRfb);
					continue;
				}

				if (prBssInfo->eCurrentOPMode ==
					OP_MODE_ACCESS_POINT) {
					if (IS_BMCAST_MAC_ADDR(
						pucEthDestAddr)) {
					prCurrSwRfb->eDst =
					RX_PKT_DESTINATION_HOST_WITH_FORWARD;
					} else if (
						secLookupStaRecIndexFromTA(
						prAdapter,
						pucEthDestAddr)
						!=
						STA_REC_INDEX_NOT_FOUND) {

						prCurrSwRfb->eDst =
						RX_PKT_DESTINATION_FORWARD;
					}
				}
#if CFG_SUPPORT_PASSPOINT
				else if (hs20IsFrameFilterEnabled(prAdapter,
					prBssInfo) &&
					hs20IsUnsecuredFrame(prAdapter,
					prBssInfo,
					prCurrSwRfb)) {
					DBGLOG(QM, WARN,
						"Mark NULL the Packet for Dropped Packet %u\n",
						ucBssIndex);
					RX_INC_CNT(&prAdapter->rRxCtrl,
						RX_HS20_DROP_COUNT);
					prCurrSwRfb->eDst =
						RX_PKT_DESTINATION_NULL;
					QUEUE_INSERT_TAIL(prReturnedQue,
						(struct QUE_ENTRY *)
						prCurrSwRfb);
					continue;
				}
#endif /* CFG_SUPPORT_PASSPOINT */

			} else {
				/* Dont not occupy other SW RFB */
				DBGLOG(QM, TRACE,
					"Mark NULL the Packet for less Free Sw Rfb\n");
				RX_INC_CNT(&prAdapter->rRxCtrl,
					RX_LESS_SW_RFB_DROP_COUNT);
				prCurrSwRfb->eDst = RX_PKT_DESTINATION_NULL;
				QUEUE_INSERT_TAIL(prReturnedQue,
					(struct QUE_ENTRY *) prCurrSwRfb);
				continue;
			}

		} else {
			uint16_t u2FrameCtrl = 0;
			struct WLAN_MAC_HEADER *prWlanHeader = NULL;

			prWlanHeader = (struct WLAN_MAC_HEADER *)
				prCurrSwRfb->pvHeader;
			u2FrameCtrl = prWlanHeader->u2FrameCtrl;
			if (prCurrSwRfb->prStaRec == NULL &&
				RXM_IS_DATA_FRAME(u2FrameCtrl) &&
				(prAdapter->prAisBssInfo) &&
				(prAdapter->prAisBssInfo->eConnectionState ==
				PARAM_MEDIA_STATE_CONNECTED)) {
				/* rx header translation */
				log_dbg(QM, INFO, "RXD Trans: FrameCtrl=0x%02x GVLD=0x%x, StaRecIdx=%d, WlanIdx=%d PktLen=%d\n",
					u2FrameCtrl, prCurrSwRfb->ucGroupVLD,
					prCurrSwRfb->ucStaRecIdx,
					prCurrSwRfb->ucWlanIdx,
					prCurrSwRfb->u2PacketLen);

				if (prAdapter->prAisBssInfo
				    && prAdapter->prAisBssInfo->prStaRecOfAP)
				if (EQUAL_MAC_ADDR(
							prWlanHeader->
							aucAddr1,
							prAdapter->
							prAisBssInfo->
							aucOwnMacAddr)
				    && EQUAL_MAC_ADDR(
							prWlanHeader->
							aucAddr2,
							prAdapter->
							prAisBssInfo->
							aucBSSID)) {
					uint16_t u2MACLen = 0;
					/* QoS data, VHT */
					if (RXM_IS_QOS_DATA_FRAME(
						u2FrameCtrl))
						u2MACLen = sizeof(
						struct WLAN_MAC_HEADER_QOS);
					else
						u2MACLen = sizeof(
							struct
							WLAN_MAC_HEADER);
					u2MACLen +=
						ETH_LLC_LEN +
						ETH_SNAP_OUI_LEN;
					u2MACLen -=
						ETHER_TYPE_LEN_OFFSET;
					prCurrSwRfb->pvHeader +=
						u2MACLen;
					kalMemCopy(
						prCurrSwRfb->pvHeader,
						prWlanHeader->aucAddr1,
						MAC_ADDR_LEN);
					kalMemCopy(
						prCurrSwRfb->pvHeader +
						MAC_ADDR_LEN,
						prWlanHeader->aucAddr2,
						MAC_ADDR_LEN);
					prCurrSwRfb->u2PacketLen -=
						u2MACLen;

					/* record StaRec related info */
					prCurrSwRfb->prStaRec =
						prAdapter->
						prAisBssInfo->
						prStaRecOfAP;
					prCurrSwRfb->ucStaRecIdx =
						prCurrSwRfb->prStaRec->
						ucIndex;
					prCurrSwRfb->ucWlanIdx =
						prCurrSwRfb->prStaRec->
						ucWlanIndex;
					GLUE_SET_PKT_BSS_IDX(
						prCurrSwRfb->pvPacket,
						secGetBssIdxByWlanIdx(
							prAdapter,
							prCurrSwRfb->
							ucWlanIdx));
					DBGLOG_MEM8(QM, WARN,
						(uint8_t *)
						prCurrSwRfb->pvHeader,
						(prCurrSwRfb->
						u2PacketLen > 64) ? 64 :
						prCurrSwRfb->
						u2PacketLen);
				}
			}
		}

#if CFG_SUPPORT_WAPI
		if (prCurrSwRfb->u2PacketLen > ETHER_HEADER_LEN) {
			uint8_t *pc = (uint8_t *) prCurrSwRfb->pvHeader;
			uint16_t u2Etype = 0;

			u2Etype = (pc[ETHER_TYPE_LEN_OFFSET] << 8) |
				  (pc[ETHER_TYPE_LEN_OFFSET + 1]);
			/* for wapi integrity test. WPI_1x packet should be
			 * always in non-encrypted mode. if we received any
			 * WPI(0x88b4) packet that is encrypted, drop here.
			 */
			if (u2Etype == ETH_WPI_1X &&
			    HAL_RX_STATUS_GET_SEC_MODE(prRxStatus) != 0 &&
			    HAL_RX_STATUS_IS_CIPHER_MISMATCH(prRxStatus) == 0) {
				DBGLOG(QM, INFO,
					"drop wpi packet with sec mode\n");
				prCurrSwRfb->eDst = RX_PKT_DESTINATION_NULL;
				QUEUE_INSERT_TAIL(prReturnedQue,
					(struct QUE_ENTRY *) prCurrSwRfb);
				continue;
			}
		}
#endif


		/* Todo:: Move the data class error check here */

#if CFG_SUPPORT_REPLAY_DETECTION
		if (prCurrSwRfb->prStaRec) {
			ucBssIndexRly = prCurrSwRfb->prStaRec->ucBssIndex;
			prBssInfoRly = GET_BSS_INFO_BY_INDEX(prAdapter,
				ucBssIndexRly);
			if (prBssInfoRly && !IS_BSS_ACTIVE(prBssInfoRly)) {
				DBGLOG(QM, INFO,
					"Mark NULL the Packet for inactive Bss %u\n",
					ucBssIndexRly);
				prCurrSwRfb->eDst = RX_PKT_DESTINATION_NULL;
				QUEUE_INSERT_TAIL(prReturnedQue,
					(struct QUE_ENTRY *) prCurrSwRfb);
				continue;
			}
		}
		if (fgIsBMC && prBssInfoRly && IS_BSS_AIS(prBssInfoRly) &&
			qmHandleRxReplay(prAdapter, prCurrSwRfb)) {
			prCurrSwRfb->eDst = RX_PKT_DESTINATION_NULL;
			QUEUE_INSERT_TAIL(prReturnedQue,
				(struct QUE_ENTRY *) prCurrSwRfb);
			continue;
		}
#endif

		if (prCurrSwRfb->fgReorderBuffer && !fgIsBMC && fgIsHTran) {
			/* If this packet should dropped or indicated to the
			 * host immediately, it should be enqueued into the
			 * rReturnedQue with specific flags. If this packet
			 * should be buffered for reordering, it should be
			 * enqueued into the reordering queue in the STA_REC
			 * rather than into the rReturnedQue.
			 */
			qmProcessPktWithReordering(prAdapter, prCurrSwRfb,
				prReturnedQue);

		} else if (prCurrSwRfb->fgDataFrame) {
			/* Check Class Error */
			if (prCurrSwRfb->prStaRec &&
				(secCheckClassError(prAdapter, prCurrSwRfb,
				prCurrSwRfb->prStaRec) == TRUE)) {
				struct RX_BA_ENTRY *prReorderQueParm = NULL;

				if (!fgIsBMC && fgIsHTran &&
					(HAL_RX_STATUS_GET_FRAME_CTL_FIELD(
					prCurrSwRfb->prRxStatusGroup4) &
					MASK_FRAME_TYPE) != MAC_FRAME_DATA) {
					prReorderQueParm =
						((prCurrSwRfb->prStaRec->
						aprRxReorderParamRefTbl)[
						prCurrSwRfb->ucTid]);
				}

				if (prReorderQueParm &&
					prReorderQueParm->fgIsValid) {
					/* Only QoS Data frame with BA aggrement
					 * shall enter reordering buffer
					 */
					qmProcessPktWithReordering(prAdapter,
						prCurrSwRfb,
						prReturnedQue);
				} else
					qmHandleRxPackets_AOSP_1;
			} else {
				DBGLOG(QM, TRACE,
					"Mark NULL the Packet for class error\n");
				RX_INC_CNT(&prAdapter->rRxCtrl,
					RX_CLASS_ERR_DROP_COUNT);
				prCurrSwRfb->eDst = RX_PKT_DESTINATION_NULL;
				QUEUE_INSERT_TAIL(prReturnedQue,
					(struct QUE_ENTRY *) prCurrSwRfb);
			}
		} else {
			struct WLAN_MAC_HEADER *prWlanMacHeader;

			ASSERT(prCurrSwRfb->pvHeader);

			prWlanMacHeader = (struct WLAN_MAC_HEADER *)
				prCurrSwRfb->pvHeader;
			prCurrSwRfb->eDst = RX_PKT_DESTINATION_NULL;

			switch (prWlanMacHeader->u2FrameCtrl &
				MASK_FRAME_TYPE) {
			/* BAR frame */
			case MAC_FRAME_BLOCK_ACK_REQ:
				qmProcessBarFrame(prAdapter,
					prCurrSwRfb, prReturnedQue);
				RX_INC_CNT(&prAdapter->rRxCtrl,
					RX_BAR_DROP_COUNT);
				break;
			default:
				DBGLOG(QM, TRACE,
					"Mark NULL the Packet for non-interesting type\n");
				RX_INC_CNT(&prAdapter->rRxCtrl,
					RX_NO_INTEREST_DROP_COUNT);
				QUEUE_INSERT_TAIL(prReturnedQue,
					(struct QUE_ENTRY *) prCurrSwRfb);
				break;
			}
		}

	} while (prNextSwRfb);

	/* The returned list of SW_RFBs must end with a NULL pointer */
	if (QUEUE_IS_NOT_EMPTY(prReturnedQue))
		QM_TX_SET_NEXT_MSDU_INFO((struct SW_RFB *) QUEUE_GET_TAIL(
			prReturnedQue), NULL);

	return (struct SW_RFB *) QUEUE_GET_HEAD(prReturnedQue);

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
void qmProcessPktWithReordering(IN struct ADAPTER *prAdapter,
	IN struct SW_RFB *prSwRfb,
	OUT struct QUE *prReturnedQue)
{

	struct STA_RECORD *prStaRec;
	struct HW_MAC_RX_DESC *prRxStatus;
	struct RX_BA_ENTRY *prReorderQueParm;

#if CFG_SUPPORT_RX_AMSDU
	uint8_t u8AmsduSubframeIdx;
	uint32_t u4SeqNo;
#endif
	DEBUGFUNC("qmProcessPktWithReordering");

	ASSERT(prSwRfb);
	ASSERT(prReturnedQue);
	ASSERT(prSwRfb->prRxStatus);

	/* We should have STA_REC here */
	prStaRec = prSwRfb->prStaRec;
	ASSERT(prStaRec);
	ASSERT(prSwRfb->ucTid < CFG_RX_MAX_BA_TID_NUM);

	prRxStatus = prSwRfb->prRxStatus;

	if (prSwRfb->ucTid >= CFG_RX_MAX_BA_TID_NUM) {
		DBGLOG(QM, WARN, "TID from RXD = %d, out of range!!\n",
			prSwRfb->ucTid);
		DBGLOG_MEM8(QM, ERROR, prSwRfb->pucRecvBuff,
			HAL_RX_STATUS_GET_RX_BYTE_CNT(prRxStatus));
		prSwRfb->eDst = RX_PKT_DESTINATION_NULL;
		QUEUE_INSERT_TAIL(prReturnedQue,
			(struct QUE_ENTRY *) prSwRfb);
		return;
	}

	/* Check whether the BA agreement exists */
	prReorderQueParm = ((
		prStaRec->aprRxReorderParamRefTbl)[prSwRfb->ucTid]);
	if (!prReorderQueParm || !(prReorderQueParm->fgIsValid)) {
		DBGLOG(QM, TRACE,
			"Reordering but no BA agreement for STA[%d] TID[%d]\n",
			prStaRec->ucIndex, prSwRfb->ucTid);
		QUEUE_INSERT_TAIL(prReturnedQue,
			(struct QUE_ENTRY *) prSwRfb);
		return;
	}

	RX_INC_CNT(&prAdapter->rRxCtrl,
		RX_DATA_REORDER_TOTAL_COUNT);

	prSwRfb->u2SSN = HAL_RX_STATUS_GET_SEQFrag_NUM(
		prSwRfb->prRxStatusGroup4) >> RX_STATUS_SEQ_NUM_OFFSET;

#if CFG_SUPPORT_RX_AMSDU
	/* RX reorder for one MSDU in AMSDU issue */
	/* QUEUE_INITIALIZE(&prSwRfb->rAmsduQue); */

	u8AmsduSubframeIdx = HAL_RX_STATUS_GET_PAYLOAD_FORMAT(prRxStatus);

	/* prMpduSwRfb = prReorderQueParm->prMpduSwRfb; */
	u4SeqNo = (uint32_t)prSwRfb->u2SSN;

	switch (u8AmsduSubframeIdx) {
	case RX_PAYLOAD_FORMAT_FIRST_SUB_AMSDU:
		if (prReorderQueParm->fgAmsduNeedLastFrame) {
			RX_INC_CNT(&prAdapter->rRxCtrl,
				RX_DATA_AMSDU_MISS_COUNT);
			prReorderQueParm->fgAmsduNeedLastFrame = FALSE;
		}
		RX_INC_CNT(&prAdapter->rRxCtrl,
			   RX_DATA_MSDU_IN_AMSDU_COUNT);
		RX_INC_CNT(&prAdapter->rRxCtrl, RX_DATA_AMSDU_COUNT);
		break;

	case RX_PAYLOAD_FORMAT_MIDDLE_SUB_AMSDU:
		prReorderQueParm->fgAmsduNeedLastFrame = TRUE;
		RX_INC_CNT(&prAdapter->rRxCtrl,
			   RX_DATA_MSDU_IN_AMSDU_COUNT);
		if (prReorderQueParm->u4SeqNo != u4SeqNo) {
			RX_INC_CNT(&prAdapter->rRxCtrl,
				RX_DATA_AMSDU_MISS_COUNT);
			RX_INC_CNT(&prAdapter->rRxCtrl,
				RX_DATA_AMSDU_COUNT);
		}
		break;
	case RX_PAYLOAD_FORMAT_LAST_SUB_AMSDU:
		prReorderQueParm->fgAmsduNeedLastFrame = FALSE;
		RX_INC_CNT(&prAdapter->rRxCtrl,
			   RX_DATA_MSDU_IN_AMSDU_COUNT);
		if (prReorderQueParm->u4SeqNo != u4SeqNo) {
			RX_INC_CNT(&prAdapter->rRxCtrl,
				RX_DATA_AMSDU_MISS_COUNT);
			RX_INC_CNT(&prAdapter->rRxCtrl,
				RX_DATA_AMSDU_COUNT);
		}
		break;

	case RX_PAYLOAD_FORMAT_MSDU:
		if (prReorderQueParm->fgAmsduNeedLastFrame) {
			RX_INC_CNT(&prAdapter->rRxCtrl,
				RX_DATA_AMSDU_MISS_COUNT);
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
	qmInsertReorderPkt(prAdapter, prSwRfb, prReorderQueParm,
		prReturnedQue);
	RX_DIRECT_REORDER_UNLOCK(prAdapter, 0);
}

void qmProcessBarFrame(IN struct ADAPTER *prAdapter,
	IN struct SW_RFB *prSwRfb, OUT struct QUE *prReturnedQue)
{

	struct STA_RECORD *prStaRec;
	struct HW_MAC_RX_DESC *prRxStatus;
	struct RX_BA_ENTRY *prReorderQueParm;
	struct CTRL_BAR_FRAME *prBarCtrlFrame;

	uint32_t u4SSN;
	uint32_t u4WinStart;
	uint32_t u4WinEnd;

	ASSERT(prSwRfb);
	ASSERT(prReturnedQue);
	ASSERT(prSwRfb->prRxStatus);
	ASSERT(prSwRfb->pvHeader);

	prRxStatus = prSwRfb->prRxStatus;

	prBarCtrlFrame = (struct CTRL_BAR_FRAME *) prSwRfb->pvHeader;

	prSwRfb->ucTid =
		(*((uint16_t *) ((uint8_t *) prBarCtrlFrame +
		CTRL_BAR_BAR_CONTROL_OFFSET))) >>
		BAR_CONTROL_TID_INFO_OFFSET;
	prSwRfb->u2SSN =
		(*((uint16_t *) ((uint8_t *) prBarCtrlFrame +
		CTRL_BAR_BAR_INFORMATION_OFFSET))) >>
		OFFSET_BAR_SSC_SN;

	prSwRfb->eDst = RX_PKT_DESTINATION_NULL;
	QUEUE_INSERT_TAIL(prReturnedQue, (struct QUE_ENTRY *) prSwRfb);

	/* Incorrect STA_REC index */
	prSwRfb->ucStaRecIdx = secLookupStaRecIndexFromTA(prAdapter,
		prBarCtrlFrame->aucSrcAddr);
	if (prSwRfb->ucStaRecIdx >= CFG_STA_REC_NUM) {
		DBGLOG(QM, WARN,
			"QM: (Warning) BAR for a NULL STA_REC, ucStaRecIdx = %d\n",
			prSwRfb->ucStaRecIdx);
		/* ASSERT(0); */
		return;
	}

	/* Check whether the STA_REC is activated */
	prSwRfb->prStaRec = cnmGetStaRecByIndex(prAdapter,
		prSwRfb->ucStaRecIdx);
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

	/* Check index out of bound */
	if (prSwRfb->ucTid >= CFG_RX_MAX_BA_TID_NUM) {
		DBGLOG(QM, WARN,
			"QM: (Warning) index out of bound: ucTid = %d\n",
			prSwRfb->ucTid);
		/* ASSERT(0); */
		return;
	}

	/* Check whether the BA agreement exists */
	prReorderQueParm = prStaRec->aprRxReorderParamRefTbl[prSwRfb->ucTid];
	if (!prReorderQueParm) {
		/* TODO: (Tehuang) Handle the Host-FW sync issue. */
		DBGLOG(QM, WARN,
			"QM: (Warning) BAR for a NULL ReorderQueParm\n");
		/* ASSERT(0); */
		return;
	}

	RX_DIRECT_REORDER_LOCK(prAdapter, 0);

	u4SSN = (uint32_t) (prSwRfb->u2SSN);
	u4WinStart = (uint32_t) (prReorderQueParm->u2WinStart);
	u4WinEnd = (uint32_t) (prReorderQueParm->u2WinEnd);

	if (qmCompareSnIsLessThan(u4WinStart, u4SSN)) {
		prReorderQueParm->u2WinStart = (uint16_t) u4SSN;
		prReorderQueParm->u2WinEnd =
			((prReorderQueParm->u2WinStart) +
			(prReorderQueParm->u2WinSize) - 1) % MAX_SEQ_NO_COUNT;
#if CFG_SUPPORT_RX_AMSDU
		/* RX reorder for one MSDU in AMSDU issue */
		prReorderQueParm->u8LastAmsduSubIdx = RX_PAYLOAD_FORMAT_MSDU;
#endif
		DBGLOG(QM, TRACE,
			"QM:(BAR)[%d](%u){%hu,%hu}\n",
			prSwRfb->ucTid, u4SSN,
			prReorderQueParm->u2WinStart,
			prReorderQueParm->u2WinEnd);
		qmPopOutDueToFallAhead(prAdapter, prReorderQueParm,
			prReturnedQue);
	} else {
		DBGLOG(QM, TRACE, "QM:(BAR)(%d)(%u){%u,%u}\n",
			prSwRfb->ucTid, u4SSN, u4WinStart, u4WinEnd);
	}
	RX_DIRECT_REORDER_UNLOCK(prAdapter, 0);
}

void qmInsertReorderPkt(IN struct ADAPTER *prAdapter,
	IN struct SW_RFB *prSwRfb,
	IN struct RX_BA_ENTRY *prReorderQueParm,
	OUT struct QUE *prReturnedQue)
{
	uint32_t u4SeqNo;
	uint32_t u4WinStart;
	uint32_t u4WinEnd;

	/* Start to reorder packets */
	u4SeqNo = (uint32_t) (prSwRfb->u2SSN);
	u4WinStart = (uint32_t) (prReorderQueParm->u2WinStart);
	u4WinEnd = (uint32_t) (prReorderQueParm->u2WinEnd);

	/* Debug */
	DBGLOG_LIMITED(QM, LOUD, "QM:(R)[%u](%u){%u,%u}\n", prSwRfb->ucTid,
		u4SeqNo, u4WinStart, u4WinEnd);

	/* Case 1: Fall within */
	if			/* 0 - start - sn - end - 4095 */
	(((u4WinStart <= u4SeqNo) && (u4SeqNo <= u4WinEnd))
	    /* 0 - end - start - sn - 4095 */
	    || ((u4WinEnd < u4WinStart) && (u4WinStart <= u4SeqNo))
	    /* 0 - sn - end - start - 4095 */
	    || ((u4SeqNo <= u4WinEnd) && (u4WinEnd < u4WinStart))) {

		qmInsertFallWithinReorderPkt(prAdapter, prSwRfb,
					     prReorderQueParm, prReturnedQue);

#if QM_RX_WIN_SSN_AUTO_ADVANCING
		if (prReorderQueParm->fgIsWaitingForPktWithSsn) {
			/* Let the first received packet
			 * pass the reorder check
			 */
			DBGLOG(QM, LOUD, "QM:(A)[%d](%u){%u,%u}\n",
				prSwRfb->ucTid, u4SeqNo, u4WinStart, u4WinEnd);

			prReorderQueParm->u2WinStart = (uint16_t) u4SeqNo;
			prReorderQueParm->u2WinEnd =
				((prReorderQueParm->u2WinStart) +
				 (prReorderQueParm->u2WinSize) - 1) %
				 MAX_SEQ_NO_COUNT;
			prReorderQueParm->fgIsWaitingForPktWithSsn = FALSE;
#if CFG_SUPPORT_RX_AMSDU
			/* RX reorder for one MSDU in AMSDU issue */
			prReorderQueParm->u8LastAmsduSubIdx =
				RX_PAYLOAD_FORMAT_MSDU;
#endif
		}
#endif

		qmPopOutDueToFallWithin(prAdapter, prReorderQueParm,
			prReturnedQue);
	}
	/* Case 2: Fall ahead */
	else if
	/* 0 - start - end - sn - (start+2048) - 4095 */
	(((u4WinStart < u4WinEnd) && (u4WinEnd < u4SeqNo) &&
		(u4SeqNo < (u4WinStart + HALF_SEQ_NO_COUNT)))
		/* 0 - sn - (start+2048) - start - end - 4095 */
	  || ((u4SeqNo < u4WinStart) && (u4WinStart < u4WinEnd) &&
			((u4SeqNo + MAX_SEQ_NO_COUNT) <
			(u4WinStart + HALF_SEQ_NO_COUNT)))
			/* 0 - end - sn - (start+2048) - start - 4095 */
			|| ((u4WinEnd < u4SeqNo) && (u4SeqNo < u4WinStart) &&
			((u4SeqNo + MAX_SEQ_NO_COUNT) < (u4WinStart +
			HALF_SEQ_NO_COUNT)))) {

		uint16_t u2Delta, u2BeforeWinEnd;
		uint32_t u4BeforeCount, u4MissingCount;

#if QM_RX_WIN_SSN_AUTO_ADVANCING
		if (prReorderQueParm->fgIsWaitingForPktWithSsn)
			prReorderQueParm->fgIsWaitingForPktWithSsn = FALSE;
#endif

		qmInsertFallAheadReorderPkt(prAdapter, prSwRfb,
			prReorderQueParm, prReturnedQue);

		u2BeforeWinEnd = prReorderQueParm->u2WinEnd;

		/* Advance the window after inserting a new tail */
		prReorderQueParm->u2WinEnd = (uint16_t) u4SeqNo;
		prReorderQueParm->u2WinStart =
			(((prReorderQueParm->u2WinEnd) + MAX_SEQ_NO_COUNT -
			prReorderQueParm->u2WinSize + 1) %
			MAX_SEQ_NO_COUNT);
#if CFG_SUPPORT_RX_AMSDU
		/* RX reorder for one MSDU in AMSDU issue */
		prReorderQueParm->u8LastAmsduSubIdx =
			RX_PAYLOAD_FORMAT_MSDU;
#endif
		u4BeforeCount = prReorderQueParm->rReOrderQue.u4NumElem;
		qmPopOutDueToFallAhead(prAdapter, prReorderQueParm,
			prReturnedQue);

		if (prReorderQueParm->u2WinEnd >= u2BeforeWinEnd)
			u2Delta = prReorderQueParm->u2WinEnd - u2BeforeWinEnd;
		else
			u2Delta = MAX_SEQ_NO_COUNT - (u2BeforeWinEnd -
				prReorderQueParm->u2WinEnd);

		u4MissingCount = u2Delta - (u4BeforeCount -
			prReorderQueParm->rReOrderQue.u4NumElem);

		RX_ADD_CNT(&prAdapter->rRxCtrl, RX_DATA_REORDER_MISS_COUNT,
			u4MissingCount);
	}
	/* Case 3: Fall behind */
	else {
#if CFG_SUPPORT_LOWLATENCY_MODE || CFG_SUPPORT_OSHARE
		if (qmIsNoDropPacket(prAdapter, prSwRfb)) {
			DBGLOG(QM, LOUD, "QM: No drop packet:[%d](%d){%d,%d}\n",
				prSwRfb->ucTid, u4SeqNo, u4WinStart, u4WinEnd);

			qmPopOutReorderPkt(prAdapter, prSwRfb,
				prReturnedQue, RX_DATA_REORDER_BEHIND_COUNT);
			return;
		}
#endif /* CFG_SUPPORT_LOWLATENCY_MODE */

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

void qmInsertFallWithinReorderPkt(IN struct ADAPTER *prAdapter,
	IN struct SW_RFB *prSwRfb,
	IN struct RX_BA_ENTRY *prReorderQueParm,
	OUT struct QUE *prReturnedQue)
{
	struct SW_RFB *prExaminedQueuedSwRfb;
	struct QUE *prReorderQue;
	struct HW_MAC_RX_DESC *prRxStatus;
	uint8_t u8AmsduSubframeIdx; /* RX reorder for one MSDU in AMSDU issue */

	ASSERT(prSwRfb);
	ASSERT(prReorderQueParm);
	ASSERT(prReturnedQue);

	prReorderQue = &(prReorderQueParm->rReOrderQue);
	prExaminedQueuedSwRfb = (struct SW_RFB *) QUEUE_GET_HEAD(
		prReorderQue);

	prRxStatus = prSwRfb->prRxStatus;
	u8AmsduSubframeIdx = HAL_RX_STATUS_GET_PAYLOAD_FORMAT(
		prRxStatus);

	/* There are no packets queued in the Reorder Queue */
	if (prExaminedQueuedSwRfb == NULL) {
		((struct QUE_ENTRY *) prSwRfb)->prPrev = NULL;
		((struct QUE_ENTRY *) prSwRfb)->prNext = NULL;
		prReorderQue->prHead = (struct QUE_ENTRY *) prSwRfb;
		prReorderQue->prTail = (struct QUE_ENTRY *) prSwRfb;
		prReorderQue->u4NumElem++;
	}

	/* Determine the insert position */
	else {
		do {
			/* Case 1: Terminate. A duplicate packet */
			if ((prExaminedQueuedSwRfb->u2SSN) ==
				(prSwRfb->u2SSN)) {
#if CFG_SUPPORT_RX_AMSDU
				/* RX reorder for one MSDU in AMSDU issue */
				/* if middle or last and first is not
				 * duplicated, not a duplicat packet
				 */
				if (!prReorderQueParm->fgIsAmsduDuplicated &&
					(u8AmsduSubframeIdx ==
					RX_PAYLOAD_FORMAT_MIDDLE_SUB_AMSDU ||
					u8AmsduSubframeIdx ==
					RX_PAYLOAD_FORMAT_LAST_SUB_AMSDU)) {

					prExaminedQueuedSwRfb =
						(struct SW_RFB *)((
						(struct QUE_ENTRY *)
						prExaminedQueuedSwRfb)->prNext);
					while (prExaminedQueuedSwRfb &&
						((prExaminedQueuedSwRfb->
						u2SSN) == (prSwRfb->u2SSN)))
						prExaminedQueuedSwRfb =
							(struct SW_RFB *)((
							(struct QUE_ENTRY *)
							prExaminedQueuedSwRfb)->
							prNext);

					break;
				}
				/* if first is duplicated,
				 * drop subsequent middle and last frames
				 */
				if (u8AmsduSubframeIdx ==
					RX_PAYLOAD_FORMAT_FIRST_SUB_AMSDU)
					prReorderQueParm->fgIsAmsduDuplicated =
						TRUE;
#endif
				prSwRfb->eDst = RX_PKT_DESTINATION_NULL;
				qmPopOutReorderPkt(prAdapter,
					prSwRfb, prReturnedQue,
					RX_DUPICATE_DROP_COUNT);
				return;
			}

			/* Case 2: Terminate. The insert point is found */
			else if (qmCompareSnIsLessThan((prSwRfb->u2SSN),
				(prExaminedQueuedSwRfb->u2SSN)))
				break;

			/* Case 3: Insert point not found.
			 * Check the next SW_RFB in the Reorder Queue
			 */
			else
				prExaminedQueuedSwRfb =
					(struct SW_RFB *) (((struct QUE_ENTRY *)
						prExaminedQueuedSwRfb)->prNext);
		} while (prExaminedQueuedSwRfb);
#if CFG_SUPPORT_RX_AMSDU
		prReorderQueParm->fgIsAmsduDuplicated = FALSE;
#endif
		/* Update the Reorder Queue Parameters according to
		 * the found insert position
		 */
		if (prExaminedQueuedSwRfb == NULL) {
			/* The received packet shall be placed at the tail */
			((struct QUE_ENTRY *) prSwRfb)->prPrev =
				prReorderQue->prTail;
			((struct QUE_ENTRY *) prSwRfb)->prNext = NULL;
			(prReorderQue->prTail)->prNext =
				(struct QUE_ENTRY *) (prSwRfb);
			prReorderQue->prTail = (struct QUE_ENTRY *) (prSwRfb);
		} else {
			((struct QUE_ENTRY *) prSwRfb)->prPrev =
				((struct QUE_ENTRY *)
				prExaminedQueuedSwRfb)->prPrev;
			((struct QUE_ENTRY *) prSwRfb)->prNext =
				(struct QUE_ENTRY *) prExaminedQueuedSwRfb;
			if (((struct QUE_ENTRY *) prExaminedQueuedSwRfb) ==
			    (prReorderQue->prHead)) {
				/* The received packet will become the head */
				prReorderQue->prHead =
					(struct QUE_ENTRY *) prSwRfb;
			} else {
				(((struct QUE_ENTRY *)
				prExaminedQueuedSwRfb)->prPrev)->prNext =
					(struct QUE_ENTRY *) prSwRfb;
			}
			((struct QUE_ENTRY *) prExaminedQueuedSwRfb)->prPrev =
				(struct QUE_ENTRY *) prSwRfb;
		}

		prReorderQue->u4NumElem++;
	}

}

void qmInsertFallAheadReorderPkt(IN struct ADAPTER *prAdapter,
	IN struct SW_RFB *prSwRfb,
	IN struct RX_BA_ENTRY *prReorderQueParm,
	OUT struct QUE *prReturnedQue)
{
	struct QUE *prReorderQue;

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
		((struct QUE_ENTRY *) prSwRfb)->prPrev = NULL;
		((struct QUE_ENTRY *) prSwRfb)->prNext = NULL;
		prReorderQue->prHead = (struct QUE_ENTRY *) prSwRfb;
	} else {
		((struct QUE_ENTRY *) prSwRfb)->prPrev =
			prReorderQue->prTail;
		((struct QUE_ENTRY *) prSwRfb)->prNext = NULL;
		(prReorderQue->prTail)->prNext = (struct QUE_ENTRY *) (
			prSwRfb);
	}
	prReorderQue->prTail = (struct QUE_ENTRY *) prSwRfb;
	prReorderQue->u4NumElem++;
}

void qmPopOutReorderPkt(IN struct ADAPTER *prAdapter,
	IN struct SW_RFB *prSwRfb, OUT struct QUE *prReturnedQue,
	IN enum ENUM_RX_STATISTIC_COUNTER eRxCounter)
{
	uint32_t u4PktCnt = 0;
	/* RX reorder for one MSDU in AMSDU issue */
#if 0
	struct SW_RFB *prAmsduSwRfb;
#endif

#if CFG_SUPPORT_RX_DYNAMIC_MCC_PRIORITY
	struct STA_RECORD *prStaRec = NULL;

	prStaRec = &(prAdapter->arStaRec[prSwRfb->ucStaRecIdx]);

	if (prStaRec->fgIsInUse) {
		if (IS_STA_IN_P2P(prStaRec)) {
			RX_INC_CNT(&prAdapter->rRxCtrl,
				RX_DATA_REORDER_WITHIN_COUNT_P2P);
		} else if (IS_STA_IN_AIS(prStaRec)) {
			RX_INC_CNT(&prAdapter->rRxCtrl,
				RX_DATA_REORDER_WITHIN_COUNT_STA);
		}
	}
#endif

	u4PktCnt++;
	QUEUE_INSERT_TAIL(prReturnedQue,
		(struct QUE_ENTRY *)prSwRfb);

#if 0
	u4PktCnt += prSwRfb->rAmsduQue.u4NumElem;
	QUEUE_REMOVE_HEAD(&prSwRfb->rAmsduQue, prAmsduSwRfb,
		struct SW_RFB *);
	while (prAmsduSwRfb) {
		/* Update MSDU destination of AMSDU */
		prAmsduSwRfb->eDst = prSwRfb->eDst;
		QUEUE_INSERT_TAIL(prReturnedQue,
			(struct QUE_ENTRY *)prAmsduSwRfb);
		QUEUE_REMOVE_HEAD(&prSwRfb->rAmsduQue, prAmsduSwRfb,
			struct SW_RFB *);
	}
#endif

	RX_ADD_CNT(&prAdapter->rRxCtrl, eRxCounter, u4PktCnt);
}

void qmPopOutDueToFallWithin(IN struct ADAPTER *prAdapter,
	IN struct RX_BA_ENTRY *prReorderQueParm,
	OUT struct QUE *prReturnedQue)
{
	struct SW_RFB *prReorderedSwRfb;
	struct QUE *prReorderQue;
	u_int8_t fgDequeuHead, fgMissing;
	OS_SYSTIME rCurrentTime, *prMissTimeout;
	struct HW_MAC_RX_DESC *prRxStatus;
	/* RX reorder for one MSDU in AMSDU issue */
	uint8_t fgIsAmsduSubframe;

	prReorderQue = &(prReorderQueParm->rReOrderQue);

	fgMissing = FALSE;
	rCurrentTime = 0;
	prMissTimeout =
		&g_arMissTimeout[prReorderQueParm->ucStaRecIdx][
		prReorderQueParm->ucTid];
	if (*prMissTimeout) {
		fgMissing = TRUE;
		GET_CURRENT_SYSTIME(&rCurrentTime);
	}

	/* Check whether any packet can be indicated to the higher layer */
	while (TRUE) {
		if (QUEUE_IS_EMPTY(prReorderQue))
			break;

		/* Always examine the head packet */
		prReorderedSwRfb = (struct SW_RFB *) QUEUE_GET_HEAD(
			prReorderQue);
		fgDequeuHead = FALSE;

		/* RX reorder for one MSDU in AMSDU issue */
		prRxStatus = prReorderedSwRfb->prRxStatus;
		fgIsAmsduSubframe = HAL_RX_STATUS_GET_PAYLOAD_FORMAT(
			prRxStatus);
#if CFG_SUPPORT_RX_AMSDU
		/* If SN + 1 come and last frame is first or middle,
		 * update winstart
		 */
		if ((qmCompareSnIsLessThan((prReorderQueParm->u2WinStart),
			(prReorderedSwRfb->u2SSN)))
			&& (prReorderQueParm->u4SeqNo !=
			prReorderQueParm->u2WinStart)) {
			if (prReorderQueParm->u8LastAmsduSubIdx ==
				RX_PAYLOAD_FORMAT_FIRST_SUB_AMSDU
				|| prReorderQueParm->u8LastAmsduSubIdx ==
				RX_PAYLOAD_FORMAT_MIDDLE_SUB_AMSDU) {

				prReorderQueParm->u2WinStart =
					(((prReorderQueParm->u2WinStart) + 1) %
					MAX_SEQ_NO_COUNT);
				prReorderQueParm->u8LastAmsduSubIdx =
					RX_PAYLOAD_FORMAT_MSDU;
			}
		}
#endif
		/* SN == WinStart, so the head packet
		 * shall be indicated (advance the window)
		 */
		if ((prReorderedSwRfb->u2SSN) ==
			(prReorderQueParm->u2WinStart)) {

			fgDequeuHead = TRUE;
			/* RX reorder for one MSDU in AMSDU issue */
			/* if last frame, winstart++.
			 * Otherwise, keep winstart
			 */
			if (fgIsAmsduSubframe ==
				RX_PAYLOAD_FORMAT_LAST_SUB_AMSDU
				|| fgIsAmsduSubframe == RX_PAYLOAD_FORMAT_MSDU)
				prReorderQueParm->u2WinStart =
				(((prReorderedSwRfb->u2SSN) + 1) %
				MAX_SEQ_NO_COUNT);
#if CFG_SUPPORT_RX_AMSDU
			prReorderQueParm->u8LastAmsduSubIdx = fgIsAmsduSubframe;
#endif
		}
		/* SN > WinStart, break to update WinEnd */
		else {
			/* Start bubble timer */
			if (!prReorderQueParm->fgHasBubble) {
				cnmTimerStartTimer(prAdapter,
				&(prReorderQueParm->rReorderBubbleTimer),
					prAdapter->u4QmRxBaMissTimeout);
				prReorderQueParm->fgHasBubble = TRUE;
				prReorderQueParm->u2FirstBubbleSn =
					prReorderQueParm->u2WinStart;

				DBGLOG(QM, TRACE,
					"QM:(Bub Timer) STA[%u] TID[%u] BubSN[%u] Win{%d, %d}\n",
					prReorderQueParm->ucStaRecIdx,
					prReorderedSwRfb->ucTid,
					prReorderQueParm->u2FirstBubbleSn,
					prReorderQueParm->u2WinStart,
					prReorderQueParm->u2WinEnd);
			}

			if (fgMissing &&
				CHECK_FOR_TIMEOUT(rCurrentTime, *prMissTimeout,
				MSEC_TO_SYSTIME(
				prAdapter->u4QmRxBaMissTimeout
				))) {

				DBGLOG(QM, TRACE,
					"QM:RX BA Timout Next Tid %d SSN %d\n",
					prReorderQueParm->ucTid,
					prReorderedSwRfb->u2SSN);
				fgDequeuHead = TRUE;
				prReorderQueParm->u2WinStart =
					(((prReorderedSwRfb->u2SSN) + 1) %
					MAX_SEQ_NO_COUNT);
#if CFG_SUPPORT_RX_AMSDU
				/* RX reorder for one MSDU in AMSDU issue */
				prReorderQueParm->u8LastAmsduSubIdx =
					RX_PAYLOAD_FORMAT_MSDU;
#endif
				fgMissing = FALSE;
			} else
				break;
		}

		/* Dequeue the head packet */
		if (fgDequeuHead) {
			if (((struct QUE_ENTRY *) prReorderedSwRfb)->prNext ==
				NULL) {
				prReorderQue->prHead = NULL;
				prReorderQue->prTail = NULL;
			} else {
				prReorderQue->prHead =
					((struct QUE_ENTRY *)
					prReorderedSwRfb)->prNext;
				(((struct QUE_ENTRY *)
					prReorderedSwRfb)->prNext)->prPrev =
					NULL;
			}
			prReorderQue->u4NumElem--;
			DBGLOG(QM, LOUD, "QM: [%d] %d (%d)\n",
				prReorderQueParm->ucTid,
				prReorderedSwRfb->u2PacketLen,
				prReorderedSwRfb->u2SSN);
			qmPopOutReorderPkt(prAdapter, prReorderedSwRfb,
				prReturnedQue, RX_DATA_REORDER_WITHIN_COUNT);
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
		(((prReorderQueParm->u2WinStart) +
		(prReorderQueParm->u2WinSize) - 1) % MAX_SEQ_NO_COUNT);

}

void qmPopOutDueToFallAhead(IN struct ADAPTER *prAdapter,
	IN struct RX_BA_ENTRY *prReorderQueParm,
	OUT struct QUE *prReturnedQue)
{
	struct SW_RFB *prReorderedSwRfb;
	struct QUE *prReorderQue;
	u_int8_t fgDequeuHead;
	struct HW_MAC_RX_DESC *prRxStatus;
	uint8_t fgIsAmsduSubframe;/* RX reorder for one MSDU in AMSDU issue */

	prReorderQue = &(prReorderQueParm->rReOrderQue);

	/* Check whether any packet can be indicated to the higher layer */
	while (TRUE) {
		if (QUEUE_IS_EMPTY(prReorderQue))
			break;

		/* Always examine the head packet */
		prReorderedSwRfb =
			(struct SW_RFB *) QUEUE_GET_HEAD(prReorderQue);
		fgDequeuHead = FALSE;

		/* RX reorder for one MSDU in AMSDU issue */
		prRxStatus = prReorderedSwRfb->prRxStatus;
		fgIsAmsduSubframe =
			HAL_RX_STATUS_GET_PAYLOAD_FORMAT(prRxStatus);
#if CFG_SUPPORT_RX_AMSDU
		/* If SN + 1 come and last frame is first or middle,
		 * update winstart
		 */
		if ((qmCompareSnIsLessThan((prReorderQueParm->u2WinStart),
			(prReorderedSwRfb->u2SSN)))
			&& (prReorderQueParm->u4SeqNo !=
			prReorderQueParm->u2WinStart)) {
			if (prReorderQueParm->u8LastAmsduSubIdx ==
				RX_PAYLOAD_FORMAT_FIRST_SUB_AMSDU
				|| prReorderQueParm->u8LastAmsduSubIdx ==
				RX_PAYLOAD_FORMAT_MIDDLE_SUB_AMSDU) {

				prReorderQueParm->u2WinStart =
					(((prReorderQueParm->u2WinStart) + 1) %
					MAX_SEQ_NO_COUNT);
				prReorderQueParm->u8LastAmsduSubIdx =
					RX_PAYLOAD_FORMAT_MSDU;
			}
		}
#endif
		/* SN == WinStart, so the head packet shall be
		 * indicated (advance the window)
		 */
		if ((prReorderedSwRfb->u2SSN) ==
			(prReorderQueParm->u2WinStart)) {

			fgDequeuHead = TRUE;
			/* RX reorder for one MSDU in AMSDU issue */
			/* if last frame, winstart++.
			 * Otherwise, keep winstart
			 */
			if (fgIsAmsduSubframe ==
				RX_PAYLOAD_FORMAT_LAST_SUB_AMSDU ||
				fgIsAmsduSubframe == RX_PAYLOAD_FORMAT_MSDU)
				prReorderQueParm->u2WinStart =
				(((prReorderedSwRfb->u2SSN) + 1) %
				MAX_SEQ_NO_COUNT);
#if CFG_SUPPORT_RX_AMSDU
			prReorderQueParm->u8LastAmsduSubIdx = fgIsAmsduSubframe;
#endif
		}

		/* SN < WinStart, so the head packet shall be
		 * indicated (do not advance the window)
		 */
		else if (qmCompareSnIsLessThan((uint32_t)(
			prReorderedSwRfb->u2SSN),
			(uint32_t)(prReorderQueParm->u2WinStart)))
			fgDequeuHead = TRUE;

		/* SN > WinStart, break to update WinEnd */
		else {
			/* Start bubble timer */
			if (!prReorderQueParm->fgHasBubble) {
				cnmTimerStartTimer(prAdapter,
					&(prReorderQueParm->
					rReorderBubbleTimer),
					prAdapter->u4QmRxBaMissTimeout);
				prReorderQueParm->fgHasBubble = TRUE;
				prReorderQueParm->u2FirstBubbleSn =
					prReorderQueParm->u2WinStart;

				DBGLOG(QM, TRACE,
					"QM:(Bub Timer) STA[%u] TID[%u] BubSN[%u] Win{%d, %d}\n",
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
			if (((struct QUE_ENTRY *) prReorderedSwRfb)->prNext ==
				NULL) {
				prReorderQue->prHead = NULL;
				prReorderQue->prTail = NULL;
			} else {
				prReorderQue->prHead = ((struct QUE_ENTRY *)
				prReorderedSwRfb)->prNext;
				(((struct QUE_ENTRY *) prReorderedSwRfb)->
				prNext)->prPrev = NULL;
			}
			prReorderQue->u4NumElem--;
			DBGLOG_LIMITED(QM, TRACE, "QM: [%u] %u (%u)\n",
				       prReorderQueParm->ucTid,
				       prReorderedSwRfb->u2PacketLen,
				       prReorderedSwRfb->u2SSN);

			qmPopOutReorderPkt(prAdapter, prReorderedSwRfb,
				prReturnedQue, RX_DATA_REORDER_AHEAD_COUNT);
		}
	}

	/* After WinStart has been determined, update the WinEnd */
	prReorderQueParm->u2WinEnd =
		(((prReorderQueParm->u2WinStart) +
		(prReorderQueParm->u2WinSize) - 1) % MAX_SEQ_NO_COUNT);

}

void qmHandleReorderBubbleTimeout(IN struct ADAPTER *prAdapter,
	IN unsigned long ulParamPtr)
{
	struct mt66xx_chip_info *prChipInfo;
	struct RX_BA_ENTRY *prReorderQueParm =
		(struct RX_BA_ENTRY *) ulParamPtr;
	struct SW_RFB *prSwRfb = (struct SW_RFB *) NULL;
	struct WIFI_EVENT *prEvent;
	struct EVENT_CHECK_REORDER_BUBBLE *prCheckReorderEvent;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);
	prChipInfo = prAdapter->chip_info;

	if (!prReorderQueParm->fgIsValid) {
		DBGLOG(QM, TRACE,
			"QM:(Bub Check Cancel) STA[%u] TID[%u], No Rx BA entry\n",
			prReorderQueParm->ucStaRecIdx, prReorderQueParm->ucTid);
		return;
	}

	if (!prReorderQueParm->fgHasBubble) {
		DBGLOG(QM, TRACE,
			"QM:(Bub Check Cancel) STA[%u] TID[%u], Bubble has been filled\n",
			prReorderQueParm->ucStaRecIdx, prReorderQueParm->ucTid);
		return;
	}

	DBGLOG(QM, TRACE,
		"QM:(Bub Timeout) STA[%u] TID[%u] BubSN[%u]\n",
		prReorderQueParm->ucStaRecIdx, prReorderQueParm->ucTid,
		prReorderQueParm->u2FirstBubbleSn);

	/* Generate a self-inited event to Rx path */
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);
	QUEUE_REMOVE_HEAD(&prAdapter->rRxCtrl.rFreeSwRfbList,
		prSwRfb, struct SW_RFB *);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);

	if (prSwRfb) {
		prEvent = (struct WIFI_EVENT *)
			(prSwRfb->pucRecvBuff + prChipInfo->rxd_size);
		prEvent->ucEID = EVENT_ID_CHECK_REORDER_BUBBLE;
		prEvent->ucSeqNum = 0;
		prEvent->u2PacketLength =
			prChipInfo->rxd_size + prChipInfo->event_hdr_size +
			sizeof(struct EVENT_CHECK_REORDER_BUBBLE);

		prCheckReorderEvent = (struct EVENT_CHECK_REORDER_BUBBLE *)
			prEvent->aucBuffer;
		prCheckReorderEvent->ucStaRecIdx =
			prReorderQueParm->ucStaRecIdx;
		prCheckReorderEvent->ucTid = prReorderQueParm->ucTid;

		prSwRfb->ucPacketType = RX_PKT_TYPE_SW_DEFINED;
		prSwRfb->prRxStatus->u2PktTYpe = RXM_RXD_PKT_TYPE_SW_EVENT;

		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
		QUEUE_INSERT_TAIL(&prAdapter->rRxCtrl.rReceivedRfbList,
			&prSwRfb->rQueEntry);
		RX_INC_CNT(&prAdapter->rRxCtrl, RX_MPDU_TOTAL_COUNT);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);

		DBGLOG(QM, LOUD,
			"QM:(Bub Check Event Sent) STA[%u] TID[%u]\n",
			prReorderQueParm->ucStaRecIdx,
			prReorderQueParm->ucTid);

		nicRxProcessRFBs(prAdapter);

		DBGLOG(QM, LOUD,
			"QM:(Bub Check Event Handled) STA[%u] TID[%u]\n",
			prReorderQueParm->ucStaRecIdx,
			prReorderQueParm->ucTid);
	} else {
		DBGLOG(QM, TRACE,
			"QM:(Bub Check Cancel) STA[%u] TID[%u], Bub check event alloc failed\n",
			prReorderQueParm->ucStaRecIdx,
			prReorderQueParm->ucTid);

		cnmTimerStartTimer(prAdapter,
			&(prReorderQueParm->rReorderBubbleTimer),
			prAdapter->u4QmRxBaMissTimeout);

		DBGLOG(QM, TRACE,
			"QM:(Bub Timer Restart) STA[%u] TID[%u] BubSN[%u] Win{%d, %d}\n",
			prReorderQueParm->ucStaRecIdx,
			prReorderQueParm->ucTid,
			prReorderQueParm->u2FirstBubbleSn,
			prReorderQueParm->u2WinStart,
			prReorderQueParm->u2WinEnd);
	}

}

void qmHandleEventCheckReorderBubble(IN struct ADAPTER *prAdapter,
	IN struct WIFI_EVENT *prEvent)
{
	struct EVENT_CHECK_REORDER_BUBBLE *prCheckReorderEvent;
	struct RX_BA_ENTRY *prReorderQueParm;
	struct QUE *prReorderQue;
	struct QUE rReturnedQue;
	struct QUE *prReturnedQue = &rReturnedQue;
	struct SW_RFB *prReorderedSwRfb, *prSwRfb;
	OS_SYSTIME *prMissTimeout;

	prCheckReorderEvent = (struct EVENT_CHECK_REORDER_BUBBLE *)
		(prEvent->aucBuffer);

	QUEUE_INITIALIZE(prReturnedQue);

	/* Get target Rx BA entry */
	prReorderQueParm = qmLookupRxBaEntry(prAdapter,
		prCheckReorderEvent->ucStaRecIdx,
		prCheckReorderEvent->ucTid);

	/* Sanity Check */
	if (!prReorderQueParm) {
		DBGLOG(QM, TRACE,
			"QM:(Bub Check Cancel) STA[%u] TID[%u], No Rx BA entry\n",
			prCheckReorderEvent->ucStaRecIdx,
			prCheckReorderEvent->ucTid);
		return;
	}

	if (!prReorderQueParm->fgIsValid) {
		DBGLOG(QM, TRACE,
			"QM:(Bub Check Cancel) STA[%u] TID[%u], No Rx BA entry\n",
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

	DBGLOG(QM, TRACE,
		"QM:(Bub Check Event Got) STA[%u] TID[%u]\n",
		prReorderQueParm->ucStaRecIdx, prReorderQueParm->ucTid);

	/* Expected bubble timeout => pop out packets before win_end */
	if (prReorderQueParm->u2FirstBubbleSn ==
		prReorderQueParm->u2WinStart) {

		prReorderedSwRfb = (struct SW_RFB *) QUEUE_GET_TAIL(
			prReorderQue);

		prReorderQueParm->u2WinStart = prReorderedSwRfb->u2SSN + 1;
		if (prReorderQueParm->u2WinStart >= MAX_SEQ_NO_COUNT)
			prReorderQueParm->u2WinStart %= MAX_SEQ_NO_COUNT;
		prReorderQueParm->u2WinEnd =
			((prReorderQueParm->u2WinStart) +
			(prReorderQueParm->u2WinSize) - 1) % MAX_SEQ_NO_COUNT;
#if CFG_SUPPORT_RX_AMSDU
		prReorderQueParm->u8LastAmsduSubIdx =
			RX_PAYLOAD_FORMAT_MSDU;
#endif
		qmPopOutDueToFallAhead(prAdapter, prReorderQueParm,
			prReturnedQue);

		DBGLOG(QM, TRACE,
			"QM:(Bub Flush) STA[%u] TID[%u] BubSN[%u] Win{%u, %u}\n",
			prReorderQueParm->ucStaRecIdx,
			prReorderQueParm->ucTid,
			prReorderQueParm->u2FirstBubbleSn,
			prReorderQueParm->u2WinStart,
			prReorderQueParm->u2WinEnd);

		prReorderQueParm->fgHasBubble = FALSE;
		RX_DIRECT_REORDER_UNLOCK(prAdapter, 0);

		/* process prReturnedQue after unlock prReturnedQue */
		if (QUEUE_IS_NOT_EMPTY(prReturnedQue)) {
			QM_TX_SET_NEXT_MSDU_INFO(
				(struct SW_RFB *) QUEUE_GET_TAIL(
				prReturnedQue), NULL);

			prSwRfb = (struct SW_RFB *)
				QUEUE_GET_HEAD(prReturnedQue);
			while (prSwRfb) {
				DBGLOG(QM, TRACE,
					"QM:(Bub Flush) STA[%u] TID[%u] Pop Out SN[%u]\n",
				  prReorderQueParm->ucStaRecIdx,
				  prReorderQueParm->ucTid,
				  prSwRfb->u2SSN);

				prSwRfb = (struct SW_RFB *)
					QUEUE_GET_NEXT_ENTRY(
					(struct QUE_ENTRY *) prSwRfb);
			}

			wlanProcessQueuedSwRfb(prAdapter,
				(struct SW_RFB *)
				QUEUE_GET_HEAD(prReturnedQue));
		} else {
			DBGLOG(QM, TRACE,
				"QM:(Bub Flush) STA[%u] TID[%u] Pop Out 0 packet\n",
				prReorderQueParm->ucStaRecIdx,
				prReorderQueParm->ucTid);
		}
	}
	/* First bubble has been filled but others exist */
	else {
		prReorderQueParm->u2FirstBubbleSn =
			prReorderQueParm->u2WinStart;

		DBGLOG(QM, TRACE,
			"QM:(Bub Timer) STA[%u] TID[%u] BubSN[%u] Win{%u, %u}\n",
			prReorderQueParm->ucStaRecIdx,
			prReorderQueParm->ucTid,
			prReorderQueParm->u2FirstBubbleSn,
			prReorderQueParm->u2WinStart,
			prReorderQueParm->u2WinEnd);
		RX_DIRECT_REORDER_UNLOCK(prAdapter, 0);

		cnmTimerStartTimer(prAdapter,
			&(prReorderQueParm->rReorderBubbleTimer),
			prAdapter->u4QmRxBaMissTimeout);
	}

	prMissTimeout = &g_arMissTimeout[
		prReorderQueParm->ucStaRecIdx][prReorderQueParm->ucTid];
	if (QUEUE_IS_EMPTY(prReorderQue)) {
		DBGLOG(QM, TRACE,
			"QM:(Bub Check) Reset prMissTimeout to zero\n");
		*prMissTimeout = 0;
	} else {
		DBGLOG(QM, TRACE,
			"QM:(Bub Check) Reset prMissTimeout to current time\n");
		GET_CURRENT_SYSTIME(prMissTimeout);
	}
}

u_int8_t qmCompareSnIsLessThan(IN uint32_t u4SnLess, IN uint32_t u4SnGreater)
{
	/* 0 <--->  SnLess   <--(gap>2048)--> SnGreater : SnLess > SnGreater */
	if ((u4SnLess + HALF_SEQ_NO_COUNT) <= u4SnGreater)
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
void qmHandleMailboxRxMessage(IN struct MAILBOX_MSG prMailboxRxMsg)
{
	/* DbgPrint("QM: Enter qmHandleMailboxRxMessage()\n"); */
	/* TODO */
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Handle ADD TX BA Event from the FW
 *
 * \param[in] prAdapter Adapter pointer
 * \param[in] prEvent The event packet from the FW
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
void qmHandleEventTxAddBa(IN struct ADAPTER *prAdapter,
	IN struct WIFI_EVENT *prEvent)
{
	struct mt66xx_chip_info *prChipInfo;
	struct EVENT_TX_ADDBA *prEventTxAddBa;
	struct STA_RECORD *prStaRec;
	uint8_t ucStaRecIdx;
	uint8_t ucTid;

	ASSERT(prAdapter);
	prChipInfo = prAdapter->chip_info;

	DBGLOG(QM, INFO, "QM:Event +TxBa\n");

	if (!prChipInfo->is_support_hw_amsdu &&
	    prChipInfo->ucMaxSwAmsduNum <= 1) {
		DBGLOG(QM, INFO, "QM:Event +TxBa but chip is not support\n");
		return;
	}

	prEventTxAddBa = (struct EVENT_TX_ADDBA *) (prEvent->aucBuffer);
	ucStaRecIdx = prEventTxAddBa->ucStaRecIdx;
	prStaRec = QM_GET_STA_REC_PTR_FROM_INDEX(prAdapter, ucStaRecIdx);
	if (!prStaRec) {
		/* Invalid STA_REC index, discard the event packet */
		/* ASSERT(0); */
		DBGLOG(QM, INFO,
		       "QM: (Warning) TX ADDBA Event for a NULL STA_REC\n");
		return;
	}

	for (ucTid = 0; ucTid < TX_DESC_TID_NUM; ucTid++) {
		uint8_t ucStaEn = prStaRec->ucAmsduEnBitmap & BIT(ucTid);
		uint8_t ucEvtEn = prEventTxAddBa->ucAmsduEnBitmap & BIT(ucTid);

		if (prChipInfo->is_support_hw_amsdu && ucStaEn != ucEvtEn)
			nicTxSetHwAmsduDescTemplate(prAdapter, prStaRec, ucTid,
						    ucEvtEn >> ucTid);
	}

	prStaRec->ucAmsduEnBitmap = prEventTxAddBa->ucAmsduEnBitmap;
	prStaRec->ucMaxMpduCount = prEventTxAddBa->ucMaxMpduCount;
	prStaRec->u4MaxMpduLen = prEventTxAddBa->u4MaxMpduLen;
	prStaRec->u4MinMpduLen = prEventTxAddBa->u4MinMpduLen;

	DBGLOG(QM, INFO,
	       "QM:Event +TxBa bitmap[0x%x] count[%u] MaxLen[%u] MinLen[%u]\n",
	       prStaRec->ucAmsduEnBitmap, prStaRec->ucMaxMpduCount,
	       prStaRec->u4MaxMpduLen, prStaRec->u4MinMpduLen);
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
void qmHandleEventRxAddBa(IN struct ADAPTER *prAdapter,
	IN struct WIFI_EVENT *prEvent)
{
	struct EVENT_RX_ADDBA *prEventRxAddBa;
	struct STA_RECORD *prStaRec;
	uint32_t u4Tid;
	uint32_t u4WinSize;

	DBGLOG(QM, INFO, "QM:Event +RxBa\n");

	prEventRxAddBa = (struct EVENT_RX_ADDBA *) (
				 prEvent->aucBuffer);
	prStaRec = QM_GET_STA_REC_PTR_FROM_INDEX(prAdapter,
			prEventRxAddBa->ucStaRecIdx);

	if (!prStaRec) {
		/* Invalid STA_REC index, discard the event packet */
		/* ASSERT(0); */
		DBGLOG(QM, INFO,
			"QM: (Warning) RX ADDBA Event for a NULL STA_REC\n");
		return;
	}
#if 0
	if (!(prStaRec->fgIsValid)) {
		/* TODO: (Tehuang) Handle the Host-FW synchronization issue */
		DBGLOG(QM, WARN,
			"QM: (Warning) RX ADDBA Event for an invalid STA_REC\n");
		/* ASSERT(0); */
		/* return; */
	}
#endif

	u4Tid = (((prEventRxAddBa->u2BAParameterSet) &
		BA_PARAM_SET_TID_MASK) >>
		BA_PARAM_SET_TID_MASK_OFFSET);

	u4WinSize = (((prEventRxAddBa->u2BAParameterSet) &
		BA_PARAM_SET_BUFFER_SIZE_MASK) >>
		BA_PARAM_SET_BUFFER_SIZE_MASK_OFFSET);

	if (!qmAddRxBaEntry(prAdapter,
		prStaRec->ucIndex,
		(uint8_t) u4Tid,
		(prEventRxAddBa->u2BAStartSeqCtrl >>
		OFFSET_BAR_SSC_SN),
		(uint16_t) u4WinSize)) {

		/* FW shall ensure the availabiilty of
		 * the free-to-use BA entry
		 */
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
void qmHandleEventRxDelBa(IN struct ADAPTER *prAdapter,
	IN struct WIFI_EVENT *prEvent)
{
	struct EVENT_RX_DELBA *prEventRxDelBa;
	struct STA_RECORD *prStaRec;

	/* DbgPrint("QM:Event -RxBa\n"); */

	prEventRxDelBa = (struct EVENT_RX_DELBA *) (
		prEvent->aucBuffer);
	prStaRec = QM_GET_STA_REC_PTR_FROM_INDEX(prAdapter,
		prEventRxDelBa->ucStaRecIdx);

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

	qmDelRxBaEntry(prAdapter, prStaRec->ucIndex,
		prEventRxDelBa->ucTid, TRUE);

}

struct RX_BA_ENTRY *qmLookupRxBaEntry(IN struct ADAPTER *prAdapter,
	uint8_t ucStaRecIdx, uint8_t ucTid)
{
	int i;
	struct QUE_MGT *prQM = &prAdapter->rQM;

	/* DbgPrint("QM: Enter qmLookupRxBaEntry()\n"); */

	for (i = 0; i < CFG_NUM_OF_RX_BA_AGREEMENTS; i++) {
		if (prQM->arRxBaTable[i].fgIsValid) {
			if ((prQM->arRxBaTable[i].ucStaRecIdx == ucStaRecIdx)
				&& (prQM->arRxBaTable[i].ucTid == ucTid))
				return &prQM->arRxBaTable[i];
		}
	}
	return NULL;
}

u_int8_t qmAddRxBaEntry(IN struct ADAPTER *prAdapter,
	IN uint8_t ucStaRecIdx, IN uint8_t ucTid,
	IN uint16_t u2WinStart, IN uint16_t u2WinSize)
{
	int i;
	struct RX_BA_ENTRY *prRxBaEntry = NULL;
	struct STA_RECORD *prStaRec;
	struct QUE_MGT *prQM = &prAdapter->rQM;

	ASSERT(ucStaRecIdx < CFG_STA_REC_NUM);

	if (ucStaRecIdx >= CFG_STA_REC_NUM) {
		/* Invalid STA_REC index, discard the event packet */
		DBGLOG(QM, WARN,
			"QM: (WARNING) RX ADDBA Event for a invalid ucStaRecIdx = %d\n",
			ucStaRecIdx);
		return FALSE;
	}

	prStaRec = &prAdapter->arStaRec[ucStaRecIdx];
	ASSERT(prStaRec);

	/* 4 <1> Delete before adding */
	/* Remove the BA entry for the same (STA, TID) tuple if it exists */
	/* prQM->ucRxBaCount-- */
	if (qmLookupRxBaEntry(prAdapter, ucStaRecIdx, ucTid))
		qmDelRxBaEntry(prAdapter, ucStaRecIdx, ucTid, TRUE);
	/* 4 <2> Add a new BA entry */
	/* No available entry to store the BA agreement info. Retrun FALSE. */
	if (prQM->ucRxBaCount >= CFG_NUM_OF_RX_BA_AGREEMENTS) {
		DBGLOG(QM, ERROR,
			"QM: **failure** (limited resource, ucRxBaCount=%d)\n",
			prQM->ucRxBaCount);
		return FALSE;
	}
	/* Find the free-to-use BA entry */
	for (i = 0; i < CFG_NUM_OF_RX_BA_AGREEMENTS; i++) {
		if (!prQM->arRxBaTable[i].fgIsValid) {
			prRxBaEntry = &(prQM->arRxBaTable[i]);
			prQM->ucRxBaCount++;
			DBGLOG(QM, LOUD,
				"QM: ucRxBaCount=%d\n", prQM->ucRxBaCount);
			break;
		}
	}

	/* If a free-to-use entry is found,
	 * configure it and associate it with the STA_REC
	 */
	u2WinSize += CFG_RX_BA_INC_SIZE;
	if (prRxBaEntry) {
		prRxBaEntry->ucStaRecIdx = ucStaRecIdx;
		prRxBaEntry->ucTid = ucTid;
		prRxBaEntry->u2WinStart = u2WinStart;
		prRxBaEntry->u2WinSize = u2WinSize;
		prRxBaEntry->u2WinEnd = ((u2WinStart + u2WinSize - 1) %
			MAX_SEQ_NO_COUNT);
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
			prRxBaEntry->u2WinEnd,
			prRxBaEntry->u2WinSize);

		/* Update the BA entry reference table for per-packet lookup */
		prStaRec->aprRxReorderParamRefTbl[ucTid] = prRxBaEntry;
	} else {
		/* This shall not happen because
		 * FW should keep track of the usage of RX BA entries
		 */
		DBGLOG(QM, ERROR, "QM: **AddBA Error** (ucRxBaCount=%d)\n",
			prQM->ucRxBaCount);
		return FALSE;
	}


	return TRUE;
}

void qmDelRxBaEntry(IN struct ADAPTER *prAdapter,
	IN uint8_t ucStaRecIdx, IN uint8_t ucTid,
	IN u_int8_t fgFlushToHost)
{
	struct RX_BA_ENTRY *prRxBaEntry;
	struct STA_RECORD *prStaRec;
	struct SW_RFB *prFlushedPacketList = NULL;
	struct QUE_MGT *prQM = &prAdapter->rQM;

	ASSERT(ucStaRecIdx < CFG_STA_REC_NUM);

	prStaRec = &prAdapter->arStaRec[ucStaRecIdx];
	ASSERT(prStaRec);

#if 0
	if (!(prStaRec->fgIsValid)) {
		DbgPrint("QM: (WARNING) Invalid STA when deleting an RX BA\n");
		return;
	}
#endif

	if (ucTid >= CFG_RX_MAX_BA_TID_NUM) {
		DBGLOG(QM, WARN, "QM: ucTid invalid: %d in %s)\n",
			ucTid, __func__);
		return;
	}

	/* Remove the BA entry for the same (STA, TID) tuple if it exists */
	prRxBaEntry = prStaRec->aprRxReorderParamRefTbl[ucTid];

	if (prRxBaEntry) {

		prFlushedPacketList = qmFlushStaRxQueue(prAdapter,
			ucStaRecIdx, ucTid);

		if (prFlushedPacketList) {

			if (fgFlushToHost) {
				wlanProcessQueuedSwRfb(prAdapter,
					prFlushedPacketList);
			} else {

				struct SW_RFB *prSwRfb;
				struct SW_RFB *prNextSwRfb;

				prSwRfb = prFlushedPacketList;

				do {
					prNextSwRfb = (struct SW_RFB *)
						QUEUE_GET_NEXT_ENTRY(
						(struct QUE_ENTRY *) prSwRfb);
					nicRxReturnRFB(prAdapter, prSwRfb);
					prSwRfb = prNextSwRfb;
				} while (prSwRfb);

			}

		}

		if (prRxBaEntry->fgHasBubble) {
			DBGLOG(QM, TRACE,
				"QM:(Bub Check Cancel) STA[%u] TID[%u], DELBA\n",
			  prRxBaEntry->ucStaRecIdx, prRxBaEntry->ucTid);

			cnmTimerStopTimer(prAdapter,
				&prRxBaEntry->rReorderBubbleTimer);
			prRxBaEntry->fgHasBubble = FALSE;
		}
#if ((QM_TEST_MODE == 0) && (QM_TEST_STA_REC_DEACTIVATION == 0))
		/* Update RX BA entry state.
		 * Note that RX queue flush is not done here
		 */
		prRxBaEntry->fgIsValid = FALSE;
		prQM->ucRxBaCount--;

		/* Debug */
#if 0
		DbgPrint("QM: ucRxBaCount=%d\n", prQM->ucRxBaCount);
#endif

		/* Update STA RX BA table */
		prStaRec->aprRxReorderParamRefTbl[ucTid] = NULL;
#endif

		DBGLOG(QM, INFO, "QM: -RxBA(STA=%d,TID=%d)\n",
			ucStaRecIdx, ucTid);

	}

	/* Debug */
#if CFG_HIF_RX_STARVATION_WARNING
	{
		struct RX_CTRL *prRxCtrl;

		prRxCtrl = &prAdapter->rRxCtrl;
		DBGLOG(QM, TRACE,
			"QM: (RX DEBUG) Enqueued: %d / Dequeued: %d\n",
			prRxCtrl->u4QueuedCnt, prRxCtrl->u4DequeuedCnt);
	}
#endif
}

void mqmParseAssocReqWmmIe(IN struct ADAPTER *prAdapter,
	IN uint8_t *pucIE, IN struct STA_RECORD *prStaRec)
{
	struct IE_WMM_INFO *prIeWmmInfo;
	uint8_t ucQosInfo;
	uint8_t ucQosInfoAC;
	uint8_t ucBmpAC;
	uint8_t aucWfaOui[] = VENDOR_OUI_WFA;

	if ((WMM_IE_OUI_TYPE(pucIE) == VENDOR_OUI_TYPE_WMM)
		&& (!kalMemCmp(WMM_IE_OUI(pucIE), aucWfaOui, 3))) {

		switch (WMM_IE_OUI_SUBTYPE(pucIE)) {
		case VENDOR_OUI_SUBTYPE_WMM_INFO:
			if (IE_LEN(pucIE) != 7)
				break;	/* WMM Info IE with a wrong length */

			prStaRec->fgIsQoS = TRUE;
			prStaRec->fgIsWmmSupported = TRUE;

			prIeWmmInfo = (struct IE_WMM_INFO *) pucIE;
			ucQosInfo = prIeWmmInfo->ucQosInfo;
			ucQosInfoAC = ucQosInfo & BITS(0, 3);

			if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucUapsd))
				prStaRec->fgIsUapsdSupported =
					(ucQosInfoAC) ? TRUE : FALSE;
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
			prStaRec->ucBmpTriggerAC = prStaRec->ucBmpDeliveryAC =
				ucBmpAC;
			prStaRec->ucUapsdSp = (ucQosInfo &
				WMM_QOS_INFO_MAX_SP_LEN_MASK) >> 5;
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
void mqmProcessAssocReq(IN struct ADAPTER *prAdapter,
	IN struct SW_RFB *prSwRfb, IN uint8_t *pucIE,
	IN uint16_t u2IELength)
{
	struct STA_RECORD *prStaRec;
	uint16_t u2Offset;
	uint8_t *pucIEStart;
	uint32_t u4Flags;

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

	/* If the device does not support QoS or if
	 * WMM is not supported by the peer, exit.
	 */
	if (IS_FEATURE_DISABLED(prAdapter->rWifiVar.ucQoS))
		return;

	/* Determine whether QoS is enabled with the association */
	else {
		prStaRec->u4Flags = 0;
		IE_FOR_EACH(pucIE, u2IELength, u2Offset) {
			switch (IE_ID(pucIE)) {
			case ELEM_ID_VENDOR:
				mqmParseAssocReqWmmIe(prAdapter,
					pucIE, prStaRec);

#if CFG_SUPPORT_MTK_SYNERGY
				if (rlmParseCheckMTKOuiIE(prAdapter,
					pucIE, &u4Flags))
					prStaRec->u4Flags = u4Flags;
#endif

				break;

			case ELEM_ID_HT_CAP:
				/* Some client won't put the WMM IE
				 * if client is 802.11n
				 */
				if (IE_LEN(pucIE) ==
					(sizeof(struct IE_HT_CAP) - 2))
					prStaRec->fgIsQoS = TRUE;
				break;
			default:
				break;
			}
		}

		DBGLOG(QM, TRACE,
			"MQM: Assoc_Req Parsing (QoS Enabled=%d)\n",
			prStaRec->fgIsQoS);

	}
}

void mqmParseAssocRspWmmIe(IN uint8_t *pucIE,
	IN struct STA_RECORD *prStaRec)
{
	uint8_t aucWfaOui[] = VENDOR_OUI_WFA;

	if ((WMM_IE_OUI_TYPE(pucIE) == VENDOR_OUI_TYPE_WMM)
		&& (!kalMemCmp(WMM_IE_OUI(pucIE), aucWfaOui, 3))) {
		struct IE_WMM_PARAM *prWmmParam = (struct IE_WMM_PARAM *) pucIE;
		enum ENUM_ACI eAci;

		switch (WMM_IE_OUI_SUBTYPE(pucIE)) {
		case VENDOR_OUI_SUBTYPE_WMM_PARAM:
			if (IE_LEN(pucIE) != 24)
				break;	/* WMM Info IE with a wrong length */
			prStaRec->fgIsQoS = TRUE;
			prStaRec->fgIsUapsdSupported =
				!!(prWmmParam->ucQosInfo & WMM_QOS_INFO_UAPSD);
			for (eAci = ACI_BE; eAci < ACI_NUM; eAci++)
				prStaRec->afgAcmRequired[eAci] = !!(
					prWmmParam->arAcParam[eAci].ucAciAifsn &
					WMM_ACIAIFSN_ACM);
			DBGLOG(WMM, INFO,
			       "WMM: " MACSTR "ACM BK=%d BE=%d VI=%d VO=%d\n",
			       MAC2STR(prStaRec->aucMacAddr),
			       prStaRec->afgAcmRequired[ACI_BK],
			       prStaRec->afgAcmRequired[ACI_BE],
			       prStaRec->afgAcmRequired[ACI_VI],
			       prStaRec->afgAcmRequired[ACI_VO]);
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
void mqmProcessAssocRsp(IN struct ADAPTER *prAdapter,
	IN struct SW_RFB *prSwRfb, IN uint8_t *pucIE,
	IN uint16_t u2IELength)
{
	struct STA_RECORD *prStaRec;
	uint16_t u2Offset;
	uint8_t *pucIEStart;
	uint32_t u4Flags;
#if DSCP_SUPPORT
	u_int8_t hasnoQosMapSetIE = TRUE;
#endif

	DEBUGFUNC("mqmProcessAssocRsp");

	ASSERT(prSwRfb);
	ASSERT(pucIE);

	prStaRec = cnmGetStaRecByIndex(prAdapter,
		prSwRfb->ucStaRecIdx);
	ASSERT(prStaRec);

	if (prStaRec == NULL)
		return;

	prStaRec->fgIsQoS = FALSE;

	pucIEStart = pucIE;

	DBGLOG(QM, TRACE,
		"QM: (fgIsWmmSupported=%d, fgSupportQoS=%d)\n",
		prStaRec->fgIsWmmSupported, prAdapter->rWifiVar.ucQoS);

	/* If the device does not support QoS
	 * or if WMM is not supported by the peer, exit.
	 */
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
				if (rlmParseCheckMTKOuiIE(prAdapter,
					pucIE, &u4Flags))
					prStaRec->u4Flags = u4Flags;
#endif

				break;

			case ELEM_ID_HT_CAP:
				/* Some AP won't put the WMM IE
				 * if client is 802.11n
				 */
				if (IE_LEN(pucIE) ==
					(sizeof(struct IE_HT_CAP) - 2))
					prStaRec->fgIsQoS = TRUE;
				break;
#if DSCP_SUPPORT
			case ELEM_ID_QOS_MAP_SET:
				DBGLOG(QM, WARN,
					"QM: received assoc resp qosmapset ie\n");
				prStaRec->qosMapSet =
					qosParseQosMapSet(prAdapter, pucIE);
				hasnoQosMapSetIE = FALSE;
				break;
#endif
			default:
				break;
			}
		}
#if DSCP_SUPPORT
		if (hasnoQosMapSetIE) {
			DBGLOG(QM, WARN,
				"QM: remove assoc resp qosmapset ie\n");
			QosMapSetRelease(prStaRec);
			prStaRec->qosMapSet = NULL;
		}
#endif
		/* Parse AC parameters and write to HW CRs */
		if ((prStaRec->fgIsQoS)
			&& (prStaRec->eStaType == STA_TYPE_LEGACY_AP)) {
			mqmParseEdcaParameters(prAdapter, prSwRfb, pucIEStart,
				u2IELength, TRUE);
#if ARP_MONITER_ENABLE
			qmResetArpDetect();
#endif
		}
		DBGLOG(QM, TRACE,
			"MQM: Assoc_Rsp Parsing (QoS Enabled=%d)\n",
			prStaRec->fgIsQoS);
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
void mqmProcessBcn(IN struct ADAPTER *prAdapter,
	IN struct SW_RFB *prSwRfb, IN uint8_t *pucIE,
	IN uint16_t u2IELength)
{
	struct BSS_INFO *prBssInfo;
	u_int8_t fgNewParameter;
	uint8_t i;

	ASSERT(prAdapter);
	ASSERT(prSwRfb);
	ASSERT(pucIE);

	DBGLOG(QM, TRACE, "Enter %s\n", __func__);

	fgNewParameter = FALSE;

	for (i = 0; i < prAdapter->ucHwBssIdNum; i++) {
		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, i);

		if (IS_BSS_ACTIVE(prBssInfo)) {
			if (prBssInfo->eCurrentOPMode ==
				OP_MODE_INFRASTRUCTURE &&
			    prBssInfo->eConnectionState ==
			    PARAM_MEDIA_STATE_CONNECTED) {
				/* P2P client or AIS infra STA */
				if (EQUAL_MAC_ADDR(prBssInfo->aucBSSID,
					((struct WLAN_MAC_MGMT_HEADER *)
					(prSwRfb->pvHeader))->aucBSSID)) {

					fgNewParameter =
						mqmParseEdcaParameters(
							prAdapter,
							prSwRfb, pucIE,
							u2IELength, FALSE);
				}
			}

			/* Appy new parameters if necessary */
			if (fgNewParameter) {
				nicQmUpdateWmmParms(prAdapter,
					prBssInfo->ucBssIndex);
				fgNewParameter = FALSE;
			}
		}		/* end of IS_BSS_ACTIVE() */
	}
}

u_int8_t mqmUpdateEdcaParameters(IN struct BSS_INFO	*prBssInfo,
	IN uint8_t *pucIE, IN u_int8_t fgForceOverride)
{
	struct AC_QUE_PARMS *prAcQueParams;
	struct IE_WMM_PARAM *prIeWmmParam;
	enum ENUM_WMM_ACI eAci;
	u_int8_t fgNewParameter = FALSE;

	do {
		if (IE_LEN(pucIE) != 24)
			break;	/* WMM Param IE with a wrong length */

		prIeWmmParam = (struct IE_WMM_PARAM *) pucIE;

		/* Check the Parameter Set Count to determine
		 * whether EDCA parameters have been changed
		 */
		if (!fgForceOverride) {
			if (mqmCompareEdcaParameters(prIeWmmParam, prBssInfo)) {
				fgNewParameter = FALSE;
				break;
			}
		}

		fgNewParameter = TRUE;
		/* Update Parameter Set Count */
		prBssInfo->ucWmmParamSetCount = (prIeWmmParam->ucQosInfo &
			WMM_QOS_INFO_PARAM_SET_CNT);
		/* Update EDCA parameters */
		for (eAci = 0; eAci < WMM_AC_INDEX_NUM; eAci++) {
			prAcQueParams = &prBssInfo->arACQueParms[eAci];
			mqmFillAcQueParam(prIeWmmParam, eAci, prAcQueParams);
			log_dbg(QM, INFO, "BSS[%u]: eAci[%d] ACM[%d] Aifsn[%d] CWmin/max[%d/%d] TxopLimit[%d] NewParameter[%d]\n",
				prBssInfo->ucBssIndex, eAci,
				prAcQueParams->ucIsACMSet,
				prAcQueParams->u2Aifsn, prAcQueParams->u2CWmin,
				prAcQueParams->u2CWmax,
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
 * \param[in] fgForceOverride    TRUE: If EDCA parameters are found, always set
 *                               to HW CRs.
 *
 * \return none
 */
/*----------------------------------------------------------------------------*/
u_int8_t
mqmParseEdcaParameters(IN struct ADAPTER *prAdapter,
	IN struct SW_RFB *prSwRfb, IN uint8_t *pucIE,
	IN uint16_t u2IELength, IN u_int8_t fgForceOverride)
{
	struct STA_RECORD *prStaRec;
	uint16_t u2Offset;
	uint8_t aucWfaOui[] = VENDOR_OUI_WFA;
	struct BSS_INFO *prBssInfo;
	u_int8_t fgNewParameter = FALSE;

	DEBUGFUNC("mqmParseEdcaParameters");

	if (!prSwRfb)
		return FALSE;

	if (!pucIE)
		return FALSE;

	prStaRec = cnmGetStaRecByIndex(prAdapter,
		prSwRfb->ucStaRecIdx);
	/* ASSERT(prStaRec); */

	if (prStaRec == NULL)
		return FALSE;

	DBGLOG(QM, TRACE, "QM: (fgIsWmmSupported=%d, fgIsQoS=%d)\n",
		prStaRec->fgIsWmmSupported, prStaRec->fgIsQoS);

	if (IS_FEATURE_DISABLED(prAdapter->rWifiVar.ucQoS)
	    || (!prStaRec->fgIsWmmSupported)
	    || (!prStaRec->fgIsQoS))
		return FALSE;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
		prStaRec->ucBssIndex);

	/* Goal: Obtain the EDCA parameters */
	IE_FOR_EACH(pucIE, u2IELength, u2Offset) {
		switch (IE_ID(pucIE)) {
		case ELEM_ID_WMM:
			if (!((WMM_IE_OUI_TYPE(pucIE) == VENDOR_OUI_TYPE_WMM) &&
			      (!kalMemCmp(WMM_IE_OUI(pucIE), aucWfaOui, 3))))
				break;

			switch (WMM_IE_OUI_SUBTYPE(pucIE)) {
			case VENDOR_OUI_SUBTYPE_WMM_PARAM:
				fgNewParameter =
					mqmUpdateEdcaParameters(prBssInfo,
					pucIE, fgForceOverride);
				break;

			default:
				/* Other WMM QoS IEs. Ignore */
				break;
			}

			/* else: VENDOR_OUI_TYPE_WPA, VENDOR_OUI_TYPE_WPS, ...
			 * (not cared)
			 */
			break;
		default:
			break;
		}
	}

	return fgNewParameter;
}

u_int8_t mqmCompareEdcaParameters(IN struct IE_WMM_PARAM *prIeWmmParam,
	IN struct BSS_INFO *prBssInfo)
{
	struct AC_QUE_PARMS *prAcQueParams;
	struct WMM_AC_PARAM *prWmmAcParams;
	enum ENUM_WMM_ACI eAci;

	/* return FALSE; */

	/* Check Set Count */
	if (prBssInfo->ucWmmParamSetCount !=
	(prIeWmmParam->ucQosInfo & WMM_QOS_INFO_PARAM_SET_CNT))
		return FALSE;

	for (eAci = 0; eAci < WMM_AC_INDEX_NUM; eAci++) {
		prAcQueParams = &prBssInfo->arACQueParms[eAci];
		prWmmAcParams = &prIeWmmParam->arAcParam[eAci];

		/* ACM */
		if (prAcQueParams->ucIsACMSet != ((prWmmAcParams->ucAciAifsn &
			WMM_ACIAIFSN_ACM) ? TRUE : FALSE))
			return FALSE;

		/* AIFSN */
		if (prAcQueParams->u2Aifsn != (prWmmAcParams->ucAciAifsn &
			WMM_ACIAIFSN_AIFSN))
			return FALSE;

		/* CW Max */
		if (prAcQueParams->u2CWmax !=
			(BIT((prWmmAcParams->ucEcw & WMM_ECW_WMAX_MASK) >>
			WMM_ECW_WMAX_OFFSET) - 1))
			return FALSE;

		/* CW Min */
		if (prAcQueParams->u2CWmin != (BIT(prWmmAcParams->ucEcw &
			WMM_ECW_WMIN_MASK) - 1))
			return FALSE;

		if (prAcQueParams->u2TxopLimit !=
			prWmmAcParams->u2TxopLimit)
			return FALSE;
	}

	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is used for parsing EDCA parameters specified in the
 *        WMM Parameter IE
 *
 * \param[in] prAdapter           Adapter pointer
 * \param[in] prIeWmmParam        The pointer to the WMM Parameter IE
 * \param[in] u4AcOffset          The offset specifying the AC queue for parsing
 * \param[in] prHwAcParams        The parameter structure used to configure the
 *                                HW CRs
 *
 * \return none
 */
/*----------------------------------------------------------------------------*/
void mqmFillAcQueParam(IN struct IE_WMM_PARAM *prIeWmmParam,
	IN uint32_t u4AcOffset,
	OUT struct AC_QUE_PARMS *prAcQueParams)
{
	struct WMM_AC_PARAM *prAcParam =
		&prIeWmmParam->arAcParam[u4AcOffset];

	prAcQueParams->ucIsACMSet = (prAcParam->ucAciAifsn &
		WMM_ACIAIFSN_ACM) ? TRUE : FALSE;

	prAcQueParams->u2Aifsn = (prAcParam->ucAciAifsn &
		WMM_ACIAIFSN_AIFSN);

	prAcQueParams->u2CWmax = BIT((prAcParam->ucEcw &
		WMM_ECW_WMAX_MASK) >> WMM_ECW_WMAX_OFFSET) - 1;

	prAcQueParams->u2CWmin = BIT(prAcParam->ucEcw &
		WMM_ECW_WMIN_MASK) - 1;

	WLAN_GET_FIELD_16(&prAcParam->u2TxopLimit,
		&prAcQueParams->u2TxopLimit);

	prAcQueParams->ucGuradTime =
		TXM_DEFAULT_FLUSH_QUEUE_GUARD_TIME;

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief To parse WMM/11n related IEs in scan results (only for AP peers)
 *
 * \param[in] prAdapter       Adapter pointer
 * \param[in]  prScanResult   The scan result which shall be parsed to
 *                            obtain needed info
 * \param[out] prStaRec       The obtained info is stored in the STA_REC
 *
 * \return none
 */
/*----------------------------------------------------------------------------*/
void mqmProcessScanResult(IN struct ADAPTER *prAdapter,
	IN struct BSS_DESC *prScanResult,
	OUT struct STA_RECORD *prStaRec)
{
	uint8_t *pucIE;
	uint16_t u2IELength;
	uint16_t u2Offset;
	uint8_t aucWfaOui[] = VENDOR_OUI_WFA;
	u_int8_t fgIsHtVht;

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
#if CFG_SUPPORT_802_11V_BSS_TRANSITION_MGT
			prStaRec->fgSupportBTM =
				!!((*(uint32_t *)(pucIE + 2)) &
			BIT(ELEM_EXT_CAP_BSS_TRANSITION_BIT));
#endif
			break;

		case ELEM_ID_WMM:
			if ((WMM_IE_OUI_TYPE(pucIE) == VENDOR_OUI_TYPE_WMM) &&
			    (!kalMemCmp(WMM_IE_OUI(pucIE), aucWfaOui, 3))) {
				struct IE_WMM_PARAM *prWmmParam =
					(struct IE_WMM_PARAM *)pucIE;
				enum ENUM_ACI eAci;

				switch (WMM_IE_OUI_SUBTYPE(pucIE)) {
				case VENDOR_OUI_SUBTYPE_WMM_PARAM:
					/* WMM Param IE with a wrong length */
					if (IE_LEN(pucIE) != 24)
						break;
					prStaRec->fgIsWmmSupported = TRUE;
					prStaRec->fgIsUapsdSupported =
						!!(prWmmParam->ucQosInfo &
						   WMM_QOS_INFO_UAPSD);
					for (eAci = ACI_BE; eAci < ACI_NUM;
					     eAci++)
						prStaRec->afgAcmRequired
							[eAci] = !!(
							prWmmParam
								->arAcParam
									[eAci]
								.ucAciAifsn &
							WMM_ACIAIFSN_ACM);
					DBGLOG(WMM, INFO,
					       "WMM: " MACSTR
					       "ACM BK=%d BE=%d VI=%d VO=%d\n",
					       MAC2STR(prStaRec->aucMacAddr),
					       prStaRec->afgAcmRequired[ACI_BK],
					       prStaRec->afgAcmRequired[ACI_BE],
					       prStaRec->afgAcmRequired[ACI_VI],
					       prStaRec->afgAcmRequired
						       [ACI_VO]);
					break;

				case VENDOR_OUI_SUBTYPE_WMM_INFO:
					/* WMM Info IE with a wrong length */
					if (IE_LEN(pucIE) != 7)
						break;
					prStaRec->fgIsWmmSupported = TRUE;
					prStaRec->fgIsUapsdSupported =
						((((
							(struct IE_WMM_INFO *)
							pucIE)->ucQosInfo)
							& WMM_QOS_INFO_UAPSD)
							? TRUE : FALSE);
					break;

				default:
					/* A WMM QoS IE that doesn't matter.
					 * Ignore it.
					 */
					break;
				}
			}
			break;

		default:
			/* A WMM IE that doesn't matter. Ignore it. */
			break;
		}
	}

	/* <1> Determine QoS */
	if (prStaRec->ucDesiredPhyTypeSet & (PHY_TYPE_SET_802_11N |
		PHY_TYPE_SET_802_11AC))
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
uint32_t
mqmFillWmmInfoIE(uint8_t *pucOutBuf,
	u_int8_t fgSupportUAPSD, uint8_t ucBmpDeliveryAC,
	uint8_t ucBmpTriggerAC, uint8_t ucUapsdSp)
{
	struct IE_WMM_INFO *prIeWmmInfo;
	uint32_t ucUapsd[] = {
		WMM_QOS_INFO_BE_UAPSD,
		WMM_QOS_INFO_BK_UAPSD,
		WMM_QOS_INFO_VI_UAPSD,
		WMM_QOS_INFO_VO_UAPSD
	};
	uint8_t aucWfaOui[] = VENDOR_OUI_WFA;

	ASSERT(pucOutBuf);

	prIeWmmInfo = (struct IE_WMM_INFO *) pucOutBuf;

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
		uint8_t ucQosInfo = 0;
		uint8_t i;

		/* Static U-APSD setting */
		for (i = ACI_BE; i <= ACI_VO; i++) {
			if (ucBmpDeliveryAC & ucBmpTriggerAC & BIT(i))
				ucQosInfo |= (uint8_t) ucUapsd[i];
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

	/* Increment the total IE length
	 * for the Element ID and Length fields.
	 */
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
uint32_t
mqmGenerateWmmInfoIEByStaRec(struct ADAPTER *prAdapter,
	struct BSS_INFO *prBssInfo, struct STA_RECORD *prStaRec,
	uint8_t *pucOutBuf)
{
	struct PM_PROFILE_SETUP_INFO *prPmProfSetupInfo;
	u_int8_t fgSupportUapsd;

	ASSERT(pucOutBuf);

	/* In case QoS is not turned off, exit directly */
	if (IS_FEATURE_DISABLED(prAdapter->rWifiVar.ucQoS))
		return 0;

	if (prStaRec == NULL)
		return 0;

	if (!prStaRec->fgIsQoS)
		return 0;

	prPmProfSetupInfo = &prBssInfo->rPmProfSetupInfo;

	fgSupportUapsd = (IS_FEATURE_ENABLED(
		prAdapter->rWifiVar.ucUapsd) &&
		prStaRec->fgIsUapsdSupported);

	return mqmFillWmmInfoIE(pucOutBuf,
		fgSupportUapsd,
		prPmProfSetupInfo->ucBmpDeliveryAC,
		prPmProfSetupInfo->ucBmpTriggerAC,
		prPmProfSetupInfo->ucUapsdSp);
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
void mqmGenerateWmmInfoIE(IN struct ADAPTER *prAdapter,
	IN struct MSDU_INFO *prMsduInfo)
{
	struct BSS_INFO *prBssInfo;
	struct STA_RECORD *prStaRec;
	uint32_t u4Length;

	DEBUGFUNC("mqmGenerateWmmInfoIE");

	ASSERT(prMsduInfo);

	prStaRec = cnmGetStaRecByIndex(prAdapter,
		prMsduInfo->ucStaRecIndex);
	ASSERT(prStaRec);

	if (prStaRec == NULL)
		return;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
		prStaRec->ucBssIndex);

	u4Length = mqmGenerateWmmInfoIEByStaRec(prAdapter,
		prBssInfo, prStaRec,
		((uint8_t *) prMsduInfo->prPacket +
		prMsduInfo->u2FrameLength));

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
void mqmGenerateWmmParamIE(IN struct ADAPTER *prAdapter,
	IN struct MSDU_INFO *prMsduInfo)
{
	struct IE_WMM_PARAM *prIeWmmParam;

	uint8_t aucWfaOui[] = VENDOR_OUI_WFA;

	uint8_t aucACI[] = {
		WMM_ACI_AC_BE,
		WMM_ACI_AC_BK,
		WMM_ACI_AC_VI,
		WMM_ACI_AC_VO
	};

	struct BSS_INFO *prBssInfo;
	struct STA_RECORD *prStaRec;
	enum ENUM_WMM_ACI eAci;
	struct WMM_AC_PARAM *prAcParam;

	DEBUGFUNC("mqmGenerateWmmParamIE");
	DBGLOG(QM, LOUD, "\n");

	ASSERT(prMsduInfo);

	/* In case QoS is not turned off, exit directly */
	if (IS_FEATURE_DISABLED(prAdapter->rWifiVar.ucQoS))
		return;

	prStaRec = cnmGetStaRecByIndex(prAdapter,
		prMsduInfo->ucStaRecIndex);

	if (prStaRec) {
		if (!prStaRec->fgIsQoS)
			return;
	}

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
		prMsduInfo->ucBssIndex);

	if (!prBssInfo->fgIsQBSS)
		return;

	prIeWmmParam = (struct IE_WMM_PARAM *)
		((uint8_t *) prMsduInfo->prPacket +
		prMsduInfo->u2FrameLength);

	prIeWmmParam->ucId = ELEM_ID_WMM;
	prIeWmmParam->ucLength = ELEM_MAX_LEN_WMM_PARAM;

	/* WMM-2.2.1 WMM Information Element Field Values */
	prIeWmmParam->aucOui[0] = aucWfaOui[0];
	prIeWmmParam->aucOui[1] = aucWfaOui[1];
	prIeWmmParam->aucOui[2] = aucWfaOui[2];
	prIeWmmParam->ucOuiType = VENDOR_OUI_TYPE_WMM;
	prIeWmmParam->ucOuiSubtype = VENDOR_OUI_SUBTYPE_WMM_PARAM;

	prIeWmmParam->ucVersion = VERSION_WMM;
	prIeWmmParam->ucQosInfo = (prBssInfo->ucWmmParamSetCount &
		WMM_QOS_INFO_PARAM_SET_CNT);

	/* UAPSD initial queue configurations (delivery and trigger enabled) */
	if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucUapsd))
		prIeWmmParam->ucQosInfo |= WMM_QOS_INFO_UAPSD;

	/* EDCA parameter */

	for (eAci = 0; eAci < WMM_AC_INDEX_NUM; eAci++) {
		prAcParam = &prIeWmmParam->arAcParam[eAci];

		/* ACI */
		prAcParam->ucAciAifsn = aucACI[eAci];
		/* ACM */
		if (prBssInfo->arACQueParmsForBcast[eAci].ucIsACMSet)
			prAcParam->ucAciAifsn |= WMM_ACIAIFSN_ACM;
		/* AIFSN */
		prAcParam->ucAciAifsn |=
			(prBssInfo->arACQueParmsForBcast[eAci].u2Aifsn &
			WMM_ACIAIFSN_AIFSN);

		/* ECW Min */
		prAcParam->ucEcw = (prBssInfo->aucCWminLog2ForBcast[eAci] &
			WMM_ECW_WMIN_MASK);
		/* ECW Max */
		prAcParam->ucEcw |=
			((prBssInfo->aucCWmaxLog2ForBcast[eAci] <<
			WMM_ECW_WMAX_OFFSET) & WMM_ECW_WMAX_MASK);

		/* Txop limit */
		WLAN_SET_FIELD_16(&prAcParam->u2TxopLimit,
			prBssInfo->arACQueParmsForBcast[eAci].u2TxopLimit);

	}

	/* Increment the total IE length
	 * for the Element ID and Length fields.
	 */
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
uint32_t mqmGenerateWmmParamIEByParam(struct ADAPTER *prAdapter,
	struct BSS_INFO *prBssInfo, uint8_t *pOutBuf)
{
	struct IE_WMM_PARAM *prIeWmmParam;

	uint8_t aucWfaOui[] = VENDOR_OUI_WFA;

	uint8_t aucACI[] = {
		WMM_ACI_AC_BE,
		WMM_ACI_AC_BK,
		WMM_ACI_AC_VI,
		WMM_ACI_AC_VO
	};

	struct AC_QUE_PARMS *prACQueParms;
	/* BE, BK, VO, VI */
	uint8_t auCWminLog2ForBcast[WMM_AC_INDEX_NUM] = {4, 4, 3, 2};
	uint8_t auCWmaxLog2ForBcast[WMM_AC_INDEX_NUM] = {10, 10, 4, 3};
	uint8_t auAifsForBcast[WMM_AC_INDEX_NUM] = {3, 7, 2, 2};
	/* If the AP is OFDM */
	uint8_t auTxopForBcast[WMM_AC_INDEX_NUM] = {0, 0, 94, 47};

	enum ENUM_WMM_ACI eAci;
	struct WMM_AC_PARAM *prAcParam;

	DEBUGFUNC("mqmGenerateWmmParamIE");
	DBGLOG(QM, LOUD, "\n");

	ASSERT(pOutBuf);

	/* In case QoS is not turned off, exit directly */
	if (IS_FEATURE_DISABLED(prAdapter->rWifiVar.ucQoS))
		return WLAN_STATUS_SUCCESS;

	if (!prBssInfo->fgIsQBSS)
		return WLAN_STATUS_SUCCESS;

	if (IS_FEATURE_ENABLED(
		prAdapter->rWifiVar.fgTdlsBufferSTASleep)) {
		prACQueParms = prBssInfo->arACQueParmsForBcast;

		for (eAci = 0; eAci < WMM_AC_INDEX_NUM; eAci++) {
			prACQueParms[eAci].ucIsACMSet = FALSE;
			prACQueParms[eAci].u2Aifsn = auAifsForBcast[eAci];
			prACQueParms[eAci].u2CWmin =
				BIT(auCWminLog2ForBcast[eAci]) - 1;
			prACQueParms[eAci].u2CWmax =
				BIT(auCWmaxLog2ForBcast[eAci]) - 1;
			prACQueParms[eAci].u2TxopLimit = auTxopForBcast[eAci];

			/* used to send WMM IE */
			prBssInfo->aucCWminLog2ForBcast[eAci] =
				auCWminLog2ForBcast[eAci];
			prBssInfo->aucCWmaxLog2ForBcast[eAci] =
				auCWmaxLog2ForBcast[eAci];
		}
	}

	prIeWmmParam = (struct IE_WMM_PARAM *) pOutBuf;

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
	 * The STAUT sends a TDLS Setup/Response/Confirm Frame,
	 * with all four AC flags set to 1 in QoS Info Field
	 * to STA 2, via the AP,
	 */
	if (IS_FEATURE_ENABLED(
		    prAdapter->rWifiVar.fgTdlsBufferSTASleep))
		prIeWmmParam->ucQosInfo = (0x0F &
			WMM_QOS_INFO_PARAM_SET_CNT);
	else
		prIeWmmParam->ucQosInfo = (prBssInfo->ucWmmParamSetCount &
			WMM_QOS_INFO_PARAM_SET_CNT);

	/* UAPSD initial queue configurations (delivery and trigger enabled) */
	if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucUapsd))
		prIeWmmParam->ucQosInfo |= WMM_QOS_INFO_UAPSD;

	/* EDCA parameter */

	for (eAci = 0; eAci < WMM_AC_INDEX_NUM; eAci++) {
		prAcParam = &prIeWmmParam->arAcParam[eAci];
		/* ACI */
		prAcParam->ucAciAifsn = aucACI[eAci];
		/* ACM */
		if (prBssInfo->arACQueParmsForBcast[eAci].ucIsACMSet)
			prAcParam->ucAciAifsn |= WMM_ACIAIFSN_ACM;
		/* AIFSN */
		prAcParam->ucAciAifsn |=
			(prBssInfo->arACQueParmsForBcast[eAci].u2Aifsn &
			WMM_ACIAIFSN_AIFSN);

		/* ECW Min */
		prAcParam->ucEcw = (prBssInfo->aucCWminLog2ForBcast[eAci] &
			WMM_ECW_WMIN_MASK);
		/* ECW Max */
		prAcParam->ucEcw |=
			((prBssInfo->aucCWmaxLog2ForBcast[eAci] <<
			WMM_ECW_WMAX_OFFSET) & WMM_ECW_WMAX_MASK);

		/* Txop limit */
		WLAN_SET_FIELD_16(&prAcParam->u2TxopLimit,
			prBssInfo->arACQueParmsForBcast[eAci].u2TxopLimit);

	}

	/* Increment the total IE length
	 * for the Element ID and Length fields.
	 */
	return IE_SIZE(prIeWmmParam);

}

#endif

u_int8_t isProbeResponse(IN struct MSDU_INFO *prMgmtTxMsdu)
{
	struct WLAN_MAC_HEADER *prWlanHdr =
		(struct WLAN_MAC_HEADER *) NULL;

	prWlanHdr =
		(struct WLAN_MAC_HEADER *) ((unsigned long)
		prMgmtTxMsdu->prPacket + MAC_TX_RESERVED_FIELD);

	return (prWlanHdr->u2FrameCtrl & MASK_FRAME_TYPE) ==
		MAC_FRAME_PROBE_RSP ? TRUE : FALSE;
}


enum ENUM_FRAME_ACTION qmGetFrameAction(IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex,
	IN uint8_t ucStaRecIdx, IN struct MSDU_INFO *prMsduInfo,
	IN enum ENUM_FRAME_TYPE_IN_CMD_Q eFrameType,
	IN uint16_t u2FrameLength)
{
	enum ENUM_FRAME_ACTION eFrameAction = FRAME_ACTION_TX_PKT;
	struct BSS_INFO *prBssInfo;
	struct STA_RECORD *prStaRec;
	uint8_t ucTC = nicTxGetFrameResourceType(eFrameType, prMsduInfo);
	uint16_t u2FreeResource = nicTxGetResource(prAdapter, ucTC);
	uint8_t ucReqResource;
	struct WIFI_VAR *prWifiVar = &prAdapter->rWifiVar;

	DEBUGFUNC("qmGetFrameAction");

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);
	prStaRec = QM_GET_STA_REC_PTR_FROM_INDEX(prAdapter, ucStaRecIdx);

	do {
		/* 4 <1> Tx, if FORCE_TX is set */
		if (prMsduInfo) {
			if (prMsduInfo->ucControlFlag &
				MSDU_CONTROL_FLAG_FORCE_TX) {
				eFrameAction = FRAME_ACTION_TX_PKT;
				break;
			}
		}
		/* 4 <2> Drop, if BSS is inactive */
		if (!IS_BSS_ACTIVE(prBssInfo)) {
			DBGLOG(QM, TRACE,
				"Drop packets (BSS[%u] is INACTIVE)\n",
				prBssInfo->ucBssIndex);
			eFrameAction = FRAME_ACTION_DROP_PKT;
			break;
		}

		/* 4 <3> Queue, if BSS is absent, drop probe response */
		if (prBssInfo->fgIsNetAbsent) {
			if (prMsduInfo && isProbeResponse(prMsduInfo)) {
				DBGLOG(TX, TRACE,
					"Drop probe response (BSS[%u] Absent)\n",
					prBssInfo->ucBssIndex);

				eFrameAction = FRAME_ACTION_DROP_PKT;
			} else {
				DBGLOG(TX, TRACE,
					"Queue packets (BSS[%u] Absent)\n",
					prBssInfo->ucBssIndex);
				eFrameAction = FRAME_ACTION_QUEUE_PKT;
			}
			break;
		}

		/* 4 <4> Check based on StaRec */
		if (prStaRec) {
			/* 4 <4.1> Drop, if StaRec is not in use */
			if (!prStaRec->fgIsInUse) {
				DBGLOG(QM, TRACE,
					"Drop packets (Sta[%u] not in USE)\n",
					prStaRec->ucIndex);
				eFrameAction = FRAME_ACTION_DROP_PKT;
				break;
			}
			/* 4 <4.2> Sta in PS */
			if (prStaRec->fgIsInPS) {
				ucReqResource = nicTxGetPageCount(prAdapter,
					u2FrameLength, FALSE) +
					prWifiVar->ucCmdRsvResource +
					QM_MGMT_QUEUED_THRESHOLD;

				/* 4 <4.2.1> Tx, if resource is enough */
				if (u2FreeResource > ucReqResource) {
					eFrameAction = FRAME_ACTION_TX_PKT;
					break;
				}
				/* 4 <4.2.2> Queue, if resource is not enough */
				else {
					DBGLOG(QM, INFO,
						"Queue packets (Sta[%u] in PS)\n",
						prStaRec->ucIndex);
					eFrameAction = FRAME_ACTION_QUEUE_PKT;
					break;
				}
			}
		}

	} while (FALSE);

	/* <5> Resource CHECK! */
	/* <5.1> Reserve resource for CMD & 1X */
	if (eFrameType == FRAME_TYPE_MMPDU) {
		ucReqResource = nicTxGetPageCount(prAdapter, u2FrameLength,
			FALSE) + prWifiVar->ucCmdRsvResource;

		if (u2FreeResource < ucReqResource) {
			eFrameAction = FRAME_ACTION_QUEUE_PKT;
			DBGLOG(QM, INFO,
				"Queue MGMT (MSDU[0x%p] Req/Rsv/Free[%u/%u/%u])\n",
				prMsduInfo,
				nicTxGetPageCount(prAdapter,
					u2FrameLength, FALSE),
				prWifiVar->ucCmdRsvResource, u2FreeResource);
		}

		/* <6> Timeout check! */
#if CFG_ENABLE_PKT_LIFETIME_PROFILE
		if ((eFrameAction == FRAME_ACTION_QUEUE_PKT) && prMsduInfo) {
			OS_SYSTIME rCurrentTime, rEnqTime;

			GET_CURRENT_SYSTIME(&rCurrentTime);
			rEnqTime = prMsduInfo->rPktProfile.rEnqueueTimestamp;

			if (CHECK_FOR_TIMEOUT(rCurrentTime, rEnqTime,
				MSEC_TO_SYSTIME(
					prWifiVar->u4MgmtQueueDelayTimeout))) {
				eFrameAction = FRAME_ACTION_DROP_PKT;
				log_dbg(QM, INFO, "Drop MGMT (MSDU[0x%p] timeout[%ums])\n",
					prMsduInfo,
					prWifiVar->u4MgmtQueueDelayTimeout);
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
void qmHandleEventBssAbsencePresence(IN struct ADAPTER *prAdapter,
	IN struct WIFI_EVENT *prEvent)
{
	struct EVENT_BSS_ABSENCE_PRESENCE *prEventBssStatus;
	struct BSS_INFO *prBssInfo;
	u_int8_t fgIsNetAbsentOld;

	prEventBssStatus = (struct EVENT_BSS_ABSENCE_PRESENCE *) (
		prEvent->aucBuffer);
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
		prEventBssStatus->ucBssIndex);
	fgIsNetAbsentOld = prBssInfo->fgIsNetAbsent;
	prBssInfo->fgIsNetAbsent = prEventBssStatus->ucIsAbsent;
	prBssInfo->ucBssFreeQuota = prEventBssStatus->ucBssFreeQuota;

	DBGLOG(QM, INFO, "NAF=%d,%d,%d\n",
		prEventBssStatus->ucBssIndex, prBssInfo->fgIsNetAbsent,
		prBssInfo->ucBssFreeQuota);

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
		else {
#if QM_ADAPTIVE_TC_RESOURCE_CTRL
			prAdapter->rQM.fgForceReassign = TRUE;
#endif
			kalSetEvent(prAdapter->prGlueInfo);
		}
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
void qmHandleEventStaChangePsMode(IN struct ADAPTER *prAdapter,
	IN struct WIFI_EVENT *prEvent)
{
	struct EVENT_STA_CHANGE_PS_MODE *prEventStaChangePsMode;
	struct STA_RECORD *prStaRec;
	u_int8_t fgIsInPSOld;

	/* DbgPrint("QM:Event -RxBa\n"); */

	prEventStaChangePsMode = (struct EVENT_STA_CHANGE_PS_MODE *)
		(prEvent->aucBuffer);
	prStaRec = QM_GET_STA_REC_PTR_FROM_INDEX(prAdapter,
		prEventStaChangePsMode->ucStaRecIdx);
	/* ASSERT(prStaRec); */

	if (prStaRec) {

		fgIsInPSOld = prStaRec->fgIsInPS;
		prStaRec->fgIsInPS = prEventStaChangePsMode->ucIsInPs;

		qmUpdateFreeQuota(prAdapter,
			prStaRec,
			prEventStaChangePsMode->ucUpdateMode,
			prEventStaChangePsMode->ucFreeQuota);

		DBGLOG(QM, INFO, "PS=%d,%d\n",
			prEventStaChangePsMode->ucStaRecIdx,
			prStaRec->fgIsInPS);

		/* From PS to Awake */
		if ((fgIsInPSOld) && (!prStaRec->fgIsInPS)) {
			if (HAL_IS_TX_DIRECT(prAdapter))
				nicTxDirectStartCheckQTimer(prAdapter);
			else {
#if QM_ADAPTIVE_TC_RESOURCE_CTRL
				prAdapter->rQM.fgForceReassign = TRUE;
#endif
				kalSetEvent(prAdapter->prGlueInfo);
			}
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
void qmHandleEventStaUpdateFreeQuota(IN struct ADAPTER *prAdapter,
	IN struct WIFI_EVENT *prEvent)
{
	struct EVENT_STA_UPDATE_FREE_QUOTA
		*prEventStaUpdateFreeQuota;
	struct STA_RECORD *prStaRec;

	prEventStaUpdateFreeQuota = (struct
		EVENT_STA_UPDATE_FREE_QUOTA *) (prEvent->aucBuffer);
	prStaRec = QM_GET_STA_REC_PTR_FROM_INDEX(prAdapter,
			prEventStaUpdateFreeQuota->ucStaRecIdx);
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
		DBGLOG(QM, TRACE, "UFQ=%d,%d,%d\n",
			prEventStaUpdateFreeQuota->ucStaRecIdx,
			prEventStaUpdateFreeQuota->ucUpdateMode,
			prEventStaUpdateFreeQuota->ucFreeQuota);

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
void
qmUpdateFreeQuota(IN struct ADAPTER *prAdapter,
	IN struct STA_RECORD *prStaRec, IN uint8_t ucUpdateMode,
	IN uint8_t ucFreeQuota)
{

	uint8_t ucFreeQuotaForNonDelivery;
	uint8_t ucFreeQuotaForDelivery;

	ASSERT(prStaRec);
	DBGLOG(QM, LOUD,
		"qmUpdateFreeQuota orig ucFreeQuota=%d Mode %u New %u\n",
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

	DBGLOG(QM, LOUD, "qmUpdateFreeQuota new ucFreeQuota=%d)\n",
		prStaRec->ucFreeQuota);

	ucFreeQuota = prStaRec->ucFreeQuota;

	ucFreeQuotaForNonDelivery = 0;
	ucFreeQuotaForDelivery = 0;

	if (ucFreeQuota > 0) {
		if (prStaRec->fgIsQoS && prStaRec->fgIsUapsdSupported
		    /* && prAdapter->rWifiVar.fgSupportQoS */
		    /* && prAdapter->rWifiVar.fgSupportUAPSD */) {
			/* XXX We should assign quota to
			 * aucFreeQuotaPerQueue[NUM_OF_PER_STA_TX_QUEUES]
			 */

			if (prStaRec->ucFreeQuotaForNonDelivery > 0
			    && prStaRec->ucFreeQuotaForDelivery > 0) {
				ucFreeQuotaForNonDelivery = ucFreeQuota >> 1;
				ucFreeQuotaForDelivery = ucFreeQuota -
					ucFreeQuotaForNonDelivery;
			} else if (prStaRec->ucFreeQuotaForNonDelivery == 0
				   && prStaRec->ucFreeQuotaForDelivery == 0) {
				ucFreeQuotaForNonDelivery = ucFreeQuota >> 1;
				ucFreeQuotaForDelivery = ucFreeQuota -
					ucFreeQuotaForNonDelivery;
			} else if (prStaRec->ucFreeQuotaForNonDelivery > 0) {
				/* NonDelivery is not busy */
				if (ucFreeQuota >= 3) {
					ucFreeQuotaForNonDelivery = 2;
					ucFreeQuotaForDelivery = ucFreeQuota -
						ucFreeQuotaForNonDelivery;
				} else {
					ucFreeQuotaForDelivery = ucFreeQuota;
					ucFreeQuotaForNonDelivery = 0;
				}
			} else if (prStaRec->ucFreeQuotaForDelivery > 0) {
				/* Delivery is not busy */
				if (ucFreeQuota >= 3) {
					ucFreeQuotaForDelivery = 2;
					ucFreeQuotaForNonDelivery =
						ucFreeQuota -
						ucFreeQuotaForDelivery;
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
	prStaRec->ucFreeQuotaForNonDelivery =
		ucFreeQuotaForNonDelivery;

	DBGLOG(QM, LOUD,
		"new QuotaForDelivery = %d  QuotaForNonDelivery = %d\n",
		prStaRec->ucFreeQuotaForDelivery,
		prStaRec->ucFreeQuotaForNonDelivery);

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
uint32_t qmGetRxReorderQueuedBufferCount(IN struct ADAPTER *prAdapter)
{
	uint32_t i, u4Total;
	struct QUE_MGT *prQM = &prAdapter->rQM;

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
uint32_t qmDumpQueueStatus(IN struct ADAPTER *prAdapter,
	IN uint8_t *pucBuf, IN uint32_t u4Max)
{
	struct TX_CTRL *prTxCtrl;
	struct QUE_MGT *prQM;
	struct GLUE_INFO *prGlueInfo;
	uint32_t i, u4TotalBufferCount, u4TotalPageCount;
	uint32_t u4CurBufferCount, u4CurPageCount;
	uint32_t u4Len = 0;

	DEBUGFUNC(("%s", __func__));

	ASSERT(prAdapter);

	prTxCtrl = &prAdapter->rTxCtrl;
	prQM = &prAdapter->rQM;
	prGlueInfo = prAdapter->prGlueInfo;
	u4TotalBufferCount = 0;
	u4TotalPageCount = 0;
	u4CurBufferCount = 0;
	u4CurPageCount = 0;

	LOGBUF(pucBuf, u4Max, u4Len, "\n");
	LOGBUF(pucBuf, u4Max, u4Len,
		"------<Dump QUEUE Status>------\n");

	for (i = TC0_INDEX; i < TC_NUM; i++) {
		LOGBUF(pucBuf, u4Max, u4Len,
			"TC%u ResCount: Max[%02u/%03u] Free[%02u/%03u] PreUsed[%03u]\n",
			i, prTxCtrl->rTc.au4MaxNumOfBuffer[i],
			prTxCtrl->rTc.au4MaxNumOfPage[i],
			prTxCtrl->rTc.au4FreeBufferCount[i],
			prTxCtrl->rTc.au4FreePageCount[i],
			prTxCtrl->rTc.au4PreUsedPageCount[i]);

		u4TotalBufferCount += prTxCtrl->rTc.au4MaxNumOfBuffer[i];
		u4TotalPageCount += prTxCtrl->rTc.au4MaxNumOfPage[i];
		u4CurBufferCount += prTxCtrl->rTc.au4FreeBufferCount[i];
		u4CurPageCount += prTxCtrl->rTc.au4FreePageCount[i];
	}

	LOGBUF(pucBuf, u4Max, u4Len,
		"ToT ResCount: Max[%02u/%03u] Free[%02u/%03u]\n",
		u4TotalBufferCount, u4TotalPageCount, u4CurBufferCount,
		u4CurPageCount);

	u4TotalBufferCount = 0;
	u4TotalPageCount = 0;
	u4CurBufferCount = 0;
	u4CurPageCount = 0;
	LOGBUF(pucBuf, u4Max, u4Len,
		"------<Dump PLE QUEUE Status>------\n");

	for (i = TC0_INDEX; i < TC_NUM; i++) {
		LOGBUF(pucBuf, u4Max, u4Len,
			"TC%u ResCount: Max[%02u/%03u] Free[%02u/%03u] PreUsed[%03u]\n",
			i, prTxCtrl->rTc.au4MaxNumOfBuffer_PLE[i],
			prTxCtrl->rTc.au4MaxNumOfPage_PLE[i],
			prTxCtrl->rTc.au4FreeBufferCount_PLE[i],
			prTxCtrl->rTc.au4FreePageCount_PLE[i],
			prTxCtrl->rTc.au4PreUsedPageCount[i]);

		u4TotalBufferCount += prTxCtrl->rTc.au4MaxNumOfBuffer_PLE[i];
		u4TotalPageCount += prTxCtrl->rTc.au4MaxNumOfPage_PLE[i];
		u4CurBufferCount += prTxCtrl->rTc.au4FreeBufferCount_PLE[i];
		u4CurPageCount += prTxCtrl->rTc.au4FreePageCount_PLE[i];
	}

	LOGBUF(pucBuf, u4Max, u4Len,
		"ToT ResCount: Max[%02u/%03u] Free[%02u/%03u]\n",
		u4TotalBufferCount, u4TotalPageCount, u4CurBufferCount,
		u4CurPageCount);

	LOGBUF(pucBuf, u4Max, u4Len,
		"---------------------------------\n");
#if QM_ADAPTIVE_TC_RESOURCE_CTRL
	for (i = TC0_INDEX; i < TC_NUM; i++) {
		LOGBUF(pucBuf, u4Max, u4Len,
			"TC%u AvgQLen[%04u] minRsv[%02u] CurTcRes[%02u] GrtdTcRes[%02u]\n",
			i, QM_GET_TX_QUEUE_LEN(prAdapter, i),
			prQM->au4MinReservedTcResource[i],
			prQM->au4CurrentTcResource[i],
			prQM->au4GuaranteedTcResource[i]);
	}

	LOGBUF(pucBuf, u4Max, u4Len,
		"Resource Residual[%u] ExtraRsv[%u]\n",
		prQM->u4ResidualTcResource,
		prQM->u4ExtraReservedTcResource);
	LOGBUF(pucBuf, u4Max, u4Len,
		"QueLenMovingAvg[%u] Time2AdjResource[%u] Time2UpdateQLen[%u]\n",
		prQM->u4QueLenMovingAverage, prQM->u4TimeToAdjustTcResource,
		prQM->u4TimeToUpdateQueLen);
#endif

	DBGLOG(SW4, INFO, "---------------------------------\n");

#if QM_FORWARDING_FAIRNESS
	for (i = 0; i < NUM_OF_PER_STA_TX_QUEUES; i++) {
		LOGBUF(pucBuf, u4Max, u4Len,
			"TC%u HeadSta[%u] ResourceUsedCount[%u]\n",
			i, prQM->au4HeadStaRecIndex[i],
			prQM->au4ResourceUsedCount[i]);
	}
#endif

	LOGBUF(pucBuf, u4Max, u4Len,
		"BMC or unknown TxQueue Len[%u]\n",
		prQM->arTxQueue[0].u4NumElem);
	LOGBUF(pucBuf, u4Max, u4Len,
		"Pending QLen Normal[%u] Sec[%u] Cmd[%u]\n",
		GLUE_GET_REF_CNT(prGlueInfo->i4TxPendingFrameNum),
		GLUE_GET_REF_CNT(prGlueInfo->i4TxPendingSecurityFrameNum),
		GLUE_GET_REF_CNT(prGlueInfo->i4TxPendingCmdNum));

#if defined(LINUX)
	for (i = 0; i < prAdapter->ucHwBssIdNum; i++) {
		LOGBUF(pucBuf, u4Max, u4Len,
			"Pending BSS[%u] QLen[%u:%u:%u:%u]\n", i,
			prGlueInfo->ai4TxPendingFrameNumPerQueue[i][0],
			prGlueInfo->ai4TxPendingFrameNumPerQueue[i][1],
			prGlueInfo->ai4TxPendingFrameNumPerQueue[i][2],
			prGlueInfo->ai4TxPendingFrameNumPerQueue[i][3]);
	}
#endif
	LOGBUF(pucBuf, u4Max, u4Len, "Pending FWD CNT[%d]\n",
		prTxCtrl->i4PendingFwdFrameCount);
	LOGBUF(pucBuf, u4Max, u4Len, "Pending MGMT CNT[%d]\n",
		prTxCtrl->i4TxMgmtPendingNum);

	LOGBUF(pucBuf, u4Max, u4Len,
		"---------------------------------\n");

	LOGBUF(pucBuf, u4Max, u4Len, "Total RFB[%u]\n",
		CFG_RX_MAX_PKT_NUM);
	LOGBUF(pucBuf, u4Max, u4Len, "rFreeSwRfbList[%u]\n",
		prAdapter->rRxCtrl.rFreeSwRfbList.u4NumElem);
	LOGBUF(pucBuf, u4Max, u4Len, "rReceivedRfbList[%u]\n",
		prAdapter->rRxCtrl.rReceivedRfbList.u4NumElem);
	LOGBUF(pucBuf, u4Max, u4Len, "rIndicatedRfbList[%u]\n",
		prAdapter->rRxCtrl.rIndicatedRfbList.u4NumElem);
	LOGBUF(pucBuf, u4Max, u4Len, "ucNumIndPacket[%u]\n",
		prAdapter->rRxCtrl.ucNumIndPacket);
	LOGBUF(pucBuf, u4Max, u4Len, "ucNumRetainedPacket[%u]\n",
		prAdapter->rRxCtrl.ucNumRetainedPacket);

	LOGBUF(pucBuf, u4Max, u4Len,
		"---------------------------------\n");
	LOGBUF(pucBuf, u4Max, u4Len,
		"CMD: Free[%u/%u] PQ[%u] CQ[%u] TCQ[%u] TCDQ[%u]\n",
		prAdapter->rFreeCmdList.u4NumElem,
		CFG_TX_MAX_CMD_PKT_NUM,
		prAdapter->rPendingCmdQueue.u4NumElem,
		prGlueInfo->rCmdQueue.u4NumElem,
		prAdapter->rTxCmdQueue.u4NumElem,
		prAdapter->rTxCmdDoneQueue.u4NumElem);
	LOGBUF(pucBuf, u4Max, u4Len,
		"MSDU: Free[%u/%u] Pending[%u] Done[%u]\n",
		prAdapter->rTxCtrl.rFreeMsduInfoList.u4NumElem,
		CFG_TX_MAX_PKT_NUM,
		prAdapter->rTxCtrl.rTxMgmtTxingQueue.u4NumElem,
		prAdapter->rTxDataDoneQueue.u4NumElem);
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
void
mqmSendDelBaFrame(IN struct ADAPTER *prAdapter,
	IN u_int8_t fgIsInitiator, IN struct STA_RECORD *prStaRec,
	IN uint32_t u4Tid, IN uint32_t u4ReasonCode)
{

	struct MSDU_INFO *prTxMsduInfo;
	struct ACTION_DELBA_FRAME *prDelBaFrame;
	struct BSS_INFO *prBssInfo;

	DBGLOG(QM, WARN, "[Puff]: Enter mqmSendDelBaFrame()\n");

	ASSERT(prStaRec);

	/* 3 <1> Block the message in case of invalid STA */
	if (!prStaRec->fgIsInUse) {
		DBGLOG(QM, WARN,
			"[Puff][%s]: (Warning) sta_rec is not inuse\n",
			__func__);
		return;
	}
	/* Check HT-capabale STA */
	if (!(prStaRec->ucDesiredPhyTypeSet & PHY_TYPE_BIT_HT)) {
		DBGLOG(QM, WARN,
			"[Puff][%s]: (Warning) sta is NOT HT-capable(0x%08X)\n",
			__func__,
			prStaRec->ucDesiredPhyTypeSet);
		return;
	}
	/* 4 <2> Construct the DELBA frame */
	prTxMsduInfo = (struct MSDU_INFO *) cnmMgtPktAlloc(
		prAdapter, ACTION_DELBA_FRAME_LEN);

	if (!prTxMsduInfo) {
		log_dbg(QM, WARN, "[Puff][%s]: (Warning) DELBA for TID=%ld was not sent (MSDU_INFO alloc failure)\n",
			__func__, u4Tid);
		return;
	}

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
		prStaRec->ucBssIndex);

	/* Fill the Action frame */
	prDelBaFrame = (struct ACTION_DELBA_FRAME *)
		((uint32_t) (prTxMsduInfo->prPacket) +
		MAC_TX_RESERVED_FIELD);
	prDelBaFrame->u2FrameCtrl = MAC_FRAME_ACTION;
#if CFG_SUPPORT_802_11W
	if (rsnCheckBipKeyInstalled(prAdapter, prStaRec)) {
		DBGLOG(QM, WARN,
			"[Puff][%s]: (Warning) DELBA is 80211w enabled\n",
			__func__);
		prDelBaFrame->u2FrameCtrl |= MASK_FC_PROTECTED_FRAME;
	}
#endif

	prDelBaFrame->u2DurationID = 0;
	prDelBaFrame->ucCategory = CATEGORY_BLOCK_ACK_ACTION;
	prDelBaFrame->ucAction = ACTION_DELBA;

	prDelBaFrame->u2DelBaParameterSet = 0;
	prDelBaFrame->u2DelBaParameterSet |= ((fgIsInitiator ?
		ACTION_DELBA_INITIATOR_MASK : 0));
	prDelBaFrame->u2DelBaParameterSet |= ((u4Tid <<
		ACTION_DELBA_TID_OFFSET) & ACTION_DELBA_TID_MASK);
	prDelBaFrame->u2ReasonCode = u4ReasonCode;

	COPY_MAC_ADDR(prDelBaFrame->aucDestAddr,
		prStaRec->aucMacAddr);
	COPY_MAC_ADDR(prDelBaFrame->aucSrcAddr,
		prBssInfo->aucOwnMacAddr);
	COPY_MAC_ADDR(prDelBaFrame->aucBSSID, prBssInfo->aucBSSID);

	/* 4 <3> Configure the MSDU_INFO and forward it to TXM */
	TX_SET_MMPDU(prAdapter,
		prTxMsduInfo,
		prStaRec->ucBssIndex,
		(prStaRec != NULL) ? (prStaRec->ucIndex) :
		(STA_REC_INDEX_NOT_FOUND),
		WLAN_MAC_HEADER_LEN, ACTION_DELBA_FRAME_LEN, NULL,
		MSDU_RATE_MODE_AUTO);

#if CFG_SUPPORT_802_11W
	if (rsnCheckBipKeyInstalled(prAdapter, prStaRec)) {
		DBGLOG(RSN, INFO, "Set MSDU_OPT_PROTECTED_FRAME\n");
		nicTxConfigPktOption(prTxMsduInfo, MSDU_OPT_PROTECTED_FRAME,
			TRUE);
	}
#endif

	/* TID and fgIsInitiator are needed
	 * when processing TX Done of the DELBA frame
	 */
	prTxMsduInfo->ucTID = (uint8_t) u4Tid;
	prTxMsduInfo->ucControlFlag = (fgIsInitiator ? 1 : 0);

	nicTxEnqueueMsdu(prAdapter, prTxMsduInfo);

	DBGLOG(QM, WARN,
		"[Puff][%s]: Send DELBA for TID=%ld Initiator=%d\n",
		__func__, u4Tid, fgIsInitiator);
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief Callback function for the TX Done event for an ADDBA_RSP
 *
 * @param prMsduInfo The TX packet
 * @param rWlanStatus WLAN_STATUS_SUCCESS if TX is successful
 *
 * @return WLAN_STATUS_BUFFER_RETAINED is returned if the buffer shall not be
 *         freed by TXM
 */
/*----------------------------------------------------------------------------*/
uint32_t
mqmCallbackAddBaRspSent(IN struct ADAPTER *prAdapter,
	IN struct MSDU_INFO *prMsduInfo,
	IN enum ENUM_TX_RESULT_CODE rTxDoneStatus)
{
	struct RX_BA_ENTRY *prRxBaEntry;
	struct STA_RECORD *prStaRec;
	struct QUE_MGT *prQM;

	uint32_t u4Tid = 0;

	/* ASSERT(prMsduInfo); */
	prStaRec = cnmGetStaRecByIndex(prAdapter,
		prMsduInfo->ucStaRecIndex);
	ASSERT(prStaRec);

	prQM = &prAdapter->rQM;

	DBGLOG(QM, WARN,
	       "[Puff]: Enter mqmCallbackAddBaRspSent()\n");

	/* 4 <0> Check STA_REC status */
	/* Check STA_REC is inuse */
	if (!prStaRec->fgIsInUse) {
		DBGLOG(QM, WARN, "[Puff][%s]: (Warning) sta_rec is not inuse\n",
			__func__);
		return WLAN_STATUS_SUCCESS;
	}
	/* Check HT-capabale STA */
	if (!(prStaRec->ucDesiredPhyTypeSet & PHY_TYPE_BIT_HT)) {
		DBGLOG(QM, WARN,
			"[Puff][%s]: (Warning) sta is NOT HT-capable(0x%08X)\n",
			__func__,
			prStaRec->ucDesiredPhyTypeSet);
		/* To free the received ADDBA_REQ directly */
		return WLAN_STATUS_SUCCESS;
	}
	/* 4 <1> Find the corresponding BA entry */
	/* TID is stored in MSDU_INFO when composing the ADDBA_RSP frame */
	u4Tid = prMsduInfo->ucTID;
	prRxBaEntry = &prQM->arRxBaTable[u4Tid];

	/* Note: Due to some reason, for example, receiving a DELBA,
	 * the BA entry may not be in state NEGO
	 */
	/* 4 <2> INVALID state */
	if (!prRxBaEntry) {
		log_dbg(QM, WARN, "[Puff][%s]: (RX_BA) ADDBA_RSP ---> peer (STA=%d TID=%d)(TX successful)(invalid BA)\n",
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
				mqmRxModifyBaEntryStatus(prAdapter, prRxBaEntry,
					BA_ENTRY_STATUS_ACTIVE);
			break;

		/* 4 <Case 2> TX Failure */
		default:

			log_dbg(QM, WARN, "[Puff][%s]: (RX_BA) ADDBA_RSP ---> peer (STA=%d TID=%ld Entry_Status=%d)(TX failed)\n",
				__func__, prStaRec->ucIndex,
				u4Tid, prRxBaEntry->ucStatus);

			/* 4 <Case 2.1> NEGO or ACTIVE state */
			/* Notify the host to delete the agreement */
			if (prRxBaEntry->ucStatus != BA_ENTRY_STATUS_DELETING) {
				mqmRxModifyBaEntryStatus(prAdapter, prRxBaEntry,
					BA_ENTRY_STATUS_DELETING);

				/* Send DELBA to the peer to ensure
				 * the BA state is synchronized
				 */
				mqmSendDelBaFrame(prAdapter,
					DELBA_ROLE_RECIPIENT,
					prStaRec, u4Tid,
					STATUS_CODE_UNSPECIFIED_FAILURE);
			}
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
void mqmTimeoutCheckIdleRxBa(IN struct ADAPTER *prAdapter,
	IN unsigned long ulParamPtr)
{
	int8_t i;
	struct RX_BA_ENTRY *prRxBa;
	uint32_t u4IdleCountThreshold = 0;
	struct STA_RECORD *prStaRec;
	struct QUE_MGT *prQM;

	DBGLOG(QM, WARN,
		"[Puff]: Enter mqmTimeoutIdleRxBaDetection()\n");

	prQM = &prAdapter->rQM;

	/* 4 <1> Restart the timer */
	cnmTimerStopTimer(prAdapter,
		&prAdapter->rMqmIdleRxBaDetectionTimer);
	cnmTimerStartTimer(prAdapter,
		&prAdapter->rMqmIdleRxBaDetectionTimer,
		MQM_IDLE_RX_BA_CHECK_INTERVAL);

	/* 4 <2> Increment the idle count for each idle BA */
	for (i = 0; i < CFG_NUM_OF_RX_BA_AGREEMENTS; i++) {

		prRxBa = &prQM->arRxBaTable[i];

		if (prRxBa->ucStatus == BA_ENTRY_STATUS_ACTIVE) {

			prStaRec = cnmGetStaRecByIndex(prAdapter,
				prRxBa->ucStaRecIdx);

			if (!prStaRec->fgIsInUse) {
				DBGLOG(QM, WARN,
					"[Puff][%s]: (Warning) sta_rec is not inuse\n",
					__func__);
				ASSERT(0);
			}
			/* Check HT-capabale STA */
			if (!(prStaRec->ucDesiredPhyTypeSet &
				PHY_TYPE_BIT_HT)) {
				DBGLOG(QM, WARN,
					"[Puff][%s]: (Warning) sta is NOT HT-capable(0x%08X)\n",
					__func__,
					prStaRec->ucDesiredPhyTypeSet);
				ASSERT(0);
			}
			/* 4 <2.1>  Idle detected, increment idle count
			 * and see if a DELBA should be sent
			 */
			if (prRxBa->u2SnapShotSN ==
			    prStaRec->au2CachedSeqCtrl[prRxBa->ucTid]) {

				prRxBa->ucIdleCount++;

				ASSERT(prRxBa->ucTid < 8);
				switch (aucTid2ACI[prRxBa->ucTid]) {
				case 0:	/* BK */
					u4IdleCountThreshold =
						MQM_DEL_IDLE_RXBA_THRESHOLD_BK;
					break;
				case 1:	/* BE */
					u4IdleCountThreshold =
						MQM_DEL_IDLE_RXBA_THRESHOLD_BE;
					break;
				case 2:	/* VI */
					u4IdleCountThreshold =
						MQM_DEL_IDLE_RXBA_THRESHOLD_VI;
					break;
				case 3:	/* VO */
					u4IdleCountThreshold =
						MQM_DEL_IDLE_RXBA_THRESHOLD_VO;
					break;
				}

				if (prRxBa->ucIdleCount >=
					u4IdleCountThreshold) {
					mqmRxModifyBaEntryStatus(prAdapter,
						prRxBa,
						BA_ENTRY_STATUS_INVALID);
					mqmSendDelBaFrame(prAdapter,
						DELBA_ROLE_RECIPIENT, prStaRec,
						(uint32_t) prRxBa->ucTid,
						REASON_CODE_PEER_TIME_OUT);
					qmDelRxBaEntry(prAdapter,
						prStaRec->ucIndex,
						prRxBa->ucTid, TRUE);
				}
			}
			/* 4 <2.2> Activity detected */
			else {
				prRxBa->u2SnapShotSN =
					prStaRec->au2CachedSeqCtrl[
					prRxBa->ucTid];
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
void
mqmRxModifyBaEntryStatus(IN struct ADAPTER *prAdapter,
	IN struct RX_BA_ENTRY *prRxBaEntry,
	IN enum ENUM_BA_ENTRY_STATUS eStatus)
{
	struct STA_RECORD *prStaRec;
	struct QUE_MGT *prQM;

	u_int8_t fgResetScoreBoard = FALSE;

	ASSERT(prRxBaEntry);

	prStaRec = cnmGetStaRecByIndex(prAdapter,
				       prRxBaEntry->ucStaRecIdx);
	ASSERT(prStaRec);
	prQM = &prAdapter->rQM;

	if (prRxBaEntry->ucStatus == (uint8_t) eStatus) {
		DBGLOG(QM, WARN, "[Puff][%s]: eStatus are identical...\n",
			__func__, prRxBaEntry->ucStatus);
		return;
	}
	/* 4 <1> State transition from state X */
	switch (prRxBaEntry->ucStatus) {

	/* 4 <1.1> From (X = INVALID) to (ACTIVE or NEGO or DELETING) */
	case BA_ENTRY_STATUS_INVALID:

		/* Associate the BA entry with the STA_REC
		 * when leaving INVALID state
		 */
		kalMemCopy(&prQM->arRxBaTable[prRxBaEntry->ucTid],
			   prRxBaEntry, sizeof(struct RX_BA_ENTRY));

		/* Increment the RX BA counter */
		prQM->ucRxBaCount++;
		ASSERT(prQM->ucRxBaCount <= CFG_NUM_OF_RX_BA_AGREEMENTS);

		/* Since AMPDU may be received during INVALID state */
		fgResetScoreBoard = TRUE;

		/* Reset Idle Count since this BA entry is being activated now.
		 *  Note: If there is no ACTIVE entry,
		 *  the idle detection timer will not be started.
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
		kalMemZero(&prQM->arRxBaTable[prRxBaEntry->ucTid],
			sizeof(struct RX_BA_ENTRY));

		/* Decrement the RX BA counter */
		prQM->ucRxBaCount--;
		ASSERT(prQM->ucRxBaCount < CFG_NUM_OF_RX_BA_AGREEMENTS);

		/* (TBC) */
		fgResetScoreBoard = TRUE;

		/* If there is not any BA agreement,
		 * stop doing idle detection
		 */
		if (prQM->ucRxBaCount == 0) {
			if (MQM_CHECK_FLAG(prAdapter->u4FlagBitmap,
				MQM_FLAG_IDLE_RX_BA_TIMER_STARTED)) {
				cnmTimerStopTimer(prAdapter,
					&prAdapter->rMqmIdleRxBaDetectionTimer);
				MQM_CLEAR_FLAG(prAdapter->u4FlagBitmap,
					MQM_FLAG_IDLE_RX_BA_TIMER_STARTED);
			}
		}

		break;

	/* 4 <2.2> From  (any) to (Y=ACTIVE) */
	case BA_ENTRY_STATUS_ACTIVE:

		/* If there is at least one BA going into ACTIVE,
		 * start idle detection
		 */
		if (!MQM_CHECK_FLAG(prAdapter->u4FlagBitmap,
			MQM_FLAG_IDLE_RX_BA_TIMER_STARTED)) {
			cnmTimerInitTimer(prAdapter,
				&prAdapter->rMqmIdleRxBaDetectionTimer,
				(PFN_MGMT_TIMEOUT_FUNC) mqmTimeoutCheckIdleRxBa,
				(unsigned long) NULL);
			/* No parameter */

			cnmTimerStopTimer(prAdapter,
				&prAdapter->rMqmIdleRxBaDetectionTimer);

#if MQM_IDLE_RX_BA_DETECTION
			cnmTimerStartTimer(prAdapter,
				&prAdapter->rMqmIdleRxBaDetectionTimer,
				MQM_IDLE_RX_BA_CHECK_INTERVAL);
			MQM_SET_FLAG(prAdapter->u4FlagBitmap,
				MQM_FLAG_IDLE_RX_BA_TIMER_STARTED);
#endif
		}

		break;

	case BA_ENTRY_STATUS_NEGO:
	default:
		break;
	}

	if (fgResetScoreBoard) {
		struct CMD_RESET_BA_SCOREBOARD *prCmdBody;

		prCmdBody = (struct CMD_RESET_BA_SCOREBOARD *)
			cnmMemAlloc(prAdapter, RAM_TYPE_BUF,
			sizeof(struct CMD_RESET_BA_SCOREBOARD));
		ASSERT(prCmdBody);

		prCmdBody->ucflag = MAC_ADDR_TID_MATCH;
		prCmdBody->ucTID = prRxBaEntry->ucTid;
		kalMemCopy(prCmdBody->aucMacAddr, prStaRec->aucMacAddr,
			PARAM_MAC_ADDR_LEN);

		wlanoidResetBAScoreboard(prAdapter, prCmdBody,
			sizeof(struct CMD_RESET_BA_SCOREBOARD));

		cnmMemFree(prAdapter, prCmdBody);
	}

	DBGLOG(QM, WARN,
		"[Puff]QM: (RX_BA) [STA=%d TID=%d] status from %d to %d\n",
		prRxBaEntry->ucStaRecIdx, prRxBaEntry->ucTid,
		prRxBaEntry->ucStatus, eStatus);

	prRxBaEntry->ucStatus = (uint8_t) eStatus;

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
void mqmHandleAddBaReq(IN struct ADAPTER *prAdapter,
	IN struct SW_RFB *prSwRfb)
{
	struct STA_RECORD *prStaRec;
	struct BSS_INFO *prBssInfo;
	struct ACTION_ADDBA_REQ_FRAME *prAddBaReq;
	struct ACTION_ADDBA_REQ_BODY rAddBaReqBody;
	struct ACTION_ADDBA_RSP_FRAME *prAddBaRsp;
	struct ACTION_ADDBA_RSP_BODY rAddBaRspBody;
	struct RX_BA_ENTRY *prRxBaEntry;
	struct MSDU_INFO *prTxMsduInfo;
	struct QUE_MGT *prQM;

	/* Reject or accept the ADDBA_REQ */
	u_int8_t fgIsReqAccepted = TRUE;
	/* Indicator: Whether a new RX BA entry will be added */
	u_int8_t fgIsNewEntryAdded = FALSE;

	uint32_t u4Tid;
	uint32_t u4StaRecIdx;
	uint16_t u2WinStart;
	uint16_t u2WinSize;
	uint32_t u4BuffSize;

#if CFG_SUPPORT_BCM
	uint32_t u4BuffSizeBT;
#endif

	ASSERT(prSwRfb);

	prStaRec = prSwRfb->prStaRec;
	prQM = &prAdapter->rQM;

	do {

		/* 4 <0> Check if this is an active HT-capable STA */
		/* Check STA_REC is inuse */
		if (!prStaRec->fgIsInUse) {
			log_dbg(QM, WARN, "[Puff][%s]: (Warning) sta_rec is not inuse\n",
				__func__);
			break;
		}
		/* Check HT-capabale STA */
		if (!(prStaRec->ucDesiredPhyTypeSet & PHY_TYPE_BIT_HT)) {
			DBGLOG(QM, WARN,
				"[Puff][%s]: (Warning) sta is NOT HT-capable(0x%08X)\n",
				__func__,
				prStaRec->ucDesiredPhyTypeSet);
			break;	/* To free the received ADDBA_REQ directly */
		}
		/* 4 <1> Check user configurations and HW capabilities */
		/* Check configurations (QoS support, AMPDU RX support) */
		if ((!prAdapter->rWifiVar.fgSupportQoS) ||
		    (!prAdapter->rWifiVar.fgSupportAmpduRx) ||
		    (!prStaRec->fgRxAmpduEn)) {
			DBGLOG(QM, WARN,
				"[Puff][%s]: (Warning) BA ACK Policy not supported fgSupportQoS(%d)",
				__func__, prAdapter->rWifiVar.fgSupportQoS);
			DBGLOG(QM, WARN,
				"fgSupportAmpduRx(%d), fgRxAmpduEn(%d)\n",
				prAdapter->rWifiVar.fgSupportAmpduRx,
				prStaRec->fgRxAmpduEn);
			/* Will send an ADDBA_RSP with DECLINED */
			fgIsReqAccepted = FALSE;
		}
		/* Check capability */
		prAddBaReq = ((struct ACTION_ADDBA_REQ_FRAME *) (
			prSwRfb->pvHeader));
		kalMemCopy((uint8_t *) (&rAddBaReqBody),
			(uint8_t *) (&(prAddBaReq->aucBAParameterSet[0])),
			6);
		if ((((rAddBaReqBody.u2BAParameterSet) &
		      BA_PARAM_SET_ACK_POLICY_MASK) >>
		     BA_PARAM_SET_ACK_POLICY_MASK_OFFSET)
		    != BA_PARAM_SET_ACK_POLICY_IMMEDIATE_BA) {
		  /* Only Immediate_BA is supported */
			DBGLOG(QM, WARN,
				"[Puff][%s]: (Warning) BA ACK Policy not supported (0x%08X)\n",
				__func__, rAddBaReqBody.u2BAParameterSet);
			/* Will send an ADDBA_RSP with DECLINED */
			fgIsReqAccepted = FALSE;
		}

		/* 4 <2> Determine the RX BA entry (existing or to be added) */
		/* Note: BA entry index = (TID, STA_REC index) */
		u4Tid = (((rAddBaReqBody.u2BAParameterSet) &
			  BA_PARAM_SET_TID_MASK) >>
			  BA_PARAM_SET_TID_MASK_OFFSET);
		u4StaRecIdx = prStaRec->ucIndex;
		DBGLOG(QM, WARN,
			"[Puff][%s]: BA entry index = [TID(%d), STA_REC index(%d)]\n",
			__func__, u4Tid, u4StaRecIdx);

		u2WinStart = ((rAddBaReqBody.u2BAStartSeqCtrl) >>
			OFFSET_BAR_SSC_SN);
		u2WinSize = (((rAddBaReqBody.u2BAParameterSet) &
			BA_PARAM_SET_BUFFER_SIZE_MASK) >>
			BA_PARAM_SET_BUFFER_SIZE_MASK_OFFSET);
		DBGLOG(QM, WARN,
			"[Puff][%s]: BA entry info = [WinStart(%d), WinSize(%d)]\n",
			__func__, u2WinStart, u2WinSize);

		if (fgIsReqAccepted) {

			prRxBaEntry = &prQM->arRxBaTable[u4Tid];

			if (!prRxBaEntry) {

				/* 4 <Case 2.1> INVALID state && BA entry
				 *   available --> Add a new entry and accept
				 */
				if (prQM->ucRxBaCount <
					CFG_NUM_OF_RX_BA_AGREEMENTS) {

					fgIsNewEntryAdded =
						qmAddRxBaEntry(prAdapter,
						(uint8_t) u4StaRecIdx,
						(uint8_t) u4Tid, u2WinStart,
						u2WinSize);

					if (!fgIsNewEntryAdded) {
						DBGLOG(QM, ERROR,
							"[Puff][%s]: (Error) Free RX BA entry alloc failure\n");
						fgIsReqAccepted = FALSE;
					} else {
						log_dbg(QM, WARN, "[Puff][%s]: Create a new BA Entry\n");
					}
				}
				/* 4 <Case 2.2> INVALID state && BA entry
				 *   unavailable --> Reject the ADDBA_REQ
				 */
				else {
					log_dbg(QM, WARN, "[Puff][%s]: (Warning) Free RX BA entry unavailable(req: %d)\n",
						__func__, prQM->ucRxBaCount);
					/* Will send ADDBA_RSP with DECLINED */
					fgIsReqAccepted = FALSE;
				}
			} else {

				/* 4 <Case 2.3> NEGO or DELETING  state -->
				 * Ignore the ADDBA_REQ
				 * For NEGO: do nothing. Wait for TX Done of
				 * ADDBA_RSP
				 * For DELETING: do nothing. Wait for TX Done
				 * of DELBA
				 */
				if (prRxBaEntry->ucStatus !=
					BA_ENTRY_STATUS_ACTIVE) {
					/* Ignore the ADDBA_REQ since
					 * the current state is NEGO
					 */
					log_dbg(QM, WARN, "[Puff][%s]:(Warning)ADDBA_REQ for TID=%ld is received, status:%d)\n",
						__func__, u4Tid,
						prRxBaEntry->ucStatus);
					break;
				}
			}
		}
		/* 4 <3> Construct the ADDBA_RSP frame */
		prTxMsduInfo = (struct MSDU_INFO *) cnmMgtPktAlloc(
				       prAdapter, ACTION_ADDBA_RSP_FRAME_LEN);
		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
						  prStaRec->ucBssIndex);

		if (!prTxMsduInfo) {

			/* The peer may send an ADDBA_REQ message later.
			 *  Do nothing to the BA entry. No DELBA will be
			 *  sent (because cnmMgtPktAlloc() may fail again).
			 *  No BA deletion event will be sent to the host
			 *  (because cnmMgtPktAlloc() may fail again).
			 */
			DBGLOG(QM, WARN,
				"[Puff][%s]: (Warning) ADDBA_RSP alloc failure\n",
				__func__);

			if (fgIsNewEntryAdded) {
				/* If a new entry has been created due
				 * to this ADDBA_REQ, delete it
				 */
				ASSERT(prRxBaEntry);
				mqmRxModifyBaEntryStatus(prAdapter,
					prRxBaEntry, BA_ENTRY_STATUS_INVALID);
			}

			break;	/* Exit directly to free the ADDBA_REQ */
		}

		/* Fill the ADDBA_RSP message */
		prAddBaRsp = (struct ACTION_ADDBA_RSP_FRAME *)
			((uint32_t) (prTxMsduInfo->prPacket) +
				MAC_TX_RESERVED_FIELD);
		prAddBaRsp->u2FrameCtrl = MAC_FRAME_ACTION;

#if CFG_SUPPORT_802_11W
		if (rsnCheckBipKeyInstalled(prAdapter, prStaRec)) {
			DBGLOG(QM, WARN,
				"[Puff][%s]: (Warning) ADDBA_RSP is 80211w enabled\n",
				__func__);
			prAddBaReq->u2FrameCtrl |= MASK_FC_PROTECTED_FRAME;
		}
#endif
		prAddBaRsp->u2DurationID = 0;
		prAddBaRsp->ucCategory = CATEGORY_BLOCK_ACK_ACTION;
		prAddBaRsp->ucAction = ACTION_ADDBA_RSP;
		prAddBaRsp->ucDialogToken = prAddBaReq->ucDialogToken;

		log_dbg(QM, WARN, "[Puff][%s]: (Warning) ADDBA_RSP DurationID(%d) Category(%d) Action(%d) DialogToken(%d)\n",
			__func__, prAddBaRsp->u2DurationID,
			prAddBaRsp->ucCategory, prAddBaRsp->ucAction,
			prAddBaRsp->ucDialogToken);

		if (fgIsReqAccepted)
			rAddBaRspBody.u2StatusCode = STATUS_CODE_SUCCESSFUL;
		else
			rAddBaRspBody.u2StatusCode = STATUS_CODE_REQ_DECLINED;

		/* WinSize = min(WinSize in ADDBA_REQ, CFG_RX_BA_MAX_WINSIZE) */
		u4BuffSize = (((rAddBaReqBody.u2BAParameterSet) &
			BA_PARAM_SET_BUFFER_SIZE_MASK) >>
			BA_PARAM_SET_BUFFER_SIZE_MASK_OFFSET);

		/*If ADDBA req WinSize<=0 => use default WinSize(16) */
		if ((u4BuffSize > CFG_RX_BA_MAX_WINSIZE)
		    || (u4BuffSize <= 0))
			u4BuffSize = CFG_RX_BA_MAX_WINSIZE;
#if CFG_SUPPORT_BCM
		/* TODO: Call BT coexistence function to limit the winsize */
		u4BuffSizeBT = bcmRequestBaWinSize();
		DBGLOG(QM, WARN,
			"[Puff][%s]: (Warning) bcmRequestBaWinSize(%d)\n",
			__func__, u4BuffSizeBT);

		if (u4BuffSize > u4BuffSizeBT)
			u4BuffSize = u4BuffSizeBT;
#endif /* CFG_SUPPORT_BCM */

		rAddBaRspBody.u2BAParameterSet = (BA_POLICY_IMMEDIATE |
			(u4Tid << BA_PARAM_SET_TID_MASK_OFFSET) |
			(u4BuffSize << BA_PARAM_SET_BUFFER_SIZE_MASK_OFFSET));

		/* TODO: Determine the BA timeout value
		 * according to the default preference
		 */
		rAddBaRspBody.u2BATimeoutValue =
			rAddBaReqBody.u2BATimeoutValue;

		DBGLOG(QM, WARN,
			"[Puff][%s]: (Warning) ADDBA_RSP u4BuffSize(%d) StatusCode(%d)",
			__func__, u4BuffSize, rAddBaRspBody.u2StatusCode);
		DBGLOG(QM, WARN,
			"BAParameterSet(0x%08X) BATimeoutValue(%d)\n",
			rAddBaRspBody.u2BAParameterSet,
			rAddBaRspBody.u2BATimeoutValue);
		kalMemCopy((uint8_t *) (&(prAddBaRsp->aucStatusCode[0])),
			(uint8_t *) (&rAddBaRspBody), 6);

		COPY_MAC_ADDR(prAddBaRsp->aucDestAddr,
			prStaRec->aucMacAddr);
		COPY_MAC_ADDR(prAddBaRsp->aucSrcAddr,
			prBssInfo->aucOwnMacAddr);
		COPY_MAC_ADDR(prAddBaRsp->aucBSSID, prAddBaReq->aucBSSID);

		/* 4 <4> Forward the ADDBA_RSP to TXM */
		TX_SET_MMPDU(prAdapter,
			prTxMsduInfo,
			prStaRec->ucBssIndex,
			(prStaRec != NULL) ? (prStaRec->ucIndex) :
			(STA_REC_INDEX_NOT_FOUND),
			WLAN_MAC_HEADER_LEN,
			ACTION_ADDBA_RSP_FRAME_LEN,
			mqmCallbackAddBaRspSent,
			MSDU_RATE_MODE_AUTO);

#if CFG_SUPPORT_802_11W
		if (rsnCheckBipKeyInstalled(prAdapter, prStaRec)) {
			DBGLOG(RSN, INFO, "Set MSDU_OPT_PROTECTED_FRAME\n");
			nicTxConfigPktOption(prTxMsduInfo,
				MSDU_OPT_PROTECTED_FRAME, TRUE);
		}
#endif

		/* Note: prTxMsduInfo->ucTID is not used for transmitting the
		 * ADDBA_RSP. However, when processing TX Done of this
		 * ADDBA_RSP, the TID value is needed, so store the TID value
		 * in advance to prevent parsing the ADDBA_RSP frame
		 */
		prTxMsduInfo->ucTID = (uint8_t) u4Tid;

		nicTxEnqueueMsdu(prAdapter, prTxMsduInfo);

		DBGLOG(QM, WARN,
			"[Puff][%s]: (RX_BA) ADDBA_RSP ---> peer (STA=%d TID=%ld)\n",
			__func__,
			prStaRec->ucIndex, u4Tid);

#if 0
		/* 4 <5> Notify the host to start buffer reordering */
		/* Only when a new BA entry is indeed
		 * added will the host be notified
		 */
		if (fgIsNewEntryAdded) {
			ASSERT(fgIsReqAccepted);

			prSwRfbEventToHost = (struct SW_RFB *) cnmMgtPktAlloc(
				EVENT_RX_ADDBA_PACKET_LEN);

			if (!prSwRfbEventToHost) {

				/* Note: DELBA will not be sent since
				 * cnmMgtPktAlloc() may fail again. However,
				 * it does not matter because upon receipt of
				 * AMPDUs without a RX BA agreement,
				 * MQM will send DELBA frames
				 */

				DBGLOG(MQM, WARN,
					"MQM: (Warning) EVENT packet alloc failed\n");

				/* Ensure that host and FW are synchronized */
				mqmRxModifyBaEntryStatus(prRxBaEntry,
					BA_ENTRY_STATUS_INVALID);

				break;	/* Free the received ADDBA_REQ */
			}
			prEventRxAddBa = (struct EVENT_RX_ADDBA *)
					 prSwRfbEventToHost->pucBuffer;
			prEventRxAddBa->ucStaRecIdx = (uint8_t) u4StaRecIdx;
			prEventRxAddBa->u2Length = EVENT_RX_ADDBA_PACKET_LEN;
			prEventRxAddBa->ucEID = EVENT_ID_RX_ADDBA;
			/* Unsolicited event packet */
			prEventRxAddBa->ucSeqNum = 0;
			prEventRxAddBa->u2BAParameterSet =
				rAddBaRspBody.u2BAParameterSet;
			prEventRxAddBa->u2BAStartSeqCtrl =
				rAddBaReqBody.u2BAStartSeqCtrl;
			prEventRxAddBa->u2BATimeoutValue =
				rAddBaReqBody.u2BATimeoutValue;
			prEventRxAddBa->ucDialogToken =
				prAddBaReq->ucDialogToken;

			log_dbg(MQM, INFO, "MQM: (RX_BA) Event ADDBA ---> driver (STA=%ld TID=%ld WinStart=%d)\n",
				u4StaRecIdx, u4Tid,
				(prEventRxAddBa->u2BAStartSeqCtrl >> 4));

			/* Configure the SW_RFB for the Event packet */
			RXM_SET_EVENT_PACKET(
				/* struct SW_RFB **/ (struct SW_RFB *)
				prSwRfbEventToHost,
				/* HIF RX Packet pointer */
				(uint8_t *) prEventRxAddBa,
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
void mqmHandleAddBaRsp(IN struct SW_RFB *prSwRfb)
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
void mqmHandleDelBa(IN struct SW_RFB *prSwRfb)
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
void mqmHandleBaActionFrame(IN struct ADAPTER *prAdapter,
	IN struct SW_RFB *prSwRfb)
{
	struct WLAN_ACTION_FRAME *prRxFrame;

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prRxFrame = (struct WLAN_ACTION_FRAME *) prSwRfb->pvHeader;
	DBGLOG(RLM, WARN, "[Puff][%s] Action(%d)\n", __func__,
		prRxFrame->ucAction);

	switch (prRxFrame->ucAction) {

	case ACTION_ADDBA_REQ:
		DBGLOG(RLM, WARN,
			"[Puff][%s] (RX_BA) ADDBA_REQ <--- peer\n", __func__);
		mqmHandleAddBaReq(prAdapter, prSwRfb);
		break;

	case ACTION_ADDBA_RSP:
		DBGLOG(RLM, WARN,
			"[Puff][%s] (RX_BA) ADDBA_RSP <--- peer\n", __func__);
		mqmHandleAddBaRsp(prSwRfb);
		break;

	case ACTION_DELBA:
		DBGLOG(RLM, WARN, "[Puff][%s] (RX_BA) DELBA <--- peer\n",
			__func__);
		mqmHandleDelBa(prSwRfb);
		break;

	default:
		DBGLOG(RLM, WARN, "[Puff][%s] Unknown BA Action Frame\n",
			__func__);
		break;
	}

}

#endif

#if ARP_MONITER_ENABLE
void qmDetectArpNoResponse(struct ADAPTER *prAdapter,
	struct MSDU_INFO *prMsduInfo)
{
	struct sk_buff *prSkb = NULL;
	uint8_t *pucData = NULL;
	uint16_t u2EtherType = 0;
	int arpOpCode = 0;

	if (!prAdapter)
		return;

	prSkb = (struct sk_buff *)prMsduInfo->prPacket;

	if (!prSkb || (prSkb->len <= ETHER_HEADER_LEN))
		return;

	pucData = prSkb->data;
	if (!pucData)
		return;
	u2EtherType = (pucData[ETH_TYPE_LEN_OFFSET] << 8) |
		(pucData[ETH_TYPE_LEN_OFFSET + 1]);

	if (u2EtherType != ETH_P_ARP)
		return;

	/* If ARP req is neither to apIp nor to gatewayIp, ignore detection */
	if (kalMemCmp(apIp, &pucData[ETH_TYPE_LEN_OFFSET + 26],
		sizeof(apIp)) &&
		kalMemCmp(gatewayIp, &pucData[ETH_TYPE_LEN_OFFSET + 26],
		sizeof(gatewayIp)))
		return;

	arpOpCode = (pucData[ETH_TYPE_LEN_OFFSET + 8] << 8) |
		(pucData[ETH_TYPE_LEN_OFFSET + 8 + 1]);

	if (arpOpCode == ARP_PRO_REQ) {
		arpMoniter++;
		if (arpMoniter > 20) {
			DBGLOG(INIT, WARN,
				"IOT Critical issue, arp no resp, check AP!\n");
			if (prAdapter->prAisBssInfo)
				prAdapter->prAisBssInfo->u2DeauthReason =
					BEACON_TIMEOUT_DUE_2_APR_NO_RESPONSE;
			aisBssBeaconTimeout(prAdapter);
			arpMoniter = 0;
			kalMemZero(apIp, sizeof(apIp));
		}
	}
}

void qmHandleRxArpPackets(struct ADAPTER *prAdapter,
	struct SW_RFB *prSwRfb)
{
	uint8_t *pucData = NULL;
	uint16_t u2EtherType = 0;
	int arpOpCode = 0;

	if (prSwRfb->u2PacketLen <= ETHER_HEADER_LEN)
		return;

	pucData = (uint8_t *)prSwRfb->pvHeader;
	if (!pucData)
		return;
	u2EtherType = (pucData[ETH_TYPE_LEN_OFFSET] << 8) |
		(pucData[ETH_TYPE_LEN_OFFSET + 1]);

	if (u2EtherType != ETH_P_ARP)
		return;

	arpOpCode = (pucData[ETH_TYPE_LEN_OFFSET + 8] << 8) |
		(pucData[ETH_TYPE_LEN_OFFSET + 8 + 1]);
	if (arpOpCode == ARP_PRO_RSP) {
		arpMoniter = 0;
		if (prAdapter->prAisBssInfo &&
			prAdapter->prAisBssInfo->prStaRecOfAP) {
			if (EQUAL_MAC_ADDR(
				&(pucData[ETH_TYPE_LEN_OFFSET + 10]),
				/* source hardware address */
				prAdapter->prAisBssInfo->
				prStaRecOfAP->aucMacAddr)) {
				kalMemCopy(apIp,
					&(pucData[ETH_TYPE_LEN_OFFSET + 16]),
					sizeof(apIp));
				DBGLOG(INIT, TRACE,
					"get arp response from AP %d.%d.%d.%d\n",
					apIp[0], apIp[1], apIp[2], apIp[3]);
			}
		}
	}
}

void qmHandleRxDhcpPackets(struct ADAPTER *prAdapter,
	struct SW_RFB *prSwRfb)
{
	uint8_t *pucData = NULL;
	uint8_t *pucEthBody = NULL;
	uint8_t *pucUdpBody = NULL;
	uint32_t udpLength = 0;
	uint32_t i = 0;
	struct BOOTP_PROTOCOL *prBootp = NULL;
	uint32_t u4DhcpMagicCode = 0;
	uint8_t dhcpTypeGot = 0;
	uint8_t dhcpGatewayGot = 0;

	if (prSwRfb->u2PacketLen <= ETHER_HEADER_LEN)
		return;

	pucData = (uint8_t *)prSwRfb->pvHeader;
	if (!pucData)
		return;
	if (((pucData[ETH_TYPE_LEN_OFFSET] << 8) |
		pucData[ETH_TYPE_LEN_OFFSET + 1]) != ETH_P_IPV4)
		return;

	pucEthBody = &pucData[ETH_HLEN];
	if (((pucEthBody[0] & IPVH_VERSION_MASK) >>
		IPVH_VERSION_OFFSET) != IPVERSION)
		return;
	if (pucEthBody[9] != IP_PRO_UDP)
		return;

	pucUdpBody = &pucEthBody[(pucEthBody[0] & 0x0F) * 4];
	if ((pucUdpBody[0] << 8 | pucUdpBody[1]) != UDP_PORT_DHCPS ||
		(pucUdpBody[2] << 8 | pucUdpBody[3]) != UDP_PORT_DHCPC)
		return;

	udpLength = pucUdpBody[4] << 8 | pucUdpBody[5];

	prBootp = (struct BOOTP_PROTOCOL *) &pucUdpBody[8];

	WLAN_GET_FIELD_BE32(&prBootp->aucOptions[0],
		&u4DhcpMagicCode);
	if (u4DhcpMagicCode != DHCP_MAGIC_NUMBER) {
		DBGLOG(INIT, WARN,
			"dhcp wrong magic number, magic code: %d\n",
			u4DhcpMagicCode);
		return;
	}

	/* 1. 248 is from udp header to the beginning of dhcp option
	 * 2. not sure the dhcp option always usd 255 as a end mark?
	 *    if so, while condition should be removed?
	 */
	while (i < udpLength - 248) {
		/* bcz of the strange struct BOOTP_PROTOCOL *,
		 * the dhcp magic code was count in dhcp options
		 * so need to [i + 4] to skip it
		 */
		switch (prBootp->aucOptions[i + 4]) {
		case 3:
			/* both dhcp ack and offer will update it */
			if (prBootp->aucOptions[i + 6] ||
				prBootp->aucOptions[i + 7] ||
				prBootp->aucOptions[i + 8] ||
				prBootp->aucOptions[i + 9]) {
				gatewayIp[0] = prBootp->aucOptions[i + 6];
				gatewayIp[1] = prBootp->aucOptions[i + 7];
				gatewayIp[2] = prBootp->aucOptions[i + 8];
				gatewayIp[3] = prBootp->aucOptions[i + 9];

				DBGLOG(INIT, TRACE, "Gateway ip: %d.%d.%d.%d\n",
					gatewayIp[0],
					gatewayIp[1],
					gatewayIp[2],
					gatewayIp[3]);
			};
			dhcpGatewayGot = 1;
			break;
		case 53:
			if (prBootp->aucOptions[i + 6] != 0x02
			    && prBootp->aucOptions[i + 6] != 0x05) {
				DBGLOG(INIT, WARN,
					"wrong dhcp message type, type: %d\n",
				  prBootp->aucOptions[i + 6]);
				if (dhcpGatewayGot)
					kalMemZero(gatewayIp,
						sizeof(gatewayIp));
				return;
			}
			dhcpTypeGot = 1;
			break;
		case 255:
			return;

		default:
			break;
		}
		if (dhcpGatewayGot && dhcpTypeGot)
			return;

		i += prBootp->aucOptions[i + 5] + 2;
	}
	DBGLOG(INIT, WARN,
	       "can't find the dhcp option 255?, need to check the net log\n");
}

void qmResetArpDetect(void)
{
	arpMoniter = 0;
	kalMemZero(apIp, sizeof(apIp));
	kalMemZero(gatewayIp, sizeof(gatewayIp));
}
#endif

#if QM_ADAPTIVE_TC_RESOURCE_CTRL
void qmResetTcControlResource(IN struct ADAPTER *prAdapter)
{
	uint32_t u4Idx;
	uint32_t u4TotalMinReservedTcResource = 0;
	uint32_t u4TotalTcResource = 0;
	uint32_t u4TotalGurantedTcResource = 0;
	struct QUE_MGT *prQM = &prAdapter->rQM;

	/* Initialize TC resource control variables */
	for (u4Idx = 0; u4Idx < TC_NUM; u4Idx++)
		prQM->au4AverageQueLen[u4Idx] = 0;

	ASSERT(prQM->u4TimeToAdjustTcResource
	       && prQM->u4TimeToUpdateQueLen);

	for (u4Idx = 0; u4Idx < TC_NUM; u4Idx++) {
		prQM->au4CurrentTcResource[u4Idx] =
			prAdapter->rTxCtrl.rTc.au4MaxNumOfBuffer[u4Idx];

		if (u4Idx != TC4_INDEX) {
			u4TotalTcResource += prQM->au4CurrentTcResource[u4Idx];
			u4TotalGurantedTcResource +=
				prQM->au4GuaranteedTcResource[u4Idx];
			u4TotalMinReservedTcResource +=
				prQM->au4MinReservedTcResource[u4Idx];
		}
	}

	/* Sanity Check */
	if (u4TotalMinReservedTcResource > u4TotalTcResource)
		kalMemZero(prQM->au4MinReservedTcResource,
			   sizeof(prQM->au4MinReservedTcResource));

	if (u4TotalGurantedTcResource > u4TotalTcResource)
		kalMemZero(prQM->au4GuaranteedTcResource,
			   sizeof(prQM->au4GuaranteedTcResource));

	u4TotalGurantedTcResource = 0;

	/* Initialize Residual TC resource */
	for (u4Idx = 0; u4Idx < TC_NUM; u4Idx++) {
		if (prQM->au4GuaranteedTcResource[u4Idx] <
		    prQM->au4MinReservedTcResource[u4Idx])
			prQM->au4GuaranteedTcResource[u4Idx] =
				prQM->au4MinReservedTcResource[u4Idx];

		if (u4Idx != TC4_INDEX)
			u4TotalGurantedTcResource +=
				prQM->au4GuaranteedTcResource[u4Idx];
	}

	prQM->u4ResidualTcResource = u4TotalTcResource -
		u4TotalGurantedTcResource;

}
#endif

#if CFG_SUPPORT_REPLAY_DETECTION
/* To change PN number to UINT64 */
#define CCMPTSCPNNUM	6
u_int8_t qmRxPNtoU64(uint8_t *pucPN, uint8_t uPNNum,
	uint64_t *pu64Rets)
{
	uint8_t ucCount = 0;
	uint64_t u64Data = 0;

	if (!pu64Rets) {
		DBGLOG(QM, ERROR, "Please input valid pu8Rets\n");
		return FALSE;
	}

	if (uPNNum > CCMPTSCPNNUM) {
		DBGLOG(QM, ERROR, "Please input valid uPNNum:%d\n", uPNNum);
		return FALSE;
	}

	*pu64Rets = 0;
	for (; ucCount < uPNNum; ucCount++) {
		u64Data = ((uint64_t) pucPN[ucCount]) << (8 * ucCount);
		*pu64Rets +=  u64Data;
	}
	return TRUE;
}

/* To check PN/TSC between RxStatus and local record.
 * return TRUE if PNS is not bigger than PNT
 */
u_int8_t qmRxDetectReplay(uint8_t *pucPNS, uint8_t *pucPNT)
{
	uint64_t u8RxNum = 0;
	uint64_t u8LocalRec = 0;

	if (!pucPNS || !pucPNT) {
		DBGLOG(QM, ERROR, "Please input valid PNS:%p and PNT:%p\n",
			pucPNS, pucPNT);
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
u_int8_t qmHandleRxReplay(struct ADAPTER *prAdapter,
			  struct SW_RFB *prSwRfb)
{
	uint8_t *pucPN = NULL;
	uint8_t ucKeyID = 0;				/* 0~4 */
	/* CIPHER_SUITE_NONE~CIPHER_SUITE_GCMP */
	uint8_t ucSecMode = CIPHER_SUITE_NONE;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct GL_WPA_INFO *prWpaInfo = NULL;
	struct GL_DETECT_REPLAY_INFO *prDetRplyInfo = NULL;
	struct HW_MAC_RX_DESC *prRxStatus = NULL;
	uint8_t ucCheckZeroPN;
	uint8_t i;

	if (!prAdapter)
		return TRUE;
	if (prSwRfb->u2PacketLen <= ETHER_HEADER_LEN)
		return TRUE;

	if (!(prSwRfb->ucGroupVLD & BIT(RX_GROUP_VLD_1))) {
		DBGLOG_LIMITED(QM, TRACE, "Group 1 invalid\n");
		return FALSE;
	}

	/* BMC only need check CCMP and TKIP Cipher suite */
	prRxStatus = prSwRfb->prRxStatus;
	ucSecMode = HAL_RX_STATUS_GET_SEC_MODE(prRxStatus);

	prGlueInfo = prAdapter->prGlueInfo;
	prWpaInfo = &prGlueInfo->rWpaInfo;

	DBGLOG_LIMITED(QM, TRACE, "ucSecMode = [%u], ChiperGroup = [%u]\n",
			ucSecMode, prWpaInfo->u4CipherGroup);

	if (ucSecMode != CIPHER_SUITE_CCMP
	    && ucSecMode != CIPHER_SUITE_TKIP) {
		DBGLOG_LIMITED(QM, TRACE,
			"SecMode: %d and CipherGroup: %d, no need check replay\n",
			ucSecMode, prWpaInfo->u4CipherGroup);
		return FALSE;
	}

	ucKeyID = HAL_RX_STATUS_GET_KEY_ID(prRxStatus);
	if (ucKeyID >= MAX_KEY_NUM) {
		DBGLOG(QM, ERROR, "KeyID: %d error\n", ucKeyID);
		return TRUE;
	}

	prDetRplyInfo = &prGlueInfo->prDetRplyInfo;

#if 0
	if (prDetRplyInfo->arReplayPNInfo[ucKeyID].fgFirstPkt) {
		prDetRplyInfo->arReplayPNInfo[ucKeyID].fgFirstPkt = FALSE;
		HAL_RX_STATUS_GET_PN(prSwRfb->prRxStatusGroup1,
			prDetRplyInfo->arReplayPNInfo[ucKeyID].auPN);
		DBGLOG(QM, INFO,
			"First check packet. Key ID:0x%x\n", ucKeyID);
		return FALSE;
	}
#endif

	pucPN = prSwRfb->prRxStatusGroup1->aucPN;
	DBGLOG_LIMITED(QM, TRACE,
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

	if (qmRxDetectReplay(pucPN,
			prDetRplyInfo->arReplayPNInfo[ucKeyID].auPN)) {
		DBGLOG_LIMITED(QM, WARN, "Drop BC replay packet!\n");
		return TRUE;
	}

	HAL_RX_STATUS_GET_PN(prSwRfb->prRxStatusGroup1,
		prDetRplyInfo->arReplayPNInfo[ucKeyID].auPN);
	return FALSE;
}
#endif

#if CFG_SUPPORT_LOWLATENCY_MODE || CFG_SUPPORT_OSHARE
u_int8_t
qmIsNoDropPacket(IN struct ADAPTER *prAdapter, IN struct SW_RFB *prSwRfb)
{
	uint8_t *pucData = (uint8_t *) prSwRfb->pvHeader;
	uint16_t u2Etype = (pucData[ETH_TYPE_LEN_OFFSET] << 8)
		| (pucData[ETH_TYPE_LEN_OFFSET + 1]);
	uint8_t ucBssIndex
		= secGetBssIdxByWlanIdx(prAdapter, prSwRfb->ucWlanIdx);
	u_int8_t fgCheckDrop = FALSE;

#if CFG_SUPPORT_LOWLATENCY_MODE
	if (prAdapter->fgEnLowLatencyMode)
		fgCheckDrop = TRUE;
#endif

#if CFG_SUPPORT_OSHARE
	if (!fgCheckDrop &&
		(prAdapter->fgEnOshareMode) &&
		(ucBssIndex <= MAX_BSSID_NUM) &&
		(GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex)->eNetworkType
			== NETWORK_TYPE_P2P))
		fgCheckDrop = TRUE;
#endif

	if (fgCheckDrop && u2Etype == ETH_P_IP) {
		uint8_t *pucEthBody = &pucData[ETH_HLEN];
		uint8_t ucIpProto = pucEthBody[IP_PROTO_HLEN];

		if (ucIpProto == IP_PRO_UDP || ucIpProto == IP_PRO_TCP)
			return TRUE;
	}
	return FALSE;
}
#endif /* CFG_SUPPORT_LOWLATENCY_MODE */

void qmMoveStaTxQueue(struct STA_RECORD *prSrcStaRec,
		      struct STA_RECORD *prDstStaRec)
{
	uint8_t ucQueArrayIdx;
	struct QUE *prSrcQue = NULL;
	struct QUE *prDstQue = NULL;
	struct MSDU_INFO *prMsduInfo = NULL;
	uint8_t ucDstStaIndex = 0;

	ASSERT(prSrcStaRec);
	ASSERT(prDstStaRec);

	prSrcQue = &prSrcStaRec->arTxQueue[0];
	prDstQue = &prDstStaRec->arTxQueue[0];
	ucDstStaIndex = prDstStaRec->ucIndex;

	DBGLOG(QM, INFO, "Pending MSDUs for TC 0~3, %u %u %u %u\n",
	       prSrcQue[TC0_INDEX].u4NumElem, prSrcQue[TC1_INDEX].u4NumElem,
	       prSrcQue[TC2_INDEX].u4NumElem, prSrcQue[TC3_INDEX].u4NumElem);
	/* Concatenate all MSDU_INFOs in TX queues of this STA_REC */
	for (ucQueArrayIdx = 0; ucQueArrayIdx < TC4_INDEX; ucQueArrayIdx++) {
		prMsduInfo = (struct MSDU_INFO *)QUEUE_GET_HEAD(
			&prSrcQue[ucQueArrayIdx]);
		while (prMsduInfo) {
			prMsduInfo->ucStaRecIndex = ucDstStaIndex;
			prMsduInfo = (struct MSDU_INFO *)QUEUE_GET_NEXT_ENTRY(
				&prMsduInfo->rQueEntry);
		}
		QUEUE_CONCATENATE_QUEUES((&prDstQue[ucQueArrayIdx]),
					 (&prSrcQue[ucQueArrayIdx]));
	}
}

void qmHandleDelTspec(struct ADAPTER *prAdapter, struct STA_RECORD *prStaRec,
		      enum ENUM_ACI eAci)
{
	uint8_t aucNextUP[ACI_NUM] = {1 /* BEtoBK */, 1 /*na */, 0 /*VItoBE */,
				      4 /*VOtoVI */};
	enum ENUM_ACI aeNextAci[ACI_NUM] = {ACI_BK, ACI_BK, ACI_BE, ACI_VI};
	uint8_t ucActivedTspec = 0;
	uint8_t ucNewUp = 0;
	struct QUE *prSrcQue = NULL;
	struct QUE *prDstQue = NULL;
	struct MSDU_INFO *prMsduInfo = NULL;
	struct AC_QUE_PARMS *prAcQueParam = NULL;
	uint8_t ucTc = 0;

	if (!prStaRec || eAci == ACI_NUM || eAci == ACI_BK || !prAdapter ||
	    !prAdapter->prAisBssInfo) {
		DBGLOG(QM, ERROR, "prSta NULL %d, eAci %d, prAdapter NULL %d\n",
		       !prStaRec, eAci, !prAdapter);
		return;
	}
	prSrcQue = &prStaRec->arTxQueue[aucWmmAC2TcResourceSet1[eAci]];
	prAcQueParam = &(prAdapter->prAisBssInfo->arACQueParms[0]);
	ucActivedTspec = wmmHasActiveTspec(&prAdapter->rWifiVar.rWmmInfo);

	while (prAcQueParam[eAci].ucIsACMSet &&
			!(ucActivedTspec & BIT(eAci)) && eAci != ACI_BK) {
		eAci = aeNextAci[eAci];
		ucNewUp = aucNextUP[eAci];
	}
	DBGLOG(QM, INFO, "new ACI %d, ACM %d, HasTs %d\n", eAci,
	       prAcQueParam[eAci].ucIsACMSet, !!(ucActivedTspec & BIT(eAci)));
	ucTc = aucWmmAC2TcResourceSet1[eAci];
	prDstQue = &prStaRec->arTxQueue[ucTc];
	prMsduInfo = (struct MSDU_INFO *)QUEUE_GET_HEAD(prSrcQue);
	while (prMsduInfo) {
		prMsduInfo->ucUserPriority = ucNewUp;
		prMsduInfo->ucTC = ucTc;
		prMsduInfo = (struct MSDU_INFO *)QUEUE_GET_NEXT_ENTRY(
			&prMsduInfo->rQueEntry);
	}
	QUEUE_CONCATENATE_QUEUES(prDstQue, prSrcQue);
#if QM_ADAPTIVE_TC_RESOURCE_CTRL
	qmUpdateAverageTxQueLen(prAdapter);
	qmReassignTcResource(prAdapter);
#endif
	nicTxAdjustTcq(prAdapter);
	kalSetEvent(prAdapter->prGlueInfo);
}
