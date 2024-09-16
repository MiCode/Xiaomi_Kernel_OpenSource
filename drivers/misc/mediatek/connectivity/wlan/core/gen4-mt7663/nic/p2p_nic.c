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
 ** Id: @(#) p2p_nic.c@@
 */

/*! \file   p2p_nic.c
 *    \brief  Wi-Fi Direct Functions that provide operation
 *              in NIC's (Network Interface Card) point of view.
 *
 *    This file includes functions which unite multiple hal(Hardware) operations
 *    and also take the responsibility of Software Resource Management in order
 *    to keep the synchronization with Hardware Manipulation.
 */

/******************************************************************************
 *                         C O M P I L E R   F L A G S
 ******************************************************************************
 */

/******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 ******************************************************************************
 */

#include "precomp.h"

/******************************************************************************
 *                              C O N S T A N T S
 ******************************************************************************
 */

/******************************************************************************
 *                             D A T A   T Y P E S
 ******************************************************************************
 */

/******************************************************************************
 *                            P U B L I C   D A T A
 ******************************************************************************
 */

/******************************************************************************
 *                           P R I V A T E   D A T A
 ******************************************************************************
 */

/******************************************************************************
 *                                 M A C R O S
 ******************************************************************************
 */

/******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 ******************************************************************************
 */

/******************************************************************************
 *                              F U N C T I O N S
 ******************************************************************************
 */

/*----------------------------------------------------------------------------*/
/*!
 * @brief When Probe Rsp & Beacon frame is received and decide a P2P device,
 *        this function will be invoked to buffer scan result
 *
 * @param prAdapter              Pointer to the Adapter structure.
 * @param prEventScanResult      Pointer of EVENT_SCAN_RESULT_T.
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void
nicRxAddP2pDevice(IN struct ADAPTER *prAdapter,
		IN struct EVENT_P2P_DEV_DISCOVER_RESULT *prP2pResult,
		IN uint8_t *pucRxIEBuf,
		IN uint16_t u2RxIELength)
{
	struct P2P_INFO *prP2pInfo = (struct P2P_INFO *) NULL;
	struct EVENT_P2P_DEV_DISCOVER_RESULT *prTargetResult =
		(struct EVENT_P2P_DEV_DISCOVER_RESULT *) NULL;
	uint32_t u4Idx = 0;
	u_int8_t bUpdate = FALSE;

	uint8_t *pucIeBuf = (uint8_t *) NULL;
	uint16_t u2IELength = 0;
	uint8_t zeroMac[] = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 };

	ASSERT(prAdapter);

	prP2pInfo = prAdapter->prP2pInfo;

	for (u4Idx = 0; u4Idx < prP2pInfo->u4DeviceNum; u4Idx++) {
		prTargetResult = &prP2pInfo->arP2pDiscoverResult[u4Idx];

		if (EQUAL_MAC_ADDR(prTargetResult->aucDeviceAddr,
			prP2pResult->aucDeviceAddr)) {

			bUpdate = TRUE;

			/* Backup OLD buffer result. */
			pucIeBuf = prTargetResult->pucIeBuf;
			u2IELength = prTargetResult->u2IELength;

			/* Update Device Info. */
			/* zero */
			kalMemZero(prTargetResult,
				sizeof(struct EVENT_P2P_DEV_DISCOVER_RESULT));

			/* then buffer */
			kalMemCopy(prTargetResult,
				(void *) prP2pResult,
				sizeof(struct EVENT_P2P_DEV_DISCOVER_RESULT));

			/* See if new IE length is longer or not. */
			if ((u2RxIELength > u2IELength) && (u2IELength != 0)) {
				/* Buffer is not enough. */
				u2RxIELength = u2IELength;
			} else if ((u2IELength == 0) && (u2RxIELength != 0)) {
				/* RX new IE buf. */
				ASSERT(pucIeBuf == NULL);
				pucIeBuf = prP2pInfo->pucCurrIePtr;

				if (((unsigned long) prP2pInfo->pucCurrIePtr
					+ (unsigned long) u2RxIELength) >
				    (unsigned long)&
				    prP2pInfo->aucCommIePool
				    [CFG_MAX_COMMON_IE_BUF_LEN]) {

					/* Common Buffer is no enough. */
					u2RxIELength =
					    (uint16_t) ((unsigned long)
					    &prP2pInfo->aucCommIePool
					    [CFG_MAX_COMMON_IE_BUF_LEN] -
						(unsigned long)
						prP2pInfo->pucCurrIePtr);
				}

				/* Step to next buffer address. */
				prP2pInfo->pucCurrIePtr =
				    (uint8_t *) ((unsigned long)
					prP2pInfo->pucCurrIePtr +
					(unsigned long) u2RxIELength);
			}

			/* Restore buffer pointer. */
			prTargetResult->pucIeBuf = pucIeBuf;

			if (pucRxIEBuf) {
				/* If new received IE is available.
				 * Replace the old one & update new IE length.
				 */
				kalMemCopy(pucIeBuf, pucRxIEBuf, u2RxIELength);
				prTargetResult->u2IELength = u2RxIELength;
			} else {
				/* There is no new IE information,
				 * keep the old one.
				 */
				prTargetResult->u2IELength = u2IELength;
			}
		}
	}

	if (!bUpdate) {
		/* We would flush the whole scan result
		 * after each scan request is issued.
		 * If P2P device is too many, it may over the scan list.
		 */
		if ((u4Idx < CFG_MAX_NUM_BSS_LIST)
			&& (UNEQUAL_MAC_ADDR(zeroMac,
			prP2pResult->aucDeviceAddr))) {

			prTargetResult = &prP2pInfo->arP2pDiscoverResult[u4Idx];

			/* zero */
			kalMemZero(prTargetResult,
				sizeof(struct EVENT_P2P_DEV_DISCOVER_RESULT));

			/* then buffer */
			kalMemCopy(prTargetResult,
				(void *) prP2pResult,
				sizeof(struct EVENT_P2P_DEV_DISCOVER_RESULT));

			if (u2RxIELength) {
				prTargetResult->pucIeBuf =
					prP2pInfo->pucCurrIePtr;

				if (((unsigned long) prP2pInfo->pucCurrIePtr
					+ (unsigned long) u2RxIELength) >
				    (unsigned long)
				    &prP2pInfo->aucCommIePool
				    [CFG_MAX_COMMON_IE_BUF_LEN]) {

					/* Common Buffer is no enough. */
					u2IELength =
						(uint16_t) ((unsigned long)
						&prP2pInfo->aucCommIePool
						[CFG_MAX_COMMON_IE_BUF_LEN] -
						(unsigned long)
						prP2pInfo->pucCurrIePtr);
				} else {
					u2IELength = u2RxIELength;
				}

				prP2pInfo->pucCurrIePtr =
				    (uint8_t *) ((unsigned long)
				    prP2pInfo->pucCurrIePtr
				    + (unsigned long) u2IELength);

				kalMemCopy((void *) prTargetResult->pucIeBuf,
					(void *) pucRxIEBuf,
					(uint32_t) u2IELength);

				prTargetResult->u2IELength = u2IELength;
			} else {
				prTargetResult->pucIeBuf = NULL;
				prTargetResult->u2IELength = 0;
			}

			prP2pInfo->u4DeviceNum++;

		} else {
			/* TODO: Fixme to replace an old one. (?) */
			ASSERT(FALSE);
		}
	}
}				/* nicRxAddP2pDevice */
