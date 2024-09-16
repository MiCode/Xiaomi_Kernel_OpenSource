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

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "precomp.h"
#include "tdls.h"

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
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
/*----------------------------------------------------------------------------*/
/*!
* @brief This function will initial all variables in regard to SW TX Queues and
*        all free lists of MSDU_INFO_T and SW_TFCB_T.
*
* @param prAdapter  Pointer to the Adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID nicTxInitialize(IN P_ADAPTER_T prAdapter)
{
	P_TX_CTRL_T prTxCtrl;
	PUINT_8 pucMemHandle;
	P_MSDU_INFO_T prMsduInfo;
	UINT_32 i;

	KAL_SPIN_LOCK_DECLARATION();

	DEBUGFUNC("nicTxInitialize");

	ASSERT(prAdapter);
	prTxCtrl = &prAdapter->rTxCtrl;

	/* 4 <1> Initialization of Traffic Class Queue Parameters */
	nicTxResetResource(prAdapter);

#if CFG_SDIO_TX_AGG
	prTxCtrl->pucTxCoalescingBufPtr = prAdapter->pucCoalescingBufCached;
#endif /* CFG_SDIO_TX_AGG */

	/* allocate MSDU_INFO_T and link it into rFreeMsduInfoList */
	QUEUE_INITIALIZE(&prTxCtrl->rFreeMsduInfoList);

	pucMemHandle = prTxCtrl->pucTxCached;
	for (i = 0; i < CFG_TX_MAX_PKT_NUM; i++) {
		prMsduInfo = (P_MSDU_INFO_T) pucMemHandle;
		kalMemZero(prMsduInfo, sizeof(MSDU_INFO_T));

		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_MSDU_INFO_LIST);
		QUEUE_INSERT_TAIL(&prTxCtrl->rFreeMsduInfoList, (P_QUE_ENTRY_T) prMsduInfo);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_MSDU_INFO_LIST);

		pucMemHandle += ALIGN_4(sizeof(MSDU_INFO_T));
	}

	ASSERT(prTxCtrl->rFreeMsduInfoList.u4NumElem == CFG_TX_MAX_PKT_NUM);
	/* Check if the memory allocation consist with this initialization function */
	ASSERT((UINT_32) (pucMemHandle - prTxCtrl->pucTxCached) == prTxCtrl->u4TxCachedSize);

	QUEUE_INITIALIZE(&prTxCtrl->rTxMgmtTxingQueue);
	prTxCtrl->i4TxMgmtPendingNum = 0;

#if CFG_HIF_STATISTICS
	prTxCtrl->u4TotalTxAccessNum = 0;
	prTxCtrl->u4TotalTxPacketNum = 0;
#endif

	prTxCtrl->i4PendingFwdFrameCount = 0;

	qmInit(prAdapter);

	TX_RESET_ALL_CNTS(prTxCtrl);

}				/* end of nicTxInitialize() */

/*----------------------------------------------------------------------------*/
/*!
* \brief Driver maintain a variable that is synchronous with the usage of individual
*        TC Buffer Count. This function will check if has enough TC Buffer for incoming
*        packet and then update the value after promise to provide the resources.
*
* \param[in] prAdapter              Pointer to the Adapter structure.
* \param[in] ucTC                   Specify the resource of TC
*
* \retval WLAN_STATUS_SUCCESS   Resource is available and been assigned.
* \retval WLAN_STATUS_RESOURCES Resource is not available.
*/
/*----------------------------------------------------------------------------*/
UINT_32 u4CurrTick;
WLAN_STATUS nicTxAcquireResource(IN P_ADAPTER_T prAdapter, IN UINT_8 ucTC, IN BOOLEAN pfgIsSecOrMgmt)
{
#define TC4_NO_RESOURCE_DELAY_MS      5    /* exponential of 5s */
#define TC4_NO_RESOURCE_DELAY_1S      1    /* exponential of 1s */

	P_TX_CTRL_T prTxCtrl;
	WLAN_STATUS u4Status = WLAN_STATUS_RESOURCES;
	P_QUE_MGT_T prQM;
	BOOLEAN fgWmtCoreDump = FALSE;


	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);
	prTxCtrl = &prAdapter->rTxCtrl;

	prQM = &prAdapter->rQM;

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_RESOURCE);

	/*
	 * DbgPrint("nicTxAcquireResource prTxCtrl->rTc.aucFreeBufferCount[%d]=%d\n",
	 * ucTC, prTxCtrl->rTc.aucFreeBufferCount[ucTC]);
	 */
	do {
		if (pfgIsSecOrMgmt && (ucTC == TC4_INDEX)) {
			if (prTxCtrl->rTc.aucFreeBufferCount[ucTC] < 2) {
				DBGLOG(TX, EVENT, "<wlan> aucFreeBufferCount = %d\n",
				prTxCtrl->rTc.aucFreeBufferCount[ucTC]);

				if (prTxCtrl->rTc.aucFreeBufferCount[ucTC])
					u4CurrTick = 0;

				break;
			}
		}

		if (prTxCtrl->rTc.aucFreeBufferCount[ucTC]) {

			if (ucTC == TC4_INDEX)
				u4CurrTick = 0;
			/* get a available TX entry */
			prTxCtrl->rTc.aucFreeBufferCount[ucTC]--;

			prQM->au4ResourceUsedCounter[ucTC]++;

			DBGLOG(TX, EVENT, "Acquire: TC = %d aucFreeBufferCount = %d\n",
					   ucTC, prTxCtrl->rTc.aucFreeBufferCount[ucTC]);

			u4Status = WLAN_STATUS_SUCCESS;
		}
	} while (FALSE);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_RESOURCE);

	if (ucTC == TC4_INDEX) {
		if (u4CurrTick == 0)
			u4CurrTick = kalGetTimeTick();

		if (CHECK_FOR_TIMEOUT(kalGetTimeTick(), u4CurrTick,
				SEC_TO_SYSTIME(TC4_NO_RESOURCE_DELAY_1S)))
			wlanDumpCommandFwStatus();

		if (CHECK_FOR_TIMEOUT(kalGetTimeTick(), u4CurrTick,
				SEC_TO_SYSTIME(TC4_NO_RESOURCE_DELAY_MS))) {
			wlanDumpTcResAndTxedCmd(NULL, 0);
			cmdBufDumpCmdQueue(&prAdapter->rPendingCmdQueue, "waiting response CMD queue");
			glDumpConnSysCpuInfo(prAdapter->prGlueInfo);
			/* dump TC4[0] ~ TC4[3] TX_DESC */
			wlanDebugHifDescriptorDump(prAdapter, MTK_AMPDU_TX_DESC, DEBUG_TC4_INDEX);

			fgWmtCoreDump = glIsWmtCodeDump();
			if (fgWmtCoreDump == FALSE) {
				DBGLOG(TX, WARN,
					"[TC4 no resource delay 5s!] Trigger Coredump\n");
				GL_RESET_TRIGGER(prAdapter, RST_FLAG_DO_CORE_DUMP | RST_FLAG_PREVENT_POWER_OFF);
			} else
				DBGLOG(TX, WARN,
					"[TC4 no resource delay 5s!] WMT is code dumping! STOP AEE & chip reset\n");

			u4CurrTick = 0;
		}

	}
	return u4Status;

}				/* end of nicTxAcquireResourceAndTFCBs() */

/*----------------------------------------------------------------------------*/
/*!
* @brief Driver maintain a variable that is synchronous with the usage of individual
*        TC Buffer Count. This function will do polling if FW has return the resource.
*        Used when driver start up before enable interrupt.
*
* @param prAdapter      Pointer to the Adapter structure.
* @param ucTC           Specify the resource of TC
*
* @retval WLAN_STATUS_SUCCESS   Resource is available.
* @retval WLAN_STATUS_FAILURE   Resource is not available.
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS nicTxPollingResource(IN P_ADAPTER_T prAdapter, IN UINT_8 ucTC)
{
	P_TX_CTRL_T prTxCtrl;
	WLAN_STATUS u4Status = WLAN_STATUS_FAILURE;
	INT_32 i = NIC_TX_RESOURCE_POLLING_TIMEOUT;
	UINT_32 au4WTSR[2];

	ASSERT(prAdapter);
	prTxCtrl = &prAdapter->rTxCtrl;

	if (ucTC >= TC_NUM)
		return WLAN_STATUS_FAILURE;

	if (prTxCtrl->rTc.aucFreeBufferCount[ucTC] > 0)
		return WLAN_STATUS_SUCCESS;

	while (i-- > 0) {
		HAL_READ_TX_RELEASED_COUNT(prAdapter, au4WTSR);

		if (kalIsCardRemoved(prAdapter->prGlueInfo) == TRUE || fgIsBusAccessFailed == TRUE) {
			u4Status = WLAN_STATUS_FAILURE;
			break;
		} else if (nicTxReleaseResource(prAdapter, (PUINT_8) au4WTSR)) {
			if (prTxCtrl->rTc.aucFreeBufferCount[ucTC] > 0) {
				u4Status = WLAN_STATUS_SUCCESS;
				break;
			}
			kalMsleep(NIC_TX_RESOURCE_POLLING_DELAY_MSEC);
		} else {
			kalMsleep(NIC_TX_RESOURCE_POLLING_DELAY_MSEC);
		}
		if (i < NIC_TX_RESOURCE_POLLING_TIMEOUT - 30) {
			wlanReadFwStatus(prAdapter);
			wlanDumpCommandFwStatus();
		}
	}

	if (i <= 0 && ucTC == TC4_INDEX) {
		DBGLOG(TX, ERROR, "polling Tx resource for Tc4 timeout\n");
		wlanDumpTcResAndTxedCmd(NULL, 0);
		glDumpConnSysCpuInfo(prAdapter->prGlueInfo);
	}
#if DBG
	{
		INT_32 i4Times = NIC_TX_RESOURCE_POLLING_TIMEOUT - (i + 1);

		if (i4Times) {
			DBGLOG(TX, TRACE, "Polling MCR_WTSR delay %d times, %d msec\n",
					   i4Times, (i4Times * NIC_TX_RESOURCE_POLLING_DELAY_MSEC));
		}
	}
#endif /* DBG */

	return u4Status;

}				/* end of nicTxPollingResource() */

/*----------------------------------------------------------------------------*/
/*!
* \brief Driver maintain a variable that is synchronous with the usage of individual
*        TC Buffer Count. This function will release TC Buffer count according to
*        the given TX_STATUS COUNTER after TX Done.
*
* \param[in] prAdapter              Pointer to the Adapter structure.
* \param[in] u4TxStatusCnt          Value of TX STATUS
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
BOOLEAN nicTxReleaseResource(IN P_ADAPTER_T prAdapter, IN unsigned char *aucTxRlsCnt)
{
	PUINT_32 pu4Tmp = (PUINT_32) aucTxRlsCnt;
	P_TX_CTRL_T prTxCtrl;
	BOOLEAN bStatus = FALSE;
	UINT_32 i;

	KAL_SPIN_LOCK_DECLARATION();

	P_QUE_MGT_T prQM = &prAdapter->rQM;

	prTxCtrl = &prAdapter->rTxCtrl;


	if (pu4Tmp[0] | pu4Tmp[1]) {

		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_RESOURCE);
		for (i = 0; i < TC_NUM; i++)
			prTxCtrl->rTc.aucFreeBufferCount[i] += aucTxRlsCnt[i];
		for (i = 0; i < TC_NUM; i++)
			prQM->au4QmTcResourceBackCounter[i] += aucTxRlsCnt[i];
		if (aucTxRlsCnt[TC4_INDEX] != 0)
			wlanTraceReleaseTcRes(prAdapter, aucTxRlsCnt, prTxCtrl->rTc.aucFreeBufferCount[TC4_INDEX]);

		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_RESOURCE);
#if 0
		for (i = 0; i < TC_NUM; i++) {
			DBGLOG(TX, TRACE, "aucFreeBufferCount[%d]: %d, aucMaxNumOfBuffer[%d]: %d\n",
					     i, prTxCtrl->rTc.aucFreeBufferCount[i], i,
					     prTxCtrl->rTc.aucMaxNumOfBuffer[i]);
		}
		DbgPrint("prTxCtrl->rTc.aucFreeBufferCount[0]=%d\n", prTxCtrl->rTc.aucFreeBufferCount[0]);
		DbgPrint("prTxCtrl->rTc.aucFreeBufferCount[1]=%d\n", prTxCtrl->rTc.aucFreeBufferCount[1]);
		DbgPrint("prTxCtrl->rTc.aucFreeBufferCount[2]=%d\n", prTxCtrl->rTc.aucFreeBufferCount[2]);
		DbgPrint("prTxCtrl->rTc.aucFreeBufferCount[3]=%d\n", prTxCtrl->rTc.aucFreeBufferCount[3]);
		DbgPrint("prTxCtrl->rTc.aucFreeBufferCount[4]=%d\n", prTxCtrl->rTc.aucFreeBufferCount[4]);
		DbgPrint("prTxCtrl->rTc.aucFreeBufferCount[5]=%d\n", prTxCtrl->rTc.aucFreeBufferCount[5]);
#endif
		ASSERT(prTxCtrl->rTc.aucFreeBufferCount[TC0_INDEX] <= prTxCtrl->rTc.aucMaxNumOfBuffer[TC0_INDEX]);
		ASSERT(prTxCtrl->rTc.aucFreeBufferCount[TC1_INDEX] <= prTxCtrl->rTc.aucMaxNumOfBuffer[TC1_INDEX]);
		ASSERT(prTxCtrl->rTc.aucFreeBufferCount[TC2_INDEX] <= prTxCtrl->rTc.aucMaxNumOfBuffer[TC2_INDEX]);
		ASSERT(prTxCtrl->rTc.aucFreeBufferCount[TC3_INDEX] <= prTxCtrl->rTc.aucMaxNumOfBuffer[TC3_INDEX]);
		ASSERT(prTxCtrl->rTc.aucFreeBufferCount[TC4_INDEX] <= prTxCtrl->rTc.aucMaxNumOfBuffer[TC4_INDEX]);
		ASSERT(prTxCtrl->rTc.aucFreeBufferCount[TC5_INDEX] <= prTxCtrl->rTc.aucMaxNumOfBuffer[TC5_INDEX]);
		bStatus = TRUE;
	}

	return bStatus;
}				/* end of nicTxReleaseResource() */

/*----------------------------------------------------------------------------*/
/*!
* \brief Reset TC Buffer Count to initialized value
*
* \param[in] prAdapter              Pointer to the Adapter structure.
*
* @return WLAN_STATUS_SUCCESS
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS nicTxResetResource(IN P_ADAPTER_T prAdapter)
{
	P_TX_CTRL_T prTxCtrl;

	KAL_SPIN_LOCK_DECLARATION();

	DEBUGFUNC("nicTxResetResource");

	ASSERT(prAdapter);
	prTxCtrl = &prAdapter->rTxCtrl;

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_RESOURCE);

	prTxCtrl->rTc.aucMaxNumOfBuffer[TC0_INDEX] = NIC_TX_BUFF_COUNT_TC0;
	prTxCtrl->rTc.aucFreeBufferCount[TC0_INDEX] = NIC_TX_BUFF_COUNT_TC0;

	prTxCtrl->rTc.aucMaxNumOfBuffer[TC1_INDEX] = NIC_TX_BUFF_COUNT_TC1;
	prTxCtrl->rTc.aucFreeBufferCount[TC1_INDEX] = NIC_TX_BUFF_COUNT_TC1;

	prTxCtrl->rTc.aucMaxNumOfBuffer[TC2_INDEX] = NIC_TX_BUFF_COUNT_TC2;
	prTxCtrl->rTc.aucFreeBufferCount[TC2_INDEX] = NIC_TX_BUFF_COUNT_TC2;

	prTxCtrl->rTc.aucMaxNumOfBuffer[TC3_INDEX] = NIC_TX_BUFF_COUNT_TC3;
	prTxCtrl->rTc.aucFreeBufferCount[TC3_INDEX] = NIC_TX_BUFF_COUNT_TC3;

	prTxCtrl->rTc.aucMaxNumOfBuffer[TC4_INDEX] = NIC_TX_BUFF_COUNT_TC4;
	prTxCtrl->rTc.aucFreeBufferCount[TC4_INDEX] = NIC_TX_BUFF_COUNT_TC4;

	prTxCtrl->rTc.aucMaxNumOfBuffer[TC5_INDEX] = NIC_TX_BUFF_COUNT_TC5;
	prTxCtrl->rTc.aucFreeBufferCount[TC5_INDEX] = NIC_TX_BUFF_COUNT_TC5;

	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_RESOURCE);

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief Driver maintain a variable that is synchronous with the usage of individual
*        TC Buffer Count. This function will return the value for other component
*        which needs this information for making decisions
*
* @param prAdapter      Pointer to the Adapter structure.
* @param ucTC           Specify the resource of TC
*
* @retval UINT_8        The number of corresponding TC number
*/
/*----------------------------------------------------------------------------*/
UINT_8 nicTxGetResource(IN P_ADAPTER_T prAdapter, IN UINT_8 ucTC)
{
	P_TX_CTRL_T prTxCtrl;

	ASSERT(prAdapter);
	prTxCtrl = &prAdapter->rTxCtrl;

	ASSERT(prTxCtrl);

	if (ucTC >= TC_NUM)
		return 0;
	else
		return prTxCtrl->rTc.aucFreeBufferCount[ucTC];
}

/*----------------------------------------------------------------------------*/
/*!
* @brief In this function, we'll aggregate frame(PACKET_INFO_T)
* corresponding to HIF TX port
*
* @param prAdapter              Pointer to the Adapter structure.
* @param prMsduInfoListHead     a link list of P_MSDU_INFO_T
*
* @retval WLAN_STATUS_SUCCESS   Bus access ok.
* @retval WLAN_STATUS_FAILURE   Bus access fail.
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS nicTxMsduInfoList(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfoListHead)
{
	P_MSDU_INFO_T prMsduInfo, prNextMsduInfo;
	QUE_T qDataPort0, qDataPort1;
	WLAN_STATUS status;
	BOOLEAN pfgIsSecOrMgmt = FALSE;

	ASSERT(prAdapter);
	ASSERT(prMsduInfoListHead);

	prMsduInfo = prMsduInfoListHead;

	QUEUE_INITIALIZE(&qDataPort0);
	QUEUE_INITIALIZE(&qDataPort1);

	/* Separate MSDU_INFO_T lists into 2 categories: for Port#0 & Port#1 */
	while (prMsduInfo) {
		prNextMsduInfo = (P_MSDU_INFO_T) QUEUE_GET_NEXT_ENTRY((P_QUE_ENTRY_T) prMsduInfo);
#if DBG && 0
		LOG_FUNC("nicTxMsduInfoList Acquire TC %d net %u mac len %u len %u Type %u 1x %u 11 %u\n",
			 prMsduInfo->ucTC,
			 prMsduInfo->ucNetworkType,
			 prMsduInfo->ucMacHeaderLength,
			 prMsduInfo->u2FrameLength,
			 prMsduInfo->ucPacketType, prMsduInfo->fgIs802_1x, prMsduInfo->fgIs802_11);

		LOG_FUNC("Dest Mac: %pM\n", prMsduInfo->aucEthDestAddr);
#endif

		/* double-check available TX resouce (need to sync with CONNSYS FW) */
		/* caller must guarantee that the TX resource is enough in the func; OR assert here */
		switch (prMsduInfo->ucTC) {
		case TC0_INDEX:
		case TC1_INDEX:
		case TC2_INDEX:
		case TC3_INDEX:
		case TC5_INDEX:	/* Broadcast/multicast data packets */
			QUEUE_GET_NEXT_ENTRY((P_QUE_ENTRY_T) prMsduInfo) = NULL;
			QUEUE_INSERT_TAIL(&qDataPort0, (P_QUE_ENTRY_T) prMsduInfo);
			status = nicTxAcquireResource(prAdapter, prMsduInfo->ucTC, FALSE);
			ASSERT(status == WLAN_STATUS_SUCCESS)

			    break;

		case TC4_INDEX:	/* Command or 802.1x packets */
			QUEUE_GET_NEXT_ENTRY((P_QUE_ENTRY_T) prMsduInfo) = NULL;
			QUEUE_INSERT_TAIL(&qDataPort1, (P_QUE_ENTRY_T) prMsduInfo);

			if ((prMsduInfo->fgIs802_1x == TRUE) ||
				(prMsduInfo->fgIs802_11 == TRUE))
				pfgIsSecOrMgmt = TRUE;

			status = nicTxAcquireResource(prAdapter, prMsduInfo->ucTC, pfgIsSecOrMgmt);
			ASSERT(status == WLAN_STATUS_SUCCESS)

			    break;

		default:
			ASSERT(0);
			break;
		}

		prMsduInfo = prNextMsduInfo;
	}

	/* send packets to HIF port0 or port1 here */
	if (qDataPort0.u4NumElem > 0)
		nicTxMsduQueue(prAdapter, 0, &qDataPort0);

	if (qDataPort1.u4NumElem > 0)
		nicTxMsduQueue(prAdapter, 1, &qDataPort1);

	return WLAN_STATUS_SUCCESS;
}

#if CFG_ENABLE_PKT_LIFETIME_PROFILE

#if CFG_PRINT_RTP_PROFILE
PKT_PROFILE_T rPrevRoundLastPkt;

BOOLEAN
nicTxLifetimePrintCheckRTP(IN P_MSDU_INFO_T prPrevProfileMsduInfo,
			   IN P_PKT_PROFILE_T prPrevRoundLastPkt,
			   IN P_PKT_PROFILE_T prPktProfile,
			   IN OUT PBOOLEAN pfgGotFirst, IN UINT_32 u4MaxDeltaTime, IN UINT_8 ucSnToBePrinted)
{
	BOOLEAN fgPrintCurPkt = FALSE;

	if (u4MaxDeltaTime) {
		/* 4 1. check delta between current round first pkt and prevous round last pkt */
		if (!*pfgGotFirst) {
			*pfgGotFirst = TRUE;

			if (prPrevRoundLastPkt->fgIsValid) {
				if (CHK_PROFILES_DELTA(prPktProfile, prPrevRoundLastPkt, u4MaxDeltaTime)) {
					PRINT_PKT_PROFILE(prPrevRoundLastPkt, "PR");
					fgPrintCurPkt = TRUE;
				}
			}
		}
		/* 4 2. check delta between current pkt and previous pkt */
		if (prPrevProfileMsduInfo) {
			if (CHK_PROFILES_DELTA(prPktProfile, &prPrevProfileMsduInfo->rPktProfile, u4MaxDeltaTime)) {
				PRINT_PKT_PROFILE(&prPrevProfileMsduInfo->rPktProfile, "P");
				fgPrintCurPkt = TRUE;
			}
		}
		/* 4 3. check delta of current pkt lifetime */
		if (CHK_PROFILE_DELTA(prPktProfile, u4MaxDeltaTime))
			fgPrintCurPkt = TRUE;
	}
	/* 4 4. print every X RTP packets */
#if CFG_SUPPORT_WFD
	if ((ucSnToBePrinted != 0) && (prPktProfile->u2RtpSn % ucSnToBePrinted) == 0)
		fgPrintCurPkt = TRUE;
#endif

	return fgPrintCurPkt;
}

BOOLEAN
nicTxLifetimePrintCheckSnOrder(IN P_MSDU_INFO_T prPrevProfileMsduInfo,
			       IN P_PKT_PROFILE_T prPrevRoundLastPkt,
			       IN P_PKT_PROFILE_T prPktProfile, IN OUT PBOOLEAN pfgGotFirst, IN UINT_8 ucLayer)
{
	BOOLEAN fgPrintCurPkt = FALSE;
	P_PKT_PROFILE_T prTarPktProfile = NULL;
	UINT_16 u2PredictSn = 0;
	UINT_16 u2CurrentSn = 0;
	UINT_8 aucNote[8];

	/* 4 1. Get the target packet profile to compare */

	/* 4 1.1 check SN between current round first pkt and prevous round last pkt */
	if ((!*pfgGotFirst) && (prPrevRoundLastPkt->fgIsValid)) {
		*pfgGotFirst = TRUE;
		prTarPktProfile = prPrevRoundLastPkt;
		kalMemCopy(aucNote, "PR\0", 3);
	}
	/* 4 1.2 check SN between current pkt and previous pkt */
	else if (prPrevProfileMsduInfo) {
		prTarPktProfile = &prPrevProfileMsduInfo->rPktProfile;
		kalMemCopy(aucNote, "P\0", 2);
	}

	if (!prTarPktProfile)
		return FALSE;
	/* 4 2. Check IP or RTP SN */
	switch (ucLayer) {
		/* Check IP SN */
	case 0:
		u2PredictSn = prTarPktProfile->u2IpSn + 1;
		u2CurrentSn = prPktProfile->u2IpSn;
		break;
		/* Check RTP SN */
	case 1:
	default:
		u2PredictSn = prTarPktProfile->u2RtpSn + 1;
		u2CurrentSn = prPktProfile->u2RtpSn;
		break;

	}
	/* 4 */
	/* 4 3. Compare SN */
	if (u2CurrentSn != u2PredictSn) {
		PRINT_PKT_PROFILE(prTarPktProfile, aucNote);
		fgPrintCurPkt = TRUE;
	}

	return fgPrintCurPkt;
}
#endif

VOID nicTxReturnMsduInfoProfiling(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfoListHead)
{
	P_MSDU_INFO_T prMsduInfo = prMsduInfoListHead, prNextMsduInfo;
	P_PKT_PROFILE_T prPktProfile;
	UINT_16 u2MagicCode = 0;

	UINT_8 ucDebugtMode = 0;
#if CFG_PRINT_RTP_PROFILE
	P_MSDU_INFO_T prPrevProfileMsduInfo = NULL;
	P_PKT_PROFILE_T prPrevRoundLastPkt = &rPrevRoundLastPkt;

	BOOLEAN fgPrintCurPkt = FALSE;
	BOOLEAN fgGotFirst = FALSE;
	UINT_8 ucSnToBePrinted = 0;

	UINT_32 u4MaxDeltaTime = 50;	/* in ms */
#endif

#if CFG_ENABLE_PER_STA_STATISTICS
	UINT_32 u4PktPrintPeriod = 0;
#endif

#if CFG_SUPPORT_WFD
	P_WFD_CFG_SETTINGS_T prWfdCfgSettings = (P_WFD_CFG_SETTINGS_T) NULL;

	if (prAdapter->fgIsP2PRegistered) {
		prWfdCfgSettings = &prAdapter->rWifiVar.prP2pFsmInfo->rWfdConfigureSettings;
		u2MagicCode = prWfdCfgSettings->u2WfdMaximumTp;
		ucDebugtMode = prAdapter->rWifiVar.prP2pFsmInfo->rWfdDebugSetting.ucWfdDebugMode;
		/* if(prWfdCfgSettings->ucWfdEnable && (prWfdCfgSettings->u4WfdFlag & BIT(0))) { */
		/* u2MagicCode = 0xE040; */
		/* } */
	}
#endif

#if CFG_PRINT_RTP_PROFILE
	if ((u2MagicCode >= 0xF000)) {
		ucSnToBePrinted = (UINT_8) (u2MagicCode & BITS(0, 7));
		u4MaxDeltaTime = (UINT_8) (((u2MagicCode & BITS(8, 11)) >> 8) * 10);
	} else {
		ucSnToBePrinted = 0;
		u4MaxDeltaTime = 0;
	}

#endif

#if CFG_ENABLE_PER_STA_STATISTICS
	if ((u2MagicCode >= 0xE000) && (u2MagicCode < 0xF000))
		u4PktPrintPeriod = (UINT_32) ((u2MagicCode & BITS(0, 7)) * 32);
	else
		u4PktPrintPeriod = 0;
#endif

	while (prMsduInfo) {
		prNextMsduInfo = (P_MSDU_INFO_T) QUEUE_GET_NEXT_ENTRY((P_QUE_ENTRY_T) prMsduInfo);
		prPktProfile = &prMsduInfo->rPktProfile;

		if (prPktProfile->fgIsValid) {

			prPktProfile->rHifTxDoneTimestamp = kalGetTimeTick();
			if (ucDebugtMode > 1) {

#if CFG_PRINT_RTP_PROFILE
#if CFG_PRINT_RTP_SN_SKIP
				fgPrintCurPkt = nicTxLifetimePrintCheckSnOrder(prPrevProfileMsduInfo,
									       prPrevRoundLastPkt,
									       prPktProfile, &fgGotFirst, 0);
#else
				fgPrintCurPkt = nicTxLifetimePrintCheckRTP(prPrevProfileMsduInfo,
									   prPrevRoundLastPkt,
									   prPktProfile,
									   &fgGotFirst,
									   u4MaxDeltaTime, ucSnToBePrinted);
#endif

				/* Print current pkt profile */
				if (fgPrintCurPkt && ucDebugtMode > 1)
					PRINT_PKT_PROFILE(prPktProfile, "C");

				prPrevProfileMsduInfo = prMsduInfo;
				fgPrintCurPkt = FALSE;
#endif
			}
#if CFG_ENABLE_PER_STA_STATISTICS
			{
				P_STA_RECORD_T prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);
				UINT_32 u4DeltaTime;
				UINT_32 u4DeltaHifTime;
#if 0
				P_QUE_MGT_T prQM = &prAdapter->rQM;
#endif
				UINT_8 ucNetIndex;

				if (prStaRec) {
					ucNetIndex = prStaRec->ucNetTypeIndex;
					u4DeltaTime = (UINT_32) (prPktProfile->rHifTxDoneTimestamp -
							prPktProfile->rHardXmitArrivalTimestamp);
					u4DeltaHifTime = (UINT_32) (prPktProfile->rHifTxDoneTimestamp -
							prPktProfile->rDequeueTimestamp);
					prStaRec->u4TotalTxPktsNumber++;

					prStaRec->u4TotalTxPktsTime += u4DeltaTime;
					prStaRec->u4TotalTxPktsHifTime += u4DeltaHifTime;

					if (u4DeltaTime > prStaRec->u4MaxTxPktsTime)
						prStaRec->u4MaxTxPktsTime = u4DeltaTime;

					if (u4DeltaHifTime > prStaRec->u4MaxTxPktsHifTime)
						prStaRec->u4MaxTxPktsHifTime = u4DeltaHifTime;


					if (u4DeltaTime >= NIC_TX_TIME_THRESHOLD)
						prStaRec->u4ThresholdCounter++;
#if 0
					if (u4PktPrintPeriod && (prStaRec->u4TotalTxPktsNumber >= u4PktPrintPeriod)) {

						DBGLOG(TX, TRACE, "[%u]N[%4u]A[%5u]M[%4u]T[%4u]E[%4u]\n",
						   prStaRec->ucIndex,
						   prStaRec->u4TotalTxPktsNumber,
						   prStaRec->u4TotalTxPktsTime / prStaRec->u4TotalTxPktsNumber,
						   prStaRec->u4MaxTxPktsTime,
						   prStaRec->u4ThresholdCounter,
						   prQM->au4QmTcResourceEmptyCounter[ucNetIndex][TC2_INDEX]);

						prStaRec->u4TotalTxPktsNumber = 0;
						prStaRec->u4TotalTxPktsTime = 0;
						prStaRec->u4MaxTxPktsTime = 0;
						prStaRec->u4ThresholdCounter = 0;
						prQM->au4QmTcResourceEmptyCounter[ucNetIndex][TC2_INDEX] = 0;
					}
#endif
				}

			}
#endif
		}

		prMsduInfo = prNextMsduInfo;
	};

#if CFG_PRINT_RTP_PROFILE
	/* 4 4. record the lifetime of current round last pkt */
	if (prPrevProfileMsduInfo) {
		prPktProfile = &prPrevProfileMsduInfo->rPktProfile;
		prPrevRoundLastPkt->u2IpSn = prPktProfile->u2IpSn;
		prPrevRoundLastPkt->u2RtpSn = prPktProfile->u2RtpSn;
		prPrevRoundLastPkt->rHardXmitArrivalTimestamp = prPktProfile->rHardXmitArrivalTimestamp;
		prPrevRoundLastPkt->rEnqueueTimestamp = prPktProfile->rEnqueueTimestamp;
		prPrevRoundLastPkt->rDequeueTimestamp = prPktProfile->rDequeueTimestamp;
		prPrevRoundLastPkt->rHifTxDoneTimestamp = prPktProfile->rHifTxDoneTimestamp;
		prPrevRoundLastPkt->ucTcxFreeCount = prPktProfile->ucTcxFreeCount;
		prPrevRoundLastPkt->fgIsPrinted = prPktProfile->fgIsPrinted;
		prPrevRoundLastPkt->fgIsValid = TRUE;
	}
#endif

	nicTxReturnMsduInfo(prAdapter, prMsduInfoListHead);

}

VOID nicTxLifetimeRecordEn(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo, IN P_NATIVE_PACKET prPacket)
{
	P_PKT_PROFILE_T prPktProfile = &prMsduInfo->rPktProfile;

	/* Enable packet lifetime profiling */
	prPktProfile->fgIsValid = TRUE;

	/* Packet arrival time at kernel Hard Xmit */
	prPktProfile->rHardXmitArrivalTimestamp = GLUE_GET_PKT_ARRIVAL_TIME(prPacket);

	/* Packet enqueue time */
	prPktProfile->rEnqueueTimestamp = (OS_SYSTIME) kalGetTimeTick();

}

#if CFG_PRINT_RTP_PROFILE
/*
 * in:
 *    data   RTP packet pointer
 *    size   RTP size
 * return
 *    0:audio 1: video, -1:none
 */
UINT8 checkRtpAV(PUINT_8 data, UINT_32 size)
{
	PUINT_8 buf = data + 12;

	while (buf + 188 <= data + size) {
		int pid = ((buf[1] << 8) & 0x1F00) | (buf[2] & 0xFF);

		if (pid == 0 || pid == 0x100 || pid == 0x1000)
			buf += 188;
		else if (pid == 0x1100)
			return 0;
		else if (pid == 0x1011)
			return 1;
	}
	return -1;
}

VOID
nicTxLifetimeCheckRTP(IN P_ADAPTER_T prAdapter,
		      IN P_MSDU_INFO_T prMsduInfo,
		      IN P_NATIVE_PACKET prPacket, IN UINT_32 u4PacketLen, IN UINT_8 ucNetworkType)
{
	struct sk_buff *prSkb = (struct sk_buff *)prPacket;
	UINT_16 u2EtherTypeLen;
	PUINT_8 aucLookAheadBuf = NULL;
	P_PKT_PROFILE_T prPktProfile = &prMsduInfo->rPktProfile;

	/* UINT_8 ucRtpHdrOffset = 28; */
	UINT_8 ucRtpSnOffset = 30;
	/* UINT_32 u4RtpSrcPort = 15550; */
	P_TX_CTRL_T prTxCtrl;
#if CFG_SUPPORT_WFD
	P_WFD_CFG_SETTINGS_T prWfdCfgSettings = (P_WFD_CFG_SETTINGS_T) NULL;
	P_WFD_DBG_CFG_SETTINGS_T prWfdDbgSettings = (P_WFD_DBG_CFG_SETTINGS_T) NULL;

	BOOLEAN fgEnProfiling = FALSE;

	if (prAdapter->fgIsP2PRegistered) {
		prWfdCfgSettings = &prAdapter->rWifiVar.prP2pFsmInfo->rWfdConfigureSettings;
		prWfdDbgSettings = &prAdapter->rWifiVar.prP2pFsmInfo->rWfdDebugSetting;
#if CFG_PRINT_RTP_SN_SKIP
		if (ucNetworkType == NETWORK_TYPE_P2P_INDEX) {
			fgEnProfiling = TRUE;
		} else
#endif
		if (((prWfdCfgSettings->u2WfdMaximumTp >= 0xF000) ||
			     (prWfdDbgSettings->ucWfdDebugMode > 0)) && (ucNetworkType == NETWORK_TYPE_P2P_INDEX)) {
			fgEnProfiling = TRUE;
		}
	}

	if (fgEnProfiling == FALSE) {
		/* prPktProfile->fgIsValid = FALSE; */
		return;
	}
#endif

	prTxCtrl = &prAdapter->rTxCtrl;
	/* prPktProfile->fgIsValid = FALSE; */

	aucLookAheadBuf = prSkb->data;

	u2EtherTypeLen = (aucLookAheadBuf[ETH_TYPE_LEN_OFFSET] << 8) | (aucLookAheadBuf[ETH_TYPE_LEN_OFFSET + 1]);

	if ((u2EtherTypeLen == ETH_P_IP) && (u4PacketLen >= LOOK_AHEAD_LEN)) {
		PUINT_8 pucIpHdr = &aucLookAheadBuf[ETH_HLEN];
		UINT_16 u2tmpIpSN = 0;
		UINT_8 ucIpVersion;

		ucIpVersion = (pucIpHdr[0] & IPVH_VERSION_MASK) >> IPVH_VERSION_OFFSET;
		if (ucIpVersion == IPVERSION) {
			if (pucIpHdr[IPV4_HDR_IP_PROTOCOL_OFFSET] == IP_PROTOCOL_UDP) {

				/*
				 * if(checkRtpAV(&pucIpHdr[ucRtpHdrOffset],
				 *  (u4PacketLen - ETH_HLEN - ucRtpHdrOffset)) == 0) {
				 */

				if (prPktProfile->fgIsValid == FALSE)
					nicTxLifetimeRecordEn(prAdapter, prMsduInfo, prPacket);

				prPktProfile->fgIsPrinted = FALSE;

				prPktProfile->ucTcxFreeCount = prTxCtrl->rTc.aucFreeBufferCount[TC2_INDEX];

				/* RTP SN */
				prPktProfile->u2RtpSn = pucIpHdr[ucRtpSnOffset] << 8 | pucIpHdr[ucRtpSnOffset + 1];

				/* IP SN */
				prPktProfile->u2IpSn = pucIpHdr[IPV4_HDR_IP_IDENTIFICATION_OFFSET] << 8 |
				    pucIpHdr[IPV4_HDR_IP_IDENTIFICATION_OFFSET + 1];
				u2tmpIpSN = prPktProfile->u2IpSn;
				if (prWfdDbgSettings->ucWfdDebugMode == 1) {
					if ((u2tmpIpSN & (prWfdDbgSettings->u2WfdSNShowPeiroid)) == 0)
						DBGLOG(TX, TRACE,
							"RtpSn=%d IPId=%d j=%lu\n", prPktProfile->u2RtpSn,
							prPktProfile->u2IpSn, jiffies);
				}
				/* } */
			}
		}
	}

}
#endif
#if CFG_ENABLE_PER_STA_STATISTICS
VOID
nicTxLifetimeCheckByAC(IN P_ADAPTER_T prAdapter,
		       IN P_MSDU_INFO_T prMsduInfo, IN P_NATIVE_PACKET prPacket, IN UINT_8 ucPriorityParam)
{
	switch (ucPriorityParam) {
		/* BK */
		/* case 1: */
		/* case 2: */

		/* BE */
		/* case 0: */
		/* case 3: */

		/* VI */
	case 4:
	case 5:

		/* VO */
	case 6:
	case 7:
		nicTxLifetimeRecordEn(prAdapter, prMsduInfo, prPacket);
		break;
	default:
		break;
	}
}

#endif

VOID
nicTxLifetimeCheck(IN P_ADAPTER_T prAdapter,
		   IN P_MSDU_INFO_T prMsduInfo,
		   IN P_NATIVE_PACKET prPacket,
		   IN UINT_8 ucPriorityParam, IN UINT_32 u4PacketLen, IN UINT_8 ucNetworkType)
{
	P_PKT_PROFILE_T prPktProfile = &prMsduInfo->rPktProfile;

	/* Reset packet profile */
	prPktProfile->fgIsValid = FALSE;

#if CFG_ENABLE_PER_STA_STATISTICS
	nicTxLifetimeCheckByAC(prAdapter, prMsduInfo, prPacket, ucPriorityParam);
#endif

#if CFG_PRINT_RTP_PROFILE
	nicTxLifetimeCheckRTP(prAdapter, prMsduInfo, prPacket, u4PacketLen, ucNetworkType);
#endif

}

#endif

/*----------------------------------------------------------------------------*/
/*!
* @brief In this function, we'll write frame(PACKET_INFO_T) into HIF.
*
* @param prAdapter              Pointer to the Adapter structure.
* @param ucPortIdx              Port Number
* @param prQue                  a link list of P_MSDU_INFO_T
*
* @retval WLAN_STATUS_SUCCESS   Bus access ok.
* @retval WLAN_STATUS_FAILURE   Bus access fail.
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS nicTxMsduQueue(IN P_ADAPTER_T prAdapter, UINT_8 ucPortIdx, P_QUE_T prQue)
{
	P_MSDU_INFO_T prMsduInfo, prNextMsduInfo;
	HIF_TX_HEADER_T rHwTxHeader;
	P_NATIVE_PACKET prNativePacket;
	UINT_16 u2OverallBufferLength;
	UINT_8 ucEtherTypeOffsetInWord;
	PUINT_8 pucOutputBuf = (PUINT_8) NULL;	/* Pointer to Transmit Data Structure Frame */
	UINT_32 u4TxHdrSize;
	UINT_32 u4ValidBufSize;
	UINT_32 u4TotalLength;
	P_TX_CTRL_T prTxCtrl;
	QUE_T rFreeQueue;
#if CFG_TCP_IP_CHKSUM_OFFLOAD
	UINT_8 ucChksumFlag;
#endif

	ASSERT(prAdapter);
	ASSERT(ucPortIdx < 2);
	ASSERT(prQue);

	prTxCtrl = &prAdapter->rTxCtrl;
	u4ValidBufSize = prAdapter->u4CoalescingBufCachedSize;

#if CFG_HIF_STATISTICS
	prTxCtrl->u4TotalTxAccessNum++;
	prTxCtrl->u4TotalTxPacketNum += prQue->u4NumElem;
#endif

	QUEUE_INITIALIZE(&rFreeQueue);

	if (prQue->u4NumElem > 0) {
		prMsduInfo = (P_MSDU_INFO_T) QUEUE_GET_HEAD(prQue);
		pucOutputBuf = prTxCtrl->pucTxCoalescingBufPtr;
		u4TotalLength = 0;

		while (prMsduInfo) {

#if (CFG_SUPPORT_TDLS_DBG == 1)
			{
				struct sk_buff *prSkb = (struct sk_buff *)prMsduInfo->prPacket;
				UINT8 *pkt = prSkb->data;
				UINT16 u2Identifier;

				if ((*(pkt + 12) == 0x08) && (*(pkt + 13) == 0x00)) {
					/* ip */
					u2Identifier = ((*(pkt + 18)) << 8) | (*(pkt + 19));
					DBGLOG(TX, TRACE, "<hif> %d\n", u2Identifier);
				}
			}
#endif
#if (CFG_SUPPORT_MET_PROFILING == 1)
			kalMetProfilingFinish(prAdapter, prMsduInfo);
#endif
			kalMemZero(&rHwTxHeader, sizeof(rHwTxHeader));

			prNativePacket = prMsduInfo->prPacket;

			ASSERT(prNativePacket);

			u4TxHdrSize = TX_HDR_SIZE;

			u2OverallBufferLength = ((prMsduInfo->u2FrameLength + TX_HDR_SIZE) &
						 (UINT_16) HIF_TX_HDR_TX_BYTE_COUNT_MASK);

			/* init TX header */
			rHwTxHeader.u2TxByteCount_UserPriority = u2OverallBufferLength;
			rHwTxHeader.u2TxByteCount_UserPriority |=
			    ((UINT_16) prMsduInfo->ucUserPriority << HIF_TX_HDR_USER_PRIORITY_OFFSET);

			if (prMsduInfo->fgIs802_11) {
				ucEtherTypeOffsetInWord =
				    (TX_HDR_SIZE + prMsduInfo->ucMacHeaderLength + prMsduInfo->ucLlcLength) >> 1;
			} else {
				ucEtherTypeOffsetInWord = ((ETHER_HEADER_LEN - ETHER_TYPE_LEN) + TX_HDR_SIZE) >> 1;
			}

			rHwTxHeader.ucEtherTypeOffset = ucEtherTypeOffsetInWord & HIF_TX_HDR_ETHER_TYPE_OFFSET_MASK;

			rHwTxHeader.ucResource_PktType_CSflags = (prMsduInfo->ucTC) << HIF_TX_HDR_RESOURCE_OFFSET;
			rHwTxHeader.ucResource_PktType_CSflags |=
			    (UINT_8) (((prMsduInfo->ucPacketType) << HIF_TX_HDR_PACKET_TYPE_OFFSET) &
				      (HIF_TX_HDR_PACKET_TYPE_MASK));

#if CFG_TCP_IP_CHKSUM_OFFLOAD
			if (prMsduInfo->eSrc == TX_PACKET_OS || prMsduInfo->eSrc == TX_PACKET_FORWARDING) {
				if (prAdapter->u4CSUMFlags &
				    (CSUM_OFFLOAD_EN_TX_TCP | CSUM_OFFLOAD_EN_TX_UDP | CSUM_OFFLOAD_EN_TX_IP)) {
					kalQueryTxChksumOffloadParam(prNativePacket, &ucChksumFlag);

					if (ucChksumFlag & TX_CS_IP_GEN)
						rHwTxHeader.ucResource_PktType_CSflags |= (UINT_8) HIF_TX_HDR_IP_CSUM;

					if (ucChksumFlag & TX_CS_TCP_UDP_GEN)
						rHwTxHeader.ucResource_PktType_CSflags |= (UINT_8) HIF_TX_HDR_TCP_CSUM;
				}
			}
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */

			rHwTxHeader.u2LLH = prMsduInfo->u2PalLLH;
			rHwTxHeader.ucStaRecIdx = prMsduInfo->ucStaRecIndex;
			rHwTxHeader.ucForwardingType_SessionID_Reserved =
			    (prMsduInfo->ucPsForwardingType) | ((prMsduInfo->ucPsSessionID) <<
								HIF_TX_HDR_PS_SESSION_ID_OFFSET)
			    | ((prMsduInfo->fgIsBurstEnd) ? HIF_TX_HDR_BURST_END_MASK : 0);

			rHwTxHeader.ucWlanHeaderLength =
			    (prMsduInfo->ucMacHeaderLength & HIF_TX_HDR_WLAN_HEADER_LEN_MASK);
			rHwTxHeader.ucPktFormtId_Flags = (prMsduInfo->ucFormatID & HIF_TX_HDR_FORMAT_ID_MASK)
			    | ((prMsduInfo->ucNetworkType << HIF_TX_HDR_NETWORK_TYPE_OFFSET) &
			       HIF_TX_HDR_NETWORK_TYPE_MASK)
			    | ((prMsduInfo->fgIs802_1x << HIF_TX_HDR_FLAG_1X_FRAME_OFFSET) &
			       HIF_TX_HDR_FLAG_1X_FRAME_MASK)
			    | ((prMsduInfo->fgIs802_11 << HIF_TX_HDR_FLAG_802_11_FORMAT_OFFSET) &
			       HIF_TX_HDR_FLAG_802_11_FORMAT_MASK);

			rHwTxHeader.u2SeqNo = prMsduInfo->u2AclSN;

			if (prMsduInfo->pfTxDoneHandler) {
				rHwTxHeader.ucPacketSeqNo = prMsduInfo->ucTxSeqNum;
				rHwTxHeader.ucAck_BIP_BasicRate = HIF_TX_HDR_NEED_ACK;
			} else {
				rHwTxHeader.ucPacketSeqNo = 0;
				rHwTxHeader.ucAck_BIP_BasicRate = 0;
			}

			if (prMsduInfo->fgNeedTxDoneStatus == TRUE)
				rHwTxHeader.ucAck_BIP_BasicRate |= HIF_TX_HDR_NEED_TX_DONE_STATUS;

			if (prMsduInfo->fgIsBIP)
				rHwTxHeader.ucAck_BIP_BasicRate |= HIF_TX_HDR_BIP;

			if (prMsduInfo->fgIsBasicRate)
				rHwTxHeader.ucAck_BIP_BasicRate |= HIF_TX_HDR_BASIC_RATE;
#if CFG_ENABLE_PKT_LIFETIME_PROFILE
			if (prMsduInfo->rPktProfile.fgIsValid)
				prMsduInfo->rPktProfile.rDequeueTimestamp = kalGetTimeTick();
#endif

			/* record the queue time in driver */
			STATS_TX_TIME_TO_HIF(prMsduInfo, &rHwTxHeader);
			wlanFillTimestamp(prAdapter, prMsduInfo->prPacket, PHASE_HIF_TX);
#if CFG_SDIO_TX_AGG
			/* attach to coalescing buffer */
			kalMemCopy(pucOutputBuf + u4TotalLength, &rHwTxHeader, u4TxHdrSize);
			u4TotalLength += u4TxHdrSize;

			if (prMsduInfo->eSrc == TX_PACKET_OS || prMsduInfo->eSrc == TX_PACKET_FORWARDING)
				kalCopyFrame(prAdapter->prGlueInfo, prNativePacket, pucOutputBuf + u4TotalLength);
			else if (prMsduInfo->eSrc == TX_PACKET_MGMT)
				kalMemCopy(pucOutputBuf + u4TotalLength, prNativePacket, prMsduInfo->u2FrameLength);
			else
				ASSERT(0);

			u4TotalLength += ALIGN_4(prMsduInfo->u2FrameLength);

#else
			kalMemCopy(pucOutputBuf, &rHwTxHeader, u4TxHdrSize);

			/* Copy Frame Body */
			if (prMsduInfo->eSrc == TX_PACKET_OS || prMsduInfo->eSrc == TX_PACKET_FORWARDING)
				kalCopyFrame(prAdapter->prGlueInfo, prNativePacket, pucOutputBuf + u4TxHdrSize);
			else if (prMsduInfo->eSrc == TX_PACKET_MGMT)
				kalMemCopy(pucOutputBuf + u4TxHdrSize, prNativePacket, prMsduInfo->u2FrameLength);
			else
				ASSERT(0);

			ASSERT(u2OverallBufferLength <= u4ValidBufSize);

			HAL_WRITE_TX_PORT(prAdapter,
					  ucPortIdx,
					  (UINT_32) u2OverallBufferLength, (PUINT_8) pucOutputBuf, u4ValidBufSize);

			/* send immediately */
#endif
			prNextMsduInfo = (P_MSDU_INFO_T)
			    QUEUE_GET_NEXT_ENTRY(&prMsduInfo->rQueEntry);

			if (prMsduInfo->eSrc == TX_PACKET_MGMT) {
				GLUE_DEC_REF_CNT(prTxCtrl->i4TxMgmtPendingNum);

				if (prMsduInfo->pfTxDoneHandler == NULL) {
					cnmMgtPktFree(prAdapter, prMsduInfo);
				} else {
					KAL_SPIN_LOCK_DECLARATION();
					DBGLOG(TX, TRACE, "Wait TxSeqNum:%d\n", prMsduInfo->ucTxSeqNum);
					KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TXING_MGMT_LIST);
					QUEUE_INSERT_TAIL(&(prTxCtrl->rTxMgmtTxingQueue), (P_QUE_ENTRY_T) prMsduInfo);
					KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TXING_MGMT_LIST);
				}
			} else {
				/* only free MSDU when it is not a MGMT frame */
				QUEUE_INSERT_TAIL(&rFreeQueue, (P_QUE_ENTRY_T) prMsduInfo);

				if (prMsduInfo->eSrc == TX_PACKET_OS)
					kalSendComplete(prAdapter->prGlueInfo, prNativePacket, WLAN_STATUS_SUCCESS);
				else if (prMsduInfo->eSrc == TX_PACKET_FORWARDING)
					GLUE_DEC_REF_CNT(prTxCtrl->i4PendingFwdFrameCount);
			}

			prMsduInfo = prNextMsduInfo;
		}

#if CFG_SDIO_TX_AGG
		ASSERT(u4TotalLength <= u4ValidBufSize);

#if CFG_DBG_GPIO_PINS
		{
			/* Start port write */
			mtk_wcn_stp_debug_gpio_assert(IDX_TX_PORT_WRITE, DBG_TIE_LOW);
			kalUdelay(1);
			mtk_wcn_stp_debug_gpio_assert(IDX_TX_PORT_WRITE, DBG_TIE_HIGH);
		}
#endif

		/* send coalescing buffer */
		HAL_WRITE_TX_PORT(prAdapter, ucPortIdx, u4TotalLength, (PUINT_8) pucOutputBuf, u4ValidBufSize);
#endif

#if CFG_ENABLE_PKT_LIFETIME_PROFILE
#if CFG_SUPPORT_WFD && CFG_PRINT_RTP_PROFILE && !CFG_ENABLE_PER_STA_STATISTICS
		do {
			P_WFD_CFG_SETTINGS_T prWfdCfgSettings = (P_WFD_CFG_SETTINGS_T) NULL;

			prWfdCfgSettings = &prAdapter->rWifiVar.prP2pFsmInfo->rWfdConfigureSettings;

			if ((prWfdCfgSettings->u2WfdMaximumTp >= 0xF000)) {
				/* Enable profiling */
				nicTxReturnMsduInfoProfiling(prAdapter, (P_MSDU_INFO_T) QUEUE_GET_HEAD(&rFreeQueue));
			} else {
				/* Skip profiling */
				nicTxReturnMsduInfo(prAdapter, (P_MSDU_INFO_T) QUEUE_GET_HEAD(&rFreeQueue));
			}
		} while (FALSE);
#else
		nicTxReturnMsduInfoProfiling(prAdapter, (P_MSDU_INFO_T) QUEUE_GET_HEAD(&rFreeQueue));
#endif
#else
		/* return */
		nicTxReturnMsduInfo(prAdapter, (P_MSDU_INFO_T) QUEUE_GET_HEAD(&rFreeQueue));
#endif
	}

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief In this function, we'll write Command(CMD_INFO_T) into HIF.
*
* @param prAdapter      Pointer to the Adapter structure.
* @param prPacketInfo   Pointer of CMD_INFO_T
* @param ucTC           Specify the resource of TC
*
* @retval WLAN_STATUS_SUCCESS   Bus access ok.
* @retval WLAN_STATUS_FAILURE   Bus access fail.
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS nicTxCmd(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN UINT_8 ucTC)
{
	P_WIFI_CMD_T prWifiCmd;
	UINT_16 u2OverallBufferLength;
	PUINT_8 pucOutputBuf = (PUINT_8) NULL;	/* Pointer to Transmit Data Structure Frame */
	UINT_8 ucPortIdx;
	HIF_TX_HEADER_T rHwTxHeader;
	P_NATIVE_PACKET prNativePacket;
	UINT_8 ucEtherTypeOffsetInWord;
	P_MSDU_INFO_T prMsduInfo;
	P_TX_CTRL_T prTxCtrl;
	BOOLEAN fgScanReqCmd = FALSE;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	prTxCtrl = &prAdapter->rTxCtrl;
	pucOutputBuf = prTxCtrl->pucTxCoalescingBufPtr;

	/* <1> Assign Data Port */
	if (ucTC != TC4_INDEX) {
		ucPortIdx = 0;
	} else {
		/* Broadcast/multicast data frames, 1x frames, command packets, MMPDU */
		ucPortIdx = 1;
	}
	wlanTraceTxCmd(prAdapter, prCmdInfo);

	if (prCmdInfo->eCmdType == COMMAND_TYPE_SECURITY_FRAME) {
		/* <2> Compose HIF_TX_HEADER */
		kalMemZero(&rHwTxHeader, sizeof(rHwTxHeader));

		prNativePacket = prCmdInfo->prPacket;

		ASSERT(prNativePacket);

		u2OverallBufferLength = TFCB_FRAME_PAD_TO_DW((prCmdInfo->u2InfoBufLen + TX_HDR_SIZE)
							     & (UINT_16) HIF_TX_HDR_TX_BYTE_COUNT_MASK);

		rHwTxHeader.u2TxByteCount_UserPriority = ((prCmdInfo->u2InfoBufLen + TX_HDR_SIZE)
							  & (UINT_16) HIF_TX_HDR_TX_BYTE_COUNT_MASK);
		ucEtherTypeOffsetInWord = ((ETHER_HEADER_LEN - ETHER_TYPE_LEN) + TX_HDR_SIZE) >> 1;

		rHwTxHeader.ucEtherTypeOffset = ucEtherTypeOffsetInWord & HIF_TX_HDR_ETHER_TYPE_OFFSET_MASK;

		rHwTxHeader.ucResource_PktType_CSflags = (ucTC << HIF_TX_HDR_RESOURCE_OFFSET);

		rHwTxHeader.ucStaRecIdx = prCmdInfo->ucStaRecIndex;
		rHwTxHeader.ucForwardingType_SessionID_Reserved = HIF_TX_HDR_BURST_END_MASK;

		rHwTxHeader.ucWlanHeaderLength = (ETH_HLEN & HIF_TX_HDR_WLAN_HEADER_LEN_MASK);
		rHwTxHeader.ucPktFormtId_Flags =
		    (((UINT_8) (prCmdInfo->eNetworkType) << HIF_TX_HDR_NETWORK_TYPE_OFFSET) &
		     HIF_TX_HDR_NETWORK_TYPE_MASK)
		    | ((1 << HIF_TX_HDR_FLAG_1X_FRAME_OFFSET) & HIF_TX_HDR_FLAG_1X_FRAME_MASK);

		rHwTxHeader.u2SeqNo = 0;
		rHwTxHeader.ucPacketSeqNo = 0;
		rHwTxHeader.ucAck_BIP_BasicRate = HIF_TX_HDR_NEED_TX_DONE_STATUS;
		rHwTxHeader.ucAck_BIP_BasicRate |= HIF_TX_HDR_BASIC_RATE /* | HIF_TX_HDR_RTS */;

		/* <2.3> Copy HIF TX HEADER */
		kalMemCopy((PVOID)&pucOutputBuf[0], (PVOID)&rHwTxHeader, TX_HDR_SIZE);

		/* <3> Copy Frame Body Copy */
		kalCopyFrame(prAdapter->prGlueInfo, prNativePacket, pucOutputBuf + TX_HDR_SIZE);
	} else if (prCmdInfo->eCmdType == COMMAND_TYPE_MANAGEMENT_FRAME) {
		prMsduInfo = (P_MSDU_INFO_T) prCmdInfo->prPacket;

		ASSERT(prMsduInfo->fgIs802_11 == TRUE);
		ASSERT(prMsduInfo->eSrc == TX_PACKET_MGMT);

		/* <2> Compose HIF_TX_HEADER */
		kalMemZero(&rHwTxHeader, sizeof(rHwTxHeader));

		u2OverallBufferLength = ((prMsduInfo->u2FrameLength + TX_HDR_SIZE) &
					 (UINT_16) HIF_TX_HDR_TX_BYTE_COUNT_MASK);

		rHwTxHeader.u2TxByteCount_UserPriority = u2OverallBufferLength;
		rHwTxHeader.u2TxByteCount_UserPriority |=
		    ((UINT_16) prMsduInfo->ucUserPriority << HIF_TX_HDR_USER_PRIORITY_OFFSET);

		ucEtherTypeOffsetInWord = (TX_HDR_SIZE + prMsduInfo->ucMacHeaderLength + prMsduInfo->ucLlcLength) >> 1;

		rHwTxHeader.ucEtherTypeOffset = ucEtherTypeOffsetInWord & HIF_TX_HDR_ETHER_TYPE_OFFSET_MASK;

		rHwTxHeader.ucResource_PktType_CSflags = (prMsduInfo->ucTC) << HIF_TX_HDR_RESOURCE_OFFSET;
		rHwTxHeader.ucResource_PktType_CSflags |=
		    (UINT_8) (((prMsduInfo->ucPacketType) << HIF_TX_HDR_PACKET_TYPE_OFFSET) &
			      (HIF_TX_HDR_PACKET_TYPE_MASK));

		rHwTxHeader.u2LLH = prMsduInfo->u2PalLLH;
		rHwTxHeader.ucStaRecIdx = prMsduInfo->ucStaRecIndex;
		rHwTxHeader.ucForwardingType_SessionID_Reserved =
		    (prMsduInfo->ucPsForwardingType) | ((prMsduInfo->ucPsSessionID) << HIF_TX_HDR_PS_SESSION_ID_OFFSET)
		    | ((prMsduInfo->fgIsBurstEnd) ? HIF_TX_HDR_BURST_END_MASK : 0);

		rHwTxHeader.ucWlanHeaderLength = (prMsduInfo->ucMacHeaderLength & HIF_TX_HDR_WLAN_HEADER_LEN_MASK);
		rHwTxHeader.ucPktFormtId_Flags = (prMsduInfo->ucFormatID & HIF_TX_HDR_FORMAT_ID_MASK)
		    | ((prMsduInfo->ucNetworkType << HIF_TX_HDR_NETWORK_TYPE_OFFSET) & HIF_TX_HDR_NETWORK_TYPE_MASK)
		    | ((prMsduInfo->fgIs802_1x << HIF_TX_HDR_FLAG_1X_FRAME_OFFSET) & HIF_TX_HDR_FLAG_1X_FRAME_MASK)
		    | ((prMsduInfo->fgIs802_11 << HIF_TX_HDR_FLAG_802_11_FORMAT_OFFSET) &
		       HIF_TX_HDR_FLAG_802_11_FORMAT_MASK);

		rHwTxHeader.u2SeqNo = prMsduInfo->u2AclSN;

		if (prMsduInfo->pfTxDoneHandler) {
			rHwTxHeader.ucPacketSeqNo = prMsduInfo->ucTxSeqNum;
			rHwTxHeader.ucAck_BIP_BasicRate = HIF_TX_HDR_NEED_ACK;
		} else {
			rHwTxHeader.ucPacketSeqNo = 0;
			rHwTxHeader.ucAck_BIP_BasicRate = 0;
		}

		if (prMsduInfo->fgIsBIP)
			rHwTxHeader.ucAck_BIP_BasicRate |= HIF_TX_HDR_BIP;

		if (prMsduInfo->fgIsBasicRate)
			rHwTxHeader.ucAck_BIP_BasicRate |= HIF_TX_HDR_BASIC_RATE;
		/* <2.3> Copy HIF TX HEADER */
		kalMemCopy((PVOID)&pucOutputBuf[0], (PVOID)&rHwTxHeader, TX_HDR_SIZE);

		/* <3> Copy Frame Body */
		kalMemCopy(pucOutputBuf + TX_HDR_SIZE, prMsduInfo->prPacket, prMsduInfo->u2FrameLength);

		/* <4> Management Frame Post-Processing */
		GLUE_DEC_REF_CNT(prTxCtrl->i4TxMgmtPendingNum);

		if (prMsduInfo->pfTxDoneHandler == NULL) {
			cnmMgtPktFree(prAdapter, prMsduInfo);
		} else {

			DBGLOG(TX, TRACE, "Wait Cmd TxSeqNum:%d\n", prMsduInfo->ucTxSeqNum);

			KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TXING_MGMT_LIST);
			QUEUE_INSERT_TAIL(&(prTxCtrl->rTxMgmtTxingQueue), (P_QUE_ENTRY_T) prMsduInfo);
			KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TXING_MGMT_LIST);
		}
	} else {
		prWifiCmd = (P_WIFI_CMD_T) prCmdInfo->pucInfoBuffer;

		/* <2> Compose the Header of Transmit Data Structure for CMD Packet */
		u2OverallBufferLength =
		    TFCB_FRAME_PAD_TO_DW((prCmdInfo->u2InfoBufLen) & (UINT_16) HIF_TX_HDR_TX_BYTE_COUNT_MASK);

		prWifiCmd->u2TxByteCount_UserPriority = u2OverallBufferLength;
		prWifiCmd->ucEtherTypeOffset = 0;
		prWifiCmd->ucResource_PktType_CSflags = (ucTC << HIF_TX_HDR_RESOURCE_OFFSET)
		    | (UINT_8) ((HIF_TX_PKT_TYPE_CMD << HIF_TX_HDR_PACKET_TYPE_OFFSET) & (HIF_TX_HDR_PACKET_TYPE_MASK));

		/* <3> Copy CMD Header to command buffer (by using pucCoalescingBufCached) */
		kalMemCopy((PVOID)&pucOutputBuf[0], (PVOID) prCmdInfo->pucInfoBuffer, prCmdInfo->u2InfoBufLen);

		ASSERT(u2OverallBufferLength <= prAdapter->u4CoalescingBufCachedSize);

		if ((prCmdInfo->ucCID == CMD_ID_SCAN_REQ) ||
			(prCmdInfo->ucCID == CMD_ID_SCAN_CANCEL) ||
			(prCmdInfo->ucCID == CMD_ID_SCAN_REQ_V2)) {
			DBGLOG(TX, INFO, "ucCmdSeqNum =%d, ucCID =%d\n", prCmdInfo->ucCmdSeqNum, prCmdInfo->ucCID);
			/*record scan request tx_desciption*/
			wlanDebugScanRecord(prAdapter
				, DBG_SCAN_WRITE_BEFORE);
			fgScanReqCmd = TRUE;

		}
	}
	prCmdInfo->u4SendToFwTime = kalGetTimeTick();
	wlanDebugCommandRecodTime(prCmdInfo);

	/* <4> Write frame to data port */
	HAL_WRITE_TX_PORT(prAdapter,
			  ucPortIdx,
			  (UINT_32) u2OverallBufferLength,
			  (PUINT_8) pucOutputBuf, (UINT_32) prAdapter->u4CoalescingBufCachedSize);
	if (fgScanReqCmd == TRUE)
		/*record scan request tx_desciption*/
		wlanDebugScanRecord(prAdapter
			, DBG_SCAN_WRITE_DONE);

	return WLAN_STATUS_SUCCESS;
}				/* end of nicTxCmd() */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will clean up all the pending frames in internal SW Queues
*        by return the pending TX packet to the system.
*
* @param prAdapter  Pointer to the Adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID nicTxRelease(IN P_ADAPTER_T prAdapter)
{
	P_TX_CTRL_T prTxCtrl;
	P_MSDU_INFO_T prMsduInfo;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);

	prTxCtrl = &prAdapter->rTxCtrl;

	nicTxFlush(prAdapter);

	/* free MSDU_INFO_T from rTxMgmtMsduInfoList */
	do {
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TXING_MGMT_LIST);
		QUEUE_REMOVE_HEAD(&prTxCtrl->rTxMgmtTxingQueue, prMsduInfo, P_MSDU_INFO_T);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TXING_MGMT_LIST);

		if (prMsduInfo) {
			/* the packet must be mgmt frame with tx done callback */
			ASSERT(prMsduInfo->eSrc == TX_PACKET_MGMT);

			/* invoke done handler */
			prMsduInfo->pfTxDoneHandler(prAdapter, prMsduInfo, TX_RESULT_LIFE_TIMEOUT);

			cnmMgtPktFree(prAdapter, prMsduInfo);
		} else {
			break;
		}
	} while (TRUE);

}				/* end of nicTxRelease() */

/*----------------------------------------------------------------------------*/
/*!
* @brief Process the TX Done interrupt and pull in more pending frames in SW
*        Queues for transmission.
*
* @param prAdapter  Pointer to the Adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID nicProcessTxInterrupt(IN P_ADAPTER_T prAdapter)
{
	P_TX_CTRL_T prTxCtrl;
#if CFG_SDIO_INTR_ENHANCE
	P_SDIO_CTRL_T prSDIOCtrl;
#else
	UINT_32 au4TxCount[2];
#endif /* CFG_SDIO_INTR_ENHANCE */

	P_GLUE_INFO_T prGlueInfo = prAdapter->prGlueInfo;

	prTxCtrl = &prAdapter->rTxCtrl;
	ASSERT(prTxCtrl);
	prGlueInfo->IsrTxCnt++;

	/* Get the TX STATUS */
#if CFG_SDIO_INTR_ENHANCE

	prSDIOCtrl = prAdapter->prSDIOCtrl;
#if DBG
	/* dumpMemory8((PUINT_8)prSDIOCtrl, sizeof(SDIO_CTRL_T)); */
#endif

	nicTxReleaseResource(prAdapter, (PUINT_8) &prSDIOCtrl->rTxInfo);
	kalMemZero(&prSDIOCtrl->rTxInfo, sizeof(prSDIOCtrl->rTxInfo));

#else

	HAL_MCR_RD(prAdapter, MCR_WTSR0, &au4TxCount[0]);
	HAL_MCR_RD(prAdapter, MCR_WTSR1, &au4TxCount[1]);
	DBGLOG(EMU, TRACE, "MCR_WTSR0: 0x%x, MCR_WTSR1: 0x%x\n", au4TxCount[0], au4TxCount[1]);

	nicTxReleaseResource(prAdapter, (PUINT_8) au4TxCount);

#endif /* CFG_SDIO_INTR_ENHANCE */

	nicTxAdjustTcq(prAdapter);

	/* Indicate Service Thread */
	if (kalGetTxPendingCmdCount(prAdapter->prGlueInfo) > 0 || wlanGetTxPendingFrameCount(prAdapter) > 0)
		kalSetEvent(prAdapter->prGlueInfo);

}				/* end of nicProcessTxInterrupt() */

/*----------------------------------------------------------------------------*/
/*!
* @brief this function frees packet of P_MSDU_INFO_T linked-list
*
* @param prAdapter              Pointer to the Adapter structure.
* @param prMsduInfoList         a link list of P_MSDU_INFO_T
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID nicTxFreeMsduInfoPacket(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfoListHead)
{
	P_NATIVE_PACKET prNativePacket;
	P_MSDU_INFO_T prMsduInfo = prMsduInfoListHead;
	P_TX_CTRL_T prTxCtrl;

	ASSERT(prAdapter);
	ASSERT(prMsduInfoListHead);

	prTxCtrl = &prAdapter->rTxCtrl;

	while (prMsduInfo) {
		prNativePacket = prMsduInfo->prPacket;

		if (prMsduInfo->eSrc == TX_PACKET_OS) {
			kalSendComplete(prAdapter->prGlueInfo, prNativePacket, WLAN_STATUS_FAILURE);
		} else if (prMsduInfo->eSrc == TX_PACKET_MGMT) {
			P_MSDU_INFO_T prTempMsduInfo = prMsduInfo;

			if (prMsduInfo->pfTxDoneHandler)
				prMsduInfo->pfTxDoneHandler(prAdapter, prMsduInfo, TX_RESULT_DROPPED_IN_DRIVER);
			prMsduInfo = (P_MSDU_INFO_T) QUEUE_GET_NEXT_ENTRY((P_QUE_ENTRY_T) prMsduInfo);
			cnmMgtPktFree(prAdapter, prTempMsduInfo);
			continue;
		} else if (prMsduInfo->eSrc == TX_PACKET_FORWARDING) {
			GLUE_DEC_REF_CNT(prTxCtrl->i4PendingFwdFrameCount);
		}

		prMsduInfo = (P_MSDU_INFO_T) QUEUE_GET_NEXT_ENTRY((P_QUE_ENTRY_T) prMsduInfo);
	}

}

/*----------------------------------------------------------------------------*/
/*!
* @brief this function returns P_MSDU_INFO_T of MsduInfoList to TxCtrl->rfreeMsduInfoList
*
* @param prAdapter              Pointer to the Adapter structure.
* @param prMsduInfoList         a link list of P_MSDU_INFO_T
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID nicTxReturnMsduInfo(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfoListHead)
{
	P_TX_CTRL_T prTxCtrl;
	P_MSDU_INFO_T prMsduInfo = prMsduInfoListHead, prNextMsduInfo;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);

	prTxCtrl = &prAdapter->rTxCtrl;
	ASSERT(prTxCtrl);

	while (prMsduInfo) {
		prNextMsduInfo = (P_MSDU_INFO_T) QUEUE_GET_NEXT_ENTRY((P_QUE_ENTRY_T) prMsduInfo);

		switch (prMsduInfo->eSrc) {
		case TX_PACKET_FORWARDING:
			wlanReturnPacket(prAdapter, prMsduInfo->prPacket);
			break;
		case TX_PACKET_OS:
		case TX_PACKET_OS_OID:
		case TX_PACKET_MGMT:
		default:
			break;
		}

		/* Reset MSDU_INFO fields */
		kalMemZero(prMsduInfo, sizeof(MSDU_INFO_T));

		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_MSDU_INFO_LIST);
		QUEUE_INSERT_TAIL(&prTxCtrl->rFreeMsduInfoList, (P_QUE_ENTRY_T) prMsduInfo);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_MSDU_INFO_LIST);
		prMsduInfo = prNextMsduInfo;
	};

}

/*----------------------------------------------------------------------------*/
/*!
* @brief this function fills packet information to P_MSDU_INFO_T
*
* @param prAdapter              Pointer to the Adapter structure.
* @param prMsduInfo             P_MSDU_INFO_T
* @param prPacket               P_NATIVE_PACKET
*
* @retval TRUE      Success to extract information
* @retval FALSE     Fail to extract correct information
*/
/*----------------------------------------------------------------------------*/
BOOLEAN nicTxFillMsduInfo(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo, IN P_NATIVE_PACKET prPacket)
{
	P_GLUE_INFO_T prGlueInfo;
	UINT_8 ucPriorityParam = 0;
	UINT_8 ucMacHeaderLen;
	UINT_8 aucEthDestAddr[PARAM_MAC_ADDR_LEN] = {0};
	BOOLEAN fgIs1x = FALSE;
	BOOLEAN fgIsPAL = FALSE;
	UINT_32 u4PacketLen = 0;
	ULONG u4SysTime;
	UINT_8 ucNetworkType = 0;
	struct sk_buff *prSkb = (struct sk_buff *)prPacket;

	ASSERT(prAdapter);

	prGlueInfo = prAdapter->prGlueInfo;
	ASSERT(prGlueInfo);

	if (kalQoSFrameClassifierAndPacketInfo(prGlueInfo,
					       prPacket,
					       &ucPriorityParam,
					       &u4PacketLen,
					       aucEthDestAddr,
					       &fgIs1x, &fgIsPAL, &ucNetworkType,
					       NULL) == FALSE) {
		DBGLOG(TX, WARN, "%s kalQoSFrameClassifierAndPacketInfo is false!\n", __func__);
		return FALSE;
	}
#if CFG_ENABLE_PKT_LIFETIME_PROFILE
	nicTxLifetimeCheck(prAdapter, prMsduInfo, prPacket, ucPriorityParam, u4PacketLen, ucNetworkType);
#endif

#if (CFG_SUPPORT_TDLS == 1)
	if (ucNetworkType == NETWORK_TYPE_P2P_INDEX &&
	    MTKTdlsEnvP2P(prAdapter))
		MTKAutoTdlsP2P(prGlueInfo, prPacket);
#endif
	/* Save the value of Priority Parameter */
	GLUE_SET_PKT_TID(prPacket, ucPriorityParam);

	if (fgIs1x)
		GLUE_SET_PKT_FLAG_1X(prPacket);

	if (fgIsPAL)
		GLUE_SET_PKT_FLAG_PAL(prPacket);

	ucMacHeaderLen = ETH_HLEN;

	/* Save the value of Header Length */
	GLUE_SET_PKT_HEADER_LEN(prPacket, ucMacHeaderLen);

	/* Save the value of Frame Length */
	GLUE_SET_PKT_FRAME_LEN(prPacket, (UINT_16) u4PacketLen);

	/* Save the value of Arrival Time */
	u4SysTime = (OS_SYSTIME) kalGetTimeTick();
	GLUE_SET_PKT_ARRIVAL_TIME(prPacket, u4SysTime);

	prMsduInfo->prPacket = prPacket;
	prMsduInfo->fgIs802_1x = fgIs1x;
	prMsduInfo->fgIs802_11 = FALSE;
	prMsduInfo->ucNetworkType = ucNetworkType;
	prMsduInfo->ucUserPriority = ucPriorityParam;
	prMsduInfo->ucMacHeaderLength = ucMacHeaderLen;
	prMsduInfo->u2FrameLength = (UINT_16) u4PacketLen;
	COPY_MAC_ADDR(prMsduInfo->aucEthDestAddr, aucEthDestAddr);

	if (prSkb->len > ETH_HLEN)
		STATS_TX_PKT_CALLBACK(prSkb->data, prMsduInfo);
	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief this function update TCQ values by passing current status to txAdjustTcQuotas
*
* @param prAdapter              Pointer to the Adapter structure.
*
* @retval WLAN_STATUS_SUCCESS   Updated successfully
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS nicTxAdjustTcq(IN P_ADAPTER_T prAdapter)
{
	UINT_32 u4Num;
	TX_TCQ_ADJUST_T rTcqAdjust;
	P_TX_CTRL_T prTxCtrl;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);

	prTxCtrl = &prAdapter->rTxCtrl;
	ASSERT(prTxCtrl);

	qmAdjustTcQuotas(prAdapter, &rTcqAdjust, &prTxCtrl->rTc);
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_RESOURCE);

	for (u4Num = 0; u4Num < TC_NUM; u4Num++) {
		prTxCtrl->rTc.aucFreeBufferCount[u4Num] += rTcqAdjust.acVariation[u4Num];
		prTxCtrl->rTc.aucMaxNumOfBuffer[u4Num] += rTcqAdjust.acVariation[u4Num];
	}

	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_RESOURCE);

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief this function flushes all packets queued in STA/AC queue
*
* @param prAdapter              Pointer to the Adapter structure.
*
* @retval WLAN_STATUS_SUCCESS   Flushed successfully
*/
/*----------------------------------------------------------------------------*/

WLAN_STATUS nicTxFlush(IN P_ADAPTER_T prAdapter)
{
	P_MSDU_INFO_T prMsduInfo;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);

	/* ask Per STA/AC queue to be fllushed and return all queued packets */
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_QM_TX_QUEUE);
	prMsduInfo = qmFlushTxQueues(prAdapter);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_QM_TX_QUEUE);

	if (prMsduInfo != NULL) {
		nicTxFreeMsduInfoPacket(prAdapter, prMsduInfo);
		nicTxReturnMsduInfo(prAdapter, prMsduInfo);
	}

	return WLAN_STATUS_SUCCESS;
}

#if CFG_ENABLE_FW_DOWNLOAD
/*----------------------------------------------------------------------------*/
/*!
* \brief In this function, we'll write Command(CMD_INFO_T) into HIF.
*        However this function is used for INIT_CMD.
*
*        In order to avoid further maintenance issues, these 2 functions are separated
*
* @param prAdapter      Pointer to the Adapter structure.
* @param prPacketInfo   Pointer of CMD_INFO_T
* @param ucTC           Specify the resource of TC
*
* @retval WLAN_STATUS_SUCCESS   Bus access ok.
* @retval WLAN_STATUS_FAILURE   Bus access fail.
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS nicTxInitCmd(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN UINT_8 ucTC)
{
	P_INIT_HIF_TX_HEADER_T prInitTxHeader;
	UINT_16 u2OverallBufferLength;
	PUINT_8 pucOutputBuf = (PUINT_8) NULL;	/* Pointer to Transmit Data Structure Frame */
	UINT_32 ucPortIdx;
	P_TX_CTRL_T prTxCtrl;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	ASSERT(ucTC == TC0_INDEX);

	prTxCtrl = &prAdapter->rTxCtrl;
	pucOutputBuf = prTxCtrl->pucTxCoalescingBufPtr;
	prInitTxHeader = (P_INIT_HIF_TX_HEADER_T) prCmdInfo->pucInfoBuffer;

	/* <1> Compose the Header of Transmit Data Structure for CMD Packet */
	u2OverallBufferLength =
	    TFCB_FRAME_PAD_TO_DW((prCmdInfo->u2InfoBufLen) & (UINT_16) HIF_TX_HDR_TX_BYTE_COUNT_MASK);

	prInitTxHeader->u2TxByteCount = u2OverallBufferLength;
	prInitTxHeader->ucEtherTypeOffset = 0;
	prInitTxHeader->ucCSflags = 0;

	/* <2> Assign Data Port */
	if (ucTC != TC4_INDEX) {
		ucPortIdx = 0;
	} else {		/* Broadcast/multicast data packets */
		ucPortIdx = 1;
	}

	/* <3> Copy CMD Header to command buffer (by using pucCoalescingBufCached) */
	kalMemCopy((PVOID)&pucOutputBuf[0], (PVOID) prCmdInfo->pucInfoBuffer, prCmdInfo->u2InfoBufLen);

	ASSERT(u2OverallBufferLength <= prAdapter->u4CoalescingBufCachedSize);

	/* <4> Write frame to data port */
	HAL_WRITE_TX_PORT(prAdapter,
			  ucPortIdx,
			  (UINT_32) u2OverallBufferLength,
			  (PUINT_8) pucOutputBuf, (UINT_32) prAdapter->u4CoalescingBufCachedSize);

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief In this function, we'll reset TX resource counter to initial value used
*        in F/W download state
*
* @param prAdapter      Pointer to the Adapter structure.
*
* @retval WLAN_STATUS_SUCCESS   Reset is done successfully.
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS nicTxInitResetResource(IN P_ADAPTER_T prAdapter)
{
	P_TX_CTRL_T prTxCtrl;

	DEBUGFUNC("nicTxInitResetResource");

	ASSERT(prAdapter);
	prTxCtrl = &prAdapter->rTxCtrl;

	prTxCtrl->rTc.aucMaxNumOfBuffer[TC0_INDEX] = NIC_TX_INIT_BUFF_COUNT_TC0;
	prTxCtrl->rTc.aucFreeBufferCount[TC0_INDEX] = NIC_TX_INIT_BUFF_COUNT_TC0;

	prTxCtrl->rTc.aucMaxNumOfBuffer[TC1_INDEX] = NIC_TX_INIT_BUFF_COUNT_TC1;
	prTxCtrl->rTc.aucFreeBufferCount[TC1_INDEX] = NIC_TX_INIT_BUFF_COUNT_TC1;

	prTxCtrl->rTc.aucMaxNumOfBuffer[TC2_INDEX] = NIC_TX_INIT_BUFF_COUNT_TC2;
	prTxCtrl->rTc.aucFreeBufferCount[TC2_INDEX] = NIC_TX_INIT_BUFF_COUNT_TC2;

	prTxCtrl->rTc.aucMaxNumOfBuffer[TC3_INDEX] = NIC_TX_INIT_BUFF_COUNT_TC3;
	prTxCtrl->rTc.aucFreeBufferCount[TC3_INDEX] = NIC_TX_INIT_BUFF_COUNT_TC3;

	prTxCtrl->rTc.aucMaxNumOfBuffer[TC4_INDEX] = NIC_TX_INIT_BUFF_COUNT_TC4;
	prTxCtrl->rTc.aucFreeBufferCount[TC4_INDEX] = NIC_TX_INIT_BUFF_COUNT_TC4;

	prTxCtrl->rTc.aucMaxNumOfBuffer[TC5_INDEX] = NIC_TX_INIT_BUFF_COUNT_TC5;
	prTxCtrl->rTc.aucFreeBufferCount[TC5_INDEX] = NIC_TX_INIT_BUFF_COUNT_TC5;

	return WLAN_STATUS_SUCCESS;

}

#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief this function enqueues MSDU_INFO_T into queue management,
*        or command queue
*
* @param prAdapter      Pointer to the Adapter structure.
*        prMsduInfo     Pointer to MSDU
*
* @retval WLAN_STATUS_SUCCESS   Reset is done successfully.
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS nicTxEnqueueMsdu(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo)
{
	P_TX_CTRL_T prTxCtrl;
	P_MSDU_INFO_T prNextMsduInfo, prRetMsduInfo, prMsduInfoHead;
	QUE_T qDataPort0, qDataPort1;
	P_CMD_INFO_T prCmdInfo;
	WLAN_STATUS u4Status = WLAN_STATUS_SUCCESS;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

	prTxCtrl = &prAdapter->rTxCtrl;
	ASSERT(prTxCtrl);

	QUEUE_INITIALIZE(&qDataPort0);
	QUEUE_INITIALIZE(&qDataPort1);

	/* check how many management frame are being queued */
	while (prMsduInfo) {
		prNextMsduInfo = (P_MSDU_INFO_T) QUEUE_GET_NEXT_ENTRY((P_QUE_ENTRY_T) prMsduInfo);

		QUEUE_GET_NEXT_ENTRY((P_QUE_ENTRY_T) prMsduInfo) = NULL;

		if (prMsduInfo->eSrc == TX_PACKET_MGMT) {
			/* MMPDU: force stick to TC4 */
			prMsduInfo->ucTC = TC4_INDEX;

			QUEUE_INSERT_TAIL(&qDataPort1, (P_QUE_ENTRY_T) prMsduInfo);
		} else {
			QUEUE_INSERT_TAIL(&qDataPort0, (P_QUE_ENTRY_T) prMsduInfo);
		}

		prMsduInfo = prNextMsduInfo;
	}

	if (qDataPort0.u4NumElem) {
		/* send to QM: queue the packet to different TX queue by policy */
		KAL_SPIN_LOCK_DECLARATION();
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_QM_TX_QUEUE);
		prRetMsduInfo = qmEnqueueTxPackets(prAdapter, (P_MSDU_INFO_T) QUEUE_GET_HEAD(&qDataPort0));
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_QM_TX_QUEUE);

		/* post-process for "dropped" packets */
		if (prRetMsduInfo != NULL) {	/* unable to enqueue */
			nicTxFreeMsduInfoPacket(prAdapter, prRetMsduInfo);
			nicTxReturnMsduInfo(prAdapter, prRetMsduInfo);
		}
	}

	if (qDataPort1.u4NumElem) {
		prMsduInfoHead = (P_MSDU_INFO_T) QUEUE_GET_HEAD(&qDataPort1);

		if (qDataPort1.u4NumElem > nicTxGetFreeCmdCount(prAdapter)) {
			/* not enough descriptors for sending */
			u4Status = WLAN_STATUS_FAILURE;

			/* free all MSDUs */
			while (prMsduInfoHead) {
				prNextMsduInfo = (P_MSDU_INFO_T) QUEUE_GET_NEXT_ENTRY(&prMsduInfoHead->rQueEntry);

				if (prMsduInfoHead->pfTxDoneHandler != NULL) {
					prMsduInfoHead->pfTxDoneHandler(prAdapter, prMsduInfoHead,
									TX_RESULT_DROPPED_IN_DRIVER);
				}

				cnmMgtPktFree(prAdapter, prMsduInfoHead);

				prMsduInfoHead = prNextMsduInfo;
			}
		} else {
			/* send to command queue */
			while (prMsduInfoHead) {
				prNextMsduInfo = (P_MSDU_INFO_T) QUEUE_GET_NEXT_ENTRY(&prMsduInfoHead->rQueEntry);

				KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_RESOURCE);
				QUEUE_REMOVE_HEAD(&prAdapter->rFreeCmdList, prCmdInfo, P_CMD_INFO_T);
				KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_RESOURCE);

				if (prCmdInfo) {
					GLUE_INC_REF_CNT(prTxCtrl->i4TxMgmtPendingNum);

					kalMemZero(prCmdInfo, sizeof(CMD_INFO_T));

					prCmdInfo->eCmdType = COMMAND_TYPE_MANAGEMENT_FRAME;
					prCmdInfo->u2InfoBufLen = prMsduInfoHead->u2FrameLength;
					prCmdInfo->pucInfoBuffer = NULL;
					prCmdInfo->prPacket = (P_NATIVE_PACKET) prMsduInfoHead;
					prCmdInfo->ucStaRecIndex = prMsduInfoHead->ucStaRecIndex;
					prCmdInfo->eNetworkType = prMsduInfoHead->ucNetworkType;
					prCmdInfo->pfCmdDoneHandler = NULL;
					prCmdInfo->pfCmdTimeoutHandler = NULL;
					prCmdInfo->fgIsOid = FALSE;
					prCmdInfo->fgSetQuery = TRUE;
					prCmdInfo->fgNeedResp = FALSE;

					kalEnqueueCommand(prAdapter->prGlueInfo, (P_QUE_ENTRY_T) prCmdInfo);
				} else {
					/* Cmd free count is larger than expected, but allocation fail. */
					ASSERT(0);

					u4Status = WLAN_STATUS_FAILURE;
					cnmMgtPktFree(prAdapter, prMsduInfoHead);
				}

				prMsduInfoHead = prNextMsduInfo;
			}
		}
	}

	/* indicate service thread for sending */
	if (prTxCtrl->i4TxMgmtPendingNum > 0 || kalGetTxPendingFrameCount(prAdapter->prGlueInfo) > 0)
		kalSetEvent(prAdapter->prGlueInfo);

	return u4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief this function returns available count in command queue
*
* @param prAdapter      Pointer to the Adapter structure.
*
* @retval
*/
/*----------------------------------------------------------------------------*/
UINT_32 nicTxGetFreeCmdCount(IN P_ADAPTER_T prAdapter)
{
	ASSERT(prAdapter);

	return prAdapter->rFreeCmdList.u4NumElem;
}

VOID nicTxConfigPktOption(P_MSDU_INFO_T prMsduInfo, UINT_32 u4OptionMask,
	BOOLEAN fgSetOption)
{
	/*
	 * prMsduInfo->fgIsBIP = fgSetOption;
	 * if (fgSetOption)
	 *	prMsduInfo->u4Option |= u4OptionMask;
	 * else
	 *	prMsduInfo->u4Option &= ~u4OptionMask;
	 */
}


