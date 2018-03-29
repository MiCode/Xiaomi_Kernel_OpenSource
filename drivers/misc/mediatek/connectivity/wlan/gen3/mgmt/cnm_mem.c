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

/*
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/mgmt/cnm_mem.c#2
*/

/*! \file   "cnm_mem.c"
    \brief  This file contain the management function of packet buffers and
	    generic memory alloc/free functioin for mailbox message.

	    A data packet has a fixed size of buffer, but a management
	    packet can be equipped with a variable size of buffer.
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

static PUINT_8 apucStaRecType[STA_TYPE_INDEX_NUM] = {
	(PUINT_8) "LEGACY",
	(PUINT_8) "P2P",
	(PUINT_8) "BOW"
};

static PUINT_8 apucStaRecRole[STA_ROLE_INDEX_NUM] = {
	(PUINT_8) "ADHOC",
	(PUINT_8) "CLIENT",
	(PUINT_8) "AP",
	(PUINT_8) "DLS"
};

#if CFG_SUPPORT_TDLS
/* The list of valid data rates. */
const UINT_8 aucValidDataRate[] = {
	RATE_1M,		/* RATE_1M_INDEX = 0 */
	RATE_2M,		/* RATE_2M_INDEX */
	RATE_5_5M,		/* RATE_5_5M_INDEX */
	RATE_11M,		/* RATE_11M_INDEX */
	RATE_22M,		/* RATE_22M_INDEX */
	RATE_33M,		/* RATE_33M_INDEX */
	RATE_6M,		/* RATE_6M_INDEX */
	RATE_9M,		/* RATE_9M_INDEX */
	RATE_12M,		/* RATE_12M_INDEX */
	RATE_18M,		/* RATE_18M_INDEX */
	RATE_24M,		/* RATE_24M_INDEX */
	RATE_36M,		/* RATE_36M_INDEX */
	RATE_48M,		/* RATE_48M_INDEX */
	RATE_54M,		/* RATE_54M_INDEX */
	RATE_VHT_PHY,		/* RATE_VHT_PHY_INDEX */
	RATE_HT_PHY		/* RATE_HT_PHY_INDEX */
};
#endif

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
static VOID cnmStaRoutinesForAbort(P_ADAPTER_T prAdapter, P_STA_RECORD_T prStaRec);

static VOID cnmStaRecHandleEventPkt(P_ADAPTER_T prAdapter, P_CMD_INFO_T prCmdInfo, PUINT_8 pucEventBuf);

static VOID cnmStaSendUpdateCmd(P_ADAPTER_T prAdapter, P_STA_RECORD_T prStaRec, BOOLEAN fgNeedResp);

static VOID
cnmStaSendRemoveCmd(P_ADAPTER_T prAdapter,
		    ENUM_STA_REC_CMD_ACTION_T eActionType, UINT_8 ucStaRecIndex, UINT_8 ucBssIndex);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
P_MSDU_INFO_T cnmPktAllocWrapper(P_ADAPTER_T prAdapter, UINT_32 u4Length, PUINT_8 pucStr)
{
	P_MSDU_INFO_T prMsduInfo;

	prMsduInfo = cnmPktAlloc(prAdapter, u4Length);
	DBGLOG(MEM, LOUD, "Alloc MSDU_INFO[0x%p] by [%s]\n", prMsduInfo, pucStr);

	return prMsduInfo;
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
VOID cnmPktFreeWrapper(P_ADAPTER_T prAdapter, P_MSDU_INFO_T prMsduInfo, PUINT_8 pucStr)
{
	DBGLOG(MEM, LOUD, "Free MSDU_INFO[0x%p] by [%s]\n", prMsduInfo, pucStr);

	cnmPktFree(prAdapter, prMsduInfo);
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
P_MSDU_INFO_T cnmPktAlloc(P_ADAPTER_T prAdapter, UINT_32 u4Length)
{
	P_MSDU_INFO_T prMsduInfo;
	P_QUE_T prQueList;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);
	prQueList = &prAdapter->rTxCtrl.rFreeMsduInfoList;

	/* Get a free MSDU_INFO_T */
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_MSDU_INFO_LIST);
	QUEUE_REMOVE_HEAD(prQueList, prMsduInfo, P_MSDU_INFO_T);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_MSDU_INFO_LIST);

	if (prMsduInfo) {
		if (u4Length) {
			prMsduInfo->prPacket = cnmMemAlloc(prAdapter, RAM_TYPE_BUF, u4Length);
			prMsduInfo->eSrc = TX_PACKET_MGMT;

			if (prMsduInfo->prPacket == NULL) {
				KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_MSDU_INFO_LIST);
				QUEUE_INSERT_TAIL(prQueList, &prMsduInfo->rQueEntry);
				KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_MSDU_INFO_LIST);
				prMsduInfo = NULL;
			}
		} else {
			prMsduInfo->prPacket = NULL;
		}
	}
#if DBG
	if (prMsduInfo == NULL) {
		DBGLOG(MEM, TRACE, "MgtDesc#=%ld\n", prQueList->u4NumElem);

#if CFG_DBG_MGT_BUF
		DBGLOG(MEM, TRACE, "rMgtBufInfo: alloc#=%ld, free#=%ld, null#=%ld\n",
				   prAdapter->rMgtBufInfo.u4AllocCount,
				   prAdapter->rMgtBufInfo.u4FreeCount, prAdapter->rMgtBufInfo.u4AllocNullCount);
#endif
	}
#endif

	return prMsduInfo;
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
VOID cnmPktFree(P_ADAPTER_T prAdapter, P_MSDU_INFO_T prMsduInfo)
{
	P_QUE_T prQueList;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);

	if (!prMsduInfo)
		return;

	prQueList = &prAdapter->rTxCtrl.rFreeMsduInfoList;

	/* ASSERT(prMsduInfo->prPacket); */
	if (prMsduInfo->prPacket) {
		cnmMemFree(prAdapter, prMsduInfo->prPacket);
		prMsduInfo->prPacket = NULL;
	}

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_MSDU_INFO_LIST);
	QUEUE_INSERT_TAIL(prQueList, &prMsduInfo->rQueEntry);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_MSDU_INFO_LIST);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is used to initial the MGMT/MSG memory pool.
*
* \param (none)
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID cnmMemInit(P_ADAPTER_T prAdapter)
{
	P_BUF_INFO_T prBufInfo;

	/* Initialize Management buffer pool */
	prBufInfo = &prAdapter->rMgtBufInfo;
	kalMemZero(prBufInfo, sizeof(prAdapter->rMgtBufInfo));
	prBufInfo->pucBuf = prAdapter->pucMgtBufCached;

	/* Setup available memory blocks. 1 indicates FREE */
	prBufInfo->rFreeBlocksBitmap = (BUF_BITMAP) BITS(0, MAX_NUM_OF_BUF_BLOCKS - 1);

	/* Initialize Message buffer pool */
	prBufInfo = &prAdapter->rMsgBufInfo;
	kalMemZero(prBufInfo, sizeof(prAdapter->rMsgBufInfo));
	prBufInfo->pucBuf = &prAdapter->aucMsgBuf[0];

	/* Setup available memory blocks. 1 indicates FREE */
	prBufInfo->rFreeBlocksBitmap = (BUF_BITMAP) BITS(0, MAX_NUM_OF_BUF_BLOCKS - 1);

}				/* end of cnmMemInit() */

/*----------------------------------------------------------------------------*/
/*!
* \brief Allocate MGMT/MSG memory pool.
*
* \param[in] eRamType       Target RAM type.
*                           TCM blk_sz= 16bytes, BUF blk_sz= 256bytes
* \param[in] u4Length       Length of the buffer to allocate.
*
* \retval !NULL    Pointer to the start address of allocated memory.
* \retval NULL     Fail to allocat memory
*/
/*----------------------------------------------------------------------------*/
PVOID cnmMemAlloc(IN P_ADAPTER_T prAdapter, IN ENUM_RAM_TYPE_T eRamType, IN UINT_32 u4Length)
{
	P_BUF_INFO_T prBufInfo;
	BUF_BITMAP rRequiredBitmap;
	UINT_32 u4BlockNum;
	UINT_32 i, u4BlkSzInPower;
	PVOID pvMemory;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);

	if (u4Length == 0) {
		DBGLOG(MEM, WARN, "%s: Length to be allocated is ZERO, skip!\n", __func__);
		return NULL;
	}

	if (eRamType == RAM_TYPE_MSG && u4Length <= 256) {
		prBufInfo = &prAdapter->rMsgBufInfo;
		u4BlkSzInPower = MSG_BUF_BLOCK_SIZE_IN_POWER_OF_2;

		u4BlockNum = (u4Length + MSG_BUF_BLOCK_SIZE - 1)
		    >> MSG_BUF_BLOCK_SIZE_IN_POWER_OF_2;

		ASSERT(u4BlockNum <= MAX_NUM_OF_BUF_BLOCKS);
	} else {
		eRamType = RAM_TYPE_BUF;

		prBufInfo = &prAdapter->rMgtBufInfo;
		u4BlkSzInPower = MGT_BUF_BLOCK_SIZE_IN_POWER_OF_2;

		u4BlockNum = (u4Length + MGT_BUF_BLOCK_SIZE - 1)
		    >> MGT_BUF_BLOCK_SIZE_IN_POWER_OF_2;

		ASSERT(u4BlockNum <= MAX_NUM_OF_BUF_BLOCKS);
	}

#if CFG_DBG_MGT_BUF
	prBufInfo->u4AllocCount++;
#endif

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, eRamType == RAM_TYPE_MSG ? SPIN_LOCK_MSG_BUF : SPIN_LOCK_MGT_BUF);

	if ((u4BlockNum > 0) && (u4BlockNum <= MAX_NUM_OF_BUF_BLOCKS)) {

		/* Convert number of block into bit cluster */
		rRequiredBitmap = BITS(0, u4BlockNum - 1);

		for (i = 0; i <= (MAX_NUM_OF_BUF_BLOCKS - u4BlockNum); i++) {

			/* Have available memory blocks */
			if ((prBufInfo->rFreeBlocksBitmap & rRequiredBitmap)
			    == rRequiredBitmap) {

				/* Clear corresponding bits of allocated memory blocks */
				prBufInfo->rFreeBlocksBitmap &= ~rRequiredBitmap;

				/* Store how many blocks be allocated */
				prBufInfo->aucAllocatedBlockNum[i] = (UINT_8) u4BlockNum;

				KAL_RELEASE_SPIN_LOCK(prAdapter,
						      eRamType == RAM_TYPE_MSG ? SPIN_LOCK_MSG_BUF : SPIN_LOCK_MGT_BUF);

				/* Return the start address of allocated memory */
				return (PVOID) (prBufInfo->pucBuf + (i << u4BlkSzInPower));

			}

			rRequiredBitmap <<= 1;
		}
	}

	/* kalMemAlloc() shall not included in spin_lock */
	KAL_RELEASE_SPIN_LOCK(prAdapter, eRamType == RAM_TYPE_MSG ? SPIN_LOCK_MSG_BUF : SPIN_LOCK_MGT_BUF);

#ifdef LINUX
	pvMemory = (PVOID) kalMemAlloc(u4Length, PHY_MEM_TYPE);
	if (pvMemory)
		kalMemZero(pvMemory, u4Length);
#else
	pvMemory = (PVOID) NULL;
#endif

#if CFG_DBG_MGT_BUF
	prBufInfo->u4AllocNullCount++;

	if (pvMemory)
		prAdapter->u4MemAllocDynamicCount++;
#endif

	return pvMemory;

}				/* end of cnmMemAlloc() */

/*----------------------------------------------------------------------------*/
/*!
* \brief Release memory to MGT/MSG memory pool.
*
* \param pucMemory  Start address of previous allocated memory
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID cnmMemFree(IN P_ADAPTER_T prAdapter, IN PVOID pvMemory)
{
	P_BUF_INFO_T prBufInfo;
	UINT_32 u4BlockIndex;
	BUF_BITMAP rAllocatedBlocksBitmap;
	ENUM_RAM_TYPE_T eRamType;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);

	if (!pvMemory)
		return;

	/* Judge it belongs to which RAM type */
	if (((ULONG) pvMemory >= (ULONG)&prAdapter->aucMsgBuf[0]) &&
	    ((ULONG) pvMemory <= (ULONG)&prAdapter->aucMsgBuf[MSG_BUFFER_SIZE - 1])) {

		prBufInfo = &prAdapter->rMsgBufInfo;
		u4BlockIndex = ((ULONG) pvMemory - (ULONG) prBufInfo->pucBuf)
		    >> MSG_BUF_BLOCK_SIZE_IN_POWER_OF_2;
		ASSERT(u4BlockIndex < MAX_NUM_OF_BUF_BLOCKS);
		eRamType = RAM_TYPE_MSG;
	} else if (((ULONG) pvMemory >= (ULONG) prAdapter->pucMgtBufCached) &&
		   ((ULONG) pvMemory <= ((ULONG) prAdapter->pucMgtBufCached + MGT_BUFFER_SIZE - 1))) {
		prBufInfo = &prAdapter->rMgtBufInfo;
		u4BlockIndex = ((ULONG) pvMemory - (ULONG) prBufInfo->pucBuf)
		    >> MGT_BUF_BLOCK_SIZE_IN_POWER_OF_2;
		ASSERT(u4BlockIndex < MAX_NUM_OF_BUF_BLOCKS);
		eRamType = RAM_TYPE_BUF;
	} else {
#ifdef LINUX
		/* For Linux, it is supported because size is not needed */
		kalMemFree(pvMemory, PHY_MEM_TYPE, 0);
#else
		/* For Windows, it is not supported because of no size argument */
		ASSERT(0);
#endif

#if CFG_DBG_MGT_BUF
		prAdapter->u4MemFreeDynamicCount++;
#endif
		return;
	}

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, eRamType == RAM_TYPE_MSG ? SPIN_LOCK_MSG_BUF : SPIN_LOCK_MGT_BUF);

#if CFG_DBG_MGT_BUF
	prBufInfo->u4FreeCount++;
#endif

	/* Convert number of block into bit cluster */
	ASSERT(prBufInfo->aucAllocatedBlockNum[u4BlockIndex] > 0);

	rAllocatedBlocksBitmap = BITS(0, prBufInfo->aucAllocatedBlockNum[u4BlockIndex] - 1);
	rAllocatedBlocksBitmap <<= u4BlockIndex;

	/* Clear saved block count for this memory segment */
	prBufInfo->aucAllocatedBlockNum[u4BlockIndex] = 0;

	/* Set corresponding bit of released memory block */
	prBufInfo->rFreeBlocksBitmap |= rAllocatedBlocksBitmap;

	KAL_RELEASE_SPIN_LOCK(prAdapter, eRamType == RAM_TYPE_MSG ? SPIN_LOCK_MSG_BUF : SPIN_LOCK_MGT_BUF);

	return;

}				/* end of cnmMemFree() */

/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID cnmStaRecInit(P_ADAPTER_T prAdapter)
{
	P_STA_RECORD_T prStaRec;
	UINT_16 i;

	for (i = 0; i < CFG_STA_REC_NUM; i++) {
		prStaRec = &prAdapter->arStaRec[i];

		prStaRec->ucIndex = (UINT_8) i;
		prStaRec->fgIsInUse = FALSE;
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
P_STA_RECORD_T cnmStaRecAlloc(P_ADAPTER_T prAdapter, ENUM_STA_TYPE_T eStaType, UINT_8 ucBssIndex, PUINT_8 pucMacAddr)
{
	P_STA_RECORD_T prStaRec;
	UINT_16 i, k;

	ASSERT(prAdapter);

	for (i = 0; i < CFG_STA_REC_NUM; i++) {
		prStaRec = &prAdapter->arStaRec[i];

		if (!prStaRec->fgIsInUse) {
			kalMemZero(prStaRec, sizeof(STA_RECORD_T));
			prStaRec->ucIndex = (UINT_8) i;
			prStaRec->ucBssIndex = ucBssIndex;
			prStaRec->fgIsInUse = TRUE;

			prStaRec->eStaType = eStaType;
			prStaRec->ucBssIndex = ucBssIndex;
			prStaRec->ucCurrentGtkId = 255;

			/* Initialize the SN caches for duplicate detection */
			for (k = 0; k < TID_NUM + 1; k++) {
				prStaRec->au2CachedSeqCtrl[k] = 0xFFFF;
				prStaRec->afgIsIgnoreAmsduDuplicate[k] = FALSE;
			}

			/* Initialize SW TX queues in STA_REC */
			for (k = 0; k < STA_WAIT_QUEUE_NUM; k++)
				LINK_INITIALIZE(&prStaRec->arStaWaitQueue[k]);

#if CFG_ENABLE_PER_STA_STATISTICS && CFG_ENABLE_PKT_LIFETIME_PROFILE
			prStaRec->u4TotalTxPktsNumber = 0;
			prStaRec->u4TotalTxPktsTime = 0;
			prStaRec->u4TotalRxPktsNumber = 0;
			prStaRec->u4MaxTxPktsTime = 0;
#endif

			for (k = 0; k < NUM_OF_PER_STA_TX_QUEUES; k++)
				QUEUE_INITIALIZE(&prStaRec->arTxQueue[k]);

			break;
		}
	}

	/* Sync to chip to allocate WTBL resource */
	if (i < CFG_STA_REC_NUM) {
		COPY_MAC_ADDR(prStaRec->aucMacAddr, pucMacAddr);
		prStaRec->ucBMCWlanIndex = WTBL_RESERVED_ENTRY;
		if (secPrivacySeekForEntry(prAdapter, prStaRec))
			cnmStaSendUpdateCmd(prAdapter, prStaRec, FALSE);
#if DBG
		else {
			prStaRec->fgIsInUse = FALSE;
			prStaRec = NULL;
			ASSERT(FALSE);
		}
#endif
	} else {
		prStaRec = NULL;
	}

	return prStaRec;
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
VOID cnmStaRecFree(P_ADAPTER_T prAdapter, P_STA_RECORD_T prStaRec)
{
	UINT_8 ucStaRecIndex, ucBssIndex;

	ASSERT(prAdapter);

	if (!prStaRec)
		return;

	ucStaRecIndex = prStaRec->ucIndex;
	ucBssIndex = prStaRec->ucBssIndex;

	cnmStaRoutinesForAbort(prAdapter, prStaRec);

	cnmStaSendRemoveCmd(prAdapter, STA_REC_CMD_ACTION_STA, ucStaRecIndex, ucBssIndex);
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
static VOID cnmStaRoutinesForAbort(P_ADAPTER_T prAdapter, P_STA_RECORD_T prStaRec)
{
	ASSERT(prAdapter);

	if (!prStaRec)
		return;

	/* To do: free related resources, e.g. timers, buffers, etc */
	cnmTimerStopTimer(prAdapter, &prStaRec->rTxReqDoneOrRxRespTimer);
	prStaRec->fgTransmitKeyExist = FALSE;
	prStaRec->ucCurrentGtkId = 0;

	prStaRec->fgSetPwrMgtBit = FALSE;

	if (prStaRec->pucAssocReqIe) {
		kalMemFree(prStaRec->pucAssocReqIe, VIR_MEM_TYPE, prStaRec->u2AssocReqIeLen);
		prStaRec->pucAssocReqIe = NULL;
		prStaRec->u2AssocReqIeLen = 0;
	}

	qmDeactivateStaRec(prAdapter, prStaRec);

	/* Update the driver part table setting */
	secPrivacyFreeSta(prAdapter, prStaRec);

	prStaRec->fgIsInUse = FALSE;
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
VOID cnmStaFreeAllStaByNetwork(P_ADAPTER_T prAdapter, UINT_8 ucBssIndex, UINT_8 ucStaRecIndexExcluded)
{
#if CFG_ENABLE_WIFI_DIRECT
	P_BSS_INFO_T prBssInfo;
#endif
	P_STA_RECORD_T prStaRec;
	UINT_16 i;

	if (ucBssIndex > MAX_BSS_INDEX)
		return;

	for (i = 0; i < CFG_STA_REC_NUM; i++) {
		prStaRec = (P_STA_RECORD_T) &prAdapter->arStaRec[i];

		if (prStaRec->fgIsInUse && prStaRec->ucBssIndex == ucBssIndex && i != ucStaRecIndexExcluded)
			cnmStaRoutinesForAbort(prAdapter, prStaRec);
	}			/* end of for loop */

	cnmStaSendRemoveCmd(prAdapter,
			    (ucStaRecIndexExcluded < CFG_STA_REC_NUM) ?
			    STA_REC_CMD_ACTION_BSS_EXCLUDE_STA : STA_REC_CMD_ACTION_BSS,
			    ucStaRecIndexExcluded, ucBssIndex);

#if CFG_ENABLE_WIFI_DIRECT
	/* To do: Confirm if it is invoked here or other location, but it should
	 *        be invoked after state sync of STA_REC
	 * Update system operation parameters for AP mode
	 */
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);
	if (prAdapter->fgIsP2PRegistered && prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT)
		rlmUpdateParamsForAP(prAdapter, prBssInfo, FALSE);
#endif
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
P_STA_RECORD_T cnmGetStaRecByIndex(P_ADAPTER_T prAdapter, UINT_8 ucIndex)
{
	P_STA_RECORD_T prStaRec;

	ASSERT(prAdapter);

	prStaRec = (ucIndex < CFG_STA_REC_NUM) ? &prAdapter->arStaRec[ucIndex] : NULL;

	if (prStaRec && prStaRec->fgIsInUse == FALSE)
		prStaRec = NULL;

	return prStaRec;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief Get STA_RECORD_T by Peer MAC Address(Usually TA).
*
* @param[in] pucPeerMacAddr      Given Peer MAC Address.
*
* @retval   Pointer to STA_RECORD_T, if found. NULL, if not found
*/
/*----------------------------------------------------------------------------*/
P_STA_RECORD_T cnmGetStaRecByAddress(P_ADAPTER_T prAdapter, UINT_8 ucBssIndex, PUINT_8 pucPeerMacAddr)
{
	P_STA_RECORD_T prStaRec;
	UINT_16 i;

	ASSERT(prAdapter);

	if (!pucPeerMacAddr)
		return NULL;

	for (i = 0; i < CFG_STA_REC_NUM; i++) {
		prStaRec = &prAdapter->arStaRec[i];

		if (prStaRec->fgIsInUse &&
		    prStaRec->ucBssIndex == ucBssIndex && EQUAL_MAC_ADDR(prStaRec->aucMacAddr, pucPeerMacAddr)) {
			break;
		}
	}

	return (i < CFG_STA_REC_NUM) ? prStaRec : NULL;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will change the ucStaState of STA_RECORD_T and also do
*        event indication to HOST to sync the STA_RECORD_T in driver.
*
* @param[in] prStaRec       Pointer to the STA_RECORD_T
* @param[in] u4NewState     New STATE to change.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID cnmStaRecChangeState(P_ADAPTER_T prAdapter, P_STA_RECORD_T prStaRec, UINT_8 ucNewState)
{
	BOOLEAN fgNeedResp;

	if (!prAdapter)
		return;

	if (!prStaRec) {
		DBGLOG(MEM, WARN, "%s: StaRec is NULL, skip!\n", __func__);
		return;
	}

	if (!prStaRec->fgIsInUse) {
		DBGLOG(MEM, WARN, "%s: StaRec[%u] is not in use, skip!\n", __func__, prStaRec->ucIndex);
		return;
	}

	/* Do nothing when following state transitions happen,
	 * other 6 conditions should be sync to FW, including 1-->1, 3-->3
	 */
	if ((ucNewState == STA_STATE_2 && prStaRec->ucStaState != STA_STATE_3) ||
	    (ucNewState == STA_STATE_1 && prStaRec->ucStaState == STA_STATE_2)) {
		prStaRec->ucStaState = ucNewState;
		return;
	}

	fgNeedResp = FALSE;
	if (ucNewState == STA_STATE_3) {
		/* secFsmEventStart(prAdapter, prStaRec); */
		if (ucNewState != prStaRec->ucStaState) {
			fgNeedResp = TRUE;
			cnmDumpStaRec(prAdapter, prStaRec->ucIndex);
		}
	} else {
		if (ucNewState != prStaRec->ucStaState && prStaRec->ucStaState == STA_STATE_3)
			qmDeactivateStaRec(prAdapter, prStaRec);
		fgNeedResp = FALSE;
	}
	prStaRec->ucStaState = ucNewState;

	cnmStaSendUpdateCmd(prAdapter, prStaRec, fgNeedResp);

#if 1				/* Marked for MT6630 */
#if CFG_ENABLE_WIFI_DIRECT
	/* To do: Confirm if it is invoked here or other location, but it should
	 *        be invoked after state sync of STA_REC
	 * Update system operation parameters for AP mode
	 */
	if (prAdapter->fgIsP2PRegistered && (IS_STA_IN_P2P(prStaRec))) {
		P_BSS_INFO_T prBssInfo;

		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);

		if (prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT)
			rlmUpdateParamsForAP(prAdapter, prBssInfo, FALSE);
	}
#endif
#endif

}

/*----------------------------------------------------------------------------*/
/*!
* @brief
*
* @param[in]
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
static VOID cnmStaRecHandleEventPkt(P_ADAPTER_T prAdapter, P_CMD_INFO_T prCmdInfo, PUINT_8 pucEventBuf)
{
	P_EVENT_ACTIVATE_STA_REC_T prEventContent;
	P_STA_RECORD_T prStaRec;

	prEventContent = (P_EVENT_ACTIVATE_STA_REC_T) pucEventBuf;
	prStaRec = cnmGetStaRecByIndex(prAdapter, prEventContent->ucStaRecIdx);

	if (prStaRec && prStaRec->ucStaState == STA_STATE_3 &&
	    !kalMemCmp(&prStaRec->aucMacAddr[0], &prEventContent->aucMacAddr[0], MAC_ADDR_LEN)) {

		qmActivateStaRec(prAdapter, prStaRec);
	}

}

/*----------------------------------------------------------------------------*/
/*!
* @brief
*
* @param[in]
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
static VOID cnmStaSendUpdateCmd(P_ADAPTER_T prAdapter, P_STA_RECORD_T prStaRec, BOOLEAN fgNeedResp)
{
	P_CMD_UPDATE_STA_RECORD_T prCmdContent;
	WLAN_STATUS rStatus;

	if (!prAdapter)
		return;

	if (!prStaRec) {
		DBGLOG(MEM, WARN, "%s: StaRec is NULL, skip!\n", __func__);
		return;
	}

	if (!prStaRec->fgIsInUse) {
		DBGLOG(MEM, WARN, "%s: StaRec[%u] is not in use, skip!\n", __func__, prStaRec->ucIndex);
		return;
	}

	/* To do: come out a mechanism to limit one STA_REC sync once for AP mode
	 *        to avoid buffer empty case when many STAs are associated
	 *        simultaneously.
	 */

	/* To do: how to avoid 2 times of allocated memory. Use Stack?
	 *        One is here, the other is in wlanSendQueryCmd()
	 */
	prCmdContent = cnmMemAlloc(prAdapter, RAM_TYPE_BUF, sizeof(CMD_UPDATE_STA_RECORD_T));

	/* To do: exception handle */
	if (!prCmdContent) {
		DBGLOG(MEM, WARN, "%s: CMD_ID_UPDATE_STA_RECORD command allocation failed\n", __func__);
		return;
	}

	/* Reset command buffer */
	kalMemZero(prCmdContent, sizeof(CMD_UPDATE_STA_RECORD_T));

	prCmdContent->ucStaIndex = prStaRec->ucIndex;
	prCmdContent->ucStaType = (UINT_8) prStaRec->eStaType;
	kalMemCopy(&prCmdContent->aucMacAddr[0], &prStaRec->aucMacAddr[0], MAC_ADDR_LEN);
	prCmdContent->u2AssocId = prStaRec->u2AssocId;
	prCmdContent->u2ListenInterval = prStaRec->u2ListenInterval;
	prCmdContent->ucBssIndex = prStaRec->ucBssIndex;

	prCmdContent->ucDesiredPhyTypeSet = prStaRec->ucDesiredPhyTypeSet;
	prCmdContent->u2DesiredNonHTRateSet = prStaRec->u2DesiredNonHTRateSet;
	prCmdContent->u2BSSBasicRateSet = prStaRec->u2BSSBasicRateSet;
	prCmdContent->ucMcsSet = prStaRec->ucMcsSet;
	prCmdContent->ucSupMcs32 = (UINT_8) prStaRec->fgSupMcs32;
	prCmdContent->u2HwDefaultFixedRateCode = prStaRec->u2HwDefaultFixedRateCode;

	kalMemCopy(prCmdContent->aucRxMcsBitmask, prStaRec->aucRxMcsBitmask,
		   sizeof(prCmdContent->aucRxMcsBitmask) /*SUP_MCS_RX_BITMASK_OCTET_NUM */);
	prCmdContent->u2RxHighestSupportedRate = prStaRec->u2RxHighestSupportedRate;
	prCmdContent->u4TxRateInfo = prStaRec->u4TxRateInfo;

	prCmdContent->u2HtCapInfo = prStaRec->u2HtCapInfo;
	prCmdContent->ucNeedResp = (UINT_8) fgNeedResp;

#if !CFG_SLT_SUPPORT
	if (prAdapter->rWifiVar.eRateSetting != FIXED_RATE_NONE) {
		/* override rate configuration */
		nicUpdateRateParams(prAdapter,
				    prAdapter->rWifiVar.eRateSetting,
				    &(prCmdContent->ucDesiredPhyTypeSet),
				    &(prCmdContent->u2DesiredNonHTRateSet),
				    &(prCmdContent->u2BSSBasicRateSet),
				    &(prCmdContent->ucMcsSet),
				    &(prCmdContent->ucSupMcs32), &(prCmdContent->u2HtCapInfo));
	}
#endif

	prCmdContent->ucIsQoS = prStaRec->fgIsQoS;
	prCmdContent->ucIsUapsdSupported = prStaRec->fgIsUapsdSupported;
	prCmdContent->ucStaState = prStaRec->ucStaState;

	prCmdContent->ucAmpduParam = prStaRec->ucAmpduParam;
	prCmdContent->u2HtExtendedCap = prStaRec->u2HtExtendedCap;
	prCmdContent->u4TxBeamformingCap = prStaRec->u4TxBeamformingCap;
	prCmdContent->ucAselCap = prStaRec->ucAselCap;
	prCmdContent->ucRCPI = prStaRec->ucRCPI;

	prCmdContent->u4VhtCapInfo = prStaRec->u4VhtCapInfo;
	prCmdContent->u2VhtRxMcsMap = prStaRec->u2VhtRxMcsMap;
	prCmdContent->u2VhtRxHighestSupportedDataRate = prStaRec->u2VhtRxHighestSupportedDataRate;
	prCmdContent->u2VhtTxMcsMap = prStaRec->u2VhtTxMcsMap;
	prCmdContent->u2VhtTxHighestSupportedDataRate = prStaRec->u2VhtTxHighestSupportedDataRate;

	prCmdContent->ucUapsdAc = prStaRec->ucBmpTriggerAC | (prStaRec->ucBmpDeliveryAC << 4);
	prCmdContent->ucUapsdSp = prStaRec->ucUapsdSp;

	prCmdContent->ucWlanIndex = prStaRec->ucWlanIndex;
	prCmdContent->ucBMCWlanIndex = prStaRec->ucBMCWlanIndex;

	prCmdContent->ucTrafficDataType = prStaRec->ucTrafficDataType;
	prCmdContent->ucTxGfMode = prStaRec->ucTxGfMode;
	prCmdContent->ucTxSgiMode = prStaRec->ucTxSgiMode;
	prCmdContent->ucTxStbcMode = prStaRec->ucTxStbcMode;
	prCmdContent->u4FixedPhyRate = prStaRec->u4FixedPhyRate;
	prCmdContent->u2MaxLinkSpeed = prStaRec->u2MaxLinkSpeed;
	prCmdContent->u2MinLinkSpeed = prStaRec->u2MinLinkSpeed;
	prCmdContent->u4Flags = prStaRec->u4Flags;

	prCmdContent->ucTxAmpdu = FEATURE_OPT_IN_COMMAND(prAdapter->rWifiVar.ucAmpduTx);
	prCmdContent->ucRxAmpdu = FEATURE_OPT_IN_COMMAND(prAdapter->rWifiVar.ucAmpduRx);

	prCmdContent->ucTxBaSize = prAdapter->rWifiVar.ucTxBaSize;

	/* if AP's max idle time is greater than 30s, then we send keep alive packets every 30 sec */
	prCmdContent->ucKeepAliveDuration = (UINT_8)prStaRec->u2MaxIdlePeriod;
	prCmdContent->ucKeepAliveOption = prStaRec->ucIdleOption;

	if (prCmdContent->ucKeepAliveDuration > 0)
		DBGLOG(CNM, INFO, "keep-alive duration is %d\n", prCmdContent->ucKeepAliveDuration);
	if (prStaRec->ucDesiredPhyTypeSet & PHY_TYPE_SET_802_11AC)
		prCmdContent->ucRxBaSize = prAdapter->rWifiVar.ucRxVhtBaSize;
	else
		prCmdContent->ucRxBaSize = prAdapter->rWifiVar.ucRxHtBaSize;

	/* RTS Policy */
	if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucSigTaRts)) {
		if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucDynBwRts))
			prCmdContent->ucRtsPolicy = RTS_POLICY_DYNAMIC_BW;
		else
			prCmdContent->ucRtsPolicy = RTS_POLICY_STATIC_BW;
	} else
		prCmdContent->ucRtsPolicy = RTS_POLICY_LEGACY;

	DBGLOG(REQ, TRACE, "Update StaRec[%u] WIDX[%u] State[%u] Type[%u] BssIdx[%u] AID[%u]\n",
			   prCmdContent->ucStaIndex,
			   prCmdContent->ucWlanIndex,
			   prCmdContent->ucStaState,
			   prCmdContent->ucStaType, prCmdContent->ucBssIndex, prCmdContent->u2AssocId);

	DBGLOG(REQ, TRACE, "Update StaRec[%u] QoS[%u] UAPSD[%u] BMCWIDX[%u]\n",
			   prCmdContent->ucStaIndex,
			   prCmdContent->ucIsQoS, prCmdContent->ucIsUapsdSupported, prCmdContent->ucBMCWlanIndex);

	rStatus = wlanSendSetQueryCmd(prAdapter,	/* prAdapter */
				      CMD_ID_UPDATE_STA_RECORD,	/* ucCID */
				      TRUE,	/* fgSetQuery */
				      fgNeedResp,	/* fgNeedResp */
				      FALSE,	/* fgIsOid */
				      fgNeedResp ? cnmStaRecHandleEventPkt : NULL, NULL, /* pfCmdTimeoutHandler */
				      sizeof(CMD_UPDATE_STA_RECORD_T),	/* u4SetQueryInfoLen */
				      (PUINT_8) prCmdContent,	/* pucInfoBuffer */
				      NULL,	/* pvSetQueryBuffer */
				      0	/* u4SetQueryBufferLen */
	    );

	cnmMemFree(prAdapter, prCmdContent);

	if (rStatus != WLAN_STATUS_PENDING)
		DBGLOG(MEM, WARN, "%s: CMD_ID_UPDATE_STA_RECORD result 0x%08x\n", __func__, rStatus);
}

/*----------------------------------------------------------------------------*/
/*!
* @brief
*
* @param[in]
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
static VOID
cnmStaSendRemoveCmd(P_ADAPTER_T prAdapter,
		    ENUM_STA_REC_CMD_ACTION_T eActionType, UINT_8 ucStaRecIndex, UINT_8 ucBssIndex)
{
	CMD_REMOVE_STA_RECORD_T rCmdContent;
	WLAN_STATUS rStatus;

	ASSERT(prAdapter);

	rCmdContent.ucActionType = (UINT_8) eActionType;
	rCmdContent.ucStaIndex = ucStaRecIndex;
	rCmdContent.ucBssIndex = ucBssIndex;
	rCmdContent.ucReserved = 0;

	rStatus = wlanSendSetQueryCmd(prAdapter,	/* prAdapter */
				      CMD_ID_REMOVE_STA_RECORD,	/* ucCID */
				      TRUE,	/* fgSetQuery */
				      FALSE,	/* fgNeedResp */
				      FALSE,	/* fgIsOid */
				      NULL,	/* pfCmdDoneHandler */
				      NULL,	/* pfCmdTimeoutHandler */
				      sizeof(CMD_REMOVE_STA_RECORD_T),	/* u4SetQueryInfoLen */
				      (PUINT_8) &rCmdContent,	/* pucInfoBuffer */
				      NULL,	/* pvSetQueryBuffer */
				      0	/* u4SetQueryBufferLen */
	    );

	if (rStatus != WLAN_STATUS_PENDING)
		DBGLOG(MEM, WARN, "%s: CMD_ID_UPDATE_STA_RECORD result 0x%08x\n", __func__, rStatus);
}

PUINT_8 cnmStaRecGetTypeString(ENUM_STA_TYPE_T eStaType)
{
	PUINT_8 pucTypeString = NULL;

	if (eStaType & STA_TYPE_LEGACY_MASK)
		pucTypeString = apucStaRecType[STA_TYPE_LEGACY_INDEX];
	if (eStaType & STA_TYPE_P2P_MASK)
		pucTypeString = apucStaRecType[STA_TYPE_P2P_INDEX];
	if (eStaType & STA_TYPE_BOW_MASK)
		pucTypeString = apucStaRecType[STA_TYPE_BOW_INDEX];

	return pucTypeString;
}

PUINT_8 cnmStaRecGetRoleString(ENUM_STA_TYPE_T eStaType)
{
	PUINT_8 pucRoleString = NULL;

	if (eStaType & STA_TYPE_ADHOC_MASK)
		pucRoleString = apucStaRecRole[STA_ROLE_ADHOC_INDEX - STA_ROLE_BASE_INDEX];
	if (eStaType & STA_TYPE_CLIENT_MASK)
		pucRoleString = apucStaRecRole[STA_ROLE_CLIENT_INDEX - STA_ROLE_BASE_INDEX];
	if (eStaType & STA_TYPE_AP_MASK)
		pucRoleString = apucStaRecRole[STA_ROLE_AP_INDEX - STA_ROLE_BASE_INDEX];
	if (eStaType & STA_TYPE_DLS_MASK)
		pucRoleString = apucStaRecRole[STA_ROLE_DLS_INDEX - STA_ROLE_BASE_INDEX];

	return pucRoleString;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief
*
* @param[in]
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID cnmDumpStaRec(IN P_ADAPTER_T prAdapter, IN UINT_8 ucStaRecIdx)
{
	UINT_8 ucWTEntry;
	UINT_32 i;
	P_BSS_INFO_T prBssInfo;
	P_STA_RECORD_T prStaRec;

	DEBUGFUNC("cnmDumpStaRec");

	prStaRec = cnmGetStaRecByIndex(prAdapter, ucStaRecIdx);

	if (!prStaRec) {
		DBGLOG(SW4, TRACE, "Invalid StaRec index[%u], skip dump!\n", ucStaRecIdx);
		return;
	}

	ucWTEntry = prStaRec->ucWlanIndex;
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);

	ASSERT(prBssInfo);

	DBGLOG(SW4, TRACE, "============= DUMP STA[%u] ===========\n", ucStaRecIdx);

	DBGLOG(SW4, TRACE,
	       "STA_IDX[%u] BSS_IDX[%u] MAC[" MACSTR "] TYPE[%s %s] WTBL[%u] USED[%u] State[%u]\n",
		prStaRec->ucIndex, prStaRec->ucBssIndex, MAC2STR(prStaRec->aucMacAddr),
		cnmStaRecGetTypeString(prStaRec->eStaType),
		cnmStaRecGetRoleString(prStaRec->eStaType), ucWTEntry, prStaRec->fgIsInUse, prStaRec->ucStaState);

	DBGLOG(SW4, TRACE, "QoS[%u] HT/VHT[%u/%u] AID[%u] WMM[%u] UAPSD[%u] SEC[%u]\n",
			   prStaRec->fgIsQoS,
			   (prStaRec->ucDesiredPhyTypeSet & PHY_TYPE_SET_802_11N) ? TRUE : FALSE,
			   (prStaRec->ucDesiredPhyTypeSet & PHY_TYPE_SET_802_11AC) ? TRUE : FALSE,
			   prStaRec->u2AssocId,
			   prStaRec->fgIsWmmSupported,
			   prStaRec->fgIsUapsdSupported, secIsProtectedBss(prAdapter, prBssInfo));

	DBGLOG(SW4, TRACE, "PhyTypeSet: BSS[0x%02x] Desired[0x%02x] NonHtBasic[0x%02x]\n",
			   prBssInfo->ucPhyTypeSet, prStaRec->ucDesiredPhyTypeSet, prStaRec->ucNonHTBasicPhyType);

	DBGLOG(SW4, TRACE,
	       "RateSet: BssBasic[0x%04x] Operational[0x%04x] DesiredNonHT[0x%02x] DeafultFixedRate[0x%02x]\n",
		prBssInfo->u2BSSBasicRateSet, prStaRec->u2OperationalRateSet,
		prStaRec->u2DesiredNonHTRateSet, prStaRec->u2HwDefaultFixedRateCode);

	DBGLOG(SW4, TRACE, "HT Cap[0x%04x] ExtCap[0x%04x] BeemCap[0x%08x] MCS[0x%02x] MCS32[%u]\n",
			   prStaRec->u2HtCapInfo,
			   prStaRec->u2HtExtendedCap,
			   prStaRec->u4TxBeamformingCap, prStaRec->ucMcsSet, prStaRec->fgSupMcs32);

	DBGLOG(SW4, TRACE, "VHT Cap[0x%08x] TxMCS[0x%04x] RxMCS[0x%04x]\n",
			   prStaRec->u4VhtCapInfo, prStaRec->u2VhtTxMcsMap, prStaRec->u2VhtRxMcsMap);

	DBGLOG(SW4, TRACE, "RCPI[%u] InPS[%u] TxAllowed[%u] KeyRdy[%u] AMPDU T/R[%u/%u]\n",
			   prStaRec->ucRCPI,
			   prStaRec->fgIsInPS,
			   prStaRec->fgIsTxAllowed,
			   prStaRec->fgIsTxKeyReady, prStaRec->fgTxAmpduEn, prStaRec->fgRxAmpduEn);

	DBGLOG(SW4, TRACE, "TxQ LEN TC[0~5] [%03u:%03u:%03u:%03u:%03u:%03u]\n",
			   prStaRec->arTxQueue[0].u4NumElem,
			   prStaRec->arTxQueue[1].u4NumElem,
			   prStaRec->arTxQueue[2].u4NumElem,
			   prStaRec->arTxQueue[3].u4NumElem,
			   prStaRec->arTxQueue[4].u4NumElem, prStaRec->arTxQueue[5].u4NumElem);

	DBGLOG(SW4, TRACE, "BMP AC Delivery/Trigger[%02x/%02x]\n",
			   prStaRec->ucBmpDeliveryAC, prStaRec->ucBmpTriggerAC);

	DBGLOG(SW4, TRACE, "FreeQuota: Total[%u] Delivery/NonDelivery[%u/%u]\n",
			   prStaRec->ucFreeQuota,
			   prStaRec->ucFreeQuotaForDelivery, prStaRec->ucFreeQuotaForNonDelivery);

	for (i = 0; i < CFG_RX_MAX_BA_TID_NUM; i++) {
		if (prStaRec->aprRxReorderParamRefTbl[i]) {
			DBGLOG(SW4, TRACE, "<Rx BA Entry TID[%u]>\n", prStaRec->aprRxReorderParamRefTbl[i]->ucTid);
			DBGLOG(SW4, TRACE,
			       " Valid[%u] WinStart/End[%u/%u] WinSize[%u] ReOrderQueLen[%u]\n",
				prStaRec->aprRxReorderParamRefTbl[i]->fgIsValid,
				prStaRec->aprRxReorderParamRefTbl[i]->u2WinStart,
				prStaRec->aprRxReorderParamRefTbl[i]->u2WinEnd,
				prStaRec->aprRxReorderParamRefTbl[i]->u2WinSize,
				prStaRec->aprRxReorderParamRefTbl[i]->rReOrderQue.u4NumElem);
			DBGLOG(SW4, TRACE,
			       " Bubble Exist[%u] SN[%u]\n",
				prStaRec->aprRxReorderParamRefTbl[i]->fgHasBubble,
				prStaRec->aprRxReorderParamRefTbl[i]->u2FirstBubbleSn);
		}
	}

	DBGLOG(SW4, TRACE, "============= DUMP END ===========\n");

}

VOID cnmDumpMemoryStatus(IN P_ADAPTER_T prAdapter)
{
	P_BUF_INFO_T prBufInfo;

#if CFG_DBG_MGT_BUF
	DBGLOG(SW4, TRACE, "============= DUMP Memory Status =============\n");

	DBGLOG(SW4, TRACE, "Dynamic alloc OS memory count: alloc[%u] free[%u]\n",
			   prAdapter->u4MemAllocDynamicCount, prAdapter->u4MemFreeDynamicCount);

	prBufInfo = &prAdapter->rMsgBufInfo;
	DBGLOG(SW4, TRACE, "MSG memory count: alloc[%u] free[%u] null[%u] bitmap[0x%08x]\n",
			   prBufInfo->u4AllocCount, prBufInfo->u4FreeCount,
			   prBufInfo->u4AllocNullCount, (UINT_32) prBufInfo->rFreeBlocksBitmap);

	prBufInfo = &prAdapter->rMgtBufInfo;
	DBGLOG(SW4, TRACE, "MGT memory count: alloc[%u] free[%u] null[%u] bitmap[0x%08x]\n",
			   prBufInfo->u4AllocCount, prBufInfo->u4FreeCount,
			   prBufInfo->u4AllocNullCount, (UINT_32) prBufInfo->rFreeBlocksBitmap);

	DBGLOG(SW4, TRACE, "============= DUMP END =============\n");

#endif
}

#if CFG_SUPPORT_TDLS
/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to add a peer record.
*
* \param[in] pvAdapter Pointer to the Adapter structure.
* \param[out] pvQueryBuf A pointer to the buffer that holds the result of
*                           the query.
* \param[in] u4QueryBufLen The length of the query buffer.
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                            bytes written into the query buffer. If the call
*                            failed due to invalid length of the query buffer,
*                            returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS			/* TDLS_STATUS  //prStaRec->ucNetTypeIndex */
cnmPeerAdd(P_ADAPTER_T prAdapter, PVOID pvSetBuffer, UINT_32 u4SetBufferLen, PUINT_32 pu4SetInfoLen)
{
	CMD_PEER_ADD_T *prCmd;
	BSS_INFO_T *prAisBssInfo;
	STA_RECORD_T *prStaRec;

	/* sanity check */

	if ((prAdapter == NULL) || (pvSetBuffer == NULL) || (pu4SetInfoLen == NULL))
		return TDLS_STATUS_FAIL;

	/* init */
	*pu4SetInfoLen = sizeof(CMD_PEER_ADD_T);
	prCmd = (CMD_PEER_ADD_T *) pvSetBuffer;

	prAisBssInfo = prAdapter->prAisBssInfo;	/* for AIS only test */
	prStaRec = cnmGetStaRecByAddress(prAdapter, (UINT_8) prAdapter->prAisBssInfo->ucBssIndex, prCmd->aucPeerMac);

	if (prStaRec == NULL) {
		prStaRec =
		    cnmStaRecAlloc(prAdapter, STA_TYPE_DLS_PEER,
				   (UINT_8) prAdapter->prAisBssInfo->ucBssIndex, prCmd->aucPeerMac);

		if (prStaRec == NULL)
			return TDLS_STATUS_RESOURCES;

		if (prAisBssInfo) {
			if (prAisBssInfo->ucBssIndex)
				prStaRec->ucBssIndex = prAisBssInfo->ucBssIndex;
		}

		/* init the prStaRec */
		/* prStaRec will be zero first in cnmStaRecAlloc() */
		COPY_MAC_ADDR(prStaRec->aucMacAddr, prCmd->aucPeerMac);

		prStaRec->u2BSSBasicRateSet = prAisBssInfo->u2BSSBasicRateSet;

		prStaRec->u2DesiredNonHTRateSet = prAdapter->rWifiVar.ucAvailablePhyTypeSet;

		prStaRec->u2OperationalRateSet = prAisBssInfo->u2OperationalRateSet;
		prStaRec->ucPhyTypeSet = prAisBssInfo->ucPhyTypeSet;
		prStaRec->eStaType = prCmd->eStaType;

		/* NOTE(Kevin): Better to change state here, not at TX Done */
		cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_1);

	} else {
		if ((prStaRec->ucStaState > STA_STATE_1) && (IS_DLS_STA(prStaRec))) {
			/* TODO: Teardown the peer */
			cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_1);
		}
	}
	return TDLS_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to update a peer record.
*
* \param[in] pvAdapter Pointer to the Adapter structure.
* \param[out] pvQueryBuf A pointer to the buffer that holds the result of
*                           the query.
* \param[in] u4QueryBufLen The length of the query buffer.
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                            bytes written into the query buffer. If the call
*                            failed due to invalid length of the query buffer,
*                            returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS			/* TDLS_STATUS */
cnmPeerUpdate(P_ADAPTER_T prAdapter, PVOID pvSetBuffer, UINT_32 u4SetBufferLen, PUINT_32 pu4SetInfoLen)
{

	CMD_PEER_UPDATE_T *prCmd;
	BSS_INFO_T *prAisBssInfo;
	STA_RECORD_T *prStaRec;
	UINT_8 ucNonHTPhyTypeSet;

	UINT_16 u2OperationalRateSet = 0;

	UINT_8 ucRate;
	UINT_16 i, j;

	/* sanity check */
	if ((!prAdapter) || (!pvSetBuffer) || (!pu4SetInfoLen))
		return TDLS_STATUS_FAIL;

	/* init */
	*pu4SetInfoLen = sizeof(CMD_PEER_ADD_T);
	prCmd = (CMD_PEER_UPDATE_T *) pvSetBuffer;

	prAisBssInfo = prAdapter->prAisBssInfo;
	if (!prAisBssInfo)
		return TDLS_STATUS_FAIL;

	prStaRec = cnmGetStaRecByAddress(prAdapter, (UINT_8) prAdapter->prAisBssInfo->ucBssIndex, prCmd->aucPeerMac);

	if ((!prStaRec) || !(prStaRec->fgIsInUse))
		return TDLS_STATUS_FAIL;

	if (!IS_DLS_STA(prStaRec))
		return TDLS_STATUS_FAIL;

	if (prAisBssInfo) {
		if (prAisBssInfo->ucBssIndex)
			prStaRec->ucBssIndex = prAisBssInfo->ucBssIndex;
	}

	/* update the record join time. */
	GET_CURRENT_SYSTIME(&prStaRec->rUpdateTime);

	/* update Station Record - Status/Reason Code */
	prStaRec->u2StatusCode = prCmd->u2StatusCode;
	prStaRec->u2AssocId = 0;	/* no use */
	prStaRec->u2ListenInterval = 0;	/* unknown */
	prStaRec->fgIsQoS = TRUE;
	prStaRec->fgIsUapsdSupported = (prCmd->UapsdBitmap == 0) ? FALSE : TRUE;
	prStaRec->u4TxBeamformingCap = 0;	/* no use */
	prStaRec->ucAselCap = 0;	/* no use */
	prStaRec->ucRCPI = 120;
	prStaRec->ucBmpTriggerAC = prCmd->UapsdBitmap;
	prStaRec->ucBmpDeliveryAC = prCmd->UapsdBitmap;
	prStaRec->ucUapsdSp = prCmd->UapsdMaxSp;
	prStaRec->eStaType = prCmd->eStaType;

	/* ++ support rate */
	if (prCmd->aucSupRate) {
		for (i = 0; i < prCmd->u2SupRateLen; i++) {
			if (prCmd->aucSupRate[i]) {
				ucRate = prCmd->aucSupRate[i] & RATE_MASK;
				/* Search all valid data rates */
				for (j = 0; j < sizeof(aucValidDataRate) / sizeof(UINT_8); j++) {
					if (ucRate == aucValidDataRate[j]) {
						u2OperationalRateSet |= BIT(j);
						break;
					}
				}
			}

		}

		prStaRec->u2OperationalRateSet = u2OperationalRateSet;
		prStaRec->u2BSSBasicRateSet = prAisBssInfo->u2BSSBasicRateSet;

		/* 4     <5> PHY type setting */

		prStaRec->ucPhyTypeSet = 0;

		if (BAND_2G4 == prAisBssInfo->eBand) {
			if (prCmd->rHtCap.rMCS.arRxMask)
				prStaRec->ucPhyTypeSet |= PHY_TYPE_BIT_HT;

			/* if not 11n only */
			if (!(prStaRec->u2BSSBasicRateSet & RATE_SET_BIT_HT_PHY)) {
				/* check if support 11g */
				if ((prStaRec->u2OperationalRateSet & RATE_SET_OFDM))
					prStaRec->ucPhyTypeSet |= PHY_TYPE_BIT_ERP;

				/* if not 11g only */
				if (!(prStaRec->u2BSSBasicRateSet & RATE_SET_OFDM)) {
					/* check if support 11b */
					if ((prStaRec->u2OperationalRateSet & RATE_SET_HR_DSSS))
						prStaRec->ucPhyTypeSet |= PHY_TYPE_BIT_HR_DSSS;
				}
			}
		} else {
			if (prCmd->rVHtCap.u2CapInfo)
				prStaRec->ucPhyTypeSet |= PHY_TYPE_BIT_VHT;

			if (prCmd->rHtCap.rMCS.arRxMask)
				prStaRec->ucPhyTypeSet |= PHY_TYPE_BIT_HT;

			/* if not 11n only */
			if (!(prStaRec->u2BSSBasicRateSet & RATE_SET_BIT_HT_PHY)) {
				/* Support 11a definitely */
				prStaRec->ucPhyTypeSet |= PHY_TYPE_BIT_OFDM;
			}
		}

		if (IS_STA_IN_AIS(prStaRec)) {
			if (!((prAdapter->rWifiVar.rConnSettings.eEncStatus == ENUM_ENCRYPTION3_ENABLED)
			      || (prAdapter->rWifiVar.rConnSettings.eEncStatus == ENUM_ENCRYPTION3_KEY_ABSENT)
			      || (prAdapter->rWifiVar.rConnSettings.eEncStatus == ENUM_ENCRYPTION_DISABLED)
			      || (prAdapter->prGlueInfo->u2WSCAssocInfoIELen)
#if CFG_SUPPORT_WAPI
			      || (prAdapter->prGlueInfo->u2WapiAssocInfoIESz)
#endif
			    )) {

				prStaRec->ucPhyTypeSet &= ~PHY_TYPE_BIT_HT;
			}
		}

		prStaRec->ucDesiredPhyTypeSet = prStaRec->ucPhyTypeSet & prAdapter->rWifiVar.ucAvailablePhyTypeSet;
		ucNonHTPhyTypeSet = prStaRec->ucDesiredPhyTypeSet & PHY_TYPE_SET_802_11ABG;

		/* Check for Target BSS's non HT Phy Types */
		if (ucNonHTPhyTypeSet) {
			if (ucNonHTPhyTypeSet & PHY_TYPE_BIT_ERP)
				prStaRec->ucNonHTBasicPhyType = PHY_TYPE_ERP_INDEX;
			else if (ucNonHTPhyTypeSet & PHY_TYPE_BIT_OFDM)
				prStaRec->ucNonHTBasicPhyType = PHY_TYPE_OFDM_INDEX;
			else
				prStaRec->ucNonHTBasicPhyType = PHY_TYPE_HR_DSSS_INDEX;

			prStaRec->fgHasBasicPhyType = TRUE;
		} else {
			/* Use mandatory for 11N only BSS */
			ASSERT(prStaRec->ucPhyTypeSet & PHY_TYPE_SET_802_11N);
			{
				/* TODO(Kevin): which value should we set for 11n ? ERP ? */
				prStaRec->ucNonHTBasicPhyType = PHY_TYPE_HR_DSSS_INDEX;
			}

			prStaRec->fgHasBasicPhyType = FALSE;
		}

	}

	/* ++HT capability */

	if (prCmd->rHtCap.rMCS.arRxMask) {
		prAdapter->rWifiVar.eRateSetting = FIXED_RATE_NONE;
		prStaRec->ucDesiredPhyTypeSet |= PHY_TYPE_BIT_HT;
		prStaRec->ucPhyTypeSet |= PHY_TYPE_BIT_HT;
		prStaRec->u2HtCapInfo = prCmd->rHtCap.u2CapInfo;
		prStaRec->ucAmpduParam = prCmd->rHtCap.ucAmpduParamsInfo;
		prStaRec->u2HtExtendedCap = prCmd->rHtCap.u2ExtHtCapInfo;
		prStaRec->u4TxBeamformingCap = prCmd->rHtCap.u4TxBfCapInfo;
		prStaRec->ucAselCap = prCmd->rHtCap.ucAntennaSelInfo;
		prStaRec->ucMcsSet = prCmd->rHtCap.rMCS.arRxMask[0];
		prStaRec->fgSupMcs32 = (prCmd->rHtCap.rMCS.arRxMask[32 / 8] & BIT(0)) ? TRUE : FALSE;
		kalMemCopy(prStaRec->aucRxMcsBitmask, prCmd->rHtCap.rMCS.arRxMask, sizeof(prStaRec->aucRxMcsBitmask));
	}
	/* TODO ++VHT */

	cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_3);

	return TDLS_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief Get TDLS peer STA_RECORD_T by Peer MAC Address(Usually TA).
*
* @param[in] pucPeerMacAddr      Given Peer MAC Address.
*
* @retval   Pointer to STA_RECORD_T, if found. NULL, if not found
*/
/*----------------------------------------------------------------------------*/
P_STA_RECORD_T cnmGetTdlsPeerByAddress(P_ADAPTER_T prAdapter, UINT_8 ucBssIndex, UINT_8 aucPeerMACAddress[])
{
	P_STA_RECORD_T prStaRec;
	UINT_16 i;

	ASSERT(prAdapter);
	ASSERT(aucPeerMACAddress);

	for (i = 0; i < CFG_STA_REC_NUM; i++) {
		prStaRec = &prAdapter->arStaRec[i];
		if (prStaRec) {
			if (prStaRec->fgIsInUse &&
			    prStaRec->eStaType == STA_TYPE_DLS_PEER &&
			    EQUAL_MAC_ADDR(prStaRec->aucMacAddr, aucPeerMACAddress)) {
				break;
			}
		}
	}

	return prStaRec;
}

#endif
