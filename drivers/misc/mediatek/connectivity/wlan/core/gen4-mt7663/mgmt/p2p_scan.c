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
 ** Id: @(#) p2p_scan.c@@
 */

/*! \file   "p2p_scan.c"
 *    \brief  This file defines the p2p scan profile and
 *      the processing function of scan result for SCAN Module.
 *
 *    The SCAN Profile selection is part of SCAN MODULE and
 *    responsible for defining SCAN Parameters -
 *    e.g. MIN_CHANNEL_TIME, number of scan channels.
 *    In this file we also define the process of SCAN Result
 *    including adding, searching and removing SCAN record from the list.
 */

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 ******************************************************************************
 */

/******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 ******************************************************************************
 */

#include "precomp.h"

/*******************************************************************************
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

void
scanP2pProcessBeaconAndProbeResp(IN struct ADAPTER *prAdapter,
		 IN struct SW_RFB *prSwRfb,
		 IN uint32_t *prStatus,
		 IN struct BSS_DESC *prBssDesc,
		 IN struct WLAN_BEACON_FRAME *prWlanBeaconFrame)
{
	u_int8_t fgIsBeacon = FALSE;
	u_int8_t fgIsSkipThisBeacon = FALSE;

	/* Indicate network to kernel for P2P interface when:
	 *  1. This is P2P network
	 *  2. Driver is configured to report all bss networks
	 */
	if (!prBssDesc->fgIsP2PPresent &&
		!prAdapter->p2p_scan_report_all_bss)
		return;

	fgIsBeacon = (prWlanBeaconFrame->u2FrameCtrl & MASK_FRAME_TYPE) ==
			MAC_FRAME_BEACON;

	if (prBssDesc->fgIsConnected && fgIsBeacon) {
		uint32_t u4Idx = 0;
		struct BSS_INFO *prP2pBssInfo =
			(struct BSS_INFO *) NULL;

		for (u4Idx = 0; u4Idx < prAdapter->ucHwBssIdNum; u4Idx++) {
			/* Check BSS for P2P. */
			/* Check BSSID. */
			prP2pBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
					(uint8_t) u4Idx);

			if (!IS_BSS_ACTIVE(prP2pBssInfo))
				continue;

			if ((prP2pBssInfo->eNetworkType != NETWORK_TYPE_P2P) ||
				(UNEQUAL_MAC_ADDR(prP2pBssInfo->aucBSSID,
					prBssDesc->aucBSSID) ||
				(!EQUAL_SSID(prP2pBssInfo->aucSSID,
					prP2pBssInfo->ucSSIDLen,
					prBssDesc->aucSSID,
					prBssDesc->ucSSIDLen)))) {
				continue;
			}
			/* P2P GC */
			/* Connected */
			if ((prP2pBssInfo->eCurrentOPMode ==
					OP_MODE_INFRASTRUCTURE) &&
				(prP2pBssInfo->eConnectionState ==
					PARAM_MEDIA_STATE_CONNECTED)) {
				fgIsSkipThisBeacon = TRUE;
				/* First Time. */
				if ((!prP2pBssInfo->ucDTIMPeriod)) {
					prP2pBssInfo->ucDTIMPeriod =
						prBssDesc->ucDTIMPeriod;
					nicPmIndicateBssConnected(
					prAdapter,
					prP2pBssInfo->ucBssIndex);
				}
			}

		}

	}

	/* Skip report beacon to upper layer if no p2p scan */
	if (prAdapter->prGlueInfo->prP2PDevInfo->prScanRequest == NULL &&
			fgIsBeacon)
		fgIsSkipThisBeacon = TRUE;

	if (fgIsBeacon && fgIsSkipThisBeacon) {
		/* Only report Probe Response frame
		 * to supplicant except passive scan.
		 */
		/* Probe response collect
		 * much more information.
		 */
		DBGLOG(P2P, TRACE, "Skip beacon [" MACSTR "][%s][ch %d]\n",
				MAC2STR(prWlanBeaconFrame->aucBSSID),
				prBssDesc->aucSSID,
				prBssDesc->ucChannelNum);
		return;
	}

	do {
		struct RF_CHANNEL_INFO rChannelInfo;

		ASSERT_BREAK((prSwRfb != NULL) && (prBssDesc != NULL));

		rChannelInfo.ucChannelNum = prBssDesc->ucChannelNum;
		rChannelInfo.eBand = prBssDesc->eBand;
		prBssDesc->fgIsP2PReport = TRUE;

		DBGLOG(P2P, INFO,
			"indicate [" MACSTR "][%s][%s][ch %d][r %d][t %u]\n",
			MAC2STR(prWlanBeaconFrame->aucBSSID),
			fgIsBeacon ? "Beacon" : "Probe Response",
			prBssDesc->aucSSID,
			prBssDesc->ucChannelNum,
			prBssDesc->ucRCPI,
			prBssDesc->rUpdateTime);

		kalP2PIndicateBssInfo(prAdapter->prGlueInfo,
				(uint8_t *) prSwRfb->pvHeader,
				(uint32_t) prSwRfb->u2PacketLen,
				&rChannelInfo,
				RCPI_TO_dBm(prBssDesc->ucRCPI));

	} while (FALSE);
}

void scnEventReturnChannel(IN struct ADAPTER *prAdapter,
		IN uint8_t ucScnSeqNum)
{

	struct CMD_SCAN_CANCEL rCmdScanCancel;

	/* send cancel message to firmware domain */
	rCmdScanCancel.ucSeqNum = ucScnSeqNum;
	rCmdScanCancel.ucIsExtChannel = (uint8_t) FALSE;

	wlanSendSetQueryCmd(prAdapter,
			    CMD_ID_SCAN_CANCEL,
			    TRUE,
			    FALSE, FALSE, NULL, NULL,
			    sizeof(struct CMD_SCAN_CANCEL),
			    (uint8_t *)&rCmdScanCancel, NULL, 0);
}				/* scnEventReturnChannel */

void scanRemoveAllP2pBssDesc(IN struct ADAPTER *prAdapter)
{
	struct LINK *prBSSDescList;
	struct BSS_DESC *prBssDesc;
	struct BSS_DESC *prBSSDescNext;

	ASSERT(prAdapter);

	prBSSDescList = &(prAdapter->rWifiVar.rScanInfo.rBSSDescList);

	/* Search BSS Desc from current SCAN result list. */
	LINK_FOR_EACH_ENTRY_SAFE(prBssDesc, prBSSDescNext, prBSSDescList,
		rLinkEntry, struct BSS_DESC) {
		scanRemoveP2pBssDesc(prAdapter, prBssDesc);
	}
}				/* scanRemoveAllP2pBssDesc */

void scanRemoveP2pBssDesc(IN struct ADAPTER *prAdapter,
		IN struct BSS_DESC *prBssDesc)
{
}				/* scanRemoveP2pBssDesc */

struct BSS_DESC *scanP2pSearchDesc(IN struct ADAPTER *prAdapter,
		IN struct P2P_CONNECTION_REQ_INFO *prConnReqInfo)
{
	struct BSS_DESC *prCandidateBssDesc = (struct BSS_DESC *) NULL,
		*prBssDesc = (struct BSS_DESC *) NULL;
	struct LINK *prBssDescList = (struct LINK *) NULL;

	do {
		if ((prAdapter == NULL) || (prConnReqInfo == NULL))
			break;

		prBssDescList = &(prAdapter->rWifiVar.rScanInfo.rBSSDescList);

		DBGLOG(P2P, LOUD,
			"Connecting to BSSID: " MACSTR "\n",
			MAC2STR(prConnReqInfo->aucBssid));
		DBGLOG(P2P, LOUD,
			"Connecting to SSID:%s, length:%d\n",
			prConnReqInfo->rSsidStruct.aucSsid,
			prConnReqInfo->rSsidStruct.ucSsidLen);

		LINK_FOR_EACH_ENTRY(prBssDesc, prBssDescList,
			rLinkEntry, struct BSS_DESC) {
			DBGLOG(P2P, LOUD,
				"Checking BSS: " MACSTR "\n",
				MAC2STR(prBssDesc->aucBSSID));

			if (prBssDesc->eBSSType != BSS_TYPE_INFRASTRUCTURE) {
				DBGLOG(P2P, LOUD,
					"Ignore mismatch BSS type.\n");
				continue;
			}

			if (UNEQUAL_MAC_ADDR(prBssDesc->aucBSSID,
				prConnReqInfo->aucBssid)) {
				DBGLOG(P2P, LOUD, "Ignore mismatch BSSID.\n");
				continue;
			}

			/* SSID should be the same?
			 * SSID is vary for each connection. so...
			 */
			if (UNEQUAL_SSID(prConnReqInfo->rSsidStruct.aucSsid,
					 prConnReqInfo->rSsidStruct.ucSsidLen,
					 prBssDesc->aucSSID,
					 prBssDesc->ucSSIDLen)) {

				DBGLOG(P2P, TRACE,
					"Connecting to BSSID: " MACSTR "\n",
					MAC2STR(prConnReqInfo->aucBssid));
				DBGLOG(P2P, TRACE,
					"Connecting to SSID:%s, length:%d\n",
					prConnReqInfo->rSsidStruct.aucSsid,
					prConnReqInfo->rSsidStruct.ucSsidLen);
				DBGLOG(P2P, TRACE,
					"Checking SSID:%s, length:%d\n",
					prBssDesc->aucSSID,
					prBssDesc->ucSSIDLen);
				DBGLOG(P2P, TRACE,
					"Ignore mismatch SSID, (But BSSID match).\n");
				/* ASSERT(FALSE); *//*let p2p re-scan again */
				continue;
			}

			if (!prBssDesc->fgIsP2PPresent) {
				DBGLOG(P2P, ERROR,
					"SSID, BSSID, BSSTYPE match, but no P2P IE present.\n");
				continue;
			}

			/* Final decision. */
			prCandidateBssDesc = prBssDesc;
			break;
		}

	} while (FALSE);

	return prCandidateBssDesc;
}				/* scanP2pSearchDesc */
