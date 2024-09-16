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

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to process the POWER ON procedure.
*
* \param[in] pvAdapter Pointer to the Adapter structure.
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID nicpmSetFWOwn(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgEnableGlobalInt)
{
	UINT_32 u4RegValue = 0;

	ASSERT(prAdapter);

	if (prAdapter->fgIsFwOwn == TRUE)
		return;

	if (nicProcessIST(prAdapter) != WLAN_STATUS_NOT_INDICATING) {
		/* pending interrupts */
		return;
	}

	if (fgEnableGlobalInt) {
		prAdapter->fgIsIntEnableWithLPOwnSet = TRUE;
	} else {
		HAL_MCR_WR(prAdapter, MCR_WHLPCR, WHLPCR_FW_OWN_REQ_SET);

		HAL_MCR_RD(prAdapter, MCR_WHLPCR, &u4RegValue);
		if (u4RegValue & WHLPCR_FW_OWN_REQ_SET) {
			/* if set firmware own not successful (possibly pending interrupts), */
			/* indicate an own clear event */
			HAL_MCR_WR(prAdapter, MCR_WHLPCR, WHLPCR_FW_OWN_REQ_CLR);

			return;
		}

		prAdapter->fgIsFwOwn = TRUE;
	}
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to process the POWER OFF procedure.
*
* \param[in] pvAdapter Pointer to the Adapter structure.
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/


UINT_32 u4OriRegValue;
BOOLEAN nicpmSetDriverOwn(IN P_ADAPTER_T prAdapter)
{
#define LP_OWN_BACK_TOTAL_DELAY_MS      2000	/* exponential of 2 */
#define LP_OWN_BACK_LOOP_DELAY_MS       1	/* exponential of 2 */
#define LP_OWN_BACK_CLR_OWN_ITERATION   200	/* exponential of 2 */

	BOOLEAN fgStatus = TRUE;
	UINT_32 i, u4CurrTick, u4WriteTick, u4WriteTickTemp, u4TickDiff = 0;
	UINT_32 u4RegValue = 0;
	GL_HIF_INFO_T *HifInfo;
	BOOLEAN fgWmtCoreDump = FALSE;

	ASSERT(prAdapter);

	if (prAdapter->fgIsFwOwn == FALSE)
		return fgStatus;

	HifInfo = &prAdapter->prGlueInfo->rHifInfo;

	u4WriteTick = 0;
	u4CurrTick = kalGetTimeTick();

	STATS_DRIVER_OWN_START_RECORD();
	i = 0;

	while (1) {
		HAL_MCR_RD(prAdapter, MCR_WHLPCR, &u4RegValue);
		DBGLOG(NIC, TRACE, "<WiFi> MCR_WHLPCR = 0x%x\n", u4RegValue);
		if (u4RegValue & WHLPCR_FW_OWN_REQ_SET) {
			HAL_MCR_RD(prAdapter, MCR_D2HRM2R, &u4OriRegValue);
			prAdapter->fgIsFwOwn = FALSE;
			break;
		} else if (kalIsCardRemoved(prAdapter->prGlueInfo) == TRUE
			   || fgIsBusAccessFailed == TRUE
			   || (u4TickDiff = (kalGetTimeTick() - u4CurrTick)) > LP_OWN_BACK_TOTAL_DELAY_MS
			   || fgIsResetting == TRUE) {
			/* ERRORLOG(("LP cannot be own back (for %ld ms)", kalGetTimeTick() - u4CurrTick)); */
			fgStatus = FALSE;
			if (fgIsResetting != TRUE) {
				UINT_32 u4FwCnt;
				static unsigned int u4OwnCnt;
				/* MCR_D2HRM2R: low 4 bit means interrupt times,
				 * high 4 bit means firmware response times.
				 * ORI_MCR_D2HRM2R: the last successful value.
				 * for example:
				 * MCR_D2HRM2R = 0x44, ORI_MCR_D2HRM2R = 0x44
				 * means firmware no receive interrupt form hardware.
				 * MCR_D2HRM2R = 0x45, ORI_MCR_D2HRM2R = 0x44
				 * means firmware no send response.
				 * MCR_D2HRM2R = 0x55, ORI_MCR_D2HRM2R = 0x44
				 * means firmware send response, but driver no receive.
				 */
				HAL_MCR_RD(prAdapter, MCR_D2HRM2R, &u4RegValue);
				DBGLOG(NIC, WARN, "<WiFi> [1]MCR_D2HRM2R = 0x%x, ORI_MCR_D2HRM2R = 0x%x\n",
					u4RegValue, u4OriRegValue);

				HAL_MCR_WR(prAdapter, MCR_WHLPCR, WHLPCR_FW_OWN_REQ_CLR);
				HAL_MCR_RD(prAdapter, MCR_WHLPCR, &u4RegValue);
				if (u4RegValue & WHLPCR_FW_OWN_REQ_SET) {
					HAL_MCR_RD(prAdapter, MCR_D2HRM2R, &u4OriRegValue);
					prAdapter->fgIsFwOwn = FALSE;
					break;
				}
				HAL_MCR_RD(prAdapter, MCR_D2HRM2R, &u4RegValue);
				DBGLOG(NIC, WARN, "<WiFi> [2]MCR_D2HRM2R = 0x%x, ORI_MCR_D2HRM2R = 0x%x\n",
					u4RegValue, u4OriRegValue);
				fgWmtCoreDump = glIsWmtCodeDump();
				DBGLOG(NIC, WARN,
					"<WiFi> Fatal error! Driver own fail!!!! %d, fgIsBusAccessFailed: %d, OWN retry:%d, fgCoreDump:%d, u4TickDiff:%u\n",
					u4OwnCnt++, fgIsBusAccessFailed, i, fgWmtCoreDump, u4TickDiff);
				DBGLOG(NIC, WARN, "CONNSYS FW CPUINFO:\n");
				for (u4FwCnt = 0; u4FwCnt < 16; u4FwCnt++)
					DBGLOG(NIC, WARN, "0x%08x ", MCU_REG_READL(HifInfo, CONN_MCU_CPUPCR));
				/* CONSYS_REG_READ(CONSYS_CPUPCR_REG) */
				if (fgWmtCoreDump == FALSE) {
					kalSendAeeWarning("[Fatal error! Driver own fail!]", __func__);
					GL_RESET_TRIGGER(prAdapter, RST_FLAG_CHIP_RESET);
				} else
					DBGLOG(NIC, WARN,
						"[Driver own fail!] WMT is code dumping !STOP AEE & chip reset\n");

			}
			break;
		}

		u4WriteTickTemp = kalGetTimeTick();
		if (((u4WriteTickTemp - u4WriteTick) > LP_OWN_BACK_CLR_OWN_ITERATION)
			|| (i == 0)) {
			/* Software get LP ownership - per  LP_OWN_BACK_CLR_OWN_ITERATION*/
			DBGLOG(NIC, TRACE, "retry i=%d, write LP_OWN_REQ_CLR cur time %u - %u\n",
				i, u4WriteTickTemp, u4WriteTick);
			HAL_MCR_WR(prAdapter, MCR_WHLPCR, WHLPCR_FW_OWN_REQ_CLR);
			u4WriteTick = u4WriteTickTemp;
		}

		/* Delay for LP engine to complete its operation. */
		if (i <= 8)
			kalMdelay(LP_OWN_BACK_LOOP_DELAY_MS);
		else
			kalMsleep(LP_OWN_BACK_LOOP_DELAY_MS);
		i++;
	}

	STATS_DRIVER_OWN_END_RECORD();
	STATS_DRIVER_OWN_STOP();

	return fgStatus;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to set ACPI power mode to D0.
*
* \param[in] pvAdapter Pointer to the Adapter structure.
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
BOOLEAN nicpmSetAcpiPowerD0(IN P_ADAPTER_T prAdapter)
{
	WLAN_STATUS u4Status = WLAN_STATUS_SUCCESS;
	UINT_32 u4Value = 0, u4WHISR = 0;
	UINT_8 aucTxCount[8];
	UINT_32 i;
#if CFG_ENABLE_FW_DOWNLOAD
	UINT_32 u4FwImgLength, u4FwLoadAddr, u4ImgSecSize;
	PVOID prFwMappingHandle;
	PVOID pvFwImageMapFile = NULL;
#if CFG_ENABLE_FW_DIVIDED_DOWNLOAD
	UINT_32 j;
	P_FIRMWARE_DIVIDED_DOWNLOAD_T prFwHead;
	BOOLEAN fgValidHead;
	const UINT_32 u4CRCOffset = offsetof(FIRMWARE_DIVIDED_DOWNLOAD_T, u4NumOfEntries);
#endif
#endif

	DEBUGFUNC("nicpmSetAcpiPowerD0");
	ASSERT(prAdapter);

	do {
		/* 0. Reset variables in ADAPTER_T */
		prAdapter->fgIsFwOwn = TRUE;
		prAdapter->fgWiFiInSleepyState = FALSE;
		prAdapter->rAcpiState = ACPI_STATE_D0;
		prAdapter->fgIsEnterD3ReqIssued = FALSE;

		/* 1. Request Ownership to enter F/W download state */
		ACQUIRE_POWER_CONTROL_FROM_PM(prAdapter);
#if !CFG_ENABLE_FULL_PM
		nicpmSetDriverOwn(prAdapter);
#endif

		/* 2. Initialize the Adapter */
		u4Status = nicInitializeAdapter(prAdapter);
		if (u4Status != WLAN_STATUS_SUCCESS) {
			DBGLOG(NIC, ERROR, "nicInitializeAdapter failed!\n");
			u4Status = WLAN_STATUS_FAILURE;
			break;
		}

#if CFG_ENABLE_FW_DOWNLOAD
		prFwMappingHandle = kalFirmwareImageMapping(prAdapter->prGlueInfo, &pvFwImageMapFile, &u4FwImgLength);
		if (!prFwMappingHandle) {
			DBGLOG(NIC, ERROR, "Fail to load FW image from file!\n");
			pvFwImageMapFile = NULL;
		}

		if (pvFwImageMapFile == NULL) {
			u4Status = WLAN_STATUS_FAILURE;
			break;
		}

		/* 3.1 disable interrupt, download is done by polling mode only */
		nicDisableInterrupt(prAdapter);

		/* 3.2 Initialize Tx Resource to fw download state */
		nicTxInitResetResource(prAdapter);

		/* 3.3 FW download here */
		u4FwLoadAddr = kalGetFwLoadAddress(prAdapter->prGlueInfo);

#if CFG_ENABLE_FW_DIVIDED_DOWNLOAD
		/* 3a. parse file header for decision of divided firmware download or not */
		prFwHead = (P_FIRMWARE_DIVIDED_DOWNLOAD_T) pvFwImageMapFile;

		if (prFwHead->u4Signature == MTK_WIFI_SIGNATURE &&
		    prFwHead->u4CRC == wlanCRC32((PUINT_8) pvFwImageMapFile + u4CRCOffset,
						 u4FwImgLength - u4CRCOffset)) {
			fgValidHead = TRUE;
		} else {
			fgValidHead = FALSE;
		}

		/* 3b. engage divided firmware downloading */
		if (fgValidHead == TRUE) {
			for (i = 0; i < prFwHead->u4NumOfEntries; i++) {
#if CFG_ENABLE_FW_DOWNLOAD_AGGREGATION
				if (wlanImageSectionDownloadAggregated(prAdapter,
					prFwHead->arSection[i].u4DestAddr,
					prFwHead->arSection[i].u4Length,
					(PUINT_8) pvFwImageMapFile +
					prFwHead->arSection[i].u4Offset) !=
					WLAN_STATUS_SUCCESS) {
					DBGLOG(NIC, ERROR, "Firmware scatter download failed!\n");
					u4Status = WLAN_STATUS_FAILURE;
				}
#else
				for (j = 0; j < prFwHead->arSection[i].u4Length; j += CMD_PKT_SIZE_FOR_IMAGE) {
					if (j + CMD_PKT_SIZE_FOR_IMAGE < prFwHead->arSection[i].u4Length)
						u4ImgSecSize = CMD_PKT_SIZE_FOR_IMAGE;
					else
						u4ImgSecSize = prFwHead->arSection[i].u4Length - j;

					if (wlanImageSectionDownload(prAdapter,
						prFwHead->arSection[i].u4DestAddr + j,
						u4ImgSecSize,
						(PUINT_8) pvFwImageMapFile +
						prFwHead->arSection[i].u4Offset + j) != WLAN_STATUS_SUCCESS) {
						DBGLOG(NIC, ERROR, "Firmware scatter download failed!\n");
						u4Status = WLAN_STATUS_FAILURE;
						break;
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
			u4FwImgLength,
			(PUINT_8) pvFwImageMapFile) != WLAN_STATUS_SUCCESS) {
			DBGLOG(NIC, ERROR, "Firmware scatter download failed!\n");
			u4Status = WLAN_STATUS_FAILURE;
		}
#else
			for (i = 0; i < u4FwImgLength; i += CMD_PKT_SIZE_FOR_IMAGE) {
				if (i + CMD_PKT_SIZE_FOR_IMAGE < u4FwImgLength)
					u4ImgSecSize = CMD_PKT_SIZE_FOR_IMAGE;
				else
					u4ImgSecSize = u4FwImgLength - i;

				if (wlanImageSectionDownload(prAdapter,
					u4FwLoadAddr + i,
					u4ImgSecSize,
					(PUINT_8) pvFwImageMapFile + i) != WLAN_STATUS_SUCCESS) {
					DBGLOG(NIC, ERROR, "wlanImageSectionDownload failed!\n");
					u4Status = WLAN_STATUS_FAILURE;
					break;
				}
			}
#endif

		if (u4Status != WLAN_STATUS_SUCCESS) {
			kalFirmwareImageUnmapping(prAdapter->prGlueInfo, prFwMappingHandle, pvFwImageMapFile);
			break;
		}
#if !CFG_ENABLE_FW_DOWNLOAD_ACK
		/* Send INIT_CMD_ID_QUERY_PENDING_ERROR command and wait for response */
		if (wlanImageQueryStatus(prAdapter) != WLAN_STATUS_SUCCESS) {
			kalFirmwareImageUnmapping(prAdapter->prGlueInfo, prFwMappingHandle, pvFwImageMapFile);
			u4Status = WLAN_STATUS_FAILURE;
			break;
		}
#endif
		kalFirmwareImageUnmapping(prAdapter->prGlueInfo, prFwMappingHandle, pvFwImageMapFile);

		/* 4. send Wi-Fi Start command */
#if CFG_OVERRIDE_FW_START_ADDRESS
		wlanConfigWifiFunc(prAdapter, TRUE, kalGetFwStartAddress(prAdapter->prGlueInfo));
#else
		wlanConfigWifiFunc(prAdapter, FALSE, 0);
#endif
#endif /* if CFG_ENABLE_FW_DOWNLOAD */

		/* 5. check Wi-Fi FW asserts ready bit */
		DBGLOG(NIC, TRACE, "wlanAdapterStart(): Waiting for Ready bit..\n");
		i = 0;
		while (1) {
			HAL_MCR_RD(prAdapter, MCR_WCIR, &u4Value);

			if (u4Value & WCIR_WLAN_READY) {
				DBGLOG(NIC, TRACE, "Ready bit asserted\n");
				break;
			} else if (kalIsCardRemoved(prAdapter->prGlueInfo) == TRUE || fgIsBusAccessFailed == TRUE) {
				u4Status = WLAN_STATUS_FAILURE;
				break;
			} else if (i >= CFG_RESPONSE_POLLING_TIMEOUT) {
				DBGLOG(NIC, ERROR, "Waiting for Ready bit: Timeout\n");
				u4Status = WLAN_STATUS_FAILURE;
				break;
			}
			i++;
			kalMsleep(10);
		}

		if (u4Status == WLAN_STATUS_SUCCESS) {
			/* 6.1 reset interrupt status */
			HAL_READ_INTR_STATUS(prAdapter, 4, (PUINT_8)(&u4WHISR));
			if (HAL_IS_TX_DONE_INTR(u4WHISR))
				HAL_READ_TX_RELEASED_COUNT(prAdapter, aucTxCount);

			/* 6.2 reset TX Resource for normal operation */
			nicTxResetResource(prAdapter);

			/* 6.3 Enable interrupt */
			nicEnableInterrupt(prAdapter);

			/* 6.4 Override network address */
			wlanUpdateNetworkAddress(prAdapter);

			/* 6.5 indicate disconnection as default status */
			kalIndicateStatusAndComplete(prAdapter->prGlueInfo, WLAN_STATUS_MEDIA_DISCONNECT, NULL, 0);
		}

		RECLAIM_POWER_CONTROL_TO_PM(prAdapter, FALSE);

		/* MGMT Initialization */
		nicInitMGMT(prAdapter, NULL);

	} while (FALSE);

	if (u4Status != WLAN_STATUS_SUCCESS)
		return FALSE;
	else
		return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief This routine is used to set ACPI power mode to D3.
*
* @param prAdapter pointer to the Adapter handler
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
BOOLEAN nicpmSetAcpiPowerD3(IN P_ADAPTER_T prAdapter)
{
	UINT_32 i;

	ASSERT(prAdapter);

	/* 1. MGMT - unitialization */
	nicUninitMGMT(prAdapter);

	/* 2. Disable Interrupt */
	nicDisableInterrupt(prAdapter);

	/* 3. emit CMD_NIC_POWER_CTRL command packet */
	wlanSendNicPowerCtrlCmd(prAdapter, 1);

	/* 4. Clear Interrupt Status */
	i = 0;
	while (i < CFG_IST_LOOP_COUNT && nicProcessIST(prAdapter) != WLAN_STATUS_NOT_INDICATING) {
		i++;
	};

	/* 5. Remove pending TX */
	nicTxRelease(prAdapter);

	/* 5.1 clear pending Security / Management Frames */
	kalClearSecurityFrames(prAdapter->prGlueInfo);
	kalClearMgmtFrames(prAdapter->prGlueInfo);

	/* 5.2 clear pending TX packet queued in glue layer */
	kalFlushPendingTxPackets(prAdapter->prGlueInfo);

	/* 6. Set Onwership to F/W */
	nicpmSetFWOwn(prAdapter, FALSE);

	/* 7. Set variables */
	prAdapter->rAcpiState = ACPI_STATE_D3;

	return TRUE;
}

