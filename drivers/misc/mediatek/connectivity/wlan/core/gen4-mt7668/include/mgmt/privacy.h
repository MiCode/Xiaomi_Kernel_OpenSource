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
** Id: include/mgmt/privacy.h#1
*/

/*! \file   privacy.h
 *  \brief This file contains the function declaration for privacy.c.
 */


#ifndef _PRIVACY_H
#define _PRIVACY_H

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
#define MAX_KEY_NUM                             4
#define WEP_40_LEN                              5
#define WEP_104_LEN                             13
#define WEP_128_LEN                             16
#define LEGACY_KEY_MAX_LEN                      16
#define CCMP_KEY_LEN                            16
#define TKIP_KEY_LEN                            32
#define MAX_KEY_LEN                             32
#define MIC_RX_KEY_OFFSET                       16
#define MIC_TX_KEY_OFFSET                       24
#define MIC_KEY_LEN                             8

#define WEP_KEY_ID_FIELD      BITS(0, 29)
#define KEY_ID_FIELD          BITS(0, 7)

#define IS_TRANSMIT_KEY       BIT(31)
#define IS_UNICAST_KEY        BIT(30)
#define IS_AUTHENTICATOR      BIT(28)

#define CIPHER_SUITE_NONE               0
#define CIPHER_SUITE_WEP40              1
#define CIPHER_SUITE_TKIP               2
#define CIPHER_SUITE_TKIP_WO_MIC        3
#define CIPHER_SUITE_CCMP               4
#define CIPHER_SUITE_WEP104             5
#define CIPHER_SUITE_BIP                6
#define CIPHER_SUITE_WEP128             7
#define CIPHER_SUITE_WPI                8
#define CIPHER_SUITE_CCMP_W_CCX         9
#define CIPHER_SUITE_GCMP               10
#define CIPHER_SUITE_GCMP_128           11
#define CIPHER_SUITE_GCMP_256           12

/* Todo:: Move to register */
#if defined(MT6630)
#define WTBL_RESERVED_ENTRY             255
#else
#define WTBL_RESERVED_ENTRY             255
#endif
/* Todo:: By chip capability */
/* Max wlan table size, the max+1 used for probe request,... mgmt frame */
/*sending use basic rate and no security */
#define WTBL_SIZE                       32

#define WTBL_ALLOC_FAIL                 WTBL_RESERVED_ENTRY
#define WTBL_DEFAULT_ENTRY              0

/*******************************************************************************
 *                         D A T A   T Y P E S
 ********************************************************************************
 */

typedef struct _IEEE_802_1X_HDR {
	UINT_8 ucVersion;
	UINT_8 ucType;
	UINT_16 u2Length;
	/* followed by length octets of data */
} IEEE_802_1X_HDR, *P_IEEE_802_1X_HDR;

typedef struct _EAPOL_KEY {
	UINT_8 ucType;
	/* Note: key_info, key_length, and key_data_length are unaligned */
	UINT_8 aucKeyInfo[2];	/* big endian */
	UINT_8 aucKeyLength[2];	/* big endian */
	UINT_8 aucReplayCounter[8];
	UINT_8 aucKeyNonce[16];
	UINT_8 aucKeyIv[16];
	UINT_8 aucKeyRsc[8];
	UINT_8 aucKeyId[8];	/* Reserved in IEEE 802.11i/RSN */
	UINT_8 aucKeyMic[16];
	UINT_8 aucKeyDataLength[2];	/* big endian */
	/* followed by key_data_length bytes of key_data */
} EAPOL_KEY, *P_EAPOL_KEY;

/* WPA2 PMKID candicate structure */
typedef struct _PMKID_CANDICATE_T {
	UINT_8 aucBssid[MAC_ADDR_LEN];
	UINT_32 u4PreAuthFlags;
} PMKID_CANDICATE_T, *P_PMKID_CANDICATE_T;

#if 0
/* WPA2 PMKID cache structure */
typedef struct _PMKID_ENTRY_T {
	PARAM_BSSID_INFO_T rBssidInfo;
	BOOLEAN fgPmkidExist;
} PMKID_ENTRY_T, *P_PMKID_ENTRY_T;
#endif

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
 *                  F U N C T I O N   D E C L A R A T I O N S
 ********************************************************************************
 */

VOID secInit(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex);

VOID secSetPortBlocked(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prSta, IN BOOLEAN fgPort);

BOOL secCheckClassError(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb, IN P_STA_RECORD_T prStaRec);

BOOL secTxPortControlCheck(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo, IN P_STA_RECORD_T prStaRec);

BOOLEAN secRxPortControlCheck(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSWRfb);

VOID secSetCipherSuite(IN P_ADAPTER_T prAdapter, IN UINT_32 u4CipherSuitesFlags);

BOOLEAN secIsProtectedFrame(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsdu, IN P_STA_RECORD_T prStaRec);

VOID secClearPmkid(IN P_ADAPTER_T prAdapter);

BOOLEAN secRsnKeyHandshakeEnabled(IN P_ADAPTER_T prAdapter);

UINT_8 secGetBmcWlanIndex(IN P_ADAPTER_T prAdapter, IN ENUM_NETWORK_TYPE_T eNetType, IN P_STA_RECORD_T prStaRec);

BOOLEAN secTransmitKeyExist(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prSta);

BOOLEAN secEnabledInAis(IN P_ADAPTER_T prAdapter);

BOOL secPrivacySeekForEntry(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prSta);

VOID secPrivacyFreeForEntry(IN P_ADAPTER_T prAdapter, IN UINT_8 ucEntry);

VOID secPrivacyFreeSta(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec);

VOID secRemoveBssBcEntry(IN P_ADAPTER_T prAdapter, IN P_BSS_INFO_T prBssInfo, IN BOOL fgRoam);

UINT_8
secPrivacySeekForBcEntry(IN P_ADAPTER_T prAdapter,
			 IN UINT_8 ucBssIndex,
			 IN PUINT_8 pucAddr, IN UINT_8 ucStaIdx, IN UINT_8 ucAlg, IN UINT_8 ucKeyId);

UINT_8 secGetStaIdxByWlanIdx(IN P_ADAPTER_T prAdapter, IN UINT_8 ucWlanIdx);

UINT_8 secGetBssIdxByWlanIdx(IN P_ADAPTER_T prAdapter, IN UINT_8 ucWlanIdx);

UINT_8 secLookupStaRecIndexFromTA(P_ADAPTER_T prAdapter, PUINT_8 pucMacAddress);

void secPrivacyDumpWTBL(IN P_ADAPTER_T prAdapter);

BOOLEAN secCheckWTBLAssign(IN P_ADAPTER_T prAdapter);

BOOLEAN secIsProtected1xFrame(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec);

BOOLEAN secIsProtectedBss(IN P_ADAPTER_T prAdapter, IN P_BSS_INFO_T prBssInfo);

BOOLEAN tkipMicDecapsulate(IN P_SW_RFB_T prSwRfb, IN PUINT_8 pucMicKey);

BOOLEAN tkipMicDecapsulateInRxHdrTransMode(IN P_SW_RFB_T prSwRfb, IN PUINT_8 pucMicKey);

void secPostUpdateAddr(IN P_ADAPTER_T prAdapter, IN P_BSS_INFO_T prBssInfo);

/*******************************************************************************
 *                              F U N C T I O N S
 ********************************************************************************
 */

#endif /* _PRIVACY_H */
