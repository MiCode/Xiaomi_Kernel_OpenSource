/*******************************************************************************
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
 ******************************************************************************/
/*! \file   wlan_lib.c
 *    \brief  Internal driver stack will export the required procedures here for
 *            GLUE Layer.
 *
 *    This file contains all routines which are exported from MediaTek 802.11
 *    Wireless LAN driver stack to GLUE Layer.
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
#include "mgmt/ais_fsm.h"

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
/* 6.1.1.2 Interpretation of priority parameter in MAC service primitives */
/* Static convert the Priority Parameter/TID(User Priority/TS Identifier) to
 * Traffic Class
 */
const uint8_t aucPriorityParam2TC[] = {
	TC1_INDEX,
	TC0_INDEX,
	TC0_INDEX,
	TC1_INDEX,
	TC2_INDEX,
	TC2_INDEX,
	TC3_INDEX,
	TC3_INDEX
};

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */
struct CODE_MAPPING {
	uint32_t u4RegisterValue;
	int32_t u4TxpowerOffset;
};

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */
u_int8_t fgIsBusAccessFailed = FALSE;
struct MIB_INFO_STAT g_arMibInfo[ENUM_BAND_NUM];

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */
#if CFG_RCPI_COMPENSATION
static u_int8_t RxFELoss[MAX_ANTENNA_NUM][FELOSS_CH_GROUP_NUM];

static const u_int16_t FELossOffset[MAX_ANTENNA_NUM][FELOSS_CH_GROUP_NUM] = {
	{G_BAND_WF0_FELOSS, A_BAND_WF0_LB_FELOSS,
	A_BAND_WF0_MB_FELOSS, A_BAND_WF0_HB_FELOSS},
	{G_BAND_WF1_FELOSS, A_BAND_WF1_LB_FELOSS,
	A_BAND_WF1_MB_FELOSS, A_BAND_WF1_HB_FELOSS}
};
#endif

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */
/* HIF suspend should wait for cfg80211 suspend done */
#define HIF_SUSPEND_MAX_WAIT_TIME 50 /* unit: 5ms */

#define SIGNED_EXTEND(n, _sValue) \
	(((_sValue) & BIT((n)-1)) ? ((_sValue) | BITS(n, 31)) : \
	 ((_sValue) & ~BITS(n, 31)))

/* TODO: Check */
/* OID set handlers without the need to access HW register */
PFN_OID_HANDLER_FUNC apfnOidSetHandlerWOHwAccess[] = {
	wlanoidSetChannel,
	wlanoidSetBeaconInterval,
	wlanoidSetAtimWindow,
	wlanoidSetFrequency,
};

/* TODO: Check */
/* OID query handlers without the need to access HW register */
PFN_OID_HANDLER_FUNC apfnOidQueryHandlerWOHwAccess[] = {
	wlanoidQueryBssid,
	wlanoidQuerySsid,
	wlanoidQueryInfrastructureMode,
	wlanoidQueryAuthMode,
	wlanoidQueryEncryptionStatus,
	wlanoidQueryPmkid,
	wlanoidQueryNetworkTypeInUse,
	wlanoidQueryBssidList,
	wlanoidQueryAcpiDevicePowerState,
	wlanoidQuerySupportedRates,
	wlanoidQueryDesiredRates,
	wlanoidQuery802dot11PowerSaveProfile,
	wlanoidQueryBeaconInterval,
	wlanoidQueryAtimWindow,
	wlanoidQueryFrequency,
};

/* OID set handlers allowed in RF test mode */
PFN_OID_HANDLER_FUNC apfnOidSetHandlerAllowedInRFTest[] = {
	wlanoidRftestSetTestMode,
	wlanoidRftestSetAbortTestMode,
	wlanoidRftestSetAutoTest,
	wlanoidSetMcrWrite,
	wlanoidSetEepromWrite
};

/* OID query handlers allowed in RF test mode */
PFN_OID_HANDLER_FUNC apfnOidQueryHandlerAllowedInRFTest[] = {
	wlanoidRftestQueryAutoTest,
	wlanoidQueryMcrRead,
	wlanoidQueryEepromRead
}

;

PFN_OID_HANDLER_FUNC apfnOidWOTimeoutCheck[] = {
	wlanoidRftestSetTestMode,
	wlanoidRftestSetAbortTestMode,
	wlanoidSetAcpiDevicePowerState,
};

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
/*----------------------------------------------------------------------------*/
/*!
 * \brief This is a private routine, which is used to check if HW access is
 *        needed for the OID query/ set handlers.
 *
 * \param[IN] pfnOidHandler Pointer to the OID handler.
 * \param[IN] fgSetInfo     It is a Set information handler.
 *
 * \retval TRUE This function needs HW access
 * \retval FALSE This function does not need HW access
 */
/*----------------------------------------------------------------------------*/
u_int8_t wlanIsHandlerNeedHwAccess(IN PFN_OID_HANDLER_FUNC
				   pfnOidHandler, IN u_int8_t fgSetInfo)
{
	PFN_OID_HANDLER_FUNC *apfnOidHandlerWOHwAccess;
	uint32_t i;
	uint32_t u4NumOfElem;

	if (fgSetInfo) {
		apfnOidHandlerWOHwAccess = apfnOidSetHandlerWOHwAccess;
		u4NumOfElem = sizeof(apfnOidSetHandlerWOHwAccess) / sizeof(
				      PFN_OID_HANDLER_FUNC);
	} else {
		apfnOidHandlerWOHwAccess = apfnOidQueryHandlerWOHwAccess;
		u4NumOfElem = sizeof(apfnOidQueryHandlerWOHwAccess) /
			      sizeof(PFN_OID_HANDLER_FUNC);
	}

	for (i = 0; i < u4NumOfElem; i++) {
		if (apfnOidHandlerWOHwAccess[i] == pfnOidHandler)
			return FALSE;
	}

	return TRUE;
} /* wlanIsHandlerNeedHwAccess */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to set flag for later handling card
 *        ejected event.
 *
 * \param[in] prAdapter Pointer to the Adapter structure.
 *
 * \return (none)
 *
 * \note When surprised removal happens, Glue layer should invoke this
 *       function to notify WPDD not to do any hw access.
 */
/*----------------------------------------------------------------------------*/
void wlanCardEjected(IN struct ADAPTER *prAdapter)
{
	DEBUGFUNC("wlanCardEjected");
	/* INITLOG(("\n")); */

	ASSERT(prAdapter);

	/* mark that the card is being ejected, NDIS will shut us down soon */
	nicTxRelease(prAdapter, FALSE);

} /* wlanCardEjected */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to check driver ready state
 *
 * \param[in] prGlueInfo Pointer to the GlueInfo structure.
 *
 * \retval TRUE Driver is ready for kernel access
 * \retval FALSE Driver is not ready
 */
/*----------------------------------------------------------------------------*/
u_int8_t wlanIsDriverReady(IN struct GLUE_INFO *prGlueInfo)
{
	return prGlueInfo && prGlueInfo->u4ReadyFlag && !kalIsResetting();
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Create adapter object
 *
 * \param prAdapter This routine is call to allocate the driver software
 *		    objects. If fails, return NULL.
 * \retval NULL If it fails, NULL is returned.
 * \retval NOT NULL If the adapter was initialized successfully.
 */
/*----------------------------------------------------------------------------*/
struct ADAPTER *wlanAdapterCreate(IN struct GLUE_INFO
				  *prGlueInfo)
{
	struct ADAPTER *prAdpater = (struct ADAPTER *) NULL;

	DEBUGFUNC("wlanAdapterCreate");

	do {
		prAdpater = (struct ADAPTER *) kalMemAlloc(sizeof(
					struct ADAPTER), VIR_MEM_TYPE);

		if (!prAdpater) {
			DBGLOG(INIT, ERROR,
			       "Allocate ADAPTER memory ==> FAILED\n");
			break;
		}
#if QM_TEST_MODE
		g_rQM.prAdapter = prAdpater;
#endif
		kalMemZero(prAdpater, sizeof(struct ADAPTER));
		prAdpater->prGlueInfo = prGlueInfo;

	} while (FALSE);

	return prAdpater;
}				/* wlanAdapterCreate */

/*----------------------------------------------------------------------------*/
/*!
 * \brief Destroy adapter object
 *
 * \param prAdapter This routine is call to destroy the driver software objects.
 *                  If fails, return NULL.
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
void wlanAdapterDestroy(IN struct ADAPTER *prAdapter)
{
	if (!prAdapter)
		return;

	scanLogCacheFlushAll(&(prAdapter->rWifiVar.rScanInfo.rScanLogCache),
		LOG_SCAN_D2D, SCAN_LOG_MSG_MAX_LEN);

	kalMemFree(prAdapter, VIR_MEM_TYPE, sizeof(struct ADAPTER));
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Initialize the adapter. The sequence is
 *        1. Disable interrupt
 *        2. Read adapter configuration from EEPROM and registry, verify chip
 *	     ID.
 *        3. Create NIC Tx/Rx resource.
 *        4. Initialize the chip
 *        5. Initialize the protocol
 *        6. Enable Interrupt
 *
 * \param prAdapter      Pointer of Adapter Data Structure
 *
 * \retval WLAN_STATUS_SUCCESS: Success
 * \retval WLAN_STATUS_FAILURE: Failed
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanAdapterStart(IN struct ADAPTER *prAdapter,
			  IN struct REG_INFO *prRegInfo)
{
	uint32_t u4Status = WLAN_STATUS_SUCCESS;
	uint32_t i;
	enum ENUM_ADAPTER_START_FAIL_REASON {
		ALLOC_ADAPTER_MEM_FAIL,
		DRIVER_OWN_FAIL,
		INIT_ADAPTER_FAIL,
		INIT_HIFINFO_FAIL,
		RAM_CODE_DOWNLOAD_FAIL,
		WAIT_FIRMWARE_READY_FAIL,
		FAIL_REASON_MAX
	} eFailReason;

	ASSERT(prAdapter);

	DEBUGFUNC("wlanAdapterStart");

	eFailReason = FAIL_REASON_MAX;
	/* 4 <0> Reset variables in ADAPTER_T */
	/* prAdapter->fgIsFwOwn = TRUE; */
	prAdapter->fgIsEnterD3ReqIssued = FALSE;

	prAdapter->u4OwnFailedCount = 0;
	prAdapter->u4OwnFailedLogCount = 0;
	prAdapter->ucHwBssIdNum = BSS_DEFAULT_NUM;
	prAdapter->ucWmmSetNum = BSS_DEFAULT_NUM;
	prAdapter->ucP2PDevBssIdx = BSS_DEFAULT_NUM;
	prAdapter->ucWtblEntryNum = WTBL_SIZE;
	prAdapter->ucTxDefaultWlanIndex = prAdapter->ucWtblEntryNum - 1;
	prAdapter->ucSerState = SER_IDLE_DONE;

	prAdapter->fgEnHifDbgInfo = true;
	prAdapter->u4HifDbgFlag = 0;
	prAdapter->u4HifChkFlag = 0;

	QUEUE_INITIALIZE(&(prAdapter->rPendingCmdQueue));
#if CFG_SUPPORT_MULTITHREAD
	QUEUE_INITIALIZE(&prAdapter->rTxCmdQueue);
	QUEUE_INITIALIZE(&prAdapter->rTxCmdDoneQueue);
#if CFG_FIX_2_TX_PORT
	QUEUE_INITIALIZE(&prAdapter->rTxP0Queue);
	QUEUE_INITIALIZE(&prAdapter->rTxP1Queue);
#else
	for (i = 0; i < TX_PORT_NUM; i++)
		QUEUE_INITIALIZE(&prAdapter->rTxPQueue[i]);
#endif
	QUEUE_INITIALIZE(&prAdapter->rRxQueue);
	QUEUE_INITIALIZE(&prAdapter->rTxDataDoneQueue);
#endif

	/* Initialize rWlanInfo */
	kalMemSet(&(prAdapter->rWlanInfo), 0,
		  sizeof(struct WLAN_INFO));

	/* Initialize aprBssInfo[].
	 * Important: index shall be same when mapping between aprBssInfo[]
	 *            and arBssInfoPool[]. rP2pDevInfo is indexed to final one.
	 */
	for (i = 0; i < MAX_BSSID_NUM; i++)
		prAdapter->aprBssInfo[i] =
			&prAdapter->rWifiVar.arBssInfoPool[i];
	prAdapter->aprBssInfo[prAdapter->ucP2PDevBssIdx] =
		&prAdapter->rWifiVar.rP2pDevInfo;

	/* 4 <0.1> reset fgIsBusAccessFailed */
	fgIsBusAccessFailed = FALSE;

	do {
		u4Status = nicAllocateAdapterMemory(prAdapter);
		if (u4Status != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR,
			       "nicAllocateAdapterMemory Error!\n");
			u4Status = WLAN_STATUS_FAILURE;
			eFailReason = ALLOC_ADAPTER_MEM_FAIL;
#if CFG_ENABLE_KEYWORD_EXCEPTION_MECHANISM
			mtk_wcn_wmt_assert_keyword(WMTDRV_TYPE_WIFI,
				"[Wi-Fi On] nicAllocateAdapterMemory Error!");
#endif
			break;
		}

		prAdapter->u4OsPacketFilter = PARAM_PACKET_FILTER_SUPPORTED;

		DBGLOG(INIT, INFO,
		       "wlanAdapterStart(): Acquiring LP-OWN\n");
		ACQUIRE_POWER_CONTROL_FROM_PM(prAdapter);
		DBGLOG(INIT, INFO,
		       "wlanAdapterStart(): Acquiring LP-OWN-end\n");

#if (CFG_ENABLE_FULL_PM == 0)
		nicpmSetDriverOwn(prAdapter);

		if (prAdapter->fgIsFwOwn == TRUE) {
			DBGLOG(INIT, ERROR, "nicpmSetDriverOwn() failed!\n");
			u4Status = WLAN_STATUS_FAILURE;
			eFailReason = DRIVER_OWN_FAIL;
#if CFG_ENABLE_KEYWORD_EXCEPTION_MECHANISM
			mtk_wcn_wmt_assert_keyword(WMTDRV_TYPE_WIFI,
				"[Wi-Fi On] nicpmSetDriverOwn() failed!");
#endif
			break;
		}
#endif

		/* 4 <1> Initialize the Adapter */
		u4Status = nicInitializeAdapter(prAdapter);
		if (u4Status != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, "nicInitializeAdapter failed!\n");
			u4Status = WLAN_STATUS_FAILURE;
			eFailReason = INIT_ADAPTER_FAIL;
			break;
		}

		/* 4 <2.1> Initialize System Service (MGMT Memory pool and
		 *	   STA_REC)
		 */
		nicInitSystemService(prAdapter);

		/* 4 <2.2> Initialize Feature Options */
		wlanInitFeatureOption(prAdapter);
#if CFG_SUPPORT_MTK_SYNERGY
		if (kalIsConfigurationExist(prAdapter->prGlueInfo) == TRUE) {
			if (prRegInfo->prNvramSettings->u2FeatureReserved &
			    BIT(MTK_FEATURE_2G_256QAM_DISABLED))
				prAdapter->rWifiVar.aucMtkFeature[0] &=
					~(MTK_SYNERGY_CAP_SUPPORT_24G_MCS89);
		}
#endif

		/* 4 <2.3> Overwrite debug level settings */
		wlanCfgSetDebugLevel(prAdapter);

		/* 4 <3> Initialize Tx */
		nicTxInitialize(prAdapter);
		wlanDefTxPowerCfg(prAdapter);

		/* 4 <4> Initialize Rx */
		nicRxInitialize(prAdapter);

		/* 4 <5> HIF SW info initialize */
		if (!halHifSwInfoInit(prAdapter)) {
			DBGLOG(INIT, ERROR, "halHifSwInfoInit failed!\n");
			u4Status = WLAN_STATUS_FAILURE;
			eFailReason = INIT_HIFINFO_FAIL;
			break;
		}

		u4Status = wlanWakeUpWiFi(prAdapter);
		if (u4Status != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, "wlanWakeUpWiFi failed!\n");
			u4Status = WLAN_STATUS_FAILURE;
			break;
		}

		/* 4 <6> Enable HIF cut-through to N9 mode, not visiting CR4 */
		HAL_ENABLE_FWDL(prAdapter, TRUE);

		/* 4 <7> Get ECO Version */
		u4Status = wlanSetChipEcoInfo(prAdapter);

		if (u4Status != WLAN_STATUS_SUCCESS)
			break;


#if CFG_ENABLE_FW_DOWNLOAD
		/* 4 <8> FW/patch download */

		/* 1. disable interrupt, download is done by polling mode only
		 */
		nicDisableInterrupt(prAdapter);

		/* 2. Initialize Tx Resource to fw download state */
		nicTxInitResetResource(prAdapter);

		u4Status = wlanDownloadFW(prAdapter);

		if (u4Status != WLAN_STATUS_SUCCESS) {
			eFailReason = RAM_CODE_DOWNLOAD_FAIL;
			break;
		}
#endif

		DBGLOG(INIT, INFO, "Waiting for Ready bit..\n");

		/* 4 <9> check Wi-Fi FW asserts ready bit */
		u4Status = wlanCheckWifiFunc(prAdapter, TRUE);

		if (u4Status == WLAN_STATUS_SUCCESS) {
#if defined(_HIF_SDIO)
			uint32_t *pu4WHISR = NULL;
			uint16_t au2TxCount[16];

			pu4WHISR = (uint32_t *)kalMemAlloc(sizeof(uint32_t),
							   PHY_MEM_TYPE);
			if (!pu4WHISR) {
				DBGLOG(INIT, ERROR,
				       "Allocate pu4WHISR fail\n");
				u4Status = WLAN_STATUS_FAILURE;
				break;
			}
			/* 1. reset interrupt status */
			HAL_READ_INTR_STATUS(prAdapter, sizeof(uint32_t),
					     (uint8_t *)pu4WHISR);
			if (HAL_IS_TX_DONE_INTR(*pu4WHISR))
				HAL_READ_TX_RELEASED_COUNT(prAdapter,
							   au2TxCount);

			if (pu4WHISR)
				kalMemFree(pu4WHISR, PHY_MEM_TYPE,
					   sizeof(uint32_t));
#endif
			/* Set FW download success flag */
			prAdapter->fgIsFwDownloaded = TRUE;

			/* 2. query & reset TX Resource for normal operation */
			wlanQueryNicResourceInformation(prAdapter);

#if (CFG_SUPPORT_NIC_CAPABILITY == 1)

			/* 2.9 Workaround for Capability CMD packet lost issue
			 */
			wlanSendDummyCmd(prAdapter, TRUE);

			/* 3. query for NIC capability */
			if (prAdapter->chip_info->isNicCapV1)
				wlanQueryNicCapability(prAdapter);

			/* 4. query for NIC capability V2 */
			wlanQueryNicCapabilityV2(prAdapter);

			/* 5. reset TX Resource for normal operation
			 *    based on the information reported from
			 *    CMD_NicCapabilityV2
			 */
			wlanUpdateNicResourceInformation(prAdapter);

			wlanPrintVersion(prAdapter);
#endif

			/* 6. update basic configuration */
			wlanUpdateBasicConfig(prAdapter);

			/* 7. Override network address */
			wlanUpdateNetworkAddress(prAdapter);

			/* 8. Apply Network Address */
			nicApplyNetworkAddress(prAdapter);

			/* 9. indicate disconnection as default status */
			kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
					WLAN_STATUS_MEDIA_DISCONNECT, NULL, 0);
		}

		RECLAIM_POWER_CONTROL_TO_PM(prAdapter, FALSE);

		if (u4Status != WLAN_STATUS_SUCCESS) {
			eFailReason = WAIT_FIRMWARE_READY_FAIL;
			break;
		}

		/* OID timeout timer initialize */
		cnmTimerInitTimer(prAdapter,
				  &prAdapter->rOidTimeoutTimer,
				  (PFN_MGMT_TIMEOUT_FUNC) wlanReleasePendingOid,
				  (unsigned long) NULL);

		prAdapter->ucOidTimeoutCount = 0;

		prAdapter->fgIsChipNoAck = FALSE;

		/* Return Indicated Rfb list timer */
		cnmTimerInitTimer(prAdapter,
				  &prAdapter->rPacketDelaySetupTimer,
				  (PFN_MGMT_TIMEOUT_FUNC)
					wlanReturnPacketDelaySetupTimeout,
				  (unsigned long) NULL);

		/* Power state initialization */
		prAdapter->fgWiFiInSleepyState = FALSE;
		prAdapter->rAcpiState = ACPI_STATE_D0;

#if 0
		/* Online scan option */
		if (prRegInfo->fgDisOnlineScan == 0)
			prAdapter->fgEnOnlineScan = TRUE;
		else
			prAdapter->fgEnOnlineScan = FALSE;

		/* Beacon lost detection option */
		if (prRegInfo->fgDisBcnLostDetection != 0)
			prAdapter->fgDisBcnLostDetection = TRUE;
#else
		if (prAdapter->rWifiVar.fgDisOnlineScan == 0)
			prAdapter->fgEnOnlineScan = TRUE;
		else
			prAdapter->fgEnOnlineScan = FALSE;

		/* Beacon lost detection option */
		if (prAdapter->rWifiVar.fgDisBcnLostDetection != 0)
			prAdapter->fgDisBcnLostDetection = TRUE;
#endif

		/* Load compile time constant */
		prAdapter->rWlanInfo.u2BeaconPeriod =
			CFG_INIT_ADHOC_BEACON_INTERVAL;
		prAdapter->rWlanInfo.u2AtimWindow =
			CFG_INIT_ADHOC_ATIM_WINDOW;

#if 1				/* set PM parameters */
		prAdapter->u4PsCurrentMeasureEn =
			prRegInfo->u4PsCurrentMeasureEn;
#if 0
		prAdapter->fgEnArpFilter = prRegInfo->fgEnArpFilter;
		prAdapter->u4UapsdAcBmp = prRegInfo->u4UapsdAcBmp;
		prAdapter->u4MaxSpLen = prRegInfo->u4MaxSpLen;
#else
		prAdapter->fgEnArpFilter =
			prAdapter->rWifiVar.fgEnArpFilter;
		prAdapter->u4UapsdAcBmp = prAdapter->rWifiVar.u4UapsdAcBmp;
		prAdapter->u4MaxSpLen = prAdapter->rWifiVar.u4MaxSpLen;
#endif
		DBGLOG(INIT, TRACE,
		       "[1] fgEnArpFilter:0x%x, u4UapsdAcBmp:0x%x, u4MaxSpLen:0x%x",
		       prAdapter->fgEnArpFilter, prAdapter->u4UapsdAcBmp,
		       prAdapter->u4MaxSpLen);

		prAdapter->fgEnCtiaPowerMode = FALSE;

#endif
		/* QA_TOOL and ICAP info struct */
		prAdapter->rIcapInfo.fgCaptureDone = FALSE;
		prAdapter->rIcapInfo.fgIcapEnable = FALSE;
		prAdapter->rIcapInfo.u2DumpIndex = 0;
		prAdapter->rIcapInfo.u4CapNode = 0;

		/* MGMT Initialization */
		nicInitMGMT(prAdapter, prRegInfo);
		/* NCHO Initialization */
#if CFG_SUPPORT_NCHO
		prAdapter->rNchoInfo.fgECHOEnabled = FALSE;
		prAdapter->rNchoInfo.eBand = NCHO_BAND_AUTO;
		prAdapter->rNchoInfo.fgChGranted = FALSE;
		prAdapter->rNchoInfo.fgIsSendingAF = FALSE;
		prAdapter->rNchoInfo.u4RoamScanControl = FALSE;
		prAdapter->rNchoInfo.rRoamScnChnl.ucChannelListNum = 0;
		prAdapter->rNchoInfo.rRoamScnChnl.arChnlInfoList[0].eBand =
			BAND_2G4;
		prAdapter->rNchoInfo.rRoamScnChnl.arChnlInfoList[0].ucChannelNum
			= 1;
		prAdapter->rNchoInfo.eDFSScnMode = NCHO_DFS_SCN_ENABLE1;
		prAdapter->rNchoInfo.i4RoamTrigger = -70;
		prAdapter->rNchoInfo.i4RoamDelta = 5;
		prAdapter->rNchoInfo.u4RoamScanPeriod =
			ROAMING_DISCOVERY_TIMEOUT_SEC;
		prAdapter->rNchoInfo.u4ScanChannelTime = 50;
		prAdapter->rNchoInfo.u4ScanHomeTime = 120;
		prAdapter->rNchoInfo.u4ScanHomeawayTime = 120;
		prAdapter->rNchoInfo.u4ScanNProbes = 2;
		prAdapter->rNchoInfo.u4WesMode = 0;
#endif

		/* Enable WZC Disassociation */
		prAdapter->rWifiVar.fgSupportWZCDisassociation = TRUE;

		/* Apply Rate Setting */
		if ((enum ENUM_REGISTRY_FIXED_RATE) (prRegInfo->u4FixedRate)
		    < FIXED_RATE_NUM)
			prAdapter->rWifiVar.eRateSetting =
					(enum ENUM_REGISTRY_FIXED_RATE)
					(prRegInfo->u4FixedRate);
		else
			prAdapter->rWifiVar.eRateSetting = FIXED_RATE_NONE;

		if (prAdapter->rWifiVar.eRateSetting == FIXED_RATE_NONE) {
			/* Enable Auto (Long/Short) Preamble */
			prAdapter->rWifiVar.ePreambleType = PREAMBLE_TYPE_AUTO;
		} else if ((prAdapter->rWifiVar.eRateSetting >=
			    FIXED_RATE_MCS0_20M_400NS &&
			    prAdapter->rWifiVar.eRateSetting <=
			    FIXED_RATE_MCS7_20M_400NS)
			   || (prAdapter->rWifiVar.eRateSetting >=
			       FIXED_RATE_MCS0_40M_400NS &&
			       prAdapter->rWifiVar.eRateSetting <=
			       FIXED_RATE_MCS32_400NS)) {
			/* Force Short Preamble */
			prAdapter->rWifiVar.ePreambleType = PREAMBLE_TYPE_SHORT;
		} else {
			/* Force Long Preamble */
			prAdapter->rWifiVar.ePreambleType = PREAMBLE_TYPE_LONG;
		}

		/* Disable Hidden SSID Join */
		prAdapter->rWifiVar.fgEnableJoinToHiddenSSID = FALSE;

		/* Enable Short Slot Time */
		prAdapter->rWifiVar.fgIsShortSlotTimeOptionEnable = TRUE;

		/* configure available PHY type set */
		nicSetAvailablePhyTypeSet(prAdapter);

#if 0				/* Marked for MT6630 */
#if 1				/* set PM parameters */
		{
#if CFG_SUPPORT_PWR_MGT
			prAdapter->u4PowerMode = prRegInfo->u4PowerMode;
#if CFG_ENABLE_WIFI_DIRECT
			prAdapter->rWlanInfo.
			arPowerSaveMode[NETWORK_TYPE_P2P_INDEX].ucNetTypeIndex
				= NETWORK_TYPE_P2P_INDEX;
			prAdapter->rWlanInfo.
			arPowerSaveMode[NETWORK_TYPE_P2P_INDEX].ucPsProfile
				= ENUM_PSP_FAST_SWITCH;
#endif
#else
			prAdapter->u4PowerMode = ENUM_PSP_CONTINUOUS_ACTIVE;
#endif

			nicConfigPowerSaveProfile(prAdapter,
					  prAdapter->prAisBssInfo->ucBssIndex,
					  prAdapter->u4PowerMode, FALSE);
		}

#endif
#endif

		/* Check hardware 5g band support */
		if (prAdapter->fgIsHw5GBandDisabled)
			prAdapter->fgEnable5GBand = FALSE;
		else
			prAdapter->fgEnable5GBand = TRUE;

#if CFG_SUPPORT_NVRAM
		/* load manufacture data */
		if (kalIsConfigurationExist(prAdapter->prGlueInfo) == TRUE)
			wlanLoadManufactureData(prAdapter, prRegInfo);
		else
			DBGLOG(INIT, WARN, "%s: load manufacture data fail\n",
			       __func__);
#endif

#if 0
		/* Update Auto rate parameters in FW */
		nicRlmArUpdateParms(prAdapter, prRegInfo->u4ArSysParam0,
				    prRegInfo->u4ArSysParam1,
				    prRegInfo->u4ArSysParam2,
				    prRegInfo->u4ArSysParam3);
#endif

		/* Default QM RX BA timeout */
		prAdapter->u4QmRxBaMissTimeout =
			QM_RX_BA_ENTRY_MISS_TIMEOUT_MS;

#if CFG_SUPPORT_LOWLATENCY_MODE
		wlanAdapterStartForLowLatency(prAdapter);
#endif /* CFG_SUPPORT_LOWLATENCY_MODE */

#if (CFG_SUPPORT_GET_MCS_INFO == 1)
		prAdapter->fgIsMcsInfoValid = FALSE;
#endif
	} while (FALSE);

	if (u4Status == WLAN_STATUS_SUCCESS) {

		/* restore to hardware default */
		HAL_SET_INTR_STATUS_READ_CLEAR(prAdapter);
		HAL_SET_MAILBOX_READ_CLEAR(prAdapter, FALSE);

		/* Enable interrupt */
		nicEnableInterrupt(prAdapter);

#if CFG_SUPPORT_SER
#if defined(_HIF_USB)
		cnmTimerInitTimer(prAdapter,
				  &prAdapter->rSerSyncTimer,
				 (PFN_MGMT_TIMEOUT_FUNC) nicSerSyncTimerHandler,
				  (unsigned long) NULL);
		cnmTimerStartTimer(prAdapter,
				   &prAdapter->rSerSyncTimer,
				   WIFI_SER_SYNC_TIMER_TIMEOUT_IN_MS);
#endif	/* _HIF_USB */
#endif	/* CFG_SUPPORT_SER */
	} else {
		prAdapter->u4HifDbgFlag |= DEG_HIF_DEFAULT_DUMP;
		halPrintHifDbgInfo(prAdapter);
		DBGLOG(INIT, WARN, "Fail reason: %d\n", eFailReason);
		/* release allocated memory */
		switch (eFailReason) {
		case WAIT_FIRMWARE_READY_FAIL:
		case RAM_CODE_DOWNLOAD_FAIL:
		case INIT_HIFINFO_FAIL:
			nicRxUninitialize(prAdapter);
			nicTxRelease(prAdapter, FALSE);
			/* System Service Uninitialization */
			nicUninitSystemService(prAdapter);
		/* fallthrough */
		case INIT_ADAPTER_FAIL:
		/* fallthrough */
		case DRIVER_OWN_FAIL:
			nicReleaseAdapterMemory(prAdapter);
			break;
		case ALLOC_ADAPTER_MEM_FAIL:
		default:
			break;
		}
	}
#if CFG_SUPPORT_CUSTOM_NETLINK
	glCustomGenlInit();
#endif

	return u4Status;
}				/* wlanAdapterStart */

/*----------------------------------------------------------------------------*/
/*!
 * \brief Uninitialize the adapter
 *
 * \param prAdapter      Pointer of Adapter Data Structure
 *
 * \retval WLAN_STATUS_SUCCESS: Success
 * \retval WLAN_STATUS_FAILURE: Failed
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanAdapterStop(IN struct ADAPTER *prAdapter)
{
	uint32_t u4Status = WLAN_STATUS_SUCCESS;

	ASSERT(prAdapter);

	/* Release all CMD/MGMT/SEC frame in command queue */
	kalClearCommandQueue(prAdapter->prGlueInfo);

#if CFG_SUPPORT_MULTITHREAD

	/* Flush all items in queues for multi-thread */
	wlanClearTxCommandQueue(prAdapter);

	wlanClearTxCommandDoneQueue(prAdapter);

	wlanClearDataQueue(prAdapter);

	wlanClearRxToOsQueue(prAdapter);

#endif
	/* Hif power off wifi */

	if (prAdapter->rAcpiState == ACPI_STATE_D0 &&
		!wlanIsChipNoAck(prAdapter)
		&& !kalIsCardRemoved(prAdapter->prGlueInfo)) {
		wlanPowerOffWifi(prAdapter);
	 }

	nicRxUninitialize(prAdapter);

	nicTxRelease(prAdapter, FALSE);

	/* MGMT - unitialization */
	nicUninitMGMT(prAdapter);

	/* System Service Uninitialization */
	nicUninitSystemService(prAdapter);

	nicReleaseAdapterMemory(prAdapter);

#if defined(_HIF_SPI)
	/* Note: restore the SPI Mode Select from 32 bit to default */
	nicRestoreSpiDefMode(prAdapter);
#endif

#if CFG_SUPPORT_CUSTOM_NETLINK
	glCustomGenlDeinit();
#endif

	return u4Status;
}				/* wlanAdapterStop */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is called by ISR (interrupt).
 *
 * \param prAdapter      Pointer of Adapter Data Structure
 *
 * \retval TRUE: NIC's interrupt
 * \retval FALSE: Not NIC's interrupt
 */
/*----------------------------------------------------------------------------*/
u_int8_t wlanISR(IN struct ADAPTER *prAdapter,
		 IN u_int8_t fgGlobalIntrCtrl)
{
	ASSERT(prAdapter);

	if (fgGlobalIntrCtrl) {
		nicDisableInterrupt(prAdapter);

		/* wlanIST(prAdapter); */
	}

	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is called by IST (task_let).
 *
 * \param prAdapter      Pointer of Adapter Data Structure
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
void wlanIST(IN struct ADAPTER *prAdapter)
{
	uint32_t u4Status = WLAN_STATUS_SUCCESS;

	ASSERT(prAdapter);

	ACQUIRE_POWER_CONTROL_FROM_PM(prAdapter);

	if (prAdapter->fgIsFwOwn == FALSE) {
		u4Status = nicProcessIST(prAdapter);
		if (u4Status != WLAN_STATUS_SUCCESS &&
			(u4Status != WLAN_STATUS_NOT_INDICATING))
			DBGLOG(REQ, INFO, "Fail: nicProcessIST! status [%d]\n",
			       u4Status);

#if CFG_ENABLE_WAKE_LOCK
		if (KAL_WAKE_LOCK_ACTIVE(prAdapter,
					 &prAdapter->prGlueInfo->rIntrWakeLock))
			KAL_WAKE_UNLOCK(prAdapter,
					&prAdapter->prGlueInfo->rIntrWakeLock);
#endif
	}

	nicEnableInterrupt(prAdapter);

	RECLAIM_POWER_CONTROL_TO_PM(prAdapter, FALSE);


}

void wlanClearPendingInterrupt(IN struct ADAPTER *prAdapter)
{
	uint32_t i;

	i = 0;
	while (i < CFG_IST_LOOP_COUNT
	       && nicProcessIST(prAdapter) != WLAN_STATUS_NOT_INDICATING) {
		i++;
	};
}

uint32_t wlanCheckWifiFunc(IN struct ADAPTER *prAdapter,
			   IN u_int8_t fgRdyChk)
{
	u_int8_t fgResult, fgTimeout;
	uint32_t u4Result, u4Status, u4StartTime, u4CurTime;
	const uint32_t ready_bits =
		prAdapter->chip_info->sw_ready_bits;

	u4StartTime = kalGetTimeTick();
	fgTimeout = FALSE;

#if defined(_HIF_USB)
	if (prAdapter->prGlueInfo->rHifInfo.state ==
	    USB_STATE_LINK_DOWN)
		return WLAN_STATUS_FAILURE;
#endif

	while (TRUE) {
		DBGLOG(INIT, INFO, "Check ready_bits(=0x%x)\n", ready_bits);
		if (fgRdyChk)
			HAL_WIFI_FUNC_READY_CHECK(prAdapter,
					ready_bits /* WIFI_FUNC_READY_BITS */,
					&fgResult);
		else {
			HAL_WIFI_FUNC_OFF_CHECK(prAdapter,
					ready_bits /* WIFI_FUNC_READY_BITS */,
					&fgResult);
#if defined(_HIF_USB) || defined(_HIF_SDIO)
			if (nicProcessIST(prAdapter) !=
				WLAN_STATUS_NOT_INDICATING)
				DBGLOG(INIT, INFO,
				       "Handle pending interrupt\n");
#endif /* _HIF_USB or _HIF_SDIO */
		}
		u4CurTime = kalGetTimeTick();

		if (CHECK_FOR_TIMEOUT(u4CurTime, u4StartTime,
				      CFG_RESPONSE_POLLING_TIMEOUT *
				      CFG_RESPONSE_POLLING_DELAY)) {

			fgTimeout = TRUE;
		}

		if (fgResult) {
			if (fgRdyChk)
				DBGLOG(INIT, INFO, "Ready bit asserted\n");
			else
				DBGLOG(INIT, INFO, "Wi-Fi power off done!\n");

			u4Status = WLAN_STATUS_SUCCESS;

			break;
		} else if (kalIsCardRemoved(prAdapter->prGlueInfo) == TRUE
			   || fgIsBusAccessFailed == TRUE) {
			u4Status = WLAN_STATUS_FAILURE;

			break;
		} else if (fgTimeout) {
			HAL_WIFI_FUNC_GET_STATUS(prAdapter, u4Result);
			DBGLOG(INIT, ERROR,
			       "Waiting for %s: Timeout, Status=0x%08x\n",
			       fgRdyChk ? "ready bit" : "power off", u4Result);
#if CFG_ENABLE_KEYWORD_EXCEPTION_MECHANISM
			mtk_wcn_wmt_assert_keyword(WMTDRV_TYPE_WIFI,
				"[Wi-Fi] [Read WCIR_WLAN_READY fail!]");
#else
			GL_RESET_TRIGGER(prAdapter, RST_FLAG_DO_CORE_DUMP |
					RST_FLAG_PREVENT_POWER_OFF);
#endif
			u4Status = WLAN_STATUS_FAILURE;
			break;
		}
		kalMsleep(CFG_RESPONSE_POLLING_DELAY);

	}

	return u4Status;
}

uint32_t wlanPowerOffWifi(IN struct ADAPTER *prAdapter)
{
	uint32_t rStatus;
	/* Hif power off wifi */
	rStatus = halHifPowerOffWifi(prAdapter);
	prAdapter->fgIsCr4FwDownloaded = FALSE;

	return rStatus;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function will check command queue to find out if any could be
 *	  dequeued and/or send to HIF to MT6620
 *
 * \param prAdapter      Pointer of Adapter Data Structure
 * \param prCmdQue       Pointer of Command Queue (in Glue Layer)
 *
 * \retval WLAN_STATUS_SUCCESS
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanProcessCommandQueue(IN struct ADAPTER
				 *prAdapter, IN struct QUE *prCmdQue)
{
	uint32_t rStatus;
	struct QUE rTempCmdQue, rMergeCmdQue, rStandInCmdQue;
	struct QUE *prTempCmdQue, *prMergeCmdQue, *prStandInCmdQue;
	struct QUE_ENTRY *prQueueEntry;
	struct CMD_INFO *prCmdInfo;
	struct MSDU_INFO *prMsduInfo;
	enum ENUM_FRAME_ACTION eFrameAction = FRAME_ACTION_DROP_PKT;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);
	ASSERT(prCmdQue);

	prTempCmdQue = &rTempCmdQue;
	prMergeCmdQue = &rMergeCmdQue;
	prStandInCmdQue = &rStandInCmdQue;

	QUEUE_INITIALIZE(prTempCmdQue);
	QUEUE_INITIALIZE(prMergeCmdQue);
	QUEUE_INITIALIZE(prStandInCmdQue);

	/* 4 <1> Move whole list of CMD_INFO to temp queue */
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_QUE);
	QUEUE_MOVE_ALL(prTempCmdQue, prCmdQue);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_QUE);

	/* 4 <2> Dequeue from head and check it is able to be sent */
	QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry,
			  struct QUE_ENTRY *);
	while (prQueueEntry) {
		prCmdInfo = (struct CMD_INFO *) prQueueEntry;

		switch (prCmdInfo->eCmdType) {
		case COMMAND_TYPE_NETWORK_IOCTL: {
			struct WIFI_CMD *prWifiCmd = (struct WIFI_CMD *) NULL;
			struct CMD_802_11_KEY *prKey = (struct CMD_802_11_KEY *)
						       NULL;
			struct BSS_INFO *prBssInfo = (struct BSS_INFO *) NULL;

			eFrameAction = FRAME_ACTION_TX_PKT;
			do {
				prWifiCmd = (struct WIFI_CMD *)
						(prCmdInfo->pucInfoBuffer);
				prKey = (struct CMD_802_11_KEY *)
						(prWifiCmd->aucBuffer);
				prBssInfo =
					prAdapter->aprBssInfo[prKey->ucBssIdx];

				if ((prCmdInfo->ucCID != CMD_ID_ADD_REMOVE_KEY)
				    || !prKey->ucTxKey
				    || !prKey->ucAddRemove
				    || (
				    prKey->ucAlgorithmId != CIPHER_SUITE_TKIP &&
				    prKey->ucAlgorithmId != CIPHER_SUITE_CCMP))
					break;
				switch (prBssInfo->eKeyAction) {
				case SEC_DROP_KEY_COMMAND:
					eFrameAction = FRAME_ACTION_DROP_PKT;
					break;
				case SEC_QUEUE_KEY_COMMAND:
					eFrameAction = FRAME_ACTION_QUEUE_PKT;
					break;
				case SEC_TX_KEY_COMMAND:
				default:
					eFrameAction = FRAME_ACTION_TX_PKT;
					break;
				}
			} while (FALSE);
			break;
		}
		case COMMAND_TYPE_GENERAL_IOCTL:
			/* command packet will be always sent */
			eFrameAction = FRAME_ACTION_TX_PKT;
			break;

		case COMMAND_TYPE_SECURITY_FRAME:
			/* inquire with QM */
			prMsduInfo = prCmdInfo->prMsduInfo;

			eFrameAction = qmGetFrameAction(prAdapter,
						prMsduInfo->ucBssIndex,
						prMsduInfo->ucStaRecIndex,
						NULL, FRAME_TYPE_802_1X,
						prCmdInfo->u2InfoBufLen);
			break;

		case COMMAND_TYPE_MANAGEMENT_FRAME:
			/* inquire with QM */
			prMsduInfo = prCmdInfo->prMsduInfo;

			eFrameAction = qmGetFrameAction(prAdapter,
						prMsduInfo->ucBssIndex,
						prMsduInfo->ucStaRecIndex,
						prMsduInfo, FRAME_TYPE_MMPDU,
						prMsduInfo->u2FrameLength);
			break;

		default:
			ASSERT(0);
			break;
		}

		/* 4 <3> handling upon dequeue result */
		if (eFrameAction == FRAME_ACTION_DROP_PKT) {
			DBGLOG(INIT, INFO,
			       "DROP CMD TYPE[%u] ID[0x%02X] SEQ[%u]\n",
			       prCmdInfo->eCmdType, prCmdInfo->ucCID,
			       prCmdInfo->ucCmdSeqNum);
			wlanReleaseCommand(prAdapter, prCmdInfo,
					   TX_RESULT_DROPPED_IN_DRIVER);
			cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
		} else if (eFrameAction == FRAME_ACTION_QUEUE_PKT) {
			DBGLOG(INIT, TRACE,
			       "QUE back CMD TYPE[%u] ID[0x%02X] SEQ[%u]\n",
			       prCmdInfo->eCmdType, prCmdInfo->ucCID,
			       prCmdInfo->ucCmdSeqNum);

			QUEUE_INSERT_TAIL(prMergeCmdQue, prQueueEntry);
		} else if (eFrameAction == FRAME_ACTION_TX_PKT) {
			/* 4 <4> Send the command */
#if CFG_SUPPORT_MULTITHREAD
			rStatus = wlanSendCommandMthread(prAdapter, prCmdInfo);

			if (rStatus == WLAN_STATUS_RESOURCES) {
				/* no more TC4 resource for further
				 * transmission
				 */
				DBGLOG(INIT, WARN,
				       "NO Res CMD TYPE[%u] ID[0x%02X] SEQ[%u]\n",
				       prCmdInfo->eCmdType, prCmdInfo->ucCID,
				       prCmdInfo->ucCmdSeqNum);

				prAdapter->u4HifDbgFlag |= DEG_HIF_ALL;
				kalSetHifDbgEvent(prAdapter->prGlueInfo);

				QUEUE_INSERT_TAIL(prMergeCmdQue, prQueueEntry);

				/*
				 * We reserve one TC4 resource for CMD
				 * specially, only break checking the left tx
				 * request if no resource for true CMD.
				 */
				if ((prCmdInfo->eCmdType !=
				     COMMAND_TYPE_SECURITY_FRAME) &&
				    (prCmdInfo->eCmdType !=
				     COMMAND_TYPE_MANAGEMENT_FRAME))
					break;
			} else if (rStatus == WLAN_STATUS_PENDING) {
				/* Do nothing */
				/* Do nothing */
			} else if (rStatus == WLAN_STATUS_SUCCESS) {
				/* Do nothing */
				/* Do nothing */
			} else {
				struct CMD_INFO *prCmdInfo = (struct CMD_INFO *)
							     prQueueEntry;

				if (prCmdInfo->fgIsOid) {
					kalOidComplete(prAdapter->prGlueInfo,
						       prCmdInfo->fgSetQuery,
						       prCmdInfo->u4SetInfoLen,
						       rStatus);
				}
				cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
			}

#else
			rStatus = wlanSendCommand(prAdapter, prCmdInfo);

			if (rStatus == WLAN_STATUS_RESOURCES) {
				/* no more TC4 resource for further
				 * transmission
				 */

				DBGLOG(INIT, WARN,
				       "NO Resource for CMD TYPE[%u] ID[0x%02X] SEQ[%u]\n",
				       prCmdInfo->eCmdType, prCmdInfo->ucCID,
				       prCmdInfo->ucCmdSeqNum);

				QUEUE_INSERT_TAIL(prMergeCmdQue, prQueueEntry);
				break;
			} else if (rStatus == WLAN_STATUS_PENDING) {
				/* command packet which needs further handling
				 * upon response
				 */
				KAL_ACQUIRE_SPIN_LOCK(prAdapter,
						      SPIN_LOCK_CMD_PENDING);
				QUEUE_INSERT_TAIL(
						&(prAdapter->rPendingCmdQueue),
						prQueueEntry);
				KAL_RELEASE_SPIN_LOCK(prAdapter,
						      SPIN_LOCK_CMD_PENDING);
			} else {
				struct CMD_INFO *prCmdInfo = (struct CMD_INFO *)
							     prQueueEntry;

				if (rStatus == WLAN_STATUS_SUCCESS) {
					if (prCmdInfo->pfCmdDoneHandler) {
						prCmdInfo->pfCmdDoneHandler(
						    prAdapter, prCmdInfo,
						    prCmdInfo->pucInfoBuffer);
					}
				} else {
					if (prCmdInfo->fgIsOid) {
						kalOidComplete(
						    prAdapter->prGlueInfo,
						    prCmdInfo->fgSetQuery,
						    prCmdInfo->u4SetInfoLen,
						    rStatus);
					}
				}

				cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
			}
#endif
		} else {
			ASSERT(0);
		}

		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry,
				  struct QUE_ENTRY *);
	}

	/* 4 <3> Merge back to original queue */
	/* 4 <3.1> Merge prMergeCmdQue & prTempCmdQue */
	QUEUE_CONCATENATE_QUEUES(prMergeCmdQue, prTempCmdQue);

	/* 4 <3.2> Move prCmdQue to prStandInQue, due to prCmdQue might differ
	 *         due to incoming 802.1X frames
	 */
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_QUE);
	QUEUE_MOVE_ALL(prStandInCmdQue, prCmdQue);

	/* 4 <3.3> concatenate prStandInQue to prMergeCmdQue */
	QUEUE_CONCATENATE_QUEUES(prMergeCmdQue, prStandInCmdQue);

	/* 4 <3.4> then move prMergeCmdQue to prCmdQue */
	QUEUE_MOVE_ALL(prCmdQue, prMergeCmdQue);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_QUE);

#if CFG_SUPPORT_MULTITHREAD
	kalSetTxCmdEvent2Hif(prAdapter->prGlueInfo);
#endif

	return WLAN_STATUS_SUCCESS;
}				/* end of wlanProcessCommandQueue() */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function will take CMD_INFO_T which carry some information of
 *        incoming OID and notify the NIC_TX to send CMD.
 *
 * \param prAdapter      Pointer of Adapter Data Structure
 * \param prCmdInfo      Pointer of P_CMD_INFO_T
 *
 * \retval WLAN_STATUS_SUCCESS   : CMD was written to HIF and be freed(CMD Done)
 *				   immediately.
 * \retval WLAN_STATUS_RESOURCE  : No resource for current command, need to wait
 *				   for previous
 *                                 frame finishing their transmission.
 * \retval WLAN_STATUS_FAILURE   : Get failure while access HIF or been
 *				   rejected.
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanSendCommand(IN struct ADAPTER *prAdapter,
			 IN struct CMD_INFO *prCmdInfo)
{
	struct TX_CTRL *prTxCtrl;
	uint8_t ucTC;		/* "Traffic Class" SW(Driver) resource
				 * classification
				 */
	uint32_t rStatus = WLAN_STATUS_SUCCESS;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	prTxCtrl = &prAdapter->rTxCtrl;

	do {
		/* <0> card removal check */
		if (kalIsCardRemoved(prAdapter->prGlueInfo) == TRUE
		    || fgIsBusAccessFailed == TRUE) {
			rStatus = WLAN_STATUS_FAILURE;
			break;
		}

		/* <1.1> Assign Traffic Class(TC) */
		ucTC = nicTxGetCmdResourceType(prCmdInfo);

		/* <1.2> Check if pending packet or resource was exhausted */
		rStatus = nicTxAcquireResource(prAdapter, ucTC,
			       nicTxGetCmdPageCount(prAdapter, prCmdInfo),
			       TRUE);
		if (rStatus == WLAN_STATUS_RESOURCES) {
			DBGLOG(INIT, INFO, "NO Resource:%d\n", ucTC);
			break;
		}
		/* <1.3> Forward CMD_INFO_T to NIC Layer */
		rStatus = nicTxCmd(prAdapter, prCmdInfo, ucTC);

		/* <1.4> Set Pending in response to Query Command/Need Response
		 */
		if (rStatus == WLAN_STATUS_SUCCESS) {
			if ((!prCmdInfo->fgSetQuery) || (prCmdInfo->fgNeedResp))
				rStatus = WLAN_STATUS_PENDING;
		}

	} while (FALSE);

	return rStatus;
}				/* end of wlanSendCommand() */

#if CFG_SUPPORT_MULTITHREAD

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function will take CMD_INFO_T which carry some information of
 *        incoming OID and notify the NIC_TX to send CMD.
 *
 * \param prAdapter      Pointer of Adapter Data Structure
 * \param prCmdInfo      Pointer of P_CMD_INFO_T
 *
 * \retval WLAN_STATUS_SUCCESS   : CMD was written to HIF and be freed(CMD Done)
 *				   immediately.
 * \retval WLAN_STATUS_RESOURCE  : No resource for current command, need to wait
 *				   for previous
 *                                 frame finishing their transmission.
 * \retval WLAN_STATUS_FAILURE   : Get failure while access HIF or been
 *				   rejected.
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanSendCommandMthread(IN struct ADAPTER
				*prAdapter, IN struct CMD_INFO *prCmdInfo)
{
	struct TX_CTRL *prTxCtrl;
	uint8_t ucTC;		/* "Traffic Class" SW(Driver) resource
				 * classification
				 */
	uint32_t rStatus = WLAN_STATUS_SUCCESS;

	struct QUE rTempCmdQue;
	struct QUE *prTempCmdQue;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	prTxCtrl = &prAdapter->rTxCtrl;

	prTempCmdQue = &rTempCmdQue;
	QUEUE_INITIALIZE(prTempCmdQue);

	do {
		/* <0> card removal check */
		if (kalIsCardRemoved(prAdapter->prGlueInfo) == TRUE
		    || fgIsBusAccessFailed == TRUE) {
			rStatus = WLAN_STATUS_FAILURE;
			break;
		}
		/* <1> Normal case of sending CMD Packet */
		/* <1.1> Assign Traffic Class(TC) */
		ucTC = nicTxGetCmdResourceType(prCmdInfo);

		/* <1.2> Check if pending packet or resource was exhausted */
		rStatus = nicTxAcquireResource(prAdapter, ucTC,
			       nicTxGetCmdPageCount(prAdapter, prCmdInfo),
			       TRUE);
		if (rStatus == WLAN_STATUS_RESOURCES) {
#if 0
			DBGLOG(INIT, WARN,
			       "%s: NO Resource for CMD TYPE[%u] ID[0x%02X] SEQ[%u] TC[%u]\n",
			       __func__, prCmdInfo->eCmdType, prCmdInfo->ucCID,
			       prCmdInfo->ucCmdSeqNum, ucTC);
#endif
			break;
		}

		/* Process to pending command queue firest */
		if ((!prCmdInfo->fgSetQuery) || (prCmdInfo->fgNeedResp)) {
			/* command packet which needs further handling upon
			 * response
			 */
			/*
			 *  KAL_ACQUIRE_SPIN_LOCK(prAdapter,
			 *			  SPIN_LOCK_CMD_PENDING);
			 *  QUEUE_INSERT_TAIL(&(prAdapter->rPendingCmdQueue),
			 *		      (struct QUE_ENTRY *)prCmdInfo);
			 *  KAL_RELEASE_SPIN_LOCK(prAdapter,
			 *			  SPIN_LOCK_CMD_PENDING);
			 */
		}
		QUEUE_INSERT_TAIL(prTempCmdQue,
				  (struct QUE_ENTRY *) prCmdInfo);

		/* <1.4> Set Pending in response to Query Command/Need Response
		 */
		if (rStatus == WLAN_STATUS_SUCCESS) {
			if ((!prCmdInfo->fgSetQuery) ||
			    (prCmdInfo->fgNeedResp) ||
			    (prCmdInfo->eCmdType ==
			    COMMAND_TYPE_SECURITY_FRAME)) {
				rStatus = WLAN_STATUS_PENDING;
			}
		}
	} while (FALSE);

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_QUE);
	QUEUE_CONCATENATE_QUEUES(&(prAdapter->rTxCmdQueue),
				 prTempCmdQue);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_QUE);

	return rStatus;
}				/* end of wlanSendCommandMthread() */

void wlanTxCmdDoneCb(IN struct ADAPTER *prAdapter,
		     IN struct CMD_INFO *prCmdInfo)
{

	KAL_SPIN_LOCK_DECLARATION();

	if ((!prCmdInfo->fgSetQuery) || (prCmdInfo->fgNeedResp)) {
#if 0
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING);
		QUEUE_INSERT_TAIL(&prAdapter->rPendingCmdQueue,
				  (struct QUE_ENTRY *) prCmdInfo);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING);
#endif
	} else {
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_DONE_QUE);
		QUEUE_INSERT_TAIL(&prAdapter->rTxCmdDoneQueue,
				  (struct QUE_ENTRY *) prCmdInfo);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_DONE_QUE);
	}

	/* call tx thread to work */
	set_bit(GLUE_FLAG_TX_CMD_DONE_BIT,
		&prAdapter->prGlueInfo->ulFlag);
	wake_up_interruptible(&prAdapter->prGlueInfo->waitq);
}

uint32_t wlanTxCmdMthread(IN struct ADAPTER *prAdapter)
{
	struct QUE rTempCmdQue;
	struct QUE *prTempCmdQue;
	struct QUE rTempCmdDoneQue;
	struct QUE *prTempCmdDoneQue;
	struct QUE_ENTRY *prQueueEntry;
	struct CMD_INFO *prCmdInfo;
	/* P_CMD_ACCESS_REG prCmdAccessReg;
	 * P_CMD_ACCESS_REG prEventAccessReg;
	 * UINT_32 u4Address;
	 */
	uint32_t u4TxDoneQueueSize;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);

	if (halIsHifStateSuspend(prAdapter)) {
		DBGLOG(TX, WARN, "Suspend TxCmdMthread\n");
		return WLAN_STATUS_SUCCESS;
	}

	prTempCmdQue = &rTempCmdQue;
	QUEUE_INITIALIZE(prTempCmdQue);

	prTempCmdDoneQue = &rTempCmdDoneQue;
	QUEUE_INITIALIZE(prTempCmdDoneQue);

	KAL_ACQUIRE_MUTEX(prAdapter, MUTEX_TX_CMD_CLEAR);

	/* TX Command Queue */
	/* 4 <1> Move whole list of CMD_INFO to temp queue */
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_QUE);
	QUEUE_MOVE_ALL(prTempCmdQue, &prAdapter->rTxCmdQueue);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_QUE);

	/* 4 <2> Dequeue from head and check it is able to be sent */
	QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry,
			  struct QUE_ENTRY *);
	while (prQueueEntry) {
		prCmdInfo = (struct CMD_INFO *) prQueueEntry;
		prCmdInfo->pfHifTxCmdDoneCb = wlanTxCmdDoneCb;

		if ((!prCmdInfo->fgSetQuery) || (prCmdInfo->fgNeedResp)) {
			KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING);
			QUEUE_INSERT_TAIL(&(prAdapter->rPendingCmdQueue),
					  (struct QUE_ENTRY *) prCmdInfo);
			KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING);
		} else {
			QUEUE_INSERT_TAIL(prTempCmdDoneQue, prQueueEntry);
		}

		nicTxCmd(prAdapter, prCmdInfo, TC4_INDEX);

		/* DBGLOG(INIT, INFO, "==> TX CMD QID: %d (Q:%d)\n",
		 *        prCmdInfo->ucCID, prTempCmdQue->u4NumElem));
		 */

		GLUE_DEC_REF_CNT(prAdapter->prGlueInfo->i4TxPendingCmdNum);
		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry,
				  struct QUE_ENTRY *);
	}

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_DONE_QUE);
	QUEUE_CONCATENATE_QUEUES(&prAdapter->rTxCmdDoneQueue,
				 prTempCmdDoneQue);
	u4TxDoneQueueSize = prAdapter->rTxCmdDoneQueue.u4NumElem;
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_DONE_QUE);

	KAL_RELEASE_MUTEX(prAdapter, MUTEX_TX_CMD_CLEAR);

	if (u4TxDoneQueueSize > 0) {
		/* call tx thread to work */
		set_bit(GLUE_FLAG_TX_CMD_DONE_BIT,
			&prAdapter->prGlueInfo->ulFlag);
		wake_up_interruptible(&prAdapter->prGlueInfo->waitq);
	}

	return WLAN_STATUS_SUCCESS;
}

uint32_t wlanTxCmdDoneMthread(IN struct ADAPTER *prAdapter)
{
	struct QUE rTempCmdQue;
	struct QUE *prTempCmdQue;
	struct QUE_ENTRY *prQueueEntry;
	struct CMD_INFO *prCmdInfo;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);

	if (halIsHifStateSuspend(prAdapter)) {
		DBGLOG(TX, WARN, "Suspend TxCmdDoneMthread\n");
		return WLAN_STATUS_SUCCESS;
	}

	prTempCmdQue = &rTempCmdQue;
	QUEUE_INITIALIZE(prTempCmdQue);

	/* 4 <1> Move whole list of CMD_INFO to temp queue */
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_DONE_QUE);
	QUEUE_MOVE_ALL(prTempCmdQue, &prAdapter->rTxCmdDoneQueue);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_DONE_QUE);

	/* 4 <2> Dequeue from head and check it is able to be sent */
	QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry,
			  struct QUE_ENTRY *);
	while (prQueueEntry) {
		prCmdInfo = (struct CMD_INFO *) prQueueEntry;

		if (prCmdInfo->pfCmdDoneHandler)
			prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo,
						    prCmdInfo->pucInfoBuffer);
		/* Not pending cmd, free it after TX succeed! */
		cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry,
				  struct QUE_ENTRY *);
	}

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is used to clear all commands in TX command queue
 * \param prAdapter  Pointer of Adapter Data Structure
 *
 * \retval none
 */
/*----------------------------------------------------------------------------*/
void wlanClearTxCommandQueue(IN struct ADAPTER *prAdapter)
{
	struct QUE rTempCmdQue;
	struct QUE *prTempCmdQue = &rTempCmdQue;
	struct QUE_ENTRY *prQueueEntry = (struct QUE_ENTRY *) NULL;
	struct CMD_INFO *prCmdInfo = (struct CMD_INFO *) NULL;

	KAL_SPIN_LOCK_DECLARATION();
	QUEUE_INITIALIZE(prTempCmdQue);

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_QUE);
	QUEUE_MOVE_ALL(prTempCmdQue, &prAdapter->rTxCmdQueue);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_QUE);

	QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry,
			  struct QUE_ENTRY *);
	while (prQueueEntry) {
		prCmdInfo = (struct CMD_INFO *) prQueueEntry;

		if (prCmdInfo->pfCmdTimeoutHandler)
			prCmdInfo->pfCmdTimeoutHandler(prAdapter, prCmdInfo);
		else
			wlanReleaseCommand(prAdapter, prCmdInfo,
					   TX_RESULT_QUEUE_CLEARANCE);

		/* Release Tx resource for CMD which resource is allocated but
		 * not used
		 */
		nicTxReleaseResource_PSE(prAdapter,
			nicTxGetCmdResourceType(prCmdInfo),
			nicTxGetCmdPageCount(prAdapter, prCmdInfo), TRUE);

		cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry,
				  struct QUE_ENTRY *);
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is used to clear OID commands in TX command queue
 * \param prAdapter  Pointer of Adapter Data Structure
 *
 * \retval none
 */
/*----------------------------------------------------------------------------*/
void wlanClearTxOidCommand(IN struct ADAPTER *prAdapter)
{
	struct QUE rTempCmdQue;
	struct QUE *prTempCmdQue = &rTempCmdQue;
	struct QUE_ENTRY *prQueueEntry = (struct QUE_ENTRY *) NULL;
	struct CMD_INFO *prCmdInfo = (struct CMD_INFO *) NULL;

	KAL_SPIN_LOCK_DECLARATION();
	QUEUE_INITIALIZE(prTempCmdQue);

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_QUE);

	QUEUE_MOVE_ALL(prTempCmdQue, &prAdapter->rTxCmdQueue);

	QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry,
			  struct QUE_ENTRY *);

	while (prQueueEntry) {
		prCmdInfo = (struct CMD_INFO *) prQueueEntry;

		if (prCmdInfo->fgIsOid) {

			if (prCmdInfo->pfCmdTimeoutHandler)
				prCmdInfo->pfCmdTimeoutHandler(prAdapter,
							       prCmdInfo);
			else
				wlanReleaseCommand(prAdapter, prCmdInfo,
						   TX_RESULT_QUEUE_CLEARANCE);

			/* Release Tx resource for CMD which resource is
			 * allocated but not used
			 */
			nicTxReleaseResource_PSE(prAdapter,
				nicTxGetCmdResourceType(prCmdInfo),
				nicTxGetCmdPageCount(prAdapter, prCmdInfo),
				TRUE);

			cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

		} else {
			QUEUE_INSERT_TAIL(&prAdapter->rTxCmdQueue,
					  prQueueEntry);
		}

		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry,
				  struct QUE_ENTRY *);
	}

	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_QUE);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is used to clear all commands in TX command done queue
 * \param prAdapter  Pointer of Adapter Data Structure
 *
 * \retval none
 */
/*----------------------------------------------------------------------------*/
void wlanClearTxCommandDoneQueue(IN struct ADAPTER
				 *prAdapter)
{
	struct QUE rTempCmdDoneQue;
	struct QUE *prTempCmdDoneQue = &rTempCmdDoneQue;
	struct QUE_ENTRY *prQueueEntry = (struct QUE_ENTRY *) NULL;
	struct CMD_INFO *prCmdInfo = (struct CMD_INFO *) NULL;

	KAL_SPIN_LOCK_DECLARATION();
	QUEUE_INITIALIZE(prTempCmdDoneQue);

	/* 4 <1> Move whole list of CMD_INFO to temp queue */
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_DONE_QUE);
	QUEUE_MOVE_ALL(prTempCmdDoneQue,
		       &prAdapter->rTxCmdDoneQueue);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_DONE_QUE);

	/* 4 <2> Dequeue from head and check it is able to be sent */
	QUEUE_REMOVE_HEAD(prTempCmdDoneQue, prQueueEntry,
			  struct QUE_ENTRY *);
	while (prQueueEntry) {
		prCmdInfo = (struct CMD_INFO *) prQueueEntry;

		if (prCmdInfo->pfCmdDoneHandler)
			prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo,
						    prCmdInfo->pucInfoBuffer);
		/* Not pending cmd, free it after TX succeed! */
		cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

		QUEUE_REMOVE_HEAD(prTempCmdDoneQue, prQueueEntry,
				  struct QUE_ENTRY *);
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is used to clear all buffer in port 0/1 queue
 * \param prAdapter  Pointer of Adapter Data Structure
 *
 * \retval none
 */
/*----------------------------------------------------------------------------*/
void wlanClearDataQueue(IN struct ADAPTER *prAdapter)
{
	if (HAL_IS_TX_DIRECT())
		nicTxDirectClearHifQ(prAdapter);
	else {
#if CFG_FIX_2_TX_PORT
		struct QUE qDataPort0, qDataPort1;
		struct QUE *prDataPort0, *prDataPort1;
		struct MSDU_INFO *prMsduInfo;

		KAL_SPIN_LOCK_DECLARATION();

		prDataPort0 = &qDataPort0;
		prDataPort1 = &qDataPort1;

		QUEUE_INITIALIZE(prDataPort0);
		QUEUE_INITIALIZE(prDataPort1);

		/* <1> Move whole list of CMD_INFO to temp queue */
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_PORT_QUE);
		QUEUE_MOVE_ALL(prDataPort0, &prAdapter->rTxP0Queue);
		QUEUE_MOVE_ALL(prDataPort1, &prAdapter->rTxP1Queue);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_PORT_QUE);

		/* <2> Release Tx resource */
		nicTxReleaseMsduResource(prAdapter,
			 (struct MSDU_INFO *) QUEUE_GET_HEAD(prDataPort0));
		nicTxReleaseMsduResource(prAdapter,
			 (struct MSDU_INFO *) QUEUE_GET_HEAD(prDataPort1));

		/* <3> Return sk buffer */
		nicTxReturnMsduInfo(prAdapter, (struct MSDU_INFO *)
						QUEUE_GET_HEAD(prDataPort0));
		nicTxReturnMsduInfo(prAdapter, (struct MSDU_INFO *)
						QUEUE_GET_HEAD(prDataPort1));

		/* <4> Clear pending MSDU info in data done queue */
		KAL_ACQUIRE_MUTEX(prAdapter, MUTEX_TX_DATA_DONE_QUE);
		while (QUEUE_IS_NOT_EMPTY(&prAdapter->rTxDataDoneQueue)) {
			QUEUE_REMOVE_HEAD(&prAdapter->rTxDataDoneQueue,
					  prMsduInfo, struct MSDU_INFO *);

			nicTxFreePacket(prAdapter, prMsduInfo, FALSE);
			nicTxReturnMsduInfo(prAdapter, prMsduInfo);
		}
		KAL_RELEASE_MUTEX(prAdapter, MUTEX_TX_DATA_DONE_QUE);
#else

		struct QUE qDataPort[TX_PORT_NUM];
		struct QUE *prDataPort[TX_PORT_NUM];
		struct MSDU_INFO *prMsduInfo;
		int32_t i;

		KAL_SPIN_LOCK_DECLARATION();

		for (i = 0; i < TX_PORT_NUM; i++) {
			prDataPort[i] = &qDataPort[i];
			QUEUE_INITIALIZE(prDataPort[i]);
		}

		/* <1> Move whole list of CMD_INFO to temp queue */
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_PORT_QUE);
		for (i = 0; i < TX_PORT_NUM; i++)
			QUEUE_MOVE_ALL(prDataPort[i], &prAdapter->rTxPQueue[i]);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_PORT_QUE);

		/* <2> Return sk buffer */
		for (i = 0; i < TX_PORT_NUM; i++) {
			nicTxReleaseMsduResource(prAdapter, (struct MSDU_INFO *)
						QUEUE_GET_HEAD(prDataPort[i]));
			nicTxReturnMsduInfo(prAdapter, (struct MSDU_INFO *)
						QUEUE_GET_HEAD(prDataPort[i]));
		}

		/* <3> Clear pending MSDU info in data done queue */
		KAL_ACQUIRE_MUTEX(prAdapter, MUTEX_TX_DATA_DONE_QUE);
		while (QUEUE_IS_NOT_EMPTY(&prAdapter->rTxDataDoneQueue)) {
			QUEUE_REMOVE_HEAD(&prAdapter->rTxDataDoneQueue,
					  prMsduInfo, struct MSDU_INFO *);

			nicTxFreePacket(prAdapter, prMsduInfo, FALSE);
			nicTxReturnMsduInfo(prAdapter, prMsduInfo);
		}
		KAL_RELEASE_MUTEX(prAdapter, MUTEX_TX_DATA_DONE_QUE);
#endif
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is used to clear all buffer in port 0/1 queue
 * \param prAdapter  Pointer of Adapter Data Structure
 *
 * \retval none
 */
/*----------------------------------------------------------------------------*/
void wlanClearRxToOsQueue(IN struct ADAPTER *prAdapter)
{
	struct QUE rTempRxQue;
	struct QUE *prTempRxQue = &rTempRxQue;
	struct QUE_ENTRY *prQueueEntry = (struct QUE_ENTRY *) NULL;

	KAL_SPIN_LOCK_DECLARATION();
	QUEUE_INITIALIZE(prTempRxQue);

	/* 4 <1> Move whole list of CMD_INFO to temp queue */
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_TO_OS_QUE);
	QUEUE_MOVE_ALL(prTempRxQue, &prAdapter->rRxQueue);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_TO_OS_QUE);

	/* 4 <2> Remove all skbuf */
	QUEUE_REMOVE_HEAD(prTempRxQue, prQueueEntry,
			  struct QUE_ENTRY *);
	while (prQueueEntry) {
		kalRxIndicateOnePkt(prAdapter->prGlueInfo,
				(void *) GLUE_GET_PKT_DESCRIPTOR(prQueueEntry));
		QUEUE_REMOVE_HEAD(prTempRxQue, prQueueEntry,
				struct QUE_ENTRY *);
	}

}
#endif

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function will release thd CMD_INFO upon its attribution
 *
 * \param prAdapter  Pointer of Adapter Data Structure
 * \param prCmdInfo  Pointer of CMD_INFO_T
 * \param rTxDoneStatus  Tx done status
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
void wlanReleaseCommand(IN struct ADAPTER *prAdapter,
			IN struct CMD_INFO *prCmdInfo,
			IN enum ENUM_TX_RESULT_CODE rTxDoneStatus)
{
	struct TX_CTRL *prTxCtrl;
	struct MSDU_INFO *prMsduInfo;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	prTxCtrl = &prAdapter->rTxCtrl;

	switch (prCmdInfo->eCmdType) {
	case COMMAND_TYPE_GENERAL_IOCTL:
	case COMMAND_TYPE_NETWORK_IOCTL:
		DBGLOG(INIT, INFO,
		       "Free CMD: ID[0x%x] SeqNum[%u] OID[%u]\n",
		       prCmdInfo->ucCID, prCmdInfo->ucCmdSeqNum,
		       prCmdInfo->fgIsOid);

		if (prCmdInfo->fgIsOid) {
			kalOidComplete(prAdapter->prGlueInfo,
				       prCmdInfo->fgSetQuery,
				       prCmdInfo->u4SetInfoLen,
				       WLAN_STATUS_FAILURE);
		}
		break;

	case COMMAND_TYPE_SECURITY_FRAME:
	case COMMAND_TYPE_MANAGEMENT_FRAME:
		prMsduInfo = prCmdInfo->prMsduInfo;

		if (prCmdInfo->eCmdType == COMMAND_TYPE_SECURITY_FRAME) {
			kalSecurityFrameSendComplete(prAdapter->prGlueInfo,
						     prCmdInfo->prPacket,
						     WLAN_STATUS_FAILURE);
			/* Avoid skb multiple free */
			prMsduInfo->prPacket = NULL;
		}

		DBGLOG(INIT, INFO,
		       "Free %s Frame: BSS[%u] WIDX:PID[%u:%u] SEQ[%u] STA[%u] RSP[%u] CMDSeq[%u]\n",
		       prCmdInfo->eCmdType == COMMAND_TYPE_SECURITY_FRAME ?
								"SEC" : "MGMT",
		       prMsduInfo->ucBssIndex,
		       prMsduInfo->ucWlanIndex,
		       prMsduInfo->ucPID,
		       prMsduInfo->ucTxSeqNum,
		       prMsduInfo->ucStaRecIndex,
		       prMsduInfo->pfTxDoneHandler ? TRUE : FALSE,
		       prCmdInfo->ucCmdSeqNum);

		/* invoke callbacks */
		if (prMsduInfo->pfTxDoneHandler != NULL)
			prMsduInfo->pfTxDoneHandler(prAdapter, prMsduInfo,
						    rTxDoneStatus);

		if (prCmdInfo->eCmdType == COMMAND_TYPE_MANAGEMENT_FRAME)
			GLUE_DEC_REF_CNT(prTxCtrl->i4TxMgmtPendingNum);

		cnmMgtPktFree(prAdapter, prMsduInfo);
		break;

	default:
		ASSERT(0);
		break;
	}

}				/* end of wlanReleaseCommand() */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function will search the CMD Queue to look for the pending OID
 *	  and compelete it immediately when system request a reset.
 *
 * \param prAdapter  ointer of Adapter Data Structure
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
void wlanReleasePendingOid(IN struct ADAPTER *prAdapter,
			   IN unsigned long ulParamPtr)
{
	struct QUE *prCmdQue;
	struct QUE rTempCmdQue;
	struct QUE *prTempCmdQue = &rTempCmdQue;
	struct QUE_ENTRY *prQueueEntry = (struct QUE_ENTRY *) NULL;
	struct CMD_INFO *prCmdInfo = (struct CMD_INFO *) NULL;

	KAL_SPIN_LOCK_DECLARATION();

	DEBUGFUNC("wlanReleasePendingOid");

	ASSERT(prAdapter);

	do {
		if (ulParamPtr == 1)
			break;

		prAdapter->ucOidTimeoutCount++;
		if (prAdapter->ucOidTimeoutCount >=
		    WLAN_OID_NO_ACK_THRESHOLD) {
			if (!prAdapter->fgIsChipNoAck) {
				DBGLOG(INIT, WARN,
				       "No response from chip for %u times, set NoAck flag!\n",
				       prAdapter->ucOidTimeoutCount);
#if 0
				glGetRstReason(RST_OID_TIMEOUT);
				GL_RESET_TRIGGER(prAdapter,
						 RST_FLAG_CHIP_RESET);
#endif
			}

			if (prAdapter->ucOidTimeoutCount >=
			    WLAN_OID_NO_ACK_THRESHOLD) {
				if (!prAdapter->fgIsChipNoAck) {
					DBGLOG(INIT, WARN,
					       "No response from chip for %u times, set NoAck flag!\n",
					       prAdapter->ucOidTimeoutCount);
				}

				prAdapter->fgIsChipNoAck = TRUE;
			}

			prAdapter->u4HifDbgFlag |= DEG_HIF_ALL;
			kalSetHifDbgEvent(prAdapter->prGlueInfo);
		}
	} while (FALSE);

	do {
#if CFG_SUPPORT_MULTITHREAD
		KAL_ACQUIRE_MUTEX(prAdapter, MUTEX_TX_CMD_CLEAR);
#endif
		/* 1: Clear pending OID in glue layer command queue */
		kalOidCmdClearance(prAdapter->prGlueInfo);

#if CFG_SUPPORT_MULTITHREAD
		/* Clear pending OID in main_thread to hif_thread command queue
		 */
		wlanClearTxOidCommand(prAdapter);
#endif

		/* 2: Clear Pending OID in prAdapter->rPendingCmdQueue */
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING);

		prCmdQue = &prAdapter->rPendingCmdQueue;
		QUEUE_MOVE_ALL(prTempCmdQue, prCmdQue);

		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry,
				  struct QUE_ENTRY *);
		while (prQueueEntry) {
			prCmdInfo = (struct CMD_INFO *) prQueueEntry;

			if (prCmdInfo->fgIsOid) {
				DBGLOG(OID, INFO,
				       "Clear pending OID CMD ID[0x%02X] SEQ[%u] buf[0x%p]\n",
				       prCmdInfo->ucCID, prCmdInfo->ucCmdSeqNum,
				       prCmdInfo->pucInfoBuffer);

				if (prCmdInfo->pfCmdTimeoutHandler) {
					prCmdInfo->pfCmdTimeoutHandler(
							prAdapter, prCmdInfo);
				} else {
					kalOidComplete(prAdapter->prGlueInfo,
						       prCmdInfo->fgSetQuery,
						       0, WLAN_STATUS_FAILURE);
				}

				KAL_RELEASE_SPIN_LOCK(prAdapter,
						      SPIN_LOCK_CMD_PENDING);
				nicTxCancelSendingCmd(prAdapter, prCmdInfo);
				KAL_ACQUIRE_SPIN_LOCK(prAdapter,
						      SPIN_LOCK_CMD_PENDING);

				cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
			} else {
				QUEUE_INSERT_TAIL(prCmdQue, prQueueEntry);
			}

			QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry,
					  struct QUE_ENTRY *);
		}

		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING);

		/* 3: Clear pending OID queued in pvOidEntry with REQ_FLAG_OID
		 *    set
		 */
		kalOidClearance(prAdapter->prGlueInfo);

#if CFG_SUPPORT_MULTITHREAD
		KAL_RELEASE_MUTEX(prAdapter, MUTEX_TX_CMD_CLEAR);
#endif
	} while (FALSE);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function will search the CMD Queue to look for the pending
 *	  CMD/OID for specific
 *        NETWORK TYPE and compelete it immediately when system request a reset.
 *
 * \param prAdapter  ointer of Adapter Data Structure
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
void wlanReleasePendingCMDbyBssIdx(IN struct ADAPTER
				   *prAdapter, IN uint8_t ucBssIndex)
{
#if 0
	struct QUE *prCmdQue;
	struct QUE rTempCmdQue;
	struct QUE *prTempCmdQue = &rTempCmdQue;
	struct QUE_ENTRY *prQueueEntry = (struct QUE_ENTRY *) NULL;
	struct CMD_INFO *prCmdInfo = (struct CMD_INFO *) NULL;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);

	do {
		/* 1: Clear Pending OID in prAdapter->rPendingCmdQueue */
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING);

		prCmdQue = &prAdapter->rPendingCmdQueue;
		QUEUE_MOVE_ALL(prTempCmdQue, prCmdQue);

		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry,
				  struct QUE_ENTRY *);
		while (prQueueEntry) {
			prCmdInfo = (struct CMD_INFO *) prQueueEntry;

			DBGLOG(P2P, TRACE, "Pending CMD for BSS:%d\n",
			       prCmdInfo->ucBssIndex);

			if (prCmdInfo->ucBssIndex == ucBssIndex) {
				if (prCmdInfo->pfCmdTimeoutHandler) {
					prCmdInfo->pfCmdTimeoutHandler(
							prAdapter, prCmdInfo);
				} else if (prCmdInfo->fgIsOid) {
					kalOidComplete(prAdapter->prGlueInfo,
						       prCmdInfo->fgSetQuery,
						       0, WLAN_STATUS_FAILURE);
				}

				cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
			} else {
				QUEUE_INSERT_TAIL(prCmdQue, prQueueEntry);
			}

			QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry,
					  struct QUE_ENTRY *);
		}

		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING);

	} while (FALSE);
#endif


}				/* wlanReleasePendingCMDbyBssIdx */

/*----------------------------------------------------------------------------*/
/*!
 * \brief Return the indicated packet buffer and reallocate one to the RFB
 *
 * \param prAdapter      Pointer of Adapter Data Structure
 * \param pvPacket       Pointer of returned packet
 *
 * \retval WLAN_STATUS_SUCCESS: Success
 * \retval WLAN_STATUS_FAILURE: Failed
 */
/*----------------------------------------------------------------------------*/
void wlanReturnPacketDelaySetupTimeout(IN struct ADAPTER
				       *prAdapter, IN unsigned long ulParamPtr)
{
	struct RX_CTRL *prRxCtrl;
	struct SW_RFB *prSwRfb = NULL;

	KAL_SPIN_LOCK_DECLARATION();
	uint32_t status = WLAN_STATUS_SUCCESS;
	struct QUE *prQueList;

	ASSERT(prAdapter);

	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);

	prQueList = &prRxCtrl->rIndicatedRfbList;
	DBGLOG(RX, WARN, "%s: IndicatedRfbList num = %u\n",
	       __func__, prQueList->u4NumElem);

	while (QUEUE_IS_NOT_EMPTY(&prRxCtrl->rIndicatedRfbList)) {
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);
		QUEUE_REMOVE_HEAD(&prRxCtrl->rIndicatedRfbList, prSwRfb,
				  struct SW_RFB *);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);

		status = nicRxSetupRFB(prAdapter, prSwRfb);
		nicRxReturnRFB(prAdapter, prSwRfb);

		if (status != WLAN_STATUS_SUCCESS)
			break;
	}

	if (status != WLAN_STATUS_SUCCESS) {
		DBGLOG(RX, WARN, "Restart ReturnIndicatedRfb Timer (%u)\n",
		       RX_RETURN_INDICATED_RFB_TIMEOUT_SEC);
		/* restart timer */
		cnmTimerStartTimer(prAdapter,
			&prAdapter->rPacketDelaySetupTimer,
			SEC_TO_MSEC(RX_RETURN_INDICATED_RFB_TIMEOUT_SEC));
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Return the packet buffer and reallocate one to the RFB
 *
 * \param prAdapter      Pointer of Adapter Data Structure
 * \param pvPacket       Pointer of returned packet
 *
 * \retval WLAN_STATUS_SUCCESS: Success
 * \retval WLAN_STATUS_FAILURE: Failed
 */
/*----------------------------------------------------------------------------*/
void wlanReturnPacket(IN struct ADAPTER *prAdapter,
		      IN void *pvPacket)
{
	struct RX_CTRL *prRxCtrl;
	struct SW_RFB *prSwRfb = NULL;

	KAL_SPIN_LOCK_DECLARATION();

	DEBUGFUNC("wlanReturnPacket");

	ASSERT(prAdapter);

	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);

	if (pvPacket) {
		kalPacketFree(prAdapter->prGlueInfo, pvPacket);
		RX_ADD_CNT(prRxCtrl, RX_DATA_RETURNED_COUNT, 1);
#if CFG_NATIVE_802_11
		if (GLUE_TEST_FLAG(prAdapter->prGlueInfo, GLUE_FLAG_HALT)) {
			/*Todo:: nothing */
			/*Todo:: nothing */
		}
#endif
	}

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);
	QUEUE_REMOVE_HEAD(&prRxCtrl->rIndicatedRfbList, prSwRfb,
			  struct SW_RFB *);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);
	if (!prSwRfb) {
		DBGLOG(RX, WARN, "No free SwRfb!\n");
		return;
	}

	if (nicRxSetupRFB(prAdapter, prSwRfb)) {
		DBGLOG(RX, WARN,
		       "Cannot allocate packet buffer for SwRfb!\n");
		if (!timerPendingTimer(
		    &prAdapter->rPacketDelaySetupTimer)) {
			DBGLOG(RX, WARN,
			       "Start ReturnIndicatedRfb Timer (%u)\n",
			       RX_RETURN_INDICATED_RFB_TIMEOUT_SEC);
			cnmTimerStartTimer(prAdapter,
			    &prAdapter->rPacketDelaySetupTimer,
			    SEC_TO_MSEC(RX_RETURN_INDICATED_RFB_TIMEOUT_SEC));
		}
	}
	nicRxReturnRFB(prAdapter, prSwRfb);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is a required function that returns information about
 *        the capabilities and status of the driver and/or its network adapter.
 *
 * \param[IN] prAdapter		Pointer to the Adapter structure.
 * \param[IN] pfnOidQryHandler	Function pointer for the OID query handler.
 * \param[IN] pvInfoBuf		Points to a buffer for return the query
 *				information.
 * \param[IN] u4QueryBufferLen Specifies the number of bytes at pvInfoBuf.
 * \param[OUT] pu4QueryInfoLen	Points to the number of bytes it written or is
 *				needed.
 *
 * \retval WLAN_STATUS_xxx Different WLAN_STATUS code returned by different
 *				handlers.
 *
 */
/*----------------------------------------------------------------------------*/
uint32_t
wlanQueryInformation(IN struct ADAPTER *prAdapter,
		     IN PFN_OID_HANDLER_FUNC pfnOidQryHandler,
		     IN void *pvInfoBuf, IN uint32_t u4InfoBufLen,
		     OUT uint32_t *pu4QryInfoLen)
{
	uint32_t status = WLAN_STATUS_FAILURE;

	ASSERT(prAdapter);
	ASSERT(pu4QryInfoLen);

	/* ignore any OID request after connected, under PS current measurement
	 * mode
	 */
	if (prAdapter->u4PsCurrentMeasureEn &&
	    (prAdapter->prGlueInfo->eParamMediaStateIndicated ==
	     PARAM_MEDIA_STATE_CONNECTED)) {
		/* note: return WLAN_STATUS_FAILURE or
		 * WLAN_STATUS_SUCCESS for blocking OIDs during current
		 * measurement ??
		 */
		return WLAN_STATUS_SUCCESS;
	}
#if 1
	/* most OID handler will just queue a command packet */
	status = pfnOidQryHandler(prAdapter, pvInfoBuf,
				  u4InfoBufLen, pu4QryInfoLen);
#else
	if (wlanIsHandlerNeedHwAccess(pfnOidQryHandler, FALSE)) {
		ACQUIRE_POWER_CONTROL_FROM_PM(prAdapter);

		/* Reset sleepy state */
		if (prAdapter->fgWiFiInSleepyState == TRUE)
			prAdapter->fgWiFiInSleepyState = FALSE;

		status = pfnOidQryHandler(prAdapter, pvInfoBuf,
					  u4InfoBufLen, pu4QryInfoLen);

		RECLAIM_POWER_CONTROL_TO_PM(prAdapter, FALSE);
	} else
		status = pfnOidQryHandler(prAdapter, pvInfoBuf,
					  u4InfoBufLen, pu4QryInfoLen);
#endif

	return status;

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is a required function that allows bound protocol
 *	  drivers, or NDIS, to request changes in the state information that
 *	  the miniport maintains for particular object identifiers, such as
 *	  changes in multicast addresses.
 *
 * \param[IN] prAdapter     Pointer to the Glue info structure.
 * \param[IN] pfnOidSetHandler     Points to the OID set handlers.
 * \param[IN] pvInfoBuf     Points to a buffer containing the OID-specific data
 *			    for the set.
 * \param[IN] u4InfoBufLen  Specifies the number of bytes at prSetBuffer.
 * \param[OUT] pu4SetInfoLen Points to the number of bytes it read or is needed.
 *
 * \retval WLAN_STATUS_xxx Different WLAN_STATUS code returned by different
 *			   handlers.
 *
 */
/*----------------------------------------------------------------------------*/
uint32_t
wlanSetInformation(IN struct ADAPTER *prAdapter,
		   IN PFN_OID_HANDLER_FUNC pfnOidSetHandler,
		   IN void *pvInfoBuf, IN uint32_t u4InfoBufLen,
		   OUT uint32_t *pu4SetInfoLen)
{
	uint32_t status = WLAN_STATUS_FAILURE;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	/* ignore any OID request after connected, under PS current measurement
	 * mode
	 */
	if (prAdapter->u4PsCurrentMeasureEn &&
	    (prAdapter->prGlueInfo->eParamMediaStateIndicated ==
	     PARAM_MEDIA_STATE_CONNECTED)) {
		/* note: return WLAN_STATUS_FAILURE or WLAN_STATUS_SUCCESS
		 * for blocking OIDs during current measurement ??
		 */
		return WLAN_STATUS_SUCCESS;
	}
#if 1
	/* most OID handler will just queue a command packet
	 * for power state transition OIDs, handler will acquire power control
	 * by itself
	 */
	status = pfnOidSetHandler(prAdapter, pvInfoBuf,
				  u4InfoBufLen, pu4SetInfoLen);
#else
	if (wlanIsHandlerNeedHwAccess(pfnOidSetHandler, TRUE)) {
		ACQUIRE_POWER_CONTROL_FROM_PM(prAdapter);

		/* Reset sleepy state */
		if (prAdapter->fgWiFiInSleepyState == TRUE)
			prAdapter->fgWiFiInSleepyState = FALSE;

		status = pfnOidSetHandler(prAdapter, pvInfoBuf,
					  u4InfoBufLen, pu4SetInfoLen);

		RECLAIM_POWER_CONTROL_TO_PM(prAdapter, FALSE);
	} else {
		status = pfnOidSetHandler(prAdapter, pvInfoBuf,
					  u4InfoBufLen, pu4SetInfoLen);
	}
#endif

	return status;
}

#if CFG_SUPPORT_WAPI
/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is a used to query driver's config wapi mode or not
 *
 * \param[IN] prAdapter     Pointer to the Glue info structure.
 *
 * \retval TRUE for use wapi mode
 *
 */
/*----------------------------------------------------------------------------*/
u_int8_t wlanQueryWapiMode(IN struct ADAPTER *prAdapter)
{
	ASSERT(prAdapter);

	return prAdapter->rWifiVar.rConnSettings.fgWapiMode;
}
#endif

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is called to set RX filter to Promiscuous Mode.
 *
 * \param[IN] prAdapter        Pointer to the Adapter structure.
 * \param[IN] fgEnablePromiscuousMode Enable/ disable RX Promiscuous Mode.
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
void wlanSetPromiscuousMode(IN struct ADAPTER *prAdapter,
			    IN u_int8_t fgEnablePromiscuousMode)
{
	ASSERT(prAdapter);

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is called to set RX filter to allow to receive
 *        broadcast address packets.
 *
 * \param[IN] prAdapter        Pointer to the Adapter structure.
 * \param[IN] fgEnableBroadcast Enable/ disable broadcast packet to be received.
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
void wlanRxSetBroadcast(IN struct ADAPTER *prAdapter,
			IN u_int8_t fgEnableBroadcast)
{
	ASSERT(prAdapter);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is called to send out CMD_ID_DUMMY command packet
 *
 * \param[IN] prAdapter        Pointer to the Adapter structure.
 *
 * \return WLAN_STATUS_SUCCESS
 * \return WLAN_STATUS_FAILURE
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanSendDummyCmd(IN struct ADAPTER *prAdapter,
			  IN u_int8_t fgIsReqTxRsrc)
{
	uint32_t status = WLAN_STATUS_SUCCESS;
	struct GLUE_INFO *prGlueInfo;
	struct CMD_INFO *prCmdInfo;
	struct WIFI_CMD *prWifiCmd;

	ASSERT(prAdapter);

	prGlueInfo = prAdapter->prGlueInfo;

	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, CMD_HDR_SIZE);
	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}

	prCmdInfo->eCmdType = COMMAND_TYPE_GENERAL_IOCTL;
	prCmdInfo->u2InfoBufLen = (uint16_t) CMD_HDR_SIZE;
	prCmdInfo->pfCmdDoneHandler = NULL;
	prCmdInfo->pfCmdTimeoutHandler = NULL;
	prCmdInfo->fgIsOid = TRUE;
	prCmdInfo->ucCID = CMD_ID_DUMMY_RSV;
	prCmdInfo->fgSetQuery = TRUE;
	prCmdInfo->fgNeedResp = FALSE;
	prCmdInfo->ucCmdSeqNum = 0;
	prCmdInfo->u4SetInfoLen = 0;

	prWifiCmd = (struct WIFI_CMD *) (prCmdInfo->pucInfoBuffer);
	prWifiCmd->u2TxByteCount = prCmdInfo->u2InfoBufLen;
	prWifiCmd->u2PQ_ID = CMD_PQ_ID;
	prWifiCmd->ucPktTypeID = CMD_PACKET_TYPE_ID;
	prWifiCmd->ucCID = prCmdInfo->ucCID;
	prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
	prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;

	if (fgIsReqTxRsrc) {
		if (wlanSendCommand(prAdapter,
				    prCmdInfo) != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR,
			       "Fail to transmit CMD_ID_DUMMY command\n");
			status = WLAN_STATUS_FAILURE;
		}
	} else {
		if (nicTxCmd(prAdapter, prCmdInfo,
			     TC4_INDEX) != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR,
			       "Fail to transmit CMD_ID_DUMMY command\n");
			status = WLAN_STATUS_FAILURE;
		}
	}
	cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

	return status;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is called to send out CMD_NIC_POWER_CTRL command packet
 *
 * \param[IN] prAdapter        Pointer to the Adapter structure.
 * \param[IN] ucPowerMode      refer to CMD/EVENT document
 *
 * \return WLAN_STATUS_SUCCESS
 * \return WLAN_STATUS_FAILURE
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanSendNicPowerCtrlCmd(IN struct ADAPTER
				 *prAdapter, IN uint8_t ucPowerMode)
{
	uint32_t status = WLAN_STATUS_SUCCESS;
	struct GLUE_INFO *prGlueInfo;
	struct CMD_INFO *prCmdInfo;
	struct WIFI_CMD *prWifiCmd;
	uint8_t ucTC, ucCmdSeqNum;

	ASSERT(prAdapter);

	prGlueInfo = prAdapter->prGlueInfo;

	/* 1. Prepare CMD */
	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, (CMD_HDR_SIZE +
					sizeof(struct CMD_NIC_POWER_CTRL)));
	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
#if CFG_ENABLE_KEYWORD_EXCEPTION_MECHANISM
		mtk_wcn_wmt_assert_keyword(WMTDRV_TYPE_WIFI,
			"[Wi-Fi Off] Allocate CMD_INFO_T ==> FAILED.");
#endif
		return WLAN_STATUS_FAILURE;
	}

	/* 2.1 increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);
	DBGLOG(REQ, TRACE, "ucCmdSeqNum =%d\n", ucCmdSeqNum);

	/* 2.2 Setup common CMD Info Packet */
	prCmdInfo->eCmdType = COMMAND_TYPE_GENERAL_IOCTL;
	prCmdInfo->u2InfoBufLen = (uint16_t) (CMD_HDR_SIZE + sizeof(
			struct CMD_NIC_POWER_CTRL));
	prCmdInfo->pfCmdDoneHandler = NULL;
	prCmdInfo->pfCmdTimeoutHandler = NULL;
	prCmdInfo->fgIsOid = g_fgIsOid;
	prCmdInfo->ucCID = CMD_ID_NIC_POWER_CTRL;
	prCmdInfo->fgSetQuery = TRUE;
	prCmdInfo->fgNeedResp = FALSE;
	prCmdInfo->ucCmdSeqNum = ucCmdSeqNum;
	prCmdInfo->u4SetInfoLen = sizeof(struct CMD_NIC_POWER_CTRL);

	/* 2.3 Setup WIFI_CMD_T */
	prWifiCmd = (struct WIFI_CMD *) (prCmdInfo->pucInfoBuffer);
	prWifiCmd->u2TxByteCount = prCmdInfo->u2InfoBufLen;
	prWifiCmd->u2PQ_ID = CMD_PQ_ID;
	prWifiCmd->ucPktTypeID = CMD_PACKET_TYPE_ID;
	prWifiCmd->ucCID = prCmdInfo->ucCID;
	prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
	prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;

	kalMemZero(prWifiCmd->aucBuffer,
		   sizeof(struct CMD_NIC_POWER_CTRL));
	((struct CMD_NIC_POWER_CTRL *) (
		 prWifiCmd->aucBuffer))->ucPowerMode = ucPowerMode;

	/* 3. Issue CMD for entering specific power mode */
	ucTC = TC4_INDEX;

	while (1) {
		/* 3.0 Removal check */
		if (kalIsCardRemoved(prAdapter->prGlueInfo) == TRUE
		    || fgIsBusAccessFailed == TRUE) {
			status = WLAN_STATUS_FAILURE;
			break;
		}
		/* 3.1 Acquire TX Resource */
		if (nicTxAcquireResource(prAdapter, ucTC,
		    nicTxGetCmdPageCount(prAdapter, prCmdInfo), TRUE)
		    == WLAN_STATUS_RESOURCES) {
			if (nicTxPollingResource(prAdapter, ucTC) !=
			    WLAN_STATUS_SUCCESS) {
				DBGLOG(INIT, ERROR,
				       "Fail to get TX resource return within timeout\n");
				status = WLAN_STATUS_FAILURE;
				prAdapter->fgIsChipNoAck = TRUE;
				break;
			}
			continue;
		}
		break;
	};

	/* 3.2 Send CMD Info Packet */
	if (nicTxCmd(prAdapter, prCmdInfo,
		     ucTC) != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR,
		       "Fail to transmit CMD_NIC_POWER_CTRL command\n");
#if CFG_ENABLE_KEYWORD_EXCEPTION_MECHANISM
			mtk_wcn_wmt_assert_keyword(WMTDRV_TYPE_WIFI,
				"[Wi-Fi Off] Fail to transmit CMD_NIC_POWER_CTRL command");
#endif
		status = WLAN_STATUS_FAILURE;
	}

	/* 4. Free CMD Info Packet. */
	cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

	/* 5. Add flag */
	if (ucPowerMode == 1)
		prAdapter->fgIsEnterD3ReqIssued = TRUE;

	return status;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is called to check if it is RF test mode and
 *        the OID is allowed to be called or not
 *
 * \param[IN] prAdapter        Pointer to the Adapter structure.
 * \param[IN] fgEnableBroadcast Enable/ disable broadcast packet to be received.
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
u_int8_t wlanIsHandlerAllowedInRFTest(IN PFN_OID_HANDLER_FUNC pfnOidHandler,
				      IN u_int8_t fgSetInfo)
{
	PFN_OID_HANDLER_FUNC *apfnOidHandlerAllowedInRFTest;
	uint32_t i;
	uint32_t u4NumOfElem;

	if (fgSetInfo) {
		apfnOidHandlerAllowedInRFTest =
			apfnOidSetHandlerAllowedInRFTest;
		u4NumOfElem = sizeof(apfnOidSetHandlerAllowedInRFTest) /
			      sizeof(PFN_OID_HANDLER_FUNC);
	} else {
		apfnOidHandlerAllowedInRFTest =
			apfnOidQueryHandlerAllowedInRFTest;
		u4NumOfElem = sizeof(apfnOidQueryHandlerAllowedInRFTest) /
			      sizeof(PFN_OID_HANDLER_FUNC);
	}

	for (i = 0; i < u4NumOfElem; i++) {
		if (apfnOidHandlerAllowedInRFTest[i] == pfnOidHandler)
			return TRUE;
	}

	return FALSE;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called to get the chip information
 *
 * @param prAdapter      Pointer to the Adapter structure.
 *
 * @return
 */
/*----------------------------------------------------------------------------*/

uint32_t wlanSetChipEcoInfo(IN struct ADAPTER *prAdapter)
{
	uint32_t hw_version, sw_version = 0;
	struct mt66xx_chip_info *prChipInfo = prAdapter->chip_info;
	uint32_t chip_id = prChipInfo->chip_id;
	/* WLAN_STATUS status; */
	uint32_t u4Status = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanSetChipEcoInfo.\n");

	if (wlanAccessRegister(prAdapter, TOP_HVR, &hw_version, 0, 0) !=
	    WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR,
		       "wlanSetChipEcoInfo >> get TOP_HVR failed.\n");
		u4Status = WLAN_STATUS_FAILURE;
	} else if (wlanAccessRegister(prAdapter, TOP_FVR, &sw_version, 0, 0) !=
	    WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR,
		       "wlanSetChipEcoInfo >> get TOP_FVR failed.\n");
		u4Status = WLAN_STATUS_FAILURE;
	} else {
		/* success */
		nicSetChipHwVer((uint8_t)(GET_HW_VER(hw_version) & 0xFF));
		nicSetChipFactoryVer((uint8_t)((GET_HW_VER(hw_version) >> 8) &
				     0xF));
		nicSetChipSwVer((uint8_t)GET_FW_VER(sw_version));

		/* Assign current chip version */
		prAdapter->chip_info->eco_ver = nicGetChipEcoVer(prAdapter);
	}

	DBGLOG(INIT, INFO,
	       "Chip ID[%04X] Version[E%u] HW[0x%08x] SW[0x%08x]\n",
	       chip_id, prAdapter->chip_info->eco_ver, hw_version,
	       sw_version);

	return u4Status;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called to read/write a certain N9
 *        register by inband command in blocking mode in ROM code stage
 *
 * @param prAdapter      Pointer to the Adapter structure.
 *        u4DestAddr     Address of destination address
 *        u4ImgSecSize   Length of the firmware block
 *        fgReset        should be set to TRUE if this is the 1st configuration
 *
 * @return WLAN_STATUS_SUCCESS
 *         WLAN_STATUS_FAILURE
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanAccessRegister(IN struct ADAPTER *prAdapter,
			    IN uint32_t u4Addr, IN uint32_t *pru4Result,
			    IN uint32_t u4Data,
			    IN uint8_t ucSetQuery)
{
	struct mt66xx_chip_info *prChipInfo;
	struct CMD_INFO *prCmdInfo;
	struct INIT_HIF_TX_HEADER *prInitHifTxHeader;
	struct INIT_WIFI_EVENT *prInitEvent;
	struct INIT_CMD_ACCESS_REG *prInitCmdAccessReg;
	struct INIT_EVENT_ACCESS_REG *prInitEventAccessReg;
	uint8_t ucTC, ucCmdSeqNum;
	uint16_t cmd_size;
	uint8_t *aucBuffer;
	uint32_t u4EventSize;
	uint32_t u4Status = WLAN_STATUS_SUCCESS;

	ASSERT(prAdapter);
	prChipInfo = prAdapter->chip_info;

	DEBUGFUNC("wlanAccessRegister");

	/* 1. Allocate CMD Info Packet and its Buffer. */
	cmd_size = sizeof(struct INIT_HIF_TX_HEADER) + sizeof(
			   struct INIT_CMD_ACCESS_REG);
	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, cmd_size);

	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}

	u4EventSize = prChipInfo->rxd_size + prChipInfo->init_event_size +
		sizeof(struct INIT_EVENT_ACCESS_REG);
	aucBuffer = kalMemAlloc(u4EventSize, PHY_MEM_TYPE);

	prCmdInfo->u2InfoBufLen = cmd_size;

#if (CFG_USE_TC4_RESOURCE_FOR_INIT_CMD == 1)
	/* 2. Use TC4's resource to download image. (TC4 as CPU) */
	ucTC = TC4_INDEX;
#else
	/* 2. Use TC0's resource to download image.
	 * Only TC0 is allowed because SDIO HW always reports
	 * MCU's TXQ_CNT at TXQ0_CNT in CR4 architecutre)
	 */
	ucTC = TC0_INDEX;
#endif

	/* 3. increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

	/* 4. Setup common CMD Info Packet */
	prInitHifTxHeader = (struct INIT_HIF_TX_HEADER *) (
				    prCmdInfo->pucInfoBuffer);
	prInitHifTxHeader->u2TxByteCount = prCmdInfo->u2InfoBufLen;
	prInitHifTxHeader->u2PQ_ID = INIT_CMD_PQ_ID;
	prInitHifTxHeader->ucHeaderFormat = INIT_CMD_PACKET_TYPE_ID;
	prInitHifTxHeader->ucPktFt = INIT_PKT_FT_CMD;

	prInitHifTxHeader->rInitWifiCmd.ucCID =
		INIT_CMD_ID_ACCESS_REG;

	prInitHifTxHeader->rInitWifiCmd.ucPktTypeID =
		INIT_CMD_PACKET_TYPE_ID;
	prInitHifTxHeader->rInitWifiCmd.ucSeqNum = ucCmdSeqNum;

	/* 5. Setup CMD_ACCESS_REG */
	prInitCmdAccessReg = (struct INIT_CMD_ACCESS_REG *) (
				     prInitHifTxHeader->rInitWifiCmd.aucBuffer);
	prInitCmdAccessReg->ucSetQuery = ucSetQuery;
	prInitCmdAccessReg->u4Address = u4Addr;
	prInitCmdAccessReg->u4Data = u4Data;

	/* 6. Send CMD_ACCESS_REG command */
	while (1) {
		/* 6.1 Acquire TX Resource */
		if (nicTxAcquireResource
		    (prAdapter, ucTC, nicTxGetPageCount(prAdapter,
		    prCmdInfo->u2InfoBufLen, TRUE), TRUE)
		    == WLAN_STATUS_RESOURCES) {
			if (nicTxPollingResource(prAdapter,
						 ucTC) != WLAN_STATUS_SUCCESS) {
				u4Status = WLAN_STATUS_FAILURE;
				DBGLOG(INIT, ERROR,
				       "Fail to get TX resource return within timeout\n");
				goto exit;
			}
			continue;
		}

		/* 6.2 Send CMD Info Packet */
		if (nicTxInitCmd(prAdapter, prCmdInfo,
		    prChipInfo->u2TxInitCmdPort) != WLAN_STATUS_SUCCESS) {
			u4Status = WLAN_STATUS_FAILURE;
			DBGLOG(INIT, ERROR,
			       "Fail to transmit image download command\n");
			goto exit;
		}

		break;
	};

	/* 7. Wait for INIT_EVENT_ID_CMD_RESULT */
	u4Status = wlanAccessRegisterStatus(prAdapter, ucCmdSeqNum, ucSetQuery,
					    aucBuffer, u4EventSize);
	if (ucSetQuery == 0) {
		prInitEvent = (struct INIT_WIFI_EVENT *)
			(aucBuffer + prChipInfo->rxd_size);
		prInitEventAccessReg = (struct INIT_EVENT_ACCESS_REG *)
			prInitEvent->aucBuffer;

		if (prInitEventAccessReg->u4Address != u4Addr) {
			DBGLOG(INIT, ERROR,
			       "Event reports address incorrect. 0x%08x, 0x%08x.\n",
			       u4Addr, prInitEventAccessReg->u4Address);
			u4Status = WLAN_STATUS_FAILURE;
		}
		*pru4Result = prInitEventAccessReg->u4Data;
	}

exit:
	/* 8. Free CMD Info Packet. */
	cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

	kalMemFree(aucBuffer, PHY_MEM_TYPE, u4EventSize);

	return u4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to get the response of INIT_CMD_ACCESS_REG
*
* @param prAdapter      Pointer to the Adapter structure.
*        ucCmdSeqNum    Sequence number of previous firmware scatter
*        ucSetQuery     Read or write
*        prEvent        the pointer of buffer to store the response
*
* @return WLAN_STATUS_SUCCESS
*         WLAN_STATUS_FAILURE
*/
/*----------------------------------------------------------------------------*/
uint32_t wlanAccessRegisterStatus(IN struct ADAPTER *prAdapter,
				  IN uint8_t ucCmdSeqNum,
				  IN uint8_t ucSetQuery, IN void *prEvent,
				  IN uint32_t u4EventLen)
{
	struct mt66xx_chip_info *prChipInfo;
	struct INIT_WIFI_EVENT *prInitEvent;
	uint32_t u4RxPktLength;
	uint32_t u4Status = WLAN_STATUS_SUCCESS;
	uint8_t ucPortIdx = IMG_DL_STATUS_PORT_IDX;
	struct HW_MAC_RX_DESC *prRxStatus;
	uint8_t ucUnexpectCnt = 0;

	ASSERT(prAdapter);
	prChipInfo = prAdapter->chip_info;

	while (TRUE) {
		if (kalIsCardRemoved(prAdapter->prGlueInfo) == TRUE
		    || fgIsBusAccessFailed == TRUE) {
			u4Status = WLAN_STATUS_FAILURE;
			break;
		} else if (nicRxWaitResponse(prAdapter, ucPortIdx, prEvent,
		    u4EventLen, &u4RxPktLength) != WLAN_STATUS_SUCCESS) {
			GL_RESET_TRIGGER(prAdapter,
					 RST_FLAG_DO_CORE_DUMP |
					 RST_FLAG_PREVENT_POWER_OFF);
			u4Status = WLAN_STATUS_FAILURE;
			break;
		} else {
			/* header checking .. */
			prRxStatus = (struct HW_MAC_RX_DESC *) prEvent;
			if (prRxStatus->u2PktTYpe !=
				RXM_RXD_PKT_TYPE_SW_EVENT) {
				DBGLOG(INIT, ERROR,
					"%s: skip unexpected Rx pkt type[0x%04x]\n",
					__func__, prRxStatus->u2PktTYpe);

				ucUnexpectCnt++;

				if (ucUnexpectCnt > 5) {
					DBGLOG(INIT, WARN,
					"%s: break since ucUnexpectCnt > %d\n",
					__func__, ucUnexpectCnt);
					u4Status = WLAN_STATUS_FAILURE;
					break;
				}
				continue;
			}

			prInitEvent = (struct INIT_WIFI_EVENT *)
				(prEvent + prChipInfo->rxd_size);

			/* EID / SeqNum check */
			if (((prInitEvent->ucEID != INIT_EVENT_ID_CMD_RESULT) &&
			     (ucSetQuery == 1)) ||
			    ((prInitEvent->ucEID != INIT_EVENT_ID_ACCESS_REG)
				&& (ucSetQuery == 0))) {
				GL_RESET_TRIGGER(prAdapter,
						 RST_FLAG_DO_CORE_DUMP |
						 RST_FLAG_PREVENT_POWER_OFF);
				u4Status = WLAN_STATUS_FAILURE;
				DBGLOG(INIT, ERROR,
				       "wlanAccessRegisterStatus: incorrect ucEID. ucSetQuery = 0x%x\n",
				       ucSetQuery);
				break;
			} else if (prInitEvent->ucSeqNum != ucCmdSeqNum) {
				u4Status = WLAN_STATUS_FAILURE;
				GL_RESET_TRIGGER(prAdapter,
						 RST_FLAG_DO_CORE_DUMP |
						 RST_FLAG_PREVENT_POWER_OFF);
				DBGLOG(INIT, ERROR,
				       "wlanAccessRegisterStatus: incorrect ucCmdSeqNum. = 0x%x\n",
				       ucCmdSeqNum);
				break;
			} else {
				break;
			}
		}
	}

	return u4Status;
}
/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called to process queued RX packets
 *
 * @param prAdapter          Pointer to the Adapter structure.
 *        prSwRfbListHead    Pointer to head of RX packets link list
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanProcessQueuedSwRfb(IN struct ADAPTER
				*prAdapter, IN struct SW_RFB *prSwRfbListHead)
{
	struct SW_RFB *prSwRfb, *prNextSwRfb;
	struct TX_CTRL *prTxCtrl;
	struct RX_CTRL *prRxCtrl;
	struct STA_RECORD *prStaRec;

	ASSERT(prAdapter);
	ASSERT(prSwRfbListHead);

	prTxCtrl = &prAdapter->rTxCtrl;
	prRxCtrl = &prAdapter->rRxCtrl;

	prSwRfb = prSwRfbListHead;

	do {
		/* save next first */
		prNextSwRfb = (struct SW_RFB *) QUEUE_GET_NEXT_ENTRY((
					struct QUE_ENTRY *) prSwRfb);

		switch (prSwRfb->eDst) {
		case RX_PKT_DESTINATION_HOST:
			prStaRec = cnmGetStaRecByIndex(prAdapter,
						       prSwRfb->ucStaRecIdx);
			if (prStaRec && IS_STA_IN_AIS(prStaRec)) {
#if ARP_MONITER_ENABLE
				qmHandleRxArpPackets(prAdapter, prSwRfb);
#endif
			}

			nicRxProcessPktWithoutReorder(prAdapter, prSwRfb);
			break;

		case RX_PKT_DESTINATION_FORWARD:
			nicRxProcessForwardPkt(prAdapter, prSwRfb);
			break;

		case RX_PKT_DESTINATION_HOST_WITH_FORWARD:
			nicRxProcessGOBroadcastPkt(prAdapter, prSwRfb);
			break;

		case RX_PKT_DESTINATION_NULL:
			nicRxReturnRFB(prAdapter, prSwRfb);
			break;

		default:
			break;
		}

#if CFG_HIF_RX_STARVATION_WARNING
		prRxCtrl->u4DequeuedCnt++;
#endif
		prSwRfb = prNextSwRfb;
	} while (prSwRfb);

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called to purge queued TX packets
 *        by indicating failure to OS and returned to free list
 *
 * @param prAdapter          Pointer to the Adapter structure.
 *        prMsduInfoListHead Pointer to head of TX packets link list
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanProcessQueuedMsduInfo(IN struct ADAPTER *prAdapter,
				   IN struct MSDU_INFO *prMsduInfoListHead)
{
	ASSERT(prAdapter);
	ASSERT(prMsduInfoListHead);

	nicTxFreeMsduInfoPacket(prAdapter, prMsduInfoListHead);
	nicTxReturnMsduInfo(prAdapter, prMsduInfoListHead);

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called to check if the OID handler needs timeout
 *
 * @param prAdapter          Pointer to the Adapter structure.
 *        pfnOidHandler      Pointer to the OID handler
 *
 * @return TRUE
 *         FALSE
 */
/*----------------------------------------------------------------------------*/
u_int8_t wlanoidTimeoutCheck(IN struct ADAPTER *prAdapter,
			     IN PFN_OID_HANDLER_FUNC pfnOidHandler)
{
	PFN_OID_HANDLER_FUNC *apfnOidHandlerWOTimeoutCheck;
	uint32_t i;
	uint32_t u4NumOfElem;
	uint32_t u4OidTimeout;

	apfnOidHandlerWOTimeoutCheck = apfnOidWOTimeoutCheck;
	u4NumOfElem = sizeof(apfnOidWOTimeoutCheck) / sizeof(
			      PFN_OID_HANDLER_FUNC);

	for (i = 0; i < u4NumOfElem; i++) {
		if (apfnOidHandlerWOTimeoutCheck[i] == pfnOidHandler)
			return FALSE;
	}

	/* Decrease OID timeout threshold if chip NoAck/resetting */
	if (wlanIsChipNoAck(prAdapter)) {
		u4OidTimeout = WLAN_OID_TIMEOUT_THRESHOLD_IN_RESETTING;
		DBGLOG(INIT, INFO,
		       "Decrease OID timeout to %ums due to NoACK/CHIP-RESET\n",
		       u4OidTimeout);
	} else {
		u4OidTimeout = WLAN_OID_TIMEOUT_THRESHOLD;
	}

	/* Set OID timer for timeout check */
	cnmTimerStartTimer(prAdapter,
			   &(prAdapter->rOidTimeoutTimer), u4OidTimeout);

	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called to clear any pending OID timeout check
 *
 * @param prAdapter          Pointer to the Adapter structure.
 *
 * @return none
 */
/*----------------------------------------------------------------------------*/
void wlanoidClearTimeoutCheck(IN struct ADAPTER *prAdapter)
{
	ASSERT(prAdapter);

	cnmTimerStopTimer(prAdapter, &(prAdapter->rOidTimeoutTimer));
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called to override network address
 *        if NVRAM has a valid value
 *
 * @param prAdapter          Pointer to the Adapter structure.
 *
 * @return WLAN_STATUS_FAILURE   The request could not be processed
 *         WLAN_STATUS_PENDING   The request has been queued for later
 *				 processing
 *         WLAN_STATUS_SUCCESS   The request has been processed
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanUpdateNetworkAddress(IN struct ADAPTER
				  *prAdapter)
{
	const uint8_t aucZeroMacAddr[] = NULL_MAC_ADDR;
	uint8_t rMacAddr[PARAM_MAC_ADDR_LEN];
	uint32_t u4SysTime;

	DEBUGFUNC("wlanUpdateNetworkAddress");

	ASSERT(prAdapter);

	if (kalRetrieveNetworkAddress(prAdapter->prGlueInfo,
				      rMacAddr) == FALSE
	    || IS_BMCAST_MAC_ADDR(rMacAddr)
	    || EQUAL_MAC_ADDR(aucZeroMacAddr, rMacAddr)) {
		/* eFUSE has a valid address, don't do anything */
		if (prAdapter->fgIsEmbbededMacAddrValid == TRUE) {
#if CFG_SHOW_MACADDR_SOURCE
			DBGLOG(INIT, INFO, "Using embedded MAC address");
#endif
			return WLAN_STATUS_SUCCESS;
		}
#if CFG_SHOW_MACADDR_SOURCE
		DBGLOG(INIT, INFO,
		       "Using dynamically generated MAC address");
#endif
		/* dynamic generate */
		u4SysTime = (uint32_t) kalGetTimeTick();

		rMacAddr[0] = 0x00;
		rMacAddr[1] = 0x08;
		rMacAddr[2] = 0x22;

		kalMemCopy(&rMacAddr[3], &u4SysTime, 3);

	} else {
#if CFG_SHOW_MACADDR_SOURCE
		DBGLOG(INIT, INFO, "Using host-supplied MAC address");
#endif
	}

	COPY_MAC_ADDR(prAdapter->rWifiVar.aucMacAddress, rMacAddr);

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called to update basic configuration into firmware
 *        domain
 *
 * @param prAdapter		Pointer to the Adapter structure.
 *
 * @return WLAN_STATUS_FAILURE	The request could not be processed
 *         WLAN_STATUS_PENDING	The request has been queued for later
 *				processing
 *         WLAN_STATUS_SUCCESS	The request has been processed
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanUpdateBasicConfig(IN struct ADAPTER *prAdapter)
{
	uint8_t ucCmdSeqNum;
	struct CMD_INFO *prCmdInfo;
	struct WIFI_CMD *prWifiCmd;
	struct CMD_BASIC_CONFIG *prCmdBasicConfig;
	struct PSE_CMD_HDR *prPseCmdHdr;
	uint32_t rResult;
	struct WIFI_VAR *prWifiVar = &prAdapter->rWifiVar;

	DEBUGFUNC("wlanUpdateBasicConfig");

	ASSERT(prAdapter);

	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter,
				CMD_HDR_SIZE + sizeof(struct CMD_BASIC_CONFIG));

	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}
	/* increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

	/* compose CMD_BUILD_CONNECTION cmd pkt */
	prCmdInfo->eCmdType = COMMAND_TYPE_GENERAL_IOCTL;
	prCmdInfo->u2InfoBufLen = CMD_HDR_SIZE + sizeof(
					  struct CMD_BASIC_CONFIG);
	prCmdInfo->pfCmdDoneHandler = NULL;
	prCmdInfo->pfCmdTimeoutHandler = NULL;
	prCmdInfo->fgIsOid = FALSE;
	prCmdInfo->ucCID = CMD_ID_BASIC_CONFIG;
	prCmdInfo->fgSetQuery = TRUE;
	prCmdInfo->fgNeedResp = FALSE;
	prCmdInfo->ucCmdSeqNum = ucCmdSeqNum;
	prCmdInfo->u4SetInfoLen = sizeof(struct CMD_BASIC_CONFIG);

	/* Setup WIFI_CMD_T */
	prWifiCmd = (struct WIFI_CMD *) (prCmdInfo->pucInfoBuffer);
	prWifiCmd->u2TxByteCount = prCmdInfo->u2InfoBufLen;
	prWifiCmd->u2PQ_ID = CMD_PQ_ID;
	prWifiCmd->ucPktTypeID = CMD_PACKET_TYPE_ID;
	prWifiCmd->ucCID = prCmdInfo->ucCID;
	prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
	prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;

	prPseCmdHdr = (struct PSE_CMD_HDR *) (
			      prCmdInfo->pucInfoBuffer);
	prPseCmdHdr->u2Qidx = TXD_Q_IDX_MCU_RQ0;
	prPseCmdHdr->u2Pidx = TXD_P_IDX_MCU;
	prPseCmdHdr->u2Hf = TXD_HF_CMD;
	prPseCmdHdr->u2Ft = TXD_FT_LONG_FORMAT;
	prPseCmdHdr->u2PktFt = TXD_PKT_FT_CMD;

	prWifiCmd->u2Length = prWifiCmd->u2TxByteCount - sizeof(
				      struct PSE_CMD_HDR);

	/* configure CMD_BASIC_CONFIG */

	prCmdBasicConfig = (struct CMD_BASIC_CONFIG *) (
				   prWifiCmd->aucBuffer);
	kalMemZero(prCmdBasicConfig,
		   sizeof(struct CMD_BASIC_CONFIG));
	prCmdBasicConfig->ucNative80211 = 0;
	prCmdBasicConfig->rCsumOffload.u2RxChecksum = 0;
	prCmdBasicConfig->rCsumOffload.u2TxChecksum = 0;
	prCmdBasicConfig->ucCtrlFlagAssertPath =
		prWifiVar->ucCtrlFlagAssertPath;
	prCmdBasicConfig->ucCtrlFlagDebugLevel =
		prWifiVar->ucCtrlFlagDebugLevel;

#if CFG_TCP_IP_CHKSUM_OFFLOAD
	if (prAdapter->fgIsSupportCsumOffload) {
		if (prAdapter->u4CSUMFlags & CSUM_OFFLOAD_EN_TX_TCP)
			prCmdBasicConfig->rCsumOffload.u2TxChecksum |= BIT(2);

		if (prAdapter->u4CSUMFlags & CSUM_OFFLOAD_EN_TX_UDP)
			prCmdBasicConfig->rCsumOffload.u2TxChecksum |= BIT(1);

		if (prAdapter->u4CSUMFlags & CSUM_OFFLOAD_EN_TX_IP)
			prCmdBasicConfig->rCsumOffload.u2TxChecksum |= BIT(0);

		if (prAdapter->u4CSUMFlags & CSUM_OFFLOAD_EN_RX_TCP)
			prCmdBasicConfig->rCsumOffload.u2RxChecksum |= BIT(2);

		if (prAdapter->u4CSUMFlags & CSUM_OFFLOAD_EN_RX_UDP)
			prCmdBasicConfig->rCsumOffload.u2RxChecksum |= BIT(1);

		if (prAdapter->u4CSUMFlags & (CSUM_OFFLOAD_EN_RX_IPv4 |
					      CSUM_OFFLOAD_EN_RX_IPv6))
			prCmdBasicConfig->rCsumOffload.u2RxChecksum |= BIT(0);
	}
#endif

	rResult = wlanSendCommand(prAdapter, prCmdInfo);

	if (rResult != WLAN_STATUS_SUCCESS) {
		kalEnqueueCommand(prAdapter->prGlueInfo,
				  (struct QUE_ENTRY *) prCmdInfo);

		return WLAN_STATUS_PENDING;
	}
	cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

	return WLAN_STATUS_SUCCESS;

}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called to check if the device is in RF test mode
 *
 * @param pfnOidHandler      Pointer to the OID handler
 *
 * @return TRUE
 *         FALSE
 */
/*----------------------------------------------------------------------------*/
u_int8_t wlanQueryTestMode(IN struct ADAPTER *prAdapter)
{
	ASSERT(prAdapter);

	return prAdapter->fgTestMode;
}

u_int8_t wlanProcessTxFrame(IN struct ADAPTER *prAdapter,
			    IN void *prPacket)
{
	uint32_t u4SysTime;
	uint8_t ucMacHeaderLen;
	struct TX_PACKET_INFO rTxPacketInfo;
	struct mt66xx_chip_info *prChipInfo;

	ASSERT(prAdapter);
	ASSERT(prPacket);
	prChipInfo = prAdapter->chip_info;

	if (kalQoSFrameClassifierAndPacketInfo(
		    prAdapter->prGlueInfo, prPacket, &rTxPacketInfo)) {

		/* Save the value of Priority Parameter */
		GLUE_SET_PKT_TID(prPacket, rTxPacketInfo.ucPriorityParam);

		if (rTxPacketInfo.u2Flag) {
			if (rTxPacketInfo.u2Flag & BIT(ENUM_PKT_1X)) {
				struct STA_RECORD *prStaRec;

				DBGLOG(RSN, INFO, "T1X len=%d\n",
				       rTxPacketInfo.u4PacketLen);

				prStaRec = cnmGetStaRecByAddress(prAdapter,
						GLUE_GET_PKT_BSS_IDX(prPacket),
						rTxPacketInfo.aucEthDestAddr);

				GLUE_SET_PKT_FLAG(prPacket, ENUM_PKT_1X);
				/*
				 * if (secIsProtected1xFrame(prAdapter,
				 *     prStaRec) &&
				 *     !kalIs24Of4Packet(prPacket))
				 *	GLUE_SET_PKT_FLAG(prPacket,
				 *			ENUM_PKT_PROTECTED_1X);
				 */
			}

			if (rTxPacketInfo.u2Flag &
			    BIT(ENUM_PKT_NON_PROTECTED_1X))
				GLUE_SET_PKT_FLAG(prPacket,
						  ENUM_PKT_NON_PROTECTED_1X);

			if (rTxPacketInfo.u2Flag & BIT(ENUM_PKT_802_3))
				GLUE_SET_PKT_FLAG(prPacket, ENUM_PKT_802_3);

			if (rTxPacketInfo.u2Flag & BIT(ENUM_PKT_VLAN_EXIST)
			    && FEAT_SUP_LLC_VLAN_TX(prChipInfo))
				GLUE_SET_PKT_FLAG(prPacket,
						  ENUM_PKT_VLAN_EXIST);

			if (rTxPacketInfo.u2Flag & BIT(ENUM_PKT_DHCP))
				GLUE_SET_PKT_FLAG(prPacket, ENUM_PKT_DHCP);

			if (rTxPacketInfo.u2Flag & BIT(ENUM_PKT_ARP))
				GLUE_SET_PKT_FLAG(prPacket, ENUM_PKT_ARP);

			if (rTxPacketInfo.u2Flag & BIT(ENUM_PKT_ICMP))
				GLUE_SET_PKT_FLAG(prPacket, ENUM_PKT_ICMP);

			if (rTxPacketInfo.u2Flag & BIT(ENUM_PKT_TDLS))
				GLUE_SET_PKT_FLAG(prPacket, ENUM_PKT_TDLS);

			if (rTxPacketInfo.u2Flag & BIT(ENUM_PKT_DNS))
				GLUE_SET_PKT_FLAG(prPacket, ENUM_PKT_DNS);

		}

		ucMacHeaderLen = ETHER_HEADER_LEN;

		/* Save the value of Header Length */
		GLUE_SET_PKT_HEADER_LEN(prPacket, ucMacHeaderLen);

		/* Save the value of Frame Length */
		GLUE_SET_PKT_FRAME_LEN(prPacket,
				       (uint16_t) rTxPacketInfo.u4PacketLen);

		/* Save the value of Arrival Time */
		u4SysTime = (OS_SYSTIME) kalGetTimeTick();
		GLUE_SET_PKT_ARRIVAL_TIME(prPacket, u4SysTime);

		return TRUE;
	}

	return FALSE;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called to identify 802.1x and Bluetooth-over-Wi-Fi
 *        security frames, and queued into command queue for strict ordering
 *        due to 802.1x frames before add-key OIDs are not to be encrypted
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 * @param prPacket       Pointer of native packet
 *
 * @return TRUE
 *         FALSE
 */
/*----------------------------------------------------------------------------*/
u_int8_t wlanProcessSecurityFrame(IN struct ADAPTER
				  *prAdapter, IN void *prPacket)
{
	struct CMD_INFO *prCmdInfo;
	struct STA_RECORD *prStaRec;
	uint8_t ucBssIndex;
	uint32_t u4PacketLen;
	uint8_t aucEthDestAddr[PARAM_MAC_ADDR_LEN];
	struct MSDU_INFO *prMsduInfo;
	uint8_t ucStaRecIndex;

	ASSERT(prAdapter);
	ASSERT(prPacket);

	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, 0);

	/* Get MSDU_INFO for TxDone */
	prMsduInfo = cnmPktAlloc(prAdapter, 0);

	u4PacketLen = (uint32_t) GLUE_GET_PKT_FRAME_LEN(prPacket);

	if (prCmdInfo && prMsduInfo) {
		ucBssIndex = GLUE_GET_PKT_BSS_IDX(prPacket);

		kalGetEthDestAddr(prAdapter->prGlueInfo, prPacket,
				  aucEthDestAddr);

		prStaRec = cnmGetStaRecByAddress(prAdapter, ucBssIndex,
						 aucEthDestAddr);

		prCmdInfo->eCmdType = COMMAND_TYPE_SECURITY_FRAME;
		prCmdInfo->u2InfoBufLen = (uint16_t) u4PacketLen;
		prCmdInfo->prPacket = prPacket;
		prCmdInfo->prMsduInfo = prMsduInfo;
		prCmdInfo->pfCmdDoneHandler = wlanSecurityFrameTxDone;
		prCmdInfo->pfCmdTimeoutHandler = wlanSecurityFrameTxTimeout;
		prCmdInfo->fgIsOid = FALSE;
		prCmdInfo->fgSetQuery = TRUE;
		prCmdInfo->fgNeedResp = FALSE;

		if (prStaRec)
			ucStaRecIndex = prStaRec->ucIndex;
		else
			ucStaRecIndex = STA_REC_INDEX_NOT_FOUND;

		/* Fill-up MSDU_INFO */
		nicTxSetDataPacket(prAdapter, prMsduInfo, ucBssIndex,
				   ucStaRecIndex, 0, u4PacketLen,
				   nicTxDummyTxDone, MSDU_RATE_MODE_AUTO,
				   TX_PACKET_OS, 0, FALSE, TRUE);

		prMsduInfo->prPacket = prPacket;
		/* No Tx descriptor template for MMPDU */
		prMsduInfo->fgIsTXDTemplateValid = FALSE;

		if (GLUE_TEST_PKT_FLAG(prPacket, ENUM_PKT_PROTECTED_1X))
			nicTxConfigPktOption(prMsduInfo,
					MSDU_OPT_PROTECTED_FRAME, TRUE);
#if CFG_SUPPORT_MULTITHREAD
		nicTxComposeSecurityFrameDesc(prAdapter, prCmdInfo,
					prMsduInfo->aucTxDescBuffer, NULL);
#endif

		kalEnqueueCommand(prAdapter->prGlueInfo,
				  (struct QUE_ENTRY *) prCmdInfo);

		GLUE_SET_EVENT(prAdapter->prGlueInfo);

		return TRUE;
	}
	DBGLOG(RSN, INFO,
	       "Failed to alloc CMD/MGMT INFO for 1X frame!!\n");
	cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
	cnmPktFree(prAdapter, prMsduInfo);


	return FALSE;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called when 802.1x or Bluetooth-over-Wi-Fi
 *        security frames has been sent to firmware
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 * @param prCmdInfo      Pointer of CMD_INFO_T
 * @param pucEventBuf    meaningless, only for API compatibility
 *
 * @return none
 */
/*----------------------------------------------------------------------------*/
void wlanSecurityFrameTxDone(IN struct ADAPTER *prAdapter,
			     IN struct CMD_INFO *prCmdInfo,
			     IN uint8_t *pucEventBuf)
{
	struct MSDU_INFO *prMsduInfo = prCmdInfo->prMsduInfo;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	if (GET_BSS_INFO_BY_INDEX(prAdapter,
				  prMsduInfo->ucBssIndex)->eNetworkType ==
	    NETWORK_TYPE_AIS
	    && prAdapter->rWifiVar.rAisSpecificBssInfo.fgCounterMeasure) {
		struct STA_RECORD *prSta = cnmGetStaRecByIndex(prAdapter,
					   prMsduInfo->ucBssIndex);

		if (prSta) {
			kalMsleep(10);
			if (authSendDeauthFrame(prAdapter,
				GET_BSS_INFO_BY_INDEX(prAdapter,
					prMsduInfo->ucBssIndex), prSta,
					(struct SW_RFB *) NULL,
					REASON_CODE_MIC_FAILURE,
					(PFN_TX_DONE_HANDLER) NULL
					/* secFsmEventDeauthTxDone left upper
					 * layer handle the 60 timer
					 */
					) != WLAN_STATUS_SUCCESS) {
				ASSERT(FALSE);
			}
			/* secFsmEventEapolTxDone(prAdapter, prSta,
			 *			  TX_RESULT_SUCCESS);
			 */
		}
	}

	kalSecurityFrameSendComplete(prAdapter->prGlueInfo,
				     prCmdInfo->prPacket, WLAN_STATUS_SUCCESS);
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called when 802.1x or Bluetooth-over-Wi-Fi
 *        security frames has failed sending to firmware
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 * @param prCmdInfo      Pointer of CMD_INFO_T
 *
 * @return none
 */
/*----------------------------------------------------------------------------*/
void wlanSecurityFrameTxTimeout(IN struct ADAPTER
				*prAdapter, IN struct CMD_INFO *prCmdInfo)
{
	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	kalSecurityFrameSendComplete(prAdapter->prGlueInfo,
				     prCmdInfo->prPacket, WLAN_STATUS_FAILURE);
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called before AIS is starting a new scan
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 *
 * @return none
 */
/*----------------------------------------------------------------------------*/
void wlanClearScanningResult(IN struct ADAPTER *prAdapter)
{
	u_int8_t fgKeepCurrOne = FALSE;
	uint32_t i;
	struct WLAN_INFO *prWlanInfo;

	ASSERT(prAdapter);
	prWlanInfo = &(prAdapter->rWlanInfo);

	/* clear scanning result */
	if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) ==
	    PARAM_MEDIA_STATE_CONNECTED) {

		for (i = 0; i < prWlanInfo->u4ScanResultNum; i++) {

		if (EQUAL_MAC_ADDR(
		    prWlanInfo->rCurrBssId.arMacAddress,
		    prWlanInfo->arScanResult[i].arMacAddress)) {
			fgKeepCurrOne = TRUE;

			if (i != 0) {
				/* copy structure */
				kalMemCopy(
				    &(prWlanInfo->arScanResult[0]),
				    &(prWlanInfo->arScanResult[i]),
				    OFFSET_OF(struct PARAM_BSSID_EX,
					      aucIEs));
			}

			if (prWlanInfo->arScanResult[i].u4IELength > 0) {
				if (prWlanInfo->apucScanResultIEs[i] !=
				    &(prWlanInfo->aucScanIEBuf[0])) {

				/* move IEs to head */
				kalMemCopy(prWlanInfo->aucScanIEBuf,
					   prWlanInfo->apucScanResultIEs[i],
					   prWlanInfo->arScanResult[i]
					   .u4IELength);
				}

				/* modify IE pointer */
				prWlanInfo->apucScanResultIEs[0] =
					&(prWlanInfo->aucScanIEBuf[0]);

			} else {
				prWlanInfo->apucScanResultIEs[0] = NULL;
			}

			break;
		} /* if */
		} /* for */
	}

	if (fgKeepCurrOne == TRUE) {
		prWlanInfo->u4ScanResultNum = 1;
		prWlanInfo->u4ScanIEBufferUsage =
		    ALIGN_4(prWlanInfo->arScanResult[0].u4IELength);
	} else {
		prWlanInfo->u4ScanResultNum = 0;
		prWlanInfo->u4ScanIEBufferUsage = 0;
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called when AIS received a beacon timeout event
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 * @param arBSSID        MAC address of the specified BSS
 *
 * @return none
 */
/*----------------------------------------------------------------------------*/
void wlanClearBssInScanningResult(IN struct ADAPTER
				  *prAdapter, IN uint8_t *arBSSID)
{
	uint32_t i, j, u4IELength = 0, u4IEMoveLength;
	uint8_t *pucIEPtr;
	struct WLAN_INFO *prWlanInfo;

	ASSERT(prAdapter);
	prWlanInfo = &(prAdapter->rWlanInfo);

	/* clear scanning result */
	i = 0;
	while (1) {
		if (i >= prWlanInfo->u4ScanResultNum)
			break;

		if (EQUAL_MAC_ADDR(arBSSID,
				   prWlanInfo->arScanResult[i].arMacAddress)) {
			/* backup current IE length */
			u4IELength =
				ALIGN_4(prWlanInfo->arScanResult[i].u4IELength);
			pucIEPtr = prWlanInfo->apucScanResultIEs[i];

			/* removed from middle */
			for (j = i + 1; j < prWlanInfo->u4ScanResultNum; j++) {
				kalMemCopy(&(prWlanInfo->arScanResult[j - 1]),
					   &(prWlanInfo->arScanResult[j]),
					   OFFSET_OF(struct PARAM_BSSID_EX,
					   aucIEs));

				prWlanInfo->apucScanResultIEs[j - 1] =
					prWlanInfo->apucScanResultIEs[j];
			}

			prWlanInfo->u4ScanResultNum--;

			/* remove IE buffer if needed := move rest of IE buffer
			 */
			if (u4IELength > 0) {
				u4IEMoveLength = prWlanInfo->u4ScanIEBufferUsage
					- (((unsigned long) pucIEPtr)
					+ u4IELength
					- ((unsigned long)
					(&(prWlanInfo->aucScanIEBuf[0]))));

				kalMemCopy(pucIEPtr,
					   (uint8_t *) (((unsigned long)
					   pucIEPtr) + u4IELength),
					   u4IEMoveLength);

				prWlanInfo->u4ScanIEBufferUsage -=
								u4IELength;

				/* correction of pointers to IE buffer */
				for (j = 0; j < prWlanInfo->u4ScanResultNum;
				     j++) {
					if (prWlanInfo->apucScanResultIEs[j] >
					    pucIEPtr) {
					prWlanInfo->apucScanResultIEs[j] =
					    (uint8_t *)((unsigned long)
					    (prWlanInfo->apucScanResultIEs[j]) -
					    u4IELength);
					}
				}
			}
		}

		i++;
	}
}

#if CFG_TEST_WIFI_DIRECT_GO
void wlanEnableP2pFunction(IN struct ADAPTER *prAdapter)
{
#if 0
	P_MSG_P2P_FUNCTION_SWITCH_T prMsgFuncSwitch =
		(P_MSG_P2P_FUNCTION_SWITCH_T) NULL;

	prMsgFuncSwitch =
		(P_MSG_P2P_FUNCTION_SWITCH_T) cnmMemAlloc(prAdapter,
			RAM_TYPE_MSG, sizeof(MSG_P2P_FUNCTION_SWITCH_T));
	if (!prMsgFuncSwitch) {
		ASSERT(FALSE);
		return;
	}

	prMsgFuncSwitch->rMsgHdr.eMsgId = MID_MNY_P2P_FUN_SWITCH;
	prMsgFuncSwitch->fgIsFuncOn = TRUE;

	mboxSendMsg(prAdapter, MBOX_ID_0,
		    (struct MSG_HDR *) prMsgFuncSwitch, MSG_SEND_METHOD_BUF);
#endif

}

void wlanEnableATGO(IN struct ADAPTER *prAdapter)
{

	struct MSG_P2P_CONNECTION_REQUEST *prMsgConnReq =
		(struct MSG_P2P_CONNECTION_REQUEST *) NULL;
	uint8_t aucTargetDeviceID[MAC_ADDR_LEN] = { 0xFF, 0xFF, 0xFF, 0xFF,
						    0xFF, 0xFF };

	prMsgConnReq =
		(struct MSG_P2P_CONNECTION_REQUEST *) cnmMemAlloc(prAdapter,
		RAM_TYPE_MSG, sizeof(struct MSG_P2P_CONNECTION_REQUEST));
	if (!prMsgConnReq) {
		ASSERT(FALSE);
		return;
	}

	prMsgConnReq->rMsgHdr.eMsgId = MID_MNY_P2P_CONNECTION_REQ;

	/*=====Param Modified for test=====*/
	COPY_MAC_ADDR(prMsgConnReq->aucDeviceID, aucTargetDeviceID);
	prMsgConnReq->fgIsTobeGO = TRUE;
	prMsgConnReq->fgIsPersistentGroup = FALSE;

	/*=====Param Modified for test=====*/

	mboxSendMsg(prAdapter, MBOX_ID_0,
		    (struct MSG_HDR *) prMsgConnReq, MSG_SEND_METHOD_BUF);

}
#endif

void wlanPrintVersion(IN struct ADAPTER *prAdapter)
{
	uint8_t aucBuf[512];

	kalMemZero(aucBuf, 512);

#if CFG_ENABLE_FW_DOWNLOAD
	fwDlGetFwdlInfo(prAdapter, aucBuf, 512);
#endif
	DBGLOG(SW4, INFO, "%s", aucBuf);
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called to retrieve NIC capability from firmware
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 *
 * @return WLAN_STATUS_SUCCESS
 *         WLAN_STATUS_FAILURE
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanQueryNicCapability(IN struct ADAPTER
				*prAdapter)
{
	struct mt66xx_chip_info *prChipInfo;
	uint8_t aucZeroMacAddr[] = NULL_MAC_ADDR;
	uint8_t ucCmdSeqNum;
	struct CMD_INFO *prCmdInfo;
	struct WIFI_CMD *prWifiCmd;
	uint32_t u4RxPktLength;
	uint8_t *aucBuffer;
	uint32_t u4EventSize;
	struct HW_MAC_RX_DESC *prRxStatus;
	struct WIFI_EVENT *prEvent;
	struct EVENT_NIC_CAPABILITY *prEventNicCapability;
	struct PSE_CMD_HDR *prPseCmdHdr;

	ASSERT(prAdapter);
	prChipInfo = prAdapter->chip_info;

	DEBUGFUNC("wlanQueryNicCapability");

	/* 1. Allocate CMD Info Packet and its Buffer */
	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter,
			CMD_HDR_SIZE + sizeof(struct EVENT_NIC_CAPABILITY));
	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}

	u4EventSize = prChipInfo->rxd_size + prChipInfo->event_hdr_size +
		sizeof(struct EVENT_NIC_CAPABILITY);
	aucBuffer = kalMemAlloc(u4EventSize, PHY_MEM_TYPE);

	/* increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

	/* compose CMD_BUILD_CONNECTION cmd pkt */
	prCmdInfo->eCmdType = COMMAND_TYPE_GENERAL_IOCTL;
	prCmdInfo->u2InfoBufLen = CMD_HDR_SIZE + sizeof(
					  struct EVENT_NIC_CAPABILITY);
	prCmdInfo->pfCmdDoneHandler = NULL;
	prCmdInfo->fgIsOid = FALSE;
	prCmdInfo->ucCID = CMD_ID_GET_NIC_CAPABILITY;
	prCmdInfo->fgSetQuery = FALSE;
	prCmdInfo->fgNeedResp = TRUE;
	prCmdInfo->ucCmdSeqNum = ucCmdSeqNum;
	prCmdInfo->u4SetInfoLen = 0;

	/* Setup WIFI_CMD_T */
	prWifiCmd = (struct WIFI_CMD *) (prCmdInfo->pucInfoBuffer);
	prWifiCmd->u2TxByteCount = prCmdInfo->u2InfoBufLen;
	prWifiCmd->u2PQ_ID = CMD_PQ_ID;
	prWifiCmd->ucPktTypeID = CMD_PACKET_TYPE_ID;
	prWifiCmd->ucCID = prCmdInfo->ucCID;
	prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
	prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;

	prPseCmdHdr = (struct PSE_CMD_HDR *) (
			      prCmdInfo->pucInfoBuffer);
	prPseCmdHdr->u2Qidx = TXD_Q_IDX_MCU_RQ0;
	prPseCmdHdr->u2Pidx = TXD_P_IDX_MCU;
	prPseCmdHdr->u2Hf = TXD_HF_CMD;
	prPseCmdHdr->u2Ft = TXD_FT_LONG_FORMAT;
	prPseCmdHdr->u2PktFt = TXD_PKT_FT_CMD;

	prWifiCmd->u2Length = prWifiCmd->u2TxByteCount - sizeof(
				      struct PSE_CMD_HDR);

	wlanSendCommand(prAdapter, prCmdInfo);

	cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

	while (TRUE) {
		if (nicRxWaitResponse(prAdapter, 1, aucBuffer, u4EventSize,
				      &u4RxPktLength) != WLAN_STATUS_SUCCESS) {

			DBGLOG(INIT, WARN, "%s: wait for event failed!\n",
			       __func__);
			kalMemFree(aucBuffer, PHY_MEM_TYPE, u4EventSize);
			return WLAN_STATUS_FAILURE;
		}
		/* header checking .. */
		prRxStatus = (struct HW_MAC_RX_DESC *) aucBuffer;
		if (prRxStatus->u2PktTYpe != RXM_RXD_PKT_TYPE_SW_EVENT) {
			DBGLOG(INIT, WARN,
			       "%s: skip unexpected Rx pkt type[0x%04x]\n",
			       __func__, prRxStatus->u2PktTYpe);
			continue;
		}

		prEvent = (struct WIFI_EVENT *)
			(aucBuffer + prChipInfo->rxd_size);
		prEventNicCapability =
			(struct EVENT_NIC_CAPABILITY *)prEvent->aucBuffer;

		if (prEvent->ucEID != EVENT_ID_NIC_CAPABILITY) {
			DBGLOG(INIT, WARN,
			      "%s: skip unexpected event ID[0x%02x]\n",
			      __func__, prEvent->ucEID);
			continue;
		} else {
			break;
		}
	}

	prEventNicCapability = (struct EVENT_NIC_CAPABILITY *) (
				       prEvent->aucBuffer);

	prAdapter->rVerInfo.u2FwProductID =
		prEventNicCapability->u2ProductID;
	kalMemCopy(prAdapter->rVerInfo.aucFwBranchInfo,
		   prEventNicCapability->aucBranchInfo, 4);
	prAdapter->rVerInfo.u2FwOwnVersion =
		prEventNicCapability->u2FwVersion;
	prAdapter->rVerInfo.ucFwBuildNumber =
		prEventNicCapability->ucFwBuildNumber;
	kalMemCopy(prAdapter->rVerInfo.aucFwDateCode,
		   prEventNicCapability->aucDateCode, 16);
	prAdapter->rVerInfo.u2FwPeerVersion =
		prEventNicCapability->u2DriverVersion;
	prAdapter->fgIsHw5GBandDisabled =
			(u_int8_t)prEventNicCapability->ucHw5GBandDisabled;
	prAdapter->fgIsEepromUsed =
			(u_int8_t)prEventNicCapability->ucEepromUsed;
	prAdapter->fgIsEmbbededMacAddrValid =
			(u_int8_t)(!IS_BMCAST_MAC_ADDR(
				prEventNicCapability->aucMacAddr) &&
				!EQUAL_MAC_ADDR(aucZeroMacAddr,
				prEventNicCapability->aucMacAddr));

	COPY_MAC_ADDR(prAdapter->rWifiVar.aucPermanentAddress,
		      prEventNicCapability->aucMacAddr);
	COPY_MAC_ADDR(prAdapter->rWifiVar.aucMacAddress,
		      prEventNicCapability->aucMacAddr);

	prAdapter->rWifiVar.ucStaVht &=
				(!(prEventNicCapability->ucHwNotSupportAC));
	prAdapter->rWifiVar.ucApVht &=
				(!(prEventNicCapability->ucHwNotSupportAC));
	prAdapter->rWifiVar.ucP2pGoVht &=
				(!(prEventNicCapability->ucHwNotSupportAC));
	prAdapter->rWifiVar.ucP2pGcVht &=
				(!(prEventNicCapability->ucHwNotSupportAC));
	prAdapter->rWifiVar.ucHwNotSupportAC =
				prEventNicCapability->ucHwNotSupportAC;

	prAdapter->u4FwCompileFlag0 =
		prEventNicCapability->u4CompileFlag0;
	prAdapter->u4FwCompileFlag1 =
		prEventNicCapability->u4CompileFlag1;
	prAdapter->u4FwFeatureFlag0 =
		prEventNicCapability->u4FeatureFlag0;
	prAdapter->u4FwFeatureFlag1 =
		prEventNicCapability->u4FeatureFlag1;

	if (prEventNicCapability->ucHwSetNss1x1)
		prAdapter->rWifiVar.ucNSS = 1;

#if CFG_SUPPORT_DBDC
	if (prEventNicCapability->ucHwNotSupportDBDC)
		prAdapter->rWifiVar.eDbdcMode = ENUM_DBDC_MODE_DISABLED;
#endif
	if (prEventNicCapability->ucHwBssIdNum > 0
	    && prEventNicCapability->ucHwBssIdNum <= MAX_BSSID_NUM) {
		prAdapter->ucHwBssIdNum =
			prEventNicCapability->ucHwBssIdNum;
		prAdapter->ucP2PDevBssIdx = prAdapter->ucHwBssIdNum;
		/* v1 event does not report WmmSetNum,
		 * Assume it is the same as HwBssNum
		 */
		prAdapter->ucWmmSetNum =
			prEventNicCapability->ucHwBssIdNum;
		prAdapter->aprBssInfo[prAdapter->ucP2PDevBssIdx] =
			&prAdapter->rWifiVar.rP2pDevInfo;
	}

#if CFG_ENABLE_CAL_LOG
	DBGLOG(INIT, TRACE,
	       "RF CAL FAIL  = (%d),BB CAL FAIL  = (%d)\n",
	       prEventNicCapability->ucRfCalFail,
	       prEventNicCapability->ucBbCalFail);
#endif
	kalMemFree(aucBuffer, PHY_MEM_TYPE, u4EventSize);

	return WLAN_STATUS_SUCCESS;
}

#if TXPWR_USE_PDSLOPE

/*----------------------------------------------------------------------------*/
/*!
 * @brief
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 *
 * @return WLAN_STATUS_SUCCESS
 *         WLAN_STATUS_FAILURE
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanQueryPdMcr(IN struct ADAPTER *prAdapter,
			struct PARAM_MCR_RW_STRUCT *prMcrRdInfo)
{
	struct mt66xx_chip_info *prChipInfo;
	uint8_t ucCmdSeqNum;
	struct CMD_INFO *prCmdInfo;
	struct WIFI_CMD *prWifiCmd;
	uint32_t u4RxPktLength;
	uint8_t *aucBuffer;
	uint32_t u4EventSize;
	struct HW_MAC_RX_DESC *prRxStatus;
	struct WIFI_EVENT *prEvent;
	struct CMD_ACCESS_REG *prCmdMcrQuery;

	ASSERT(prAdapter);
	prChipInfo = prAdapter->chip_info;

	/* 1. Allocate CMD Info Packet and its Buffer */
	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter,
				CMD_HDR_SIZE + sizeof(struct CMD_ACCESS_REG));

	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}

	u4EventSize = prChipInfo->rxd_size + prChipInfo->event_hdr_size +
		struct CMD_ACCESS_REG;
	aucBuffer = kalMemAlloc(u4EventSize, PHY_MEM_TYPE);

	/* increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

	/* compose CMD_BUILD_CONNECTION cmd pkt */
	prCmdInfo->eCmdType = COMMAND_TYPE_GENERAL_IOCTL;
	prCmdInfo->u2InfoBufLen = (uint16_t) (CMD_HDR_SIZE + sizeof(
			struct CMD_ACCESS_REG));
	prCmdInfo->pfCmdDoneHandler = NULL;
	prCmdInfo->pfCmdTimeoutHandler = nicOidCmdTimeoutCommon;
	prCmdInfo->fgIsOid = FALSE;
	prCmdInfo->ucCID = CMD_ID_ACCESS_REG;
	prCmdInfo->fgSetQuery = FALSE;
	prCmdInfo->fgNeedResp = TRUE;
	prCmdInfo->ucCmdSeqNum = ucCmdSeqNum;
	prCmdInfo->u4SetInfoLen = sizeof(struct CMD_ACCESS_REG);

	/* Setup WIFI_CMD_T */
	prWifiCmd = (struct WIFI_CMD *) (prCmdInfo->pucInfoBuffer);
	prWifiCmd->u2TxByteCount = prCmdInfo->u2InfoBufLen;
	prWifiCmd->u2PQ_ID = CMD_PQ_ID;
	prWifiCmd->ucPktTypeID = CMD_PACKET_TYPE_ID;
	prWifiCmd->ucCID = prCmdInfo->ucCID;
	prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
	prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;
	kalMemCopy(prWifiCmd->aucBuffer, prMcrRdInfo,
		   sizeof(struct CMD_ACCESS_REG));

	wlanSendCommand(prAdapter, prCmdInfo);
	cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

	if (nicRxWaitResponse(prAdapter, 1, aucBuffer, u4EventSize,
			      &u4RxPktLength) != WLAN_STATUS_SUCCESS) {
		kalMemFree(aucBuffer, PHY_MEM_TYPE, u4EventSize);
		return WLAN_STATUS_FAILURE;
	}
	/* header checking .. */
	prRxStatus = (struct HW_MAC_RX_DESC *) aucBuffer;
	if (prRxStatus->u2PktTYpe != RXM_RXD_PKT_TYPE_SW_EVENT) {
		kalMemFree(aucBuffer, PHY_MEM_TYPE, u4EventSize);
		return WLAN_STATUS_FAILURE;
	}

	prEvent = (struct WIFI_EVENT *)
		(aucBuffer + prChipInfo->rxd_size);
	if (prEvent->ucEID != EVENT_ID_ACCESS_REG) {
		kalMemFree(aucBuffer, PHY_MEM_TYPE, u4EventSize);
		return WLAN_STATUS_FAILURE;
	}

	prCmdMcrQuery = (struct CMD_ACCESS_REG *) (
				prEvent->aucBuffer);
	prMcrRdInfo->u4McrOffset = prCmdMcrQuery->u4Address;
	prMcrRdInfo->u4McrData = prCmdMcrQuery->u4Data;

	kalMemFree(aucBuffer, PHY_MEM_TYPE, u4EventSize);

	return WLAN_STATUS_SUCCESS;
}

static int32_t wlanIntRound(int32_t au4Input)
{

	if (au4Input >= 0) {
		if ((au4Input % 10) == 5) {
			au4Input = au4Input + 5;
			return au4Input;
		}
	}

	if (au4Input < 0) {
		if ((au4Input % 10) == -5) {
			au4Input = au4Input - 5;
			return au4Input;
		}
	}

	return au4Input;
}

static int32_t wlanCal6628EfuseForm(IN struct ADAPTER
				    *prAdapter, int32_t au4Input)
{

	struct PARAM_MCR_RW_STRUCT rMcrRdInfo;
	int32_t au4PdSlope, au4TxPwrOffset, au4TxPwrOffset_Round;
	int8_t auTxPwrOffset_Round;

	rMcrRdInfo.u4McrOffset = 0x60205c68;
	rMcrRdInfo.u4McrData = 0;
	au4TxPwrOffset = au4Input;
	wlanQueryPdMcr(prAdapter, &rMcrRdInfo);

	au4PdSlope = (rMcrRdInfo.u4McrData) & BITS(0, 6);
	au4TxPwrOffset_Round = wlanIntRound((au4TxPwrOffset *
					     au4PdSlope)) / 10;

	au4TxPwrOffset_Round = -au4TxPwrOffset_Round;

	if (au4TxPwrOffset_Round < -128)
		au4TxPwrOffset_Round = 128;
	else if (au4TxPwrOffset_Round < 0)
		au4TxPwrOffset_Round += 256;
	else if (au4TxPwrOffset_Round > 127)
		au4TxPwrOffset_Round = 127;

	auTxPwrOffset_Round = (uint8_t) au4TxPwrOffset_Round;

	return au4TxPwrOffset_Round;
}

#endif

#if CFG_SUPPORT_NVRAM_5G
uint32_t wlanLoadManufactureData_5G(IN struct ADAPTER
				    *prAdapter, IN struct REG_INFO *prRegInfo)
{

	struct BANDEDGE_5G *pr5GBandEdge;

	ASSERT(prAdapter);

	pr5GBandEdge =
		&prRegInfo->prOldEfuseMapping->r5GBandEdgePwr;

	/* 1. set band edge tx power if available */
	if (pr5GBandEdge->uc5GBandEdgePwrUsed != 0) {
		struct CMD_EDGE_TXPWR_LIMIT rCmdEdgeTxPwrLimit;

		rCmdEdgeTxPwrLimit.cBandEdgeMaxPwrCCK = 0;
		rCmdEdgeTxPwrLimit.cBandEdgeMaxPwrOFDM20 =
			pr5GBandEdge->c5GBandEdgeMaxPwrOFDM20;
		rCmdEdgeTxPwrLimit.cBandEdgeMaxPwrOFDM40 =
			pr5GBandEdge->c5GBandEdgeMaxPwrOFDM40;
		rCmdEdgeTxPwrLimit.cBandEdgeMaxPwrOFDM80 =
			pr5GBandEdge->c5GBandEdgeMaxPwrOFDM80;

		wlanSendSetQueryCmd(prAdapter,
				    CMD_ID_SET_EDGE_TXPWR_LIMIT_5G,
				    TRUE,
				    FALSE,
				    FALSE,
				    NULL,
				    NULL, sizeof(struct CMD_EDGE_TXPWR_LIMIT),
				    (uint8_t *) &rCmdEdgeTxPwrLimit, NULL, 0);

		/* dumpMemory8(&rCmdEdgeTxPwrLimit,4); */
	}

	/*2.set channel offset for 8 sub-band */
	if (prRegInfo->prOldEfuseMapping->uc5GChannelOffsetVaild) {
		struct CMD_POWER_OFFSET rCmdPowerOffset;
		uint8_t i;

		rCmdPowerOffset.ucBand = BAND_5G;
		for (i = 0; i < MAX_SUBBAND_NUM_5G; i++)
			rCmdPowerOffset.ucSubBandOffset[i] =
				prRegInfo->prOldEfuseMapping->auc5GChOffset[i];

		wlanSendSetQueryCmd(prAdapter,
				    CMD_ID_SET_CHANNEL_PWR_OFFSET,
				    TRUE,
				    FALSE,
				    FALSE, NULL, NULL, sizeof(rCmdPowerOffset),
				    (uint8_t *) &rCmdPowerOffset, NULL, 0);
		/* dumpMemory8(&rCmdPowerOffset,9); */
	}

	/*3.set 5G AC power */
	if (prRegInfo->prOldEfuseMapping->uc11AcTxPwrValid) {

		struct CMD_TX_AC_PWR rCmdAcPwr;

		kalMemCopy(&rCmdAcPwr.rAcPwr,
			   &prRegInfo->prOldEfuseMapping->r11AcTxPwr,
			   sizeof(struct AC_PWR_SETTING_STRUCT));
		rCmdAcPwr.ucBand = BAND_5G;

		wlanSendSetQueryCmd(prAdapter, CMD_ID_SET_80211AC_TX_PWR,
				    TRUE, FALSE, FALSE, NULL, NULL,
				    sizeof(struct CMD_TX_AC_PWR),
				    (uint8_t *) &rCmdAcPwr, NULL, 0);
		/* dumpMemory8(&rCmdAcPwr,9); */
	}

	return WLAN_STATUS_SUCCESS;
}
#endif
/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called to load manufacture data from NVRAM
 * if available and valid
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 * @param prRegInfo      Pointer of REG_INFO_T
 *
 * @return WLAN_STATUS_SUCCESS
 *         WLAN_STATUS_FAILURE
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanLoadManufactureData(IN struct ADAPTER
				 *prAdapter, IN struct REG_INFO *prRegInfo)
{
#if CFG_SUPPORT_RDD_TEST_MODE
	struct CMD_RDD_CH rRddParam;
#endif
	struct CMD_NVRAM_SETTING rCmdNvramSettings;

	ASSERT(prAdapter);

	/* 1. Version Check */
	if (prAdapter->prGlueInfo->fgNvramAvailable == TRUE) {
		prAdapter->rVerInfo.u2Part1CfgOwnVersion =
			prRegInfo->prNvramSettings->u2Part1OwnVersion;
		prAdapter->rVerInfo.u2Part1CfgPeerVersion =
			prRegInfo->prNvramSettings->u2Part1PeerVersion;
		prAdapter->rVerInfo.u2Part2CfgOwnVersion =
			prRegInfo->prNvramSettings->u2Part2OwnVersion;
		prAdapter->rVerInfo.u2Part2CfgPeerVersion =
			prRegInfo->prNvramSettings->u2Part2PeerVersion;
	}

#if (CFG_SW_NVRAM_VERSION_CHECK == 1)
	if (prAdapter->rVerInfo.u2Part1CfgPeerVersion >
	    CFG_DRV_OWN_VERSION
	    || prAdapter->rVerInfo.u2Part2CfgPeerVersion >
	    CFG_DRV_OWN_VERSION
	    || prAdapter->rVerInfo.u2Part1CfgOwnVersion <
	    CFG_DRV_PEER_VERSION
	    || prAdapter->rVerInfo.u2Part2CfgOwnVersion <
	    CFG_DRV_PEER_VERSION) {
		return WLAN_STATUS_FAILURE;
	}
#endif

	/* MT6620 E1/E2 would be ignored directly */
	if (prAdapter->rVerInfo.u2Part1CfgOwnVersion == 0x0001) {
		prRegInfo->ucTxPwrValid = 1;
	} else {
		/* 2. Load TX power gain parameters if valid */
		if (prRegInfo->ucTxPwrValid != 0) {
			/* send to F/W */

			nicUpdateTxPower(prAdapter,
				(struct CMD_TX_PWR *) (&(prRegInfo->rTxPwr)));
		}
	}

	/* Todo : Temp Open 20150806 Sam */
	prRegInfo->ucEnable5GBand = 1;
	prRegInfo->ucSupport5GBand = 1;

	/* 3. Check if needs to support 5GHz */
	if (prRegInfo->ucEnable5GBand) {
#if CFG_SUPPORT_NVRAM_5G
		wlanLoadManufactureData_5G(prAdapter, prRegInfo);
#endif
		/* check if it is disabled by hardware */
		if (prAdapter->fgIsHw5GBandDisabled
		    || prRegInfo->ucSupport5GBand == 0)
			prAdapter->fgEnable5GBand = FALSE;
		else
			prAdapter->fgEnable5GBand = TRUE;
	} else
		prAdapter->fgEnable5GBand = FALSE;

	/* 4. Send EFUSE data */
#if CFG_SUPPORT_NVRAM_5G
	/* If NvRAM read failed, this pointer will be NULL */
	if (prRegInfo->prOldEfuseMapping) {
		/*2.set channel offset for 3 sub-band */
		if (prRegInfo->prOldEfuseMapping->ucChannelOffsetVaild) {
			struct CMD_POWER_OFFSET rCmdPowerOffset;
			uint8_t i;

			rCmdPowerOffset.ucBand = BAND_2G4;
			for (i = 0; i < 3; i++)
				rCmdPowerOffset.ucSubBandOffset[i] =
					prRegInfo->prOldEfuseMapping
					->aucChOffset[i];
			rCmdPowerOffset.ucSubBandOffset[i] =
				prRegInfo->prOldEfuseMapping
				->acAllChannelOffset;

			wlanSendSetQueryCmd(prAdapter,
					    CMD_ID_SET_CHANNEL_PWR_OFFSET,
					    TRUE, FALSE, FALSE, NULL, NULL,
					    sizeof(rCmdPowerOffset),
					    (uint8_t *) &rCmdPowerOffset, NULL,
					    0);
			/* dumpMemory8(&rCmdPowerOffset,9); */
		}
	}
#else

	wlanSendSetQueryCmd(prAdapter,
			    CMD_ID_SET_PHY_PARAM,
			    TRUE,
			    FALSE,
			    FALSE, NULL, NULL, sizeof(struct CMD_PHY_PARAM),
			    (uint8_t *) (prRegInfo->aucEFUSE), NULL, 0);

#endif
	/*RSSI path compasation */
	if (prRegInfo->ucRssiPathCompasationUsed) {
		struct CMD_RSSI_PATH_COMPASATION rCmdRssiPathCompasation;

		rCmdRssiPathCompasation.c2GRssiCompensation =
			prRegInfo->rRssiPathCompasation.c2GRssiCompensation;
		rCmdRssiPathCompasation.c5GRssiCompensation =
			prRegInfo->rRssiPathCompasation.c5GRssiCompensation;

		wlanSendSetQueryCmd(prAdapter, CMD_ID_SET_PATH_COMPASATION,
				    TRUE, FALSE, FALSE, NULL, NULL,
				    sizeof(rCmdRssiPathCompasation),
				    (uint8_t *) &rCmdRssiPathCompasation,
				    NULL, 0);
	}
#if CFG_SUPPORT_RDD_TEST_MODE
	rRddParam.ucRddTestMode = (uint8_t)
				  prRegInfo->u4RddTestMode;
	rRddParam.ucRddShutCh = (uint8_t) prRegInfo->u4RddShutFreq;
	rRddParam.ucRddStartCh = (uint8_t) nicFreq2ChannelNum(
					 prRegInfo->u4RddStartFreq);
	rRddParam.ucRddStopCh = (uint8_t) nicFreq2ChannelNum(
					prRegInfo->u4RddStopFreq);
	rRddParam.ucRddDfs = (uint8_t) prRegInfo->u4RddDfs;
	prAdapter->ucRddStatus = 0;
	nicUpdateRddTestMode(prAdapter,
			     (struct CMD_RDD_CH *) (&rRddParam));
#endif

	/* 5. Get 16-bits Country Code and Bandwidth */
	prAdapter->rWifiVar.rConnSettings.u2CountryCode =
		(((uint16_t) prRegInfo->au2CountryCode[0]) << 8) | (((
			uint16_t) prRegInfo->au2CountryCode[1]) & BITS(0, 7));

#if 0	/* Bandwidth control will be controlled by GUI. 20110930
	 * So ignore the setting from registry/NVRAM
	 */
	prAdapter->rWifiVar.rConnSettings.uc2G4BandwidthMode =
		prRegInfo->uc2G4BwFixed20M ? CONFIG_BW_20M :
		CONFIG_BW_20_40M;
	prAdapter->rWifiVar.rConnSettings.uc5GBandwidthMode =
		prRegInfo->uc5GBwFixed20M ? CONFIG_BW_20M :
		CONFIG_BW_20_40M;
#endif

	/* 6. Set domain and channel information to chip */
	rlmDomainSendCmd(prAdapter);

	/* Update supported channel list in channel table */
	wlanUpdateChannelTable(prAdapter->prGlueInfo);


	/* 7. set band edge tx power if available */
	if (prRegInfo->fg2G4BandEdgePwrUsed) {
		struct CMD_EDGE_TXPWR_LIMIT rCmdEdgeTxPwrLimit;

		rCmdEdgeTxPwrLimit.cBandEdgeMaxPwrCCK =
			prRegInfo->cBandEdgeMaxPwrCCK;
		rCmdEdgeTxPwrLimit.cBandEdgeMaxPwrOFDM20 =
			prRegInfo->cBandEdgeMaxPwrOFDM20;
		rCmdEdgeTxPwrLimit.cBandEdgeMaxPwrOFDM40 =
			prRegInfo->cBandEdgeMaxPwrOFDM40;

		wlanSendSetQueryCmd(prAdapter,
				    CMD_ID_SET_EDGE_TXPWR_LIMIT,
				    TRUE,
				    FALSE,
				    FALSE,
				    NULL,
				    NULL, sizeof(struct CMD_EDGE_TXPWR_LIMIT),
				    (uint8_t *) &rCmdEdgeTxPwrLimit, NULL, 0);
	}
	/*8. Set 2.4G AC power */
	if (prRegInfo->prOldEfuseMapping
	    && prRegInfo->prOldEfuseMapping->uc11AcTxPwrValid2G) {

		struct CMD_TX_AC_PWR rCmdAcPwr;

		kalMemCopy(&rCmdAcPwr.rAcPwr,
			   &prRegInfo->prOldEfuseMapping->r11AcTxPwr2G,
			   sizeof(struct AC_PWR_SETTING_STRUCT));
		rCmdAcPwr.ucBand = BAND_2G4;

		wlanSendSetQueryCmd(prAdapter, CMD_ID_SET_80211AC_TX_PWR,
				    TRUE, FALSE, FALSE, NULL, NULL,
				    sizeof(struct CMD_TX_AC_PWR),
				    (uint8_t *) &rCmdAcPwr, NULL, 0);
		/* dumpMemory8(&rCmdAcPwr,9); */
	}
	/* 9. Send the full Parameters of NVRAM to FW */

	kalMemCopy(&rCmdNvramSettings.rNvramSettings,
		   &prRegInfo->prNvramSettings->u2Part1OwnVersion,
		   sizeof(struct WIFI_CFG_PARAM_STRUCT));
	ASSERT(sizeof(struct WIFI_CFG_PARAM_STRUCT) == 512);
	wlanSendSetQueryCmd(prAdapter,
			    CMD_ID_SET_NVRAM_SETTINGS,
			    TRUE,
			    FALSE,
			    FALSE, NULL, NULL, sizeof(rCmdNvramSettings),
			    (uint8_t *) &rCmdNvramSettings, NULL, 0);

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called to check
 *        Media Stream Mode is set to non-default value or not,
 *        and clear to default value if above criteria is met
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 *
 * @return TRUE
 *           The media stream mode was non-default value and has been reset
 *         FALSE
 *           The media stream mode is default value
 */
/*----------------------------------------------------------------------------*/
u_int8_t wlanResetMediaStreamMode(IN struct ADAPTER
				  *prAdapter)
{
	ASSERT(prAdapter);

	if (prAdapter->rWlanInfo.eLinkAttr.ucMediaStreamMode != 0) {
		prAdapter->rWlanInfo.eLinkAttr.ucMediaStreamMode = 0;

		return TRUE;
	} else {
		return FALSE;
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called to check if any pending timer has expired
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 *
 * @return WLAN_STATUS_SUCCESS
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanTimerTimeoutCheck(IN struct ADAPTER *prAdapter)
{
	ASSERT(prAdapter);

	cnmTimerDoTimeOutCheck(prAdapter);

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called to check if any pending mailbox message
 *        to be handled
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 *
 * @return WLAN_STATUS_SUCCESS
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanProcessMboxMessage(IN struct ADAPTER
				*prAdapter)
{
	uint32_t i;

	ASSERT(prAdapter);

	for (i = 0; i < MBOX_ID_TOTAL_NUM; i++)
		mboxRcvAllMsg(prAdapter, (enum ENUM_MBOX_ID) i);

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called to enqueue a single TX packet into CORE
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 *        prNativePacket Pointer of Native Packet
 *
 * @return WLAN_STATUS_SUCCESS
 *         WLAN_STATUS_RESOURCES
 *         WLAN_STATUS_INVALID_PACKET
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanEnqueueTxPacket(IN struct ADAPTER *prAdapter,
			     IN void *prNativePacket)
{
	struct TX_CTRL *prTxCtrl;
	struct MSDU_INFO *prMsduInfo;

	ASSERT(prAdapter);

	prTxCtrl = &prAdapter->rTxCtrl;

	prMsduInfo = cnmPktAlloc(prAdapter, 0);

	if (!prMsduInfo)
		return WLAN_STATUS_RESOURCES;

	if (nicTxFillMsduInfo(prAdapter, prMsduInfo,
			      prNativePacket)) {
		/* prMsduInfo->eSrc = TX_PACKET_OS; */

		/* Tx profiling */
		wlanTxProfilingTagMsdu(prAdapter, prMsduInfo,
				       TX_PROF_TAG_DRV_ENQUE);

		/* enqueue to QM */
		nicTxEnqueueMsdu(prAdapter, prMsduInfo);

		return WLAN_STATUS_SUCCESS;
	}
	kalSendComplete(prAdapter->prGlueInfo, prNativePacket,
			WLAN_STATUS_INVALID_PACKET);

	nicTxReturnMsduInfo(prAdapter, prMsduInfo);

	return WLAN_STATUS_INVALID_PACKET;

}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called to flush pending TX packets in CORE
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 *
 * @return WLAN_STATUS_SUCCESS
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanFlushTxPendingPackets(IN struct ADAPTER *prAdapter)
{
	ASSERT(prAdapter);

	return nicTxFlush(prAdapter);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief this function sends pending MSDU_INFO_T to MT6620
 *
 * @param prAdapter      Pointer to the Adapter structure.
 * @param pfgHwAccess    Pointer for tracking LP-OWN status
 *
 * @retval WLAN_STATUS_SUCCESS   Reset is done successfully.
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanTxPendingPackets(IN struct ADAPTER *prAdapter,
			      IN OUT u_int8_t *pfgHwAccess)
{
	struct TX_CTRL *prTxCtrl;
	struct MSDU_INFO *prMsduInfo;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);
	prTxCtrl = &prAdapter->rTxCtrl;

#if !CFG_SUPPORT_MULTITHREAD
	ASSERT(pfgHwAccess);
#endif

	/* <1> dequeue packet by txDequeuTxPackets() */
#if CFG_SUPPORT_MULTITHREAD
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_QM_TX_QUEUE);
	prMsduInfo = qmDequeueTxPacketsMthread(prAdapter,
					       &prTxCtrl->rTc);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_QM_TX_QUEUE);
#else
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_QM_TX_QUEUE);
	prMsduInfo = qmDequeueTxPackets(prAdapter, &prTxCtrl->rTc);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_QM_TX_QUEUE);
#endif
	if (prMsduInfo != NULL) {
		if (kalIsCardRemoved(prAdapter->prGlueInfo) == FALSE) {
#if !CFG_SUPPORT_MULTITHREAD
			/* <2> Acquire LP-OWN if necessary */
			if (*pfgHwAccess == FALSE) {
				*pfgHwAccess = TRUE;

				wlanAcquirePowerControl(prAdapter);
			}
#endif
			/* <3> send packets */
#if CFG_SUPPORT_MULTITHREAD
			nicTxMsduInfoListMthread(prAdapter, prMsduInfo);
#else
			nicTxMsduInfoList(prAdapter, prMsduInfo);
#endif
			/* <4> update TC by txAdjustTcQuotas() */
			nicTxAdjustTcq(prAdapter);
		} else
			wlanProcessQueuedMsduInfo(prAdapter, prMsduInfo);
	}

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called to acquire power control from firmware
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 *
 * @return WLAN_STATUS_SUCCESS
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanAcquirePowerControl(IN struct ADAPTER *prAdapter)
{
	ASSERT(prAdapter);

	/* DBGLOG(INIT, INFO, ("Acquire Power Ctrl\n")); */

	ACQUIRE_POWER_CONTROL_FROM_PM(prAdapter);

	/* Reset sleepy state */
	if (prAdapter->fgWiFiInSleepyState == TRUE)
		prAdapter->fgWiFiInSleepyState = FALSE;

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called to release power control to firmware
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 *
 * @return WLAN_STATUS_SUCCESS
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanReleasePowerControl(IN struct ADAPTER *prAdapter)
{
	ASSERT(prAdapter);

	/* DBGLOG(INIT, INFO, ("Release Power Ctrl\n")); */

	RECLAIM_POWER_CONTROL_TO_PM(prAdapter, FALSE);

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called to report currently pending TX frames count
 *        (command packets are not included)
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 *
 * @return number of pending TX frames
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanGetTxPendingFrameCount(IN struct ADAPTER *prAdapter)
{
	struct TX_CTRL *prTxCtrl;
	uint32_t u4Num;

	ASSERT(prAdapter);
	prTxCtrl = &prAdapter->rTxCtrl;

	u4Num = kalGetTxPendingFrameCount(prAdapter->prGlueInfo) +
		(uint32_t) GLUE_GET_REF_CNT(
			prTxCtrl->i4PendingFwdFrameCount);

	return u4Num;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is to report current ACPI state
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 *
 * @return ACPI_STATE_D0 Normal Operation Mode
 *         ACPI_STATE_D3 Suspend Mode
 */
/*----------------------------------------------------------------------------*/
enum ENUM_ACPI_STATE wlanGetAcpiState(IN struct ADAPTER *prAdapter)
{
	ASSERT(prAdapter);

	return prAdapter->rAcpiState;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is to update current ACPI state only
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 * @param ePowerState    ACPI_STATE_D0 Normal Operation Mode
 *                       ACPI_STATE_D3 Suspend Mode
 *
 * @return none
 */
/*----------------------------------------------------------------------------*/
void wlanSetAcpiState(IN struct ADAPTER *prAdapter,
		      IN enum ENUM_ACPI_STATE ePowerState)
{
	ASSERT(prAdapter);
	ASSERT(ePowerState <= ACPI_STATE_D3);

	prAdapter->rAcpiState = ePowerState;

}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is to query ECO version from HIFSYS CR
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 *
 * @return zero      Unable to retrieve ECO version information
 *         non-zero  ECO version (1-based)
 */
/*----------------------------------------------------------------------------*/
uint8_t wlanGetEcoVersion(IN struct ADAPTER *prAdapter)
{
	uint8_t ucEcoVersion;

	ASSERT(prAdapter);

#if CFG_MULTI_ECOVER_SUPPORT
	ucEcoVersion = nicGetChipEcoVer(prAdapter);
	DBGLOG(INIT, TRACE, "%s: %u\n", __func__, ucEcoVersion);
	return ucEcoVersion;
#else
	if (nicVerifyChipID(prAdapter) == TRUE)
		return prAdapter->ucRevID + 1;
	else
		return 0;
#endif
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is to query ROM version from HIFSYS CR
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 *
 * @return zero      Unable to retrieve ROM version information
 *         non-zero  ROM version (1-based)
 */
/*----------------------------------------------------------------------------*/
uint8_t wlanGetRomVersion(IN struct ADAPTER *prAdapter)
{
	uint8_t ucRomVersion;

	ASSERT(prAdapter);

	ucRomVersion = nicGetChipSwVer();
	DBGLOG(INIT, TRACE, "%s: %u\n", __func__, ucRomVersion);
	return ucRomVersion;

}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is to setting the default Tx Power configuration
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 *
 * @return zero      Unable to retrieve ECO version information
 *         non-zero  ECO version (1-based)
 */
/*----------------------------------------------------------------------------*/
void wlanDefTxPowerCfg(IN struct ADAPTER *prAdapter)
{
	uint8_t i;
	struct GLUE_INFO *prGlueInfo = prAdapter->prGlueInfo;
	struct SET_TXPWR_CTRL *prTxpwr;

	ASSERT(prGlueInfo);

	prTxpwr = &prGlueInfo->rTxPwr;

	prTxpwr->c2GLegacyStaPwrOffset = 0;
	prTxpwr->c2GHotspotPwrOffset = 0;
	prTxpwr->c2GP2pPwrOffset = 0;
	prTxpwr->c2GBowPwrOffset = 0;
	prTxpwr->c5GLegacyStaPwrOffset = 0;
	prTxpwr->c5GHotspotPwrOffset = 0;
	prTxpwr->c5GP2pPwrOffset = 0;
	prTxpwr->c5GBowPwrOffset = 0;
	prTxpwr->ucConcurrencePolicy = 0;
	for (i = 0; i < 3; i++)
		prTxpwr->acReserved1[i] = 0;

	for (i = 0; i < 14; i++)
		prTxpwr->acTxPwrLimit2G[i] = 63;

	for (i = 0; i < 4; i++)
		prTxpwr->acTxPwrLimit5G[i] = 63;

	for (i = 0; i < 2; i++)
		prTxpwr->acReserved2[i] = 0;

}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is to
 *        set preferred band configuration corresponding to network type
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 * @param eBand          Given band
 * @param ucBssIndex     BSS Info Index
 *
 * @return none
 */
/*----------------------------------------------------------------------------*/
void wlanSetPreferBandByNetwork(IN struct ADAPTER *prAdapter,
				IN enum ENUM_BAND eBand, IN uint8_t ucBssIndex)
{
	ASSERT(prAdapter);
	ASSERT(eBand <= BAND_NUM);
	ASSERT(ucBssIndex <= prAdapter->ucHwBssIdNum);


	/* 1. set prefer band according to network type */
	prAdapter->aePreferBand[ucBssIndex] = eBand;

	/* 2. remove buffered BSS descriptors correspondingly */
	if (eBand == BAND_2G4)
		scanRemoveBssDescByBandAndNetwork(prAdapter, BAND_5G,
						  ucBssIndex);
	else if (eBand == BAND_5G)
		scanRemoveBssDescByBandAndNetwork(prAdapter, BAND_2G4,
						  ucBssIndex);

}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is to
 *        get channel information corresponding to specified network type
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 * @param ucBssIndex     BSS Info Index
 *
 * @return channel number
 */
/*----------------------------------------------------------------------------*/
uint8_t wlanGetChannelNumberByNetwork(IN struct ADAPTER
				      *prAdapter, IN uint8_t ucBssIndex)
{
	struct BSS_INFO *prBssInfo;

	ASSERT(prAdapter);
	ASSERT(ucBssIndex <= prAdapter->ucHwBssIdNum);

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

	return prBssInfo->ucPrimaryChannel;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is to
 *        check unconfigured system properties and generate related message on
 *        scan list to notify users
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 *
 * @return WLAN_STATUS_SUCCESS
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanCheckSystemConfiguration(IN struct ADAPTER
				      *prAdapter)
{
#if (CFG_NVRAM_EXISTENCE_CHECK == 1) || (CFG_SW_NVRAM_VERSION_CHECK == 1)
	const uint8_t aucZeroMacAddr[] = NULL_MAC_ADDR;
	u_int8_t fgIsConfExist = TRUE;
	u_int8_t fgGenErrMsg = FALSE;
	struct REG_INFO *prRegInfo = NULL;
#if 0
	const uint8_t aucBCAddr[] = BC_MAC_ADDR;
	struct WLAN_BEACON_FRAME *prBeacon = NULL;
	struct IE_SSID *prSsid = NULL;
	uint32_t u4ErrCode = 0;
	uint8_t aucErrMsg[32];
	struct PARAM_SSID rSsid;
	struct PARAM_802_11_CONFIG rConfiguration;
	uint8_t rSupportedRates[PARAM_MAX_LEN_RATES_EX];
#endif
#endif

	DEBUGFUNC("wlanCheckSystemConfiguration");

	ASSERT(prAdapter);

#if (CFG_NVRAM_EXISTENCE_CHECK == 1)
	if (kalIsConfigurationExist(prAdapter->prGlueInfo) ==
	    FALSE) {
		fgIsConfExist = FALSE;
		fgGenErrMsg = TRUE;
	}
#endif

#if (CFG_SW_NVRAM_VERSION_CHECK == 1)
	prRegInfo = kalGetConfiguration(prAdapter->prGlueInfo);

#if (CFG_SUPPORT_PWR_LIMIT_COUNTRY == 1)
	if (fgIsConfExist == TRUE
	    && (prAdapter->rVerInfo.u2Part1CfgPeerVersion >
		CFG_DRV_OWN_VERSION
		|| prAdapter->rVerInfo.u2Part2CfgPeerVersion >
		CFG_DRV_OWN_VERSION
		|| prAdapter->rVerInfo.u2Part1CfgOwnVersion <
		CFG_DRV_PEER_VERSION
		|| prAdapter->rVerInfo.u2Part2CfgOwnVersion <
		CFG_DRV_PEER_VERSION/* NVRAM */
		|| prAdapter->rVerInfo.u2FwPeerVersion > CFG_DRV_OWN_VERSION
		|| prAdapter->rVerInfo.u2FwOwnVersion < CFG_DRV_PEER_VERSION
		|| (prAdapter->fgIsEmbbededMacAddrValid == FALSE &&
		    (IS_BMCAST_MAC_ADDR(prRegInfo->aucMacAddr)
		     || EQUAL_MAC_ADDR(aucZeroMacAddr, prRegInfo->aucMacAddr)))
		|| prRegInfo->ucTxPwrValid == 0
		|| prAdapter->fgIsPowerLimitTableValid == FALSE))
		fgGenErrMsg = TRUE;
#else
	if (fgIsConfExist == TRUE
	    && (prAdapter->rVerInfo.u2Part1CfgPeerVersion >
		CFG_DRV_OWN_VERSION
		|| prAdapter->rVerInfo.u2Part2CfgPeerVersion >
		CFG_DRV_OWN_VERSION
		|| prAdapter->rVerInfo.u2Part1CfgOwnVersion <
		CFG_DRV_PEER_VERSION
		|| prAdapter->rVerInfo.u2Part2CfgOwnVersion <
		CFG_DRV_PEER_VERSION/* NVRAM */
		|| prAdapter->rVerInfo.u2FwPeerVersion > CFG_DRV_OWN_VERSION
		|| prAdapter->rVerInfo.u2FwOwnVersion < CFG_DRV_PEER_VERSION
		|| (prAdapter->fgIsEmbbededMacAddrValid == FALSE &&
		    (IS_BMCAST_MAC_ADDR(prRegInfo->aucMacAddr)
		     || EQUAL_MAC_ADDR(aucZeroMacAddr, prRegInfo->aucMacAddr)))
		|| prRegInfo->ucTxPwrValid == 0))
		fgGenErrMsg = TRUE;
#endif
#endif
#if 0/* remove NVRAM WARNING in scan result */
	if (fgGenErrMsg == TRUE) {
		prBeacon = cnmMemAlloc(prAdapter, RAM_TYPE_BUF,
				       sizeof(struct WLAN_BEACON_FRAME) +
				       sizeof(struct IE_SSID));

		/* initialization */
		kalMemZero(prBeacon, sizeof(struct WLAN_BEACON_FRAME) +
			   sizeof(struct IE_SSID));

		/* prBeacon initialization */
		prBeacon->u2FrameCtrl = MAC_FRAME_BEACON;
		COPY_MAC_ADDR(prBeacon->aucDestAddr, aucBCAddr);
		COPY_MAC_ADDR(prBeacon->aucSrcAddr, aucZeroMacAddr);
		COPY_MAC_ADDR(prBeacon->aucBSSID, aucZeroMacAddr);
		prBeacon->u2BeaconInterval = 100;
		prBeacon->u2CapInfo = CAP_INFO_ESS;

		/* prSSID initialization */
		prSsid = (struct IE_SSID *) (&prBeacon->aucInfoElem[0]);
		prSsid->ucId = ELEM_ID_SSID;

		/* rConfiguration initialization */
		rConfiguration.u4Length = sizeof(struct
						 PARAM_802_11_CONFIG);
		rConfiguration.u4BeaconPeriod = 100;
		rConfiguration.u4ATIMWindow = 1;
		rConfiguration.u4DSConfig = 2412;
		rConfiguration.rFHConfig.u4Length = sizeof(
				struct PARAM_802_11_CONFIG_FH);

		/* rSupportedRates initialization */
		kalMemZero(rSupportedRates,
			   (sizeof(uint8_t) * PARAM_MAX_LEN_RATES_EX));
	}
#if (CFG_NVRAM_EXISTENCE_CHECK == 1)
#define NVRAM_ERR_MSG "NVRAM WARNING: Err = 0x01"
	if (kalIsConfigurationExist(prAdapter->prGlueInfo) ==
	    FALSE) {
		COPY_SSID(prSsid->aucSSID, prSsid->ucLength, NVRAM_ERR_MSG,
			  (uint8_t) (strlen(NVRAM_ERR_MSG)));

		kalIndicateBssInfo(prAdapter->prGlueInfo,
				   (uint8_t *) prBeacon,
				   OFFSET_OF(struct WLAN_BEACON_FRAME,
					     aucInfoElem) + OFFSET_OF(
					     struct IE_SSID, aucSSID) +
					     prSsid->ucLength, 1, 0);

		COPY_SSID(rSsid.aucSsid, rSsid.u4SsidLen, NVRAM_ERR_MSG,
			  strlen(NVRAM_ERR_MSG));
		nicAddScanResult(prAdapter,
				 prBeacon->aucBSSID,
				 &rSsid,
				 0,
				 0,
				 PARAM_NETWORK_TYPE_FH,
				 &rConfiguration,
				 NET_TYPE_INFRA,
				 rSupportedRates,
				 OFFSET_OF(struct WLAN_BEACON_FRAME,
					aucInfoElem) + OFFSET_OF(
					struct IE_SSID, aucSSID) +
					prSsid->ucLength -
					WLAN_MAC_MGMT_HEADER_LEN,
				 (uint8_t *) ((unsigned long) (prBeacon) +
					WLAN_MAC_MGMT_HEADER_LEN));
	}
#endif

#if (CFG_SW_NVRAM_VERSION_CHECK == 1)
#define VER_ERR_MSG     "NVRAM WARNING: Err = 0x%02X"
	if (fgIsConfExist == TRUE) {
		if ((prAdapter->rVerInfo.u2Part1CfgPeerVersion >
		     CFG_DRV_OWN_VERSION
		     || prAdapter->rVerInfo.u2Part2CfgPeerVersion >
		     CFG_DRV_OWN_VERSION
		     || prAdapter->rVerInfo.u2Part1CfgOwnVersion <
		     CFG_DRV_PEER_VERSION
		     || prAdapter->rVerInfo.u2Part2CfgOwnVersion <
		     CFG_DRV_PEER_VERSION	/* NVRAM */
		     || prAdapter->rVerInfo.u2FwPeerVersion >
			CFG_DRV_OWN_VERSION
		     || prAdapter->rVerInfo.u2FwOwnVersion <
		     CFG_DRV_PEER_VERSION))
			u4ErrCode |= NVRAM_ERROR_VERSION_MISMATCH;

		if (prRegInfo->ucTxPwrValid == 0)
			u4ErrCode |= NVRAM_ERROR_INVALID_TXPWR;

		if (prAdapter->fgIsEmbbededMacAddrValid == FALSE
		    && (IS_BMCAST_MAC_ADDR(prRegInfo->aucMacAddr)
			|| EQUAL_MAC_ADDR(aucZeroMacAddr,
					  prRegInfo->aucMacAddr))) {
			u4ErrCode |= NVRAM_ERROR_INVALID_MAC_ADDR;
		}
#if CFG_SUPPORT_PWR_LIMIT_COUNTRY
		if (prAdapter->fgIsPowerLimitTableValid == FALSE)
			u4ErrCode |= NVRAM_POWER_LIMIT_TABLE_INVALID;
#endif
		if (u4ErrCode != 0) {
			sprintf(aucErrMsg, VER_ERR_MSG,
				(unsigned int)u4ErrCode);
			COPY_SSID(prSsid->aucSSID, prSsid->ucLength, aucErrMsg,
				  (uint8_t) (strlen(aucErrMsg)));

			kalIndicateBssInfo(prAdapter->prGlueInfo,
					   (uint8_t *) prBeacon,
					   OFFSET_OF(struct WLAN_BEACON_FRAME,
						     aucInfoElem) + OFFSET_OF(
						     struct IE_SSID, aucSSID) +
						     prSsid->ucLength, 1, 0);

			COPY_SSID(rSsid.aucSsid, rSsid.u4SsidLen, NVRAM_ERR_MSG,
				  strlen(NVRAM_ERR_MSG));
			nicAddScanResult(prAdapter, prBeacon->aucBSSID, &rSsid,
					 0, 0, PARAM_NETWORK_TYPE_FH,
					 &rConfiguration, NET_TYPE_INFRA,
					 rSupportedRates,
					 OFFSET_OF(struct WLAN_BEACON_FRAME,
						aucInfoElem) +
					 OFFSET_OF(struct IE_SSID,
						aucSSID) + prSsid->ucLength -
						WLAN_MAC_MGMT_HEADER_LEN,
					 (uint8_t *) ((unsigned long) (prBeacon)
						+ WLAN_MAC_MGMT_HEADER_LEN));
		}
	}
#endif

	if (fgGenErrMsg == TRUE)
		cnmMemFree(prAdapter, prBeacon);
#endif
	return WLAN_STATUS_SUCCESS;
}

uint32_t
wlanoidQueryBssStatistics(IN struct ADAPTER *prAdapter,
			  IN void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
			  OUT uint32_t *pu4QueryInfoLen)
{
	struct PARAM_GET_BSS_STATISTICS *prQueryBssStatistics;
	struct BSS_INFO *prBssInfo;
	struct STA_RECORD *prStaRec;
	uint32_t rResult = WLAN_STATUS_FAILURE;
	uint8_t ucBssIndex;
	enum ENUM_WMM_ACI eAci;

	DEBUGFUNC("wlanoidQueryBssStatistics");

	do {
		ASSERT(pvQueryBuffer);

		/* 4 1. Sanity test */
		if ((prAdapter == NULL) || (pu4QueryInfoLen == NULL))
			break;

		if ((u4QueryBufferLen) && (pvQueryBuffer == NULL))
			break;

		if (u4QueryBufferLen <
		    sizeof(struct PARAM_GET_BSS_STATISTICS *)) {
			*pu4QueryInfoLen =
				sizeof(struct PARAM_GET_BSS_STATISTICS *);
			rResult = WLAN_STATUS_BUFFER_TOO_SHORT;
			break;
		}

		prQueryBssStatistics = (struct PARAM_GET_BSS_STATISTICS *)
				       pvQueryBuffer;
		*pu4QueryInfoLen = sizeof(struct PARAM_GET_BSS_STATISTICS);

		ucBssIndex = prQueryBssStatistics->ucBssIndex;
		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

		if (prBssInfo) {	/*AIS*/
			if (prBssInfo->eCurrentOPMode ==
			    OP_MODE_INFRASTRUCTURE) {
			struct WIFI_WMM_AC_STAT *prQueryLss = NULL;
			struct WIFI_WMM_AC_STAT *prStaLss = NULL;
			struct WIFI_WMM_AC_STAT *prBssLss = NULL;

			prQueryLss = prQueryBssStatistics->arLinkStatistics;
			prBssLss = prBssInfo->arLinkStatistics;
			prStaRec = prBssInfo->prStaRecOfAP;
			if (prStaRec) {
				prStaLss = prStaRec->arLinkStatistics;
				for (eAci = 0;
				     eAci < WMM_AC_INDEX_NUM; eAci++) {
				prQueryLss[eAci].u4TxMsdu =
					prStaLss[eAci].u4TxMsdu;
				prQueryLss[eAci].u4RxMsdu =
					prStaLss[eAci].u4RxMsdu;
				prQueryLss[eAci].u4TxDropMsdu =
					prStaLss[eAci].u4TxDropMsdu +
					prBssLss[eAci].u4TxDropMsdu;
				prQueryLss[eAci].u4TxFailMsdu =
					prStaLss[eAci].u4TxFailMsdu;
				prQueryLss[eAci].u4TxRetryMsdu =
					prStaLss[eAci].u4TxRetryMsdu;
				}
			}
			}
			rResult = WLAN_STATUS_SUCCESS;

			/*P2P */
			/* TODO */

			/*BOW*/
			/* TODO */
		}

	} while (FALSE);

	return rResult;

}

void wlanDumpBssStatistics(IN struct ADAPTER *prAdapter,
			   uint8_t ucBssIdx)
{
	struct BSS_INFO *prBssInfo;
	struct STA_RECORD *prStaRec;
	enum ENUM_WMM_ACI eAci;
	struct WIFI_WMM_AC_STAT arLLStats[WMM_AC_INDEX_NUM];
	uint8_t ucIdx;

	if (ucBssIdx > prAdapter->ucHwBssIdNum) {
		DBGLOG(SW4, INFO, "Invalid BssInfo index[%u], skip dump!\n",
		       ucBssIdx);
		return;
	}

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIdx);
	if (!prBssInfo) {
		DBGLOG(SW4, INFO, "Invalid BssInfo index[%u], skip dump!\n",
		       ucBssIdx);
		return;
	}
	/* <1> fill per-BSS statistics */
#if 0
	/*AIS*/ if (prBssInfo->eCurrentOPMode ==
		    OP_MODE_INFRASTRUCTURE) {
		prStaRec = prBssInfo->prStaRecOfAP;
		if (prStaRec) {
			for (eAci = 0; eAci < WMM_AC_INDEX_NUM; eAci++) {
				prBssInfo->arLinkStatistics[eAci].u4TxMsdu
					= prStaRec->arLinkStatistics[eAci]
					  .u4TxMsdu;
				prBssInfo->arLinkStatistics[eAci].u4RxMsdu
					= prStaRec->arLinkStatistics[eAci]
					  .u4RxMsdu;
				prBssInfo->arLinkStatistics[eAci].u4TxDropMsdu
					+= prStaRec->arLinkStatistics[eAci]
					   .u4TxDropMsdu;
				prBssInfo->arLinkStatistics[eAci].u4TxFailMsdu
					= prStaRec->arLinkStatistics[eAci]
					  .u4TxFailMsdu;
				prBssInfo->arLinkStatistics[eAci].u4TxRetryMsdu
					= prStaRec->arLinkStatistics[eAci]
					  .u4TxRetryMsdu;
			}
		}
	}
#else
	for (eAci = 0; eAci < WMM_AC_INDEX_NUM; eAci++) {
		arLLStats[eAci].u4TxMsdu =
			prBssInfo->arLinkStatistics[eAci].u4TxMsdu;
		arLLStats[eAci].u4RxMsdu =
			prBssInfo->arLinkStatistics[eAci].u4RxMsdu;
		arLLStats[eAci].u4TxDropMsdu =
			prBssInfo->arLinkStatistics[eAci].u4TxDropMsdu;
		arLLStats[eAci].u4TxFailMsdu =
			prBssInfo->arLinkStatistics[eAci].u4TxFailMsdu;
		arLLStats[eAci].u4TxRetryMsdu =
			prBssInfo->arLinkStatistics[eAci].u4TxRetryMsdu;
	}

	for (ucIdx = 0; ucIdx < CFG_STA_REC_NUM; ucIdx++) {
		prStaRec = cnmGetStaRecByIndex(prAdapter, ucIdx);
		if (!prStaRec)
			continue;
		if (prStaRec->ucBssIndex != ucBssIdx)
			continue;
		/* now the valid sta_rec is valid */
		for (eAci = 0; eAci < WMM_AC_INDEX_NUM; eAci++) {
			arLLStats[eAci].u4TxMsdu +=
				prStaRec->arLinkStatistics[eAci].u4TxMsdu;
			arLLStats[eAci].u4RxMsdu +=
				prStaRec->arLinkStatistics[eAci].u4RxMsdu;
			arLLStats[eAci].u4TxDropMsdu +=
				prStaRec->arLinkStatistics[eAci].u4TxDropMsdu;
			arLLStats[eAci].u4TxFailMsdu +=
				prStaRec->arLinkStatistics[eAci].u4TxFailMsdu;
			arLLStats[eAci].u4TxRetryMsdu +=
				prStaRec->arLinkStatistics[eAci].u4TxRetryMsdu;
		}
	}
#endif

	/* <2>Dump BSS statistics */
	for (eAci = 0; eAci < WMM_AC_INDEX_NUM; eAci++) {
		DBGLOG(SW4, INFO,
		       "LLS BSS[%u] %s: T[%06u] R[%06u] T_D[%06u] T_F[%06u]\n",
		       prBssInfo->ucBssIndex, apucACI2Str[eAci],
		       arLLStats[eAci].u4TxMsdu,
		       arLLStats[eAci].u4RxMsdu, arLLStats[eAci].u4TxDropMsdu,
		       arLLStats[eAci].u4TxFailMsdu);
	}
}

void wlanDumpAllBssStatistics(IN struct ADAPTER *prAdapter)
{
	struct BSS_INFO *prBssInfo;
	/* ENUM_WMM_ACI_T eAci; */
	uint32_t ucIdx;

	/* wlanUpdateAllBssStatistics(prAdapter); */

	for (ucIdx = 0; ucIdx < prAdapter->ucHwBssIdNum; ucIdx++) {
		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucIdx);
		if (!IS_BSS_ACTIVE(prBssInfo)) {
			DBGLOG(SW4, TRACE,
			       "Invalid BssInfo index[%u], skip dump!\n",
			       ucIdx);
			continue;
		}

		wlanDumpBssStatistics(prAdapter, ucIdx);
	}
}

uint32_t
wlanoidQueryStaStatistics(IN struct ADAPTER *prAdapter,
			  IN void *pvQueryBuffer,
			  IN uint32_t u4QueryBufferLen,
			  OUT uint32_t *pu4QueryInfoLen)
{
	return wlanQueryStaStatistics(prAdapter, pvQueryBuffer,
				      u4QueryBufferLen,
				      pu4QueryInfoLen,
				      g_fgIsOid);
}

uint32_t
wlanQueryStaStatistics(IN struct ADAPTER *prAdapter,
		       IN void *pvQueryBuffer,
		       IN uint32_t u4QueryBufferLen,
		       OUT uint32_t *pu4QueryInfoLen,
		       u_int8_t fgIsOid)
{
	uint32_t rResult = WLAN_STATUS_FAILURE;
	struct STA_RECORD *prStaRec, *prTempStaRec;
	struct PARAM_GET_STA_STATISTICS *prQueryStaStatistics;
	uint8_t ucStaRecIdx;
	struct QUE_MGT *prQM;
	struct CMD_GET_STA_STATISTICS rQueryCmdStaStatistics;
	uint8_t ucIdx;
	enum ENUM_WMM_ACI eAci;

	DEBUGFUNC("wlanoidQueryStaStatistics");

	if (prAdapter == NULL)
		return WLAN_STATUS_FAILURE;
	prQM = &prAdapter->rQM;

	if (prAdapter->fgIsEnableLpdvt)
		return WLAN_STATUS_NOT_SUPPORTED;

	do {
		ASSERT(pvQueryBuffer);

		/* 4 1. Sanity test */
		if (pu4QueryInfoLen == NULL)
			break;

		if ((u4QueryBufferLen) && (pvQueryBuffer == NULL))
			break;

		if (u4QueryBufferLen <
		    sizeof(struct PARAM_GET_STA_STATISTICS)) {
			*pu4QueryInfoLen =
					sizeof(struct PARAM_GET_STA_STATISTICS);
			rResult = WLAN_STATUS_BUFFER_TOO_SHORT;
			break;
		}

		prQueryStaStatistics = (struct PARAM_GET_STA_STATISTICS *)
				       pvQueryBuffer;
		*pu4QueryInfoLen = sizeof(struct PARAM_GET_STA_STATISTICS);

		/* 4 5. Get driver global QM counter */
#if QM_ADAPTIVE_TC_RESOURCE_CTRL
		for (ucIdx = TC0_INDEX; ucIdx <= TC3_INDEX; ucIdx++) {
			prQueryStaStatistics->au4TcAverageQueLen[ucIdx] =
				prQM->au4AverageQueLen[ucIdx];
			prQueryStaStatistics->au4TcCurrentQueLen[ucIdx] =
				prQM->au4CurrentTcResource[ucIdx];
		}
#endif

		/* 4 2. Get StaRec by MAC address */
		prStaRec = NULL;

		for (ucStaRecIdx = 0; ucStaRecIdx < CFG_STA_REC_NUM;
		     ucStaRecIdx++) {
			prTempStaRec = &(prAdapter->arStaRec[ucStaRecIdx]);
			if (prTempStaRec->fgIsValid &&
			    prTempStaRec->fgIsInUse) {
				if (EQUAL_MAC_ADDR(prTempStaRec->aucMacAddr,
				    prQueryStaStatistics->aucMacAddr)) {
					prStaRec = prTempStaRec;
					break;
				}
			}
		}

		if (!prStaRec) {
			rResult = WLAN_STATUS_INVALID_DATA;
			break;
		}

		prQueryStaStatistics->u4Flag |= BIT(0);

#if CFG_ENABLE_PER_STA_STATISTICS
		/* 4 3. Get driver statistics */
		prQueryStaStatistics->u4TxTotalCount =
			prStaRec->u4TotalTxPktsNumber;
		prQueryStaStatistics->u4RxTotalCount =
			prStaRec->u4TotalRxPktsNumber;
		prQueryStaStatistics->u4TxExceedThresholdCount =
			prStaRec->u4ThresholdCounter;
		prQueryStaStatistics->u4TxMaxTime =
			prStaRec->u4MaxTxPktsTime;
		prQueryStaStatistics->u4TxMaxHifTime =
			prStaRec->u4MaxTxPktsHifTime;

		if (prStaRec->u4TotalTxPktsNumber) {
			prQueryStaStatistics->u4TxAverageProcessTime =
				(prStaRec->u4TotalTxPktsTime /
				 prStaRec->u4TotalTxPktsNumber);
			prQueryStaStatistics->u4TxAverageHifTime =
				prStaRec->u4TotalTxPktsHifTxTime /
				prStaRec->u4TotalTxPktsNumber;
		} else
			prQueryStaStatistics->u4TxAverageProcessTime = 0;

		/*link layer statistics */
		for (eAci = 0; eAci < WMM_AC_INDEX_NUM; eAci++) {
			prQueryStaStatistics->arLinkStatistics[eAci].u4TxMsdu =
				prStaRec->arLinkStatistics[eAci].u4TxMsdu;
			prQueryStaStatistics->arLinkStatistics[eAci].u4RxMsdu =
				prStaRec->arLinkStatistics[eAci].u4RxMsdu;
			prQueryStaStatistics->arLinkStatistics[
				eAci].u4TxDropMsdu =
				prStaRec->arLinkStatistics[eAci].u4TxDropMsdu;
		}

		for (ucIdx = TC0_INDEX; ucIdx <= TC3_INDEX; ucIdx++) {
			prQueryStaStatistics->au4TcResourceEmptyCount[ucIdx] =
				prQM->au4QmTcResourceEmptyCounter[
				prStaRec->ucBssIndex][ucIdx];
			/* Reset */
			prQM->au4QmTcResourceEmptyCounter[
				prStaRec->ucBssIndex][ucIdx] = 0;
			prQueryStaStatistics->au4TcResourceBackCount[ucIdx] =
				prQM->au4QmTcResourceBackCounter[ucIdx];
			prQM->au4QmTcResourceBackCounter[ucIdx] = 0;
			prQueryStaStatistics->au4DequeueNoTcResource[ucIdx]
				= prQM->au4DequeueNoTcResourceCounter[ucIdx];
			prQM->au4DequeueNoTcResourceCounter[ucIdx] = 0;
			prQueryStaStatistics->au4TcResourceUsedPageCount[ucIdx]
				= prQM->au4QmTcUsedPageCounter[ucIdx];
			prQM->au4QmTcUsedPageCounter[ucIdx] = 0;
			prQueryStaStatistics->au4TcResourceWantedPageCount[
				ucIdx] = prQM->au4QmTcWantedPageCounter[ucIdx];
			prQM->au4QmTcWantedPageCounter[ucIdx] = 0;
		}

		prQueryStaStatistics->u4EnqueueCounter =
			prQM->u4EnqueueCounter;
		prQueryStaStatistics->u4EnqueueStaCounter =
			prStaRec->u4EnqueueCounter;

		prQueryStaStatistics->u4DequeueCounter =
			prQM->u4DequeueCounter;
		prQueryStaStatistics->u4DequeueStaCounter =
			prStaRec->u4DeqeueuCounter;

		prQueryStaStatistics->IsrCnt =
			prAdapter->prGlueInfo->IsrCnt;
		prQueryStaStatistics->IsrPassCnt =
			prAdapter->prGlueInfo->IsrPassCnt;
		prQueryStaStatistics->TaskIsrCnt =
			prAdapter->prGlueInfo->TaskIsrCnt;

		prQueryStaStatistics->IsrAbnormalCnt =
			prAdapter->prGlueInfo->IsrAbnormalCnt;
		prQueryStaStatistics->IsrSoftWareCnt =
			prAdapter->prGlueInfo->IsrSoftWareCnt;
		prQueryStaStatistics->IsrRxCnt =
			prAdapter->prGlueInfo->IsrRxCnt;
		prQueryStaStatistics->IsrTxCnt =
			prAdapter->prGlueInfo->IsrTxCnt;

		/* 4 4.1 Reset statistics */
		if (prQueryStaStatistics->ucReadClear) {
			prStaRec->u4ThresholdCounter = 0;
			prStaRec->u4TotalTxPktsNumber = 0;
			prStaRec->u4TotalTxPktsHifTxTime = 0;

			prStaRec->u4TotalTxPktsTime = 0;
			prStaRec->u4TotalRxPktsNumber = 0;
			prStaRec->u4MaxTxPktsTime = 0;
			prStaRec->u4MaxTxPktsHifTime = 0;
			prQM->u4EnqueueCounter = 0;
			prQM->u4DequeueCounter = 0;
			prStaRec->u4EnqueueCounter = 0;
			prStaRec->u4DeqeueuCounter = 0;

			prAdapter->prGlueInfo->IsrCnt = 0;
			prAdapter->prGlueInfo->IsrPassCnt = 0;
			prAdapter->prGlueInfo->TaskIsrCnt = 0;

			prAdapter->prGlueInfo->IsrAbnormalCnt = 0;
			prAdapter->prGlueInfo->IsrSoftWareCnt = 0;
			prAdapter->prGlueInfo->IsrRxCnt = 0;
			prAdapter->prGlueInfo->IsrTxCnt = 0;
		}
		/*link layer statistics */
		if (prQueryStaStatistics->ucLlsReadClear) {
			for (eAci = 0; eAci < WMM_AC_INDEX_NUM; eAci++) {
				prStaRec->arLinkStatistics[eAci].u4TxMsdu = 0;
				prStaRec->arLinkStatistics[eAci].u4RxMsdu = 0;
				prStaRec->arLinkStatistics[eAci].u4TxDropMsdu
									  = 0;
			}
		}
#endif

		for (ucIdx = TC0_INDEX; ucIdx <= TC3_INDEX; ucIdx++)
			prQueryStaStatistics->au4TcQueLen[ucIdx] =
				prStaRec->aprTargetQueue[ucIdx]->u4NumElem;

		rResult = WLAN_STATUS_SUCCESS;

		/* 4 6. Ensure FW supports get station link status */
		rQueryCmdStaStatistics.ucIndex = prStaRec->ucIndex;
		COPY_MAC_ADDR(rQueryCmdStaStatistics.aucMacAddr,
			      prQueryStaStatistics->aucMacAddr);
		rQueryCmdStaStatistics.ucReadClear =
			prQueryStaStatistics->ucReadClear;
		rQueryCmdStaStatistics.ucLlsReadClear =
			prQueryStaStatistics->ucLlsReadClear;
		rQueryCmdStaStatistics.ucResetCounter =
			prQueryStaStatistics->ucResetCounter;

		rResult = wlanSendSetQueryCmd(prAdapter,
				      CMD_ID_GET_STA_STATISTICS,
				      FALSE,
				      TRUE,
				      fgIsOid,
				      nicCmdEventQueryStaStatistics,
				      nicOidCmdTimeoutCommon,
				      sizeof(struct CMD_GET_STA_STATISTICS),
				      (uint8_t *)&rQueryCmdStaStatistics,
				      pvQueryBuffer, u4QueryBufferLen);

		if ((!fgIsOid) && (rResult == WLAN_STATUS_PENDING))
			rResult = WLAN_STATUS_SUCCESS;

		prQueryStaStatistics->u4Flag |= BIT(1);

	} while (FALSE);

	return rResult;
}				/* wlanoidQueryP2pVersion */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is to query Nic resource information
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 *
 * @return WLAN_STATUS_SUCCESS
 */
/*----------------------------------------------------------------------------*/
void wlanQueryNicResourceInformation(IN struct ADAPTER *prAdapter)
{
	/* 3 1. Get Nic resource information from FW */

	/* 3 2. Setup resource parameter */

	/* 3 3. Reset Tx resource */
	nicTxResetResource(prAdapter);
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is to query Nic resource information
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 *
 * @return WLAN_STATUS_SUCCESS
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanQueryNicCapabilityV2(IN struct ADAPTER *prAdapter)
{
	uint8_t ucCmdSeqNum;
	struct CMD_INFO *prCmdInfo;
	struct WIFI_CMD *prWifiCmd;
	uint32_t u4RxPktLength;
	uint8_t *prEventBuff;
	struct HW_MAC_RX_DESC *prRxStatus;
	struct WIFI_EVENT *prEvent;
	struct mt66xx_chip_info *prChipInfo;
	uint32_t chip_id;

	ASSERT(prAdapter);
	prChipInfo = prAdapter->chip_info;
	chip_id = prChipInfo->chip_id;

	ASSERT(prAdapter);

	/* Get Nic resource information from FW */
	if (!prChipInfo->isNicCapV1
	    || (prAdapter->u4FwFeatureFlag0 &
		FEATURE_FLAG0_NIC_CAPABILITY_V2)) {

		DBGLOG(INIT, INFO, "Support NIC_CAPABILITY_V2 feature\n");

		/*
		 * send NIC_CAPABILITY_V2 query cmd
		 */

		/* 1. Allocate CMD Info Packet and its Buffer */
		prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, CMD_HDR_SIZE);
		if (!prCmdInfo) {
			DBGLOG(INIT, ERROR,
			       "Allocate CMD_INFO_T ==> FAILED.\n");
			return WLAN_STATUS_FAILURE;
		}
		/* increase command sequence number */
		ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

		/* compose CMD_BUILD_CONNECTION cmd pkt */
		prCmdInfo->eCmdType = COMMAND_TYPE_GENERAL_IOCTL;
		prCmdInfo->u2InfoBufLen = CMD_HDR_SIZE;
		prCmdInfo->pfCmdDoneHandler = NULL;
		prCmdInfo->fgIsOid = FALSE;
		prCmdInfo->ucCID = CMD_ID_GET_NIC_CAPABILITY_V2;
		prCmdInfo->fgSetQuery = FALSE;
		prCmdInfo->fgNeedResp = TRUE;
		prCmdInfo->ucCmdSeqNum = ucCmdSeqNum;
		prCmdInfo->u4SetInfoLen = 0;

		/* Setup WIFI_CMD_T */
		prWifiCmd = (struct WIFI_CMD *) (prCmdInfo->pucInfoBuffer);
		prWifiCmd->u2TxByteCount = prCmdInfo->u2InfoBufLen;
		prWifiCmd->u2PQ_ID = CMD_PQ_ID;
		prWifiCmd->ucPktTypeID = CMD_PACKET_TYPE_ID;
		prWifiCmd->ucCID = prCmdInfo->ucCID;
		prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
		prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;

		prWifiCmd->u2Length = prCmdInfo->u2InfoBufLen -
				(uint16_t) OFFSET_OF(struct WIFI_CMD, u2Length);

		wlanSendCommand(prAdapter, prCmdInfo);

		cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

		/*
		 * receive nic_capability_v2 event
		 */

		/* allocate event buffer */
		prEventBuff = cnmMemAlloc(prAdapter, RAM_TYPE_BUF,
					  CFG_RX_MAX_PKT_SIZE);
		if (!prEventBuff) {
			DBGLOG(INIT, WARN, "%s: event buffer alloc failed!\n",
			       __func__);
			return WLAN_STATUS_FAILURE;
		}

		/* get event */
		while (TRUE) {
			if (nicRxWaitResponse(prAdapter,
					      1,
					      prEventBuff,
					      CFG_RX_MAX_PKT_SIZE,
					      &u4RxPktLength)
			    != WLAN_STATUS_SUCCESS) {
				DBGLOG(INIT, WARN,
				       "%s: wait for event failed!\n",
				       __func__);

				/* free event buffer */
				cnmMemFree(prAdapter, prEventBuff);

				return WLAN_STATUS_FAILURE;
			}

			/* header checking .. */
			prRxStatus = (struct HW_MAC_RX_DESC *) prEventBuff;
			if ((prRxStatus->u2PktTYpe &
			    RXM_RXD_PKT_TYPE_SW_BITMAP) !=
			    RXM_RXD_PKT_TYPE_SW_EVENT) {
				DBGLOG(INIT, WARN,
				       "%s: skip unexpected Rx pkt type[0x%04x]\n",
				       __func__, prRxStatus->u2PktTYpe);

				continue;
			}

			prEvent = (struct WIFI_EVENT *)
				(prEventBuff + prChipInfo->rxd_size);
			if (prEvent->ucEID != EVENT_ID_NIC_CAPABILITY_V2) {
				DBGLOG(INIT, WARN,
				       "%s: skip unexpected event ID[0x%02x]\n",
				       __func__, prEvent->ucEID);

				continue;
			} else {
				/* hit */
				break;
			}
		}

		/*
		 * parsing elemens
		 */

		nicCmdEventQueryNicCapabilityV2(prAdapter,
						prEvent->aucBuffer);

		/*
		 * free event buffer
		 */
		cnmMemFree(prAdapter, prEventBuff);
	}

	/* Fill capability for different Chip version */
	if (chip_id == HQA_CHIP_ID_6632) {
		/* 6632 only */
		prAdapter->fgIsSupportBufferBinSize16Byte = TRUE;
		prAdapter->fgIsSupportDelayCal = FALSE;
		prAdapter->fgIsSupportGetFreeEfuseBlockCount = FALSE;
		prAdapter->fgIsSupportQAAccessEfuse = FALSE;
		prAdapter->fgIsSupportPowerOnSendBufferModeCMD = FALSE;
		prAdapter->fgIsSupportGetTxPower = FALSE;
	} else {
		prAdapter->fgIsSupportBufferBinSize16Byte = FALSE;
		prAdapter->fgIsSupportDelayCal = TRUE;
		prAdapter->fgIsSupportGetFreeEfuseBlockCount = TRUE;
		prAdapter->fgIsSupportQAAccessEfuse = TRUE;
		prAdapter->fgIsSupportPowerOnSendBufferModeCMD = TRUE;
		prAdapter->fgIsSupportGetTxPower = TRUE;
	}

	return WLAN_STATUS_SUCCESS;
}

void wlanSetNicResourceParameters(IN struct ADAPTER
				  *prAdapter)
{
	uint8_t string[128], idx;
	uint32_t u4share;
	uint32_t u4MaxPageCntPerFrame =
		prAdapter->rTxCtrl.u4MaxPageCntPerFrame;
	struct WIFI_VAR *prWifiVar = &prAdapter->rWifiVar;
#if QM_ADAPTIVE_TC_RESOURCE_CTRL
	struct QUE_MGT *prQM = &prAdapter->rQM;
#endif

	/*
	 * Use the settings in config file first,
	 * else, use the settings reported from firmware.
	 */


	/*
	 * 1. assign free page count for each TC
	 */

	/* 1 1. update free page count in TC control: MCU and LMAC */
	prWifiVar->au4TcPageCount[TC4_INDEX] =
		prAdapter->nicTxReousrce.u4CmdTotalResource *
		u4MaxPageCntPerFrame;	 /* MCU */

	u4share = prAdapter->nicTxReousrce.u4DataTotalResource /
		  (TC_NUM - 1); /* LMAC. Except TC_4, which is MCU */
	for (idx = TC0_INDEX; idx < TC_NUM; idx++) {
		if (idx != TC4_INDEX)
			prWifiVar->au4TcPageCount[idx] = u4share *
							 u4MaxPageCntPerFrame;
	}

	/* 1 2. if there is remaings, give them to TC_3, which is VO */
	prWifiVar->au4TcPageCount[TC3_INDEX] +=
		(prAdapter->nicTxReousrce.u4DataTotalResource %
		 (TC_NUM - 1)) * u4MaxPageCntPerFrame;

#if QM_ADAPTIVE_TC_RESOURCE_CTRL
	/*
	 * 2. assign guaranteed page count for each TC
	 */

	/* 2 1. update guaranteed page count in QM */
	for (idx = 0; idx < TC_NUM; idx++)
		prQM->au4GuaranteedTcResource[idx] =
			prWifiVar->au4TcPageCount[idx];
#endif


#if CFG_SUPPORT_CFG_FILE
	/*
	 * 3. Use the settings in config file first,
	 *    else, use the settings reported from firmware.
	 */

	/* 3 1. update for free page count */
	for (idx = 0; idx < TC_NUM; idx++) {

		/* construct prefix: Tc0Page, Tc1Page... */
		memset(string, 0, sizeof(string) / sizeof(uint8_t));
		snprintf(string, sizeof(string) / sizeof(uint8_t),
			 "Tc%xPage", idx);

		/* update the final value */
		prWifiVar->au4TcPageCount[idx] =
			(uint32_t) wlanCfgGetUint32(prAdapter, string,
						prWifiVar->au4TcPageCount[idx]);
	}

#if QM_ADAPTIVE_TC_RESOURCE_CTRL
	/* 3 2. update for guaranteed page count */
	for (idx = 0; idx < TC_NUM; idx++) {

		/* construct prefix: Tc0Grt, Tc1Grt... */
		memset(string, 0, sizeof(string) / sizeof(uint8_t));
		snprintf(string, sizeof(string) / sizeof(uint8_t),
			 "Tc%xGrt", idx);

		/* update the final value */
		prQM->au4GuaranteedTcResource[idx] =
			(uint32_t) wlanCfgGetUint32(prAdapter, string,
					prQM->au4GuaranteedTcResource[idx]);
	}
#endif /* end of #if QM_ADAPTIVE_TC_RESOURCE_CTRL */

#endif /* end of #if CFG_SUPPORT_CFG_FILE */
}


/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is to re-assign tx resource based on firmware's report
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 *
 * @return WLAN_STATUS_SUCCESS
 */
/*----------------------------------------------------------------------------*/
void wlanUpdateNicResourceInformation(IN struct ADAPTER
				      *prAdapter)
{
	/*
	 * 3 1. Query TX resource
	 */

	/* information is not got from firmware, use default value */
	if (prAdapter->fgIsNicTxReousrceValid != TRUE)
		return;

	/* 3 2. Setup resource parameters */
	if (prAdapter->nicTxReousrce.txResourceInit)
		prAdapter->nicTxReousrce.txResourceInit(prAdapter);
	else
		wlanSetNicResourceParameters(prAdapter);/* 6632, 7668 ways*/

	/* 3 3. Reset Tx resource */
	nicTxResetResource(prAdapter);

#if QM_ADAPTIVE_TC_RESOURCE_CTRL
	/* 3 4. Reset QM resource */
	qmResetTcControlResource(
		prAdapter); /*CHIAHSUAN, TBD, NO PLE YET*/
#endif

	halTxResourceResetHwTQCounter(prAdapter);
}


#if 0
/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is to SET network interface index for a network
 *        interface.
 *        A network interface is a TX/RX data port hooked to OS.
 *
 * @param prGlueInfo                     Pointer of prGlueInfo Data Structure
 * @param ucNetInterfaceIndex            Index of network interface
 * @param ucBssIndex                     Index of BSS
 *
 * @return VOID
 */
/*----------------------------------------------------------------------------*/
void wlanBindNetInterface(IN struct GLUE_INFO *prGlueInfo,
			  IN uint8_t ucNetInterfaceIndex,
			  IN void *pvNetInterface)
{
	struct NET_INTERFACE_INFO *prNetIfInfo;

	prNetIfInfo =
		&prGlueInfo->arNetInterfaceInfo[ucNetInterfaceIndex];

	prNetIfInfo->pvNetInterface = pvNetInterface;
}
#endif
/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is to SET BSS index for a network interface.
 *           A network interface is a TX/RX data port hooked to OS.
 *
 * @param prGlueInfo                     Pointer of prGlueInfo Data Structure
 * @param ucNetInterfaceIndex            Index of network interface
 * @param ucBssIndex                     Index of BSS
 *
 * @return VOID
 */
/*----------------------------------------------------------------------------*/
void wlanBindBssIdxToNetInterface(IN struct GLUE_INFO *prGlueInfo,
				  IN uint8_t ucBssIndex,
				  IN void *pvNetInterface)
{
	struct NET_INTERFACE_INFO *prNetIfInfo;

	if (ucBssIndex >= prGlueInfo->prAdapter->ucHwBssIdNum) {
		DBGLOG(INIT, ERROR,
		       "Array index out of bound, ucBssIndex=%u\n", ucBssIndex);
		return;
	}

	prNetIfInfo = &prGlueInfo->arNetInterfaceInfo[ucBssIndex];

	prNetIfInfo->ucBssIndex = ucBssIndex;
	prNetIfInfo->pvNetInterface = pvNetInterface;
	/* prGlueInfo->aprBssIdxToNetInterfaceInfo[ucBssIndex] = prNetIfInfo; */
}

#if 0
/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is to GET BSS index for a network interface.
 *           A network interface is a TX/RX data port hooked to OS.
 *
 * @param prGlueInfo                     Pointer of prGlueInfo Data Structure
 * @param ucNetInterfaceIndex       Index of network interface
 *
 * @return UINT_8                         Index of BSS
 */
/*----------------------------------------------------------------------------*/
uint8_t wlanGetBssIdxByNetInterface(IN struct GLUE_INFO
				    *prGlueInfo, IN void *pvNetInterface)
{
	uint8_t ucIdx = 0;

	ASSERT(prGlueInfo);

	for (ucIdx = 0; ucIdx < prGlueInfo->prAdapter->ucHwBssIdNum;
	     ucIdx++) {
		if (prGlueInfo->arNetInterfaceInfo[ucIdx].pvNetInterface ==
		    pvNetInterface)
			break;
	}

	return ucIdx;
}
#endif
/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is to GET network interface for a BSS.
 *           A network interface is a TX/RX data port hooked to OS.
 *
 * @param prGlueInfo                     Pointer of prGlueInfo Data Structure
 * @param ucBssIndex                     Index of BSS
 *
 * @return PVOID                         pointer of network interface structure
 */
/*----------------------------------------------------------------------------*/
void *wlanGetNetInterfaceByBssIdx(IN struct GLUE_INFO
				  *prGlueInfo, IN uint8_t ucBssIndex)
{
	return prGlueInfo->arNetInterfaceInfo[ucBssIndex].pvNetInterface;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is to get BSS-INDEX for AIS network.
 *
 * @param prAdapter  Pointer of ADAPTER_T
 *
 * @return value, as corresponding index of BSS
 */
/*----------------------------------------------------------------------------*/
uint8_t wlanGetAisBssIndex(IN struct ADAPTER *prAdapter)
{
	ASSERT(prAdapter);
	ASSERT(prAdapter->prAisBssInfo);

	return prAdapter->prAisBssInfo->ucBssIndex;
}
/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is to initialize WLAN feature options
 *
 * @param prAdapter  Pointer of ADAPTER_T
 *
 * @return none
 */
/*----------------------------------------------------------------------------*/
void wlanInitFeatureOption(IN struct ADAPTER *prAdapter)
{
	struct WIFI_VAR *prWifiVar = &prAdapter->rWifiVar;
#if QM_ADAPTIVE_TC_RESOURCE_CTRL
	struct QUE_MGT *prQM = &prAdapter->rQM;
#endif

	/* Feature options will be filled by config file */

	prWifiVar->ucQoS = (uint8_t) wlanCfgGetUint32(prAdapter, "Qos",
					FEATURE_ENABLED);

	prWifiVar->ucStaHt = (uint8_t) wlanCfgGetUint32(prAdapter, "StaHT",
					FEATURE_ENABLED);
	prWifiVar->ucStaVht = (uint8_t) wlanCfgGetUint32(prAdapter, "StaVHT",
					FEATURE_ENABLED);

	prWifiVar->ucApHt = (uint8_t) wlanCfgGetUint32(prAdapter, "ApHT",
					FEATURE_ENABLED);
	prWifiVar->ucApVht = (uint8_t) wlanCfgGetUint32(prAdapter, "ApVHT",
					FEATURE_ENABLED);

	prWifiVar->ucP2pGoHt = (uint8_t) wlanCfgGetUint32(prAdapter, "P2pGoHT",
					FEATURE_ENABLED);
	prWifiVar->ucP2pGoVht = (uint8_t) wlanCfgGetUint32(prAdapter,
					"P2pGoVHT", FEATURE_ENABLED);

	prWifiVar->ucP2pGcHt = (uint8_t) wlanCfgGetUint32(prAdapter, "P2pGcHT",
					FEATURE_ENABLED);
	prWifiVar->ucP2pGcVht = (uint8_t) wlanCfgGetUint32(prAdapter,
					"P2pGcVHT", FEATURE_ENABLED);

	prWifiVar->ucAmpduRx = (uint8_t) wlanCfgGetUint32(prAdapter, "AmpduRx",
					FEATURE_ENABLED);
	prWifiVar->ucAmpduTx = (uint8_t) wlanCfgGetUint32(prAdapter, "AmpduTx",
					FEATURE_ENABLED);

	prWifiVar->ucAmsduInAmpduRx = (uint8_t) wlanCfgGetUint32(prAdapter,
					"AmsduInAmpduRx", FEATURE_ENABLED);
	prWifiVar->ucAmsduInAmpduTx = (uint8_t) wlanCfgGetUint32(prAdapter,
					"AmsduInAmpduTx", FEATURE_ENABLED);
	prWifiVar->ucHtAmsduInAmpduRx = (uint8_t) wlanCfgGetUint32(prAdapter,
					"HtAmsduInAmpduRx", FEATURE_DISABLED);
	prWifiVar->ucHtAmsduInAmpduTx = (uint8_t) wlanCfgGetUint32(prAdapter,
					"HtAmsduInAmpduTx", FEATURE_DISABLED);
	prWifiVar->ucVhtAmsduInAmpduRx = (uint8_t) wlanCfgGetUint32(prAdapter,
					"VhtAmsduInAmpduRx", FEATURE_ENABLED);
	prWifiVar->ucVhtAmsduInAmpduTx = (uint8_t) wlanCfgGetUint32(prAdapter,
					"VhtAmsduInAmpduTx", FEATURE_ENABLED);

	prWifiVar->ucTspec = (uint8_t) wlanCfgGetUint32(prAdapter, "Tspec",
					FEATURE_DISABLED);

	prWifiVar->ucUapsd = (uint8_t) wlanCfgGetUint32(prAdapter, "Uapsd",
					FEATURE_ENABLED);
	prWifiVar->ucStaUapsd = (uint8_t) wlanCfgGetUint32(prAdapter,
					"StaUapsd", FEATURE_DISABLED);
	prWifiVar->ucApUapsd = (uint8_t) wlanCfgGetUint32(prAdapter,
					"ApUapsd", FEATURE_DISABLED);
	prWifiVar->ucP2pUapsd = (uint8_t) wlanCfgGetUint32(prAdapter,
					"P2pUapsd", FEATURE_ENABLED);
	prWifiVar->u4RegP2pIfAtProbe = (uint8_t) wlanCfgGetUint32(prAdapter,
					"RegP2pIfAtProbe", FEATURE_DISABLED);
	prWifiVar->ucP2pShareMacAddr = (uint8_t) wlanCfgGetUint32(prAdapter,
					"P2pShareMacAddr", FEATURE_DISABLED);

	prWifiVar->ucTxShortGI = (uint8_t) wlanCfgGetUint32(prAdapter, "SgiTx",
					FEATURE_ENABLED);
	prWifiVar->ucRxShortGI = (uint8_t) wlanCfgGetUint32(prAdapter, "SgiRx",
					FEATURE_ENABLED);

	prWifiVar->ucTxLdpc = (uint8_t) wlanCfgGetUint32(prAdapter, "LdpcTx",
					FEATURE_ENABLED);
	prWifiVar->ucRxLdpc = (uint8_t) wlanCfgGetUint32(prAdapter, "LdpcRx",
					FEATURE_ENABLED);

	prWifiVar->ucTxStbc = (uint8_t) wlanCfgGetUint32(prAdapter, "StbcTx",
					FEATURE_ENABLED);
	prWifiVar->ucRxStbc = (uint8_t) wlanCfgGetUint32(prAdapter, "StbcRx",
					FEATURE_ENABLED);
	prWifiVar->ucRxStbcNss = (uint8_t) wlanCfgGetUint32(prAdapter,
					"StbcRxNss", 1);

	prWifiVar->ucTxGf = (uint8_t) wlanCfgGetUint32(prAdapter, "GfTx",
					FEATURE_ENABLED);
	prWifiVar->ucRxGf = (uint8_t) wlanCfgGetUint32(prAdapter, "GfRx",
					FEATURE_ENABLED);

	prWifiVar->ucMCS32 = (uint8_t) wlanCfgGetUint32(prAdapter, "MCS32",
					FEATURE_DISABLED);

	prWifiVar->ucSigTaRts = (uint8_t) wlanCfgGetUint32(prAdapter,
					"SigTaRts", FEATURE_DISABLED);
	prWifiVar->ucDynBwRts = (uint8_t) wlanCfgGetUint32(prAdapter,
					"DynBwRts", FEATURE_DISABLED);
	prWifiVar->ucTxopPsTx = (uint8_t) wlanCfgGetUint32(prAdapter,
					"TxopPsTx", FEATURE_DISABLED);

	prWifiVar->ucStaHtBfee = (uint8_t) wlanCfgGetUint32(prAdapter,
					"StaHTBfee", FEATURE_DISABLED);
	prWifiVar->ucStaVhtBfee = (uint8_t) wlanCfgGetUint32(prAdapter,
					"StaVHTBfee", FEATURE_ENABLED);
	prWifiVar->ucStaVhtMuBfee = (uint8_t)wlanCfgGetUint32(prAdapter,
					"StaVHTMuBfee", FEATURE_ENABLED);
	prWifiVar->ucStaHtBfer = (uint8_t) wlanCfgGetUint32(prAdapter,
					"StaHTBfer", FEATURE_DISABLED);
	prWifiVar->ucStaVhtBfer = (uint8_t) wlanCfgGetUint32(prAdapter,
					"StaVHTBfer", FEATURE_DISABLED);

	/* 0: disabled
	 * 1: Tx done event to driver
	 * 2: Tx status to FW only
	 */
	prWifiVar->ucDataTxDone = (uint8_t) wlanCfgGetUint32(
					prAdapter, "DataTxDone", 0);
	prWifiVar->ucDataTxRateMode = (uint8_t) wlanCfgGetUint32(
					prAdapter, "DataTxRateMode",
					DATA_RATE_MODE_AUTO);
	prWifiVar->u4DataTxRateCode = wlanCfgGetUint32(
					prAdapter, "DataTxRateCode", 0x0);

	prWifiVar->ucApWpsMode = (uint8_t) wlanCfgGetUint32(
					prAdapter, "ApWpsMode", 0);
	DBGLOG(INIT, TRACE, "ucApWpsMode = %u\n", prWifiVar->ucApWpsMode);

	prWifiVar->ucThreadScheduling = (uint8_t) wlanCfgGetUint32(
					prAdapter, "ThreadSched", 0);
	prWifiVar->ucThreadPriority = (uint8_t) wlanCfgGetUint32(
					prAdapter, "ThreadPriority",
					WLAN_THREAD_TASK_PRIORITY);
	prWifiVar->cThreadNice = (int8_t) wlanCfgGetInt32(
					prAdapter, "ThreadNice",
					WLAN_THREAD_TASK_NICE);

	prAdapter->rQM.u4MaxForwardBufferCount = (uint32_t) wlanCfgGetUint32(
					prAdapter, "ApForwardBufferCnt",
					QM_FWD_PKT_QUE_THRESHOLD);

	/* AP channel setting
	 * 0: auto
	 */
	prWifiVar->ucApChannel = (uint8_t) wlanCfgGetUint32(
					prAdapter, "ApChannel", 0);

	/*
	 * 0: SCN
	 * 1: SCA
	 * 2: RES
	 * 3: SCB
	 */
	prWifiVar->ucApSco = (uint8_t) wlanCfgGetUint32(
						prAdapter, "ApSco", 0);
	prWifiVar->ucP2pGoSco = (uint8_t) wlanCfgGetUint32(
						prAdapter, "P2pGoSco", 0);

	/* Max bandwidth setting
	 * 0: 20Mhz
	 * 1: 40Mhz
	 * 2: 80Mhz
	 * 3: 160Mhz
	 * 4: 80+80Mhz
	 * Note: For VHT STA, BW 80Mhz is a must!
	 */
	prWifiVar->ucStaBandwidth = (uint8_t) wlanCfgGetUint32(
				prAdapter, "StaBw", MAX_BW_160MHZ);
	prWifiVar->ucSta2gBandwidth = (uint8_t) wlanCfgGetUint32(
				prAdapter, "Sta2gBw", MAX_BW_20MHZ);
	prWifiVar->ucSta5gBandwidth = (uint8_t) wlanCfgGetUint32(
				prAdapter, "Sta5gBw", MAX_BW_80MHZ);
	/* GC,GO */
	prWifiVar->ucP2p2gBandwidth = (uint8_t) wlanCfgGetUint32(
				prAdapter, "P2p2gBw", MAX_BW_20MHZ);
	prWifiVar->ucP2p5gBandwidth = (uint8_t) wlanCfgGetUint32(
				prAdapter, "P2p5gBw", MAX_BW_80MHZ);
	prWifiVar->ucApBandwidth = (uint8_t) wlanCfgGetUint32(
				prAdapter, "ApBw", MAX_BW_160MHZ);
	prWifiVar->ucAp2gBandwidth = (uint8_t) wlanCfgGetUint32(
				prAdapter, "Ap2gBw", MAX_BW_20MHZ);
	prWifiVar->ucAp5gBandwidth = (uint8_t) wlanCfgGetUint32(
				prAdapter, "Ap5gBw", MAX_BW_80MHZ);
	prWifiVar->ucApChnlDefFromCfg = (uint8_t) wlanCfgGetUint32(
				prAdapter, "ApChnlDefFromCfg", FEATURE_ENABLED);
	prWifiVar->ucApAllowHtVhtTkip = (uint8_t) wlanCfgGetUint32(
				prAdapter, "ApAllowHtVhtTkip",
				FEATURE_DISABLED);

	prWifiVar->ucNSS = (uint8_t) wlanCfgGetUint32(prAdapter, "Nss", 2);

	/* Max Rx MPDU length setting
	 * 0: 3k
	 * 1: 8k
	 * 2: 11k
	 */
	prWifiVar->ucRxMaxMpduLen = (uint8_t) wlanCfgGetUint32(
					prAdapter, "RxMaxMpduLen",
					VHT_CAP_INFO_MAX_MPDU_LEN_3K);
	/* Max Tx AMSDU in AMPDU length *in BYTES* */
	prWifiVar->u4TxMaxAmsduInAmpduLen = wlanCfgGetUint32(
					prAdapter, "TxMaxAmsduInAmpduLen",
					4096);

	prWifiVar->ucTcRestrict = (uint8_t) wlanCfgGetUint32(
					prAdapter, "TcRestrict", 0xFF);
	/* Max Tx dequeue limit: 0 => auto */
	prWifiVar->u4MaxTxDeQLimit = (uint32_t) wlanCfgGetUint32(
					prAdapter, "MaxTxDeQLimit", 0x0);
	prWifiVar->ucAlwaysResetUsedRes = (uint32_t) wlanCfgGetUint32(
					prAdapter, "AlwaysResetUsedRes", 0x0);

	prWifiVar->u4BeaconTimoutFilterDurationMs =
		wlanCfgGetUint32(prAdapter,
			"BeaconTimoutFilterDurationMs",
			CFG_BEACON_TIMEOUT_FILTER_DURATION_DEFAULT_VALUE);

#if CFG_SUPPORT_MTK_SYNERGY
	prWifiVar->ucMtkOui = (uint8_t) wlanCfgGetUint32(
					prAdapter, "MtkOui", FEATURE_ENABLED);
	prWifiVar->u4MtkOuiCap = (uint32_t) wlanCfgGetUint32(
					prAdapter, "MtkOuiCap", 0);
	prWifiVar->aucMtkFeature[0] = 0xff;
	prWifiVar->aucMtkFeature[1] = 0xff;
	prWifiVar->aucMtkFeature[2] = 0xff;
	prWifiVar->aucMtkFeature[3] = 0xff;
	prWifiVar->ucGbandProbe256QAM = (uint8_t) wlanCfgGetUint32(
					prAdapter, "Probe256QAM",
					FEATURE_DISABLED);
#endif
#if CFG_SUPPORT_VHT_IE_IN_2G
	prWifiVar->ucVhtIeIn2g =
		(uint8_t) wlanCfgGetUint32(prAdapter, "VhtIeIn2G",
					FEATURE_DISABLED);
#endif
	prWifiVar->ucCmdRsvResource = (uint8_t) wlanCfgGetUint32(
					prAdapter, "TxCmdRsv",
					QM_CMD_RESERVED_THRESHOLD);
	prWifiVar->u4MgmtQueueDelayTimeout =
		(uint32_t) wlanCfgGetUint32(prAdapter, "TxMgmtQueTO",
					QM_MGMT_QUEUED_TIMEOUT); /* ms */

	/* Performance related */
	prWifiVar->u4HifIstLoopCount = (uint32_t) wlanCfgGetUint32(
					prAdapter, "IstLoop",
					CFG_IST_LOOP_COUNT);
	prWifiVar->u4Rx2OsLoopCount = (uint32_t) wlanCfgGetUint32(
					prAdapter, "Rx2OsLoop", 4);
	prWifiVar->u4HifTxloopCount = (uint32_t) wlanCfgGetUint32(
					prAdapter, "HifTxLoop", 1);
	prWifiVar->u4TxFromOsLoopCount = (uint32_t) wlanCfgGetUint32(
					prAdapter, "OsTxLoop", 1);
	prWifiVar->u4TxRxLoopCount = (uint32_t) wlanCfgGetUint32(
					prAdapter, "Rx2ReorderLoop", 1);
	prWifiVar->u4TxIntThCount = (uint32_t) wlanCfgGetUint32(
					prAdapter, "IstTxTh",
					HIF_IST_TX_THRESHOLD);

	prWifiVar->u4NetifStopTh = (uint32_t) wlanCfgGetUint32(
					prAdapter, "NetifStopTh",
					CFG_TX_STOP_NETIF_PER_QUEUE_THRESHOLD);
	prWifiVar->u4NetifStartTh = (uint32_t) wlanCfgGetUint32(
					prAdapter, "NetifStartTh",
					CFG_TX_START_NETIF_PER_QUEUE_THRESHOLD);
	prWifiVar->ucTxBaSize = (uint8_t) wlanCfgGetUint32(
					prAdapter, "TxBaSize", 64);
	prWifiVar->ucRxHtBaSize = (uint8_t) wlanCfgGetUint32(
					prAdapter, "RxHtBaSize", 64);
	prWifiVar->ucRxVhtBaSize = (uint8_t) wlanCfgGetUint32(
					prAdapter, "RxVhtBaSize", 64);

	/* Tx Buffer Management */
	prWifiVar->ucExtraTxDone = (uint32_t) wlanCfgGetUint32(
					prAdapter, "ExtraTxDone", 1);
	prWifiVar->ucTxDbg = (uint32_t) wlanCfgGetUint32(prAdapter, "TxDbg", 0);

	kalMemZero(prWifiVar->au4TcPageCount,
					sizeof(prWifiVar->au4TcPageCount));

	prWifiVar->au4TcPageCount[TC0_INDEX] = (uint32_t) wlanCfgGetUint32(
					prAdapter, "Tc0Page",
					NIC_TX_PAGE_COUNT_TC0);
	prWifiVar->au4TcPageCount[TC1_INDEX] = (uint32_t) wlanCfgGetUint32(
					prAdapter, "Tc1Page",
					NIC_TX_PAGE_COUNT_TC1);
	prWifiVar->au4TcPageCount[TC2_INDEX] = (uint32_t) wlanCfgGetUint32(
					prAdapter, "Tc2Page",
					NIC_TX_PAGE_COUNT_TC2);
	prWifiVar->au4TcPageCount[TC3_INDEX] = (uint32_t) wlanCfgGetUint32(
					prAdapter, "Tc3Page",
					NIC_TX_PAGE_COUNT_TC3);
	prWifiVar->au4TcPageCount[TC4_INDEX] = (uint32_t) wlanCfgGetUint32(
					prAdapter, "Tc4Page",
					NIC_TX_PAGE_COUNT_TC4);

#if QM_ADAPTIVE_TC_RESOURCE_CTRL
	prQM->au4MinReservedTcResource[TC0_INDEX] = (uint32_t) wlanCfgGetUint32(
					prAdapter, "Tc0MinRsv",
					QM_MIN_RESERVED_TC0_RESOURCE);
	prQM->au4MinReservedTcResource[TC1_INDEX] = (uint32_t) wlanCfgGetUint32(
					prAdapter, "Tc1MinRsv",
					QM_MIN_RESERVED_TC1_RESOURCE);
	prQM->au4MinReservedTcResource[TC2_INDEX] = (uint32_t) wlanCfgGetUint32(
					prAdapter, "Tc2MinRsv",
					QM_MIN_RESERVED_TC2_RESOURCE);
	prQM->au4MinReservedTcResource[TC3_INDEX] = (uint32_t) wlanCfgGetUint32(
					prAdapter, "Tc3MinRsv",
					QM_MIN_RESERVED_TC3_RESOURCE);
	prQM->au4MinReservedTcResource[TC4_INDEX] = (uint32_t) wlanCfgGetUint32(
					prAdapter, "Tc4MinRsv",
					QM_MIN_RESERVED_TC4_RESOURCE);

	prQM->au4GuaranteedTcResource[TC0_INDEX] = (uint32_t) wlanCfgGetUint32(
					prAdapter, "Tc0Grt",
					QM_GUARANTEED_TC0_RESOURCE);
	prQM->au4GuaranteedTcResource[TC1_INDEX] =
		(uint32_t) wlanCfgGetUint32(prAdapter, "Tc1Grt",
					    QM_GUARANTEED_TC1_RESOURCE);
	prQM->au4GuaranteedTcResource[TC2_INDEX] =
		(uint32_t) wlanCfgGetUint32(prAdapter, "Tc2Grt",
					    QM_GUARANTEED_TC2_RESOURCE);
	prQM->au4GuaranteedTcResource[TC3_INDEX] = (uint32_t) wlanCfgGetUint32(
					prAdapter, "Tc3Grt",
					QM_GUARANTEED_TC3_RESOURCE);
	prQM->au4GuaranteedTcResource[TC4_INDEX] = (uint32_t) wlanCfgGetUint32(
					prAdapter, "Tc4Grt",
					QM_GUARANTEED_TC4_RESOURCE);

	prQM->u4TimeToAdjustTcResource = (uint32_t) wlanCfgGetUint32(
					prAdapter, "TcAdjustTime",
					QM_INIT_TIME_TO_ADJUST_TC_RSC);
	prQM->u4TimeToUpdateQueLen = (uint32_t) wlanCfgGetUint32(
					prAdapter, "QueLenUpdateTime",
					QM_INIT_TIME_TO_UPDATE_QUE_LEN);
	prQM->u4QueLenMovingAverage = (uint32_t) wlanCfgGetUint32(
					prAdapter, "QueLenMovingAvg",
					QM_QUE_LEN_MOVING_AVE_FACTOR);
	prQM->u4ExtraReservedTcResource = (uint32_t) wlanCfgGetUint32(
					prAdapter, "TcExtraRsv",
					QM_EXTRA_RESERVED_RESOURCE_WHEN_BUSY);
#endif

	/* Stats log */
	prWifiVar->u4StatsLogTimeout = (uint32_t) wlanCfgGetUint32(
					prAdapter, "StatsLogTO",
					WLAN_TX_STATS_LOG_TIMEOUT);
	prWifiVar->u4StatsLogDuration = (uint32_t) wlanCfgGetUint32(
					prAdapter, "StatsLogDur",
					WLAN_TX_STATS_LOG_DURATION);

	prWifiVar->ucDhcpTxDone = (uint8_t) wlanCfgGetUint32(
					prAdapter, "DhcpTxDone", 1);
	prWifiVar->ucArpTxDone = (uint8_t) wlanCfgGetUint32(
					prAdapter, "ArpTxDone", 1);

	prWifiVar->ucMacAddrOverride = (uint8_t) wlanCfgGetInt32(
				       prAdapter, "MacOverride", 0);
	if (wlanCfgGet(prAdapter, "MacAddr", prWifiVar->aucMacAddrStr,
	    "00:0c:e7:66:32:e1", 0))
		DBGLOG(INIT, ERROR, "get MacAddr fail, use defaul\n");

	prWifiVar->ucCtiaMode = (uint8_t) wlanCfgGetUint32(
					prAdapter, "CtiaMode", 0);

	/* Combine ucTpTestMode and ucSigmaTestMode in one flag */
	/* ucTpTestMode == 0, for normal driver */
	/* ucTpTestMode == 1, for pure throughput test mode (ex: RvR) */
	/* ucTpTestMode == 2, for sigma TGn/TGac/PMF */
	/* ucTpTestMode == 3, for sigma WMM PS */
	prWifiVar->ucTpTestMode = (uint8_t) wlanCfgGetUint32(
					prAdapter, "TpTestMode", 0);

#if IS_ENABLED(CFG_RX_NAPI_SUPPORT)
	prWifiVar->ucRxNapiEnable = (uint8_t) wlanCfgGetUint32(
					prAdapter, "RxNapi", 0);
	prWifiVar->ucRxNapiPktChk = (uint8_t) wlanCfgGetUint32(
					prAdapter, "RxNapiPktChk", 1);
	prWifiVar->ucRxNapiThread = (uint8_t) wlanCfgGetUint32(
					prAdapter, "RxNapiThread", 0);
	prWifiVar->ucRxNapiNoTx = (uint8_t) wlanCfgGetUint32(
					prAdapter, "RxNapiNoTx", 1);
	prWifiVar->ucRxNapiThreshold = (uint8_t) wlanCfgGetUint32(
					prAdapter, "RxNapiThreshold", 50);
#endif /* CFG_RX_NAPI_SUPPORT */

#if 0
	prWifiVar->ucSigmaTestMode = (uint8_t) wlanCfgGetUint32(
					prAdapter, "SigmaTestMode", 0);
#endif

#if CFG_SUPPORT_DBDC
	prWifiVar->eDbdcMode = (uint8_t) wlanCfgGetUint32(
					prAdapter, "DbdcMode",
					ENUM_DBDC_MODE_DISABLED);
#endif /*CFG_SUPPORT_DBDC*/
#if (CFG_EFUSE_BUFFER_MODE_DELAY_CAL == 1)
	prWifiVar->ucEfuseBufferModeCal = (uint8_t) wlanCfgGetUint32(
					prAdapter, "EfuseBufferModeCal", 0);
#endif
	prWifiVar->ucCalTimingCtrl = (uint8_t) wlanCfgGetUint32(
					prAdapter, "CalTimingCtrl",
					0 /* power on full cal */);
	prWifiVar->ucWow = (uint8_t) wlanCfgGetUint32(
					prAdapter, "Wow", FEATURE_DISABLED);
	prWifiVar->ucOffload = (uint8_t) wlanCfgGetUint32(
					prAdapter, "Offload", FEATURE_DISABLED);
	prWifiVar->ucAdvPws = (uint8_t) wlanCfgGetUint32(
					prAdapter, "AdvPws", FEATURE_DISABLED);
	prWifiVar->ucWowOnMdtim = (uint8_t) wlanCfgGetUint32(
					prAdapter, "WowOnMdtim", 1);
	prWifiVar->ucWowOffMdtim = (uint8_t) wlanCfgGetUint32(
					prAdapter, "WowOffMdtim", 3);
	prWifiVar->ucWowPwsMode = (uint8_t) wlanCfgGetUint32(
		prAdapter, "WowPsMode", Param_PowerModeFast_PSP);
	prWifiVar->ucListenDtimInterval =
		(uint8_t) wlanCfgGetUint32(prAdapter, "ListenDtimInt",
		DEFAULT_LISTEN_INTERVAL_BY_DTIM_PERIOD);
	prWifiVar->ucEapolOffload = (uint8_t) wlanCfgGetUint32(
#if CFG_SUPPORT_REPLAY_DETECTION
		prAdapter, "EapolOffload", FEATURE_DISABLED);
#else
		prAdapter, "EapolOffload", FEATURE_ENABLED);
#endif
	prWifiVar->ucEnforcePSMode = (uint8_t) wlanCfgGetUint32(
		prAdapter, "EnforcePSMode", Param_PowerModeMax);
#if CFG_SUPPORT_REPLAY_DETECTION
	prWifiVar->ucRpyDetectOffload = (uint8_t) wlanCfgGetUint32(
					prAdapter, "rpydetectoffload",
					FEATURE_ENABLED);
#endif


#if CFG_WOW_SUPPORT
	prAdapter->rWowCtrl.fgWowEnable = (uint8_t) wlanCfgGetUint32(
					prAdapter, "WowEnable",
					FEATURE_ENABLED);
	prAdapter->rWowCtrl.ucScenarioId = (uint8_t) wlanCfgGetUint32(
					prAdapter, "WowScenarioId", 0);
	prAdapter->rWowCtrl.ucBlockCount = (uint8_t) wlanCfgGetUint32(
					prAdapter, "WowPinCnt", 1);
	prAdapter->rWowCtrl.astWakeHif[0].ucWakeupHif =
					(uint8_t) wlanCfgGetUint32(
						prAdapter, "WowHif",
						ENUM_HIF_TYPE_GPIO);
	prAdapter->rWowCtrl.astWakeHif[0].ucGpioPin =
		(uint8_t) wlanCfgGetUint32(prAdapter, "WowGpioPin", 0xFF);
	prAdapter->rWowCtrl.astWakeHif[0].ucTriggerLvl =
		(uint8_t) wlanCfgGetUint32(prAdapter, "WowTriigerLevel", 3);
	prAdapter->rWowCtrl.astWakeHif[0].u4GpioInterval =
		wlanCfgGetUint32(prAdapter, "GpioInterval", 0);
#endif

	/* SW Test Mode: Mainly used for Sigma */
	prWifiVar->u4SwTestMode = (uint8_t) wlanCfgGetUint32(
					prAdapter, "Sigma",
					ENUM_SW_TEST_MODE_NONE);
	prWifiVar->ucCtrlFlagAssertPath = (uint8_t) wlanCfgGetUint32(
					prAdapter, "AssertPath",
					DBG_ASSERT_PATH_DEFAULT);
	prWifiVar->ucCtrlFlagDebugLevel = (uint8_t) wlanCfgGetUint32(
					prAdapter, "AssertLevel",
					DBG_ASSERT_CTRL_LEVEL_DEFAULT);
	prWifiVar->u4ScanCtrl = (uint8_t) wlanCfgGetUint32(
					prAdapter, "ScanCtrl",
					SCN_CTRL_DEFAULT_SCAN_CTRL);
	prWifiVar->ucScanChannelListenTime = (uint8_t) wlanCfgGetUint32(
					prAdapter, "ScnChListenTime", 0);

	/* Wake lock related configuration */
	prWifiVar->u4WakeLockRxTimeout = wlanCfgGetUint32(
					prAdapter, "WakeLockRxTO",
					WAKE_LOCK_RX_TIMEOUT);
	prWifiVar->u4WakeLockThreadWakeup = wlanCfgGetUint32(
					prAdapter, "WakeLockThreadTO",
					WAKE_LOCK_THREAD_WAKEUP_TIMEOUT);

	prWifiVar->ucSmartRTS = (uint8_t) wlanCfgGetUint32(
					prAdapter, "SmartRTS", 0);
	prWifiVar->ePowerMode = (enum PARAM_POWER_MODE) wlanCfgGetUint32(
					prAdapter, "PowerSave",
					Param_PowerModeMax);

	prWifiVar->fgActiveModeCam = (uint8_t) wlanCfgGetUint32(
					prAdapter, "ActiveModeCam",
					FEATURE_DISABLED);

#if 1
	/* add more cfg from RegInfo */
	prWifiVar->u4UapsdAcBmp = (uint32_t) wlanCfgGetUint32(
					prAdapter, "UapsdAcBmp", 0);
	prWifiVar->u4MaxSpLen = (uint32_t) wlanCfgGetUint32(
					prAdapter, "MaxSpLen", 0);
	prWifiVar->fgDisOnlineScan = (uint32_t) wlanCfgGetUint32(
					prAdapter, "DisOnlineScan", 0);
	prWifiVar->fgDisBcnLostDetection = (uint32_t) wlanCfgGetUint32(
					prAdapter, "DisBcnLostDetection", 0);
	prWifiVar->fgDisRoaming = (uint32_t) wlanCfgGetUint32(
					prAdapter, "DisRoaming", 0);
	prWifiVar->fgEnArpFilter = (uint32_t) wlanCfgGetUint32(
					prAdapter, "EnArpFilter",
					FEATURE_ENABLED);
#endif

	/* Driver Flow Control Dequeue Quota. Now is only used by DBDC */
	prWifiVar->uDeQuePercentEnable =
		(uint8_t) wlanCfgGetUint32(prAdapter, "DeQuePercentEnable", 1);
	prWifiVar->u4DeQuePercentVHT80Nss1 =
		(uint32_t) wlanCfgGetUint32(prAdapter, "DeQuePercentVHT80NSS1",
					QM_DEQUE_PERCENT_VHT80_NSS1);
	prWifiVar->u4DeQuePercentVHT40Nss1 =
		(uint32_t) wlanCfgGetUint32(prAdapter, "DeQuePercentVHT40NSS1",
					QM_DEQUE_PERCENT_VHT40_NSS1);
	prWifiVar->u4DeQuePercentVHT20Nss1 =
		(uint32_t) wlanCfgGetUint32(prAdapter, "DeQuePercentVHT20NSS1",
					QM_DEQUE_PERCENT_VHT20_NSS1);
	prWifiVar->u4DeQuePercentHT40Nss1 =
		(uint32_t) wlanCfgGetUint32(prAdapter, "DeQuePercentHT40NSS1",
					QM_DEQUE_PERCENT_HT40_NSS1);
	prWifiVar->u4DeQuePercentHT20Nss1 =
		(uint32_t) wlanCfgGetUint32(prAdapter, "DeQuePercentHT20NSS1",
					QM_DEQUE_PERCENT_HT20_NSS1);

	/* Support TDLS 5.5.4.2 optional case */
	prWifiVar->fgTdlsBufferSTASleep = (u_int8_t) wlanCfgGetUint32(prAdapter,
					"TdlsBufferSTASleep", FEATURE_DISABLED);
	/* Support USB Whole chip reset recover */
	prWifiVar->fgChipResetRecover = (u_int8_t) wlanCfgGetUint32(prAdapter,
					"ChipResetRecover", FEATURE_DISABLED);

	prWifiVar->u4PerfMonUpdatePeriod =
		(uint32_t) wlanCfgGetUint32(prAdapter, "PerfMonPeriod",
					    PERF_MON_UPDATE_INTERVAL);

	prWifiVar->u4PerfMonTpTh[0] =
		(uint32_t) wlanCfgGetUint32(prAdapter, "PerfMonLv1", 20);
	prWifiVar->u4PerfMonTpTh[1] =
		(uint32_t) wlanCfgGetUint32(prAdapter, "PerfMonLv2", 50);
	prWifiVar->u4PerfMonTpTh[2] =
		(uint32_t) wlanCfgGetUint32(prAdapter, "PerfMonLv3", 135);
	prWifiVar->u4PerfMonTpTh[3] =
		(uint32_t) wlanCfgGetUint32(prAdapter, "PerfMonLv4", 180);
	prWifiVar->u4PerfMonTpTh[4] =
		(uint32_t) wlanCfgGetUint32(prAdapter, "PerfMonLv5", 250);
	prWifiVar->u4PerfMonTpTh[5] =
		(uint32_t) wlanCfgGetUint32(prAdapter, "PerfMonLv6", 300);
	prWifiVar->u4PerfMonTpTh[6] =
		(uint32_t) wlanCfgGetUint32(prAdapter, "PerfMonLv7", 400);
	prWifiVar->u4PerfMonTpTh[7] =
		(uint32_t) wlanCfgGetUint32(prAdapter, "PerfMonLv8", 500);
	prWifiVar->u4PerfMonTpTh[8] =
		(uint32_t) wlanCfgGetUint32(prAdapter, "PerfMonLv9", 600);
	prWifiVar->u4PerfMonTpTh[9] =
		(uint32_t) wlanCfgGetUint32(prAdapter, "PerfMonLv10", 700);
	prWifiVar->u4BoostCpuTh =
		(uint32_t) wlanCfgGetUint32(prAdapter, "BoostCpuTh", 1);

	/*
	 * For Certification purpose,forcibly set
	 * "Compressed Steering Number of Beamformer Antennas Supported" to our
	 * own capability.
	 */
	prWifiVar->fgForceSTSNum = (uint8_t)wlanCfgGetUint32(
					   prAdapter, "ForceSTSNum", 0);

#if CFG_SUPPORT_SPE_IDX_CONTROL
	prWifiVar->ucSpeIdxCtrl = (uint8_t) wlanCfgGetUint32(
					prAdapter, "SpeIdxCtrl", 2);
#if CFG_SUPPORT_COEX_NON_COTX
	prWifiVar->ucSpeIdxCtrl2g = (uint8_t) wlanCfgGetUint32(
			prAdapter, "SpeIdxCtrl2g", 2);
	prWifiVar->fgCoexNonCoTx = (uint8_t) wlanCfgGetUint32(
			prAdapter, "CoexNonCoTx", FEATURE_DISABLED);
#endif
#endif

#if CFG_SUPPORT_LOWLATENCY_MODE
	prWifiVar->ucLowLatencyModeScan = (uint32_t) wlanCfgGetUint32(
			prAdapter, "LowLatencyModeScan", FEATURE_ENABLED);
	prWifiVar->ucLowLatencyModeReOrder = (uint32_t) wlanCfgGetUint32(
			prAdapter, "LowLatencyModeReOrder", FEATURE_ENABLED);
	prWifiVar->ucLowLatencyModePower = (uint32_t) wlanCfgGetUint32(
			prAdapter, "LowLatencyModePower", FEATURE_ENABLED);
#endif /* CFG_SUPPORT_LOWLATENCY_MODE */

#ifdef CFG_SUPPORT_ADJUST_JOIN_CH_REQ_INTERVAL
	prWifiVar->u4AisJoinChReqIntervel =
		(uint32_t) wlanCfgGetUint32(prAdapter, "AisJoinChReqIntervel",
		AIS_JOIN_CH_REQUEST_INTERVAL);
	if (AIS_JOIN_CH_REQUEST_MAX_INTERVAL <
		prWifiVar->u4AisJoinChReqIntervel)
		prWifiVar->u4AisJoinChReqIntervel =
		AIS_JOIN_CH_REQUEST_MAX_INTERVAL;
#endif
	prWifiVar->ucEd2GNonEU = (int32_t) wlanCfgGetInt32(prAdapter,
		"Ed2GNonEU", ED_CCA_BW20_2G_DEFAULT);
	prWifiVar->ucEd5GNonEU = (int32_t) wlanCfgGetInt32(prAdapter,
		"Ed5GNonEU", ED_CCA_BW20_5G_DEFAULT);
	prWifiVar->ucEd2GEU = (int32_t) wlanCfgGetInt32(prAdapter,
		"Ed2GEU", ED_CCA_BW20_2G_DEFAULT);
	prWifiVar->ucEd5GEU = (int32_t) wlanCfgGetInt32(prAdapter,
		"Ed5GEU", ED_CCA_BW20_5G_DEFAULT);
	prWifiVar->ucEnforceCAM2G =
		(uint8_t) wlanCfgGetUint32(prAdapter, "EnforceCAM2G", 0);

}

void wlanCfgSetSwCtrl(IN struct ADAPTER *prAdapter)
{
	uint32_t i = 0;
	int8_t aucKey[WLAN_CFG_VALUE_LEN_MAX];
	int8_t aucValue[WLAN_CFG_VALUE_LEN_MAX];

	const int8_t acDelim[] = " ";
	int8_t *pcPtr = NULL;
	int8_t *pcDupValue = NULL;
	uint32_t au4Values[2];
	uint32_t u4TokenCount = 0;
	uint32_t u4BufLen = 0;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	struct GLUE_INFO *prGlueInfo = prAdapter->prGlueInfo;
	struct PARAM_CUSTOM_SW_CTRL_STRUCT rSwCtrlInfo;
	int32_t u4Ret = 0;

	for (i = 0; i < WLAN_CFG_SET_SW_CTRL_LEN_MAX; i++) {
		kalMemZero(aucValue, WLAN_CFG_VALUE_LEN_MAX);
		kalMemZero(aucKey, WLAN_CFG_VALUE_LEN_MAX);
		kalSnprintf(aucKey, sizeof(aucKey), "SwCtrl%d", i);

		/* get nothing */
		if (wlanCfgGet(prAdapter, aucKey, aucValue, "",
			       0) != WLAN_STATUS_SUCCESS)
			continue;
		if (!kalStrCmp(aucValue, ""))
			continue;

		pcDupValue = aucValue;
		u4TokenCount = 0;

		while ((pcPtr = kalStrSep((char **)(&pcDupValue), acDelim))
		       != NULL) {

			if (!kalStrCmp(pcPtr, ""))
				continue;

			/* au4Values[u4TokenCount] = kalStrtoul(pcPtr, NULL, 0);
			 */
			u4Ret = kalkStrtou32(pcPtr, 0,
					     &(au4Values[u4TokenCount]));
			if (u4Ret)
				DBGLOG(INIT, LOUD,
				       "parse au4Values error u4Ret=%d\n",
				       u4Ret);
			u4TokenCount++;

			/* Only need 2 tokens */
			if (u4TokenCount >= 2)
				break;
		}

		if (u4TokenCount != 2)
			continue;

		rSwCtrlInfo.u4Id = au4Values[0];
		rSwCtrlInfo.u4Data = au4Values[1];

		rStatus = kalIoctl(prGlueInfo, wlanoidSetSwCtrlWrite,
				   &rSwCtrlInfo, sizeof(rSwCtrlInfo),
				   FALSE, FALSE, TRUE, &u4BufLen);

	}
}

void wlanCfgSetChip(IN struct ADAPTER *prAdapter)
{
	uint32_t i = 0;
	int8_t aucKey[WLAN_CFG_VALUE_LEN_MAX];
	int8_t aucValue[WLAN_CFG_VALUE_LEN_MAX];

	uint32_t u4BufLen = 0;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	struct GLUE_INFO *prGlueInfo = prAdapter->prGlueInfo;
	struct PARAM_CUSTOM_CHIP_CONFIG_STRUCT rChipConfigInfo;

	for (i = 0; i < WLAN_CFG_SET_CHIP_LEN_MAX; i++) {
		kalMemZero(aucValue, WLAN_CFG_VALUE_LEN_MAX);
		kalMemZero(aucKey, WLAN_CFG_VALUE_LEN_MAX);
		kalSnprintf(aucKey, sizeof(aucKey), "SetChip%d", i);

		/* get nothing */
		if (wlanCfgGet(prAdapter, aucKey, aucValue, "",
			       0) != WLAN_STATUS_SUCCESS)
			continue;
		if (!kalStrCmp(aucValue, ""))
			continue;

		kalMemZero(&rChipConfigInfo, sizeof(rChipConfigInfo));

		rChipConfigInfo.ucType = CHIP_CONFIG_TYPE_WO_RESPONSE;
		rChipConfigInfo.u2MsgSize = kalStrnLen(aucValue,
						       WLAN_CFG_VALUE_LEN_MAX);
		kalStrnCpy(rChipConfigInfo.aucCmd, aucValue,
			   CHIP_CONFIG_RESP_SIZE);

		rStatus = kalIoctl(prGlueInfo, wlanoidSetChipConfig,
				   &rChipConfigInfo, sizeof(rChipConfigInfo),
				   FALSE, FALSE, TRUE, &u4BufLen);
	}

}

void wlanCfgSetDebugLevel(IN struct ADAPTER *prAdapter)
{
	uint32_t i = 0;
	int8_t aucKey[WLAN_CFG_VALUE_LEN_MAX];
	int8_t aucValue[WLAN_CFG_VALUE_LEN_MAX];
	const int8_t acDelim[] = " ";
	int8_t *pcDupValue;
	int8_t *pcPtr = NULL;

	uint32_t au4Values[2];
	uint32_t u4TokenCount = 0;
	uint32_t u4DbgIdx = 0;
	uint32_t u4DbgMask = 0;
	int32_t u4Ret = 0;

	for (i = 0; i < WLAN_CFG_SET_DEBUG_LEVEL_LEN_MAX; i++) {
		kalMemZero(aucValue, WLAN_CFG_VALUE_LEN_MAX);
		kalMemZero(aucKey, WLAN_CFG_VALUE_LEN_MAX);
		kalSnprintf(aucKey, sizeof(aucKey), "DbgLevel%d", i);

		/* get nothing */
		if (wlanCfgGet(prAdapter, aucKey, aucValue, "",
			       0) != WLAN_STATUS_SUCCESS)
			continue;
		if (!kalStrCmp(aucValue, ""))
			continue;

		pcDupValue = aucValue;
		u4TokenCount = 0;

		while ((pcPtr = kalStrSep((char **)(&pcDupValue),
					  acDelim)) != NULL) {

			if (!kalStrCmp(pcPtr, ""))
				continue;

			/* au4Values[u4TokenCount] =
			 *			kalStrtoul(pcPtr, NULL, 0);
			 */
			u4Ret = kalkStrtou32(pcPtr, 0,
					     &(au4Values[u4TokenCount]));
			if (u4Ret)
				DBGLOG(INIT, LOUD,
				       "parse au4Values error u4Ret=%d\n",
				       u4Ret);
			u4TokenCount++;

			/* Only need 2 tokens */
			if (u4TokenCount >= 2)
				break;
		}

		if (u4TokenCount != 2)
			continue;

		u4DbgIdx = au4Values[0];
		u4DbgMask = au4Values[1];

		/* DBG level special control */
		if (u4DbgIdx == 0xFFFFFFFF) {
			wlanSetDriverDbgLevel(DBG_ALL_MODULE_IDX, u4DbgMask);
			DBGLOG(INIT, INFO,
			       "Set ALL DBG module log level to [0x%02x]!",
			       (uint8_t) u4DbgMask);
		} else if (u4DbgIdx == 0xFFFFFFFE) {
			wlanDebugInit();
			DBGLOG(INIT, INFO,
			       "Reset ALL DBG module log level to DEFAULT!");
		} else if (u4DbgIdx < DBG_MODULE_NUM) {
			wlanSetDriverDbgLevel(u4DbgIdx, u4DbgMask);
			DBGLOG(INIT, INFO,
			       "Set DBG module[%u] log level to [0x%02x]!",
			       u4DbgIdx, (uint8_t) u4DbgMask);
		}
	}
}

void wlanCfgSetCountryCode(IN struct ADAPTER *prAdapter)
{
	int8_t aucValue[WLAN_CFG_VALUE_LEN_MAX];

	/* Apply COUNTRY Config */
	if (wlanCfgGet(prAdapter, "Country", aucValue, "",
		       0) == WLAN_STATUS_SUCCESS) {
		prAdapter->rWifiVar.rConnSettings.u2CountryCode =
			(((uint16_t) aucValue[0]) << 8) |
			((uint16_t) aucValue[1]);

		DBGLOG(INIT, INFO, "u2CountryCode=0x%04x\n",
		       prAdapter->rWifiVar.rConnSettings.u2CountryCode);

		if (regd_is_single_sku_en()) {
			rlmDomainOidSetCountry(prAdapter, aucValue, 2);
			return;
		}

		/* Force to re-search country code in regulatory domains */
		prAdapter->prDomainInfo = NULL;
		rlmDomainSendCmd(prAdapter);

		/* Update supported channel list in channel table based on
		 * current country domain
		 */
		wlanUpdateChannelTable(prAdapter->prGlueInfo);
	}
}

#if CFG_SUPPORT_CFG_FILE

struct WLAN_CFG_ENTRY *wlanCfgGetEntry(IN struct ADAPTER *prAdapter,
				       const int8_t *pucKey,
				       u_int8_t fgGetCfgRec)
{

	struct WLAN_CFG_ENTRY *prWlanCfgEntry;
	struct WLAN_CFG *prWlanCfg = NULL;
	struct WLAN_CFG_REC *prWlanCfgRec = NULL;
	uint32_t i, u32MaxNum;

	if (fgGetCfgRec) {
		prWlanCfgRec = prAdapter->prWlanCfgRec;
		u32MaxNum = WLAN_CFG_REC_ENTRY_NUM_MAX;
		ASSERT(prWlanCfgRec);
	} else {
		prWlanCfg = prAdapter->prWlanCfg;
		u32MaxNum = WLAN_CFG_ENTRY_NUM_MAX;
		ASSERT(prWlanCfg);
	}


	ASSERT(pucKey);

	prWlanCfgEntry = NULL;

	for (i = 0; i < u32MaxNum; i++) {
		if (fgGetCfgRec)
			prWlanCfgEntry = &prWlanCfgRec->arWlanCfgBuf[i];
		else
			prWlanCfgEntry = &prWlanCfg->arWlanCfgBuf[i];

		if (prWlanCfgEntry->aucKey[0] != '\0') {
			if (kalStrnCmp(pucKey, prWlanCfgEntry->aucKey,
				       WLAN_CFG_KEY_LEN_MAX - 1) == 0)
				return prWlanCfgEntry;
		}
	}

	return NULL;

}


struct WLAN_CFG_ENTRY *wlanCfgGetEntryByIndex(
	IN struct ADAPTER *prAdapter, const uint8_t ucIdx,
	uint32_t flag)
{

	struct WLAN_CFG_ENTRY *prWlanCfgEntry;
	struct WLAN_CFG *prWlanCfg;
	struct WLAN_CFG_REC *prWlanCfgRec;


	prWlanCfg = prAdapter->prWlanCfg;
	prWlanCfgRec = prAdapter->prWlanCfgRec;

	ASSERT(prWlanCfg);
	ASSERT(prWlanCfgRec);


	prWlanCfgEntry = NULL;

	if (flag & WLAN_CFG_REC_FLAG_BIT)
		prWlanCfgEntry = &prWlanCfgRec->arWlanCfgBuf[ucIdx];
	else
		prWlanCfgEntry = &prWlanCfg->arWlanCfgBuf[ucIdx];

	if (prWlanCfgEntry->aucKey[0] != '\0') {
		DBGLOG(INIT, LOUD, "get Index(%d) saved key %s\n", ucIdx,
		       prWlanCfgEntry->aucKey);
		return prWlanCfgEntry;
	}

	DBGLOG(INIT, TRACE,
	       "wifi config there is no entry at index(%d)\n", ucIdx);
	return NULL;

}



uint32_t wlanCfgGet(IN struct ADAPTER *prAdapter,
		    const int8_t *pucKey, int8_t *pucValue, int8_t *pucValueDef,
		    uint32_t u4Flags)
{

	struct WLAN_CFG_ENTRY *prWlanCfgEntry;
	struct WLAN_CFG *prWlanCfg;

	prWlanCfg = prAdapter->prWlanCfg;

	ASSERT(prWlanCfg);
	ASSERT(pucValue);

	/* Find the exist */
	prWlanCfgEntry = wlanCfgGetEntry(prAdapter, pucKey, FALSE);

	if (prWlanCfgEntry) {
		kalMemCopy(pucValue, prWlanCfgEntry->aucValue,
			   WLAN_CFG_VALUE_LEN_MAX - 1);
		return WLAN_STATUS_SUCCESS;
	}
	if (pucValueDef)
		kalMemCopy(pucValue, pucValueDef,
			   WLAN_CFG_VALUE_LEN_MAX - 1);
	return WLAN_STATUS_FAILURE;


}

void wlanCfgRecordValue(IN struct ADAPTER *prAdapter,
			const int8_t *pucKey, uint32_t u4Value)
{
	struct WLAN_CFG_ENTRY *prWlanCfgEntry;
	uint8_t aucBuf[WLAN_CFG_VALUE_LEN_MAX];

	prWlanCfgEntry = wlanCfgGetEntry(prAdapter, pucKey, TRUE);

	kalMemZero(aucBuf, sizeof(aucBuf));

	kalSnprintf(aucBuf, WLAN_CFG_VALUE_LEN_MAX, "0x%x",
		    (unsigned int)u4Value);

	wlanCfgSet(prAdapter, pucKey, aucBuf, 1);
}



uint32_t wlanCfgGetUint32(IN struct ADAPTER *prAdapter,
			  const int8_t *pucKey, uint32_t u4ValueDef)
{
	struct WLAN_CFG_ENTRY *prWlanCfgEntry;
	struct WLAN_CFG *prWlanCfg;
	uint32_t u4Value;
	int32_t u4Ret;

	prWlanCfg = prAdapter->prWlanCfg;

	ASSERT(prWlanCfg);

	u4Value = u4ValueDef;
	/* Find the exist */
	prWlanCfgEntry = wlanCfgGetEntry(prAdapter, pucKey, FALSE);

	if (prWlanCfgEntry) {
		/* u4Ret = kalStrtoul(prWlanCfgEntry->aucValue, NULL, 0); */
		u4Ret = kalkStrtou32(prWlanCfgEntry->aucValue, 0, &u4Value);
		if (u4Ret)
			DBGLOG(INIT, LOUD, "parse aucValue error u4Ret=%d\n",
			       u4Ret);
	}

	wlanCfgRecordValue(prAdapter, pucKey, u4Value);

	return u4Value;
}

int32_t wlanCfgGetInt32(IN struct ADAPTER *prAdapter,
			const int8_t *pucKey, int32_t i4ValueDef)
{
	struct WLAN_CFG_ENTRY *prWlanCfgEntry;
	struct WLAN_CFG *prWlanCfg;
	int32_t i4Value = 0;
	int32_t i4Ret = 0;

	prWlanCfg = prAdapter->prWlanCfg;

	ASSERT(prWlanCfg);

	i4Value = i4ValueDef;
	/* Find the exist */
	prWlanCfgEntry = wlanCfgGetEntry(prAdapter, pucKey, FALSE);

	if (prWlanCfgEntry) {
		/* i4Ret = kalStrtol(prWlanCfgEntry->aucValue, NULL, 0); */
		i4Ret = kalkStrtos32(prWlanCfgEntry->aucValue, 0, &i4Value);
		if (i4Ret)
			DBGLOG(INIT, LOUD, "parse aucValue error i4Ret=%d\n",
			       i4Ret);
	}

	wlanCfgRecordValue(prAdapter, pucKey, (uint32_t)i4Value);

	return i4Value;
}

uint32_t wlanCfgSet(IN struct ADAPTER *prAdapter,
		    const int8_t *pucKey, int8_t *pucValue, uint32_t u4Flags)
{

	struct WLAN_CFG_ENTRY *prWlanCfgEntry;
	struct WLAN_CFG *prWlanCfg = NULL;
	struct WLAN_CFG_REC *prWlanCfgRec = NULL;
	uint32_t u4EntryIndex;
	uint32_t i;
	uint8_t ucExist;
	u_int8_t fgGetCfgRec = FALSE;


	fgGetCfgRec = u4Flags & WLAN_CFG_REC_FLAG_BIT;

	ASSERT(pucKey);

	/* Find the exist */
	ucExist = 0;
	if (fgGetCfgRec) {
		prWlanCfgEntry = wlanCfgGetEntry(prAdapter, pucKey, TRUE);
		prWlanCfgRec = prAdapter->prWlanCfgRec;
		ASSERT(prWlanCfgRec);
	} else {
		prWlanCfgEntry = wlanCfgGetEntry(prAdapter, pucKey, FALSE);
		prWlanCfg = prAdapter->prWlanCfg;
		ASSERT(prWlanCfg);
	}

	if (!prWlanCfgEntry) {
		/* Find the empty */
		for (i = 0; i < WLAN_CFG_ENTRY_NUM_MAX; i++) {
			if (fgGetCfgRec)
				prWlanCfgEntry = &prWlanCfgRec->arWlanCfgBuf[i];
			else
				prWlanCfgEntry = &prWlanCfg->arWlanCfgBuf[i];
			if (prWlanCfgEntry->aucKey[0] == '\0')
				break;
		}

		u4EntryIndex = i;
		if (u4EntryIndex < WLAN_CFG_ENTRY_NUM_MAX) {
			if (fgGetCfgRec)
				prWlanCfgEntry =
				    &prWlanCfgRec->arWlanCfgBuf[u4EntryIndex];
			else
				prWlanCfgEntry =
				    &prWlanCfg->arWlanCfgBuf[u4EntryIndex];
			kalMemZero(prWlanCfgEntry,
				   sizeof(struct WLAN_CFG_ENTRY));
		} else {
			prWlanCfgEntry = NULL;
			DBGLOG(INIT, ERROR,
			       "wifi config there is no empty entry\n");
		}
	} /* !prWlanCfgEntry */
	else
		ucExist = 1;

	if (prWlanCfgEntry) {
		if (ucExist == 0) {
			kalStrnCpy(prWlanCfgEntry->aucKey, pucKey,
				   WLAN_CFG_KEY_LEN_MAX - 1);
			prWlanCfgEntry->aucKey[WLAN_CFG_KEY_LEN_MAX - 1] = '\0';
		}

		if (pucValue && pucValue[0] != '\0') {
			kalStrnCpy(prWlanCfgEntry->aucValue, pucValue,
				   WLAN_CFG_VALUE_LEN_MAX - 1);
			prWlanCfgEntry->aucValue[WLAN_CFG_VALUE_LEN_MAX - 1] =
									'\0';

			if (ucExist) {
				if (prWlanCfgEntry->pfSetCb)
					prWlanCfgEntry->pfSetCb(prAdapter,
						prWlanCfgEntry->aucKey,
						prWlanCfgEntry->aucValue,
						prWlanCfgEntry->pPrivate, 0);
			}
		} else {
			/* Call the pfSetCb if value is empty ? */
			/* remove the entry if value is empty */
			kalMemZero(prWlanCfgEntry,
				   sizeof(struct WLAN_CFG_ENTRY));
		}

	}
	/* prWlanCfgEntry */
	if (prWlanCfgEntry) {
		return WLAN_STATUS_SUCCESS;
	}
	if (pucKey)
		DBGLOG(INIT, ERROR, "Set wifi config error key \'%s\'\n",
		       pucKey);

	if (pucValue)
		DBGLOG(INIT, ERROR, "Set wifi config error value \'%s\'\n",
		       pucValue);

	return WLAN_STATUS_FAILURE;


}

uint32_t
wlanCfgSetCb(IN struct ADAPTER *prAdapter,
	     const int8_t *pucKey, WLAN_CFG_SET_CB pfSetCb,
	     void *pPrivate, uint32_t u4Flags)
{

	struct WLAN_CFG_ENTRY *prWlanCfgEntry;
	struct WLAN_CFG *prWlanCfg;

	prWlanCfg = prAdapter->prWlanCfg;
	ASSERT(prWlanCfg);

	/* Find the exist */
	prWlanCfgEntry = wlanCfgGetEntry(prAdapter, pucKey, FALSE);

	if (prWlanCfgEntry) {
		prWlanCfgEntry->pfSetCb = pfSetCb;
		prWlanCfgEntry->pPrivate = pPrivate;
	}

	if (prWlanCfgEntry)
		return WLAN_STATUS_SUCCESS;
	else
		return WLAN_STATUS_FAILURE;

}

uint32_t wlanCfgSetUint32(IN struct ADAPTER *prAdapter,
			  const int8_t *pucKey, uint32_t u4Value)
{

	struct WLAN_CFG *prWlanCfg;
	uint8_t aucBuf[WLAN_CFG_VALUE_LEN_MAX];

	prWlanCfg = prAdapter->prWlanCfg;

	ASSERT(prWlanCfg);

	kalMemZero(aucBuf, sizeof(aucBuf));

	kalSnprintf(aucBuf, WLAN_CFG_VALUE_LEN_MAX, "0x%x",
		    (unsigned int)u4Value);

	return wlanCfgSet(prAdapter, pucKey, aucBuf, 0);
}

enum {
	STATE_EOF = 0,
	STATE_TEXT = 1,
	STATE_NEWLINE = 2
};

struct WLAN_CFG_PARSE_STATE_S {
	int8_t *ptr;
	int8_t *text;
#if CFG_SUPPORT_EASY_DEBUG
	uint32_t textsize;
#endif
	int32_t nexttoken;
	uint32_t maxSize;
};

int32_t wlanCfgFindNextToken(struct WLAN_CFG_PARSE_STATE_S
			     *state)
{
	int8_t *x = state->ptr;
	int8_t *s;

	if (state->nexttoken) {
		int32_t t = state->nexttoken;

		state->nexttoken = 0;
		return t;
	}

	for (;;) {
		switch (*x) {
		case 0:
			state->ptr = x;
			return STATE_EOF;
		case '\n':
			x++;
			state->ptr = x;
			return STATE_NEWLINE;
		case ' ':
		case ',':
		/*case ':':  should not including : , mac addr would be fail*/
		case '\t':
		case '\r':
			x++;
			continue;
		case '#':
			while (*x && (*x != '\n'))
				x++;
			if (*x == '\n') {
				state->ptr = x + 1;
				return STATE_NEWLINE;
			}
			state->ptr = x;
			return STATE_EOF;

		default:
			goto text;
		}
	}

textdone:
	state->ptr = x;
	*s = 0;
	return STATE_TEXT;
text:
	state->text = s = x;
textresume:
	for (;;) {
		switch (*x) {
		case 0:
			goto textdone;
		case ' ':
		case ',':
		/* case ':': */
		case '\t':
		case '\r':
			x++;
			goto textdone;
		case '\n':
			state->nexttoken = STATE_NEWLINE;
			x++;
			goto textdone;
		case '"':
			x++;
			for (;;) {
				switch (*x) {
				case 0:
					/* unterminated quoted thing */
					state->ptr = x;
					return STATE_EOF;
				case '"':
					x++;
					goto textresume;
				default:
					*s++ = *x++;
				}
			}
			break;
		case '\\':
			x++;
			switch (*x) {
			case 0:
				goto textdone;
			case 'n':
				*s++ = '\n';
				break;
			case 'r':
				*s++ = '\r';
				break;
			case 't':
				*s++ = '\t';
				break;
			case '\\':
				*s++ = '\\';
				break;
			case '\r':
				/* \ <cr> <lf> -> line continuation */
				if (x[1] != '\n') {
					x++;
					continue;
				}
			case '\n':
				/* \ <lf> -> line continuation */
				x++;
				/* eat any extra whitespace */
				while ((*x == ' ') || (*x == '\t'))
					x++;
				continue;
			default:
				/* unknown escape -- just copy */
				*s++ = *x++;
			}
			continue;
		default:
			*s++ = *x++;
#if CFG_SUPPORT_EASY_DEBUG
			state->textsize++;
#endif
		}
	}
	return STATE_EOF;
}

uint32_t wlanCfgParseArgument(int8_t *cmdLine,
			      int32_t *argc, int8_t *argv[])
{
	struct WLAN_CFG_PARSE_STATE_S state;
	int8_t **args;
	int32_t nargs;

	if (cmdLine == NULL || argc == NULL || argv == NULL) {
		DBGLOG(INIT, ERROR, "parameter is NULL: %p, %p, %p\n",
		       cmdLine, argc, argv);
		return WLAN_STATUS_FAILURE;
	}
	args = argv;
	nargs = 0;
	state.ptr = cmdLine;
	state.nexttoken = 0;
	state.maxSize = 0;
#if CFG_SUPPORT_EASY_DEBUG
	state.textsize = 0;
#endif

	if (kalStrnLen(cmdLine, 512) >= 512) {
		DBGLOG(INIT, ERROR, "cmdLine >= 512\n");
		return WLAN_STATUS_FAILURE;
	}

	for (;;) {
		switch (wlanCfgFindNextToken(&state)) {
		case STATE_EOF:
			goto exit;
		case STATE_NEWLINE:
			goto exit;
		case STATE_TEXT:
			if (nargs < WLAN_CFG_ARGV_MAX)
				args[nargs++] = state.text;
			break;
		}
	}

exit:
	*argc = nargs;
	return WLAN_STATUS_SUCCESS;
}

#if CFG_WOW_SUPPORT
uint32_t wlanCfgParseArgumentLong(int8_t *cmdLine,
				  int32_t *argc, int8_t *argv[])
{
	struct WLAN_CFG_PARSE_STATE_S state;
	int8_t **args;
	int32_t nargs;

	if (cmdLine == NULL || argc == NULL || argv == NULL) {
		DBGLOG(INIT, ERROR, "parameter is NULL: %p, %p, %p\n",
		       cmdLine, argc, argv);
		return WLAN_STATUS_FAILURE;
	}
	args = argv;
	nargs = 0;
	state.ptr = cmdLine;
	state.nexttoken = 0;
	state.maxSize = 0;
#if CFG_SUPPORT_EASY_DEBUG
	state.textsize = 0;
#endif

	if (kalStrnLen(cmdLine, 512) >= 512) {
		DBGLOG(INIT, ERROR, "cmdLine >= 512\n");
		return WLAN_STATUS_FAILURE;
	}

	for (;;) {
		switch (wlanCfgFindNextToken(&state)) {
		case STATE_EOF:
			goto exit;
		case STATE_NEWLINE:
			goto exit;
		case STATE_TEXT:
			if (nargs < WLAN_CFG_ARGV_MAX_LONG)
				args[nargs++] = state.text;
			break;
		}
	}

exit:
	*argc = nargs;
	return WLAN_STATUS_SUCCESS;
}
#endif

uint32_t
wlanCfgParseAddEntry(IN struct ADAPTER *prAdapter,
		     uint8_t *pucKeyHead, uint8_t *pucKeyTail,
		     uint8_t *pucValueHead, uint8_t *pucValueTail)
{

	uint8_t aucKey[WLAN_CFG_KEY_LEN_MAX];
	uint8_t aucValue[WLAN_CFG_VALUE_LEN_MAX];
	uint32_t u4Len;

	kalMemZero(aucKey, sizeof(aucKey));
	kalMemZero(aucValue, sizeof(aucValue));

	if ((pucKeyHead == NULL)
	    || (pucValueHead == NULL)
	   )
		return WLAN_STATUS_FAILURE;

	if (pucKeyTail) {
		if (pucKeyHead > pucKeyTail)
			return WLAN_STATUS_FAILURE;
		u4Len = pucKeyTail - pucKeyHead + 1;
	} else
		u4Len = kalStrnLen(pucKeyHead, WLAN_CFG_KEY_LEN_MAX - 1);

	if (u4Len >= WLAN_CFG_KEY_LEN_MAX)
		u4Len = WLAN_CFG_KEY_LEN_MAX - 1;

	kalStrnCpy(aucKey, pucKeyHead, u4Len);

	if (pucValueTail) {
		if (pucValueHead > pucValueTail)
			return WLAN_STATUS_FAILURE;
		u4Len = pucValueTail - pucValueHead + 1;
	} else
		u4Len = kalStrnLen(pucValueHead,
				   WLAN_CFG_VALUE_LEN_MAX - 1);

	if (u4Len >= WLAN_CFG_VALUE_LEN_MAX)
		u4Len = WLAN_CFG_VALUE_LEN_MAX - 1;

	kalStrnCpy(aucValue, pucValueHead, u4Len);

	return wlanCfgSet(prAdapter, aucKey, aucValue, 0);
}

enum {
	WAIT_KEY_HEAD = 0,
	WAIT_KEY_TAIL,
	WAIT_VALUE_HEAD,
	WAIT_VALUE_TAIL,
	WAIT_COMMENT_TAIL
};

#if CFG_SUPPORT_EASY_DEBUG

int8_t atoi(uint8_t ch)
{
	if (ch >= 'a' && ch <= 'f')
		return ch - 87;
	else if (ch >= 'A' && ch <= 'F')
		return ch - 55;
	else if (ch >= '0' && ch <= '9')
		return ch - 48;

	return 0;
}

uint32_t wlanCfgParseToFW(int8_t **args, int8_t *args_size,
			  uint8_t nargs, int8_t *buffer, uint8_t times)
{
	uint8_t *data = NULL;
	char ch;
	int32_t i = 0, j = 0;
	uint32_t bufferindex = 0, base = 0;
	uint32_t sum = 0, startOffset = 0;
	struct CMD_FORMAT_V1 cmd_v1;

	memset(&cmd_v1, 0, sizeof(struct CMD_FORMAT_V1));

#if 0
	cmd_v1.itemType = atoi(*args[ED_ITEMTYPE_SITE]);
#else
	cmd_v1.itemType = ITEM_TYPE_DEC;
#endif
	if (buffer == NULL ||
	    args_size[ED_STRING_SITE] == 0 ||
	    args_size[ED_VALUE_SITE] == 0 ||
	    (cmd_v1.itemType < ITEM_TYPE_DEC
	     || cmd_v1.itemType > ITEM_TYPE_STR)) {
		DBGLOG(INIT, ERROR, "cfg args wrong\n");
		return WLAN_STATUS_FAILURE;
	}

	cmd_v1.itemStringLength = args_size[ED_STRING_SITE];
	strncpy(cmd_v1.itemString, args[ED_STRING_SITE],
		cmd_v1.itemStringLength);
	DBGLOG(INIT, INFO, "itemString:");
	for (i = 0; i <  cmd_v1.itemStringLength; i++)
		DBGLOG(INIT, INFO, "%c", cmd_v1.itemString[i]);
	DBGLOG(INIT, INFO, "\n");

	DBGLOG(INIT, INFO, "cmd_v1.itemType = %d\n",
	       cmd_v1.itemType);
	if (cmd_v1.itemType == ITEM_TYPE_DEC
	    || cmd_v1.itemType == ITEM_TYPE_HEX) {
		data = args[ED_VALUE_SITE];

		switch (cmd_v1.itemType) {
		case ITEM_TYPE_DEC:
			base = 10;
			startOffset = 0;
			break;
		case ITEM_TYPE_HEX:
			ch = *data;
			if (args_size[ED_VALUE_SITE] < 3 || ch != '0') {
				DBGLOG(INIT, WARN,
				       "Hex args must have prefix '0x'\n");
				return WLAN_STATUS_FAILURE;
			}

			data++;
			ch = *data;
			if (ch != 'x' && ch != 'X') {
				DBGLOG(INIT, WARN,
				       "Hex args must have prefix '0x'\n");
				return WLAN_STATUS_FAILURE;
			}
			data++;
			base = 16;
			startOffset = 2;
			break;
		}

		for (j = args_size[ED_VALUE_SITE] - 1 - startOffset; j >= 0;
		     j--) {
			sum = sum * base + atoi(*data);
			DBGLOG(INIT, WARN, "size:%d data[%d]=%u, sum=%u\n",
			       args_size[ED_VALUE_SITE], j, atoi(*data), sum);

			data++;
		}

		bufferindex = 0;
		do {
			cmd_v1.itemValue[bufferindex++] = sum & 0xFF;
			sum = sum >> 8;
		} while (sum > 0);
		cmd_v1.itemValueLength = bufferindex;
	} else if (cmd_v1.itemType == ITEM_TYPE_STR) {
		cmd_v1.itemValueLength = args_size[ED_VALUE_SITE];
		strncpy(cmd_v1.itemValue, args[ED_VALUE_SITE],
			cmd_v1.itemValueLength);
	}

	DBGLOG(INIT, INFO, "Length = %d itemValue:",
	       cmd_v1.itemValueLength);
	for (i = cmd_v1.itemValueLength - 1; i >= 0; i--)
		DBGLOG(INIT, ERROR, "%d,", cmd_v1.itemValue[i]);
	DBGLOG(INIT, INFO, "\n");
	memcpy(((struct CMD_FORMAT_V1 *)buffer) + times, &cmd_v1,
	       sizeof(struct CMD_FORMAT_V1));

	return WLAN_STATUS_SUCCESS;
}


/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is to send WLAN feature options to firmware
 *
 * @param prAdapter  Pointer of ADAPTER_T
 *
 * @return none
 */
/*----------------------------------------------------------------------------*/
void wlanFeatureToFw(IN struct ADAPTER *prAdapter)
{

	struct WLAN_CFG_ENTRY *prWlanCfgEntry;
	uint32_t i;
	struct CMD_HEADER rCmdV1Header;
	uint32_t rStatus;
	struct CMD_FORMAT_V1 rCmd_v1;
	uint8_t  ucTimes = 0;



	rCmdV1Header.cmdType = CMD_TYPE_SET;
	rCmdV1Header.cmdVersion = CMD_VER_1;
	rCmdV1Header.cmdBufferLen = 0;
	rCmdV1Header.itemNum = 0;

	kalMemSet(rCmdV1Header.buffer, 0, MAX_CMD_BUFFER_LENGTH);
	kalMemSet(&rCmd_v1, 0, sizeof(struct CMD_FORMAT_V1));


	prWlanCfgEntry = NULL;

	for (i = 0; i < WLAN_CFG_ENTRY_NUM_MAX; i++) {

		prWlanCfgEntry = wlanCfgGetEntryByIndex(prAdapter, i, 0);

		if (prWlanCfgEntry) {

			rCmd_v1.itemType = ITEM_TYPE_STR;


			/*send string format to firmware */
			rCmd_v1.itemStringLength = kalStrLen(
							prWlanCfgEntry->aucKey);
			kalMemZero(rCmd_v1.itemString, MAX_CMD_NAME_MAX_LENGTH);
			kalMemCopy(rCmd_v1.itemString, prWlanCfgEntry->aucKey,
				   rCmd_v1.itemStringLength);


			rCmd_v1.itemValueLength = kalStrLen(
						  prWlanCfgEntry->aucValue);
			kalMemZero(rCmd_v1.itemValue, MAX_CMD_VALUE_MAX_LENGTH);
			kalMemCopy(rCmd_v1.itemValue, prWlanCfgEntry->aucValue,
				   rCmd_v1.itemValueLength);



			DBGLOG(INIT, WARN,
			       "Send key word (%s) WITH (%s) to firmware\n",
			       rCmd_v1.itemString, rCmd_v1.itemValue);

			kalMemCopy(((struct CMD_FORMAT_V1 *)rCmdV1Header.buffer)
				   + ucTimes,
				   &rCmd_v1, sizeof(struct CMD_FORMAT_V1));


			ucTimes++;
			rCmdV1Header.cmdBufferLen +=
						sizeof(struct CMD_FORMAT_V1);
			rCmdV1Header.itemNum += ucTimes;

			if (ucTimes == MAX_CMD_ITEM_MAX) {
				/* Send to FW */
				rCmdV1Header.itemNum = ucTimes;

				rStatus = wlanSendSetQueryCmd(
						/* prAdapter */
						prAdapter,
						/* 0x70 */
						CMD_ID_GET_SET_CUSTOMER_CFG,
						/* fgSetQuery */
						TRUE,
						/* fgNeedResp */
						FALSE,
						/* fgIsOid */
						FALSE,
						/* pfCmdDoneHandler*/
						NULL,
						/* pfCmdTimeoutHandler */
						NULL,
						/* u4SetQueryInfoLen */
						sizeof(struct CMD_HEADER),
						/* pucInfoBuffer */
						(uint8_t *)&rCmdV1Header,
						/* pvSetQueryBuffer */
						NULL,
						/* u4SetQueryBufferLen */
						0);

				if (rStatus == WLAN_STATUS_FAILURE)
					DBGLOG(INIT, INFO,
					       "[Fail]kalIoctl wifiSefCFG fail 0x%x\n",
					       rStatus);

				DBGLOG(INIT, INFO,
				       "kalIoctl wifiSefCFG num:%d\n", ucTimes);
				kalMemSet(rCmdV1Header.buffer, 0,
					  MAX_CMD_BUFFER_LENGTH);
				rCmdV1Header.cmdBufferLen = 0;
				ucTimes = 0;
			}


		} else {
			break;
		}
	}


	if (ucTimes != 0) {
		/* Send to FW */
		rCmdV1Header.itemNum = ucTimes;

		DBGLOG(INIT, INFO, "cmdV1Header.itemNum:%d\n",
		       rCmdV1Header.itemNum);

		rStatus = wlanSendSetQueryCmd(
			  prAdapter,	/* prAdapter */
			  CMD_ID_GET_SET_CUSTOMER_CFG,	/* 0x70 */
			  TRUE,		/* fgSetQuery */
			  FALSE,	/* fgNeedResp */
			  FALSE,	/* fgIsOid */
			  NULL,		/* pfCmdDoneHandler*/
			  NULL,		/* pfCmdTimeoutHandler */
			  sizeof(struct CMD_HEADER),	/* u4SetQueryInfoLen */
			  (uint8_t *)&rCmdV1Header,/* pucInfoBuffer */
			  NULL,	/* pvSetQueryBuffer */
			  0);	/* u4SetQueryBufferLen */

		if (rStatus == WLAN_STATUS_FAILURE)
			DBGLOG(INIT, INFO,
			       "[Fail]kalIoctl wifiSefCFG fail 0x%x\n",
			       rStatus);

		DBGLOG(INIT, INFO, "kalIoctl wifiSefCFG num:%d\n", ucTimes);
		kalMemSet(rCmdV1Header.buffer, 0, MAX_CMD_BUFFER_LENGTH);
		rCmdV1Header.cmdBufferLen = 0;
		ucTimes = 0;
	}

}



uint32_t wlanCfgParse(IN struct ADAPTER *prAdapter,
		      uint8_t *pucConfigBuf, uint32_t u4ConfigBufLen,
		      u_int8_t isFwConfig)
{
	struct WLAN_CFG_PARSE_STATE_S state;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	int8_t **ppcArgs;
	int32_t i4Nargs;
	int8_t   arcArgv_size[WLAN_CFG_ARGV_MAX];
	uint8_t  ucTimes = 0;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct CMD_HEADER rCmdV1Header;
	int8_t ucTmp[WLAN_CFG_VALUE_LEN_MAX];
	uint8_t i;

	uint8_t *pucCurrBuf = ucTmp;
	uint32_t u4CurrSize = ARRAY_SIZE(ucTmp);
	uint32_t u4RetSize = 0;

	rCmdV1Header.cmdType = CMD_TYPE_SET;
	rCmdV1Header.cmdVersion = CMD_VER_1;
	rCmdV1Header.cmdBufferLen = 0;
	kalMemSet(rCmdV1Header.buffer, 0, MAX_CMD_BUFFER_LENGTH);

	if (pucConfigBuf == NULL) {
		DBGLOG(INIT, ERROR, "pucConfigBuf is NULL\n");
		return WLAN_STATUS_FAILURE;
	}
	if (kalStrnLen(pucConfigBuf, 4000) >= 4000) {
		DBGLOG(INIT, ERROR, "pucConfigBuf >= 4000\n");
		return WLAN_STATUS_FAILURE;
	}
	if (u4ConfigBufLen == 0)
		return WLAN_STATUS_FAILURE;

	ppcArgs = apcArgv;
	i4Nargs = 0;
	state.ptr = pucConfigBuf;
	state.nexttoken = 0;
	state.textsize = 0;
	state.maxSize = u4ConfigBufLen;
	DBGLOG(INIT, INFO, "wlanCfgParse()\n");

	for (;;) {
		switch (wlanCfgFindNextToken(&state)) {
		case STATE_EOF:
			if (i4Nargs < 2)
				goto exit;

			DBGLOG(INIT, INFO, "STATE_EOF\n");

			/* 3 parmeter mode transforation */
			if (i4Nargs == 3 && !isFwConfig &&
			    arcArgv_size[0] == 1) {

				/* parsing and transfer the format
				 * Format  1:Dec 2.Hex 3.String
				 */

				kalMemZero(ucTmp, WLAN_CFG_VALUE_LEN_MAX);
				pucCurrBuf = ucTmp;
				u4CurrSize = ARRAY_SIZE(ucTmp);

				if ((*ppcArgs[0] == '2') &&
				    (*(ppcArgs[2]) != '0') &&
				    (*(ppcArgs[2] + 1) != 'x')) {
					DBGLOG(INIT, WARN,
					       "config file got a hex format\n"
					       );
					kalSnprintf(pucCurrBuf, u4CurrSize,
						    "0x%s", ppcArgs[2]);
				} else {
					kalSnprintf(pucCurrBuf, u4CurrSize,
						    "%s", ppcArgs[2]);
				}
				DBGLOG(INIT, WARN,
				       "[3 parameter mode][%s],[%s],[%s]\n",
				       ppcArgs[0], ppcArgs[1], ucTmp);
				wlanCfgParseAddEntry(prAdapter, ppcArgs[1],
						     NULL, ucTmp, NULL);
				kalMemSet(arcArgv_size, 0, WLAN_CFG_ARGV_MAX);
				kalMemSet(apcArgv, 0,
					WLAN_CFG_ARGV_MAX * sizeof(int8_t *));
				i4Nargs = 0;
				goto exit;

			}

			wlanCfgParseAddEntry(prAdapter, ppcArgs[0], NULL,
					     ppcArgs[1], NULL);

			if (isFwConfig) {
				uint32_t ret;

				ret = wlanCfgParseToFW(ppcArgs, arcArgv_size,
						       i4Nargs,
						       rCmdV1Header.buffer,
						       ucTimes);
				if (ret == WLAN_STATUS_SUCCESS) {
					ucTimes++;
					rCmdV1Header.cmdBufferLen +=
						sizeof(struct CMD_FORMAT_V1);
				}
			}

			goto exit;


		case STATE_NEWLINE:
			if (i4Nargs < 2)
				break;

			DBGLOG(INIT, INFO, "STATE_NEWLINE\n");
#if 1
			/* 3 parmeter mode transforation */
			if (i4Nargs == 3 && !isFwConfig &&
			    arcArgv_size[0] == 1) {
				/* parsing and transfer the format
				 * Format  1:Dec 2.Hex 3.String
				 */
				kalMemZero(ucTmp, WLAN_CFG_VALUE_LEN_MAX);
				pucCurrBuf = ucTmp;
				u4CurrSize = ARRAY_SIZE(ucTmp);

				if ((*ppcArgs[0] == '2') &&
				    (*(ppcArgs[2]) != '0') &&
				    (*(ppcArgs[2] + 1) != 'x')) {
					DBGLOG(INIT, WARN,
					       "config file got a hex format\n");
					kalSnprintf(pucCurrBuf, u4CurrSize,
						    "0x%s", ppcArgs[2]);

				} else {
					kalSnprintf(pucCurrBuf, u4CurrSize,
						    "%s", ppcArgs[2]);
				}


				DBGLOG(INIT, WARN,
				       "[3 parameter mode][%s],[%s],[%s]\n",
				       ppcArgs[0], ppcArgs[1], ucTmp);
				wlanCfgParseAddEntry(prAdapter, ppcArgs[1],
						     NULL, ucTmp, NULL);
				kalMemSet(arcArgv_size, 0, WLAN_CFG_ARGV_MAX);
				kalMemSet(apcArgv, 0,
					WLAN_CFG_ARGV_MAX * sizeof(int8_t *));
				i4Nargs = 0;
				break;

			}
#if 1
			/*combine the argument to save in temp*/
			pucCurrBuf = ucTmp;
			u4CurrSize = ARRAY_SIZE(ucTmp);

			kalMemZero(ucTmp, WLAN_CFG_VALUE_LEN_MAX);

			if (i4Nargs == 2) {
				/* no space for it, driver can't accept space in
				 * the end of the line
				 */
				/* ToDo: skip the space when parsing */
				kalSnprintf(pucCurrBuf, u4CurrSize, "%s",
					    ppcArgs[1]);
			} else {
				for (i = 1; i < i4Nargs; i++) {
					if (u4CurrSize <= 1) {
						DBGLOG(INIT, ERROR,
						       "write to pucCurrBuf out of bound, i=%d\n",
						       i);
						break;
					}
					u4RetSize = scnprintf(pucCurrBuf,
							      u4CurrSize, "%s ",
							      ppcArgs[i]);
					pucCurrBuf += u4RetSize;
					u4CurrSize -= u4RetSize;
				}
			}

			DBGLOG(INIT, WARN,
			       "Save to driver temp buffer as [%s]\n",
			       ucTmp);
			wlanCfgParseAddEntry(prAdapter, ppcArgs[0], NULL, ucTmp,
					     NULL);
#else
			wlanCfgParseAddEntry(prAdapter, ppcArgs[0], NULL,
					     ppcArgs[1], NULL);
#endif

			if (isFwConfig) {

				uint32_t ret;

				ret = wlanCfgParseToFW(ppcArgs, arcArgv_size,
					i4Nargs, rCmdV1Header.buffer, ucTimes);
				if (ret == WLAN_STATUS_SUCCESS) {
					ucTimes++;
					rCmdV1Header.cmdBufferLen +=
						sizeof(struct CMD_FORMAT_V1);
				}

				if (ucTimes == MAX_CMD_ITEM_MAX) {
					/* Send to FW */
					rCmdV1Header.itemNum = ucTimes;
					rStatus = wlanSendSetQueryCmd(
						/* prAdapter */
						prAdapter,
						/* 0x70 */
						CMD_ID_GET_SET_CUSTOMER_CFG,
						/* fgSetQuery */
						TRUE,
						/* fgNeedResp */
						FALSE,
						/* fgIsOid */
						FALSE,
						/* pfCmdDoneHandler*/
						NULL,
						/* pfCmdTimeoutHandler */
						NULL,
						/* u4SetQueryInfoLen */
						sizeof(struct CMD_HEADER),
						/* pucInfoBuffer */
						(uint8_t *) &rCmdV1Header,
						/* pvSetQueryBuffer */
						NULL,
						/* u4SetQueryBufferLen */
						0);

					if (rStatus == WLAN_STATUS_FAILURE)
						DBGLOG(INIT, INFO,
						       "kalIoctl wifiSefCFG fail 0x%x\n",
						       rStatus);
					DBGLOG(INIT, INFO,
					       "kalIoctl wifiSefCFG num:%d X\n",
					       ucTimes);
					kalMemSet(rCmdV1Header.buffer, 0,
						  MAX_CMD_BUFFER_LENGTH);
					rCmdV1Header.cmdBufferLen = 0;
					ucTimes = 0;
				}

			}

#endif
			kalMemSet(arcArgv_size, 0, WLAN_CFG_ARGV_MAX);
			kalMemSet(apcArgv, 0,
				WLAN_CFG_ARGV_MAX * sizeof(int8_t *));
			i4Nargs = 0;
			break;

		case STATE_TEXT:
			if (i4Nargs < WLAN_CFG_ARGV_MAX) {
				ppcArgs[i4Nargs++] = state.text;
				arcArgv_size[i4Nargs - 1] = state.textsize;
				state.textsize = 0;
				DBGLOG(INIT, INFO,
				       " nargs= %d STATE_TEXT = %s, SIZE = %d\n",
				       i4Nargs - 1, ppcArgs[i4Nargs - 1],
				       arcArgv_size[i4Nargs - 1]);
			}
			break;
		}
	}

exit:
	if (ucTimes != 0 && isFwConfig) {
		/* Send to FW */
		rCmdV1Header.itemNum = ucTimes;

		DBGLOG(INIT, INFO, "cmdV1Header.itemNum:%d\n",
		       rCmdV1Header.itemNum);
		rStatus = wlanSendSetQueryCmd(
			  prAdapter,	/* prAdapter */
			  CMD_ID_GET_SET_CUSTOMER_CFG,	/* 0x70 */
			  TRUE,		/* fgSetQuery */
			  FALSE,	/* fgNeedResp */
			  FALSE,	/* fgIsOid */
			  NULL,		/* pfCmdDoneHandler*/
			  NULL,		/* pfCmdTimeoutHandler */
			  sizeof(struct CMD_HEADER), /* u4SetQueryInfoLen */
			  (uint8_t *) &rCmdV1Header, /* pucInfoBuffer */
			  NULL,		/* pvSetQueryBuffer */
			  0		/* u4SetQueryBufferLen */
			  );

		if (rStatus == WLAN_STATUS_FAILURE)
			DBGLOG(INIT, WARN, "kalIoctl wifiSefCFG fail 0x%x\n",
			       rStatus);

		DBGLOG(INIT, WARN, "kalIoctl wifiSefCFG num:%d X\n",
		       ucTimes);
		kalMemSet(rCmdV1Header.buffer, 0, MAX_CMD_BUFFER_LENGTH);
		rCmdV1Header.cmdBufferLen = 0;
		ucTimes = 0;
	}
	return WLAN_STATUS_SUCCESS;
}

#else
uint32_t wlanCfgParse(IN struct ADAPTER *prAdapter,
		      uint8_t *pucConfigBuf, uint32_t u4ConfigBufLen)
{

	struct WLAN_CFG_PARSE_STATE_S state;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	int8_t **args;
	int32_t nargs;

	if (pucConfigBuf == NULL) {
		DBGLOG(INIT, ERROR, "pucConfigBuf is NULL\n");
		return WLAN_STATUS_FAILURE;
	}
	if (kalStrnLen(pucConfigBuf, 4000) >= 4000) {
		DBGLOG(INIT, ERROR, "pucConfigBuf >= 4000\n");
		return WLAN_STATUS_FAILURE;
	}
	if (u4ConfigBufLen == 0)
		return WLAN_STATUS_FAILURE;
	args = apcArgv;
	nargs = 0;
	state.ptr = pucConfigBuf;
	state.nexttoken = 0;
	state.maxSize = u4ConfigBufLen;

	for (;;) {
		switch (wlanCfgFindNextToken(&state)) {
		case STATE_EOF:
			if (nargs > 1)
				wlanCfgParseAddEntry(prAdapter, args[0], NULL,
						     args[1], NULL);
			goto exit;
		case STATE_NEWLINE:
			if (nargs > 1)
				wlanCfgParseAddEntry(prAdapter, args[0], NULL,
						     args[1], NULL);
			/*args[0] is parameter, args[1] is the value*/
			nargs = 0;
			break;
		case STATE_TEXT:
			if (nargs < WLAN_CFG_ARGV_MAX)
				args[nargs++] = state.text;
			break;
		}
	}

exit:
	return WLAN_STATUS_SUCCESS;

#if 0
	/* Old version */
	uint32_t i;
	uint8_t c;
	uint8_t *pbuf;
	uint8_t ucState;
	uint8_t *pucKeyTail = NULL;
	uint8_t *pucKeyHead = NULL;
	uint8_t *pucValueHead = NULL;
	uint8_t *pucValueTail = NULL;

	ucState = WAIT_KEY_HEAD;
	pbuf = pucConfigBuf;

	for (i = 0; i < u4ConfigBufLen; i++) {
		c = pbuf[i];
		if (c == '\r' || c == '\n') {

			if (ucState == WAIT_VALUE_TAIL) {
				/* Entry found */
				if (pucValueHead)
					wlanCfgParseAddEntry(prAdapter,
						pucKeyHead, pucKeyTail,
						pucValueHead, pucValueTail);
			}
			ucState = WAIT_KEY_HEAD;
			pucKeyTail = NULL;
			pucKeyHead = NULL;
			pucValueHead = NULL;
			pucValueTail = NULL;

		} else if (c == '=') {
			if (ucState == WAIT_KEY_TAIL) {
				pucKeyTail = &pbuf[i - 1];
				ucState = WAIT_VALUE_HEAD;
			}
		} else if (c == ' ' || c == '\t') {
			if (ucState == WAIT_KEY_TAIL) {
				pucKeyTail = &pbuf[i - 1];
				ucState = WAIT_VALUE_HEAD;
			}
		} else {

			if (c == '#') {
				/* comments */
				if (ucState == WAIT_KEY_HEAD)
					ucState = WAIT_COMMENT_TAIL;
				else if (ucState == WAIT_VALUE_TAIL)
					pucValueTail = &pbuf[i];

			} else {
				if (ucState == WAIT_KEY_HEAD) {
					pucKeyHead = &pbuf[i];
					pucKeyTail = &pbuf[i];
					ucState = WAIT_KEY_TAIL;
				} else if (ucState == WAIT_VALUE_HEAD) {
					pucValueHead = &pbuf[i];
					pucValueTail = &pbuf[i];
					ucState = WAIT_VALUE_TAIL;
				} else if (ucState == WAIT_VALUE_TAIL)
					pucValueTail = &pbuf[i];
			}
		}

	}			/* for */

	if (ucState == WAIT_VALUE_TAIL) {
		/* Entry found */
		if (pucValueTail)
			wlanCfgParseAddEntry(prAdapter, pucKeyHead, pucKeyTail,
					     pucValueHead, pucValueTail);
	}
#endif

	return WLAN_STATUS_SUCCESS;
}
#endif


uint32_t wlanCfgInit(IN struct ADAPTER *prAdapter,
		     uint8_t *pucConfigBuf, uint32_t u4ConfigBufLen,
		     uint32_t u4Flags)
{
	struct WLAN_CFG *prWlanCfg;
	struct WLAN_CFG_REC *prWlanCfgRec;
	/* P_WLAN_CFG_ENTRY_T prWlanCfgEntry; */
	prAdapter->prWlanCfg = &prAdapter->rWlanCfg;
	prWlanCfg = prAdapter->prWlanCfg;

	prAdapter->prWlanCfgRec = &prAdapter->rWlanCfgRec;
	prWlanCfgRec = prAdapter->prWlanCfgRec;

	kalMemZero(prWlanCfg, sizeof(struct WLAN_CFG));
	ASSERT(prWlanCfg);
	prWlanCfg->u4WlanCfgEntryNumMax = WLAN_CFG_ENTRY_NUM_MAX;
	prWlanCfg->u4WlanCfgKeyLenMax = WLAN_CFG_KEY_LEN_MAX;
	prWlanCfg->u4WlanCfgValueLenMax = WLAN_CFG_VALUE_LEN_MAX;

	prWlanCfgRec->u4WlanCfgEntryNumMax =
		WLAN_CFG_REC_ENTRY_NUM_MAX;
	prWlanCfgRec->u4WlanCfgKeyLenMax =
		WLAN_CFG_KEY_LEN_MAX;
	prWlanCfgRec->u4WlanCfgValueLenMax =
		WLAN_CFG_VALUE_LEN_MAX;

	DBGLOG(INIT, INFO, "Init wifi config len %u max entry %u\n",
	       u4ConfigBufLen, prWlanCfg->u4WlanCfgEntryNumMax);
#if DBG
	/* self test */
	wlanCfgSet(prAdapter, "ConfigValid", "0x123", 0);
	if (wlanCfgGetUint32(prAdapter, "ConfigValid", 0) != 0x123)
		DBGLOG(INIT, INFO, "wifi config error %u\n", __LINE__);

	wlanCfgSet(prAdapter, "ConfigValid", "1", 0);
	if (wlanCfgGetUint32(prAdapter, "ConfigValid", 0) != 1)
		DBGLOG(INIT, INFO, "wifi config error %u\n", __LINE__);

#endif
	/*load default value because kalMemZero in this function*/
	wlanLoadDefaultCustomerSetting(prAdapter);

	/* Parse the pucConfigBuf */
	if (pucConfigBuf && (u4ConfigBufLen > 0))
#if CFG_SUPPORT_EASY_DEBUG
		wlanCfgParse(prAdapter, pucConfigBuf, u4ConfigBufLen,
			     FALSE);
#else
		wlanCfgParse(prAdapter, pucConfigBuf, u4ConfigBufLen);
#endif
	return WLAN_STATUS_SUCCESS;
}

#endif /* CFG_SUPPORT_CFG_FILE */

int32_t wlanHexToNum(int8_t c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

int32_t wlanHexToByte(int8_t *hex)
{
	int32_t a, b;

	a = wlanHexToNum(*hex++);
	if (a < 0)
		return -1;
	b = wlanHexToNum(*hex++);
	if (b < 0)
		return -1;
	return (a << 4) | b;
}

int32_t wlanHwAddrToBin(int8_t *txt, uint8_t *addr)
{
	int32_t i;
	int8_t *pos = txt;

	for (i = 0; i < 6; i++) {
		int32_t a, b;

		while (*pos == ':' || *pos == '.' || *pos == '-')
			pos++;

		a = wlanHexToNum(*pos++);
		if (a < 0)
			return -1;
		b = wlanHexToNum(*pos++);
		if (b < 0)
			return -1;
		*addr++ = (a << 4) | b;
	}

	return pos - txt;
}

u_int8_t wlanIsChipNoAck(IN struct ADAPTER *prAdapter)
{
	u_int8_t fgIsNoAck;

	fgIsNoAck = prAdapter->fgIsChipNoAck
#if CFG_CHIP_RESET_SUPPORT
		    || kalIsResetting()
#endif
		    || fgIsBusAccessFailed;

	return fgIsNoAck;
}

u_int8_t wlanIsChipRstRecEnabled(IN struct ADAPTER
				 *prAdapter)
{
	return prAdapter->rWifiVar.fgChipResetRecover;
}

u_int8_t wlanIsChipAssert(IN struct ADAPTER *prAdapter)
{
	return prAdapter->rWifiVar.fgChipResetRecover
		&& prAdapter->fgIsChipAssert;
}

void wlanChipRstPreAct(IN struct ADAPTER *prAdapter)
{
	struct BSS_INFO *prBssInfo = (struct BSS_INFO *) NULL;
	int32_t i4BssIdx;
	uint32_t u4ClientCount = 0;
	struct STA_RECORD *prCurrStaRec = (struct STA_RECORD *)
					  NULL;
	struct STA_RECORD *prNextCurrStaRec = (struct STA_RECORD *)
					      NULL;
	struct LINK *prClientList;
	struct GLUE_INFO *prGlueInfo = prAdapter->prGlueInfo;

	KAL_ACQUIRE_MUTEX(prAdapter, MUTEX_CHIP_RST);
	if (prAdapter->fgIsChipAssert) {
		KAL_RELEASE_MUTEX(prAdapter, MUTEX_CHIP_RST);
		return;
	}
	prAdapter->fgIsChipAssert = TRUE;
	KAL_RELEASE_MUTEX(prAdapter, MUTEX_CHIP_RST);

	for (i4BssIdx = 0; i4BssIdx < prAdapter->ucHwBssIdNum;
	     i4BssIdx++) {
		prBssInfo = prAdapter->aprBssInfo[i4BssIdx];

		if (!prBssInfo->fgIsInUse)
			continue;

		if (prBssInfo->eNetworkType == NETWORK_TYPE_AIS) {

			if (prGlueInfo->eParamMediaStateIndicated ==
			    PARAM_MEDIA_STATE_CONNECTED)
				kalIndicateStatusAndComplete(prGlueInfo,
					WLAN_STATUS_MEDIA_DISCONNECT, NULL, 0);
		} else if (prBssInfo->eNetworkType == NETWORK_TYPE_P2P) {
			if (prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT) {
				u4ClientCount = bssGetClientCount(prAdapter,
								  prBssInfo);
				if (u4ClientCount == 0)
					continue;

				prClientList = &prBssInfo->rStaRecOfClientList;
				LINK_FOR_EACH_ENTRY_SAFE(prCurrStaRec,
				    prNextCurrStaRec, prClientList,
				    rLinkEntry, struct STA_RECORD) {
					kalP2PGOStationUpdate(
					    prAdapter->prGlueInfo,
					    (uint8_t) prBssInfo->u4PrivateData,
					    prCurrStaRec, FALSE);
					LINK_REMOVE_KNOWN_ENTRY(prClientList,
					    &prCurrStaRec->rLinkEntry);
				}
			} else if (prBssInfo->eCurrentOPMode ==
				   OP_MODE_INFRASTRUCTURE) {
				if (prBssInfo->prStaRecOfAP == NULL)
					continue;
#if CFG_WPS_DISCONNECT || (KERNEL_VERSION(4, 4, 0) <= CFG80211_VERSION_CODE)
				kalP2PGCIndicateConnectionStatus(prGlueInfo,
					(uint8_t) prBssInfo->u4PrivateData,
					NULL, NULL, 0, 0,
					WLAN_STATUS_MEDIA_DISCONNECT);
#else
				kalP2PGCIndicateConnectionStatus(prGlueInfo,
					(uint8_t) prBssInfo->u4PrivateData,
					NULL, NULL, 0, 0);
#endif
				prBssInfo->prStaRecOfAP = NULL;

			}
		}
	}
}

#if CFG_ENABLE_PER_STA_STATISTICS
void wlanTxLifetimeUpdateStaStats(IN struct ADAPTER
				  *prAdapter, IN struct MSDU_INFO *prMsduInfo)
{
	struct STA_RECORD *prStaRec;
	uint32_t u4DeltaTime;
	uint32_t u4DeltaHifTxTime;
	struct PKT_PROFILE *prPktProfile = &prMsduInfo->rPktProfile;
#if 0
	struct QUE_MGT *prQM = &prAdapter->rQM;
	uint32_t u4PktPrintPeriod = 0;
#endif

	prStaRec = cnmGetStaRecByIndex(prAdapter,
				       prMsduInfo->ucStaRecIndex);

	if (prStaRec) {
		u4DeltaTime = (uint32_t) (prPktProfile->rHifTxDoneTimestamp -
				prPktProfile->rHardXmitArrivalTimestamp);
		u4DeltaHifTxTime = (uint32_t) (
				prPktProfile->rHifTxDoneTimestamp -
				prPktProfile->rDequeueTimestamp);

		/* Update StaRec statistics */
		prStaRec->u4TotalTxPktsNumber++;
		prStaRec->u4TotalTxPktsTime += u4DeltaTime;
		prStaRec->u4TotalTxPktsHifTxTime += u4DeltaHifTxTime;

		if (u4DeltaTime > prStaRec->u4MaxTxPktsTime)
			prStaRec->u4MaxTxPktsTime = u4DeltaTime;

		if (u4DeltaHifTxTime > prStaRec->u4MaxTxPktsHifTime)
			prStaRec->u4MaxTxPktsHifTime = u4DeltaHifTxTime;

		if (u4DeltaTime >= NIC_TX_TIME_THRESHOLD)
			prStaRec->u4ThresholdCounter++;

#if 0
		if (u4PktPrintPeriod &&
		    (prStaRec->u4TotalTxPktsNumber >= u4PktPrintPeriod)) {
			DBGLOG(TX, INFO, "[%u]N[%u]A[%u]M[%u]T[%u]E[%4u]\n",
			       prStaRec->ucIndex,
			       prStaRec->u4TotalTxPktsNumber,
			       prStaRec->u4TotalTxPktsTime,
			       prStaRec->u4MaxTxPktsTime,
			       prStaRec->u4ThresholdCounter,
			       prQM->au4QmTcResourceEmptyCounter[
				prStaRec->ucBssIndex][TC2_INDEX]);

			prStaRec->u4TotalTxPktsNumber = 0;
			prStaRec->u4TotalTxPktsTime = 0;
			prStaRec->u4MaxTxPktsTime = 0;
			prStaRec->u4ThresholdCounter = 0;
			prQM->au4QmTcResourceEmptyCounter[
				prStaRec->ucBssIndex][TC2_INDEX] = 0;
		}
#endif
	}
}
#endif

u_int8_t wlanTxLifetimeIsProfilingEnabled(
	IN struct ADAPTER *prAdapter)
{
	u_int8_t fgEnabled = TRUE;
#if CFG_SUPPORT_WFD
	struct WFD_CFG_SETTINGS *prWfdCfgSettings =
		(struct WFD_CFG_SETTINGS *) NULL;

	prWfdCfgSettings =
		&prAdapter->rWifiVar.rWfdConfigureSettings;

	if (prWfdCfgSettings->ucWfdEnable > 0)
		fgEnabled = TRUE;
#endif

	return fgEnabled;
}

u_int8_t wlanTxLifetimeIsTargetMsdu(IN struct ADAPTER
				    *prAdapter, IN struct MSDU_INFO *prMsduInfo)
{
	u_int8_t fgResult = TRUE;

#if 0
	switch (prMsduInfo->ucTID) {
	/* BK */
	case 1:
	case 2:

	/* BE */
	case 0:
	case 3:
		fgResult = FALSE;
		break;
	/* VI */
	case 4:
	case 5:

	/* VO */
	case 6:
	case 7:
		fgResult = TRUE;
		break;
	default:
		break;
	}
#endif
	return fgResult;
}

void wlanTxLifetimeTagPacket(IN struct ADAPTER *prAdapter,
			     IN struct MSDU_INFO *prMsduInfo,
			     IN enum ENUM_TX_PROFILING_TAG eTag)
{
	struct PKT_PROFILE *prPktProfile = &prMsduInfo->rPktProfile;

	if (!wlanTxLifetimeIsProfilingEnabled(prAdapter))
		return;

	switch (eTag) {
	case TX_PROF_TAG_OS_TO_DRV:
		/* arrival time is tagged in wlanProcessTxFrame */
		break;

	case TX_PROF_TAG_DRV_ENQUE:
		/* Reset packet profile */
		prPktProfile->fgIsValid = FALSE;
		if (wlanTxLifetimeIsTargetMsdu(prAdapter, prMsduInfo)) {
			/* Enable packet lifetime profiling */
			prPktProfile->fgIsValid = TRUE;

			/* Packet arrival time at kernel Hard Xmit */
			prPktProfile->rHardXmitArrivalTimestamp =
				GLUE_GET_PKT_ARRIVAL_TIME(prMsduInfo->prPacket);

			/* Packet enqueue time */
			prPktProfile->rEnqueueTimestamp = (OS_SYSTIME)
							  kalGetTimeTick();
		}
		break;

	case TX_PROF_TAG_DRV_DEQUE:
		if (prPktProfile->fgIsValid)
			prPktProfile->rDequeueTimestamp = (OS_SYSTIME)
							  kalGetTimeTick();
		break;

	case TX_PROF_TAG_DRV_TX_DONE:
		if (prPktProfile->fgIsValid) {
			prPktProfile->rHifTxDoneTimestamp = (OS_SYSTIME)
							    kalGetTimeTick();
#if CFG_ENABLE_PER_STA_STATISTICS
			wlanTxLifetimeUpdateStaStats(prAdapter, prMsduInfo);
#endif
		}
		break;

	case TX_PROF_TAG_MAC_TX_DONE:
		break;

	default:
		break;
	}
}

void wlanTxProfilingTagPacket(IN struct ADAPTER *prAdapter,
			      IN void *prPacket,
			      IN enum ENUM_TX_PROFILING_TAG eTag)
{
#if CFG_MET_PACKET_TRACE_SUPPORT
	kalMetTagPacket(prAdapter->prGlueInfo, prPacket, eTag);
#endif
}

void wlanTxProfilingTagMsdu(IN struct ADAPTER *prAdapter,
			    IN struct MSDU_INFO *prMsduInfo,
			    IN enum ENUM_TX_PROFILING_TAG eTag)
{
	wlanTxLifetimeTagPacket(prAdapter, prMsduInfo, eTag);

	wlanTxProfilingTagPacket(prAdapter, prMsduInfo->prPacket,
				 eTag);
}

void wlanUpdateTxStatistics(IN struct ADAPTER *prAdapter,
			    IN struct MSDU_INFO *prMsduInfo,
			    IN u_int8_t fgTxDrop)
{
	struct STA_RECORD *prStaRec;
	struct BSS_INFO *prBssInfo;
	enum ENUM_WMM_ACI eAci = WMM_AC_BE_INDEX;
	struct QUE_MGT *prQM = &prAdapter->rQM;
	OS_SYSTIME rCurTime;

	eAci = aucTid2ACI[prMsduInfo->ucUserPriority];

	prStaRec = cnmGetStaRecByIndex(prAdapter,
				       prMsduInfo->ucStaRecIndex);

	if (prStaRec) {
		if (fgTxDrop)
			prStaRec->arLinkStatistics[eAci].u4TxDropMsdu++;
		else
			prStaRec->arLinkStatistics[eAci].u4TxMsdu++;
	} else {
		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
						  prMsduInfo->ucBssIndex);

		if (fgTxDrop)
			prBssInfo->arLinkStatistics[eAci].u4TxDropMsdu++;
		else
			prBssInfo->arLinkStatistics[eAci].u4TxMsdu++;
	}

	/* Trigger FW stats log every 20s */
	rCurTime = (OS_SYSTIME) kalGetTimeTick();

	DBGLOG(INIT, LOUD, "CUR[%u] LAST[%u] TO[%u]\n", rCurTime,
	       prQM->rLastTxPktDumpTime,
	       CHECK_FOR_TIMEOUT(rCurTime, prQM->rLastTxPktDumpTime,
				 MSEC_TO_SYSTIME(
				 prAdapter->rWifiVar.u4StatsLogTimeout)));

	if (CHECK_FOR_TIMEOUT(rCurTime, prQM->rLastTxPktDumpTime,
			      MSEC_TO_SYSTIME(
			      prAdapter->rWifiVar.u4StatsLogTimeout))) {

		wlanTriggerStatsLog(prAdapter,
				    prAdapter->rWifiVar.u4StatsLogDuration);
		wlanDumpAllBssStatistics(prAdapter);

		prQM->rLastTxPktDumpTime = rCurTime;
	}
}

void wlanUpdateRxStatistics(IN struct ADAPTER *prAdapter,
			    IN struct SW_RFB *prSwRfb)
{
	struct STA_RECORD *prStaRec;
	enum ENUM_WMM_ACI eAci = WMM_AC_BE_INDEX;

	eAci = aucTid2ACI[prSwRfb->ucTid];

	prStaRec = cnmGetStaRecByIndex(prAdapter,
				       prSwRfb->ucStaRecIdx);
	if (prStaRec)
		prStaRec->arLinkStatistics[eAci].u4RxMsdu++;
}

uint32_t wlanTriggerStatsLog(IN struct ADAPTER *prAdapter,
			     IN uint32_t u4DurationInMs)
{
	return WLAN_STATUS_SUCCESS;
}

uint32_t
wlanPktTxDone(IN struct ADAPTER *prAdapter,
	      IN struct MSDU_INFO *prMsduInfo,
	      IN enum ENUM_TX_RESULT_CODE rTxDoneStatus)
{
	OS_SYSTIME rCurrent = kalGetTimeTick();
	struct PKT_PROFILE *prPktProfile = &prMsduInfo->rPktProfile;

	uint8_t *apucPktType[ENUM_PKT_FLAG_NUM] = {
		(uint8_t *) DISP_STRING("INVALID"),
		(uint8_t *) DISP_STRING("802_3"),
		(uint8_t *) DISP_STRING("1X"),
		(uint8_t *) DISP_STRING("PROTECTED_1X"),
		(uint8_t *) DISP_STRING("NON_PROTECTED_1X"),
		(uint8_t *) DISP_STRING("VLAN_EXIST"),
		(uint8_t *) DISP_STRING("DHCP"),
		(uint8_t *) DISP_STRING("ARP"),
		(uint8_t *) DISP_STRING("ICMP"),
		(uint8_t *) DISP_STRING("TDLS"),
		(uint8_t *) DISP_STRING("DNS")
	};
	if (prMsduInfo->ucPktType >= ENUM_PKT_FLAG_NUM)
		prMsduInfo->ucPktType = 0;

	if (prPktProfile->fgIsValid &&
		((prMsduInfo->ucPktType == ENUM_PKT_ARP) ||
		(prMsduInfo->ucPktType == ENUM_PKT_DHCP))) {
		if (rCurrent - prPktProfile->rHardXmitArrivalTimestamp > 2000) {
			DBGLOG(TX, INFO,
				"valid %d; ArriveDrv %u, Enq %u, Deq %u, LeaveDrv %u, TxDone %u\n",
				prPktProfile->fgIsValid,
				prPktProfile->rHardXmitArrivalTimestamp,
				prPktProfile->rEnqueueTimestamp,
				prPktProfile->rDequeueTimestamp,
				prPktProfile->rHifTxDoneTimestamp, rCurrent);

			if (prMsduInfo->ucPktType == ENUM_PKT_ARP)
				prAdapter->prGlueInfo->fgTxDoneDelayIsARP =
									TRUE;
			prAdapter->prGlueInfo->u4ArriveDrvTick =
				prPktProfile->rHardXmitArrivalTimestamp;
			prAdapter->prGlueInfo->u4EnQueTick =
				prPktProfile->rEnqueueTimestamp;
			prAdapter->prGlueInfo->u4DeQueTick =
				prPktProfile->rDequeueTimestamp;
			prAdapter->prGlueInfo->u4LeaveDrvTick =
				prPktProfile->rHifTxDoneTimestamp;
			prAdapter->prGlueInfo->u4CurrTick = rCurrent;
			prAdapter->prGlueInfo->u8CurrTime = sched_clock();
		}
	}

	DBGLOG_LIMITED(TX, INFO,
		"TX DONE, Type[%s] Tag[0x%08x] WIDX:PID[%u:%u] Status[%u], SeqNo: %d\n",
		apucPktType[prMsduInfo->ucPktType], prMsduInfo->u4TxDoneTag,
		prMsduInfo->ucWlanIndex, prMsduInfo->ucPID, rTxDoneStatus,
		prMsduInfo->ucTxSeqNum);

	if (prMsduInfo->ucPktType == ENUM_PKT_1X) {
		p2pRoleFsmNotifyEapolTxStatus(prAdapter,
				prMsduInfo->ucBssIndex,
				prMsduInfo->eEapolKeyType,
				rTxDoneStatus);
		secHandleEapolTxStatus(prAdapter, prMsduInfo,
				rTxDoneStatus);
	}

	return WLAN_STATUS_SUCCESS;
}

#if CFG_ASSERT_DUMP
void wlanCorDumpTimerInit(IN struct ADAPTER *prAdapter,
				u_int8_t fgIsResetN9)
{
	if (fgIsResetN9) {
		cnmTimerInitTimer(prAdapter,
				  &prAdapter->rN9CorDumpTimer,
				  (PFN_MGMT_TIMEOUT_FUNC) wlanN9CorDumpTimeOut,
				  (unsigned long) NULL);

	} else {
		cnmTimerInitTimer(prAdapter,
				  &prAdapter->rCr4CorDumpTimer,
				  (PFN_MGMT_TIMEOUT_FUNC) wlanCr4CorDumpTimeOut,
				  (unsigned long) NULL);
	}
}

void wlanCorDumpTimerReset(IN struct ADAPTER *prAdapter,
			   u_int8_t fgIsResetN9)
{
	if (prAdapter->fgN9AssertDumpOngoing
	    || prAdapter->fgCr4AssertDumpOngoing) {

		if (fgIsResetN9) {
			cnmTimerStopTimer(prAdapter,
					  &prAdapter->rN9CorDumpTimer);
			cnmTimerStartTimer(prAdapter,
					   &prAdapter->rN9CorDumpTimer, 5000);
		} else {
			cnmTimerStopTimer(prAdapter,
					  &prAdapter->rCr4CorDumpTimer);
			cnmTimerStartTimer(prAdapter,
					   &prAdapter->rCr4CorDumpTimer, 5000);
		}
	} else {
		DBGLOG(INIT, INFO,
		       "Cr4, N9 CorDump Is not ongoing, ignore timer reset\n");
	}
}

void wlanN9CorDumpTimeOut(IN struct ADAPTER *prAdapter,
			  IN unsigned long ulParamPtr)
{
	if (prAdapter->fgN9CorDumpFileOpend) {
		DBGLOG(INIT, STATE, "\n[DUMP_N9]====N9 ASSERT_END====\n");
		prAdapter->fgN9AssertDumpOngoing = FALSE;
		kalCloseCorDumpFile(TRUE);
		prAdapter->fgN9CorDumpFileOpend = FALSE;

#if CFG_CHIP_RESET_SUPPORT
#ifdef CFG_SUPPORT_CONNAC2X
#else
		/* Trigger RESET */
		glGetRstReason(RST_FW_ASSERT);
		DBGLOG(INIT, STATE, "eResetReason = %d\n", eResetReason);
		GL_RESET_TRIGGER(prAdapter, RST_FLAG_CHIP_RESET);
#endif
#endif

	}
}

void wlanCr4CorDumpTimeOut(IN struct ADAPTER *prAdapter,
			   IN unsigned long ulParamPtr)
{
	if (prAdapter->fgCr4CorDumpFileOpend) {
		DBGLOG(INIT, STATE, "\n[DUMP_Cr4]====Cr4 ASSERT_END====\n");
		prAdapter->fgCr4AssertDumpOngoing = FALSE;
		kalCloseCorDumpFile(FALSE);
		prAdapter->fgCr4CorDumpFileOpend = FALSE;
#if CFG_CHIP_RESET_SUPPORT
#ifdef CFG_SUPPORT_CONNAC2X
#else
		/* Trigger RESET */
		glGetRstReason(RST_FW_ASSERT);
		GL_RESET_TRIGGER(prAdapter, RST_FLAG_CHIP_RESET);
#endif
#endif
	}
}
#endif

u_int8_t
wlanGetWlanIdxByAddress(IN struct ADAPTER *prAdapter,
			IN uint8_t *pucAddr, OUT uint8_t *pucIndex)
{
	uint8_t ucStaRecIdx;
	struct STA_RECORD *prTempStaRec;

	for (ucStaRecIdx = 0; ucStaRecIdx < CFG_STA_REC_NUM;
	     ucStaRecIdx++) {
		prTempStaRec = &(prAdapter->arStaRec[ucStaRecIdx]);
		if (pucAddr) {
			if (prTempStaRec->fgIsInUse &&
			    EQUAL_MAC_ADDR(prTempStaRec->aucMacAddr,
			    pucAddr)) {
				*pucIndex = prTempStaRec->ucWlanIndex;
				return TRUE;
			}
		} else {
			if (prTempStaRec->fgIsInUse
			    && prTempStaRec->ucStaState == STA_STATE_3) {
				*pucIndex = prTempStaRec->ucWlanIndex;
				return TRUE;
			}
		}
	}
	return FALSE;
}


uint8_t *
wlanGetStaAddrByWlanIdx(IN struct ADAPTER *prAdapter,
			IN uint8_t ucIndex)
{
	struct WLAN_TABLE *prWtbl;

	if (!prAdapter || ucIndex >= WTBL_SIZE)
		return NULL;

	prWtbl = prAdapter->rWifiVar.arWtbl;
	if (prWtbl[ucIndex].ucUsed && prWtbl[ucIndex].ucPairwise)
		return &prWtbl[ucIndex].aucMacAddr[0];

	return NULL;
}

void
wlanNotifyFwSuspend(struct GLUE_INFO *prGlueInfo,
		    struct net_device *prDev, u_int8_t fgSuspend)
{
	uint32_t rStatus;
	uint32_t u4SetInfoLen;
	struct NETDEV_PRIVATE_GLUE_INFO *prNetDevPrivate =
		(struct NETDEV_PRIVATE_GLUE_INFO *) NULL;
	struct CMD_SUSPEND_MODE_SETTING rSuspendCmd;

	prNetDevPrivate = (struct NETDEV_PRIVATE_GLUE_INFO *)
			  netdev_priv(prDev);

	if (prNetDevPrivate->prGlueInfo != prGlueInfo)
		DBGLOG(REQ, WARN, "%s: unexpected prGlueInfo(0x%p)!\n",
		       __func__, prNetDevPrivate->prGlueInfo);

	rSuspendCmd.ucBssIndex = prNetDevPrivate->ucBssIdx;
	rSuspendCmd.ucEnableSuspendMode = fgSuspend;

	if (prGlueInfo->prAdapter->rWifiVar.ucWow
	    && prGlueInfo->prAdapter->rWowCtrl.fgWowEnable) {
		/* cfg enable + wow enable => Wow On mdtim*/
		rSuspendCmd.ucMdtim =
			prGlueInfo->prAdapter->rWifiVar.ucWowOnMdtim;
		rSuspendCmd.ucWowSuspend = 1;
		DBGLOG(REQ, INFO, "mdtim [1]\n");
	} else if (prGlueInfo->prAdapter->rWifiVar.ucWow
		&& !prGlueInfo->prAdapter->rWowCtrl.fgWowEnable
		&& (prGlueInfo->prAdapter->rWifiVar.ucAdvPws)) {
		/* cfg enable + wow disable + adv pws enable
		 * => Wow Off mdtim
		 */
		rSuspendCmd.ucMdtim =
			prGlueInfo->prAdapter->rWifiVar.ucWowOffMdtim;
		rSuspendCmd.ucWowSuspend = 1;
		DBGLOG(REQ, INFO, "mdtim [2]\n");
	} else if (prGlueInfo->prAdapter->rWifiVar.ucWow
		&& !prGlueInfo->prAdapter->rWowCtrl.fgWowEnable
		&& (!prGlueInfo->prAdapter->rWifiVar.ucAdvPws)) {
		/* cfg enable + wow disable + adv pws disable
		 * => Wow Off mdtim
		 * => for android screen on/off case.
		 */
		rSuspendCmd.ucMdtim =
			prGlueInfo->prAdapter->rWifiVar.ucWowOffMdtim;
		rSuspendCmd.ucWowSuspend = 0;
		DBGLOG(REQ, INFO, "mdtim [2B]\n");
	} else if (!prGlueInfo->prAdapter->rWifiVar.ucWow) {
		/* cfg disable => MT6632 case
		 * => Wow Off mdtim
		 */
		rSuspendCmd.ucMdtim =
			prGlueInfo->prAdapter->rWifiVar.ucWowOffMdtim;
		rSuspendCmd.ucWowSuspend = 0;
		DBGLOG(REQ, INFO, "mdtim [3]\n");
	}

	/* When FW receive command, it check connection state to decide apply
	 * setting or not
	 */

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidNotifyFwSuspend,
			   (void *)&rSuspendCmd,
			   sizeof(rSuspendCmd),
			   FALSE,
			   FALSE,
			   TRUE,
			   &u4SetInfoLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(REQ, INFO, "wlanNotifyFwSuspend fail\n");
}

uint32_t
wlanGetStaIdxByWlanIdx(IN struct ADAPTER *prAdapter,
		       IN uint8_t ucIndex, OUT uint8_t *pucStaIdx)
{
	struct WLAN_TABLE *prWtbl;

	if (!prAdapter || ucIndex >= WTBL_SIZE)
		return WLAN_STATUS_FAILURE;

	prWtbl = prAdapter->rWifiVar.arWtbl;

	if (prWtbl[ucIndex].ucUsed && prWtbl[ucIndex].ucPairwise) {
		*pucStaIdx = prWtbl[ucIndex].ucStaIndex;
		return WLAN_STATUS_SUCCESS;
	}

	return WLAN_STATUS_FAILURE;
}

#if CFG_AUTO_CHANNEL_SEL_SUPPORT
/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to query LTE safe channels.
 *
 * \param[in]  pvAdapter        Pointer to the Adapter structure.
 * \param[out] pvQueryBuffer    A pointer to the buffer that holds the result of
 *                              the query.
 * \param[in]  u4QueryBufferLen The length of the query buffer.
 * \param[out] pu4QueryInfoLen  If the call is successful, returns the number of
 *                              bytes written into the query buffer. If the call
 *                              failed due to invalid length of the query
 *                              buffer, returns the amount of storage needed.
 *
 * \retval WLAN_STATUS_PENDING
 * \retval WLAN_STATUS_FAILURE
 */
/*----------------------------------------------------------------------------*/
uint32_t
wlanoidQueryLteSafeChannel(IN struct ADAPTER *prAdapter,
			   IN void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
			   OUT uint32_t *pu4QueryInfoLen)
{
	uint32_t rResult = WLAN_STATUS_FAILURE;
	struct CMD_GET_LTE_SAFE_CHN rQuery_LTE_SAFE_CHN;

	do {
		/* Sanity test */
		if ((prAdapter == NULL) || (pu4QueryInfoLen == NULL))
			break;
		if ((pvQueryBuffer == NULL) || (u4QueryBufferLen == 0))
			break;

		/* Get LTE safe channel list */
		rResult = wlanSendSetQueryCmd(prAdapter,
			CMD_ID_GET_LTE_CHN,
			FALSE,
			TRUE,
			g_fgIsOid, /* Query ID */
			nicCmdEventQueryLteSafeChn, /* The handler to receive
						     * firmware notification
						     */
			nicOidCmdTimeoutCommon,
			sizeof(struct CMD_GET_LTE_SAFE_CHN),
			(uint8_t *)&rQuery_LTE_SAFE_CHN,
			pvQueryBuffer,
			u4QueryBufferLen);
		DBGLOG(P2P, INFO, "[ACS] Get safe LTE Channels\n");
	} while (FALSE);

	return rResult;
}				/* wlanoidQueryLteSafeChannel */

/*----------------------------------------------------------------------------*/
/*!
 * \brief Add dirtiness to neighbor channels of a BSS to estimate channel
 *        quality.
 *
 * \param[in]  prAdapter        Pointer to the Adapter structure.
 * \param[in]  prBssDesc        Pointer to the BSS description.
 * \param[in]  u4Dirtiness      Expected dirtiness value.
 * \param[in]  ucCentralChannel Central channel of the given BSS.
 * \param[in]  ucCoveredRange   With ucCoveredRange and ucCentralChannel,
 *                              all the affected channels can be enumerated.
 */
/*----------------------------------------------------------------------------*/
static void
wlanAddDirtinessToAffectedChannels(struct ADAPTER *prAdapter,
				   struct BSS_DESC *prBssDesc,
				   uint32_t u4Dirtiness,
				   uint8_t ucCentralChannel,
				   uint8_t ucCoveredRange)
{
	uint8_t ucIdx, ucStart, ucEnd;
	u_int8_t bIs5GChl = ucCentralChannel > 14;
	uint8_t ucLeftNeighborChannel, ucRightNeighborChannel,
		ucLeftNeighborChannel2 = 0, ucRightNeighborChannel2 = 0,
		ucLeftestCoveredChannel, ucRightestCoveredChannel;
	struct PARAM_GET_CHN_INFO *prGetChnLoad = &
			(prAdapter->rWifiVar.rChnLoadInfo);

	ucLeftestCoveredChannel = ucCentralChannel > ucCoveredRange
				  ?
				  ucCentralChannel - ucCoveredRange : 1;

	ucLeftNeighborChannel = ucLeftestCoveredChannel ?
				ucLeftestCoveredChannel - 1 : 0;

	/* align leftest covered ch and left neighbor ch to valid 5g ch */
	if (bIs5GChl) {
		ucLeftestCoveredChannel += 2;
		ucLeftNeighborChannel -= 1;
	} else {
		/* we select the nearest 2 ch to the leftest covered ch as left
		 * neighbor chs
		 */
		ucLeftNeighborChannel2 = ucLeftNeighborChannel > 1 ?
						ucLeftNeighborChannel - 1 : 0;
	}

	/* handle corner cases of 5g ch*/
	if (ucLeftestCoveredChannel > 14
	    && ucLeftestCoveredChannel <= 36) {
		ucLeftestCoveredChannel = 36;
		ucLeftNeighborChannel = 0;
	} else if (ucLeftestCoveredChannel > 64
		   && ucLeftestCoveredChannel <= 100) {
		ucLeftestCoveredChannel = 100;
		ucLeftNeighborChannel = 0;
	} else if (ucLeftestCoveredChannel > 144 &&
		ucLeftestCoveredChannel <= 149) {
		ucLeftestCoveredChannel = 149;
		ucLeftNeighborChannel = 0;
	}

	/*
	 * because ch 14 is 12MHz away to ch13, we must shift the leftest
	 * covered ch and left neighbor ch when central ch is ch 14
	 */
	if (ucCentralChannel == 14) {
		ucLeftestCoveredChannel = 13;
		ucLeftNeighborChannel = 12;
		ucLeftNeighborChannel2 = 11;
	}

	ucRightestCoveredChannel = ucCentralChannel +
				   ucCoveredRange;
	ucRightNeighborChannel = ucRightestCoveredChannel + 1;

	/* align rightest covered ch and right neighbor ch to valid 5g ch */
	if (bIs5GChl) {
		ucRightestCoveredChannel -= 2;
		ucRightNeighborChannel += 1;
	} else {
		/* we select the nearest 2 ch to the rightest covered ch as
		 * right neighbor ch
		 */
		ucRightNeighborChannel2 = ucRightNeighborChannel < 13 ?
						ucRightNeighborChannel + 1 : 0;
	}

	/* handle corner cases */
	if (ucRightestCoveredChannel >= 14
	    && ucRightestCoveredChannel < 36) {
		if (ucRightestCoveredChannel == 14) {
			ucRightestCoveredChannel = 13;
			ucRightNeighborChannel = 14;
		} else {
			ucRightestCoveredChannel = 14;
			ucRightNeighborChannel = 0;
		}

		ucRightNeighborChannel2 = 0;
	} else if (ucRightestCoveredChannel >= 64
		   && ucRightestCoveredChannel < 100) {
		ucRightestCoveredChannel = 64;
		ucRightNeighborChannel = 0;
	} else if (ucRightestCoveredChannel >= 144 &&
		ucRightestCoveredChannel < 149) {
		ucRightestCoveredChannel = 144;
		ucRightNeighborChannel = 0;
	} else if (ucRightestCoveredChannel >= 165) {
		ucRightestCoveredChannel = 165;
		ucRightNeighborChannel = 0;
	}

	DBGLOG(SCN, TEMP, "central ch %u\n", ucCentralChannel);

	ucStart = wlanGetChannelIndex(ucLeftestCoveredChannel);
	ucEnd = wlanGetChannelIndex(ucRightestCoveredChannel);
	if (ucStart >= MAX_CHN_NUM || ucEnd >= MAX_CHN_NUM) {
		DBGLOG(SCN, ERROR, "Invalid ch idx of start %u, or end %u\n",
			ucStart, ucEnd);
		return;
	}

	for (ucIdx = ucStart; ucIdx <= ucEnd; ucIdx++) {
		prGetChnLoad->rEachChnLoad[ucIdx].u4Dirtiness +=
			u4Dirtiness;
		DBGLOG(SCN, TEMP, "Add dirtiness %d, to covered ch %d\n",
		       u4Dirtiness,
		       prGetChnLoad->rEachChnLoad[ucIdx].ucChannel);
	}

	if (ucLeftNeighborChannel != 0) {
		ucIdx = wlanGetChannelIndex(ucLeftNeighborChannel);
		if (ucIdx < MAX_CHN_NUM) {
			prGetChnLoad->rEachChnLoad[ucIdx].u4Dirtiness +=
				(u4Dirtiness >> 1);
			DBGLOG(SCN, TEMP,
				"Add dirtiness %d, to neighbor ch %d\n",
				u4Dirtiness >> 1,
				prGetChnLoad->rEachChnLoad[ucIdx].ucChannel);
		}
	}

	if (ucRightNeighborChannel != 0) {
		ucIdx = wlanGetChannelIndex(ucRightNeighborChannel);
		if (ucIdx < MAX_CHN_NUM) {
			prGetChnLoad->rEachChnLoad[ucIdx].u4Dirtiness +=
				(u4Dirtiness >> 1);
			DBGLOG(SCN, TEMP,
				"Add dirtiness %d, to neighbor ch %d\n",
				u4Dirtiness >> 1,
				prGetChnLoad->rEachChnLoad[ucIdx].ucChannel);
		}
	}

	if (bIs5GChl)
		return;

	/* Only necesaary for 2.5G */
	if (ucLeftNeighborChannel2 != 0) {
		ucIdx = wlanGetChannelIndex(ucLeftNeighborChannel2);
		if (ucIdx < MAX_CHN_NUM) {
			prGetChnLoad->rEachChnLoad[ucIdx].u4Dirtiness +=
				(u4Dirtiness >> 1);
			DBGLOG(SCN, TEMP,
				"Add dirtiness %d, to neighbor ch %d\n",
				u4Dirtiness >> 1,
				prGetChnLoad->rEachChnLoad[ucIdx].ucChannel);
		}
	}

	if (ucRightNeighborChannel2 != 0) {
		ucIdx = wlanGetChannelIndex(ucRightNeighborChannel2);
		if (ucIdx < MAX_CHN_NUM) {
			prGetChnLoad->rEachChnLoad[ucIdx].u4Dirtiness +=
				(u4Dirtiness >> 1);
			DBGLOG(SCN, TEMP,
				"Add dirtiness %d, to neighbor ch %d\n",
				u4Dirtiness >> 1,
				prGetChnLoad->rEachChnLoad[ucIdx].ucChannel);
		}
	}

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief For a scanned BSS, add dirtiness to the channels 1)around its primary
 *        channels and 2) in its working BW to represent the quality degrade.
 *
 * \param[in]  prAdapter        Pointer to the Adapter structure.
 * \param[in]  prBssDesc        Pointer to the BSS description.
 * \param[in]  u4Dirtiness      Expected dirtiness value.
 * \param[in]  bIsIndexOne      True means index 1, False means index 2.
 */
/*----------------------------------------------------------------------------*/
static void
wlanCalculateChannelDirtiness(IN struct ADAPTER *prAdapter,
			      struct BSS_DESC *prBssDesc, uint32_t u4Dirtiness,
			      u_int8_t bIsIndexOne)
{
	uint8_t ucCoveredRange = 0, ucCentralChannel = 0,
		ucCentralChannel2 = 0;

	if (bIsIndexOne) {
		DBGLOG(SCN, TEMP, "Process dirtiness index 1\n");
		ucCentralChannel = prBssDesc->ucChannelNum;
		ucCoveredRange = 2;
	} else {
		DBGLOG(SCN, TEMP, "Process dirtiness index 2, ");
		switch (prBssDesc->eChannelWidth) {
		case CW_20_40MHZ:
			if (prBssDesc->eSco == CHNL_EXT_SCA) {
				DBGLOG(SCN, TEMP, "BW40\n");
				ucCentralChannel = prBssDesc->ucChannelNum + 2;
				ucCoveredRange = 4;
			} else if (prBssDesc->eSco == CHNL_EXT_SCB) {
				DBGLOG(SCN, TEMP, "BW40\n");
				ucCentralChannel = prBssDesc->ucChannelNum - 2;
				ucCoveredRange = 4;
			} else {
				DBGLOG(SCN, TEMP, "BW20\n");
				ucCentralChannel = prBssDesc->ucChannelNum;
				ucCoveredRange = 2;
			}
			break;
		case CW_80MHZ:
			DBGLOG(SCN, TEMP, "BW80\n");
			ucCentralChannel = prBssDesc->ucCenterFreqS1;
			ucCoveredRange = 8;
			break;
		case CW_160MHZ:
			DBGLOG(SCN, TEMP, "BW160\n");
			ucCentralChannel = prBssDesc->ucCenterFreqS1;
			ucCoveredRange = 16;
			break;
		case CW_80P80MHZ:
			DBGLOG(SCN, TEMP, "BW8080\n");
			ucCentralChannel = prBssDesc->ucCenterFreqS1;
			ucCentralChannel2 = prBssDesc->ucCenterFreqS2;
			ucCoveredRange = 8;
			break;
		default:
			ucCentralChannel = prBssDesc->ucChannelNum;
			ucCoveredRange = 2;
			break;
		};
	}

	wlanAddDirtinessToAffectedChannels(prAdapter, prBssDesc,
					   u4Dirtiness,
					   ucCentralChannel, ucCoveredRange);

	/* 80 + 80 secondary 80 case */
	if (bIsIndexOne || ucCentralChannel2 == 0)
		return;

	wlanAddDirtinessToAffectedChannels(prAdapter, prBssDesc,
					   u4Dirtiness,
					   ucCentralChannel2, ucCoveredRange);
}

void
wlanInitChnLoadInfoChannelList(IN struct ADAPTER *prAdapter)
{
	uint8_t ucIdx = 0;
	struct PARAM_GET_CHN_INFO *prGetChnLoad = &
			(prAdapter->rWifiVar.rChnLoadInfo);

	for (ucIdx = 0; ucIdx < MAX_CHN_NUM; ucIdx++)
		prGetChnLoad->rEachChnLoad[ucIdx].ucChannel =
			wlanGetChannelNumFromIndex(ucIdx);
}

uint32_t
wlanCalculateAllChannelDirtiness(IN struct ADAPTER
				 *prAdapter)
{
	uint32_t rResult = WLAN_STATUS_SUCCESS;
	int32_t i4Rssi = 0;
	struct BSS_DESC *prBssDesc = NULL;
	uint32_t u4Dirtiness = 0;
	struct LINK *prBSSDescList =
				&(prAdapter->rWifiVar.rScanInfo.rBSSDescList);

	LINK_FOR_EACH_ENTRY(prBssDesc, prBSSDescList, rLinkEntry,
			    struct BSS_DESC) {
		i4Rssi = RCPI_TO_dBm(prBssDesc->ucRCPI);

		if (i4Rssi >= ACS_AP_RSSI_LEVEL_HIGH)
			u4Dirtiness = ACS_DIRTINESS_LEVEL_HIGH;
		else if (i4Rssi >= ACS_AP_RSSI_LEVEL_LOW)
			u4Dirtiness = ACS_DIRTINESS_LEVEL_MID;
		else
			u4Dirtiness = ACS_DIRTINESS_LEVEL_LOW;

		DBGLOG(SCN, TEMP, "Found an AP(%s), primary ch %d\n",
		       prBssDesc->aucSSID, prBssDesc->ucChannelNum);

		/* dirtiness index1 */
		wlanCalculateChannelDirtiness(prAdapter, prBssDesc,
					      u4Dirtiness, TRUE);

		/* dirtiness index2 */
		wlanCalculateChannelDirtiness(prAdapter, prBssDesc,
					      u4Dirtiness >> 1, FALSE);
	}

	return rResult;
}

uint8_t
wlanGetChannelIndex(IN uint8_t channel)
{
	uint8_t ucIdx = MAX_CHN_NUM;

	if (channel <= 14)
		ucIdx = channel - 1;
	else if (channel >= 36 && channel <= 64)
		ucIdx = 14 + (channel - 36) / 4;
	else if (channel >= 100 && channel <= 144)
		ucIdx = 14 + 8 + (channel - 100) / 4;
	else if (channel >= 149 && channel <= 165)
		ucIdx = 14 + 8 + 12 + (channel - 149) / 4;

	return ucIdx;
}

/*---------------------------------------------------------------------*/
/*!
 * \brief Get ch index by the given ch num; the reverse function of
 *        wlanGetChannelIndex
 *
 * \param[in]    ucIdx                 Channel index
 * \param[out]   ucChannel             Channel number
 */
/*---------------------------------------------------------------------*/

uint8_t
wlanGetChannelNumFromIndex(IN uint8_t ucIdx)
{
	uint8_t ucChannel = 0;

	if (ucIdx >= 34)
		ucChannel = ((ucIdx - 34) << 2) + 149;
	else if (ucIdx >= 22)
		ucChannel = ((ucIdx - 22) << 2) + 100;
	else if (ucIdx >= 14)
		ucChannel = ((ucIdx - 14) << 2) + 36;
	else
		ucChannel = ucIdx + 1;

	return ucChannel;
}

void
wlanSortChannel(IN struct ADAPTER *prAdapter)
{
	struct PARAM_GET_CHN_INFO *prChnLoadInfo = &
			(prAdapter->rWifiVar.rChnLoadInfo);
	int8_t ucIdx = 0, ucRoot = 0, ucChild = 0;
	struct PARAM_CHN_RANK_INFO rChnRankInfo;

	/* prepare unsorted ch rank list */
	for (ucIdx = 0; ucIdx < MAX_CHN_NUM; ++ucIdx) {
		prChnLoadInfo->rChnRankList[ucIdx].ucChannel =
			prChnLoadInfo->rEachChnLoad[ucIdx].ucChannel;
		prChnLoadInfo->rChnRankList[ucIdx].u4Dirtiness =
			prChnLoadInfo->rEachChnLoad[ucIdx].u4Dirtiness;
	}

	/* heapify ch rank list */
	for (ucIdx = MAX_CHN_NUM / 2 - 1; ucIdx >= 0; --ucIdx) {
		for (ucRoot = ucIdx; ucRoot * 2 + 1 < MAX_CHN_NUM;
		     ucRoot = ucChild) {

			ucChild = ucRoot * 2 + 1;
			if (ucChild < MAX_CHN_NUM - 1 && prChnLoadInfo->
			    rChnRankList[ucChild + 1].u4Dirtiness >
			    prChnLoadInfo->rChnRankList[ucChild].u4Dirtiness)
				ucChild += 1;

			if (prChnLoadInfo->rChnRankList[ucChild].u4Dirtiness <=
			    prChnLoadInfo->rChnRankList[ucRoot].u4Dirtiness)
				break;

			rChnRankInfo = prChnLoadInfo->rChnRankList[ucChild];
			prChnLoadInfo->rChnRankList[ucChild] =
				prChnLoadInfo->rChnRankList[ucRoot];
			prChnLoadInfo->rChnRankList[ucRoot] = rChnRankInfo;
		}
	}

	/* sort ch rank list */
	for (ucIdx = MAX_CHN_NUM - 1; ucIdx > 0; ucIdx--) {
		rChnRankInfo = prChnLoadInfo->rChnRankList[0];
		prChnLoadInfo->rChnRankList[0] =
			prChnLoadInfo->rChnRankList[ucIdx];
		prChnLoadInfo->rChnRankList[ucIdx] = rChnRankInfo;

		for (ucRoot = 0; ucRoot * 2 + 1 < ucIdx; ucRoot = ucChild) {
			ucChild = ucRoot * 2 + 1;
			if (ucChild < ucIdx - 1 && prChnLoadInfo->
			    rChnRankList[ucChild + 1].u4Dirtiness >
			    prChnLoadInfo->rChnRankList[ucChild].u4Dirtiness)
				ucChild += 1;

			if (prChnLoadInfo->rChnRankList[ucChild].u4Dirtiness <=
			    prChnLoadInfo->rChnRankList[ucRoot].u4Dirtiness)
				break;

			rChnRankInfo = prChnLoadInfo->rChnRankList[ucChild];
			prChnLoadInfo->rChnRankList[ucChild] =
				prChnLoadInfo->rChnRankList[ucRoot];
			prChnLoadInfo->rChnRankList[ucRoot] = rChnRankInfo;
		}
	}

	for (ucIdx = 0; ucIdx < MAX_CHN_NUM; ++ucIdx)
		log_dbg(P2P, TEMP, "[ACS]channel=%d, dirtiness=%d\n",
		       prChnLoadInfo->rChnRankList[ucIdx].ucChannel,
		       prChnLoadInfo->rChnRankList[ucIdx].u4Dirtiness);

}
#endif

#if ((CFG_SISO_SW_DEVELOP == 1) || (CFG_SUPPORT_SPE_IDX_CONTROL == 1))
uint8_t
wlanGetAntPathType(IN struct ADAPTER *prAdapter,
		   IN enum ENUM_WF_PATH_FAVOR_T eWfPathFavor,
		   IN uint8_t ucBssIndex)
{
	uint8_t ucFianlWfPathType = eWfPathFavor;
#if (CFG_SUPPORT_SPE_IDX_CONTROL == 1)
	uint8_t ucNss = prAdapter->rWifiVar.ucNSS;
	uint8_t ucSpeIdxCtrl = GET_SPE_IDX_CTRL(prAdapter);
#if CFG_SUPPORT_COEX_NON_COTX
	enum ENUM_BAND eBand;
	struct BSS_INFO *prBssInfo;

	if (GET_COEX_NON_COTX(prAdapter) &&
		ucNss == 2) {
		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);
		if (prBssInfo->fgIsGranted)
			eBand = prBssInfo->eBandGranted;
		else
			eBand = prBssInfo->eBand;
		if (eBand == BAND_2G4 &&
			prAdapter->rWifiVar.ucSpeIdxCtrl2g != 2)
			ucSpeIdxCtrl = prAdapter->rWifiVar.ucSpeIdxCtrl2g;
	}
#endif
	if (ucNss <= 2) {
		if (ucSpeIdxCtrl == 0)
			ucFianlWfPathType = ENUM_WF_0_ONE_STREAM_PATH_FAVOR;
		else if (ucSpeIdxCtrl == 1)
			ucFianlWfPathType = ENUM_WF_1_ONE_STREAM_PATH_FAVOR;
		else if (ucSpeIdxCtrl == 2) {
			if (ucNss > 1)
				ucFianlWfPathType =
					ENUM_WF_0_1_DUP_STREAM_PATH_FAVOR;
			else
				ucFianlWfPathType = ENUM_WF_NON_FAVOR;
		} else
			ucFianlWfPathType = ENUM_WF_NON_FAVOR;
	}
	DBGLOG(TX, INFO, "WfPathType:%d, SpeIdxCtrl=%d\n",
	       ucFianlWfPathType, ucSpeIdxCtrl);
#endif
	return ucFianlWfPathType;
}

uint8_t
wlanAntPathFavorSelect(IN struct ADAPTER *prAdapter,
		       IN enum ENUM_WF_PATH_FAVOR_T eWfPathFavor)
{
	uint8_t ucRetValSpeIdx = 0x18;
#if (CFG_SUPPORT_SPE_IDX_CONTROL == 1)
	uint8_t ucNss = prAdapter->rWifiVar.ucNSS;

	if (ucNss <= 2) {
		if ((eWfPathFavor == ENUM_WF_NON_FAVOR) ||
			(eWfPathFavor == ENUM_WF_0_ONE_STREAM_PATH_FAVOR) ||
			(eWfPathFavor == ENUM_WF_0_1_TWO_STREAM_PATH_FAVOR))
			ucRetValSpeIdx = ANTENNA_WF0;
		else if (eWfPathFavor == ENUM_WF_0_1_DUP_STREAM_PATH_FAVOR)
			ucRetValSpeIdx = 0x18;
		else if (eWfPathFavor == ENUM_WF_1_ONE_STREAM_PATH_FAVOR)
			ucRetValSpeIdx = ANTENNA_WF1;
		else
			ucRetValSpeIdx = ANTENNA_WF0;
	}
#endif
	return ucRetValSpeIdx;
}
#endif

uint8_t
wlanGetSpeIdx(IN struct ADAPTER *prAdapter,
	      IN uint8_t ucBssIndex,
	      IN enum ENUM_WF_PATH_FAVOR_T eWfPathFavor)
{
	uint8_t ucRetValSpeIdx = 0;
#if ((CFG_SISO_SW_DEVELOP == 1) || (CFG_SUPPORT_SPE_IDX_CONTROL == 1))
	struct BSS_INFO *prBssInfo;
	enum ENUM_BAND eBand = BAND_NULL;

	if (ucBssIndex > prAdapter->ucHwBssIdNum) {
		DBGLOG(SW4, ERROR, "Invalid BssInfo index[%u], skip dump!\n",
		       ucBssIndex);
		return ucRetValSpeIdx;
	}
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);
	/*
	 * if DBDC enable return 0, else depend 2.4G/5G & support WF path
	 * retrun accurate value
	 */
	if (!prAdapter->rWifiVar.fgDbDcModeEn) {
		if (prBssInfo->fgIsGranted)
			eBand = prBssInfo->eBandGranted;
		else
			eBand = prBssInfo->eBand;

		if (eBand == BAND_2G4) {
			if (IS_WIFI_2G4_SISO(prAdapter)) {
				if (IS_WIFI_2G4_WF0_SUPPORT(prAdapter))
					ucRetValSpeIdx = ANTENNA_WF0;
				else
					ucRetValSpeIdx = ANTENNA_WF1;
			} else {
				if (IS_WIFI_SMART_GEAR_SUPPORT_WF0_SISO(
				    prAdapter))
					ucRetValSpeIdx = ANTENNA_WF0;
				else if (IS_WIFI_SMART_GEAR_SUPPORT_WF1_SISO(
				    prAdapter))
					ucRetValSpeIdx = ANTENNA_WF1;
				else
					ucRetValSpeIdx = wlanAntPathFavorSelect(
						prAdapter, eWfPathFavor);
			}
		} else if (eBand == BAND_5G) {
			if (IS_WIFI_5G_SISO(prAdapter)) {
				if (IS_WIFI_5G_WF0_SUPPORT(prAdapter))
					ucRetValSpeIdx = ANTENNA_WF0;
				else
					ucRetValSpeIdx = ANTENNA_WF1;
			} else {
				if (IS_WIFI_SMART_GEAR_SUPPORT_WF0_SISO(
				    prAdapter))
					ucRetValSpeIdx = ANTENNA_WF0;
				else if (IS_WIFI_SMART_GEAR_SUPPORT_WF1_SISO(
				    prAdapter))
					ucRetValSpeIdx = ANTENNA_WF1;
				else
					ucRetValSpeIdx = wlanAntPathFavorSelect(
						prAdapter, eWfPathFavor);
			}
		} else
			ucRetValSpeIdx = wlanAntPathFavorSelect(prAdapter,
				eWfPathFavor);
	}
	DBGLOG(INIT, INFO, "SpeIdx:%d,D:%d,G=%d,B=%d,Bss=%d\n",
	       ucRetValSpeIdx, prAdapter->rWifiVar.fgDbDcModeEn,
	       prBssInfo->fgIsGranted, eBand, ucBssIndex);
#endif
	return ucRetValSpeIdx;
}

uint8_t
wlanGetSupportNss(IN struct ADAPTER *prAdapter,
		  IN uint8_t ucBssIndex)
{
	uint8_t ucRetValNss = prAdapter->rWifiVar.ucNSS;
#if CFG_SISO_SW_DEVELOP
	struct BSS_INFO *prBssInfo;
	enum ENUM_BAND eBand = BAND_NULL;

	if (ucBssIndex > prAdapter->ucHwBssIdNum) {
		DBGLOG(SW4, ERROR, "Invalid BssInfo index[%u], skip dump!\n",
		       ucBssIndex);
		return ucRetValNss;
	}
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);
	/*
	 * depend 2.4G/5G support SISO/MIMO
	 * retrun accurate value
	 */
	if (prBssInfo->fgIsGranted)
		eBand = prBssInfo->eBandGranted;
	else
		eBand = prBssInfo->eBand;

	if ((eBand == BAND_2G4) && IS_WIFI_2G4_SISO(prAdapter))
		ucRetValNss = 1;
	else if ((eBand == BAND_5G) && IS_WIFI_5G_SISO(prAdapter))
		ucRetValNss = 1;
	DBGLOG(INIT, INFO, "Nss=%d,G=%d,B=%d,Bss=%d\n",
	       ucRetValNss, prBssInfo->fgIsGranted, eBand, ucBssIndex);
#endif
	return ucRetValNss;
}

#if CFG_SUPPORT_LOWLATENCY_MODE
/*----------------------------------------------------------------------------*/
/*!
 * \brief This is a private routine, which is used to initialize the variables
 *        for low latency mode.
 *
 * \param prAdapter      Pointer of Adapter Data Structure
 *
 * \retval WLAN_STATUS_SUCCESS: Success
 * \retval WLAN_STATUS_FAILURE: Failed
 */
/*----------------------------------------------------------------------------*/
uint32_t
wlanAdapterStartForLowLatency(IN struct ADAPTER *prAdapter)
{
	uint32_t u4Status = WLAN_STATUS_SUCCESS;

	/* Default disable low latency mode */
	prAdapter->fgEnLowLatencyMode = FALSE;

	/* Default enable scan */
	prAdapter->fgEnCfg80211Scan = TRUE;

	return u4Status;
}
#endif /* CFG_SUPPORT_LOWLATENCY_MODE */
int32_t wlanGetFileContent(struct ADAPTER *prAdapter,
	const uint8_t *pcFileName, uint8_t *pucBuf,
	uint32_t u4MaxFileLen, uint32_t *pu4ReadFileLen, u_int8_t bReqFw)
{
	if (bReqFw)
		return kalRequestFirmware(pcFileName, pucBuf,
				 u4MaxFileLen, pu4ReadFileLen,
				 prAdapter->prGlueInfo->prDev);

	return kalReadToFile(pcFileName, pucBuf,
				u4MaxFileLen, pu4ReadFileLen);
}

void wlanReleasePendingCmdById(struct ADAPTER *prAdapter, uint8_t ucCid)
{
	struct QUE *prCmdQue;
	struct QUE rTempCmdQue;
	struct QUE *prTempCmdQue = &rTempCmdQue;
	struct QUE_ENTRY *prQueueEntry = (struct QUE_ENTRY *) NULL;
	struct CMD_INFO *prCmdInfo = (struct CMD_INFO *) NULL;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);
	DBGLOG(OID, INFO, "Remove pending Cmd: CID %d\n", ucCid);

	/* 1: Clear Pending OID in prAdapter->rPendingCmdQueue */
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING);

	prCmdQue = &prAdapter->rPendingCmdQueue;
	QUEUE_MOVE_ALL(prTempCmdQue, prCmdQue);

	QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, struct QUE_ENTRY *);
	while (prQueueEntry) {
		prCmdInfo = (struct CMD_INFO *) prQueueEntry;
		if (prCmdInfo->ucCID != ucCid) {
			QUEUE_INSERT_TAIL(prCmdQue, prQueueEntry);
			continue;
		}

		if (prCmdInfo->pfCmdTimeoutHandler) {
			prCmdInfo->pfCmdTimeoutHandler(prAdapter, prCmdInfo);
		} else if (prCmdInfo->fgIsOid) {
			kalOidComplete(prAdapter->prGlueInfo,
				       prCmdInfo->fgSetQuery, 0,
				       WLAN_STATUS_FAILURE);
		}

		cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry,
				  struct QUE_ENTRY *);
	}

	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This is a routine, which is used to release tx cmd after bus suspend
 *
 * \param prAdapter      Pointer of Adapter Data Structure
 */
/*----------------------------------------------------------------------------*/
void wlanReleaseAllTxCmdQueue(struct ADAPTER *prAdapter)
{
	struct QUE *prCmdQue;
	struct QUE rTempCmdQue;
	struct QUE *prTempCmdQue = &rTempCmdQue;
	struct QUE_ENTRY *prQueueEntry = (struct QUE_ENTRY *) NULL;
	struct CMD_INFO *prCmdInfo = (struct CMD_INFO *) NULL;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);

	/* dump queue info before release for debug */
	cmdBufDumpCmdQueue(&prAdapter->rPendingCmdQueue,
				   "waiting response CMD queue");
	cmdBufDumpCmdQueue(&prAdapter->rTxCmdQueue,
				   "Tx CMD queue");

	DBGLOG(OID, INFO, "Remove all pending Cmd\n");
	/* 1: Clear Pending OID */
	wlanReleasePendingOid(prAdapter, 1);

	/* 2: Clear other pending cmd in prAdapter->rPendingCmdQueue */
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING);

	prCmdQue = &prAdapter->rPendingCmdQueue;
	QUEUE_MOVE_ALL(prTempCmdQue, prCmdQue);

	QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, struct QUE_ENTRY *);
	while (prQueueEntry) {
		prCmdInfo = (struct CMD_INFO *) prQueueEntry;

		if (prCmdInfo->pfCmdTimeoutHandler) {
			prCmdInfo->pfCmdTimeoutHandler(prAdapter, prCmdInfo);
		} else if (prCmdInfo->fgIsOid) {
			kalOidComplete(prAdapter->prGlueInfo,
				       prCmdInfo->fgSetQuery, 0,
				       WLAN_STATUS_FAILURE);
		}

		cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry,
				  struct QUE_ENTRY *);
	}

	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING);

	/* 3. clear tx cmd queue*/
	wlanClearTxCommandQueue(prAdapter);

	/* 4. clear tx cmd done queue*/
	wlanClearTxCommandDoneQueue(prAdapter);

}

void
wlanWaitCfg80211SuspendDone(struct GLUE_INFO *prGlueInfo)
{
	uint8_t u1Count = 0;

	while (!(test_bit(SUSPEND_FLAG_CLEAR_WHEN_RESUME,
		&prGlueInfo->prAdapter->ulSuspendFlag))) {
		if (u1Count > HIF_SUSPEND_MAX_WAIT_TIME) {
			DBGLOG(HAL, ERROR, "cfg80211 not suspend\n");
			break;
		}
		usleep_range(5000, 6000);
		u1Count++;
	}
}

/* Translate Decimals string to Hex
** The result will be put in a 2bytes variable.
** Integer part will occupy the left most 3 bits, and decimal part is in the
** left 13 bits
** Integer part can be parsed by kstrtou16, decimal part should be translated by
** mutiplying
** 16 and then pick integer part.
** For example
*/
uint32_t wlanDecimalStr2Hexadecimals(uint8_t *pucDecimalStr, uint16_t *pu2Out)
{
	uint8_t aucDecimalStr[32] = {0,};
	uint8_t *pucDecimalPart = NULL;
	uint8_t *tmp = NULL;
	uint32_t u4Result = 0;
	uint32_t u4Ret = 0;
	uint32_t u4Degree = 0;
	uint32_t u4Remain = 0;
	uint8_t ucAccuracy = 4; /* Hex decimals accuarcy is 4 bytes */
	uint32_t u4Base = 1;

	if (!pu2Out || !pucDecimalStr)
		return 1;

	while (*pucDecimalStr == '0')
		pucDecimalStr++;
	kalStrnCpy(aucDecimalStr, pucDecimalStr, sizeof(aucDecimalStr) - 1);
	pucDecimalPart = strchr(aucDecimalStr, '.');
	if (!pucDecimalPart) {
		DBGLOG(INIT, INFO, "No decimal part, ori str %s\n",
		       pucDecimalStr);
		goto integer_part;
	}
	*pucDecimalPart++ = 0;
	/* get decimal degree */
	tmp = pucDecimalPart + strlen(pucDecimalPart);
	do {
		if (tmp == pucDecimalPart) {
			DBGLOG(INIT, INFO,
			       "Decimal part are all 0, ori str %s\n",
			       pucDecimalStr);
			goto integer_part;
		}
		tmp--;
	} while (*tmp == '0');

	*(++tmp) = 0;
	u4Degree = (uint32_t)(tmp - pucDecimalPart);
	/* if decimal part is not 0, translate it to hexadecimal decimals */
	/* Power(10, degree) */
	for (; u4Remain < u4Degree; u4Remain++)
		u4Base *= 10;

	while (*pucDecimalPart == '0')
		pucDecimalPart++;

	u4Ret = kstrtou32(pucDecimalPart, 0, &u4Remain);
	if (u4Ret) {
		DBGLOG(INIT, ERROR, "Parse decimal str %s error, degree %u\n",
			   pucDecimalPart, u4Degree);
		return u4Ret;
	}

	do {
		u4Remain *= 16;
		u4Result |= (u4Remain / u4Base) << ((ucAccuracy-1) * 4);
		u4Remain %= u4Base;
		ucAccuracy--;
	} while (u4Remain && ucAccuracy > 0);
	/* Each Hex Decimal byte was left shift more than 3 bits, so need
	** right shift 3 bits at last
	** For example, mmmnnnnnnnnnnnnn.
	** mmm is integer part, n represents decimals part.
	** the left most 4 n are shift 9 bits. But in for loop, we shift 12 bits
	**/
	u4Result >>= 3;
	u4Remain = 0;

integer_part:
	u4Ret = kstrtou32(aucDecimalStr, 0, &u4Remain);
	u4Result |= u4Remain << 13;

	if (u4Ret)
		DBGLOG(INIT, ERROR, "Parse integer str %s error\n",
		       aucDecimalStr);
	else {
		*pu2Out = u4Result & 0xffff;
		DBGLOG(INIT, TRACE, "Result 0x%04x\n", *pu2Out);
	}
	return u4Ret;
}

uint32_t wlanGetSupportedFeatureSet(IN struct GLUE_INFO *prGlueInfo)
{
	uint32_t u4FeatureSet = WIFI_HAL_FEATURE_SET;
	struct REG_INFO *prRegInfo;

	prRegInfo = kalGetConfiguration(prGlueInfo);
	if ((prRegInfo != NULL) && (prRegInfo->ucSupport5GBand))
		u4FeatureSet |= WIFI_FEATURE_INFRA_5G;

	return u4FeatureSet;
}

uint32_t wlanSetEd(IN struct ADAPTER *prAdapter, int32_t u4EdVal2G,
	int32_t u4EdVal5G, uint32_t u4Sel)
{
	uint32_t u4BufLen = 0;
	struct GLUE_INFO *prGlueInfo = prAdapter->prGlueInfo;
	struct PARAM_CUSTOM_SW_CTRL_STRUCT rSwCtrlInfo;

	rSwCtrlInfo.u4Id = CMD_SW_DBGCTL_ADVCTL_SET_ID + CMD_ADVCTL_ED_ID;
	rSwCtrlInfo.u4Data = ((u4EdVal2G & 0xFF) |
		((u4EdVal5G & 0xFF)<<16) | (u4Sel << 31));
	DBGLOG(REQ, INFO, "rSwCtrlInfo.u4Data=0x%x,\n", rSwCtrlInfo.u4Data);
	return kalIoctl(prGlueInfo, wlanoidSetSwCtrlWrite, &rSwCtrlInfo,
		sizeof(rSwCtrlInfo), FALSE, FALSE, TRUE, &u4BufLen);
}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is a wrapper to send eapol offload (rekey) command
* with PN sync consideration
*
* @param prGlueInfo  Pointer of prGlueInfo Data Structure
*
* @return VOID
*/
/*----------------------------------------------------------------------------*/
int wlanSuspendRekeyOffload(struct GLUE_INFO *prGlueInfo, uint8_t ucRekeyMode)
{
	uint32_t u4BufLen;
	struct PARAM_GTK_REKEY_DATA *prGtkData;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	int32_t i4Rslt = -EINVAL;
#if CFG_SUPPORT_REPLAY_DETECTION
	struct GL_DETECT_REPLAY_INFO *prDetRplyInfo = NULL;
	uint8_t ucCurKeyId;
	uint8_t ucRpyOffload;
#endif

	ASSERT(prGlueInfo);

#if CFG_SUPPORT_REPLAY_DETECTION
	ucRpyOffload = prGlueInfo->prAdapter->rWifiVar.ucRpyDetectOffload;

	if ((ucRekeyMode == GTK_REKEY_CMD_MODE_SET_BCMC_PN) &&
		(ucRpyOffload == FALSE)) {
		DBGLOG(RSN, INFO,
			"Set PN to fw, but feature off. no action\n");
		return WLAN_STATUS_SUCCESS;
	}

	if ((ucRekeyMode == GTK_REKEY_CMD_MODE_GET_BCMC_PN) &&
		(ucRpyOffload == FALSE)) {
		DBGLOG(RSN, INFO,
			"Get PN from fw, but feature off. no action\n");
		return WLAN_STATUS_SUCCESS;
	}
#endif

	prGtkData =
		(struct PARAM_GTK_REKEY_DATA *) kalMemAlloc(sizeof(
				struct PARAM_GTK_REKEY_DATA), VIR_MEM_TYPE);

	if (!prGtkData)
		return WLAN_STATUS_SUCCESS;

	kalMemZero(prGtkData, sizeof(struct PARAM_GTK_REKEY_DATA));

	/* if enable, FW rekey offload. if disable, rekey back to supplicant */
	prGtkData->ucRekeyMode = ucRekeyMode;
	DBGLOG(RSN, INFO, "GTK Rekey ucRekeyMode = %d\n", ucRekeyMode);

	if (ucRekeyMode == GTK_REKEY_CMD_MODE_OFFLOAD_ON) {
		DBGLOG(RSN, INFO, "kek\n");
		DBGLOG_MEM8(RSN, INFO, (uint8_t *)prGlueInfo->rWpaInfo.aucKek,
			NL80211_KEK_LEN);
		DBGLOG(RSN, INFO, "kck\n");
		DBGLOG_MEM8(RSN, INFO, (uint8_t *)prGlueInfo->rWpaInfo.aucKck,
			NL80211_KCK_LEN);
		DBGLOG(RSN, INFO, "replay count\n");
		DBGLOG_MEM8(RSN, INFO,
			(uint8_t *)prGlueInfo->rWpaInfo.aucReplayCtr,
			NL80211_REPLAY_CTR_LEN);

		kalMemCopy(prGtkData->aucKek, prGlueInfo->rWpaInfo.aucKek,
			NL80211_KEK_LEN);
		kalMemCopy(prGtkData->aucKck, prGlueInfo->rWpaInfo.aucKck,
			NL80211_KCK_LEN);
		kalMemCopy(prGtkData->aucReplayCtr,
			prGlueInfo->rWpaInfo.aucReplayCtr,
			NL80211_REPLAY_CTR_LEN);

		prGtkData->ucBssIndex =
			prGlueInfo->prAdapter->prAisBssInfo->ucBssIndex;

		prGtkData->u4Proto = NL80211_WPA_VERSION_2;
		if (prGlueInfo->rWpaInfo.u4WpaVersion ==
					IW_AUTH_WPA_VERSION_WPA)
			prGtkData->u4Proto = NL80211_WPA_VERSION_1;

		if (prGlueInfo->rWpaInfo.u4CipherPairwise ==
					IW_AUTH_CIPHER_TKIP)
			prGtkData->u4PairwiseCipher = BIT(3);
		else if (prGlueInfo->rWpaInfo.u4CipherPairwise ==
					IW_AUTH_CIPHER_CCMP)
			prGtkData->u4PairwiseCipher = BIT(4);
		else {
			kalMemFree(prGtkData, VIR_MEM_TYPE,
				sizeof(PARAM_GTK_REKEY_DATA));
			return WLAN_STATUS_SUCCESS;
		}

		if (prGlueInfo->rWpaInfo.u4CipherGroup ==
					IW_AUTH_CIPHER_TKIP)
			prGtkData->u4GroupCipher    = BIT(3);
		else if (prGlueInfo->rWpaInfo.u4CipherGroup ==
					IW_AUTH_CIPHER_CCMP)
			prGtkData->u4GroupCipher    = BIT(4);
		else {
			kalMemFree(prGtkData, VIR_MEM_TYPE,
				sizeof(PARAM_GTK_REKEY_DATA));
			return WLAN_STATUS_SUCCESS;
		}

		prGtkData->u4KeyMgmt = prGlueInfo->rWpaInfo.u4KeyMgmt;
		prGtkData->u4MgmtGroupCipher = 0;

	}

	if (ucRekeyMode == GTK_REKEY_CMD_MODE_OFLOAD_OFF) {
		/* inform FW disable EAPOL offload */
		DBGLOG(RSN, INFO, "Disable EAPOL offload\n");
	}

#if CFG_SUPPORT_REPLAY_DETECTION
	if (ucRekeyMode == GTK_REKEY_CMD_MODE_RPY_OFFLOAD_ON)
		DBGLOG(RSN, INFO,
			"ucRekeyMode(rpy rekey offload on): %d\n",
			ucRekeyMode);

	if (ucRekeyMode == GTK_REKEY_CMD_MODE_RPY_OFFLOAD_OFF)
		DBGLOG(RSN, INFO,
			"ucRekeyMode(rpy rekey offload off): %d\n",
			ucRekeyMode);

	if ((ucRekeyMode == GTK_REKEY_CMD_MODE_SET_BCMC_PN) &&
	   (ucRpyOffload == TRUE)) {

		prDetRplyInfo = &prGlueInfo->prDetRplyInfo;
		ucCurKeyId = prDetRplyInfo->ucCurKeyId;
		prGtkData->ucCurKeyId = ucCurKeyId;
		DBGLOG_MEM8(RSN, INFO,
			(uint8_t *)prGtkData->aucReplayCtr,
			NL80211_REPLAY_CTR_LEN);
		kalMemCopy(prGtkData->aucReplayCtr,
			prDetRplyInfo->arReplayPNInfo[ucCurKeyId].auPN,
			NL80211_REPLAY_CTR_LEN);

		/* set bc/mc PN zero before suspend */
		kalMemZero(prDetRplyInfo->arReplayPNInfo[ucCurKeyId].auPN,
			NL80211_REPLAY_CTR_LEN);
	}

	if ((ucRekeyMode == GTK_REKEY_CMD_MODE_GET_BCMC_PN) &&
		(ucRpyOffload == TRUE)) {
		prGtkData->ucBssIndex =
			prGlueInfo->prAdapter->prAisBssInfo->ucBssIndex;
	}
#endif

	rStatus = kalIoctl(prGlueInfo,
				wlanoidSetGtkRekeyData,
				prGtkData, sizeof(struct PARAM_GTK_REKEY_DATA),
				FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, ERROR, "Suspend rekey data err:%x\n", rStatus);
	else
		i4Rslt = 0;

	kalMemFree(prGtkData, VIR_MEM_TYPE, sizeof(PARAM_GTK_REKEY_DATA));

	return i4Rslt;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is a wrapper to send power-saving mode command
*        when AIS enter wow, and send WOW command
*        Also let GC/GO/AP enter deactivate state to enter TOP sleep
*
* @param prGlueInfo                     Pointer of prGlueInfo Data Structure
*
* @return VOID
*/
/*----------------------------------------------------------------------------*/
void wlanSuspendPmHandle(struct GLUE_INFO *prGlueInfo)
{
	uint8_t idx;
	enum PARAM_POWER_MODE ePwrMode;
	/* struct BSS_INFO *prBssInfo; */
	uint8_t ucKekZeroCnt = 0;
	uint8_t ucKckZeroCnt = 0;
	uint8_t ucGtkOffload = TRUE;
	uint8_t i = 0;
#if CFG_SUPPORT_REPLAY_DETECTION
	struct GL_DETECT_REPLAY_INFO *prDetRplyInfo = NULL;
	uint8_t ucKeyIdx = 0;
	uint8_t ucRpyOffload = 0;
#endif
	struct STA_RECORD *prStaRec;
	struct RX_BA_ENTRY *prRxBaEntry;

#if CFG_SUPPORT_ADVANCE_CONTROL
	if (prGlueInfo->prAdapter->u4IsKeepFullPwrBitmap)
		wlanKeepFullPwr(prGlueInfo->prAdapter, FALSE);
#endif
	/* if cfg EAPOL offload is 0, we set rekey offload when enter wow */
	if (!prGlueInfo->prAdapter->rWifiVar.ucEapolOffload) {

		/*
		 * check if KCK, KEK not sync from supplicant.
		 * if no these info updated from supplicant,
		 * disable GTK offload feature.
		 */
		for (i = 0; i < NL80211_KEK_LEN; i++) {
			if (prGlueInfo->rWpaInfo.aucKek[i] == 0x00)
				ucKekZeroCnt++;
		}

		for (i = 0; i < NL80211_KCK_LEN; i++) {
			if (prGlueInfo->rWpaInfo.aucKck[i] == 0x00)
				ucKckZeroCnt++;
		}

		if ((ucKekZeroCnt == NL80211_KCK_LEN) ||
				(ucKckZeroCnt == NL80211_KCK_LEN)) {
			DBGLOG(RSN, INFO, "no offload, no KCK/KEK from cfg\n");

			ucGtkOffload = FALSE;
			/* set bc/mc replay detection off to fw */
			wlanSuspendRekeyOffload(prGlueInfo,
				GTK_REKEY_CMD_MODE_RPY_OFFLOAD_OFF);
		}

#if CFG_SUPPORT_REPLAY_DETECTION
		ucRpyOffload =
			prGlueInfo->prAdapter->rWifiVar.ucRpyDetectOffload;

		if (ucRpyOffload && ucGtkOffload)
			wlanSuspendRekeyOffload(prGlueInfo,
				GTK_REKEY_CMD_MODE_SET_BCMC_PN);
#endif

		if (ucGtkOffload)
			wlanSuspendRekeyOffload(prGlueInfo,
				GTK_REKEY_CMD_MODE_OFFLOAD_ON);

#if CFG_SUPPORT_REPLAY_DETECTION
		prDetRplyInfo = &prGlueInfo->prDetRplyInfo;
		for (ucKeyIdx = 0; ucKeyIdx < 4; ucKeyIdx++) {
			kalMemZero(prDetRplyInfo->arReplayPNInfo[ucKeyIdx].auPN,
				NL80211_KEYRSC_LEN);
		}
#endif

		DBGLOG(HAL, STATE, "Suspend rekey offload\n");
	}

	/* Pending Timer related to CNM need to check and
	 * perform corresponding timeout handler. Without it,
	 * Might happen CNM abnormal after resume or during suspend.
	 */
	cnmCheckPendingTimer(prGlueInfo->prAdapter);

	/* 1) wifi cfg "Wow" is true, 2) wow or AdvPws is enable
	 * 3) WIfI connected => execute WOW flow
	 * Send power-saving cmd when enter wow, even w/o cfg80211 support
	 */
	if (prGlueInfo->prAdapter->rWifiVar.ucWow &&
		(prGlueInfo->prAdapter->rWowCtrl.fgWowEnable ||
		prGlueInfo->prAdapter->rWifiVar.ucAdvPws)) {
		if (kalGetMediaStateIndicated(prGlueInfo) ==
			PARAM_MEDIA_STATE_CONNECTED) {
			/* AIS bss enter wow power mode, default fast pws */
			ePwrMode = prGlueInfo->prAdapter->rWifiVar.ucWowPwsMode;
			idx = prGlueInfo->prAdapter->prAisBssInfo->ucBssIndex;
			nicConfigPowerSaveWowProfile(prGlueInfo->prAdapter, idx,
				ePwrMode, FALSE, TRUE);
			DBGLOG(HAL, STATE, "Wow AIS_idx:%d, pwr mode:%d\n",
				idx, ePwrMode);

			DBGLOG(HAL, STATE, "enter WOW flow\n");
			kalWowProcess(prGlueInfo, TRUE);
		}
	}

	/* After resuming, WinStart will unsync with AP's SN.
	 * Set fgFirstSnToWinStart for all valid BA entry before suspend.
	 */
	for (idx = 0; idx < CFG_STA_REC_NUM; idx++) {
		prStaRec = cnmGetStaRecByIndex(prGlueInfo->prAdapter, idx);
		if (!prStaRec)
			continue;

		for (i = 0; i < CFG_RX_MAX_BA_TID_NUM; i++) {
			prRxBaEntry = prStaRec->aprRxReorderParamRefTbl[i];
			if (!prRxBaEntry || !(prRxBaEntry->fgIsValid))
				continue;

			prRxBaEntry->fgFirstSnToWinStart = TRUE;
		}
	}
}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is to restore power-saving mode command when leave wow
*        But ignore GC/GO/AP role
*
* @param prGlueInfo                     Pointer of prGlueInfo Data Structure
*
* @return VOID
*/
/*----------------------------------------------------------------------------*/
void wlanResumePmHandle(struct GLUE_INFO *prGlueInfo)
{
#if 1
	enum PARAM_POWER_MODE ePwrMode = Param_PowerModeCAM;
	uint8_t ucKekZeroCnt = 0;
	uint8_t ucKckZeroCnt = 0;
	uint8_t ucGtkOffload = TRUE;
	uint8_t i = 0;
	struct ADAPTER *prAdapter;
#if CFG_SUPPORT_REPLAY_DETECTION
	struct GL_DETECT_REPLAY_INFO *prDetRplyInfo = NULL;
	uint8_t ucKeyIdx = 0;
	uint8_t ucRpyOffload = 0;
#endif

	prAdapter = prGlueInfo->prAdapter;

	/* if cfg EAPOL offload disble, we disable offload when leave wow */
	if (!prGlueInfo->prAdapter->rWifiVar.ucEapolOffload) {

		/*
		 * check if KCK, KEK not sync from supplicant.
		 * if no these info updated from supplicant,
		 *disable GTK offload feature.
		 */
		for (i = 0; i < NL80211_KEK_LEN; i++) {
			if (prGlueInfo->rWpaInfo.aucKek[i] == 0x00)
				ucKekZeroCnt++;
		}

		for (i = 0; i < NL80211_KCK_LEN; i++) {
			if (prGlueInfo->rWpaInfo.aucKck[i] == 0x00)
				ucKckZeroCnt++;
		}

		if ((ucKekZeroCnt == NL80211_KCK_LEN) ||
				(ucKckZeroCnt == NL80211_KCK_LEN)) {

			DBGLOG(RSN, INFO, "no offload, no KCK/KEK from cfg\n");

			ucGtkOffload = FALSE;
			/* set bc/mc replay detection off to fw */
			wlanSuspendRekeyOffload(prGlueInfo,
				GTK_REKEY_CMD_MODE_RPY_OFFLOAD_OFF);
		}

#if CFG_SUPPORT_REPLAY_DETECTION
		prDetRplyInfo = &prGlueInfo->prDetRplyInfo;

		/* Reset BC/MC KeyRSC to prevent incorrect replay detection. */
		for (ucKeyIdx = 0; ucKeyIdx < 4; ucKeyIdx++) {
			kalMemZero(prDetRplyInfo->arReplayPNInfo[ucKeyIdx].auPN,
				NL80211_KEYRSC_LEN);
		}

		ucRpyOffload =
			prGlueInfo->prAdapter->rWifiVar.ucRpyDetectOffload;

		/* sync BC/MC PN */
		if (ucRpyOffload && ucGtkOffload)
			wlanSuspendRekeyOffload(prGlueInfo,
				GTK_REKEY_CMD_MODE_GET_BCMC_PN);
#endif

		if (ucGtkOffload) {
			wlanSuspendRekeyOffload(prGlueInfo,
				GTK_REKEY_CMD_MODE_OFLOAD_OFF);

			DBGLOG(HAL, STATE, "Resume rekey offload disable\n");
		}
	}

	if (prGlueInfo->prAdapter->rWifiVar.ucWow &&
		(prGlueInfo->prAdapter->rWowCtrl.fgWowEnable ||
		prGlueInfo->prAdapter->rWifiVar.ucAdvPws)) {
		if (kalGetMediaStateIndicated(prGlueInfo) ==
			PARAM_MEDIA_STATE_CONNECTED) {
			DBGLOG(HAL, STATE, "leave WOW. AIS BssIdx:%d\n",
				prAdapter->prAisBssInfo->ucBssIndex);
			kalWowProcess(prGlueInfo, FALSE);

			/* Restore AIS pws when leave wow, ignore ePwrMode */
			nicConfigPowerSaveWowProfile(prGlueInfo->prAdapter,
				prAdapter->prAisBssInfo->ucBssIndex,
				ePwrMode, FALSE, FALSE);
		}
	}
#endif
#if CFG_SUPPORT_ADVANCE_CONTROL
	if (prGlueInfo->prAdapter->u4IsKeepFullPwrBitmap)
		wlanKeepFullPwr(prGlueInfo->prAdapter, TRUE);
#endif

}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is to wake up WiFi
*
* @param prAdapter      Pointer to the Adapter structure.
*
* @return WLAN_STATUS_SUCCESS
*         WLAN_STATUS_FAILURE
*/
/*----------------------------------------------------------------------------*/
uint32_t wlanWakeUpWiFi(IN struct ADAPTER *prAdapter)
{
	u_int8_t fgReady;

	if (!prAdapter)
		return WLAN_STATUS_FAILURE;

	HAL_WIFI_FUNC_READY_CHECK(prAdapter, prChipInfo->sw_ready_bits,
				  &fgReady);

	if (fgReady) {
		DBGLOG(INIT, INFO,
		       "Wi-Fi is already ON!, turn off before FW DL!\n");
#if defined(_HIF_USB)
		wlanSendDummyCmd(prAdapter, FALSE); /* for deep sleep mode */
		nicEnableInterrupt(prAdapter); /* clear USB EPIN FIFO */
#endif

		if (wlanPowerOffWifi(prAdapter) != WLAN_STATUS_SUCCESS)
			return WLAN_STATUS_FAILURE;

	}

	nicpmWakeUpWiFi(prAdapter);
	HAL_HIF_INIT(prAdapter);

	return WLAN_STATUS_SUCCESS;
}

void disconnect_sta(struct ADAPTER *prAdapter, struct STA_RECORD *sta_rec)
{
	struct GLUE_INFO *glue_info;
	struct MSG_AIS_ABORT *ais_abort_msg = NULL;
	struct MSG_P2P_CONNECTION_ABORT *p2p_abot_msg = NULL;
	struct BSS_INFO *p2p_bss_info = NULL;
	uint8_t role_idx = 0;


	if (!prAdapter) {
		DBGLOG(MEM, ERROR, "prAdapter is NULL\n");
		return;
	}
	if (!sta_rec) {
		DBGLOG(MEM, ERROR, "sta_rec is NULL\n");
		return;
	}

	glue_info = prAdapter->prGlueInfo;

	switch (sta_rec->eStaType) {
	case STA_TYPE_LEGACY_AP:
		if (prAdapter->rAcpiState == ACPI_STATE_D3)
			return;
		/* prepare message to AIS */
		prAdapter->rWifiVar.rConnSettings.fgIsConnReqIssued = FALSE;
		/* Send AIS Abort Message */
		ais_abort_msg =
			(struct MSG_AIS_ABORT *)
			cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
				    sizeof(struct MSG_AIS_ABORT));
		if (ais_abort_msg) {
			ais_abort_msg->rMsgHdr.eMsgId =
				MID_OID_AIS_FSM_JOIN_REQ;
			ais_abort_msg->ucReasonOfDisconnect =
				DISCONNECT_REASON_CODE_DISASSOCIATED;
			ais_abort_msg->fgDelayIndication = FALSE;

			DBGLOG(AIS, INFO,
			       "Disconnect STA["MACSTR"] type:0x%x\n",
			       MAC2STR(sta_rec->aucMacAddr), sta_rec->eStaType);

			mboxSendMsg(prAdapter, MBOX_ID_0,
				    (struct MSG_HDR *) ais_abort_msg,
				    MSG_SEND_METHOD_UNBUF);
#define DISCONNECT_STATUS WLAN_STATUS_MEDIA_DISCONNECT
			/* indicate for disconnection */
			if (kalGetMediaStateIndicated(glue_info) ==
			    PARAM_MEDIA_STATE_CONNECTED) {
				kalIndicateStatusAndComplete(glue_info,
							     DISCONNECT_STATUS,
							     NULL,
							     0);
			}
#undef DISCONNECT_STATUS
		}
		break;
	case STA_TYPE_LEGACY_CLIENT:
	case STA_TYPE_P2P_GC:
	case STA_TYPE_P2P_GO:
		p2p_abot_msg =
			(struct MSG_P2P_CONNECTION_ABORT *)
			cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
				    sizeof(struct MSG_P2P_CONNECTION_ABORT));

		if (p2p_abot_msg) {
			p2p_bss_info =
				GET_BSS_INFO_BY_INDEX(prAdapter,
						      sta_rec->ucBssIndex);
			role_idx = p2p_bss_info->u4PrivateData;
			p2p_abot_msg->rMsgHdr.eMsgId =
				MID_MNY_P2P_CONNECTION_ABORT;
			COPY_MAC_ADDR(p2p_abot_msg->aucTargetID,
				      sta_rec->aucMacAddr);

			p2p_abot_msg->u2ReasonCode = REASON_CODE_UNSPECIFIED;
			p2p_abot_msg->ucRoleIdx = role_idx;
			p2p_abot_msg->fgSendDeauth = FALSE;

			DBGLOG(P2P, INFO,
			       "Disconnect STA["MACSTR"] type:0x%x\n",
			       MAC2STR(sta_rec->aucMacAddr), sta_rec->eStaType);

			mboxSendMsg(prAdapter, MBOX_ID_0,
				    (struct MSG_HDR *)p2p_abot_msg,
				    MSG_SEND_METHOD_UNBUF);
		}
		break;
	default:
		break;
	}
}

uint32_t wlanData2RateInMs(uint32_t data, uint32_t interval)
{
	/* interval in millisecond
	* TODO : 32/64 bits overflow problem
	*
	* Max $data in 32bits system is 2^12 * 2^20 = 4096M
	* Consider this basic Data2Rate formula : data*MSEC_PER_SEC/interval.
	* We should reserve 10-bits for multiplication to avoid data overflow,
	* and the formula becomes 4M * MSEC_PER_SEC
	* If $data is over 4M, the result would be overflow in this case.
	*
	* Calculation optimized to avoid data overflow
	*/
	uint32_t rate;

	if (interval == MSEC_PER_SEC)
		rate = data;
	else if (data > interval)
		rate = (data / interval) * MSEC_PER_SEC;
	else
		rate = (data * MSEC_PER_SEC) / interval;
	return rate;
}

#if CFG_RCPI_COMPENSATION
void wlanLoadEfuseRxFELoss(struct ADAPTER *prAdapter)
{
	uint32_t u4Efuse_addr = 0, u4Index;
	uint8_t ucGroup, ucAnt;
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	struct PARAM_CUSTOM_ACCESS_EFUSE *prAccessEfuseInfo = NULL;

	prGlueInfo = prAdapter->prGlueInfo;


	/* allocate memory for Access Efuse Info */
	prAccessEfuseInfo = (struct PARAM_CUSTOM_ACCESS_EFUSE *)
			kalMemAlloc(sizeof(struct PARAM_CUSTOM_ACCESS_EFUSE),
			VIR_MEM_TYPE);
	if (prAccessEfuseInfo == NULL)
		goto label_exit;

	kalMemZero(prAccessEfuseInfo, sizeof(struct PARAM_CUSTOM_ACCESS_EFUSE));

	for (ucAnt = 0; ucAnt < MAX_ANTENNA_NUM; ucAnt++) {
		for (ucGroup = 0; ucGroup < FELOSS_CH_GROUP_NUM; ucGroup++) {
			u4Efuse_addr = FELossOffset[ucAnt][ucGroup];
			prAccessEfuseInfo->u4Address =
			(u4Efuse_addr / EFUSE_BLOCK_SIZE) * EFUSE_BLOCK_SIZE;
			u4Index = u4Efuse_addr % EFUSE_BLOCK_SIZE;

			if (u4Index == 0) {
				rStatus = kalIoctl(prGlueInfo,
					wlanoidQueryProcessAccessEfuseRead,
					prAccessEfuseInfo, sizeof(
					struct PARAM_CUSTOM_ACCESS_EFUSE),
					TRUE, TRUE, TRUE, &u4BufLen);
			}

			RxFELoss[ucAnt][ucGroup] =
			(prAdapter->aucEepromVaule[u4Index] & RX_FELOSS_MASK)
			>> RX_FELOSS_OFFSET;

			DBGLOG(REQ, LOUD, "Ant[%d] Gruoup[%d] FEloss[%d]",
			      ucAnt, ucGroup, RxFELoss[ucAnt][ucGroup]);
		}
	}
label_exit:
	if (prAccessEfuseInfo != NULL)
		kalMemFree(prAccessEfuseInfo, VIR_MEM_TYPE,
		sizeof(struct PARAM_CUSTOM_ACCESS_EFUSE));
}

void wlanLoadBufferbinRxFELoss(struct ADAPTER *prAdapter)
{
	uint32_t u4Efuse_addr = 0;
	uint8_t ucGroup, ucAnt;

	for (ucGroup = 0; ucGroup < FELOSS_CH_GROUP_NUM; ucGroup++) {
		for (ucAnt = 0; ucAnt < MAX_ANTENNA_NUM; ucAnt++) {
			u4Efuse_addr = FELossOffset[ucAnt][ucGroup];
			RxFELoss[ucAnt][ucGroup] =
			(uacEEPROMImage[u4Efuse_addr] & RX_FELOSS_MASK)
			>> RX_FELOSS_OFFSET;
			DBGLOG(REQ, LOUD, "Ant[%d] Gruoup[%d] FEloss[%d]",
			      ucAnt, ucGroup, RxFELoss[ucAnt][ucGroup]);
		}
	}
}

void wlanUpdateRxFELoss(IN struct SW_RFB *prSwRfb)
{
	struct HW_MAC_RX_DESC *prRxStatus;
	struct HW_MAC_RX_STS_GROUP_3 *prRxStatusGroup3;
	uint8_t ucChanNum, ucGroupIdx = 0;
	uint8_t ucRcpi0, ucRcpi1;
	uint8_t ucRxFELoss0, ucRxFELoss1;

	prRxStatus = prSwRfb->prRxStatus;
	prRxStatusGroup3 = prSwRfb->prRxStatusGroup3;
	ucChanNum = HAL_RX_STATUS_GET_CHNL_NUM(prRxStatus);

	/* Check channel group */
	if (HAL_RX_STATUS_GET_RF_BAND(prRxStatus) == BAND_2G4) {
		ucGroupIdx = 0;
	} else {
		/* cyclic ch group process for high ch in group 0 */
		if (ucChanNum > A_BAND_FELOSS_BOUND_2)
			ucGroupIdx = 1;
		else if (ucChanNum > A_BAND_FELOSS_BOUND_1)
			ucGroupIdx = 3; /* BOUND_2 ~ BOUND_1 */
		else if (ucChanNum > A_BAND_FELOSS_BOUND_0)
			ucGroupIdx = 2; /* BOUND_1 ~ BOUND_0 */
		else
			ucGroupIdx = 1; /* BOUND_0 ~ */
	}
	ucRxFELoss0 = RxFELoss[ANTENNA_WF0][ucGroupIdx];
	ucRxFELoss1 = RxFELoss[ANTENNA_WF1][ucGroupIdx];

	DBGLOG(REQ, LOUD,
	       "ucChanNum = %d, Group=%d ucRxFELoss0 = %d ucRxFELoss1 = %d\n",
	       ucChanNum, ucGroupIdx, ucRxFELoss0, ucRxFELoss1);

	/* Do the compensation */
	ucRcpi0 = HAL_RX_STATUS_GET_RCPI0(prRxStatusGroup3);
	if (ucRcpi0 < RCPI_MEASUREMENT_NOT_AVAILABLE)
		HAL_RX_STATUS_SET_RCPI0(prRxStatusGroup3,
					(ucRcpi0 + ucRxFELoss0));

	ucRcpi1 = HAL_RX_STATUS_GET_RCPI1(prRxStatusGroup3);
	if (ucRcpi1 < RCPI_MEASUREMENT_NOT_AVAILABLE)
		HAL_RX_STATUS_SET_RCPI1(prRxStatusGroup3,
					(ucRcpi1 + ucRxFELoss1));
}

uint8_t wlanGetCurrChRxFELoss(struct ADAPTER *prAdapter,
					uint8_t ucStaIdx, uint8_t ucAnt)
{
	uint8_t ucChanNum = 0, ucGroupIdx = 0, ucRxFELoss = 0;
	struct STA_RECORD *prStaRec;

	prStaRec = cnmGetStaRecByIndex(prAdapter, ucStaIdx);

	if (prStaRec == NULL)
		return ucRxFELoss;

	if (IS_STA_IN_AIS(prStaRec)) {
		if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) ==
			PARAM_MEDIA_STATE_CONNECTED)
			ucChanNum = prAdapter->prAisBssInfo->ucPrimaryChannel;
	} else if (prAdapter->fgIsP2PRegistered && IS_STA_IN_P2P(prStaRec)) {
		ucChanNum = prAdapter->rWifiVar.ucApChannel;
	} else {
		DBGLOG(REQ, WARN, "Cannot Get Channel\n");
	}

	if (ucChanNum <= HW_CHNL_NUM_MAX_2G4) {
		ucGroupIdx = 0;
	} else {
		/* cyclic ch group process for high ch in group 0 */
		if (ucChanNum > A_BAND_FELOSS_BOUND_2)
			ucGroupIdx = 1;
		else if (ucChanNum > A_BAND_FELOSS_BOUND_1)
			ucGroupIdx = 3; /* BOUND_2 ~ BOUND_1 */
		else if (ucChanNum > A_BAND_FELOSS_BOUND_0)
			ucGroupIdx = 2; /* BOUND_1 ~ BOUND_0 */
		else
			ucGroupIdx = 1; /* BOUND_0 ~ */
	}
	ucRxFELoss = RxFELoss[ucAnt][ucGroupIdx];

	return ucRxFELoss;
}
#endif /* CFG_RCPI_COMPENSATION */
#if CFG_SUPPORT_HW_1T2R
void wlanLoadEfuse1T2R(struct ADAPTER *prAdapter)
{
	uint32_t u4Efuse_addr = 0, u4Index;
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	struct PARAM_CUSTOM_ACCESS_EFUSE *prAccessEfuseInfo = NULL;

	prGlueInfo = prAdapter->prGlueInfo;


	/* allocate memory for Access Efuse Info */
	prAccessEfuseInfo = (struct PARAM_CUSTOM_ACCESS_EFUSE *)
			kalMemAlloc(sizeof(struct PARAM_CUSTOM_ACCESS_EFUSE),
			VIR_MEM_TYPE);
	if (prAccessEfuseInfo == NULL)
		goto label_exit;

	kalMemZero(prAccessEfuseInfo, sizeof(struct PARAM_CUSTOM_ACCESS_EFUSE));
	u4Efuse_addr = EFUSE_1T2R_ADDR;
	prAccessEfuseInfo->u4Address =
	(u4Efuse_addr / EFUSE_BLOCK_SIZE) * EFUSE_BLOCK_SIZE;
	u4Index = u4Efuse_addr % EFUSE_BLOCK_SIZE;
	rStatus = kalIoctl(prGlueInfo,
			wlanoidQueryProcessAccessEfuseRead,
			prAccessEfuseInfo, sizeof(
			struct PARAM_CUSTOM_ACCESS_EFUSE),
			TRUE, TRUE, TRUE, &u4BufLen);
	if (rStatus == WLAN_STATUS_SUCCESS) {
		prAdapter->fgIsHW1T2R =
			((prAdapter->aucEepromVaule[u4Index]
			>> EFUSE_1T2R_OFFSET) & 0x1);
	} else
		prAdapter->fgIsHW1T2R = FALSE;
	DBGLOG(REQ, INFO,
			"%s IsHW1T2R = %d\n",
			__func__, prAdapter->fgIsHW1T2R);

label_exit:
	if (prAccessEfuseInfo != NULL)
		kalMemFree(prAccessEfuseInfo, VIR_MEM_TYPE,
		sizeof(struct PARAM_CUSTOM_ACCESS_EFUSE));
}
void wlanLoadBufferbin1T2R(struct ADAPTER *prAdapter)
{
	prAdapter->fgIsHW1T2R =
			((uacEEPROMImage[EFUSE_1T2R_ADDR]
			>> EFUSE_1T2R_OFFSET) & 0x1);
	DBGLOG(REQ, INFO,
		"%s IsHW1T2R = %d\n",
		__func__, prAdapter->fgIsHW1T2R);
}
#endif

#if CFG_SUPPORT_ADVANCE_CONTROL
/*----------------------------------------------------------------------------*/
/*!
* \brief This function is called to set g_fgKeepFullPwr flag in firmware
*
* \param[IN] prAdapter        Pointer to the Adapter structure.
* \param[IN] fgEnable         Boolean of enable
*
* \return WLAN_STATUS_SUCCESS
* \return WLAN_STATUS_FAILURE
*/
/*----------------------------------------------------------------------------*/
uint32_t wlanKeepFullPwr(struct ADAPTER *prAdapter, uint8_t fgEnable)
{

	struct CMD_KEEP_FULL_PWR rCmdKeepFullPwr;

	ASSERT(prAdapter);

	rCmdKeepFullPwr.ucEnable = fgEnable;
	DBGLOG(HAL, STATE, "KeepFullPwr: %d\n", rCmdKeepFullPwr.ucEnable);

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_KEEP_FULL_PWR,
				   TRUE,
				   FALSE,
				   FALSE, NULL, NULL,
				   sizeof(struct CMD_KEEP_FULL_PWR),
				   (uint8_t *)&rCmdKeepFullPwr, NULL, 0);
}
#endif

#if CFG_SUPPORT_GET_MCS_INFO
void wlanRxMcsInfoMonitor(struct ADAPTER *prAdapter,
					    unsigned long ulParamPtr)
{
	static uint8_t ucSmapleCnt;
	uint8_t ucStaIdx = 0;
	struct STA_RECORD *prStaRec;

	if (prAdapter->prAisBssInfo->prStaRecOfAP == NULL)
		goto out;

	ucStaIdx = prAdapter->prAisBssInfo->prStaRecOfAP->ucIndex;
	prStaRec = cnmGetStaRecByIndex(prAdapter, ucStaIdx);

	if (!prStaRec)
		goto out;

	if (prStaRec->fgIsValid && prStaRec->fgIsInUse) {
		prStaRec->au4RxV0[ucSmapleCnt] = prStaRec->u4RxVector0;
		prStaRec->au4RxV1[ucSmapleCnt] = prStaRec->u4RxVector1;

		ucSmapleCnt = (ucSmapleCnt + 1) % MCS_INFO_SAMPLE_CNT;
	}

out:
	cnmTimerStartTimer(prAdapter, &prAdapter->rRxMcsInfoTimer,
			   MCS_INFO_SAMPLE_PERIOD);
}
#endif
