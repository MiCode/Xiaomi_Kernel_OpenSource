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
 * Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/nic/nic_pwr_mgt.c#1
 */

/*
 * ! \file   "nic_pwr_mgt.c"
 *   \brief  In this file we define the STATE and EVENT for Power Management FSM.
 *
 *   The SCAN FSM is responsible for performing SCAN behavior when the Arbiter enter
 *   ARB_STATE_SCAN. The STATE and EVENT for SCAN FSM are defined here with detail
 *   description.
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
	BOOLEAN fgHifFwOwn = TRUE;

	ASSERT(prAdapter);

	HAL_MCR_RD(prAdapter, MCR_WHLPCR, &u4RegValue);
	fgHifFwOwn = !(u4RegValue & WHLPCR_FW_OWN_REQ_SET);
	if (fgHifFwOwn != prAdapter->fgIsFwOwn) {
		DBGLOG(NIC, ERROR,
		       "FW own status mismatch! fgIsFwOwn=%d, WHLPCR=0x%x\n",
		       prAdapter->fgIsFwOwn, u4RegValue);
		if (fgHifFwOwn) {
			prAdapter->fgIsFwOwn = TRUE;
			return;
		}
	} else if (prAdapter->fgIsFwOwn)
		return;

	if (nicProcessIST(prAdapter) != WLAN_STATUS_NOT_INDICATING) {
		DBGLOG(NIC, INFO, "FW OWN Failed due to pending INT\n");
		/* pending interrupts */
		return;
	}

	if (fgEnableGlobalInt) {
		DBGLOG(NIC, INFO, "FW OWN, fgEnableGlobalInt=TRUE\n");
		prAdapter->fgIsIntEnableWithLPOwnSet = TRUE;
		return;
	}
	HAL_MCR_WR(prAdapter, MCR_WHLPCR, WHLPCR_FW_OWN_REQ_SET);

	HAL_MCR_RD(prAdapter, MCR_WHLPCR, &u4RegValue);
	if (u4RegValue & WHLPCR_FW_OWN_REQ_SET) {
		/* if set firmware own not successful (possibly pending interrupts), */
		/* indicate an own clear event */
		HAL_MCR_WR(prAdapter, MCR_WHLPCR, WHLPCR_FW_OWN_REQ_CLR);
		DBGLOG(NIC, INFO, "FW OWN fail\n");
		return;
	}
	kalTakeVcoreAction(VCORE_RESTORE_DEF);
	prAdapter->fgIsFwOwn = TRUE;
	DBGLOG(NIC, TRACE, "FW OWN\n");
}

VOID nicpmCheckAndTriggerDriverOwn(IN P_ADAPTER_T prAdapter)
{
	UINT_32 u4RegValue = 0;

	HAL_MCR_RD(prAdapter, MCR_WHLPCR, &u4RegValue);

	if (u4RegValue & WHLPCR_FW_OWN_REQ_SET) {
		/* WLAN_DRV_OWN is asserted on initial stage, but chip WLAN function is FW_OWN state actually,
		 * this is an issue due to HIF un-sync reset.
		 *
		 * Trigger FW_OWN to let HIF clear WLAN_DRV_OWN bit and make power state synchronized.
		 * F/W should remember to clear the residual bit in HWFISR.DRV_SET_FW_OWN.
		 */
		DBGLOG(NIC, WARN, "DRIVER OWN already set on initial stage!! trigger FW OWN to sync power state\n");
		HAL_MCR_WR(prAdapter, MCR_WHLPCR, WHLPCR_FW_OWN_REQ_SET);

		HAL_MCR_RD(prAdapter, MCR_WHLPCR, &u4RegValue);
		if (u4RegValue & WHLPCR_FW_OWN_REQ_SET) {
			/* Impossible case, H/W will clear WLAN_DRV_OWN bit immediately after
			 * WHLPCR.FW_OWN_REQ_SET is set
			 */
			DBGLOG(NIC, ERROR, "FW OWN fail, anyway continue to trigger DRIVER OWN\n");
		}
	}

	prAdapter->fgIsFwOwn = TRUE;
	HAL_MCR_WR(prAdapter, MCR_WHLPCR, WHLPCR_FW_OWN_REQ_CLR);
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
BOOLEAN nicpmSetDriverOwn(IN P_ADAPTER_T prAdapter)
{
#define LP_OWN_BACK_TOTAL_DELAY_MS      512	/* exponential of 2 */
#define LP_OWN_BACK_CLR_OWN_ITERATION   256	/* exponential of 2 */
#define LP_OWN_BACK_FAILED_RETRY_CNT    5
#define LP_OWN_BACK_FAILED_LOG_SKIP_MS  2000
#define LP_OWN_BACK_FAILED_RESET_CNT    5
#define LP_OWN_BACK_LOOP_DELAY_MIN_US   900
#define LP_OWN_BACK_LOOP_DELAY_MAX_US   1000

/* Polling cpupcr before driver own. WARN: Only for debug. Enable this feature will cut down throughput */
#define POLL_CPUPCR_BEFORE_DRIVER_OWN	0
#if POLL_CPUPCR_BEFORE_DRIVER_OWN
#define POLL_CPUPCR_BEFORE_DRIVER_OWN_COUNT 5
	UINT_32 u4PolCnt = 0;
	UINT_32 au4Cpupcr[POLL_CPUPCR_BEFORE_DRIVER_OWN_COUNT] = {0};
#endif
	BOOLEAN fgStatus = TRUE;
	UINT_32 i, u4CurrTick, u4RegValue = 0;
	BOOLEAN fgTimeout;
	BOOLEAN fgHifFwOwn = TRUE;

	ASSERT(prAdapter);

	HAL_MCR_RD(prAdapter, MCR_WHLPCR, &u4RegValue);
	fgHifFwOwn = !(u4RegValue & WHLPCR_FW_OWN_REQ_SET);
	if (fgHifFwOwn != prAdapter->fgIsFwOwn) {
		DBGLOG(NIC, ERROR,
		       "Driver own status mismatch! fgIsFwOwn=%d, WHLPCR=0x%x\n",
		       prAdapter->fgIsFwOwn, u4RegValue);
		if (!fgHifFwOwn) {
			prAdapter->fgIsFwOwn = FALSE;
			return fgStatus;
		}
	} else if (!prAdapter->fgIsFwOwn)
		return fgStatus;

	DBGLOG(INIT, TRACE, "DRIVER OWN\n");

	u4CurrTick = kalGetTimeTick();
#if POLL_CPUPCR_BEFORE_DRIVER_OWN
	for (u4PolCnt = 0; u4PolCnt < POLL_CPUPCR_BEFORE_DRIVER_OWN_COUNT; u4PolCnt++) {
		if (wlanGetCpupcr(&au4Cpupcr[u4PolCnt]) == WLAN_STATUS_FAILURE)
			DBGLOG(INIT, ERROR,
				"Polling Cpupcr before driver own %d failed!\n", u4PolCnt);
	}
#endif
	i = 0;
	while (1) {
		HAL_MCR_RD(prAdapter, MCR_WHLPCR, &u4RegValue);

		fgTimeout = ((kalGetTimeTick() - u4CurrTick) > LP_OWN_BACK_TOTAL_DELAY_MS) ? TRUE : FALSE;

		if (u4RegValue & WHLPCR_FW_OWN_REQ_SET) {
			prAdapter->fgIsFwOwn = FALSE;
			prAdapter->u4OwnFailedCount = 0;
			prAdapter->u4OwnFailedLogCount = 0;
			break;
		} else if ((i > LP_OWN_BACK_FAILED_RETRY_CNT) &&
			   (kalIsCardRemoved(prAdapter->prGlueInfo) || fgIsBusAccessFailed || fgTimeout
			    || wlanIsChipNoAck(prAdapter))) {

			if ((prAdapter->u4OwnFailedCount == 0) ||
			    CHECK_FOR_TIMEOUT(u4CurrTick, prAdapter->rLastOwnFailedLogTime,
					      MSEC_TO_SYSTIME(LP_OWN_BACK_FAILED_LOG_SKIP_MS))) {

				DBGLOG(NIC, ERROR,
				       "LP fail, Timeout(%ums) Bus Error[%u] Resetting[%u] NoAck[%u] Cnt[%u]",
					kalGetTimeTick() - u4CurrTick, fgIsBusAccessFailed, kalIsResetting(),
					wlanIsChipNoAck(prAdapter), prAdapter->u4OwnFailedCount);
				/* polling cpupcr for debug */
#if POLL_CPUPCR_BEFORE_DRIVER_OWN
				for (u4PolCnt = 0; u4PolCnt < POLL_CPUPCR_BEFORE_DRIVER_OWN_COUNT; u4PolCnt++) {
					DBGLOG(INIT, ERROR,
						"count:%d cpupcr before: %08x\n",
						u4PolCnt, au4Cpupcr[u4PolCnt]);
				}
#endif
				wlanPollingCpupcr(4, 5);
				prAdapter->u4OwnFailedLogCount++;
				if (prAdapter->u4OwnFailedLogCount > LP_OWN_BACK_FAILED_RESET_CNT) {
					/* Trigger RESET */
					GL_RESET_TRIGGER(prAdapter, RST_FLAG_CHIP_RESET);
				}
				GET_CURRENT_SYSTIME(&prAdapter->rLastOwnFailedLogTime);
			}

			prAdapter->u4OwnFailedCount++;
			fgStatus = FALSE;
			break;
		}

		if ((i & (LP_OWN_BACK_CLR_OWN_ITERATION - 1)) == 0) {
			/* Driver request LP ownership - per 256 iterations */
			HAL_MCR_WR(prAdapter, MCR_WHLPCR, WHLPCR_FW_OWN_REQ_CLR);
		}

		/* Delay for LP engine to complete its operation. */
		kalUsleep_range(LP_OWN_BACK_LOOP_DELAY_MIN_US, LP_OWN_BACK_LOOP_DELAY_MAX_US);
		i++;
	}
	if (i > 10)
		DBGLOG(NIC, INFO, "DRIVER OWN, status=%d count=%d\n", fgStatus, i);
	else
		DBGLOG(NIC, TRACE, "DRIVER OWN, status=%d count=%d\n", fgStatus, i);

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
	UINT_16 au2TxCount[16];
	UINT_32 i;
#if CFG_ENABLE_FW_DOWNLOAD
	UINT_32 u4FwImgLength, u4FwLoadAddr;
	PVOID prFwMappingHandle;
	PVOID pvFwImageMapFile = NULL;
#if CFG_ENABLE_FW_DIVIDED_DOWNLOAD
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

		if (pvFwImageMapFile) {
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
				wlanFwDvdDwnloadHandler(prAdapter, prFwHead, pvFwImageMapFile, &u4Status);
			} else
#endif
			{
				if (wlanImageSectionConfig(prAdapter,
							   u4FwLoadAddr,
							   u4FwImgLength,
							   TRUE,
							   TRUE,
							   0) != WLAN_STATUS_SUCCESS) {
					DBGLOG(INIT, ERROR, "Firmware download configuration failed!\n");

					u4Status = WLAN_STATUS_FAILURE;
					break;
				}

				wlanFwDwnloadHandler(prAdapter, u4FwImgLength, pvFwImageMapFile, &u4Status);
			}
			/* escape to top */
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
		} else {
			u4Status = WLAN_STATUS_FAILURE;
			break;
		}

		/* 4. send Wi-Fi Start command */
#if CFG_OVERRIDE_FW_START_ADDRESS
		wlanConfigWifiFunc(prAdapter, TRUE, kalGetFwStartAddress(prAdapter->prGlueInfo));
#else
		wlanConfigWifiFunc(prAdapter, FALSE, 0);
#endif
#endif

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
			HAL_READ_INTR_STATUS(prAdapter, 4, (PUINT_8)&u4WHISR);
			if (HAL_IS_TX_DONE_INTR(u4WHISR))
				HAL_READ_TX_RELEASED_COUNT(prAdapter, au2TxCount);

			/* 6.2 reset TX Resource for normal operation */
			nicTxResetResource(prAdapter);

			/* 6.3 Enable interrupt */
			nicEnableInterrupt(prAdapter);

			/* 6.4 Update basic configuration */
			wlanUpdateBasicConfig(prAdapter);

			/* 6.5 Apply Network Address */
			nicApplyNetworkAddress(prAdapter);

			/* 6.6 indicate disconnection as default status */
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
	nicTxRelease(prAdapter, TRUE);

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
