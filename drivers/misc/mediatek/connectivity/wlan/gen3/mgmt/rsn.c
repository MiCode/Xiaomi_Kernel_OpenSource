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
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/mgmt/rsn.c#3
*/

/*! \file   "rsn.c"
    \brief  This file including the 802.11i, wpa and wpa2(rsn) related function.

    This file provided the macros and functions library support the wpa/rsn ie parsing,
    cipher and AKM check to help the AP seleced deciding, tkip mic error handler and rsn PMKID support.
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
* \brief This routine is called to parse RSN IE.
*
* \param[in]  prInfoElem Pointer to the RSN IE
* \param[out] prRsnInfo Pointer to the BSSDescription structure to store the
**                  RSN information from the given RSN IE
*
* \retval TRUE - Succeeded
* \retval FALSE - Failed
*/
/*----------------------------------------------------------------------------*/
BOOLEAN rsnParseRsnIE(IN P_ADAPTER_T prAdapter, IN P_RSN_INFO_ELEM_T prInfoElem, OUT P_RSN_INFO_T prRsnInfo)
{
	UINT_32 i;
	INT_32 u4RemainRsnIeLen;
	UINT_16 u2Version;
	UINT_16 u2Cap = 0;
	UINT_32 u4GroupSuite = RSN_CIPHER_SUITE_CCMP;
	UINT_16 u2PairSuiteCount = 0;
	UINT_16 u2AuthSuiteCount = 0;
	PUINT_8 pucPairSuite = NULL;
	PUINT_8 pucAuthSuite = NULL;
	PUINT_8 cp;

	DEBUGFUNC("rsnParseRsnIE");

	ASSERT(prInfoElem);
	ASSERT(prRsnInfo);

	/* Verify the length of the RSN IE. */
	if (prInfoElem->ucLength < 2) {
		DBGLOG(RSN, TRACE, "RSN IE length too short (length=%d)\n", prInfoElem->ucLength);
		return FALSE;
	}

	/* Check RSN version: currently, we only support version 1. */
	WLAN_GET_FIELD_16(&prInfoElem->u2Version, &u2Version);
	if (u2Version != 1) {
		DBGLOG(RSN, TRACE, "Unsupported RSN IE version: %d\n", u2Version);
		return FALSE;
	}

	cp = (PUCHAR)&prInfoElem->u4GroupKeyCipherSuite;
	u4RemainRsnIeLen = (INT_32) prInfoElem->ucLength - 2;

	do {
		if (u4RemainRsnIeLen == 0)
			break;

		/* Parse the Group Key Cipher Suite field. */
		if (u4RemainRsnIeLen < 4) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse RSN IE in group cipher suite (IE len: %d)\n", prInfoElem->ucLength);
			return FALSE;
		}

		WLAN_GET_FIELD_32(cp, &u4GroupSuite);
		cp += 4;
		u4RemainRsnIeLen -= 4;

		if (u4RemainRsnIeLen == 0)
			break;

		/* Parse the Pairwise Key Cipher Suite Count field. */
		if (u4RemainRsnIeLen < 2) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse RSN IE in pairwise cipher suite count (IE len: %d)\n",
				prInfoElem->ucLength);
			return FALSE;
		}

		WLAN_GET_FIELD_16(cp, &u2PairSuiteCount);
		cp += 2;
		u4RemainRsnIeLen -= 2;

		/* Parse the Pairwise Key Cipher Suite List field. */
		i = (UINT_32) u2PairSuiteCount * 4;
		if (u4RemainRsnIeLen < (INT_32) i) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse RSN IE in pairwise cipher suite list (IE len: %d)\n",
				prInfoElem->ucLength);
			return FALSE;
		}

		pucPairSuite = cp;

		cp += i;
		u4RemainRsnIeLen -= (INT_32) i;

		if (u4RemainRsnIeLen == 0)
			break;

		/* Parse the Authentication and Key Management Cipher Suite Count field. */
		if (u4RemainRsnIeLen < 2) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse RSN IE in auth & key mgt suite count (IE len: %d)\n",
				prInfoElem->ucLength);
			return FALSE;
		}

		WLAN_GET_FIELD_16(cp, &u2AuthSuiteCount);
		cp += 2;
		u4RemainRsnIeLen -= 2;

		/* Parse the Authentication and Key Management Cipher Suite List
		   field. */
		i = (UINT_32) u2AuthSuiteCount * 4;
		if (u4RemainRsnIeLen < (INT_32) i) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse RSN IE in auth & key mgt suite list (IE len: %d)\n",
				prInfoElem->ucLength);
			return FALSE;
		}

		pucAuthSuite = cp;

		cp += i;
		u4RemainRsnIeLen -= (INT_32) i;

		if (u4RemainRsnIeLen == 0)
			break;

		/* Parse the RSN u2Capabilities field. */
		if (u4RemainRsnIeLen < 2) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse RSN IE in RSN capabilities (IE len: %d)\n", prInfoElem->ucLength);
			return FALSE;
		}

		WLAN_GET_FIELD_16(cp, &u2Cap);
	} while (FALSE);

	/* Save the RSN information for the BSS. */
	prRsnInfo->ucElemId = ELEM_ID_RSN;

	prRsnInfo->u2Version = u2Version;

	prRsnInfo->u4GroupKeyCipherSuite = u4GroupSuite;

	DBGLOG(RSN, LOUD, "RSN: version %d, group key cipher suite %02x-%02x-%02x-%02x\n",
			   u2Version, (UCHAR) (u4GroupSuite & 0x000000FF),
			   (UCHAR) ((u4GroupSuite >> 8) & 0x000000FF),
			   (UCHAR) ((u4GroupSuite >> 16) & 0x000000FF), (UCHAR) ((u4GroupSuite >> 24) & 0x000000FF));

	if (pucPairSuite) {
		/* The information about the pairwise key cipher suites is present. */
		if (u2PairSuiteCount > MAX_NUM_SUPPORTED_CIPHER_SUITES)
			u2PairSuiteCount = MAX_NUM_SUPPORTED_CIPHER_SUITES;

		prRsnInfo->u4PairwiseKeyCipherSuiteCount = (UINT_32) u2PairSuiteCount;

		for (i = 0; i < (UINT_32) u2PairSuiteCount; i++) {
			WLAN_GET_FIELD_32(pucPairSuite, &prRsnInfo->au4PairwiseKeyCipherSuite[i]);
			pucPairSuite += 4;

			DBGLOG(RSN, LOUD,
			       "RSN: pairwise key cipher suite [%d]: %02x-%02x-%02x-%02x\n",
				(UINT_8) i,
				(UCHAR) (prRsnInfo->au4PairwiseKeyCipherSuite[i] & 0x000000FF),
				(UCHAR) ((prRsnInfo->au4PairwiseKeyCipherSuite[i] >> 8) & 0x000000FF),
				(UCHAR) ((prRsnInfo->au4PairwiseKeyCipherSuite[i] >> 16) & 0x000000FF),
				(UCHAR) ((prRsnInfo->au4PairwiseKeyCipherSuite[i] >> 24) & 0x000000FF));
		}
	} else {
		/* The information about the pairwise key cipher suites is not present.
		   Use the default chipher suite for RSN: CCMP. */
		prRsnInfo->u4PairwiseKeyCipherSuiteCount = 1;
		prRsnInfo->au4PairwiseKeyCipherSuite[0] = RSN_CIPHER_SUITE_CCMP;

		DBGLOG(RSN, LOUD,
		       "RSN: pairwise key cipher suite: %02x-%02x-%02x-%02x (default)\n",
			(UCHAR) (prRsnInfo->au4PairwiseKeyCipherSuite[0] & 0x000000FF),
			(UCHAR) ((prRsnInfo->au4PairwiseKeyCipherSuite[0] >> 8) & 0x000000FF),
			(UCHAR) ((prRsnInfo->au4PairwiseKeyCipherSuite[0] >> 16) & 0x000000FF),
			(UCHAR) ((prRsnInfo->au4PairwiseKeyCipherSuite[0] >> 24) & 0x000000FF));
	}

	if (pucAuthSuite) {
		/* The information about the authentication and key management suites
		   is present. */
		if (u2AuthSuiteCount > MAX_NUM_SUPPORTED_AKM_SUITES)
			u2AuthSuiteCount = MAX_NUM_SUPPORTED_AKM_SUITES;

		prRsnInfo->u4AuthKeyMgtSuiteCount = (UINT_32) u2AuthSuiteCount;

		for (i = 0; i < (UINT_32) u2AuthSuiteCount; i++) {
			WLAN_GET_FIELD_32(pucAuthSuite, &prRsnInfo->au4AuthKeyMgtSuite[i]);
			pucAuthSuite += 4;

			DBGLOG(RSN, LOUD, "RSN: AKM suite [%d]: %02x-%02x-%02x-%02x\n",
					   (UINT_8) i,
					   (UCHAR) (prRsnInfo->au4AuthKeyMgtSuite[i] & 0x000000FF),
					   (UCHAR) ((prRsnInfo->au4AuthKeyMgtSuite[i] >> 8) & 0x000000FF),
					   (UCHAR) ((prRsnInfo->au4AuthKeyMgtSuite[i] >> 16) & 0x000000FF),
					   (UCHAR) ((prRsnInfo->au4AuthKeyMgtSuite[i] >> 24) & 0x000000FF));
		}
	} else {
		/* The information about the authentication and key management suites
		   is not present. Use the default AKM suite for RSN. */
		prRsnInfo->u4AuthKeyMgtSuiteCount = 1;
		prRsnInfo->au4AuthKeyMgtSuite[0] = RSN_AKM_SUITE_802_1X;

		DBGLOG(RSN, LOUD, "RSN: AKM suite: %02x-%02x-%02x-%02x (default)\n",
				   (UCHAR) (prRsnInfo->au4AuthKeyMgtSuite[0] & 0x000000FF),
				   (UCHAR) ((prRsnInfo->au4AuthKeyMgtSuite[0] >> 8) & 0x000000FF),
				   (UCHAR) ((prRsnInfo->au4AuthKeyMgtSuite[0] >> 16) & 0x000000FF),
				   (UCHAR) ((prRsnInfo->au4AuthKeyMgtSuite[0] >> 24) & 0x000000FF));
	}

	prRsnInfo->u2RsnCap = u2Cap;
	prRsnInfo->fgRsnCapPresent = TRUE;
	DBGLOG(RSN, LOUD, "RSN cap: 0x%04x\n", prRsnInfo->u2RsnCap);

	return TRUE;
}				/* rsnParseRsnIE */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to parse WPA IE.
*
* \param[in]  prInfoElem Pointer to the WPA IE.
* \param[out] prWpaInfo Pointer to the BSSDescription structure to store the
*                       WPA information from the given WPA IE.
*
* \retval TRUE Succeeded.
* \retval FALSE Failed.
*/
/*----------------------------------------------------------------------------*/
BOOLEAN rsnParseWpaIE(IN P_ADAPTER_T prAdapter, IN P_WPA_INFO_ELEM_T prInfoElem, OUT P_RSN_INFO_T prWpaInfo)
{
	UINT_32 i;
	INT_32 u4RemainWpaIeLen;
	UINT_16 u2Version;
	UINT_16 u2Cap = 0;
	UINT_32 u4GroupSuite = WPA_CIPHER_SUITE_TKIP;
	UINT_16 u2PairSuiteCount = 0;
	UINT_16 u2AuthSuiteCount = 0;
	PUCHAR pucPairSuite = NULL;
	PUCHAR pucAuthSuite = NULL;
	PUCHAR cp;
	BOOLEAN fgCapPresent = FALSE;

	DEBUGFUNC("rsnParseWpaIE");

	ASSERT(prInfoElem);
	ASSERT(prWpaInfo);

	/* Verify the length of the WPA IE. */
	if (prInfoElem->ucLength < 6) {
		DBGLOG(RSN, TRACE, "WPA IE length too short (length=%d)\n", prInfoElem->ucLength);
		return FALSE;
	}

	/* Check WPA version: currently, we only support version 1. */
	WLAN_GET_FIELD_16(&prInfoElem->u2Version, &u2Version);
	if (u2Version != 1) {
		DBGLOG(RSN, TRACE, "Unsupported WPA IE version: %d\n", u2Version);
		return FALSE;
	}

	cp = (PUCHAR) &prInfoElem->u4GroupKeyCipherSuite;
	u4RemainWpaIeLen = (INT_32) prInfoElem->ucLength - 6;

	do {
		if (u4RemainWpaIeLen == 0)
			break;

		/* WPA_OUI      : 4
		   Version      : 2
		   GroupSuite   : 4
		   PairwiseCount: 2
		   PairwiseSuite: 4 * pairSuiteCount
		   AuthCount    : 2
		   AuthSuite    : 4 * authSuiteCount
		   Cap          : 2 */

		/* Parse the Group Key Cipher Suite field. */
		if (u4RemainWpaIeLen < 4) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse WPA IE in group cipher suite (IE len: %d)\n", prInfoElem->ucLength);
			return FALSE;
		}

		WLAN_GET_FIELD_32(cp, &u4GroupSuite);
		cp += 4;
		u4RemainWpaIeLen -= 4;

		if (u4RemainWpaIeLen == 0)
			break;

		/* Parse the Pairwise Key Cipher Suite Count field. */
		if (u4RemainWpaIeLen < 2) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse WPA IE in pairwise cipher suite count (IE len: %d)\n",
				prInfoElem->ucLength);
			return FALSE;
		}

		WLAN_GET_FIELD_16(cp, &u2PairSuiteCount);
		cp += 2;
		u4RemainWpaIeLen -= 2;

		/* Parse the Pairwise Key Cipher Suite List field. */
		i = (UINT_32) u2PairSuiteCount * 4;
		if (u4RemainWpaIeLen < (INT_32) i) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse WPA IE in pairwise cipher suite list (IE len: %d)\n",
				prInfoElem->ucLength);
			return FALSE;
		}

		pucPairSuite = cp;

		cp += i;
		u4RemainWpaIeLen -= (INT_32) i;

		if (u4RemainWpaIeLen == 0)
			break;

		/* Parse the Authentication and Key Management Cipher Suite Count
		   field. */
		if (u4RemainWpaIeLen < 2) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse WPA IE in auth & key mgt suite count (IE len: %d)\n",
				prInfoElem->ucLength);
			return FALSE;
		}

		WLAN_GET_FIELD_16(cp, &u2AuthSuiteCount);
		cp += 2;
		u4RemainWpaIeLen -= 2;

		/* Parse the Authentication and Key Management Cipher Suite List
		   field. */
		i = (UINT_32) u2AuthSuiteCount * 4;
		if (u4RemainWpaIeLen < (INT_32) i) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse WPA IE in auth & key mgt suite list (IE len: %d)\n",
				prInfoElem->ucLength);
			return FALSE;
		}

		pucAuthSuite = cp;

		cp += i;
		u4RemainWpaIeLen -= (INT_32) i;

		if (u4RemainWpaIeLen == 0)
			break;

		/* Parse the WPA u2Capabilities field. */
		if (u4RemainWpaIeLen < 2) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse WPA IE in WPA capabilities (IE len: %d)\n", prInfoElem->ucLength);
			return FALSE;
		}

		fgCapPresent = TRUE;
		WLAN_GET_FIELD_16(cp, &u2Cap);
		u4RemainWpaIeLen -= 2;
	} while (FALSE);

	/* Save the WPA information for the BSS. */

	prWpaInfo->ucElemId = ELEM_ID_WPA;

	prWpaInfo->u2Version = u2Version;

	prWpaInfo->u4GroupKeyCipherSuite = u4GroupSuite;

	DBGLOG(RSN, LOUD, "WPA: version %d, group key cipher suite %02x-%02x-%02x-%02x\n",
			   u2Version, (UCHAR) (u4GroupSuite & 0x000000FF),
			   (UCHAR) ((u4GroupSuite >> 8) & 0x000000FF),
			   (UCHAR) ((u4GroupSuite >> 16) & 0x000000FF), (UCHAR) ((u4GroupSuite >> 24) & 0x000000FF));

	if (pucPairSuite) {
		/* The information about the pairwise key cipher suites is present. */
		if (u2PairSuiteCount > MAX_NUM_SUPPORTED_CIPHER_SUITES)
			u2PairSuiteCount = MAX_NUM_SUPPORTED_CIPHER_SUITES;

		prWpaInfo->u4PairwiseKeyCipherSuiteCount = (UINT_32) u2PairSuiteCount;

		for (i = 0; i < (UINT_32) u2PairSuiteCount; i++) {
			WLAN_GET_FIELD_32(pucPairSuite, &prWpaInfo->au4PairwiseKeyCipherSuite[i]);
			pucPairSuite += 4;

			DBGLOG(RSN, LOUD,
			       "WPA: pairwise key cipher suite [%d]: %02x-%02x-%02x-%02x\n",
				(UINT_8) i,
				(UCHAR) (prWpaInfo->au4PairwiseKeyCipherSuite[i] & 0x000000FF),
				(UCHAR) ((prWpaInfo->au4PairwiseKeyCipherSuite[i] >> 8) & 0x000000FF),
				(UCHAR) ((prWpaInfo->au4PairwiseKeyCipherSuite[i] >> 16) & 0x000000FF),
				(UCHAR) ((prWpaInfo->au4PairwiseKeyCipherSuite[i] >> 24) & 0x000000FF));
		}
	} else {
		/* The information about the pairwise key cipher suites is not present.
		   Use the default chipher suite for WPA: TKIP. */
		prWpaInfo->u4PairwiseKeyCipherSuiteCount = 1;
		prWpaInfo->au4PairwiseKeyCipherSuite[0] = WPA_CIPHER_SUITE_TKIP;

		DBGLOG(RSN, LOUD,
		       "WPA: pairwise key cipher suite: %02x-%02x-%02x-%02x (default)\n",
			(UCHAR) (prWpaInfo->au4PairwiseKeyCipherSuite[0] & 0x000000FF),
			(UCHAR) ((prWpaInfo->au4PairwiseKeyCipherSuite[0] >> 8) & 0x000000FF),
			(UCHAR) ((prWpaInfo->au4PairwiseKeyCipherSuite[0] >> 16) & 0x000000FF),
			(UCHAR) ((prWpaInfo->au4PairwiseKeyCipherSuite[0] >> 24) & 0x000000FF));
	}

	if (pucAuthSuite) {
		/* The information about the authentication and key management suites
		   is present. */
		if (u2AuthSuiteCount > MAX_NUM_SUPPORTED_AKM_SUITES)
			u2AuthSuiteCount = MAX_NUM_SUPPORTED_AKM_SUITES;

		prWpaInfo->u4AuthKeyMgtSuiteCount = (UINT_32) u2AuthSuiteCount;

		for (i = 0; i < (UINT_32) u2AuthSuiteCount; i++) {
			WLAN_GET_FIELD_32(pucAuthSuite, &prWpaInfo->au4AuthKeyMgtSuite[i]);
			pucAuthSuite += 4;

			DBGLOG(RSN, LOUD, "WPA: AKM suite [%d]: %02x-%02x-%02x-%02x\n",
					   (UINT_8) i,
					   (UCHAR) (prWpaInfo->au4AuthKeyMgtSuite[i] & 0x000000FF),
					   (UCHAR) ((prWpaInfo->au4AuthKeyMgtSuite[i] >> 8) & 0x000000FF),
					   (UCHAR) ((prWpaInfo->au4AuthKeyMgtSuite[i] >> 16) & 0x000000FF),
					   (UCHAR) ((prWpaInfo->au4AuthKeyMgtSuite[i] >> 24) & 0x000000FF));
		}
	} else {
		/* The information about the authentication and key management suites
		   is not present. Use the default AKM suite for WPA. */
		prWpaInfo->u4AuthKeyMgtSuiteCount = 1;
		prWpaInfo->au4AuthKeyMgtSuite[0] = WPA_AKM_SUITE_802_1X;

		DBGLOG(RSN, LOUD, "WPA: AKM suite: %02x-%02x-%02x-%02x (default)\n",
				   (UCHAR) (prWpaInfo->au4AuthKeyMgtSuite[0] & 0x000000FF),
				   (UCHAR) ((prWpaInfo->au4AuthKeyMgtSuite[0] >> 8) & 0x000000FF),
				   (UCHAR) ((prWpaInfo->au4AuthKeyMgtSuite[0] >> 16) & 0x000000FF),
				   (UCHAR) ((prWpaInfo->au4AuthKeyMgtSuite[0] >> 24) & 0x000000FF));
	}

	if (fgCapPresent) {
		prWpaInfo->fgRsnCapPresent = TRUE;
		prWpaInfo->u2RsnCap = u2Cap;
		DBGLOG(RSN, LOUD, "WPA: RSN cap: 0x%04x\n", prWpaInfo->u2RsnCap);
	} else {
		prWpaInfo->fgRsnCapPresent = FALSE;
		prWpaInfo->u2RsnCap = 0;
	}

	return TRUE;
}				/* rsnParseWpaIE */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to search the desired pairwise
*        cipher suite from the MIB Pairwise Cipher Suite
*        configuration table.
*
* \param[in] u4Cipher The desired pairwise cipher suite to be searched
* \param[out] pu4Index Pointer to the index of the desired pairwise cipher in
*                      the table
*
* \retval TRUE - The desired pairwise cipher suite is found in the table.
* \retval FALSE - The desired pairwise cipher suite is not found in the
*                 table.
*/
/*----------------------------------------------------------------------------*/
BOOLEAN rsnSearchSupportedCipher(IN P_ADAPTER_T prAdapter, IN UINT_32 u4Cipher, OUT PUINT_32 pu4Index)
{
	UINT_8 i;
	P_DOT11_RSNA_CONFIG_PAIRWISE_CIPHERS_ENTRY prEntry;

	DEBUGFUNC("rsnSearchSupportedCipher");

	ASSERT(pu4Index);

	for (i = 0; i < MAX_NUM_SUPPORTED_CIPHER_SUITES; i++) {
		prEntry = &prAdapter->rMib.dot11RSNAConfigPairwiseCiphersTable[i];
		if (prEntry->dot11RSNAConfigPairwiseCipher ==
			u4Cipher && prEntry->dot11RSNAConfigPairwiseCipherEnabled) {
			*pu4Index = i;
			return TRUE;
		}
	}
	return FALSE;
}				/* rsnSearchSupportedCipher */

/*----------------------------------------------------------------------------*/
/*!
* \brief Whether BSS RSN is matched from upper layer set.
*
* \param[in] prAdapter Pointer to the Adapter structure, BSS RSN Information
*
* \retval BOOLEAN
*/
/*----------------------------------------------------------------------------*/
BOOLEAN rsnIsSuitableBSS(IN P_ADAPTER_T prAdapter, IN P_RSN_INFO_T prBssRsnInfo)
{
	UINT_8 i = 0;

	DEBUGFUNC("rsnIsSuitableBSS");

	do {

		if ((prAdapter->rWifiVar.rConnSettings.rRsnInfo.u4GroupKeyCipherSuite & 0x000000FF) !=
		    GET_SELECTOR_TYPE(prBssRsnInfo->u4GroupKeyCipherSuite)) {
			DBGLOG(RSN, TRACE, "Break by GroupKeyCipherSuite\n");
			break;
		}
		for (i = 0; i < prBssRsnInfo->u4PairwiseKeyCipherSuiteCount; i++) {
			if (((prAdapter->rWifiVar.rConnSettings.rRsnInfo.au4PairwiseKeyCipherSuite[0] & 0x000000FF) !=
			     GET_SELECTOR_TYPE(prBssRsnInfo->au4PairwiseKeyCipherSuite[i]))
			    && (i == prBssRsnInfo->u4PairwiseKeyCipherSuiteCount - 1)) {
				DBGLOG(RSN, TRACE, "Break by PairwiseKeyCipherSuite\n");
				break;
			}
		}
		for (i = 0; i < prBssRsnInfo->u4AuthKeyMgtSuiteCount; i++) {
			if (((prAdapter->rWifiVar.rConnSettings.rRsnInfo.au4AuthKeyMgtSuite[0] & 0x000000FF) !=
			     GET_SELECTOR_TYPE(prBssRsnInfo->au4AuthKeyMgtSuite[0]))
			    && (i == prBssRsnInfo->u4AuthKeyMgtSuiteCount - 1)) {
				DBGLOG(RSN, TRACE, "Break by AuthKeyMgtSuite\n");
				break;
			}
		}
		return TRUE;
	} while (FALSE);
	return FALSE;
}

/*----------------------------------------------------------------------------*/
/*!
*
* \brief This routine is called to search the desired
*        authentication and key management (AKM) suite from the
*        MIB Authentication and Key Management Suites table.
*
* \param[in]  u4AkmSuite The desired AKM suite to be searched
* \param[out] pu4Index   Pointer to the index of the desired AKM suite in the
*                        table
*
* \retval TRUE  The desired AKM suite is found in the table.
* \retval FALSE The desired AKM suite is not found in the table.
*
* \note
*/
/*----------------------------------------------------------------------------*/
BOOLEAN rsnSearchAKMSuite(IN P_ADAPTER_T prAdapter, IN UINT_32 u4AkmSuite, OUT PUINT_32 pu4Index)
{
	UINT_8 i;
	P_DOT11_RSNA_CONFIG_AUTHENTICATION_SUITES_ENTRY prEntry;

	DEBUGFUNC("rsnSearchAKMSuite");

	ASSERT(pu4Index);

	for (i = 0; i < MAX_NUM_SUPPORTED_AKM_SUITES; i++) {
		prEntry = &prAdapter->rMib.dot11RSNAConfigAuthenticationSuitesTable[i];
		if (prEntry->dot11RSNAConfigAuthenticationSuite == u4AkmSuite &&
		    prEntry->dot11RSNAConfigAuthenticationSuiteEnabled) {
			*pu4Index = i;
			return TRUE;
		}
	}
	return FALSE;
}				/* rsnSearchAKMSuite */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to perform RSNA or TSN policy
*        selection for a given BSS.
*
* \param[in]  prBss Pointer to the BSS description
*
* \retval TRUE - The RSNA/TSN policy selection for the given BSS is
*                successful. The selected pairwise and group cipher suites
*                are returned in the BSS description.
* \retval FALSE - The RSNA/TSN policy selection for the given BSS is failed.
*                 The driver shall not attempt to join the given BSS.
*
* \note The Encrypt status matched score will save to bss for final ap select.
*/
/*----------------------------------------------------------------------------*/
BOOLEAN rsnPerformPolicySelection(IN P_ADAPTER_T prAdapter, IN P_BSS_DESC_T prBss)
{
#if CFG_SUPPORT_802_11W
	INT_32 i;
	UINT_32 j;
#else
	UINT_32 i, j;
#endif
	BOOLEAN fgSuiteSupported;
	UINT_32 u4PairwiseCipher = 0;
	UINT_32 u4GroupCipher = 0;
	UINT_32 u4AkmSuite = 0;
	P_RSN_INFO_T prBssRsnInfo;
	UINT_8 ucBssIndex;
	BOOLEAN fgIsWpsActive = (BOOLEAN) FALSE;

	DEBUGFUNC("rsnPerformPolicySelection");

	ASSERT(prBss);

	DBGLOG(RSN, TRACE, "rsnPerformPolicySelection\n");
	/* Todo:: */
	ucBssIndex = prAdapter->prAisBssInfo->ucBssIndex;

	prBss->u4RsnSelectedPairwiseCipher = 0;
	prBss->u4RsnSelectedGroupCipher = 0;
	prBss->u4RsnSelectedAKMSuite = 0;
	prBss->ucEncLevel = 0;

#if CFG_SUPPORT_WPS
	fgIsWpsActive = kalWSCGetActiveState(prAdapter->prGlueInfo);

	/* CR1640, disable the AP select privacy check */
	if (fgIsWpsActive &&
	    (prAdapter->rWifiVar.rConnSettings.eAuthMode < AUTH_MODE_WPA) &&
	    (prAdapter->rWifiVar.rConnSettings.eOPMode == NET_TYPE_INFRA)) {
		DBGLOG(RSN, TRACE, "-- Skip the Protected BSS check\n");
		return TRUE;
	}
#endif

	/* Protection is not required in this BSS. */
	if ((prBss->u2CapInfo & CAP_INFO_PRIVACY) == 0) {

		if (secEnabledInAis(prAdapter) == FALSE) {
			DBGLOG(RSN, TRACE, "-- No Protected BSS\n");
		} else {
			DBGLOG(RSN, TRACE, "-- Protected BSS\n");
			return FALSE;
		}
		return TRUE;
	}

	/* Protection is required in this BSS. */
	if ((prBss->u2CapInfo & CAP_INFO_PRIVACY) != 0) {
		if (secEnabledInAis(prAdapter) == FALSE) {
			DBGLOG(RSN, TRACE, "-- Protected BSS\n");
			return FALSE;
		}
	}

	if (prAdapter->rWifiVar.rConnSettings.eAuthMode == AUTH_MODE_WPA ||
	    prAdapter->rWifiVar.rConnSettings.eAuthMode == AUTH_MODE_WPA_PSK ||
	    prAdapter->rWifiVar.rConnSettings.eAuthMode == AUTH_MODE_WPA_NONE) {

		if (prBss->fgIEWPA) {
			prBssRsnInfo = &prBss->rWPAInfo;
		} else {
			DBGLOG(RSN, TRACE, "WPA Information Element does not exist.\n");
			return FALSE;
		}
	} else if (prAdapter->rWifiVar.rConnSettings.eAuthMode == AUTH_MODE_WPA2 ||
		   prAdapter->rWifiVar.rConnSettings.eAuthMode == AUTH_MODE_WPA2_PSK) {

		if (prBss->fgIERSN) {
			prBssRsnInfo = &prBss->rRSNInfo;
		} else {
			DBGLOG(RSN, TRACE, "RSN Information Element does not exist.\n");
			return FALSE;
		}
	} else if (prAdapter->rWifiVar.rConnSettings.eEncStatus != ENUM_ENCRYPTION1_ENABLED) {
		/* If the driver is configured to use WEP only, ignore this BSS. */
		DBGLOG(RSN, TRACE, "-- Not WEP-only legacy BSS\n");
		return FALSE;
	} else if (prAdapter->rWifiVar.rConnSettings.eEncStatus == ENUM_ENCRYPTION1_ENABLED) {
		/* If the driver is configured to use WEP only, use this BSS. */
		DBGLOG(RSN, TRACE, "-- WEP-only legacy BSS\n");
		return TRUE;
	}

	if (!rsnIsSuitableBSS(prAdapter, prBssRsnInfo)) {
		DBGLOG(RSN, TRACE, "RSN info check no matched\n");
		return FALSE;
	}

	if (prBssRsnInfo->u4PairwiseKeyCipherSuiteCount == 1 &&
	    GET_SELECTOR_TYPE(prBssRsnInfo->au4PairwiseKeyCipherSuite[0]) == CIPHER_SUITE_NONE) {
		/* Since the pairwise cipher use the same cipher suite as the group
		   cipher in the BSS, we check the group cipher suite against the
		   current encryption status. */
		fgSuiteSupported = FALSE;

		switch (prBssRsnInfo->u4GroupKeyCipherSuite) {
		case WPA_CIPHER_SUITE_CCMP:
		case RSN_CIPHER_SUITE_CCMP:
			if (prAdapter->rWifiVar.rConnSettings.eEncStatus == ENUM_ENCRYPTION3_ENABLED)
				fgSuiteSupported = TRUE;
			break;

		case WPA_CIPHER_SUITE_TKIP:
		case RSN_CIPHER_SUITE_TKIP:
			if (prAdapter->rWifiVar.rConnSettings.eEncStatus == ENUM_ENCRYPTION2_ENABLED)
				fgSuiteSupported = TRUE;
			break;

		case WPA_CIPHER_SUITE_WEP40:
		case WPA_CIPHER_SUITE_WEP104:
			if (prAdapter->rWifiVar.rConnSettings.eEncStatus == ENUM_ENCRYPTION1_ENABLED)
				fgSuiteSupported = TRUE;
			break;
		}

		if (fgSuiteSupported) {
			u4PairwiseCipher = WPA_CIPHER_SUITE_NONE;
			u4GroupCipher = prBssRsnInfo->u4GroupKeyCipherSuite;
		}
#if DBG
		else {
			DBGLOG(RSN, TRACE,
			       "Inproper encryption status %d for group-key-only BSS\n",
				prAdapter->rWifiVar.rConnSettings.eEncStatus);
		}
#endif
	} else {
		fgSuiteSupported = FALSE;

		DBGLOG(RSN, TRACE,
		       "eEncStatus %d %u 0x%x\n", prAdapter->rWifiVar.rConnSettings.eEncStatus,
			prBssRsnInfo->u4PairwiseKeyCipherSuiteCount, prBssRsnInfo->au4PairwiseKeyCipherSuite[0]);
		/* Select pairwise/group ciphers */
		switch (prAdapter->rWifiVar.rConnSettings.eEncStatus) {
		case ENUM_ENCRYPTION3_ENABLED:
			for (i = 0; i < prBssRsnInfo->u4PairwiseKeyCipherSuiteCount; i++) {
				if (GET_SELECTOR_TYPE(prBssRsnInfo->au4PairwiseKeyCipherSuite[i])
				    == CIPHER_SUITE_CCMP) {
					u4PairwiseCipher = prBssRsnInfo->au4PairwiseKeyCipherSuite[i];
				}
			}
			u4GroupCipher = prBssRsnInfo->u4GroupKeyCipherSuite;
			break;

		case ENUM_ENCRYPTION2_ENABLED:
			for (i = 0; i < prBssRsnInfo->u4PairwiseKeyCipherSuiteCount; i++) {
				if (GET_SELECTOR_TYPE(prBssRsnInfo->au4PairwiseKeyCipherSuite[i])
				    == CIPHER_SUITE_TKIP) {
					u4PairwiseCipher = prBssRsnInfo->au4PairwiseKeyCipherSuite[i];
				}
			}
			if (GET_SELECTOR_TYPE(prBssRsnInfo->u4GroupKeyCipherSuite) == CIPHER_SUITE_CCMP) {
				/* ToDo:: DBGLOG */
				DBGLOG(RSN, TRACE, "Cannot join CCMP BSS\n");
			} else
				u4GroupCipher = prBssRsnInfo->u4GroupKeyCipherSuite;
			break;

		case ENUM_ENCRYPTION1_ENABLED:
			for (i = 0; i < prBssRsnInfo->u4PairwiseKeyCipherSuiteCount; i++) {
				if (GET_SELECTOR_TYPE(prBssRsnInfo->au4PairwiseKeyCipherSuite[i])
				    == CIPHER_SUITE_WEP40 ||
				    GET_SELECTOR_TYPE(prBssRsnInfo->au4PairwiseKeyCipherSuite[i])
				    == CIPHER_SUITE_WEP104) {
					u4PairwiseCipher = prBssRsnInfo->au4PairwiseKeyCipherSuite[i];
				}
			}
			if (GET_SELECTOR_TYPE(prBssRsnInfo->u4GroupKeyCipherSuite) ==
			    CIPHER_SUITE_CCMP ||
			    GET_SELECTOR_TYPE(prBssRsnInfo->u4GroupKeyCipherSuite) == CIPHER_SUITE_TKIP) {
				DBGLOG(RSN, TRACE, "Cannot join CCMP/TKIP BSS\n");
			} else {
				u4GroupCipher = prBssRsnInfo->u4GroupKeyCipherSuite;
			}
			break;

		default:
			break;
		}
	}

	/* Exception handler */
	/* If we cannot find proper pairwise and group cipher suites to join the
	   BSS, do not check the supported AKM suites. */
	if (u4PairwiseCipher == 0 || u4GroupCipher == 0) {
		DBGLOG(RSN, TRACE, "Failed to select pairwise/group cipher (0x%08lx/0x%08lx)\n",
				    u4PairwiseCipher, u4GroupCipher);
		return FALSE;
	}
#if CFG_ENABLE_WIFI_DIRECT
	if ((prAdapter->fgIsP2PRegistered) &&
	    (GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex)->eNetworkType == NETWORK_TYPE_P2P)) {
		if (u4PairwiseCipher != RSN_CIPHER_SUITE_CCMP ||
		    u4GroupCipher != RSN_CIPHER_SUITE_CCMP || u4AkmSuite != RSN_AKM_SUITE_PSK) {
			DBGLOG(RSN, TRACE,
			       "Failed to select pairwise/group cipher for P2P network (0x%08lx/0x%08lx)\n",
				u4PairwiseCipher, u4GroupCipher);
			return FALSE;
		}
	}
#endif

#if CFG_ENABLE_BT_OVER_WIFI
	if (GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex)->eNetworkType == NETWORK_TYPE_BOW) {
		if (u4PairwiseCipher != RSN_CIPHER_SUITE_CCMP ||
		    u4GroupCipher != RSN_CIPHER_SUITE_CCMP || u4AkmSuite != RSN_AKM_SUITE_PSK) {
			/* Todo:: Nothing */
		}
		DBGLOG(RSN, TRACE,
		       "Failed to select pairwise/group cipher for BT over Wi-Fi network (0x%08lx/0x%08lx)\n",
			u4PairwiseCipher, u4GroupCipher);
		return FALSE;
	}
#endif

	/* Verify if selected pairwisse cipher is supported */
	fgSuiteSupported = rsnSearchSupportedCipher(prAdapter, u4PairwiseCipher, &i);

	/* Verify if selected group cipher is supported */
	if (fgSuiteSupported)
		fgSuiteSupported = rsnSearchSupportedCipher(prAdapter, u4GroupCipher, &i);

	if (!fgSuiteSupported) {
		DBGLOG(RSN, TRACE,
		       "Failed to support selected pairwise/group cipher (0x%08lx/0x%08lx)\n",
			u4PairwiseCipher, u4GroupCipher);
		return FALSE;
	}

	/* Select AKM */
	/* If the driver cannot support any authentication suites advertised in
	   the given BSS, we fail to perform RSNA policy selection. */
	/* Attempt to find any overlapping supported AKM suite. */
#if CFG_SUPPORT_802_11W
	if (i != 0)
		for (i = (prBssRsnInfo->u4AuthKeyMgtSuiteCount - 1); i >= 0; i--) {
#else
	for (i = 0; i < prBssRsnInfo->u4AuthKeyMgtSuiteCount; i++) {
#endif
		if (rsnSearchAKMSuite(prAdapter, prBssRsnInfo->au4AuthKeyMgtSuite[i], &j)) {
			u4AkmSuite = prBssRsnInfo->au4AuthKeyMgtSuite[i];
			break;
		}
	}

	if (u4AkmSuite == 0) {
		DBGLOG(RSN, TRACE, "Cannot support any AKM suites\n");
		return FALSE;
	}

	DBGLOG(RSN, TRACE,
	       "Selected pairwise/group cipher: %02x-%02x-%02x-%02x/%02x-%02x-%02x-%02x\n",
		(UINT_8) (u4PairwiseCipher & 0x000000FF),
		(UINT_8) ((u4PairwiseCipher >> 8) & 0x000000FF),
		(UINT_8) ((u4PairwiseCipher >> 16) & 0x000000FF),
		(UINT_8) ((u4PairwiseCipher >> 24) & 0x000000FF),
		(UINT_8) (u4GroupCipher & 0x000000FF), (UINT_8) ((u4GroupCipher >> 8) & 0x000000FF),
		(UINT_8) ((u4GroupCipher >> 16) & 0x000000FF), (UINT_8) ((u4GroupCipher >> 24) & 0x000000FF));

	DBGLOG(RSN, TRACE, "Selected AKM suite: %02x-%02x-%02x-%02x\n",
			    (UINT_8) (u4AkmSuite & 0x000000FF),
			    (UINT_8) ((u4AkmSuite >> 8) & 0x000000FF),
			    (UINT_8) ((u4AkmSuite >> 16) & 0x000000FF), (UINT_8) ((u4AkmSuite >> 24) & 0x000000FF));

#if CFG_SUPPORT_802_11W
	DBGLOG(RSN, TRACE, "[MFP] MFP setting = %u\n ", kalGetMfpSetting(prAdapter->prGlueInfo));

	if (kalGetMfpSetting(prAdapter->prGlueInfo) == RSN_AUTH_MFP_REQUIRED) {
		if (!prBssRsnInfo->fgRsnCapPresent) {
			DBGLOG(RSN, TRACE, "[MFP] Skip RSN IE, No MFP Required Capability.\n");
			return FALSE;
		} else if (!(prBssRsnInfo->u2RsnCap & ELEM_WPA_CAP_MFPC)) {
			DBGLOG(RSN, TRACE, "[MFP] Skip RSN IE, No MFP Required\n");
			return FALSE;
		}
		prAdapter->rWifiVar.rAisSpecificBssInfo.fgMgmtProtection = TRUE;
	} else if (kalGetMfpSetting(prAdapter->prGlueInfo) == RSN_AUTH_MFP_OPTIONAL) {
		if (prBssRsnInfo->u2RsnCap && ((prBssRsnInfo->u2RsnCap & ELEM_WPA_CAP_MFPR) ||
					       (prBssRsnInfo->u2RsnCap & ELEM_WPA_CAP_MFPC))) {
			prAdapter->rWifiVar.rAisSpecificBssInfo.fgMgmtProtection = TRUE;
		} else {
			prAdapter->rWifiVar.rAisSpecificBssInfo.fgMgmtProtection = FALSE;
		}
	} else {
		if (prBssRsnInfo->fgRsnCapPresent && prBssRsnInfo->u2RsnCap) {
			if (/* (prBssRsnInfo->u2RsnCap & ELEM_WPA_CAP_MFPR) || */
				   (prBssRsnInfo->u2RsnCap & ELEM_WPA_CAP_MFPC)) {
				prAdapter->rWifiVar.rAisSpecificBssInfo.fgMgmtProtection = TRUE;
				prAdapter->prGlueInfo->rWpaInfo.u4Mfp = RSN_AUTH_MFP_OPTIONAL;
			}
			/* else if ((prBssRsnInfo->u2RsnCap & ELEM_WPA_CAP_MFPC) && */
			/* !(prBssRsnInfo->u2RsnCap & ELEM_WPA_CAP_MFPR)) { */
			/* prAdapter->rWifiVar.rAisSpecificBssInfo.fgMgmtProtection = TRUE; */
			/* prAdapter->prGlueInfo->rWpaInfo.u4Mfp = RSN_AUTH_MFP_OPTIONAL; */
			/* } */
			else
				prAdapter->rWifiVar.rAisSpecificBssInfo.fgMgmtProtection = FALSE;
		}
		if (prBssRsnInfo->fgRsnCapPresent && (prBssRsnInfo->u2RsnCap & ELEM_WPA_CAP_MFPR)) {
			if (prAdapter->rWifiVar.rAisSpecificBssInfo.fgMgmtProtection == FALSE) {
				DBGLOG(RSN, INFO, "[MFP] Skip RSN IE, No MFP Required Capability\n");
				return FALSE;
			}
		}
	}
	DBGLOG(RSN, TRACE,
	       "[MFP] fgMgmtProtection = %d\n ", prAdapter->rWifiVar.rAisSpecificBssInfo.fgMgmtProtection);

	prAdapter->rWifiVar.rAisSpecificBssInfo.fgAPApplyPmfReq = FALSE;
	if (prBssRsnInfo->fgRsnCapPresent && (prBssRsnInfo->u2RsnCap & ELEM_WPA_CAP_MFPR))
		prAdapter->rWifiVar.rAisSpecificBssInfo.fgAPApplyPmfReq = TRUE;
#endif

	if (GET_SELECTOR_TYPE(u4GroupCipher) == CIPHER_SUITE_CCMP) {
		prBss->ucEncLevel = 3;
	} else if (GET_SELECTOR_TYPE(u4GroupCipher) == CIPHER_SUITE_TKIP) {
		prBss->ucEncLevel = 2;
	} else if (GET_SELECTOR_TYPE(u4GroupCipher) == CIPHER_SUITE_WEP40 ||
		   GET_SELECTOR_TYPE(u4GroupCipher) == CIPHER_SUITE_WEP104) {
		prBss->ucEncLevel = 1;
	} else {
		ASSERT(FALSE);
	}
	prBss->u4RsnSelectedPairwiseCipher = u4PairwiseCipher;
	prBss->u4RsnSelectedGroupCipher = u4GroupCipher;
	prBss->u4RsnSelectedAKMSuite = u4AkmSuite;

	return TRUE;

}				/* rsnPerformPolicySelection */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to generate WPA IE for beacon frame.
*
* \param[in] pucIeStartAddr Pointer to put the generated WPA IE.
*
* \return The append WPA-None IE length
* \note
*      Called by: JOIN module, compose beacon IE
*/
/*----------------------------------------------------------------------------*/
VOID rsnGenerateWpaNoneIE(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo)
{
	UINT_32 i;
	P_WPA_INFO_ELEM_T prWpaIE;
	UINT_32 u4Suite;
	UINT_16 u2SuiteCount;
	PUINT_8 cp, cp2;
	UINT_8 ucExpendedLen = 0;
	PUINT_8 pucBuffer;
	UINT_8 ucBssIndex;

	DEBUGFUNC("rsnGenerateWpaNoneIE");

	ASSERT(prMsduInfo);

	if (prAdapter->rWifiVar.rConnSettings.eAuthMode != AUTH_MODE_WPA_NONE)
		return;

	ucBssIndex = prMsduInfo->ucBssIndex;

	if (GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex)->eNetworkType != NETWORK_TYPE_AIS)
		return;

	pucBuffer = (PUINT_8) ((ULONG) prMsduInfo->prPacket + (ULONG) prMsduInfo->u2FrameLength);

	ASSERT(pucBuffer);

	prWpaIE = (P_WPA_INFO_ELEM_T) (pucBuffer);

	/* Start to construct a WPA IE. */
	/* Fill the Element ID field. */
	prWpaIE->ucElemId = ELEM_ID_WPA;

	/* Fill the OUI and OUI Type fields. */
	prWpaIE->aucOui[0] = 0x00;
	prWpaIE->aucOui[1] = 0x50;
	prWpaIE->aucOui[2] = 0xF2;
	prWpaIE->ucOuiType = VENDOR_OUI_TYPE_WPA;

	/* Fill the Version field. */
	WLAN_SET_FIELD_16(&prWpaIE->u2Version, 1);	/* version 1 */
	ucExpendedLen = 6;

	/* Fill the Pairwise Key Cipher Suite List field. */
	u2SuiteCount = 0;
	cp = (PUINT_8) &prWpaIE->aucPairwiseKeyCipherSuite1[0];

	if (rsnSearchSupportedCipher(prAdapter, WPA_CIPHER_SUITE_CCMP, &i))
		u4Suite = WPA_CIPHER_SUITE_CCMP;
	else if (rsnSearchSupportedCipher(prAdapter, WPA_CIPHER_SUITE_TKIP, &i))
		u4Suite = WPA_CIPHER_SUITE_TKIP;
	else if (rsnSearchSupportedCipher(prAdapter, WPA_CIPHER_SUITE_WEP104, &i))
		u4Suite = WPA_CIPHER_SUITE_WEP104;
	else if (rsnSearchSupportedCipher(prAdapter, WPA_CIPHER_SUITE_WEP40, &i))
		u4Suite = WPA_CIPHER_SUITE_WEP40;
	else
		u4Suite = WPA_CIPHER_SUITE_TKIP;

	WLAN_SET_FIELD_32(cp, u4Suite);
	u2SuiteCount++;
	ucExpendedLen += 4;
	cp += 4;

	/* Fill the Group Key Cipher Suite field as the same in pair-wise key. */
	WLAN_SET_FIELD_32(&prWpaIE->u4GroupKeyCipherSuite, u4Suite);
	ucExpendedLen += 4;

	/* Fill the Pairwise Key Cipher Suite Count field. */
	WLAN_SET_FIELD_16(&prWpaIE->u2PairwiseKeyCipherSuiteCount, u2SuiteCount);
	ucExpendedLen += 2;

	cp2 = cp;

	/* Fill the Authentication and Key Management Suite List field. */
	u2SuiteCount = 0;
	cp += 2;

	if (rsnSearchAKMSuite(prAdapter, WPA_AKM_SUITE_802_1X, &i))
		u4Suite = WPA_AKM_SUITE_802_1X;
	else if (rsnSearchAKMSuite(prAdapter, WPA_AKM_SUITE_PSK, &i))
		u4Suite = WPA_AKM_SUITE_PSK;
	else
		u4Suite = WPA_AKM_SUITE_NONE;

	/* This shall be the only available value for current implementation */
	ASSERT(u4Suite == WPA_AKM_SUITE_NONE);

	WLAN_SET_FIELD_32(cp, u4Suite);
	u2SuiteCount++;
	ucExpendedLen += 4;
	cp += 4;

	/* Fill the Authentication and Key Management Suite Count field. */
	WLAN_SET_FIELD_16(cp2, u2SuiteCount);
	ucExpendedLen += 2;

	/* Fill the Length field. */
	prWpaIE->ucLength = (UINT_8) ucExpendedLen;

	/* Increment the total IE length for the Element ID and Length fields. */
	prMsduInfo->u2FrameLength += IE_SIZE(pucBuffer);

}				/* rsnGenerateWpaNoneIE */

/*----------------------------------------------------------------------------*/
/*!
*
* \brief This routine is called to generate WPA IE for
*        associate request frame.
*
* \param[in]  prCurrentBss     The Selected BSS description
*
* \retval The append WPA IE length
*
* \note
*      Called by: AIS module, Associate request
*/
/*----------------------------------------------------------------------------*/
VOID rsnGenerateWPAIE(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo)
{
	PUCHAR cp;
	PUINT_8 pucBuffer;
	UINT_8 ucBssIndex;
	P_P2P_SPECIFIC_BSS_INFO_T prP2pSpecificBssInfo;

	DEBUGFUNC("rsnGenerateWPAIE");

	ASSERT(prMsduInfo);

	pucBuffer = (PUINT_8) ((ULONG) prMsduInfo->prPacket + (ULONG) prMsduInfo->u2FrameLength);

	ASSERT(pucBuffer);

	ucBssIndex = prMsduInfo->ucBssIndex;
	prP2pSpecificBssInfo = prAdapter->rWifiVar.prP2pSpecificBssInfo;

	/* if (eNetworkId != NETWORK_TYPE_AIS_INDEX) */
	/* return; */

#if CFG_ENABLE_WIFI_DIRECT
	if ((1 /* prCurrentBss->fgIEWPA */  &&
	     ((prAdapter->fgIsP2PRegistered) &&
	      (GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex)->eNetworkType == NETWORK_TYPE_P2P) &&
	      (kalP2PGetTkipCipher(prAdapter->prGlueInfo)))) ||
	      ((prAdapter->rWifiVar.rConnSettings.eAuthMode == AUTH_MODE_WPA) ||
	       (prAdapter->rWifiVar.rConnSettings.eAuthMode == AUTH_MODE_WPA_PSK))) {
#else
	if ((1 /* prCurrentBss->fgIEWPA */  &&
	    ((prAdapter->rWifiVar.rConnSettings.eAuthMode == AUTH_MODE_WPA) ||
	     (prAdapter->rWifiVar.rConnSettings.eAuthMode == AUTH_MODE_WPA_PSK)))) {
#endif
		if (prAdapter->fgIsP2PRegistered && prP2pSpecificBssInfo && (prP2pSpecificBssInfo->u2WpaIeLen != 0)) {
			kalMemCopy(pucBuffer, prP2pSpecificBssInfo->aucWpaIeBuffer, prP2pSpecificBssInfo->u2WpaIeLen);
			prMsduInfo->u2FrameLength += prP2pSpecificBssInfo->u2WpaIeLen;
			return;
		}
		/* Construct a WPA IE for association request frame. */
		WPA_IE(pucBuffer)->ucElemId = ELEM_ID_WPA;
		WPA_IE(pucBuffer)->ucLength = ELEM_ID_WPA_LEN_FIXED;
		WPA_IE(pucBuffer)->aucOui[0] = 0x00;
		WPA_IE(pucBuffer)->aucOui[1] = 0x50;
		WPA_IE(pucBuffer)->aucOui[2] = 0xF2;
		WPA_IE(pucBuffer)->ucOuiType = VENDOR_OUI_TYPE_WPA;
		WLAN_SET_FIELD_16(&WPA_IE(pucBuffer)->u2Version, 1);

#if CFG_ENABLE_WIFI_DIRECT
		if (prAdapter->fgIsP2PRegistered
		    && GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex)->eNetworkType == NETWORK_TYPE_P2P) {
			WLAN_SET_FIELD_32(&WPA_IE(pucBuffer)->u4GroupKeyCipherSuite, WPA_CIPHER_SUITE_TKIP);
		} else
#endif
			WLAN_SET_FIELD_32(&WPA_IE(pucBuffer)->u4GroupKeyCipherSuite,
					  prAdapter->prAisBssInfo->u4RsnSelectedGroupCipher);

		cp = (PUCHAR) &WPA_IE(pucBuffer)->aucPairwiseKeyCipherSuite1[0];

		WLAN_SET_FIELD_16(&WPA_IE(pucBuffer)->u2PairwiseKeyCipherSuiteCount, 1);
#if CFG_ENABLE_WIFI_DIRECT
		if (prAdapter->fgIsP2PRegistered
		    && GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex)->eNetworkType == NETWORK_TYPE_P2P) {
			WLAN_SET_FIELD_32(cp, WPA_CIPHER_SUITE_TKIP);
		} else
#endif
			WLAN_SET_FIELD_32(cp, prAdapter->prAisBssInfo->u4RsnSelectedPairwiseCipher);
		cp += 4;

		WLAN_SET_FIELD_16(cp, 1);
		cp += 2;
#if CFG_ENABLE_WIFI_DIRECT
		if (prAdapter->fgIsP2PRegistered
		    && GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex)->eNetworkType == NETWORK_TYPE_P2P) {
			WLAN_SET_FIELD_32(cp, WPA_AKM_SUITE_PSK);
		} else
#endif
			WLAN_SET_FIELD_32(cp, prAdapter->prAisBssInfo->u4RsnSelectedAKMSuite);
		cp += 4;

		WPA_IE(pucBuffer)->ucLength = ELEM_ID_WPA_LEN_FIXED;

		prMsduInfo->u2FrameLength += IE_SIZE(pucBuffer);
	}

}				/* rsnGenerateWPAIE */

/*----------------------------------------------------------------------------*/
/*!
*
* \brief This routine is called to generate RSN IE for
*        associate request frame.
*
* \param[in]  prMsduInfo     The Selected BSS description
*
* \retval The append RSN IE length
*
* \note
*      Called by: AIS module, P2P module, BOW module Associate request
*/
/*----------------------------------------------------------------------------*/
VOID rsnGenerateRSNIE(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo)
{
	UINT_32 u4Entry;
	PUCHAR cp;
	/* UINT_8                ucExpendedLen = 0; */
	PUINT_8 pucBuffer;
	UINT_8 ucBssIndex;
	P_STA_RECORD_T prStaRec;

	DEBUGFUNC("rsnGenerateRSNIE");

	ASSERT(prMsduInfo);

	pucBuffer = (PUINT_8) ((ULONG) prMsduInfo->prPacket + (ULONG) prMsduInfo->u2FrameLength);

	ASSERT(pucBuffer);

	/* Todo:: network id */
	ucBssIndex = prMsduInfo->ucBssIndex;

	if (
#if CFG_ENABLE_WIFI_DIRECT
		   ((prAdapter->fgIsP2PRegistered) &&
		    (GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex)->eNetworkType == NETWORK_TYPE_P2P)
		    && (kalP2PGetCcmpCipher(prAdapter->prGlueInfo))) ||
#endif
#if CFG_ENABLE_BT_OVER_WIFI
		   (GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex)->eNetworkType == NETWORK_TYPE_BOW)
		   ||
#endif
		   (GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex)->eNetworkType ==
		    NETWORK_TYPE_AIS /* prCurrentBss->fgIERSN */  &&
		    ((prAdapter->rWifiVar.rConnSettings.eAuthMode == AUTH_MODE_WPA2)
		     || (prAdapter->rWifiVar.rConnSettings.eAuthMode == AUTH_MODE_WPA2_PSK)))) {
		/* Construct a RSN IE for association request frame. */
		RSN_IE(pucBuffer)->ucElemId = ELEM_ID_RSN;
		RSN_IE(pucBuffer)->ucLength = ELEM_ID_RSN_LEN_FIXED;
		WLAN_SET_FIELD_16(&RSN_IE(pucBuffer)->u2Version, 1);	/* Version */
		WLAN_SET_FIELD_32(&RSN_IE(pucBuffer)->u4GroupKeyCipherSuite,
			GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex)->u4RsnSelectedGroupCipher); /* Group key suite */
		cp = (PUCHAR) &RSN_IE(pucBuffer)->aucPairwiseKeyCipherSuite1[0];
		WLAN_SET_FIELD_16(&RSN_IE(pucBuffer)->u2PairwiseKeyCipherSuiteCount, 1);
		WLAN_SET_FIELD_32(cp, GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex)->u4RsnSelectedPairwiseCipher);
		cp += 4;
		WLAN_SET_FIELD_16(cp, 1);	/* AKM suite count */
		cp += 2;
		/* AKM suite */
		WLAN_SET_FIELD_32(cp, GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex)->u4RsnSelectedAKMSuite);
		cp += 4;
#if CFG_SUPPORT_802_11W
		/* Capabilities */
		WLAN_SET_FIELD_16(cp, GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex)->u2RsnSelectedCapInfo);
		DBGLOG(RSN, TRACE,
		       "Gen RSN IE = %x\n", GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex)->u2RsnSelectedCapInfo);
		if (GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex)->eNetworkType == NETWORK_TYPE_AIS
		    && prAdapter->rWifiVar.rAisSpecificBssInfo.fgMgmtProtection) {
			if (kalGetMfpSetting(prAdapter->prGlueInfo) == RSN_AUTH_MFP_REQUIRED
			    /* prAdapter->rWifiVar.rAisSpecificBssInfo.fgAPApplyPmfReq */) {
				WLAN_SET_FIELD_16(cp, ELEM_WPA_CAP_MFPC | ELEM_WPA_CAP_MFPR);	/* Capabilities */
				DBGLOG(RSN, TRACE, "RSN_AUTH_MFP_NO - MFPC & MFPR\n");
			} else {
				WLAN_SET_FIELD_16(cp, ELEM_WPA_CAP_MFPC);	/* Capabilities */
				DBGLOG(RSN, TRACE, "RSN_AUTH_MFP_NO - MFPC\n");
			}
		}
#else
		/* Capabilities */
		WLAN_SET_FIELD_16(cp, GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex)->u2RsnSelectedCapInfo);
#endif
		cp += 2;

		if (GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex)->eNetworkType == NETWORK_TYPE_AIS)
			prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

		if (GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex)->eNetworkType == NETWORK_TYPE_AIS
		    && rsnSearchPmkidEntry(prAdapter, prStaRec->aucMacAddr, &u4Entry)) {
			/* DBGLOG(RSN, TRACE, ("Add Pmk at assoc req\n")); */
			/* DBGLOG(RSN, TRACE, ("addr " MACSTR" PMKID "MACSTR"\n", */
			/* MAC2STR(prAdapter->rWifiVar.rAisSpecificBssInfo.arPmkidCache[u4Entry].rBssidInfo.arBSSID),
			 * MAC2STR(prAdapter->rWifiVar.rAisSpecificBssInfo.
			 * arPmkidCache[u4Entry].rBssidInfo.arPMKID))); */
			if (prAdapter->rWifiVar.rAisSpecificBssInfo.arPmkidCache[u4Entry].fgPmkidExist) {
				RSN_IE(pucBuffer)->ucLength = 38;
				WLAN_SET_FIELD_16(cp, 1);	/* PMKID count */
				cp += 2;
				DBGLOG(RSN, TRACE,
					"BSSID " MACSTR " ind=%u\n", MAC2STR(prStaRec->aucMacAddr), u4Entry);

/*
				DBGLOG(RSN, TRACE,
				       "use PMKID " MACSTR "\n",
					MAC2STR(prAdapter->rWifiVar.rAisSpecificBssInfo.
						arPmkidCache[u4Entry].rBssidInfo.arPMKID));
*/
				kalMemCopy(cp,
					   (PVOID) prAdapter->rWifiVar.rAisSpecificBssInfo.
					   arPmkidCache[u4Entry].rBssidInfo.arPMKID, sizeof(PARAM_PMKID_VALUE));
				/* ucExpendedLen = 40; */
			} else {
				WLAN_SET_FIELD_16(cp, 0);	/* PMKID count */
				/* ucExpendedLen = ELEM_ID_RSN_LEN_FIXED + 2; */
#if CFG_SUPPORT_802_11W
				cp += 2;
				RSN_IE(pucBuffer)->ucLength += 2;
#endif
			}
		} else {
			WLAN_SET_FIELD_16(cp, 0);	/* PMKID count */
			/* ucExpendedLen = ELEM_ID_RSN_LEN_FIXED + 2; */
#if CFG_SUPPORT_802_11W
			if ((GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex)->eNetworkType == NETWORK_TYPE_AIS)
			    && prAdapter->rWifiVar.rAisSpecificBssInfo.fgMgmtProtection
			    && (kalGetMfpSetting(prAdapter->prGlueInfo) != RSN_AUTH_MFP_DISABLED)
			    /* (mgmt_group_cipher == WPA_CIPHER_AES_128_CMAC) */
			    ) {
				cp += 2;
				RSN_IE(pucBuffer)->ucLength += 2;
			}
#endif
		}

#if CFG_SUPPORT_802_11W
		if ((GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex)->eNetworkType == NETWORK_TYPE_AIS)
		    && prAdapter->rWifiVar.rAisSpecificBssInfo.fgMgmtProtection &&
		    (kalGetMfpSetting(prAdapter->prGlueInfo) != RSN_AUTH_MFP_DISABLED)
		    /* (mgmt_group_cipher == WPA_CIPHER_AES_128_CMAC) */) {
			WLAN_SET_FIELD_32(cp, RSN_CIPHER_SUITE_AES_128_CMAC);
			cp += 4;
			RSN_IE(pucBuffer)->ucLength += 4;
		}
#endif
		prMsduInfo->u2FrameLength += IE_SIZE(pucBuffer);
	}

}				/* rsnGenerateRSNIE */

/*----------------------------------------------------------------------------*/
/*!
* \brief Parse the given IE buffer and check if it is WFA IE and return Type and
*        SubType for further process.
*
* \param[in] pucBuf             Pointer to the buffer of WFA Information Element.
* \param[out] pucOuiType        Pointer to the storage of OUI Type.
* \param[out] pu2SubTypeVersion Pointer to the storage of OUI SubType and Version.

* \retval TRUE  Parse IE ok
* \retval FALSE Parse IE fail
*/
/*----------------------------------------------------------------------------*/
BOOLEAN
rsnParseCheckForWFAInfoElem(IN P_ADAPTER_T prAdapter,
			    IN PUINT_8 pucBuf, OUT PUINT_8 pucOuiType, OUT PUINT_16 pu2SubTypeVersion)
{
	UINT_8 aucWfaOui[] = VENDOR_OUI_WFA;
	P_IE_WFA_T prWfaIE;

	ASSERT(pucBuf);
	ASSERT(pucOuiType);
	ASSERT(pu2SubTypeVersion);
	prWfaIE = (P_IE_WFA_T) pucBuf;

	do {
		if (IE_LEN(pucBuf) <= ELEM_MIN_LEN_WFA_OUI_TYPE_SUBTYPE) {
			break;
		} else if (prWfaIE->aucOui[0] != aucWfaOui[0] ||
			   prWfaIE->aucOui[1] != aucWfaOui[1] || prWfaIE->aucOui[2] != aucWfaOui[2]) {
			break;
		}

		*pucOuiType = prWfaIE->ucOuiType;
		WLAN_GET_FIELD_16(&prWfaIE->aucOuiSubTypeVersion[0], pu2SubTypeVersion);

		return TRUE;
	} while (FALSE);

	return FALSE;

}				/* end of rsnParseCheckForWFAInfoElem() */

#if CFG_SUPPORT_AAA
/*----------------------------------------------------------------------------*/
/*!
* \brief Parse the given IE buffer and check if it is RSN IE with CCMP PSK
*
* \param[in] prAdapter             Pointer to Adapter
* \param[in] prSwRfb               Pointer to the rx buffer
* \param[in] pIE                      Pointer rthe buffer of Information Element.
* \param[out] prStatusCode     Pointer to the return status code.

* \retval none
*/
/*----------------------------------------------------------------------------*/
void rsnParserCheckForRSNCCMPPSK(P_ADAPTER_T prAdapter, P_RSN_INFO_ELEM_T prIe, PUINT_16 pu2StatusCode)
{

	RSN_INFO_T rRsnIe;

	ASSERT(prAdapter);
	ASSERT(prIe);
	ASSERT(pu2StatusCode);

	*pu2StatusCode = STATUS_CODE_INVALID_INFO_ELEMENT;

	if (rsnParseRsnIE(prAdapter, prIe, &rRsnIe)) {
		if ((rRsnIe.u4PairwiseKeyCipherSuiteCount != 1)
		    || (rRsnIe.au4PairwiseKeyCipherSuite[0] != RSN_CIPHER_SUITE_CCMP)) {
			*pu2StatusCode = STATUS_CODE_INVALID_PAIRWISE_CIPHER;
			return;
		}
		if (rRsnIe.u4GroupKeyCipherSuite != RSN_CIPHER_SUITE_CCMP) {
			*pu2StatusCode = STATUS_CODE_INVALID_GROUP_CIPHER;
			return;
		}
		if ((rRsnIe.u4AuthKeyMgtSuiteCount != 1)
		    || (rRsnIe.au4AuthKeyMgtSuite[0] != RSN_AKM_SUITE_PSK)) {
			*pu2StatusCode = STATUS_CODE_INVALID_AKMP;
			return;
		}

		DBGLOG(RSN, TRACE, "RSN with CCMP-PSK\n");
		*pu2StatusCode = WLAN_STATUS_SUCCESS;
	}

}
#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to generate an authentication event to NDIS.
*
* \param[in] u4Flags Authentication event: \n
*                     PARAM_AUTH_REQUEST_REAUTH 0x01 \n
*                     PARAM_AUTH_REQUEST_KEYUPDATE 0x02 \n
*                     PARAM_AUTH_REQUEST_PAIRWISE_ERROR 0x06 \n
*                     PARAM_AUTH_REQUEST_GROUP_ERROR 0x0E \n
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID rsnGenMicErrorEvent(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgFlags)
{
	P_PARAM_AUTH_EVENT_T prAuthEvent;

	DEBUGFUNC("rsnGenMicErrorEvent");

	prAuthEvent = (P_PARAM_AUTH_EVENT_T) prAdapter->aucIndicationEventBuffer;

	/* Status type: Authentication Event */
	prAuthEvent->rStatus.eStatusType = ENUM_STATUS_TYPE_AUTHENTICATION;

	/* Authentication request */
	prAuthEvent->arRequest[0].u4Length = sizeof(PARAM_AUTH_REQUEST_T);
	kalMemCopy((PVOID) prAuthEvent->arRequest[0].arBssid, (PVOID) prAdapter->prAisBssInfo->aucBSSID, MAC_ADDR_LEN);

	if (fgFlags == TRUE)
		prAuthEvent->arRequest[0].u4Flags = PARAM_AUTH_REQUEST_GROUP_ERROR;
	else
		prAuthEvent->arRequest[0].u4Flags = PARAM_AUTH_REQUEST_PAIRWISE_ERROR;

	kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
				     WLAN_STATUS_MEDIA_SPECIFIC_INDICATION,
				     (PVOID) prAuthEvent,
				     sizeof(PARAM_STATUS_INDICATION_T) + sizeof(PARAM_AUTH_REQUEST_T));

}				/* rsnGenMicErrorEvent */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to handle TKIP MIC failures.
*
* \param[in] adapter_p Pointer to the adapter object data area.
* \param[in] prSta Pointer to the STA which occur MIC Error
* \param[in] fgErrorKeyType type of error key
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID rsnTkipHandleMICFailure(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prSta, IN BOOLEAN fgErrorKeyType)
{
	/* UINT_32               u4RsnaCurrentMICFailTime; */
	/* P_AIS_SPECIFIC_BSS_INFO_T prAisSpecBssInfo; */

	DEBUGFUNC("rsnTkipHandleMICFailure");

	ASSERT(prAdapter);
#if 1
	rsnGenMicErrorEvent(prAdapter, /* prSta, */ fgErrorKeyType);

	/* Generate authentication request event. */
	DBGLOG(RSN, INFO, "Generate TKIP MIC error event (type: 0%d)\n", fgErrorKeyType);
#else
	ASSERT(prSta);

	prAisSpecBssInfo = &prAdapter->rWifiVar.rAisSpecificBssInfo;

	/* Record the MIC error occur time. */
	GET_CURRENT_SYSTIME(&u4RsnaCurrentMICFailTime);

	/* Generate authentication request event. */
	DBGLOG(RSN, INFO, "Generate TKIP MIC error event (type: 0%d)\n", fgErrorKeyType);

	/* If less than 60 seconds have passed since a previous TKIP MIC failure,
	   disassociate from the AP and wait for 60 seconds before (re)associating
	   with the same AP. */
	if (prAisSpecBssInfo->u4RsnaLastMICFailTime != 0 &&
	    !CHECK_FOR_TIMEOUT(u4RsnaCurrentMICFailTime,
			       prAisSpecBssInfo->u4RsnaLastMICFailTime, SEC_TO_SYSTIME(TKIP_COUNTERMEASURE_SEC))) {
		/* If less than 60 seconds expired since last MIC error, we have to
		   block traffic. */

		DBGLOG(RSN, INFO, "Start blocking traffic!\n");
		rsnGenMicErrorEvent(prAdapter, /* prSta, */ fgErrorKeyType);

		secFsmEventStartCounterMeasure(prAdapter, prSta);
	} else {
		rsnGenMicErrorEvent(prAdapter, /* prSta, */ fgErrorKeyType);
		DBGLOG(RSN, INFO, "First TKIP MIC error!\n");
	}

	COPY_SYSTIME(prAisSpecBssInfo->u4RsnaLastMICFailTime, u4RsnaCurrentMICFailTime);
#endif
}				/* rsnTkipHandleMICFailure */

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is called to select a list of BSSID from
*        the scan results for PMKID candidate list.
*
* \param[in] prBssDesc the BSS Desc at scan result list
* \param[out] pu4CandidateCount Pointer to the number of selected candidates.
*                         It is set to zero if no BSSID matches our requirement.
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID rsnSelectPmkidCandidateList(IN P_ADAPTER_T prAdapter, IN P_BSS_DESC_T prBssDesc)
{
	P_CONNECTION_SETTINGS_T prConnSettings;
	P_BSS_INFO_T prAisBssInfo;

	DEBUGFUNC("rsnSelectPmkidCandidateList");

	ASSERT(prBssDesc);

	prConnSettings = &prAdapter->rWifiVar.rConnSettings;
	prAisBssInfo = prAdapter->prAisBssInfo;

	/* Search a BSS with the same SSID from the given BSS description set. */
	/* DBGLOG(RSN, TRACE, ("Check scan result ["MACSTR"]\n", */
	/* MAC2STR(prBssDesc->aucBSSID))); */

	if (UNEQUAL_SSID(prBssDesc->aucSSID, prBssDesc->ucSSIDLen,
		prConnSettings->aucSSID, prConnSettings->ucSSIDLen)) {
		DBGLOG(RSN, TRACE, "-- SSID not matched\n");
		return;
	}
#if 0
	if ((prBssDesc->u2BSSBasicRateSet &
	     ~(rPhyAttributes[prAisBssInfo->ePhyType].u2SupportedRateSet)) || prBssDesc->fgIsUnknownBssBasicRate) {
		DBGLOG(RSN, TRACE, "-- Rate set not matched\n");
		return;
	}

	if (/* prBssDesc->u4RsnSelectedPairwiseCipher != prAisBssInfo->u4RsnSelectedPairwiseCipher || */
		   prBssDesc->u4RsnSelectedGroupCipher != prAisBssInfo->u4RsnSelectedGroupCipher	/*||
		   prBssDesc->u4RsnSelectedAKMSuite != prAisBssInfo->u4RsnSelectedAKMSuite */) {
		DBGLOG(RSN, TRACE, "-- Encrypt status not matched for PMKID\n");
		return;
	}
#endif

	rsnUpdatePmkidCandidateList(prAdapter, prBssDesc);

}				/* rsnSelectPmkidCandidateList */

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is called to select a list of BSSID from
*        the scan results for PMKID candidate list.
*
* \param[in] prBssDesc the BSS DESC at scan result list
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID rsnUpdatePmkidCandidateList(IN P_ADAPTER_T prAdapter, IN P_BSS_DESC_T prBssDesc)
{
	UINT_32 i;
	P_CONNECTION_SETTINGS_T prConnSettings;
	P_AIS_SPECIFIC_BSS_INFO_T prAisSpecBssInfo;

	DEBUGFUNC("rsnUpdatePmkidCandidateList");

	ASSERT(prBssDesc);

	prConnSettings = &prAdapter->rWifiVar.rConnSettings;
	prAisSpecBssInfo = &prAdapter->rWifiVar.rAisSpecificBssInfo;

	if (UNEQUAL_SSID(prBssDesc->aucSSID, prBssDesc->ucSSIDLen,
	    prConnSettings->aucSSID, prConnSettings->ucSSIDLen)) {
		DBGLOG(RSN, TRACE, "-- SSID not matched\n");
		return;
	}

	for (i = 0; i < CFG_MAX_PMKID_CACHE; i++) {
		if (EQUAL_MAC_ADDR(prBssDesc->aucBSSID, prAisSpecBssInfo->arPmkidCandicate[i].aucBssid))
			return;
	}

	/* If the number of selected BSSID exceed MAX_NUM_PMKID_CACHE(16),
	   then we only store MAX_NUM_PMKID_CACHE(16) in PMKID cache */
	if ((prAisSpecBssInfo->u4PmkidCandicateCount + 1) > CFG_MAX_PMKID_CACHE)
		prAisSpecBssInfo->u4PmkidCandicateCount--;

	i = prAisSpecBssInfo->u4PmkidCandicateCount;

	COPY_MAC_ADDR((PVOID) prAisSpecBssInfo->arPmkidCandicate[i].aucBssid, (PVOID) prBssDesc->aucBSSID);

	if (prBssDesc->u2RsnCap & MASK_RSNIE_CAP_PREAUTH) {
		prAisSpecBssInfo->arPmkidCandicate[i].u4PreAuthFlags = 1;
		DBGLOG(RSN, TRACE, "Add " MACSTR " with pre-auth to candidate list\n",
				MAC2STR(prAisSpecBssInfo->arPmkidCandicate[i].aucBssid));
	} else {
		prAisSpecBssInfo->arPmkidCandicate[i].u4PreAuthFlags = 0;
		DBGLOG(RSN, TRACE, "Add " MACSTR " without pre-auth to candidate list\n",
				MAC2STR(prAisSpecBssInfo->arPmkidCandicate[i].aucBssid));
	}

	prAisSpecBssInfo->u4PmkidCandicateCount++;

}				/* rsnUpdatePmkidCandidateList */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to search the desired entry in
*        PMKID cache according to the BSSID
*
* \param[in] pucBssid Pointer to the BSSID
* \param[out] pu4EntryIndex Pointer to place the found entry index
*
* \retval TRUE, if found one entry for specified BSSID
* \retval FALSE, if not found
*/
/*----------------------------------------------------------------------------*/
BOOLEAN rsnSearchPmkidEntry(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucBssid, OUT PUINT_32 pu4EntryIndex)
{
	UINT_32 i;
	P_AIS_SPECIFIC_BSS_INFO_T prAisSpecBssInfo;

	DEBUGFUNC("rsnSearchPmkidEntry");

	ASSERT(pucBssid);
	ASSERT(pu4EntryIndex);

	prAisSpecBssInfo = &prAdapter->rWifiVar.rAisSpecificBssInfo;

	if (prAisSpecBssInfo->u4PmkidCacheCount > CFG_MAX_PMKID_CACHE)
		return FALSE;

	ASSERT(prAisSpecBssInfo->u4PmkidCacheCount <= CFG_MAX_PMKID_CACHE);

	/* Search for desired BSSID */
	for (i = 0; i < prAisSpecBssInfo->u4PmkidCacheCount; i++) {
		if (!kalMemCmp(prAisSpecBssInfo->arPmkidCache[i].rBssidInfo.arBSSID, pucBssid, MAC_ADDR_LEN))
			break;
	}

	/* If desired BSSID is found, then set the PMKID */
	if (i < prAisSpecBssInfo->u4PmkidCacheCount) {
		*pu4EntryIndex = i;

		return TRUE;
	}

	return FALSE;
}				/* rsnSearchPmkidEntry */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to check if there is difference
*        between PMKID candicate list and PMKID cache. If there
*        is new candicate that no cache entry is available, then
*        add a new entry for the new candicate in the PMKID cache
*        and set the PMKID indication flag to TRUE.
*
* \retval TRUE, if new member in the PMKID candicate list
* \retval FALSe, if no new member in the PMKID candicate list
*/
/*----------------------------------------------------------------------------*/
BOOLEAN rsnCheckPmkidCandicate(IN P_ADAPTER_T prAdapter)
{
	P_AIS_SPECIFIC_BSS_INFO_T prAisSpecBssInfo;
	UINT_32 i;		/* Index for PMKID candicate */
	UINT_32 j;		/* Indix for PMKID cache */
	BOOLEAN status = FALSE;

	DEBUGFUNC("rsnCheckPmkidCandicate");

	prAisSpecBssInfo = &prAdapter->rWifiVar.rAisSpecificBssInfo;

	/* Check for each candicate */
	for (i = 0; i < prAisSpecBssInfo->u4PmkidCandicateCount; i++) {
		for (j = 0; j < prAisSpecBssInfo->u4PmkidCacheCount; j++) {
			if (!kalMemCmp(prAisSpecBssInfo->arPmkidCache[j].rBssidInfo.arBSSID,
				       prAisSpecBssInfo->arPmkidCandicate[i].aucBssid, MAC_ADDR_LEN)) {
				/* DBGLOG(RSN, TRACE, (MACSTR" at PMKID cache!!\n",
				 * MAC2STR(prAisSpecBssInfo->arPmkidCandicate[i].aucBssid))); */
				break;
			}
		}

		/* No entry found in PMKID cache for the candicate, add new one */
		if (j == prAisSpecBssInfo->u4PmkidCacheCount
		    && prAisSpecBssInfo->u4PmkidCacheCount < CFG_MAX_PMKID_CACHE) {
			DBGLOG(RSN, TRACE,
			       "Add " MACSTR " to PMKID cache!!\n",
				MAC2STR(prAisSpecBssInfo->arPmkidCandicate[i].aucBssid));
			kalMemCopy((PVOID) prAisSpecBssInfo->
				   arPmkidCache[prAisSpecBssInfo->u4PmkidCacheCount].rBssidInfo.arBSSID,
				   (PVOID) prAisSpecBssInfo->arPmkidCandicate[i].aucBssid, MAC_ADDR_LEN);
			prAisSpecBssInfo->arPmkidCache[prAisSpecBssInfo->u4PmkidCacheCount].fgPmkidExist = FALSE;
			prAisSpecBssInfo->u4PmkidCacheCount++;

			status = TRUE;
		}
	}

	return status;
}				/* rsnCheckPmkidCandicate */

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is called to wait a duration to indicate the pre-auth AP candicate
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID rsnIndicatePmkidCand(IN P_ADAPTER_T prAdapter, IN ULONG ulParamPtr)
{
	DBGLOG(RSN, EVENT, "Security - Time to indicate the PMKID cand.\n");

	/* If the authentication mode is WPA2 and indication PMKID flag
	   is available, then we indicate the PMKID candidate list to NDIS and
	   clear the flag, indicatePMKID */

	if (prAdapter->prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED &&
	    prAdapter->rWifiVar.rConnSettings.eAuthMode == AUTH_MODE_WPA2) {
		rsnGeneratePmkidIndication(prAdapter);
	}

}				/* end of rsnIndicatePmkidCand() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to check the BSS Desc at scan result
*             with pre-auth cap at wpa2 mode. If there
*             is candicate that no cache entry is available, then
*             add a new entry for the new candicate in the PMKID cache
*             and set the PMKID indication flag to TRUE.
*
* \param[in] prBss The BSS Desc at scan result
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID rsnCheckPmkidCache(IN P_ADAPTER_T prAdapter, IN P_BSS_DESC_T prBss)
{
	P_BSS_INFO_T prAisBssInfo;
	P_AIS_SPECIFIC_BSS_INFO_T prAisSpecBssInfo;
	P_CONNECTION_SETTINGS_T prConnSettings;

	DEBUGFUNC("rsnCheckPmkidCandicate");

	ASSERT(prBss);

	prConnSettings = &prAdapter->rWifiVar.rConnSettings;
	prAisBssInfo = prAdapter->prAisBssInfo;
	prAisSpecBssInfo = &prAdapter->rWifiVar.rAisSpecificBssInfo;

	if ((prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED) &&
	    (prConnSettings->eAuthMode == AUTH_MODE_WPA2)) {
		rsnSelectPmkidCandidateList(prAdapter, prBss);

		/* Set indication flag of PMKID to TRUE, and then connHandleNetworkConnection()
		   will indicate this later */
		if (rsnCheckPmkidCandicate(prAdapter)) {
			DBGLOG(RSN, TRACE, "Prepare a timer to indicate candidate PMKID Candidate\n");
			cnmTimerStopTimer(prAdapter, &prAisSpecBssInfo->rPreauthenticationTimer);
			cnmTimerStartTimer(prAdapter, &prAisSpecBssInfo->rPreauthenticationTimer,
					   SEC_TO_MSEC(WAIT_TIME_IND_PMKID_CANDICATE_SEC));
		}
	}
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to generate an PMKID candidate list
*        indication to NDIS.
*
* \param[in] prAdapter Pointer to the adapter object data area.
* \param[in] u4Flags PMKID candidate list event:
*                    PARAM_PMKID_CANDIDATE_PREAUTH_ENABLED 0x01
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID rsnGeneratePmkidIndication(IN P_ADAPTER_T prAdapter)
{
	P_PARAM_STATUS_INDICATION_T prStatusEvent;
	P_PARAM_PMKID_CANDIDATE_LIST_T prPmkidEvent;
	P_AIS_SPECIFIC_BSS_INFO_T prAisSpecificBssInfo;
	UINT_8 i, j = 0, count = 0;
	UINT_32 u4LenOfUsedBuffer;

	DEBUGFUNC("rsnGeneratePmkidIndication");

	ASSERT(prAdapter);

	prStatusEvent = (P_PARAM_STATUS_INDICATION_T) prAdapter->aucIndicationEventBuffer;

	/* Status type: PMKID Candidatelist Event */
	prStatusEvent->eStatusType = ENUM_STATUS_TYPE_CANDIDATE_LIST;
	ASSERT(prStatusEvent);

	prPmkidEvent = (P_PARAM_PMKID_CANDIDATE_LIST_T) (&prStatusEvent->eStatusType + 1);
	ASSERT(prPmkidEvent);

	prAisSpecificBssInfo = &prAdapter->rWifiVar.rAisSpecificBssInfo;
	ASSERT(prAisSpecificBssInfo);

	for (i = 0; i < prAisSpecificBssInfo->u4PmkidCandicateCount; i++) {
		for (j = 0; j < prAisSpecificBssInfo->u4PmkidCacheCount; j++) {
			if (EQUAL_MAC_ADDR(prAisSpecificBssInfo->arPmkidCache[j].rBssidInfo.arBSSID,
					   prAisSpecificBssInfo->arPmkidCandicate[i].aucBssid) &&
			    (prAisSpecificBssInfo->arPmkidCache[j].fgPmkidExist == TRUE)) {
				break;
			}
		}
		if (count >= CFG_MAX_PMKID_CACHE)
			break;

		if (j == prAisSpecificBssInfo->u4PmkidCacheCount) {
			kalMemCopy((PVOID) prPmkidEvent->arCandidateList[count].arBSSID,
				   (PVOID) prAisSpecificBssInfo->arPmkidCandicate[i].aucBssid, PARAM_MAC_ADDR_LEN);
			prPmkidEvent->arCandidateList[count].u4Flags =
			    prAisSpecificBssInfo->arPmkidCandicate[i].u4PreAuthFlags;
			DBGLOG(RSN, TRACE,
			       MACSTR " %u\n",
				MAC2STR(prPmkidEvent->arCandidateList[count].arBSSID),
				prPmkidEvent->arCandidateList[count].u4Flags);
			count++;
		}
	}

	/* PMKID Candidate List */
	prPmkidEvent->u4Version = 1;
	prPmkidEvent->u4NumCandidates = count;
	DBGLOG(RSN, TRACE, "rsnGeneratePmkidIndication #%u\n", prPmkidEvent->u4NumCandidates);
	u4LenOfUsedBuffer = sizeof(ENUM_STATUS_TYPE_T) + (2 * sizeof(UINT_32)) +
	    (count * sizeof(PARAM_PMKID_CANDIDATE_T));
	/* dumpMemory8((PUINT_8)prAdapter->aucIndicationEventBuffer, u4LenOfUsedBuffer); */

	kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
				     WLAN_STATUS_MEDIA_SPECIFIC_INDICATION,
				     (PVOID) prAdapter->aucIndicationEventBuffer, u4LenOfUsedBuffer);

}				/* rsnGeneratePmkidIndication */

#if CFG_SUPPORT_WPS2
/*----------------------------------------------------------------------------*/
/*!
*
* \brief This routine is called to generate WSC IE for
*        associate request frame.
*
* \param[in]  prCurrentBss     The Selected BSS description
*
* \retval The append WSC IE length
*
* \note
*      Called by: AIS module, Associate request
*/
/*----------------------------------------------------------------------------*/
VOID rsnGenerateWSCIE(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo)
{
	PUINT_8 pucBuffer;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

	if (prMsduInfo->ucBssIndex != prAdapter->prAisBssInfo->ucBssIndex)
		return;

	pucBuffer = (PUINT_8) ((ULONG) prMsduInfo->prPacket + (ULONG) prMsduInfo->u2FrameLength);

	/* ASSOC INFO IE ID: 221 :0xDD */
	if (prAdapter->prGlueInfo->u2WSCAssocInfoIELen) {
		kalMemCopy(pucBuffer, &prAdapter->prGlueInfo->aucWSCAssocInfoIE,
			   prAdapter->prGlueInfo->u2WSCAssocInfoIELen);
		prMsduInfo->u2FrameLength += prAdapter->prGlueInfo->u2WSCAssocInfoIELen;
	}

}
#endif

#if CFG_SUPPORT_802_11W

/*----------------------------------------------------------------------------*/
/*!
* \brief to check if the Bip Key installed or not
*
* \param[in]
*           prAdapter
*
* \return
*           TRUE
*           FALSE
*/
/*----------------------------------------------------------------------------*/
UINT_32 rsnCheckBipKeyInstalled(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec)
{
	if (prStaRec
	    && GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex)->eNetworkType == (UINT_8) NETWORK_TYPE_AIS)
		return prAdapter->rWifiVar.rAisSpecificBssInfo.fgBipKeyInstalled;
	else
		return FALSE;
}

/*----------------------------------------------------------------------------*/
/*!
*
* \brief This routine is called to check the Sa query timeout.
*
*
* \note
*      Called by: AIS module, Handle by Sa Quert timeout
*/
/*----------------------------------------------------------------------------*/
UINT_8 rsnCheckSaQueryTimeout(IN P_ADAPTER_T prAdapter)
{
	P_AIS_SPECIFIC_BSS_INFO_T prBssSpecInfo;
	UINT_32 now;

	prBssSpecInfo = &prAdapter->rWifiVar.rAisSpecificBssInfo;
	ASSERT(prBssSpecInfo);

	GET_CURRENT_SYSTIME(&now);

	if (CHECK_FOR_TIMEOUT(now, prBssSpecInfo->u4SaQueryStart, TU_TO_MSEC(1000))) {
		DBGLOG(RSN, INFO, "association SA Query timed out\n");

		prBssSpecInfo->ucSaQueryTimedOut = 1;
		kalMemFree(prBssSpecInfo->pucSaQueryTransId, VIR_MEM_TYPE,
			   prBssSpecInfo->u4SaQueryCount * ACTION_SA_QUERY_TR_ID_LEN);
		prBssSpecInfo->pucSaQueryTransId = NULL;
		prBssSpecInfo->u4SaQueryCount = 0;
		cnmTimerStopTimer(prAdapter, &prBssSpecInfo->rSaQueryTimer);
#if 1
		if (prAdapter->prAisBssInfo->eConnectionState ==
		    PARAM_MEDIA_STATE_CONNECTED /* STA_STATE_3 == prStaRec->ucStaState */) {
			P_MSG_AIS_ABORT_T prAisAbortMsg;

			prAisAbortMsg =
			    (P_MSG_AIS_ABORT_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_AIS_ABORT_T));
			if (!prAisAbortMsg)
				return 0;
			prAisAbortMsg->rMsgHdr.eMsgId = MID_SAA_AIS_FSM_ABORT;
			prAisAbortMsg->ucReasonOfDisconnect = DISCONNECT_REASON_CODE_DISASSOCIATED;
			prAisAbortMsg->fgDelayIndication = FALSE;

			mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prAisAbortMsg, MSG_SEND_METHOD_BUF);
		}
#else
		/* Re-connect */
		kalIndicateStatusAndComplete(prAdapter->prGlueInfo, WLAN_STATUS_MEDIA_DISCONNECT, NULL, 0);
#endif
		return 1;
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
*
* \brief This routine is called to start the 802.11w sa query timer.
*
*
* \note
*      Called by: AIS module, Handle Rx mgmt request
*/
/*----------------------------------------------------------------------------*/
void rsnStartSaQueryTimer(IN P_ADAPTER_T prAdapter, IN ULONG ulParamPtr)
{
	P_BSS_INFO_T prBssInfo;
	P_AIS_SPECIFIC_BSS_INFO_T prBssSpecInfo;
	P_MSDU_INFO_T prMsduInfo;
	P_ACTION_SA_QUERY_FRAME prTxFrame;
	UINT_16 u2PayloadLen;
	PUINT_8 pucTmp = NULL;
	UINT_8 ucTransId[ACTION_SA_QUERY_TR_ID_LEN];

	prBssInfo = prAdapter->prAisBssInfo;
	ASSERT(prBssInfo);

	prBssSpecInfo = &prAdapter->rWifiVar.rAisSpecificBssInfo;
	ASSERT(prBssSpecInfo);

	DBGLOG(RSN, INFO, "MFP: Start Sa Query\n");

	if (prBssInfo->prStaRecOfAP == NULL) {
		DBGLOG(RSN, INFO, "MFP: unassociated AP!\n");
		return;
	}

	if (prBssSpecInfo->u4SaQueryCount > 0 && rsnCheckSaQueryTimeout(prAdapter)) {
		DBGLOG(RSN, INFO, "MFP: u4SaQueryCount count =%u\n", prBssSpecInfo->u4SaQueryCount);
		return;
	}

	prMsduInfo = (P_MSDU_INFO_T) cnmMgtPktAlloc(prAdapter, MAC_TX_RESERVED_FIELD + PUBLIC_ACTION_MAX_LEN);

	if (!prMsduInfo)
		return;

	prTxFrame = (P_ACTION_SA_QUERY_FRAME)
	    ((ULONG) (prMsduInfo->prPacket) + MAC_TX_RESERVED_FIELD);

	prTxFrame->u2FrameCtrl = MAC_FRAME_ACTION;
	if (rsnCheckBipKeyInstalled(prAdapter, prBssInfo->prStaRecOfAP))
		prTxFrame->u2FrameCtrl |= MASK_FC_PROTECTED_FRAME;
	COPY_MAC_ADDR(prTxFrame->aucDestAddr, prBssInfo->aucBSSID);
	COPY_MAC_ADDR(prTxFrame->aucSrcAddr, prBssInfo->aucOwnMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucBSSID, prBssInfo->aucBSSID);

	prTxFrame->ucCategory = CATEGORY_SA_QUERY_ACTION;
	prTxFrame->ucAction = ACTION_SA_QUERY_REQUEST;

	if (prBssSpecInfo->u4SaQueryCount == 0)
		GET_CURRENT_SYSTIME(&prBssSpecInfo->u4SaQueryStart);

	if (prBssSpecInfo->u4SaQueryCount) {
		pucTmp = kalMemAlloc(prBssSpecInfo->u4SaQueryCount * ACTION_SA_QUERY_TR_ID_LEN, VIR_MEM_TYPE);
		if (!pucTmp) {
			DBGLOG(RSN, INFO, "MFP: Fail to alloc tmp buffer for backup sa query id\n");
			cnmMgtPktFree(prAdapter, prMsduInfo);
			return;
		}
		kalMemCopy(pucTmp, prBssSpecInfo->pucSaQueryTransId,
			   prBssSpecInfo->u4SaQueryCount * ACTION_SA_QUERY_TR_ID_LEN);
	}

	kalMemFree(prBssSpecInfo->pucSaQueryTransId, VIR_MEM_TYPE,
		   prBssSpecInfo->u4SaQueryCount * ACTION_SA_QUERY_TR_ID_LEN);

	ucTransId[0] = (UINT_8) (kalRandomNumber() & 0xFF);
	ucTransId[1] = (UINT_8) (kalRandomNumber() & 0xFF);

	kalMemCopy(prTxFrame->ucTransId, ucTransId, ACTION_SA_QUERY_TR_ID_LEN);

	prBssSpecInfo->u4SaQueryCount++;

	prBssSpecInfo->pucSaQueryTransId =
	    kalMemAlloc(prBssSpecInfo->u4SaQueryCount * ACTION_SA_QUERY_TR_ID_LEN, VIR_MEM_TYPE);
	if (!prBssSpecInfo->pucSaQueryTransId) {
		kalMemFree(pucTmp, VIR_MEM_TYPE, (prBssSpecInfo->u4SaQueryCount - 1) * ACTION_SA_QUERY_TR_ID_LEN);
		DBGLOG(RSN, INFO, "MFP: Fail to alloc buffer for sa query id list\n");
		cnmMgtPktFree(prAdapter, prMsduInfo);
		return;
	}

	if (pucTmp) {
		kalMemCopy(prBssSpecInfo->pucSaQueryTransId, pucTmp,
			   (prBssSpecInfo->u4SaQueryCount - 1) * ACTION_SA_QUERY_TR_ID_LEN);
		kalMemCopy(&prBssSpecInfo->pucSaQueryTransId[(prBssSpecInfo->u4SaQueryCount -
							      1) * ACTION_SA_QUERY_TR_ID_LEN], ucTransId,
			   ACTION_SA_QUERY_TR_ID_LEN);
		kalMemFree(pucTmp, VIR_MEM_TYPE, (prBssSpecInfo->u4SaQueryCount - 1) * ACTION_SA_QUERY_TR_ID_LEN);
	} else {
		kalMemCopy(prBssSpecInfo->pucSaQueryTransId, ucTransId, ACTION_SA_QUERY_TR_ID_LEN);
	}

	u2PayloadLen = 2 + ACTION_SA_QUERY_TR_ID_LEN;

	/* 4 <3> Update information of MSDU_INFO_T */
	TX_SET_MMPDU(prAdapter,
		     prMsduInfo,
		     prBssInfo->prStaRecOfAP->ucBssIndex,
		     prBssInfo->prStaRecOfAP->ucIndex,
		     WLAN_MAC_MGMT_HEADER_LEN, WLAN_MAC_MGMT_HEADER_LEN + u2PayloadLen, NULL, MSDU_RATE_MODE_AUTO);

	if (rsnCheckBipKeyInstalled(prAdapter, prBssInfo->prStaRecOfAP)) {
		DBGLOG(RSN, INFO, "Set MSDU_OPT_PROTECTED_FRAME\n");
		nicTxConfigPktOption(prMsduInfo, MSDU_OPT_PROTECTED_FRAME, TRUE);
	}
	/* 4 Enqueue the frame to send this action frame. */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);

	DBGLOG(RSN, INFO, "Set SA Query timer %u (%d Tu)\n", prBssSpecInfo->u4SaQueryCount, 201);

	cnmTimerStartTimer(prAdapter, &prBssSpecInfo->rSaQueryTimer, TU_TO_MSEC(201));

}

/*----------------------------------------------------------------------------*/
/*!
*
* \brief This routine is called to start the 802.11w sa query.
*
*
* \note
*      Called by: AIS module, Handle Rx mgmt request
*/
/*----------------------------------------------------------------------------*/
void rsnStartSaQuery(IN P_ADAPTER_T prAdapter)
{
	P_AIS_SPECIFIC_BSS_INFO_T prBssSpecInfo;

	prBssSpecInfo = &prAdapter->rWifiVar.rAisSpecificBssInfo;
	ASSERT(prBssSpecInfo);

	if (prBssSpecInfo->u4SaQueryCount == 0)
		rsnStartSaQueryTimer(prAdapter, (ULONG) NULL);
}

/*----------------------------------------------------------------------------*/
/*!
*
* \brief This routine is called to stop the 802.11w sa query.
*
*
* \note
*      Called by: AIS module, Handle Rx mgmt request
*/
/*----------------------------------------------------------------------------*/
void rsnStopSaQuery(IN P_ADAPTER_T prAdapter)
{
	P_AIS_SPECIFIC_BSS_INFO_T prBssSpecInfo;

	prBssSpecInfo = &prAdapter->rWifiVar.rAisSpecificBssInfo;
	ASSERT(prBssSpecInfo);

	cnmTimerStopTimer(prAdapter, &prBssSpecInfo->rSaQueryTimer);
	kalMemFree(prBssSpecInfo->pucSaQueryTransId, VIR_MEM_TYPE,
		   prBssSpecInfo->u4SaQueryCount * ACTION_SA_QUERY_TR_ID_LEN);
	prBssSpecInfo->pucSaQueryTransId = NULL;
	prBssSpecInfo->u4SaQueryCount = 0;
}

/*----------------------------------------------------------------------------*/
/*!
*
* \brief This routine is called to process the 802.11w sa query action frame.
*
*
* \note
*      Called by: AIS module, Handle Rx mgmt request
*/
/*----------------------------------------------------------------------------*/
void rsnSaQueryRequest(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
	P_BSS_INFO_T prBssInfo;
	P_MSDU_INFO_T prMsduInfo;
	P_ACTION_SA_QUERY_FRAME prRxFrame = NULL;
	UINT_16 u2PayloadLen;
	P_STA_RECORD_T prStaRec;
	P_ACTION_SA_QUERY_FRAME prTxFrame;

	prBssInfo = prAdapter->prAisBssInfo;
	ASSERT(prBssInfo);

	if (!prSwRfb)
		return;

	prRxFrame = (P_ACTION_SA_QUERY_FRAME) prSwRfb->pvHeader;
	if (!prRxFrame)
		return;

	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);
	if (!prStaRec)	/* Todo:: for not AIS check */
		return;

	DBGLOG(RSN, INFO, "IEEE 802.11: Received SA Query Request from " MACSTR "\n",
		MAC2STR(prStaRec->aucMacAddr));

	DBGLOG_MEM8(RSN, INFO, prRxFrame->ucTransId, ACTION_SA_QUERY_TR_ID_LEN);

	if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) == PARAM_MEDIA_STATE_DISCONNECTED) {
		DBGLOG(RSN, INFO, "IEEE 802.11: Ignore SA Query Request from unassociated STA " MACSTR "\n",
			MAC2STR(prStaRec->aucMacAddr));
		return;
	}

	DBGLOG(RSN, INFO, "IEEE 802.11: Sending SA Query Response to " MACSTR "\n", MAC2STR(prStaRec->aucMacAddr));

	prMsduInfo = (P_MSDU_INFO_T) cnmMgtPktAlloc(prAdapter, MAC_TX_RESERVED_FIELD + PUBLIC_ACTION_MAX_LEN);

	if (!prMsduInfo)
		return;

	prTxFrame = (P_ACTION_SA_QUERY_FRAME)
	    ((ULONG) (prMsduInfo->prPacket) + MAC_TX_RESERVED_FIELD);

	prTxFrame->u2FrameCtrl = MAC_FRAME_ACTION;
	if (rsnCheckBipKeyInstalled(prAdapter, prBssInfo->prStaRecOfAP))
		prTxFrame->u2FrameCtrl |= MASK_FC_PROTECTED_FRAME;
	COPY_MAC_ADDR(prTxFrame->aucDestAddr, prBssInfo->aucBSSID);
	COPY_MAC_ADDR(prTxFrame->aucSrcAddr, prBssInfo->aucOwnMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucBSSID, prBssInfo->aucBSSID);

	prTxFrame->ucCategory = CATEGORY_SA_QUERY_ACTION;
	prTxFrame->ucAction = ACTION_SA_QUERY_RESPONSE;

	kalMemCopy(prTxFrame->ucTransId, prRxFrame->ucTransId, ACTION_SA_QUERY_TR_ID_LEN);

	u2PayloadLen = 2 + ACTION_SA_QUERY_TR_ID_LEN;

	/* 4 <3> Update information of MSDU_INFO_T */
	TX_SET_MMPDU(prAdapter,
		     prMsduInfo,
		     prBssInfo->prStaRecOfAP->ucBssIndex,
		     prBssInfo->prStaRecOfAP->ucIndex,
		     WLAN_MAC_MGMT_HEADER_LEN, WLAN_MAC_MGMT_HEADER_LEN + u2PayloadLen, NULL, MSDU_RATE_MODE_AUTO);

	if (rsnCheckBipKeyInstalled(prAdapter, prBssInfo->prStaRecOfAP)) {
		DBGLOG(RSN, INFO, "Set MSDU_OPT_PROTECTED_FRAME\n");
		nicTxConfigPktOption(prMsduInfo, MSDU_OPT_PROTECTED_FRAME, TRUE);
	}
#if 0
	/* 4 Update information of MSDU_INFO_T */
	prMsduInfo->ucPacketType = HIF_TX_PACKET_TYPE_MGMT;	/* Management frame */
	prMsduInfo->ucStaRecIndex = prBssInfo->prStaRecOfAP->ucIndex;
	prMsduInfo->ucNetworkType = prBssInfo->ucNetTypeIndex;
	prMsduInfo->ucMacHeaderLength = WLAN_MAC_MGMT_HEADER_LEN;
	prMsduInfo->fgIs802_1x = FALSE;
	prMsduInfo->fgIs802_11 = TRUE;
	prMsduInfo->u2FrameLength = WLAN_MAC_MGMT_HEADER_LEN + u2PayloadLen;
	prMsduInfo->ucPID = nicAssignPID(prAdapter);
	prMsduInfo->pfTxDoneHandler = NULL;
	prMsduInfo->fgIsBasicRate = FALSE;
#endif
	/* 4 Enqueue the frame to send this action frame. */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);

}

/*----------------------------------------------------------------------------*/
/*!
*
* \brief This routine is called to process the 802.11w sa query action frame.
*
*
* \note
*      Called by: AIS module, Handle Rx mgmt request
*/
/*----------------------------------------------------------------------------*/
void rsnSaQueryAction(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
	P_AIS_SPECIFIC_BSS_INFO_T prBssSpecInfo;
	P_ACTION_SA_QUERY_FRAME prRxFrame;
	P_STA_RECORD_T prStaRec;
	UINT_32 i;

	prBssSpecInfo = &prAdapter->rWifiVar.rAisSpecificBssInfo;
	ASSERT(prBssSpecInfo);

	prRxFrame = (P_ACTION_SA_QUERY_FRAME) prSwRfb->pvHeader;
	if (!prRxFrame)
		return;
	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);
	if (!prStaRec)
		return;
	if (prSwRfb->u2PacketLen < ACTION_SA_QUERY_TR_ID_LEN) {
		DBGLOG(RSN, INFO, "IEEE 802.11: Too short SA Query Action frame (len=%lu)\n",
			(unsigned long)prSwRfb->u2PacketLen);
		return;
	}

	if (prRxFrame->ucAction == ACTION_SA_QUERY_REQUEST) {
		rsnSaQueryRequest(prAdapter, prSwRfb);
		return;
	}

	if (prRxFrame->ucAction != ACTION_SA_QUERY_RESPONSE) {
		DBGLOG(RSN, INFO, "IEEE 802.11: Unexpected SA Query " "Action %d\n", prRxFrame->ucAction);
		return;
	}

	DBGLOG(RSN, INFO, "IEEE 802.11: Received SA Query Response from " MACSTR "\n", MAC2STR(prStaRec->aucMacAddr));

	DBGLOG_MEM8(RSN, INFO, prRxFrame->ucTransId, ACTION_SA_QUERY_TR_ID_LEN);

	/* MLME-SAQuery.confirm */

	for (i = 0; i < prBssSpecInfo->u4SaQueryCount; i++) {
		if (kalMemCmp(prBssSpecInfo->pucSaQueryTransId +
			      i * ACTION_SA_QUERY_TR_ID_LEN, prRxFrame->ucTransId, ACTION_SA_QUERY_TR_ID_LEN) == 0)
			break;
	}

	if (i >= prBssSpecInfo->u4SaQueryCount) {
		DBGLOG(RSN, INFO, "IEEE 802.11: No matching SA Query " "transaction identifier found\n");
		return;
	}

	DBGLOG(RSN, INFO, "Reply to pending SA Query received\n");

	rsnStopSaQuery(prAdapter);
}
#endif

#if CFG_SUPPORT_DETECT_SECURITY_MODE_CHANGE
static BOOLEAN rsnCheckWpaRsnInfo(P_BSS_INFO_T prBss, P_RSN_INFO_T prWpaRsnInfo)
{
	UINT_32 i = 0;

	if (prWpaRsnInfo->u4GroupKeyCipherSuite != prBss->u4RsnSelectedGroupCipher) {
		DBGLOG(RSN, INFO, "GroupCipherSuite change, old=0x%04x, new=0x%04x\n",
				prBss->u4RsnSelectedGroupCipher, prWpaRsnInfo->u4GroupKeyCipherSuite);
		return TRUE;
	}
	for (; i < prWpaRsnInfo->u4AuthKeyMgtSuiteCount; i++)
		if (prBss->u4RsnSelectedAKMSuite == prWpaRsnInfo->au4AuthKeyMgtSuite[i])
			break;
	if (i == prWpaRsnInfo->u4AuthKeyMgtSuiteCount) {
		DBGLOG(RSN, INFO, "KeyMgmt change, not find 0x%04x in new beacon\n",
				prBss->u4RsnSelectedAKMSuite);
		return TRUE;
	}

	for (i = 0; i < prWpaRsnInfo->u4PairwiseKeyCipherSuiteCount; i++)
		if (prBss->u4RsnSelectedPairwiseCipher == prWpaRsnInfo->au4PairwiseKeyCipherSuite[i])
			break;
	if (i == prWpaRsnInfo->u4PairwiseKeyCipherSuiteCount) {
		DBGLOG(RSN, INFO, "Pairwise Cipher change, not find 0x%04x in new beacon\n",
				prBss->u4RsnSelectedPairwiseCipher);
		return TRUE;
	}

	return FALSE;
}

BOOLEAN rsnCheckSecurityModeChanged(P_ADAPTER_T prAdapter, P_BSS_INFO_T prBssInfo, P_BSS_DESC_T prBssDesc)
{
	ENUM_PARAM_AUTH_MODE_T eAuthMode = prAdapter->rWifiVar.rConnSettings.eAuthMode;

	switch (eAuthMode) {
	case AUTH_MODE_OPEN: /* original is open system */
		if ((prBssDesc->u2CapInfo & CAP_INFO_PRIVACY) &&
			!prAdapter->prGlueInfo->rWpaInfo.fgPrivacyInvoke) {
			DBGLOG(RSN, INFO, "security change, open->privacy\n");
			return TRUE;
		}
		break;
	case AUTH_MODE_SHARED: /* original is WEP */
	case AUTH_MODE_AUTO_SWITCH:
		if ((prBssDesc->u2CapInfo & CAP_INFO_PRIVACY) == 0) {
			DBGLOG(RSN, INFO, "security change, WEP->open\n");
			return TRUE;
		} else if (prBssDesc->fgIERSN || prBssDesc->fgIEWPA) {
			DBGLOG(RSN, INFO, "security change, WEP->WPA/WPA2\n");
			return TRUE;
		}
		break;
	case AUTH_MODE_WPA: /*original is WPA */
	case AUTH_MODE_WPA_PSK:
	case AUTH_MODE_WPA_NONE:
		if (prBssDesc->fgIEWPA)
			return rsnCheckWpaRsnInfo(prBssInfo, &prBssDesc->rWPAInfo);
		DBGLOG(RSN, INFO, "security change, WPA->%s\n",
				prBssDesc->fgIERSN ? "WPA2" :
				(prBssDesc->u2CapInfo & CAP_INFO_PRIVACY ? "WEP" : "OPEN"));
		return TRUE;
	case AUTH_MODE_WPA2: /*original is WPA2 */
	case AUTH_MODE_WPA2_PSK:
		if (prBssDesc->fgIERSN)
			return rsnCheckWpaRsnInfo(prBssInfo, &prBssDesc->rRSNInfo);
		DBGLOG(RSN, INFO, "security change, WPA2->%s\n",
				prBssDesc->fgIEWPA ? "WPA" :
				(prBssDesc->u2CapInfo & CAP_INFO_PRIVACY ? "WEP" : "OPEN"));
		return TRUE;
	default:
		DBGLOG(RSN, WARN, "unknowned eAuthMode=%d\n", eAuthMode);
		break;
	}
	/*DBGLOG(RSN, INFO, "rsnCheckSecurityModeChanged, eAuthMode=%d, u2CapInfo=0x%02x, fgIEWPA=%d, fgIERSN=%d\n",
			eAuthMode, prBssDesc->u2CapInfo, prBssDesc->fgIEWPA, prBssDesc->fgIERSN);*/
	return FALSE;
}
#endif

#if CFG_SUPPORT_AAA
#define WPS_DEV_OUI_WFA                 0x0050f204
#define ATTR_RESPONSE_TYPE              0x103b

#define ATTR_VERSION                    0x104a
#define ATTR_VENDOR_EXT                 0x1049
#define WPS_VENDOR_ID_WFA               14122

VOID rsnGenerateWSCIEForAssocRsp(P_ADAPTER_T prAdapter, P_MSDU_INFO_T prMsduInfo)
{
	P_IE_HDR_T PucIE;
	PUINT_8 puc;
	P_WIFI_VAR_T prWifiVar = NULL;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);
	ASSERT(IS_NET_ACTIVE(prAdapter, prMsduInfo->ucBssIndex));

	prWifiVar = &(prAdapter->rWifiVar);
	ASSERT(prWifiVar);

	if (!prWifiVar->ucApWpsMode)	/* Todo::Only at WPS certification ? */
		return;

	DBGLOG(RSN, TRACE, "WPS: Building WPS IE for (Re)Association Response");

	PucIE = (P_IE_HDR_T) (((PUINT_8) prMsduInfo->prPacket) + prMsduInfo->u2FrameLength);

	PucIE->ucId = ELEM_ID_VENDOR;

	puc = PucIE->aucInfo;

	WLAN_SET_FIELD_BE32(puc, WPS_DEV_OUI_WFA);
	puc += 4;

	WLAN_SET_FIELD_BE16(puc, ATTR_VERSION);
	puc += 2;
	WLAN_SET_FIELD_BE16(puc, 1);
	puc += 2;
	*puc = 0x10;
	puc++;

	WLAN_SET_FIELD_BE16(puc, ATTR_RESPONSE_TYPE);
	puc += 2;
	WLAN_SET_FIELD_BE16(puc, 1);
	puc += 2;
	*puc = 0x3;
	puc++;

	WLAN_SET_FIELD_BE16(puc, ATTR_VENDOR_EXT);
	puc += 2;
	WLAN_SET_FIELD_BE16(puc, 6);
	puc += 2;
	WLAN_SET_FIELD_BE24(puc, 14122);
	puc += 3;
	*puc = 0x00;		/* Version 2 */
	puc++;
	*puc = 0x01;
	puc++;
	*puc = 0x20;		/* WPS2.0 */
	puc++;

	PucIE->ucLength = puc - PucIE->aucInfo;

	prMsduInfo->u2FrameLength += IE_SIZE(PucIE);

}

#endif
