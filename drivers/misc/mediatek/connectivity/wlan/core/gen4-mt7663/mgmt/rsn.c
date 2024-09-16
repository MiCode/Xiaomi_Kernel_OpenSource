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
 * Id: mgmt/rsn.c#3
 */

/*! \file   "rsn.c"
 *  \brief  This file including the 802.11i, wpa and wpa2(rsn) related function.
 *
 *    This file provided the macros and functions library to support
 *	the wpa/rsn ie parsing, cipher and AKM check to help the AP seleced
 *	deciding, tkip mic error handler and rsn PMKID support.
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

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to parse RSN IE.
 *
 * \param[in]  prInfoElem Pointer to the RSN IE
 * \param[out] prRsnInfo Pointer to the BSSDescription structure to store the
 *                  RSN information from the given RSN IE
 *
 * \retval TRUE - Succeeded
 * \retval FALSE - Failed
 */
/*----------------------------------------------------------------------------*/
u_int8_t rsnParseRsnIE(IN struct ADAPTER *prAdapter,
		       IN struct RSN_INFO_ELEM *prInfoElem,
		       OUT struct RSN_INFO *prRsnInfo)
{
	uint32_t i;
	int32_t u4RemainRsnIeLen;
	uint16_t u2Version;
	uint16_t u2Cap = 0;
	uint32_t u4GroupSuite = RSN_CIPHER_SUITE_CCMP;
	uint32_t u4GroupMgmtSuite = 0;
	uint16_t u2PairSuiteCount = 0;
	uint16_t u2AuthSuiteCount = 0;
	uint8_t *pucPairSuite = NULL;
	uint8_t *pucAuthSuite = NULL;
	uint8_t *cp;
#if CFG_SUPPORT_CFG80211_AUTH
	uint16_t u2DesiredPmkidCnt = 0;
	uint16_t u2SupportedPmkidCnt = 0;
#endif

	DEBUGFUNC("rsnParseRsnIE");

	ASSERT(prInfoElem);
	ASSERT(prRsnInfo);

	/* Verify the length of the RSN IE. */
	if (prInfoElem->ucLength < 2) {
		DBGLOG(RSN, TRACE, "RSN IE length too short (length=%d)\n",
		       prInfoElem->ucLength);
		return FALSE;
	}

	/* Check RSN version: currently, we only support version 1. */
	WLAN_GET_FIELD_16(&prInfoElem->u2Version, &u2Version);
	if (u2Version != 1) {
		DBGLOG(RSN, TRACE, "Unsupported RSN IE version: %d\n",
		       u2Version);
		return FALSE;
	}

	cp = (uint8_t *) &prInfoElem->u4GroupKeyCipherSuite;
	u4RemainRsnIeLen = (int32_t) prInfoElem->ucLength - 2;

	do {
		if (u4RemainRsnIeLen == 0)
			break;

		/* Parse the Group Key Cipher Suite field. */
		if (u4RemainRsnIeLen < 4) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse RSN IE in group cipher suite (IE len: %d)\n",
			       prInfoElem->ucLength);
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
		i = (uint32_t) u2PairSuiteCount * 4;
		if (u4RemainRsnIeLen < (int32_t) i) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse RSN IE in pairwise cipher suite list (IE len: %d)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}

		pucPairSuite = cp;

		cp += i;
		u4RemainRsnIeLen -= (int32_t) i;

		if (u4RemainRsnIeLen == 0)
			break;

		/* Parse the Authentication and Key Management Cipher
		 * Suite Count field.
		 */
		if (u4RemainRsnIeLen < 2) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse RSN IE in auth & key mgt suite count (IE len: %d)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}

		WLAN_GET_FIELD_16(cp, &u2AuthSuiteCount);
		cp += 2;
		u4RemainRsnIeLen -= 2;

		/* Parse the Authentication and Key Management Cipher
		 * Suite List field.
		 */
		i = (uint32_t) u2AuthSuiteCount * 4;
		if (u4RemainRsnIeLen < (int32_t) i) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse RSN IE in auth & key mgt suite list (IE len: %d)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}

		pucAuthSuite = cp;

		cp += i;
		u4RemainRsnIeLen -= (int32_t) i;

		if (u4RemainRsnIeLen == 0)
			break;

		/* Parse the RSN u2Capabilities field. */
		if (u4RemainRsnIeLen < 2) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse RSN IE in RSN capabilities (IE len: %d)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}

		WLAN_GET_FIELD_16(cp, &u2Cap);
		cp += 2;
		u4RemainRsnIeLen -= 2;

		if (u4RemainRsnIeLen == 0)
			break;
#if CFG_SUPPORT_CFG80211_AUTH
		/* 9.4.2.25.5 PMKID
		* The PMKID Count and List fields are used only in
		* the RSNE in the (Re)Association Request frame to an AP
		* and in FT authentication sequence frames.
		*/
		/* Parse PMKID count field */
		if (u4RemainRsnIeLen < 2) {
			DBGLOG(RSN, TRACE,
				"Fail to parse RSN IE in PMKID (IE len: %d)\n",
				prInfoElem->ucLength);
			return FALSE;
		}

		WLAN_GET_FIELD_16(cp, &u2DesiredPmkidCnt);
		cp += 2;
		u4RemainRsnIeLen -= 2;

		if (u2DesiredPmkidCnt > MAX_NUM_SUPPORTED_PMKID) {
			u2SupportedPmkidCnt = MAX_NUM_SUPPORTED_PMKID;
			DBGLOG(RSN, WARN,
				"Support maximum PMKID Cnt = %d with desired PMKID Cnt = %d\n",
				MAX_NUM_SUPPORTED_PMKID, u2DesiredPmkidCnt);
		} else
			u2SupportedPmkidCnt = u2DesiredPmkidCnt;

		/* Parse PMKID List field */
		i = (uint32_t) u2DesiredPmkidCnt * RSN_PMKID_LEN;
		if (u4RemainRsnIeLen < (int32_t) i) {
			DBGLOG(RSN, TRACE,
				"Fail to parse RSN IE in pairwise cipher suite list (IE len: %d)\n",
				prInfoElem->ucLength);
			return FALSE;
		}

		if (u2SupportedPmkidCnt > 0) {
			kalMemCopy(prRsnInfo->aucPmkidList, cp,
				(u2SupportedPmkidCnt * RSN_PMKID_LEN));

			DBGLOG(RSN, INFO, "== Dump cached PMKIDs ==\n");
			DBGLOG_MEM8(RSN, INFO, prRsnInfo->aucPmkidList,
				(u2SupportedPmkidCnt * RSN_PMKID_LEN));
		}
		cp += u2DesiredPmkidCnt * RSN_PMKID_LEN;
		u4RemainRsnIeLen -=
			(int32_t)(u2DesiredPmkidCnt * RSN_PMKID_LEN);

		if (u4RemainRsnIeLen == 0)
			break;
#endif

	/* Parse Group Mgmt Cipher Suite field */
	if (u4RemainRsnIeLen < 4) {
		DBGLOG(RSN, TRACE,
		"Fail to parse RSN IE in GroupMgmtCipher (IELen: %d)\n",
		prInfoElem->ucLength);
		/* Continued to connect
		 * when PMKID List field is truncated.
		 */
		break;
	}
	WLAN_GET_FIELD_32(cp, &u4GroupMgmtSuite);
	} while (FALSE);

	/* Save the RSN information for the BSS. */
	prRsnInfo->ucElemId = ELEM_ID_RSN;

	prRsnInfo->ucRsneLen = prInfoElem->ucLength;

	prRsnInfo->u2Version = u2Version;

	prRsnInfo->u4GroupKeyCipherSuite = u4GroupSuite;

	prRsnInfo->u4GroupMgmtKeyCipherSuite = u4GroupMgmtSuite;

#if CFG_SUPPORT_CFG80211_AUTH
	prRsnInfo->u2PmkidCnt = u2SupportedPmkidCnt;
#endif

	DBGLOG(RSN, LOUD,
		"RSN: version %d, group mgmt key cipher suite %02x-%02x-%02x-%02x\n",
			u2Version, (uint8_t) (u4GroupMgmtSuite & 0x000000FF),
			(uint8_t) ((u4GroupMgmtSuite >> 8) & 0x000000FF),
			(uint8_t) ((u4GroupMgmtSuite >> 16) & 0x000000FF),
			(uint8_t) ((u4GroupMgmtSuite >> 24) & 0x000000FF));

	DBGLOG(RSN, LOUD,
	       "RSN: version %d, group key cipher suite %02x-%02x-%02x-%02x\n",
	       u2Version, (uint8_t) (u4GroupSuite & 0x000000FF),
	       (uint8_t) ((u4GroupSuite >> 8) & 0x000000FF),
	       (uint8_t) ((u4GroupSuite >> 16) & 0x000000FF),
	       (uint8_t) ((u4GroupSuite >> 24) & 0x000000FF));

	if (pucPairSuite) {
		/* The information about the pairwise key cipher suites
		 * is present.
		 */
		if (u2PairSuiteCount > MAX_NUM_SUPPORTED_CIPHER_SUITES)
			u2PairSuiteCount = MAX_NUM_SUPPORTED_CIPHER_SUITES;

		prRsnInfo->u4PairwiseKeyCipherSuiteCount =
		    (uint32_t) u2PairSuiteCount;

		for (i = 0; i < (uint32_t) u2PairSuiteCount; i++) {
			WLAN_GET_FIELD_32(pucPairSuite,
					  &prRsnInfo->au4PairwiseKeyCipherSuite
					  [i]);
			pucPairSuite += 4;

			DBGLOG(RSN, LOUD,
			       "RSN: pairwise key cipher suite [%d]: %02x-%02x-%02x-%02x\n",
			       (uint8_t) i,
			       (uint8_t) (prRsnInfo->au4PairwiseKeyCipherSuite
					  [i]
					  & 0x000000FF),
			       (uint8_t) ((prRsnInfo->au4PairwiseKeyCipherSuite
					   [i] >> 8)
					  & 0x000000FF),
			       (uint8_t) ((prRsnInfo->au4PairwiseKeyCipherSuite
					   [i] >> 16)
					  & 0x000000FF),
			       (uint8_t) ((prRsnInfo->au4PairwiseKeyCipherSuite
					   [i] >> 24)
					  & 0x000000FF));
		}
	} else {
		/* The information about the pairwise key cipher suites
		 * is not present. Use the default chipher suite for RSN: CCMP.
		 */
		prRsnInfo->u4PairwiseKeyCipherSuiteCount = 1;
		prRsnInfo->au4PairwiseKeyCipherSuite[0] = RSN_CIPHER_SUITE_CCMP;

		DBGLOG(RSN, LOUD,
		       "RSN: pairwise key cipher suite: %02x-%02x-%02x-%02x (default)\n",
		       (uint8_t) (prRsnInfo->au4PairwiseKeyCipherSuite[0]
				  & 0x000000FF),
		       (uint8_t) ((prRsnInfo->au4PairwiseKeyCipherSuite[0] >> 8)
				  & 0x000000FF),
		       (uint8_t) ((prRsnInfo->au4PairwiseKeyCipherSuite[0] >>
				   16)
				  & 0x000000FF),
		       (uint8_t) ((prRsnInfo->au4PairwiseKeyCipherSuite[0] >>
				   24)
				  & 0x000000FF));
	}

	if (pucAuthSuite) {
		/* The information about the authentication and
		 * key management suites is present.
		 */
		if (u2AuthSuiteCount > MAX_NUM_SUPPORTED_AKM_SUITES)
			u2AuthSuiteCount = MAX_NUM_SUPPORTED_AKM_SUITES;

		prRsnInfo->u4AuthKeyMgtSuiteCount = (uint32_t)
		    u2AuthSuiteCount;

		for (i = 0; i < (uint32_t) u2AuthSuiteCount; i++) {
			WLAN_GET_FIELD_32(pucAuthSuite,
					  &prRsnInfo->au4AuthKeyMgtSuite[i]);
			pucAuthSuite += 4;

			DBGLOG(RSN, LOUD,
			       "RSN: AKM suite [%d]: %02x-%02x-%02x-%02x\n",
			       (uint8_t) i,
			       (uint8_t) (prRsnInfo->au4AuthKeyMgtSuite[i]
					  & 0x000000FF),
			       (uint8_t) ((prRsnInfo->au4AuthKeyMgtSuite[i] >>
					   8)
					  & 0x000000FF),
			       (uint8_t) ((prRsnInfo->au4AuthKeyMgtSuite[i] >>
					   16)
					  & 0x000000FF),
			       (uint8_t) ((prRsnInfo->au4AuthKeyMgtSuite[i] >>
					   24)
					  & 0x000000FF));
		}
	} else {
		/* The information about the authentication and
		 * key management suites is not present.
		 * Use the default AKM suite for RSN.
		 */
		prRsnInfo->u4AuthKeyMgtSuiteCount = 1;
		prRsnInfo->au4AuthKeyMgtSuite[0] = RSN_AKM_SUITE_802_1X;

		DBGLOG(RSN, LOUD,
		       "RSN: AKM suite: %02x-%02x-%02x-%02x (default)\n",
		       (uint8_t) (prRsnInfo->au4AuthKeyMgtSuite[0] &
				  0x000000FF),
		       (uint8_t) ((prRsnInfo->au4AuthKeyMgtSuite[0] >> 8)
				  & 0x000000FF),
		       (uint8_t) ((prRsnInfo->au4AuthKeyMgtSuite[0] >> 16)
				  & 0x000000FF),
		       (uint8_t) ((prRsnInfo->au4AuthKeyMgtSuite[0] >> 24)
				  & 0x000000FF));
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
u_int8_t rsnParseWpaIE(IN struct ADAPTER *prAdapter,
		       IN struct WPA_INFO_ELEM *prInfoElem,
		       OUT struct RSN_INFO *prWpaInfo)
{
	uint32_t i;
	int32_t u4RemainWpaIeLen;
	uint16_t u2Version;
	uint16_t u2Cap = 0;
	uint32_t u4GroupSuite = WPA_CIPHER_SUITE_TKIP;
	uint16_t u2PairSuiteCount = 0;
	uint16_t u2AuthSuiteCount = 0;
	uint8_t *pucPairSuite = NULL;
	uint8_t *pucAuthSuite = NULL;
	uint8_t *cp;
	u_int8_t fgCapPresent = FALSE;

	DEBUGFUNC("rsnParseWpaIE");

	ASSERT(prInfoElem);
	ASSERT(prWpaInfo);

	/* Verify the length of the WPA IE. */
	if (prInfoElem->ucLength < 6) {
		DBGLOG(RSN, TRACE, "WPA IE length too short (length=%d)\n",
		       prInfoElem->ucLength);
		return FALSE;
	}

	/* Check WPA version: currently, we only support version 1. */
	WLAN_GET_FIELD_16(&prInfoElem->u2Version, &u2Version);
	if (u2Version != 1) {
		DBGLOG(RSN, TRACE, "Unsupported WPA IE version: %d\n",
		       u2Version);
		return FALSE;
	}

	cp = (uint8_t *) &prInfoElem->u4GroupKeyCipherSuite;
	u4RemainWpaIeLen = (int32_t) prInfoElem->ucLength - 6;

	do {
		if (u4RemainWpaIeLen == 0)
			break;

		/* WPA_OUI      : 4
		 *  Version      : 2
		 *  GroupSuite   : 4
		 *  PairwiseCount: 2
		 *  PairwiseSuite: 4 * pairSuiteCount
		 *  AuthCount    : 2
		 *  AuthSuite    : 4 * authSuiteCount
		 *  Cap          : 2
		 */

		/* Parse the Group Key Cipher Suite field. */
		if (u4RemainWpaIeLen < 4) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse WPA IE in group cipher suite (IE len: %d)\n",
			       prInfoElem->ucLength);
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
		i = (uint32_t) u2PairSuiteCount * 4;
		if (u4RemainWpaIeLen < (int32_t) i) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse WPA IE in pairwise cipher suite list (IE len: %d)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}

		pucPairSuite = cp;

		cp += i;
		u4RemainWpaIeLen -= (int32_t) i;

		if (u4RemainWpaIeLen == 0)
			break;

		/* Parse the Authentication and Key Management Cipher Suite
		 * Count field.
		 */
		if (u4RemainWpaIeLen < 2) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse WPA IE in auth & key mgt suite count (IE len: %d)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}

		WLAN_GET_FIELD_16(cp, &u2AuthSuiteCount);
		cp += 2;
		u4RemainWpaIeLen -= 2;

		/* Parse the Authentication and Key Management Cipher Suite
		 * List field.
		 */
		i = (uint32_t) u2AuthSuiteCount * 4;
		if (u4RemainWpaIeLen < (int32_t) i) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse WPA IE in auth & key mgt suite list (IE len: %d)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}

		pucAuthSuite = cp;

		cp += i;
		u4RemainWpaIeLen -= (int32_t) i;

		if (u4RemainWpaIeLen == 0)
			break;

		/* Parse the WPA u2Capabilities field. */
		if (u4RemainWpaIeLen < 2) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse WPA IE in WPA capabilities (IE len: %d)\n",
			       prInfoElem->ucLength);
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

	DBGLOG(RSN, LOUD,
	       "WPA: version %d, group key cipher suite %02x-%02x-%02x-%02x\n",
	       u2Version, (uint8_t) (u4GroupSuite & 0x000000FF),
	       (uint8_t) ((u4GroupSuite >> 8) & 0x000000FF),
	       (uint8_t) ((u4GroupSuite >> 16) & 0x000000FF),
	       (uint8_t) ((u4GroupSuite >> 24) & 0x000000FF));

	if (pucPairSuite) {
		/* The information about the pairwise key cipher suites
		 * is present.
		 */
		if (u2PairSuiteCount > MAX_NUM_SUPPORTED_CIPHER_SUITES)
			u2PairSuiteCount = MAX_NUM_SUPPORTED_CIPHER_SUITES;

		prWpaInfo->u4PairwiseKeyCipherSuiteCount =
		    (uint32_t) u2PairSuiteCount;

		for (i = 0; i < (uint32_t) u2PairSuiteCount; i++) {
			WLAN_GET_FIELD_32(pucPairSuite,
					  &prWpaInfo->au4PairwiseKeyCipherSuite
					  [i]);
			pucPairSuite += 4;

			DBGLOG(RSN, LOUD,
			       "WPA: pairwise key cipher suite [%d]: %02x-%02x-%02x-%02x\n",
			       (uint8_t) i,
			       (uint8_t) (prWpaInfo->au4PairwiseKeyCipherSuite
					  [i]
					  & 0x000000FF),
			       (uint8_t) ((prWpaInfo->au4PairwiseKeyCipherSuite
					   [i] >> 8)
					  & 0x000000FF),
			       (uint8_t) ((prWpaInfo->au4PairwiseKeyCipherSuite
					   [i] >> 16)
					  & 0x000000FF),
			       (uint8_t) ((prWpaInfo->au4PairwiseKeyCipherSuite
					   [i] >> 24)
					  & 0x000000FF));
		}
	} else {
		/* The information about the pairwise key cipher suites
		 * is not present.
		 * Use the default chipher suite for WPA: TKIP.
		 */
		prWpaInfo->u4PairwiseKeyCipherSuiteCount = 1;
		prWpaInfo->au4PairwiseKeyCipherSuite[0] = WPA_CIPHER_SUITE_TKIP;

		DBGLOG(RSN, LOUD,
		       "WPA: pairwise key cipher suite: %02x-%02x-%02x-%02x (default)\n",
		       (uint8_t) (prWpaInfo->au4PairwiseKeyCipherSuite[0]
				  & 0x000000FF),
		       (uint8_t) ((prWpaInfo->au4PairwiseKeyCipherSuite[0] >> 8)
				  & 0x000000FF),
		       (uint8_t) ((prWpaInfo->au4PairwiseKeyCipherSuite[0] >>
				   16)
				  & 0x000000FF),
		       (uint8_t) ((prWpaInfo->au4PairwiseKeyCipherSuite[0] >>
				   24)
				  & 0x000000FF));
	}

	if (pucAuthSuite) {
		/* The information about the authentication and
		 * key management suites is present.
		 */
		if (u2AuthSuiteCount > MAX_NUM_SUPPORTED_AKM_SUITES)
			u2AuthSuiteCount = MAX_NUM_SUPPORTED_AKM_SUITES;

		prWpaInfo->u4AuthKeyMgtSuiteCount = (uint32_t)
		    u2AuthSuiteCount;

		for (i = 0; i < (uint32_t) u2AuthSuiteCount; i++) {
			WLAN_GET_FIELD_32(pucAuthSuite,
					  &prWpaInfo->au4AuthKeyMgtSuite[i]);
			pucAuthSuite += 4;

			DBGLOG(RSN, LOUD,
			       "WPA: AKM suite [%d]: %02x-%02x-%02x-%02x\n",
			       (uint8_t) i,
			       (uint8_t)(prWpaInfo->au4AuthKeyMgtSuite[i]
					& 0x000000FF),
			       (uint8_t)((prWpaInfo->au4AuthKeyMgtSuite[i]>>8)
					& 0x000000FF),
			       (uint8_t)((prWpaInfo->au4AuthKeyMgtSuite[i]>>16)
					& 0x000000FF),
			       (uint8_t)((prWpaInfo->au4AuthKeyMgtSuite[i]>>24)
					& 0x000000FF));
		}
	} else {
		/* The information about the authentication
		 * and key management suites is not present.
		 * Use the default AKM suite for WPA.
		 */
		prWpaInfo->u4AuthKeyMgtSuiteCount = 1;
		prWpaInfo->au4AuthKeyMgtSuite[0] = WPA_AKM_SUITE_802_1X;

		DBGLOG(RSN, LOUD,
		       "WPA: AKM suite: %02x-%02x-%02x-%02x (default)\n",
		       (uint8_t)(prWpaInfo->au4AuthKeyMgtSuite[0]
				& 0x000000FF),
		       (uint8_t)((prWpaInfo->au4AuthKeyMgtSuite[0]>>8)
				& 0x000000FF),
		       (uint8_t)((prWpaInfo->au4AuthKeyMgtSuite[0]>>16)
				& 0x000000FF),
		       (uint8_t)((prWpaInfo->au4AuthKeyMgtSuite[0]>>24)
				& 0x000000FF));
	}

	if (fgCapPresent) {
		prWpaInfo->fgRsnCapPresent = TRUE;
		prWpaInfo->u2RsnCap = u2Cap;
		DBGLOG(RSN, LOUD, "WPA: RSN cap: 0x%04x\n",
		       prWpaInfo->u2RsnCap);
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
u_int8_t rsnSearchSupportedCipher(IN struct ADAPTER *prAdapter,
				  IN uint32_t u4Cipher, OUT uint32_t *pu4Index)
{
	uint8_t i;
	struct DOT11_RSNA_CONFIG_PAIRWISE_CIPHERS_ENTRY *prEntry;

	DEBUGFUNC("rsnSearchSupportedCipher");

	ASSERT(pu4Index);

	for (i = 0; i < MAX_NUM_SUPPORTED_CIPHER_SUITES; i++) {
		prEntry =
		    &prAdapter->rMib.dot11RSNAConfigPairwiseCiphersTable[i];
		if (prEntry->dot11RSNAConfigPairwiseCipher == u4Cipher &&
		    prEntry->dot11RSNAConfigPairwiseCipherEnabled) {
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
u_int8_t rsnIsSuitableBSS(IN struct ADAPTER *prAdapter,
			  IN struct RSN_INFO *prBssRsnInfo)
{
	uint8_t i = 0;

	DEBUGFUNC("rsnIsSuitableBSS");

	if ((prAdapter->rWifiVar.rConnSettings.rRsnInfo.u4GroupKeyCipherSuite
		& 0x000000FF) !=
		GET_SELECTOR_TYPE(prBssRsnInfo->u4GroupKeyCipherSuite)) {
		DBGLOG(RSN, WARN, "Break by GroupKeyCipherSuite\n");
		return FALSE;
	}

	for (i = 0; i < prBssRsnInfo->u4PairwiseKeyCipherSuiteCount;
		i++) {
		if ((prAdapter->rWifiVar.rConnSettings.
			rRsnInfo.au4PairwiseKeyCipherSuite[0]
			& 0x000000FF) ==
			GET_SELECTOR_TYPE(
			prBssRsnInfo->au4PairwiseKeyCipherSuite[i])) {
			break;
		} else if (i ==
			prBssRsnInfo->u4PairwiseKeyCipherSuiteCount - 1) {
			DBGLOG(RSN, WARN, "Break by PairwiseKeyCipherSuite\n");
			return FALSE;
		}
	}

	for (i = 0; i < prBssRsnInfo->u4AuthKeyMgtSuiteCount; i++) {
		if ((prAdapter->rWifiVar.rConnSettings.
			rRsnInfo.au4AuthKeyMgtSuite[0]
			& 0x000000FF) ==
			GET_SELECTOR_TYPE(
			prBssRsnInfo->au4AuthKeyMgtSuite[i])) {
			break;
		} else if (i == prBssRsnInfo->u4AuthKeyMgtSuiteCount - 1) {
			DBGLOG(RSN, WARN, "Break by AuthKeyMgtSuite\n");
			return FALSE;
		}
	}

	return TRUE;
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
u_int8_t rsnSearchAKMSuite(IN struct ADAPTER *prAdapter,
			   IN uint32_t u4AkmSuite, OUT uint32_t *pu4Index)
{
	uint8_t i;
	struct DOT11_RSNA_CONFIG_AUTHENTICATION_SUITES_ENTRY
	*prEntry;

	DEBUGFUNC("rsnSearchAKMSuite");

	ASSERT(pu4Index);

	for (i = 0; i < MAX_NUM_SUPPORTED_AKM_SUITES; i++) {
		prEntry = &prAdapter->rMib.
				dot11RSNAConfigAuthenticationSuitesTable[i];
		if (prEntry->dot11RSNAConfigAuthenticationSuite ==
		    u4AkmSuite &&
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
u_int8_t rsnPerformPolicySelection(
		IN struct ADAPTER *prAdapter, IN struct BSS_DESC *prBss)
{
#if CFG_SUPPORT_802_11W
	int32_t i;
	uint32_t j;
#else
	uint32_t i, j;
#endif
	u_int8_t fgSuiteSupported;
	uint32_t u4PairwiseCipher = 0;
	uint32_t u4GroupCipher = 0;
	uint32_t u4AkmSuite = 0;
	struct RSN_INFO *prBssRsnInfo;
	uint8_t ucBssIndex;
	u_int8_t fgIsWpsActive = (u_int8_t) FALSE;

	DEBUGFUNC("rsnPerformPolicySelection");

	ASSERT(prBss);

	DBGLOG(RSN, TRACE, "rsnPerformPolicySelection\n");
	/* Todo:: */
	ucBssIndex = prAdapter->prAisBssInfo->ucBssIndex;

	prBss->u4RsnSelectedPairwiseCipher = 0;
	prBss->u4RsnSelectedGroupCipher = 0;
	prBss->u4RsnSelectedAKMSuite = 0;
	prBss->ucEncLevel = 0;

	prAdapter->rWifiVar.rAisSpecificBssInfo.fgMgmtProtection = FALSE;

#if CFG_SUPPORT_WPS
	fgIsWpsActive = kalWSCGetActiveState(prAdapter->prGlueInfo);

	/* CR1640, disable the AP select privacy check */
	if (fgIsWpsActive &&
	    (prAdapter->rWifiVar.rConnSettings.eAuthMode <
	     AUTH_MODE_WPA) &&
	    (prAdapter->rWifiVar.rConnSettings.eOPMode == NET_TYPE_INFRA)) {
		DBGLOG(RSN, INFO, "-- Skip the Protected BSS check\n");
		return TRUE;
	}
#endif

	/* Protection is not required in this BSS. */
	if ((prBss->u2CapInfo & CAP_INFO_PRIVACY) == 0) {

		if (secEnabledInAis(prAdapter) == FALSE) {
			DBGLOG(RSN, INFO, "-- No Protected BSS\n");
			return TRUE;
		}
		DBGLOG(RSN, INFO, "-- Protected BSS but No need\n");
		return FALSE;
	}

	/* Protection is required in this BSS. */
	if ((prBss->u2CapInfo & CAP_INFO_PRIVACY) != 0) {
		if (secEnabledInAis(prAdapter) == FALSE) {
			DBGLOG(RSN, INFO, "-- Protected BSS\n");
			return FALSE;
		}
	}

	if (prAdapter->rWifiVar.rConnSettings.eAuthMode ==
	    AUTH_MODE_WPA ||
	    prAdapter->rWifiVar.rConnSettings.eAuthMode ==
	    AUTH_MODE_WPA_PSK ||
	    prAdapter->rWifiVar.rConnSettings.eAuthMode == AUTH_MODE_WPA_NONE) {

		if (prBss->fgIEWPA) {
			prBssRsnInfo = &prBss->rWPAInfo;
		} else {
			DBGLOG(RSN, INFO,
			       "WPA Information Element does not exist.\n");
			return FALSE;
		}
	} else if (prAdapter->rWifiVar.rConnSettings.eAuthMode ==
		   AUTH_MODE_WPA2 ||
		   prAdapter->rWifiVar.rConnSettings.eAuthMode ==
		   AUTH_MODE_WPA2_PSK ||
		   prAdapter->rWifiVar.rConnSettings.eAuthMode ==
		   AUTH_MODE_WPA2_FT_PSK ||
		   prAdapter->rWifiVar.rConnSettings.eAuthMode ==
		   AUTH_MODE_WPA2_FT
#if CFG_SUPPORT_SAE
		|| prAdapter->rWifiVar.rConnSettings.eAuthMode
		== AUTH_MODE_WPA2_SAE
#endif
	) {
		if (prBss->fgIERSN) {
			prBssRsnInfo = &prBss->rRSNInfo;
		} else {
			DBGLOG(RSN, INFO,
			       "RSN Information Element does not exist.\n");
			return FALSE;
		}
#if CFG_SUPPORT_PASSPOINT
	} else if (prAdapter->rWifiVar.rConnSettings.eAuthMode ==
		   AUTH_MODE_WPA_OSEN) {
		/* OSEN is mutual exclusion with RSN,
		 * so we can reuse RSN's flag and variables
		 */
		if (prBss->fgIEOsen) {
			prBssRsnInfo = &prBss->rRSNInfo;
		} else {
			DBGLOG(RSN, WARN,
			       "OSEN Information Element does not exist.\n");
			return FALSE;
		}
#endif
	} else if (prAdapter->rWifiVar.rConnSettings.eEncStatus !=
		   ENUM_ENCRYPTION1_ENABLED) {
		/* If the driver is configured to use WEP only,
		 * ignore this BSS.
		 */
		DBGLOG(RSN, INFO, "-- Not WEP-only legacy BSS\n");
		return FALSE;
	} else if (prAdapter->rWifiVar.rConnSettings.eEncStatus ==
		   ENUM_ENCRYPTION1_ENABLED) {
		/* If the driver is configured to use WEP only, use this BSS. */
		DBGLOG(RSN, INFO, "-- WEP-only legacy BSS\n");
		return TRUE;
	}

	if (!rsnIsSuitableBSS(prAdapter, prBssRsnInfo)) {
		/* Support AP Selection */
		DBGLOG(RSN, WARN, "RSN info check no matched,RSN Score [%d]\n",
		       CFG_SUPPORT_RSN_SCORE);
#if CFG_SUPPORT_RSN_SCORE
		prBss->fgIsRSNSuitableBss = FALSE;
	} else
		prBss->fgIsRSNSuitableBss = TRUE;
#else

		return FALSE;
	}
#endif
	/* end Support AP Selection */

	if (prBssRsnInfo->u4PairwiseKeyCipherSuiteCount == 1 &&
	    GET_SELECTOR_TYPE(prBssRsnInfo->au4PairwiseKeyCipherSuite[0]) ==
	    CIPHER_SUITE_NONE) {
		/* Since the pairwise cipher use the same cipher suite
		 * as the group cipher in the BSS, we check the group cipher
		 * suite against the current encryption status.
		 */
		fgSuiteSupported = FALSE;

		switch (prBssRsnInfo->u4GroupKeyCipherSuite) {
#if CFG_SUPPORT_SUITB
		case RSN_CIPHER_SUITE_GCMP_256:
			if (prAdapter->rWifiVar.rConnSettings.eEncStatus ==
				ENUM_ENCRYPTION4_ENABLED)
				fgSuiteSupported = TRUE;
			break;
#endif
		case WPA_CIPHER_SUITE_CCMP:
		case RSN_CIPHER_SUITE_CCMP:
			if (prAdapter->rWifiVar.rConnSettings.eEncStatus ==
			    ENUM_ENCRYPTION3_ENABLED)
				fgSuiteSupported = TRUE;
			break;

		case WPA_CIPHER_SUITE_TKIP:
		case RSN_CIPHER_SUITE_TKIP:
			if (prAdapter->rWifiVar.rConnSettings.eEncStatus ==
			    ENUM_ENCRYPTION2_ENABLED)
				fgSuiteSupported = TRUE;
			break;

		case WPA_CIPHER_SUITE_WEP40:
		case WPA_CIPHER_SUITE_WEP104:
			if (prAdapter->rWifiVar.rConnSettings.eEncStatus ==
			    ENUM_ENCRYPTION1_ENABLED)
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
		       "eEncStatus %d %d 0x%08x\n",
		       prAdapter->rWifiVar.rConnSettings.eEncStatus,
		       prBssRsnInfo->u4PairwiseKeyCipherSuiteCount,
		       prBssRsnInfo->au4PairwiseKeyCipherSuite[0]);
		/* Select pairwise/group ciphers */
		switch (prAdapter->rWifiVar.rConnSettings.eEncStatus) {
#if CFG_SUPPORT_SUITB
		case ENUM_ENCRYPTION4_ENABLED:
			for (i = 0;
				i < prBssRsnInfo->u4PairwiseKeyCipherSuiteCount;
				i++) {
				/* TODO: WTBL cipher filed cannot 1-1 mapping
				 * to spec cipher suite number
				 */
				if (prBssRsnInfo->au4PairwiseKeyCipherSuite[i]
					== RSN_CIPHER_SUITE_GCMP_256)
					u4PairwiseCipher = prBssRsnInfo
						->au4PairwiseKeyCipherSuite[i];
			}
			u4GroupCipher = prBssRsnInfo->u4GroupKeyCipherSuite;
			break;
#endif
		case ENUM_ENCRYPTION3_ENABLED:
			for (i = 0; i < prBssRsnInfo->
				u4PairwiseKeyCipherSuiteCount; i++) {
				if (GET_SELECTOR_TYPE(
					prBssRsnInfo->
						au4PairwiseKeyCipherSuite[i])
					== CIPHER_SUITE_CCMP) {
					u4PairwiseCipher =
						prBssRsnInfo->
						au4PairwiseKeyCipherSuite[i];
				}
			}
			u4GroupCipher = prBssRsnInfo->u4GroupKeyCipherSuite;
			break;

		case ENUM_ENCRYPTION2_ENABLED:
			for (i = 0;
			     i < prBssRsnInfo->u4PairwiseKeyCipherSuiteCount;
			     i++) {
				if (GET_SELECTOR_TYPE
				    (prBssRsnInfo->au4PairwiseKeyCipherSuite[i])
				    == CIPHER_SUITE_TKIP) {
					u4PairwiseCipher =
					    prBssRsnInfo->
					    au4PairwiseKeyCipherSuite[i];
				}
			}
			if (GET_SELECTOR_TYPE
			    (prBssRsnInfo->u4GroupKeyCipherSuite)
			    == CIPHER_SUITE_CCMP)
				DBGLOG(RSN, TRACE, "Cannot join CCMP BSS\n");
			else
				u4GroupCipher =
				    prBssRsnInfo->u4GroupKeyCipherSuite;
			break;

		case ENUM_ENCRYPTION1_ENABLED:
			for (i = 0;
				i < prBssRsnInfo->
					u4PairwiseKeyCipherSuiteCount;
				i++) {
				if (GET_SELECTOR_TYPE(
					    prBssRsnInfo->
						au4PairwiseKeyCipherSuite[i])
					== CIPHER_SUITE_WEP40 ||
				    GET_SELECTOR_TYPE(
					    prBssRsnInfo->
						au4PairwiseKeyCipherSuite[i])
					== CIPHER_SUITE_WEP104) {
					u4PairwiseCipher = prBssRsnInfo->
						au4PairwiseKeyCipherSuite[i];
				}
			}
			if (GET_SELECTOR_TYPE(prBssRsnInfo->
				u4GroupKeyCipherSuite)
			    == CIPHER_SUITE_CCMP ||
			    GET_SELECTOR_TYPE(prBssRsnInfo->
				u4GroupKeyCipherSuite) == CIPHER_SUITE_TKIP) {
				DBGLOG(RSN, TRACE,
					"Cannot join CCMP/TKIP BSS\n");
			} else {
				u4GroupCipher =
					prBssRsnInfo->u4GroupKeyCipherSuite;
			}
			break;

		default:
			break;
		}
	}

	/* Exception handler */
	/* If we cannot find proper pairwise and group cipher suites
	 * to join the BSS, do not check the supported AKM suites.
	 */
	if (u4PairwiseCipher == 0 || u4GroupCipher == 0) {
		DBGLOG(RSN, TRACE,
		       "Failed to select pairwise/group cipher (0x%08x/0x%08x)\n",
		       u4PairwiseCipher, u4GroupCipher);
		return FALSE;
	}
#if CFG_ENABLE_WIFI_DIRECT
	if ((prAdapter->fgIsP2PRegistered) &&
	    (GET_BSS_INFO_BY_INDEX(prAdapter,
		ucBssIndex)->eNetworkType == NETWORK_TYPE_P2P)) {
		if (u4PairwiseCipher != RSN_CIPHER_SUITE_CCMP ||
		    u4GroupCipher != RSN_CIPHER_SUITE_CCMP
		    || u4AkmSuite != RSN_AKM_SUITE_PSK) {
			DBGLOG(RSN, TRACE,
			       "Failed to select pairwise/group cipher for P2P network (0x%08x/0x%08x)\n",
			       u4PairwiseCipher, u4GroupCipher);
			return FALSE;
		}
	}
#endif

#if CFG_ENABLE_BT_OVER_WIFI
	if (GET_BSS_INFO_BY_INDEX(prAdapter,
		ucBssIndex)->eNetworkType == NETWORK_TYPE_BOW) {
		if (u4PairwiseCipher != RSN_CIPHER_SUITE_CCMP ||
		    u4GroupCipher != RSN_CIPHER_SUITE_CCMP
		    || u4AkmSuite != RSN_AKM_SUITE_PSK) {
			DBGLOG(RSN, TRACE,
			       "Failed to select pairwise/group cipher for BT over Wi-Fi network (0x%08x/0x%08x)\n",
			       u4PairwiseCipher, u4GroupCipher);
			return FALSE;
		}
	}
#endif

	/* Verify if selected pairwisse cipher is supported */
	fgSuiteSupported = rsnSearchSupportedCipher(prAdapter,
						    u4PairwiseCipher, &i);

	/* Verify if selected group cipher is supported */
	if (fgSuiteSupported)
		fgSuiteSupported = rsnSearchSupportedCipher(prAdapter,
							    u4GroupCipher, &i);

	if (!fgSuiteSupported) {
		DBGLOG(RSN, TRACE,
		       "Failed to support selected pairwise/group cipher (0x%08x/0x%08x)\n",
		       u4PairwiseCipher, u4GroupCipher);
		return FALSE;
	}

	/* Select AKM */
	/* If the driver cannot support any authentication suites advertised in
	 *  the given BSS, we fail to perform RSNA policy selection.
	 */
	/* Attempt to find any overlapping supported AKM suite. */
	if (prAdapter->rWifiVar.rConnSettings.eAuthMode ==
		    AUTH_MODE_WPA2_FT_PSK &&
			rsnSearchAKMSuite(prAdapter, RSN_AKM_SUITE_FT_PSK, &j))
		u4AkmSuite = RSN_AKM_SUITE_FT_PSK;
	else if (prAdapter->rWifiVar.rConnSettings.eAuthMode ==
			AUTH_MODE_WPA2_FT &&
			rsnSearchAKMSuite(prAdapter,
				RSN_AKM_SUITE_FT_802_1X, &j))
		u4AkmSuite = RSN_AKM_SUITE_FT_802_1X;
	else
#if CFG_SUPPORT_802_11W
	if (i != 0)
		for (i = (prBssRsnInfo->u4AuthKeyMgtSuiteCount - 1); i >= 0;
		     i--) {
#else
		for (i = 0; i < prBssRsnInfo->u4AuthKeyMgtSuiteCount; i++) {
#endif
			if (rsnSearchAKMSuite(prAdapter,
				prBssRsnInfo->au4AuthKeyMgtSuite[i], &j)) {
				u4AkmSuite =
					prBssRsnInfo->au4AuthKeyMgtSuite[i];
				break;
			}
		}

	if (u4AkmSuite == 0) {
		DBGLOG(RSN, TRACE, "Cannot support any AKM suites\n");
		return FALSE;
	}

	DBGLOG(RSN, TRACE,
	       "Selected pairwise/group cipher: %02x-%02x-%02x-%02x/%02x-%02x-%02x-%02x\n",
	       (uint8_t) (u4PairwiseCipher & 0x000000FF),
	       (uint8_t) ((u4PairwiseCipher >> 8) & 0x000000FF),
	       (uint8_t) ((u4PairwiseCipher >> 16) & 0x000000FF),
	       (uint8_t) ((u4PairwiseCipher >> 24) & 0x000000FF),
	       (uint8_t) (u4GroupCipher & 0x000000FF),
	       (uint8_t) ((u4GroupCipher >> 8) & 0x000000FF),
	       (uint8_t) ((u4GroupCipher >> 16) & 0x000000FF),
	       (uint8_t) ((u4GroupCipher >> 24) & 0x000000FF));

	DBGLOG(RSN, TRACE,
	       "Selected AKM suite: %02x-%02x-%02x-%02x\n",
	       (uint8_t) (u4AkmSuite & 0x000000FF),
	       (uint8_t) ((u4AkmSuite >> 8) & 0x000000FF),
	       (uint8_t) ((u4AkmSuite >> 16) & 0x000000FF),
	       (uint8_t) ((u4AkmSuite >> 24) & 0x000000FF));

#if CFG_SUPPORT_802_11W
	DBGLOG(RSN, TRACE, "[MFP] MFP setting = %d\n ",
	       kalGetMfpSetting(prAdapter->prGlueInfo));

	if (kalGetMfpSetting(prAdapter->prGlueInfo) ==
	    RSN_AUTH_MFP_REQUIRED) {
		if (!prBssRsnInfo->fgRsnCapPresent) {
			DBGLOG(RSN, TRACE,
			       "[MFP] Skip RSN IE, No MFP Required Capability.\n");
			return FALSE;
		} else if (!(prBssRsnInfo->u2RsnCap & ELEM_WPA_CAP_MFPC)) {
			DBGLOG(RSN, WARN,
			       "[MFP] Skip RSN IE, No MFP Required\n");
			return FALSE;
		}
		prAdapter->rWifiVar.rAisSpecificBssInfo.fgMgmtProtection = TRUE;
	} else if (kalGetMfpSetting(prAdapter->prGlueInfo) ==
		   RSN_AUTH_MFP_OPTIONAL) {
		if (prBssRsnInfo->u2RsnCap & (ELEM_WPA_CAP_MFPR |
					      ELEM_WPA_CAP_MFPC))
			prAdapter->rWifiVar.rAisSpecificBssInfo.
			    fgMgmtProtection = TRUE;
	} else {
		if ((prBssRsnInfo->fgRsnCapPresent) &&
		(prBssRsnInfo->u2RsnCap & ELEM_WPA_CAP_MFPR)) {
			DBGLOG(RSN, INFO,
			       "[MFP] Skip RSN IE, No MFP Required Capability\n");
			return FALSE;
		}
	}

	DBGLOG(RSN, INFO,
	       "setting=%d, Cap=%d, CapPresent=%d, MgmtProtection = %d\n",
	       kalGetMfpSetting(prAdapter->prGlueInfo),
	       prBssRsnInfo->u2RsnCap,
	       prBssRsnInfo->fgRsnCapPresent,
	       prAdapter->rWifiVar.rAisSpecificBssInfo.fgMgmtProtection);
#endif

	if (GET_SELECTOR_TYPE(u4GroupCipher) == CIPHER_SUITE_CCMP) {
		prBss->ucEncLevel = 3;
#if CFG_SUPPORT_SUITB
	} else if (u4GroupCipher == RSN_CIPHER_SUITE_GCMP_256) {
		prBss->ucEncLevel = 4;
#endif
	} else if (GET_SELECTOR_TYPE(u4GroupCipher) == CIPHER_SUITE_TKIP) {
		prBss->ucEncLevel = 2;
	} else if (GET_SELECTOR_TYPE(u4GroupCipher) ==
		   CIPHER_SUITE_WEP40 ||
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
void rsnGenerateWpaNoneIE(IN struct ADAPTER *prAdapter,
			  IN struct MSDU_INFO *prMsduInfo)
{
	uint32_t i;
	struct WPA_INFO_ELEM *prWpaIE;
	uint32_t u4Suite;
	uint16_t u2SuiteCount;
	uint8_t *cp, *cp2;
	uint8_t ucExpendedLen = 0;
	uint8_t *pucBuffer;
	uint8_t ucBssIndex;

	DEBUGFUNC("rsnGenerateWpaNoneIE");

	ASSERT(prMsduInfo);

	if (prAdapter->rWifiVar.rConnSettings.eAuthMode != AUTH_MODE_WPA_NONE)
		return;

	ucBssIndex = prMsduInfo->ucBssIndex;

	if (GET_BSS_INFO_BY_INDEX(prAdapter,
				  ucBssIndex)->eNetworkType != NETWORK_TYPE_AIS)
		return;

	pucBuffer = (uint8_t *) ((unsigned long)
				 prMsduInfo->prPacket + (unsigned long)
				 prMsduInfo->u2FrameLength);

	ASSERT(pucBuffer);

	prWpaIE = (struct WPA_INFO_ELEM *)(pucBuffer);

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
	cp = (uint8_t *) &prWpaIE->aucPairwiseKeyCipherSuite1[0];

	if (rsnSearchSupportedCipher(prAdapter, WPA_CIPHER_SUITE_CCMP, &i))
		u4Suite = WPA_CIPHER_SUITE_CCMP;
	else if (rsnSearchSupportedCipher(prAdapter, WPA_CIPHER_SUITE_TKIP, &i))
		u4Suite = WPA_CIPHER_SUITE_TKIP;
	else if (rsnSearchSupportedCipher(prAdapter,
					  WPA_CIPHER_SUITE_WEP104, &i))
		u4Suite = WPA_CIPHER_SUITE_WEP104;
	else if (rsnSearchSupportedCipher(prAdapter,
					  WPA_CIPHER_SUITE_WEP40, &i))
		u4Suite = WPA_CIPHER_SUITE_WEP40;
	else
		u4Suite = WPA_CIPHER_SUITE_TKIP;

	WLAN_SET_FIELD_32(cp, u4Suite);
	u2SuiteCount++;
	ucExpendedLen += 4;

	cp = pucBuffer + sizeof(struct WPA_INFO_ELEM);

	/* Fill the Group Key Cipher Suite field as
	 * the same in pair-wise key.
	 */
	WLAN_SET_FIELD_32(&prWpaIE->u4GroupKeyCipherSuite, u4Suite);
	ucExpendedLen += 4;

	/* Fill the Pairwise Key Cipher Suite Count field. */
	WLAN_SET_FIELD_16(&prWpaIE->u2PairwiseKeyCipherSuiteCount,
			  u2SuiteCount);
	ucExpendedLen += 2;

	cp2 = cp;

	/* Fill the Authentication and Key Management Suite
	 * List field.
	 */
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
	prWpaIE->ucLength = (uint8_t) ucExpendedLen;

	/* Increment the total IE length for the Element ID
	 * and Length fields.
	 */
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
void rsnGenerateWPAIE(IN struct ADAPTER *prAdapter,
		      IN struct MSDU_INFO *prMsduInfo)
{
	uint8_t *cp;
	uint8_t *pucBuffer;
	uint8_t ucBssIndex;
	struct BSS_INFO *prBssInfo;
	struct P2P_SPECIFIC_BSS_INFO *prP2pSpecificBssInfo;

	DEBUGFUNC("rsnGenerateWPAIE");

	ASSERT(prMsduInfo);

	pucBuffer = (uint8_t *) ((unsigned long)
				 prMsduInfo->prPacket + (unsigned long)
				 prMsduInfo->u2FrameLength);

	ASSERT(pucBuffer);

	ucBssIndex = prMsduInfo->ucBssIndex;
	prBssInfo = prAdapter->aprBssInfo[ucBssIndex];
	prP2pSpecificBssInfo =
		prAdapter->rWifiVar.
			prP2pSpecificBssInfo[prBssInfo->u4PrivateData];

	/* if (eNetworkId != NETWORK_TYPE_AIS_INDEX) */
	/* return; */

#if CFG_ENABLE_WIFI_DIRECT
	if ((prAdapter->fgIsP2PRegistered &&
	     GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex)->
			eNetworkType == NETWORK_TYPE_P2P &&
	     kalP2PGetTkipCipher(prAdapter->prGlueInfo,
				 (uint8_t) prBssInfo->u4PrivateData)) ||
	    (GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex)->
			eNetworkType == NETWORK_TYPE_AIS &&
	     (prAdapter->rWifiVar.rConnSettings.eAuthMode ==
			AUTH_MODE_WPA ||
	      prAdapter->rWifiVar.rConnSettings.eAuthMode ==
			AUTH_MODE_WPA_PSK))) {
#else
	if (GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex)->
			eNetworkType == NETWORK_TYPE_AIS &&
	    (prAdapter->rWifiVar.rConnSettings.eAuthMode ==
			AUTH_MODE_WPA ||
	     prAdapter->rWifiVar.rConnSettings.eAuthMode ==
			AUTH_MODE_WPA_PSK)) {
#endif
		if (prAdapter->fgIsP2PRegistered && prP2pSpecificBssInfo
		    && (prP2pSpecificBssInfo->u2WpaIeLen != 0)) {
			kalMemCopy(pucBuffer,
				prP2pSpecificBssInfo->aucWpaIeBuffer,
				prP2pSpecificBssInfo->u2WpaIeLen);
			prMsduInfo->u2FrameLength +=
			    prP2pSpecificBssInfo->u2WpaIeLen;
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
		    && GET_BSS_INFO_BY_INDEX(prAdapter,
			     ucBssIndex)->eNetworkType == NETWORK_TYPE_P2P) {
			WLAN_SET_FIELD_32(
				&WPA_IE(pucBuffer)->u4GroupKeyCipherSuite,
				WPA_CIPHER_SUITE_TKIP);
		} else
#endif
			WLAN_SET_FIELD_32(
				&WPA_IE(pucBuffer)->u4GroupKeyCipherSuite,
				prAdapter->prAisBssInfo->
					u4RsnSelectedGroupCipher);

		cp = (uint8_t *) &
		    WPA_IE(pucBuffer)->aucPairwiseKeyCipherSuite1[0];

		WLAN_SET_FIELD_16(&WPA_IE(pucBuffer)->
			u2PairwiseKeyCipherSuiteCount, 1);
#if CFG_ENABLE_WIFI_DIRECT
		if (prAdapter->fgIsP2PRegistered
			&& GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex)->
			eNetworkType == NETWORK_TYPE_P2P) {
			WLAN_SET_FIELD_32(cp, WPA_CIPHER_SUITE_TKIP);
		} else
#endif
			WLAN_SET_FIELD_32(cp, prAdapter->prAisBssInfo
						->u4RsnSelectedPairwiseCipher);

		cp = pucBuffer + sizeof(struct WPA_INFO_ELEM);

		WLAN_SET_FIELD_16(cp, 1);
		cp += 2;
#if CFG_ENABLE_WIFI_DIRECT
		if (prAdapter->fgIsP2PRegistered
		    && GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex)->
			eNetworkType == NETWORK_TYPE_P2P) {
			WLAN_SET_FIELD_32(cp, WPA_AKM_SUITE_PSK);
		} else
#endif
			WLAN_SET_FIELD_32(cp, prAdapter->prAisBssInfo
						->u4RsnSelectedAKMSuite);
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
void rsnGenerateRSNIE(IN struct ADAPTER *prAdapter,
		      IN struct MSDU_INFO *prMsduInfo)
{
	uint8_t *cp;
	/* UINT_8                ucExpendedLen = 0; */
	uint8_t *pucBuffer;
	uint8_t ucBssIndex;
	struct BSS_INFO *prBssInfo;
#if !CFG_SUPPORT_CFG80211_AUTH
	uint32_t u4Entry;
	struct STA_RECORD *prStaRec;
	uint8_t fgPmkCacheExist = FALSE;
#endif
#if CFG_SUPPORT_CFG80211_AUTH
	uint32_t u4GroupMgmt = 0;
	struct CONNECTION_SETTINGS *prConnSettings = NULL;

	prConnSettings = &prAdapter->rWifiVar.rConnSettings;

	ASSERT(prConnSettings);
#endif


	DEBUGFUNC("rsnGenerateRSNIE");

	ASSERT(prMsduInfo);

	pucBuffer = (uint8_t *) ((unsigned long)
				 prMsduInfo->prPacket + (unsigned long)
				 prMsduInfo->u2FrameLength);

	ASSERT(pucBuffer);

	/* Todo:: network id */
	ucBssIndex = prMsduInfo->ucBssIndex;
	/* for Fast Bss Transition,  we reuse the RSN Element composed in
	 * userspace
	 */
	if ((prAdapter->rWifiVar.rConnSettings.eAuthMode == AUTH_MODE_WPA2_FT ||
	     prAdapter->rWifiVar.rConnSettings.eAuthMode ==
		     AUTH_MODE_WPA2_FT_PSK) &&
		prAdapter->prGlueInfo->rFtIeForTx.prRsnIE) {
		authAddRSNIE(prAdapter, prMsduInfo);
		return;
	}
	prBssInfo = prAdapter->aprBssInfo[ucBssIndex];
	ASSERT(prBssInfo);

	if (
#if CFG_ENABLE_WIFI_DIRECT
		((prAdapter->fgIsP2PRegistered) &&
		 (GET_BSS_INFO_BY_INDEX(prAdapter,
			ucBssIndex)->eNetworkType == NETWORK_TYPE_P2P)
		 && (kalP2PGetCcmpCipher(prAdapter->prGlueInfo,
			(uint8_t) prBssInfo->u4PrivateData))) ||
#endif
#if CFG_ENABLE_BT_OVER_WIFI
		(GET_BSS_INFO_BY_INDEX(prAdapter,
			ucBssIndex)->eNetworkType == NETWORK_TYPE_BOW)
		||
#endif
		(GET_BSS_INFO_BY_INDEX(prAdapter,
			ucBssIndex)->eNetworkType ==
		 NETWORK_TYPE_AIS /* prCurrentBss->fgIERSN */  &&
		 ((prAdapter->rWifiVar.rConnSettings.eAuthMode ==
		   AUTH_MODE_WPA2)
		  || (prAdapter->rWifiVar.rConnSettings.eAuthMode ==
		      AUTH_MODE_WPA2_PSK)
		  || (prAdapter->rWifiVar.rConnSettings.eAuthMode ==
		      AUTH_MODE_WPA2_FT_PSK)
		  || (prAdapter->rWifiVar.rConnSettings.eAuthMode ==
		      AUTH_MODE_WPA2_FT)
#if CFG_SUPPORT_CFG80211_AUTH
		|| (prAdapter->rWifiVar.rConnSettings.eAuthMode ==
		AUTH_MODE_WPA2_SAE)
#endif
		))) {
		/* Construct a RSN IE for association request frame. */
		RSN_IE(pucBuffer)->ucElemId = ELEM_ID_RSN;
#if CFG_SUPPORT_CFG80211_AUTH
		RSN_IE(pucBuffer)->ucLength =
			prAdapter->prGlueInfo->rWpaInfo.ucRsneLen;
		if (RSN_IE(pucBuffer)->ucLength < 2) {
			if ((prBssInfo->eCurrentOPMode ==
				OP_MODE_ACCESS_POINT) ||
				(prBssInfo->eNetworkType == NETWORK_TYPE_P2P)) {
				RSN_IE(pucBuffer)->ucLength =
							ELEM_ID_RSN_LEN_FIXED;
			} else {
				DBGLOG(RSN, WARN,
					"Desired RSN IE from upper is too short (length=%d)\n",
					RSN_IE(pucBuffer)->ucLength);
				return;
			}
		}
#else
		RSN_IE(pucBuffer)->ucLength = ELEM_ID_RSN_LEN_FIXED;
#endif
		/* Version */
		WLAN_SET_FIELD_16(&RSN_IE(pucBuffer)->u2Version, 1);
		WLAN_SET_FIELD_32(&RSN_IE(pucBuffer)->u4GroupKeyCipherSuite,
				GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex)->
				u4RsnSelectedGroupCipher);
				/* Group key suite */
		cp = (uint8_t *) &RSN_IE(
			     pucBuffer)->aucPairwiseKeyCipherSuite1[0];
		WLAN_SET_FIELD_16(&RSN_IE(
			pucBuffer)->u2PairwiseKeyCipherSuiteCount, 1);
		WLAN_SET_FIELD_32(cp, GET_BSS_INFO_BY_INDEX(prAdapter,
			ucBssIndex)->u4RsnSelectedPairwiseCipher);

		cp = pucBuffer + sizeof(struct RSN_INFO_ELEM);

		WLAN_SET_FIELD_16(cp, 1);	/* AKM suite count */
		cp += 2;
		/* AKM suite */
		WLAN_SET_FIELD_32(cp, GET_BSS_INFO_BY_INDEX(prAdapter,
				    ucBssIndex)->u4RsnSelectedAKMSuite);
		cp += 4;

		/* Capabilities */
		WLAN_SET_FIELD_16(cp, GET_BSS_INFO_BY_INDEX(prAdapter,
				  ucBssIndex)->u2RsnSelectedCapInfo);
		DBGLOG(RSN, TRACE,
		       "Gen RSN IE = %x\n", GET_BSS_INFO_BY_INDEX(prAdapter,
				       ucBssIndex)->u2RsnSelectedCapInfo);
 #if CFG_SUPPORT_802_11W
		if (GET_BSS_INFO_BY_INDEX(prAdapter,
			ucBssIndex)->eNetworkType == NETWORK_TYPE_AIS) {
			if (kalGetRsnIeMfpCap(prAdapter->prGlueInfo) ==
			    RSN_AUTH_MFP_REQUIRED) {
				WLAN_SET_FIELD_16(cp,
					ELEM_WPA_CAP_MFPC | ELEM_WPA_CAP_MFPR);
					/* Capabilities */
				DBGLOG(RSN, TRACE,
					"RSN_AUTH_MFP - MFPC & MFPR\n");
			} else if (kalGetRsnIeMfpCap(prAdapter->prGlueInfo) ==
				   RSN_AUTH_MFP_OPTIONAL) {
				WLAN_SET_FIELD_16(cp, ELEM_WPA_CAP_MFPC);
					/* Capabilities */
				DBGLOG(RSN, TRACE, "RSN_AUTH_MFP - MFPC\n");
			} else {
				DBGLOG(RSN, TRACE,
					"!RSN_AUTH_MFP - No MFPC!\n");
			}
		} else if ((GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex)->
				eNetworkType == NETWORK_TYPE_P2P) &&
			   (GET_BSS_INFO_BY_INDEX(prAdapter,
				ucBssIndex)->eCurrentOPMode ==
				(uint8_t) OP_MODE_ACCESS_POINT)) {
			/* AP PMF */
			/* for AP mode, keep origin RSN IE content w/o update */
		}
#endif
		cp += 2;

#if CFG_SUPPORT_CFG80211_AUTH
		/*Fill PMKID and Group Management Cipher for AIS */
		if (GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex)->eNetworkType
			== NETWORK_TYPE_AIS) {
			/* Fill PMKID Count field */
			WLAN_SET_FIELD_16(cp,
				prConnSettings->rRsnInfo.u2PmkidCnt);
			cp += 2;

			/* Fill PMKID List field */
			if (prConnSettings->rRsnInfo.u2PmkidCnt > 0) {
				kalMemCopy(cp,
					&prConnSettings->rRsnInfo.aucPmkidList,
					(prConnSettings
					->rRsnInfo.u2PmkidCnt * RSN_PMKID_LEN));
				DBGLOG(RSN, INFO,
					"Dump PMDID when gen rsn ie & len:%d\n",
					RSN_IE(pucBuffer)->ucLength);
				DBGLOG_MEM8(RSN, INFO, cp,
					(prConnSettings
					->rRsnInfo.u2PmkidCnt * RSN_PMKID_LEN));
			}

			cp += (prConnSettings
				->rRsnInfo.u2PmkidCnt * RSN_PMKID_LEN);
#if CFG_SUPPORT_802_11W
			/* Fill Group Management Cipher field */
			u4GroupMgmt =
			prAdapter->prGlueInfo->rWpaInfo.u4CipherGroupMgmt;
			WLAN_SET_FIELD_32(cp, u4GroupMgmt);
#endif
		}
#else
		/* Fill PMKID and Group Management Cipher for AIS */
		if (GET_BSS_INFO_BY_INDEX(prAdapter,
				ucBssIndex)->eNetworkType == NETWORK_TYPE_AIS) {
			prStaRec = cnmGetStaRecByIndex(prAdapter,
						prMsduInfo->ucStaRecIndex);

			if (rsnSearchPmkidEntry(prAdapter,
				prStaRec->aucMacAddr, &u4Entry)) {
				if (prAdapter->rWifiVar.
					rAisSpecificBssInfo.
					arPmkidCache[u4Entry].fgPmkidExist)
					fgPmkCacheExist = TRUE;
			}

			/* Fill PMKID Count and List field */
			if (fgPmkCacheExist) {
				RSN_IE(pucBuffer)->ucLength = 38;
				/* Fill PMKID Count field */
				WLAN_SET_FIELD_16(cp, 1);
				cp += 2;
				DBGLOG(RSN, TRACE,
				       "BSSID " MACSTR " ind=%d\n",
				       MAC2STR(prStaRec->aucMacAddr),
				       u4Entry);
				DBGLOG(RSN, TRACE,
				       "use PMKID " MACSTR "\n",
				       MAC2STR(prAdapter->rWifiVar.
						rAisSpecificBssInfo.
						arPmkidCache[u4Entry].
						rBssidInfo.arPMKID));
				/* Fill PMKID List field */
				kalMemCopy(cp,
					(void *) prAdapter->rWifiVar.
						rAisSpecificBssInfo.
						arPmkidCache[u4Entry].
						rBssidInfo.arPMKID,
					(sizeof(uint8_t) * 16));
				cp += 16;
			}
#if CFG_SUPPORT_802_11W
			else {
				/* Follow supplicant flow to
				 * fill PMKID Count field = 0 only when
				 * Group Management Cipher field
				 * need to be filled
				 */
				if (prAdapter->rWifiVar.
					rAisSpecificBssInfo.fgMgmtProtection) {
					WLAN_SET_FIELD_16(cp, 0)

					cp += 2;
					RSN_IE(pucBuffer)->ucLength += 2;
				}
			}

			/* Fill Group Management Cipher field */
			if (prAdapter->rWifiVar.
				rAisSpecificBssInfo.fgMgmtProtection) {
				WLAN_SET_FIELD_32(cp,
					RSN_CIPHER_SUITE_AES_128_CMAC);
				cp += 4;
				RSN_IE(pucBuffer)->ucLength += 4;
			}
#endif
		}
#endif
		prMsduInfo->u2FrameLength += IE_SIZE(pucBuffer);
	}

}				/* rsnGenerateRSNIE */

/*----------------------------------------------------------------------------*/
/*!
 * \brief Parse the given IE buffer and check if it is WFA IE and return Type
 *	and SubType for further process.
 *
 * \param[in] pucBuf             Pointer to buffer of WFA Information Element.
 * \param[out] pucOuiType        Pointer to the storage of OUI Type.
 * \param[out] pu2SubTypeVersion Pointer to the storage of OUI SubType
 *					and Version.
 * \retval TRUE  Parse IE ok
 * \retval FALSE Parse IE fail
 */
/*----------------------------------------------------------------------------*/
u_int8_t
rsnParseCheckForWFAInfoElem(IN struct ADAPTER *prAdapter,
			    IN uint8_t *pucBuf, OUT uint8_t *pucOuiType,
			    OUT uint16_t *pu2SubTypeVersion)
{
	uint8_t aucWfaOui[] = VENDOR_OUI_WFA;
	struct IE_WFA *prWfaIE;

	ASSERT(pucBuf);
	ASSERT(pucOuiType);
	ASSERT(pu2SubTypeVersion);
	prWfaIE = (struct IE_WFA *)pucBuf;

	do {
		if (IE_LEN(pucBuf) <= ELEM_MIN_LEN_WFA_OUI_TYPE_SUBTYPE) {
			break;
		} else if (prWfaIE->aucOui[0] != aucWfaOui[0] ||
			   prWfaIE->aucOui[1] != aucWfaOui[1]
			   || prWfaIE->aucOui[2] != aucWfaOui[2]) {
			break;
		}

		*pucOuiType = prWfaIE->ucOuiType;
		WLAN_GET_FIELD_16(&prWfaIE->aucOuiSubTypeVersion[0],
				  pu2SubTypeVersion);

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
 * \param[in] pIE                   Pointer rthe buffer of Information Element.
 * \param[out] prStatusCode     Pointer to the return status code.

 * \retval none
 */
/*----------------------------------------------------------------------------*/
void rsnParserCheckForRSNCCMPPSK(struct ADAPTER *prAdapter,
				 struct RSN_INFO_ELEM *prIe,
				 struct STA_RECORD *prStaRec,
				 uint16_t *pu2StatusCode)
{

	struct RSN_INFO rRsnIe;
	struct BSS_INFO *prBssInfo;
	uint8_t i;
	uint16_t statusCode;

	ASSERT(prAdapter);
	ASSERT(prIe);
	ASSERT(prStaRec);
	ASSERT(pu2StatusCode);

	*pu2StatusCode = STATUS_CODE_INVALID_INFO_ELEMENT;

	if (rsnParseRsnIE(prAdapter, prIe, &rRsnIe)) {
		if ((rRsnIe.u4PairwiseKeyCipherSuiteCount != 1)
		    || (rRsnIe.au4PairwiseKeyCipherSuite[0] !=
			RSN_CIPHER_SUITE_CCMP)) {
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

#if CFG_SUPPORT_802_11W
		/* AP PMF */
		/* 1st check: if already PMF connection, reject assoc req:
		 * error 30 ASSOC_REJECTED_TEMPORARILY
		 */
		if (rsnCheckBipKeyInstalled(prAdapter, prStaRec)) {
			*pu2StatusCode = STATUS_CODE_ASSOC_REJECTED_TEMPORARILY;
			return;
		}

		/* if RSN capability not exist, just return */
		if (!rRsnIe.fgRsnCapPresent) {
			*pu2StatusCode = WLAN_STATUS_SUCCESS;
			return;
		}

		prStaRec->rPmfCfg.fgMfpc = (rRsnIe.u2RsnCap &
					    ELEM_WPA_CAP_MFPC) ? 1 : 0;
		prStaRec->rPmfCfg.fgMfpr = (rRsnIe.u2RsnCap &
					    ELEM_WPA_CAP_MFPR) ? 1 : 0;

		for (i = 0; i < rRsnIe.u4AuthKeyMgtSuiteCount; i++) {
			if ((rRsnIe.au4AuthKeyMgtSuite[i] ==
			     RSN_AKM_SUITE_802_1X_SHA256) ||
			    (rRsnIe.au4AuthKeyMgtSuite[i] ==
			     RSN_AKM_SUITE_PSK_SHA256)) {
				DBGLOG(RSN, INFO, "STA SHA256 support\n");
				prStaRec->rPmfCfg.fgSha256 = TRUE;
				break;
			}
		}

		DBGLOG(RSN, INFO,
		       "STA Assoc req mfpc:%d, mfpr:%d, sha256:%d, bssIndex:%d, applyPmf:%d\n",
		       prStaRec->rPmfCfg.fgMfpc, prStaRec->rPmfCfg.fgMfpr,
		       prStaRec->rPmfCfg.fgSha256, prStaRec->ucBssIndex,
		       prStaRec->rPmfCfg.fgApplyPmf);

		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
						  prStaRec->ucBssIndex);

		/* if PMF validation fail, return success as legacy association
		 */
		statusCode = rsnPmfCapableValidation(prAdapter, prBssInfo,
						     prStaRec);
		*pu2StatusCode = statusCode;
#endif
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
void rsnGenMicErrorEvent(IN struct ADAPTER *prAdapter, IN u_int8_t fgFlags)
{
	struct PARAM_AUTH_EVENT *prAuthEvent;

	DEBUGFUNC("rsnGenMicErrorEvent");

	prAuthEvent = (struct PARAM_AUTH_EVENT *)
	    prAdapter->aucIndicationEventBuffer;

	/* Status type: Authentication Event */
	prAuthEvent->rStatus.eStatusType = ENUM_STATUS_TYPE_AUTHENTICATION;

	/* Authentication request */
	prAuthEvent->arRequest[0].u4Length = sizeof(struct PARAM_AUTH_REQUEST);
	kalMemCopy((void *)prAuthEvent->arRequest[0].arBssid,
		   (void *)prAdapter->prAisBssInfo->aucBSSID, MAC_ADDR_LEN);

	if (fgFlags == TRUE)
		prAuthEvent->arRequest[0].u4Flags =
		    PARAM_AUTH_REQUEST_GROUP_ERROR;
	else
		prAuthEvent->arRequest[0].u4Flags =
		    PARAM_AUTH_REQUEST_PAIRWISE_ERROR;

	kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
				     WLAN_STATUS_MEDIA_SPECIFIC_INDICATION,
				     (void *) prAuthEvent,
				     sizeof(struct PARAM_STATUS_INDICATION)
				     + sizeof(struct PARAM_AUTH_REQUEST));

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
void rsnTkipHandleMICFailure(IN struct ADAPTER *prAdapter,
			     IN struct STA_RECORD *prSta,
			     IN u_int8_t fgErrorKeyType)
{
	/* UINT_32               u4RsnaCurrentMICFailTime; */
	/* P_AIS_SPECIFIC_BSS_INFO_T prAisSpecBssInfo; */

	DEBUGFUNC("rsnTkipHandleMICFailure");

	ASSERT(prAdapter);
#if 1
	rsnGenMicErrorEvent(prAdapter, /* prSta, */ fgErrorKeyType);

	/* Generate authentication request event. */
	DBGLOG(RSN, INFO,
	       "Generate TKIP MIC error event (type: 0%d)\n", fgErrorKeyType);
#else
	ASSERT(prSta);

	prAisSpecBssInfo = &prAdapter->rWifiVar.rAisSpecificBssInfo;

	/* Record the MIC error occur time. */
	GET_CURRENT_SYSTIME(&u4RsnaCurrentMICFailTime);

	/* Generate authentication request event. */
	DBGLOG(RSN, INFO,
	       "Generate TKIP MIC error event (type: 0%d)\n", fgErrorKeyType);

	/* If less than 60 seconds have passed since a previous TKIP MIC
	 * failure, disassociate from the AP and wait for 60 seconds
	 * before (re)associating with the same AP.
	 */
	if (prAisSpecBssInfo->u4RsnaLastMICFailTime != 0 &&
	    !CHECK_FOR_TIMEOUT(u4RsnaCurrentMICFailTime,
			       prAisSpecBssInfo->u4RsnaLastMICFailTime,
			       SEC_TO_SYSTIME(TKIP_COUNTERMEASURE_SEC))) {
		/* If less than 60 seconds expired since last MIC error,
		 * we have to block traffic.
		 */

		DBGLOG(RSN, INFO, "Start blocking traffic!\n");
		rsnGenMicErrorEvent(prAdapter, /* prSta, */ fgErrorKeyType);

		secFsmEventStartCounterMeasure(prAdapter, prSta);
	} else {
		rsnGenMicErrorEvent(prAdapter, /* prSta, */ fgErrorKeyType);
		DBGLOG(RSN, INFO, "First TKIP MIC error!\n");
	}

	COPY_SYSTIME(prAisSpecBssInfo->u4RsnaLastMICFailTime,
		     u4RsnaCurrentMICFailTime);
#endif
}				/* rsnTkipHandleMICFailure */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is called to select a list of BSSID from
 *        the scan results for PMKID candidate list.
 *
 * \param[in] prBssDesc the BSS Desc at scan result list
 * \param[out] pu4CandidateCount Pointer to the number of selected candidates.
 *                      It is set to zero if no BSSID matches our requirement.
 *
 * \retval none
 */
/*----------------------------------------------------------------------------*/
void rsnSelectPmkidCandidateList(IN struct ADAPTER
				 *prAdapter, IN struct BSS_DESC *prBssDesc)
{
	struct CONNECTION_SETTINGS *prConnSettings;
	struct BSS_INFO *prAisBssInfo;

	DEBUGFUNC("rsnSelectPmkidCandidateList");

	ASSERT(prBssDesc);

	prConnSettings = &prAdapter->rWifiVar.rConnSettings;
	prAisBssInfo = prAdapter->prAisBssInfo;

	/* Search a BSS with the same SSID from the given BSS description set.
	 * DBGLOG(RSN, TRACE, ("Check scan result ["MACSTR"]\n",
	 *     MAC2STR(prBssDesc->aucBSSID)));
	 */

	if (UNEQUAL_SSID(prBssDesc->aucSSID, prBssDesc->ucSSIDLen,
			 prConnSettings->aucSSID, prConnSettings->ucSSIDLen)) {
		DBGLOG(RSN, TRACE, "-- SSID not matched\n");
		return;
	}

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
void rsnUpdatePmkidCandidateList(IN struct ADAPTER
				 *prAdapter, IN struct BSS_DESC *prBssDesc)
{
	uint32_t i;
	struct CONNECTION_SETTINGS *prConnSettings;
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecBssInfo;

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
		if (EQUAL_MAC_ADDR(prBssDesc->aucBSSID,
			prAisSpecBssInfo->arPmkidCandicate[i].aucBssid))
			return;
	}

	/* If the number of selected BSSID exceed MAX_NUM_PMKID_CACHE(16),
	 *  then we only store MAX_NUM_PMKID_CACHE(16) in PMKID cache
	 */
	if ((prAisSpecBssInfo->u4PmkidCandicateCount + 1) > CFG_MAX_PMKID_CACHE)
		prAisSpecBssInfo->u4PmkidCandicateCount--;

	i = prAisSpecBssInfo->u4PmkidCandicateCount;

	COPY_MAC_ADDR((void *)
		      prAisSpecBssInfo->arPmkidCandicate[i].aucBssid,
		      (void *)prBssDesc->aucBSSID);

	if (prBssDesc->u2RsnCap & MASK_RSNIE_CAP_PREAUTH) {
		prAisSpecBssInfo->arPmkidCandicate[i].u4PreAuthFlags = 1;
		DBGLOG(RSN, TRACE,
		       "Add " MACSTR " with pre-auth to candidate list\n",
		       MAC2STR(prAisSpecBssInfo->arPmkidCandicate[i].aucBssid));
	} else {
		prAisSpecBssInfo->arPmkidCandicate[i].u4PreAuthFlags = 0;
		DBGLOG(RSN, TRACE,
		       "Add " MACSTR " without pre-auth to candidate list\n",
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
u_int8_t rsnSearchPmkidEntry(IN struct ADAPTER *prAdapter,
			     IN uint8_t *pucBssid,
			     OUT uint32_t *pu4EntryIndex)
{
	uint32_t i;
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecBssInfo;

	DEBUGFUNC("rsnSearchPmkidEntry");

	ASSERT(pucBssid);
	ASSERT(pu4EntryIndex);

	prAisSpecBssInfo = &prAdapter->rWifiVar.rAisSpecificBssInfo;

	if (prAisSpecBssInfo->u4PmkidCacheCount > CFG_MAX_PMKID_CACHE)
		return FALSE;

	ASSERT(prAisSpecBssInfo->u4PmkidCacheCount <= CFG_MAX_PMKID_CACHE);

	/* Search for desired BSSID */
	for (i = 0; i < prAisSpecBssInfo->u4PmkidCacheCount; i++) {
		if (!kalMemCmp(
			prAisSpecBssInfo->arPmkidCache[i].rBssidInfo.arBSSID,
			pucBssid, MAC_ADDR_LEN))
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
u_int8_t rsnCheckPmkidCandicate(IN struct ADAPTER *prAdapter)
{
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecBssInfo;
	uint32_t i;		/* Index for PMKID candicate */
	uint32_t j;		/* Indix for PMKID cache */
	u_int8_t status = FALSE;

	DEBUGFUNC("rsnCheckPmkidCandicate");

	prAisSpecBssInfo = &prAdapter->rWifiVar.rAisSpecificBssInfo;

	/* Check for each candicate */
	for (i = 0; i < prAisSpecBssInfo->u4PmkidCandicateCount; i++) {
		for (j = 0; j < prAisSpecBssInfo->u4PmkidCacheCount; j++) {
			if (!kalMemCmp
			    (prAisSpecBssInfo->arPmkidCache[j].rBssidInfo.
			     arBSSID,
			     prAisSpecBssInfo->arPmkidCandicate[i].aucBssid,
			     MAC_ADDR_LEN)) {
				/* DBGLOG(
				 * RSN, TRACE, (MACSTR" at PMKID cache!!\n",
				 * MAC2STR(prAisSpecBssInfo->
				 *      arPmkidCandicate[i].aucBssid)));
				 */
				break;
			}
		}

		/* No entry found in PMKID cache for the candicate,
		 * add new one
		 */
		if (j == prAisSpecBssInfo->u4PmkidCacheCount
		    && prAisSpecBssInfo->u4PmkidCacheCount <
		    CFG_MAX_PMKID_CACHE) {
			DBGLOG(RSN, TRACE,
			       "Add " MACSTR " to PMKID cache!!\n",
			       MAC2STR(prAisSpecBssInfo->
					arPmkidCandicate[i].aucBssid));
			kalMemCopy((void *) prAisSpecBssInfo->
					arPmkidCache[prAisSpecBssInfo->
					u4PmkidCacheCount].rBssidInfo.arBSSID,
				   (void *) prAisSpecBssInfo->
					arPmkidCandicate[i].aucBssid,
					MAC_ADDR_LEN);
			prAisSpecBssInfo->arPmkidCache[prAisSpecBssInfo->
				u4PmkidCacheCount].fgPmkidExist
				= FALSE;
			prAisSpecBssInfo->u4PmkidCacheCount++;

			status = TRUE;
		}
	}

	return status;
}				/* rsnCheckPmkidCandicate */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is called to wait a duration
 *		to indicate the pre-auth AP candicate
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
void rsnIndicatePmkidCand(IN struct ADAPTER *prAdapter,
			  IN unsigned long ulParamPtr)
{
	DBGLOG(RSN, EVENT, "Security - Time to indicate the PMKID cand.\n");

	/* If the authentication mode is WPA2 and indication PMKID flag
	 *  is available, then we indicate the PMKID candidate list to NDIS and
	 *  clear the flag, indicatePMKID
	 */

	if (prAdapter->prAisBssInfo->eConnectionState ==
	    PARAM_MEDIA_STATE_CONNECTED &&
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
void rsnCheckPmkidCache(IN struct ADAPTER *prAdapter, IN struct BSS_DESC *prBss)
{
	struct BSS_INFO *prAisBssInfo;
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecBssInfo;
	struct CONNECTION_SETTINGS *prConnSettings;

	DEBUGFUNC("rsnCheckPmkidCandicate");

	ASSERT(prBss);

	prConnSettings = &prAdapter->rWifiVar.rConnSettings;
	prAisBssInfo = prAdapter->prAisBssInfo;
	prAisSpecBssInfo = &prAdapter->rWifiVar.rAisSpecificBssInfo;

	if ((prAisBssInfo->eConnectionState ==
	     PARAM_MEDIA_STATE_CONNECTED) &&
	    (prConnSettings->eAuthMode == AUTH_MODE_WPA2)) {
		rsnSelectPmkidCandidateList(prAdapter, prBss);

		/* Set indication flag of PMKID to TRUE, and then
		 *  connHandleNetworkConnection() will indicate this later
		 */
		if (rsnCheckPmkidCandicate(prAdapter)) {
			DBGLOG(RSN, TRACE,
			       "Prepare a timer to indicate candidate PMKID Candidate\n");
			cnmTimerStopTimer(prAdapter,
				&prAisSpecBssInfo->rPreauthenticationTimer);
			cnmTimerStartTimer(prAdapter,
				&prAisSpecBssInfo->rPreauthenticationTimer,
				SEC_TO_MSEC(
				WAIT_TIME_IND_PMKID_CANDICATE_SEC));
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
void rsnGeneratePmkidIndication(IN struct ADAPTER
				*prAdapter)
{
	struct PARAM_STATUS_INDICATION *prStatusEvent;
	struct PARAM_PMKID_CANDIDATE_LIST *prPmkidEvent;
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecificBssInfo;
	uint8_t i, j = 0, count = 0;
	uint32_t u4LenOfUsedBuffer;

	DEBUGFUNC("rsnGeneratePmkidIndication");

	ASSERT(prAdapter);

	prStatusEvent = (struct PARAM_STATUS_INDICATION *)
	    prAdapter->aucIndicationEventBuffer;

	/* Status type: PMKID Candidatelist Event */
	prStatusEvent->eStatusType = ENUM_STATUS_TYPE_CANDIDATE_LIST;
	ASSERT(prStatusEvent);

	prPmkidEvent =
	    (struct PARAM_PMKID_CANDIDATE_LIST *)(&prStatusEvent->eStatusType +
						  1);
	ASSERT(prPmkidEvent);

	prAisSpecificBssInfo = &prAdapter->rWifiVar.rAisSpecificBssInfo;
	ASSERT(prAisSpecificBssInfo);

	for (i = 0; i < prAisSpecificBssInfo->u4PmkidCandicateCount;
	     i++) {
		for (j = 0; j < prAisSpecificBssInfo->u4PmkidCacheCount;
		     j++) {
			if (EQUAL_MAC_ADDR(
				prAisSpecificBssInfo->arPmkidCache[j].
					rBssidInfo.arBSSID,
				prAisSpecificBssInfo->arPmkidCandicate[i].
					aucBssid) &&
			    (prAisSpecificBssInfo->arPmkidCache[j].fgPmkidExist
				== TRUE)) {
				break;
			}
		}
		if (count >= CFG_MAX_PMKID_CACHE)
			break;

		if (j == prAisSpecificBssInfo->u4PmkidCacheCount) {
			kalMemCopy((void *)
				   prPmkidEvent->arCandidateList[count].arBSSID,
				   (void *) prAisSpecificBssInfo->
					arPmkidCandicate[i].aucBssid,
				   PARAM_MAC_ADDR_LEN);
			prPmkidEvent->arCandidateList[count].u4Flags =
				prAisSpecificBssInfo->arPmkidCandicate[i].
					u4PreAuthFlags;
			DBGLOG(RSN, TRACE,
			       MACSTR " %x\n",
			       MAC2STR(prPmkidEvent->arCandidateList[count].
					arBSSID),
			       prPmkidEvent->arCandidateList[count].u4Flags);
			count++;
		}
	}

	/* PMKID Candidate List */
	prPmkidEvent->u4Version = 1;
	prPmkidEvent->u4NumCandidates = count;
	DBGLOG(RSN, TRACE, "rsnGeneratePmkidIndication #%d\n",
	       prPmkidEvent->u4NumCandidates);
	u4LenOfUsedBuffer = sizeof(enum ENUM_STATUS_TYPE) +
			    (2 * sizeof(uint32_t)) +
			    (count * sizeof(struct PARAM_PMKID_CANDIDATE));
	/* dumpMemory8((uint8_t *)prAdapter->aucIndicationEventBuffer,
	 *    u4LenOfUsedBuffer);
	 */

	kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
			WLAN_STATUS_MEDIA_SPECIFIC_INDICATION,
			(void *) prAdapter->aucIndicationEventBuffer,
			u4LenOfUsedBuffer);

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
void rsnGenerateWSCIE(IN struct ADAPTER *prAdapter,
		      IN struct MSDU_INFO *prMsduInfo)
{
	uint8_t *pucBuffer;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

	if (prMsduInfo->ucBssIndex != prAdapter->prAisBssInfo->ucBssIndex)
		return;

	pucBuffer = (uint8_t *) ((unsigned long)
				 prMsduInfo->prPacket + (unsigned long)
				 prMsduInfo->u2FrameLength);

	/* ASSOC INFO IE ID: 221 :0xDD */
	if (prAdapter->prGlueInfo->u2WSCAssocInfoIELen) {
		kalMemCopy(pucBuffer,
			   &prAdapter->prGlueInfo->aucWSCAssocInfoIE,
			   prAdapter->prGlueInfo->u2WSCAssocInfoIELen);
		prMsduInfo->u2FrameLength +=
		    prAdapter->prGlueInfo->u2WSCAssocInfoIELen;
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
uint32_t rsnCheckBipKeyInstalled(IN struct ADAPTER
				 *prAdapter, IN struct STA_RECORD *prStaRec)
{
	/* caution: prStaRec might be null ! */
	if (prStaRec) {
		if (GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex)
		    ->eNetworkType == (uint8_t) NETWORK_TYPE_AIS) {
			return prAdapter->rWifiVar.rAisSpecificBssInfo.
			    fgBipKeyInstalled;
		} else if ((GET_BSS_INFO_BY_INDEX(prAdapter,
				prStaRec->ucBssIndex)
				->eNetworkType == NETWORK_TYPE_P2P)
				&&
			(GET_BSS_INFO_BY_INDEX(prAdapter,
				prStaRec->ucBssIndex)
				->eCurrentOPMode == OP_MODE_ACCESS_POINT)) {
			DBGLOG(RSN, INFO, "AP-STA PMF capable:%d\n",
			       prStaRec->rPmfCfg.fgApplyPmf);
			return prStaRec->rPmfCfg.fgApplyPmf;
		} else {
			return FALSE;
		}
	} else
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
uint8_t rsnCheckSaQueryTimeout(IN struct ADAPTER *prAdapter)
{
	struct AIS_SPECIFIC_BSS_INFO *prBssSpecInfo;
	uint32_t now;

	prBssSpecInfo = &prAdapter->rWifiVar.rAisSpecificBssInfo;
	ASSERT(prBssSpecInfo);

	GET_CURRENT_SYSTIME(&now);

	if (CHECK_FOR_TIMEOUT(now, prBssSpecInfo->u4SaQueryStart,
			      TU_TO_MSEC(1000))) {
		DBGLOG(RSN, INFO, "association SA Query timed out\n");

		prBssSpecInfo->ucSaQueryTimedOut = 1;
		kalMemFree(prBssSpecInfo->pucSaQueryTransId, VIR_MEM_TYPE,
			   prBssSpecInfo->u4SaQueryCount
				* ACTION_SA_QUERY_TR_ID_LEN);
		prBssSpecInfo->pucSaQueryTransId = NULL;
		prBssSpecInfo->u4SaQueryCount = 0;
		cnmTimerStopTimer(prAdapter, &prBssSpecInfo->rSaQueryTimer);
#if 1
		if (prAdapter->prAisBssInfo->eConnectionState ==
		    PARAM_MEDIA_STATE_CONNECTED) {
			struct MSG_AIS_ABORT *prAisAbortMsg;

			prAisAbortMsg =
				(struct MSG_AIS_ABORT *) cnmMemAlloc(prAdapter,
						RAM_TYPE_MSG,
						sizeof(struct MSG_AIS_ABORT));
			if (!prAisAbortMsg)
				return 0;
			prAisAbortMsg->rMsgHdr.eMsgId = MID_SAA_AIS_FSM_ABORT;
			prAisAbortMsg->ucReasonOfDisconnect =
			    DISCONNECT_REASON_CODE_DISASSOCIATED;
			prAisAbortMsg->fgDelayIndication = FALSE;

#if CFG_DISCONN_DEBUG_FEATURE
			g_rDisconnInfoTemp.ucTrigger =
				DISCONNECT_TRIGGER_ACTIVE;
#endif

			mboxSendMsg(prAdapter, MBOX_ID_0,
				    (struct MSG_HDR *) prAisAbortMsg,
				    MSG_SEND_METHOD_BUF);
		}
#else
		/* Re-connect */
		kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
					WLAN_STATUS_MEDIA_DISCONNECT, NULL, 0);
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
void rsnStartSaQueryTimer(IN struct ADAPTER *prAdapter,
			  IN unsigned long ulParamPtr)
{
	struct BSS_INFO *prBssInfo;
	struct AIS_SPECIFIC_BSS_INFO *prBssSpecInfo;
	struct MSDU_INFO *prMsduInfo;
	struct ACTION_SA_QUERY_FRAME *prTxFrame;
	uint16_t u2PayloadLen;
	uint8_t *pucTmp = NULL;
	uint8_t ucTransId[ACTION_SA_QUERY_TR_ID_LEN];

	prBssInfo = prAdapter->prAisBssInfo;
	ASSERT(prBssInfo);

	prBssSpecInfo = &prAdapter->rWifiVar.rAisSpecificBssInfo;
	ASSERT(prBssSpecInfo);

	DBGLOG(RSN, INFO, "MFP: Start Sa Query\n");

	if (prBssInfo->prStaRecOfAP == NULL) {
		if (prBssSpecInfo->u4SaQueryCount > 0)
			rsnStopSaQuery(prAdapter);
		DBGLOG(RSN, INFO, "MFP: unassociated AP!\n");
		return;
	}

	if (prBssSpecInfo->u4SaQueryCount > 0
	    && rsnCheckSaQueryTimeout(prAdapter)) {
		DBGLOG(RSN, INFO, "MFP: u4SaQueryCount count =%d\n",
		       prBssSpecInfo->u4SaQueryCount);
		return;
	}

	prMsduInfo = (struct MSDU_INFO *)cnmMgtPktAlloc(prAdapter,
							MAC_TX_RESERVED_FIELD +
							PUBLIC_ACTION_MAX_LEN);

	if (!prMsduInfo)
		return;

	prTxFrame = (struct ACTION_SA_QUERY_FRAME *)
	    ((unsigned long)(prMsduInfo->prPacket) + MAC_TX_RESERVED_FIELD);

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
		pucTmp = kalMemAlloc(prBssSpecInfo->u4SaQueryCount *
				     ACTION_SA_QUERY_TR_ID_LEN, VIR_MEM_TYPE);
		if (!pucTmp) {
			DBGLOG(RSN, INFO,
			       "MFP: Fail to alloc tmp buffer for backup sa query id\n");
			cnmMgtPktFree(prAdapter, prMsduInfo);
			return;
		}
		kalMemCopy(pucTmp, prBssSpecInfo->pucSaQueryTransId,
			   prBssSpecInfo->u4SaQueryCount
				* ACTION_SA_QUERY_TR_ID_LEN);
	}

	kalMemFree(prBssSpecInfo->pucSaQueryTransId, VIR_MEM_TYPE,
		   prBssSpecInfo->u4SaQueryCount * ACTION_SA_QUERY_TR_ID_LEN);

	ucTransId[0] = (uint8_t) (kalRandomNumber() & 0xFF);
	ucTransId[1] = (uint8_t) (kalRandomNumber() & 0xFF);

	kalMemCopy(prTxFrame->ucTransId, ucTransId, ACTION_SA_QUERY_TR_ID_LEN);

	prBssSpecInfo->u4SaQueryCount++;

	prBssSpecInfo->pucSaQueryTransId =
	    kalMemAlloc(prBssSpecInfo->u4SaQueryCount *
			ACTION_SA_QUERY_TR_ID_LEN, VIR_MEM_TYPE);
	if (!prBssSpecInfo->pucSaQueryTransId) {
		kalMemFree(pucTmp, VIR_MEM_TYPE,
			   (prBssSpecInfo->u4SaQueryCount - 1) *
			   ACTION_SA_QUERY_TR_ID_LEN);
		DBGLOG(RSN, INFO,
		       "MFP: Fail to alloc buffer for sa query id list\n");
		cnmMgtPktFree(prAdapter, prMsduInfo);
		return;
	}

	if (pucTmp) {
		kalMemCopy(prBssSpecInfo->pucSaQueryTransId, pucTmp,
			   (prBssSpecInfo->u4SaQueryCount - 1) *
			   ACTION_SA_QUERY_TR_ID_LEN);
		kalMemCopy(
			&prBssSpecInfo->pucSaQueryTransId[
				(prBssSpecInfo->u4SaQueryCount - 1)
				* ACTION_SA_QUERY_TR_ID_LEN],
				ucTransId, ACTION_SA_QUERY_TR_ID_LEN);
		kalMemFree(pucTmp, VIR_MEM_TYPE,
			   (prBssSpecInfo->u4SaQueryCount -
			    1) * ACTION_SA_QUERY_TR_ID_LEN);
	} else {
		kalMemCopy(prBssSpecInfo->pucSaQueryTransId, ucTransId,
			   ACTION_SA_QUERY_TR_ID_LEN);
	}

	u2PayloadLen = 2 + ACTION_SA_QUERY_TR_ID_LEN;

	/* 4 <3> Update information of MSDU_INFO_T */
	TX_SET_MMPDU(prAdapter,
		     prMsduInfo,
		     prBssInfo->prStaRecOfAP->ucBssIndex,
		     prBssInfo->prStaRecOfAP->ucIndex,
		     WLAN_MAC_MGMT_HEADER_LEN,
		     WLAN_MAC_MGMT_HEADER_LEN + u2PayloadLen, NULL,
		     MSDU_RATE_MODE_AUTO);

	if (rsnCheckBipKeyInstalled(prAdapter,
				    prBssInfo->prStaRecOfAP)) {
		DBGLOG(RSN, INFO, "Set MSDU_OPT_PROTECTED_FRAME");
		nicTxConfigPktOption(prMsduInfo, MSDU_OPT_PROTECTED_FRAME,
				     TRUE);
	}
	/* 4 Enqueue the frame to send this action frame. */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);

	DBGLOG(RSN, INFO, "Set SA Query timer %d (%d Tu)",
	       prBssSpecInfo->u4SaQueryCount, 201);

	cnmTimerStartTimer(prAdapter, &prBssSpecInfo->rSaQueryTimer,
			   TU_TO_MSEC(201));

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
void rsnStartSaQuery(IN struct ADAPTER *prAdapter)
{
	struct AIS_SPECIFIC_BSS_INFO *prBssSpecInfo;

	prBssSpecInfo = &prAdapter->rWifiVar.rAisSpecificBssInfo;
	ASSERT(prBssSpecInfo);
	DBGLOG(RSN, INFO, "prBssSpecInfo->u4SaQueryCount %d\n",
		prBssSpecInfo->u4SaQueryCount);

	if (prBssSpecInfo->u4SaQueryCount == 0)
		rsnStartSaQueryTimer(prAdapter, (unsigned long)NULL);
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
void rsnStopSaQuery(IN struct ADAPTER *prAdapter)
{
	struct AIS_SPECIFIC_BSS_INFO *prBssSpecInfo;

	prBssSpecInfo = &prAdapter->rWifiVar.rAisSpecificBssInfo;
	ASSERT(prBssSpecInfo);

	cnmTimerStopTimer(prAdapter, &prBssSpecInfo->rSaQueryTimer);

	if (prBssSpecInfo->pucSaQueryTransId) {
		kalMemFree(prBssSpecInfo->pucSaQueryTransId, VIR_MEM_TYPE,
			   prBssSpecInfo->u4SaQueryCount
				* ACTION_SA_QUERY_TR_ID_LEN);
		prBssSpecInfo->pucSaQueryTransId = NULL;
	}

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
void rsnSaQueryRequest(IN struct ADAPTER *prAdapter, IN struct SW_RFB *prSwRfb)
{
	struct BSS_INFO *prBssInfo;
	struct MSDU_INFO *prMsduInfo;
	struct ACTION_SA_QUERY_FRAME *prRxFrame = NULL;
	uint16_t u2PayloadLen;
	struct STA_RECORD *prStaRec;
	struct ACTION_SA_QUERY_FRAME *prTxFrame;

	prBssInfo = prAdapter->prAisBssInfo;
	ASSERT(prBssInfo);

	if (!prSwRfb)
		return;

	prRxFrame = (struct ACTION_SA_QUERY_FRAME *)
	    prSwRfb->pvHeader;
	if (!prRxFrame)
		return;

	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);
	if (!prStaRec)		/* Todo:: for not AIS check */
		return;

	DBGLOG(RSN, INFO,
	       "IEEE 802.11: Received SA Query Request from " MACSTR "\n",
	       MAC2STR(prStaRec->aucMacAddr));

	DBGLOG_MEM8(RSN, INFO, prRxFrame->ucTransId, ACTION_SA_QUERY_TR_ID_LEN);

	if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) ==
	    PARAM_MEDIA_STATE_DISCONNECTED) {
		DBGLOG(RSN, INFO,
		       "IEEE 802.11: Ignore SA Query Request from unassociated STA "
		       MACSTR "\n", MAC2STR(prStaRec->aucMacAddr));
		return;
	}

	DBGLOG(RSN, INFO,
	       "IEEE 802.11: Sending SA Query Response to " MACSTR "\n",
	       MAC2STR(prStaRec->aucMacAddr));

	prMsduInfo = (struct MSDU_INFO *)cnmMgtPktAlloc(prAdapter,
							MAC_TX_RESERVED_FIELD +
							PUBLIC_ACTION_MAX_LEN);

	if (!prMsduInfo)
		return;

	prTxFrame = (struct ACTION_SA_QUERY_FRAME *)
	    ((unsigned long)(prMsduInfo->prPacket) + MAC_TX_RESERVED_FIELD);

	prTxFrame->u2FrameCtrl = MAC_FRAME_ACTION;
	if (rsnCheckBipKeyInstalled(prAdapter, prBssInfo->prStaRecOfAP))
		prTxFrame->u2FrameCtrl |= MASK_FC_PROTECTED_FRAME;
	COPY_MAC_ADDR(prTxFrame->aucDestAddr, prBssInfo->aucBSSID);
	COPY_MAC_ADDR(prTxFrame->aucSrcAddr, prBssInfo->aucOwnMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucBSSID, prBssInfo->aucBSSID);

	prTxFrame->ucCategory = CATEGORY_SA_QUERY_ACTION;
	prTxFrame->ucAction = ACTION_SA_QUERY_RESPONSE;

	kalMemCopy(prTxFrame->ucTransId, prRxFrame->ucTransId,
		   ACTION_SA_QUERY_TR_ID_LEN);

	u2PayloadLen = 2 + ACTION_SA_QUERY_TR_ID_LEN;

	/* 4 <3> Update information of MSDU_INFO_T */
	TX_SET_MMPDU(prAdapter,
		     prMsduInfo,
		     prBssInfo->prStaRecOfAP->ucBssIndex,
		     prBssInfo->prStaRecOfAP->ucIndex,
		     WLAN_MAC_MGMT_HEADER_LEN,
		     WLAN_MAC_MGMT_HEADER_LEN + u2PayloadLen, NULL,
		     MSDU_RATE_MODE_AUTO);

	if (rsnCheckBipKeyInstalled(prAdapter, prBssInfo->prStaRecOfAP)) {
		DBGLOG(RSN, INFO, "Set MSDU_OPT_PROTECTED_FRAME\n");
		nicTxConfigPktOption(prMsduInfo, MSDU_OPT_PROTECTED_FRAME,
				     TRUE);
	}
#if 0
	/* 4 Update information of MSDU_INFO_T */
	/* Management frame */
	prMsduInfo->ucPacketType = HIF_TX_PACKET_TYPE_MGMT;
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
void rsnSaQueryAction(IN struct ADAPTER *prAdapter, IN struct SW_RFB *prSwRfb)
{
	struct AIS_SPECIFIC_BSS_INFO *prBssSpecInfo;
	struct ACTION_SA_QUERY_FRAME *prRxFrame;
	struct STA_RECORD *prStaRec;
	uint32_t i;

	prBssSpecInfo = &prAdapter->rWifiVar.rAisSpecificBssInfo;
	ASSERT(prBssSpecInfo);

	prRxFrame = (struct ACTION_SA_QUERY_FRAME *)
	    prSwRfb->pvHeader;
	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);

	if (prSwRfb->u2PacketLen < ACTION_SA_QUERY_TR_ID_LEN) {
		DBGLOG(RSN, INFO,
		       "IEEE 802.11: Too short SA Query Action frame (len=%lu)\n",
		       (unsigned long)prSwRfb->u2PacketLen);
		return;
	}

	if (prRxFrame->ucAction == ACTION_SA_QUERY_REQUEST) {
		rsnSaQueryRequest(prAdapter, prSwRfb);
		return;
	}

	if (prRxFrame->ucAction != ACTION_SA_QUERY_RESPONSE) {
		DBGLOG(RSN, INFO,
		       "IEEE 802.11: Unexpected SA Query Action %d\n",
		       prRxFrame->ucAction);
		return;
	}

	DBGLOG(RSN, INFO,
	       "IEEE 802.11: Received SA Query Response from " MACSTR "\n",
	       MAC2STR(prStaRec->aucMacAddr));

	DBGLOG_MEM8(RSN, INFO, prRxFrame->ucTransId, ACTION_SA_QUERY_TR_ID_LEN);

	/* MLME-SAQuery.confirm */

	for (i = 0; i < prBssSpecInfo->u4SaQueryCount; i++) {
		if (kalMemCmp(prBssSpecInfo->pucSaQueryTransId +
			      i * ACTION_SA_QUERY_TR_ID_LEN,
			      prRxFrame->ucTransId,
			      ACTION_SA_QUERY_TR_ID_LEN) == 0)
			break;
	}

	if (i >= prBssSpecInfo->u4SaQueryCount) {
		DBGLOG(RSN, INFO,
		       "IEEE 802.11: No matching SA Query transaction identifier found\n");
		return;
	}

	DBGLOG(RSN, INFO, "Reply to pending SA Query received\n");

	rsnStopSaQuery(prAdapter);
}
#endif

static u_int8_t rsnCheckWpaRsnInfo(struct BSS_INFO *prBss,
				   struct RSN_INFO *prWpaRsnInfo)
{
	uint32_t i = 0;

	if (prWpaRsnInfo->u4GroupKeyCipherSuite !=
	    prBss->u4RsnSelectedGroupCipher) {
		DBGLOG(RSN, INFO,
		       "GroupCipherSuite change, old=0x%04x, new=0x%04x\n",
		       prBss->u4RsnSelectedGroupCipher,
		       prWpaRsnInfo->u4GroupKeyCipherSuite);
		return TRUE;
	}
	for (; i < prWpaRsnInfo->u4AuthKeyMgtSuiteCount; i++)
		if (prBss->u4RsnSelectedAKMSuite ==
		    prWpaRsnInfo->au4AuthKeyMgtSuite[i])
			break;
	if (i == prWpaRsnInfo->u4AuthKeyMgtSuiteCount) {
		DBGLOG(RSN, INFO,
		       "KeyMgmt change, not find 0x%04x in new beacon\n",
		       prBss->u4RsnSelectedAKMSuite);
		return TRUE;
	}

	for (i = 0; i < prWpaRsnInfo->u4PairwiseKeyCipherSuiteCount; i++)
		if (prBss->u4RsnSelectedPairwiseCipher ==
		    prWpaRsnInfo->au4PairwiseKeyCipherSuite[i])
			break;
	if (i == prWpaRsnInfo->u4PairwiseKeyCipherSuiteCount) {
		DBGLOG(RSN, INFO,
		       "Pairwise Cipher change, not find 0x%04x in new beacon\n",
		       prBss->u4RsnSelectedPairwiseCipher);
		return TRUE;
	}

	return FALSE;
}

u_int8_t rsnCheckSecurityModeChanged(
			struct ADAPTER *prAdapter,
			struct BSS_INFO *prBssInfo,
			struct BSS_DESC *prBssDesc)
{
	enum ENUM_PARAM_AUTH_MODE eAuthMode =
	    prAdapter->rWifiVar.rConnSettings.eAuthMode;

	switch (eAuthMode) {
	case AUTH_MODE_OPEN: /* original is open system */
	case AUTH_MODE_NON_RSN_FT:
		if ((prBssDesc->u2CapInfo & CAP_INFO_PRIVACY) &&
		    !prAdapter->prGlueInfo->rWpaInfo.fgPrivacyInvoke &&
		    !secIsWepBss(prAdapter, prBssInfo)) {
			DBGLOG(RSN, INFO, "security change, open->privacy\n");
			return TRUE;
		}
		break;
	case AUTH_MODE_SHARED:	/* original is WEP */
	case AUTH_MODE_AUTO_SWITCH:
		if ((prBssDesc->u2CapInfo & CAP_INFO_PRIVACY) == 0) {
			DBGLOG(RSN, INFO, "security change, WEP->open\n");
			return TRUE;
		} else if (prBssDesc->fgIERSN || prBssDesc->fgIEWPA) {
			DBGLOG(RSN, INFO, "security change, WEP->WPA/WPA2\n");
			return TRUE;
		}
		break;
	case AUTH_MODE_WPA:	/*original is WPA */
	case AUTH_MODE_WPA_PSK:
	case AUTH_MODE_WPA_NONE:
		if (prBssDesc->fgIEWPA)
			return rsnCheckWpaRsnInfo(prBssInfo,
				&prBssDesc->rWPAInfo);
		DBGLOG(RSN, INFO, "security change, WPA->%s\n",
		       prBssDesc->fgIERSN ? "WPA2" :
		       (prBssDesc->u2CapInfo & CAP_INFO_PRIVACY ?
				"WEP" : "OPEN"));
		return TRUE;
		break;
	case AUTH_MODE_WPA2:	/*original is WPA2 */
	case AUTH_MODE_WPA2_PSK:
	case AUTH_MODE_WPA2_FT:
	case AUTH_MODE_WPA2_FT_PSK:
#if CFG_SUPPORT_CFG80211_AUTH
	case AUTH_MODE_WPA2_SAE:
#endif
		if (prBssDesc->fgIERSN)
			return rsnCheckWpaRsnInfo(prBssInfo,
				&prBssDesc->rRSNInfo);
		DBGLOG(RSN, INFO, "security change, WPA2->%s\n",
		       prBssDesc->fgIEWPA ? "WPA" :
		       (prBssDesc->u2CapInfo & CAP_INFO_PRIVACY ?
				"WEP" : "OPEN"));
		return TRUE;
		break;
	default:
		DBGLOG(RSN, WARN, "unknowned eAuthMode=%d\n", eAuthMode);
		break;
	}
	return FALSE;
}

#if CFG_SUPPORT_AAA
#define WPS_DEV_OUI_WFA                 0x0050f204
#define ATTR_RESPONSE_TYPE              0x103b

#define ATTR_VERSION                    0x104a
#define ATTR_VENDOR_EXT                 0x1049
#define WPS_VENDOR_ID_WFA               14122

void rsnGenerateWSCIEForAssocRsp(struct ADAPTER *prAdapter,
				 struct MSDU_INFO *prMsduInfo)
{
	struct WIFI_VAR *prWifiVar = NULL;
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *)NULL;
	uint16_t u2IELen = 0;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);
	ASSERT(IS_NET_ACTIVE(prAdapter, prMsduInfo->ucBssIndex));

	prWifiVar = &(prAdapter->rWifiVar);
	ASSERT(prWifiVar);

	DBGLOG(RSN, TRACE, "WPS: Building WPS IE for (Re)Association Response");
	prP2pBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prMsduInfo->ucBssIndex);

	if (prP2pBssInfo->eNetworkType != NETWORK_TYPE_P2P)
		return;

	u2IELen = kalP2PCalWSC_IELen(prAdapter->prGlueInfo, 3,
				     (uint8_t) prP2pBssInfo->u4PrivateData);

	kalP2PGenWSC_IE(prAdapter->prGlueInfo,
			3,
			(uint8_t *) ((unsigned long) prMsduInfo->prPacket +
				  (unsigned long) prMsduInfo->u2FrameLength),
			(uint8_t) prP2pBssInfo->u4PrivateData);
	prMsduInfo->u2FrameLength += (uint16_t) kalP2PCalWSC_IELen(
					prAdapter->prGlueInfo, 3,
					(uint8_t) prP2pBssInfo->u4PrivateData);
}

#endif

#if CFG_SUPPORT_802_11W
/* AP PMF */
/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to validate setting if PMF connection capable
 *    or not. If AP MFPC=1, and STA MFPC=1, we let this as PMF connection
 *
 *
 * \return status code
 */
/*----------------------------------------------------------------------------*/
uint16_t rsnPmfCapableValidation(IN struct ADAPTER
				 *prAdapter, IN struct BSS_INFO *prBssInfo,
				 IN struct STA_RECORD *prStaRec)
{
	u_int8_t selfMfpc, selfMfpr, peerMfpc, peerMfpr;

	selfMfpc = prBssInfo->rApPmfCfg.fgMfpc;
	selfMfpr = prBssInfo->rApPmfCfg.fgMfpr;
	peerMfpc = prStaRec->rPmfCfg.fgMfpc;
	peerMfpr = prStaRec->rPmfCfg.fgMfpr;

	DBGLOG(RSN, INFO,
	       "AP mfpc:%d, mfpr:%d / STA mfpc:%d, mfpr:%d\n",
	       selfMfpc, selfMfpr, peerMfpc, peerMfpr);

	if ((selfMfpc == TRUE) && (peerMfpc == FALSE)) {
		if ((selfMfpr == TRUE) && (peerMfpr == FALSE)) {
			DBGLOG(RSN, ERROR,
				"PMF policy violation for case 4\n");
			return STATUS_CODE_ROBUST_MGMT_FRAME_POLICY_VIOLATION;
		}

		if (peerMfpr == TRUE) {
			DBGLOG(RSN, ERROR,
				"PMF policy violation for case 7\n");
			return STATUS_CODE_ROBUST_MGMT_FRAME_POLICY_VIOLATION;
		}
	}

	if ((selfMfpc == TRUE) && (peerMfpc == TRUE)) {
		DBGLOG(RSN, ERROR, "PMF Connection\n");
		prStaRec->rPmfCfg.fgApplyPmf = TRUE;
	}

	return STATUS_CODE_SUCCESSFUL;

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to generate TIMEOUT INTERVAL IE
 *     for association resp.
 *     Add Timeout interval IE (56) when PMF invalid association.
 *
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
void rsnPmfGenerateTimeoutIE(struct ADAPTER *prAdapter,
			     struct MSDU_INFO *prMsduInfo)
{
	struct IE_TIMEOUT_INTERVAL *prTimeout;
	struct STA_RECORD *prStaRec = NULL;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

	if (!prStaRec)
		return;

	prTimeout = (struct IE_TIMEOUT_INTERVAL *)
	    (((uint8_t *) prMsduInfo->prPacket) + prMsduInfo->u2FrameLength);

	/* only when PMF connection, and association error code is 30 */
	if ((rsnCheckBipKeyInstalled(prAdapter, prStaRec) == TRUE)
	    &&
	    (prStaRec->u2StatusCode ==
	     STATUS_CODE_ASSOC_REJECTED_TEMPORARILY)) {

		DBGLOG(RSN, INFO, "rsnPmfGenerateTimeoutIE TRUE\n");
		prTimeout->ucId = ELEM_ID_TIMEOUT_INTERVAL;
		prTimeout->ucLength = ELEM_MAX_LEN_TIMEOUT_IE;
		prTimeout->ucType = IE_TIMEOUT_INTERVAL_TYPE_ASSOC_COMEBACK;
		prTimeout->u4Value = 1 << 10;
		prMsduInfo->u2FrameLength += IE_SIZE(prTimeout);
	}
}

/*----------------------------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to check the Sa query timeout.
 * check if total retry time is greater than 1000ms
 *
 * \retval 1: retry max timeout. 0: not timeout
 * \note
 *      Called by: AAA module, Handle by Sa Query timeout
 */
/*----------------------------------------------------------------------------*/
uint8_t rsnApCheckSaQueryTimeout(IN struct ADAPTER
				 *prAdapter, IN struct STA_RECORD *prStaRec)
{
	struct BSS_INFO *prBssInfo;
	uint32_t now;

	GET_CURRENT_SYSTIME(&now);

	if (CHECK_FOR_TIMEOUT(now, prStaRec->rPmfCfg.u4SAQueryStart,
			      TU_TO_MSEC(1000))) {
		DBGLOG(RSN, INFO, "association SA Query timed out\n");

		/* XXX PMF TODO how to report STA REC disconnect?? */
		/* when SAQ retry count timeout, clear this STA */
		prStaRec->rPmfCfg.ucSAQueryTimedOut = 1;
		prStaRec->rPmfCfg.u2TransactionID = 0;
		prStaRec->rPmfCfg.u4SAQueryCount = 0;
		cnmTimerStopTimer(prAdapter, &prStaRec->rPmfCfg.rSAQueryTimer);

		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
						  prStaRec->ucBssIndex);

		/* refer to p2pRoleFsmRunEventRxDeauthentication */
		if (prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT) {
			if (bssRemoveClient(prAdapter, prBssInfo, prStaRec)) {
				/* Indicate disconnect to Host. */
				p2pFuncDisconnect(prAdapter, prBssInfo,
					prStaRec, FALSE, 0);
				/* Deactive BSS if PWR is IDLE and no peer */
				if (IS_NET_PWR_STATE_IDLE(prAdapter,
					prBssInfo->ucBssIndex)
				    &&
				    (bssGetClientCount(prAdapter, prBssInfo)
					== 0)) {
					/* All Peer disconnected !!
					 * Stop BSS now!!
					 */
					p2pFuncStopComplete(prAdapter,
						prBssInfo);
				}
			}
		}

		return 1;
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to start the 802.11w sa query timer.
 *    This routine is triggered every 201ms, and every time enter function,
 *    check max timeout
 *
 * \note
 *      Called by: AAA module, Handle TX SAQ request
 */
/*----------------------------------------------------------------------------*/
void rsnApStartSaQueryTimer(IN struct ADAPTER *prAdapter,
			    IN struct STA_RECORD *prStaRec,
			    IN unsigned long ulParamPtr)
{
	struct BSS_INFO *prBssInfo;
	struct MSDU_INFO *prMsduInfo;
	struct ACTION_SA_QUERY_FRAME *prTxFrame;
	uint16_t u2PayloadLen;

	ASSERT(prStaRec);

	DBGLOG(RSN, INFO, "MFP: AP Start Sa Query timer\n");

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);

	if (prStaRec->rPmfCfg.u4SAQueryCount > 0
	    && rsnApCheckSaQueryTimeout(prAdapter, prStaRec)) {
		DBGLOG(RSN, INFO,
		       "MFP: retry max timeout, u4SaQueryCount count =%d\n",
		       prStaRec->rPmfCfg.u4SAQueryCount);
		return;
	}

	prMsduInfo = (struct MSDU_INFO *)cnmMgtPktAlloc(prAdapter,
							MAC_TX_RESERVED_FIELD +
							PUBLIC_ACTION_MAX_LEN);

	if (!prMsduInfo)
		return;

	prTxFrame = (struct ACTION_SA_QUERY_FRAME *)
	    ((unsigned long)(prMsduInfo->prPacket) + MAC_TX_RESERVED_FIELD);

	prTxFrame->u2FrameCtrl = MAC_FRAME_ACTION;
	if (rsnCheckBipKeyInstalled(prAdapter, prStaRec))
		prTxFrame->u2FrameCtrl |= MASK_FC_PROTECTED_FRAME;
	COPY_MAC_ADDR(prTxFrame->aucDestAddr, prStaRec->aucMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucSrcAddr, prBssInfo->aucBSSID);
	COPY_MAC_ADDR(prTxFrame->aucBSSID, prBssInfo->aucBSSID);

	prTxFrame->ucCategory = CATEGORY_SA_QUERY_ACTION;
	prTxFrame->ucAction = ACTION_SA_QUERY_REQUEST;

	if (prStaRec->rPmfCfg.u4SAQueryCount == 0)
		GET_CURRENT_SYSTIME(&prStaRec->rPmfCfg.u4SAQueryStart);

	/* if retry, transcation id ++ */
	if (prStaRec->rPmfCfg.u4SAQueryCount) {
		prStaRec->rPmfCfg.u2TransactionID++;
	} else {
		/* if first SAQ request, random pick transaction id */
		prStaRec->rPmfCfg.u2TransactionID =
		    (uint16_t) (kalRandomNumber() & 0xFFFF);
	}

	DBGLOG(RSN, INFO, "SAQ transaction id:%d\n",
	       prStaRec->rPmfCfg.u2TransactionID);

	/* trnsform U16 to U8 array */
	prTxFrame->ucTransId[0] = ((prStaRec->rPmfCfg.u2TransactionID
				& 0xff00) >> 8);
	prTxFrame->ucTransId[1] = ((prStaRec->rPmfCfg.u2TransactionID
				& 0x00ff) >> 0);

	prStaRec->rPmfCfg.u4SAQueryCount++;

	u2PayloadLen = 2 + ACTION_SA_QUERY_TR_ID_LEN;

	/* 4 <3> Update information of MSDU_INFO_T */
	TX_SET_MMPDU(prAdapter,
		     prMsduInfo,
		     prStaRec->ucBssIndex,
		     prStaRec->ucIndex,
		     WLAN_MAC_MGMT_HEADER_LEN,
		     WLAN_MAC_MGMT_HEADER_LEN + u2PayloadLen, NULL,
		     MSDU_RATE_MODE_AUTO);

	if (rsnCheckBipKeyInstalled(prAdapter, prStaRec)) {
		DBGLOG(RSN, INFO, "SAQ Set MSDU_OPT_PROTECTED_FRAME\n");
		nicTxConfigPktOption(prMsduInfo, MSDU_OPT_PROTECTED_FRAME,
				     TRUE);
	}
	/* 4 Enqueue the frame to send this action frame. */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);

	DBGLOG(RSN, INFO, "AP Set SA Query timer %d (%d Tu)\n",
	       prStaRec->rPmfCfg.u4SAQueryCount, 201);

	cnmTimerStartTimer(prAdapter,
			   &prStaRec->rPmfCfg.rSAQueryTimer, TU_TO_MSEC(201));

}

/*----------------------------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to start the 802.11w TX SA query.
 *
 *
 * \note
 *      Called by: AAA module, Handle Tx action frame request
 */
/*----------------------------------------------------------------------------*/
void rsnApStartSaQuery(IN struct ADAPTER *prAdapter,
		       IN struct STA_RECORD *prStaRec)
{
	if (!prStaRec)
		return;

	DBGLOG(RSN, INFO, "rsnApStartSaQuery\n");

	cnmTimerStopTimer(prAdapter,
			  &prStaRec->rPmfCfg.rSAQueryTimer);
	cnmTimerInitTimer(prAdapter,
			  &prStaRec->rPmfCfg.rSAQueryTimer,
			(PFN_MGMT_TIMEOUT_FUNC)rsnApStartSaQueryTimer,
				 (unsigned long) prStaRec);

	if (prStaRec->rPmfCfg.u4SAQueryCount == 0)
		rsnApStartSaQueryTimer(prAdapter, prStaRec,
				       (unsigned long)NULL);
}

/*----------------------------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to stop the 802.11w SA query.
 *
 *
 * \note
 *      Called by: AAA module, stop TX SAQ if receive correct SAQ response
 */
/*----------------------------------------------------------------------------*/
void rsnApStopSaQuery(IN struct ADAPTER *prAdapter,
		      IN struct STA_RECORD *prStaRec)
{
	ASSERT(prStaRec);

	cnmTimerStopTimer(prAdapter, &prStaRec->rPmfCfg.rSAQueryTimer);
	prStaRec->rPmfCfg.u2TransactionID = 0;
	prStaRec->rPmfCfg.u4SAQueryCount = 0;
	prStaRec->rPmfCfg.ucSAQueryTimedOut = 0;
}

/*----------------------------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to process the 802.11w sa query action frame.
 *
 *
 * \note
 *      Called by: AAA module, Handle Rx action request
 */
/*----------------------------------------------------------------------------*/
void rsnApSaQueryRequest(IN struct ADAPTER *prAdapter,
			 IN struct SW_RFB *prSwRfb)
{
	struct BSS_INFO *prBssInfo;
	struct MSDU_INFO *prMsduInfo;
	struct ACTION_SA_QUERY_FRAME *prRxFrame = NULL;
	uint16_t u2PayloadLen;
	struct STA_RECORD *prStaRec;
	struct ACTION_SA_QUERY_FRAME *prTxFrame;

	if (!prSwRfb)
		return;

	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);
	if (!prStaRec)		/* Todo:: for not AIS check */
		return;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);
	ASSERT(prBssInfo);

	prRxFrame = (struct ACTION_SA_QUERY_FRAME *)
	    prSwRfb->pvHeader;
	if (!prRxFrame)
		return;

	DBGLOG(RSN, INFO,
	       "IEEE 802.11: AP Received SA Query Request from " MACSTR
	       "\n", MAC2STR(prStaRec->aucMacAddr));

	DBGLOG_MEM8(RSN, INFO, prRxFrame->ucTransId, ACTION_SA_QUERY_TR_ID_LEN);

	if (!rsnCheckBipKeyInstalled(prAdapter, prStaRec)) {
		DBGLOG(RSN, INFO,
		       "IEEE 802.11: AP Ignore SA Query Request non-PMF STA "
		       MACSTR "\n", MAC2STR(prStaRec->aucMacAddr));
		return;
	}

	DBGLOG(RSN, INFO,
	       "IEEE 802.11: Sending SA Query Response to " MACSTR "\n",
	       MAC2STR(prStaRec->aucMacAddr));

	prMsduInfo = (struct MSDU_INFO *)cnmMgtPktAlloc(prAdapter,
							MAC_TX_RESERVED_FIELD +
							PUBLIC_ACTION_MAX_LEN);

	if (!prMsduInfo)
		return;

	/* drop cipher mismatch */
	if (rsnCheckBipKeyInstalled(prAdapter, prStaRec)) {
		if (HAL_RX_STATUS_IS_CIPHER_MISMATCH(prSwRfb->prRxStatus) ||
		    HAL_RX_STATUS_IS_CLM_ERROR(prSwRfb->prRxStatus)) {
			/* if cipher mismatch, or incorrect encrypt,
			 * just drop
			 */
			DBGLOG(RSN, ERROR, "drop SAQ req CM/CLM=1\n");
			return;
		}
	}

	prTxFrame = (struct ACTION_SA_QUERY_FRAME *)
	    ((unsigned long)(prMsduInfo->prPacket) + MAC_TX_RESERVED_FIELD);

	prTxFrame->u2FrameCtrl = MAC_FRAME_ACTION;
	if (rsnCheckBipKeyInstalled(prAdapter, prStaRec)) {
		prTxFrame->u2FrameCtrl |= MASK_FC_PROTECTED_FRAME;
		DBGLOG(RSN, INFO, "AP SAQ resp set FC PF bit\n");
	}
	COPY_MAC_ADDR(prTxFrame->aucDestAddr, prStaRec->aucMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucSrcAddr, prBssInfo->aucBSSID);
	COPY_MAC_ADDR(prTxFrame->aucBSSID, prBssInfo->aucBSSID);

	prTxFrame->ucCategory = CATEGORY_SA_QUERY_ACTION;
	prTxFrame->ucAction = ACTION_SA_QUERY_RESPONSE;

	kalMemCopy(prTxFrame->ucTransId, prRxFrame->ucTransId,
		   ACTION_SA_QUERY_TR_ID_LEN);

	u2PayloadLen = 2 + ACTION_SA_QUERY_TR_ID_LEN;

	/* 4 <3> Update information of MSDU_INFO_T */
	TX_SET_MMPDU(prAdapter,
		     prMsduInfo,
		     prStaRec->ucBssIndex,
		     prStaRec->ucIndex,
		     WLAN_MAC_MGMT_HEADER_LEN,
		     WLAN_MAC_MGMT_HEADER_LEN + u2PayloadLen, NULL,
		     MSDU_RATE_MODE_AUTO);

	if (rsnCheckBipKeyInstalled(prAdapter, prStaRec)) {
		DBGLOG(RSN, INFO, "AP SAQ resp set MSDU_OPT_PROTECTED_FRAME\n");
		nicTxConfigPktOption(prMsduInfo, MSDU_OPT_PROTECTED_FRAME,
				     TRUE);
	}

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
 *      Called by: AAA module, Handle Rx action request
 */
/*----------------------------------------------------------------------------*/
void rsnApSaQueryAction(IN struct ADAPTER *prAdapter, IN struct SW_RFB *prSwRfb)
{
	struct ACTION_SA_QUERY_FRAME *prRxFrame;
	struct STA_RECORD *prStaRec;
	uint16_t u2SwapTrID;

	prRxFrame = (struct ACTION_SA_QUERY_FRAME *)
	    prSwRfb->pvHeader;
	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);

	if (prStaRec == NULL) {
		DBGLOG(RSN, INFO, "rsnApSaQueryAction: prStaRec is NULL");
		return;
	}

	DBGLOG(RSN, TRACE,
	       "AP PMF SAQ action enter from " MACSTR "\n",
	       MAC2STR(prStaRec->aucMacAddr));
	if (prSwRfb->u2PacketLen < ACTION_SA_QUERY_TR_ID_LEN) {
		DBGLOG(RSN, INFO,
		       "IEEE 802.11: Too short SA Query Action frame (len=%lu)\n",
		       (unsigned long)prSwRfb->u2PacketLen);
		return;
	}

	if (prRxFrame->ucAction == ACTION_SA_QUERY_REQUEST) {
		rsnApSaQueryRequest(prAdapter, prSwRfb);
		return;
	}

	if (prRxFrame->ucAction != ACTION_SA_QUERY_RESPONSE) {
		DBGLOG(RSN, INFO,
		       "IEEE 802.11: Unexpected SA Query Action %d\n",
		       prRxFrame->ucAction);
		return;
	}

	DBGLOG(RSN, INFO,
	       "IEEE 802.11: Received SA Query Response from " MACSTR "\n",
	       MAC2STR(prStaRec->aucMacAddr));

	DBGLOG_MEM8(RSN, INFO, prRxFrame->ucTransId, ACTION_SA_QUERY_TR_ID_LEN);

	/* MLME-SAQuery.confirm */
	/* transform to network byte order */
	u2SwapTrID = htons(prStaRec->rPmfCfg.u2TransactionID);
	if (kalMemCmp((uint8_t *) &u2SwapTrID, prRxFrame->ucTransId,
		      ACTION_SA_QUERY_TR_ID_LEN) == 0) {
		DBGLOG(RSN, INFO, "AP Reply to SA Query received\n");
		rsnApStopSaQuery(prAdapter, prStaRec);
	} else {
		DBGLOG(RSN, INFO,
		       "IEEE 802.11: AP No matching SA Query transaction identifier found\n");
	}

}

#endif /* CFG_SUPPORT_802_11W */

#if CFG_SUPPORT_PASSPOINT
u_int8_t rsnParseOsenIE(struct ADAPTER *prAdapter,
			struct IE_WFA_OSEN *prInfoElem,
			struct RSN_INFO *prOsenInfo)
{
	uint32_t i;
	int32_t u4RemainRsnIeLen;
	uint16_t u2Version = 0;
	uint16_t u2Cap = 0;
	uint32_t u4GroupSuite = RSN_CIPHER_SUITE_CCMP;
	uint16_t u2PairSuiteCount = 0;
	uint16_t u2AuthSuiteCount = 0;
	uint8_t *pucPairSuite = NULL;
	uint8_t *pucAuthSuite = NULL;
	uint8_t *cp;

	ASSERT(prOsenInfo);

	cp = ((uint8_t *) prInfoElem) + 6;
	u4RemainRsnIeLen = (int32_t) prInfoElem->ucLength - 4;
	do {
		if (u4RemainRsnIeLen == 0)
			break;

		/* Parse the Group Key Cipher Suite field. */
		if (u4RemainRsnIeLen < 4) {
			DBGLOG(RSN, WARN,
			       "Fail to parse RSN IE in group cipher suite (IE len: %d)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}

		WLAN_GET_FIELD_32(cp, &u4GroupSuite);
		cp += 4;
		u4RemainRsnIeLen -= 4;

		if (u4RemainRsnIeLen == 0)
			break;

		/* Parse the Pairwise Key Cipher Suite Count field. */
		if (u4RemainRsnIeLen < 2) {
			DBGLOG(RSN, WARN,
			       "Fail to parse RSN IE in pairwise cipher suite count (IE len: %d)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}

		WLAN_GET_FIELD_16(cp, &u2PairSuiteCount);
		cp += 2;
		u4RemainRsnIeLen -= 2;

		/* Parse the Pairwise Key Cipher Suite List field. */
		i = (uint32_t) u2PairSuiteCount * 4;
		if (u4RemainRsnIeLen < (int32_t) i) {
			DBGLOG(RSN, WARN,
			       "Fail to parse RSN IE in pairwise cipher suite list (IE len: %d, Remain %u, Cnt %d GS %x)\n",
			       prInfoElem->ucLength, u4RemainRsnIeLen,
			       u2PairSuiteCount, u4GroupSuite);
			return FALSE;
		}

		pucPairSuite = cp;

		cp += i;
		u4RemainRsnIeLen -= (int32_t) i;

		if (u4RemainRsnIeLen == 0)
			break;

		/* Parse the Authentication and Key Management Cipher Suite
		 * Count field.
		 */
		if (u4RemainRsnIeLen < 2) {
			DBGLOG(RSN, WARN,
			       "Fail to parse RSN IE in auth & key mgt suite count (IE len: %d)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}

		WLAN_GET_FIELD_16(cp, &u2AuthSuiteCount);
		cp += 2;
		u4RemainRsnIeLen -= 2;

		/* Parse the Authentication and Key Management Cipher Suite
		 * List field.
		 */
		i = (uint32_t) u2AuthSuiteCount * 4;
		if (u4RemainRsnIeLen < (int32_t) i) {
			DBGLOG(RSN, WARN,
			       "Fail to parse RSN IE in auth & key mgt suite list (IE len: %d)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}

		pucAuthSuite = cp;

		cp += i;
		u4RemainRsnIeLen -= (int32_t) i;

		if (u4RemainRsnIeLen == 0)
			break;

		/* Parse the RSN u2Capabilities field. */
		if (u4RemainRsnIeLen < 2) {
			DBGLOG(RSN, WARN,
			       "Fail to parse RSN IE in RSN capabilities (IE len: %d)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}

		WLAN_GET_FIELD_16(cp, &u2Cap);
	} while (FALSE);

	/* Save the RSN information for the BSS. */
	prOsenInfo->ucElemId = ELEM_ID_VENDOR;

	prOsenInfo->u2Version = 0;

	prOsenInfo->u4GroupKeyCipherSuite = u4GroupSuite;

	DBGLOG(RSN, TRACE,
	       "RSN: version %d, group key cipher suite %02x-%02x-%02x-%02x\n",
	       u2Version, (uint8_t) (u4GroupSuite & 0x000000FF),
	       (uint8_t) ((u4GroupSuite >> 8) & 0x000000FF),
	       (uint8_t) ((u4GroupSuite >> 16) & 0x000000FF),
	       (uint8_t) ((u4GroupSuite >> 24) & 0x000000FF));

	if (pucPairSuite) {
		/* The information about the pairwise key cipher suites
		 * is present.
		 */
		if (u2PairSuiteCount > MAX_NUM_SUPPORTED_CIPHER_SUITES)
			u2PairSuiteCount = MAX_NUM_SUPPORTED_CIPHER_SUITES;

		prOsenInfo->u4PairwiseKeyCipherSuiteCount =
		    (uint32_t) u2PairSuiteCount;

		for (i = 0; i < (uint32_t) u2PairSuiteCount; i++) {
			WLAN_GET_FIELD_32(pucPairSuite,
				&prOsenInfo->au4PairwiseKeyCipherSuite[i]);
			pucPairSuite += 4;

			DBGLOG(RSN, TRACE,
			       "RSN: pairwise key cipher suite [%d]: %02x-%02x-%02x-%02x\n",
			       (uint8_t) i, (uint8_t) (
				       prOsenInfo->au4PairwiseKeyCipherSuite[i]
						& 0x000000FF),
			       (uint8_t)
			       ((prOsenInfo->au4PairwiseKeyCipherSuite[i] >> 8)
						& 0x000000FF),
			       (uint8_t)
			       ((prOsenInfo->au4PairwiseKeyCipherSuite[i] >> 16)
						& 0x000000FF),
			       (uint8_t)
			       ((prOsenInfo->au4PairwiseKeyCipherSuite[i] >> 24)
						& 0x000000FF));
		}
	} else {
		/* The information about the pairwise key cipher suites
		 * is not present. Use the default chipher suite for RSN: CCMP
		 */

		prOsenInfo->u4PairwiseKeyCipherSuiteCount = 1;
		prOsenInfo->au4PairwiseKeyCipherSuite[0] =
		    RSN_CIPHER_SUITE_CCMP;

		DBGLOG(RSN, WARN,
		       "No Pairwise Cipher Suite found, using default (CCMP)\n");
	}

	if (pucAuthSuite) {
		/* The information about the authentication
		 * and key management suites is present.
		 */

		if (u2AuthSuiteCount > MAX_NUM_SUPPORTED_AKM_SUITES)
			u2AuthSuiteCount = MAX_NUM_SUPPORTED_AKM_SUITES;

		prOsenInfo->u4AuthKeyMgtSuiteCount = (uint32_t)
		    u2AuthSuiteCount;

		for (i = 0; i < (uint32_t) u2AuthSuiteCount; i++) {
			WLAN_GET_FIELD_32(pucAuthSuite,
					  &prOsenInfo->au4AuthKeyMgtSuite[i]);
			pucAuthSuite += 4;

			DBGLOG(RSN, TRACE,
			       "RSN: AKM suite [%d]: %02x-%02x-%02x-%02x\n",
			       (uint8_t) i,
			       (uint8_t) (prOsenInfo->au4AuthKeyMgtSuite[i]
					& 0x000000FF),
			       (uint8_t)
			       ((prOsenInfo->au4AuthKeyMgtSuite[i] >> 8)
					& 0x000000FF),
			       (uint8_t)
			       ((prOsenInfo->au4AuthKeyMgtSuite[i] >> 16)
					& 0x000000FF),
			       (uint8_t)
			       ((prOsenInfo->au4AuthKeyMgtSuite[i] >> 24)
					& 0x000000FF));
		}
	} else {
		/* The information about the authentication and
		 * key management suites is not present.
		 * Use the default AKM suite for RSN.
		 */
		prOsenInfo->u4AuthKeyMgtSuiteCount = 1;
		prOsenInfo->au4AuthKeyMgtSuite[0] = RSN_AKM_SUITE_802_1X;

		DBGLOG(RSN, WARN, "No AKM found, using default (802.1X)\n");
	}

	prOsenInfo->u2RsnCap = u2Cap;
#if CFG_SUPPORT_802_11W
	prOsenInfo->fgRsnCapPresent = TRUE;
#endif
	DBGLOG(RSN, TRACE, "RSN cap: 0x%04x\n", prOsenInfo->u2RsnCap);

	return TRUE;
}
#endif /* CFG_SUPPORT_PASSPOINT */

uint32_t rsnCalculateFTIELen(struct ADAPTER *prAdapter, uint8_t ucBssIdx,
			     struct STA_RECORD *prStaRec)
{
	enum ENUM_PARAM_AUTH_MODE eAuthMode =
		prAdapter->rWifiVar.rConnSettings.eAuthMode;
	struct FT_IES *prFtIEs = &prAdapter->prGlueInfo->rFtIeForTx;

	if (!IS_BSS_INDEX_VALID(ucBssIdx) ||
	    !IS_BSS_AIS(GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIdx)) ||
	    !prFtIEs->prFTIE || (eAuthMode != AUTH_MODE_WPA2_FT &&
				 eAuthMode != AUTH_MODE_WPA2_FT_PSK))
		return 0;
	return IE_SIZE(prFtIEs->prFTIE);
}

void rsnGenerateFTIE(IN struct ADAPTER *prAdapter,
		     IN OUT struct MSDU_INFO *prMsduInfo)
{
	enum ENUM_PARAM_AUTH_MODE eAuthMode =
		prAdapter->rWifiVar.rConnSettings.eAuthMode;
	struct FT_IES *prFtIEs = &prAdapter->prGlueInfo->rFtIeForTx;
	uint8_t *pucBuffer =
		(uint8_t *)prMsduInfo->prPacket + prMsduInfo->u2FrameLength;
	uint32_t ucFtIeSize = 0;
	uint8_t ucBssIdx = prMsduInfo->ucBssIndex;

	if (!IS_BSS_INDEX_VALID(ucBssIdx) ||
	    !IS_BSS_AIS(GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIdx)) ||
	    !prFtIEs->prFTIE || (eAuthMode != AUTH_MODE_WPA2_FT &&
				 eAuthMode != AUTH_MODE_WPA2_FT_PSK))
		return;
	ucFtIeSize = IE_SIZE(prFtIEs->prFTIE);
	prMsduInfo->u2FrameLength += ucFtIeSize;
	kalMemCopy(pucBuffer, prFtIEs->prFTIE, ucFtIeSize);
}

#if CFG_SUPPORT_OWE
/*----------------------------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to generate OWE IE for
 *        associate request frame.
 *
 * \param[in]  prAdapter	The Selected BSS description
 * \param[in]  prMsduInfo	MSDU packet buffer
 *
 * \retval N/A
 *
 * \note
 *      Called by: AIS module, Associate request
 */
/*----------------------------------------------------------------------------*/
void rsnGenerateOWEIE(IN struct ADAPTER *prAdapter,
		      IN OUT struct MSDU_INFO *prMsduInfo)
{
	uint8_t *pucBuffer = NULL;
	struct CONNECTION_SETTINGS *prConnSettings = NULL;
	uint8_t ucLength = 0;

	DBGLOG(RSN, INFO, "rsnGenerateOWEIE\n");
	prConnSettings = &prAdapter->rWifiVar.rConnSettings;

	if (prConnSettings->rOweInfo.ucLength == 0)
		return;
	ucLength = prConnSettings->rOweInfo.ucLength + 2;
	ASSERT(prMsduInfo);

	pucBuffer = (uint8_t *) ((unsigned long)
				 prMsduInfo->prPacket + (unsigned long)
				 prMsduInfo->u2FrameLength);
	ASSERT(pucBuffer);

	/* if (eNetworkId != NETWORK_TYPE_AIS_INDEX) */
	/* return; */
	kalMemCopy(pucBuffer, &(prAdapter->rWifiVar.rConnSettings.rOweInfo),
		   ucLength);
	prMsduInfo->u2FrameLength += IE_SIZE(pucBuffer);
	DBGLOG_MEM8(RSN, INFO, pucBuffer, IE_SIZE(pucBuffer));
}

/*----------------------------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to calculate OWE IE length for
 *        associate request frame.
 *
 * \param[in]  prAdapter	Major data structure for driver operation
 * \param[in]  ucBssIndex	unused for this function
 * \param[in]  prStaRec		unused for this function
 *
 * \retval The append WPA IE length
 *
 * \note
 *      Called by: AIS module, Associate request
 */
/*----------------------------------------------------------------------------*/
uint32_t rsnCalOweIELen(IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex, struct STA_RECORD *prStaRec)
{
	if (prAdapter->rWifiVar.rConnSettings.rOweInfo.ucLength != 0)
		return prAdapter->rWifiVar.rConnSettings.rOweInfo.ucLength + 2;
	else
		return 0;
}
#endif

