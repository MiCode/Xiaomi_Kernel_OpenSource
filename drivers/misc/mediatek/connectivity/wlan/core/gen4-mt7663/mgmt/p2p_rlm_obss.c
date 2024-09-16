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
 ** Id: @(#) gl_p2p_cfg80211.c@@
 */

/*! \file   gl_p2p_cfg80211.c
 *    \brief  Main routines of Linux driver interface for Wi-Fi Direct
 *	    using cfg80211 interface
 *
 *    This file contains the main routines of Linux driver
 *    for MediaTek Inc. 802.11
 *    Wireless LAN Adapters.
 */

/******************************************************************************
 *                         C O M P I L E R   F L A G S
 ******************************************************************************
 */

/******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 ******************************************************************************
 */

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

#include "precomp.h"

static uint8_t rlmObssChnlLevelIn2G4(struct BSS_INFO *prBssInfo,
		uint8_t ucPriChannel, enum ENUM_CHNL_EXT eExtend);

static uint8_t rlmObssChnlLevelIn5G(struct BSS_INFO *prBssInfo,
		uint8_t ucPriChannel, enum ENUM_CHNL_EXT eExtend);

/*----------------------------------------------------------------------------*/
/*!
 * \brief Different concurrent network has itself channel lists, and
 *        concurrent networks should have been recorded in channel lists.
 *        If role of active P2P is GO, assume associated AP of AIS will
 *        record our Beacon for P2P GO because of same channel.
 *
 *        Note: If we have scenario of different channel in the future,
 *              the internal FW communication channel shall be established.
 *
 * \param[in]
 *
 * \return none
 */
/*----------------------------------------------------------------------------*/
uint8_t rlmObssChnlLevel(struct BSS_INFO *prBssInfo,
		enum ENUM_BAND eBand,
		uint8_t ucPriChannel,
		enum ENUM_CHNL_EXT eExtend)
{
	uint8_t ucChannelLevel;

	ASSERT(prBssInfo);

	if (eBand == BAND_2G4) {
		ucChannelLevel =
			rlmObssChnlLevelIn2G4(prBssInfo, ucPriChannel, eExtend);

		/* (TBD) If concurrent networks permit different channel, extra
		 *       channel judgement should be added. Please refer to
		 *       previous version of this file.
		 */
	} else if (eBand == BAND_5G) {
		ucChannelLevel =
			rlmObssChnlLevelIn5G(prBssInfo, ucPriChannel, eExtend);

		/* (TBD) If concurrent networks permit different channel, extra
		 *       channel judgement should be added. Please refer to
		 *       previous version of this file.
		 */
	} else {
		ucChannelLevel = CHNL_LEVEL0;
	}

	return ucChannelLevel;
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
static uint8_t rlmObssChnlLevelIn2G4(struct BSS_INFO *prBssInfo,
		uint8_t ucPriChannel,
		enum ENUM_CHNL_EXT eExtend)
{
	uint8_t i, ucChannelLevel;
	uint8_t ucSecChannel, ucCenterChannel;
	uint8_t ucAffectedChnl_L, ucAffectedChnl_H;

	ASSERT(prBssInfo);

	ucChannelLevel = CHNL_LEVEL2;

	/* Calculate center channel for 2.4G band */
	if (eExtend == CHNL_EXT_SCA) {
		ucCenterChannel = ucPriChannel + 2;
		ucSecChannel = ucPriChannel + 4;
	} else if (eExtend == CHNL_EXT_SCB) {
		ucCenterChannel = ucPriChannel - 2;
		ucSecChannel = ucPriChannel - 4;
	} else {
		return CHNL_LEVEL0;
	}
	ASSERT(ucCenterChannel >= 1 && ucCenterChannel <= 14);

	/* Calculated low/upper channels in affected freq range */
	ucAffectedChnl_L =
		(ucCenterChannel <= AFFECTED_CHNL_OFFSET)
		? 1
		: (ucCenterChannel - AFFECTED_CHNL_OFFSET);

	ucAffectedChnl_H = (ucCenterChannel >= (14 - AFFECTED_CHNL_OFFSET))
		? 14
		: (ucCenterChannel + AFFECTED_CHNL_OFFSET);

	/* Check intolerant (Non-HT) channel list */
	ASSERT(prBssInfo->auc2G_NonHtChnlList[0] <= CHNL_LIST_SZ_2G);
	for (i = 1; i <= prBssInfo->auc2G_NonHtChnlList[0]
		&& i <= CHNL_LIST_SZ_2G; i++) {
		if ((prBssInfo->auc2G_NonHtChnlList[i] >= ucAffectedChnl_L
			&&
			prBssInfo->auc2G_NonHtChnlList[i] <= ucAffectedChnl_H)
			&&
			prBssInfo->auc2G_NonHtChnlList[i] != ucPriChannel) {

			ucChannelLevel = CHNL_LEVEL0;
			goto L_2G4_level_end;
		}
	}

	/* Check 20M BW request channel list */
	ASSERT(prBssInfo->auc2G_20mReqChnlList[0] <= CHNL_LIST_SZ_2G);
	for (i = 1; i <= prBssInfo->auc2G_20mReqChnlList[0]
		&& i <= CHNL_LIST_SZ_2G; i++) {
		if ((prBssInfo->auc2G_20mReqChnlList[i] >= ucAffectedChnl_L
			&&
			prBssInfo->auc2G_20mReqChnlList[i]
			<= ucAffectedChnl_H)) {

			ucChannelLevel = CHNL_LEVEL0;
			goto L_2G4_level_end;
		}
	}

	/* Check 2.4G primary channel list */
	ASSERT(prBssInfo->auc2G_PriChnlList[0] <= CHNL_LIST_SZ_2G);
	for (i = 1; i <= prBssInfo->auc2G_PriChnlList[0]
		&& i <= CHNL_LIST_SZ_2G; i++) {
		if ((prBssInfo->auc2G_PriChnlList[i] >= ucAffectedChnl_L
			&& prBssInfo->auc2G_PriChnlList[i] <= ucAffectedChnl_H)
			&& prBssInfo->auc2G_PriChnlList[i] != ucPriChannel) {

			ucChannelLevel = CHNL_LEVEL0;
			goto L_2G4_level_end;
		}
	}

	/* Check 2.4G secondary channel list */
	ASSERT(prBssInfo->auc2G_SecChnlList[0] <= CHNL_LIST_SZ_2G);
	for (i = 1; i <= prBssInfo->auc2G_SecChnlList[0]
		&& i <= CHNL_LIST_SZ_2G; i++) {
		if ((prBssInfo->auc2G_SecChnlList[i] >= ucAffectedChnl_L
			&& prBssInfo->auc2G_SecChnlList[i] <= ucAffectedChnl_H)
			&& prBssInfo->auc2G_SecChnlList[i] != ucSecChannel) {

			ucChannelLevel = CHNL_LEVEL0;
			goto L_2G4_level_end;
		}
	}

L_2G4_level_end:

	return ucChannelLevel;
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
static uint8_t rlmObssChnlLevelIn5G(struct BSS_INFO *prBssInfo,
		uint8_t ucPriChannel,
		enum ENUM_CHNL_EXT eExtend)
{
	uint8_t i, ucChannelLevel;
	uint8_t ucSecChannel;

	ASSERT(prBssInfo);

	ucChannelLevel = CHNL_LEVEL2;

	/* Calculate center channel for 2.4G band */
	if (eExtend == CHNL_EXT_SCA)
		ucSecChannel = ucPriChannel + 4;
	else if (eExtend == CHNL_EXT_SCB)
		ucSecChannel = ucPriChannel - 4;
	else
		return CHNL_LEVEL0;
	ASSERT(ucSecChannel >= 36);

	/* Check 5G primary channel list */
	ASSERT(prBssInfo->auc5G_PriChnlList[0] <= CHNL_LIST_SZ_5G);
	for (i = 1; i <= prBssInfo->auc5G_PriChnlList[0]
		&& i <= CHNL_LIST_SZ_5G; i++) {
		if (prBssInfo->auc5G_PriChnlList[i] == ucSecChannel) {

			ucChannelLevel = CHNL_LEVEL0;
			goto L_5G_level_end;
		} else if (prBssInfo->auc5G_PriChnlList[i] == ucPriChannel) {
			ucChannelLevel = CHNL_LEVEL1;
		}
	}

	/* Check non-HT channel list */
	ASSERT(prBssInfo->auc5G_NonHtChnlList[0] <= CHNL_LIST_SZ_5G);
	for (i = 1; i <= prBssInfo->auc5G_NonHtChnlList[0]
		&& i <= CHNL_LIST_SZ_5G; i++) {
		if (prBssInfo->auc5G_NonHtChnlList[i] == ucSecChannel) {

			ucChannelLevel = CHNL_LEVEL0;
			goto L_5G_level_end;
		} else if (prBssInfo->auc5G_NonHtChnlList[i] == ucPriChannel) {
			ucChannelLevel = CHNL_LEVEL1;
		}
	}

	/* Check secondary channel list */
	ASSERT(prBssInfo->auc5G_SecChnlList[0] <= CHNL_LIST_SZ_5G);
	for (i = 1; i <= prBssInfo->auc5G_SecChnlList[0]
		&& i <= CHNL_LIST_SZ_5G; i++) {
		if (prBssInfo->auc5G_SecChnlList[i] == ucPriChannel) {

			ucChannelLevel = CHNL_LEVEL0;
			goto L_5G_level_end;
		}
	}

L_5G_level_end:

	return ucChannelLevel;
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
void rlmObssScanExemptionRsp(struct ADAPTER *prAdapter,
		struct BSS_INFO *prBssInfo,
		struct SW_RFB *prSwRfb)
{
	struct MSDU_INFO *prMsduInfo;
	struct ACTION_20_40_COEXIST_FRAME *prTxFrame;

	/* To do: need an algorithm to do judgement.
	 * Now always reject request
	 */

	prMsduInfo = (struct MSDU_INFO *)
	    cnmMgtPktAlloc(prAdapter, PUBLIC_ACTION_MAX_LEN);
	if (prMsduInfo == NULL)
		return;

	DBGLOG(RLM, INFO, "Send 20/40 coexistence rsp frame!\n");

	prTxFrame = (struct ACTION_20_40_COEXIST_FRAME *) prMsduInfo->prPacket;

	prTxFrame->u2FrameCtrl = MAC_FRAME_ACTION;
	COPY_MAC_ADDR(prTxFrame->aucDestAddr,
		((struct ACTION_20_40_COEXIST_FRAME *)
			prSwRfb->pvHeader)->aucSrcAddr);
	COPY_MAC_ADDR(prTxFrame->aucSrcAddr,
		prBssInfo->aucOwnMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucBSSID,
		prBssInfo->aucBSSID);

	prTxFrame->ucCategory = CATEGORY_PUBLIC_ACTION;
	prTxFrame->ucAction = ACTION_PUBLIC_20_40_COEXIST;

	/* To do: find correct algorithm */
	prTxFrame->rBssCoexist.ucId = ELEM_ID_20_40_BSS_COEXISTENCE;
	prTxFrame->rBssCoexist.ucLength = 1;
	prTxFrame->rBssCoexist.ucData = 0;

	ASSERT((WLAN_MAC_HEADER_LEN + 5) <= PUBLIC_ACTION_MAX_LEN);

	TX_SET_MMPDU(prAdapter,
		     prMsduInfo,
		     prBssInfo->ucBssIndex,
		     prSwRfb->ucStaRecIdx,
		     WLAN_MAC_MGMT_HEADER_LEN, WLAN_MAC_MGMT_HEADER_HTC_LEN + 5,
		     NULL, MSDU_RATE_MODE_AUTO);

	/* Send them to HW queue */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);
}
