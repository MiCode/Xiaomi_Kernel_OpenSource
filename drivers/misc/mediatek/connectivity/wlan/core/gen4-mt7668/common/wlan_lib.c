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
/*! \file   wlan_lib.c
*    \brief  Internal driver stack will export the required procedures here for GLUE Layer.
*
*    This file contains all routines which are exported from MediaTek 802.11 Wireless
*    LAN driver stack to GLUE Layer.
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

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
typedef struct _CODE_MAPPING_T {
	UINT_32 u4RegisterValue;
	INT_32 u4TxpowerOffset;
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
#if CFG_SUPPORT_COMPRESSION_FW_OPTION
#define COMPRESSION_OPTION_OFFSET   4
#define COMPRESSION_OPTION_MASK     BIT(4)
#endif


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
	nicTxRelease(prAdapter, FALSE);

}				/* wlanCardEjected */

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
#if QM_TEST_MODE
		g_rQM.prAdapter = prAdpater;
#endif
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
WLAN_STATUS wlanAdapterStart(IN P_ADAPTER_T prAdapter, IN P_REG_INFO_T prRegInfo)
{
	WLAN_STATUS u4Status = WLAN_STATUS_SUCCESS;
	UINT_32 i;

	ASSERT(prAdapter);

	DEBUGFUNC("wlanAdapterStart");

	/* 4 <0> Reset variables in ADAPTER_T */
	/* prAdapter->fgIsFwOwn = TRUE; */
	prAdapter->fgIsEnterD3ReqIssued = FALSE;

	prAdapter->u4OwnFailedCount = 0;
	prAdapter->u4OwnFailedLogCount = 0;

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
	kalMemSet(&(prAdapter->rWlanInfo), 0, sizeof(WLAN_INFO_T));

	/* Initialize aprBssInfo[].
	 * Important: index shall be same when mapping between aprBssInfo[]
	 *            and arBssInfoPool[]. rP2pDevInfo is indexed to final one.
	 */
	for (i = 0; i < BSS_INFO_NUM; i++)
		prAdapter->aprBssInfo[i] = &prAdapter->rWifiVar.arBssInfoPool[i];
	prAdapter->aprBssInfo[P2P_DEV_BSS_INDEX] = &prAdapter->rWifiVar.rP2pDevInfo;

	/* 4 <0.1> reset fgIsBusAccessFailed */
	fgIsBusAccessFailed = FALSE;

	do {
		u4Status = nicAllocateAdapterMemory(prAdapter);
		if (u4Status != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, "nicAllocateAdapterMemory Error!\n");
			u4Status = WLAN_STATUS_FAILURE;
			break;
		}

		prAdapter->u4OsPacketFilter = PARAM_PACKET_FILTER_SUPPORTED;

		DBGLOG(INIT, INFO, "wlanAdapterStart(): Acquiring LP-OWN\n");
		ACQUIRE_POWER_CONTROL_FROM_PM(prAdapter);
		DBGLOG(INIT, INFO, "wlanAdapterStart(): Acquiring LP-OWN-end\n");

#if (CFG_ENABLE_FULL_PM == 0)
		nicpmSetDriverOwn(prAdapter);
		if (prAdapter->fgIsFwOwn == TRUE) {
			DBGLOG(INIT, ERROR, "nicpmSetDriverOwn() failed!\n");
			u4Status = WLAN_STATUS_FAILURE;
			break;
		}
#endif

		/* 4 <1> Initialize the Adapter */
		u4Status = nicInitializeAdapter(prAdapter);
		if (u4Status != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, "nicInitializeAdapter failed!\n");
			u4Status = WLAN_STATUS_FAILURE;
			break;
		}

		/* 4 <2.1> Initialize System Service (MGMT Memory pool and STA_REC) */
		nicInitSystemService(prAdapter);

		/* 4 <2.2> Initialize Feature Options */
		wlanInitFeatureOption(prAdapter);
#if CFG_SUPPORT_MTK_SYNERGY
		if (kalIsConfigurationExist(prAdapter->prGlueInfo) == TRUE) {
			if (prRegInfo->prNvramSettings->u2FeatureReserved & BIT(MTK_FEATURE_2G_256QAM_DISABLED))
				prAdapter->rWifiVar.aucMtkFeature[0] &= ~(MTK_SYNERGY_CAP_SUPPORT_24G_MCS89);
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
		halHifSwInfoInit(prAdapter);

		/* 4 <6> Enable HIF cut-through to N9 mode, not visiting CR4 */
		u4Status = wlanWakeUpWiFi(prAdapter);
		if (u4Status != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, "wlanWakeUpWiFi failed!\n");
			u4Status = WLAN_STATUS_FAILURE;
			break;
		}
		HAL_ENABLE_FWDL(prAdapter, TRUE);

		/* 4 <7> Get ECO Version */
		u4Status = wlanSetChipEcoInfo(prAdapter);

		if (u4Status != WLAN_STATUS_SUCCESS)
			break;

#if CFG_ENABLE_FW_DOWNLOAD
		/* 4 <8> FW/patch download */

		/* 1. disable interrupt, download is done by polling mode only */
		nicDisableInterrupt(prAdapter);

		/* 2. Initialize Tx Resource to fw download state */
		nicTxInitResetResource(prAdapter);

		u4Status = wlanDownloadFW(prAdapter);

		if (u4Status != WLAN_STATUS_SUCCESS)
			break;
#endif

		DBGLOG(INIT, INFO, "Waiting for Ready bit..\n");

		/* 4 <9> check Wi-Fi FW asserts ready bit */
		u4Status = wlanCheckWifiFunc(prAdapter, TRUE);

		if (u4Status == WLAN_STATUS_SUCCESS) {
#if defined(_HIF_SDIO)
			PUINT_32 pu4WHISR = NULL;
			UINT_16 au2TxCount[16];

			pu4WHISR = (PUINT_32)kalMemAlloc(sizeof(UINT_32),
							 PHY_MEM_TYPE);
			if (!pu4WHISR) {
				DBGLOG(INIT, ERROR,
				       "Allocate pu4WHISR fail\n");
				u4Status = WLAN_STATUS_FAILURE;
				break;
			}
			/* 1. reset interrupt status */
			HAL_READ_INTR_STATUS(prAdapter, sizeof(UINT_32),
					     (PUINT_8)pu4WHISR);
			if (HAL_IS_TX_DONE_INTR(*pu4WHISR))
				HAL_READ_TX_RELEASED_COUNT(prAdapter,
							   au2TxCount);

			if (pu4WHISR)
				kalMemFree(pu4WHISR, PHY_MEM_TYPE,
					   sizeof(UINT_32));
#endif
			/* Set FW download success flag */
			prAdapter->fgIsFwDownloaded = TRUE;

			/* 2. query & reset TX Resource for normal operation */
			wlanQueryNicResourceInformation(prAdapter);

#if (CFG_SUPPORT_NIC_CAPABILITY == 1)

			/* 2.9 Workaround for Capability CMD packet lost issue */
			DBGLOG(INIT, WARN, "Send a Dummy CMD as workaround\n");
			wlanSendDummyCmd(prAdapter, TRUE);

			/* 3. query for NIC capability */
			wlanQueryNicCapability(prAdapter);

			/* 4. query for NIC capability V2 */
			wlanQueryNicCapabilityV2(prAdapter);

			/* 5. reset TX Resource for normal operation
			*	  based on the information reported from CMD_NicCapabilityV2
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
			kalIndicateStatusAndComplete(prAdapter->prGlueInfo, WLAN_STATUS_MEDIA_DISCONNECT, NULL, 0);
		}

		RECLAIM_POWER_CONTROL_TO_PM(prAdapter, FALSE);

		if (u4Status != WLAN_STATUS_SUCCESS)
			break;

		/* OID timeout timer initialize */
		cnmTimerInitTimer(prAdapter,
				  &prAdapter->rOidTimeoutTimer,
				  (PFN_MGMT_TIMEOUT_FUNC) wlanReleasePendingOid, (ULONG) NULL);

		prAdapter->ucOidTimeoutCount = 0;
		prAdapter->fgIsChipNoAck = FALSE;

		/* Return Indicated Rfb list timer */
		cnmTimerInitTimer(prAdapter,
				  &prAdapter->rPacketDelaySetupTimer,
				  (PFN_MGMT_TIMEOUT_FUNC) wlanReturnPacketDelaySetupTimeout, (ULONG) NULL);

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
		prAdapter->rWlanInfo.u2BeaconPeriod = CFG_INIT_ADHOC_BEACON_INTERVAL;
		prAdapter->rWlanInfo.u2AtimWindow = CFG_INIT_ADHOC_ATIM_WINDOW;

#if 1				/* set PM parameters */
		prAdapter->u4PsCurrentMeasureEn = prRegInfo->u4PsCurrentMeasureEn;
#if 0
		prAdapter->fgEnArpFilter = prRegInfo->fgEnArpFilter;
		prAdapter->u4UapsdAcBmp = prRegInfo->u4UapsdAcBmp;
		prAdapter->u4MaxSpLen = prRegInfo->u4MaxSpLen;
#else
		prAdapter->fgEnArpFilter = prAdapter->rWifiVar.fgEnArpFilter;
		prAdapter->u4UapsdAcBmp = prAdapter->rWifiVar.u4UapsdAcBmp;
		prAdapter->u4MaxSpLen = prAdapter->rWifiVar.u4MaxSpLen;
#endif
		DBGLOG(INIT, TRACE, "[1] fgEnArpFilter:0x%x, u4UapsdAcBmp:0x%x, u4MaxSpLen:0x%x",
		       prAdapter->fgEnArpFilter, prAdapter->u4UapsdAcBmp, prAdapter->u4MaxSpLen);

		prAdapter->fgEnCtiaPowerMode = FALSE;

#endif

		/* MGMT Initialization */
		nicInitMGMT(prAdapter, prRegInfo);

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

		/* configure available PHY type set */
		nicSetAvailablePhyTypeSet(prAdapter);

#if 0				/* Marked for MT6630 */
#if 1				/* set PM parameters */
		{
#if CFG_SUPPORT_PWR_MGT
			prAdapter->u4PowerMode = prRegInfo->u4PowerMode;
#if CFG_ENABLE_WIFI_DIRECT
			prAdapter->rWlanInfo.arPowerSaveMode[NETWORK_TYPE_P2P_INDEX].ucNetTypeIndex =
			    NETWORK_TYPE_P2P_INDEX;
			prAdapter->rWlanInfo.arPowerSaveMode[NETWORK_TYPE_P2P_INDEX].ucPsProfile = ENUM_PSP_FAST_SWITCH;
#endif
#else
			prAdapter->u4PowerMode = ENUM_PSP_CONTINUOUS_ACTIVE;
#endif

			nicConfigPowerSaveProfile(prAdapter,
						  prAdapter->prAisBssInfo->ucBssIndex, prAdapter->u4PowerMode, FALSE);
		}

#endif
#endif
		/* Check if it is disabled by hardware */
		if (prAdapter->fgIsHw5GBandDisabled)
			prAdapter->fgEnable5GBand = FALSE;
		else
			prAdapter->fgEnable5GBand = TRUE;

#if CFG_SUPPORT_NVRAM
		/* load manufacture data */
		if (kalIsConfigurationExist(prAdapter->prGlueInfo) == TRUE)
			wlanLoadManufactureData(prAdapter, prRegInfo);
		else
			DBGLOG(INIT, WARN, "%s: load manufacture data fail\n", __func__);
#endif

#if 0
		/* Update Auto rate parameters in FW */
		nicRlmArUpdateParms(prAdapter,
				    prRegInfo->u4ArSysParam0,
				    prRegInfo->u4ArSysParam1, prRegInfo->u4ArSysParam2, prRegInfo->u4ArSysParam3);
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
		nicReleaseAdapterMemory(prAdapter);
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
	WLAN_STATUS u4Status = WLAN_STATUS_SUCCESS;

	ASSERT(prAdapter);

	/* MGMT - unitialization */
	nicUninitMGMT(prAdapter);

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
#if 1
	wlanPowerOffWifi(prAdapter);
#else
	if (prAdapter->rAcpiState == ACPI_STATE_D0 &&
	    !wlanIsChipNoAck(prAdapter) && !kalIsCardRemoved(prAdapter->prGlueInfo)) {
		/* 0. Disable interrupt, this can be done without Driver own */
		nicDisableInterrupt(prAdapter);

		ACQUIRE_POWER_CONTROL_FROM_PM(prAdapter);

		/* 1. Set CMD to FW to tell WIFI to stop (enter power off state) */
		if (prAdapter->fgIsFwOwn == FALSE && wlanSendNicPowerCtrlCmd(prAdapter, 1) == WLAN_STATUS_SUCCESS) {
			UINT_32 i;
			/* 2. Clear pending interrupt */
			i = 0;
			while (i < CFG_IST_LOOP_COUNT && nicProcessIST(prAdapter) != WLAN_STATUS_NOT_INDICATING) {
				i++;
			};

			/* 3. Wait til RDY bit has been cleaerd */
			wlanCheckWifiFunc(prAdapter, FALSE);
		}
#if !CFG_ENABLE_FULL_PM
		/* 4. Set Onwership to F/W */
		nicpmSetFWOwn(prAdapter, FALSE);
#endif

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
#endif

	nicRxUninitialize(prAdapter);

	nicTxRelease(prAdapter, FALSE);

	/* System Service Uninitialization */
	nicUninitSystemService(prAdapter);

	nicReleaseAdapterMemory(prAdapter);

#if defined(_HIF_SPI)
	/* Note: restore the SPI Mode Select from 32 bit to default */
	nicRestoreSpiDefMode(prAdapter);
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
BOOL wlanISR(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgGlobalIntrCtrl)
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
VOID wlanIST(IN P_ADAPTER_T prAdapter)
{
	UINT_32 u4Status = WLAN_STATUS_SUCCESS;
	ASSERT(prAdapter);

	ACQUIRE_POWER_CONTROL_FROM_PM(prAdapter);

	u4Status = nicProcessIST(prAdapter);
	if (u4Status != WLAN_STATUS_SUCCESS)
		DBGLOG(REQ, INFO, "Fail in nicProcessIST! status [%x]\n",
		       u4Status);

#if defined(CONFIG_ANDROID) && (CFG_ENABLE_WAKE_LOCK)
	if (KAL_WAKE_LOCK_ACTIVE(prAdapter, &prAdapter->prGlueInfo->rIntrWakeLock))
		KAL_WAKE_UNLOCK(prAdapter, &prAdapter->prGlueInfo->rIntrWakeLock);
#endif
	nicEnableInterrupt(prAdapter);

	RECLAIM_POWER_CONTROL_TO_PM(prAdapter, FALSE);


}

VOID wlanClearPendingInterrupt(IN P_ADAPTER_T prAdapter)
{
	UINT_32 i;

	i = 0;
	while (i < CFG_IST_LOOP_COUNT && nicProcessIST(prAdapter) != WLAN_STATUS_NOT_INDICATING) {
		i++;
	};
}

WLAN_STATUS wlanCheckWifiFunc(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgRdyChk)
{
	BOOLEAN fgResult, fgTimeout;
	UINT_32 u4Result, u4Status, u4StartTime, u4CurTime;

	u4StartTime = kalGetTimeTick();
	fgTimeout = FALSE;

#if defined(_HIF_USB)
	if (prAdapter->prGlueInfo->rHifInfo.state == USB_STATE_LINK_DOWN)
		return WLAN_STATUS_FAILURE;
#endif

	while (TRUE) {
		if (fgRdyChk)
			HAL_WIFI_FUNC_READY_CHECK(prAdapter, WIFI_FUNC_READY_BITS, &fgResult);
		else {
			HAL_WIFI_FUNC_OFF_CHECK(prAdapter, WIFI_FUNC_READY_BITS, &fgResult);

			if (nicProcessIST(prAdapter) != WLAN_STATUS_NOT_INDICATING)
				DBGLOG(INIT, INFO, "Handle pending interrupt\n");
		}
		u4CurTime = kalGetTimeTick();

		if (CHECK_FOR_TIMEOUT(u4CurTime, u4StartTime,
				      CFG_RESPONSE_POLLING_TIMEOUT * CFG_RESPONSE_POLLING_DELAY)) {

			fgTimeout = TRUE;
		}

		if (fgResult) {
			if (fgRdyChk)
				DBGLOG(INIT, INFO, "Ready bit asserted\n");
			else
				DBGLOG(INIT, INFO, "Wi-Fi power off done!\n");

			u4Status = WLAN_STATUS_SUCCESS;

			break;
		} else if (kalIsCardRemoved(prAdapter->prGlueInfo) == TRUE || fgIsBusAccessFailed == TRUE) {
			u4Status = WLAN_STATUS_FAILURE;

			break;
		} else if (fgTimeout) {
			HAL_WIFI_FUNC_GET_STATUS(prAdapter, u4Result);
			DBGLOG(INIT, ERROR, "Waiting for %s: Timeout, Status=0x%08x\n",
				fgRdyChk ? "ready bit" : "power off", u4Result);

			u4Status = WLAN_STATUS_FAILURE;

			break;
		}
			kalMsleep(CFG_RESPONSE_POLLING_DELAY);

	}

	return u4Status;
}

WLAN_STATUS wlanPowerOffWifi(IN P_ADAPTER_T prAdapter)
{
	WLAN_STATUS rStatus;
	/* Hif power off wifi */
	rStatus = halHifPowerOffWifi(prAdapter);
	prAdapter->fgIsCr4FwDownloaded = FALSE;

	return rStatus;
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
	QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
	while (prQueueEntry) {
		prCmdInfo = (P_CMD_INFO_T) prQueueEntry;

		switch (prCmdInfo->eCmdType) {
		case COMMAND_TYPE_GENERAL_IOCTL:
		case COMMAND_TYPE_NETWORK_IOCTL:
			/* command packet will be always sent */
			eFrameAction = FRAME_ACTION_TX_PKT;
			break;

		case COMMAND_TYPE_SECURITY_FRAME:
			/* inquire with QM */
			prMsduInfo = prCmdInfo->prMsduInfo;

			eFrameAction = qmGetFrameAction(prAdapter, prMsduInfo->ucBssIndex,
							prMsduInfo->ucStaRecIndex, NULL, FRAME_TYPE_802_1X,
							prCmdInfo->u2InfoBufLen);
			break;

		case COMMAND_TYPE_MANAGEMENT_FRAME:
			/* inquire with QM */
			prMsduInfo = prCmdInfo->prMsduInfo;

			eFrameAction = qmGetFrameAction(prAdapter, prMsduInfo->ucBssIndex,
							prMsduInfo->ucStaRecIndex, prMsduInfo, FRAME_TYPE_MMPDU,
							prMsduInfo->u2FrameLength);
			break;

		default:
			ASSERT(0);
			break;
		}

		/* 4 <3> handling upon dequeue result */
		if (eFrameAction == FRAME_ACTION_DROP_PKT) {
			DBGLOG(INIT, INFO, "DROP CMD TYPE[%u] ID[0x%02X] SEQ[%u]\n",
			       prCmdInfo->eCmdType, prCmdInfo->ucCID, prCmdInfo->ucCmdSeqNum);
			wlanReleaseCommand(prAdapter, prCmdInfo, TX_RESULT_DROPPED_IN_DRIVER);
			cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
		} else if (eFrameAction == FRAME_ACTION_QUEUE_PKT) {
			DBGLOG(INIT, TRACE, "QUE back CMD TYPE[%u] ID[0x%02X] SEQ[%u]\n",
			       prCmdInfo->eCmdType, prCmdInfo->ucCID, prCmdInfo->ucCmdSeqNum);

			QUEUE_INSERT_TAIL(prMergeCmdQue, prQueueEntry);
		} else if (eFrameAction == FRAME_ACTION_TX_PKT) {
			/* 4 <4> Send the command */
#if CFG_SUPPORT_MULTITHREAD
			rStatus = wlanSendCommandMthread(prAdapter, prCmdInfo);

			if (rStatus == WLAN_STATUS_RESOURCES) {
				/* no more TC4 resource for further transmission */
				DBGLOG(INIT, WARN, "NO Res CMD TYPE[%u] ID[0x%02X] SEQ[%u]\n",
				       prCmdInfo->eCmdType, prCmdInfo->ucCID, prCmdInfo->ucCmdSeqNum);

				set_bit(GLUE_FLAG_HIF_PRT_HIF_DBG_INFO_BIT, &(prAdapter->prGlueInfo->ulFlag));

				QUEUE_INSERT_TAIL(prMergeCmdQue, prQueueEntry);
				break;
			} else if (rStatus == WLAN_STATUS_PENDING) {
				/* Do nothing */
				/* Do nothing */
			} else if (rStatus == WLAN_STATUS_SUCCESS) {
				/* Do nothing */
				/* Do nothing */
			} else {
				P_CMD_INFO_T prCmdInfo = (P_CMD_INFO_T) prQueueEntry;

				if (prCmdInfo->fgIsOid) {
					kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery,
						       prCmdInfo->u4SetInfoLen, rStatus);
				}
				cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
			}

#else
			rStatus = wlanSendCommand(prAdapter, prCmdInfo);

			if (rStatus == WLAN_STATUS_RESOURCES) {
				/* no more TC4 resource for further transmission */

				DBGLOG(INIT, WARN, "NO Resource for CMD TYPE[%u] ID[0x%02X] SEQ[%u]\n",
				       prCmdInfo->eCmdType, prCmdInfo->ucCID, prCmdInfo->ucCmdSeqNum);

				QUEUE_INSERT_TAIL(prMergeCmdQue, prQueueEntry);
				break;
			} else if (rStatus == WLAN_STATUS_PENDING) {
				/* command packet which needs further handling upon response */
				KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING);
				QUEUE_INSERT_TAIL(&(prAdapter->rPendingCmdQueue), prQueueEntry);
				KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING);
			} else {
				P_CMD_INFO_T prCmdInfo = (P_CMD_INFO_T) prQueueEntry;

				if (rStatus == WLAN_STATUS_SUCCESS) {
					if (prCmdInfo->pfCmdDoneHandler) {
						prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo,
									    prCmdInfo->pucInfoBuffer);
					}
				} else {
					if (prCmdInfo->fgIsOid) {
						kalOidComplete(prAdapter->prGlueInfo,
							       prCmdInfo->fgSetQuery, prCmdInfo->u4SetInfoLen, rStatus);
					}
				}

				cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
			}
#endif
		} else {
			ASSERT(0);
		}

		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
	}

	/* 4 <3> Merge back to original queue */
	/* 4 <3.1> Merge prMergeCmdQue & prTempCmdQue */
	QUEUE_CONCATENATE_QUEUES(prMergeCmdQue, prTempCmdQue);

	/* 4 <3.2> Move prCmdQue to prStandInQue, due to prCmdQue might differ due to incoming 802.1X frames */
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

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	prTxCtrl = &prAdapter->rTxCtrl;

	do {
		/* <0> card removal check */
		if (kalIsCardRemoved(prAdapter->prGlueInfo) == TRUE || fgIsBusAccessFailed == TRUE) {
			rStatus = WLAN_STATUS_FAILURE;
			break;
		}

		/* <1.1> Assign Traffic Class(TC) */
		ucTC = nicTxGetCmdResourceType(prCmdInfo);

		/* <1.2> Check if pending packet or resource was exhausted */
		rStatus = nicTxAcquireResource(prAdapter, ucTC, nicTxGetCmdPageCount(prCmdInfo), TRUE);
		if (rStatus == WLAN_STATUS_RESOURCES) {
			DBGLOG(INIT, INFO, "NO Resource:%d\n", ucTC);
			break;
		}
		/* <1.3> Forward CMD_INFO_T to NIC Layer */
		rStatus = nicTxCmd(prAdapter, prCmdInfo, ucTC);

		/* <1.4> Set Pending in response to Query Command/Need Response */
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
* \retval WLAN_STATUS_SUCCESS   : CMD was written to HIF and be freed(CMD Done) immediately.
* \retval WLAN_STATUS_RESOURCE  : No resource for current command, need to wait for previous
*                                 frame finishing their transmission.
* \retval WLAN_STATUS_FAILURE   : Get failure while access HIF or been rejected.
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanSendCommandMthread(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo)
{
	P_TX_CTRL_T prTxCtrl;
	UINT_8 ucTC;		/* "Traffic Class" SW(Driver) resource classification */
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;

	QUE_T rTempCmdQue;
	P_QUE_T prTempCmdQue;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	prTxCtrl = &prAdapter->rTxCtrl;

	prTempCmdQue = &rTempCmdQue;
	QUEUE_INITIALIZE(prTempCmdQue);

	do {
		/* <0> card removal check */
		if (kalIsCardRemoved(prAdapter->prGlueInfo) == TRUE || fgIsBusAccessFailed == TRUE) {
			rStatus = WLAN_STATUS_FAILURE;
			break;
		}
		/* <1> Normal case of sending CMD Packet */
		/* <1.1> Assign Traffic Class(TC) */
		ucTC = nicTxGetCmdResourceType(prCmdInfo);

		/* <1.2> Check if pending packet or resource was exhausted */
		rStatus = nicTxAcquireResource(prAdapter, ucTC, nicTxGetCmdPageCount(prCmdInfo), TRUE);
		if (rStatus == WLAN_STATUS_RESOURCES) {
#if 0
			DBGLOG(INIT, WARN, "%s: NO Resource for CMD TYPE[%u] ID[0x%02X] SEQ[%u] TC[%u]\n",
			       __func__, prCmdInfo->eCmdType, prCmdInfo->ucCID, prCmdInfo->ucCmdSeqNum, ucTC);
#endif
			break;
		}

		/* Process to pending command queue firest */
		if ((!prCmdInfo->fgSetQuery) || (prCmdInfo->fgNeedResp)) {
			/* command packet which needs further handling upon response */
			/*
			 *  KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING);
			 *  QUEUE_INSERT_TAIL(&(prAdapter->rPendingCmdQueue), (P_QUE_ENTRY_T)prCmdInfo);
			 *  KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING);
			 */
		}
		QUEUE_INSERT_TAIL(prTempCmdQue, (P_QUE_ENTRY_T) prCmdInfo);

		/* <1.4> Set Pending in response to Query Command/Need Response */
		if (rStatus == WLAN_STATUS_SUCCESS) {
			if ((!prCmdInfo->fgSetQuery) ||
			    (prCmdInfo->fgNeedResp) || (prCmdInfo->eCmdType == COMMAND_TYPE_SECURITY_FRAME)) {
				rStatus = WLAN_STATUS_PENDING;
			}
		}
	} while (FALSE);

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_QUE);
	QUEUE_CONCATENATE_QUEUES(&(prAdapter->rTxCmdQueue), prTempCmdQue);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_QUE);

	return rStatus;
}				/* end of wlanSendCommandMthread() */

VOID wlanTxCmdDoneCb(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo)
{

	KAL_SPIN_LOCK_DECLARATION();

	if ((!prCmdInfo->fgSetQuery) || (prCmdInfo->fgNeedResp)) {
#if 0
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING);
		QUEUE_INSERT_TAIL(&prAdapter->rPendingCmdQueue, (P_QUE_ENTRY_T) prCmdInfo);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING);
#endif
	} else {
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_DONE_QUE);
		QUEUE_INSERT_TAIL(&prAdapter->rTxCmdDoneQueue, (P_QUE_ENTRY_T) prCmdInfo);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_DONE_QUE);
	}

	/* call tx thread to work */
	set_bit(GLUE_FLAG_TX_CMD_DONE_BIT, &prAdapter->prGlueInfo->ulFlag);
	wake_up_interruptible(&prAdapter->prGlueInfo->waitq);
}

WLAN_STATUS wlanTxCmdMthread(IN P_ADAPTER_T prAdapter)
{
	QUE_T rTempCmdQue;
	P_QUE_T prTempCmdQue;
	QUE_T rTempCmdDoneQue;
	P_QUE_T prTempCmdDoneQue;
	P_QUE_ENTRY_T prQueueEntry;
	P_CMD_INFO_T prCmdInfo;
/*	P_CMD_ACCESS_REG prCmdAccessReg;
*	P_CMD_ACCESS_REG prEventAccessReg;
*	UINT_32 u4Address;
*/
	UINT_32 u4TxDoneQueueSize;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);

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
	QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
	while (prQueueEntry) {
		prCmdInfo = (P_CMD_INFO_T) prQueueEntry;
		prCmdInfo->pfHifTxCmdDoneCb = wlanTxCmdDoneCb;

		if ((!prCmdInfo->fgSetQuery) || (prCmdInfo->fgNeedResp)) {
			KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING);
			QUEUE_INSERT_TAIL(&(prAdapter->rPendingCmdQueue), (P_QUE_ENTRY_T) prCmdInfo);
			KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING);
		} else {
			QUEUE_INSERT_TAIL(prTempCmdDoneQue, prQueueEntry);
		}

		nicTxCmd(prAdapter, prCmdInfo, TC4_INDEX);

		/* DBGLOG(INIT, INFO,
		 * ("==> TX CMD QID: %d (Q:%d)\n", prCmdInfo->ucCID, prTempCmdQue->u4NumElem));
		 */

		GLUE_DEC_REF_CNT(prAdapter->prGlueInfo->i4TxPendingCmdNum);
		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
	}

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_DONE_QUE);
	QUEUE_CONCATENATE_QUEUES(&prAdapter->rTxCmdDoneQueue, prTempCmdDoneQue);
	u4TxDoneQueueSize = prAdapter->rTxCmdDoneQueue.u4NumElem;
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_DONE_QUE);

	KAL_RELEASE_MUTEX(prAdapter, MUTEX_TX_CMD_CLEAR);

	if (u4TxDoneQueueSize > 0) {
		/* call tx thread to work */
		set_bit(GLUE_FLAG_TX_CMD_DONE_BIT, &prAdapter->prGlueInfo->ulFlag);
		wake_up_interruptible(&prAdapter->prGlueInfo->waitq);
	}

	return WLAN_STATUS_SUCCESS;
}

WLAN_STATUS wlanTxCmdDoneMthread(IN P_ADAPTER_T prAdapter)
{
	QUE_T rTempCmdQue;
	P_QUE_T prTempCmdQue;
	P_QUE_ENTRY_T prQueueEntry;
	P_CMD_INFO_T prCmdInfo;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);

	prTempCmdQue = &rTempCmdQue;
	QUEUE_INITIALIZE(prTempCmdQue);

	/* 4 <1> Move whole list of CMD_INFO to temp queue */
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_DONE_QUE);
	QUEUE_MOVE_ALL(prTempCmdQue, &prAdapter->rTxCmdDoneQueue);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_DONE_QUE);

	/* 4 <2> Dequeue from head and check it is able to be sent */
	QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
	while (prQueueEntry) {
		prCmdInfo = (P_CMD_INFO_T) prQueueEntry;

		if (prCmdInfo->pfCmdDoneHandler)
			prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo, prCmdInfo->pucInfoBuffer);
		/* Not pending cmd, free it after TX succeed! */
		cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
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
VOID wlanClearTxCommandQueue(IN P_ADAPTER_T prAdapter)
{
	QUE_T rTempCmdQue;
	P_QUE_T prTempCmdQue = &rTempCmdQue;
	P_QUE_ENTRY_T prQueueEntry = (P_QUE_ENTRY_T) NULL;
	P_CMD_INFO_T prCmdInfo = (P_CMD_INFO_T) NULL;

	KAL_SPIN_LOCK_DECLARATION();
	QUEUE_INITIALIZE(prTempCmdQue);

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_QUE);
	QUEUE_MOVE_ALL(prTempCmdQue, &prAdapter->rTxCmdQueue);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_QUE);

	QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
	while (prQueueEntry) {
		prCmdInfo = (P_CMD_INFO_T) prQueueEntry;

		if (prCmdInfo->pfCmdTimeoutHandler)
			prCmdInfo->pfCmdTimeoutHandler(prAdapter, prCmdInfo);
		else
			wlanReleaseCommand(prAdapter, prCmdInfo,
				TX_RESULT_QUEUE_CLEARANCE);

		/* Release Tx resource for CMD which resource is allocated but not used */
		nicTxReleaseResource(prAdapter, nicTxGetCmdResourceType(prCmdInfo),
			nicTxGetCmdPageCount(prCmdInfo), TRUE);

		cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
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
VOID wlanClearTxOidCommand(IN P_ADAPTER_T prAdapter)
{
	QUE_T rTempCmdQue;
	P_QUE_T prTempCmdQue = &rTempCmdQue;
	P_QUE_ENTRY_T prQueueEntry = (P_QUE_ENTRY_T) NULL;
	P_CMD_INFO_T prCmdInfo = (P_CMD_INFO_T) NULL;

	KAL_SPIN_LOCK_DECLARATION();
	QUEUE_INITIALIZE(prTempCmdQue);

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_QUE);

	QUEUE_MOVE_ALL(prTempCmdQue, &prAdapter->rTxCmdQueue);

	QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);

	while (prQueueEntry) {
		prCmdInfo = (P_CMD_INFO_T) prQueueEntry;

		if (prCmdInfo->fgIsOid) {

			if (prCmdInfo->pfCmdTimeoutHandler)
				prCmdInfo->pfCmdTimeoutHandler(prAdapter, prCmdInfo);
			else
				wlanReleaseCommand(prAdapter, prCmdInfo, TX_RESULT_QUEUE_CLEARANCE);

			/* Release Tx resource for CMD which resource is allocated but not used */
			nicTxReleaseResource(prAdapter, nicTxGetCmdResourceType(prCmdInfo),
				nicTxGetCmdPageCount(prCmdInfo), TRUE);

			cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

		} else {
			QUEUE_INSERT_TAIL(&prAdapter->rTxCmdQueue, prQueueEntry);
		}

		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
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
VOID wlanClearTxCommandDoneQueue(IN P_ADAPTER_T prAdapter)
{
	QUE_T rTempCmdDoneQue;
	P_QUE_T prTempCmdDoneQue = &rTempCmdDoneQue;
	P_QUE_ENTRY_T prQueueEntry = (P_QUE_ENTRY_T) NULL;
	P_CMD_INFO_T prCmdInfo = (P_CMD_INFO_T) NULL;

	KAL_SPIN_LOCK_DECLARATION();
	QUEUE_INITIALIZE(prTempCmdDoneQue);

	/* 4 <1> Move whole list of CMD_INFO to temp queue */
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_DONE_QUE);
	QUEUE_MOVE_ALL(prTempCmdDoneQue, &prAdapter->rTxCmdDoneQueue);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_DONE_QUE);

	/* 4 <2> Dequeue from head and check it is able to be sent */
	QUEUE_REMOVE_HEAD(prTempCmdDoneQue, prQueueEntry, P_QUE_ENTRY_T);
	while (prQueueEntry) {
		prCmdInfo = (P_CMD_INFO_T) prQueueEntry;

		if (prCmdInfo->pfCmdDoneHandler)
			prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo, prCmdInfo->pucInfoBuffer);
		/* Not pending cmd, free it after TX succeed! */
		cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

		QUEUE_REMOVE_HEAD(prTempCmdDoneQue, prQueueEntry, P_QUE_ENTRY_T);
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
VOID wlanClearDataQueue(IN P_ADAPTER_T prAdapter)
{
	if (HAL_IS_TX_DIRECT())
		nicTxDirectClearHifQ(prAdapter);
	else {
#if CFG_FIX_2_TX_PORT
		QUE_T qDataPort0, qDataPort1;
		P_QUE_T prDataPort0, prDataPort1;
		P_MSDU_INFO_T prMsduInfo;

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
		nicTxReleaseMsduResource(prAdapter, (P_MSDU_INFO_T) QUEUE_GET_HEAD(prDataPort0));
		nicTxReleaseMsduResource(prAdapter, (P_MSDU_INFO_T) QUEUE_GET_HEAD(prDataPort1));

		/* <3> Return sk buffer */
		nicTxReturnMsduInfo(prAdapter, (P_MSDU_INFO_T) QUEUE_GET_HEAD(prDataPort0));
		nicTxReturnMsduInfo(prAdapter, (P_MSDU_INFO_T) QUEUE_GET_HEAD(prDataPort1));

		/* <4> Clear pending MSDU info in data done queue */
		KAL_ACQUIRE_MUTEX(prAdapter, MUTEX_TX_DATA_DONE_QUE);
		while (QUEUE_IS_NOT_EMPTY(&prAdapter->rTxDataDoneQueue)) {
			QUEUE_REMOVE_HEAD(&prAdapter->rTxDataDoneQueue, prMsduInfo, P_MSDU_INFO_T);

			nicTxFreePacket(prAdapter, prMsduInfo, FALSE);
			nicTxReturnMsduInfo(prAdapter, prMsduInfo);
		}
		KAL_RELEASE_MUTEX(prAdapter, MUTEX_TX_DATA_DONE_QUE);
#else

		QUE_T qDataPort[TX_PORT_NUM];
		P_QUE_T prDataPort[TX_PORT_NUM];
		P_MSDU_INFO_T prMsduInfo;
		INT_32 i;

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
			nicTxReleaseMsduResource(prAdapter, (P_MSDU_INFO_T) QUEUE_GET_HEAD(prDataPort[i]));
			nicTxReturnMsduInfo(prAdapter, (P_MSDU_INFO_T) QUEUE_GET_HEAD(prDataPort[i]));
		}

		/* <3> Clear pending MSDU info in data done queue */
		KAL_ACQUIRE_MUTEX(prAdapter, MUTEX_TX_DATA_DONE_QUE);
		while (QUEUE_IS_NOT_EMPTY(&prAdapter->rTxDataDoneQueue)) {
			QUEUE_REMOVE_HEAD(&prAdapter->rTxDataDoneQueue, prMsduInfo, P_MSDU_INFO_T);

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
VOID wlanClearRxToOsQueue(IN P_ADAPTER_T prAdapter)
{
	QUE_T rTempRxQue;
	P_QUE_T prTempRxQue = &rTempRxQue;
	P_QUE_ENTRY_T prQueueEntry = (P_QUE_ENTRY_T) NULL;

	KAL_SPIN_LOCK_DECLARATION();
	QUEUE_INITIALIZE(prTempRxQue);

	/* 4 <1> Move whole list of CMD_INFO to temp queue */
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_TO_OS_QUE);
	QUEUE_MOVE_ALL(prTempRxQue, &prAdapter->rRxQueue);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_TO_OS_QUE);

	/* 4 <2> Remove all skbuf */
	QUEUE_REMOVE_HEAD(prTempRxQue, prQueueEntry, P_QUE_ENTRY_T);
	while (prQueueEntry) {
		kalRxIndicateOnePkt(prAdapter->prGlueInfo, (PVOID) GLUE_GET_PKT_DESCRIPTOR(prQueueEntry));
		QUEUE_REMOVE_HEAD(prTempRxQue, prQueueEntry, P_QUE_ENTRY_T);
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
VOID wlanReleaseCommand(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN ENUM_TX_RESULT_CODE_T rTxDoneStatus)
{
	P_TX_CTRL_T prTxCtrl;
	P_MSDU_INFO_T prMsduInfo;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	prTxCtrl = &prAdapter->rTxCtrl;

	switch (prCmdInfo->eCmdType) {
	case COMMAND_TYPE_GENERAL_IOCTL:
	case COMMAND_TYPE_NETWORK_IOCTL:
		DBGLOG(INIT, INFO, "Free CMD: ID[0x%x] SeqNum[%u] OID[%u]\n",
		       prCmdInfo->ucCID, prCmdInfo->ucCmdSeqNum, prCmdInfo->fgIsOid);

		if (prCmdInfo->fgIsOid) {
			kalOidComplete(prAdapter->prGlueInfo,
				       prCmdInfo->fgSetQuery, prCmdInfo->u4SetInfoLen, WLAN_STATUS_FAILURE);
		}
		break;

	case COMMAND_TYPE_SECURITY_FRAME:
	case COMMAND_TYPE_MANAGEMENT_FRAME:
		prMsduInfo = prCmdInfo->prMsduInfo;

		if (prCmdInfo->eCmdType == COMMAND_TYPE_SECURITY_FRAME) {
			kalSecurityFrameSendComplete(prAdapter->prGlueInfo, prCmdInfo->prPacket, WLAN_STATUS_FAILURE);
			/* Avoid skb multiple free */
			prMsduInfo->prPacket = NULL;
		}

		DBGLOG(INIT, INFO,
		       "Free %s Frame: BSS[%u] WIDX:PID[%u:%u] SEQ[%u] STA[%u] RSP[%u] CMDSeq[%u]\n",
		       prCmdInfo->eCmdType == COMMAND_TYPE_SECURITY_FRAME ? "SEC" : "MGMT",
		       prMsduInfo->ucBssIndex,
		       prMsduInfo->ucWlanIndex,
		       prMsduInfo->ucPID,
		       prMsduInfo->ucTxSeqNum,
		       prMsduInfo->ucStaRecIndex, prMsduInfo->pfTxDoneHandler ? TRUE : FALSE, prCmdInfo->ucCmdSeqNum);

		/* invoke callbacks */
		if (prMsduInfo->pfTxDoneHandler != NULL)
			prMsduInfo->pfTxDoneHandler(prAdapter, prMsduInfo, rTxDoneStatus);

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
* \brief This function will search the CMD Queue to look for the pending OID and
*        compelete it immediately when system request a reset.
*
* \param prAdapter  ointer of Adapter Data Structure
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID wlanReleasePendingOid(IN P_ADAPTER_T prAdapter, IN ULONG ulParamPtr)
{
	P_QUE_T prCmdQue;
	QUE_T rTempCmdQue;
	P_QUE_T prTempCmdQue = &rTempCmdQue;
	P_QUE_ENTRY_T prQueueEntry = (P_QUE_ENTRY_T) NULL;
	P_CMD_INFO_T prCmdInfo = (P_CMD_INFO_T) NULL;

	KAL_SPIN_LOCK_DECLARATION();

	DEBUGFUNC("wlanReleasePendingOid");

	ASSERT(prAdapter);

	if (prAdapter->prGlueInfo->ulFlag & GLUE_FLAG_HALT) {
		DBGLOG(OID, TRACE, "%s stopped! Releasing pending OIDs ..\n", KAL_GET_CURRENT_THREAD_NAME());
	} else {
		DBGLOG(OID, ERROR, "OID Timeout! Releasing pending OIDs ..\n");
		prAdapter->ucOidTimeoutCount++;

		if (prAdapter->ucOidTimeoutCount >= WLAN_OID_NO_ACK_THRESHOLD) {
			if (!prAdapter->fgIsChipNoAck) {
				DBGLOG(INIT, WARN,
				       "No response from chip for %u times, set NoAck flag!\n",
				       prAdapter->ucOidTimeoutCount);
			}

			prAdapter->fgIsChipNoAck = TRUE;
#if CFG_CHIP_RESET_SUPPORT
			DBGLOG(HAL, ERROR, "fgIsChipNoAck = %d\n",
						prAdapter->fgIsChipNoAck);
			glResetTrigger(prAdapter);
#endif
		}
		set_bit(GLUE_FLAG_HIF_PRT_HIF_DBG_INFO_BIT, &(prAdapter->prGlueInfo->ulFlag));
	}

	do {
#if CFG_SUPPORT_MULTITHREAD
		KAL_ACQUIRE_MUTEX(prAdapter, MUTEX_TX_CMD_CLEAR);
#endif

		/* 1: Clear Pending OID in prAdapter->rPendingCmdQueue */
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING);

		prCmdQue = &prAdapter->rPendingCmdQueue;
		QUEUE_MOVE_ALL(prTempCmdQue, prCmdQue);

		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
		while (prQueueEntry) {
			prCmdInfo = (P_CMD_INFO_T) prQueueEntry;

			if (prCmdInfo->fgIsOid) {
				DBGLOG(OID, INFO, "Clear pending OID CMD ID[0x%02X] SEQ[%u] buf[0x%p]\n",
				       prCmdInfo->ucCID, prCmdInfo->ucCmdSeqNum, prCmdInfo->pucInfoBuffer);

				if (prCmdInfo->pfCmdTimeoutHandler) {
					prCmdInfo->pfCmdTimeoutHandler(prAdapter, prCmdInfo);
				} else {
					kalOidComplete(prAdapter->prGlueInfo,
						       prCmdInfo->fgSetQuery, 0, WLAN_STATUS_FAILURE);
				}

				KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING);
				nicTxCancelSendingCmd(prAdapter, prCmdInfo);
				KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING);

				cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
			} else {
				QUEUE_INSERT_TAIL(prCmdQue, prQueueEntry);
			}

			QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
		}

		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING);

#if CFG_SUPPORT_MULTITHREAD
		/* Clear pending OID in main_thread to hif_thread command queue */
		wlanClearTxOidCommand(prAdapter);
#endif

		/* 2: Clear pending OID in glue layer command queue */
		kalOidCmdClearance(prAdapter->prGlueInfo);

		/* 3: Clear pending OID queued in pvOidEntry with REQ_FLAG_OID set */
		kalOidClearance(prAdapter->prGlueInfo);

#if CFG_SUPPORT_MULTITHREAD
		KAL_RELEASE_MUTEX(prAdapter, MUTEX_TX_CMD_CLEAR);
#endif
	} while (FALSE);

	DBGLOG(OID, INFO, "End of Release pending OID\n");


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
VOID wlanReleasePendingCMDbyBssIdx(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex)
{
#if 0

	P_QUE_T prCmdQue;
	QUE_T rTempCmdQue;
	P_QUE_T prTempCmdQue = &rTempCmdQue;
	P_QUE_ENTRY_T prQueueEntry = (P_QUE_ENTRY_T) NULL;
	P_CMD_INFO_T prCmdInfo = (P_CMD_INFO_T) NULL;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);

	do {
		/* 1: Clear Pending OID in prAdapter->rPendingCmdQueue */
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING);

		prCmdQue = &prAdapter->rPendingCmdQueue;
		QUEUE_MOVE_ALL(prTempCmdQue, prCmdQue);

		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
		while (prQueueEntry) {
			prCmdInfo = (P_CMD_INFO_T) prQueueEntry;

			DBGLOG(P2P, TRACE, "Pending CMD for BSS:%d\n", prCmdInfo->ucBssIndex);

			if (prCmdInfo->ucBssIndex == ucBssIndex) {
				if (prCmdInfo->pfCmdTimeoutHandler) {
					prCmdInfo->pfCmdTimeoutHandler(prAdapter, prCmdInfo);
				} else if (prCmdInfo->fgIsOid) {
					kalOidComplete(prAdapter->prGlueInfo,
						       prCmdInfo->fgSetQuery, 0, WLAN_STATUS_FAILURE);
				}

				cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
			} else {
				QUEUE_INSERT_TAIL(prCmdQue, prQueueEntry);
			}

			QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
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
VOID wlanReturnPacketDelaySetupTimeout(IN P_ADAPTER_T prAdapter, IN ULONG ulParamPtr)
{
	P_RX_CTRL_T prRxCtrl;
	P_SW_RFB_T prSwRfb = NULL;

	KAL_SPIN_LOCK_DECLARATION();
	WLAN_STATUS status = WLAN_STATUS_SUCCESS;
	P_QUE_T prQueList;

	ASSERT(prAdapter);

	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);

	prQueList = &prRxCtrl->rIndicatedRfbList;
	DBGLOG(RX, WARN, "%s: IndicatedRfbList num = %u\n", __func__, prQueList->u4NumElem);

	while (QUEUE_IS_NOT_EMPTY(&prRxCtrl->rIndicatedRfbList)) {
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);
		QUEUE_REMOVE_HEAD(&prRxCtrl->rIndicatedRfbList, prSwRfb, P_SW_RFB_T);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);

		status = nicRxSetupRFB(prAdapter, prSwRfb);
		nicRxReturnRFB(prAdapter, prSwRfb);

		if (status != WLAN_STATUS_SUCCESS)
			break;
	}

	if (status != WLAN_STATUS_SUCCESS) {
		DBGLOG(RX, WARN, "Restart ReturnIndicatedRfb Timer (%u)\n", RX_RETURN_INDICATED_RFB_TIMEOUT_SEC);
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
VOID wlanReturnPacket(IN P_ADAPTER_T prAdapter, IN PVOID pvPacket)
{
	P_RX_CTRL_T prRxCtrl;
	P_SW_RFB_T prSwRfb = NULL;

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
	QUEUE_REMOVE_HEAD(&prRxCtrl->rIndicatedRfbList, prSwRfb, P_SW_RFB_T);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);
	if (!prSwRfb) {
		DBGLOG(RX, WARN, "No free SwRfb!\n");
		return;
	}

	if (nicRxSetupRFB(prAdapter, prSwRfb)) {
		DBGLOG(RX, WARN, "Cannot allocate packet buffer for SwRfb!\n");
		if (!timerPendingTimer(&prAdapter->rPacketDelaySetupTimer)) {
			DBGLOG(RX, WARN, "Start ReturnIndicatedRfb Timer (%u)\n", RX_RETURN_INDICATED_RFB_TIMEOUT_SEC);
			cnmTimerStartTimer(prAdapter, &prAdapter->rPacketDelaySetupTimer,
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
	if (prAdapter->u4PsCurrentMeasureEn &&
	    (prAdapter->prGlueInfo->eParamMediaStateIndicated == PARAM_MEDIA_STATE_CONNECTED)) {
		/* note: return WLAN_STATUS_FAILURE or
		 * WLAN_STATUS_SUCCESS for blocking OIDs during current measurement ??
		 */
		return WLAN_STATUS_SUCCESS;
	}
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
	if (prAdapter->u4PsCurrentMeasureEn &&
	    (prAdapter->prGlueInfo->eParamMediaStateIndicated == PARAM_MEDIA_STATE_CONNECTED)) {
		/* note: return WLAN_STATUS_FAILURE or WLAN_STATUS_SUCCESS
		 * for blocking OIDs during current measurement ??
		 */
		return WLAN_STATUS_SUCCESS;
	}
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
* \brief This function is called to send out CMD_ID_DUMMY command packet
*
* \param[IN] prAdapter        Pointer to the Adapter structure.
*
* \return WLAN_STATUS_SUCCESS
* \return WLAN_STATUS_FAILURE
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanSendDummyCmd(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgIsReqTxRsrc)
{
	WLAN_STATUS status = WLAN_STATUS_SUCCESS;
	P_GLUE_INFO_T prGlueInfo;
	P_CMD_INFO_T prCmdInfo;
	P_WIFI_CMD_T prWifiCmd;

	ASSERT(prAdapter);

	prGlueInfo = prAdapter->prGlueInfo;

	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, CMD_HDR_SIZE);
	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}

	prCmdInfo->eCmdType = COMMAND_TYPE_GENERAL_IOCTL;
	prCmdInfo->u2InfoBufLen = (UINT_16) CMD_HDR_SIZE;
	prCmdInfo->pfCmdDoneHandler = NULL;
	prCmdInfo->pfCmdTimeoutHandler = NULL;
	prCmdInfo->fgIsOid = TRUE;
	prCmdInfo->ucCID = CMD_ID_DUMMY_RSV;
	prCmdInfo->fgSetQuery = TRUE;
	prCmdInfo->fgNeedResp = FALSE;
	prCmdInfo->ucCmdSeqNum = 0;
	prCmdInfo->u4SetInfoLen = 0;

	prWifiCmd = (P_WIFI_CMD_T) (prCmdInfo->pucInfoBuffer);
	prWifiCmd->u2TxByteCount = prCmdInfo->u2InfoBufLen;
	prWifiCmd->u2PQ_ID = CMD_PQ_ID;
	prWifiCmd->ucPktTypeID = CMD_PACKET_TYPE_ID;
	prWifiCmd->ucCID = prCmdInfo->ucCID;
	prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
	prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;

	if (fgIsReqTxRsrc) {
		if (wlanSendCommand(prAdapter, prCmdInfo) != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, "Fail to transmit CMD_ID_DUMMY command\n");
			status = WLAN_STATUS_FAILURE;
		}
	} else {
		if (nicTxCmd(prAdapter, prCmdInfo, TC4_INDEX) != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, "Fail to transmit CMD_ID_DUMMY command\n");
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
	prCmdInfo->ucCmdSeqNum = ucCmdSeqNum;
	prCmdInfo->u4SetInfoLen = sizeof(CMD_NIC_POWER_CTRL);

	/* 2.3 Setup WIFI_CMD_T */
	prWifiCmd = (P_WIFI_CMD_T) (prCmdInfo->pucInfoBuffer);
	prWifiCmd->u2TxByteCount = prCmdInfo->u2InfoBufLen;
	prWifiCmd->u2PQ_ID = CMD_PQ_ID;
	prWifiCmd->ucPktTypeID = CMD_PACKET_TYPE_ID;
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
		if (nicTxAcquireResource(prAdapter, ucTC, nicTxGetCmdPageCount(prCmdInfo), TRUE)
			== WLAN_STATUS_RESOURCES) {
			if (nicTxPollingResource(prAdapter, ucTC) != WLAN_STATUS_SUCCESS) {
				DBGLOG(INIT, ERROR, "Fail to get TX resource return within timeout\n");
				status = WLAN_STATUS_FAILURE;
				prAdapter->fgIsChipNoAck = TRUE;
#if CFG_CHIP_RESET_SUPPORT
				DBGLOG(HAL, ERROR, "fgIsChipNoAck = %d\n",
						prAdapter->fgIsChipNoAck);
				glResetTrigger(prAdapter);
#endif
				break;
			}
			continue;
		}
		break;
	};

	/* 3.2 Send CMD Info Packet */
	if (nicTxCmd(prAdapter, prCmdInfo, ucTC) != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR, "Fail to transmit CMD_NIC_POWER_CTRL command\n");
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
* \brief This function is called to set g_fgKeepFullPwr flag in firmware
*
* \param[IN] prAdapter        Pointer to the Adapter structure.
* \param[IN] fgEnable         Boolean of enable
*                             True: wlan stays awake and keeps working in full power state
*                             False: wlan may go to sleep and consumes less power.
*
* \return WLAN_STATUS_SUCCESS
* \return WLAN_STATUS_FAILURE
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanKeepFullPwr(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgEnable)
{
	struct CMD_KEEP_FULL_PWR_T rCmdKeepFullPwr;

	ASSERT(prAdapter);

	rCmdKeepFullPwr.ucEnable = fgEnable;
	DBGLOG(HAL, STATE, "KeepFullPwr: %d\n", rCmdKeepFullPwr.ucEnable);

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_KEEP_FULL_PWR, TRUE, FALSE, FALSE, NULL, NULL,
				   sizeof(struct CMD_KEEP_FULL_PWR_T), (PUINT_8)&rCmdKeepFullPwr, NULL, 0);
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
VOID wlanImageSectionGetFwInfo(IN P_ADAPTER_T prAdapter,
			       IN PVOID pvFwImageMapFile, IN UINT_32 u4FwImageFileLength,
			       IN UINT_8 ucTotSecNum, IN UINT_8 ucCurSecNum, IN ENUM_IMG_DL_IDX_T eDlIdx,
			       OUT PUINT_32 pu4StartOffset, OUT PUINT_32 pu4Addr, OUT PUINT_32 pu4Len,
			       OUT PUINT_32 pu4DataMode)
{
	UINT_32 u4DataMode = 0;
	fw_image_tailer_t *prFwHead;
	tailer_format_t *prTailer;

	prFwHead = (fw_image_tailer_t *) (pvFwImageMapFile + u4FwImageFileLength - sizeof(fw_image_tailer_t));
	if (ucTotSecNum == 1)
		prTailer = &prFwHead->dlm_info;
	else
		prTailer = &prFwHead->ilm_info;


	prTailer = &prTailer[ucCurSecNum];

	*pu4StartOffset = 0;
	*pu4Addr = prTailer->addr;
	*pu4Len = (prTailer->len + LEN_4_BYTE_CRC);
	if (prTailer->feature_set & DOWNLOAD_CONFIG_ENCRYPTION_MODE) {
		u4DataMode |= DOWNLOAD_CONFIG_RESET_OPTION;
		u4DataMode |= (prTailer->feature_set & DOWNLOAD_CONFIG_KEY_INDEX_MASK);
		u4DataMode |= DOWNLOAD_CONFIG_ENCRYPTION_MODE;
	}

	if (eDlIdx == IMG_DL_IDX_CR4_FW)
		u4DataMode |= DOWNLOAD_CONFIG_WORKING_PDA_OPTION;

#if CFG_ENABLE_FW_DOWNLOAD_ACK
	u4DataMode |= DOWNLOAD_CONFIG_ACK_OPTION;	/* ACK needed */
#endif

	*pu4DataMode = u4DataMode;

	/* Dump image information */
	if (ucCurSecNum == 0) {
		DBGLOG(INIT, INFO, "%s INFO: chip_info[%u:E%u] feature[0x%02X]\n",
		       (eDlIdx == IMG_DL_IDX_N9_FW) ? "N9" : "CR4", prTailer->chip_info,
		       prTailer->eco_code + 1, prTailer->feature_set);
		DBGLOG(INIT, INFO, "date[%s] version[%c%c%c%c%c%c%c%c%c%c]\n",
		       prTailer->ram_built_date,
		       prTailer->ram_version[0], prTailer->ram_version[1],
		       prTailer->ram_version[2], prTailer->ram_version[3],
		       prTailer->ram_version[4], prTailer->ram_version[5],
		       prTailer->ram_version[6], prTailer->ram_version[7],
		       prTailer->ram_version[8], prTailer->ram_version[9]);
	}

	/* Backup to FW version info */
	if (eDlIdx == IMG_DL_IDX_N9_FW)
		kalMemCopy(&prAdapter->rVerInfo.rN9tailer, prTailer, sizeof(tailer_format_t));
	else
		kalMemCopy(&prAdapter->rVerInfo.rCR4tailer, prTailer, sizeof(tailer_format_t));
}
#if CFG_SUPPORT_COMPRESSION_FW_OPTION
VOID wlanImageSectionGetCompressFwInfo(IN P_ADAPTER_T prAdapter,
IN PVOID pvFwImageMapFile, IN UINT_32 u4FwImageFileLength, IN UINT_8 ucTotSecNum, IN UINT_8 ucCurSecNum,
IN ENUM_IMG_DL_IDX_T eDlIdx, OUT PUINT_32 pu4StartOffset, OUT PUINT_32 pu4Addr, OUT PUINT_32 pu4Len,
OUT PUINT_32 pu4DataMode, OUT PUINT_32 pu4BlockSize, OUT PUINT_32 pu4CRC, OUT PUINT_32 pu4UncompressedLength)
{
	UINT_32 u4DataMode = 0;
	fw_image_tailer_t_2 *prFwHead;
	tailer_format_t_2 *prTailer;

	prFwHead = (fw_image_tailer_t_2 *) (pvFwImageMapFile + u4FwImageFileLength - sizeof(fw_image_tailer_t_2));
	if (ucTotSecNum == 1)
		prTailer = &prFwHead->dlm_info;
	else
		prTailer = &prFwHead->ilm_info;

	prTailer = &prTailer[ucCurSecNum];

	*pu4StartOffset = 0;
	*pu4Addr = prTailer->addr;
	*pu4Len = (prTailer->len);
	*pu4BlockSize = (prTailer->block_size);
	*pu4CRC = (prTailer->crc);
	*pu4UncompressedLength = (prTailer->real_size);
	if (prTailer->feature_set & DOWNLOAD_CONFIG_ENCRYPTION_MODE) {
		u4DataMode |= DOWNLOAD_CONFIG_RESET_OPTION;
		u4DataMode |= (prTailer->feature_set & DOWNLOAD_CONFIG_KEY_INDEX_MASK);
		u4DataMode |= DOWNLOAD_CONFIG_ENCRYPTION_MODE;
	}
	if (eDlIdx == IMG_DL_IDX_CR4_FW)
		u4DataMode |= DOWNLOAD_CONFIG_WORKING_PDA_OPTION;

#if CFG_ENABLE_FW_DOWNLOAD_ACK
	u4DataMode |= DOWNLOAD_CONFIG_ACK_OPTION;	/* ACK needed */
#endif

	*pu4DataMode = u4DataMode;

	/* Dump image information */
	if (ucCurSecNum == 0) {
		DBGLOG(INIT, INFO, "%s INFO: chip_info[%u:E%u] feature[0x%02X]\n",
			(eDlIdx == IMG_DL_IDX_N9_FW) ? "N9" : "CR4", prTailer->chip_info,
			prTailer->eco_code, prTailer->feature_set);
		DBGLOG(INIT, INFO, "date[%s] version[%c%c%c%c%c%c%c%c%c%c]\n", prTailer->ram_built_date,
			prTailer->ram_version[0], prTailer->ram_version[1],
			prTailer->ram_version[2], prTailer->ram_version[3],
			prTailer->ram_version[4], prTailer->ram_version[5],
			prTailer->ram_version[6], prTailer->ram_version[7],
			prTailer->ram_version[8], prTailer->ram_version[9]);
	}
    /* Backup to FW version info */
	if (eDlIdx == IMG_DL_IDX_N9_FW) {
		kalMemCopy(&prAdapter->rVerInfo.rN9Compressedtailer, prTailer, sizeof(tailer_format_t_2));
		prAdapter->rVerInfo.fgIsN9CompressedFW = TRUE;
	} else {
		kalMemCopy(&prAdapter->rVerInfo.rCR4Compressedtailer, prTailer, sizeof(tailer_format_t_2));
		prAdapter->rVerInfo.fgIsCR4CompressedFW = TRUE;
	}
}
#endif
VOID wlanImageSectionGetPatchInfo(IN P_ADAPTER_T prAdapter,
				  IN PVOID pvFwImageMapFile, IN UINT_32 u4FwImageFileLength,
				  IN UINT_8 ucTotSecNum, IN UINT_8 ucCurSecNum, IN ENUM_IMG_DL_IDX_T eDlIdx,
				  OUT PUINT_32 pu4StartOffset, OUT PUINT_32 pu4Addr, OUT PUINT_32 pu4Len,
				  OUT PUINT_32 pu4DataMode)
{
	P_PATCH_FORMAT_T prPatchFormat;
	UINT_32 u4DataMode = 0;
	UINT_8 aucBuffer[32];
	struct mt66xx_chip_info *prChipInfo = prAdapter->chip_info;

	prPatchFormat = (P_PATCH_FORMAT_T) pvFwImageMapFile;

	*pu4StartOffset = offsetof(PATCH_FORMAT_T, ucPatchImage);
	*pu4Addr = prChipInfo->patch_addr;
	*pu4Len = u4FwImageFileLength - offsetof(PATCH_FORMAT_T, ucPatchImage);

#if CFG_ENABLE_FW_DOWNLOAD_ACK
	u4DataMode |= DOWNLOAD_CONFIG_ACK_OPTION;	/* ACK needed */
#endif
	*pu4DataMode = u4DataMode;

	/* Dump image information */
	kalStrnCpy(aucBuffer, prPatchFormat->aucPlatform, 4);
	aucBuffer[4] = '\0';
	DBGLOG(INIT, INFO, "PATCH INFO: platform[%s] HW/SW ver[0x%04X] ver[0x%04X]\n",
	       aucBuffer, prPatchFormat->u4SwHwVersion, prPatchFormat->u4PatchVersion);

	kalStrnCpy(aucBuffer, prPatchFormat->aucBuildDate, 16);
	aucBuffer[16] = '\0';
	DBGLOG(INIT, INFO, "date[%s]\n", aucBuffer);

	/* Backup to FW version info */
	kalMemCopy(&prAdapter->rVerInfo.rPatchHeader, prPatchFormat, sizeof(PATCH_FORMAT_T));
}

VOID wlanImageSectionGetInfo(IN P_ADAPTER_T prAdapter,
			     IN PVOID pvFwImageMapFile, IN UINT_32 u4FwImageFileLength,
			     IN UINT_8 ucTotSecNum, IN UINT_8 ucCurSecNum, IN ENUM_IMG_DL_IDX_T eDlIdx,
			     OUT PUINT_32 pu4StartOffset, OUT PUINT_32 pu4Addr, OUT PUINT_32 pu4Len,
			     OUT PUINT_32 pu4DataMode)
{
	if (eDlIdx == IMG_DL_IDX_PATCH) {
		wlanImageSectionGetPatchInfo(prAdapter, pvFwImageMapFile, u4FwImageFileLength,
					     ucTotSecNum, ucCurSecNum, eDlIdx, pu4StartOffset, pu4Addr, pu4Len,
					     pu4DataMode);
	} else {
		wlanImageSectionGetFwInfo(prAdapter, pvFwImageMapFile, u4FwImageFileLength,
					  ucTotSecNum, ucCurSecNum, eDlIdx, pu4StartOffset, pu4Addr, pu4Len,
					  pu4DataMode);
	}
}
#if CFG_SUPPORT_COMPRESSION_FW_OPTION

BOOLEAN wlanImageSectionCheckFwCompressInfo(IN P_ADAPTER_T prAdapter,
	IN PVOID pvFwImageMapFile, IN UINT_32 u4FwImageFileLength, IN ENUM_IMG_DL_IDX_T eDlIdx) {
	UINT_8 ucCompression;
	fw_image_tailer_check *prCheckInfo;

	if (eDlIdx == IMG_DL_IDX_PATCH)
		return FALSE;

	prCheckInfo = (fw_image_tailer_check *)
		(pvFwImageMapFile + u4FwImageFileLength - sizeof(fw_image_tailer_check));
	DBGLOG(INIT, INFO, "feature_set %d\n", prCheckInfo->feature_set);
	ucCompression = (UINT_8)((prCheckInfo->feature_set & COMPRESSION_OPTION_MASK)
					>> COMPRESSION_OPTION_OFFSET);
	DBGLOG(INIT, INFO, "Compressed Check INFORMATION %d\n", ucCompression);
	if (ucCompression == 1) {
		DBGLOG(INIT, INFO, "Compressed FW\n");
		return TRUE;
	}
	return FALSE;
}


WLAN_STATUS wlanImageSectionDownloadStage(IN P_ADAPTER_T prAdapter, IN PVOID pvFwImageMapFile,
	IN UINT_32 u4FwImageFileLength, IN UINT_8 ucSectionNumber, IN ENUM_IMG_DL_IDX_T eDlIdx,
	OUT PUINT_8 pucIsCompressed, OUT P_INIT_CMD_WIFI_DECOMPRESSION_START prFwImageInFo)
{
	UINT_32 u4ImgSecSize;
	UINT_32 j, i;
	INT_32  i4TotalLen;
	UINT_32 u4FileOffset = 0;
	UINT_32 u4StartOffset = 0;
	UINT_32 u4DataMode = 0;
	UINT_32 u4Addr, u4Len, u4BlockSize, u4CRC, u4UnCompressedLength;
	WLAN_STATUS u4Status = WLAN_STATUS_SUCCESS;
	PUINT_8 pucSecBuf, pucStartPtr;
	UINT_32 u4offset = 0, u4ChunkSize;
	/* 3a. parse file header for decision of divided firmware download or not */
	for (i = 0; i < ucSectionNumber; ++i) {
		if (wlanImageSectionCheckFwCompressInfo(prAdapter, pvFwImageMapFile,
			u4FwImageFileLength, eDlIdx) == TRUE){
			wlanImageSectionGetCompressFwInfo(prAdapter, pvFwImageMapFile,
					u4FwImageFileLength, ucSectionNumber, i, eDlIdx,
					&u4StartOffset, &u4Addr, &u4Len, &u4DataMode,
					&u4BlockSize, &u4CRC, &u4UnCompressedLength);
			u4offset = 0;
			if (i == 0) {
				prFwImageInFo->u4BlockSize = u4BlockSize;
				prFwImageInFo->u4Region1Address = u4Addr;
				prFwImageInFo->u4Region1CRC = u4CRC;
				prFwImageInFo->u4Region1length = u4UnCompressedLength;
			} else {
				prFwImageInFo->u4Region2Address = u4Addr;
				prFwImageInFo->u4Region2CRC = u4CRC;
				prFwImageInFo->u4Region2length = u4UnCompressedLength;
			}
		    i4TotalLen = u4Len;
			DBGLOG(INIT, INFO, "DL Offset[%u] addr[0x%08x] len[%u] datamode[0x%08x]\n",
					u4FileOffset, u4Addr, u4Len, u4DataMode);
			DBGLOG(INIT, INFO, "DL BLOCK[%u]  COMlen[%u] CRC[%u]\n",
					u4BlockSize, u4UnCompressedLength, u4CRC);
			pucStartPtr = (PUINT_8)pvFwImageMapFile + u4StartOffset;
			while (i4TotalLen) {
				u4ChunkSize =  *((unsigned int *)(pucStartPtr+u4FileOffset));
				u4FileOffset += 4;
				DBGLOG(INIT, INFO, "Downloaded Length %d! Addr %x\n", i4TotalLen, u4Addr + u4offset);
				DBGLOG(INIT, INFO, "u4ChunkSize Length %d!\n", u4ChunkSize);
				if (wlanImageSectionConfig(prAdapter, (u4Addr + u4offset), u4ChunkSize,
					u4DataMode, eDlIdx) != WLAN_STATUS_SUCCESS) {
					DBGLOG(INIT, ERROR, "Firmware download configuration failed!\n");
					u4Status = WLAN_STATUS_FAILURE;
					break;
				}
				for (j = 0; j < u4ChunkSize; j += CMD_PKT_SIZE_FOR_IMAGE) {
					if (j + CMD_PKT_SIZE_FOR_IMAGE < u4ChunkSize)
						u4ImgSecSize = CMD_PKT_SIZE_FOR_IMAGE;
					else
						u4ImgSecSize = u4ChunkSize - j;
					pucSecBuf = (PUINT_8)pucStartPtr + u4FileOffset + j;
					if (wlanImageSectionDownload(prAdapter, u4ImgSecSize, pucSecBuf)
						!= WLAN_STATUS_SUCCESS) {
						DBGLOG(INIT, ERROR, "Firmware scatter download failed!\n");
						u4Status = WLAN_STATUS_FAILURE;
						break;
					}
				}
/* escape from loop if any pending error occurs */
				if (u4Status == WLAN_STATUS_FAILURE)
					break;
				i4TotalLen -= u4ChunkSize;
				u4offset += u4BlockSize;
				u4FileOffset += u4ChunkSize;
				if (i4TotalLen < 0) {
					DBGLOG(INIT, ERROR, "Firmware scatter download failed!\n");
					u4Status = WLAN_STATUS_FAILURE;
					break;
				}
			}
			*pucIsCompressed = TRUE;
		} else {
				wlanImageSectionGetInfo(prAdapter, pvFwImageMapFile,
					u4FwImageFileLength, ucSectionNumber, i, eDlIdx,
					&u4StartOffset, &u4Addr, &u4Len, &u4DataMode);
				pucStartPtr = (PUINT_8)pvFwImageMapFile + u4StartOffset;

				DBGLOG(INIT, INFO, "DL Offset[%u] addr[0x%08x] len[%u] datamode[0x%08x]\n",
					u4FileOffset, u4Addr, u4Len, u4DataMode);

				if (wlanImageSectionConfig(prAdapter, u4Addr, u4Len, u4DataMode, eDlIdx)
								!= WLAN_STATUS_SUCCESS) {
					DBGLOG(INIT, ERROR, "Firmware download configuration failed!\n");

					u4Status = WLAN_STATUS_FAILURE;
					break;
				}
				for (j = 0; j < u4Len; j += CMD_PKT_SIZE_FOR_IMAGE) {
					if (j + CMD_PKT_SIZE_FOR_IMAGE < u4Len)
						u4ImgSecSize = CMD_PKT_SIZE_FOR_IMAGE;
					else
						u4ImgSecSize = u4Len - j;

					pucSecBuf = (PUINT_8)pucStartPtr + u4FileOffset + j;
					if (wlanImageSectionDownload(prAdapter, u4ImgSecSize, pucSecBuf)
						!= WLAN_STATUS_SUCCESS) {
						DBGLOG(INIT, ERROR, "Firmware scatter download failed!\n");
						u4Status = WLAN_STATUS_FAILURE;
						break;
					}
				}

				/* escape from loop if any pending error occurs */
				if (u4Status == WLAN_STATUS_FAILURE)
					break;
				u4FileOffset += u4Len;
			*pucIsCompressed = FALSE;
		}
	}
	return u4Status;
}
#else
WLAN_STATUS wlanImageSectionDownloadStage(IN P_ADAPTER_T prAdapter,
					  IN PVOID pvFwImageMapFile, IN UINT_32 u4FwImageFileLength,
					  IN UINT_8 ucSectionNumber, IN ENUM_IMG_DL_IDX_T eDlIdx)
{
	UINT_32 u4ImgSecSize;
	UINT_32 j, i;
	UINT_32 u4FileOffset = 0;
	UINT_32 u4StartOffset = 0;
	UINT_32 u4DataMode = 0;
	UINT_32 u4Addr, u4Len;
	WLAN_STATUS u4Status = WLAN_STATUS_SUCCESS;
	PUINT_8 pucSecBuf, pucStartPtr;

	/* 3a. parse file header for decision of divided firmware download or not */
	for (i = 0; i < ucSectionNumber; ++i) {
		wlanImageSectionGetInfo(prAdapter, pvFwImageMapFile,
					u4FwImageFileLength, ucSectionNumber, i, eDlIdx,
					&u4StartOffset, &u4Addr, &u4Len, &u4DataMode);

		pucStartPtr = (PUINT_8) pvFwImageMapFile + u4StartOffset;

		DBGLOG(INIT, INFO, "DL Offset[%u] addr[0x%08x] len[%u] datamode[0x%08x]\n",
		       u4FileOffset, u4Addr, u4Len, u4DataMode);

		if (wlanImageSectionConfig(prAdapter, u4Addr, u4Len, u4DataMode, eDlIdx)
		    != WLAN_STATUS_SUCCESS) {

			DBGLOG(INIT, ERROR, "Firmware download configuration failed!\n");

			u4Status = WLAN_STATUS_FAILURE;
			break;
		}

		for (j = 0; j < u4Len; j += CMD_PKT_SIZE_FOR_IMAGE) {
			if (j + CMD_PKT_SIZE_FOR_IMAGE < u4Len)
				u4ImgSecSize = CMD_PKT_SIZE_FOR_IMAGE;
			else
				u4ImgSecSize = u4Len - j;

			pucSecBuf = (PUINT_8) pucStartPtr + u4FileOffset + j;
			if (wlanImageSectionDownload(prAdapter, u4ImgSecSize, pucSecBuf)
			    != WLAN_STATUS_SUCCESS) {

				DBGLOG(INIT, ERROR, "Firmware scatter download failed!\n");

				u4Status = WLAN_STATUS_FAILURE;
				break;
			}
			kalMdelay(1);
		}

		/* escape from loop if any pending error occurs */
		if (u4Status == WLAN_STATUS_FAILURE)
			break;


		u4FileOffset += u4Len;
	}

	return u4Status;
}
#endif
/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to confirm the status of
*        previously patch semaphore control
*
* @param prAdapter      Pointer to the Adapter structure.
*        ucCmdSeqNum    Sequence number of previous firmware scatter
*
* @return WLAN_STATUS_SUCCESS
*         WLAN_STATUS_FAILURE
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanPatchRecvSemaResp(IN P_ADAPTER_T prAdapter, IN UINT_8 ucCmdSeqNum, OUT PUINT_8 pucPatchStatus)
{
	UINT_8 aucBuffer[sizeof(INIT_HIF_RX_HEADER_T) + sizeof(INIT_EVENT_CMD_RESULT)];
	P_INIT_HIF_RX_HEADER_T prInitHifRxHeader;
	P_INIT_EVENT_CMD_RESULT prEventCmdResult;
	UINT_32 u4RxPktLength;

	ASSERT(prAdapter);

	if (kalIsCardRemoved(prAdapter->prGlueInfo) == TRUE || fgIsBusAccessFailed == TRUE)
		return WLAN_STATUS_FAILURE;


	if (nicRxWaitResponse(prAdapter, 0, aucBuffer,
			      sizeof(INIT_HIF_RX_HEADER_T) + sizeof(INIT_EVENT_CMD_RESULT),
			      &u4RxPktLength) != WLAN_STATUS_SUCCESS) {

		DBGLOG(INIT, WARN, "Wait patch semaphore response fail\n");
		return WLAN_STATUS_FAILURE;
	}

	prInitHifRxHeader = (P_INIT_HIF_RX_HEADER_T) aucBuffer;
	if (prInitHifRxHeader->rInitWifiEvent.ucEID != INIT_EVENT_ID_PATCH_SEMA_CTRL) {
		DBGLOG(INIT, WARN, "Unexpected EVENT ID, get 0x%0x\n", prInitHifRxHeader->rInitWifiEvent.ucEID);
		return WLAN_STATUS_FAILURE;
	}

	if (prInitHifRxHeader->rInitWifiEvent.ucSeqNum != ucCmdSeqNum) {
		DBGLOG(INIT, WARN, "Unexpected SeqNum %d, %d\n", ucCmdSeqNum,
		       prInitHifRxHeader->rInitWifiEvent.ucSeqNum);
		return WLAN_STATUS_FAILURE;
	}

	prEventCmdResult = (P_INIT_EVENT_CMD_RESULT) (prInitHifRxHeader->rInitWifiEvent.aucBuffer);

	*pucPatchStatus = prEventCmdResult->ucStatus;

#if 0
	if (prEventCmdResult->ucStatus != PATCH_STATUS_GET_SEMA_NEED_PATCH) {
		DBGLOG(INIT, INFO, "Patch status[%d], skip patch\n", prEventCmdResult->ucStatus);
		return WLAN_STATUS_FAILURE;
	}
		DBGLOG(INIT, INFO, "Status[%d], ready to patch\n", prEventCmdResult->ucStatus);
		return WLAN_STATUS_SUCCESS;

#endif

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to check the patch semaphore control.
*
* @param prAdapter      Pointer to the Adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanPatchSendSemaControl(IN P_ADAPTER_T prAdapter, OUT PUINT_8 pucSeqNum)
{
	P_CMD_INFO_T prCmdInfo;
	P_INIT_HIF_TX_HEADER_T prInitHifTxHeader;
	WLAN_STATUS u4Status = WLAN_STATUS_SUCCESS;
	P_INIT_CMD_PATCH_SEMA_CONTROL prPatchSemaControl;

	ASSERT(prAdapter);

	DEBUGFUNC("wlanImagePatchSemaphoreCheck");

	/* 1. Allocate CMD Info Packet and its Buffer. */
	prCmdInfo =
	    cmdBufAllocateCmdInfo(prAdapter, sizeof(INIT_HIF_TX_HEADER_T) + sizeof(INIT_CMD_PATCH_SEMA_CONTROL));

	/* DBGLOG(INIT, ERROR, "sizeof INIT_HIF_TX_HEADER_T = %d\n", sizeof(INIT_HIF_TX_HEADER_T)); */
	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}

	prCmdInfo->u2InfoBufLen = sizeof(INIT_HIF_TX_HEADER_T) + sizeof(INIT_CMD_PATCH_SEMA_CONTROL);

	/* 2. Setup common CMD Info Packet */
	prInitHifTxHeader = (P_INIT_HIF_TX_HEADER_T) (prCmdInfo->pucInfoBuffer);
	prInitHifTxHeader->u2TxByteCount = prCmdInfo->u2InfoBufLen;
	prInitHifTxHeader->u2PQ_ID = INIT_CMD_PQ_ID;
	prInitHifTxHeader->ucHeaderFormat = INIT_CMD_PACKET_TYPE_ID;
	prInitHifTxHeader->ucPktFt = INIT_PKT_FT_CMD;

	prInitHifTxHeader->rInitWifiCmd.ucCID = INIT_CMD_ID_PATCH_SEMAPHORE_CONTROL;
	prInitHifTxHeader->rInitWifiCmd.ucPktTypeID = INIT_CMD_PDA_PACKET_TYPE_ID;
	prInitHifTxHeader->rInitWifiCmd.ucSeqNum = nicIncreaseCmdSeqNum(prAdapter);

	*pucSeqNum = prInitHifTxHeader->rInitWifiCmd.ucSeqNum;

	/* 3. Setup DOWNLOAD_BUF */
	prPatchSemaControl = (P_INIT_CMD_PATCH_SEMA_CONTROL) prInitHifTxHeader->rInitWifiCmd.aucBuffer;
	kalMemZero(prPatchSemaControl, sizeof(INIT_CMD_PATCH_SEMA_CONTROL));
	prPatchSemaControl->ucGetSemaphore = PATCH_GET_SEMA_CONTROL;

	/* 4. Send FW_Download command */
	if (nicTxInitCmd(prAdapter, prCmdInfo) != WLAN_STATUS_SUCCESS) {
		u4Status = WLAN_STATUS_FAILURE;
		DBGLOG(INIT, ERROR, "Fail to transmit image download command\n");
	}
	/* 5. Free CMD Info Packet. */
	cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

	return u4Status;
}

BOOLEAN wlanPatchIsDownloaded(IN P_ADAPTER_T prAdapter)
{
	UINT_8 ucSeqNum, ucPatchStatus;
	WLAN_STATUS rStatus;
	UINT_32 u4Count;

	ucPatchStatus = PATCH_STATUS_NO_SEMA_NEED_PATCH;
	u4Count = 0;

	while (ucPatchStatus == PATCH_STATUS_NO_SEMA_NEED_PATCH) {

		if (u4Count)
			kalMdelay(100);


		rStatus = wlanPatchSendSemaControl(prAdapter, &ucSeqNum);
		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, WARN, "Send patch SEMA control CMD failed!!\n");
			break;
		}

		rStatus = wlanPatchRecvSemaResp(prAdapter, ucSeqNum, &ucPatchStatus);
		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, WARN, "Recv patch SEMA control EVT failed!!\n");
			break;
		}

		u4Count++;

		if (u4Count > 50) {
			DBGLOG(INIT, WARN, "Patch status check timeout!!\n");
			break;
		}
	}

	if (ucPatchStatus == PATCH_STATUS_NO_NEED_TO_PATCH)
		return TRUE;
	else
		return FALSE;

}

WLAN_STATUS wlanPatchSendComplete(IN P_ADAPTER_T prAdapter)
{
	P_CMD_INFO_T prCmdInfo;
	P_INIT_HIF_TX_HEADER_T prInitHifTxHeader;
	UINT_8 ucTC, ucCmdSeqNum;
	WLAN_STATUS u4Status = WLAN_STATUS_SUCCESS;

	ASSERT(prAdapter);

	/* 1. Allocate CMD Info Packet and its Buffer. */
	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, sizeof(INIT_HIF_TX_HEADER_T));

	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}

	kalMemZero(prCmdInfo->pucInfoBuffer, sizeof(INIT_HIF_TX_HEADER_T));
	prCmdInfo->u2InfoBufLen = sizeof(INIT_HIF_TX_HEADER_T);

#if (CFG_USE_TC4_RESOURCE_FOR_INIT_CMD == 1)
	/* 2. Always use TC4 (TC4 as CPU) */
	ucTC = TC4_INDEX;
#else
	/* 2. Use TC0's resource to send patch finish command.
	 * Only TC0 is allowed because SDIO HW always reports
	 * MCU's TXQ_CNT at TXQ0_CNT in CR4 architecutre)
	 */
	ucTC = TC0_INDEX;
#endif

	/* 3. increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

	/* 4. Setup common CMD Info Packet */
	prInitHifTxHeader = (P_INIT_HIF_TX_HEADER_T) (prCmdInfo->pucInfoBuffer);
	prInitHifTxHeader->u2TxByteCount = prCmdInfo->u2InfoBufLen;
	prInitHifTxHeader->u2PQ_ID = INIT_CMD_PQ_ID;
	prInitHifTxHeader->ucPktFt = INIT_PKT_FT_CMD;

	prInitHifTxHeader->rInitWifiCmd.ucCID = INIT_CMD_ID_PATCH_FINISH;
	prInitHifTxHeader->rInitWifiCmd.ucPktTypeID = INIT_CMD_PACKET_TYPE_ID;
	prInitHifTxHeader->rInitWifiCmd.ucSeqNum = ucCmdSeqNum;

	/* 5. Seend WIFI start command */
	while (1) {
		/* 5.1 Acquire TX Resource */
		if (nicTxAcquireResource(prAdapter, ucTC, nicTxGetPageCount(prCmdInfo->u2InfoBufLen, TRUE), TRUE)
			== WLAN_STATUS_RESOURCES) {
			if (nicTxPollingResource(prAdapter, ucTC) != WLAN_STATUS_SUCCESS) {
				u4Status = WLAN_STATUS_FAILURE;
				DBGLOG(INIT, ERROR, "Fail to get TX resource return within timeout\n");
				goto exit;
			}
			continue;
		}
		/* 5.2 Send CMD Info Packet */
		if (nicTxInitCmd(prAdapter, prCmdInfo) != WLAN_STATUS_SUCCESS) {
			u4Status = WLAN_STATUS_FAILURE;
			DBGLOG(INIT, ERROR, "Fail to transmit WIFI start command\n");
			goto exit;
		}

		break;
	};

	DBGLOG(INIT, INFO, "PATCH FINISH CMD send, waiting for RSP\n");

	/* kalMdelay(10000); */

	u4Status = wlanConfigWifiFuncStatus(prAdapter, ucCmdSeqNum);

	if (u4Status != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, INFO, "PATCH FINISH EVT failed\n");
	else
		DBGLOG(INIT, INFO, "PATCH FINISH EVT success!!\n");

exit:
	/* 6. Free CMD Info Packet. */
	cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

	return u4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to configure FWDL parameters
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
WLAN_STATUS wlanImageSectionConfig(IN P_ADAPTER_T prAdapter,
				   IN UINT_32 u4DestAddr, IN UINT_32 u4ImgSecSize, IN UINT_32 u4DataMode,
				   IN ENUM_IMG_DL_IDX_T eDlIdx)
{
	P_CMD_INFO_T prCmdInfo;
	P_INIT_HIF_TX_HEADER_T prInitHifTxHeader;
	P_INIT_CMD_DOWNLOAD_CONFIG prInitCmdDownloadConfig;
	UINT_8 ucTC, ucCmdSeqNum;
	WLAN_STATUS u4Status = WLAN_STATUS_SUCCESS;

	ASSERT(prAdapter);

	DEBUGFUNC("wlanImageSectionConfig");

	if (u4ImgSecSize == 0)
		return WLAN_STATUS_SUCCESS;
	/* 1. Allocate CMD Info Packet and its Buffer. */
	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, sizeof(INIT_HIF_TX_HEADER_T) + sizeof(INIT_CMD_DOWNLOAD_CONFIG));

	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}

	prCmdInfo->u2InfoBufLen = sizeof(INIT_HIF_TX_HEADER_T) + sizeof(INIT_CMD_DOWNLOAD_CONFIG);

#if (CFG_USE_TC4_RESOURCE_FOR_INIT_CMD == 1)
	/* 2. Use TC4's resource to download image. (TC4 as CPU) */
	ucTC = TC4_INDEX;
#else
	/* 2. Use TC0's resource to send init_cmd.
	 * Only TC0 is allowed because SDIO HW always reports
	 * MCU's TXQ_CNT at TXQ0_CNT in CR4 architecutre)
	 */
	ucTC = TC0_INDEX;
#endif

	/* 3. increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

	/* 4. Setup common CMD Info Packet */
	prInitHifTxHeader = (P_INIT_HIF_TX_HEADER_T) (prCmdInfo->pucInfoBuffer);
	prInitHifTxHeader->u2TxByteCount = prCmdInfo->u2InfoBufLen;
	prInitHifTxHeader->u2PQ_ID = INIT_CMD_PQ_ID;
	prInitHifTxHeader->ucHeaderFormat = INIT_CMD_PACKET_TYPE_ID;
	prInitHifTxHeader->ucPktFt = INIT_PKT_FT_CMD;

	if (eDlIdx == IMG_DL_IDX_PATCH)
		prInitHifTxHeader->rInitWifiCmd.ucCID = INIT_CMD_ID_PATCH_START;
	else
		prInitHifTxHeader->rInitWifiCmd.ucCID = INIT_CMD_ID_DOWNLOAD_CONFIG;


	prInitHifTxHeader->rInitWifiCmd.ucPktTypeID = INIT_CMD_PACKET_TYPE_ID;
	prInitHifTxHeader->rInitWifiCmd.ucSeqNum = ucCmdSeqNum;

	/* 5. Setup CMD_DOWNLOAD_CONFIG */
	prInitCmdDownloadConfig = (P_INIT_CMD_DOWNLOAD_CONFIG) (prInitHifTxHeader->rInitWifiCmd.aucBuffer);
	prInitCmdDownloadConfig->u4Address = u4DestAddr;
	prInitCmdDownloadConfig->u4Length = u4ImgSecSize;
	prInitCmdDownloadConfig->u4DataMode = u4DataMode;

	/* 6. Send FW_Download command */
	while (1) {
		/* 6.1 Acquire TX Resource */
		if (nicTxAcquireResource(prAdapter, ucTC, nicTxGetPageCount(prCmdInfo->u2InfoBufLen, TRUE), TRUE)
			== WLAN_STATUS_RESOURCES) {
			if (nicTxPollingResource(prAdapter, ucTC) != WLAN_STATUS_SUCCESS) {
				u4Status = WLAN_STATUS_FAILURE;
				DBGLOG(INIT, ERROR, "Fail to get TX resource return within timeout\n");
				goto exit;
			}
			continue;
		}
		/* 6.2 Send CMD Info Packet */
		if (nicTxInitCmd(prAdapter, prCmdInfo) != WLAN_STATUS_SUCCESS) {
			u4Status = WLAN_STATUS_FAILURE;
			DBGLOG(INIT, ERROR, "Fail to transmit image download command\n");
			goto exit;
		}

		break;
	};

#if CFG_ENABLE_FW_DOWNLOAD_ACK
	/* 7. Wait for INIT_EVENT_ID_CMD_RESULT */
	u4Status = wlanImageSectionDownloadStatus(prAdapter, ucCmdSeqNum);
#endif

exit:
	/* 8. Free CMD Info Packet. */
	cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

	return u4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to download FW image.
*
* @param prAdapter      Pointer to the Adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanImageSectionDownload(IN P_ADAPTER_T prAdapter, IN UINT_32 u4ImgSecSize, IN PUINT_8 pucImgSecBuf)
{
	P_CMD_INFO_T prCmdInfo;
	P_INIT_HIF_TX_HEADER_T prInitHifTxHeader;
	WLAN_STATUS u4Status = WLAN_STATUS_SUCCESS;

	ASSERT(prAdapter);
	ASSERT(pucImgSecBuf);
	ASSERT(u4ImgSecSize <= CMD_PKT_SIZE_FOR_IMAGE);

	DEBUGFUNC("wlanImageSectionDownload");

	if (u4ImgSecSize == 0)
		return WLAN_STATUS_SUCCESS;
	/* 1. Allocate CMD Info Packet and its Buffer. */
	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, sizeof(INIT_HIF_TX_HEADER_T) + u4ImgSecSize);

	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}

	prCmdInfo->u2InfoBufLen = sizeof(INIT_HIF_TX_HEADER_T) + (UINT_16) u4ImgSecSize;

	/* 2. Setup common CMD Info Packet */
	prInitHifTxHeader = (P_INIT_HIF_TX_HEADER_T) (prCmdInfo->pucInfoBuffer);
	prInitHifTxHeader->u2TxByteCount = prCmdInfo->u2InfoBufLen;
	prInitHifTxHeader->u2PQ_ID = INIT_CMD_PDA_PQ_ID;
	prInitHifTxHeader->ucHeaderFormat = INIT_CMD_PDA_PACKET_TYPE_ID;
	prInitHifTxHeader->ucPktFt = INIT_PKT_FT_PDA_FWDL;

	prInitHifTxHeader->rInitWifiCmd.ucCID = 0;
	prInitHifTxHeader->rInitWifiCmd.ucPktTypeID = INIT_CMD_PDA_PACKET_TYPE_ID;
	prInitHifTxHeader->rInitWifiCmd.ucSeqNum = 0;

	/* 3. Setup DOWNLOAD_BUF */
	kalMemCopy(prInitHifTxHeader->rInitWifiCmd.aucBuffer, pucImgSecBuf, u4ImgSecSize);

	/* 4. Send FW_Download command */
	if (nicTxInitCmd(prAdapter, prCmdInfo) != WLAN_STATUS_SUCCESS) {
		u4Status = WLAN_STATUS_FAILURE;
		DBGLOG(INIT, ERROR, "Fail to transmit image download command\n");
	}
	/* 5. Free CMD Info Packet. */
	cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

	return u4Status;
}

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

	kalMemZero(prCmdInfo->pucInfoBuffer, sizeof(INIT_HIF_TX_HEADER_T));
	prCmdInfo->u2InfoBufLen = sizeof(INIT_HIF_TX_HEADER_T);

#if (CFG_USE_TC4_RESOURCE_FOR_INIT_CMD == 1)
		/* 2. Always use TC4 */
		ucTC = TC4_INDEX;
#else
		/* 2. Use TC0's resource to send init_cmd
		 * Only TC0 is allowed because SDIO HW always reports
		 * CPU's TXQ_CNT at TXQ0_CNT in CR4 architecutre)
		 */
		ucTC = TC0_INDEX;
#endif

	/* 3. increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

	/* 4. Setup common CMD Info Packet */
	prInitHifTxHeader = (P_INIT_HIF_TX_HEADER_T) (prCmdInfo->pucInfoBuffer);

	prInitHifTxHeader->u2TxByteCount = prCmdInfo->u2InfoBufLen;
	prInitHifTxHeader->u2PQ_ID = INIT_CMD_PQ_ID;

	prInitHifTxHeader->rInitWifiCmd.ucCID = INIT_CMD_ID_QUERY_PENDING_ERROR;
	prInitHifTxHeader->rInitWifiCmd.ucPktTypeID = INIT_CMD_PACKET_TYPE_ID;
	prInitHifTxHeader->rInitWifiCmd.ucSeqNum = ucCmdSeqNum;

	/* 5. Send command */
	while (1) {
		/* 5.1 Acquire TX Resource */
		if (nicTxAcquireResource(prAdapter, ucTC, nicTxGetPageCount(prCmdInfo->u2InfoBufLen, TRUE), TRUE)
			== WLAN_STATUS_RESOURCES) {
			if (nicTxPollingResource(prAdapter, ucTC) != WLAN_STATUS_SUCCESS) {
				u4Status = WLAN_STATUS_FAILURE;
				DBGLOG(INIT, ERROR, "Fail to get TX resource return within timeout\n");
				break;
			}
				continue;

		}
		/* 5.2 Send CMD Info Packet */
		if (nicTxInitCmd(prAdapter, prCmdInfo) != WLAN_STATUS_SUCCESS) {
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
					     sizeof(INIT_HIF_RX_HEADER_T) +
					     sizeof(INIT_EVENT_PENDING_ERROR), &u4RxPktLength) != WLAN_STATUS_SUCCESS) {
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

	UINT_8 ucPortIdx = IMG_DL_STATUS_PORT_IDX;

	ASSERT(prAdapter);

	do {
		if (kalIsCardRemoved(prAdapter->prGlueInfo) == TRUE || fgIsBusAccessFailed == TRUE) {
			u4Status = WLAN_STATUS_FAILURE;
		} else if (nicRxWaitResponse(prAdapter,
					     ucPortIdx,
					     aucBuffer,
					     sizeof(INIT_HIF_RX_HEADER_T) +
					     sizeof(INIT_EVENT_CMD_RESULT), &u4RxPktLength) != WLAN_STATUS_SUCCESS) {
			u4Status = WLAN_STATUS_FAILURE;
		} else {
			prInitHifRxHeader = (P_INIT_HIF_RX_HEADER_T) aucBuffer;

			/* EID / SeqNum check */
			if (prInitHifRxHeader->rInitWifiEvent.ucEID != INIT_EVENT_ID_CMD_RESULT) {
				u4Status = WLAN_STATUS_FAILURE;
			} else if (prInitHifRxHeader->rInitWifiEvent.ucSeqNum != ucCmdSeqNum) {
				u4Status = WLAN_STATUS_FAILURE;
			} else {
				prEventCmdResult =
				    (P_INIT_EVENT_CMD_RESULT) (prInitHifRxHeader->rInitWifiEvent.aucBuffer);
				if (prEventCmdResult->ucStatus != 0) {	/* 0 for download success */
					DBGLOG(INIT, ERROR, "Start CMD failed, status[%u]\n",
					       prEventCmdResult->ucStatus);
#if CFG_SUPPORT_COMPRESSION_FW_OPTION
				if (prEventCmdResult->ucStatus == WIFI_FW_DECOMPRESSION_FAILED)
					DBGLOG(INIT, ERROR, "Start Decompression CMD failed, status[%u]\n",
					       prEventCmdResult->ucStatus);
#endif
					u4Status = WLAN_STATUS_FAILURE;
				} else {
					u4Status = WLAN_STATUS_SUCCESS;
				}
			}
		}
	} while (FALSE);

	return u4Status;
}

WLAN_STATUS wlanConfigWifiFunc(IN P_ADAPTER_T prAdapter,
			       IN BOOLEAN fgEnable, IN UINT_32 u4StartAddress, IN UINT_8 ucPDA)
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

#if (CFG_USE_TC4_RESOURCE_FOR_INIT_CMD == 1)
	/* 2. Always use TC4 (TC4 as CPU) */
	ucTC = TC4_INDEX;
#else
	/* 2. Use TC0's resource to send init_cmd.
	 * Only TC0 is allowed because SDIO HW always reports
	 * CPU's TXQ_CNT at TXQ0_CNT in CR4 architecutre)
	 */
	ucTC = TC0_INDEX;
#endif

	/* 3. increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

	/* 4. Setup common CMD Info Packet */
	prInitHifTxHeader = (P_INIT_HIF_TX_HEADER_T) (prCmdInfo->pucInfoBuffer);
	prInitHifTxHeader->u2TxByteCount = prCmdInfo->u2InfoBufLen;
	prInitHifTxHeader->u2PQ_ID = INIT_CMD_PQ_ID;
	prInitHifTxHeader->ucPktFt = INIT_PKT_FT_CMD;

	prInitHifTxHeader->rInitWifiCmd.ucCID = INIT_CMD_ID_WIFI_START;
	prInitHifTxHeader->rInitWifiCmd.ucPktTypeID = INIT_CMD_PACKET_TYPE_ID;
	prInitHifTxHeader->rInitWifiCmd.ucSeqNum = ucCmdSeqNum;

	prInitCmdWifiStart = (P_INIT_CMD_WIFI_START) (prInitHifTxHeader->rInitWifiCmd.aucBuffer);
	prInitCmdWifiStart->u4Override = 0;
	if (fgEnable)
		prInitCmdWifiStart->u4Override |= START_OVERRIDE_START_ADDRESS;

    /* 5G cal until send efuse buffer mode CMD */
#if (CFG_EFUSE_BUFFER_MODE_DELAY_CAL == 1)
	if (prAdapter->fgIsSupportDelayCal == TRUE)
		prInitCmdWifiStart->u4Override |= START_DELAY_CALIBRATION;
#endif

	if (ucPDA == PDA_CR4)
		prInitCmdWifiStart->u4Override |= START_WORKING_PDA_OPTION;

	prInitCmdWifiStart->u4Address = u4StartAddress;

	/* 5. Seend WIFI start command */
	while (1) {
		/* 5.1 Acquire TX Resource */
		if (nicTxAcquireResource(prAdapter, ucTC, nicTxGetPageCount(prCmdInfo->u2InfoBufLen, TRUE), TRUE)
			== WLAN_STATUS_RESOURCES) {
			if (nicTxPollingResource(prAdapter, ucTC) != WLAN_STATUS_SUCCESS) {
				u4Status = WLAN_STATUS_FAILURE;
				DBGLOG(INIT, ERROR, "Fail to get TX resource return within timeout\n");
				goto exit;
			}
			continue;
		}
		/* 5.2 Send CMD Info Packet */
		if (nicTxInitCmd(prAdapter, prCmdInfo) != WLAN_STATUS_SUCCESS) {
			u4Status = WLAN_STATUS_FAILURE;
			DBGLOG(INIT, ERROR, "Fail to transmit WIFI start command\n");
			goto exit;
		}

		break;
	};

	DBGLOG(INIT, INFO, "FW_START CMD send, waiting for RSP\n");

	u4Status = wlanConfigWifiFuncStatus(prAdapter, ucCmdSeqNum);

	if (u4Status != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, INFO, "FW_START EVT failed\n");
	else
		DBGLOG(INIT, INFO, "FW_START EVT success!!\n");

exit:
	/* 6. Free CMD Info Packet. */
	cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

	return u4Status;
}
#if CFG_SUPPORT_COMPRESSION_FW_OPTION
WLAN_STATUS
wlanCompressedFWConfigWifiFunc(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgEnable,
	IN UINT_32 u4StartAddress, IN UINT_8 ucPDA, IN P_INIT_CMD_WIFI_DECOMPRESSION_START prFwImageInFo)
{
	P_CMD_INFO_T prCmdInfo;
	P_INIT_HIF_TX_HEADER_T prInitHifTxHeader;
	P_INIT_CMD_WIFI_DECOMPRESSION_START prInitCmdWifiStart;
	UINT_8 ucTC, ucCmdSeqNum;
	WLAN_STATUS u4Status = WLAN_STATUS_SUCCESS;

	ASSERT(prAdapter);
	DEBUGFUNC("wlanConfigWifiFunc");
	/* 1. Allocate CMD Info Packet and its Buffer. */
	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter,
					  sizeof(INIT_HIF_TX_HEADER_T) +
					  sizeof(INIT_CMD_WIFI_DECOMPRESSION_START));

	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}

	kalMemZero(prCmdInfo->pucInfoBuffer,
		sizeof(INIT_HIF_TX_HEADER_T) + sizeof(INIT_CMD_WIFI_DECOMPRESSION_START));
	prCmdInfo->u2InfoBufLen =
		sizeof(INIT_HIF_TX_HEADER_T) + sizeof(INIT_CMD_WIFI_DECOMPRESSION_START);

	/* 2. Always use TC0 */
	ucTC = TC0_INDEX;

	/* 3. increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

	/* 4. Setup common CMD Info Packet */
	prInitHifTxHeader = (P_INIT_HIF_TX_HEADER_T) (prCmdInfo->pucInfoBuffer);
	prInitHifTxHeader->u2TxByteCount = prCmdInfo->u2InfoBufLen;
	prInitHifTxHeader->u2PQ_ID = INIT_CMD_PQ_ID;
	prInitHifTxHeader->ucPktFt = INIT_PKT_FT_CMD;
	prInitHifTxHeader->rInitWifiCmd.ucCID = INIT_CMD_ID_DECOMPRESSED_WIFI_START;
	prInitHifTxHeader->rInitWifiCmd.ucPktTypeID = INIT_CMD_PACKET_TYPE_ID;
	prInitHifTxHeader->rInitWifiCmd.ucSeqNum = ucCmdSeqNum;

	prInitCmdWifiStart = (P_INIT_CMD_WIFI_DECOMPRESSION_START) (prInitHifTxHeader->rInitWifiCmd.aucBuffer);
	prInitCmdWifiStart->u4Override = 0;
	if (fgEnable)
		prInitCmdWifiStart->u4Override |= START_OVERRIDE_START_ADDRESS;

    /* 5G cal until send efuse buffer mode CMD */
#if (CFG_EFUSE_BUFFER_MODE_DELAY_CAL == 1)
	if (prAdapter->fgIsSupportDelayCal == TRUE)
		prInitCmdWifiStart->u4Override |= START_DELAY_CALIBRATION;
#endif
	if (ucPDA == PDA_CR4)
		prInitCmdWifiStart->u4Override |= START_WORKING_PDA_OPTION;

#if CFG_COMPRESSION_DEBUG
		prInitCmdWifiStart->u4Override |= START_CRC_CHECK;
#endif
#if CFG_DECOMPRESSION_TMP_ADDRESS
		prInitCmdWifiStart->u4Override |= CHANGE_DECOMPRESSION_TMP_ADDRESS;
		prInitCmdWifiStart->u4DecompressTmpAddress = 0xE6000;
#endif
	prInitCmdWifiStart->u4Address = u4StartAddress;
	prInitCmdWifiStart->u4Region1Address = prFwImageInFo->u4Region1Address;
	prInitCmdWifiStart->u4Region1CRC = prFwImageInFo->u4Region1CRC;
	prInitCmdWifiStart->u4BlockSize = prFwImageInFo->u4BlockSize;
	prInitCmdWifiStart->u4Region1length = prFwImageInFo->u4Region1length;
	prInitCmdWifiStart->u4Region2Address = prFwImageInFo->u4Region2Address;
	prInitCmdWifiStart->u4Region2CRC = prFwImageInFo->u4Region2CRC;
	prInitCmdWifiStart->u4Region2length = prFwImageInFo->u4Region2length;

	while (1) {
		/* 5.1 Acquire TX Resource */
		if (nicTxAcquireResource(prAdapter, ucTC, nicTxGetPageCount(prCmdInfo->u2InfoBufLen, TRUE), TRUE)
			== WLAN_STATUS_RESOURCES) {
			if (nicTxPollingResource(prAdapter, ucTC) != WLAN_STATUS_SUCCESS) {
				u4Status = WLAN_STATUS_FAILURE;
				DBGLOG(INIT, ERROR, "Fail to get TX resource return within timeout\n");
				break;
			}
				continue;

		}
		/* 5.2 Send CMD Info Packet */
		if (nicTxInitCmd(prAdapter, prCmdInfo) != WLAN_STATUS_SUCCESS) {
			u4Status = WLAN_STATUS_FAILURE;
			DBGLOG(INIT, ERROR, "Fail to transmit WIFI start command\n");
		}

		break;
	};

	DBGLOG(INIT, INFO, "FW_START CMD send, waiting for RSP\n");

	u4Status = wlanConfigWifiFuncStatus(prAdapter, ucCmdSeqNum);

	if (u4Status != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, INFO, "FW_START EVT failed\n");
	else
		DBGLOG(INIT, INFO, "FW_START EVT success!!\n");


	/* 6. Free CMD Info Packet. */
	cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

	return u4Status;
}
#endif
#if 0
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
UINT_32 wlanCRC32(PUINT_8 buf, UINT_32 len)
{
	UINT_32 i, crc32 = 0xFFFFFFFF;
	const UINT_32 crc32_ccitt_table[256] = {
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

	for (i = 0; i < len; i++)
		crc32 = crc32_ccitt_table[(crc32 ^ buf[i]) & 0xff] ^ (crc32 >> 8);

	return ~crc32;
}
#endif

WLAN_STATUS wlanDownloadFW(IN P_ADAPTER_T prAdapter)
{
	UINT_32 u4FwSize = 0;
	PVOID prFwBuffer = NULL;
	BOOLEAN fgReady;
	WLAN_STATUS rDlStatus = 0;
	WLAN_STATUS	rCfgStatus = 0;
#if CFG_SUPPORT_COMPRESSION_FW_OPTION
	BOOLEAN fgIsCompressed = FALSE;
	INIT_CMD_WIFI_DECOMPRESSION_START rFwImageInFo;

#endif

	if (!prAdapter)
		return WLAN_STATUS_FAILURE;


	HAL_WIFI_FUNC_READY_CHECK(prAdapter, WIFI_FUNC_READY_BITS, &fgReady);

	if (fgReady) {
		DBGLOG(INIT, INFO, "Wi-Fi is already ON!, turn off before FW DL!\n");

		if (wlanPowerOffWifi(prAdapter) != WLAN_STATUS_SUCCESS)
			return WLAN_STATUS_FAILURE;

		nicpmWakeUpWiFi(prAdapter);
		HAL_HIF_INIT(prAdapter);
	}

	HAL_ENABLE_FWDL(prAdapter, TRUE);

#if (MTK_WCN_HIF_SDIO == 0)
	wlanDownloadPatch(prAdapter);
#endif

	DBGLOG(INIT, INFO, "FW download Start\n");

	do {
		/* N9 ILM+DLM */
		kalFirmwareImageMapping(prAdapter->prGlueInfo, &prFwBuffer, &u4FwSize, IMG_DL_IDX_N9_FW);
		if (prFwBuffer == NULL) {
			DBGLOG(INIT, WARN, "FW[%u] load error!\n", IMG_DL_IDX_N9_FW);
			break;
		}
#if CFG_SUPPORT_COMPRESSION_FW_OPTION
		rDlStatus = wlanImageSectionDownloadStage(prAdapter, prFwBuffer, u4FwSize, 2,
		IMG_DL_IDX_N9_FW, &fgIsCompressed, &rFwImageInFo);
		if (fgIsCompressed == TRUE)
			rCfgStatus = wlanCompressedFWConfigWifiFunc(prAdapter, FALSE, 0, PDA_N9, &rFwImageInFo);
		else
			rCfgStatus = wlanConfigWifiFunc(prAdapter, FALSE, 0, PDA_N9);

#else
		rDlStatus = wlanImageSectionDownloadStage(prAdapter, prFwBuffer, u4FwSize, 2, IMG_DL_IDX_N9_FW);
		rCfgStatus = wlanConfigWifiFunc(prAdapter, FALSE, 0, PDA_N9);
#endif
		kalFirmwareImageUnmapping(prAdapter->prGlueInfo, NULL, prFwBuffer);

		if ((rDlStatus != WLAN_STATUS_SUCCESS) || (rCfgStatus != WLAN_STATUS_SUCCESS))
			break;
		/* wlanCheckWifiN9Func(prAdapter); */

		/* CR4 bin */
		kalFirmwareImageMapping(prAdapter->prGlueInfo, &prFwBuffer, &u4FwSize, IMG_DL_IDX_CR4_FW);
		if (prFwBuffer == NULL) {
			DBGLOG(INIT, WARN, "FW[%u] load error!\n", IMG_DL_IDX_CR4_FW);
			break;
		}
#if CFG_SUPPORT_COMPRESSION_FW_OPTION
		rDlStatus = wlanImageSectionDownloadStage(prAdapter, prFwBuffer,
				u4FwSize, CR4_FWDL_SECTION_NUM, IMG_DL_IDX_CR4_FW, &fgIsCompressed, &rFwImageInFo);
		prAdapter->fgIsCr4FwDownloaded = TRUE;
		if (fgIsCompressed == TRUE)
			rCfgStatus = wlanCompressedFWConfigWifiFunc(prAdapter, FALSE, 0, PDA_CR4, &rFwImageInFo);
		else
			rCfgStatus = wlanConfigWifiFunc(prAdapter, FALSE, 0, PDA_CR4);

#else
		rDlStatus = wlanImageSectionDownloadStage(prAdapter, prFwBuffer,
			u4FwSize, CR4_FWDL_SECTION_NUM, IMG_DL_IDX_CR4_FW);
		prAdapter->fgIsCr4FwDownloaded = TRUE;
		rCfgStatus = wlanConfigWifiFunc(prAdapter, FALSE, 0, PDA_CR4);
#endif
		kalFirmwareImageUnmapping(prAdapter->prGlueInfo, NULL, prFwBuffer);

		if ((rDlStatus != WLAN_STATUS_SUCCESS) || (rCfgStatus != WLAN_STATUS_SUCCESS))
			break;

	} while (0);
	DBGLOG(INIT, INFO, "FW download End\n");

	HAL_ENABLE_FWDL(prAdapter, FALSE);

	if ((rDlStatus != WLAN_STATUS_SUCCESS) || (rCfgStatus != WLAN_STATUS_SUCCESS))
		return WLAN_STATUS_FAILURE;
	else
		return WLAN_STATUS_SUCCESS;

}

WLAN_STATUS wlanWakeUpWiFi(IN P_ADAPTER_T prAdapter)
{
	BOOLEAN fgReady;

	if (!prAdapter)
		return WLAN_STATUS_FAILURE;

	HAL_WIFI_FUNC_READY_CHECK(prAdapter, WIFI_FUNC_READY_BITS, &fgReady);
	if (fgReady) {
		DBGLOG(INIT, WARN,
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

WLAN_STATUS wlanDownloadPatch(IN P_ADAPTER_T prAdapter)
{
	UINT_32 u4FwSize = 0;
	PVOID prFwBuffer = NULL;
#if CFG_SUPPORT_COMPRESSION_FW_OPTION
	UINT_8 ucIsCompressed;
#endif
	if (!prAdapter)
		return WLAN_STATUS_FAILURE;


	DBGLOG(INIT, INFO, "Patch download Start\n");

	prAdapter->rVerInfo.fgPatchIsDlByDrv = FALSE;

	kalFirmwareImageMapping(prAdapter->prGlueInfo, &prFwBuffer, &u4FwSize, IMG_DL_IDX_PATCH);
	if (prFwBuffer == NULL) {
		DBGLOG(INIT, WARN, "FW[%u] load error!\n", IMG_DL_IDX_PATCH);
		return WLAN_STATUS_FAILURE;
	}

	if (wlanPatchIsDownloaded(prAdapter)) {
		kalFirmwareImageUnmapping(prAdapter->prGlueInfo, NULL, prFwBuffer);
		DBGLOG(INIT, INFO, "No need to DL patch\n");
		return WLAN_STATUS_SUCCESS;
	}

	/* Patch DL */
	do {
#if CFG_SUPPORT_COMPRESSION_FW_OPTION
		wlanImageSectionDownloadStage(prAdapter, prFwBuffer, u4FwSize, 1, IMG_DL_IDX_PATCH,
		&ucIsCompressed, NULL);
#else
		wlanImageSectionDownloadStage(prAdapter, prFwBuffer, u4FwSize, 1, IMG_DL_IDX_PATCH);
#endif
		wlanPatchSendComplete(prAdapter);
		kalFirmwareImageUnmapping(prAdapter->prGlueInfo, NULL, prFwBuffer);

		prAdapter->rVerInfo.fgPatchIsDlByDrv = TRUE;
	} while (0);

	DBGLOG(INIT, INFO, "Patch download End\n");

	return WLAN_STATUS_SUCCESS;
}

WLAN_STATUS wlanGetPatchInfo(IN P_ADAPTER_T prAdapter)
{
	UINT_32 u4FwSize = 0;
	PVOID prFwBuffer = NULL;
	UINT_32 u4StartOffset, u4Addr, u4Len, u4DataMode;

	if (!prAdapter)
		return WLAN_STATUS_FAILURE;

	kalFirmwareImageMapping(prAdapter->prGlueInfo, &prFwBuffer, &u4FwSize, IMG_DL_IDX_PATCH);
	if (prFwBuffer == NULL) {
		DBGLOG(INIT, WARN, "FW[%u] load error!\n", IMG_DL_IDX_PATCH);
		return WLAN_STATUS_FAILURE;
	}

	wlanImageSectionGetInfo(prAdapter, prFwBuffer, u4FwSize, 1, 1, IMG_DL_IDX_PATCH,
		&u4StartOffset, &u4Addr, &u4Len, &u4DataMode);

	kalFirmwareImageUnmapping(prAdapter->prGlueInfo, NULL, prFwBuffer);

	return WLAN_STATUS_SUCCESS;
}

#endif

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to get the chip information
*
* @param prAdapter      Pointer to the Adapter structure.
*
* @return
*/
/*----------------------------------------------------------------------------*/

WLAN_STATUS wlanSetChipEcoInfo(IN P_ADAPTER_T prAdapter)
{
	UINT_32 hw_version, sw_version = 0;
	struct mt66xx_chip_info *prChipInfo = prAdapter->chip_info;
	UINT_32 chip_id = prChipInfo->chip_id;
	/* WLAN_STATUS status; */
	WLAN_STATUS u4Status = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanSetChipEcoInfo.\n");

	if (wlanAccessRegister(prAdapter, TOP_HVR, &hw_version, 0, 0) != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR, "wlanSetChipEcoInfo >> get TOP_HVR failed.\n");
		u4Status = WLAN_STATUS_FAILURE;
	} else if (wlanAccessRegister(prAdapter, TOP_FVR, &sw_version, 0, 0) != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR, "wlanSetChipEcoInfo >> get TOP_FVR failed.\n");
		u4Status = WLAN_STATUS_FAILURE;
	} else {
		/* success */
		nicSetChipHwVer((UINT_8)(GET_HW_VER(hw_version) & 0xFF));
		nicSetChipFactoryVer((UINT_8)((GET_HW_VER(hw_version) >> 8) & 0xF));
		nicSetChipSwVer((UINT_8)GET_FW_VER(sw_version));

		/* Assign current chip version */
		prAdapter->chip_info->eco_ver = nicGetChipEcoVer(prAdapter);
	}

	DBGLOG(INIT, INFO, "Chip ID[%04X] Version[E%u] HW[0x%08x] SW[0x%08x]\n",
		chip_id, prAdapter->chip_info->eco_ver, hw_version, sw_version);

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
WLAN_STATUS wlanAccessRegister(IN P_ADAPTER_T prAdapter,
				   IN UINT_32 u4Addr, IN UINT_32 *pru4Result, IN UINT_32 u4Data,
				   IN UINT_8 ucSetQuery)
{
	P_CMD_INFO_T prCmdInfo;
	P_INIT_HIF_TX_HEADER_T prInitHifTxHeader;
	P_INIT_HIF_RX_HEADER_T prInitHifRxHeader;
	P_INIT_CMD_ACCESS_REG prInitCmdAccessReg;
	P_INIT_EVENT_ACCESS_REG prInitEventAccessReg;
	UINT_8 ucTC, ucCmdSeqNum;
	UINT_16 cmd_size;
	UINT_8 aucBuffer[sizeof(INIT_HIF_RX_HEADER_T) + sizeof(INIT_EVENT_ACCESS_REG)];
	WLAN_STATUS u4Status = WLAN_STATUS_SUCCESS;

	ASSERT(prAdapter);

	DEBUGFUNC("wlanAccessRegister");


	/* 1. Allocate CMD Info Packet and its Buffer. */
	cmd_size = sizeof(INIT_HIF_TX_HEADER_T) + sizeof(INIT_CMD_ACCESS_REG);
	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, cmd_size);

	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}

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
	prInitHifTxHeader = (P_INIT_HIF_TX_HEADER_T) (prCmdInfo->pucInfoBuffer);
	prInitHifTxHeader->u2TxByteCount = prCmdInfo->u2InfoBufLen;
	prInitHifTxHeader->u2PQ_ID = INIT_CMD_PQ_ID;
	prInitHifTxHeader->ucHeaderFormat = INIT_CMD_PACKET_TYPE_ID;
	prInitHifTxHeader->ucPktFt = INIT_PKT_FT_CMD;

	prInitHifTxHeader->rInitWifiCmd.ucCID = INIT_CMD_ID_ACCESS_REG;

	prInitHifTxHeader->rInitWifiCmd.ucPktTypeID = INIT_CMD_PACKET_TYPE_ID;
	prInitHifTxHeader->rInitWifiCmd.ucSeqNum = ucCmdSeqNum;

	/* 5. Setup CMD_ACCESS_REG */
	prInitCmdAccessReg = (P_INIT_CMD_ACCESS_REG) (prInitHifTxHeader->rInitWifiCmd.aucBuffer);
	prInitCmdAccessReg->ucSetQuery = ucSetQuery;
	prInitCmdAccessReg->u4Address = u4Addr;
	prInitCmdAccessReg->u4Data = u4Data;

	/* 6. Send CMD_ACCESS_REG command */
	while (1) {
		/* 6.1 Acquire TX Resource */
		if (nicTxAcquireResource
			(prAdapter, ucTC, nicTxGetPageCount(prCmdInfo->u2InfoBufLen, TRUE), TRUE)
				== WLAN_STATUS_RESOURCES) {
			if (nicTxPollingResource(prAdapter, ucTC) != WLAN_STATUS_SUCCESS) {
				u4Status = WLAN_STATUS_FAILURE;
				DBGLOG(INIT, ERROR, "Fail to get TX resource return within timeout\n");
				goto exit;
			}
			continue;
		}
		/* 6.2 Send CMD Info Packet */
		if (nicTxInitCmd(prAdapter, prCmdInfo) != WLAN_STATUS_SUCCESS) {
			u4Status = WLAN_STATUS_FAILURE;
			DBGLOG(INIT, ERROR, "Fail to transmit image download command\n");
			goto exit;
		}

		break;
	};

	/* 7. Wait for INIT_EVENT_ID_CMD_RESULT */
	u4Status = wlanAccessRegisterStatus(prAdapter, ucCmdSeqNum, ucSetQuery, aucBuffer, sizeof(aucBuffer));
	if (ucSetQuery == 0) {
		prInitHifRxHeader = (P_INIT_HIF_RX_HEADER_T)aucBuffer;
		prInitEventAccessReg = (P_INIT_EVENT_ACCESS_REG)prInitHifRxHeader->rInitWifiEvent.aucBuffer;

		if (prInitEventAccessReg->u4Address != u4Addr) {
			DBGLOG(INIT, ERROR, "Event reports address incorrect. 0x%08x, 0x%08x.\n",
				u4Addr, prInitEventAccessReg->u4Address);
			u4Status = WLAN_STATUS_FAILURE;
		}
		*pru4Result = prInitEventAccessReg->u4Data;
	}

exit:
	/* 8. Free CMD Info Packet. */
	cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

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
WLAN_STATUS wlanAccessRegisterStatus(IN P_ADAPTER_T prAdapter, IN UINT_8 ucCmdSeqNum,
									 IN UINT_8 ucSetQuery, IN PVOID prEvent,
									 IN UINT_32 u4EventLen)
{
	/* UINT_8 aucBuffer[sizeof(INIT_HIF_RX_HEADER_T) + sizeof(INIT_CMD_ACCESS_REG)]; */
	P_INIT_HIF_RX_HEADER_T prInitHifRxHeader;
/*	P_INIT_EVENT_CMD_RESULT prEventCmdResult;
*	P_INIT_CMD_ACCESS_REG prEventCmdAccessReg;
*/
	UINT_32 u4RxPktLength;
	WLAN_STATUS u4Status = WLAN_STATUS_SUCCESS;

	UINT_8 ucPortIdx = IMG_DL_STATUS_PORT_IDX;

	ASSERT(prAdapter);

	do {
		if (kalIsCardRemoved(prAdapter->prGlueInfo) == TRUE || fgIsBusAccessFailed == TRUE) {
			u4Status = WLAN_STATUS_FAILURE;
		} else if (nicRxWaitResponse(prAdapter,
					     ucPortIdx,
					     prEvent,
					     u4EventLen, &u4RxPktLength) != WLAN_STATUS_SUCCESS) {
			u4Status = WLAN_STATUS_FAILURE;
		} else {
			prInitHifRxHeader = (P_INIT_HIF_RX_HEADER_T) prEvent;

			/* EID / SeqNum check */
			if (((prInitHifRxHeader->rInitWifiEvent.ucEID != INIT_EVENT_ID_CMD_RESULT)
				&& (ucSetQuery == 1)) ||
				((prInitHifRxHeader->rInitWifiEvent.ucEID != INIT_EVENT_ID_ACCESS_REG)
				&& (ucSetQuery == 0))) {
				u4Status = WLAN_STATUS_FAILURE;
				DBGLOG(INIT, ERROR,
					"wlanAccessRegisterStatus: incorrect ucEID. ucSetQuery = 0x%x\n", ucSetQuery);
			} else if (prInitHifRxHeader->rInitWifiEvent.ucSeqNum != ucCmdSeqNum) {
				u4Status = WLAN_STATUS_FAILURE;
				DBGLOG(INIT, ERROR,
					"wlanAccessRegisterStatus: incorrect ucCmdSeqNum. = 0x%x\n", ucCmdSeqNum);
			} else {
			}
		}
	} while (FALSE);

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
WLAN_STATUS wlanProcessQueuedSwRfb(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfbListHead)
{
	P_SW_RFB_T prSwRfb, prNextSwRfb;
	P_TX_CTRL_T prTxCtrl;
	P_RX_CTRL_T prRxCtrl;

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
BOOLEAN wlanoidTimeoutCheck(IN P_ADAPTER_T prAdapter, IN PFN_OID_HANDLER_FUNC pfnOidHandler, IN UINT_32 u4Timeout)
{
	PFN_OID_HANDLER_FUNC *apfnOidHandlerWOTimeoutCheck;
	UINT_32 i;
	UINT_32 u4NumOfElem;
	UINT_32 u4OidTimeout;

	apfnOidHandlerWOTimeoutCheck = apfnOidWOTimeoutCheck;
	u4NumOfElem = sizeof(apfnOidWOTimeoutCheck) / sizeof(PFN_OID_HANDLER_FUNC);

	for (i = 0; i < u4NumOfElem; i++) {
		if (apfnOidHandlerWOTimeoutCheck[i] == pfnOidHandler)
			return FALSE;
	}

	/* Decrease OID timeout threshold if chip NoAck/resetting */
	if (wlanIsChipNoAck(prAdapter)) {
		u4OidTimeout = WLAN_OID_TIMEOUT_THRESHOLD_IN_RESETTING;
		DBGLOG(INIT, INFO, "Decrease OID timeout to %ums due to NoACK/CHIP-RESET\n", u4OidTimeout);
	} else {
		u4OidTimeout = u4Timeout;
	}

	/* Set OID timer for timeout check */
	cnmTimerStartTimer(prAdapter, &(prAdapter->rOidTimeoutTimer), u4OidTimeout);

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
* @brief This function is called to override network address
*        if NVRAM has a valid value
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
	PARAM_MAC_ADDRESS rMacAddr;
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
			DBGLOG(INIT, INFO, "Using dynamically generated MAC address");
#endif
			/* dynamic generate */
			u4SysTime = (UINT_32) kalGetTimeTick();

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
* @brief This function is called to update basic configuration into firmware domain
*
* @param prAdapter          Pointer to the Adapter structure.
*
* @return WLAN_STATUS_FAILURE   The request could not be processed
*         WLAN_STATUS_PENDING   The request has been queued for later processing
*         WLAN_STATUS_SUCCESS   The request has been processed
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanUpdateBasicConfig(IN P_ADAPTER_T prAdapter)
{
	UINT_8 ucCmdSeqNum;
	P_CMD_INFO_T prCmdInfo;
	P_WIFI_CMD_T prWifiCmd;
	P_CMD_BASIC_CONFIG_T prCmdBasicConfig;
	P_PSE_CMD_HDR_T prPseCmdHdr;
	WLAN_STATUS rResult;
	P_WIFI_VAR_T prWifiVar = &prAdapter->rWifiVar;

	DEBUGFUNC("wlanUpdateBasicConfig");

	ASSERT(prAdapter);

	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, CMD_HDR_SIZE + sizeof(CMD_BASIC_CONFIG_T));

	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}
	/* increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

	/* compose CMD_BUILD_CONNECTION cmd pkt */
	prCmdInfo->eCmdType = COMMAND_TYPE_GENERAL_IOCTL;
	prCmdInfo->u2InfoBufLen = CMD_HDR_SIZE + sizeof(CMD_BASIC_CONFIG_T);
	prCmdInfo->pfCmdDoneHandler = NULL;
	prCmdInfo->pfCmdTimeoutHandler = NULL;
	prCmdInfo->fgIsOid = FALSE;
	prCmdInfo->ucCID = CMD_ID_BASIC_CONFIG;
	prCmdInfo->fgSetQuery = TRUE;
	prCmdInfo->fgNeedResp = FALSE;
	prCmdInfo->ucCmdSeqNum = ucCmdSeqNum;
	prCmdInfo->u4SetInfoLen = sizeof(CMD_BASIC_CONFIG_T);

	/* Setup WIFI_CMD_T */
	prWifiCmd = (P_WIFI_CMD_T) (prCmdInfo->pucInfoBuffer);
	prWifiCmd->u2TxByteCount = prCmdInfo->u2InfoBufLen;
	prWifiCmd->u2PQ_ID = CMD_PQ_ID;
	prWifiCmd->ucPktTypeID = CMD_PACKET_TYPE_ID;
	prWifiCmd->ucCID = prCmdInfo->ucCID;
	prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
	prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;

	prPseCmdHdr = (P_PSE_CMD_HDR_T) (prCmdInfo->pucInfoBuffer);
	prPseCmdHdr->u2Qidx = TXD_Q_IDX_MCU_RQ0;
	prPseCmdHdr->u2Pidx = TXD_P_IDX_MCU;
	prPseCmdHdr->u2Hf = TXD_HF_CMD;
	prPseCmdHdr->u2Ft = TXD_FT_LONG_FORMAT;
	prPseCmdHdr->u2PktFt = TXD_PKT_FT_CMD;

	prWifiCmd->u2Length = prWifiCmd->u2TxByteCount - sizeof(PSE_CMD_HDR_T);

	/* configure CMD_BASIC_CONFIG */

	prCmdBasicConfig = (P_CMD_BASIC_CONFIG_T) (prWifiCmd->aucBuffer);
	kalMemZero(prCmdBasicConfig, sizeof(CMD_BASIC_CONFIG_T));
	prCmdBasicConfig->ucNative80211 = 0;
	prCmdBasicConfig->rCsumOffload.u2RxChecksum = 0;
	prCmdBasicConfig->rCsumOffload.u2TxChecksum = 0;
	prCmdBasicConfig->ucCtrlFlagAssertPath = prWifiVar->ucCtrlFlagAssertPath;
	prCmdBasicConfig->ucCtrlFlagDebugLevel = prWifiVar->ucCtrlFlagDebugLevel;

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

		if (prAdapter->u4CSUMFlags & (CSUM_OFFLOAD_EN_RX_IPv4 | CSUM_OFFLOAD_EN_RX_IPv6))
			prCmdBasicConfig->rCsumOffload.u2RxChecksum |= BIT(0);
	}
#endif

	rResult = wlanSendCommand(prAdapter, prCmdInfo);

	if (rResult != WLAN_STATUS_SUCCESS) {
		kalEnqueueCommand(prAdapter->prGlueInfo, (P_QUE_ENTRY_T) prCmdInfo);

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
BOOLEAN wlanQueryTestMode(IN P_ADAPTER_T prAdapter)
{
	ASSERT(prAdapter);

	return prAdapter->fgTestMode;
}

BOOLEAN wlanProcessTxFrame(IN P_ADAPTER_T prAdapter, IN P_NATIVE_PACKET prPacket)
{
	UINT_32 u4SysTime;
	UINT_8 ucMacHeaderLen;
	TX_PACKET_INFO rTxPacketInfo;
	struct mt66xx_chip_info *prChipInfo;

	ASSERT(prAdapter);
	ASSERT(prPacket);
	prChipInfo = prAdapter->chip_info;

	if (kalQoSFrameClassifierAndPacketInfo(prAdapter->prGlueInfo, prPacket, &rTxPacketInfo)) {

		/* Save the value of Priority Parameter */
		GLUE_SET_PKT_TID(prPacket, rTxPacketInfo.ucPriorityParam);

#if 1
		if (rTxPacketInfo.u2Flag) {
			if (rTxPacketInfo.u2Flag & BIT(ENUM_PKT_1X)) {
				P_STA_RECORD_T prStaRec;

				DBGLOG(RSN, INFO, "T1X len=%d\n", rTxPacketInfo.u4PacketLen);

				prStaRec = cnmGetStaRecByAddress(prAdapter,
								 GLUE_GET_PKT_BSS_IDX(prPacket),
								 rTxPacketInfo.aucEthDestAddr);

				GLUE_SET_PKT_FLAG(prPacket, ENUM_PKT_1X);
/*
*				if (secIsProtected1xFrame(prAdapter, prStaRec) && !kalIs24Of4Packet(prPacket))
*					GLUE_SET_PKT_FLAG(prPacket, ENUM_PKT_PROTECTED_1X);
*/
			}

			if (rTxPacketInfo.u2Flag & BIT(ENUM_PKT_NON_PROTECTED_1X))
				GLUE_SET_PKT_FLAG(prPacket, ENUM_PKT_NON_PROTECTED_1X);

			if (rTxPacketInfo.u2Flag & BIT(ENUM_PKT_802_3))
				GLUE_SET_PKT_FLAG(prPacket, ENUM_PKT_802_3);

			if (rTxPacketInfo.u2Flag & BIT(ENUM_PKT_VLAN_EXIST)
				&& FEAT_SUP_LLC_VLAN_TX(prChipInfo))
				GLUE_SET_PKT_FLAG(prPacket, ENUM_PKT_VLAN_EXIST);

			if (rTxPacketInfo.u2Flag & BIT(ENUM_PKT_DHCP))
				GLUE_SET_PKT_FLAG(prPacket, ENUM_PKT_DHCP);

			if (rTxPacketInfo.u2Flag & BIT(ENUM_PKT_ARP))
				GLUE_SET_PKT_FLAG(prPacket, ENUM_PKT_ARP);
		}
#else
		if (rTxPacketInfo.fgIs1X) {
			P_STA_RECORD_T prStaRec;

			DBGLOG(RSN, INFO, "T1X len=%d\n", rTxPacketInfo.u4PacketLen);

			prStaRec = cnmGetStaRecByAddress(prAdapter,
							 GLUE_GET_PKT_BSS_IDX(prPacket), rTxPacketInfo.aucEthDestAddr);

			GLUE_SET_PKT_FLAG(prPacket, ENUM_PKT_1X);

			if (secIsProtected1xFrame(prAdapter, prStaRec))
				GLUE_SET_PKT_FLAG(prPacket, ENUM_PKT_PROTECTED_1X);
		}

		if (rTxPacketInfo.fgIs802_3)
			GLUE_SET_PKT_FLAG(prPacket, ENUM_PKT_802_3);

		if (rTxPacketInfo.fgIsVlanExists)
			GLUE_SET_PKT_FLAG(prPacket, ENUM_PKT_VLAN_EXIST);

		if (rTxPacketInfo.fgIsDhcp)
			GLUE_SET_PKT_FLAG(prPacket, ENUM_PKT_DHCP);

		if (rTxPacketInfo.fgIsArp)
			GLUE_SET_PKT_FLAG(prPacket, ENUM_PKT_ARP);
#endif

		ucMacHeaderLen = ETHER_HEADER_LEN;

		/* Save the value of Header Length */
		GLUE_SET_PKT_HEADER_LEN(prPacket, ucMacHeaderLen);

		/* Save the value of Frame Length */
		GLUE_SET_PKT_FRAME_LEN(prPacket, (UINT_16) rTxPacketInfo.u4PacketLen);

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
BOOLEAN wlanProcessSecurityFrame(IN P_ADAPTER_T prAdapter, IN P_NATIVE_PACKET prPacket)
{
	P_CMD_INFO_T prCmdInfo;
	P_STA_RECORD_T prStaRec;
	UINT_8 ucBssIndex;
	UINT_32 u4PacketLen;
	UINT_8 aucEthDestAddr[PARAM_MAC_ADDR_LEN];
	P_MSDU_INFO_T prMsduInfo;
	UINT_8 ucStaRecIndex;

	ASSERT(prAdapter);
	ASSERT(prPacket);

	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, 0);

	/* Get MSDU_INFO for TxDone */
	prMsduInfo = cnmPktAlloc(prAdapter, 0);

	u4PacketLen = (UINT_32) GLUE_GET_PKT_FRAME_LEN(prPacket);

	if (prCmdInfo && prMsduInfo) {
		ucBssIndex = GLUE_GET_PKT_BSS_IDX(prPacket);

		kalGetEthDestAddr(prAdapter->prGlueInfo, prPacket, aucEthDestAddr);

		prStaRec = cnmGetStaRecByAddress(prAdapter, ucBssIndex, aucEthDestAddr);

		prCmdInfo->eCmdType = COMMAND_TYPE_SECURITY_FRAME;
		prCmdInfo->u2InfoBufLen = (UINT_16) u4PacketLen;
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
				   ucStaRecIndex, 0, u4PacketLen, nicTxDummyTxDone,
				   MSDU_RATE_MODE_AUTO, TX_PACKET_OS, 0, FALSE, TRUE);

		prMsduInfo->prPacket = prPacket;
		/* No Tx descriptor template for MMPDU */
		prMsduInfo->fgIsTXDTemplateValid = FALSE;

		if (GLUE_TEST_PKT_FLAG(prPacket, ENUM_PKT_PROTECTED_1X))
			nicTxConfigPktOption(prMsduInfo, MSDU_OPT_PROTECTED_FRAME, TRUE);
#if CFG_SUPPORT_MULTITHREAD
		nicTxComposeSecurityFrameDesc(prAdapter, prCmdInfo, prMsduInfo->aucTxDescBuffer, NULL);
#endif

		kalEnqueueCommand(prAdapter->prGlueInfo, (P_QUE_ENTRY_T) prCmdInfo);

		GLUE_SET_EVENT(prAdapter->prGlueInfo);

		return TRUE;
	}
		DBGLOG(RSN, INFO, "Failed to alloc CMD/MGMT INFO for 1X frame!!\n");
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
VOID wlanSecurityFrameTxDone(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	P_MSDU_INFO_T prMsduInfo = prCmdInfo->prMsduInfo;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	if (GET_BSS_INFO_BY_INDEX(prAdapter, prMsduInfo->ucBssIndex)->eNetworkType ==
	    NETWORK_TYPE_AIS && prAdapter->rWifiVar.rAisSpecificBssInfo.fgCounterMeasure) {
		P_STA_RECORD_T prSta = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucBssIndex);

		if (prSta) {
			kalMsleep(10);
			if (authSendDeauthFrame(prAdapter,
						GET_BSS_INFO_BY_INDEX(prAdapter,
								      prMsduInfo->ucBssIndex), prSta,
						(P_SW_RFB_T) NULL, REASON_CODE_MIC_FAILURE, (PFN_TX_DONE_HANDLER) NULL
						/* secFsmEventDeauthTxDone left upper layer handle the 60 timer */
			    ) != WLAN_STATUS_SUCCESS) {
				ASSERT(FALSE);
			}
			/* secFsmEventEapolTxDone(prAdapter, prSta, TX_RESULT_SUCCESS); */
		}
	}

	kalSecurityFrameSendComplete(prAdapter->prGlueInfo, prCmdInfo->prPacket, WLAN_STATUS_SUCCESS);
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

	/* clear scanning result */
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

	/* clear scanning result */
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
				    (((ULONG) pucIEPtr) + u4IELength -
				     ((ULONG) (&(prAdapter->rWlanInfo.aucScanIEBuf[0]))));

				kalMemCopy(pucIEPtr, (PUINT_8) (((ULONG) pucIEPtr) + u4IELength), u4IEMoveLength);

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

VOID wlanPrintVersion(IN P_ADAPTER_T prAdapter)
{
	P_WIFI_VER_INFO_T prVerInfo = &prAdapter->rVerInfo;
	tailer_format_t *prTailer;
	UINT_8 aucBuf[32], aucDate[32];

	kalMemCopy(aucBuf, prVerInfo->aucFwBranchInfo, 4);
	aucBuf[4] = '\0';
	DBGLOG(SW4, INFO, "N9 FW version %s-%u.%u.%u[DEC] (%s)\n",
		aucBuf, (UINT_32)(prVerInfo->u2FwOwnVersion >> 8),
		(UINT_32)(prVerInfo->u2FwOwnVersion & BITS(0, 7)),
		prVerInfo->ucFwBuildNumber, prVerInfo->aucFwDateCode);
#if CFG_SUPPORT_COMPRESSION_FW_OPTION
	if (prVerInfo->fgIsN9CompressedFW) {
		tailer_format_t_2 *prTailer;

		prTailer = &prVerInfo->rN9Compressedtailer;
		kalMemCopy(aucBuf, prTailer->ram_version, 10);
		aucBuf[10] = '\0';
		DBGLOG(SW4, INFO, "N9  tailer version %s (%s) info %u:E%u\n",
		aucBuf, prTailer->ram_built_date, prTailer->chip_info,
		prTailer->eco_code + 1);
	} else {
		prTailer = &prVerInfo->rN9tailer;
		kalMemCopy(aucBuf, prTailer->ram_version, 10);
		aucBuf[10] = '\0';
		DBGLOG(SW4, INFO, "N9  tailer version %s (%s) info %u:E%u\n",
		aucBuf, prTailer->ram_built_date, prTailer->chip_info,
		prTailer->eco_code + 1);
	}
	if (prVerInfo->fgIsCR4CompressedFW) {
		tailer_format_t_2 *prTailer;

		prTailer = &prVerInfo->rCR4Compressedtailer;
		kalMemCopy(aucBuf, prTailer->ram_version, 10);
		aucBuf[10] = '\0';
		DBGLOG(SW4, INFO, "CR4 tailer version %s (%s) info %u:E%u\n",
		aucBuf, prTailer->ram_built_date, prTailer->chip_info,
		prTailer->eco_code + 1);
	} else {
		prTailer = &prVerInfo->rCR4tailer;
		kalMemCopy(aucBuf, prTailer->ram_version, 10);
		aucBuf[10] = '\0';
		DBGLOG(SW4, INFO, "CR4 tailer version %s (%s) info %u:E%u\n",
		aucBuf, prTailer->ram_built_date, prTailer->chip_info,
		prTailer->eco_code + 1);
	}
#else
	prTailer = &prVerInfo->rN9tailer;
	kalMemCopy(aucBuf, prTailer->ram_version, 10);
	aucBuf[10] = '\0';
	DBGLOG(SW4, INFO, "N9  tailer version %s (%s) info %u:E%u\n",
		aucBuf, prTailer->ram_built_date, prTailer->chip_info,
		prTailer->eco_code + 1);

	prTailer = &prVerInfo->rCR4tailer;
	kalMemCopy(aucBuf, prTailer->ram_version, 10);
	aucBuf[10] = '\0';
	DBGLOG(SW4, INFO, "CR4 tailer version %s (%s) info %u:E%u\n",
		aucBuf, prTailer->ram_built_date, prTailer->chip_info,
		prTailer->eco_code + 1);
#endif
	if (!prVerInfo->fgPatchIsDlByDrv) {
		DBGLOG(SW4, INFO, "Patch is not downloaded by driver, read patch binary\n");
		wlanGetPatchInfo(prAdapter);
	}

	kalStrnCpy(aucBuf, prVerInfo->rPatchHeader.aucPlatform, 4);
	aucBuf[4] = '\0';
	kalStrnCpy(aucDate, prVerInfo->rPatchHeader.aucBuildDate, 16);
	aucDate[16] = '\0';
	DBGLOG(SW4, INFO, "Patch platform %s version 0x%04X %s\n",
		aucBuf, prVerInfo->rPatchHeader.u4PatchVersion, aucDate);

	DBGLOG(SW4, INFO, "Drv version %u.%u[DEC]\n",
		(UINT_32)(prVerInfo->u2FwPeerVersion >> 8),
		(UINT_32)(prVerInfo->u2FwPeerVersion & BITS(0, 7)));
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
	UINT_8 aucZeroMacAddr[] = NULL_MAC_ADDR;
	UINT_8 ucCmdSeqNum;
	P_CMD_INFO_T prCmdInfo;
	P_WIFI_CMD_T prWifiCmd;
	UINT_32 u4RxPktLength;
	UINT_8 aucBuffer[sizeof(WIFI_EVENT_T) + sizeof(EVENT_NIC_CAPABILITY_T)];
	P_HW_MAC_RX_DESC_T prRxStatus;
	P_WIFI_EVENT_T prEvent;
	P_EVENT_NIC_CAPABILITY_T prEventNicCapability;
	P_PSE_CMD_HDR_T prPseCmdHdr;

	ASSERT(prAdapter);

	DEBUGFUNC("wlanQueryNicCapability");

	/* 1. Allocate CMD Info Packet and its Buffer */
	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, CMD_HDR_SIZE + sizeof(EVENT_NIC_CAPABILITY_T));
	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}
	/* increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

	/* compose CMD_BUILD_CONNECTION cmd pkt */
	prCmdInfo->eCmdType = COMMAND_TYPE_GENERAL_IOCTL;
	prCmdInfo->u2InfoBufLen = CMD_HDR_SIZE + sizeof(EVENT_NIC_CAPABILITY_T);
	prCmdInfo->pfCmdDoneHandler = NULL;
	prCmdInfo->fgIsOid = FALSE;
	prCmdInfo->ucCID = CMD_ID_GET_NIC_CAPABILITY;
	prCmdInfo->fgSetQuery = FALSE;
	prCmdInfo->fgNeedResp = TRUE;
	prCmdInfo->ucCmdSeqNum = ucCmdSeqNum;
	prCmdInfo->u4SetInfoLen = 0;

	/* Setup WIFI_CMD_T */
	prWifiCmd = (P_WIFI_CMD_T) (prCmdInfo->pucInfoBuffer);
	prWifiCmd->u2TxByteCount = prCmdInfo->u2InfoBufLen;
	prWifiCmd->u2PQ_ID = CMD_PQ_ID;
	prWifiCmd->ucPktTypeID = CMD_PACKET_TYPE_ID;
	prWifiCmd->ucCID = prCmdInfo->ucCID;
	prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
	prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;

	prPseCmdHdr = (P_PSE_CMD_HDR_T) (prCmdInfo->pucInfoBuffer);
	prPseCmdHdr->u2Qidx = TXD_Q_IDX_MCU_RQ0;
	prPseCmdHdr->u2Pidx = TXD_P_IDX_MCU;
	prPseCmdHdr->u2Hf = TXD_HF_CMD;
	prPseCmdHdr->u2Ft = TXD_FT_LONG_FORMAT;
	prPseCmdHdr->u2PktFt = TXD_PKT_FT_CMD;

	prWifiCmd->u2Length = prWifiCmd->u2TxByteCount - sizeof(PSE_CMD_HDR_T);

	wlanSendCommand(prAdapter, prCmdInfo);

	cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

	while (TRUE) {
		if (nicRxWaitResponse(prAdapter, 1, aucBuffer,
			sizeof(WIFI_EVENT_T) + sizeof(EVENT_NIC_CAPABILITY_T), &u4RxPktLength) != WLAN_STATUS_SUCCESS) {

			DBGLOG(INIT, WARN, "%s: wait for event failed!\n", __func__);
			return WLAN_STATUS_FAILURE;
		}
		/* header checking .. */
		prRxStatus = (P_HW_MAC_RX_DESC_T) aucBuffer;
		if (prRxStatus->u2PktTYpe != RXM_RXD_PKT_TYPE_SW_EVENT) {
			DBGLOG(INIT, WARN, "%s: skip unexpected Rx pkt type[0x%04x]\n", __func__,
				prRxStatus->u2PktTYpe);
			continue;
		}

		prEvent = (P_WIFI_EVENT_T) aucBuffer;
		if (prEvent->ucEID != EVENT_ID_NIC_CAPABILITY) {
			DBGLOG(INIT, WARN, "%s: skip unexpected event ID[0x%02x]\n", __func__, prEvent->ucEID);
			continue;
		} else {
			break;
		}
	}

	prEventNicCapability = (P_EVENT_NIC_CAPABILITY_T) (prEvent->aucBuffer);

	prAdapter->rVerInfo.u2FwProductID = prEventNicCapability->u2ProductID;
	kalMemCopy(prAdapter->rVerInfo.aucFwBranchInfo, prEventNicCapability->aucBranchInfo, 4);
	prAdapter->rVerInfo.u2FwOwnVersion = prEventNicCapability->u2FwVersion;
	prAdapter->rVerInfo.ucFwBuildNumber = prEventNicCapability->ucFwBuildNumber;
	kalMemCopy(prAdapter->rVerInfo.aucFwDateCode, prEventNicCapability->aucDateCode, 16);
	prAdapter->rVerInfo.u2FwPeerVersion = prEventNicCapability->u2DriverVersion;
	prAdapter->fgIsHw5GBandDisabled = (BOOLEAN) prEventNicCapability->ucHw5GBandDisabled;
	prAdapter->fgIsEepromUsed = (BOOLEAN) prEventNicCapability->ucEepromUsed;
	prAdapter->fgIsEmbbededMacAddrValid = (BOOLEAN)
	    (!IS_BMCAST_MAC_ADDR(prEventNicCapability->aucMacAddr) &&
	     !EQUAL_MAC_ADDR(aucZeroMacAddr, prEventNicCapability->aucMacAddr));

	COPY_MAC_ADDR(prAdapter->rWifiVar.aucPermanentAddress, prEventNicCapability->aucMacAddr);
	COPY_MAC_ADDR(prAdapter->rWifiVar.aucMacAddress, prEventNicCapability->aucMacAddr);

	prAdapter->rWifiVar.ucStaVht &= (!(prEventNicCapability->ucHwNotSupportAC));
	prAdapter->rWifiVar.ucApVht &= (!(prEventNicCapability->ucHwNotSupportAC));
	prAdapter->rWifiVar.ucP2pGoVht &= (!(prEventNicCapability->ucHwNotSupportAC));
	prAdapter->rWifiVar.ucP2pGcVht &= (!(prEventNicCapability->ucHwNotSupportAC));

	prAdapter->rWifiVar.ucStaVhtBfee &= (!(prEventNicCapability->ucHwNotSupportAC));
	prAdapter->rWifiVar.ucStaVhtMuBfee &= (!(prEventNicCapability->ucHwNotSupportAC));
	prAdapter->rWifiVar.ucStaVhtBfer &= (!(prEventNicCapability->ucHwNotSupportAC));

	prAdapter->rWifiVar.ucVhtAmsduInAmpduRx &= (!(prEventNicCapability->ucHwNotSupportAC));
	prAdapter->rWifiVar.ucVhtAmsduInAmpduTx &= (!(prEventNicCapability->ucHwNotSupportAC));

	if (prEventNicCapability->ucHwNotSupportAC) {
		prAdapter->rWifiVar.ucStaBandwidth = MAX_BW_40MHZ;
		prAdapter->rWifiVar.ucSta5gBandwidth =  MAX_BW_40MHZ;
		prAdapter->rWifiVar.ucP2p5gBandwidth = MAX_BW_40MHZ;
		prAdapter->rWifiVar.ucApBandwidth = MAX_BW_40MHZ;
		prAdapter->rWifiVar.ucAp5gBandwidth = MAX_BW_40MHZ;
#if CFG_SUPPORT_MTK_SYNERGY
		/* Disable the 2.4G 256QAM feature bit if N only chip*/
		prAdapter->rWifiVar.aucMtkFeature[0] &=
			~(MTK_SYNERGY_CAP_SUPPORT_24G_MCS89);
		DBGLOG(INIT, WARN,
			"Disable 2.4G 256QAM support if N only chip\n");
#endif
	}

	prAdapter->u4FwCompileFlag0 = prEventNicCapability->u4CompileFlag0;
	prAdapter->u4FwCompileFlag1 = prEventNicCapability->u4CompileFlag1;
	prAdapter->u4FwFeatureFlag0 = prEventNicCapability->u4FeatureFlag0;
	prAdapter->u4FwFeatureFlag1 = prEventNicCapability->u4FeatureFlag1;

	if (prEventNicCapability->ucHwSetNss1x1)
		prAdapter->rWifiVar.ucNSS = 1;

#if CFG_SUPPORT_DBDC
	if (prEventNicCapability->ucHwNotSupportDBDC)
		prAdapter->rWifiVar.ucDbdcMode = DBDC_MODE_DISABLED;
#endif

#if CFG_ENABLE_CAL_LOG
	DBGLOG(INIT, TRACE, "RF CAL FAIL  = (%d),BB CAL FAIL  = (%d)\n",
	       prEventNicCapability->ucRfCalFail, prEventNicCapability->ucBbCalFail);
#endif

#if CFG_SISO_SW_DEVELOP
	if ((!prEventNicCapability->ucHwNotSupportDBDC) &&
		(!prEventNicCapability->ucHwSetNss1x1) &&
		(!prEventNicCapability->ucHwWiFiZeroOnly)) {
		prAdapter->rWifiFemCfg.u2WifiPath =
			(WLAN_FLAG_2G4_WF0 | WLAN_FLAG_5G_WF0 | WLAN_FLAG_2G4_WF1 | WLAN_FLAG_5G_WF1);
	} else if ((!prEventNicCapability->ucHwNotSupportDBDC) &&
			(prEventNicCapability->ucHwSetNss1x1) &&
			(!prEventNicCapability->ucHwWiFiZeroOnly)) {
		prAdapter->rWifiFemCfg.u2WifiPath =
			(WLAN_FLAG_5G_WF0 | WLAN_FLAG_2G4_WF1);
	} else if ((prEventNicCapability->ucHwNotSupportDBDC) &&
		(prEventNicCapability->ucHwSetNss1x1) &&
		(!prEventNicCapability->ucHwWiFiZeroOnly)) {
		prAdapter->rWifiFemCfg.u2WifiPath =
			(WLAN_FLAG_5G_WF0 | WLAN_FLAG_2G4_WF1);
	} else if ((prEventNicCapability->ucHwNotSupportDBDC) &&
		(!prEventNicCapability->ucHwSetNss1x1) &&
		(!prEventNicCapability->ucHwWiFiZeroOnly)) {
		prAdapter->rWifiFemCfg.u2WifiPath =
			(WLAN_FLAG_2G4_WF0 | WLAN_FLAG_5G_WF0 | WLAN_FLAG_2G4_WF1 | WLAN_FLAG_5G_WF1);
	} else if ((prEventNicCapability->ucHwNotSupportDBDC) &&
		(prEventNicCapability->ucHwSetNss1x1) &&
		(prEventNicCapability->ucHwWiFiZeroOnly)) {
		prAdapter->rWifiFemCfg.u2WifiPath =
			(WLAN_FLAG_2G4_WF0 | WLAN_FLAG_5G_WF0);
	} else {
		DBGLOG(INIT, ERROR, "ucHwNotSupportDBDC = %d\n", prEventNicCapability->ucHwNotSupportDBDC);
		DBGLOG(INIT, ERROR, "ucHwSetNss1x1 = %d\n", prEventNicCapability->ucHwSetNss1x1);
		DBGLOG(INIT, ERROR, "ucHwWiFiZeroOnly = %d\n", prEventNicCapability->ucHwWiFiZeroOnly);
		ASSERT(0);
	}

	DBGLOG(INIT, INFO, "wifi path = %8x\n", prAdapter->rWifiFemCfg.u2WifiPath);
#endif

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
WLAN_STATUS wlanQueryPdMcr(IN P_ADAPTER_T prAdapter, P_PARAM_MCR_RW_STRUCT_T prMcrRdInfo)
{
	UINT_8 ucCmdSeqNum;
	P_CMD_INFO_T prCmdInfo;
	P_WIFI_CMD_T prWifiCmd;
	UINT_32 u4RxPktLength;
	UINT_8 aucBuffer[sizeof(WIFI_EVENT_T) + sizeof(CMD_ACCESS_REG)];
	P_HW_MAC_RX_DESC_T prRxStatus;
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
	prCmdInfo->ucCmdSeqNum = ucCmdSeqNum;
	prCmdInfo->u4SetInfoLen = sizeof(CMD_ACCESS_REG);

	/* Setup WIFI_CMD_T */
	prWifiCmd = (P_WIFI_CMD_T) (prCmdInfo->pucInfoBuffer);
	prWifiCmd->u2TxByteCount = prCmdInfo->u2InfoBufLen;
	prWifiCmd->u2PQ_ID = CMD_PQ_ID;
	prWifiCmd->ucPktTypeID = CMD_PACKET_TYPE_ID;
	prWifiCmd->ucCID = prCmdInfo->ucCID;
	prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
	prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;
	kalMemCopy(prWifiCmd->aucBuffer, prMcrRdInfo, sizeof(CMD_ACCESS_REG));

	wlanSendCommand(prAdapter, prCmdInfo);
	cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

	if (nicRxWaitResponse(prAdapter,
			      1,
			      aucBuffer,
			      sizeof(WIFI_EVENT_T) + sizeof(CMD_ACCESS_REG), &u4RxPktLength) != WLAN_STATUS_SUCCESS) {
		return WLAN_STATUS_FAILURE;
	}
	/* header checking .. */
	prRxStatus = (P_HW_MAC_RX_DESC_T) aucBuffer;
	if (prRxStatus->u2PktTYpe != RXM_RXD_PKT_TYPE_SW_EVENT)
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

	auTxPwrOffset_Round = (UINT_8) au4TxPwrOffset_Round;

	return au4TxPwrOffset_Round;
}

#endif

#if CFG_SUPPORT_NVRAM_5G
WLAN_STATUS wlanLoadManufactureData_5G(IN P_ADAPTER_T prAdapter, IN P_REG_INFO_T prRegInfo)
{

	P_BANDEDGE_5G_T pr5GBandEdge;

	ASSERT(prAdapter);

	pr5GBandEdge = &prRegInfo->prOldEfuseMapping->r5GBandEdgePwr;

	/* 1. set band edge tx power if available */
	if (pr5GBandEdge->uc5GBandEdgePwrUsed != 0) {
		CMD_EDGE_TXPWR_LIMIT_T rCmdEdgeTxPwrLimit;

		rCmdEdgeTxPwrLimit.cBandEdgeMaxPwrCCK = 0;
		rCmdEdgeTxPwrLimit.cBandEdgeMaxPwrOFDM20 = pr5GBandEdge->c5GBandEdgeMaxPwrOFDM20;
		rCmdEdgeTxPwrLimit.cBandEdgeMaxPwrOFDM40 = pr5GBandEdge->c5GBandEdgeMaxPwrOFDM40;
		rCmdEdgeTxPwrLimit.cBandEdgeMaxPwrOFDM80 = pr5GBandEdge->c5GBandEdgeMaxPwrOFDM80;

		wlanSendSetQueryCmd(prAdapter,
				    CMD_ID_SET_EDGE_TXPWR_LIMIT_5G,
				    TRUE,
				    FALSE,
				    FALSE,
				    NULL,
				    NULL, sizeof(CMD_EDGE_TXPWR_LIMIT_T), (PUINT_8) &rCmdEdgeTxPwrLimit, NULL, 0);

		/* dumpMemory8(&rCmdEdgeTxPwrLimit,4); */
	}

	DEBUGFUNC("wlanLoadManufactureData_5G");

	/*2.set channel offset for 8 sub-band */
	if (prRegInfo->prOldEfuseMapping->uc5GChannelOffsetVaild) {
		CMD_POWER_OFFSET_T rCmdPowerOffset;
		UINT_8 i;

		rCmdPowerOffset.ucBand = BAND_5G;
		for (i = 0; i < MAX_SUBBAND_NUM_5G; i++)
			rCmdPowerOffset.ucSubBandOffset[i] = prRegInfo->prOldEfuseMapping->auc5GChOffset[i];

		wlanSendSetQueryCmd(prAdapter,
				    CMD_ID_SET_CHANNEL_PWR_OFFSET,
				    TRUE,
				    FALSE,
				    FALSE, NULL, NULL, sizeof(rCmdPowerOffset), (PUINT_8) &rCmdPowerOffset, NULL, 0);
		/* dumpMemory8(&rCmdPowerOffset,9); */
	}

	/*3.set 5G AC power */
	if (prRegInfo->prOldEfuseMapping->uc11AcTxPwrValid) {

		CMD_TX_AC_PWR_T rCmdAcPwr;

		kalMemCopy(&rCmdAcPwr.rAcPwr, &prRegInfo->prOldEfuseMapping->r11AcTxPwr, sizeof(AC_PWR_SETTING_STRUCT));
		rCmdAcPwr.ucBand = BAND_5G;

		wlanSendSetQueryCmd(prAdapter,
				    CMD_ID_SET_80211AC_TX_PWR,
				    TRUE,
				    FALSE, FALSE, NULL, NULL, sizeof(CMD_TX_AC_PWR_T), (PUINT_8) &rCmdAcPwr, NULL, 0);
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
WLAN_STATUS wlanLoadManufactureData(IN P_ADAPTER_T prAdapter, IN P_REG_INFO_T prRegInfo)
{
#if CFG_SUPPORT_RDD_TEST_MODE
	CMD_RDD_CH_T rRddParam;
#endif
	CMD_NVRAM_SETTING_T rCmdNvramSettings;

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
	    || prAdapter->rVerInfo.u2Part1CfgOwnVersion < CFG_DRV_PEER_VERSION
	    || prAdapter->rVerInfo.u2Part2CfgOwnVersion < CFG_DRV_PEER_VERSION) {
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

			nicUpdateTxPower(prAdapter, (P_CMD_TX_PWR_T) (&(prRegInfo->rTxPwr)));
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
		if (prAdapter->fgIsHw5GBandDisabled || prRegInfo->ucSupport5GBand == 0)
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
			CMD_POWER_OFFSET_T rCmdPowerOffset;
			UINT_8 i;

			rCmdPowerOffset.ucBand = BAND_2G4;
			for (i = 0; i < 3; i++)
				rCmdPowerOffset.ucSubBandOffset[i] = prRegInfo->prOldEfuseMapping->aucChOffset[i];
			rCmdPowerOffset.ucSubBandOffset[i] = prRegInfo->prOldEfuseMapping->acAllChannelOffset;

			wlanSendSetQueryCmd(prAdapter,
					    CMD_ID_SET_CHANNEL_PWR_OFFSET,
					    TRUE,
					    FALSE,
					    FALSE,
					    NULL, NULL, sizeof(rCmdPowerOffset), (PUINT_8) &rCmdPowerOffset, NULL, 0);
			/* dumpMemory8(&rCmdPowerOffset,9); */
		}
	}
#else

	wlanSendSetQueryCmd(prAdapter,
			    CMD_ID_SET_PHY_PARAM,
			    TRUE,
			    FALSE,
			    FALSE, NULL, NULL, sizeof(CMD_PHY_PARAM_T), (PUINT_8) (prRegInfo->aucEFUSE), NULL, 0);

#endif
	/*RSSI path compasation */
	if (prRegInfo->ucRssiPathCompasationUsed) {
		CMD_RSSI_PATH_COMPASATION_T rCmdRssiPathCompasation;

		rCmdRssiPathCompasation.c2GRssiCompensation = prRegInfo->rRssiPathCompasation.c2GRssiCompensation;
		rCmdRssiPathCompasation.c5GRssiCompensation = prRegInfo->rRssiPathCompasation.c5GRssiCompensation;

		wlanSendSetQueryCmd(prAdapter,
				    CMD_ID_SET_PATH_COMPASATION,
				    TRUE,
				    FALSE,
				    FALSE,
				    NULL,
				    NULL,
				    sizeof(rCmdRssiPathCompasation), (PUINT_8) &rCmdRssiPathCompasation, NULL, 0);
	}
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

#if 0				/* Bandwidth control will be controlled by GUI. 20110930
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


	/* 7. set band edge tx power if available */
	if (prRegInfo->fg2G4BandEdgePwrUsed) {
		CMD_EDGE_TXPWR_LIMIT_T rCmdEdgeTxPwrLimit;

		rCmdEdgeTxPwrLimit.cBandEdgeMaxPwrCCK = prRegInfo->cBandEdgeMaxPwrCCK;
		rCmdEdgeTxPwrLimit.cBandEdgeMaxPwrOFDM20 = prRegInfo->cBandEdgeMaxPwrOFDM20;
		rCmdEdgeTxPwrLimit.cBandEdgeMaxPwrOFDM40 = prRegInfo->cBandEdgeMaxPwrOFDM40;

		wlanSendSetQueryCmd(prAdapter,
				    CMD_ID_SET_EDGE_TXPWR_LIMIT,
				    TRUE,
				    FALSE,
				    FALSE,
				    NULL,
				    NULL, sizeof(CMD_EDGE_TXPWR_LIMIT_T), (PUINT_8) &rCmdEdgeTxPwrLimit, NULL, 0);
	}
	/*8. Set 2.4G AC power */
	if (prRegInfo->prOldEfuseMapping && prRegInfo->prOldEfuseMapping->uc11AcTxPwrValid2G) {

		CMD_TX_AC_PWR_T rCmdAcPwr;

		kalMemCopy(&rCmdAcPwr.rAcPwr, &prRegInfo->prOldEfuseMapping->r11AcTxPwr2G,
			   sizeof(AC_PWR_SETTING_STRUCT));
		rCmdAcPwr.ucBand = BAND_2G4;

		wlanSendSetQueryCmd(prAdapter,
				    CMD_ID_SET_80211AC_TX_PWR,
				    TRUE,
				    FALSE, FALSE, NULL, NULL, sizeof(CMD_TX_AC_PWR_T), (PUINT_8) &rCmdAcPwr, NULL, 0);
		/* dumpMemory8(&rCmdAcPwr,9); */
	}
	/* 9. Send the full Parameters of NVRAM to FW */

	kalMemCopy(&rCmdNvramSettings.rNvramSettings,
		   &prRegInfo->prNvramSettings->u2Part1OwnVersion, sizeof(WIFI_CFG_PARAM_STRUCT));
	ASSERT(sizeof(WIFI_CFG_PARAM_STRUCT) == 512);
	wlanSendSetQueryCmd(prAdapter,
			    CMD_ID_SET_NVRAM_SETTINGS,
			    TRUE,
			    FALSE,
			    FALSE, NULL, NULL, sizeof(rCmdNvramSettings), (PUINT_8) &rCmdNvramSettings, NULL, 0);

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

	for (i = 0; i < MBOX_ID_TOTAL_NUM; i++)
		mboxRcvAllMsg(prAdapter, (ENUM_MBOX_ID_T) i);

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

	ASSERT(prAdapter);

	prTxCtrl = &prAdapter->rTxCtrl;

	prMsduInfo = cnmPktAlloc(prAdapter, 0);

	if (!prMsduInfo)
		return WLAN_STATUS_RESOURCES;

	if (nicTxFillMsduInfo(prAdapter, prMsduInfo, prNativePacket)) {
		/* prMsduInfo->eSrc = TX_PACKET_OS; */

		/* Tx profiling */
		wlanTxProfilingTagMsdu(prAdapter, prMsduInfo, TX_PROF_TAG_DRV_ENQUE);

		/* enqueue to QM */
		nicTxEnqueueMsdu(prAdapter, prMsduInfo);

		return WLAN_STATUS_SUCCESS;
	}
		kalSendComplete(prAdapter->prGlueInfo, prNativePacket, WLAN_STATUS_INVALID_PACKET);

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

#if !CFG_SUPPORT_MULTITHREAD
	ASSERT(pfgHwAccess);
#endif

	/* <1> dequeue packet by txDequeuTxPackets() */
#if CFG_SUPPORT_MULTITHREAD
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_QM_TX_QUEUE);
	prMsduInfo = qmDequeueTxPacketsMthread(prAdapter, &prTxCtrl->rTc);
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
WLAN_STATUS wlanAcquirePowerControl(IN P_ADAPTER_T prAdapter)
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
WLAN_STATUS wlanReleasePowerControl(IN P_ADAPTER_T prAdapter)
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
UINT_32 wlanGetTxPendingFrameCount(IN P_ADAPTER_T prAdapter)
{
	P_TX_CTRL_T prTxCtrl;
	UINT_32 u4Num;

	ASSERT(prAdapter);
	prTxCtrl = &prAdapter->rTxCtrl;

	u4Num = kalGetTxPendingFrameCount(prAdapter->prGlueInfo) +
		(UINT_32) GLUE_GET_REF_CNT(prTxCtrl->i4PendingFwdFrameCount);

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
	UINT_8 ucEcoVersion;

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
uint8_t wlanGetRomVersion(IN P_ADAPTER_T prAdapter)
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
VOID wlanDefTxPowerCfg(IN P_ADAPTER_T prAdapter)
{
	UINT_8 i;
	P_GLUE_INFO_T prGlueInfo = prAdapter->prGlueInfo;
	P_SET_TXPWR_CTRL_T prTxpwr;

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
VOID wlanSetPreferBandByNetwork(IN P_ADAPTER_T prAdapter, IN ENUM_BAND_T eBand, IN UINT_8 ucBssIndex)
{
	ASSERT(prAdapter);
	ASSERT(eBand <= BAND_NUM);
	ASSERT(ucBssIndex <= MAX_BSS_INDEX);


	/* 1. set prefer band according to network type */
	prAdapter->aePreferBand[ucBssIndex] = eBand;

	/* 2. remove buffered BSS descriptors correspondingly */
	if (eBand == BAND_2G4)
		scanRemoveBssDescByBandAndNetwork(prAdapter, BAND_5G, ucBssIndex);
	else if (eBand == BAND_5G)
		scanRemoveBssDescByBandAndNetwork(prAdapter, BAND_2G4, ucBssIndex);

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
UINT_8 wlanGetChannelNumberByNetwork(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex)
{
	P_BSS_INFO_T prBssInfo;

	ASSERT(prAdapter);
	ASSERT(ucBssIndex <= MAX_BSS_INDEX);

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
WLAN_STATUS wlanCheckSystemConfiguration(IN P_ADAPTER_T prAdapter)
{
#if (CFG_NVRAM_EXISTENCE_CHECK == 1) || (CFG_SW_NVRAM_VERSION_CHECK == 1)
	const UINT_8 aucZeroMacAddr[] = NULL_MAC_ADDR;
	BOOLEAN fgIsConfExist = TRUE;
	BOOLEAN fgGenErrMsg = FALSE;
	P_REG_INFO_T prRegInfo = NULL;
#if 0
	const UINT_8 aucBCAddr[] = BC_MAC_ADDR;
	P_WLAN_BEACON_FRAME_T prBeacon = NULL;
	P_IE_SSID_T prSsid = NULL;
	UINT_32 u4ErrCode = 0;
	UINT_8 aucErrMsg[32];
	PARAM_SSID_T rSsid;
	PARAM_802_11_CONFIG_T rConfiguration;
	PARAM_RATES_EX rSupportedRates;
#endif
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

#if (CFG_SUPPORT_PWR_LIMIT_COUNTRY == 1)
	if (fgIsConfExist == TRUE && (prAdapter->rVerInfo.u2Part1CfgPeerVersion > CFG_DRV_OWN_VERSION
					  || prAdapter->rVerInfo.u2Part2CfgPeerVersion > CFG_DRV_OWN_VERSION
					  || prAdapter->rVerInfo.u2Part1CfgOwnVersion < CFG_DRV_PEER_VERSION
					  || prAdapter->rVerInfo.u2Part2CfgOwnVersion < CFG_DRV_PEER_VERSION/* NVRAM */
				      || prAdapter->rVerInfo.u2FwPeerVersion > CFG_DRV_OWN_VERSION
				      || prAdapter->rVerInfo.u2FwOwnVersion < CFG_DRV_PEER_VERSION
				      || (prAdapter->fgIsEmbbededMacAddrValid == FALSE &&
					  (IS_BMCAST_MAC_ADDR(prRegInfo->aucMacAddr)
					  || EQUAL_MAC_ADDR(aucZeroMacAddr, prRegInfo->aucMacAddr)))
				      || prRegInfo->ucTxPwrValid == 0
					  || prAdapter->fgIsPowerLimitTableValid == FALSE))
		fgGenErrMsg = TRUE;
#else
	if (fgIsConfExist == TRUE && (prAdapter->rVerInfo.u2Part1CfgPeerVersion > CFG_DRV_OWN_VERSION
					  || prAdapter->rVerInfo.u2Part2CfgPeerVersion > CFG_DRV_OWN_VERSION
					  || prAdapter->rVerInfo.u2Part1CfgOwnVersion < CFG_DRV_PEER_VERSION
					  || prAdapter->rVerInfo.u2Part2CfgOwnVersion < CFG_DRV_PEER_VERSION/* NVRAM */
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
		prBeacon = cnmMemAlloc(prAdapter, RAM_TYPE_BUF, sizeof(WLAN_BEACON_FRAME_T) + sizeof(IE_SSID_T));

		/* initialization */
		kalMemZero(prBeacon, sizeof(WLAN_BEACON_FRAME_T) + sizeof(IE_SSID_T));

		/* prBeacon initialization */
		prBeacon->u2FrameCtrl = MAC_FRAME_BEACON;
		COPY_MAC_ADDR(prBeacon->aucDestAddr, aucBCAddr);
		COPY_MAC_ADDR(prBeacon->aucSrcAddr, aucZeroMacAddr);
		COPY_MAC_ADDR(prBeacon->aucBSSID, aucZeroMacAddr);
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
	if (kalIsConfigurationExist(prAdapter->prGlueInfo) == FALSE) {
		COPY_SSID(prSsid->aucSSID, prSsid->ucLength, NVRAM_ERR_MSG, (UINT_8) (strlen(NVRAM_ERR_MSG)));

		kalIndicateBssInfo(prAdapter->prGlueInfo,
				   (PUINT_8) prBeacon,
				   OFFSET_OF(WLAN_BEACON_FRAME_T,
					     aucInfoElem) + OFFSET_OF(IE_SSID_T, aucSSID) + prSsid->ucLength, 1, 0);

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
#endif

#if (CFG_SW_NVRAM_VERSION_CHECK == 1)
#define VER_ERR_MSG     "NVRAM WARNING: Err = 0x%02X"
	if (fgIsConfExist == TRUE) {
		if ((prAdapter->rVerInfo.u2Part1CfgPeerVersion > CFG_DRV_OWN_VERSION
			 || prAdapter->rVerInfo.u2Part2CfgPeerVersion > CFG_DRV_OWN_VERSION
			 || prAdapter->rVerInfo.u2Part1CfgOwnVersion < CFG_DRV_PEER_VERSION
			 || prAdapter->rVerInfo.u2Part2CfgOwnVersion < CFG_DRV_PEER_VERSION	/* NVRAM */
		     || prAdapter->rVerInfo.u2FwPeerVersion > CFG_DRV_OWN_VERSION
		     || prAdapter->rVerInfo.u2FwOwnVersion < CFG_DRV_PEER_VERSION))
			u4ErrCode |= NVRAM_ERROR_VERSION_MISMATCH;

		if (prRegInfo->ucTxPwrValid == 0)
			u4ErrCode |= NVRAM_ERROR_INVALID_TXPWR;

		if (prAdapter->fgIsEmbbededMacAddrValid == FALSE && (IS_BMCAST_MAC_ADDR(prRegInfo->aucMacAddr)
								     || EQUAL_MAC_ADDR(aucZeroMacAddr,
										       prRegInfo->aucMacAddr))) {
			u4ErrCode |= NVRAM_ERROR_INVALID_MAC_ADDR;
		}
#if CFG_SUPPORT_PWR_LIMIT_COUNTRY
		if (prAdapter->fgIsPowerLimitTableValid == FALSE)
			u4ErrCode |= NVRAM_POWER_LIMIT_TABLE_INVALID;
#endif
		if (u4ErrCode != 0) {
			sprintf(aucErrMsg, VER_ERR_MSG, (unsigned int)u4ErrCode);
			COPY_SSID(prSsid->aucSSID, prSsid->ucLength, aucErrMsg, (UINT_8) (strlen(aucErrMsg)));

			kalIndicateBssInfo(prAdapter->prGlueInfo,
					   (PUINT_8) prBeacon,
					   OFFSET_OF(WLAN_BEACON_FRAME_T,
						     aucInfoElem) + OFFSET_OF(IE_SSID_T,
									      aucSSID) + prSsid->ucLength, 1, 0);

			COPY_SSID(rSsid.aucSsid, rSsid.u4SsidLen, NVRAM_ERR_MSG, strlen(NVRAM_ERR_MSG));
			nicAddScanResult(prAdapter, prBeacon->aucBSSID, &rSsid, 0, 0,
					 PARAM_NETWORK_TYPE_FH, &rConfiguration, NET_TYPE_INFRA,
					 rSupportedRates, OFFSET_OF(WLAN_BEACON_FRAME_T,
								    aucInfoElem) +
					 OFFSET_OF(IE_SSID_T,
						   aucSSID) + prSsid->ucLength -
					 WLAN_MAC_MGMT_HEADER_LEN,
					 (PUINT_8) ((ULONG) (prBeacon) + WLAN_MAC_MGMT_HEADER_LEN));
		}
	}
#endif

	if (fgGenErrMsg == TRUE)
		cnmMemFree(prAdapter, prBeacon);
#endif
	return WLAN_STATUS_SUCCESS;
}

WLAN_STATUS
wlanoidQueryBssStatistics(IN P_ADAPTER_T prAdapter,
			  IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	P_PARAM_GET_BSS_STATISTICS prQueryBssStatistics;
	P_BSS_INFO_T prBssInfo;
	P_STA_RECORD_T prStaRec;
	WLAN_STATUS rResult = WLAN_STATUS_FAILURE;
	UINT_8 ucBssIndex;
	ENUM_WMM_ACI_T eAci;

	DEBUGFUNC("wlanoidQueryBssStatistics");

	do {
		ASSERT(pvQueryBuffer);

		/* 4 1. Sanity test */
		if ((prAdapter == NULL) || (pu4QueryInfoLen == NULL))
			break;

		if ((u4QueryBufferLen) && (pvQueryBuffer == NULL))
			break;

		if (u4QueryBufferLen < sizeof(P_PARAM_GET_BSS_STATISTICS)) {
			*pu4QueryInfoLen = sizeof(P_PARAM_GET_BSS_STATISTICS);
			rResult = WLAN_STATUS_BUFFER_TOO_SHORT;
			break;
		}

		prQueryBssStatistics = (P_PARAM_GET_BSS_STATISTICS) pvQueryBuffer;
		*pu4QueryInfoLen = sizeof(PARAM_GET_BSS_STATISTICS);

		ucBssIndex = prQueryBssStatistics->ucBssIndex;
		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

		if (prBssInfo) {	/*AIS*/
			if (prBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE) {
				prStaRec = prBssInfo->prStaRecOfAP;
				if (prStaRec) {
					for (eAci = 0; eAci < WMM_AC_INDEX_NUM; eAci++) {
						prQueryBssStatistics->arLinkStatistics[eAci].u4TxMsdu =
						    prStaRec->arLinkStatistics[eAci].u4TxMsdu;
						prQueryBssStatistics->arLinkStatistics[eAci].u4RxMsdu =
						    prStaRec->arLinkStatistics[eAci].u4RxMsdu;
						prQueryBssStatistics->arLinkStatistics[eAci].u4TxDropMsdu =
						    prStaRec->arLinkStatistics[eAci].u4TxDropMsdu +
						    prBssInfo->arLinkStatistics[eAci].u4TxDropMsdu;
						prQueryBssStatistics->arLinkStatistics[eAci].u4TxFailMsdu =
						    prStaRec->arLinkStatistics[eAci].u4TxFailMsdu;
						prQueryBssStatistics->arLinkStatistics[eAci].u4TxRetryMsdu =
						    prStaRec->arLinkStatistics[eAci].u4TxRetryMsdu;
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

VOID wlanDumpBssStatistics(IN P_ADAPTER_T prAdapter, UINT_8 ucBssIdx)
{
	P_BSS_INFO_T prBssInfo;
	P_STA_RECORD_T prStaRec;
	ENUM_WMM_ACI_T eAci;
	WIFI_WMM_AC_STAT_T arLLStats[WMM_AC_INDEX_NUM];
	UINT_8 ucIdx;

	if (ucBssIdx > MAX_BSS_INDEX) {
		DBGLOG(SW4, INFO, "Invalid BssInfo index[%u], skip dump!\n", ucBssIdx);
		return;
	}

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIdx);
	if (!prBssInfo) {
		DBGLOG(SW4, INFO, "Invalid BssInfo index[%u], skip dump!\n", ucBssIdx);
		return;
	}
	/* <1> fill per-BSS statistics */
#if 0
	 /*AIS*/ if (prBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE) {
		prStaRec = prBssInfo->prStaRecOfAP;
		if (prStaRec) {
			for (eAci = 0; eAci < WMM_AC_INDEX_NUM; eAci++) {
				prBssInfo->arLinkStatistics[eAci].u4TxMsdu = prStaRec->arLinkStatistics[eAci].u4TxMsdu;
				prBssInfo->arLinkStatistics[eAci].u4RxMsdu = prStaRec->arLinkStatistics[eAci].u4RxMsdu;
				prBssInfo->arLinkStatistics[eAci].u4TxDropMsdu +=
				    prStaRec->arLinkStatistics[eAci].u4TxDropMsdu;
				prBssInfo->arLinkStatistics[eAci].u4TxFailMsdu =
				    prStaRec->arLinkStatistics[eAci].u4TxFailMsdu;
				prBssInfo->arLinkStatistics[eAci].u4TxRetryMsdu =
				    prStaRec->arLinkStatistics[eAci].u4TxRetryMsdu;
			}
		}
	}
#else
	for (eAci = 0; eAci < WMM_AC_INDEX_NUM; eAci++) {
		arLLStats[eAci].u4TxMsdu = prBssInfo->arLinkStatistics[eAci].u4TxMsdu;
		arLLStats[eAci].u4RxMsdu = prBssInfo->arLinkStatistics[eAci].u4RxMsdu;
		arLLStats[eAci].u4TxDropMsdu = prBssInfo->arLinkStatistics[eAci].u4TxDropMsdu;
		arLLStats[eAci].u4TxFailMsdu = prBssInfo->arLinkStatistics[eAci].u4TxFailMsdu;
		arLLStats[eAci].u4TxRetryMsdu = prBssInfo->arLinkStatistics[eAci].u4TxRetryMsdu;
	}

	for (ucIdx = 0; ucIdx < CFG_STA_REC_NUM; ucIdx++) {
		prStaRec = cnmGetStaRecByIndex(prAdapter, ucIdx);
		if (!prStaRec)
			continue;
		if (prStaRec->ucBssIndex != ucBssIdx)
			continue;
		/* now the valid sta_rec is valid */
		for (eAci = 0; eAci < WMM_AC_INDEX_NUM; eAci++) {
			arLLStats[eAci].u4TxMsdu += prStaRec->arLinkStatistics[eAci].u4TxMsdu;
			arLLStats[eAci].u4RxMsdu += prStaRec->arLinkStatistics[eAci].u4RxMsdu;
			arLLStats[eAci].u4TxDropMsdu += prStaRec->arLinkStatistics[eAci].u4TxDropMsdu;
			arLLStats[eAci].u4TxFailMsdu += prStaRec->arLinkStatistics[eAci].u4TxFailMsdu;
			arLLStats[eAci].u4TxRetryMsdu += prStaRec->arLinkStatistics[eAci].u4TxRetryMsdu;
		}
	}
#endif

	/* <2>Dump BSS statistics */
	for (eAci = 0; eAci < WMM_AC_INDEX_NUM; eAci++) {
		DBGLOG(SW4, INFO, "LLS BSS[%u] %s: T[%06u] R[%06u] T_D[%06u] T_F[%06u]\n",
		       prBssInfo->ucBssIndex, apucACI2Str[eAci], arLLStats[eAci].u4TxMsdu,
		       arLLStats[eAci].u4RxMsdu, arLLStats[eAci].u4TxDropMsdu, arLLStats[eAci].u4TxFailMsdu);
	}
}

VOID wlanDumpAllBssStatistics(IN P_ADAPTER_T prAdapter)
{
	P_BSS_INFO_T prBssInfo;
	/* ENUM_WMM_ACI_T eAci; */
	UINT_32 ucIdx;

	/* wlanUpdateAllBssStatistics(prAdapter); */

	for (ucIdx = 0; ucIdx < BSS_INFO_NUM; ucIdx++) {
		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucIdx);
		if (!IS_BSS_ACTIVE(prBssInfo)) {
			DBGLOG(SW4, TRACE, "Invalid BssInfo index[%u], skip dump!\n", ucIdx);
			continue;
		}

		wlanDumpBssStatistics(prAdapter, ucIdx);
	}
}

WLAN_STATUS
wlanoidQueryStaStatistics(IN P_ADAPTER_T prAdapter,
			  IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	return _wlanoidQueryStaStatistics(prAdapter,
					pvQueryBuffer,
					u4QueryBufferLen,
					pu4QueryInfoLen,
					g_fgIsOid);
}				/* wlanoidQueryStaStatistics */

WLAN_STATUS
_wlanoidQueryStaStatistics(IN P_ADAPTER_T prAdapter,
			  IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen,
			  OUT PUINT_32 pu4QueryInfoLen, IN BOOLEAN fgIsOid)
{
	WLAN_STATUS rResult = WLAN_STATUS_FAILURE;
	P_STA_RECORD_T prStaRec, prTempStaRec;
	P_PARAM_GET_STA_STATISTICS prQueryStaStatistics;
	UINT_8 ucStaRecIdx;
	P_QUE_MGT_T prQM;
	CMD_GET_STA_STATISTICS_T rQueryCmdStaStatistics;
	UINT_8 ucIdx;
	ENUM_WMM_ACI_T eAci;

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

		if (u4QueryBufferLen < sizeof(PARAM_GET_STA_STA_STATISTICS)) {
			*pu4QueryInfoLen = sizeof(PARAM_GET_STA_STA_STATISTICS);
			rResult = WLAN_STATUS_BUFFER_TOO_SHORT;
			break;
		}

		prQueryStaStatistics = (P_PARAM_GET_STA_STATISTICS) pvQueryBuffer;
		*pu4QueryInfoLen = sizeof(PARAM_GET_STA_STA_STATISTICS);

		/* 4 5. Get driver global QM counter */
#if QM_ADAPTIVE_TC_RESOURCE_CTRL
		for (ucIdx = TC0_INDEX; ucIdx <= TC3_INDEX; ucIdx++) {
			prQueryStaStatistics->au4TcAverageQueLen[ucIdx] = prQM->au4AverageQueLen[ucIdx];
			prQueryStaStatistics->au4TcCurrentQueLen[ucIdx] = prQM->au4CurrentTcResource[ucIdx];
		}
#endif

		/* 4 2. Get StaRec by MAC address */
		prStaRec = NULL;

		for (ucStaRecIdx = 0; ucStaRecIdx < CFG_STA_REC_NUM; ucStaRecIdx++) {
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
		prQueryStaStatistics->u4TxTotalCount = prStaRec->u4TotalTxPktsNumber;
		prQueryStaStatistics->u4RxTotalCount = prStaRec->u4TotalRxPktsNumber;
		prQueryStaStatistics->u4TxExceedThresholdCount = prStaRec->u4ThresholdCounter;
		prQueryStaStatistics->u4TxMaxTime = prStaRec->u4MaxTxPktsTime;
		if (prStaRec->u4TotalTxPktsNumber) {
			prQueryStaStatistics->u4TxAverageProcessTime =
			    (prStaRec->u4TotalTxPktsTime / prStaRec->u4TotalTxPktsNumber);
		} else
			prQueryStaStatistics->u4TxAverageProcessTime = 0;

		/*link layer statistics */
		for (eAci = 0; eAci < WMM_AC_INDEX_NUM; eAci++) {
			prQueryStaStatistics->arLinkStatistics[eAci].u4TxMsdu =
			    prStaRec->arLinkStatistics[eAci].u4TxMsdu;
			prQueryStaStatistics->arLinkStatistics[eAci].u4RxMsdu =
			    prStaRec->arLinkStatistics[eAci].u4RxMsdu;
			prQueryStaStatistics->arLinkStatistics[eAci].u4TxDropMsdu =
			    prStaRec->arLinkStatistics[eAci].u4TxDropMsdu;
		}

		for (ucIdx = TC0_INDEX; ucIdx <= TC3_INDEX; ucIdx++) {
			prQueryStaStatistics->au4TcResourceEmptyCount[ucIdx] =
			    prQM->au4QmTcResourceEmptyCounter[prStaRec->ucBssIndex][ucIdx];
			/* Reset */
			prQM->au4QmTcResourceEmptyCounter[prStaRec->ucBssIndex][ucIdx] = 0;
		}

		/* 4 4.1 Reset statistics */
		if (prQueryStaStatistics->ucReadClear) {
			prStaRec->u4ThresholdCounter = 0;
			prStaRec->u4TotalTxPktsNumber = 0;
			prStaRec->u4TotalTxPktsTime = 0;
			prStaRec->u4TotalRxPktsNumber = 0;
			prStaRec->u4MaxTxPktsTime = 0;
		}
		/*link layer statistics */
		if (prQueryStaStatistics->ucLlsReadClear) {
			for (eAci = 0; eAci < WMM_AC_INDEX_NUM; eAci++) {
				prStaRec->arLinkStatistics[eAci].u4TxMsdu = 0;
				prStaRec->arLinkStatistics[eAci].u4RxMsdu = 0;
				prStaRec->arLinkStatistics[eAci].u4TxDropMsdu = 0;
			}
		}
#endif

		for (ucIdx = TC0_INDEX; ucIdx <= TC3_INDEX; ucIdx++)
			prQueryStaStatistics->au4TcQueLen[ucIdx] = prStaRec->aprTargetQueue[ucIdx]->u4NumElem;

		rResult = WLAN_STATUS_SUCCESS;

		/* 4 6. Ensure FW supports get station link status */
		if (prAdapter->u4FwCompileFlag0 & COMPILE_FLAG0_GET_STA_LINK_STATUS) {

			DBGLOG(REQ, LOUD, "%s index[%x]\n", __func__, prStaRec->ucIndex);
			rQueryCmdStaStatistics.ucIndex = prStaRec->ucIndex;
			COPY_MAC_ADDR(rQueryCmdStaStatistics.aucMacAddr, prQueryStaStatistics->aucMacAddr);
			rQueryCmdStaStatistics.ucReadClear = prQueryStaStatistics->ucReadClear;
			rQueryCmdStaStatistics.ucLlsReadClear = prQueryStaStatistics->ucLlsReadClear;
			rQueryCmdStaStatistics.ucResetCounter = prQueryStaStatistics->ucResetCounter;

			rResult = wlanSendSetQueryCmd(prAdapter,
						      CMD_ID_GET_STA_STATISTICS,
						      FALSE,
						      TRUE,
						      fgIsOid,
						      nicCmdEventQueryStaStatistics,
						      nicOidCmdTimeoutCommon,
						      sizeof(CMD_GET_STA_STATISTICS_T),
						      (PUINT_8) &rQueryCmdStaStatistics,
						      pvQueryBuffer, u4QueryBufferLen);

			prQueryStaStatistics->u4Flag |= BIT(1);
		} else {
			rResult = WLAN_STATUS_NOT_SUPPORTED;
		}

	} while (FALSE);

	return rResult;
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
VOID wlanQueryNicResourceInformation(IN P_ADAPTER_T prAdapter)
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


WLAN_STATUS wlanQueryNicCapabilityV2(IN P_ADAPTER_T prAdapter)
{
	UINT_8 ucCmdSeqNum;
	P_CMD_INFO_T prCmdInfo;
	P_WIFI_CMD_T prWifiCmd;
	UINT_32 u4RxPktLength;
	P_UINT_8 prEventBuff;
	P_HW_MAC_RX_DESC_T prRxStatus;
	P_WIFI_EVENT_T prEvent;
	struct mt66xx_chip_info *prChipInfo;
	UINT_32 chip_id;

	prChipInfo = prAdapter->chip_info;
	chip_id = prChipInfo->chip_id;


	ASSERT(prAdapter);

	/* Get Nic resource information from FW */
	if (prAdapter->u4FwFeatureFlag0 & FEATURE_FLAG0_NIC_CAPABILITY_V2) {

		DBGLOG(INIT, INFO, "Support NIC_CAPABILITY_V2 feature\n");

		/*
		 * send NIC_CAPABILITY_V2 query cmd
		 */

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
		prCmdInfo->ucCID = CMD_ID_GET_NIC_CAPABILITY_V2;
		prCmdInfo->fgSetQuery = FALSE;
		prCmdInfo->fgNeedResp = TRUE;
		prCmdInfo->ucCmdSeqNum = ucCmdSeqNum;
		prCmdInfo->u4SetInfoLen = 0;

		/* Setup WIFI_CMD_T */
		prWifiCmd = (P_WIFI_CMD_T) (prCmdInfo->pucInfoBuffer);
		prWifiCmd->u2TxByteCount = prCmdInfo->u2InfoBufLen;
		prWifiCmd->u2PQ_ID = CMD_PQ_ID;
		prWifiCmd->ucPktTypeID = CMD_PACKET_TYPE_ID;
		prWifiCmd->ucCID = prCmdInfo->ucCID;
		prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
		prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;

		prWifiCmd->u2Length = prCmdInfo->u2InfoBufLen - (UINT_16) OFFSET_OF(WIFI_CMD_T, u2Length);

		wlanSendCommand(prAdapter, prCmdInfo);

		cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

		/*
		 * receive nic_capability_v2 event
		 */


		/* allocate event buffer */
		prEventBuff = cnmMemAlloc(prAdapter, RAM_TYPE_BUF, CFG_RX_MAX_PKT_SIZE);
		if (!prEventBuff) {
			DBGLOG(INIT, WARN, "%s: event buffer alloc failed!\n", __func__);
			return WLAN_STATUS_FAILURE;
		}

		/* get event */
		while (TRUE) {
			if (nicRxWaitResponse(prAdapter,
						  1,
						  prEventBuff,
						  CFG_RX_MAX_PKT_SIZE,
						  &u4RxPktLength) != WLAN_STATUS_SUCCESS) {
				DBGLOG(INIT, WARN, "%s: wait for event failed!\n", __func__);

				/* free event buffer */
				cnmMemFree(prAdapter, prEventBuff);

				return WLAN_STATUS_FAILURE;
			}

			/* header checking .. */
			prRxStatus = (P_HW_MAC_RX_DESC_T) prEventBuff;
			if ((prRxStatus->u2PktTYpe & RXM_RXD_PKT_TYPE_SW_BITMAP) != RXM_RXD_PKT_TYPE_SW_EVENT) {

				DBGLOG(INIT, WARN, "%s: skip unexpected Rx pkt type[0x%04x]\n", __func__,
					prRxStatus->u2PktTYpe);

				continue;
			}

			prEvent = (P_WIFI_EVENT_T) prEventBuff;
			if (prEvent->ucEID != EVENT_ID_NIC_CAPABILITY_V2) {
				DBGLOG(INIT, WARN, "%s: skip unexpected event ID[0x%02x]\n", __func__, prEvent->ucEID);

				continue;
			} else {
				/* hit */
				break;
			}
		}

		/*
		 * parsing elemens
		 */

		nicCmdEventQueryNicCapabilityV2(prAdapter, prEvent->aucBuffer);

#if CFG_SUPPORT_BFER
		if (prAdapter->ucRModeOnlyFlag)
			prAdapter->fgIsHwSupportBfer = FALSE;
		else
			prAdapter->fgIsHwSupportBfer = TRUE;
#endif

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

VOID wlanSetNicResourceParameters(IN P_ADAPTER_T prAdapter)
{
	UINT_8 string[128], idx;
	UINT_32 u4share;
	P_WIFI_VAR_T prWifiVar = &prAdapter->rWifiVar;
#if QM_ADAPTIVE_TC_RESOURCE_CTRL
	P_QUE_MGT_T prQM = &prAdapter->rQM;
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
			prAdapter->nicTxReousrce.u4McuTotalResource * NIC_TX_MAX_PAGE_PER_FRAME;	 /* MCU */

	u4share = prAdapter->nicTxReousrce.u4LmacTotalResource/(TC_NUM - 1); /* LMAC. Except TC_4, which is MCU */
	for (idx = TC0_INDEX; idx < TC_NUM; idx++) {
		if (idx != TC4_INDEX)
			prWifiVar->au4TcPageCount[idx] = u4share * NIC_TX_MAX_PAGE_PER_FRAME;
	}

	/* 1 2. if there is remaings, give them to TC_3, which is VO */
	prWifiVar->au4TcPageCount[TC3_INDEX] +=
			(prAdapter->nicTxReousrce.u4LmacTotalResource%(TC_NUM - 1)) * NIC_TX_MAX_PAGE_PER_FRAME;

#if QM_ADAPTIVE_TC_RESOURCE_CTRL
	/*
	 * 2. assign guaranteed page count for each TC
	 */

	/* 2 1. update guaranteed page count in QM */
	for (idx = 0; idx < TC_NUM; idx++)
		prQM->au4GuaranteedTcResource[idx] = prWifiVar->au4TcPageCount[idx];
#endif


#if CFG_SUPPORT_CFG_FILE
	/*
	 * 3. Use the settings in config file first,
	 *    else, use the settings reported from firmware.
	 */

	/* 3 1. update for free page count */
	for (idx = 0; idx < TC_NUM; idx++) {

		/* construct prefix: Tc0Page, Tc1Page... */
		memset(string, 0, sizeof(string)/sizeof(UINT_8));
		snprintf(string, sizeof(string)/sizeof(UINT_8), "Tc%xPage", idx);

		/* update the final value */
		prWifiVar->au4TcPageCount[idx] =
								(UINT_32) wlanCfgGetUint32(prAdapter,
								string, prWifiVar->au4TcPageCount[idx]);
	}

#if QM_ADAPTIVE_TC_RESOURCE_CTRL
	/* 3 2. update for guaranteed page count */
	for (idx = 0; idx < TC_NUM; idx++) {

		/* construct prefix: Tc0Grt, Tc1Grt... */
		memset(string, 0, sizeof(string)/sizeof(UINT_8));
		snprintf(string, sizeof(string)/sizeof(UINT_8), "Tc%xGrt", idx);

		/* update the final value */
		prQM->au4GuaranteedTcResource[idx] =
									(UINT_32) wlanCfgGetUint32(prAdapter,
									string, prQM->au4GuaranteedTcResource[idx]);
	}
#endif /* end of #if QM_ADAPTIVE_TC_RESOURCE_CTRL */

#endif /* end of #if CFG_SUPPORT_CFG_FILE */
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
VOID wlanUpdateNicResourceInformation(IN P_ADAPTER_T prAdapter)
{
	/*
	 * 3 1. Query TX resource
	 */

	/* information is not got from firmware, use default value */
	if (prAdapter->fgIsNicTxReousrceValid != TRUE)
		return;

	/* 3 2. Setup resource parameters */
	wlanSetNicResourceParameters(prAdapter);

	/* 3 3. Reset Tx resource */
	nicTxResetResource(prAdapter);

#if QM_ADAPTIVE_TC_RESOURCE_CTRL
	/* 3 4. Reset QM resource */
	qmResetTcControlResource(prAdapter);
#endif

	halTxResourceResetHwTQCounter(prAdapter);
}


#if 0
/*----------------------------------------------------------------------------*/
/*!
* @brief This function is to SET network interface index for a network interface.
*           A network interface is a TX/RX data port hooked to OS.
*
* @param prGlueInfo                     Pointer of prGlueInfo Data Structure
* @param ucNetInterfaceIndex            Index of network interface
* @param ucBssIndex                     Index of BSS
*
* @return VOID
*/
/*----------------------------------------------------------------------------*/
VOID wlanBindNetInterface(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucNetInterfaceIndex, IN PVOID pvNetInterface)
{
	P_NET_INTERFACE_INFO_T prNetIfInfo;

	prNetIfInfo = &prGlueInfo->arNetInterfaceInfo[ucNetInterfaceIndex];

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
VOID wlanBindBssIdxToNetInterface(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucBssIndex, IN PVOID pvNetInterface)
{
	P_NET_INTERFACE_INFO_T prNetIfInfo;

	if (ucBssIndex >= ARRAY_SIZE(prGlueInfo->arNetInterfaceInfo)) {
		DBGLOG(INIT, ERROR, "Array index out of bound, ucBssIndex=%u\n", ucBssIndex);
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
UINT_8 wlanGetBssIdxByNetInterface(IN P_GLUE_INFO_T prGlueInfo, IN PVOID pvNetInterface)
{
	UINT_8 ucIdx = 0;

	for (ucIdx = 0; ucIdx < HW_BSSID_NUM; ucIdx++) {
		if (prGlueInfo->arNetInterfaceInfo[ucIdx].pvNetInterface == pvNetInterface)
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
PVOID wlanGetNetInterfaceByBssIdx(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucBssIndex)
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
UINT_8 wlanGetAisBssIndex(IN P_ADAPTER_T prAdapter)
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
VOID wlanInitFeatureOption(IN P_ADAPTER_T prAdapter)
{
	P_WIFI_VAR_T prWifiVar = &prAdapter->rWifiVar;
#if QM_ADAPTIVE_TC_RESOURCE_CTRL
	P_QUE_MGT_T prQM = &prAdapter->rQM;
#endif

	/* Feature options will be filled by config file */

	prWifiVar->ucQoS = (UINT_8) wlanCfgGetUint32(prAdapter, "Qos", FEATURE_ENABLED);

	prWifiVar->ucStaHt = (UINT_8) wlanCfgGetUint32(prAdapter, "StaHT", FEATURE_ENABLED);
	prWifiVar->ucStaVht = (UINT_8) wlanCfgGetUint32(prAdapter, "StaVHT", FEATURE_ENABLED);

	prWifiVar->ucApHt = (UINT_8) wlanCfgGetUint32(prAdapter, "ApHT", FEATURE_ENABLED);
	prWifiVar->ucApVht = (UINT_8) wlanCfgGetUint32(prAdapter, "ApVHT", FEATURE_ENABLED);

	prWifiVar->ucP2pGoHt = (UINT_8) wlanCfgGetUint32(prAdapter, "P2pGoHT", FEATURE_ENABLED);
	prWifiVar->ucP2pGoVht = (UINT_8) wlanCfgGetUint32(prAdapter, "P2pGoVHT", FEATURE_ENABLED);

	prWifiVar->ucP2pGcHt = (UINT_8) wlanCfgGetUint32(prAdapter, "P2pGcHT", FEATURE_ENABLED);
	prWifiVar->ucP2pGcVht = (UINT_8) wlanCfgGetUint32(prAdapter, "P2pGcVHT", FEATURE_ENABLED);

	prWifiVar->ucAmpduRx = (UINT_8) wlanCfgGetUint32(prAdapter, "AmpduRx", FEATURE_ENABLED);
	prWifiVar->ucAmpduTx = (UINT_8) wlanCfgGetUint32(prAdapter, "AmpduTx", FEATURE_ENABLED);

	prWifiVar->ucAmsduInAmpduRx = (UINT_8) wlanCfgGetUint32(prAdapter, "AmsduInAmpduRx", FEATURE_ENABLED);
	prWifiVar->ucAmsduInAmpduTx = (UINT_8) wlanCfgGetUint32(prAdapter, "AmsduInAmpduTx", FEATURE_ENABLED);
	prWifiVar->ucHtAmsduInAmpduRx = (UINT_8) wlanCfgGetUint32(prAdapter, "HtAmsduInAmpduRx", FEATURE_DISABLED);
	prWifiVar->ucHtAmsduInAmpduTx = (UINT_8) wlanCfgGetUint32(prAdapter, "HtAmsduInAmpduTx", FEATURE_DISABLED);
	prWifiVar->ucVhtAmsduInAmpduRx = (UINT_8) wlanCfgGetUint32(prAdapter, "VhtAmsduInAmpduRx", FEATURE_ENABLED);
	prWifiVar->ucVhtAmsduInAmpduTx = (UINT_8) wlanCfgGetUint32(prAdapter, "VhtAmsduInAmpduTx", FEATURE_ENABLED);

	prWifiVar->ucTspec = (UINT_8) wlanCfgGetUint32(prAdapter, "Tspec", FEATURE_DISABLED);

	prWifiVar->ucUapsd = (UINT_8) wlanCfgGetUint32(prAdapter, "Uapsd", FEATURE_ENABLED);
	prWifiVar->ucStaUapsd = (UINT_8) wlanCfgGetUint32(prAdapter, "StaUapsd", FEATURE_DISABLED);
	prWifiVar->ucApUapsd = (UINT_8) wlanCfgGetUint32(prAdapter, "ApUapsd", FEATURE_DISABLED);
	prWifiVar->ucP2pUapsd = (UINT_8) wlanCfgGetUint32(prAdapter, "P2pUapsd", FEATURE_ENABLED);
	prWifiVar->u4RegP2pIfAtProbe = (UINT_8) wlanCfgGetUint32(prAdapter, "RegP2pIfAtProbe", FEATURE_DISABLED);
	prWifiVar->ucP2pShareMacAddr = (UINT_8) wlanCfgGetUint32(prAdapter, "P2pShareMacAddr", FEATURE_DISABLED);

	prWifiVar->ucTxShortGI = (UINT_8) wlanCfgGetUint32(prAdapter, "SgiTx", FEATURE_ENABLED);
	prWifiVar->ucRxShortGI = (UINT_8) wlanCfgGetUint32(prAdapter, "SgiRx", FEATURE_ENABLED);

	prWifiVar->ucTxLdpc = (UINT_8) wlanCfgGetUint32(prAdapter, "LdpcTx", FEATURE_ENABLED);
	prWifiVar->ucRxLdpc = (UINT_8) wlanCfgGetUint32(prAdapter, "LdpcRx", FEATURE_ENABLED);

	prWifiVar->ucTxStbc = (UINT_8) wlanCfgGetUint32(prAdapter, "StbcTx", FEATURE_ENABLED);
	prWifiVar->ucRxStbc = (UINT_8) wlanCfgGetUint32(prAdapter, "StbcRx", FEATURE_ENABLED);
	prWifiVar->ucRxStbcNss = (UINT_8) wlanCfgGetUint32(prAdapter, "StbcRxNss", 1);

	prWifiVar->ucTxGf = (UINT_8) wlanCfgGetUint32(prAdapter, "GfTx", FEATURE_ENABLED);
	prWifiVar->ucRxGf = (UINT_8) wlanCfgGetUint32(prAdapter, "GfRx", FEATURE_ENABLED);

	prWifiVar->ucMCS32 = (UINT_8) wlanCfgGetUint32(prAdapter, "MCS32", FEATURE_DISABLED);

	prWifiVar->ucSigTaRts = (UINT_8) wlanCfgGetUint32(prAdapter, "SigTaRts", FEATURE_DISABLED);
	prWifiVar->ucDynBwRts = (UINT_8) wlanCfgGetUint32(prAdapter, "DynBwRts", FEATURE_DISABLED);
	prWifiVar->ucTxopPsTx = (UINT_8) wlanCfgGetUint32(prAdapter, "TxopPsTx", FEATURE_DISABLED);

	prWifiVar->ucStaHtBfee = (UINT_8) wlanCfgGetUint32(prAdapter,
					"StaHTBfee", FEATURE_DISABLED);
	prWifiVar->ucStaVhtBfee = (UINT_8) wlanCfgGetUint32(prAdapter, "StaVHTBfee", FEATURE_ENABLED);
	prWifiVar->ucStaVhtMuBfee = (UINT_8)wlanCfgGetUint32(prAdapter, "StaVHTMuBfee", FEATURE_ENABLED);
	prWifiVar->ucStaHtBfer = (UINT_8) wlanCfgGetUint32(prAdapter, "StaHTBfer", FEATURE_DISABLED);
	prWifiVar->ucStaVhtBfer = (UINT_8) wlanCfgGetUint32(prAdapter, "StaVHTBfer", FEATURE_DISABLED);

	/* 0: disabled
	 * 1: Tx done event to driver
	 * 2: Tx status to FW only
	 */
	prWifiVar->ucDataTxDone = (UINT_8) wlanCfgGetUint32(prAdapter, "DataTxDone", 0);
	prWifiVar->ucDataTxRateMode = (UINT_8) wlanCfgGetUint32(prAdapter, "DataTxRateMode", DATA_RATE_MODE_AUTO);
	prWifiVar->u4DataTxRateCode = wlanCfgGetUint32(prAdapter, "DataTxRateCode", 0x0);

	prWifiVar->ucApWpsMode = (UINT_8) wlanCfgGetUint32(prAdapter, "ApWpsMode", 0);
	DBGLOG(INIT, INFO, "ucApWpsMode = %u\n", prWifiVar->ucApWpsMode);

	prWifiVar->ucThreadScheduling = (UINT_8) wlanCfgGetUint32(prAdapter, "ThreadSched", 0);
	prWifiVar->ucThreadPriority = (UINT_8) wlanCfgGetUint32(prAdapter, "ThreadPriority", WLAN_THREAD_TASK_PRIORITY);
	prWifiVar->cThreadNice = (INT_8) wlanCfgGetInt32(prAdapter, "ThreadNice", WLAN_THREAD_TASK_NICE);

	prAdapter->rQM.u4MaxForwardBufferCount =
	    (UINT_32) wlanCfgGetUint32(prAdapter, "ApForwardBufferCnt", QM_FWD_PKT_QUE_THRESHOLD);

	/* AP channel setting
	 * 0: auto
	 */
	prWifiVar->ucApChannel = (UINT_8) wlanCfgGetUint32(prAdapter, "ApChannel", 0);

	/*
	 * 0: SCN
	 * 1: SCA
	 * 2: RES
	 * 3: SCB
	 */
	prWifiVar->ucApSco = (UINT_8) wlanCfgGetUint32(prAdapter, "ApSco", 0);
	prWifiVar->ucP2pGoSco = (UINT_8) wlanCfgGetUint32(prAdapter, "P2pGoSco", 0);

	/* Max bandwidth setting
	 * 0: 20Mhz
	 * 1: 40Mhz
	 * 2: 80Mhz
	 * 3: 160Mhz
	 * 4: 80+80Mhz
	 * Note: For VHT STA, BW 80Mhz is a must!
	 */
	prWifiVar->ucStaBandwidth = (UINT_8) wlanCfgGetUint32(prAdapter, "StaBw", MAX_BW_160MHZ);
	prWifiVar->ucSta2gBandwidth = (UINT_8) wlanCfgGetUint32(prAdapter, "Sta2gBw", MAX_BW_20MHZ);
	prWifiVar->ucSta5gBandwidth = (UINT_8) wlanCfgGetUint32(prAdapter, "Sta5gBw", MAX_BW_80MHZ);
	prWifiVar->ucP2p2gBandwidth = (UINT_8) wlanCfgGetUint32(prAdapter, "P2p2gBw", MAX_BW_40MHZ);
	prWifiVar->ucP2p5gBandwidth = (UINT_8) wlanCfgGetUint32(prAdapter, "P2p5gBw", MAX_BW_80MHZ);
	prWifiVar->ucApBandwidth = (UINT_8) wlanCfgGetUint32(prAdapter, "ApBw", MAX_BW_160MHZ);
	prWifiVar->ucAp2gBandwidth = (UINT_8) wlanCfgGetUint32(prAdapter, "Ap2gBw", MAX_BW_40MHZ);
	prWifiVar->ucAp5gBandwidth = (UINT_8) wlanCfgGetUint32(prAdapter, "Ap5gBw", MAX_BW_80MHZ);
	prWifiVar->ucApChnlDefFromCfg = (UINT_8) wlanCfgGetUint32(prAdapter, "ApChnlDefFromCfg", FEATURE_ENABLED);
	prWifiVar->ucApAllowHtVhtTkip =
		(UINT_8) wlanCfgGetUint32(prAdapter,
			"ApAllowHtVhtTkip", FEATURE_DISABLED);

	prWifiVar->ucNSS = (UINT_8) wlanCfgGetUint32(prAdapter, "Nss", 2);

	/* Max Rx MPDU length setting
	 * 0: 3k
	 * 1: 8k
	 * 2: 11k
	 */
	prWifiVar->ucRxMaxMpduLen = (UINT_8) wlanCfgGetUint32(prAdapter, "RxMaxMpduLen", VHT_CAP_INFO_MAX_MPDU_LEN_3K);
	/* Max Tx AMSDU in AMPDU length *in BYTES* */
	prWifiVar->u4TxMaxAmsduInAmpduLen = wlanCfgGetUint32(prAdapter, "TxMaxAmsduInAmpduLen", 4096);

	prWifiVar->ucStaDisconnectDetectTh = (UINT_8) wlanCfgGetUint32(prAdapter, "StaDisconnectDetectTh", 0);
	prWifiVar->ucApDisconnectDetectTh = (UINT_8) wlanCfgGetUint32(prAdapter, "ApDisconnectDetectTh", 0);
	prWifiVar->ucP2pDisconnectDetectTh = (UINT_8) wlanCfgGetUint32(prAdapter, "P2pDisconnectDetectTh", 0);

	prWifiVar->ucTcRestrict = (UINT_8) wlanCfgGetUint32(prAdapter, "TcRestrict", 0xFF);
	/* Max Tx dequeue limit: 0 => auto */
	prWifiVar->u4MaxTxDeQLimit = (UINT_32) wlanCfgGetUint32(prAdapter, "MaxTxDeQLimit", 0x0);
	prWifiVar->ucAlwaysResetUsedRes = (UINT_32) wlanCfgGetUint32(prAdapter, "AlwaysResetUsedRes", 0x0);

#if CFG_SUPPORT_MTK_SYNERGY
	prWifiVar->ucMtkOui = (UINT_8) wlanCfgGetUint32(prAdapter, "MtkOui", FEATURE_ENABLED);
	prWifiVar->u4MtkOuiCap = (UINT_32) wlanCfgGetUint32(prAdapter, "MtkOuiCap", 0);
	prWifiVar->aucMtkFeature[0] = 0xff;
	prWifiVar->aucMtkFeature[1] = 0xff;
	prWifiVar->aucMtkFeature[2] = 0xff;
	prWifiVar->aucMtkFeature[3] = 0xff;
#endif

	prWifiVar->ucCmdRsvResource = (UINT_8) wlanCfgGetUint32(prAdapter, "TxCmdRsv", QM_CMD_RESERVED_THRESHOLD);
	prWifiVar->u4MgmtQueueDelayTimeout =
			(UINT_32) wlanCfgGetUint32(prAdapter, "TxMgmtQueTO", QM_MGMT_QUEUED_TIMEOUT);	/* ms */

	/* Performance related */
	prWifiVar->u4HifIstLoopCount = (UINT_32) wlanCfgGetUint32(prAdapter, "IstLoop", CFG_IST_LOOP_COUNT);
	prWifiVar->u4Rx2OsLoopCount = (UINT_32) wlanCfgGetUint32(prAdapter, "Rx2OsLoop", 4);
	prWifiVar->u4HifTxloopCount = (UINT_32) wlanCfgGetUint32(prAdapter, "HifTxLoop", 1);
	prWifiVar->u4TxFromOsLoopCount = (UINT_32) wlanCfgGetUint32(prAdapter, "OsTxLoop", 1);
	prWifiVar->u4TxRxLoopCount = (UINT_32) wlanCfgGetUint32(prAdapter, "Rx2ReorderLoop", 1);
	prWifiVar->u4TxIntThCount = (UINT_32) wlanCfgGetUint32(prAdapter, "IstTxTh", HIF_IST_TX_THRESHOLD);

	prWifiVar->u4NetifStopTh =
	    (UINT_32) wlanCfgGetUint32(prAdapter, "NetifStopTh", CFG_TX_STOP_NETIF_PER_QUEUE_THRESHOLD);
	prWifiVar->u4NetifStartTh =
	    (UINT_32) wlanCfgGetUint32(prAdapter, "NetifStartTh", CFG_TX_START_NETIF_PER_QUEUE_THRESHOLD);
	prWifiVar->ucTxBaSize = (UINT_8) wlanCfgGetUint32(prAdapter, "TxBaSize", 64);
	prWifiVar->ucRxHtBaSize = (UINT_8) wlanCfgGetUint32(prAdapter, "RxHtBaSize", 64);
	prWifiVar->ucRxVhtBaSize = (UINT_8) wlanCfgGetUint32(prAdapter, "RxVhtBaSize", 64);

	/* Tx Buffer Management */
	prWifiVar->ucExtraTxDone = (UINT_32) wlanCfgGetUint32(prAdapter, "ExtraTxDone", 1);
	prWifiVar->ucTxDbg = (UINT_32) wlanCfgGetUint32(prAdapter, "TxDbg", 0);

	kalMemZero(prWifiVar->au4TcPageCount, sizeof(prWifiVar->au4TcPageCount));

	prWifiVar->au4TcPageCount[TC0_INDEX] = (UINT_32) wlanCfgGetUint32(prAdapter, "Tc0Page", NIC_TX_PAGE_COUNT_TC0);
	prWifiVar->au4TcPageCount[TC1_INDEX] = (UINT_32) wlanCfgGetUint32(prAdapter, "Tc1Page", NIC_TX_PAGE_COUNT_TC1);
	prWifiVar->au4TcPageCount[TC2_INDEX] = (UINT_32) wlanCfgGetUint32(prAdapter, "Tc2Page", NIC_TX_PAGE_COUNT_TC2);
	prWifiVar->au4TcPageCount[TC3_INDEX] = (UINT_32) wlanCfgGetUint32(prAdapter, "Tc3Page", NIC_TX_PAGE_COUNT_TC3);
	prWifiVar->au4TcPageCount[TC4_INDEX] = (UINT_32) wlanCfgGetUint32(prAdapter, "Tc4Page", NIC_TX_PAGE_COUNT_TC4);
	prWifiVar->au4TcPageCount[TC5_INDEX] = (UINT_32) wlanCfgGetUint32(prAdapter, "Tc5Page", NIC_TX_PAGE_COUNT_TC5);

#if QM_ADAPTIVE_TC_RESOURCE_CTRL
	prQM->au4MinReservedTcResource[TC0_INDEX] =
	    (UINT_32) wlanCfgGetUint32(prAdapter, "Tc0MinRsv", QM_MIN_RESERVED_TC0_RESOURCE);
	prQM->au4MinReservedTcResource[TC1_INDEX] =
	    (UINT_32) wlanCfgGetUint32(prAdapter, "Tc1MinRsv", QM_MIN_RESERVED_TC1_RESOURCE);
	prQM->au4MinReservedTcResource[TC2_INDEX] =
	    (UINT_32) wlanCfgGetUint32(prAdapter, "Tc2MinRsv", QM_MIN_RESERVED_TC2_RESOURCE);
	prQM->au4MinReservedTcResource[TC3_INDEX] =
	    (UINT_32) wlanCfgGetUint32(prAdapter, "Tc3MinRsv", QM_MIN_RESERVED_TC3_RESOURCE);
	prQM->au4MinReservedTcResource[TC4_INDEX] =
	    (UINT_32) wlanCfgGetUint32(prAdapter, "Tc4MinRsv", QM_MIN_RESERVED_TC4_RESOURCE);
	prQM->au4MinReservedTcResource[TC5_INDEX] =
	    (UINT_32) wlanCfgGetUint32(prAdapter, "Tc5MinRsv", QM_MIN_RESERVED_TC5_RESOURCE);

	prQM->au4GuaranteedTcResource[TC0_INDEX] =
	    (UINT_32) wlanCfgGetUint32(prAdapter, "Tc0Grt", QM_GUARANTEED_TC0_RESOURCE);
	prQM->au4GuaranteedTcResource[TC1_INDEX] =
	    (UINT_32) wlanCfgGetUint32(prAdapter, "Tc1Grt", QM_GUARANTEED_TC1_RESOURCE);
	prQM->au4GuaranteedTcResource[TC2_INDEX] =
	    (UINT_32) wlanCfgGetUint32(prAdapter, "Tc2Grt", QM_GUARANTEED_TC2_RESOURCE);
	prQM->au4GuaranteedTcResource[TC3_INDEX] =
	    (UINT_32) wlanCfgGetUint32(prAdapter, "Tc3Grt", QM_GUARANTEED_TC3_RESOURCE);
	prQM->au4GuaranteedTcResource[TC4_INDEX] =
	    (UINT_32) wlanCfgGetUint32(prAdapter, "Tc4Grt", QM_GUARANTEED_TC4_RESOURCE);
	prQM->au4GuaranteedTcResource[TC5_INDEX] =
	    (UINT_32) wlanCfgGetUint32(prAdapter, "Tc5Grt", QM_GUARANTEED_TC5_RESOURCE);

	prQM->u4TimeToAdjustTcResource =
	    (UINT_32) wlanCfgGetUint32(prAdapter, "TcAdjustTime", QM_INIT_TIME_TO_ADJUST_TC_RSC);
	prQM->u4TimeToUpdateQueLen =
	    (UINT_32) wlanCfgGetUint32(prAdapter, "QueLenUpdateTime", QM_INIT_TIME_TO_UPDATE_QUE_LEN);
	prQM->u4QueLenMovingAverage =
	    (UINT_32) wlanCfgGetUint32(prAdapter, "QueLenMovingAvg", QM_QUE_LEN_MOVING_AVE_FACTOR);
	prQM->u4ExtraReservedTcResource =
	    (UINT_32) wlanCfgGetUint32(prAdapter, "TcExtraRsv", QM_EXTRA_RESERVED_RESOURCE_WHEN_BUSY);
#endif

	/* Stats log */
	prWifiVar->u4StatsLogTimeout = (UINT_32) wlanCfgGetUint32(prAdapter, "StatsLogTO", WLAN_TX_STATS_LOG_TIMEOUT);
	prWifiVar->u4StatsLogDuration =
	    (UINT_32) wlanCfgGetUint32(prAdapter, "StatsLogDur", WLAN_TX_STATS_LOG_DURATION);

	prWifiVar->ucDhcpTxDone = (UINT_8) wlanCfgGetUint32(prAdapter, "DhcpTxDone", 1);
	prWifiVar->ucArpTxDone = (UINT_8) wlanCfgGetUint32(prAdapter, "ArpTxDone", 1);

	prWifiVar->ucMacAddrOverride = (UINT_8) wlanCfgGetInt32(prAdapter, "MacOverride", 0);
	if (wlanCfgGet(prAdapter, "MacAddr", prWifiVar->aucMacAddrStr, "00:0c:e7:66:32:e1", 0))
		DBGLOG(INIT, INFO, "get MacAddr fail, use defaul\n");

	prWifiVar->ucCtiaMode = (UINT_8) wlanCfgGetUint32(prAdapter, "CtiaMode", 0);

	/* Combine ucTpTestMode and ucSigmaTestMode in one flag */
	/* ucTpTestMode == 0, for normal driver */
	/* ucTpTestMode == 1, for pure throughput test mode (ex: RvR) */
	/* ucTpTestMode == 2, for sigma TGn/TGac/PMF */
	/* ucTpTestMode == 3, for sigma WMM PS */
	prWifiVar->ucTpTestMode = (UINT_8) wlanCfgGetUint32(prAdapter, "TpTestMode", 0);

#if 0
	prWifiVar->ucSigmaTestMode = (UINT_8) wlanCfgGetUint32(prAdapter, "SigmaTestMode", 0);
#endif

#if CFG_SUPPORT_DBDC
	prWifiVar->ucDbdcMode = (UINT_8) wlanCfgGetUint32(prAdapter, "DbdcMode", DBDC_MODE_DYNAMIC);
#endif /*CFG_SUPPORT_DBDC*/
#if (CFG_EFUSE_BUFFER_MODE_DELAY_CAL == 1)
	prWifiVar->ucEfuseBufferModeCal = (UINT_8) wlanCfgGetUint32(prAdapter, "EfuseBufferModeCal", 0);
#endif
	prWifiVar->ucCalTimingCtrl = (UINT_8) wlanCfgGetUint32(prAdapter, "CalTimingCtrl", 0 /* power on full cal */);
	prWifiVar->ucWow = (UINT_8) wlanCfgGetUint32(prAdapter, "Wow", FEATURE_DISABLED);
	prWifiVar->ucOffload = (UINT_8) wlanCfgGetUint32(prAdapter, "Offload", FEATURE_DISABLED);
	prWifiVar->ucAdvPws = (UINT_8) wlanCfgGetUint32(prAdapter,
				"AdvPws", FEATURE_DISABLED);
	prWifiVar->ucWowOnMdtim = (UINT_8) wlanCfgGetUint32(prAdapter, "WowOnMdtim", 1);
	prWifiVar->ucWowOffMdtim = (UINT_8) wlanCfgGetUint32(prAdapter, "WowOffMdtim", 3);
	prWifiVar->ucWowPwsMode = (UINT_8) wlanCfgGetUint32(prAdapter, "WowPwsMode", Param_PowerModeFast_PSP);
	prWifiVar->ucListenDtimInterval =
		(UINT_8) wlanCfgGetUint32(prAdapter, "ListenDtimInt", DEFAULT_LISTEN_INTERVAL_BY_DTIM_PERIOD);
	/* prWifiVar->ucEapolOffload = (UINT_8) wlanCfgGetUint32(prAdapter, "EapolOffload", FEATURE_ENABLED); */

	/* ucEapolOffload: only offload eapol rekey as suspen/resume case. */
	prWifiVar->ucEapolOffload = FEATURE_DISABLED;

#if CFG_SUPPORT_REPLAY_DETECTION
	prWifiVar->ucRpyDetectOffload = (UINT_8) wlanCfgGetUint32(prAdapter, "rpydetectoffload", FEATURE_ENABLED);
#endif

#if CFG_WOW_SUPPORT
	prAdapter->rWowCtrl.fgWowEnable = (UINT_8) wlanCfgGetUint32(prAdapter, "WowEnable", FEATURE_ENABLED);
	prAdapter->rWowCtrl.ucScenarioId = (UINT_8) wlanCfgGetUint32(prAdapter, "WowScenarioId", 0);
	prAdapter->rWowCtrl.ucBlockCount = (UINT_8) wlanCfgGetUint32(prAdapter, "WowPinCnt", 1);
	prAdapter->rWowCtrl.astWakeHif[0].ucWakeupHif =
		(UINT_8) wlanCfgGetUint32(prAdapter, "WowHif", ENUM_HIF_TYPE_GPIO);
	prAdapter->rWowCtrl.astWakeHif[0].ucGpioPin = (UINT_8) wlanCfgGetUint32(prAdapter, "WowGpioPin", 0xFF);
	prAdapter->rWowCtrl.astWakeHif[0].ucTriggerLvl = (UINT_8) wlanCfgGetUint32(prAdapter, "WowTriggerLevel", 3);
	prAdapter->rWowCtrl.astWakeHif[0].u4GpioInterval = wlanCfgGetUint32(prAdapter, "GpioInterval", 0);
#endif

	/* SW Test Mode: Mainly used for Sigma */
	prWifiVar->u4SwTestMode = (UINT_8) wlanCfgGetUint32(prAdapter, "Sigma", ENUM_SW_TEST_MODE_NONE);
	prWifiVar->ucCtrlFlagAssertPath =
	    (UINT_8) wlanCfgGetUint32(prAdapter, "AssertPath", DBG_ASSERT_PATH_DEFAULT);
	prWifiVar->ucCtrlFlagDebugLevel =
	    (UINT_8) wlanCfgGetUint32(prAdapter, "AssertLevel", DBG_ASSERT_CTRL_LEVEL_DEFAULT);
	prWifiVar->u4ScanCtrl =
	    (UINT_8) wlanCfgGetUint32(prAdapter, "ScanCtrl", SCN_CTRL_DEFAULT_SCAN_CTRL);
	prWifiVar->ucScanChannelListenTime =
	    (UINT_8) wlanCfgGetUint32(prAdapter, "ScnChListenTime", 0);

	/* Wake lock related configuration */
	prWifiVar->u4WakeLockRxTimeout =
		wlanCfgGetUint32(prAdapter, "WakeLockRxTO", WAKE_LOCK_RX_TIMEOUT);
	prWifiVar->u4WakeLockThreadWakeup =
		wlanCfgGetUint32(prAdapter, "WakeLockThreadTO", WAKE_LOCK_THREAD_WAKEUP_TIMEOUT);

	prWifiVar->ucSmartRTS = (UINT_8) wlanCfgGetUint32(prAdapter, "SmartRTS", 0);
#if 1
	/* add more cfg from RegInfo */
	prWifiVar->u4UapsdAcBmp = (UINT_32) wlanCfgGetUint32(prAdapter, "UapsdAcBmp", 0);
	prWifiVar->u4MaxSpLen = (UINT_32) wlanCfgGetUint32(prAdapter, "MaxSpLen", 0);
	prWifiVar->fgDisOnlineScan = (UINT_32) wlanCfgGetUint32(prAdapter, "DisOnlineScan", 0);
	prWifiVar->fgDisBcnLostDetection = (UINT_32) wlanCfgGetUint32(prAdapter, "DisBcnLostDetection", 0);
	prWifiVar->fgDisRoaming = (UINT_32) wlanCfgGetUint32(prAdapter, "DisRoaming", 0);
	prWifiVar->fgEnArpFilter = (UINT_32) wlanCfgGetUint32(prAdapter, "EnArpFilter", FEATURE_ENABLED);
#endif

	/* Driver Flow Control Dequeue Quota. Now is only used by DBDC */
	prWifiVar->uDeQuePercentEnable =
		(UINT_8) wlanCfgGetUint32(prAdapter, "DeQuePercentEnable", 1);
	prWifiVar->u4DeQuePercentVHT80Nss1 =
		(UINT_32) wlanCfgGetUint32(prAdapter, "DeQuePercentVHT80NSS1", QM_DEQUE_PERCENT_VHT80_NSS1);
	prWifiVar->u4DeQuePercentVHT40Nss1 =
		(UINT_32) wlanCfgGetUint32(prAdapter, "DeQuePercentVHT40NSS1", QM_DEQUE_PERCENT_VHT40_NSS1);
	prWifiVar->u4DeQuePercentVHT20Nss1 =
		(UINT_32) wlanCfgGetUint32(prAdapter, "DeQuePercentVHT20NSS1", QM_DEQUE_PERCENT_VHT20_NSS1);
	prWifiVar->u4DeQuePercentHT40Nss1 =
		(UINT_32) wlanCfgGetUint32(prAdapter, "DeQuePercentHT40NSS1", QM_DEQUE_PERCENT_HT40_NSS1);
	prWifiVar->u4DeQuePercentHT20Nss1 =
		(UINT_32) wlanCfgGetUint32(prAdapter, "DeQuePercentHT20NSS1", QM_DEQUE_PERCENT_HT20_NSS1);

	/* Support TDLS 5.5.4.2 optional case */
	prWifiVar->fgTdlsBufferSTASleep = (BOOLEAN) wlanCfgGetUint32(prAdapter, "TdlsBufferSTASleep", FEATURE_DISABLED);
	/* Support USB Whole chip reset recover */
	prWifiVar->fgChipResetRecover = (BOOLEAN) wlanCfgGetUint32(prAdapter, "ChipResetRecover", FEATURE_ENABLED);

#if CFG_SUPPORT_ANT_SELECT
	prWifiVar->ucSpeIdxCtrl = (UINT_8) wlanCfgGetUint32(prAdapter, "SpeIdxCtrl", 2);
#endif

#ifdef CFG_SUPPORT_ADJUST_JOIN_CH_REQ_INTERVAL
	prWifiVar->u4AisJoinChReqIntervel =
		(UINT_32) wlanCfgGetUint32(prAdapter, "AisJoinChReqIntervel",
		AIS_JOIN_CH_REQUEST_INTERVAL);
	if (AIS_JOIN_CH_REQUEST_MAX_INTERVAL <
	    prWifiVar->u4AisJoinChReqIntervel)
		prWifiVar->u4AisJoinChReqIntervel =
			AIS_JOIN_CH_REQUEST_MAX_INTERVAL;
#endif
}

VOID wlanCfgSetSwCtrl(IN P_ADAPTER_T prAdapter)
{
	UINT_32 i = 0;
	CHAR aucKey[WLAN_CFG_VALUE_LEN_MAX];
	CHAR aucValue[WLAN_CFG_VALUE_LEN_MAX];

	const CHAR acDelim[] = " ";
	CHAR *pcPtr = NULL;
	CHAR *pcDupValue = NULL;
	UINT_32 au4Values[2];
	UINT_32 u4TokenCount = 0;
	UINT_32 u4BufLen = 0;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	P_GLUE_INFO_T prGlueInfo = prAdapter->prGlueInfo;
	PARAM_CUSTOM_SW_CTRL_STRUCT_T rSwCtrlInfo;
	INT_32 u4Ret = 0;

	for (i = 0; i < WLAN_CFG_SET_SW_CTRL_LEN_MAX; i++) {
		kalMemZero(aucValue, WLAN_CFG_VALUE_LEN_MAX);
		kalMemZero(aucKey, WLAN_CFG_VALUE_LEN_MAX);
		kalSnprintf(aucKey, sizeof(aucKey), "SwCtrl%d", i);

		/* get nothing */
		if (wlanCfgGet(prAdapter, aucKey, aucValue, "", 0) != WLAN_STATUS_SUCCESS)
			continue;
		if (!kalStrCmp(aucValue, ""))
			continue;

		pcDupValue = aucValue;
		u4TokenCount = 0;

		while ((pcPtr = kalStrSep((char **)(&pcDupValue), acDelim)) != NULL) {

			if (!kalStrCmp(pcPtr, ""))
				continue;

			/* au4Values[u4TokenCount] = kalStrtoul(pcPtr, NULL, 0); */
			u4Ret = kalkStrtou32(pcPtr, 0, &(au4Values[u4TokenCount]));
			if (u4Ret)
				DBGLOG(INIT, LOUD, "parse au4Values error u4Ret=%d\n", u4Ret);
			u4TokenCount++;

			/* Only need 2 tokens */
			if (u4TokenCount >= 2)
				break;
		}

		if (u4TokenCount != 2)
			continue;

		rSwCtrlInfo.u4Id = au4Values[0];
		rSwCtrlInfo.u4Data = au4Values[1];

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetSwCtrlWrite,
				   &rSwCtrlInfo, sizeof(rSwCtrlInfo), FALSE, FALSE, TRUE, &u4BufLen);

	}
}

VOID wlanCfgSetChip(IN P_ADAPTER_T prAdapter)
{
	UINT_32 i = 0;
	CHAR aucKey[WLAN_CFG_VALUE_LEN_MAX];
	CHAR aucValue[WLAN_CFG_VALUE_LEN_MAX];

	UINT_32 u4BufLen = 0;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	P_GLUE_INFO_T prGlueInfo = prAdapter->prGlueInfo;
	PARAM_CUSTOM_CHIP_CONFIG_STRUCT_T rChipConfigInfo;

	for (i = 0; i < WLAN_CFG_SET_CHIP_LEN_MAX; i++) {
		kalMemZero(aucValue, WLAN_CFG_VALUE_LEN_MAX);
		kalMemZero(aucKey, WLAN_CFG_VALUE_LEN_MAX);
		kalSnprintf(aucKey, sizeof(aucKey), "SetChip%d", i);

		/* get nothing */
		if (wlanCfgGet(prAdapter, aucKey, aucValue, "", 0) != WLAN_STATUS_SUCCESS)
			continue;
		if (!kalStrCmp(aucValue, ""))
			continue;

		kalMemZero(&rChipConfigInfo, sizeof(rChipConfigInfo));

		rChipConfigInfo.ucType = CHIP_CONFIG_TYPE_WO_RESPONSE;
		rChipConfigInfo.u2MsgSize = kalStrnLen(aucValue, WLAN_CFG_VALUE_LEN_MAX);
		kalStrnCpy(rChipConfigInfo.aucCmd, aucValue, CHIP_CONFIG_RESP_SIZE);

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetChipConfig,
				   &rChipConfigInfo, sizeof(rChipConfigInfo), FALSE, FALSE, TRUE, &u4BufLen);
	}

}

VOID wlanCfgSetDebugLevel(IN P_ADAPTER_T prAdapter)
{
	UINT_32 i = 0;
	CHAR aucKey[WLAN_CFG_VALUE_LEN_MAX];
	CHAR aucValue[WLAN_CFG_VALUE_LEN_MAX];
	const CHAR acDelim[] = " ";
	CHAR *pcDupValue;
	CHAR *pcPtr = NULL;

	UINT_32 au4Values[2];
	UINT_32 u4TokenCount = 0;
	UINT_32 u4DbgIdx = 0;
	UINT_32 u4DbgMask = 0;
	INT_32 u4Ret = 0;

	for (i = 0; i < WLAN_CFG_SET_DEBUG_LEVEL_LEN_MAX; i++) {
		kalMemZero(aucValue, WLAN_CFG_VALUE_LEN_MAX);
		kalMemZero(aucKey, WLAN_CFG_VALUE_LEN_MAX);
		kalSnprintf(aucKey, sizeof(aucKey), "DbgLevel%d", i);

		/* get nothing */
		if (wlanCfgGet(prAdapter, aucKey, aucValue, "", 0) != WLAN_STATUS_SUCCESS)
			continue;
		if (!kalStrCmp(aucValue, ""))
			continue;

		pcDupValue = aucValue;
		u4TokenCount = 0;

		while ((pcPtr = kalStrSep((char **)(&pcDupValue), acDelim)) != NULL) {

			if (!kalStrCmp(pcPtr, ""))
				continue;

			/* au4Values[u4TokenCount] = kalStrtoul(pcPtr, NULL, 0); */
			u4Ret = kalkStrtou32(pcPtr, 0, &(au4Values[u4TokenCount]));
			if (u4Ret)
				DBGLOG(INIT, LOUD, "parse au4Values error u4Ret=%d\n", u4Ret);
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
			wlanSetDebugLevel(DBG_ALL_MODULE_IDX, u4DbgMask);
			DBGLOG(INIT, INFO, "Set ALL DBG module log level to [0x%02x]!", (UINT_8) u4DbgMask);
		} else if (u4DbgIdx == 0xFFFFFFFE) {
			wlanDebugInit();
			DBGLOG(INIT, INFO, "Reset ALL DBG module log level to DEFAULT!");
		} else if (u4DbgIdx < DBG_MODULE_NUM) {
			wlanSetDebugLevel(u4DbgIdx, u4DbgMask);
			DBGLOG(INIT, INFO,
				"Set DBG module[%u] log level to [0x%02x]!",
				u4DbgIdx, (UINT_8) u4DbgMask);
		}
	}

}

VOID wlanCfgSetCountryCode(IN P_ADAPTER_T prAdapter)
{
	CHAR aucValue[WLAN_CFG_VALUE_LEN_MAX];

	/* Apply COUNTRY Config */
	if (wlanCfgGet(prAdapter, "Country", aucValue, "", 0) == WLAN_STATUS_SUCCESS) {
		prAdapter->rWifiVar.rConnSettings.u2CountryCode =
		    (((UINT_16) aucValue[0]) << 8) | ((UINT_16) aucValue[1]);

		DBGLOG(INIT, TRACE, "u2CountryCode=0x%04x\n",
			   prAdapter->rWifiVar.rConnSettings.u2CountryCode);

		if (regd_is_single_sku_en()) {
			rlmDomainOidSetCountry(prAdapter, aucValue, 2);
			return;
		}

		/* Force to re-search country code in country domains */
		prAdapter->prDomainInfo = NULL;
		rlmDomainSendCmd(prAdapter, TRUE);

		/* Update supported channel list in channel table based on current country domain */
		wlanUpdateChannelTable(prAdapter->prGlueInfo);
	}
}

#if CFG_SUPPORT_CFG_FILE

P_WLAN_CFG_ENTRY_T wlanCfgGetEntry(IN P_ADAPTER_T prAdapter, const PCHAR pucKey, BOOLEAN fgGetCfgRec)
{

	P_WLAN_CFG_ENTRY_T prWlanCfgEntry;
	P_WLAN_CFG_T prWlanCfg = NULL;
	P_WLAN_CFG_REC_T prWlanCfgRec = NULL;
	UINT_32 i, u32MaxNum;

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
			DBGLOG(INIT, LOUD, "compare key %s saved key %s\n", pucKey, prWlanCfgEntry->aucKey);
			if (kalStrnCmp(pucKey, prWlanCfgEntry->aucKey, WLAN_CFG_KEY_LEN_MAX - 1) == 0)
				return prWlanCfgEntry;
		}
	}

	DBGLOG(INIT, TRACE, "wifi config there is no entry \'%s\'\n", pucKey);
	return NULL;

}


P_WLAN_CFG_ENTRY_T wlanCfgGetEntryByIndex(IN P_ADAPTER_T prAdapter, const UINT_8 ucIdx, UINT_32 flag)
{

	P_WLAN_CFG_ENTRY_T prWlanCfgEntry;
	P_WLAN_CFG_T prWlanCfg;
	P_WLAN_CFG_REC_T prWlanCfgRec;


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
		DBGLOG(INIT, LOUD, "get Index(%d) saved key %s\n", ucIdx, prWlanCfgEntry->aucKey);
			return prWlanCfgEntry;
	}

	DBGLOG(INIT, TRACE, "wifi config there is no entry at index(%d)\n", ucIdx);
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
	prWlanCfgEntry = wlanCfgGetEntry(prAdapter, pucKey, FALSE);

	if (prWlanCfgEntry) {
		kalStrnCpy(pucValue, prWlanCfgEntry->aucValue, WLAN_CFG_VALUE_LEN_MAX - 1);
		return WLAN_STATUS_SUCCESS;
	}
		if (pucValueDef)
			kalStrnCpy(pucValue, pucValueDef, WLAN_CFG_VALUE_LEN_MAX - 1);
		return WLAN_STATUS_FAILURE;


}

VOID wlanCfgRecordValue(IN P_ADAPTER_T prAdapter, const PCHAR pucKey, UINT_32 u4Value)
{
	P_WLAN_CFG_ENTRY_T prWlanCfgEntry;
	UINT_8 aucBuf[WLAN_CFG_VALUE_LEN_MAX];

	prWlanCfgEntry = wlanCfgGetEntry(prAdapter, pucKey, TRUE);

	kalMemZero(aucBuf, sizeof(aucBuf));

	kalSnprintf(aucBuf, WLAN_CFG_VALUE_LEN_MAX, "0x%x", (unsigned int)u4Value);

	wlanCfgSet(prAdapter, pucKey, aucBuf, 1);
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
	prWlanCfgEntry = wlanCfgGetEntry(prAdapter, pucKey, FALSE);

	if (prWlanCfgEntry) {
		/* u4Ret = kalStrtoul(prWlanCfgEntry->aucValue, NULL, 0); */
		u4Ret = kalkStrtou32(prWlanCfgEntry->aucValue, 0, &u4Value);
		if (u4Ret)
			DBGLOG(INIT, LOUD, "parse aucValue error u4Ret=%d\n", u4Ret);
	}

	wlanCfgRecordValue(prAdapter, pucKey, u4Value);

	return u4Value;
}

INT_32 wlanCfgGetInt32(IN P_ADAPTER_T prAdapter, const PCHAR pucKey, INT_32 i4ValueDef)
{
	P_WLAN_CFG_ENTRY_T prWlanCfgEntry;
	P_WLAN_CFG_T prWlanCfg;
	INT_32 i4Value = 0;
	INT_32 i4Ret = 0;

	prWlanCfg = prAdapter->prWlanCfg;

	ASSERT(prWlanCfg);

	i4Value = i4ValueDef;
	/* Find the exist */
	prWlanCfgEntry = wlanCfgGetEntry(prAdapter, pucKey, FALSE);

	if (prWlanCfgEntry) {
		/* i4Ret = kalStrtol(prWlanCfgEntry->aucValue, NULL, 0); */
		i4Ret = kalkStrtos32(prWlanCfgEntry->aucValue, 0, &i4Value);
		if (i4Ret)
			DBGLOG(INIT, LOUD, "parse aucValue error i4Ret=%d\n", i4Ret);
	}

	wlanCfgRecordValue(prAdapter, pucKey, (UINT_32)i4Value);

	return i4Value;
}

WLAN_STATUS wlanCfgSet(IN P_ADAPTER_T prAdapter, const PCHAR pucKey, PCHAR pucValue, UINT_32 u4Flags)
{

	P_WLAN_CFG_ENTRY_T prWlanCfgEntry;
	P_WLAN_CFG_T prWlanCfg = NULL;
	P_WLAN_CFG_REC_T prWlanCfgRec = NULL;
	UINT_32 u4EntryIndex;
	UINT_32 i;
	UINT_8 ucExist;
	BOOLEAN fgGetCfgRec = FALSE;


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
				prWlanCfgEntry = &prWlanCfgRec->arWlanCfgBuf[u4EntryIndex];
			else
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
		/* this will lead the log too much ,and modify  the log level*/
		DBGLOG(INIT, TRACE, "Set wifi config exist %u \'%s\' \'%s\'\n",
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
#if CFG_SUPPORT_EASY_DEBUG
	UINT_32 textsize;
#endif
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
		/*case ':':
		*/
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
#if CFG_SUPPORT_EASY_DEBUG
	state.textsize = 0;
#endif

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

#if CFG_WOW_SUPPORT
WLAN_STATUS wlanCfgParseArgumentLong(CHAR *cmdLine, INT_32 *argc, CHAR *argv[])
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
#if CFG_SUPPORT_EASY_DEBUG
	state.textsize = 0;
#endif

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

	kalStrnCpy(aucKey, pucKeyHead, u4Len);

	if (pucValueTail) {
		if (pucValueHead > pucValueTail)
			return WLAN_STATUS_FAILURE;
		u4Len = pucValueTail - pucValueHead + 1;
	} else
		u4Len = kalStrnLen(pucValueHead, WLAN_CFG_VALUE_LEN_MAX - 1);

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

INT_8 atoi(UCHAR ch)
{
	if (ch >= 'a' && ch <= 'f')
		return ch - 87;
	else if (ch >= 'A' && ch <= 'F')
		return ch - 55;
	else if (ch >= '0' && ch <= '9')
		return ch - 48;

	return 0;
}

WLAN_STATUS wlanCfgParseToFW(PCHAR *args, PCHAR args_size, UCHAR nargs, PCHAR buffer, UCHAR times)
{
	PUCHAR data = NULL;
	char ch;
	INT_32 i = 0, j = 0;
	UINT_32 bufferindex = 0, base = 0;
	UINT_32 sum = 0, startOffset = 0;
	CMD_FORMAT_V1_T cmd_v1;

	memset(&cmd_v1, 0, sizeof(CMD_FORMAT_V1_T));

#if 0
	cmd_v1.itemType = atoi(*args[ED_ITEMTYPE_SITE]);
#else
	cmd_v1.itemType = ITEM_TYPE_DEC;
#endif
	if (buffer == NULL ||
		args_size[ED_STRING_SITE] == 0 ||
		args_size[ED_VALUE_SITE] == 0 ||
		(cmd_v1.itemType < ITEM_TYPE_DEC || cmd_v1.itemType > ITEM_TYPE_STR)) {
		DBGLOG(INIT, ERROR, "cfg args wrong\n");
		return WLAN_STATUS_FAILURE;
	}

	cmd_v1.itemStringLength = args_size[ED_STRING_SITE];
	strncpy(cmd_v1.itemString, args[ED_STRING_SITE],  cmd_v1.itemStringLength);
	DBGLOG(INIT, INFO, "itemString:");
	for (i = 0; i <  cmd_v1.itemStringLength; i++)
		DBGLOG(INIT, INFO, "%c", cmd_v1.itemString[i]);
	DBGLOG(INIT, INFO, "\n");

	DBGLOG(INIT, INFO, "cmd_v1.itemType = %d\n", cmd_v1.itemType);
	if (cmd_v1.itemType == ITEM_TYPE_DEC || cmd_v1.itemType == ITEM_TYPE_HEX) {
		data = args[ED_VALUE_SITE];

		switch (cmd_v1.itemType) {
		case ITEM_TYPE_DEC:
			base = 10;
			startOffset = 0;
			break;
		case ITEM_TYPE_HEX:
			ch = *data;
			if (args_size[ED_VALUE_SITE] < 3 || ch != '0') {
				DBGLOG(INIT, WARN, "Hex args must have prefix '0x'\n");
				return WLAN_STATUS_FAILURE;
			}

			data++;
			ch = *data;
			if (ch != 'x' && ch != 'X') {
				DBGLOG(INIT, WARN, "Hex args must have prefix '0x'\n");
				return WLAN_STATUS_FAILURE;
			}
			data++;
			base = 16;
			startOffset = 2;
			break;
		}

		for (j = args_size[ED_VALUE_SITE] - 1 - startOffset; j >= 0; j--) {
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
		strncpy(cmd_v1.itemValue, args[ED_VALUE_SITE], cmd_v1.itemValueLength);
	}

	DBGLOG(INIT, INFO, "Length = %d itemValue:", cmd_v1.itemValueLength);
	for (i = cmd_v1.itemValueLength - 1; i >= 0; i--)
		DBGLOG(INIT, ERROR, "%d,", cmd_v1.itemValue[i]);
	DBGLOG(INIT, INFO, "\n");
	memcpy(((P_CMD_FORMAT_V1_T)buffer)+times, &cmd_v1,  sizeof(CMD_FORMAT_V1_T));

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
VOID wlanFeatureToFw(IN P_ADAPTER_T prAdapter)
{

	P_WLAN_CFG_ENTRY_T prWlanCfgEntry;
	UINT_32 i;
	CMD_HEADER_T rCmdV1Header;
	WLAN_STATUS rStatus;
	CMD_FORMAT_V1_T rCmd_v1;
	UCHAR  ucTimes = 0;



	rCmdV1Header.cmdType = CMD_TYPE_SET;
	rCmdV1Header.cmdVersion = CMD_VER_1;
	rCmdV1Header.cmdBufferLen = 0;
	rCmdV1Header.itemNum = 0;

	kalMemSet(rCmdV1Header.buffer, 0, MAX_CMD_BUFFER_LENGTH);
	kalMemSet(&rCmd_v1, 0, sizeof(CMD_FORMAT_V1_T));


	prWlanCfgEntry = NULL;

	for (i = 0; i < WLAN_CFG_ENTRY_NUM_MAX; i++) {

		prWlanCfgEntry = wlanCfgGetEntryByIndex(prAdapter, i, 0);

		if (prWlanCfgEntry) {

			rCmd_v1.itemType = ITEM_TYPE_STR;


			/*send string format to firmware */
			rCmd_v1.itemStringLength = kalStrLen(prWlanCfgEntry->aucKey);
			kalMemZero(rCmd_v1.itemString, MAX_CMD_NAME_MAX_LENGTH);
			kalMemCopy(rCmd_v1.itemString, prWlanCfgEntry->aucKey, rCmd_v1.itemStringLength);


			rCmd_v1.itemValueLength = kalStrLen(prWlanCfgEntry->aucValue);
			kalMemZero(rCmd_v1.itemValue, MAX_CMD_VALUE_MAX_LENGTH);
			kalMemCopy(rCmd_v1.itemValue, prWlanCfgEntry->aucValue, rCmd_v1.itemValueLength);



			DBGLOG(INIT, INFO, "Send key word (%s) WITH (%s) to firmware\n",
				rCmd_v1.itemString, rCmd_v1.itemValue);

			kalMemCopy(((P_CMD_FORMAT_V1_T)rCmdV1Header.buffer)+ucTimes,
				&rCmd_v1,  sizeof(CMD_FORMAT_V1_T));


			ucTimes++;
			rCmdV1Header.cmdBufferLen += sizeof(CMD_FORMAT_V1_T);
			rCmdV1Header.itemNum += ucTimes;


			if (ucTimes == MAX_CMD_ITEM_MAX) {
				/* Send to FW */
				rCmdV1Header.itemNum = ucTimes;

				rStatus = wlanSendSetQueryCmd(
					prAdapter,				/* prAdapter */
					CMD_ID_GET_SET_CUSTOMER_CFG,	/* 0x70 */
					TRUE,					/* fgSetQuery */
					FALSE,					/* fgNeedResp */
					FALSE,					/* fgIsOid */
					NULL,					/* pfCmdDoneHandler*/
					NULL,	/* pfCmdTimeoutHandler */
					sizeof(CMD_HEADER_T),	/* u4SetQueryInfoLen */
					(PUINT_8)&rCmdV1Header,	/* pucInfoBuffer */
					NULL,	/* pvSetQueryBuffer */
					0	/* u4SetQueryBufferLen */
				);

				if (rStatus == WLAN_STATUS_FAILURE)
					DBGLOG(INIT, INFO, "[Fail]kalIoctl wifiSefCFG fail 0x%x\n", rStatus);

				DBGLOG(INIT, INFO, "kalIoctl wifiSefCFG num:%d\n", ucTimes);
				kalMemSet(rCmdV1Header.buffer, 0, MAX_CMD_BUFFER_LENGTH);
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

		DBGLOG(INIT, INFO, "cmdV1Header.itemNum:%d\n", rCmdV1Header.itemNum);

		rStatus = wlanSendSetQueryCmd(
			prAdapter,					/* prAdapter */
			CMD_ID_GET_SET_CUSTOMER_CFG,	/* 0x70 */
			TRUE,						/* fgSetQuery */
			FALSE,						/* fgNeedResp */
			FALSE,						/* fgIsOid */
			NULL,						/* pfCmdDoneHandler*/
			NULL,						/* pfCmdTimeoutHandler */
			sizeof(CMD_HEADER_T),		/* u4SetQueryInfoLen */
			(PUINT_8)&rCmdV1Header,		/* pucInfoBuffer */
			NULL,                       /* pvSetQueryBuffer */
			0                           /* u4SetQueryBufferLen */
		);

		if (rStatus == WLAN_STATUS_FAILURE)
			DBGLOG(INIT, INFO, "[Fail]kalIoctl wifiSefCFG fail 0x%x\n", rStatus);

		DBGLOG(INIT, INFO, "kalIoctl wifiSefCFG num:%d\n", ucTimes);
		kalMemSet(rCmdV1Header.buffer, 0, MAX_CMD_BUFFER_LENGTH);
		rCmdV1Header.cmdBufferLen = 0;
		ucTimes = 0;
	}

}



WLAN_STATUS wlanCfgParse(IN P_ADAPTER_T prAdapter, PUINT_8 pucConfigBuf, UINT_32 u4ConfigBufLen, BOOLEAN isFwConfig)
{
	struct WLAN_CFG_PARSE_STATE_S state;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	PPCHAR ppcArgs;
	INT_32 i4Nargs;
	CHAR   arcArgv_size[WLAN_CFG_ARGV_MAX];
	UCHAR  ucTimes = 0;
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;
	CMD_HEADER_T rCmdV1Header;
	CHAR ucTmp[WLAN_CFG_VALUE_LEN_MAX];
	UINT_8 i;

	PUINT_8 pucCurrBuf = ucTmp;
	UINT_32 u4CurrSize = ARRAY_SIZE(ucTmp);
	UINT_32 u4RetSize = 0;

	rCmdV1Header.cmdType = CMD_TYPE_SET;
	rCmdV1Header.cmdVersion = CMD_VER_1;
	rCmdV1Header.cmdBufferLen = 0;
	kalMemSet(rCmdV1Header.buffer, 0, MAX_CMD_BUFFER_LENGTH);

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

			/*3 parmeter mode transforation */
			if (i4Nargs == 3 && !isFwConfig &&
				 arcArgv_size[0] == 1) {

				/*parsing and transfer the format
				*Format  1:Dec 2.Hex 3.String
				*/

				kalMemZero(ucTmp, WLAN_CFG_VALUE_LEN_MAX);
				pucCurrBuf = ucTmp;
				u4CurrSize = ARRAY_SIZE(ucTmp);

				if ((*ppcArgs[0] == '2') && (*(ppcArgs[2]) != '0') && (*(ppcArgs[2]+1) != 'x')) {
					DBGLOG(INIT, WARN, "config file got a hex format\n");
					kalSnprintf(pucCurrBuf, u4CurrSize, "0x%s", ppcArgs[2]);
				} else {
					kalSnprintf(pucCurrBuf, u4CurrSize, "%s", ppcArgs[2]);
				}
				DBGLOG(INIT, WARN, "[3 parameter mode][%s],[%s],[%s]\n", ppcArgs[0], ppcArgs[1], ucTmp);
				wlanCfgParseAddEntry(prAdapter, ppcArgs[1], NULL, ucTmp, NULL);
				kalMemSet(arcArgv_size, 0, WLAN_CFG_ARGV_MAX);
				kalMemSet(apcArgv, 0,
					WLAN_CFG_ARGV_MAX * sizeof(int8_t *));
				i4Nargs = 0;
				goto exit;

			}

			wlanCfgParseAddEntry(prAdapter, ppcArgs[0], NULL, ppcArgs[1], NULL);

			if (isFwConfig) {

				WLAN_STATUS ret;


				ret = wlanCfgParseToFW(ppcArgs, arcArgv_size, i4Nargs, rCmdV1Header.buffer, ucTimes);
				if (ret == WLAN_STATUS_SUCCESS) {
					ucTimes++;
					rCmdV1Header.cmdBufferLen += sizeof(CMD_FORMAT_V1_T);
				}
			}

			goto exit;


		case STATE_NEWLINE:
			if (i4Nargs < 2)
				break;

			DBGLOG(INIT, INFO, "STATE_NEWLINE\n");
#if 1
			/*3 parmeter mode transforation */
			if (i4Nargs == 3 && !isFwConfig &&
				 arcArgv_size[0] == 1) {

				/*parsing and transfer the format
				*Format  1:Dec 2.Hex 3.String
				*/
				kalMemZero(ucTmp, WLAN_CFG_VALUE_LEN_MAX);
				pucCurrBuf = ucTmp;
				u4CurrSize = ARRAY_SIZE(ucTmp);

				if ((*ppcArgs[0] == '2')  && (*(ppcArgs[2]) != '0') && (*(ppcArgs[2]+1) != 'x')) {
					DBGLOG(INIT, WARN, "config file got a hex format\n");
					kalSnprintf(pucCurrBuf, u4CurrSize, "0x%s", ppcArgs[2]);

				} else {
					kalSnprintf(pucCurrBuf, u4CurrSize, "%s", ppcArgs[2]);
				}


				DBGLOG(INIT, WARN, "[3 parameter mode][%s],[%s],[%s]\n", ppcArgs[0], ppcArgs[1], ucTmp);
				wlanCfgParseAddEntry(prAdapter, ppcArgs[1], NULL, ucTmp, NULL);
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
				/*no space for it, driver can't accept space in the end of the line*/
				/*ToDo: skip the space when parsing*/
				kalSnprintf(pucCurrBuf, u4CurrSize, "%s", ppcArgs[1]);
			} else {
				for (i = 1; i < i4Nargs; i++) {
					if (u4CurrSize <= 1) {
						DBGLOG(INIT, ERROR, "write to pucCurrBuf out of bound, i=%d\n", i);
						break;
					}
					u4RetSize = scnprintf(pucCurrBuf, u4CurrSize, "%s ", ppcArgs[i]);
					pucCurrBuf += u4RetSize;
					u4CurrSize -= u4RetSize;
				}
			}

			DBGLOG(INIT, INFO, "Save to driver temp buffer as [%s]\n", ucTmp);
			wlanCfgParseAddEntry(prAdapter, ppcArgs[0], NULL, ucTmp, NULL);
#else
			wlanCfgParseAddEntry(prAdapter, ppcArgs[0], NULL, ppcArgs[1], NULL);
#endif

			if (isFwConfig) {

				WLAN_STATUS ret;

				ret = wlanCfgParseToFW(ppcArgs, arcArgv_size, i4Nargs, rCmdV1Header.buffer, ucTimes);
				if (ret == WLAN_STATUS_SUCCESS) {
					ucTimes++;
					rCmdV1Header.cmdBufferLen += sizeof(CMD_FORMAT_V1_T);
				}

				if (ucTimes == MAX_CMD_ITEM_MAX) {
					/* Send to FW */
					rCmdV1Header.itemNum = ucTimes;
					rStatus = wlanSendSetQueryCmd(
						prAdapter,				/* prAdapter */
						CMD_ID_GET_SET_CUSTOMER_CFG,	/* 0x70 */
						TRUE,					/* fgSetQuery */
						FALSE,					/* fgNeedResp */
						FALSE,					/* fgIsOid */
						NULL,					/* pfCmdDoneHandler*/
						NULL,	/* pfCmdTimeoutHandler */
						sizeof(CMD_HEADER_T),	/* u4SetQueryInfoLen */
						(PUINT_8) &rCmdV1Header,	/* pucInfoBuffer */
						NULL,	/* pvSetQueryBuffer */
						0	/* u4SetQueryBufferLen */
					);

					if (rStatus == WLAN_STATUS_FAILURE)
						DBGLOG(INIT, INFO, "kalIoctl wifiSefCFG fail 0x%x\n", rStatus);
					DBGLOG(INIT, INFO, "kalIoctl wifiSefCFG num:%d X\n", ucTimes);
					kalMemSet(rCmdV1Header.buffer, 0, MAX_CMD_BUFFER_LENGTH);
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
				arcArgv_size[i4Nargs-1] = state.textsize;
				state.textsize = 0;
				DBGLOG(INIT, INFO, " nargs= %d STATE_TEXT = %s, SIZE = %d\n",
					i4Nargs-1, ppcArgs[i4Nargs-1], arcArgv_size[i4Nargs-1]);
			}
			break;
		}
	}

exit:
	if (ucTimes != 0 && isFwConfig) {
		/* Send to FW */
		rCmdV1Header.itemNum = ucTimes;

		DBGLOG(INIT, INFO, "cmdV1Header.itemNum:%d\n", rCmdV1Header.itemNum);
		rStatus = wlanSendSetQueryCmd(
			prAdapter,					/* prAdapter */
			CMD_ID_GET_SET_CUSTOMER_CFG,	/* 0x70 */
			TRUE,						/* fgSetQuery */
			FALSE,						/* fgNeedResp */
			FALSE,						/* fgIsOid */
			NULL,						/* pfCmdDoneHandler*/
			NULL,						/* pfCmdTimeoutHandler */
			sizeof(CMD_HEADER_T),		/* u4SetQueryInfoLen */
			(PUINT_8) &rCmdV1Header,		/* pucInfoBuffer */
			NULL,                       /* pvSetQueryBuffer */
			0                           /* u4SetQueryBufferLen */
		);

		if (rStatus == WLAN_STATUS_FAILURE)
			DBGLOG(INIT, WARN, "kalIoctl wifiSefCFG fail 0x%x\n", rStatus);

		DBGLOG(INIT, WARN, "kalIoctl wifiSefCFG num:%d X\n", ucTimes);
		kalMemSet(rCmdV1Header.buffer, 0, MAX_CMD_BUFFER_LENGTH);
		rCmdV1Header.cmdBufferLen = 0;
		ucTimes = 0;
	}
	return WLAN_STATUS_SUCCESS;
}

#else
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
			wlanCfgParseAddEntry(prAdapter, pucKeyHead, pucKeyTail, pucValueHead, pucValueTail);
	}
#endif

	return WLAN_STATUS_SUCCESS;
}
#endif


WLAN_STATUS wlanCfgInit(IN P_ADAPTER_T prAdapter, PUINT_8 pucConfigBuf, UINT_32 u4ConfigBufLen, UINT_32 u4Flags)
{
	P_WLAN_CFG_T prWlanCfg;
	P_WLAN_CFG_REC_T prWlanCfgRec;
	/* P_WLAN_CFG_ENTRY_T prWlanCfgEntry; */
	prAdapter->prWlanCfg = &prAdapter->rWlanCfg;
	prWlanCfg = prAdapter->prWlanCfg;

	prAdapter->prWlanCfgRec = &prAdapter->rWlanCfgRec;
	prWlanCfgRec = prAdapter->prWlanCfgRec;

	kalMemZero(prWlanCfg, sizeof(WLAN_CFG_T));
	ASSERT(prWlanCfg);
	prWlanCfg->u4WlanCfgEntryNumMax = WLAN_CFG_ENTRY_NUM_MAX;
	prWlanCfg->u4WlanCfgKeyLenMax = WLAN_CFG_KEY_LEN_MAX;
	prWlanCfg->u4WlanCfgValueLenMax = WLAN_CFG_VALUE_LEN_MAX;

	prWlanCfgRec->u4WlanCfgEntryNumMax = WLAN_CFG_REC_ENTRY_NUM_MAX;
	prWlanCfgRec->u4WlanCfgKeyLenMax = WLAN_CFG_REC_ENTRY_NUM_MAX;
	prWlanCfgRec->u4WlanCfgValueLenMax = WLAN_CFG_REC_ENTRY_NUM_MAX;

	DBGLOG(INIT, INFO, "Init wifi config len %u max entry %u\n", u4ConfigBufLen, prWlanCfg->u4WlanCfgEntryNumMax);
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

	/* Parse the pucConfigBuff */

	if (pucConfigBuf && (u4ConfigBufLen > 0))
#if CFG_SUPPORT_EASY_DEBUG
		wlanCfgParse(prAdapter, pucConfigBuf, u4ConfigBufLen, FALSE);
#else
		wlanCfgParse(prAdapter, pucConfigBuf, u4ConfigBufLen);
#endif
	return WLAN_STATUS_SUCCESS;
}

#endif /* CFG_SUPPORT_CFG_FILE */

INT_32 wlanHexToNum(CHAR c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

INT_32 wlanHexToByte(PCHAR hex)
{
	INT_32 a, b;

	a = wlanHexToNum(*hex++);
	if (a < 0)
		return -1;
	b = wlanHexToNum(*hex++);
	if (b < 0)
		return -1;
	return (a << 4) | b;
}

INT_32 wlanHwAddrToBin(PCHAR txt, UINT_8 *addr)
{
	INT_32 i;
	PCHAR pos = txt;

	for (i = 0; i < 6; i++) {
		INT_32 a, b;

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

BOOLEAN wlanIsChipNoAck(IN P_ADAPTER_T prAdapter)
{
	BOOLEAN fgIsNoAck;

	fgIsNoAck = prAdapter->fgIsChipNoAck
#if CFG_CHIP_RESET_SUPPORT
	    || kalIsResetting()
#endif
	    || fgIsBusAccessFailed;

	return fgIsNoAck;
}

BOOLEAN wlanIsChipRstRecEnabled(IN P_ADAPTER_T prAdapter)
{
	return prAdapter->rWifiVar.fgChipResetRecover;
}

BOOLEAN wlanIsChipAssert(IN P_ADAPTER_T prAdapter)
{
	if (prAdapter == NULL)
		return TRUE;
	return prAdapter->rWifiVar.fgChipResetRecover && prAdapter->fgIsChipAssert;
}

VOID wlanChipRstPreAct(IN P_ADAPTER_T prAdapter)
{
	P_BSS_INFO_T prBssInfo = (P_BSS_INFO_T) NULL;
	INT_32 i4BssIdx;
	UINT_32 u4ClientCount = 0;
	P_STA_RECORD_T prCurrStaRec = (P_STA_RECORD_T) NULL;
	P_STA_RECORD_T prNextCurrStaRec = (P_STA_RECORD_T) NULL;
	P_LINK_T prClientList;
	P_GLUE_INFO_T prGlueInfo = prAdapter->prGlueInfo;

	spin_lock_bh(&prGlueInfo->rSpinLock[SPIN_LOCK_CHIP_RST]);
	if (prAdapter->fgIsChipAssert) {
		spin_unlock_bh(&prGlueInfo->rSpinLock[SPIN_LOCK_CHIP_RST]);
		return;
	}
	prAdapter->fgIsChipAssert = TRUE;
	spin_unlock_bh(&prGlueInfo->rSpinLock[SPIN_LOCK_CHIP_RST]);

	for (i4BssIdx = 0; i4BssIdx < HW_BSSID_NUM; i4BssIdx++) {
		prBssInfo = prAdapter->aprBssInfo[i4BssIdx];

		if (!prBssInfo->fgIsInUse)
			continue;

		if (prBssInfo->eNetworkType == NETWORK_TYPE_AIS) {

			if (prGlueInfo->eParamMediaStateIndicated == PARAM_MEDIA_STATE_CONNECTED)
				kalIndicateStatusAndComplete(prGlueInfo, WLAN_STATUS_MEDIA_DISCONNECT, NULL, 0);
		} else if (prBssInfo->eNetworkType == NETWORK_TYPE_P2P) {
			if (prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT) {
				u4ClientCount = bssGetClientCount(prAdapter, prBssInfo);

				if (u4ClientCount == 0)
					continue;

				prClientList = &prBssInfo->rStaRecOfClientList;
				LINK_FOR_EACH_ENTRY_SAFE(prCurrStaRec, prNextCurrStaRec,
					prClientList, rLinkEntry, STA_RECORD_T) {
					kalP2PGOStationUpdate(prAdapter->prGlueInfo,
						(UINT_8) prBssInfo->u4PrivateData, prCurrStaRec, FALSE);
					LINK_REMOVE_KNOWN_ENTRY(prClientList, &prCurrStaRec->rLinkEntry);
				}
			} else if (prBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE) {
				if (prBssInfo->prStaRecOfAP == NULL)
					continue;
#if CFG_WPS_DISCONNECT || (KERNEL_VERSION(4, 4, 0) <= CFG80211_VERSION_CODE)
				kalP2PGCIndicateConnectionStatus(prGlueInfo,
					(UINT_8) prBssInfo->u4PrivateData,
					NULL, NULL, 0, 0,
					WLAN_STATUS_MEDIA_DISCONNECT);
#else
				kalP2PGCIndicateConnectionStatus(prGlueInfo,
							 (UINT_8) prBssInfo->u4PrivateData, NULL, NULL, 0, 0);
#endif
				prBssInfo->prStaRecOfAP = NULL;

			}
		}
	}
}

#if CFG_ENABLE_PER_STA_STATISTICS
VOID wlanTxLifetimeUpdateStaStats(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo)
{
	P_STA_RECORD_T prStaRec;
	UINT_32 u4DeltaTime;
	P_PKT_PROFILE_T prPktProfile = &prMsduInfo->rPktProfile;

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

	if (prStaRec) {
		u4DeltaTime = (UINT_32) (prPktProfile->rHifTxDoneTimestamp - prPktProfile->rHardXmitArrivalTimestamp);

		/* Update StaRec statistics */
		prStaRec->u4TotalTxPktsNumber++;
		prStaRec->u4TotalTxPktsTime += u4DeltaTime;

		if (u4DeltaTime > prStaRec->u4MaxTxPktsTime)
			prStaRec->u4MaxTxPktsTime = u4DeltaTime;
		if (u4DeltaTime >= NIC_TX_TIME_THRESHOLD)
			prStaRec->u4ThresholdCounter++;
	}
}
#endif

BOOLEAN wlanTxLifetimeIsProfilingEnabled(IN P_ADAPTER_T prAdapter)
{
	BOOLEAN fgEnabled = FALSE;
#if CFG_SUPPORT_WFD
	P_WFD_CFG_SETTINGS_T prWfdCfgSettings = (P_WFD_CFG_SETTINGS_T) NULL;

	prWfdCfgSettings = &prAdapter->rWifiVar.rWfdConfigureSettings;

	if (prWfdCfgSettings->ucWfdEnable > 0)
		fgEnabled = TRUE;
#endif

	return fgEnabled;
}

BOOLEAN wlanTxLifetimeIsTargetMsdu(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo)
{
	BOOLEAN fgResult = TRUE;

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

VOID wlanTxLifetimeTagPacket(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_PROFILING_TAG_T eTag)
{
	P_PKT_PROFILE_T prPktProfile = &prMsduInfo->rPktProfile;

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
			prPktProfile->rHardXmitArrivalTimestamp = GLUE_GET_PKT_ARRIVAL_TIME(prMsduInfo->prPacket);

			/* Packet enqueue time */
			prPktProfile->rEnqueueTimestamp = (OS_SYSTIME) kalGetTimeTick();
		}
		break;

	case TX_PROF_TAG_DRV_DEQUE:
		if (prPktProfile->fgIsValid)
			prPktProfile->rDequeueTimestamp = (OS_SYSTIME) kalGetTimeTick();
		break;

	case TX_PROF_TAG_DRV_TX_DONE:
		if (prPktProfile->fgIsValid) {
			prPktProfile->rHifTxDoneTimestamp = (OS_SYSTIME) kalGetTimeTick();

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

VOID wlanTxProfilingTagPacket(IN P_ADAPTER_T prAdapter, IN P_NATIVE_PACKET prPacket, IN ENUM_TX_PROFILING_TAG_T eTag)
{
#if CFG_MET_PACKET_TRACE_SUPPORT
	kalMetTagPacket(prAdapter->prGlueInfo, prPacket, eTag);
#endif
}

VOID wlanTxProfilingTagMsdu(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_PROFILING_TAG_T eTag)
{
	wlanTxLifetimeTagPacket(prAdapter, prMsduInfo, eTag);

	wlanTxProfilingTagPacket(prAdapter, prMsduInfo->prPacket, eTag);
}

VOID wlanUpdateTxStatistics(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo, IN BOOLEAN fgTxDrop)
{
	P_STA_RECORD_T prStaRec;
	P_BSS_INFO_T prBssInfo;
	ENUM_WMM_ACI_T eAci = WMM_AC_BE_INDEX;
	P_QUE_MGT_T prQM = &prAdapter->rQM;
	OS_SYSTIME rCurTime;

	eAci = aucTid2ACI[prMsduInfo->ucUserPriority];

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

	if (prStaRec) {
		if (fgTxDrop)
			prStaRec->arLinkStatistics[eAci].u4TxDropMsdu++;
		else
			prStaRec->arLinkStatistics[eAci].u4TxMsdu++;
	} else {
		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prMsduInfo->ucBssIndex);

		if (fgTxDrop)
			prBssInfo->arLinkStatistics[eAci].u4TxDropMsdu++;
		else
			prBssInfo->arLinkStatistics[eAci].u4TxMsdu++;
	}

	/* Trigger FW stats log every 20s */
	rCurTime = (OS_SYSTIME) kalGetTimeTick();

	DBGLOG(INIT, LOUD, "CUR[%u] LAST[%u] TO[%u]\n", rCurTime,
	       prQM->rLastTxPktDumpTime, CHECK_FOR_TIMEOUT(rCurTime,
							   prQM->rLastTxPktDumpTime,
							   MSEC_TO_SYSTIME(prAdapter->rWifiVar.u4StatsLogTimeout)));

	if (CHECK_FOR_TIMEOUT(rCurTime, prQM->rLastTxPktDumpTime,
			      MSEC_TO_SYSTIME(prAdapter->rWifiVar.u4StatsLogTimeout))) {

		wlanTriggerStatsLog(prAdapter, prAdapter->rWifiVar.u4StatsLogDuration);
		wlanDumpAllBssStatistics(prAdapter);

		prQM->rLastTxPktDumpTime = rCurTime;
	}
}

VOID wlanUpdateRxStatistics(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
	P_STA_RECORD_T prStaRec;
	ENUM_WMM_ACI_T eAci = WMM_AC_BE_INDEX;

	eAci = aucTid2ACI[prSwRfb->ucTid];

	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);
	if (prStaRec)
		prStaRec->arLinkStatistics[eAci].u4RxMsdu++;
}

WLAN_STATUS wlanTriggerStatsLog(IN P_ADAPTER_T prAdapter, IN UINT_32 u4DurationInMs)
{
	CMD_STATS_LOG_T rStatsLogCmd;
	WLAN_STATUS rResult;

	if (prAdapter->fgIsEnableLpdvt)
		return WLAN_STATUS_NOT_SUPPORTED;

	kalMemZero(&rStatsLogCmd, sizeof(CMD_STATS_LOG_T));

	rStatsLogCmd.u4DurationInMs = u4DurationInMs;

	rResult = wlanSendSetQueryCmd(prAdapter, CMD_ID_STATS_LOG, TRUE, FALSE,
				      FALSE, nicCmdEventSetCommon, nicOidCmdTimeoutCommon,
				      sizeof(CMD_STATS_LOG_T), (PUINT_8) &rStatsLogCmd, NULL, 0);

	return rResult;
}

WLAN_STATUS
wlanDhcpTxDone(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_RESULT_CODE_T rTxDoneStatus)
{
	DBGLOG(SW4, INFO, "DHCP PKT[0x%08x] WIDX:PID[%u:%u] Status[%u]\n",
	       prMsduInfo->u4TxDoneTag, prMsduInfo->ucWlanIndex, prMsduInfo->ucPID, rTxDoneStatus);

	return WLAN_STATUS_SUCCESS;
}

WLAN_STATUS wlanArpTxDone(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_RESULT_CODE_T rTxDoneStatus)
{
	DBGLOG(SW4, INFO, "ARP PKT[0x%08x] WIDX:PID[%u:%u] Status[%u]\n",
	       prMsduInfo->u4TxDoneTag, prMsduInfo->ucWlanIndex, prMsduInfo->ucPID, rTxDoneStatus);

	return WLAN_STATUS_SUCCESS;
}

WLAN_STATUS wlan1xTxDone(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo,
	IN ENUM_TX_RESULT_CODE_T rTxDoneStatus)
{
	DBGLOG(SW4, INFO, "1x PKT[0x%08x] WIDX:PID[%u:%u] Status[%u]\n",
		prMsduInfo->u4TxDoneTag, prMsduInfo->ucWlanIndex, prMsduInfo->ucPID, rTxDoneStatus);

	return WLAN_STATUS_SUCCESS;
}

#if CFG_ASSERT_DUMP
VOID wlanCorDumpTimerReset(IN P_ADAPTER_T prAdapter, BOOLEAN fgIsResetN9)
{

	if (prAdapter->fgN9AssertDumpOngoing || prAdapter->fgCr4AssertDumpOngoing) {

		if (fgIsResetN9) {
			cnmTimerStopTimer(prAdapter, &prAdapter->rN9CorDumpTimer);
			cnmTimerStartTimer(prAdapter, &prAdapter->rN9CorDumpTimer, 5000);
		} else {
			cnmTimerStopTimer(prAdapter, &prAdapter->rCr4CorDumpTimer);
			cnmTimerStartTimer(prAdapter, &prAdapter->rCr4CorDumpTimer, 5000);
		}
	} else {
		DBGLOG(INIT, INFO, "Cr4, N9 CorDump Is not ongoing, ignore timer reset\n");
	}
}

VOID wlanN9CorDumpTimeOut(IN P_ADAPTER_T prAdapter, IN ULONG ulParamPtr)
{

	if (prAdapter->fgN9CorDumpFileOpend) {
		DBGLOG(INIT, INFO, "\n[DUMP_N9]====N9 ASSERT_END====\n");
		prAdapter->fgN9AssertDumpOngoing = FALSE;
		kalCloseCorDumpFile(TRUE);
		prAdapter->fgN9CorDumpFileOpend = FALSE;
	}
}

VOID wlanCr4CorDumpTimeOut(IN P_ADAPTER_T prAdapter, IN ULONG ulParamPtr)
{

	if (prAdapter->fgCr4CorDumpFileOpend) {
		DBGLOG(INIT, INFO, "\n[DUMP_Cr4]====Cr4 ASSERT_END====\n");
		prAdapter->fgCr4AssertDumpOngoing = FALSE;
		kalCloseCorDumpFile(FALSE);
		prAdapter->fgCr4CorDumpFileOpend = FALSE;
	}
}
#endif

BOOL
wlanGetWlanIdxByAddress(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucAddr, OUT PUINT_8 pucIndex)
{
	UINT_8 ucStaRecIdx;
	P_STA_RECORD_T prTempStaRec;

	for (ucStaRecIdx = 0; ucStaRecIdx < CFG_STA_REC_NUM; ucStaRecIdx++) {
		prTempStaRec = &(prAdapter->arStaRec[ucStaRecIdx]);
		if (pucAddr) {
			if (prTempStaRec->fgIsInUse && EQUAL_MAC_ADDR(prTempStaRec->aucMacAddr, pucAddr)) {
				*pucIndex = prTempStaRec->ucWlanIndex;
				return TRUE;
			}
		} else {
			if (prTempStaRec->fgIsInUse && prTempStaRec->ucStaState == STA_STATE_3) {
				*pucIndex = prTempStaRec->ucWlanIndex;
				return TRUE;
			}
		}
	}
	return FALSE;
}


PUINT_8
wlanGetStaAddrByWlanIdx(IN P_ADAPTER_T prAdapter, IN UINT_8 ucIndex)
{
	P_WLAN_TABLE_T prWtbl;

	if (!prAdapter || ucIndex >= WTBL_SIZE)
		return NULL;

	prWtbl = prAdapter->rWifiVar.arWtbl;
	if (prWtbl[ucIndex].ucUsed && prWtbl[ucIndex].ucPairwise)
		return &prWtbl[ucIndex].aucMacAddr[0];

	return NULL;
}

VOID
wlanNotifyFwSuspend(P_GLUE_INFO_T prGlueInfo, struct net_device *prDev, BOOLEAN fgSuspend)
{
	WLAN_STATUS rStatus;
	UINT_32 u4SetInfoLen;
	P_NETDEV_PRIVATE_GLUE_INFO prNetDevPrivate = (P_NETDEV_PRIVATE_GLUE_INFO) NULL;
	CMD_SUSPEND_MODE_SETTING_T rSuspendCmd;

	prNetDevPrivate = (P_NETDEV_PRIVATE_GLUE_INFO) netdev_priv(prDev);

	if (prNetDevPrivate->prGlueInfo != prGlueInfo)
		DBGLOG(REQ, WARN, "%s: unexpected prGlueInfo(0x%p)!\n", __func__, prNetDevPrivate->prGlueInfo);

	rSuspendCmd.ucBssIndex = prNetDevPrivate->ucBssIdx;
	rSuspendCmd.ucEnableSuspendMode = fgSuspend;

	if (prGlueInfo->prAdapter->rWifiVar.ucWow && prGlueInfo->prAdapter->rWowCtrl.fgWowEnable) {
		/* cfg enable + wow enable => Wow On mdtim*/
		rSuspendCmd.ucMdtim = prGlueInfo->prAdapter->rWifiVar.ucWowOnMdtim;
		DBGLOG(REQ, INFO, "mdtim [1]\n");
	} else if (prGlueInfo->prAdapter->rWifiVar.ucWow && !prGlueInfo->prAdapter->rWowCtrl.fgWowEnable) {
		if (prGlueInfo->prAdapter->rWifiVar.ucAdvPws) {
			/* cfg enable + wow disable + adv pws enable => Wow Off mdtim */
			rSuspendCmd.ucMdtim = prGlueInfo->prAdapter->rWifiVar.ucWowOffMdtim;
			DBGLOG(REQ, INFO, "mdtim [2]\n");
		} else {
			rSuspendCmd.ucMdtim = prGlueInfo->prAdapter->rWifiVar.ucWowOnMdtim;
		}
	} else if (!prGlueInfo->prAdapter->rWifiVar.ucWow) {
		if (prGlueInfo->prAdapter->rWifiVar.ucAdvPws) {
			/* cfg disable + adv pws enable => MT6632 case => Wow Off mdtim */
			rSuspendCmd.ucMdtim = prGlueInfo->prAdapter->rWifiVar.ucWowOffMdtim;
			DBGLOG(REQ, INFO, "mdtim [3]\n");
		} else {
			rSuspendCmd.ucMdtim = prGlueInfo->prAdapter->rWifiVar.ucWowOnMdtim;
		}
	}

    /* When FW receive command, it check connection state to decide apply setting or not */

	rStatus = kalIoctl(prGlueInfo,
				wlanoidNotifyFwSuspend,
				(PVOID)&rSuspendCmd,
				sizeof(rSuspendCmd),
				FALSE,
				FALSE,
				TRUE,
				&u4SetInfoLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(REQ, INFO, "wlanNotifyFwSuspend fail\n");
}

WLAN_STATUS
wlanGetStaIdxByWlanIdx(IN P_ADAPTER_T prAdapter, IN UINT_8 ucIndex, OUT PUINT_8 pucStaIdx)
{
	P_WLAN_TABLE_T prWtbl;

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
*                              failed due to invalid length of the query buffer,
*                              returns the amount of storage needed.
*
* \retval WLAN_STATUS_PENDING
* \retval WLAN_STATUS_FAILURE
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryLteSafeChannel(IN P_ADAPTER_T prAdapter,
			   IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	WLAN_STATUS rResult = WLAN_STATUS_FAILURE;
	CMD_GET_LTE_SAFE_CHN_T rQuery_LTE_SAFE_CHN;

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
					nicCmdEventQueryLteSafeChn, /* The handler to receive firmware notification */
					nicOidCmdTimeoutCommon,
					sizeof(CMD_GET_LTE_SAFE_CHN_T),
					(PUINT_8)&rQuery_LTE_SAFE_CHN,
					pvQueryBuffer,
					u4QueryBufferLen);
		DBGLOG(P2P, INFO, "[ACS] Get safe LTE Channels\n");
	} while (FALSE);

	return rResult;
}				/* wlanoidQueryLteSafeChannel */

/*----------------------------------------------------------------------------*/
/*!
* \brief Add dirtiness to neighbor channels of a BSS to estimate channel quality.
*
* \param[in]  prAdapter        Pointer to the Adapter structure.
* \param[in]  prBssDesc        Pointer to the BSS description.
* \param[in]  u4Dirtiness      Expected dirtiness value.
* \param[in]  ucCentralChannel Central channel of the given BSS.
* \param[in]  ucCoveredRange   With ucCoveredRange and ucCentralChannel,
*                              all the affected channels can be enumerated.
*/
/*----------------------------------------------------------------------------*/
static VOID
wlanAddDirtinessToAffectedChannels(P_ADAPTER_T prAdapter,
	P_BSS_DESC_T prBssDesc, UINT_32 u4Dirtiness, UINT_8 ucCentralChannel,
	UINT_8 ucCoveredRange)
{
	UINT_8 ucIdx, ucStart, ucEnd;
	BOOL bIs5GChl = ucCentralChannel > 14;
	UINT_8 ucLeftNeighborChannel, ucRightNeighborChannel,
		ucLeftNeighborChannel2 = 0, ucRightNeighborChannel2 = 0,
		ucLeftestCoveredChannel, ucRightestCoveredChannel;
	P_PARAM_GET_CHN_INFO prGetChnLoad = &(prAdapter->rWifiVar.rChnLoadInfo);

	ucLeftestCoveredChannel = ucCentralChannel > ucCoveredRange ?
		ucCentralChannel - ucCoveredRange : 1;

	ucLeftNeighborChannel = ucLeftestCoveredChannel ?
		ucLeftestCoveredChannel - 1 : 0;

	/* align leftest covered ch and left neighbor ch to valid 5g ch */
	if (bIs5GChl) {
		ucLeftestCoveredChannel += 2;
		ucLeftNeighborChannel -= 1;
	} else {
		/* we select the nearest 2 ch to the leftest covered ch as left neighbor chs */
		ucLeftNeighborChannel2 =
			ucLeftNeighborChannel > 1 ? ucLeftNeighborChannel - 1 : 0;
	}

	/* handle corner cases of 5g ch*/
	if (ucLeftestCoveredChannel > 14 && ucLeftestCoveredChannel <= 36) {
		ucLeftestCoveredChannel = 36;
		ucLeftNeighborChannel = 0;
	} else if (ucLeftestCoveredChannel > 64 && ucLeftestCoveredChannel <= 100) {
		ucLeftestCoveredChannel = 100;
		ucLeftNeighborChannel = 0;
	} else if (ucLeftestCoveredChannel > 140 && ucLeftestCoveredChannel <= 149) {
		ucLeftestCoveredChannel = 149;
		ucLeftNeighborChannel = 0;
	} else if (ucLeftestCoveredChannel > 173 && ucLeftestCoveredChannel <= 184) {
		ucLeftestCoveredChannel = 184;
		ucLeftNeighborChannel = 0;
	}

	/*
	  * because ch 14 is 12MHz away to ch13, we must shift the leftest covered ch and
	  * left neighbor ch when central ch is ch 14
	  */
	if (ucCentralChannel == 14) {
		ucLeftestCoveredChannel = 13;
		ucLeftNeighborChannel = 12;
		ucLeftNeighborChannel2 = 11;
	}

	ucRightestCoveredChannel = ucCentralChannel + ucCoveredRange;
	ucRightNeighborChannel = ucRightestCoveredChannel + 1;

	/* align rightest covered ch and right neighbor ch to valid 5g ch */
	if (bIs5GChl) {
		ucRightestCoveredChannel -= 2;
		ucRightNeighborChannel += 1;
	} else {
		/* we select the nearest 2 ch to the rightest covered ch as right neighbor ch */
		ucRightNeighborChannel2 =
			ucRightNeighborChannel < 13 ? ucRightNeighborChannel + 1 : 0;
	}

	/* handle corner cases */
	if (ucRightestCoveredChannel >= 14 && ucRightestCoveredChannel < 36) {
		if (ucRightestCoveredChannel == 14) {
			ucRightestCoveredChannel = 13;
			ucRightNeighborChannel = 14;
		} else {
			ucRightestCoveredChannel = 14;
			ucRightNeighborChannel = 0;
		}

		ucRightNeighborChannel2 = 0;
	} else if (ucRightestCoveredChannel >= 64 && ucRightestCoveredChannel < 100) {
		ucRightestCoveredChannel = 64;
		ucRightNeighborChannel = 0;
	} else if (ucRightestCoveredChannel >= 140 && ucRightestCoveredChannel < 149) {
		ucRightestCoveredChannel = 140;
		ucRightNeighborChannel = 0;
	} else if (ucRightestCoveredChannel >= 173 && ucRightestCoveredChannel < 184) {
		ucRightestCoveredChannel = 173;
		ucRightNeighborChannel = 0;
	}

	DBGLOG(SCN, TRACE, "central ch %d\n", ucCentralChannel);

	ucStart = wlanGetChannelIndex(ucLeftestCoveredChannel);
	ucEnd = wlanGetChannelIndex(ucRightestCoveredChannel);

	for (ucIdx = ucStart; ucIdx <= ucEnd; ucIdx++) {
		prGetChnLoad->rEachChnLoad[ucIdx].u4Dirtiness += u4Dirtiness;
		DBGLOG(SCN, TRACE, "Add dirtiness %d, to covered ch %d\n",
			u4Dirtiness, prGetChnLoad->rEachChnLoad[ucIdx].ucChannel);
	}

	if (ucLeftNeighborChannel != 0) {
		ucIdx = wlanGetChannelIndex(ucLeftNeighborChannel);
		prGetChnLoad->rEachChnLoad[ucIdx].u4Dirtiness += (u4Dirtiness >> 1);
		DBGLOG(SCN, TRACE, "Add dirtiness %d, to neighbor ch %d\n",
			u4Dirtiness >> 1, prGetChnLoad->rEachChnLoad[ucIdx].ucChannel);
	}

	if (ucRightNeighborChannel != 0) {
		ucIdx = wlanGetChannelIndex(ucRightNeighborChannel);
		prGetChnLoad->rEachChnLoad[ucIdx].u4Dirtiness += (u4Dirtiness >> 1);
		DBGLOG(SCN, TRACE, "Add dirtiness %d, to neighbor ch %d\n",
			u4Dirtiness >> 1, prGetChnLoad->rEachChnLoad[ucIdx].ucChannel);
	}

	if (!bIs5GChl) {
		if (ucLeftNeighborChannel2 != 0) {
			ucIdx = wlanGetChannelIndex(ucLeftNeighborChannel2);
			prGetChnLoad->rEachChnLoad[ucIdx].u4Dirtiness += (u4Dirtiness >> 1);
			DBGLOG(SCN, TRACE, "Add dirtiness %d, to neighbor ch %d\n",
				u4Dirtiness >> 1, prGetChnLoad->rEachChnLoad[ucIdx].ucChannel);
		}

		if (ucRightNeighborChannel2 != 0) {
			ucIdx = wlanGetChannelIndex(ucRightNeighborChannel2);
			prGetChnLoad->rEachChnLoad[ucIdx].u4Dirtiness += (u4Dirtiness >> 1);
			DBGLOG(SCN, TRACE, "Add dirtiness %d, to neighbor ch %d\n",
				u4Dirtiness >> 1, prGetChnLoad->rEachChnLoad[ucIdx].ucChannel);
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
static VOID
wlanCalculateChannelDirtiness(IN P_ADAPTER_T prAdapter,
	P_BSS_DESC_T prBssDesc, UINT_32 u4Dirtiness, BOOL bIsIndexOne)
{
	UINT_8 ucCoveredRange = 0, ucCentralChannel = 0, ucCentralChannel2 = 0;

	if (bIsIndexOne) {
		DBGLOG(SCN, TRACE, "Process dirtiness index 1\n");
		ucCentralChannel = prBssDesc->ucChannelNum;
		ucCoveredRange = 2;
	} else {
		DBGLOG(SCN, TRACE, "Process dirtiness index 2, ");
		switch (prBssDesc->eChannelWidth) {
		case CW_20_40MHZ:
			if (prBssDesc->eSco == CHNL_EXT_SCA) {
				DBGLOG(SCN, TRACE, "BW40\n");
				ucCentralChannel = prBssDesc->ucChannelNum + 2;
				ucCoveredRange = 4;
			} else if (prBssDesc->eSco == CHNL_EXT_SCB) {
				DBGLOG(SCN, TRACE, "BW40\n");
				ucCentralChannel = prBssDesc->ucChannelNum - 2;
				ucCoveredRange = 4;
			} else {
				DBGLOG(SCN, TRACE, "BW20\n");
				ucCentralChannel = prBssDesc->ucChannelNum;
				ucCoveredRange = 2;
			}
			break;
		case CW_80MHZ:
			DBGLOG(SCN, TRACE, "BW80\n");
			ucCentralChannel = prBssDesc->ucCenterFreqS1;
			ucCoveredRange = 8;
			break;
		case CW_160MHZ:
			DBGLOG(SCN, TRACE, "BW160\n");
			ucCentralChannel = prBssDesc->ucCenterFreqS1;
			ucCoveredRange = 16;
			break;
		case CW_80P80MHZ:
			DBGLOG(SCN, TRACE, "BW8080\n");
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

	wlanAddDirtinessToAffectedChannels(prAdapter, prBssDesc, u4Dirtiness,
		ucCentralChannel, ucCoveredRange);

	/* 80 + 80 secondary 80 case */
	if (bIsIndexOne || ucCentralChannel2 == 0)
		return;

	wlanAddDirtinessToAffectedChannels(prAdapter, prBssDesc, u4Dirtiness,
		ucCentralChannel2, ucCoveredRange);
}

VOID
wlanInitChnLoadInfoChannelList(IN P_ADAPTER_T prAdapter)
{
	UINT_8 ucIdx = 0;
	P_PARAM_GET_CHN_INFO prGetChnLoad = &(prAdapter->rWifiVar.rChnLoadInfo);

	for (ucIdx = 0; ucIdx < MAX_CHN_NUM; ucIdx++)
		prGetChnLoad->rEachChnLoad[ucIdx].ucChannel =
			wlanGetChannelNumFromIndex(ucIdx);
}

WLAN_STATUS
wlanCalculateAllChannelDirtiness(IN P_ADAPTER_T prAdapter)
{
	WLAN_STATUS rResult = WLAN_STATUS_SUCCESS;
	PARAM_RSSI i4Rssi = 0;
	P_BSS_DESC_T prBssDesc = NULL;
	UINT_32 u4Dirtiness = 0;
	P_LINK_T prBSSDescList = &(prAdapter->rWifiVar.rScanInfo.rBSSDescList);

	LINK_FOR_EACH_ENTRY(prBssDesc, prBSSDescList, rLinkEntry, BSS_DESC_T) {
		i4Rssi = RCPI_TO_dBm(prBssDesc->ucRCPI);

		if (i4Rssi >= ACS_AP_RSSI_LEVEL_HIGH)
			u4Dirtiness = ACS_DIRTINESS_LEVEL_HIGH;
		else if (i4Rssi >= ACS_AP_RSSI_LEVEL_LOW)
			u4Dirtiness = ACS_DIRTINESS_LEVEL_MID;
		else
			u4Dirtiness = ACS_DIRTINESS_LEVEL_LOW;

		DBGLOG(SCN, TRACE, "Found an AP(%s), primary ch %d\n",
			prBssDesc->aucSSID, prBssDesc->ucChannelNum);

		/* dirtiness index1 */
		wlanCalculateChannelDirtiness(prAdapter, prBssDesc, u4Dirtiness, TRUE);

		/* dirtiness index2 */
		wlanCalculateChannelDirtiness(prAdapter, prBssDesc,
			u4Dirtiness >> 1, FALSE);
	}

	return rResult;
}

UINT_8
wlanGetChannelIndex(IN UINT_8 channel)
{
	UINT_8 ucIdx = 1;

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

/*---------------------------------------------------------------------*/
/*!
* \brief Get ch index by the given ch num; the reverse function of
*        wlanGetChannelIndex
*
* \param[in]    ucIdx                 Channel index
* \param[out]   ucChannel             Channel number
*/
/*---------------------------------------------------------------------*/

UINT_8
wlanGetChannelNumFromIndex(IN UINT_8 ucIdx)
{
	UINT_8 ucChannel = 0;

	if (ucIdx >= 40)
		ucChannel = ((ucIdx - 40) << 2) + 184;
	else if (ucIdx >= 33)
		ucChannel = ((ucIdx - 33) << 2) + 149;
	else if (ucIdx >= 22)
		ucChannel = ((ucIdx - 22) << 2) + 100;
	else if (ucIdx >= 14)
		ucChannel = ((ucIdx - 14) << 2) + 36;
	else
		ucChannel = ucIdx + 1;

	return ucChannel;
}

VOID
wlanSortChannel(IN P_ADAPTER_T prAdapter)
{
	P_PARAM_GET_CHN_INFO prChnLoadInfo = &(prAdapter->rWifiVar.rChnLoadInfo);
	INT_8 ucIdx = 0, ucRoot = 0, ucChild = 0;
	PARAM_CHN_RANK_INFO rChnRankInfo;

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
			if (ucChild < MAX_CHN_NUM - 1 &&
				prChnLoadInfo->rChnRankList[ucChild + 1].u4Dirtiness >
				prChnLoadInfo->rChnRankList[ucChild].u4Dirtiness)
				ucChild += 1;

			if (prChnLoadInfo->rChnRankList[ucChild].u4Dirtiness <=
				prChnLoadInfo->rChnRankList[ucRoot].u4Dirtiness)
				break;

			rChnRankInfo = prChnLoadInfo->rChnRankList[ucChild];
			prChnLoadInfo->rChnRankList[ucChild] = prChnLoadInfo->rChnRankList[ucRoot];
			prChnLoadInfo->rChnRankList[ucRoot] = rChnRankInfo;
		}
	}

	/* sort ch rank list */
	for (ucIdx = MAX_CHN_NUM - 1; ucIdx > 0; ucIdx--) {
		rChnRankInfo = prChnLoadInfo->rChnRankList[0];
		prChnLoadInfo->rChnRankList[0] = prChnLoadInfo->rChnRankList[ucIdx];
		prChnLoadInfo->rChnRankList[ucIdx] = rChnRankInfo;

		for (ucRoot = 0; ucRoot * 2 + 1 < ucIdx; ucRoot = ucChild) {
			ucChild = ucRoot * 2 + 1;
			if (ucChild < ucIdx - 1 &&
				prChnLoadInfo->rChnRankList[ucChild + 1].u4Dirtiness >
				prChnLoadInfo->rChnRankList[ucChild].u4Dirtiness)
				ucChild += 1;

			if (prChnLoadInfo->rChnRankList[ucChild].u4Dirtiness <=
				prChnLoadInfo->rChnRankList[ucRoot].u4Dirtiness)
				break;

			rChnRankInfo = prChnLoadInfo->rChnRankList[ucChild];
			prChnLoadInfo->rChnRankList[ucChild] = prChnLoadInfo->rChnRankList[ucRoot];
			prChnLoadInfo->rChnRankList[ucRoot] = rChnRankInfo;
		}
	}

	for (ucIdx = 0; ucIdx < MAX_CHN_NUM; ++ucIdx)
		DBGLOG(SCN, INFO, "[ACS]channel=%d, dirtiness=%d\n",
			prChnLoadInfo->rChnRankList[ucIdx].ucChannel,
			prChnLoadInfo->rChnRankList[ucIdx].u4Dirtiness);

}
#endif

UINT_8  wlanGetAntPathType(
	IN P_ADAPTER_T prAdapter,
	IN enum ENUM_WF_PATH_FAVOR_T eWfPathFavor
	)
{
	UINT_8 ucFianlWfPathType = eWfPathFavor;
#if CFG_SUPPORT_ANT_SELECT
	UINT_8 ucNss = prAdapter->rWifiVar.ucNSS;
	UINT_8 ucSpeIdxCtrl = prAdapter->rWifiVar.ucSpeIdxCtrl;

	if (ucSpeIdxCtrl == 0)
		ucFianlWfPathType = ENUM_WF_0_ONE_STREAM_PATH_FAVOR;
	else if (ucSpeIdxCtrl == 1)
		ucFianlWfPathType = ENUM_WF_1_ONE_STREAM_PATH_FAVOR;
	else if (ucSpeIdxCtrl == 2) {
		if (ucNss > 1)
			ucFianlWfPathType = ENUM_WF_0_1_DUP_STREAM_PATH_FAVOR;
		else
			ucFianlWfPathType = ENUM_WF_NON_FAVOR;
	} else
		ucFianlWfPathType = ENUM_WF_NON_FAVOR;
#endif
	return ucFianlWfPathType;
}

#if ((CFG_SISO_SW_DEVELOP == 1) || (CFG_SUPPORT_ANT_SELECT == 1))
UINT_8
wlanAntPathFavorSelect(
	enum ENUM_WF_PATH_FAVOR_T eWfPathFavor
	)
{
	UINT_8 ucRetValSpeIdx = 0;

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

	return ucRetValSpeIdx;
}
#endif

UINT_8
wlanGetSpeIdx(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex, IN enum ENUM_WF_PATH_FAVOR_T eWfPathFavor)
{
	UINT_8 ucRetValSpeIdx = 0;
#if ((CFG_SISO_SW_DEVELOP == 1) || (CFG_SUPPORT_ANT_SELECT == 1))
	P_BSS_INFO_T prBssInfo;
	ENUM_BAND_T eBand = BAND_NULL;

	if (ucBssIndex > MAX_BSS_INDEX) {
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
			} else
				ucRetValSpeIdx = wlanAntPathFavorSelect(eWfPathFavor);
		} else if (eBand == BAND_5G) {
			if (IS_WIFI_5G_SISO(prAdapter)) {
				if (IS_WIFI_5G_WF0_SUPPORT(prAdapter))
					ucRetValSpeIdx = ANTENNA_WF0;
				else
					ucRetValSpeIdx = ANTENNA_WF1;
			} else
				ucRetValSpeIdx = wlanAntPathFavorSelect(eWfPathFavor);
		} else
			ucRetValSpeIdx = wlanAntPathFavorSelect(eWfPathFavor);
	}
	DBGLOG(INIT, INFO, "SpeIdx:%d,D:%d,G=%d,B=%d,Bss=%d\n",
						ucRetValSpeIdx, prAdapter->rWifiVar.fgDbDcModeEn,
						prBssInfo->fgIsGranted, eBand, ucBssIndex);
#endif
	return ucRetValSpeIdx;
}

UINT_8
wlanGetSupportNss(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex)
{
	UINT_8 ucRetValNss = prAdapter->rWifiVar.ucNSS;
#if CFG_SISO_SW_DEVELOP
	P_BSS_INFO_T prBssInfo;
	ENUM_BAND_T eBand = BAND_NULL;

	if (ucBssIndex > MAX_BSS_INDEX) {
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

INT_32 wlanGetFileContent(P_ADAPTER_T prAdapter,
	const PUINT_8 pcFileName, PUINT_8 pucBuf,
	UINT_32 u4MaxFileLen, PUINT_32 pu4ReadFileLen, BOOL bReqFw)
{
	if (bReqFw)
		return kalRequestFirmware(pcFileName, pucBuf,
				 u4MaxFileLen, pu4ReadFileLen,
				 prAdapter->prGlueInfo->prDev);

	return kalReadToFile(pcFileName, pucBuf,
				u4MaxFileLen, pu4ReadFileLen);
}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is to update some info before connected,
*        some decision need some info before bss info update
*
* @param prAdapter      Pointer of Adapter Data Structure
*
* @return WLAN_STATUS_SUCCESS
*/
/*----------------------------------------------------------------------------*/
#if CFG_SUPPORT_ANT_SELECT
WLAN_STATUS wlanUpdateExtInfo(IN P_ADAPTER_T prAdapter)
{
	UINT_32 u4BufLen = 0;
	P_GLUE_INFO_T prGlueInfo = prAdapter->prGlueInfo;
	PARAM_CUSTOM_SW_CTRL_STRUCT_T rSwCtrlInfo;

	rSwCtrlInfo.u4Id = 0xa0640001;
	rSwCtrlInfo.u4Data = prAdapter->rWifiVar.ucNSS;

	return kalIoctl(prGlueInfo,
					wlanoidSetSwCtrlWrite,
					&rSwCtrlInfo, sizeof(rSwCtrlInfo),
					FALSE, FALSE, TRUE, &u4BufLen);
}
#endif

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is a wrapper to send eapol offload (rekey) command
*
* @param prGlueInfo                     Pointer of prGlueInfo Data Structure
*
* @return VOID
*/
/*----------------------------------------------------------------------------*/
int wlanSuspendRekeyOffload(P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucRekeyMode)
{
	UINT_32 u4BufLen;
	P_PARAM_GTK_REKEY_DATA prGtkData;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	INT_32 i4Rslt = -EINVAL;
#if CFG_SUPPORT_REPLAY_DETECTION
	P_BSS_INFO_T prBssInfo = NULL;
	struct SEC_DETECT_REPLAY_INFO *prDetRplyInfo = NULL;
	UINT_8 ucCurKeyId;
	UINT_8 ucRpyOffload;
#endif

	ASSERT(prGlueInfo);

#if CFG_SUPPORT_REPLAY_DETECTION
	ucRpyOffload = prGlueInfo->prAdapter->rWifiVar.ucRpyDetectOffload;

	if ((ucRekeyMode == GTK_REKEY_CMD_MODE_SET_BCMC_PN) &&
		(ucRpyOffload == FALSE)) {
		DBGLOG(RSN, INFO, "Set PN to fw, but feature off. no action\n");
		return WLAN_STATUS_SUCCESS;
	}

	if ((ucRekeyMode == GTK_REKEY_CMD_MODE_GET_BCMC_PN) &&
		(ucRpyOffload == FALSE)) {
		DBGLOG(RSN, INFO, "Get PN from fw, but feature off. no action\n");
		return WLAN_STATUS_SUCCESS;
	}
#endif

	prGtkData =
		(P_PARAM_GTK_REKEY_DATA) kalMemAlloc(sizeof(PARAM_GTK_REKEY_DATA), VIR_MEM_TYPE);

	if (!prGtkData)
		return WLAN_STATUS_SUCCESS;

	kalMemZero(prGtkData, sizeof(PARAM_GTK_REKEY_DATA));

	/* if enable => enable FW rekey offload. if disable, let rekey back to supplicant */
	prGtkData->ucRekeyMode = ucRekeyMode;
	DBGLOG(RSN, INFO, "GTK Rekey ucRekeyMode = %d\n", ucRekeyMode);

	if (ucRekeyMode == GTK_REKEY_CMD_MODE_OFFLOAD_ON) {
		DBGLOG(RSN, INFO, "kek\n");
		DBGLOG_MEM8(RSN, ERROR, (PUINT_8)prGlueInfo->rWpaInfo.aucKek, NL80211_KEK_LEN);
		DBGLOG(RSN, INFO, "kck\n");
		DBGLOG_MEM8(RSN, ERROR, (PUINT_8)prGlueInfo->rWpaInfo.aucKck, NL80211_KCK_LEN);
		DBGLOG(RSN, INFO, "replay count\n");
		DBGLOG_MEM8(RSN, ERROR, (PUINT_8)prGlueInfo->rWpaInfo.aucReplayCtr, NL80211_REPLAY_CTR_LEN);

		kalMemCopy(prGtkData->aucKek, prGlueInfo->rWpaInfo.aucKek, NL80211_KEK_LEN);
		kalMemCopy(prGtkData->aucKck, prGlueInfo->rWpaInfo.aucKck, NL80211_KCK_LEN);
		kalMemCopy(prGtkData->aucReplayCtr, prGlueInfo->rWpaInfo.aucReplayCtr, NL80211_REPLAY_CTR_LEN);

		prGtkData->ucBssIndex = prGlueInfo->prAdapter->prAisBssInfo->ucBssIndex;

		prGtkData->u4Proto = NL80211_WPA_VERSION_2;
		if (prGlueInfo->rWpaInfo.u4WpaVersion == IW_AUTH_WPA_VERSION_WPA)
			prGtkData->u4Proto = NL80211_WPA_VERSION_1;

		if (prGlueInfo->rWpaInfo.u4CipherPairwise == IW_AUTH_CIPHER_TKIP)
			prGtkData->u4PairwiseCipher = BIT(3);
		else if (prGlueInfo->rWpaInfo.u4CipherPairwise == IW_AUTH_CIPHER_CCMP)
			prGtkData->u4PairwiseCipher = BIT(4);
		else {
			kalMemFree(prGtkData, VIR_MEM_TYPE, sizeof(PARAM_GTK_REKEY_DATA));
			return WLAN_STATUS_SUCCESS;
		}

		if (prGlueInfo->rWpaInfo.u4CipherGroup == IW_AUTH_CIPHER_TKIP)
			prGtkData->u4GroupCipher    = BIT(3);
		else if (prGlueInfo->rWpaInfo.u4CipherGroup == IW_AUTH_CIPHER_CCMP)
			prGtkData->u4GroupCipher    = BIT(4);
		else {
			kalMemFree(prGtkData, VIR_MEM_TYPE, sizeof(PARAM_GTK_REKEY_DATA));
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
		DBGLOG(RSN, INFO, "ucRekeyMode(rpy rekey offload on): %d\n", ucRekeyMode);

	if (ucRekeyMode == GTK_REKEY_CMD_MODE_RPY_OFFLOAD_OFF)
		DBGLOG(RSN, INFO, "ucRekeyMode(rpy rekey offload off): %d\n", ucRekeyMode);

	if ((ucRekeyMode == GTK_REKEY_CMD_MODE_SET_BCMC_PN) &&
		(ucRpyOffload == TRUE)) {
		prGtkData->ucBssIndex = prGlueInfo->prAdapter->prAisBssInfo->ucBssIndex;
		prBssInfo = GET_BSS_INFO_BY_INDEX(prGlueInfo->prAdapter,
			prGlueInfo->prAdapter->prAisBssInfo->ucBssIndex);
		prDetRplyInfo = &prBssInfo->rDetRplyInfo;
		ucCurKeyId = prDetRplyInfo->ucCurKeyId;
		prGtkData->ucCurKeyId = ucCurKeyId;
		DBGLOG_MEM8(RSN, INFO, (PUINT_8)prGtkData->aucReplayCtr, NL80211_REPLAY_CTR_LEN);
		kalMemCopy(prGtkData->aucReplayCtr,
			prDetRplyInfo->arReplayPNInfo[ucCurKeyId].auPN,
			NL80211_REPLAY_CTR_LEN);

		/* set bc/mc PN zero before suspend */
		kalMemZero(prDetRplyInfo->arReplayPNInfo[ucCurKeyId].auPN, NL80211_REPLAY_CTR_LEN);
	}

	if ((ucRekeyMode == GTK_REKEY_CMD_MODE_GET_BCMC_PN) &&
		(ucRpyOffload == TRUE)) {
		prGtkData->ucBssIndex = prGlueInfo->prAdapter->prAisBssInfo->ucBssIndex;
	}
#endif

	rStatus = kalIoctl(prGlueInfo,
				wlanoidSetGtkRekeyData,
				prGtkData, sizeof(PARAM_GTK_REKEY_DATA), FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, INFO, "Suspend GTK rekey data error:%x\n",
				rStatus);
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
VOID wlanSuspendPmHandle(P_GLUE_INFO_T prGlueInfo)
{
	UINT_8 idx;
	PARAM_POWER_MODE ePwrMode;
	P_BSS_INFO_T prBssInfo;
	UINT_8 ucKekZeroCnt = 0;
	UINT_8 ucKckZeroCnt = 0;
	UINT_8 ucGtkOffload = TRUE;
	UINT_8 i = 0;
#if CFG_SUPPORT_REPLAY_DETECTION
	struct SEC_DETECT_REPLAY_INFO *prDetRplyInfo = NULL;
	UINT_8 ucIdx = 0;
	UINT_8 ucKeyIdx = 0;
	UINT_8 ucRpyOffload = 0;
#endif
	P_STA_RECORD_T prStaRec;
	P_RX_BA_ENTRY_T prRxBaEntry;

	if (prGlueInfo->prAdapter->u4IsKeepFullPwrBitmap)
		wlanKeepFullPwr(prGlueInfo->prAdapter, FALSE);

	/* if wifi.cfg EAPOL offload is 0, we set rekey offload when enter wow */
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
			DBGLOG(RSN, INFO, "no rekey offload, due to no KCK/KEK from cfg80211\n");

			prGlueInfo->prAdapter->rWifiVar.ucRpyDetectOffload = FEATURE_DISABLED;

			ucGtkOffload = FALSE;
			/* set bc/mc replay detection off to fw */
			wlanSuspendRekeyOffload(prGlueInfo,
				GTK_REKEY_CMD_MODE_RPY_OFFLOAD_OFF);
		}


#if CFG_SUPPORT_REPLAY_DETECTION
		ucRpyOffload = prGlueInfo->prAdapter->rWifiVar.ucRpyDetectOffload;

		if (ucRpyOffload && ucGtkOffload)
			wlanSuspendRekeyOffload(prGlueInfo, GTK_REKEY_CMD_MODE_SET_BCMC_PN);
#endif
		if (ucGtkOffload)
			wlanSuspendRekeyOffload(prGlueInfo, GTK_REKEY_CMD_MODE_OFFLOAD_ON);

#if CFG_SUPPORT_REPLAY_DETECTION

		prBssInfo = GET_BSS_INFO_BY_INDEX(prGlueInfo->prAdapter,
			prGlueInfo->prAdapter->prAisBssInfo->ucBssIndex);
		prDetRplyInfo = &prBssInfo->rDetRplyInfo;

		for (ucKeyIdx = 0; ucKeyIdx < 4; ucKeyIdx++) {
			for (ucIdx = 0; ucIdx < 6; ucIdx++)
				prDetRplyInfo->arReplayPNInfo[ucKeyIdx].auPN[ucIdx] = 0x0;
		}
#endif
		DBGLOG(HAL, STATE, "Suspend rekey offload\n");
	}


	/* Abort Obss scan if the scan FSM is not IDLE for all HIF*/
	rlmObssAbortScan(prGlueInfo->prAdapter);

	/* Pending Timer related to CNM need to check and
	 * perform corresponding timeout handler. Without it,
	 * Might happen CNM abnormal after resume or during suspend.
	 */
	cnmCheckPendingTimer(prGlueInfo->prAdapter);

	/* 1) wifi cfg "Wow" is true, 2) wow or AdvPws is enable
	 * 3) WIfI connected => execute WOW flow
	 * Send power-saving cmd when enter wow state, even w/o cfg80211 support
     */
	if (prGlueInfo->prAdapter->rWifiVar.ucWow
		&& (prGlueInfo->prAdapter->rWowCtrl.fgWowEnable
			|| prGlueInfo->prAdapter->rWifiVar.ucAdvPws)) {
		if (kalGetMediaStateIndicated(prGlueInfo) == PARAM_MEDIA_STATE_CONNECTED) {
			/* AIS bss enter wow power mode, default fast power-saving */
			ePwrMode = prGlueInfo->prAdapter->rWifiVar.ucWowPwsMode;
			idx = prGlueInfo->prAdapter->prAisBssInfo->ucBssIndex;
			nicConfigPowerSaveWowProfile(prGlueInfo->prAdapter, idx, ePwrMode, FALSE, TRUE);
			DBGLOG(HAL, STATE, "Suspend wow power save idx:%d, mode:%d\n", idx, ePwrMode);

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
* @brief This function is to restore power-saving mode command when AIS leave wow
*        But ignore GC/GO/AP role
*
* @param prGlueInfo                     Pointer of prGlueInfo Data Structure
*
* @return VOID
*/
/*----------------------------------------------------------------------------*/
VOID wlanResumePmHandle(P_GLUE_INFO_T prGlueInfo)
{
	PARAM_POWER_MODE ePwrMode = Param_PowerModeCAM;
	UINT_8 ucKekZeroCnt = 0;
	UINT_8 ucKckZeroCnt = 0;
	UINT_8 ucGtkOffload = TRUE;
	UINT_8 i = 0;
#if CFG_SUPPORT_REPLAY_DETECTION
	struct SEC_DETECT_REPLAY_INFO *prDetRplyInfo = NULL;
	P_BSS_INFO_T prBssInfo = NULL;
	UINT_8 ucIdx = 0;
	UINT_8 ucKeyIdx = 0;
	UINT_8 ucRpyOffload = 0;
#endif

	/* if wifi.cfg EAPOL offload is disble, we disable FW offload when leave wow */
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

			DBGLOG(RSN, INFO, "no rekey offload, due to no KCK/KEK from cfg80211\n");

			prGlueInfo->prAdapter->rWifiVar.ucRpyDetectOffload = FEATURE_DISABLED;

			ucGtkOffload = FALSE;
			/* set bc/mc replay detection off to fw */
			wlanSuspendRekeyOffload(prGlueInfo,
				GTK_REKEY_CMD_MODE_RPY_OFFLOAD_OFF);

		}

#if CFG_SUPPORT_REPLAY_DETECTION
		prBssInfo = GET_BSS_INFO_BY_INDEX(prGlueInfo->prAdapter,
			prGlueInfo->prAdapter->prAisBssInfo->ucBssIndex);
		prDetRplyInfo = &prBssInfo->rDetRplyInfo;

		/* as resume performed, reset BC/MC KeyRSC, to prevent incorrect replay detection. */
		for (ucKeyIdx = 0; ucKeyIdx < 4; ucKeyIdx++) {
			for (ucIdx = 0; ucIdx < NL80211_KEYRSC_LEN; ucIdx++)
				prDetRplyInfo->arReplayPNInfo[ucKeyIdx].auPN[ucIdx] = 0x0;
		}

		ucRpyOffload = prGlueInfo->prAdapter->rWifiVar.ucRpyDetectOffload;

		/* sync BC/MC PN */
		if (ucRpyOffload && ucGtkOffload)
			wlanSuspendRekeyOffload(prGlueInfo, GTK_REKEY_CMD_MODE_GET_BCMC_PN);
#endif

		if (ucGtkOffload) {
			wlanSuspendRekeyOffload(prGlueInfo, GTK_REKEY_CMD_MODE_OFLOAD_OFF);
		DBGLOG(HAL, STATE, "Resume rekey offload disable\n");
	}
	}

	if (prGlueInfo->prAdapter->rWifiVar.ucWow
		&& (prGlueInfo->prAdapter->rWowCtrl.fgWowEnable
			|| prGlueInfo->prAdapter->rWifiVar.ucAdvPws)) {
		if (kalGetMediaStateIndicated(prGlueInfo) == PARAM_MEDIA_STATE_CONNECTED) {
			DBGLOG(HAL, STATE, "leave WOW flow. AIS BssIndex:%d\n",
				prGlueInfo->prAdapter->prAisBssInfo->ucBssIndex);
			kalWowProcess(prGlueInfo, FALSE);

			/* resume AIS power-saving cmd when leave wow state, ignore ePwrMode input */
			nicConfigPowerSaveWowProfile(prGlueInfo->prAdapter,
				prGlueInfo->prAdapter->prAisBssInfo->ucBssIndex, ePwrMode, FALSE, FALSE);
		}
	}

	if (prGlueInfo->prAdapter->u4IsKeepFullPwrBitmap)
		wlanKeepFullPwr(prGlueInfo->prAdapter, TRUE);
}

void disconnect_sta(P_ADAPTER_T prAdapter, P_STA_RECORD_T sta_rec)
{
	P_GLUE_INFO_T glue_info;
	P_MSG_AIS_ABORT_T ais_abort_msg = NULL;
	P_MSG_P2P_CONNECTION_ABORT_T p2p_abot_msg = NULL;
	P_BSS_INFO_T p2p_bss_info = NULL;
	unsigned char role_idx = 0;


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
			(P_MSG_AIS_ABORT_T)cnmMemAlloc(prAdapter,
						       RAM_TYPE_MSG,
						       sizeof(MSG_AIS_ABORT_T));
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
				    (P_MSG_HDR_T) ais_abort_msg,
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
			(P_MSG_P2P_CONNECTION_ABORT_T)
			cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
				    sizeof(MSG_P2P_CONNECTION_ABORT_T));

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
				    (P_MSG_HDR_T)p2p_abot_msg,
				    MSG_SEND_METHOD_UNBUF);
		}
		break;
	default:
		break;
	}
}

#if CFG_SUPPORT_CSI
bool wlanPushCSIData(P_ADAPTER_T prAdapter, struct CSI_DATA_T *prCSIData)
{
	struct CSI_INFO_T *prCSIInfo = &(prAdapter->rCSIInfo);

	KAL_ACQUIRE_MUTEX(prAdapter, MUTEX_CSI_BUFFER);

	/* Put the CSI data into CSI event queue */
	if (prCSIInfo->u4CSIBufferUsed != 0) {
		prCSIInfo->u4CSIBufferTail++;
		prCSIInfo->u4CSIBufferTail %= CSI_RING_SIZE;
	}

	kalMemCopy(&(prCSIInfo->arCSIBuffer[prCSIInfo->u4CSIBufferTail]),
		prCSIData, sizeof(struct CSI_DATA_T));

	if (prCSIInfo->u4CSIBufferUsed < CSI_RING_SIZE) {
		prCSIInfo->u4CSIBufferUsed++;
	} else {
		/*
		 * While new CSI event comes and the ring buffer is
		 * already full, the new coming CSI event will
		 * overwrite the oldest one in the ring buffer.
		 * Thus, the Head pointer which points to * the
		 * oldest CSI event in the buffer should be moved too.
		 */
		prCSIInfo->u4CSIBufferHead++;
		prCSIInfo->u4CSIBufferHead %= CSI_RING_SIZE;
	}

	KAL_RELEASE_MUTEX(prAdapter, MUTEX_CSI_BUFFER);

	return TRUE;
}

bool wlanPopCSIData(P_ADAPTER_T prAdapter, struct CSI_DATA_T *prCSIData)
{
	struct CSI_INFO_T *prCSIInfo = &(prAdapter->rCSIInfo);

	KAL_ACQUIRE_MUTEX(prAdapter, MUTEX_CSI_BUFFER);

	/* No CSI data in the ring buffer */
	if (prCSIInfo->u4CSIBufferUsed == 0) {
		KAL_RELEASE_MUTEX(prAdapter, MUTEX_CSI_BUFFER);
		return FALSE;
	}

	kalMemCopy(prCSIData,
		&(prCSIInfo->arCSIBuffer[prCSIInfo->u4CSIBufferHead]),
		sizeof(struct CSI_DATA_T));

	prCSIInfo->u4CSIBufferUsed--;
	if (prCSIInfo->u4CSIBufferUsed != 0) {
		prCSIInfo->u4CSIBufferHead++;
		prCSIInfo->u4CSIBufferHead %= CSI_RING_SIZE;
	}
	KAL_RELEASE_MUTEX(prAdapter, MUTEX_CSI_BUFFER);

	return TRUE;
}

VOID
wlanApplyCSIToneMask(
	UINT_8 ucRxMode,
	UINT_8 ucCBW,
	UINT_8 ucDBW,
	UINT_8 ucPrimaryChIdx,
	INT_16 *ai2IData,
	INT_16 *ai2QData)
{
	UINT_8 ucSize = sizeof(INT_16);

#define ZERO(index) \
{ ai2IData[index] = 0; ai2QData[index] = 0; }

#define ZERO_RANGE(start, end) \
{\
	kalMemZero(&ai2IData[start], ucSize * (end - start + 1));\
	kalMemZero(&ai2QData[start], ucSize * (end - start + 1));\
}

	if (ucRxMode == RX_VT_LEGACY_OFDM) {
		if (ucCBW == RX_VT_FR_MODE_20) {
			ZERO(0);
			ZERO_RANGE(27, 37);
		} else if (ucCBW == RX_VT_FR_MODE_40) {
			if (ucDBW == RX_VT_FR_MODE_40) {
				ZERO(32); ZERO(96);
				ZERO_RANGE(0, 5);
				ZERO_RANGE(59, 69);
				ZERO_RANGE(123, 127);
			} else if (ucDBW == RX_VT_FR_MODE_20) {
				if (ucPrimaryChIdx == 0) {
					ZERO(96);
					ZERO_RANGE(0, 69);
					ZERO_RANGE(123, 127);
				} else {
					ZERO(32);
					ZERO_RANGE(0, 5);
					ZERO_RANGE(59, 127);
				}
			}
		} else if (ucCBW == RX_VT_FR_MODE_80) {
			if (ucDBW == RX_VT_FR_MODE_80) {
				ZERO(32); ZERO(96);
				ZERO(160); ZERO(224);
				ZERO_RANGE(0, 5);
				ZERO_RANGE(59, 69);
				ZERO_RANGE(123, 133);
				ZERO_RANGE(187, 197);
				ZERO_RANGE(251, 255);
			} else if (ucDBW == RX_VT_FR_MODE_40) {
				if (ucPrimaryChIdx <= 1) {
					ZERO(160); ZERO(224);
					ZERO_RANGE(0, 133);
					ZERO_RANGE(187, 197);
					ZERO_RANGE(251, 255);
				} else {
					ZERO(32); ZERO(96);
					ZERO_RANGE(0, 5);
					ZERO_RANGE(59, 69);
					ZERO_RANGE(123, 255);
				}
			} else if (ucDBW == RX_VT_FR_MODE_20) {
				if (ucPrimaryChIdx == 0) {
					ZERO(160);
					ZERO_RANGE(0, 133);
					ZERO_RANGE(187, 255);
				} else if (ucPrimaryChIdx == 1) {
					ZERO(224);
					ZERO_RANGE(0, 197);
					ZERO_RANGE(251, 255);
				} else if (ucPrimaryChIdx == 2) {
					ZERO(32);
					ZERO_RANGE(0, 5);
					ZERO_RANGE(59, 255);
				} else {
					ZERO(96);
					ZERO_RANGE(0, 69);
					ZERO_RANGE(123, 255);
				}
			}
		}
	} else if (ucRxMode == RX_VT_MIXED_MODE ||
		ucRxMode == RX_VT_GREEN_MODE ||
		ucRxMode == RX_VT_VHT_MODE) {
		if (ucCBW == RX_VT_FR_MODE_20) {
			ZERO(0);
			ZERO_RANGE(29, 35);
		} else if (ucCBW == RX_VT_FR_MODE_40) {
			if (ucDBW == RX_VT_FR_MODE_40) {
				ZERO(0); ZERO(1); ZERO(127);
				ZERO_RANGE(59, 69);
			} else if (ucDBW == RX_VT_FR_MODE_20) {
				if (ucPrimaryChIdx == 0) {
					ZERO(96);
					ZERO_RANGE(0, 67);
					ZERO_RANGE(125, 127);
				} else {
					ZERO(32);
					ZERO_RANGE(0, 3);
					ZERO_RANGE(61, 127);
				}
			}
		} else if (ucCBW == RX_VT_FR_MODE_80) {
			if (ucDBW == RX_VT_FR_MODE_80) {
				ZERO(0); ZERO(1); ZERO(255);
				ZERO_RANGE(123, 133);
			} else if (ucDBW == RX_VT_FR_MODE_40) {
				if (ucPrimaryChIdx <= 1) {
					ZERO_RANGE(0, 133);
					ZERO_RANGE(191, 193);
					ZERO_RANGE(251, 255);
				} else {
					ZERO_RANGE(0, 5);
					ZERO_RANGE(63, 65);
					ZERO_RANGE(123, 127);
				}
			} else if (ucDBW == RX_VT_FR_MODE_20) {
				if (ucPrimaryChIdx == 0) {
					ZERO(160);
					ZERO_RANGE(0, 131);
					ZERO_RANGE(189, 255);
				} else if (ucPrimaryChIdx == 1) {
					ZERO(224);
					ZERO_RANGE(0, 195);
					ZERO_RANGE(253, 255);
				} else if (ucPrimaryChIdx == 2) {
					ZERO(32);
					ZERO_RANGE(0, 3);
					ZERO_RANGE(61, 255);
				} else {
					ZERO(96);
					ZERO_RANGE(0, 67);
					ZERO_RANGE(125, 255);
				}
			}
		}
	}

	/* Mask the VHT Pilots */
	if (ucRxMode == RX_VT_VHT_MODE) {
		if (ucCBW == RX_VT_FR_MODE_20) {
			ZERO(7); ZERO(21); ZERO(43); ZERO(57);
		} else if (ucCBW == RX_VT_FR_MODE_40) {
			if (ucDBW == RX_VT_FR_MODE_40) {
				ZERO(11); ZERO(25); ZERO(53);
				ZERO(75); ZERO(103); ZERO(117);
			} else if (ucDBW == RX_VT_FR_MODE_20) {
				if (ucPrimaryChIdx == 0) {
					ZERO(75); ZERO(89);
					ZERO(103); ZERO(117);
				} else {
					ZERO(11); ZERO(25);
					ZERO(39); ZERO(53);
				}
			}
		} else if (ucCBW == RX_VT_FR_MODE_80) {
			if (ucDBW == RX_VT_FR_MODE_80) {
				ZERO(11); ZERO(39); ZERO(75); ZERO(103);
				ZERO(153); ZERO(181); ZERO(217); ZERO(245);
			} else if (ucDBW == RX_VT_FR_MODE_40) {
				if (ucPrimaryChIdx <= 1) {
					ZERO(139); ZERO(167); ZERO(181);
					ZERO(203); ZERO(217); ZERO(245);
				} else {
					ZERO(11); ZERO(39); ZERO(53);
					ZERO(75); ZERO(89); ZERO(117);
				}
			} else if (ucDBW == RX_VT_FR_MODE_20) {
				if (ucPrimaryChIdx == 0) {
					ZERO(139); ZERO(153);
					ZERO(167); ZERO(181);
				} else if (ucPrimaryChIdx == 1) {
					ZERO(203); ZERO(217);
					ZERO(231); ZERO(245);
				} else if (ucPrimaryChIdx == 2) {
					ZERO(11); ZERO(25);
					ZERO(39); ZERO(53);
				} else {
					ZERO(75); ZERO(89);
					ZERO(103); ZERO(117);
				}
			}
		}
	}
}


VOID
wlanShiftCSI(
	UINT_8 ucRxMode,
	UINT_8 ucCBW,
	UINT_8 ucDBW,
	UINT_8 ucPrimaryChIdx,
	INT_16 *ai2IData,
	INT_16 *ai2QData,
	INT_16 *ai2ShiftIData,
	INT_16 *ai2ShiftQData)
{
	UINT_8 ucSize = sizeof(INT_16);
#define COPY_RANGE(dest, start, end) \
{\
	kalMemCopy(&ai2ShiftIData[dest], \
		&ai2IData[start], ucSize * (end - start + 1)); \
	kalMemCopy(&ai2ShiftQData[dest], \
		&ai2QData[start], ucSize * (end - start + 1)); \
}

#define COPY(dest, src) \
{ ai2ShiftIData[dest] = ai2IData[src]; ai2ShiftQData[dest] = ai2QData[src]; }

	if (ucRxMode == RX_VT_LEGACY_OFDM) {
		if (ucCBW == RX_VT_FR_MODE_20) {
			COPY_RANGE(0, 0, 63);
		} else if (ucCBW == RX_VT_FR_MODE_40) {
			if (ucDBW == RX_VT_FR_MODE_40) {
				COPY_RANGE(0, 0, 127);
			} else if (ucDBW == RX_VT_FR_MODE_20) {
				if (ucPrimaryChIdx == 0) {
					COPY(0, 96);
					COPY_RANGE(38, 70, 95);
					COPY_RANGE(1, 97, 122);
				} else {
					COPY(0, 32);
					COPY_RANGE(38, 6, 31);
					COPY_RANGE(1, 33, 58);
				}
			}
		} else if (ucCBW == RX_VT_FR_MODE_80) {
			if (ucDBW == RX_VT_FR_MODE_80) {
				COPY_RANGE(0, 0, 255);
			} else if (ucDBW == RX_VT_FR_MODE_40) {
				if (ucPrimaryChIdx <= 1) {
					COPY(0, 192);
					COPY_RANGE(2, 198, 250);
					COPY_RANGE(74, 134, 186);
				} else {
					COPY(0, 64);
					COPY_RANGE(2, 70, 122);
					COPY_RANGE(74, 6, 58);
				}
			} else if (ucDBW == RX_VT_FR_MODE_20) {
				if (ucPrimaryChIdx == 0) {
					COPY(0, 160);
					COPY_RANGE(1, 161, 186);
					COPY_RANGE(38, 134, 159);
				} else if (ucPrimaryChIdx == 1) {
					COPY(0, 224);
					COPY_RANGE(1, 225, 250);
					COPY_RANGE(38, 198, 223);
				} else if (ucPrimaryChIdx == 2) {
					COPY(0, 32);
					COPY_RANGE(1, 33, 58);
					COPY_RANGE(38, 6, 31);
				} else {
					COPY(0, 96);
					COPY_RANGE(1, 97, 122);
					COPY_RANGE(38, 70, 95);
				}
			}
		}
	} else if (ucRxMode == RX_VT_MIXED_MODE ||
		ucRxMode == RX_VT_GREEN_MODE ||
		ucRxMode == RX_VT_VHT_MODE) {
		if (ucCBW == RX_VT_FR_MODE_20) {
			COPY_RANGE(0, 0, 63);
		} else if (ucCBW == RX_VT_FR_MODE_40) {
			if (ucDBW == RX_VT_FR_MODE_40) {
				COPY_RANGE(0, 0, 127);
			} else if (ucDBW == RX_VT_FR_MODE_20) {
				if (ucPrimaryChIdx == 0) {
					COPY(0, 96);
					COPY_RANGE(36, 68, 95);
					COPY_RANGE(1, 97, 124);
				} else {
					COPY(0, 32);
					COPY_RANGE(36, 4, 31);
					COPY_RANGE(1, 33, 60);
				}
			}
		} else if (ucCBW == RX_VT_FR_MODE_80) {
			if (ucDBW == RX_VT_FR_MODE_80) {
				COPY_RANGE(0, 0, 255);
			} else if (ucDBW == RX_VT_FR_MODE_40) {
				if (ucPrimaryChIdx <= 1) {
					COPY(0, 192);
					COPY_RANGE(2, 194, 250);
					COPY_RANGE(70, 134, 190);
				} else {
					COPY(0, 64);
					COPY_RANGE(2, 66, 122);
					COPY_RANGE(70, 6, 62);
				}
			} else if (ucDBW == RX_VT_FR_MODE_20) {
				if (ucPrimaryChIdx == 0) {
					COPY(0, 160);
					COPY_RANGE(1, 161, 188);
					COPY_RANGE(36, 132, 159);
				} else if (ucPrimaryChIdx == 1) {
					COPY(0, 224);
					COPY_RANGE(1, 225, 252);
					COPY_RANGE(36, 196, 223);
				} else if (ucPrimaryChIdx == 2) {
					COPY(0, 32);
					COPY_RANGE(1, 33, 60);
					COPY_RANGE(36, 4, 31);
				} else {
					COPY(0, 96);
					COPY_RANGE(1, 97, 124);
					COPY_RANGE(36, 68, 95);
				}
			}
		}
	}
}
#endif

