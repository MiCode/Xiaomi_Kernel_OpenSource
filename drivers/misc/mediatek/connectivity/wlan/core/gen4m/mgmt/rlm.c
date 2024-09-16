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

/* Retry limit of sending operation notification frame */
#define OPERATION_NOTICATION_TX_LIMIT	2

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */
enum ENUM_OP_NOTIFY_STATE_T {
	OP_NOTIFY_STATE_KEEP = 0, /* Won't change OP mode */
	OP_NOTIFY_STATE_SENDING,  /* Sending OP notification frame */
	OP_NOTIFY_STATE_SUCCESS,  /* OP notification Tx success */
	OP_NOTIFY_STATE_FAIL,     /* OP notification Tx fail(over retry limit)*/
	OP_NOTIFY_STATE_NUM
};

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */
/*
** Should Not Force to BW 20 after Channel Switch.
** Enable for DFS Certification
*/
#ifdef CFG_DFS_CHSW_FORCE_BW20
u_int8_t g_fgHasChannelSwitchIE = FALSE;
#endif
u_int8_t g_fgHasStopTx = FALSE;

#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST
struct RLM_CAL_RESULT_ALL_V2 g_rBackupCalDataAllV2;
#endif

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
static void rlmFillHtCapIE(struct ADAPTER *prAdapter,
			   struct BSS_INFO *prBssInfo,
			   struct MSDU_INFO *prMsduInfo);

static void rlmFillExtCapIE(struct ADAPTER *prAdapter,
			    struct BSS_INFO *prBssInfo,
			    struct MSDU_INFO *prMsduInfo);

static void rlmFillHtOpIE(struct ADAPTER *prAdapter, struct BSS_INFO *prBssInfo,
			  struct MSDU_INFO *prMsduInfo);

static uint8_t rlmRecIeInfoForClient(struct ADAPTER *prAdapter,
				     struct BSS_INFO *prBssInfo, uint8_t *pucIE,
				     uint16_t u2IELength);

static u_int8_t rlmRecBcnFromNeighborForClient(struct ADAPTER *prAdapter,
					       struct BSS_INFO *prBssInfo,
					       struct SW_RFB *prSwRfb,
					       uint8_t *pucIE,
					       uint16_t u2IELength);

static u_int8_t rlmRecBcnInfoForClient(struct ADAPTER *prAdapter,
				       struct BSS_INFO *prBssInfo,
				       struct SW_RFB *prSwRfb, uint8_t *pucIE,
				       uint16_t u2IELength);

static void rlmBssReset(struct ADAPTER *prAdapter, struct BSS_INFO *prBssInfo);

#if CFG_SUPPORT_802_11AC
static void rlmFillVhtCapIE(struct ADAPTER *prAdapter,
			    struct BSS_INFO *prBssInfo,
			    struct MSDU_INFO *prMsduInfo);
static void rlmFillVhtOpNotificationIE(struct ADAPTER *prAdapter,
				       struct BSS_INFO *prBssInfo,
				       struct MSDU_INFO *prMsduInfo,
				       u_int8_t fgIsMaxCap);

#endif

/* Operating BW/Nss change and notification */
static void rlmOpModeTxDoneHandler(struct ADAPTER *prAdapter,
				   struct MSDU_INFO *prMsduInfo,
				   uint8_t ucOpChangeType,
				   u_int8_t fgIsSuccess);
static void rlmChangeOwnOpInfo(struct ADAPTER *prAdapter,
			       struct BSS_INFO *prBssInfo);
static void rlmCompleteOpModeChange(struct ADAPTER *prAdapter,
				    struct BSS_INFO *prBssInfo,
				    u_int8_t fgIsSuccess);
static void rlmRollbackOpChangeParam(struct BSS_INFO *prBssInfo,
				     u_int8_t fgIsRollbackBw,
				     u_int8_t fgIsRollbackNss);
static uint8_t rlmGetOpModeBwByVhtAndHtOpInfo(struct BSS_INFO *prBssInfo);
static u_int8_t rlmCheckOpChangeParamValid(struct ADAPTER *prAdapter,
					   struct BSS_INFO *prBssInfo,
					   uint8_t ucChannelWidth,
					   uint8_t ucOpRxNss,
					   uint8_t ucOpTxNss);
static void rlmRecOpModeBwForClient(uint8_t ucVhtOpModeChannelWidth,
				    struct BSS_INFO *prBssInfo);

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
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
void rlmFsmEventInit(struct ADAPTER *prAdapter)
{
	ASSERT(prAdapter);

	/* Note: assume struct TIMER structures are reset to zero or stopped
	 * before invoking this function.
	 */

	/* Initialize OBSS FSM */
	rlmObssInit(prAdapter);

#if CFG_SUPPORT_PWR_LIMIT_COUNTRY
	rlmDomainCheckCountryPowerLimitTable(prAdapter);
#endif

#ifdef CFG_DFS_CHSW_FORCE_BW20
	g_fgHasChannelSwitchIE = FALSE;
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
void rlmFsmEventUninit(struct ADAPTER *prAdapter)
{
	struct BSS_INFO *prBssInfo;
	uint8_t i;

	ASSERT(prAdapter);

	for (i = 0; i < prAdapter->ucHwBssIdNum; i++) {
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
void rlmReqGeneratePowerCapIE(struct ADAPTER *prAdapter,
			      struct MSDU_INFO *prMsduInfo)
{
	uint8_t *pucBuffer;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

	pucBuffer =
		(uint8_t *)(prMsduInfo->prPacket + prMsduInfo->u2FrameLength);

	POWER_CAP_IE(pucBuffer)->ucId = ELEM_ID_PWR_CAP;
	POWER_CAP_IE(pucBuffer)->ucLength = ELEM_MAX_LEN_POWER_CAP;
	POWER_CAP_IE(pucBuffer)->cMinTxPowerCap = RLM_MIN_TX_PWR;
	POWER_CAP_IE(pucBuffer)->cMaxTxPowerCap = RLM_MAX_TX_PWR;

	prMsduInfo->u2FrameLength += IE_SIZE(pucBuffer);
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
void rlmReqGenerateSupportedChIE(struct ADAPTER *prAdapter,
				 struct MSDU_INFO *prMsduInfo)
{
	uint8_t *pucBuffer;
	struct BSS_INFO *prBssInfo;
	struct RF_CHANNEL_INFO auc2gChannelList[MAX_2G_BAND_CHN_NUM];
	struct RF_CHANNEL_INFO auc5gChannelList[MAX_5G_BAND_CHN_NUM];
	uint8_t ucNumOf2gChannel = 0;
	uint8_t ucNumOf5gChannel = 0;
	uint8_t ucChIdx = 0;
	uint8_t ucIdx = 0;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prMsduInfo->ucBssIndex);
	kalMemZero(&auc2gChannelList[0], sizeof(auc2gChannelList));
	kalMemZero(&auc5gChannelList[0], sizeof(auc5gChannelList));

	/* We should add supported channels IE in assoc/reassoc request
	 * if the spectrum management bit is set to 1 in Capability Info
	 * field, or the connection will be rejected by Marvell APs in
	 * some TGn items. (e.g. 5.2.3). Spectrum management related
	 * feature (802.11h) is for 5G band.
	 */
	if (!prBssInfo || prBssInfo->eBand != BAND_5G)
		return;

	pucBuffer =
		(uint8_t *)(prMsduInfo->prPacket + prMsduInfo->u2FrameLength);

	rlmDomainGetChnlList(prAdapter, BAND_2G4, TRUE, MAX_2G_BAND_CHN_NUM,
			     &ucNumOf2gChannel, auc2gChannelList);
	rlmDomainGetChnlList(prAdapter, BAND_5G, TRUE, MAX_5G_BAND_CHN_NUM,
			     &ucNumOf5gChannel, auc5gChannelList);

	SUP_CH_IE(pucBuffer)->ucId = ELEM_ID_SUP_CHS;
	SUP_CH_IE(pucBuffer)->ucLength =
		(ucNumOf2gChannel + ucNumOf5gChannel) * 2;

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
void rlmReqGenerateHtCapIE(struct ADAPTER *prAdapter,
			   struct MSDU_INFO *prMsduInfo)
{
	struct BSS_INFO *prBssInfo;
	struct STA_RECORD *prStaRec;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

	prBssInfo = prAdapter->aprBssInfo[prMsduInfo->ucBssIndex];
	if (!prBssInfo)
		return;

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

	if ((prAdapter->rWifiVar.ucAvailablePhyTypeSet &
	     PHY_TYPE_SET_802_11N) &&
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
void rlmReqGenerateExtCapIE(struct ADAPTER *prAdapter,
			    struct MSDU_INFO *prMsduInfo)
{
	struct BSS_INFO *prBssInfo;
	struct STA_RECORD *prStaRec;
#if CFG_SUPPORT_PASSPOINT
	struct HS20_INFO *prHS20Info;
#endif

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

	prBssInfo = prAdapter->aprBssInfo[prMsduInfo->ucBssIndex];
	if (!prBssInfo)
		return;

#if CFG_SUPPORT_PASSPOINT
	prHS20Info = aisGetHS20Info(prAdapter,
		prBssInfo->ucBssIndex);
	if (!prHS20Info)
		return;
#endif

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

	if ((prAdapter->rWifiVar.ucAvailablePhyTypeSet &
	     PHY_TYPE_SET_802_11N) &&
	    (!prStaRec || (prStaRec->ucPhyTypeSet & PHY_TYPE_SET_802_11N)))
		rlmFillExtCapIE(prAdapter, prBssInfo, prMsduInfo);
#if CFG_SUPPORT_PASSPOINT
	else if (prHS20Info->fgConnectHS20AP == TRUE)
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
void rlmRspGenerateHtCapIE(struct ADAPTER *prAdapter,
			   struct MSDU_INFO *prMsduInfo)
{
	struct BSS_INFO *prBssInfo;
	struct STA_RECORD *prStaRec;
	uint8_t ucPhyTypeSet;

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

	if (RLM_NET_IS_11N(prBssInfo) &&
	    (ucPhyTypeSet & PHY_TYPE_SET_802_11N) &&
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
void rlmRspGenerateExtCapIE(struct ADAPTER *prAdapter,
			    struct MSDU_INFO *prMsduInfo)
{
	struct BSS_INFO *prBssInfo;
	struct STA_RECORD *prStaRec;
	uint8_t ucPhyTypeSet;

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
void rlmRspGenerateHtOpIE(struct ADAPTER *prAdapter,
			  struct MSDU_INFO *prMsduInfo)
{
	struct BSS_INFO *prBssInfo;
	struct STA_RECORD *prStaRec;
	uint8_t ucPhyTypeSet;

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

	if (RLM_NET_IS_11N(prBssInfo) &&
	    (ucPhyTypeSet & PHY_TYPE_SET_802_11N) &&
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
void rlmRspGenerateErpIE(struct ADAPTER *prAdapter,
			 struct MSDU_INFO *prMsduInfo)
{
	struct BSS_INFO *prBssInfo;
	struct STA_RECORD *prStaRec;
	struct IE_ERP *prErpIe;
	uint8_t ucPhyTypeSet;

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

	if (RLM_NET_IS_11GN(prBssInfo) && prBssInfo->eBand == BAND_2G4 &&
	    (ucPhyTypeSet & PHY_TYPE_SET_802_11GN)) {
		prErpIe = (struct IE_ERP *)(((uint8_t *)prMsduInfo->prPacket) +
					    prMsduInfo->u2FrameLength);

		/* Add ERP IE */
		prErpIe->ucId = ELEM_ID_ERP_INFO;
		prErpIe->ucLength = 1;

		prErpIe->ucERP = prBssInfo->fgObssErpProtectMode
					 ? ERP_INFO_USE_PROTECTION
					 : 0;

		if (prBssInfo->fgErpProtectMode)
			prErpIe->ucERP |= (ERP_INFO_NON_ERP_PRESENT |
					   ERP_INFO_USE_PROTECTION);

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
void rlmGenerateMTKOuiIE(struct ADAPTER *prAdapter,
			 struct MSDU_INFO *prMsduInfo)
{
	struct BSS_INFO *prBssInfo;
	uint8_t *pucBuffer;
	uint8_t aucMtkOui[] = VENDOR_OUI_MTK;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

	if (prAdapter->rWifiVar.ucMtkOui == FEATURE_DISABLED)
		return;

	prBssInfo = prAdapter->aprBssInfo[prMsduInfo->ucBssIndex];
	if (!prBssInfo)
		return;

	pucBuffer = (uint8_t *)((unsigned long)prMsduInfo->prPacket +
				(unsigned long)prMsduInfo->u2FrameLength);

	MTK_OUI_IE(pucBuffer)->ucId = ELEM_ID_VENDOR;
	MTK_OUI_IE(pucBuffer)->ucLength = ELEM_MIN_LEN_MTK_OUI;
	MTK_OUI_IE(pucBuffer)->aucOui[0] = aucMtkOui[0];
	MTK_OUI_IE(pucBuffer)->aucOui[1] = aucMtkOui[1];
	MTK_OUI_IE(pucBuffer)->aucOui[2] = aucMtkOui[2];
	MTK_OUI_IE(pucBuffer)->aucCapability[0] =
		MTK_SYNERGY_CAP0 & (prAdapter->rWifiVar.aucMtkFeature[0]);
	MTK_OUI_IE(pucBuffer)->aucCapability[1] =
		MTK_SYNERGY_CAP1 & (prAdapter->rWifiVar.aucMtkFeature[1]);
	MTK_OUI_IE(pucBuffer)->aucCapability[2] =
		MTK_SYNERGY_CAP2 & (prAdapter->rWifiVar.aucMtkFeature[2]);
	MTK_OUI_IE(pucBuffer)->aucCapability[3] =
		MTK_SYNERGY_CAP3 & (prAdapter->rWifiVar.aucMtkFeature[3]);

	/* Disable the 2.4G 256QAM feature bit if chip doesn't support AC*/
	if (prAdapter->rWifiVar.ucHwNotSupportAC) {
		MTK_OUI_IE(pucBuffer)->aucCapability[0] &=
			~(MTK_SYNERGY_CAP_SUPPORT_24G_MCS89);
		DBGLOG(INIT, WARN,
		       "Disable 2.4G 256QAM support if N only chip\n");
	}

	prMsduInfo->u2FrameLength += IE_SIZE(pucBuffer);
	pucBuffer += IE_SIZE(pucBuffer);
} /* rlmGenerateMTKOuiIE */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is used to check MTK Vendor Specific OUI
 *
 *
 * @return true:  correct MTK OUI
 *             false: incorrect MTK OUI
 */
/*----------------------------------------------------------------------------*/
u_int8_t rlmParseCheckMTKOuiIE(IN struct ADAPTER *prAdapter, IN uint8_t *pucBuf,
			       IN uint32_t *pu4Cap)
{
	uint8_t aucMtkOui[] = VENDOR_OUI_MTK;
	struct IE_MTK_OUI *prMtkOuiIE = (struct IE_MTK_OUI *)NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (pucBuf != NULL));

		prMtkOuiIE = (struct IE_MTK_OUI *)pucBuf;

		if (prAdapter->rWifiVar.ucMtkOui == FEATURE_DISABLED)
			break;
		else if (IE_LEN(pucBuf) < ELEM_MIN_LEN_MTK_OUI)
			break;
		else if (prMtkOuiIE->aucOui[0] != aucMtkOui[0] ||
			 prMtkOuiIE->aucOui[1] != aucMtkOui[1] ||
			 prMtkOuiIE->aucOui[2] != aucMtkOui[2])
			break;
		/* apply NvRam setting */
		prMtkOuiIE->aucCapability[0] =
			prMtkOuiIE->aucCapability[0] &
			(prAdapter->rWifiVar.aucMtkFeature[0]);
		prMtkOuiIE->aucCapability[1] =
			prMtkOuiIE->aucCapability[1] &
			(prAdapter->rWifiVar.aucMtkFeature[1]);
		prMtkOuiIE->aucCapability[2] =
			prMtkOuiIE->aucCapability[2] &
			(prAdapter->rWifiVar.aucMtkFeature[2]);
		prMtkOuiIE->aucCapability[3] =
			prMtkOuiIE->aucCapability[3] &
			(prAdapter->rWifiVar.aucMtkFeature[3]);

		/* Disable the 2.4G 256QAM feature bit if chip doesn't support
		 * AC. Otherwise, FW would choose wrong max rate of auto rate.
		 */
		if (prAdapter->rWifiVar.ucHwNotSupportAC) {
			prMtkOuiIE->aucCapability[0] &=
				~(MTK_SYNERGY_CAP_SUPPORT_24G_MCS89);
			DBGLOG(INIT, WARN,
			       "Disable 2.4G 256QAM support if N only chip\n");
		}

		kalMemCopy(pu4Cap, prMtkOuiIE->aucCapability, sizeof(uint32_t));

		return TRUE;
	} while (FALSE);

	return FALSE;
} /* rlmParseCheckMTKOuiIE */

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
void rlmGenerateCsaIE(struct ADAPTER *prAdapter, struct MSDU_INFO *prMsduInfo)
{
	uint8_t *pucBuffer;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

	if (prAdapter->rWifiVar.fgCsaInProgress) {

		pucBuffer =
			(uint8_t *)((unsigned long)prMsduInfo->prPacket +
				    (unsigned long)prMsduInfo->u2FrameLength);

		CSA_IE(pucBuffer)->ucId = ELEM_ID_CH_SW_ANNOUNCEMENT;
		CSA_IE(pucBuffer)->ucLength = ELEM_MIN_LEN_CSA;
		CSA_IE(pucBuffer)->ucChannelSwitchMode =
			prAdapter->rWifiVar.ucChannelSwitchMode;
		CSA_IE(pucBuffer)->ucNewChannelNum =
			prAdapter->rWifiVar.ucNewChannelNumber;
		CSA_IE(pucBuffer)->ucChannelSwitchCount =
			prAdapter->rWifiVar.ucChannelSwitchCount;

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
static void rlmFillHtCapIE(struct ADAPTER *prAdapter,
			   struct BSS_INFO *prBssInfo,
			   struct MSDU_INFO *prMsduInfo)
{
	struct IE_HT_CAP *prHtCap;
	struct SUP_MCS_SET_FIELD *prSupMcsSet;
	u_int8_t fg40mAllowed;
	uint8_t ucIdx;

	ASSERT(prAdapter);
	ASSERT(prBssInfo);
	ASSERT(prMsduInfo);

	fg40mAllowed = prBssInfo->fgAssoc40mBwAllowed;

	prHtCap = (struct IE_HT_CAP *)(((uint8_t *)prMsduInfo->prPacket) +
				       prMsduInfo->u2FrameLength);

	/* Add HT capabilities IE */
	prHtCap->ucId = ELEM_ID_HT_CAP;
	prHtCap->ucLength = sizeof(struct IE_HT_CAP) - ELEM_HDR_LEN;

	prHtCap->u2HtCapInfo = HT_CAP_INFO_DEFAULT_VAL;

	if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucRxShortGI))
		prHtCap->u2HtCapInfo |=
			(HT_CAP_INFO_SHORT_GI_20M | HT_CAP_INFO_SHORT_GI_40M);

	if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucRxLdpc))
		prHtCap->u2HtCapInfo |= HT_CAP_INFO_LDPC_CAP;

	if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucTxStbc) &&
			prBssInfo->ucOpTxNss >= 2)
		prHtCap->u2HtCapInfo |= HT_CAP_INFO_TX_STBC;

	if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucRxStbc)) {

		uint8_t tempRxStbcNss;

		tempRxStbcNss = prAdapter->rWifiVar.ucRxStbcNss;
		tempRxStbcNss =
			(tempRxStbcNss >
			 wlanGetSupportNss(prAdapter, prBssInfo->ucBssIndex))
				? wlanGetSupportNss(prAdapter,
						    prBssInfo->ucBssIndex)
				: (tempRxStbcNss);
		if (tempRxStbcNss != prAdapter->rWifiVar.ucRxStbcNss) {
			DBGLOG(RLM, WARN, "Apply Nss:%d as RxStbcNss in HT Cap",
			       wlanGetSupportNss(prAdapter,
						 prBssInfo->ucBssIndex));
			DBGLOG(RLM, WARN,
			       " due to set RxStbcNss more than Nss is not appropriate.\n");
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
					  HT_CAP_INFO_SHORT_GI_40M |
					  HT_CAP_INFO_DSSS_CCK_IN_40M);

	/* SM power saving (IEEE 802.11, 2016, 10.2.4)
	 * A non-AP HT STA may also use SM Power Save bits in the HT
	 * Capabilities element of its Association Request to achieve
	 * the same purpose. The latter allows the STA to use only a
	 * single receive chain immediately after association.
	 */
	if (prBssInfo->ucOpRxNss <
		wlanGetSupportNss(prAdapter, prBssInfo->ucBssIndex))
		prHtCap->u2HtCapInfo &=
			~HT_CAP_INFO_SM_POWER_SAVE; /*Set as static power save*/

	prHtCap->ucAmpduParam = AMPDU_PARAM_DEFAULT_VAL;

	prSupMcsSet = &prHtCap->rSupMcsSet;
	kalMemZero((void *)&prSupMcsSet->aucRxMcsBitmask[0],
		   SUP_MCS_RX_BITMASK_OCTET_NUM);

	for (ucIdx = 0;
	     ucIdx < wlanGetSupportNss(prAdapter, prBssInfo->ucBssIndex);
	     ucIdx++)
		prSupMcsSet->aucRxMcsBitmask[ucIdx] = BITS(0, 7);

	/* prSupMcsSet->aucRxMcsBitmask[0] = BITS(0, 7); */

	if (fg40mAllowed && IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucMCS32))
		prSupMcsSet->aucRxMcsBitmask[32 / 8] = BIT(0); /* MCS32 */
	prSupMcsSet->u2RxHighestSupportedRate = SUP_MCS_RX_DEFAULT_HIGHEST_RATE;
	prSupMcsSet->u4TxRateInfo = SUP_MCS_TX_DEFAULT_VAL;

	prHtCap->u2HtExtendedCap = HT_EXT_CAP_DEFAULT_VAL;
	if (!fg40mAllowed ||
	    prBssInfo->eCurrentOPMode != OP_MODE_INFRASTRUCTURE)
		prHtCap->u2HtExtendedCap &=
			~(HT_EXT_CAP_PCO | HT_EXT_CAP_PCO_TRANS_TIME_NONE);

	prHtCap->u4TxBeamformingCap = TX_BEAMFORMING_CAP_DEFAULT_VAL;
	if ((prAdapter->rWifiVar.eDbdcMode == ENUM_DBDC_MODE_DISABLED) ||
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
static void rlmFillExtCapIE(struct ADAPTER *prAdapter,
			    struct BSS_INFO *prBssInfo,
			    struct MSDU_INFO *prMsduInfo)
{
#if CFG_SUPPORT_PASSPOINT
	struct IE_HS20_EXT_CAP_T *prHsExtCap;
	struct HS20_INFO *prHS20Info;
#else
	struct IE_EXT_CAP *prExtCap;
#endif
	u_int8_t fg40mAllowed, fgAppendVhtCap;
	struct STA_RECORD *prStaRec;
	struct CONNECTION_SETTINGS *prConnSettings;
	const uint8_t *extCapConn;
	uint32_t extCapIeLen = 0;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

	fg40mAllowed = prBssInfo->fgAssoc40mBwAllowed;
	prConnSettings = aisGetConnSettings(prAdapter, prBssInfo->ucBssIndex);
	extCapConn = kalFindIeMatchMask(ELEM_ID_EXTENDED_CAP,
				       prConnSettings->pucAssocIEs,
				       prConnSettings->assocIeLen,
				       NULL, 0, 0, NULL);
	if (extCapConn) {
		extCapIeLen = ELEM_HDR_LEN + RSN_IE(extCapConn)->ucLength;
		DBGLOG_MEM8(SAA, INFO, extCapConn, extCapIeLen);
	}

#if CFG_SUPPORT_PASSPOINT
	prHS20Info = aisGetHS20Info(prAdapter,
			prBssInfo->ucBssIndex);
	if (!prHS20Info)
		return;

	prHsExtCap =
		(struct IE_HS20_EXT_CAP_T *)(((uint8_t *)prMsduInfo->prPacket) +
					     prMsduInfo->u2FrameLength);
	prHsExtCap->ucId = ELEM_ID_EXTENDED_CAP;

	if (prHS20Info->fgConnectHS20AP == TRUE)
		prHsExtCap->ucLength = ELEM_MAX_LEN_EXT_CAP;
	else
		prHsExtCap->ucLength = 3 - ELEM_HDR_LEN;

	kalMemZero(prHsExtCap->aucCapabilities, ELEM_MAX_LEN_EXT_CAP);

	prHsExtCap->aucCapabilities[0] = ELEM_EXT_CAP_DEFAULT_VAL;

	if (!fg40mAllowed)
		prHsExtCap->aucCapabilities[0] &=
			~ELEM_EXT_CAP_20_40_COEXIST_SUPPORT;

	if (prBssInfo->eCurrentOPMode != OP_MODE_INFRASTRUCTURE)
		prHsExtCap->aucCapabilities[0] &= ~ELEM_EXT_CAP_PSMP_CAP;

#if CFG_SUPPORT_802_11AC
	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);
	fgAppendVhtCap = FALSE;

	/* Check append rule */
	if (prAdapter->rWifiVar.ucAvailablePhyTypeSet
		& PHY_TYPE_SET_802_11AC) {
		/* Note: For AIS connecting state,
		 * structure in BSS_INFO will not be inited
		 *	 So, we check StaRec instead of BssInfo
		 */
		if (prStaRec) {
			if (prStaRec->ucPhyTypeSet & PHY_TYPE_SET_802_11AC)
				fgAppendVhtCap = TRUE;
		} else if (RLM_NET_IS_11AC(prBssInfo) &&
				((prBssInfo->eCurrentOPMode ==
				OP_MODE_INFRASTRUCTURE) ||
				(prBssInfo->eCurrentOPMode ==
				OP_MODE_ACCESS_POINT))) {
			fgAppendVhtCap = TRUE;
		}

	}

	if (fgAppendVhtCap) {
		if (prHsExtCap->ucLength < ELEM_MAX_LEN_EXT_CAP)
			prHsExtCap->ucLength = ELEM_MAX_LEN_EXT_CAP;

		SET_EXT_CAP(prHsExtCap->aucCapabilities, ELEM_MAX_LEN_EXT_CAP,
			    ELEM_EXT_CAP_OP_MODE_NOTIFICATION_BIT);
	}
#endif

	if (prHS20Info->fgConnectHS20AP == TRUE) {
		SET_EXT_CAP(prHsExtCap->aucCapabilities, ELEM_MAX_LEN_EXT_CAP,
			    ELEM_EXT_CAP_INTERWORKING_BIT);
		SET_EXT_CAP(prHsExtCap->aucCapabilities, ELEM_MAX_LEN_EXT_CAP,
			    ELEM_EXT_CAP_QOSMAPSET_BIT);
		SET_EXT_CAP(prHsExtCap->aucCapabilities, ELEM_MAX_LEN_EXT_CAP,
			    ELEM_EXT_CAP_BSS_TRANSITION_BIT);
		/* For R2 WNM-Notification */
		SET_EXT_CAP(prHsExtCap->aucCapabilities, ELEM_MAX_LEN_EXT_CAP,
			    ELEM_EXT_CAP_WNM_NOTIFICATION_BIT);
	}

#if CFG_SUPPORT_802_11V_BSS_TRANSITION_MGT
	prHsExtCap->ucLength = ELEM_MAX_LEN_EXT_CAP;
	SET_EXT_CAP(prHsExtCap->aucCapabilities, ELEM_MAX_LEN_EXT_CAP,
		    ELEM_EXT_CAP_BSS_TRANSITION_BIT);
#endif

#if (CFG_SUPPORT_802_11V_MBSSID == 1)
	if (prBssInfo->eCurrentOPMode != OP_MODE_ACCESS_POINT) {
	prExtCap->ucLength = ELEM_MAX_LEN_EXT_CAP;
	SET_EXT_CAP(prHsExtCap->aucCapabilities, ELEM_MAX_LEN_EXT_CAP,
		    ELEM_EXT_CAP_MBSSID_BIT);
	}
#endif

	if ((extCapIeLen - ELEM_HDR_LEN) > prHsExtCap->ucLength)
		prHsExtCap->ucLength = ELEM_MAX_LEN_EXT_CAP;

	if (extCapConn)
		rlmSyncExtCapIEwithSupplicant(prHsExtCap->aucCapabilities,
			extCapConn, extCapIeLen);
	else
		DBGLOG(RLM, WARN, "extCapConn = NULL!");

	ASSERT(IE_SIZE(prHsExtCap) <= (ELEM_HDR_LEN + ELEM_MAX_LEN_EXT_CAP));

	prMsduInfo->u2FrameLength += IE_SIZE(prHsExtCap);

#else
	/* Add Extended Capabilities IE */
	prExtCap = (struct IE_EXT_CAP *)(((uint8_t *)prMsduInfo->prPacket) +
					 prMsduInfo->u2FrameLength);

	prExtCap->ucId = ELEM_ID_EXTENDED_CAP;

	prExtCap->ucLength = 3 - ELEM_HDR_LEN;
	kalMemZero(prExtCap->aucCapabilities, ELEM_MAX_LEN_EXT_CAP);

	prExtCap->aucCapabilities[0] = ELEM_EXT_CAP_DEFAULT_VAL;

	if (!fg40mAllowed)
		prExtCap->aucCapabilities[0] &=
			~ELEM_EXT_CAP_20_40_COEXIST_SUPPORT;

	if (prBssInfo->eCurrentOPMode != OP_MODE_INFRASTRUCTURE)
		prExtCap->aucCapabilities[0] &= ~ELEM_EXT_CAP_PSMP_CAP;

#if CFG_SUPPORT_802_11AC
	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);
	fgAppendVhtCap = FALSE;

	/* Check append rule */
	if (prAdapter->rWifiVar.ucAvailablePhyTypeSet
		& PHY_TYPE_SET_802_11AC) {
		/* Note: For AIS connecting state,
		 * structure in BSS_INFO will not be inited
		 *       So, we check StaRec instead of BssInfo
		 */
		if (prStaRec) {
			if (prStaRec->ucPhyTypeSet & PHY_TYPE_SET_802_11AC)
				fgAppendVhtCap = TRUE;
		} else if (RLM_NET_IS_11AC(prBssInfo) &&
				((prBssInfo->eCurrentOPMode ==
				OP_MODE_INFRASTRUCTURE) ||
				(prBssInfo->eCurrentOPMode ==
				OP_MODE_ACCESS_POINT))) {
			fgAppendVhtCap = TRUE;
		}
	}

	if (fgAppendVhtCap) {
		if (prExtCap->ucLength < ELEM_MAX_LEN_EXT_CAP)
			prExtCap->ucLength = ELEM_MAX_LEN_EXT_CAP;

		SET_EXT_CAP(prExtCap->aucCapabilities, ELEM_MAX_LEN_EXT_CAP,
			    ELEM_EXT_CAP_OP_MODE_NOTIFICATION_BIT);
	}
#endif

#if (CFG_SUPPORT_TWT == 1)
	prExtCap->aucCapabilities[ELEM_EXT_CAP_TWT_REQUESTER_BIT >> 3] |=
		BIT(ELEM_EXT_CAP_TWT_REQUESTER_BIT % 8);
#endif

#if CFG_SUPPORT_802_11V_BSS_TRANSITION_MGT
	prExtCap->ucLength = ELEM_MAX_LEN_EXT_CAP;
	SET_EXT_CAP(prExtCap->aucCapabilities, ELEM_MAX_LEN_EXT_CAP,
				ELEM_EXT_CAP_BSS_TRANSITION_BIT);
#endif

#if (CFG_SUPPORT_802_11V_MBSSID == 1)
	if (prBssInfo->eCurrentOPMode != OP_MODE_ACCESS_POINT) {
	prExtCap->ucLength = ELEM_MAX_LEN_EXT_CAP;
	SET_EXT_CAP(prExtCap->aucCapabilities, ELEM_MAX_LEN_EXT_CAP,
				ELEM_EXT_CAP_MBSSID_BIT);
	}
#endif

#if (CFG_SUPPORT_802_11AX == 1)
	if ((prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT)
		&& (prBssInfo->eBand == BAND_2G4)
		&& !RLM_NET_IS_11AX(prBssInfo))
		prExtCap->ucLength = ELEM_MAX_LEN_EXT_CAP_11ABGNAC;
#endif

	if ((extCapIeLen - ELEM_HDR_LEN) > prExtCap->ucLength)
		prExtCap->ucLength = ELEM_MAX_LEN_EXT_CAP;

	if (extCapConn)
		rlmSyncExtCapIEwithSupplicant(prExtCap->aucCapabilities,
			extCapConn, extCapIeLen);
	else
		DBGLOG(RLM, WARN, "extCapConn = NULL!");

	ASSERT(IE_SIZE(prExtCap) <= (ELEM_HDR_LEN + ELEM_MAX_LEN_EXT_CAP));

	prMsduInfo->u2FrameLength += IE_SIZE(prExtCap);
#endif
}

void rlmSyncExtCapIEwithSupplicant(uint8_t *aucCapabilities,
			const uint8_t *supExtCapIEs, size_t IElen) {
	uint32_t i;

	for (i = ELEM_HDR_LEN * 8; i < IElen * 8; i++) {
		if (supExtCapIEs[i / 8] & BIT(i % 8)) {
			SET_EXT_CAP(aucCapabilities, ELEM_MAX_LEN_EXT_CAP,
			    i-ELEM_HDR_LEN*8);
		}
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
static void rlmFillHtOpIE(struct ADAPTER *prAdapter, struct BSS_INFO *prBssInfo,
			  struct MSDU_INFO *prMsduInfo)
{
	struct IE_HT_OP *prHtOp;
	uint16_t i;

	ASSERT(prAdapter);
	ASSERT(prBssInfo);
	ASSERT(prMsduInfo);

	prHtOp = (struct IE_HT_OP *)(((uint8_t *)prMsduInfo->prPacket) +
				     prMsduInfo->u2FrameLength);

	/* Add HT operation IE */
	prHtOp->ucId = ELEM_ID_HT_OP;
	prHtOp->ucLength = sizeof(struct IE_HT_OP) - ELEM_HDR_LEN;

	/* RIFS and 20/40 bandwidth operations are included */
	prHtOp->ucPrimaryChannel = prBssInfo->ucPrimaryChannel;
	prHtOp->ucInfo1 = prBssInfo->ucHtOpInfo1;

	/* Decide HT protection mode field */
	if (prBssInfo->eHtProtectMode == HT_PROTECT_MODE_NON_HT)
		prHtOp->u2Info2 = (uint8_t)HT_PROTECT_MODE_NON_HT;
	else if (prBssInfo->eObssHtProtectMode == HT_PROTECT_MODE_NON_MEMBER)
		prHtOp->u2Info2 = (uint8_t)HT_PROTECT_MODE_NON_MEMBER;
	else {
		/* It may be SYS_PROTECT_MODE_NONE or SYS_PROTECT_MODE_20M */
		prHtOp->u2Info2 = (uint8_t)prBssInfo->eHtProtectMode;
	}

	if (prBssInfo->eGfOperationMode != GF_MODE_NORMAL) {
		/* It may be GF_MODE_PROTECT or GF_MODE_DISALLOWED
		 * Note: it will also be set in ad-hoc network
		 */
		prHtOp->u2Info2 |= HT_OP_INFO2_NON_GF_HT_STA_PRESENT;
	}

	if (0 /* Regulatory class 16 */ &&
	    prBssInfo->eObssHtProtectMode == HT_PROTECT_MODE_NON_MEMBER) {
		/* (TBD) It is HT_PROTECT_MODE_NON_MEMBER, so require protection
		 * although it is possible to have no protection by spec.
		 */
		prHtOp->u2Info2 |= HT_OP_INFO2_OBSS_NON_HT_STA_PRESENT;
	}

	prHtOp->u2Info3 = prBssInfo->u2HtOpInfo3; /* To do: handle L-SIG TXOP */

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
void rlmReqGenerateVhtCapIE(struct ADAPTER *prAdapter,
			    struct MSDU_INFO *prMsduInfo)
{
	struct BSS_INFO *prBssInfo;
	struct STA_RECORD *prStaRec;
#if CFG_SUPPORT_VHT_IE_IN_2G
	struct BSS_DESC *prBssDesc = NULL;
	u_int8_t fgIsVHTPresent = FALSE;
#endif

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

	if (prAdapter) {
		prBssInfo = prAdapter->aprBssInfo[prMsduInfo->ucBssIndex];
#if CFG_SUPPORT_VHT_IE_IN_2G
		prBssDesc = aisGetTargetBssDesc(prAdapter,
			prMsduInfo->ucBssIndex);
		if (prBssDesc) {
			fgIsVHTPresent = prBssDesc->fgIsVHTPresent;
			DBGLOG(RLM, TRACE,
				"fgIsVHTPresent=%d", fgIsVHTPresent);
		}
#endif
	} else {
		DBGLOG(RLM, ERROR, "prAdapter is NULL, return!");
		return;
	}

	if (!prBssInfo) {
		DBGLOG(RLM, ERROR, "prBssInfo is NULL, return!");
		return;
	}

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

	if (!prStaRec) {
		DBGLOG(RLM, ERROR, "prStaRec is NULL, return!");
		return;
	}

	if ((prAdapter->rWifiVar.ucAvailablePhyTypeSet & PHY_TYPE_SET_802_11AC)
		&& (prStaRec->ucPhyTypeSet & PHY_TYPE_SET_802_11AC))
		rlmFillVhtCapIE(prAdapter, prBssInfo, prMsduInfo);
#if CFG_SUPPORT_VHT_IE_IN_2G
	else if ((prBssInfo->eBand == BAND_2G4) &&
			(prStaRec->ucPhyTypeSet & PHY_TYPE_SET_802_11N) &&
			((prAdapter->rWifiVar.ucVhtIeIn2g
				==  FEATURE_FORCE_ENABLED) ||
			((prAdapter->rWifiVar.ucVhtIeIn2g
				== FEATURE_ENABLED) && fgIsVHTPresent))) {
		DBGLOG(RLM, TRACE,
			"Add VHT IE in 2.4G, ucPhyTypeSet=%02x, ucVhtIeIn2g=%02x",
			prStaRec->ucPhyTypeSet,
			prAdapter->rWifiVar.ucVhtIeIn2g);
		rlmFillVhtCapIE(prAdapter, prBssInfo, prMsduInfo);
	}
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
void rlmRspGenerateVhtCapIE(struct ADAPTER *prAdapter,
			    struct MSDU_INFO *prMsduInfo)
{
	struct BSS_INFO *prBssInfo;
	struct STA_RECORD *prStaRec;
	uint8_t ucPhyTypeSet;

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

	if (RLM_NET_IS_11AC(prBssInfo) &&
	    (ucPhyTypeSet & PHY_TYPE_SET_802_11AC))
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
void rlmRspGenerateVhtOpIE(struct ADAPTER *prAdapter,
			   struct MSDU_INFO *prMsduInfo)
{
	struct BSS_INFO *prBssInfo;
	struct STA_RECORD *prStaRec;
	uint8_t ucPhyTypeSet;

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

	if (RLM_NET_IS_11AC(prBssInfo) &&
	    (ucPhyTypeSet & PHY_TYPE_SET_802_11AC))
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
void rlmReqGenerateVhtOpNotificationIE(struct ADAPTER *prAdapter,
				       struct MSDU_INFO *prMsduInfo)
{
	struct BSS_INFO *prBssInfo;
	struct STA_RECORD *prStaRec;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

	prBssInfo = prAdapter->aprBssInfo[prMsduInfo->ucBssIndex];
	if (!prBssInfo)
		return;
	/* [TGac 5.2.46 STBC Receive Test with UCC 9.2.x]
	 * Operating Notification IE of Nss=2 will make Ralink testbed send data
	 * frames without STBC
	 * Enable the Operating Notification IE only for DBDC enable case.
	 */
	if (!prAdapter->rWifiVar.fgDbDcModeEn)
		return;
	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

	if ((prAdapter->rWifiVar.ucAvailablePhyTypeSet &
	     PHY_TYPE_SET_802_11AC) &&
	    (!prStaRec || (prStaRec->ucPhyTypeSet & PHY_TYPE_SET_802_11AC))) {
		/* Fill own capability in channel width field in OP mode element
		 * since we haven't filled in channel width info in BssInfo at
		 * current state
		 */
		rlmFillVhtOpNotificationIE(prAdapter, prBssInfo, prMsduInfo,
					   TRUE);
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
void rlmRspGenerateVhtOpNotificationIE(struct ADAPTER *prAdapter,
				       struct MSDU_INFO *prMsduInfo)
{
	struct BSS_INFO *prBssInfo;
	struct STA_RECORD *prStaRec;
	uint8_t ucPhyTypeSet;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

	prBssInfo = prAdapter->aprBssInfo[prMsduInfo->ucBssIndex];
	if (!prBssInfo)
		return;

	if (!IS_BSS_ACTIVE(prBssInfo))
		return;

	if (!prBssInfo->fgIsOpChangeRxNss)
		return;

	/* Decide PHY type set source */
	if (prStaRec) {
		/* Get PHY type set from target STA */
		ucPhyTypeSet = prStaRec->ucPhyTypeSet;
	} else {
		/* Get PHY type set from current BSS */
		ucPhyTypeSet = prBssInfo->ucPhyTypeSet;
	}

	if (RLM_NET_IS_11AC(prBssInfo) &&
	    (ucPhyTypeSet & PHY_TYPE_SET_802_11AC))
		rlmFillVhtOpNotificationIE(prAdapter, prBssInfo, prMsduInfo,
					   FALSE);
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
static void rlmFillVhtOpNotificationIE(struct ADAPTER *prAdapter,
				       struct BSS_INFO *prBssInfo,
				       struct MSDU_INFO *prMsduInfo,
				       u_int8_t fgIsOwnCap)
{
	struct IE_VHT_OP_MODE_NOTIFICATION *prVhtOpMode;
	uint8_t ucOpModeBw = VHT_OP_MODE_CHANNEL_WIDTH_20;

	ASSERT(prAdapter);
	ASSERT(prBssInfo);
	ASSERT(prMsduInfo);

	prVhtOpMode = (struct IE_VHT_OP_MODE_NOTIFICATION
			       *)(((uint8_t *)prMsduInfo->prPacket) +
				  prMsduInfo->u2FrameLength);

	kalMemZero((void *)prVhtOpMode,
		   sizeof(struct IE_VHT_OP_MODE_NOTIFICATION));

	prVhtOpMode->ucId = ELEM_ID_OP_MODE;
	prVhtOpMode->ucLength =
		sizeof(struct IE_VHT_OP_MODE_NOTIFICATION) - ELEM_HDR_LEN;

	DBGLOG(RLM, TRACE, "rlmFillVhtOpNotificationIE(%d) %u %u\n",
	       prBssInfo->ucBssIndex, fgIsOwnCap, prBssInfo->ucOpRxNss);

	if (fgIsOwnCap) {
		ucOpModeBw = cnmGetDbdcBwCapability(prAdapter,
						    prBssInfo->ucBssIndex);

		/*handle 80P80 case*/
		if (ucOpModeBw >= MAX_BW_160MHZ)
			ucOpModeBw = VHT_OP_MODE_CHANNEL_WIDTH_160_80P80;

		prVhtOpMode->ucOperatingMode |= ucOpModeBw;
		prVhtOpMode->ucOperatingMode |=
			(((prBssInfo->ucOpRxNss - 1)
				<< VHT_OP_MODE_RX_NSS_OFFSET) &
			 VHT_OP_MODE_RX_NSS);

	} else {

		ucOpModeBw = rlmGetOpModeBwByVhtAndHtOpInfo(prBssInfo);

		prVhtOpMode->ucOperatingMode |= ucOpModeBw;
		prVhtOpMode->ucOperatingMode |=
			(((prBssInfo->ucOpRxNss - 1)
				<< VHT_OP_MODE_RX_NSS_OFFSET) &
			 VHT_OP_MODE_RX_NSS);
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
static void rlmFillVhtCapIE(struct ADAPTER *prAdapter,
			    struct BSS_INFO *prBssInfo,
			    struct MSDU_INFO *prMsduInfo)
{
	struct IE_VHT_CAP *prVhtCap;
	struct VHT_SUPPORTED_MCS_FIELD *prVhtSupportedMcsSet;
	uint8_t i;
	uint8_t ucMaxBw;
	struct STA_RECORD *prStaRec;

	ASSERT(prAdapter);
	ASSERT(prBssInfo);
	ASSERT(prMsduInfo);

	prVhtCap = (struct IE_VHT_CAP *)(((uint8_t *)prMsduInfo->prPacket) +
					 prMsduInfo->u2FrameLength);

	prVhtCap->ucId = ELEM_ID_VHT_CAP;
	prVhtCap->ucLength = sizeof(struct IE_VHT_CAP) - ELEM_HDR_LEN;
	prVhtCap->u4VhtCapInfo = VHT_CAP_INFO_DEFAULT_VAL;

	ucMaxBw = cnmGetBssMaxBw(prAdapter, prBssInfo->ucBssIndex);

	prVhtCap->u4VhtCapInfo |= (prAdapter->rWifiVar.ucRxMaxMpduLen &
				   VHT_CAP_INFO_MAX_MPDU_LEN_MASK);
#if CFG_SUPPORT_VHT_IE_IN_2G
	if (prBssInfo->eBand == BAND_2G4) {
		prVhtCap->u4VhtCapInfo |=
			VHT_CAP_INFO_MAX_SUP_CHANNEL_WIDTH_SET_NONE;
	} else {
#endif
		if (ucMaxBw == MAX_BW_160MHZ)
			prVhtCap->u4VhtCapInfo |=
				VHT_CAP_INFO_MAX_SUP_CHANNEL_WIDTH_SET_160;
		else if (ucMaxBw == MAX_BW_80_80_MHZ)
			prVhtCap->u4VhtCapInfo |=
		VHT_CAP_INFO_MAX_SUP_CHANNEL_WIDTH_SET_160_80P80;

		if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucStaVhtBfee)) {
			prVhtCap->u4VhtCapInfo |= FIELD_VHT_CAP_INFO_BFEE;
#if CFG_SUPPORT_BFEE
		prStaRec = cnmGetStaRecByIndex(prAdapter,
					       prMsduInfo->ucStaRecIndex);

		if (prStaRec &&
		(prStaRec->ucVhtCapNumSoundingDimensions == 0x2) &&
		!prAdapter->rWifiVar.fgForceSTSNum) {
		/* For the compatibility with netgear R7000 AP */
			prVhtCap->u4VhtCapInfo |=
		(((uint32_t)prStaRec->ucVhtCapNumSoundingDimensions)
<< VHT_CAP_INFO_COMPRESSED_STEERING_NUMBER_OF_BEAMFORMER_ANTENNAS_SUP_OFF);
			DBGLOG(RLM, INFO, "Set VHT Cap BFEE STS CAP=%d\n",
				       prStaRec->ucVhtCapNumSoundingDimensions);
		} else {
		/* For 11ac cert. VHT-5.2.63C MU-BFee step3,
		 * it requires STAUT to set its maximum STS capability here
		 */
			prVhtCap->u4VhtCapInfo |=
VHT_CAP_INFO_COMPRESSED_STEERING_NUMBER_OF_BEAMFORMER_ANTENNAS_4_SUP;
			DBGLOG(RLM, TRACE, "Set VHT Cap BFEE STS CAP=%d\n",
				VHT_CAP_INFO_BEAMFORMEE_STS_CAP_MAX);
		}
/* DBGLOG(RLM, INFO, "VhtCapInfo=%x\n", prVhtCap->u4VhtCapInfo); */
#endif
		if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucStaVhtMuBfee))
			prVhtCap->u4VhtCapInfo |=
				VHT_CAP_INFO_MU_BEAMFOMEE_CAPABLE;
		}

		if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucStaVhtBfer))
			prVhtCap->u4VhtCapInfo |= FIELD_VHT_CAP_INFO_BFER;
#if CFG_SUPPORT_VHT_IE_IN_2G
	}
#endif
	if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucRxShortGI)) {
		if (ucMaxBw >= MAX_BW_80MHZ)
			prVhtCap->u4VhtCapInfo |= VHT_CAP_INFO_SHORT_GI_80;

		if (ucMaxBw >= MAX_BW_160MHZ)
			prVhtCap->u4VhtCapInfo |=
				VHT_CAP_INFO_SHORT_GI_160_80P80;
	}

	if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucRxLdpc))
		prVhtCap->u4VhtCapInfo |= VHT_CAP_INFO_RX_LDPC;

	if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucRxStbc)) {
		uint8_t tempRxStbcNss;

		if (prAdapter->rWifiVar.u4SwTestMode ==
		    ENUM_SW_TEST_MODE_SIGMA_AC) {
			tempRxStbcNss = 1;
			DBGLOG(RLM, INFO,
			       "Set RxStbcNss to 1 for 11ac certification.\n");
		} else {
			tempRxStbcNss = prAdapter->rWifiVar.ucRxStbcNss;
			tempRxStbcNss =
				(tempRxStbcNss >
				 wlanGetSupportNss(prAdapter,
						   prBssInfo->ucBssIndex))
					? wlanGetSupportNss(
						  prAdapter,
						  prBssInfo->ucBssIndex)
					: (tempRxStbcNss);
			if (tempRxStbcNss != prAdapter->rWifiVar.ucRxStbcNss) {
				DBGLOG(RLM, WARN,
				       "Apply Nss:%d as RxStbcNss in VHT Cap",
				       wlanGetSupportNss(
					       prAdapter,
					       prBssInfo->ucBssIndex));
				DBGLOG(RLM, WARN,
				       "due to set RxStbcNss more than Nss is not appropriate.\n");
			}
		}
		prVhtCap->u4VhtCapInfo |=
			((tempRxStbcNss << VHT_CAP_INFO_RX_STBC_OFFSET) &
			 VHT_CAP_INFO_RX_STBC_MASK);
	}

	if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucTxStbc) &&
			prBssInfo->ucOpTxNss >= 2)
		prVhtCap->u4VhtCapInfo |= VHT_CAP_INFO_TX_STBC;

	/*set MCS map */
	prVhtSupportedMcsSet = &prVhtCap->rVhtSupportedMcsSet;
	kalMemZero((void *)prVhtSupportedMcsSet,
		   sizeof(struct VHT_SUPPORTED_MCS_FIELD));

	for (i = 0; i < 8; i++) {
		uint8_t ucOffset = i * 2;
		uint8_t ucMcsMap;

		if (i < wlanGetSupportNss(prAdapter, prBssInfo->ucBssIndex))
			ucMcsMap = VHT_CAP_INFO_MCS_MAP_MCS9;
		else
			ucMcsMap = VHT_CAP_INFO_MCS_NOT_SUPPORTED;

		prVhtSupportedMcsSet->u2RxMcsMap |= (ucMcsMap << ucOffset);
		prVhtSupportedMcsSet->u2TxMcsMap |= (ucMcsMap << ucOffset);
	}

#if 0
	for (i = 0; i < wlanGetSupportNss(prAdapter,
		prBssInfo->ucBssIndex); i++) {
		uint8_t ucOffset = i * 2;

		prVhtSupportedMcsSet->u2RxMcsMap &=
			((VHT_CAP_INFO_MCS_MAP_MCS9 << ucOffset) &
			BITS(ucOffset, ucOffset + 1));
		prVhtSupportedMcsSet->u2TxMcsMap &=
			((VHT_CAP_INFO_MCS_MAP_MCS9 << ucOffset) &
			BITS(ucOffset, ucOffset + 1));
	}
#endif

	prVhtSupportedMcsSet->u2RxHighestSupportedDataRate =
		VHT_CAP_INFO_DEFAULT_HIGHEST_DATA_RATE;
	prVhtSupportedMcsSet->u2TxHighestSupportedDataRate =
		VHT_CAP_INFO_DEFAULT_HIGHEST_DATA_RATE;

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
void rlmFillVhtOpIE(struct ADAPTER *prAdapter, struct BSS_INFO *prBssInfo,
		    struct MSDU_INFO *prMsduInfo)
{
	struct IE_VHT_OP *prVhtOp;

	ASSERT(prAdapter);
	ASSERT(prBssInfo);
	ASSERT(prMsduInfo);

	prVhtOp = (struct IE_VHT_OP *)(((uint8_t *)prMsduInfo->prPacket) +
				       prMsduInfo->u2FrameLength);

	/* Add HT operation IE */
	prVhtOp->ucId = ELEM_ID_VHT_OP;
	prVhtOp->ucLength = sizeof(struct IE_VHT_OP) - ELEM_HDR_LEN;

	ASSERT(IE_SIZE(prVhtOp) <= (ELEM_HDR_LEN + ELEM_MAX_LEN_VHT_OP));

	/* (UINT8)VHT_OP_CHANNEL_WIDTH_80; */
	prVhtOp->ucVhtOperation[0] = prBssInfo->ucVhtChannelWidth;
	prVhtOp->ucVhtOperation[1] = prBssInfo->ucVhtChannelFrequencyS1;
	prVhtOp->ucVhtOperation[2] = prBssInfo->ucVhtChannelFrequencyS2;

#if 0
	if (cnmGetBssMaxBw(prAdapter, prBssInfo->ucBssIndex) < MAX_BW_80MHZ) {
		prVhtOp->ucVhtOperation[0] = VHT_OP_CHANNEL_WIDTH_20_40;
		prVhtOp->ucVhtOperation[1] = 0;
		prVhtOp->ucVhtOperation[2] = 0;
	} else if (cnmGetBssMaxBw(prAdapter, prBssInfo->ucBssIndex) ==
			MAX_BW_80MHZ) {
		prVhtOp->ucVhtOperation[0] = VHT_OP_CHANNEL_WIDTH_80;
		prVhtOp->ucVhtOperation[1] =
			nicGetVhtS1(prBssInfo->ucPrimaryChannel);
		prVhtOp->ucVhtOperation[2] = 0;
	} else {
		/* TODO: BW80 + 80/160 support */
	}
#endif

	prVhtOp->u2VhtBasicMcsSet = prBssInfo->u2VhtBasicMcsSet;

	prMsduInfo->u2FrameLength += IE_SIZE(prVhtOp);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Get RxNss from VHT CAP IE.
 *
 * \param[in] prVhtCap
 *
 * \return ucRxNss
 */
/*----------------------------------------------------------------------------*/
uint8_t
rlmGetSupportRxNssInVhtCap(struct IE_VHT_CAP *prVhtCap)
{
	uint8_t ucRxNss = 1;

	if (prVhtCap) {
		if (((prVhtCap->rVhtSupportedMcsSet.u2RxMcsMap &
			VHT_CAP_INFO_MCS_2SS_MASK) >>
			VHT_CAP_INFO_MCS_2SS_OFFSET)
			!= VHT_CAP_INFO_MCS_NOT_SUPPORTED)
			ucRxNss = 2;
		if (((prVhtCap->rVhtSupportedMcsSet.u2RxMcsMap &
			VHT_CAP_INFO_MCS_3SS_MASK)
			>> VHT_CAP_INFO_MCS_3SS_OFFSET)
			!= VHT_CAP_INFO_MCS_NOT_SUPPORTED)
			ucRxNss = 3;
		if (((prVhtCap->rVhtSupportedMcsSet.u2RxMcsMap &
			VHT_CAP_INFO_MCS_4SS_MASK)
			>> VHT_CAP_INFO_MCS_4SS_OFFSET)
			!= VHT_CAP_INFO_MCS_NOT_SUPPORTED)
			ucRxNss = 4;
		if (((prVhtCap->rVhtSupportedMcsSet.u2RxMcsMap &
			VHT_CAP_INFO_MCS_5SS_MASK)
			>> VHT_CAP_INFO_MCS_5SS_OFFSET)
			!= VHT_CAP_INFO_MCS_NOT_SUPPORTED)
			ucRxNss = 5;
		if (((prVhtCap->rVhtSupportedMcsSet.u2RxMcsMap &
			VHT_CAP_INFO_MCS_6SS_MASK)
			>> VHT_CAP_INFO_MCS_6SS_OFFSET)
			!= VHT_CAP_INFO_MCS_NOT_SUPPORTED)
			ucRxNss = 6;
		if (((prVhtCap->rVhtSupportedMcsSet.u2RxMcsMap &
			VHT_CAP_INFO_MCS_7SS_MASK)
			>> VHT_CAP_INFO_MCS_7SS_OFFSET)
			!= VHT_CAP_INFO_MCS_NOT_SUPPORTED)
			ucRxNss = 7;
		if (((prVhtCap->rVhtSupportedMcsSet.u2RxMcsMap &
			VHT_CAP_INFO_MCS_8SS_MASK)
			>> VHT_CAP_INFO_MCS_8SS_OFFSET)
			!= VHT_CAP_INFO_MCS_NOT_SUPPORTED)
			ucRxNss = 8;
		} else
			DBGLOG(RLM, WARN, "null prVhtCap, assume RxNss=1\n");

	return ucRxNss;
}

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
void rlmGenerateCountryIE(struct ADAPTER *prAdapter,
		struct MSDU_INFO *prMsduInfo)
{
	struct BSS_INFO *prBssInfo =
		prAdapter->aprBssInfo[prMsduInfo->ucBssIndex];
	unsigned char *pucBuf =
		(((unsigned char *) prMsduInfo->prPacket) +
		prMsduInfo->u2FrameLength);

	if (prBssInfo->aucCountryStr[0] == 0)
		return;

	COUNTRY_IE(pucBuf)->ucId = ELEM_ID_COUNTRY_INFO;
	COUNTRY_IE(pucBuf)->ucLength = prBssInfo->ucCountryIELen;
	COUNTRY_IE(pucBuf)->aucCountryStr[0] = prBssInfo->aucCountryStr[0];
	COUNTRY_IE(pucBuf)->aucCountryStr[1] = prBssInfo->aucCountryStr[1];
	COUNTRY_IE(pucBuf)->aucCountryStr[2] = prBssInfo->aucCountryStr[2];
	kalMemCopy(COUNTRY_IE(pucBuf)->arCountryStr,
		prBssInfo->aucSubbandTriplet,
		prBssInfo->ucCountryIELen - 3);

	prMsduInfo->u2FrameLength += IE_SIZE(pucBuf);
}

#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST
uint32_t rlmCalBackup(struct ADAPTER *prAdapter, uint8_t ucReason,
		      uint8_t ucAction, uint8_t ucRomRam)
{
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct PARAM_CAL_BACKUP_STRUCT_V2 rCalBackupDataV2;
	uint32_t u4BufLen = 0;

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

		rStatus = kalIoctl(prGlueInfo, wlanoidQueryCalBackupV2,
				   &rCalBackupDataV2,
				   sizeof(struct PARAM_CAL_BACKUP_STRUCT_V2),
				   TRUE, TRUE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(RFTEST, INFO,
			       "RLM CMD : Get Thermal Temp from FW Return Fail (0x%08x)!!!!!!!!!!!\n",
			       rStatus);
			return rStatus;
		}

		DBGLOG(RFTEST, INFO,
		       "CMD : Get Thermal Temp (%d) from FW. Finish!!!!!!!!!!!\n",
		       rCalBackupDataV2.u4ThermalValue);
	} else if (ucReason == 1 && ucAction == 2) {
		DBGLOG(RFTEST, INFO, "RLM CMD : Trigger FW Do All Cal.\n");
		/* Step 2 : Trigger All Cal Function */

		rStatus = kalIoctl(prGlueInfo, wlanoidSetCalBackup,
				   &rCalBackupDataV2,
				   sizeof(struct PARAM_CAL_BACKUP_STRUCT_V2),
				   FALSE, FALSE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(RFTEST, INFO,
			       "RLM CMD : Trigger FW Do All Cal Return Fail (0x%08x)!!!!!!!!!!!\n",
			       rStatus);
			return rStatus;
		}

		DBGLOG(RFTEST, INFO,
		       "CMD : Trigger FW Do All Cal. Finish!!!!!!!!!!!\n");
	} else if (ucReason == 0 && ucAction == 1) {
		DBGLOG(RFTEST, INFO,
		       "RLM CMD : Get Cal Data (%s) Size from FW.\n",
		       ucRomRam == 0 ? "ROM" : "RAM");
		/* Step 3 : Get Cal Data Size from FW */

		rStatus = kalIoctl(prGlueInfo, wlanoidQueryCalBackupV2,
				   &rCalBackupDataV2,
				   sizeof(struct PARAM_CAL_BACKUP_STRUCT_V2),
				   TRUE, TRUE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(RFTEST, INFO,
			       "RLM CMD : Get Cal Data (%s) Size from FW Return Fail (0x%08x)!!!!!!!!!!!\n",
			       ucRomRam == 0 ? "ROM" : "RAM", rStatus);
			return rStatus;
		}

		DBGLOG(RFTEST, INFO,
		       "CMD : Get Cal Data (%s) Size from FW. Finish!!!!!!!!!!!\n",
		       ucRomRam == 0 ? "ROM" : "RAM");
	} else if (ucReason == 2 && ucAction == 4) {
		DBGLOG(RFTEST, INFO,
		       "RLM CMD : Get Cal Data from FW (%s). Start!!!!!!!!!!!!!!!!\n",
		       ucRomRam == 0 ? "ROM" : "RAM");
		DBGLOG(RFTEST, INFO, "Thermal Temp = %d\n",
		       g_rBackupCalDataAllV2.u4ThermalInfo);
		/* Step 4 : Get Cal Data from FW */

		rStatus = kalIoctl(prGlueInfo, wlanoidQueryCalBackupV2,
				   &rCalBackupDataV2,
				   sizeof(struct PARAM_CAL_BACKUP_STRUCT_V2),
				   TRUE, TRUE, TRUE, &u4BufLen);
		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(RFTEST, INFO,
			       "RLM CMD : Get Cal Data (%s) Size from FW Return Fail (0x%08x)!!!!!!!!!!!\n",
			       ucRomRam == 0 ? "ROM" : "RAM", rStatus);
			return rStatus;
		}

		DBGLOG(RFTEST, INFO,
		       "CMD : Get Cal Data from FW (%s). Finish!!!!!!!!!!!\n",
		       ucRomRam == 0 ? "ROM" : "RAM");

		if (ucRomRam == 0) {
			DBGLOG(RFTEST, INFO,
			       "Check some of elements (0x%08x), (0x%08x), (0x%08x), (0x%08x), (0x%08x)\n",
			       g_rBackupCalDataAllV2.au4RomCalData[670],
			       g_rBackupCalDataAllV2.au4RomCalData[671],
			       g_rBackupCalDataAllV2.au4RomCalData[672],
			       g_rBackupCalDataAllV2.au4RomCalData[673],
			       g_rBackupCalDataAllV2.au4RomCalData[674]);
			DBGLOG(RFTEST, INFO,
			       "Check some of elements (0x%08x), (0x%08x), (0x%08x), (0x%08x), (0x%08x)\n",
			       g_rBackupCalDataAllV2.au4RomCalData[675],
			       g_rBackupCalDataAllV2.au4RomCalData[676],
			       g_rBackupCalDataAllV2.au4RomCalData[677],
			       g_rBackupCalDataAllV2.au4RomCalData[678],
			       g_rBackupCalDataAllV2.au4RomCalData[679]);
		}
	} else if (ucReason == 4 && ucAction == 6) {
		DBGLOG(RFTEST, INFO, "RLM CMD : Print Cal Data in FW (%s).\n",
		       ucRomRam == 0 ? "ROM" : "RAM");
		/* Debug Use : Print Cal Data in FW */

		rStatus = kalIoctl(prGlueInfo, wlanoidSetCalBackup,
				   &rCalBackupDataV2,
				   sizeof(struct PARAM_CAL_BACKUP_STRUCT_V2),
				   TRUE, TRUE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(RFTEST, INFO,
			       "RLM CMD : Print Cal Data in FW (%s) Return Fail (0x%08x)!!!!!!!!!!!\n",
			       ucRomRam == 0 ? "ROM" : "RAM", rStatus);
			return rStatus;
		}

		DBGLOG(RFTEST, INFO,
		       "CMD : Print Cal Data in FW (%s). Finish!!!!!!!!!!!\n",
		       ucRomRam == 0 ? "ROM" : "RAM");
	} else if (ucReason == 3 && ucAction == 5) {
		DBGLOG(RFTEST, INFO, "RLM CMD : Send Cal Data to FW (%s).\n",
		       ucRomRam == 0 ? "ROM" : "RAM");
		/* Send Cal Data to FW */

		rStatus = kalIoctl(prGlueInfo, wlanoidSetCalBackup,
				   &rCalBackupDataV2,
				   sizeof(struct PARAM_CAL_BACKUP_STRUCT_V2),
				   TRUE, TRUE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(RFTEST, INFO,
			       "RLM CMD : Send Cal Data to FW (%s) Return Fail (0x%08x)!!!!!!!!!!!\n",
			       ucRomRam == 0 ? "ROM" : "RAM", rStatus);
			return rStatus;
		}

		DBGLOG(RFTEST, INFO,
		       "CMD : Send Cal Data to FW (%s). Finish!!!!!!!!!!!\n",
		       ucRomRam == 0 ? "ROM" : "RAM");
	} else {
		DBGLOG(RFTEST, INFO,
		       "CMD : Undefined Reason (%d) and Action (%d) for Cal Backup in Host Side!\n",
		       ucReason, ucAction);

		return rStatus;
	}

	return rStatus;
}

uint32_t rlmTriggerCalBackup(struct ADAPTER *prAdapter,
			     u_int8_t fgIsCalDataBackuped)
{
	uint32_t rStatus = WLAN_STATUS_SUCCESS;

	if (!fgIsCalDataBackuped) {
		DBGLOG(RFTEST, INFO,
		       "======== Boot Time Wi-Fi Enable........\n");
		DBGLOG(RFTEST, INFO,
		       "Step 0 : Reset All Cal Data in Driver.\n");
		memset(&g_rBackupCalDataAllV2, 1,
		       sizeof(struct RLM_CAL_RESULT_ALL_V2));
		g_rBackupCalDataAllV2.u4MagicNum1 = 6632;
		g_rBackupCalDataAllV2.u4MagicNum2 = 6632;

		DBGLOG(RFTEST, INFO, "Step 1 : Get Thermal Temp from FW.\n");
		if (rlmCalBackup(prAdapter, 0, 0, 0) == WLAN_STATUS_FAILURE) {
			DBGLOG(RFTEST, INFO, "Step 1 : Return Failure.\n");
			return WLAN_STATUS_FAILURE;
		}

		DBGLOG(RFTEST, INFO,
		       "Step 2 : Get Rom Cal Data Size from FW.\n");
		if (rlmCalBackup(prAdapter, 0, 1, 0) == WLAN_STATUS_FAILURE) {
			DBGLOG(RFTEST, INFO, "Step 2 : Return Failure.\n");
			return WLAN_STATUS_FAILURE;
		}

		DBGLOG(RFTEST, INFO,
		       "Step 3 : Get Ram Cal Data Size from FW.\n");
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

void rlmModifyVhtBwPara(uint8_t *pucVhtChannelFrequencyS1,
			uint8_t *pucVhtChannelFrequencyS2,
			uint8_t *pucVhtChannelWidth)
{
	uint8_t i = 0, ucTempS = 0;

	if ((*pucVhtChannelFrequencyS1 != 0) &&
	    (*pucVhtChannelFrequencyS2 != 0)) {

		uint8_t ucBW160Inteval = 8;

		if (((*pucVhtChannelFrequencyS2 - *pucVhtChannelFrequencyS1) ==
		     ucBW160Inteval) ||
		    ((*pucVhtChannelFrequencyS1 - *pucVhtChannelFrequencyS2) ==
		     ucBW160Inteval)) {
			/*C160 case*/

			/* NEW spec should set central ch of bw80 at S1,
			 * set central ch of bw160 at S2
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
				DBGLOG(RLM, WARN,
				       "please check BW160 setting, find central freq fail\n");
				return;
			}

			*pucVhtChannelFrequencyS1 = ucTempS;
			*pucVhtChannelFrequencyS2 = 0;
			*pucVhtChannelWidth = CW_160MHZ;
		} else {
			/*real 80P80 case*/
		}
	}
}

static void rlmRevisePreferBandwidthNss(struct ADAPTER *prAdapter,
					uint8_t ucBssIndex,
					struct STA_RECORD *prStaRec)
{
	enum ENUM_CHANNEL_WIDTH eChannelWidth = CW_20_40MHZ;
	struct BSS_INFO *prBssInfo;

#define VHT_MCS_TX_RX_MAX_2SS BITS(2, 3)
#define VHT_MCS_TX_RX_MAX_2SS_SHIFT 2

#define AR_STA_2AC_MCS(prStaRec)                                               \
	(((prStaRec)->u2VhtRxMcsMap & VHT_MCS_TX_RX_MAX_2SS) >>                \
	 VHT_MCS_TX_RX_MAX_2SS_SHIFT)

#define AR_IS_STA_2SS_AC(prStaRec) ((AR_STA_2AC_MCS(prStaRec) != BITS(0, 1)))

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

	eChannelWidth = prBssInfo->ucVhtChannelWidth;

	/*
	 * Prefer setting modification
	 * 80+80 1x1 and 80 2x2 have the same phy rate, choose the 80 2x2
	 */

	if (AR_IS_STA_2SS_AC(prStaRec)) {
		/*
		 * DBGLOG(RLM, WARN, "support 2ss\n");
		 */

		if ((eChannelWidth == CW_80P80MHZ &&
		     prBssInfo->ucVhtChannelFrequencyS2 != 0)) {
			DBGLOG(RLM, WARN, "support (2Nss) and (80+80)\n");
			DBGLOG(RLM, WARN,
			       "choose (2Nss) and (80) for Bss_info\n");
			prBssInfo->ucVhtChannelWidth = CW_80MHZ;
			prBssInfo->ucVhtChannelFrequencyS2 = 0;
		}
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Revise operating BW by own maximum bandwidth capability
 *
 * \param[in]
 *
 * \return none
 */
/*----------------------------------------------------------------------------*/
void rlmReviseMaxBw(struct ADAPTER *prAdapter, uint8_t ucBssIndex,
		    enum ENUM_CHNL_EXT *peExtend,
		    enum ENUM_CHANNEL_WIDTH *peChannelWidth, uint8_t *pucS1,
		    uint8_t *pucPrimaryCh)
{
	uint8_t ucMaxBandwidth = MAX_BW_80MHZ;
	uint8_t ucCurrentBandwidth = MAX_BW_20MHZ;
	uint8_t ucOffset = (MAX_BW_80MHZ - CW_80MHZ);

	ucMaxBandwidth = cnmGetDbdcBwCapability(prAdapter, ucBssIndex);

	if (*peChannelWidth > CW_20_40MHZ) {
		/*case BW > 80 , 160 80P80 */
		ucCurrentBandwidth = (uint8_t)*peChannelWidth + ucOffset;
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
			/* BW80, BW160, BW80P80
			 * ucMaxBandwidth Must be
			 * MAX_BW_80MHZ,MAX_BW_160MHZ,MAX_BW_80MHZ
			 * peExtend should not change
			 */
			*peChannelWidth = (ucMaxBandwidth - ucOffset);

			if (ucMaxBandwidth == MAX_BW_80MHZ) {
				/* modify S1 for Bandwidth 160 downgrade 80 case
				 */
				if (ucCurrentBandwidth == MAX_BW_160MHZ) {
					if ((*pucPrimaryCh >= 36) &&
					    (*pucPrimaryCh <= 48))
						*pucS1 = 42;
					else if ((*pucPrimaryCh >= 52) &&
						 (*pucPrimaryCh <= 64))
						*pucS1 = 58;
					else if ((*pucPrimaryCh >= 100) &&
						 (*pucPrimaryCh <= 112))
						*pucS1 = 106;
					else if ((*pucPrimaryCh >= 116) &&
						 (*pucPrimaryCh <= 128))
						*pucS1 = 122;
					else if ((*pucPrimaryCh >= 132) &&
						 (*pucPrimaryCh <= 144))
						/* 160 downgrade should not in
						 * this case
						 */
						*pucS1 = 138;
					else if ((*pucPrimaryCh >= 149) &&
						 (*pucPrimaryCh <= 161))
						/* 160 downgrade should not in
						 * this case
						 */
						*pucS1 = 155;
					else
						DBGLOG(RLM, INFO,
						       "Check connect 160 downgrde (%d) case\n",
						       ucMaxBandwidth);

					DBGLOG(RLM, INFO,
					       "Decreasse the BW160 to BW80, shift S1 to (%d)\n",
					       *pucS1);
				}
			}
		}

		DBGLOG(RLM, INFO, "Modify ChannelWidth (%d) and Extend (%d)\n",
		       *peChannelWidth, *peExtend);
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Fill VHT Operation Information(VHT BW, S1, S2) by BSS operating
 *  channel width
 *
 * \param[in]
 *
 * \return none
 */
/*----------------------------------------------------------------------------*/
void rlmFillVhtOpInfoByBssOpBw(struct BSS_INFO *prBssInfo, uint8_t ucBssOpBw)
{
	ASSERT(prBssInfo);

	if (ucBssOpBw < MAX_BW_80MHZ || prBssInfo->eBand == BAND_2G4) {
		prBssInfo->ucVhtChannelWidth = VHT_OP_CHANNEL_WIDTH_20_40;
		prBssInfo->ucVhtChannelFrequencyS1 = 0;
		prBssInfo->ucVhtChannelFrequencyS2 = 0;
	} else if (ucBssOpBw == MAX_BW_80MHZ) {
		prBssInfo->ucVhtChannelWidth = VHT_OP_CHANNEL_WIDTH_80;
		prBssInfo->ucVhtChannelFrequencyS1 = nicGetVhtS1(
			prBssInfo->ucPrimaryChannel, VHT_OP_CHANNEL_WIDTH_80);
		prBssInfo->ucVhtChannelFrequencyS2 = 0;
	} else if (ucBssOpBw == MAX_BW_160MHZ) {
		prBssInfo->ucVhtChannelWidth = VHT_OP_CHANNEL_WIDTH_160;
		prBssInfo->ucVhtChannelFrequencyS1 = nicGetVhtS1(
			prBssInfo->ucPrimaryChannel, VHT_OP_CHANNEL_WIDTH_160);
		prBssInfo->ucVhtChannelFrequencyS2 = 0;
	} else {
		/* 4 TODO: / BW80+80 support */
		DBGLOG(RLM, INFO, "Unsupport BW setting, back to VHT20_40\n");

		prBssInfo->ucVhtChannelWidth = VHT_OP_CHANNEL_WIDTH_20_40;
		prBssInfo->ucVhtChannelFrequencyS1 = 0;
		prBssInfo->ucVhtChannelFrequencyS2 = 0;
	}
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
static uint8_t rlmRecIeInfoForClient(struct ADAPTER *prAdapter,
				     struct BSS_INFO *prBssInfo, uint8_t *pucIE,
				     uint16_t u2IELength)
{
	uint16_t u2Offset;
	struct STA_RECORD *prStaRec;
	struct IE_HT_CAP *prHtCap;
	struct IE_HT_OP *prHtOp;
	struct IE_OBSS_SCAN_PARAM *prObssScnParam;
	uint8_t ucERP, ucPrimaryChannel;
	struct WIFI_VAR *prWifiVar = &prAdapter->rWifiVar;
#if CFG_SUPPORT_QUIET && 0
	u_int8_t fgHasQuietIE = FALSE;
#endif
	u_int8_t IsfgHtCapChange = FALSE;

#if CFG_SUPPORT_802_11AC
	struct IE_VHT_OP *prVhtOp;
	struct IE_VHT_CAP *prVhtCap = NULL;
	struct IE_OP_MODE_NOTIFICATION *prOPNotif;
	uint8_t fgHasOPModeIE = FALSE;
	uint8_t fgHasNewOPModeIE = FALSE;
	uint8_t ucVhtOpModeChannelWidth = 0;
	uint8_t ucVhtOpModeRxNss = 0;
	uint8_t ucMaxBwAllowed;
	uint8_t ucInitVhtOpMode = 0;
#endif

#if CFG_SUPPORT_DFS
	u_int8_t fgHasWideBandIE = FALSE;
	u_int8_t fgHasSCOIE = FALSE;
	u_int8_t fgHasChannelSwitchIE = FALSE;
	u_int8_t fgNeedSwitchChannel = FALSE;
	uint8_t ucChannelAnnouncePri;
	enum ENUM_CHNL_EXT eChannelAnnounceSco;
	uint8_t ucChannelAnnounceChannelS1 = 0;
	uint8_t ucChannelAnnounceChannelS2 = 0;
	uint8_t ucChannelAnnounceVhtBw;
	struct IE_CHANNEL_SWITCH *prChannelSwitchAnnounceIE;
	struct IE_SECONDARY_OFFSET *prSecondaryOffsetIE;
	struct IE_WIDE_BAND_CHANNEL *prWideBandChannelIE;
#endif
	uint8_t *pucDumpIE;

	ASSERT(prAdapter);
	ASSERT(prBssInfo);
	ASSERT(pucIE);

	prStaRec = prBssInfo->prStaRecOfAP;
	if (!prStaRec)
		return 0;

	prBssInfo->fgUseShortPreamble = prBssInfo->fgIsShortPreambleAllowed;
	ucPrimaryChannel = 0;
	prObssScnParam = NULL;
	ucMaxBwAllowed = cnmGetBssMaxBw(prAdapter, prBssInfo->ucBssIndex);
	pucDumpIE = pucIE;

	/* Note: HT-related members in staRec may not be zero before, so
	 *       if following IE does not exist, they are still not zero.
	 *       These HT-related parameters are valid only when the
	 * corresponding
	 *       BssInfo supports 802.11n, i.e., RLM_NET_IS_11N()
	 */
	IE_FOR_EACH(pucIE, u2IELength, u2Offset)
	{
		switch (IE_ID(pucIE)) {
		case ELEM_ID_HT_CAP:
			if (!RLM_NET_IS_11N(prBssInfo) ||
			    IE_LEN(pucIE) != (sizeof(struct IE_HT_CAP) - 2))
				break;
			prHtCap = (struct IE_HT_CAP *)pucIE;
			prStaRec->ucMcsSet =
				prHtCap->rSupMcsSet.aucRxMcsBitmask[0];
			prStaRec->fgSupMcs32 =
				(prHtCap->rSupMcsSet.aucRxMcsBitmask[32 / 8] &
				 BIT(0))
					? TRUE
					: FALSE;

			kalMemCopy(
				prStaRec->aucRxMcsBitmask,
				prHtCap->rSupMcsSet.aucRxMcsBitmask,
				/*SUP_MCS_RX_BITMASK_OCTET_NUM */
				sizeof(prStaRec->aucRxMcsBitmask));

			prStaRec->u2RxHighestSupportedRate =
				prHtCap->rSupMcsSet.u2RxHighestSupportedRate;
			prStaRec->u4TxRateInfo =
				prHtCap->rSupMcsSet.u4TxRateInfo;

			if ((prStaRec->u2HtCapInfo &
			     HT_CAP_INFO_SM_POWER_SAVE) !=
			    (prHtCap->u2HtCapInfo & HT_CAP_INFO_SM_POWER_SAVE))
				/* Purpose : To detect SMPS change */
				IsfgHtCapChange = TRUE;

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
				prStaRec->u2HtCapInfo |=
					HT_CAP_INFO_SHORT_GI_20M;
				prStaRec->u2HtCapInfo |=
					HT_CAP_INFO_SHORT_GI_40M;
			} else if (IS_FEATURE_DISABLED(
					   prWifiVar->ucTxShortGI)) {
				prStaRec->u2HtCapInfo &=
					~HT_CAP_INFO_SHORT_GI_20M;
				prStaRec->u2HtCapInfo &=
					~HT_CAP_INFO_SHORT_GI_40M;
			}

			/* Set HT Greenfield Tx capability */
			if (IS_FEATURE_FORCE_ENABLED(prWifiVar->ucTxGf))
				prStaRec->u2HtCapInfo |= HT_CAP_INFO_HT_GF;
			else if (IS_FEATURE_DISABLED(prWifiVar->ucTxGf))
				prStaRec->u2HtCapInfo &= ~HT_CAP_INFO_HT_GF;

			prStaRec->ucAmpduParam = prHtCap->ucAmpduParam;
			prStaRec->u2HtExtendedCap = prHtCap->u2HtExtendedCap;
			prStaRec->u4TxBeamformingCap =
				prHtCap->u4TxBeamformingCap;
			prStaRec->ucAselCap = prHtCap->ucAselCap;
			break;

		case ELEM_ID_HT_OP:
			if (!RLM_NET_IS_11N(prBssInfo) ||
			    IE_LEN(pucIE) != (sizeof(struct IE_HT_OP) - 2))
				break;
			prHtOp = (struct IE_HT_OP *)pucIE;
			/* Workaround that some APs fill primary channel field
			 * by its
			 * secondary channel, but its DS IE is correct 20110610
			 */
			if (ucPrimaryChannel == 0)
				ucPrimaryChannel = prHtOp->ucPrimaryChannel;
			prBssInfo->ucHtOpInfo1 = prHtOp->ucInfo1;
			prBssInfo->u2HtOpInfo2 = prHtOp->u2Info2;
			prBssInfo->u2HtOpInfo3 = prHtOp->u2Info3;

			/*Backup peer HT OP Info*/
			prStaRec->ucHtPeerOpInfo1 = prHtOp->ucInfo1;

			if (!prBssInfo->fg40mBwAllowed)
				prBssInfo->ucHtOpInfo1 &=
					~(HT_OP_INFO1_SCO |
					  HT_OP_INFO1_STA_CHNL_WIDTH);

			if ((prBssInfo->ucHtOpInfo1 & HT_OP_INFO1_SCO) !=
			    CHNL_EXT_RES)
				prBssInfo->eBssSCO = (enum ENUM_CHNL_EXT)(
					prBssInfo->ucHtOpInfo1 &
					HT_OP_INFO1_SCO);

			/* Revise by own OP BW */
			if (prBssInfo->fgIsOpChangeChannelWidth &&
			    prBssInfo->ucOpChangeChannelWidth == MAX_BW_20MHZ) {
				prBssInfo->ucHtOpInfo1 &=
					~(HT_OP_INFO1_SCO |
					  HT_OP_INFO1_STA_CHNL_WIDTH);
				prBssInfo->eBssSCO = CHNL_EXT_SCN;
			}

			prBssInfo->eHtProtectMode = (enum ENUM_HT_PROTECT_MODE)(
				prBssInfo->u2HtOpInfo2 &
				HT_OP_INFO2_HT_PROTECTION);

			/* To do: process regulatory class 16 */
			if ((prBssInfo->u2HtOpInfo2 &
			     HT_OP_INFO2_OBSS_NON_HT_STA_PRESENT) &&
			    0 /* && regulatory class is 16 */)
				prBssInfo->eGfOperationMode =
					GF_MODE_DISALLOWED;
			else if (prBssInfo->u2HtOpInfo2 &
				 HT_OP_INFO2_NON_GF_HT_STA_PRESENT)
				prBssInfo->eGfOperationMode = GF_MODE_PROTECT;
			else
				prBssInfo->eGfOperationMode = GF_MODE_NORMAL;

			prBssInfo->eRifsOperationMode =
				(prBssInfo->ucHtOpInfo1 & HT_OP_INFO1_RIFS_MODE)
					? RIFS_MODE_NORMAL
					: RIFS_MODE_DISALLOWED;

			break;

#if CFG_SUPPORT_802_11AC
		case ELEM_ID_VHT_CAP:
			if (!RLM_NET_IS_11AC(prBssInfo) ||
			    IE_LEN(pucIE) != (sizeof(struct IE_VHT_CAP) - 2))
				break;

			prVhtCap = (struct IE_VHT_CAP *)pucIE;

			prStaRec->u4VhtCapInfo = prVhtCap->u4VhtCapInfo;
			/* Set Tx LDPC capability */
			if (IS_FEATURE_FORCE_ENABLED(prWifiVar->ucTxLdpc))
				prStaRec->u4VhtCapInfo |= VHT_CAP_INFO_RX_LDPC;
			else if (IS_FEATURE_DISABLED(prWifiVar->ucTxLdpc))
				prStaRec->u4VhtCapInfo &= ~VHT_CAP_INFO_RX_LDPC;

			/* Set Tx STBC capability */
			if (IS_FEATURE_FORCE_ENABLED(prWifiVar->ucTxStbc))
				prStaRec->u4VhtCapInfo |=
					VHT_CAP_INFO_RX_STBC_MASK;
			else if (IS_FEATURE_DISABLED(prWifiVar->ucTxStbc))
				prStaRec->u4VhtCapInfo &=
					~VHT_CAP_INFO_RX_STBC_MASK;

			/* Set Tx TXOP PS capability */
			if (IS_FEATURE_FORCE_ENABLED(prWifiVar->ucTxopPsTx))
				prStaRec->u4VhtCapInfo |=
					VHT_CAP_INFO_VHT_TXOP_PS;
			else if (IS_FEATURE_DISABLED(prWifiVar->ucTxopPsTx))
				prStaRec->u4VhtCapInfo &=
					~VHT_CAP_INFO_VHT_TXOP_PS;

			/* Set Tx Short GI capability */
			if (IS_FEATURE_FORCE_ENABLED(prWifiVar->ucTxShortGI)) {
				prStaRec->u4VhtCapInfo |=
					VHT_CAP_INFO_SHORT_GI_80;
				prStaRec->u4VhtCapInfo |=
					VHT_CAP_INFO_SHORT_GI_160_80P80;
			} else if (IS_FEATURE_DISABLED(
					   prWifiVar->ucTxShortGI)) {
				prStaRec->u4VhtCapInfo &=
					~VHT_CAP_INFO_SHORT_GI_80;
				prStaRec->u4VhtCapInfo &=
					~VHT_CAP_INFO_SHORT_GI_160_80P80;
			}

			prStaRec->u2VhtRxMcsMap =
				prVhtCap->rVhtSupportedMcsSet.u2RxMcsMap;
			prStaRec->u2VhtRxMcsMapAssoc =
				prStaRec->u2VhtRxMcsMap;

			prStaRec->u2VhtRxHighestSupportedDataRate =
				prVhtCap->rVhtSupportedMcsSet
					.u2RxHighestSupportedDataRate;
			prStaRec->u2VhtTxMcsMap =
				prVhtCap->rVhtSupportedMcsSet.u2TxMcsMap;
			prStaRec->u2VhtTxHighestSupportedDataRate =
				prVhtCap->rVhtSupportedMcsSet
					.u2TxHighestSupportedDataRate;

			break;

		case ELEM_ID_VHT_OP:
			if (!RLM_NET_IS_11AC(prBssInfo) ||
			    IE_LEN(pucIE) != (sizeof(struct IE_VHT_OP) - 2))
				break;

			prVhtOp = (struct IE_VHT_OP *)pucIE;

			/*Backup peer VHT OpInfo*/
			prStaRec->ucVhtOpChannelWidth =
				prVhtOp->ucVhtOperation[0];
			prStaRec->ucVhtOpChannelFrequencyS1 =
				prVhtOp->ucVhtOperation[1];
			prStaRec->ucVhtOpChannelFrequencyS2 =
				prVhtOp->ucVhtOperation[2];

			rlmModifyVhtBwPara(&prStaRec->ucVhtOpChannelFrequencyS1,
					   &prStaRec->ucVhtOpChannelFrequencyS2,
					   &prStaRec->ucVhtOpChannelWidth);

			prBssInfo->ucVhtChannelWidth =
				prVhtOp->ucVhtOperation[0];
			prBssInfo->ucVhtChannelFrequencyS1 =
				prVhtOp->ucVhtOperation[1];
			prBssInfo->ucVhtChannelFrequencyS2 =
				prVhtOp->ucVhtOperation[2];
			prBssInfo->u2VhtBasicMcsSet = prVhtOp->u2VhtBasicMcsSet;

			rlmModifyVhtBwPara(&prBssInfo->ucVhtChannelFrequencyS1,
					   &prBssInfo->ucVhtChannelFrequencyS2,
					   &prBssInfo->ucVhtChannelWidth);

			/* Revise by own OP BW if needed */
			if ((prBssInfo->fgIsOpChangeChannelWidth) &&
			    (rlmGetVhtOpBwByBssOpBw(
				     prBssInfo->ucOpChangeChannelWidth) <
			     prBssInfo->ucVhtChannelWidth)) {
				rlmFillVhtOpInfoByBssOpBw(
					prBssInfo,
					prBssInfo->ucOpChangeChannelWidth);
			}

			break;
		case ELEM_ID_OP_MODE:
			if (!RLM_NET_IS_11AC(prBssInfo) ||
			    IE_LEN(pucIE) !=
				    (sizeof(struct IE_OP_MODE_NOTIFICATION) -
				     2))
				break;
			prOPNotif = (struct IE_OP_MODE_NOTIFICATION *) pucIE;

			/* NOTE: An AP always sets this field to 0,
			 * so break it if this bit is set.
			 */
			if ((prOPNotif->ucOpMode & VHT_OP_MODE_RX_NSS_TYPE)
			    == VHT_OP_MODE_RX_NSS_TYPE) {
				break;
			}
			fgHasOPModeIE = TRUE;

			/* Same OP mode, no need to update.
			 * Let the further flow not to update VhtOpMode.
			 */
			if (prStaRec->ucVhtOpMode == prOPNotif->ucOpMode) {
				ucInitVhtOpMode = prStaRec->ucVhtOpMode;
				break;
			}

			fgHasNewOPModeIE = TRUE;
			prStaRec->ucVhtOpMode = prOPNotif->ucOpMode;
			ucVhtOpModeChannelWidth =
				(prOPNotif->ucOpMode &
				 VHT_OP_MODE_CHANNEL_WIDTH);
			ucVhtOpModeRxNss =
				(prOPNotif->ucOpMode & VHT_OP_MODE_RX_NSS)
				>> VHT_OP_MODE_RX_NSS_OFFSET;

			if (ucVhtOpModeRxNss == VHT_OP_MODE_NSS_2) {
				prStaRec->u2VhtRxMcsMap = BITS(0, 15) &
					(~(VHT_CAP_INFO_MCS_1SS_MASK |
					VHT_CAP_INFO_MCS_2SS_MASK));

				prStaRec->u2VhtRxMcsMap |=
					(prStaRec->u2VhtRxMcsMapAssoc &
					(VHT_CAP_INFO_MCS_1SS_MASK |
					VHT_CAP_INFO_MCS_2SS_MASK));
			} else {
				prStaRec->u2VhtRxMcsMap = BITS(0, 15) &
					(~VHT_CAP_INFO_MCS_1SS_MASK);

				prStaRec->u2VhtRxMcsMap |=
					(prStaRec->u2VhtRxMcsMapAssoc &
					VHT_CAP_INFO_MCS_1SS_MASK);
			}
			DBGLOG(RLM, INFO,
			       "NSS=%x RxMcsMap:0x%x, McsMapAssoc:0x%x\n",
			       ucVhtOpModeRxNss, prStaRec->u2VhtRxMcsMap,
			       prStaRec->u2VhtRxMcsMapAssoc);
			break;
#if CFG_SUPPORT_DFS
		case ELEM_ID_WIDE_BAND_CHANNEL_SWITCH:
			if (!RLM_NET_IS_11AC(prBssInfo) ||
			    IE_LEN(pucIE) !=
				    (sizeof(struct IE_WIDE_BAND_CHANNEL) - 2))
				break;
			DBGLOG(RLM, INFO,
			       "[Channel Switch] ELEM_ID_WIDE_BAND_CHANNEL_SWITCH, 11AC\n");
			prWideBandChannelIE =
				(struct IE_WIDE_BAND_CHANNEL *)pucIE;
			ucChannelAnnounceVhtBw =
				prWideBandChannelIE->ucNewChannelWidth;
			ucChannelAnnounceChannelS1 =
				prWideBandChannelIE->ucChannelS1;
			ucChannelAnnounceChannelS2 =
				prWideBandChannelIE->ucChannelS2;
			fgHasWideBandIE = TRUE;
			DBGLOG(RLM, INFO, "[Ch] BW=%d, s1=%d, s2=%d\n",
			       ucChannelAnnounceVhtBw,
			       ucChannelAnnounceChannelS1,
			       ucChannelAnnounceChannelS2);
			break;
#endif

#endif
		case ELEM_ID_20_40_BSS_COEXISTENCE:
			if (!RLM_NET_IS_11N(prBssInfo))
				break;
			/* To do: store if scanning exemption grant to BssInfo
			 */
			break;

		case ELEM_ID_OBSS_SCAN_PARAMS:
			if (!RLM_NET_IS_11N(prBssInfo) ||
			    IE_LEN(pucIE) !=
				    (sizeof(struct IE_OBSS_SCAN_PARAM) - 2))
				break;
			/* Store OBSS parameters to BssInfo */
			prObssScnParam = (struct IE_OBSS_SCAN_PARAM *)pucIE;
			break;

		case ELEM_ID_EXTENDED_CAP:
			if (!RLM_NET_IS_11N(prBssInfo))
				break;
			/* To do: store extended capability (PSMP, coexist) to
			 * BssInfo
			 */
			break;

		case ELEM_ID_ERP_INFO:
			if (IE_LEN(pucIE) != (sizeof(struct IE_ERP) - 2) ||
			    prBssInfo->eBand != BAND_2G4)
				break;
			ucERP = ERP_INFO_IE(pucIE)->ucERP;
			prBssInfo->fgErpProtectMode =
				(ucERP & ERP_INFO_USE_PROTECTION) ? TRUE
								  : FALSE;

			if (ucERP & ERP_INFO_BARKER_PREAMBLE_MODE)
				prBssInfo->fgUseShortPreamble = FALSE;
			break;

		case ELEM_ID_DS_PARAM_SET:
			if (IE_LEN(pucIE) == ELEM_MAX_LEN_DS_PARAMETER_SET)
				ucPrimaryChannel =
					DS_PARAM_IE(pucIE)->ucCurrChnl;
			break;
#if CFG_SUPPORT_DFS
		case ELEM_ID_CH_SW_ANNOUNCEMENT:
			if (IE_LEN(pucIE) !=
			    (sizeof(struct IE_CHANNEL_SWITCH) - 2))
				break;

			prChannelSwitchAnnounceIE =
				(struct IE_CHANNEL_SWITCH *)pucIE;

			DBGLOG(RLM, INFO, "[Ch] Count=%d\n",
			       prChannelSwitchAnnounceIE->ucChannelSwitchCount);

			if (prChannelSwitchAnnounceIE
						->ucChannelSwitchMode == 1) {
				/* Need to stop data transmission immediately */
				fgHasChannelSwitchIE = TRUE;
				if (!g_fgHasStopTx) {
					g_fgHasStopTx = TRUE;
					/* AP */
					qmSetStaRecTxAllowed(prAdapter,
								 prStaRec,
								 FALSE);
					DBGLOG(RLM, EVENT,
						"[Ch] TxAllowed = FALSE\n");
				}

				if (prChannelSwitchAnnounceIE
					    ->ucChannelSwitchCount <= 5) {
					DBGLOG(RLM, INFO,
					       "[Ch] switch channel [%d]->[%d]\n",
					       prBssInfo->ucPrimaryChannel,
					       prChannelSwitchAnnounceIE
						       ->ucNewChannelNum);
					ucChannelAnnouncePri =
						prChannelSwitchAnnounceIE
							->ucNewChannelNum;
					fgNeedSwitchChannel = TRUE;
#ifdef CFG_DFS_CHSW_FORCE_BW20
					g_fgHasChannelSwitchIE = TRUE;
#endif
				}
#ifdef CFG_DFS_CHSW_FORCE_BW20
				if (RLM_NET_IS_11AC(prBssInfo)) {
					DBGLOG(RLM, INFO,
					       "Send Operation Action Frame");
					rlmSendOpModeNotificationFrame(
						prAdapter, prStaRec,
						VHT_OP_MODE_CHANNEL_WIDTH_20,
						1);
				} else {
					DBGLOG(RLM, INFO,
					       "Skip Send Operation Action Frame");
				}
#endif
			}

			break;
		case ELEM_ID_SCO:
			if (IE_LEN(pucIE) !=
			    (sizeof(struct IE_SECONDARY_OFFSET) - 2))
				break;

			prSecondaryOffsetIE =
				(struct IE_SECONDARY_OFFSET *)pucIE;
			DBGLOG(RLM, INFO, "[Channel Switch] SCO [%d]->[%d]\n",
			       prBssInfo->eBssSCO,
			       prSecondaryOffsetIE->ucSecondaryOffset);
			eChannelAnnounceSco =
				(enum ENUM_CHNL_EXT)
					prSecondaryOffsetIE->ucSecondaryOffset;
			fgHasSCOIE = TRUE;
			break;
#endif

#if CFG_SUPPORT_QUIET && 0
		/* Note: RRM code should be moved to independent RRM function by
		 *       component design rule. But we attach it to RLM
		 * temporarily
		 */
		case ELEM_ID_QUIET:
			rrmQuietHandleQuietIE(prBssInfo,
					      (struct IE_QUIET *)pucIE);
			fgHasQuietIE = TRUE;
			break;
#endif

#if (CFG_SUPPORT_802_11AX == 1)
		case ELEM_ID_RESERVED:
			if (fgEfuseCtrlAxOn == 1) {
			if (IE_ID_EXT(pucIE) == ELEM_EXT_ID_HE_CAP)
				heRlmRecHeCapInfo(prAdapter, prStaRec, pucIE);
			else if (IE_ID_EXT(pucIE) == ELEM_EXT_ID_HE_OP)
				heRlmRecHeOperation(prAdapter,
							prBssInfo, pucIE);
			}
			break;
#endif

		default:
			break;
		} /* end of switch */
	}	 /* end of IE_FOR_EACH */

	if (IsfgHtCapChange && (prStaRec->ucStaState == STA_STATE_3))
		cnmStaSendUpdateCmd(prAdapter, prStaRec, NULL, FALSE);

	/* Some AP will have wrong channel number (255) when running time.
	 * Check if correct channel number information. 20110501
	 */
	if ((prBssInfo->eBand == BAND_2G4 && ucPrimaryChannel > 14) ||
	    (prBssInfo->eBand != BAND_2G4 &&
	     (ucPrimaryChannel >= 200 || ucPrimaryChannel <= 14)))
		ucPrimaryChannel = 0;
#if CFG_SUPPORT_802_11AC
	/* Check whether the Operation Mode IE is exist or not.
	 *  If exists, then the channel bandwidth of VHT operation field  is
	 * changed
	 *  with the channel bandwidth setting of Operation Mode field.
	 *  The channel bandwidth of OP Mode IE  is  0, represent as 20MHz.
	 *  The channel bandwidth of OP Mode IE  is  1, represent as 40MHz.
	 *  The channel bandwidth of OP Mode IE  is  2, represent as 80MHz.
	 *  The channel bandwidth of OP Mode IE  is  3, represent as
	 * 160/80+80MHz.
	 */
	if (fgHasNewOPModeIE == TRUE) {
		if (prStaRec->ucStaState == STA_STATE_3) {
			/* 1. Modify channel width parameters */
			rlmRecOpModeBwForClient(ucVhtOpModeChannelWidth,
						prBssInfo);

			/* 2. Update StaRec to FW (BssInfo will be updated after
			 * return from this function)
			 */
			DBGLOG(RLM, INFO,
			       "Update OpMode to 0x%x, to FW due to OpMode Notificaition",
			       prStaRec->ucVhtOpMode);
			cnmStaSendUpdateCmd(prAdapter, prStaRec, NULL, FALSE);

			/* 3. Revise by own OP BW if needed */
			if ((prBssInfo->fgIsOpChangeChannelWidth)) {
				/* VHT */
				if (rlmGetVhtOpBwByBssOpBw(
					    prBssInfo->ucOpChangeChannelWidth) <
				    prBssInfo->ucVhtChannelWidth)
					rlmFillVhtOpInfoByBssOpBw(
					prBssInfo,
					prBssInfo
					->ucOpChangeChannelWidth);
				/* HT */
				if (prBssInfo->fgIsOpChangeChannelWidth &&
				    prBssInfo->ucOpChangeChannelWidth ==
					    MAX_BW_20MHZ) {
					prBssInfo->ucHtOpInfo1 &=
						~(HT_OP_INFO1_SCO |
						  HT_OP_INFO1_STA_CHNL_WIDTH);
					prBssInfo->eBssSCO = CHNL_EXT_SCN;
				}
			}
		}
	} else { /* Set Default if the VHT OP mode field is not present */
		if (!fgHasOPModeIE) {
			ucInitVhtOpMode |=
				rlmGetOpModeBwByVhtAndHtOpInfo(prBssInfo);
			ucInitVhtOpMode |=
				((rlmGetSupportRxNssInVhtCap(prVhtCap) - 1)
				<< VHT_OP_MODE_RX_NSS_OFFSET) &
				VHT_OP_MODE_RX_NSS;
		}
		if ((prStaRec->ucVhtOpMode != ucInitVhtOpMode) &&
			(prStaRec->ucStaState == STA_STATE_3)) {
			prStaRec->ucVhtOpMode = ucInitVhtOpMode;
			DBGLOG(RLM, INFO, "Update OpMode to 0x%x",
			       prStaRec->ucVhtOpMode);
			DBGLOG(RLM, INFO,
			       "to FW due to NO OpMode Notificaition\n");
			cnmStaSendUpdateCmd(prAdapter, prStaRec, NULL, FALSE);
		} else
			prStaRec->ucVhtOpMode = ucInitVhtOpMode;
	}
#endif

#if CFG_SUPPORT_DFS
	/* Check whether Channel Announcement IE, Secondary Offset IE &
	 * Wide Bandwidth Channel Switch IE exist or not. If exist, the
	 * priority is
	 * the highest.
	 */

	if (fgNeedSwitchChannel) {
		struct BSS_DESC *prBssDesc;
		struct PARAM_SSID rSsid;

		prBssInfo->ucPrimaryChannel = ucChannelAnnouncePri;
		prBssInfo->eBand =
			(prBssInfo->ucPrimaryChannel <= 14)
				? BAND_2G4
				: BAND_5G;
		/* Change to BW20 for certification issue due to signal sidelope
		 * leakage
		 */
		prBssInfo->ucVhtChannelWidth = 0;
		prBssInfo->ucVhtChannelFrequencyS1 = 0;
		prBssInfo->ucVhtChannelFrequencyS2 = 0;
		prBssInfo->eBssSCO = 0;

		if (fgHasWideBandIE != FALSE) {
			prBssInfo->ucVhtChannelWidth = ucChannelAnnounceVhtBw;
			prBssInfo->ucVhtChannelFrequencyS1 =
				ucChannelAnnounceChannelS1;
			prBssInfo->ucVhtChannelFrequencyS2 =
				ucChannelAnnounceChannelS2;

			/* Revise by own OP BW if needed */
			if ((prBssInfo->fgIsOpChangeChannelWidth) &&
			    (rlmGetVhtOpBwByBssOpBw(
				     prBssInfo->ucOpChangeChannelWidth) <
			     prBssInfo->ucVhtChannelWidth)) {

				DBGLOG(RLM, LOUD,
				       "Change to w:%d s1:%d s2:%d since own changed BW < peer's WideBand BW",
				       prBssInfo->ucVhtChannelWidth,
				       prBssInfo->ucVhtChannelFrequencyS1,
				       prBssInfo->ucVhtChannelFrequencyS2);
				rlmFillVhtOpInfoByBssOpBw(
					prBssInfo,
					prBssInfo->ucOpChangeChannelWidth);
			}
		}
		if (fgHasSCOIE != FALSE)
			prBssInfo->eBssSCO = eChannelAnnounceSco;

		COPY_SSID(rSsid.aucSsid, rSsid.u4SsidLen, prBssInfo->aucSSID,
			  prBssInfo->ucSSIDLen);
		prBssDesc = scanSearchBssDescByBssidAndSsid(
			prAdapter, prBssInfo->aucBSSID, TRUE, &rSsid);

		if (prBssDesc) {
			DBGLOG(RLM, INFO,
			       "DFS: BSS: " MACSTR
			       " Desc found, channel from %u to %u\n ",
			       MAC2STR(prBssInfo->aucBSSID),
			       prBssDesc->ucChannelNum, ucChannelAnnouncePri);
			prBssDesc->ucChannelNum = ucChannelAnnouncePri;
			prBssDesc->eChannelWidth = prBssInfo->ucVhtChannelWidth;
			prBssDesc->ucCenterFreqS1 =
				prBssInfo->ucVhtChannelFrequencyS1;
			prBssDesc->ucCenterFreqS2 =
				prBssInfo->ucVhtChannelFrequencyS2;
			kalIndicateChannelSwitch(
				prAdapter->prGlueInfo,
				prBssInfo->eBssSCO,
				prBssDesc->ucChannelNum);
		} else {
			DBGLOG(RLM, INFO,
			       "DFS: BSS: " MACSTR " Desc is not found\n ",
			       MAC2STR(prBssInfo->aucBSSID));
		}
	}
#endif

	if (!fgHasChannelSwitchIE && g_fgHasStopTx) {
		/* AP */
		qmSetStaRecTxAllowed(prAdapter, prStaRec, TRUE);

		DBGLOG(RLM, EVENT, "[Ch] TxAllowed = TRUE\n");
		g_fgHasStopTx = FALSE;
	}

#if CFG_SUPPORT_DFS
#ifdef CFG_DFS_CHSW_FORCE_BW20
	/*DFS Certification for Channel Bandwidth 20MHz */
	DBGLOG(RLM, INFO, "Ch : SwitchIE = %d\n", g_fgHasChannelSwitchIE);
	if (g_fgHasChannelSwitchIE == TRUE) {
		prBssInfo->eBssSCO = CHNL_EXT_SCN;
		prBssInfo->ucVhtChannelWidth = CW_20_40MHZ;
		prBssInfo->ucVhtChannelFrequencyS1 = 0;
		prBssInfo->ucVhtChannelFrequencyS2 = 255;
		prBssInfo->ucHtOpInfo1 &=
			~(HT_OP_INFO1_SCO | HT_OP_INFO1_STA_CHNL_WIDTH);
		DBGLOG(RLM, INFO, "Ch : DFS has Appeared\n");
	}
#endif
#endif
	rlmReviseMaxBw(prAdapter, prBssInfo->ucBssIndex, &prBssInfo->eBssSCO,
		       (enum ENUM_CHANNEL_WIDTH *)&prBssInfo->ucVhtChannelWidth,
		       &prBssInfo->ucVhtChannelFrequencyS1,
		       &prBssInfo->ucPrimaryChannel);

	rlmRevisePreferBandwidthNss(prAdapter, prBssInfo->ucBssIndex, prStaRec);

	/* printk("Modify ChannelWidth (%d) and Extend
	 * (%d)\n",prBssInfo->eBssSCO,
	 * prBssInfo->ucVhtChannelWidth);
	 */

	if (!rlmDomainIsValidRfSetting(
		    prAdapter, prBssInfo->eBand, prBssInfo->ucPrimaryChannel,
		    prBssInfo->eBssSCO, prBssInfo->ucVhtChannelWidth,
		    prBssInfo->ucVhtChannelFrequencyS1,
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
		prBssInfo->ucHtOpInfo1 &=
			~(HT_OP_INFO1_SCO | HT_OP_INFO1_STA_CHNL_WIDTH);

		/* Check SAP channel */
		p2pFuncSwitchSapChannel(prAdapter);

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
		if (prObssScnParam->u2TriggerScanInterval <
		    OBSS_SCAN_MIN_INTERVAL)
			prObssScnParam->u2TriggerScanInterval =
				OBSS_SCAN_MIN_INTERVAL;
		if (prBssInfo->u2ObssScanInterval !=
		    prObssScnParam->u2TriggerScanInterval) {

			prBssInfo->u2ObssScanInterval =
				prObssScnParam->u2TriggerScanInterval;

			/* Start timer to trigger OBSS scanning */
			cnmTimerStartTimer(
				prAdapter, &prBssInfo->rObssScanTimer,
				prBssInfo->u2ObssScanInterval * MSEC_PER_SEC);
		}
	}

	return ucPrimaryChannel;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Update parameters from channel width field in OP Mode IE/action frame
 *
 * \param[in]
 *
 * \return none
 */
/*----------------------------------------------------------------------------*/
static void rlmRecOpModeBwForClient(uint8_t ucVhtOpModeChannelWidth,
				    struct BSS_INFO *prBssInfo)
{

	struct STA_RECORD *prStaRec = NULL;

	if (!prBssInfo)
		return;

	prStaRec = prBssInfo->prStaRecOfAP;
	if (!prStaRec)
		return;

	switch (ucVhtOpModeChannelWidth) {
	case VHT_OP_MODE_CHANNEL_WIDTH_20:
		prBssInfo->ucVhtChannelWidth = VHT_OP_CHANNEL_WIDTH_20_40;
		prBssInfo->ucHtOpInfo1 &= ~HT_OP_INFO1_STA_CHNL_WIDTH;
		prStaRec->u2HtCapInfo &= ~HT_CAP_INFO_SUP_CHNL_WIDTH;

#if CFG_OPMODE_CONFLICT_OPINFO
		if (prBssInfo->eBssSCO != CHNL_EXT_SCN) {
			DBGLOG(RLM, WARN,
			       "HT_OP_Info != OPmode_Notifify, follow OPmode_Notify to BW20.\n");
			prBssInfo->eBssSCO = CHNL_EXT_SCN;
		}
#endif
		break;
	case VHT_OP_MODE_CHANNEL_WIDTH_40:
		prBssInfo->ucVhtChannelWidth = VHT_OP_CHANNEL_WIDTH_20_40;
		prBssInfo->ucHtOpInfo1 |= HT_OP_INFO1_STA_CHNL_WIDTH;
		prStaRec->u2HtCapInfo |= HT_CAP_INFO_SUP_CHNL_WIDTH;

#if CFG_OPMODE_CONFLICT_OPINFO
		if (prBssInfo->eBssSCO == CHNL_EXT_SCN) {
			prBssInfo->ucHtOpInfo1 &= ~HT_OP_INFO1_STA_CHNL_WIDTH;
			prStaRec->u2HtCapInfo &= ~HT_CAP_INFO_SUP_CHNL_WIDTH;
			DBGLOG(RLM, WARN,
			       "HT_OP_Info != OPmode_Notifify, follow HT_OP_Info to BW20.\n");
		}
#endif
		break;
	case VHT_OP_MODE_CHANNEL_WIDTH_80:
#if CFG_OPMODE_CONFLICT_OPINFO
		if (prBssInfo->ucVhtChannelWidth !=
		    VHT_OP_MODE_CHANNEL_WIDTH_80) {
			DBGLOG(RLM, WARN,
			       "VHT_OP != OPmode:%d, follow VHT_OP to VHT_OP:%d HT_OP:%d\n",
			       ucVhtOpModeChannelWidth,
			       prBssInfo->ucVhtChannelWidth,
			       (uint8_t)(prBssInfo->ucHtOpInfo1 &
					 HT_OP_INFO1_STA_CHNL_WIDTH) >>
				       HT_OP_INFO1_STA_CHNL_WIDTH_OFFSET);
		} else
#endif
		{
			prBssInfo->ucVhtChannelWidth = VHT_OP_CHANNEL_WIDTH_80;
			prBssInfo->ucHtOpInfo1 |= HT_OP_INFO1_STA_CHNL_WIDTH;
			prStaRec->u2HtCapInfo |= HT_CAP_INFO_SUP_CHNL_WIDTH;
		}
		break;
	case VHT_OP_MODE_CHANNEL_WIDTH_160_80P80:
/* Determine BW160 or BW80+BW80 by VHT OP Info */
#if CFG_OPMODE_CONFLICT_OPINFO
		if ((prBssInfo->ucVhtChannelWidth !=
		     VHT_OP_CHANNEL_WIDTH_160) &&
		    (prBssInfo->ucVhtChannelWidth !=
		     VHT_OP_CHANNEL_WIDTH_80P80)) {
			DBGLOG(RLM, WARN,
			       "VHT_OP != OPmode:%d, follow VHT_OP to VHT_OP:%d HT_OP:%d\n",
			       ucVhtOpModeChannelWidth,
			       prBssInfo->ucVhtChannelWidth,
			       (uint8_t)(prBssInfo->ucHtOpInfo1 &
					 HT_OP_INFO1_STA_CHNL_WIDTH) >>
				       HT_OP_INFO1_STA_CHNL_WIDTH_OFFSET);
		} else
#endif
		{
			prBssInfo->ucHtOpInfo1 |= HT_OP_INFO1_STA_CHNL_WIDTH;
			prStaRec->u2HtCapInfo |= HT_CAP_INFO_SUP_CHNL_WIDTH;
		}
		break;
	default:
		break;
	}
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
static void rlmRecAssocRespIeInfoForClient(struct ADAPTER *prAdapter,
					   struct BSS_INFO *prBssInfo,
					   uint8_t *pucIE, uint16_t u2IELength)
{
	uint16_t u2Offset;
	struct STA_RECORD *prStaRec;
	u_int8_t fgIsHasHtCap = FALSE;
	u_int8_t fgIsHasVhtCap = FALSE;
#if (CFG_SUPPORT_802_11AX == 1)
	u_int8_t fgIsHasHeCap = FALSE;
#endif
	struct BSS_DESC *prBssDesc;
	struct PARAM_SSID rSsid;

	ASSERT(prAdapter);
	ASSERT(prBssInfo);
	ASSERT(pucIE);

	prStaRec = prBssInfo->prStaRecOfAP;

	if (!prStaRec)
		return;
	COPY_SSID(rSsid.aucSsid, rSsid.u4SsidLen, prBssInfo->aucSSID,
		  prBssInfo->ucSSIDLen);
	prBssDesc = scanSearchBssDescByBssidAndSsid(
		prAdapter, prStaRec->aucMacAddr, TRUE, &rSsid);

	IE_FOR_EACH(pucIE, u2IELength, u2Offset)
	{
		switch (IE_ID(pucIE)) {
		case ELEM_ID_HT_CAP:
			if (!RLM_NET_IS_11N(prBssInfo) ||
			    IE_LEN(pucIE) != (sizeof(struct IE_HT_CAP) - 2))
				break;
			fgIsHasHtCap = TRUE;
			break;
#if CFG_SUPPORT_802_11AC
		case ELEM_ID_VHT_CAP:
			if (!RLM_NET_IS_11AC(prBssInfo) ||
			    IE_LEN(pucIE) != (sizeof(struct IE_VHT_CAP) - 2))
				break;
			fgIsHasVhtCap = TRUE;
			break;
#endif
#if (CFG_SUPPORT_802_11AX == 1)
		case ELEM_ID_RESERVED:
			if (fgEfuseCtrlAxOn == 1) {
			if (IE_ID_EXT(pucIE) != ELEM_EXT_ID_HE_CAP ||
			    !RLM_NET_IS_11AX(prBssInfo))
				break;
			fgIsHasHeCap = TRUE;
			}
			break;
#endif

		default:
			break;
		} /* end of switch */
	}	 /* end of IE_FOR_EACH */

	if (!fgIsHasHtCap) {
		prStaRec->ucDesiredPhyTypeSet &= ~PHY_TYPE_BIT_HT;
		if (prBssDesc) {
			if (prBssDesc->ucPhyTypeSet & PHY_TYPE_BIT_HT) {
				DBGLOG(RLM, WARN,
				       "PhyTypeSet in Beacon and AssocResp are unsync. ");
				DBGLOG(RLM, WARN,
				       "Follow AssocResp to disable HT.\n");
			}
		}
	}
	if (!fgIsHasVhtCap) {
		prStaRec->ucDesiredPhyTypeSet &= ~PHY_TYPE_BIT_VHT;
		if (prBssDesc) {
			if (prBssDesc->ucPhyTypeSet & PHY_TYPE_BIT_VHT) {
				DBGLOG(RLM, WARN,
				       "PhyTypeSet in Beacon and AssocResp are unsync. ");
				DBGLOG(RLM, WARN,
				       "Follow AssocResp to disable VHT.\n");
			}
		}
	}
#if (CFG_SUPPORT_802_11AX == 1)
	if (fgEfuseCtrlAxOn == 1) {
	if (!fgIsHasHeCap) {
		prStaRec->ucDesiredPhyTypeSet &= ~PHY_TYPE_BIT_HE;
		if (prBssDesc) {
			if (prBssDesc->ucPhyTypeSet & PHY_TYPE_BIT_HE) {
				DBGLOG(RLM, WARN, "PhyTypeSet are unsync. ");
				DBGLOG(RLM, WARN, "Disable HE per assoc.\n");
			}
		}
	}
	}
#endif
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
static u_int8_t rlmRecBcnFromNeighborForClient(struct ADAPTER *prAdapter,
					       struct BSS_INFO *prBssInfo,
					       struct SW_RFB *prSwRfb,
					       uint8_t *pucIE,
					       uint16_t u2IELength)
{
	uint16_t u2Offset, i;
	uint8_t ucPriChannel, ucSecChannel;
	enum ENUM_CHNL_EXT eSCO;
	u_int8_t fgHtBss, fg20mReq;
	enum ENUM_BAND eBand = 0;
	struct RX_DESC_OPS_T *prRxDescOps;

	ASSERT(prAdapter);
	ASSERT(prBssInfo && prSwRfb);
	ASSERT(pucIE);
	prRxDescOps = prAdapter->chip_info->prRxDescOps;

	/* Record it to channel list to change 20/40 bandwidth */
	ucPriChannel = 0;
	eSCO = CHNL_EXT_SCN;

	fgHtBss = FALSE;
	fg20mReq = FALSE;

	IE_FOR_EACH(pucIE, u2IELength, u2Offset)
	{
		switch (IE_ID(pucIE)) {
		case ELEM_ID_HT_CAP: {
			struct IE_HT_CAP *prHtCap;

			if (IE_LEN(pucIE) != (sizeof(struct IE_HT_CAP) - 2))
				break;

			prHtCap = (struct IE_HT_CAP *)pucIE;
			if (prHtCap->u2HtCapInfo & HT_CAP_INFO_40M_INTOLERANT)
				fg20mReq = TRUE;
			fgHtBss = TRUE;
			break;
		}
		case ELEM_ID_HT_OP: {
			struct IE_HT_OP *prHtOp;

			if (IE_LEN(pucIE) != (sizeof(struct IE_HT_OP) - 2))
				break;

			prHtOp = (struct IE_HT_OP *)pucIE;
			/* Workaround that some APs fill primary channel field
			 * by its
			 * secondary channel, but its DS IE is correct 20110610
			 */
			if (ucPriChannel == 0)
				ucPriChannel = prHtOp->ucPrimaryChannel;

			if ((prHtOp->ucInfo1 & HT_OP_INFO1_SCO) != CHNL_EXT_RES)
				eSCO = (enum ENUM_CHNL_EXT)(prHtOp->ucInfo1 &
							    HT_OP_INFO1_SCO);
			break;
		}
		case ELEM_ID_20_40_BSS_COEXISTENCE: {
			struct IE_20_40_COEXIST *prCoexist;

			if (IE_LEN(pucIE) !=
			    (sizeof(struct IE_20_40_COEXIST) - 2))
				break;

			prCoexist = (struct IE_20_40_COEXIST *)pucIE;
			if (prCoexist->ucData & BSS_COEXIST_40M_INTOLERANT)
				fg20mReq = TRUE;
			break;
		}
		case ELEM_ID_DS_PARAM_SET:
			if (IE_LEN(pucIE) !=
			    (sizeof(struct IE_DS_PARAM_SET) - 2))
				break;
			ucPriChannel = DS_PARAM_IE(pucIE)->ucCurrChnl;
			break;

		default:
			break;
		}
	}

	/* To do: Update channel list and 5G band. All channel lists have the
	 * same
	 * update procedure. We should give it the entry pointer of desired
	 * channel list.
	 */
	RX_STATUS_GET(prRxDescOps, eBand, get_rf_band, prSwRfb->prRxStatus);
	if (eBand != BAND_2G4)
		return FALSE;

	if (ucPriChannel == 0 || ucPriChannel > 14) {
		RX_STATUS_GET(
			prRxDescOps,
			ucPriChannel,
			get_ch_num,
			prSwRfb->prRxStatus);
	}

	if (fgHtBss) {
		ASSERT(prBssInfo->auc2G_PriChnlList[0] <= CHNL_LIST_SZ_2G);
		for (i = 1; i <= prBssInfo->auc2G_PriChnlList[0] &&
			    i <= CHNL_LIST_SZ_2G;
		     i++) {
			if (prBssInfo->auc2G_PriChnlList[i] == ucPriChannel)
				break;
		}
		if ((i > prBssInfo->auc2G_PriChnlList[0]) &&
		    (i <= CHNL_LIST_SZ_2G)) {
			prBssInfo->auc2G_PriChnlList[i] = ucPriChannel;
			prBssInfo->auc2G_PriChnlList[0]++;
		}

		/* Update secondary channel */
		if (eSCO != CHNL_EXT_SCN) {
			ucSecChannel = (eSCO == CHNL_EXT_SCA)
					       ? (ucPriChannel + 4)
					       : (ucPriChannel - 4);

			ASSERT(prBssInfo->auc2G_SecChnlList[0] <=
			       CHNL_LIST_SZ_2G);
			for (i = 1; i <= prBssInfo->auc2G_SecChnlList[0] &&
				    i <= CHNL_LIST_SZ_2G;
			     i++) {
				if (prBssInfo->auc2G_SecChnlList[i] ==
				    ucSecChannel)
					break;
			}
			if ((i > prBssInfo->auc2G_SecChnlList[0]) &&
			    (i <= CHNL_LIST_SZ_2G)) {
				prBssInfo->auc2G_SecChnlList[i] = ucSecChannel;
				prBssInfo->auc2G_SecChnlList[0]++;
			}
		}

		/* Update 20M bandwidth request channels */
		if (fg20mReq) {
			ASSERT(prBssInfo->auc2G_20mReqChnlList[0] <=
			       CHNL_LIST_SZ_2G);
			for (i = 1; i <= prBssInfo->auc2G_20mReqChnlList[0] &&
				    i <= CHNL_LIST_SZ_2G;
			     i++) {
				if (prBssInfo->auc2G_20mReqChnlList[i] ==
				    ucPriChannel)
					break;
			}
			if ((i > prBssInfo->auc2G_20mReqChnlList[0]) &&
			    (i <= CHNL_LIST_SZ_2G)) {
				prBssInfo->auc2G_20mReqChnlList[i] =
					ucPriChannel;
				prBssInfo->auc2G_20mReqChnlList[0]++;
			}
		}
	} else {
		/* Update non-HT channel list */
		ASSERT(prBssInfo->auc2G_NonHtChnlList[0] <= CHNL_LIST_SZ_2G);
		for (i = 1; i <= prBssInfo->auc2G_NonHtChnlList[0] &&
			    i <= CHNL_LIST_SZ_2G;
		     i++) {
			if (prBssInfo->auc2G_NonHtChnlList[i] == ucPriChannel)
				break;
		}
		if ((i > prBssInfo->auc2G_NonHtChnlList[0]) &&
		    (i <= CHNL_LIST_SZ_2G)) {
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
static u_int8_t rlmRecBcnInfoForClient(struct ADAPTER *prAdapter,
				       struct BSS_INFO *prBssInfo,
				       struct SW_RFB *prSwRfb, uint8_t *pucIE,
				       uint16_t u2IELength)
{
	/* For checking if syncing params are different from
	 * last syncing and need to sync again
	 */
	struct CMD_SET_BSS_RLM_PARAM rBssRlmParam;
	u_int8_t fgNewParameter = FALSE;

	ASSERT(prAdapter);
	ASSERT(prBssInfo && prSwRfb);
	ASSERT(pucIE);

#if 0 /* SW migration 2010/8/20 */
	/* Note: we shall not update parameters when scanning, otherwise
	 * channel and bandwidth will not be correct or asserted failure
	 * during scanning.
	 * Note: remove channel checking. All received Beacons should be
	 * processed if measurement or other actions are executed in adjacent
	 * channels and Beacon content checking mechanism is not disabled.
	 */
	if (IS_SCAN_ACTIVE()
	    /* || prBssInfo->ucPrimaryChannel != CHNL_NUM_BY_SWRFB(prSwRfb) */
	    ) {
		return FALSE;
	}
#endif

	/* Handle change of slot time */
	prBssInfo->u2CapInfo =
		((struct WLAN_BEACON_FRAME *)(prSwRfb->pvHeader))->u2CapInfo;
	prBssInfo->fgUseShortSlotTime =
		((prBssInfo->u2CapInfo & CAP_INFO_SHORT_SLOT_TIME) ||
		 (prBssInfo->eBand != BAND_2G4))
			? TRUE
			: FALSE;

	/* Check if syncing params are different from last syncing and need to
	 * sync again
	 * If yes, return TRUE and sync with FW; Otherwise, return FALSE.
	 */
	rBssRlmParam.ucRfBand = (u_int8_t)prBssInfo->eBand;
	rBssRlmParam.ucPrimaryChannel = prBssInfo->ucPrimaryChannel;
	rBssRlmParam.ucRfSco = (u_int8_t)prBssInfo->eBssSCO;
	rBssRlmParam.ucErpProtectMode = (u_int8_t)prBssInfo->fgErpProtectMode;
	rBssRlmParam.ucHtProtectMode = (u_int8_t)prBssInfo->eHtProtectMode;
	rBssRlmParam.ucGfOperationMode = (u_int8_t)prBssInfo->eGfOperationMode;
	rBssRlmParam.ucTxRifsMode = (u_int8_t)prBssInfo->eRifsOperationMode;
	rBssRlmParam.u2HtOpInfo3 = prBssInfo->u2HtOpInfo3;
	rBssRlmParam.u2HtOpInfo2 = prBssInfo->u2HtOpInfo2;
	rBssRlmParam.ucHtOpInfo1 = prBssInfo->ucHtOpInfo1;
	rBssRlmParam.ucUseShortPreamble = prBssInfo->fgUseShortPreamble;
	rBssRlmParam.ucUseShortSlotTime = prBssInfo->fgUseShortSlotTime;
	rBssRlmParam.ucVhtChannelWidth = prBssInfo->ucVhtChannelWidth;
	rBssRlmParam.ucVhtChannelFrequencyS1 =
		prBssInfo->ucVhtChannelFrequencyS1;
	rBssRlmParam.ucVhtChannelFrequencyS2 =
		prBssInfo->ucVhtChannelFrequencyS2;
	rBssRlmParam.u2VhtBasicMcsSet = prBssInfo->u2VhtBasicMcsSet;
	rBssRlmParam.ucRxNss = prBssInfo->ucOpRxNss;
	rBssRlmParam.ucTxNss = prBssInfo->ucOpTxNss;

	rlmRecIeInfoForClient(prAdapter, prBssInfo, pucIE, u2IELength);

	if (rBssRlmParam.ucRfBand != prBssInfo->eBand ||
		rBssRlmParam.ucPrimaryChannel != prBssInfo->ucPrimaryChannel ||
		rBssRlmParam.ucRfSco != prBssInfo->eBssSCO ||
		rBssRlmParam.ucErpProtectMode != prBssInfo->fgErpProtectMode ||
		rBssRlmParam.ucHtProtectMode != prBssInfo->eHtProtectMode ||
		rBssRlmParam.ucGfOperationMode != prBssInfo->eGfOperationMode ||
		rBssRlmParam.ucTxRifsMode != prBssInfo->eRifsOperationMode ||
		rBssRlmParam.u2HtOpInfo3 != prBssInfo->u2HtOpInfo3 ||
		rBssRlmParam.u2HtOpInfo2 != prBssInfo->u2HtOpInfo2 ||
		rBssRlmParam.ucHtOpInfo1 != prBssInfo->ucHtOpInfo1 ||
		rBssRlmParam.ucUseShortPreamble !=
			prBssInfo->fgUseShortPreamble ||
		rBssRlmParam.ucUseShortSlotTime !=
			prBssInfo->fgUseShortSlotTime ||
		rBssRlmParam.ucVhtChannelWidth !=
			prBssInfo->ucVhtChannelWidth ||
		rBssRlmParam.ucVhtChannelFrequencyS1 !=
			prBssInfo->ucVhtChannelFrequencyS1 ||
		rBssRlmParam.ucVhtChannelFrequencyS2 !=
			prBssInfo->ucVhtChannelFrequencyS2 ||
		rBssRlmParam.u2VhtBasicMcsSet != prBssInfo->u2VhtBasicMcsSet ||
		rBssRlmParam.ucRxNss != prBssInfo->ucOpRxNss ||
		rBssRlmParam.ucTxNss != prBssInfo->ucOpTxNss)
		fgNewParameter = TRUE;
	else {
		DBGLOG(RLM, TRACE,
		       "prBssInfo's params are all the same! not to sync!\n");
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
void rlmProcessBcn(struct ADAPTER *prAdapter, struct SW_RFB *prSwRfb,
		   uint8_t *pucIE, uint16_t u2IELength)
{
	struct BSS_INFO *prBssInfo;
	u_int8_t fgNewParameter;
#if (CFG_SUPPORT_802_11AX == 1)
	u_int8_t fgNewSRParam = FALSE;
#endif
	uint8_t i;

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
	for (i = 0; i < prAdapter->ucHwBssIdNum; i++) {
		prBssInfo = prAdapter->aprBssInfo[i];

		if (IS_BSS_BOW(prBssInfo))
			continue;

		if (IS_BSS_ACTIVE(prBssInfo)) {
			if (prBssInfo->eCurrentOPMode ==
				    OP_MODE_INFRASTRUCTURE &&
			    prBssInfo->eConnectionState ==
				    MEDIA_STATE_CONNECTED) {
				/* P2P client or AIS infra STA */
				if (EQUAL_MAC_ADDR(
					    prBssInfo->aucBSSID,
					    ((struct WLAN_MAC_MGMT_HEADER
						      *)(prSwRfb->pvHeader))
						    ->aucBSSID)) {

					fgNewParameter = rlmRecBcnInfoForClient(
						prAdapter, prBssInfo, prSwRfb,
						pucIE, u2IELength);
#if (CFG_SUPPORT_802_11AX == 1)
					if (fgEfuseCtrlAxOn == 1) {
					fgNewSRParam = heRlmRecHeSRParams(
						prAdapter, prBssInfo,
						prSwRfb, pucIE, u2IELength);
					}
#endif
				} else {
					fgNewParameter =
						rlmRecBcnFromNeighborForClient(
							prAdapter, prBssInfo,
							prSwRfb, pucIE,
							u2IELength);
				}
			}
#if CFG_ENABLE_WIFI_DIRECT
			else if (prAdapter->fgIsP2PRegistered &&
				 (prBssInfo->eCurrentOPMode ==
					  OP_MODE_ACCESS_POINT ||
				  prBssInfo->eCurrentOPMode ==
					  OP_MODE_P2P_DEVICE)) {
				/* AP scan to check if 20/40M bandwidth is
				 * permitted
				 */
				rlmRecBcnFromNeighborForClient(
					prAdapter, prBssInfo, prSwRfb, pucIE,
					u2IELength);
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
#if (CFG_SUPPORT_802_11AX == 1)
			if (fgEfuseCtrlAxOn == 1) {
				if (fgNewSRParam) {
					nicRlmUpdateSRParams(prAdapter,
						prBssInfo->ucBssIndex);
					fgNewSRParam = FALSE;
				}
			}
#endif

		} /* end of IS_BSS_ACTIVE() */
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
void rlmProcessAssocRsp(struct ADAPTER *prAdapter, struct SW_RFB *prSwRfb,
			uint8_t *pucIE, uint16_t u2IELength)
{
	struct BSS_INFO *prBssInfo;
	struct STA_RECORD *prStaRec;
	uint8_t ucPriChannel;
#if (CFG_SUPPORT_802_11AX == 1)
	uint8_t fgNewSRParam;
#endif

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

	prBssInfo->fgUseShortSlotTime =
		((prBssInfo->u2CapInfo & CAP_INFO_SHORT_SLOT_TIME) ||
		 (prBssInfo->eBand != BAND_2G4))
			? TRUE
			: FALSE;
	ucPriChannel =
		rlmRecIeInfoForClient(prAdapter, prBssInfo, pucIE, u2IELength);

	/*Update the parameters from Association Response only,
	 *if the parameters need to be updated by both Beacon and Association
	 *Response,
	 *user should use another function, rlmRecIeInfoForClient()
	 */
	rlmRecAssocRespIeInfoForClient(prAdapter, prBssInfo, pucIE, u2IELength);

	if (prBssInfo->ucPrimaryChannel != ucPriChannel) {
		DBGLOG(RLM, INFO,
		       "Use RF pri channel[%u].Pri channel in HT OP IE is :[%u]\n",
		       prBssInfo->ucPrimaryChannel, ucPriChannel);
	}
	/* Avoid wrong primary channel info in HT operation
	 * IE info when accept association response
	 */
#if 0
	if (ucPriChannel > 0)
		prBssInfo->ucPrimaryChannel = ucPriChannel;
#endif

	if (!RLM_NET_IS_11N(prBssInfo) ||
	    !(prStaRec->u2HtCapInfo & HT_CAP_INFO_SUP_CHNL_WIDTH))
		prBssInfo->fg40mBwAllowed = FALSE;

#if (CFG_SUPPORT_802_11AX == 1)
	if (fgEfuseCtrlAxOn == 1) {
		fgNewSRParam = heRlmRecHeSRParams(prAdapter, prBssInfo,
						prSwRfb, pucIE, u2IELength);
		/* ASSERT(fgNewSRParam); */
		nicRlmUpdateSRParams(prAdapter, prBssInfo->ucBssIndex);
	}
#endif

	/* Note: Update its capabilities to WTBL by cnmStaRecChangeState(),
	 * which
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
void rlmProcessHtAction(struct ADAPTER *prAdapter, struct SW_RFB *prSwRfb)
{
	struct ACTION_NOTIFY_CHNL_WIDTH_FRAME *prRxFrame;
	struct ACTION_SM_POWER_SAVE_FRAME *prRxSmpsFrame;
	struct STA_RECORD *prStaRec;
	struct BSS_INFO *prBssInfo;
	uint16_t u2HtCapInfoBitmask = 0;

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prRxFrame = (struct ACTION_NOTIFY_CHNL_WIDTH_FRAME *)prSwRfb->pvHeader;
	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);

	if (!prStaRec)
		return;

	switch (prRxFrame->ucAction) {
	case ACTION_HT_NOTIFY_CHANNEL_WIDTH:
		if (prStaRec->ucStaState != STA_STATE_3 ||
		    prSwRfb->u2PacketLen <
			    sizeof(struct ACTION_NOTIFY_CHNL_WIDTH_FRAME)) {
			return;
		}

		/* To do: depending regulation class 13 and 14 based on spec
		 * Note: (ucChannelWidth==1) shall restored back to original
		 * capability, not current setting to 40MHz BW here
		 */
		/* 1. Update StaRec for AP/STA mode */
		if (prRxFrame->ucChannelWidth == HT_NOTIFY_CHANNEL_WIDTH_20)
			prStaRec->u2HtCapInfo &= ~HT_CAP_INFO_SUP_CHNL_WIDTH;
		else if (prRxFrame->ucChannelWidth ==
			 HT_NOTIFY_CHANNEL_WIDTH_ANY_SUPPORT_CAHNNAEL_WIDTH)
			prStaRec->u2HtCapInfo |= HT_CAP_INFO_SUP_CHNL_WIDTH;

		cnmStaSendUpdateCmd(prAdapter, prStaRec, NULL, FALSE);

		/* 2. Update BssInfo for STA mode */
		prBssInfo = prAdapter->aprBssInfo[prStaRec->ucBssIndex];
		if (prBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE) {
			if (prRxFrame->ucChannelWidth ==
			    HT_NOTIFY_CHANNEL_WIDTH_20) {
				prBssInfo->ucHtOpInfo1 &=
					~HT_OP_INFO1_STA_CHNL_WIDTH;
				prBssInfo->eBssSCO = CHNL_EXT_SCN;
			} else if (
				prRxFrame->ucChannelWidth ==
	HT_NOTIFY_CHANNEL_WIDTH_ANY_SUPPORT_CAHNNAEL_WIDTH)
				prBssInfo->ucHtOpInfo1 |=
					HT_OP_INFO1_STA_CHNL_WIDTH;

			/* Revise by own OP BW if needed */
			if (prBssInfo->fgIsOpChangeChannelWidth &&
			    prBssInfo->ucOpChangeChannelWidth == MAX_BW_20MHZ) {
				prBssInfo->ucHtOpInfo1 &=
					~(HT_OP_INFO1_SCO |
					  HT_OP_INFO1_STA_CHNL_WIDTH);
				prBssInfo->eBssSCO = CHNL_EXT_SCN;
			}

			/* 3. Update OP BW to FW */
			rlmSyncOperationParams(prAdapter, prBssInfo);
		}
		break;
		/* Support SM power save */ /* TH3_Huang */
	case ACTION_HT_SM_POWER_SAVE:
		prRxSmpsFrame =
			(struct ACTION_SM_POWER_SAVE_FRAME *)prSwRfb->pvHeader;
		if (prStaRec->ucStaState != STA_STATE_3 ||
		    prSwRfb->u2PacketLen <
			    sizeof(struct ACTION_SM_POWER_SAVE_FRAME)) {
			return;
		}

		/* The SM power enable bit is different definition in HtCap and
		 * SMpower IE field
		 */
		if (!(prRxSmpsFrame->ucSmPowerCtrl &
		      (HT_SM_POWER_SAVE_CONTROL_ENABLED |
		       HT_SM_POWER_SAVE_CONTROL_SM_MODE)))
			u2HtCapInfoBitmask |= HT_CAP_INFO_SM_POWER_SAVE;

		/* Support SMPS action frame, TH3_Huang */
		/* Update StaRec if SM power state changed */
		if ((prStaRec->u2HtCapInfo & HT_CAP_INFO_SM_POWER_SAVE) !=
		    u2HtCapInfoBitmask) {
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
void rlmProcessVhtAction(struct ADAPTER *prAdapter, struct SW_RFB *prSwRfb)
{
	struct ACTION_OP_MODE_NOTIFICATION_FRAME *prRxFrame;
	struct STA_RECORD *prStaRec;
	struct BSS_INFO *prBssInfo;
	uint8_t ucVhtOpModeChannelWidth = 0;

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prRxFrame =
		(struct ACTION_OP_MODE_NOTIFICATION_FRAME *)prSwRfb->pvHeader;
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
		    prSwRfb->u2PacketLen <
			    sizeof(struct ACTION_OP_MODE_NOTIFICATION_FRAME)) {
			return;
		}

		if (((prRxFrame->ucOperatingMode & VHT_OP_MODE_RX_NSS_TYPE) !=
		     VHT_OP_MODE_RX_NSS_TYPE) &&
		    (prStaRec->ucVhtOpMode != prRxFrame->ucOperatingMode)) {
			/* 1. Fill OP mode notification info */
			prStaRec->ucVhtOpMode = prRxFrame->ucOperatingMode;
			DBGLOG(RLM, INFO,
			       "rlmProcessVhtAction -- Update ucVhtOpMode to 0x%x\n",
			       prStaRec->ucVhtOpMode);

			/* 2. Modify channel width parameters */
			ucVhtOpModeChannelWidth = prRxFrame->ucOperatingMode &
						  VHT_OP_MODE_CHANNEL_WIDTH;
			if (prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT) {
				if (ucVhtOpModeChannelWidth ==
				    VHT_OP_MODE_CHANNEL_WIDTH_20)
					prStaRec->u2HtCapInfo &=
						~HT_CAP_INFO_SUP_CHNL_WIDTH;
				else /* for other 3 VHT cases: 40/80/160 */
					prStaRec->u2HtCapInfo |=
						HT_CAP_INFO_SUP_CHNL_WIDTH;
			} else if (prBssInfo->eCurrentOPMode ==
				   OP_MODE_INFRASTRUCTURE)
				rlmRecOpModeBwForClient(ucVhtOpModeChannelWidth,
							prBssInfo);

			/* 3. Update StaRec to FW */
			/* As defined in spec, 11 means not support this MCS */
			if (((prRxFrame->ucOperatingMode & VHT_OP_MODE_RX_NSS)
				>> VHT_OP_MODE_RX_NSS_OFFSET) ==
				VHT_OP_MODE_NSS_2) {
				prStaRec->u2VhtRxMcsMap = BITS(0, 15) &
					(~(VHT_CAP_INFO_MCS_1SS_MASK |
					VHT_CAP_INFO_MCS_2SS_MASK));

				prStaRec->u2VhtRxMcsMap |=
					(prStaRec->u2VhtRxMcsMapAssoc &
					(VHT_CAP_INFO_MCS_1SS_MASK |
					VHT_CAP_INFO_MCS_2SS_MASK));

				DBGLOG(RLM, INFO,
				       "NSS=2 RxMcsMap:0x%x, McsMapAssoc:0x%x\n",
				       prStaRec->u2VhtRxMcsMap,
				       prStaRec->u2VhtRxMcsMapAssoc);
			} else {
				/* NSS = 1 or others */
				prStaRec->u2VhtRxMcsMap = BITS(0, 15) &
					(~VHT_CAP_INFO_MCS_1SS_MASK);

				prStaRec->u2VhtRxMcsMap |=
					(prStaRec->u2VhtRxMcsMapAssoc &
					VHT_CAP_INFO_MCS_1SS_MASK);

				DBGLOG(RLM, INFO,
				       "NSS=1 RxMcsMap:0x%x, McsMapAssoc:0x%x\n",
				       prStaRec->u2VhtRxMcsMap,
				       prStaRec->u2VhtRxMcsMapAssoc);
			}
			cnmStaSendUpdateCmd(prAdapter, prStaRec, NULL, FALSE);

			/* 4. Update BW parameters in BssInfo for STA mode only
			 */
			if (prBssInfo->eCurrentOPMode ==
			    OP_MODE_INFRASTRUCTURE) {
				/* 4.1 Revise by own OP BW if needed for STA
				 * mode only
				 */
				if (prBssInfo->fgIsOpChangeChannelWidth) {
					/* VHT */
					if (rlmGetVhtOpBwByBssOpBw(
					prBssInfo
					->ucOpChangeChannelWidth) <
						prBssInfo->ucVhtChannelWidth)
						rlmFillVhtOpInfoByBssOpBw(
						prBssInfo,
						prBssInfo
						->ucOpChangeChannelWidth);
					/* HT */
					if (prBssInfo
					->fgIsOpChangeChannelWidth &&
					    prBssInfo->ucOpChangeChannelWidth ==
						    MAX_BW_20MHZ) {
						prBssInfo->ucHtOpInfo1 &= ~(
						HT_OP_INFO1_SCO |
						HT_OP_INFO1_STA_CHNL_WIDTH);
						prBssInfo->eBssSCO =
							CHNL_EXT_SCN;
					}
				}

				/* 4.2 Check if OP BW parameter valid */
				if (!rlmDomainIsValidRfSetting(
					    prAdapter, prBssInfo->eBand,
					    prBssInfo->ucPrimaryChannel,
					    prBssInfo->eBssSCO,
					    prBssInfo->ucVhtChannelWidth,
					    prBssInfo->ucVhtChannelFrequencyS1,
				prBssInfo->ucVhtChannelFrequencyS2)) {

					DBGLOG(RLM, WARN,
					       "rlmProcessVhtAction invalid RF settings\n");

					/* Error Handling for Non-predicted IE -
					 * Fixed to set 20MHz
					 */
					prBssInfo->ucVhtChannelWidth =
						CW_20_40MHZ;
					prBssInfo->ucVhtChannelFrequencyS1 = 0;
					prBssInfo->ucVhtChannelFrequencyS2 = 0;
					prBssInfo->eBssSCO = CHNL_EXT_SCN;
					prBssInfo->ucHtOpInfo1 &=
						~(HT_OP_INFO1_SCO |
						  HT_OP_INFO1_STA_CHNL_WIDTH);
				}

				/* 4.3 Update BSS OP BW to FW for STA mode only
				 */
				rlmSyncOperationParams(prAdapter, prBssInfo);
			}
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
void rlmFillSyncCmdParam(struct CMD_SET_BSS_RLM_PARAM *prCmdBody,
			 struct BSS_INFO *prBssInfo)
{
	if (!prCmdBody || !prBssInfo)
		return;

	prCmdBody->ucBssIndex = prBssInfo->ucBssIndex;
	prCmdBody->ucRfBand = (uint8_t)prBssInfo->eBand;
	prCmdBody->ucPrimaryChannel = prBssInfo->ucPrimaryChannel;
	prCmdBody->ucRfSco = (uint8_t)prBssInfo->eBssSCO;
	prCmdBody->ucErpProtectMode = (uint8_t)prBssInfo->fgErpProtectMode;
	prCmdBody->ucHtProtectMode = (uint8_t)prBssInfo->eHtProtectMode;
	prCmdBody->ucGfOperationMode = (uint8_t)prBssInfo->eGfOperationMode;
	prCmdBody->ucTxRifsMode = (uint8_t)prBssInfo->eRifsOperationMode;
	prCmdBody->u2HtOpInfo3 = prBssInfo->u2HtOpInfo3;
	prCmdBody->u2HtOpInfo2 = prBssInfo->u2HtOpInfo2;
	prCmdBody->ucHtOpInfo1 = prBssInfo->ucHtOpInfo1;
	prCmdBody->ucUseShortPreamble = prBssInfo->fgUseShortPreamble;
	prCmdBody->ucUseShortSlotTime = prBssInfo->fgUseShortSlotTime;
	prCmdBody->ucVhtChannelWidth = prBssInfo->ucVhtChannelWidth;
	prCmdBody->ucVhtChannelFrequencyS1 = prBssInfo->ucVhtChannelFrequencyS1;
	prCmdBody->ucVhtChannelFrequencyS2 = prBssInfo->ucVhtChannelFrequencyS2;
	prCmdBody->u2VhtBasicMcsSet = prBssInfo->u2BSSBasicRateSet;
	prCmdBody->ucTxNss = prBssInfo->ucOpTxNss;
	prCmdBody->ucRxNss = prBssInfo->ucOpRxNss;

	if (RLM_NET_PARAM_VALID(prBssInfo)) {
		DBGLOG(RLM, INFO,
		       "N=%d b=%d c=%d s=%d e=%d h=%d I=0x%02x l=%d p=%d w=%d s1=%d s2=%d RxN=%d, TxN=%d\n",
		       prCmdBody->ucBssIndex, prCmdBody->ucRfBand,
		       prCmdBody->ucPrimaryChannel, prCmdBody->ucRfSco,
		       prCmdBody->ucErpProtectMode, prCmdBody->ucHtProtectMode,
		       prCmdBody->ucHtOpInfo1, prCmdBody->ucUseShortSlotTime,
		       prCmdBody->ucUseShortPreamble,
		       prCmdBody->ucVhtChannelWidth,
		       prCmdBody->ucVhtChannelFrequencyS1,
		       prCmdBody->ucVhtChannelFrequencyS2,
		       prCmdBody->ucRxNss,
		       prCmdBody->ucTxNss);
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
void rlmSyncOperationParams(struct ADAPTER *prAdapter,
			    struct BSS_INFO *prBssInfo)
{
	struct CMD_SET_BSS_RLM_PARAM *prCmdBody;
	uint32_t rStatus;

	ASSERT(prAdapter);
	ASSERT(prBssInfo);

	prCmdBody = (struct CMD_SET_BSS_RLM_PARAM *)cnmMemAlloc(
		prAdapter, RAM_TYPE_BUF, sizeof(struct CMD_SET_BSS_RLM_PARAM));

	/* ASSERT(prCmdBody); */
	/* To do: exception handle */
	if (!prCmdBody) {
		DBGLOG(RLM, WARN, "No buf for sync RLM params (Net=%d)\n",
		       prBssInfo->ucBssIndex);
		return;
	}

	rlmFillSyncCmdParam(prCmdBody, prBssInfo);

	rStatus = wlanSendSetQueryCmd(
		prAdapter,			      /* prAdapter */
		CMD_ID_SET_BSS_RLM_PARAM,	     /* ucCID */
		TRUE,				      /* fgSetQuery */
		FALSE,				      /* fgNeedResp */
		FALSE,				      /* fgIsOid */
		NULL,				      /* pfCmdDoneHandler */
		NULL,				      /* pfCmdTimeoutHandler */
		sizeof(struct CMD_SET_BSS_RLM_PARAM), /* u4SetQueryInfoLen */
		(uint8_t *)prCmdBody,		      /* pucInfoBuffer */
		NULL,				      /* pvSetQueryBuffer */
		0				      /* u4SetQueryBufferLen */
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
void rlmProcessAssocReq(struct ADAPTER *prAdapter, struct SW_RFB *prSwRfb,
			uint8_t *pucIE, uint16_t u2IELength)
{
	struct BSS_INFO *prBssInfo;
	struct STA_RECORD *prStaRec;
	uint16_t u2Offset;
	struct IE_HT_CAP *prHtCap;
#if CFG_SUPPORT_802_11AC
	struct IE_VHT_CAP *prVhtCap = NULL;
	struct IE_OP_MODE_NOTIFICATION
		*prOPModeNotification; /* Operation Mode Notification */
	u_int8_t fgHasOPModeIE = FALSE;
#endif

	ASSERT(prAdapter);
	ASSERT(prSwRfb);
	ASSERT(pucIE);

	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);
	if (!prStaRec)
		return;
	ASSERT(prStaRec->ucBssIndex <= prAdapter->ucHwBssIdNum);

	prBssInfo = prAdapter->aprBssInfo[prStaRec->ucBssIndex];

	IE_FOR_EACH(pucIE, u2IELength, u2Offset)
	{
		switch (IE_ID(pucIE)) {
		case ELEM_ID_HT_CAP:
			if (!RLM_NET_IS_11N(prBssInfo) ||
			    IE_LEN(pucIE) != (sizeof(struct IE_HT_CAP) - 2))
				break;
			prHtCap = (struct IE_HT_CAP *)pucIE;
			prStaRec->ucMcsSet =
				prHtCap->rSupMcsSet.aucRxMcsBitmask[0];
			prStaRec->fgSupMcs32 =
				(prHtCap->rSupMcsSet.aucRxMcsBitmask[32 / 8] &
				 BIT(0))
					? TRUE
					: FALSE;

			kalMemCopy(
				prStaRec->aucRxMcsBitmask,
				prHtCap->rSupMcsSet.aucRxMcsBitmask,
				/*SUP_MCS_RX_BITMASK_OCTET_NUM */
				sizeof(prStaRec->aucRxMcsBitmask));

			prStaRec->u2HtCapInfo = prHtCap->u2HtCapInfo;

			/* Set Short LDPC Tx capability */
			if (IS_FEATURE_FORCE_ENABLED(
				    prAdapter->rWifiVar.ucTxLdpc))
				prStaRec->u2HtCapInfo |= HT_CAP_INFO_LDPC_CAP;
			else if (IS_FEATURE_DISABLED(
					 prAdapter->rWifiVar.ucTxLdpc))
				prStaRec->u2HtCapInfo &= ~HT_CAP_INFO_LDPC_CAP;

			/* Set STBC Tx capability */
			if (IS_FEATURE_FORCE_ENABLED(
				    prAdapter->rWifiVar.ucTxStbc))
				prStaRec->u2HtCapInfo |= HT_CAP_INFO_TX_STBC;
			else if (IS_FEATURE_DISABLED(
					 prAdapter->rWifiVar.ucTxStbc))
				prStaRec->u2HtCapInfo &= ~HT_CAP_INFO_TX_STBC;
			/* Set Short GI Tx capability */
			if (IS_FEATURE_FORCE_ENABLED(
				    prAdapter->rWifiVar.ucTxShortGI)) {
				prStaRec->u2HtCapInfo |=
					HT_CAP_INFO_SHORT_GI_20M;
				prStaRec->u2HtCapInfo |=
					HT_CAP_INFO_SHORT_GI_40M;
			} else if (IS_FEATURE_DISABLED(
					   prAdapter->rWifiVar.ucTxShortGI)) {
				prStaRec->u2HtCapInfo &=
					~HT_CAP_INFO_SHORT_GI_20M;
				prStaRec->u2HtCapInfo &=
					~HT_CAP_INFO_SHORT_GI_40M;
			}

			/* Set HT Greenfield Tx capability */
			if (IS_FEATURE_FORCE_ENABLED(
				    prAdapter->rWifiVar.ucTxGf))
				prStaRec->u2HtCapInfo |= HT_CAP_INFO_HT_GF;
			else if (IS_FEATURE_DISABLED(
					 prAdapter->rWifiVar.ucTxGf))
				prStaRec->u2HtCapInfo &= ~HT_CAP_INFO_HT_GF;

			prStaRec->ucAmpduParam = prHtCap->ucAmpduParam;
			prStaRec->u2HtExtendedCap = prHtCap->u2HtExtendedCap;
			prStaRec->u4TxBeamformingCap =
				prHtCap->u4TxBeamformingCap;
			prStaRec->ucAselCap = prHtCap->ucAselCap;
			break;

#if (CFG_SUPPORT_802_11AX == 1)
		case ELEM_ID_RESERVED:
			if (IE_ID_EXT(pucIE) == ELEM_EXT_ID_HE_CAP)
				heRlmRecHeCapInfo(prAdapter, prStaRec, pucIE);
			break;
#endif

#if CFG_SUPPORT_802_11AC
		case ELEM_ID_VHT_CAP:
			if (!RLM_NET_IS_11AC(prBssInfo) ||
			    IE_LEN(pucIE) != (sizeof(struct IE_VHT_CAP) - 2))
				break;

			prVhtCap = (struct IE_VHT_CAP *)pucIE;

			prStaRec->u4VhtCapInfo = prVhtCap->u4VhtCapInfo;

			/* Set Tx LDPC capability */
			if (IS_FEATURE_FORCE_ENABLED(
				    prAdapter->rWifiVar.ucTxLdpc))
				prStaRec->u4VhtCapInfo |= VHT_CAP_INFO_RX_LDPC;
			else if (IS_FEATURE_DISABLED(
					 prAdapter->rWifiVar.ucTxLdpc))
				prStaRec->u4VhtCapInfo &= ~VHT_CAP_INFO_RX_LDPC;

			/* Set Tx STBC capability */
			if (IS_FEATURE_FORCE_ENABLED(
				    prAdapter->rWifiVar.ucTxStbc))
				prStaRec->u4VhtCapInfo |= VHT_CAP_INFO_TX_STBC;
			else if (IS_FEATURE_DISABLED(
					 prAdapter->rWifiVar.ucTxStbc))
				prStaRec->u4VhtCapInfo &= ~VHT_CAP_INFO_TX_STBC;

			/* Set Tx TXOP PS capability */
			if (IS_FEATURE_FORCE_ENABLED(
				    prAdapter->rWifiVar.ucTxopPsTx))
				prStaRec->u4VhtCapInfo |=
					VHT_CAP_INFO_VHT_TXOP_PS;
			else if (IS_FEATURE_DISABLED(
					 prAdapter->rWifiVar.ucTxopPsTx))
				prStaRec->u4VhtCapInfo &=
					~VHT_CAP_INFO_VHT_TXOP_PS;

			/* Set Tx Short GI capability */
			if (IS_FEATURE_FORCE_ENABLED(
				    prAdapter->rWifiVar.ucTxShortGI)) {
				prStaRec->u4VhtCapInfo |=
					VHT_CAP_INFO_SHORT_GI_80;
				prStaRec->u4VhtCapInfo |=
					VHT_CAP_INFO_SHORT_GI_160_80P80;
			} else if (IS_FEATURE_DISABLED(
					   prAdapter->rWifiVar.ucTxShortGI)) {
				prStaRec->u4VhtCapInfo &=
					~VHT_CAP_INFO_SHORT_GI_80;
				prStaRec->u4VhtCapInfo &=
					~VHT_CAP_INFO_SHORT_GI_160_80P80;
			}

			prStaRec->u2VhtRxMcsMap =
				prVhtCap->rVhtSupportedMcsSet.u2RxMcsMap;

			prStaRec->u2VhtRxMcsMapAssoc =
				prStaRec->u2VhtRxMcsMap;

			prStaRec->u2VhtRxHighestSupportedDataRate =
				prVhtCap->rVhtSupportedMcsSet
					.u2RxHighestSupportedDataRate;
			prStaRec->u2VhtTxMcsMap =
				prVhtCap->rVhtSupportedMcsSet.u2TxMcsMap;
			prStaRec->u2VhtTxHighestSupportedDataRate =
				prVhtCap->rVhtSupportedMcsSet
					.u2TxHighestSupportedDataRate;

			/* Set initial value of VHT OP mode */
			prStaRec->ucVhtOpMode = 0;
			prStaRec->ucVhtOpMode |=
				rlmGetOpModeBwByVhtAndHtOpInfo(prBssInfo);
			prStaRec->ucVhtOpMode |=
				((rlmGetSupportRxNssInVhtCap(prVhtCap) - 1)
				 << VHT_OP_MODE_RX_NSS_OFFSET) &
				VHT_OP_MODE_RX_NSS;

			break;
		case ELEM_ID_OP_MODE:
			if (!RLM_NET_IS_11AC(prBssInfo) ||
			    IE_LEN(pucIE) !=
				    (sizeof(struct IE_OP_MODE_NOTIFICATION) -
				     2))
				break;
			prOPModeNotification =
				(struct IE_OP_MODE_NOTIFICATION *)pucIE;

			if ((prOPModeNotification->ucOpMode &
			     VHT_OP_MODE_RX_NSS_TYPE) !=
			    VHT_OP_MODE_RX_NSS_TYPE) {
				fgHasOPModeIE = TRUE;
			}

			break;

#endif

		default:
			break;
		} /* end of switch */
	}	 /* end of IE_FOR_EACH */
#if CFG_SUPPORT_802_11AC
	/*Fill by OP Mode IE after completing parsing all IE to make sure it
	 * won't be overwrite
	 */
	if (fgHasOPModeIE == TRUE)
		prStaRec->ucVhtOpMode = prOPModeNotification->ucOpMode;
#endif
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
void rlmBssInitForAPandIbss(struct ADAPTER *prAdapter,
			    struct BSS_INFO *prBssInfo)
{
	ASSERT(prAdapter);
	ASSERT(prBssInfo);

#if CFG_ENABLE_WIFI_DIRECT
	if (prAdapter->fgIsP2PRegistered &&
	    prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT)
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
void rlmBssAborted(struct ADAPTER *prAdapter, struct BSS_INFO *prBssInfo)
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
static void rlmBssReset(struct ADAPTER *prAdapter, struct BSS_INFO *prBssInfo)
{
	ASSERT(prAdapter);
	ASSERT(prBssInfo);

	/* HT related parameters */
	prBssInfo->ucHtOpInfo1 = 0; /* RIFS disabled. 20MHz */
	prBssInfo->u2HtOpInfo2 = 0;
	prBssInfo->u2HtOpInfo3 = 0;

#if CFG_SUPPORT_802_11AC
	prBssInfo->ucVhtChannelWidth = 0;       /* VHT_OP_CHANNEL_WIDTH_80; */
	prBssInfo->ucVhtChannelFrequencyS1 = 0; /* 42; */
	prBssInfo->ucVhtChannelFrequencyS2 = 0;
	prBssInfo->u2VhtBasicMcsSet = 0; /* 0xFFFF; */
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

	prBssInfo->fgObssErpProtectMode = 0;    /* GO only */
	prBssInfo->eObssHtProtectMode = 0;      /* GO only */
	prBssInfo->eObssGfOperationMode = 0;    /* GO only */
	prBssInfo->fgObssRifsOperationMode = 0; /* GO only */
	prBssInfo->fgObssActionForcedTo20M = 0; /* GO only */
	prBssInfo->fgObssBeaconForcedTo20M = 0; /* GO only */

	/* OP mode change control parameters */
	prBssInfo->fgIsOpChangeChannelWidth = FALSE;
	prBssInfo->fgIsOpChangeRxNss = FALSE;
	prBssInfo->fgIsOpChangeTxNss = FALSE;

#if (CFG_SUPPORT_802_11AX == 1)
	if (fgEfuseCtrlAxOn == 1) {
		/* MU EDCA params */
		prBssInfo->ucMUEdcaUpdateCnt = 0;
		kalMemSet(&prBssInfo->arMUEdcaParams[0], 0,
		sizeof(struct _CMD_MU_EDCA_PARAMS_T) * WMM_AC_INDEX_NUM);

		/* Spatial Reuse params */
		prBssInfo->ucSRControl = 0;
		prBssInfo->ucNonSRGObssPdMaxOffset = 0;
		prBssInfo->ucSRGObssPdMinOffset = 0;
		prBssInfo->ucSRGObssPdMaxOffset = 0;
		prBssInfo->u8SRGBSSColorBitmap = 0;
		prBssInfo->u8SRGPartialBSSIDBitmap = 0;

		/* HE Operation */
		memset(prBssInfo->ucHeOpParams, 0, HE_OP_BYTE_NUM);
		/* set TXOP to 0x3FF (Spec. define default value) */
		prBssInfo->ucHeOpParams[0]
			|= HE_OP_PARAM0_TXOP_DUR_RTS_THRESHOLD_MASK;
		prBssInfo->ucHeOpParams[1]
			|= HE_OP_PARAM1_TXOP_DUR_RTS_THRESHOLD_MASK;
		prBssInfo->ucBssColorInfo = 0;
		prBssInfo->u2HeBasicMcsSet = 0;
	}
#endif

#ifdef CFG_DFS_CHSW_FORCE_BW20
	g_fgHasChannelSwitchIE = FALSE;
#endif
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
uint32_t rlmFillVhtCapIEByAdapter(struct ADAPTER *prAdapter,
				  struct BSS_INFO *prBssInfo, uint8_t *pOutBuf)
{
	struct IE_VHT_CAP *prVhtCap;
	struct VHT_SUPPORTED_MCS_FIELD *prVhtSupportedMcsSet;
	uint8_t i;

	ASSERT(prAdapter);
	ASSERT(prBssInfo);
	/* ASSERT(prMsduInfo); */

	prVhtCap = (struct IE_VHT_CAP *)pOutBuf;

	prVhtCap->ucId = ELEM_ID_VHT_CAP;
	prVhtCap->ucLength = sizeof(struct IE_VHT_CAP) - ELEM_HDR_LEN;
	prVhtCap->u4VhtCapInfo = VHT_CAP_INFO_DEFAULT_VAL;

	if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucRxShortGI))
		prVhtCap->u4VhtCapInfo |= VHT_CAP_INFO_SHORT_GI_80;

	if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucRxLdpc))
		prVhtCap->u4VhtCapInfo |= VHT_CAP_INFO_RX_LDPC;

	if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucRxStbc))
		prVhtCap->u4VhtCapInfo |= VHT_CAP_INFO_RX_STBC_ONE_STREAM;

	/*set MCS map */
	prVhtSupportedMcsSet = &prVhtCap->rVhtSupportedMcsSet;
	kalMemZero((void *)prVhtSupportedMcsSet,
		   sizeof(struct VHT_SUPPORTED_MCS_FIELD));

	for (i = 0; i < 8; i++) {
		prVhtSupportedMcsSet->u2RxMcsMap |= BITS(2 * i, (2 * i + 1));
		prVhtSupportedMcsSet->u2TxMcsMap |= BITS(2 * i, (2 * i + 1));
	}

	prVhtSupportedMcsSet->u2RxMcsMap &=
		(VHT_CAP_INFO_MCS_MAP_MCS9 << VHT_CAP_INFO_MCS_1SS_OFFSET);
	prVhtSupportedMcsSet->u2TxMcsMap &=
		(VHT_CAP_INFO_MCS_MAP_MCS9 << VHT_CAP_INFO_MCS_1SS_OFFSET);
	prVhtSupportedMcsSet->u2RxHighestSupportedDataRate =
		VHT_CAP_INFO_DEFAULT_HIGHEST_DATA_RATE;
	prVhtSupportedMcsSet->u2TxHighestSupportedDataRate =
		VHT_CAP_INFO_DEFAULT_HIGHEST_DATA_RATE;

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
uint32_t rlmFillHtCapIEByParams(u_int8_t fg40mAllowed,
				u_int8_t fgShortGIDisabled,
				uint8_t u8SupportRxSgi20,
				uint8_t u8SupportRxSgi40, uint8_t u8SupportRxGf,
				enum ENUM_OP_MODE eCurrentOPMode,
				uint8_t *pOutBuf)
{
	struct IE_HT_CAP *prHtCap;
	struct SUP_MCS_SET_FIELD *prSupMcsSet;

	ASSERT(pOutBuf);

	prHtCap = (struct IE_HT_CAP *)pOutBuf;

	/* Add HT capabilities IE */
	prHtCap->ucId = ELEM_ID_HT_CAP;
	prHtCap->ucLength = sizeof(struct IE_HT_CAP) - ELEM_HDR_LEN;

	prHtCap->u2HtCapInfo = HT_CAP_INFO_DEFAULT_VAL;
	if (!fg40mAllowed) {
		prHtCap->u2HtCapInfo &= ~(HT_CAP_INFO_SUP_CHNL_WIDTH |
					  HT_CAP_INFO_SHORT_GI_40M |
					  HT_CAP_INFO_DSSS_CCK_IN_40M);
	}
	if (fgShortGIDisabled)
		prHtCap->u2HtCapInfo &=
			~(HT_CAP_INFO_SHORT_GI_20M | HT_CAP_INFO_SHORT_GI_40M);

	if (u8SupportRxSgi20 == 2)
		prHtCap->u2HtCapInfo &= ~(HT_CAP_INFO_SHORT_GI_20M);
	if (u8SupportRxSgi40 == 2)
		prHtCap->u2HtCapInfo &= ~(HT_CAP_INFO_SHORT_GI_40M);
	if (u8SupportRxGf == 2)
		prHtCap->u2HtCapInfo &= ~(HT_CAP_INFO_HT_GF);

	prHtCap->ucAmpduParam = AMPDU_PARAM_DEFAULT_VAL;

	prSupMcsSet = &prHtCap->rSupMcsSet;
	kalMemZero((void *)&prSupMcsSet->aucRxMcsBitmask[0],
		   SUP_MCS_RX_BITMASK_OCTET_NUM);

	prSupMcsSet->aucRxMcsBitmask[0] = BITS(0, 7);

	if (fg40mAllowed)
		prSupMcsSet->aucRxMcsBitmask[32 / 8] = BIT(0); /* MCS32 */
	prSupMcsSet->u2RxHighestSupportedRate = SUP_MCS_RX_DEFAULT_HIGHEST_RATE;
	prSupMcsSet->u4TxRateInfo = SUP_MCS_TX_DEFAULT_VAL;

	prHtCap->u2HtExtendedCap = HT_EXT_CAP_DEFAULT_VAL;
	if (!fg40mAllowed || eCurrentOPMode != OP_MODE_INFRASTRUCTURE)
		prHtCap->u2HtExtendedCap &=
			~(HT_EXT_CAP_PCO | HT_EXT_CAP_PCO_TRANS_TIME_NONE);

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
uint32_t rlmFillHtCapIEByAdapter(struct ADAPTER *prAdapter,
				 struct BSS_INFO *prBssInfo, uint8_t *pOutBuf)
{
	struct IE_HT_CAP *prHtCap;
	struct SUP_MCS_SET_FIELD *prSupMcsSet;
	u_int8_t fg40mAllowed;

	ASSERT(prAdapter);
	ASSERT(prBssInfo);
	ASSERT(pOutBuf);

	fg40mAllowed = prBssInfo->fgAssoc40mBwAllowed;

	prHtCap = (struct IE_HT_CAP *)pOutBuf;

	/* Add HT capabilities IE */
	prHtCap->ucId = ELEM_ID_HT_CAP;
	prHtCap->ucLength = sizeof(struct IE_HT_CAP) - ELEM_HDR_LEN;

	prHtCap->u2HtCapInfo = HT_CAP_INFO_DEFAULT_VAL;
	if (!fg40mAllowed) {
		prHtCap->u2HtCapInfo &= ~(HT_CAP_INFO_SUP_CHNL_WIDTH |
					  HT_CAP_INFO_SHORT_GI_40M |
					  HT_CAP_INFO_DSSS_CCK_IN_40M);
	}
	if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucRxShortGI))
		prHtCap->u2HtCapInfo |=
			(HT_CAP_INFO_SHORT_GI_20M | HT_CAP_INFO_SHORT_GI_40M);

	if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucRxLdpc))
		prHtCap->u2HtCapInfo |= HT_CAP_INFO_LDPC_CAP;

	if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucRxStbc))
		prHtCap->u2HtCapInfo |= HT_CAP_INFO_RX_STBC_1_SS;

	prHtCap->ucAmpduParam = AMPDU_PARAM_DEFAULT_VAL;

	prSupMcsSet = &prHtCap->rSupMcsSet;
	kalMemZero((void *)&prSupMcsSet->aucRxMcsBitmask[0],
		   SUP_MCS_RX_BITMASK_OCTET_NUM);

	prSupMcsSet->aucRxMcsBitmask[0] = BITS(0, 7);

	if (fg40mAllowed && IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucMCS32))
		prSupMcsSet->aucRxMcsBitmask[32 / 8] = BIT(0); /* MCS32 */
	prSupMcsSet->u2RxHighestSupportedRate = SUP_MCS_RX_DEFAULT_HIGHEST_RATE;
	prSupMcsSet->u4TxRateInfo = SUP_MCS_TX_DEFAULT_VAL;

	prHtCap->u2HtExtendedCap = HT_EXT_CAP_DEFAULT_VAL;
	if (!fg40mAllowed ||
	    prBssInfo->eCurrentOPMode != OP_MODE_INFRASTRUCTURE)
		prHtCap->u2HtExtendedCap &=
			~(HT_EXT_CAP_PCO | HT_EXT_CAP_PCO_TRANS_TIME_NONE);

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
static void tpcComposeReportFrame(IN struct ADAPTER *prAdapter,
				  IN struct STA_RECORD *prStaRec,
				  IN PFN_TX_DONE_HANDLER pfTxDoneHandler)
{
	struct MSDU_INFO *prMsduInfo;
	struct BSS_INFO *prBssInfo;
	struct ACTION_TPC_REPORT_FRAME *prTxFrame;
	uint16_t u2PayloadLen;

	ASSERT(prAdapter);
	ASSERT(prStaRec);

	prBssInfo = &prAdapter->rWifiVar.arBssInfoPool[prStaRec->ucBssIndex];
	ASSERT(prBssInfo);

	prMsduInfo = (struct MSDU_INFO *)cnmMgtPktAlloc(
		prAdapter, MAC_TX_RESERVED_FIELD + PUBLIC_ACTION_MAX_LEN);

	if (!prMsduInfo)
		return;

	prTxFrame = (struct ACTION_TPC_REPORT_FRAME
			     *)((unsigned long)(prMsduInfo->prPacket) +
				MAC_TX_RESERVED_FIELD);

	prTxFrame->u2FrameCtrl = MAC_FRAME_ACTION;

	COPY_MAC_ADDR(prTxFrame->aucDestAddr, prStaRec->aucMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucSrcAddr, prBssInfo->aucOwnMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucBSSID, prBssInfo->aucBSSID);

	prTxFrame->ucCategory = CATEGORY_SPEC_MGT;
	prTxFrame->ucAction = ACTION_TPC_REPORT;

	/* 3 Compose the frame body's frame. */
	prTxFrame->ucDialogToken = prStaRec->ucSmDialogToken;
	prTxFrame->ucElemId = ELEM_ID_TPC_REPORT;
	prTxFrame->ucLength =
		sizeof(prTxFrame->ucLinkMargin) + sizeof(prTxFrame->ucTransPwr);
	prTxFrame->ucTransPwr = prAdapter->u4GetTxPower;
	prTxFrame->ucLinkMargin =
		prAdapter->rLinkQuality.rLq[prStaRec->ucBssIndex].
		cRssi - (0 - MIN_RCV_PWR);

	u2PayloadLen = ACTION_SM_TPC_REPORT_LEN;

	/* 4 Update information of MSDU_INFO_T */
	TX_SET_MMPDU(prAdapter, prMsduInfo, prStaRec->ucBssIndex,
		     prStaRec->ucIndex, WLAN_MAC_MGMT_HEADER_LEN,
		     WLAN_MAC_MGMT_HEADER_LEN + u2PayloadLen, pfTxDoneHandler,
		     MSDU_RATE_MODE_AUTO);

	DBGLOG(RLM, TRACE, "ucDialogToken %d ucTransPwr %d ucLinkMargin %d\n",
	       prTxFrame->ucDialogToken, prTxFrame->ucTransPwr,
	       prTxFrame->ucLinkMargin);

	/* 4 Enqueue the frame to send this action frame. */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);

	return;

} /* end of tpcComposeReportFrame() */

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
static void msmtComposeReportFrame(IN struct ADAPTER *prAdapter,
				   IN struct STA_RECORD *prStaRec,
				   IN PFN_TX_DONE_HANDLER pfTxDoneHandler)
{
	struct MSDU_INFO *prMsduInfo;
	struct BSS_INFO *prBssInfo;
	struct ACTION_SM_REQ_FRAME *prTxFrame;
	struct IE_MEASUREMENT_REPORT *prMeasurementRepIE;
	uint8_t *pucIE;
	uint16_t u2PayloadLen;

	ASSERT(prAdapter);
	ASSERT(prStaRec);

	prBssInfo = &prAdapter->rWifiVar.arBssInfoPool[prStaRec->ucBssIndex];
	ASSERT(prBssInfo);

	prMsduInfo = (struct MSDU_INFO *)cnmMgtPktAlloc(
		prAdapter, MAC_TX_RESERVED_FIELD + PUBLIC_ACTION_MAX_LEN);

	if (!prMsduInfo)
		return;

	prTxFrame = (struct ACTION_SM_REQ_FRAME
			     *)((unsigned long)(prMsduInfo->prPacket) +
				MAC_TX_RESERVED_FIELD);
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
		prMeasurementRepIE->ucLength =
		     sizeof(struct SM_BASIC_REPORT) + 3;
		u2PayloadLen = ACTION_SM_MEASURE_REPORT_LEN+
		     ACTION_SM_BASIC_REPORT_LEN;
	} else if (prStaRec->ucSmMsmtRequestMode == ELEM_RM_TYPE_CCA_REQ) {
		prMeasurementRepIE->ucLength = sizeof(struct SM_CCA_REPORT) + 3;
		u2PayloadLen = ACTION_SM_MEASURE_REPORT_LEN+
		     ACTION_SM_CCA_REPORT_LEN;
	} else if (prStaRec->ucSmMsmtRequestMode ==
		     ELEM_RM_TYPE_RPI_HISTOGRAM_REQ) {
		prMeasurementRepIE->ucLength = sizeof(struct SM_RPI_REPORT) + 3;
		u2PayloadLen = ACTION_SM_MEASURE_REPORT_LEN+
		     ACTION_SM_PRI_REPORT_LEN;
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
	TX_SET_MMPDU(prAdapter, prMsduInfo, prStaRec->ucBssIndex,
		     prStaRec->ucIndex, WLAN_MAC_MGMT_HEADER_LEN,
		     WLAN_MAC_MGMT_HEADER_LEN + u2PayloadLen, pfTxDoneHandler,
		     MSDU_RATE_MODE_AUTO);

	DBGLOG(RLM, TRACE,
	       "ucDialogToken %d ucToken %d ucReportMode %d ucMeasurementType %d\n",
	       prTxFrame->ucDialogToken, prMeasurementRepIE->ucToken,
	       prMeasurementRepIE->ucReportMode,
	       prMeasurementRepIE->ucMeasurementType);

	/* 4 Enqueue the frame to send this action frame. */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);

	return;

} /* end of msmtComposeReportFrame() */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function handle spectrum management action frame
 *
 * \param[in]
 *
 * \return none
 */
/*----------------------------------------------------------------------------*/
void rlmProcessSpecMgtAction(struct ADAPTER *prAdapter, struct SW_RFB *prSwRfb)
{
	uint8_t *pucIE;
	struct STA_RECORD *prStaRec;
	struct BSS_INFO *prBssInfo;
	uint16_t u2IELength;
	uint16_t u2Offset = 0;
	struct IE_CHANNEL_SWITCH *prChannelSwitchAnnounceIE;
	struct IE_SECONDARY_OFFSET *prSecondaryOffsetIE;
	struct IE_WIDE_BAND_CHANNEL *prWideBandChannelIE;
	struct IE_TPC_REQ *prTpcReqIE;
	struct IE_TPC_REPORT *prTpcRepIE;
	struct IE_MEASUREMENT_REQ *prMeasurementReqIE;
	struct IE_MEASUREMENT_REPORT *prMeasurementRepIE;
	struct ACTION_SM_REQ_FRAME *prRxFrame;
	u_int8_t fgHasWideBandIE = FALSE;
	u_int8_t fgHasSCOIE = FALSE;
	u_int8_t fgHasChannelSwitchIE = FALSE;

	DBGLOG(RLM, INFO, "[Mgt Action]rlmProcessSpecMgtAction\n");
	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	u2IELength =
		prSwRfb->u2PacketLen -
		(uint16_t)OFFSET_OF(struct ACTION_SM_REQ_FRAME, aucInfoElem[0]);

	prRxFrame = (struct ACTION_SM_REQ_FRAME *)prSwRfb->pvHeader;
	pucIE = prRxFrame->aucInfoElem;

	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);
	if (!prStaRec)
		return;

	if (prStaRec->ucBssIndex > prAdapter->ucHwBssIdNum)
		return;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);

	prStaRec->ucSmDialogToken = prRxFrame->ucDialogToken;

	DBGLOG_MEM8(RLM, INFO, pucIE, u2IELength);
	switch (prRxFrame->ucAction) {
	case ACTION_MEASUREMENT_REQ:
		DBGLOG(RLM, INFO, "[Mgt Action] Measure Request\n");
		prMeasurementReqIE = SM_MEASUREMENT_REQ_IE(pucIE);
		if (prMeasurementReqIE->ucId == ELEM_ID_MEASUREMENT_REQ) {
			/* Check IE length is valid */
			if (prMeasurementReqIE->ucLength != 0 &&
				(prMeasurementReqIE->ucLength >=
				sizeof(struct IE_MEASUREMENT_REQ) - 2)) {
				prStaRec->ucSmMsmtRequestMode =
					prMeasurementReqIE->ucRequestMode;
				prStaRec->ucSmMsmtToken =
					prMeasurementReqIE->ucToken;
				msmtComposeReportFrame(prAdapter, prStaRec,
							NULL);
			}
		}

		break;
	case ACTION_MEASUREMENT_REPORT:
		DBGLOG(RLM, INFO, "[Mgt Action] Measure Report\n");
		prMeasurementRepIE = SM_MEASUREMENT_REP_IE(pucIE);
		if (prMeasurementRepIE->ucId == ELEM_ID_MEASUREMENT_REPORT)
			DBGLOG(RLM, TRACE,
			       "[Mgt Action] Correct Measurement report IE !!\n");
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
			DBGLOG(RLM, TRACE,
			       "[Mgt Action] Correct TPC report IE !!\n");

		break;
	case ACTION_CHNL_SWITCH:
		IE_FOR_EACH(pucIE, u2IELength, u2Offset)
		{
			switch (IE_ID(pucIE)) {

			case ELEM_ID_WIDE_BAND_CHANNEL_SWITCH:
				if (!RLM_NET_IS_11AC(prBssInfo) ||
				    IE_LEN(pucIE) !=
					    (sizeof(struct
						    IE_WIDE_BAND_CHANNEL) -
					     2)) {
					DBGLOG(RLM, INFO,
					       "[Mgt Action] ELEM_ID_WIDE_BAND_CHANNEL_SWITCH, Length\n");
					break;
				}
				DBGLOG(RLM, INFO,
				       "[Mgt Action] ELEM_ID_WIDE_BAND_CHANNEL_SWITCH, 11AC\n");
				prWideBandChannelIE =
					(struct IE_WIDE_BAND_CHANNEL *)pucIE;
				prBssInfo->ucVhtChannelWidth =
					prWideBandChannelIE->ucNewChannelWidth;
				prBssInfo->ucVhtChannelFrequencyS1 =
					prWideBandChannelIE->ucChannelS1;
				prBssInfo->ucVhtChannelFrequencyS2 =
					prWideBandChannelIE->ucChannelS2;

				/* Revise by own OP BW if needed */
				if ((prBssInfo->fgIsOpChangeChannelWidth) &&
				    (rlmGetVhtOpBwByBssOpBw(
					     prBssInfo
						     ->ucOpChangeChannelWidth) <
				     prBssInfo->ucVhtChannelWidth)) {

					DBGLOG(RLM, LOUD,
					"Change to w:%d s1:%d s2:%d since own changed BW < peer's WideBand BW",
					prBssInfo->ucVhtChannelWidth,
					prBssInfo->ucVhtChannelFrequencyS1,
					prBssInfo->ucVhtChannelFrequencyS2);
					rlmFillVhtOpInfoByBssOpBw(
					prBssInfo,
					prBssInfo
					->ucOpChangeChannelWidth);
				}

				fgHasWideBandIE = TRUE;
				break;

			case ELEM_ID_CH_SW_ANNOUNCEMENT:
				if (IE_LEN(pucIE) !=
				    (sizeof(struct IE_CHANNEL_SWITCH) - 2)) {
					DBGLOG(RLM, INFO,
					       "[Mgt Action] ELEM_ID_CH_SW_ANNOUNCEMENT, Length\n");
					break;
				}

				prChannelSwitchAnnounceIE =
					(struct IE_CHANNEL_SWITCH *)pucIE;

				if (prChannelSwitchAnnounceIE
					    ->ucChannelSwitchMode == 1) {

					/* Need to stop data
					 * transmission immediately
					 */
					if (!g_fgHasStopTx) {
						g_fgHasStopTx = TRUE;
						/* AP */
						qmSetStaRecTxAllowed(prAdapter,
							   prStaRec,
							   FALSE);
						DBGLOG(RLM, EVENT,
							"[Ch] TxAllowed = FALSE\n");
					}

					if (prChannelSwitchAnnounceIE
						->ucChannelSwitchCount <= 5) {
						DBGLOG(RLM, INFO,
						"[Mgt Action] switch channel [%d]->[%d]\n",
						prBssInfo->ucPrimaryChannel,
						prChannelSwitchAnnounceIE
						    ->ucNewChannelNum);
						prBssInfo->ucPrimaryChannel =
						    prChannelSwitchAnnounceIE
						    ->ucNewChannelNum;
						prBssInfo->eBand =
						    (prBssInfo
						    ->ucPrimaryChannel
						    <= 14) ? BAND_2G4 :
						    BAND_5G;
					}
				} else {
					DBGLOG(RLM, INFO,
					       "[Mgt Action] ucChannelSwitchMode = 0\n");
				}

				fgHasChannelSwitchIE = TRUE;
				break;
			case ELEM_ID_SCO:
				if (IE_LEN(pucIE) !=
				    (sizeof(struct IE_SECONDARY_OFFSET) - 2)) {
					DBGLOG(RLM, INFO,
					       "[Mgt Action] ELEM_ID_SCO, Length\n");
					break;
				}
				prSecondaryOffsetIE =
					(struct IE_SECONDARY_OFFSET *)pucIE;
				DBGLOG(RLM, INFO,
				       "[Mgt Action] SCO [%d]->[%d]\n",
				       prBssInfo->eBssSCO,
				       prSecondaryOffsetIE->ucSecondaryOffset);
				prBssInfo->eBssSCO =
					prSecondaryOffsetIE->ucSecondaryOffset;
				fgHasSCOIE = TRUE;
				break;
			default:
				break;
			} /*end of switch IE_ID */
		}	 /*end of IE_FOR_EACH */
		if (fgHasChannelSwitchIE != FALSE) {
			struct BSS_DESC *prBssDesc;
			struct PARAM_SSID rSsid;

			COPY_SSID(rSsid.aucSsid, rSsid.u4SsidLen,
			      prBssInfo->aucSSID, prBssInfo->ucSSIDLen);
			prBssDesc = scanSearchBssDescByBssidAndSsid(
				prAdapter, prBssInfo->aucBSSID, TRUE, &rSsid);

			if (fgHasWideBandIE == FALSE) {
				prBssInfo->ucVhtChannelWidth = 0;
				prBssInfo->ucVhtChannelFrequencyS1 =
					prBssInfo->ucPrimaryChannel;
				prBssInfo->ucVhtChannelFrequencyS2 = 0;
			}
			if (fgHasSCOIE == FALSE)
				prBssInfo->eBssSCO = CHNL_EXT_SCN;

			/* Check SAP channel */
			p2pFuncSwitchSapChannel(prAdapter);
			if (prBssDesc) {
				prBssDesc->ucChannelNum =
						prBssInfo->ucPrimaryChannel;
				prBssDesc->eChannelWidth =
						prBssInfo->ucVhtChannelWidth;
				prBssDesc->ucCenterFreqS1 = prBssInfo->
					ucVhtChannelFrequencyS1;
				prBssDesc->ucCenterFreqS2 = prBssInfo->
					ucVhtChannelFrequencyS2;
			} else {
				DBGLOG(RLM, WARN,
				"[Mgt Action] BssDesc is not found!\n");
			}
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
uint32_t rlmSendOpModeNotificationFrame(struct ADAPTER *prAdapter,
				    struct STA_RECORD *prStaRec,
				    uint8_t ucChannelWidth, uint8_t ucOpRxNss)
{

	struct MSDU_INFO *prMsduInfo;
	struct ACTION_OP_MODE_NOTIFICATION_FRAME *prTxFrame;
	struct BSS_INFO *prBssInfo;
	uint16_t u2EstimatedFrameLen;
	PFN_TX_DONE_HANDLER pfTxDoneHandler = (PFN_TX_DONE_HANDLER)NULL;

	/* Sanity Check */
	if (!prStaRec)
		return WLAN_STATUS_FAILURE;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);
	if (!prBssInfo)
		return WLAN_STATUS_FAILURE;

	/* Calculate MSDU buffer length */
	u2EstimatedFrameLen = MAC_TX_RESERVED_FIELD +
			      sizeof(struct ACTION_OP_MODE_NOTIFICATION_FRAME);

	/* Alloc MSDU_INFO */
	prMsduInfo = (struct MSDU_INFO *)cnmMgtPktAlloc(prAdapter,
							u2EstimatedFrameLen);

	if (!prMsduInfo)
		return WLAN_STATUS_FAILURE;

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

	prTxFrame->ucOperatingMode |=
		(ucChannelWidth & VHT_OP_MODE_CHANNEL_WIDTH);

	if (ucOpRxNss == 0)
		ucOpRxNss = 1;
	prTxFrame->ucOperatingMode |=
		(((ucOpRxNss - 1) << 4) & VHT_OP_MODE_RX_NSS);
	prTxFrame->ucOperatingMode &= ~VHT_OP_MODE_RX_NSS_TYPE;

	if (prBssInfo->pfOpChangeHandler)
		pfTxDoneHandler = rlmNotifyVhtOpModeTxDone;

	/* 4 Update information of MSDU_INFO_T */
	TX_SET_MMPDU(prAdapter, prMsduInfo, prBssInfo->ucBssIndex,
		     prStaRec->ucIndex, WLAN_MAC_MGMT_HEADER_LEN,
		     sizeof(struct ACTION_OP_MODE_NOTIFICATION_FRAME),
		     pfTxDoneHandler, MSDU_RATE_MODE_AUTO);

	/* 4 Enqueue the frame to send this action frame. */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);

	return WLAN_STATUS_SUCCESS;
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
uint32_t rlmSendSmPowerSaveFrame(struct ADAPTER *prAdapter,
			     struct STA_RECORD *prStaRec, uint8_t ucOpRxNss)
{
	struct MSDU_INFO *prMsduInfo;
	struct ACTION_SM_POWER_SAVE_FRAME *prTxFrame;
	struct BSS_INFO *prBssInfo;
	uint16_t u2EstimatedFrameLen;
	PFN_TX_DONE_HANDLER pfTxDoneHandler = (PFN_TX_DONE_HANDLER)NULL;

	/* Sanity Check */
	if (!prStaRec)
		return WLAN_STATUS_FAILURE;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);
	if (!prBssInfo)
		return WLAN_STATUS_FAILURE;

	/* Calculate MSDU buffer length */
	u2EstimatedFrameLen = MAC_TX_RESERVED_FIELD +
			      sizeof(struct ACTION_SM_POWER_SAVE_FRAME);

	/* Alloc MSDU_INFO */
	prMsduInfo = (struct MSDU_INFO *)cnmMgtPktAlloc(prAdapter,
							u2EstimatedFrameLen);

	if (!prMsduInfo)
		return WLAN_STATUS_FAILURE;

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

	if (ucOpRxNss == 1)
		prTxFrame->ucSmPowerCtrl |= HT_SM_POWER_SAVE_CONTROL_ENABLED;
	else if (ucOpRxNss == 2)
		prTxFrame->ucSmPowerCtrl &= ~HT_SM_POWER_SAVE_CONTROL_ENABLED;
	else {
		DBGLOG(RLM, WARN,
		       "Can't switch to RxNss = %d since we don't support.\n",
		       ucOpRxNss);
		return WLAN_STATUS_FAILURE;
	}

	/* Static SM power save mode */
	prTxFrame->ucSmPowerCtrl &=
		(~HT_SM_POWER_SAVE_CONTROL_SM_MODE);

	if (prBssInfo->pfOpChangeHandler)
		pfTxDoneHandler = rlmSmPowerSaveTxDone;

	/* 4 Update information of MSDU_INFO_T */
	TX_SET_MMPDU(prAdapter, prMsduInfo, prBssInfo->ucBssIndex,
		     prStaRec->ucIndex, WLAN_MAC_MGMT_HEADER_LEN,
		     sizeof(struct ACTION_SM_POWER_SAVE_FRAME), pfTxDoneHandler,
		     MSDU_RATE_MODE_AUTO);

	/* 4 Enqueue the frame to send this action frame. */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Send Notify Channel Width frame (HT action frame)
 *
 * \param[in] ucChannelWidth 0:20MHz, 1:Any channel width
 *  in the STAs Supported Channel Width Set subfield
 *
 * \return none
 */
/*----------------------------------------------------------------------------*/
uint32_t rlmSendNotifyChannelWidthFrame(struct ADAPTER *prAdapter,
				    struct STA_RECORD *prStaRec,
				    uint8_t ucChannelWidth)
{
	struct MSDU_INFO *prMsduInfo;
	struct ACTION_NOTIFY_CHANNEL_WIDTH_FRAME *prTxFrame;
	struct BSS_INFO *prBssInfo;
	uint16_t u2EstimatedFrameLen;
	PFN_TX_DONE_HANDLER pfTxDoneHandler = (PFN_TX_DONE_HANDLER)NULL;

	/* Sanity Check */
	if (!prStaRec)
		return WLAN_STATUS_FAILURE;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);
	if (!prBssInfo)
		return WLAN_STATUS_FAILURE;

	/* Calculate MSDU buffer length */
	u2EstimatedFrameLen = MAC_TX_RESERVED_FIELD +
			      sizeof(struct ACTION_NOTIFY_CHANNEL_WIDTH_FRAME);

	/* Alloc MSDU_INFO */
	prMsduInfo = (struct MSDU_INFO *)cnmMgtPktAlloc(prAdapter,
							u2EstimatedFrameLen);

	if (!prMsduInfo)
		return WLAN_STATUS_FAILURE;

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

	if (prBssInfo->pfOpChangeHandler)
		pfTxDoneHandler = rlmNotifyChannelWidthtTxDone;

	/* 4 Update information of MSDU_INFO_T */
	TX_SET_MMPDU(prAdapter, prMsduInfo, prBssInfo->ucBssIndex,
		     prStaRec->ucIndex, WLAN_MAC_MGMT_HEADER_LEN,
		     sizeof(struct ACTION_NOTIFY_CHANNEL_WIDTH_FRAME),
		     pfTxDoneHandler, MSDU_RATE_MODE_AUTO);

	/* 4 Enqueue the frame to send this action frame. */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief
 *
 * \param[in]
 *
 * \return none
 *
 */
/*----------------------------------------------------------------------------*/
uint32_t rlmNotifyVhtOpModeTxDone(IN struct ADAPTER *prAdapter,
				  IN struct MSDU_INFO *prMsduInfo,
				  IN enum ENUM_TX_RESULT_CODE rTxDoneStatus)
{
	u_int8_t fgIsSuccess = FALSE;

	do {
		ASSERT((prAdapter != NULL) && (prMsduInfo != NULL));

		if (rTxDoneStatus == TX_RESULT_SUCCESS)
			fgIsSuccess = TRUE;

	} while (FALSE);

	rlmOpModeTxDoneHandler(prAdapter, prMsduInfo, OP_NOTIFY_TYPE_VHT_NSS_BW,
			       fgIsSuccess);

	return WLAN_STATUS_SUCCESS;
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
uint32_t rlmSmPowerSaveTxDone(IN struct ADAPTER *prAdapter,
			      IN struct MSDU_INFO *prMsduInfo,
			      IN enum ENUM_TX_RESULT_CODE rTxDoneStatus)
{
	u_int8_t fgIsSuccess = FALSE;

	do {
		ASSERT((prAdapter != NULL) && (prMsduInfo != NULL));

		if (rTxDoneStatus == TX_RESULT_SUCCESS)
			fgIsSuccess = TRUE;

	} while (FALSE);

	rlmOpModeTxDoneHandler(prAdapter, prMsduInfo, OP_NOTIFY_TYPE_HT_NSS,
			       fgIsSuccess);

	return WLAN_STATUS_SUCCESS;
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
uint32_t rlmNotifyChannelWidthtTxDone(IN struct ADAPTER *prAdapter,
				      IN struct MSDU_INFO *prMsduInfo,
				      IN enum ENUM_TX_RESULT_CODE rTxDoneStatus)
{
	u_int8_t fgIsSuccess = FALSE;

	do {
		ASSERT((prAdapter != NULL) && (prMsduInfo != NULL));

		if (rTxDoneStatus == TX_RESULT_SUCCESS)
			fgIsSuccess = TRUE;

	} while (FALSE);

	rlmOpModeTxDoneHandler(prAdapter, prMsduInfo, OP_NOTIFY_TYPE_HT_BW,
			       fgIsSuccess);

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Handle TX done for OP mode noritfication frame
 *
 * \param[in]
 *
 * \return none
 */
/*----------------------------------------------------------------------------*/
static void rlmOpModeTxDoneHandler(IN struct ADAPTER *prAdapter,
				   IN struct MSDU_INFO *prMsduInfo,
				   IN uint8_t ucOpChangeType,
				   IN u_int8_t fgIsSuccess)
{
	uint32_t u4Status = WLAN_STATUS_SUCCESS;
	struct BSS_INFO *prBssInfo = NULL;
	struct STA_RECORD *prStaRec = NULL;
	u_int8_t fgIsOpModeChangeSuccess = FALSE; /* OP change result */
	uint8_t ucRelatedFrameType =
		OP_NOTIFY_TYPE_NUM; /* Used for HT notification frame */
		/* Used for HT notification frame */
	uint8_t *pucCurrOpState = NULL;
	uint8_t *pucRelatedOpState = NULL;

	/* Sanity check */
	ASSERT((prAdapter != NULL) && (prMsduInfo != NULL));

	prBssInfo = prAdapter->aprBssInfo[prMsduInfo->ucBssIndex];

	ASSERT(prBssInfo);

	prStaRec = prBssInfo->prStaRecOfAP;

	DBGLOG(RLM, INFO,
	       "OP notification Tx done: BSS[%d] Type[%d] Status[%d] IsSuccess[%d]\n",
	       prBssInfo->ucBssIndex, ucOpChangeType,
	       prBssInfo->aucOpModeChangeState[ucOpChangeType], fgIsSuccess);

	do {
		/* <1>handle abnormal case */
		if ((prBssInfo->aucOpModeChangeState[ucOpChangeType] !=
		     OP_NOTIFY_STATE_KEEP) &&
		    (prBssInfo->aucOpModeChangeState[ucOpChangeType] !=
		     OP_NOTIFY_STATE_SENDING)) {
			DBGLOG(RLM, WARN,
			       "Unexpected BSS[%d] OpModeChangeState[%d]\n",
			       prBssInfo->ucBssIndex,
			       prBssInfo->aucOpModeChangeState[ucOpChangeType]);
			rlmRollbackOpChangeParam(prBssInfo, TRUE, TRUE);
			fgIsOpModeChangeSuccess = FALSE;
			break;
		}

		if (ucOpChangeType >= OP_NOTIFY_TYPE_NUM) {
			DBGLOG(RLM, WARN,
			       "Uxexpected Bss[%d] OpChangeType[%d]\n",
			       prMsduInfo->ucBssIndex, ucOpChangeType);
			rlmRollbackOpChangeParam(prBssInfo, TRUE, TRUE);
			fgIsOpModeChangeSuccess = FALSE;
			break;
		}

		pucCurrOpState = &prBssInfo
				->aucOpModeChangeState[ucOpChangeType];

		/* <2>Assign Op notification Type/State for HT notification
		 * frame
		 */
		if ((ucOpChangeType == OP_NOTIFY_TYPE_HT_BW) ||
		    (ucOpChangeType == OP_NOTIFY_TYPE_HT_NSS)) {

			ucRelatedFrameType =
				(ucOpChangeType == OP_NOTIFY_TYPE_HT_BW)
					? OP_NOTIFY_TYPE_HT_NSS
					: OP_NOTIFY_TYPE_HT_BW;

			pucRelatedOpState = &prBssInfo
				->aucOpModeChangeState[ucRelatedFrameType];
		}

		/* <3.1>handle TX done - SUCCESS */
		if (fgIsSuccess == TRUE) {

			/* Clear retry count */
			prBssInfo->aucOpModeChangeRetryCnt[ucOpChangeType] = 0;

			if (ucOpChangeType == OP_NOTIFY_TYPE_VHT_NSS_BW) {
				*pucCurrOpState = OP_NOTIFY_STATE_SUCCESS;

				if (prBssInfo->aucOpModeChangeState
					[OP_NOTIFY_TYPE_HT_BW] ==
						OP_NOTIFY_STATE_SENDING ||
					prBssInfo->aucOpModeChangeState
					[OP_NOTIFY_TYPE_HT_NSS] ==
						OP_NOTIFY_STATE_SENDING) {
					/* Wait for HT BW/Nss notification
					 * frames Tx done
					 */
					return;
				}

				/* VHT notification frame sent */
				fgIsOpModeChangeSuccess = TRUE;
				break;
			}

			/* HT notification frame sent */
			if (*pucCurrOpState ==
			    OP_NOTIFY_STATE_SENDING) { /* Change OpMode */
				*pucCurrOpState = OP_NOTIFY_STATE_SUCCESS;

				/* Case1: Wait for VHT notification frame &
				 * HT BW/Nss notification frame TX done
				 */
				if (*pucRelatedOpState ==
					OP_NOTIFY_STATE_SENDING ||
					prBssInfo->aucOpModeChangeState
					[OP_NOTIFY_TYPE_VHT_NSS_BW] ==
						OP_NOTIFY_STATE_SENDING)
					return;

				/* Case2: Both BW and Nss notification TX done
				 * or only change either BW or Nss
				 */
				if ((*pucRelatedOpState ==
				     OP_NOTIFY_STATE_KEEP) ||
				    (*pucRelatedOpState ==
				     OP_NOTIFY_STATE_SUCCESS)) {
					fgIsOpModeChangeSuccess = TRUE;

					/* Case3: One of the notification TX
					 * failed,
					 * re-send a notification frame to
					 * rollback the successful one
					 */
				} else if (*pucRelatedOpState ==
					   OP_NOTIFY_STATE_FAIL) {
					/*Rollback to keep the original BW/Nss
					 */
					*pucCurrOpState = OP_NOTIFY_STATE_KEEP;
					if (ucOpChangeType ==
					    OP_NOTIFY_TYPE_HT_BW)
						u4Status =
						rlmSendNotifyChannelWidthFrame(
						prAdapter, prStaRec,
						rlmGetBssOpBwByVhtAndHtOpInfo(
						prBssInfo));
					else if (ucOpChangeType ==
						 OP_NOTIFY_TYPE_HT_NSS)
						u4Status =
						rlmSendSmPowerSaveFrame(
							prAdapter, prStaRec,
							prBssInfo->ucOpRxNss);

					DBGLOG(RLM, INFO,
						"Bss[%d] OpType[%d] Tx Failed, send OpType",
						prMsduInfo->ucBssIndex,
						ucRelatedFrameType);
					DBGLOG(RLM, INFO,
						"[%d] for roll back to BW[%d] RxNss[%d]\n",
						ucOpChangeType,
						rlmGetBssOpBwByVhtAndHtOpInfo
							(prBssInfo),
						prBssInfo->ucOpRxNss);

					if (u4Status == WLAN_STATUS_SUCCESS)
						return;
				}
			} else if (*pucCurrOpState ==
				   OP_NOTIFY_STATE_KEEP) { /* Rollback OpMode */

				/* Case4: Rollback success, keep original OP
				 * BW/Nss
				 */
				if (ucOpChangeType == OP_NOTIFY_TYPE_HT_BW)
					rlmRollbackOpChangeParam(prBssInfo,
								 TRUE, FALSE);
				else if (ucOpChangeType ==
					 OP_NOTIFY_TYPE_HT_NSS)
					rlmRollbackOpChangeParam(prBssInfo,
								 FALSE, TRUE);

				fgIsOpModeChangeSuccess = FALSE;
			}
		} /* End of processing TX success */
		/* <3.2>handle TX done - FAIL */
		else {
			prBssInfo->aucOpModeChangeRetryCnt[ucOpChangeType]++;

			/* Re-send notification frame */
			if (prBssInfo
				    ->aucOpModeChangeRetryCnt[ucOpChangeType] <=
			    OPERATION_NOTICATION_TX_LIMIT) {
				if (ucOpChangeType == OP_NOTIFY_TYPE_VHT_NSS_BW)
					u4Status =
					rlmSendOpModeNotificationFrame(
					prAdapter, prStaRec,
					prBssInfo->ucOpChangeChannelWidth,
					prBssInfo->ucOpChangeRxNss);
				else if (ucOpChangeType ==
					 OP_NOTIFY_TYPE_HT_NSS)
					u4Status = rlmSendSmPowerSaveFrame(
						prAdapter, prStaRec,
						prBssInfo->ucOpChangeRxNss);
				else if (ucOpChangeType == OP_NOTIFY_TYPE_HT_BW)
					u4Status =
					rlmSendNotifyChannelWidthFrame(
						prAdapter, prStaRec,
						prBssInfo
						->ucOpChangeChannelWidth);

				if (u4Status == WLAN_STATUS_SUCCESS)
					return;
			}

			/* Clear retry count when retry count > TX limit */
			prBssInfo->aucOpModeChangeRetryCnt[ucOpChangeType] = 0;

			/* VHT notification frame sent */
			if (ucOpChangeType ==
			    OP_NOTIFY_TYPE_VHT_NSS_BW) {
				*pucCurrOpState = OP_NOTIFY_STATE_FAIL;

				/* Change failed, keep original OP BW/Nss */
				rlmRollbackOpChangeParam(prBssInfo, TRUE, TRUE);
				fgIsOpModeChangeSuccess = FALSE;
				break;
			}

			/* HT notification frame sent */
			if (*pucCurrOpState ==
			    OP_NOTIFY_STATE_SENDING) { /* Change OpMode */
				*pucCurrOpState = OP_NOTIFY_STATE_FAIL;

				/* Change failed, keep original OP BW/Nss */
				if (ucOpChangeType == OP_NOTIFY_TYPE_HT_BW)
					rlmRollbackOpChangeParam(prBssInfo,
								 TRUE, FALSE);
				else if (ucOpChangeType ==
					 OP_NOTIFY_TYPE_HT_NSS)
					rlmRollbackOpChangeParam(prBssInfo,
								 FALSE, TRUE);

				/* Case1: Wait for both HT BW/Nss notification
				 * frame TX done
				 */
				if (*pucRelatedOpState ==
				    OP_NOTIFY_STATE_SENDING) {
					return;

					/* Case2: Both BW and Nss notification
					 * TX done
					 * or only change either BW or Nss
					 */
				} else if ((*pucRelatedOpState ==
					    OP_NOTIFY_STATE_KEEP) ||
					   (*pucRelatedOpState ==
					    OP_NOTIFY_STATE_FAIL)) {
					fgIsOpModeChangeSuccess = FALSE;

					/* Case3: One of the notification TX
					 * failed,
					 * re-send a notification frame to
					 * rollback the successful one
					 */
				} else if (*pucRelatedOpState ==
					   OP_NOTIFY_STATE_SUCCESS) {
					/*Rollback to keep the original BW/Nss
					 */
					*pucRelatedOpState =
						OP_NOTIFY_STATE_KEEP;

					if (ucRelatedFrameType ==
					    OP_NOTIFY_TYPE_HT_BW) {
						u4Status =
						rlmSendNotifyChannelWidthFrame(
						prAdapter, prStaRec,
						rlmGetBssOpBwByVhtAndHtOpInfo(
						prBssInfo));
					} else if (ucRelatedFrameType ==
						   OP_NOTIFY_TYPE_HT_NSS)
						u4Status =
						rlmSendSmPowerSaveFrame(
							prAdapter, prStaRec,
							prBssInfo->ucOpRxNss);

					DBGLOG(RLM, INFO,
					       "Bss[%d] OpType[%d] Tx Failed, send a OpType[%d] for roll back to BW[%d] RxNss[%d]\n",
					       prMsduInfo->ucBssIndex,
					       ucOpChangeType,
					       ucRelatedFrameType,
					       rlmGetBssOpBwByVhtAndHtOpInfo(
						       prBssInfo),
					       prBssInfo->ucOpRxNss);

					if (u4Status == WLAN_STATUS_SUCCESS)
						return;
				}
			} else if (*pucCurrOpState ==
				   OP_NOTIFY_STATE_KEEP) /* Rollback OpMode */
				/* Case4: Rollback failed, keep changing OP
				 * BW/Nss
				 */
				fgIsOpModeChangeSuccess = FALSE;
		} /* End of processing TX failed */

	} while (FALSE);

	/* <4>Change own OP info */
	rlmCompleteOpModeChange(prAdapter, prBssInfo, fgIsOpModeChangeSuccess);
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
static void rlmRollbackOpChangeParam(struct BSS_INFO *prBssInfo,
				     u_int8_t fgIsRollbackBw,
				     u_int8_t fgIsRollbackNss)
{

	ASSERT(prBssInfo);

	if (fgIsRollbackBw == TRUE) {
		prBssInfo->fgIsOpChangeChannelWidth = FALSE;
		prBssInfo->ucOpChangeChannelWidth =
			rlmGetBssOpBwByVhtAndHtOpInfo(prBssInfo);
	}

	if (fgIsRollbackNss == TRUE) {
		prBssInfo->fgIsOpChangeRxNss = FALSE;
		prBssInfo->fgIsOpChangeTxNss = FALSE;
		prBssInfo->ucOpChangeRxNss = prBssInfo->ucOpRxNss;
		prBssInfo->ucOpChangeTxNss = prBssInfo->ucOpTxNss;
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Get BSS operating channel width by VHT and HT OP Info
 *
 * \param[in]
 *
 * \return ucBssOpBw 0:20MHz, 1:40MHz, 2:80MHz, 3:160MHz 4:80+80MHz
 *
 */
/*----------------------------------------------------------------------------*/
uint8_t rlmGetBssOpBwByVhtAndHtOpInfo(struct BSS_INFO *prBssInfo)
{

	uint8_t ucBssOpBw = MAX_BW_20MHZ;

	ASSERT(prBssInfo);

	switch (prBssInfo->ucVhtChannelWidth) {
	case VHT_OP_CHANNEL_WIDTH_80P80:
		ucBssOpBw = MAX_BW_80_80_MHZ;
		break;

	case VHT_OP_CHANNEL_WIDTH_160:
		ucBssOpBw = MAX_BW_160MHZ;
		break;

	case VHT_OP_CHANNEL_WIDTH_80:
		ucBssOpBw = MAX_BW_80MHZ;
		break;

	case VHT_OP_CHANNEL_WIDTH_20_40:
		if (prBssInfo->eBssSCO != CHNL_EXT_SCN)
			ucBssOpBw = MAX_BW_40MHZ;
		break;
	default:
		DBGLOG(RLM, WARN, "%s: unexpected VHT channel width: %d\n",
		       __func__, prBssInfo->ucVhtChannelWidth);
#if CFG_SUPPORT_802_11AC
		if (RLM_NET_IS_11AC(prBssInfo))
			/*VHT default should support BW 80*/
			ucBssOpBw = MAX_BW_80MHZ;
#endif
		break;
	}

	return ucBssOpBw;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief
 *
 * \param[in]
 *
 * \return ucVhtOpBw 0:20M/40Hz, 1:80MHz, 2:160MHz, 3:80+80MHz
 *
 */
/*----------------------------------------------------------------------------*/
uint8_t rlmGetVhtOpBwByBssOpBw(uint8_t ucBssOpBw)
{
	uint8_t ucVhtOpBw =
		VHT_OP_CHANNEL_WIDTH_80; /*VHT default should support BW 80*/

	switch (ucBssOpBw) {
	case MAX_BW_20MHZ:
	case MAX_BW_40MHZ:
		ucVhtOpBw = VHT_OP_CHANNEL_WIDTH_20_40;
		break;

	case MAX_BW_80MHZ:
		ucVhtOpBw = VHT_OP_CHANNEL_WIDTH_80;
		break;

	case MAX_BW_160MHZ:
		ucVhtOpBw = VHT_OP_CHANNEL_WIDTH_160;
		break;

	case MAX_BW_80_80_MHZ:
		ucVhtOpBw = VHT_OP_CHANNEL_WIDTH_80P80;
		break;
	default:
		DBGLOG(RLM, WARN, "%s: unexpected Bss OP BW: %d\n", __func__,
		       ucBssOpBw);
		break;
	}

	return ucVhtOpBw;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Get operating notification channel width by VHT and HT operating Info
 *
 * \param[in]
 *
 * \return ucOpModeBw 0:20MHz, 1:40MHz, 2:80MHz, 3:160MHz/80+80MHz
 *
 */
/*----------------------------------------------------------------------------*/
static uint8_t rlmGetOpModeBwByVhtAndHtOpInfo(struct BSS_INFO *prBssInfo)
{
	uint8_t ucOpModeBw = VHT_OP_MODE_CHANNEL_WIDTH_20;

	ASSERT(prBssInfo);

	switch (prBssInfo->ucVhtChannelWidth) {
	case VHT_OP_CHANNEL_WIDTH_20_40:
		if (prBssInfo->eBssSCO != CHNL_EXT_SCN)
			ucOpModeBw = VHT_OP_MODE_CHANNEL_WIDTH_40;
		break;
	case VHT_OP_CHANNEL_WIDTH_80:
		ucOpModeBw = VHT_OP_MODE_CHANNEL_WIDTH_80;
		break;
	case VHT_OP_CHANNEL_WIDTH_160:
	case VHT_OP_CHANNEL_WIDTH_80P80:
		ucOpModeBw = VHT_OP_MODE_CHANNEL_WIDTH_160_80P80;
		break;
	default:
		DBGLOG(RLM, WARN, "%s: unexpected VHT channel width: %d\n",
		       __func__, prBssInfo->ucVhtChannelWidth);
		/*VHT default IE should support BW 80*/
		ucOpModeBw = VHT_OP_MODE_CHANNEL_WIDTH_80;
		break;
	}

	return ucOpModeBw;
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
static void rlmChangeOwnOpInfo(struct ADAPTER *prAdapter,
			       struct BSS_INFO *prBssInfo)
{
	struct STA_RECORD *prStaRec;

	ASSERT((prAdapter != NULL) && (prBssInfo != NULL));

	/* Update own operating channel Width */
	if (prBssInfo->fgIsOpChangeChannelWidth) {
		if (prBssInfo->ucPhyTypeSet & PHY_TYPE_BIT_HT) {
#if CFG_SUPPORT_802_11AC
			/* Update VHT OP Info*/
			if (prBssInfo->ucPhyTypeSet & PHY_TYPE_BIT_VHT) {
				rlmFillVhtOpInfoByBssOpBw(
					prBssInfo,
					prBssInfo->ucOpChangeChannelWidth);

				DBGLOG(RLM, INFO,
				       "Update BSS[%d] VHT Channel Width Info to w=%d s1=%d s2=%d\n",
				       prBssInfo->ucBssIndex,
				       prBssInfo->ucVhtChannelWidth,
				       prBssInfo->ucVhtChannelFrequencyS1,
				       prBssInfo->ucVhtChannelFrequencyS2);
			}
#endif

			/* Update HT OP Info*/
			if (prBssInfo->ucOpChangeChannelWidth == MAX_BW_20MHZ) {
				prBssInfo->ucHtOpInfo1 &=
					~HT_OP_INFO1_STA_CHNL_WIDTH;
				prBssInfo->eBssSCO = CHNL_EXT_SCN;
			} else {
				prBssInfo->ucHtOpInfo1 |=
					HT_OP_INFO1_STA_CHNL_WIDTH;

				if (prBssInfo->eCurrentOPMode ==
				    OP_MODE_INFRASTRUCTURE) {
					prStaRec = prBssInfo->prStaRecOfAP;
					if (!prStaRec)
						return;

					if ((prStaRec->ucHtPeerOpInfo1 &
					     HT_OP_INFO1_SCO) != CHNL_EXT_RES)
						prBssInfo->eBssSCO =
						(enum ENUM_CHNL_EXT)(
						prStaRec->ucHtPeerOpInfo1 &
						HT_OP_INFO1_SCO);
				} else if (prBssInfo->eCurrentOPMode ==
					   OP_MODE_ACCESS_POINT) {
					prBssInfo->eBssSCO = rlmDecideScoForAP(
						prAdapter, prBssInfo);
				}
			}

			DBGLOG(RLM, INFO,
			       "Update BSS[%d] HT Channel Width Info to bw=%d sco=%d\n",
			       prBssInfo->ucBssIndex,
			       (uint8_t)((prBssInfo->ucHtOpInfo1 &
					  HT_OP_INFO1_STA_CHNL_WIDTH) >>
					 HT_OP_INFO1_STA_CHNL_WIDTH_OFFSET),
			       prBssInfo->eBssSCO);
		}
	}

	/* Update own operating RxNss */
	if (prBssInfo->fgIsOpChangeRxNss) {
		prBssInfo->ucOpRxNss = prBssInfo->ucOpChangeRxNss;
		DBGLOG(RLM, INFO, "Update OP RxNss[%d]\n",
			prBssInfo->ucOpRxNss);
	}

	/* Update own operating TxNss */
	if (prBssInfo->fgIsOpChangeTxNss) {
		prBssInfo->ucOpTxNss = prBssInfo->ucOpChangeTxNss;
		DBGLOG(RLM, INFO, "Update OP TxNss[%d]\n",
			prBssInfo->ucOpTxNss);
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
static void rlmCompleteOpModeChange(struct ADAPTER *prAdapter,
				    struct BSS_INFO *prBssInfo,
				    u_int8_t fgIsSuccess)
{
	PFN_OPMODE_NOTIFY_DONE_FUNC pfnCallback;

	ASSERT((prAdapter != NULL) && (prBssInfo != NULL));

	if ((prBssInfo->fgIsOpChangeChannelWidth) ||
		(prBssInfo->fgIsOpChangeRxNss) ||
		(prBssInfo->fgIsOpChangeTxNss)) {

		/* <1> Update own OP BW/Nss */
		rlmChangeOwnOpInfo(prAdapter, prBssInfo);

		/* <2> Update OP BW/Nss to FW */
		rlmSyncOperationParams(prAdapter, prBssInfo);

		/* <3> Update BCN/Probe Resp IE to notify peers our OP info is
		 * changed (AP mode)
		 */
		if (prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT)
			bssUpdateBeaconContent(prAdapter,
					       prBssInfo->ucBssIndex);
	}

	DBGLOG(RLM, INFO,
		"Complete BSS[%d] OP Mode change to BW[%d] RxNss[%d] TxNss[%d]",
		prBssInfo->ucBssIndex,
		rlmGetBssOpBwByVhtAndHtOpInfo(prBssInfo),
		prBssInfo->ucOpRxNss,
		prBssInfo->ucOpTxNss);

	/* <4> Tell OpMode change caller the change result
	 * Allow callback function re-trigger OpModeChange immediately.
	 */
	pfnCallback = prBssInfo->pfOpChangeHandler;
	prBssInfo->pfOpChangeHandler = NULL;
	if (pfnCallback)
		pfnCallback(prAdapter, prBssInfo->ucBssIndex,
					     fgIsSuccess);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Change OpMode Nss/Channel Width
 *
 * \param[in] ucChannelWidth 0:20MHz, 1:40MHz, 2:80MHz, 3:160MHz 4:80+80MHz
 *
 * \return fgIsChangeOpMode
 *	TRUE: Can change/Don't need to change operation mode
 *	FALSE: Can't change operation mode
 */
/*----------------------------------------------------------------------------*/
enum ENUM_OP_CHANGE_STATUS_T
rlmChangeOperationMode(
	struct ADAPTER *prAdapter, uint8_t ucBssIndex,
	uint8_t ucChannelWidth, uint8_t ucOpRxNss, uint8_t ucOpTxNss,
	#if CFG_SUPPORT_SMART_GEAR
	uint8_t eNewReq,
	#endif
	PFN_OPMODE_NOTIFY_DONE_FUNC pfOpChangeHandler
	)
{
	struct BSS_INFO *prBssInfo;
	struct STA_RECORD *prStaRec = (struct STA_RECORD *)NULL;
	u_int8_t fgIsChangeBw = TRUE,
		 fgIsChangeRxNss = TRUE, /* Indicate if need to change */
		 fgIsChangeTxNss = TRUE;
	uint8_t i;
	uint32_t u4Status = WLAN_STATUS_SUCCESS;

	/* Sanity check */
	if (ucBssIndex >= prAdapter->ucHwBssIdNum)
		return OP_CHANGE_STATUS_INVALID;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

	if (!prBssInfo)
		return OP_CHANGE_STATUS_INVALID;

	/* <1>Check if OP change parameter is valid */
	if (rlmCheckOpChangeParamValid(prAdapter, prBssInfo, ucChannelWidth,
				       ucOpRxNss, ucOpTxNss) == FALSE)
		return OP_CHANGE_STATUS_INVALID;

	/* <2>Check if OpMode notification is ongoing, if not, register the call
	 * back function
	 */
	if (prBssInfo->pfOpChangeHandler) {
		DBGLOG(RLM, INFO,
		       "BSS[%d] OpMode change notification is ongoing\n",
		       ucBssIndex);
		return OP_CHANGE_STATUS_INVALID;
	}

	prBssInfo->pfOpChangeHandler = pfOpChangeHandler;

	/* <3>Check if the current operating BW/Nss is the same as the target
	 * one
	 */
	if (ucChannelWidth == rlmGetBssOpBwByVhtAndHtOpInfo(prBssInfo)) {
		fgIsChangeBw = FALSE;
		prBssInfo->fgIsOpChangeChannelWidth = FALSE;
	}
	if (ucOpRxNss == prBssInfo->ucOpRxNss) {
		fgIsChangeRxNss = FALSE;
		prBssInfo->fgIsOpChangeRxNss = FALSE;
	}
	if (ucOpTxNss == prBssInfo->ucOpTxNss) {
		fgIsChangeTxNss = FALSE;
		prBssInfo->fgIsOpChangeTxNss = FALSE;
	}
	if ((!fgIsChangeBw) && (!fgIsChangeRxNss) && (!fgIsChangeTxNss)) {
		DBGLOG(RLM, INFO,
			"BSS[%d] target OpMode BW[%d] RxNss[%d] TxNss[%d] No change, return\n",
			ucBssIndex, ucChannelWidth, ucOpRxNss, ucOpTxNss);
		rlmCompleteOpModeChange(prAdapter, prBssInfo, TRUE);
		return OP_CHANGE_STATUS_VALID_NO_CHANGE;
	}

	DBGLOG(RLM, INFO,
		"Intend to change BSS[%d] OP Mode to BW[%d] RxNss[%d] TxNss[%d]\n",
		ucBssIndex, ucChannelWidth, ucOpRxNss, ucOpTxNss);

	/* <4> Fill OP Change Info into BssInfo*/
	if (fgIsChangeBw) {
		prBssInfo->ucOpChangeChannelWidth = ucChannelWidth;
		prBssInfo->fgIsOpChangeChannelWidth = TRUE;
		DBGLOG(RLM, INFO, "Intend to change BSS[%d] to BW[%d]\n",
		       ucBssIndex, ucChannelWidth);
	}
	if (fgIsChangeRxNss) {
		prBssInfo->ucOpChangeRxNss = ucOpRxNss;
		prBssInfo->fgIsOpChangeRxNss = TRUE;
		DBGLOG(RLM, INFO, "Intend to change BSS[%d] to RxNss[%d]\n",
		       ucBssIndex, ucOpRxNss);
	}
	if (fgIsChangeTxNss) {
		prBssInfo->ucOpChangeTxNss = ucOpTxNss;
		prBssInfo->fgIsOpChangeTxNss = TRUE;
		DBGLOG(RLM, INFO, "Intend to change BSS[%d] to TxNss[%d]\n",
			ucBssIndex, ucOpTxNss);
	}

	/* <5>Handling OP Info change for STA/GC */
	if ((prBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE) &&
	    (prBssInfo->prStaRecOfAP)) {
		prStaRec = prBssInfo->prStaRecOfAP;
		/* <5.1>Initialize OP mode change parameters related to
		 * notification Tx done handler (STA mode)
		 */
		if (prBssInfo->pfOpChangeHandler) {
			for (i = 0; i < OP_NOTIFY_TYPE_NUM; i++) {
				prBssInfo->aucOpModeChangeState[i] =
					OP_NOTIFY_STATE_KEEP;
				prBssInfo->aucOpModeChangeRetryCnt[i] = 0;
			}
		}

#if CFG_SUPPORT_SMART_GEAR
		if (eNewReq != 0x04 /* CNM_OPMODE_REQ_SMARTGEAR_1T2R */) {
#endif		/* <5.2> Send operating mode notification frame (STA mode)
		 * No action frame is needed if we only changed OpTxNss.
		 */
#if CFG_SUPPORT_802_11AC
		if (RLM_NET_IS_11AC(prBssInfo) &&
			(fgIsChangeBw || fgIsChangeRxNss)) {
			if (prBssInfo->pfOpChangeHandler)
				prBssInfo->aucOpModeChangeState
					[OP_NOTIFY_TYPE_VHT_NSS_BW] =
					OP_NOTIFY_STATE_SENDING;
			DBGLOG(RLM, INFO,
				"Send VHT OP notification frame: BSS[%d] BW[%d] RxNss[%d]\n",
				ucBssIndex, ucChannelWidth, ucOpRxNss);
			u4Status = rlmSendOpModeNotificationFrame(
				prAdapter, prStaRec,
				ucChannelWidth, ucOpRxNss);
		}
#endif
		if (RLM_NET_IS_11N(prBssInfo) &&
			(fgIsChangeBw || fgIsChangeRxNss)) {
			if (prBssInfo->pfOpChangeHandler) {
				if (fgIsChangeRxNss)
					prBssInfo->aucOpModeChangeState
						[OP_NOTIFY_TYPE_HT_NSS] =
						OP_NOTIFY_STATE_SENDING;
				if (fgIsChangeBw)
					prBssInfo->aucOpModeChangeState
						[OP_NOTIFY_TYPE_HT_BW] =
						OP_NOTIFY_STATE_SENDING;
			}
			if (fgIsChangeRxNss) {
				u4Status = rlmSendSmPowerSaveFrame(
					prAdapter, prStaRec, ucOpRxNss);
				DBGLOG(RLM, INFO,
					"Send HT SM Power Save frame: ");
				DBGLOG(RLM, INFO, "BSS[%d] RxNss[%d]\n",
					ucBssIndex, ucOpRxNss);
			}
			if (fgIsChangeBw) {
				u4Status = rlmSendNotifyChannelWidthFrame(
					prAdapter, prStaRec, ucChannelWidth);
				DBGLOG(RLM, INFO,
					"Send HT Notify Channel Width frame: ");
				DBGLOG(RLM, INFO, "BSS[%d] BW[%d]\n",
					ucBssIndex, ucChannelWidth);
			}
		}
#if CFG_SUPPORT_SMART_GEAR
	}
#endif
		/* Error handling */
		if (u4Status != WLAN_STATUS_SUCCESS) {
			rlmCompleteOpModeChange(prAdapter, prBssInfo, FALSE);
			return OP_CHANGE_STATUS_INVALID;
		}

		/* <5.3> Change OP Info w/o waiting for notification Tx done */
		if (prBssInfo->pfOpChangeHandler == NULL ||
#if CFG_SUPPORT_SMART_GEAR
			eNewReq == 0x04 /* CNM_OPMODE_REQ_SMARTGEAR_1T2R */ ||
#endif
			(!fgIsChangeBw && !fgIsChangeRxNss)) {
			rlmCompleteOpModeChange(prAdapter, prBssInfo, TRUE);
			/* No callback */
			return OP_CHANGE_STATUS_VALID_CHANGE_CALLBACK_DONE;
		}

		return OP_CHANGE_STATUS_VALID_CHANGE_CALLBACK_WAIT;
	}
	/* <6>Handling OP Info change for AP/GO */
	else if (prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT) {
		/* Complete OP Info change after notifying client by beacon */
		rlmCompleteOpModeChange(prAdapter, prBssInfo, TRUE);
		return OP_CHANGE_STATUS_VALID_CHANGE_CALLBACK_DONE;
	}

	/* Complete OP mode change if no sending action frames */
	rlmCompleteOpModeChange(prAdapter, prBssInfo, TRUE);
	return OP_CHANGE_STATUS_VALID_CHANGE_CALLBACK_DONE;
}


/*----------------------------------------------------------------------------*/
/*!
 * \brief Check our desired BW/RxNss is less or equal than peer's capability to
 *        prevent IOT issue.
 *
 * \param[in] prBssInfo
 * \param[in] ucChannelWidth
 * \param[in] ucOpTxNss
 *
 * \return
 *	TRUE : Can change to desired BW/RxNss
 *	FALSE: Should not change operation mode
 */
/*----------------------------------------------------------------------------*/
static u_int8_t rlmCheckOpChangeParamForClient(struct BSS_INFO *prBssInfo,
					       uint8_t ucChannelWidth,
					       uint8_t ucOpRxNss)
{
	struct STA_RECORD *prStaRec;

	prStaRec = prBssInfo->prStaRecOfAP;

	if (!prStaRec)
		return FALSE;

#if CFG_SUPPORT_802_11AC
	if (RLM_NET_IS_11AC(prBssInfo)) { /* VHT */
		/* Check peer OP Channel Width */
		switch (ucChannelWidth) {
		case MAX_BW_80_80_MHZ:
			if (prStaRec->ucVhtOpChannelWidth !=
			    VHT_OP_CHANNEL_WIDTH_80P80) {
				DBGLOG(RLM, INFO,
				       "Can't change BSS[%d] OP BW to:%d for peer VHT OP BW is:%d\n",
				       prBssInfo->ucBssIndex, ucChannelWidth,
				       prStaRec->ucVhtOpChannelWidth);
				return FALSE;
			}
			break;
		case MAX_BW_160MHZ:
			if (prStaRec->ucVhtOpChannelWidth !=
			    VHT_OP_CHANNEL_WIDTH_160) {
				DBGLOG(RLM, INFO,
				       "Can't change BSS[%d] OP BW to:%d for peer VHT OP BW is:%d\n",
				       prBssInfo->ucBssIndex, ucChannelWidth,
				       prStaRec->ucVhtOpChannelWidth);
				return FALSE;
			}
			break;
		case MAX_BW_80MHZ:
			if (prStaRec->ucVhtOpChannelWidth <
			    VHT_OP_CHANNEL_WIDTH_80) {
				DBGLOG(RLM, INFO,
				       "Can't change BSS[%d] OP BW to:%d for peer VHT OP BW is:%d\n",
				       prBssInfo->ucBssIndex, ucChannelWidth,
				       prStaRec->ucVhtOpChannelWidth);
				return FALSE;
			}
			break;
		case MAX_BW_40MHZ:
			if (!(prStaRec->ucHtPeerOpInfo1 &
			      HT_OP_INFO1_STA_CHNL_WIDTH) ||
			    (!prBssInfo->fg40mBwAllowed)) {
				DBGLOG(RLM, INFO,
				       "Can't change BSS[%d] OP BW to:%d for PeerOpBw:%d fg40mBwAllowed:%d\n",
				       prBssInfo->ucBssIndex, ucChannelWidth,
				       (uint8_t)(prStaRec->ucHtPeerOpInfo1 &
						 HT_OP_INFO1_STA_CHNL_WIDTH),
				       prBssInfo->fg40mBwAllowed);
				return FALSE;
			}
			break;
		case MAX_BW_20MHZ:
			break;
		default:
			DBGLOG(RLM, WARN,
			       "BSS[%d] target OP BW:%d is invalid for VHT OpMode change\n",
			       prBssInfo->ucBssIndex, ucChannelWidth);
			return FALSE;
		}

		/* Check peer Rx Nss Cap */
		if (ucOpRxNss == 2 &&
		    ((prStaRec->u2VhtRxMcsMap & VHT_CAP_INFO_MCS_2SS_MASK) >>
		     VHT_CAP_INFO_MCS_2SS_OFFSET) ==
			    VHT_CAP_INFO_MCS_NOT_SUPPORTED) {
			DBGLOG(RLM, INFO,
			       "Don't change Nss since VHT peer doesn't support 2ss\n");
			return FALSE;
		}

	} else
#endif
	{
		if (RLM_NET_IS_11N(prBssInfo)) { /* HT */
			/* Check peer Channel Width */
			if (ucChannelWidth >= MAX_BW_80MHZ) {
				DBGLOG(RLM, WARN,
				       "BSS[%d] target OP BW:%d is invalid for HT OpMode change\n",
				       prBssInfo->ucBssIndex, ucChannelWidth);
				return FALSE;
			} else if (ucChannelWidth ==
					MAX_BW_40MHZ) {
				if (!(prStaRec->ucHtPeerOpInfo1 &
				      HT_OP_INFO1_STA_CHNL_WIDTH) ||
				    (!prBssInfo->fg40mBwAllowed)) {
					DBGLOG(RLM, INFO,
					       "Can't change BSS[%d] OP BW to:%d for PeerOpBw:%d fg40mBwAllowed:%d\n",
					       prBssInfo->ucBssIndex,
					       ucChannelWidth,
					       (uint8_t)(
					prStaRec->ucHtPeerOpInfo1 &
					HT_OP_INFO1_STA_CHNL_WIDTH),
					prBssInfo->fg40mBwAllowed);
					return FALSE;
				}
			}

			/* Check peer Rx Nss Cap */
			if (ucOpRxNss == 2 &&
				(prStaRec->aucRxMcsBitmask[1] == 0)) {
				DBGLOG(RLM, INFO,
				       "Don't change Nss since HT peer doesn't support 2ss\n");
				return FALSE;
			}
		}
	}
	return TRUE;
}

static u_int8_t rlmCheckOpChangeParamValid(struct ADAPTER *prAdapter,
					   struct BSS_INFO *prBssInfo,
					   uint8_t ucChannelWidth,
					   uint8_t ucOpRxNss,
					   uint8_t ucOpTxNss)
{
	uint8_t ucCapNss;

	ASSERT(prBssInfo);

	/* <1>Check if BSS PHY type is legacy mode */
	if (!RLM_NET_IS_11N(prBssInfo)) {
		DBGLOG(RLM, WARN,
		       "Can't change BSS[%d] OP info for legacy BSS\n",
		       prBssInfo->ucBssIndex);
		return FALSE;
	}

	/* <2>Check network type */
	if ((prBssInfo->eCurrentOPMode != OP_MODE_INFRASTRUCTURE) &&
	    (prBssInfo->eCurrentOPMode != OP_MODE_ACCESS_POINT)) {
		DBGLOG(RLM, WARN,
		       "Can't change BSS[%d] OP info for OpMode:%d\n",
		       prBssInfo->ucBssIndex, prBssInfo->eCurrentOPMode);
		return FALSE;
	}

	/* <3>Check if target OP BW/Nss <= Own Cap BW/Nss */
	ucCapNss = wlanGetSupportNss(prAdapter, prBssInfo->ucBssIndex);
	if (ucOpRxNss > ucCapNss || ucOpRxNss == 0 ||
		ucOpTxNss > ucCapNss || ucOpTxNss == 0) {
		DBGLOG(RLM, WARN,
		       "Can't change BSS[%d] OP RxNss[%d]TxNss[%d] due to CapNss[%d]\n",
		       prBssInfo->ucBssIndex, ucOpRxNss, ucOpTxNss, ucCapNss);
		return FALSE;
	}

	if (ucChannelWidth > cnmGetBssMaxBw(prAdapter, prBssInfo->ucBssIndex)) {
		DBGLOG(RLM, WARN,
		       "Can't change BSS[%d] OP BW[%d] due to CapBW[%d]\n",
		       prBssInfo->ucBssIndex, ucChannelWidth,
		       cnmGetBssMaxBw(prAdapter, prBssInfo->ucBssIndex));
		return FALSE;
	}

	/* <4>Check if target OP BW is valid for band and primary channel of
	 * current BSS
	 */
	if (prBssInfo->eBand == BAND_2G4) {
		if ((ucChannelWidth != MAX_BW_20MHZ) &&
		    (ucChannelWidth != MAX_BW_40MHZ)) {
			DBGLOG(RLM, WARN,
			       "Can't change BSS[%d] OP BW to:%d for 2.4G\n",
			       prBssInfo->ucBssIndex, ucChannelWidth);
			return FALSE;
		}
	} else {
		/* It can only use BW20 for CH165 */
		if ((ucChannelWidth != MAX_BW_20MHZ) &&
			(prBssInfo->ucPrimaryChannel == 165)) {
			DBGLOG(RLM, WARN,
			       "Can't change BSS[%d] OP BW to:%d for CH165\n",
			       prBssInfo->ucBssIndex, ucChannelWidth);
			return FALSE;
		}

		if ((ucChannelWidth == MAX_BW_160MHZ) &&
		    ((prBssInfo->ucPrimaryChannel < 36) ||
		     ((prBssInfo->ucPrimaryChannel > 64) &&
		      (prBssInfo->ucPrimaryChannel < 100)) ||
		     (prBssInfo->ucPrimaryChannel > 128))) {
			DBGLOG(RLM, WARN,
			       "Can't change BSS[%d] to OP BW160 for primary CH%d\n",
			       prBssInfo->ucBssIndex,
			       prBssInfo->ucPrimaryChannel);
			return FALSE;
		}
	}

	/* <5>Check if target OP BW/Nss <= peer's BW/Nss (STA mode) */
	if (prBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE) {
		if (rlmCheckOpChangeParamForClient(prBssInfo, ucChannelWidth,
						   ucOpRxNss) == FALSE)
			return FALSE;
	}

	return TRUE;
}

void rlmDummyChangeOpHandler(struct ADAPTER *prAdapter, uint8_t ucBssIndex,
			     bool fgIsChangeSuccess)
{
	DBGLOG(RLM, INFO, "OP change done for BSS[%d] IsSuccess[%d]\n",
	       ucBssIndex, fgIsChangeSuccess);
}

void rlmSetMaxTxPwrLimit(IN struct ADAPTER *prAdapter, int8_t cLimit,
			 uint8_t ucEnable)
{
	struct CMD_SET_AP_CONSTRAINT_PWR_LIMIT rTxPwrLimit;

	kalMemZero(&rTxPwrLimit, sizeof(rTxPwrLimit));
	rTxPwrLimit.ucCmdVer =  0x1;
	rTxPwrLimit.ucPwrSetEnable =  ucEnable;
	if (ucEnable) {
		if (cLimit > RLM_MAX_TX_PWR) {
			DBGLOG(RLM, INFO,
			       "LM: Target MaxPwr %d Higher than Capability, reset to capability\n",
			       cLimit);
			cLimit = RLM_MAX_TX_PWR;
		}
		if (cLimit < RLM_MIN_TX_PWR) {
			DBGLOG(RLM, INFO,
			       "LM: Target MinPwr %d Lower than Capability, reset to capability\n",
			       cLimit);
			cLimit = RLM_MIN_TX_PWR;
		}
		DBGLOG(RLM, INFO,
		       "LM: Set Max Tx Power Limit %d, Min Limit %d\n", cLimit,
		       RLM_MIN_TX_PWR);
		rTxPwrLimit.cMaxTxPwr =
			cLimit * 2; /* unit of cMaxTxPwr is 0.5 dBm */
		rTxPwrLimit.cMinTxPwr = RLM_MIN_TX_PWR * 2;
	} else
		DBGLOG(RLM, TRACE, "LM: Disable Tx Power Limit\n");
	wlanSendSetQueryCmd(prAdapter, CMD_ID_SET_AP_CONSTRAINT_PWR_LIMIT, TRUE,
			    FALSE, FALSE, nicCmdEventSetCommon,
			    nicOidCmdTimeoutCommon,
			    sizeof(struct CMD_SET_AP_CONSTRAINT_PWR_LIMIT),
			    (uint8_t *)&rTxPwrLimit, NULL, 0);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Send channel switch frame
 *
 * \param[in]
 *
 * \return none
 */
/*----------------------------------------------------------------------------*/
uint32_t rlmSendChannelSwitchTxDone(IN struct ADAPTER *prAdapter,
	IN struct MSDU_INFO *prMsduInfo,
	IN enum ENUM_TX_RESULT_CODE rTxDoneStatus)
{
	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsduInfo != NULL));

		DBGLOG(P2P, INFO,
			"CSA TX Done Status: %d, seqNo: %d\n",
			rTxDoneStatus,
			prMsduInfo->ucTxSeqNum);

	} while (FALSE);

	return WLAN_STATUS_SUCCESS;
}

void rlmSendChannelSwitchFrame(struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex)
{
	struct MSDU_INFO *prMsduInfo;
	struct ACTION_CHANNEL_SWITCH_FRAME *prTxFrame;
	struct BSS_INFO *prBssInfo;
	uint16_t u2EstimatedFrameLen;
	PFN_TX_DONE_HANDLER pfTxDoneHandler = (PFN_TX_DONE_HANDLER)NULL;
	uint8_t aucBMC[] = BC_MAC_ADDR;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);
	if (!prBssInfo)
		return;

	/* Calculate MSDU buffer length */
	u2EstimatedFrameLen = MAC_TX_RESERVED_FIELD +
			      sizeof(struct ACTION_CHANNEL_SWITCH_FRAME);

	/* Alloc MSDU_INFO */
	prMsduInfo = (struct MSDU_INFO *)cnmMgtPktAlloc(prAdapter,
							u2EstimatedFrameLen);
	if (!prMsduInfo)
		return;

	kalMemZero(prMsduInfo->prPacket, u2EstimatedFrameLen);

	prTxFrame = prMsduInfo->prPacket;

	/* Fill frame ctrl */
	prTxFrame->u2FrameCtrl = MAC_FRAME_ACTION;

	COPY_MAC_ADDR(prTxFrame->aucDestAddr, aucBMC);
	COPY_MAC_ADDR(prTxFrame->aucSrcAddr, prBssInfo->aucOwnMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucBSSID, prBssInfo->aucBSSID);

	/* 3 Compose the frame body's frame */
	prTxFrame->ucCategory = CATEGORY_SPEC_MGT;
	prTxFrame->ucAction = ACTION_CHNL_SWITCH;

	prTxFrame->aucInfoElem[0] = ELEM_ID_CH_SW_ANNOUNCEMENT;
	prTxFrame->aucInfoElem[1] = 3;
	prTxFrame->aucInfoElem[2]
		= prAdapter->rWifiVar.ucChannelSwitchMode;
	prTxFrame->aucInfoElem[3]
		= prAdapter->rWifiVar.ucNewChannelNumber;
	prTxFrame->aucInfoElem[4]
		= prAdapter->rWifiVar.ucChannelSwitchCount;

	pfTxDoneHandler = rlmSendChannelSwitchTxDone;

	/* 4 Update information of MSDU_INFO_T */
	TX_SET_MMPDU(prAdapter, prMsduInfo, prBssInfo->ucBssIndex,
		     STA_REC_INDEX_BMCAST, WLAN_MAC_MGMT_HEADER_LEN,
		     sizeof(struct ACTION_CHANNEL_SWITCH_FRAME),
		     pfTxDoneHandler,
		     MSDU_RATE_MODE_AUTO);

	/* 4 Enqueue the frame to send this action frame. */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);
}

