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
** Id: //Department/DaVinci/TRUNK/WiFi_P2P_Driver/common/wlan_p2p.c#8
*/

/*! \file wlan_bow.c
*    \brief This file contains the Wi-Fi Direct commands processing routines for
*	   MediaTek Inc. 802.11 Wireless LAN Adapters.
*/


/******************************************************************************
*                         C O M P I L E R   F L A G S
*******************************************************************************
*/

/******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
*******************************************************************************
*/
#include "precomp.h"
#include "gl_p2p_ioctl.h"

/******************************************************************************
*                              C O N S T A N T S
*******************************************************************************
*/

/******************************************************************************
*                             D A T A   T Y P E S
*******************************************************************************
*/

/******************************************************************************
*                            P U B L I C   D A T A
*******************************************************************************
*/

/******************************************************************************
*                           P R I V A T E   D A T A
*******************************************************************************
*/

/******************************************************************************
*                                 M A C R O S
*******************************************************************************
*/

/******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
*******************************************************************************
*/

/******************************************************************************
*                              F U N C T I O N S
*******************************************************************************
*/
/*----------------------------------------------------------------------------*/
/*!
* \brief command packet generation utility
*
* \param[in] prAdapter          Pointer to the Adapter structure.
* \param[in] ucCID              Command ID
* \param[in] fgSetQuery         Set or Query
* \param[in] fgNeedResp         Need for response
* \param[in] pfCmdDoneHandler   Function pointer when command is done
* \param[in] u4SetQueryInfoLen  The length of the set/query buffer
* \param[in] pucInfoBuffer      Pointer to set/query buffer
*
*
* \retval WLAN_STATUS_PENDING
* \retval WLAN_STATUS_FAILURE
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSendSetQueryP2PCmd(IN P_ADAPTER_T prAdapter,
			  IN UINT_8 ucCID,
			  IN UINT_8 ucBssIdx,
			  IN BOOLEAN fgSetQuery,
			  IN BOOLEAN fgNeedResp,
			  IN BOOLEAN fgIsOid,
			  IN PFN_CMD_DONE_HANDLER pfCmdDoneHandler,
			  IN PFN_CMD_TIMEOUT_HANDLER pfCmdTimeoutHandler,
			  IN UINT_32 u4SetQueryInfoLen,
			  IN PUINT_8 pucInfoBuffer, OUT PVOID pvSetQueryBuffer, IN UINT_32 u4SetQueryBufferLen)
{
	P_GLUE_INFO_T prGlueInfo;
	P_CMD_INFO_T prCmdInfo;
	P_WIFI_CMD_T prWifiCmd;
	UINT_8 ucCmdSeqNum;

	ASSERT(prAdapter);

	prGlueInfo = prAdapter->prGlueInfo;
	ASSERT(prGlueInfo);

	DEBUGFUNC("wlanoidSendSetQueryP2PCmd");
	DBGLOG(REQ, TRACE, "Command ID = 0x%08X\n", ucCID);

	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, (CMD_HDR_SIZE + u4SetQueryInfoLen));

	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}
	/* increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);
	DBGLOG(REQ, TRACE, "ucCmdSeqNum =%d\n", ucCmdSeqNum);

	/* Setup common CMD Info Packet */
	prCmdInfo->eCmdType = COMMAND_TYPE_NETWORK_IOCTL;
	prCmdInfo->u2InfoBufLen = (UINT_16) (CMD_HDR_SIZE + u4SetQueryInfoLen);
	prCmdInfo->pfCmdDoneHandler = pfCmdDoneHandler;
	prCmdInfo->pfCmdTimeoutHandler = pfCmdTimeoutHandler;
	prCmdInfo->fgIsOid = fgIsOid;
	prCmdInfo->ucCID = ucCID;
	prCmdInfo->fgSetQuery = fgSetQuery;
	prCmdInfo->fgNeedResp = fgNeedResp;
	prCmdInfo->ucCmdSeqNum = ucCmdSeqNum;
	prCmdInfo->u4SetInfoLen = u4SetQueryInfoLen;
	prCmdInfo->pvInformationBuffer = pvSetQueryBuffer;
	prCmdInfo->u4InformationBufferLength = u4SetQueryBufferLen;

	/* Setup WIFI_CMD_T (no payload) */
	prWifiCmd = (P_WIFI_CMD_T) (prCmdInfo->pucInfoBuffer);
	prWifiCmd->u2TxByteCount = prCmdInfo->u2InfoBufLen;
	prWifiCmd->ucCID = prCmdInfo->ucCID;
	prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
	prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;

	if (u4SetQueryInfoLen > 0 && pucInfoBuffer != NULL)
		kalMemCopy(prWifiCmd->aucBuffer, pucInfoBuffer, u4SetQueryInfoLen);
	/* insert into prCmdQueue */
	kalEnqueueCommand(prGlueInfo, (P_QUE_ENTRY_T) prCmdInfo);

	/* wakeup txServiceThread later */
	GLUE_SET_EVENT(prGlueInfo);
	return WLAN_STATUS_PENDING;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set a key to Wi-Fi Direct driver
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[in] pvSetBuffer A pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                          bytes read from the set buffer. If the call failed
*                          due to invalid length of the set buffer, returns
*                          the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_ADAPTER_NOT_READY
* \retval WLAN_STATUS_INVALID_LENGTH
* \retval WLAN_STATUS_INVALID_DATA
*/
/*----------------------------------------------------------------------------*/
#if 0
WLAN_STATUS
wlanoidSetAddP2PKey(IN P_ADAPTER_T prAdapter,
		    IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	CMD_802_11_KEY rCmdKey;
	P_PARAM_KEY_T prNewKey;
	P_BSS_INFO_T prBssInfo = (P_BSS_INFO_T) NULL;
	P_STA_RECORD_T prStaRec = (P_STA_RECORD_T) NULL;

	DEBUGFUNC("wlanoidSetAddP2PKey");
	DBGLOG(REQ, INFO, "\n");

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	prNewKey = (P_PARAM_KEY_T) pvSetBuffer;

	/* Verify the key structure length. */
	if (prNewKey->u4Length > u4SetBufferLen) {
		DBGLOG(REQ, WARN,
		       "Invalid key structure length (%d) greater than total buffer length (%d)\n",
		       (UINT_8) prNewKey->u4Length, (UINT_8) u4SetBufferLen);

		*pu4SetInfoLen = u4SetBufferLen;
		return WLAN_STATUS_INVALID_LENGTH;
	}
	/* Verify the key material length for key material buffer */
	else if (prNewKey->u4KeyLength > prNewKey->u4Length - OFFSET_OF(PARAM_KEY_T, aucKeyMaterial)) {
		DBGLOG(REQ, WARN, "Invalid key material length (%d)\n", (UINT_8) prNewKey->u4KeyLength);
		*pu4SetInfoLen = u4SetBufferLen;
		return WLAN_STATUS_INVALID_DATA;
	}
	/* Exception check */
	else if (prNewKey->u4KeyIndex & 0x0fffff00)
		return WLAN_STATUS_INVALID_DATA;
	/* Exception check, pairwise key must with transmit bit enabled */
	else if ((prNewKey->u4KeyIndex & BITS(30, 31)) == IS_UNICAST_KEY) {
		return WLAN_STATUS_INVALID_DATA;
	} else if (!(prNewKey->u4KeyLength == CCMP_KEY_LEN)
		   && !(prNewKey->u4KeyLength == TKIP_KEY_LEN)) {
		return WLAN_STATUS_INVALID_DATA;
	}
	/* Exception check, pairwise key must with transmit bit enabled */
	else if ((prNewKey->u4KeyIndex & BITS(30, 31)) == BITS(30, 31)) {
		if (((prNewKey->u4KeyIndex & 0xff) != 0) ||
		    ((prNewKey->arBSSID[0] == 0xff) && (prNewKey->arBSSID[1] == 0xff)
		     && (prNewKey->arBSSID[2] == 0xff) && (prNewKey->arBSSID[3] == 0xff)
		     && (prNewKey->arBSSID[4] == 0xff) && (prNewKey->arBSSID[5] == 0xff))) {
			return WLAN_STATUS_INVALID_DATA;
		}
	}

	*pu4SetInfoLen = u4SetBufferLen;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prNewKey->ucBssIdx);
	ASSERT(prBssInfo);
#if 0
	if (prBssInfo->ucBMCWlanIndex >= WTBL_SIZE) {
		prBssInfo->ucBMCWlanIndex =
		    secPrivacySeekForBcEntry(prAdapter, prBssInfo->ucBssIndex, prBssInfo->aucBSSID,
					     0xff, CIPHER_SUITE_NONE, 0xff);
	}
#endif
	/* fill CMD_802_11_KEY */
	kalMemZero(&rCmdKey, sizeof(CMD_802_11_KEY));
	rCmdKey.ucAddRemove = 1;	/* add */
	rCmdKey.ucTxKey = ((prNewKey->u4KeyIndex & IS_TRANSMIT_KEY) == IS_TRANSMIT_KEY) ? 1 : 0;
	rCmdKey.ucKeyType = ((prNewKey->u4KeyIndex & IS_UNICAST_KEY) == IS_UNICAST_KEY) ? 1 : 0;
#if 0
	if (prBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE) {	/* group client */
#else
	if (kalP2PGetRole(prAdapter->prGlueInfo) == 1) {	/* group client */
#endif

		rCmdKey.ucIsAuthenticator = 0;
	} else {		/* group owner */
		rCmdKey.ucIsAuthenticator = 1;
		/* Force to set GO/AP Tx */
		rCmdKey.ucTxKey = 1;
	}

	COPY_MAC_ADDR(rCmdKey.aucPeerAddr, prNewKey->arBSSID);
	rCmdKey.ucBssIdx = prNewKey->ucBssIdx;
	if (prNewKey->u4KeyLength == CCMP_KEY_LEN)
		rCmdKey.ucAlgorithmId = CIPHER_SUITE_CCMP;	/* AES */
	else if (prNewKey->u4KeyLength == TKIP_KEY_LEN)
		rCmdKey.ucAlgorithmId = CIPHER_SUITE_TKIP;	/* TKIP */
	else if (prNewKey->u4KeyLength == WEP_40_LEN)
		rCmdKey.ucAlgorithmId = CIPHER_SUITE_WEP40;	/* WEP 40 */
	else if (prNewKey->u4KeyLength == WEP_104_LEN)
		rCmdKey.ucAlgorithmId = CIPHER_SUITE_WEP104;	/* WEP 104 */
	else
		ASSERT(FALSE);
	rCmdKey.ucKeyId = (UINT_8) (prNewKey->u4KeyIndex & 0xff);
	rCmdKey.ucKeyLen = (UINT_8) prNewKey->u4KeyLength;
	kalMemCopy(rCmdKey.aucKeyMaterial, (PUINT_8) prNewKey->aucKeyMaterial, rCmdKey.ucKeyLen);

	if ((rCmdKey.aucPeerAddr[0] & rCmdKey.aucPeerAddr[1] & rCmdKey.aucPeerAddr[2] &
	     rCmdKey.aucPeerAddr[3] & rCmdKey.aucPeerAddr[4] & rCmdKey.aucPeerAddr[5]) == 0xFF) {
		kalMemCopy(rCmdKey.aucPeerAddr, prBssInfo->aucBSSID, MAC_ADDR_LEN);
		if (!rCmdKey.ucIsAuthenticator) {
			prStaRec = cnmGetStaRecByAddress(prAdapter, rCmdKey.ucBssIdx, rCmdKey.aucPeerAddr);
			if (!prStaRec)
				ASSERT(FALSE);
		}
	} else {
		prStaRec = cnmGetStaRecByAddress(prAdapter, rCmdKey.ucBssIdx, rCmdKey.aucPeerAddr);
	}

	if (rCmdKey.ucTxKey) {
		if (prStaRec) {
			if (rCmdKey.ucKeyType) {	/* RSN STA */
				ASSERT(prStaRec->ucWlanIndex < WTBL_SIZE);
				rCmdKey.ucWlanIndex = prStaRec->ucWlanIndex;
				prStaRec->fgTransmitKeyExist = TRUE;	/* wait for CMD Done ? */
			} else {
				ASSERT(FALSE);
			}
		} else {
			if (prBssInfo) {	/* GO/AP Tx BC */
				ASSERT(prBssInfo->ucBMCWlanIndex < WTBL_SIZE);
				rCmdKey.ucWlanIndex = prBssInfo->ucBMCWlanIndex;
				prBssInfo->fgBcDefaultKeyExist = TRUE;
				prBssInfo->ucTxDefaultKeyID = rCmdKey.ucKeyId;
			} else {
				rCmdKey.ucWlanIndex = 255;	/* GC WEP Tx key ? */
				ASSERT(FALSE);
			}
		}
	} else {
		if (((rCmdKey.aucPeerAddr[0] & rCmdKey.aucPeerAddr[1] & rCmdKey.aucPeerAddr[2] &
		      rCmdKey.aucPeerAddr[3] & rCmdKey.aucPeerAddr[4] & rCmdKey.aucPeerAddr[5]) == 0xFF)
		    ||
		    ((rCmdKey.aucPeerAddr[0] | rCmdKey.aucPeerAddr[1] | rCmdKey.
		      aucPeerAddr[2] | rCmdKey.aucPeerAddr[3] | rCmdKey.aucPeerAddr[4] | rCmdKey.aucPeerAddr[5]) ==
		     0x00)) {
			rCmdKey.ucWlanIndex = 255;	/* GC WEP ? */
			ASSERT(FALSE);
		} else {
			if (prStaRec) {	/* GC Rx RSN Group key */
				rCmdKey.ucWlanIndex =
				    secPrivacySeekForBcEntry(prAdapter, prStaRec->ucBssIndex,
							     prStaRec->aucMacAddr,
							     prStaRec->ucIndex, rCmdKey.ucAlgorithmId, rCmdKey.ucKeyId);
				prStaRec->ucBMCWlanIndex = rCmdKey.ucWlanIndex;
				ASSERT(prStaRec->ucBMCWlanIndex < WTBL_SIZE);
			} else {	/* Exist this case ? */
				ASSERT(FALSE);
			}
		}
	}

	return wlanoidSendSetQueryP2PCmd(prAdapter,
					 CMD_ID_ADD_REMOVE_KEY,
					 prNewKey->ucBssIdx,
					 TRUE,
					 FALSE,
					 TRUE,
					 nicCmdEventSetCommon,
					 NULL,
					 sizeof(CMD_802_11_KEY), (PUINT_8)&rCmdKey, pvSetBuffer, u4SetBufferLen);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to request Wi-Fi Direct driver to remove keys
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[in] pvSetBuffer A pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                          bytes read from the set buffer. If the call failed
*                          due to invalid length of the set buffer, returns
*                          the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_DATA
* \retval WLAN_STATUS_INVALID_LENGTH
* \retval WLAN_STATUS_INVALID_DATA
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetRemoveP2PKey(IN P_ADAPTER_T prAdapter,
		       IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	CMD_802_11_KEY rCmdKey;
	P_PARAM_REMOVE_KEY_T prRemovedKey;
	P_BSS_INFO_T prBssInfo = (P_BSS_INFO_T) NULL;
	P_STA_RECORD_T prStaRec = (P_STA_RECORD_T) NULL;

	DEBUGFUNC("wlanoidSetRemoveP2PKey");
	ASSERT(prAdapter);

	if (u4SetBufferLen < sizeof(PARAM_REMOVE_KEY_T))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);
	prRemovedKey = (P_PARAM_REMOVE_KEY_T) pvSetBuffer;

	/* Check bit 31: this bit should always 0 */
	if (prRemovedKey->u4KeyIndex & IS_TRANSMIT_KEY) {
		/* Bit 31 should not be set */
		DBGLOG(REQ, ERROR, "invalid key index: 0x%08lx\n", prRemovedKey->u4KeyIndex);
		return WLAN_STATUS_INVALID_DATA;
	}

	/* Check bits 8 ~ 29 should always be 0 */
	if (prRemovedKey->u4KeyIndex & BITS(8, 29)) {
		/* Bit 31 should not be set */
		DBGLOG(REQ, ERROR, "invalid key index: 0x%08lx\n", prRemovedKey->u4KeyIndex);
		return WLAN_STATUS_INVALID_DATA;
	}

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prRemovedKey->ucBssIdx);

	kalMemZero((PUINT_8)&rCmdKey, sizeof(CMD_802_11_KEY));

	rCmdKey.ucAddRemove = 0;	/* remove */
	if (prBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE) {	/* group client */
		rCmdKey.ucIsAuthenticator = 0;
	} else {		/* group owner */
		rCmdKey.ucIsAuthenticator = 1;
	}
	kalMemCopy(rCmdKey.aucPeerAddr, (PUINT_8) prRemovedKey->arBSSID, MAC_ADDR_LEN);
	rCmdKey.ucBssIdx = prRemovedKey->ucBssIdx;
	rCmdKey.ucKeyId = (UINT_8) (prRemovedKey->u4KeyIndex & 0x000000ff);

	/* Clean up the Tx key flag */
	prStaRec = cnmGetStaRecByAddress(prAdapter, prRemovedKey->ucBssIdx, prRemovedKey->arBSSID);

	/* mark for MR1 to avoid remove-key, but remove the wlan_tbl0 at the same time */
	if (1 /*prRemovedKey->u4KeyIndex & IS_UNICAST_KEY */) {
		if (prStaRec) {
			rCmdKey.ucKeyType = 1;
			rCmdKey.ucWlanIndex = prStaRec->ucWlanIndex;
			prStaRec->fgTransmitKeyExist = FALSE;
		} else if (rCmdKey.ucIsAuthenticator)
			prBssInfo->fgBcDefaultKeyExist = FALSE;
	} else {
		if (rCmdKey.ucIsAuthenticator)
			prBssInfo->fgBcDefaultKeyExist = FALSE;
	}

	if (!prStaRec) {
		if (prAdapter->rWifiVar.rConnSettings.eAuthMode < AUTH_MODE_WPA
		    && prAdapter->rWifiVar.rConnSettings.eEncStatus != ENUM_ENCRYPTION_DISABLED) {
			rCmdKey.ucWlanIndex = prBssInfo->ucBMCWlanIndex;
		} else {
			rCmdKey.ucWlanIndex = WTBL_RESERVED_ENTRY;
			return WLAN_STATUS_SUCCESS;
		}
	}

	/* mark for MR1 to avoid remove-key, but remove the wlan_tbl0 at the same time */
	/* secPrivacyFreeForEntry(prAdapter, rCmdKey.ucWlanIndex); */

	return wlanoidSendSetQueryP2PCmd(prAdapter,
					 CMD_ID_ADD_REMOVE_KEY,
					 prRemovedKey->ucBssIdx,
					 TRUE,
					 FALSE,
					 TRUE,
					 nicCmdEventSetCommon,
					 NULL,
					 sizeof(CMD_802_11_KEY), (PUINT_8)&rCmdKey, pvSetBuffer, u4SetBufferLen);
}
#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief Setting the IP address for pattern search function.
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[in] pvSetBuffer A pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                           bytes read from the set buffer. If the call failed
*                           due to invalid length of the set buffer, returns
*                           the amount of storage needed.
*
* \return WLAN_STATUS_SUCCESS
* \return WLAN_STATUS_ADAPTER_NOT_READY
* \return WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetP2pNetworkAddress(IN P_ADAPTER_T prAdapter,
			    IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 i, j;
	P_CMD_SET_NETWORK_ADDRESS_LIST prCmdNetworkAddressList;
	P_PARAM_NETWORK_ADDRESS_LIST prNetworkAddressList = (P_PARAM_NETWORK_ADDRESS_LIST) pvSetBuffer;
	P_PARAM_NETWORK_ADDRESS prNetworkAddress;
	P_PARAM_NETWORK_ADDRESS_IP prNetAddrIp;
	UINT_32 u4IpAddressCount, u4CmdSize;

	DEBUGFUNC("wlanoidSetP2pNetworkAddress");
	DBGLOG(INIT, TRACE, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = 4;

	if (u4SetBufferLen < sizeof(PARAM_NETWORK_ADDRESS_LIST))
		return WLAN_STATUS_INVALID_DATA;

	*pu4SetInfoLen = 0;
	u4IpAddressCount = 0;

	prNetworkAddress = prNetworkAddressList->arAddress;
	for (i = 0; i < prNetworkAddressList->u4AddressCount; i++) {
		if (prNetworkAddress->u2AddressType == PARAM_PROTOCOL_ID_TCP_IP &&
		    prNetworkAddress->u2AddressLength == sizeof(PARAM_NETWORK_ADDRESS_IP)) {
			u4IpAddressCount++;
		}

		prNetworkAddress = (P_PARAM_NETWORK_ADDRESS) ((ULONG) prNetworkAddress +
							      (ULONG) (prNetworkAddress->u2AddressLength +
								       OFFSET_OF(PARAM_NETWORK_ADDRESS, aucAddress)));
	}

	/* construct payload of command packet */
	u4CmdSize = OFFSET_OF(CMD_SET_NETWORK_ADDRESS_LIST, arNetAddress) +
	    sizeof(IPV4_NETWORK_ADDRESS) * u4IpAddressCount;

	prCmdNetworkAddressList = (P_CMD_SET_NETWORK_ADDRESS_LIST) kalMemAlloc(u4CmdSize, VIR_MEM_TYPE);

	if (prCmdNetworkAddressList == NULL)
		return WLAN_STATUS_FAILURE;

	/* fill P_CMD_SET_NETWORK_ADDRESS_LIST */
	prCmdNetworkAddressList->ucBssIndex = prNetworkAddressList->ucBssIdx;
	prCmdNetworkAddressList->ucAddressCount = (UINT_8) u4IpAddressCount;
	prNetworkAddress = prNetworkAddressList->arAddress;
	for (i = 0, j = 0; i < prNetworkAddressList->u4AddressCount; i++) {
		if (prNetworkAddress->u2AddressType == PARAM_PROTOCOL_ID_TCP_IP &&
		    prNetworkAddress->u2AddressLength == sizeof(PARAM_NETWORK_ADDRESS_IP)) {
			prNetAddrIp = (P_PARAM_NETWORK_ADDRESS_IP) prNetworkAddress->aucAddress;

			kalMemCopy(prCmdNetworkAddressList->arNetAddress[j].aucIpAddr,
				   &(prNetAddrIp->in_addr), sizeof(UINT_32));

			j++;
		}

		prNetworkAddress = (P_PARAM_NETWORK_ADDRESS) ((ULONG) prNetworkAddress +
							      (ULONG) (prNetworkAddress->u2AddressLength +
								       OFFSET_OF(PARAM_NETWORK_ADDRESS, aucAddress)));
	}

	rStatus = wlanSendSetQueryCmd(prAdapter,
				      CMD_ID_SET_IP_ADDRESS,
				      TRUE,
				      FALSE,
				      TRUE,
				      nicCmdEventSetIpAddress,
				      nicOidCmdTimeoutCommon,
				      u4CmdSize, (PUINT_8) prCmdNetworkAddressList, pvSetBuffer, u4SetBufferLen);

	kalMemFree(prCmdNetworkAddressList, VIR_MEM_TYPE, u4CmdSize);
	return rStatus;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to query the power save profile.
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[out] pvQueryBuf A pointer to the buffer that holds the result of
*                           the query.
* \param[in] u4QueryBufLen The length of the query buffer.
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                            bytes written into the query buffer. If the call
*                            failed due to invalid length of the query buffer,
*                            returns the amount of storage needed.
*
* \return WLAN_STATUS_SUCCESS
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryP2pPowerSaveProfile(IN P_ADAPTER_T prAdapter,
				IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	DEBUGFUNC("wlanoidQueryP2pPowerSaveProfile");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);

	if (u4QueryBufferLen != 0) {
		ASSERT(pvQueryBuffer);
		/* TODO: FIXME */
		/* *(PPARAM_POWER_MODE) pvQueryBuffer =
		 * (PARAM_POWER_MODE)(prAdapter->rWlanInfo.arPowerSaveMode[P2P_DEV_BSS_INDEX].ucPsProfile);
		 */
		/* *pu4QueryInfoLen = sizeof(PARAM_POWER_MODE); */
	}

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to set the power save profile.
*
* \param[in] pvAdapter Pointer to the Adapter structure.
* \param[in] pvSetBuffer A pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                          bytes read from the set buffer. If the call failed
*                          due to invalid length of the set buffer, returns
*                          the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetP2pPowerSaveProfile(IN P_ADAPTER_T prAdapter,
			      IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	WLAN_STATUS status;
	PARAM_POWER_MODE ePowerMode;

	DEBUGFUNC("wlanoidSetP2pPowerSaveProfile");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(PARAM_POWER_MODE);
	if (u4SetBufferLen < sizeof(PARAM_POWER_MODE)) {
		DBGLOG(REQ, WARN, "Invalid length %u\n", u4SetBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	} else if (*(PPARAM_POWER_MODE) pvSetBuffer >= Param_PowerModeMax) {
		DBGLOG(REQ, WARN, "Invalid power mode %d\n", *(PPARAM_POWER_MODE) pvSetBuffer);
		return WLAN_STATUS_INVALID_DATA;
	}

	ePowerMode = *(PPARAM_POWER_MODE) pvSetBuffer;

	if (prAdapter->fgEnCtiaPowerMode) {
		if (ePowerMode == Param_PowerModeCAM) {
			/*Todo::  Nothing */
			/*Todo::  Nothing */
		} else {
			/* User setting to PS mode (Param_PowerModeMAX_PSP or Param_PowerModeFast_PSP) */

			if (prAdapter->u4CtiaPowerMode == 0) {
				/* force to keep in CAM mode */
				ePowerMode = Param_PowerModeCAM;
			} else if (prAdapter->u4CtiaPowerMode == 1) {
				ePowerMode = Param_PowerModeMAX_PSP;
			} else if (prAdapter->u4CtiaPowerMode == 2) {
				ePowerMode = Param_PowerModeFast_PSP;
			}
		}
	}

	status = nicConfigPowerSaveProfile(prAdapter, P2P_DEV_BSS_INDEX,	/* TODO: FIXME */
					   ePowerMode, TRUE);
	return status;
}				/* end of wlanoidSetP2pPowerSaveProfile() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to set the power save profile.
*
* \param[in] pvAdapter Pointer to the Adapter structure.
* \param[in] pvSetBuffer A pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                          bytes read from the set buffer. If the call failed
*                          due to invalid length of the set buffer, returns
*                          the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetP2pSetNetworkAddress(IN P_ADAPTER_T prAdapter,
			       IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 i, j;
	P_CMD_SET_NETWORK_ADDRESS_LIST prCmdNetworkAddressList;
	P_PARAM_NETWORK_ADDRESS_LIST prNetworkAddressList = (P_PARAM_NETWORK_ADDRESS_LIST) pvSetBuffer;
	P_PARAM_NETWORK_ADDRESS prNetworkAddress;
	P_PARAM_NETWORK_ADDRESS_IP prNetAddrIp;
	UINT_32 u4IpAddressCount, u4CmdSize;
	PUINT_8 pucBuf = (PUINT_8) pvSetBuffer;

	DEBUGFUNC("wlanoidSetP2pSetNetworkAddress");
	DBGLOG(INIT, TRACE, "\n");
	DBGLOG(INIT, INFO, "wlanoidSetP2pSetNetworkAddress (%d)\n", (INT_16) u4SetBufferLen);

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = 4;

	if (u4SetBufferLen < sizeof(PARAM_NETWORK_ADDRESS_LIST))
		return WLAN_STATUS_INVALID_DATA;

	*pu4SetInfoLen = 0;
	u4IpAddressCount = 0;

	prNetworkAddress = prNetworkAddressList->arAddress;
	for (i = 0; i < prNetworkAddressList->u4AddressCount; i++) {
		if (prNetworkAddress->u2AddressType == PARAM_PROTOCOL_ID_TCP_IP &&
		    prNetworkAddress->u2AddressLength == sizeof(PARAM_NETWORK_ADDRESS_IP)) {
			u4IpAddressCount++;
		}

		prNetworkAddress = (P_PARAM_NETWORK_ADDRESS) ((ULONG) prNetworkAddress +
							      (ULONG) (prNetworkAddress->u2AddressLength +
								       OFFSET_OF(PARAM_NETWORK_ADDRESS, aucAddress)));
	}

	/* construct payload of command packet */
	u4CmdSize = OFFSET_OF(CMD_SET_NETWORK_ADDRESS_LIST, arNetAddress) +
	    sizeof(IPV4_NETWORK_ADDRESS) * u4IpAddressCount;

	if (u4IpAddressCount == 0)
		u4CmdSize = sizeof(CMD_SET_NETWORK_ADDRESS_LIST);

	prCmdNetworkAddressList = (P_CMD_SET_NETWORK_ADDRESS_LIST) kalMemAlloc(u4CmdSize, VIR_MEM_TYPE);

	if (prCmdNetworkAddressList == NULL)
		return WLAN_STATUS_FAILURE;

	/* fill P_CMD_SET_NETWORK_ADDRESS_LIST */
	prCmdNetworkAddressList->ucBssIndex = prNetworkAddressList->ucBssIdx;

	/* only to set IP address to FW once ARP filter is enabled */
	if (prAdapter->fgEnArpFilter) {
		prCmdNetworkAddressList->ucAddressCount = (UINT_8) u4IpAddressCount;
		prNetworkAddress = prNetworkAddressList->arAddress;

		DBGLOG(INIT, INFO, "u4IpAddressCount (%u)\n",
				(INT_32) u4IpAddressCount);
		for (i = 0, j = 0; i < prNetworkAddressList->u4AddressCount; i++) {
			if (prNetworkAddress->u2AddressType == PARAM_PROTOCOL_ID_TCP_IP &&
			    prNetworkAddress->u2AddressLength == sizeof(PARAM_NETWORK_ADDRESS_IP)) {
				prNetAddrIp = (P_PARAM_NETWORK_ADDRESS_IP) prNetworkAddress->aucAddress;

				kalMemCopy(prCmdNetworkAddressList->arNetAddress[j].aucIpAddr,
					   &(prNetAddrIp->in_addr), sizeof(UINT_32));

				j++;

				pucBuf = (PUINT_8) &prNetAddrIp->in_addr;
				DBGLOG(INIT, INFO, "prNetAddrIp->in_addr:%d:%d:%d:%d\n",
				       (UINT_8) pucBuf[0], (UINT_8) pucBuf[1], (UINT_8) pucBuf[2], (UINT_8) pucBuf[3]);
			}

			prNetworkAddress = (P_PARAM_NETWORK_ADDRESS) ((ULONG) prNetworkAddress +
								      (ULONG) (prNetworkAddress->u2AddressLength +
									       OFFSET_OF
									       (PARAM_NETWORK_ADDRESS, aucAddress)));
		}

	} else {
		prCmdNetworkAddressList->ucAddressCount = 0;
	}

	rStatus = wlanSendSetQueryCmd(prAdapter,
				      CMD_ID_SET_IP_ADDRESS,
				      TRUE,
				      FALSE,
				      TRUE,
				      nicCmdEventSetIpAddress,
				      nicOidCmdTimeoutCommon,
				      u4CmdSize, (PUINT_8) prCmdNetworkAddressList, pvSetBuffer, u4SetBufferLen);

	kalMemFree(prCmdNetworkAddressList, VIR_MEM_TYPE, u4CmdSize);
	return rStatus;
}				/* end of wlanoidSetP2pSetNetworkAddress() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set Multicast Address List.
*
* \param[in] prAdapter      Pointer to the Adapter structure.
* \param[in] pvSetBuffer    Pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                           bytes read from the set buffer. If the call failed
*                           due to invalid length of the set buffer, returns
*                           the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
* \retval WLAN_STATUS_ADAPTER_NOT_READY
* \retval WLAN_STATUS_MULTICAST_FULL
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetP2PMulticastList(IN P_ADAPTER_T prAdapter,
			   IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	CMD_MAC_MCAST_ADDR rCmdMacMcastAddr;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	/* The data must be a multiple of the Ethernet address size. */
	if ((u4SetBufferLen % MAC_ADDR_LEN)) {
		DBGLOG(REQ, WARN, "Invalid MC list length %u\n",
				u4SetBufferLen);

		*pu4SetInfoLen = (((u4SetBufferLen + MAC_ADDR_LEN) - 1) / MAC_ADDR_LEN) * MAC_ADDR_LEN;

		return WLAN_STATUS_INVALID_LENGTH;
	}

	*pu4SetInfoLen = u4SetBufferLen;

	/* Verify if we can support so many multicast addresses. */
	if (u4SetBufferLen > MAX_NUM_GROUP_ADDR * MAC_ADDR_LEN) {
		DBGLOG(REQ, WARN, "Too many MC addresses\n");

		return WLAN_STATUS_MULTICAST_FULL;
	}

	/* NOTE(Kevin): Windows may set u4SetBufferLen == 0 &&
	 * pvSetBuffer == NULL to clear exist Multicast List.
	 */
	if (u4SetBufferLen)
		ASSERT(pvSetBuffer);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in set multicast list! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	rCmdMacMcastAddr.u4NumOfGroupAddr = u4SetBufferLen / MAC_ADDR_LEN;
	rCmdMacMcastAddr.ucBssIndex = P2P_DEV_BSS_INDEX;	/* TODO: */
	kalMemCopy(rCmdMacMcastAddr.arAddress, pvSetBuffer, u4SetBufferLen);

	return wlanoidSendSetQueryP2PCmd(prAdapter, CMD_ID_MAC_MCAST_ADDR, P2P_DEV_BSS_INDEX,
				/* TODO: */
				/* This CMD response is no need to complete the OID. Or the event would unsync. */
					 TRUE, FALSE, FALSE,
					 nicCmdEventSetCommon,
					 nicOidCmdTimeoutCommon,
					 sizeof(CMD_MAC_MCAST_ADDR),
					 (PUINT_8) &rCmdMacMcastAddr, pvSetBuffer, u4SetBufferLen);

}				/* end of wlanoidSetP2PMulticastList() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to send GAS frame for P2P Service Discovery Request
*
* \param[in] prAdapter      Pointer to the Adapter structure.
* \param[in] pvSetBuffer    Pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                           bytes read from the set buffer. If the call failed
*                           due to invalid length of the set buffer, returns
*                           the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
* \retval WLAN_STATUS_ADAPTER_NOT_READY
* \retval WLAN_STATUS_MULTICAST_FULL
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSendP2PSDRequest(IN P_ADAPTER_T prAdapter,
			IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	if (u4SetBufferLen)
		ASSERT(pvSetBuffer);

	if (u4SetBufferLen < sizeof(PARAM_P2P_SEND_SD_REQUEST)) {
		*pu4SetInfoLen = sizeof(PARAM_P2P_SEND_SD_REQUEST);
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}
/* rWlanStatus = p2pFsmRunEventSDRequest(prAdapter, (P_PARAM_P2P_SEND_SD_REQUEST)pvSetBuffer); */

	return rWlanStatus;
}				/* end of wlanoidSendP2PSDRequest() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to send GAS frame for P2P Service Discovery Response
*
* \param[in] prAdapter      Pointer to the Adapter structure.
* \param[in] pvSetBuffer    Pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                           bytes read from the set buffer. If the call failed
*                           due to invalid length of the set buffer, returns
*                           the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
* \retval WLAN_STATUS_ADAPTER_NOT_READY
* \retval WLAN_STATUS_MULTICAST_FULL
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSendP2PSDResponse(IN P_ADAPTER_T prAdapter,
			 IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	if (u4SetBufferLen)
		ASSERT(pvSetBuffer);

	if (u4SetBufferLen < sizeof(PARAM_P2P_SEND_SD_RESPONSE)) {
		*pu4SetInfoLen = sizeof(PARAM_P2P_SEND_SD_RESPONSE);
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}
/* rWlanStatus = p2pFsmRunEventSDResponse(prAdapter, (P_PARAM_P2P_SEND_SD_RESPONSE)pvSetBuffer); */

	return rWlanStatus;
}				/* end of wlanoidGetP2PSDRequest() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to get GAS frame for P2P Service Discovery Request
*
* \param[in]  prAdapter        Pointer to the Adapter structure.
* \param[out] pvQueryBuffer    A pointer to the buffer that holds the result of
*                              the query.
* \param[in]  u4QueryBufferLen The length of the query buffer.
* \param[out] pu4QueryInfoLen  If the call is successful, returns the number of
*                              bytes written into the query buffer. If the call
*                              failed due to invalid length of the query buffer,
*                              returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
* \retval WLAN_STATUS_ADAPTER_NOT_READY
* \retval WLAN_STATUS_MULTICAST_FULL
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidGetP2PSDRequest(IN P_ADAPTER_T prAdapter,
		       IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;
/* PUINT_8 pucChannelNum = NULL; */
/* UINT_8 ucChannelNum = 0, ucSeqNum = 0; */

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);

	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (u4QueryBufferLen < sizeof(PARAM_P2P_GET_SD_REQUEST)) {
		*pu4QueryInfoLen = sizeof(PARAM_P2P_GET_SD_REQUEST);
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}

	DBGLOG(P2P, TRACE, "Get Service Discovery Request\n");

	*pu4QueryInfoLen = 0;
	return rWlanStatus;
}				/* end of wlanoidGetP2PSDRequest() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to get GAS frame for P2P Service Discovery Response
*
* \param[in]  prAdapter        Pointer to the Adapter structure.
* \param[out] pvQueryBuffer    A pointer to the buffer that holds the result of
*                              the query.
* \param[in]  u4QueryBufferLen The length of the query buffer.
* \param[out] pu4QueryInfoLen  If the call is successful, returns the number of
*                              bytes written into the query buffer. If the call
*                              failed due to invalid length of the query buffer,
*                              returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
* \retval WLAN_STATUS_ADAPTER_NOT_READY
* \retval WLAN_STATUS_MULTICAST_FULL
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidGetP2PSDResponse(IN P_ADAPTER_T prAdapter,
			IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;
	/* UINT_8 ucSeqNum = 0, */

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);

	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (u4QueryBufferLen < sizeof(PARAM_P2P_GET_SD_RESPONSE)) {
		*pu4QueryInfoLen = sizeof(PARAM_P2P_GET_SD_RESPONSE);
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}

	DBGLOG(P2P, TRACE, "Get Service Discovery Response\n");

	*pu4QueryInfoLen = 0;
	return rWlanStatus;
}				/* end of wlanoidGetP2PSDResponse() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to terminate P2P Service Discovery Phase
*
* \param[in] prAdapter      Pointer to the Adapter structure.
* \param[in] pvSetBuffer    Pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                           bytes read from the set buffer. If the call failed
*                           due to invalid length of the set buffer, returns
*                           the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
* \retval WLAN_STATUS_ADAPTER_NOT_READY
* \retval WLAN_STATUS_MULTICAST_FULL
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetP2PTerminateSDPhase(IN P_ADAPTER_T prAdapter,
			      IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;
	P_PARAM_P2P_TERMINATE_SD_PHASE prP2pTerminateSD = (P_PARAM_P2P_TERMINATE_SD_PHASE) NULL;
	UINT_8 aucNullAddr[] = NULL_MAC_ADDR;

	do {
		if ((prAdapter == NULL) || (pu4SetInfoLen == NULL))
			break;

		if ((u4SetBufferLen) && (pvSetBuffer == NULL))
			break;

		if (u4SetBufferLen < sizeof(PARAM_P2P_TERMINATE_SD_PHASE)) {
			*pu4SetInfoLen = sizeof(PARAM_P2P_TERMINATE_SD_PHASE);
			rWlanStatus = WLAN_STATUS_BUFFER_TOO_SHORT;
			break;
		}

		prP2pTerminateSD = (P_PARAM_P2P_TERMINATE_SD_PHASE) pvSetBuffer;

		if (EQUAL_MAC_ADDR(prP2pTerminateSD->rPeerAddr, aucNullAddr)) {
			DBGLOG(P2P, TRACE, "Service Discovery Version 2.0\n");
/* p2pFuncSetVersionNumOfSD(prAdapter, 2); */
		}
		/* rWlanStatus = p2pFsmRunEventSDAbort(prAdapter); */

	} while (FALSE);

	return rWlanStatus;
}				/* end of wlanoidSetP2PTerminateSDPhase() */

#if CFG_SUPPORT_ANTI_PIRACY
/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to
*
* \param[in] prAdapter      Pointer to the Adapter structure.
* \param[in] pvSetBuffer    Pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                           bytes read from the set buffer. If the call failed
*                           due to invalid length of the set buffer, returns
*                           the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
* \retval WLAN_STATUS_ADAPTER_NOT_READY
* \retval WLAN_STATUS_MULTICAST_FULL
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetSecCheckRequest(IN P_ADAPTER_T prAdapter,
			  IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	if (u4SetBufferLen)
		ASSERT(pvSetBuffer);

#if 0  /* Comment it because CMD_ID_SEC_CHECK is not defined */
	return wlanoidSendSetQueryP2PCmd(prAdapter,
					 CMD_ID_SEC_CHECK,
					 P2P_DEV_BSS_INDEX,
					 FALSE,
					 TRUE,
					 TRUE,
					 NULL,
					 nicOidCmdTimeoutCommon,
					 u4SetBufferLen, (PUINT_8) pvSetBuffer, pvSetBuffer, u4SetBufferLen);
#else
	return WLAN_STATUS_NOT_SUPPORTED;
#endif
}				/* end of wlanoidSetSecCheckRequest() */

#if 0
/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to
*
* \param[in]  prAdapter        Pointer to the Adapter structure.
* \param[out] pvQueryBuffer    A pointer to the buffer that holds the result of
*                              the query.
* \param[in]  u4QueryBufferLen The length of the query buffer.
* \param[out] pu4QueryInfoLen  If the call is successful, returns the number of
*                              bytes written into the query buffer. If the call
*                              failed due to invalid length of the query buffer,
*                              returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
* \retval WLAN_STATUS_ADAPTER_NOT_READY
* \retval WLAN_STATUS_MULTICAST_FULL
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidGetSecCheckResponse(IN P_ADAPTER_T prAdapter,
			   IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;
	/* P_WLAN_MAC_HEADER_T prWlanHdr = (P_WLAN_MAC_HEADER_T)NULL; */
	P_GLUE_INFO_T prGlueInfo;

	prGlueInfo = prAdapter->prGlueInfo;

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);

	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (u4QueryBufferLen > 256)
		u4QueryBufferLen = 256;

	*pu4QueryInfoLen = u4QueryBufferLen;

#if DBG
	DBGLOG_MEM8(SEC, LOUD, prGlueInfo->prP2PInfo[0]->aucSecCheckRsp, u4QueryBufferLen);
#endif
	kalMemCopy((PUINT_8) (pvQueryBuffer + OFFSET_OF(IW_P2P_TRANSPORT_STRUCT, aucBuffer)),
		   prGlueInfo->prP2PInfo[0]->aucSecCheckRsp, u4QueryBufferLen);

	return rWlanStatus;
}				/* end of wlanoidGetSecCheckResponse() */
#endif
#endif

WLAN_STATUS
wlanoidSetNoaParam(IN P_ADAPTER_T prAdapter,
		   IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_PARAM_CUSTOM_NOA_PARAM_STRUCT_T prNoaParam;
	CMD_CUSTOM_NOA_PARAM_STRUCT_T rCmdNoaParam;

	DEBUGFUNC("wlanoidSetNoaParam");
	DBGLOG(INIT, TRACE, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(PARAM_CUSTOM_NOA_PARAM_STRUCT_T);

	if (u4SetBufferLen < sizeof(PARAM_CUSTOM_NOA_PARAM_STRUCT_T))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prNoaParam = (P_PARAM_CUSTOM_NOA_PARAM_STRUCT_T) pvSetBuffer;

	kalMemZero(&rCmdNoaParam, sizeof(CMD_CUSTOM_NOA_PARAM_STRUCT_T));
	rCmdNoaParam.u4NoaDurationMs = prNoaParam->u4NoaDurationMs;
	rCmdNoaParam.u4NoaIntervalMs = prNoaParam->u4NoaIntervalMs;
	rCmdNoaParam.u4NoaCount = prNoaParam->u4NoaCount;

#if 0
	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_SET_NOA_PARAM,
				   TRUE,
				   FALSE,
				   TRUE,
				   nicCmdEventSetCommon,
				   nicOidCmdTimeoutCommon,
				   sizeof(CMD_CUSTOM_NOA_PARAM_STRUCT_T),
				   (PUINT_8) &rCmdNoaParam, pvSetBuffer, u4SetBufferLen);
#else
	return wlanoidSendSetQueryP2PCmd(prAdapter,
					 CMD_ID_SET_NOA_PARAM,
					 prNoaParam->ucBssIdx,
					 TRUE,
					 FALSE,
					 g_fgIsOid,
					 NULL,
					 nicOidCmdTimeoutCommon,
					 sizeof(CMD_CUSTOM_NOA_PARAM_STRUCT_T),
					 (PUINT_8) &rCmdNoaParam, pvSetBuffer, u4SetBufferLen);

#endif

}

WLAN_STATUS
wlanoidSetOppPsParam(IN P_ADAPTER_T prAdapter,
		     IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_PARAM_CUSTOM_OPPPS_PARAM_STRUCT_T prOppPsParam;
	CMD_CUSTOM_OPPPS_PARAM_STRUCT_T rCmdOppPsParam;

	DEBUGFUNC("wlanoidSetOppPsParam");
	DBGLOG(INIT, TRACE, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(PARAM_CUSTOM_OPPPS_PARAM_STRUCT_T);

	if (u4SetBufferLen < sizeof(PARAM_CUSTOM_OPPPS_PARAM_STRUCT_T))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prOppPsParam = (P_PARAM_CUSTOM_OPPPS_PARAM_STRUCT_T) pvSetBuffer;

	kalMemZero(&rCmdOppPsParam, sizeof(CMD_CUSTOM_OPPPS_PARAM_STRUCT_T));

	rCmdOppPsParam.ucBssIdx = prOppPsParam->ucBssIdx;

	if (prOppPsParam->ucOppPs) {
		/* [spec. 3.3.2] CTWindow should be at least 10 TU */
		if (prOppPsParam->u4CTwindowMs < 10)
			rCmdOppPsParam.u4CTwindowMs = 10;
		else
			rCmdOppPsParam.u4CTwindowMs = prOppPsParam->u4CTwindowMs;
	} else
		rCmdOppPsParam.u4CTwindowMs = 0;/* Set 0 means disable OppPs */

	return wlanSendSetQueryCmd(prAdapter,
					CMD_ID_SET_OPPPS_PARAM,
					TRUE,
					FALSE,
					g_fgIsOid,
					nicCmdEventSetCommon,
					nicOidCmdTimeoutCommon,
					sizeof(CMD_CUSTOM_OPPPS_PARAM_STRUCT_T),
					(PUINT_8)&rCmdOppPsParam, pvSetBuffer, u4SetBufferLen);
}

WLAN_STATUS
wlanoidSetUApsdParam(IN P_ADAPTER_T prAdapter,
		     IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_PARAM_CUSTOM_UAPSD_PARAM_STRUCT_T prUapsdParam;
	CMD_CUSTOM_UAPSD_PARAM_STRUCT_T rCmdUapsdParam;
	P_PM_PROFILE_SETUP_INFO_T prPmProfSetupInfo;
	P_BSS_INFO_T prBssInfo;

	DEBUGFUNC("wlanoidSetUApsdParam");
	DBGLOG(INIT, TRACE, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(PARAM_CUSTOM_UAPSD_PARAM_STRUCT_T);

	if (u4SetBufferLen < sizeof(PARAM_CUSTOM_UAPSD_PARAM_STRUCT_T))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prUapsdParam = (P_PARAM_CUSTOM_UAPSD_PARAM_STRUCT_T) pvSetBuffer;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prUapsdParam->ucBssIdx);
	prPmProfSetupInfo = &prBssInfo->rPmProfSetupInfo;

	kalMemZero(&rCmdUapsdParam, sizeof(CMD_CUSTOM_OPPPS_PARAM_STRUCT_T));
	rCmdUapsdParam.fgEnAPSD = prUapsdParam->fgEnAPSD;

	rCmdUapsdParam.fgEnAPSD_AcBe = prUapsdParam->fgEnAPSD_AcBe;
	rCmdUapsdParam.fgEnAPSD_AcBk = prUapsdParam->fgEnAPSD_AcBk;
	rCmdUapsdParam.fgEnAPSD_AcVo = prUapsdParam->fgEnAPSD_AcVo;
	rCmdUapsdParam.fgEnAPSD_AcVi = prUapsdParam->fgEnAPSD_AcVi;
	prPmProfSetupInfo->ucBmpDeliveryAC =
	    ((prUapsdParam->fgEnAPSD_AcBe << 0) |
	     (prUapsdParam->fgEnAPSD_AcBk << 1) |
	     (prUapsdParam->fgEnAPSD_AcVi << 2) | (prUapsdParam->fgEnAPSD_AcVo << 3));
	prPmProfSetupInfo->ucBmpTriggerAC =
	    ((prUapsdParam->fgEnAPSD_AcBe << 0) |
	     (prUapsdParam->fgEnAPSD_AcBk << 1) |
	     (prUapsdParam->fgEnAPSD_AcVi << 2) | (prUapsdParam->fgEnAPSD_AcVo << 3));

	rCmdUapsdParam.ucMaxSpLen = prUapsdParam->ucMaxSpLen;
	prPmProfSetupInfo->ucUapsdSp = prUapsdParam->ucMaxSpLen;

#if 0
	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_SET_UAPSD_PARAM,
				   TRUE,
				   FALSE,
				   TRUE,
				   nicCmdEventSetCommon,
				   nicOidCmdTimeoutCommon,
				   sizeof(CMD_CUSTOM_OPPPS_PARAM_STRUCT_T),
				   (PUINT_8) &rCmdUapsdParam, pvSetBuffer, u4SetBufferLen);
#else
	return wlanoidSendSetQueryP2PCmd(prAdapter,
					 CMD_ID_SET_UAPSD_PARAM,
					 prBssInfo->ucBssIndex,
					 TRUE,
					 FALSE,
					 TRUE,
					 NULL,
					 nicOidCmdTimeoutCommon,
					 sizeof(CMD_CUSTOM_OPPPS_PARAM_STRUCT_T),
					 (PUINT_8) &rCmdUapsdParam, pvSetBuffer, u4SetBufferLen);

#endif
}

WLAN_STATUS
wlanoidQueryP2pVersion(IN P_ADAPTER_T prAdapter,
		       IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	WLAN_STATUS rResult = WLAN_STATUS_FAILURE;
/* PUINT_8 pucVersionNum = (PUINT_8)pvQueryBuffer; */

	do {
		if ((prAdapter == NULL) || (pu4QueryInfoLen == NULL))
			break;

		if ((u4QueryBufferLen) && (pvQueryBuffer == NULL))
			break;

		if (u4QueryBufferLen < sizeof(UINT_8)) {
			*pu4QueryInfoLen = sizeof(UINT_8);
			rResult = WLAN_STATUS_BUFFER_TOO_SHORT;
			break;
		}

	} while (FALSE);

	return rResult;
}				/* wlanoidQueryP2pVersion */

#if CFG_SUPPORT_HOTSPOT_WPS_MANAGER

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to set the WPS mode.
*
* \param[in] pvAdapter Pointer to the Adapter structure.
* \param[in] pvSetBuffer A pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                          bytes read from the set buffer. If the call failed
*                          due to invalid length of the set buffer, returns
*                          the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetP2pWPSmode(IN P_ADAPTER_T prAdapter,
		     IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	WLAN_STATUS status;
	UINT_32 u4IsWPSmode = 0;
	int i = 0;

	DEBUGFUNC("wlanoidSetP2pWPSmode");
	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	if (pvSetBuffer)
		u4IsWPSmode = *(PUINT_32) pvSetBuffer;
	else
		u4IsWPSmode = 0;

	for (i = 0; i < KAL_P2P_NUM; i++)/* set all Role to the same value */
		if (u4IsWPSmode)
			prAdapter->rWifiVar.prP2PConnSettings[i]->fgIsWPSMode = 1;
		else
			prAdapter->rWifiVar.prP2PConnSettings[i]->fgIsWPSMode = 0;

	status = nicUpdateBss(prAdapter, P2P_DEV_BSS_INDEX);

	return status;
}				/* end of wlanoidSetP2pWPSmode() */

#endif

WLAN_STATUS
wlanoidSetP2pSupplicantVersion(IN P_ADAPTER_T prAdapter,
			       IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	WLAN_STATUS rResult = WLAN_STATUS_FAILURE;
	UINT_8 ucVersionNum;

	do {
		if ((prAdapter == NULL) || (pu4SetInfoLen == NULL)) {

			rResult = WLAN_STATUS_INVALID_DATA;
			break;
		}

		if ((u4SetBufferLen) && (pvSetBuffer == NULL)) {
			rResult = WLAN_STATUS_INVALID_DATA;
			break;
		}

		*pu4SetInfoLen = sizeof(UINT_8);

		if (u4SetBufferLen < sizeof(UINT_8)) {
			rResult = WLAN_STATUS_INVALID_LENGTH;
			break;
		}

		ucVersionNum = *((PUINT_8) pvSetBuffer);

		rResult = WLAN_STATUS_SUCCESS;
	} while (FALSE);

	return rResult;
}				/* wlanoidSetP2pSupplicantVersion */

#if CFG_SUPPORT_P2P_RSSI_QUERY
WLAN_STATUS
wlanoidQueryP2pRssi(IN P_ADAPTER_T prAdapter,
		    IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	DEBUGFUNC("wlanoidQueryP2pRssi");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (prAdapter->fgIsEnableLpdvt)
		return WLAN_STATUS_NOT_SUPPORTED;

	*pu4QueryInfoLen = sizeof(PARAM_RSSI);

	/* Check for query buffer length */
	if (u4QueryBufferLen < *pu4QueryInfoLen) {
		DBGLOG(REQ, WARN, "Too short length %ld\n", u4QueryBufferLen);
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}

	if (prAdapter->fgIsP2pLinkQualityValid == TRUE &&
	    (kalGetTimeTick() - prAdapter->rP2pLinkQualityUpdateTime) <= CFG_LINK_QUALITY_VALID_PERIOD) {
		PARAM_RSSI rRssi;

		rRssi = (PARAM_RSSI) prAdapter->rP2pLinkQuality.cRssi;	/* ranged from (-128 ~ 30) in unit of dBm */

		if (rRssi > PARAM_WHQL_RSSI_MAX_DBM)
			rRssi = PARAM_WHQL_RSSI_MAX_DBM;
		else if (rRssi < PARAM_WHQL_RSSI_MIN_DBM)
			rRssi = PARAM_WHQL_RSSI_MIN_DBM;

		kalMemCopy(pvQueryBuffer, &rRssi, sizeof(PARAM_RSSI));
		return WLAN_STATUS_SUCCESS;
	}
#ifdef LINUX
	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_GET_LINK_QUALITY,
				   FALSE,
				   TRUE,
				   TRUE,
				   nicCmdEventQueryLinkQuality,
				   nicOidCmdTimeoutCommon,
				   *pu4QueryInfoLen, pvQueryBuffer, pvQueryBuffer, u4QueryBufferLen);
#else
	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_GET_LINK_QUALITY,
				   FALSE,
				   TRUE,
				   TRUE,
				   nicCmdEventQueryLinkQuality,
				   nicOidCmdTimeoutCommon, 0, NULL, pvQueryBuffer, u4QueryBufferLen);

#endif
}				/* wlanoidQueryP2pRssi */
#endif
