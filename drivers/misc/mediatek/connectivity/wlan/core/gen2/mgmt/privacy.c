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

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "precomp.h"

#if CFG_PRIVACY_MIGRATION

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
VOID secInit(IN P_ADAPTER_T prAdapter, IN UINT_8 ucNetTypeIdx)
{
	UINT_8 i;
	P_CONNECTION_SETTINGS_T prConnSettings;
	P_BSS_INFO_T prBssInfo;
	P_AIS_SPECIFIC_BSS_INFO_T prAisSpecBssInfo;

	DEBUGFUNC("secInit");

	ASSERT(prAdapter);

	prConnSettings = &prAdapter->rWifiVar.rConnSettings;
	prBssInfo = &prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX];
	prAisSpecBssInfo = &prAdapter->rWifiVar.rAisSpecificBssInfo;

	prBssInfo->u4RsnSelectedGroupCipher = 0;
	prBssInfo->u4RsnSelectedPairwiseCipher = 0;
	prBssInfo->u4RsnSelectedAKMSuite = 0;

#if CFG_ENABLE_WIFI_DIRECT
	prBssInfo = &prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX];

	prBssInfo->u4RsnSelectedGroupCipher = RSN_CIPHER_SUITE_CCMP;
	prBssInfo->u4RsnSelectedPairwiseCipher = RSN_CIPHER_SUITE_CCMP;
	prBssInfo->u4RsnSelectedAKMSuite = RSN_AKM_SUITE_PSK;
#endif

#if CFG_ENABLE_BT_OVER_WIFI
	prBssInfo = &prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_BOW_INDEX];

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
	prAdapter->rMib.dot11RSNAConfigPairwiseCiphersTable[8].dot11RSNAConfigPairwiseCipher =
		RSN_CIPHER_SUITE_GROUP_NOT_USED;

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

	prAdapter->rMib.dot11RSNAConfigAuthenticationSuitesTable[6].dot11RSNAConfigAuthenticationSuite =
		RSN_AKM_SUITE_FT_802_1X;
	prAdapter->rMib.dot11RSNAConfigAuthenticationSuitesTable[7].dot11RSNAConfigAuthenticationSuite =
		RSN_AKM_SUITE_FT_PSK;
	prAdapter->rMib.dot11RSNAConfigAuthenticationSuitesTable[8].dot11RSNAConfigAuthenticationSuite =
		WFA_AKM_SUITE_OSEN;

#if CFG_SUPPORT_802_11W
	prAdapter->rMib.dot11RSNAConfigAuthenticationSuitesTable[9].dot11RSNAConfigAuthenticationSuite =
	    RSN_AKM_SUITE_802_1X_SHA256;
	prAdapter->rMib.dot11RSNAConfigAuthenticationSuitesTable[10].dot11RSNAConfigAuthenticationSuite =
	    RSN_AKM_SUITE_PSK_SHA256;
	prAdapter->rMib.dot11RSNAConfigAuthenticationSuitesTable[11].dot11RSNAConfigAuthenticationSuite =
	    RSN_CIPHER_SUITE_SAE;
	prAdapter->rMib.dot11RSNAConfigAuthenticationSuitesTable[12].dot11RSNAConfigAuthenticationSuite =
	    RSN_CIPHER_SUITE_OWE;
#else

#endif

	for (i = 0; i < MAX_NUM_SUPPORTED_AKM_SUITES; i++) {
		prAdapter->rMib.dot11RSNAConfigAuthenticationSuitesTable[i].dot11RSNAConfigAuthenticationSuiteEnabled =
		    FALSE;
	}
#if (CFG_REFACTORY_PMKSA == 0)
	secClearPmkid(prAdapter);

	cnmTimerInitTimer(prAdapter,
			  &prAisSpecBssInfo->rPreauthenticationTimer,
			  (PFN_MGMT_TIMEOUT_FUNC) rsnIndicatePmkidCand, (ULONG) NULL);
#endif
#if CFG_SUPPORT_802_11W
	cnmTimerInitTimer(prAdapter,
			  &prAisSpecBssInfo->rSaQueryTimer, (PFN_MGMT_TIMEOUT_FUNC) rsnStartSaQueryTimer, (ULONG) NULL);
#endif

	prAisSpecBssInfo->fgCounterMeasure = FALSE;
	prAisSpecBssInfo->ucWEPDefaultKeyID = 0;

#if 0
	for (i = 0; i < WTBL_SIZE; i++) {
		g_prWifiVar->arWtbl[i].fgUsed = FALSE;
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
BOOLEAN secCheckClassError(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb, IN P_STA_RECORD_T prStaRec)
{
	ENUM_NETWORK_TYPE_INDEX_T eNetTypeIndex;
	P_BSS_INFO_T prBssInfo;
	P_SW_RFB_T prCurrSwRfb;
	P_HIF_RX_HEADER_T prHifRxHdr;
	UINT_16 u2PktTmpLen;

	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_BSS_DESC_T prBssDesc;

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	if (!prStaRec)
		return FALSE;

	eNetTypeIndex = prStaRec->ucNetTypeIndex;
	if (!IS_NET_ACTIVE(prAdapter, eNetTypeIndex))
		return FALSE;

	prBssInfo = &prAdapter->rWifiVar.arBssInfo[eNetTypeIndex];
	if ((prStaRec->ucStaState != STA_STATE_3) && prBssInfo->fgIsNetAbsent == FALSE) {
		/*(IS_AP_STA(prStaRec) || IS_CLIENT_STA(prStaRec))) { */

#if 0	/* by scott's suggestions, do not put work-around in JB2,we need to find the root cause */
		/* work-around for CR ALPS00816361 */
		if (eNetTypeIndex == NETWORK_TYPE_P2P_INDEX) {
			DBGLOG(RSN, INFO,
			       "p2p> skip to send Deauth to MAC:[%pM] for Rx Class 3.\n",
				prStaRec->aucMacAddr);
			return TRUE;
		}
#endif
		/* Skip to send deaut to AP which STA is connecting */
		if (eNetTypeIndex == NETWORK_TYPE_AIS_INDEX) {
			prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
			prBssDesc = prAisFsmInfo->prTargetBssDesc;
			if ((prBssDesc->fgIsConnected == TRUE || prBssDesc->fgIsConnecting == TRUE)
				&& EQUAL_MAC_ADDR(prBssDesc->aucBSSID, prStaRec->aucMacAddr)) {
				DBGLOG(RSN, INFO, "Skip to send Deauth to MAC:[%pM] for Rx Class 3",
					prStaRec->aucMacAddr);
				return TRUE;
			}
		}

		if (authSendDeauthFrame(prAdapter,
							       prStaRec,
							       NULL,
							       REASON_CODE_CLASS_3_ERR,
							       (PFN_TX_DONE_HANDLER) NULL) == WLAN_STATUS_SUCCESS)
			DBGLOG(RSN, INFO, "Send Deauth to [ %pM ] for Rx Class 3 Error.\n",
					   prStaRec->aucMacAddr);
		else
			DBGLOG(RSN, INFO, "Host sends Deauth to [ %pM ] for Rx Class 3 fail.\n",
					   prStaRec->aucMacAddr);
		DBGLOG(RSN, WARN, "received class 3 data frame !!!");

		/* dump Rx Pkt */
		prCurrSwRfb = prSwRfb;

		prHifRxHdr = prCurrSwRfb->prHifRxHdr;

		DBGLOG(SW4, WARN, "QM RX DATA: net %u sta idx %u wlan idx %u ssn %u tid %u ptype %u 11 %u\n",
			(UINT_32) HIF_RX_HDR_GET_NETWORK_IDX(prHifRxHdr),
			prHifRxHdr->ucStaRecIdx, prCurrSwRfb->ucWlanIdx,
			(UINT_32) HIF_RX_HDR_GET_SN(prHifRxHdr),	/* The new SN of the frame */
			(UINT_32) HIF_RX_HDR_GET_TID(prHifRxHdr),
			prCurrSwRfb->ucPacketType,
			(UINT_32) HIF_RX_HDR_GET_80211_FLAG(prHifRxHdr));

		u2PktTmpLen = prCurrSwRfb->u2PacketLen;
		if (u2PktTmpLen > 48)
			u2PktTmpLen = 48;

		dumpMemory8((PUINT_8) prCurrSwRfb->pvHeader, u2PktTmpLen);

		return FALSE;
	}

	return secRxPortControlCheck(prAdapter, prSwRfb);
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
	if (prSta == NULL)
		return;

	prSta->fgPortBlock = fgPortBlock;

	DBGLOG(RSN, TRACE,
	       "The STA %pM port %s\n", prSta->aucMacAddr, fgPortBlock == TRUE ? "BLOCK" : " OPEN");
}

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
BOOLEAN				/* ENUM_PORT_CONTROL_RESULT */
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
			DBGLOG(SEC, TRACE, "Drop Tx packet due Port Control!\n");
			return FALSE;
		}
#if CFG_SUPPORT_WAPI
		if (prAdapter->rWifiVar.rConnSettings.fgWapiMode)
			return TRUE;
#endif
		if (IS_STA_IN_AIS(prStaRec)) {
			if (!prAdapter->rWifiVar.rAisSpecificBssInfo.fgTransmitKeyExist &&
			    (prAdapter->rWifiVar.rConnSettings.eEncStatus == ENUM_ENCRYPTION1_ENABLED)) {
				DBGLOG(SEC, TRACE, "Drop Tx packet due the key is removed!!!\n");
				return FALSE;
			}
		}
	}

	return TRUE;
}

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
		case RSN_CIPHER_SUITE_GROUP_NOT_USED:
			if (u4CipherSuitesFlags & (CIPHER_FLAG_CCMP | CIPHER_FLAG_TKIP))
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
	else if (rsnSearchSupportedCipher(prAdapter, RSN_CIPHER_SUITE_GROUP_NOT_USED, &i))
		prMib->dot11RSNAConfigGroupCipher = RSN_CIPHER_SUITE_GROUP_NOT_USED;
	else
		prMib->dot11RSNAConfigGroupCipher = WPA_CIPHER_SUITE_NONE;

}				/* secSetCipherSuite */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to handle The 2nd Tx EAPoL Frame.
*
* \param[in] prAdapter Pointer to the Adapter structure
* \param[in] prMsduInfo pointer to the packet info pointer
* \param[in] pucPayload pointer to the 1x hdr
* \param[in] u2PayloadLen the 1x payload length
*
* \retval TRUE Accept the packet
* \retval FALSE Refuse the MSDU packet due port control
*
*/
/*----------------------------------------------------------------------------*/
BOOLEAN
secProcessEAPOL(IN P_ADAPTER_T prAdapter,
		IN P_MSDU_INFO_T prMsduInfo, IN P_STA_RECORD_T prStaRec, IN PUINT_8 pucPayload, IN UINT_16 u2PayloadLen)
{
	P_EAPOL_KEY prEapol = (P_EAPOL_KEY) NULL;
	P_IEEE_802_1X_HDR pr1xHdr;
	UINT_16 u2KeyInfo;

	ASSERT(prMsduInfo);
	ASSERT(prStaRec);

	/* prStaRec = &(g_arStaRec[prMsduInfo->ucStaRecIndex]); */
	ASSERT(prStaRec);

	if (prStaRec && IS_AP_STA(prStaRec)) {
		pr1xHdr = (P_IEEE_802_1X_HDR) pucPayload;
		if ((pr1xHdr->ucType == 3) /* EAPoL key */ && ((u2PayloadLen - 4) > sizeof(EAPOL_KEY))) {
			prEapol = (P_EAPOL_KEY) ((PUINT_32) (pucPayload + 4));
			WLAN_GET_FIELD_BE16(prEapol->aucKeyInfo, &u2KeyInfo);
			if ((prEapol->ucType == 254) && (u2KeyInfo & MASK_2ND_EAPOL)) {
				if (u2KeyInfo & WPA_KEY_INFO_SECURE) {
					/* 4th EAPoL check at secHandleTxDoneCallback() */
					/* DBGLOG(RSN, TRACE, ("Tx 4th EAPoL frame\r\n")); */
				} else if (u2PayloadLen == 123 /* Not include LLC */) {
					DBGLOG(RSN, INFO, "Tx 2nd EAPoL frame\r\n");
					secFsmEvent2ndEapolTx(prAdapter, prStaRec);
				}
			}
		}
	}

	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function will handle the 4th EAPoL Tx done and mic Error Report frame.
*
* \param[in] prAdapter            Pointer to the Adapter structure
* \param[in] pMsduInfo            Pointer to the Msdu Info
* \param[in] rStatus                The Tx done status
*
* \return -
*/
/*----------------------------------------------------------------------------*/
VOID
secHandleTxDoneCallback(IN P_ADAPTER_T prAdapter,
			IN P_MSDU_INFO_T prMsduInfo, IN P_STA_RECORD_T prStaRec, IN WLAN_STATUS rStatus)
{
	PUINT_8 pucPayload;
	P_IEEE_802_1X_HDR pr1xHdr = (P_IEEE_802_1X_HDR) NULL;
	P_EAPOL_KEY prEapol = (P_EAPOL_KEY) NULL;
	UINT_16 u2KeyInfo;
	UINT_16 u2PayloadLen;

	DEBUGFUNC("secHandleTxDoneCallback");

	ASSERT(prMsduInfo);
	/* Todo:: Notice if using the TX free immediate after send to firmware, the payload may not correcttly!!!! */

	ASSERT(prStaRec);

	/* Todo:: This call back may not need because the order of set key and send 4th 1x can be make sure */
	/* Todo:: Notice the LLC offset */
#if 1
	pucPayload = (PUINT_8) prMsduInfo->prPacket;
	ASSERT(pucPayload);

	u2PayloadLen = prMsduInfo->u2FrameLength;

	if (0 /* prMsduInfo->fgIs1xFrame */) {

		if (prStaRec && IS_AP_STA(prStaRec)) {
			pr1xHdr = (P_IEEE_802_1X_HDR) (PUINT_32) (pucPayload + 8);
			if ((pr1xHdr->ucType == 3) /* EAPoL key */ && ((u2PayloadLen - 4) > sizeof(EAPOL_KEY))) {
				prEapol = (P_EAPOL_KEY) (PUINT_32) (pucPayload + 12);
				WLAN_GET_FIELD_BE16(prEapol->aucKeyInfo, &u2KeyInfo);
				if ((prEapol->ucType == 254) && (u2KeyInfo & MASK_2ND_EAPOL)) {
					if (prStaRec->rSecInfo.fg2nd1xSend == TRUE
					    && u2PayloadLen ==
					    107 /* include LLC *//* u2KeyInfo & WPA_KEY_INFO_SECURE */) {
						DBGLOG(RSN, INFO, "Tx 4th EAPoL frame\r\n");
						secFsmEvent4ndEapolTxDone(prAdapter, prStaRec);
					} else if (prAdapter->rWifiVar.rAisSpecificBssInfo.fgCheckEAPoLTxDone) {
						DBGLOG(RSN, INFO, "Tx EAPoL Error report frame\r\n");
						/* secFsmEventEapolTxDone(prAdapter, (UINT_32)prMsduInfo->prStaRec); */
					}
				}
			}
		}

	}
#endif
}

#if (CFG_REFACTORY_PMKSA == 0)
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
#endif
/*----------------------------------------------------------------------------*/
/*!
* \brief Whether WPA, or WPA2 but not WPA-None is enabled.
*
* \param[in] prAdapter Pointer to the Adapter structure
*
* \retval BOOLEAN
*/
/*----------------------------------------------------------------------------*/
BOOLEAN secRsnKeyHandshakeEnabled(IN P_ADAPTER_T prAdapter)
{
	P_CONNECTION_SETTINGS_T prConnSettings;

	ASSERT(prAdapter);

	prConnSettings = &prAdapter->rWifiVar.rConnSettings;

	ASSERT(prConnSettings);

	ASSERT(prConnSettings->eEncStatus < ENUM_ENCRYPTION3_KEY_ABSENT);

	if (prConnSettings->eEncStatus == ENUM_ENCRYPTION_DISABLED)
		return FALSE;

	ASSERT(prConnSettings->eAuthMode < AUTH_MODE_NUM);
	if ((prConnSettings->eAuthMode >= AUTH_MODE_WPA) && (prConnSettings->eAuthMode != AUTH_MODE_WPA_NONE))
		return TRUE;

	return FALSE;
}				/* secRsnKeyHandshakeEnabled */

/*----------------------------------------------------------------------------*/
/*!
* \brief Return whether the transmit key alread installed.
*
* \param[in] prAdapter Pointer to the Adapter structure
* \param[in] prSta Pointer the sta record
*
* \retval TRUE Default key or Transmit key installed
*         FALSE Default key or Transmit key not installed
*
* \note:
*/
/*----------------------------------------------------------------------------*/
BOOLEAN secTransmitKeyExist(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prSta)
{
	ASSERT(prSta);

	if (prSta->fgTransmitKeyExist)
		return TRUE;
	else
		return FALSE;
}				/* secTransmitKeyExist */

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
	DEBUGFUNC("secEnabled");

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
}				/* secEnabled */

BOOLEAN secWpaEnabledInAis(IN P_ADAPTER_T prAdapter)
{
	DEBUGFUNC("secEnabled");

	ASSERT(prAdapter->rWifiVar.rConnSettings.eEncStatus < ENUM_ENCRYPTION3_KEY_ABSENT);

	switch (prAdapter->rWifiVar.rConnSettings.eEncStatus) {
	case ENUM_ENCRYPTION_DISABLED:
	case ENUM_ENCRYPTION1_ENABLED:
		return FALSE;
	case ENUM_ENCRYPTION2_ENABLED:
	case ENUM_ENCRYPTION3_ENABLED:
		return TRUE;
	default:
		DBGLOG(RSN, TRACE, "Unknown encryption setting %d\n", prAdapter->rWifiVar.rConnSettings.eEncStatus);
		break;
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
	ASSERT(prAdapter);

	ASSERT(prMsdu);

	ASSERT(prStaRec);
	/* prStaRec = &(g_arStaRec[prMsdu->ucStaRecIndex]); */

	if (prStaRec == NULL) {
		if (prAdapter->rWifiVar.rAisSpecificBssInfo.fgTransmitKeyExist)
			return TRUE;
		return FALSE;	/* No privacy bit */
	}

	/* Todo:: */
	if (0 /* prMsdu->fgIs1xFrame */) {
		if (IS_STA_IN_AIS(prStaRec) && prAdapter->rWifiVar.rConnSettings.eAuthMode < AUTH_MODE_WPA) {
			DBGLOG(RSN, LOUD, "For AIS Legacy 1x, always not encryped\n");
			return FALSE;
		} else if (!prStaRec->fgTransmitKeyExist) {
			DBGLOG(RSN, LOUD, "1x Not Protected.\n");
			return FALSE;
		} else if (prStaRec->rSecInfo.fgKeyStored) {
			DBGLOG(RSN, LOUD, "1x not Protected due key stored!\n");
			return FALSE;
		}
		DBGLOG(RSN, LOUD, "1x Protected.\n");
		return TRUE;
	}
	if (!prStaRec->fgTransmitKeyExist) {
		/* whsu , check for AIS only */
		if (prAdapter->rWifiVar.rConnSettings.eAuthMode < AUTH_MODE_WPA &&
		    prAdapter->rWifiVar.rAisSpecificBssInfo.fgTransmitKeyExist) {
			DBGLOG(RSN, LOUD, "Protected\n");
			return TRUE;
		}
	} else {
			DBGLOG(RSN, LOUD, "Protected.\n");
		return TRUE;
	}

	/* No sec or key is removed!!! */
	return FALSE;
}

VOID secHandleEapolTxStatus(ADAPTER_T *prAdapter, UINT_8 *pucEvtBuf)
{
	EVENT_TX_DONE_STATUS_T *prTxDone;
	UINT_8 status;
	PUINT_8 pucPkt;
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;
	enum ENUM_EAPOL_KEY_TYPE_T keyType;

	do {
		prP2pBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX]);
		/* TODO: only handle p2p case */
		if (prP2pBssInfo->eConnectionState != PARAM_MEDIA_STATE_CONNECTED ||
			prP2pBssInfo->eCurrentOPMode != OP_MODE_INFRASTRUCTURE)
			break;
		prTxDone = (EVENT_TX_DONE_STATUS_T *) pucEvtBuf;
		pucPkt = &prTxDone->aucPktBuf[64];
		status = prTxDone->ucStatus;
		keyType = secGetEapolKeyType(pucPkt);
		if (keyType == EAPOL_KEY_4_OF_4) {
			if (status == 0) {
				DBGLOG(RSN, INFO, "EAPOL key 4/4 TX success\n");
				prP2pBssInfo->eKeyAction = SEC_TX_KEY_COMMAND;
			} else {
				DBGLOG(RSN, INFO, "EAPOL key 4/4 TX fail\n");
				prP2pBssInfo->eKeyAction = SEC_DROP_KEY_COMMAND;
			}
			kalSetEvent(prAdapter->prGlueInfo);
		}
	} while (FALSE);

}

/* return the type of EAPOL frame. */
enum ENUM_EAPOL_KEY_TYPE_T secGetEapolKeyType(PUINT_8 pucPkt)
{
	UINT_16 u2EtherType = 0;
	PUINT_8 pucEthBody = NULL;
	PUINT_8 pucEapol = NULL;
	UINT_16 u2KeyInfo = 0;
	UINT_8 ucEapolType;

	do {
		ASSERT_BREAK(pucPkt != NULL);
		u2EtherType = (pucPkt[ETH_TYPE_LEN_OFFSET] << 8) | (pucPkt[ETH_TYPE_LEN_OFFSET + 1]);
		pucEthBody = &pucPkt[ETH_HLEN];
		if (u2EtherType != ETH_P_1X)
			break;
		pucEapol = pucEthBody;
		ucEapolType = pucEapol[1];
		/*
		 * EAPOL type:
		 *   0: eap packet
		 *   1: eapol start
		 *   3: eapol key
		 */
		if (ucEapolType != 3)
			break;
		u2KeyInfo = pucEapol[5] << 8 | pucEapol[6];
		if (u2KeyInfo == 0x008a)
			return EAPOL_KEY_1_OF_4;
		else if (u2KeyInfo == 0x010a)
			return EAPOL_KEY_2_OF_4;
		else if (u2KeyInfo == 0x13ca)
			return EAPOL_KEY_3_OF_4;
		else if (u2KeyInfo == 0x030a)
			return EAPOL_KEY_4_OF_4;
	} while (FALSE);

	return EAPOL_KEY_NOT_KEY;
}

VOID secHandleTxStatus(ADAPTER_T *prAdapter, UINT_8 *pucEvtBuf)
{
	STATS_TX_PKT_DONE_INFO_DISPLAY(prAdapter, pucEvtBuf);
#if CFG_SUPPORT_P2P_EAP_FAIL_WORKAROUND
	p2pFuncEAPfailureWorkaround(prAdapter, pucEvtBuf);
#endif
	secHandleEapolTxStatus(prAdapter, pucEvtBuf);
	p2pFsmNotifyTxStatus(prAdapter, pucEvtBuf);
}

VOID secHandleRxEapolPacket(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prRetSwRfb,
		IN P_STA_RECORD_T prStaRec)
{
	enum ENUM_EAPOL_KEY_TYPE_T eKeyType;
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;

	do {
		if (prRetSwRfb->u2PacketLen <= ETHER_HEADER_LEN)
			break;

		if (!prStaRec) {
			DBGLOG(RSN, ERROR, "can NOT get prStaRec\n");
			break;
		}
		if (prStaRec->ucNetTypeIndex >= NETWORK_TYPE_INDEX_NUM) {
			DBGLOG(RSN, ERROR, "invalid ucNetTypeIndex\n");
			break;
		}
		/* TODO: only handle p2p case */
		if (prStaRec->ucNetTypeIndex != NETWORK_TYPE_P2P_INDEX)
			break;
		prP2pBssInfo = &(prAdapter->rWifiVar.arBssInfo[prStaRec->ucNetTypeIndex]);
		if (prP2pBssInfo->eCurrentOPMode != OP_MODE_INFRASTRUCTURE ||
			prP2pBssInfo->eConnectionState != PARAM_MEDIA_STATE_CONNECTED)
			break;

		eKeyType = secGetEapolKeyType((PUINT_8) prRetSwRfb->pvHeader);
		if (eKeyType != EAPOL_KEY_3_OF_4)
			break;
		DBGLOG(RSN, INFO, "RX EAPOL 3/4 key\n");
		prP2pBssInfo->eKeyAction = SEC_QUEUE_KEY_COMMAND;
	} while (FALSE);
}
#endif
