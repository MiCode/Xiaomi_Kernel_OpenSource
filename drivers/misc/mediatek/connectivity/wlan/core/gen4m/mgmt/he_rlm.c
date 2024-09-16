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
** Id: @(#) he_rlm.c@@
*/

/*! \file   "he_rlm.c"
*    \brief This file contains HE Phy processing routines.
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

#if (CFG_SUPPORT_802_11AX == 1)
#include "he_rlm.h"

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/*
 * Ignore PPE Threshold (Optional) currently
 * PPE threshold should be filled based on HW RX capability
 */
#define CFG_RX_PPE_THRESHOLD              1

/*
 * NSS: 3 bits
 * RU Index Bitmask: 4 bits
 * PPET8 & PPET16 info for 2NSS and 4 RU index size: 6 * 2 * 4 = 48 bits
 * PPE pad: 1 bit
 * Total = 56 bits = 7 bytes
 */
#define MAX_PPE_THRESHOLD_LEN_2NSS  7
#define MAX_PPE_THRESHOLD_LEN_1NSS  4

#if (CFG_SUPPORT_HE_ER == 1)
#define MAX_SUPPORT_CONSTELLATION_DCM_QPSK 2
#endif

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/
/* TBD: where/what is the proper place/way to store this config */
uint8_t g_au8RlmHeCfgContellIdx[4][4][2] = {
	/* NSS1 */
	{{CONSTELL_IDX_BPSK, CONSTELL_IDX_NONE},	/* RU idx 0 */
	 {CONSTELL_IDX_BPSK, CONSTELL_IDX_NONE},	/* RU idx 1 */
	 {CONSTELL_IDX_BPSK, CONSTELL_IDX_NONE},	/* RU idx 2 */
	 {CONSTELL_IDX_BPSK, CONSTELL_IDX_NONE} },	/* RU idx 3 */
	/* NSS2 */
	{{CONSTELL_IDX_BPSK, CONSTELL_IDX_NONE},	/* RU idx 0 */
	 {CONSTELL_IDX_BPSK, CONSTELL_IDX_NONE},	/* RU idx 1 */
	 {CONSTELL_IDX_BPSK, CONSTELL_IDX_NONE},	/* RU idx 2 */
	 {CONSTELL_IDX_BPSK, CONSTELL_IDX_NONE} },	/* RU idx 3 */
	/* NSS3 */
	{{CONSTELL_IDX_BPSK, CONSTELL_IDX_NONE},	/* RU idx 0 */
	 {CONSTELL_IDX_BPSK, CONSTELL_IDX_NONE},	/* RU idx 1 */
	 {CONSTELL_IDX_BPSK, CONSTELL_IDX_NONE},	/* RU idx 2 */
	 {CONSTELL_IDX_BPSK, CONSTELL_IDX_NONE} },	/* RU idx 3 */
	/* NSS4 */
	{{CONSTELL_IDX_BPSK, CONSTELL_IDX_NONE},	/* RU idx 0 */
	 {CONSTELL_IDX_BPSK, CONSTELL_IDX_NONE},	/* RU idx 1 */
	 {CONSTELL_IDX_BPSK, CONSTELL_IDX_NONE},	/* RU idx 2 */
	 {CONSTELL_IDX_BPSK, CONSTELL_IDX_NONE} }	/* RU idx 3 */
};

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/
#define PPET_SUBFIELD_MASK 0x7
#define BIT64(n)                ((u_int64_t) 1UL << (n))
#define BITS64(m, n)            (~(BIT64(m)-1) & ((BIT64(n) - 1) | BIT64(n)))

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
uint8_t _heGetBssBandBw(struct ADAPTER *prAdapter,
	struct BSS_INFO *prBssInfo,
	enum ENUM_BAND eBand)
{
	uint8_t ucMaxBandwidth = MAX_BW_20MHZ;

	if (IS_BSS_AIS(prBssInfo)) {
		if (eBand == BAND_2G4)
			ucMaxBandwidth = prAdapter->rWifiVar.ucSta2gBandwidth;
		else
			ucMaxBandwidth = prAdapter->rWifiVar.ucSta5gBandwidth;

		if (ucMaxBandwidth > prAdapter->rWifiVar.ucStaBandwidth)
			ucMaxBandwidth = prAdapter->rWifiVar.ucStaBandwidth;
	} else if (IS_BSS_P2P(prBssInfo)) {
		/* AP mode */
		if (p2pFuncIsAPMode(
				prAdapter->rWifiVar.prP2PConnSettings[
					prBssInfo->u4PrivateData])) {
			if (prBssInfo->eBand == BAND_2G4)
				ucMaxBandwidth = prAdapter->rWifiVar
					.ucAp2gBandwidth;
			else
				ucMaxBandwidth = prAdapter->rWifiVar
					.ucAp5gBandwidth;

			if (ucMaxBandwidth
				> prAdapter->rWifiVar.ucApBandwidth)
				ucMaxBandwidth = prAdapter->rWifiVar
					.ucApBandwidth;
		}
		/* P2P mode */
		else {
			if (prBssInfo->eBand == BAND_2G4)
				ucMaxBandwidth = prAdapter->rWifiVar
					.ucP2p2gBandwidth;
			else
				ucMaxBandwidth = prAdapter->rWifiVar
					.ucP2p5gBandwidth;
		}
	}

	return ucMaxBandwidth;
}

uint32_t heRlmCalculateHeCapIELen(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	struct STA_RECORD *prStaRec)
{
	uint8_t ucMaxBw;
	struct BSS_INFO *prBssInfo = (struct BSS_INFO *) NULL;
	uint32_t u4OverallLen = OFFSET_OF(struct _IE_HE_CAP_T, aucVarInfo[0]);

	if (fgEfuseCtrlAxOn == 1) {

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);
	ucMaxBw = cnmGetBssMaxBw(prAdapter, prBssInfo->ucBssIndex);

	u4OverallLen += 4;

	if (ucMaxBw >= MAX_BW_160MHZ)
		u4OverallLen += 4;

	if (ucMaxBw >= MAX_BW_80_80_MHZ)
		u4OverallLen += 4;

#if (CFG_RX_PPE_THRESHOLD == 1)
	u4OverallLen += sizeof(struct _PPE_THRESHOLD_FIELD);
#endif
	} else {
		u4OverallLen = 0;
	}

	return u4OverallLen;
}

uint32_t heRlmCalculateHeOpIELen(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	struct STA_RECORD *prStaRec)
{
	uint32_t u4OverallLen = OFFSET_OF(struct _IE_HE_OP_T, aucVarInfo[0]);

	return u4OverallLen;
}

static void heRlmFillMCSMap(
	struct ADAPTER *prAdapter,
	struct BSS_INFO *prBssInfo,
	struct _HE_SUPPORTED_MCS_FIELD *prHeSupportedMcsSet)
{
	uint8_t i, ucSupportedNss;

	kalMemZero((void *) prHeSupportedMcsSet,
		sizeof(struct _HE_SUPPORTED_MCS_FIELD));
	ucSupportedNss = wlanGetSupportNss(prAdapter, prBssInfo->ucBssIndex);

	for (i = 0; i < 8; i++) {
		uint8_t ucOffset = i * 2;
		uint8_t ucMcsMap;

		if (i < ucSupportedNss) {
			if (prAdapter->fgMcsMapBeenSet)
				ucMcsMap = prAdapter->ucMcsMapSetFromSigma;
			else
				ucMcsMap = HE_CAP_INFO_MCS_MAP_MCS11;
		} else {
			ucMcsMap = HE_CAP_INFO_MCS_NOT_SUPPORTED;
		}

		prHeSupportedMcsSet->u2RxMcsMap |= (ucMcsMap << ucOffset);
		prHeSupportedMcsSet->u2TxMcsMap |= (ucMcsMap << ucOffset);
	}

	prHeSupportedMcsSet->u2RxMcsMap =
		CPU_TO_LE32(prHeSupportedMcsSet->u2RxMcsMap);
	prHeSupportedMcsSet->u2TxMcsMap =
		CPU_TO_LE32(prHeSupportedMcsSet->u2TxMcsMap);
}

/*
 * ucBitPos: bit position (starting from 0) for setting 3-bit PPE value
 * ucVal has to be 3-bit value (i.e. no greater than 7)
 */
static void heRlmSet3BitPPE(uint8_t *pMem, uint8_t ucBitPos, uint8_t ucVal)
{
	uint8_t ucValidBits, ucPosInCurByte;
	uint8_t *pucCurByte = pMem + (ucBitPos >> 3);

	ucPosInCurByte = (ucBitPos & 0x7);
	ucValidBits = 8 - ucPosInCurByte;
	if (ucValidBits >= 3) {
		(*pucCurByte) |= (ucVal  << ucPosInCurByte);
	} else {
		(*pucCurByte) |= (((uint8_t)(ucVal << ucPosInCurByte)) &
			BITS(ucPosInCurByte, 7));
		(*(pucCurByte + 1)) |= (ucVal >> ucValidBits);
	}
}

/* ucBitPos: bit position (starting from 0) for getting 3-bit PPE value */
static uint8_t heRlmGet3BitPPE(uint8_t *pMem, uint8_t ucBitPos)
{
	/* ucValidBits: valid bit counts in current byte */
	uint8_t ucValidBits, ucPosInCurByte, ucPPE;
	uint8_t *pucCurByte;

	/* Get the pointer of the byte that contains the ucBitPos */
	pucCurByte = pMem + (ucBitPos >> 3);  /* divided by 8 */
	ucPosInCurByte = (ucBitPos & 0x7);
	/* get the remaining bits in this byte */
	ucValidBits = 8 - ucPosInCurByte;
	if (ucValidBits >= 3) {	/* enough bits for getting 3-bit PPE */
		ucPPE = ((*pucCurByte) &
			BITS(ucPosInCurByte, ucPosInCurByte + 2)) >>
			ucPosInCurByte;
	} else {
		uint8_t ucLSb, ucMSb;

		ucLSb = ((*pucCurByte) & BITS(ucPosInCurByte, 7)) >>
			ucPosInCurByte;
		ucMSb = (*(pucCurByte + 1)) & BITS(0, 2 - ucValidBits);
		ucPPE = ucLSb | (ucMSb << ucValidBits);
	}

	return ucPPE;
}

/* Note: ucSupportedNss is (actual NSS - 1) */
static uint8_t heRlmFillPPEThresholdInfo(
	uint8_t ucSupportedNss,
	uint8_t ucRUIdxSize,
	uint8_t *pMem
)
{
	uint8_t ucNssIdx, ucRUIdx;
	uint8_t ucT16Pos = HE_CAP_PPE_PPET16_NSS1_RU0_SHFT;
	uint8_t ucT8Pos = HE_CAP_PPE_PPET8_NSS1_RU0_SHFT;
	uint8_t ucMaxSupportNss = (sizeof(g_au8RlmHeCfgContellIdx) /
				sizeof(g_au8RlmHeCfgContellIdx[0]));

	/* Coverity:
	 * due to ucSupportedNss is Supported Nss - 1
	 * if it > first dimention of g_au8RlmHeCfgContellIdx - 1
	 * assign it to first dimention of g_au8RlmHeCfgContellIdx - 1
	 */
	ucSupportedNss = (ucSupportedNss > (ucMaxSupportNss - 1)) ?
				(ucMaxSupportNss - 1) : ucSupportedNss;

	for (ucNssIdx = 0; ucNssIdx <= ucSupportedNss; ucNssIdx++) {
		for (ucRUIdx = 0; ucRUIdx < ucRUIdxSize; ucRUIdx++) {
			heRlmSet3BitPPE(pMem, ucT16Pos,
				g_au8RlmHeCfgContellIdx[ucNssIdx][ucRUIdx][0]);
			heRlmSet3BitPPE(pMem, ucT8Pos,
				g_au8RlmHeCfgContellIdx[ucNssIdx][ucRUIdx][1]);
			ucT16Pos += PPE_SUBFIELD_BITS_NUM;
			ucT8Pos += PPE_SUBFIELD_BITS_NUM;
		}
	}

	return ucT16Pos;
}

static uint8_t heRlmFillPPEThreshold(
	struct ADAPTER *prAdapter,
	struct BSS_INFO *prBssInfo,
	uint8_t *pPPEThreshold)
{
	uint8_t ucRUIdxSize = 0, ucLen;
	uint8_t ucSupportedNss =
		wlanGetSupportNss(prAdapter, prBssInfo->ucBssIndex) - 1;
	uint8_t ucMaxBw =
		cnmGetBssMaxBw(prAdapter, prBssInfo->ucBssIndex);

	kalMemZero((void *) pPPEThreshold, sizeof(struct _PPE_THRESHOLD_FIELD));

	/* Note: this field in spec is (actual NSS - 1) */
	(*pPPEThreshold) = ucSupportedNss;

	switch (ucMaxBw) {
	case MAX_BW_20MHZ:
		(*pPPEThreshold) |= HE_CAP_PPE_242_RU_IDX;
		ucRUIdxSize = 1;
		break;
	case MAX_BW_40MHZ:
		(*pPPEThreshold) |= HE_CAP_PPE_484_RU_IDX;
		ucRUIdxSize = 2;
		break;
	case MAX_BW_80MHZ:
		(*pPPEThreshold) |= HE_CAP_PPE_996_RU_IDX;
		ucRUIdxSize = 3;
		break;
	case MAX_BW_160MHZ:
	case MAX_BW_80_80_MHZ:
		(*pPPEThreshold) |= HE_CAP_PPE_996X2_RU_IDX;
		ucRUIdxSize = 4;
		break;
	default:
		/* should never come here */
		ASSERT(0);
	}

	ucLen = heRlmFillPPEThresholdInfo(ucSupportedNss,
		ucRUIdxSize, pPPEThreshold);

	ucLen = (ucLen % 8) ? (ucLen / 8 + 1) : (ucLen / 8);

	return ucLen;
}

static void heRlmFillHeCapIE(
	struct ADAPTER *prAdapter,
	struct BSS_INFO *prBssInfo,
	struct MSDU_INFO *prMsduInfo)
{
	struct _IE_HE_CAP_T *prHeCap;
	struct _HE_SUPPORTED_MCS_FIELD *prHeSupportedMcsSet;
	uint32_t u4OverallLen = OFFSET_OF(struct _IE_HE_CAP_T, aucVarInfo[0]);
	uint16_t ucMaxBw;
	u_int8_t fgBfEn = TRUE;

	struct WIFI_VAR *prWifiVar = &prAdapter->rWifiVar;
#if (CFG_RX_PPE_THRESHOLD == 1)
	uint8_t *pPPEThreshold;
#endif
	uint8_t ucSupportedNss =
		wlanGetSupportNss(prAdapter, prBssInfo->ucBssIndex) - 1;
	struct AIS_FSM_INFO *prAisFsmInfo =
		aisGetAisFsmInfo(prAdapter, prBssInfo->ucBssIndex);
	struct BSS_DESC *prBssDesc;

	ASSERT(prAdapter);
	ASSERT(prBssInfo);
	ASSERT(prMsduInfo);

	prHeCap = (struct _IE_HE_CAP_T *)
		(((uint8_t *)prMsduInfo->prPacket)+prMsduInfo->u2FrameLength);

	prHeCap->ucId = ELEM_ID_RESERVED;
	prHeCap->ucExtId = ELEM_EXT_ID_HE_CAP;

	ucMaxBw = cnmGetBssMaxBw(prAdapter, prBssInfo->ucBssIndex);

	/* MAC capabilities */
	HE_RESET_MAC_CAP(prHeCap->ucHeMacCap);

	HE_SET_MAC_CAP_TRIGGER_PAD_DURATION(prHeCap->ucHeMacCap,
		prWifiVar->ucTrigMacPadDur);

	HE_SET_MAC_CAP_HTC_HE(prHeCap->ucHeMacCap);
	HE_SET_MAC_CAP_OM_CTRL(prHeCap->ucHeMacCap);

#if (CFG_SUPPORT_TWT == 1)
	if (IS_BSS_AIS(prBssInfo) &&
		IS_FEATURE_ENABLED(prWifiVar->ucTWTRequester))
		HE_SET_MAC_CAP_TWT_REQ(prHeCap->ucHeMacCap);
#endif

	/* PHY capabilities */
	HE_RESET_PHY_CAP(prHeCap->ucHePhyCap);

	if (_heGetBssBandBw(prAdapter, prBssInfo, BAND_2G4)
		>= MAX_BW_40MHZ)
		HE_SET_PHY_CAP_CHAN_WIDTH_SET_BW40_2G(prHeCap->ucHePhyCap);

	if (_heGetBssBandBw(prAdapter, prBssInfo, BAND_5G)
		>= MAX_BW_40MHZ)
		HE_SET_PHY_CAP_CHAN_WIDTH_SET_BW40_BW80_5G(
			prHeCap->ucHePhyCap);

	if (_heGetBssBandBw(prAdapter, prBssInfo, BAND_5G)
		>= MAX_BW_160MHZ)
		HE_SET_PHY_CAP_CHAN_WIDTH_SET_BW160_5G(prHeCap->ucHePhyCap);

	if (_heGetBssBandBw(prAdapter, prBssInfo, BAND_5G)
		>= MAX_BW_80_80_MHZ)
		HE_SET_PHY_CAP_CHAN_WIDTH_SET_BW80P80_5G(prHeCap->ucHePhyCap);

	if (IS_FEATURE_ENABLED(prWifiVar->ucRxLdpc) &&
		IS_FEATURE_ENABLED(prWifiVar->ucTxLdpc))
		HE_SET_PHY_CAP_LDPC_CODING_IN_PAYLOAD(prHeCap->ucHePhyCap);

	if (IS_FEATURE_ENABLED(prWifiVar->ucTxStbc)) {
		HE_SET_PHY_CAP_STBC_TX_LT_OR_EQ_80M(prHeCap->ucHePhyCap);
		if (IS_BSS_AIS(prBssInfo))
			HE_SET_PHY_CAP_STBC_TX_GT_80M(prHeCap->ucHePhyCap);
	}

	if (IS_FEATURE_ENABLED(prWifiVar->ucRxStbc)) {
		HE_SET_PHY_CAP_STBC_RX_LT_OR_EQ_80M(prHeCap->ucHePhyCap);
		if (IS_BSS_AIS(prBssInfo))
			HE_SET_PHY_CAP_STBC_RX_GT_80M(prHeCap->ucHePhyCap);
	}

#if CFG_SUPPORT_BFEE
#if CFG_SUPPORT_CONDITIONAL_BFEE
	if ((prAdapter->rWifiVar.u4SwTestMode != ENUM_SW_TEST_MODE_SIGMA_AX) &&
		(IS_BSS_AIS(prBssInfo) && prAisFsmInfo != NULL)) {
		prBssDesc = prAisFsmInfo->prTargetBssDesc;
		if (prBssDesc != NULL && (bssGetRxNss(prAdapter, prBssDesc) ==
			wlanGetSupportNss(prAdapter, prBssInfo->ucBssIndex))) {
			fgBfEn = FALSE;
			DBGLOG(SW4, ERROR,
				"Disable Bfee due to same Nss between STA and AP\n");
		} else {
			fgBfEn = TRUE;
		}
	}
#endif
	if ((fgBfEn == TRUE) && (IS_FEATURE_ENABLED(prWifiVar->ucStaHeBfee))) {
		HE_SET_PHY_CAP_SU_BFMEE(prHeCap->ucHePhyCap);
		HE_SET_PHY_CAP_BFMEE_STS_LT_OR_EQ_80M(prHeCap->ucHePhyCap, 3);
		HE_SET_PHY_CAP_CODE_BOOK_4_2_SU_FB(prHeCap->ucHePhyCap);
		HE_SET_PHY_CAP_CODE_BOOK_7_5_MU_FB(prHeCap->ucHePhyCap);
		HE_SET_PHY_CAP_TRIG_SU_BF_FB(prHeCap->ucHePhyCap);
		HE_SET_PHY_CAP_TRIG_MU_BF_PARTIAL_BW_FB(prHeCap->ucHePhyCap);
		HE_SET_PHY_CAP_MAX_NC(prHeCap->ucHePhyCap, ucSupportedNss);
	}
#endif

#if (CFG_SUPPORT_HE_ER == 1)
	if (IS_FEATURE_ENABLED(prWifiVar->u4ExtendedRange)) {
		HE_SET_PHY_CAP_DCM_MAX_CONSTELLATION_TX(prHeCap->ucHePhyCap,
			MAX_SUPPORT_CONSTELLATION_DCM_QPSK);
		HE_SET_PHY_CAP_DCM_MAX_CONSTELLATION_RX(prHeCap->ucHePhyCap,
			MAX_SUPPORT_CONSTELLATION_DCM_QPSK);
		HE_SET_PHY_CAP_PARTIAL_BW_EXTENDED_RANGE(prHeCap->ucHePhyCap);
		HE_SET_PHY_CAP_ER_SU_4X_HE_LTF(prHeCap->ucHePhyCap);
		HE_SET_PHY_CAP_ER_SU_PPDU_1X_HE_LTF(prHeCap->ucHePhyCap);

		DBGLOG(RLM, INFO, "ER: Set ER Phy capabilities\n");
	}
#endif

	/* Set MCS map */
	prHeSupportedMcsSet = (struct _HE_SUPPORTED_MCS_FIELD *)
		(((uint8_t *) prHeCap) + u4OverallLen);
	heRlmFillMCSMap(prAdapter, prBssInfo, prHeSupportedMcsSet);
	u4OverallLen += sizeof(struct _HE_SUPPORTED_MCS_FIELD);

	if (ucMaxBw >= MAX_BW_160MHZ) {
		prHeSupportedMcsSet = (struct _HE_SUPPORTED_MCS_FIELD *)
			(((uint8_t *) prHeCap) + u4OverallLen);
		heRlmFillMCSMap(prAdapter, prBssInfo, prHeSupportedMcsSet);
		u4OverallLen += sizeof(struct _HE_SUPPORTED_MCS_FIELD);
	}

	if (ucMaxBw >= MAX_BW_80_80_MHZ) {
		prHeSupportedMcsSet = (struct _HE_SUPPORTED_MCS_FIELD *)
			(((uint8_t *) prHeCap) + u4OverallLen);
		heRlmFillMCSMap(prAdapter, prBssInfo, prHeSupportedMcsSet);
		u4OverallLen += sizeof(struct _HE_SUPPORTED_MCS_FIELD);
	}

#if (CFG_RX_PPE_THRESHOLD == 1)
	pPPEThreshold = ((uint8_t *) prHeCap) + u4OverallLen;
	u4OverallLen +=
		heRlmFillPPEThreshold(prAdapter, prBssInfo, pPPEThreshold);
#endif
	/* The Element ID Extension byte is included in length calculation */
	prHeCap->ucLength = u4OverallLen - ELEM_HDR_LEN;

	prMsduInfo->u2FrameLength += IE_SIZE(prHeCap);
}

void heRlmReqGenerateHeCapIE(
	struct ADAPTER *prAdapter,
	struct MSDU_INFO *prMsduInfo)
{
	struct BSS_INFO *prBssInfo;
	struct STA_RECORD *prStaRec;

	if (fgEfuseCtrlAxOn == 1) {

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

	prBssInfo = prAdapter->aprBssInfo[prMsduInfo->ucBssIndex];
	if (!prBssInfo)
		return;

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

	if ((prAdapter->rWifiVar.ucAvailablePhyTypeSet & PHY_TYPE_SET_802_11AX)
	    && (!prStaRec || (prStaRec->ucPhyTypeSet & PHY_TYPE_SET_802_11AX)))
		heRlmFillHeCapIE(prAdapter, prBssInfo, prMsduInfo);
	}
}

void heRlmRspGenerateHeCapIE(
	struct ADAPTER *prAdapter,
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

	if (RLM_NET_IS_11AX(prBssInfo) &&
	    (ucPhyTypeSet & PHY_TYPE_SET_802_11AX))
		heRlmFillHeCapIE(prAdapter, prBssInfo, prMsduInfo);
}

static void heRlmFillHeOpIE(
	struct ADAPTER *prAdapter,
	struct BSS_INFO *prBssInfo,
	struct MSDU_INFO *prMsduInfo)
{
	struct _IE_HE_OP_T *prHeOp;
	uint32_t u4OverallLen = OFFSET_OF(struct _IE_HE_OP_T, aucVarInfo[0]);

	ASSERT(prAdapter);
	ASSERT(prBssInfo);
	ASSERT(prMsduInfo);

	prHeOp = (struct _IE_HE_OP_T *)
		(((uint8_t *)prMsduInfo->prPacket)+prMsduInfo->u2FrameLength);

	prHeOp->ucId = ELEM_ID_RESERVED;
	prHeOp->ucExtId = ELEM_EXT_ID_HE_OP;

	memcpy(prHeOp->ucHeOpParams, prBssInfo->ucHeOpParams, HE_OP_BYTE_NUM);
	prHeOp->ucBssColorInfo = prBssInfo->ucBssColorInfo;
	prHeOp->u2HeBasicMcsSet = CPU_TO_LE16(prBssInfo->u2HeBasicMcsSet);

	prHeOp->ucLength = u4OverallLen - ELEM_HDR_LEN;
	prMsduInfo->u2FrameLength += IE_SIZE(prHeOp);
}

void heRlmRspGenerateHeOpIE(
	struct ADAPTER *prAdapter,
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

	if (RLM_NET_IS_11AX(prBssInfo) &&
	    (ucPhyTypeSet & PHY_TYPE_SET_802_11AX))
		heRlmFillHeOpIE(prAdapter, prBssInfo, prMsduInfo);
}

static uint16_t heRlmGetHeMcsMap(uint8_t *pSrc)
{
	uint16_t u2McsMap;

	kalMemCopy(&u2McsMap, pSrc, sizeof(u2McsMap));
	u2McsMap = LE16_TO_CPU(u2McsMap);

	return u2McsMap;
}

static uint32_t heRlmRecHeMcsMap(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec,
	struct _IE_HE_CAP_T *prHeCap)
{
	uint32_t u4McsMapOffset;
	uint16_t u2McsMap;
	struct BSS_INFO *prBssInfo;
	uint8_t ucHeCapMcsOwnNotSupportOffset = 0, ucMaxBw;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);
	ucMaxBw = cnmGetBssMaxBw(prAdapter, prBssInfo->ucBssIndex);

	/* BW 80Mhz */
	u4McsMapOffset = OFFSET_OF(struct _IE_HE_CAP_T, aucVarInfo[0]);

	u2McsMap = heRlmGetHeMcsMap(((uint8_t *)prHeCap) + u4McsMapOffset);
	prStaRec->u2HeRxMcsMapBW80 = u2McsMap;

	if (wlanGetSupportNss(prAdapter, prStaRec->ucBssIndex) < 8) {
		ucHeCapMcsOwnNotSupportOffset =
			wlanGetSupportNss(prAdapter, prStaRec->ucBssIndex) * 2;
		/* Mark Rx Mcs Map which we don't support */
		prStaRec->u2HeRxMcsMapBW80 |=
			BITS(ucHeCapMcsOwnNotSupportOffset, 15);
	}
	if (prStaRec->u2HeRxMcsMapBW80 != u2McsMap)
		DBGLOG(RLM, TRACE, "Change HeRxMcsMapBW80 from 0x%x to 0x%x\n",
			u2McsMap, prStaRec->u2HeRxMcsMapBW80);

	u4McsMapOffset += sizeof(uint16_t);

	u2McsMap = heRlmGetHeMcsMap(((uint8_t *)prHeCap) + u4McsMapOffset);
	prStaRec->u2HeTxMcsMapBW80 = u2McsMap;

	u4McsMapOffset += sizeof(uint16_t);

	/* BW 160Mhz */
	if (HE_IS_PHY_CAP_CHAN_WIDTH_SET_BW160_5G(prStaRec->ucHePhyCapInfo)) {
		u2McsMap = heRlmGetHeMcsMap(((uint8_t *)prHeCap) +
			u4McsMapOffset);
		prStaRec->u2HeRxMcsMapBW160 = u2McsMap;

		if (ucMaxBw >= MAX_BW_160MHZ) {
			/* Mark Rx Mcs Map which we don't support */
			prStaRec->u2HeRxMcsMapBW160 |=
				BITS(ucHeCapMcsOwnNotSupportOffset, 15);

			if (prStaRec->u2HeRxMcsMapBW160 != u2McsMap)
				DBGLOG(RLM, TRACE,
				    "Change McsMapBW160 from 0x%x to 0x%x\n",
				    u2McsMap, prStaRec->u2HeRxMcsMapBW160);
		} else {
			/* BW160 is not supported locally */
			prStaRec->u2HeRxMcsMapBW160 = BITS(0, 15);
		}

		u4McsMapOffset += sizeof(uint16_t);
		u2McsMap = heRlmGetHeMcsMap(((uint8_t *)prHeCap) +
			u4McsMapOffset);
		prStaRec->u2HeTxMcsMapBW160 = u2McsMap;
		u4McsMapOffset += sizeof(uint16_t);
	} else {
		prStaRec->u2HeRxMcsMapBW160 = BITS(0, 15);
		prStaRec->u2HeTxMcsMapBW160 = BITS(0, 15);
	}

	/* BW 80+80 */
	if (HE_IS_PHY_CAP_CHAN_WIDTH_SET_BW80P80_5G(prStaRec->ucHePhyCapInfo)) {
		u2McsMap = heRlmGetHeMcsMap(((uint8_t *)prHeCap) +
			u4McsMapOffset);
		prStaRec->u2HeRxMcsMapBW80P80 = u2McsMap;

		if (ucMaxBw >= MAX_BW_80_80_MHZ) {
			/* Mark Rx Mcs Map which we don't support */
			prStaRec->u2HeRxMcsMapBW80P80 |=
				BITS(ucHeCapMcsOwnNotSupportOffset, 15);

			if (prStaRec->u2HeRxMcsMapBW80P80 != u2McsMap)
				DBGLOG(RLM, TRACE,
				    "Change McsMapBW80P80 from 0x%x to 0x%x\n",
				    u2McsMap, prStaRec->u2HeRxMcsMapBW80P80);
		} else {
			/* BW 80+80 is not supported locally */
			prStaRec->u2HeRxMcsMapBW80P80 = BITS(0, 15);
		}

		u4McsMapOffset += sizeof(uint16_t);
		u2McsMap = heRlmGetHeMcsMap(((uint8_t *)prHeCap) +
			u4McsMapOffset);
		prStaRec->u2HeTxMcsMapBW80P80 = u2McsMap;
		u4McsMapOffset += sizeof(uint16_t);
	} else {
		prStaRec->u2HeRxMcsMapBW80P80 = BITS(0, 15);
		prStaRec->u2HeTxMcsMapBW80P80 = BITS(0, 15);
	}

	return u4McsMapOffset;
}

static void heRlmRecHePPEThresholds(struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec,
	struct _IE_HE_CAP_T *prHeCap,
	uint32_t u4Offset)
{
	/* To store subfiled values in peer's PPE threshold */
	uint8_t i, j, k, ucNSS, ucRUIdxBMP, ucRUIdxBMPTmp, ucPPEInfo[32], ucPos;
	uint8_t *pucMem = ((uint8_t *)prHeCap) + u4Offset;

	ucNSS = (*pucMem) & HE_CAP_PPE_NSS;
	ucRUIdxBMP = ((*pucMem) & HE_CAP_PPE_RU_IDX_BMP) >>
		HE_CAP_PPE_RU_IDX_BMP_SHFT;

	k = 0;
	ucPos = HE_CAP_PPE_PPET16_NSS1_RU0_SHFT;
	for (i = 0; i <= ucNSS; i++) {
		ucRUIdxBMPTmp = ucRUIdxBMP;
		for (j = 0; j < PPE_RU_IDX_SIZE * 2; j++) {
			if (ucRUIdxBMPTmp & 0x1) {
				ucPPEInfo[k] = heRlmGet3BitPPE(pucMem, ucPos);
				ucPos += 3;
			} else {
				ucPPEInfo[k] = 0;
			}
			k++;
			ucRUIdxBMPTmp = (ucRUIdxBMPTmp >> 1);
		}
	}
}

void heRlmRecHeCapInfo(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec,
	uint8_t *pucIE)
{
	uint32_t u4Offset;
	struct WIFI_VAR *prWifiVar = &prAdapter->rWifiVar;
	struct _IE_HE_CAP_T *prHeCap = (struct _IE_HE_CAP_T *) pucIE;

	/* if payload not contain any aucVarInfo,
	 * IE size = sizeof(struct _IE_HE_CAP_T)
	 */
	if (IE_SIZE(prHeCap) < (sizeof(struct _IE_HE_CAP_T))) {
		DBGLOG(SCN, WARN, "HE_CAP IE_LEN err(%d)!\n", IE_LEN(prHeCap));
		return;
	}

	memcpy(prStaRec->ucHeMacCapInfo, prHeCap->ucHeMacCap,
		HE_MAC_CAP_BYTE_NUM);
	memcpy(prStaRec->ucHePhyCapInfo, prHeCap->ucHePhyCap,
		HE_PHY_CAP_BYTE_NUM);

#if (CFG_SUPPORT_HE_ER == 1)
	DBGLOG(RLM, INFO, "ER: TX:%d, RX:%d, bw:%d, 4x LTF:%d, 1X LTF:%d\n",
		HE_GET_PHY_CAP_DCM_MAX_CONSTELLATION_TX(
			prStaRec->ucHePhyCapInfo),
		HE_GET_PHY_CAP_DCM_MAX_CONSTELLATION_RX(
			prStaRec->ucHePhyCapInfo),
		HE_GET_PHY_CAP_PARTIAL_BW_EXTENDED_RANGE(
			prStaRec->ucHePhyCapInfo),
		HE_GET_PHY_CAP_ER_SU_4X_HE_LTF(
			prStaRec->ucHePhyCapInfo),
		HE_GET_PHY_CAP_ER_SU_PPDU_1X_HE_LTF(
			prStaRec->ucHePhyCapInfo));
#endif

	/* Disable peer bfer cap indication to FW
	 * if our bfee feature is not on
	 */
	if (IS_FEATURE_DISABLED(prWifiVar->ucStaHeBfee)) {
		HE_UNSET_PHY_CAP_SU_BFMER(prStaRec->ucHePhyCapInfo);
		HE_UNSET_PHY_CAP_MU_BFMER(prStaRec->ucHePhyCapInfo);
	}

	/* Set HE Rx Mcs Map upon peer's capability and our capability */
	u4Offset = heRlmRecHeMcsMap(prAdapter, prStaRec, prHeCap);

	/* Set HE PPE Thresholds if it exists */
	if (HE_IS_PHY_CAP_PPE_THRESHOLD(prStaRec->ucHePhyCapInfo))
		heRlmRecHePPEThresholds(prAdapter, prStaRec, prHeCap, u4Offset);
}

void heRlmRecHeOperation(
	struct ADAPTER *prAdapter,
	struct BSS_INFO *prBssInfo,
	uint8_t *pucIE)
{
	struct _IE_HE_OP_T *prHeOp = (struct _IE_HE_OP_T *) pucIE;
#if (CFG_SUPPORT_HE_ER == 1)
	struct WIFI_VAR *prWifiVar = &prAdapter->rWifiVar;
	/* if payload not contain any aucVarInfo,
	 * IE size = sizeof(struct _IE_HE_OP_T)
	 */
	if (IE_SIZE(prHeOp) < (sizeof(struct _IE_HE_OP_T))) {
		DBGLOG(SCN, WARN, "HE_OP IE_LEN err(%d)!\n", IE_LEN(prHeOp));
		return;
	}

	if (IS_FEATURE_DISABLED(prWifiVar->u4ExtendedRange)) {
		HE_SET_OP_PARAM_ER_SU_DISABLE(prHeOp->ucHeOpParams);

		DBGLOG(RLM, INFO, "ER: is ER SU: %d\n",
			HE_IS_ER_SU_DISABLE(prHeOp->ucHeOpParams));
	}
#endif

	memcpy(prBssInfo->ucHeOpParams, prHeOp->ucHeOpParams, HE_OP_BYTE_NUM);
	prBssInfo->ucBssColorInfo = prHeOp->ucBssColorInfo;
	prBssInfo->u2HeBasicMcsSet = LE16_TO_CPU(prHeOp->u2HeBasicMcsSet);
}

uint8_t heRlmUpdateSRParams(
	struct BSS_INFO *prBssInfo,
	uint8_t *pucIE)
{
	struct _IE_SR_PARAM_T *prSRParam = (struct _IE_SR_PARAM_T *) pucIE;
	uint32_t u4IEOffset;
	uint8_t fgIsNew = FALSE;

	if (prBssInfo->ucSRControl != prSRParam->ucSRControl) {
		fgIsNew = TRUE;
		prBssInfo->ucSRControl = prSRParam->ucSRControl;
	}

	u4IEOffset = OFFSET_OF(struct _IE_SR_PARAM_T, aucVarInfo[0]);

	if (prBssInfo->ucSRControl & SR_PARAM_NON_SRG_OFFSET_PRESENT) {
		struct _NON_SRG_SR_INFO_T *prNonSRGInfo =
			(struct _NON_SRG_SR_INFO_T *)
			(((uint8_t *) pucIE) + u4IEOffset);

		if (prBssInfo->ucNonSRGObssPdMaxOffset !=
			prNonSRGInfo->ucObssPdMaxOffset) {

			fgIsNew = TRUE;
			prBssInfo->ucNonSRGObssPdMaxOffset =
				prNonSRGInfo->ucObssPdMaxOffset;
		}

		u4IEOffset += sizeof(struct _NON_SRG_SR_INFO_T);
	} else
		prBssInfo->ucNonSRGObssPdMaxOffset = 0;

	if (prBssInfo->ucSRControl & SR_PARAM_SRG_INFO_PRESENT) {
		struct _SRG_SR_INFO_T *prSRGInfo = (struct _SRG_SR_INFO_T *)
			(((uint8_t *) pucIE) + u4IEOffset);

		if (prBssInfo->ucSRGObssPdMinOffset !=
			prSRGInfo->ucObssPdMinOffset) {
			fgIsNew = TRUE;
			prBssInfo->ucSRGObssPdMinOffset =
				prSRGInfo->ucObssPdMinOffset;
		}
		if (prBssInfo->ucSRGObssPdMaxOffset !=
			prSRGInfo->ucObssPdMaxOffset) {
			fgIsNew = TRUE;
			prBssInfo->ucSRGObssPdMaxOffset =
				prSRGInfo->ucObssPdMaxOffset;
		}
		if (prBssInfo->u8SRGBSSColorBitmap !=
			LE64_TO_CPU(prSRGInfo->u8BSSColorBitmap)) {
			fgIsNew = TRUE;
			prBssInfo->u8SRGBSSColorBitmap =
				LE64_TO_CPU(prSRGInfo->u8BSSColorBitmap);
		}
		if (prBssInfo->u8SRGPartialBSSIDBitmap !=
			LE64_TO_CPU(prSRGInfo->u8PartialBSSIDBitmap)) {
			fgIsNew = TRUE;
			prBssInfo->u8SRGPartialBSSIDBitmap =
				LE64_TO_CPU(prSRGInfo->u8PartialBSSIDBitmap);
		}
	} else {
		prBssInfo->ucSRGObssPdMinOffset = 0;
		prBssInfo->ucSRGObssPdMaxOffset = 0;
		prBssInfo->u8SRGBSSColorBitmap = 0;
		prBssInfo->u8SRGPartialBSSIDBitmap = 0;
	}

	return fgIsNew;
}

uint8_t
heRlmRecHeSRParams(
	struct ADAPTER *prAdapter,
	struct BSS_INFO *prBssInfo,
	struct SW_RFB *prSwRfb,
	uint8_t *pucIE,
	uint16_t u2IELength)
{
	uint16_t u2Offset;
	uint8_t fgNewParameter = FALSE;

	DEBUGFUNC("rlmParseHeSRParams");

	if (!pucIE)
		return FALSE;

	/* Obtain the SR parameters */
	IE_FOR_EACH(pucIE, u2IELength, u2Offset) {
		switch (IE_ID(pucIE)) {
		case ELEM_ID_RESERVED:
			if (IE_ID_EXT(pucIE) == ELEM_EXT_ID_SR_PARAM) {
				fgNewParameter =
					heRlmUpdateSRParams(prBssInfo, pucIE);
			}
			break;
		default:
			break;
		}
	}

	return fgNewParameter;
}

void heRlmInitHeHtcACtrlOMAndUPH(
	struct ADAPTER *prAdapter)
{
	HE_SET_HTC_HE_VARIANT(prAdapter->u4HeHtcOM);
	HE_SET_HTC_1ST_A_CTRL_ID(prAdapter->u4HeHtcOM, HTC_HE_A_CTRL_OM);
	HE_SET_HTC_HE_OM_RX_NSS(prAdapter->u4HeHtcOM, 1);
	HE_SET_HTC_HE_OM_TX_NSTS(prAdapter->u4HeHtcOM, 1);
	HE_SET_HTC_HE_OM_CH_WIDTH(prAdapter->u4HeHtcOM, CH_BW_80);
	HE_SET_HTC_HE_OM_UL_MU_DISABLE(prAdapter->u4HeHtcOM, 0);
	HE_SET_HTC_HE_OM_UL_MU_DATA_DISABLE(prAdapter->u4HeHtcOM, 0);
	HE_SET_HTC_2ND_A_CTRL_ID(prAdapter->u4HeHtcOM, HTC_HE_A_CTRL_UPH);
	HE_SET_HTC_HE_UPH(prAdapter->u4HeHtcOM, 10);
	HE_SET_HTC_HE_UPH_MIN_TX_PWR_FLAG(prAdapter->u4HeHtcOM, 0);
}

void heRlmReqGenerateHeHtcACtrlOM(
	IN struct HE_A_CTRL_OM_T *prHeActrlOM,
	OUT uint32_t *pHtc)
{
	uint32_t u4HTC = 0;

	if (!pHtc || !prHeActrlOM) {
		DBGLOG(RLM, WARN,
		 "heRlmReqGenerateHeHtcACtrlOM:: Please check function parameters.\n");
		return;
	}

	HE_SET_HTC_1ST_A_CTRL_ID(u4HTC, HTC_HE_A_CTRL_OM);
	HE_SET_HTC_HE_OM_RX_NSS(u4HTC, prHeActrlOM->ucRxNss);
	HE_SET_HTC_HE_OM_TX_NSTS(u4HTC, prHeActrlOM->ucTxNsts);
	HE_SET_HTC_HE_OM_CH_WIDTH(u4HTC, prHeActrlOM->ucBW);
	HE_SET_HTC_HE_OM_UL_MU_DISABLE(u4HTC, prHeActrlOM->fgDisMuUL);
	HE_SET_HTC_HE_OM_UL_MU_DATA_DISABLE(u4HTC, prHeActrlOM->fgDisMuULData);
	*pHtc = u4HTC;
	DBGLOG(RLM, STATE, "heRlmReqGenerateHeHtcACtrlOM:: u4HTC = 0x%08x\n");
}

void heRlmParseHeHtcACtrlOM(
	uint32_t u4Htc,
	struct HE_A_CTRL_OM_T *prHeActrlOM)
{
	DBGLOG(RLM, STATE,
		"heRlmParseHeHtcACtrlOM:: HTC Control Field = 0x%08x\n",
		u4Htc);
	if (HE_GET_HTC_1ST_A_CTRL_ID(u4Htc) == HTC_HE_A_CTRL_OM) {
		prHeActrlOM->ucRxNss = HE_GET_HTC_HE_OM_RX_NSS(u4Htc);
		prHeActrlOM->ucTxNsts = HE_GET_HTC_HE_OM_TX_NSTS(u4Htc);
		prHeActrlOM->ucBW = HE_GET_HTC_HE_OM_CH_WIDTH(u4Htc);
		prHeActrlOM->fgDisMuUL = HE_GET_HTC_HE_OM_UL_MU_DISABLE(u4Htc);
		prHeActrlOM->fgDisMuULData =
			HE_GET_HTC_HE_OM_UL_MU_DATA_DISABLE(u4Htc);
	} else
		DBGLOG(RLM, WARN, "This is not Operating mode (OM).\n");
}


/*----------------------------------------------------------------------------*/
/*!
* @brief This function will compose the QoS Null + HTC Data frame.
*
* @param[in] prAdapter              Pointer to the Adapter structure.
* @param[in] pucBuffer              Pointer to the frame buffer.
* @param[in] prStaRec               Pointer to the STA_RECORD_T.
* @param[in] ucUP                   User Priority.
* @param[in] fgSetEOSP              Set the EOSP bit.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
void
heRlmComposeHtcNullFrame(
	IN struct ADAPTER *prAdapter,
	IN uint8_t *pucBuffer,
	IN struct STA_RECORD *prStaRec,
	IN uint8_t ucUP,
	IN u_int8_t fgSetEOSP)
{
	struct WLAN_MAC_HEADER_HT *prQoSNullFrame;
	struct BSS_INFO *prBssInfo;
	uint16_t u2FrameCtrl;
	uint16_t u2QosControl;
	uint8_t ucBssIndex;

	ASSERT(prStaRec);
	ucBssIndex = prStaRec->ucBssIndex;

	ASSERT(ucBssIndex <= prAdapter->ucHwBssIdNum);

	ASSERT(pucBuffer);

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);
	ASSERT(prBssInfo);

	prQoSNullFrame = (struct WLAN_MAC_HEADER_HT *) pucBuffer;

	/* 4 <1> Decide the Frame Control Field */
#if 0
	u2FrameCtrl = MAC_FRAME_QOS_DATA | MASK_FC_ORDER;
#else
	u2FrameCtrl = MAC_FRAME_QOS_NULL | MASK_FC_ORDER;
#endif

	if (IS_AP_STA(prStaRec)) {
		u2FrameCtrl |= MASK_FC_TO_DS;
	} else if (IS_CLIENT_STA(prStaRec)) {
		u2FrameCtrl |= MASK_FC_FROM_DS;
	} else if (IS_DLS_STA(prStaRec)) {
		/* TODO(Kevin) */
	} else {
		/* NOTE(Kevin): We won't send QoS Null frame for IBSS */
		ASSERT(0);
		return;
	}

	/* 4 <2> Compose the QoS Null frame */
	/* Fill the Frame Control field. */
	/* WLAN_SET_FIELD_16(&prQoSNullFrame->u2FrameCtrl, u2FrameCtrl); */
	prQoSNullFrame->u2FrameCtrl = u2FrameCtrl;

	/* Fill the Address 1 field with Target Peer Address. */
	COPY_MAC_ADDR(prQoSNullFrame->aucAddr1, prStaRec->aucMacAddr);

	/* Fill the Address 2 field with our MAC Address. */
	COPY_MAC_ADDR(prQoSNullFrame->aucAddr2, prBssInfo->aucOwnMacAddr);

	/* Fill the Address 3 field with Target BSSID. */
	COPY_MAC_ADDR(prQoSNullFrame->aucAddr3, prBssInfo->aucBSSID);

	/* Clear the SEQ/FRAG_NO field */
	/* (HW won't overide the FRAG_NO, so we need to clear it). */
	prQoSNullFrame->u2SeqCtrl = 0;

	u2QosControl = (uint16_t) (ucUP & WMM_QC_UP_MASK);

	if (fgSetEOSP)
		u2QosControl |= WMM_QC_EOSP;

	prQoSNullFrame->u2QosCtrl = u2QosControl;

#ifdef	CFG_AX_CERT_WKR
	HE_SET_HTC_2ND_A_CTRL_ID(prAdapter->u4HeHtcOM, HTC_HE_A_CTRL_UPH);
	HE_SET_HTC_HE_UPH(prAdapter->u4HeHtcOM, 10);
	HE_SET_HTC_HE_UPH_MIN_TX_PWR_FLAG(prAdapter->u4HeHtcOM, 0);
#endif

	prQoSNullFrame->u4HtCtrl = prAdapter->u4HeHtcOM;

	return;

}				/* end of bssComposeQoSNullFrameHeader() */

uint32_t heRlmSendHtcNullFrame(
	IN struct ADAPTER *prAdapter,
	IN struct STA_RECORD *prStaRec,
	IN uint8_t ucUP,
	IN PFN_TX_DONE_HANDLER pfTxDoneHandler)
{
	struct MSDU_INFO *prMsduInfo;
	uint16_t u2EstimatedFrameLen;

	/* 4 <1> Allocate a PKT_INFO_T for Null Frame */
	/* Init with MGMT Header Length */
	u2EstimatedFrameLen = MAC_TX_RESERVED_FIELD
		+ WLAN_MAC_HEADER_QOS_HTC_LEN;

	/* Allocate a MSDU_INFO_T */
	prMsduInfo = cnmMgtPktAlloc(prAdapter, u2EstimatedFrameLen);
	if (prMsduInfo == NULL) {
		DBGLOG(RLM, WARN, "No PKT_INFO_T for sending Null Frame.\n");
		return WLAN_STATUS_RESOURCES;
	}
	/* 4 <2> Compose Null frame in MSDU_INfO_T. */
	heRlmComposeHtcNullFrame(prAdapter,
		(uint8_t *)((unsigned long)(prMsduInfo->prPacket)
			+ MAC_TX_RESERVED_FIELD),
		prStaRec, ucUP, FALSE);

	TX_SET_MMPDU(prAdapter,
		     prMsduInfo,
		     prStaRec->ucBssIndex,
		     prStaRec->ucIndex,
		     WLAN_MAC_HEADER_QOS_HTC_LEN,
		     WLAN_MAC_HEADER_QOS_HTC_LEN,
		     pfTxDoneHandler,
		     MSDU_RATE_MODE_AUTO);

	prMsduInfo->fgMgmtUseDataQ = TRUE;
	/* prMsduInfo->ucUserPriority = 0;*/
	prMsduInfo->u4Option |= MSDU_OPT_NO_AGGREGATE;
	prMsduInfo->u4Option |= MSDU_OPT_SW_HTC;
	/* 4 <4> Inform TXM  to send this Null frame. */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);

	return WLAN_STATUS_SUCCESS;

}				/* end of bssSendQoSNullFrame() */

void heRlmInit(
	struct ADAPTER *prAdapter)
{
	struct __HE_CFG_INFO_T *prHeCfg;

	prHeCfg = &prAdapter->rHeCfg;
	prHeCfg->fgHeSupport = TRUE;
	/* It can be disabled by wifi.cfg or iwpriv command */
	prHeCfg->fgHeEnable = TRUE;
	/* It can be disabled by wifi.cfg or iwpriv command */
	prHeCfg->fgTwtRequesterEnable = TRUE;
}
#endif /* CFG_SUPPORT_802_11AX == 1 */
