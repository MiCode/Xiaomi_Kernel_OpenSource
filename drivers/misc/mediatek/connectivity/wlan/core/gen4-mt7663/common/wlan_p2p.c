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
 *    \brief This file contains the Wi-Fi Direct commands processing routines
 *             for MediaTek Inc. 802.11 Wireless LAN Adapters.
 */


/******************************************************************************
 *                         C O M P I L E R   F L A G S
 ******************************************************************************
 */

/******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 ******************************************************************************
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
 ******************************************************************************
 */
/*---------------------------------------------------------------------------*/
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
/*---------------------------------------------------------------------------*/
uint32_t
wlanoidSendSetQueryP2PCmd(IN struct ADAPTER *prAdapter,
		IN uint8_t ucCID,
		IN uint8_t ucBssIdx,
		IN u_int8_t fgSetQuery,
		IN u_int8_t fgNeedResp,
		IN u_int8_t fgIsOid,
		IN PFN_CMD_DONE_HANDLER pfCmdDoneHandler,
		IN PFN_CMD_TIMEOUT_HANDLER pfCmdTimeoutHandler,
		IN uint32_t u4SetQueryInfoLen,
		IN uint8_t *pucInfoBuffer,
		OUT void *pvSetQueryBuffer,
		IN uint32_t u4SetQueryBufferLen)
{
	struct GLUE_INFO *prGlueInfo;
	struct CMD_INFO *prCmdInfo;
	struct WIFI_CMD *prWifiCmd;
	uint8_t ucCmdSeqNum;

	ASSERT(prAdapter);

	prGlueInfo = prAdapter->prGlueInfo;
	ASSERT(prGlueInfo);

	DEBUGFUNC("wlanoidSendSetQueryP2PCmd");
	DBGLOG(REQ, TRACE, "Command ID = 0x%08X\n", ucCID);

	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter,
			(CMD_HDR_SIZE + u4SetQueryInfoLen));

	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}
	/* increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);
	DBGLOG(REQ, TRACE, "ucCmdSeqNum =%d\n", ucCmdSeqNum);

	/* Setup common CMD Info Packet */
	prCmdInfo->eCmdType = COMMAND_TYPE_NETWORK_IOCTL;
	prCmdInfo->u2InfoBufLen = (uint16_t) (CMD_HDR_SIZE + u4SetQueryInfoLen);
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
	prWifiCmd = (struct WIFI_CMD *) (prCmdInfo->pucInfoBuffer);
	prWifiCmd->u2TxByteCount = prCmdInfo->u2InfoBufLen;
	prWifiCmd->u2PQ_ID = CMD_PQ_ID;
	prWifiCmd->u2Length = prCmdInfo->u2InfoBufLen -
		(uint16_t) OFFSET_OF(struct WIFI_CMD, u2Length);
	prWifiCmd->u2PqId = CMD_PQ_ID;
	prWifiCmd->ucPktTypeID = CMD_PACKET_TYPE_ID;
	prWifiCmd->ucCID = prCmdInfo->ucCID;
	prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
	prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;

	if (u4SetQueryInfoLen > 0 && pucInfoBuffer != NULL)
		kalMemCopy(prWifiCmd->aucBuffer,
				pucInfoBuffer, u4SetQueryInfoLen);
	/* insert into prCmdQueue */
	kalEnqueueCommand(prGlueInfo, (struct QUE_ENTRY *) prCmdInfo);

	/* wakeup txServiceThread later */
	GLUE_SET_EVENT(prGlueInfo);
	return WLAN_STATUS_PENDING;
}

/*---------------------------------------------------------------------------*/
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
/*---------------------------------------------------------------------------*/
#if 0
uint32_t
wlanoidSetAddP2PKey(IN struct ADAPTER *prAdapter,
		IN void *pvSetBuffer,
		IN uint32_t u4SetBufferLen,
		OUT uint32_t *pu4SetInfoLen)
{
	struct CMD_802_11_KEY rCmdKey;
	struct PARAM_KEY *prNewKey;
	struct BSS_INFO *prBssInfo = (struct BSS_INFO *) NULL;
	struct STA_RECORD *prStaRec = (struct STA_RECORD *) NULL;

	DEBUGFUNC("wlanoidSetAddP2PKey");
	DBGLOG(REQ, INFO, "\n");

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	prNewKey = (struct PARAM_KEY *) pvSetBuffer;

	/* Verify the key structure length. */
	if (prNewKey->u4Length > u4SetBufferLen) {
		log_dbg(REQ, WARN,
		       "Invalid key structure length (%d) greater than total buffer length (%d)\n",
		       (uint8_t) prNewKey->u4Length, (uint8_t) u4SetBufferLen);

		*pu4SetInfoLen = u4SetBufferLen;
		return WLAN_STATUS_INVALID_LENGTH;
	}
	/* Verify the key material length for key material buffer */
	else if (prNewKey->u4KeyLength >
		prNewKey->u4Length -
		OFFSET_OF(struct PARAM_KEY, aucKeyMaterial)) {
		log_dbg(REQ, WARN,
				"Invalid key material length (%d)\n",
				(uint8_t) prNewKey->u4KeyLength);
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
			((prNewKey->arBSSID[0] == 0xff) &&
			(prNewKey->arBSSID[1] == 0xff) &&
			(prNewKey->arBSSID[2] == 0xff) &&
			(prNewKey->arBSSID[3] == 0xff) &&
			(prNewKey->arBSSID[4] == 0xff) &&
			(prNewKey->arBSSID[5] == 0xff))) {
			return WLAN_STATUS_INVALID_DATA;
		}
	}

	*pu4SetInfoLen = u4SetBufferLen;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prNewKey->ucBssIdx);
	ASSERT(prBssInfo);
#if 0
	if (prBssInfo->ucBMCWlanIndex >= WTBL_SIZE) {
		prBssInfo->ucBMCWlanIndex =
		    secPrivacySeekForBcEntry(prAdapter,
				prBssInfo->ucBssIndex, prBssInfo->aucBSSID,
					0xff, CIPHER_SUITE_NONE, 0xff);
	}
#endif
	/* fill CMD_802_11_KEY */
	kalMemZero(&rCmdKey, sizeof(struct CMD_802_11_KEY));
	rCmdKey.ucAddRemove = 1;	/* add */
	rCmdKey.ucTxKey =
		((prNewKey->u4KeyIndex & IS_TRANSMIT_KEY) == IS_TRANSMIT_KEY)
		? 1 : 0;
	rCmdKey.ucKeyType =
		((prNewKey->u4KeyIndex & IS_UNICAST_KEY) == IS_UNICAST_KEY)
		? 1 : 0;
#if 0
	/* group client */
	if (prBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE) {
#else
	/* group client */
	if (kalP2PGetRole(prAdapter->prGlueInfo) == 1) {
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
	rCmdKey.ucKeyId = (uint8_t) (prNewKey->u4KeyIndex & 0xff);
	rCmdKey.ucKeyLen = (uint8_t) prNewKey->u4KeyLength;
	kalMemCopy(rCmdKey.aucKeyMaterial,
		(uint8_t *) prNewKey->aucKeyMaterial, rCmdKey.ucKeyLen);

	if ((rCmdKey.aucPeerAddr[0] &
		rCmdKey.aucPeerAddr[1] & rCmdKey.aucPeerAddr[2] &
	    rCmdKey.aucPeerAddr[3] & rCmdKey.aucPeerAddr[4] &
	    rCmdKey.aucPeerAddr[5]) == 0xFF) {
		kalMemCopy(rCmdKey.aucPeerAddr,
			prBssInfo->aucBSSID, MAC_ADDR_LEN);
		if (!rCmdKey.ucIsAuthenticator) {
			prStaRec = cnmGetStaRecByAddress(prAdapter,
					rCmdKey.ucBssIdx, rCmdKey.aucPeerAddr);
			if (!prStaRec)
				ASSERT(FALSE);
		}
	} else {
		prStaRec = cnmGetStaRecByAddress(prAdapter,
					rCmdKey.ucBssIdx, rCmdKey.aucPeerAddr);
	}

	if (rCmdKey.ucTxKey) {
		if (prStaRec) {
			if (rCmdKey.ucKeyType) {	/* RSN STA */
				ASSERT(prStaRec->ucWlanIndex < WTBL_SIZE);
				rCmdKey.ucWlanIndex = prStaRec->ucWlanIndex;
				/* wait for CMD Done ? */
				prStaRec->fgTransmitKeyExist = TRUE;
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
				/* GC WEP Tx key ? */
				rCmdKey.ucWlanIndex = 255;
				ASSERT(FALSE);
			}
		}
	} else {
		if (((rCmdKey.aucPeerAddr[0] & rCmdKey.aucPeerAddr[1] &
			rCmdKey.aucPeerAddr[2] & rCmdKey.aucPeerAddr[3] &
			rCmdKey.aucPeerAddr[4] &
			rCmdKey.aucPeerAddr[5]) == 0xFF) ||
		    ((rCmdKey.aucPeerAddr[0] | rCmdKey.aucPeerAddr[1] |
		    rCmdKey.aucPeerAddr[2] | rCmdKey.aucPeerAddr[3] |
		    rCmdKey.aucPeerAddr[4] | rCmdKey.aucPeerAddr[5]) == 0x00)) {
			rCmdKey.ucWlanIndex = 255;	/* GC WEP ? */
			ASSERT(FALSE);
		} else {
			if (prStaRec) {	/* GC Rx RSN Group key */
				rCmdKey.ucWlanIndex =
					secPrivacySeekForBcEntry(prAdapter,
						prStaRec->ucBssIndex,
						prStaRec->aucMacAddr,
						prStaRec->ucIndex,
						rCmdKey.ucAlgorithmId,
						rCmdKey.ucKeyId);
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
					 sizeof(struct CMD_802_11_KEY),
					 (uint8_t *)&rCmdKey,
					 pvSetBuffer,
					 u4SetBufferLen);
}

/*---------------------------------------------------------------------------*/
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
/*---------------------------------------------------------------------------*/
uint32_t
wlanoidSetRemoveP2PKey(IN struct ADAPTER *prAdapter,
		IN void *pvSetBuffer,
		IN uint32_t u4SetBufferLen,
		OUT uint32_t *pu4SetInfoLen)
{
	struct CMD_802_11_KEY rCmdKey;
	struct PARAM_REMOVE_KEY *prRemovedKey;
	struct BSS_INFO *prBssInfo = (struct BSS_INFO *) NULL;
	struct STA_RECORD *prStaRec = (struct STA_RECORD *) NULL;

	DEBUGFUNC("wlanoidSetRemoveP2PKey");
	ASSERT(prAdapter);

	if (u4SetBufferLen < sizeof(struct PARAM_REMOVE_KEY))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);
	prRemovedKey = (struct PARAM_REMOVE_KEY *) pvSetBuffer;

	/* Check bit 31: this bit should always 0 */
	if (prRemovedKey->u4KeyIndex & IS_TRANSMIT_KEY) {
		/* Bit 31 should not be set */
		DBGLOG(REQ, ERROR, "invalid key index: 0x%08lx\n",
				prRemovedKey->u4KeyIndex);
		return WLAN_STATUS_INVALID_DATA;
	}

	/* Check bits 8 ~ 29 should always be 0 */
	if (prRemovedKey->u4KeyIndex & BITS(8, 29)) {
		/* Bit 31 should not be set */
		DBGLOG(REQ, ERROR, "invalid key index: 0x%08lx\n",
				prRemovedKey->u4KeyIndex);
		return WLAN_STATUS_INVALID_DATA;
	}

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prRemovedKey->ucBssIdx);

	kalMemZero((uint8_t *)&rCmdKey, sizeof(struct CMD_802_11_KEY));

	rCmdKey.ucAddRemove = 0;	/* remove */
	/* group client */
	if (prBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE) {
		rCmdKey.ucIsAuthenticator = 0;
	} else {		/* group owner */
		rCmdKey.ucIsAuthenticator = 1;
	}
	kalMemCopy(rCmdKey.aucPeerAddr,
		(uint8_t *) prRemovedKey->arBSSID, MAC_ADDR_LEN);
	rCmdKey.ucBssIdx = prRemovedKey->ucBssIdx;
	rCmdKey.ucKeyId = (uint8_t) (prRemovedKey->u4KeyIndex & 0x000000ff);

	/* Clean up the Tx key flag */
	prStaRec = cnmGetStaRecByAddress(prAdapter,
			prRemovedKey->ucBssIdx, prRemovedKey->arBSSID);

	/* mark for MR1 to avoid remove-key,
	 * but remove the wlan_tbl0 at the same time
	 */
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
		    && prAdapter->rWifiVar.rConnSettings.eEncStatus
		    != ENUM_ENCRYPTION_DISABLED) {
			rCmdKey.ucWlanIndex = prBssInfo->ucBMCWlanIndex;
		} else {
			rCmdKey.ucWlanIndex = WTBL_RESERVED_ENTRY;
			return WLAN_STATUS_SUCCESS;
		}
	}

	/* mark for MR1 to avoid remove-key,
	 * but remove the wlan_tbl0 at the same time
	 */
	/* secPrivacyFreeForEntry(prAdapter, rCmdKey.ucWlanIndex); */

	return wlanoidSendSetQueryP2PCmd(prAdapter,
					 CMD_ID_ADD_REMOVE_KEY,
					 prRemovedKey->ucBssIdx,
					 TRUE,
					 FALSE,
					 TRUE,
					 nicCmdEventSetCommon,
					 NULL,
					 sizeof(struct CMD_802_11_KEY),
					 (uint8_t *)&rCmdKey,
					 pvSetBuffer,
					 u4SetBufferLen);
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
uint32_t
wlanoidSetP2pNetworkAddress(IN struct ADAPTER *prAdapter,
		IN void *pvSetBuffer,
		IN uint32_t u4SetBufferLen,
		OUT uint32_t *pu4SetInfoLen)
{
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t i, j;
	struct CMD_SET_NETWORK_ADDRESS_LIST *prCmdNWAddrList;
	struct PARAM_NETWORK_ADDRESS_LIST *prNWAddrList =
		(struct PARAM_NETWORK_ADDRESS_LIST *) pvSetBuffer;
	struct PARAM_NETWORK_ADDRESS *prNWAddress;
	struct PARAM_NETWORK_ADDRESS_IP *prNetAddrIp;
	uint32_t u4IpAddressCount, u4CmdSize;

	DEBUGFUNC("wlanoidSetP2pNetworkAddress");
	DBGLOG(INIT, TRACE, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = 4;

	if (u4SetBufferLen < sizeof(struct PARAM_NETWORK_ADDRESS_LIST))
		return WLAN_STATUS_INVALID_DATA;

	*pu4SetInfoLen = 0;
	u4IpAddressCount = 0;

	prNWAddress = prNWAddrList->arAddress;
	for (i = 0; i < prNWAddrList->u4AddressCount; i++) {
		if (prNWAddress->u2AddressType
				== PARAM_PROTOCOL_ID_TCP_IP &&
			prNWAddress->u2AddressLength
				== sizeof(struct PARAM_NETWORK_ADDRESS_IP)) {
			u4IpAddressCount++;
		}

		prNWAddress = (struct PARAM_NETWORK_ADDRESS *)
			((unsigned long) prNWAddress +
			(unsigned long) (prNWAddress->u2AddressLength +
			OFFSET_OF(struct PARAM_NETWORK_ADDRESS, aucAddress)));
	}

	/* construct payload of command packet */
	u4CmdSize =
		OFFSET_OF(struct CMD_SET_NETWORK_ADDRESS_LIST, arNetAddress) +
		sizeof(struct IPV4_NETWORK_ADDRESS) * u4IpAddressCount;

	prCmdNWAddrList = (struct CMD_SET_NETWORK_ADDRESS_LIST *)
		kalMemAlloc(u4CmdSize, VIR_MEM_TYPE);

	if (prCmdNWAddrList == NULL)
		return WLAN_STATUS_FAILURE;

	/* fill P_CMD_SET_NETWORK_ADDRESS_LIST */
	prCmdNWAddrList->ucBssIndex = prNWAddrList->ucBssIdx;
	prCmdNWAddrList->ucAddressCount = (uint8_t) u4IpAddressCount;
	prNWAddress = prNWAddrList->arAddress;
	for (i = 0, j = 0; i < prNWAddrList->u4AddressCount; i++) {
		if (prNWAddress->u2AddressType
				== PARAM_PROTOCOL_ID_TCP_IP &&
			prNWAddress->u2AddressLength
				== sizeof(struct PARAM_NETWORK_ADDRESS_IP)) {
			prNetAddrIp = (struct PARAM_NETWORK_ADDRESS_IP *)
				prNWAddress->aucAddress;

			kalMemCopy(
				prCmdNWAddrList->arNetAddress[j].aucIpAddr,
				&(prNetAddrIp->in_addr), sizeof(uint32_t));

			j++;
		}

		prNWAddress = (struct PARAM_NETWORK_ADDRESS *)
			((unsigned long) prNWAddress +
			(unsigned long) (prNWAddress->u2AddressLength +
			OFFSET_OF(struct PARAM_NETWORK_ADDRESS, aucAddress)));
	}

	rStatus = wlanSendSetQueryCmd(prAdapter,
				      CMD_ID_SET_IP_ADDRESS,
				      TRUE,
				      FALSE,
				      TRUE,
				      nicCmdEventSetIpAddress,
				      nicOidCmdTimeoutCommon,
				      u4CmdSize,
				      (uint8_t *) prCmdNWAddrList,
				      pvSetBuffer,
				      u4SetBufferLen);

	kalMemFree(prCmdNWAddrList, VIR_MEM_TYPE, u4CmdSize);
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
uint32_t
wlanoidQueryP2pPowerSaveProfile(IN struct ADAPTER *prAdapter,
		IN void *pvQueryBuffer,
		IN uint32_t u4QueryBufferLen,
		OUT uint32_t *pu4QueryInfoLen)
{
	DEBUGFUNC("wlanoidQueryP2pPowerSaveProfile");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);

	if (u4QueryBufferLen != 0) {
		ASSERT(pvQueryBuffer);
		/* TODO: FIXME */
		/* *(enum PARAM_POWER_MODE *) pvQueryBuffer =
		 * (enum PARAM_POWER_MODE)
		 *(prAdapter->rWlanInfo.
		 *	arPowerSaveMode[prAdapter->ucP2PDevBssIdx].ucPsProfile);
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
uint32_t
wlanoidSetP2pPowerSaveProfile(IN struct ADAPTER *prAdapter,
		IN void *pvSetBuffer,
		IN uint32_t u4SetBufferLen,
		OUT uint32_t *pu4SetInfoLen)
{
	uint32_t status;
	enum PARAM_POWER_MODE ePowerMode;

	DEBUGFUNC("wlanoidSetP2pPowerSaveProfile");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(enum PARAM_POWER_MODE);
	if (u4SetBufferLen < sizeof(enum PARAM_POWER_MODE)) {
		DBGLOG(REQ, WARN, "Invalid length %u\n", u4SetBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	} else if (*(enum PARAM_POWER_MODE *)
			pvSetBuffer >= Param_PowerModeMax) {
		DBGLOG(REQ, WARN, "Invalid power mode %d\n",
			*(enum PARAM_POWER_MODE *) pvSetBuffer);
		return WLAN_STATUS_INVALID_DATA;
	}

	ePowerMode = *(enum PARAM_POWER_MODE *) pvSetBuffer;

	if (prAdapter->fgEnCtiaPowerMode) {
		if (ePowerMode == Param_PowerModeCAM) {
			/*Todo::  Nothing */
			/*Todo::  Nothing */
		} else {
			/* User setting to PS mode
			 *(Param_PowerModeMAX_PSP or Param_PowerModeFast_PSP)
			 */

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

	/* TODO: FIXME */
	status = nicConfigPowerSaveProfile(prAdapter, prAdapter->ucP2PDevBssIdx,
				ePowerMode, g_fgIsOid, PS_CALLER_P2P);
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
uint32_t
wlanoidSetP2pSetNetworkAddress(IN struct ADAPTER *prAdapter,
		IN void *pvSetBuffer,
		IN uint32_t u4SetBufferLen,
		OUT uint32_t *pu4SetInfoLen)
{
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t i, j;
	struct CMD_SET_NETWORK_ADDRESS_LIST *prCmdNWAddrList;
	struct PARAM_NETWORK_ADDRESS_LIST *prNWAddrList =
		(struct PARAM_NETWORK_ADDRESS_LIST *) pvSetBuffer;
	struct PARAM_NETWORK_ADDRESS *prNWAddress;
	struct PARAM_NETWORK_ADDRESS_IP *prNetAddrIp;
	uint32_t u4IpAddressCount, u4CmdSize;
	uint8_t *pucBuf = (uint8_t *) pvSetBuffer;

	DEBUGFUNC("wlanoidSetP2pSetNetworkAddress");
	DBGLOG(INIT, TRACE, "\n");
	DBGLOG(INIT, INFO, "wlanoidSetP2pSetNetworkAddress (%d)\n",
		(int16_t) u4SetBufferLen);

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = 4;

	if (u4SetBufferLen < sizeof(struct PARAM_NETWORK_ADDRESS_LIST))
		return WLAN_STATUS_INVALID_DATA;

	*pu4SetInfoLen = 0;
	u4IpAddressCount = 0;

	prNWAddress = prNWAddrList->arAddress;
	for (i = 0; i < prNWAddrList->u4AddressCount; i++) {
		if (prNWAddress->u2AddressType
				== PARAM_PROTOCOL_ID_TCP_IP &&
		    prNWAddress->u2AddressLength
			== sizeof(struct PARAM_NETWORK_ADDRESS_IP)) {
			u4IpAddressCount++;
		}

		prNWAddress = (struct PARAM_NETWORK_ADDRESS *)
			((unsigned long) prNWAddress +
			(unsigned long) (prNWAddress->u2AddressLength +
			OFFSET_OF(struct PARAM_NETWORK_ADDRESS, aucAddress)));
	}

	/* construct payload of command packet */
	u4CmdSize =
		OFFSET_OF(struct CMD_SET_NETWORK_ADDRESS_LIST, arNetAddress) +
	    sizeof(struct IPV4_NETWORK_ADDRESS) * u4IpAddressCount;

	if (u4IpAddressCount == 0)
		u4CmdSize = sizeof(struct CMD_SET_NETWORK_ADDRESS_LIST);

	prCmdNWAddrList = (struct CMD_SET_NETWORK_ADDRESS_LIST *)
		kalMemAlloc(u4CmdSize, VIR_MEM_TYPE);

	if (prCmdNWAddrList == NULL)
		return WLAN_STATUS_FAILURE;

	/* fill P_CMD_SET_NETWORK_ADDRESS_LIST */
	prCmdNWAddrList->ucBssIndex = prNWAddrList->ucBssIdx;

	/* only to set IP address to FW once ARP filter is enabled */
	if (prAdapter->fgEnArpFilter) {
		prCmdNWAddrList->ucAddressCount =
			(uint8_t) u4IpAddressCount;
		prNWAddress = prNWAddrList->arAddress;

		DBGLOG(INIT, INFO, "u4IpAddressCount (%u)\n",
			(int32_t) u4IpAddressCount);

		for (i = 0, j = 0; i < prNWAddrList->u4AddressCount; i++) {
			if (prNWAddress->u2AddressType
					== PARAM_PROTOCOL_ID_TCP_IP &&
			    prNWAddress->u2AddressLength
				== sizeof(struct PARAM_NETWORK_ADDRESS_IP)) {

				prNetAddrIp =
					(struct PARAM_NETWORK_ADDRESS_IP *)
					prNWAddress->aucAddress;

				kalMemCopy(
					prCmdNWAddrList->arNetAddress[j]
						.aucIpAddr,
					&(prNetAddrIp->in_addr),
					sizeof(uint32_t));

				j++;

				pucBuf = (uint8_t *) &prNetAddrIp->in_addr;
				DBGLOG(INIT, INFO,
						"prNetAddrIp->in_addr:%d:%d:%d:%d\n",
						(uint8_t) pucBuf[0],
						(uint8_t) pucBuf[1],
						(uint8_t) pucBuf[2],
						(uint8_t) pucBuf[3]);
			}

			prNWAddress = (struct PARAM_NETWORK_ADDRESS *)
				((unsigned long) prNWAddress +
				(unsigned long) (prNWAddress->u2AddressLength +
				OFFSET_OF(struct PARAM_NETWORK_ADDRESS,
					aucAddress)));
		}

	} else {
		prCmdNWAddrList->ucAddressCount = 0;
	}

	rStatus = wlanSendSetQueryCmd(prAdapter,
				      CMD_ID_SET_IP_ADDRESS,
				      TRUE,
				      FALSE,
				      TRUE,
				      nicCmdEventSetIpAddress,
				      nicOidCmdTimeoutCommon,
				      u4CmdSize,
				      (uint8_t *) prCmdNWAddrList,
				      pvSetBuffer,
				      u4SetBufferLen);

	kalMemFree(prCmdNWAddrList, VIR_MEM_TYPE, u4CmdSize);
	return rStatus;
}				/* end of wlanoidSetP2pSetNetworkAddress() */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to set Multicast Address List.
 *
 * \param[in] prAdapter      Pointer to the Adapter structure.
 * \param[in] pvSetBuffer    Pointer to the buffer
 *                                     that holds the data to be set.
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
uint32_t
wlanoidSetP2PMulticastList(IN struct ADAPTER *prAdapter,
		IN void *pvSetBuffer,
		IN uint32_t u4SetBufferLen,
		OUT uint32_t *pu4SetInfoLen)
{
	struct CMD_MAC_MCAST_ADDR rCmdMacMcastAddr;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	/* The data must be a multiple of the Ethernet address size. */
	if ((u4SetBufferLen % MAC_ADDR_LEN)) {
		DBGLOG(REQ, WARN, "Invalid MC list length %u\n",
			u4SetBufferLen);

		*pu4SetInfoLen =
			(((u4SetBufferLen + MAC_ADDR_LEN) - 1)
			/ MAC_ADDR_LEN) * MAC_ADDR_LEN;

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
	/* TODO: */
	rCmdMacMcastAddr.ucBssIndex = prAdapter->ucP2PDevBssIdx;
	kalMemCopy(rCmdMacMcastAddr.arAddress, pvSetBuffer, u4SetBufferLen);

	return wlanoidSendSetQueryP2PCmd(prAdapter,
				CMD_ID_MAC_MCAST_ADDR,
				prAdapter->ucP2PDevBssIdx,
				/* TODO: */
				/* This CMD response is no need
				 * to complete the OID.
				 * Or the event would unsync.
				 */
				TRUE, FALSE, FALSE,
				nicCmdEventSetCommon,
				nicOidCmdTimeoutCommon,
				sizeof(struct CMD_MAC_MCAST_ADDR),
				(uint8_t *) &rCmdMacMcastAddr,
				pvSetBuffer,
				u4SetBufferLen);

}				/* end of wlanoidSetP2PMulticastList() */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to send GAS frame
 *          for P2P Service Discovery Request
 *
 * \param[in] prAdapter  Pointer to the Adapter structure.
 * \param[in] pvSetBuffer  Pointer to the buffer that holds the data to be set.
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
uint32_t
wlanoidSendP2PSDRequest(IN struct ADAPTER *prAdapter,
		IN void *pvSetBuffer,
		IN uint32_t u4SetBufferLen,
		OUT uint32_t *pu4SetInfoLen)
{
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	if (u4SetBufferLen)
		ASSERT(pvSetBuffer);

	if (u4SetBufferLen < sizeof(struct PARAM_P2P_SEND_SD_REQUEST)) {
		*pu4SetInfoLen = sizeof(struct PARAM_P2P_SEND_SD_REQUEST);
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}
/* rWlanStatus = p2pFsmRunEventSDRequest(prAdapter
 * , (P_PARAM_P2P_SEND_SD_REQUEST)pvSetBuffer);
 */

	return rWlanStatus;
}				/* end of wlanoidSendP2PSDRequest() */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to send GAS frame
 *          for P2P Service Discovery Response
 *
 * \param[in] prAdapter  Pointer to the Adapter structure.
 * \param[in] pvSetBuffer  Pointer to the buffer that holds the data to be set.
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
uint32_t
wlanoidSendP2PSDResponse(IN struct ADAPTER *prAdapter,
		IN void *pvSetBuffer,
		IN uint32_t u4SetBufferLen,
		OUT uint32_t *pu4SetInfoLen)
{
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	if (u4SetBufferLen)
		ASSERT(pvSetBuffer);

	if (u4SetBufferLen < sizeof(struct PARAM_P2P_SEND_SD_RESPONSE)) {
		*pu4SetInfoLen = sizeof(struct PARAM_P2P_SEND_SD_RESPONSE);
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}
/* rWlanStatus = p2pFsmRunEventSDResponse(prAdapter
 * , (P_PARAM_P2P_SEND_SD_RESPONSE)pvSetBuffer);
 */

	return rWlanStatus;
}				/* end of wlanoidGetP2PSDRequest() */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to get GAS frame
 *          for P2P Service Discovery Request
 *
 * \param[in]  prAdapter  Pointer to the Adapter structure.
 * \param[out] pvQueryBuffer  A pointer to the buffer that holds the result of
 *                          the query.
 * \param[in]  u4QueryBufferLen The length of the query buffer.
 * \param[out] pu4QueryInfoLen  If the call is successful,
 *                          returns the number of
 *                          bytes written into the query buffer. If the call
 *                          failed due to invalid length of the query buffer,
 *                          returns the amount of storage needed.
 *
 * \retval WLAN_STATUS_SUCCESS
 * \retval WLAN_STATUS_INVALID_LENGTH
 * \retval WLAN_STATUS_ADAPTER_NOT_READY
 * \retval WLAN_STATUS_MULTICAST_FULL
 */
/*----------------------------------------------------------------------------*/
uint32_t
wlanoidGetP2PSDRequest(IN struct ADAPTER *prAdapter,
		IN void *pvQueryBuffer,
		IN uint32_t u4QueryBufferLen,
		OUT uint32_t *pu4QueryInfoLen)
{
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;
/* PUINT_8 pucChannelNum = NULL; */
/* UINT_8 ucChannelNum = 0, ucSeqNum = 0; */

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);

	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (u4QueryBufferLen < sizeof(struct PARAM_P2P_GET_SD_REQUEST)) {
		*pu4QueryInfoLen = sizeof(struct PARAM_P2P_GET_SD_REQUEST);
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}

	DBGLOG(P2P, TRACE, "Get Service Discovery Request\n");

	*pu4QueryInfoLen = 0;
	return rWlanStatus;
}				/* end of wlanoidGetP2PSDRequest() */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to get GAS frame
 *          for P2P Service Discovery Response
 *
 * \param[in]  prAdapter        Pointer to the Adapter structure.
 * \param[out] pvQueryBuffer    A pointer to the buffer that holds the result of
 *                          the query.
 * \param[in]  u4QueryBufferLen The length of the query buffer.
 * \param[out] pu4QueryInfoLen  If the call is successful, returns the number of
 *                          bytes written into the query buffer. If the call
 *                          failed due to invalid length of the query buffer,
 *                          returns the amount of storage needed.
 *
 * \retval WLAN_STATUS_SUCCESS
 * \retval WLAN_STATUS_INVALID_LENGTH
 * \retval WLAN_STATUS_ADAPTER_NOT_READY
 * \retval WLAN_STATUS_MULTICAST_FULL
 */
/*----------------------------------------------------------------------------*/
uint32_t
wlanoidGetP2PSDResponse(IN struct ADAPTER *prAdapter,
		IN void *pvQueryBuffer,
		IN uint32_t u4QueryBufferLen,
		OUT uint32_t *pu4QueryInfoLen)
{
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;
	/* UINT_8 ucSeqNum = 0, */

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);

	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (u4QueryBufferLen < sizeof(struct PARAM_P2P_GET_SD_RESPONSE)) {
		*pu4QueryInfoLen = sizeof(struct PARAM_P2P_GET_SD_RESPONSE);
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
 * \param[in] prAdapter  Pointer to the Adapter structure.
 * \param[in] pvSetBuffer  Pointer to the buffer that holds the data to be set.
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
uint32_t
wlanoidSetP2PTerminateSDPhase(IN struct ADAPTER *prAdapter,
		IN void *pvSetBuffer,
		IN uint32_t u4SetBufferLen,
		OUT uint32_t *pu4SetInfoLen)
{
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;
	struct PARAM_P2P_TERMINATE_SD_PHASE *prP2pTerminateSD =
		(struct PARAM_P2P_TERMINATE_SD_PHASE *) NULL;
	uint8_t aucNullAddr[] = NULL_MAC_ADDR;

	do {
		if ((prAdapter == NULL) || (pu4SetInfoLen == NULL))
			break;

		if ((u4SetBufferLen) && (pvSetBuffer == NULL))
			break;

		if (u4SetBufferLen
			< sizeof(struct PARAM_P2P_TERMINATE_SD_PHASE)) {

			*pu4SetInfoLen =
				sizeof(struct PARAM_P2P_TERMINATE_SD_PHASE);
			rWlanStatus = WLAN_STATUS_BUFFER_TOO_SHORT;
			break;
		}

		prP2pTerminateSD =
			(struct PARAM_P2P_TERMINATE_SD_PHASE *) pvSetBuffer;

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
 * \param[in] prAdapter  Pointer to the Adapter structure.
 * \param[in] pvSetBuffer  Pointer to the buffer that holds the data to be set.
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
uint32_t
wlanoidSetSecCheckRequest(IN struct ADAPTER *prAdapter,
		IN void *pvSetBuffer,
		IN uint32_t u4SetBufferLen,
		OUT uint32_t *pu4SetInfoLen)
{
	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	if (u4SetBufferLen)
		ASSERT(pvSetBuffer);

#if 0  /* Comment it because CMD_ID_SEC_CHECK is not defined */
	return wlanoidSendSetQueryP2PCmd(prAdapter,
					 CMD_ID_SEC_CHECK,
					 prAdapter->ucP2PDevBssIdx,
					 FALSE,
					 TRUE,
					 TRUE,
					 NULL,
					 nicOidCmdTimeoutCommon,
					 u4SetBufferLen,
					 (uint8_t *) pvSetBuffer,
					 pvSetBuffer,
					 u4SetBufferLen);
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
 *                              bytes written into the query buffer.
 *                              If the call failed due to invalid length
 *                              of the query buffer,
 *                              returns the amount of storage needed.
 *
 * \retval WLAN_STATUS_SUCCESS
 * \retval WLAN_STATUS_INVALID_LENGTH
 * \retval WLAN_STATUS_ADAPTER_NOT_READY
 * \retval WLAN_STATUS_MULTICAST_FULL
 */
/*----------------------------------------------------------------------------*/
uint32_t
wlanoidGetSecCheckResponse(IN struct ADAPTER *prAdapter,
		IN void *pvQueryBuffer,
		IN uint32_t u4QueryBufferLen,
		OUT uint32_t *pu4QueryInfoLen)
{
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;
	/* P_WLAN_MAC_HEADER_T prWlanHdr = (P_WLAN_MAC_HEADER_T)NULL; */
	struct GLUE_INFO *prGlueInfo;

	prGlueInfo = prAdapter->prGlueInfo;

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);

	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (u4QueryBufferLen > 256)
		u4QueryBufferLen = 256;

	*pu4QueryInfoLen = u4QueryBufferLen;

#if DBG
	DBGLOG_MEM8(SEC, LOUD,
		prGlueInfo->prP2PInfo[0]->aucSecCheckRsp,
		u4QueryBufferLen);
#endif
	kalMemCopy((uint8_t *)
		(pvQueryBuffer +
		OFFSET_OF(struct iw_p2p_transport_struct, aucBuffer)),
		prGlueInfo->prP2PInfo[0]->aucSecCheckRsp,
		u4QueryBufferLen);

	return rWlanStatus;
}				/* end of wlanoidGetSecCheckResponse() */
#endif
#endif

uint32_t
wlanoidSetNoaParam(IN struct ADAPTER *prAdapter,
		IN void *pvSetBuffer,
		IN uint32_t u4SetBufferLen,
		OUT uint32_t *pu4SetInfoLen)
{
	struct PARAM_CUSTOM_NOA_PARAM_STRUCT *prNoaParam;
	struct CMD_CUSTOM_NOA_PARAM_STRUCT rCmdNoaParam;

	DEBUGFUNC("wlanoidSetNoaParam");
	DBGLOG(INIT, TRACE, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(struct PARAM_CUSTOM_NOA_PARAM_STRUCT);

	if (u4SetBufferLen < sizeof(struct PARAM_CUSTOM_NOA_PARAM_STRUCT))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prNoaParam = (struct PARAM_CUSTOM_NOA_PARAM_STRUCT *) pvSetBuffer;

	kalMemZero(&rCmdNoaParam, sizeof(struct CMD_CUSTOM_NOA_PARAM_STRUCT));
	rCmdNoaParam.u4NoaDurationMs = prNoaParam->u4NoaDurationMs;
	rCmdNoaParam.u4NoaIntervalMs = prNoaParam->u4NoaIntervalMs;
	rCmdNoaParam.u4NoaCount = prNoaParam->u4NoaCount;
	rCmdNoaParam.ucBssIdx = prNoaParam->ucBssIdx;

#if 0
	return wlanSendSetQueryCmd(prAdapter,
				CMD_ID_SET_NOA_PARAM,
				TRUE,
				FALSE,
				TRUE,
				nicCmdEventSetCommon,
				nicOidCmdTimeoutCommon,
				sizeof(struct CMD_CUSTOM_NOA_PARAM_STRUCT),
				(uint8_t *) &rCmdNoaParam,
				pvSetBuffer,
				u4SetBufferLen);
#else
	return wlanoidSendSetQueryP2PCmd(prAdapter,
				CMD_ID_SET_NOA_PARAM,
				prNoaParam->ucBssIdx,
				TRUE,
				FALSE,
				g_fgIsOid,
				nicCmdEventSetCommon,
				nicOidCmdTimeoutCommon,
				sizeof(struct CMD_CUSTOM_NOA_PARAM_STRUCT),
				(uint8_t *) &rCmdNoaParam,
				pvSetBuffer,
				u4SetBufferLen);

#endif

}

uint32_t
wlanoidSetOppPsParam(IN struct ADAPTER *prAdapter,
		IN void *pvSetBuffer,
		IN uint32_t u4SetBufferLen,
		OUT uint32_t *pu4SetInfoLen)
{
	struct PARAM_CUSTOM_OPPPS_PARAM_STRUCT *prOppPsParam;
	struct CMD_CUSTOM_OPPPS_PARAM_STRUCT rCmdOppPsParam;

	DEBUGFUNC("wlanoidSetOppPsParam");
	DBGLOG(INIT, TRACE, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(struct PARAM_CUSTOM_OPPPS_PARAM_STRUCT);

	if (u4SetBufferLen < sizeof(struct PARAM_CUSTOM_OPPPS_PARAM_STRUCT))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prOppPsParam = (struct PARAM_CUSTOM_OPPPS_PARAM_STRUCT *) pvSetBuffer;

	kalMemZero(&rCmdOppPsParam,
		sizeof(struct CMD_CUSTOM_OPPPS_PARAM_STRUCT));
	rCmdOppPsParam.u4CTwindowMs = prOppPsParam->u4CTwindowMs;
	rCmdOppPsParam.ucBssIdx = prOppPsParam->ucBssIdx;

#if 0
	return wlanSendSetQueryCmd(prAdapter,
				CMD_ID_SET_OPPPS_PARAM,
				TRUE,
				FALSE,
				TRUE,
				nicCmdEventSetCommon,
				nicOidCmdTimeoutCommon,
				sizeof(struct CMD_CUSTOM_OPPPS_PARAM_STRUCT),
				(uint8_t *) &rCmdOppPsParam,
				pvSetBuffer,
				u4SetBufferLen);
#else
	return wlanoidSendSetQueryP2PCmd(prAdapter,
				CMD_ID_SET_OPPPS_PARAM,
				prOppPsParam->ucBssIdx,
				TRUE,
				FALSE,
				g_fgIsOid,
				nicCmdEventSetCommon,
				nicOidCmdTimeoutCommon,
				sizeof(struct CMD_CUSTOM_OPPPS_PARAM_STRUCT),
				(uint8_t *) &rCmdOppPsParam,
				pvSetBuffer,
				u4SetBufferLen);

#endif

}

uint32_t
wlanoidSetUApsdParam(IN struct ADAPTER *prAdapter,
		IN void *pvSetBuffer,
		IN uint32_t u4SetBufferLen,
		OUT uint32_t *pu4SetInfoLen)
{
	struct PARAM_CUSTOM_UAPSD_PARAM_STRUCT *prUapsdParam;
	struct CMD_CUSTOM_UAPSD_PARAM_STRUCT rCmdUapsdParam;
	struct PM_PROFILE_SETUP_INFO *prPmProfSetupInfo;
	struct BSS_INFO *prBssInfo;

	DEBUGFUNC("wlanoidSetUApsdParam");
	DBGLOG(INIT, TRACE, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(struct PARAM_CUSTOM_UAPSD_PARAM_STRUCT);

	if (u4SetBufferLen < sizeof(struct PARAM_CUSTOM_UAPSD_PARAM_STRUCT))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prUapsdParam = (struct PARAM_CUSTOM_UAPSD_PARAM_STRUCT *) pvSetBuffer;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prUapsdParam->ucBssIdx);
	prPmProfSetupInfo = &prBssInfo->rPmProfSetupInfo;

	kalMemZero(&rCmdUapsdParam,
		sizeof(struct CMD_CUSTOM_UAPSD_PARAM_STRUCT));

	rCmdUapsdParam.fgEnAPSD = prUapsdParam->fgEnAPSD;

	rCmdUapsdParam.fgEnAPSD_AcBe = prUapsdParam->fgEnAPSD_AcBe;
	rCmdUapsdParam.fgEnAPSD_AcBk = prUapsdParam->fgEnAPSD_AcBk;
	rCmdUapsdParam.fgEnAPSD_AcVo = prUapsdParam->fgEnAPSD_AcVo;
	rCmdUapsdParam.fgEnAPSD_AcVi = prUapsdParam->fgEnAPSD_AcVi;

	prPmProfSetupInfo->ucBmpDeliveryAC =
	    ((prUapsdParam->fgEnAPSD_AcBe << 0) |
	     (prUapsdParam->fgEnAPSD_AcBk << 1) |
	     (prUapsdParam->fgEnAPSD_AcVi << 2) |
	     (prUapsdParam->fgEnAPSD_AcVo << 3));

	prPmProfSetupInfo->ucBmpTriggerAC =
	    ((prUapsdParam->fgEnAPSD_AcBe << 0) |
	     (prUapsdParam->fgEnAPSD_AcBk << 1) |
	     (prUapsdParam->fgEnAPSD_AcVi << 2) |
	     (prUapsdParam->fgEnAPSD_AcVo << 3));

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
				sizeof(struct CMD_CUSTOM_UAPSD_PARAM_STRUCT),
				(uint8_t *) &rCmdUapsdParam,
				pvSetBuffer,
				u4SetBufferLen);
#else
	return wlanoidSendSetQueryP2PCmd(prAdapter,
				CMD_ID_SET_UAPSD_PARAM,
				prBssInfo->ucBssIndex,
				TRUE,
				FALSE,
				g_fgIsOid,
				nicCmdEventSetCommon,
				nicOidCmdTimeoutCommon,
				sizeof(struct CMD_CUSTOM_UAPSD_PARAM_STRUCT),
				(uint8_t *) &rCmdUapsdParam,
				pvSetBuffer,
				u4SetBufferLen);

#endif
}

uint32_t
wlanoidQueryP2pVersion(IN struct ADAPTER *prAdapter,
		IN void *pvQueryBuffer,
		IN uint32_t u4QueryBufferLen,
		OUT uint32_t *pu4QueryInfoLen)
{
	uint32_t rResult = WLAN_STATUS_FAILURE;
/* PUINT_8 pucVersionNum = (PUINT_8)pvQueryBuffer; */

	do {
		if ((prAdapter == NULL) || (pu4QueryInfoLen == NULL))
			break;

		if ((u4QueryBufferLen) && (pvQueryBuffer == NULL))
			break;

		if (u4QueryBufferLen < sizeof(uint8_t)) {
			*pu4QueryInfoLen = sizeof(uint8_t);
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
uint32_t
wlanoidSetP2pWPSmode(IN struct ADAPTER *prAdapter,
		IN void *pvSetBuffer,
		IN uint32_t u4SetBufferLen,
		OUT uint32_t *pu4SetInfoLen)
{
	uint32_t status;
	uint32_t u4IsWPSmode = 0;
	int i = 0;

	DEBUGFUNC("wlanoidSetP2pWPSmode");
	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	if (pvSetBuffer)
		u4IsWPSmode = *(uint32_t *) pvSetBuffer;
	else
		u4IsWPSmode = 0;

	/* set all Role to the same value */
	for (i = 0; i < KAL_P2P_NUM; i++)
		if (u4IsWPSmode)
			prAdapter->rWifiVar.prP2PConnSettings[i]->fgIsWPSMode
				= 1;
		else
			prAdapter->rWifiVar.prP2PConnSettings[i]->fgIsWPSMode
				= 0;

	status = nicUpdateBss(prAdapter, prAdapter->ucP2PDevBssIdx);

	return status;
}				/* end of wlanoidSetP2pWPSmode() */

#endif

uint32_t
wlanoidSetP2pSupplicantVersion(IN struct ADAPTER *prAdapter,
		IN void *pvSetBuffer,
		IN uint32_t u4SetBufferLen,
		OUT uint32_t *pu4SetInfoLen)
{
	uint32_t rResult = WLAN_STATUS_FAILURE;
	uint8_t ucVersionNum;

	do {
		if ((prAdapter == NULL) || (pu4SetInfoLen == NULL)) {

			rResult = WLAN_STATUS_INVALID_DATA;
			break;
		}

		if ((u4SetBufferLen) && (pvSetBuffer == NULL)) {
			rResult = WLAN_STATUS_INVALID_DATA;
			break;
		}

		*pu4SetInfoLen = sizeof(uint8_t);

		if (u4SetBufferLen < sizeof(uint8_t)) {
			rResult = WLAN_STATUS_INVALID_LENGTH;
			break;
		}

		ucVersionNum = *((uint8_t *) pvSetBuffer);

		rResult = WLAN_STATUS_SUCCESS;
	} while (FALSE);

	return rResult;
}				/* wlanoidSetP2pSupplicantVersion */

#if CFG_SUPPORT_P2P_RSSI_QUERY
uint32_t
wlanoidQueryP2pRssi(IN struct ADAPTER *prAdapter,
		IN void *pvQueryBuffer,
		IN uint32_t u4QueryBufferLen,
		OUT uint32_t *pu4QueryInfoLen)
{
	DEBUGFUNC("wlanoidQueryP2pRssi");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (prAdapter->fgIsEnableLpdvt)
		return WLAN_STATUS_NOT_SUPPORTED;

	*pu4QueryInfoLen = sizeof(int32_t);

	/* Check for query buffer length */
	if (u4QueryBufferLen < *pu4QueryInfoLen) {
		DBGLOG(REQ, WARN, "Too short length %ld\n", u4QueryBufferLen);
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}

	if (prAdapter->fgIsP2pLinkQualityValid == TRUE
		&& (kalGetTimeTick() - prAdapter->rP2pLinkQualityUpdateTime)
		<= CFG_LINK_QUALITY_VALID_PERIOD) {

		int32_t rRssi;

		/* ranged from (-128 ~ 30) in unit of dBm */
		rRssi = (int32_t) prAdapter->rP2pLinkQuality.cRssi;

		if (rRssi > PARAM_WHQL_RSSI_MAX_DBM)
			rRssi = PARAM_WHQL_RSSI_MAX_DBM;
		else if (rRssi < PARAM_WHQL_RSSI_MIN_DBM)
			rRssi = PARAM_WHQL_RSSI_MIN_DBM;

		kalMemCopy(pvQueryBuffer, &rRssi, sizeof(int32_t));
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
				*pu4QueryInfoLen,
				pvQueryBuffer,
				pvQueryBuffer,
				u4QueryBufferLen);
#else
	return wlanSendSetQueryCmd(prAdapter,
				CMD_ID_GET_LINK_QUALITY,
				FALSE,
				TRUE,
				TRUE,
				nicCmdEventQueryLinkQuality,
				nicOidCmdTimeoutCommon,
				0,
				NULL,
				pvQueryBuffer,
				u4QueryBufferLen);

#endif
}				/* wlanoidQueryP2pRssi */
#endif

uint32_t
wlanoidAbortP2pScan(IN struct ADAPTER *prAdapter,
		OUT void *pvQueryBuffer,
		IN uint32_t u4QueryBufferLen,
		OUT uint32_t *pu4QueryInfoLen) {

	DBGLOG(P2P, INFO, "wlanoidAbortP2pScan\n");

	ASSERT(prAdapter);

	p2pDevFsmRunEventScanAbort(prAdapter, NULL);

	return WLAN_STATUS_SUCCESS;
}

