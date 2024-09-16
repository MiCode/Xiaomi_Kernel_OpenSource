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

