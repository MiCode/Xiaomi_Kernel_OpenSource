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

/*! \file   "nic_rate.c"
 *    \brief  This file contains the transmission rate handling routines.
 *
 *    This file contains the transmission rate handling routines for setting up
 *    ACK/CTS Rate, Highest Tx Rate, Lowest Tx Rate, Initial Tx Rate and do
 *    conversion between Rate Set and Data Rates.
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
#define RA_FIXEDRATE_FIELD_CCK_MCS_S		0
#define RA_FIXEDRATE_FIELD_CCK_MCS_E		1
#define RA_FIXEDRATE_FIELD_S_PREAMBLE		2
#define RA_FIXEDRATE_FIELD_MCS_S		0
#define RA_FIXEDRATE_FIELD_MCS_E		5

#define RA_FIXEDRATE_V1_FIELD_MODE_S		6
#define RA_FIXEDRATE_V1_FIELD_MODE_E		8
#define RA_FIXEDRATE_V1_FIELD_VHTNSS_S		9
#define RA_FIXEDRATE_V1_FIELD_VHTNSS_E		10

#define RA_FIXEDRATE_V2_FIELD_MODE_S		6
#define RA_FIXEDRATE_V2_FIELD_MODE_E		10
#define RA_FIXEDRATE_V2_FIELD_VHTNSS_S		12
#define RA_FIXEDRATE_V2_FIELD_VHTNSS_E		14

#define RA_FIXEDRATE_FIELD_STBC			11

#define RA_FIXEDRATE_FIELD_HE_LTF_MASK		BITS(15, 16)
#define RA_FIXEDRATE_FIELD_HE_LTF_OFFSET        15
#define RA_FIXEDRATE_FIELD_HE_GI_MASK		BITS(17, 18)
#define RA_FIXEDRATE_FIELD_HE_GI_OFFSET		17
#define RA_FIXEDRATE_FIELD_HE_ER_DCM		19
#define RA_FIXEDRATE_FIELD_HE_ER_106		20

#define RA_FIXEDRATE_FIELD_FORMAT_VER_MASK	BITS(23, 24)
#define RA_FIXEDRATE_FIELD_FORMAT_VER_OFFSET	23

#define RA_FIXEDRATE_FIELD_BAND			25
#define RA_FIXEDRATE_FIELD_BW_S			26
#define RA_FIXEDRATE_FIELD_BW_E			27
#define RA_FIXEDRATE_FIELD_SPEEN		28
#define RA_FIXEDRATE_FIELD_LDPC			29
#define RA_FIXEDRATE_FIELD_SGI			30
#define RA_FIXEDRATE				BIT(31)


const uint16_t au2RateCCKLong[CCK_RATE_NUM] = {
	RATE_CCK_1M_LONG,	/* RATE_1M_INDEX = 0 */
	RATE_CCK_2M_LONG,	/* RATE_2M_INDEX */
	RATE_CCK_5_5M_LONG,	/* RATE_5_5M_INDEX */
	RATE_CCK_11M_LONG	/* RATE_11M_INDEX */
};

const uint16_t au2RateCCKShort[CCK_RATE_NUM] = {
	RATE_CCK_1M_LONG,	/* RATE_1M_INDEX = 0 */
	RATE_CCK_2M_SHORT,	/* RATE_2M_INDEX */
	RATE_CCK_5_5M_SHORT,	/* RATE_5_5M_INDEX */
	RATE_CCK_11M_SHORT	/* RATE_11M_INDEX */
};

const uint16_t au2RateOFDM[OFDM_RATE_NUM] = {
	RATE_OFDM_6M,		/* RATE_6M_INDEX */
	RATE_OFDM_9M,		/* RATE_9M_INDEX */
	RATE_OFDM_12M,		/* RATE_12M_INDEX */
	RATE_OFDM_18M,		/* RATE_18M_INDEX */
	RATE_OFDM_24M,		/* RATE_24M_INDEX */
	RATE_OFDM_36M,		/* RATE_36M_INDEX */
	RATE_OFDM_48M,		/* RATE_48M_INDEX */
	RATE_OFDM_54M		/* RATE_54M_INDEX */
};

const uint16_t au2RateHTMixed[HT_RATE_NUM] = {
	RATE_MM_MCS_32,		/* RATE_MCS32_INDEX, */
	RATE_MM_MCS_0,		/* RATE_MCS0_INDEX, */
	RATE_MM_MCS_1,		/* RATE_MCS1_INDEX, */
	RATE_MM_MCS_2,		/* RATE_MCS2_INDEX, */
	RATE_MM_MCS_3,		/* RATE_MCS3_INDEX, */
	RATE_MM_MCS_4,		/* RATE_MCS4_INDEX, */
	RATE_MM_MCS_5,		/* RATE_MCS5_INDEX, */
	RATE_MM_MCS_6,		/* RATE_MCS6_INDEX, */
	RATE_MM_MCS_7		/* RATE_MCS7_INDEX, */
};

const uint16_t au2RateHTGreenField[HT_RATE_NUM] = {
	RATE_GF_MCS_32,		/* RATE_MCS32_INDEX, */
	RATE_GF_MCS_0,		/* RATE_MCS0_INDEX, */
	RATE_GF_MCS_1,		/* RATE_MCS1_INDEX, */
	RATE_GF_MCS_2,		/* RATE_MCS2_INDEX, */
	RATE_GF_MCS_3,		/* RATE_MCS3_INDEX, */
	RATE_GF_MCS_4,		/* RATE_MCS4_INDEX, */
	RATE_GF_MCS_5,		/* RATE_MCS5_INDEX, */
	RATE_GF_MCS_6,		/* RATE_MCS6_INDEX, */
	RATE_GF_MCS_7,		/* RATE_MCS7_INDEX, */
};

const uint16_t au2RateVHT[VHT_RATE_NUM] = {
	RATE_VHT_MCS_0,		/* RATE_MCS0_INDEX, */
	RATE_VHT_MCS_1,		/* RATE_MCS1_INDEX, */
	RATE_VHT_MCS_2,		/* RATE_MCS2_INDEX, */
	RATE_VHT_MCS_3,		/* RATE_MCS3_INDEX, */
	RATE_VHT_MCS_4,		/* RATE_MCS4_INDEX, */
	RATE_VHT_MCS_5,		/* RATE_MCS5_INDEX, */
	RATE_VHT_MCS_6,		/* RATE_MCS6_INDEX, */
	RATE_VHT_MCS_7,		/* RATE_MCS7_INDEX, */
	RATE_VHT_MCS_8,		/* RATE_MCS8_INDEX, */
	RATE_VHT_MCS_9		/* RATE_MCS9_INDEX, */
};

/* in unit of 100kb/s */
const struct EMU_MAC_RATE_INFO arMcsRate2PhyRate[] = {
	/* Phy Rate Code,
	 * BW20,  BW20 SGI, BW40, BW40 SGI, BW80, BW80 SGI, BW160, BW160 SGI
	 */
	RATE_INFO(PHY_RATE_MCS0, 65, 72, 135, 150, 293, 325, 585, 650),
	RATE_INFO(PHY_RATE_MCS1, 130, 144, 270, 300, 585, 650, 1170, 1300),
	RATE_INFO(PHY_RATE_MCS2, 195, 217, 405, 450, 878, 975, 1755, 1950),
	RATE_INFO(PHY_RATE_MCS3, 260, 289, 540, 600, 1170, 1300, 2340, 2600),
	RATE_INFO(PHY_RATE_MCS4, 390, 433, 810, 900, 1755, 1950, 3510, 3900),
	RATE_INFO(PHY_RATE_MCS5, 520, 578, 1080, 1200, 2340, 2600, 4680, 5200),
	RATE_INFO(PHY_RATE_MCS6, 585, 650, 1215, 1350, 2633, 2925, 5265, 5850),
	RATE_INFO(PHY_RATE_MCS7, 650, 722, 1350, 1500, 2925, 3250, 5850, 6500),
	RATE_INFO(PHY_RATE_MCS8, 780, 867, 1620, 1800, 3510, 3900, 7020, 7800),
	RATE_INFO(PHY_RATE_MCS9, 867, 963, 1800, 2000, 3900, 4333, 7800, 8667),
	RATE_INFO(PHY_RATE_MCS32, 0, 0, 60, 67, 0, 0, 0, 0)
};

/* in uint of 500kb/s */
const uint8_t aucHwRate2PhyRate[] = {
	RATE_1M,		/*1M long */
	RATE_2M,		/*2M long */
	RATE_5_5M,		/*5.5M long */
	RATE_11M,		/*11M long */
	RATE_1M,		/*1M short invalid */
	RATE_2M,		/*2M short */
	RATE_5_5M,		/*5.5M short */
	RATE_11M,		/*11M short */
	RATE_48M,		/*48M */
	RATE_24M,		/*24M */
	RATE_12M,		/*12M */
	RATE_6M,		/*6M */
	RATE_54M,		/*54M */
	RATE_36M,		/*36M */
	RATE_18M,		/*18M */
	RATE_9M			/*9M */
};
/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

char *HW_TX_MODE_STR[] = {
	"CCK", "OFDM", "MM", "GF", "VHT", "PLR",
	"N/A", "N/A", "HE_SU", "HE_ER", "HE_TRIG", "HE_MU"};
char *HW_TX_RATE_CCK_STR[] = {"1M", "2M", "5.5M", "11M", "N/A"};
char *HW_TX_RATE_OFDM_STR[] = {"6M", "9M", "12M", "18M", "24M", "36M",
				      "48M", "54M", "N/A"};
char *HW_TX_RATE_BW[] = {"BW20", "BW40", "BW80", "BW160/BW8080", "N/A"};

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */
#define TX_GET_GI(_gi, _mode)	\
	((_mode) >= TX_RATE_MODE_HE_SU ? (((_gi) & 0xf0) >> 4) : ((_gi) & 0xf))

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
uint32_t
nicGetPhyRateByMcsRate(
	IN uint8_t ucIdx,
	IN uint8_t ucBw,
	IN uint8_t ucGI)
{
	return	arMcsRate2PhyRate[ucIdx].u4PhyRate[ucBw][ucGI];
}

uint32_t
nicGetHwRateByPhyRate(
	IN uint8_t ucIdx)
{
	return	aucHwRate2PhyRate[ucIdx]; /* uint : 500 kbps */
}

uint32_t
nicSwIndex2RateIndex(
	IN uint8_t ucRateSwIndex,
	OUT uint8_t *pucRateIndex,
	OUT uint8_t *pucPreambleOption
)
{
	ASSERT(pucRateIndex);
	ASSERT(pucPreambleOption);

	if (ucRateSwIndex >= RATE_6M_SW_INDEX) {
		*pucRateIndex = ucRateSwIndex - RATE_6M_SW_INDEX;
		*pucPreambleOption = PREAMBLE_OFDM_MODE;
	} else {
		*pucRateIndex = ucRateSwIndex;
		*pucPreambleOption = PREAMBLE_DEFAULT_LONG_NONE;
	}
	return WLAN_STATUS_SUCCESS;
}

uint32_t nicRateIndex2RateCode(IN uint8_t ucPreambleOption,
	IN uint8_t ucRateIndex, OUT uint16_t *pu2RateCode)
{
	switch (ucPreambleOption) {
	case PREAMBLE_DEFAULT_LONG_NONE:
		if (ucRateIndex >= CCK_RATE_NUM)
			return WLAN_STATUS_INVALID_DATA;
		*pu2RateCode = au2RateCCKLong[ucRateIndex];
		break;

	case PREAMBLE_OPTION_SHORT:
		if (ucRateIndex >= CCK_RATE_NUM)
			return WLAN_STATUS_INVALID_DATA;
		*pu2RateCode = au2RateCCKShort[ucRateIndex];
		break;

	case PREAMBLE_OFDM_MODE:
		if (ucRateIndex >= OFDM_RATE_NUM)
			return WLAN_STATUS_INVALID_DATA;
		*pu2RateCode = au2RateOFDM[ucRateIndex];
		break;

	case PREAMBLE_HT_MIXED_MODE:
		if (ucRateIndex >= HT_RATE_NUM)
			return WLAN_STATUS_INVALID_DATA;
		*pu2RateCode = au2RateHTMixed[ucRateIndex];
		break;

	case PREAMBLE_HT_GREEN_FIELD:
		if (ucRateIndex >= HT_RATE_NUM)
			return WLAN_STATUS_INVALID_DATA;
		*pu2RateCode = au2RateHTGreenField[ucRateIndex];
		break;

	case PREAMBLE_VHT_FIELD:
		if (ucRateIndex >= VHT_RATE_NUM)
			return WLAN_STATUS_INVALID_DATA;
		*pu2RateCode = au2RateVHT[ucRateIndex];
		break;

	default:
		return WLAN_STATUS_INVALID_DATA;
	}

	return WLAN_STATUS_SUCCESS;
}

uint32_t
nicRateCode2PhyRate(
	IN uint16_t  u2RateCode,
	IN uint8_t   ucBandwidth,
	IN uint8_t   ucGI,
	IN uint8_t   ucRateNss)
{
	uint8_t ucPhyRate;
	uint16_t u2TxMode;
	uint32_t u4PhyRateBy1SS, u4PhyRateIn100Kbps = 0;

	ucPhyRate = RATE_CODE_GET_PHY_RATE(u2RateCode);
	u2TxMode = u2RateCode & RATE_TX_MODE_MASK;
	ucRateNss = ucRateNss + AR_SS_1; /* change to be base=1 */

	if ((u2TxMode == TX_MODE_HT_GF)
	    || (u2TxMode == TX_MODE_HT_MM)) {

		if (ucPhyRate > PHY_RATE_MCS7)
			u2RateCode = u2RateCode - HT_RATE_MCS7_INDEX;
		else
			ucRateNss = AR_SS_1;

	} else if ((u2TxMode == TX_MODE_OFDM)
		   || (u2TxMode == TX_MODE_CCK)) {
		ucRateNss = AR_SS_1;
	}
	DBGLOG(NIC, LOUD,
	       "Coex:nicRateCode2PhyRate,RC:%x,B:%d,I:%d\n",
	       u2RateCode, ucBandwidth, ucGI);

	u4PhyRateBy1SS = nicRateCode2DataRate(u2RateCode,
					      ucBandwidth, ucGI);
	u4PhyRateIn100Kbps = u4PhyRateBy1SS * ucRateNss;

	DBGLOG(NIC, LOUD,
	       "Coex:nicRateCode2PhyRate,1ss R:%d,PHY R:%d\n",
	       u4PhyRateBy1SS, u4PhyRateIn100Kbps);

	return u4PhyRateIn100Kbps;
}

uint32_t
nicRateCode2DataRate(
	IN uint16_t  u2RateCode,
	IN uint8_t   ucBandwidth,
	IN uint8_t   ucGI)
{
	uint8_t ucPhyRate, ucIdx, ucBw = 0;
	uint32_t u4PhyRateIn100Kbps = 0;
	uint16_t u2TxMode;

	if ((ucBandwidth == FIX_BW_NO_FIXED)
	    || (ucBandwidth == FIX_BW_20))
		ucBw = MAC_BW_20;
	else if (ucBandwidth == FIX_BW_40)
		ucBw = MAC_BW_40;
	else if (ucBandwidth == FIX_BW_80)
		ucBw = MAC_BW_80;
	else if (ucBandwidth == FIX_BW_160)
		ucBw = MAC_BW_160;

	ucPhyRate = RATE_CODE_GET_PHY_RATE(u2RateCode);
	u2TxMode = u2RateCode & RATE_TX_MODE_MASK;
	/* Set MMSS parameter if HT/VHT rate */
	if ((u2TxMode == TX_MODE_HT_GF) ||
	    (u2TxMode == TX_MODE_HT_MM) ||
	    (u2TxMode == TX_MODE_VHT)) {
		/* No SGI Greenfield for 1T */
		/* Refer to section 20.3.11.11.6 of IEEE802.11-2012 */
		if (u2TxMode == TX_MODE_HT_GF)
			ucGI = MAC_GI_NORMAL;

		ucIdx = ucPhyRate;

		if (ucIdx == PHY_RATE_MCS32)
			ucIdx = 10;

		u4PhyRateIn100Kbps = nicGetPhyRateByMcsRate(ucIdx, ucBw,
				     ucGI);
	} else if ((u2TxMode == TX_MODE_OFDM) ||
		   (u2TxMode == TX_MODE_CCK)) {
		u4PhyRateIn100Kbps = (nicGetHwRateByPhyRate(
					      ucPhyRate & BITS(0, 3))) * 5;
	} else {
		ASSERT(FALSE);
	}
	return u4PhyRateIn100Kbps;
}

u_int8_t
nicGetRateIndexFromRateSetWithLimit(
	IN uint16_t u2RateSet,
	IN uint32_t u4PhyRateLimit,
	IN u_int8_t fgGetLowest,
	OUT uint8_t *pucRateSwIndex)
{
	uint32_t i;
	uint32_t u4CurPhyRate, u4TarPhyRate, u4HighestPhyRate,
		 u4LowestPhyRate;
	uint8_t ucRateIndex, ucRatePreamble, ucTarRateSwIndex,
		ucHighestPhyRateSwIdx, ucLowestPhyRateSwIdx;
	uint16_t u2CurRateCode;
	uint32_t u4Status;

	/* Set init value */
	if (fgGetLowest) {
		u4TarPhyRate = 0xFFFFFFFF;
		u4HighestPhyRate = 0;
		ucHighestPhyRateSwIdx = RATE_NUM_SW;
	} else {
		u4TarPhyRate = 0;
		u4LowestPhyRate = 0xFFFFFFFF;
		ucLowestPhyRateSwIdx = RATE_NUM_SW;
	}

	ucTarRateSwIndex = RATE_NUM_SW;

	/* Find SW rate index by limitation */
	for (i = RATE_1M_SW_INDEX; i <= RATE_54M_SW_INDEX; i++) {
		if (u2RateSet & BIT(i)) {

			/* Convert SW rate index to phy rate in 100kbps */
			nicSwIndex2RateIndex(i, &ucRateIndex, &ucRatePreamble);
			u4Status = nicRateIndex2RateCode(ucRatePreamble,
				ucRateIndex, &u2CurRateCode);

			if (u4Status != WLAN_STATUS_SUCCESS)
				continue;

			u4CurPhyRate =
				nicRateCode2DataRate(u2CurRateCode, MAC_BW_20,
						     MAC_GI_NORMAL);

			/* Compare */
			if (fgGetLowest) {
				if (u4HighestPhyRate < u4CurPhyRate) {
					u4HighestPhyRate = u4CurPhyRate;
					ucHighestPhyRateSwIdx = i;
				}
				if ((u4CurPhyRate >= u4PhyRateLimit)
				    && (u4CurPhyRate <= u4TarPhyRate)) {
					u4TarPhyRate = u4CurPhyRate;
					ucTarRateSwIndex = i;
				}
			} else {
				if (u4LowestPhyRate > u4CurPhyRate) {
					u4LowestPhyRate = u4CurPhyRate;
					ucLowestPhyRateSwIdx = i;
				}
				if ((u4CurPhyRate <= u4PhyRateLimit)
				    && (u4CurPhyRate >= u4TarPhyRate)) {
					u4TarPhyRate = u4CurPhyRate;
					ucTarRateSwIndex = i;
				}
			}
		}
	}

	/* Return target SW rate index */
	if (ucTarRateSwIndex < RATE_NUM_SW) {
		*pucRateSwIndex = ucTarRateSwIndex;
	} else {
		if (fgGetLowest)
			*pucRateSwIndex = ucHighestPhyRateSwIdx;
		else
			*pucRateSwIndex = ucLowestPhyRateSwIdx;
	}
	return TRUE;
}

char *nicHwRateOfdmStr(
	uint16_t ofdm_idx)
{
	switch (ofdm_idx) {
	case 11: /* 6M */
		return HW_TX_RATE_OFDM_STR[0];
	case 15: /* 9M */
		return HW_TX_RATE_OFDM_STR[1];
	case 10: /* 12M */
		return HW_TX_RATE_OFDM_STR[2];
	case 14: /* 18M */
		return HW_TX_RATE_OFDM_STR[3];
	case 9: /* 24M */
		return HW_TX_RATE_OFDM_STR[4];
	case 13: /* 36M */
		return HW_TX_RATE_OFDM_STR[5];
	case 8: /* 48M */
		return HW_TX_RATE_OFDM_STR[6];
	case 12: /* 54M */
		return HW_TX_RATE_OFDM_STR[7];
	default:
		return HW_TX_RATE_OFDM_STR[8];
	}
}

uint32_t nicSetFixedRateData(
	struct FIXED_RATE_INFO *pFixedRate,
	uint32_t *pu4Data)
{
	uint32_t u4Data = 0;
	uint8_t u4Nsts = 1;
	uint8_t u1FormatVer;
	uint8_t u1TxModeMcsNumMax[ENUM_TX_MODE_NUM];

	kalMemZero(u1TxModeMcsNumMax, ENUM_TX_MODE_NUM);
	/* u1TxModeMcsNumMax[ENUM_TX_MODE_NUM] = {4, 8, 33, 33, 10}; */
	u1TxModeMcsNumMax[ENUM_TX_MODE_CCK] = 4;
	u1TxModeMcsNumMax[ENUM_TX_MODE_OFDM] = 8;
	u1TxModeMcsNumMax[ENUM_TX_MODE_MM] = 33;
	u1TxModeMcsNumMax[ENUM_TX_MODE_GF] = 33;
	u1TxModeMcsNumMax[ENUM_TX_MODE_VHT] = 10;

#if (CFG_SUPPORT_802_11AX == 1)
	if (fgEfuseCtrlAxOn == 1) {
	/* u1TxModeMcsNumMax[ENUM_TX_MODE_NUM] */
		/* = {4, 8, 33, 33, 10, 2, 0, 0, 12, 12, 12, 12}; */
		u1TxModeMcsNumMax[ENUM_TX_MODE_PLR] = 2;
		u1TxModeMcsNumMax[ENUM_TX_MODE_HE_SU] = 12;
		u1TxModeMcsNumMax[ENUM_TX_MODE_HE_ER] = 12;
		u1TxModeMcsNumMax[ENUM_TX_MODE_HE_TRIG] = 12;
		u1TxModeMcsNumMax[ENUM_TX_MODE_HE_MU] = 12;
	}
#endif

	u4Data |= RA_FIXEDRATE;

	u1FormatVer = (pFixedRate->u4Mode < TX_RATE_MODE_HE_SU) ?
			RATE_VER_1 : RATE_VER_2;

	u4Data |= ((u1FormatVer << RA_FIXEDRATE_FIELD_FORMAT_VER_OFFSET)
			& RA_FIXEDRATE_FIELD_FORMAT_VER_MASK);

	if (u1FormatVer == RATE_VER_1) {
		if (pFixedRate->u4SGI)
			u4Data |= BIT(RA_FIXEDRATE_FIELD_SGI);
	} else {
		if (pFixedRate->u4SGI < GI_HE_NUM)
			u4Data |= ((pFixedRate->u4SGI <<
				RA_FIXEDRATE_FIELD_HE_GI_OFFSET) &
				RA_FIXEDRATE_FIELD_HE_GI_MASK);
		else
			DBGLOG(INIT, ERROR,
				"Wrong HE GI! SGI=0, MGI=1, LGI=2\n");
	}

	if (pFixedRate->u4LDPC)
		u4Data |= BIT(RA_FIXEDRATE_FIELD_LDPC);
	if (pFixedRate->u4SpeEn)
		u4Data |= BIT(RA_FIXEDRATE_FIELD_SPEEN);
	if (pFixedRate->u4STBC)
		u4Data |= BIT(RA_FIXEDRATE_FIELD_STBC);

	if (pFixedRate->u4Bw <= MAC_BW_160)
		u4Data |= ((pFixedRate->u4Bw << RA_FIXEDRATE_FIELD_BW_S)
		& BITS(RA_FIXEDRATE_FIELD_BW_S, RA_FIXEDRATE_FIELD_BW_E));
	else {
		DBGLOG(INIT, ERROR,
		       "Wrong BW! BW20=0, BW40=1, BW80=2,BW160=3\n");
		return WLAN_STATUS_INVALID_DATA;
	}

	if (pFixedRate->u4Mode < ENUM_TX_MODE_NUM) {
		if (u1FormatVer == RATE_VER_1)
			u4Data |=
			((pFixedRate->u4Mode << RA_FIXEDRATE_V1_FIELD_MODE_S)
			& BITS(RA_FIXEDRATE_V1_FIELD_MODE_S,
				RA_FIXEDRATE_V1_FIELD_MODE_E));
		else
			u4Data |=
			((pFixedRate->u4Mode << RA_FIXEDRATE_V2_FIELD_MODE_S)
			& BITS(RA_FIXEDRATE_V2_FIELD_MODE_S,
				RA_FIXEDRATE_V2_FIELD_MODE_E));

		if (pFixedRate->u4Mcs < u1TxModeMcsNumMax[pFixedRate->u4Mode]) {
			if (pFixedRate->u4Mode != TX_RATE_MODE_OFDM)
				u4Data |= pFixedRate->u4Mcs;
		} else {
			DBGLOG(INIT, ERROR, "%s mode but wrong MCS(=%d)!\n",
				HW_TX_MODE_STR[pFixedRate->u4Mode],
				pFixedRate->u4Mcs);
				return WLAN_STATUS_INVALID_DATA;
		}

		switch (pFixedRate->u4Mode) {
		case TX_RATE_MODE_CCK:
			if (pFixedRate->u4Preamble)
				if (pFixedRate->u4Mcs > 0)
					u4Data |=
					BIT(RA_FIXEDRATE_FIELD_S_PREAMBLE);
				else {
					DBGLOG(INIT, ERROR, "SP but MCS=0!\n");
					return WLAN_STATUS_INVALID_DATA;
				}
			else
				u4Data &= ~BIT(RA_FIXEDRATE_FIELD_S_PREAMBLE);
			break;
		case TX_RATE_MODE_OFDM:
			switch (pFixedRate->u4Mcs) {
			case 0:
				/* 6'b001011 */
				u4Data |= 11;
				break;
			case 1:
				/* 6'b001111 */
				u4Data |= 15;
				break;
			case 2:
				/* 6'b001010 */
				u4Data |= 10;
				break;
			case 3:
				/* 6'b001110 */
				u4Data |= 14;
				break;
			case 4:
				/* 6'b001001 */
				u4Data |= 9;
				break;
			case 5:
				/* 6'b001101 */
				u4Data |= 13;
				break;
			case 6:
				/* 6'b001000 */
				u4Data |= 8;
				break;
			case 7:
				/* 6'b001100 */
				u4Data |= 12;
				break;
			default:
				DBGLOG(INIT, ERROR,
				       "OFDM mode but wrong MCS!\n");
				return WLAN_STATUS_INVALID_DATA;
			}
			break;
		case TX_RATE_MODE_HTMIX:
		case TX_RATE_MODE_HTGF:
			if (pFixedRate->u4Mcs != 32) {
				u4Nsts += (pFixedRate->u4Mcs >> 3);
				if (pFixedRate->u4STBC && (u4Nsts == 1))
					u4Nsts++;
			}
			break;
		case TX_RATE_MODE_PLR:
			break;
		case TX_RATE_MODE_VHT:
		case TX_RATE_MODE_HE_SU:
		case TX_RATE_MODE_HE_ER:
		case TX_RATE_MODE_HE_TRIG:
		case TX_RATE_MODE_HE_MU:
			if (pFixedRate->u4STBC && (pFixedRate->u4VhtNss == 1))
				u4Nsts++;
			else
				u4Nsts = pFixedRate->u4VhtNss;
			break;
		default:
			break;
		}
	} else {
		DBGLOG(INIT, ERROR,
			"Wrong TxMode! CCK=0, OFDM=1, HT=2, GF=3, VHT=4");
		DBGLOG(INIT, ERROR,
			"HE_SU=8, HE_ER_SU=9, HE_TRIG=10, HE_MU=11\n");
		return WLAN_STATUS_INVALID_DATA;
	}

	if (u1FormatVer == RATE_VER_1)
		u4Data |= (((u4Nsts - 1) << RA_FIXEDRATE_V1_FIELD_VHTNSS_S)
			& BITS(RA_FIXEDRATE_V1_FIELD_VHTNSS_S,
			RA_FIXEDRATE_V1_FIELD_VHTNSS_E));
	else {
		u4Data |= (((u4Nsts - 1) << RA_FIXEDRATE_V2_FIELD_VHTNSS_S)
			& BITS(RA_FIXEDRATE_V2_FIELD_VHTNSS_S,
			RA_FIXEDRATE_V2_FIELD_VHTNSS_E));
		u4Data |= ((pFixedRate->u4HeLTF <<
			RA_FIXEDRATE_FIELD_HE_LTF_OFFSET)
			& RA_FIXEDRATE_FIELD_HE_LTF_MASK);

		if (pFixedRate->u4Mode == TX_RATE_MODE_HE_ER) {
			if (pFixedRate->u4HeErDCM)
				u4Data |= RA_FIXEDRATE_FIELD_HE_ER_DCM;
			if (pFixedRate->u4HeEr106t)
				u4Data |= RA_FIXEDRATE_FIELD_HE_ER_106;
		}
	}

	*pu4Data = u4Data;
	return WLAN_STATUS_SUCCESS;
}

uint32_t nicRateHeLtfCheckGi(
	struct FIXED_RATE_INFO *pFixedRate)
{
	uint32_t u4Mode, u4GI;

	u4Mode = pFixedRate->u4Mode;
	u4GI = pFixedRate->u4SGI;

	if (u4Mode < TX_RATE_MODE_HE_SU)
		return WLAN_STATUS_SUCCESS;

	switch (pFixedRate->u4HeLTF) {
	case HE_LTF_1X:
		if (u4GI == GI_HE_SGI) {
			if ((u4Mode == TX_RATE_MODE_HE_SU) ||
				(u4Mode == TX_RATE_MODE_HE_ER))
				return WLAN_STATUS_SUCCESS;
		} else if (u4GI == GI_HE_MGI) {
			/* also need non-OFDMA */
			if (u4Mode == TX_RATE_MODE_HE_TRIG)
				return WLAN_STATUS_SUCCESS;
		}
		break;
	case HE_LTF_2X:
		if (u4GI == GI_HE_SGI) {
			if ((u4Mode == TX_RATE_MODE_HE_SU) ||
				(u4Mode == TX_RATE_MODE_HE_ER) ||
				(u4Mode == TX_RATE_MODE_HE_MU))
				return WLAN_STATUS_SUCCESS;
		} else if (u4GI == GI_HE_MGI) {
			if ((u4Mode >= TX_RATE_MODE_HE_SU) &&
				(u4Mode <= TX_RATE_MODE_HE_MU))
				return WLAN_STATUS_SUCCESS;
		}
		break;
	case HE_LTF_4X:
		if (u4GI == GI_HE_SGI) {
			if ((u4Mode == TX_RATE_MODE_HE_SU) ||
				(u4Mode == TX_RATE_MODE_HE_ER) ||
				(u4Mode == TX_RATE_MODE_HE_MU))
				return WLAN_STATUS_SUCCESS;
		} else if (u4GI == GI_HE_LGI) {
			if ((u4Mode >= TX_RATE_MODE_HE_SU) &&
				(u4Mode <= TX_RATE_MODE_HE_MU))
				return WLAN_STATUS_SUCCESS;
		}
		break;

	default:
		break;
	}

	return WLAN_STATUS_FAILURE;
}

uint8_t nicGetTxSgiInfo(
	IN struct PARAM_PEER_CAP *prWtblPeerCap,
	IN uint8_t u1TxMode)
{
	if (!prWtblPeerCap)
		return FALSE;

	switch (prWtblPeerCap->ucFrequencyCapability) {
	case BW_20:
		return TX_GET_GI(prWtblPeerCap->fgG2, u1TxMode);
	case BW_40:
		return TX_GET_GI(prWtblPeerCap->fgG4, u1TxMode);
	case BW_80:
		return TX_GET_GI(prWtblPeerCap->fgG8, u1TxMode);
	case BW_160:
		return TX_GET_GI(prWtblPeerCap->fgG16, u1TxMode);
	default:
		return FALSE;
	}
}

uint8_t nicGetTxLdpcInfo(
	IN struct PARAM_TX_CONFIG *prWtblTxConfig)
{
	if (!prWtblTxConfig)
		return FALSE;

	if (prWtblTxConfig->fgIsHE)
		return prWtblTxConfig->fgHeLDPC;
	else if (prWtblTxConfig->fgIsVHT)
		return prWtblTxConfig->fgVhtLDPC;
	else
		return prWtblTxConfig->fgLDPC;
}

uint16_t nicGetStatIdxInfo(IN struct ADAPTER *prAdapter,
				  IN uint8_t ucWlanIdx)
{
	static uint8_t aucWlanIdxArray[CFG_STAT_DBG_PEER_NUM] = {0};
	static uint16_t u2ValidBitMask;	/* support max 16 peers */
	uint8_t ucIdx, ucStaIdx, ucCnt = 0;
	uint8_t ucWlanIdxExist;

	/* check every wlanIdx and unmask no longer used wlanIdx */
	for (ucIdx = 0; ucIdx < CFG_STAT_DBG_PEER_NUM; ucIdx++) {
		if (u2ValidBitMask & BIT(ucIdx)) {
			ucWlanIdxExist = aucWlanIdxArray[ucIdx];

			if (wlanGetStaIdxByWlanIdx(prAdapter, ucWlanIdxExist,
				&ucStaIdx) != WLAN_STATUS_SUCCESS)
				u2ValidBitMask &= ~BIT(ucIdx);
		}
	}

	/* Search matched WlanIdx */
	for (ucIdx = 0; ucIdx < CFG_STAT_DBG_PEER_NUM; ucIdx++) {
		if (u2ValidBitMask & BIT(ucIdx)) {
			ucCnt++;
			ucWlanIdxExist = aucWlanIdxArray[ucIdx];

			if (ucWlanIdxExist == ucWlanIdx) {
				DBGLOG(REQ, INFO,
				    "=== Matched, Mask=0x%x, ucIdx=%d ===\n",
				    u2ValidBitMask, ucIdx);
				return ucIdx;
			}
		}
	}

	/* No matched WlanIdx, add new one */
	if (ucCnt < CFG_STAT_DBG_PEER_NUM) {
		for (ucIdx = 0; ucIdx < CFG_STAT_DBG_PEER_NUM; ucIdx++)	{
			if (~u2ValidBitMask & BIT(ucIdx)) {
				u2ValidBitMask |= BIT(ucIdx);
				aucWlanIdxArray[ucIdx] = ucWlanIdx;
				DBGLOG(REQ, INFO,
				    "=== New Add, Mask=0x%x, ucIdx=%d ===\n",
				    u2ValidBitMask, ucIdx);
				return ucIdx;
			}
		}
	}

	return 0xFFFF;
}

int32_t nicGetTxRateInfo(IN char *pcCommand, IN int i4TotalLen,
			u_int8_t fgDumpAll,
			struct PARAM_HW_WLAN_INFO *prHwWlanInfo,
			struct PARAM_GET_STA_STATISTICS *prQueryStaStatistics)
{
	uint8_t i, txmode, rate, stbc, sgi;
	uint8_t nsts;
	int32_t i4BytesWritten = 0;

	for (i = 0; i < AUTO_RATE_NUM; i++) {
		txmode = HW_TX_RATE_TO_MODE(
				prHwWlanInfo->rWtblRateInfo.au2RateCode[i]);
		if (txmode >= ENUM_TX_MODE_NUM)
			txmode = ENUM_TX_MODE_NUM - 1;
		rate = HW_TX_RATE_TO_MCS(
			prHwWlanInfo->rWtblRateInfo.au2RateCode[i]);
		nsts = HW_TX_RATE_TO_NSS(
			prHwWlanInfo->rWtblRateInfo.au2RateCode[i]) + 1;
		stbc = HW_TX_RATE_TO_STBC(
			prHwWlanInfo->rWtblRateInfo.au2RateCode[i]);
		sgi = nicGetTxSgiInfo(&prHwWlanInfo->rWtblPeerCap, txmode);

		if (fgDumpAll) {
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"Rate index[%d]    ", i);

			if (prHwWlanInfo->rWtblRateInfo.ucRateIdx == i) {
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%s", "--> ");
			} else {
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%s", "    ");
			}
		}

		if (!fgDumpAll) {
			if (prHwWlanInfo->rWtblRateInfo.ucRateIdx != i)
				continue;
			else
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%-20s%s", "Auto TX Rate", " = ");
		}

		if (txmode == TX_RATE_MODE_CCK)
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%s, ", HW_TX_RATE_CCK_STR[rate & 0x3]);
		else if (txmode == TX_RATE_MODE_OFDM)
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%s, ", nicHwRateOfdmStr(rate));
		else if ((txmode == TX_RATE_MODE_HTMIX) ||
			 (txmode == TX_RATE_MODE_HTGF))
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"MCS%d, ", rate);
		else
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%s%d_MCS%d, ", stbc ? "NSTS" : "NSS",
				nsts, rate);

		if (prQueryStaStatistics->ucSkipAr) {
			i4BytesWritten += kalScnprintf(
			    pcCommand + i4BytesWritten,
			    i4TotalLen - i4BytesWritten, "%s, ",
			    prHwWlanInfo->rWtblPeerCap.ucFrequencyCapability
			      < 4 ? HW_TX_RATE_BW[
			      prHwWlanInfo->rWtblPeerCap.ucFrequencyCapability]
			      : HW_TX_RATE_BW[4]);
		} else {
			if ((txmode == TX_RATE_MODE_CCK) ||
			    (txmode == TX_RATE_MODE_OFDM))
				i4BytesWritten += kalScnprintf(
				    pcCommand + i4BytesWritten,
				    i4TotalLen - i4BytesWritten,
				    "%s, ", HW_TX_RATE_BW[0]);
			else
			if (i > prHwWlanInfo->rWtblPeerCap
				.ucChangeBWAfterRateN)
				i4BytesWritten += kalScnprintf(
				    pcCommand + i4BytesWritten,
				    i4TotalLen - i4BytesWritten, "%s, ",
				    prHwWlanInfo->rWtblPeerCap.
					ucFrequencyCapability < 4 ?
				    (prHwWlanInfo->rWtblPeerCap.
					ucFrequencyCapability > BW_20 ?
					HW_TX_RATE_BW[prHwWlanInfo->
					    rWtblPeerCap
					    .ucFrequencyCapability - 1] :
					HW_TX_RATE_BW[prHwWlanInfo->rWtblPeerCap
					    .ucFrequencyCapability]) :
				    HW_TX_RATE_BW[4]);
			else
				i4BytesWritten += kalScnprintf(
				    pcCommand + i4BytesWritten,
				    i4TotalLen - i4BytesWritten,
				    "%s, ",
				    prHwWlanInfo->rWtblPeerCap.
					ucFrequencyCapability < 4 ?
				    HW_TX_RATE_BW[
					prHwWlanInfo->rWtblPeerCap
					.ucFrequencyCapability] :
				    HW_TX_RATE_BW[4]);
		}

		if (txmode == TX_RATE_MODE_CCK)
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%s, ", rate < 4 ? "LP" : "SP");
		else if (txmode == TX_RATE_MODE_OFDM)
			;
		else if ((txmode == TX_RATE_MODE_HTMIX) ||
			 (txmode == TX_RATE_MODE_HTGF) ||
			 (txmode == TX_RATE_MODE_VHT) ||
			 (txmode == TX_RATE_MODE_PLR))
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%s, ", sgi == 0 ? "LGI" : "SGI");
		else
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%s, ", sgi == 0 ? "SGI" :
				(sgi == 1 ? "MGI" : "LGI"));

		if (prQueryStaStatistics->ucSkipAr) {
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%s%s%s\n",
				txmode <= ENUM_TX_MODE_NUM ?
				    HW_TX_MODE_STR[txmode] : "N/A",
				stbc ? ", STBC, " : ", ",
				nicGetTxLdpcInfo(
				    &prHwWlanInfo->rWtblTxConfig) == 0 ?
				    "BCC" : "LDPC");
		} else {
#if (CFG_SUPPORT_RA_GEN == 0)
			if (prQueryStaStatistics->aucArRatePer[
			    prQueryStaStatistics->aucRateEntryIndex[i]] == 0xFF)
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%s%s%s   (--)\n",
					txmode < ENUM_TX_MODE_NUM ?
					    HW_TX_MODE_STR[txmode] : "N/A",
					stbc ? ", STBC, " : ", ",
					((nicGetTxLdpcInfo(
					    &prHwWlanInfo->rWtblTxConfig) == 0)
					    || (txmode == TX_RATE_MODE_CCK)
					    || (txmode == TX_RATE_MODE_OFDM)) ?
						"BCC" : "LDPC");
			else
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%s%s%s   (%d)\n",
					txmode < ENUM_TX_MODE_NUM ?
					    HW_TX_MODE_STR[txmode] : "N/A",
					stbc ? ", STBC, " : ", ",
					((nicGetTxLdpcInfo(
					    &prHwWlanInfo->rWtblTxConfig) == 0)
					    || (txmode == TX_RATE_MODE_CCK)
					    || (txmode == TX_RATE_MODE_OFDM))
						? "BCC" : "LDPC",
					prQueryStaStatistics->aucArRatePer[
					    prQueryStaStatistics
					    ->aucRateEntryIndex[i]]);
#else
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%s%s%s\n",
				txmode < ENUM_TX_MODE_NUM ?
				    HW_TX_MODE_STR[txmode] : "N/A",
				stbc ? ", STBC, " : ", ",
				((nicGetTxLdpcInfo(
				    &prHwWlanInfo->rWtblTxConfig) == 0) ||
				    (txmode == TX_RATE_MODE_CCK) ||
				    (txmode == TX_RATE_MODE_OFDM)) ?
				    "BCC" : "LDPC");
#endif
		}

		if (!fgDumpAll)
			break;
	}

	return i4BytesWritten;
}

int32_t nicGetRxRateInfo(struct ADAPTER *prAdapter, IN char *pcCommand,
				 IN int i4TotalLen, IN uint8_t ucWlanIdx)
{
	uint32_t txmode, rate, frmode, sgi, nsts, ldpc, stbc, groupid, mu;
	int32_t i4BytesWritten = 0;
	uint32_t u4RxVector0 = 0, u4RxVector1 = 0;
	uint8_t ucStaIdx;
	struct CHIP_DBG_OPS *prChipDbg;

	if (wlanGetStaIdxByWlanIdx(prAdapter, ucWlanIdx, &ucStaIdx) ==
	    WLAN_STATUS_SUCCESS) {
		u4RxVector0 = prAdapter->arStaRec[ucStaIdx].u4RxVector0;
		u4RxVector1 = prAdapter->arStaRec[ucStaIdx].u4RxVector1;
		DBGLOG(REQ, LOUD, "****** RX Vector0 = 0x%08x ******\n",
		       u4RxVector0);
		DBGLOG(REQ, LOUD, "****** RX Vector1 = 0x%08x ******\n",
		       u4RxVector1);
	} else {
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s", "Last RX Rate", " = NOT SUPPORT");
		return i4BytesWritten;
	}

	prChipDbg = prAdapter->chip_info->prDebugOps;

	if (prChipDbg && prChipDbg->show_rx_rate_info) {
		i4BytesWritten = prChipDbg->show_rx_rate_info(
				prAdapter,
				pcCommand,
				i4TotalLen,
				ucStaIdx);
		return i4BytesWritten;
	}

	txmode = (u4RxVector0 & RX_VT_RX_MODE_MASK) >> RX_VT_RX_MODE_OFFSET;
	rate = (u4RxVector0 & RX_VT_RX_RATE_MASK) >> RX_VT_RX_RATE_OFFSET;
	frmode = (u4RxVector0 & RX_VT_FR_MODE_MASK) >> RX_VT_FR_MODE_OFFSET;
	nsts = ((u4RxVector1 & RX_VT_NSTS_MASK) >> RX_VT_NSTS_OFFSET);
	stbc = (u4RxVector0 & RX_VT_STBC_MASK) >> RX_VT_STBC_OFFSET;
	sgi = u4RxVector0 & RX_VT_SHORT_GI;
	ldpc = u4RxVector0 & RX_VT_LDPC;
	groupid = (u4RxVector1 & RX_VT_GROUP_ID_MASK) >> RX_VT_GROUP_ID_OFFSET;

	if (groupid && groupid != 63) {
		mu = 1;
	} else {
		mu = 0;
		nsts += 1;
	}

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten, "%-20s%s", "Last RX Rate", " = ");

	if (txmode == TX_RATE_MODE_CCK)
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "%s, ",
			rate < 4 ? HW_TX_RATE_CCK_STR[rate] :
				HW_TX_RATE_CCK_STR[4]);
	else if (txmode == TX_RATE_MODE_OFDM)
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "%s, ",
			nicHwRateOfdmStr(rate));
	else if ((txmode == TX_RATE_MODE_HTMIX) ||
		 (txmode == TX_RATE_MODE_HTGF))
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "MCS%d, ", rate);
	else
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "NSS%d_MCS%d, ",
			nsts, rate);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten, "%s, ",
		frmode < 4 ? HW_TX_RATE_BW[frmode] : HW_TX_RATE_BW[4]);

	if (txmode == TX_RATE_MODE_CCK)
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "%s, ",
			rate < 4 ? "LP" : "SP");
	else if (txmode == TX_RATE_MODE_OFDM)
		;
	else
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "%s, ",
			sgi == 0 ? "LGI" : "SGI");

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten, "%s", stbc == 0 ? "" : "STBC, ");

	if (mu) {
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "%s, %s, %s (%d)\n",
			(txmode < ENUM_TX_MODE_NUM ?
			 HW_TX_MODE_STR[txmode] : "N/A"),
			ldpc == 0 ? "BCC" : "LDPC", "MU", groupid);
	} else {
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "%s, %s\n",
			(txmode < ENUM_TX_MODE_NUM ?
			 HW_TX_MODE_STR[txmode] : "N/A"),
			ldpc == 0 ? "BCC" : "LDPC");
	}

	return i4BytesWritten;
}


