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
 ** Id: //Department/DaVinci/BRANCHES/
 *      MT6620_WIFI_DRIVER_V2_3/nic/nic_pwr_mgt.c#1
 */

/*!   \file   "nic_pwr_mgt.c"
 *    \brief  In this file we define the STATE and EVENT for Power Management
 *            FSM.
 *    The SCAN FSM is responsible for performing SCAN behavior when the Arbiter
 *    enter ARB_STATE_SCAN. The STATE and EVENT for SCAN FSM are defined here
 *    with detail description.
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

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

void nicpmWakeUpWiFi(IN struct ADAPTER *prAdapter)
{
	if (!nicVerifyChipID(prAdapter)) {
		DBGLOG(INIT, ERROR, "Chip id verify error!\n");
		return;
	}
	HAL_WAKE_UP_WIFI(prAdapter);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is used to process the POWER ON procedure.
 *
 * \param[in] pvAdapter Pointer to the Adapter structure.
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
void nicpmSetFWOwn(IN struct ADAPTER *prAdapter,
		   IN u_int8_t fgEnableGlobalInt)
{
	halSetFWOwn(prAdapter, fgEnableGlobalInt);
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
u_int8_t nicpmSetDriverOwn(IN struct ADAPTER *prAdapter)
{
	return halSetDriverOwn(prAdapter);
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
u_int8_t nicpmSetAcpiPowerD0(IN struct ADAPTER *prAdapter)
{

#if 0
	uint32_t u4Status = WLAN_STATUS_SUCCESS;
	uint32_t u4Value = 0, u4WHISR = 0;
	uint16_t au2TxCount[16];
	uint32_t i;
#if CFG_ENABLE_FW_DOWNLOAD
	uint32_t u4FwImgLength, u4FwLoadAddr, u4Cr4FwImgLength;
	void *prFwMappingHandle;
	void *pvFwImageMapFile = NULL;
	void *pvCr4FwImageMapFile = NULL;
#if CFG_ENABLE_FW_DIVIDED_DOWNLOAD
	struct FIRMWARE_DIVIDED_DOWNLOAD *prFwHead;
	u_int8_t fgValidHead = TRUE;

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

#if defined(MT6630)
		/* 1. Request Ownership to enter F/W download state */
		ACQUIRE_POWER_CONTROL_FROM_PM(prAdapter);
#if !CFG_ENABLE_FULL_PM
		nicpmSetDriverOwn(prAdapter);
#endif

		/* 2. Initialize the Adapter */
		u4Status = nicInitializeAdapter(prAdapter);
		if (u4Status != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, "nicInitializeAdapter failed!\n");
			u4Status = WLAN_STATUS_FAILURE;
			break;
		}
#endif

#if CFG_ENABLE_FW_DOWNLOAD
		prFwMappingHandle =
			kalFirmwareImageMapping(prAdapter->prGlueInfo,
				&pvFwImageMapFile, &u4FwImgLength,
				&pvCr4FwImageMapFile, &u4Cr4FwImgLength);
		if (!prFwMappingHandle) {
			DBGLOG(INIT, ERROR,
				"Fail to load FW image from file!\n");
			pvFwImageMapFile = NULL;
		}
#if defined(MT6630)
		if (pvFwImageMapFile) {
			/* 3.1 disable interrupt,
			 * download is done by polling mode only
			 */
			nicDisableInterrupt(prAdapter);

			/* 3.2 Initialize Tx Resource to fw download state */
			nicTxInitResetResource(prAdapter);

			/* 3.3 FW download here */
			u4FwLoadAddr =
				kalGetFwLoadAddress(prAdapter->prGlueInfo);

#if CFG_ENABLE_FW_DIVIDED_DOWNLOAD
			/* 3a. parse file header for decision of
			 * divided firmware download or not
			 */
			prFwHead =
				(struct FIRMWARE_DIVIDED_DOWNLOAD *)
				((uint8_t *)
				pvFwImageMapFile + u4FwImgLength -
				(2 * sizeof(struct FWDL_SECTION_INFO)));
#if 0
			if (prFwHead->u4Signature == MTK_WIFI_SIGNATURE &&
			    prFwHead->u4CRC == wlanCRC32(
						(uint8_t *) pvFwImageMapFile +
						u4CRCOffset,
						u4FwImgLength - u4CRCOffset)) {
				fgValidHead = TRUE;
			} else {
				fgValidHead = FALSE;
			}
#endif
			/* 3b. engage divided firmware downloading */
			if (fgValidHead == TRUE) {
				wlanFwDvdDwnloadHandler(prAdapter, prFwHead,
					pvFwImageMapFile, &u4Status);
			} else
#endif
			{
				if (wlanImageSectionConfig(prAdapter,
						u4FwLoadAddr,
						u4FwImgLength, TRUE)
						!= WLAN_STATUS_SUCCESS) {
					DBGLOG(INIT, ERROR,
					       "Firmware download configuration failed!\n");

					u4Status = WLAN_STATUS_FAILURE;
					break;
				}
				wlanFwDwnloadHandler(prAdapter, u4FwImgLength,
					pvFwImageMapFile, &u4Status);
			}
			/* escape to top */
			if (u4Status != WLAN_STATUS_SUCCESS) {
				kalFirmwareImageUnmapping(prAdapter->prGlueInfo,
					prFwMappingHandle, pvFwImageMapFile,
					pvCr4FwImageMapFile);
				break;
			}
#if !CFG_ENABLE_FW_DOWNLOAD_ACK
			/* Send INIT_CMD_ID_QUERY_PENDING_ERROR command
			 * and wait for response
			 */
			if (wlanImageQueryStatus(prAdapter) !=
			    WLAN_STATUS_SUCCESS) {
				kalFirmwareImageUnmapping(prAdapter->prGlueInfo,
					prFwMappingHandle, pvFwImageMapFile,
					pvCr4FwImageMapFile);
				u4Status = WLAN_STATUS_FAILURE;
				break;
			}
#endif

			kalFirmwareImageUnmapping(prAdapter->prGlueInfo,
				prFwMappingHandle, pvFwImageMapFile,
				pvCr4FwImageMapFile);
		} else {
			u4Status = WLAN_STATUS_FAILURE;
			break;
		}

		/* 4. send Wi-Fi Start command */
#if CFG_OVERRIDE_FW_START_ADDRESS
		wlanConfigWifiFunc(prAdapter, TRUE,
				   kalGetFwStartAddress(prAdapter->prGlueInfo));
#else
		wlanConfigWifiFunc(prAdapter, FALSE, 0);
#endif
#endif
#endif

		/* 5. check Wi-Fi FW asserts ready bit */
		DBGLOG(INIT, TRACE,
		       "wlanAdapterStart(): Waiting for Ready bit..\n");
		i = 0;
		while (1) {
			HAL_MCR_RD(prAdapter, MCR_WCIR, &u4Value);

			if (u4Value & WCIR_WLAN_READY) {
				DBGLOG(INIT, TRACE, "Ready bit asserted\n");
				break;
			} else if (
					kalIsCardRemoved(
						prAdapter->prGlueInfo) == TRUE
				   || fgIsBusAccessFailed == TRUE) {
				u4Status = WLAN_STATUS_FAILURE;
				break;
			} else if (i >= CFG_RESPONSE_POLLING_TIMEOUT) {
				DBGLOG(INIT, ERROR,
					"Waiting for Ready bit: Timeout\n");
				u4Status = WLAN_STATUS_FAILURE;
				break;
			}
			i++;
			kalMsleep(10);
		}

		if (u4Status == WLAN_STATUS_SUCCESS) {
			/* 6.1 reset interrupt status */
			HAL_READ_INTR_STATUS(prAdapter, 4, (uint8_t *)&u4WHISR);
			if (HAL_IS_TX_DONE_INTR(u4WHISR))
				HAL_READ_TX_RELEASED_COUNT(prAdapter,
					au2TxCount);

			/* 6.2 reset TX Resource for normal operation */
			nicTxResetResource(prAdapter);

			/* 6.3 Enable interrupt */
			nicEnableInterrupt(prAdapter);

			/* 6.4 Update basic configuration */
			wlanUpdateBasicConfig(prAdapter);

			/* 6.5 Apply Network Address */
			nicApplyNetworkAddress(prAdapter);

			/* 6.6 indicate disconnection as default status */
			kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
				WLAN_STATUS_MEDIA_DISCONNECT, NULL, 0);
		}

		RECLAIM_POWER_CONTROL_TO_PM(prAdapter, FALSE);

		/* MGMT Initialization */
		nicInitMGMT(prAdapter, NULL);

	} while (FALSE);

	if (u4Status != WLAN_STATUS_SUCCESS)
		return FALSE;
	else
		return TRUE;
#else
	return TRUE;
#endif
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
u_int8_t nicpmSetAcpiPowerD3(IN struct ADAPTER *prAdapter)
{
	/*	UINT_32 i; */

	ASSERT(prAdapter);

#if 0
	/* 1. MGMT - unitialization */
	nicUninitMGMT(prAdapter);

	/* 2. Disable Interrupt */
	nicDisableInterrupt(prAdapter);

	/* 3. emit CMD_NIC_POWER_CTRL command packet */
	wlanSendNicPowerCtrlCmd(prAdapter, 1);

	/* 4. Clear Interrupt Status */
	i = 0;
	while (i < CFG_IST_LOOP_COUNT
	       && nicProcessIST(prAdapter) != WLAN_STATUS_NOT_INDICATING) {
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
#endif
	return TRUE;
}
