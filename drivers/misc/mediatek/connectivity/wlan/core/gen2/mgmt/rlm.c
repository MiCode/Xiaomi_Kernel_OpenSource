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

#define FIX_P2P_HT20	0

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
TIMER_T rBeaconReqTimer;
TIMER_T rTSMReqTimer;
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

static BOOLEAN rlmAllMeasurementIssued(struct RADIO_MEASUREMENT_REQ_PARAMS *prReq);

static VOID rlmFreeMeasurementResources(P_ADAPTER_T prAdapter);

static VOID rlmCalibrateRepetions(struct RADIO_MEASUREMENT_REQ_PARAMS *prRmReq);

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
	{
		struct RADIO_MEASUREMENT_REQ_PARAMS *prRmReqParam =
			&prAdapter->rWifiVar.rRmReqParams;
		struct RADIO_MEASUREMENT_REPORT_PARAMS *prRmRepParam =
			&prAdapter->rWifiVar.rRmRepParams;

		kalMemZero(prRmRepParam, sizeof(*prRmRepParam));
		kalMemZero(prRmReqParam, sizeof(*prRmReqParam));
		prRmReqParam->rBcnRmParam.eState = RM_NO_REQUEST;
		prRmReqParam->fgRmIsOngoing = FALSE;
		LINK_INITIALIZE(&prRmRepParam->rFreeReportLink);
		LINK_INITIALIZE(&prRmRepParam->rReportLink);
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
VOID rlmFsmEventUninit(P_ADAPTER_T prAdapter)
{
	P_BSS_INFO_T prBssInfo;
	UINT_8 ucNetIdx;

	ASSERT(prAdapter);

	RLM_NET_FOR_EACH(ucNetIdx) {
		prBssInfo = &prAdapter->rWifiVar.arBssInfo[ucNetIdx];
		ASSERT(prBssInfo);

		/* Note: all RLM timers will also be stopped.
		 *       Now only one OBSS scan timer.
		 */
		rlmBssReset(prAdapter, prBssInfo);
	}
	rlmCancelRadioMeasurement(prAdapter);
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

	prBssInfo = &prAdapter->rWifiVar.arBssInfo[prMsduInfo->ucNetworkType];
	ASSERT(prBssInfo);

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

	prBssInfo = &prAdapter->rWifiVar.arBssInfo[prMsduInfo->ucNetworkType];
	ASSERT(prBssInfo);

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

	if ((prAdapter->rWifiVar.ucAvailablePhyTypeSet & PHY_TYPE_SET_802_11N) &&
	    (!prStaRec || (prStaRec->ucPhyTypeSet & PHY_TYPE_SET_802_11N)))
		rlmFillExtCapIE(prAdapter, prBssInfo, prMsduInfo);
#if CFG_SUPPORT_HOTSPOT_2_0
	else if (prAdapter->prGlueInfo->fgConnectHS20AP == TRUE)
		hs20FillExtCapIE(prAdapter, prBssInfo, prMsduInfo);
#endif
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

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);
	ASSERT(IS_NET_ACTIVE(prAdapter, prMsduInfo->ucNetworkType));

	prBssInfo = &prAdapter->rWifiVar.arBssInfo[prMsduInfo->ucNetworkType];
	ASSERT(prBssInfo);

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

	if (prStaRec)
		DBGLOG(RLM, TRACE, "%s index %d ,availPhyTypeSet:0x%x prStaRec->ucPhyTypeSet:0x%x [%pM]\n ", __func__
		, prMsduInfo->ucStaRecIndex, prAdapter->rWifiVar.ucAvailablePhyTypeSet
		, prStaRec->ucPhyTypeSet, prStaRec->aucMacAddr);
	else
		DBGLOG(RLM, TRACE, "%s prStaRec is null ,availPhyTypeSet:0x%x, index :%d\n", __func__
		, prAdapter->rWifiVar.ucAvailablePhyTypeSet, prMsduInfo->ucStaRecIndex);

	if (prAdapter->rWifiVar.ucAvailablePhyTypeSet & PHY_TYPE_SET_802_11N) {
		if (prBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE ||
			prBssInfo->eCurrentOPMode == OP_MODE_P2P_DEVICE) {
			if (prStaRec && (prStaRec->ucPhyTypeSet & PHY_TYPE_SET_802_11N))
				rlmFillHtCapIE(prAdapter, prBssInfo, prMsduInfo);
		} else if (prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT && RLM_NET_IS_11N(prBssInfo))
			rlmFillHtCapIE(prAdapter, prBssInfo, prMsduInfo);
	}
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

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);
	ASSERT(IS_NET_ACTIVE(prAdapter, prMsduInfo->ucNetworkType));

	prBssInfo = &prAdapter->rWifiVar.arBssInfo[prMsduInfo->ucNetworkType];
	ASSERT(prBssInfo);

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

	if (prStaRec)
		DBGLOG(RLM, TRACE, "%s index %d ,availPhyTypeSet:0x%x prStaRec->ucPhyTypeSet:0x%x [%pM]\n ", __func__
		, prMsduInfo->ucStaRecIndex, prAdapter->rWifiVar.ucAvailablePhyTypeSet
		, prStaRec->ucPhyTypeSet, prStaRec->aucMacAddr);
	else
		DBGLOG(RLM, TRACE, "%s prStaRec is null ,availPhyTypeSet:0x%x, index :%d\n", __func__
		, prAdapter->rWifiVar.ucAvailablePhyTypeSet, prMsduInfo->ucStaRecIndex);

	if (prAdapter->rWifiVar.ucAvailablePhyTypeSet & PHY_TYPE_SET_802_11N) {
		if (prBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE ||
			prBssInfo->eCurrentOPMode == OP_MODE_P2P_DEVICE) {
			if (prStaRec && (prStaRec->ucPhyTypeSet & PHY_TYPE_SET_802_11N))
				rlmFillExtCapIE(prAdapter, prBssInfo, prMsduInfo);
		} else if (prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT && RLM_NET_IS_11N(prBssInfo))
			rlmFillExtCapIE(prAdapter, prBssInfo, prMsduInfo);
	}
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

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);
	ASSERT(IS_NET_ACTIVE(prAdapter, prMsduInfo->ucNetworkType));

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

	prBssInfo = &prAdapter->rWifiVar.arBssInfo[prMsduInfo->ucNetworkType];
	ASSERT(prBssInfo);

	if (prStaRec)
		DBGLOG(RLM, TRACE, "%s index %d ,availPhyTypeSet:0x%x prStaRec->ucPhyTypeSet:0x%x [%pM]\n ", __func__
		, prMsduInfo->ucStaRecIndex, prAdapter->rWifiVar.ucAvailablePhyTypeSet
		, prStaRec->ucPhyTypeSet, prStaRec->aucMacAddr);
	else
		DBGLOG(RLM, TRACE, "%s prStaRec is null ,availPhyTypeSet:0x%x, index :%d\n", __func__
		, prAdapter->rWifiVar.ucAvailablePhyTypeSet, prMsduInfo->ucStaRecIndex);

	if (prAdapter->rWifiVar.ucAvailablePhyTypeSet & PHY_TYPE_SET_802_11N) {
		if (prBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE ||
			prBssInfo->eCurrentOPMode == OP_MODE_P2P_DEVICE) {
			if (prStaRec && (prStaRec->ucPhyTypeSet & PHY_TYPE_SET_802_11N))
				rlmFillHtOpIE(prAdapter, prBssInfo, prMsduInfo);
		} else if (prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT && RLM_NET_IS_11N(prBssInfo))
			rlmFillHtOpIE(prAdapter, prBssInfo, prMsduInfo);
	}
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

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);
	ASSERT(IS_NET_ACTIVE(prAdapter, prMsduInfo->ucNetworkType));

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

	prBssInfo = &prAdapter->rWifiVar.arBssInfo[prMsduInfo->ucNetworkType];
	ASSERT(prBssInfo);

	if (RLM_NET_IS_11GN(prBssInfo) && prBssInfo->eBand == BAND_2G4 &&
	    (!prStaRec || (prStaRec->ucPhyTypeSet & PHY_TYPE_SET_802_11GN))) {
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
	PUINT_8 pucBuffer;
	UINT_8 aucMtkOui[] = VENDOR_OUI_MTK;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

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
UINT32
rlmFillHtCapIEByParams(BOOLEAN fg40mAllowed,
		       BOOLEAN fgShortGIDisabled,
		       UINT_8 u8SupportRxSgi20,
		       UINT_8 u8SupportRxSgi40,
		       UINT_8 u8SupportRxGf, UINT_8 u8SupportRxSTBC, ENUM_OP_MODE_T eCurrentOPMode, UINT_8 *pOutBuf)
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
	if (u8SupportRxSTBC == 2)
		prHtCap->u2HtCapInfo &= ~(HT_CAP_INFO_RX_STBC_1_SS);
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
static VOID rlmFillHtCapIE(P_ADAPTER_T prAdapter, P_BSS_INFO_T prBssInfo, P_MSDU_INFO_T prMsduInfo)
{
	P_IE_HT_CAP_T prHtCap;
/* P_SUP_MCS_SET_FIELD     prSupMcsSet; */
	BOOLEAN fg40mAllowed;

	ASSERT(prAdapter);
	ASSERT(prBssInfo);
	ASSERT(prMsduInfo);

	fg40mAllowed = prBssInfo->fgAssoc40mBwAllowed;

	prHtCap = (P_IE_HT_CAP_T)
	    (((PUINT_8) prMsduInfo->prPacket) + prMsduInfo->u2FrameLength);

#if 0
	/* Add HT capabilities IE */
	prHtCap->ucId = ELEM_ID_HT_CAP;
	prHtCap->ucLength = sizeof(IE_HT_CAP_T) - ELEM_HDR_LEN;

	prHtCap->u2HtCapInfo = HT_CAP_INFO_DEFAULT_VAL;
	if (!fg40mAllowed)
		prHtCap->u2HtCapInfo &= ~(HT_CAP_INFO_SUP_CHNL_WIDTH |
					  HT_CAP_INFO_SHORT_GI_40M | HT_CAP_INFO_DSSS_CCK_IN_40M);
	if (prAdapter->rWifiVar.rConnSettings.fgRxShortGIDisabled)
		prHtCap->u2HtCapInfo &= ~(HT_CAP_INFO_SHORT_GI_20M | HT_CAP_INFO_SHORT_GI_40M);

	if (prAdapter->rWifiVar.u8SupportRxSgi20 == 2)
		prHtCap->u2HtCapInfo &= ~(HT_CAP_INFO_SHORT_GI_20M);
	if (prAdapter->rWifiVar.u8SupportRxSgi40 == 2)
		prHtCap->u2HtCapInfo &= ~(HT_CAP_INFO_SHORT_GI_40M);
	if (prAdapter->rWifiVar.u8SupportRxGf == 2)
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
	if (!fg40mAllowed || prBssInfo->eCurrentOPMode != OP_MODE_INFRASTRUCTURE)
		prHtCap->u2HtExtendedCap &= ~(HT_EXT_CAP_PCO | HT_EXT_CAP_PCO_TRANS_TIME_NONE);

	prHtCap->u4TxBeamformingCap = TX_BEAMFORMING_CAP_DEFAULT_VAL;

	prHtCap->ucAselCap = ASEL_CAP_DEFAULT_VAL;

	ASSERT(IE_SIZE(prHtCap) <= (ELEM_HDR_LEN + ELEM_MAX_LEN_HT_CAP));

	prMsduInfo->u2FrameLength += IE_SIZE(prHtCap);
#else

	prMsduInfo->u2FrameLength += rlmFillHtCapIEByParams(fg40mAllowed,
							    prAdapter->rWifiVar.rConnSettings.fgRxShortGIDisabled,
							    prAdapter->rWifiVar.u8SupportRxSgi20,
							    prAdapter->rWifiVar.u8SupportRxSgi40,
							    prAdapter->rWifiVar.u8SupportRxGf,
							    prAdapter->rWifiVar.u8SupportRxSTBC,
							    prBssInfo->eCurrentOPMode, (UINT_8 *) prHtCap);
#endif
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
#if CFG_SUPPORT_HOTSPOT_2_0
	P_HS20_EXT_CAP_T prHsExtCap;
#else
	P_EXT_CAP_T prExtCap;
#endif
	BOOLEAN fg40mAllowed;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

	fg40mAllowed = prBssInfo->fgAssoc40mBwAllowed;

#if CFG_SUPPORT_HOTSPOT_2_0
	prHsExtCap = (P_HS20_EXT_CAP_T)
	    (((PUINT_8) prMsduInfo->prPacket) + prMsduInfo->u2FrameLength);
	prHsExtCap->ucId = ELEM_ID_EXTENDED_CAP;

	if (prAdapter->prGlueInfo->fgConnectHS20AP == TRUE)
		prHsExtCap->ucLength = ELEM_MAX_LEN_EXT_CAP;
	else
		prHsExtCap->ucLength = 3 - ELEM_HDR_LEN;

	kalMemZero(prHsExtCap->aucCapabilities, sizeof(prHsExtCap->aucCapabilities));

	prHsExtCap->aucCapabilities[0] = ELEM_EXT_CAP_DEFAULT_VAL;

	if (!fg40mAllowed)
		prHsExtCap->aucCapabilities[0] &= ~ELEM_EXT_CAP_20_40_COEXIST_SUPPORT;

	if (prBssInfo->eCurrentOPMode != OP_MODE_INFRASTRUCTURE)
		prHsExtCap->aucCapabilities[0] &= ~ELEM_EXT_CAP_PSMP_CAP;
#if CFG_SUPPORT_P2P_ECSA
	/* Only set Extended Channel Switch support bit when as GO */
	if (prBssInfo->ucNetTypeIndex == NETWORK_TYPE_P2P_INDEX &&
	    prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT &&
	    !prAdapter->rWifiVar.prP2pFsmInfo->fgIsApMode)
		prHsExtCap->aucCapabilities[0] |= ELEM_EXT_CAP_ECS_SUPPORT;
#endif
	if (prAdapter->prGlueInfo->fgConnectHS20AP == TRUE) {
		SET_EXT_CAP(prHsExtCap->aucCapabilities, ELEM_MAX_LEN_EXT_CAP, ELEM_EXT_CAP_INTERWORKING_BIT);
		SET_EXT_CAP(prHsExtCap->aucCapabilities, ELEM_MAX_LEN_EXT_CAP, ELEM_EXT_CAP_QOSMAPSET_BIT);

		/* For R2 WNM-Notification */
		SET_EXT_CAP(prHsExtCap->aucCapabilities, ELEM_MAX_LEN_EXT_CAP, ELEM_EXT_CAP_WNM_NOTIFICATION_BIT);
	}

#if CFG_SUPPORT_802_11V_BSS_TRANSITION_MGT
	SET_EXT_CAP(prHsExtCap->aucCapabilities, 3, ELEM_EXT_CAP_BSS_TRANSITION_BIT);
	prHsExtCap->ucLength = 3;
#endif

	ASSERT(IE_SIZE(prHsExtCap) <= (ELEM_HDR_LEN + ELEM_MAX_LEN_EXT_CAP));

	prMsduInfo->u2FrameLength += IE_SIZE(prHsExtCap);

#else
	/* Add Extended Capabilities IE */
	prExtCap = (P_EXT_CAP_T)
	    (((PUINT_8) prMsduInfo->prPacket) + prMsduInfo->u2FrameLength);

	prExtCap->ucId = ELEM_ID_EXTENDED_CAP;

	prExtCap->ucLength = 3 - ELEM_HDR_LEN;
	kalMemZero(prExtCap->aucCapabilities, sizeof(prExtCap->aucCapabilities));

	prExtCap->aucCapabilities[0] = ELEM_EXT_CAP_DEFAULT_VAL;

	if (!fg40mAllowed)
		prExtCap->aucCapabilities[0] &= ~ELEM_EXT_CAP_20_40_COEXIST_SUPPORT;

	if (prBssInfo->eCurrentOPMode != OP_MODE_INFRASTRUCTURE)
		prExtCap->aucCapabilities[0] &= ~ELEM_EXT_CAP_PSMP_CAP;

#if CFG_SUPPORT_802_11V_BSS_TRANSITION_MGT
	SET_EXT_CAP(prExtCap->aucCapabilities, 3, ELEM_EXT_CAP_BSS_TRANSITION_BIT);
	prExtCap->ucLength = 3;
#endif

#if CFG_SUPPORT_P2P_ECSA
	/* Only set Extended Channel Switch support bit when as GO */
	if (prBssInfo->ucNetTypeIndex == NETWORK_TYPE_P2P_INDEX &&
	    prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT &&
	    !prAdapter->rWifiVar.prP2pFsmInfo->fgIsApMode)
		prExtCap->aucCapabilities[0] |= ELEM_EXT_CAP_ECS_SUPPORT;
#endif

	ASSERT(IE_SIZE(prExtCap) <= (ELEM_HDR_LEN + ELEM_MAX_LEN_EXT_CAP));

	prMsduInfo->u2FrameLength += IE_SIZE(prExtCap);
#endif
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
UINT32 rlmFillHtOpIeBody(P_BSS_INFO_T prBssInfo, UINT_8 *pFme)
{
	P_IE_HT_OP_T prHtOp;
	UINT_16 i;

	prHtOp = (P_IE_HT_OP_T) pFme;

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

	return sizeof(IE_HT_OP_T);
}

static VOID rlmFillHtOpIE(P_ADAPTER_T prAdapter, P_BSS_INFO_T prBssInfo, P_MSDU_INFO_T prMsduInfo)
{
/* P_IE_HT_OP_T        prHtOp; */
/* UINT_16             i; */

	ASSERT(prAdapter);
	ASSERT(prBssInfo);
	ASSERT(prMsduInfo);

	prMsduInfo->u2FrameLength += rlmFillHtOpIeBody(prBssInfo,
						       (((PUINT_8) prMsduInfo->prPacket) + prMsduInfo->u2FrameLength));
#if 0
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
#endif
}
#if CFG_SUPPORT_P2P_ECSA
VOID rlmGenerateCSAIE(P_ADAPTER_T prAdapter, P_MSDU_INFO_T prMsduInfo)
{
	P_IE_CHANNEL_SWITCH_T prCsaIe;
	P_BSS_INFO_T prBssInfo;

	prBssInfo = &prAdapter->rWifiVar.arBssInfo[prMsduInfo->ucNetworkType];

	prCsaIe = (P_IE_CHANNEL_SWITCH_T)
	    (((PUINT_8) prMsduInfo->prPacket) + prMsduInfo->u2FrameLength);

	/* Add CSA IE */
	prCsaIe->ucId = ELEM_ID_CH_SW_ANNOUNCEMENT;
	prCsaIe->ucLength = 3;
	prCsaIe->ucChannelSwitchCount = prBssInfo->ucSwitchCount;
	prCsaIe->ucChannelSwitchMode = prBssInfo->ucSwitchMode;
	prCsaIe->ucNewChannelNum = prBssInfo->ucEcsaChannel;
	prMsduInfo->u2FrameLength += IE_SIZE(prCsaIe);
}

VOID rlmGenerateECSAIE(P_ADAPTER_T prAdapter, P_MSDU_INFO_T prMsduInfo)
{
	P_IE_EXT_CHANNEL_SWITCH_T prEcsaIe;
	P_BSS_INFO_T prBssInfo;

	prBssInfo = &prAdapter->rWifiVar.arBssInfo[prMsduInfo->ucNetworkType];

	prEcsaIe = (P_IE_EXT_CHANNEL_SWITCH_T)
	    (((PUINT_8) prMsduInfo->prPacket) + prMsduInfo->u2FrameLength);

	/* Add ECSA IE */
	prEcsaIe->ucId = ELEM_ID_CH_ESW_ANNOUNCEMENT;
	prEcsaIe->ucLength = 4;
	prEcsaIe->ucOpClass = prBssInfo->ucOpClass;
	prEcsaIe->ucChannelSwitchCount = prBssInfo->ucSwitchCount;
	prEcsaIe->ucChannelSwitchMode = prBssInfo->ucSwitchMode;
	prEcsaIe->ucNewChannelNum = prBssInfo->ucEcsaChannel;
	prMsduInfo->u2FrameLength += IE_SIZE(prEcsaIe);
}

VOID  rlmFreqToChannelExt(unsigned int freq,
						   int sec_channel,
						   u8 *op_class, u8 *channel)
{
	/* TODO: more operating classes */
	if (sec_channel > 1 || sec_channel < -1 || !op_class || !channel)
		return;

	if (freq >= 2412 && freq <= 2472) {
		if ((freq - 2407) % 5)
			return;
		/* 2.407 GHz, channels 1..13 */
		if (sec_channel == 1)
			*op_class = 83;
		else if (sec_channel == -1)
			*op_class = 84;
		else
			*op_class = 81;
		*channel = (freq - 2407) / 5;
		return;
	}

	if (freq == 2484) {
		if (sec_channel)
			return;
		*op_class = 82; /* channel 14 */
		*channel = 14;
		return;
	}

	if (freq >= 4900 && freq < 5000) {
		if ((freq - 4000) % 5)
			return;
		*channel = (freq - 4000) / 5;
		*op_class = 0; /* TODO */
		return;
	}

	/* 5 GHz, channels 36..48 */
	if (freq >= 5180 && freq <= 5240) {
		if ((freq - 5000) % 5)
			return;
		if (sec_channel == 1)
			*op_class = 116;
		else if (sec_channel == -1)
			*op_class = 117;
		else
			*op_class = 115;
		*channel = (freq - 5000) / 5;
		return;
	}

	/* 5 GHz, channels 149..169 */
	if (freq >= 5745 && freq <= 5845) {
		if ((freq - 5000) % 5)
			return;

		if (sec_channel == 1)
			*op_class = 126;
		else if (sec_channel == -1)
			*op_class = 127;
		else if (freq <= 5805)
			*op_class = 124;
		else
			*op_class = 125;
		*channel = (freq - 5000) / 5;
		return;
	}

	/* 5 GHz, channels 100..140 */
	if (freq >= 5000 && freq <= 5700) {
		if ((freq - 5000) % 5)
			return;
		if (sec_channel == 1)
			*op_class = 122;
		else if (sec_channel == -1)
			*op_class = 123;
		else
			*op_class = 121;
		*channel = (freq - 5000) / 5;
		return;
	}

	if (freq >= 5000 && freq < 5900) {
		if ((freq - 5000) % 5)
			return;
		*channel = (freq - 5000) / 5;
		*op_class = 0; /* TODO */
		return;
	}

	/* 56.16 GHz, channel 1..4 */
	if (freq >= 56160 + 2160 * 1 && freq <= 56160 + 2160 * 4) {
		if (sec_channel)
			return;
		*channel = (freq - 56160) / 2160;
		*op_class = 180;
		return;
	}
}


/*
 * generate channel switch Action frame header
 */
void rlmGenActionCSHdr(u8 *buf,
			u8 *da, u8 *sa, u8 *bssid,
			u8 category, u8 action)
{
	P_ACTION_CHANNEL_SWITCH_FRAME p = (P_ACTION_CHANNEL_SWITCH_FRAME)buf;

	if (!buf || !da || !sa || !bssid)
		return;

	/* build MAC header */
	p->u2FrameCtrl = MAC_FRAME_ACTION;
	p->u2Duration = 0;
	p->u2SeqCtrl = 0;
	COPY_MAC_ADDR(p->aucDestAddr, da);
	COPY_MAC_ADDR(p->aucSrcAddr, sa);
	COPY_MAC_ADDR(p->aucBSSID, bssid);

	p->ucCategory = category;
	p->ucAction = action;
}

void rlmGenActionCSA(u8 *buf,
			u8 mode,
			u8 channel,
			u8 count,
			u8 sco)
{
	P_ACTION_CSA_T p = (P_ACTION_CSA_T)buf;

	p->csa_id = ELEM_ID_CH_SW_ANNOUNCEMENT;
	p->clen = 3;
	p->mode = mode;
	p->new_ch_num = channel;
	p->count = count;

	p->sco_id = ELEM_ID_SCO;
	p->slen = 1;
	p->sco = sco;
}

void rlmGenActionECSA(u8 *buf,
			u8 mode,
			u8 channel,
			u8 count,
			u8 op_class)
{
	P_ACTION_ECSA_T p = (P_ACTION_ECSA_T)buf;

	p->mode = mode;
	p->new_operating_class = op_class;
	p->new_ch_num = channel;
	p->count = count;
}
#endif
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
#if CFG_SUPPORT_QUIET && 0
	BOOLEAN fgHasQuietIE = FALSE;
#endif

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

			prStaRec->u2HtCapInfo = prHtCap->u2HtCapInfo;
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

			if (!prBssInfo->fg40mBwAllowed)
				prBssInfo->ucHtOpInfo1 &= ~(HT_OP_INFO1_SCO | HT_OP_INFO1_STA_CHNL_WIDTH);

			if ((prBssInfo->ucHtOpInfo1 & HT_OP_INFO1_SCO) != CHNL_EXT_RES)
				prBssInfo->eBssSCO = (ENUM_CHNL_EXT_T)(prBssInfo->ucHtOpInfo1 & HT_OP_INFO1_SCO);

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

#if CFG_SUPPORT_DFS		/* Add by Enlai */
		case ELEM_ID_CH_SW_ANNOUNCEMENT:
			{
				rlmProcessChannelSwitchIE(prAdapter, (P_IE_CHANNEL_SWITCH_T) pucIE);
			}
			break;

#if CFG_SUPPORT_QUIET && 0
			/* Note: RRM code should be moved to independent RRM function by
			 *       component design rule. But we attach it to RLM temporarily
			 */
		case ELEM_ID_QUIET:
			rrmQuietHandleQuietIE(prBssInfo, (P_IE_QUIET_T) pucIE);
			fgHasQuietIE = TRUE;
			break;
#endif
#endif

		default:
			break;
		}		/* end of switch */
	}			/* end of IE_FOR_EACH */

	/* Some AP will have wrong channel number (255) when running time.
	 * Check if correct channel number information. 20110501
	 */
	if ((prBssInfo->eBand == BAND_2G4 && ucPrimaryChannel > 14) ||
	    (prBssInfo->eBand != BAND_2G4 && (ucPrimaryChannel >= 200 || ucPrimaryChannel <= 14)))
		ucPrimaryChannel = 0;
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

	if ((prAdapter == NULL)
		|| (pucIE == NULL)
		|| (prBssInfo == NULL)
		|| (prSwRfb == NULL)) {
		ASSERT(FALSE);
		return FALSE;
	}

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
	if (HIF_RX_HDR_GET_RF_BAND(prSwRfb->prHifRxHdr) != BAND_2G4)
		return FALSE;

	if (ucPriChannel == 0 || ucPriChannel > 14)
		ucPriChannel = HIF_RX_HDR_GET_CHNL_NUM(prSwRfb->prHifRxHdr);

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
	/* For checking if syncing params are different from
	 * last syncing and need to sync again
	 */
	CMD_SET_BSS_RLM_PARAM_T rBssRlmParam;
	BOOLEAN fgNewParameter = FALSE;

	if ((prAdapter == NULL)
		|| (pucIE == NULL)
		|| (prBssInfo == NULL)
		|| (prSwRfb == NULL)) {
		ASSERT(FALSE);
		return FALSE;
	}

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
	prBssInfo->fgUseShortSlotTime = (prBssInfo->u2CapInfo & CAP_INFO_SHORT_SLOT_TIME) ? TRUE : FALSE;

	rBssRlmParam.ucRfBand = (UINT_8) prBssInfo->eBand;
	rBssRlmParam.ucPrimaryChannel = prBssInfo->ucPrimaryChannel;
	rBssRlmParam.ucRfSco = (UINT_8) prBssInfo->eBssSCO;
	rBssRlmParam.ucErpProtectMode = (UINT_8) prBssInfo->fgErpProtectMode;
	rBssRlmParam.ucHtProtectMode = (UINT_8) prBssInfo->eHtProtectMode;
	rBssRlmParam.ucGfOperationMode = (UINT_8) prBssInfo->eGfOperationMode;
	rBssRlmParam.ucTxRifsMode = (UINT_8) prBssInfo->eRifsOperationMode;
	rBssRlmParam.u2HtOpInfo3 = prBssInfo->u2HtOpInfo3;
	rBssRlmParam.u2HtOpInfo2 = prBssInfo->u2HtOpInfo2;
	rBssRlmParam.ucHtOpInfo1 = prBssInfo->ucHtOpInfo1;
	rBssRlmParam.ucUseShortPreamble = prBssInfo->fgUseShortPreamble;
	rBssRlmParam.ucUseShortSlotTime = prBssInfo->fgUseShortSlotTime;

	rlmRecIeInfoForClient(prAdapter, prBssInfo, pucIE, u2IELength);

	if (rBssRlmParam.ucRfBand != prBssInfo->eBand
		|| rBssRlmParam.ucPrimaryChannel != prBssInfo->ucPrimaryChannel
		|| rBssRlmParam.ucRfSco != prBssInfo->eBssSCO
		|| rBssRlmParam.ucErpProtectMode != prBssInfo->fgErpProtectMode
		|| rBssRlmParam.ucHtProtectMode != prBssInfo->eHtProtectMode
		|| rBssRlmParam.ucGfOperationMode != prBssInfo->eGfOperationMode
		|| rBssRlmParam.ucTxRifsMode != prBssInfo->eRifsOperationMode
		|| rBssRlmParam.u2HtOpInfo3 != prBssInfo->u2HtOpInfo3
		|| rBssRlmParam.u2HtOpInfo2 != prBssInfo->u2HtOpInfo2
		|| rBssRlmParam.ucHtOpInfo1 != prBssInfo->ucHtOpInfo1
		|| rBssRlmParam.ucUseShortPreamble != prBssInfo->fgUseShortPreamble
		|| rBssRlmParam.ucUseShortSlotTime != prBssInfo->fgUseShortSlotTime)
		fgNewParameter = TRUE;
	else {
		DBGLOG(RLM, TRACE, "prBssInfo's params are all the same! not to sync!\n");
		fgNewParameter = FALSE;
	}

	return fgNewParameter;
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
	UINT_8 ucNetIdx;

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
	RLM_NET_FOR_EACH_NO_BOW(ucNetIdx) {
		prBssInfo = &prAdapter->rWifiVar.arBssInfo[ucNetIdx];
		ASSERT(prBssInfo);

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
											prBssInfo, prSwRfb, pucIE,
											u2IELength);
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
				/* Do nothing */
				/* To do: Ad-hoc */
			}

			/* Appy new parameters if necessary */
			if (fgNewParameter) {
				DBGLOG(RLM, TRACE, "rlmProcessBcn\n");
				rlmSyncOperationParams(prAdapter, prBssInfo);
				fgNewParameter = FALSE;
			}
		}		/* end of IS_BSS_ACTIVE() */
	}			/* end of RLM_NET_FOR_EACH_NO_BOW */
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
	UINT_16 u2Offset;

	ASSERT(prAdapter);
	ASSERT(prSwRfb);
	ASSERT(pucIE);

	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);
	ASSERT(prStaRec);
	if (!prStaRec)
		return;
	ASSERT(prStaRec->ucNetTypeIndex < NETWORK_TYPE_INDEX_NUM);

	prBssInfo = &prAdapter->rWifiVar.arBssInfo[prStaRec->ucNetTypeIndex];
	ASSERT(prStaRec == prBssInfo->prStaRecOfAP);

	/* To do: the invoked function is used to clear all members. It may be
	 *        done by center mechanism in invoker.
	 */
	rlmBssReset(prAdapter, prBssInfo);

	prBssInfo->fgUseShortSlotTime = (prBssInfo->u2CapInfo & CAP_INFO_SHORT_SLOT_TIME) ? TRUE : FALSE;

	ucPriChannel = rlmRecIeInfoForClient(prAdapter, prBssInfo, pucIE, u2IELength);
	if (ucPriChannel > 0)
		prBssInfo->ucPrimaryChannel = ucPriChannel;

	if (!RLM_NET_IS_11N(prBssInfo) || !(prStaRec->u2HtCapInfo & HT_CAP_INFO_SUP_CHNL_WIDTH))
		prBssInfo->fg40mBwAllowed = FALSE;

	/* Note: Update its capabilities to WTBL by cnmStaRecChangeState(), which
	 *       shall be invoked afterwards.
	 *       Update channel, bandwidth and protection mode by nicUpdateBss()
	 */
#if FIX_P2P_HT20
	if (prStaRec->ucNetTypeIndex == NETWORK_TYPE_P2P_INDEX) {

		DBGLOG(P2P, WARN, "Force P2P BW to 20\n");
		prBssInfo->fgAssoc40mBwAllowed = FALSE;
	}
#endif
	IE_FOR_EACH(pucIE, u2IELength, u2Offset) {
		switch (IE_ID(pucIE)) {
		case ELEM_ID_BSS_MAX_IDLE_PERIOD:
		{
			struct IE_BSS_MAX_IDLE_PERIOD *prBssMaxIdle = (struct IE_BSS_MAX_IDLE_PERIOD *)pucIE;

			prStaRec->u2MaxIdlePeriod = prBssMaxIdle->u2MaxIdlePeriod;
			prStaRec->ucIdleOption = prBssMaxIdle->ucIdleOption;
			break;
		}
		default:
			break;
		}		/* end of switch */
	}			/* end of IE_FOR_EACH */

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
VOID rlmFillSyncCmdParam(P_CMD_SET_BSS_RLM_PARAM_T prCmdBody, P_BSS_INFO_T prBssInfo)
{
	ASSERT(prCmdBody && prBssInfo);
	if (!prCmdBody || !prBssInfo)
		return;

	prCmdBody->ucNetTypeIndex = prBssInfo->ucNetTypeIndex;
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
	prCmdBody->ucCheckId = 0x72;

	if (RLM_NET_PARAM_VALID(prBssInfo)) {
		DBGLOG(RLM, INFO, "N=%d b=%d c=%d s=%d e=%d h=%d I=0x%02x l=%d p=%d\n",
				   prCmdBody->ucNetTypeIndex, prCmdBody->ucRfBand,
				   prCmdBody->ucPrimaryChannel, prCmdBody->ucRfSco,
				   prCmdBody->ucErpProtectMode, prCmdBody->ucHtProtectMode,
				   prCmdBody->ucHtOpInfo1, prCmdBody->ucUseShortSlotTime,
				   prCmdBody->ucUseShortPreamble);
	} else {
		DBGLOG(RLM, TRACE, "N=%d closed\n", prCmdBody->ucNetTypeIndex);
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
	ASSERT(prCmdBody);

	/* To do: exception handle */
	if (!prCmdBody) {
		DBGLOG(RLM, WARN, "No buf for sync RLM params (Net=%d)\n", prBssInfo->ucNetTypeIndex);
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

	ASSERT(rStatus == WLAN_STATUS_PENDING);

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

	ASSERT(prAdapter);
	ASSERT(prSwRfb);
	ASSERT(pucIE);

	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);
	ASSERT(prStaRec);
	if (!prStaRec)
		return;
	ASSERT(prStaRec->ucNetTypeIndex < NETWORK_TYPE_INDEX_NUM);

	prBssInfo = &prAdapter->rWifiVar.arBssInfo[prStaRec->ucNetTypeIndex];

	IE_FOR_EACH(pucIE, u2IELength, u2Offset) {
		switch (IE_ID(pucIE)) {
		case ELEM_ID_HT_CAP:
			if (!RLM_NET_IS_11N(prBssInfo) || IE_LEN(pucIE) != (sizeof(IE_HT_CAP_T) - 2))
				break;
			prHtCap = (P_IE_HT_CAP_T) pucIE;
			prStaRec->ucMcsSet = prHtCap->rSupMcsSet.aucRxMcsBitmask[0];
			prStaRec->fgSupMcs32 = (prHtCap->rSupMcsSet.aucRxMcsBitmask[32 / 8] & BIT(0)) ? TRUE : FALSE;

			prStaRec->u2HtCapInfo = prHtCap->u2HtCapInfo;
			prStaRec->ucAmpduParam = prHtCap->ucAmpduParam;
			prStaRec->u2HtExtendedCap = prHtCap->u2HtExtendedCap;
			prStaRec->u4TxBeamformingCap = prHtCap->u4TxBeamformingCap;
			prStaRec->ucAselCap = prHtCap->ucAselCap;
			break;

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

#if CFG_SUPPORT_DFS		/* Add by Enlai */
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
	P_ACTION_CHANNEL_SWITCH_FRAME prRxFrame;

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	DBGLOG(RLM, INFO, "[5G DFS]rlmProcessSpecMgtAction \r\n");

	prRxFrame = (P_ACTION_CHANNEL_SWITCH_FRAME) prSwRfb->pvHeader;
	DBGLOG(RLM, INFO, "[5G DFS]prRxFrame->ucAction[%d] \r\n", prRxFrame->ucAction);
	if (prRxFrame->ucAction == ACTION_CHNL_SWITCH)
		rlmProcessChannelSwitchIE(prAdapter, (P_IE_CHANNEL_SWITCH_T) prRxFrame->aucInfoElem);

}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function process Channel Switch IE
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID rlmProcessChannelSwitchIE(P_ADAPTER_T prAdapter, P_IE_CHANNEL_SWITCH_T prChannelSwitchIE)
{
	P_BSS_INFO_T prAisBssInfo;

	ASSERT(prAdapter);
	ASSERT(prChannelSwitchIE);

	DBGLOG(RLM, INFO, "[5G DFS] rlmProcessChannelSwitchIE \r\n");
	DBGLOG(RLM, INFO, "[5G DFS] ucChannelSwitchMode[%d], ucChannelSwitchCount[%d], ucNewChannelNum[%d] \r\n",
	       prChannelSwitchIE->ucChannelSwitchMode,
	       prChannelSwitchIE->ucChannelSwitchCount, prChannelSwitchIE->ucNewChannelNum);
	if (prChannelSwitchIE->ucChannelSwitchMode == 1) {
		prAisBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);
		DBGLOG(RLM, INFO, "[5G DFS] switch channel [%d]->[%d] \r\n", prAisBssInfo->ucPrimaryChannel,
		       prChannelSwitchIE->ucNewChannelNum);
		prAisBssInfo->ucPrimaryChannel = prChannelSwitchIE->ucNewChannelNum;
		nicUpdateBss(prAdapter, prAisBssInfo->ucNetTypeIndex);
	}

}

#endif

#if (CFG_SUPPORT_TXR_ENC == 1)
VOID
rlmTxRateEnhanceConfig(
	P_ADAPTER_T         prAdapter
	)
{
	P_GLUE_INFO_T prGlueInfo;
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;
	CMD_RLM_INFO_T rTxRInfo;
	UINT_32 u4SetInfoLen = 0;


	/* init */
	prGlueInfo = prAdapter->prGlueInfo;

	/* suggestion from Tsaiyuan.Hsu */
	kalMemZero(&rTxRInfo, sizeof(CMD_RLM_INFO_T));
	rTxRInfo.fgIsErrRatioEnhanceApplied = TRUE;
	rTxRInfo.ucErrRatio2LimitMinRate = 3;
	rTxRInfo.ucMinLegacyRateIdx = 2;
	rTxRInfo.cMinRssiThreshold = -60;
	rTxRInfo.fgIsRtsApplied = TRUE;
	rTxRInfo.ucRecoverTime = 60;

	DBGLOG(RLM, INFO, "Enable tx rate enhance function\n");

	rStatus = kalIoctl(prGlueInfo,
			wlanoidSetTxRateInfo,
			&rTxRInfo,
			sizeof(rTxRInfo),
			TRUE,
			TRUE,
			TRUE,
			FALSE,
			&u4SetInfoLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(RLM, WARN, "set tx rate advance info fail 0x%lx\n", rStatus);
}


/*----------------------------------------------------------------------------*/
/*! \brief  This routine is called to send a command to TX Auto Rate module.
*
* \param[in] prGlueInfo		Pointer to the Adapter structure
* \param[in] prInBuf		A pointer to the command string buffer
* \param[in] u4InBufLen	The length of the buffer
* \param[out] None
*
* \retval None
*/
/*----------------------------------------------------------------------------*/
VOID
rlmCmd(P_GLUE_INFO_T prGlueInfo, UINT_8	*prInBuf, UINT_32 u4InBufLen)
{
	UINT_32 u4Subcmd;


	/* parse TAR sub-command */
	u4Subcmd = CmdStringDecParse(prInBuf, &prInBuf, &u4InBufLen);
	DBGLOG(RLM, INFO, "<tar_cmd> sub command = %u\n", (UINT32)u4Subcmd);

	/* handle different sub-command */
	switch (u4Subcmd) {
	case 0x00: /* configure */
		/* iwpriv wlan0 set_str_cmd 1_0_0_1_3_2_60_1_60 */
		WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;
		CMD_RLM_INFO_T rTxRInfo;
		UINT_32 u4SetInfoLen = 0;

		kalMemZero(&rTxRInfo, sizeof(CMD_RLM_INFO_T));
		rTxRInfo.u4Version = CmdStringDecParse(prInBuf, &prInBuf, &u4InBufLen);
		rTxRInfo.fgIsErrRatioEnhanceApplied = CmdStringDecParse(prInBuf, &prInBuf, &u4InBufLen);
		rTxRInfo.ucErrRatio2LimitMinRate = CmdStringDecParse(prInBuf, &prInBuf, &u4InBufLen);
		rTxRInfo.ucMinLegacyRateIdx = CmdStringDecParse(prInBuf, &prInBuf, &u4InBufLen);
		rTxRInfo.cMinRssiThreshold = 0 - CmdStringDecParse(prInBuf, &prInBuf, &u4InBufLen);
		rTxRInfo.fgIsRtsApplied = CmdStringDecParse(prInBuf, &prInBuf, &u4InBufLen);
		rTxRInfo.ucRecoverTime = CmdStringDecParse(prInBuf, &prInBuf, &u4InBufLen);

		DBGLOG(RLM, INFO, "<tar_cmd> rlmCmd = %u %u %u %u %d %u %u\n",
			rTxRInfo.u4Version,
			rTxRInfo.fgIsErrRatioEnhanceApplied,
			rTxRInfo.ucErrRatio2LimitMinRate,
			rTxRInfo.ucMinLegacyRateIdx,
			rTxRInfo.cMinRssiThreshold,
			rTxRInfo.fgIsRtsApplied,
			rTxRInfo.ucRecoverTime));

		rStatus = kalIoctl(prGlueInfo,
				wlanoidSetTxRateInfo,
				&rTxRInfo,
				sizeof(rTxRInfo),
				TRUE,
				TRUE,
				TRUE,
				FALSE,
				&u4SetInfoLen);
		break;

	default:
		break;
	}
}
#endif /* CFG_SUPPORT_TXR_ENC */

VOID rlmComposeEmptyBeaconReport(P_ADAPTER_T prAdapter)
{
	struct RADIO_MEASUREMENT_REQ_PARAMS *prRmReq = &prAdapter->rWifiVar.rRmReqParams;
	struct RADIO_MEASUREMENT_REPORT_PARAMS *prRmRep = &prAdapter->rWifiVar.rRmRepParams;
	PUINT_8 pucReportFrame = prRmRep->pucReportFrameBuff + prRmRep->u2ReportFrameLen;
	P_IE_MEASUREMENT_REPORT_T prRepIE = (P_IE_MEASUREMENT_REPORT_T)pucReportFrame;
	struct RM_BCN_REPORT *prBcnReport = (struct RM_BCN_REPORT *)prRepIE->aucReportFields;

	/* fill in basic content of Measurement report IE */
	prRepIE->ucId = ELEM_ID_MEASUREMENT_REPORT;
	prRepIE->ucToken = prRmReq->prCurrMeasElem->ucToken;
	prRepIE->ucMeasurementType = prRmReq->prCurrMeasElem->ucMeasurementType;
	prRepIE->ucReportMode = 0;
	prRepIE->ucLength = 3 + OFFSET_OF(struct RM_BCN_REPORT, aucOptElem);
	kalMemZero(prBcnReport, OFFSET_OF(struct RM_BCN_REPORT, aucOptElem));
	prBcnReport->ucRegulatoryClass = 255; /* 255 means reglatory is not available */
	prBcnReport->ucChannel = 255; /* 255 means channel is not available */
	prBcnReport->ucReportInfo = 255; /* 255 means report frame info is not available */
	prBcnReport->ucRSNI = 255; /* 255 means RSNI is not available */
	prBcnReport->ucAntennaID = 1;

	prRmRep->u2ReportFrameLen += IE_SIZE(&prRepIE);
}

VOID rlmFreeMeasurementResources(P_ADAPTER_T prAdapter)
{
	struct RADIO_MEASUREMENT_REQ_PARAMS *prRmReq = &prAdapter->rWifiVar.rRmReqParams;
	struct RADIO_MEASUREMENT_REPORT_PARAMS *prRmRep = &prAdapter->rWifiVar.rRmRepParams;
	struct RM_MEASURE_REPORT_ENTRY *prReportEntry = NULL;
	P_LINK_T prReportLink = &prRmRep->rReportLink;
	P_LINK_T prFreeReportLink = &prRmRep->rFreeReportLink;

	kalMemFree(prRmReq->pucReqIeBuf, VIR_MEM_TYPE, prRmReq->u2ReqIeBufLen);
	kalMemFree(prRmRep->pucReportFrameBuff, VIR_MEM_TYPE, RM_REPORT_FRAME_MAX_LENGTH);
	while (!LINK_IS_EMPTY(prReportLink)) {
		LINK_REMOVE_HEAD(prReportLink, prReportEntry, struct RM_MEASURE_REPORT_ENTRY *);
		kalMemFree(prReportEntry, VIR_MEM_TYPE, sizeof(*prReportEntry));
	}
	while (!LINK_IS_EMPTY(prFreeReportLink)) {
		LINK_REMOVE_HEAD(prFreeReportLink, prReportEntry, struct RM_MEASURE_REPORT_ENTRY *);
		kalMemFree(prReportEntry, VIR_MEM_TYPE, sizeof(*prReportEntry));
	}
	kalMemZero(prRmReq, sizeof(*prRmReq));
	kalMemZero(prRmRep, sizeof(*prRmRep));
	prRmReq->rBcnRmParam.eState = RM_NO_REQUEST;
	prRmReq->fgRmIsOngoing = FALSE;
	LINK_INITIALIZE(&prRmRep->rFreeReportLink);
	LINK_INITIALIZE(&prRmRep->rReportLink);
}

/* purpose: check if Radio Measurement is done */
static BOOLEAN rlmAllMeasurementIssued(struct RADIO_MEASUREMENT_REQ_PARAMS *prReq)
{
	return prReq->u2RemainReqLen > IE_SIZE(prReq->prCurrMeasElem) ? FALSE : TRUE;
}

VOID rlmComposeIncapableRmRep(
	struct RADIO_MEASUREMENT_REPORT_PARAMS *prRep, UINT_8 ucToken, UINT_8 ucMeasType)
{
	P_IE_MEASUREMENT_REPORT_T prRepIE =
		(P_IE_MEASUREMENT_REPORT_T)(prRep->pucReportFrameBuff + prRep->u2ReportFrameLen);

	prRepIE->ucId = ELEM_ID_MEASUREMENT_REPORT;
	prRepIE->ucToken = ucToken;
	prRepIE->ucMeasurementType = ucMeasType;
	prRepIE->ucLength = 3;
	prRepIE->ucReportMode = RM_REP_MODE_INCAPABLE;
	prRep->u2ReportFrameLen += 5;
}
/* Purpose: Interative processing Measurement Request Element. If it is not the first element,
	will copy all collected report element to the report frame buffer. and may tx the radio report frame.
	prAdapter: pointer to the Adapter
	fgNewStarted: if it is the first element in measurement request frame
*/
VOID rlmStartNextMeasurement(P_ADAPTER_T prAdapter, BOOLEAN fgNewStarted)
{
	struct RADIO_MEASUREMENT_REQ_PARAMS *prRmReq = &prAdapter->rWifiVar.rRmReqParams;
	struct RADIO_MEASUREMENT_REPORT_PARAMS *prRmRep = &prAdapter->rWifiVar.rRmRepParams;
	P_IE_MEASUREMENT_REQ_T prCurrReq = prRmReq->prCurrMeasElem;
	UINT_16 u2RandomTime = 0;

schedule_next:
	if (!prRmReq->fgRmIsOngoing) {
		DBGLOG(RLM, INFO, "Rm has been stopped\n");
		return;
	}
	/* we don't support parallel measurement now */
	if (prCurrReq->ucRequestMode & RM_REQ_MODE_PARALLEL_BIT) {
		DBGLOG(RLM, WARN, "Parallel request, compose incapable report\n");
		if (prRmRep->u2ReportFrameLen + 5 > RM_REPORT_FRAME_MAX_LENGTH)
			rlmTxRadioMeasurementReport(prAdapter);
		rlmComposeIncapableRmRep(prRmRep, prCurrReq->ucToken, prCurrReq->ucMeasurementType);
		if (rlmAllMeasurementIssued(prRmReq)) {
			if (prRmReq->rBcnRmParam.fgExistBcnReq && RM_EXIST_REPORT(prRmRep))
				rlmComposeEmptyBeaconReport(prAdapter);
			rlmTxRadioMeasurementReport(prAdapter);

			/* repeat measurement if repetitions is required and not only parallel measurements.
			** otherwise, no need to repeat, it is not make sense to do that.
			*/
			if (prRmReq->u2Repetitions > 0) {
				prRmReq->fgInitialLoop = FALSE;
				prRmReq->u2Repetitions--;
				prCurrReq = prRmReq->prCurrMeasElem = (P_IE_MEASUREMENT_REQ_T)prRmReq->pucReqIeBuf;
				prRmReq->u2RemainReqLen = prRmReq->u2ReqIeBufLen;
			} else {
				rlmFreeMeasurementResources(prAdapter);
				DBGLOG(RLM, INFO, "Radio Measurement done\n");
				return;
			}
		} else {
			UINT_16 u2IeSize = IE_SIZE(prRmReq->prCurrMeasElem);

			prCurrReq = prRmReq->prCurrMeasElem =
				(P_IE_MEASUREMENT_REQ_T)((PUINT_8)prRmReq->prCurrMeasElem + u2IeSize);
			prRmReq->u2RemainReqLen -= u2IeSize;
		}
		fgNewStarted = FALSE;
		goto schedule_next;
	}
	/* copy collected measurement report for specific measurement type */
	if (!fgNewStarted) {
		struct RM_MEASURE_REPORT_ENTRY *prReportEntry = NULL;
		P_LINK_T prReportLink = &prRmRep->rReportLink;
		P_LINK_T prFreeReportLink = &prRmRep->rFreeReportLink;
		PUINT_8 pucReportFrame = prRmRep->pucReportFrameBuff + prRmRep->u2ReportFrameLen;
		UINT_16 u2IeSize = 0;
		BOOLEAN fgNewLoop = FALSE;

		DBGLOG(RLM, INFO, "total %u report element for current request\n", prReportLink->u4NumElem);
		/* copy collected report into the Measurement Report Frame Buffer. */
		while (1) {
			LINK_REMOVE_HEAD(prReportLink, prReportEntry, struct RM_MEASURE_REPORT_ENTRY *);
			if (!prReportEntry)
				break;
			u2IeSize = IE_SIZE(prReportEntry->aucMeasReport);
			/* if reach the max length of a MMPDU size, send a Rm report first */
			if (u2IeSize + prRmRep->u2ReportFrameLen > RM_REPORT_FRAME_MAX_LENGTH) {
				rlmTxRadioMeasurementReport(prAdapter);
				pucReportFrame = prRmRep->pucReportFrameBuff + prRmRep->u2ReportFrameLen;
			}
			kalMemCopy(pucReportFrame, prReportEntry->aucMeasReport, u2IeSize);
			pucReportFrame += u2IeSize;
			prRmRep->u2ReportFrameLen += u2IeSize;
			LINK_INSERT_TAIL(prFreeReportLink, &prReportEntry->rLinkEntry);
		}
		/* if Measurement is done, free report element memory */
		if (rlmAllMeasurementIssued(prRmReq)) {
			if (prRmReq->rBcnRmParam.fgExistBcnReq && RM_EXIST_REPORT(prRmRep))
				rlmComposeEmptyBeaconReport(prAdapter);
			rlmTxRadioMeasurementReport(prAdapter);

			/* repeat measurement if repetitions is required */
			if (prRmReq->u2Repetitions > 0) {
				fgNewLoop = TRUE;
				prRmReq->fgInitialLoop = FALSE;
				prRmReq->u2Repetitions--;
				prRmReq->prCurrMeasElem = (P_IE_MEASUREMENT_REQ_T)prRmReq->pucReqIeBuf;
				prRmReq->u2RemainReqLen = prRmReq->u2ReqIeBufLen;
			} else {
				/* don't free radio measurement resource due to TSM is running */
				if (!wmmTsmIsOngoing(prAdapter)) {
					rlmFreeMeasurementResources(prAdapter);
					DBGLOG(RLM, INFO, "Radio Measurement done\n");
				}
				return;
			}
		}
		if (!fgNewLoop) {
			u2IeSize = IE_SIZE(prRmReq->prCurrMeasElem);
			prCurrReq = prRmReq->prCurrMeasElem =
				(P_IE_MEASUREMENT_REQ_T)((PUINT_8)prRmReq->prCurrMeasElem + u2IeSize);
			prRmReq->u2RemainReqLen -= u2IeSize;
		}
	}

	/* do specific measurement */
	switch (prCurrReq->ucMeasurementType) {
	case ELEM_RM_TYPE_BEACON_REQ:
	{
		P_RM_BCN_REQ_T prBeaconReq = (P_RM_BCN_REQ_T)&prCurrReq->aucRequestFields[0];

		if (!prRmReq->fgInitialLoop) {
			/* If this is the repeating measurement, then wait next scan done */
			prRmReq->rBcnRmParam.eState = RM_WAITING;
			break;
		}
		if (prBeaconReq->u2RandomInterval == 0)
			rlmDoBeaconMeasurement(prAdapter, 0);
		else {
			get_random_bytes(&u2RandomTime, 2);
			u2RandomTime = (u2RandomTime * prBeaconReq->u2RandomInterval) / 65535;
			u2RandomTime = TU_TO_MSEC(u2RandomTime);
			if (u2RandomTime > 0) {
				cnmTimerStopTimer(prAdapter, &rBeaconReqTimer);
				cnmTimerInitTimer(prAdapter, &rBeaconReqTimer, rlmDoBeaconMeasurement, 0);
				cnmTimerStartTimer(prAdapter, &rBeaconReqTimer, u2RandomTime);
			} else
				rlmDoBeaconMeasurement(prAdapter, 0);
		}
		break;
	}
	case ELEM_RM_TYPE_TSM_REQ:
	{
		P_RM_TS_MEASURE_REQ_T prTsmReqIE = (P_RM_TS_MEASURE_REQ_T)&prCurrReq->aucRequestFields[0];
		struct RM_TSM_REQ *prTsmReq = NULL;
		UINT_16 u2OffSet = 0;
		PUINT_8 pucIE = prTsmReqIE->aucSubElements;
		P_ACTION_RM_REPORT_FRAME prReportFrame = NULL;

		/* In case of repeating measurement, no need to start triggered measurement again.
		** According to current specification of Radio Measurement, only TSM has the triggered
		** type of measurement.
		*/
		if ((prCurrReq->ucRequestMode & RM_REQ_MODE_ENABLE_BIT) && !prRmReq->fgInitialLoop)
			goto schedule_next;

		/* if enable bit is 1 and report bit is 0, need to stop all triggered TSM measurement */
		if ((prCurrReq->ucRequestMode & (RM_REQ_MODE_ENABLE_BIT|RM_REQ_MODE_REPORT_BIT)) ==
			RM_REQ_MODE_ENABLE_BIT) {
			wmmRemoveAllTsmMeasurement(prAdapter, TRUE);
			break;
		}
		prTsmReq = cnmMemAlloc(prAdapter, RAM_TYPE_BUF, sizeof(struct RM_TSM_REQ));
		if (!prTsmReq) {
			DBGLOG(RLM, ERROR, "No memory\n");
			break;
		}
		prTsmReq->ucToken = prCurrReq->ucToken;
		prTsmReq->u2Duration = prTsmReqIE->u2Duration;
		prTsmReq->ucTID = (prTsmReqIE->ucTrafficID & 0xf0) >> 4;
		prTsmReq->ucB0Range = prTsmReqIE->ucBin0Range;
		prReportFrame = (P_ACTION_RM_REPORT_FRAME)prRmRep->pucReportFrameBuff;
		COPY_MAC_ADDR(prTsmReq->aucPeerAddr, prReportFrame->aucDestAddr);
		IE_FOR_EACH(pucIE, prCurrReq->ucLength - 3, u2OffSet) {
			switch (IE_ID(pucIE)) {
			case 1: /* Triggered Reporting */
				kalMemCopy(&prTsmReq->rTriggerCond, pucIE+2, IE_LEN(pucIE));
				break;
			case 221: /* Vendor Specified */
				break; /* No vendor IE now */
			default:
				break;
			}
		}
		if (!prTsmReqIE->u2RandomInterval) {
			wmmStartTsmMeasurement(prAdapter, (ULONG)prTsmReq);
			break;
		}
		get_random_bytes(&u2RandomTime, 2);
		u2RandomTime = (u2RandomTime * prTsmReqIE->u2RandomInterval) / 65535;
		u2RandomTime = TU_TO_MSEC(u2RandomTime);
		cnmTimerStopTimer(prAdapter, &rTSMReqTimer);
		cnmTimerInitTimer(prAdapter, &rTSMReqTimer, wmmStartTsmMeasurement, (ULONG)prTsmReq);
		cnmTimerStartTimer(prAdapter, &rTSMReqTimer, u2RandomTime);
		break;
	}
	default:
	{
		if (prRmRep->u2ReportFrameLen + 5 > RM_REPORT_FRAME_MAX_LENGTH)
			rlmTxRadioMeasurementReport(prAdapter);
		rlmComposeIncapableRmRep(prRmRep, prCurrReq->ucToken, prCurrReq->ucMeasurementType);
		fgNewStarted = FALSE;
		DBGLOG(RLM, INFO, "RM type %d is not supported on this chip\n", prCurrReq->ucMeasurementType);
		goto schedule_next;
	}
	}
}

/* If disconnect with the target AP, radio measurement should be canceled. */
VOID rlmCancelRadioMeasurement(P_ADAPTER_T prAdapter)
{
	BOOLEAN fgHasBcnReqTimer = timerPendingTimer(&rBeaconReqTimer);
	BOOLEAN fgHasTsmTimer = timerPendingTimer(&rTSMReqTimer);

	DBGLOG(RLM, INFO, "Cancel measurement, Beacon Req timer is %d and TSM Req timer is %d\n",
		fgHasBcnReqTimer, fgHasTsmTimer);
	if (fgHasBcnReqTimer)
		cnmTimerStopTimer(prAdapter, &rBeaconReqTimer);
	if (fgHasTsmTimer)
		cnmTimerStopTimer(prAdapter, &rTSMReqTimer);
	rlmFreeMeasurementResources(prAdapter);
}

BOOLEAN rlmBcnRmRunning(P_ADAPTER_T prAdapter)
{
	return prAdapter->rWifiVar.rRmReqParams.rBcnRmParam.eState == RM_ON_GOING;
}

BOOLEAN rlmFillScanMsg(P_ADAPTER_T prAdapter, P_MSG_SCN_SCAN_REQ prMsg)
{
	struct RADIO_MEASUREMENT_REQ_PARAMS *prRmReq = &prAdapter->rWifiVar.rRmReqParams;
	P_IE_MEASUREMENT_REQ_T prCurrReq = NULL;
	P_RM_BCN_REQ_T prBeaconReq = NULL;
	UINT_16 u2RemainLen = 0;
	PUINT_8 pucSubIE = NULL;

	if (prRmReq->rBcnRmParam.eState != RM_ON_GOING || !prMsg)
		return FALSE;

	prCurrReq = prRmReq->prCurrMeasElem;
	prBeaconReq = (P_RM_BCN_REQ_T)&prCurrReq->aucRequestFields[0];
	prMsg->ucSSIDType = SCAN_REQ_SSID_WILDCARD;
	switch (prBeaconReq->ucMeasurementMode) {
	case RM_BCN_REQ_PASSIVE_MODE:
		prMsg->eScanType = SCAN_TYPE_PASSIVE_SCAN;
		break;
	case RM_BCN_REQ_ACTIVE_MODE:
		prMsg->eScanType = SCAN_TYPE_ACTIVE_SCAN;
		break;
	}

	WLAN_GET_FIELD_16(&prBeaconReq->u2Duration, &prMsg->u2ChannelDwellTime);
	COPY_MAC_ADDR(prMsg->aucBSSID, prBeaconReq->aucBssid);
#if 0 /* debug specific bssid scan */
	kalMemCopy(prMsg->aucBSSID, "\x74\x67\xf7\x17\xf4\xd0", MAC_ADDR_LEN);
#endif
	/* if mandatory bit is set, we should do */
	if (prCurrReq->ucRequestMode & RM_REQ_MODE_DURATION_MANDATORY_BIT)
		prMsg->u2MinChannelDwellTime = prMsg->u2ChannelDwellTime;
	else
		prMsg->u2MinChannelDwellTime = (prMsg->u2ChannelDwellTime * 2) / 3;
	if (prBeaconReq->ucChannel == 0)
		prMsg->eScanChannel = SCAN_CHANNEL_FULL;
	else if (prBeaconReq->ucChannel == 255) { /* latest Ap Channel Report */
		P_BSS_DESC_T prBssDesc = prAdapter->rWifiVar.rAisFsmInfo.prTargetBssDesc;
		PUINT_8 pucChnl = NULL;
		UINT_8 ucChnlNum = 0;
		UINT_8 ucIndex = 0;
		P_RF_CHANNEL_INFO_T prChnlInfo = prMsg->arChnlInfoList;

		prMsg->eScanChannel = SCAN_CHANNEL_SPECIFIED;
		prMsg->ucChannelListNum = 0;
		if (prBssDesc) {
			PUINT_8 pucIE = NULL;
			UINT_16 u2IELength = 0;
			UINT_16 u2Offset = 0;

			pucIE = prBssDesc->aucIEBuf;
			u2IELength = prBssDesc->u2IELength;
			IE_FOR_EACH(pucIE, u2IELength, u2Offset) {
				if (IE_ID(pucIE) != ELEM_ID_AP_CHANNEL_REPORT)
					continue;
				pucChnl = ((struct IE_AP_CHNL_REPORT_T *)pucIE)->aucChnlList;
				ucChnlNum = pucIE[1] - 1;
				DBGLOG(RLM, INFO, "Channel number in latest AP channel report %d\n", ucChnlNum);
				while (ucIndex < ucChnlNum &&
					prMsg->ucChannelListNum < MAXIMUM_OPERATION_CHANNEL_LIST) {
					if (pucChnl[ucIndex] <= 14)
						prChnlInfo[prMsg->ucChannelListNum].eBand = BAND_2G4;
					else
						prChnlInfo[prMsg->ucChannelListNum].eBand = BAND_5G;
					prChnlInfo[prMsg->ucChannelListNum].ucChannelNum = pucChnl[ucIndex];
					prMsg->ucChannelListNum++;
					ucIndex++;
				}
			}
		}
	} else {
		prMsg->eScanChannel = SCAN_CHANNEL_SPECIFIED;
		prMsg->ucChannelListNum = 1;
		prMsg->arChnlInfoList[0].ucChannelNum = prBeaconReq->ucChannel;
		if (prBeaconReq->ucChannel <= 14)
			prMsg->arChnlInfoList[0].eBand = BAND_2G4;
		else
			prMsg->arChnlInfoList[0].eBand = BAND_5G;
	}
	u2RemainLen = prCurrReq->ucLength - 3 - OFFSET_OF(RM_BCN_REQ_T, aucSubElements);
	pucSubIE = &prBeaconReq->aucSubElements[0];
	while (u2RemainLen > 0) {
		if (IE_SIZE(pucSubIE) > u2RemainLen)
			break;
		switch (pucSubIE[0]) {
		case 0: /* SSID */
			/* length of sub-element ssid is 0 or first byte is 0, means wildcard ssid matching */
			if (!IE_LEN(pucSubIE) || !pucSubIE[2])
				break;
			COPY_SSID(prMsg->aucSSID, prMsg->ucSSIDLength, &pucSubIE[2], pucSubIE[1]);
			prMsg->ucSSIDType = SCAN_REQ_SSID_SPECIFIED_ONLY;
			break;
		case 51: /* AP channel report */
		{
			struct IE_AP_CHNL_REPORT_T *prApChnl = (struct IE_AP_CHNL_REPORT_T *)pucSubIE;
			UINT_8 ucChannelCnt = prApChnl->ucLength - 1;
			UINT_8 ucIndex = 0;

			if (prBeaconReq->ucChannel == 0)
				break;
			prMsg->eScanChannel = SCAN_CHANNEL_SPECIFIED;
			DBGLOG(RLM, INFO, "Channel number in measurement AP channel report %d\n", ucChannelCnt);
			while (ucIndex < ucChannelCnt &&
				prMsg->ucChannelListNum < MAXIMUM_OPERATION_CHANNEL_LIST) {
				if (prApChnl->aucChnlList[ucIndex] <= 14)
					prMsg->arChnlInfoList[prMsg->ucChannelListNum].eBand = BAND_2G4;
				else
					prMsg->arChnlInfoList[prMsg->ucChannelListNum].eBand = BAND_5G;
				prMsg->arChnlInfoList[prMsg->ucChannelListNum].ucChannelNum =
					prApChnl->aucChnlList[ucIndex];
				prMsg->ucChannelListNum++;
				ucIndex++;
			}
			break;
		}
		}
		u2RemainLen -= IE_SIZE(pucSubIE);
		pucSubIE += IE_SIZE(pucSubIE);
	}

	/*
	 * If an AP Channel Report is not available in the STA, the STA shall
	 * iteratively conduct measurements on all supported channels in the
	 * specified Regulatory Class that are valid for the current
	 * regulatory domain.
	 */
	if (prMsg->ucChannelListNum == 0)
		prMsg->eScanChannel = SCAN_CHANNEL_FULL;

	DBGLOG(RLM, INFO, "SSIDtype %d, ScanType %d, ChnlType %d, Dwell %d, MinDwell %d, ChnlNum %d\n",
		prMsg->ucSSIDType, prMsg->eScanType, prMsg->eScanChannel, prMsg->u2ChannelDwellTime,
		prMsg->u2MinChannelDwellTime, prMsg->ucChannelListNum);
	return TRUE;
}

VOID rlmDoBeaconMeasurement(P_ADAPTER_T prAdapter, ULONG ulParam)
{
	P_CONNECTION_SETTINGS_T prConnSettings = &(prAdapter->rWifiVar.rConnSettings);
	struct RADIO_MEASUREMENT_REQ_PARAMS *prRmReq = &prAdapter->rWifiVar.rRmReqParams;
	P_RM_BCN_REQ_T prBcnReq = (P_RM_BCN_REQ_T)&prRmReq->prCurrMeasElem->aucRequestFields[0];

	if (prBcnReq->ucMeasurementMode == RM_BCN_REQ_TABLE_MODE) {
		P_LINK_T prBSSDescList = &prAdapter->rWifiVar.rScanInfo.rBSSDescList;
		P_BSS_DESC_T prBssDesc = NULL;
		struct RM_BEACON_REPORT_PARAMS rRepParams;
		PUINT_16 pu2BcnInterval = (PUINT_16)&rRepParams.aucBcnFixedField[8];
		PUINT_16 pu2CapInfo = (PUINT_16)&rRepParams.aucBcnFixedField[10];

		kalMemZero(&rRepParams, sizeof(rRepParams));
		/* if this is a one antenna only device, the antenna id is always 1. 7.3.2.40 */
		rRepParams.ucAntennaID = 1;
		rRepParams.ucRSNI = 255; /* 255 means RSNI not available. see 7.3.2.41 */
		rRepParams.ucFrameInfo = 255;

		prRmReq->rBcnRmParam.eState = RM_ON_GOING;
		prBcnReq->ucChannel = 0;
		DBGLOG(RLM, INFO, "Beacon Table Mode, Beacon Table Num %u\n", prBSSDescList->u4NumElem);
		LINK_FOR_EACH_ENTRY(prBssDesc, prBSSDescList, rLinkEntry, BSS_DESC_T) {
			rRepParams.ucRCPI = prBssDesc->ucRCPI;
			rRepParams.ucChannel = prBssDesc->ucChannelNum;
			kalMemCopy(&rRepParams.aucBcnFixedField, &prBssDesc->u8TimeStamp, 8);
			*pu2BcnInterval = prBssDesc->u2BeaconInterval;
			*pu2CapInfo = prBssDesc->u2CapInfo;
			scanCollectBeaconReport(prAdapter, prBssDesc->aucIEBuf, prBssDesc->u2IELength,
				prBssDesc->aucBSSID, &rRepParams);
		}
		rlmStartNextMeasurement(prAdapter, FALSE);
		return;
	}
	if (prConnSettings->fgIsScanReqIssued) {
		prRmReq->rBcnRmParam.eState = RM_WAITING;
	} else {
		prRmReq->rBcnRmParam.eState = RM_ON_GOING;
		GET_CURRENT_SYSTIME(&prRmReq->rStartTime);
		aisFsmScanRequest(prAdapter, NULL, NULL, 0);
	}
}

static BOOLEAN rlmRmFrameIsValid(P_SW_RFB_T prSwRfb)
{
	UINT_16 u2ElemLen = 0;
	UINT_16 u2Offset = (UINT_16)OFFSET_OF(ACTION_RM_REQ_FRAME, aucInfoElem);
	PUINT_8 pucIE = (PUINT_8)prSwRfb->pvHeader;
	P_IE_MEASUREMENT_REQ_T prCurrMeasElem = NULL;
	UINT_16 u2CalcIELen = 0;
	UINT_16 u2IELen = 0;

	if (prSwRfb->u2PacketLen <= u2Offset) {
		DBGLOG(RLM, ERROR, "Rm Packet length %d is too short\n", prSwRfb->u2PacketLen);
		return FALSE;
	}
	pucIE += u2Offset;
	u2ElemLen = prSwRfb->u2PacketLen - u2Offset;
	IE_FOR_EACH(pucIE, u2ElemLen, u2Offset) {
		u2IELen = IE_LEN(pucIE);

		/* The minimum value of the Length field is 3 (based on a minimum length for the */
		/* Measurement Request field of 0 octets) */
		if (u2IELen <= 3) {
			DBGLOG(RLM, ERROR, "Abnormal RM IE length is %d\n", u2IELen);
			return FALSE;
		}

		/* Check whether the length of each measurment request element is reasonable */
		prCurrMeasElem = (P_IE_MEASUREMENT_REQ_T)pucIE;
		switch (prCurrMeasElem->ucMeasurementType) {
		case ELEM_RM_TYPE_BEACON_REQ:
			if (u2IELen < (3 + OFFSET_OF(RM_BCN_REQ_T, aucSubElements))) {
				DBGLOG(RLM, ERROR, "Abnormal Becaon Req IE length is %d\n", u2IELen);
				return FALSE;
			}
			break;
		case ELEM_RM_TYPE_TSM_REQ:
			if (u2IELen < (3 + OFFSET_OF(RM_TS_MEASURE_REQ_T, aucSubElements))) {
				DBGLOG(RLM, ERROR, "Abnormal TSM Req IE length is %d\n", u2IELen);
				return FALSE;
			}
			break;
		default:
			DBGLOG(RLM, ERROR, "Not support: MeasurementType is %d, IE length is %d\n",
				prCurrMeasElem->ucMeasurementType, u2IELen);
			return FALSE;
		}

		u2CalcIELen += IE_SIZE(pucIE);
	}
	if (u2CalcIELen != u2ElemLen) {
		DBGLOG(RLM, ERROR, "Calculated Total IE len is not equal to received length\n");
		return FALSE;
	}
	return TRUE;
}
/*
*/
VOID rlmProcessRadioMeasurementRequest(P_ADAPTER_T prAdapter, P_SW_RFB_T prSwRfb)
{
	P_ACTION_RM_REQ_FRAME prRmReqFrame = NULL;
	P_ACTION_RM_REPORT_FRAME prReportFrame = NULL;
	struct RADIO_MEASUREMENT_REQ_PARAMS *prRmReqParam = NULL;
	struct RADIO_MEASUREMENT_REPORT_PARAMS *prRmRepParam = NULL;
	enum RM_REQ_PRIORITY eNewPriority;

	ASSERT(prAdapter);
	ASSERT(prSwRfb);
	prRmReqFrame = (P_ACTION_RM_REQ_FRAME)prSwRfb->pvHeader;
	prRmReqParam = &prAdapter->rWifiVar.rRmReqParams;
	prRmRepParam = &prAdapter->rWifiVar.rRmRepParams;

	if (!rlmRmFrameIsValid(prSwRfb))
		return;
	DBGLOG(RLM, INFO, "RM Request From %pM, DialogToken %d\n",
			prRmReqFrame->aucSrcAddr, prRmReqFrame->ucDialogToken);
	eNewPriority = rlmGetRmRequestPriority(prRmReqFrame->aucDestAddr);
	if (prRmReqParam->ePriority > eNewPriority) {
		DBGLOG(RLM, INFO, "ignore lower precedence rm request\n");
		return;
	}
	prRmReqParam->ePriority = eNewPriority;
	/* */
	if (prRmReqParam->fgRmIsOngoing) {
		DBGLOG(RLM, INFO, "Old RM is on-going, cancel it first\n");
		rlmTxRadioMeasurementReport(prAdapter);
		wmmRemoveAllTsmMeasurement(prAdapter, FALSE);
		rlmCancelRadioMeasurement(prAdapter);
	}
	prRmReqParam->fgRmIsOngoing = TRUE;
	/* Step1: Save Measurement Request Params */
	prRmReqParam->u2ReqIeBufLen = prRmReqParam->u2RemainReqLen =
		prSwRfb->u2PacketLen - OFFSET_OF(ACTION_RM_REQ_FRAME, aucInfoElem);
	if (prRmReqParam->u2RemainReqLen <= sizeof(IE_MEASUREMENT_REQ_T)) {
		DBGLOG(RLM, ERROR, "empty Radio Measurement Request Frame, Elem Len %d\n",
			prRmReqParam->u2RemainReqLen);
		return;
	}
	WLAN_GET_FIELD_BE16(&prRmReqFrame->u2Repetitions, &prRmReqParam->u2Repetitions);
	prRmReqParam->pucReqIeBuf = kalMemAlloc(prRmReqParam->u2RemainReqLen, VIR_MEM_TYPE);
	if (!prRmReqParam->pucReqIeBuf) {
		DBGLOG(RLM, ERROR, "Alloc %d bytes Req IE Buffer failed, No Memory\n", prRmReqParam->u2RemainReqLen);
		return;
	}
	kalMemCopy(prRmReqParam->pucReqIeBuf, &prRmReqFrame->aucInfoElem[0], prRmReqParam->u2RemainReqLen);
	prRmReqParam->prCurrMeasElem = (P_IE_MEASUREMENT_REQ_T)prRmReqParam->pucReqIeBuf;
	prRmReqParam->fgInitialLoop = TRUE;

	/* Step2: Prepare Report Frame and fill in Frame Header */
	prRmRepParam->pucReportFrameBuff = kalMemAlloc(RM_REPORT_FRAME_MAX_LENGTH, VIR_MEM_TYPE);
	if (!prRmRepParam->pucReportFrameBuff) {
		DBGLOG(RLM, ERROR, "Alloc Memory for Measurement Report Frame buffer failed\n");
		return;
	}
	kalMemZero(prRmRepParam->pucReportFrameBuff, RM_REPORT_FRAME_MAX_LENGTH);
	prReportFrame = (P_ACTION_RM_REPORT_FRAME)prRmRepParam->pucReportFrameBuff;
	prReportFrame->u2FrameCtrl = MAC_FRAME_ACTION;
	COPY_MAC_ADDR(prReportFrame->aucDestAddr, prRmReqFrame->aucSrcAddr);
	COPY_MAC_ADDR(prReportFrame->aucSrcAddr,
		prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX].aucOwnMacAddr);
	COPY_MAC_ADDR(prReportFrame->aucBSSID, prRmReqFrame->aucBSSID);
	prReportFrame->ucCategory = CATEGORY_RM_ACTION;
	prReportFrame->ucAction = RM_ACTION_RM_REPORT;
	prReportFrame->ucDialogToken = prRmReqFrame->ucDialogToken;
	prRmRepParam->u2ReportFrameLen = OFFSET_OF(ACTION_RM_REPORT_FRAME, aucInfoElem);
	rlmCalibrateRepetions(prRmReqParam);
	/* Step3: Start to process Measurement Request Element */
	rlmStartNextMeasurement(prAdapter, TRUE);
}

/*VOID rlmProcessLinkMeasurementRequest(P_ADAPTER_T prAdapter, P_WLAN_ACTION_FRAME prAction)
*{
*	struct ACTION_LM_REQUEST_FRAME prLmRequest = (struct ACTION_LM_REQUEST_FRAME *)prAction;
*
*	ASSERT(prAdapter);
*	ASSERT(prLmRequest);
*	DBGLOG(RLM, INFO, "LM Request From %pM, DialogToken %d\n",
*			prLmRequest->aucSrcAddr, prLmRequest->ucDialogToken);
*}
*/

VOID rlmProcessNeighborReportResonse(
	P_ADAPTER_T prAdapter, P_WLAN_ACTION_FRAME prAction, UINT_16 u2PacketLen)
{
	struct ACTION_NEIGHBOR_REPORT_FRAME *prNeighborResponse =
			(struct ACTION_NEIGHBOR_REPORT_FRAME *)prAction;

	ASSERT(prAdapter);
	ASSERT(prNeighborResponse);
	DBGLOG(RLM, INFO, "Neighbor Resp From %pM, DialogToken %d\n",
			prNeighborResponse->aucSrcAddr, prNeighborResponse->ucDialogToken);
	aisCollectNeighborAPChannel(prAdapter,
		(struct IE_NEIGHBOR_REPORT_T *)&prNeighborResponse->aucInfoElem[0],
		u2PacketLen - OFFSET_OF(struct ACTION_NEIGHBOR_REPORT_FRAME, aucInfoElem));
}

VOID rlmTxNeighborReportRequest(P_ADAPTER_T prAdapter, P_STA_RECORD_T prStaRec,
		struct SUB_ELEMENT_LIST *prSubIEs)
{
	static UINT_8 ucDialogToken = 1;
	P_MSDU_INFO_T prMsduInfo = NULL;
	P_BSS_INFO_T prBssInfo = NULL;
	PUINT_8 pucPayload = NULL;
	struct ACTION_NEIGHBOR_REPORT_FRAME *prTxFrame = NULL;
	UINT_16 u2TxFrameLen = 500;
	UINT_16 u2FrameLen = 0;

	prBssInfo = &prAdapter->rWifiVar.arBssInfo[prStaRec->ucNetTypeIndex];
	ASSERT(prBssInfo);
	/* 1 Allocate MSDU Info */
	prMsduInfo = (P_MSDU_INFO_T) cnmMgtPktAlloc(prAdapter, MAC_TX_RESERVED_FIELD + u2TxFrameLen);
	if (!prMsduInfo)
		return;
	prTxFrame = (struct ACTION_NEIGHBOR_REPORT_FRAME *)
	    ((ULONG) (prMsduInfo->prPacket) + MAC_TX_RESERVED_FIELD);

	/* 2 Compose The Mac Header. */
	prTxFrame->u2FrameCtrl = MAC_FRAME_ACTION;
	COPY_MAC_ADDR(prTxFrame->aucDestAddr, prStaRec->aucMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucSrcAddr, prBssInfo->aucOwnMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucBSSID, prBssInfo->aucBSSID);
	prTxFrame->ucCategory = CATEGORY_RM_ACTION;
	prTxFrame->ucAction = RM_ACTION_NEIGHBOR_REQUEST;
	u2FrameLen = OFFSET_OF(struct ACTION_NEIGHBOR_REPORT_FRAME, aucInfoElem);
	/* 3 Compose the frame body's frame. */
	prTxFrame->ucDialogToken = ucDialogToken++;
	u2TxFrameLen -= sizeof(*prTxFrame) - 1;
	pucPayload = &prTxFrame->aucInfoElem[0];
	while (prSubIEs && u2TxFrameLen >= (prSubIEs->rSubIE.ucLength + 2)) {
		kalMemCopy(pucPayload, &prSubIEs->rSubIE, prSubIEs->rSubIE.ucLength + 2);
		pucPayload += prSubIEs->rSubIE.ucLength + 2;
		u2FrameLen += prSubIEs->rSubIE.ucLength + 2;
		prSubIEs = prSubIEs->prNext;
	}

	/* 4 Update information of MSDU_INFO_T */
	prMsduInfo->ucPacketType = HIF_TX_PACKET_TYPE_MGMT;	/* Management frame */
	prMsduInfo->ucStaRecIndex = prStaRec->ucIndex;
	prMsduInfo->ucNetworkType = prStaRec->ucNetTypeIndex;
	prMsduInfo->ucMacHeaderLength = WLAN_MAC_MGMT_HEADER_LEN;
	prMsduInfo->fgIs802_1x = FALSE;
	prMsduInfo->fgIs802_11 = TRUE;
	prMsduInfo->u2FrameLength = u2FrameLen;
	prMsduInfo->ucTxSeqNum = nicIncreaseTxSeqNum(prAdapter);
	prMsduInfo->pfTxDoneHandler = NULL;
	prMsduInfo->fgIsBasicRate = FALSE;

	/* 5 Enqueue the frame to send this action frame. */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);
}

VOID rlmTxRadioMeasurementReport(P_ADAPTER_T prAdapter)
{
	P_MSDU_INFO_T prMsduInfo = NULL;
	struct RADIO_MEASUREMENT_REPORT_PARAMS *prRmRepParam = &prAdapter->rWifiVar.rRmRepParams;

	if (prRmRepParam->u2ReportFrameLen <= OFFSET_OF(ACTION_RM_REPORT_FRAME, aucInfoElem)) {
		DBGLOG(RLM, INFO, "report frame length is too short, %d\n", prRmRepParam->u2ReportFrameLen);
		return;
	}

	if (prAdapter->rWifiVar.rAisFsmInfo.prTargetStaRec == NULL) {
		DBGLOG(RLM, INFO, "prAdapter->rWifiVar.rAisFsmInfo.prTargetStaRec is NULL\n");
		return;
	}

	prMsduInfo = (P_MSDU_INFO_T) cnmMgtPktAlloc(prAdapter, prRmRepParam->u2ReportFrameLen);
	if (!prMsduInfo) {
		DBGLOG(RLM, INFO, "Alloc MSDU Info failed, frame length %d\n", prRmRepParam->u2ReportFrameLen);
		return;
	}
	DBGLOG(RLM, INFO, "frame length %d\n", prRmRepParam->u2ReportFrameLen);
	kalMemCopy(prMsduInfo->prPacket, prRmRepParam->pucReportFrameBuff, prRmRepParam->u2ReportFrameLen);

	/* 2 Update information of MSDU_INFO_T */
	prMsduInfo->ucPacketType = HIF_TX_PACKET_TYPE_MGMT;	/* Management frame */
	prMsduInfo->ucStaRecIndex = prAdapter->rWifiVar.rAisFsmInfo.prTargetStaRec->ucIndex;
	prMsduInfo->ucNetworkType = NETWORK_TYPE_AIS_INDEX;
	prMsduInfo->ucMacHeaderLength = WLAN_MAC_MGMT_HEADER_LEN;
	prMsduInfo->fgIs802_1x = FALSE;
	prMsduInfo->fgIs802_11 = TRUE;
	prMsduInfo->u2FrameLength = prRmRepParam->u2ReportFrameLen;
	prMsduInfo->ucTxSeqNum = nicIncreaseTxSeqNum(prAdapter);
	prMsduInfo->pfTxDoneHandler = NULL;
	prMsduInfo->fgIsBasicRate = FALSE;
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);

	/* reset u2ReportFrameLen after tx frame */
	prRmRepParam->u2ReportFrameLen = OFFSET_OF(ACTION_RM_REPORT_FRAME, aucInfoElem);
}



VOID rlmGernerateRRMEnabledCapIE(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo)
{
	P_IE_RRM_ENABLED_CAP_T prRrmEnabledCap = NULL;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

	prRrmEnabledCap = (P_IE_RRM_ENABLED_CAP_T)
	    (((PUINT_8) prMsduInfo->prPacket) + prMsduInfo->u2FrameLength);
	prRrmEnabledCap->ucId = ELEM_ID_RRM_ENABLED_CAP;
	prRrmEnabledCap->ucLength = ELEM_MAX_LEN_RRM_CAP;
	kalMemZero(&prRrmEnabledCap->aucCap[0], ELEM_MAX_LEN_RRM_CAP);
	rlmFillRrmCapa(&prRrmEnabledCap->aucCap[0]);
	prMsduInfo->u2FrameLength += IE_SIZE(prRrmEnabledCap);
}

VOID rlmFillRrmCapa(PUINT_8 pucCapa)
{
	UINT_8 ucIndex = 0;
	UINT_8 aucEnabledBits[] = {RRM_CAP_INFO_LINK_MEASURE_BIT, RRM_CAP_INFO_NEIGHBOR_REPORT_BIT,
			RRM_CAP_INFO_BEACON_PASSIVE_MEASURE_BIT, RRM_CAP_INFO_BEACON_ACTIVE_MEASURE_BIT,
			RRM_CAP_INFO_BEACON_TABLE_BIT, RRM_CAP_INFO_TSM_BIT, RRM_CAP_INFO_RRM_BIT};

	for (; ucIndex < sizeof(aucEnabledBits); ucIndex++)
		SET_EXT_CAP(pucCapa, ELEM_MAX_LEN_RRM_CAP, aucEnabledBits[ucIndex]);
}

VOID rlmGerneratePowerCapIE(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo)
{
	P_IE_POWER_CAP_T prPwrCap = NULL;
	UINT_8 ucChannel = 0;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

	ucChannel = prAdapter->rWifiVar.rAisFsmInfo.prTargetBssDesc->ucChannelNum;
	prPwrCap = (P_IE_POWER_CAP_T)
	    (((PUINT_8) prMsduInfo->prPacket) + prMsduInfo->u2FrameLength);
	prPwrCap->ucId = ELEM_ID_PWR_CAP;
	prPwrCap->ucLength = 2;
	prPwrCap->cMaxTxPowerCap = RLM_MAX_TX_PWR;
	prPwrCap->cMinTxPowerCap = RLM_MIN_TX_PWR;
	prMsduInfo->u2FrameLength += IE_SIZE(prPwrCap);
}

VOID rlmSetMaxTxPwrLimit(IN P_ADAPTER_T prAdapter, INT_8 cLimit, UINT_8 ucEnable)
{
	struct CMD_SET_MAX_TXPWR_LIMIT rTxPwrLimit;

	kalMemZero(&rTxPwrLimit, sizeof(rTxPwrLimit));
	rTxPwrLimit.ucMaxTxPwrLimitEnable = ucEnable;
	if (ucEnable) {
		if (cLimit > RLM_MAX_TX_PWR) {
			DBGLOG(RLM, TRACE, "Target MaxPwr %d Higher than Capability, reset to capability\n", cLimit);
			cLimit = RLM_MAX_TX_PWR;
		}
		if (cLimit < RLM_MIN_TX_PWR) {
			DBGLOG(RLM, TRACE, "Target MinPwr %d Lower than Capability, reset to capability\n", cLimit);
			cLimit = RLM_MIN_TX_PWR;
		}
		DBGLOG(RLM, TRACE, "Set Max Tx Power Limit %d, Min Limit %d\n", cLimit, RLM_MIN_TX_PWR);
		rTxPwrLimit.cMaxTxPwr = cLimit * 2; /* unit of cMaxTxPwr is 0.5 dBm */
		rTxPwrLimit.cMinTxPwr = RLM_MIN_TX_PWR * 2;
	} else
		DBGLOG(RLM, TRACE, "Disable Tx Power Limit\n");

	wlanSendSetQueryCmd(prAdapter,
					  CMD_ID_SET_MAX_TXPWR_LIMIT,
					  TRUE,
					  FALSE,
					  FALSE,
					  nicCmdEventSetCommon,
					  nicOidCmdTimeoutCommon,
					  sizeof(struct CMD_SET_MAX_TXPWR_LIMIT),
					  (PUINT_8) &rTxPwrLimit, NULL, 0);
}

enum RM_REQ_PRIORITY rlmGetRmRequestPriority(PUINT_8 pucDestAddr)
{
	if (IS_UCAST_MAC_ADDR(pucDestAddr))
		return RM_PRI_UNICAST;
	else if (EQUAL_MAC_ADDR(pucDestAddr, "\xff\xff\xff\xff\xff\xff"))
		return RM_PRI_BROADCAST;
	return RM_PRI_MULTICAST;
}

static VOID rlmCalibrateRepetions(struct RADIO_MEASUREMENT_REQ_PARAMS *prRmReq)
{
	UINT_16 u2IeSize = 0;
	UINT_16 u2RemainReqLen = prRmReq->u2ReqIeBufLen;
	P_IE_MEASUREMENT_REQ_T prCurrReq = (P_IE_MEASUREMENT_REQ_T)prRmReq->prCurrMeasElem;

	if (prRmReq->u2Repetitions == 0)
		return;

	u2IeSize = IE_SIZE(prCurrReq);
	while (u2RemainReqLen >= u2IeSize) {
		/* 1. If all measurement request has enable bit, no need to repeat
		** see 11.10.6 Measurement request elements with the enable bit set to 1 shall be processed once
		** regardless of the value in the number of repetitions in the measurement request.
		** 2. Due to we don't support parallel measurement, if all request has parallel bit, no need to repeat
		** measurement, to avoid frequent composing incapable response IE and exhauste CPU resource
		** and then cause watch dog timeout.
		** 3. if all measurements are not supported, no need to repeat. currently we only support Beacon request
		** on this chip.
		*/
		if (!(prCurrReq->ucRequestMode & (RM_REQ_MODE_ENABLE_BIT | RM_REQ_MODE_PARALLEL_BIT))) {
			if (prCurrReq->ucMeasurementType == ELEM_RM_TYPE_BEACON_REQ)
			return;
		}
		u2RemainReqLen -= u2IeSize;
		prCurrReq = (P_IE_MEASUREMENT_REQ_T)((PUINT_8)prCurrReq + u2IeSize);
		u2IeSize = IE_SIZE(prCurrReq);
	}
	DBGLOG(RLM, INFO,
		"All Measurement has set enable bit, or all are parallel or not supported, don't repeat\n");
	prRmReq->u2Repetitions = 0;
}

VOID rlmRunEventProcessNextRm(P_ADAPTER_T prAdapter, P_MSG_HDR_T prMsgHdr)
{
	cnmMemFree(prAdapter, prMsgHdr);
	rlmStartNextMeasurement(prAdapter, FALSE);
}

VOID rlmScheduleNextRm(P_ADAPTER_T prAdapter)
{
	P_MSG_HDR_T prMsg = NULL;

	prMsg = cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(*prMsg));

	if (!prMsg) {
		DBGLOG(RLM, WARN, "No buf for schedule next rm\n");
		return;
	}
	prMsg->eMsgId = MID_RLM_RM_SCHEDULE;
	mboxSendMsg(prAdapter, MBOX_ID_0, prMsg, MSG_SEND_METHOD_BUF);
}

#if CFG_SUPPORT_RLM_ACT_NETWORK
VOID rlmActivateNetwork(P_ADAPTER_T prAdapter, ENUM_NETWORK_TYPE_INDEX_T eNetworkTypeIdx,
			ENUM_NET_ACTIVE_SRC_T eNetActiveSrcIdx)
{
	bool isActive = false;

	ASSERT(prAdapter);

	if (eNetworkTypeIdx < NETWORK_TYPE_AIS_INDEX ||
		eNetworkTypeIdx >= NETWORK_TYPE_INDEX_NUM) {
		DBGLOG(NIC, WARN, "rlmActivateNetwork, Idx=%d\n",
		eNetworkTypeIdx);
		return;
	}

	prAdapter->rWifiVar.arBssInfo[eNetworkTypeIdx].ucNetActiveSrc |= eNetActiveSrcIdx;

	isActive = IS_NET_ACTIVE(prAdapter, eNetworkTypeIdx);
	if (!isActive) {
		SET_NET_ACTIVE(prAdapter, eNetworkTypeIdx);
		/* sync with firmware */
		nicActivateNetwork(prAdapter, eNetworkTypeIdx);
		if (eNetworkTypeIdx == NETWORK_TYPE_P2P_INDEX)
			nicUpdateBss(prAdapter, eNetworkTypeIdx);
	}
	DBGLOG(RLM, INFO, "rlm: active=%d, Type=%d, Src=%d, SrcVal=%d\n",
		isActive, eNetworkTypeIdx, eNetActiveSrcIdx,
		prAdapter->rWifiVar.arBssInfo[eNetworkTypeIdx].ucNetActiveSrc);
}

VOID rlmDeactivateNetwork(P_ADAPTER_T prAdapter, ENUM_NETWORK_TYPE_INDEX_T eNetworkTypeIdx,
			ENUM_NET_ACTIVE_SRC_T eNetActiveSrcIdx)
{
	ASSERT(prAdapter);

	if (eNetworkTypeIdx < NETWORK_TYPE_AIS_INDEX ||
		eNetworkTypeIdx >= NETWORK_TYPE_INDEX_NUM) {
		DBGLOG(NIC, WARN, "rlmDeactivateNetwork, Idx=%d\n",
		eNetworkTypeIdx);
		return;
	}

	prAdapter->rWifiVar.arBssInfo[eNetworkTypeIdx].ucNetActiveSrc &= ~eNetActiveSrcIdx;

	DBGLOG(RLM, INFO, "rlmDeactivateNetwork, Type = %d, Src = %d, Active = %d, SrcVal= %d\n",
		eNetworkTypeIdx, eNetActiveSrcIdx, IS_NET_ACTIVE(prAdapter, eNetworkTypeIdx),
		prAdapter->rWifiVar.arBssInfo[eNetworkTypeIdx].ucNetActiveSrc);

	if ((IS_NET_ACTIVE(prAdapter, eNetworkTypeIdx)) &&
		prAdapter->rWifiVar.arBssInfo[eNetworkTypeIdx].ucNetActiveSrc == 0) {
		UNSET_NET_ACTIVE(prAdapter, eNetworkTypeIdx);
		/* sync with firmware */
		nicDeactivateNetwork(prAdapter, eNetworkTypeIdx);
	}
}

#endif


