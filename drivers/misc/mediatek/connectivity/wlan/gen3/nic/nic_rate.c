/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

/*! \file   "nic_rate.c"
    \brief  This file contains the transmission rate handling routines.

    This file contains the transmission rate handling routines for setting up
    ACK/CTS Rate, Highest Tx Rate, Lowest Tx Rate, Initial Tx Rate and do
    conversion between Rate Set and Data Rates.
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

const UINT_16 au2RateCCKLong[CCK_RATE_NUM] = {
	RATE_CCK_1M_LONG,	/* RATE_1M_INDEX = 0 */
	RATE_CCK_2M_LONG,	/* RATE_2M_INDEX */
	RATE_CCK_5_5M_LONG,	/* RATE_5_5M_INDEX */
	RATE_CCK_11M_LONG	/* RATE_11M_INDEX */
};

const UINT_16 au2RateCCKShort[CCK_RATE_NUM] = {
	RATE_CCK_1M_LONG,	/* RATE_1M_INDEX = 0 */
	RATE_CCK_2M_SHORT,	/* RATE_2M_INDEX */
	RATE_CCK_5_5M_SHORT,	/* RATE_5_5M_INDEX */
	RATE_CCK_11M_SHORT	/* RATE_11M_INDEX */
};

const UINT_16 au2RateOFDM[OFDM_RATE_NUM] = {
	RATE_OFDM_6M,		/* RATE_6M_INDEX */
	RATE_OFDM_9M,		/* RATE_9M_INDEX */
	RATE_OFDM_12M,		/* RATE_12M_INDEX */
	RATE_OFDM_18M,		/* RATE_18M_INDEX */
	RATE_OFDM_24M,		/* RATE_24M_INDEX */
	RATE_OFDM_36M,		/* RATE_36M_INDEX */
	RATE_OFDM_48M,		/* RATE_48M_INDEX */
	RATE_OFDM_54M		/* RATE_54M_INDEX */
};

const UINT_16 au2RateHTMixed[HT_RATE_NUM] = {
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

const UINT_16 au2RateHTGreenField[HT_RATE_NUM] = {
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

const UINT_16 au2RateVHT[VHT_RATE_NUM] = {
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

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

WLAN_STATUS nicRateIndex2RateCode(IN UINT_8 ucPreambleOption, IN UINT_8 ucRateIndex, OUT PUINT_16 pu2RateCode)
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
