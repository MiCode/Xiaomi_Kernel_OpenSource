/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
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

#define WPA_KEY_INFO_KEY_TYPE BIT(3)	/* 1 = Pairwise, 0 = Group key */
#define WPA_KEY_INFO_MIC      BIT(8)
#define WPA_KEY_INFO_SECURE   BIT(9)

#define MASK_2ND_EAPOL       (WPA_KEY_INFO_KEY_TYPE | WPA_KEY_INFO_MIC)

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

enum ENUM_EAPOL_KEY_TYPE_T {
	EAPOL_KEY_NOT_KEY = 0,
	EAPOL_KEY_1_OF_4  = 1,
	EAPOL_KEY_2_OF_4  = 2,
	EAPOL_KEY_3_OF_4  = 3,
	EAPOL_KEY_4_OF_4  = 4,
	EAPOL_KEY_1_OF_2  = 5,
	EAPOL_KEY_2_OF_2  = 6,
};

enum ENUM_KEY_ACTION_TYPE_T {
	SEC_DROP_KEY_COMMAND  = 0,
	SEC_QUEUE_KEY_COMMAND = 1,
	SEC_TX_KEY_COMMAND    = 2,
	SEC_ACTION_KEY_NUM
};

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

VOID secInit(IN P_ADAPTER_T prAdapter, IN UINT_8 ucNetTypeIdx);

VOID secSetPortBlocked(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prSta, IN BOOLEAN fgPort);

BOOLEAN secCheckClassError(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb, IN P_STA_RECORD_T prStaRec);

BOOLEAN secTxPortControlCheck(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo, IN P_STA_RECORD_T prStaRec);

BOOLEAN secRxPortControlCheck(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSWRfb);

VOID secSetCipherSuite(IN P_ADAPTER_T prAdapter, IN UINT_32 u4CipherSuitesFlags);

BOOLEAN
secProcessEAPOL(IN P_ADAPTER_T prAdapter,
		IN P_MSDU_INFO_T prMsduInfo,
		IN P_STA_RECORD_T prStaRec, IN PUINT_8 pucPayload, IN UINT_16 u2PayloadLen);

VOID
secHandleTxDoneCallback(IN P_ADAPTER_T prAdapter,
			IN P_MSDU_INFO_T pMsduInfo, IN P_STA_RECORD_T prStaRec, IN WLAN_STATUS rStatus);

BOOLEAN secIsProtectedFrame(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsdu, IN P_STA_RECORD_T prStaRec);

#if (CFG_REFACTORY_PMKSA == 0)
VOID secClearPmkid(IN P_ADAPTER_T prAdapter);
#endif

BOOLEAN secRsnKeyHandshakeEnabled(IN P_ADAPTER_T prAdapter);

BOOLEAN secTransmitKeyExist(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prSta);

BOOLEAN secEnabledInAis(IN P_ADAPTER_T prAdapter);

BOOLEAN secWpaEnabledInAis(IN P_ADAPTER_T prAdapter);

enum ENUM_EAPOL_KEY_TYPE_T secGetEapolKeyType(PUINT_8 pucPacket);

VOID secHandleTxStatus(ADAPTER_T *prAdapter, UINT_8 *pucEvtBuf);

VOID secHandleRxEapolPacket(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prRetSwRfb,
		IN P_STA_RECORD_T prStaRec);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

#endif /* _PRIVACY_H */
