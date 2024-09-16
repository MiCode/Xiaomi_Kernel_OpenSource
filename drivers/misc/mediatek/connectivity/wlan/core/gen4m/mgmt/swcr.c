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
 ** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/mgmt/swcr.c#1
 */

/*! \file   "swcr.c"
 *    \brief
 *
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
#include "swcr.h"

#if CFG_SUPPORT_SWCR

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */
uint32_t g_au4SwCr[SWCR_CR_NUM];	/*: 0: command other: data */

/* JB mDNS Filter*/
uint32_t g_u4RXFilter;		/* [31] 0: stop 1: start, [3] IPv6 [2] IPv4 */

static struct TIMER g_rSwcrDebugTimer;
static u_int8_t g_fgSwcrDebugTimer = FALSE;
static uint32_t g_u4SwcrDebugCheckTimeout;
static enum ENUM_SWCR_DBG_TYPE g_ucSwcrDebugCheckType;
static uint32_t g_u4SwcrDebugFrameDumpType;

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */
static const PFN_CMD_RW_T g_arSwCtrlCmd[] = {
	swCtrlCmdCategory0,
	swCtrlCmdCategory1
#if TEST_PS
	, testPsCmdCategory0, testPsCmdCategory1
#endif
#if CFG_SUPPORT_802_11V
#if (CFG_SUPPORT_802_11V_TIMING_MEASUREMENT == 1) && (WNM_UNIT_TEST == 1)
	, testWNMCmdCategory0
#endif
#endif
};

const PFN_SWCR_RW_T g_arSwCrModHandle[] = {
	swCtrlSwCr,
	NULL
};

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

enum {
	SWCTRL_MAGIC,
	SWCTRL_DEBUG,
	SWCTRL_WIFI_VAR,
	SWCTRL_ENABLE_INT,
	SWCTRL_DISABLE_INT,
	SWCTRL_TXM_INFO,
	SWCTRL_RXM_INFO,
	SWCTRL_DUMP_BSS,
	SWCTRL_QM_INFO,
	SWCTRL_DUMP_ALL_QUEUE_LEN,
	SWCTRL_DUMP_MEM,
	SWCTRL_TX_CTRL_INFO,
	SWCTRL_DUMP_QUEUE,
	SWCTRL_DUMP_QM_DBG_CNT,
	SWCTRL_QM_DBG_CNT,
	SWCTRL_RX_PKTS_DUMP,
	SWCTRL_RX_FILTER,
#if CFG_INIT_ENABLE_PATTERN_FILTER_ARP
	SWCTRL_RX_ARP_OFFLOAD,
#endif
	SWCTRL_PS_DTIM_SKIP,
	SWCTRL_ROAMING,
	SWCTRL_CATA0_INDEX_NUM
};

enum {
	SWCTRL_STA_INFO,
	SWCTRL_DUMP_STA,
	SWCTRL_STA_QUE_INFO,
	SWCTRL_CATA1_INDEX_NUM
};

/* JB mDNS Filter*/
#define RX_FILTER_START (1<<31)
#define RX_FILTER_IPV4  (1<<2)
#define RX_FILTER_IPV6  (1<<3)
enum ENUM_SWCR_RX_FILTER_CMD {
	SWCR_RX_FILTER_CMD_STOP = 0,
	SWCR_RX_FILTER_CMD_START,
	SWCR_RX_FILTER_CMD_ADD,
	SWCR_RX_FILTER_CMD_REMOVE,
	SWCR_RX_FILTER_NUM
};

#if TEST_PS
enum {
	TEST_PS_MAGIC,
	TEST_PS_SETUP_BSS,
	TEST_PS_ENABLE_BEACON,
	TEST_PS_TRIGGER_BMC,
	TEST_PS_SEND_NULL,
	TEST_PS_BUFFER_BMC,
	TEST_PS_UPDATE_BEACON,
	TEST_PS_CATA0_INDEX_NUM
};

enum {
	TEST_PS_STA_PS,
	TEST_PS_STA_ENTER_PS,
	TEST_PS_STA_EXIT_PS,
	TEST_PS_STA_TRIGGER_PSPOLL,
	TEST_PS_STA_TRIGGER_FRAME,
	TEST_PS_CATA1_INDEX_NUM
};
#endif

#if CFG_SUPPORT_802_11V
#if WNM_UNIT_TEST
enum {
	TEST_WNM_TIMING_MEAS,
	TEST_WNM_CATA0_INDEX_NUM
};
#endif
#endif

#define _SWCTRL_MAGIC 0x66201642

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

void dumpQueue(struct ADAPTER *prAdapter)
{

	struct TX_CTRL *prTxCtrl;
	struct QUE_MGT *prQM;
	struct GLUE_INFO *prGlueInfo;
	uint32_t i;
	uint32_t j;

	DEBUGFUNC("dumpQueue");

	prTxCtrl = &prAdapter->rTxCtrl;
	prQM = &prAdapter->rQM;
	prGlueInfo = prAdapter->prGlueInfo;
#if QM_ADAPTIVE_TC_RESOURCE_CTRL
	for (i = TC0_INDEX; i < TC_NUM; i++) {
		DBGLOG(SW4, INFO, "TC %u\n", i);
		DBGLOG(SW4, INFO, "Max %u Free %u\n",
		       prTxCtrl->rTc.au4MaxNumOfBuffer[i],
		       prTxCtrl->rTc.au4FreeBufferCount[i]);

		DBGLOG(SW4, INFO,
		       "Average %u minReserved %u CurrentTcResource %u GuaranteedTcResource %u\n",
		       QM_GET_TX_QUEUE_LEN(prAdapter, i),
		       prQM->au4MinReservedTcResource[i],
		       prQM->au4CurrentTcResource[i],
		       prQM->au4GuaranteedTcResource[i]);

	}
#endif

#if QM_FORWARDING_FAIRNESS
	for (i = 0; i < NUM_OF_PER_STA_TX_QUEUES; i++) {
		DBGLOG(SW4, INFO,
		       "TC %u HeadStaIdx %u ForwardCount %u\n", i,
		       prQM->au4HeadStaRecIndex[i],
		       prQM->au4ResourceUsedCount[i]);
	}
#endif

	DBGLOG(SW4, INFO, "BMC or unknown TxQueue Len %u\n",
	       prQM->arTxQueue[0].u4NumElem);
	DBGLOG(SW4, INFO, "Pending %d\n",
	       prGlueInfo->i4TxPendingFrameNum);
	DBGLOG(SW4, INFO, "Pending Security %d\n",
	       prGlueInfo->i4TxPendingSecurityFrameNum);
#if defined(LINUX)
	for (i = 0; i < 4; i++) {
		for (j = 0; j < CFG_MAX_TXQ_NUM; j++) {
			DBGLOG(SW4, INFO,
			       "Pending Q[%u][%u] %d\n", i, j,
			       prGlueInfo->ai4TxPendingFrameNumPerQueue[i][j]);
		}
	}
#endif

	DBGLOG(SW4, INFO, " rFreeSwRfbList %u\n",
	       prAdapter->rRxCtrl.rFreeSwRfbList.u4NumElem);
	DBGLOG(SW4, INFO, " rReceivedRfbList %u\n",
	       prAdapter->rRxCtrl.rReceivedRfbList.u4NumElem);
	DBGLOG(SW4, INFO, " rIndicatedRfbList %u\n",
	       prAdapter->rRxCtrl.rIndicatedRfbList.u4NumElem);
	DBGLOG(SW4, INFO, " ucNumIndPacket %u\n",
	       prAdapter->rRxCtrl.ucNumIndPacket);
	DBGLOG(SW4, INFO, " ucNumRetainedPacket %u\n",
	       prAdapter->rRxCtrl.ucNumRetainedPacket);

}

void dumpSTA(struct ADAPTER *prAdapter, struct STA_RECORD *prStaRec)
{
	uint8_t ucWTEntry;
	uint32_t i;
	struct BSS_INFO *prBssInfo;

	DEBUGFUNC("dumpSTA");

	ASSERT(prStaRec);
	ucWTEntry = prStaRec->ucWlanIndex;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);
	ASSERT(prBssInfo);

	DBGLOG(SW4, INFO, "Mac address: " MACSTR " Rcpi %u\n",
	       MAC2STR(prStaRec->aucMacAddr), prStaRec->ucRCPI);

	DBGLOG(SW4, INFO,
	       "Idx %u Wtbl %u Used %u State %u Bss Phy 0x%x Sta DesiredPhy 0x%x\n",
	       prStaRec->ucIndex, ucWTEntry,
	       prStaRec->fgIsInUse, prStaRec->ucStaState,
	       prBssInfo->ucPhyTypeSet, prStaRec->ucDesiredPhyTypeSet);

	DBGLOG(SW4, INFO,
	       "Sta Operation 0x%x  DesiredNontHtRateSet  0x%x Mcs 0x%x u2HtCapInfo 0x%x\n",
	       prStaRec->u2OperationalRateSet,
	       prStaRec->u2DesiredNonHTRateSet, prStaRec->ucMcsSet,
	       prStaRec->u2HtCapInfo);

	for (i = 0; i < NUM_OF_PER_STA_TX_QUEUES; i++)
		DBGLOG(SW4, INFO, "TC %u Queue Len %u\n", i,
		       prStaRec->aprTargetQueue[i]->u4NumElem);

	DBGLOG(SW4, INFO, "BmpDeliveryAC %x\n",
	       prStaRec->ucBmpDeliveryAC);
	DBGLOG(SW4, INFO, "BmpTriggerAC  %x\n",
	       prStaRec->ucBmpTriggerAC);
	DBGLOG(SW4, INFO, "UapsdSpSupproted  %u\n",
	       prStaRec->fgIsUapsdSupported);
	DBGLOG(SW4, INFO, "IsQoS  %u\n", prStaRec->fgIsQoS);
	DBGLOG(SW4, INFO, "AssocId %u\n", prStaRec->u2AssocId);

	DBGLOG(SW4, INFO, "fgIsInPS %u\n", prStaRec->fgIsInPS);
	DBGLOG(SW4, INFO, "ucFreeQuota %u\n",
	       prStaRec->ucFreeQuota);
	DBGLOG(SW4, INFO, "ucFreeQuotaForDelivery %u\n",
	       prStaRec->ucFreeQuotaForDelivery);
	DBGLOG(SW4, INFO, "ucFreeQuotaForNonDelivery %u\n",
	       prStaRec->ucFreeQuotaForNonDelivery);

#if 0
	DBGLOG(SW4, INFO, "IsQmmSup  %u\n",
	       prStaRec->fgIsWmmSupported);
	DBGLOG(SW4, INFO, "IsUapsdSup  %u\n",
	       prStaRec->fgIsUapsdSupported);
	DBGLOG(SW4, INFO, "AvailabaleDeliverPkts  %u\n",
	       prStaRec->ucAvailableDeliverPkts);
	DBGLOG(SW4, INFO, "BmpDeliverPktsAC  %u\n",
	       prStaRec->u4BmpDeliverPktsAC);
	DBGLOG(SW4, INFO, "BmpBufferAC  %u\n",
	       prStaRec->u4BmpBufferAC);
	DBGLOG(SW4, INFO, "BmpNonDeliverPktsAC  %u\n",
	       prStaRec->u4BmpNonDeliverPktsAC);
#endif

	for (i = 0; i < CFG_RX_MAX_BA_TID_NUM; i++) {
		if (prStaRec->aprRxReorderParamRefTbl[i]) {
			DBGLOG(SW4, INFO, "RxReorder fgIsValid: %u\n",
				prStaRec->aprRxReorderParamRefTbl[i]->
				fgIsValid);
			DBGLOG(SW4, INFO, "RxReorder Tid: %u\n",
				prStaRec->aprRxReorderParamRefTbl[i]->ucTid);
			DBGLOG(SW4, INFO,
				"RxReorder rReOrderQue Len: %u\n",
				prStaRec->aprRxReorderParamRefTbl[i]->
				rReOrderQue.u4NumElem);
			DBGLOG(SW4, INFO, "RxReorder WinStart: %u\n",
				prStaRec->aprRxReorderParamRefTbl[i]->
				u2WinStart);
			DBGLOG(SW4, INFO, "RxReorder WinEnd: %u\n",
				 prStaRec->aprRxReorderParamRefTbl[i]->
				 u2WinEnd);
			DBGLOG(SW4, INFO, "RxReorder WinSize: %u\n",
				prStaRec->aprRxReorderParamRefTbl[i]->
				u2WinSize);
		}
	}

}

void dumpBss(struct ADAPTER *prAdapter,
	     struct BSS_INFO *prBssInfo)
{

	DBGLOG(SW4, INFO, "SSID %s\n", HIDE(prBssInfo->aucSSID));
	DBGLOG(SW4, INFO, "OWN " MACSTR "\n",
	       MAC2STR(prBssInfo->aucOwnMacAddr));
	DBGLOG(SW4, INFO, "BSSID " MACSTR "\n",
	       MAC2STR(prBssInfo->aucBSSID));
	DBGLOG(SW4, INFO, "eNetworkType %u\n",
	       prBssInfo->eNetworkType);
	DBGLOG(SW4, INFO, "ucBssIndex %u\n", prBssInfo->ucBssIndex);
	DBGLOG(SW4, INFO, "eConnectionState %u\n",
	       prBssInfo->eConnectionState);
	DBGLOG(SW4, INFO, "eCurrentOPMode %u\n",
	       prBssInfo->eCurrentOPMode);
	DBGLOG(SW4, INFO, "fgIsQBSS %u\n", prBssInfo->fgIsQBSS);
	DBGLOG(SW4, INFO, "fgIsShortPreambleAllowed %u\n",
	       prBssInfo->fgIsShortPreambleAllowed);
	DBGLOG(SW4, INFO, "fgUseShortPreamble %u\n",
	       prBssInfo->fgUseShortPreamble);
	DBGLOG(SW4, INFO, "fgUseShortSlotTime %u\n",
	       prBssInfo->fgUseShortSlotTime);
	DBGLOG(SW4, INFO, "ucNonHTBasicPhyType %x\n",
	       prBssInfo->ucNonHTBasicPhyType);
	DBGLOG(SW4, INFO, "u2OperationalRateSet %x\n",
	       prBssInfo->u2OperationalRateSet);
	DBGLOG(SW4, INFO, "u2BSSBasicRateSet %x\n",
	       prBssInfo->u2BSSBasicRateSet);
	DBGLOG(SW4, INFO, "ucPhyTypeSet %x\n",
	       prBssInfo->ucPhyTypeSet);
	DBGLOG(SW4, INFO, "rStaRecOfClientList %d\n",
	       prBssInfo->rStaRecOfClientList.u4NumElem);
	DBGLOG(SW4, INFO, "u2CapInfo %x\n", prBssInfo->u2CapInfo);
	DBGLOG(SW4, INFO, "u2ATIMWindow %x\n",
	       prBssInfo->u2ATIMWindow);
	DBGLOG(SW4, INFO, "u2AssocId %x\n", prBssInfo->u2AssocId);
	DBGLOG(SW4, INFO, "ucDTIMPeriod %x\n",
	       prBssInfo->ucDTIMPeriod);
	DBGLOG(SW4, INFO, "ucDTIMCount %x\n",
	       prBssInfo->ucDTIMCount);
	DBGLOG(SW4, INFO, "fgIsNetAbsent %x\n",
	       prBssInfo->fgIsNetAbsent);
	DBGLOG(SW4, INFO, "eBand %d\n", prBssInfo->eBand);
	DBGLOG(SW4, INFO, "ucPrimaryChannel %d\n",
	       prBssInfo->ucPrimaryChannel);
	DBGLOG(SW4, INFO, "ucHtOpInfo1 %d\n",
	       prBssInfo->ucHtOpInfo1);
	DBGLOG(SW4, INFO, "ucHtOpInfo2 %d\n",
	       prBssInfo->u2HtOpInfo2);
	DBGLOG(SW4, INFO, "ucHtOpInfo3 %d\n",
	       prBssInfo->u2HtOpInfo3);
	DBGLOG(SW4, INFO, "fgErpProtectMode %d\n",
	       prBssInfo->fgErpProtectMode);
	DBGLOG(SW4, INFO, "eHtProtectMode %d\n",
	       prBssInfo->eHtProtectMode);
	DBGLOG(SW4, INFO, "eGfOperationMode %d\n",
	       prBssInfo->eGfOperationMode);
	DBGLOG(SW4, INFO, "eRifsOperationMode %d\n",
	       prBssInfo->eRifsOperationMode);
	DBGLOG(SW4, INFO, "fgObssErpProtectMode %d\n",
	       prBssInfo->fgObssErpProtectMode);
	DBGLOG(SW4, INFO, "eObssHtProtectMode %d\n",
	       prBssInfo->eObssHtProtectMode);
	DBGLOG(SW4, INFO, "eObssGfProtectMode %d\n",
	       prBssInfo->eObssGfOperationMode);
	DBGLOG(SW4, INFO, "fgObssRifsOperationMode %d\n",
	       prBssInfo->fgObssRifsOperationMode);
	DBGLOG(SW4, INFO, "fgAssoc40mBwAllowed %d\n",
	       prBssInfo->fgAssoc40mBwAllowed);
	DBGLOG(SW4, INFO, "fg40mBwAllowed %d\n",
	       prBssInfo->fg40mBwAllowed);
	DBGLOG(SW4, INFO, "eBssSCO %d\n", prBssInfo->eBssSCO);

}

void swCtrlCmdCategory0(struct ADAPTER *prAdapter,
			uint8_t ucCate, uint8_t ucAction, uint8_t ucOpt0,
			uint8_t ucOpt1)
{
	uint8_t ucIndex, ucRead;
	uint32_t i;
	struct CMD_RX_PACKET_FILTER rSetRxPacketFilter;

	DEBUGFUNC("swCtrlCmdCategory0");

	SWCR_GET_RW_INDEX(ucAction, ucRead, ucIndex);

	i = 0;

	if (ucIndex >= SWCTRL_CATA0_INDEX_NUM)
		return;

	if (ucRead == SWCR_WRITE) {
		switch (ucIndex) {
		case SWCTRL_DEBUG:
			break;
		case SWCTRL_WIFI_VAR:
			break;

#if QM_DEBUG_COUNTER
		case SWCTRL_DUMP_QM_DBG_CNT:
			for (i = 0; i < QM_DBG_CNT_NUM; i++)
				prAdapter->rQM.au4QmDebugCounters[i] = 0;
			break;
		case SWCTRL_QM_DBG_CNT:
			prAdapter->rQM.au4QmDebugCounters[ucOpt0] =
				g_au4SwCr[1];

			break;
#endif
#if CFG_RX_PKTS_DUMP
		case SWCTRL_RX_PKTS_DUMP:
			prAdapter->rRxCtrl.u4RxPktsDumpTypeMask = g_au4SwCr[1];
			break;
#endif
		case SWCTRL_RX_FILTER: {
			uint32_t u4rxfilter;
			u_int8_t fgUpdate = FALSE;
			uint32_t rStatus = WLAN_STATUS_SUCCESS;

			if (ucOpt0 == SWCR_RX_FILTER_CMD_STOP) {
				g_u4RXFilter &= ~(RX_FILTER_START);
				/* changed by jeffrey to align
				 * Android behavior
				 */
#if 0
				if (prAdapter->fgAllMulicastFilter == FALSE)
					prAdapter->u4OsPacketFilter &=
					~PARAM_PACKET_FILTER_ALL_MULTICAST;
#endif
				prAdapter->u4OsPacketFilter &=
					~PARAM_PACKET_FILTER_MULTICAST;
				u4rxfilter = prAdapter->u4OsPacketFilter;
				fgUpdate = TRUE;
			} else if (ucOpt0 == SWCR_RX_FILTER_CMD_START) {
				g_u4RXFilter |= (RX_FILTER_START);

				if ((g_u4RXFilter & RX_FILTER_IPV4)
				    || (g_u4RXFilter & RX_FILTER_IPV6)) {
#if 0
					prAdapter->u4OsPacketFilter |=
					PARAM_PACKET_FILTER_ALL_MULTICAST;
#endif
					prAdapter->u4OsPacketFilter |=
						PARAM_PACKET_FILTER_MULTICAST;
				}
				u4rxfilter = prAdapter->u4OsPacketFilter;
				fgUpdate = TRUE;
			} else if (ucOpt0 == SWCR_RX_FILTER_CMD_ADD) {
				if (ucOpt1 < 31)
					g_u4RXFilter |= (1 << ucOpt1);
			} else if (ucOpt0 == SWCR_RX_FILTER_CMD_REMOVE) {
				if (ucOpt1 < 31)
					g_u4RXFilter &= ~(1 << ucOpt1);
			}

			if (fgUpdate == TRUE) {
				rSetRxPacketFilter.
				u4RxPacketFilter = u4rxfilter;
				rStatus =
					wlanoidSetPacketFilter(
						prAdapter, &rSetRxPacketFilter,
						FALSE, NULL, 0);
			}
			/* DBGLOG(SW4, INFO,("SWCTRL_RX_FILTER:
			 * g_u4RXFilter %x ucOpt0 %x ucOpt1 %x fgUpdate %x
			 *  u4rxfilter %x, rStatus %x\n",
			 * g_u4RXFilter, ucOpt0, ucOpt1,
			 * fgUpdate, u4rxfilter, rStatus));
			 */
		}
		break;

#if CFG_INIT_ENABLE_PATTERN_FILTER_ARP
		case SWCTRL_RX_ARP_OFFLOAD: {
			uint32_t rStatus = WLAN_STATUS_FAILURE;
			uint32_t u4SetInfoLen = 0;
			uint32_t u4Len = OFFSET_OF(struct
				PARAM_NETWORK_ADDRESS_LIST, arAddress);
			uint32_t u4NumIPv4 = 0, u4NumIPv6 = 0;
			uint32_t i = 0;
			uint8_t *pucBufIpAddr = NULL;
			struct PARAM_NETWORK_ADDRESS_LIST *prParamNetAddrList =
					NULL;
			struct PARAM_NETWORK_ADDRESS_IP *prParamIpAddr = NULL;
			uint8_t *pucIp = NULL;
			/* PUINT_8                         pucIpv6 = NULL; */
			uint32_t bufSize =
				u4Len + (OFFSET_OF(struct PARAM_NETWORK_ADDRESS,
				aucAddress) +
				sizeof(
				struct PARAM_NETWORK_ADDRESS_IP)) * 3;
			struct PARAM_NETWORK_ADDRESS *prParamNetAddr = NULL;

			/* <1> allocate IP address buffer */
			pucBufIpAddr = kalMemAlloc(bufSize, VIR_MEM_TYPE);
			/* TODO: replace 3 to macro */
			pucIp = kalMemAlloc(3 * 4, VIR_MEM_TYPE);

			prParamNetAddrList =
				(struct PARAM_NETWORK_ADDRESS_LIST *)
				pucBufIpAddr;
			prParamNetAddr = prParamNetAddrList->arAddress;
			/* <2> clear IP address buffer */
			kalMemZero(pucBufIpAddr, bufSize);
			kalMemZero(pucIp, 3 * 4);

			/* <3> setup the number of IP address */
			if (ucOpt1 == 1) {
				/* TODO: repleace 3 to macro */
				if (wlanGetIPV4Address(
						prAdapter->prGlueInfo, pucIp,
						&u4NumIPv4) && u4NumIPv4 > 3)
					u4NumIPv4 = 3;
			} else if (ucOpt1 == 0) {
				u4NumIPv4 = u4NumIPv6 = 0;
			}
			DBGLOG(INIT, INFO, "u4Len:%d bufSize:%d u4NumIPv4:%d\n",
			       u4Len, bufSize, u4NumIPv4);

			prParamNetAddrList->u4AddressCount =
				u4NumIPv6 + u4NumIPv4;
			prParamNetAddrList->u2AddressType =
				PARAM_PROTOCOL_ID_TCP_IP;

			for (i = 0; i < u4NumIPv4; i++) {
				prParamNetAddr->u2AddressLength = sizeof(struct
					PARAM_NETWORK_ADDRESS_IP);
				prParamNetAddr->u2AddressType =
					PARAM_PROTOCOL_ID_TCP_IP;
				prParamIpAddr =
					(struct PARAM_NETWORK_ADDRESS_IP *)
					prParamNetAddr->aucAddress;
				kalMemCopy(&prParamIpAddr->in_addr,
					pucIp + (i * 4), 4);
				prParamNetAddr =
					(struct PARAM_NETWORK_ADDRESS *)
						((uint32_t) prParamNetAddr +
							OFFSET_OF
							(struct
							PARAM_NETWORK_ADDRESS,
							aucAddress) +
							sizeof(
							struct
							PARAM_NETWORK_ADDRESS_IP
							));
				u4Len +=
					OFFSET_OF(struct PARAM_NETWORK_ADDRESS,
						  aucAddress) +
						  sizeof(
						  struct
						  PARAM_NETWORK_ADDRESS_IP);
			}

#if 0
#ifdef CONFIG_IPV6
			if (!wlanGetIPV6Address(prAdapter->prGlueInfo, pucIp,
						&u4NumIPv6)
			    || (u4NumIPv6 + u4NumIPv4) > 3) {
				goto bailout;
			}

			pucIpv6 = kalMemAlloc(u4NumIPv6 * 16, VIR_MEM_TYPE);

			for (i = 0; i < u4NumIPv6; i++) {
				prParamNetAddr->u2AddressLength = 6;
				prParamNetAddr->u2AddressType =
					PARAM_PROTOCOL_ID_TCP_IP;
				kalMemCopy(prParamNetAddr->aucAddress,
					pucIpv6 + (i * 16),
					16);
				prParamNetAddr =
					(struct PARAM_NETWORK_ADDRESS *)
					((uint32_t) prParamNetAddr +
						sizeof(ip6));
				u4Len += OFFSET_OF(struct PARAM_NETWORK_ADDRESS,
						   aucAddress) + sizeof(ip6);
			}
#endif
#endif

			ASSERT(u4Len <= bufSize);

			rStatus = wlanoidSetNetworkAddress(prAdapter,
				(void *) prParamNetAddrList,
				u4Len, &u4SetInfoLen);

			if (rStatus != WLAN_STATUS_SUCCESS)
				DBGLOG(INIT, INFO,
					"set HW packet filter fail 0x%1x\n",
				  rStatus);

			/* TODO: replace 3 to marco */
			if (pucIp)
				kalMemFree(pucIp, VIR_MEM_TYPE, 3 * 4);
			if (pucBufIpAddr)
				kalMemFree(pucBufIpAddr, VIR_MEM_TYPE, bufSize);

		}
		break;
#endif
		case SWCTRL_PS_DTIM_SKIP:
			break;
		case SWCTRL_ROAMING:
			break;
		default:
			break;
		}
	} else {
		switch (ucIndex) {
		case SWCTRL_DEBUG:
			break;
		case SWCTRL_MAGIC:
			g_au4SwCr[1] = _SWCTRL_MAGIC;
			break;
		case SWCTRL_QM_INFO: {
			struct QUE_MGT *prQM = &prAdapter->rQM;

			switch (ucOpt0) {
			case 0:
#if QM_ADAPTIVE_TC_RESOURCE_CTRL
				g_au4SwCr[1] =
					(QM_GET_TX_QUEUE_LEN(prAdapter,
						ucOpt1));
				g_au4SwCr[2] =
					prQM->au4MinReservedTcResource[ucOpt1];
				g_au4SwCr[3] =
					prQM->au4CurrentTcResource[ucOpt1];
				g_au4SwCr[4] =
					prQM->au4GuaranteedTcResource[ucOpt1];
#endif
				break;

			case 1:
#if QM_FORWARDING_FAIRNESS
				g_au4SwCr[1] =
					prQM->au4ResourceUsedCount[ucOpt1];
				g_au4SwCr[2] = prQM->au4HeadStaRecIndex[ucOpt1];
#endif
				break;

			case 2:
				/* only one */
				g_au4SwCr[1] =
					prQM->arTxQueue[ucOpt1].u4NumElem;

				break;
			}
		}
		break;
		case SWCTRL_TX_CTRL_INFO: {
			struct TX_CTRL *prTxCtrl;

			prTxCtrl = &prAdapter->rTxCtrl;
			switch (ucOpt0) {
			case 0:
				g_au4SwCr[1] =
					prAdapter->rTxCtrl.rTc.
					au4FreeBufferCount[ucOpt1];
				g_au4SwCr[2] =
					prAdapter->rTxCtrl.rTc.
					au4MaxNumOfBuffer[ucOpt1];
				break;
			}

		}
		break;
		case SWCTRL_DUMP_QUEUE:
			dumpQueue(prAdapter);

			break;
#if QM_DEBUG_COUNTER
		case SWCTRL_DUMP_QM_DBG_CNT:
			for (i = 0; i < QM_DBG_CNT_NUM; i++)
				DBGLOG(SW4, INFO, "QM:DBG %u %u\n", i,
				       prAdapter->rQM.au4QmDebugCounters[i]);
			break;

		case SWCTRL_QM_DBG_CNT:
			g_au4SwCr[1] =
				prAdapter->rQM.au4QmDebugCounters[ucOpt0];
			break;
#endif
		case SWCTRL_DUMP_BSS: {
			dumpBss(prAdapter, GET_BSS_INFO_BY_INDEX(prAdapter,
					ucOpt0));
		}
		break;

		default:
			break;
		}

	}
}

void swCtrlCmdCategory1(struct ADAPTER *prAdapter,
			uint8_t ucCate, uint8_t ucAction, uint8_t ucOpt0,
			uint8_t ucOpt1)
{
	uint8_t ucIndex, ucRead;
	uint8_t ucWTEntry;
	struct STA_RECORD *prStaRec;

	DEBUGFUNC("swCtrlCmdCategory1");

	SWCR_GET_RW_INDEX(ucAction, ucRead, ucIndex);

	if (ucOpt0 >= CFG_STA_REC_NUM)
		return;

	/* prStaRec = cnmGetStaRecByIndex (prAdapter, ucOpt0); */
	prStaRec = &prAdapter->arStaRec[ucOpt0];
	ucWTEntry = prStaRec->ucWlanIndex;
	if (ucRead == SWCR_WRITE) {
		/* ToDo:: Nothing */
	} else {
		/* Read */
		switch (ucIndex) {
		case SWCTRL_STA_QUE_INFO: {
			g_au4SwCr[1] = prStaRec->arTxQueue[ucOpt1].u4NumElem;
		}
		break;
		case SWCTRL_STA_INFO:
			switch (ucOpt1) {
			case 0:
				g_au4SwCr[1] = prStaRec->fgIsInPS;
				break;
			}

			break;

		case SWCTRL_DUMP_STA: {
			dumpSTA(prAdapter, prStaRec);
		}
		break;

		default:

			break;
		}
	}

}

#if TEST_PS

void
testPsSendQoSNullFrame(IN struct ADAPTER *prAdapter,
		       IN struct STA_RECORD *prStaRec,
		       IN uint8_t ucUP,
		       IN uint8_t ucBssIndex,
		       IN u_int8_t fgBMC,
		       IN u_int8_t fgIsBurstEnd, IN u_int8_t ucPacketType,
		       IN u_int8_t ucPsSessionID, IN u_int8_t fgSetEOSP)
{
	struct MSDU_INFO *prMsduInfo;
	uint16_t u2EstimatedFrameLen;
	struct WLAN_MAC_HEADER_QOS *prQoSNullFrame;

	DEBUGFUNC("testPsSendQoSNullFrame");
	DBGLOG(SW4, LOUD, "\n");

	/* 4 <1> Allocate a PKT_INFO_T for Null Frame */
	/* Init with MGMT Header Length */
	u2EstimatedFrameLen = MAC_TX_RESERVED_FIELD +
			      WLAN_MAC_HEADER_QOS_LEN;

	/* Allocate a MSDU_INFO_T */

	prMsduInfo = cnmMgtPktAlloc(prAdapter, u2EstimatedFrameLen);

	if (prMsduInfo == NULL) {
		DBGLOG(SW4, WARN,
		       "No PKT_INFO_T for sending Null Frame.\n");
		return;
	}
	/* 4 <2> Compose Null frame in MSDU_INfO_T. */
	bssComposeQoSNullFrame(prAdapter,
		(uint8_t *) ((unsigned long) (prMsduInfo->prPacket) +
		MAC_TX_RESERVED_FIELD),
		prStaRec, ucUP, fgSetEOSP);

	TX_SET_MMPDU(prAdapter,
		     prMsduInfo,
		     ucBssIndex,
		     prStaRec->ucIndex, WLAN_MAC_HEADER_QOS_LEN,
		     WLAN_MAC_HEADER_QOS_LEN, NULL, MSDU_RATE_MODE_AUTO);

	prMsduInfo->ucUserPriority = ucUP;
	prMsduInfo->ucPacketType = ucPacketType;

	prQoSNullFrame = (struct WLAN_MAC_HEADER_QOS *) ((uint8_t *)
			 ((unsigned long) (prMsduInfo->prPacket) +
			  MAC_TX_RESERVED_FIELD));

	if (fgBMC)
		prQoSNullFrame->aucAddr1[0] = 0xfd;
	else
		prQoSNullFrame->aucAddr1[5] = 0xdd;

	/* 4 <4> Inform TXM  to send this Null frame. */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);

}

void testPsSetupBss(IN struct ADAPTER *prAdapter,
		    IN uint8_t ucBssIndex)
{
	struct BSS_INFO *prBssInfo;
	uint8_t _aucZeroMacAddr[] = NULL_MAC_ADDR;

	DEBUGFUNC("testPsSetupBss()");
	DBGLOG(SW4, INFO, "index %d\n", ucBssIndex);

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

	/* 4 <1.2> Initiate PWR STATE */
	/* SET_NET_PWR_STATE_IDLE(prAdapter, ucNetworkTypeIndex); */

	/* 4 <2> Initiate BSS_INFO_T - common part */
	BSS_INFO_INIT(prAdapter, prBssInfo);

	prBssInfo->eConnectionState =
		MEDIA_STATE_DISCONNECTED;
	prBssInfo->eConnectionStateIndicated =
		MEDIA_STATE_DISCONNECTED;
	prBssInfo->eCurrentOPMode = OP_MODE_ACCESS_POINT;
	prBssInfo->fgIsNetActive = TRUE;
	prBssInfo->ucBssIndex = ucBssIndex;
	prBssInfo->ucReasonOfDisconnect =
		DISCONNECT_REASON_CODE_RESERVED;

	/* Depend on eBand */
	prBssInfo->ucPhyTypeSet =
		PHY_TYPE_SET_802_11BG;
	/* Depend on eCurrentOPMode and ucPhyTypeSet */
	prBssInfo->ucConfigAdHocAPMode =
		AP_MODE_MIXED_11BG;
	prBssInfo->u2BSSBasicRateSet = RATE_SET_ERP;
	prBssInfo->u2OperationalRateSet = RATE_SET_OFDM;
	prBssInfo->fgErpProtectMode = FALSE;
	prBssInfo->fgIsQBSS = TRUE;

	/* 4 <1.5> Setup MIB for current BSS */
	prBssInfo->u2BeaconInterval = 100;
	prBssInfo->ucDTIMPeriod = DOT11_DTIM_PERIOD_DEFAULT;
	prBssInfo->u2ATIMWindow = 0;

	prBssInfo->ucBeaconTimeoutCount = 0;

	bssInitForAP(prAdapter, prBssInfo, TRUE);

	COPY_MAC_ADDR(prBssInfo->aucBSSID, _aucZeroMacAddr);
	LINK_INITIALIZE(&prBssInfo->rStaRecOfClientList);
	prBssInfo->fgIsBeaconActivated = TRUE;
	prBssInfo->u2HwDefaultFixedRateCode = RATE_CCK_1M_LONG;

	COPY_MAC_ADDR(prBssInfo->aucOwnMacAddr,
		      prAdapter->rWifiVar.aucMacAddress);

	/* 4 <3> Initiate BSS_INFO_T - private part */
	/* TODO */
	prBssInfo->eBand = BAND_2G4;
	prBssInfo->ucPrimaryChannel = 1;
	prBssInfo->prStaRecOfAP = (struct STA_RECORD *) NULL;

	/* prBssInfo->fgErpProtectMode =  eErpProectMode; */
	/* prBssInfo->eHtProtectMode = eHtProtectMode; */
	/* prBssInfo->eGfOperationMode = eGfOperationMode; */

	/* 4 <4> Allocate MSDU_INFO_T for Beacon */
	prBssInfo->prBeacon = cnmMgtPktAlloc(prAdapter,
		OFFSET_OF(struct WLAN_BEACON_FRAME,
		aucInfoElem[0]) + MAX_IE_LENGTH);

	if (prBssInfo->prBeacon) {
		prBssInfo->prBeacon->eSrc = TX_PACKET_MGMT;
		prBssInfo->prBeacon->ucBssIndex = ucBssIndex;
	} else {
		DBGLOG(SW4, INFO, "prBeacon allocation fail\n");
	}

#if 0
	prBssInfo->rPmProfSetupInfo.ucBmpDeliveryAC = PM_UAPSD_ALL;
	prBssInfo->rPmProfSetupInfo.ucBmpTriggerAC = PM_UAPSD_ALL;
	prBssInfo->rPmProfSetupInfo.ucUapsdSp = WMM_MAX_SP_LENGTH_2;
#else
	prBssInfo->rPmProfSetupInfo.ucBmpDeliveryAC =
		(uint8_t) prAdapter->u4UapsdAcBmp;
	prBssInfo->rPmProfSetupInfo.ucBmpTriggerAC =
		(uint8_t) prAdapter->u4UapsdAcBmp;
	prBssInfo->rPmProfSetupInfo.ucUapsdSp = (uint8_t)
						prAdapter->u4MaxSpLen;
#endif

#if 0
	for (eAci = 0; eAci < WMM_AC_INDEX_NUM; eAci++) {

		prBssInfo->arACQueParms[eAci].ucIsACMSet = FALSE;
		prBssInfo->arACQueParms[eAci].u2Aifsn = (uint16_t) eAci;
		prBssInfo->arACQueParms[eAci].u2CWmin = 7;
		prBssInfo->arACQueParms[eAci].u2CWmax = 31;
		prBssInfo->arACQueParms[eAci].u2TxopLimit = eAci + 1;
		DBGLOG(SW4, INFO,
		       "MQM: eAci = %d, ACM = %d, Aifsn = %d, CWmin = %d, CWmax = %d, TxopLimit = %d\n",
		       eAci, prBssInfo->arACQueParms[eAci].ucIsACMSet,
		       prBssInfo->arACQueParms[eAci].u2Aifsn,
		       prBssInfo->arACQueParms[eAci].u2CWmin,
		       prBssInfo->arACQueParms[eAci].u2CWmax,
		       prBssInfo->arACQueParms[eAci].u2TxopLimit);

	}
#endif

	DBGLOG(SW4, INFO,
	       "[2] ucBmpDeliveryAC:0x%x, ucBmpTriggerAC:0x%x, ucUapsdSp:0x%x",
	       prBssInfo->rPmProfSetupInfo.ucBmpDeliveryAC,
	       prBssInfo->rPmProfSetupInfo.ucBmpTriggerAC,
	       prBssInfo->rPmProfSetupInfo.ucUapsdSp);
}

void testPsCmdCategory0(struct ADAPTER *prAdapter,
			uint8_t ucCate, uint8_t ucAction, uint8_t ucOpt0,
			uint8_t ucOpt1)
{
	uint8_t ucIndex, ucRead;
	struct STA_RECORD *prStaRec;

	DEBUGFUNC("testPsCmdCategory0");
	SWCR_GET_RW_INDEX(ucAction, ucRead, ucIndex);

	DBGLOG(SW4, LOUD, "Read %u Index %u\n", ucRead, ucIndex);

	prStaRec = cnmGetStaRecByIndex(prAdapter, 0);

	if (ucIndex >= TEST_PS_CATA0_INDEX_NUM)
		return;

	if (ucRead == SWCR_WRITE) {
		switch (ucIndex) {
		case TEST_PS_SETUP_BSS:
			testPsSetupBss(prAdapter, ucOpt0);
			break;

		case TEST_PS_ENABLE_BEACON:
			break;

		case TEST_PS_TRIGGER_BMC:
			/* txmForwardQueuedBmcPkts (ucOpt0); */
			break;
		case TEST_PS_SEND_NULL: {
			if (prStaRec == NULL)
				break;
			testPsSendQoSNullFrame(prAdapter, prStaRec,
				/* UP */
				(uint8_t) (g_au4SwCr[1] & 0xFF),
				/* BMC */
				ucOpt0, (u_int8_t) ((g_au4SwCr[1] >> 8) & 0xFF),
				/* BurstEnd */
				(u_int8_t) ((g_au4SwCr[1] >> 16) & 0xFF),
				/* Packet type */
				(u_int8_t) ((g_au4SwCr[1] >> 24) & 0xFF),
				/* PS sesson ID 7: NOACK */
				(uint8_t) ((g_au4SwCr[2]) & 0xFF),
				/* EOSP */
				FALSE
				);
		}
		break;
		case TEST_PS_BUFFER_BMC:
			break;
		case TEST_PS_UPDATE_BEACON:
			bssUpdateBeaconContent(prAdapter,
				ucOpt0 /*networktype */);
			break;

		default:
			break;
		}
	} else {
		switch (ucIndex) {

		case TEST_PS_MAGIC:
			g_au4SwCr[1] = 0x88660011;
			break;

		}
	}
}

#endif /* TEST_PS */

#if TEST_PS

void testPsCmdCategory1(struct ADAPTER *prAdapter,
			uint8_t ucCate, uint8_t ucAction, uint8_t ucOpt0,
			uint8_t ucOpt1)
{
	uint8_t ucIndex, ucRead;
	uint8_t ucWTEntry;
	struct STA_RECORD *prStaRec;

	DEBUGFUNC("testPsCmdCategory1");

	SWCR_GET_RW_INDEX(ucAction, ucRead, ucIndex);

	if (ucOpt0 >= CFG_STA_REC_NUM)
		return;

	prStaRec = cnmGetStaRecByIndex(prAdapter, ucOpt0);

	if (!prStaRec) {
		DBGLOG(SW4, INFO, "prStaRec is NULL, ucOpt0:%d\n", ucOpt0);
		return;
	}

	ucWTEntry = prStaRec->ucWlanIndex;
	if (ucRead == SWCR_WRITE) {

		switch (ucIndex) {
		case TEST_PS_STA_PS:
			prStaRec->fgIsInPS = (u_int8_t) (g_au4SwCr[1] & 0x1);
			prStaRec->fgIsQoS =
				(u_int8_t) (g_au4SwCr[1] >> 8 & 0xFF);
			prStaRec->fgIsUapsdSupported =
				(u_int8_t) (g_au4SwCr[1] >> 16 & 0xFF);
			prStaRec->ucBmpDeliveryAC =
				(u_int8_t) (g_au4SwCr[1] >> 24 & 0xFF);
			break;

		}

	} else {
		/* Read */
		switch (ucIndex) {
		default:
			break;
		}
	}

}

#endif /* TEST_PS */

#if CFG_SUPPORT_802_11V
#if (CFG_SUPPORT_802_11V_TIMING_MEASUREMENT == 1) && (WNM_UNIT_TEST == 1)
void testWNMCmdCategory0(struct ADAPTER *prAdapter,
			 uint8_t ucCate, uint8_t ucAction, uint8_t ucOpt0,
			 uint8_t ucOpt1)
{
	uint8_t ucIndex, ucRead;
	struct STA_RECORD *prStaRec;

	DEBUGFUNC("testWNMCmdCategory0");
	SWCR_GET_RW_INDEX(ucAction, ucRead, ucIndex);

	DBGLOG(SW4, INFO, "Read %u Index %u\n", ucRead, ucIndex);

	if (ucIndex >= TEST_WNM_CATA0_INDEX_NUM)
		return;

	if (ucRead == SWCR_WRITE) {
		switch (ucIndex) {
		case TEST_WNM_TIMING_MEAS:
			wnmTimingMeasUnitTest1(prAdapter, ucOpt0);
			break;

		default:
			break;
		}
	}
}
#endif /* TEST_WNM */
#endif /* CFG_SUPPORT_802_11V */

void swCtrlSwCr(struct ADAPTER *prAdapter, uint8_t ucRead,
		uint16_t u2Addr, uint32_t *pu4Data)
{
	/* According other register STAIDX */
	uint8_t ucOffset;

	ucOffset = (u2Addr >> 2) & 0x3F;

	if (ucOffset >= SWCR_CR_NUM)
		return;

	if (ucRead == SWCR_WRITE) {
		g_au4SwCr[ucOffset] = *pu4Data;
		if (ucOffset == 0x0) {
			/* Commmand   [31:24]: Category */
			/* Commmand   [23:23]: 1(W) 0(R) */
			/* Commmand   [22:16]: Index */
			/* Commmand   [15:08]: Option0  */
			/* Commmand   [07:00]: Option1   */
			uint8_t ucCate;
			uint32_t u4Cmd;

			u4Cmd = g_au4SwCr[0];
			ucCate = (uint8_t) (u4Cmd >> 24);
			if (ucCate < ARRAY_SIZE(g_arSwCtrlCmd)) {
				if (g_arSwCtrlCmd[ucCate] != NULL) {
					g_arSwCtrlCmd[ucCate] (
						prAdapter, ucCate,
						(uint8_t) (u4Cmd >> 16 & 0xFF),
						(uint8_t) ((u4Cmd >> 8) & 0xFF),
						(uint8_t) (u4Cmd & 0xFF));
				}
			}
		}
	} else {
		*pu4Data = g_au4SwCr[ucOffset];
	}
}

void swCrReadWriteCmd(struct ADAPTER *prAdapter,
		      uint8_t ucRead, uint16_t u2Addr, uint32_t *pu4Data)
{
	uint8_t ucMod;

	ucMod = u2Addr >> 8;
	/* Address [15:8] MOD ID */
	/* Address [7:0] OFFSET */

	DEBUGFUNC("swCrReadWriteCmd");
	DBGLOG(SW4, INFO, "%u addr 0x%x data 0x%x\n", ucRead,
	       u2Addr, *pu4Data);

	if (ucMod < (ARRAY_SIZE(g_arSwCrModHandle))) {

		if (g_arSwCrModHandle[ucMod] != NULL)
			g_arSwCrModHandle[ucMod] (prAdapter, ucRead, u2Addr,
						  pu4Data);
	}			/* ucMod */
}

/* Debug Support */
void swCrFrameCheckEnable(struct ADAPTER *prAdapter,
			  uint32_t u4DumpType)
{
	g_u4SwcrDebugFrameDumpType = u4DumpType;
#if CFG_RX_PKTS_DUMP
	prAdapter->rRxCtrl.u4RxPktsDumpTypeMask = u4DumpType;
#endif
}

void swCrDebugInit(struct ADAPTER *prAdapter)
{
	/* frame dump */
	if (g_u4SwcrDebugFrameDumpType)
		swCrFrameCheckEnable(prAdapter, g_u4SwcrDebugFrameDumpType);
	/* debug counter */
	g_fgSwcrDebugTimer = FALSE;

	cnmTimerInitTimer(prAdapter, &g_rSwcrDebugTimer,
			  (PFN_MGMT_TIMEOUT_FUNC) swCrDebugCheckTimeout,
			  (unsigned long) NULL);

	if (g_u4SwcrDebugCheckTimeout)
		swCrDebugCheckEnable(prAdapter, TRUE,
			g_ucSwcrDebugCheckType, g_u4SwcrDebugCheckTimeout);
}

void swCrDebugUninit(struct ADAPTER *prAdapter)
{
	cnmTimerStopTimer(prAdapter, &g_rSwcrDebugTimer);

	g_fgSwcrDebugTimer = FALSE;
}

void swCrDebugCheckEnable(struct ADAPTER *prAdapter,
	u_int8_t fgIsEnable, uint8_t ucType, uint32_t u4Timeout)
{
	if (fgIsEnable) {
		g_ucSwcrDebugCheckType = ucType;
		g_u4SwcrDebugCheckTimeout = u4Timeout;
		if (g_fgSwcrDebugTimer == FALSE)
			swCrDebugCheckTimeout(prAdapter, 0);
	} else {
		cnmTimerStopTimer(prAdapter, &g_rSwcrDebugTimer);
		g_u4SwcrDebugCheckTimeout = 0;
	}

	g_fgSwcrDebugTimer = fgIsEnable;
}

void swCrDebugCheck(struct ADAPTER *prAdapter,
		    struct CMD_SW_DBG_CTRL *prCmdSwCtrl)
{
	struct RX_CTRL *prRxCtrl;
	struct TX_CTRL *prTxCtrl;

	ASSERT(prAdapter);

	prTxCtrl = &prAdapter->rTxCtrl;
	prRxCtrl = &prAdapter->rRxCtrl;

	/* dump counters */
	if (prCmdSwCtrl) {
		if (prCmdSwCtrl->u4Data == SWCR_DBG_TYPE_ALL) {

			/* TX Counter from fw */
			DBGLOG(SW4, INFO, "TX0\n"
				"%08x %08x %08x %08x\n"
				"%08x %08x %08x %08x\n",
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_TX_CNT],
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_TX_BCN_CNT],
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_TX_FAILED_CNT],
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_TX_RETRY_CNT],
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_TX_AGING_TIMEOUT_CNT],
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_TX_PS_OVERFLOW_CNT],
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_TX_MGNT_DROP_CNT],
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_TX_ERROR_CNT]);
#if 1
			/* TX Counter from drv */
			DBGLOG(SW4, INFO, "TX1\n"
				"%08x %08x %08x %08x\n",
				(uint32_t) TX_GET_CNT(prTxCtrl,
				TX_INACTIVE_BSS_DROP),
				(uint32_t) TX_GET_CNT(prTxCtrl,
				TX_INACTIVE_STA_DROP),
				(uint32_t) TX_GET_CNT(prTxCtrl,
				TX_FORWARD_OVERFLOW_DROP),
				(uint32_t) TX_GET_CNT(prTxCtrl,
				TX_AP_BORADCAST_DROP));
#endif

			/* RX Counter */
			DBGLOG(SW4, INFO, "RX0\n"
				"%08x %08x %08x %08x\n"
				"%08x %08x %08x %08x\n"
				"%08x %08x %08x %08x\n"
				"%08x %08x %08x %08x\n",
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_RX_CNT],
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_RX_DROP_CNT],
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_RX_DUP_DROP_CNT],
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_RX_TYPE_ERROR_DROP_CNT],
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_RX_CLASS_ERROR_DROP_CNT],
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_RX_AMPDU_ERROR_DROP_CNT],
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_RX_STATUS_ERROR_DROP_CNT],
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_RX_FORMAT_ERROR_DROP_CNT],
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_RX_ICV_ERROR_DROP_CNT],
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_RX_KEY_ERROR_DROP_CNT],
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_RX_TKIP_ERROR_DROP_CNT],
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_RX_MIC_ERROR_DROP_CNT],
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_RX_BIP_ERROR_DROP_CNT],
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_RX_FCSERR_CNT],
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_RX_FIFOFULL_CNT],
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_RX_PFDROP_CNT]);

			DBGLOG(SW4, INFO, "RX1\n"
				"%08x %08x %08x %08x\n"
				"%08x %08x %08x %08x\n",
				(uint32_t) RX_GET_CNT(prRxCtrl,
				RX_MPDU_TOTAL_COUNT),
				(uint32_t) RX_GET_CNT(prRxCtrl,
				RX_DATA_INDICATION_COUNT),
				(uint32_t) RX_GET_CNT(prRxCtrl,
				RX_DATA_RETURNED_COUNT),
				(uint32_t) RX_GET_CNT(prRxCtrl,
				RX_DATA_RETAINED_COUNT),
				(uint32_t) RX_GET_CNT(prRxCtrl,
				RX_DROP_TOTAL_COUNT),
				(uint32_t) RX_GET_CNT(prRxCtrl,
				RX_TYPE_ERR_DROP_COUNT),
				(uint32_t) RX_GET_CNT(prRxCtrl,
				RX_CLASS_ERR_DROP_COUNT),
				(uint32_t) RX_GET_CNT(prRxCtrl,
				RX_DST_NULL_DROP_COUNT));

			DBGLOG(SW4, INFO, "PWR\n"
				"%08x %08x %08x %08x\n"
				"%08x %08x %08x %08x\n",
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_PWR_PS_POLL_CNT],
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_PWR_TRIGGER_NULL_CNT],
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_PWR_BCN_IND_CNT],
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_PWR_BCN_TIMEOUT_CNT],
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_PWR_PM_STATE0],
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_PWR_PM_STATE1],
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_PWR_CUR_PS_PROF0],
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_PWR_CUR_PS_PROF1]);

			DBGLOG(SW4, INFO, "ARM\n"
				"%08x %08x %08x %08x\n"
				"%08x %08x\n",
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_AR_STA0_RATE],
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_AR_STA0_BWGI],
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_AR_STA0_RX_RATE_RCPI],
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_ROAMING_ENABLE],
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_ROAMING_ROAM_CNT],
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_ROAMING_INT_CNT]);

			DBGLOG(SW4, INFO, "BB\n"
				"%08x %08x %08x %08x\n"
				"%08x %08x %08x %08x\n",
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_BB_RX_MDRDY_CNT],
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_BB_RX_FCSERR_CNT],
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_BB_CCK_PD_CNT],
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_BB_OFDM_PD_CNT],
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_BB_CCK_SFDERR_CNT],
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_BB_CCK_SIGERR_CNT],
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_BB_OFDM_TAGERR_CNT],
				prCmdSwCtrl->u4DebugCnt[
				SWCR_DBG_ALL_BB_OFDM_SIGERR_CNT]);

		}
	}
	/* start the next check */
	if (g_u4SwcrDebugCheckTimeout)
		cnmTimerStartTimer(prAdapter, &g_rSwcrDebugTimer,
				   g_u4SwcrDebugCheckTimeout * MSEC_PER_SEC);
}

void swCrDebugCheckTimeout(IN struct ADAPTER *prAdapter,
			   unsigned long ulParamPtr)
{
	struct CMD_SW_DBG_CTRL rCmdSwCtrl;
	uint32_t rStatus;

	rCmdSwCtrl.u4Id = (0xb000 << 16) + g_ucSwcrDebugCheckType;
	rCmdSwCtrl.u4Data = 0;
	rStatus = wlanSendSetQueryCmd(prAdapter,	/* prAdapter */
		CMD_ID_SW_DBG_CTRL,	/* ucCID */
		FALSE,	/* fgSetQuery */
		TRUE,	/* fgNeedResp */
		FALSE,	/* fgIsOid */
		swCrDebugQuery,	/* pfCmdDoneHandler */
		swCrDebugQueryTimeout, /* pfCmdTimeoutHandler */
		sizeof(struct CMD_SW_DBG_CTRL),	/* u4SetQueryInfoLen */
		(uint8_t *) &rCmdSwCtrl, /* pucInfoBuffer */
		NULL,	/* pvSetQueryBuffer */
		0	/* u4SetQueryBufferLen */
		);

	ASSERT(rStatus == WLAN_STATUS_PENDING);
}

void swCrDebugQuery(IN struct ADAPTER *prAdapter,
		    IN struct CMD_INFO *prCmdInfo, IN uint8_t *pucEventBuf)
{
	ASSERT(prAdapter);

	swCrDebugCheck(prAdapter,
		       (struct CMD_SW_DBG_CTRL *) (pucEventBuf));
}

void swCrDebugQueryTimeout(IN struct ADAPTER *prAdapter,
			   IN struct CMD_INFO *prCmdInfo)
{
	ASSERT(prAdapter);

	swCrDebugCheck(prAdapter, NULL);
}

#endif /* CFG_SUPPORT_SWCR */
