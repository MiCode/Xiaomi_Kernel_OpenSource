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
#include "mgmt/ais_fsm.h"

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
/* 6.1.1.2 Interpretation of priority parameter in MAC service primitives */
/* Static convert the Priority Parameter/TID(User Priority/TS Identifier) to Traffic Class */
const UINT_8 aucPriorityParam2TC[] = {
	TC1_INDEX,
	TC0_INDEX,
	TC0_INDEX,
	TC1_INDEX,
	TC2_INDEX,
	TC2_INDEX,
	TC3_INDEX,
	TC3_INDEX
};

#define WLAN_WAIT_READY_BIT_TIMEOUT		3000

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
typedef struct _CODE_MAPPING_T {
	UINT_32 u4RegisterValue;
	INT_32 i4TxpowerOffset;
} CODE_MAPPING_T, *P_CODE_MAPPING_T;

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/
BOOLEAN fgIsBusAccessFailed = FALSE;

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/
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
#if (CFG_REFACTORY_PMKSA == 0)
	wlanoidQueryPmkid,
#endif
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
#if 0				/* no use */
/*----------------------------------------------------------------------------*/
/*!
* \brief This is a private routine, which is used to check if HW access is needed
*        for the OID query/ set handlers.
*
* \param[IN] pfnOidHandler Pointer to the OID handler.
* \param[IN] fgSetInfo     It is a Set information handler.
*
* \retval TRUE This function needs HW access
* \retval FALSE This function does not need HW access
*/
/*----------------------------------------------------------------------------*/
BOOLEAN wlanIsHandlerNeedHwAccess(IN PFN_OID_HANDLER_FUNC pfnOidHandler, IN BOOLEAN fgSetInfo)
{
	PFN_OID_HANDLER_FUNC *apfnOidHandlerWOHwAccess;
	UINT_32 i;
	UINT_32 u4NumOfElem;

	if (fgSetInfo) {
		apfnOidHandlerWOHwAccess = apfnOidSetHandlerWOHwAccess;
		u4NumOfElem = sizeof(apfnOidSetHandlerWOHwAccess) / sizeof(PFN_OID_HANDLER_FUNC);
	} else {
		apfnOidHandlerWOHwAccess = apfnOidQueryHandlerWOHwAccess;
		u4NumOfElem = sizeof(apfnOidQueryHandlerWOHwAccess) / sizeof(PFN_OID_HANDLER_FUNC);
	}

	for (i = 0; i < u4NumOfElem; i++) {
		if (apfnOidHandlerWOHwAccess[i] == pfnOidHandler)
			return FALSE;
	}

	return TRUE;
}				/* wlanIsHandlerNeedHwAccess */

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
VOID wlanCardEjected(IN P_ADAPTER_T prAdapter)
{
	DEBUGFUNC("wlanCardEjected");
	/* INITLOG(("\n")); */

	ASSERT(prAdapter);

	/* mark that the card is being ejected, NDIS will shut us down soon */
	nicTxRelease(prAdapter);

}				/* wlanCardEjected */
#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief Create adapter object
*
* \param prAdapter This routine is call to allocate the driver software objects.
*                  If fails, return NULL.
* \retval NULL If it fails, NULL is returned.
* \retval NOT NULL If the adapter was initialized successfully.
*/
/*----------------------------------------------------------------------------*/
P_ADAPTER_T wlanAdapterCreate(IN P_GLUE_INFO_T prGlueInfo)
{
	P_ADAPTER_T prAdpater = (P_ADAPTER_T) NULL;

	DEBUGFUNC("wlanAdapterCreate");

	do {
		prAdpater = (P_ADAPTER_T) kalMemAlloc(sizeof(ADAPTER_T), VIR_MEM_TYPE);

		if (!prAdpater) {
			DBGLOG(INIT, ERROR, "Allocate ADAPTER memory ==> FAILED\n");
			break;
		}

		kalMemZero(prAdpater, sizeof(ADAPTER_T));
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
VOID wlanAdapterDestroy(IN P_ADAPTER_T prAdapter)
{

	if (!prAdapter)
		return;

	kalMemFree(prAdapter, VIR_MEM_TYPE, sizeof(ADAPTER_T));

}

/*----------------------------------------------------------------------------*/
/*!
* \brief Initialize the adapter. The sequence is
*        1. Disable interrupt
*        2. Read adapter configuration from EEPROM and registry, verify chip ID.
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
WLAN_STATUS
wlanAdapterStart(IN P_ADAPTER_T prAdapter,
		 IN P_REG_INFO_T prRegInfo, IN PVOID pvFwImageMapFile, IN UINT_32 u4FwImageFileLength)
{
	WLAN_STATUS u4Status = WLAN_STATUS_SUCCESS;
	UINT_32 i, u4Value = 0;
	UINT_32 u4WHISR = 0;
	UINT_32 u4Time, u4Current;
	UINT_8 aucTxCount[8];
#if CFG_ENABLE_FW_DOWNLOAD
	UINT_32 u4FwLoadAddr, u4ImgSecSize;
	BOOLEAN fgFWDLDumped = FALSE;
#if CFG_ENABLE_FW_DIVIDED_DOWNLOAD
	UINT_32 j;
	P_FIRMWARE_DIVIDED_DOWNLOAD_T prFwHead;
	BOOLEAN fgValidHead;
	const UINT_32 u4CRCOffset = offsetof(FIRMWARE_DIVIDED_DOWNLOAD_T, u4NumOfEntries);
#endif
#endif
	enum Adapter_Start_Fail_Reason {
		ALLOC_ADAPTER_MEM_FAIL,
		DRIVER_OWN_FAIL,
		INIT_ADAPTER_FAIL,
		RAM_CODE_DOWNLOAD_FAIL,
		WAIT_FIRMWARE_READY_FAIL,
		FAIL_REASON_MAX
	} eFailReason;

	ASSERT(prAdapter);

	DEBUGFUNC("wlanAdapterStart");

	eFailReason = FAIL_REASON_MAX;
	/* 4 <0> Reset variables in ADAPTER_T */
	prAdapter->fgIsFwOwn = TRUE;
	prAdapter->fgIsEnterD3ReqIssued = FALSE;

	QUEUE_INITIALIZE(&(prAdapter->rPendingCmdQueue));

	/* Initialize rWlanInfo */
	kalMemSet(&(prAdapter->rWlanInfo), 0, sizeof(WLAN_INFO_T));

	/* 4 <0.1> reset fgIsBusAccessFailed */
	fgIsBusAccessFailed = FALSE;
	prAdapter->ulSuspendFlag = 0;

	do {
		u4Status = nicAllocateAdapterMemory(prAdapter);
		if (u4Status != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, "nicAllocateAdapterMemory Error!\n");
			u4Status = WLAN_STATUS_FAILURE;
			eFailReason = ALLOC_ADAPTER_MEM_FAIL;
#if CFG_ENABLE_KEYWORD_EXCEPTION_MECHANISM
			mtk_wcn_wmt_assert_keyword(WMTDRV_TYPE_WIFI,
				"[Wi-Fi On] nicAllocateAdapterMemory Error!");
#endif
			break;
		}

		prAdapter->u4OsPacketFilter = PARAM_PACKET_FILTER_SUPPORTED;

		DBGLOG(INIT, TRACE, "wlanAdapterStart(): Acquiring LP-OWN %d\n", fgIsResetting);
		ACQUIRE_POWER_CONTROL_FROM_PM(prAdapter);

#if !CFG_ENABLE_FULL_PM
		nicpmSetDriverOwn(prAdapter);
#endif

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
		/* 4 <1> Initialize the Adapter */
		u4Status = nicInitializeAdapter(prAdapter);
		if (u4Status != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, "nicInitializeAdapter failed!\n");
			u4Status = WLAN_STATUS_FAILURE;
			eFailReason = INIT_ADAPTER_FAIL;
			break;
		}

		/* init wake lock before interrupt enable and tx thread */
		KAL_WAKE_LOCK_INIT(prAdapter, prAdapter->rTxThreadWakeLock, "WLAN TX THREAD");

		/* 4 <2> Initialize System Service (MGMT Memory pool and STA_REC) */
		nicInitSystemService(prAdapter);

		/* 4 <3> Initialize Tx */
		nicTxInitialize(prAdapter);
		wlanDefTxPowerCfg(prAdapter);

		/* 4 <4> Initialize Rx */
		nicRxInitialize(prAdapter);

#if CFG_ENABLE_FW_DOWNLOAD

		prAdapter->prGlueInfo->fgIsFwDlDone = FALSE;

		wlanFWDLDebugInit();

		if (pvFwImageMapFile == NULL) {
			DBGLOG(INIT, ERROR, "No Firmware found!\n");
			u4Status = WLAN_STATUS_FAILURE;
			eFailReason = RAM_CODE_DOWNLOAD_FAIL;
			break;
		}

		/* 1. disable interrupt, download is done by polling mode only */
		nicDisableInterrupt(prAdapter);

		/* 2. Initialize Tx Resource to fw download state */
		nicTxInitResetResource(prAdapter);

		/* 3. FW download here */
		u4FwLoadAddr = prRegInfo->u4LoadAddress;

#if CFG_ENABLE_FW_DIVIDED_DOWNLOAD
		/* 3a. parse file header for decision of divided firmware download or not */
		prFwHead = (P_FIRMWARE_DIVIDED_DOWNLOAD_T) pvFwImageMapFile;

		if (prFwHead->u4Signature == MTK_WIFI_SIGNATURE &&
		    prFwHead->u4CRC == wlanCRC32((PUINT_8) pvFwImageMapFile + u4CRCOffset,
						 u4FwImageFileLength - u4CRCOffset)) {
			fgValidHead = TRUE;
		} else {
			fgValidHead = FALSE;
		}

		u4Time = kalGetTimeTick();

		DBGLOG(INIT, INFO, "<wifi> Start to download firmware, time=%u\n",
			u4Time);

		/* 3b. engage divided firmware downloading */
		if (fgValidHead == TRUE) {
			DBGLOG(INIT, TRACE, "wlanAdapterStart(): fgValidHead == TRUE\n");
			wlanDumpMcuChipId(prAdapter);
			for (i = 0; i < prFwHead->u4NumOfEntries; i++) {

#if CFG_START_ADDRESS_IS_1ST_SECTION_ADDR
				if (i == 0) {
					prRegInfo->u4StartAddress = prFwHead->arSection[i].u4DestAddr;
					DBGLOG(INIT, TRACE,
					       "wlanAdapterStart(): FW start address 0x%08x\n",
						prRegInfo->u4StartAddress);
				}
#endif

#if CFG_ENABLE_FW_DOWNLOAD_AGGREGATION
				if (wlanImageSectionDownloadAggregated(prAdapter,
								       prFwHead->arSection[i].u4DestAddr,
								       prFwHead->arSection[i].u4Length,
								       (PUINT_8) pvFwImageMapFile +
								       prFwHead->arSection[i].u4Offset) !=
				    WLAN_STATUS_SUCCESS) {
					DBGLOG(INIT, ERROR, "Firmware scatter download failed!\n");
					u4Status = WLAN_STATUS_FAILURE;
				}
#else
				for (j = 0; j < prFwHead->arSection[i].u4Length; j += CMD_PKT_SIZE_FOR_IMAGE) {
					if (j + CMD_PKT_SIZE_FOR_IMAGE < prFwHead->arSection[i].u4Length)
						u4ImgSecSize = CMD_PKT_SIZE_FOR_IMAGE;
					else
						u4ImgSecSize = prFwHead->arSection[i].u4Length - j;

					wlanFWDLDebugStartSectionPacketInfo(i, j, kalGetTimeTick());

					if (wlanImageSectionDownload(prAdapter,
								     prFwHead->arSection[i].u4DestAddr + j,
								     u4ImgSecSize,
								     (PUINT_8) pvFwImageMapFile +
								     prFwHead->arSection[i].u4Offset + j) !=
					    WLAN_STATUS_SUCCESS) {
						DBGLOG(INIT, ERROR,
						       "Firmware scatter download failed %d!\n", (int)i);
						u4Status = WLAN_STATUS_FAILURE;
						break;
					}

					/* timeout exceeding check, dump FWDL log if timeout (>2.5s) */
					u4Current = kalGetTimeTick();
					if ((u4Current > u4Time) &&
						((u4Current - u4Time) > WLAN_DOWNLOAD_IMAGE_TIMEOUT) &&
						(fgFWDLDumped == FALSE)) {
						DBGLOG(INIT, ERROR, "FW download timeout > 2.5s, FWDL dump info(%u)\n",
							wlanFWDLDebugGetPktCnt());
						wlanFWDLDebugDumpInfo();
						fgFWDLDumped = TRUE;
					}
				}
#endif
				/* escape from loop if any pending error occurs */
				if (u4Status == WLAN_STATUS_FAILURE)
					break;
			}
		} else
#endif
#if CFG_ENABLE_FW_DOWNLOAD_AGGREGATION
		if (wlanImageSectionDownloadAggregated(prAdapter,
							       u4FwLoadAddr,
							       u4FwImageFileLength,
							       (PUINT_8) pvFwImageMapFile) !=
			    WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, "Firmware scatter download failed!\n");
			u4Status = WLAN_STATUS_FAILURE;
		}
#else
			for (i = 0; i < u4FwImageFileLength; i += CMD_PKT_SIZE_FOR_IMAGE) {
				if (i + CMD_PKT_SIZE_FOR_IMAGE < u4FwImageFileLength)
					u4ImgSecSize = CMD_PKT_SIZE_FOR_IMAGE;
				else
					u4ImgSecSize = u4FwImageFileLength - i;

				wlanFWDLDebugStartSectionPacketInfo(0, i, kalGetTimeTick());

				if (wlanImageSectionDownload(prAdapter,
							     u4FwLoadAddr + i,
							     u4ImgSecSize,
							     (PUINT_8) pvFwImageMapFile + i) !=
				    WLAN_STATUS_SUCCESS) {
					DBGLOG(INIT, ERROR, "Firmware scatter download failed!\n");
					u4Status = WLAN_STATUS_FAILURE;
					break;
				}

				/* timeout exceeding check, dump FWDL log if timeout (>2.5s) */
				u4Current = kalGetTimeTick();
				if ((u4Current > u4Time) &&
					((u4Current - u4Time) > WLAN_DOWNLOAD_IMAGE_TIMEOUT) &&
					(fgFWDLDumped == FALSE)) {
					DBGLOG(INIT, ERROR, "FW download timeout > 2.5s, FWDL dump info! Pkt Cnt=%u\n",
						wlanFWDLDebugGetPktCnt());
					wlanFWDLDebugDumpInfo();
					fgFWDLDumped = TRUE;
				}
			}
#endif

		DBGLOG(INIT, INFO, "<wifi> Download FW done, total cnt11=%u spend time=%u\n",
			wlanFWDLDebugGetPktCnt(), kalGetTimeTick() - u4Time);

		wlanFWDLDebugUninit();

		if (u4Status != WLAN_STATUS_SUCCESS) {
			eFailReason = RAM_CODE_DOWNLOAD_FAIL;
			break;
		}
#if !CFG_ENABLE_FW_DOWNLOAD_ACK
		/* Send INIT_CMD_ID_QUERY_PENDING_ERROR command and wait for response */
		if (wlanImageQueryStatus(prAdapter) != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, "Firmware download failed!\n");
			u4Status = WLAN_STATUS_FAILURE;
			break;
		}
#endif

		/* 4. send Wi-Fi Start command */
		DBGLOG(INIT, INFO, "<wifi> send Wi-Fi Start command\n");
#if CFG_OVERRIDE_FW_START_ADDRESS
		wlanConfigWifiFunc(prAdapter, TRUE, prRegInfo->u4StartAddress);
#else
		wlanConfigWifiFunc(prAdapter, FALSE, 0);
#endif
#endif

		DBGLOG(INIT, TRACE, "wlanAdapterStart(): Waiting for Ready bit..\n");
		/* 4 <5> check Wi-Fi FW asserts ready bit */
		i = 0;
		while (1) {
			HAL_MCR_RD(prAdapter, MCR_WCIR, &u4Value);

			if (u4Value & WCIR_WLAN_READY) {
				DBGLOG(INIT, TRACE, "Ready bit asserted\n");
				break;
			} else if (kalIsCardRemoved(prAdapter->prGlueInfo) == TRUE || fgIsBusAccessFailed == TRUE) {
				u4Status = WLAN_STATUS_FAILURE;
				eFailReason = WAIT_FIRMWARE_READY_FAIL;
				break;
			} else if (i >= CFG_RESPONSE_POLLING_TIMEOUT) {
				UINT_32 u4MailBox0;

				nicGetMailbox(prAdapter, 0, &u4MailBox0);
				DBGLOG(INIT, ERROR, "Waiting for Ready bit: Timeout, ID=%u\n",
						     (u4MailBox0 & 0x0000FFFF));
				u4Status = WLAN_STATUS_FAILURE;
				eFailReason = WAIT_FIRMWARE_READY_FAIL;
				GL_RESET_TRIGGER(prAdapter, RST_FLAG_DO_CORE_DUMP | RST_FLAG_PREVENT_POWER_OFF);
				break;
			}
			i++;
			kalMsleep(10);
		}

		prAdapter->prGlueInfo->fgIsFwDlDone = TRUE;

		if (u4Status == WLAN_STATUS_SUCCESS) {
			/* 1. reset interrupt status */
			HAL_READ_INTR_STATUS(prAdapter, 4, (PUINT_8)&u4WHISR);
			if (HAL_IS_TX_DONE_INTR(u4WHISR))
				HAL_READ_TX_RELEASED_COUNT(prAdapter, aucTxCount);

			/* 2. reset TX Resource for normal operation */
			nicTxResetResource(prAdapter);

			/* 3. query for permanent address by polling */
			wlanQueryPermanentAddress(prAdapter);

#if (CFG_SUPPORT_NIC_CAPABILITY == 1)
			/* 4. query for NIC capability */
			wlanQueryNicCapability(prAdapter);
#endif
			/* 4.1 query for compiler flags */
			wlanQueryCompileFlags(prAdapter);

			/* 5. Override network address */
			wlanUpdateNetworkAddress(prAdapter);

			/* 6. indicate disconnection as default status */
			kalIndicateStatusAndComplete(prAdapter->prGlueInfo, WLAN_STATUS_MEDIA_DISCONNECT, NULL, 0);
		}

		RECLAIM_POWER_CONTROL_TO_PM(prAdapter, FALSE);

		if (u4Status != WLAN_STATUS_SUCCESS) {
			eFailReason = WAIT_FIRMWARE_READY_FAIL;
			break;
		}

		/* OID timeout timer initialize */
		cnmTimerInitTimer(prAdapter,
				  &prAdapter->rOidTimeoutTimer,
				  (PFN_MGMT_TIMEOUT_FUNC) wlanReleasePendingOid, (ULONG) NULL);

		/* Return Indicated Rfb list timer */
		cnmTimerInitTimer(prAdapter,
				  &prAdapter->rReturnIndicatedRfbListTimer,
				  (PFN_MGMT_TIMEOUT_FUNC) wlanReturnIndicatedPacketsTimeOut, (ULONG) NULL);

		/* Power state initialization */
		prAdapter->fgWiFiInSleepyState = FALSE;
		prAdapter->rAcpiState = ACPI_STATE_D0;

		/* Online scan option */
		if (prRegInfo->fgDisOnlineScan == 0)
			prAdapter->fgEnOnlineScan = TRUE;
		else
			prAdapter->fgEnOnlineScan = FALSE;

		/* Beacon lost detection option */
		if (prRegInfo->fgDisBcnLostDetection != 0)
			prAdapter->fgDisBcnLostDetection = TRUE;

		/* Load compile time constant */
		prAdapter->rWlanInfo.u2BeaconPeriod = CFG_INIT_ADHOC_BEACON_INTERVAL;
		prAdapter->rWlanInfo.u2AtimWindow = CFG_INIT_ADHOC_ATIM_WINDOW;

#if 1				/* set PM parameters */
		prAdapter->fgEnArpFilter = prRegInfo->fgEnArpFilter;
		prAdapter->u4PsCurrentMeasureEn = prRegInfo->u4PsCurrentMeasureEn;

		prAdapter->u4UapsdAcBmp = prRegInfo->u4UapsdAcBmp;

		prAdapter->u4MaxSpLen = prRegInfo->u4MaxSpLen;

		DBGLOG(INIT, TRACE, "[1] fgEnArpFilter:0x%x, u4UapsdAcBmp:0x%x, u4MaxSpLen:0x%x",
				     prAdapter->fgEnArpFilter, prAdapter->u4UapsdAcBmp, prAdapter->u4MaxSpLen);

		prAdapter->fgEnCtiaPowerMode = FALSE;

#if CFG_SUPPORT_DBG_POWERMODE
		prAdapter->fgEnDbgPowerMode = FALSE;
#endif

#endif

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
		prAdapter->rNchoInfo.rRoamScnChnl.arChnlInfoList[0].eBand = BAND_2G4;
		prAdapter->rNchoInfo.rRoamScnChnl.arChnlInfoList[0].ucChannelNum = 1;
		prAdapter->rNchoInfo.eDFSScnMode = NCHO_DFS_SCN_ENABLE1;
		prAdapter->rNchoInfo.i4RoamTrigger = -70;
		prAdapter->rNchoInfo.i4RoamDelta = 5;
		prAdapter->rNchoInfo.u4RoamScanPeriod = ROAMING_DISCOVERY_TIMEOUT_SEC;
		prAdapter->rNchoInfo.u4ScanChannelTime = 50;
		prAdapter->rNchoInfo.u4ScanHomeTime = 120;
		prAdapter->rNchoInfo.u4ScanHomeawayTime = 120;
		prAdapter->rNchoInfo.u4ScanNProbes = 2;
		prAdapter->rNchoInfo.u4WesMode = 0;
#endif

		/* Enable WZC Disassociation */
		prAdapter->rWifiVar.fgSupportWZCDisassociation = TRUE;

		/* Apply Rate Setting */
		if ((ENUM_REGISTRY_FIXED_RATE_T) (prRegInfo->u4FixedRate) < FIXED_RATE_NUM)
			prAdapter->rWifiVar.eRateSetting = (ENUM_REGISTRY_FIXED_RATE_T) (prRegInfo->u4FixedRate);
		else
			prAdapter->rWifiVar.eRateSetting = FIXED_RATE_NONE;

		if (prAdapter->rWifiVar.eRateSetting == FIXED_RATE_NONE) {
			/* Enable Auto (Long/Short) Preamble */
			prAdapter->rWifiVar.ePreambleType = PREAMBLE_TYPE_AUTO;
		} else if ((prAdapter->rWifiVar.eRateSetting >= FIXED_RATE_MCS0_20M_400NS &&
			    prAdapter->rWifiVar.eRateSetting <= FIXED_RATE_MCS7_20M_400NS)
			   || (prAdapter->rWifiVar.eRateSetting >= FIXED_RATE_MCS0_40M_400NS &&
			       prAdapter->rWifiVar.eRateSetting <= FIXED_RATE_MCS32_400NS)) {
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

#if CFG_RX_BA_REORDERING_ENHANCEMENT
		/* Enable drop independent packets with Rx Ba reordering */
		prAdapter->rWifiVar.fgEnableReportIndependentPkt = TRUE;
#endif

		/* configure available PHY type set */
		nicSetAvailablePhyTypeSet(prAdapter);

#ifdef CFG_TC1_FEATURE /* for Passive Scan */
		prAdapter->ucScanType = SCAN_TYPE_ACTIVE_SCAN;
#endif

#if 1				/* set PM parameters */
		{
#if  CFG_SUPPORT_PWR_MGT
			prAdapter->u4PowerMode = prRegInfo->u4PowerMode;
			prAdapter->rWlanInfo.arPowerSaveMode[NETWORK_TYPE_P2P_INDEX].ucNetTypeIndex =
			    NETWORK_TYPE_P2P_INDEX;
			prAdapter->rWlanInfo.arPowerSaveMode[NETWORK_TYPE_P2P_INDEX].ucPsProfile = ENUM_PSP_FAST_SWITCH;
#else
			prAdapter->u4PowerMode = ENUM_PSP_CONTINUOUS_ACTIVE;
#endif

			nicConfigPowerSaveProfile(prAdapter, NETWORK_TYPE_AIS_INDEX,	/* FIXIT */
						  prAdapter->u4PowerMode, FALSE);
		}

#endif

#if CFG_SUPPORT_NVRAM
		/* load manufacture data */
		wlanLoadManufactureData(prAdapter, prRegInfo);
#endif

#if CFG_TC1_FEATURE	/* 1 //keep alive packet time change from default 30secs to 20secs. //TC01// */
		{
			CMD_SW_DBG_CTRL_T rCmdSwCtrl;

			rCmdSwCtrl.u4Id = 0x90100000;
			rCmdSwCtrl.u4Data = 30;
			DBGLOG(INIT, TRACE, "wlanAdapterStart Keepaliveapcket 0x%x, %d\n",
				rCmdSwCtrl.u4Id, rCmdSwCtrl.u4Data);
			wlanSendSetQueryCmd(prAdapter,
					    CMD_ID_SW_DBG_CTRL,
					    TRUE,
					    FALSE,
					    FALSE,
					    NULL, NULL, sizeof(CMD_SW_DBG_CTRL_T), (PUINT_8) (&rCmdSwCtrl), NULL, 0);
		}
#endif
#if (CFG_SRAM_SIZE_OPTION == 1 || CFG_SRAM_SIZE_OPTION == 0)
		/* ALPS02494017 for DIR-635/DIR-655 IOT issue (BA size must be power of 2) */
		nicQmSetRxBASize(prAdapter, true, IOT_RX_BA_SIZE);
#endif

#if 0
		/* Update Auto rate parameters in FW */
		nicRlmArUpdateParms(prAdapter,
				    prRegInfo->u4ArSysParam0,
				    prRegInfo->u4ArSysParam1, prRegInfo->u4ArSysParam2, prRegInfo->u4ArSysParam3);
#endif

#if (MT6620_E1_ASIC_HIFSYS_WORKAROUND == 1)
		/* clock gating workaround */
		prAdapter->fgIsClockGatingEnabled = FALSE;
#endif

		prAdapter->u4QmRxBaMissTimeout = DEFAULT_QM_RX_BA_ENTRY_MISS_TIMEOUT_MS;
		prAdapter->fgEnCfg80211Scan = TRUE;

#if CFG_SUPPORT_GAMING_MODE
		prAdapter->fgEnGamingMode = FALSE;
#endif
#if CFG_SUPPORT_OSHARE
		prAdapter->fgEnOshareMode = FALSE;
#endif
	} while (FALSE);

	if (u4Status == WLAN_STATUS_SUCCESS) {
		/* restore to hardware default */
		HAL_SET_INTR_STATUS_READ_CLEAR(prAdapter);
		HAL_SET_MAILBOX_READ_CLEAR(prAdapter, FALSE);

		/* Enable interrupt */
		nicEnableInterrupt(prAdapter);

	} else {
		/* release allocated memory */
		switch (eFailReason) {
		case WAIT_FIRMWARE_READY_FAIL:
			DBGLOG(INIT, ERROR, "Wait firmware ready fail, FailReason: %d\n",
					eFailReason);
			kalSendAeeWarning("[Wait firmware ready fail!]", __func__);
			KAL_WAKE_LOCK_DESTROY(prAdapter, prAdapter->rTxThreadWakeLock);
			nicRxUninitialize(prAdapter);
			nicTxRelease(prAdapter);
			/* System Service Uninitialization */
			nicUninitSystemService(prAdapter);
			nicReleaseAdapterMemory(prAdapter);
			break;
		case RAM_CODE_DOWNLOAD_FAIL:
			DBGLOG(INIT, ERROR, "Ram code download fail, FailReason: %d\n",
					eFailReason);
			kalSendAeeWarning("[Ram code download fail!]", __func__);
			KAL_WAKE_LOCK_DESTROY(prAdapter, prAdapter->rTxThreadWakeLock);
			nicRxUninitialize(prAdapter);
			nicTxRelease(prAdapter);
			/* System Service Uninitialization */
			nicUninitSystemService(prAdapter);
			nicReleaseAdapterMemory(prAdapter);
			break;
		case INIT_ADAPTER_FAIL:
			nicReleaseAdapterMemory(prAdapter);
			break;
		case DRIVER_OWN_FAIL:
			nicReleaseAdapterMemory(prAdapter);
			break;
		case ALLOC_ADAPTER_MEM_FAIL:
			break;
		default:
			break;
		}
	}

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
WLAN_STATUS wlanAdapterStop(IN P_ADAPTER_T prAdapter)
{
	UINT_32 i, u4Value = 0;
	UINT_32 u4CurrTick;
	WLAN_STATUS u4Status = WLAN_STATUS_SUCCESS;

	ASSERT(prAdapter);

#if (MT6620_E1_ASIC_HIFSYS_WORKAROUND == 1)
	if (prAdapter->fgIsClockGatingEnabled == TRUE)
		nicDisableClockGating(prAdapter);
#endif

	if (prAdapter->rAcpiState == ACPI_STATE_D0 &&
#if (CFG_CHIP_RESET_SUPPORT == 1)
	    kalIsResetting() == FALSE &&
#endif
	    kalIsCardRemoved(prAdapter->prGlueInfo) == FALSE) {

		/* 0. Disable interrupt, this can be done without Driver own */
		nicDisableInterrupt(prAdapter);

		ACQUIRE_POWER_CONTROL_FROM_PM(prAdapter);

		/* 1. Set CMD to FW to tell WIFI to stop (enter power off state) */
		/* the command must be issue to firmware even in wlanRemove() */
		if (prAdapter->fgIsFwOwn == FALSE && wlanSendNicPowerCtrlCmd(prAdapter, 1) == WLAN_STATUS_SUCCESS) {
			/* 2. Clear pending interrupt */
			i = 0;
			while (i < CFG_IST_LOOP_COUNT && nicProcessIST(prAdapter) != WLAN_STATUS_NOT_INDICATING) {
				i++;
			};

			/* 3. Wait til RDY bit has been cleaerd */
			u4CurrTick = kalGetTimeTick();
			while (1) {
				HAL_MCR_RD(prAdapter, MCR_WCIR, &u4Value);

				if ((u4Value & WCIR_WLAN_READY) == 0)
					break;
				else if (kalIsCardRemoved(prAdapter->prGlueInfo) == TRUE
					 || fgIsBusAccessFailed == TRUE ||
					CHECK_FOR_TIMEOUT(kalGetTimeTick(), u4CurrTick, WLAN_WAIT_READY_BIT_TIMEOUT)) {
					wlanDumpCommandFwStatus();
					wlanDumpTcResAndTxedCmd(NULL, 0);
					cmdBufDumpCmdQueue(&prAdapter->rPendingCmdQueue, "waiting response CMD queue");
					glDumpConnSysCpuInfo(prAdapter->prGlueInfo);
					/* dump TC4[0] ~ TC4[3] TX_DESC */
					wlanDebugHifDescriptorDump(prAdapter, MTK_AMPDU_TX_DESC, DEBUG_TC4_INDEX);
#if CFG_ENABLE_KEYWORD_EXCEPTION_MECHANISM
					mtk_wcn_wmt_assert_keyword(WMTDRV_TYPE_WIFI,
						"[Wi-Fi On] [Read WCIR_WLAN_READY fail!]");
#else
					kalSendAeeWarning("[Read WCIR_WLAN_READY fail!]", __func__);
#endif
					break;
				}
			}
		}

		/* 4. Set Onwership to F/W */
		nicpmSetFWOwn(prAdapter, FALSE);

#if CFG_FORCE_RESET_UNDER_BUS_ERROR
		if (HAL_TEST_FLAG(prAdapter, ADAPTER_FLAG_HW_ERR) == TRUE) {
			/* force acquire firmware own */
			kalDevRegWrite(prAdapter->prGlueInfo, MCR_WHLPCR, WHLPCR_FW_OWN_REQ_CLR);

			/* delay for 10ms */
			kalMdelay(10);

			/* force firmware reset via software interrupt */
			kalDevRegWrite(prAdapter->prGlueInfo, MCR_WSICR, WSICR_H2D_SW_INT_SET);

			/* force release firmware own */
			kalDevRegWrite(prAdapter->prGlueInfo, MCR_WHLPCR, WHLPCR_FW_OWN_REQ_SET);
		}
#endif

		RECLAIM_POWER_CONTROL_TO_PM(prAdapter, FALSE);
	}

	nicRxUninitialize(prAdapter);

	nicTxRelease(prAdapter);

	/* MGMT - unitialization */
	nicUninitMGMT(prAdapter);

	/* System Service Uninitialization */
	nicUninitSystemService(prAdapter);

	nicReleaseAdapterMemory(prAdapter);

#if defined(_HIF_SPI)
	/* Note: restore the SPI Mode Select from 32 bit to default */
	nicRestoreSpiDefMode(prAdapter);
#endif

	return u4Status;
}				/* wlanAdapterStop */

#if 0
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
BOOLEAN wlanISR(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgGlobalIntrCtrl)
{
	ASSERT(prAdapter);

	if (fgGlobalIntrCtrl) {
		nicDisableInterrupt(prAdapter);

		/* wlanIST(prAdapter); */
	}

	return TRUE;
}
#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is called by IST (task_let).
*
* \param prAdapter      Pointer of Adapter Data Structure
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID wlanIST(IN P_ADAPTER_T prAdapter)
{
	ASSERT(prAdapter);

	/* wake up CONNSYS */
	ACQUIRE_POWER_CONTROL_FROM_PM(prAdapter);

	/* handle interrupts */
	nicProcessIST(prAdapter);

	/* re-enable HIF interrupts */
	nicEnableInterrupt(prAdapter);

	/* CONNSYS can decide to sleep */
	RECLAIM_POWER_CONTROL_TO_PM(prAdapter, FALSE);

}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function will check command queue to find out if any could be dequeued
*        and/or send to HIF to MT6620
*
* \param prAdapter      Pointer of Adapter Data Structure
* \param prCmdQue       Pointer of Command Queue (in Glue Layer)
*
* \retval WLAN_STATUS_SUCCESS
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanProcessCommandQueue(IN P_ADAPTER_T prAdapter, IN P_QUE_T prCmdQue)
{
	WLAN_STATUS rStatus;
	QUE_T rTempCmdQue, rMergeCmdQue, rStandInCmdQue;
	P_QUE_T prTempCmdQue, prMergeCmdQue, prStandInCmdQue;
	P_QUE_ENTRY_T prQueueEntry;
	P_CMD_INFO_T prCmdInfo;
	P_MSDU_INFO_T prMsduInfo;
	ENUM_FRAME_ACTION_T eFrameAction = FRAME_ACTION_DROP_PKT;

	KAL_SPIN_LOCK_DECLARATION();

	/* sanity check */
	ASSERT(prAdapter);
	ASSERT(prCmdQue);

	/* init */
	prTempCmdQue = &rTempCmdQue;
	prMergeCmdQue = &rMergeCmdQue;
	prStandInCmdQue = &rStandInCmdQue;

	QUEUE_INITIALIZE(prTempCmdQue);
	QUEUE_INITIALIZE(prMergeCmdQue);
	QUEUE_INITIALIZE(prStandInCmdQue);

	/* 4 <1> Move whole list of CMD_INFO to the temp queue */
	/* copy all commands to prTempCmdQue and empty prCmdQue */
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_QUE);
	QUEUE_MOVE_ALL(prTempCmdQue, prCmdQue);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_QUE);

	/* 4 <2> Dequeue from head and check it is able to be sent */
	/* remove the first one */
	QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);

	while (prQueueEntry) {
		prCmdInfo = (P_CMD_INFO_T) prQueueEntry;

		/* check how to handle the command: drop, queue, or tx */
		switch (prCmdInfo->eCmdType) {
		case COMMAND_TYPE_KEY_IOCTL:
		{
			P_WIFI_CMD_T prWifiCmd = (P_WIFI_CMD_T) NULL;
			P_CMD_802_11_KEY prKey = (P_CMD_802_11_KEY) NULL;
			P_BSS_INFO_T prBssInfo = (P_BSS_INFO_T) NULL;

			eFrameAction = FRAME_ACTION_TX_PKT;
			do {
				prWifiCmd = (P_WIFI_CMD_T) (prCmdInfo->pucInfoBuffer);
				prKey = (P_CMD_802_11_KEY) (prWifiCmd->aucBuffer);
				prBssInfo = &prAdapter->rWifiVar.arBssInfo[prKey->ucNetType];

				/* TODO: only handle p2p case */
				if (prKey->ucNetType != NETWORK_TYPE_P2P_INDEX ||
					prBssInfo->eCurrentOPMode != OP_MODE_INFRASTRUCTURE ||
					prBssInfo->eConnectionState != PARAM_MEDIA_STATE_CONNECTED)
					break;
				if (!prKey->ucTxKey ||
					(prKey->ucAlgorithmId != CIPHER_SUITE_TKIP &&
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
					eFrameAction = FRAME_ACTION_TX_PKT;
					break;
				default:
					break;
				}
				DBGLOG(TX, INFO, "Add Key eKeyAction: %d\n", eFrameAction);
			} while (FALSE);
			break;
		}
		case COMMAND_TYPE_GENERAL_IOCTL:
		case COMMAND_TYPE_NETWORK_IOCTL:
			/* command packet will be always sent */
			eFrameAction = FRAME_ACTION_TX_PKT;
			break;

		case COMMAND_TYPE_SECURITY_FRAME:
			/* inquire with QM */
			eFrameAction = qmGetFrameAction(prAdapter,
							prCmdInfo->eNetworkType,
							prCmdInfo->ucStaRecIndex, NULL, FRAME_TYPE_802_1X);
			break;

		case COMMAND_TYPE_MANAGEMENT_FRAME:
			/* inquire with QM */
			prMsduInfo = (P_MSDU_INFO_T) (prCmdInfo->prPacket);

			eFrameAction = qmGetFrameAction(prAdapter,
							prMsduInfo->ucNetworkType,
							prMsduInfo->ucStaRecIndex, prMsduInfo, FRAME_TYPE_MMPDU);
			break;

		default:
			ASSERT(0);
			break;
		}

		/* 4 <3> handling upon dequeue result */
		if (eFrameAction == FRAME_ACTION_DROP_PKT) {
			if (prCmdInfo->eCmdType == COMMAND_TYPE_SECURITY_FRAME)
				DBGLOG(TX, WARN, "Drop Security frame seqNo=%d\n",
					prCmdInfo->ucCmdSeqNum);
			wlanReleaseCommand(prAdapter, prCmdInfo);
		} else if (eFrameAction == FRAME_ACTION_QUEUE_PKT) {
			if (prCmdInfo->eCmdType == COMMAND_TYPE_SECURITY_FRAME)
				DBGLOG(TX, INFO, "Queue Security frame seqNo=%d\n",
					prCmdInfo->ucCmdSeqNum);
			QUEUE_INSERT_TAIL(prMergeCmdQue, prQueueEntry);
		} else if (eFrameAction == FRAME_ACTION_TX_PKT) {
			/* 4 <4> Send the command */
			rStatus = wlanSendCommand(prAdapter, prCmdInfo);

			if (rStatus == WLAN_STATUS_RESOURCES) {
				QUEUE_INSERT_TAIL(prMergeCmdQue, prQueueEntry);
				DBGLOGLIMITED(TX, INFO,
					"No TC4 resource to send cmd, CID=0x%x, SEQ=%d, CMD type=%d, OID=%d\n",
					prCmdInfo->ucCID, prCmdInfo->ucCmdSeqNum,
					prCmdInfo->eCmdType, prCmdInfo->fgIsOid);

				/*
				 * We reserve one TC4 resource for CMD specially, only break
				 * checking the left tx request if no resource for true CMD.
				 */
				if ((prCmdInfo->eCmdType != COMMAND_TYPE_SECURITY_FRAME) &&
				    (prCmdInfo->eCmdType != COMMAND_TYPE_MANAGEMENT_FRAME))
					break;
			} else if (rStatus == WLAN_STATUS_PENDING) {
				/* command packet which needs further handling upon response */
				/* i.e. we need to wait for FW's response */
				KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING);
				QUEUE_INSERT_TAIL(&(prAdapter->rPendingCmdQueue), prQueueEntry);
				KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING);
			} else {
				/* send success or fail */
				P_CMD_INFO_T prCmdInfo = (P_CMD_INFO_T) prQueueEntry;

				if (rStatus == WLAN_STATUS_SUCCESS) {
					/* send success */
					if (prCmdInfo->pfCmdDoneHandler) {
						prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo,
									    prCmdInfo->pucInfoBuffer);
					}
#if CFG_SUPPORT_FCC_POWER_BACK_OFF
					else
						nicCmdEventSetCommon(prAdapter, prCmdInfo,
							      prCmdInfo->pucInfoBuffer);
#endif
				} else {
					/* send fail */
					if (prCmdInfo->fgIsOid) {
						kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery,
							       prCmdInfo->u4SetInfoLen, rStatus);
					}
					DBGLOG(TX, WARN, "Send CMD, status=%u, CID=%d, SEQ=%d, CMD type=%d, OID=%d\n",
							rStatus, prCmdInfo->ucCID, prCmdInfo->ucCmdSeqNum,
							prCmdInfo->eCmdType, prCmdInfo->fgIsOid);
				}

				/* free the command memory */
				cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
			}
		} else {

			/* impossible, wrong eFrameAction */
			ASSERT(0);
		}

		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
	}

	/* 4 <3> Merge back to original queue */
	/* 4 <3.1> Merge prMergeCmdQue & prTempCmdQue */
	QUEUE_CONCATENATE_QUEUES(prMergeCmdQue, prTempCmdQue);

	/* 4 <3.2> Move prCmdQue to prStandInQue, due to prCmdQue might differ due to incoming 802.1X frames */
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_QUE);

	/* ??? here, prCmdQue shall be empty, why QUEUE_MOVE_ALL ??? */
	QUEUE_MOVE_ALL(prStandInCmdQue, prCmdQue);

	/* 4 <3.3> concatenate prStandInQue to prMergeCmdQue */
	QUEUE_CONCATENATE_QUEUES(prMergeCmdQue, prStandInCmdQue);

	/* 4 <3.4> then move prMergeCmdQue to prCmdQue */
	QUEUE_MOVE_ALL(prCmdQue, prMergeCmdQue);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_QUE);

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
* \retval WLAN_STATUS_SUCCESS   : CMD was written to HIF and be freed(CMD Done) immediately.
* \retval WLAN_STATUS_RESOURCE  : No resource for current command, need to wait for previous
*                                 frame finishing their transmission.
* \retval WLAN_STATUS_FAILURE   : Get failure while access HIF or been rejected.
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanSendCommand(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo)
{
	P_TX_CTRL_T prTxCtrl;
	UINT_8 ucTC;		/* "Traffic Class" SW(Driver) resource classification */
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	BOOLEAN pfgIsSecOrMgmt = FALSE;

	/* sanity check */
	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	/* init */
	prTxCtrl = &prAdapter->rTxCtrl;

	/* DbgPrint("wlanSendCommand()\n"); */
	/*  */
	/*  */
#if DBG && 0
	LOG_FUNC("wlanSendCommand()\n");
	LOG_FUNC("CmdType %u NetworkType %u StaRecIndex %u Oid %u CID 0x%x SetQuery %u NeedResp %u CmdSeqNum %u\n",
		 prCmdInfo->eCmdType,
		 prCmdInfo->eNetworkType,
		 prCmdInfo->ucStaRecIndex,
		 prCmdInfo->fgIsOid,
		 prCmdInfo->ucCID, prCmdInfo->fgSetQuery, prCmdInfo->fgNeedResp, prCmdInfo->ucCmdSeqNum);
#endif

#if (MT6620_E1_ASIC_HIFSYS_WORKAROUND == 1)
	if (prAdapter->fgIsClockGatingEnabled == TRUE)
		nicDisableClockGating(prAdapter);
#endif

	do {
		/* <0> card removal check */
		if (kalIsCardRemoved(prAdapter->prGlueInfo) == TRUE || fgIsBusAccessFailed == TRUE) {
			rStatus = WLAN_STATUS_FAILURE;
			break;
		}
		/* <1> Normal case of sending CMD Packet */
		if (!prCmdInfo->fgDriverDomainMCR) {
			/* <1.1> Assign Traffic Class(TC) = TC4. */
			ucTC = TC4_INDEX;

			if ((prCmdInfo->eCmdType == COMMAND_TYPE_SECURITY_FRAME) ||
				(prCmdInfo->eCmdType == COMMAND_TYPE_MANAGEMENT_FRAME))
				pfgIsSecOrMgmt = TRUE;

			wlanReadFwStatus(prAdapter);
			/* <1.2> Check if pending packet or resource was exhausted */
			rStatus = nicTxAcquireResource(prAdapter, ucTC, pfgIsSecOrMgmt);
			if (rStatus == WLAN_STATUS_RESOURCES) {
				DbgPrint("NO Resource:%d\n", ucTC);
				break;
			}
			/* <1.3> Forward CMD_INFO_T to NIC Layer */
			rStatus = nicTxCmd(prAdapter, prCmdInfo, ucTC);

			/* <1.4> Set Pending in response to Query Command/Need Response */
			if (rStatus == WLAN_STATUS_SUCCESS) {
				if ((!prCmdInfo->fgSetQuery) || (prCmdInfo->fgNeedResp))
					rStatus = WLAN_STATUS_PENDING;
			}
		}
		/* <2> "Special case" for access Driver Domain MCR */
		else {

			P_CMD_ACCESS_REG prCmdAccessReg;

			prCmdAccessReg = (P_CMD_ACCESS_REG) (prCmdInfo->pucInfoBuffer + CMD_HDR_SIZE);

			if (prCmdInfo->fgSetQuery) {
				/* address is in DWORD unit */
				HAL_MCR_WR(prAdapter, (prCmdAccessReg->u4Address & BITS(2, 31)),
					   prCmdAccessReg->u4Data);
			} else {
				P_CMD_ACCESS_REG prEventAccessReg;
				UINT_32 u4Address;

				u4Address = prCmdAccessReg->u4Address;
				prEventAccessReg = (P_CMD_ACCESS_REG) prCmdInfo->pucInfoBuffer;
				prEventAccessReg->u4Address = u4Address;
				/* address is in DWORD unit */
				HAL_MCR_RD(prAdapter, prEventAccessReg->u4Address & BITS(2, 31),
					   &prEventAccessReg->u4Data);
			}
		}

	} while (FALSE);

#if (MT6620_E1_ASIC_HIFSYS_WORKAROUND == 1)
	if (prAdapter->fgIsClockGatingEnabled == FALSE)
		nicEnableClockGating(prAdapter);
#endif

	return rStatus;
}				/* end of wlanSendCommand() */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function will release thd CMD_INFO upon its attribution
 *
 * \param prAdapter  Pointer of Adapter Data Structure
 * \param prCmdInfo  Pointer of CMD_INFO_T
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
VOID wlanReleaseCommand(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo)
{
	P_TX_CTRL_T prTxCtrl;
	P_MSDU_INFO_T prMsduInfo;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	prTxCtrl = &prAdapter->rTxCtrl;

	switch (prCmdInfo->eCmdType) {
	case COMMAND_TYPE_GENERAL_IOCTL:
	case COMMAND_TYPE_NETWORK_IOCTL:
		if (prCmdInfo->fgIsOid) {
			/* for OID command, we need to do complete() to wake up kalIoctl() */
			kalOidComplete(prAdapter->prGlueInfo,
				       prCmdInfo->fgSetQuery, prCmdInfo->u4SetInfoLen, WLAN_STATUS_FAILURE);
		}
		break;

	case COMMAND_TYPE_SECURITY_FRAME:
		/* free packets in kalSecurityFrameSendComplete() */
		kalSecurityFrameSendComplete(prAdapter->prGlueInfo, prCmdInfo->prPacket, WLAN_STATUS_FAILURE);
		break;

	case COMMAND_TYPE_MANAGEMENT_FRAME:
		prMsduInfo = (P_MSDU_INFO_T) prCmdInfo->prPacket;

		/* invoke callbacks */
		if (prMsduInfo->pfTxDoneHandler != NULL)
			prMsduInfo->pfTxDoneHandler(prAdapter, prMsduInfo, TX_RESULT_DROPPED_IN_DRIVER);

		GLUE_DEC_REF_CNT(prTxCtrl->i4TxMgmtPendingNum);
		cnmMgtPktFree(prAdapter, prMsduInfo);
		break;

	default:
		/* impossible, shall not be here */
		ASSERT(0);
		break;
	}

	/* free command buffer and return the command header to command pool */
	cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

}				/* end of wlanReleaseCommand() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This function will search the CMD Queue to look for the pending OID and
*        compelete it immediately when system request a reset.
*
* \param prAdapter  ointer of Adapter Data Structure
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID wlanReleasePendingOid(IN P_ADAPTER_T prAdapter, IN ULONG ulData)
{
	P_QUE_T prCmdQue;
	QUE_T rTempCmdQue;
	P_QUE_T prTempCmdQue = &rTempCmdQue;
	P_QUE_ENTRY_T prQueueEntry = (P_QUE_ENTRY_T) NULL;
	P_CMD_INFO_T prCmdInfo = (P_CMD_INFO_T) NULL;

	KAL_SPIN_LOCK_DECLARATION();

	DEBUGFUNC("wlanReleasePendingOid");

	ASSERT(prAdapter);

	DBGLOG(OID, ERROR, "OID Timeout! Releasing pending OIDs ..\n");

	do {
		/* 1: Handle OID commands in pending queue */
		/* Clear Pending OID in prAdapter->rPendingCmdQueue */
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING);

		/* move all pending commands to prTempCmdQue and empty prCmdQue */
		prCmdQue = &prAdapter->rPendingCmdQueue;
		QUEUE_MOVE_ALL(prTempCmdQue, prCmdQue);

		/* get first pending command */
		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);

		while (prQueueEntry) {
			prCmdInfo = (P_CMD_INFO_T) prQueueEntry;

			if (prCmdInfo->fgIsOid) {
				if (prCmdInfo->pfCmdTimeoutHandler) {
					prCmdInfo->pfCmdTimeoutHandler(prAdapter, prCmdInfo);
				} else {
					/* send complete() to wake up kalIoctl() */
					kalOidComplete(prAdapter->prGlueInfo,
						       prCmdInfo->fgSetQuery, 0, WLAN_STATUS_FAILURE);
				}

				/* free command memory */
				cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
			} else {
				/* nothing to do so re-queue it to prCmdQue */
				QUEUE_INSERT_TAIL(prCmdQue, prQueueEntry);
			}

			QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
		}

		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING);

		/* 2: Clear pending OID staying in command queue */
		kalOidCmdClearance(prAdapter->prGlueInfo);

		/* 3: Do complete(), do we need this? because we have completed in kalOidComplete */
		kalOidClearance(prAdapter->prGlueInfo);

	} while (FALSE);

}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function will search the CMD Queue to look for the pending CMD/OID for specific
*        NETWORK TYPE and compelete it immediately when system request a reset.
*
* \param prAdapter  ointer of Adapter Data Structure
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID wlanReleasePendingCMDbyNetwork(IN P_ADAPTER_T prAdapter, IN ENUM_NETWORK_TYPE_INDEX_T eNetworkType)
{
	P_QUE_T prCmdQue;
	QUE_T rTempCmdQue;
	P_QUE_T prTempCmdQue = &rTempCmdQue;
	P_QUE_ENTRY_T prQueueEntry = (P_QUE_ENTRY_T) NULL;
	P_CMD_INFO_T prCmdInfo = (P_CMD_INFO_T) NULL;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);

	/* only free commands from the network interface, AIS, P2P, or BOW */

	do {
		/* 1: Clear Pending OID in prAdapter->rPendingCmdQueue */
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING);

		prCmdQue = &prAdapter->rPendingCmdQueue;
		QUEUE_MOVE_ALL(prTempCmdQue, prCmdQue);

		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
		while (prQueueEntry) {
			prCmdInfo = (P_CMD_INFO_T) prQueueEntry;

			DBGLOG(P2P, TRACE, "Pending CMD for Network Type:%d\n", prCmdInfo->eNetworkType);

			if (prCmdInfo->eNetworkType == eNetworkType) {
				if (prCmdInfo->pfCmdTimeoutHandler) {
					prCmdInfo->pfCmdTimeoutHandler(prAdapter, prCmdInfo);
				} else
					kalOidComplete(prAdapter->prGlueInfo,
						       prCmdInfo->fgSetQuery, 0, WLAN_STATUS_FAILURE);

				cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
			} else {
				QUEUE_INSERT_TAIL(prCmdQue, prQueueEntry);
			}

			QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
		}

		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING);

	} while (FALSE);

}				/* wlanReleasePendingCMDbyNetwork */

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
VOID wlanReturnPacket(IN P_ADAPTER_T prAdapter, IN PVOID pvPacket)
{
	P_RX_CTRL_T prRxCtrl;
	P_SW_RFB_T prSwRfb = NULL;
	BOOLEAN fgIsUninitRfb = FALSE;

	KAL_SPIN_LOCK_DECLARATION();

	DEBUGFUNC("wlanReturnPacket");

	ASSERT(prAdapter);

	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);

	/* free the packet */
	if (pvPacket) {
		kalPacketFree(prAdapter->prGlueInfo, pvPacket);
		RX_ADD_CNT(prRxCtrl, RX_DATA_RETURNED_COUNT, 1);
#if CFG_NATIVE_802_11
		if (GLUE_TEST_FLAG(prAdapter->prGlueInfo, GLUE_FLAG_HALT)) {
			/*Todo:: nothing*/
			/*Todo:: nothing*/
		}
#endif
	}

	/* free the packet control block */
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
	QUEUE_REMOVE_HEAD(&prRxCtrl->rIndicatedRfbList, prSwRfb, P_SW_RFB_T);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
	if (!prSwRfb) {
		ASSERT(0);
		return;
	}

	if (nicRxSetupRFB(prAdapter, prSwRfb)) {
		ASSERT(0);
		/* return; // Don't return here or it would lost SwRfb --kc */
		if (!timerPendingTimer(&prAdapter->rReturnIndicatedRfbListTimer)) {
			DBGLOG(RX, WARN,
			       "wlanReturnPacket, Start ReturnIndicatedRfbList Timer (%ds)\n",
				RX_RETURN_INDICATED_RFB_TIMEOUT_SEC);
			cnmTimerStartTimer(prAdapter, &prAdapter->rReturnIndicatedRfbListTimer,
					   SEC_TO_MSEC(RX_RETURN_INDICATED_RFB_TIMEOUT_SEC));
		}
		fgIsUninitRfb = TRUE;
	}
	nicRxReturnRFBwithUninit(prAdapter, prSwRfb, fgIsUninitRfb);
}

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
VOID wlanReturnIndicatedPacketsTimeOut(IN P_ADAPTER_T prAdapter, IN ULONG ulData)
{
	P_RX_CTRL_T prRxCtrl;
	P_SW_RFB_T prSwRfb = NULL;
	BOOLEAN fgIsUninitRfb = FALSE;

	KAL_SPIN_LOCK_DECLARATION();
	WLAN_STATUS status = WLAN_STATUS_SUCCESS;
	P_QUE_T prQueList;

	DEBUGFUNC("wlanReturnIndicatedPacketsTimeOut");
	DBGLOG(RX, WARN, "wlanReturnIndicatedPacketsTimeOut");

	ASSERT(prAdapter);

	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);

	prQueList = &prRxCtrl->rIndicatedRfbList;
	DBGLOG(RX, WARN, "IndicatedRfbList num = %u\n", (unsigned int)prQueList->u4NumElem);

	while (QUEUE_IS_NOT_EMPTY(&prRxCtrl->rIndicatedRfbList)) {
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
		QUEUE_REMOVE_HEAD(&prRxCtrl->rIndicatedRfbList, prSwRfb, P_SW_RFB_T);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);

		if (nicRxSetupRFB(prAdapter, prSwRfb)) {
			status = WLAN_STATUS_RESOURCES;
			ASSERT(0);
			fgIsUninitRfb = TRUE;
		}
		nicRxReturnRFBwithUninit(prAdapter, prSwRfb, fgIsUninitRfb);
		if (status == WLAN_STATUS_RESOURCES)
			break;
	}
	if (status == WLAN_STATUS_RESOURCES) {
		DBGLOG(RX, WARN, "Start ReturnIndicatedRfbList Timer (%ds)\n", RX_RETURN_INDICATED_RFB_TIMEOUT_SEC);
		/* restart timer */
		cnmTimerStartTimer(prAdapter,
				   &prAdapter->rReturnIndicatedRfbListTimer,
				   SEC_TO_MSEC(RX_RETURN_INDICATED_RFB_TIMEOUT_SEC));
	}
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is a required function that returns information about
*        the capabilities and status of the driver and/or its network adapter.
*
* \param[IN] prAdapter        Pointer to the Adapter structure.
* \param[IN] pfnOidQryHandler Function pointer for the OID query handler.
* \param[IN] pvInfoBuf        Points to a buffer for return the query information.
* \param[IN] u4QueryBufferLen Specifies the number of bytes at pvInfoBuf.
* \param[OUT] pu4QueryInfoLen  Points to the number of bytes it written or is needed.
*
* \retval WLAN_STATUS_xxx Different WLAN_STATUS code returned by different handlers.
*
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanQueryInformation(IN P_ADAPTER_T prAdapter,
		     IN PFN_OID_HANDLER_FUNC pfnOidQryHandler,
		     IN PVOID pvInfoBuf, IN UINT_32 u4InfoBufLen, OUT PUINT_32 pu4QryInfoLen)
{
	WLAN_STATUS status = WLAN_STATUS_FAILURE;

	ASSERT(prAdapter);
	ASSERT(pu4QryInfoLen);

	/* ignore any OID request after connected, under PS current measurement mode */
	/* note: return WLAN_STATUS_FAILURE or WLAN_STATUS_SUCCESS for
	 * blocking OIDs during current measurement
	 */
	if (prAdapter->u4PsCurrentMeasureEn &&
	    (prAdapter->prGlueInfo->eParamMediaStateIndicated == PARAM_MEDIA_STATE_CONNECTED))
		return WLAN_STATUS_SUCCESS;
#if 1
	/* most OID handler will just queue a command packet */
	status = pfnOidQryHandler(prAdapter, pvInfoBuf, u4InfoBufLen, pu4QryInfoLen);
#else
	if (wlanIsHandlerNeedHwAccess(pfnOidQryHandler, FALSE)) {
		ACQUIRE_POWER_CONTROL_FROM_PM(prAdapter);

		/* Reset sleepy state */
		if (prAdapter->fgWiFiInSleepyState == TRUE)
			prAdapter->fgWiFiInSleepyState = FALSE;

		status = pfnOidQryHandler(prAdapter, pvInfoBuf, u4InfoBufLen, pu4QryInfoLen);

		RECLAIM_POWER_CONTROL_TO_PM(prAdapter, FALSE);
	} else
		status = pfnOidQryHandler(prAdapter, pvInfoBuf, u4InfoBufLen, pu4QryInfoLen);
#endif

	return status;

}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is a required function that allows bound protocol drivers,
*        or NDIS, to request changes in the state information that the miniport
*        maintains for particular object identifiers, such as changes in multicast
*        addresses.
*
* \param[IN] prAdapter     Pointer to the Glue info structure.
* \param[IN] pfnOidSetHandler     Points to the OID set handlers.
* \param[IN] pvInfoBuf     Points to a buffer containing the OID-specific data for the set.
* \param[IN] u4InfoBufLen  Specifies the number of bytes at prSetBuffer.
* \param[OUT] pu4SetInfoLen Points to the number of bytes it read or is needed.
*
* \retval WLAN_STATUS_xxx Different WLAN_STATUS code returned by different handlers.
*
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanSetInformation(IN P_ADAPTER_T prAdapter,
		   IN PFN_OID_HANDLER_FUNC pfnOidSetHandler,
		   IN PVOID pvInfoBuf, IN UINT_32 u4InfoBufLen, OUT PUINT_32 pu4SetInfoLen)
{
	WLAN_STATUS status = WLAN_STATUS_FAILURE;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	/* ignore any OID request after connected, under PS current measurement mode */
	/* note: return WLAN_STATUS_FAILURE or WLAN_STATUS_SUCCESS for blocking
	 * OIDs during current measurement
	 */
	if (prAdapter->u4PsCurrentMeasureEn &&
	    (prAdapter->prGlueInfo->eParamMediaStateIndicated == PARAM_MEDIA_STATE_CONNECTED))
		return WLAN_STATUS_SUCCESS;
#if 1
	/* most OID handler will just queue a command packet
	 * for power state transition OIDs, handler will acquire power control by itself
	 */
	status = pfnOidSetHandler(prAdapter, pvInfoBuf, u4InfoBufLen, pu4SetInfoLen);
#else
	if (wlanIsHandlerNeedHwAccess(pfnOidSetHandler, TRUE)) {
		ACQUIRE_POWER_CONTROL_FROM_PM(prAdapter);

		/* Reset sleepy state */
		if (prAdapter->fgWiFiInSleepyState == TRUE)
			prAdapter->fgWiFiInSleepyState = FALSE;

		status = pfnOidSetHandler(prAdapter, pvInfoBuf, u4InfoBufLen, pu4SetInfoLen);

		RECLAIM_POWER_CONTROL_TO_PM(prAdapter, FALSE);
	} else {
		status = pfnOidSetHandler(prAdapter, pvInfoBuf, u4InfoBufLen, pu4SetInfoLen);
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
BOOLEAN wlanQueryWapiMode(IN P_ADAPTER_T prAdapter)
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
VOID wlanSetPromiscuousMode(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgEnablePromiscuousMode)
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
VOID wlanRxSetBroadcast(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgEnableBroadcast)
{
	ASSERT(prAdapter);
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
WLAN_STATUS wlanSendNicPowerCtrlCmd(IN P_ADAPTER_T prAdapter, IN UINT_8 ucPowerMode)
{
	WLAN_STATUS status = WLAN_STATUS_SUCCESS;
	P_GLUE_INFO_T prGlueInfo;
	P_CMD_INFO_T prCmdInfo;
	P_WIFI_CMD_T prWifiCmd;
	UINT_8 ucTC, ucCmdSeqNum;

	ASSERT(prAdapter);

	prGlueInfo = prAdapter->prGlueInfo;

	/* 1. Prepare CMD */
	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, (CMD_HDR_SIZE + sizeof(CMD_NIC_POWER_CTRL)));
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
	prCmdInfo->u2InfoBufLen = (UINT_16) (CMD_HDR_SIZE + sizeof(CMD_NIC_POWER_CTRL));
	prCmdInfo->pfCmdDoneHandler = NULL;
	prCmdInfo->pfCmdTimeoutHandler = NULL;
	prCmdInfo->fgIsOid = TRUE;
	prCmdInfo->ucCID = CMD_ID_NIC_POWER_CTRL;
	prCmdInfo->fgSetQuery = TRUE;
	prCmdInfo->fgNeedResp = FALSE;
	prCmdInfo->fgDriverDomainMCR = FALSE;
	prCmdInfo->ucCmdSeqNum = ucCmdSeqNum;
	prCmdInfo->u4SetInfoLen = sizeof(CMD_NIC_POWER_CTRL);

	/* 2.3 Setup WIFI_CMD_T */
	prWifiCmd = (P_WIFI_CMD_T) (prCmdInfo->pucInfoBuffer);
	prWifiCmd->u2TxByteCount_UserPriority = prCmdInfo->u2InfoBufLen;
	prWifiCmd->ucCID = prCmdInfo->ucCID;
	prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
	prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;

	kalMemZero(prWifiCmd->aucBuffer, sizeof(CMD_NIC_POWER_CTRL));
	((P_CMD_NIC_POWER_CTRL) (prWifiCmd->aucBuffer))->ucPowerMode = ucPowerMode;

	/* 3. Issue CMD for entering specific power mode */
	ucTC = TC4_INDEX;

	while (1) {
		/* 3.0 Removal check */
		if (kalIsCardRemoved(prAdapter->prGlueInfo) == TRUE || fgIsBusAccessFailed == TRUE) {
			status = WLAN_STATUS_FAILURE;
			break;
		}
		/* 3.1 Acquire TX Resource */
		if (nicTxAcquireResource(prAdapter, ucTC, FALSE) == WLAN_STATUS_RESOURCES) {

			/* wait and poll tx resource */
			if (nicTxPollingResource(prAdapter, ucTC) != WLAN_STATUS_SUCCESS) {
				DBGLOG(INIT, ERROR, "Fail to get TX resource return within timeout\n");
				status = WLAN_STATUS_FAILURE;
#if CFG_ENABLE_KEYWORD_EXCEPTION_MECHANISM
				mtk_wcn_wmt_assert_keyword(WMTDRV_TYPE_WIFI,
					"[Wi-Fi Off] Fail to get TX resource return within timeout");
#endif
				break;
			}
			continue;
		}
		/* 3.2 Send CMD Info Packet */
		if (nicTxCmd(prAdapter, prCmdInfo, ucTC) != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, "Fail to transmit CMD_NIC_POWER_CTRL command\n");
			status = WLAN_STATUS_FAILURE;
#if CFG_ENABLE_KEYWORD_EXCEPTION_MECHANISM
			mtk_wcn_wmt_assert_keyword(WMTDRV_TYPE_WIFI,
				"[Wi-Fi Off] Fail to transmit CMD_NIC_POWER_CTRL command");
#endif
		}

		break;
	};

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
BOOLEAN wlanIsHandlerAllowedInRFTest(IN PFN_OID_HANDLER_FUNC pfnOidHandler, IN BOOLEAN fgSetInfo)
{
	PFN_OID_HANDLER_FUNC *apfnOidHandlerAllowedInRFTest;
	UINT_32 i;
	UINT_32 u4NumOfElem;

	if (fgSetInfo) {
		apfnOidHandlerAllowedInRFTest = apfnOidSetHandlerAllowedInRFTest;
		u4NumOfElem = sizeof(apfnOidSetHandlerAllowedInRFTest) / sizeof(PFN_OID_HANDLER_FUNC);
	} else {
		apfnOidHandlerAllowedInRFTest = apfnOidQueryHandlerAllowedInRFTest;
		u4NumOfElem = sizeof(apfnOidQueryHandlerAllowedInRFTest) / sizeof(PFN_OID_HANDLER_FUNC);
	}

	for (i = 0; i < u4NumOfElem; i++) {
		if (apfnOidHandlerAllowedInRFTest[i] == pfnOidHandler)
			return TRUE;
	}

	return FALSE;
}

#if CFG_ENABLE_FW_DOWNLOAD
#if CFG_ENABLE_FW_DOWNLOAD_AGGREGATION
/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to download FW image in an aggregated way
*
* @param prAdapter      Pointer to the Adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanImageSectionDownloadAggregated(IN P_ADAPTER_T prAdapter,
				   IN UINT_32 u4DestAddr, IN UINT_32 u4ImgSecSize, IN PUINT_8 pucImgSecBuf)
{
#if defined(MT6620) || defined(MT6628)
	P_CMD_INFO_T prCmdInfo;
	P_INIT_HIF_TX_HEADER_T prInitHifTxHeader;
	P_INIT_CMD_DOWNLOAD_BUF prInitCmdDownloadBuf;
	UINT_8 ucTC, ucCmdSeqNum;
	WLAN_STATUS u4Status = WLAN_STATUS_SUCCESS;
	PUINT_8 pucOutputBuf = (PUINT_8) NULL;	/* Pointer to Transmit Data Structure Frame */
	UINT_32 u4PktCnt, u4Offset, u4Length;
	UINT_32 u4TotalLength;

	ASSERT(prAdapter);
	ASSERT(pucImgSecBuf);

	pucOutputBuf = prAdapter->rTxCtrl.pucTxCoalescingBufPtr;

	DEBUGFUNC("wlanImageSectionDownloadAggregated");

	if (u4ImgSecSize == 0)
		return WLAN_STATUS_SUCCESS;
	/* 1. Allocate CMD Info Packet and Pre-fill Headers */
	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter,
					  sizeof(INIT_HIF_TX_HEADER_T) + sizeof(INIT_CMD_DOWNLOAD_BUF) +
					  CMD_PKT_SIZE_FOR_IMAGE);

	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}

	prCmdInfo->u2InfoBufLen = sizeof(INIT_HIF_TX_HEADER_T) + sizeof(INIT_CMD_DOWNLOAD_BUF) + CMD_PKT_SIZE_FOR_IMAGE;

	/* 2. Use TC0's resource to download image. (only TC0 is allowed) */
	ucTC = TC0_INDEX;

	/* 3. Setup common CMD Info Packet */
	prInitHifTxHeader = (P_INIT_HIF_TX_HEADER_T) (prCmdInfo->pucInfoBuffer);
	prInitHifTxHeader->ucEtherTypeOffset = 0;
	prInitHifTxHeader->ucCSflags = 0;
	prInitHifTxHeader->rInitWifiCmd.ucCID = INIT_CMD_ID_DOWNLOAD_BUF;

	/* 4. Setup CMD_DOWNLOAD_BUF */
	prInitCmdDownloadBuf = (P_INIT_CMD_DOWNLOAD_BUF) (prInitHifTxHeader->rInitWifiCmd.aucBuffer);
	prInitCmdDownloadBuf->u4DataMode = 0
#if CFG_ENABLE_FW_ENCRYPTION
	    | DOWNLOAD_BUF_ENCRYPTION_MODE
#endif
	    ;

	/* 5.0 reset loop control variable */
	u4TotalLength = 0;
	u4Offset = u4PktCnt = 0;

	/* 5.1 main loop for maximize transmission count per access */
	while (u4Offset < u4ImgSecSize) {
		if (nicTxAcquireResource(prAdapter, ucTC, FALSE) == WLAN_STATUS_SUCCESS) {
			/* 5.1.1 calculate u4Length */
			if (u4Offset + CMD_PKT_SIZE_FOR_IMAGE < u4ImgSecSize)
				u4Length = CMD_PKT_SIZE_FOR_IMAGE;
			else
				u4Length = u4ImgSecSize - u4Offset;

			/* 5.1.1 increase command sequence number */
			ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);
			prInitHifTxHeader->rInitWifiCmd.ucSeqNum = ucCmdSeqNum;

			/* 5.1.2 update HIF TX hardware header */
			prInitHifTxHeader->u2TxByteCount =
			    ALIGN_4(sizeof(INIT_HIF_TX_HEADER_T) + sizeof(INIT_CMD_DOWNLOAD_BUF) + (UINT_16) u4Length);

			/* 5.1.3 fill command header */
			prInitCmdDownloadBuf->u4Address = u4DestAddr + u4Offset;
			prInitCmdDownloadBuf->u4Length = u4Length;
			prInitCmdDownloadBuf->u4CRC32 = wlanCRC32(pucImgSecBuf + u4Offset, u4Length);

			/* 5.1.4.1 copy header to coalescing buffer */
			kalMemCopy(pucOutputBuf + u4TotalLength,
				   (PVOID) prCmdInfo->pucInfoBuffer,
				   sizeof(INIT_HIF_TX_HEADER_T) + sizeof(INIT_CMD_DOWNLOAD_BUF));

			/* 5.1.4.2 copy payload to coalescing buffer */
			kalMemCopy(pucOutputBuf + u4TotalLength + sizeof(INIT_HIF_TX_HEADER_T) +
				   sizeof(INIT_CMD_DOWNLOAD_BUF), pucImgSecBuf + u4Offset, u4Length);

			/* 5.1.4.3 update length and other variables */
			u4TotalLength +=
			    ALIGN_4(sizeof(INIT_HIF_TX_HEADER_T) + sizeof(INIT_CMD_DOWNLOAD_BUF) + u4Length);
			u4Offset += u4Length;
			u4PktCnt++;

			if (u4Offset < u4ImgSecSize)
				continue;
		} else if (u4PktCnt == 0) {
			/* no resource, so get some back */
			if (nicTxPollingResource(prAdapter, ucTC) != WLAN_STATUS_SUCCESS) {
				u4Status = WLAN_STATUS_FAILURE;
				DBGLOG(INIT, ERROR, "Fail to get TX resource return within timeout\n");
				break;
			}
		}

		if (u4PktCnt != 0) {
			/* start transmission */
			HAL_WRITE_TX_PORT(prAdapter,
					  0,
					  u4TotalLength, (PUINT_8) pucOutputBuf, prAdapter->u4CoalescingBufCachedSize);

			/* reset varaibles */
			u4PktCnt = 0;
			u4TotalLength = 0;
		}
	}

	/* 8. Free CMD Info Packet. */
	cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

	return u4Status;

#else
#error "Only MT6620/MT6628/MT6582 supports firmware download in an aggregated way"

	return WLAN_STATUS_FAILURE;

#endif
}

#endif
/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to download FW image.
*
* @param prAdapter      Pointer to the Adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanImageSectionDownload(IN P_ADAPTER_T prAdapter,
			 IN UINT_32 u4DestAddr, IN UINT_32 u4ImgSecSize, IN PUINT_8 pucImgSecBuf)
{
	P_CMD_INFO_T prCmdInfo;
	P_INIT_HIF_TX_HEADER_T prInitHifTxHeader;
	P_INIT_CMD_DOWNLOAD_BUF prInitCmdDownloadBuf;
	UINT_8 ucTC, ucCmdSeqNum;
	WLAN_STATUS u4Status = WLAN_STATUS_SUCCESS;

	ASSERT(prAdapter);
	ASSERT(pucImgSecBuf);
	ASSERT(u4ImgSecSize <= CMD_PKT_SIZE_FOR_IMAGE);

	DEBUGFUNC("wlanImageSectionDownload");

	if (u4ImgSecSize == 0)
		return WLAN_STATUS_SUCCESS;
	/* 1. Allocate CMD Info Packet and its Buffer. */
	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter,
					  sizeof(INIT_HIF_TX_HEADER_T) + sizeof(INIT_CMD_DOWNLOAD_BUF) + u4ImgSecSize);

	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}

	prCmdInfo->u2InfoBufLen = sizeof(INIT_HIF_TX_HEADER_T) + sizeof(INIT_CMD_DOWNLOAD_BUF) + (UINT_16) u4ImgSecSize;

	/* 2. Use TC0's resource to download image. (only TC0 is allowed) */
	ucTC = TC0_INDEX;

	/* 3. increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

	/* 4. Setup common CMD Info Packet */
	prInitHifTxHeader = (P_INIT_HIF_TX_HEADER_T) (prCmdInfo->pucInfoBuffer);
	prInitHifTxHeader->rInitWifiCmd.ucCID = INIT_CMD_ID_DOWNLOAD_BUF;
	prInitHifTxHeader->rInitWifiCmd.ucSeqNum = ucCmdSeqNum;

	/* 5. Setup CMD_DOWNLOAD_BUF */
	prInitCmdDownloadBuf = (P_INIT_CMD_DOWNLOAD_BUF) (prInitHifTxHeader->rInitWifiCmd.aucBuffer);
	prInitCmdDownloadBuf->u4Address = u4DestAddr;
	prInitCmdDownloadBuf->u4Length = u4ImgSecSize;
	prInitCmdDownloadBuf->u4CRC32 = wlanCRC32(pucImgSecBuf, u4ImgSecSize);

	prInitCmdDownloadBuf->u4DataMode = 0
#if CFG_ENABLE_FW_DOWNLOAD_ACK
	    | DOWNLOAD_BUF_ACK_OPTION	/* ACK needed */
#endif
#if CFG_ENABLE_FW_ENCRYPTION
	    | DOWNLOAD_BUF_ENCRYPTION_MODE
#endif
	    ;

	kalMemCopy(prInitCmdDownloadBuf->aucBuffer, pucImgSecBuf, u4ImgSecSize);

	/* 6. Send FW_Download command */
	while (1) {
		/* 6.1 Acquire TX Resource */
		if (nicTxAcquireResource(prAdapter, ucTC, FALSE) == WLAN_STATUS_RESOURCES) {
			if (nicTxPollingResource(prAdapter, ucTC) != WLAN_STATUS_SUCCESS) {
				u4Status = WLAN_STATUS_FAILURE;
				DBGLOG(INIT, ERROR, "Fail to get TX resource return within timeout\n");
				break;
			}
			continue;
		}

		wlanFWDLDebugAddTxStartTime(kalGetTimeTick());

		/* 6.2 Send CMD Info Packet */
		if (nicTxInitCmd(prAdapter, prCmdInfo, ucTC) != WLAN_STATUS_SUCCESS) {
			u4Status = WLAN_STATUS_FAILURE;
			DBGLOG(INIT, ERROR, "Fail to transmit image download command\n");
		}

		wlanFWDLDebugAddTxDoneTime(kalGetTimeTick());

		break;
	};

#if CFG_ENABLE_FW_DOWNLOAD_ACK
	/* 7. Wait for INIT_EVENT_ID_CMD_RESULT */
	u4Status = wlanImageSectionDownloadStatus(prAdapter, ucCmdSeqNum);
#endif

	/* 8. Free CMD Info Packet. */
	cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

	return u4Status;
}

#if !CFG_ENABLE_FW_DOWNLOAD_ACK
/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to confirm previously firmware download is done without error
*
* @param prAdapter      Pointer to the Adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanImageQueryStatus(IN P_ADAPTER_T prAdapter)
{
	P_CMD_INFO_T prCmdInfo;
	P_INIT_HIF_TX_HEADER_T prInitHifTxHeader;
	UINT_8 aucBuffer[sizeof(INIT_HIF_RX_HEADER_T) + sizeof(INIT_EVENT_PENDING_ERROR)];
	UINT_32 u4RxPktLength;
	P_INIT_HIF_RX_HEADER_T prInitHifRxHeader;
	P_INIT_EVENT_PENDING_ERROR prEventPendingError;
	WLAN_STATUS u4Status = WLAN_STATUS_SUCCESS;
	UINT_8 ucTC, ucCmdSeqNum;

	ASSERT(prAdapter);

	DEBUGFUNC("wlanImageQueryStatus");

	/* 1. Allocate CMD Info Packet and it Buffer. */
	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, sizeof(INIT_HIF_TX_HEADER_T));

	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}

	kalMemZero(prCmdInfo, sizeof(INIT_HIF_TX_HEADER_T));
	prCmdInfo->u2InfoBufLen = sizeof(INIT_HIF_TX_HEADER_T);

	/* 2. Use TC0's resource to download image. (only TC0 is allowed) */
	ucTC = TC0_INDEX;

	/* 3. increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

	/* 4. Setup common CMD Info Packet */
	prInitHifTxHeader = (P_INIT_HIF_TX_HEADER_T) (prCmdInfo->pucInfoBuffer);
	prInitHifTxHeader->rInitWifiCmd.ucCID = INIT_CMD_ID_QUERY_PENDING_ERROR;
	prInitHifTxHeader->rInitWifiCmd.ucSeqNum = ucCmdSeqNum;

	/* 5. Send command */
	while (1) {
		/* 5.1 Acquire TX Resource */
		if (nicTxAcquireResource(prAdapter, ucTC, FALSE) == WLAN_STATUS_RESOURCES) {
			if (nicTxPollingResource(prAdapter, ucTC) != WLAN_STATUS_SUCCESS) {
				u4Status = WLAN_STATUS_FAILURE;
				DBGLOG(INIT, ERROR, "Fail to get TX resource return within timeout\n");
				break;
			}
			continue;
		}
		/* 5.2 Send CMD Info Packet */
		if (nicTxInitCmd(prAdapter, prCmdInfo, ucTC) != WLAN_STATUS_SUCCESS) {
			u4Status = WLAN_STATUS_FAILURE;
			DBGLOG(INIT, ERROR, "Fail to transmit image download command\n");
		}

		break;
	};

	/* 6. Wait for INIT_EVENT_ID_PENDING_ERROR */
	do {
		if (kalIsCardRemoved(prAdapter->prGlueInfo) == TRUE || fgIsBusAccessFailed == TRUE) {
			u4Status = WLAN_STATUS_FAILURE;
		} else if (nicRxWaitResponse(prAdapter,
					     0,
					     aucBuffer,
					     sizeof(INIT_HIF_RX_HEADER_T) + sizeof(INIT_EVENT_PENDING_ERROR),
					     &u4RxPktLength) != WLAN_STATUS_SUCCESS) {
			u4Status = WLAN_STATUS_FAILURE;
		} else {
			prInitHifRxHeader = (P_INIT_HIF_RX_HEADER_T) aucBuffer;

			/* EID / SeqNum check */
			if (prInitHifRxHeader->rInitWifiEvent.ucEID != INIT_EVENT_ID_PENDING_ERROR) {
				u4Status = WLAN_STATUS_FAILURE;
			} else if (prInitHifRxHeader->rInitWifiEvent.ucSeqNum != ucCmdSeqNum) {
				u4Status = WLAN_STATUS_FAILURE;
			} else {
				prEventPendingError =
				    (P_INIT_EVENT_PENDING_ERROR) (prInitHifRxHeader->rInitWifiEvent.aucBuffer);
				if (prEventPendingError->ucStatus != 0) {	/* 0 for download success */
					u4Status = WLAN_STATUS_FAILURE;
				} else {
					u4Status = WLAN_STATUS_SUCCESS;
				}
			}
		}
	} while (FALSE);

	/* 7. Free CMD Info Packet. */
	cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

	return u4Status;
}

#else
/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to confirm the status of
*        previously downloaded firmware scatter
*
* @param prAdapter      Pointer to the Adapter structure.
*        ucCmdSeqNum    Sequence number of previous firmware scatter
*
* @return WLAN_STATUS_SUCCESS
*         WLAN_STATUS_FAILURE
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanImageSectionDownloadStatus(IN P_ADAPTER_T prAdapter, IN UINT_8 ucCmdSeqNum)
{
	UINT_8 aucBuffer[sizeof(INIT_HIF_RX_HEADER_T) + sizeof(INIT_EVENT_CMD_RESULT)];
	P_INIT_HIF_RX_HEADER_T prInitHifRxHeader;
	P_INIT_EVENT_CMD_RESULT prEventCmdResult;
	UINT_32 u4RxPktLength;
	WLAN_STATUS u4Status;

	ASSERT(prAdapter);

	do {
		if (kalIsCardRemoved(prAdapter->prGlueInfo) == TRUE || fgIsBusAccessFailed == TRUE) {
			DBGLOG(INIT, ERROR, "kalIsCardRemoved or fgIsBusAccessFailed\n");
			u4Status = WLAN_STATUS_FAILURE;
		} else if (nicRxWaitResponse(prAdapter,
					0,
					aucBuffer,
					sizeof(INIT_HIF_RX_HEADER_T) + sizeof(INIT_EVENT_CMD_RESULT),/* 4B + 4B */
					&u4RxPktLength) != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, "nicRxWaitResponse fail at SeqNo (%d)\n", ucCmdSeqNum);

			/*Dump WLAN TX Status Register TQ0_CNT, make sure FW write ROM success before CMD send*/
			wlanDumpTxReleaseCount(prAdapter);
			/*Dump  TX_DESC and RX_DESC*/
			wlanDebugHifDescriptorDump(prAdapter, MTK_AMPDU_TX_DESC, DEBUG_TC0_INDEX);
			wlanDebugHifDescriptorDump(prAdapter, MTK_AMPDU_RX_DESC, DEBUG_TC0_INDEX);
			GL_RESET_TRIGGER(prAdapter, RST_FLAG_DO_CORE_DUMP | RST_FLAG_PREVENT_POWER_OFF);
			u4Status = WLAN_STATUS_FAILURE;
		} else {
			prInitHifRxHeader = (P_INIT_HIF_RX_HEADER_T) aucBuffer;
			/* EID / SeqNum check */
			if (prInitHifRxHeader->rInitWifiEvent.ucEID != INIT_EVENT_ID_CMD_RESULT) {
				DBGLOG(INIT, ERROR, "rInitWifiEvent.ucEID != INIT_EVENT_ID_CMD_RESULT\n");
				u4Status = WLAN_STATUS_FAILURE;
				kalSendAeeWarning("[Check EID error!]", __func__);
			} else if (prInitHifRxHeader->rInitWifiEvent.ucSeqNum != ucCmdSeqNum) {
				DBGLOG(INIT, ERROR, "rInitWifiEvent.ucSeqNum != ucCmdSeqNum\n");
				u4Status = WLAN_STATUS_FAILURE;
				kalSendAeeWarning("[Check SeqNum error!]", __func__);
			} else {
				prEventCmdResult =
				    (P_INIT_EVENT_CMD_RESULT) (prInitHifRxHeader->rInitWifiEvent.aucBuffer);
				if (prEventCmdResult->ucStatus != 0) {	/* 0 for download success */
					/*
					 *  0: success
					 *  1: rejected by invalid param
					 *  2: rejected by incorrect CRC
					 *  3: rejected by decryption failure
					 *  4: unknown CMD
					 */
					DBGLOG(INIT, ERROR, "Read Response status error = %d\n",
							     prEventCmdResult->ucStatus);
					u4Status = WLAN_STATUS_FAILURE;
				} else {
					u4Status = WLAN_STATUS_SUCCESS;
				}
			}
			if (u4Status == WLAN_STATUS_FAILURE)
				GL_RESET_TRIGGER(prAdapter, RST_FLAG_DO_CORE_DUMP | RST_FLAG_PREVENT_POWER_OFF);
		}
	} while (FALSE);

	return u4Status;
}

#endif
/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to start FW normal operation.
*
* @param prAdapter      Pointer to the Adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanConfigWifiFunc(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgEnable, IN UINT_32 u4StartAddress)
{
	P_CMD_INFO_T prCmdInfo;
	P_INIT_HIF_TX_HEADER_T prInitHifTxHeader;
	P_INIT_CMD_WIFI_START prInitCmdWifiStart;
	UINT_8 ucTC, ucCmdSeqNum;
	WLAN_STATUS u4Status = WLAN_STATUS_SUCCESS;

	ASSERT(prAdapter);

	DEBUGFUNC("wlanConfigWifiFunc");

	/* 1. Allocate CMD Info Packet and its Buffer. */
	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, sizeof(INIT_HIF_TX_HEADER_T) + sizeof(INIT_CMD_WIFI_START));

	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}

	kalMemZero(prCmdInfo->pucInfoBuffer, sizeof(INIT_HIF_TX_HEADER_T) + sizeof(INIT_CMD_WIFI_START));
	prCmdInfo->u2InfoBufLen = sizeof(INIT_HIF_TX_HEADER_T) + sizeof(INIT_CMD_WIFI_START);

	/* 2. Always use TC0 */
	ucTC = TC0_INDEX;

	/* 3. increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

	/* 4. Setup common CMD Info Packet */
	prInitHifTxHeader = (P_INIT_HIF_TX_HEADER_T) (prCmdInfo->pucInfoBuffer);
	prInitHifTxHeader->rInitWifiCmd.ucCID = INIT_CMD_ID_WIFI_START;
	prInitHifTxHeader->rInitWifiCmd.ucSeqNum = ucCmdSeqNum;

	prInitCmdWifiStart = (P_INIT_CMD_WIFI_START) (prInitHifTxHeader->rInitWifiCmd.aucBuffer);
	prInitCmdWifiStart->u4Override = (fgEnable == TRUE ? 1 : 0);
	prInitCmdWifiStart->u4Address = u4StartAddress;

	/* 5. Seend WIFI start command */
	while (1) {
		/* 5.1 Acquire TX Resource */
		if (nicTxAcquireResource(prAdapter, ucTC, FALSE) == WLAN_STATUS_RESOURCES) {

			/* wait and poll tx resource */
			if (nicTxPollingResource(prAdapter, ucTC) != WLAN_STATUS_SUCCESS) {
				u4Status = WLAN_STATUS_FAILURE;
				DBGLOG(INIT, ERROR, "Fail to get TX resource return within timeout\n");
				break;
			}

			continue;
		}
		/* 5.2 Send CMD Info Packet */
		if (nicTxInitCmd(prAdapter, prCmdInfo, ucTC) != WLAN_STATUS_SUCCESS) {
			u4Status = WLAN_STATUS_FAILURE;
			DBGLOG(INIT, ERROR, "Fail to transmit WIFI start command\n");
		}

		break;
	};

	/* 6. Free CMD Info Packet. */
	cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

	return u4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is used to generate CRC32 checksum
*
* @param buf Pointer to the data.
* @param len data length
*
* @return crc32 value
*/
/*----------------------------------------------------------------------------*/
static const UINT_32 crc32_ccitt_table[256] = {
		0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419,
		0x706af48f, 0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4,
		0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07,
		0x90bf1d91, 0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de,
		0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7, 0x136c9856,
		0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
		0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4,
		0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
		0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3,
		0x45df5c75, 0xdcd60dcf, 0xabd13d59, 0x26d930ac, 0x51de003a,
		0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599,
		0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
		0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190,
		0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f,
		0x9fbfe4a5, 0xe8b8d433, 0x7807c9a2, 0x0f00f934, 0x9609a88e,
		0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
		0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed,
		0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
		0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3,
		0xfbd44c65, 0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2,
		0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a,
		0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5,
		0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa, 0xbe0b1010,
		0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
		0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17,
		0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6,
		0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615,
		0x73dc1683, 0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
		0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1, 0xf00f9344,
		0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
		0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a,
		0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
		0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1,
		0xa6bc5767, 0x3fb506dd, 0x48b2364b, 0xd80d2bda, 0xaf0a1b4c,
		0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef,
		0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
		0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe,
		0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31,
		0x2cd99e8b, 0x5bdeae1d, 0x9b64c2b0, 0xec63f226, 0x756aa39c,
		0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
		0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b,
		0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
		0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1,
		0x18b74777, 0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c,
		0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45, 0xa00ae278,
		0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7,
		0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc, 0x40df0b66,
		0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
		0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605,
		0xcdd70693, 0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8,
		0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b,
		0x2d02ef8d
	};

UINT_32 wlanCRC32(PUINT_8 buf, UINT_32 len)
{
	UINT_32 i, crc32 = 0xFFFFFFFF;

	for (i = 0; i < len; i++)
		crc32 = crc32_ccitt_table[(crc32 ^ buf[i]) & 0xff] ^ (crc32 >> 8);

	return ~crc32;
}
#endif

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
WLAN_STATUS wlanProcessQueuedSwRfb(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfbListHead)
{
	P_SW_RFB_T prSwRfb, prNextSwRfb;
	P_TX_CTRL_T prTxCtrl;
	P_RX_CTRL_T prRxCtrl;
	P_STA_RECORD_T prStaRec;

	ASSERT(prAdapter);
	ASSERT(prSwRfbListHead);

	prTxCtrl = &prAdapter->rTxCtrl;
	prRxCtrl = &prAdapter->rRxCtrl;

	prSwRfb = prSwRfbListHead;

	do {
		/* save next first */
		prNextSwRfb = (P_SW_RFB_T) QUEUE_GET_NEXT_ENTRY((P_QUE_ENTRY_T) prSwRfb);

		switch (prSwRfb->eDst) {
		case RX_PKT_DESTINATION_HOST:
			/* to host */
			prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);
			if (prStaRec && IS_STA_IN_AIS(prStaRec)) {
#if ARP_MONITER_ENABLE
				qmHandleRxArpPackets(prAdapter, prSwRfb);
#endif
			}
			nicRxProcessPktWithoutReorder(prAdapter, prSwRfb);
			break;

		case RX_PKT_DESTINATION_FORWARD:
			/* need ot forward */
			nicRxProcessForwardPkt(prAdapter, prSwRfb);
			break;

		case RX_PKT_DESTINATION_HOST_WITH_FORWARD:
			/* to host and forward */
			nicRxProcessGOBroadcastPkt(prAdapter, prSwRfb);
			break;

		case RX_PKT_DESTINATION_NULL:
			/* free it */
			nicRxReturnRFB(prAdapter, prSwRfb);
			break;

		default:
			break;
		}

#if CFG_HIF_RX_STARVATION_WARNING
		prRxCtrl->u4DequeuedCnt++;
#endif

		/* check next queued packet */
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
WLAN_STATUS wlanProcessQueuedMsduInfo(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfoListHead)
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
BOOLEAN wlanoidTimeoutCheck(IN P_ADAPTER_T prAdapter, IN PFN_OID_HANDLER_FUNC pfnOidHandler)
{
	PFN_OID_HANDLER_FUNC *apfnOidHandlerWOTimeoutCheck;
	UINT_32 i;
	UINT_32 u4NumOfElem;

	apfnOidHandlerWOTimeoutCheck = apfnOidWOTimeoutCheck;
	u4NumOfElem = sizeof(apfnOidWOTimeoutCheck) / sizeof(PFN_OID_HANDLER_FUNC);

	/* skip some OID timeout checks ? */
	for (i = 0; i < u4NumOfElem; i++) {
		if (apfnOidHandlerWOTimeoutCheck[i] == pfnOidHandler)
			return FALSE;
	}

	/* set timer if need timeout check */
	/* cnmTimerStartTimer(prAdapter, */
	/* &(prAdapter->rOidTimeoutTimer), */
	/* 1000); */
	cnmTimerStartTimer(prAdapter, &(prAdapter->rOidTimeoutTimer), 2000);

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
VOID wlanoidClearTimeoutCheck(IN P_ADAPTER_T prAdapter)
{
	ASSERT(prAdapter);

	cnmTimerStopTimer(prAdapter, &(prAdapter->rOidTimeoutTimer));
}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to update network address in firmware domain
*
* @param prAdapter          Pointer to the Adapter structure.
*
* @return WLAN_STATUS_FAILURE   The request could not be processed
*         WLAN_STATUS_PENDING   The request has been queued for later processing
*         WLAN_STATUS_SUCCESS   The request has been processed
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanUpdateNetworkAddress(IN P_ADAPTER_T prAdapter)
{
	const UINT_8 aucZeroMacAddr[] = NULL_MAC_ADDR;
	PARAM_MAC_ADDRESS rMacAddr = {0};
	UINT_8 ucCmdSeqNum;
	P_CMD_INFO_T prCmdInfo;
	P_WIFI_CMD_T prWifiCmd;
	P_CMD_BASIC_CONFIG prCmdBasicConfig;
	UINT_32 u4SysTime;

	DEBUGFUNC("wlanUpdateNetworkAddress");

	ASSERT(prAdapter);

	if (kalRetrieveNetworkAddress(prAdapter->prGlueInfo, &rMacAddr) == FALSE || IS_BMCAST_MAC_ADDR(rMacAddr)
	    || EQUAL_MAC_ADDR(aucZeroMacAddr, rMacAddr)) {
		/* eFUSE has a valid address, don't do anything */
		if (prAdapter->fgIsEmbbededMacAddrValid == TRUE) {
#if CFG_SHOW_MACADDR_SOURCE
			DBGLOG(INIT, INFO, "Using embedded MAC address");
#endif
			return WLAN_STATUS_SUCCESS;
		}
#if CFG_SHOW_MACADDR_SOURCE
		DBGLOG(INIT, TRACE, "Using dynamically generated MAC address");
#endif
		/* dynamic generate */
		u4SysTime = kalGetTimeTick();

		rMacAddr[0] = 0x00;
		rMacAddr[1] = 0x08;
		rMacAddr[2] = 0x22;

		kalMemCopy(&rMacAddr[3], &u4SysTime, 3);
	} else {
#if CFG_SHOW_MACADDR_SOURCE
		DBGLOG(INIT, INFO, "Using host-supplied MAC address");
#endif
	}

	/* allocate command memory */
	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, CMD_HDR_SIZE + sizeof(CMD_BASIC_CONFIG));

	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}
	/* increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

	/* compose CMD_BUILD_CONNECTION cmd pkt */
	prCmdInfo->eCmdType = COMMAND_TYPE_GENERAL_IOCTL;
	prCmdInfo->u2InfoBufLen = CMD_HDR_SIZE + sizeof(CMD_BASIC_CONFIG);
	prCmdInfo->pfCmdDoneHandler = NULL;
	prCmdInfo->pfCmdTimeoutHandler = NULL;
	prCmdInfo->fgIsOid = FALSE;
	prCmdInfo->ucCID = CMD_ID_BASIC_CONFIG;
	prCmdInfo->fgSetQuery = TRUE;
	prCmdInfo->fgNeedResp = FALSE;
	prCmdInfo->fgDriverDomainMCR = FALSE;
	prCmdInfo->ucCmdSeqNum = ucCmdSeqNum;
	prCmdInfo->u4SetInfoLen = sizeof(CMD_BASIC_CONFIG);

	/* Setup WIFI_CMD_T */
	prWifiCmd = (P_WIFI_CMD_T) (prCmdInfo->pucInfoBuffer);
	prWifiCmd->u2TxByteCount_UserPriority = prCmdInfo->u2InfoBufLen;
	prWifiCmd->ucCID = prCmdInfo->ucCID;
	prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
	prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;

	/* configure CMD_BASIC_CONFIG */
	prCmdBasicConfig = (P_CMD_BASIC_CONFIG) (prWifiCmd->aucBuffer);
	kalMemCopy(&(prCmdBasicConfig->rMyMacAddr), &rMacAddr, PARAM_MAC_ADDR_LEN);
	prCmdBasicConfig->ucNative80211 = 0;
	prCmdBasicConfig->rCsumOffload.u2RxChecksum = 0;
	prCmdBasicConfig->rCsumOffload.u2TxChecksum = 0;

#if CFG_TCP_IP_CHKSUM_OFFLOAD
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

	if (prAdapter->u4CSUMFlags & (CSUM_OFFLOAD_EN_RX_IPv4 | CSUM_OFFLOAD_EN_RX_IPv6))
		prCmdBasicConfig->rCsumOffload.u2RxChecksum |= BIT(0);
#endif

	/* send the command to FW */
	if (wlanSendCommand(prAdapter, prCmdInfo) == WLAN_STATUS_RESOURCES) {

		/* backup the command to wait response */
		prCmdInfo->pfCmdDoneHandler = nicCmdEventQueryAddress;
		kalEnqueueCommand(prAdapter->prGlueInfo, (P_QUE_ENTRY_T) prCmdInfo);

		return WLAN_STATUS_PENDING;
	}
	/* send ok without response */
	nicCmdEventQueryAddress(prAdapter, prCmdInfo, (PUINT_8) prCmdBasicConfig);
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
BOOLEAN wlanQueryTestMode(IN P_ADAPTER_T prAdapter)
{
	ASSERT(prAdapter);

	return prAdapter->fgTestMode;
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
BOOLEAN wlanProcessSecurityFrame(IN P_ADAPTER_T prAdapter, IN P_NATIVE_PACKET prPacket)
{
	UINT_8 ucPriorityParam;
	UINT_8 aucEthDestAddr[PARAM_MAC_ADDR_LEN];
	BOOLEAN fgIs1x = FALSE;
	BOOLEAN fgIsPAL = FALSE;
	UINT_32 u4PacketLen;
	ULONG u4SysTime;
	UINT_8 ucNetworkType;
	P_CMD_INFO_T prCmdInfo;
	UINT_8 ucCmdSeqNo = 0;

	/* 1x data packets */
	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);
	ASSERT(prPacket);

	/* retrieve some information for packet classification */
	if (kalQoSFrameClassifierAndPacketInfo(prAdapter->prGlueInfo,
						prPacket,
						&ucPriorityParam,
						&u4PacketLen,
						aucEthDestAddr,
						&fgIs1x,
						&fgIsPAL,
						&ucNetworkType,
						&ucCmdSeqNo) == TRUE) {
		/* almost TRUE except frame length < 14B */

		if (fgIs1x == FALSE)
			return FALSE;

		/* get a free command entry */
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_RESOURCE);
		QUEUE_REMOVE_HEAD(&prAdapter->rFreeCmdList, prCmdInfo, P_CMD_INFO_T);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_RESOURCE);

		if (prCmdInfo) {
			P_STA_RECORD_T prStaRec;

			/* fill arrival time */
			u4SysTime = (OS_SYSTIME) kalGetTimeTick();
			GLUE_SET_PKT_ARRIVAL_TIME(prPacket, u4SysTime);

			kalMemZero(prCmdInfo, sizeof(CMD_INFO_T));

			prCmdInfo->eCmdType = COMMAND_TYPE_SECURITY_FRAME;
			prCmdInfo->u2InfoBufLen = (UINT_16) u4PacketLen;
			prCmdInfo->pucInfoBuffer = NULL;
			prCmdInfo->prPacket = prPacket;
			prCmdInfo->ucCmdSeqNum = ucCmdSeqNo;
#if 0
			prCmdInfo->ucStaRecIndex = qmGetStaRecIdx(prAdapter,
								  aucEthDestAddr,
								  (ENUM_NETWORK_TYPE_INDEX_T) ucNetworkType);
#endif
			prStaRec = cnmGetStaRecByAddress(prAdapter,
							 (ENUM_NETWORK_TYPE_INDEX_T) ucNetworkType,
							 aucEthDestAddr);
			if (prStaRec)
				prCmdInfo->ucStaRecIndex = prStaRec->ucIndex;
			else
				prCmdInfo->ucStaRecIndex = STA_REC_INDEX_NOT_FOUND;

			prCmdInfo->eNetworkType = (ENUM_NETWORK_TYPE_INDEX_T) ucNetworkType;
			prCmdInfo->pfCmdDoneHandler = wlanSecurityFrameTxDone;
			prCmdInfo->pfCmdTimeoutHandler = wlanSecurityFrameTxTimeout;
			prCmdInfo->fgIsOid = FALSE;
			prCmdInfo->fgSetQuery = TRUE;
			prCmdInfo->fgNeedResp = FALSE;

			/*
			 * queue the 1x packet and we will send the packet to CONNSYS by
			 *  using command queue
			 */
			kalEnqueueCommand(prAdapter->prGlueInfo, (P_QUE_ENTRY_T) prCmdInfo);

			/* TRUE: means we have already handled it in the function */
			return TRUE;
		}

		/* no memory, why assert ? can skip the packet ? */
		ASSERT(0);
		return FALSE;
	}
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
VOID wlanSecurityFrameTxDone(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	if (prCmdInfo->eNetworkType == NETWORK_TYPE_AIS_INDEX &&
	    prAdapter->rWifiVar.rAisSpecificBssInfo.fgCounterMeasure) {

		/* AIS counter measure so change RSN FSM to SEND_DEAUTH state */
		P_STA_RECORD_T prSta = cnmGetStaRecByIndex(prAdapter, prCmdInfo->ucStaRecIndex);

		if (prSta) {
			kalMsleep(10);
			secFsmEventEapolTxDone(prAdapter, prSta, TX_RESULT_SUCCESS);
		}
	}

	/* free the packet */
	kalSecurityFrameSendComplete(prAdapter->prGlueInfo, prCmdInfo->prPacket, WLAN_STATUS_SUCCESS);
	DBGLOG(TX, TRACE, "Security frame tx done, SeqNum: %d\n", prCmdInfo->ucCmdSeqNum);
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
VOID wlanSecurityFrameTxTimeout(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo)
{
	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	/* free the packet */
	kalSecurityFrameSendComplete(prAdapter->prGlueInfo, prCmdInfo->prPacket, WLAN_STATUS_FAILURE);
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
VOID wlanClearScanningResult(IN P_ADAPTER_T prAdapter)
{
	BOOLEAN fgKeepCurrOne = FALSE;
	UINT_32 i;

	ASSERT(prAdapter);

	/* clear scanning result except current one */
	/* copy current one to prAdapter->rWlanInfo.arScanResult[0] */
	if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) == PARAM_MEDIA_STATE_CONNECTED) {
		for (i = 0; i < prAdapter->rWlanInfo.u4ScanResultNum; i++) {

			if (EQUAL_MAC_ADDR(prAdapter->rWlanInfo.rCurrBssId.arMacAddress,
					   prAdapter->rWlanInfo.arScanResult[i].arMacAddress)) {
				fgKeepCurrOne = TRUE;

				if (i != 0) {
					/* copy structure */
					kalMemCopy(&(prAdapter->rWlanInfo.arScanResult[0]),
						   &(prAdapter->rWlanInfo.arScanResult[i]),
						   OFFSET_OF(PARAM_BSSID_EX_T, aucIEs));
				}

				if (prAdapter->rWlanInfo.arScanResult[i].u4IELength > 0) {
					if (prAdapter->rWlanInfo.apucScanResultIEs[i] !=
					    &(prAdapter->rWlanInfo.aucScanIEBuf[0])) {
						/* move IEs to head */
						kalMemCopy(prAdapter->rWlanInfo.aucScanIEBuf,
							   prAdapter->rWlanInfo.apucScanResultIEs[i],
							   prAdapter->rWlanInfo.arScanResult[i].u4IELength);
					}
					/* modify IE pointer */
					prAdapter->rWlanInfo.apucScanResultIEs[0] =
					    &(prAdapter->rWlanInfo.aucScanIEBuf[0]);
				} else {
					prAdapter->rWlanInfo.apucScanResultIEs[0] = NULL;
				}

				break;
			}
		}
	}

	if (fgKeepCurrOne == TRUE) {
		prAdapter->rWlanInfo.u4ScanResultNum = 1;
		prAdapter->rWlanInfo.u4ScanIEBufferUsage = ALIGN_4(prAdapter->rWlanInfo.arScanResult[0].u4IELength);
	} else {
		prAdapter->rWlanInfo.u4ScanResultNum = 0;
		prAdapter->rWlanInfo.u4ScanIEBufferUsage = 0;
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
VOID wlanClearBssInScanningResult(IN P_ADAPTER_T prAdapter, IN PUINT_8 arBSSID)
{
	UINT_32 i, j, u4IELength = 0, u4IEMoveLength;
	PUINT_8 pucIEPtr;

	ASSERT(prAdapter);

	/* clear the scanning result for arBSSID */
	i = 0;
	while (1) {
		if (i >= prAdapter->rWlanInfo.u4ScanResultNum)
			break;

		if (EQUAL_MAC_ADDR(arBSSID, prAdapter->rWlanInfo.arScanResult[i].arMacAddress)) {

			/* backup current IE length */
			u4IELength = ALIGN_4(prAdapter->rWlanInfo.arScanResult[i].u4IELength);
			pucIEPtr = prAdapter->rWlanInfo.apucScanResultIEs[i];

			/* removed from middle */
			for (j = i + 1; j < prAdapter->rWlanInfo.u4ScanResultNum; j++) {
				kalMemCopy(&(prAdapter->rWlanInfo.arScanResult[j - 1]),
					   &(prAdapter->rWlanInfo.arScanResult[j]),
					   OFFSET_OF(PARAM_BSSID_EX_T, aucIEs));

				prAdapter->rWlanInfo.apucScanResultIEs[j - 1] =
				    prAdapter->rWlanInfo.apucScanResultIEs[j];
			}

			prAdapter->rWlanInfo.u4ScanResultNum--;

			/* remove IE buffer if needed := move rest of IE buffer */
			if (u4IELength > 0) {
				u4IEMoveLength = prAdapter->rWlanInfo.u4ScanIEBufferUsage -
				    (((ULONG) pucIEPtr) + (ULONG) u4IELength -
				     ((ULONG) (&(prAdapter->rWlanInfo.aucScanIEBuf[0]))));

				kalMemCopy(pucIEPtr, pucIEPtr + u4IELength, u4IEMoveLength);

				prAdapter->rWlanInfo.u4ScanIEBufferUsage -= u4IELength;

				/* correction of pointers to IE buffer */
				for (j = 0; j < prAdapter->rWlanInfo.u4ScanResultNum; j++) {
					if (prAdapter->rWlanInfo.apucScanResultIEs[j] > pucIEPtr) {
						prAdapter->rWlanInfo.apucScanResultIEs[j] =
						    (PUINT_8) ((ULONG) (prAdapter->rWlanInfo.apucScanResultIEs[j]) -
							       u4IELength);
					}
				}
			}
		}

		i++;
	}

}

#if CFG_TEST_WIFI_DIRECT_GO
VOID wlanEnableP2pFunction(IN P_ADAPTER_T prAdapter)
{
#if 0
	P_MSG_P2P_FUNCTION_SWITCH_T prMsgFuncSwitch = (P_MSG_P2P_FUNCTION_SWITCH_T) NULL;

	prMsgFuncSwitch =
	    (P_MSG_P2P_FUNCTION_SWITCH_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_P2P_FUNCTION_SWITCH_T));
	if (!prMsgFuncSwitch) {
		ASSERT(FALSE);
		return;
	}

	prMsgFuncSwitch->rMsgHdr.eMsgId = MID_MNY_P2P_FUN_SWITCH;
	prMsgFuncSwitch->fgIsFuncOn = TRUE;

	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prMsgFuncSwitch, MSG_SEND_METHOD_BUF);
#endif

}

VOID wlanEnableATGO(IN P_ADAPTER_T prAdapter)
{

	P_MSG_P2P_CONNECTION_REQUEST_T prMsgConnReq = (P_MSG_P2P_CONNECTION_REQUEST_T) NULL;
	UINT_8 aucTargetDeviceID[MAC_ADDR_LEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

	prMsgConnReq =
	    (P_MSG_P2P_CONNECTION_REQUEST_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_P2P_CONNECTION_REQUEST_T));
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

	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prMsgConnReq, MSG_SEND_METHOD_BUF);

}
#endif

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to retrieve permanent address from firmware
*
* @param prAdapter      Pointer of Adapter Data Structure
*
* @return WLAN_STATUS_SUCCESS
*         WLAN_STATUS_FAILURE
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanQueryPermanentAddress(IN P_ADAPTER_T prAdapter)
{
	UINT_8 ucCmdSeqNum;
	P_CMD_INFO_T prCmdInfo;
	P_WIFI_CMD_T prWifiCmd;
	UINT_32 u4RxPktLength;
	UINT_8 aucBuffer[sizeof(WIFI_EVENT_T) + sizeof(EVENT_BASIC_CONFIG)];
	P_HIF_RX_HEADER_T prHifRxHdr;
	P_WIFI_EVENT_T prEvent;
	P_EVENT_BASIC_CONFIG prEventBasicConfig;

	ASSERT(prAdapter);

	DEBUGFUNC("wlanQueryPermanentAddress");

	/* 1. Allocate CMD Info Packet and its Buffer */
	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, CMD_HDR_SIZE + sizeof(CMD_BASIC_CONFIG));
	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}
	/* increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

	/* compose CMD_BUILD_CONNECTION cmd pkt */
	prCmdInfo->eCmdType = COMMAND_TYPE_GENERAL_IOCTL;
	prCmdInfo->u2InfoBufLen = CMD_HDR_SIZE + sizeof(CMD_BASIC_CONFIG);
	prCmdInfo->pfCmdDoneHandler = NULL;
	prCmdInfo->fgIsOid = FALSE;
	prCmdInfo->ucCID = CMD_ID_BASIC_CONFIG;
	prCmdInfo->fgSetQuery = FALSE;
	prCmdInfo->fgNeedResp = TRUE;
	prCmdInfo->fgDriverDomainMCR = FALSE;
	prCmdInfo->ucCmdSeqNum = ucCmdSeqNum;
	prCmdInfo->u4SetInfoLen = sizeof(CMD_BASIC_CONFIG);

	/* Setup WIFI_CMD_T */
	prWifiCmd = (P_WIFI_CMD_T) (prCmdInfo->pucInfoBuffer);
	prWifiCmd->u2TxByteCount_UserPriority = prCmdInfo->u2InfoBufLen;
	prWifiCmd->ucCID = prCmdInfo->ucCID;
	prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
	prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;

	/* send the command */
	wlanSendCommand(prAdapter, prCmdInfo);
	cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

	/* wait for response */
	if (nicRxWaitResponse(prAdapter,
				1,
				aucBuffer,
				sizeof(WIFI_EVENT_T) + sizeof(EVENT_BASIC_CONFIG),	/* 8B + 12B */
				&u4RxPktLength) != WLAN_STATUS_SUCCESS)
		return WLAN_STATUS_FAILURE;
	/* header checking .. */
	prHifRxHdr = (P_HIF_RX_HEADER_T) aucBuffer;
	if ((prHifRxHdr->u2PacketType & HIF_RX_HDR_PACKET_TYPE_MASK) != HIF_RX_PKT_TYPE_EVENT)
		return WLAN_STATUS_FAILURE;

	prEvent = (P_WIFI_EVENT_T) aucBuffer;
	if (prEvent->ucEID != EVENT_ID_BASIC_CONFIG)
		return WLAN_STATUS_FAILURE;

	prEventBasicConfig = (P_EVENT_BASIC_CONFIG) (prEvent->aucBuffer);

	COPY_MAC_ADDR(prAdapter->rWifiVar.aucPermanentAddress, &(prEventBasicConfig->rMyMacAddr));
	COPY_MAC_ADDR(prAdapter->rWifiVar.aucMacAddress, &(prEventBasicConfig->rMyMacAddr));

	return WLAN_STATUS_SUCCESS;
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
WLAN_STATUS wlanQueryNicCapability(IN P_ADAPTER_T prAdapter)
{
	UINT_8 ucCmdSeqNum;
	P_CMD_INFO_T prCmdInfo;
	P_WIFI_CMD_T prWifiCmd;
	UINT_32 u4RxPktLength;
	UINT_32 u4FwIDVersion = 0;
	UINT_8 aucBuffer[sizeof(WIFI_EVENT_T) + sizeof(EVENT_NIC_CAPABILITY)];
	P_HIF_RX_HEADER_T prHifRxHdr;
	P_WIFI_EVENT_T prEvent;
	P_EVENT_NIC_CAPABILITY prEventNicCapability;

	ASSERT(prAdapter);

	DEBUGFUNC("wlanQueryNicCapability");

	/* 1. Allocate CMD Info Packet and its Buffer */
	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, CMD_HDR_SIZE + sizeof(EVENT_NIC_CAPABILITY));
	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}
	/* increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

	/* compose CMD_BUILD_CONNECTION cmd pkt */
	prCmdInfo->eCmdType = COMMAND_TYPE_GENERAL_IOCTL;
	prCmdInfo->u2InfoBufLen = CMD_HDR_SIZE + sizeof(EVENT_NIC_CAPABILITY);
	prCmdInfo->pfCmdDoneHandler = NULL;
	prCmdInfo->fgIsOid = FALSE;
	prCmdInfo->ucCID = CMD_ID_GET_NIC_CAPABILITY;
	prCmdInfo->fgSetQuery = FALSE;
	prCmdInfo->fgNeedResp = TRUE;
	prCmdInfo->fgDriverDomainMCR = FALSE;
	prCmdInfo->ucCmdSeqNum = ucCmdSeqNum;
	prCmdInfo->u4SetInfoLen = 0;

	/* Setup WIFI_CMD_T */
	prWifiCmd = (P_WIFI_CMD_T) (prCmdInfo->pucInfoBuffer);
	prWifiCmd->u2TxByteCount_UserPriority = prCmdInfo->u2InfoBufLen;
	prWifiCmd->ucCID = prCmdInfo->ucCID;
	prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
	prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;

	/* send the command */
	wlanSendCommand(prAdapter, prCmdInfo);
	cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

	/* wait for FW response */
	if (nicRxWaitResponse(prAdapter,
			      1,
			      aucBuffer,
			      sizeof(WIFI_EVENT_T) + sizeof(EVENT_NIC_CAPABILITY),
			      &u4RxPktLength) != WLAN_STATUS_SUCCESS)
		return WLAN_STATUS_FAILURE;
	/* header checking .. */
	prHifRxHdr = (P_HIF_RX_HEADER_T) aucBuffer;
	if ((prHifRxHdr->u2PacketType & HIF_RX_HDR_PACKET_TYPE_MASK) != HIF_RX_PKT_TYPE_EVENT)
		return WLAN_STATUS_FAILURE;

	prEvent = (P_WIFI_EVENT_T) aucBuffer;
	if (prEvent->ucEID != EVENT_ID_NIC_CAPABILITY)
		return WLAN_STATUS_FAILURE;

	prEventNicCapability = (P_EVENT_NIC_CAPABILITY) (prEvent->aucBuffer);

	prAdapter->rVerInfo.u2FwProductID = prEventNicCapability->u2ProductID;
	prAdapter->rVerInfo.u2FwOwnVersion = prEventNicCapability->u2FwVersion;
	prAdapter->rVerInfo.u2FwPeerVersion = prEventNicCapability->u2DriverVersion;
	prAdapter->rVerInfo.u2FwOwnVersionExtend = prEventNicCapability->aucReserved[0];

	prAdapter->fgIsHw5GBandDisabled = (BOOLEAN) prEventNicCapability->ucHw5GBandDisabled;
	prAdapter->fgIsEepromUsed = (BOOLEAN) prEventNicCapability->ucEepromUsed;
	prAdapter->fgIsEfuseValid = (BOOLEAN) prEventNicCapability->ucEfuseValid;
	prAdapter->fgIsEmbbededMacAddrValid = (BOOLEAN) prEventNicCapability->ucMacAddrValid;

	u4FwIDVersion = (prAdapter->rVerInfo.u2FwProductID << 16) | (prAdapter->rVerInfo.u2FwOwnVersion);
	mtk_wcn_wmt_set_wifi_ver(u4FwIDVersion);

	DBGLOG(INIT, INFO, "<wifi> ProductID: 0x%x FwVer: 0x%x.%x DriVer:%s\n"
		, prAdapter->rVerInfo.u2FwProductID
		, prAdapter->rVerInfo.u2FwOwnVersion
		, prAdapter->rVerInfo.u2FwOwnVersionExtend
		, WIFI_DRIVER_VERSION);

#if (CFG_SUPPORT_TDLS == 1)
	if (prEventNicCapability->ucFeatureSet & (1 << FEATURE_SET_OFFSET_TDLS))
		prAdapter->fgTdlsIsSup = TRUE;
	DBGLOG(TDLS, TRACE, "<wifi> support flag: 0x%x\n", prEventNicCapability->ucFeatureSet);
#else
	prAdapter->fgTdlsIsSup = 0;
#endif /* CFG_SUPPORT_TDLS */

	if (!(prEventNicCapability->ucFeatureSet & (1 << FEATURE_SET_OFFSET_5G_SUPPORT)))
		prAdapter->fgEnable5GBand = FALSE;	/* firmware does not support */

#if CFG_ENABLE_CAL_LOG
	DBGLOG(INIT, LOUD, " RF CAL FAIL  = (%d),BB CAL FAIL  = (%d)\n",
			    prEventNicCapability->ucRfCalFail, prEventNicCapability->ucBbCalFail);
#endif
	return WLAN_STATUS_SUCCESS;
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
WLAN_STATUS wlanQueryDebugCode(IN P_ADAPTER_T prAdapter)
{
	UINT_8 ucCmdSeqNum;
	P_CMD_INFO_T prCmdInfo;
	P_WIFI_CMD_T prWifiCmd;

	ASSERT(prAdapter);

	DEBUGFUNC("wlanQueryDebugCode");

	/* 1. Allocate CMD Info Packet and its Buffer */
	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, CMD_HDR_SIZE);
	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}
	/* increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

	/* compose CMD_BUILD_CONNECTION cmd pkt */
	prCmdInfo->eCmdType = COMMAND_TYPE_GENERAL_IOCTL;
	prCmdInfo->u2InfoBufLen = CMD_HDR_SIZE;
	prCmdInfo->pfCmdDoneHandler = NULL;
	prCmdInfo->fgIsOid = FALSE;
	prCmdInfo->ucCID = CMD_ID_GET_DEBUG_CODE;
	prCmdInfo->fgSetQuery = FALSE;
	prCmdInfo->fgNeedResp = FALSE;
	prCmdInfo->fgDriverDomainMCR = FALSE;
	prCmdInfo->ucCmdSeqNum = ucCmdSeqNum;
	prCmdInfo->u4SetInfoLen = 0;

	/* Setup WIFI_CMD_T */
	prWifiCmd = (P_WIFI_CMD_T) (prCmdInfo->pucInfoBuffer);
	prWifiCmd->u2TxByteCount_UserPriority = prCmdInfo->u2InfoBufLen;
	prWifiCmd->ucCID = prCmdInfo->ucCID;
	prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
	prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;

	/* send the command */
	wlanSendCommand(prAdapter, prCmdInfo);
	cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to retrieve compiler flag from firmware
*
* @param prAdapter      Pointer of Adapter Data Structure
*
* @return WLAN_STATUS_SUCCESS
*         WLAN_STATUS_FAILURE
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanQueryCompileFlag(IN P_ADAPTER_T prAdapter, IN UINT_32 u4QueryID, OUT PUINT_32 pu4CompilerFlag)
{
	UINT_8 ucCmdSeqNum;
	P_CMD_INFO_T prCmdInfo;
	P_WIFI_CMD_T prWifiCmd;
	UINT_32 u4RxPktLength;
	UINT_8 aucBuffer[sizeof(WIFI_EVENT_T) + sizeof(CMD_SW_DBG_CTRL_T)];
	P_HIF_RX_HEADER_T prHifRxHdr;
	P_WIFI_EVENT_T prEvent;
	P_CMD_SW_DBG_CTRL_T prCmdNicCompileFlag, prEventNicCompileFlag;

	ASSERT(prAdapter);

	DEBUGFUNC(__func__);

	/* 1. Allocate CMD Info Packet and its Buffer */
	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, CMD_HDR_SIZE + sizeof(CMD_SW_DBG_CTRL_T));
	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}
	/* increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

	/* compose CMD_BUILD_CONNECTION cmd pkt */
	prCmdInfo->eCmdType = COMMAND_TYPE_GENERAL_IOCTL;
	prCmdInfo->u2InfoBufLen = CMD_HDR_SIZE + sizeof(CMD_SW_DBG_CTRL_T);
	prCmdInfo->pfCmdDoneHandler = NULL;
	prCmdInfo->fgIsOid = FALSE;
	prCmdInfo->ucCID = CMD_ID_SW_DBG_CTRL;
	prCmdInfo->fgSetQuery = FALSE;
	prCmdInfo->fgNeedResp = TRUE;
	prCmdInfo->fgDriverDomainMCR = FALSE;
	prCmdInfo->ucCmdSeqNum = ucCmdSeqNum;
	prCmdInfo->u4SetInfoLen = 0;

	/* Setup WIFI_CMD_T */
	prWifiCmd = (P_WIFI_CMD_T) (prCmdInfo->pucInfoBuffer);
	prWifiCmd->u2TxByteCount_UserPriority = prCmdInfo->u2InfoBufLen;
	prWifiCmd->ucCID = prCmdInfo->ucCID;
	prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
	prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;

	/* Fill up SW CR */
	prCmdNicCompileFlag = (P_CMD_SW_DBG_CTRL_T) (prWifiCmd->aucBuffer);

	prCmdNicCompileFlag->u4Id = u4QueryID;

	wlanSendCommand(prAdapter, prCmdInfo);
	cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

	if (nicRxWaitResponse(prAdapter,
			      1,
			      aucBuffer,
			      sizeof(WIFI_EVENT_T) + sizeof(CMD_SW_DBG_CTRL_T),
			      &u4RxPktLength) != WLAN_STATUS_SUCCESS)
		return WLAN_STATUS_FAILURE;
	/* header checking .. */
	prHifRxHdr = (P_HIF_RX_HEADER_T) aucBuffer;
	if ((prHifRxHdr->u2PacketType & HIF_RX_HDR_PACKET_TYPE_MASK) != HIF_RX_PKT_TYPE_EVENT)
		return WLAN_STATUS_FAILURE;

	prEvent = (P_WIFI_EVENT_T) aucBuffer;
	if (prEvent->ucEID != EVENT_ID_SW_DBG_CTRL)
		return WLAN_STATUS_FAILURE;

	prEventNicCompileFlag = (P_CMD_SW_DBG_CTRL_T) (prEvent->aucBuffer);

	*pu4CompilerFlag = prEventNicCompileFlag->u4Data;

	return WLAN_STATUS_SUCCESS;
}

WLAN_STATUS wlanQueryCompileFlags(IN P_ADAPTER_T prAdapter)
{
	wlanQueryCompileFlag(prAdapter, 0xA0240000, &prAdapter->u4FwCompileFlag0);
	wlanQueryCompileFlag(prAdapter, 0xA0240001, &prAdapter->u4FwCompileFlag1);

	DBGLOG(INIT, TRACE,
	       "Compile Flags: 0x%08x 0x%08x\n", prAdapter->u4FwCompileFlag0, prAdapter->u4FwCompileFlag1);

	return WLAN_STATUS_SUCCESS;
}

#if defined(MT6628)
static INT_32 wlanChangeCodeWord(INT_32 au4Input)
{

	UINT_16 i;
#if TXPWR_USE_PDSLOPE
	CODE_MAPPING_T arCodeTable[] = {
		{0X100, -40},
		{0X104, -35},
		{0X128, -30},
		{0X14C, -25},
		{0X170, -20},
		{0X194, -15},
		{0X1B8, -10},
		{0X1DC, -5},
		{0, 0},
		{0X24, 5},
		{0X48, 10},
		{0X6C, 15},
		{0X90, 20},
		{0XB4, 25},
		{0XD8, 30},
		{0XFC, 35},
		{0XFF, 40},

	};
#else
	CODE_MAPPING_T arCodeTable[] = {
		{0X100, 0x80},
		{0X104, 0x80},
		{0X128, 0x80},
		{0X14C, 0x80},
		{0X170, 0x80},
		{0X194, 0x94},
		{0X1B8, 0XB8},
		{0X1DC, 0xDC},
		{0, 0},
		{0X24, 0x24},
		{0X48, 0x48},
		{0X6C, 0x6c},
		{0X90, 0x7F},
		{0XB4, 0x7F},
		{0XD8, 0x7F},
		{0XFC, 0x7F},
		{0XFF, 0x7F},

	};
#endif

	for (i = 0; i < sizeof(arCodeTable) / sizeof(CODE_MAPPING_T); i++) {

		if (arCodeTable[i].u4RegisterValue == au4Input)
			return arCodeTable[i].i4TxpowerOffset;
	}

	return 0;
}
#endif

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
WLAN_STATUS wlanQueryPdMcr(IN P_ADAPTER_T prAdapter, P_PARAM_MCR_RW_STRUCT_T prMcrRdInfo)
{
	UINT_8 ucCmdSeqNum;
	P_CMD_INFO_T prCmdInfo;
	P_WIFI_CMD_T prWifiCmd;
	UINT_32 u4RxPktLength;
	UINT_8 aucBuffer[sizeof(WIFI_EVENT_T) + sizeof(CMD_ACCESS_REG)];
	P_HIF_RX_HEADER_T prHifRxHdr;
	P_WIFI_EVENT_T prEvent;
	P_CMD_ACCESS_REG prCmdMcrQuery;

	ASSERT(prAdapter);

	/* 1. Allocate CMD Info Packet and its Buffer */
	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, CMD_HDR_SIZE + sizeof(CMD_ACCESS_REG));

	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}
	/* increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

	/* compose CMD_BUILD_CONNECTION cmd pkt */
	prCmdInfo->eCmdType = COMMAND_TYPE_GENERAL_IOCTL;
	prCmdInfo->u2InfoBufLen = (UINT_16) (CMD_HDR_SIZE + sizeof(CMD_ACCESS_REG));
	prCmdInfo->pfCmdDoneHandler = NULL;
	prCmdInfo->pfCmdTimeoutHandler = nicOidCmdTimeoutCommon;
	prCmdInfo->fgIsOid = FALSE;
	prCmdInfo->ucCID = CMD_ID_ACCESS_REG;
	prCmdInfo->fgSetQuery = FALSE;
	prCmdInfo->fgNeedResp = TRUE;
	prCmdInfo->fgDriverDomainMCR = FALSE;
	prCmdInfo->ucCmdSeqNum = ucCmdSeqNum;
	prCmdInfo->u4SetInfoLen = sizeof(CMD_ACCESS_REG);

	/* Setup WIFI_CMD_T */
	prWifiCmd = (P_WIFI_CMD_T) (prCmdInfo->pucInfoBuffer);
	prWifiCmd->u2TxByteCount_UserPriority = prCmdInfo->u2InfoBufLen;
	prWifiCmd->ucCID = prCmdInfo->ucCID;
	prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
	prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;
	kalMemCopy(prWifiCmd->aucBuffer, prMcrRdInfo, sizeof(CMD_ACCESS_REG));

	wlanSendCommand(prAdapter, prCmdInfo);
	cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

	if (nicRxWaitResponse(prAdapter,
			      1,
			      aucBuffer,
			      sizeof(WIFI_EVENT_T) + sizeof(CMD_ACCESS_REG), &u4RxPktLength) != WLAN_STATUS_SUCCESS)
		return WLAN_STATUS_FAILURE;
	/* header checking .. */
	prHifRxHdr = (P_HIF_RX_HEADER_T) aucBuffer;
	if ((prHifRxHdr->u2PacketType & HIF_RX_HDR_PACKET_TYPE_MASK) != HIF_RX_PKT_TYPE_EVENT)
		return WLAN_STATUS_FAILURE;

	prEvent = (P_WIFI_EVENT_T) aucBuffer;

	if (prEvent->ucEID != EVENT_ID_ACCESS_REG)
		return WLAN_STATUS_FAILURE;

	prCmdMcrQuery = (P_CMD_ACCESS_REG) (prEvent->aucBuffer);
	prMcrRdInfo->u4McrOffset = prCmdMcrQuery->u4Address;
	prMcrRdInfo->u4McrData = prCmdMcrQuery->u4Data;

	return WLAN_STATUS_SUCCESS;
}

static INT_32 wlanIntRound(INT_32 au4Input)
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

static INT_32 wlanCal6628EfuseForm(IN P_ADAPTER_T prAdapter, INT_32 au4Input)
{

	PARAM_MCR_RW_STRUCT_T rMcrRdInfo;
	INT_32 au4PdSlope, au4TxPwrOffset, au4TxPwrOffset_Round;
	INT_8 auTxPwrOffset_Round;

	rMcrRdInfo.u4McrOffset = 0x60205c68;
	rMcrRdInfo.u4McrData = 0;
	au4TxPwrOffset = au4Input;
	wlanQueryPdMcr(prAdapter, &rMcrRdInfo);

	au4PdSlope = (rMcrRdInfo.u4McrData) & BITS(0, 6);
	au4TxPwrOffset_Round = wlanIntRound((au4TxPwrOffset * au4PdSlope)) / 10;

	au4TxPwrOffset_Round = -au4TxPwrOffset_Round;

	if (au4TxPwrOffset_Round < -128)
		au4TxPwrOffset_Round = 128;
	else if (au4TxPwrOffset_Round < 0)
		au4TxPwrOffset_Round += 256;
	else if (au4TxPwrOffset_Round > 127)
		au4TxPwrOffset_Round = 127;

	auTxPwrOffset_Round = (UINT8) au4TxPwrOffset_Round;

	return au4TxPwrOffset_Round;
}

#endif

#if defined(MT6628)
static VOID wlanChangeNvram6620to6628(PUINT_8 pucEFUSE)
{

#define EFUSE_CH_OFFSET1_L_MASK_6620         BITS(0, 8)
#define EFUSE_CH_OFFSET1_L_SHIFT_6620        0
#define EFUSE_CH_OFFSET1_M_MASK_6620         BITS(9, 17)
#define EFUSE_CH_OFFSET1_M_SHIFT_6620        9
#define EFUSE_CH_OFFSET1_H_MASK_6620         BITS(18, 26)
#define EFUSE_CH_OFFSET1_H_SHIFT_6620        18
#define EFUSE_CH_OFFSET1_VLD_MASK_6620       BIT(27)
#define EFUSE_CH_OFFSET1_VLD_SHIFT_6620      27

#define EFUSE_CH_OFFSET1_L_MASK_5931         BITS(0, 7)
#define EFUSE_CH_OFFSET1_L_SHIFT_5931        0
#define EFUSE_CH_OFFSET1_M_MASK_5931         BITS(8, 15)
#define EFUSE_CH_OFFSET1_M_SHIFT_5931        8
#define EFUSE_CH_OFFSET1_H_MASK_5931         BITS(16, 23)
#define EFUSE_CH_OFFSET1_H_SHIFT_5931        16
#define EFUSE_CH_OFFSET1_VLD_MASK_5931       BIT(24)
#define EFUSE_CH_OFFSET1_VLD_SHIFT_5931      24
#define EFUSE_ALL_CH_OFFSET1_MASK_5931       BITS(25, 27)
#define EFUSE_ALL_CH_OFFSET1_SHIFT_5931      25

	INT_32 au4ChOffset;
	INT_16 au2ChOffsetL, au2ChOffsetM, au2ChOffsetH;

	au4ChOffset = *(UINT_32 *) (pucEFUSE + 72);

	if ((au4ChOffset & EFUSE_CH_OFFSET1_VLD_MASK_6620) && ((*(UINT_32 *) (pucEFUSE + 28)) == 0)) {

		au2ChOffsetL = ((au4ChOffset & EFUSE_CH_OFFSET1_L_MASK_6620) >> EFUSE_CH_OFFSET1_L_SHIFT_6620);

		au2ChOffsetM = ((au4ChOffset & EFUSE_CH_OFFSET1_M_MASK_6620) >> EFUSE_CH_OFFSET1_M_SHIFT_6620);

		au2ChOffsetH = ((au4ChOffset & EFUSE_CH_OFFSET1_H_MASK_6620) >> EFUSE_CH_OFFSET1_H_SHIFT_6620);

		au2ChOffsetL = wlanChangeCodeWord(au2ChOffsetL);
		au2ChOffsetM = wlanChangeCodeWord(au2ChOffsetM);
		au2ChOffsetH = wlanChangeCodeWord(au2ChOffsetH);

		au4ChOffset = 0;
		au4ChOffset |= *(UINT_32 *) (pucEFUSE + 72)
		    >> (EFUSE_CH_OFFSET1_VLD_SHIFT_6620 -
			EFUSE_CH_OFFSET1_VLD_SHIFT_5931) & EFUSE_CH_OFFSET1_VLD_MASK_5931;

		au4ChOffset |=
		    ((((UINT_32) au2ChOffsetL) << EFUSE_CH_OFFSET1_L_SHIFT_5931) & EFUSE_CH_OFFSET1_L_MASK_5931);
		au4ChOffset |=
		    ((((UINT_32) au2ChOffsetM) << EFUSE_CH_OFFSET1_M_SHIFT_5931) & EFUSE_CH_OFFSET1_M_MASK_5931);
		au4ChOffset |=
		    ((((UINT_32) au2ChOffsetH) << EFUSE_CH_OFFSET1_H_SHIFT_5931) & EFUSE_CH_OFFSET1_H_MASK_5931);

		*((INT_32 *) ((pucEFUSE + 28))) = au4ChOffset;

	}

}
#endif

ENUM_BAND_EDGE_CERT_T getBandEdgeCert(P_ADAPTER_T prAdapter)
{
	P_DOMAIN_INFO_ENTRY prDomainInfo;
	P_DOMAIN_SUBBAND_INFO prSubband;
	UINT32 i;

	prDomainInfo = rlmDomainGetDomainInfo(prAdapter);
	ASSERT(prDomainInfo);

	for (i = 0; i < MAX_SUBBAND_NUM; i++) {
		prSubband = &prDomainInfo->rSubBand[i];

		if (prSubband->ucBand == BAND_2G4) {
			if (prSubband->ucFirstChannelNum == 1) {
				if (prSubband->ucNumChannels == 13)
					return BAND_EDGE_CERT_KCC;
				else
					return BAND_EDGE_CERT_FCC;
			}
		}
	}
	return BAND_EDGE_CERT_FCC;
}

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
WLAN_STATUS wlanLoadManufactureData(IN P_ADAPTER_T prAdapter, IN P_REG_INFO_T prRegInfo)
{
#if CFG_SUPPORT_RDD_TEST_MODE
	CMD_RDD_CH_T rRddParam;
#endif
#if CFG_SUPPORT_FCC_DYNAMIC_TX_PWR_ADJUST
	CMD_FCC_TX_PWR_ADJUST FccTxPwrAdjust = {0x00};
#endif
	CMD_BAND_SUPPORT_T rCmdBandSupport;

	UINT8 uc_NVRAM[EXTEND_NVRAM_SIZE] = {0x0};
	UINT16 NVRAMSize = 0;

	ASSERT(prAdapter);

	/* 1. Version Check */
	kalGetConfigurationVersion(prAdapter->prGlueInfo,
				   &(prAdapter->rVerInfo.u2Part1CfgOwnVersion),
				   &(prAdapter->rVerInfo.u2Part1CfgPeerVersion),
				   &(prAdapter->rVerInfo.u2Part2CfgOwnVersion),
				   &(prAdapter->rVerInfo.u2Part2CfgPeerVersion));

#if (CFG_SW_NVRAM_VERSION_CHECK == 1)
	if (prAdapter->rVerInfo.u2Part1CfgPeerVersion > CFG_DRV_OWN_VERSION
	    || prAdapter->rVerInfo.u2Part2CfgPeerVersion > CFG_DRV_OWN_VERSION
	    || prAdapter->rVerInfo.u2Part1CfgOwnVersion <= CFG_DRV_PEER_VERSION
	    || prAdapter->rVerInfo.u2Part2CfgOwnVersion <= CFG_DRV_PEER_VERSION) {
		return WLAN_STATUS_FAILURE;
	}
#endif

	/* Only when NVRAM size is EXTEND_NVRAM_SIZE bytes, send the whole NVRAM data to FW */
	if (kalCfgDataRead16(prAdapter->prGlueInfo,
		OFFSET_OF(WIFI_CFG_PARAM_STRUCT, u2SizeOfNvram),
		(PUINT_16)&NVRAMSize) == TRUE) {
		DBGLOG(INIT, INFO, "current NVRAMSize :%d and extend Size:%d\n"
			, NVRAMSize, EXTEND_NVRAM_SIZE);
		if (NVRAMSize >= EXTEND_NVRAM_SIZE) {
			if (kalCfgDataRead(prAdapter->prGlueInfo,
			0,
			sizeof(UINT_8)*EXTEND_NVRAM_SIZE,
			(PUINT_16)&uc_NVRAM[0]) == TRUE)

				wlanSendSetQueryCmd(prAdapter,
						CMD_ID_SET_NVRAM_SETTINGS,
						TRUE,
						FALSE,
						FALSE, NULL, NULL, sizeof(UINT_8) * EXTEND_NVRAM_SIZE,
						(PUINT_8)(&uc_NVRAM[0]), NULL, 0);
			else
				DBGLOG(INIT, WARN, "Nvram read fail!\n");

			/* MT6620 E1/E2 would be ignored directly */
		}
	} else
		DBGLOG(INIT, WARN, "u2SizeOfNvram read fail!\n");

	if (prAdapter->rVerInfo.u2Part1CfgOwnVersion == 0x0001) {
		prRegInfo->ucTxPwrValid = 1;
	} else {
		/* 2. Load TX power gain parameters if valid */
		if (prRegInfo->ucTxPwrValid != 0) {
			/* send to F/W */
			nicUpdateTxPower(prAdapter, (P_CMD_TX_PWR_T) (&(prRegInfo->rTxPwr)));

#if CFG_SUPPORT_TX_POWER_BACK_OFF
			if (prRegInfo->fgRlmMitigatedPwrByChByMode)
				nicUpdateTxPowerOffset(prAdapter,
					(P_CMD_MITIGATED_PWR_OFFSET_T) (prRegInfo->arRlmMitigatedPwrByChByMode));
#endif
		}
	}

#if CFG_SUPPORT_FCC_DYNAMIC_TX_PWR_ADJUST
	/* Tx Power Adjust for FCC/CE Certification */
	FccTxPwrAdjust.fgFccTxPwrAdjust = 1;	/* 1:enable; 0:disable */
	FccTxPwrAdjust.Offset_CCK = 14;		/* drop 7dB */
	FccTxPwrAdjust.Offset_HT20 = 16;	/* drop 8dB */
	FccTxPwrAdjust.Offset_HT40 = 14;	/* drop 7dB*/
	FccTxPwrAdjust.Channel_CCK[0] = 11;	/* [0] for start channel */
	FccTxPwrAdjust.Channel_CCK[1] = 13;	/* [1] for ending channel */
	FccTxPwrAdjust.Channel_HT20[0] = 11;	/* [0] for start channel */
	FccTxPwrAdjust.Channel_HT20[1] = 13;	/* [1] for ending channel */
	FccTxPwrAdjust.Channel_HT40[0] = 7;	/* [0] for start channel,engineer mode ch9(2452) */
	FccTxPwrAdjust.Channel_HT40[1] = 9;	/* [1] for ending channel,engineer mode ch11(2462) */

	wlanSendSetQueryCmd(prAdapter,
			    CMD_ID_SET_FCC_TX_PWR_CERT,
			    TRUE,
			    FALSE,
			    FALSE, NULL, NULL, sizeof(CMD_FCC_TX_PWR_ADJUST), (PUINT_8) (&FccTxPwrAdjust), NULL, 0);

#endif

	/* 3. Check if needs to support 5GHz */
	/* if(prRegInfo->ucEnable5GBand) { // Frank workaround */
	if (1) {
		/* check if it is disabled by hardware */
		if (prAdapter->fgIsHw5GBandDisabled || prRegInfo->ucSupport5GBand == 0)
			prAdapter->fgEnable5GBand = FALSE;
		else
			prAdapter->fgEnable5GBand = TRUE;
	} else
		prAdapter->fgEnable5GBand = FALSE;

	/*
	 * DBGLOG(INIT, INFO, "NVRAM 5G Enable(%d) SW_En(%d) HW_Dis(%d)\n",
	 *     prRegInfo->ucEnable5GBand, prRegInfo->ucSupport5GBand, prAdapter->fgIsHw5GBandDisabled);
	 */
	DBGLOG(INIT, INFO, "HW_Dis(%d), TxPwrValid(%d)\n",
	       prAdapter->fgIsHw5GBandDisabled,
	       prRegInfo->ucTxPwrValid);
	/* 4. Send EFUSE data */
#if  defined(MT6628)
	wlanChangeNvram6620to6628(prRegInfo->aucEFUSE);
#endif

	wlanSendSetQueryCmd(prAdapter,
			    CMD_ID_SET_PHY_PARAM,
			    TRUE,
			    FALSE,
			    FALSE, NULL, NULL, sizeof(CMD_PHY_PARAM_T), (PUINT_8) (prRegInfo->aucEFUSE), NULL, 0);

#if CFG_SUPPORT_RDD_TEST_MODE
	rRddParam.ucRddTestMode = (UINT_8) prRegInfo->u4RddTestMode;
	rRddParam.ucRddShutCh = (UINT_8) prRegInfo->u4RddShutFreq;
	rRddParam.ucRddStartCh = (UINT_8) nicFreq2ChannelNum(prRegInfo->u4RddStartFreq);
	rRddParam.ucRddStopCh = (UINT_8) nicFreq2ChannelNum(prRegInfo->u4RddStopFreq);
	rRddParam.ucRddDfs = (UINT_8) prRegInfo->u4RddDfs;
	prAdapter->ucRddStatus = 0;
	nicUpdateRddTestMode(prAdapter, (P_CMD_RDD_CH_T) (&rRddParam));
#endif

	/* 5. Get 16-bits Country Code and Bandwidth */
	prAdapter->rWifiVar.rConnSettings.u2CountryCode =
	    (((UINT_16) prRegInfo->au2CountryCode[0]) << 8) | (((UINT_16) prRegInfo->au2CountryCode[1]) & BITS(0, 7));

	DBGLOG(INIT, INFO, "NVRAM 5G Enable(%d) SW_En(%d) HW_Dis(%d) CountryCode(0x%x 0x%x)\n",
		prRegInfo->ucEnable5GBand, prRegInfo->ucSupport5GBand, prAdapter->fgIsHw5GBandDisabled,
		prRegInfo->au2CountryCode[0], prRegInfo->au2CountryCode[1]);

#if 0  /* Bandwidth control will be controlled by GUI. 20110930
	* So ignore the setting from registry/NVRAM
	*/
	prAdapter->rWifiVar.rConnSettings.uc2G4BandwidthMode =
	    prRegInfo->uc2G4BwFixed20M ? CONFIG_BW_20M : CONFIG_BW_20_40M;
	prAdapter->rWifiVar.rConnSettings.uc5GBandwidthMode =
	    prRegInfo->uc5GBwFixed20M ? CONFIG_BW_20M : CONFIG_BW_20_40M;
#endif

	/* 6. Set domain and channel information to chip */
	rlmDomainSendCmd(prAdapter, FALSE);
	/* Update supported channel list in channel table */
	wlanUpdateChannelTable(prAdapter->prGlueInfo);

	/* 7. Set band edge tx power if available */
	if (prRegInfo->fg2G4BandEdgePwrUsed) {
		CMD_EDGE_TXPWR_LIMIT_T rCmdEdgeTxPwrLimit;

		rCmdEdgeTxPwrLimit.cBandEdgeMaxPwrCCK = prRegInfo->cBandEdgeMaxPwrCCK;
		rCmdEdgeTxPwrLimit.cBandEdgeMaxPwrOFDM20 = prRegInfo->cBandEdgeMaxPwrOFDM20;
		rCmdEdgeTxPwrLimit.cBandEdgeMaxPwrOFDM40 = prRegInfo->cBandEdgeMaxPwrOFDM40;
		rCmdEdgeTxPwrLimit.cBandEdgeCert = getBandEdgeCert(prAdapter);

		DBGLOG(INIT, TRACE, "NVRAM 2G Bandedge CCK(%d) HT20(%d)HT40(%d)\n",
		       rCmdEdgeTxPwrLimit.cBandEdgeMaxPwrCCK,
		       rCmdEdgeTxPwrLimit.cBandEdgeMaxPwrOFDM20, rCmdEdgeTxPwrLimit.cBandEdgeMaxPwrOFDM40);

		wlanSendSetQueryCmd(prAdapter,
				    CMD_ID_SET_EDGE_TXPWR_LIMIT,
				    TRUE,
				    FALSE,
				    FALSE,
				    NULL,
				    NULL, sizeof(CMD_EDGE_TXPWR_LIMIT_T), (PUINT_8)&rCmdEdgeTxPwrLimit, NULL, 0);
	}
	/* 8. set 5G band edge tx power if available (add for 6625) */
	if (prAdapter->fgEnable5GBand) {
#define NVRAM_5G_TX_BANDEDGE_VALID_OFFSET  10
#define NVRAM_5G_TX_BANDEDGE_OFDM20_OFFSET 11
#define NVRAM_5G_TX_BANDEDGE_OFDM40_OFFSET 12

		if (prRegInfo->aucEFUSE[NVRAM_5G_TX_BANDEDGE_VALID_OFFSET]) {
			CMD_EDGE_TXPWR_LIMIT_T rCmdEdgeTxPwrLimit;

			rCmdEdgeTxPwrLimit.cBandEdgeMaxPwrOFDM20
			    = prRegInfo->aucEFUSE[NVRAM_5G_TX_BANDEDGE_OFDM20_OFFSET];
			rCmdEdgeTxPwrLimit.cBandEdgeMaxPwrOFDM40
			    = prRegInfo->aucEFUSE[NVRAM_5G_TX_BANDEDGE_OFDM40_OFFSET];

			DBGLOG(INIT, TRACE, "NVRAM 5G Bandedge HT20(%d)HT40(%d)\n",
			       rCmdEdgeTxPwrLimit.cBandEdgeMaxPwrOFDM20, rCmdEdgeTxPwrLimit.cBandEdgeMaxPwrOFDM40);

			wlanSendSetQueryCmd(prAdapter,
					    CMD_ID_SET_5G_EDGE_TXPWR_LIMIT,
					    TRUE,
					    FALSE,
					    FALSE,
					    NULL,
					    NULL,
					    sizeof(CMD_EDGE_TXPWR_LIMIT_T), (PUINT_8)&rCmdEdgeTxPwrLimit, NULL, 0);
		}
	}
	/* 9. set RSSI compensation */
	/*
	 * DBGLOG(INIT, INFO, ("[frank] RSSI valid(%d) 2G(%d) 5G(%d)",
	 *     prRegInfo->fgRssiCompensationValidbit,
	 *     prRegInfo->uc2GRssiCompensation,
	 *     prRegInfo->uc5GRssiCompensation));
	 */
	if (prRegInfo->fgRssiCompensationValidbit) {
		CMD_RSSI_COMPENSATE_T rCmdRssiCompensate;

		rCmdRssiCompensate.uc2GRssiCompensation = prRegInfo->uc2GRssiCompensation;
		rCmdRssiCompensate.uc5GRssiCompensation = prRegInfo->uc5GRssiCompensation;

		DBGLOG(INIT, LOUD, "NVRAM RSSI Comp. 2G(%d)5G(%d)\n",
		       rCmdRssiCompensate.uc2GRssiCompensation, rCmdRssiCompensate.uc5GRssiCompensation);
		wlanSendSetQueryCmd(prAdapter,
				    CMD_ID_SET_RSSI_COMPENSATE,
				    TRUE,
				    FALSE,
				    FALSE,
				    NULL, NULL, sizeof(CMD_RSSI_COMPENSATE_T), (PUINT_8)&rCmdRssiCompensate, NULL, 0);
	}
	/* 10. notify FW Band Support 5G */

	rCmdBandSupport.uc5GBandSupport = prAdapter->fgEnable5GBand;
	DBGLOG(INIT, TRACE, "notify NVRAM 5G BandSupport %d\n", rCmdBandSupport.uc5GBandSupport);

	wlanSendSetQueryCmd(prAdapter,
			    CMD_ID_SET_BAND_SUPPORT,
			    TRUE,
			    FALSE,
			    FALSE,
			    NULL, NULL, sizeof(CMD_BAND_SUPPORT_T), (PUINT_8)&rCmdBandSupport, NULL, 0);

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
BOOLEAN wlanResetMediaStreamMode(IN P_ADAPTER_T prAdapter)
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
WLAN_STATUS wlanTimerTimeoutCheck(IN P_ADAPTER_T prAdapter)
{
	ASSERT(prAdapter);

	/* check timer status */
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
WLAN_STATUS wlanProcessMboxMessage(IN P_ADAPTER_T prAdapter)
{
	UINT_32 i;

	ASSERT(prAdapter);

	for (i = 0; i < MBOX_ID_TOTAL_NUM; i++) {	/* MBOX_ID_TOTAL_NUM = 1 */
		mboxRcvAllMsg(prAdapter, (ENUM_MBOX_ID_T) i);
	}

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
WLAN_STATUS wlanEnqueueTxPacket(IN P_ADAPTER_T prAdapter, IN P_NATIVE_PACKET prNativePacket)
{
	P_TX_CTRL_T prTxCtrl;
	P_MSDU_INFO_T prMsduInfo;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);

	prTxCtrl = &prAdapter->rTxCtrl;

	/* get a free packet header */
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_MSDU_INFO_LIST);
	QUEUE_REMOVE_HEAD(&prTxCtrl->rFreeMsduInfoList, prMsduInfo, P_MSDU_INFO_T);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_MSDU_INFO_LIST);

	if (prMsduInfo == NULL) {
		DBGLOG(TX, WARN, "%s prMsduInfo is null!\n", __func__);
		return WLAN_STATUS_RESOURCES;
	}
	prMsduInfo->eSrc = TX_PACKET_OS;

	if (nicTxFillMsduInfo(prAdapter, prMsduInfo, prNativePacket) == FALSE) {
		/* packet is not extractable */

		/* fill fails */
		kalSendComplete(prAdapter->prGlueInfo, prNativePacket, WLAN_STATUS_INVALID_PACKET);

		nicTxReturnMsduInfo(prAdapter, prMsduInfo);

		DBGLOG(TX, WARN, "%s WLAN_STATUS_INVALID_PACKET!\n", __func__);

		return WLAN_STATUS_INVALID_PACKET;
	}
	/* enqueue to QM */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);
	return WLAN_STATUS_SUCCESS;
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
WLAN_STATUS wlanFlushTxPendingPackets(IN P_ADAPTER_T prAdapter)
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
WLAN_STATUS wlanTxPendingPackets(IN P_ADAPTER_T prAdapter, IN OUT PBOOLEAN pfgHwAccess)
{
	P_TX_CTRL_T prTxCtrl;
	P_MSDU_INFO_T prMsduInfo;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);
	prTxCtrl = &prAdapter->rTxCtrl;

	ASSERT(pfgHwAccess);

	/* <1> dequeue packets by txDequeuTxPackets() */
	/* Note: prMsduInfo is a packet list queue */
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_QM_TX_QUEUE);
	prMsduInfo = qmDequeueTxPackets(prAdapter, &prTxCtrl->rTc);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_QM_TX_QUEUE);

	if (prMsduInfo != NULL) {
		if (kalIsCardRemoved(prAdapter->prGlueInfo) == FALSE) {
			/* <2> Acquire LP-OWN if necessary */
			if (*pfgHwAccess == FALSE) {
				*pfgHwAccess = TRUE;

				wlanAcquirePowerControl(prAdapter);
			}
#if (MT6620_E1_ASIC_HIFSYS_WORKAROUND == 1)
			if (prAdapter->fgIsClockGatingEnabled == TRUE)
				nicDisableClockGating(prAdapter);
#endif
			/* <3> send packet"s" to HIF */
			nicTxMsduInfoList(prAdapter, prMsduInfo);

			/* <4> update TC by txAdjustTcQuotas() */
			nicTxAdjustTcq(prAdapter);
		} else
			wlanProcessQueuedMsduInfo(prAdapter, prMsduInfo); /* free the packet */
	} else {
		if (prAdapter->prGlueInfo->i4TxPendingFrameNum > 0)
			DBGLOGLIMITED(INIT, WARN, "prMsduInfo is Null and PendingPKT(%u)\n"
			, prAdapter->prGlueInfo->i4TxPendingFrameNum);
	}

#if (MT6620_E1_ASIC_HIFSYS_WORKAROUND == 1)
	if (prAdapter->fgIsClockGatingEnabled == FALSE)
		nicEnableClockGating(prAdapter);
#endif

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
WLAN_STATUS wlanAcquirePowerControl(IN P_ADAPTER_T prAdapter)
{
	ASSERT(prAdapter);

	/* do driver own */
	ACQUIRE_POWER_CONTROL_FROM_PM(prAdapter);

	/* Reset sleepy state *//* no use */
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
WLAN_STATUS wlanReleasePowerControl(IN P_ADAPTER_T prAdapter)
{
	ASSERT(prAdapter);

	/* do FW own */
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
UINT_32 wlanGetTxPendingFrameCount(IN P_ADAPTER_T prAdapter)
{
	P_TX_CTRL_T prTxCtrl;
	UINT_32 u4Num;

	ASSERT(prAdapter);
	prTxCtrl = &prAdapter->rTxCtrl;

	/* number in prTxQueue + number in RX forward */
	u4Num = kalGetTxPendingFrameCount(prAdapter->prGlueInfo) + (UINT_32) (prTxCtrl->i4PendingFwdFrameCount);

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
ENUM_ACPI_STATE_T wlanGetAcpiState(IN P_ADAPTER_T prAdapter)
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
VOID wlanSetAcpiState(IN P_ADAPTER_T prAdapter, IN ENUM_ACPI_STATE_T ePowerState)
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
UINT_8 wlanGetEcoVersion(IN P_ADAPTER_T prAdapter)
{
	ASSERT(prAdapter);

	if (nicVerifyChipID(prAdapter) == TRUE)
		return prAdapter->ucRevID + 1;
	else
		return 0;
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
VOID wlanDefTxPowerCfg(IN P_ADAPTER_T prAdapter)
{
	UINT_8 i;
	P_GLUE_INFO_T prGlueInfo = prAdapter->prGlueInfo;
	P_SET_TXPWR_CTRL_T prTxpwr;
#if CFG_SUPPORT_TX_POWER_BACK_OFF
	P_REG_INFO_T prRegInfo;
#endif
	ASSERT(prGlueInfo);

#if CFG_SUPPORT_TX_POWER_BACK_OFF
	prRegInfo = &prGlueInfo->rRegInfo;
	ASSERT(prRegInfo);
#endif
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

#if CFG_SUPPORT_TX_POWER_BACK_OFF
	for (i = 0; i < 40; i++) {
		/* 40 : MAXNUM_MITIGATED_PWR_BY_CH_BY_MODE */
		prTxpwr->arRlmMitigatedPwrByChByMode[i].channel =
			prRegInfo->arRlmMitigatedPwrByChByMode[i].channel;
		prTxpwr->arRlmMitigatedPwrByChByMode[i].mitigatedCckDsss =
			prRegInfo->arRlmMitigatedPwrByChByMode[i].mitigatedCckDsss;
		prTxpwr->arRlmMitigatedPwrByChByMode[i].mitigatedOfdm =
			prRegInfo->arRlmMitigatedPwrByChByMode[i].mitigatedOfdm;
		prTxpwr->arRlmMitigatedPwrByChByMode[i].mitigatedHt20 =
			prRegInfo->arRlmMitigatedPwrByChByMode[i].mitigatedHt20;
		prTxpwr->arRlmMitigatedPwrByChByMode[i].mitigatedHt40 =
			prRegInfo->arRlmMitigatedPwrByChByMode[i].mitigatedHt40;
	}
#endif
}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is to
*        set preferred band configuration corresponding to network type
*
* @param prAdapter      Pointer of Adapter Data Structure
* @param eBand          Given band
* @param eNetTypeIndex  Given Network Type
*
* @return none
*/
/*----------------------------------------------------------------------------*/
VOID
wlanSetPreferBandByNetwork(IN P_ADAPTER_T prAdapter, IN ENUM_BAND_T eBand, IN ENUM_NETWORK_TYPE_INDEX_T eNetTypeIndex)
{
	ASSERT(prAdapter);
	ASSERT(eBand <= BAND_NUM);
	ASSERT(eNetTypeIndex <= NETWORK_TYPE_INDEX_NUM);

	/* 1. set prefer band according to network type */
	prAdapter->aePreferBand[eNetTypeIndex] = eBand;

	/* 2. remove buffered BSS descriptors correspondingly */
	if (eBand == BAND_2G4)
		scanRemoveBssDescByBandAndNetwork(prAdapter, BAND_5G, eNetTypeIndex);
	else if (eBand == BAND_5G)
		scanRemoveBssDescByBandAndNetwork(prAdapter, BAND_2G4, eNetTypeIndex);

}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is to
*        get channel information corresponding to specified network type
*
* @param prAdapter      Pointer of Adapter Data Structure
* @param eNetTypeIndex  Given Network Type
*
* @return channel number
*/
/*----------------------------------------------------------------------------*/
UINT_8 wlanGetChannelNumberByNetwork(IN P_ADAPTER_T prAdapter, IN ENUM_NETWORK_TYPE_INDEX_T eNetTypeIndex)
{
	P_BSS_INFO_T prBssInfo;

	ASSERT(prAdapter);
	ASSERT(eNetTypeIndex <= NETWORK_TYPE_INDEX_NUM);

	prBssInfo = &(prAdapter->rWifiVar.arBssInfo[eNetTypeIndex]);

	return prBssInfo->ucPrimaryChannel;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is to
*        get BSS descriptor information corresponding to specified network type
*
* @param prAdapter      Pointer of Adapter Data Structure
* @param eNetTypeIndex  Given Network Type
*
* @return pointer to BSS_DESC_T
*/
/*----------------------------------------------------------------------------*/
P_BSS_DESC_T wlanGetTargetBssDescByNetwork(IN P_ADAPTER_T prAdapter, IN ENUM_NETWORK_TYPE_INDEX_T eNetTypeIndex)
{
	ASSERT(prAdapter);
	ASSERT(eNetTypeIndex <= NETWORK_TYPE_INDEX_NUM);

	switch (eNetTypeIndex) {
	case NETWORK_TYPE_AIS_INDEX:
		return prAdapter->rWifiVar.rAisFsmInfo.prTargetBssDesc;

	case NETWORK_TYPE_P2P_INDEX:
		return NULL;

	case NETWORK_TYPE_BOW_INDEX:
		return prAdapter->rWifiVar.rBowFsmInfo.prTargetBssDesc;

	default:
		return NULL;
	}
}

#if CFG_SUPPORT_ADD_CONN_AP
/*----------------------------------------------------------------------------*/
/*!
* @brief This function is to
*       check if there is the connected AP in the scan list while the connected AP is weak signal. If there is no,
*	add a connected AP to scan result list.
*
* @param prAdapter      Pointer of Adapter Data Structure
*
* @return WLAN_STATUS_SUCCESS
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanCheckConnectedAP(IN P_ADAPTER_T prAdapter)
{
	const UINT_8 aucBCAddr[] = BC_MAC_ADDR;
	BOOLEAN fgGenAPMsg = FALSE;
	P_WLAN_BEACON_FRAME_T prBeacon = NULL;
	P_IE_SSID_T prSsid = NULL;
	PARAM_SSID_T rSsid;
	PARAM_802_11_CONFIG_T rConfiguration;
	PARAM_RATES_EX rSupportedRates;
	P_BSS_DESC_T prBssDesc = NULL;
	ENUM_PARAM_NETWORK_TYPE_T eNetworkType;
	ENUM_PARAM_OP_MODE_T eOpMode;

	DEBUGFUNC("wlanCheckConnectedAP");

	ASSERT(prAdapter);

	prBssDesc = prAdapter->rWifiVar.rAisFsmInfo.prTargetBssDesc;
	if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) == PARAM_MEDIA_STATE_DISCONNECTED) {
		DBGLOG(SCN, WARN, "disconnect state, no need to report!\n");
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (prBssDesc &&
			(prBssDesc->u2RawLength == 0) &&
			prAdapter->fgIsLinkQualityValid &&
			(prAdapter->rLinkQuality.cRssi != -127)) {
		DBGLOG(SCN, WARN,
			"connected state but no connected ap in scan results and poll signal is %d!\n",
			(PARAM_RSSI)prAdapter->rLinkQuality.cRssi);
		fgGenAPMsg = TRUE;
	}

	if (fgGenAPMsg == TRUE) {
		if (!prBssDesc->u2IELength)
			return WLAN_STATUS_FAILURE;
		prBeacon = cnmMemAlloc(prAdapter, RAM_TYPE_BUF, sizeof(WLAN_BEACON_FRAME_T) + prBssDesc->u2IELength);
		if (!prBeacon) {
			ASSERT(FALSE);
			return WLAN_STATUS_FAILURE;
		}

		/* initialization */
		kalMemZero(prBeacon, sizeof(WLAN_BEACON_FRAME_T) + prBssDesc->u2IELength);

		/* prBeacon initialization */
		prBeacon->u2FrameCtrl = MAC_FRAME_BEACON;
		COPY_MAC_ADDR(prBeacon->aucDestAddr, aucBCAddr);
		COPY_MAC_ADDR(prBeacon->aucSrcAddr, prBssDesc->aucSrcAddr);
		COPY_MAC_ADDR(prBeacon->aucBSSID, prBssDesc->aucBSSID);
		prBeacon->u2BeaconInterval = prBssDesc->u2BeaconInterval;
		prBeacon->u2CapInfo = prBssDesc->u2CapInfo;

		/* prSSID initialization */
		if (prBssDesc->ucSSIDLen > ELEM_MAX_LEN_SSID)
			prBssDesc->ucSSIDLen = ELEM_MAX_LEN_SSID;
		kalMemCopy(prBeacon->aucInfoElem, prBssDesc->aucIEBuf,
			prBssDesc->ucSSIDLen + OFFSET_OF(IE_SSID_T, aucSSID));
		prSsid = (P_IE_SSID_T) (&prBeacon->aucInfoElem[0]);
		COPY_SSID(rSsid.aucSsid, rSsid.u4SsidLen, prSsid->aucSSID, prSsid->ucLength);

		/* rConfiguration initialization */
		rConfiguration.u4Length = sizeof(PARAM_802_11_CONFIG_T);
		rConfiguration.u4BeaconPeriod = (UINT_32) prBeacon->u2BeaconInterval;
		rConfiguration.u4ATIMWindow = prBssDesc->u2ATIMWindow;
		rConfiguration.u4DSConfig = nicChannelNum2Freq(prBssDesc->ucChannelNum);
		rConfiguration.rFHConfig.u4Length = sizeof(PARAM_802_11_CONFIG_FH_T);

		if (prBssDesc->eBand == BAND_2G4) {
			if ((prBssDesc->u2OperationalRateSet & RATE_SET_OFDM)
			    || prBssDesc->fgIsERPPresent) {
				eNetworkType = PARAM_NETWORK_TYPE_OFDM24;
			} else {
				eNetworkType = PARAM_NETWORK_TYPE_DS;
			}
		} else {
			ASSERT(prBssDesc->eBand == BAND_5G);
			eNetworkType = PARAM_NETWORK_TYPE_OFDM5;
		}

		switch (prBssDesc->eBSSType) {
		case BSS_TYPE_IBSS:
			eOpMode = NET_TYPE_IBSS;
			break;

		case BSS_TYPE_INFRASTRUCTURE:
		case BSS_TYPE_P2P_DEVICE:
		case BSS_TYPE_BOW_DEVICE:
		default:
			eOpMode = NET_TYPE_INFRA;
			break;
		}
		/* rSupportedRates initialization */
		kalMemZero(rSupportedRates, sizeof(PARAM_RATES_EX));
	}
	if (prBeacon) {
		kalIndicateBssInfo(prAdapter->prGlueInfo,
				   (PUINT_8) prBeacon,
				   sizeof(WLAN_BEACON_FRAME_T) + prBssDesc->u2IELength,
				   prBssDesc->ucChannelNum, (PARAM_RSSI) prAdapter->rLinkQuality.cRssi);
		nicAddScanResult(prAdapter,
			 prBeacon->aucBSSID,
			 &rSsid,
			 prBeacon->u2CapInfo & CAP_INFO_PRIVACY ? 1 : 0,
			 (PARAM_RSSI) prAdapter->rLinkQuality.cRssi,
			 eNetworkType,
			 &rConfiguration,
			 eOpMode,
			 rSupportedRates,
			 prBssDesc->u2IELength, (PUINT_8) ((ULONG) (prBeacon) + WLAN_MAC_MGMT_HEADER_LEN));
		cnmMemFree(prAdapter, prBeacon);
	}

	return WLAN_STATUS_SUCCESS;
}
#endif

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
WLAN_STATUS wlanCheckSystemConfiguration(IN P_ADAPTER_T prAdapter)
{
#if (CFG_NVRAM_EXISTENCE_CHECK == 1) || (CFG_SW_NVRAM_VERSION_CHECK == 1)
	const UINT_8 aucZeroMacAddr[] = NULL_MAC_ADDR;
	const UINT_8 aucBCAddr[] = BC_MAC_ADDR;
	BOOLEAN fgIsConfExist = TRUE;
	BOOLEAN fgGenErrMsg = FALSE;
	P_REG_INFO_T prRegInfo = NULL;
	P_WLAN_BEACON_FRAME_T prBeacon = NULL;
	P_IE_SSID_T prSsid = NULL;
	UINT_32 u4ErrCode = 0;
	UINT_8 aucErrMsg[32];
	PARAM_SSID_T rSsid;
	PARAM_802_11_CONFIG_T rConfiguration;
	PARAM_RATES_EX rSupportedRates;
#endif

	DEBUGFUNC("wlanCheckSystemConfiguration");

	ASSERT(prAdapter);

#if (CFG_NVRAM_EXISTENCE_CHECK == 1)
	if (kalIsConfigurationExist(prAdapter->prGlueInfo) == FALSE) {
		fgIsConfExist = FALSE;
		fgGenErrMsg = TRUE;
	}
#endif

#if (CFG_SW_NVRAM_VERSION_CHECK == 1)
	prRegInfo = kalGetConfiguration(prAdapter->prGlueInfo);

	if (fgIsConfExist == TRUE && (prAdapter->rVerInfo.u2Part1CfgPeerVersion > CFG_DRV_OWN_VERSION
					|| prAdapter->rVerInfo.u2Part2CfgPeerVersion > CFG_DRV_OWN_VERSION
					|| prAdapter->rVerInfo.u2Part1CfgOwnVersion <= CFG_DRV_PEER_VERSION
					|| prAdapter->rVerInfo.u2Part2CfgOwnVersion <= CFG_DRV_PEER_VERSION /* NVRAM */
					|| prAdapter->rVerInfo.u2FwPeerVersion > CFG_DRV_OWN_VERSION
					|| prAdapter->rVerInfo.u2FwOwnVersion <= CFG_DRV_PEER_VERSION
#if CFG_SUPPORT_PWR_LIMIT_COUNTRY
					|| prAdapter->fgIsPowerLimitTableValid == FALSE
#endif
					|| (prAdapter->fgIsEmbbededMacAddrValid == FALSE &&
					  (IS_BMCAST_MAC_ADDR(prRegInfo->aucMacAddr)
					   || EQUAL_MAC_ADDR(aucZeroMacAddr, prRegInfo->aucMacAddr)))
					|| prRegInfo->ucTxPwrValid == 0))
		fgGenErrMsg = TRUE;
#endif

	if (fgGenErrMsg == TRUE) {
		prBeacon = cnmMemAlloc(prAdapter, RAM_TYPE_BUF, sizeof(WLAN_BEACON_FRAME_T) + sizeof(IE_SSID_T));
		if (!prBeacon) {
			ASSERT(FALSE);
			return WLAN_STATUS_FAILURE;
		}

		/* initialization */
		kalMemZero(prBeacon, sizeof(WLAN_BEACON_FRAME_T) + sizeof(IE_SSID_T));

		/* prBeacon initialization */
		prBeacon->u2FrameCtrl = MAC_FRAME_BEACON;
		COPY_MAC_ADDR(prBeacon->aucDestAddr, aucBCAddr);
		COPY_MAC_ADDR(prBeacon->aucSrcAddr, aucZeroMacAddr);
		COPY_MAC_ADDR(prBeacon->aucBSSID, aucBCAddr);
		prBeacon->u2BeaconInterval = 100;
		prBeacon->u2CapInfo = CAP_INFO_ESS;

		/* prSSID initialization */
		prSsid = (P_IE_SSID_T) (&prBeacon->aucInfoElem[0]);
		prSsid->ucId = ELEM_ID_SSID;

		/* rConfiguration initialization */
		rConfiguration.u4Length = sizeof(PARAM_802_11_CONFIG_T);
		rConfiguration.u4BeaconPeriod = 100;
		rConfiguration.u4ATIMWindow = 1;
		rConfiguration.u4DSConfig = 2412;
		rConfiguration.rFHConfig.u4Length = sizeof(PARAM_802_11_CONFIG_FH_T);

		/* rSupportedRates initialization */
		kalMemZero(rSupportedRates, sizeof(PARAM_RATES_EX));
	}
#if (CFG_NVRAM_EXISTENCE_CHECK == 1)
#define NVRAM_ERR_MSG "NVRAM WARNING: Err = 0x01"
	if ((kalIsConfigurationExist(prAdapter->prGlueInfo) == FALSE) && (prBeacon) && (prSsid)) {
		COPY_SSID(prSsid->aucSSID, prSsid->ucLength, NVRAM_ERR_MSG, strlen(NVRAM_ERR_MSG));

		kalIndicateBssInfo(prAdapter->prGlueInfo,
				   (PUINT_8) prBeacon,
				   OFFSET_OF(WLAN_BEACON_FRAME_T, aucInfoElem) + OFFSET_OF(IE_SSID_T,
											   aucSSID) + prSsid->ucLength,
				   1, 0);
		COPY_SSID(rSsid.aucSsid, rSsid.u4SsidLen, NVRAM_ERR_MSG, strlen(NVRAM_ERR_MSG));
		nicAddScanResult(prAdapter,
				 prBeacon->aucBSSID,
				 &rSsid,
				 0,
				 0,
				 PARAM_NETWORK_TYPE_FH,
				 &rConfiguration,
				 NET_TYPE_INFRA,
				 rSupportedRates,
				 OFFSET_OF(WLAN_BEACON_FRAME_T, aucInfoElem) + OFFSET_OF(IE_SSID_T,
											 aucSSID) + prSsid->ucLength -
				 WLAN_MAC_MGMT_HEADER_LEN, (PUINT_8) ((ULONG) (prBeacon) + WLAN_MAC_MGMT_HEADER_LEN));
	}
#endif

#if (CFG_SW_NVRAM_VERSION_CHECK == 1)
#define VER_ERR_MSG     "NVRAM WARNING: Err = 0x%02X"
	if ((fgIsConfExist == TRUE) && (prBeacon) && (prSsid)) {
		if ((prAdapter->rVerInfo.u2Part1CfgPeerVersion > CFG_DRV_OWN_VERSION
			|| prAdapter->rVerInfo.u2Part2CfgPeerVersion > CFG_DRV_OWN_VERSION
			|| prAdapter->rVerInfo.u2Part1CfgOwnVersion <= CFG_DRV_PEER_VERSION
			|| prAdapter->rVerInfo.u2Part2CfgOwnVersion <= CFG_DRV_PEER_VERSION	/* NVRAM */
			|| prAdapter->rVerInfo.u2FwPeerVersion > CFG_DRV_OWN_VERSION
			|| prAdapter->rVerInfo.u2FwOwnVersion <= CFG_DRV_PEER_VERSION))
			u4ErrCode |= NVRAM_ERROR_VERSION_MISMATCH;

		if (prRegInfo->ucTxPwrValid == 0)
			u4ErrCode |= NVRAM_ERROR_INVALID_TXPWR;

		if (prAdapter->fgIsEmbbededMacAddrValid == FALSE && (IS_BMCAST_MAC_ADDR(prRegInfo->aucMacAddr)
								     || EQUAL_MAC_ADDR(aucZeroMacAddr,
										       prRegInfo->aucMacAddr)))
			u4ErrCode |= NVRAM_ERROR_INVALID_MAC_ADDR;

#if CFG_SUPPORT_PWR_LIMIT_COUNTRY
		if (prAdapter->fgIsPowerLimitTableValid == FALSE)
			u4ErrCode |= NVRAM_POWER_LIMIT_TABLE_INVALID;
#endif
		if (u4ErrCode != 0) {
			sprintf(aucErrMsg, VER_ERR_MSG, (unsigned int)u4ErrCode);
			COPY_SSID(prSsid->aucSSID, prSsid->ucLength, aucErrMsg, strlen(aucErrMsg));

			kalIndicateBssInfo(prAdapter->prGlueInfo,
					   (PUINT_8) prBeacon,
					   OFFSET_OF(WLAN_BEACON_FRAME_T, aucInfoElem) + OFFSET_OF(IE_SSID_T,
												   aucSSID) +
					   prSsid->ucLength, 1, 0);

			COPY_SSID(rSsid.aucSsid, rSsid.u4SsidLen, NVRAM_ERR_MSG, strlen(NVRAM_ERR_MSG));
			nicAddScanResult(prAdapter,
					 prBeacon->aucBSSID,
					 &rSsid,
					 0,
					 0,
					 PARAM_NETWORK_TYPE_FH,
					 &rConfiguration,
					 NET_TYPE_INFRA,
					 rSupportedRates,
					 OFFSET_OF(WLAN_BEACON_FRAME_T, aucInfoElem) + OFFSET_OF(IE_SSID_T,
												 aucSSID) +
					 prSsid->ucLength - WLAN_MAC_MGMT_HEADER_LEN,
					 (PUINT_8) ((ULONG) (prBeacon) + WLAN_MAC_MGMT_HEADER_LEN));
		}
	}
#endif

	if (fgGenErrMsg == TRUE)
		cnmMemFree(prAdapter, prBeacon);

	return WLAN_STATUS_SUCCESS;
}

WLAN_STATUS
wlanoidQueryStaStatistics(IN P_ADAPTER_T prAdapter,
			  IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	WLAN_STATUS rResult = WLAN_STATUS_FAILURE;
	P_STA_RECORD_T prStaRec, prTempStaRec;
	P_PARAM_GET_STA_STATISTICS prQueryStaStatistics;
	UINT_8 ucStaRecIdx;
	P_QUE_MGT_T prQM;
	CMD_GET_STA_STATISTICS_T rQueryCmdStaStatistics;
	UINT_8 ucIdx;
	P_GLUE_INFO_T prGlueInfo;

	if (prAdapter == NULL) {
		DBGLOG(INIT, ERROR, "prAdapter is Null\n");
		return rResult;
	}
	prQM = &prAdapter->rQM;
	prGlueInfo = prAdapter->prGlueInfo;
	do {
		ASSERT(pvQueryBuffer);

		/* 4 1. Sanity test */
		if ((prAdapter == NULL) || (pu4QueryInfoLen == NULL))
			break;

		if ((u4QueryBufferLen) && (pvQueryBuffer == NULL))
			break;

		if (u4QueryBufferLen < sizeof(PARAM_GET_STA_STA_STATISTICS)) {
			*pu4QueryInfoLen = sizeof(PARAM_GET_STA_STA_STATISTICS);
			rResult = WLAN_STATUS_BUFFER_TOO_SHORT;
			break;
		}

		prGlueInfo = prAdapter->prGlueInfo;
		if (prGlueInfo == NULL)
			break;

		prQueryStaStatistics = (P_PARAM_GET_STA_STATISTICS) pvQueryBuffer;
		*pu4QueryInfoLen = sizeof(PARAM_GET_STA_STA_STATISTICS);

		/* 4 5. Get driver global QM counter */
		for (ucIdx = TC0_INDEX; ucIdx <= TC3_INDEX; ucIdx++) {
			prQueryStaStatistics->au4TcAverageQueLen[ucIdx] = prQM->au4AverageQueLen[ucIdx];
			prQueryStaStatistics->au4TcCurrentQueLen[ucIdx] = prQM->au4CurrentTcResource[ucIdx];
		}

		/* 4 2. Get StaRec by MAC address */
		prStaRec = NULL;

		for (ucStaRecIdx = 0; ucStaRecIdx < CFG_NUM_OF_STA_RECORD; ucStaRecIdx++) {
			prTempStaRec = &(prAdapter->arStaRec[ucStaRecIdx]);
			if (prTempStaRec->fgIsValid && prTempStaRec->fgIsInUse) {
				if (EQUAL_MAC_ADDR(prTempStaRec->aucMacAddr, prQueryStaStatistics->aucMacAddr)) {
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
		DBGLOG(TX, INFO, "skbToDriver %lld, skbFreed: %lld\n",
			prAdapter->prGlueInfo->u8SkbToDriver,
			prAdapter->prGlueInfo->u8SkbFreed);
		prAdapter->prGlueInfo->u8SkbFreed = 0;
		prAdapter->prGlueInfo->u8SkbToDriver = 0;

		prQueryStaStatistics->u4TxTotalCount = prStaRec->u4TotalTxPktsNumber;
		prQueryStaStatistics->u4TxExceedThresholdCount = prStaRec->u4ThresholdCounter;
		prQueryStaStatistics->u4TxMaxTime = prStaRec->u4MaxTxPktsTime;
		prQueryStaStatistics->u4TxMaxHifTime = prStaRec->u4MaxTxPktsHifTime;
		if (prStaRec->u4TotalTxPktsNumber) {
			prQueryStaStatistics->u4TxAverageProcessTime =
			    (prStaRec->u4TotalTxPktsTime / prStaRec->u4TotalTxPktsNumber);
			prQueryStaStatistics->u4TxAverageHifTime =
			    (prStaRec->u4TotalTxPktsHifTime / prStaRec->u4TotalTxPktsNumber);
		} else
			prQueryStaStatistics->u4TxAverageProcessTime = 0;

		for (ucIdx = TC0_INDEX; ucIdx <= TC3_INDEX; ucIdx++) {
			prQueryStaStatistics->au4TcResourceEmptyCount[ucIdx] =
			    prQM->au4QmTcResourceEmptyCounter[prStaRec->ucNetTypeIndex][ucIdx];
			/* Reset */
			prQM->au4QmTcResourceEmptyCounter[prStaRec->ucNetTypeIndex][ucIdx] = 0;
			prQueryStaStatistics->au4TcResourceBackCount[ucIdx] =
				prQM->au4QmTcResourceBackCounter[ucIdx];
			prQM->au4QmTcResourceBackCounter[ucIdx] = 0;

			prQueryStaStatistics->au4DequeueNoTcResource[ucIdx] =
				prQM->au4DequeueNoTcResourceCounter[ucIdx];
			prQM->au4DequeueNoTcResourceCounter[ucIdx] = 0;
			prQueryStaStatistics->au4TcResourceUsedCount[ucIdx] =
				prQM->au4ResourceUsedCounter[ucIdx];
			prQM->au4ResourceUsedCounter[ucIdx] = 0;
			prQueryStaStatistics->au4TcResourceWantedCount[ucIdx] =
				prQM->au4ResourceWantedCounter[ucIdx];
			prQM->au4ResourceWantedCounter[ucIdx] = 0;
		}

		prQueryStaStatistics->u4EnqueueCounter = prQM->u4EnqeueuCounter;
		prQueryStaStatistics->u4DequeueCounter = prQM->u4DequeueCounter;
		prQueryStaStatistics->u4EnqueueStaCounter = prStaRec->u4EnqeueuCounter;
		prQueryStaStatistics->u4DequeueStaCounter = prStaRec->u4DeqeueuCounter;

		prQueryStaStatistics->IsrCnt = prGlueInfo->IsrCnt - prGlueInfo->IsrPreCnt;
		prQueryStaStatistics->IsrPassCnt = prGlueInfo->IsrPassCnt - prGlueInfo->IsrPrePassCnt;
		prQueryStaStatistics->TaskIsrCnt = prGlueInfo->TaskIsrCnt - prGlueInfo->TaskPreIsrCnt;

		prQueryStaStatistics->IsrAbnormalCnt = prGlueInfo->IsrAbnormalCnt;
		prQueryStaStatistics->IsrSoftWareCnt = prGlueInfo->IsrSoftWareCnt;
		prQueryStaStatistics->IsrRxCnt = prGlueInfo->IsrRxCnt;
		prQueryStaStatistics->IsrTxCnt = prGlueInfo->IsrTxCnt;

		/* 4 4.1 Reset statistics */
		prStaRec->u4ThresholdCounter = 0;
		prStaRec->u4TotalTxPktsNumber = 0;
		prStaRec->u4TotalTxPktsTime = 0;
		prStaRec->u4MaxTxPktsTime = 0;
		prStaRec->u4MaxTxPktsHifTime = 0;

		prStaRec->u4EnqeueuCounter = 0;
		prStaRec->u4DeqeueuCounter = 0;

		prQM->u4EnqeueuCounter = 0;
		prQM->u4DequeueCounter = 0;

		prGlueInfo->IsrPreCnt = prGlueInfo->IsrCnt;
		prGlueInfo->IsrPrePassCnt = prGlueInfo->IsrPassCnt;
		prGlueInfo->TaskPreIsrCnt = prGlueInfo->TaskIsrCnt;
		prGlueInfo->IsrAbnormalCnt = 0;
		prGlueInfo->IsrSoftWareCnt = 0;
		prGlueInfo->IsrRxCnt = 0;
		prGlueInfo->IsrTxCnt = 0;
#endif

		for (ucIdx = TC0_INDEX; ucIdx <= TC3_INDEX; ucIdx++)
			prQueryStaStatistics->au4TcQueLen[ucIdx] = prStaRec->arTxQueue[ucIdx].u4NumElem;

		rResult = WLAN_STATUS_SUCCESS;

		/* 4 6. Ensure FW supports get station link status */
		if (prAdapter->u4FwCompileFlag0 & COMPILE_FLAG0_GET_STA_LINK_STATUS) {

			rQueryCmdStaStatistics.ucIndex = prStaRec->ucIndex;
			COPY_MAC_ADDR(rQueryCmdStaStatistics.aucMacAddr, prQueryStaStatistics->aucMacAddr);
			rQueryCmdStaStatistics.ucReadClear = TRUE;

			rResult = wlanSendSetQueryCmd(prAdapter,
						      CMD_ID_GET_STA_STATISTICS,
						      FALSE,
						      TRUE,
						      TRUE,
						      nicCmdEventQueryStaStatistics,
						      nicOidCmdTimeoutCommon,
						      sizeof(CMD_GET_STA_STATISTICS_T),
						      (PUINT_8)&rQueryCmdStaStatistics,
						      pvQueryBuffer, u4QueryBufferLen);

			prQueryStaStatistics->u4Flag |= BIT(1);
		} else {
			rResult = WLAN_STATUS_NOT_SUPPORTED;
		}

	} while (FALSE);

	return rResult;
}				/* wlanoidQueryP2pVersion */

#if CFG_SUPPORT_CFG_FILE

P_WLAN_CFG_ENTRY_T wlanCfgGetEntry(IN P_ADAPTER_T prAdapter, const PCHAR pucKey)
{

	P_WLAN_CFG_ENTRY_T prWlanCfgEntry;
	P_WLAN_CFG_T prWlanCfg;
	UINT_32 i;

	prWlanCfg = prAdapter->prWlanCfg;

	ASSERT(prWlanCfg);
	ASSERT(pucKey);

	prWlanCfgEntry = NULL;

	for (i = 0; i < WLAN_CFG_ENTRY_NUM_MAX; i++) {
		prWlanCfgEntry = &prWlanCfg->arWlanCfgBuf[i];
		if (prWlanCfgEntry->aucKey[0] != '\0') {
			DBGLOG(INIT, LOUD, "compare key %s saved key %s\n", pucKey, prWlanCfgEntry->aucKey);
			if (kalStrniCmp(pucKey, prWlanCfgEntry->aucKey, WLAN_CFG_KEY_LEN_MAX - 1) == 0)
				return prWlanCfgEntry;
		}
	}

	DBGLOG(INIT, LOUD, "wifi config there is no entry \'%s\'\n", pucKey);
	return NULL;

}

WLAN_STATUS wlanCfgGet(IN P_ADAPTER_T prAdapter, const PCHAR pucKey, PCHAR pucValue, PCHAR pucValueDef, UINT_32 u4Flags)
{

	P_WLAN_CFG_ENTRY_T prWlanCfgEntry;
	P_WLAN_CFG_T prWlanCfg;

	prWlanCfg = prAdapter->prWlanCfg;

	ASSERT(prWlanCfg);
	ASSERT(pucValue);

	/* Find the exist */
	prWlanCfgEntry = wlanCfgGetEntry(prAdapter, pucKey);

	if (prWlanCfgEntry) {
		kalStrnCpy(pucValue, prWlanCfgEntry->aucValue, WLAN_CFG_VALUE_LEN_MAX - 1);
		return WLAN_STATUS_SUCCESS;
	}
	if (pucValueDef)
		kalStrnCpy(pucValue, pucValueDef, WLAN_CFG_VALUE_LEN_MAX - 1);
	return WLAN_STATUS_FAILURE;

}

UINT_32 wlanCfgGetUint32(IN P_ADAPTER_T prAdapter, const PCHAR pucKey, UINT_32 u4ValueDef)
{
	P_WLAN_CFG_ENTRY_T prWlanCfgEntry;
	P_WLAN_CFG_T prWlanCfg;
	UINT_32 u4Value;
	INT_32 u4Ret;

	prWlanCfg = prAdapter->prWlanCfg;

	ASSERT(prWlanCfg);

	u4Value = u4ValueDef;
	/* Find the exist */
	prWlanCfgEntry = wlanCfgGetEntry(prAdapter, pucKey);

	if (prWlanCfgEntry) {
		u4Ret = kalkStrtou32(prWlanCfgEntry->aucValue, 0, &u4Value);
		if (u4Ret)
			DBGLOG(INIT, ERROR, "parse prWlanCfgEntry->aucValue u4Ret=%u\n", u4Ret);
		/* u4Value = kalStrtoul(prWlanCfgEntry->aucValue, NULL, 0); */
	}

	return u4Value;
}

INT_32 wlanCfgGetInt32(IN P_ADAPTER_T prAdapter, const PCHAR pucKey, INT_32 i4ValueDef)
{
	P_WLAN_CFG_ENTRY_T prWlanCfgEntry;
	P_WLAN_CFG_T prWlanCfg;
	INT_32 i4Value;
	INT_32 i4Ret;

	prWlanCfg = prAdapter->prWlanCfg;

	ASSERT(prWlanCfg);

	i4Value = i4ValueDef;
	/* Find the exist */
	prWlanCfgEntry = wlanCfgGetEntry(prAdapter, pucKey);

	if (prWlanCfgEntry) {
		i4Ret = kalkStrtos32(prWlanCfgEntry->aucValue, 0, &i4Value);
		/* i4Ret = kalStrtol(prWlanCfgEntry->aucValue, NULL, 0); */
		if (i4Ret)
			DBGLOG(INIT, ERROR, "parse prWlanCfgEntry->aucValue i4Ret=%u\n\r", i4Ret);
	}

	return i4Value;
}

WLAN_STATUS wlanCfgSet(IN P_ADAPTER_T prAdapter, const PCHAR pucKey, PCHAR pucValue, UINT_32 u4Flags)
{

	P_WLAN_CFG_ENTRY_T prWlanCfgEntry;
	P_WLAN_CFG_T prWlanCfg;
	UINT_32 u4EntryIndex;
	UINT_32 i;
	UINT_8 ucExist;

	prWlanCfg = prAdapter->prWlanCfg;
	ASSERT(prWlanCfg);
	ASSERT(pucKey);

	/* Find the exist */
	ucExist = 0;
	prWlanCfgEntry = wlanCfgGetEntry(prAdapter, pucKey);

	if (!prWlanCfgEntry) {
		/* Find the empty */
		for (i = 0; i < WLAN_CFG_ENTRY_NUM_MAX; i++) {
			prWlanCfgEntry = &prWlanCfg->arWlanCfgBuf[i];
			if (prWlanCfgEntry->aucKey[0] == '\0')
				break;
		}

		u4EntryIndex = i;
		if (u4EntryIndex < WLAN_CFG_ENTRY_NUM_MAX) {
			prWlanCfgEntry = &prWlanCfg->arWlanCfgBuf[u4EntryIndex];
			kalMemZero(prWlanCfgEntry, sizeof(WLAN_CFG_ENTRY_T));
		} else {
			prWlanCfgEntry = NULL;
			DBGLOG(INIT, ERROR, "wifi config there is no empty entry\n");
		}
	} /* !prWlanCfgEntry */
	else
		ucExist = 1;

	if (prWlanCfgEntry) {
		if (ucExist == 0) {
			kalStrnCpy(prWlanCfgEntry->aucKey, pucKey, WLAN_CFG_KEY_LEN_MAX - 1);
			prWlanCfgEntry->aucKey[WLAN_CFG_KEY_LEN_MAX - 1] = '\0';
		}

		if (pucValue && pucValue[0] != '\0') {
			kalStrnCpy(prWlanCfgEntry->aucValue, pucValue, WLAN_CFG_VALUE_LEN_MAX - 1);
			prWlanCfgEntry->aucValue[WLAN_CFG_VALUE_LEN_MAX - 1] = '\0';

			if (ucExist) {
				if (prWlanCfgEntry->pfSetCb)
					prWlanCfgEntry->pfSetCb(prAdapter,
								prWlanCfgEntry->aucKey,
								prWlanCfgEntry->aucValue, prWlanCfgEntry->pPrivate, 0);
			}
		} else {
			/* Call the pfSetCb if value is empty ? */
			/* remove the entry if value is empty */
			kalMemZero(prWlanCfgEntry, sizeof(WLAN_CFG_ENTRY_T));
		}

	}
	/* prWlanCfgEntry */
	if (prWlanCfgEntry) {
		DBGLOG(INIT, LOUD, "Set wifi config exist %u \'%s\' \'%s\'\n",
				    ucExist, prWlanCfgEntry->aucKey, prWlanCfgEntry->aucValue);
		return WLAN_STATUS_SUCCESS;
	}
	if (pucKey)
		DBGLOG(INIT, ERROR, "Set wifi config error key \'%s\'\n", pucKey);
	if (pucValue)
		DBGLOG(INIT, ERROR, "Set wifi config error value \'%s\'\n", pucValue);
	return WLAN_STATUS_FAILURE;

}

WLAN_STATUS
wlanCfgSetCb(IN P_ADAPTER_T prAdapter, const PCHAR pucKey, WLAN_CFG_SET_CB pfSetCb, void *pPrivate, UINT_32 u4Flags)
{

	P_WLAN_CFG_ENTRY_T prWlanCfgEntry;
	P_WLAN_CFG_T prWlanCfg;

	prWlanCfg = prAdapter->prWlanCfg;
	ASSERT(prWlanCfg);

	/* Find the exist */
	prWlanCfgEntry = wlanCfgGetEntry(prAdapter, pucKey);

	if (prWlanCfgEntry) {
		prWlanCfgEntry->pfSetCb = pfSetCb;
		prWlanCfgEntry->pPrivate = pPrivate;
	}

	if (prWlanCfgEntry)
		return WLAN_STATUS_SUCCESS;
	else
		return WLAN_STATUS_FAILURE;

}

WLAN_STATUS wlanCfgSetUint32(IN P_ADAPTER_T prAdapter, const PCHAR pucKey, UINT_32 u4Value)
{

	P_WLAN_CFG_T prWlanCfg;
	UINT_8 aucBuf[WLAN_CFG_VALUE_LEN_MAX];

	prWlanCfg = prAdapter->prWlanCfg;

	ASSERT(prWlanCfg);

	kalMemZero(aucBuf, sizeof(aucBuf));

	kalSnprintf(aucBuf, WLAN_CFG_VALUE_LEN_MAX, "0x%x", (unsigned int)u4Value);

	return wlanCfgSet(prAdapter, pucKey, aucBuf, 0);
}

enum {
	STATE_EOF = 0,
	STATE_TEXT = 1,
	STATE_NEWLINE = 2
};

struct WLAN_CFG_PARSE_STATE_S {
	CHAR *ptr;
	CHAR *text;
	INT_32 nexttoken;
	UINT_32 maxSize;
};

INT_32 wlanCfgFindNextToken(struct WLAN_CFG_PARSE_STATE_S *state)
{
	CHAR *x = state->ptr;
	CHAR *s;

	if (state->nexttoken) {
		INT_32 t = state->nexttoken;

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
		}
	}
	return STATE_EOF;
}

WLAN_STATUS wlanCfgParseArgument(CHAR *cmdLine, INT_32 *argc, CHAR *argv[])
{
	struct WLAN_CFG_PARSE_STATE_S state;
	CHAR **args;
	INT_32 nargs;

	if (cmdLine == NULL || argc == NULL || argv == NULL) {
		ASSERT(0);
		return WLAN_STATUS_FAILURE;
	}
	args = argv;
	nargs = 0;
	state.ptr = cmdLine;
	state.nexttoken = 0;
	state.maxSize = 0;

	if (kalStrnLen(cmdLine, 512) >= 512) {
		ASSERT(0);
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

WLAN_STATUS
wlanCfgParseAddEntry(IN P_ADAPTER_T prAdapter,
		     PUINT_8 pucKeyHead, PUINT_8 pucKeyTail, PUINT_8 pucValueHead, PUINT_8 pucValueTail)
{

	UINT_8 aucKey[WLAN_CFG_KEY_LEN_MAX];
	UINT_8 aucValue[WLAN_CFG_VALUE_LEN_MAX];
	UINT_32 u4Len;

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

	if (u4Len < WLAN_CFG_VALUE_LEN_MAX)
		kalStrnCpy(aucKey, pucKeyHead, u4Len);
	else
		DBGLOG(INIT, ERROR, "wifi entry parse error: Data len > %d\n", u4Len);

	if (pucValueTail) {
		if (pucValueHead > pucValueTail)
			return WLAN_STATUS_FAILURE;
		u4Len = pucValueTail - pucValueHead + 1;
	} else
		u4Len = kalStrnLen(pucValueHead, WLAN_CFG_VALUE_LEN_MAX - 1);

	if (u4Len >= WLAN_CFG_VALUE_LEN_MAX)
		u4Len = WLAN_CFG_VALUE_LEN_MAX - 1;

	if (u4Len < WLAN_CFG_VALUE_LEN_MAX)
		kalStrnCpy(aucValue, pucValueHead, u4Len);
	else
		DBGLOG(INIT, ERROR, "wifi entry parse error: Data len > %d\n", u4Len);

	return wlanCfgSet(prAdapter, aucKey, aucValue, 0);
}

enum {
	WAIT_KEY_HEAD = 0,
	WAIT_KEY_TAIL,
	WAIT_VALUE_HEAD,
	WAIT_VALUE_TAIL,
	WAIT_COMMENT_TAIL
};

WLAN_STATUS wlanCfgParse(IN P_ADAPTER_T prAdapter, PUINT_8 pucConfigBuf, UINT_32 u4ConfigBufLen)
{

	struct WLAN_CFG_PARSE_STATE_S state;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	CHAR **args;
	INT_32 nargs;

	if (pucConfigBuf == NULL) {
		ASSERT(0);
		return WLAN_STATUS_FAILURE;
	}
	if (kalStrnLen(pucConfigBuf, 4000) >= 4000) {
		ASSERT(0);
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
				wlanCfgParseAddEntry(prAdapter, args[0], NULL, args[1], NULL);
			goto exit;
		case STATE_NEWLINE:
			if (nargs > 1)
				wlanCfgParseAddEntry(prAdapter, args[0], NULL, args[1], NULL);
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
	UINT_32 i;
	UINT_8 c;
	PUINT_8 pbuf;
	UINT_8 ucState;
	PUINT_8 pucKeyTail = NULL;
	PUINT_8 pucKeyHead = NULL;
	PUINT_8 pucValueHead = NULL;
	PUINT_8 pucValueTail = NULL;

	ucState = WAIT_KEY_HEAD;
	pbuf = pucConfigBuf;

	for (i = 0; i < u4ConfigBufLen; i++) {
		c = pbuf[i];
		if (c == '\r' || c == '\n') {

			if (ucState == WAIT_VALUE_TAIL) {
				/* Entry found */
				if (pucValueHead)
					wlanCfgParseAddEntry(prAdapter, pucKeyHead, pucKeyTail,
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
			if (ucState == WAIT_KEY_HEAD)
				;
			else if (ucState == WAIT_KEY_TAIL) {
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
			wlanCfgParseAddEntry(prAdapter, pucKeyHead, pucKeyTail, pucValueHead, pucValueTail);
	}
#endif

	return WLAN_STATUS_SUCCESS;
}
#endif

#if CFG_SUPPORT_CFG_FILE
WLAN_STATUS wlanCfgInit(IN P_ADAPTER_T prAdapter, PUINT_8 pucConfigBuf, UINT_32 u4ConfigBufLen, UINT_32 u4Flags)
{
	P_WLAN_CFG_T prWlanCfg;
	/* P_WLAN_CFG_ENTRY_T prWlanCfgEntry; */
	prAdapter->prWlanCfg = &prAdapter->rWlanCfg;
	prWlanCfg = prAdapter->prWlanCfg;

	kalMemZero(prWlanCfg, sizeof(WLAN_CFG_T));
	ASSERT(prWlanCfg);
	prWlanCfg->u4WlanCfgEntryNumMax = WLAN_CFG_ENTRY_NUM_MAX;
	prWlanCfg->u4WlanCfgKeyLenMax = WLAN_CFG_KEY_LEN_MAX;
	prWlanCfg->u4WlanCfgValueLenMax = WLAN_CFG_VALUE_LEN_MAX;
#if 0
	DBGLOG(INIT, INFO, "Init wifi config len %u max entry %u\n", u4ConfigBufLen, prWlanCfg->u4WlanCfgEntryNumMax);
#endif
	/* self test */
	wlanCfgSet(prAdapter, "ConfigValid", "0x123", 0);
	if (wlanCfgGetUint32(prAdapter, "ConfigValid", 0) != 0x123)
		DBGLOG(INIT, ERROR, "wifi config error %u\n", __LINE__);
	wlanCfgSet(prAdapter, "ConfigValid", "1", 0);
	if (wlanCfgGetUint32(prAdapter, "ConfigValid", 0) != 1)
		DBGLOG(INIT, ERROR, "wifi config error %u\n", __LINE__);
#if 0				/* soc chip didn't support these parameters now */
	/* Add initil config */
	/* use g,wlan,p2p,ap as prefix */
	/* Don't set cb here , overwrite by another api */
	wlanCfgSet(prAdapter, "TxLdpc", "1", 0);
	wlanCfgSet(prAdapter, "RxLdpc", "1", 0);
	wlanCfgSet(prAdapter, "RxBeamformee", "1", 0);
	wlanCfgSet(prAdapter, "RoamTh1", "100", 0);
	wlanCfgSet(prAdapter, "RoamTh2", "150", 0);
	wlanCfgSet(prAdapter, "wlanRxLdpc", "1", 0);
	wlanCfgSet(prAdapter, "apRxLdpc", "1", 0);
	wlanCfgSet(prAdapter, "p2pRxLdpc", "1", 0);
#endif
	/* Parse the pucConfigBuff */

	if (pucConfigBuf && (u4ConfigBufLen > 0))
		wlanCfgParse(prAdapter, pucConfigBuf, u4ConfigBufLen);

	return WLAN_STATUS_SUCCESS;
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
VOID wlanCfgApply(IN P_ADAPTER_T prAdapter)
{
#define STR2BYTE(s) (((((PUINT_8)s)[0]-'0')*10)+(((PUINT_8)s)[1]-'0'))
	CHAR aucValue[WLAN_CFG_VALUE_LEN_MAX];
	P_WIFI_VAR_T prWifiVar = &prAdapter->rWifiVar;
	P_REG_INFO_T prRegInfo = &prAdapter->prGlueInfo->rRegInfo;
	P_TX_PWR_PARAM_T prTxPwr = &prRegInfo->rTxPwr;

	kalMemZero(aucValue, sizeof(aucValue));
	DBGLOG(INIT, LOUD, "CFG_FILE: Apply Config File\n");
	/* Apply COUNTRY Config */
	if (wlanCfgGet(prAdapter, "country", aucValue, "", 0) == WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, LOUD, "CFG_FILE: Found Country Key, Value=%s\n", aucValue);
		prAdapter->rWifiVar.rConnSettings.u2CountryCode =
		    (((UINT_16) aucValue[0]) << 8) | ((UINT_16) aucValue[1]);
		prRegInfo->au2CountryCode[0] = aucValue[0];
		prRegInfo->au2CountryCode[1] = aucValue[1];
	}
	prWifiVar->ucApWpsMode = (UINT_8) wlanCfgGetUint32(prAdapter, "ApWpsMode", 0);
	prWifiVar->ucCert11nMode = (UINT_8)wlanCfgGetUint32(prAdapter, "Cert11nMode", 0);
	prWifiVar->ucApChannel = (UINT_8) wlanCfgGetUint32(prAdapter, "ApChannel", 0);
	DBGLOG(INIT, LOUD, "CFG_FILE: ucApWpsMode = %u, ucCert11nMode = %u, ucApChannel = %u\n",
		prWifiVar->ucApWpsMode, prWifiVar->ucCert11nMode, prWifiVar->ucApChannel);
#if 0
	if (prWifiVar->ucCert11nMode == 1)
		nicWriteMcr(prAdapter, 0x11111115, 1);
#endif
#if CFG_SUPPORT_MTK_SYNERGY
	prWifiVar->ucMtkOui = (UINT_8) wlanCfgGetUint32(prAdapter, "MtkOui", 1);
	prWifiVar->u4MtkOuiCap = (UINT_32) wlanCfgGetUint32(prAdapter, "MtkOuiCap", 0);
	prWifiVar->aucMtkFeature[0] = 0xff;
	prWifiVar->aucMtkFeature[1] = 0xff;
	prWifiVar->aucMtkFeature[2] = 0xff;
	prWifiVar->aucMtkFeature[3] = 0xff;
#endif

	if (wlanCfgGet(prAdapter, "5G_support", aucValue, "", 0) == WLAN_STATUS_SUCCESS)
		prRegInfo->ucSupport5GBand = (*aucValue == 'y') ? 1 : 0;
	if (wlanCfgGet(prAdapter, "TxPower2G4CCK", aucValue, "", 0) == WLAN_STATUS_SUCCESS
			&& kalStrLen(aucValue) == 2) {
		prTxPwr->cTxPwr2G4Cck = STR2BYTE(aucValue);
		DBGLOG(INIT, LOUD, "2.4G cck=%d\n", prTxPwr->cTxPwr2G4Cck);
	}
	if (wlanCfgGet(prAdapter, "TxPower2G4OFDM", aucValue, "", 0) == WLAN_STATUS_SUCCESS &&
	    kalStrLen(aucValue) == 10) {
		prTxPwr->cTxPwr2G4OFDM_BPSK = STR2BYTE(aucValue);
		prTxPwr->cTxPwr2G4OFDM_QPSK = STR2BYTE(aucValue + 2);
		prTxPwr->cTxPwr2G4OFDM_16QAM = STR2BYTE(aucValue + 4);
		prTxPwr->cTxPwr2G4OFDM_48Mbps = STR2BYTE(aucValue + 6);
		prTxPwr->cTxPwr2G4OFDM_54Mbps = STR2BYTE(aucValue + 8);
		DBGLOG(INIT, LOUD, "2.4G OFDM=%d,%d,%d,%d,%d\n",
			prTxPwr->cTxPwr2G4OFDM_BPSK, prTxPwr->cTxPwr2G4OFDM_QPSK,
			prTxPwr->cTxPwr2G4OFDM_16QAM, prTxPwr->cTxPwr2G4OFDM_48Mbps,
			prTxPwr->cTxPwr2G4OFDM_54Mbps);
	}
	if (wlanCfgGet(prAdapter, "TxPower2G4HT20", aucValue, "", 0) == WLAN_STATUS_SUCCESS &&
	    kalStrLen(aucValue) == 12) {
		prTxPwr->cTxPwr2G4HT20_BPSK = STR2BYTE(aucValue);
		prTxPwr->cTxPwr2G4HT20_QPSK = STR2BYTE(aucValue + 2);
		prTxPwr->cTxPwr2G4HT20_16QAM = STR2BYTE(aucValue + 4);
		prTxPwr->cTxPwr2G4HT20_MCS5 = STR2BYTE(aucValue + 6);
		prTxPwr->cTxPwr2G4HT20_MCS6 = STR2BYTE(aucValue + 8);
		prTxPwr->cTxPwr2G4HT20_MCS7 = STR2BYTE(aucValue + 10);
		DBGLOG(INIT, LOUD, "2.4G HT20=%d,%d,%d,%d,%d,%d\n",
			prTxPwr->cTxPwr2G4HT20_BPSK, prTxPwr->cTxPwr2G4HT20_QPSK,
			prTxPwr->cTxPwr2G4HT20_16QAM, prTxPwr->cTxPwr2G4HT20_MCS5,
			prTxPwr->cTxPwr2G4HT20_MCS6, prTxPwr->cTxPwr2G4HT20_MCS7);
	}
	if (wlanCfgGet(prAdapter, "TxPower2G4HT40", aucValue, "", 0) == WLAN_STATUS_SUCCESS &&
	    kalStrLen(aucValue) == 12) {
		prTxPwr->cTxPwr2G4HT40_BPSK = STR2BYTE(aucValue);
		prTxPwr->cTxPwr2G4HT40_QPSK = STR2BYTE(aucValue + 2);
		prTxPwr->cTxPwr2G4HT40_16QAM = STR2BYTE(aucValue + 4);
		prTxPwr->cTxPwr2G4HT40_MCS5 = STR2BYTE(aucValue + 6);
		prTxPwr->cTxPwr2G4HT40_MCS6 = STR2BYTE(aucValue + 8);
		prTxPwr->cTxPwr2G4HT40_MCS7 = STR2BYTE(aucValue + 10);
		DBGLOG(INIT, LOUD, "2.4G HT40=%d,%d,%d,%d,%d,%d\n",
			prTxPwr->cTxPwr2G4HT40_BPSK, prTxPwr->cTxPwr2G4HT40_QPSK,
			prTxPwr->cTxPwr2G4HT40_16QAM, prTxPwr->cTxPwr2G4HT40_MCS5,
			prTxPwr->cTxPwr2G4HT40_MCS6, prTxPwr->cTxPwr2G4HT40_MCS7);
	}
	if (wlanCfgGet(prAdapter, "TxPower5GOFDM", aucValue, "", 0) == WLAN_STATUS_SUCCESS
			&& kalStrLen(aucValue) == 10) {
		prTxPwr->cTxPwr5GOFDM_BPSK = STR2BYTE(aucValue);
		prTxPwr->cTxPwr5GOFDM_QPSK = STR2BYTE(aucValue + 2);
		prTxPwr->cTxPwr5GOFDM_16QAM = STR2BYTE(aucValue + 4);
		prTxPwr->cTxPwr5GOFDM_48Mbps = STR2BYTE(aucValue + 6);
		prTxPwr->cTxPwr5GOFDM_54Mbps = STR2BYTE(aucValue + 8);
		DBGLOG(INIT, LOUD, "5G OFDM=%d,%d,%d,%d,%d\n",
			prTxPwr->cTxPwr5GOFDM_BPSK, prTxPwr->cTxPwr5GOFDM_QPSK,
			prTxPwr->cTxPwr5GOFDM_16QAM, prTxPwr->cTxPwr5GOFDM_48Mbps,
			prTxPwr->cTxPwr5GOFDM_54Mbps);
	}
	if (wlanCfgGet(prAdapter, "TxPower5GHT20", aucValue, "", 0) == WLAN_STATUS_SUCCESS
			&& kalStrLen(aucValue) == 12) {
		prTxPwr->cTxPwr5GHT20_BPSK = STR2BYTE(aucValue);
		prTxPwr->cTxPwr5GHT20_QPSK = STR2BYTE(aucValue + 2);
		prTxPwr->cTxPwr5GHT20_16QAM = STR2BYTE(aucValue + 4);
		prTxPwr->cTxPwr5GHT20_MCS5 = STR2BYTE(aucValue + 6);
		prTxPwr->cTxPwr5GHT20_MCS6 = STR2BYTE(aucValue + 8);
		prTxPwr->cTxPwr5GHT20_MCS7 = STR2BYTE(aucValue + 10);
		DBGLOG(INIT, LOUD, "5G HT20=%d,%d,%d,%d,%d,%d\n",
			prTxPwr->cTxPwr5GHT20_BPSK, prTxPwr->cTxPwr5GHT20_QPSK,
			prTxPwr->cTxPwr5GHT20_16QAM, prTxPwr->cTxPwr5GHT20_MCS5, prTxPwr->cTxPwr5GHT20_MCS6,
			prTxPwr->cTxPwr5GHT20_MCS7);
	}
	if (wlanCfgGet(prAdapter, "TxPower5GHT40", aucValue, "", 0) == WLAN_STATUS_SUCCESS
			&& kalStrLen(aucValue) == 12) {
		prTxPwr->cTxPwr5GHT40_BPSK = STR2BYTE(aucValue);
		prTxPwr->cTxPwr5GHT40_QPSK = STR2BYTE(aucValue + 2);
		prTxPwr->cTxPwr5GHT40_16QAM = STR2BYTE(aucValue + 4);
		prTxPwr->cTxPwr5GHT40_MCS5 = STR2BYTE(aucValue + 6);
		prTxPwr->cTxPwr5GHT40_MCS6 = STR2BYTE(aucValue + 8);
		prTxPwr->cTxPwr5GHT40_MCS7 = STR2BYTE(aucValue + 10);
		DBGLOG(INIT, LOUD, "5G HT40=%d,%d,%d,%d,%d,%d\n",
			prTxPwr->cTxPwr5GHT40_BPSK, prTxPwr->cTxPwr5GHT40_QPSK,
			prTxPwr->cTxPwr5GHT40_16QAM, prTxPwr->cTxPwr5GHT40_MCS5, prTxPwr->cTxPwr5GHT40_MCS6,
			prTxPwr->cTxPwr5GHT40_MCS7);
	}
	if (wlanCfgGet(prAdapter, "MacAddr", aucValue, "", 0) == WLAN_STATUS_SUCCESS) {
		PUINT_8 pucMac = &prRegInfo->aucMacAddr[0];

		if (sscanf(aucValue, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
			pucMac, pucMac+1, pucMac+2, pucMac+3, pucMac+4, pucMac+5) != 6) {
			DBGLOG(INIT, ERROR, "Parse mac address failed, macstr %s\n", aucValue);
			kalMemZero(pucMac, MAC_ADDR_LEN);
		}
	}
	if (wlanCfgGet(prAdapter, "ApUapsd", aucValue, "", 0) == WLAN_STATUS_SUCCESS) {
		if (*aucValue == '1')
			prAdapter->rWifiVar.fgSupportUAPSD = TRUE;
		else if (*aucValue == '0')
			prAdapter->rWifiVar.fgSupportUAPSD = FALSE;

		DBGLOG(INIT, INFO, "Ap Mode Uapsd Status: %s\n", aucValue);
	}

	prWifiVar->aucAifsN[WMM_AC_BE_INDEX] =
		(UINT_8) wlanCfgGetUint32(prAdapter, "BeAifsN", 3);
	prWifiVar->aucAifsN[WMM_AC_BK_INDEX] =
		(UINT_8) wlanCfgGetUint32(prAdapter, "BkAifsN", 7);
	prWifiVar->aucAifsN[WMM_AC_VI_INDEX] =
		(UINT_8) wlanCfgGetUint32(prAdapter, "ViAifsN", 1);
	prWifiVar->aucAifsN[WMM_AC_VO_INDEX] =
		(UINT_8) wlanCfgGetUint32(prAdapter, "VoAifsN", 1);

	prWifiVar->aucCwMin[WMM_AC_BE_INDEX] =
		(UINT_8) wlanCfgGetUint32(prAdapter, "BeCwMin", 4);
	prWifiVar->aucCwMin[WMM_AC_BK_INDEX] =
		(UINT_8) wlanCfgGetUint32(prAdapter, "BkCwMin", 4);
	prWifiVar->aucCwMin[WMM_AC_VI_INDEX] =
		(UINT_8) wlanCfgGetUint32(prAdapter, "ViCwMin", 3);
	prWifiVar->aucCwMin[WMM_AC_VO_INDEX] =
		(UINT_8) wlanCfgGetUint32(prAdapter, "VoCwMin", 2);

	prWifiVar->au2CwMax[WMM_AC_BE_INDEX] =
		(UINT_16) wlanCfgGetUint32(prAdapter, "BeCwMax", 7);
	prWifiVar->au2CwMax[WMM_AC_BK_INDEX] =
		(UINT_16) wlanCfgGetUint32(prAdapter, "BkCwMax", 10);
	prWifiVar->au2CwMax[WMM_AC_VI_INDEX] =
		(UINT_16) wlanCfgGetUint32(prAdapter, "ViCwMax", 4);
	prWifiVar->au2CwMax[WMM_AC_VO_INDEX] =
		(UINT_16) wlanCfgGetUint32(prAdapter, "VoCwMax", 3);

	prWifiVar->au2TxOp[WMM_AC_BE_INDEX] =
		(UINT_16) wlanCfgGetUint32(prAdapter, "BeTxOp", 0);
	prWifiVar->au2TxOp[WMM_AC_BK_INDEX] =
		(UINT_16) wlanCfgGetUint32(prAdapter, "BkTxOp", 0);
	prWifiVar->au2TxOp[WMM_AC_VI_INDEX] =
		(UINT_16) wlanCfgGetUint32(prAdapter, "ViTxOp", 94);
	prWifiVar->au2TxOp[WMM_AC_VO_INDEX] =
		(UINT_16) wlanCfgGetUint32(prAdapter, "VoTxOp", 47);

	prAdapter->prGlueInfo->i4Priority = wlanCfgGetInt32(prAdapter, "RTPri", 0);
	/* TODO: Apply other Config */
}
#endif /* CFG_SUPPORT_CFG_FILE */

VOID wlanReleasePendingCmdById(P_ADAPTER_T prAdapter, UINT_8 ucCid)
{
	P_QUE_T prCmdQue;
	QUE_T rTempCmdQue;
	P_QUE_T prTempCmdQue = &rTempCmdQue;
	P_QUE_ENTRY_T prQueueEntry = (P_QUE_ENTRY_T) NULL;
	P_CMD_INFO_T prCmdInfo = (P_CMD_INFO_T) NULL;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);
	DBGLOG(OID, INFO, "Remove pending Cmd: CID %d\n", ucCid);

	/* 1: Clear Pending OID in prAdapter->rPendingCmdQueue */
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING);

	prCmdQue = &prAdapter->rPendingCmdQueue;
	QUEUE_MOVE_ALL(prTempCmdQue, prCmdQue);

	QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
	while (prQueueEntry) {
		prCmdInfo = (P_CMD_INFO_T) prQueueEntry;
		if (prCmdInfo->ucCID != ucCid) {
			QUEUE_INSERT_TAIL(prCmdQue, prQueueEntry);
			continue;
		}

		if (prCmdInfo->pfCmdTimeoutHandler) {
			prCmdInfo->pfCmdTimeoutHandler(prAdapter, prCmdInfo);
		} else if (prCmdInfo->fgIsOid) {
			kalOidComplete(prAdapter->prGlueInfo,
					   prCmdInfo->fgSetQuery, 0, WLAN_STATUS_FAILURE);
		}

		cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
	}

	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING);
}

/* Translate Decimals string to Hex
** The result will be put in a 2bytes variable.
** Integer part will occupy the left most 3 bits, and decimal part is in the left 13 bits
** Integer part can be parsed by kstrtou16, decimal part should be translated by mutiplying
** 16 and then pick integer part.
** For example
*/
UINT_32 wlanDecimalStr2Hexadecimals(PUINT_8 pucDecimalStr, PUINT_16 pu2Out)
{
	UINT_8 aucDecimalStr[32] = {0,};
	PUINT_8 pucDecimalPart = NULL;
	PUINT_8 tmp = NULL;
	UINT_32 u4Result = 0;
	UINT_32 u4Ret = 0;
	UINT_32 u4Degree = 0;
	UINT_32 u4Remain = 0;
	UINT_8 ucAccuracy = 4; /* Hex decimals accuarcy is 4 bytes */
	UINT_32 u4Base = 1;

	if (!pu2Out || !pucDecimalStr)
		return 1;

	while (*pucDecimalStr == '0')
		pucDecimalStr++;
	kalStrnCpy(aucDecimalStr, pucDecimalStr, sizeof(aucDecimalStr) - 1);
	pucDecimalPart = strchr(aucDecimalStr, '.');
	if (!pucDecimalPart) {
		DBGLOG(INIT, INFO, "No decimal part, ori str %s\n", pucDecimalStr);
		goto integer_part;
	}
	*pucDecimalPart++ = 0;
	/* get decimal degree */
	tmp = pucDecimalPart + strlen(pucDecimalPart);
	do {
		if (tmp == pucDecimalPart) {
			DBGLOG(INIT, INFO, "Decimal part are all 0, ori str %s\n", pucDecimalStr);
			goto integer_part;
		}
		tmp--;
	} while (*tmp == '0');

	*(++tmp) = 0;
	u4Degree = (UINT_32)(tmp - pucDecimalPart);
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
		DBGLOG(INIT, ERROR, "Parse integer str %s error\n", aucDecimalStr);
	else {
		*pu2Out = u4Result & 0xffff;
		DBGLOG(INIT, TRACE, "Result 0x%04x\n", *pu2Out);
	}
	return u4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query LTE safe channels.
*
* \param[in]  pvAdapter        Pointer to the Adapter structure.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_FAILURE
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanQueryLteSafeChannel(IN P_ADAPTER_T prAdapter)
{
	WLAN_STATUS rResult = WLAN_STATUS_FAILURE;
	PARAM_GET_CHN_INFO rQueryLteChn;

	DBGLOG(P2P, TRACE, "[ACS]Get LTE safe channels\n");

	do {
		if (!prAdapter)
			break;

		kalMemZero(&rQueryLteChn, sizeof(PARAM_GET_CHN_INFO));

		/* Get LTE safe channel list */
		wlanSendSetQueryCmd(prAdapter,
				CMD_ID_GET_LTE_CHN,
				FALSE,
				TRUE,
				FALSE,
				nicCmdEventQueryLteSafeChn,
				nicOidCmdTimeoutCommon,
				0,
				NULL,
				&rQueryLteChn,
				0);
		rResult = WLAN_STATUS_SUCCESS;
	} while (FALSE);

	return rResult;
}				/* wlanoidQueryLteSafeChannel */

uint8_t
wlanGetChannelIndex(IN uint8_t channel)
{
	uint8_t ucIdx = 1;

	if (channel <= 14)
		ucIdx = channel - 1;
	else if (channel >= 36 && channel <= 64)
		ucIdx = 14 + (channel - 36) / 4;
	else if (channel >= 100 && channel <= 140)
		ucIdx = 14 + 8 + (channel - 100) / 4;
	else if (channel >= 149 && channel <= 173)
		ucIdx = 14 + 8 + 11 + (channel - 149) / 4;
	else if (channel >= 184 && channel <= 216)
		ucIdx = 14 + 8 + 11 + 7 + (channel - 184) / 4;
	else
		DBGLOG(SCN, ERROR, "Invalid ch %u\n", channel);

	return ucIdx;
}

