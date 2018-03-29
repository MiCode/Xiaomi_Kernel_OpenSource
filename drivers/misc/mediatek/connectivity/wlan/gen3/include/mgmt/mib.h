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

/*
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/include/mgmt/mib.h#1
*/

/*! \file  mib.h
 *  \brief This file contains the IEEE 802.11 family related MIB definition
 *         for MediaTek 802.11 Wireless LAN Adapters.
 */

#ifndef _MIB_H
#define _MIB_H

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 ********************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 ********************************************************************************
 */

/*******************************************************************************
 *                              C O N S T A N T S
 ********************************************************************************
 */

/*******************************************************************************
 *                         D A T A   T Y P E S
 ********************************************************************************
 */
/* Entry in SMT AuthenticationAlgorithms Table: dot11AuthenticationAlgorithmsEntry */
typedef struct _DOT11_AUTHENTICATION_ALGORITHMS_ENTRY {
	BOOLEAN dot11AuthenticationAlgorithmsEnable;	/* dot11AuthenticationAlgorithmsEntry 3 */
} DOT11_AUTHENTICATION_ALGORITHMS_ENTRY, *P_DOT11_AUTHENTICATION_ALGORITHMS_ENTRY;

/* Entry in SMT dot11RSNAConfigPairwiseCiphersTalbe Table: dot11RSNAConfigPairwiseCiphersEntry */
typedef struct _DOT11_RSNA_CONFIG_PAIRWISE_CIPHERS_ENTRY {
	UINT_32 dot11RSNAConfigPairwiseCipher;	/* dot11RSNAConfigPairwiseCiphersEntry 2 */
	BOOLEAN dot11RSNAConfigPairwiseCipherEnabled;	/* dot11RSNAConfigPairwiseCiphersEntry 3 */
} DOT11_RSNA_CONFIG_PAIRWISE_CIPHERS_ENTRY, *P_DOT11_RSNA_CONFIG_PAIRWISE_CIPHERS_ENTRY;

/* Entry in SMT dot11RSNAConfigAuthenticationSuitesTalbe Table: dot11RSNAConfigAuthenticationSuitesEntry */
typedef struct _DOT11_RSNA_CONFIG_AUTHENTICATION_SUITES_ENTRY {
	UINT_32 dot11RSNAConfigAuthenticationSuite;	/* dot11RSNAConfigAuthenticationSuitesEntry 2 */
	BOOLEAN dot11RSNAConfigAuthenticationSuiteEnabled;	/* dot11RSNAConfigAuthenticationSuitesEntry 3 */
} DOT11_RSNA_CONFIG_AUTHENTICATION_SUITES_ENTRY, *P_DOT11_RSNA_CONFIG_AUTHENTICATION_SUITES_ENTRY;

/* ----- IEEE 802.11 MIB Major sections ----- */
typedef struct _IEEE_802_11_MIB_T {
	/* dot11PrivacyTable                            (dot11smt 5) */
	UINT_8 dot11WEPDefaultKeyID;	/* dot11PrivacyEntry 2 */
	BOOLEAN dot11TranmitKeyAvailable;
	UINT_32 dot11WEPICVErrorCount;	/* dot11PrivacyEntry 5 */
	UINT_32 dot11WEPExcludedCount;	/* dot11PrivacyEntry 6 */

	/* dot11RSNAConfigTable                         (dot11smt 8) */
	UINT_32 dot11RSNAConfigGroupCipher;	/* dot11RSNAConfigEntry 4 */

	/* dot11RSNAConfigPairwiseCiphersTable          (dot11smt 9) */
	DOT11_RSNA_CONFIG_PAIRWISE_CIPHERS_ENTRY dot11RSNAConfigPairwiseCiphersTable[MAX_NUM_SUPPORTED_CIPHER_SUITES];

	/* dot11RSNAConfigAuthenticationSuitesTable     (dot11smt 10) */
	 DOT11_RSNA_CONFIG_AUTHENTICATION_SUITES_ENTRY
	    dot11RSNAConfigAuthenticationSuitesTable[MAX_NUM_SUPPORTED_AKM_SUITES];

#if 0				/* SUPPORT_WAPI */
	BOOLEAN fgWapiKeyInstalled;
	PARAM_WPI_KEY_T rWapiPairwiseKey[2];
	BOOLEAN fgPairwiseKeyUsed[2];
	UINT_8 ucWpiActivedPWKey;	/* Must be 0 or 1, by wapi spec */
	PARAM_WPI_KEY_T rWapiGroupKey[2];
	BOOLEAN fgGroupKeyUsed[2];
#endif
} IEEE_802_11_MIB_T, *P_IEEE_802_11_MIB_T;

/* ------------------ IEEE 802.11 non HT PHY characteristics ---------------- */
typedef const struct _NON_HT_PHY_ATTRIBUTE_T {
	UINT_16 u2SupportedRateSet;

	BOOLEAN fgIsShortPreambleOptionImplemented;

	BOOLEAN fgIsShortSlotTimeOptionImplemented;
} NON_HT_PHY_ATTRIBUTE_T, *P_NON_HT_PHY_ATTRIBUTE_T;

typedef const struct _NON_HT_ADHOC_MODE_ATTRIBUTE_T {
	ENUM_PHY_TYPE_INDEX_T ePhyTypeIndex;

	UINT_16 u2BSSBasicRateSet;
} NON_HT_ADHOC_MODE_ATTRIBUTE_T, *P_NON_HT_ADHOC_MODE_ATTRIBUTE_T;

typedef NON_HT_ADHOC_MODE_ATTRIBUTE_T NON_HT_AP_MODE_ATTRIBUTE_T;

/*******************************************************************************
 *                            P U B L I C   D A T A
 ********************************************************************************
 */
extern NON_HT_PHY_ATTRIBUTE_T rNonHTPhyAttributes[];
extern NON_HT_ADHOC_MODE_ATTRIBUTE_T rNonHTAdHocModeAttributes[];
extern NON_HT_AP_MODE_ATTRIBUTE_T rNonHTApModeAttributes[];

/*******************************************************************************
 *                           P R I V A T E   D A T A
 ********************************************************************************
 */

/*******************************************************************************
 *                                 M A C R O S
 ********************************************************************************
 */

/*******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
 ********************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 ********************************************************************************
 */

#endif /* _MIB_H */
