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
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/mgmt/rlm_domain.c#2
*/

/*! \file   "rlm_domain.c"
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

#include "rlm_txpwr_init.h"


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

/* The following country or domain shall be set from host driver.
 * And host driver should pass specified DOMAIN_INFO_ENTRY to MT6620 as
 * the channel list of being a STA to do scanning/searching AP or being an
 * AP to choose an adequate channel if auto-channel is set.
 */

/* Define mapping tables between country code and its channel set
 */
static const UINT_16 g_u2CountryGroup0[] = {
	COUNTRY_CODE_AO, COUNTRY_CODE_BZ, COUNTRY_CODE_BJ, COUNTRY_CODE_BT,
	COUNTRY_CODE_BO, COUNTRY_CODE_BI, COUNTRY_CODE_CM, COUNTRY_CODE_CF,
	COUNTRY_CODE_TD, COUNTRY_CODE_KM, COUNTRY_CODE_CD, COUNTRY_CODE_CG,
	COUNTRY_CODE_CI, COUNTRY_CODE_DJ, COUNTRY_CODE_GQ, COUNTRY_CODE_ER,
	COUNTRY_CODE_FJ, COUNTRY_CODE_GA, COUNTRY_CODE_GM, COUNTRY_CODE_GN,
	COUNTRY_CODE_GW, COUNTRY_CODE_RKS, COUNTRY_CODE_KG, COUNTRY_CODE_LY,
	COUNTRY_CODE_MG, COUNTRY_CODE_ML, COUNTRY_CODE_NR, COUNTRY_CODE_NC,
	COUNTRY_CODE_ST, COUNTRY_CODE_SC, COUNTRY_CODE_SL, COUNTRY_CODE_SB,
	COUNTRY_CODE_SO, COUNTRY_CODE_SR, COUNTRY_CODE_SZ, COUNTRY_CODE_TJ,
	COUNTRY_CODE_TG, COUNTRY_CODE_TO, COUNTRY_CODE_TM, COUNTRY_CODE_TV,
	COUNTRY_CODE_VU, COUNTRY_CODE_YE
};

static const UINT_16 g_u2CountryGroup1[] = {
	COUNTRY_CODE_AS, COUNTRY_CODE_AI, COUNTRY_CODE_BM, COUNTRY_CODE_CA,
	COUNTRY_CODE_KY, COUNTRY_CODE_GU, COUNTRY_CODE_FM, COUNTRY_CODE_PR,
	COUNTRY_CODE_US, COUNTRY_CODE_VI,

};

static const UINT_16 g_u2CountryGroup2[] = {
	COUNTRY_CODE_AR, COUNTRY_CODE_AU, COUNTRY_CODE_AZ, COUNTRY_CODE_BW,
	COUNTRY_CODE_KH, COUNTRY_CODE_CX, COUNTRY_CODE_CO, COUNTRY_CODE_CR,
	COUNTRY_CODE_EC, COUNTRY_CODE_GD, COUNTRY_CODE_GT, COUNTRY_CODE_HK,
	COUNTRY_CODE_KI, COUNTRY_CODE_LB, COUNTRY_CODE_LR, COUNTRY_CODE_MN,
	COUNTRY_CODE_AN, COUNTRY_CODE_NZ, COUNTRY_CODE_NI, COUNTRY_CODE_PW,
	COUNTRY_CODE_PY, COUNTRY_CODE_PE, COUNTRY_CODE_PH, COUNTRY_CODE_WS,
	COUNTRY_CODE_SG, COUNTRY_CODE_LK, COUNTRY_CODE_TH, COUNTRY_CODE_TT,
	COUNTRY_CODE_UY, COUNTRY_CODE_VN
};

static const UINT_16 g_u2CountryGroup3[] = {
	COUNTRY_CODE_AW, COUNTRY_CODE_LA, COUNTRY_CODE_SA, COUNTRY_CODE_AE,
	COUNTRY_CODE_UG
};

static const UINT_16 g_u2CountryGroup4[] = { COUNTRY_CODE_MM };

static const UINT_16 g_u2CountryGroup5[] = {
	COUNTRY_CODE_AL, COUNTRY_CODE_DZ, COUNTRY_CODE_AD, COUNTRY_CODE_AT,
	COUNTRY_CODE_BY, COUNTRY_CODE_BE, COUNTRY_CODE_BA, COUNTRY_CODE_VG,
	COUNTRY_CODE_BG, COUNTRY_CODE_CV, COUNTRY_CODE_HR, COUNTRY_CODE_CY,
	COUNTRY_CODE_CZ, COUNTRY_CODE_DK, COUNTRY_CODE_EE, COUNTRY_CODE_ET,
	COUNTRY_CODE_FI, COUNTRY_CODE_FR, COUNTRY_CODE_GF, COUNTRY_CODE_PF,
	COUNTRY_CODE_TF, COUNTRY_CODE_GE, COUNTRY_CODE_DE, COUNTRY_CODE_GH,
	COUNTRY_CODE_GR, COUNTRY_CODE_GP, COUNTRY_CODE_HU, COUNTRY_CODE_IS,
	COUNTRY_CODE_IQ, COUNTRY_CODE_IE, COUNTRY_CODE_IT, COUNTRY_CODE_KE,
	COUNTRY_CODE_LV, COUNTRY_CODE_LS, COUNTRY_CODE_LI, COUNTRY_CODE_LT,
	COUNTRY_CODE_LU, COUNTRY_CODE_MK, COUNTRY_CODE_MT, COUNTRY_CODE_MQ,
	COUNTRY_CODE_MR, COUNTRY_CODE_MU, COUNTRY_CODE_YT, COUNTRY_CODE_MD,
	COUNTRY_CODE_MC, COUNTRY_CODE_ME, COUNTRY_CODE_MS, COUNTRY_CODE_NL,
	COUNTRY_CODE_NO, COUNTRY_CODE_OM, COUNTRY_CODE_PL, COUNTRY_CODE_PT,
	COUNTRY_CODE_RE, COUNTRY_CODE_RO, COUNTRY_CODE_MF, COUNTRY_CODE_SM,
	COUNTRY_CODE_SN, COUNTRY_CODE_RS, COUNTRY_CODE_SK, COUNTRY_CODE_SI,
	COUNTRY_CODE_ZA, COUNTRY_CODE_ES, COUNTRY_CODE_SE, COUNTRY_CODE_CH,
	COUNTRY_CODE_TR, COUNTRY_CODE_TC, COUNTRY_CODE_GB, COUNTRY_CODE_VA,
	COUNTRY_CODE_EU
};
static const UINT_16 g_u2CountryGroup6[] = { COUNTRY_CODE_JP };

static const UINT_16 g_u2CountryGroup7[] = {
	COUNTRY_CODE_AM, COUNTRY_CODE_IL, COUNTRY_CODE_KW, COUNTRY_CODE_MA,
	COUNTRY_CODE_NE, COUNTRY_CODE_TN,
};
static const UINT_16 g_u2CountryGroup8[] = { COUNTRY_CODE_NP };
static const UINT_16 g_u2CountryGroup9[] = { COUNTRY_CODE_AF };

static const UINT_16 g_u2CountryGroup10[] = {
	COUNTRY_CODE_AG, COUNTRY_CODE_BS, COUNTRY_CODE_BH, COUNTRY_CODE_BB,
	COUNTRY_CODE_BN, COUNTRY_CODE_CL, COUNTRY_CODE_CN, COUNTRY_CODE_EG,
	COUNTRY_CODE_SV, COUNTRY_CODE_IN, COUNTRY_CODE_MY, COUNTRY_CODE_MV,
	COUNTRY_CODE_PA, COUNTRY_CODE_VE, COUNTRY_CODE_ZM,

};
static const UINT_16 g_u2CountryGroup11[] = { COUNTRY_CODE_JO, COUNTRY_CODE_PG };

static const UINT_16 g_u2CountryGroup12[] = {
	COUNTRY_CODE_BF, COUNTRY_CODE_GY, COUNTRY_CODE_HT, COUNTRY_CODE_HN,
	COUNTRY_CODE_JM, COUNTRY_CODE_MO, COUNTRY_CODE_MW, COUNTRY_CODE_PK,
	COUNTRY_CODE_QA, COUNTRY_CODE_RW, COUNTRY_CODE_KN, COUNTRY_CODE_TZ,

};
static const UINT_16 g_u2CountryGroup13[] = { COUNTRY_CODE_ID };
static const UINT_16 g_u2CountryGroup14[] = { COUNTRY_CODE_KR };
static const UINT_16 g_u2CountryGroup15[] = { COUNTRY_CODE_NG };

static const UINT_16 g_u2CountryGroup16[] = {
	COUNTRY_CODE_BD, COUNTRY_CODE_BR, COUNTRY_CODE_DM, COUNTRY_CODE_DO,
	COUNTRY_CODE_FK, COUNTRY_CODE_KZ, COUNTRY_CODE_MX, COUNTRY_CODE_MZ,
	COUNTRY_CODE_NA, COUNTRY_CODE_RU, COUNTRY_CODE_LC, COUNTRY_CODE_VC,
	COUNTRY_CODE_UA, COUNTRY_CODE_UZ, COUNTRY_CODE_ZW
};
static const UINT_16 g_u2CountryGroup17[] = { COUNTRY_CODE_MP };
static const UINT_16 g_u2CountryGroup18[] = { COUNTRY_CODE_TW };

static const UINT_16 g_u2CountryGroup19[] = {
	COUNTRY_CODE_CK, COUNTRY_CODE_CU, COUNTRY_CODE_TL, COUNTRY_CODE_FO,
	COUNTRY_CODE_GI, COUNTRY_CODE_GG, COUNTRY_CODE_IR, COUNTRY_CODE_IM,
	COUNTRY_CODE_JE, COUNTRY_CODE_KP, COUNTRY_CODE_MH, COUNTRY_CODE_NU,
	COUNTRY_CODE_NF, COUNTRY_CODE_PS, COUNTRY_CODE_PN, COUNTRY_CODE_PM,
	COUNTRY_CODE_SS, COUNTRY_CODE_SD, COUNTRY_CODE_SY
};

static const UINT_16 g_u2CountryGroup20[] = {
	COUNTRY_CODE_DF
	    /* When country code is not found, this domain info will be used.
	     * So mark all country codes to reduce search time. 20110908
	     */
};

#if (CFG_SUPPORT_SINGLE_SKU == 1)
mtk_regd_control g_mtk_regd_control = {
	.en = FALSE,
	.state = REGD_STATE_UNDEFINED
};

#if (CFG_SUPPORT_SINGLE_SKU_LOCAL_DB == 1)
const struct ieee80211_regdomain default_regdom_ww = {
	.n_reg_rules = 4,
	.alpha2 = "99",
	.reg_rules = {
	/* channels 1..13 */
	REG_RULE_LIGHT(2412-10, 2472+10, 40, 0),
	/* channels 14 */
	REG_RULE_LIGHT(2484-10, 2484+10, 20, 0),
	/* channel 36..64 */
	REG_RULE_LIGHT(5150-10, 5350+10, 80, 0),
	/* channel 100..165 */
	REG_RULE_LIGHT(5470-10, 5850+10, 80, 0),
	}
};
#endif

const char *gTx_Pwr_Limit_Section[TX_PWR_LIMIT_SECTION_NUM] = {
	"legacy", "ht20", "ht40", "vht20", "offset"
};

const u8 gTx_Pwr_Limit_Element_Num[TX_PWR_LIMIT_SECTION_NUM] = {
	7, 6, 7, 7, 5
};

const char *gTx_Pwr_Limit_Element[TX_PWR_LIMIT_SECTION_NUM][TX_PWR_LIMIT_ELEMENT_NUM] = {
	{"cck1_2", "cck_5_11", "ofdm6_9", "ofdm12_18", "ofdm24_36", "ofdm48", "ofdm54"},
	{"mcs0_8", "mcs1_2_9_10", "mcs3_4_11_12", "mcs5_13", "mcs6_14", "mcs7_15"},
	{"mcs0_8", "mcs1_2_9_10", "mcs3_4_11_12", "mcs5_13", "mcs6_14", "mcs7_15", "mcs32"},
	{"mcs0", "mcs1_2", "mcs3_4", "mcs5_6", "mcs7", "mcs8", "mcs9"},
	{"lg40", "lg80", "vht40", "vht80", "vht160nc"}
};

static const INT_8 gTx_Pwr_Limit_2g_Ch[] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
static const INT_8 gTx_Pwr_Limit_5g_Ch[] = {
	36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62, 64, 100, 102,
	104, 106, 108, 110, 112, 114, 116, 118, 120, 122, 124, 126, 128, 132,
	134, 136, 138, 140, 142, 144, 149, 151, 153, 155, 157, 159, 161, 165};

#define TX_PWR_LIMIT_2G_CH_NUM (ARRAY_SIZE(gTx_Pwr_Limit_2g_Ch))
#define TX_PWR_LIMIT_5G_CH_NUM (ARRAY_SIZE(gTx_Pwr_Limit_5g_Ch))


#endif

DOMAIN_INFO_ENTRY arSupportedRegDomains[] = {
	{
	 (PUINT_16) g_u2CountryGroup0, sizeof(g_u2CountryGroup0) / 2,
	 {
	  {81, BAND_2G4, CHNL_SPAN_5, 1, 13, FALSE}
	  ,			/* CH_SET_2G4_1_13 */

	  {115, BAND_NULL, 0, 0, 0, FALSE}
	  ,			/* CH_SET_UNII_LOW_NA */
	  {118, BAND_NULL, 0, 0, 0, FALSE}
	  ,			/* CH_SET_UNII_MID_NA */
	  {121, BAND_NULL, 0, 0, 0, FALSE}
	  ,			/* CH_SET_UNII_WW_NA */
	  {125, BAND_NULL, 0, 0, 0, FALSE}
	  ,			/* CH_SET_UNII_UPPER_NA */
	  {0, BAND_NULL, 0, 0, 0, FALSE}
	  }
	 }
	,
	{
	 (PUINT_16) g_u2CountryGroup1, sizeof(g_u2CountryGroup1) / 2,
	 {
	  {81, BAND_2G4, CHNL_SPAN_5, 1, 11, FALSE}
	  ,			/* CH_SET_2G4_1_11 */

	  {115, BAND_5G, CHNL_SPAN_20, 36, 4, FALSE}
	  ,			/* CH_SET_UNII_LOW_36_48 */
	  {118, BAND_5G, CHNL_SPAN_20, 52, 4, TRUE}
	  ,			/* CH_SET_UNII_MID_52_64 */
	  {121, BAND_5G, CHNL_SPAN_20, 100, 12, TRUE}
	  ,			/* CH_SET_UNII_WW_100_144 */
	  {125, BAND_5G, CHNL_SPAN_20, 149, 5, FALSE}
	  ,			/* CH_SET_UNII_UPPER_149_165 */
	  {0, BAND_NULL, 0, 0, 0, FALSE}
	  }
	 }
	,
	{
	 (PUINT_16) g_u2CountryGroup2, sizeof(g_u2CountryGroup2) / 2,
	 {
	  {81, BAND_2G4, CHNL_SPAN_5, 1, 13, FALSE}
	  ,			/* CH_SET_2G4_1_13 */

	  {115, BAND_5G, CHNL_SPAN_20, 36, 4, FALSE}
	  ,			/* CH_SET_UNII_LOW_36_48 */
	  {118, BAND_5G, CHNL_SPAN_20, 52, 4, TRUE}
	  ,			/* CH_SET_UNII_MID_52_64 */
	  {121, BAND_5G, CHNL_SPAN_20, 100, 12, TRUE}
	  ,			/* CH_SET_UNII_WW_100_144 */
	  {125, BAND_5G, CHNL_SPAN_20, 149, 5, FALSE}
	  ,			/* CH_SET_UNII_UPPER_149_165 */
	  {0, BAND_NULL, 0, 0, 0, FALSE}
	  }
	 }
	,
	{
	 (PUINT_16) g_u2CountryGroup3, sizeof(g_u2CountryGroup3) / 2,
	 {
	  {81, BAND_2G4, CHNL_SPAN_5, 1, 13, FALSE}
	  ,			/* CH_SET_2G4_1_13 */

	  {115, BAND_5G, CHNL_SPAN_20, 36, 4, FALSE}
	  ,			/* CH_SET_UNII_LOW_36_48 */
	  {118, BAND_5G, CHNL_SPAN_20, 52, 4, TRUE}
	  ,			/* CH_SET_UNII_MID_52_64 */
	  {121, BAND_5G, CHNL_SPAN_20, 100, 12, TRUE}
	  ,			/* CH_SET_UNII_WW_100_144 */
	  {125, BAND_5G, CHNL_SPAN_20, 149, 4, FALSE}
	  ,			/* CH_SET_UNII_UPPER_149_161 */
	  {0, BAND_NULL, 0, 0, 0, FALSE}
	  }
	 }
	,
	{
	 (PUINT_16) g_u2CountryGroup4, sizeof(g_u2CountryGroup4) / 2,
	 {
	  {81, BAND_2G4, CHNL_SPAN_5, 1, 13, FALSE}
	  ,			/* CH_SET_2G4_1_13 */

	  {115, BAND_5G, CHNL_SPAN_20, 36, 4, FALSE}
	  ,			/* CH_SET_UNII_LOW_36_48 */
	  {118, BAND_5G, CHNL_SPAN_20, 52, 4, TRUE}
	  ,			/* CH_SET_UNII_MID_52_64 */
	  {121, BAND_5G, CHNL_SPAN_20, 100, 12, TRUE}
	  ,			/* CH_SET_UNII_WW_100_144 */
	  {125, BAND_NULL, 0, 0, 0, FALSE}
	  ,			/* CH_SET_UNII_UPPER_NA */
	  {0, BAND_NULL, 0, 0, 0, FALSE}
	  }
	 }
	,
	{
	 (PUINT_16) g_u2CountryGroup5, sizeof(g_u2CountryGroup5) / 2,
	 {
	  {81, BAND_2G4, CHNL_SPAN_5, 1, 13, FALSE}
	  ,			/* CH_SET_2G4_1_13 */

	  {115, BAND_5G, CHNL_SPAN_20, 36, 4, FALSE}
	  ,			/* CH_SET_UNII_LOW_36_48 */
	  {118, BAND_5G, CHNL_SPAN_20, 52, 4, TRUE}
	  ,			/* CH_SET_UNII_MID_52_64 */
	  {121, BAND_5G, CHNL_SPAN_20, 100, 11, TRUE}
	  ,			/* CH_SET_UNII_WW_100_140 */
	  {125, BAND_NULL, 0, 0, 0, FALSE}
	  ,			/* CH_SET_UNII_UPPER_NA */
	  {0, BAND_NULL, 0, 0, 0, FALSE}
	  }
	 }
	,
	{
	 (PUINT_16) g_u2CountryGroup6, sizeof(g_u2CountryGroup6) / 2,
	 {
	  {81, BAND_2G4, CHNL_SPAN_5, 1, 13, FALSE}
	  ,			/* CH_SET_2G4_1_13 */
	  {82, BAND_2G4, CHNL_SPAN_5, 14, 1, FALSE}
	  ,			/* CH_SET_2G4_14_14 */
	  {115, BAND_5G, CHNL_SPAN_20, 36, 4, FALSE}
	  ,			/* CH_SET_UNII_LOW_36_48 */
	  {118, BAND_5G, CHNL_SPAN_20, 52, 4, TRUE}
	  ,			/* CH_SET_UNII_MID_52_64 */
	  {121, BAND_5G, CHNL_SPAN_20, 100, 11, TRUE}
	  ,			/* CH_SET_UNII_WW_100_140 */
	  {125, BAND_NULL, 0, 0, 0, FALSE}
	  ,			/* CH_SET_UNII_UPPER_NA */
	  }
	 }
	,
	{
	 (PUINT_16) g_u2CountryGroup7, sizeof(g_u2CountryGroup7) / 2,
	 {
	  {81, BAND_2G4, CHNL_SPAN_5, 1, 13, FALSE}
	  ,			/* CH_SET_2G4_1_13 */

	  {115, BAND_5G, CHNL_SPAN_20, 36, 4, FALSE}
	  ,			/* CH_SET_UNII_LOW_36_48 */
	  {118, BAND_5G, CHNL_SPAN_20, 52, 4, TRUE}
	  ,			/* CH_SET_UNII_MID_52_64 */
	  {121, BAND_NULL, 0, 0, 0, FALSE}
	  ,			/* CH_SET_UNII_WW_NA */
	  {125, BAND_NULL, 0, 0, 0, FALSE}
	  ,			/* CH_SET_UNII_UPPER_NA */
	  {0, BAND_NULL, 0, 0, 0, FALSE}
	  }
	 }
	,
	{
	 (PUINT_16) g_u2CountryGroup8, sizeof(g_u2CountryGroup8) / 2,
	 {
	  {81, BAND_2G4, CHNL_SPAN_5, 1, 13, FALSE}
	  ,			/* CH_SET_2G4_1_13 */

	  {115, BAND_5G, CHNL_SPAN_20, 36, 4, FALSE}
	  ,			/* CH_SET_UNII_LOW_36_48 */
	  {118, BAND_5G, CHNL_SPAN_20, 52, 4, TRUE}
	  ,			/* CH_SET_UNII_MID_52_64 */
	  {121, BAND_NULL, 0, 0, 0, FALSE}
	  ,			/* CH_SET_UNII_WW_NA */
	  {125, BAND_5G, CHNL_SPAN_20, 149, 4, FALSE}
	  ,			/* CH_SET_UNII_UPPER_149_161 */
	  {0, BAND_NULL, 0, 0, 0, FALSE}
	  }
	 }
	,
	{
	 (PUINT_16) g_u2CountryGroup9, sizeof(g_u2CountryGroup9) / 2,
	 {
	  {81, BAND_2G4, CHNL_SPAN_5, 1, 13, FALSE}
	  ,			/* CH_SET_2G4_1_13 */

	  {115, BAND_5G, CHNL_SPAN_20, 36, 4, FALSE}
	  ,			/* CH_SET_UNII_LOW_36_48 */
	  {118, BAND_NULL, 0, 0, 0, FALSE}
	  ,			/* CH_SET_UNII_MID_NA */
	  {121, BAND_NULL, 0, 0, 0, FALSE}
	  ,			/* CH_SET_UNII_WW_NA */
	  {125, BAND_NULL, 0, 0, 0, FALSE}
	  ,			/* CH_SET_UNII_UPPER_NA */
	  {0, BAND_NULL, 0, 0, 0, FALSE}
	  }
	 }
	,
	{
	 (PUINT_16) g_u2CountryGroup10, sizeof(g_u2CountryGroup10) / 2,
	 {
	  {81, BAND_2G4, CHNL_SPAN_5, 1, 13, FALSE}
	  ,			/* CH_SET_2G4_1_13 */

	  {115, BAND_5G, CHNL_SPAN_20, 36, 4, FALSE}
	  ,			/* CH_SET_UNII_LOW_36_48 */
	  {118, BAND_5G, CHNL_SPAN_20, 52, 4, TRUE}
	  ,			/* CH_SET_UNII_MID_52_64 */
	  {121, BAND_NULL, 0, 0, 0, FALSE}
	  ,			/* CH_SET_UNII_WW_NA */
	  {125, BAND_5G, CHNL_SPAN_20, 149, 5, FALSE}
	  ,			/* CH_SET_UNII_UPPER_149_165 */
	  {0, BAND_NULL, 0, 0, 0, FALSE}
	  }
	 }
	,
	{
	 (PUINT_16) g_u2CountryGroup11, sizeof(g_u2CountryGroup11) / 2,
	 {
	  {81, BAND_2G4, CHNL_SPAN_5, 1, 13, FALSE}
	  ,			/* CH_SET_2G4_1_13 */

	  {115, BAND_5G, CHNL_SPAN_20, 36, 4, FALSE}
	  ,			/* CH_SET_UNII_LOW_36_48 */
	  {118, BAND_NULL, 0, 0, 0, FALSE}
	  ,			/* CH_SET_UNII_MID_NA */
	  {121, BAND_NULL, 0, 0, 0, FALSE}
	  ,			/* CH_SET_UNII_WW_NA */
	  {125, BAND_5G, CHNL_SPAN_20, 149, 5, FALSE}
	  ,			/* CH_SET_UNII_UPPER_149_165 */
	  {0, BAND_NULL, 0, 0, 0, FALSE}
	  }
	 }
	,
	{
	 (PUINT_16) g_u2CountryGroup12, sizeof(g_u2CountryGroup12) / 2,
	 {
	  {81, BAND_2G4, CHNL_SPAN_5, 1, 13, FALSE}
	  ,			/* CH_SET_2G4_1_13 */

	  {115, BAND_NULL, 0, 0, 0, FALSE}
	  ,			/* CH_SET_UNII_LOW_NA */
	  {118, BAND_NULL, 0, 0, 0, FALSE}
	  ,			/* CH_SET_UNII_MID_NA */
	  {121, BAND_NULL, 0, 0, 0, FALSE}
	  ,			/* CH_SET_UNII_WW_NA */
	  {125, BAND_5G, CHNL_SPAN_20, 149, 5, FALSE}
	  ,			/* CH_SET_UNII_UPPER_149_165 */
	  {0, BAND_NULL, 0, 0, 0, FALSE}
	  }
	 }
	,
	{
	 (PUINT_16) g_u2CountryGroup13, sizeof(g_u2CountryGroup13) / 2,
	 {
	  {81, BAND_2G4, CHNL_SPAN_5, 1, 13, FALSE}
	  ,			/* CH_SET_2G4_1_13 */

	  {115, BAND_NULL, 0, 0, 0, FALSE}
	  ,			/* CH_SET_UNII_LOW_NA */
	  {118, BAND_NULL, 0, 0, 0, FALSE}
	  ,			/* CH_SET_UNII_MID_NA */
	  {121, BAND_NULL, 0, 0, 0, FALSE}
	  ,			/* CH_SET_UNII_WW_NA */
	  {125, BAND_5G, CHNL_SPAN_20, 149, 4, FALSE}
	  ,			/* CH_SET_UNII_UPPER_149_161 */
	  {0, BAND_NULL, 0, 0, 0, FALSE}
	  }
	 }
	,
	{
	 (PUINT_16) g_u2CountryGroup14, sizeof(g_u2CountryGroup14) / 2,
	 {
	  {81, BAND_2G4, CHNL_SPAN_5, 1, 13, FALSE}
	  ,			/* CH_SET_2G4_1_13 */

	  {115, BAND_5G, CHNL_SPAN_20, 36, 4, FALSE}
	  ,			/* CH_SET_UNII_LOW_36_48 */
	  {118, BAND_5G, CHNL_SPAN_20, 52, 4, FALSE}
	  ,			/* CH_SET_UNII_MID_52_64 */
	  {121, BAND_5G, CHNL_SPAN_20, 100, 8, FALSE}
	  ,			/* CH_SET_UNII_WW_100_128 */
	  {125, BAND_5G, CHNL_SPAN_20, 149, 4, FALSE}
	  ,			/* CH_SET_UNII_UPPER_149_161 */
	  {0, BAND_NULL, 0, 0, 0, FALSE}
	  }
	 }
	,
	{
	 (PUINT_16) g_u2CountryGroup15, sizeof(g_u2CountryGroup15) / 2,
	 {
	  {81, BAND_2G4, CHNL_SPAN_5, 1, 13, FALSE}
	  ,			/* CH_SET_2G4_1_13 */

	  {115, BAND_NULL, 0, 0, 0, FALSE}
	  ,			/* CH_SET_UNII_LOW_36_48 */
	  {118, BAND_5G, CHNL_SPAN_20, 52, 4, TRUE}
	  ,			/* CH_SET_UNII_MID_52_64 */
	  {121, BAND_5G, CHNL_SPAN_20, 100, 11, TRUE}
	  ,			/* CH_SET_UNII_WW_100_140 */
	  {125, BAND_5G, CHNL_SPAN_20, 149, 5, FALSE}
	  ,			/* CH_SET_UNII_UPPER_149_165 */
	  {0, BAND_NULL, 0, 0, 0, FALSE}
	  }
	 }
	,
	{
	 (PUINT_16) g_u2CountryGroup16, sizeof(g_u2CountryGroup16) / 2,
	 {
	  {81, BAND_2G4, CHNL_SPAN_5, 1, 13, FALSE}
	  ,			/* CH_SET_2G4_1_13 */

	  {115, BAND_5G, CHNL_SPAN_20, 36, 4, FALSE}
	  ,			/* CH_SET_UNII_LOW_36_48 */
	  {118, BAND_5G, CHNL_SPAN_20, 52, 4, FALSE}
	  ,			/* CH_SET_UNII_MID_52_64 */
	  {121, BAND_5G, CHNL_SPAN_20, 100, 11, TRUE}
	  ,			/* CH_SET_UNII_WW_100_140 */
	  {125, BAND_5G, CHNL_SPAN_20, 149, 5, FALSE}
	  ,			/* CH_SET_UNII_UPPER_149_165 */
	  {0, BAND_NULL, 0, 0, 0, FALSE}
	  }
	 }
	,
	{
	 (PUINT_16) g_u2CountryGroup17, sizeof(g_u2CountryGroup17) / 2,
	 {
	  {81, BAND_2G4, CHNL_SPAN_5, 1, 11, FALSE}
	  ,			/* CH_SET_2G4_1_11 */

	  {115, BAND_5G, CHNL_SPAN_20, 36, 4, FALSE}
	  ,			/* CH_SET_UNII_LOW_36_48 */
	  {118, BAND_5G, CHNL_SPAN_20, 52, 4, TRUE}
	  ,			/* CH_SET_UNII_MID_52_64 */
	  {121, BAND_5G, CHNL_SPAN_20, 100, 11, TRUE}
	  ,			/* CH_SET_UNII_WW_100_140 */
	  {125, BAND_5G, CHNL_SPAN_20, 149, 5, FALSE}
	  ,			/* CH_SET_UNII_UPPER_149_165 */
	  {0, BAND_NULL, 0, 0, 0, FALSE}
	  }
	 }
	,
	{
	 (PUINT_16) g_u2CountryGroup18, sizeof(g_u2CountryGroup18) / 2,
	 {
	  {81, BAND_2G4, CHNL_SPAN_5, 1, 11, FALSE}
	  ,			/* CH_SET_2G4_1_11 */

	  {115, BAND_5G, CHNL_SPAN_20, 36, 4, FALSE}
	  ,			/* CH_SET_UNII_LOW_36_48 */
	  {118, BAND_5G, CHNL_SPAN_20, 52, 4, FALSE}
	  ,			/* CH_SET_UNII_MID_52_64 */
	  {121, BAND_5G, CHNL_SPAN_20, 100, 11, FALSE}
	  ,			/* CH_SET_UNII_WW_100_140 */
	  {125, BAND_5G, CHNL_SPAN_20, 149, 5, FALSE}
	  ,			/* CH_SET_UNII_UPPER_149_165 */
	  {0, BAND_NULL, 0, 0, 0, FALSE}
	  }
	 }
	,
	{
	 (PUINT_16) g_u2CountryGroup19, sizeof(g_u2CountryGroup19) / 2,
	 {
	  {81, BAND_2G4, CHNL_SPAN_5, 1, 13, FALSE}
	  ,			/* CH_SET_2G4_1_13 */

	  {115, BAND_5G, CHNL_SPAN_20, 36, 4, FALSE}
	  ,			/* CH_SET_UNII_LOW_36_48 */
	  {118, BAND_5G, CHNL_SPAN_20, 52, 4, TRUE}
	  ,			/* CH_SET_UNII_MID_52_64 */
	  {121, BAND_5G, CHNL_SPAN_20, 100, 12, TRUE}
	  ,			/* CH_SET_UNII_WW_100_144 */
	  {125, BAND_5G, CHNL_SPAN_20, 149, 5, FALSE}
	  ,			/* CH_SET_UNII_UPPER_149_165 */
	  {0, BAND_NULL, 0, 0, 0, FALSE}
	  }
	 }
	,
	{
	 /* Note: The final one is for Europe union now.(Default group if no matched country code) */
	 (PUINT_16) g_u2CountryGroup20, sizeof(g_u2CountryGroup20) / 2,
	 {
	  {81, BAND_2G4, CHNL_SPAN_5, 1, 13, FALSE}
	  ,			/* CH_SET_2G4_1_13 */

	  {115, BAND_5G, CHNL_SPAN_20, 36, 4, FALSE}
	  ,			/* CH_SET_UNII_LOW_36_48 */
	  {118, BAND_5G, CHNL_SPAN_20, 52, 4, TRUE}
	  ,			/* CH_SET_UNII_MID_52_64 */
	  {121, BAND_5G, CHNL_SPAN_20, 100, 12, TRUE}
	  ,			/* CH_SET_UNII_WW_100_144 */
	  {125, BAND_5G, CHNL_SPAN_20, 149, 7, FALSE}
	  ,			/* CH_SET_UNII_UPPER_149_173 */
	  {0, BAND_NULL, 0, 0, 0, FALSE}
	  }
	 }
};

#define REG_DOMAIN_PASSIVE_DEF_IDX	1

static const UINT_16 g_u2CountryGroup0_Passive[] = {
	COUNTRY_CODE_TW
};

DOMAIN_INFO_ENTRY arSupportedRegDomains_Passive[] = {
	{
	 (PUINT_16) g_u2CountryGroup0_Passive, sizeof(g_u2CountryGroup0_Passive) / 2,
	 {
	  {81, BAND_2G4, CHNL_SPAN_5, 1, 0, 0}
	  ,			/* CH_SET_2G4_1_14_NA */
	  {82, BAND_2G4, CHNL_SPAN_5, 14, 0, 0}
	  ,

	  {115, BAND_5G, CHNL_SPAN_20, 36, 4, 0}
	  ,			/* CH_SET_UNII_LOW_36_48 */
	  {118, BAND_5G, CHNL_SPAN_20, 52, 4, 0}
	  ,			/* CH_SET_UNII_MID_52_64 */
	  {121, BAND_5G, CHNL_SPAN_20, 100, 11, 0}
	  ,			/* CH_SET_UNII_WW_100_140 */
	  {125, BAND_5G, CHNL_SPAN_20, 149, 0, 0}
	  ,			/* CH_SET_UNII_UPPER_NA */
	  }
	 }
	,

	{
	 /* default passive channel table is empty */
	 NULL, 0,
	 {
	  {81, BAND_2G4, CHNL_SPAN_5, 1, 0, 0}
	  ,			/* CH_SET_2G4_1_14_NA */
	  {82, BAND_2G4, CHNL_SPAN_5, 14, 0, 0}
	  ,

	  {115, BAND_5G, CHNL_SPAN_20, 36, 0, 0}
	  ,			/* CH_SET_UNII_LOW_NA */
	  {118, BAND_5G, CHNL_SPAN_20, 52, 4, 0}
	  ,			/* CH_SET_UNII_MID_52_64 */
	  {121, BAND_5G, CHNL_SPAN_20, 100, 12, 0}
	  ,			/* CH_SET_UNII_WW_100_144 */
	  {125, BAND_5G, CHNL_SPAN_20, 149, 0, 0}
	  ,			/* CH_SET_UNII_UPPER_NA */
	  }
	 }

};

#define REG_DOMAIN_PASSIVE_GROUP_NUM \
		(sizeof(arSupportedRegDomains_Passive) / sizeof(DOMAIN_INFO_ENTRY))


SUBBAND_CHANNEL_T g_rRlmSubBand[] = {

	{BAND_2G4_LOWER_BOUND, BAND_2G4_UPPER_BOUND, 1, 0}
	,			/* 2.4G */
	{UNII1_LOWER_BOUND, UNII1_UPPER_BOUND, 2, 0}
	,			/* ch36,38,40,..,48 */
	{UNII2A_LOWER_BOUND, UNII2A_UPPER_BOUND, 2, 0}
	,			/* ch52,54,56,..,64 */
	{UNII2C_LOWER_BOUND, UNII2C_UPPER_BOUND, 2, 0}
	,			/* ch100,102,104,...,144 */
	{UNII3_LOWER_BOUND, UNII3_UPPER_BOUND, 2, 0}	/* ch149,151,153,....,173 */

};

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

/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in/out]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
P_DOMAIN_INFO_ENTRY rlmDomainGetDomainInfo(P_ADAPTER_T prAdapter)
{
#define REG_DOMAIN_DEF_IDX          20	/* EU (Europe Union) */
#define REG_DOMAIN_GROUP_NUM \
	(sizeof(arSupportedRegDomains) / sizeof(DOMAIN_INFO_ENTRY))

	P_DOMAIN_INFO_ENTRY prDomainInfo;
	P_REG_INFO_T prRegInfo;
	UINT_16 u2TargetCountryCode;
	UINT_16 i, j;

	ASSERT(prAdapter);

	if (prAdapter->prDomainInfo)
		return prAdapter->prDomainInfo;

	prRegInfo = &prAdapter->prGlueInfo->rRegInfo;

	DBGLOG(RLM, TRACE, "eRegChannelListMap=%d, u2CountryCode=0x%04x\n",
			   prRegInfo->eRegChannelListMap,
			   prAdapter->rWifiVar.rConnSettings.u2CountryCode);

	/*
	* Domain info can be specified by given idx of arSupportedRegDomains table,
	* customized, or searched by country code,
	* only one is set among these three methods in NVRAM.
	*/
	if (prRegInfo->eRegChannelListMap == REG_CH_MAP_TBL_IDX &&
	    prRegInfo->ucRegChannelListIndex < REG_DOMAIN_GROUP_NUM) {
		/* by given table idx */
		DBGLOG(RLM, TRACE, "ucRegChannelListIndex=%d\n", prRegInfo->ucRegChannelListIndex);
		prDomainInfo = &arSupportedRegDomains[prRegInfo->ucRegChannelListIndex];
	} else if (prRegInfo->eRegChannelListMap == REG_CH_MAP_CUSTOMIZED) {
		/* by customized */
		prDomainInfo = &prRegInfo->rDomainInfo;
	} else {
		/* by country code */
		u2TargetCountryCode = prAdapter->rWifiVar.rConnSettings.u2CountryCode;

		for (i = 0; i < REG_DOMAIN_GROUP_NUM; i++) {
			prDomainInfo = &arSupportedRegDomains[i];

			if ((prDomainInfo->u4CountryNum && prDomainInfo->pu2CountryGroup) ||
			    prDomainInfo->u4CountryNum == 0) {
				for (j = 0; j < prDomainInfo->u4CountryNum; j++) {
					if (prDomainInfo->pu2CountryGroup[j] == u2TargetCountryCode)
						break;
				}
				if (j < prDomainInfo->u4CountryNum)
					break;	/* Found */
			}
		}

		/* If no matched country code, use the default regulatory domain */
		if (i >= REG_DOMAIN_GROUP_NUM) {
			DBGLOG(RLM, INFO, "No matched country code, use the default regulatory domain\n");
			prDomainInfo = &arSupportedRegDomains[REG_DOMAIN_DEF_IDX];
		}
	}

	prAdapter->prDomainInfo = prDomainInfo;
	return prDomainInfo;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in/out] The input variable pointed by pucNumOfChannel is the max
*                arrary size. The return value indciates meaning list size.
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID
rlmDomainGetChnlList_V2(P_ADAPTER_T prAdapter,
		     ENUM_BAND_T eSpecificBand, BOOLEAN fgNoDfs,
		     UINT_8 ucMaxChannelNum, PUINT_8 pucNumOfChannel, P_RF_CHANNEL_INFO_T paucChannelList)
{
#if (CFG_SUPPORT_SINGLE_SKU == 1)
	ENUM_BAND_T band;
	UINT_8 max_count, i, ucNum;
	struct channel *prCh;

	if (eSpecificBand == BAND_2G4) {
		i = 0;
		max_count = rlmDomainGetActiveChannelCount(KAL_BAND_2GHZ);
	} else if (eSpecificBand == BAND_5G) {
		i = rlmDomainGetActiveChannelCount(KAL_BAND_2GHZ);
		max_count = rlmDomainGetActiveChannelCount(KAL_BAND_5GHZ) +
			rlmDomainGetActiveChannelCount(KAL_BAND_2GHZ);
	} else {
		i = 0;
		max_count = rlmDomainGetActiveChannelCount(KAL_BAND_5GHZ) +
			rlmDomainGetActiveChannelCount(KAL_BAND_2GHZ);
	}

	ucNum = 0;
	for (; i < max_count; i++) {
		prCh = rlmDomainGetActiveChannels() + i;
		if (fgNoDfs && (prCh->flags & IEEE80211_CHAN_RADAR))
			continue; /*not match*/

		if (i < rlmDomainGetActiveChannelCount(KAL_BAND_2GHZ))
			band = BAND_2G4;
		else
			band = BAND_5G;

		paucChannelList[ucNum].eBand = band;
		paucChannelList[ucNum].ucChannelNum = prCh->chNum;

		ucNum++;
		if (ucMaxChannelNum == ucNum)
			break;
	}

	*pucNumOfChannel = ucNum;
#else
	*pucNumOfChannel = 0;
#endif
}


VOID
rlmDomainGetChnlList(P_ADAPTER_T prAdapter,
		     ENUM_BAND_T eSpecificBand, BOOLEAN fgNoDfs,
		     UINT_8 ucMaxChannelNum, PUINT_8 pucNumOfChannel, P_RF_CHANNEL_INFO_T paucChannelList)
{
	UINT_8 i, j, ucNum;
	P_DOMAIN_SUBBAND_INFO prSubband;
	P_DOMAIN_INFO_ENTRY prDomainInfo;

	ASSERT(prAdapter);
	ASSERT(paucChannelList);
	ASSERT(pucNumOfChannel);

	if (regd_is_single_sku_en())
		return rlmDomainGetChnlList_V2(prAdapter, eSpecificBand,
										fgNoDfs, ucMaxChannelNum,
										pucNumOfChannel, paucChannelList);

	/* If no matched country code, the final one will be used */
	prDomainInfo = rlmDomainGetDomainInfo(prAdapter);
	ASSERT(prDomainInfo);

	ucNum = 0;
	for (i = 0; i < MAX_SUBBAND_NUM; i++) {
		prSubband = &prDomainInfo->rSubBand[i];

		if (prSubband->ucBand == BAND_NULL || prSubband->ucBand >= BAND_NUM ||
		    (prSubband->ucBand == BAND_5G && !prAdapter->fgEnable5GBand))
			continue;

		/*repoert to upper layer only non-DFS channel for ap mode usage*/
		if (fgNoDfs == TRUE && prSubband->fgDfs == TRUE)
			continue;

		if (eSpecificBand == BAND_NULL || prSubband->ucBand == eSpecificBand) {
			for (j = 0; j < prSubband->ucNumChannels; j++) {
				if (ucNum >= ucMaxChannelNum)
					break;
				paucChannelList[ucNum].eBand = prSubband->ucBand;
				paucChannelList[ucNum].ucChannelNum =
				    prSubband->ucFirstChannelNum + j * prSubband->ucChannelSpan;
				ucNum++;
			}
		}
	}

	*pucNumOfChannel = ucNum;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief
*
* @param[in]
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID rlmDomainSendCmd(P_ADAPTER_T prAdapter, BOOLEAN fgIsOid)
{
	if (!regd_is_single_sku_en())
		rlmDomainSendPassiveScanInfoCmd(prAdapter, fgIsOid);
	rlmDomainSendDomainInfoCmd(prAdapter, fgIsOid);
#if CFG_SUPPORT_PWR_LIMIT_COUNTRY
	rlmDomainSendPwrLimitCmd(prAdapter);
#endif


}


VOID rlmDomainSendDomainInfoCmd_V2(P_ADAPTER_T prAdapter, BOOLEAN fgIsOid)
{
#if (CFG_SUPPORT_SINGLE_SKU == 1)
	u8 max_channel_count = 0;
	u32 buff_max_size, buff_valid_size;
	P_CMD_SET_DOMAIN_INFO_V2_T prCmd;
	struct acctive_channel_list *prChs;
	struct wiphy *pWiphy;


	pWiphy = priv_to_wiphy(prAdapter->prGlueInfo);
	if (pWiphy->bands[KAL_BAND_2GHZ] != NULL)
		max_channel_count += pWiphy->bands[KAL_BAND_2GHZ]->n_channels;
	if (pWiphy->bands[KAL_BAND_5GHZ] != NULL)
		max_channel_count += pWiphy->bands[KAL_BAND_5GHZ]->n_channels;

	if (max_channel_count == 0) {
		DBGLOG(RLM, ERROR, "%s, invalid channel count.\n", __func__);
		ASSERT(0);
	}


	buff_max_size = sizeof(CMD_SET_DOMAIN_INFO_V2_T) +
					max_channel_count * sizeof(struct channel);

	prCmd = cnmMemAlloc(prAdapter, RAM_TYPE_BUF, buff_max_size);
	prChs = &(prCmd->active_chs);



	/*
	 * Fill in the active channels
	 */
	rlmExtractChannelInfo(max_channel_count, prChs);

	prCmd->u4CountryCode = rlmDomainGetCountryCode();
	prCmd->uc2G4Bandwidth = prAdapter->rWifiVar.rConnSettings.uc2G4BandwidthMode;
	prCmd->uc5GBandwidth = prAdapter->rWifiVar.rConnSettings.uc5GBandwidthMode;
	prCmd->aucReserved[0] = 0;
	prCmd->aucReserved[1] = 0;

	buff_valid_size = sizeof(CMD_SET_DOMAIN_INFO_V2_T) +
					(prChs->n_channels_2g + prChs->n_channels_5g) *
					sizeof(struct channel);

	DBGLOG(RLM, INFO, "rlmDomainSendDomainInfoCmd_V2(), buff_valid_size = 0x%x\n", buff_valid_size);


	/* Set domain info to chip */
	wlanSendSetQueryCmd(prAdapter, /* prAdapter */
				      CMD_ID_SET_DOMAIN_INFO, /* ucCID */
				      TRUE,	/* fgSetQuery */
				      FALSE, /* fgNeedResp */
				      fgIsOid, /* fgIsOid */
				      NULL, /* pfCmdDoneHandler */
				      NULL, /* pfCmdTimeoutHandler */
					  buff_valid_size,
				      (PUINT_8) prCmd, /* pucInfoBuffer */
				      NULL,	/* pvSetQueryBuffer */
				      0	/* u4SetQueryBufferLen */
	    );

	cnmMemFree(prAdapter, prCmd);
#endif
}


VOID rlmDomainSendDomainInfoCmd(P_ADAPTER_T prAdapter, BOOLEAN fgIsOid)
{
	P_DOMAIN_INFO_ENTRY prDomainInfo;
	P_CMD_SET_DOMAIN_INFO_T prCmd;
	P_DOMAIN_SUBBAND_INFO prSubBand;
	UINT_8 i;

	if (regd_is_single_sku_en())
		return rlmDomainSendDomainInfoCmd_V2(prAdapter, fgIsOid);


	prDomainInfo = rlmDomainGetDomainInfo(prAdapter);
	ASSERT(prDomainInfo);

	prCmd = cnmMemAlloc(prAdapter, RAM_TYPE_BUF, sizeof(CMD_SET_DOMAIN_INFO_T));
	if (!prCmd) {
		DBGLOG(RLM, ERROR, "Alloc cmd buffer failed\n");
		return;
	}
	kalMemZero(prCmd, sizeof(CMD_SET_DOMAIN_INFO_T));

	prCmd->u2CountryCode = prAdapter->rWifiVar.rConnSettings.u2CountryCode;
	prCmd->u2IsSetPassiveScan = 0;
	prCmd->uc2G4Bandwidth = prAdapter->rWifiVar.rConnSettings.uc2G4BandwidthMode;
	prCmd->uc5GBandwidth = prAdapter->rWifiVar.rConnSettings.uc5GBandwidthMode;
	prCmd->aucReserved[0] = 0;
	prCmd->aucReserved[1] = 0;

	for (i = 0; i < MAX_SUBBAND_NUM; i++) {
		prSubBand = &prDomainInfo->rSubBand[i];

		prCmd->rSubBand[i].ucRegClass = prSubBand->ucRegClass;
		prCmd->rSubBand[i].ucBand = prSubBand->ucBand;

		if (prSubBand->ucBand != BAND_NULL && prSubBand->ucBand < BAND_NUM) {
			prCmd->rSubBand[i].ucChannelSpan = prSubBand->ucChannelSpan;
			prCmd->rSubBand[i].ucFirstChannelNum = prSubBand->ucFirstChannelNum;
			prCmd->rSubBand[i].ucNumChannels = prSubBand->ucNumChannels;
		}
	}

	/* Set domain info to chip */
	wlanSendSetQueryCmd(prAdapter, /* prAdapter */
				      CMD_ID_SET_DOMAIN_INFO, /* ucCID */
				      TRUE,	/* fgSetQuery */
				      FALSE, /* fgNeedResp */
				      fgIsOid, /* fgIsOid */
				      NULL, /* pfCmdDoneHandler */
				      NULL, /* pfCmdTimeoutHandler */
				      sizeof(CMD_SET_DOMAIN_INFO_T), /* u4SetQueryInfoLen */
				      (PUINT_8) prCmd, /* pucInfoBuffer */
				      NULL,	/* pvSetQueryBuffer */
				      0	/* u4SetQueryBufferLen */
	    );

	cnmMemFree(prAdapter, prCmd);
}

/*----------------------------------------------------------------------------*/
/*!
* @brief
*
* @param[in]
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID rlmDomainSendPassiveScanInfoCmd(P_ADAPTER_T prAdapter, BOOLEAN fgIsOid)
{
	P_DOMAIN_INFO_ENTRY prDomainInfo;
	P_CMD_SET_DOMAIN_INFO_T prCmd;
	WLAN_STATUS rStatus;
	P_DOMAIN_SUBBAND_INFO prSubBand;
	UINT_16 u2TargetCountryCode;
	UINT_8 i, j;

	prCmd = cnmMemAlloc(prAdapter, RAM_TYPE_BUF, sizeof(CMD_SET_DOMAIN_INFO_T));
	if (!prCmd) {
		DBGLOG(RLM, ERROR, "Alloc cmd buffer failed\n");
		return;
	}
	kalMemZero(prCmd, sizeof(CMD_SET_DOMAIN_INFO_T));

	prCmd->u2CountryCode = prAdapter->rWifiVar.rConnSettings.u2CountryCode;
	prCmd->u2IsSetPassiveScan = 1;
	prCmd->uc2G4Bandwidth = prAdapter->rWifiVar.rConnSettings.uc2G4BandwidthMode;
	prCmd->uc5GBandwidth = prAdapter->rWifiVar.rConnSettings.uc5GBandwidthMode;
	prCmd->aucReserved[0] = 0;
	prCmd->aucReserved[1] = 0;

	DBGLOG(RLM, TRACE, "u2CountryCode=0x%04x\n", prAdapter->rWifiVar.rConnSettings.u2CountryCode);

	u2TargetCountryCode = prAdapter->rWifiVar.rConnSettings.u2CountryCode;

	for (i = 0; i < REG_DOMAIN_PASSIVE_GROUP_NUM; i++) {
		prDomainInfo = &arSupportedRegDomains_Passive[i];

		for (j = 0; j < prDomainInfo->u4CountryNum; j++) {
			if (prDomainInfo->pu2CountryGroup[j] == u2TargetCountryCode)
				break;
		}
		if (j < prDomainInfo->u4CountryNum)
			break;	/* Found */
	}

	if (i >= REG_DOMAIN_PASSIVE_GROUP_NUM)
		prDomainInfo = &arSupportedRegDomains_Passive[REG_DOMAIN_PASSIVE_DEF_IDX];

	for (i = 0; i < MAX_SUBBAND_NUM; i++) {
		prSubBand = &prDomainInfo->rSubBand[i];

		prCmd->rSubBand[i].ucRegClass = prSubBand->ucRegClass;
		prCmd->rSubBand[i].ucBand = prSubBand->ucBand;

		if (prSubBand->ucBand != BAND_NULL && prSubBand->ucBand < BAND_NUM) {
			prCmd->rSubBand[i].ucChannelSpan = prSubBand->ucChannelSpan;
			prCmd->rSubBand[i].ucFirstChannelNum = prSubBand->ucFirstChannelNum;
			prCmd->rSubBand[i].ucNumChannels = prSubBand->ucNumChannels;
		}
	}

	/* Set passive scan channel info to chip */
	rStatus = wlanSendSetQueryCmd(prAdapter, /* prAdapter */
				      CMD_ID_SET_DOMAIN_INFO, /* ucCID */
				      TRUE,	/* fgSetQuery */
				      FALSE, /* fgNeedResp */
				      fgIsOid, /* fgIsOid */
				      NULL, /* pfCmdDoneHandler */
				      NULL, /* pfCmdTimeoutHandler */
				      sizeof(CMD_SET_DOMAIN_INFO_T), /* u4SetQueryInfoLen */
				      (PUINT_8) prCmd, /* pucInfoBuffer */
				      NULL,	/* pvSetQueryBuffer */
				      0	/* u4SetQueryBufferLen */
	    );

	ASSERT(rStatus == WLAN_STATUS_PENDING);

	cnmMemFree(prAdapter, prCmd);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in/out]
*
* \return TRUE  Legal channel
*         FALSE Illegal channel for current regulatory domain
*/
/*----------------------------------------------------------------------------*/
BOOLEAN rlmDomainIsLegalChannel_V2(P_ADAPTER_T prAdapter, ENUM_BAND_T eBand, UINT_8 ucChannel)
{
#if (CFG_SUPPORT_SINGLE_SKU == 1)
	UINT_8 idx, start_idx, end_idx;
	struct channel *prCh;

	if (eBand == BAND_2G4) {
		start_idx = 0;
		end_idx = rlmDomainGetActiveChannelCount(KAL_BAND_2GHZ);
	} else {
		start_idx = rlmDomainGetActiveChannelCount(KAL_BAND_2GHZ);
		end_idx = rlmDomainGetActiveChannelCount(KAL_BAND_2GHZ) +
					rlmDomainGetActiveChannelCount(KAL_BAND_5GHZ);
	}

	for (idx = start_idx; idx < end_idx; idx++) {
		prCh = rlmDomainGetActiveChannels() + idx;

		if (prCh->chNum == ucChannel)
			return TRUE;
	}

	return FALSE;
#else
	return FALSE;
#endif
}

BOOLEAN rlmDomainIsLegalChannel(P_ADAPTER_T prAdapter, ENUM_BAND_T eBand, UINT_8 ucChannel)
{
	UINT_8 i, j;
	P_DOMAIN_SUBBAND_INFO prSubband;
	P_DOMAIN_INFO_ENTRY prDomainInfo;

	if (regd_is_single_sku_en())
		return rlmDomainIsLegalChannel_V2(prAdapter, eBand, ucChannel);


	prDomainInfo = rlmDomainGetDomainInfo(prAdapter);
	ASSERT(prDomainInfo);

	for (i = 0; i < MAX_SUBBAND_NUM; i++) {
		prSubband = &prDomainInfo->rSubBand[i];

		if (prSubband->ucBand == BAND_5G && !prAdapter->fgEnable5GBand)
			continue;

		if (prSubband->ucBand == eBand) {
			for (j = 0; j < prSubband->ucNumChannels; j++) {
				if ((prSubband->ucFirstChannelNum + j * prSubband->ucChannelSpan)
				    == ucChannel) {
					return TRUE;
				}
			}
		}
	}

	return FALSE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in/out]
*
* \return none
*/
/*----------------------------------------------------------------------------*/

UINT_32 rlmDomainSupOperatingClassIeFill(PUINT_8 pBuf)
{
	/*
	 *  The Country element should only be included for Status Code 0 (Successful).
	 */
	UINT_32 u4IeLen;
	UINT_8 aucClass[12] = { 0x01, 0x02, 0x03, 0x05, 0x16, 0x17, 0x19, 0x1b,
		0x1c, 0x1e, 0x20, 0x21
	};

	/*
	 *  The Supported Operating Classes element is used by a STA to advertise the
	 *  operating classes that it is capable of operating with in this country.
	 *
	 *  The Country element (see 8.4.2.10) allows a STA to configure its PHY and MAC
	 *  for operation when the operating triplet of Operating Extension Identifier,
	 *  Operating Class, and Coverage Class fields is present.
	 */
	SUP_OPERATING_CLASS_IE(pBuf)->ucId = ELEM_ID_SUP_OPERATING_CLASS;
	SUP_OPERATING_CLASS_IE(pBuf)->ucLength = 1 + sizeof(aucClass);
	SUP_OPERATING_CLASS_IE(pBuf)->ucCur = 0x0c;	/* 0x51 */
	kalMemCopy(SUP_OPERATING_CLASS_IE(pBuf)->ucSup, aucClass, sizeof(aucClass));
	u4IeLen = (SUP_OPERATING_CLASS_IE(pBuf)->ucLength + 2);
	pBuf += u4IeLen;

	COUNTRY_IE(pBuf)->ucId = ELEM_ID_COUNTRY_INFO;
	COUNTRY_IE(pBuf)->ucLength = 6;
	COUNTRY_IE(pBuf)->aucCountryStr[0] = 0x55;
	COUNTRY_IE(pBuf)->aucCountryStr[1] = 0x53;
	COUNTRY_IE(pBuf)->aucCountryStr[2] = 0x20;
	COUNTRY_IE(pBuf)->arCountryStr[0].ucFirstChnlNum = 1;
	COUNTRY_IE(pBuf)->arCountryStr[0].ucNumOfChnl = 11;
	COUNTRY_IE(pBuf)->arCountryStr[0].cMaxTxPwrLv = 0x1e;
	u4IeLen += (COUNTRY_IE(pBuf)->ucLength + 2);

	return u4IeLen;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief
*
* @param[in]
*
* @return (fgValid) : 0 -> inValid, 1 -> Valid
*/
/*----------------------------------------------------------------------------*/
BOOLEAN rlmDomainCheckChannelEntryValid(P_ADAPTER_T prAdapter, UINT_8 ucCentralCh)
{
	BOOLEAN fgValid = FALSE;
	UINT_8 ucTemp = 0xff;
	UINT_8 i;
	/*Check Power limit table channel efficient or not */

	for (i = POWER_LIMIT_2G4; i < POWER_LIMIT_SUBAND_NUM; i++) {
		if ((ucCentralCh >= g_rRlmSubBand[i].ucStartCh) && (ucCentralCh <= g_rRlmSubBand[i].ucEndCh))
			ucTemp = (ucCentralCh - g_rRlmSubBand[i].ucStartCh) % g_rRlmSubBand[i].ucInterval;
	}

#if 0
	/*2.4G, ex 1, 2, 3 */
	if (ucCentralCh >= BAND_2G4_LOWER_BOUND && ucCentralCh <= BAND_2G4_UPPER_BOUND)
		ucTemp = 0;
	/*FCC- Spec : Band UNII-1, ex 36, 38, 40.... */
	else if (ucCentralCh >= UNII1_LOWER_BOUND && ucCentralCh <= UNII1_UPPER_BOUND)
		ucTemp = (ucCentralCh - UNII1_LOWER_BOUND) % 2;
	/*FCC- Spec : Band UNII-2A, ex 52, 54, 56.... */
	else if (ucCentralCh >= UNII2A_LOWER_BOUND && ucCentralCh <= UNII2A_UPPER_BOUND)
		ucTemp = (ucCentralCh - UNII2A_LOWER_BOUND) % 2;
	/*FCC- Spec : Band UNII-2C, ex 100, 102, 104.... */
	else if (ucCentralCh >= UNII2C_LOWER_BOUND && ucCentralCh <= UNII2C_UPPER_BOUND)
		ucTemp = (ucCentralCh - UNII2C_LOWER_BOUND) % 2;
	/*FCC- Spec : Band UNII-3, ex 149, 151, 153... */
	else if (ucCentralCh >= UNII3_LOWER_BOUND && ucCentralCh <= UNII3_UPPER_BOUND)
		ucTemp = (ucCentralCh - UNII3_LOWER_BOUND) % 2;
#endif
	if (ucTemp == 0)
		fgValid = TRUE;
	return fgValid;

}

/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in]
*
* \return
*/
/*----------------------------------------------------------------------------*/
UINT_8 rlmDomainGetCenterChannel(ENUM_BAND_T eBand, UINT_8 ucPriChannel, ENUM_CHNL_EXT_T eExtend)
{
	UINT_8 ucCenterChannel;

	if (eExtend == CHNL_EXT_SCA)
		ucCenterChannel = ucPriChannel + 2;
	else if (eExtend == CHNL_EXT_SCB)
		ucCenterChannel = ucPriChannel - 2;
	else
		ucCenterChannel = ucPriChannel;

	return ucCenterChannel;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in]
*
* \return
*/
/*----------------------------------------------------------------------------*/
BOOLEAN
rlmDomainIsValidRfSetting(P_ADAPTER_T prAdapter,
			  ENUM_BAND_T eBand,
			  UINT_8 ucPriChannel,
			  ENUM_CHNL_EXT_T eExtend,
			  ENUM_CHANNEL_WIDTH_T eChannelWidth, UINT_8 ucChannelS1, UINT_8 ucChannelS2)
{
	UINT_8 ucCenterChannel = 0;
	UINT_8  ucUpperChannel;
	UINT_8  ucLowerChannel;
	BOOLEAN fgValidChannel = TRUE;
	BOOLEAN fgUpperChannel = TRUE;
	BOOLEAN fgLowerChannel = TRUE;
	BOOLEAN fgValidBW = TRUE;
	BOOLEAN fgValidRfSetting = TRUE;
	UINT_32 u4PrimaryOffset;

	/*DBG msg for Channel InValid */
	if (eChannelWidth == CW_20_40MHZ) {
		ucCenterChannel = rlmDomainGetCenterChannel(eBand, ucPriChannel, eExtend);

		/* Check Central Channel Valid or Not */
		fgValidChannel = rlmDomainCheckChannelEntryValid(prAdapter, ucCenterChannel);
		if (fgValidChannel == FALSE)
			DBGLOG(RLM, WARN, "Rf20: CentralCh=%d\n", ucCenterChannel);

		/* Check Upper Channel and Lower Channel */
		switch (eExtend) {
		case CHNL_EXT_SCA:
			ucUpperChannel = ucPriChannel + 4;
			ucLowerChannel = ucPriChannel;
			break;
		case CHNL_EXT_SCB:
			ucUpperChannel = ucPriChannel;
			ucLowerChannel = ucPriChannel - 4;
			break;
		default:
			ucUpperChannel = ucPriChannel;
			ucLowerChannel = ucPriChannel;
			break;
		}

		fgUpperChannel = rlmDomainCheckChannelEntryValid(prAdapter, ucUpperChannel);
		if (fgUpperChannel == FALSE)
			DBGLOG(RLM, WARN, "Rf20: UpperCh=%d\n", ucUpperChannel);

		fgLowerChannel = rlmDomainCheckChannelEntryValid(prAdapter, ucLowerChannel);
		if (fgLowerChannel == FALSE)
			DBGLOG(RLM, WARN, "Rf20: LowerCh=%d\n", ucLowerChannel);

	} else if ((eChannelWidth == CW_80MHZ) || (eChannelWidth == CW_160MHZ)) {
		ucCenterChannel = ucChannelS1;

		/* Check Central Channel Valid or Not */
		if (eChannelWidth != CW_160MHZ) {
			/*BW not check , ex: primary 36 and central channel 50 will fail the check*/
			fgValidChannel = rlmDomainCheckChannelEntryValid(prAdapter, ucCenterChannel);
		}

		if (fgValidChannel == FALSE)
			DBGLOG(RLM, WARN, "Rf80/160C: CentralCh=%d\n", ucCenterChannel);
	} else if (eChannelWidth == CW_80P80MHZ) {
		ucCenterChannel = ucChannelS1;

		fgValidChannel = rlmDomainCheckChannelEntryValid(prAdapter, ucCenterChannel);

		if (fgValidChannel == FALSE)
			DBGLOG(RLM, WARN, "Rf160NC: CentralCh1=%d\n", ucCenterChannel);

		ucCenterChannel = ucChannelS2;

		fgValidChannel = rlmDomainCheckChannelEntryValid(prAdapter, ucCenterChannel);

		if (fgValidChannel == FALSE)
			DBGLOG(RLM, WARN, "Rf160NC: CentralCh2=%d\n", ucCenterChannel);

		/* Check Central Channel Valid or Not */
	} else {
		DBGLOG(RLM, ERROR, "Wrong BW =%d\n", eChannelWidth);
		fgValidChannel = FALSE;
	}

	/* Check BW Setting Correct or Not */
	if (eBand == BAND_2G4) {
		if (eChannelWidth != CW_20_40MHZ) {
			fgValidBW = FALSE;
			DBGLOG(RLM, WARN, "Rf: B=%d, W=%d\n", eBand, eChannelWidth);
		}
	} else {
		if ((eChannelWidth == CW_80MHZ) || (eChannelWidth == CW_80P80MHZ)) {
			u4PrimaryOffset = CAL_CH_OFFSET_80M(ucPriChannel, ucChannelS1);
			if (u4PrimaryOffset >= 4) {
				fgValidBW = FALSE;
				DBGLOG(RLM, WARN, "Rf: PriOffSet=%d, W=%d\n", u4PrimaryOffset, eChannelWidth);
			}
		} else if (eChannelWidth == CW_160MHZ) {
			u4PrimaryOffset = CAL_CH_OFFSET_160M(ucPriChannel, ucCenterChannel);
			if (u4PrimaryOffset >= 8) {
				fgValidBW = FALSE;
				DBGLOG(RLM, WARN, "Rf: PriOffSet=%d, W=%d\n", u4PrimaryOffset, eChannelWidth);
			}
		}
	}

	if ((fgValidBW == FALSE) || (fgValidChannel == FALSE) || (fgUpperChannel == FALSE) || (fgLowerChannel == FALSE))
		fgValidRfSetting = FALSE;

	return fgValidRfSetting;

}

#if (CFG_SUPPORT_SINGLE_SKU == 1)

/*
 * This function coverts country code from alphabet chars to u32,
 * the caller need to pass country code chars and do size check
 */
UINT_32 rlmDomainAlpha2ToU32(PCHAR pcAlpha2, UINT_8 ucAlpha2Size)
{
	UINT_8 ucIdx;
	UINT_32 u4CountryCode = 0;

	if (ucAlpha2Size > TX_PWR_LIMIT_COUNTRY_STR_MAX_LEN) {
		DBGLOG(RLM, ERROR, "alpha2 size %d is invalid!(max: %d)\n",
			ucAlpha2Size, TX_PWR_LIMIT_COUNTRY_STR_MAX_LEN);
		ucAlpha2Size = TX_PWR_LIMIT_COUNTRY_STR_MAX_LEN;
	}

	for (ucIdx = 0; ucIdx < ucAlpha2Size; ucIdx++)
		u4CountryCode |= (pcAlpha2[ucIdx] << (ucIdx * 8));

	return u4CountryCode;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Search the tx power limit setting range of the specified in the text file
*
* \param[IN] u4CountryCode The u32 type of the specified country.
* \param[IN] pucBuf The content of the text file.
* \param[IN] u4cBufLen End boundary of the text file.
* \param[OUT] pu4CountryStart Store the start position of the desired country settings.
* \param[OUT] pu4CountryEnd Store the end position of the desired country settings.
*
* \retval TRUE Success.
* \retval FALSE Failure.
*/
/*----------------------------------------------------------------------------*/

BOOL rlmDomainTxPwrLimitGetCountryRange(
	UINT_32 u4CountryCode, PUINT_8 pucBuf, UINT_32 u4BufLen,
	PUINT_32 pu4CountryStart, PUINT_32 pu4CountryEnd)
{
	UINT_32 u4TmpPos = 0;
	char pcrCountryStr[TX_PWR_LIMIT_COUNTRY_STR_MAX_LEN + 1] = {0};
	UINT_8 cIdx = 0;
	bool search_next = false;

	while (1) {
		if (!search_next) {
			/* Search country code entry */
			while (u4TmpPos < u4BufLen && pucBuf[u4TmpPos] != '[')
				u4TmpPos++;
			/* skip the '[' char */
			u4TmpPos++;
		}

		cIdx = 0;
		while (u4TmpPos < u4BufLen &&
		       cIdx < TX_PWR_LIMIT_COUNTRY_STR_MAX_LEN &&
		       pucBuf[u4TmpPos] != ']' && pucBuf[u4TmpPos] != ',') {
			pcrCountryStr[cIdx++] = pucBuf[u4TmpPos];
			u4TmpPos++;
		}

		if (pucBuf[u4TmpPos] == ',')
			search_next = true;
		else
			search_next = false;

		/* skip the ']' or ',' char */
		u4TmpPos++;

		if (u4TmpPos >= u4BufLen || cIdx > TX_PWR_LIMIT_COUNTRY_STR_MAX_LEN)
			return FALSE;

		if (u4CountryCode == rlmDomainAlpha2ToU32(pcrCountryStr, cIdx)) {
			DBGLOG(RLM, INFO, "Found TxPwrLimit table for CountryCode \"%s\"\n",
				pcrCountryStr);
			/* the location after char ']' or ',' */
			*pu4CountryStart = u4TmpPos;
			break;
		}
	}

	while (u4TmpPos < u4BufLen && pucBuf[u4TmpPos] != '[')
		u4TmpPos++;

	*pu4CountryEnd = u4TmpPos;

	return TRUE;
}

BOOL rlmDomainTxPwrLimitSearchSection(const char *pSectionName,
	PINT_8 pucBuf, PUINT_32 pu4Pos, UINT_32 u4BufEnd)
{
	UINT_32 u4TmpPos = *pu4Pos;
	UINT_8 uSectionNameLen = kalStrLen(pSectionName);

	while (1) {
		while (u4TmpPos < u4BufEnd && pucBuf[u4TmpPos] != '<')
			u4TmpPos++;

		u4TmpPos++; /* skip char '<' */

		if (u4TmpPos + uSectionNameLen >= u4BufEnd)
			return FALSE;

		if (kalStrnCmp(&pucBuf[u4TmpPos], pSectionName, uSectionNameLen) == 0) {

			/* Go to the end of section header line */
			while (u4TmpPos < u4BufEnd && pucBuf[u4TmpPos] != '\n')
				u4TmpPos++;

			*pu4Pos = u4TmpPos;

			break;
		}
	}

	return TRUE;
}

BOOL rlmDomainTxPwrLimitSectionEnd(PUINT_8 pucBuf,
	const char *pSectionName, PUINT_32 pu4Pos, UINT_32 u4BufEnd)
{
	UINT_32 u4TmpPos = *pu4Pos;
	char cTmpChar = 0;
	UINT_8 uSectionNameLen = kalStrLen(pSectionName);

	while (u4TmpPos < u4BufEnd) {
		cTmpChar = pucBuf[u4TmpPos];

		/* skip blank lines */
		if (cTmpChar == ' ' || cTmpChar == '\t' ||
			cTmpChar == '\n' || cTmpChar == '\r') {
			u4TmpPos++;
			continue;
		}

		break;
	}

	if (u4TmpPos + uSectionNameLen + 2 >= u4BufEnd) {/* 2 means '/' and '>' */
		*pu4Pos = u4BufEnd;
		return FALSE;
	}

	if (pucBuf[u4TmpPos] != '<')
		return FALSE;

	if (pucBuf[u4TmpPos + 1] != '/' ||
		pucBuf[u4TmpPos + 2 + uSectionNameLen] != '>' ||
		kalStrnCmp(&pucBuf[u4TmpPos + 2], pSectionName, uSectionNameLen)) {

		*pu4Pos = u4TmpPos + uSectionNameLen + 2;
		return FALSE;
	}

	*pu4Pos = u4TmpPos + uSectionNameLen + 3; /* 3 means go to the location after '>' */

	return TRUE;
}

INT_8 rlmDomainTxPwrLimitGetChIdx(
	struct TX_PWR_LIMIT_DATA *pTxPwrLimit, UINT_8 ucChannel)
{
	INT_8 cIdx = 0;

	for (cIdx = 0; cIdx < pTxPwrLimit->ucChNum; cIdx++)
		if (ucChannel == pTxPwrLimit->rChannelTxPwrLimit[cIdx].ucChannel)
			return cIdx;

	DBGLOG(RLM, ERROR, "Can't find idx of channel %d in TxPwrLimit data\n",
		ucChannel);

	return -1;
}

BOOL rlmDomainTxPwrLimitLoadChannelSetting(
	PUINT_8 pucBuf, PUINT_32 pu4Pos, UINT_32 u4BufEnd,
	struct TX_PWR_LIMIT_DATA *pTxPwrLimit, UINT_8 ucSectionIdx)
{
	UINT_32 u4TmpPos = *pu4Pos;
	char cTmpChar = 0;
	struct CHANNEL_TX_PWR_LIMIT *prChTxPwrLimit = NULL;
	BOOL bNeg = FALSE;
	INT_8 cLimitValue = 0, cChIdx = 0;
	UINT_8 ucIdx = 0, ucChannel = 0;

	/* skip blank lines */
	while (u4TmpPos < u4BufEnd) {
		cTmpChar = pucBuf[u4TmpPos];

		if (cTmpChar == ' ' || cTmpChar == '\t' ||
			cTmpChar == '\n' || cTmpChar == '\r') {
			u4TmpPos++;
			continue;
		}

		break;
	}

	/* current is at the location of 'c', check remaining buf length for 'chxxx' */
	if (u4TmpPos + 5 >= u4BufEnd) {
		DBGLOG(RLM, ERROR,
			"Invalid location of ch setting: %u/%u\n",
			u4TmpPos, u4BufEnd);
		return FALSE;
	}

	if (pucBuf[u4TmpPos] == 'c' && pucBuf[u4TmpPos + 1] == 'h') {
		ucChannel = (pucBuf[u4TmpPos + 2] - '0') * 100 +
					(pucBuf[u4TmpPos + 3] - '0') * 10 +
					(pucBuf[u4TmpPos + 4] - '0');
	} else { /* invalid format */
		*pu4Pos = u4TmpPos;
		DBGLOG(RLM, ERROR, "Invalid ch setting starting chars: %c%c\n",
			pucBuf[u4TmpPos], pucBuf[u4TmpPos + 1]);

		/* goto next line */
		while (*pu4Pos < u4BufEnd && pucBuf[*pu4Pos] != '\n')
			(*pu4Pos)++;

		return TRUE;
	}

	cChIdx = rlmDomainTxPwrLimitGetChIdx(pTxPwrLimit, ucChannel);

	if (cChIdx == -1) {
		*pu4Pos = u4TmpPos;
		DBGLOG(RLM, ERROR, "Invalid ch %u %c%c%c\n", ucChannel,
			pucBuf[u4TmpPos + 2], pucBuf[u4TmpPos + 3], pucBuf[u4TmpPos + 4]);

		/* goto next line */
		while (*pu4Pos < u4BufEnd && pucBuf[*pu4Pos] != '\n')
			(*pu4Pos)++;

		return TRUE;
	}

	u4TmpPos += 5;

	prChTxPwrLimit = &pTxPwrLimit->rChannelTxPwrLimit[cChIdx];

	/* read the channel TxPwrLimit settings */
	for (ucIdx = 0; ucIdx < gTx_Pwr_Limit_Element_Num[ucSectionIdx]; ucIdx++) {

		/* skip blank and comma */
		while (u4TmpPos < u4BufEnd) {
			cTmpChar = pucBuf[u4TmpPos];

			if (cTmpChar == ' ' || cTmpChar == '\t' || cTmpChar == ',') {
				u4TmpPos++;
				continue;
			}
			break;
		}

		if (u4TmpPos >= u4BufEnd) {
			*pu4Pos = u4BufEnd;
			DBGLOG(RLM, ERROR,
				"Invalid location of ch tx pwr limit val: %u/%u\n",
				u4TmpPos, u4BufEnd);
			return FALSE;
		}

		bNeg = FALSE;

		cTmpChar = pucBuf[u4TmpPos];

		if (cTmpChar == '-') {
			bNeg = TRUE;
			u4TmpPos++;
		} else if (cTmpChar == 'x') {
			prChTxPwrLimit->rTxPwrLimitValue[ucSectionIdx][ucIdx] =
				TX_PWR_LIMIT_MAX_VAL;
			u4TmpPos++;
			continue;
		}

		cLimitValue = 0;
		while (u4TmpPos < u4BufEnd) {
			cTmpChar = pucBuf[u4TmpPos];

			if (cTmpChar < '0' || cTmpChar > '9')
				break;

			cLimitValue = (cLimitValue * 10) + (cTmpChar - '0');
			u4TmpPos++;
		}

		if (bNeg)
			cLimitValue = -cLimitValue;

		prChTxPwrLimit->rTxPwrLimitValue[ucSectionIdx][ucIdx] = cLimitValue;
	}

	*pu4Pos = u4TmpPos;
	return TRUE;
}

VOID rlmDomainTxPwrLimitRemoveComments(
	PUINT_8 pucBuf, UINT_32 u4BufLen)
{
	UINT_32 u4TmpPos = 0;
	char cTmpChar = 0;

	while (u4TmpPos < u4BufLen) {
		cTmpChar = pucBuf[u4TmpPos];

		if (cTmpChar == '#') {
			while (cTmpChar != '\n') {
				pucBuf[u4TmpPos] = ' ';

				u4TmpPos++;
				if (u4TmpPos >= u4BufLen)
					break;

				cTmpChar = pucBuf[u4TmpPos];
			}
		}
		u4TmpPos++;
	}
}

BOOL rlmDomainTxPwrLimitLoad(
	P_ADAPTER_T prAdapter, PUINT_8 pucBuf, UINT_32 u4BufLen,
	UINT_32 u4CountryCode, struct TX_PWR_LIMIT_DATA *pTxPwrLimit)
{
	UINT_8 uSecIdx = 0;
	UINT_32 u4CountryStart = 0, u4CountryEnd = 0, u4Pos = 0;

	rlmDomainTxPwrLimitRemoveComments(pucBuf, u4BufLen);

	if (!rlmDomainTxPwrLimitGetCountryRange(u4CountryCode, pucBuf,
		u4BufLen, &u4CountryStart, &u4CountryEnd)) {
		DBGLOG(RLM, ERROR, "Can't find specified table in %s\n",
			WLAN_TX_PWR_LIMIT_FILE_NAME);

		/* Use WW as default country */
		if (!rlmDomainTxPwrLimitGetCountryRange(COUNTRY_CODE_WW, pucBuf,
			u4BufLen, &u4CountryStart, &u4CountryEnd)) {
			DBGLOG(RLM, ERROR,
				"Can't find default table (WW) in %s\n",
				WLAN_TX_PWR_LIMIT_FILE_NAME);
			return FALSE;
		}
	}

	u4Pos = u4CountryStart;

	for (uSecIdx = 0; uSecIdx < TX_PWR_LIMIT_SECTION_NUM; uSecIdx++) {
		if (!rlmDomainTxPwrLimitSearchSection(gTx_Pwr_Limit_Section[uSecIdx],
			pucBuf, &u4Pos, u4CountryEnd)) {
			DBGLOG(RLM, ERROR, "Can't find specified section %s in %s\n",
				gTx_Pwr_Limit_Section[uSecIdx], WLAN_TX_PWR_LIMIT_FILE_NAME);
			return FALSE;
		}

		DBGLOG(RLM, INFO, "Find specified section %s in %s\n",
			gTx_Pwr_Limit_Section[uSecIdx], WLAN_TX_PWR_LIMIT_FILE_NAME);

		while (!rlmDomainTxPwrLimitSectionEnd(pucBuf,
			gTx_Pwr_Limit_Section[uSecIdx], &u4Pos, u4CountryEnd) &&
			u4Pos < u4CountryEnd) {
			if (!rlmDomainTxPwrLimitLoadChannelSetting(pucBuf, &u4Pos,
				u4CountryEnd, pTxPwrLimit, uSecIdx))
				return FALSE;
		}
	}

	DBGLOG(RLM, INFO, "Load %s finished\n", WLAN_TX_PWR_LIMIT_FILE_NAME);
	return TRUE;
}

VOID rlmDomainTxPwrLimitSetChValues(
	P_CMD_CHANNEL_POWER_LIMIT_V2 pCmd, struct CHANNEL_TX_PWR_LIMIT *pChTxPwrLimit)
{
	UINT_8 section = 0, e = 0;

	pCmd->tx_pwr_dsss_cck = pChTxPwrLimit->rTxPwrLimitValue[0][0];
	pCmd->tx_pwr_dsss_bpsk = pChTxPwrLimit->rTxPwrLimitValue[0][1];

	pCmd->tx_pwr_ofdm_bpsk = pChTxPwrLimit->rTxPwrLimitValue[0][2]; /* 6M, 9M */
	pCmd->tx_pwr_ofdm_qpsk = pChTxPwrLimit->rTxPwrLimitValue[0][3]; /* 12M, 18M */
	pCmd->tx_pwr_ofdm_16qam = pChTxPwrLimit->rTxPwrLimitValue[0][4]; /* 24M, 36M */
	pCmd->tx_pwr_ofdm_48m = pChTxPwrLimit->rTxPwrLimitValue[0][5];
	pCmd->tx_pwr_ofdm_54m = pChTxPwrLimit->rTxPwrLimitValue[0][6];

	pCmd->tx_pwr_ht20_bpsk = pChTxPwrLimit->rTxPwrLimitValue[1][0]; /* MCS0*/
	pCmd->tx_pwr_ht20_qpsk = pChTxPwrLimit->rTxPwrLimitValue[1][1]; /* MCS1, MCS2*/
	pCmd->tx_pwr_ht20_16qam = pChTxPwrLimit->rTxPwrLimitValue[1][2]; /* MCS3, MCS4*/
	pCmd->tx_pwr_ht20_mcs5 = pChTxPwrLimit->rTxPwrLimitValue[1][3]; /* MCS5*/
	pCmd->tx_pwr_ht20_mcs6 = pChTxPwrLimit->rTxPwrLimitValue[1][4]; /* MCS6*/
	pCmd->tx_pwr_ht20_mcs7 = pChTxPwrLimit->rTxPwrLimitValue[1][5]; /* MCS7*/

	pCmd->tx_pwr_ht40_bpsk = pChTxPwrLimit->rTxPwrLimitValue[2][0]; /* MCS0*/
	pCmd->tx_pwr_ht40_qpsk = pChTxPwrLimit->rTxPwrLimitValue[2][1]; /* MCS1, MCS2*/
	pCmd->tx_pwr_ht40_16qam = pChTxPwrLimit->rTxPwrLimitValue[2][2]; /* MCS3, MCS4*/
	pCmd->tx_pwr_ht40_mcs5 = pChTxPwrLimit->rTxPwrLimitValue[2][3]; /* MCS5*/
	pCmd->tx_pwr_ht40_mcs6 = pChTxPwrLimit->rTxPwrLimitValue[2][4]; /* MCS6*/
	pCmd->tx_pwr_ht40_mcs7 = pChTxPwrLimit->rTxPwrLimitValue[2][5]; /* MCS7*/
	pCmd->tx_pwr_ht40_mcs32 = pChTxPwrLimit->rTxPwrLimitValue[2][6]; /* MCS32*/

	pCmd->tx_pwr_vht20_bpsk = pChTxPwrLimit->rTxPwrLimitValue[3][0]; /* MCS0*/
	pCmd->tx_pwr_vht20_qpsk = pChTxPwrLimit->rTxPwrLimitValue[3][1]; /* MCS1, MCS2*/
	pCmd->tx_pwr_vht20_16qam = pChTxPwrLimit->rTxPwrLimitValue[3][2]; /* MCS3, MCS4*/
	pCmd->tx_pwr_vht20_64qam = pChTxPwrLimit->rTxPwrLimitValue[3][3]; /* MCS5, MCS6*/
	pCmd->tx_pwr_vht20_mcs7 = pChTxPwrLimit->rTxPwrLimitValue[3][4];
	pCmd->tx_pwr_vht20_mcs8 = pChTxPwrLimit->rTxPwrLimitValue[3][5];
	pCmd->tx_pwr_vht20_mcs9 = pChTxPwrLimit->rTxPwrLimitValue[3][6];

	pCmd->tx_pwr_vht_40 = pChTxPwrLimit->rTxPwrLimitValue[4][2];
	pCmd->tx_pwr_vht_80 = pChTxPwrLimit->rTxPwrLimitValue[4][3];
	pCmd->tx_pwr_vht_160c = pChTxPwrLimit->rTxPwrLimitValue[4][5];
	pCmd->tx_pwr_vht_160nc = pChTxPwrLimit->rTxPwrLimitValue[4][4];
	pCmd->tx_pwr_lg_40 = pChTxPwrLimit->rTxPwrLimitValue[4][0];
	pCmd->tx_pwr_lg_80 = pChTxPwrLimit->rTxPwrLimitValue[4][1];


	DBGLOG(RLM, TRACE, "ch %d\n", pCmd->ucCentralCh);
	for (section = 0; section < TX_PWR_LIMIT_SECTION_NUM; section++)
		for (e = 0; e < gTx_Pwr_Limit_Element_Num[section]; e++)
			DBGLOG(RLM, TRACE, "TxPwrLimit[%s][%s]= %d\n",
				gTx_Pwr_Limit_Section[section],
				gTx_Pwr_Limit_Element[section][e],
				pChTxPwrLimit->rTxPwrLimitValue[section][e]);
}

VOID rlmDomainTxPwrLimitSetValues(
	P_CMD_SET_COUNTRY_CHANNEL_POWER_LIMIT_V2_T pSetCmd,
	struct TX_PWR_LIMIT_DATA *pTxPwrLimit)
{
	UINT_8 ucIdx = 0;
	INT_8 cChIdx = 0;
	P_CMD_CHANNEL_POWER_LIMIT_V2 pCmd = NULL;
	struct CHANNEL_TX_PWR_LIMIT *pChTxPwrLimit = NULL;

	if (!pSetCmd || !pTxPwrLimit) {
		DBGLOG(RLM, ERROR, "%s: Invalid request!!!\n", __func__);
		return;
	}

	for (ucIdx = 0; ucIdx < pSetCmd->ucNum; ucIdx++) {
		pCmd = &(pSetCmd->rChannelPowerLimit[ucIdx]);
		cChIdx = rlmDomainTxPwrLimitGetChIdx(pTxPwrLimit, pCmd->ucCentralCh);
		if (cChIdx == -1) {
			DBGLOG(RLM, ERROR, "Invalid ch idx found while assigning values\n");
			continue;
		}
		pChTxPwrLimit = &pTxPwrLimit->rChannelTxPwrLimit[cChIdx];
		rlmDomainTxPwrLimitSetChValues(pCmd, pChTxPwrLimit);
	}
}

BOOL rlmDomainTxPwrLimitLoadFromFile(P_ADAPTER_T prAdapter,
	UINT_32 u4CountryCode, struct TX_PWR_LIMIT_DATA *pTxPwrLimit)
{
	PUINT_8 pucConfigBuf;
	UINT_32 u4ConfigReadLen;
	BOOL bRet = TRUE;

	pucConfigBuf = (PUINT_8) kalMemAlloc(WLAN_TX_PWR_LIMIT_FILE_BUF_SIZE, VIR_MEM_TYPE);

	if (!pucConfigBuf)
		return FALSE;

	kalMemZero(pucConfigBuf, WLAN_TX_PWR_LIMIT_FILE_BUF_SIZE);
	u4ConfigReadLen = 0;

	if (wlanGetFileContent(prAdapter, WLAN_TX_PWR_LIMIT_FILE_NAME, pucConfigBuf,
				 WLAN_TX_PWR_LIMIT_FILE_BUF_SIZE, &u4ConfigReadLen, TRUE) == 0) {
		/* ToDo:: Nothing */
	} else if (wlanGetFileContent(prAdapter, "/storage/sdcard0/" WLAN_TX_PWR_LIMIT_FILE_NAME, pucConfigBuf,
				 WLAN_TX_PWR_LIMIT_FILE_BUF_SIZE, &u4ConfigReadLen, FALSE) == 0) {
		/* ToDo:: Nothing */
	} else if (wlanGetFileContent(prAdapter, "/data/misc/" WLAN_TX_PWR_LIMIT_FILE_NAME, pucConfigBuf,
				 WLAN_TX_PWR_LIMIT_FILE_BUF_SIZE, &u4ConfigReadLen, FALSE) == 0) {
		/* ToDo:: Nothing */
	} else if (wlanGetFileContent(prAdapter, "/data/misc/wifi/" WLAN_TX_PWR_LIMIT_FILE_NAME, pucConfigBuf,
				 WLAN_TX_PWR_LIMIT_FILE_BUF_SIZE, &u4ConfigReadLen, FALSE) == 0) {
		/* ToDo:: Nothing */
	} else {
		bRet = FALSE;
		goto error;
	}

	if (pucConfigBuf[0] == '\0' || u4ConfigReadLen == 0) {
		bRet = FALSE;
		goto error;
	}

	if (!rlmDomainTxPwrLimitLoad(prAdapter, pucConfigBuf, u4ConfigReadLen,
		u4CountryCode, pTxPwrLimit)) {
		bRet = FALSE;
		goto error;
	}

error:
	kalMemFree(pucConfigBuf, VIR_MEM_TYPE, WLAN_TX_PWR_LIMIT_FILE_BUF_SIZE);
	return bRet;
}

BOOLEAN rlmDomainGetTxPwrLimit(u32 country_code,
							  P_GLUE_INFO_T prGlueInfo,
							  P_CMD_SET_COUNTRY_CHANNEL_POWER_LIMIT_V2_T pSetCmd_2g,
							  P_CMD_SET_COUNTRY_CHANNEL_POWER_LIMIT_V2_T pSetCmd_5g
							  )
{
	int bRet = TRUE;
	UINT_8 ucIdx = 0, ucCnt = 0;
	struct TX_PWR_LIMIT_DATA *pTxPwrLimit =
		(struct TX_PWR_LIMIT_DATA *) kalMemAlloc(sizeof(struct TX_PWR_LIMIT_DATA), VIR_MEM_TYPE);

	if (!pTxPwrLimit) {
		bRet = FALSE;
		DBGLOG(RLM, ERROR, "Alloc buffer for TxPwrLimit main struct failed\n");
		return bRet;
	}

	pTxPwrLimit->ucChNum = (pSetCmd_2g ? pSetCmd_2g->ucNum : 0) + (pSetCmd_5g ? pSetCmd_5g->ucNum : 0);

	pTxPwrLimit->rChannelTxPwrLimit =
		(struct CHANNEL_TX_PWR_LIMIT *) kalMemAlloc(sizeof(struct CHANNEL_TX_PWR_LIMIT) *
			(pTxPwrLimit->ucChNum), VIR_MEM_TYPE);

	if (!pTxPwrLimit->rChannelTxPwrLimit) {
		bRet = FALSE;
		DBGLOG(RLM, ERROR, "Alloc buffer for TxPwrLimit ch values failed\n");
		goto error;
	}

	kalMemSet(pTxPwrLimit->rChannelTxPwrLimit, MAX_TX_POWER,
		sizeof(struct CHANNEL_TX_PWR_LIMIT) * (pTxPwrLimit->ucChNum));

	if (pSetCmd_2g)
		for (ucIdx = 0; ucIdx < pSetCmd_2g->ucNum; ucIdx++) {
			pTxPwrLimit->rChannelTxPwrLimit[ucCnt].ucChannel =
				pSetCmd_2g->rChannelPowerLimit[ucIdx].ucCentralCh;
			ucCnt++;
		}

	if (pSetCmd_5g)
		for (ucIdx = 0; ucIdx < pSetCmd_5g->ucNum; ucIdx++) {
			pTxPwrLimit->rChannelTxPwrLimit[ucCnt].ucChannel =
				pSetCmd_5g->rChannelPowerLimit[ucIdx].ucCentralCh;
			ucCnt++;
		}

	bRet = rlmDomainTxPwrLimitLoadFromFile(prGlueInfo->prAdapter,
		country_code, pTxPwrLimit);

	if (bRet) {
		rlmDomainTxPwrLimitSetValues(pSetCmd_2g, pTxPwrLimit);
		rlmDomainTxPwrLimitSetValues(pSetCmd_5g, pTxPwrLimit);
	}

	kalMemFree(pTxPwrLimit->rChannelTxPwrLimit, VIR_MEM_TYPE,
		sizeof(CHANNEL_TX_PWR_LIMIT) * pTxPwrLimit->ucChNum);

error:
	kalMemFree(pTxPwrLimit, VIR_MEM_TYPE, sizeof(TX_PWR_LIMIT_DATA));

	return bRet;
}

#endif

#if CFG_SUPPORT_PWR_LIMIT_COUNTRY

/*----------------------------------------------------------------------------*/
/*!
* @brief
*
* @param[in]
*
* @return (fgValid) : 0 -> inValid, 1 -> Valid
*/
/*----------------------------------------------------------------------------*/
BOOLEAN
rlmDomainCheckPowerLimitValid(P_ADAPTER_T prAdapter,
			      COUNTRY_POWER_LIMIT_TABLE_CONFIGURATION rPowerLimitTableConfiguration,
			      UINT_8 ucPwrLimitNum)
{
	UINT_8 i;
	BOOLEAN fgValid = TRUE;
	PINT_8 prPwrLimit;

	prPwrLimit = &rPowerLimitTableConfiguration.aucPwrLimit[0];

	for (i = 0; i < ucPwrLimitNum; i++, prPwrLimit++) {
		if (*prPwrLimit > MAX_TX_POWER || *prPwrLimit < MIN_TX_POWER) {
			fgValid = FALSE;
			break;	/*Find out Wrong Power limit */
		}
	}
	return fgValid;

}

/*----------------------------------------------------------------------------*/
/*!
* @brief
*
* @param[in]
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID rlmDomainCheckCountryPowerLimitTable(P_ADAPTER_T prAdapter)
{
	UINT_8 i, j;
	UINT_16 u2CountryCodeTable, u2CountryCodeCheck;
	BOOLEAN fgChannelValid = FALSE;
	BOOLEAN fgPowerLimitValid = FALSE;
	BOOLEAN fgEntryRepetetion = FALSE;
	BOOLEAN fgTableValid = TRUE;

	/*Configuration Table Check */
	for (i = 0; i < sizeof(g_rRlmPowerLimitConfiguration) / sizeof(COUNTRY_POWER_LIMIT_TABLE_CONFIGURATION); i++) {
		/*Table Country Code */
		WLAN_GET_FIELD_BE16(&g_rRlmPowerLimitConfiguration[i].aucCountryCode[0], &u2CountryCodeTable);

		/*Repetition Entry Check */
		for (j = i + 1;
		     j < ARRAY_SIZE(g_rRlmPowerLimitConfiguration);
		     j++) {

			WLAN_GET_FIELD_BE16(&g_rRlmPowerLimitConfiguration[j].aucCountryCode[0], &u2CountryCodeCheck);
			if (((g_rRlmPowerLimitConfiguration[i].ucCentralCh) ==
			     g_rRlmPowerLimitConfiguration[j].ucCentralCh)
			    && (u2CountryCodeTable == u2CountryCodeCheck)) {
				fgEntryRepetetion = TRUE;
				DBGLOG(RLM, INFO, "Domain: Configuration Repetition CC=%c%c, Ch=%d\n",
				       g_rRlmPowerLimitConfiguration[i].aucCountryCode[0],
				       g_rRlmPowerLimitConfiguration[i].aucCountryCode[1],
				       g_rRlmPowerLimitConfiguration[i].ucCentralCh);
			}
		}

		/*Channel Number Check */
		fgChannelValid =
		    rlmDomainCheckChannelEntryValid(prAdapter, g_rRlmPowerLimitConfiguration[i].ucCentralCh);

		/*Power Limit Check */
		fgPowerLimitValid =
		    rlmDomainCheckPowerLimitValid(prAdapter, g_rRlmPowerLimitConfiguration[i], PWR_LIMIT_NUM);

		if (fgChannelValid == FALSE || fgPowerLimitValid == FALSE) {
			fgTableValid = FALSE;
			DBGLOG(RLM, INFO, "Domain: CC=%c%c, Ch=%d, Limit: %d,%d,%d,%d,%d\n",
			       g_rRlmPowerLimitConfiguration[i].aucCountryCode[0],
			       g_rRlmPowerLimitConfiguration[i].aucCountryCode[1],
			       g_rRlmPowerLimitConfiguration[i].ucCentralCh,
			       g_rRlmPowerLimitConfiguration[i].aucPwrLimit[PWR_LIMIT_CCK],
			       g_rRlmPowerLimitConfiguration[i].aucPwrLimit[PWR_LIMIT_20M],
			       g_rRlmPowerLimitConfiguration[i].aucPwrLimit[PWR_LIMIT_40M],
			       g_rRlmPowerLimitConfiguration[i].aucPwrLimit[PWR_LIMIT_80M],
			       g_rRlmPowerLimitConfiguration[i].aucPwrLimit[PWR_LIMIT_160M]);
		}

		if (u2CountryCodeTable == COUNTRY_CODE_NULL) {
			DBGLOG(RLM, INFO, "Domain: Full search down\n");
			break;	/*End of country table entry */
		}

	}

	if (fgEntryRepetetion == FALSE)
		DBGLOG(RLM, INFO, "Domain: Configuration Table no Repetiton.\n");

	/*Configuration Table no error */
	if (fgTableValid == TRUE)
		prAdapter->fgIsPowerLimitTableValid = TRUE;
	else
		prAdapter->fgIsPowerLimitTableValid = FALSE;

	/*Default Table Check */
	fgEntryRepetetion = FALSE;
	for (i = 0; i < sizeof(g_rRlmPowerLimitDefault) / sizeof(COUNTRY_POWER_LIMIT_TABLE_DEFAULT); i++) {

		WLAN_GET_FIELD_BE16(&g_rRlmPowerLimitDefault[i].aucCountryCode[0], &u2CountryCodeTable);

		for (j = i + 1; j < sizeof(g_rRlmPowerLimitDefault) / sizeof(COUNTRY_POWER_LIMIT_TABLE_DEFAULT); j++) {
			WLAN_GET_FIELD_BE16(&g_rRlmPowerLimitDefault[j].aucCountryCode[0], &u2CountryCodeCheck);
			if (u2CountryCodeTable == u2CountryCodeCheck) {
				fgEntryRepetetion = TRUE;
				DBGLOG(RLM, INFO,
				       "Domain: Default Repetition CC=%c%c\n",
				       g_rRlmPowerLimitDefault[j].aucCountryCode[0],
				       g_rRlmPowerLimitDefault[j].aucCountryCode[1]);
			}
		}
	}
	if (fgEntryRepetetion == FALSE)
		DBGLOG(RLM, INFO, "Domain: Default Table no Repetiton.\n");
}

/*----------------------------------------------------------------------------*/
/*!
* @brief
*
* @param[in]
*
* @return (u2TableIndex) : if  0xFFFF -> No Table Match
*/
/*----------------------------------------------------------------------------*/
UINT_16 rlmDomainPwrLimitDefaultTableDecision(P_ADAPTER_T prAdapter, UINT_16 u2CountryCode)
{

	UINT_16 i;
	UINT_16 u2CountryCodeTable = COUNTRY_CODE_NULL;
	UINT_16 u2TableIndex = POWER_LIMIT_TABLE_NULL;	/* No Table Match */

	/*Default Table Index */
	for (i = 0; i < sizeof(g_rRlmPowerLimitDefault) / sizeof(COUNTRY_POWER_LIMIT_TABLE_DEFAULT); i++) {

		WLAN_GET_FIELD_BE16(&g_rRlmPowerLimitDefault[i].aucCountryCode[0], &u2CountryCodeTable);

		if (u2CountryCodeTable == u2CountryCode) {
			u2TableIndex = i;
			break;	/*match country code */
		} else if (u2CountryCodeTable == COUNTRY_CODE_NULL) {
			u2TableIndex = i;
			break;	/*find last one country- Default */
		}
	}

	DBGLOG(RLM, INFO, "Domain: Default Table Index = %d\n", u2TableIndex);

	return u2TableIndex;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief
*
* @param[in]
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID rlmDomainBuildCmdByDefaultTable(P_CMD_SET_COUNTRY_CHANNEL_POWER_LIMIT_T prCmd, UINT_16 u2DefaultTableIndex)
{
	UINT_8 i, k;
	P_COUNTRY_POWER_LIMIT_TABLE_DEFAULT prPwrLimitSubBand;
	P_CMD_CHANNEL_POWER_LIMIT prCmdPwrLimit;

	prCmdPwrLimit = &prCmd->rChannelPowerLimit[0];
	prPwrLimitSubBand = &g_rRlmPowerLimitDefault[u2DefaultTableIndex];

	/*Build power limit cmd by default table information */

	for (i = POWER_LIMIT_2G4; i < POWER_LIMIT_SUBAND_NUM; i++) {
		if (prPwrLimitSubBand->aucPwrLimitSubBand[i] < MAX_TX_POWER) {
			for (k = g_rRlmSubBand[i].ucStartCh; k <= g_rRlmSubBand[i].ucEndCh;
			     k += g_rRlmSubBand[i].ucInterval) {
				if ((prPwrLimitSubBand->ucPwrUnit & BIT(i)) == 0) {
					prCmdPwrLimit->ucCentralCh = k;
					kalMemSet(&prCmdPwrLimit->cPwrLimitCCK,
						  prPwrLimitSubBand->aucPwrLimitSubBand[i], PWR_LIMIT_NUM);
					prCmdPwrLimit++;
					prCmd->ucNum++;

				} else {
					/* ex:    40MHz power limit(mW\MHz) = 20MHz power limit(mW\MHz) * 2
					 * ---> 40MHz power limit(dBm) = 20MHz power limit(dBm) + 6;
					 */
					prCmdPwrLimit->ucCentralCh = k;
					prCmdPwrLimit->cPwrLimitCCK = prPwrLimitSubBand->aucPwrLimitSubBand[i];
					prCmdPwrLimit->cPwrLimit20 = prPwrLimitSubBand->aucPwrLimitSubBand[i];
					prCmdPwrLimit->cPwrLimit40 = prPwrLimitSubBand->aucPwrLimitSubBand[i] + 6;
					if (prCmdPwrLimit->cPwrLimit40 > MAX_TX_POWER)
						prCmdPwrLimit->cPwrLimit40 = MAX_TX_POWER;
					prCmdPwrLimit->cPwrLimit80 = prPwrLimitSubBand->aucPwrLimitSubBand[i] + 12;
					if (prCmdPwrLimit->cPwrLimit80 > MAX_TX_POWER)
						prCmdPwrLimit->cPwrLimit80 = MAX_TX_POWER;
					prCmdPwrLimit->cPwrLimit160 = prPwrLimitSubBand->aucPwrLimitSubBand[i] + 18;
					if (prCmdPwrLimit->cPwrLimit160 > MAX_TX_POWER)
						prCmdPwrLimit->cPwrLimit160 = MAX_TX_POWER;
					prCmdPwrLimit++;
					prCmd->ucNum++;
				}
			}
		}
	}

#if 0
	if (prPwrLimitSubBand->aucPwrLimitSubBand[POWER_LIMIT_2G4] < MAX_TX_POWER) {
		for (i = BAND_2G4_LOWER_BOUND; i <= BAND_2G4_UPPER_BOUND; i++) {
			prCmdPwrLimit->ucCentralCh = i;
			kalMemSet(&prCmdPwrLimit->cPwrLimitCCK, prPwrLimitSubBand->aucPwrLimitSubBand[POWER_LIMIT_2G4],
				  PWR_LIMIT_NUM);
			prCmdPwrLimit++;
			prCmd->ucNum++;
		}
	}

	if (prPwrLimitSubBand->aucPwrLimitSubBand[POWER_LIMIT_UNII1] < MAX_TX_POWER) {
		if (prCmd->u2CountryCode != COUNTRY_CODE_KR) {
			for (i = UNII1_LOWER_BOUND; i <= UNII1_UPPER_BOUND; i += 2) {
				prCmdPwrLimit->ucCentralCh = i;
				kalMemSet(&prCmdPwrLimit->cPwrLimitCCK,
					  prPwrLimitSubBand->aucPwrLimitSubBand[POWER_LIMIT_UNII1], PWR_LIMIT_NUM);
				prCmdPwrLimit++;
				prCmd->ucNum++;
			}
		} else {
			for (i = UNII1_LOWER_BOUND; i <= UNII1_UPPER_BOUND; i += 2) {
				/* ex:    40MHz power limit(mW\MHz) = 20MHz power limit(mW\MHz) * 2
				 * ---> 40MHz power limit(dBm) = 20MHz power limit(dBm) + 6;
				 */
				prCmdPwrLimit->ucCentralCh = i;
				prCmdPwrLimit->cPwrLimitCCK =
				    g_rRlmPowerLimitDefault[u2DefaultTableIndex].cPwrLimitUnii1;
				prCmdPwrLimit->cPwrLimit20 =
				    g_rRlmPowerLimitDefault[u2DefaultTableIndex].cPwrLimitUnii1;
				prCmdPwrLimit->cPwrLimit40 =
				    g_rRlmPowerLimitDefault[u2DefaultTableIndex].cPwrLimitUnii1 + 6;
				prCmdPwrLimit->cPwrLimit80 =
				    g_rRlmPowerLimitDefault[u2DefaultTableIndex].cPwrLimitUnii1 + 12;
				prCmdPwrLimit->cPwrLimit160 =
				    g_rRlmPowerLimitDefault[u2DefaultTableIndex].cPwrLimitUnii1 + 18;
				prCmdPwrLimit++;
				prCmd->ucNum++;
			}
		}
	}

	if (prPwrLimitSubBand->aucPwrLimitSubBand[POWER_LIMIT_UNII2A] < MAX_TX_POWER) {
		for (i = UNII2A_LOWER_BOUND; i <= UNII2A_UPPER_BOUND; i += 2) {
			prCmdPwrLimit->ucCentralCh = i;
			kalMemSet(&prCmdPwrLimit->cPwrLimitCCK,
				  prPwrLimitSubBand->aucPwrLimitSubBand[POWER_LIMIT_UNII2A], PWR_LIMIT_NUM);
			prCmdPwrLimit++;
			prCmd->ucNum++;
		}
	}

	if (prPwrLimitSubBand->aucPwrLimitSubBand[POWER_LIMIT_UNII2C] < MAX_TX_POWER) {
		for (i = UNII2C_LOWER_BOUND; i <= UNII2C_UPPER_BOUND; i += 2) {
			prCmdPwrLimit->ucCentralCh = i;
			kalMemSet(&prCmdPwrLimit->cPwrLimitCCK,
				  prPwrLimitSubBand->aucPwrLimitSubBand[POWER_LIMIT_UNII2C], PWR_LIMIT_NUM);
			prCmdPwrLimit++;
			prCmd->ucNum++;
		}
	}
	if (prPwrLimitSubBand->aucPwrLimitSubBand[POWER_LIMIT_UNII3] < MAX_TX_POWER) {
		for (i = UNII3_LOWER_BOUND; i <= UNII3_UPPER_BOUND; i += 2) {
			prCmdPwrLimit->ucCentralCh = i;
			kalMemSet(&prCmdPwrLimit->cPwrLimitCCK,
				  prPwrLimitSubBand->aucPwrLimitSubBand[POWER_LIMIT_UNII3], PWR_LIMIT_NUM);
			prCmdPwrLimit++;
			prCmd->ucNum++;
		}
	}
#endif
}

/*----------------------------------------------------------------------------*/
/*!
* @brief
*
* @param[in]
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID rlmDomainBuildCmdByConfigTable(P_ADAPTER_T prAdapter, P_CMD_SET_COUNTRY_CHANNEL_POWER_LIMIT_T prCmd)
{
	UINT_8 i, k;
	UINT_16 u2CountryCodeTable = COUNTRY_CODE_NULL;
	P_CMD_CHANNEL_POWER_LIMIT prCmdPwrLimit;
	BOOLEAN fgChannelValid;

	/*Build power limit cmd by configuration table information */

	for (i = 0; i < sizeof(g_rRlmPowerLimitConfiguration) / sizeof(COUNTRY_POWER_LIMIT_TABLE_CONFIGURATION); i++) {

		WLAN_GET_FIELD_BE16(&g_rRlmPowerLimitConfiguration[i].aucCountryCode[0], &u2CountryCodeTable);

		fgChannelValid =
		    rlmDomainCheckChannelEntryValid(prAdapter, g_rRlmPowerLimitConfiguration[i].ucCentralCh);

		if (u2CountryCodeTable == COUNTRY_CODE_NULL) {
			DBGLOG(RLM, INFO, "Domain: full search configuration table done.\n");
			break;	/*end of configuration table */
		} else if ((u2CountryCodeTable == prCmd->u2CountryCode) && (fgChannelValid == TRUE)) {

			prCmdPwrLimit = &prCmd->rChannelPowerLimit[0];

			if (prCmd->ucNum != 0) {
				for (k = 0; k < prCmd->ucNum; k++) {
					if (prCmdPwrLimit->ucCentralCh ==
								g_rRlmPowerLimitConfiguration[i].ucCentralCh) {

						/*Cmd setting (Default table information) and
						 *  Configuration table has repetition channel entry,
						 *  ex : Default table (ex: 2.4G, limit = 20dBm) -->
						 *  ch1~14 limit =20dBm,
						 *  Configuration table (ex: ch1, limit = 22dBm) -->
						 *  ch 1 = 22 dBm
						 *  Cmd final setting -->  ch1 = 22dBm, ch12~14 = 20dBm
						 */
						kalMemCopy(&prCmdPwrLimit->cPwrLimitCCK,
							   &g_rRlmPowerLimitConfiguration[i].aucPwrLimit,
							   PWR_LIMIT_NUM);

						DBGLOG(RLM, INFO,
						       "Domain: CC=%c%c,ConfigCh=%d,Limit=%d,%d,%d,%d,%d,Fg=%d\n",
						       ((prCmd->u2CountryCode & 0xff00) >> 8),
						       (prCmd->u2CountryCode & 0x00ff), prCmdPwrLimit->ucCentralCh,
						       prCmdPwrLimit->cPwrLimitCCK, prCmdPwrLimit->cPwrLimit20,
						       prCmdPwrLimit->cPwrLimit40, prCmdPwrLimit->cPwrLimit80,
						       prCmdPwrLimit->cPwrLimit160, prCmdPwrLimit->ucFlag);

						break;
					}
					prCmdPwrLimit++;
				}
				if (k == prCmd->ucNum) {

					/*Full search cmd (Default table setting) no match channey,
					 *  ex : Default table (ex: 2.4G, limit = 20dBm) -->
					 *  ch1~14 limit =20dBm,
					 *  Configuration table (ex: ch36, limit = 22dBm) -->
					 *  ch 36 = 22 dBm
					 *  Cmd final setting -->  ch1~14 = 20dBm, ch36= 22dBm
					 */
					kalMemCopy(&prCmdPwrLimit->cPwrLimitCCK,
						   &g_rRlmPowerLimitConfiguration[i].aucPwrLimit, PWR_LIMIT_NUM);
					prCmd->ucNum++;

					DBGLOG(RLM, INFO,
					       "Domain: Full CC=%c%c,ConfigCh=%d,Limit=%d,%d,%d,%d,%d,Fg=%d\n",
					       ((prCmd->u2CountryCode & 0xff00) >> 8), (prCmd->u2CountryCode & 0x00ff),
					       prCmdPwrLimit->ucCentralCh, prCmdPwrLimit->cPwrLimitCCK,
					       prCmdPwrLimit->cPwrLimit20, prCmdPwrLimit->cPwrLimit40,
					       prCmdPwrLimit->cPwrLimit80, prCmdPwrLimit->cPwrLimit160,
					       prCmdPwrLimit->ucFlag);

				}
			} else {

				/*Default table power limit value are 63--> cmd table no channel entry
				 *  ex : Default table (ex: 2.4G, limit = 63Bm) --> no channel entry in cmd,
				 *  Configuration table (ex: ch36, limit = 22dBm) --> ch 36 = 22 dBm
				 *  Cmd final setting -->   ch36= 22dBm
				 */
				prCmdPwrLimit->ucCentralCh = g_rRlmPowerLimitConfiguration[i].ucCentralCh;
				kalMemCopy(&prCmdPwrLimit->cPwrLimitCCK, &g_rRlmPowerLimitConfiguration[i].aucPwrLimit,
					   PWR_LIMIT_NUM);
				prCmd->ucNum++;

				DBGLOG(RLM, INFO, "Domain: Default table power limit value are 63.\n");
				DBGLOG(RLM, INFO, "Domain: CC=%c%c,ConfigCh=%d,Limit=%d,%d,%d,%d,%d,Fg=%d\n",
				       ((prCmd->u2CountryCode & 0xff00) >> 8),
				       (prCmd->u2CountryCode & 0x00ff), prCmdPwrLimit->ucCentralCh,
				       prCmdPwrLimit->cPwrLimitCCK, prCmdPwrLimit->cPwrLimit20,
				       prCmdPwrLimit->cPwrLimit40, prCmdPwrLimit->cPwrLimit80,
				       prCmdPwrLimit->cPwrLimit160, prCmdPwrLimit->ucFlag);

			}
		}
	}
}

/*----------------------------------------------------------------------------*/
/*!
* @brief
*
* @param[in]
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID rlmDomainSendPwrLimitCmd_V2(P_ADAPTER_T prAdapter)
{
#if (CFG_SUPPORT_SINGLE_SKU == 1)
	WLAN_STATUS rStatus;
	UINT_32 u4SetQueryInfoLen;
	UINT_32 ch_cnt;
	struct wiphy *wiphy;
	u8 band_idx, ch_idx;
	P_CMD_SET_COUNTRY_CHANNEL_POWER_LIMIT_V2_T prCmd[KAL_NUM_BANDS] = {NULL};
	UINT_32 u4SetCmdTableMaxSize[KAL_NUM_BANDS] = {0};
	const INT_8 *prChannelList = NULL;


	DBGLOG(RLM, INFO, "rlmDomainSendPwrLimitCmd()\n");

	wiphy = priv_to_wiphy(prAdapter->prGlueInfo);
	for (band_idx = 0; band_idx < KAL_NUM_BANDS; band_idx++) {
		if (band_idx != KAL_BAND_2GHZ && band_idx != KAL_BAND_5GHZ)
			continue;

		prChannelList = (band_idx == KAL_BAND_2GHZ) ?
			gTx_Pwr_Limit_2g_Ch : gTx_Pwr_Limit_5g_Ch;

		ch_cnt = (band_idx == KAL_BAND_2GHZ) ? TX_PWR_LIMIT_2G_CH_NUM :
			TX_PWR_LIMIT_5G_CH_NUM;

		if (!ch_cnt)
			continue;

		u4SetCmdTableMaxSize[band_idx] =
			sizeof(CMD_SET_COUNTRY_CHANNEL_POWER_LIMIT_V2_T) +
			ch_cnt * sizeof(CMD_CHANNEL_POWER_LIMIT_V2);

		/* compile time assertion on firmware command structs */
		DATA_STRUCT_INSPECTING_ASSERT(sizeof(CMD_SET_COUNTRY_CHANNEL_POWER_LIMIT_V2_T) == 8);
		DATA_STRUCT_INSPECTING_ASSERT(sizeof(CMD_CHANNEL_POWER_LIMIT_V2) == 40);
		DATA_STRUCT_INSPECTING_ASSERT(
			OFFSET_OF(CMD_SET_COUNTRY_CHANNEL_POWER_LIMIT_V2_T, rChannelPowerLimit) ==
			sizeof(CMD_SET_COUNTRY_CHANNEL_POWER_LIMIT_V2_T));
		DATA_STRUCT_INSPECTING_ASSERT(
			OFFSET_OF(CMD_SET_COUNTRY_CHANNEL_POWER_LIMIT_V2_T, rChannelPowerLimit[1]) ==
			sizeof(CMD_SET_COUNTRY_CHANNEL_POWER_LIMIT_V2_T) + 1 * sizeof(CMD_CHANNEL_POWER_LIMIT_V2));
		DATA_STRUCT_INSPECTING_ASSERT(
			OFFSET_OF(CMD_SET_COUNTRY_CHANNEL_POWER_LIMIT_V2_T, rChannelPowerLimit[2]) ==
			sizeof(CMD_SET_COUNTRY_CHANNEL_POWER_LIMIT_V2_T) + 2 * sizeof(CMD_CHANNEL_POWER_LIMIT_V2));

		prCmd[band_idx] = cnmMemAlloc(prAdapter, RAM_TYPE_BUF, u4SetCmdTableMaxSize[band_idx]);


		if (!prCmd[band_idx]) {
			DBGLOG(RLM, ERROR, "Domain: no buf to send cmd\n");
			goto error;
		}

		/*initialize tw pwr table*/
		kalMemSet(prCmd[band_idx], MAX_TX_POWER, u4SetCmdTableMaxSize[band_idx]);

		prCmd[band_idx]->ucNum = ch_cnt;
		prCmd[band_idx]->eband = (band_idx == KAL_BAND_2GHZ) ? BAND_2G4 : BAND_5G;
		prCmd[band_idx]->countryCode = rlmDomainGetCountryCode();

		DBGLOG(RLM, INFO, "%s, active n_channels=%d, band=%d\n", __func__, ch_cnt, prCmd[band_idx]->eband);

		for (ch_idx = 0; ch_idx < ch_cnt; ch_idx++) {
			prCmd[band_idx]->rChannelPowerLimit[ch_idx].ucCentralCh =
				prChannelList[ch_idx];
		}
	}


	/*
	 * Get Max Tx Power from MT_TxPwrLimit.dat
	 */
	rlmDomainGetTxPwrLimit(rlmDomainGetCountryCode(),
							prAdapter->prGlueInfo,
							prCmd[KAL_BAND_2GHZ],
							prCmd[KAL_BAND_5GHZ]);


	for (band_idx = 0; band_idx < KAL_NUM_BANDS; band_idx++) {
		UINT_8 ucRemainChNum, i, ucTempChNum, prCmdBatchNum;
		UINT_32 u4BufSize = 0;
		P_CMD_SET_COUNTRY_CHANNEL_POWER_LIMIT_V2_T prTempCmd = NULL;
		ENUM_BAND_T eBand = (band_idx == KAL_BAND_2GHZ) ?
				BAND_2G4 : BAND_5G;

		if (!prCmd[band_idx])
			continue;

		ucRemainChNum = prCmd[band_idx]->ucNum;
		prCmdBatchNum =
			(ucRemainChNum + TX_PWR_LIMIT_CMD_CH_NUM_THRESHOLD - 1) /
			TX_PWR_LIMIT_CMD_CH_NUM_THRESHOLD;

		for (i = 0; i < prCmdBatchNum; i++) {
			if (i == prCmdBatchNum - 1)
				ucTempChNum = ucRemainChNum;
			else
				ucTempChNum = TX_PWR_LIMIT_CMD_CH_NUM_THRESHOLD;

			u4BufSize = sizeof(CMD_SET_COUNTRY_CHANNEL_POWER_LIMIT_V2_T) +
				ucTempChNum * sizeof(CMD_CHANNEL_POWER_LIMIT_V2);

			prTempCmd = cnmMemAlloc(prAdapter, RAM_TYPE_BUF, u4BufSize);

			if (!prTempCmd) {
				DBGLOG(RLM, ERROR, "Domain: no buf to send cmd\n");
				goto error;
			}

			/*copy partial tx pwr limit*/
			prTempCmd->ucNum = ucTempChNum;
			prTempCmd->eband = eBand;
			prTempCmd->countryCode = rlmDomainGetCountryCode();
			kalMemCopy(&prTempCmd->rChannelPowerLimit[0],
				&prCmd[band_idx]->rChannelPowerLimit[i * TX_PWR_LIMIT_CMD_CH_NUM_THRESHOLD],
				ucTempChNum * sizeof(CMD_CHANNEL_POWER_LIMIT_V2));

			u4SetQueryInfoLen = u4BufSize;
			/* Update tx max. power info to chip */
			rStatus = wlanSendSetQueryCmd(prAdapter,	/* prAdapter */
				      CMD_ID_SET_COUNTRY_POWER_LIMIT,	/* ucCID */
				      TRUE,	/* fgSetQuery */
				      FALSE,	/* fgNeedResp */
				      FALSE,	/* fgIsOid */
				      NULL,	/* pfCmdDoneHandler */
				      NULL,	/* pfCmdTimeoutHandler */
				      u4SetQueryInfoLen,	/* u4SetQueryInfoLen */
				      (PUINT_8) prTempCmd,	/* pucInfoBuffer */
				      NULL,	/* pvSetQueryBuffer */
				      0	/* u4SetQueryBufferLen */
					);

			cnmMemFree(prAdapter, prTempCmd);

			ucRemainChNum -= ucTempChNum;
		}
	}

error:
	for (band_idx = 0; band_idx < KAL_NUM_BANDS; band_idx++) {
		if (prCmd[band_idx])
			cnmMemFree(prAdapter, prCmd[band_idx]);
	}
#endif
}

VOID rlmDomainSendPwrLimitCmd(P_ADAPTER_T prAdapter)
{
	P_CMD_SET_COUNTRY_CHANNEL_POWER_LIMIT_T prCmd;
	WLAN_STATUS rStatus;
	UINT_8 i;
	UINT_16 u2DefaultTableIndex;
	UINT_32 u4SetCmdTableMaxSize;
	UINT_32 u4SetQueryInfoLen;
	P_CMD_CHANNEL_POWER_LIMIT prCmdPwrLimit;	/* for print usage */

	if (regd_is_single_sku_en())
		return rlmDomainSendPwrLimitCmd_V2(prAdapter);


	u4SetCmdTableMaxSize =
	    sizeof(CMD_SET_COUNTRY_CHANNEL_POWER_LIMIT_T) +
	    MAX_CMD_SUPPORT_CHANNEL_NUM * sizeof(CMD_CHANNEL_POWER_LIMIT);

	prCmd = cnmMemAlloc(prAdapter, RAM_TYPE_BUF, u4SetCmdTableMaxSize);

	if (!prCmd) {
		DBGLOG(RLM, ERROR, "Domain: no buf to send cmd\n");
		return;
	}
	kalMemZero(prCmd, u4SetCmdTableMaxSize);

	u2DefaultTableIndex =
	    rlmDomainPwrLimitDefaultTableDecision(prAdapter, prAdapter->rWifiVar.rConnSettings.u2CountryCode);

	if (u2DefaultTableIndex != POWER_LIMIT_TABLE_NULL) {

		WLAN_GET_FIELD_BE16(&g_rRlmPowerLimitDefault[u2DefaultTableIndex].aucCountryCode[0],
				    &prCmd->u2CountryCode);

		prCmd->ucNum = 0;

		if (prCmd->u2CountryCode != COUNTRY_CODE_NULL) {
			/*Command - default table information */
			rlmDomainBuildCmdByDefaultTable(prCmd, u2DefaultTableIndex);

			/*Command - configuration table information */
			rlmDomainBuildCmdByConfigTable(prAdapter, prCmd);
		}
	}
#if 0
	u4SetCmdTableMaxSize =
	    sizeof(CMD_SET_COUNTRY_CHANNEL_POWER_LIMIT_T) +
	    MAX_CMD_SUPPORT_CHANNEL_NUM * sizeof(CMD_CHANNEL_POWER_LIMIT);

	prCmd = cnmMemAlloc(prAdapter, RAM_TYPE_BUF, u4SetCmdTableMaxSize);
	ASSERT(prCmd);

	/* To do: exception handle */
	if (!prCmd) {
		DBGLOG(RLM, ERROR, "Domain: no buf to send cmd\n");
		return;
	}
	kalMemZero(prCmd, u4SetCmdTableMaxSize);	/* TODO memzero */

	if (u2TableIndex != POWER_LIMIT_TABLE_NULL && u2TableIndex < MAX_DEFAULT_TABLE_COUNTRY_NUM) {

		prCmd->u2CountryCode = (((UINT_16) g_rRlmCountryPowerLimitTable[u2TableIndex].aucCountryCode[0]) << 8) |
		    (((UINT_16) g_rRlmCountryPowerLimitTable[u2TableIndex].aucCountryCode[1]) & BITS(0, 7));
		prChPwrLimit = &g_rRlmCountryPowerLimitTable[u2TableIndex].rChannelPowerLimit[0];
		prCmdPwrLimit = &prCmd->rChannelPowerLimit[0];
		prCmd->ucNum = 0;
		for (i = 0; i < MAX_CMD_SUPPORT_CHANNEL_NUM; i++) {

			if (prChPwrLimit->ucCentralCh != ENDCH) {

				/*Check Power limit table channel efficient or not */
				fgChannelValid = rlmDomainCheckChannelEntryValid(prAdapter, prChPwrLimit->ucCentralCh);

				/*Cmd set up */
				if (fgChannelValid) {
					kalMemCopy(prCmdPwrLimit, prChPwrLimit, sizeof(CMD_CHANNEL_POWER_LIMIT));
					DBGLOG(RLM, INFO,
					       "Domain: ValidCh=%d,Limit=%d,%d,%d,%d,%d,Fg=%d\n",
					       prCmdPwrLimit->ucCentralCh, prCmdPwrLimit->cPwrLimitCCK,
					       prCmdPwrLimit->cPwrLimit20, prCmdPwrLimit->cPwrLimit40,
					       prCmdPwrLimit->cPwrLimit80, prCmdPwrLimit->cPwrLimit160,
					       prCmdPwrLimit->ucFlag);
					prCmd->ucNum++;
					prCmdPwrLimit++;
				} else {
					DBGLOG(RLM, INFO,
					       "Domain: Non-Ch=%d,Limit=%d,%d,%d,%d,%d,Fg=%d\n",
					       prChPwrLimit->ucCentralCh, prChPwrLimit->cPwrLimitCCK,
					       prChPwrLimit->cPwrLimit20, prChPwrLimit->cPwrLimit40,
					       prChPwrLimit->cPwrLimit80, prChPwrLimit->cPwrLimit160,
					       prChPwrLimit->ucFlag);
				}
				prChPwrLimit++;
			} else {
				/*End of the chanel entry */
				break;
			}
		};
	}
#endif

	if (prCmd->u2CountryCode != 0) {
		DBGLOG(RLM, INFO,
		       "Domain: ValidCC =%c%c, ChNum=%d\n", ((prCmd->u2CountryCode & 0xff00) >> 8),
		       (prCmd->u2CountryCode & 0x00ff), prCmd->ucNum);
	} else {
		DBGLOG(RLM, INFO, "Domain: ValidCC =0x%04x, ChNum=%d\n", prCmd->u2CountryCode, prCmd->ucNum);
	}
	prCmdPwrLimit = &prCmd->rChannelPowerLimit[0];

	for (i = 0; i < prCmd->ucNum; i++) {
		DBGLOG(RLM, INFO, "Domain: Ch=%d,Limit=%d,%d,%d,%d,%d,Fg=%d\n", prCmdPwrLimit->ucCentralCh,
		       prCmdPwrLimit->cPwrLimitCCK, prCmdPwrLimit->cPwrLimit20, prCmdPwrLimit->cPwrLimit40,
		       prCmdPwrLimit->cPwrLimit80, prCmdPwrLimit->cPwrLimit160, prCmdPwrLimit->ucFlag);
		prCmdPwrLimit++;
	}

	u4SetQueryInfoLen =
	    (sizeof(CMD_SET_COUNTRY_CHANNEL_POWER_LIMIT_T) + (prCmd->ucNum) * sizeof(CMD_CHANNEL_POWER_LIMIT));

	/* Update domain info to chip */
	if (prCmd->ucNum <= MAX_CMD_SUPPORT_CHANNEL_NUM) {
		rStatus = wlanSendSetQueryCmd(prAdapter,	/* prAdapter */
					      CMD_ID_SET_COUNTRY_POWER_LIMIT,	/* ucCID */
					      TRUE,	/* fgSetQuery */
					      FALSE,	/* fgNeedResp */
					      FALSE,	/* fgIsOid */
					      NULL,	/* pfCmdDoneHandler */
					      NULL,	/* pfCmdTimeoutHandler */
					      u4SetQueryInfoLen,	/* u4SetQueryInfoLen */
					      (PUINT_8) prCmd,	/* pucInfoBuffer */
					      NULL,	/* pvSetQueryBuffer */
					      0	/* u4SetQueryBufferLen */
		    );
	} else {
		DBGLOG(RLM, INFO, "Domain: illegal power limit table");
	}

	/* ASSERT(rStatus == WLAN_STATUS_PENDING); */

	cnmMemFree(prAdapter, prCmd);

}
#endif
BOOLEAN regd_is_single_sku_en(void)
{
#if (CFG_SUPPORT_SINGLE_SKU == 1)
	return g_mtk_regd_control.en;
#else
	return FALSE;
#endif
}

#if (CFG_SUPPORT_SINGLE_SKU == 1)
BOOLEAN rlmDomainIsCtrlStateEqualTo(enum regd_state state)
{
	return (g_mtk_regd_control.state == state) ? TRUE : FALSE;
}

enum regd_state rlmDomainGetCtrlState(void)
{
	return g_mtk_regd_control.state;
}


void rlmDomainResetActiveChannel(void)
{
	g_mtk_regd_control.n_channel_active_2g = 0;
	g_mtk_regd_control.n_channel_active_5g = 0;
}

void rlmDomainAddActiveChannel(u8 band)

{
	if (band == KAL_BAND_2GHZ)
		g_mtk_regd_control.n_channel_active_2g += 1;
	else if (band == KAL_BAND_5GHZ)
		g_mtk_regd_control.n_channel_active_5g += 1;
}

u8 rlmDomainGetActiveChannelCount(u8 band)
{
	if (band == KAL_BAND_2GHZ)
		return g_mtk_regd_control.n_channel_active_2g;
	else if (band == KAL_BAND_5GHZ)
		return g_mtk_regd_control.n_channel_active_5g;
	else
		return 0;
}

struct channel *rlmDomainGetActiveChannels(void)
{
	return g_mtk_regd_control.channels;
}

void rlmDomainSetDefaultCountryCode(void)
{
	g_mtk_regd_control.alpha2 = COUNTRY_CODE_WW;
}

void rlmDomainResetCtrlInfo(void)
{
	if (g_mtk_regd_control.state == REGD_STATE_UNDEFINED) {

		memset(&g_mtk_regd_control, 0, sizeof(mtk_regd_control));

		g_mtk_regd_control.state = REGD_STATE_INIT;

		rlmDomainSetDefaultCountryCode();

#if (CFG_SUPPORT_SINGLE_SKU_LOCAL_DB == 1)
		g_mtk_regd_control.flag |= REGD_CTRL_FLAG_SUPPORT_LOCAL_REGD_DB;
#endif
	}
}

BOOLEAN rlmDomainIsUsingLocalRegDomainDataBase(void)
{
#if (CFG_SUPPORT_SINGLE_SKU_LOCAL_DB == 1)
	return (g_mtk_regd_control.flag & REGD_CTRL_FLAG_SUPPORT_LOCAL_REGD_DB) ? TRUE : FALSE;
#else
	return FALSE;
#endif
}

bool rlmDomainIsSameCountryCode(char *alpha2, u8 size_of_alpha2)
{
	u8 idx;
	u32 alpha2_hex = 0;

	for (idx = 0; idx < size_of_alpha2; idx++)
		alpha2_hex |= (alpha2[idx] << (idx * 8));

	return (rlmDomainGetCountryCode() == alpha2_hex) ? TRUE : FALSE;
}

void rlmDomainSetCountryCode(char *alpha2, u8 size_of_alpha2)
{
	u8 max;
	u8 buf_size;

	buf_size = sizeof(g_mtk_regd_control.alpha2);
	max = (buf_size < size_of_alpha2) ? buf_size : size_of_alpha2;

	g_mtk_regd_control.alpha2 = rlmDomainAlpha2ToU32(alpha2, max);
}
void rlmDomainSetDfsRegion(enum nl80211_dfs_regions dfs_region)
{
	g_mtk_regd_control.dfs_region = dfs_region;
}

enum nl80211_dfs_regions rlmDomainGetDfsRegion(void)
{
	return g_mtk_regd_control.dfs_region;
}

void rlmDomainSetTempCountryCode(char *alpha2, u8 size_of_alpha2)
{
	u8 idx, max;
	u8 buf_size;

	buf_size = sizeof(g_mtk_regd_control.tmp_alpha2);
	max = (buf_size < size_of_alpha2) ? buf_size : size_of_alpha2;

	g_mtk_regd_control.tmp_alpha2 = 0;

	for (idx = 0; idx < max; idx++)
		g_mtk_regd_control.tmp_alpha2 |= (alpha2[idx] << (idx * 8));

}

enum regd_state rlmDomainStateTransition(enum regd_state request_state, struct regulatory_request *pRequest)
{
	enum regd_state next_state, old_state;
	bool the_same = 0;

	old_state = g_mtk_regd_control.state;
	next_state = REGD_STATE_INVALID;

	if (old_state == REGD_STATE_INVALID)
		DBGLOG(RLM, ERROR, "%s(): invalid state. trasntion is not allowed.\n", __func__);


	switch (request_state) {
	case REGD_STATE_SET_WW_CORE:
		if ((old_state == REGD_STATE_SET_WW_CORE) || (old_state == REGD_STATE_INIT)
		    || old_state == REGD_STATE_SET_COUNTRY_USER
		    || old_state == REGD_STATE_SET_COUNTRY_IE)
			next_state = request_state;

		break;

	case REGD_STATE_SET_COUNTRY_USER:
		/* Allow user to set multiple times */
		if ((old_state == REGD_STATE_SET_WW_CORE) || (old_state == REGD_STATE_INIT)
		    || old_state == REGD_STATE_SET_COUNTRY_USER
		    || old_state == REGD_STATE_SET_COUNTRY_IE)
			next_state = request_state;
		else
			DBGLOG(RLM, ERROR, "Invalid old state = %d\n", old_state);
		break;
	case REGD_STATE_SET_COUNTRY_DRIVER:
		if (old_state == REGD_STATE_SET_COUNTRY_USER) {
			/*
			 * Error.
			 * Mixing using set_country_by_user and set_country_by_driver
			 * is not allowed.
			 */
			break;
		}

		next_state = request_state;
		break;

	case REGD_STATE_SET_COUNTRY_IE:
		next_state = request_state;
		break;

	default:
		break;
	}

	if (next_state == REGD_STATE_INVALID) {
		DBGLOG(RLM, ERROR, "%s():  ERROR. trasntion to invalid state. o=%x, r=%x, s=%x\n",
				__func__, old_state, request_state, the_same);
	} else
		DBGLOG(RLM, INFO, "%s():  trasntion to state = %x (old = %x)\n",
		__func__, next_state, g_mtk_regd_control.state);

	g_mtk_regd_control.state = next_state;

	return g_mtk_regd_control.state;
}

void rlmDomainSetRefWiphy(struct wiphy *pWiphy)
{
	g_mtk_regd_control.pRefWiphy = pWiphy;
}

struct wiphy *rlmDomainGetRefWiphy(void)
{
	return g_mtk_regd_control.pRefWiphy;
}

/**
 * rlmDomainChannelFlagString - Transform channel flags to readable string
 *
 * @ flags: the ieee80211_channel->flags for a channel
 * @ buf: string buffer to put the transformed string
 * @ buf_size: size of the buf
 **/
void rlmDomainChannelFlagString(u32 flags, char *buf, size_t buf_size)
{
	INT_32 buf_written = 0;

	if (!flags || !buf || !buf_size)
		return;

	if (flags & IEEE80211_CHAN_DISABLED) {
		LOGBUF(buf, ((INT_32)buf_size), buf_written, "DISABLED ");
		/* If DISABLED, don't need to check other flags */
		return;
	}
	if (flags & IEEE80211_CHAN_PASSIVE_FLAG)
		LOGBUF(buf, ((INT_32)buf_size), buf_written, IEEE80211_CHAN_PASSIVE_STR " ");
	if (flags & IEEE80211_CHAN_RADAR)
		LOGBUF(buf, ((INT_32)buf_size), buf_written, "RADAR ");
	if (flags & IEEE80211_CHAN_NO_HT40PLUS)
		LOGBUF(buf, ((INT_32)buf_size), buf_written, "NO_HT40PLUS ");
	if (flags & IEEE80211_CHAN_NO_HT40MINUS)
		LOGBUF(buf, ((INT_32)buf_size), buf_written, "NO_HT40MINUS ");
	if (flags & IEEE80211_CHAN_NO_80MHZ)
		LOGBUF(buf, ((INT_32)buf_size), buf_written, "NO_80MHZ ");
	if (flags & IEEE80211_CHAN_NO_160MHZ)
		LOGBUF(buf, ((INT_32)buf_size), buf_written, "NO_160MHZ ");
}

void rlmDomainParsingChannel(IN struct wiphy *pWiphy)
{
	u32 band_idx, ch_idx;
	u32 ch_count;
	struct ieee80211_supported_band *sband;
	struct ieee80211_channel *chan;
	struct channel *pCh;
	char chan_flag_string[64] = {0};
	P_GLUE_INFO_T prGlueInfo;
	UINT_8 ucChannelNum = 0;
	BOOLEAN fgDisconnection = FALSE;
	WLAN_STATUS rStatus;
	UINT_32 u4BufLen;


	if (!pWiphy) {
		DBGLOG(RLM, ERROR, "%s():  ERROR. pWiphy = NULL.\n", __func__);
		ASSERT(0);
		return;
	}

	/* Retrieve connected channel */
	prGlueInfo = rlmDomainGetGlueInfo();
	if (prGlueInfo && kalGetMediaStateIndicated(prGlueInfo) == PARAM_MEDIA_STATE_CONNECTED) {
		ucChannelNum = wlanGetChannelNumberByNetwork(prGlueInfo->prAdapter,
							     prGlueInfo->prAdapter->prAisBssInfo->ucBssIndex);
	}

	/*
	 * Ready to parse the channel for bands
	 */

	rlmDomainResetActiveChannel();
	rlmDomainSetRefWiphy(pWiphy);

	ch_count = 0;
	for (band_idx = 0; band_idx < KAL_NUM_BANDS; band_idx++) {
		sband = pWiphy->bands[band_idx];
		if (!sband)
			continue;

		for (ch_idx = 0; ch_idx < sband->n_channels; ch_idx++) {
			chan = &sband->channels[ch_idx];
			pCh = (rlmDomainGetActiveChannels() + ch_count);
			/* Parse flags and get readable string */
			rlmDomainChannelFlagString(chan->flags, chan_flag_string, sizeof(chan_flag_string));

			if (chan->flags & IEEE80211_CHAN_DISABLED) {
				DBGLOG(RLM, INFO, "channels[%d][%d]: ch%d (freq = %d) flags=0x%x [ %s]\n",
				    band_idx, ch_idx, chan->hw_value, chan->center_freq, chan->flags,
				    chan_flag_string);

				/* Disconnect AP in the end of this function*/
				if (chan->hw_value == ucChannelNum)
					fgDisconnection = TRUE;

				continue;
			}

			/* Allowable channel */
			if (ch_count == MAX_SUPPORTED_CH_COUNT) {
				DBGLOG(RLM, ERROR, "%s(): no buffer to store channel information.\n", __func__);
				break;
			}

			rlmDomainAddActiveChannel(band_idx);

			DBGLOG(RLM, INFO, "channels[%d][%d]: ch%d (freq = %d) flgs=0x%x [ %s]\n",
				band_idx, ch_idx, chan->hw_value, chan->center_freq, chan->flags,
				chan_flag_string);

			pCh->chNum = chan->hw_value;
			pCh->flags = chan->flags;

			ch_count += 1;
		}

	}

	/* Disconnect with AP if connected channel is disabled in new country */
	if (fgDisconnection) {
		DBGLOG(RLM, STATE, "Disconnect! CH%d is DISABLED in this country\n", ucChannelNum);
		rStatus = kalIoctl(prGlueInfo, wlanoidSetDisassociate, NULL, 0, FALSE, FALSE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(RLM, WARN, "disassociate error:%x\n", rStatus);
	}
}
void rlmExtractChannelInfo(u32 max_ch_count, struct acctive_channel_list *prBuff)
{
	u32 ch_count, idx;
	struct channel *pCh;

	prBuff->n_channels_2g = rlmDomainGetActiveChannelCount(KAL_BAND_2GHZ);
	prBuff->n_channels_5g = rlmDomainGetActiveChannelCount(KAL_BAND_5GHZ);
	ch_count = prBuff->n_channels_2g + prBuff->n_channels_5g;

	if (ch_count > max_ch_count) {
		ch_count = max_ch_count;
		DBGLOG(RLM, WARN, "%s(); active channel list is not a complete one.\n", __func__);
	}

	for (idx = 0; idx < ch_count; idx++) {
		pCh = &(prBuff->channels[idx]);

		pCh->chNum = (rlmDomainGetActiveChannels() + idx)->chNum;
		pCh->flags = (rlmDomainGetActiveChannels() + idx)->flags;
	}

}

const struct ieee80211_regdomain *rlmDomainSearchRegdomainFromLocalDataBase(char *alpha2)
{
#if (CFG_SUPPORT_SINGLE_SKU_LOCAL_DB == 1)
	u8 idx;
	const struct mtk_regdomain *prRegd;

	idx = 0;
	while (g_prRegRuleTable[idx]) {
		prRegd = g_prRegRuleTable[idx];

		if ((prRegd->country_code[0] == alpha2[0]) &&
			(prRegd->country_code[1] == alpha2[1]) &&
			(prRegd->country_code[2] == alpha2[2]) &&
			(prRegd->country_code[3] == alpha2[3]))
			return prRegd->prRegdRules;

		idx++;
	}

	DBGLOG(RLM, ERROR, "%s(): Error, Cannot find the correct RegDomain. country = %s.\n",
			__func__, alpha2);
	DBGLOG(RLM, INFO, "    Set as default WW.\n");

	return &default_regdom_ww; /*default world wide*/
#else
	return NULL;
#endif
}


const struct ieee80211_regdomain *rlmDomainGetLocalDefaultRegd(void)
{
#if (CFG_SUPPORT_SINGLE_SKU_LOCAL_DB == 1)
	return &default_regdom_ww;
#else
	return NULL;
#endif
}
P_GLUE_INFO_T rlmDomainGetGlueInfo(void)
{
	return g_mtk_regd_control.pGlueInfo;
}

bool rlmDomainIsEfuseUsed(void)
{
	return g_mtk_regd_control.isEfuseCountryCodeUsed;
}

UINT_8 rlmDomainGetChannelBw(UINT_8 channelNum)
{
	UINT_32 ch_idx = 0, start_idx = 0, end_idx = 0;
	UINT_8 channelBw = MAX_BW_80_80_MHZ;
	struct channel *pCh;

	end_idx = rlmDomainGetActiveChannelCount(KAL_BAND_2GHZ)
			+ rlmDomainGetActiveChannelCount(KAL_BAND_5GHZ);

	for (ch_idx = start_idx; ch_idx < end_idx; ch_idx++) {
		pCh = (rlmDomainGetActiveChannels() + ch_idx);

		if (pCh->chNum != channelNum)
			continue;

		/* Max BW */
		if ((pCh->flags & IEEE80211_CHAN_NO_160MHZ)
						== IEEE80211_CHAN_NO_160MHZ)
			channelBw = MAX_BW_80MHZ;
		if ((pCh->flags & IEEE80211_CHAN_NO_80MHZ)
						== IEEE80211_CHAN_NO_80MHZ)
			channelBw = MAX_BW_40MHZ;
		if ((pCh->flags & IEEE80211_CHAN_NO_HT40)
						== IEEE80211_CHAN_NO_HT40)
			channelBw = MAX_BW_20MHZ;
	}

	DBGLOG(RLM, INFO, "ch=%d, BW=%d\n", channelNum, channelBw);
	return channelBw;
}
#endif

WLAN_STATUS rlmDomainExtractSingleSkuInfoFromFirmware(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucEventBuf)
{
#if (CFG_SUPPORT_SINGLE_SKU == 1)
	P_SINGLE_SKU_INFO prSkuInfo = (P_SINGLE_SKU_INFO)pucEventBuf;

	g_mtk_regd_control.en = TRUE;

	if (prSkuInfo->isEfuseValid) {
		if (!rlmDomainIsUsingLocalRegDomainDataBase()) {

			DBGLOG(RLM, ERROR, "%s(): Error. In efuse mode, must use local data base.\n", __func__);

			ASSERT(0);
			return WLAN_STATUS_NOT_SUPPORTED; /*force using local db if getting country code from efuse*/
		}

		rlmDomainSetCountryCode((char *)&prSkuInfo->u4EfuseCountryCode, sizeof(prSkuInfo->u4EfuseCountryCode));
		g_mtk_regd_control.isEfuseCountryCodeUsed = TRUE;

	}
#endif

	return WLAN_STATUS_SUCCESS;
}

void rlmDomainSendInfoToFirmware(IN P_ADAPTER_T prAdapter)
{
#if (CFG_SUPPORT_SINGLE_SKU == 1)
	struct regulatory_request request;
	struct regulatory_request *prReq = NULL;

	if (!regd_is_single_sku_en())
		return; /*not support single sku*/

	if (g_mtk_regd_control.isEfuseCountryCodeUsed) {
		request.initiator = NL80211_REGDOM_SET_BY_DRIVER;
		prReq = &request;
	}

	g_mtk_regd_control.pGlueInfo = prAdapter->prGlueInfo;
	mtk_reg_notify(priv_to_wiphy(prAdapter->prGlueInfo), prReq);
#endif
}

ENUM_CHNL_EXT_T rlmSelectSecondaryChannelType(P_ADAPTER_T prAdapter, ENUM_BAND_T band, u8 primary_ch)
{
#if (CFG_SUPPORT_SINGLE_SKU == 1)
	u8 below_ch, above_ch;

	below_ch = primary_ch - CHNL_SPAN_20;
	above_ch = primary_ch + CHNL_SPAN_20;

	if (rlmDomainIsLegalChannel(prAdapter, band, above_ch))
		return CHNL_EXT_SCA;

	if (rlmDomainIsLegalChannel(prAdapter, band, below_ch))
		return CHNL_EXT_SCB;

#endif

	return CHNL_EXT_SCN;
}

void rlmDomainOidSetCountry(IN P_ADAPTER_T prAdapter, char *country, u8 size_of_country)
{
#if (CFG_SUPPORT_SINGLE_SKU == 1)
	struct regulatory_request request;

	if (rlmDomainIsUsingLocalRegDomainDataBase()) {
		rlmDomainSetTempCountryCode(country, size_of_country);
		request.initiator = NL80211_REGDOM_SET_BY_DRIVER;
		mtk_reg_notify(priv_to_wiphy(prAdapter->prGlueInfo), &request);
	} else {
		DBGLOG(RLM, INFO, "%s(): Using driver hint to query CRDA getting regd.\n", __func__);
		regulatory_hint(priv_to_wiphy(prAdapter->prGlueInfo), country);
	}
#endif
}

u32 rlmDomainGetCountryCode(void)
{
#if (CFG_SUPPORT_SINGLE_SKU == 1)
	return g_mtk_regd_control.alpha2;
#else
	return 0;
#endif
}

u32 rlmDomainGetTempCountryCode(void)
{
#if (CFG_SUPPORT_SINGLE_SKU == 1)
	return g_mtk_regd_control.tmp_alpha2;
#else
	return 0;
#endif
}

void rlmDomainAssert(BOOLEAN cond)
{
	/* bypass this check because single sku is not enable */
	if (!regd_is_single_sku_en())
		return;

	if (!cond) {
		WARN_ON(1);
		DBGLOG(RLM, ERROR, "[WARNING!!] RLM unexpected case.\n");
	}

}


