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
 * Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/
 *							include/mgmt/mib.h#1
 */

/*! \file  mib.h
 *  \brief This file contains the IEEE 802.11 family related MIB definition
 *         for MediaTek 802.11 Wireless LAN Adapters.
 */


#ifndef _MIB_H
#define _MIB_H

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */

/*******************************************************************************
 *                         D A T A   T Y P E S
 *******************************************************************************
 */
/* Entry in SMT AuthenticationAlgorithms Table:
 *	dot11AuthenticationAlgorithmsEntry
 */
struct DOT11_AUTHENTICATION_ALGORITHMS_ENTRY {
	/* dot11AuthenticationAlgorithmsEntry 3 */
	u_int8_t dot11AuthenticationAlgorithmsEnable;
};

/* Entry in SMT dot11RSNAConfigPairwiseCiphersTalbe Table:
 *	dot11RSNAConfigPairwiseCiphersEntry
 */
struct DOT11_RSNA_CONFIG_PAIRWISE_CIPHERS_ENTRY {
	/* dot11RSNAConfigPairwiseCiphersEntry 2 */
	uint32_t dot11RSNAConfigPairwiseCipher;
	/* dot11RSNAConfigPairwiseCiphersEntry 3 */
	u_int8_t dot11RSNAConfigPairwiseCipherEnabled;
};

/* Entry in SMT dot11RSNAConfigAuthenticationSuitesTalbe Table:
 *	dot11RSNAConfigAuthenticationSuitesEntry
 */
struct DOT11_RSNA_CONFIG_AUTHENTICATION_SUITES_ENTRY {
	/* dot11RSNAConfigAuthenticationSuitesEntry 2 */
	uint32_t dot11RSNAConfigAuthenticationSuite;
	/* dot11RSNAConfigAuthenticationSuitesEntry 3 */
	u_int8_t dot11RSNAConfigAuthenticationSuiteEnabled;
};

/* ----- IEEE 802.11 MIB Major sections ----- */
struct IEEE_802_11_MIB {
	/* dot11PrivacyTable                            (dot11smt 5) */
	uint8_t dot11WEPDefaultKeyID;	/* dot11PrivacyEntry 2 */
	u_int8_t dot11TranmitKeyAvailable;
	uint32_t dot11WEPICVErrorCount;	/* dot11PrivacyEntry 5 */
	uint32_t dot11WEPExcludedCount;	/* dot11PrivacyEntry 6 */

	/* dot11RSNAConfigTable                         (dot11smt 8) */
	uint32_t dot11RSNAConfigGroupCipher;	/* dot11RSNAConfigEntry 4 */

	/* dot11RSNAConfigPairwiseCiphersTable          (dot11smt 9) */
	struct DOT11_RSNA_CONFIG_PAIRWISE_CIPHERS_ENTRY
	 dot11RSNAConfigPairwiseCiphersTable[MAX_NUM_SUPPORTED_CIPHER_SUITES];

	/* dot11RSNAConfigAuthenticationSuitesTable     (dot11smt 10) */
	struct DOT11_RSNA_CONFIG_AUTHENTICATION_SUITES_ENTRY
	 dot11RSNAConfigAuthenticationSuitesTable[MAX_NUM_SUPPORTED_AKM_SUITES];

#if 0				/* SUPPORT_WAPI */
	u_int8_t fgWapiKeyInstalled;
	struct PARAM_WPI_KEY rWapiPairwiseKey[2];
	u_int8_t fgPairwiseKeyUsed[2];
	uint8_t ucWpiActivedPWKey;	/* Must be 0 or 1, by wapi spec */
	struct PARAM_WPI_KEY rWapiGroupKey[2];
	u_int8_t fgGroupKeyUsed[2];
#endif
};

/* ------------------ IEEE 802.11 non HT PHY characteristics ---------------- */
struct NON_HT_PHY_ATTRIBUTE {
	uint16_t u2SupportedRateSet;

	u_int8_t fgIsShortPreambleOptionImplemented;

	u_int8_t fgIsShortSlotTimeOptionImplemented;
};

struct NON_HT_ATTRIBUTE {
	enum ENUM_PHY_TYPE_INDEX ePhyTypeIndex;

	uint16_t u2BSSBasicRateSet;
};


/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */
extern const struct NON_HT_PHY_ATTRIBUTE
	rNonHTPhyAttributes[];
extern const struct NON_HT_ATTRIBUTE
	rNonHTAdHocModeAttributes[];
extern const struct NON_HT_ATTRIBUTE
	rNonHTApModeAttributes[];

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

#endif /* _MIB_H */
