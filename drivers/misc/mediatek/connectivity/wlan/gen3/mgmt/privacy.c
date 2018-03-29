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
/*
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/mgmt/privacy.c#1
*/

/*! \file   "privacy.c"
    \brief  This file including the protocol layer privacy function.

    This file provided the macros and functions library support for the
    protocol layer security setting from rsn.c and nic_privacy.c

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
* \brief This routine is called to initialize the privacy-related
*        parameters.
*
* \param[in] prAdapter Pointer to the Adapter structure
* \param[in] ucNetTypeIdx  Pointer to netowrk type index
*
* \retval NONE
*/
/*----------------------------------------------------------------------------*/
VOID secInit(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex)
{
	UINT_8 i;
	P_BSS_INFO_T prBssInfo;
	P_AIS_SPECIFIC_BSS_INFO_T prAisSpecBssInfo;

	DEBUGFUNC("secInit");

	ASSERT(prAdapter);

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);
	prAisSpecBssInfo = &prAdapter->rWifiVar.rAisSpecificBssInfo;

	prBssInfo->u4RsnSelectedGroupCipher = 0;
	prBssInfo->u4RsnSelectedPairwiseCipher = 0;
	prBssInfo->u4RsnSelectedAKMSuite = 0;

#if 0				/* CFG_ENABLE_WIFI_DIRECT */
	prBssInfo = &prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P];

	prBssInfo->u4RsnSelectedGroupCipher = RSN_CIPHER_SUITE_CCMP;
	prBssInfo->u4RsnSelectedPairwiseCipher = RSN_CIPHER_SUITE_CCMP;
	prBssInfo->u4RsnSelectedAKMSuite = RSN_AKM_SUITE_PSK;
#endif

#if 0				/* CFG_ENABLE_BT_OVER_WIFI */
	prBssInfo = &prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_BOW];

	prBssInfo->u4RsnSelectedGroupCipher = RSN_CIPHER_SUITE_CCMP;
	prBssInfo->u4RsnSelectedPairwiseCipher = RSN_CIPHER_SUITE_CCMP;
	prBssInfo->u4RsnSelectedAKMSuite = RSN_AKM_SUITE_PSK;
#endif

	prAdapter->rMib.dot11RSNAConfigPairwiseCiphersTable[0].dot11RSNAConfigPairwiseCipher = WPA_CIPHER_SUITE_WEP40;
	prAdapter->rMib.dot11RSNAConfigPairwiseCiphersTable[1].dot11RSNAConfigPairwiseCipher = WPA_CIPHER_SUITE_TKIP;
	prAdapter->rMib.dot11RSNAConfigPairwiseCiphersTable[2].dot11RSNAConfigPairwiseCipher = WPA_CIPHER_SUITE_CCMP;
	prAdapter->rMib.dot11RSNAConfigPairwiseCiphersTable[3].dot11RSNAConfigPairwiseCipher = WPA_CIPHER_SUITE_WEP104;

	prAdapter->rMib.dot11RSNAConfigPairwiseCiphersTable[4].dot11RSNAConfigPairwiseCipher = RSN_CIPHER_SUITE_WEP40;
	prAdapter->rMib.dot11RSNAConfigPairwiseCiphersTable[5].dot11RSNAConfigPairwiseCipher = RSN_CIPHER_SUITE_TKIP;
	prAdapter->rMib.dot11RSNAConfigPairwiseCiphersTable[6].dot11RSNAConfigPairwiseCipher = RSN_CIPHER_SUITE_CCMP;
	prAdapter->rMib.dot11RSNAConfigPairwiseCiphersTable[7].dot11RSNAConfigPairwiseCipher = RSN_CIPHER_SUITE_WEP104;

	for (i = 0; i < MAX_NUM_SUPPORTED_CIPHER_SUITES; i++)
		prAdapter->rMib.dot11RSNAConfigPairwiseCiphersTable[i].dot11RSNAConfigPairwiseCipherEnabled = FALSE;

	prAdapter->rMib.dot11RSNAConfigAuthenticationSuitesTable[0].dot11RSNAConfigAuthenticationSuite =
	    WPA_AKM_SUITE_NONE;
	prAdapter->rMib.dot11RSNAConfigAuthenticationSuitesTable[1].dot11RSNAConfigAuthenticationSuite =
	    WPA_AKM_SUITE_802_1X;
	prAdapter->rMib.dot11RSNAConfigAuthenticationSuitesTable[2].dot11RSNAConfigAuthenticationSuite =
	    WPA_AKM_SUITE_PSK;
	prAdapter->rMib.dot11RSNAConfigAuthenticationSuitesTable[3].dot11RSNAConfigAuthenticationSuite =
	    RSN_AKM_SUITE_NONE;
	prAdapter->rMib.dot11RSNAConfigAuthenticationSuitesTable[4].dot11RSNAConfigAuthenticationSuite =
	    RSN_AKM_SUITE_802_1X;
	prAdapter->rMib.dot11RSNAConfigAuthenticationSuitesTable[5].dot11RSNAConfigAuthenticationSuite =
	    RSN_AKM_SUITE_PSK;

#if CFG_SUPPORT_802_11W
	prAdapter->rMib.dot11RSNAConfigAuthenticationSuitesTable[6].dot11RSNAConfigAuthenticationSuite =
	    RSN_AKM_SUITE_802_1X_SHA256;
	prAdapter->rMib.dot11RSNAConfigAuthenticationSuitesTable[7].dot11RSNAConfigAuthenticationSuite =
	    RSN_AKM_SUITE_PSK_SHA256;
#endif

	for (i = 0; i < MAX_NUM_SUPPORTED_AKM_SUITES; i++) {
		prAdapter->rMib.dot11RSNAConfigAuthenticationSuitesTable[i].dot11RSNAConfigAuthenticationSuiteEnabled =
		    FALSE;
	}

	secClearPmkid(prAdapter);

	cnmTimerInitTimer(prAdapter,
			  &prAisSpecBssInfo->rPreauthenticationTimer,
			  (PFN_MGMT_TIMEOUT_FUNC) rsnIndicatePmkidCand, (ULONG) NULL);

#if CFG_SUPPORT_802_11W
	cnmTimerInitTimer(prAdapter,
			  &prAisSpecBssInfo->rSaQueryTimer, (PFN_MGMT_TIMEOUT_FUNC) rsnStartSaQueryTimer, (ULONG) NULL);
#endif

	prAisSpecBssInfo->fgCounterMeasure = FALSE;
	prAdapter->prAisBssInfo->ucTxDefaultKeyID = 0;
	prAdapter->prAisBssInfo->fgTxBcKeyExist = FALSE;

#if 0
	for (i = 0; i < WTBL_SIZE; i++) {
		g_prWifiVar->arWtbl[i].ucUsed = FALSE;
		g_prWifiVar->arWtbl[i].prSta = NULL;
		g_prWifiVar->arWtbl[i].ucNetTypeIdx = NETWORK_TYPE_INDEX_NUM;

	}
	nicPrivacyInitialize((UINT_8) NETWORK_TYPE_INDEX_NUM);
#endif

}				/* secInit */

/*----------------------------------------------------------------------------*/
/*!
* \brief This function will indicate an Event of "Rx Class Error" to SEC_FSM for
*        JOIN Module.
*
* \param[in] prAdapter     Pointer to the Adapter structure
* \param[in] prSwRfb       Pointer to the SW RFB.
*
* \return FALSE                Class Error
*/
/*----------------------------------------------------------------------------*/
BOOL secCheckClassError(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb, IN P_STA_RECORD_T prStaRec)
{
	P_HW_MAC_RX_DESC_T prRxStatus;

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prRxStatus = prSwRfb->prRxStatus;

#if 1
	if (!prStaRec || (prRxStatus->u2StatusFlag & RXS_DW2_RX_CLASSERR_BITMAP) == RXS_DW2_RX_CLASSERR_VALUE) {

		DBGLOG(RSN, TRACE,
		       "prStaRec=%p RX Status = %hx RX_CLASSERR check!\n", prStaRec, prRxStatus->u2StatusFlag);

		/* if (IS_NET_ACTIVE(prAdapter, ucBssIndex)) { */
		authSendDeauthFrame(prAdapter,
				    NULL, NULL, prSwRfb, REASON_CODE_CLASS_3_ERR, (PFN_TX_DONE_HANDLER) NULL);
		return FALSE;
		/* } */
	}
#else
	if ((prStaRec) && 1 /* RXM_IS_DATA_FRAME(prSwRfb) */) {
		UINT_8 ucBssIndex = prStaRec->ucBssIndex;

		if (IS_NET_ACTIVE(prAdapter, ucBssIndex)) {
			/* P_BSS_INFO_T prBssInfo; */
			/* prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex); */

			if ((prRxStatus->u2StatusFlag & RXS_DW2_RX_CLASSERR_BITMAP) == RXS_DW2_RX_CLASSERR_VALUE) {
				/*if ((STA_STATE_3 != prStaRec->ucStaState) &&
				   IS_BSS_ACTIVE(prBssInfo) &&
				   prBssInfo->fgIsNetAbsent == FALSE) {
				   (IS_AP_STA(prStaRec) || IS_CLIENT_STA(prStaRec))) { */
				if (WLAN_STATUS_SUCCESS == authSendDeauthFrame(prAdapter,
									       prStaRec,
									       NULL,
									       REASON_CODE_CLASS_3_ERR,
									       (PFN_TX_DONE_HANDLER)
									       NULL)) {

					DBGLOG(RSN, TRACE,
					       "Send Deauth to MAC:[" MACSTR
						"] for Rx Class 3 Error.\n", MAC2STR(prStaRec->aucMacAddr));
				}

				return FALSE;
			}

			return secRxPortControlCheck(prAdapter, prSwRfb);
		}
	}
#endif
	return TRUE;

}				/* end of secCheckClassError() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to setting the sta port status.
*
* \param[in]  prAdapter Pointer to the Adapter structure
* \param[in]  prSta Pointer to the sta
* \param[in]  fgPortBlock The port status
*
* \retval none
*
*/
/*----------------------------------------------------------------------------*/
VOID secSetPortBlocked(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prSta, IN BOOLEAN fgPortBlock)
{
#if 0				/* Marked for MT6630 */
	if (prSta == NULL)
		return;

	prSta->fgPortBlock = fgPortBlock;

	DBGLOG(RSN, TRACE,
	       "The STA " MACSTR " port %s\n", MAC2STR(prSta->aucMacAddr), fgPortBlock == TRUE ? "BLOCK" : " OPEN");
#endif
}

#if 0				/* Marked for MT6630 */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to report the sta port status.
*
* \param[in]  prAdapter Pointer to the Adapter structure
* \param[in]  prSta Pointer to the sta
* \param[out]  fgPortBlock The port status
*
* \return TRUE sta exist, FALSE sta not exist
*
*/
/*----------------------------------------------------------------------------*/
BOOLEAN secGetPortStatus(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prSta, OUT PBOOLEAN pfgPortStatus)
{
	if (prSta == NULL)
		return FALSE;

	*pfgPortStatus = prSta->fgPortBlock;

	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to handle Peer device Tx Security process MSDU.
*
* \param[in] prMsduInfo pointer to the packet info pointer
*
* \retval TRUE Accept the packet
* \retval FALSE Refuse the MSDU packet due port blocked
*
*/
/*----------------------------------------------------------------------------*/
BOOL				/* ENUM_PORT_CONTROL_RESULT */
secTxPortControlCheck(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo, IN P_STA_RECORD_T prStaRec)
{
	ASSERT(prAdapter);
	ASSERT(prMsduInfo);
	ASSERT(prStaRec);

	if (prStaRec) {

		/* Todo:: */
		if (prMsduInfo->fgIs802_1x)
			return TRUE;

		if (prStaRec->fgPortBlock == TRUE) {
			DBGLOG(INIT, TRACE, "Drop Tx packet due Port Control!\n");
			return FALSE;
		}
#if CFG_SUPPORT_WAPI
		if (prAdapter->rWifiVar.rConnSettings.fgWapiMode)
			return TRUE;
#endif
		if (IS_STA_IN_AIS(prStaRec)) {
			if (!prAdapter->rWifiVar.rAisSpecificBssInfo.fgTransmitKeyExist &&
			    (prAdapter->rWifiVar.rConnSettings.eEncStatus == ENUM_ENCRYPTION1_ENABLED)) {
				DBGLOG(INIT, TRACE, "Drop Tx packet due the key is removed!!!\n");
				return FALSE;
			}
		}
	}

	return TRUE;
}
#endif /* Marked for MT6630 */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to handle The Rx Security process MSDU.
*
* \param[in] prAdapter Pointer to the Adapter structure
* \param[in] prSWRfb SW rfb pinter
*
* \retval TRUE Accept the packet
* \retval FALSE Refuse the MSDU packet due port control
*/
/*----------------------------------------------------------------------------*/
BOOLEAN secRxPortControlCheck(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSWRfb)
{
	ASSERT(prSWRfb);

#if 0
	/* whsu:Todo: Process MGMT and DATA */
	if (prSWRfb->prStaRec) {
		if (prSWRfb->prStaRec->fgPortBlock == TRUE) {
			if (1 /* prSWRfb->fgIsDataFrame and not 1x */  &&
			    (g_prWifiVar->rConnSettings.eAuthMode >= AUTH_MODE_WPA)) {
				/* DBGLOG(SEC, WARN, ("Drop Rx data due port control !\r\n")); */
				return TRUE;	/* Todo: whsu FALSE; */
			}
			/* if (!RX_STATUS_IS_PROTECT(prSWRfb->prRxStatus)) { */
			/* DBGLOG(RSN, WARN, ("Drop rcv non-encrypted data frame!\n")); */
			/* return FALSE; */
			/* } */
		}
	} else {
	}
#endif
	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine will enable/disable the cipher suite
*
* \param[in] prAdapter Pointer to the adapter object data area.
* \param[in] u4CipherSuitesFlags flag for cipher suite
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID secSetCipherSuite(IN P_ADAPTER_T prAdapter, IN UINT_32 u4CipherSuitesFlags)
{

	UINT_32 i;
	P_DOT11_RSNA_CONFIG_PAIRWISE_CIPHERS_ENTRY prEntry;
	P_IEEE_802_11_MIB_T prMib;

	ASSERT(prAdapter);

	prMib = &prAdapter->rMib;

	ASSERT(prMib);

	if (u4CipherSuitesFlags == CIPHER_FLAG_NONE) {
		/* Disable all the pairwise cipher suites. */
		for (i = 0; i < MAX_NUM_SUPPORTED_CIPHER_SUITES; i++)
			prMib->dot11RSNAConfigPairwiseCiphersTable[i].dot11RSNAConfigPairwiseCipherEnabled = FALSE;

		/* Update the group cipher suite. */
		prMib->dot11RSNAConfigGroupCipher = WPA_CIPHER_SUITE_NONE;

		return;
	}

	for (i = 0; i < MAX_NUM_SUPPORTED_CIPHER_SUITES; i++) {
		prEntry = &prMib->dot11RSNAConfigPairwiseCiphersTable[i];

		switch (prEntry->dot11RSNAConfigPairwiseCipher) {
		case WPA_CIPHER_SUITE_WEP40:
		case RSN_CIPHER_SUITE_WEP40:
			if (u4CipherSuitesFlags & CIPHER_FLAG_WEP40)
				prEntry->dot11RSNAConfigPairwiseCipherEnabled = TRUE;
			else
				prEntry->dot11RSNAConfigPairwiseCipherEnabled = FALSE;
			break;

		case WPA_CIPHER_SUITE_TKIP:
		case RSN_CIPHER_SUITE_TKIP:
			if (u4CipherSuitesFlags & CIPHER_FLAG_TKIP)
				prEntry->dot11RSNAConfigPairwiseCipherEnabled = TRUE;
			else
				prEntry->dot11RSNAConfigPairwiseCipherEnabled = FALSE;
			break;

		case WPA_CIPHER_SUITE_CCMP:
		case RSN_CIPHER_SUITE_CCMP:
			if (u4CipherSuitesFlags & CIPHER_FLAG_CCMP)
				prEntry->dot11RSNAConfigPairwiseCipherEnabled = TRUE;
			else
				prEntry->dot11RSNAConfigPairwiseCipherEnabled = FALSE;
			break;

		case WPA_CIPHER_SUITE_WEP104:
		case RSN_CIPHER_SUITE_WEP104:
			if (u4CipherSuitesFlags & CIPHER_FLAG_WEP104)
				prEntry->dot11RSNAConfigPairwiseCipherEnabled = TRUE;
			else
				prEntry->dot11RSNAConfigPairwiseCipherEnabled = FALSE;
			break;
		default:
			break;
		}
	}

	/* Update the group cipher suite. */
	if (rsnSearchSupportedCipher(prAdapter, WPA_CIPHER_SUITE_CCMP, &i))
		prMib->dot11RSNAConfigGroupCipher = WPA_CIPHER_SUITE_CCMP;
	else if (rsnSearchSupportedCipher(prAdapter, WPA_CIPHER_SUITE_TKIP, &i))
		prMib->dot11RSNAConfigGroupCipher = WPA_CIPHER_SUITE_TKIP;
	else if (rsnSearchSupportedCipher(prAdapter, WPA_CIPHER_SUITE_WEP104, &i))
		prMib->dot11RSNAConfigGroupCipher = WPA_CIPHER_SUITE_WEP104;
	else if (rsnSearchSupportedCipher(prAdapter, WPA_CIPHER_SUITE_WEP40, &i))
		prMib->dot11RSNAConfigGroupCipher = WPA_CIPHER_SUITE_WEP40;
	else
		prMib->dot11RSNAConfigGroupCipher = WPA_CIPHER_SUITE_NONE;

}				/* secSetCipherSuite */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to initialize the pmkid parameters.
*
* \param[in] prAdapter Pointer to the Adapter structure
*
* \retval NONE
*/
/*----------------------------------------------------------------------------*/
VOID secClearPmkid(IN P_ADAPTER_T prAdapter)
{
	P_AIS_SPECIFIC_BSS_INFO_T prAisSpecBssInfo;

	DEBUGFUNC("secClearPmkid");

	prAisSpecBssInfo = &prAdapter->rWifiVar.rAisSpecificBssInfo;
	DBGLOG(RSN, TRACE, "secClearPmkid\n");
	prAisSpecBssInfo->u4PmkidCandicateCount = 0;
	prAisSpecBssInfo->u4PmkidCacheCount = 0;
	kalMemZero((PVOID) prAisSpecBssInfo->arPmkidCandicate, sizeof(PMKID_CANDICATE_T) * CFG_MAX_PMKID_CACHE);
	kalMemZero((PVOID) prAisSpecBssInfo->arPmkidCache, sizeof(PMKID_ENTRY_T) * CFG_MAX_PMKID_CACHE);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Whether 802.11 privacy is enabled.
*
* \param[in] prAdapter Pointer to the Adapter structure
*
* \retval BOOLEAN
*/
/*----------------------------------------------------------------------------*/
BOOLEAN secEnabledInAis(IN P_ADAPTER_T prAdapter)
{
	DEBUGFUNC("secEnabledInAis");

	ASSERT(prAdapter->rWifiVar.rConnSettings.eEncStatus < ENUM_ENCRYPTION3_KEY_ABSENT);

	switch (prAdapter->rWifiVar.rConnSettings.eEncStatus) {
	case ENUM_ENCRYPTION_DISABLED:
		return FALSE;
	case ENUM_ENCRYPTION1_ENABLED:
	case ENUM_ENCRYPTION2_ENABLED:
	case ENUM_ENCRYPTION3_ENABLED:
		return TRUE;
	default:
		DBGLOG(RSN, TRACE, "Unknown encryption setting %d\n", prAdapter->rWifiVar.rConnSettings.eEncStatus);
		break;
	}
	return FALSE;

}				/* secEnabledInAis */

/**
	return the type of Eapol frame.
**/
enum EAPOL_KEY_TYPE secGetEapolKeyType(PUINT_8 pucPacket)
{
	UINT_16 u2EthType = 0;
	UINT_16 u2KeyInfo = 0;

	if (!pucPacket)
		return EAPOL_KEY_NOT_KEY;
	WLAN_GET_FIELD_BE16(&pucPacket[ETHER_HEADER_LEN - ETHER_TYPE_LEN], &u2EthType);
	if (u2EthType != ETH_P_1X)
		return EAPOL_KEY_NOT_KEY;
	u2KeyInfo = pucPacket[5+ETHER_HEADER_LEN]<<8 | pucPacket[6+ETHER_HEADER_LEN];
	/*	802.11-2012, 11.6.2, Figure 11-29
		Key Info: BIT(0,2)-Key descriptor & Version; BIT(3)-Key type; BIT(4,5)-reserved;
		BIT(6)-install; BIT(7)-Ack; BIT(8)-MIC; BIT(9)-secure; BIT(10)-error; BIT(11)-request
		BIT(12)-Encrypted data; BIT(13)-SMK; BIT(14,15)-reserved;

		802.11-2012, 11.6.6 ~ 11.6.7
		1/4: 0x89
		2/4: 0x109
		3/4: 0x13c9 or 0x1389
		4/4: 0x309
		1/2: 0x381
		2/2: 0x301 */
	DBGLOG(RSN, TRACE, "u2KeyInfo=0x%x\n", (u2KeyInfo & 0x388));
	/* Only check Bit9, Bit8, Bit7 and Bit3, for Bit6, some times it may be missing */
	switch (u2KeyInfo & 0x388) {
	case 0x388:
	case 0x188: /* BIT(9) may be missing in some AP, we should compatiable with it */
		return EAPOL_KEY_3_OF_4;
	case 0x308: /* if BIT(9) in 3/4 is missing, it is hard to distinguish 4/4 and 2/4 */
		return EAPOL_KEY_4_OF_4;
	case 0x108: /* BIT(8) | BIT(3) */
		return EAPOL_KEY_2_OF_4;
	case 0x88: /* BIT(3) | BIT(7) */
		return EAPOL_KEY_1_OF_4;
	case 0x180: /* don't check BIT(9) since some Authenticator may miss it */
		return EAPOL_KEY_1_OF_2;
	case 0x100: /* don't check BIT(9) since some Authenticator may miss it */
		return EAPOL_KEY_2_OF_2;
	}
	return EAPOL_KEY_NOT_KEY;
}

BOOLEAN secIsProtected1xFrame(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec)
{
	P_BSS_INFO_T prBssInfo;

	ASSERT(prAdapter);

	if (prStaRec) {
		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);
		if (prBssInfo && prBssInfo->eNetworkType == NETWORK_TYPE_AIS) {
#if CFG_SUPPORT_WAPI
			if (wlanQueryWapiMode(prAdapter))
				return FALSE;
#endif
		}

		return prStaRec->fgTransmitKeyExist;
	}
	return FALSE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set the privacy bit at mac header for TxM
*
* \param[in] prAdapter Pointer to the Adapter structure
* \param[in] prMsdu the msdu for known the sta record
*
* \return TRUE the privacy need to set
*            FALSE the privacy no need to set
*/
/*----------------------------------------------------------------------------*/
BOOLEAN secIsProtectedFrame(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsdu, IN P_STA_RECORD_T prStaRec)
{
	/* P_BSS_INFO_T prBssInfo; */

	ASSERT(prAdapter);
	ASSERT(prMsdu);
	/* ASSERT(prStaRec); */

#if CFG_SUPPORT_802_11W
	if (prMsdu->ucPacketType == TX_PACKET_TYPE_MGMT) {
		BOOL fgRobustActionWithProtect = FALSE;
#if 0				/* Decide by Compose module */
		P_BSS_INFO_T prBssInfo;

		if (prStaRec) {
			prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);
			ASSERT(prBssInfo);
			if ((prBssInfo->eNetworkType == NETWORK_TYPE_AIS) &&
			    prAdapter->rWifiVar.rAisSpecificBssInfo.fgMgmtProtection /* Use MFP */) {

				fgRobustActionWithProtect = TRUE;
			}
		}
#endif
		if (prStaRec && fgRobustActionWithProtect /* AIS & Robust action frame */)
			return TRUE;
		else
			return FALSE;
	}
#else
	if (prMsdu->ucPacketType == TX_PACKET_TYPE_MGMT)
		return FALSE;
#endif

#if 1
	return secIsProtectedBss(prAdapter, GET_BSS_INFO_BY_INDEX(prAdapter, prMsdu->ucBssIndex));
#else
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prMsdu->ucBssIndex);

	ASSERT(prBssInfo);

	if (prBssInfo->eNetworkType == NETWORK_TYPE_AIS) {
#if CFG_SUPPORT_WAPI
		if (wlanQueryWapiMode(prAdapter))
			return TRUE;
#endif
		return secEnabledInAis(prAdapter);
	}
#if CFG_ENABLE_WIFI_DIRECT
	else if (prBssInfo->eNetworkType == NETWORK_TYPE_P2P)
		return kalP2PGetCipher(prAdapter->prGlueInfo);
#endif
	else if (prBssInfo->eNetworkType == NETWORK_TYPE_BOW)
		return TRUE;

	ASSERT(FALSE);
	return FALSE;
#endif
}

BOOLEAN secIsProtectedBss(IN P_ADAPTER_T prAdapter, IN P_BSS_INFO_T prBssInfo)
{
	ASSERT(prBssInfo);

	if (prBssInfo->eNetworkType == NETWORK_TYPE_AIS) {
#if CFG_SUPPORT_WAPI
		if (wlanQueryWapiMode(prAdapter))
			return TRUE;
#endif
		return secEnabledInAis(prAdapter);
	}
#if CFG_ENABLE_WIFI_DIRECT
	else if (prBssInfo->eNetworkType == NETWORK_TYPE_P2P)
		return kalP2PGetCipher(prAdapter->prGlueInfo);
#endif
	else if (prBssInfo->eNetworkType == NETWORK_TYPE_BOW)
		return TRUE;

	ASSERT(FALSE);
	return FALSE;
}

UINT_8 secGetBmcWlanIndex(IN P_ADAPTER_T prAdapter, IN ENUM_NETWORK_TYPE_T eNetType, IN P_STA_RECORD_T prStaRec)
{
	UINT_8 ucBssIndex;
	UINT_8 ucEntry;

	if ((!prAdapter) || (!prStaRec))
		return WTBL_RESERVED_ENTRY;

	ucBssIndex = prStaRec->ucBssIndex;

	if (prAdapter->aprBssInfo[ucBssIndex]->fgTxBcKeyExist) {
		if (prStaRec && prAdapter->aprBssInfo[ucBssIndex]->ucBMCWlanIndex == WTBL_RESERVED_ENTRY) {
			ucEntry =
			    secPrivacySeekForBcEntry(prAdapter, ucBssIndex, prStaRec->aucMacAddr,
						     prStaRec->ucIndex, CIPHER_SUITE_NONE, 0xff, 0x0, BIT(0));
			prAdapter->aprBssInfo[ucBssIndex]->ucBMCWlanIndex = ucEntry;
		}

		DBGLOG(RSN, INFO,
		       "[Wlan index] secGetBmcWlanIndex = %d\n", prAdapter->aprBssInfo[ucBssIndex]->ucBMCWlanIndex);
		return prAdapter->aprBssInfo[ucBssIndex]->ucBMCWlanIndex;
	}

	ucEntry = secPrivacySeekForBcEntry(prAdapter, ucBssIndex, prStaRec->aucMacAddr,
				   prStaRec->ucIndex, CIPHER_SUITE_NONE, 0xff, 0x0, BIT(0));

	return ucEntry;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used before add/update a WLAN entry.
*        Info the WLAN Table has available entry for this request
*
* \param[in] prAdapter Pointer to the Adapter structure
* \param[in]  prSta the P_STA_RECORD_T for store
*
* \return TRUE Free Wlan table is reserved for this request
*            FALSE No free entry for this request
*
* \note
*/
/*----------------------------------------------------------------------------*/
BOOL secPrivacySeekForEntry(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prSta)
{
	UINT_8 ucEntry = WTBL_RESERVED_ENTRY;
	UINT_8 i;
	UINT_8 ucStartIDX = 0, ucMaxIDX = 0;
	P_WLAN_TABLE_T prWtbl;

	ASSERT(prSta);

	if (!prSta->fgIsInUse)
		ASSERT(FALSE);

	DBGLOG(RSN, TRACE, MACSTR "\n", MAC2STR(prSta->aucMacAddr));

	prWtbl = prAdapter->rWifiVar.arWtbl;

#if 1
	ucStartIDX = 0;
	ucMaxIDX = NIC_TX_DEFAULT_WLAN_INDEX - 1;
#else
	if (prSta->eStaType == STA_TYPE_DLS_PEER) {
		ucStartIDX = WTBL_STA_IDX_MAX;
		ucMaxIDX = WTBL_AIS_DLS_MAX_IDX;
	} else if (IS_ADHOC_STA(prSta)) {
		ucStartIDX = 0;
		ucMaxIDX = WTBL_IBSS_STA_IDX_MAX;
	} else {
		ucStartIDX = 0;
		ucMaxIDX = WTBL_STA_IDX_MAX;
	}
#endif

	if (ucStartIDX != 0 || ucMaxIDX != 0) {
		if (ucStartIDX > ucMaxIDX) {
			for (i = ucStartIDX; i > ucMaxIDX; i--) {
				if (prWtbl[i].ucUsed && EQUAL_MAC_ADDR(prSta->aucMacAddr, prWtbl[i].aucMacAddr)
				    && prWtbl[i].ucPairwise) {
					ucEntry = i;
					DBGLOG(RSN, TRACE, "[Wlan index]: Reuse entry #%d\n", i);
					break;
				}
			}
			if (i == ucMaxIDX) {
				for (i = ucStartIDX; i > ucMaxIDX; i--) {
					if (prWtbl[i].ucUsed == FALSE) {
						ucEntry = i;
						DBGLOG(RSN, TRACE, "[Wlan index]: Assign entry #%d\n", i);
						break;
					}
				}
			}
		} else {
			for (i = ucStartIDX; i <= ucMaxIDX; i++) {
				if (prWtbl[i].ucUsed && EQUAL_MAC_ADDR(prSta->aucMacAddr, prWtbl[i].aucMacAddr)
				    && prWtbl[i].ucPairwise) {
					ucEntry = i;
					DBGLOG(RSN, TRACE, "[Wlan index]: Reuse entry #%d\n", i);
					break;
				}
			}
			if (i == (ucMaxIDX + 1)) {
				for (i = ucStartIDX; i <= ucMaxIDX; i++) {
					if (prWtbl[i].ucUsed == FALSE) {
						ucEntry = i;
						DBGLOG(RSN, TRACE, "[Wlan index]: Assign entry #%d\n", i);
						break;
					}
				}
			}
		}
	}

	/* Save to the driver maintain table */
	if (ucEntry < WTBL_SIZE) {

		prWtbl[ucEntry].ucUsed = TRUE;
		prWtbl[ucEntry].ucBssIndex = prSta->ucBssIndex;
		prWtbl[ucEntry].ucKeyId = 0;
		prWtbl[ucEntry].ucPairwise = 1;
		COPY_MAC_ADDR(prWtbl[ucEntry].aucMacAddr, prSta->aucMacAddr);
		prWtbl[ucEntry].ucStaIndex = prSta->ucIndex;

		prSta->ucWlanIndex = ucEntry;

		DBGLOG(RSN, INFO,
		       "[Wlan index] BSS#%d keyid#%d P=%d use WlanIndex#%d STAIdx=%d " MACSTR
			" staType=%x\n", prSta->ucBssIndex, 0, prWtbl[ucEntry].ucPairwise, ucEntry,
			prSta->ucIndex, MAC2STR(prSta->aucMacAddr), prSta->eStaType);

		if (IS_AP_STA(prSta)) {
			prSta->ucBMCWlanIndex = secGetBmcWlanIndex(prAdapter, NETWORK_TYPE_AIS, prSta);
			ASSERT(prSta->ucBMCWlanIndex < WTBL_SIZE);
		} else {
			/* DBGLOG(RSN, TRACE, ("AP or GO\n")); */
			/* For AP/GO, BC entry saved at BSS INFO */
			prSta->ucBMCWlanIndex = 255;
		}
#if DBG
		secCheckWTBLAssign(prAdapter);
#endif
	} else {
#if DBG
		secCheckWTBLAssign(prAdapter);
#endif
		DBGLOG(RSN, WARN, "[Wlan index] No more wlan table entry available!!!!\n");
		return FALSE;
	}
	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used free a WLAN entry.
*
* \param[in] prAdapter Pointer to the Adapter structure
* \param[in]  ucEntry the wlan table index to free
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID secPrivacyFreeForEntry(IN P_ADAPTER_T prAdapter, IN UINT_8 ucEntry)
{
	P_WLAN_TABLE_T prWtbl;

	ASSERT(prAdapter);

	if (ucEntry >= WTBL_SIZE)
		return;

	DBGLOG(RSN, TRACE, "secPrivacyFreeForEntry %d", ucEntry);

	prWtbl = prAdapter->rWifiVar.arWtbl;

	prWtbl[ucEntry].ucUsed = FALSE;
	prWtbl[ucEntry].ucKeyId = 0;
	prWtbl[ucEntry].ucBssIndex = 0;	/* Set non-Zero as default ? */
	prWtbl[ucEntry].ucPairwise = 0;
	kalMemSet(prWtbl[ucEntry].aucMacAddr, 0x0, MAC_ADDR_LEN);
	prWtbl[ucEntry].ucStaIndex = 0xff;

#if DBG
	{
		P_HAL_WTBL_SEC_CONFIG_T prWtblCfg;

		prWtblCfg = prAdapter->rWifiVar.arWtblCfg;

		kalMemZero((PUINT_8)&prWtblCfg[ucEntry], sizeof(HAL_WTBL_SEC_CONFIG_T));

		secPrivacyDumpWTBL3(prAdapter, ucEntry);

		secCheckWTBLAssign(prAdapter);
	}
#endif

}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used free a STA WLAN entry.
*
* \param[in] prAdapter Pointer to the Adapter structure
* \param[in]  prStaRec the sta which want to free
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID secPrivacyFreeSta(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec)
{
	UINT_32 entry;
	P_WLAN_TABLE_T prWtbl;

	if (!prStaRec)
		return;

	prWtbl = prAdapter->rWifiVar.arWtbl;
	prStaRec->ucWlanIndex = WTBL_RESERVED_ENTRY;
	prStaRec->ucBMCWlanIndex = WTBL_RESERVED_ENTRY;

	/* if ((IS_ADHOC_STA(prStaRec) || IS_STA_IN_AIS(prStaRec))  */
	/*  && prStaRec->ucBMCWlanIndex < WTBL_SIZE *//*) { */
	/* the hotspot mode would be assert after connect-disconnect field try WTBL_SIZE times */
	if (TRUE) {
		for (entry = 0; entry < WTBL_SIZE; entry++) {
			if (prWtbl[entry].ucUsed &&
				(prStaRec->ucIndex == prWtbl[entry].ucStaIndex)) {
				secPrivacyFreeForEntry(prAdapter, entry);
				DBGLOG(RSN, TRACE, "Free the STA entry (%u)!\n", entry);
			}
		}
	}

}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used for adding the broadcast key used, to assign a wlan table entry
*         for reserved the specific entry for these key for
*
* \param[in] prAdapter Pointer to the Adapter structure
* \param[in] ucBssIndex The BSS index
* \param[in] ucNetTypeIdx The Network index
* \param[in] ucAlg the entry assign related with algorithm
* \param[in] ucKeyId The key id
* \param[in] ucTxRx The Type of the key
*
* \return ucEntryIndex The entry to be used, WTBL_ALLOC_FAIL for allocation fail
*
* \note
*/
/*----------------------------------------------------------------------------*/
UINT_8
secPrivacySeekForBcEntry(IN P_ADAPTER_T prAdapter,
			 IN UINT_8 ucBssIndex,
			 IN PUINT_8 pucAddr,
			 IN UINT_8 ucStaIdx,
			 IN UINT_8 ucAlg, IN UINT_8 ucKeyId, IN UINT_8 ucCurrentKeyId, IN UINT_8 ucTxRx)
{
	UINT_8 ucEntry = WTBL_ALLOC_FAIL;
	UINT_8 ucStartIDX = 0, ucMaxIDX = 0;
	UINT_8 i = 0;
	BOOLEAN fgCheckKeyId = TRUE;
	P_WLAN_TABLE_T prWtbl;
	P_BSS_INFO_T prBSSInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

	prWtbl = prAdapter->rWifiVar.arWtbl;

	ASSERT(prAdapter);
	ASSERT(pucAddr);

#if 1
	ucStartIDX = 0;
	ucMaxIDX = NIC_TX_DEFAULT_WLAN_INDEX - 1;
#else
	if (ucAlg == CIPHER_SUITE_BIP) {
		if (ucNetTypeIdx != NETWORK_TYPE_AIS) {
			ASSERT(FALSE);
			return ucEntry;
		}
		ucEntry = WTBL_AIS_BIP_IDX;	/* Only support 11w at STA mode */
	}
#endif

	if (ucAlg == CIPHER_SUITE_WPI || ucAlg == CIPHER_SUITE_WEP40 ||
	    ucAlg == CIPHER_SUITE_WEP104 || ucAlg == CIPHER_SUITE_WEP128 || ucAlg == CIPHER_SUITE_NONE)
		fgCheckKeyId = FALSE;

#if 0
	if ((ucNetTypeIdx == NETWORK_TYPE_AIS)
	    && (prAdapter->aprBssInfo[ucBssIndex]->eCurrentOPMode == OP_MODE_IBSS)) {
		ucStartIDX = WTBL_IBSS_BC_IDX_0;
		ucMaxIDX = WTBL_BC_IDX_MAX;
	} else {
		ucStartIDX = WTBL_BC_IDX_0;
		ucMaxIDX = WTBL_BC_IDX_MAX;
	}
#endif

	for (i = ucStartIDX; i <= ucMaxIDX; i++) {
#if DBG
		if (i < 10) {
			DBGLOG(RSN, TRACE,
			       "idx=%d use=%d P=%d BSSIdx=%d Addr=" MACSTR " keyid=%d\n", i,
				prWtbl[i].ucUsed, prWtbl[i].ucPairwise, prWtbl[i].ucBssIndex,
				MAC2STR(prWtbl[i].aucMacAddr),
				prWtbl[i].ucKeyId);
		}
#endif
		if (prWtbl[i].ucUsed && !prWtbl[i].ucPairwise && prWtbl[i].ucBssIndex == ucBssIndex &&
			(EQUAL_MAC_ADDR(prWtbl[i].aucMacAddr, pucAddr) ||
			(prBSSInfo && (prBSSInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT)))
		    ) {
			if (!fgCheckKeyId || prWtbl[i].ucKeyId == 0xff
			    || (fgCheckKeyId && prWtbl[i].ucKeyId == ucKeyId)) {
				ucEntry = i;
				DBGLOG(RSN, TRACE, "[Wlan index]: Reuse entry #%d\n", i);
				break;
			}
		}
	}

	if (i == (ucMaxIDX + 1)) {
		for (i = ucStartIDX; i <= ucMaxIDX; i++) {
			if (prWtbl[i].ucUsed == FALSE) {
				ucEntry = i;
				DBGLOG(RSN, TRACE, "[Wlan index]: Assign entry #%d\n", i);
				break;
			}
		}
	}

	if (ucEntry < WTBL_SIZE) {
		prWtbl[ucEntry].ucUsed = TRUE;
		prWtbl[ucEntry].ucKeyId = ucKeyId;
		prWtbl[ucEntry].ucBssIndex = ucBssIndex;
		prWtbl[ucEntry].ucPairwise = 0;
		kalMemCopy(prWtbl[ucEntry].aucMacAddr, pucAddr, MAC_ADDR_LEN);
		prWtbl[ucEntry].ucStaIndex = ucStaIdx;

		DBGLOG(RSN, TRACE,
		       "[Wlan index] BSS#%d keyid#%d P=%d use WlanIndex#%d STAIdx=%d " MACSTR
			"\n", ucBssIndex, ucKeyId, prWtbl[ucEntry].ucPairwise, ucEntry, ucStaIdx, MAC2STR(pucAddr));

#if DBG
		secCheckWTBLAssign(prAdapter);
#endif
	}

	ASSERT(ucEntry != WTBL_ALLOC_FAIL);

	return ucEntry;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in] prAdapter Pointer to the Adapter structure
*
* \return ucEntryIndex The entry to be used, WTBL_ALLOC_FAIL for allocation fail
*
* \note
*/
/*----------------------------------------------------------------------------*/
BOOLEAN secCheckWTBLAssign(IN P_ADAPTER_T prAdapter)
{
	UINT_8 i;
	BOOLEAN fgCheckFail = FALSE;

	secPrivacyDumpWTBL(prAdapter);

	for (i = 0; i <= WTBL_SIZE; i++) {
		/* AIS STA should just has max 2 entry */

		/* Max STA check */
	}

	if (fgCheckFail)
		ASSERT(FALSE);

	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Got the STA record index by wlan index
*
* \param[in] prAdapter Pointer to the Adapter structure
* \param[in] ucWlanIdx The Rx wlan index
*
* \return The STA record index, 0xff for invalid sta index
*/
/*----------------------------------------------------------------------------*/
UINT_8 secGetStaIdxByWlanIdx(P_ADAPTER_T prAdapter, UINT_8 ucWlanIdx)
{
	P_WLAN_TABLE_T prWtbl;

	ASSERT(prAdapter);

	if (ucWlanIdx >= WTBL_SIZE)
		return 0xff;

	prWtbl = prAdapter->rWifiVar.arWtbl;

	/* DBGLOG(RSN, TRACE, ("secGetStaIdxByWlanIdx=%d "MACSTR" used=%d\n",
	 * ucWlanIdx, MAC2STR(prWtbl[ucWlanIdx].aucMacAddr), prWtbl[ucWlanIdx].ucUsed)); */

	if (prWtbl[ucWlanIdx].ucUsed)
		return prWtbl[ucWlanIdx].ucStaIndex;
	else
		return 0xff;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  At Sw wlan table, got the BSS index by wlan index
*
* \param[in] prAdapter Pointer to the Adapter structure
* \param[in] ucWlanIdx The Rx wlan index
*
* \return The BSS index, 0xff for invalid bss index
*/
/*----------------------------------------------------------------------------*/
UINT_8 secGetBssIdxByWlanIdx(P_ADAPTER_T prAdapter, UINT_8 ucWlanIdx)
{
	P_WLAN_TABLE_T prWtbl;

	ASSERT(prAdapter);

	if (ucWlanIdx >= WTBL_SIZE)
		return 0xff;

	prWtbl = prAdapter->rWifiVar.arWtbl;

	if (prWtbl[ucWlanIdx].ucUsed)
		return prWtbl[ucWlanIdx].ucBssIndex;
	else
		return 0xff;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Got the STA record index by mac addr
*
* \param[in] prAdapter Pointer to the Adapter structure
* \param[in] pucMacAddress MAC Addr
*
* \return The STA record index, 0xff for invalid sta index
*/
/*----------------------------------------------------------------------------*/
UINT_8 secLookupStaRecIndexFromTA(P_ADAPTER_T prAdapter, PUINT_8 pucMacAddress)
{
	int i;
	P_WLAN_TABLE_T prWtbl;

	ASSERT(prAdapter);
	prWtbl = prAdapter->rWifiVar.arWtbl;

	for (i = 0; i < WTBL_SIZE; i++) {
		if (prWtbl[i].ucUsed) {
			if (EQUAL_MAC_ADDR(pucMacAddress, prWtbl[i].aucMacAddr))
				return prWtbl[i].ucStaIndex;
		}
	}

	return 0xff;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in] prAdapter Pointer to the Adapter structure
*
* \note
*/
/*----------------------------------------------------------------------------*/
void secPrivacyDumpWTBL(IN P_ADAPTER_T prAdapter)
{
	P_WLAN_TABLE_T prWtbl;
	UINT_8 i;

	prWtbl = prAdapter->rWifiVar.arWtbl;

	DBGLOG(RSN, INFO, "The Wlan Table content\n");

	for (i = 0; i <= WTBL_SIZE; i++) {
		DBGLOG(RSN, INFO,
		       "#%d Used=%d  BSSIdx=%d keyid=%d pairwise=%d STAIdx=%d" MACSTR "\n", i,
			prWtbl[i].ucUsed, prWtbl[i].ucBssIndex, prWtbl[i].ucKeyId,
			prWtbl[i].ucPairwise, prWtbl[i].ucStaIndex, MAC2STR(prWtbl[i].aucMacAddr));
	}

}

#if DBG
void secPrivacyDumpWTBL3(IN P_ADAPTER_T prAdapter, IN UINT_8 ucIndex)
{
	P_HAL_WTBL_SEC_CONFIG_T prWtblCfg;
	P_WLAN_TABLE_T prWtbl;
	UINT_8 i;

	prWtbl = prAdapter->rWifiVar.arWtbl;
	prWtblCfg = prAdapter->rWifiVar.arWtblCfg;

	if (ucIndex < WTBL_SIZE) {
		DBGLOG(RSN, INFO, "The Wlan Table#3 content\n");

		/* for (i = 0; i <= WTBL_SIZE; i++){ */
		i = ucIndex;
		DBGLOG(RSN, INFO,
		       "#%d(%d) BSS=%d " MACSTR
			" RV=%d RKV=%d RCA2=%d ALG=%d RCID=%d keyid=%d RCA1=%d MUAR=%02x IKV=%d PN=%d\n",
			i, prWtbl[i].ucUsed, prWtbl[i].ucBssIndex, MAC2STR(prWtbl[i].aucMacAddr),
			prWtblCfg[i].fgRV, prWtblCfg[i].fgRKV, prWtblCfg[i].fgRCA2,
			prWtblCfg[i].ucCipherSuit, prWtblCfg[i].fgRCID, prWtbl[i].ucKeyId,
			prWtblCfg[i].fgRCA1, prWtblCfg[i].ucMUARIdx, prWtblCfg[i].fgIKV, prWtblCfg[i].fgEvenPN));
		/* } */
	}

}

#endif

VOID secSetKeyCmdAction(P_BSS_INFO_T prBssInfo, UINT_8 ucEapolKeyType, UINT_8 ucAction)
{
	ASSERT(prBssInfo);

	switch (ucEapolKeyType) {
	/* some AP may miss Bit9 in KeyInfo, so the 4/4 we send will also miss it, and it seems like 2/4 */
	case EAPOL_KEY_4_OF_4:
	case EAPOL_KEY_2_OF_4:
		prBssInfo->ucKeyCmdAction = ucAction;
		break;
	case EAPOL_KEY_3_OF_4:
		prBssInfo->ucKeyCmdAction = ucAction;
		if (ucAction == SEC_QUEUE_KEY_COMMAND)
			prBssInfo->fgUnencryptedEapol = FALSE;
		/* If 3/4 is unencrypted,then 4/4 will be unencrypted */
		else
			prBssInfo->fgUnencryptedEapol = TRUE;
		DBGLOG(RSN, INFO, "ucKeyCmdAction is %d, prBssInfo->fgUnencryptedEapol is %d\n",
			 prBssInfo->ucKeyCmdAction, prBssInfo->fgUnencryptedEapol);

		break;
	}
}

UINT_8 secGetBssIdxByNetType(P_ADAPTER_T prAdapter)
{
	P_BSS_INFO_T prBssInfo = NULL;
	UINT_8 i = 0;
	UINT_8 ucBssIndex = 0xff;
	BOOLEAN fgP2pDevice = FALSE;
	BOOLEAN fgAisDevice = FALSE;

	for (; i < BSS_INFO_NUM; i++) {
		prBssInfo = prAdapter->aprBssInfo[i];
		if (prBssInfo->eConnectionState != PARAM_MEDIA_STATE_CONNECTED)
			continue;
		if (prBssInfo->eNetworkType == NETWORK_TYPE_AIS) {
			fgAisDevice = TRUE;
			ucBssIndex = prBssInfo->ucBssIndex;
#if CFG_ENABLE_WIFI_DIRECT
		} else	if (prBssInfo->eNetworkType == NETWORK_TYPE_P2P &&
			prBssInfo->eCurrentOPMode == OP_MODE_P2P_DEVICE) {
			fgP2pDevice = TRUE;
			ucBssIndex = prBssInfo->ucBssIndex;
#endif
		}
	}
	if (fgP2pDevice && fgAisDevice) {
		DBGLOG(RSN, INFO, "p2p device & ais co-exist\n");
		return 0xff;
	}
	return ucBssIndex;
}

