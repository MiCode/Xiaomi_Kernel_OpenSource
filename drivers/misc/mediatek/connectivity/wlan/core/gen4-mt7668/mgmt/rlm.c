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
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/mgmt/rlm.c#3
*/

/*! \file   "rlm.c"
*    \brief
*
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
BOOLEAN g_bCaptureDone = FALSE;
BOOLEAN g_bIcapEnable = FALSE;
UINT_16 g_u2DumpIndex;
BOOLEAN g_fgHasStopTx = FALSE;

#if CFG_SUPPORT_QA_TOOL
UINT_32 g_au4Offset[2][2];
UINT_32 g_au4IQData[256];
#endif

#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST
RLM_CAL_RESULT_ALL_V2_T			g_rBackupCalDataAllV2;
#endif

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
static VOID rlmFillHtCapIE(P_ADAPTER_T prAdapter, P_BSS_INFO_T prBssInfo, P_MSDU_INFO_T prMsduInfo);

static VOID rlmFillExtCapIE(P_ADAPTER_T prAdapter, P_BSS_INFO_T prBssInfo, P_MSDU_INFO_T prMsduInfo);

static VOID rlmFillHtOpIE(P_ADAPTER_T prAdapter, P_BSS_INFO_T prBssInfo, P_MSDU_INFO_T prMsduInfo);

static UINT_8 rlmRecIeInfoForClient(P_ADAPTER_T prAdapter, P_BSS_INFO_T prBssInfo, PUINT_8 pucIE, UINT_16 u2IELength);

static BOOLEAN
rlmRecBcnFromNeighborForClient(P_ADAPTER_T prAdapter,
			       P_BSS_INFO_T prBssInfo, P_SW_RFB_T prSwRfb, PUINT_8 pucIE, UINT_16 u2IELength);

static BOOLEAN
rlmRecBcnInfoForClient(P_ADAPTER_T prAdapter,
		       P_BSS_INFO_T prBssInfo, P_SW_RFB_T prSwRfb, PUINT_8 pucIE, UINT_16 u2IELength);

static VOID rlmBssReset(P_ADAPTER_T prAdapter, P_BSS_INFO_T prBssInfo);

#if CFG_SUPPORT_802_11AC
static VOID rlmFillVhtCapIE(P_ADAPTER_T prAdapter, P_BSS_INFO_T prBssInfo, P_MSDU_INFO_T prMsduInfo);
static VOID rlmFillVhtOpNotificationIE(P_ADAPTER_T prAdapter, P_BSS_INFO_T prBssInfo,
		P_MSDU_INFO_T prMsduInfo, BOOLEAN fgIsMaxCap);

#endif
/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID rlmFsmEventInit(P_ADAPTER_T prAdapter)
{
	ASSERT(prAdapter);

	/* Note: assume TIMER_T structures are reset to zero or stopped
	 * before invoking this function.
	 */

	/* Initialize OBSS FSM */
	rlmObssInit(prAdapter);

#if CFG_SUPPORT_PWR_LIMIT_COUNTRY
	rlmDomainCheckCountryPowerLimitTable(prAdapter);
#endif

	g_bCaptureDone = FALSE;
	g_bIcapEnable = FALSE;
	g_u2DumpIndex = 0;
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
VOID rlmFsmEventUninit(P_ADAPTER_T prAdapter)
{
	P_BSS_INFO_T prBssInfo;
	UINT_8 i;

	ASSERT(prAdapter);

	for (i = 0; i < BSS_INFO_NUM; i++) {
		prBssInfo = prAdapter->aprBssInfo[i];

		/* Note: all RLM timers will also be stopped.
		 *       Now only one OBSS scan timer.
		 */
		rlmBssReset(prAdapter, prBssInfo);
	}
}

/*----------------------------------------------------------------------------*/
/*!
* \brief For association request, power capability
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID rlmReqGeneratePowerCapIE(P_ADAPTER_T prAdapter, P_MSDU_INFO_T prMsduInfo)
{
	PUINT_8 pucBuffer;
	P_BSS_INFO_T prBssInfo;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prMsduInfo->ucBssIndex);

	/* We should add power capability IE in assoc/reassoc req if the spectrum
	 * management bit is set to 1 in Capability Infor field, or the connection
	 * will be rejected by Marvell APs in some TGn items. (e.g. 5.2.32).
	 * Spectrum management related feature (802.11h) is for 5G band.
	 */
	if (!prBssInfo || prBssInfo->eBand != BAND_5G)
		return;

	pucBuffer = (PUINT_8) ((ULONG) prMsduInfo->prPacket + (ULONG) prMsduInfo->u2FrameLength);

	POWER_CAP_IE(pucBuffer)->ucId = ELEM_ID_PWR_CAP;
	POWER_CAP_IE(pucBuffer)->ucLength = ELEM_MAX_LEN_POWER_CAP;
	POWER_CAP_IE(pucBuffer)->cMinTxPowerCap = 15;
	POWER_CAP_IE(pucBuffer)->cMaxTxPowerCap = 20;

	prMsduInfo->u2FrameLength += IE_SIZE(pucBuffer);
	pucBuffer += IE_SIZE(pucBuffer);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief For association request, supported channels
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID rlmReqGenerateSupportedChIE(P_ADAPTER_T prAdapter, P_MSDU_INFO_T prMsduInfo)
{
	PUINT_8 pucBuffer;
	P_BSS_INFO_T prBssInfo;
	RF_CHANNEL_INFO_T auc2gChannelList[MAX_2G_BAND_CHN_NUM];
	RF_CHANNEL_INFO_T auc5gChannelList[MAX_5G_BAND_CHN_NUM];
	UINT_8 ucNumOf2gChannel = 0;
	UINT_8 ucNumOf5gChannel = 0;
	UINT_8 ucChIdx = 0;
	UINT_8 ucIdx = 0;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prMsduInfo->ucBssIndex);

	/* We should add supported channels IE in assoc/reassoc req if the spectrum
	 * management bit is set to 1 in Capability Infor field, or the connection
	 * will be rejected by Marvell APs in some TGn items. (e.g. 5.2.3).
	 * Spectrum management related feature (802.11h) is for 5G band.
	 */
	if (!prBssInfo || prBssInfo->eBand != BAND_5G)
		return;


	pucBuffer = (PUINT_8) ((ULONG) prMsduInfo->prPacket + (ULONG) prMsduInfo->u2FrameLength);

	rlmDomainGetChnlList(prAdapter, BAND_2G4, TRUE,
					 MAX_2G_BAND_CHN_NUM, &ucNumOf2gChannel, auc2gChannelList);
	rlmDomainGetChnlList(prAdapter, BAND_5G, TRUE,
					 MAX_5G_BAND_CHN_NUM, &ucNumOf5gChannel, auc5gChannelList);

	SUP_CH_IE(pucBuffer)->ucId = ELEM_ID_SUP_CHS;
	SUP_CH_IE(pucBuffer)->ucLength = (ucNumOf2gChannel + ucNumOf5gChannel) * 2;

	for (ucIdx = 0; ucIdx < ucNumOf2gChannel; ucIdx++, ucChIdx += 2) {
		SUP_CH_IE(pucBuffer)->ucChannelNum[ucChIdx] =
			auc2gChannelList[ucIdx].ucChannelNum;
		SUP_CH_IE(pucBuffer)->ucChannelNum[ucChIdx + 1] = 1;
	}

	for (ucIdx = 0; ucIdx < ucNumOf5gChannel; ucIdx++, ucChIdx += 2) {
		SUP_CH_IE(pucBuffer)->ucChannelNum[ucChIdx] =
			auc5gChannelList[ucIdx].ucChannelNum;
		SUP_CH_IE(pucBuffer)->ucChannelNum[ucChIdx + 1] = 1;
	}

	prMsduInfo->u2FrameLength += IE_SIZE(pucBuffer);
	pucBuffer += IE_SIZE(pucBuffer);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief For probe request, association request
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID rlmReqGenerateHtCapIE(P_ADAPTER_T prAdapter, P_MSDU_INFO_T prMsduInfo)
{
	P_BSS_INFO_T prBssInfo;
	P_STA_RECORD_T prStaRec;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

	prBssInfo = prAdapter->aprBssInfo[prMsduInfo->ucBssIndex];
	if (!prBssInfo)
		return;

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

	if ((prAdapter->rWifiVar.ucAvailablePhyTypeSet & PHY_TYPE_SET_802_11N) &&
	    (!prStaRec || (prStaRec->ucPhyTypeSet & PHY_TYPE_SET_802_11N)))
		rlmFillHtCapIE(prAdapter, prBssInfo, prMsduInfo);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief For probe request, association request
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID rlmReqGenerateExtCapIE(P_ADAPTER_T prAdapter, P_MSDU_INFO_T prMsduInfo)
{
	P_BSS_INFO_T prBssInfo;
	P_STA_RECORD_T prStaRec;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

	prBssInfo = prAdapter->aprBssInfo[prMsduInfo->ucBssIndex];
	if (!prBssInfo)
		return;

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

	if ((prAdapter->rWifiVar.ucAvailablePhyTypeSet & PHY_TYPE_SET_802_11N) &&
	    (!prStaRec || (prStaRec->ucPhyTypeSet & PHY_TYPE_SET_802_11N)))
		rlmFillExtCapIE(prAdapter, prBssInfo, prMsduInfo);
#if CFG_SUPPORT_PASSPOINT
	else if (prAdapter->prGlueInfo->fgConnectHS20AP == TRUE)
		hs20FillExtCapIE(prAdapter, prBssInfo, prMsduInfo);
#endif /* CFG_SUPPORT_PASSPOINT */
}

/*----------------------------------------------------------------------------*/
/*!
* \brief For probe response (GO, IBSS) and association response
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID rlmRspGenerateHtCapIE(P_ADAPTER_T prAdapter, P_MSDU_INFO_T prMsduInfo)
{
	P_BSS_INFO_T prBssInfo;
	P_STA_RECORD_T prStaRec;
	UINT_8 ucPhyTypeSet;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

	prBssInfo = prAdapter->aprBssInfo[prMsduInfo->ucBssIndex];
	if (!prBssInfo)
		return;

	if (!IS_BSS_ACTIVE(prBssInfo))
		return;

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

	/* Decide PHY type set source */
	if (prStaRec) {
		/* Get PHY type set from target STA */
		ucPhyTypeSet = prStaRec->ucPhyTypeSet;
	} else {
		/* Get PHY type set from current BSS */
		ucPhyTypeSet = prBssInfo->ucPhyTypeSet;
	}

	if (RLM_NET_IS_11N(prBssInfo) && (ucPhyTypeSet & PHY_TYPE_SET_802_11N) &&
		(!prBssInfo->fgIsWepCipherGroup))
		rlmFillHtCapIE(prAdapter, prBssInfo, prMsduInfo);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief For probe response (GO, IBSS) and association response
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID rlmRspGenerateExtCapIE(P_ADAPTER_T prAdapter, P_MSDU_INFO_T prMsduInfo)
{
	P_BSS_INFO_T prBssInfo;
	P_STA_RECORD_T prStaRec;
	UINT_8 ucPhyTypeSet;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

	prBssInfo = prAdapter->aprBssInfo[prMsduInfo->ucBssIndex];
	if (!prBssInfo)
		return;

	if (!IS_BSS_ACTIVE(prBssInfo))
		return;

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

	/* Decide PHY type set source */
	if (prStaRec) {
		/* Get PHY type set from target STA */
		ucPhyTypeSet = prStaRec->ucPhyTypeSet;
	} else {
		/* Get PHY type set from current BSS */
		ucPhyTypeSet = prBssInfo->ucPhyTypeSet;
	}

	if (RLM_NET_IS_11N(prBssInfo) && (ucPhyTypeSet & PHY_TYPE_SET_802_11N))
		rlmFillExtCapIE(prAdapter, prBssInfo, prMsduInfo);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief For probe response (GO, IBSS) and association response
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID rlmRspGenerateHtOpIE(P_ADAPTER_T prAdapter, P_MSDU_INFO_T prMsduInfo)
{
	P_BSS_INFO_T prBssInfo;
	P_STA_RECORD_T prStaRec;
	UINT_8 ucPhyTypeSet;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

	prBssInfo = prAdapter->aprBssInfo[prMsduInfo->ucBssIndex];
	if (!prBssInfo)
		return;

	if (!IS_BSS_ACTIVE(prBssInfo))
		return;

	/* Decide PHY type set source */
	if (prStaRec) {
		/* Get PHY type set from target STA */
		ucPhyTypeSet = prStaRec->ucPhyTypeSet;
	} else {
		/* Get PHY type set from current BSS */
		ucPhyTypeSet = prBssInfo->ucPhyTypeSet;
	}

	if (RLM_NET_IS_11N(prBssInfo) && (ucPhyTypeSet & PHY_TYPE_SET_802_11N) &&
		(!prBssInfo->fgIsWepCipherGroup))
		rlmFillHtOpIE(prAdapter, prBssInfo, prMsduInfo);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief For probe response (GO, IBSS) and association response
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID rlmRspGenerateErpIE(P_ADAPTER_T prAdapter, P_MSDU_INFO_T prMsduInfo)
{
	P_BSS_INFO_T prBssInfo;
	P_STA_RECORD_T prStaRec;
	P_IE_ERP_T prErpIe;
	UINT_8 ucPhyTypeSet;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

	prBssInfo = prAdapter->aprBssInfo[prMsduInfo->ucBssIndex];
	if (!prBssInfo)
		return;

	if (!IS_BSS_ACTIVE(prBssInfo))
		return;

	/* Decide PHY type set source */
	if (prStaRec) {
		/* Get PHY type set from target STA */
		ucPhyTypeSet = prStaRec->ucPhyTypeSet;
	} else {
		/* Get PHY type set from current BSS */
		ucPhyTypeSet = prBssInfo->ucPhyTypeSet;
	}

	if (RLM_NET_IS_11GN(prBssInfo) && prBssInfo->eBand == BAND_2G4 && (ucPhyTypeSet & PHY_TYPE_SET_802_11GN)) {
		prErpIe = (P_IE_ERP_T)
		    (((PUINT_8) prMsduInfo->prPacket) + prMsduInfo->u2FrameLength);

		/* Add ERP IE */
		prErpIe->ucId = ELEM_ID_ERP_INFO;
		prErpIe->ucLength = 1;

		prErpIe->ucERP = prBssInfo->fgObssErpProtectMode ? ERP_INFO_USE_PROTECTION : 0;

		if (prBssInfo->fgErpProtectMode)
			prErpIe->ucERP |= (ERP_INFO_NON_ERP_PRESENT | ERP_INFO_USE_PROTECTION);

		/* Handle barker preamble */
		if (!prBssInfo->fgUseShortPreamble)
			prErpIe->ucERP |= ERP_INFO_BARKER_PREAMBLE_MODE;

		ASSERT(IE_SIZE(prErpIe) <= (ELEM_HDR_LEN + ELEM_MAX_LEN_ERP));

		prMsduInfo->u2FrameLength += IE_SIZE(prErpIe);
	}
}

#if CFG_SUPPORT_MTK_SYNERGY
/*----------------------------------------------------------------------------*/
/*!
* \brief This function is used to generate MTK Vendor Specific OUI
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID rlmGenerateMTKOuiIE(P_ADAPTER_T prAdapter, P_MSDU_INFO_T prMsduInfo)
{
	P_BSS_INFO_T prBssInfo;
	PUINT_8 pucBuffer;
	UINT_8 aucMtkOui[] = VENDOR_OUI_MTK;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

	if (prAdapter->rWifiVar.ucMtkOui == FEATURE_DISABLED)
		return;

	prBssInfo = prAdapter->aprBssInfo[prMsduInfo->ucBssIndex];
	if (!prBssInfo)
		return;

	pucBuffer = (PUINT_8) ((ULONG) prMsduInfo->prPacket + (ULONG) prMsduInfo->u2FrameLength);

	MTK_OUI_IE(pucBuffer)->ucId = ELEM_ID_VENDOR;
	MTK_OUI_IE(pucBuffer)->ucLength = ELEM_MIN_LEN_MTK_OUI;
	MTK_OUI_IE(pucBuffer)->aucOui[0] = aucMtkOui[0];
	MTK_OUI_IE(pucBuffer)->aucOui[1] = aucMtkOui[1];
	MTK_OUI_IE(pucBuffer)->aucOui[2] = aucMtkOui[2];
	MTK_OUI_IE(pucBuffer)->aucCapability[0] = MTK_SYNERGY_CAP0 & (prAdapter->rWifiVar.aucMtkFeature[0]);
	MTK_OUI_IE(pucBuffer)->aucCapability[1] = MTK_SYNERGY_CAP1 & (prAdapter->rWifiVar.aucMtkFeature[1]);
	MTK_OUI_IE(pucBuffer)->aucCapability[2] = MTK_SYNERGY_CAP2 & (prAdapter->rWifiVar.aucMtkFeature[2]);
	MTK_OUI_IE(pucBuffer)->aucCapability[3] = MTK_SYNERGY_CAP3 & (prAdapter->rWifiVar.aucMtkFeature[3]);

	prMsduInfo->u2FrameLength += IE_SIZE(pucBuffer);
	pucBuffer += IE_SIZE(pucBuffer);
}				/* rlmGenerateMTKOuiIE */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is used to check MTK Vendor Specific OUI
*
*
* @return true:  correct MTK OUI
*             false: incorrect MTK OUI
*/
/*----------------------------------------------------------------------------*/
BOOLEAN rlmParseCheckMTKOuiIE(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucBuf, IN PUINT_32 pu4Cap)
{
	UINT_8 aucMtkOui[] = VENDOR_OUI_MTK;
	P_IE_MTK_OUI_T prMtkOuiIE = (P_IE_MTK_OUI_T) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (pucBuf != NULL));

		prMtkOuiIE = (P_IE_MTK_OUI_T) pucBuf;

		if (prAdapter->rWifiVar.ucMtkOui == FEATURE_DISABLED)
			break;
		else if (IE_LEN(pucBuf) < ELEM_MIN_LEN_MTK_OUI)
			break;
		else if (prMtkOuiIE->aucOui[0] != aucMtkOui[0] ||
			 prMtkOuiIE->aucOui[1] != aucMtkOui[1] || prMtkOuiIE->aucOui[2] != aucMtkOui[2])
			break;
		/* apply NvRam setting */
		prMtkOuiIE->aucCapability[0] = prMtkOuiIE->aucCapability[0] & (prAdapter->rWifiVar.aucMtkFeature[0]);
		prMtkOuiIE->aucCapability[1] = prMtkOuiIE->aucCapability[1] & (prAdapter->rWifiVar.aucMtkFeature[1]);
		prMtkOuiIE->aucCapability[2] = prMtkOuiIE->aucCapability[2] & (prAdapter->rWifiVar.aucMtkFeature[2]);
		prMtkOuiIE->aucCapability[3] = prMtkOuiIE->aucCapability[3] & (prAdapter->rWifiVar.aucMtkFeature[3]);

		kalMemCopy(pu4Cap, prMtkOuiIE->aucCapability, sizeof(UINT_32));

		return TRUE;
	} while (FALSE);

	return FALSE;
}				/* rlmParseCheckMTKOuiIE */

#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID rlmGenerateCsaIE(P_ADAPTER_T prAdapter, P_MSDU_INFO_T prMsduInfo)
{
	PUINT_8 pucBuffer;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

	if (prAdapter->rWifiVar.fgCsaInProgress) {

		pucBuffer = (PUINT_8) ((ULONG) prMsduInfo->prPacket + (ULONG) prMsduInfo->u2FrameLength);

		CSA_IE(pucBuffer)->ucId = ELEM_ID_CH_SW_ANNOUNCEMENT;
		CSA_IE(pucBuffer)->ucLength = ELEM_MIN_LEN_CSA;
		CSA_IE(pucBuffer)->ucChannelSwitchMode = prAdapter->rWifiVar.ucChannelSwitchMode;
		CSA_IE(pucBuffer)->ucNewChannelNum = prAdapter->rWifiVar.ucNewChannelNumber;
		CSA_IE(pucBuffer)->ucChannelSwitchCount = prAdapter->rWifiVar.ucChannelSwitchCount;

		prMsduInfo->u2FrameLength += IE_SIZE(pucBuffer);
		pucBuffer += IE_SIZE(pucBuffer);
	}

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
static VOID rlmFillHtCapIE(P_ADAPTER_T prAdapter, P_BSS_INFO_T prBssInfo, P_MSDU_INFO_T prMsduInfo)
{
	P_IE_HT_CAP_T prHtCap;
	P_SUP_MCS_SET_FIELD prSupMcsSet;
	BOOLEAN fg40mAllowed;
	UINT_8 ucIdx;

	ASSERT(prAdapter);
	ASSERT(prBssInfo);
	ASSERT(prMsduInfo);

	fg40mAllowed = prBssInfo->fgAssoc40mBwAllowed;

	prHtCap = (P_IE_HT_CAP_T)
	    (((PUINT_8) prMsduInfo->prPacket) + prMsduInfo->u2FrameLength);

	/* Add HT capabilities IE */
	prHtCap->ucId = ELEM_ID_HT_CAP;
	prHtCap->ucLength = sizeof(IE_HT_CAP_T) - ELEM_HDR_LEN;

	prHtCap->u2HtCapInfo = HT_CAP_INFO_DEFAULT_VAL;

	if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucRxShortGI))
		prHtCap->u2HtCapInfo |= (HT_CAP_INFO_SHORT_GI_20M | HT_CAP_INFO_SHORT_GI_40M);

	if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucRxLdpc))
		prHtCap->u2HtCapInfo |= HT_CAP_INFO_LDPC_CAP;

	if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucTxStbc))
		prHtCap->u2HtCapInfo |= HT_CAP_INFO_TX_STBC;

	if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucRxStbc)) {

		UINT_8 tempRxStbcNss;

		tempRxStbcNss = prAdapter->rWifiVar.ucRxStbcNss;
		tempRxStbcNss = (tempRxStbcNss > wlanGetSupportNss(prAdapter, prBssInfo->ucBssIndex)) ?
			wlanGetSupportNss(prAdapter, prBssInfo->ucBssIndex) : (tempRxStbcNss);
		if (tempRxStbcNss != prAdapter->rWifiVar.ucRxStbcNss) {
			DBGLOG(RLM, WARN, "Apply Nss:%d as RxStbcNss in HT Cap",
				wlanGetSupportNss(prAdapter, prBssInfo->ucBssIndex));
			DBGLOG(RLM, WARN, " due to set RxStbcNss more than Nss is not appropriate.\n");
		}
		if (tempRxStbcNss == 1)
			prHtCap->u2HtCapInfo |= HT_CAP_INFO_RX_STBC_1_SS;
		else if (tempRxStbcNss == 2)
			prHtCap->u2HtCapInfo |= HT_CAP_INFO_RX_STBC_2_SS;
		else if (tempRxStbcNss >= 3)
			prHtCap->u2HtCapInfo |= HT_CAP_INFO_RX_STBC_3_SS;
	}

	if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucRxGf))
		prHtCap->u2HtCapInfo |= HT_CAP_INFO_HT_GF;

	if (prAdapter->rWifiVar.ucRxMaxMpduLen > VHT_CAP_INFO_MAX_MPDU_LEN_3K)
		prHtCap->u2HtCapInfo |= HT_CAP_INFO_MAX_AMSDU_LEN;

	if (!fg40mAllowed)
		prHtCap->u2HtCapInfo &= ~(HT_CAP_INFO_SUP_CHNL_WIDTH |
					  HT_CAP_INFO_SHORT_GI_40M | HT_CAP_INFO_DSSS_CCK_IN_40M);

	/* SM power saving */ /* TH3_Huang */
	if (prBssInfo->ucNss < wlanGetSupportNss(prAdapter, prBssInfo->ucBssIndex))
		prHtCap->u2HtCapInfo &= ~HT_CAP_INFO_SM_POWER_SAVE;/*Set as static power save */

	prHtCap->ucAmpduParam = AMPDU_PARAM_DEFAULT_VAL;

	prSupMcsSet = &prHtCap->rSupMcsSet;
	kalMemZero((PVOID)&prSupMcsSet->aucRxMcsBitmask[0], SUP_MCS_RX_BITMASK_OCTET_NUM);

	for (ucIdx = 0; ucIdx < wlanGetSupportNss(prAdapter, prBssInfo->ucBssIndex); ucIdx++)
		prSupMcsSet->aucRxMcsBitmask[ucIdx] = BITS(0, 7);

	/* prSupMcsSet->aucRxMcsBitmask[0] = BITS(0, 7); */

	if (fg40mAllowed && IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucMCS32))
		prSupMcsSet->aucRxMcsBitmask[32 / 8] = BIT(0);	/* MCS32 */
	prSupMcsSet->u2RxHighestSupportedRate = SUP_MCS_RX_DEFAULT_HIGHEST_RATE;
	prSupMcsSet->u4TxRateInfo = SUP_MCS_TX_DEFAULT_VAL;

	prHtCap->u2HtExtendedCap = HT_EXT_CAP_DEFAULT_VAL;
	if (!fg40mAllowed || prBssInfo->eCurrentOPMode != OP_MODE_INFRASTRUCTURE)
		prHtCap->u2HtExtendedCap &= ~(HT_EXT_CAP_PCO | HT_EXT_CAP_PCO_TRANS_TIME_NONE);

	prHtCap->u4TxBeamformingCap = TX_BEAMFORMING_CAP_DEFAULT_VAL;
	if ((prAdapter->rWifiVar.ucDbdcMode == DBDC_MODE_DISABLED) ||
		(prBssInfo->eBand == BAND_5G)) {
		if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucStaHtBfee))
			prHtCap->u4TxBeamformingCap = TX_BEAMFORMING_CAP_BFEE;
		if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucStaHtBfer))
			prHtCap->u4TxBeamformingCap |= TX_BEAMFORMING_CAP_BFER;
	}

	prHtCap->ucAselCap = ASEL_CAP_DEFAULT_VAL;

	ASSERT(IE_SIZE(prHtCap) <= (ELEM_HDR_LEN + ELEM_MAX_LEN_HT_CAP));

	prMsduInfo->u2FrameLength += IE_SIZE(prHtCap);
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
static VOID rlmFillExtCapIE(P_ADAPTER_T prAdapter, P_BSS_INFO_T prBssInfo, P_MSDU_INFO_T prMsduInfo)
{
	P_EXT_CAP_T prExtCap;
	BOOLEAN fg40mAllowed, fgAppendVhtCap;
	P_STA_RECORD_T prStaRec;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

	fg40mAllowed = prBssInfo->fgAssoc40mBwAllowed;

	/* Add Extended Capabilities IE */
	prExtCap = (P_EXT_CAP_T)
	    (((PUINT_8) prMsduInfo->prPacket) + prMsduInfo->u2FrameLength);

	prExtCap->ucId = ELEM_ID_EXTENDED_CAP;
#if 0				/* CFG_SUPPORT_HOTSPOT_2_0 */
	if (prAdapter->prGlueInfo->fgConnectHS20AP == TRUE)
		prExtCap->ucLength = ELEM_MAX_LEN_EXT_CAP;
	else
#endif
		prExtCap->ucLength = 1;

	/* Reset memory */
	kalMemZero(prExtCap->aucCapabilities, ELEM_MAX_LEN_EXT_CAP);

	prExtCap->aucCapabilities[0] = ELEM_EXT_CAP_DEFAULT_VAL;

	if (!fg40mAllowed)
		prExtCap->aucCapabilities[0] &= ~ELEM_EXT_CAP_20_40_COEXIST_SUPPORT;

	if (prBssInfo->eCurrentOPMode != OP_MODE_INFRASTRUCTURE)
		prExtCap->aucCapabilities[0] &= ~ELEM_EXT_CAP_PSMP_CAP;

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

#if CFG_SUPPORT_802_11AC
	fgAppendVhtCap = FALSE;

	/* Check append rule */
	if (prAdapter->rWifiVar.ucAvailablePhyTypeSet & PHY_TYPE_SET_802_11AC) {
		/* Note: For AIS connecting state, structure in BSS_INFO will not be inited */
		/*       So, we check StaRec instead of BssInfo */
		if (prStaRec) {
			if (prStaRec->ucPhyTypeSet & PHY_TYPE_SET_802_11AC)
				fgAppendVhtCap = TRUE;
		} else if ((RLM_NET_IS_11AC(prBssInfo)) && ((prBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE) ||
		(prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT)))
			fgAppendVhtCap = TRUE;
	}

	if (fgAppendVhtCap) {
		if (prExtCap->ucLength < ELEM_MAX_LEN_EXT_CAP)
			prExtCap->ucLength = ELEM_MAX_LEN_EXT_CAP;

		SET_EXT_CAP(prExtCap->aucCapabilities, ELEM_MAX_LEN_EXT_CAP, ELEM_EXT_CAP_OP_MODE_NOTIFICATION_BIT);

	}
#endif

#if CFG_SUPPORT_PASSPOINT
	if (prAdapter->prGlueInfo->fgConnectHS20AP == TRUE) {

		if (prExtCap->ucLength < ELEM_MAX_LEN_EXT_CAP)
			prExtCap->ucLength = ELEM_MAX_LEN_EXT_CAP;

		SET_EXT_CAP(prExtCap->aucCapabilities, ELEM_MAX_LEN_EXT_CAP, ELEM_EXT_CAP_INTERWORKING_BIT);

		/* For R2 WNM-Notification */
		SET_EXT_CAP(prExtCap->aucCapabilities, ELEM_MAX_LEN_EXT_CAP, ELEM_EXT_CAP_WNM_NOTIFICATION_BIT);
	}
#endif /* CFG_SUPPORT_PASSPOINT */

	ASSERT(IE_SIZE(prExtCap) <= (ELEM_HDR_LEN + ELEM_MAX_LEN_EXT_CAP));

	prMsduInfo->u2FrameLength += IE_SIZE(prExtCap);
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
static VOID rlmFillHtOpIE(P_ADAPTER_T prAdapter, P_BSS_INFO_T prBssInfo, P_MSDU_INFO_T prMsduInfo)
{
	P_IE_HT_OP_T prHtOp;
	UINT_16 i;

	ASSERT(prAdapter);
	ASSERT(prBssInfo);
	ASSERT(prMsduInfo);

	prHtOp = (P_IE_HT_OP_T)
	    (((PUINT_8) prMsduInfo->prPacket) + prMsduInfo->u2FrameLength);

	/* Add HT operation IE */
	prHtOp->ucId = ELEM_ID_HT_OP;
	prHtOp->ucLength = sizeof(IE_HT_OP_T) - ELEM_HDR_LEN;

	/* RIFS and 20/40 bandwidth operations are included */
	prHtOp->ucPrimaryChannel = prBssInfo->ucPrimaryChannel;
	prHtOp->ucInfo1 = prBssInfo->ucHtOpInfo1;

	/* Decide HT protection mode field */
	if (prBssInfo->eHtProtectMode == HT_PROTECT_MODE_NON_HT)
		prHtOp->u2Info2 = (UINT_8) HT_PROTECT_MODE_NON_HT;
	else if (prBssInfo->eObssHtProtectMode == HT_PROTECT_MODE_NON_MEMBER)
		prHtOp->u2Info2 = (UINT_8) HT_PROTECT_MODE_NON_MEMBER;
	else {
		/* It may be SYS_PROTECT_MODE_NONE or SYS_PROTECT_MODE_20M */
		prHtOp->u2Info2 = (UINT_8) prBssInfo->eHtProtectMode;
	}

	if (prBssInfo->eGfOperationMode != GF_MODE_NORMAL) {
		/* It may be GF_MODE_PROTECT or GF_MODE_DISALLOWED
		 * Note: it will also be set in ad-hoc network
		 */
		prHtOp->u2Info2 |= HT_OP_INFO2_NON_GF_HT_STA_PRESENT;
	}

	if (0 /* Regulatory class 16 */  &&
	    prBssInfo->eObssHtProtectMode == HT_PROTECT_MODE_NON_MEMBER) {
		/* (TBD) It is HT_PROTECT_MODE_NON_MEMBER, so require protection
		 * although it is possible to have no protection by spec.
		 */
		prHtOp->u2Info2 |= HT_OP_INFO2_OBSS_NON_HT_STA_PRESENT;
	}

	prHtOp->u2Info3 = prBssInfo->u2HtOpInfo3;	/* To do: handle L-SIG TXOP */

	/* No basic MCSx are needed temporarily */
	for (i = 0; i < 16; i++)
		prHtOp->aucBasicMcsSet[i] = 0;

	ASSERT(IE_SIZE(prHtOp) <= (ELEM_HDR_LEN + ELEM_MAX_LEN_HT_OP));

	prMsduInfo->u2FrameLength += IE_SIZE(prHtOp);
}

#if CFG_SUPPORT_802_11AC

/*----------------------------------------------------------------------------*/
/*!
* \brief For probe request, association request
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID rlmReqGenerateVhtCapIE(P_ADAPTER_T prAdapter, P_MSDU_INFO_T prMsduInfo)
{
	P_BSS_INFO_T prBssInfo;
	P_STA_RECORD_T prStaRec;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

	prBssInfo = prAdapter->aprBssInfo[prMsduInfo->ucBssIndex];
	if (!prBssInfo)
		return;

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

	if ((prAdapter->rWifiVar.ucAvailablePhyTypeSet & PHY_TYPE_SET_802_11AC) &&
	    (!prStaRec || (prStaRec->ucPhyTypeSet & PHY_TYPE_SET_802_11AC)))
		rlmFillVhtCapIE(prAdapter, prBssInfo, prMsduInfo);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief For probe response (GO, IBSS) and association response
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID rlmRspGenerateVhtCapIE(P_ADAPTER_T prAdapter, P_MSDU_INFO_T prMsduInfo)
{
	P_BSS_INFO_T prBssInfo;
	P_STA_RECORD_T prStaRec;
	UINT_8 ucPhyTypeSet;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

	prBssInfo = prAdapter->aprBssInfo[prMsduInfo->ucBssIndex];
	if (!prBssInfo)
		return;

	if (!IS_BSS_ACTIVE(prBssInfo))
		return;

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

	/* Decide PHY type set source */
	if (prStaRec) {
		/* Get PHY type set from target STA */
		ucPhyTypeSet = prStaRec->ucPhyTypeSet;
	} else {
		/* Get PHY type set from current BSS */
		ucPhyTypeSet = prBssInfo->ucPhyTypeSet;
	}

	if (RLM_NET_IS_11AC(prBssInfo) && (ucPhyTypeSet & PHY_TYPE_SET_802_11AC))
		rlmFillVhtCapIE(prAdapter, prBssInfo, prMsduInfo);

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
VOID rlmRspGenerateVhtOpIE(P_ADAPTER_T prAdapter, P_MSDU_INFO_T prMsduInfo)
{
	P_BSS_INFO_T prBssInfo;
	P_STA_RECORD_T prStaRec;
	UINT_8 ucPhyTypeSet;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

	prBssInfo = prAdapter->aprBssInfo[prMsduInfo->ucBssIndex];
	if (!prBssInfo)
		return;

	if (!IS_BSS_ACTIVE(prBssInfo))
		return;

	/* Decide PHY type set source */
	if (prStaRec) {
		/* Get PHY type set from target STA */
		ucPhyTypeSet = prStaRec->ucPhyTypeSet;
	} else {
		/* Get PHY type set from current BSS */
		ucPhyTypeSet = prBssInfo->ucPhyTypeSet;
	}

	if (RLM_NET_IS_11AC(prBssInfo) && (ucPhyTypeSet & PHY_TYPE_SET_802_11AC))
		rlmFillVhtOpIE(prAdapter, prBssInfo, prMsduInfo);
}



/*----------------------------------------------------------------------------*/
/*!
* \brief For probe request, association request
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID rlmReqGenerateVhtOpNotificationIE(P_ADAPTER_T prAdapter, P_MSDU_INFO_T prMsduInfo)
{
	P_BSS_INFO_T prBssInfo;
	P_STA_RECORD_T prStaRec;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

	prBssInfo = prAdapter->aprBssInfo[prMsduInfo->ucBssIndex];
	if (!prBssInfo)
		return;
	/* [TGac 5.2.46 STBC Receive Test with UCC 9.2.x]
	 * Operating Notification IE of Nss=2 will make Ralink testbed send data frames without STBC
	 * Enable the Operating Notification IE only for DBDC enable case.
	 */
	if (!prAdapter->rWifiVar.fgDbDcModeEn)
		return;
	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

	if ((prAdapter->rWifiVar.ucAvailablePhyTypeSet & PHY_TYPE_SET_802_11AC) &&
	    (!prStaRec || (prStaRec->ucPhyTypeSet & PHY_TYPE_SET_802_11AC)))
		rlmFillVhtOpNotificationIE(prAdapter, prBssInfo, prMsduInfo, TRUE);
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
VOID rlmRspGenerateVhtOpNotificationIE(P_ADAPTER_T prAdapter, P_MSDU_INFO_T prMsduInfo)
{
	P_BSS_INFO_T prBssInfo;
	P_STA_RECORD_T prStaRec;
	UINT_8 ucPhyTypeSet;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

	prBssInfo = prAdapter->aprBssInfo[prMsduInfo->ucBssIndex];
	if (!prBssInfo)
		return;

	if (!IS_BSS_ACTIVE(prBssInfo))
		return;

	/* Decide PHY type set source */
	if (prStaRec) {
		/* Get PHY type set from target STA */
		ucPhyTypeSet = prStaRec->ucPhyTypeSet;
	} else {
		/* Get PHY type set from current BSS */
		ucPhyTypeSet = prBssInfo->ucPhyTypeSet;
	}

	if (RLM_NET_IS_11AC(prBssInfo) && (ucPhyTypeSet & PHY_TYPE_SET_802_11AC))
		rlmFillVhtOpNotificationIE(prAdapter, prBssInfo, prMsduInfo, FALSE);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief
* add VHT operation notification IE for VHT-BW40 case specific
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
static VOID rlmFillVhtOpNotificationIE(P_ADAPTER_T prAdapter, P_BSS_INFO_T prBssInfo,
		P_MSDU_INFO_T prMsduInfo, BOOLEAN fgIsMaxCap)
{
	P_IE_VHT_OP_MODE_NOTIFICATION_T prVhtOpMode;
	UINT_8 ucMaxBw;

	ASSERT(prAdapter);
	ASSERT(prBssInfo);
	ASSERT(prMsduInfo);

	prVhtOpMode = (P_IE_VHT_OP_MODE_NOTIFICATION_T)
	    (((PUINT_8) prMsduInfo->prPacket) + prMsduInfo->u2FrameLength);

	kalMemZero((PVOID) prVhtOpMode, sizeof(IE_VHT_OP_MODE_NOTIFICATION_T));

	prVhtOpMode->ucId = ELEM_ID_OP_MODE;
	prVhtOpMode->ucLength = sizeof(IE_VHT_OP_MODE_NOTIFICATION_T) - ELEM_HDR_LEN;

	DBGLOG(RLM, INFO, "rlmFillVhtOpNotificationIE(%d) %u %u\n",
		prBssInfo->ucBssIndex, fgIsMaxCap, prBssInfo->ucNss);

	if (fgIsMaxCap) {

		ucMaxBw = cnmGetBssMaxBw(prAdapter, prBssInfo->ucBssIndex);

		/*handle 80P80 case*/
		if (ucMaxBw >= MAX_BW_160MHZ)
			ucMaxBw = MAX_BW_160MHZ;

		prVhtOpMode->ucOperatingMode |= ucMaxBw;

		prVhtOpMode->ucOperatingMode |=
			(((prBssInfo->ucNss-1) << VHT_OP_MODE_RX_NSS_OFFSET) & VHT_OP_MODE_RX_NSS);

	} else {

		switch (prBssInfo->ucVhtChannelWidth) {
		case VHT_OP_CHANNEL_WIDTH_80P80:
			ucMaxBw = MAX_BW_160MHZ;
			break;

		case VHT_OP_CHANNEL_WIDTH_160:
			ucMaxBw = MAX_BW_160MHZ;
			break;

		case VHT_OP_CHANNEL_WIDTH_80:
			ucMaxBw = MAX_BW_80MHZ;
			break;

		case VHT_OP_CHANNEL_WIDTH_20_40:
			{
				ucMaxBw = MAX_BW_20MHZ;

				if (prBssInfo->eBssSCO != CHNL_EXT_SCN)
					ucMaxBw = MAX_BW_40MHZ;
			}
			break;

		default:
			/*VHT default IE should support BW 80*/
			ucMaxBw = MAX_BW_80MHZ;
			break;
		}

		prVhtOpMode->ucOperatingMode |= ucMaxBw;

		prVhtOpMode->ucOperatingMode |= (((prBssInfo->ucNss-1)
			<< VHT_OP_MODE_RX_NSS_OFFSET) & VHT_OP_MODE_RX_NSS);
	}


	prMsduInfo->u2FrameLength += IE_SIZE(prVhtOpMode);

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
static VOID rlmFillVhtCapIE(P_ADAPTER_T prAdapter, P_BSS_INFO_T prBssInfo, P_MSDU_INFO_T prMsduInfo)
{
	P_IE_VHT_CAP_T prVhtCap;
	P_VHT_SUPPORTED_MCS_FIELD prVhtSupportedMcsSet;
	UINT_8 i;
	UINT_8 ucMaxBw;
	P_STA_RECORD_T prStaRec;

	ASSERT(prAdapter);
	ASSERT(prBssInfo);
	ASSERT(prMsduInfo);

	prVhtCap = (P_IE_VHT_CAP_T)
	    (((PUINT_8) prMsduInfo->prPacket) + prMsduInfo->u2FrameLength);

	prVhtCap->ucId = ELEM_ID_VHT_CAP;
	prVhtCap->ucLength = sizeof(IE_VHT_CAP_T) - ELEM_HDR_LEN;
	prVhtCap->u4VhtCapInfo = VHT_CAP_INFO_DEFAULT_VAL;

	ucMaxBw = cnmGetBssMaxBw(prAdapter, prBssInfo->ucBssIndex);

	prVhtCap->u4VhtCapInfo |= (prAdapter->rWifiVar.ucRxMaxMpduLen & VHT_CAP_INFO_MAX_MPDU_LEN_MASK);

	if (ucMaxBw == MAX_BW_160MHZ)
		prVhtCap->u4VhtCapInfo |= VHT_CAP_INFO_MAX_SUP_CHANNEL_WIDTH_SET_160;
	else if (ucMaxBw == MAX_BW_80_80_MHZ)
		prVhtCap->u4VhtCapInfo |= VHT_CAP_INFO_MAX_SUP_CHANNEL_WIDTH_SET_160_80P80;

	if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucStaVhtBfee)) {
		prVhtCap->u4VhtCapInfo |= FIELD_VHT_CAP_INFO_BFEE;
#if CFG_SUPPORT_BFEE
		prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

		if (prStaRec && (prStaRec->ucVhtCapNumSoundingDimensions == 0x2)) {
			/* For the compatibility with netgear R7000 AP */
			prVhtCap->u4VhtCapInfo |= (((UINT_32)prStaRec->ucVhtCapNumSoundingDimensions) <<
			VHT_CAP_INFO_COMP_STEERING_NUM_OF_BFER_ANT_SUP_OFFSET);
			DBGLOG(RLM, INFO, "Set VHT Cap BFEE STS CAP=%d\n",
				prStaRec->ucVhtCapNumSoundingDimensions);
		} else {
			/* For 11ac cert. VHT-5.2.63C MU-BFee step3,
			 * it requires STAUT to set its maximum STS capability here
			 */
			prVhtCap->u4VhtCapInfo |=
				VHT_CAP_INFO_COMPRESSED_STEERING_NUMBER_OF_BEAMFORMER_ANTENNAS_4_SUPPOERTED;
			DBGLOG(RLM, INFO, "Set VHT Cap BFEE STS CAP=%d\n", VHT_CAP_INFO_BEAMFORMEE_STS_CAP_MAX);
		}
#endif
		if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucStaVhtMuBfee))
			prVhtCap->u4VhtCapInfo |= VHT_CAP_INFO_MU_BEAMFOMEE_CAPABLE;
	}

	if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucStaVhtBfer))
		prVhtCap->u4VhtCapInfo |= FIELD_VHT_CAP_INFO_BFER;

	if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucRxShortGI)) {
		if (ucMaxBw >= MAX_BW_80MHZ)
			prVhtCap->u4VhtCapInfo |= VHT_CAP_INFO_SHORT_GI_80;

		if (ucMaxBw >= MAX_BW_160MHZ)
			prVhtCap->u4VhtCapInfo |= VHT_CAP_INFO_SHORT_GI_160_80P80;
	}

	if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucRxLdpc))
		prVhtCap->u4VhtCapInfo |= VHT_CAP_INFO_RX_LDPC;

	if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucRxStbc)) {
		UINT_8 tempRxStbcNss;

		if (prAdapter->rWifiVar.u4SwTestMode == ENUM_SW_TEST_MODE_SIGMA_AC) {
			tempRxStbcNss = 1;
			DBGLOG(RLM, INFO, "Set RxStbcNss to 1 for 11ac certification.\n");
		} else {
			tempRxStbcNss = prAdapter->rWifiVar.ucRxStbcNss;
			tempRxStbcNss = (tempRxStbcNss > wlanGetSupportNss(prAdapter, prBssInfo->ucBssIndex)) ?
				wlanGetSupportNss(prAdapter, prBssInfo->ucBssIndex) : (tempRxStbcNss);
			if (tempRxStbcNss != prAdapter->rWifiVar.ucRxStbcNss) {
				DBGLOG(RLM, WARN, "Apply Nss:%d as RxStbcNss in VHT Cap",
					wlanGetSupportNss(prAdapter, prBssInfo->ucBssIndex));
				DBGLOG(RLM, WARN, "due to set RxStbcNss more than Nss is not appropriate.\n");
			}
		}
		prVhtCap->u4VhtCapInfo |= ((tempRxStbcNss << VHT_CAP_INFO_RX_STBC_OFFSET) & VHT_CAP_INFO_RX_STBC_MASK);
	}

	if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucTxStbc))
		prVhtCap->u4VhtCapInfo |= VHT_CAP_INFO_TX_STBC;

	/*set MCS map */
	prVhtSupportedMcsSet = &prVhtCap->rVhtSupportedMcsSet;
	kalMemZero((PVOID) prVhtSupportedMcsSet, sizeof(VHT_SUPPORTED_MCS_FIELD));

	for (i = 0; i < 8; i++) {
		UINT_8 ucOffset = i * 2;
		UINT_8 ucMcsMap;

		if (i < wlanGetSupportNss(prAdapter, prBssInfo->ucBssIndex))
			ucMcsMap = VHT_CAP_INFO_MCS_MAP_MCS9;
		else
			ucMcsMap = VHT_CAP_INFO_MCS_NOT_SUPPORTED;

		prVhtSupportedMcsSet->u2RxMcsMap |= (ucMcsMap << ucOffset);
		prVhtSupportedMcsSet->u2TxMcsMap |= (ucMcsMap << ucOffset);
	}

#if 0
	for (i = 0; i < wlanGetSupportNss(prAdapter, prBssInfo->ucBssIndex); i++) {
		UINT_8 ucOffset = i * 2;

		prVhtSupportedMcsSet->u2RxMcsMap &=
			((VHT_CAP_INFO_MCS_MAP_MCS9 << ucOffset) & BITS(ucOffset, ucOffset + 1));
		prVhtSupportedMcsSet->u2TxMcsMap &=
			((VHT_CAP_INFO_MCS_MAP_MCS9 << ucOffset) & BITS(ucOffset, ucOffset + 1));
	}
#endif

	prVhtSupportedMcsSet->u2RxHighestSupportedDataRate = VHT_CAP_INFO_DEFAULT_HIGHEST_DATA_RATE;
	prVhtSupportedMcsSet->u2TxHighestSupportedDataRate = VHT_CAP_INFO_DEFAULT_HIGHEST_DATA_RATE;

	ASSERT(IE_SIZE(prVhtCap) <= (ELEM_HDR_LEN + ELEM_MAX_LEN_VHT_CAP));

	prMsduInfo->u2FrameLength += IE_SIZE(prVhtCap);

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
VOID rlmFillVhtOpIE(P_ADAPTER_T prAdapter, P_BSS_INFO_T prBssInfo, P_MSDU_INFO_T prMsduInfo)
{
	P_IE_VHT_OP_T prVhtOp;

	ASSERT(prAdapter);
	ASSERT(prBssInfo);
	ASSERT(prMsduInfo);

	prVhtOp = (P_IE_VHT_OP_T)
	    (((PUINT_8) prMsduInfo->prPacket) + prMsduInfo->u2FrameLength);

	/* Add HT operation IE */
	prVhtOp->ucId = ELEM_ID_VHT_OP;
	prVhtOp->ucLength = sizeof(IE_VHT_OP_T) - ELEM_HDR_LEN;

	ASSERT(IE_SIZE(prVhtOp) <= (ELEM_HDR_LEN + ELEM_MAX_LEN_VHT_OP));

	prVhtOp->ucVhtOperation[0] = prBssInfo->ucVhtChannelWidth;	/* (UINT8)VHT_OP_CHANNEL_WIDTH_80; */
	prVhtOp->ucVhtOperation[1] = prBssInfo->ucVhtChannelFrequencyS1;
	prVhtOp->ucVhtOperation[2] = prBssInfo->ucVhtChannelFrequencyS2;

#if 0
	if (cnmGetBssMaxBw(prAdapter, prBssInfo->ucBssIndex) < MAX_BW_80MHZ) {
		prVhtOp->ucVhtOperation[0] = VHT_OP_CHANNEL_WIDTH_20_40;
		prVhtOp->ucVhtOperation[1] = 0;
		prVhtOp->ucVhtOperation[2] = 0;
	} else if (cnmGetBssMaxBw(prAdapter, prBssInfo->ucBssIndex) == MAX_BW_80MHZ) {
		prVhtOp->ucVhtOperation[0] = VHT_OP_CHANNEL_WIDTH_80;
		prVhtOp->ucVhtOperation[1] = nicGetVhtS1(prBssInfo->ucPrimaryChannel);
		prVhtOp->ucVhtOperation[2] = 0;
	} else {
		/* TODO: BW80 + 80/160 support */
	}
#endif

	prVhtOp->u2VhtBasicMcsSet = prBssInfo->u2VhtBasicMcsSet;

	prMsduInfo->u2FrameLength += IE_SIZE(prVhtOp);
}

#endif

#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST
WLAN_STATUS rlmCalBackup(
	P_ADAPTER_T prAdapter,
	UINT_8		ucReason,
	UINT_8		ucAction,
	UINT_8		ucRomRam
	)
{
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_CAL_BACKUP_STRUCT_V2_T	rCalBackupDataV2;
	UINT_32 u4BufLen = 0;

	ASSERT(prAdapter);
	ASSERT(prAdapter->prGlueInfo);

	prGlueInfo = prAdapter->prGlueInfo;

	rCalBackupDataV2.ucReason = ucReason;
	rCalBackupDataV2.ucAction = ucAction;
	rCalBackupDataV2.ucNeedResp = 1;
	rCalBackupDataV2.ucFragNum = 0;
	rCalBackupDataV2.ucRomRam = ucRomRam;
	rCalBackupDataV2.u4ThermalValue = 0;
	rCalBackupDataV2.u4Address = 0;
	rCalBackupDataV2.u4Length = 0;
	rCalBackupDataV2.u4RemainLength = 0;

	if (ucReason == 0 && ucAction == 0) {
		DBGLOG(RFTEST, INFO, "RLM CMD : Get Thermal Temp from FW.\n");
		/* Step 1 : Get Thermal Temp from FW */

		rStatus = kalIoctl(prGlueInfo,
					wlanoidQueryCalBackupV2,
					&rCalBackupDataV2,
					sizeof(PARAM_CAL_BACKUP_STRUCT_V2_T),
					TRUE,
					TRUE,
					TRUE,
					&u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(RFTEST, INFO,
				"RLM CMD : Get Thermal Temp from FW Return Fail (0x%08x)!!!!!!!!!!!\n", rStatus);
			return rStatus;
		}

		DBGLOG(RFTEST, INFO, "CMD : Get Thermal Temp (%d) from FW. Finish!!!!!!!!!!!\n",
			rCalBackupDataV2.u4ThermalValue);
	} else if (ucReason == 1 && ucAction == 2) {
		DBGLOG(RFTEST, INFO, "RLM CMD : Trigger FW Do All Cal.\n");
		/* Step 2 : Trigger All Cal Function */

		rStatus = kalIoctl(prGlueInfo,
					wlanoidSetCalBackup,
					&rCalBackupDataV2,
					sizeof(PARAM_CAL_BACKUP_STRUCT_V2_T),
					FALSE,
					FALSE,
					TRUE,
					&u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(RFTEST, INFO,
				"RLM CMD : Trigger FW Do All Cal Return Fail (0x%08x)!!!!!!!!!!!\n", rStatus);
			return rStatus;
		}

		DBGLOG(RFTEST, INFO, "CMD : Trigger FW Do All Cal. Finish!!!!!!!!!!!\n");
	} else if (ucReason == 0 && ucAction == 1) {
		DBGLOG(RFTEST, INFO, "RLM CMD : Get Cal Data (%s) Size from FW.\n",
			ucRomRam == 0 ? "ROM" : "RAM");
		/* Step 3 : Get Cal Data Size from FW */

		rStatus = kalIoctl(prGlueInfo,
					wlanoidQueryCalBackupV2,
					&rCalBackupDataV2,
					sizeof(PARAM_CAL_BACKUP_STRUCT_V2_T),
					TRUE,
					TRUE,
					TRUE,
					&u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(RFTEST, INFO,
				"RLM CMD : Get Cal Data (%s) Size from FW Return Fail (0x%08x)!!!!!!!!!!!\n",
				ucRomRam == 0 ? "ROM" : "RAM", rStatus);
			return rStatus;
		}

		DBGLOG(RFTEST, INFO, "CMD : Get Cal Data (%s) Size from FW. Finish!!!!!!!!!!!\n",
			ucRomRam == 0 ? "ROM" : "RAM");
	} else if (ucReason == 2 && ucAction == 4) {
		DBGLOG(RFTEST, INFO, "RLM CMD : Get Cal Data from FW (%s). Start!!!!!!!!!!!!!!!!\n",
			ucRomRam == 0 ? "ROM" : "RAM");
		DBGLOG(RFTEST, INFO, "Thermal Temp = %d\n", g_rBackupCalDataAllV2.u4ThermalInfo);
		/* Step 4 : Get Cal Data from FW */

		rStatus = kalIoctl(prGlueInfo,
					wlanoidQueryCalBackupV2,
					&rCalBackupDataV2,
					sizeof(PARAM_CAL_BACKUP_STRUCT_V2_T),
					TRUE,
					TRUE,
					TRUE,
					&u4BufLen);
		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(RFTEST, INFO,
				"RLM CMD : Get Cal Data (%s) Size from FW Return Fail (0x%08x)!!!!!!!!!!!\n",
				ucRomRam == 0 ? "ROM" : "RAM", rStatus);
			return rStatus;
		}

		DBGLOG(RFTEST, INFO, "CMD : Get Cal Data from FW (%s). Finish!!!!!!!!!!!\n",
			ucRomRam == 0 ? "ROM" : "RAM");

		if (ucRomRam == 0) {
			DBGLOG(RFTEST, INFO,
				"Check some of elements (0x%08x), (0x%08x), (0x%08x), (0x%08x), (0x%08x)\n",
				g_rBackupCalDataAllV2.au4RomCalData[670], g_rBackupCalDataAllV2.au4RomCalData[671],
				g_rBackupCalDataAllV2.au4RomCalData[672], g_rBackupCalDataAllV2.au4RomCalData[673],
				g_rBackupCalDataAllV2.au4RomCalData[674]);
			DBGLOG(RFTEST, INFO,
				"Check some of elements (0x%08x), (0x%08x), (0x%08x), (0x%08x), (0x%08x)\n",
				g_rBackupCalDataAllV2.au4RomCalData[675], g_rBackupCalDataAllV2.au4RomCalData[676],
				g_rBackupCalDataAllV2.au4RomCalData[677], g_rBackupCalDataAllV2.au4RomCalData[678],
				g_rBackupCalDataAllV2.au4RomCalData[679]);
		}
	} else if (ucReason == 4 && ucAction == 6) {
		DBGLOG(RFTEST, INFO, "RLM CMD : Print Cal Data in FW (%s).\n",
			ucRomRam == 0 ? "ROM" : "RAM");
		/* Debug Use : Print Cal Data in FW */

		rStatus = kalIoctl(prGlueInfo,
					wlanoidSetCalBackup,
					&rCalBackupDataV2,
					sizeof(PARAM_CAL_BACKUP_STRUCT_V2_T),
					TRUE,
					TRUE,
					TRUE,
					&u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(RFTEST, INFO,
				"RLM CMD : Print Cal Data in FW (%s) Return Fail (0x%08x)!!!!!!!!!!!\n",
				ucRomRam == 0 ? "ROM" : "RAM", rStatus);
			return rStatus;
		}

		DBGLOG(RFTEST, INFO, "CMD : Print Cal Data in FW (%s). Finish!!!!!!!!!!!\n",
			ucRomRam == 0 ? "ROM" : "RAM");
	} else if (ucReason == 3 && ucAction == 5) {
		DBGLOG(RFTEST, INFO, "RLM CMD : Send Cal Data to FW (%s).\n",
			ucRomRam == 0 ? "ROM" : "RAM");
		/* Send Cal Data to FW */

		rStatus = kalIoctl(prGlueInfo,
					wlanoidSetCalBackup,
					&rCalBackupDataV2,
					sizeof(PARAM_CAL_BACKUP_STRUCT_V2_T),
					TRUE,
					TRUE,
					TRUE,
					&u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(RFTEST, INFO,
				"RLM CMD : Send Cal Data to FW (%s) Return Fail (0x%08x)!!!!!!!!!!!\n",
				ucRomRam == 0 ? "ROM" : "RAM", rStatus);
			return rStatus;
		}

		DBGLOG(RFTEST, INFO, "CMD : Send Cal Data to FW (%s). Finish!!!!!!!!!!!\n",
			ucRomRam == 0 ? "ROM" : "RAM");
	} else {
		DBGLOG(RFTEST, INFO, "CMD : Undefined Reason (%d) and Action (%d) for Cal Backup in Host Side!\n",
			ucReason, ucAction);

		return rStatus;
	}

	return rStatus;
}

WLAN_STATUS rlmTriggerCalBackup(
	P_ADAPTER_T prAdapter,
	BOOLEAN		fgIsCalDataBackuped
	)
{
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;

	if (!fgIsCalDataBackuped) {
		DBGLOG(RFTEST, INFO, "======== Boot Time Wi-Fi Enable........\n");
		DBGLOG(RFTEST, INFO, "Step 0 : Reset All Cal Data in Driver.\n");
		memset(&g_rBackupCalDataAllV2, 1, sizeof(RLM_CAL_RESULT_ALL_V2_T));
		g_rBackupCalDataAllV2.u4MagicNum1 = 6632;
		g_rBackupCalDataAllV2.u4MagicNum2 = 6632;

		DBGLOG(RFTEST, INFO, "Step 1 : Get Thermal Temp from FW.\n");
		if (rlmCalBackup(prAdapter, 0, 0, 0) == WLAN_STATUS_FAILURE) {
			DBGLOG(RFTEST, INFO, "Step 1 : Return Failure.\n");
			return WLAN_STATUS_FAILURE;
		}

		DBGLOG(RFTEST, INFO, "Step 2 : Get Rom Cal Data Size from FW.\n");
		if (rlmCalBackup(prAdapter, 0, 1, 0) == WLAN_STATUS_FAILURE) {
			DBGLOG(RFTEST, INFO, "Step 2 : Return Failure.\n");
			return WLAN_STATUS_FAILURE;
		}

		DBGLOG(RFTEST, INFO, "Step 3 : Get Ram Cal Data Size from FW.\n");
		if (rlmCalBackup(prAdapter, 0, 1, 1) == WLAN_STATUS_FAILURE) {
			DBGLOG(RFTEST, INFO, "Step 3 : Return Failure.\n");
			return WLAN_STATUS_FAILURE;
		}

		DBGLOG(RFTEST, INFO, "Step 4 : Trigger FW Do Full Cal.\n");
		if (rlmCalBackup(prAdapter, 1, 2, 0) == WLAN_STATUS_FAILURE) {
			DBGLOG(RFTEST, INFO, "Step 4 : Return Failure.\n");
			return WLAN_STATUS_FAILURE;
		}
	} else {
		DBGLOG(RFTEST, INFO, "======== Normal Wi-Fi Enable........\n");
		DBGLOG(RFTEST, INFO, "Step 0 : Sent Rom Cal data to FW.\n");
		if (rlmCalBackup(prAdapter, 3, 5, 0) == WLAN_STATUS_FAILURE) {
			DBGLOG(RFTEST, INFO, "Step 0 : Return Failure.\n");
			return WLAN_STATUS_FAILURE;
		}

		DBGLOG(RFTEST, INFO, "Step 1 : Sent Ram Cal data to FW.\n");
		if (rlmCalBackup(prAdapter, 3, 5, 1) == WLAN_STATUS_FAILURE) {
			DBGLOG(RFTEST, INFO, "Step 1 : Return Failure.\n");
			return WLAN_STATUS_FAILURE;
		}
	}

	return rStatus;
}
#endif

VOID rlmModifyVhtBwPara(
	PUINT_8 pucVhtChannelFrequencyS1,
	PUINT_8 pucVhtChannelFrequencyS2,
	PUINT_8 pucVhtChannelWidth
	)
{
	UINT_8 i = 0, ucTempS = 0;

		if ((*pucVhtChannelFrequencyS1 != 0) &&
			(*pucVhtChannelFrequencyS2 != 0)) {

			UINT_8		ucBW160Inteval = 8;

			DBGLOG(RLM, WARN, "S1=%d, S2=%d\n", *pucVhtChannelFrequencyS1, *pucVhtChannelFrequencyS2);

			if (((*pucVhtChannelFrequencyS2 - *pucVhtChannelFrequencyS1) == ucBW160Inteval) ||
				((*pucVhtChannelFrequencyS1 - *pucVhtChannelFrequencyS2) == ucBW160Inteval)) {
				/*C160 case*/

				/*NEW spec should set central ch of bw80 at S1,
				*set central ch of bw160 at S2
				*/
				for (i = 0; i < 2; i++) {

					if (i == 0)
						ucTempS = *pucVhtChannelFrequencyS1;
					else
						ucTempS = *pucVhtChannelFrequencyS2;

					if ((ucTempS == 50) || (ucTempS == 82) ||
						(ucTempS == 114) || (ucTempS == 163))
						break;
				}


				if (ucTempS == 0) {
					DBGLOG(RLM, WARN, "please check BW160 setting, find central freq fail\n");
					return;
				}

				*pucVhtChannelFrequencyS1 =  ucTempS;
				*pucVhtChannelFrequencyS2 =  0;
				*pucVhtChannelWidth = CW_160MHZ;
				DBGLOG(RLM, WARN, "[BW160][new spec] S1 change to %d\n", *pucVhtChannelFrequencyS1);
			} else {
				/*real 80P80 case*/
			}

		}

}

static VOID rlmRevisePreferBandwidthNss(
	P_ADAPTER_T prAdapter,
	UINT_8 ucBssIndex,
	P_STA_RECORD_T prStaRec
	)
{
	ENUM_CHANNEL_WIDTH_T eChannelWidth = CW_20_40MHZ;
	P_BSS_INFO_T prBssInfo;

#define VHT_MCS_TX_RX_MAX_2SS BITS(2, 3)
#define VHT_MCS_TX_RX_MAX_2SS_SHIFT 2

#define AR_STA_2AC_MCS(prStaRec) \
		(((prStaRec)->u2VhtRxMcsMap &  VHT_MCS_TX_RX_MAX_2SS) >> VHT_MCS_TX_RX_MAX_2SS_SHIFT)

#define AR_IS_STA_2SS_AC(prStaRec) \
		((AR_STA_2AC_MCS(prStaRec) != BITS(0, 1)))



	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

	eChannelWidth = prBssInfo->ucVhtChannelWidth;

	/*
	*Prefer setting modification
	*80+80 1x1 and 80 2x2 have the same phy rate, choose the 80 2x2
	*/

	if (AR_IS_STA_2SS_AC(prStaRec)) {
		/*
		*DBGLOG(RLM, WARN, "support 2ss\n");
		*/

		if ((eChannelWidth == CW_80P80MHZ && prBssInfo->ucVhtChannelFrequencyS2 != 0)) {
			DBGLOG(RLM, WARN, "support (2Nss) and (80+80)\n");
			DBGLOG(RLM, WARN, "choose (2Nss) and (80) for Bss_info\n");
			prBssInfo->ucVhtChannelWidth = CW_80MHZ;
			prBssInfo->ucVhtChannelFrequencyS2 = 0;
		}
	}
}

VOID rlmReviseMaxBw(
	P_ADAPTER_T prAdapter,
	UINT_8 ucBssIndex,
	P_ENUM_CHNL_EXT_T peExtend,
	P_ENUM_CHANNEL_WIDTH_P peChannelWidth,
	PUINT_8 pucS1,
	PUINT_8 pucPrimaryCh)
{
	UINT_8 ucMaxBandwidth = MAX_BW_80MHZ;
	UINT_8 ucCurrentBandwidth = MAX_BW_20MHZ;
	UINT_8 ucOffset = (MAX_BW_80MHZ - CW_80MHZ);

	ucMaxBandwidth = cnmGetBssMaxBw(prAdapter, ucBssIndex);

	if (*peChannelWidth > CW_20_40MHZ) {
		/*case BW > 80 , 160 80P80 */
		ucCurrentBandwidth = (UINT_8)*peChannelWidth + ucOffset;
	} else {
		/*case BW20 BW40 */
		if (*peExtend != CHNL_EXT_SCN) {
			/*case BW40 */
			ucCurrentBandwidth = MAX_BW_40MHZ;
		}
	}

	if (ucCurrentBandwidth > ucMaxBandwidth) {
		DBGLOG(RLM, INFO, "Decreasse the BW to (%d)\n", ucMaxBandwidth);

		if (ucMaxBandwidth <= MAX_BW_40MHZ) {
			/*BW20 * BW40*/
			*peChannelWidth = CW_20_40MHZ;

			if (ucMaxBandwidth == MAX_BW_20MHZ)
				*peExtend = CHNL_EXT_SCN;
		} else {
			/* BW80, BW160, BW80P80 */
			/* ucMaxBandwidth Must be MAX_BW_80MHZ,MAX_BW_160MHZ,MAX_BW_80MHZ */
			/* peExtend should not change */
			*peChannelWidth = (ucMaxBandwidth - ucOffset);

			if (ucMaxBandwidth == MAX_BW_80MHZ) {
				/* modify S1 for Bandwidth 160 downgrade 80 case */
				if (ucCurrentBandwidth == MAX_BW_160MHZ) {
					if ((*pucPrimaryCh >= 36) && (*pucPrimaryCh <= 48))
						*pucS1 = 42;
					else if ((*pucPrimaryCh >= 52) && (*pucPrimaryCh <= 64))
						*pucS1 = 58;
					else if ((*pucPrimaryCh >= 100) && (*pucPrimaryCh <= 112))
						*pucS1 = 106;
					else if ((*pucPrimaryCh >= 116) && (*pucPrimaryCh <= 128))
						*pucS1 = 122;
					else if ((*pucPrimaryCh >= 132) && (*pucPrimaryCh <= 144))
						*pucS1 = 138; /*160 downgrade should not in this case*/
					else if ((*pucPrimaryCh >= 149) && (*pucPrimaryCh <= 161))
						*pucS1 = 155; /*160 downgrade should not in this case*/
					else
						DBGLOG(RLM, INFO, "Check connect 160 downgrde (%d) case\n"
						, ucMaxBandwidth);

					DBGLOG(RLM, INFO, "Decreasse the BW160 to BW80, shift S1 to (%d)\n", *pucS1);
				}
			}
		}

		DBGLOG(RLM, INFO, "Modify ChannelWidth (%d) and Extend (%d)\n", *peChannelWidth, *peExtend);
	}
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Change VHT OP Channel Width, S1, S2
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID rlmChangeVhtOpBwPara(P_ADAPTER_T prAdapter, UINT_8 ucBssIndex, UINT_8 ucChannelWidth)
{
	P_BSS_INFO_T prBssInfo;
	UINT_8 ucVhtLowerChannelFrequency, ucVhtUpperChannelFrequency = 0;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

	if (!prBssInfo)
		return;

	if (ucChannelWidth == MAX_BW_80_80_MHZ)
		prBssInfo->ucVhtChannelWidth = VHT_OP_CHANNEL_WIDTH_80P80;
	else if (ucChannelWidth == MAX_BW_160MHZ)
		prBssInfo->ucVhtChannelWidth = VHT_OP_CHANNEL_WIDTH_160;
	else if (ucChannelWidth == MAX_BW_80MHZ) {
		prBssInfo->ucVhtChannelWidth = VHT_OP_CHANNEL_WIDTH_80;
		if (prBssInfo->ucVhtPeerChannelWidth == VHT_OP_CHANNEL_WIDTH_160)
			prBssInfo->ucVhtChannelFrequencyS1 =
			(prBssInfo->ucVhtPeerChannelFrequencyS1 > prBssInfo->ucPrimaryChannel) ?
			(prBssInfo->ucVhtPeerChannelFrequencyS1 - 8) : (prBssInfo->ucVhtPeerChannelFrequencyS1 + 8);
		else if (prBssInfo->ucVhtPeerChannelWidth == VHT_OP_CHANNEL_WIDTH_80P80) {
			if (prBssInfo->ucVhtChannelFrequencyS1 < prBssInfo->ucVhtChannelFrequencyS2) {
				ucVhtLowerChannelFrequency = prBssInfo->ucVhtChannelFrequencyS1;
				ucVhtUpperChannelFrequency = prBssInfo->ucVhtChannelFrequencyS2;
			} else {
				ucVhtLowerChannelFrequency = prBssInfo->ucVhtChannelFrequencyS2;
				ucVhtUpperChannelFrequency = prBssInfo->ucVhtChannelFrequencyS1;
			}
			prBssInfo->ucVhtChannelFrequencyS1 = prBssInfo->ucPrimaryChannel <
					((ucVhtLowerChannelFrequency + ucVhtUpperChannelFrequency)/2) ?
					ucVhtLowerChannelFrequency : ucVhtUpperChannelFrequency;
			prBssInfo->ucVhtChannelFrequencyS2 = 0;
		}
	} else if ((ucChannelWidth == MAX_BW_40MHZ) || (ucChannelWidth == MAX_BW_20MHZ))
		prBssInfo->ucVhtChannelWidth = VHT_OP_CHANNEL_WIDTH_20_40;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function should be invoked to update parameters of associated AP.
*        (Association response and Beacon)
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
static UINT_8 rlmRecIeInfoForClient(P_ADAPTER_T prAdapter, P_BSS_INFO_T prBssInfo, PUINT_8 pucIE, UINT_16 u2IELength)
{
	UINT_16 u2Offset;
	P_STA_RECORD_T prStaRec;
	P_IE_HT_CAP_T prHtCap;
	P_IE_HT_OP_T prHtOp;
	P_IE_OBSS_SCAN_PARAM_T prObssScnParam;
	UINT_8 ucERP, ucPrimaryChannel;
	P_WIFI_VAR_T prWifiVar = &prAdapter->rWifiVar;
#if CFG_SUPPORT_QUIET && 0
	BOOLEAN fgHasQuietIE = FALSE;
#endif
	BOOLEAN IsfgHtCapChange = FALSE;

#if CFG_SUPPORT_802_11AC
	P_IE_VHT_OP_T prVhtOp;
	P_IE_VHT_CAP_T prVhtCap;
	P_IE_OP_MODE_NOTIFICATION_T prOPModeNotification;	/* Operation Mode Notification */
	BOOLEAN fgHasOPModeIE = FALSE;
	UINT_8 ucVhtOpModeChannelWidth = 0;
	UINT_8 ucVhtOpModeRxNss = 0;
	UINT_8 ucVhtCapMcsOwnNotSupportOffset;
	UINT_8 ucMaxBwAllowed;
	UINT_8 ucInitVhtOpMode = 0;
#endif

#if CFG_SUPPORT_DFS
	BOOLEAN fgHasWideBandIE = FALSE;
	BOOLEAN fgHasSCOIE = FALSE;
	BOOLEAN fgHasChannelSwitchIE = FALSE;
	BOOLEAN fgNeedSwitchChannel = FALSE;
	UINT_8 ucChannelAnnouncePri;
	ENUM_CHNL_EXT_T eChannelAnnounceSco;
	UINT_8 ucChannelAnnounceChannelS1 = 0;
	UINT_8 ucChannelAnnounceChannelS2 = 0;
	UINT_8 ucChannelAnnounceVhtBw;
	P_IE_CHANNEL_SWITCH_T prChannelSwitchAnnounceIE;
	P_IE_SECONDARY_OFFSET_T prSecondaryOffsetIE;
	P_IE_WIDE_BAND_CHANNEL_T prWideBandChannelIE;
#endif
	PUINT_8 pucDumpIE;

	ASSERT(prAdapter);
	ASSERT(prBssInfo);
	ASSERT(pucIE);

	prStaRec = prBssInfo->prStaRecOfAP;
	ASSERT(prStaRec);
	if (!prStaRec)
		return 0;

	prBssInfo->fgUseShortPreamble = prBssInfo->fgIsShortPreambleAllowed;
	ucPrimaryChannel = 0;
	prObssScnParam = NULL;
	ucMaxBwAllowed = cnmGetBssMaxBw(prAdapter, prBssInfo->ucBssIndex);
	pucDumpIE = pucIE;

	/* Note: HT-related members in staRec may not be zero before, so
	 *       if following IE does not exist, they are still not zero.
	 *       These HT-related parameters are valid only when the corresponding
	 *       BssInfo supports 802.11n, i.e., RLM_NET_IS_11N()
	 */
	IE_FOR_EACH(pucIE, u2IELength, u2Offset) {
		switch (IE_ID(pucIE)) {
		case ELEM_ID_HT_CAP:
			if (!RLM_NET_IS_11N(prBssInfo) || IE_LEN(pucIE) != (sizeof(IE_HT_CAP_T) - 2))
				break;
			prHtCap = (P_IE_HT_CAP_T) pucIE;
			prStaRec->ucMcsSet = prHtCap->rSupMcsSet.aucRxMcsBitmask[0];
			prStaRec->fgSupMcs32 = (prHtCap->rSupMcsSet.aucRxMcsBitmask[32 / 8] & BIT(0)) ? TRUE : FALSE;

			kalMemCopy(prStaRec->aucRxMcsBitmask, prHtCap->rSupMcsSet.aucRxMcsBitmask,
				   sizeof(prStaRec->aucRxMcsBitmask) /*SUP_MCS_RX_BITMASK_OCTET_NUM */);

			prStaRec->u2RxHighestSupportedRate = prHtCap->rSupMcsSet.u2RxHighestSupportedRate;
			prStaRec->u4TxRateInfo = prHtCap->rSupMcsSet.u4TxRateInfo;

			if ((prStaRec->u2HtCapInfo & HT_CAP_INFO_SM_POWER_SAVE) !=
				(prHtCap->u2HtCapInfo & HT_CAP_INFO_SM_POWER_SAVE))
				IsfgHtCapChange = TRUE;/* Purpose : To detect SMPS change */

			prStaRec->u2HtCapInfo = prHtCap->u2HtCapInfo;
			/* Set LDPC Tx capability */
			if (IS_FEATURE_FORCE_ENABLED(prWifiVar->ucTxLdpc))
				prStaRec->u2HtCapInfo |= HT_CAP_INFO_LDPC_CAP;
			else if (IS_FEATURE_DISABLED(prWifiVar->ucTxLdpc))
				prStaRec->u2HtCapInfo &= ~HT_CAP_INFO_LDPC_CAP;

			/* Set STBC Tx capability */
			if (IS_FEATURE_FORCE_ENABLED(prWifiVar->ucTxStbc))
				prStaRec->u2HtCapInfo |= HT_CAP_INFO_RX_STBC;
			else if (IS_FEATURE_DISABLED(prWifiVar->ucTxStbc))
				prStaRec->u2HtCapInfo &= ~HT_CAP_INFO_RX_STBC;

			/* Set Short GI Tx capability */
			if (IS_FEATURE_FORCE_ENABLED(prWifiVar->ucTxShortGI)) {
				prStaRec->u2HtCapInfo |= HT_CAP_INFO_SHORT_GI_20M;
				prStaRec->u2HtCapInfo |= HT_CAP_INFO_SHORT_GI_40M;
			} else if (IS_FEATURE_DISABLED(prWifiVar->ucTxShortGI)) {
				prStaRec->u2HtCapInfo &= ~HT_CAP_INFO_SHORT_GI_20M;
				prStaRec->u2HtCapInfo &= ~HT_CAP_INFO_SHORT_GI_40M;
			}

			/* Set HT Greenfield Tx capability */
			if (IS_FEATURE_FORCE_ENABLED(prWifiVar->ucTxGf))
				prStaRec->u2HtCapInfo |= HT_CAP_INFO_HT_GF;
			else if (IS_FEATURE_DISABLED(prWifiVar->ucTxGf))
				prStaRec->u2HtCapInfo &= ~HT_CAP_INFO_HT_GF;

			prStaRec->ucAmpduParam = prHtCap->ucAmpduParam;
			prStaRec->u2HtExtendedCap = prHtCap->u2HtExtendedCap;
			prStaRec->u4TxBeamformingCap = prHtCap->u4TxBeamformingCap;
			prStaRec->ucAselCap = prHtCap->ucAselCap;
			break;

		case ELEM_ID_HT_OP:
			if (!RLM_NET_IS_11N(prBssInfo) || IE_LEN(pucIE) != (sizeof(IE_HT_OP_T) - 2))
				break;
			prHtOp = (P_IE_HT_OP_T) pucIE;
			/* Workaround that some APs fill primary channel field by its
			 * secondary channel, but its DS IE is correct 20110610
			 */
			if (ucPrimaryChannel == 0)
				ucPrimaryChannel = prHtOp->ucPrimaryChannel;
			prBssInfo->ucHtOpInfo1 = prHtOp->ucInfo1;
			prBssInfo->u2HtOpInfo2 = prHtOp->u2Info2;
			prBssInfo->u2HtOpInfo3 = prHtOp->u2Info3;

			/*Backup peer HT OP Info*/
			prBssInfo->ucHtPeerOpInfo1 = prHtOp->ucInfo1;

			if (!prBssInfo->fg40mBwAllowed)
				prBssInfo->ucHtOpInfo1 &= ~(HT_OP_INFO1_SCO | HT_OP_INFO1_STA_CHNL_WIDTH);

			if ((prBssInfo->ucHtOpInfo1 & HT_OP_INFO1_SCO) != CHNL_EXT_RES)
				prBssInfo->eBssSCO = (ENUM_CHNL_EXT_T) (prBssInfo->ucHtOpInfo1 & HT_OP_INFO1_SCO);

			if (prBssInfo->ucOpChangeChannelWidth == MAX_BW_20MHZ && prBssInfo->fgIsOpChangeChannelWidth) {
				prBssInfo->ucHtOpInfo1 &= ~(HT_OP_INFO1_SCO | HT_OP_INFO1_STA_CHNL_WIDTH);
				prBssInfo->eBssSCO = CHNL_EXT_SCN;
			}

			prBssInfo->eHtProtectMode = (ENUM_HT_PROTECT_MODE_T)
			    (prBssInfo->u2HtOpInfo2 & HT_OP_INFO2_HT_PROTECTION);

			/* To do: process regulatory class 16 */
			if ((prBssInfo->u2HtOpInfo2 & HT_OP_INFO2_OBSS_NON_HT_STA_PRESENT)
			    && 0 /* && regulatory class is 16 */)
				prBssInfo->eGfOperationMode = GF_MODE_DISALLOWED;
			else if (prBssInfo->u2HtOpInfo2 & HT_OP_INFO2_NON_GF_HT_STA_PRESENT)
				prBssInfo->eGfOperationMode = GF_MODE_PROTECT;
			else
				prBssInfo->eGfOperationMode = GF_MODE_NORMAL;

			prBssInfo->eRifsOperationMode =
			    (prBssInfo->ucHtOpInfo1 & HT_OP_INFO1_RIFS_MODE) ? RIFS_MODE_NORMAL : RIFS_MODE_DISALLOWED;

			break;

#if CFG_SUPPORT_802_11AC
		case ELEM_ID_VHT_CAP:
			if (!RLM_NET_IS_11AC(prBssInfo) || IE_LEN(pucIE) != (sizeof(IE_VHT_CAP_T) - 2))
				break;

			prVhtCap = (P_IE_VHT_CAP_T) pucIE;

			prStaRec->u4VhtCapInfo = prVhtCap->u4VhtCapInfo;
			/* Set Tx LDPC capability */
			if (IS_FEATURE_FORCE_ENABLED(prWifiVar->ucTxLdpc))
				prStaRec->u4VhtCapInfo |= VHT_CAP_INFO_RX_LDPC;
			else if (IS_FEATURE_DISABLED(prWifiVar->ucTxLdpc))
				prStaRec->u4VhtCapInfo &= ~VHT_CAP_INFO_RX_LDPC;

			/* Set Tx STBC capability */
			if (IS_FEATURE_FORCE_ENABLED(prWifiVar->ucTxStbc))
				prStaRec->u4VhtCapInfo |= VHT_CAP_INFO_RX_STBC_MASK;
			else if (IS_FEATURE_DISABLED(prWifiVar->ucTxStbc))
				prStaRec->u4VhtCapInfo &= ~VHT_CAP_INFO_RX_STBC_MASK;

			/* Set Tx TXOP PS capability */
			if (IS_FEATURE_FORCE_ENABLED(prWifiVar->ucTxopPsTx))
				prStaRec->u4VhtCapInfo |= VHT_CAP_INFO_VHT_TXOP_PS;
			else if (IS_FEATURE_DISABLED(prWifiVar->ucTxopPsTx))
				prStaRec->u4VhtCapInfo &= ~VHT_CAP_INFO_VHT_TXOP_PS;

			/* Set Tx Short GI capability */
			if (IS_FEATURE_FORCE_ENABLED(prWifiVar->ucTxShortGI)) {
				prStaRec->u4VhtCapInfo |= VHT_CAP_INFO_SHORT_GI_80;
				prStaRec->u4VhtCapInfo |= VHT_CAP_INFO_SHORT_GI_160_80P80;
			} else if (IS_FEATURE_DISABLED(prWifiVar->ucTxShortGI)) {
				prStaRec->u4VhtCapInfo &= ~VHT_CAP_INFO_SHORT_GI_80;
				prStaRec->u4VhtCapInfo &= ~VHT_CAP_INFO_SHORT_GI_160_80P80;
			}

			/*Set Vht Rx Mcs Map upon peer's capability and our capability */
			prStaRec->u2VhtRxMcsMap = prVhtCap->rVhtSupportedMcsSet.u2RxMcsMap;
			if (wlanGetSupportNss(prAdapter, prStaRec->ucBssIndex) < 8) {
				ucVhtCapMcsOwnNotSupportOffset = wlanGetSupportNss(prAdapter, prStaRec->ucBssIndex) * 2;
				/*Mark Rx Mcs Map which we don't support*/
				prStaRec->u2VhtRxMcsMap |= BITS(ucVhtCapMcsOwnNotSupportOffset, 15);
			}
			if (prStaRec->u2VhtRxMcsMap != prVhtCap->rVhtSupportedMcsSet.u2RxMcsMap)
				DBGLOG(RLM, INFO, "Change VhtRxMcsMap from 0x%x to 0x%x due to our Nss setting\n",
						prVhtCap->rVhtSupportedMcsSet.u2RxMcsMap, prStaRec->u2VhtRxMcsMap);

			prStaRec->u2VhtRxHighestSupportedDataRate =
			    prVhtCap->rVhtSupportedMcsSet.u2RxHighestSupportedDataRate;
			prStaRec->u2VhtTxMcsMap = prVhtCap->rVhtSupportedMcsSet.u2TxMcsMap;
			prStaRec->u2VhtTxHighestSupportedDataRate =
				prVhtCap->rVhtSupportedMcsSet.u2TxHighestSupportedDataRate;

			break;

		case ELEM_ID_VHT_OP:
			if (!RLM_NET_IS_11AC(prBssInfo) || IE_LEN(pucIE) != (sizeof(IE_VHT_OP_T) - 2))
				break;

			prVhtOp = (P_IE_VHT_OP_T) pucIE;

			/*Backup peer VHT OpInfo*/
			prBssInfo->ucVhtPeerChannelWidth = prVhtOp->ucVhtOperation[0];
			prBssInfo->ucVhtPeerChannelFrequencyS1 = prVhtOp->ucVhtOperation[1];
			prBssInfo->ucVhtPeerChannelFrequencyS2 = prVhtOp->ucVhtOperation[2];

			rlmModifyVhtBwPara(&prBssInfo->ucVhtPeerChannelFrequencyS1,
				&prBssInfo->ucVhtPeerChannelFrequencyS2,
				&prBssInfo->ucVhtPeerChannelWidth);

			prBssInfo->ucVhtChannelWidth = prVhtOp->ucVhtOperation[0];
			prBssInfo->ucVhtChannelFrequencyS1 = prVhtOp->ucVhtOperation[1];
			prBssInfo->ucVhtChannelFrequencyS2 = prVhtOp->ucVhtOperation[2];
			prBssInfo->u2VhtBasicMcsSet = prVhtOp->u2VhtBasicMcsSet;

			rlmModifyVhtBwPara(&prBssInfo->ucVhtChannelFrequencyS1,
				&prBssInfo->ucVhtChannelFrequencyS2,
				&prBssInfo->ucVhtChannelWidth);

			if (prBssInfo->fgIsOpChangeChannelWidth)
				rlmChangeVhtOpBwPara(prAdapter, prBssInfo->ucBssIndex,
							prBssInfo->ucOpChangeChannelWidth);

			/* Set initial value of VHT OP mode */
			ucInitVhtOpMode = 0;
			switch (prBssInfo->ucVhtChannelWidth) {
			case VHT_OP_CHANNEL_WIDTH_20_40:
				ucInitVhtOpMode |= VHT_OP_MODE_CHANNEL_WIDTH_40;
				break;
			case VHT_OP_CHANNEL_WIDTH_80:
				ucInitVhtOpMode |= VHT_OP_MODE_CHANNEL_WIDTH_80;
				break;
			case VHT_OP_CHANNEL_WIDTH_160:
			case VHT_OP_CHANNEL_WIDTH_80P80:
				ucInitVhtOpMode |= VHT_OP_MODE_CHANNEL_WIDTH_160_80P80;
				break;
			default:
				ucInitVhtOpMode |= VHT_OP_MODE_CHANNEL_WIDTH_80;
				break;
			}
			ucInitVhtOpMode |= ((prBssInfo->ucNss-1) << VHT_OP_MODE_RX_NSS_OFFSET) & VHT_OP_MODE_RX_NSS;
			break;
		case ELEM_ID_OP_MODE:
			if (!RLM_NET_IS_11AC(prBssInfo) || IE_LEN(pucIE) != (sizeof(IE_OP_MODE_NOTIFICATION_T) - 2))
				break;
			prOPModeNotification = (P_IE_OP_MODE_NOTIFICATION_T) pucIE;

			if ((prOPModeNotification->ucOpMode & VHT_OP_MODE_RX_NSS_TYPE)
			    != VHT_OP_MODE_RX_NSS_TYPE) {
				if (prStaRec->ucVhtOpMode != prOPModeNotification->ucOpMode) {
					prStaRec->ucVhtOpMode = prOPModeNotification->ucOpMode;
					fgHasOPModeIE = TRUE;
					ucVhtOpModeChannelWidth =
					    ((prOPModeNotification->ucOpMode) & VHT_OP_MODE_CHANNEL_WIDTH);
					ucVhtOpModeRxNss =
					    ((prOPModeNotification->ucOpMode) & VHT_OP_MODE_RX_NSS) >>
						VHT_OP_MODE_RX_NSS_OFFSET;
				} else /* Let the further flow not to update VhtOpMode */
					ucInitVhtOpMode = prStaRec->ucVhtOpMode;
			}

			break;
#if CFG_SUPPORT_DFS
		case ELEM_ID_WIDE_BAND_CHANNEL_SWITCH:
			if (!RLM_NET_IS_11AC(prBssInfo) || IE_LEN(pucIE) != (sizeof(IE_WIDE_BAND_CHANNEL_T) - 2))
				break;
			DBGLOG(RLM, INFO, "[Channel Switch] ELEM_ID_WIDE_BAND_CHANNEL_SWITCH, 11AC\n");
			prWideBandChannelIE = (P_IE_WIDE_BAND_CHANNEL_T) pucIE;
			ucChannelAnnounceVhtBw = prWideBandChannelIE->ucNewChannelWidth;
			ucChannelAnnounceChannelS1 = prWideBandChannelIE->ucChannelS1;
			ucChannelAnnounceChannelS2 = prWideBandChannelIE->ucChannelS2;
			fgHasWideBandIE = TRUE;
			DBGLOG(RLM, INFO,
			       "[Ch] BW=%d, s1=%d, s2=%d\n", ucChannelAnnounceVhtBw, ucChannelAnnounceChannelS1,
			       ucChannelAnnounceChannelS2);
			break;
#endif

#endif
		case ELEM_ID_20_40_BSS_COEXISTENCE:
			if (!RLM_NET_IS_11N(prBssInfo))
				break;
			/* To do: store if scanning exemption grant to BssInfo */
			break;

		case ELEM_ID_OBSS_SCAN_PARAMS:
			if (!RLM_NET_IS_11N(prBssInfo) || IE_LEN(pucIE) != (sizeof(IE_OBSS_SCAN_PARAM_T) - 2))
				break;
			/* Store OBSS parameters to BssInfo */
			prObssScnParam = (P_IE_OBSS_SCAN_PARAM_T) pucIE;
			break;

		case ELEM_ID_EXTENDED_CAP:
			if (!RLM_NET_IS_11N(prBssInfo))
				break;
			/* To do: store extended capability (PSMP, coexist) to BssInfo */
			break;

		case ELEM_ID_ERP_INFO:
			if (IE_LEN(pucIE) != (sizeof(IE_ERP_T) - 2) || prBssInfo->eBand != BAND_2G4)
				break;
			ucERP = ERP_INFO_IE(pucIE)->ucERP;
			prBssInfo->fgErpProtectMode = (ucERP & ERP_INFO_USE_PROTECTION) ? TRUE : FALSE;

			if (ucERP & ERP_INFO_BARKER_PREAMBLE_MODE)
				prBssInfo->fgUseShortPreamble = FALSE;
			break;

		case ELEM_ID_DS_PARAM_SET:
			if (IE_LEN(pucIE) == ELEM_MAX_LEN_DS_PARAMETER_SET)
				ucPrimaryChannel = DS_PARAM_IE(pucIE)->ucCurrChnl;
			break;
#if CFG_SUPPORT_DFS
		case ELEM_ID_CH_SW_ANNOUNCEMENT:
			if (IE_LEN(pucIE) != (sizeof(IE_CHANNEL_SWITCH_T) - 2))
				break;

			prChannelSwitchAnnounceIE = (P_IE_CHANNEL_SWITCH_T) pucIE;

			DBGLOG(RLM, INFO, "[Ch] Count=%d\n", prChannelSwitchAnnounceIE->ucChannelSwitchCount);

			if (prChannelSwitchAnnounceIE->ucChannelSwitchMode == 1) {
				/* Need to stop data transmission immediately */
				fgHasChannelSwitchIE = TRUE;
				if (!g_fgHasStopTx) {
					g_fgHasStopTx = TRUE;
#if CFG_SUPPORT_TDLS
					/* TDLS peers */
					TdlsTxCtrl(prAdapter, prBssInfo, FALSE);
#endif
					/* AP */
					qmSetStaRecTxAllowed(prAdapter, prStaRec, FALSE);
					DBGLOG(RLM, EVENT, "[Ch] TxAllowed = FALSE\n");
				}

				if (prChannelSwitchAnnounceIE->ucChannelSwitchCount <= 3) {
					DBGLOG(RLM, INFO,
					       "[Ch] switch channel [%d]->[%d]\n", prBssInfo->ucPrimaryChannel,
					       prChannelSwitchAnnounceIE->ucNewChannelNum);
					ucChannelAnnouncePri = prChannelSwitchAnnounceIE->ucNewChannelNum;
					fgNeedSwitchChannel = TRUE;
				}
			}

			break;
		case ELEM_ID_SCO:
			if (IE_LEN(pucIE) != (sizeof(IE_SECONDARY_OFFSET_T) - 2))
				break;

			prSecondaryOffsetIE = (P_IE_SECONDARY_OFFSET_T) pucIE;
			DBGLOG(RLM, INFO,
			       "[Channel Switch] SCO [%d]->[%d]\n", prBssInfo->eBssSCO,
			       prSecondaryOffsetIE->ucSecondaryOffset);
			eChannelAnnounceSco = (ENUM_CHNL_EXT_T) prSecondaryOffsetIE->ucSecondaryOffset;
			fgHasSCOIE = TRUE;
			break;
#endif

#if CFG_SUPPORT_QUIET && 0
			/* Note: RRM code should be moved to independent RRM function by
			 *       component design rule. But we attach it to RLM temporarily
			 */
		case ELEM_ID_QUIET:
			rrmQuietHandleQuietIE(prBssInfo, (P_IE_QUIET_T) pucIE);
			fgHasQuietIE = TRUE;
			break;
#endif
		default:
			break;
		}		/* end of switch */
	}			/* end of IE_FOR_EACH */

	if (IsfgHtCapChange && (prStaRec->ucStaState == STA_STATE_3))
		cnmStaSendUpdateCmd(prAdapter, prStaRec, NULL, FALSE);

	/* Some AP will have wrong channel number (255) when running time.
	 * Check if correct channel number information. 20110501
	 */
	if ((prBssInfo->eBand == BAND_2G4 && ucPrimaryChannel > 14) ||
	    (prBssInfo->eBand != BAND_2G4 && (ucPrimaryChannel >= 200 || ucPrimaryChannel <= 14)))
		ucPrimaryChannel = 0;
#if CFG_SUPPORT_802_11AC
	/* Check whether the Operation Mode IE is exist or not.
	 *  If exists, then the channel bandwidth of VHT operation field  is changed
	 *  with the channel bandwidth setting of Operation Mode field.
	 *  The channel bandwidth of OP Mode IE  is  0, represent as 20MHz.
	 *  The channel bandwidth of OP Mode IE  is  1, represent as 40MHz.
	 *  The channel bandwidth of OP Mode IE  is  2, represent as 80MHz.
	 *  The channel bandwidth of OP Mode IE  is  3, represent as 160/80+80MHz.
	 */
	if (fgHasOPModeIE == TRUE) {
		/* 1.Channel width change */

		if (ucVhtOpModeChannelWidth == VHT_OP_MODE_CHANNEL_WIDTH_20) {
			prBssInfo->ucVhtChannelWidth = CW_20_40MHZ;
			prBssInfo->ucHtOpInfo1 &= ~HT_OP_INFO1_STA_CHNL_WIDTH;
			prStaRec->u2HtCapInfo &= ~HT_CAP_INFO_SUP_CHNL_WIDTH;

#if CFG_WORKAROUND_OPMODE_CONFLICT_OPINFO
			if (prBssInfo->eBssSCO != CHNL_EXT_SCN) {
				DBGLOG(RLM, WARN, "HT_OP_Info != OPmode_Notifify, follow OPmode_Notify to BW20.\n");
				prBssInfo->eBssSCO = CHNL_EXT_SCN;
			}
#endif
		}

		else if (ucVhtOpModeChannelWidth == VHT_OP_MODE_CHANNEL_WIDTH_40) {
			prBssInfo->ucVhtChannelWidth = CW_20_40MHZ;
			prBssInfo->ucHtOpInfo1 |= HT_OP_INFO1_STA_CHNL_WIDTH;
			prStaRec->u2HtCapInfo |= HT_CAP_INFO_SUP_CHNL_WIDTH;

#if CFG_WORKAROUND_OPMODE_CONFLICT_OPINFO
			if (prBssInfo->eBssSCO == CHNL_EXT_SCN) {
				prBssInfo->ucHtOpInfo1 &= ~HT_OP_INFO1_STA_CHNL_WIDTH;
				prStaRec->u2HtCapInfo &= ~HT_CAP_INFO_SUP_CHNL_WIDTH;
				DBGLOG(RLM, WARN, "HT_OP_Info != OPmode_Notifify, follow HT_OP_Info to BW20.\n");
			}
#endif
		}

		else if (ucVhtOpModeChannelWidth == VHT_OP_MODE_CHANNEL_WIDTH_80) {
			prBssInfo->ucVhtChannelWidth = CW_80MHZ;
			prBssInfo->ucHtOpInfo1 |= HT_OP_INFO1_STA_CHNL_WIDTH;
			prStaRec->u4VhtCapInfo &= ~VHT_CAP_INFO_MAX_SUP_CHANNEL_WIDTH_MASK;
			prStaRec->u2HtCapInfo |= HT_CAP_INFO_SUP_CHNL_WIDTH;
		}

		else if (ucVhtOpModeChannelWidth == VHT_OP_MODE_CHANNEL_WIDTH_160_80P80) {
			prBssInfo->ucHtOpInfo1 |= HT_OP_INFO1_STA_CHNL_WIDTH;
			prStaRec->u2HtCapInfo |= HT_CAP_INFO_SUP_CHNL_WIDTH;

			/* Use VHT OP Info to determine it's BW160 or BW80+BW80  */
			prStaRec->u4VhtCapInfo &= ~VHT_CAP_INFO_MAX_SUP_CHANNEL_WIDTH_MASK;
			if (prBssInfo->ucVhtChannelWidth == VHT_OP_CHANNEL_WIDTH_160)
				prStaRec->u4VhtCapInfo |= VHT_CAP_INFO_MAX_SUP_CHANNEL_WIDTH_SET_160;
			else if (prBssInfo->ucVhtChannelWidth == VHT_OP_CHANNEL_WIDTH_80P80)
				prStaRec->u4VhtCapInfo |= VHT_CAP_INFO_MAX_SUP_CHANNEL_WIDTH_SET_160_80P80;
		}

		if (prStaRec->ucStaState == STA_STATE_3) {
			DBGLOG(RLM, INFO, "Update OpMode to 0x%x", prStaRec->ucVhtOpMode);
			DBGLOG(RLM, INFO, "to FW due to OpMode Notificaition\n");
			cnmStaSendUpdateCmd(prAdapter, prStaRec, NULL, FALSE);
		}
	} else {/* Set Default if the VHT OP mode field is not present */
		if ((prStaRec->ucVhtOpMode != ucInitVhtOpMode) && (prStaRec->ucStaState == STA_STATE_3)) {
			prStaRec->ucVhtOpMode = ucInitVhtOpMode;
			DBGLOG(RLM, INFO, "Update OpMode to 0x%x", prStaRec->ucVhtOpMode);
			DBGLOG(RLM, INFO, "to FW due to NO OpMode Notificaition\n");
			cnmStaSendUpdateCmd(prAdapter, prStaRec, NULL, FALSE);
		} else
			prStaRec->ucVhtOpMode = ucInitVhtOpMode;
	}
#endif

#if CFG_SUPPORT_DFS
	/*Check whether Channel Announcement IE, Secondary Offset IE &
	 *  Wide Bandwidth Channel Switch IE exist or not. If exist, the priority is
	 the highest.
	 */

	if (fgNeedSwitchChannel) {
		P_BSS_DESC_T prBssDesc;

		prBssInfo->ucPrimaryChannel = ucChannelAnnouncePri;
		prBssInfo->ucVhtChannelWidth = 0;
		prBssInfo->ucVhtChannelFrequencyS1 = 0;
		prBssInfo->ucVhtChannelFrequencyS2 = 0;
		prBssInfo->eBssSCO = 0;

		prBssDesc = scanSearchBssDescByBssid(prAdapter, prBssInfo->aucBSSID);

		if (prBssDesc) {
			DBGLOG(RLM, INFO, "DFS: BSS: " MACSTR " Desc found, channel from %u to %u\n ",
			       MAC2STR(prBssInfo->aucBSSID), prBssDesc->ucChannelNum, ucChannelAnnouncePri);
			prBssDesc->ucChannelNum = ucChannelAnnouncePri;
		} else {
			DBGLOG(RLM, INFO, "DFS: BSS: " MACSTR " Desc is not found\n ", MAC2STR(prBssInfo->aucBSSID));
		}

		if (fgHasWideBandIE != FALSE) {
			prBssInfo->ucVhtChannelWidth = ucChannelAnnounceVhtBw;
			prBssInfo->ucVhtChannelFrequencyS1 = ucChannelAnnounceChannelS1;
			prBssInfo->ucVhtChannelFrequencyS2 = ucChannelAnnounceChannelS2;
		}
		if (fgHasSCOIE != FALSE)
			prBssInfo->eBssSCO = eChannelAnnounceSco;

		if (prBssDesc) {
			kalIndicateChannelSwitch(prAdapter->prGlueInfo,
					prBssInfo->eBssSCO,
					prBssDesc->ucChannelNum);
		}
	}

	if (!fgHasChannelSwitchIE && g_fgHasStopTx) {
#if CFG_SUPPORT_TDLS
		/* TDLS peers */
		TdlsTxCtrl(prAdapter, prBssInfo, TRUE);
#endif
		/* AP */
		qmSetStaRecTxAllowed(prAdapter, prStaRec, TRUE);

		DBGLOG(RLM, EVENT, "[Ch] TxAllowed = TRUE\n");
		g_fgHasStopTx = FALSE;
	}

#endif
	rlmReviseMaxBw(prAdapter, prBssInfo->ucBssIndex, &prBssInfo->eBssSCO,
			(P_ENUM_CHANNEL_WIDTH_P)&prBssInfo->ucVhtChannelWidth,
		&prBssInfo->ucVhtChannelFrequencyS1, &prBssInfo->ucPrimaryChannel);

	rlmRevisePreferBandwidthNss(prAdapter, prBssInfo->ucBssIndex, prStaRec);

	/*printk("Modify ChannelWidth (%d) and Extend (%d)\n",prBssInfo->eBssSCO,prBssInfo->ucVhtChannelWidth);*/

	if (!rlmDomainIsValidRfSetting(prAdapter, prBssInfo->eBand,
				       prBssInfo->ucPrimaryChannel, prBssInfo->eBssSCO,
				       prBssInfo->ucVhtChannelWidth, prBssInfo->ucVhtChannelFrequencyS1,
				       prBssInfo->ucVhtChannelFrequencyS2)) {

		/*Dump IE Inforamtion */
		DBGLOG(RLM, WARN, "rlmRecIeInfoForClient IE Information\n");
		DBGLOG(RLM, WARN, "IE Length = %d\n", u2IELength);
		DBGLOG_MEM8(RLM, WARN, pucDumpIE, u2IELength);

		/*Error Handling for Non-predicted IE - Fixed to set 20MHz */
		prBssInfo->ucVhtChannelWidth = CW_20_40MHZ;
		prBssInfo->ucVhtChannelFrequencyS1 = 0;
		prBssInfo->ucVhtChannelFrequencyS2 = 0;
		prBssInfo->eBssSCO = CHNL_EXT_SCN;
		prBssInfo->ucHtOpInfo1 &= ~(HT_OP_INFO1_SCO | HT_OP_INFO1_STA_CHNL_WIDTH);
	}
#if CFG_SUPPORT_QUIET && 0
	if (!fgHasQuietIE)
		rrmQuietIeNotExist(prAdapter, prBssInfo);
#endif

	/* Check if OBSS scan process will launch */
	if (!prAdapter->fgEnOnlineScan || !prObssScnParam ||
	    !(prStaRec->u2HtCapInfo & HT_CAP_INFO_SUP_CHNL_WIDTH) ||
	    prBssInfo->eBand != BAND_2G4 || !prBssInfo->fg40mBwAllowed) {

		/* Note: it is ok not to stop rObssScanTimer() here */
		prBssInfo->u2ObssScanInterval = 0;
	} else {
		if (prObssScnParam->u2TriggerScanInterval < OBSS_SCAN_MIN_INTERVAL)
			prObssScnParam->u2TriggerScanInterval = OBSS_SCAN_MIN_INTERVAL;
		if (prBssInfo->u2ObssScanInterval != prObssScnParam->u2TriggerScanInterval) {
			DBGLOG(RLM, EVENT, "Start Obss Scan timer [%d--%d]\n",
			prBssInfo->u2ObssScanInterval,
			prObssScnParam->u2TriggerScanInterval);

			prBssInfo->u2ObssScanInterval = prObssScnParam->u2TriggerScanInterval;

			/* Start timer to trigger OBSS scanning */
			cnmTimerStartTimer(prAdapter, &prBssInfo->rObssScanTimer,
					   prBssInfo->u2ObssScanInterval * MSEC_PER_SEC);
		}
	}

	return ucPrimaryChannel;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Update parameters from Association Response frame
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
static VOID
rlmRecAssocRespIeInfoForClient(P_ADAPTER_T prAdapter, P_BSS_INFO_T prBssInfo, PUINT_8 pucIE, UINT_16 u2IELength)
{
	UINT_16 u2Offset;
	P_STA_RECORD_T prStaRec;
	BOOLEAN fgIsHasHtCap = FALSE;
	BOOLEAN fgIsHasVhtCap = FALSE;
	P_BSS_DESC_T prBssDesc;

	ASSERT(prAdapter);
	ASSERT(prBssInfo);
	ASSERT(pucIE);

	prStaRec = prBssInfo->prStaRecOfAP;

	ASSERT(prStaRec);
	if (!prStaRec)
		return;

	prBssDesc = scanSearchBssDescByBssid(prAdapter, prStaRec->aucMacAddr);

	IE_FOR_EACH(pucIE, u2IELength, u2Offset) {
		switch (IE_ID(pucIE)) {
		case ELEM_ID_HT_CAP:
			if (!RLM_NET_IS_11N(prBssInfo) || IE_LEN(pucIE) != (sizeof(IE_HT_CAP_T) - 2))
				break;
			fgIsHasHtCap = TRUE;
			break;
#if CFG_SUPPORT_802_11AC
		case ELEM_ID_VHT_CAP:
			if (!RLM_NET_IS_11AC(prBssInfo) || IE_LEN(pucIE) != (sizeof(IE_VHT_CAP_T) - 2))
				break;
			fgIsHasVhtCap = TRUE;
			break;
#endif
		default:
			break;
		} /* end of switch */
	} /* end of IE_FOR_EACH */

	if (!fgIsHasHtCap) {
		prStaRec->ucDesiredPhyTypeSet &= ~PHY_TYPE_BIT_HT;
		if (prBssDesc) {
			if (prBssDesc->ucPhyTypeSet & PHY_TYPE_BIT_HT) {
				DBGLOG(RLM, WARN, "PhyTypeSet in Beacon and AssocResp are unsync. ");
				DBGLOG(RLM, WARN, "Follow AssocResp to disable HT.\n");
			}
		}
	}
	if (!fgIsHasVhtCap) {
		prStaRec->ucDesiredPhyTypeSet &= ~PHY_TYPE_BIT_VHT;
		if (prBssDesc) {
			if (prBssDesc->ucPhyTypeSet & PHY_TYPE_BIT_VHT) {
				DBGLOG(RLM, WARN, "PhyTypeSet in Beacon and AssocResp are unsync. ");
				DBGLOG(RLM, WARN, "Follow AssocResp to disable VHT.\n");
			}
		}
	}
}

/*----------------------------------------------------------------------------*/
/*!
* \brief AIS or P2P GC.
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
static BOOLEAN
rlmRecBcnFromNeighborForClient(P_ADAPTER_T prAdapter,
			       P_BSS_INFO_T prBssInfo, P_SW_RFB_T prSwRfb, PUINT_8 pucIE, UINT_16 u2IELength)
{
	UINT_16 u2Offset, i;
	UINT_8 ucPriChannel, ucSecChannel;
	ENUM_CHNL_EXT_T eSCO;
	BOOLEAN fgHtBss, fg20mReq;

	ASSERT(prAdapter);
	ASSERT(prBssInfo && prSwRfb);
	ASSERT(pucIE);

	/* Record it to channel list to change 20/40 bandwidth */
	ucPriChannel = 0;
	eSCO = CHNL_EXT_SCN;

	fgHtBss = FALSE;
	fg20mReq = FALSE;

	IE_FOR_EACH(pucIE, u2IELength, u2Offset) {
		switch (IE_ID(pucIE)) {
		case ELEM_ID_HT_CAP:
			{
				P_IE_HT_CAP_T prHtCap;

				if (IE_LEN(pucIE) != (sizeof(IE_HT_CAP_T) - 2))
					break;

				prHtCap = (P_IE_HT_CAP_T) pucIE;
				if (prHtCap->u2HtCapInfo & HT_CAP_INFO_40M_INTOLERANT)
					fg20mReq = TRUE;
				fgHtBss = TRUE;
				break;
			}
		case ELEM_ID_HT_OP:
			{
				P_IE_HT_OP_T prHtOp;

				if (IE_LEN(pucIE) != (sizeof(IE_HT_OP_T) - 2))
					break;

				prHtOp = (P_IE_HT_OP_T) pucIE;
				/* Workaround that some APs fill primary channel field by its
				 * secondary channel, but its DS IE is correct 20110610
				 */
				if (ucPriChannel == 0)
					ucPriChannel = prHtOp->ucPrimaryChannel;

				if ((prHtOp->ucInfo1 & HT_OP_INFO1_SCO) != CHNL_EXT_RES)
					eSCO = (ENUM_CHNL_EXT_T) (prHtOp->ucInfo1 & HT_OP_INFO1_SCO);
				break;
			}
		case ELEM_ID_20_40_BSS_COEXISTENCE:
			{
				P_IE_20_40_COEXIST_T prCoexist;

				if (IE_LEN(pucIE) != (sizeof(IE_20_40_COEXIST_T) - 2))
					break;

				prCoexist = (P_IE_20_40_COEXIST_T) pucIE;
				if (prCoexist->ucData & BSS_COEXIST_40M_INTOLERANT)
					fg20mReq = TRUE;
				break;
			}
		case ELEM_ID_DS_PARAM_SET:
			if (IE_LEN(pucIE) != (sizeof(IE_DS_PARAM_SET_T) - 2))
				break;
			ucPriChannel = DS_PARAM_IE(pucIE)->ucCurrChnl;
			break;

		default:
			break;
		}
	}

	/* To do: Update channel list and 5G band. All channel lists have the same
	 * update procedure. We should give it the entry pointer of desired
	 * channel list.
	 */
	if (HAL_RX_STATUS_GET_RF_BAND(prSwRfb->prRxStatus) != BAND_2G4)
		return FALSE;

	if (ucPriChannel == 0 || ucPriChannel > 14)
		ucPriChannel = HAL_RX_STATUS_GET_CHNL_NUM(prSwRfb->prRxStatus);

	if (fgHtBss) {
		ASSERT(prBssInfo->auc2G_PriChnlList[0] <= CHNL_LIST_SZ_2G);
		for (i = 1; i <= prBssInfo->auc2G_PriChnlList[0] && i <= CHNL_LIST_SZ_2G; i++) {
			if (prBssInfo->auc2G_PriChnlList[i] == ucPriChannel)
				break;
		}
		if ((i > prBssInfo->auc2G_PriChnlList[0]) && (i <= CHNL_LIST_SZ_2G)) {
			prBssInfo->auc2G_PriChnlList[i] = ucPriChannel;
			prBssInfo->auc2G_PriChnlList[0]++;
		}

		/* Update secondary channel */
		if (eSCO != CHNL_EXT_SCN) {
			ucSecChannel = (eSCO == CHNL_EXT_SCA) ? (ucPriChannel + 4) : (ucPriChannel - 4);

			ASSERT(prBssInfo->auc2G_SecChnlList[0] <= CHNL_LIST_SZ_2G);
			for (i = 1; i <= prBssInfo->auc2G_SecChnlList[0] && i <= CHNL_LIST_SZ_2G; i++) {
				if (prBssInfo->auc2G_SecChnlList[i] == ucSecChannel)
					break;
			}
			if ((i > prBssInfo->auc2G_SecChnlList[0]) && (i <= CHNL_LIST_SZ_2G)) {
				prBssInfo->auc2G_SecChnlList[i] = ucSecChannel;
				prBssInfo->auc2G_SecChnlList[0]++;
			}
		}

		/* Update 20M bandwidth request channels */
		if (fg20mReq) {
			ASSERT(prBssInfo->auc2G_20mReqChnlList[0] <= CHNL_LIST_SZ_2G);
			for (i = 1; i <= prBssInfo->auc2G_20mReqChnlList[0] && i <= CHNL_LIST_SZ_2G; i++) {
				if (prBssInfo->auc2G_20mReqChnlList[i] == ucPriChannel)
					break;
			}
			if ((i > prBssInfo->auc2G_20mReqChnlList[0]) && (i <= CHNL_LIST_SZ_2G)) {
				prBssInfo->auc2G_20mReqChnlList[i] = ucPriChannel;
				prBssInfo->auc2G_20mReqChnlList[0]++;
			}
		}
	} else {
		/* Update non-HT channel list */
		ASSERT(prBssInfo->auc2G_NonHtChnlList[0] <= CHNL_LIST_SZ_2G);
		for (i = 1; i <= prBssInfo->auc2G_NonHtChnlList[0] && i <= CHNL_LIST_SZ_2G; i++) {
			if (prBssInfo->auc2G_NonHtChnlList[i] == ucPriChannel)
				break;
		}
		if ((i > prBssInfo->auc2G_NonHtChnlList[0]) && (i <= CHNL_LIST_SZ_2G)) {
			prBssInfo->auc2G_NonHtChnlList[i] = ucPriChannel;
			prBssInfo->auc2G_NonHtChnlList[0]++;
		}

	}

	return FALSE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief AIS or P2P GC.
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
static BOOLEAN
rlmRecBcnInfoForClient(P_ADAPTER_T prAdapter,
		       P_BSS_INFO_T prBssInfo, P_SW_RFB_T prSwRfb, PUINT_8 pucIE, UINT_16 u2IELength)
{
	ASSERT(prAdapter);
	ASSERT(prBssInfo && prSwRfb);
	ASSERT(pucIE);

#if 0				/* SW migration 2010/8/20 */
	/* Note: we shall not update parameters when scanning, otherwise
	 *       channel and bandwidth will not be correct or asserted failure
	 *       during scanning.
	 * Note: remove channel checking. All received Beacons should be processed
	 *       if measurement or other actions are executed in adjacent channels
	 *       and Beacon content checking mechanism is not disabled.
	 */
	if (IS_SCAN_ACTIVE()
	    /* || prBssInfo->ucPrimaryChannel != CHNL_NUM_BY_SWRFB(prSwRfb) */
	    ) {
		return FALSE;
	}
#endif

	/* Handle change of slot time */
	prBssInfo->u2CapInfo = ((P_WLAN_BEACON_FRAME_T) (prSwRfb->pvHeader))->u2CapInfo;
	prBssInfo->fgUseShortSlotTime = ((prBssInfo->u2CapInfo & CAP_INFO_SHORT_SLOT_TIME)
					 || (prBssInfo->eBand != BAND_2G4)) ? TRUE : FALSE;

	rlmRecIeInfoForClient(prAdapter, prBssInfo, pucIE, u2IELength);

	return TRUE;
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
VOID rlmProcessBcn(P_ADAPTER_T prAdapter, P_SW_RFB_T prSwRfb, PUINT_8 pucIE, UINT_16 u2IELength)
{
	P_BSS_INFO_T prBssInfo;
	BOOLEAN fgNewParameter;
	UINT_8 i;

	ASSERT(prAdapter);
	ASSERT(prSwRfb);
	ASSERT(pucIE);

	fgNewParameter = FALSE;

	/* When concurrent networks exist, GO shall have the same handle as
	 * the other BSS, so the Beacon shall be processed for bandwidth and
	 * protection mechanism.
	 * Note1: we do not have 2 AP (GO) cases simultaneously now.
	 * Note2: If we are GO, concurrent AIS AP should detect it and reflect
	 *        action in its Beacon, so AIS STA just follows Beacon from AP.
	 */
	for (i = 0; i < BSS_INFO_NUM; i++) {
		prBssInfo = prAdapter->aprBssInfo[i];

		if (IS_BSS_BOW(prBssInfo))
			continue;

		if (IS_BSS_ACTIVE(prBssInfo)) {
			if (prBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE &&
			    prBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED) {
				/* P2P client or AIS infra STA */
				if (EQUAL_MAC_ADDR(prBssInfo->aucBSSID, ((P_WLAN_MAC_MGMT_HEADER_T)
									 (prSwRfb->pvHeader))->aucBSSID)) {

					fgNewParameter = rlmRecBcnInfoForClient(prAdapter,
										prBssInfo, prSwRfb, pucIE, u2IELength);
				} else {
					fgNewParameter = rlmRecBcnFromNeighborForClient(prAdapter,
											prBssInfo,
											prSwRfb, pucIE, u2IELength);
				}
			}
#if CFG_ENABLE_WIFI_DIRECT
			else if (prAdapter->fgIsP2PRegistered &&
				 (prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT ||
				  prBssInfo->eCurrentOPMode == OP_MODE_P2P_DEVICE)) {
				/* AP scan to check if 20/40M bandwidth is permitted */
				rlmRecBcnFromNeighborForClient(prAdapter, prBssInfo, prSwRfb, pucIE, u2IELength);
			}
#endif
			else if (prBssInfo->eCurrentOPMode == OP_MODE_IBSS) {
				/* To do: Nothing */
				/* To do: Ad-hoc */
			}

			/* Appy new parameters if necessary */
			if (fgNewParameter) {
				rlmSyncOperationParams(prAdapter, prBssInfo);
				fgNewParameter = FALSE;
			}
		}		/* end of IS_BSS_ACTIVE() */
	}
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function should be invoked after judging successful association.
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID rlmProcessAssocRsp(P_ADAPTER_T prAdapter, P_SW_RFB_T prSwRfb, PUINT_8 pucIE, UINT_16 u2IELength)
{
	P_BSS_INFO_T prBssInfo;
	P_STA_RECORD_T prStaRec;
	UINT_8 ucPriChannel;

	ASSERT(prAdapter);
	ASSERT(prSwRfb);
	ASSERT(pucIE);

	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);
	if (!prStaRec)
		return;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);
	if (!prBssInfo)
		return;

	if (prStaRec != prBssInfo->prStaRecOfAP)
		return;

	/* To do: the invoked function is used to clear all members. It may be
	 *        done by center mechanism in invoker.
	 */
	rlmBssReset(prAdapter, prBssInfo);

	prBssInfo->fgUseShortSlotTime = ((prBssInfo->u2CapInfo & CAP_INFO_SHORT_SLOT_TIME)
					 || (prBssInfo->eBand != BAND_2G4)) ? TRUE : FALSE;
	ucPriChannel = rlmRecIeInfoForClient(prAdapter, prBssInfo, pucIE, u2IELength);

	/*Update the parameters from Association Response only,
	*if the parameters need to be updated by both Beacon and Association Response,
	*user should use another function, rlmRecIeInfoForClient()
	*/
	rlmRecAssocRespIeInfoForClient(prAdapter, prBssInfo, pucIE, u2IELength);

	if (prBssInfo->ucPrimaryChannel != ucPriChannel) {
		DBGLOG(RLM, INFO,
		       "Use RF pri channel[%u].Pri channel in HT OP IE is :[%u]\n", prBssInfo->ucPrimaryChannel,
		       ucPriChannel);
	}
	/*Avoid wrong primary channel info in HT operation IE info when accept association response */
#if 0
	if (ucPriChannel > 0)
		prBssInfo->ucPrimaryChannel = ucPriChannel;
#endif

	if (!RLM_NET_IS_11N(prBssInfo) || !(prStaRec->u2HtCapInfo & HT_CAP_INFO_SUP_CHNL_WIDTH))
		prBssInfo->fg40mBwAllowed = FALSE;

	/* Note: Update its capabilities to WTBL by cnmStaRecChangeState(), which
	 *       shall be invoked afterwards.
	 *       Update channel, bandwidth and protection mode by nicUpdateBss()
	 */
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
VOID rlmProcessHtAction(P_ADAPTER_T prAdapter, P_SW_RFB_T prSwRfb)
{
	P_ACTION_NOTIFY_CHNL_WIDTH_FRAME prRxFrame;
	P_ACTION_SM_POWER_SAVE_FRAME prRxSmpsFrame;
	P_STA_RECORD_T prStaRec;
	P_BSS_INFO_T prBssInfo;
	UINT_16 u2HtCapInfoBitmask = 0;

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prRxFrame = (P_ACTION_NOTIFY_CHNL_WIDTH_FRAME) prSwRfb->pvHeader;
	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);

	if (!prStaRec)
		return;

	switch (prRxFrame->ucAction) {
	case ACTION_HT_NOTIFY_CHANNEL_WIDTH:
		if (prStaRec->ucStaState != STA_STATE_3 ||
		    prSwRfb->u2PacketLen < sizeof(ACTION_NOTIFY_CHNL_WIDTH_FRAME)) {
			return;
		}

		/* To do: depending regulation class 13 and 14 based on spec
		 * Note: (ucChannelWidth==1) shall restored back to original capability,
		 *       not current setting to 40MHz BW here
		 */
		 /* 1. Update StaRec for AP/STA mode */
		if (prRxFrame->ucChannelWidth == 0)
			prStaRec->u2HtCapInfo &= ~HT_CAP_INFO_SUP_CHNL_WIDTH;
		else if (prRxFrame->ucChannelWidth == 1)
			prStaRec->u2HtCapInfo |= HT_CAP_INFO_SUP_CHNL_WIDTH;

		cnmStaSendUpdateCmd(prAdapter, prStaRec, NULL, FALSE);

		/* 2. Update BssInfo for STA mode */
		prBssInfo = prAdapter->aprBssInfo[prStaRec->ucBssIndex];
		if (prBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE) {
			if (prRxFrame->ucChannelWidth == 0)
				prBssInfo->ucHtOpInfo1 &= ~HT_OP_INFO1_STA_CHNL_WIDTH;
			if (prRxFrame->ucChannelWidth == 1)
				prBssInfo->ucHtOpInfo1 |= HT_OP_INFO1_STA_CHNL_WIDTH;

			nicUpdateBss(prAdapter, prBssInfo->ucBssIndex);
		}
	break;
	/* Support SM power save */ /* TH3_Huang */
	case ACTION_HT_SM_POWER_SAVE:
		prRxSmpsFrame = (P_ACTION_SM_POWER_SAVE_FRAME) prSwRfb->pvHeader;
		if (prStaRec->ucStaState != STA_STATE_3 ||
			prSwRfb->u2PacketLen < sizeof(ACTION_SM_POWER_SAVE_FRAME)) {
			return;
		}

		/* The SM power enable bit is different definition in HtCap and SMpower IE field */
		if (!(prRxSmpsFrame->ucSmPowerCtrl &
			 (HT_SM_POWER_SAVE_CONTROL_ENABLED|HT_SM_POWER_SAVE_CONTROL_SM_MODE)))
			u2HtCapInfoBitmask |= HT_CAP_INFO_SM_POWER_SAVE;

		/* Support SMPS action frame, TH3_Huang */
		/* Update StaRec if SM power state changed */
		if ((prStaRec->u2HtCapInfo & HT_CAP_INFO_SM_POWER_SAVE) != u2HtCapInfoBitmask) {
			prStaRec->u2HtCapInfo &= ~HT_CAP_INFO_SM_POWER_SAVE;
			prStaRec->u2HtCapInfo |= u2HtCapInfoBitmask;
			DBGLOG(RLM, INFO,
				"rlmProcessHtAction -- SMPS change u2HtCapInfo to (%x)\n",
				 prStaRec->u2HtCapInfo);
			cnmStaSendUpdateCmd(prAdapter, prStaRec, NULL, FALSE);
		}
	break;
	default:
	break;
	}
}

#if CFG_SUPPORT_802_11AC
/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID rlmProcessVhtAction(P_ADAPTER_T prAdapter, P_SW_RFB_T prSwRfb)
{
	P_ACTION_OP_MODE_NOTIFICATION_FRAME prRxFrame;
	P_STA_RECORD_T prStaRec;
	P_BSS_INFO_T prBssInfo;

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prRxFrame = (P_ACTION_OP_MODE_NOTIFICATION_FRAME) prSwRfb->pvHeader;
	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);

	if (!prStaRec)
		return;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);

	if (!prBssInfo)
		return;

	switch (prRxFrame->ucAction) {
	/* Support Operating mode notification action frame, TH3_Huang */
	case ACTION_OPERATING_MODE_NOTIFICATION:
		if (prStaRec->ucStaState != STA_STATE_3 ||
		    prSwRfb->u2PacketLen < sizeof(ACTION_OP_MODE_NOTIFICATION_FRAME)) {
			return;
		}

		if (((prRxFrame->ucOperatingMode & VHT_OP_MODE_RX_NSS_TYPE)
			!= VHT_OP_MODE_RX_NSS_TYPE) &&
			(prStaRec->ucVhtOpMode != prRxFrame->ucOperatingMode)) {
			prStaRec->ucVhtOpMode = prRxFrame->ucOperatingMode;
			DBGLOG(RLM, INFO,
				"rlmProcessVhtAction -- Update ucVhtOpMode to 0x%x\n", prStaRec->ucVhtOpMode);
			cnmStaSendUpdateCmd(prAdapter, prStaRec, NULL, FALSE);
		}
	break;
	default:
	break;
	}
}
#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief This function should be invoked after judging successful association.
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID rlmFillSyncCmdParam(P_CMD_SET_BSS_RLM_PARAM_T prCmdBody, P_BSS_INFO_T prBssInfo)
{
	ASSERT(prCmdBody && prBssInfo);
	if (!prCmdBody || !prBssInfo)
		return;

	prCmdBody->ucBssIndex = prBssInfo->ucBssIndex;
	prCmdBody->ucRfBand = (UINT_8) prBssInfo->eBand;
	prCmdBody->ucPrimaryChannel = prBssInfo->ucPrimaryChannel;
	prCmdBody->ucRfSco = (UINT_8) prBssInfo->eBssSCO;
	prCmdBody->ucErpProtectMode = (UINT_8) prBssInfo->fgErpProtectMode;
	prCmdBody->ucHtProtectMode = (UINT_8) prBssInfo->eHtProtectMode;
	prCmdBody->ucGfOperationMode = (UINT_8) prBssInfo->eGfOperationMode;
	prCmdBody->ucTxRifsMode = (UINT_8) prBssInfo->eRifsOperationMode;
	prCmdBody->u2HtOpInfo3 = prBssInfo->u2HtOpInfo3;
	prCmdBody->u2HtOpInfo2 = prBssInfo->u2HtOpInfo2;
	prCmdBody->ucHtOpInfo1 = prBssInfo->ucHtOpInfo1;
	prCmdBody->ucUseShortPreamble = prBssInfo->fgUseShortPreamble;
	prCmdBody->ucUseShortSlotTime = prBssInfo->fgUseShortSlotTime;
	prCmdBody->ucVhtChannelWidth = prBssInfo->ucVhtChannelWidth;
	prCmdBody->ucVhtChannelFrequencyS1 = prBssInfo->ucVhtChannelFrequencyS1;
	prCmdBody->ucVhtChannelFrequencyS2 = prBssInfo->ucVhtChannelFrequencyS2;
	prCmdBody->u2VhtBasicMcsSet = prBssInfo->u2BSSBasicRateSet;
	prCmdBody->ucNss = prBssInfo->ucNss;

	if (RLM_NET_PARAM_VALID(prBssInfo)) {
		DBGLOG(RLM, INFO, "N=%d b=%d c=%d s=%d e=%d h=%d I=0x%02x l=%d p=%d w=%d s1=%d s2=%d n=%d\n",
		       prCmdBody->ucBssIndex, prCmdBody->ucRfBand,
		       prCmdBody->ucPrimaryChannel, prCmdBody->ucRfSco,
		       prCmdBody->ucErpProtectMode, prCmdBody->ucHtProtectMode,
		       prCmdBody->ucHtOpInfo1, prCmdBody->ucUseShortSlotTime,
		       prCmdBody->ucUseShortPreamble,
		       prCmdBody->ucVhtChannelWidth,
		       prCmdBody->ucVhtChannelFrequencyS1, prCmdBody->ucVhtChannelFrequencyS2,
		       prCmdBody->ucNss);
	} else {
		DBGLOG(RLM, INFO, "N=%d closed\n", prCmdBody->ucBssIndex);
	}
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function will operation parameters based on situations of
*        concurrent networks. Channel, bandwidth, protection mode, supported
*        rate will be modified.
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID rlmSyncOperationParams(P_ADAPTER_T prAdapter, P_BSS_INFO_T prBssInfo)
{
	P_CMD_SET_BSS_RLM_PARAM_T prCmdBody;
	WLAN_STATUS rStatus;

	ASSERT(prAdapter);
	ASSERT(prBssInfo);

	prCmdBody = (P_CMD_SET_BSS_RLM_PARAM_T)
	    cnmMemAlloc(prAdapter, RAM_TYPE_BUF, sizeof(CMD_SET_BSS_RLM_PARAM_T));

	/* ASSERT(prCmdBody); */
	/* To do: exception handle */
	if (!prCmdBody) {
		DBGLOG(RLM, WARN, "No buf for sync RLM params (Net=%d)\n", prBssInfo->ucBssIndex);
		return;
	}

	rlmFillSyncCmdParam(prCmdBody, prBssInfo);

	rStatus = wlanSendSetQueryCmd(prAdapter,	/* prAdapter */
				      CMD_ID_SET_BSS_RLM_PARAM,	/* ucCID */
				      TRUE,	/* fgSetQuery */
				      FALSE,	/* fgNeedResp */
				      FALSE,	/* fgIsOid */
				      NULL,	/* pfCmdDoneHandler */
				      NULL,	/* pfCmdTimeoutHandler */
				      sizeof(CMD_SET_BSS_RLM_PARAM_T),	/* u4SetQueryInfoLen */
				      (PUINT_8) prCmdBody,	/* pucInfoBuffer */
				      NULL,	/* pvSetQueryBuffer */
				      0	/* u4SetQueryBufferLen */
	    );

	/* ASSERT(rStatus == WLAN_STATUS_PENDING); */
	if (rStatus != WLAN_STATUS_PENDING)
		DBGLOG(RLM, WARN, "rlmSyncOperationParams set cmd fail\n");

	cnmMemFree(prAdapter, prCmdBody);
}

#if CFG_SUPPORT_AAA
/*----------------------------------------------------------------------------*/
/*!
* \brief This function should be invoked after judging successful association.
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID rlmProcessAssocReq(P_ADAPTER_T prAdapter, P_SW_RFB_T prSwRfb, PUINT_8 pucIE, UINT_16 u2IELength)
{
	P_BSS_INFO_T prBssInfo;
	P_STA_RECORD_T prStaRec;
	UINT_16 u2Offset;
	P_IE_HT_CAP_T prHtCap;
#if CFG_SUPPORT_802_11AC
	P_IE_VHT_CAP_T prVhtCap;
	UINT_8 ucVhtCapMcsOwnNotSupportOffset;
	/* UINT_8 ucVhtCapMcsPeerNotSupportOffset; */
	P_IE_OP_MODE_NOTIFICATION_T prOPModeNotification;	/* Operation Mode Notification */
#endif

	ASSERT(prAdapter);
	ASSERT(prSwRfb);
	ASSERT(pucIE);

	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);
	if (!prStaRec)
		return;
	ASSERT(prStaRec->ucBssIndex <= MAX_BSS_INDEX);

	prBssInfo = prAdapter->aprBssInfo[prStaRec->ucBssIndex];

	IE_FOR_EACH(pucIE, u2IELength, u2Offset) {
		switch (IE_ID(pucIE)) {
		case ELEM_ID_HT_CAP:
			if (!RLM_NET_IS_11N(prBssInfo) || IE_LEN(pucIE) != (sizeof(IE_HT_CAP_T) - 2))
				break;
			prHtCap = (P_IE_HT_CAP_T) pucIE;
			prStaRec->ucMcsSet = prHtCap->rSupMcsSet.aucRxMcsBitmask[0];
			prStaRec->fgSupMcs32 = (prHtCap->rSupMcsSet.aucRxMcsBitmask[32 / 8] & BIT(0)) ? TRUE : FALSE;

			kalMemCopy(prStaRec->aucRxMcsBitmask, prHtCap->rSupMcsSet.aucRxMcsBitmask,
				   sizeof(prStaRec->aucRxMcsBitmask) /*SUP_MCS_RX_BITMASK_OCTET_NUM */);

			prStaRec->u2HtCapInfo = prHtCap->u2HtCapInfo;

			/* Set Short LDPC Tx capability */
			if (IS_FEATURE_FORCE_ENABLED(prAdapter->rWifiVar.ucTxLdpc))
				prStaRec->u2HtCapInfo |= HT_CAP_INFO_LDPC_CAP;
			else if (IS_FEATURE_DISABLED(prAdapter->rWifiVar.ucTxLdpc))
				prStaRec->u2HtCapInfo &= ~HT_CAP_INFO_LDPC_CAP;

			/* Set STBC Tx capability */
			if (IS_FEATURE_FORCE_ENABLED(prAdapter->rWifiVar.ucTxStbc))
				prStaRec->u2HtCapInfo |= HT_CAP_INFO_RX_STBC;
			else if (IS_FEATURE_DISABLED(prAdapter->rWifiVar.ucTxStbc))
				prStaRec->u2HtCapInfo &= ~HT_CAP_INFO_RX_STBC;
			/* Set Short GI Tx capability */
			if (IS_FEATURE_FORCE_ENABLED(prAdapter->rWifiVar.ucTxShortGI)) {
				prStaRec->u2HtCapInfo |= HT_CAP_INFO_SHORT_GI_20M;
				prStaRec->u2HtCapInfo |= HT_CAP_INFO_SHORT_GI_40M;
			} else if (IS_FEATURE_DISABLED(prAdapter->rWifiVar.ucTxShortGI)) {
				prStaRec->u2HtCapInfo &= ~HT_CAP_INFO_SHORT_GI_20M;
				prStaRec->u2HtCapInfo &= ~HT_CAP_INFO_SHORT_GI_40M;
			}

			/* Set HT Greenfield Tx capability */
			if (IS_FEATURE_FORCE_ENABLED(prAdapter->rWifiVar.ucTxGf))
				prStaRec->u2HtCapInfo |= HT_CAP_INFO_HT_GF;
			else if (IS_FEATURE_DISABLED(prAdapter->rWifiVar.ucTxGf))
				prStaRec->u2HtCapInfo &= ~HT_CAP_INFO_HT_GF;

			prStaRec->ucAmpduParam = prHtCap->ucAmpduParam;
			prStaRec->u2HtExtendedCap = prHtCap->u2HtExtendedCap;
			prStaRec->u4TxBeamformingCap = prHtCap->u4TxBeamformingCap;
			prStaRec->ucAselCap = prHtCap->ucAselCap;
			break;

#if CFG_SUPPORT_802_11AC
		case ELEM_ID_VHT_CAP:
			if (!RLM_NET_IS_11AC(prBssInfo) || IE_LEN(pucIE) != (sizeof(IE_VHT_CAP_T) - 2))
				break;

			prVhtCap = (P_IE_VHT_CAP_T) pucIE;

			prStaRec->u4VhtCapInfo = prVhtCap->u4VhtCapInfo;

			/* Set Tx LDPC capability */
			if (IS_FEATURE_FORCE_ENABLED(prAdapter->rWifiVar.ucTxLdpc))
				prStaRec->u4VhtCapInfo |= VHT_CAP_INFO_RX_LDPC;
			else if (IS_FEATURE_DISABLED(prAdapter->rWifiVar.ucTxLdpc))
				prStaRec->u4VhtCapInfo &= ~VHT_CAP_INFO_RX_LDPC;

			/* Set Tx STBC capability */
			if (IS_FEATURE_FORCE_ENABLED(prAdapter->
						rWifiVar.ucTxStbc)) {
				prStaRec->u4VhtCapInfo |=
					VHT_CAP_INFO_RX_STBC_MASK;
			} else if (IS_FEATURE_DISABLED(prAdapter->
						rWifiVar.ucTxStbc)) {
				prStaRec->u4VhtCapInfo &=
					~VHT_CAP_INFO_RX_STBC_MASK;
			}
			/* Set Tx TXOP PS capability */
			if (IS_FEATURE_FORCE_ENABLED(prAdapter->rWifiVar.ucTxopPsTx))
				prStaRec->u4VhtCapInfo |= VHT_CAP_INFO_VHT_TXOP_PS;
			else if (IS_FEATURE_DISABLED(prAdapter->rWifiVar.ucTxopPsTx))
				prStaRec->u4VhtCapInfo &= ~VHT_CAP_INFO_VHT_TXOP_PS;

			/* Set Tx Short GI capability */
			if (IS_FEATURE_FORCE_ENABLED(prAdapter->rWifiVar.ucTxShortGI)) {
				prStaRec->u4VhtCapInfo |= VHT_CAP_INFO_SHORT_GI_80;
				prStaRec->u4VhtCapInfo |= VHT_CAP_INFO_SHORT_GI_160_80P80;
			} else if (IS_FEATURE_DISABLED(prAdapter->rWifiVar.ucTxShortGI)) {
				prStaRec->u4VhtCapInfo &= ~VHT_CAP_INFO_SHORT_GI_80;
				prStaRec->u4VhtCapInfo &= ~VHT_CAP_INFO_SHORT_GI_160_80P80;
			}

			/*Set Vht Rx Mcs Map upon peer's capability and our capability */
			prStaRec->u2VhtRxMcsMap = prVhtCap->rVhtSupportedMcsSet.u2RxMcsMap;
			if (wlanGetSupportNss(prAdapter, prStaRec->ucBssIndex) < 8) {
				ucVhtCapMcsOwnNotSupportOffset =
					wlanGetSupportNss(prAdapter, prStaRec->ucBssIndex) * 2;
				prStaRec->u2VhtRxMcsMap |= BITS(ucVhtCapMcsOwnNotSupportOffset, 15);
				/*Mark Rx Mcs Map which we don't support*/
			}
			if (prStaRec->u2VhtRxMcsMap != prVhtCap->rVhtSupportedMcsSet.u2RxMcsMap)
				DBGLOG(RLM, INFO, "Change VhtRxMcsMap from 0x%x to 0x%x due to our Nss setting\n",
								prVhtCap->rVhtSupportedMcsSet.u2RxMcsMap,
								prStaRec->u2VhtRxMcsMap);

			prStaRec->u2VhtRxHighestSupportedDataRate =
			    prVhtCap->rVhtSupportedMcsSet.u2RxHighestSupportedDataRate;
			prStaRec->u2VhtTxMcsMap = prVhtCap->rVhtSupportedMcsSet.u2TxMcsMap;
			prStaRec->u2VhtTxHighestSupportedDataRate =
				prVhtCap->rVhtSupportedMcsSet.u2TxHighestSupportedDataRate;

			/* Set initial value of VHT OP mode */
			prStaRec->ucVhtOpMode = 0;
			switch (prBssInfo->ucVhtChannelWidth) {
			case VHT_OP_CHANNEL_WIDTH_20_40:
				prStaRec->ucVhtOpMode |= VHT_OP_MODE_CHANNEL_WIDTH_40;
				break;
			case VHT_OP_CHANNEL_WIDTH_80:
				prStaRec->ucVhtOpMode |= VHT_OP_MODE_CHANNEL_WIDTH_80;
				break;
			case VHT_OP_CHANNEL_WIDTH_160:
			case VHT_OP_CHANNEL_WIDTH_80P80:
				prStaRec->ucVhtOpMode |= VHT_OP_MODE_CHANNEL_WIDTH_160_80P80;
				break;
			default:
				prStaRec->ucVhtOpMode |= VHT_OP_MODE_CHANNEL_WIDTH_80;
				break;
			}
			prStaRec->ucVhtOpMode |= ((prBssInfo->ucNss-1) <<
				VHT_OP_MODE_RX_NSS_OFFSET) & VHT_OP_MODE_RX_NSS;

			break;
		case ELEM_ID_OP_MODE:
			if (!RLM_NET_IS_11AC(prBssInfo) || IE_LEN(pucIE) != (sizeof(IE_OP_MODE_NOTIFICATION_T) - 2))
				break;
			prOPModeNotification = (P_IE_OP_MODE_NOTIFICATION_T) pucIE;

			if ((prOPModeNotification->ucOpMode & VHT_OP_MODE_RX_NSS_TYPE)
			    != VHT_OP_MODE_RX_NSS_TYPE) {
				prStaRec->ucVhtOpMode = prOPModeNotification->ucOpMode;
			}

			break;

#endif

		default:
			break;
		}		/* end of switch */
	}			/* end of IE_FOR_EACH */
}
#endif /* CFG_SUPPORT_AAA */

/*----------------------------------------------------------------------------*/
/*!
* \brief It is for both STA and AP modes
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID rlmBssInitForAPandIbss(P_ADAPTER_T prAdapter, P_BSS_INFO_T prBssInfo)
{
	ASSERT(prAdapter);
	ASSERT(prBssInfo);

#if CFG_ENABLE_WIFI_DIRECT
	if (prAdapter->fgIsP2PRegistered && prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT)
		rlmBssInitForAP(prAdapter, prBssInfo);
#endif
}

/*----------------------------------------------------------------------------*/
/*!
* \brief It is for both STA and AP modes
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID rlmBssAborted(P_ADAPTER_T prAdapter, P_BSS_INFO_T prBssInfo)
{
	ASSERT(prAdapter);
	ASSERT(prBssInfo);

	rlmBssReset(prAdapter, prBssInfo);

	prBssInfo->fg40mBwAllowed = FALSE;
	prBssInfo->fgAssoc40mBwAllowed = FALSE;

	/* Assume FW state is updated by CMD_ID_SET_BSS_INFO, so
	 * the sync CMD is not needed here.
	 */
}

/*----------------------------------------------------------------------------*/
/*!
* \brief All RLM timers will also be stopped.
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
static VOID rlmBssReset(P_ADAPTER_T prAdapter, P_BSS_INFO_T prBssInfo)
{
	ASSERT(prAdapter);
	ASSERT(prBssInfo);

	/* HT related parameters */
	prBssInfo->ucHtOpInfo1 = 0;	/* RIFS disabled. 20MHz */
	prBssInfo->u2HtOpInfo2 = 0;
	prBssInfo->u2HtOpInfo3 = 0;

#if CFG_SUPPORT_802_11AC
	prBssInfo->ucVhtChannelWidth = 0;	/* VHT_OP_CHANNEL_WIDTH_80; */
	prBssInfo->ucVhtChannelFrequencyS1 = 0;	/* 42; */
	prBssInfo->ucVhtChannelFrequencyS2 = 0;
	prBssInfo->u2VhtBasicMcsSet = 0;	/* 0xFFFF; */
#endif

	prBssInfo->eBssSCO = 0;
	prBssInfo->fgErpProtectMode = 0;
	prBssInfo->eHtProtectMode = 0;
	prBssInfo->eGfOperationMode = 0;
	prBssInfo->eRifsOperationMode = 0;

	/* OBSS related parameters */
	prBssInfo->auc2G_20mReqChnlList[0] = 0;
	prBssInfo->auc2G_NonHtChnlList[0] = 0;
	prBssInfo->auc2G_PriChnlList[0] = 0;
	prBssInfo->auc2G_SecChnlList[0] = 0;
	prBssInfo->auc5G_20mReqChnlList[0] = 0;
	prBssInfo->auc5G_NonHtChnlList[0] = 0;
	prBssInfo->auc5G_PriChnlList[0] = 0;
	prBssInfo->auc5G_SecChnlList[0] = 0;

	/* All RLM timers will also be stopped */
	cnmTimerStopTimer(prAdapter, &prBssInfo->rObssScanTimer);
	prBssInfo->u2ObssScanInterval = 0;

	prBssInfo->fgObssErpProtectMode = 0;	/* GO only */
	prBssInfo->eObssHtProtectMode = 0;	/* GO only */
	prBssInfo->eObssGfOperationMode = 0;	/* GO only */
	prBssInfo->fgObssRifsOperationMode = 0;	/* GO only */
	prBssInfo->fgObssActionForcedTo20M = 0;	/* GO only */
	prBssInfo->fgObssBeaconForcedTo20M = 0;	/* GO only */
}

#if CFG_SUPPORT_TDLS
/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
UINT_32 rlmFillVhtCapIEByAdapter(P_ADAPTER_T prAdapter, P_BSS_INFO_T prBssInfo, UINT_8 *pOutBuf)
{
	P_IE_VHT_CAP_T prVhtCap;
	P_VHT_SUPPORTED_MCS_FIELD prVhtSupportedMcsSet;
	UINT_8 i;

	ASSERT(prAdapter);
	ASSERT(prBssInfo);
	/* ASSERT(prMsduInfo); */

	prVhtCap = (P_IE_VHT_CAP_T) pOutBuf;

	prVhtCap->ucId = ELEM_ID_VHT_CAP;
	prVhtCap->ucLength = sizeof(IE_VHT_CAP_T) - ELEM_HDR_LEN;
	prVhtCap->u4VhtCapInfo = VHT_CAP_INFO_DEFAULT_VAL;

	if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucRxShortGI))
		prVhtCap->u4VhtCapInfo |= VHT_CAP_INFO_SHORT_GI_80;

	if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucRxLdpc))
		prVhtCap->u4VhtCapInfo |= VHT_CAP_INFO_RX_LDPC;

	if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucRxStbc))
		prVhtCap->u4VhtCapInfo |= VHT_CAP_INFO_RX_STBC_ONE_STREAM;

	/*set MCS map */
	prVhtSupportedMcsSet = &prVhtCap->rVhtSupportedMcsSet;
	kalMemZero((PVOID) prVhtSupportedMcsSet, sizeof(VHT_SUPPORTED_MCS_FIELD));

	for (i = 0; i < 8; i++) {
		prVhtSupportedMcsSet->u2RxMcsMap |= BITS(2 * i, (2 * i + 1));
		prVhtSupportedMcsSet->u2TxMcsMap |= BITS(2 * i, (2 * i + 1));
	}

	prVhtSupportedMcsSet->u2RxMcsMap &= (VHT_CAP_INFO_MCS_MAP_MCS9 << VHT_CAP_INFO_MCS_1SS_OFFSET);
	prVhtSupportedMcsSet->u2TxMcsMap &= (VHT_CAP_INFO_MCS_MAP_MCS9 << VHT_CAP_INFO_MCS_1SS_OFFSET);
	prVhtSupportedMcsSet->u2RxHighestSupportedDataRate = VHT_CAP_INFO_DEFAULT_HIGHEST_DATA_RATE;
	prVhtSupportedMcsSet->u2TxHighestSupportedDataRate = VHT_CAP_INFO_DEFAULT_HIGHEST_DATA_RATE;

	ASSERT(IE_SIZE(prVhtCap) <= (ELEM_HDR_LEN + ELEM_MAX_LEN_VHT_CAP));

	return IE_SIZE(prVhtCap);
}
#endif

#if CFG_SUPPORT_TDLS
/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
UINT_32
rlmFillHtCapIEByParams(BOOLEAN fg40mAllowed,
		       BOOLEAN fgShortGIDisabled,
		       UINT_8 u8SupportRxSgi20,
		       UINT_8 u8SupportRxSgi40, UINT_8 u8SupportRxGf, ENUM_OP_MODE_T eCurrentOPMode, UINT_8 *pOutBuf)
{
	P_IE_HT_CAP_T prHtCap;
	P_SUP_MCS_SET_FIELD prSupMcsSet;

	ASSERT(pOutBuf);

	prHtCap = (P_IE_HT_CAP_T) pOutBuf;

	/* Add HT capabilities IE */
	prHtCap->ucId = ELEM_ID_HT_CAP;
	prHtCap->ucLength = sizeof(IE_HT_CAP_T) - ELEM_HDR_LEN;

	prHtCap->u2HtCapInfo = HT_CAP_INFO_DEFAULT_VAL;
	if (!fg40mAllowed) {
		prHtCap->u2HtCapInfo &= ~(HT_CAP_INFO_SUP_CHNL_WIDTH |
					  HT_CAP_INFO_SHORT_GI_40M | HT_CAP_INFO_DSSS_CCK_IN_40M);
	}
	if (fgShortGIDisabled)
		prHtCap->u2HtCapInfo &= ~(HT_CAP_INFO_SHORT_GI_20M | HT_CAP_INFO_SHORT_GI_40M);

	if (u8SupportRxSgi20 == 2)
		prHtCap->u2HtCapInfo &= ~(HT_CAP_INFO_SHORT_GI_20M);
	if (u8SupportRxSgi40 == 2)
		prHtCap->u2HtCapInfo &= ~(HT_CAP_INFO_SHORT_GI_40M);
	if (u8SupportRxGf == 2)
		prHtCap->u2HtCapInfo &= ~(HT_CAP_INFO_HT_GF);

	prHtCap->ucAmpduParam = AMPDU_PARAM_DEFAULT_VAL;

	prSupMcsSet = &prHtCap->rSupMcsSet;
	kalMemZero((PVOID)&prSupMcsSet->aucRxMcsBitmask[0], SUP_MCS_RX_BITMASK_OCTET_NUM);

	prSupMcsSet->aucRxMcsBitmask[0] = BITS(0, 7);

	if (fg40mAllowed)
		prSupMcsSet->aucRxMcsBitmask[32 / 8] = BIT(0);	/* MCS32 */
	prSupMcsSet->u2RxHighestSupportedRate = SUP_MCS_RX_DEFAULT_HIGHEST_RATE;
	prSupMcsSet->u4TxRateInfo = SUP_MCS_TX_DEFAULT_VAL;

	prHtCap->u2HtExtendedCap = HT_EXT_CAP_DEFAULT_VAL;
	if (!fg40mAllowed || eCurrentOPMode != OP_MODE_INFRASTRUCTURE)
		prHtCap->u2HtExtendedCap &= ~(HT_EXT_CAP_PCO | HT_EXT_CAP_PCO_TRANS_TIME_NONE);

	prHtCap->u4TxBeamformingCap = TX_BEAMFORMING_CAP_DEFAULT_VAL;

	prHtCap->ucAselCap = ASEL_CAP_DEFAULT_VAL;

	ASSERT(IE_SIZE(prHtCap) <= (ELEM_HDR_LEN + ELEM_MAX_LEN_HT_CAP));

	return IE_SIZE(prHtCap);
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
UINT_32 rlmFillHtCapIEByAdapter(P_ADAPTER_T prAdapter, P_BSS_INFO_T prBssInfo, UINT_8 *pOutBuf)
{
	P_IE_HT_CAP_T prHtCap;
	P_SUP_MCS_SET_FIELD prSupMcsSet;
	BOOLEAN fg40mAllowed;

	ASSERT(prAdapter);
	ASSERT(prBssInfo);
	ASSERT(pOutBuf);

	fg40mAllowed = prBssInfo->fgAssoc40mBwAllowed;

	prHtCap = (P_IE_HT_CAP_T) pOutBuf;

	/* Add HT capabilities IE */
	prHtCap->ucId = ELEM_ID_HT_CAP;
	prHtCap->ucLength = sizeof(IE_HT_CAP_T) - ELEM_HDR_LEN;

	prHtCap->u2HtCapInfo = HT_CAP_INFO_DEFAULT_VAL;
	if (!fg40mAllowed) {
		prHtCap->u2HtCapInfo &= ~(HT_CAP_INFO_SUP_CHNL_WIDTH |
					  HT_CAP_INFO_SHORT_GI_40M | HT_CAP_INFO_DSSS_CCK_IN_40M);
	}
	if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucRxShortGI))
		prHtCap->u2HtCapInfo |= (HT_CAP_INFO_SHORT_GI_20M | HT_CAP_INFO_SHORT_GI_40M);

	if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucRxLdpc))
		prHtCap->u2HtCapInfo |= HT_CAP_INFO_LDPC_CAP;

	if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucRxStbc))
		prHtCap->u2HtCapInfo |= HT_CAP_INFO_RX_STBC_1_SS;

	prHtCap->ucAmpduParam = AMPDU_PARAM_DEFAULT_VAL;

	prSupMcsSet = &prHtCap->rSupMcsSet;
	kalMemZero((PVOID)&prSupMcsSet->aucRxMcsBitmask[0], SUP_MCS_RX_BITMASK_OCTET_NUM);

	prSupMcsSet->aucRxMcsBitmask[0] = BITS(0, 7);

	if (fg40mAllowed && IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucMCS32))
		prSupMcsSet->aucRxMcsBitmask[32 / 8] = BIT(0);	/* MCS32 */
	prSupMcsSet->u2RxHighestSupportedRate = SUP_MCS_RX_DEFAULT_HIGHEST_RATE;
	prSupMcsSet->u4TxRateInfo = SUP_MCS_TX_DEFAULT_VAL;

	prHtCap->u2HtExtendedCap = HT_EXT_CAP_DEFAULT_VAL;
	if (!fg40mAllowed || prBssInfo->eCurrentOPMode != OP_MODE_INFRASTRUCTURE)
		prHtCap->u2HtExtendedCap &= ~(HT_EXT_CAP_PCO | HT_EXT_CAP_PCO_TRANS_TIME_NONE);

	prHtCap->u4TxBeamformingCap = TX_BEAMFORMING_CAP_DEFAULT_VAL;

	prHtCap->ucAselCap = ASEL_CAP_DEFAULT_VAL;

	ASSERT(IE_SIZE(prHtCap) <= (ELEM_HDR_LEN + ELEM_MAX_LEN_HT_CAP));

	return IE_SIZE(prHtCap);

}

#endif

#if CFG_SUPPORT_DFS
/*----------------------------------------------------------------------------*/
/*!
* @brief This function will compose the TPC Report frame.
*
* @param[in] prAdapter              Pointer to the Adapter structure.
* @param[in] prStaRec               Pointer to the STA_RECORD_T.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
static VOID
tpcComposeReportFrame(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec, IN PFN_TX_DONE_HANDLER pfTxDoneHandler)
{
	P_MSDU_INFO_T prMsduInfo;
	P_BSS_INFO_T prBssInfo;
	P_ACTION_TPC_REPORT_FRAME prTxFrame;
	UINT_16 u2PayloadLen;

	ASSERT(prAdapter);
	ASSERT(prStaRec);

	prBssInfo = &prAdapter->rWifiVar.arBssInfoPool[prStaRec->ucBssIndex];
	ASSERT(prBssInfo);

	prMsduInfo = (P_MSDU_INFO_T) cnmMgtPktAlloc(prAdapter, MAC_TX_RESERVED_FIELD + PUBLIC_ACTION_MAX_LEN);

	if (!prMsduInfo)
		return;

	prTxFrame = (P_ACTION_TPC_REPORT_FRAME)
	    ((ULONG) (prMsduInfo->prPacket) + MAC_TX_RESERVED_FIELD);

	prTxFrame->u2FrameCtrl = MAC_FRAME_ACTION;

	COPY_MAC_ADDR(prTxFrame->aucDestAddr, prStaRec->aucMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucSrcAddr, prBssInfo->aucOwnMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucBSSID, prBssInfo->aucBSSID);

	prTxFrame->ucCategory = CATEGORY_SPEC_MGT;
	prTxFrame->ucAction = ACTION_TPC_REPORT;

	/* 3 Compose the frame body's frame. */
	prTxFrame->ucDialogToken = prStaRec->ucSmDialogToken;
	prTxFrame->ucElemId = ELEM_ID_TPC_REPORT;
	prTxFrame->ucLength = sizeof(prTxFrame->ucLinkMargin)+sizeof(prTxFrame->ucTransPwr);
	prTxFrame->ucTransPwr = prAdapter->u4GetTxPower;
	prTxFrame->ucLinkMargin = prAdapter->rLinkQuality.cRssi - (0 - MIN_RCV_PWR);

	u2PayloadLen = ACTION_SM_TPC_REPORT_LEN;

	/* 4 Update information of MSDU_INFO_T */
	TX_SET_MMPDU(prAdapter,
		     prMsduInfo,
		     prStaRec->ucBssIndex,
		     prStaRec->ucIndex,
		     WLAN_MAC_MGMT_HEADER_LEN,
		     WLAN_MAC_MGMT_HEADER_LEN + u2PayloadLen, pfTxDoneHandler, MSDU_RATE_MODE_AUTO);

	DBGLOG(RLM, TRACE, "ucDialogToken %d ucTransPwr %d ucLinkMargin %d\n",
	       prTxFrame->ucDialogToken, prTxFrame->ucTransPwr, prTxFrame->ucLinkMargin);

	/* 4 Enqueue the frame to send this action frame. */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);

	return;

}				/* end of tpcComposeReportFrame() */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will compose the Measurement Report frame.
*
* @param[in] prAdapter              Pointer to the Adapter structure.
* @param[in] prStaRec               Pointer to the STA_RECORD_T.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
static VOID
msmtComposeReportFrame(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec, IN PFN_TX_DONE_HANDLER pfTxDoneHandler)
{
	P_MSDU_INFO_T prMsduInfo;
	P_BSS_INFO_T prBssInfo;
	P_ACTION_SM_REPORT_FRAME prTxFrame;
	P_IE_MEASUREMENT_REPORT_T prMeasurementRepIE;
	PUINT_8 pucIE;
	UINT_16 u2PayloadLen;

	ASSERT(prAdapter);
	ASSERT(prStaRec);

	prBssInfo = &prAdapter->rWifiVar.arBssInfoPool[prStaRec->ucBssIndex];
	ASSERT(prBssInfo);

	prMsduInfo = (P_MSDU_INFO_T) cnmMgtPktAlloc(prAdapter, MAC_TX_RESERVED_FIELD + PUBLIC_ACTION_MAX_LEN);

	if (!prMsduInfo)
		return;

	prTxFrame = (P_ACTION_SM_REPORT_FRAME)
	    ((ULONG) (prMsduInfo->prPacket) + MAC_TX_RESERVED_FIELD);
	pucIE = prTxFrame->aucInfoElem;
	prMeasurementRepIE = SM_MEASUREMENT_REP_IE(pucIE);

	prTxFrame->u2FrameCtrl = MAC_FRAME_ACTION;

	COPY_MAC_ADDR(prTxFrame->aucDestAddr, prStaRec->aucMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucSrcAddr, prBssInfo->aucOwnMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucBSSID, prBssInfo->aucBSSID);

	prTxFrame->ucCategory = CATEGORY_SPEC_MGT;
	prTxFrame->ucAction = ACTION_MEASUREMENT_REPORT;

	/* 3 Compose the frame body's frame. */
	prTxFrame->ucDialogToken = prStaRec->ucSmDialogToken;
	prMeasurementRepIE->ucId = ELEM_ID_MEASUREMENT_REPORT;

#if 0
	if (prStaRec->ucSmMsmtRequestMode == ELEM_RM_TYPE_BASIC_REQ) {
		prMeasurementRepIE->ucLength = sizeof(SM_BASIC_REPORT_T) + 3;
		u2PayloadLen = ACTION_SM_MEASURE_REPORT_LEN+ACTION_SM_BASIC_REPORT_LEN;
	} else if (prStaRec->ucSmMsmtRequestMode == ELEM_RM_TYPE_CCA_REQ) {
		prMeasurementRepIE->ucLength = sizeof(SM_CCA_REPORT_T) + 3;
		u2PayloadLen = ACTION_SM_MEASURE_REPORT_LEN+ACTION_SM_CCA_REPORT_LEN;
	} else if (prStaRec->ucSmMsmtRequestMode == ELEM_RM_TYPE_RPI_HISTOGRAM_REQ) {
		prMeasurementRepIE->ucLength = sizeof(SM_RPI_REPORT_T) + 3;
		u2PayloadLen = ACTION_SM_MEASURE_REPORT_LEN+ACTION_SM_PRI_REPORT_LEN;
	} else {
		prMeasurementRepIE->ucLength = 3;
		u2PayloadLen = ACTION_SM_MEASURE_REPORT_LEN;
	}
#else
	prMeasurementRepIE->ucLength = 3;
	u2PayloadLen = ACTION_SM_MEASURE_REPORT_LEN;
	prMeasurementRepIE->ucToken = prStaRec->ucSmMsmtToken;
	prMeasurementRepIE->ucReportMode = BIT(1);
	prMeasurementRepIE->ucMeasurementType = prStaRec->ucSmMsmtRequestMode;
#endif

	/* 4 Update information of MSDU_INFO_T */
	TX_SET_MMPDU(prAdapter,
		     prMsduInfo,
		     prStaRec->ucBssIndex,
		     prStaRec->ucIndex,
		     WLAN_MAC_MGMT_HEADER_LEN,
		     WLAN_MAC_MGMT_HEADER_LEN + u2PayloadLen, pfTxDoneHandler, MSDU_RATE_MODE_AUTO);

	DBGLOG(RLM, TRACE, "ucDialogToken %d ucToken %d ucReportMode %d ucMeasurementType %d\n",
	       prTxFrame->ucDialogToken, prMeasurementRepIE->ucToken,
	       prMeasurementRepIE->ucReportMode, prMeasurementRepIE->ucMeasurementType);

	/* 4 Enqueue the frame to send this action frame. */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);

	return;

}				/* end of msmtComposeReportFrame() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This function handle spectrum management action frame
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID rlmProcessSpecMgtAction(P_ADAPTER_T prAdapter, P_SW_RFB_T prSwRfb)
{
	PUINT_8 pucIE;
	P_STA_RECORD_T prStaRec;
	P_BSS_INFO_T prBssInfo;
	UINT_16 u2IELength;
	UINT_16 u2Offset = 0;
	P_IE_CHANNEL_SWITCH_T prChannelSwitchAnnounceIE;
	P_IE_SECONDARY_OFFSET_T prSecondaryOffsetIE;
	P_IE_WIDE_BAND_CHANNEL_T prWideBandChannelIE;
	P_IE_TPC_REQ_T prTpcReqIE;
	P_IE_TPC_REPORT_T prTpcRepIE;
	P_IE_MEASUREMENT_REQ_T prMeasurementReqIE;
	P_IE_MEASUREMENT_REPORT_T prMeasurementRepIE;
	P_ACTION_SM_REQ_FRAME prRxFrame;
	BOOLEAN fgHasWideBandIE = FALSE;
	BOOLEAN fgHasSCOIE = FALSE;
	BOOLEAN fgHasChannelSwitchIE = FALSE;
	BOOLEAN fgNeedSwitchChannel = FALSE;

	DBGLOG(RLM, INFO, "[Mgt Action]rlmProcessSpecMgtAction\n");
	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	u2IELength = prSwRfb->u2PacketLen -
		(UINT_16) OFFSET_OF(ACTION_SM_REQ_FRAME, aucInfoElem[0]);

	prRxFrame = (P_ACTION_SM_REQ_FRAME) prSwRfb->pvHeader;
	pucIE = prRxFrame->aucInfoElem;

	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);
	if (!prStaRec)
		return;

	if (prStaRec->ucBssIndex > MAX_BSS_INDEX)
		return;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);

	prStaRec->ucSmDialogToken = prRxFrame->ucDialogToken;

	DBGLOG_MEM8(RLM, INFO, pucIE, u2IELength);
	switch (prRxFrame->ucAction) {
	case ACTION_MEASUREMENT_REQ:
		DBGLOG(RLM, INFO, "[Mgt Action] Measure Request\n");
		prMeasurementReqIE = SM_MEASUREMENT_REQ_IE(pucIE);
		if (prMeasurementReqIE->ucId == ELEM_ID_MEASUREMENT_REQ) {
			prStaRec->ucSmMsmtRequestMode = prMeasurementReqIE->ucRequestMode;
			prStaRec->ucSmMsmtToken = prMeasurementReqIE->ucToken;
			msmtComposeReportFrame(prAdapter, prStaRec, NULL);
		}

		break;
	case ACTION_MEASUREMENT_REPORT:
		DBGLOG(RLM, INFO, "[Mgt Action] Measure Report\n");
		prMeasurementRepIE = SM_MEASUREMENT_REP_IE(pucIE);
		if (prMeasurementRepIE->ucId == ELEM_ID_MEASUREMENT_REPORT)
			DBGLOG(RLM, TRACE, "[Mgt Action] Correct Measurement report IE !!\n");
		break;
	case ACTION_TPC_REQ:
		DBGLOG(RLM, INFO, "[Mgt Action] TPC Request\n");
		prTpcReqIE = SM_TPC_REQ_IE(pucIE);

		if (prTpcReqIE->ucId == ELEM_ID_TPC_REQ)
			tpcComposeReportFrame(prAdapter, prStaRec, NULL);

		break;
	case ACTION_TPC_REPORT:
		DBGLOG(RLM, INFO, "[Mgt Action] TPC Report\n");
		prTpcRepIE = SM_TPC_REP_IE(pucIE);

		if (prTpcRepIE->ucId == ELEM_ID_TPC_REPORT)
			DBGLOG(RLM, TRACE, "[Mgt Action] Correct TPC report IE !!\n");

		break;
	case ACTION_CHNL_SWITCH:
		IE_FOR_EACH(pucIE, u2IELength, u2Offset) {
			switch (IE_ID(pucIE)) {

			case ELEM_ID_WIDE_BAND_CHANNEL_SWITCH:
				if (!RLM_NET_IS_11AC(prBssInfo) ||
				    IE_LEN(pucIE) != (sizeof(IE_WIDE_BAND_CHANNEL_T) - 2)) {
					DBGLOG(RLM, INFO, "[Mgt Action] ELEM_ID_WIDE_BAND_CHANNEL_SWITCH, Length\n");
					break;
				}
				DBGLOG(RLM, INFO, "[Mgt Action] ELEM_ID_WIDE_BAND_CHANNEL_SWITCH, 11AC\n");
				prWideBandChannelIE = (P_IE_WIDE_BAND_CHANNEL_T) pucIE;
				prBssInfo->ucVhtChannelWidth = prWideBandChannelIE->ucNewChannelWidth;
				prBssInfo->ucVhtChannelFrequencyS1 = prWideBandChannelIE->ucChannelS1;
				prBssInfo->ucVhtChannelFrequencyS2 = prWideBandChannelIE->ucChannelS2;
				fgHasWideBandIE = TRUE;
				break;

			case ELEM_ID_CH_SW_ANNOUNCEMENT:
				if (IE_LEN(pucIE) != (sizeof(IE_CHANNEL_SWITCH_T) - 2)) {
					DBGLOG(RLM, INFO, "[Mgt Action] ELEM_ID_CH_SW_ANNOUNCEMENT, Length\n");
					break;
				}

				prChannelSwitchAnnounceIE = (P_IE_CHANNEL_SWITCH_T) pucIE;

				if (prChannelSwitchAnnounceIE->ucChannelSwitchMode == 1) {
					/* Need to stop data transmission immediately */
					if (!g_fgHasStopTx) {
						g_fgHasStopTx = TRUE;
#if CFG_SUPPORT_TDLS
						/* TDLS peers */
						TdlsTxCtrl(prAdapter, prBssInfo, FALSE);
#endif
						/* AP */
						qmSetStaRecTxAllowed(prAdapter, prStaRec, FALSE);
						DBGLOG(RLM, EVENT, "[Ch] TxAllowed = FALSE\n");
					}

					if (prChannelSwitchAnnounceIE->ucChannelSwitchCount <= 3) {
						DBGLOG(RLM, INFO,
						       "[Mgt Action] switch channel [%d]->[%d]\n",
							prBssInfo->ucPrimaryChannel,
							prChannelSwitchAnnounceIE->ucNewChannelNum);
						prBssInfo->ucPrimaryChannel =
							prChannelSwitchAnnounceIE->ucNewChannelNum;
						fgNeedSwitchChannel = TRUE;
					}
				} else {
					DBGLOG(RLM, INFO, "[Mgt Action] ucChannelSwitchMode = 0\n");
				}

				fgHasChannelSwitchIE = TRUE;
				break;
			case ELEM_ID_SCO:
				if (IE_LEN(pucIE) != (sizeof(IE_SECONDARY_OFFSET_T) - 2)) {
					DBGLOG(RLM, INFO, "[Mgt Action] ELEM_ID_SCO, Length\n");
					break;
				}
				prSecondaryOffsetIE = (P_IE_SECONDARY_OFFSET_T) pucIE;
				DBGLOG(RLM, INFO,
				       "[Mgt Action] SCO [%d]->[%d]\n", prBssInfo->eBssSCO,
				       prSecondaryOffsetIE->ucSecondaryOffset);
				prBssInfo->eBssSCO = prSecondaryOffsetIE->ucSecondaryOffset;
				fgHasSCOIE = TRUE;
				break;
			default:
				break;
			}	/*end of switch IE_ID */
		}		/*end of IE_FOR_EACH */
		if (fgHasChannelSwitchIE != FALSE) {
			if (fgHasWideBandIE == FALSE) {
				prBssInfo->ucVhtChannelWidth = 0;
				prBssInfo->ucVhtChannelFrequencyS1 = prBssInfo->ucPrimaryChannel;
				prBssInfo->ucVhtChannelFrequencyS2 = 0;
			}
			if (fgHasSCOIE == FALSE)
				prBssInfo->eBssSCO = CHNL_EXT_SCN;
			if (fgNeedSwitchChannel)
				kalIndicateChannelSwitch(
					prAdapter->prGlueInfo,
					prBssInfo->eBssSCO,
					prBssInfo->ucPrimaryChannel);
		}
		nicUpdateBss(prAdapter, prBssInfo->ucBssIndex);
		break;
	default:
		break;
	}
}

#endif


/*----------------------------------------------------------------------------*/
/*!
* \brief Send OpMode Norification frame (VHT action frame)
*
* \param[in] ucChannelWidth 0:20MHz, 1:40MHz, 2:80MHz, 3:160MHz or 80+80MHz
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID rlmSendOpModeNotificationFrame(P_ADAPTER_T prAdapter, P_STA_RECORD_T prStaRec, UINT_8 ucChannelWidth, UINT_8 ucNss)
{

	P_MSDU_INFO_T prMsduInfo;
	P_ACTION_OP_MODE_NOTIFICATION_FRAME prTxFrame;
	P_BSS_INFO_T prBssInfo;
	UINT_16 u2EstimatedFrameLen;
	/* PFN_TX_DONE_HANDLER pfTxDoneHandler = (PFN_TX_DONE_HANDLER) NULL; */

	/* Sanity Check */
	if (!prStaRec)
		return;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);
	if (!prBssInfo)
		return;

	/* Calculate MSDU buffer length */
	u2EstimatedFrameLen = MAC_TX_RESERVED_FIELD + sizeof(ACTION_OP_MODE_NOTIFICATION_FRAME);

	/* Alloc MSDU_INFO */
	prMsduInfo = (P_MSDU_INFO_T) cnmMgtPktAlloc(prAdapter, u2EstimatedFrameLen);

	if (!prMsduInfo)
		return;

	kalMemZero(prMsduInfo->prPacket, u2EstimatedFrameLen);

	prTxFrame = prMsduInfo->prPacket;

	/* Fill frame ctrl */
	prTxFrame->u2FrameCtrl = MAC_FRAME_ACTION;

	COPY_MAC_ADDR(prTxFrame->aucDestAddr, prStaRec->aucMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucSrcAddr, prBssInfo->aucOwnMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucBSSID, prBssInfo->aucBSSID);

	/* 3 Compose the frame body's frame */
	prTxFrame->ucCategory = CATEGORY_VHT_ACTION;
	prTxFrame->ucAction = ACTION_OPERATING_MODE_NOTIFICATION;

	prTxFrame->ucOperatingMode |= (ucChannelWidth & VHT_OP_MODE_CHANNEL_WIDTH);

	if (ucNss == 0)
		ucNss = 1;
	prTxFrame->ucOperatingMode |= (((ucNss - 1) << 4) & VHT_OP_MODE_RX_NSS);
	prTxFrame->ucOperatingMode &= ~VHT_OP_MODE_RX_NSS_TYPE;

	/* 4 Update information of MSDU_INFO_T */
	TX_SET_MMPDU(prAdapter,
		     prMsduInfo,
		     prBssInfo->ucBssIndex,
		     prStaRec->ucIndex,
		     WLAN_MAC_MGMT_HEADER_LEN, sizeof(ACTION_OP_MODE_NOTIFICATION_FRAME), NULL, MSDU_RATE_MODE_AUTO);

	/* 4 Enqueue the frame to send this action frame. */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);

}

/*----------------------------------------------------------------------------*/
/*!
* \brief Send SM Power Save frame (HT action frame)
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID rlmSendSmPowerSaveFrame(P_ADAPTER_T prAdapter, P_STA_RECORD_T prStaRec, UINT_8 ucNss)
{
	P_MSDU_INFO_T prMsduInfo;
	P_ACTION_SM_POWER_SAVE_FRAME prTxFrame;
	P_BSS_INFO_T prBssInfo;
	UINT_16 u2EstimatedFrameLen;
	/* PFN_TX_DONE_HANDLER pfTxDoneHandler = (PFN_TX_DONE_HANDLER) NULL; */

	/* Sanity Check */
	if (!prStaRec)
		return;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);
	if (!prBssInfo)
		return;

	/* Calculate MSDU buffer length */
	u2EstimatedFrameLen = MAC_TX_RESERVED_FIELD + sizeof(ACTION_SM_POWER_SAVE_FRAME);

	/* Alloc MSDU_INFO */
	prMsduInfo = (P_MSDU_INFO_T) cnmMgtPktAlloc(prAdapter, u2EstimatedFrameLen);

	if (!prMsduInfo)
		return;

	kalMemZero(prMsduInfo->prPacket, u2EstimatedFrameLen);

	prTxFrame = prMsduInfo->prPacket;

	/* Fill frame ctrl */
	prTxFrame->u2FrameCtrl = MAC_FRAME_ACTION;

	COPY_MAC_ADDR(prTxFrame->aucDestAddr, prStaRec->aucMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucSrcAddr, prBssInfo->aucOwnMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucBSSID, prBssInfo->aucBSSID);

	/* 3 Compose the frame body's frame */
	prTxFrame->ucCategory = CATEGORY_HT_ACTION;
	prTxFrame->ucAction = ACTION_HT_SM_POWER_SAVE;

	if (ucNss == 1)
		prTxFrame->ucSmPowerCtrl |= HT_SM_POWER_SAVE_CONTROL_ENABLED;
	else if (ucNss == 2)
		prTxFrame->ucSmPowerCtrl &= ~HT_SM_POWER_SAVE_CONTROL_ENABLED;
	else {
		DBGLOG(RLM, WARN, "Can't switch to Nss = %d since we don't support.\n", ucNss);
		return;
	}

	prTxFrame->ucSmPowerCtrl &= (~HT_SM_POWER_SAVE_CONTROL_SM_MODE); /* Static SM power save mode */

	/* 4 Update information of MSDU_INFO_T */
	TX_SET_MMPDU(prAdapter,
		     prMsduInfo,
		     prBssInfo->ucBssIndex,
		     prStaRec->ucIndex,
		     WLAN_MAC_MGMT_HEADER_LEN, sizeof(ACTION_SM_POWER_SAVE_FRAME), NULL, MSDU_RATE_MODE_AUTO);

	/* 4 Enqueue the frame to send this action frame. */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);

}


/*----------------------------------------------------------------------------*/
/*!
* \brief Send Notify Channel Width frame (HT action frame)
*
* \param[in] ucChannelWidth 0:20MHz, 1:Any channel width in the STA¡¦s Supported Channel Width Set subfield
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID rlmSendNotifyChannelWidthFrame(P_ADAPTER_T prAdapter, P_STA_RECORD_T prStaRec, UINT_8 ucChannelWidth)
{
	P_MSDU_INFO_T prMsduInfo;
	P_ACTION_NOTIFY_CHANNEL_WIDTH_FRAME prTxFrame;
	P_BSS_INFO_T prBssInfo;
	UINT_16 u2EstimatedFrameLen;
	/* PFN_TX_DONE_HANDLER pfTxDoneHandler = (PFN_TX_DONE_HANDLER) NULL; */

	/* Sanity Check */
	if (!prStaRec)
		return;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);
	if (!prBssInfo)
		return;

	/* Calculate MSDU buffer length */
	u2EstimatedFrameLen = MAC_TX_RESERVED_FIELD + sizeof(ACTION_NOTIFY_CHANNEL_WIDTH_FRAME);

	/* Alloc MSDU_INFO */
	prMsduInfo = (P_MSDU_INFO_T) cnmMgtPktAlloc(prAdapter, u2EstimatedFrameLen);

	if (!prMsduInfo)
		return;

	kalMemZero(prMsduInfo->prPacket, u2EstimatedFrameLen);

	prTxFrame = prMsduInfo->prPacket;

	/* Fill frame ctrl */
	prTxFrame->u2FrameCtrl = MAC_FRAME_ACTION;

	COPY_MAC_ADDR(prTxFrame->aucDestAddr, prStaRec->aucMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucSrcAddr, prBssInfo->aucOwnMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucBSSID, prBssInfo->aucBSSID);

	/* 3 Compose the frame body's frame */
	prTxFrame->ucCategory = CATEGORY_HT_ACTION;
	prTxFrame->ucAction = ACTION_HT_NOTIFY_CHANNEL_WIDTH;

	prTxFrame->ucChannelWidth = ucChannelWidth;

	/* 4 Update information of MSDU_INFO_T */
	TX_SET_MMPDU(prAdapter,
		     prMsduInfo,
		     prBssInfo->ucBssIndex,
		     prStaRec->ucIndex,
		     WLAN_MAC_MGMT_HEADER_LEN, sizeof(ACTION_NOTIFY_CHANNEL_WIDTH_FRAME), NULL, MSDU_RATE_MODE_AUTO);

	/* 4 Enqueue the frame to send this action frame. */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);

}

/*----------------------------------------------------------------------------*/
/*!
* \brief Change OpMode Nss/Channel Width
*
* \param[in] ucChannelWidth 0:20MHz, 1:40MHz, 2:80MHz, 3:160MHz 4:80+80MHz
*
* \return none
*/
/*----------------------------------------------------------------------------*/
BOOLEAN rlmChangeOperationMode(P_ADAPTER_T prAdapter, UINT_8 ucBssIndex, UINT_8 ucChannelWidth, UINT_8 ucNss)
{
	P_BSS_INFO_T prBssInfo;
	P_STA_RECORD_T prStaRec = (P_STA_RECORD_T) NULL;
	/*BOOLEAN fgIsSuccess = FALSE;*/
	BOOLEAN fgIsChangeVhtBw = TRUE, fgIsChangeHtBw = TRUE;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

	/* No need to change BSS 4 rlm parameter */
	if (ucBssIndex >= HW_BSSID_NUM)
		return FALSE;

	if (!prBssInfo)
		return FALSE;

	DBGLOG(RLM, INFO, "Intend to change BSS[%d] OP Mode to BW[%d] Nss[%d]\n", ucBssIndex, ucChannelWidth, ucNss);

#if CFG_SUPPORT_802_11AC
	/* Check peer VHT/HT OP Channel Width */
	if (ucChannelWidth == prBssInfo->ucOpChangeChannelWidth)
		fgIsChangeVhtBw = FALSE;
	else if (prBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE) {
		switch (ucChannelWidth) {
		case MAX_BW_80_80_MHZ:
			if (prBssInfo->ucVhtPeerChannelWidth != VHT_OP_CHANNEL_WIDTH_80P80) {
				DBGLOG(RLM, INFO,
				"Can't change to BW80_80 due to peer VHT OP BW is BW[%d]\n"
				, prBssInfo->ucVhtPeerChannelWidth);
				fgIsChangeVhtBw = FALSE;
			}
			break;
		case MAX_BW_160MHZ:
			if (prBssInfo->ucVhtPeerChannelWidth != VHT_OP_CHANNEL_WIDTH_160) {
				DBGLOG(RLM, INFO,
					"Can't change to BW160 due to peer VHT OP BW is BW[%d]\n",
					prBssInfo->ucVhtPeerChannelWidth);
				fgIsChangeVhtBw = FALSE;
			}
			break;
		case MAX_BW_80MHZ:
			if (prBssInfo->ucVhtPeerChannelWidth < VHT_OP_CHANNEL_WIDTH_80) {
				DBGLOG(RLM, INFO,
					"Can't change to BW80 due to peer VHT OP BW is BW[%d]\n",
					prBssInfo->ucVhtPeerChannelWidth);
				fgIsChangeVhtBw = FALSE;
			}
			break;
		case MAX_BW_40MHZ:
			if (!(prBssInfo->ucHtPeerOpInfo1 & HT_OP_INFO1_STA_CHNL_WIDTH) ||
				(!prBssInfo->fg40mBwAllowed)) {
				DBGLOG(RLM, INFO,
				"Can't change to BW40: PeerOpBw[%d] fg40mBwAllowed[%d]\n",
				(UINT_8)(prBssInfo->ucHtPeerOpInfo1 &
					HT_OP_INFO1_STA_CHNL_WIDTH),
				prBssInfo->fg40mBwAllowed);
				fgIsChangeVhtBw = FALSE;
			}
			break;
		case MAX_BW_20MHZ:
			break;
		default:
			DBGLOG(RLM, WARN, "BW[%d] is invalid for OpMode change\n", ucChannelWidth);
			fgIsChangeVhtBw = FALSE;
		}
	}
#endif

	/* Check HT OP Channel Width */
	if (ucChannelWidth == prBssInfo->ucOpChangeChannelWidth)
		fgIsChangeHtBw = FALSE;
	else if (ucChannelWidth >= MAX_BW_80MHZ) {
		DBGLOG(RLM, WARN, "BW[%d] is invalid for HT OpMode change\n", ucChannelWidth);
		fgIsChangeHtBw = FALSE;
	} else if (ucChannelWidth == MAX_BW_40MHZ) {
		if (!(prBssInfo->ucHtPeerOpInfo1 & HT_OP_INFO1_STA_CHNL_WIDTH) ||
			(!prBssInfo->fg40mBwAllowed)) {
			DBGLOG(RLM, INFO,
				"Can't change to BW40: PeerOpBw[%d] fg40mBwAllowed[%d]\n",
				(UINT_8)(prBssInfo->ucHtPeerOpInfo1 &
					HT_OP_INFO1_STA_CHNL_WIDTH),
				prBssInfo->fg40mBwAllowed);
			fgIsChangeHtBw = FALSE;
		}
	}

	if (fgIsChangeHtBw) {
		/* <4>Update HT Channel Width */
		if (ucChannelWidth == MAX_BW_20MHZ) {
			prBssInfo->ucHtOpInfo1 &= ~HT_OP_INFO1_STA_CHNL_WIDTH;
			prBssInfo->eBssSCO = CHNL_EXT_SCN;
		} else if (ucChannelWidth == MAX_BW_40MHZ) {
			prBssInfo->ucHtOpInfo1 |= HT_OP_INFO1_STA_CHNL_WIDTH;
			if ((prBssInfo->ucHtPeerOpInfo1 & HT_OP_INFO1_SCO) != CHNL_EXT_RES)
				prBssInfo->eBssSCO =
				(ENUM_CHNL_EXT_T) (prBssInfo->ucHtPeerOpInfo1 & HT_OP_INFO1_SCO);
		} else
			fgIsChangeHtBw = FALSE;
	}

#if CFG_SUPPORT_802_11AC
	if (fgIsChangeVhtBw) {
		prBssInfo->ucOpChangeChannelWidth = ucChannelWidth;
		prBssInfo->fgIsOpChangeChannelWidth = TRUE;
		/* <3>Update VHT Channel Width*/
		rlmChangeVhtOpBwPara(prAdapter, ucBssIndex, prBssInfo->ucOpChangeChannelWidth);

		DBGLOG(RLM, INFO, "Update VHT Channel Width Info to w=%d s1=%d s2=%d\n",
				prBssInfo->ucVhtChannelWidth,
				prBssInfo->ucVhtChannelFrequencyS1,
				prBssInfo->ucVhtChannelFrequencyS2);
	}
#endif
	if (fgIsChangeHtBw) {
		prBssInfo->ucOpChangeChannelWidth = ucChannelWidth;
		prBssInfo->fgIsOpChangeChannelWidth = TRUE;

		DBGLOG(RLM, INFO, "Update HT Channel Width Info to bw=%d s=%d\n",
			(UINT_8)(prBssInfo->ucHtOpInfo1 &
				HT_OP_INFO1_STA_CHNL_WIDTH) >> 2,
			prBssInfo->eBssSCO);
	}

	if ((prBssInfo->ucNss != ucNss) || fgIsChangeVhtBw || fgIsChangeHtBw) {
		/* 1. Update BSS Info */
		prBssInfo->ucNss = ucNss;
		rlmSyncOperationParams(prAdapter, prBssInfo);

		if (prBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE) { /* For infrastructure, GC */
			if (prBssInfo->prStaRecOfAP) {
				prStaRec = prBssInfo->prStaRecOfAP;
#if CFG_SUPPORT_802_11AC
				/* 2. Check if we can change OpMode and Send OPmode notification frame */
				if (prStaRec->ucDesiredPhyTypeSet & PHY_TYPE_BIT_VHT) { /*Send VHT notification frame*/
					/* <1> Notify VHT Nss and Channel Width change*/
					rlmSendOpModeNotificationFrame(prAdapter, prStaRec, ucChannelWidth,
						prBssInfo->ucNss);
					DBGLOG(RLM, INFO, "Send VHT OPmode notification frame, BW=%d, Nss=%d\n"
						, ucChannelWidth, prBssInfo->ucNss);
				} else
#endif
				if (prStaRec->ucDesiredPhyTypeSet & PHY_TYPE_BIT_HT) { /*Send HT notification frame*/
					/* <1> Notify HT Nss change */
					rlmSendSmPowerSaveFrame(prAdapter, prStaRec, prBssInfo->ucNss);
					DBGLOG(RLM, INFO, "Send HT SM Power Save frame, Nss=%d\n",
						prBssInfo->ucNss);
				}

				if (fgIsChangeHtBw) {
					/* <3> Notify HT Channel Width change */
					rlmSendNotifyChannelWidthFrame(prAdapter, prStaRec, ucChannelWidth);
					DBGLOG(RLM, INFO, "Send HT Notify Channel Width frame, BW=%d\n",
						ucChannelWidth);
				}
			} else
				DBGLOG(RLM, WARN, "Can't change OpMode at legacy mode\n");
		} else if (prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT) {
			P_LINK_T prClientList;

			/* 4. Update BCN/Probe Resp IE to notify peers the OP is be changed */
			DBGLOG(RLM, INFO, "Beacon content update with Bssidex(%d)\n",
				prBssInfo->ucBssIndex);

			prClientList = &prBssInfo->rStaRecOfClientList;

			LINK_FOR_EACH_ENTRY(prStaRec, prClientList, rLinkEntry, STA_RECORD_T) {
#if CFG_SUPPORT_802_11AC
				/* 2. Check if we can change OpMode and Send OPmode notification frame */
				if (prStaRec->ucDesiredPhyTypeSet & PHY_TYPE_BIT_VHT) { /*Send VHT notification frame*/
					/* <1> Notify VHT Nss and Channel Width change*/
					rlmSendOpModeNotificationFrame(prAdapter, prStaRec, ucChannelWidth,
						prBssInfo->ucNss);
					DBGLOG(RLM, INFO, "Send VHT OPmode notification frame, BW=%d, Nss=%d\n"
						, ucChannelWidth, prBssInfo->ucNss);
				} else
#endif
				if (prStaRec->ucDesiredPhyTypeSet & PHY_TYPE_BIT_HT) { /*Send HT notification frame*/
					/* <1> Notify HT Nss change */
					rlmSendSmPowerSaveFrame(prAdapter, prStaRec, prBssInfo->ucNss);
					DBGLOG(RLM, INFO, "Send HT SM Power Save frame, Nss=%d\n",
						prBssInfo->ucNss);
				}

				if (fgIsChangeHtBw) {
					/* <3> Notify HT Channel Width change */
					rlmSendNotifyChannelWidthFrame(prAdapter, prStaRec, ucChannelWidth);
					DBGLOG(RLM, INFO, "Send HT Notify Channel Width frame, BW=%d\n",
						ucChannelWidth);
				}
			}

			bssUpdateBeaconContent(prAdapter, prBssInfo->ucBssIndex);
		} else
			return FALSE;
	}
	return TRUE;
}

#if CFG_SUPPORT_BFER
/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID rlmBfStaRecPfmuUpdate(P_ADAPTER_T prAdapter, P_STA_RECORD_T prStaRec)
{
	UINT_8 ucBFerMaxNr, ucBFeeMaxNr, ucMode;
	P_BSS_INFO_T prBssInfo;
	P_CMD_STAREC_BF prStaRecBF;
	P_CMD_STAREC_UPDATE_T prStaRecUpdateInfo;
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4SetBufferLen = sizeof(CMD_STAREC_BF);

	prBssInfo = prAdapter->aprBssInfo[prStaRec->ucBssIndex];

	if (RLM_NET_IS_11AC(prBssInfo) &&
	    IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucStaVhtBfer))
		ucMode = MODE_VHT;
	else if (RLM_NET_IS_11N(prBssInfo) &&
		 IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucStaHtBfer))
		ucMode = MODE_HT;
	else
		ucMode = MODE_LEGACY;

	prStaRecBF =
	    (P_CMD_STAREC_BF) cnmMemAlloc(prAdapter,
		RAM_TYPE_MSG, u4SetBufferLen);

	if (!prStaRecBF) {
		DBGLOG(RLM, ERROR, "STA Rec memory alloc fail\n");
		return;
	}

	prStaRecUpdateInfo =
	    (P_CMD_STAREC_UPDATE_T) cnmMemAlloc(prAdapter,
		RAM_TYPE_MSG, (CMD_STAREC_UPDATE_HDR_SIZE + u4SetBufferLen));

	if (!prStaRecUpdateInfo) {
		cnmMemFree(prAdapter, prStaRecBF);
		DBGLOG(RLM, ERROR, "STA Rec Update Info memory alloc fail\n");
		return;
	}

	switch (ucMode) {
	case MODE_VHT:
		prStaRec->rTxBfPfmuStaInfo.fgSU_MU = FALSE;
		prStaRec->rTxBfPfmuStaInfo.fgETxBfCap =
				rlmClientSupportsVhtETxBF(prStaRec);

		if (prStaRec->rTxBfPfmuStaInfo.fgETxBfCap) {
			/* OFDM, NDPA/Report Poll/CTS2Self tx mode */
			prStaRec->rTxBfPfmuStaInfo.ucSoundingPhy =
							TX_RATE_MODE_OFDM;

			/* 9: OFDM 24M */
			prStaRec->rTxBfPfmuStaInfo.ucNdpaRate = PHY_RATE_24M;

			/* VHT mode, NDP tx mode */
			prStaRec->rTxBfPfmuStaInfo.ucTxMode = TX_RATE_MODE_VHT;

			/* 0: MCS0 */
			prStaRec->rTxBfPfmuStaInfo.ucNdpRate = PHY_RATE_MCS0;


			switch (prBssInfo->ucVhtChannelWidth) {
			case VHT_OP_CHANNEL_WIDTH_80:
				prStaRec->rTxBfPfmuStaInfo.ucCBW =
							MAX_BW_80MHZ;
				break;

			case VHT_OP_CHANNEL_WIDTH_20_40:
			default:
				prStaRec->rTxBfPfmuStaInfo.ucCBW =
							MAX_BW_20MHZ;
				if (prBssInfo->eBssSCO != CHNL_EXT_SCN)
					prStaRec->rTxBfPfmuStaInfo.ucCBW =
								MAX_BW_40MHZ;
				break;
			}

			ucBFerMaxNr = 1; /* 7668 is 2x2 */
			ucBFeeMaxNr = rlmClientSupportsVhtBfeeStsCap(prStaRec);
			prStaRec->rTxBfPfmuStaInfo.ucNr =
				(ucBFerMaxNr < ucBFeeMaxNr) ?
					ucBFerMaxNr : ucBFeeMaxNr;
			prStaRec->rTxBfPfmuStaInfo.ucNc =
				((prStaRec->u2VhtRxMcsMap &
					VHT_CAP_INFO_MCS_2SS_MASK)
						!= BITS(2, 3)) ? 1 : 0;
		}
		break;

	case MODE_HT:
		prStaRec->rTxBfPfmuStaInfo.fgSU_MU = FALSE;
		prStaRec->rTxBfPfmuStaInfo.fgETxBfCap =
				rlmClientSupportsHtETxBF(prStaRec);

		if (prStaRec->rTxBfPfmuStaInfo.fgETxBfCap) {
			/* 0: HT MCS0 */
			prStaRec->rTxBfPfmuStaInfo.ucNdpaRate = PHY_RATE_MCS0;

			/* HT mode, NDPA/NDP tx mode */
			prStaRec->rTxBfPfmuStaInfo.ucTxMode =
						TX_RATE_MODE_HTMIX;

			prStaRec->rTxBfPfmuStaInfo.ucCBW = MAX_BW_20MHZ;
			if (prBssInfo->eBssSCO != CHNL_EXT_SCN)
				prStaRec->rTxBfPfmuStaInfo.ucCBW = MAX_BW_40MHZ;

			ucBFerMaxNr = 1; /* 7668 is 2x2 */
			ucBFeeMaxNr =
				(prStaRec->u4TxBeamformingCap &
				TXBF_COMPRESSED_TX_ANTENNANUM_SUPPORTED) >>
				TXBF_COMPRESSED_TX_ANTENNANUM_SUPPORTED_OFFSET;
			prStaRec->rTxBfPfmuStaInfo.ucNr =
				(ucBFerMaxNr < ucBFeeMaxNr) ?
					ucBFerMaxNr : ucBFeeMaxNr;
			prStaRec->rTxBfPfmuStaInfo.ucNc =
				(prStaRec->aucRxMcsBitmask[1] > 0) ? 1 : 0;
			prStaRec->rTxBfPfmuStaInfo.ucNdpRate =
				prStaRec->rTxBfPfmuStaInfo.ucNr * 8;
		}
		break;
	default:
		break;
	}

	DBGLOG(RLM, INFO, "ucMode=%d\n", ucMode);
	DBGLOG(RLM, INFO, "rlmClientSupportsVhtETxBF(prStaRec)=%d\n",
				rlmClientSupportsVhtETxBF(prStaRec));
	DBGLOG(RLM, INFO, "rlmClientSupportsVhtBfeeStsCap(prStaRec)=%d\n",
				rlmClientSupportsVhtBfeeStsCap(prStaRec));
	DBGLOG(RLM, INFO, "prStaRec->u2VhtRxMcsMap=%x\n",
				prStaRec->u2VhtRxMcsMap);

	DBGLOG(RLM, INFO,
	    "====================== BF StaRec Info =====================\n");
	DBGLOG(RLM, INFO, "u2PfmuId       =%d\n",
				prStaRec->rTxBfPfmuStaInfo.u2PfmuId);
	DBGLOG(RLM, INFO, "fgSU_MU        =%d\n",
				prStaRec->rTxBfPfmuStaInfo.fgSU_MU);
	DBGLOG(RLM, INFO, "fgETxBfCap     =%d\n",
				prStaRec->rTxBfPfmuStaInfo.fgETxBfCap);
	DBGLOG(RLM, INFO, "ucSoundingPhy  =%d\n",
				prStaRec->rTxBfPfmuStaInfo.ucSoundingPhy);
	DBGLOG(RLM, INFO, "ucNdpaRate     =%d\n",
				prStaRec->rTxBfPfmuStaInfo.ucNdpaRate);
	DBGLOG(RLM, INFO, "ucNdpRate      =%d\n",
				prStaRec->rTxBfPfmuStaInfo.ucNdpRate);
	DBGLOG(RLM, INFO, "ucReptPollRate =%d\n",
				prStaRec->rTxBfPfmuStaInfo.ucReptPollRate);
	DBGLOG(RLM, INFO, "ucTxMode       =%d\n",
				prStaRec->rTxBfPfmuStaInfo.ucTxMode);
	DBGLOG(RLM, INFO, "ucNc           =%d\n",
				prStaRec->rTxBfPfmuStaInfo.ucNc);
	DBGLOG(RLM, INFO, "ucNr           =%d\n",
				prStaRec->rTxBfPfmuStaInfo.ucNr);
	DBGLOG(RLM, INFO, "ucCBW          =%d\n",
				prStaRec->rTxBfPfmuStaInfo.ucCBW);
	DBGLOG(RLM, INFO, "ucTotMemRequire=%d\n",
				prStaRec->rTxBfPfmuStaInfo.ucTotMemRequire);
	DBGLOG(RLM, INFO, "ucMemRequire20M=%d\n",
				prStaRec->rTxBfPfmuStaInfo.ucMemRequire20M);
	DBGLOG(RLM, INFO, "ucMemRow0      =%d\n",
				prStaRec->rTxBfPfmuStaInfo.ucMemRow0);
	DBGLOG(RLM, INFO, "ucMemCol0      =%d\n",
				prStaRec->rTxBfPfmuStaInfo.ucMemCol0);
	DBGLOG(RLM, INFO, "ucMemRow1      =%d\n",
				prStaRec->rTxBfPfmuStaInfo.ucMemRow1);
	DBGLOG(RLM, INFO, "ucMemCol1      =%d\n",
				prStaRec->rTxBfPfmuStaInfo.ucMemCol1);
	DBGLOG(RLM, INFO, "ucMemRow2      =%d\n",
				prStaRec->rTxBfPfmuStaInfo.ucMemRow2);
	DBGLOG(RLM, INFO, "ucMemCol2      =%d\n",
				prStaRec->rTxBfPfmuStaInfo.ucMemCol2);
	DBGLOG(RLM, INFO, "ucMemRow3      =%d\n",
				prStaRec->rTxBfPfmuStaInfo.ucMemRow3);
	DBGLOG(RLM, INFO, "ucMemCol3      =%d\n",
				prStaRec->rTxBfPfmuStaInfo.ucMemCol3);
	DBGLOG(RLM, INFO,
	    "===========================================================\n");



	prStaRecBF->u2Tag = STA_REC_BF;
	prStaRecBF->u2Length = u4SetBufferLen;
	kalMemCopy(&prStaRecBF->rTxBfPfmuInfo,
		&prStaRec->rTxBfPfmuStaInfo, sizeof(TXBF_PFMU_STA_INFO));


	prStaRecUpdateInfo->ucBssIndex = prStaRec->ucBssIndex;
	prStaRecUpdateInfo->ucWlanIdx = prStaRec->ucWlanIndex;
	prStaRecUpdateInfo->u2TotalElementNum = 1;
	kalMemCopy(prStaRecUpdateInfo->aucBuffer, prStaRecBF, u4SetBufferLen);


	rWlanStatus = wlanSendSetQueryExtCmd(prAdapter,
			     CMD_ID_LAYER_0_EXT_MAGIC_NUM,
			     EXT_CMD_ID_STAREC_UPDATE,
			     TRUE,
			     FALSE,
			     FALSE,
			     nicCmdEventSetCommon,
			     nicOidCmdTimeoutCommon,
			     (CMD_STAREC_UPDATE_HDR_SIZE + u4SetBufferLen),
			     (PUINT_8) prStaRecUpdateInfo, NULL, 0);

	if (rWlanStatus == WLAN_STATUS_FAILURE)
		DBGLOG(RLM, ERROR, "Send starec update cmd fail\n");

	cnmMemFree(prAdapter, prStaRecBF);
	cnmMemFree(prAdapter, prStaRecUpdateInfo);

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
VOID rlmETxBfTriggerPeriodicSounding(P_ADAPTER_T prAdapter)
{
	UINT_32 u4SetBufferLen = sizeof(PARAM_CUSTOM_TXBF_ACTION_STRUCT_T);
	PARAM_CUSTOM_TXBF_ACTION_STRUCT_T rTxBfActionInfo;
	CMD_TXBF_ACTION_T rCmdTxBfActionInfo;
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;

	DBGLOG(RLM, INFO, "rlmETxBfTriggerPeriodicSounding\n");

	rTxBfActionInfo.rTxBfSoundingStart.rTxBfSounding.
			rExtCmdExtBfSndPeriodicTriggerCtrl.ucCmdCategoryID =
								BF_SOUNDING_ON;

	rTxBfActionInfo.rTxBfSoundingStart.rTxBfSounding.
			rExtCmdExtBfSndPeriodicTriggerCtrl.ucSuMuSndMode =
						    AUTO_SU_PERIODIC_SOUNDING;

	kalMemCopy(&rCmdTxBfActionInfo, &rTxBfActionInfo,
					sizeof(CMD_TXBF_ACTION_T));

	rWlanStatus = wlanSendSetQueryExtCmd(prAdapter,
					     CMD_ID_LAYER_0_EXT_MAGIC_NUM,
					     EXT_CMD_ID_BF_ACTION,
					     TRUE,
					     FALSE,
					     FALSE,
					     nicCmdEventSetCommon,
					     nicOidCmdTimeoutCommon,
					     sizeof(CMD_TXBF_ACTION_T),
					     (PUINT_8) & rCmdTxBfActionInfo,
					     &rTxBfActionInfo, u4SetBufferLen);

	if (rWlanStatus == WLAN_STATUS_FAILURE)
		DBGLOG(RLM, ERROR, "Send BF sounding cmd fail\n");
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
BOOLEAN
rlmClientSupportsVhtETxBF(P_STA_RECORD_T prStaRec)
{
	UINT_8 ucVhtCapSuBfeeCap;

	ucVhtCapSuBfeeCap =
		(prStaRec->u4VhtCapInfo & VHT_CAP_INFO_SU_BEAMFORMEE_CAPABLE)
		>> VHT_CAP_INFO_SU_BEAMFORMEE_CAPABLE_OFFSET;

	return (ucVhtCapSuBfeeCap) ? TRUE : FALSE;
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
UINT_8
rlmClientSupportsVhtBfeeStsCap(P_STA_RECORD_T prStaRec)
{
	UINT_8 ucVhtCapBfeeStsCap;

	ucVhtCapBfeeStsCap =
	    (prStaRec->u4VhtCapInfo &
	    VHT_CAP_INFO_COMP_STEERING_NUM_OF_BFER_ANT_SUP) >>
	    VHT_CAP_INFO_COMP_STEERING_NUM_OF_BFER_ANT_SUP_OFFSET;

	return ucVhtCapBfeeStsCap;
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
BOOLEAN
rlmClientSupportsHtETxBF(P_STA_RECORD_T prStaRec)
{
	UINT_32 u4RxNDPCap, u4ComBfFbkCap;

	u4RxNDPCap = (prStaRec->u4TxBeamformingCap & TXBF_RX_NDP_CAPABLE)
						>> TXBF_RX_NDP_CAPABLE_OFFSET;
	/* Support compress feedback */
	u4ComBfFbkCap = (prStaRec->u4TxBeamformingCap &
			TXBF_EXPLICIT_COMPRESSED_FEEDBACK_IMMEDIATE_CAPABLE)
			>> TXBF_EXPLICIT_COMPRESSED_FEEDBACK_CAPABLE_OFFSET;

	return (u4RxNDPCap == 1) && (u4ComBfFbkCap > 0);
}

#endif
