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
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/common/wlan_oid.c#11
*/

/*! \file wlanoid.c
 *   \brief This file contains the WLAN OID processing routines of Windows driver for
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
#include "mgmt/rsn.h"
#include "gl_wext.h"
#include "gl_vendor.h"
#include "debug.h"
#include <stddef.h>

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

PARAM_CUSTOM_KEY_CFG_STRUCT_T g_rDefaulteSetting[] = {
		/*format :
		*: {"firmware config parameter", "firmware config value"}
		*/
		{"AdapScan", "0x0"}
};


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
#if DBG && 0
static VOID SetRCID(BOOLEAN fgOneTb3, BOOL *fgRCID);
#endif

#if CFG_SLT_SUPPORT
static VOID SetTestChannel(UINT_8 *pucPrimaryChannel);
#endif

/******************************************************************************
*                              F U N C T I O N S
*******************************************************************************
*/
#if CFG_ENABLE_STATISTICS_BUFFERING
static BOOLEAN IsBufferedStatisticsUsable(P_ADAPTER_T prAdapter)
{
	ASSERT(prAdapter);

	if (prAdapter->fgIsStatValid == TRUE &&
	    (kalGetTimeTick() - prAdapter->rStatUpdateTime) <= CFG_STATISTICS_VALID_CYCLE)
		return TRUE;
	else
		return FALSE;
}
#endif

#if DBG && 0
static VOID SetRCID(BOOLEAN fgOneTb3, BOOL *fgRCID)
{
	if (fgOneTb3)
		*fgRCID = 0;
	else
		*fgRCID = 1;
}
#endif

#if CFG_SLT_SUPPORT
static VOID SetTestChannel(UINT_8 *pucPrimaryChannel)
{
	if (*pucPrimaryChannel < 5)
		*pucPrimaryChannel = 8;
	else if (*pucPrimaryChannel > 10)
		*pucPrimaryChannel = 3;
}
#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the supported physical layer network
*        type that can be used by the driver.
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[out] pvQueryBuffer A pointer to the buffer that holds the result of
*                           the query.
* \param[in] u4QueryBufferLen The length of the query buffer.
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                            bytes written into the query buffer. If the call
*                            failed due to invalid length of the query buffer,
*                            returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryNetworkTypesSupported(IN P_ADAPTER_T prAdapter,
				  OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	UINT_32 u4NumItem = 0;
	ENUM_PARAM_NETWORK_TYPE_T eSupportedNetworks[PARAM_NETWORK_TYPE_NUM];
	PPARAM_NETWORK_TYPE_LIST prSupported;

	/* The array of all physical layer network subtypes that the driver supports. */

	DEBUGFUNC("wlanoidQueryNetworkTypesSupported");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	/* Init. */
	for (u4NumItem = 0; u4NumItem < PARAM_NETWORK_TYPE_NUM; u4NumItem++)
		eSupportedNetworks[u4NumItem] = 0;

	u4NumItem = 0;

	eSupportedNetworks[u4NumItem] = PARAM_NETWORK_TYPE_DS;
	u4NumItem++;

	eSupportedNetworks[u4NumItem] = PARAM_NETWORK_TYPE_OFDM24;
	u4NumItem++;

	*pu4QueryInfoLen =
	    (UINT_32) OFFSET_OF(PARAM_NETWORK_TYPE_LIST, eNetworkType) +
	    (u4NumItem * sizeof(ENUM_PARAM_NETWORK_TYPE_T));

	if (u4QueryBufferLen < *pu4QueryInfoLen)
		return WLAN_STATUS_INVALID_LENGTH;

	prSupported = (PPARAM_NETWORK_TYPE_LIST) pvQueryBuffer;
	prSupported->NumberOfItems = u4NumItem;
	kalMemCopy(prSupported->eNetworkType, eSupportedNetworks, u4NumItem * sizeof(ENUM_PARAM_NETWORK_TYPE_T));

	DBGLOG(REQ, TRACE, "NDIS supported network type list: %u\n",
			prSupported->NumberOfItems);
	DBGLOG_MEM8(REQ, INFO, prSupported, *pu4QueryInfoLen);

	return WLAN_STATUS_SUCCESS;
}				/* wlanoidQueryNetworkTypesSupported */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the current physical layer network
*        type used by the driver.
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[out] pvQueryBuffer A pointer to the buffer that holds the result of
*             the query.
* \param[in] u4QueryBufferLen The length of the query buffer.
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                             bytes written into the query buffer. If the
*                             call failed due to invalid length of the query
*                             buffer, returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_BUFFER_TOO_SHORT
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryNetworkTypeInUse(IN P_ADAPTER_T prAdapter,
			     OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	/* TODO: need to check the OID handler content again!! */

	ENUM_PARAM_NETWORK_TYPE_T rCurrentNetworkTypeInUse = PARAM_NETWORK_TYPE_OFDM24;

	DEBUGFUNC("wlanoidQueryNetworkTypeInUse");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (u4QueryBufferLen < sizeof(ENUM_PARAM_NETWORK_TYPE_T)) {
		*pu4QueryInfoLen = sizeof(ENUM_PARAM_NETWORK_TYPE_T);
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}

	if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) == PARAM_MEDIA_STATE_CONNECTED)
		rCurrentNetworkTypeInUse = (ENUM_PARAM_NETWORK_TYPE_T) (prAdapter->rWlanInfo.ucNetworkType);
	else
		rCurrentNetworkTypeInUse = (ENUM_PARAM_NETWORK_TYPE_T) (prAdapter->rWlanInfo.ucNetworkTypeInUse);

	*(P_ENUM_PARAM_NETWORK_TYPE_T) pvQueryBuffer = rCurrentNetworkTypeInUse;
	*pu4QueryInfoLen = sizeof(ENUM_PARAM_NETWORK_TYPE_T);

	DBGLOG(REQ, TRACE, "Network type in use: %d\n", rCurrentNetworkTypeInUse);

	return WLAN_STATUS_SUCCESS;
}				/* wlanoidQueryNetworkTypeInUse */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set the physical layer network type used
*        by the driver.
*
* \param[in] pvAdapter Pointer to the Adapter structure.
* \param[in] pvSetBuffer A pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                          bytes read from the set buffer. If the call failed
*                          due to invalid length of the set buffer, returns the
*                          amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS The given network type is supported and accepted.
* \retval WLAN_STATUS_INVALID_DATA The given network type is not in the
*                                  supported list.
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetNetworkTypeInUse(IN P_ADAPTER_T prAdapter,
			   IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	/* TODO: need to check the OID handler content again!! */

	ENUM_PARAM_NETWORK_TYPE_T eNewNetworkType;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidSetNetworkTypeInUse");

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	if (u4SetBufferLen < sizeof(ENUM_PARAM_NETWORK_TYPE_T)) {
		*pu4SetInfoLen = sizeof(ENUM_PARAM_NETWORK_TYPE_T);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	eNewNetworkType = *(P_ENUM_PARAM_NETWORK_TYPE_T) pvSetBuffer;
	*pu4SetInfoLen = sizeof(ENUM_PARAM_NETWORK_TYPE_T);

	DBGLOG(REQ, INFO, "New network type: %d mode\n", eNewNetworkType);

	switch (eNewNetworkType) {

	case PARAM_NETWORK_TYPE_DS:
		prAdapter->rWlanInfo.ucNetworkTypeInUse = (UINT_8) PARAM_NETWORK_TYPE_DS;
		break;

	case PARAM_NETWORK_TYPE_OFDM5:
		prAdapter->rWlanInfo.ucNetworkTypeInUse = (UINT_8) PARAM_NETWORK_TYPE_OFDM5;
		break;

	case PARAM_NETWORK_TYPE_OFDM24:
		prAdapter->rWlanInfo.ucNetworkTypeInUse = (UINT_8) PARAM_NETWORK_TYPE_OFDM24;
		break;

	case PARAM_NETWORK_TYPE_AUTOMODE:
		prAdapter->rWlanInfo.ucNetworkTypeInUse = (UINT_8) PARAM_NETWORK_TYPE_AUTOMODE;
		break;

	case PARAM_NETWORK_TYPE_FH:
		DBGLOG(REQ, INFO, "Not support network type: %d\n", eNewNetworkType);
		rStatus = WLAN_STATUS_NOT_SUPPORTED;
		break;

	default:
		DBGLOG(REQ, INFO, "Unknown network type: %d\n", eNewNetworkType);
		rStatus = WLAN_STATUS_INVALID_DATA;
		break;
	}

	/* Verify if we support the new network type. */
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(REQ, WARN, "Unknown network type: %d\n", eNewNetworkType);

	return rStatus;
}				/* wlanoidSetNetworkTypeInUse */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the current BSSID.
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[out] pvQueryBuffer A pointer to the buffer that holds the result of
*                           the query.
* \param[in] u4QueryBufferLen The length of the query buffer.
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                             bytes written into the query buffer. If the call
*                             failed due to invalid length of the query buffer,
*                             returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_ADAPTER_NOT_READY
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryBssid(IN P_ADAPTER_T prAdapter,
		  OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidQueryBssid");

	ASSERT(prAdapter);

	if (u4QueryBufferLen < MAC_ADDR_LEN) {
		ASSERT(pu4QueryInfoLen);
		*pu4QueryInfoLen = MAC_ADDR_LEN;
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}

	ASSERT(u4QueryBufferLen >= MAC_ADDR_LEN);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) == PARAM_MEDIA_STATE_CONNECTED)
		kalMemCopy(pvQueryBuffer, prAdapter->rWlanInfo.rCurrBssId.arMacAddress, MAC_ADDR_LEN);
	else if (prAdapter->rWifiVar.rConnSettings.eOPMode == NET_TYPE_IBSS) {
		PARAM_MAC_ADDRESS aucTemp;	/*!< BSSID */

		COPY_MAC_ADDR(aucTemp, prAdapter->rWlanInfo.rCurrBssId.arMacAddress);
		aucTemp[0] &= ~BIT(0);
		aucTemp[1] |= BIT(1);
		COPY_MAC_ADDR(pvQueryBuffer, aucTemp);
	} else
		rStatus = WLAN_STATUS_ADAPTER_NOT_READY;

	*pu4QueryInfoLen = MAC_ADDR_LEN;
	return rStatus;
}				/* wlanoidQueryBssid */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the list of all BSSIDs detected by
*        the driver.
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[out] pvQueryBuffer A pointer to the buffer that holds the result of
*                           the query.
* \param[in] u4QueryBufferLen The length of the query buffer.
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                             bytes written into the query buffer. If the call
*                             failed due to invalid length of the query buffer,
*                             returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
* \retval WLAN_STATUS_ADAPTER_NOT_READY
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryBssidList(IN P_ADAPTER_T prAdapter,
		      OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	P_GLUE_INFO_T prGlueInfo;
	UINT_32 i, u4BssidListExLen;
	P_PARAM_BSSID_LIST_EX_T prList;
	P_PARAM_BSSID_EX_T prBssidEx;
	PUINT_8 cp;

	DEBUGFUNC("wlanoidQueryBssidList");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);

	if (u4QueryBufferLen) {
		ASSERT(pvQueryBuffer);

		if (!pvQueryBuffer)
			return WLAN_STATUS_INVALID_DATA;
	}

	prGlueInfo = prAdapter->prGlueInfo;

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in qeury BSSID list! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	u4BssidListExLen = 0;

	if (prAdapter->fgIsRadioOff == FALSE) {
		for (i = 0; i < prAdapter->rWlanInfo.u4ScanResultNum; i++)
			u4BssidListExLen += ALIGN_4(prAdapter->rWlanInfo.arScanResult[i].u4Length);
	}

	if (u4BssidListExLen)
		u4BssidListExLen += 4;	/* u4NumberOfItems. */
	else
		u4BssidListExLen = sizeof(PARAM_BSSID_LIST_EX_T);

	*pu4QueryInfoLen = u4BssidListExLen;

	if (u4QueryBufferLen < *pu4QueryInfoLen)
		return WLAN_STATUS_INVALID_LENGTH;

	/* Clear the buffer */
	kalMemZero(pvQueryBuffer, u4BssidListExLen);

	prList = (P_PARAM_BSSID_LIST_EX_T) pvQueryBuffer;
	cp = (PUINT_8) &prList->arBssid[0];

	if (prAdapter->fgIsRadioOff == FALSE && prAdapter->rWlanInfo.u4ScanResultNum > 0) {
		/* fill up for each entry */
		for (i = 0; i < prAdapter->rWlanInfo.u4ScanResultNum; i++) {
			prBssidEx = (P_PARAM_BSSID_EX_T) cp;

			/* copy structure */
			kalMemCopy(prBssidEx,
				   &(prAdapter->rWlanInfo.arScanResult[i]), OFFSET_OF(PARAM_BSSID_EX_T, aucIEs));

			/*For WHQL test, Rssi should be in range -10 ~ -200 dBm */
			if (prBssidEx->rRssi > PARAM_WHQL_RSSI_MAX_DBM)
				prBssidEx->rRssi = PARAM_WHQL_RSSI_MAX_DBM;

			if (prAdapter->rWlanInfo.arScanResult[i].u4IELength > 0) {
				/* copy IEs */
				kalMemCopy(prBssidEx->aucIEs,
					   prAdapter->rWlanInfo.apucScanResultIEs[i],
					   prAdapter->rWlanInfo.arScanResult[i].u4IELength);
			}
			/* 4-bytes alignement */
			prBssidEx->u4Length = ALIGN_4(prBssidEx->u4Length);

			cp += prBssidEx->u4Length;
			prList->u4NumberOfItems++;
		}
	}

	return WLAN_STATUS_SUCCESS;
}				/* wlanoidQueryBssidList */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to request the driver to perform
*        scanning.
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[in] pvSetBuffer Pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                          bytes read from the set buffer. If the call failed
*                          due to invalid length of the set buffer, returns
*                          the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_ADAPTER_NOT_READY
* \retval WLAN_STATUS_FAILURE
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetBssidListScan(IN P_ADAPTER_T prAdapter,
			IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_PARAM_SSID_T prSsid;
	PARAM_SSID_T rSsid;

	DEBUGFUNC("wlanoidSetBssidListScan()");

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in set BSSID list scan! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	ASSERT(pu4SetInfoLen);
	*pu4SetInfoLen = 0;

	if (prAdapter->fgIsRadioOff) {
		DBGLOG(REQ, WARN, "Return from BSSID list scan! (radio off). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_SUCCESS;
	}

	if (pvSetBuffer != NULL && u4SetBufferLen != 0) {
		COPY_SSID(rSsid.aucSsid, rSsid.u4SsidLen, pvSetBuffer, u4SetBufferLen);
		prSsid = &rSsid;
	} else {
		prSsid = NULL;
	}

#if CFG_SUPPORT_RDD_TEST_MODE
	if (prAdapter->prGlueInfo->rRegInfo.u4RddTestMode) {
		if ((prAdapter->fgEnOnlineScan == TRUE) && (prAdapter->ucRddStatus)) {
			if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) != PARAM_MEDIA_STATE_CONNECTED)
				aisFsmScanRequest(prAdapter, prSsid, NULL, 0);
			else
				return WLAN_STATUS_FAILURE;
		} else
			return WLAN_STATUS_FAILURE;
	} else
#endif
	{
		if (prAdapter->fgEnOnlineScan == TRUE)
			aisFsmScanRequest(prAdapter, prSsid, NULL, 0);
		else if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) != PARAM_MEDIA_STATE_CONNECTED)
			aisFsmScanRequest(prAdapter, prSsid, NULL, 0);
		else
			return WLAN_STATUS_FAILURE;
	}

	cnmTimerStartTimer(prAdapter,
		&prAdapter->rWifiVar.rAisFsmInfo.rScanDoneTimer,
		SEC_TO_MSEC(AIS_SCN_DONE_TIMEOUT_SEC));

	return WLAN_STATUS_SUCCESS;
}				/* wlanoidSetBssidListScan */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to request the driver to perform
*        scanning with attaching information elements(IEs) specified from user space
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[in] pvSetBuffer Pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                          bytes read from the set buffer. If the call failed
*                          due to invalid length of the set buffer, returns
*                          the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_ADAPTER_NOT_READY
* \retval WLAN_STATUS_FAILURE
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetBssidListScanExt(IN P_ADAPTER_T prAdapter,
			   IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_PARAM_SCAN_REQUEST_EXT_T prScanRequest;
	P_PARAM_SSID_T prSsid;
	PUINT_8 pucIe;
	UINT_32 u4IeLength;

	DEBUGFUNC("wlanoidSetBssidListScanExt()");

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in set BSSID list scan! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	ASSERT(pu4SetInfoLen);
	*pu4SetInfoLen = 0;

	if (u4SetBufferLen != sizeof(PARAM_SCAN_REQUEST_EXT_T))
		return WLAN_STATUS_INVALID_LENGTH;

	if (prAdapter->fgIsRadioOff) {
		DBGLOG(REQ, WARN, "Return from BSSID list scan! (radio off). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_SUCCESS;
	}
	DBGLOG(AIS, INFO, "ScanEX\n");
	if (pvSetBuffer != NULL && u4SetBufferLen != 0) {
		prScanRequest = (P_PARAM_SCAN_REQUEST_EXT_T) pvSetBuffer;
		prSsid = &(prScanRequest->rSsid);
		pucIe = prScanRequest->pucIE;
		u4IeLength = prScanRequest->u4IELength;
	} else {
		prScanRequest = NULL;
		prSsid = NULL;
		pucIe = NULL;
		u4IeLength = 0;
	}

#if CFG_SUPPORT_RDD_TEST_MODE
	if (prAdapter->prGlueInfo->rRegInfo.u4RddTestMode) {
		if ((prAdapter->fgEnOnlineScan == TRUE) && (prAdapter->ucRddStatus)) {
			if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) != PARAM_MEDIA_STATE_CONNECTED)
				aisFsmScanRequest(prAdapter, prSsid, pucIe, u4IeLength);
			else
				return WLAN_STATUS_FAILURE;
		} else
			return WLAN_STATUS_FAILURE;
	} else
#endif
	{
		if (prAdapter->fgEnOnlineScan == TRUE)
			aisFsmScanRequest(prAdapter, prSsid, pucIe, u4IeLength);
		else if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) != PARAM_MEDIA_STATE_CONNECTED)
			aisFsmScanRequest(prAdapter, prSsid, pucIe, u4IeLength);
		else
			return WLAN_STATUS_FAILURE;
	}

	return WLAN_STATUS_SUCCESS;
}				/* wlanoidSetBssidListScanWithIE */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to request the driver to perform
*        scanning with attaching information elements(IEs) specified from user space
*        and multiple SSID
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[in] pvSetBuffer Pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                          bytes read from the set buffer. If the call failed
*                          due to invalid length of the set buffer, returns
*                          the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_ADAPTER_NOT_READY
* \retval WLAN_STATUS_FAILURE
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetBssidListScanAdv(IN P_ADAPTER_T prAdapter,
			   IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_PARAM_SCAN_REQUEST_ADV_T prScanRequest;
	PARAM_SSID_T rSsid[CFG_SCAN_SSID_MAX_NUM];
	PUINT_8 pucIe;
	UINT_8 ucSsidNum;
	UINT_32 i, u4IeLength;

	DEBUGFUNC("wlanoidSetBssidListScanAdv()");

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in set BSSID list scan! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	ASSERT(pu4SetInfoLen);
	*pu4SetInfoLen = 0;

	if (u4SetBufferLen != sizeof(PARAM_SCAN_REQUEST_ADV_T))
		return WLAN_STATUS_INVALID_LENGTH;
	else if (pvSetBuffer == NULL)
		return WLAN_STATUS_INVALID_DATA;

	if (prAdapter->fgIsRadioOff) {
		DBGLOG(REQ, WARN, "Return from BSSID list scan! (radio off). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_SUCCESS;
	}

	prScanRequest = (P_PARAM_SCAN_REQUEST_ADV_T) pvSetBuffer;

	ucSsidNum = (UINT_8) (prScanRequest->u4SsidNum);
	for (i = 0; i < prScanRequest->u4SsidNum; i++) {
		if (prScanRequest->rSsid[i].u4SsidLen > ELEM_MAX_LEN_SSID) {
			DBGLOG(REQ, ERROR,
			       "[%s] SSID(%s) Length(%d) is over than ELEM_MAX_LEN_SSID(%d)\n",
			       __func__, prScanRequest->rSsid[i].aucSsid,
			       prScanRequest->rSsid[i].u4SsidLen, ELEM_MAX_LEN_SSID);
			DBGLOG_MEM8(REQ, ERROR, prScanRequest, sizeof(PARAM_SCAN_REQUEST_ADV_T));

		}
		COPY_SSID(rSsid[i].aucSsid,
			  rSsid[i].u4SsidLen, prScanRequest->rSsid[i].aucSsid, prScanRequest->rSsid[i].u4SsidLen);
	}

	pucIe = prScanRequest->pucIE;
	u4IeLength = prScanRequest->u4IELength;

#if CFG_SUPPORT_RDD_TEST_MODE
	if (prAdapter->prGlueInfo->rRegInfo.u4RddTestMode) {
		if ((prAdapter->fgEnOnlineScan == TRUE) && (prAdapter->ucRddStatus)) {
			if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) != PARAM_MEDIA_STATE_CONNECTED)
#if CFG_SCAN_CHANNEL_SPECIFIED
				aisFsmScanRequestAdv(prAdapter, ucSsidNum, rSsid,
					prScanRequest->ucChannelListNum, prScanRequest->arChnlInfoList,
					pucIe, u4IeLength);
#else
				aisFsmScanRequestAdv(prAdapter, ucSsidNum, rSsid, 0, NULL, pucIe, u4IeLength);
#endif
			else
				return WLAN_STATUS_FAILURE;
		} else
			return WLAN_STATUS_FAILURE;
	} else
#endif
	{
		if (prAdapter->fgEnOnlineScan == TRUE)
#if CFG_SCAN_CHANNEL_SPECIFIED
			aisFsmScanRequestAdv(prAdapter, ucSsidNum, rSsid,
				prScanRequest->ucChannelListNum, prScanRequest->arChnlInfoList,
				pucIe, u4IeLength);
#else
			aisFsmScanRequestAdv(prAdapter, ucSsidNum, rSsid, 0, NULL, pucIe, u4IeLength);
#endif
		else if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) != PARAM_MEDIA_STATE_CONNECTED)
#if CFG_SCAN_CHANNEL_SPECIFIED
			aisFsmScanRequestAdv(prAdapter, ucSsidNum, rSsid,
				prScanRequest->ucChannelListNum, prScanRequest->arChnlInfoList,
				pucIe, u4IeLength);
#else
			aisFsmScanRequestAdv(prAdapter, ucSsidNum, rSsid, 0, NULL, pucIe, u4IeLength);
#endif
		else
			return WLAN_STATUS_FAILURE;
	}
	cnmTimerStartTimer(prAdapter, &prAdapter->rWifiVar.rAisFsmInfo.rScanDoneTimer,
			   SEC_TO_MSEC(AIS_SCN_DONE_TIMEOUT_SEC));
	return WLAN_STATUS_SUCCESS;
}				/* wlanoidSetBssidListScanAdv */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine will initiate the join procedure to attempt to associate
*        with the specified BSSID.
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
* \retval WLAN_STATUS_INVALID_DATA
* \retval WLAN_STATUS_ADAPTER_NOT_READY
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetBssid(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_GLUE_INFO_T prGlueInfo;
	P_UINT_8 pAddr;
	UINT_32 i;
	INT_32 i4Idx = -1;
	P_MSG_AIS_ABORT_T prAisAbortMsg;
	UINT_8 ucReasonOfDisconnect;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = MAC_ADDR_LEN;
	if (u4SetBufferLen != MAC_ADDR_LEN) {
		*pu4SetInfoLen = MAC_ADDR_LEN;
		return WLAN_STATUS_INVALID_LENGTH;
	} else if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN, "Fail in set ssid! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	prGlueInfo = prAdapter->prGlueInfo;
	pAddr = (P_UINT_8) pvSetBuffer;

	/* re-association check */
	if (kalGetMediaStateIndicated(prGlueInfo) == PARAM_MEDIA_STATE_CONNECTED) {
		if (EQUAL_MAC_ADDR(prAdapter->rWlanInfo.rCurrBssId.arMacAddress, pAddr)) {
			kalSetMediaStateIndicated(prGlueInfo, PARAM_MEDIA_STATE_TO_BE_INDICATED);
			ucReasonOfDisconnect = DISCONNECT_REASON_CODE_REASSOCIATION;
		} else {
			kalIndicateStatusAndComplete(prGlueInfo, WLAN_STATUS_MEDIA_DISCONNECT, NULL, 0);
			ucReasonOfDisconnect = DISCONNECT_REASON_CODE_NEW_CONNECTION;
		}
	} else {
		ucReasonOfDisconnect = DISCONNECT_REASON_CODE_NEW_CONNECTION;
	}

	/* check if any scanned result matchs with the BSSID */
	for (i = 0; i < prAdapter->rWlanInfo.u4ScanResultNum; i++) {
		if (EQUAL_MAC_ADDR(prAdapter->rWlanInfo.arScanResult[i].arMacAddress, pAddr)) {
			i4Idx = (INT_32) i;
			break;
		}
	}

	/* prepare message to AIS */
	if (prAdapter->rWifiVar.rConnSettings.eOPMode == NET_TYPE_IBSS
	    || prAdapter->rWifiVar.rConnSettings.eOPMode == NET_TYPE_DEDICATED_IBSS) {
		/* IBSS *//* beacon period */
		prAdapter->rWifiVar.rConnSettings.u2BeaconPeriod = prAdapter->rWlanInfo.u2BeaconPeriod;
		prAdapter->rWifiVar.rConnSettings.u2AtimWindow = prAdapter->rWlanInfo.u2AtimWindow;
	}

	/* Set Connection Request Issued Flag */
	prAdapter->rWifiVar.rConnSettings.fgIsConnReqIssued = TRUE;
	prAdapter->rWifiVar.rConnSettings.eConnectionPolicy = CONNECT_BY_BSSID;

	/* Send AIS Abort Message */
	prAisAbortMsg = (P_MSG_AIS_ABORT_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_AIS_ABORT_T));
	if (!prAisAbortMsg) {
		DBGLOG(REQ, ERROR, "Fail in allocating AisAbortMsg.\n");
		return WLAN_STATUS_FAILURE;
	}

	prAisAbortMsg->rMsgHdr.eMsgId = MID_OID_AIS_FSM_JOIN_REQ;
	prAisAbortMsg->ucReasonOfDisconnect = ucReasonOfDisconnect;

	/* Update the information to CONNECTION_SETTINGS_T */
	prAdapter->rWifiVar.rConnSettings.ucSSIDLen = 0;
	prAdapter->rWifiVar.rConnSettings.aucSSID[0] = '\0';
	COPY_MAC_ADDR(prAdapter->rWifiVar.rConnSettings.aucBSSID, pAddr);

	if (EQUAL_MAC_ADDR(prAdapter->rWlanInfo.rCurrBssId.arMacAddress, pAddr))
		prAisAbortMsg->fgDelayIndication = TRUE;
	else
		prAisAbortMsg->fgDelayIndication = FALSE;

#if CFG_DISCONN_DEBUG_FEATURE
	/* used to disconnect debug capability */
	g_rDisconnInfoTemp.ucTrigger = DISCONNECT_TRIGGER_ACTIVE;
#endif

	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prAisAbortMsg, MSG_SEND_METHOD_BUF);

	return WLAN_STATUS_SUCCESS;
}				/* end of wlanoidSetBssid() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine will initiate the join procedure to attempt
*        to associate with the new SSID. If the previous scanning
*        result is aged, we will scan the channels at first.
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[in] pvSetBuffer Pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                           bytes read from the set buffer. If the call failed
*                           due to invalid length of the set buffer, returns
*                           the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_DATA
* \retval WLAN_STATUS_ADAPTER_NOT_READY
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetSsid(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_GLUE_INFO_T prGlueInfo;
	P_PARAM_SSID_T pParamSsid;
	UINT_32 i;
	INT_32 i4Idx = -1, i4MaxRSSI = INT_MIN;
	P_MSG_AIS_ABORT_T prAisAbortMsg;
	BOOLEAN fgIsValidSsid = TRUE;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	/* MSDN:
	 * Powering on the radio if the radio is powered off through a setting of OID_802_11_DISASSOCIATE
	 */
	if (prAdapter->fgIsRadioOff == TRUE)
		prAdapter->fgIsRadioOff = FALSE;

	if (u4SetBufferLen < sizeof(PARAM_SSID_T) || u4SetBufferLen > sizeof(PARAM_SSID_T))
		return WLAN_STATUS_INVALID_LENGTH;
	else if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN, "Fail in set ssid! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	pParamSsid = (P_PARAM_SSID_T) pvSetBuffer;

	if (pParamSsid->u4SsidLen > 32)
		return WLAN_STATUS_INVALID_LENGTH;

	prGlueInfo = prAdapter->prGlueInfo;

	/* prepare for CMD_BUILD_CONNECTION & CMD_GET_CONNECTION_STATUS */
	/* re-association check */
	if (kalGetMediaStateIndicated(prGlueInfo) == PARAM_MEDIA_STATE_CONNECTED) {
		if (EQUAL_SSID(prAdapter->rWlanInfo.rCurrBssId.rSsid.aucSsid,
			       prAdapter->rWlanInfo.rCurrBssId.rSsid.u4SsidLen,
			       pParamSsid->aucSsid, pParamSsid->u4SsidLen)) {
			kalSetMediaStateIndicated(prGlueInfo, PARAM_MEDIA_STATE_TO_BE_INDICATED);
		} else
			kalIndicateStatusAndComplete(prGlueInfo, WLAN_STATUS_MEDIA_DISCONNECT, NULL, 0);
	}
	/* check if any scanned result matchs with the SSID */
	for (i = 0; i < prAdapter->rWlanInfo.u4ScanResultNum; i++) {
		PUINT_8 aucSsid = prAdapter->rWlanInfo.arScanResult[i].rSsid.aucSsid;
		UINT_8 ucSsidLength = (UINT_8) prAdapter->rWlanInfo.arScanResult[i].rSsid.u4SsidLen;
		INT_32 i4RSSI = prAdapter->rWlanInfo.arScanResult[i].rRssi;

		if (EQUAL_SSID(aucSsid, ucSsidLength, pParamSsid->aucSsid, pParamSsid->u4SsidLen) &&
		    i4RSSI >= i4MaxRSSI) {
			i4Idx = (INT_32) i;
			i4MaxRSSI = i4RSSI;
		}
	}

	/* prepare message to AIS */
	if (prAdapter->rWifiVar.rConnSettings.eOPMode == NET_TYPE_IBSS
	    || prAdapter->rWifiVar.rConnSettings.eOPMode == NET_TYPE_DEDICATED_IBSS) {
		/* IBSS *//* beacon period */
		prAdapter->rWifiVar.rConnSettings.u2BeaconPeriod = prAdapter->rWlanInfo.u2BeaconPeriod;
		prAdapter->rWifiVar.rConnSettings.u2AtimWindow = prAdapter->rWlanInfo.u2AtimWindow;
	}

	if (prAdapter->rWifiVar.fgSupportWZCDisassociation) {
		if (pParamSsid->u4SsidLen == ELEM_MAX_LEN_SSID) {
			fgIsValidSsid = FALSE;

			for (i = 0; i < ELEM_MAX_LEN_SSID; i++) {
				if (!((pParamSsid->aucSsid[i] > 0)
				      && (pParamSsid->aucSsid[i] <= 0x1F))) {
					fgIsValidSsid = TRUE;
					break;
				}
			}
		}
	}

	/* Set Connection Request Issued Flag */
	if (fgIsValidSsid) {
		prAdapter->rWifiVar.rConnSettings.fgIsConnReqIssued = TRUE;

		if (pParamSsid->u4SsidLen)
			prAdapter->rWifiVar.rConnSettings.eConnectionPolicy = CONNECT_BY_SSID_BEST_RSSI;
		else
			/* wildcard SSID */
			prAdapter->rWifiVar.rConnSettings.eConnectionPolicy = CONNECT_BY_SSID_ANY;
	} else
		prAdapter->rWifiVar.rConnSettings.fgIsConnReqIssued = FALSE;

	/* Send AIS Abort Message */
	prAisAbortMsg = (P_MSG_AIS_ABORT_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_AIS_ABORT_T));
	if (!prAisAbortMsg) {
		DBGLOG(REQ, ERROR, "Fail in allocating AisAbortMsg.\n");
		return WLAN_STATUS_FAILURE;
	}

	prAisAbortMsg->rMsgHdr.eMsgId = MID_OID_AIS_FSM_JOIN_REQ;
	prAisAbortMsg->ucReasonOfDisconnect = DISCONNECT_REASON_CODE_NEW_CONNECTION;
	COPY_SSID(prAdapter->rWifiVar.rConnSettings.aucSSID,
		  prAdapter->rWifiVar.rConnSettings.ucSSIDLen, pParamSsid->aucSsid, (UINT_8) pParamSsid->u4SsidLen);

	if (EQUAL_SSID(prAdapter->rWlanInfo.rCurrBssId.rSsid.aucSsid,
		       prAdapter->rWlanInfo.rCurrBssId.rSsid.u4SsidLen, pParamSsid->aucSsid, pParamSsid->u4SsidLen)) {
		prAisAbortMsg->fgDelayIndication = TRUE;
	} else {
		/* Update the information to CONNECTION_SETTINGS_T */
		prAisAbortMsg->fgDelayIndication = FALSE;
	}
	DBGLOG(SCN, INFO, "SSID %s\n", prAdapter->rWifiVar.rConnSettings.aucSSID);

#if CFG_DISCONN_DEBUG_FEATURE
	/* used to disconnect debug capability */
	g_rDisconnInfoTemp.ucTrigger = DISCONNECT_TRIGGER_ACTIVE;
#endif

	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prAisAbortMsg, MSG_SEND_METHOD_BUF);

	return WLAN_STATUS_SUCCESS;

}				/* end of wlanoidSetSsid() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine will initiate the join procedure to attempt
*        to associate with the new BSS, base on given SSID, BSSID, and freqency.
*	If the target connecting BSS is in the same ESS as current connected BSS, roaming
*	will be performed.
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[in] pvSetBuffer Pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                           bytes read from the set buffer. If the call failed
*                           due to invalid length of the set buffer, returns
*                           the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_DATA
* \retval WLAN_STATUS_ADAPTER_NOT_READY
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetConnect(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_GLUE_INFO_T prGlueInfo;
	P_PARAM_CONNECT_T pParamConn;
	P_CONNECTION_SETTINGS_T prConnSettings;
	UINT_32 i;
	P_MSG_AIS_ABORT_T prAisAbortMsg;
	BOOLEAN fgIsValidSsid = TRUE;
	BOOLEAN fgEqualSsid = FALSE;
	BOOLEAN fgEqualBssid = FALSE;
	const UINT_8 aucZeroMacAddr[] = NULL_MAC_ADDR;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	/* MSDN:
	 * Powering on the radio if the radio is powered off through a setting of OID_802_11_DISASSOCIATE
	 */
	if (prAdapter->fgIsRadioOff == TRUE)
		prAdapter->fgIsRadioOff = FALSE;

	if (u4SetBufferLen != sizeof(PARAM_CONNECT_T))
		return WLAN_STATUS_INVALID_LENGTH;
	else if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN, "Fail in set ssid! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}
	prAisAbortMsg = (P_MSG_AIS_ABORT_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_AIS_ABORT_T));
	if (!prAisAbortMsg) {
		DBGLOG(REQ, ERROR, "Fail in allocating AisAbortMsg.\n");
		return WLAN_STATUS_FAILURE;
	}
	prAisAbortMsg->rMsgHdr.eMsgId = MID_OID_AIS_FSM_JOIN_REQ;

	pParamConn = (P_PARAM_CONNECT_T) pvSetBuffer;
	prConnSettings = &prAdapter->rWifiVar.rConnSettings;

	if (pParamConn->u4SsidLen > 32) {
		cnmMemFree(prAdapter, prAisAbortMsg);
		DBGLOG(OID, WARN, "SsidLen [%d] is invalid!\n",
		       pParamConn->u4SsidLen);
		return WLAN_STATUS_INVALID_LENGTH;
	} else if (!pParamConn->pucBssid && !pParamConn->pucSsid) {
		cnmMemFree(prAdapter, prAisAbortMsg);
		DBGLOG(OID, WARN, "Bssid or ssid is invalid!\n");
		return WLAN_STATUS_INVALID_LENGTH;
	}

	prGlueInfo = prAdapter->prGlueInfo;
	kalMemZero(prConnSettings->aucSSID, sizeof(prConnSettings->aucSSID));
	prConnSettings->ucSSIDLen = 0;
	kalMemZero(prConnSettings->aucBSSID, sizeof(prConnSettings->aucBSSID));
	prConnSettings->eConnectionPolicy = CONNECT_BY_SSID_ANY;
	prConnSettings->fgIsConnByBssidIssued = FALSE;

	if (pParamConn->pucSsid) {
		prConnSettings->eConnectionPolicy = CONNECT_BY_SSID_BEST_RSSI;
		COPY_SSID(prConnSettings->aucSSID,
			  prConnSettings->ucSSIDLen, pParamConn->pucSsid, (UINT_8) pParamConn->u4SsidLen);
		if (EQUAL_SSID(prAdapter->rWlanInfo.rCurrBssId.rSsid.aucSsid,
			       prAdapter->rWlanInfo.rCurrBssId.rSsid.u4SsidLen,
			       pParamConn->pucSsid, pParamConn->u4SsidLen))
			fgEqualSsid = TRUE;
	}
	if (pParamConn->pucBssid) {
		if (!EQUAL_MAC_ADDR(aucZeroMacAddr, pParamConn->pucBssid) && IS_UCAST_MAC_ADDR(pParamConn->pucBssid)) {
			prConnSettings->eConnectionPolicy = CONNECT_BY_BSSID;
			prConnSettings->fgIsConnByBssidIssued = TRUE;
			COPY_MAC_ADDR(prConnSettings->aucBSSID, pParamConn->pucBssid);
			if (EQUAL_MAC_ADDR(prAdapter->rWlanInfo.rCurrBssId.arMacAddress, pParamConn->pucBssid))
				fgEqualBssid = TRUE;
		} else
			DBGLOG(INIT, INFO, "wrong bssid " MACSTR "to connect\n", MAC2STR(pParamConn->pucBssid));
	} else
		DBGLOG(INIT, INFO, "No Bssid set\n");
	prConnSettings->u4FreqInKHz = pParamConn->u4CenterFreq;

	/* prepare for CMD_BUILD_CONNECTION & CMD_GET_CONNECTION_STATUS */
	/* re-association check */
	if (kalGetMediaStateIndicated(prGlueInfo) == PARAM_MEDIA_STATE_CONNECTED) {
		if (fgEqualSsid) {
			prAisAbortMsg->ucReasonOfDisconnect = DISCONNECT_REASON_CODE_REASSOCIATION;
			if (fgEqualBssid)
				kalSetMediaStateIndicated(prGlueInfo, PARAM_MEDIA_STATE_TO_BE_INDICATED);
		} else {
			DBGLOG(INIT, INFO, "DisBySsid\n");
			kalIndicateStatusAndComplete(prGlueInfo, WLAN_STATUS_MEDIA_DISCONNECT, NULL, 0);
			prAisAbortMsg->ucReasonOfDisconnect = DISCONNECT_REASON_CODE_NEW_CONNECTION;
		}
	} else
		prAisAbortMsg->ucReasonOfDisconnect = DISCONNECT_REASON_CODE_NEW_CONNECTION;

#if 0
	/* check if any scanned result matchs with the SSID */
	for (i = 0; i < prAdapter->rWlanInfo.u4ScanResultNum; i++) {
		PUINT_8 aucSsid = prAdapter->rWlanInfo.arScanResult[i].rSsid.aucSsid;
		UINT_8 ucSsidLength = (UINT_8) prAdapter->rWlanInfo.arScanResult[i].rSsid.u4SsidLen;
		INT_32 i4RSSI = prAdapter->rWlanInfo.arScanResult[i].rRssi;

		if (EQUAL_SSID(aucSsid, ucSsidLength, pParamConn->pucSsid, pParamConn->u4SsidLen) &&
		    i4RSSI >= i4MaxRSSI) {
			i4Idx = (INT_32) i;
			i4MaxRSSI = i4RSSI;
		}
		if (EQUAL_MAC_ADDR(prAdapter->rWlanInfo.arScanResult[i].arMacAddress, pAddr)) {
			i4Idx = (INT_32) i;
			break;
		}
	}
#endif
	/* prepare message to AIS */
	if (prConnSettings->eOPMode == NET_TYPE_IBSS || prConnSettings->eOPMode == NET_TYPE_DEDICATED_IBSS) {
		/* IBSS *//* beacon period */
		prConnSettings->u2BeaconPeriod = prAdapter->rWlanInfo.u2BeaconPeriod;
		prConnSettings->u2AtimWindow = prAdapter->rWlanInfo.u2AtimWindow;
	}

	if (prAdapter->rWifiVar.fgSupportWZCDisassociation) {
		if (pParamConn->u4SsidLen == ELEM_MAX_LEN_SSID) {
			fgIsValidSsid = FALSE;

			if (pParamConn->pucSsid) {
				for (i = 0; i < ELEM_MAX_LEN_SSID; i++) {
					if (!((pParamConn->pucSsid[i] > 0) && (pParamConn->pucSsid[i] <= 0x1F))) {
						fgIsValidSsid = TRUE;
						break;
					}
				}
			} else {
				DBGLOG(INIT, ERROR, "pParamConn->pucSsid is NULL\n");
			}
		}
	}

	/* Set Connection Request Issued Flag */
	if (fgIsValidSsid)
		prConnSettings->fgIsConnReqIssued = TRUE;
	else
		prConnSettings->fgIsConnReqIssued = FALSE;

	if (fgEqualSsid || fgEqualBssid)
		prAisAbortMsg->fgDelayIndication = TRUE;
	else
		/* Update the information to CONNECTION_SETTINGS_T */
		prAisAbortMsg->fgDelayIndication = FALSE;

#if CFG_DISCONN_DEBUG_FEATURE
	/* used to disconnect debug capability */
	g_rDisconnInfoTemp.ucTrigger = DISCONNECT_TRIGGER_ACTIVE;
#endif

	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prAisAbortMsg, MSG_SEND_METHOD_BUF);

	DBGLOG(INIT, INFO, "ssid %s, bssid " MACSTR ", conn policy %d, disc reason %d\n",
	       prConnSettings->aucSSID, MAC2STR(prConnSettings->aucBSSID),
	       prConnSettings->eConnectionPolicy, prAisAbortMsg->ucReasonOfDisconnect);
	return WLAN_STATUS_SUCCESS;
}				/* end of wlanoidSetConnect */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the currently associated SSID.
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[in] pvQueryBuffer Pointer to the buffer that holds the result of
*                           the query.
* \param[in] u4QueryBufferLen The length of the query buffer.
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                             bytes written into the query buffer. If the call
*                             failed due to invalid length of the query buffer,
*                             returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQuerySsid(IN P_ADAPTER_T prAdapter,
		 OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	P_PARAM_SSID_T prAssociatedSsid;

	DEBUGFUNC("wlanoidQuerySsid");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);

	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(PARAM_SSID_T);

	/* Check for query buffer length */
	if (u4QueryBufferLen < *pu4QueryInfoLen) {
		DBGLOG(REQ, WARN, "Invalid length %u\n", u4QueryBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	prAssociatedSsid = (P_PARAM_SSID_T) pvQueryBuffer;

	kalMemZero(prAssociatedSsid->aucSsid, sizeof(prAssociatedSsid->aucSsid));

	if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) == PARAM_MEDIA_STATE_CONNECTED) {
		prAssociatedSsid->u4SsidLen = prAdapter->rWlanInfo.rCurrBssId.rSsid.u4SsidLen;

		if (prAssociatedSsid->u4SsidLen) {
			kalMemCopy(prAssociatedSsid->aucSsid,
				   prAdapter->rWlanInfo.rCurrBssId.rSsid.aucSsid, prAssociatedSsid->u4SsidLen);
		}
	} else {
		prAssociatedSsid->u4SsidLen = 0;

		DBGLOG(REQ, TRACE, "Null SSID\n");
	}

	return WLAN_STATUS_SUCCESS;
}				/* wlanoidQuerySsid */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the current 802.11 network type.
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[in] pvQueryBuffer Pointer to the buffer that holds the result of
*                           the query.
* \param[in] u4QueryBufferLen The length of the query buffer.
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                             bytes written into the query buffer. If the call
*                             failed due to invalid length of the query buffer,
*                             returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_BUFFER_TOO_SHORT
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryInfrastructureMode(IN P_ADAPTER_T prAdapter,
			       OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	DEBUGFUNC("wlanoidQueryInfrastructureMode");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);

	*pu4QueryInfoLen = sizeof(ENUM_PARAM_OP_MODE_T);

	if (u4QueryBufferLen < sizeof(ENUM_PARAM_OP_MODE_T))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*(P_ENUM_PARAM_OP_MODE_T) pvQueryBuffer = prAdapter->rWifiVar.rConnSettings.eOPMode;

	/*
	 ** According to OID_802_11_INFRASTRUCTURE_MODE
	 ** If there is no prior OID_802_11_INFRASTRUCTURE_MODE,
	 ** NDIS_STATUS_ADAPTER_NOT_READY shall be returned.
	 */
#if DBG
	switch (*(P_ENUM_PARAM_OP_MODE_T) pvQueryBuffer) {
	case NET_TYPE_IBSS:
		DBGLOG(REQ, INFO, "IBSS mode\n");
		break;
	case NET_TYPE_INFRA:
		DBGLOG(REQ, INFO, "Infrastructure mode\n");
		break;
	default:
		DBGLOG(REQ, INFO, "Automatic mode\n");
	}
#endif

	return WLAN_STATUS_SUCCESS;
}				/* wlanoidQueryInfrastructureMode */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set mode to infrastructure or
*        IBSS, or automatic switch between the two.
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[in] pvSetBuffer Pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*             bytes read from the set buffer. If the call failed due to invalid
*             length of the set buffer, returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_DATA
* \retval WLAN_STATUS_ADAPTER_NOT_READY
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetInfrastructureMode(IN P_ADAPTER_T prAdapter,
			     IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_GLUE_INFO_T prGlueInfo;
	ENUM_PARAM_OP_MODE_T eOpMode;
/* P_WLAN_TABLE_T       prWlanTable; */
#if CFG_SUPPORT_802_11W
/* P_AIS_SPECIFIC_BSS_INFO_T prAisSpecBssInfo; */
#endif
/* P_BSS_INFO_T         prBssInfo; */
/* UINT_8 i; */

	DEBUGFUNC("wlanoidSetInfrastructureMode");

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	prGlueInfo = prAdapter->prGlueInfo;

	if (u4SetBufferLen < sizeof(ENUM_PARAM_OP_MODE_T))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	*pu4SetInfoLen = sizeof(ENUM_PARAM_OP_MODE_T);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in set infrastructure mode! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	eOpMode = *(P_ENUM_PARAM_OP_MODE_T) pvSetBuffer;
	/* Verify the new infrastructure mode. */
	if (eOpMode >= NET_TYPE_NUM) {
		DBGLOG(REQ, TRACE, "Invalid mode value %d\n", eOpMode);
		return WLAN_STATUS_INVALID_DATA;
	}

	/* check if possible to switch to AdHoc mode */
	if (eOpMode == NET_TYPE_IBSS || eOpMode == NET_TYPE_DEDICATED_IBSS) {
		if (cnmAisIbssIsPermitted(prAdapter) == FALSE) {
			DBGLOG(REQ, TRACE, "Mode value %d unallowed\n", eOpMode);
			return WLAN_STATUS_FAILURE;
		}
	}

	/* Save the new infrastructure mode setting. */
	prAdapter->rWifiVar.rConnSettings.eOPMode = eOpMode;

	prAdapter->rWifiVar.rConnSettings.fgWapiMode = FALSE;
#if CFG_SUPPORT_WAPI
	prAdapter->prGlueInfo->u2WapiAssocInfoIESz = 0;
	kalMemZero(&prAdapter->prGlueInfo->aucWapiAssocInfoIEs, 42);
#endif

#if CFG_SUPPORT_802_11W
	prAdapter->rWifiVar.rAisSpecificBssInfo.fgMgmtProtection = FALSE;
	prAdapter->rWifiVar.rAisSpecificBssInfo.fgBipKeyInstalled = FALSE;
#endif

#if CFG_SUPPORT_WPS2
	kalMemZero(&prAdapter->prGlueInfo->aucWSCAssocInfoIE, 200);
	prAdapter->prGlueInfo->u2WSCAssocInfoIELen = 0;
#endif

#if 0				/* STA record remove at AIS_ABORT nicUpdateBss and DISCONNECT */
	for (i = 0; i < BSS_INFO_NUM; i++) {
		prBssInfo = prAdapter->aprBssInfo[i];
		if (prBssInfo->eNetworkType == NETWORK_TYPE_AIS)
			cnmStaFreeAllStaByNetwork(prAdapter, prBssInfo->ucBssIndex, 0);
	}
#endif

	/* Clean up the Tx key flag */
	prAdapter->prAisBssInfo->fgBcDefaultKeyExist = FALSE;
	prAdapter->prAisBssInfo->ucBcDefaultKeyIdx = 0xFF;

	/* prWlanTable = prAdapter->rWifiVar.arWtbl; */
	/* prWlanTable[prAdapter->prAisBssInfo->ucBMCWlanIndex].ucKeyId = 0; */

#if DBG
	DBGLOG(RSN, TRACE, "wlanoidSetInfrastructureMode\n");
#endif

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_INFRASTRUCTURE,
				   TRUE,
				   FALSE,
				   g_fgIsOid,
				   nicCmdEventSetCommon, nicOidCmdTimeoutCommon, 0, NULL, pvSetBuffer, u4SetBufferLen);
}				/* wlanoidSetInfrastructureMode */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the current 802.11 authentication
*        mode.
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[out] pvQueryBuffer A pointer to the buffer that holds the result of
*                           the query.
* \param[in] u4QueryBufferLen The length of the query buffer.
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                            bytes written into the query buffer. If the call
*                            failed due to invalid length of the query buffer,
*                            returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_BUFFER_TOO_SHORT
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryAuthMode(IN P_ADAPTER_T prAdapter,
		     OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	DEBUGFUNC("wlanoidQueryAuthMode");

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	*pu4QueryInfoLen = sizeof(ENUM_PARAM_AUTH_MODE_T);

	if (u4QueryBufferLen < sizeof(ENUM_PARAM_AUTH_MODE_T))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	*(P_ENUM_PARAM_AUTH_MODE_T) pvQueryBuffer = prAdapter->rWifiVar.rConnSettings.eAuthMode;

#if DBG
	switch (*(P_ENUM_PARAM_AUTH_MODE_T) pvQueryBuffer) {
	case AUTH_MODE_OPEN:
		DBGLOG(REQ, INFO, "Current auth mode: Open\n");
		break;

	case AUTH_MODE_SHARED:
		DBGLOG(REQ, INFO, "Current auth mode: Shared\n");
		break;

	case AUTH_MODE_AUTO_SWITCH:
		DBGLOG(REQ, INFO, "Current auth mode: Auto-switch\n");
		break;

	case AUTH_MODE_WPA:
		DBGLOG(REQ, INFO, "Current auth mode: WPA\n");
		break;

	case AUTH_MODE_WPA_PSK:
		DBGLOG(REQ, INFO, "Current auth mode: WPA PSK\n");
		break;

	case AUTH_MODE_WPA_NONE:
		DBGLOG(REQ, INFO, "Current auth mode: WPA None\n");
		break;

	case AUTH_MODE_WPA2:
		DBGLOG(REQ, INFO, "Current auth mode: WPA2\n");
		break;

	case AUTH_MODE_WPA2_PSK:
		DBGLOG(REQ, INFO, "Current auth mode: WPA2 PSK\n");
		break;

#if CFG_SUPPORT_CFG80211_AUTH
	case AUTH_MODE_WPA2_SAE:
		DBGLOG(REQ, INFO, "Current auth mode: SAE\n");
		break;
#endif

	default:
		DBGLOG(REQ, INFO, "Current auth mode: %d\n", *(P_ENUM_PARAM_AUTH_MODE_T) pvQueryBuffer);
		break;
	}
#endif
	return WLAN_STATUS_SUCCESS;
}				/* wlanoidQueryAuthMode */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set the IEEE 802.11 authentication mode
*        to the driver.
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
* \retval WLAN_STATUS_ADAPTER_NOT_READY
* \retval WLAN_STATUS_NOT_ACCEPTED
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetAuthMode(IN P_ADAPTER_T prAdapter,
		   IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_GLUE_INFO_T prGlueInfo;
	/* UINT_32       i, u4AkmSuite; */
	/* P_DOT11_RSNA_CONFIG_AUTHENTICATION_SUITES_ENTRY prEntry; */

	DEBUGFUNC("wlanoidSetAuthMode");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	prGlueInfo = prAdapter->prGlueInfo;

	*pu4SetInfoLen = sizeof(ENUM_PARAM_AUTH_MODE_T);

	if (u4SetBufferLen < sizeof(ENUM_PARAM_AUTH_MODE_T))
		return WLAN_STATUS_INVALID_LENGTH;

	/* RF Test */
	/* if (IS_ARB_IN_RFTEST_STATE(prAdapter)) { */
	/* return WLAN_STATUS_SUCCESS; */
	/* } */

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in set Authentication mode! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	/* Check if the new authentication mode is valid. */
	if (*(P_ENUM_PARAM_AUTH_MODE_T) pvSetBuffer >= AUTH_MODE_NUM) {
		DBGLOG(REQ, TRACE, "Invalid auth mode %d\n", *(P_ENUM_PARAM_AUTH_MODE_T) pvSetBuffer);
		return WLAN_STATUS_INVALID_DATA;
	}

	switch (*(P_ENUM_PARAM_AUTH_MODE_T) pvSetBuffer) {
	case AUTH_MODE_WPA:
	case AUTH_MODE_WPA_PSK:
	case AUTH_MODE_WPA2:
	case AUTH_MODE_WPA2_PSK:
		/* infrastructure mode only */
		if (prAdapter->rWifiVar.rConnSettings.eOPMode != NET_TYPE_INFRA)
			return WLAN_STATUS_NOT_ACCEPTED;
		break;

	case AUTH_MODE_WPA_NONE:
		/* ad hoc mode only */
		if (prAdapter->rWifiVar.rConnSettings.eOPMode != NET_TYPE_IBSS)
			return WLAN_STATUS_NOT_ACCEPTED;
		break;

	default:
		break;
	}

	/* Save the new authentication mode. */
	prAdapter->rWifiVar.rConnSettings.eAuthMode = *(P_ENUM_PARAM_AUTH_MODE_T) pvSetBuffer;

#if 1				/* DBG */
	switch (prAdapter->rWifiVar.rConnSettings.eAuthMode) {
	case AUTH_MODE_OPEN:
		DBGLOG(RSN, TRACE, "New auth mode: open\n");
		break;

	case AUTH_MODE_SHARED:
		DBGLOG(RSN, TRACE, "New auth mode: shared\n");
		break;

	case AUTH_MODE_AUTO_SWITCH:
		DBGLOG(RSN, TRACE, "New auth mode: auto-switch\n");
		break;

	case AUTH_MODE_WPA:
		DBGLOG(RSN, TRACE, "New auth mode: WPA\n");
		break;

	case AUTH_MODE_WPA_PSK:
		DBGLOG(RSN, TRACE, "New auth mode: WPA PSK\n");
		break;

	case AUTH_MODE_WPA_NONE:
		DBGLOG(RSN, TRACE, "New auth mode: WPA None\n");
		break;

	case AUTH_MODE_WPA2:
		DBGLOG(RSN, TRACE, "New auth mode: WPA2\n");
		break;

	case AUTH_MODE_WPA2_PSK:
		DBGLOG(RSN, TRACE, "New auth mode: WPA2 PSK\n");
		break;
#if CFG_SUPPORT_SAE
	case AUTH_MODE_WPA2_SAE:
		DBGLOG(RSN, INFO, "New auth mode: SAE\n");
		break;
#endif
	default:
		DBGLOG(RSN, TRACE, "New auth mode: unknown (%d)\n", prAdapter->rWifiVar.rConnSettings.eAuthMode);
	}
#endif

#if 0
	if (prAdapter->rWifiVar.rConnSettings.eAuthMode >= AUTH_MODE_WPA) {
		switch (prAdapter->rWifiVar.rConnSettings.eAuthMode) {
		case AUTH_MODE_WPA:
			u4AkmSuite = WPA_AKM_SUITE_802_1X;
			break;

		case AUTH_MODE_WPA_PSK:
			u4AkmSuite = WPA_AKM_SUITE_PSK;
			break;

		case AUTH_MODE_WPA_NONE:
			u4AkmSuite = WPA_AKM_SUITE_NONE;
			break;

		case AUTH_MODE_WPA2:
			u4AkmSuite = RSN_AKM_SUITE_802_1X;
			break;

		case AUTH_MODE_WPA2_PSK:
			u4AkmSuite = RSN_AKM_SUITE_PSK;
			break;

		default:
			u4AkmSuite = 0;
		}
	} else {
		u4AkmSuite = 0;
	}

	/* Enable the specific AKM suite only. */
	for (i = 0; i < MAX_NUM_SUPPORTED_AKM_SUITES; i++) {
		prEntry = &prAdapter->rMib.dot11RSNAConfigAuthenticationSuitesTable[i];

		if (prEntry->dot11RSNAConfigAuthenticationSuite == u4AkmSuite)
			prEntry->dot11RSNAConfigAuthenticationSuiteEnabled = TRUE;
		else
			prEntry->dot11RSNAConfigAuthenticationSuiteEnabled = FALSE;
#if CFG_SUPPORT_802_11W
		if (kalGetMfpSetting(prAdapter->prGlueInfo) != RSN_AUTH_MFP_DISABLED) {
			if ((u4AkmSuite == RSN_AKM_SUITE_PSK) &&
			    prEntry->dot11RSNAConfigAuthenticationSuite == RSN_AKM_SUITE_PSK_SHA256) {
				DBGLOG(RSN, TRACE, "Enable RSN_AKM_SUITE_PSK_SHA256 AKM support\n");
				prEntry->dot11RSNAConfigAuthenticationSuiteEnabled = TRUE;

			}
			if ((u4AkmSuite == RSN_AKM_SUITE_802_1X) &&
			    prEntry->dot11RSNAConfigAuthenticationSuite == RSN_AKM_SUITE_802_1X_SHA256) {
				DBGLOG(RSN, TRACE, "Enable RSN_AKM_SUITE_802_1X_SHA256 AKM support\n");
				prEntry->dot11RSNAConfigAuthenticationSuiteEnabled = TRUE;
			}
		}
#endif
	}
#endif

	return WLAN_STATUS_SUCCESS;

}				/* wlanoidSetAuthMode */

#if 0
/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the current 802.11 privacy filter
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[out] pvQueryBuffer A pointer to the buffer that holds the result of
*                           the query.
* \param[in] u4QueryBufferLen The length of the query buffer.
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                            bytes written into the query buffer. If the call
*                            failed due to invalid length of the query buffer,
*                            returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_BUFFER_TOO_SHORT
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryPrivacyFilter(IN P_ADAPTER_T prAdapter,
			  OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	DEBUGFUNC("wlanoidQueryPrivacyFilter");

	ASSERT(prAdapter);

	ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	*pu4QueryInfoLen = sizeof(ENUM_PARAM_PRIVACY_FILTER_T);

	if (u4QueryBufferLen < sizeof(ENUM_PARAM_PRIVACY_FILTER_T))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	*(P_ENUM_PARAM_PRIVACY_FILTER_T) pvQueryBuffer = prAdapter->rWlanInfo.ePrivacyFilter;

#if DBG
	switch (*(P_ENUM_PARAM_PRIVACY_FILTER_T) pvQueryBuffer) {
	case PRIVACY_FILTER_ACCEPT_ALL:
		DBGLOG(REQ, INFO, "Current privacy mode: open mode\n");
		break;

	case PRIVACY_FILTER_8021xWEP:
		DBGLOG(REQ, INFO, "Current privacy mode: filtering mode\n");
		break;

	default:
		DBGLOG(REQ, INFO, "Current auth mode: %d\n", *(P_ENUM_PARAM_AUTH_MODE_T) pvQueryBuffer);
	}
#endif
	return WLAN_STATUS_SUCCESS;
}				/* wlanoidQueryPrivacyFilter */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set the IEEE 802.11 privacy filter
*        to the driver.
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
* \retval WLAN_STATUS_ADAPTER_NOT_READY
* \retval WLAN_STATUS_NOT_ACCEPTED
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetPrivacyFilter(IN P_ADAPTER_T prAdapter,
			IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_GLUE_INFO_T prGlueInfo;

	DEBUGFUNC("wlanoidSetPrivacyFilter");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	prGlueInfo = prAdapter->prGlueInfo;

	*pu4SetInfoLen = sizeof(ENUM_PARAM_PRIVACY_FILTER_T);

	if (u4SetBufferLen < sizeof(ENUM_PARAM_PRIVACY_FILTER_T))
		return WLAN_STATUS_INVALID_LENGTH;

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in set Authentication mode! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	/* Check if the new authentication mode is valid. */
	if (*(P_ENUM_PARAM_PRIVACY_FILTER_T) pvSetBuffer >= PRIVACY_FILTER_NUM) {
		DBGLOG(REQ, TRACE, "Invalid privacy filter %d\n", *(P_ENUM_PARAM_PRIVACY_FILTER_T) pvSetBuffer);
		return WLAN_STATUS_INVALID_DATA;
	}

	switch (*(P_ENUM_PARAM_PRIVACY_FILTER_T) pvSetBuffer) {
	default:
		break;
	}

	/* Save the new authentication mode. */
	prAdapter->rWlanInfo.ePrivacyFilter = *(ENUM_PARAM_PRIVACY_FILTER_T) pvSetBuffer;

	return WLAN_STATUS_SUCCESS;

}				/* wlanoidSetPrivacyFilter */
#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to reload the available default settings for
*        the specified type field.
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
* \retval WLAN_STATUS_INVALID_DATA
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetReloadDefaults(IN P_ADAPTER_T prAdapter,
			 IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	ENUM_PARAM_NETWORK_TYPE_T eNetworkType;
	UINT_32 u4Len;
	UINT_8 ucCmdSeqNum;

	DEBUGFUNC("wlanoidSetReloadDefaults");

	ASSERT(prAdapter);

	ASSERT(pu4SetInfoLen);
	*pu4SetInfoLen = sizeof(PARAM_RELOAD_DEFAULTS);

	/* if (IS_ARB_IN_RFTEST_STATE(prAdapter)) { */
	/* return WLAN_STATUS_SUCCESS; */
	/* } */

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in set Reload default! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	ASSERT(pvSetBuffer);
	/* Verify the available reload options and reload the settings. */
	switch (*(P_PARAM_RELOAD_DEFAULTS) pvSetBuffer) {
	case ENUM_RELOAD_WEP_KEYS:
		/* Reload available default WEP keys from the permanent
		 *  storage.
		 */
		prAdapter->rWifiVar.rConnSettings.eAuthMode = AUTH_MODE_OPEN;
		/* ENUM_ENCRYPTION_DISABLED; */
		prAdapter->rWifiVar.rConnSettings.eEncStatus = ENUM_ENCRYPTION1_KEY_ABSENT;
		{
			P_GLUE_INFO_T prGlueInfo;
			P_CMD_INFO_T prCmdInfo;
			P_WIFI_CMD_T prWifiCmd;
			P_CMD_802_11_KEY prCmdKey;
			UINT_8 aucBCAddr[] = BC_MAC_ADDR;

			prGlueInfo = prAdapter->prGlueInfo;
			prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, (CMD_HDR_SIZE + sizeof(CMD_802_11_KEY)));

			if (!prCmdInfo) {
				DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
				return WLAN_STATUS_FAILURE;
			}
			/* increase command sequence number */
			ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

			/* compose CMD_802_11_KEY cmd pkt */
			prCmdInfo->eCmdType = COMMAND_TYPE_NETWORK_IOCTL;
			prCmdInfo->u2InfoBufLen = CMD_HDR_SIZE + sizeof(CMD_802_11_KEY);
			prCmdInfo->pfCmdDoneHandler = nicCmdEventSetCommon;
			prCmdInfo->pfCmdTimeoutHandler = nicOidCmdTimeoutCommon;
			prCmdInfo->fgIsOid = TRUE;
			prCmdInfo->ucCID = CMD_ID_ADD_REMOVE_KEY;
			prCmdInfo->fgSetQuery = TRUE;
			prCmdInfo->fgNeedResp = FALSE;
			prCmdInfo->ucCmdSeqNum = ucCmdSeqNum;
			prCmdInfo->u4SetInfoLen = sizeof(PARAM_REMOVE_KEY_T);
			prCmdInfo->pvInformationBuffer = pvSetBuffer;
			prCmdInfo->u4InformationBufferLength = u4SetBufferLen;

			/* Setup WIFI_CMD_T */
			prWifiCmd = (P_WIFI_CMD_T) (prCmdInfo->pucInfoBuffer);
			prWifiCmd->u2TxByteCount = prCmdInfo->u2InfoBufLen;
			prWifiCmd->u2PQ_ID = CMD_PQ_ID;
			prWifiCmd->ucPktTypeID = CMD_PACKET_TYPE_ID;
			prWifiCmd->ucCID = prCmdInfo->ucCID;
			prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
			prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;

			prCmdKey = (P_CMD_802_11_KEY) (prWifiCmd->aucBuffer);

			kalMemZero((PUINT_8) prCmdKey, sizeof(CMD_802_11_KEY));

			prCmdKey->ucAddRemove = 0;	/* Remove */
			prCmdKey->ucKeyId = 0;	/* (UINT_8)(prRemovedKey->u4KeyIndex & 0x000000ff); */
			kalMemCopy(prCmdKey->aucPeerAddr, aucBCAddr, MAC_ADDR_LEN);

			ASSERT(prCmdKey->ucKeyId < MAX_KEY_NUM);

			prCmdKey->ucKeyType = 0;

			/* insert into prCmdQueue */
			kalEnqueueCommand(prGlueInfo, (P_QUE_ENTRY_T) prCmdInfo);

			/* wakeup txServiceThread later */
			GLUE_SET_EVENT(prGlueInfo);

			return WLAN_STATUS_PENDING;
		}

		break;

	default:
		DBGLOG(REQ, TRACE, "Invalid reload option %d\n", *(P_PARAM_RELOAD_DEFAULTS) pvSetBuffer);
		rStatus = WLAN_STATUS_INVALID_DATA;
	}

	/* OID_802_11_RELOAD_DEFAULTS requiest to reset to auto mode */
	eNetworkType = PARAM_NETWORK_TYPE_AUTOMODE;
	wlanoidSetNetworkTypeInUse(prAdapter, &eNetworkType, sizeof(eNetworkType), &u4Len);

	return rStatus;
}				/* wlanoidSetReloadDefaults */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set a WEP key to the driver.
*
* \param[in]  prAdapter Pointer to the Adapter structure.
* \param[in]  pvSetBuffer A pointer to the buffer that holds the data to be set.
* \param[in]  u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                          bytes read from the set buffer. If the call failed
*                          due to invalid length of the set buffer, returns
*                          the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
* \retval WLAN_STATUS_INVALID_DATA
* \retval WLAN_STATUS_ADAPTER_NOT_READY
* \retval WLAN_STATUS_BUFFER_TOO_SHORT
*/
/*----------------------------------------------------------------------------*/
#ifdef LINUX
UINT_8 keyBuffer[sizeof(PARAM_KEY_T) + 16 /* LEGACY_KEY_MAX_LEN */];
UINT_8 aucBCAddr[] = BC_MAC_ADDR;
#endif
WLAN_STATUS
wlanoidSetAddWep(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
#ifndef LINUX
	UINT_8 keyBuffer[sizeof(PARAM_KEY_T) + 16 /* LEGACY_KEY_MAX_LEN */];
	UINT_8 aucBCAddr[] = BC_MAC_ADDR;
#endif
	P_PARAM_WEP_T prNewWepKey;
	P_PARAM_KEY_T prParamKey = (P_PARAM_KEY_T) keyBuffer;
	UINT_32 u4KeyId, u4SetLen;

	DEBUGFUNC("wlanoidSetAddWep");

	ASSERT(prAdapter);

	*pu4SetInfoLen = OFFSET_OF(PARAM_WEP_T, aucKeyMaterial);

	if (u4SetBufferLen < OFFSET_OF(PARAM_WEP_T, aucKeyMaterial)) {
		ASSERT(pu4SetInfoLen);
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}

	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN, "Fail in set add WEP! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	prNewWepKey = (P_PARAM_WEP_T) pvSetBuffer;

	/* Verify the total buffer for minimum length. */
	if (u4SetBufferLen < OFFSET_OF(PARAM_WEP_T, aucKeyMaterial) + prNewWepKey->u4KeyLength) {
		DBGLOG(REQ, WARN, "Invalid total buffer length (%d) than minimum length (%d)\n",
		       (UINT_8) u4SetBufferLen, (UINT_8) OFFSET_OF(PARAM_WEP_T, aucKeyMaterial));

		*pu4SetInfoLen = OFFSET_OF(PARAM_WEP_T, aucKeyMaterial);
		return WLAN_STATUS_INVALID_DATA;
	}

	/* Verify the key structure length. */
	if (prNewWepKey->u4Length > u4SetBufferLen) {
		DBGLOG(REQ, WARN,
		       "Invalid key structure length (%d) greater than total buffer length (%d)\n",
		       (UINT_8) prNewWepKey->u4Length, (UINT_8) u4SetBufferLen);

		*pu4SetInfoLen = u4SetBufferLen;
		return WLAN_STATUS_INVALID_DATA;
	}

	/* Verify the key material length for maximum key material length:16 */
	if (prNewWepKey->u4KeyLength > 16 /* LEGACY_KEY_MAX_LEN */) {
		DBGLOG(REQ, WARN,
		       "Invalid key material length (%d) greater than maximum key material length (16)\n",
		       (UINT_8) prNewWepKey->u4KeyLength);

		*pu4SetInfoLen = u4SetBufferLen;
		return WLAN_STATUS_INVALID_DATA;
	}

	*pu4SetInfoLen = u4SetBufferLen;

	u4KeyId = prNewWepKey->u4KeyIndex & BITS(0, 29) /* WEP_KEY_ID_FIELD */;

	/* Verify whether key index is valid or not, current version
	 *  driver support only 4 global WEP keys setting by this OID
	 */
	if (u4KeyId > MAX_KEY_NUM - 1) {
		DBGLOG(REQ, ERROR, "Error, invalid WEP key ID: %d\n", (UINT_8) u4KeyId);
		return WLAN_STATUS_INVALID_DATA;
	}

	prParamKey->u4KeyIndex = u4KeyId;

	/* Transmit key */
	if (prNewWepKey->u4KeyIndex & IS_TRANSMIT_KEY)
		prParamKey->u4KeyIndex |= IS_TRANSMIT_KEY;

	/* Per client key */
	if (prNewWepKey->u4KeyIndex & IS_UNICAST_KEY)
		prParamKey->u4KeyIndex |= IS_UNICAST_KEY;

	prParamKey->u4KeyLength = prNewWepKey->u4KeyLength;

	kalMemCopy(prParamKey->arBSSID, aucBCAddr, MAC_ADDR_LEN);

	kalMemCopy(prParamKey->aucKeyMaterial, prNewWepKey->aucKeyMaterial, prNewWepKey->u4KeyLength);

	prParamKey->ucBssIdx = prAdapter->prAisBssInfo->ucBssIndex;

	if (prParamKey->u4KeyLength == WEP_40_LEN)
		prParamKey->ucCipher = CIPHER_SUITE_WEP40;
	else if (prParamKey->u4KeyLength == WEP_104_LEN)
		prParamKey->ucCipher = CIPHER_SUITE_WEP104;
	else if (prParamKey->u4KeyLength == WEP_128_LEN)
		prParamKey->ucCipher = CIPHER_SUITE_WEP128;

	prParamKey->u4Length = OFFSET_OF(PARAM_KEY_T, aucKeyMaterial) + prNewWepKey->u4KeyLength;

	wlanoidSetAddKey(prAdapter, (PVOID) prParamKey, prParamKey->u4Length, &u4SetLen);

	return WLAN_STATUS_PENDING;
}				/* wlanoidSetAddWep */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to request the driver to remove the WEP key
*          at the specified key index.
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
* \retval WLAN_STATUS_ADAPTER_NOT_READY
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetRemoveWep(IN P_ADAPTER_T prAdapter,
		    IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	UINT_32 u4KeyId, u4SetLen;
	PARAM_REMOVE_KEY_T rRemoveKey;
	UINT_8 aucBCAddr[] = BC_MAC_ADDR;

	DEBUGFUNC("wlanoidSetRemoveWep");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(PARAM_KEY_INDEX);

	if (u4SetBufferLen < sizeof(PARAM_KEY_INDEX))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);
	u4KeyId = *(PUINT_32) pvSetBuffer;

	/* Dump PARAM_WEP content. */
	DBGLOG(REQ, INFO, "Set: Dump PARAM_KEY_INDEX content\n");
	DBGLOG(REQ, INFO, "Index : %u\n", u4KeyId);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in set remove WEP! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	if (u4KeyId & IS_TRANSMIT_KEY) {
		/* Bit 31 should not be set */
		DBGLOG(REQ, ERROR, "Invalid WEP key index: %u\n", u4KeyId);
		return WLAN_STATUS_INVALID_DATA;
	}

	u4KeyId &= BITS(0, 7);

	/* Verify whether key index is valid or not. Current version
	 *  driver support only 4 global WEP keys.
	 */
	if (u4KeyId > MAX_KEY_NUM - 1) {
		DBGLOG(REQ, ERROR, "invalid WEP key ID %u\n", u4KeyId);
		return WLAN_STATUS_INVALID_DATA;
	}

	rRemoveKey.u4Length = sizeof(PARAM_REMOVE_KEY_T);
	rRemoveKey.u4KeyIndex = *(PUINT_32) pvSetBuffer;

	kalMemCopy(rRemoveKey.arBSSID, aucBCAddr, MAC_ADDR_LEN);

	wlanoidSetRemoveKey(prAdapter, (PVOID)&rRemoveKey, sizeof(PARAM_REMOVE_KEY_T), &u4SetLen);

	return WLAN_STATUS_PENDING;
}				/* wlanoidSetRemoveWep */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set a key to the driver.
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
*
* \note The setting buffer PARAM_KEY_T, which is set by NDIS, is unpacked.
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetAddKey(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_GLUE_INFO_T prGlueInfo;
	P_CMD_INFO_T prCmdInfo;
	P_WIFI_CMD_T prWifiCmd;
	P_PARAM_KEY_T prNewKey;
	P_CMD_802_11_KEY prCmdKey;
	UINT_8 ucCmdSeqNum;
	P_BSS_INFO_T prBssInfo;
	P_AIS_SPECIFIC_BSS_INFO_T prAisSpecBssInfo;
	P_STA_RECORD_T prStaRec = NULL;
	BOOL fgNoHandshakeSec = FALSE;
#if CFG_SUPPORT_TDLS
	P_STA_RECORD_T prTmpStaRec;
#endif

	DEBUGFUNC("wlanoidSetAddKey");
	DBGLOG(REQ, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	DBGLOG(RSN, INFO, "wlanoidSetAddKey\n");

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(RSN, WARN, "Fail in set add key! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	prNewKey = (P_PARAM_KEY_T) pvSetBuffer;

	/* Verify the key structure length. */
	if (prNewKey->u4Length > u4SetBufferLen) {
		DBGLOG(RSN, WARN,
		       "Invalid key structure length (%d) greater than total buffer length (%d)\n",
		       (UINT_8) prNewKey->u4Length, (UINT_8) u4SetBufferLen);

		*pu4SetInfoLen = u4SetBufferLen;
		return WLAN_STATUS_INVALID_LENGTH;
	}

	/* Verify the key material length for key material buffer */
	if (prNewKey->u4KeyLength > prNewKey->u4Length - OFFSET_OF(PARAM_KEY_T, aucKeyMaterial)) {
		DBGLOG(RSN, WARN, "Invalid key material length (%d)\n", (UINT_8) prNewKey->u4KeyLength);
		*pu4SetInfoLen = u4SetBufferLen;
		return WLAN_STATUS_INVALID_DATA;
	}

	/* Exception check */
	if (prNewKey->u4KeyIndex & 0x0fffff00)
		return WLAN_STATUS_INVALID_DATA;

	/* Exception check, pairwise key must with transmit bit enabled */
	if ((prNewKey->u4KeyIndex & BITS(30, 31)) == IS_UNICAST_KEY)
		return WLAN_STATUS_INVALID_DATA;

	if (!(prNewKey->u4KeyLength == WEP_40_LEN || prNewKey->u4KeyLength == WEP_104_LEN ||
	      prNewKey->u4KeyLength == CCMP_KEY_LEN || prNewKey->u4KeyLength == TKIP_KEY_LEN)) {
		return WLAN_STATUS_INVALID_DATA;
	}

	/* Exception check, pairwise key must with transmit bit enabled */
	if ((prNewKey->u4KeyIndex & BITS(30, 31)) == BITS(30, 31)) {
		if (/* ((prNewKey->u4KeyIndex & 0xff) != 0) || */
		    ((prNewKey->arBSSID[0] == 0xff) && (prNewKey->arBSSID[1] == 0xff)
		     && (prNewKey->arBSSID[2] == 0xff) && (prNewKey->arBSSID[3] == 0xff)
		     && (prNewKey->arBSSID[4] == 0xff) && (prNewKey->arBSSID[5] == 0xff))) {
			return WLAN_STATUS_INVALID_DATA;
		}
	}

	*pu4SetInfoLen = u4SetBufferLen;

	/* Dump PARAM_KEY content. */
	DBGLOG(RSN, INFO, "Set: Dump PARAM_KEY content\n");
	DBGLOG(RSN, INFO, "Length    : 0x%08x\n", prNewKey->u4Length);
	DBGLOG(RSN, INFO, "Key Index : 0x%08x\n", prNewKey->u4KeyIndex);
	DBGLOG(RSN, INFO, "Key Length: 0x%08x\n", prNewKey->u4KeyLength);
	DBGLOG(RSN, INFO, "BSSID:\n");
	DBGLOG(RSN, INFO, MACSTR "\n", MAC2STR(prNewKey->arBSSID));
	DBGLOG(RSN, INFO, "Cipher    : %d\n", prNewKey->ucCipher);
	DBGLOG(RSN, TRACE, "Key RSC:\n");
	DBGLOG_MEM8(RSN, TRACE, &prNewKey->rKeyRSC, sizeof(PARAM_KEY_RSC));
	DBGLOG(RSN, INFO, "Key Material:\n");
	DBGLOG_MEM8(RSN, INFO, prNewKey->aucKeyMaterial, prNewKey->u4KeyLength);

	prGlueInfo = prAdapter->prGlueInfo;
	prAisSpecBssInfo = &prAdapter->rWifiVar.rAisSpecificBssInfo;
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prNewKey->ucBssIdx);

	if (!prBssInfo) {
		DBGLOG(REQ, INFO, "BSS Info not exist !!\n");
		return WLAN_STATUS_SUCCESS;
	}

	/*        Tx    Rx    KeyType addr
	 *  STA, GC:
	 *  case1:  1      1      0           BC addr (no sta record of AP at this moment)  WEP,
	 *  notice: tx at default key setting WEP key now save to BSS_INFO
	 *  case2:  0      1      0            BSSID (sta record of AP)                 RSN BC key
	 *  case3:  1      1      1            AP addr (sta record of AP)               RSN STA key
	 *
	 *  GO:
	 *  case1:  1          1      0                BSSID (no sta record)            WEP -- Not support
	 *  case2:  1          0      0                BSSID (no sta record)            RSN BC key
	 *  case3:  1          1      1                STA addr                         STA key
	 */

	if (prNewKey->ucCipher == CIPHER_SUITE_WEP40 ||
	    prNewKey->ucCipher == CIPHER_SUITE_WEP104 || prNewKey->ucCipher == CIPHER_SUITE_WEP128) {
		/* check if the key no need handshake, then save to bss wep key for global usage */
		fgNoHandshakeSec = TRUE;
	}

	if (fgNoHandshakeSec) {
#if DBG
		if (IS_BSS_AIS(prBssInfo)) {
			if (prAdapter->rWifiVar.rConnSettings.eAuthMode >= AUTH_MODE_WPA
				&& prAdapter->rWifiVar.rConnSettings.eAuthMode != AUTH_MODE_WPA_NONE) {
				DBGLOG(RSN, WARN, "Set wep at not open/shared setting\n");
				return WLAN_STATUS_SUCCESS;
			}
		}
#endif
	}

	if ((prNewKey->u4KeyIndex & IS_UNICAST_KEY) == IS_UNICAST_KEY) {
		prStaRec = cnmGetStaRecByAddress(prAdapter, prBssInfo->ucBssIndex, prNewKey->arBSSID);
		if (!prStaRec) {	/* Already disconnected ? */
			DBGLOG(REQ, INFO, "[wlan] Not set the peer key while disconnect\n");
			return WLAN_STATUS_SUCCESS;
		}
	}

	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, (CMD_HDR_SIZE + sizeof(CMD_802_11_KEY)));

	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}
	/* increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);
	DBGLOG(REQ, INFO, "ucCmdSeqNum = %d\n", ucCmdSeqNum);

	/* compose CMD_802_11_KEY cmd pkt */
	prCmdInfo->eCmdType = COMMAND_TYPE_NETWORK_IOCTL;
	prCmdInfo->u2InfoBufLen = CMD_HDR_SIZE + sizeof(CMD_802_11_KEY);
#if CFG_SUPPORT_REPLAY_DETECTION
	prCmdInfo->pfCmdDoneHandler = nicCmdEventSetAddKey;
	prCmdInfo->pfCmdTimeoutHandler = nicOidCmdTimeoutSetAddKey;
#else
	prCmdInfo->pfCmdDoneHandler = nicCmdEventSetCommon;
	prCmdInfo->pfCmdTimeoutHandler = nicOidCmdTimeoutCommon;
#endif
	prCmdInfo->fgIsOid = g_fgIsOid;
	prCmdInfo->ucCID = CMD_ID_ADD_REMOVE_KEY;
	prCmdInfo->fgSetQuery = TRUE;
	prCmdInfo->fgNeedResp = FALSE;
	prCmdInfo->ucCmdSeqNum = ucCmdSeqNum;
	prCmdInfo->u4SetInfoLen = u4SetBufferLen;
	prCmdInfo->pvInformationBuffer = pvSetBuffer;
	prCmdInfo->u4InformationBufferLength = u4SetBufferLen;

	/* Setup WIFI_CMD_T */
	prWifiCmd = (P_WIFI_CMD_T) (prCmdInfo->pucInfoBuffer);
	prWifiCmd->u2TxByteCount = prCmdInfo->u2InfoBufLen;
	prWifiCmd->u2PQ_ID = CMD_PQ_ID;
	prWifiCmd->ucPktTypeID = CMD_PACKET_TYPE_ID;
	prWifiCmd->ucCID = prCmdInfo->ucCID;
	prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
	prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;

	prCmdKey = (P_CMD_802_11_KEY) (prWifiCmd->aucBuffer);

	kalMemZero(prCmdKey, sizeof(CMD_802_11_KEY));

	prCmdKey->ucAddRemove = 1;	/* Add */

	prCmdKey->ucTxKey = ((prNewKey->u4KeyIndex & IS_TRANSMIT_KEY) == IS_TRANSMIT_KEY) ? 1 : 0;
	prCmdKey->ucKeyType = ((prNewKey->u4KeyIndex & IS_UNICAST_KEY) == IS_UNICAST_KEY) ? 1 : 0;
	prCmdKey->ucIsAuthenticator = ((prNewKey->u4KeyIndex & IS_AUTHENTICATOR) == IS_AUTHENTICATOR) ? 1 : 0;

	/* Copy the addr of the key */
	if ((prNewKey->u4KeyIndex & IS_UNICAST_KEY) == IS_UNICAST_KEY) {
		if (prStaRec) {
			/* Overwrite the fgNoHandshakeSec in case */
			fgNoHandshakeSec = FALSE;	/* Legacy 802.1x wep case ? */
			/* ASSERT(FALSE); */
		}
	} else {
		if (!IS_BSS_ACTIVE(prBssInfo))
			DBGLOG(REQ, INFO, "[wlan] BSS info (%d) not active yet!", prNewKey->ucBssIdx);

	}

	prCmdKey->ucBssIdx = prBssInfo->ucBssIndex;
	prCmdKey->ucKeyId = (UINT_8) (prNewKey->u4KeyIndex & 0xff);

	/* Note: the key length may not correct for WPA-None */
	prCmdKey->ucKeyLen = (UINT_8) prNewKey->u4KeyLength;

	kalMemCopy(prCmdKey->aucKeyMaterial, (PUINT_8) prNewKey->aucKeyMaterial, prCmdKey->ucKeyLen);

	if (prNewKey->ucCipher) {
		prCmdKey->ucAlgorithmId = prNewKey->ucCipher;
		if (IS_BSS_AIS(prBssInfo)) {
#if CFG_SUPPORT_802_11W
			if (prCmdKey->ucAlgorithmId == CIPHER_SUITE_BIP) {
				if (prCmdKey->ucKeyId >= 4) {
					P_AIS_SPECIFIC_BSS_INFO_T prAisSpecBssInfo;

					prAisSpecBssInfo = &prAdapter->rWifiVar.rAisSpecificBssInfo;
					prAisSpecBssInfo->fgBipKeyInstalled = TRUE;
				}
			}
#endif
			if ((prCmdKey->ucAlgorithmId == CIPHER_SUITE_CCMP) && rsnCheckPmkidCandicate(prAdapter)) {
				DBGLOG(RSN, TRACE, "Add key: Prepare a timer to indicate candidate PMKID Candidate\n");
				cnmTimerStopTimer(prAdapter, &prAisSpecBssInfo->rPreauthenticationTimer);
				cnmTimerStartTimer(prAdapter,
						   &prAisSpecBssInfo->rPreauthenticationTimer,
						   SEC_TO_MSEC(WAIT_TIME_IND_PMKID_CANDICATE_SEC));
			}
			if (prCmdKey->ucAlgorithmId == CIPHER_SUITE_TKIP) {
				/* Todo:: Support AP mode defragment */
				/* for pairwise key only */
				if ((prNewKey->u4KeyIndex & BITS(30, 31)) == ((IS_UNICAST_KEY) | (IS_TRANSMIT_KEY))) {
					kalMemCopy(prAdapter->rWifiVar.rAisSpecificBssInfo.aucRxMicKey,
						   &prCmdKey->aucKeyMaterial[16], MIC_KEY_LEN);
					kalMemCopy(prAdapter->rWifiVar.rAisSpecificBssInfo.aucTxMicKey,
						   &prCmdKey->aucKeyMaterial[24], MIC_KEY_LEN);
				}
			}
		} else {
#if CFG_SUPPORT_802_11W
			/* AP PMF */
			if ((prCmdKey->ucKeyId >= 4 && prCmdKey->ucKeyId <= 5) &&
				(prCmdKey->ucAlgorithmId == CIPHER_SUITE_BIP)) {
				DBGLOG(RSN, INFO, "AP mode set BIP\n");
				prBssInfo->rApPmfCfg.fgBipKeyInstalled = TRUE;
			}
#endif
		}
	} else {		/* Legacy windows NDIS no cipher info */
#if 0
		if (prNewKey->u4KeyLength == 5) {
			prCmdKey->ucAlgorithmId = CIPHER_SUITE_WEP40;
		} else if (prNewKey->u4KeyLength == 13) {
			prCmdKey->ucAlgorithmId = CIPHER_SUITE_WEP104;
		} else if (prNewKey->u4KeyLength == 16) {
			if (prAdapter->rWifiVar.rConnSettings.eAuthMode < AUTH_MODE_WPA)
				prCmdKey->ucAlgorithmId = CIPHER_SUITE_WEP128;
			else {
				if (IS_BSS_AIS(prBssInfo)) {
#if CFG_SUPPORT_802_11W
					if (prCmdKey->ucKeyId >= 4) {
						P_AIS_SPECIFIC_BSS_INFO_T prAisSpecBssInfo;

						prCmdKey->ucAlgorithmId = CIPHER_SUITE_BIP;

						prAisSpecBssInfo = &prAdapter->rWifiVar.rAisSpecificBssInfo;
						prAisSpecBssInfo->fgBipKeyInstalled = TRUE;
					} else
#endif
					{
					prCmdKey->ucAlgorithmId = CIPHER_SUITE_CCMP;
					if (rsnCheckPmkidCandicate(prAdapter)) {

						DBGLOG(RSN, TRACE,
							("Add key: Prepare a timer to indicate candidate PMKID\n"));
						cnmTimerStopTimer(prAdapter,
								&prAisSpecBssInfo->rPreauthenticationTimer);
						cnmTimerStartTimer(prAdapter,
								&prAisSpecBssInfo->rPreauthenticationTimer,
								SEC_TO_MSEC
								(WAIT_TIME_IND_PMKID_CANDICATE_SEC));
					}
					}
				}
			}
		} else if (prNewKey->u4KeyLength == 32) {
			if (IS_BSS_AIS(prBssInfo)) {
				if (prAdapter->rWifiVar.rConnSettings.eAuthMode == AUTH_MODE_WPA_NONE) {
					if (prAdapter->rWifiVar.rConnSettings.eEncStatus == ENUM_ENCRYPTION2_ENABLED) {
						prCmdKey->ucAlgorithmId = CIPHER_SUITE_TKIP;
					} else if (prAdapter->rWifiVar.rConnSettings.eEncStatus ==
						   ENUM_ENCRYPTION3_ENABLED) {
						prCmdKey->ucAlgorithmId = CIPHER_SUITE_CCMP;
						prCmdKey->ucKeyLen = CCMP_KEY_LEN;
					}
				} else {
					prCmdKey->ucAlgorithmId = CIPHER_SUITE_TKIP;
					kalMemCopy(prAdapter->rWifiVar.rAisSpecificBssInfo.aucRxMicKey,
						   &prCmdKey->aucKeyMaterial[16], MIC_KEY_LEN);
					kalMemCopy(prAdapter->rWifiVar.rAisSpecificBssInfo.aucTxMicKey,
						   &prCmdKey->aucKeyMaterial[24], MIC_KEY_LEN);
			if (0 /* Todo::GCMP & GCMP-BIP ? */) {
				if (rsnCheckPmkidCandicate(prAdapter)) {

					DBGLOG(RSN, TRACE,
						("Add key: Prepare a timer to indicate candidate PMKID\n"));
					cnmTimerStopTimer(prAdapter,
							&prAisSpecBssInfo->rPreauthenticationTimer);
					cnmTimerStartTimer(prAdapter,
							&prAisSpecBssInfo->rPreauthenticationTimer,
							SEC_TO_MSEC
							(WAIT_TIME_IND_PMKID_CANDICATE_SEC));
				}
			} else {
				prCmdKey->ucAlgorithmId = CIPHER_SUITE_TKIP;
			}
		}
}
#endif
	}

	{
#if CFG_SUPPORT_TDLS
		prTmpStaRec = cnmGetStaRecByAddress(prAdapter, prBssInfo->ucBssIndex, prNewKey->arBSSID);
		if (prTmpStaRec) {
			if (IS_DLS_STA(prTmpStaRec)) {
				prStaRec = prTmpStaRec;
				prCmdKey->ucAlgorithmId = CIPHER_SUITE_CCMP;	/*128 ,TODO  GCMP 256 */
				kalMemCopy(prCmdKey->aucPeerAddr, prStaRec->aucMacAddr, MAC_ADDR_LEN);
			}
		}
#endif

#if CFG_SUPPORT_802_11W
		/* AP PMF */
		if (prCmdKey->ucAlgorithmId == CIPHER_SUITE_BIP) {
			if (prCmdKey->ucIsAuthenticator) {
				DBGLOG(RSN, INFO, "Authenticator BIP bssid:%d\n", prBssInfo->ucBssIndex);

				prCmdKey->ucWlanIndex =
					secPrivacySeekForBcEntry(prAdapter,
						prBssInfo->ucBssIndex,
						prBssInfo->aucOwnMacAddr,
						STA_REC_INDEX_NOT_FOUND,
						prCmdKey->ucAlgorithmId, prCmdKey->ucKeyId);
			} else {
				prCmdKey->ucWlanIndex =
				    secPrivacySeekForBcEntry(prAdapter,
					     prBssInfo->ucBssIndex,
					     prBssInfo->prStaRecOfAP->aucMacAddr,
					     prBssInfo->prStaRecOfAP->ucIndex,
					     prCmdKey->ucAlgorithmId, prCmdKey->ucKeyId);
			}

			DBGLOG(RSN, INFO, "BIP BC wtbl index:%d\n", prCmdKey->ucWlanIndex);
		} else
#endif
		if (1) {
			if (prStaRec) {
				if (prCmdKey->ucKeyType) {	/* RSN STA */
					P_WLAN_TABLE_T prWtbl;

					prWtbl = prAdapter->rWifiVar.arWtbl;
					prWtbl[prStaRec->ucWlanIndex].ucKeyId = prCmdKey->ucKeyId;

					prCmdKey->ucWlanIndex = prStaRec->ucWlanIndex;
					prStaRec->fgTransmitKeyExist = TRUE;	/* wait for CMD Done ? */
					kalMemCopy(prCmdKey->aucPeerAddr, prNewKey->arBSSID, MAC_ADDR_LEN);
#if CFG_SUPPORT_802_11W
					/* AP PMF */
					DBGLOG(RSN, INFO, "Assign client PMF flag = %d\n",
						prStaRec->rPmfCfg.fgApplyPmf);
					prCmdKey->ucMgmtProtection = prStaRec->rPmfCfg.fgApplyPmf;
#endif
				} else {
					ASSERT(FALSE);
				}
			} else {	/* Overwrite the old one for AP and STA WEP */
				if (prBssInfo->prStaRecOfAP) {
					DBGLOG(RSN, INFO, "AP REC\n");
					prCmdKey->ucWlanIndex =
					    secPrivacySeekForBcEntry(prAdapter,
								     prBssInfo->ucBssIndex,
								     prBssInfo->prStaRecOfAP->aucMacAddr,
								     prBssInfo->prStaRecOfAP->ucIndex,
								     prCmdKey->ucAlgorithmId, prCmdKey->ucKeyId);

					kalMemCopy(prCmdKey->aucPeerAddr,
						   prBssInfo->prStaRecOfAP->aucMacAddr, MAC_ADDR_LEN);
				} else {
					DBGLOG(RSN, INFO, "!AP && !STA REC\n");
					prCmdKey->ucWlanIndex =
						secPrivacySeekForBcEntry(prAdapter,
						prBssInfo->ucBssIndex,
						prBssInfo->aucOwnMacAddr,	/* Should replace by BSSID later */
						STA_REC_INDEX_NOT_FOUND,
						prCmdKey->ucAlgorithmId,
						prCmdKey->ucKeyId);
					kalMemCopy(prCmdKey->aucPeerAddr, prBssInfo->aucOwnMacAddr, MAC_ADDR_LEN);
				}

				/* overflow check */
				if (prCmdKey->ucKeyId >= MAX_KEY_NUM) {
					DBGLOG(RSN, ERROR, "invalid keyId: %d\n", prCmdKey->ucKeyId);
					prCmdKey->ucKeyId &= 0x3;
				}

				if (fgNoHandshakeSec) {	/* WEP: STA and AP */
					prBssInfo->wepkeyWlanIdx = prCmdKey->ucWlanIndex;
					prBssInfo->wepkeyUsed[prCmdKey->ucKeyId] = TRUE;
				} else if (!prBssInfo->prStaRecOfAP) {	/* AP WPA/RSN */
					prBssInfo->ucBMCWlanIndexS[prCmdKey->ucKeyId] = prCmdKey->ucWlanIndex;
					prBssInfo->ucBMCWlanIndexSUsed[prCmdKey->ucKeyId] = TRUE;
				} else {	/* STA WPA/RSN, should not have tx but no sta record */
					prBssInfo->ucBMCWlanIndexS[prCmdKey->ucKeyId] = prCmdKey->ucWlanIndex;
					prBssInfo->ucBMCWlanIndexSUsed[prCmdKey->ucKeyId] = TRUE;
					DBGLOG(RSN, INFO, "BMCWlanIndex kid = %d, index = %d\n", prCmdKey->ucKeyId,
					       prCmdKey->ucWlanIndex);
				}

				if (prCmdKey->ucTxKey) {	/* */
					prBssInfo->fgBcDefaultKeyExist = TRUE;
					prBssInfo->ucBcDefaultKeyIdx = prCmdKey->ucKeyId;
				}
			}
		}
	}

#if DBG
	DBGLOG(RSN, INFO, "Add key cmd to wlan index %d:", prCmdKey->ucWlanIndex);
	DBGLOG(RSN, INFO, "(BSS = %d) " MACSTR "\n", prCmdKey->ucBssIdx, MAC2STR(prCmdKey->aucPeerAddr));
	DBGLOG(RSN, INFO, "Tx = %d type = %d Auth = %d\n", prCmdKey->ucTxKey, prCmdKey->ucKeyType,
	       prCmdKey->ucIsAuthenticator);
	DBGLOG(RSN, INFO, "cipher = %d keyid = %d keylen = %d\n", prCmdKey->ucAlgorithmId, prCmdKey->ucKeyId,
	       prCmdKey->ucKeyLen);
	DBGLOG_MEM8(RSN, INFO, prCmdKey->aucKeyMaterial, prCmdKey->ucKeyLen);

	DBGLOG(RSN, INFO, "wepkeyUsed = %d\n", prBssInfo->wepkeyUsed[prCmdKey->ucKeyId]);
	DBGLOG(RSN, INFO, "wepkeyWlanIdx = %d:", prBssInfo->wepkeyWlanIdx);
	DBGLOG(RSN, INFO, "ucBMCWlanIndexSUsed = %d\n", prBssInfo->ucBMCWlanIndexSUsed[prCmdKey->ucKeyId]);
	DBGLOG(RSN, INFO, "ucBMCWlanIndexS = %d:", prBssInfo->ucBMCWlanIndexS[prCmdKey->ucKeyId]);
#endif

	/* insert into prCmdQueue */
	kalEnqueueCommand(prGlueInfo, (P_QUE_ENTRY_T) prCmdInfo);

	/* wakeup txServiceThread later */
	GLUE_SET_EVENT(prGlueInfo);

	return WLAN_STATUS_PENDING;
}				/* wlanoidSetAddKey */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to request the driver to remove the key at
*        the specified key index.
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
* \retval WLAN_STATUS_ADAPTER_NOT_READY
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetRemoveKey(IN P_ADAPTER_T prAdapter,
		    IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_GLUE_INFO_T prGlueInfo;
	P_CMD_INFO_T prCmdInfo;
	P_WIFI_CMD_T prWifiCmd;
	P_PARAM_REMOVE_KEY_T prRemovedKey;
	P_CMD_802_11_KEY prCmdKey;
	UINT_8 ucCmdSeqNum;
	P_WLAN_TABLE_T prWlanTable;
	P_STA_RECORD_T prStaRec = NULL;
	P_BSS_INFO_T prBssInfo;
	/*	UINT_8 i = 0;	*/
	BOOL fgRemoveWepKey = FALSE;
	BOOL fgRemoveBCKey = FALSE;
	UINT_32 ucRemoveBCKeyAtIdx = WTBL_RESERVED_ENTRY;
	UINT_32 u4KeyIndex;

	DEBUGFUNC("wlanoidSetRemoveKey");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	DBGLOG(RSN, INFO, "wlanoidSetRemoveKey\n");

	prWlanTable = prAdapter->rWifiVar.arWtbl;

	*pu4SetInfoLen = sizeof(PARAM_REMOVE_KEY_T);

	if (u4SetBufferLen < sizeof(PARAM_REMOVE_KEY_T))
		return WLAN_STATUS_INVALID_LENGTH;

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in set remove key! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	ASSERT(pvSetBuffer);
	prRemovedKey = (P_PARAM_REMOVE_KEY_T) pvSetBuffer;

	/* Dump PARAM_REMOVE_KEY content. */
	DBGLOG(RSN, INFO, "Set: Dump PARAM_REMOVE_KEY content\n");
	DBGLOG(RSN, INFO, "Length    : 0x%08x\n", prRemovedKey->u4Length);
	DBGLOG(RSN, INFO, "Key Index : 0x%08x\n", prRemovedKey->u4KeyIndex);
	DBGLOG(RSN, INFO, "BSS_INDEX : %d\n", prRemovedKey->ucBssIdx);
	DBGLOG(RSN, INFO, "BSSID:\n");
	DBGLOG_MEM8(RSN, INFO, prRemovedKey->arBSSID, MAC_ADDR_LEN);

	prGlueInfo = prAdapter->prGlueInfo;
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prRemovedKey->ucBssIdx);
	ASSERT(prBssInfo);

	u4KeyIndex = prRemovedKey->u4KeyIndex & 0x000000FF;
#if CFG_SUPPORT_802_11W
	ASSERT(u4KeyIndex < MAX_KEY_NUM + 2);
#else
	/* ASSERT(prCmdKey->ucKeyId < MAX_KEY_NUM); */
#endif

	if (u4KeyIndex >= 4) {
		DBGLOG(RSN, INFO, "Remove bip key Index : 0x%08x\n",
				u4KeyIndex);
		return WLAN_STATUS_SUCCESS;
	}

	/* Clean up the Tx key flag */
	if (prRemovedKey->u4KeyIndex & IS_UNICAST_KEY) {
		prStaRec = cnmGetStaRecByAddress(prAdapter, prRemovedKey->ucBssIdx, prRemovedKey->arBSSID);
		if (!prStaRec)
			return WLAN_STATUS_SUCCESS;
	} else {
		if (u4KeyIndex == prBssInfo->ucBcDefaultKeyIdx)
			prBssInfo->fgBcDefaultKeyExist = FALSE;
	}

	if (!prStaRec) {
		if (prBssInfo->wepkeyUsed[u4KeyIndex] == TRUE)
			fgRemoveWepKey = TRUE;

		if (fgRemoveWepKey) {
			DBGLOG(RSN, INFO, "Remove wep key id = %d", u4KeyIndex);
			prBssInfo->wepkeyUsed[u4KeyIndex] = FALSE;
			if (prBssInfo->fgBcDefaultKeyExist &&
			    prBssInfo->ucBcDefaultKeyIdx == u4KeyIndex) {
				prBssInfo->fgBcDefaultKeyExist = FALSE;
				prBssInfo->ucBcDefaultKeyIdx = 0xff;
			}
			ASSERT(prBssInfo->wepkeyWlanIdx < WTBL_SIZE);
			ucRemoveBCKeyAtIdx = prBssInfo->wepkeyWlanIdx;
			fgRemoveBCKey = TRUE;
		} else {
			DBGLOG(RSN, INFO, "Remove group key id = %d", u4KeyIndex);

			if (prBssInfo->ucBMCWlanIndexSUsed[u4KeyIndex]) {

				if (prBssInfo->fgBcDefaultKeyExist &&
				    prBssInfo->ucBcDefaultKeyIdx == u4KeyIndex) {
					prBssInfo->fgBcDefaultKeyExist = FALSE;
					prBssInfo->ucBcDefaultKeyIdx = 0xff;
				}
				if (u4KeyIndex != 0)
					ASSERT(prBssInfo->ucBMCWlanIndexS[u4KeyIndex] < WTBL_SIZE);
				ucRemoveBCKeyAtIdx = prBssInfo->ucBMCWlanIndexS[u4KeyIndex];

				prBssInfo->ucBMCWlanIndexSUsed[u4KeyIndex]
					= FALSE;
				prBssInfo->ucBMCWlanIndexS[u4KeyIndex]
					= WTBL_RESERVED_ENTRY;
				fgRemoveBCKey = TRUE;
			}
		}
		/* Change the wtbl to not used state */
		if (fgRemoveBCKey)
			prWlanTable[ucRemoveBCKeyAtIdx].ucUsed = FALSE;

		DBGLOG(RSN, INFO, "ucRemoveBCKeyAtIdx = %d", ucRemoveBCKeyAtIdx);

		if (ucRemoveBCKeyAtIdx >= WTBL_SIZE)
			return WLAN_STATUS_SUCCESS;
	}

	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, (CMD_HDR_SIZE + sizeof(CMD_802_11_KEY)));

	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}


	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prRemovedKey->ucBssIdx);

	/* increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

	/* compose CMD_802_11_KEY cmd pkt */
	prCmdInfo->eCmdType = COMMAND_TYPE_NETWORK_IOCTL;
	/* prCmdInfo->ucBssIndex = prRemovedKey->ucBssIdx; */
	prCmdInfo->u2InfoBufLen = CMD_HDR_SIZE + sizeof(CMD_802_11_KEY);
	prCmdInfo->pfCmdDoneHandler = nicCmdEventSetCommon;
	prCmdInfo->pfCmdTimeoutHandler = nicOidCmdTimeoutCommon;
	prCmdInfo->fgIsOid = g_fgIsOid;
	prCmdInfo->ucCID = CMD_ID_ADD_REMOVE_KEY;
	prCmdInfo->fgSetQuery = TRUE;
	prCmdInfo->fgNeedResp = FALSE;
	/* prCmdInfo->fgDriverDomainMCR = FALSE; */
	prCmdInfo->ucCmdSeqNum = ucCmdSeqNum;
	prCmdInfo->u4SetInfoLen = sizeof(PARAM_REMOVE_KEY_T);
	prCmdInfo->pvInformationBuffer = pvSetBuffer;
	prCmdInfo->u4InformationBufferLength = u4SetBufferLen;

	/* Setup WIFI_CMD_T */
	prWifiCmd = (P_WIFI_CMD_T) (prCmdInfo->pucInfoBuffer);
	prWifiCmd->u2TxByteCount = prCmdInfo->u2InfoBufLen;
	prWifiCmd->u2PQ_ID = CMD_PQ_ID;
	prWifiCmd->ucPktTypeID = CMD_PACKET_TYPE_ID;
	prWifiCmd->ucCID = prCmdInfo->ucCID;
	prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
	prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;

	prCmdKey = (P_CMD_802_11_KEY) (prWifiCmd->aucBuffer);

	kalMemZero((PUINT_8) prCmdKey, sizeof(CMD_802_11_KEY));

	prCmdKey->ucAddRemove = 0;	/* Remove */
	prCmdKey->ucKeyId = (UINT_8) u4KeyIndex;
	kalMemCopy(prCmdKey->aucPeerAddr, (PUINT_8) prRemovedKey->arBSSID, MAC_ADDR_LEN);
	prCmdKey->ucBssIdx = prRemovedKey->ucBssIdx;

	if (prStaRec) {
		prCmdKey->ucKeyType = 1;
		prCmdKey->ucWlanIndex = prStaRec->ucWlanIndex;
		prStaRec->fgTransmitKeyExist = FALSE;
	} else if (ucRemoveBCKeyAtIdx < WTBL_SIZE) {
		prCmdKey->ucWlanIndex = ucRemoveBCKeyAtIdx;
	} else {
		ASSERT(FALSE);
	}

	/* insert into prCmdQueue */
	kalEnqueueCommand(prGlueInfo, (P_QUE_ENTRY_T) prCmdInfo);

	/* wakeup txServiceThread later */
	GLUE_SET_EVENT(prGlueInfo);

	return WLAN_STATUS_PENDING;
}				/* wlanoidSetRemoveKey */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set the default key
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
*
* \note The setting buffer PARAM_KEY_T, which is set by NDIS, is unpacked.
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetDefaultKey(IN P_ADAPTER_T prAdapter,
		     IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_GLUE_INFO_T prGlueInfo;
	P_CMD_INFO_T prCmdInfo;
	P_WIFI_CMD_T prWifiCmd;
	P_PARAM_DEFAULT_KEY_T prDefaultKey;
	P_CMD_DEFAULT_KEY prCmdDefaultKey;
	UINT_8 ucCmdSeqNum;
	P_BSS_INFO_T prBssInfo;
	BOOL fgSetWepKey = FALSE;
	UINT_8 ucWlanIndex = WTBL_RESERVED_ENTRY;

	DEBUGFUNC("wlanoidSetDefaultKey");
	DBGLOG(REQ, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN, "Fail in set add key! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	prDefaultKey = (P_PARAM_DEFAULT_KEY_T) pvSetBuffer;

	*pu4SetInfoLen = u4SetBufferLen;

	/* Dump PARAM_DEFAULT_KEY_T content. */
	DBGLOG(RSN, INFO, "Key Index : %d\n", prDefaultKey->ucKeyID);
	DBGLOG(RSN, INFO, "Unicast Key : %d\n", prDefaultKey->ucUnicast);
	DBGLOG(RSN, INFO, "Multicast Key : %d\n", prDefaultKey->ucMulticast);

	/* prWlanTable = prAdapter->rWifiVar.arWtbl; */
	prGlueInfo = prAdapter->prGlueInfo;

	if (prDefaultKey->ucBssIdx > HW_BSSID_NUM)
		return WLAN_STATUS_FAILURE;
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prDefaultKey->ucBssIdx);

	DBGLOG(RSN, INFO, "WlanIdx = %d\n", prBssInfo->wepkeyWlanIdx);

	if (prDefaultKey->ucMulticast) {
		if (prBssInfo->prStaRecOfAP) {	/* Actually GC not have wep */
			if (prBssInfo->wepkeyUsed[prDefaultKey->ucKeyID]) {
				prBssInfo->ucBcDefaultKeyIdx = prDefaultKey->ucKeyID;
				prBssInfo->fgBcDefaultKeyExist = TRUE;
				ucWlanIndex = prBssInfo->wepkeyWlanIdx;
			} else {
				DBGLOG(RSN, ERROR, "WPA encryption should retrun");
				return WLAN_STATUS_SUCCESS;
			}
		} else {	/* For AP mode only */

			if (prBssInfo->wepkeyUsed[prDefaultKey->ucKeyID] == TRUE)
				fgSetWepKey = TRUE;

			if (fgSetWepKey) {
				ucWlanIndex = prBssInfo->wepkeyWlanIdx;
			} else {
				if (!prBssInfo->ucBMCWlanIndexSUsed[prDefaultKey->ucKeyID]) {
					DBGLOG(RSN, ERROR, "Set AP wep default but key not exist!");
					return WLAN_STATUS_SUCCESS;
				}
				ucWlanIndex = prBssInfo->ucBMCWlanIndexS[prDefaultKey->ucKeyID];
			}
			prBssInfo->ucBcDefaultKeyIdx = prDefaultKey->ucKeyID;
			prBssInfo->fgBcDefaultKeyExist = TRUE;
		}
		if (ucWlanIndex > WTBL_SIZE)
			ASSERT(FALSE);

	} else {
		DBGLOG(RSN, ERROR, "Check the case set unicast default key!");
		ASSERT(FALSE);
	}

	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, (CMD_HDR_SIZE + sizeof(CMD_DEFAULT_KEY)));

	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}
	/* increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);
	DBGLOG(REQ, INFO, "ucCmdSeqNum = %d\n", ucCmdSeqNum);

	/* compose CMD_802_11_KEY cmd pkt */
	prCmdInfo->eCmdType = COMMAND_TYPE_NETWORK_IOCTL;
	prCmdInfo->u2InfoBufLen = CMD_HDR_SIZE + sizeof(CMD_DEFAULT_KEY);
	prCmdInfo->pfCmdDoneHandler = nicCmdEventSetCommon;
	prCmdInfo->pfCmdTimeoutHandler = nicOidCmdTimeoutCommon;
	prCmdInfo->fgIsOid = g_fgIsOid;
	prCmdInfo->ucCID = CMD_ID_DEFAULT_KEY_ID;
	prCmdInfo->fgSetQuery = TRUE;
	prCmdInfo->fgNeedResp = FALSE;
	prCmdInfo->ucCmdSeqNum = ucCmdSeqNum;
	prCmdInfo->u4SetInfoLen = u4SetBufferLen;
	prCmdInfo->pvInformationBuffer = pvSetBuffer;
	prCmdInfo->u4InformationBufferLength = u4SetBufferLen;

	/* Setup WIFI_CMD_T */
	prWifiCmd = (P_WIFI_CMD_T) (prCmdInfo->pucInfoBuffer);
	prWifiCmd->u2TxByteCount = prCmdInfo->u2InfoBufLen;
	prWifiCmd->u2PQ_ID = CMD_PQ_ID;
	prWifiCmd->ucPktTypeID = CMD_PACKET_TYPE_ID;
	prWifiCmd->ucCID = prCmdInfo->ucCID;
	prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
	prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;

	prCmdDefaultKey = (P_CMD_DEFAULT_KEY) (prWifiCmd->aucBuffer);

	kalMemZero(prCmdDefaultKey, sizeof(CMD_DEFAULT_KEY));

	prCmdDefaultKey->ucBssIdx = prDefaultKey->ucBssIdx;
	prCmdDefaultKey->ucKeyId = prDefaultKey->ucKeyID;
	prCmdDefaultKey->ucWlanIndex = ucWlanIndex;
	prCmdDefaultKey->ucMulticast = prDefaultKey->ucMulticast;

	DBGLOG(RSN, INFO, "CMD_ID_DEFAULT_KEY_ID (%d) with wlan idx = %d\n", prDefaultKey->ucKeyID, ucWlanIndex);

	/* insert into prCmdQueue */
	kalEnqueueCommand(prGlueInfo, (P_QUE_ENTRY_T) prCmdInfo);

	/* wakeup txServiceThread later */
	GLUE_SET_EVENT(prGlueInfo);

	return WLAN_STATUS_PENDING;
}				/* wlanoidSetDefaultKey */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the current encryption status.
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[out] pvQueryBuffer A pointer to the buffer that holds the result of
*                           the query.
* \param[in] u4QueryBufferLen The length of the query buffer.
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                            bytes written into the query buffer. If the call
*                            failed due to invalid length of the query buffer,
*                            returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryEncryptionStatus(IN P_ADAPTER_T prAdapter,
			     IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	BOOLEAN fgTransmitKeyAvailable = TRUE;
	ENUM_PARAM_ENCRYPTION_STATUS_T eEncStatus = 0;

	DEBUGFUNC("wlanoidQueryEncryptionStatus");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(ENUM_PARAM_ENCRYPTION_STATUS_T);

	fgTransmitKeyAvailable = prAdapter->prAisBssInfo->fgBcDefaultKeyExist;

	switch (prAdapter->rWifiVar.rConnSettings.eEncStatus) {
	case ENUM_ENCRYPTION3_ENABLED:
		if (fgTransmitKeyAvailable)
			eEncStatus = ENUM_ENCRYPTION3_ENABLED;
		else
			eEncStatus = ENUM_ENCRYPTION3_KEY_ABSENT;
		break;

	case ENUM_ENCRYPTION2_ENABLED:
		if (fgTransmitKeyAvailable) {
			eEncStatus = ENUM_ENCRYPTION2_ENABLED;
			break;
		}
		eEncStatus = ENUM_ENCRYPTION2_KEY_ABSENT;
		break;

	case ENUM_ENCRYPTION1_ENABLED:
		if (fgTransmitKeyAvailable)
			eEncStatus = ENUM_ENCRYPTION1_ENABLED;
		else
			eEncStatus = ENUM_ENCRYPTION1_KEY_ABSENT;
		break;

	case ENUM_ENCRYPTION_DISABLED:
		eEncStatus = ENUM_ENCRYPTION_DISABLED;
		break;

	default:
		DBGLOG(REQ, ERROR, "Unknown Encryption Status Setting:%d\n",
		       prAdapter->rWifiVar.rConnSettings.eEncStatus);
	}

#if DBG
	DBGLOG(REQ, INFO,
	       "Encryption status: %d Return:%d\n", prAdapter->rWifiVar.rConnSettings.eEncStatus, eEncStatus);
#endif

	*(P_ENUM_PARAM_ENCRYPTION_STATUS_T) pvQueryBuffer = eEncStatus;

	return WLAN_STATUS_SUCCESS;
}				/* wlanoidQueryEncryptionStatus */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set the encryption status to the driver.
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
* \retval WLAN_STATUS_NOT_SUPPORTED
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetEncryptionStatus(IN P_ADAPTER_T prAdapter,
			   IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_GLUE_INFO_T prGlueInfo;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	ENUM_PARAM_ENCRYPTION_STATUS_T eEewEncrypt;

	DEBUGFUNC("wlanoidSetEncryptionStatus");

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	prGlueInfo = prAdapter->prGlueInfo;

	*pu4SetInfoLen = sizeof(ENUM_PARAM_ENCRYPTION_STATUS_T);

	/* if (IS_ARB_IN_RFTEST_STATE(prAdapter)) { */
	/* return WLAN_STATUS_SUCCESS; */
	/* } */

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in set encryption status! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	eEewEncrypt = *(P_ENUM_PARAM_ENCRYPTION_STATUS_T) pvSetBuffer;
	DBGLOG(REQ, INFO, "ENCRYPTION_STATUS %d\n", eEewEncrypt);

	switch (eEewEncrypt) {
	case ENUM_ENCRYPTION_DISABLED:	/* Disable WEP, TKIP, AES */
		DBGLOG(RSN, INFO, "Disable Encryption\n");
		secSetCipherSuite(prAdapter, CIPHER_FLAG_WEP40 | CIPHER_FLAG_WEP104 | CIPHER_FLAG_WEP128);
		break;

	case ENUM_ENCRYPTION1_ENABLED:	/* Enable WEP. Disable TKIP, AES */
		DBGLOG(RSN, INFO, "Enable Encryption1\n");
		secSetCipherSuite(prAdapter, CIPHER_FLAG_WEP40 | CIPHER_FLAG_WEP104 | CIPHER_FLAG_WEP128);
		break;

	case ENUM_ENCRYPTION2_ENABLED:	/* Enable WEP, TKIP. Disable AES */
		secSetCipherSuite(prAdapter,
				  CIPHER_FLAG_WEP40 | CIPHER_FLAG_WEP104 | CIPHER_FLAG_WEP128 | CIPHER_FLAG_TKIP);
		DBGLOG(RSN, INFO, "Enable Encryption2\n");
		break;

	case ENUM_ENCRYPTION3_ENABLED:	/* Enable WEP, TKIP, AES */
		secSetCipherSuite(prAdapter,
				  CIPHER_FLAG_WEP40 |
				  CIPHER_FLAG_WEP104 | CIPHER_FLAG_WEP128 | CIPHER_FLAG_TKIP | CIPHER_FLAG_CCMP);
		DBGLOG(RSN, INFO, "Enable Encryption3\n");
		break;
#if CFG_SUPPORT_SUITB
	case ENUM_ENCRYPTION4_ENABLED: /* Eanble GCMP256 */
		secSetCipherSuite(prAdapter, CIPHER_FLAG_GCMP256);
		DBGLOG(RSN, INFO, "Enable Encryption4\n");
		break;
#endif
	default:
		DBGLOG(RSN, INFO, "Unacceptible encryption status: %d\n",
		       *(P_ENUM_PARAM_ENCRYPTION_STATUS_T) pvSetBuffer);

		rStatus = WLAN_STATUS_NOT_SUPPORTED;
	}

	if (rStatus == WLAN_STATUS_SUCCESS) {
		/* Save the new encryption status. */
		prAdapter->rWifiVar.rConnSettings.eEncStatus = *(P_ENUM_PARAM_ENCRYPTION_STATUS_T) pvSetBuffer;
	}

	return rStatus;
}				/* wlanoidSetEncryptionStatus */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to test the driver.
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
* \retval WLAN_STATUS_INVALID_LENGTH
* \retval WLAN_STATUS_INVALID_DATA
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetTest(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_PARAM_802_11_TEST_T prTest;
	PVOID pvTestData;
	PVOID pvStatusBuffer;
	UINT_32 u4StatusBufferSize;

	DEBUGFUNC("wlanoidSetTest");

	ASSERT(prAdapter);

	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = u4SetBufferLen;

	prTest = (P_PARAM_802_11_TEST_T) pvSetBuffer;

	DBGLOG(REQ, TRACE, "Test - Type %u\n", prTest->u4Type);

	switch (prTest->u4Type) {
	case 1:		/* Type 1: generate an authentication event */
		pvTestData = (PVOID) &prTest->u.AuthenticationEvent;
		pvStatusBuffer = (PVOID) prAdapter->aucIndicationEventBuffer;
		u4StatusBufferSize = prTest->u4Length - 8;
		if (u4StatusBufferSize > sizeof(prTest->u.AuthenticationEvent))
			return WLAN_STATUS_INVALID_LENGTH;
		break;

	case 2:		/* Type 2: generate an RSSI status indication */
		pvTestData = (PVOID) &prTest->u.RssiTrigger;
		pvStatusBuffer = (PVOID) &prAdapter->rWlanInfo.rCurrBssId.rRssi;
		u4StatusBufferSize = sizeof(PARAM_RSSI);
		break;

	default:
		return WLAN_STATUS_INVALID_DATA;
	}

	/* Get the contents of the StatusBuffer from the test structure. */
	kalMemCopy(pvStatusBuffer, pvTestData, u4StatusBufferSize);

	kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
				     WLAN_STATUS_MEDIA_SPECIFIC_INDICATION, pvStatusBuffer, u4StatusBufferSize);

	return WLAN_STATUS_SUCCESS;
}				/* wlanoidSetTest */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the driver's WPA2 status.
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[out] pvQueryBuffer A pointer to the buffer that holds the result of
*                           the query.
* \param[in] u4QueryBufferLen The length of the query buffer.
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                            bytes written into the query buffer. If the call
*                            failed due to invalid length of the query buffer,
*                            returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryCapability(IN P_ADAPTER_T prAdapter,
		       OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	P_PARAM_CAPABILITY_T prCap;
	P_PARAM_AUTH_ENCRYPTION_T prAuthenticationEncryptionSupported;

	DEBUGFUNC("wlanoidQueryCapability");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = 4 * sizeof(UINT_32) + 14 * sizeof(PARAM_AUTH_ENCRYPTION_T);

	if (u4QueryBufferLen < *pu4QueryInfoLen)
		return WLAN_STATUS_INVALID_LENGTH;

	prCap = (P_PARAM_CAPABILITY_T) pvQueryBuffer;

	prCap->u4Length = *pu4QueryInfoLen;
	prCap->u4Version = 2;	/* WPA2 */
	prCap->u4NoOfPMKIDs = CFG_MAX_PMKID_CACHE;
	prCap->u4NoOfAuthEncryptPairsSupported = 14;

	prAuthenticationEncryptionSupported = &prCap->arAuthenticationEncryptionSupported[0];

	/* fill 14 entries of supported settings */
	prAuthenticationEncryptionSupported[0].eAuthModeSupported = AUTH_MODE_OPEN;

	prAuthenticationEncryptionSupported[0].eEncryptStatusSupported = ENUM_ENCRYPTION_DISABLED;

	prAuthenticationEncryptionSupported[1].eAuthModeSupported = AUTH_MODE_OPEN;
	prAuthenticationEncryptionSupported[1].eEncryptStatusSupported = ENUM_ENCRYPTION1_ENABLED;

	prAuthenticationEncryptionSupported[2].eAuthModeSupported = AUTH_MODE_SHARED;
	prAuthenticationEncryptionSupported[2].eEncryptStatusSupported = ENUM_ENCRYPTION_DISABLED;

	prAuthenticationEncryptionSupported[3].eAuthModeSupported = AUTH_MODE_SHARED;
	prAuthenticationEncryptionSupported[3].eEncryptStatusSupported = ENUM_ENCRYPTION1_ENABLED;

	prAuthenticationEncryptionSupported[4].eAuthModeSupported = AUTH_MODE_WPA;
	prAuthenticationEncryptionSupported[4].eEncryptStatusSupported = ENUM_ENCRYPTION2_ENABLED;

	prAuthenticationEncryptionSupported[5].eAuthModeSupported = AUTH_MODE_WPA;
	prAuthenticationEncryptionSupported[5].eEncryptStatusSupported = ENUM_ENCRYPTION3_ENABLED;

	prAuthenticationEncryptionSupported[6].eAuthModeSupported = AUTH_MODE_WPA_PSK;
	prAuthenticationEncryptionSupported[6].eEncryptStatusSupported = ENUM_ENCRYPTION2_ENABLED;

	prAuthenticationEncryptionSupported[7].eAuthModeSupported = AUTH_MODE_WPA_PSK;
	prAuthenticationEncryptionSupported[7].eEncryptStatusSupported = ENUM_ENCRYPTION3_ENABLED;

	prAuthenticationEncryptionSupported[8].eAuthModeSupported = AUTH_MODE_WPA_NONE;
	prAuthenticationEncryptionSupported[8].eEncryptStatusSupported = ENUM_ENCRYPTION2_ENABLED;

	prAuthenticationEncryptionSupported[9].eAuthModeSupported = AUTH_MODE_WPA_NONE;
	prAuthenticationEncryptionSupported[9].eEncryptStatusSupported = ENUM_ENCRYPTION3_ENABLED;

	prAuthenticationEncryptionSupported[10].eAuthModeSupported = AUTH_MODE_WPA2;
	prAuthenticationEncryptionSupported[10].eEncryptStatusSupported = ENUM_ENCRYPTION2_ENABLED;

	prAuthenticationEncryptionSupported[11].eAuthModeSupported = AUTH_MODE_WPA2;
	prAuthenticationEncryptionSupported[11].eEncryptStatusSupported = ENUM_ENCRYPTION3_ENABLED;

	prAuthenticationEncryptionSupported[12].eAuthModeSupported = AUTH_MODE_WPA2_PSK;
	prAuthenticationEncryptionSupported[12].eEncryptStatusSupported = ENUM_ENCRYPTION2_ENABLED;

	prAuthenticationEncryptionSupported[13].eAuthModeSupported = AUTH_MODE_WPA2_PSK;
	prAuthenticationEncryptionSupported[13].eEncryptStatusSupported = ENUM_ENCRYPTION3_ENABLED;

	return WLAN_STATUS_SUCCESS;

}				/* wlanoidQueryCapability */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the PMKID in the PMK cache.
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[out] pvQueryBuffer A pointer to the buffer that holds the result of
*                           the query.
* \param[in] u4QueryBufferLen The length of the query buffer.
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                             bytes written into the query buffer. If the call
*                             failed due to invalid length of the query buffer,
*                             returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryPmkid(IN P_ADAPTER_T prAdapter,
		  OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	UINT_32 i;
	P_PARAM_PMKID_T prPmkid;
	P_AIS_SPECIFIC_BSS_INFO_T prAisSpecBssInfo;

	DEBUGFUNC("wlanoidQueryPmkid");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	prAisSpecBssInfo = &prAdapter->rWifiVar.rAisSpecificBssInfo;

	*pu4QueryInfoLen = OFFSET_OF(PARAM_PMKID_T, arBSSIDInfo) +
	    prAisSpecBssInfo->u4PmkidCacheCount * sizeof(PARAM_BSSID_INFO_T);

	if (u4QueryBufferLen < *pu4QueryInfoLen)
		return WLAN_STATUS_INVALID_LENGTH;

	prPmkid = (P_PARAM_PMKID_T) pvQueryBuffer;

	prPmkid->u4Length = *pu4QueryInfoLen;
	prPmkid->u4BSSIDInfoCount = prAisSpecBssInfo->u4PmkidCacheCount;

	for (i = 0; i < prAisSpecBssInfo->u4PmkidCacheCount; i++) {
		kalMemCopy(prPmkid->arBSSIDInfo[i].arBSSID,
			   prAisSpecBssInfo->arPmkidCache[i].rBssidInfo.arBSSID, sizeof(PARAM_MAC_ADDRESS));
		kalMemCopy(prPmkid->arBSSIDInfo[i].arPMKID,
			   prAisSpecBssInfo->arPmkidCache[i].rBssidInfo.arPMKID, sizeof(PARAM_PMKID_VALUE));
	}

	return WLAN_STATUS_SUCCESS;

}				/* wlanoidQueryPmkid */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set the PMKID to the PMK cache in the driver.
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[in] pvSetBuffer A pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                           bytes read from the set buffer. If the call failed
*                           due to invalid length of the set buffer, returns
*                           the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_BUFFER_TOO_SHORT
* \retval WLAN_STATUS_INVALID_DATA
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetPmkid(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	UINT_32 i, j;
	P_PARAM_PMKID_T prPmkid;
	P_AIS_SPECIFIC_BSS_INFO_T prAisSpecBssInfo;

	DEBUGFUNC("wlanoidSetPmkid");

	DBGLOG(REQ, INFO, "wlanoidSetPmkid\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = u4SetBufferLen;

	/* It's possibble BSSIDInfoCount is zero, because OS wishes to clean PMKID */
	if (u4SetBufferLen < OFFSET_OF(PARAM_PMKID_T, arBSSIDInfo))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	ASSERT(pvSetBuffer);
	prPmkid = (P_PARAM_PMKID_T) pvSetBuffer;

	if (u4SetBufferLen <
	    ((prPmkid->u4BSSIDInfoCount * sizeof(PARAM_BSSID_INFO_T)) + OFFSET_OF(PARAM_PMKID_T, arBSSIDInfo)))
		return WLAN_STATUS_INVALID_DATA;

	if (prPmkid->u4BSSIDInfoCount > CFG_MAX_PMKID_CACHE)
		return WLAN_STATUS_INVALID_DATA;

	DBGLOG(REQ, INFO, "Count %u\n", prPmkid->u4BSSIDInfoCount);

	prAisSpecBssInfo = &prAdapter->rWifiVar.rAisSpecificBssInfo;

	/* This OID replace everything in the PMKID cache. */
	if (prPmkid->u4BSSIDInfoCount == 0) {
		prAisSpecBssInfo->u4PmkidCacheCount = 0;
		kalMemZero(prAisSpecBssInfo->arPmkidCache, sizeof(PMKID_ENTRY_T) * CFG_MAX_PMKID_CACHE);
	}
	if ((prAisSpecBssInfo->u4PmkidCacheCount + prPmkid->u4BSSIDInfoCount > CFG_MAX_PMKID_CACHE)) {
		prAisSpecBssInfo->u4PmkidCacheCount = 0;
		kalMemZero(prAisSpecBssInfo->arPmkidCache, sizeof(PMKID_ENTRY_T) * CFG_MAX_PMKID_CACHE);
	}

	/*
	 *  The driver can only clear its PMKID cache whenever it make a media disconnect
	 *  indication. Otherwise, it must change the PMKID cache only when set through this OID.
	 */
	for (i = 0; i < prPmkid->u4BSSIDInfoCount; i++) {
		/* Search for desired BSSID. If desired BSSID is found,
		 *  then set the PMKID
		 */
		if (!rsnSearchPmkidEntry(prAdapter, (PUINT_8) prPmkid->arBSSIDInfo[i].arBSSID, &j)) {
			/* No entry found for the specified BSSID, so add one entry */
			if (prAisSpecBssInfo->u4PmkidCacheCount < CFG_MAX_PMKID_CACHE - 1) {
				j = prAisSpecBssInfo->u4PmkidCacheCount;
				kalMemCopy(prAisSpecBssInfo->arPmkidCache[j].rBssidInfo.arBSSID,
					   prPmkid->arBSSIDInfo[i].arBSSID, sizeof(PARAM_MAC_ADDRESS));
				prAisSpecBssInfo->u4PmkidCacheCount++;
			} else {
				j = CFG_MAX_PMKID_CACHE;
			}
		}

		if (j < CFG_MAX_PMKID_CACHE) {
			kalMemCopy(prAisSpecBssInfo->arPmkidCache[j].rBssidInfo.arPMKID,
				   prPmkid->arBSSIDInfo[i].arPMKID, sizeof(PARAM_PMKID_VALUE));
			DBGLOG(RSN, TRACE,
			"Add BSSID " MACSTR " idx=%u PMKID value " MACSTR "\n",
			       MAC2STR(prAisSpecBssInfo->arPmkidCache[j].rBssidInfo.arBSSID), j,
			       MAC2STR(prAisSpecBssInfo->arPmkidCache[j].rBssidInfo.arPMKID));
			prAisSpecBssInfo->arPmkidCache[j].fgPmkidExist = TRUE;
		}
	}

	return WLAN_STATUS_SUCCESS;
}				/* wlanoidSetPmkid */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the set of supported data rates that
*          the radio is capable of running
*
* \param[in] prAdapter Pointer to the Adapter structure
* \param[out] pvQueryBuffer A pointer to the buffer that holds the result of
*                           the query
* \param[in] u4QueryBufferLen The length of the query buffer
* \param[out] pu4QueryInfoLen If the call is successful, returns the number
*                             of bytes written into the query buffer. If the
*                             call failed due to invalid length of the query
*                             buffer, returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQuerySupportedRates(IN P_ADAPTER_T prAdapter,
			   OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	PARAM_RATES eRate = {
		/* BSSBasicRateSet for 802.11n Non-HT rates */
		0x8C,		/* 6M */
		0x92,		/* 9M */
		0x98,		/* 12M */
		0xA4,		/* 18M */
		0xB0,		/* 24M */
		0xC8,		/* 36M */
		0xE0,		/* 48M */
		0xEC		/* 54M */
	};

	DEBUGFUNC("wlanoidQuerySupportedRates");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(PARAM_RATES_EX);

	if (u4QueryBufferLen < *pu4QueryInfoLen) {
		DBGLOG(REQ, WARN, "Invalid length %u\n", u4QueryBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	kalMemCopy(pvQueryBuffer, (PVOID) &eRate, sizeof(PARAM_RATES));

	return WLAN_STATUS_SUCCESS;
}				/* end of wlanoidQuerySupportedRates() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query current desired rates.
*
* \param[in] prAdapter          Pointer to the Adapter structure.
* \param[in] pvQueryBuffer      Pointer to the buffer that holds the result of
*                               the query.
* \param[in] u4QueryBufferLen   The length of the query buffer.
* \param[out] pu4QueryInfoLen   If the call is successful, returns the number of
*                               bytes written into the query buffer. If the call
*                               failed due to invalid length of the query buffer,
*                               returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryDesiredRates(IN P_ADAPTER_T prAdapter,
			 OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	DEBUGFUNC("wlanoidQueryDesiredRates");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(PARAM_RATES_EX);

	if (u4QueryBufferLen < *pu4QueryInfoLen) {
		DBGLOG(REQ, WARN, "Invalid length %u\n", u4QueryBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	kalMemCopy(pvQueryBuffer, (PVOID) &(prAdapter->rWlanInfo.eDesiredRates), sizeof(PARAM_RATES));

	return WLAN_STATUS_SUCCESS;

}				/* end of wlanoidQueryDesiredRates() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to Set the desired rates.
*
* \param[in] prAdapter      Pointer to the Adapter structure.
* \param[in] pvSetBuffer    Pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                           bytes read from the set buffer. If the call failed
*                           due to invalid length of the set buffer, returns
*                           the amount of storage needed.
*
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
* \retval WLAN_STATUS_INVALID_DATA
* \retval WLAN_STATUS_ADAPTER_NOT_READY
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetDesiredRates(IN P_ADAPTER_T prAdapter,
		       IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	UINT_32 i;

	DEBUGFUNC("wlanoidSetDesiredRates");

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	if (u4SetBufferLen < sizeof(PARAM_RATES)) {
		DBGLOG(REQ, WARN, "Invalid length %u\n", u4SetBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	*pu4SetInfoLen = sizeof(PARAM_RATES);

	if (u4SetBufferLen < sizeof(PARAM_RATES))
		return WLAN_STATUS_INVALID_LENGTH;

	kalMemCopy((PVOID) &(prAdapter->rWlanInfo.eDesiredRates), pvSetBuffer, sizeof(PARAM_RATES));

	prAdapter->rWlanInfo.eLinkAttr.ucDesiredRateLen = PARAM_MAX_LEN_RATES;
	for (i = 0; i < PARAM_MAX_LEN_RATES; i++)
		prAdapter->rWlanInfo.eLinkAttr.u2DesiredRate[i] = (UINT_16) (prAdapter->rWlanInfo.eDesiredRates[i]);

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_LINK_ATTRIB,
				   TRUE,
				   FALSE,
				   TRUE,
				   nicCmdEventSetCommon,
				   nicOidCmdTimeoutCommon,
				   sizeof(CMD_LINK_ATTRIB),
				   (PUINT_8) &(prAdapter->rWlanInfo.eLinkAttr), pvSetBuffer, u4SetBufferLen);

}				/* end of wlanoidSetDesiredRates() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the maximum frame size in bytes,
*        not including the header.
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[out] pvQueryBuffer A pointer to the buffer that holds the result of
*                           the query.
* \param[in] u4QueryBufferLen The length of the query buffer.
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                               bytes written into the query buffer. If the
*                               call failed due to invalid length of the query
*                               buffer, returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryMaxFrameSize(IN P_ADAPTER_T prAdapter,
			 OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	DEBUGFUNC("wlanoidQueryMaxFrameSize");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (u4QueryBufferLen < sizeof(UINT_32)) {
		*pu4QueryInfoLen = sizeof(UINT_32);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	*(PUINT_32) pvQueryBuffer = ETHERNET_MAX_PKT_SZ - ETHERNET_HEADER_SZ;
	*pu4QueryInfoLen = sizeof(UINT_32);

	return WLAN_STATUS_SUCCESS;
}				/* wlanoidQueryMaxFrameSize */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the maximum total packet length
*        in bytes.
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[out] pvQueryBuffer A pointer to the buffer that holds the result of
*                           the query.
* \param[in] u4QueryBufferLen The length of the query buffer.
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                            bytes written into the query buffer. If the call
*                            failed due to invalid length of the query buffer,
*                            returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryMaxTotalSize(IN P_ADAPTER_T prAdapter,
			 OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	DEBUGFUNC("wlanoidQueryMaxTotalSize");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (u4QueryBufferLen < sizeof(UINT_32)) {
		*pu4QueryInfoLen = sizeof(UINT_32);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	*(PUINT_32) pvQueryBuffer = ETHERNET_MAX_PKT_SZ;
	*pu4QueryInfoLen = sizeof(UINT_32);

	return WLAN_STATUS_SUCCESS;
}				/* wlanoidQueryMaxTotalSize */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the vendor ID of the NIC.
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[out] pvQueryBuffer A pointer to the buffer that holds the result of
*                           the query.
* \param[in] u4QueryBufferLen The length of the query buffer.
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                            bytes written into the query buffer. If the call
*                            failed due to invalid length of the query buffer,
*                            returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryVendorId(IN P_ADAPTER_T prAdapter,
		     OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
#if DBG
	PUINT_8 cp;
#endif
	DEBUGFUNC("wlanoidQueryVendorId");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (u4QueryBufferLen < sizeof(UINT_32)) {
		*pu4QueryInfoLen = sizeof(UINT_32);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	kalMemCopy(pvQueryBuffer, prAdapter->aucMacAddress, 3);
	*((PUINT_8) pvQueryBuffer + 3) = 1;
	*pu4QueryInfoLen = sizeof(UINT_32);

#if DBG
	cp = (PUINT_8) pvQueryBuffer;
	DBGLOG(REQ, LOUD, "Vendor ID=%02x-%02x-%02x-%02x\n", cp[0], cp[1], cp[2], cp[3]);
#endif

	return WLAN_STATUS_SUCCESS;
}				/* wlanoidQueryVendorId */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the current RSSI value.
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[in] pvQueryBuffer Pointer to the buffer that holds the result of the query.
* \param[in] u4QueryBufferLen The length of the query buffer.
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*   bytes written into the query buffer. If the call failed due to invalid length of
*   the query buffer, returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_BUFFER_TOO_SHORT
* \retval WLAN_STATUS_ADAPTER_NOT_READY
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryRssi(IN P_ADAPTER_T prAdapter,
		 OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	return _wlanoidQueryRssi(prAdapter,
				pvQueryBuffer,
				u4QueryBufferLen,
				pu4QueryInfoLen,
				g_fgIsOid);
}

WLAN_STATUS
_wlanoidQueryRssi(IN P_ADAPTER_T prAdapter,
		OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen,
		OUT PUINT_32 pu4QueryInfoLen, IN BOOLEAN fgIsOid)
{
	DEBUGFUNC("wlanoidQueryRssi");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (prAdapter->fgIsEnableLpdvt)
		return WLAN_STATUS_NOT_SUPPORTED;

	*pu4QueryInfoLen = sizeof(PARAM_RSSI);

	/* Check for query buffer length */
	if (u4QueryBufferLen < *pu4QueryInfoLen) {
		DBGLOG(REQ, WARN, "Too short length %u\n", u4QueryBufferLen);
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}

	if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) == PARAM_MEDIA_STATE_DISCONNECTED) {
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (prAdapter->fgIsLinkQualityValid == TRUE &&
		   (kalGetTimeTick() - prAdapter->rLinkQualityUpdateTime) <= CFG_LINK_QUALITY_VALID_PERIOD) {
		PARAM_RSSI rRssi;

		rRssi = (PARAM_RSSI) prAdapter->rLinkQuality.cRssi;	/* ranged from (-128 ~ 30) in unit of dBm */

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
				   fgIsOid,
				   nicCmdEventQueryLinkQuality,
				   nicOidCmdTimeoutCommon,
				   *pu4QueryInfoLen, pvQueryBuffer, pvQueryBuffer, u4QueryBufferLen);
#else
	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_GET_LINK_QUALITY,
				   FALSE,
				   TRUE,
				   fgIsOid,
				   nicCmdEventQueryLinkQuality,
				   nicOidCmdTimeoutCommon, 0, NULL, pvQueryBuffer, u4QueryBufferLen);

#endif
}				/* end of wlanoidQueryRssi() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the current RSSI trigger value.
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[in] pvQueryBuffer Pointer to the buffer that holds the result of the query.
* \param[in] u4QueryBufferLen The length of the query buffer.
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*   bytes written into the query buffer. If the call failed due to invalid length of
*   the query buffer, returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_BUFFER_TOO_SHORT
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryRssiTrigger(IN P_ADAPTER_T prAdapter,
			OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	DEBUGFUNC("wlanoidQueryRssiTrigger");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (prAdapter->rWlanInfo.eRssiTriggerType == ENUM_RSSI_TRIGGER_NONE)
		return WLAN_STATUS_ADAPTER_NOT_READY;

	*pu4QueryInfoLen = sizeof(PARAM_RSSI);

	/* Check for query buffer length */
	if (u4QueryBufferLen < *pu4QueryInfoLen) {
		DBGLOG(REQ, WARN, "Too short length %u\n", u4QueryBufferLen);
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}

	*(PARAM_RSSI *) pvQueryBuffer = prAdapter->rWlanInfo.rRssiTriggerValue;
	DBGLOG(REQ, INFO, "RSSI trigger: %d dBm\n",
			*(PARAM_RSSI *) pvQueryBuffer);

	return WLAN_STATUS_SUCCESS;
}				/* wlanoidQueryRssiTrigger */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set a trigger value of the RSSI event.
*
* \param[in] prAdapter Pointer to the Adapter structure
* \param[in] pvSetBuffer A pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                          bytes read from the set buffer. If the call failed
*                          due to invalid length of the set buffer, returns the
*                          amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
*
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetRssiTrigger(IN P_ADAPTER_T prAdapter,
		      IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	PARAM_RSSI rRssiTriggerValue;

	DEBUGFUNC("wlanoidSetRssiTrigger");

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(PARAM_RSSI);
	rRssiTriggerValue = *(PARAM_RSSI *) pvSetBuffer;

	if (rRssiTriggerValue > PARAM_WHQL_RSSI_MAX_DBM || rRssiTriggerValue < PARAM_WHQL_RSSI_MIN_DBM)
		return
		    /* Save the RSSI trigger value to the Adapter structure */
		    prAdapter->rWlanInfo.rRssiTriggerValue = rRssiTriggerValue;

	/* If the RSSI trigger value is equal to the current RSSI value, the
	 * indication triggers immediately. We need to indicate the protocol
	 * that an RSSI status indication event triggers.
	 */
	if (rRssiTriggerValue == (PARAM_RSSI) (prAdapter->rLinkQuality.cRssi)) {
		prAdapter->rWlanInfo.eRssiTriggerType = ENUM_RSSI_TRIGGER_TRIGGERED;

		kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
					     WLAN_STATUS_MEDIA_SPECIFIC_INDICATION,
					     (PVOID) &prAdapter->rWlanInfo.rRssiTriggerValue, sizeof(PARAM_RSSI));
	} else if (rRssiTriggerValue < (PARAM_RSSI) (prAdapter->rLinkQuality.cRssi))
		prAdapter->rWlanInfo.eRssiTriggerType = ENUM_RSSI_TRIGGER_GREATER;
	else if (rRssiTriggerValue > (PARAM_RSSI) (prAdapter->rLinkQuality.cRssi))
		prAdapter->rWlanInfo.eRssiTriggerType = ENUM_RSSI_TRIGGER_LESS;

	return WLAN_STATUS_SUCCESS;
}				/* wlanoidSetRssiTrigger */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set a suggested value for the number of
*        bytes of received packet data that will be indicated to the protocol
*        driver. We just accept the set and ignore this value.
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
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetCurrentLookahead(IN P_ADAPTER_T prAdapter,
			   IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	DEBUGFUNC("wlanoidSetCurrentLookahead");

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	if (u4SetBufferLen < sizeof(UINT_32)) {
		*pu4SetInfoLen = sizeof(UINT_32);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	*pu4SetInfoLen = sizeof(UINT_32);
	return WLAN_STATUS_SUCCESS;
}				/* wlanoidSetCurrentLookahead */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the number of frames that the driver
*        receives but does not indicate to the protocols due to errors.
*
* \param[in] pvAdapter Pointer to the Adapter structure.
* \param[out] pvQueryBuf A pointer to the buffer that holds the result of
*                           the query.
* \param[in] u4QueryBufLen The length of the query buffer.
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                            bytes written into the query buffer. If the call
*                            failed due to invalid length of the query buffer,
*                            returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
* \retval WLAN_STATUS_ADAPTER_NOT_READY
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryRcvError(IN P_ADAPTER_T prAdapter,
		     IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	DEBUGFUNC("wlanoidQueryRcvError");
	DBGLOG(REQ, LOUD, "\n");

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in query receive error! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		*pu4QueryInfoLen = sizeof(UINT_32);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (u4QueryBufferLen < sizeof(UINT_32)
		   || (u4QueryBufferLen > sizeof(UINT_32) && u4QueryBufferLen < sizeof(UINT_64))) {
		*pu4QueryInfoLen = sizeof(UINT_64);
		return WLAN_STATUS_INVALID_LENGTH;
	}
#if CFG_ENABLE_STATISTICS_BUFFERING
	if (IsBufferedStatisticsUsable(prAdapter) == TRUE) {
		/* @FIXME, RX_ERROR_DROP_COUNT/RX_FIFO_FULL_DROP_COUNT is not calculated */
		if (u4QueryBufferLen == sizeof(UINT_32)) {
			*pu4QueryInfoLen = sizeof(UINT_32);
			*(PUINT_32) pvQueryBuffer = (UINT_32) prAdapter->rStatStruct.rFCSErrorCount.QuadPart;
		} else {
			*pu4QueryInfoLen = sizeof(UINT_64);
			*(PUINT_64) pvQueryBuffer = (UINT_64) prAdapter->rStatStruct.rFCSErrorCount.QuadPart;
		}

		return WLAN_STATUS_SUCCESS;
	}
#endif

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_GET_STATISTICS,
				   FALSE,
				   TRUE,
				   TRUE,
				   nicCmdEventQueryRecvError,
				   nicOidCmdTimeoutCommon, 0, NULL, pvQueryBuffer, u4QueryBufferLen);

}				/* wlanoidQueryRcvError */

/*----------------------------------------------------------------------------*/
/*! \brief This routine is called to query the number of frames that the NIC
*          cannot receive due to lack of NIC receive buffer space.
*
* \param[in] pvAdapter Pointer to the Adapter structure
* \param[in] pvQueryBuf A pointer to the buffer that holds the result of the
*                          query buffer
* \param[in] u4QueryBufLen The length of the query buffer
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                            bytes written into the query buffer. If the call
*                            failed due to invalid length of the query buffer,
*                            returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS If success;
* \retval WLAN_STATUS_INVALID_LENGTH
* \retval WLAN_STATUS_ADAPTER_NOT_READY
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryRcvNoBuffer(IN P_ADAPTER_T prAdapter,
			IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	DEBUGFUNC("wlanoidQueryRcvNoBuffer");
	DBGLOG(REQ, LOUD, "\n");

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in query receive error! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		*pu4QueryInfoLen = sizeof(UINT_32);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (u4QueryBufferLen < sizeof(UINT_32)
		   || (u4QueryBufferLen > sizeof(UINT_32) && u4QueryBufferLen < sizeof(UINT_64))) {
		*pu4QueryInfoLen = sizeof(UINT_64);
		return WLAN_STATUS_INVALID_LENGTH;
	}
#if CFG_ENABLE_STATISTICS_BUFFERING
	if (IsBufferedStatisticsUsable(prAdapter) == TRUE) {
		if (u4QueryBufferLen == sizeof(UINT_32)) {
			*pu4QueryInfoLen = sizeof(UINT_32);
			*(PUINT_32) pvQueryBuffer = (UINT_32) 0;	/* @FIXME */
		} else {
			*pu4QueryInfoLen = sizeof(UINT_64);
			*(PUINT_64) pvQueryBuffer = (UINT_64) 0;	/* @FIXME */
		}

		return WLAN_STATUS_SUCCESS;
	}
#endif

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_GET_STATISTICS,
				   FALSE,
				   TRUE,
				   TRUE,
				   nicCmdEventQueryRecvNoBuffer,
				   nicOidCmdTimeoutCommon, 0, NULL, pvQueryBuffer, u4QueryBufferLen);

}				/* wlanoidQueryRcvNoBuffer */

/*----------------------------------------------------------------------------*/
/*! \brief This routine is called to query the number of frames that the NIC
*          received and it is CRC error.
*
* \param[in] pvAdapter Pointer to the Adapter structure
* \param[in] pvQueryBuf A pointer to the buffer that holds the result of the
*                          query buffer
* \param[in] u4QueryBufLen The length of the query buffer
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                            bytes written into the query buffer. If the call
*                            failed due to invalid length of the query buffer,
*                            returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS If success;
* \retval WLAN_STATUS_INVALID_LENGTH
* \retval WLAN_STATUS_ADAPTER_NOT_READY
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryRcvCrcError(IN P_ADAPTER_T prAdapter,
			IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	DEBUGFUNC("wlanoidQueryRcvCrcError");
	DBGLOG(REQ, LOUD, "\n");

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in query receive error! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		*pu4QueryInfoLen = sizeof(UINT_32);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (u4QueryBufferLen < sizeof(UINT_32)
		   || (u4QueryBufferLen > sizeof(UINT_32) && u4QueryBufferLen < sizeof(UINT_64))) {
		*pu4QueryInfoLen = sizeof(UINT_64);
		return WLAN_STATUS_INVALID_LENGTH;
	}
#if CFG_ENABLE_STATISTICS_BUFFERING
	if (IsBufferedStatisticsUsable(prAdapter) == TRUE) {
		if (u4QueryBufferLen == sizeof(UINT_32)) {
			*pu4QueryInfoLen = sizeof(UINT_32);
			*(PUINT_32) pvQueryBuffer = (UINT_32) prAdapter->rStatStruct.rFCSErrorCount.QuadPart;
		} else {
			*pu4QueryInfoLen = sizeof(UINT_64);
			*(PUINT_64) pvQueryBuffer = (UINT_64) prAdapter->rStatStruct.rFCSErrorCount.QuadPart;
		}

		return WLAN_STATUS_SUCCESS;
	}
#endif

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_GET_STATISTICS,
				   FALSE,
				   TRUE,
				   TRUE,
				   nicCmdEventQueryRecvCrcError,
				   nicOidCmdTimeoutCommon, 0, NULL, pvQueryBuffer, u4QueryBufferLen);

}				/* wlanoidQueryRcvCrcError */

/*----------------------------------------------------------------------------*/
/*! \brief  This routine is called to query the current 802.11 statistics.
*
* \param[in] pvAdapter Pointer to the Adapter structure
* \param[in] pvQueryBuf A pointer to the buffer that holds the result of the
*                          query buffer
* \param[in] u4QueryBufLen The length of the query buffer
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                            bytes written into the query buffer. If the call
*                            failed due to invalid length of the query buffer,
*                            returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryStatistics(IN P_ADAPTER_T prAdapter,
		       IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	PARAM_802_11_STATISTICS_STRUCT_T  rStatistics;

	DEBUGFUNC("wlanoidQueryStatistics");
	DBGLOG(REQ, LOUD, "\n");

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	*pu4QueryInfoLen = sizeof(PARAM_802_11_STATISTICS_STRUCT_T);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in query receive error! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		*pu4QueryInfoLen = sizeof(UINT_32);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (u4QueryBufferLen < sizeof(PARAM_802_11_STATISTICS_STRUCT_T)) {
		DBGLOG(REQ, WARN, "Too short length %u\n", u4QueryBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}
#if CFG_ENABLE_STATISTICS_BUFFERING
	if (IsBufferedStatisticsUsable(prAdapter) == TRUE) {
		P_PARAM_802_11_STATISTICS_STRUCT_T prStatistics;

		*pu4QueryInfoLen = sizeof(PARAM_802_11_STATISTICS_STRUCT_T);
		prStatistics = (P_PARAM_802_11_STATISTICS_STRUCT_T) pvQueryBuffer;

		prStatistics->u4Length = sizeof(PARAM_802_11_STATISTICS_STRUCT_T);
		prStatistics->rTransmittedFragmentCount = prAdapter->rStatStruct.rTransmittedFragmentCount;
		prStatistics->rMulticastTransmittedFrameCount = prAdapter->rStatStruct.rMulticastTransmittedFrameCount;
		prStatistics->rFailedCount = prAdapter->rStatStruct.rFailedCount;
		prStatistics->rRetryCount = prAdapter->rStatStruct.rRetryCount;
		prStatistics->rMultipleRetryCount = prAdapter->rStatStruct.rMultipleRetryCount;
		prStatistics->rRTSSuccessCount = prAdapter->rStatStruct.rRTSSuccessCount;
		prStatistics->rRTSFailureCount = prAdapter->rStatStruct.rRTSFailureCount;
		prStatistics->rACKFailureCount = prAdapter->rStatStruct.rACKFailureCount;
		prStatistics->rFrameDuplicateCount = prAdapter->rStatStruct.rFrameDuplicateCount;
		prStatistics->rReceivedFragmentCount = prAdapter->rStatStruct.rReceivedFragmentCount;
		prStatistics->rMulticastReceivedFrameCount = prAdapter->rStatStruct.rMulticastReceivedFrameCount;
		prStatistics->rFCSErrorCount = prAdapter->rStatStruct.rFCSErrorCount;
		prStatistics->rTKIPLocalMICFailures.QuadPart = 0;
		prStatistics->rTKIPICVErrors.QuadPart = 0;
		prStatistics->rTKIPCounterMeasuresInvoked.QuadPart = 0;
		prStatistics->rTKIPReplays.QuadPart = 0;
		prStatistics->rCCMPFormatErrors.QuadPart = 0;
		prStatistics->rCCMPReplays.QuadPart = 0;
		prStatistics->rCCMPDecryptErrors.QuadPart = 0;
		prStatistics->rFourWayHandshakeFailures.QuadPart = 0;
		prStatistics->rWEPUndecryptableCount.QuadPart = 0;
		prStatistics->rWEPICVErrorCount.QuadPart = 0;
		prStatistics->rDecryptSuccessCount.QuadPart = 0;
		prStatistics->rDecryptFailureCount.QuadPart = 0;

		return WLAN_STATUS_SUCCESS;
	}
#endif

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_GET_STATISTICS,
				   FALSE,
				   TRUE,
				   g_fgIsOid,
				   nicCmdEventQueryStatistics,
				   nicOidCmdTimeoutCommon,
				   sizeof(PARAM_802_11_STATISTICS_STRUCT_T), (PUINT_8)&rStatistics,
				   pvQueryBuffer, u4QueryBufferLen);

}				/* wlanoidQueryStatistics */

/*----------------------------------------------------------------------------*/
/*! \brief  This routine is called to query current media streaming status.
*
* \param[in] pvAdapter Pointer to the Adapter structure
* \param[in] pvQueryBuf A pointer to the buffer that holds the result of the
*                          query buffer
* \param[in] u4QueryBufLen The length of the query buffer
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                            bytes written into the query buffer. If the call
*                            failed due to invalid length of the query buffer,
*                            returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryMediaStreamMode(IN P_ADAPTER_T prAdapter,
			    IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	DEBUGFUNC("wlanoidQueryMediaStreamMode");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(ENUM_MEDIA_STREAM_MODE);

	if (u4QueryBufferLen < *pu4QueryInfoLen) {
		DBGLOG(REQ, WARN, "Invalid length %u\n", u4QueryBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	*(P_ENUM_MEDIA_STREAM_MODE) pvQueryBuffer =
	    prAdapter->rWlanInfo.eLinkAttr.ucMediaStreamMode == 0 ? ENUM_MEDIA_STREAM_OFF : ENUM_MEDIA_STREAM_ON;

	return WLAN_STATUS_SUCCESS;

}				/* wlanoidQueryMediaStreamMode */

/*----------------------------------------------------------------------------*/
/*! \brief  This routine is called to enter media streaming mode or exit media streaming mode
*
* \param[in] pvAdapter Pointer to the Adapter structure
* \param[in] pvQueryBuf A pointer to the buffer that holds the result of the
*                          query buffer
* \param[in] u4QueryBufLen The length of the query buffer
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                            bytes written into the query buffer. If the call
*                            failed due to invalid length of the query buffer,
*                            returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetMediaStreamMode(IN P_ADAPTER_T prAdapter,
			  IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	ENUM_MEDIA_STREAM_MODE eStreamMode;

	DEBUGFUNC("wlanoidSetMediaStreamMode");

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	if (u4SetBufferLen < sizeof(ENUM_MEDIA_STREAM_MODE)) {
		DBGLOG(REQ, WARN, "Invalid length %u\n", u4SetBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	*pu4SetInfoLen = sizeof(ENUM_MEDIA_STREAM_MODE);

	eStreamMode = *(P_ENUM_MEDIA_STREAM_MODE) pvSetBuffer;

	if (eStreamMode == ENUM_MEDIA_STREAM_OFF)
		prAdapter->rWlanInfo.eLinkAttr.ucMediaStreamMode = 0;
	else
		prAdapter->rWlanInfo.eLinkAttr.ucMediaStreamMode = 1;

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_LINK_ATTRIB,
				   TRUE,
				   FALSE,
				   TRUE,
				   nicCmdEventSetMediaStreamMode,
				   nicOidCmdTimeoutCommon,
				   sizeof(CMD_LINK_ATTRIB),
				   (PUINT_8) &(prAdapter->rWlanInfo.eLinkAttr), pvSetBuffer, u4SetBufferLen);
}				/* wlanoidSetMediaStreamMode */

/*----------------------------------------------------------------------------*/
/*! \brief  This routine is called to query the permanent MAC address of the NIC.
*
* \param[in] pvAdapter Pointer to the Adapter structure
* \param[in] pvQueryBuf A pointer to the buffer that holds the result of the
*                          query buffer
* \param[in] u4QueryBufLen The length of the query buffer
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                            bytes written into the query buffer. If the call
*                            failed due to invalid length of the query buffer,
*                            returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryPermanentAddr(IN P_ADAPTER_T prAdapter,
			  IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	DEBUGFUNC("wlanoidQueryPermanentAddr");
	DBGLOG(INIT, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (u4QueryBufferLen < MAC_ADDR_LEN)
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	COPY_MAC_ADDR(pvQueryBuffer, prAdapter->rWifiVar.aucPermanentAddress);
	*pu4QueryInfoLen = MAC_ADDR_LEN;

	return WLAN_STATUS_SUCCESS;
}				/* wlanoidQueryPermanentAddr */

/*----------------------------------------------------------------------------*/
/*! \brief  This routine is called to query the MAC address the NIC is currently using.
*
* \param[in] pvAdapter Pointer to the Adapter structure
* \param[in] pvQueryBuf A pointer to the buffer that holds the result of the
*                          query buffer
* \param[in] u4QueryBufLen The length of the query buffer
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                            bytes written into the query buffer. If the call
*                            failed due to invalid length of the query buffer,
*                            returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_BUFFER_TOO_SHORT
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryCurrentAddr(IN P_ADAPTER_T prAdapter,
			IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	DEBUGFUNC("wlanoidQueryCurrentAddr");
	DBGLOG(INIT, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (u4QueryBufferLen < MAC_ADDR_LEN)
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	COPY_MAC_ADDR(pvQueryBuffer, prAdapter->rWifiVar.aucMacAddress);
	*pu4QueryInfoLen = MAC_ADDR_LEN;

	return WLAN_STATUS_SUCCESS;
}				/* wlanoidQueryCurrentAddr */

/*----------------------------------------------------------------------------*/
/*! \brief  This routine is called to query NIC link speed.
*
* \param[in] pvAdapter Pointer to the Adapter structure
* \param[in] pvQueryBuf A pointer to the buffer that holds the result of the
*                          query buffer
* \param[in] u4QueryBufLen The length of the query buffer
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                            bytes written into the query buffer. If the call
*                            failed due to invalid length of the query buffer,
*                            returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_BUFFER_TOO_SHORT
*
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryLinkSpeed(IN P_ADAPTER_T prAdapter,
		      IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	DEBUGFUNC("wlanoidQueryLinkSpeed");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (prAdapter->fgIsEnableLpdvt)
		return WLAN_STATUS_NOT_SUPPORTED;

	*pu4QueryInfoLen = sizeof(UINT_32);

	if (u4QueryBufferLen < sizeof(UINT_32))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) != PARAM_MEDIA_STATE_CONNECTED) {
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (prAdapter->fgIsLinkRateValid == TRUE &&
		   (kalGetTimeTick() - prAdapter->rLinkRateUpdateTime) <= CFG_LINK_QUALITY_VALID_PERIOD) {
		*(PUINT_32) pvQueryBuffer = prAdapter->rLinkQuality.u2LinkSpeed * 5000;	/* change to unit of 100bps */
		return WLAN_STATUS_SUCCESS;
	} else {
		return wlanSendSetQueryCmd(prAdapter,
					   CMD_ID_GET_LINK_QUALITY,
					   FALSE,
					   TRUE,
					   g_fgIsOid,
					   nicCmdEventQueryLinkSpeed,
					   nicOidCmdTimeoutCommon, 0, NULL, pvQueryBuffer, u4QueryBufferLen);
	}
}				/* end of wlanoidQueryLinkSpeed() */

#if CFG_SUPPORT_QA_TOOL
#if CFG_SUPPORT_BUFFER_MODE
WLAN_STATUS
wlanoidSetEfusBufferMode(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen,
			 OUT PUINT_32 pu4SetInfoLen)
{
	P_PARAM_CUSTOM_EFUSE_BUFFER_MODE_T prSetEfuseBufModeInfo;
	CMD_EFUSE_BUFFER_MODE_T *prCmdSetEfuseBufModeInfo = NULL;
	PFN_CMD_DONE_HANDLER pfCmdDoneHandler;
	UINT_32 u4EfuseContentSize, u4QueryInfoLen;
	BOOLEAN fgSetQuery, fgNeedResp;
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidSetEfusBufferMode");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	/* get the buffer mode info */
	prSetEfuseBufModeInfo = (P_PARAM_CUSTOM_EFUSE_BUFFER_MODE_T) pvSetBuffer;

	/* copy command header */
	prCmdSetEfuseBufModeInfo =
	(CMD_EFUSE_BUFFER_MODE_T *) kalMemAlloc(sizeof(CMD_EFUSE_BUFFER_MODE_T), VIR_MEM_TYPE);
	if (prCmdSetEfuseBufModeInfo == NULL)
		return WLAN_STATUS_FAILURE;
	kalMemZero(prCmdSetEfuseBufModeInfo, sizeof(CMD_EFUSE_BUFFER_MODE_T));
	prCmdSetEfuseBufModeInfo->ucSourceMode = prSetEfuseBufModeInfo->ucSourceMode;
	prCmdSetEfuseBufModeInfo->ucCount      = prSetEfuseBufModeInfo->ucCount;
	prCmdSetEfuseBufModeInfo->ucCmdType    = prSetEfuseBufModeInfo->ucCmdType;
	prCmdSetEfuseBufModeInfo->ucReserved   = prSetEfuseBufModeInfo->ucReserved;

	/* decide content size and SetQuery / NeedResp flag */
	if (prAdapter->fgIsSupportBufferBinSize16Byte == TRUE) {
		u4EfuseContentSize  = sizeof(BIN_CONTENT_T) * EFUSE_CONTENT_SIZE;
		pfCmdDoneHandler = nicCmdEventSetCommon;
		fgSetQuery = TRUE;
		fgNeedResp = FALSE;
	} else {
#if (CFG_FW_Report_Efuse_Address == 1)
		u4EfuseContentSize = (prAdapter->u4EfuseEndAddress) - (prAdapter->u4EfuseStartAddress) + 1;
#else
		u4EfuseContentSize = EFUSE_CONTENT_BUFFER_SIZE;
#endif
		pfCmdDoneHandler = NULL;
		fgSetQuery = FALSE;
		fgNeedResp = TRUE;
	}

	u4QueryInfoLen = OFFSET_OF(CMD_EFUSE_BUFFER_MODE_T, aBinContent) + u4EfuseContentSize;

	if (u4SetBufferLen < u4QueryInfoLen) {
		kalMemFree(prCmdSetEfuseBufModeInfo, VIR_MEM_TYPE, sizeof(CMD_EFUSE_BUFFER_MODE_T));
		return WLAN_STATUS_INVALID_LENGTH;
	}

	*pu4SetInfoLen = u4QueryInfoLen;
	kalMemCopy(prCmdSetEfuseBufModeInfo->aBinContent, prSetEfuseBufModeInfo->aBinContent,
		   u4EfuseContentSize);

	rWlanStatus = wlanSendSetQueryExtCmd(prAdapter,
						CMD_ID_LAYER_0_EXT_MAGIC_NUM,
						EXT_CMD_ID_EFUSE_BUFFER_MODE,
						fgSetQuery,
						fgNeedResp,
						g_fgIsOid,
						pfCmdDoneHandler,
						nicOidCmdTimeoutCommon,
						u4QueryInfoLen,
						(PUINT_8) (prCmdSetEfuseBufModeInfo),
						pvSetBuffer, u4SetBufferLen);

	kalMemFree(prCmdSetEfuseBufModeInfo, VIR_MEM_TYPE, sizeof(CMD_EFUSE_BUFFER_MODE_T));

	return rWlanStatus;
}

/*#if (CFG_EEPROM_PAGE_ACCESS == 1)*/
/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to read efuse content.
*
* \param[in] pvAdapter Pointer to the Adapter structure.
* \param[out] pvQueryBuf A pointer to the buffer that holds the result of
*                           the query.
* \param[in] u4QueryBufLen The length of the query buffer.
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                            bytes written into the query buffer. If the call
*                            failed due to invalid length of the query buffer,
*                            returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryProcessAccessEfuseRead(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen,
			 OUT PUINT_32 pu4SetInfoLen)
{
	P_PARAM_CUSTOM_ACCESS_EFUSE_T prSetAccessEfuseInfo;
	CMD_ACCESS_EFUSE_T rCmdSetAccessEfuse;
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidQueryProcessAccessEfuseRead");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(PARAM_CUSTOM_ACCESS_EFUSE_T);

	if (u4SetBufferLen < sizeof(PARAM_CUSTOM_ACCESS_EFUSE_T))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prSetAccessEfuseInfo = (P_PARAM_CUSTOM_ACCESS_EFUSE_T) pvSetBuffer;

	kalMemSet(&rCmdSetAccessEfuse, 0, sizeof(CMD_ACCESS_EFUSE_T));

	rCmdSetAccessEfuse.u4Address = prSetAccessEfuseInfo->u4Address;
	rCmdSetAccessEfuse.u4Valid = prSetAccessEfuseInfo->u4Valid;


	DBGLOG(INIT, INFO, "MT6632 : wlanoidQueryProcessAccessEfuseRead, address=%d\n", rCmdSetAccessEfuse.u4Address);

	kalMemCopy(rCmdSetAccessEfuse.aucData, prSetAccessEfuseInfo->aucData,
	       sizeof(UINT_8) * 16);

	rWlanStatus = wlanSendSetQueryExtCmd(prAdapter,
					CMD_ID_LAYER_0_EXT_MAGIC_NUM,
					EXT_CMD_ID_EFUSE_ACCESS,
					FALSE,   /* Query Bit:  True->write  False->read*/
					TRUE,
					g_fgIsOid,
					NULL, /* No Tx done function wait until fw ack */
					nicOidCmdTimeoutCommon,
					sizeof(CMD_ACCESS_EFUSE_T),
					(PUINT_8) (&rCmdSetAccessEfuse), pvSetBuffer, u4SetBufferLen);

	return rWlanStatus;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to write efuse content.
*
* \param[in] pvAdapter Pointer to the Adapter structure.
* \param[out] pvQueryBuf A pointer to the buffer that holds the result of
*                           the query.
* \param[in] u4QueryBufLen The length of the query buffer.
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                            bytes written into the query buffer. If the call
*                            failed due to invalid length of the query buffer,
*                            returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryProcessAccessEfuseWrite(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen,
			 OUT PUINT_32 pu4SetInfoLen)
{
	P_PARAM_CUSTOM_ACCESS_EFUSE_T prSetAccessEfuseInfo;
	CMD_ACCESS_EFUSE_T rCmdSetAccessEfuse;
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidQueryProcessAccessEfuseWrite");
	DBGLOG(INIT, INFO, "MT6632 : wlanoidQueryProcessAccessEfuseWrite\n");


	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(PARAM_CUSTOM_ACCESS_EFUSE_T);

	if (u4SetBufferLen < sizeof(PARAM_CUSTOM_ACCESS_EFUSE_T))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prSetAccessEfuseInfo = (P_PARAM_CUSTOM_ACCESS_EFUSE_T) pvSetBuffer;

	kalMemSet(&rCmdSetAccessEfuse, 0, sizeof(CMD_ACCESS_EFUSE_T));

	rCmdSetAccessEfuse.u4Address = prSetAccessEfuseInfo->u4Address;
	rCmdSetAccessEfuse.u4Valid = prSetAccessEfuseInfo->u4Valid;

	DBGLOG(INIT, INFO, "MT6632 : wlanoidQueryProcessAccessEfuseWrite, address=%d\n", rCmdSetAccessEfuse.u4Address);


	kalMemCopy(rCmdSetAccessEfuse.aucData, prSetAccessEfuseInfo->aucData,
		sizeof(UINT_8) * 16);

	rWlanStatus = wlanSendSetQueryExtCmd(prAdapter,
					CMD_ID_LAYER_0_EXT_MAGIC_NUM,
					EXT_CMD_ID_EFUSE_ACCESS,
					TRUE,   /* Query Bit:  True->write  False->read*/
					TRUE,
					g_fgIsOid,
					NULL, /* No Tx done function wait until fw ack */
					nicOidCmdTimeoutCommon,
					sizeof(CMD_ACCESS_EFUSE_T),
					(PUINT_8) (&rCmdSetAccessEfuse), pvSetBuffer, u4SetBufferLen);

	return rWlanStatus;
}




WLAN_STATUS
wlanoidQueryEfuseFreeBlock(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen,
			 OUT PUINT_32 pu4SetInfoLen)
{
	P_PARAM_CUSTOM_EFUSE_FREE_BLOCK_T prGetEfuseFreeBlockInfo;
	CMD_EFUSE_FREE_BLOCK_T rCmdGetEfuseFreeBlock;
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidQueryEfuseFreeBlock");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(PARAM_CUSTOM_EFUSE_FREE_BLOCK_T);

	if (u4SetBufferLen < sizeof(PARAM_CUSTOM_EFUSE_FREE_BLOCK_T))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prGetEfuseFreeBlockInfo = (P_PARAM_CUSTOM_EFUSE_FREE_BLOCK_T) pvSetBuffer;

	kalMemSet(&rCmdGetEfuseFreeBlock, 0, sizeof(CMD_EFUSE_FREE_BLOCK_T));


	rWlanStatus = wlanSendSetQueryExtCmd(prAdapter,
					CMD_ID_LAYER_0_EXT_MAGIC_NUM,
					EXT_CMD_ID_EFUSE_FREE_BLOCK,
					FALSE,   /* Query Bit:  True->write  False->read*/
					TRUE,
					g_fgIsOid,
					NULL, /* No Tx done function wait until fw ack */
					nicOidCmdTimeoutCommon,
					sizeof(CMD_EFUSE_FREE_BLOCK_T),
					(PUINT_8) (&rCmdGetEfuseFreeBlock), pvSetBuffer, u4SetBufferLen);

	return rWlanStatus;
}

WLAN_STATUS
wlanoidQueryGetTxPower(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen,
			 OUT PUINT_32 pu4SetInfoLen)
{
	P_PARAM_CUSTOM_GET_TX_POWER_T prGetTxPowerInfo;
	CMD_GET_TX_POWER_T rCmdGetTxPower;
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidQueryGetTxPower");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(P_PARAM_CUSTOM_GET_TX_POWER_T);

	if (u4SetBufferLen < sizeof(P_PARAM_CUSTOM_GET_TX_POWER_T))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prGetTxPowerInfo = (P_PARAM_CUSTOM_GET_TX_POWER_T) pvSetBuffer;

	kalMemSet(&rCmdGetTxPower, 0, sizeof(CMD_GET_TX_POWER_T));

	rCmdGetTxPower.ucTxPwrType = EXT_EVENT_TARGET_TX_POWER;
	rCmdGetTxPower.ucCenterChannel = prGetTxPowerInfo->ucCenterChannel;
	rCmdGetTxPower.ucDbdcIdx = prGetTxPowerInfo->ucDbdcIdx;
	rCmdGetTxPower.ucBand = prGetTxPowerInfo->ucBand;


	rWlanStatus = wlanSendSetQueryExtCmd(prAdapter,
					CMD_ID_LAYER_0_EXT_MAGIC_NUM,
					EXT_CMD_ID_GET_TX_POWER,
					FALSE,   /* Query Bit:  True->write  False->read*/
					TRUE,
					g_fgIsOid,
					NULL, /* No Tx done function wait until fw ack */
					nicOidCmdTimeoutCommon,
					sizeof(CMD_GET_TX_POWER_T),
					(PUINT_8) (&rCmdGetTxPower), pvSetBuffer, u4SetBufferLen);

	return rWlanStatus;
}


/*#endif*/

#endif /* CFG_SUPPORT_BUFFER_MODE */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query RX statistics.
*
* \param[in] pvAdapter Pointer to the Adapter structure.
* \param[out] pvQueryBuf A pointer to the buffer that holds the result of
*                           the query.
* \param[in] u4QueryBufLen The length of the query buffer.
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                            bytes written into the query buffer. If the call
*                            failed due to invalid length of the query buffer,
*                            returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryRxStatistics(IN P_ADAPTER_T prAdapter,
			 IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	P_PARAM_CUSTOM_ACCESS_RX_STAT prRxStatistics;
	P_CMD_ACCESS_RX_STAT prCmdAccessRxStat;
	CMD_ACCESS_RX_STAT rCmdAccessRxStat;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
/*	UINT_32 u4MemSize = PARAM_MEM_DUMP_MAX_SIZE; */
	UINT_32 u4SeqNum = 0;
	UINT_32 u4TotalNum = 0;

	prCmdAccessRxStat = &rCmdAccessRxStat;

	DEBUGFUNC("wlanoidQueryRxStatistics");
	DBGLOG(INIT, LOUD, "\n");

	DBGLOG(INIT, ERROR, "MT6632 : wlanoidQueryRxStatistics\n");

	prRxStatistics = (P_PARAM_CUSTOM_ACCESS_RX_STAT) pvQueryBuffer;

	*pu4QueryInfoLen = 8 + prRxStatistics->u4TotalNum;

	u4SeqNum = prRxStatistics->u4SeqNum;
	u4TotalNum = prRxStatistics->u4TotalNum;

	do {
		prCmdAccessRxStat->u4SeqNum = u4SeqNum;
		prCmdAccessRxStat->u4TotalNum = u4TotalNum;

		rStatus = wlanSendSetQueryCmd(prAdapter,
					      CMD_ID_ACCESS_RX_STAT,
					      FALSE,
					      TRUE,
					      g_fgIsOid,
					      nicCmdEventQueryRxStatistics,
					      nicOidCmdTimeoutCommon,
					      sizeof(CMD_ACCESS_RX_STAT),
					      (PUINT_8) prCmdAccessRxStat, pvQueryBuffer, u4QueryBufferLen);
	} while (FALSE);

	return rStatus;
}

#if CFG_SUPPORT_TX_BF

WLAN_STATUS
wlanoidStaRecUpdate(IN P_ADAPTER_T prAdapter,
		    IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_CMD_STAREC_UPDATE_T prStaRecUpdateInfo;
	P_CMD_STAREC_COMMON_T prStaRecCmm;
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidStaRecUpdate");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(CMD_STAREC_COMMON_T);
	if (u4SetBufferLen < sizeof(CMD_STAREC_COMMON_T))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prStaRecUpdateInfo =
	    (P_CMD_STAREC_UPDATE_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, (CMD_STAREC_UPDATE_HDR_SIZE + u4SetBufferLen));
	if (!prStaRecUpdateInfo) {
		DBGLOG(INIT, ERROR, "Allocate P_CMD_DEV_INFO_UPDATE_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}

	/* fix me: configurable ucBssIndex */
	prStaRecCmm = (P_CMD_STAREC_COMMON_T) pvSetBuffer;
	prStaRecUpdateInfo->ucBssIndex = 0;
	prStaRecUpdateInfo->ucWlanIdx = prStaRecCmm->u2Reserve1;
	prStaRecUpdateInfo->u2TotalElementNum = 1;
	kalMemCopy(prStaRecUpdateInfo->aucBuffer, pvSetBuffer, u4SetBufferLen);

	rWlanStatus = wlanSendSetQueryExtCmd(prAdapter,
					     CMD_ID_LAYER_0_EXT_MAGIC_NUM,
					     EXT_CMD_ID_STAREC_UPDATE,
					     TRUE,
					     FALSE,
					     g_fgIsOid,
					     nicCmdEventSetCommon,
					     nicOidCmdTimeoutCommon,
					     (CMD_STAREC_UPDATE_HDR_SIZE + u4SetBufferLen),
					     (PUINT_8) prStaRecUpdateInfo, NULL, 0);

	cnmMemFree(prAdapter, prStaRecUpdateInfo);

	return rWlanStatus;
}

WLAN_STATUS
wlanoidStaRecBFUpdate(IN P_ADAPTER_T prAdapter,
		      IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_CMD_STAREC_UPDATE_T prStaRecUpdateInfo;
	P_CMD_STAREC_BF prStaRecBF;
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidStaRecBFUpdate");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(CMD_STAREC_BF);
	if (u4SetBufferLen < sizeof(CMD_STAREC_BF))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prStaRecUpdateInfo =
	    (P_CMD_STAREC_UPDATE_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, (CMD_STAREC_UPDATE_HDR_SIZE + u4SetBufferLen));
	if (!prStaRecUpdateInfo) {
		DBGLOG(INIT, ERROR, "Allocate P_CMD_DEV_INFO_UPDATE_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}

	/* fix me: configurable ucBssIndex */
	prStaRecBF = (P_CMD_STAREC_BF) pvSetBuffer;
	prStaRecUpdateInfo->ucBssIndex = prStaRecBF->ucReserved[0];
	prStaRecUpdateInfo->ucWlanIdx = prStaRecBF->ucReserved[1];
	prStaRecUpdateInfo->u2TotalElementNum = 1;
	kalMemCopy(prStaRecUpdateInfo->aucBuffer, pvSetBuffer, u4SetBufferLen);

	rWlanStatus = wlanSendSetQueryExtCmd(prAdapter,
					     CMD_ID_LAYER_0_EXT_MAGIC_NUM,
					     EXT_CMD_ID_STAREC_UPDATE,
					     TRUE,
					     FALSE,
					     g_fgIsOid,
					     nicCmdEventSetCommon,
					     nicOidCmdTimeoutCommon,
					     (CMD_STAREC_UPDATE_HDR_SIZE + u4SetBufferLen),
					     (PUINT_8) prStaRecUpdateInfo, NULL, 0);

	cnmMemFree(prAdapter, prStaRecUpdateInfo);

	return rWlanStatus;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief extend command packet generation utility
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[in] ucCID Command ID
* \param[in] ucExtCID Extend command ID
* \param[in] fgSetQuery Set or Query
* \param[in] fgNeedResp Need for response
* \param[in] pfCmdDoneHandler Function pointer when command is done
* \param[in] u4SetQueryInfoLen The length of the set/query buffer
* \param[in] pucInfoBuffer Pointer to set/query buffer
*
*
* \retval WLAN_STATUS_PENDING
* \retval WLAN_STATUS_FAILURE
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanSendSetQueryExtCmd(IN P_ADAPTER_T prAdapter,
		       UINT_8 ucCID,
		       UINT_8 ucExtCID,
		       BOOLEAN fgSetQuery,
		       BOOLEAN fgNeedResp,
		       BOOLEAN fgIsOid,
		       PFN_CMD_DONE_HANDLER pfCmdDoneHandler,
		       PFN_CMD_TIMEOUT_HANDLER pfCmdTimeoutHandler,
		       UINT_32 u4SetQueryInfoLen,
		       PUINT_8 pucInfoBuffer, OUT PVOID pvSetQueryBuffer, IN UINT_32 u4SetQueryBufferLen)
{
	P_GLUE_INFO_T prGlueInfo;
	P_CMD_INFO_T prCmdInfo;
	P_WIFI_CMD_T prWifiCmd;
	UINT_8 ucCmdSeqNum;

	prGlueInfo = prAdapter->prGlueInfo;
	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, (CMD_HDR_SIZE + u4SetQueryInfoLen));

	DEBUGFUNC("wlanSendSetQueryCmd");

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
	prWifiCmd->u2PQ_ID = CMD_PQ_ID;
	prWifiCmd->u2Length = prCmdInfo->u2InfoBufLen - (UINT_16) OFFSET_OF(WIFI_CMD_T, u2Length);
	prWifiCmd->u2PqId = CMD_PQ_ID;
	prWifiCmd->ucPktTypeID = CMD_PACKET_TYPE_ID;
	prWifiCmd->ucCID = prCmdInfo->ucCID;
	prWifiCmd->ucExtenCID = ucExtCID;
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

WLAN_STATUS
wlanoidBssInfoBasic(IN P_ADAPTER_T prAdapter,
		    IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_CMD_BSS_INFO_UPDATE_T prBssInfoUpdateBasic;
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidManualAssoc");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(CMD_BSSINFO_BASIC_T);
	if (u4SetBufferLen < sizeof(CMD_BSSINFO_BASIC_T))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prBssInfoUpdateBasic = cnmMemAlloc(prAdapter, RAM_TYPE_MSG, (CMD_BSSINFO_UPDATE_HDR_SIZE + u4SetBufferLen));
	if (!prBssInfoUpdateBasic) {
		DBGLOG(INIT, ERROR, "Allocate P_CMD_DEV_INFO_UPDATE_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}

	/* fix me: configurable ucBssIndex */
	prBssInfoUpdateBasic->ucBssIndex = 0;
	prBssInfoUpdateBasic->u2TotalElementNum = 1;
	kalMemCopy(prBssInfoUpdateBasic->aucBuffer, pvSetBuffer, u4SetBufferLen);

	rWlanStatus = wlanSendSetQueryExtCmd(prAdapter,
					     CMD_ID_LAYER_0_EXT_MAGIC_NUM,
					     EXT_CMD_ID_BSSINFO_UPDATE,
					     TRUE,
					     FALSE,
					     g_fgIsOid,
					     nicCmdEventSetCommon,
					     nicOidCmdTimeoutCommon,
					     (CMD_BSSINFO_UPDATE_HDR_SIZE + u4SetBufferLen),
					     (PUINT_8) prBssInfoUpdateBasic, NULL, 0);

	cnmMemFree(prAdapter, prBssInfoUpdateBasic);

	return rWlanStatus;
}

WLAN_STATUS
wlanoidDevInfoActive(IN P_ADAPTER_T prAdapter,
		     IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_CMD_DEV_INFO_UPDATE_T prDevInfoUpdateActive;
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidManualAssoc");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(CMD_DEVINFO_ACTIVE_T);
	if (u4SetBufferLen < sizeof(CMD_DEVINFO_ACTIVE_T))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prDevInfoUpdateActive = cnmMemAlloc(prAdapter, RAM_TYPE_MSG, (CMD_DEVINFO_UPDATE_HDR_SIZE + u4SetBufferLen));
	if (!prDevInfoUpdateActive) {
		DBGLOG(INIT, ERROR, "Allocate P_CMD_DEV_INFO_UPDATE_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}

	/* fix me: configurable ucOwnMacIdx */
	prDevInfoUpdateActive->ucOwnMacIdx = 0;
	prDevInfoUpdateActive->ucAppendCmdTLV = 0;
	prDevInfoUpdateActive->u2TotalElementNum = 1;
	kalMemCopy(prDevInfoUpdateActive->aucBuffer, pvSetBuffer, u4SetBufferLen);

	rWlanStatus = wlanSendSetQueryExtCmd(prAdapter,
					     CMD_ID_LAYER_0_EXT_MAGIC_NUM,
					     EXT_CMD_ID_DEVINFO_UPDATE,
					     TRUE,
					     FALSE,
					     g_fgIsOid,
					     nicCmdEventSetCommon,
					     nicOidCmdTimeoutCommon,
					     (CMD_DEVINFO_UPDATE_HDR_SIZE + u4SetBufferLen),
					     (PUINT_8) prDevInfoUpdateActive, NULL, 0);

	cnmMemFree(prAdapter, prDevInfoUpdateActive);

	return rWlanStatus;
}

WLAN_STATUS
wlanoidManualAssoc(IN P_ADAPTER_T prAdapter,
		   IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_CMD_STAREC_UPDATE_T prStaRecManualAssoc;
	P_CMD_MANUAL_ASSOC_STRUCT_T prManualAssoc;
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidManualAssoc");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(CMD_STAREC_UPDATE_T);
	if (u4SetBufferLen < sizeof(CMD_STAREC_UPDATE_T))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prStaRecManualAssoc = cnmMemAlloc(prAdapter, RAM_TYPE_MSG, (CMD_STAREC_UPDATE_HDR_SIZE + u4SetBufferLen));
	if (!prStaRecManualAssoc) {
		DBGLOG(INIT, ERROR, "Allocate P_CMD_STAREC_UPDATE_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}

	prManualAssoc = (P_CMD_MANUAL_ASSOC_STRUCT_T) pvSetBuffer;
	prStaRecManualAssoc->ucWlanIdx = prManualAssoc->ucWtbl;
	prStaRecManualAssoc->ucBssIndex = prManualAssoc->ucOwnmac;
	prStaRecManualAssoc->u2TotalElementNum = 1;
	kalMemCopy(prStaRecManualAssoc->aucBuffer, pvSetBuffer, u4SetBufferLen);

	rWlanStatus = wlanSendSetQueryExtCmd(prAdapter,
					     CMD_ID_LAYER_0_EXT_MAGIC_NUM,
					     EXT_CMD_ID_STAREC_UPDATE,
					     TRUE,
					     FALSE,
					     g_fgIsOid,
					     nicCmdEventSetCommon,
					     nicOidCmdTimeoutCommon,
					     (CMD_STAREC_UPDATE_HDR_SIZE + u4SetBufferLen),
					     (PUINT_8) prStaRecManualAssoc, NULL, 0);

	cnmMemFree(prAdapter, prStaRecManualAssoc);

	return rWlanStatus;
}

typedef struct _TXBF_CMD_DONE_HANDLER_T {
	UINT_32 u4TxBfCmdId;
	 VOID (*pFunc)(P_ADAPTER_T, P_CMD_INFO_T, PUINT_8);
} TXBF_CMD_DONE_HANDLER_T, *P_TXBF_CMD_DONE_HANDLER_T;

TXBF_CMD_DONE_HANDLER_T rTxBfCmdDoneHandler[] = {
	{BF_SOUNDING_OFF, nicCmdEventSetCommon},
	{BF_SOUNDING_ON, nicCmdEventSetCommon},
	{BF_HW_CTRL, nicCmdEventSetCommon},
	{BF_DATA_PACKET_APPLY, nicCmdEventSetCommon},
	{BF_PFMU_MEM_ALLOCATE, nicCmdEventSetCommon},
	{BF_PFMU_MEM_RELEASE, nicCmdEventSetCommon},
	{BF_PFMU_TAG_READ, nicCmdEventPfmuTagRead},
	{BF_PFMU_TAG_WRITE, nicCmdEventSetCommon},
	{BF_PROFILE_READ, nicCmdEventPfmuDataRead},
	{BF_PROFILE_WRITE, nicCmdEventSetCommon},
	{BF_PN_READ, nicCmdEventSetCommon},
	{BF_PN_WRITE, nicCmdEventSetCommon},
	{BF_PFMU_MEM_ALLOC_MAP_READ, nicCmdEventSetCommon}
};

WLAN_STATUS
wlanoidTxBfAction(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_PARAM_CUSTOM_TXBF_ACTION_STRUCT_T prTxBfActionInfo;
	CMD_TXBF_ACTION_T rCmdTxBfActionInfo;
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;
	BOOLEAN fgSetQuery, fgNeedResp;
	UINT_32 u4TxBfCmdId;

	DEBUGFUNC("wlanoidTxBfAction");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(PARAM_CUSTOM_TXBF_ACTION_STRUCT_T);

	if (u4SetBufferLen < sizeof(PARAM_CUSTOM_TXBF_ACTION_STRUCT_T))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prTxBfActionInfo = (P_PARAM_CUSTOM_TXBF_ACTION_STRUCT_T) pvSetBuffer;

	memcpy(&rCmdTxBfActionInfo, prTxBfActionInfo, sizeof(CMD_TXBF_ACTION_T));

	u4TxBfCmdId = rCmdTxBfActionInfo.rProfileTagRead.ucTxBfCategory;
	if (TXBF_CMD_NEED_TO_RESPONSE(u4TxBfCmdId) == 0) {	/* don't need response */
		fgSetQuery = TRUE;
		fgNeedResp = FALSE;
	} else {
		fgSetQuery = FALSE;
		fgNeedResp = TRUE;
	}

	rWlanStatus = wlanSendSetQueryExtCmd(prAdapter,
					     CMD_ID_LAYER_0_EXT_MAGIC_NUM,
					     EXT_CMD_ID_BF_ACTION,
					     fgSetQuery,
					     fgNeedResp,
					     g_fgIsOid,
					     rTxBfCmdDoneHandler[u4TxBfCmdId].pFunc,
					     nicOidCmdTimeoutCommon,
					     sizeof(CMD_TXBF_ACTION_T),
					     (PUINT_8) &rCmdTxBfActionInfo, pvSetBuffer, u4SetBufferLen);

	return rWlanStatus;
}

#if CFG_SUPPORT_MU_MIMO
WLAN_STATUS
wlanoidMuMimoAction(IN P_ADAPTER_T prAdapter,
		    IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_PARAM_CUSTOM_MUMIMO_ACTION_STRUCT_T prMuMimoActionInfo;
	CMD_MUMIMO_ACTION_T rCmdMuMimoActionInfo;
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;
	BOOLEAN fgSetQuery, fgNeedResp;
	UINT_32 u4MuMimoCmdId;
	VOID (*pFunc)(P_ADAPTER_T, P_CMD_INFO_T, PUINT_8);

	DEBUGFUNC("wlanoidMuMimoAction");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(PARAM_CUSTOM_MUMIMO_ACTION_STRUCT_T);

	if (u4SetBufferLen < sizeof(PARAM_CUSTOM_MUMIMO_ACTION_STRUCT_T))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prMuMimoActionInfo = (P_PARAM_CUSTOM_MUMIMO_ACTION_STRUCT_T) pvSetBuffer;

	memcpy(&rCmdMuMimoActionInfo, prMuMimoActionInfo, sizeof(CMD_MUMIMO_ACTION_T));

	u4MuMimoCmdId = rCmdMuMimoActionInfo.ucMuMimoCategory;
	if (MU_CMD_NEED_TO_RESPONSE(u4MuMimoCmdId) == 0) {
		fgSetQuery = TRUE;
		fgNeedResp = FALSE;
	} else {
		fgSetQuery = FALSE;
		fgNeedResp = TRUE;
	}

	pFunc = nicCmdEventSetCommon;
	if (u4MuMimoCmdId == MU_HQA_GET_QD)
		pFunc = nicCmdEventGetQd;
	else if (u4MuMimoCmdId == MU_HQA_GET_CALC_LQ)
		pFunc = nicCmdEventGetCalcLq;
	else if (u4MuMimoCmdId == MU_GET_CALC_INIT_MCS)
		pFunc = nicCmdEventGetCalcInitMcs;

	rWlanStatus = wlanSendSetQueryExtCmd(prAdapter,
					     CMD_ID_LAYER_0_EXT_MAGIC_NUM,
					     EXT_CMD_ID_MU_CTRL,
					     fgSetQuery,
					     fgNeedResp,
					     g_fgIsOid,
					     pFunc,
					     nicOidCmdTimeoutCommon,
					     sizeof(CMD_MUMIMO_ACTION_T),
					     (PUINT_8) &rCmdMuMimoActionInfo, pvSetBuffer, u4SetBufferLen);

	return rWlanStatus;
}
#endif /* CFG_SUPPORT_MU_MIMO */
#endif /* CFG_SUPPORT_TX_BF */
#endif /* CFG_SUPPORT_QA_TOOL */

#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST
/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to Trigger FW Cal for Backup Cal Data to Host Side.
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[in] pvSetBuffer A pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                           bytes read from the set buffer. If the call failed
*                           due to invalid length of the set buffer, returns
*                           the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetCalBackup(IN P_ADAPTER_T prAdapter,
		   IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	WLAN_STATUS							rWlanStatus = WLAN_STATUS_SUCCESS;
	P_PARAM_CAL_BACKUP_STRUCT_V2_T		prCalBackupDataV2Info;

	DBGLOG(RFTEST, INFO, "%s\n", __func__);

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(PARAM_CAL_BACKUP_STRUCT_V2_T);

	if (u4SetBufferLen < sizeof(PARAM_CAL_BACKUP_STRUCT_V2_T))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prCalBackupDataV2Info = (P_PARAM_CAL_BACKUP_STRUCT_V2_T) pvSetBuffer;

	if (prCalBackupDataV2Info->ucReason == 1 && prCalBackupDataV2Info->ucAction == 2) {
		/* Trigger All Cal Function */
		return wlanoidSendCalBackupV2Cmd(prAdapter, pvSetBuffer, u4SetBufferLen);
	} else if (prCalBackupDataV2Info->ucReason == 4 && prCalBackupDataV2Info->ucAction == 6) {
		/* For Debug Use, Tell FW Print Cal Data (Rom or Ram) */
		return wlanoidSendCalBackupV2Cmd(prAdapter, pvSetBuffer, u4SetBufferLen);
	} else if (prCalBackupDataV2Info->ucReason == 3 && prCalBackupDataV2Info->ucAction == 5) {
		/* Send Cal Data to FW */
		if (prCalBackupDataV2Info->ucRomRam == 0)
			prCalBackupDataV2Info->u4RemainLength = g_rBackupCalDataAllV2.u4ValidRomCalDataLength;
		else if (prCalBackupDataV2Info->ucRomRam == 1)
			prCalBackupDataV2Info->u4RemainLength = g_rBackupCalDataAllV2.u4ValidRamCalDataLength;

		return wlanoidSendCalBackupV2Cmd(prAdapter, pvSetBuffer, u4SetBufferLen);
	}

	return rWlanStatus;
}

WLAN_STATUS wlanoidSendCalBackupV2Cmd(IN P_ADAPTER_T prAdapter, IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen)
{
	WLAN_STATUS							rWlanStatus = WLAN_STATUS_SUCCESS;
	P_PARAM_CAL_BACKUP_STRUCT_V2_T		prCalBackupDataV2Info;
	P_CMD_CAL_BACKUP_STRUCT_V2_T		prCmdCalBackupDataV2;
	UINT_8	ucReason, ucAction, ucNeedResp, ucFragNum, ucRomRam;
	UINT_32	u4DumpMaxSize = PARAM_CAL_DATA_DUMP_MAX_SIZE;
	UINT_32	u4RemainLength, u4CurrAddr, u4CurrLen;

	DBGLOG(RFTEST, INFO, "%s\n", __func__);

	prCmdCalBackupDataV2 =
		(P_CMD_CAL_BACKUP_STRUCT_V2_T) kalMemAlloc(sizeof(CMD_CAL_BACKUP_STRUCT_V2_T), VIR_MEM_TYPE);

	prCalBackupDataV2Info = (P_PARAM_CAL_BACKUP_STRUCT_V2_T) pvQueryBuffer;

	ucReason = prCalBackupDataV2Info->ucReason;
	ucAction = prCalBackupDataV2Info->ucAction;
	ucNeedResp = prCalBackupDataV2Info->ucNeedResp;
	ucRomRam = prCalBackupDataV2Info->ucRomRam;

	if (ucAction == 2) {
		/* Trigger All Cal Function */
		prCmdCalBackupDataV2->ucReason = ucReason;
		prCmdCalBackupDataV2->ucAction = ucAction;
		prCmdCalBackupDataV2->ucNeedResp = ucNeedResp;
		prCmdCalBackupDataV2->ucFragNum = prCalBackupDataV2Info->ucFragNum;
		prCmdCalBackupDataV2->ucRomRam = ucRomRam;
		prCmdCalBackupDataV2->u4ThermalValue = prCalBackupDataV2Info->u4ThermalValue;
		prCmdCalBackupDataV2->u4Address = prCalBackupDataV2Info->u4Address;
		prCmdCalBackupDataV2->u4Length = prCalBackupDataV2Info->u4Length;
		prCmdCalBackupDataV2->u4RemainLength = prCalBackupDataV2Info->u4RemainLength;
#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST_DBGLOG
		DBGLOG(RFTEST, INFO, "=========== Driver Send Query CMD#0 or CMD#1 (Info) ===========\n");
		DBGLOG(RFTEST, INFO, "Reason = %d\n", prCmdCalBackupDataV2->ucReason);
		DBGLOG(RFTEST, INFO, "Action = %d\n", prCmdCalBackupDataV2->ucAction);
		DBGLOG(RFTEST, INFO, "NeedResp = %d\n", prCmdCalBackupDataV2->ucNeedResp);
		DBGLOG(RFTEST, INFO, "FragNum = %d\n", prCmdCalBackupDataV2->ucFragNum);
		DBGLOG(RFTEST, INFO, "RomRam = %d\n", prCmdCalBackupDataV2->ucRomRam);
		DBGLOG(RFTEST, INFO, "ThermalValue = %d\n", prCmdCalBackupDataV2->u4ThermalValue);
		DBGLOG(RFTEST, INFO, "Address = 0x%08x\n", prCmdCalBackupDataV2->u4Address);
		DBGLOG(RFTEST, INFO, "Length = %d\n", prCmdCalBackupDataV2->u4Length);
		DBGLOG(RFTEST, INFO, "RemainLength = %d\n", prCmdCalBackupDataV2->u4RemainLength);
		DBGLOG(RFTEST, INFO, "================================================================\n");
#endif

		rWlanStatus = wlanSendSetQueryCmd(prAdapter,
					CMD_ID_CAL_BACKUP_IN_HOST_V2,
					TRUE,
					FALSE,
					g_fgIsOid,
					nicCmdEventSetCommon,
					NULL,
					sizeof(CMD_CAL_BACKUP_STRUCT_V2_T),
					(PUINT_8) prCmdCalBackupDataV2, pvQueryBuffer, u4QueryBufferLen);

		kalMemFree(prCmdCalBackupDataV2, VIR_MEM_TYPE, sizeof(CMD_CAL_BACKUP_STRUCT_V2_T));
	} else if (ucAction == 0 || ucAction == 1 || ucAction == 6) {
		/* Query CMD#0 and CMD#1. */
		/* For Thermal Value and Total Cal Data Length. */
		prCmdCalBackupDataV2->ucReason = ucReason;
		prCmdCalBackupDataV2->ucAction = ucAction;
		prCmdCalBackupDataV2->ucNeedResp = ucNeedResp;
		prCmdCalBackupDataV2->ucFragNum = prCalBackupDataV2Info->ucFragNum;
		prCmdCalBackupDataV2->ucRomRam = ucRomRam;
		prCmdCalBackupDataV2->u4ThermalValue = prCalBackupDataV2Info->u4ThermalValue;
		prCmdCalBackupDataV2->u4Address = prCalBackupDataV2Info->u4Address;
		prCmdCalBackupDataV2->u4Length = prCalBackupDataV2Info->u4Length;
		prCmdCalBackupDataV2->u4RemainLength = prCalBackupDataV2Info->u4RemainLength;
#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST_DBGLOG
		DBGLOG(RFTEST, INFO, "=========== Driver Send Query CMD#0 or CMD#1 (Info) ===========\n");
		DBGLOG(RFTEST, INFO, "Reason = %d\n", prCmdCalBackupDataV2->ucReason);
		DBGLOG(RFTEST, INFO, "Action = %d\n", prCmdCalBackupDataV2->ucAction);
		DBGLOG(RFTEST, INFO, "NeedResp = %d\n", prCmdCalBackupDataV2->ucNeedResp);
		DBGLOG(RFTEST, INFO, "FragNum = %d\n", prCmdCalBackupDataV2->ucFragNum);
		DBGLOG(RFTEST, INFO, "RomRam = %d\n", prCmdCalBackupDataV2->ucRomRam);
		DBGLOG(RFTEST, INFO, "ThermalValue = %d\n", prCmdCalBackupDataV2->u4ThermalValue);
		DBGLOG(RFTEST, INFO, "Address = 0x%08x\n", prCmdCalBackupDataV2->u4Address);
		DBGLOG(RFTEST, INFO, "Length = %d\n", prCmdCalBackupDataV2->u4Length);
		DBGLOG(RFTEST, INFO, "RemainLength = %d\n", prCmdCalBackupDataV2->u4RemainLength);
		DBGLOG(RFTEST, INFO, "================================================================\n");
#endif
		rWlanStatus = wlanSendSetQueryCmd(prAdapter,
					CMD_ID_CAL_BACKUP_IN_HOST_V2,
					FALSE,
					TRUE,
					FALSE,
					nicCmdEventQueryCalBackupV2,
					NULL,
					sizeof(CMD_CAL_BACKUP_STRUCT_V2_T),
					(PUINT_8) prCmdCalBackupDataV2, pvQueryBuffer, u4QueryBufferLen);

		kalMemFree(prCmdCalBackupDataV2, VIR_MEM_TYPE, sizeof(CMD_CAL_BACKUP_STRUCT_V2_T));
	} else if (ucAction == 4) {
		/* Query  All Cal Data from FW (Rom or Ram). */
		u4RemainLength = prCalBackupDataV2Info->u4RemainLength;
		u4CurrAddr = prCalBackupDataV2Info->u4Address + prCalBackupDataV2Info->u4Length;
		ucFragNum = prCalBackupDataV2Info->ucFragNum + 1;

		if (u4RemainLength > u4DumpMaxSize) {
			u4CurrLen = u4DumpMaxSize;
			u4RemainLength -= u4DumpMaxSize;
		} else {
			u4CurrLen = u4RemainLength;
			u4RemainLength = 0;
		}

		prCmdCalBackupDataV2->ucReason = ucReason;
		prCmdCalBackupDataV2->ucAction = ucAction;
		prCmdCalBackupDataV2->ucNeedResp = ucNeedResp;
		prCmdCalBackupDataV2->ucFragNum = ucFragNum;
		prCmdCalBackupDataV2->ucRomRam = ucRomRam;
		prCmdCalBackupDataV2->u4ThermalValue = prCalBackupDataV2Info->u4ThermalValue;
		prCmdCalBackupDataV2->u4Address = u4CurrAddr;
		prCmdCalBackupDataV2->u4Length = u4CurrLen;
		prCmdCalBackupDataV2->u4RemainLength = u4RemainLength;
#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST_DBGLOG
		DBGLOG(RFTEST, INFO, "========= Driver Send Query All Cal Data from FW (Info) =========\n");
		DBGLOG(RFTEST, INFO, "Reason = %d\n", prCmdCalBackupDataV2->ucReason);
		DBGLOG(RFTEST, INFO, "Action = %d\n", prCmdCalBackupDataV2->ucAction);
		DBGLOG(RFTEST, INFO, "NeedResp = %d\n", prCmdCalBackupDataV2->ucNeedResp);
		DBGLOG(RFTEST, INFO, "FragNum = %d\n", prCmdCalBackupDataV2->ucFragNum);
		DBGLOG(RFTEST, INFO, "RomRam = %d\n", prCmdCalBackupDataV2->ucRomRam);
		DBGLOG(RFTEST, INFO, "ThermalValue = %d\n", prCmdCalBackupDataV2->u4ThermalValue);
		DBGLOG(RFTEST, INFO, "Address = 0x%08x\n", prCmdCalBackupDataV2->u4Address);
		DBGLOG(RFTEST, INFO, "Length = %d\n", prCmdCalBackupDataV2->u4Length);
		DBGLOG(RFTEST, INFO, "RemainLength = %d\n", prCmdCalBackupDataV2->u4RemainLength);
		DBGLOG(RFTEST, INFO, "=================================================================\n");
#endif
		rWlanStatus = wlanSendSetQueryCmd(prAdapter,
					CMD_ID_CAL_BACKUP_IN_HOST_V2,
					FALSE,
					TRUE,
					FALSE,
					nicCmdEventQueryCalBackupV2,
					NULL,
					sizeof(CMD_CAL_BACKUP_STRUCT_V2_T),
					(PUINT_8) prCmdCalBackupDataV2, pvQueryBuffer, u4QueryBufferLen);

		kalMemFree(prCmdCalBackupDataV2, VIR_MEM_TYPE, sizeof(CMD_CAL_BACKUP_STRUCT_V2_T));
	} else if (ucAction == 5) {
		/* Send  All Cal Data to FW (Rom or Ram). */
		u4RemainLength = prCalBackupDataV2Info->u4RemainLength;
		u4CurrAddr = prCalBackupDataV2Info->u4Address + prCalBackupDataV2Info->u4Length;
		ucFragNum = prCalBackupDataV2Info->ucFragNum + 1;

		if (u4RemainLength > u4DumpMaxSize) {
			u4CurrLen = u4DumpMaxSize;
			u4RemainLength -= u4DumpMaxSize;
		} else {
			u4CurrLen = u4RemainLength;
			u4RemainLength = 0;
		}

		prCmdCalBackupDataV2->ucReason = ucReason;
		prCmdCalBackupDataV2->ucAction = ucAction;
		prCmdCalBackupDataV2->ucNeedResp = ucNeedResp;
		prCmdCalBackupDataV2->ucFragNum = ucFragNum;
		prCmdCalBackupDataV2->ucRomRam = ucRomRam;
		prCmdCalBackupDataV2->u4ThermalValue = prCalBackupDataV2Info->u4ThermalValue;
		prCmdCalBackupDataV2->u4Address = u4CurrAddr;
		prCmdCalBackupDataV2->u4Length = u4CurrLen;
		prCmdCalBackupDataV2->u4RemainLength = u4RemainLength;
#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST_DBGLOG
		DBGLOG(RFTEST, INFO, "========= Driver Send All Cal Data to FW (Info) =========\n");
		DBGLOG(RFTEST, INFO, "Reason = %d\n", prCmdCalBackupDataV2->ucReason);
		DBGLOG(RFTEST, INFO, "Action = %d\n", prCmdCalBackupDataV2->ucAction);
		DBGLOG(RFTEST, INFO, "NeedResp = %d\n", prCmdCalBackupDataV2->ucNeedResp);
		DBGLOG(RFTEST, INFO, "FragNum = %d\n", prCmdCalBackupDataV2->ucFragNum);
		DBGLOG(RFTEST, INFO, "RomRam = %d\n", prCmdCalBackupDataV2->ucRomRam);
		DBGLOG(RFTEST, INFO, "ThermalValue = %d\n", prCmdCalBackupDataV2->u4ThermalValue);
		DBGLOG(RFTEST, INFO, "Address = 0x%08x\n", prCmdCalBackupDataV2->u4Address);
		DBGLOG(RFTEST, INFO, "Length = %d\n", prCmdCalBackupDataV2->u4Length);
		DBGLOG(RFTEST, INFO, "RemainLength = %d\n", prCmdCalBackupDataV2->u4RemainLength);
#endif
		/* Copy Cal Data From Driver to FW */
		if (prCmdCalBackupDataV2->ucRomRam == 0)
			kalMemCopy((PUINT_8)(prCmdCalBackupDataV2->au4Buffer),
			(PUINT_8)(g_rBackupCalDataAllV2.au4RomCalData) + prCmdCalBackupDataV2->u4Address,
			prCmdCalBackupDataV2->u4Length);
		else if (prCmdCalBackupDataV2->ucRomRam == 1)
			kalMemCopy((PUINT_8)(prCmdCalBackupDataV2->au4Buffer),
			(PUINT_8)(g_rBackupCalDataAllV2.au4RamCalData) + prCmdCalBackupDataV2->u4Address,
			prCmdCalBackupDataV2->u4Length);
#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST_DBGLOG
		DBGLOG(RFTEST, INFO, "Check some of elements (0x%08x), (0x%08x), (0x%08x), (0x%08x), (0x%08x)\n",
				prCmdCalBackupDataV2->au4Buffer[0], prCmdCalBackupDataV2->au4Buffer[1],
				prCmdCalBackupDataV2->au4Buffer[2], prCmdCalBackupDataV2->au4Buffer[3],
				prCmdCalBackupDataV2->au4Buffer[4]);
		DBGLOG(RFTEST, INFO, "Check some of elements (0x%08x), (0x%08x), (0x%08x), (0x%08x), (0x%08x)\n",
				prCmdCalBackupDataV2->au4Buffer[(prCmdCalBackupDataV2->u4Length/sizeof(UINT_32)) - 5],
				prCmdCalBackupDataV2->au4Buffer[(prCmdCalBackupDataV2->u4Length/sizeof(UINT_32)) - 4],
				prCmdCalBackupDataV2->au4Buffer[(prCmdCalBackupDataV2->u4Length/sizeof(UINT_32)) - 3],
				prCmdCalBackupDataV2->au4Buffer[(prCmdCalBackupDataV2->u4Length/sizeof(UINT_32)) - 2],
				prCmdCalBackupDataV2->au4Buffer[(prCmdCalBackupDataV2->u4Length/sizeof(UINT_32)) - 1]);

		DBGLOG(RFTEST, INFO, "=================================================================\n");
#endif
		rWlanStatus = wlanSendSetQueryCmd(prAdapter,
					CMD_ID_CAL_BACKUP_IN_HOST_V2,
					FALSE,
					TRUE,
					FALSE,
					nicCmdEventQueryCalBackupV2,
					NULL,
					sizeof(CMD_CAL_BACKUP_STRUCT_V2_T),
					(PUINT_8) prCmdCalBackupDataV2, pvQueryBuffer, u4QueryBufferLen);

		kalMemFree(prCmdCalBackupDataV2, VIR_MEM_TYPE, sizeof(CMD_CAL_BACKUP_STRUCT_V2_T));
	}

	 return rWlanStatus;
}

WLAN_STATUS
wlanoidQueryCalBackupV2(IN P_ADAPTER_T prAdapter,
		    IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	WLAN_STATUS							rWlanStatus = WLAN_STATUS_SUCCESS;
	P_PARAM_CAL_BACKUP_STRUCT_V2_T		prCalBackupDataV2Info;

	DBGLOG(RFTEST, INFO, "%s\n", __func__);

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(CMD_CAL_BACKUP_STRUCT_V2_T);

	prCalBackupDataV2Info = (P_PARAM_CAL_BACKUP_STRUCT_V2_T) pvQueryBuffer;

	if (prCalBackupDataV2Info->ucReason == 0 && prCalBackupDataV2Info->ucAction == 0) {
		/* Get Thermal Temp from FW */
		return wlanoidSendCalBackupV2Cmd(prAdapter, pvQueryBuffer, u4QueryBufferLen);
	} else if (prCalBackupDataV2Info->ucReason == 0 && prCalBackupDataV2Info->ucAction == 1) {
		/* Get Cal Data Size from FW */
		return wlanoidSendCalBackupV2Cmd(prAdapter, pvQueryBuffer, u4QueryBufferLen);
	} else if (prCalBackupDataV2Info->ucReason == 2 && prCalBackupDataV2Info->ucAction == 4) {
		/* Get Cal Data from FW */
		if (prCalBackupDataV2Info->ucRomRam == 0)
			prCalBackupDataV2Info->u4RemainLength = g_rBackupCalDataAllV2.u4ValidRomCalDataLength;
		else if (prCalBackupDataV2Info->ucRomRam == 1)
			prCalBackupDataV2Info->u4RemainLength = g_rBackupCalDataAllV2.u4ValidRamCalDataLength;

		return wlanoidSendCalBackupV2Cmd(prAdapter, pvQueryBuffer, u4QueryBufferLen);
	} else {
		return rWlanStatus;
	}
}
#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to do Coex Isolation Detection.

* \param[in] pvAdapter Pointer to the Adapter structure.
* \param[out] pvQueryBuf A pointer to the buffer that holds the result of
*                                   the query.
* \param[in] u4QueryBufLen The length of the query buffer.
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                                   bytes written into the query buffer. If the call
*                                   failed due to invalid length of the query buffer,
*                            returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryCoexIso(IN P_ADAPTER_T prAdapter,
		    IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	struct PARAM_COEX_CTRL *prParaCoexCtrl;
	struct PARAM_COEX_ISO_DETECT *prParaCoexIsoDetect;
	struct CMD_COEX_CTRL rCmdCoexCtrl;
	struct CMD_COEX_ISO_DETECT rCmdCoexIsoDetect;

	DBGLOG(INIT, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);

	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(struct PARAM_COEX_CTRL);

	if (u4QueryBufferLen < sizeof(struct PARAM_COEX_CTRL))
		return WLAN_STATUS_INVALID_LENGTH;

	prParaCoexCtrl = (struct PARAM_COEX_CTRL *) pvQueryBuffer;
	prParaCoexIsoDetect = (struct PARAM_COEX_ISO_DETECT *) &prParaCoexCtrl->aucBuffer[0];

	rCmdCoexIsoDetect.u4Channel = prParaCoexIsoDetect->u4Channel;
	/*rCmdCoexIsoDetect.u4Band = prParaCoexIsoDetect->u4Band;*/
	rCmdCoexIsoDetect.u4IsoPath = prParaCoexIsoDetect->u4IsoPath;
	rCmdCoexIsoDetect.u4Isolation = prParaCoexIsoDetect->u4Isolation;

	rCmdCoexCtrl.u4SubCmd = prParaCoexCtrl->u4SubCmd;

	/* Copy Memory */
	kalMemCopy(rCmdCoexCtrl.aucBuffer, &rCmdCoexIsoDetect, sizeof(rCmdCoexIsoDetect));

	return wlanSendSetQueryCmd(prAdapter,
				CMD_ID_COEX_CTRL,
				FALSE,
				TRUE,
				g_fgIsOid,
				nicCmdEventQueryCoexIso,
				nicOidCmdTimeoutCommon,
				sizeof(struct CMD_COEX_CTRL),
				(unsigned char *) &rCmdCoexCtrl,
				pvQueryBuffer,
				u4QueryBufferLen);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to get coex information.

* \param[in] pvAdapter Pointer to the Adapter structure.
* \param[out] pvQueryBuf A pointer to the buffer that holds the result of
*                                   the query.
* \param[in] u4QueryBufLen The length of the query buffer.
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                                   bytes written into the query buffer. If the call
*                                   failed due to invalid length of the query buffer,
*                            returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryCoexGetInfo(IN P_ADAPTER_T prAdapter,
		    IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	struct PARAM_COEX_CTRL *prParaCoexCtrl;
	struct PARAM_COEX_GET_INFO *prParaCoexGetInfo;
	struct CMD_COEX_CTRL rCmdCoexCtrl;
	struct CMD_COEX_GET_INFO rCmdCoexGetInfo;

	DBGLOG(INIT, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);

	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(struct PARAM_COEX_CTRL);

	if (u4QueryBufferLen < sizeof(struct PARAM_COEX_CTRL))
		return WLAN_STATUS_INVALID_LENGTH;

	prParaCoexCtrl = (struct PARAM_COEX_CTRL *) pvQueryBuffer;
	prParaCoexGetInfo = (struct PARAM_COEX_GET_INFO *) &prParaCoexCtrl->aucBuffer[0];

	kalMemZero(rCmdCoexGetInfo.u4CoexInfo, sizeof(rCmdCoexGetInfo.u4CoexInfo));

	rCmdCoexCtrl.u4SubCmd = prParaCoexCtrl->u4SubCmd;

	/* Copy Memory */
	kalMemCopy(rCmdCoexCtrl.aucBuffer, &rCmdCoexGetInfo, sizeof(rCmdCoexGetInfo));
	DBGLOG(REQ, INFO, "wlanoidQueryCoexGetInfo end\n");

	return wlanSendSetQueryCmd(prAdapter,
				CMD_ID_COEX_CTRL,
				FALSE,
				TRUE,
				g_fgIsOid,
				nicCmdEventQueryCoexGetInfo,
				nicOidCmdTimeoutCommon,
				sizeof(struct CMD_COEX_CTRL),
				(unsigned char *) &rCmdCoexCtrl,
				pvQueryBuffer,
				u4QueryBufferLen);

}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query MCR value.
*
* \param[in] pvAdapter Pointer to the Adapter structure.
* \param[out] pvQueryBuf A pointer to the buffer that holds the result of
*                           the query.
* \param[in] u4QueryBufLen The length of the query buffer.
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                            bytes written into the query buffer. If the call
*                            failed due to invalid length of the query buffer,
*                            returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryMcrRead(IN P_ADAPTER_T prAdapter,
		    IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	P_PARAM_CUSTOM_MCR_RW_STRUCT_T prMcrRdInfo;
	CMD_ACCESS_REG rCmdAccessReg;

	DEBUGFUNC("wlanoidQueryMcrRead");
	DBGLOG(INIT, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(PARAM_CUSTOM_MCR_RW_STRUCT_T);

	if (u4QueryBufferLen < sizeof(PARAM_CUSTOM_MCR_RW_STRUCT_T))
		return WLAN_STATUS_INVALID_LENGTH;

	prMcrRdInfo = (P_PARAM_CUSTOM_MCR_RW_STRUCT_T) pvQueryBuffer;

	/* 0x9000 - 0x9EFF reserved for FW */
#if CFG_SUPPORT_SWCR
	if ((prMcrRdInfo->u4McrOffset >> 16) == 0x9F00) {
		swCrReadWriteCmd(prAdapter,
				 SWCR_READ,
				 (UINT_16) (prMcrRdInfo->u4McrOffset & BITS(0, 15)), &prMcrRdInfo->u4McrData);
		return WLAN_STATUS_SUCCESS;
	}
#endif /* CFG_SUPPORT_SWCR */

	/* Check if access F/W Domain MCR (due to WiFiSYS is placed from 0x6000-0000 */
	if (prMcrRdInfo->u4McrOffset & 0xFFFF0000) {
		/* fill command */
		rCmdAccessReg.u4Address = prMcrRdInfo->u4McrOffset;
		rCmdAccessReg.u4Data = 0;

		return wlanSendSetQueryCmd(prAdapter,
					   CMD_ID_ACCESS_REG,
					   FALSE,
					   TRUE,
					   g_fgIsOid,
					   nicCmdEventQueryMcrRead,
					   nicOidCmdTimeoutCommon,
					   sizeof(CMD_ACCESS_REG),
					   (PUINT_8) &rCmdAccessReg, pvQueryBuffer, u4QueryBufferLen);
	} else {
		HAL_MCR_RD(prAdapter, (prMcrRdInfo->u4McrOffset & BITS(2, 31)),	/* address is in DWORD unit */
			   &prMcrRdInfo->u4McrData);

		DBGLOG(INIT, TRACE, "MCR Read: Offset = %#08x, Data = %#08x\n",
		       prMcrRdInfo->u4McrOffset, prMcrRdInfo->u4McrData);
		return WLAN_STATUS_SUCCESS;
	}
}				/* end of wlanoidQueryMcrRead() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to write MCR and enable specific function.
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[in] pvSetBuffer A pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                           bytes read from the set buffer. If the call failed
*                           due to invalid length of the set buffer, returns
*                           the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetMcrWrite(IN P_ADAPTER_T prAdapter,
		   IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_PARAM_CUSTOM_MCR_RW_STRUCT_T prMcrWrInfo;
	CMD_ACCESS_REG rCmdAccessReg;

#if CFG_STRESS_TEST_SUPPORT
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_BSS_INFO_T prBssInfo = prAdapter->prAisBssInfo;
	P_STA_RECORD_T prStaRec = prBssInfo->prStaRecOfAP;
	UINT_32 u4McrOffset, u4McrData;
#endif

	DEBUGFUNC("wlanoidSetMcrWrite");
	DBGLOG(INIT, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(PARAM_CUSTOM_MCR_RW_STRUCT_T);

	if (u4SetBufferLen < sizeof(PARAM_CUSTOM_MCR_RW_STRUCT_T))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prMcrWrInfo = (P_PARAM_CUSTOM_MCR_RW_STRUCT_T) pvSetBuffer;

	/* 0x9000 - 0x9EFF reserved for FW */
	/* 0xFFFE          reserved for FW */

	/* -- Puff Stress Test Begin */
#if CFG_STRESS_TEST_SUPPORT

	/* 0xFFFFFFFE for Control Rate */
	if (prMcrWrInfo->u4McrOffset == 0xFFFFFFFE) {
		if (prMcrWrInfo->u4McrData < FIXED_RATE_NUM && prMcrWrInfo->u4McrData > 0)
			prAdapter->rWifiVar.eRateSetting = (ENUM_REGISTRY_FIXED_RATE_T) (prMcrWrInfo->u4McrData);
		cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_1);
		cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_3);
		DEBUGFUNC("[Stress Test]Complete Rate is Changed...\n");
		DBGLOG(INIT, TRACE, "[Stress Test] Rate is Changed to index %d...\n", prAdapter->rWifiVar.eRateSetting);
	}
	/* 0xFFFFFFFD for Switch Channel */
	else if (prMcrWrInfo->u4McrOffset == 0xFFFFFFFD) {
		if (prMcrWrInfo->u4McrData <= 11 && prMcrWrInfo->u4McrData >= 1)
			prBssInfo->ucPrimaryChannel = prMcrWrInfo->u4McrData;
		nicUpdateBss(prAdapter, prBssInfo->ucNetTypeIndex);
		DBGLOG(INIT, TRACE, "[Stress Test] Channel is switched to %d ...\n", prBssInfo->ucPrimaryChannel);

		return WLAN_STATUS_SUCCESS;
	}
	/* 0xFFFFFFFFC for Control RF Band and SCO */
	else if (prMcrWrInfo->u4McrOffset == 0xFFFFFFFC) {
		/* Band */
		if (prMcrWrInfo->u4McrData & 0x80000000) {
			/* prBssInfo->eBand = BAND_5G; */
			/* prBssInfo->ucPrimaryChannel = 52;  // Bond to Channel 52 */
		} else {
			prBssInfo->eBand = BAND_2G4;
			prBssInfo->ucPrimaryChannel = 8;	/* Bond to Channel 6 */
		}

		/* Bandwidth */
		if (prMcrWrInfo->u4McrData & 0x00010000) {
			prStaRec->u2HtCapInfo |= HT_CAP_INFO_SUP_CHNL_WIDTH;
			prStaRec->ucDesiredPhyTypeSet = PHY_TYPE_BIT_HT;

			if (prMcrWrInfo->u4McrData == 0x00010002) {
				prBssInfo->eBssSCO = CHNL_EXT_SCB;	/* U20 */
				prBssInfo->ucPrimaryChannel += 2;
			} else if (prMcrWrInfo->u4McrData == 0x00010001) {
				prBssInfo->eBssSCO = CHNL_EXT_SCA;	/* L20 */
				prBssInfo->ucPrimaryChannel -= 2;
			} else {
				prBssInfo->eBssSCO = CHNL_EXT_SCA;	/* 40 */
			}
		}

		rlmBssInitForAPandIbss(prAdapter, prBssInfo);
	}
	/* 0xFFFFFFFB for HT Capability */
	else if (prMcrWrInfo->u4McrOffset == 0xFFFFFFFB) {
		/* Enable HT Capability */
		if (prMcrWrInfo->u4McrData & 0x00000001) {
			prStaRec->u2HtCapInfo |= HT_CAP_INFO_HT_GF;
			DEBUGFUNC("[Stress Test]Enable HT capability...\n");
		} else {
			prStaRec->u2HtCapInfo &= (~HT_CAP_INFO_HT_GF);
			DEBUGFUNC("[Stress Test]Disable HT capability...\n");
		}
		cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_1);
		cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_3);
	}
	/* 0xFFFFFFFA for Enable Random Rx Reset */
	else if (prMcrWrInfo->u4McrOffset == 0xFFFFFFFA) {
		rCmdAccessReg.u4Address = prMcrWrInfo->u4McrOffset;
		rCmdAccessReg.u4Data = prMcrWrInfo->u4McrData;

		return wlanSendSetQueryCmd(prAdapter,
					   CMD_ID_RANDOM_RX_RESET_EN,
					   TRUE,
					   FALSE,
					   g_fgIsOid,
					   nicCmdEventSetCommon,
					   nicOidCmdTimeoutCommon,
					   sizeof(CMD_ACCESS_REG),
					   (PUINT_8) &rCmdAccessReg, pvSetBuffer, u4SetBufferLen);
	}
	/* 0xFFFFFFF9 for Disable Random Rx Reset */
	else if (prMcrWrInfo->u4McrOffset == 0xFFFFFFF9) {
		rCmdAccessReg.u4Address = prMcrWrInfo->u4McrOffset;
		rCmdAccessReg.u4Data = prMcrWrInfo->u4McrData;

		return wlanSendSetQueryCmd(prAdapter,
					   CMD_ID_RANDOM_RX_RESET_DE,
					   TRUE,
					   FALSE,
					   g_fgIsOid,
					   nicCmdEventSetCommon,
					   nicOidCmdTimeoutCommon,
					   sizeof(CMD_ACCESS_REG),
					   (PUINT_8) &rCmdAccessReg, pvSetBuffer, u4SetBufferLen);
	}
	/* 0xFFFFFFF8 for Enable SAPP */
	else if (prMcrWrInfo->u4McrOffset == 0xFFFFFFF8) {
		rCmdAccessReg.u4Address = prMcrWrInfo->u4McrOffset;
		rCmdAccessReg.u4Data = prMcrWrInfo->u4McrData;

		return wlanSendSetQueryCmd(prAdapter,
					   CMD_ID_SAPP_EN,
					   TRUE,
					   FALSE,
					   g_fgIsOid,
					   nicCmdEventSetCommon,
					   nicOidCmdTimeoutCommon,
					   sizeof(CMD_ACCESS_REG),
					   (PUINT_8) &rCmdAccessReg, pvSetBuffer, u4SetBufferLen);
	}
	/* 0xFFFFFFF7 for Disable SAPP */
	else if (prMcrWrInfo->u4McrOffset == 0xFFFFFFF7) {
		rCmdAccessReg.u4Address = prMcrWrInfo->u4McrOffset;
		rCmdAccessReg.u4Data = prMcrWrInfo->u4McrData;

		return wlanSendSetQueryCmd(prAdapter,
					   CMD_ID_SAPP_DE,
					   TRUE,
					   FALSE,
					   g_fgIsOid,
					   nicCmdEventSetCommon,
					   nicOidCmdTimeoutCommon,
					   sizeof(CMD_ACCESS_REG),
					   (PUINT_8) &rCmdAccessReg, pvSetBuffer, u4SetBufferLen);
	}

	else
#endif
		/* -- Puff Stress Test End */

		/* Check if access F/W Domain MCR */
	if (prMcrWrInfo->u4McrOffset & 0xFFFF0000) {

		/* 0x9000 - 0x9EFF reserved for FW */
#if CFG_SUPPORT_SWCR
		if ((prMcrWrInfo->u4McrOffset >> 16) == 0x9F00) {
			swCrReadWriteCmd(prAdapter,
					 SWCR_WRITE,
					 (UINT_16) (prMcrWrInfo->u4McrOffset & BITS(0, 15)), &prMcrWrInfo->u4McrData);
			return WLAN_STATUS_SUCCESS;
		}
#endif /* CFG_SUPPORT_SWCR */

#if 1
		/* low power test special command */
		if (prMcrWrInfo->u4McrOffset == 0x11111110) {
			WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
			/* DbgPrint("Enter test mode\n"); */
			prAdapter->fgTestMode = TRUE;
			return rStatus;
		}
		if (prMcrWrInfo->u4McrOffset == 0x11111111) {
			/* DbgPrint("nicpmSetAcpiPowerD3\n"); */

			nicpmSetAcpiPowerD3(prAdapter);
			kalDevSetPowerState(prAdapter->prGlueInfo, (UINT_32) ParamDeviceStateD3);
			return WLAN_STATUS_SUCCESS;
		}
		if (prMcrWrInfo->u4McrOffset == 0x11111112) {

			/* DbgPrint("LP enter sleep\n"); */

			/* fill command */
			rCmdAccessReg.u4Address = prMcrWrInfo->u4McrOffset;
			rCmdAccessReg.u4Data = prMcrWrInfo->u4McrData;

			return wlanSendSetQueryCmd(prAdapter,
						   CMD_ID_ACCESS_REG,
						   TRUE,
						   FALSE,
						   g_fgIsOid,
						   nicCmdEventSetCommon,
						   nicOidCmdTimeoutCommon,
						   sizeof(CMD_ACCESS_REG),
						   (PUINT_8) &rCmdAccessReg, pvSetBuffer, u4SetBufferLen);
		}
#endif

#if 1
		/* low power test special command */
		if (prMcrWrInfo->u4McrOffset == 0x11111110) {
			WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
			/* DbgPrint("Enter test mode\n"); */
			prAdapter->fgTestMode = TRUE;
			return rStatus;
		}
		if (prMcrWrInfo->u4McrOffset == 0x11111111) {
			/* DbgPrint("nicpmSetAcpiPowerD3\n"); */

			nicpmSetAcpiPowerD3(prAdapter);
			kalDevSetPowerState(prAdapter->prGlueInfo, (UINT_32) ParamDeviceStateD3);
			return WLAN_STATUS_SUCCESS;
		}
		if (prMcrWrInfo->u4McrOffset == 0x11111112) {

			/* DbgPrint("LP enter sleep\n"); */

			/* fill command */
			rCmdAccessReg.u4Address = prMcrWrInfo->u4McrOffset;
			rCmdAccessReg.u4Data = prMcrWrInfo->u4McrData;

			return wlanSendSetQueryCmd(prAdapter,
						   CMD_ID_ACCESS_REG,
						   TRUE,
						   FALSE,
						   g_fgIsOid,
						   nicCmdEventSetCommon,
						   nicOidCmdTimeoutCommon,
						   sizeof(CMD_ACCESS_REG),
						   (PUINT_8) &rCmdAccessReg, pvSetBuffer, u4SetBufferLen);
		}
#endif

#if CFG_SUPPORT_SDIO_READ_WRITE_PATTERN
		if (prMcrWrInfo->u4McrOffset == 0x22220000) {
			/* read test mode */
			kalSetSdioTestPattern(prAdapter->prGlueInfo, TRUE, TRUE);

			return WLAN_STATUS_SUCCESS;
		}

		if (prMcrWrInfo->u4McrOffset == 0x22220001) {
			/* write test mode */
			kalSetSdioTestPattern(prAdapter->prGlueInfo, TRUE, FALSE);

			return WLAN_STATUS_SUCCESS;
		}

		if (prMcrWrInfo->u4McrOffset == 0x22220002) {
			/* leave from test mode */
			kalSetSdioTestPattern(prAdapter->prGlueInfo, FALSE, FALSE);

			return WLAN_STATUS_SUCCESS;
		}
#endif

		/* fill command */
		rCmdAccessReg.u4Address = prMcrWrInfo->u4McrOffset;
		rCmdAccessReg.u4Data = prMcrWrInfo->u4McrData;

		return wlanSendSetQueryCmd(prAdapter,
					   CMD_ID_ACCESS_REG,
					   TRUE,
					   FALSE,
					   g_fgIsOid,
					   nicCmdEventSetCommon,
					   nicOidCmdTimeoutCommon,
					   sizeof(CMD_ACCESS_REG),
					   (PUINT_8) &rCmdAccessReg, pvSetBuffer, u4SetBufferLen);
	} else {
		HAL_MCR_WR(prAdapter, (prMcrWrInfo->u4McrOffset & BITS(2, 31)),	/* address is in DWORD unit */
			   prMcrWrInfo->u4McrData);

		DBGLOG(INIT, TRACE, "MCR Write: Offset = %#08x, Data = %#08x\n",
		       prMcrWrInfo->u4McrOffset, prMcrWrInfo->u4McrData);

		return WLAN_STATUS_SUCCESS;
	}
}				/* wlanoidSetMcrWrite */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query driver MCR value.
*
* \param[in] pvAdapter Pointer to the Adapter structure.
* \param[out] pvQueryBuf A pointer to the buffer that holds the result of
*                           the query.
* \param[in] u4QueryBufLen The length of the query buffer.
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                            bytes written into the query buffer. If the call
*                            failed due to invalid length of the query buffer,
*                            returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryDrvMcrRead(IN P_ADAPTER_T prAdapter,
		       IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	P_PARAM_CUSTOM_MCR_RW_STRUCT_T prMcrRdInfo;
	/* CMD_ACCESS_REG rCmdAccessReg; */

	DEBUGFUNC("wlanoidQueryMcrRead");
	DBGLOG(INIT, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(PARAM_CUSTOM_MCR_RW_STRUCT_T);

	if (u4QueryBufferLen < sizeof(PARAM_CUSTOM_MCR_RW_STRUCT_T))
		return WLAN_STATUS_INVALID_LENGTH;

	prMcrRdInfo = (P_PARAM_CUSTOM_MCR_RW_STRUCT_T) pvQueryBuffer;

	ACQUIRE_POWER_CONTROL_FROM_PM(prAdapter);
	HAL_MCR_RD(prAdapter, (prMcrRdInfo->u4McrOffset & BITS(2, 31)), &prMcrRdInfo->u4McrData);

	DBGLOG(INIT, TRACE, "DRV MCR Read: Offset = %#08x, Data = %#08x\n",
	       prMcrRdInfo->u4McrOffset, prMcrRdInfo->u4McrData);

	return WLAN_STATUS_SUCCESS;

}				/* end of wlanoidQueryMcrRead() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to write MCR and enable specific function.
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[in] pvSetBuffer A pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                           bytes read from the set buffer. If the call failed
*                           due to invalid length of the set buffer, returns
*                           the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetDrvMcrWrite(IN P_ADAPTER_T prAdapter,
		      IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_PARAM_CUSTOM_MCR_RW_STRUCT_T prMcrWrInfo;
	/* CMD_ACCESS_REG rCmdAccessReg;  */

	DEBUGFUNC("wlanoidSetMcrWrite");
	DBGLOG(INIT, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(PARAM_CUSTOM_MCR_RW_STRUCT_T);

	if (u4SetBufferLen < sizeof(PARAM_CUSTOM_MCR_RW_STRUCT_T))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prMcrWrInfo = (P_PARAM_CUSTOM_MCR_RW_STRUCT_T) pvSetBuffer;

	ACQUIRE_POWER_CONTROL_FROM_PM(prAdapter);
	HAL_MCR_WR(prAdapter, (prMcrWrInfo->u4McrOffset & BITS(2, 31)), prMcrWrInfo->u4McrData);

	DBGLOG(INIT, TRACE, "DRV MCR Write: Offset = %#08x, Data = %#08x\n",
	       prMcrWrInfo->u4McrOffset, prMcrWrInfo->u4McrData);

	return WLAN_STATUS_SUCCESS;
}				/* wlanoidSetMcrWrite */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query SW CTRL
*
* \param[in] pvAdapter Pointer to the Adapter structure.
* \param[out] pvQueryBuf A pointer to the buffer that holds the result of
*                           the query.
* \param[in] u4QueryBufLen The length of the query buffer.
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                            bytes written into the query buffer. If the call
*                            failed due to invalid length of the query buffer,
*                            returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQuerySwCtrlRead(IN P_ADAPTER_T prAdapter,
		       IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	P_PARAM_CUSTOM_SW_CTRL_STRUCT_T prSwCtrlInfo;
	WLAN_STATUS rWlanStatus;
	UINT_16 u2Id, u2SubId;
	UINT_32 u4Data;

	CMD_SW_DBG_CTRL_T rCmdSwCtrl;

	DEBUGFUNC("wlanoidQuerySwCtrlRead");
	DBGLOG(INIT, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(PARAM_CUSTOM_SW_CTRL_STRUCT_T);

	if (u4QueryBufferLen < sizeof(PARAM_CUSTOM_SW_CTRL_STRUCT_T))
		return WLAN_STATUS_INVALID_LENGTH;

	prSwCtrlInfo = (P_PARAM_CUSTOM_SW_CTRL_STRUCT_T) pvQueryBuffer;

	u2Id = (UINT_16) (prSwCtrlInfo->u4Id >> 16);
	u2SubId = (UINT_16) (prSwCtrlInfo->u4Id & BITS(0, 15));
	u4Data = 0;
	rWlanStatus = WLAN_STATUS_SUCCESS;

	switch (u2Id) {
		/* 0x9000 - 0x9EFF reserved for FW */
		/* 0xFFFE          reserved for FW */

#if CFG_SUPPORT_SWCR
	case 0x9F00:
		swCrReadWriteCmd(prAdapter, SWCR_READ /* Read */,
				 (UINT_16) u2SubId, &u4Data);
		break;
#endif /* CFG_SUPPORT_SWCR */

	case 0xFFFF:
		{
			u4Data = 0x5AA56620;
		}
		break;

	case 0xBABA:
		switch ((u2SubId >> 8) & BITS(0, 7)) {
		case 0x00:
			/* Dump Tx resource and queue status */
			qmDumpQueueStatus(prAdapter, NULL, 0);
			cnmDumpMemoryStatus(prAdapter, NULL, 0);
			break;

		case 0x01:
			/* Dump StaRec info by index */
			cnmDumpStaRec(prAdapter, (UINT_8) (u2SubId & BITS(0, 7)));
			break;

		case 0x02:
			/* Dump BSS info by index */
			bssDumpBssInfo(prAdapter, (UINT_8) (u2SubId & BITS(0, 7)));
			break;

		case 0x03:
			/*Dump BSS statistics by index */
			wlanDumpBssStatistics(prAdapter, (UINT_8) (u2SubId & BITS(0, 7)));
			break;

		case 0x04:
			halDumpHifStatus(prAdapter, NULL, 0);
			break;

		default:
			break;
		}

		u4Data = 0xBABABABA;
		break;

	case 0x9000:
	default:
		{
			rCmdSwCtrl.u4Id = prSwCtrlInfo->u4Id;
			rCmdSwCtrl.u4Data = 0;
			rWlanStatus = wlanSendSetQueryCmd(prAdapter,
							  CMD_ID_SW_DBG_CTRL,
							  FALSE,
							  TRUE,
							  g_fgIsOid,
							  nicCmdEventQuerySwCtrlRead,
							  nicOidCmdTimeoutCommon,
							  sizeof(CMD_SW_DBG_CTRL_T),
							  (PUINT_8) &rCmdSwCtrl, pvQueryBuffer, u4QueryBufferLen);
			return rWlanStatus;
		}
	}			/* switch(u2Id) */

	prSwCtrlInfo->u4Data = u4Data;

	return rWlanStatus;

}

 /* end of wlanoidQuerySwCtrlRead() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to write SW CTRL
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[in] pvSetBuffer A pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                           bytes read from the set buffer. If the call failed
*                           due to invalid length of the set buffer, returns
*                           the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetSwCtrlWrite(IN P_ADAPTER_T prAdapter,
		      IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_PARAM_CUSTOM_SW_CTRL_STRUCT_T prSwCtrlInfo;
	CMD_SW_DBG_CTRL_T rCmdSwCtrl;
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;
	UINT_16 u2Id, u2SubId;
	UINT_32 u4Data;
	UINT_8 ucNss;
	UINT_8 ucChannelWidth;
	UINT_8 ucBssIndex;

	DEBUGFUNC("wlanoidSetSwCtrlWrite");
	DBGLOG(INIT, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(PARAM_CUSTOM_SW_CTRL_STRUCT_T);

	if (u4SetBufferLen < sizeof(PARAM_CUSTOM_SW_CTRL_STRUCT_T))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prSwCtrlInfo = (P_PARAM_CUSTOM_SW_CTRL_STRUCT_T) pvSetBuffer;

	u2Id = (UINT_16) (prSwCtrlInfo->u4Id >> 16);
	u2SubId = (UINT_16) (prSwCtrlInfo->u4Id & BITS(0, 15));
	u4Data = prSwCtrlInfo->u4Data;

	switch (u2Id) {

		/* 0x9000 - 0x9EFF reserved for FW */
		/* 0xFFFE          reserved for FW */

#if CFG_SUPPORT_SWCR
	case 0x9F00:
		swCrReadWriteCmd(prAdapter, SWCR_WRITE, (UINT_16) u2SubId, &u4Data);
		break;
#endif /* CFG_SUPPORT_SWCR */

	case 0x2222:
		ucNss = (UINT_8)(u4Data & BITS(0, 3));
		ucChannelWidth = (UINT_8)((u4Data & BITS(4, 7)) >> 4);
		ucBssIndex = (UINT_8) u2SubId;

		/* ucChannelWidth 0:20MHz, 1:40MHz, 2:80MHz, 3:160MHz 4:80+80MHz */
		DBGLOG(REQ, INFO, "Change BSS[%d] OpMode to BW[%d] Nss[%d]\n",
			ucBssIndex, ucChannelWidth, ucNss);
		rlmChangeOperationMode(prAdapter, ucBssIndex, ucChannelWidth, ucNss);
		break;

	case 0x1000:
		if (u2SubId == 0x8000) {
			/* CTIA power save mode setting (code: 0x10008000) */
			prAdapter->u4CtiaPowerMode = u4Data;
			prAdapter->fgEnCtiaPowerMode = TRUE;

			/*  */
			{
				PARAM_POWER_MODE ePowerMode;

				if (prAdapter->u4CtiaPowerMode == 0)
					/* force to keep in CAM mode */
					ePowerMode = Param_PowerModeCAM;
				else if (prAdapter->u4CtiaPowerMode == 1)
					ePowerMode = Param_PowerModeMAX_PSP;
				else
					ePowerMode = Param_PowerModeFast_PSP;

				nicConfigPowerSaveProfile(prAdapter,
				  prAdapter->prAisBssInfo->ucBssIndex,
				  ePowerMode, g_fgIsOid);
			}
		}
		break;
	case 0x1001:
		if (u2SubId == 0x0)
			prAdapter->fgEnOnlineScan = (BOOLEAN) u4Data;
		else if (u2SubId == 0x1)
			prAdapter->fgDisBcnLostDetection = (BOOLEAN) u4Data;
		else if (u2SubId == 0x2)
			prAdapter->rWifiVar.ucUapsd = (BOOLEAN) u4Data;
		else if (u2SubId == 0x3) {
			prAdapter->u4UapsdAcBmp = u4Data & BITS(0, 15);
			GET_BSS_INFO_BY_INDEX(prAdapter,
					      u4Data >> 16)->rPmProfSetupInfo.ucBmpDeliveryAC =
			    (UINT_8) prAdapter->u4UapsdAcBmp;
			GET_BSS_INFO_BY_INDEX(prAdapter,
					      u4Data >> 16)->rPmProfSetupInfo.ucBmpTriggerAC =
			    (UINT_8) prAdapter->u4UapsdAcBmp;
		} else if (u2SubId == 0x4)
			prAdapter->fgDisStaAgingTimeoutDetection = (BOOLEAN) u4Data;
		else if (u2SubId == 0x5)
			prAdapter->rWifiVar.rConnSettings.uc2G4BandwidthMode = (UINT_8) u4Data;
		else if (u2SubId == 0x0100) {
			if (u4Data == 2)
				prAdapter->rWifiVar.ucRxGf = FEATURE_DISABLED;
			else
				prAdapter->rWifiVar.ucRxGf = FEATURE_ENABLED;
		} else if (u2SubId == 0x0101)
			prAdapter->rWifiVar.ucRxShortGI = (UINT_8) u4Data;
		else if (u2SubId == 0x0110) {
			prAdapter->fgIsEnableLpdvt = (BOOLEAN) u4Data;
			prAdapter->fgEnOnlineScan = (BOOLEAN) u4Data;
			DBGLOG(INIT, INFO, "--- Enable LPDVT [%d] ---\n", prAdapter->fgIsEnableLpdvt);
		}

		break;

#if CFG_SUPPORT_SWCR
	case 0x1002:
#if CFG_RX_PKTS_DUMP
		if (u2SubId == 0x0) {
			if (u4Data)
				u4Data = BIT(HIF_RX_PKT_TYPE_MANAGEMENT);
			swCrFrameCheckEnable(prAdapter, u4Data);
		}
#endif
		if (u2SubId == 0x1) {
			BOOLEAN fgIsEnable;
			UINT_8 ucType;
			UINT_32 u4Timeout;

			fgIsEnable = (BOOLEAN) (u4Data & 0xff);
			ucType = 0;	/* ((u4Data>>4) & 0xf); */
			u4Timeout = ((u4Data >> 8) & 0xff);
			swCrDebugCheckEnable(prAdapter, fgIsEnable, ucType, u4Timeout);
		}
		break;
#endif

#if CFG_SUPPORT_802_11W
	case 0x2000:
		DBGLOG(RSN, INFO, "802.11w test 0x%x\n", u2SubId);
		if (u2SubId == 0x0)
			rsnStartSaQuery(prAdapter);
		if (u2SubId == 0x1)
			rsnStopSaQuery(prAdapter);
		if (u2SubId == 0x2)
			rsnSaQueryRequest(prAdapter, NULL);
		if (u2SubId == 0x3) {
			P_BSS_INFO_T prBssInfo = prAdapter->prAisBssInfo;

			authSendDeauthFrame(prAdapter, prBssInfo, prBssInfo->prStaRecOfAP, NULL, 7, NULL);
		}
		/* wext_set_mode */
		/*
		 *  if (u2SubId == 0x3) {
		 *  prAdapter->prGlueInfo->rWpaInfo.u4Mfp = RSN_AUTH_MFP_DISABLED;
		 *  }
		 *  if (u2SubId == 0x4) {
		 *  //prAdapter->rWifiVar.rAisSpecificBssInfo.fgMgmtProtection = TRUE;
		 *  prAdapter->prGlueInfo->rWpaInfo.u4Mfp = RSN_AUTH_MFP_OPTIONAL;
		 *  }
		 *  if (u2SubId == 0x5) {
		 *  //prAdapter->rWifiVar.rAisSpecificBssInfo.fgMgmtProtection = TRUE;
		 *  prAdapter->prGlueInfo->rWpaInfo.u4Mfp = RSN_AUTH_MFP_REQUIRED;
		 *  }
		 */
		break;
#endif
	case 0xFFFF:
		{
			/* CMD_ACCESS_REG rCmdAccessReg; */
#if 1				/* CFG_MT6573_SMT_TEST */
			if (u2SubId == 0x0123) {

				DBGLOG(HAL, INFO,
						"set smt fixed rate: %u\n",
						u4Data);

				if ((ENUM_REGISTRY_FIXED_RATE_T) (u4Data) < FIXED_RATE_NUM)
					prAdapter->rWifiVar.eRateSetting = (ENUM_REGISTRY_FIXED_RATE_T) (u4Data);
				else
					prAdapter->rWifiVar.eRateSetting = FIXED_RATE_NONE;

				if (prAdapter->rWifiVar.eRateSetting == FIXED_RATE_NONE)
					/* Enable Auto (Long/Short) Preamble */
					prAdapter->rWifiVar.ePreambleType = PREAMBLE_TYPE_AUTO;
				else if ((prAdapter->rWifiVar.eRateSetting >= FIXED_RATE_MCS0_20M_400NS &&
					  prAdapter->rWifiVar.eRateSetting <= FIXED_RATE_MCS7_20M_400NS)
					 || (prAdapter->rWifiVar.eRateSetting >= FIXED_RATE_MCS0_40M_400NS &&
					     prAdapter->rWifiVar.eRateSetting <= FIXED_RATE_MCS32_400NS))
					/* Force Short Preamble */
					prAdapter->rWifiVar.ePreambleType = PREAMBLE_TYPE_SHORT;
				else
					/* Force Long Preamble */
					prAdapter->rWifiVar.ePreambleType = PREAMBLE_TYPE_LONG;

				/* abort to re-connect */
#if 1
				kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
							     WLAN_STATUS_MEDIA_DISCONNECT, NULL, 0);
#else
				aisBssBeaconTimeout(prAdapter);
#endif

				return WLAN_STATUS_SUCCESS;

			} else if (u2SubId == 0x1234) {
				/* 1. Disable On-Lin Scan */
				/* 3. Disable FIFO FULL no ack */
				/* 4. Disable Roaming */
				/* Disalbe auto tx power */
				/* 2. Keep at CAM mode */
				/* 5. Disable Beacon Timeout Detection */
				rWlanStatus = nicEnterCtiaMode(prAdapter, TRUE, TRUE);
			} else if (u2SubId == 0x1235) {
				/* 1. Enaable On-Lin Scan */
				/* 3. Enable FIFO FULL no ack */
				/* 4. Enable Roaming */
				/* Enable auto tx power */
				/* 2. Keep at Fast PS */
				/* 5. Enable Beacon Timeout Detection */
				rWlanStatus = nicEnterCtiaMode(prAdapter, FALSE, TRUE);
			}
#endif
#if CFG_MTK_STAGE_SCAN
			else if (u2SubId == 0x1250)
				prAdapter->aePreferBand[KAL_NETWORK_TYPE_AIS_INDEX] = BAND_NULL;
			else if (u2SubId == 0x1251)
				prAdapter->aePreferBand[KAL_NETWORK_TYPE_AIS_INDEX] = BAND_2G4;
			else if (u2SubId == 0x1252) {
				if (prAdapter->fgEnable5GBand)
					prAdapter->aePreferBand[KAL_NETWORK_TYPE_AIS_INDEX] = BAND_5G;
				else
					/* Skip this setting if 5G band is disabled */
					DBGLOG(SCN, INFO, "Skip 5G stage scan request due to 5G is disabled\n");
			}
#endif
		}
		break;

	case 0x9000:
	default:
		{
			rCmdSwCtrl.u4Id = prSwCtrlInfo->u4Id;
			rCmdSwCtrl.u4Data = prSwCtrlInfo->u4Data;
			rWlanStatus = wlanSendSetQueryCmd(prAdapter,
							  CMD_ID_SW_DBG_CTRL,
							  TRUE,
							  FALSE,
							  g_fgIsOid,
							  nicCmdEventSetCommon,
							  nicOidCmdTimeoutCommon,
							  sizeof(CMD_SW_DBG_CTRL_T),
							  (PUINT_8) &rCmdSwCtrl, pvSetBuffer, u4SetBufferLen);
		}
	}			/* switch(u2Id)  */

	return rWlanStatus;
}				/* wlanoidSetSwCtrlWrite */

WLAN_STATUS
wlanoidQueryChipConfig(IN P_ADAPTER_T prAdapter,
		       IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	P_PARAM_CUSTOM_CHIP_CONFIG_STRUCT_T prChipConfigInfo;
	CMD_CHIP_CONFIG_T rCmdChipConfig;
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidQuerySwCtrlRead");
	DBGLOG(INIT, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(PARAM_CUSTOM_CHIP_CONFIG_STRUCT_T);

	if (u4QueryBufferLen < sizeof(PARAM_CUSTOM_CHIP_CONFIG_STRUCT_T))
		return WLAN_STATUS_INVALID_LENGTH;

	prChipConfigInfo = (P_PARAM_CUSTOM_CHIP_CONFIG_STRUCT_T) pvQueryBuffer;
	kalMemZero(&rCmdChipConfig, sizeof(rCmdChipConfig));

	rCmdChipConfig.u2Id = prChipConfigInfo->u2Id;
	rCmdChipConfig.ucType = prChipConfigInfo->ucType;
	rCmdChipConfig.ucRespType = prChipConfigInfo->ucRespType;
	rCmdChipConfig.u2MsgSize = prChipConfigInfo->u2MsgSize;
	if (rCmdChipConfig.u2MsgSize > CHIP_CONFIG_RESP_SIZE) {
		DBGLOG(REQ, INFO, "Chip config Msg Size %u is not valid (query)\n", rCmdChipConfig.u2MsgSize);
		rCmdChipConfig.u2MsgSize = CHIP_CONFIG_RESP_SIZE;
	}
	kalMemCopy(rCmdChipConfig.aucCmd, prChipConfigInfo->aucCmd, rCmdChipConfig.u2MsgSize);

	rWlanStatus = wlanSendSetQueryCmd(prAdapter,
					  CMD_ID_CHIP_CONFIG, FALSE,
					  TRUE, g_fgIsOid,
					  /*nicCmdEventQuerySwCtrlRead, */
					  nicCmdEventQueryChipConfig,
					  nicOidCmdTimeoutCommon,
					  sizeof(CMD_CHIP_CONFIG_T),
					  (PUINT_8) &rCmdChipConfig, pvQueryBuffer, u4QueryBufferLen);

	return rWlanStatus;

}

 /* end of wlanoidQueryChipConfig() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set chip
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[in] pvSetBuffer A pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                           bytes read from the set buffer. If the call failed
*                           due to invalid length of the set buffer, returns
*                           the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetChipConfig(IN P_ADAPTER_T prAdapter,
		     IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_PARAM_CUSTOM_CHIP_CONFIG_STRUCT_T prChipConfigInfo;
	CMD_CHIP_CONFIG_T rCmdChipConfig;
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;

	DATA_STRUCT_INSPECTING_ASSERT(sizeof(prChipConfigInfo->aucCmd) == CHIP_CONFIG_RESP_SIZE);
	DEBUGFUNC("wlanoidSetChipConfig");
	DBGLOG(INIT, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(PARAM_CUSTOM_CHIP_CONFIG_STRUCT_T);

	if (u4SetBufferLen < sizeof(PARAM_CUSTOM_CHIP_CONFIG_STRUCT_T))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prChipConfigInfo = (P_PARAM_CUSTOM_CHIP_CONFIG_STRUCT_T) pvSetBuffer;
	kalMemZero(&rCmdChipConfig, sizeof(rCmdChipConfig));

	rCmdChipConfig.u2Id = prChipConfigInfo->u2Id;
	rCmdChipConfig.ucType = prChipConfigInfo->ucType;
	rCmdChipConfig.ucRespType = prChipConfigInfo->ucRespType;
	rCmdChipConfig.u2MsgSize = prChipConfigInfo->u2MsgSize;
	if (rCmdChipConfig.u2MsgSize > CHIP_CONFIG_RESP_SIZE) {
		DBGLOG(REQ, INFO, "Chip config Msg Size %u is not valid (set)\n", rCmdChipConfig.u2MsgSize);
		rCmdChipConfig.u2MsgSize = CHIP_CONFIG_RESP_SIZE;
	}
	kalMemCopy(rCmdChipConfig.aucCmd, prChipConfigInfo->aucCmd, rCmdChipConfig.u2MsgSize);

	rWlanStatus = wlanSendSetQueryCmd(prAdapter,
					  CMD_ID_CHIP_CONFIG,
					  TRUE,
					  FALSE,
					  g_fgIsOid,
					  nicCmdEventSetCommon,
					  nicOidCmdTimeoutCommon,
					  sizeof(CMD_CHIP_CONFIG_T),
					  (PUINT_8) &rCmdChipConfig, pvSetBuffer, u4SetBufferLen);

	return rWlanStatus;
}				/* wlanoidSetChipConfig */






VOID
wlanLoadDefaultCustomerSetting(IN P_ADAPTER_T prAdapter) {

	UINT_8 ucItemNum, i;


	ucItemNum = (sizeof(g_rDefaulteSetting) / sizeof(PARAM_CUSTOM_KEY_CFG_STRUCT_T));
	DBGLOG(INIT, INFO, "[wlanLoadDefaultCustomerSetting] default firmware setting %d item\n", ucItemNum);


	for (i = 0; i < ucItemNum; i++) {
		wlanCfgSet(prAdapter, g_rDefaulteSetting[i].aucKey, g_rDefaulteSetting[i].aucValue, 0);
		DBGLOG(INIT, INFO, "%s with %s\n", g_rDefaulteSetting[i].aucKey, g_rDefaulteSetting[i].aucValue);
	}

#if 1
	/*If need to re-parsing , included wlanInitFeatureOption*/
	wlanInitFeatureOption(prAdapter);
#endif

}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set cfg and callback
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[in] pvSetBuffer A pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                           bytes read from the set buffer. If the call failed
*                           due to invalid length of the set buffer, returns
*                           the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetKeyCfg(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;
	P_PARAM_CUSTOM_KEY_CFG_STRUCT_T prKeyCfgInfo;

	DEBUGFUNC("wlanoidSetKeyCfg");
	DBGLOG(INIT, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(PARAM_CUSTOM_KEY_CFG_STRUCT_T);

	if (u4SetBufferLen < sizeof(PARAM_CUSTOM_KEY_CFG_STRUCT_T))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);
	prKeyCfgInfo = (P_PARAM_CUSTOM_KEY_CFG_STRUCT_T) pvSetBuffer;

	if (kalMemCmp(prKeyCfgInfo->aucKey, "reload", 6) == 0)
		wlanGetConfig(prAdapter); /* Reload config file */
	else
		wlanCfgSet(prAdapter, prKeyCfgInfo->aucKey, prKeyCfgInfo->aucValue, 0);

	wlanInitFeatureOption(prAdapter);
#if CFG_SUPPORT_EASY_DEBUG
	wlanFeatureToFw(prAdapter);
#endif

	return rWlanStatus;
}

   /* wlanoidSetSwCtrlWrite */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query EEPROM value.
*
* \param[in] pvAdapter Pointer to the Adapter structure.
* \param[out] pvQueryBuf A pointer to the buffer that holds the result of
*                           the query.
* \param[in] u4QueryBufLen The length of the query buffer.
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                            bytes written into the query buffer. If the call
*                            failed due to invalid length of the query buffer,
*                            returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_FAILURE
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryEepromRead(IN P_ADAPTER_T prAdapter,
		       IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	P_PARAM_CUSTOM_EEPROM_RW_STRUCT_T prEepromRwInfo;
	CMD_ACCESS_EEPROM rCmdAccessEeprom;

	DEBUGFUNC("wlanoidQueryEepromRead");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(PARAM_CUSTOM_EEPROM_RW_STRUCT_T);

	if (u4QueryBufferLen < sizeof(PARAM_CUSTOM_EEPROM_RW_STRUCT_T))
		return WLAN_STATUS_INVALID_LENGTH;

	prEepromRwInfo = (P_PARAM_CUSTOM_EEPROM_RW_STRUCT_T) pvQueryBuffer;

	kalMemZero(&rCmdAccessEeprom, sizeof(CMD_ACCESS_EEPROM));
	rCmdAccessEeprom.u2Offset = prEepromRwInfo->ucEepromIndex;

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_ACCESS_EEPROM,
				   FALSE,
				   TRUE,
				   g_fgIsOid,
				   nicCmdEventQueryEepromRead,
				   nicOidCmdTimeoutCommon,
				   sizeof(CMD_ACCESS_EEPROM),
				   (PUINT_8) &rCmdAccessEeprom, pvQueryBuffer, u4QueryBufferLen);

}				/* wlanoidQueryEepromRead */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to write EEPROM value.
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[in] pvSetBuffer A pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                           bytes read from the set buffer. If the call failed
*                           due to invalid length of the set buffer, returns
*                           the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_FAILURE
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetEepromWrite(IN P_ADAPTER_T prAdapter,
		      IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_PARAM_CUSTOM_EEPROM_RW_STRUCT_T prEepromRwInfo;
	CMD_ACCESS_EEPROM rCmdAccessEeprom;

	DEBUGFUNC("wlanoidSetEepromWrite");
	DBGLOG(INIT, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(PARAM_CUSTOM_EEPROM_RW_STRUCT_T);

	if (u4SetBufferLen < sizeof(PARAM_CUSTOM_EEPROM_RW_STRUCT_T))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prEepromRwInfo = (P_PARAM_CUSTOM_EEPROM_RW_STRUCT_T) pvSetBuffer;

	kalMemZero(&rCmdAccessEeprom, sizeof(CMD_ACCESS_EEPROM));
	rCmdAccessEeprom.u2Offset = prEepromRwInfo->ucEepromIndex;
	rCmdAccessEeprom.u2Data = prEepromRwInfo->u2EepromData;

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_ACCESS_EEPROM,
				   TRUE,
				   FALSE,
				   TRUE,
				   nicCmdEventSetCommon,
				   nicOidCmdTimeoutCommon,
				   sizeof(CMD_ACCESS_EEPROM),
				   (PUINT_8) &rCmdAccessEeprom, pvSetBuffer, u4SetBufferLen);

}				/* wlanoidSetEepromWrite */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the number of the successfully transmitted
*        packets.
*
* \param[in] pvAdapter Pointer to the Adapter structure.
* \param[out] pvQueryBuf A pointer to the buffer that holds the result of
*                           the query.
* \param[in] u4QueryBufLen The length of the query buffer.
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                            bytes written into the query buffer. If the call
*                            failed due to invalid length of the query buffer,
*                            returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryXmitOk(IN P_ADAPTER_T prAdapter,
		   IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	DEBUGFUNC("wlanoidQueryXmitOk");
	DBGLOG(REQ, LOUD, "\n");

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in query receive error! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		*pu4QueryInfoLen = sizeof(UINT_32);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (u4QueryBufferLen < sizeof(UINT_32)
		   || (u4QueryBufferLen > sizeof(UINT_32) && u4QueryBufferLen < sizeof(UINT_64))) {
		*pu4QueryInfoLen = sizeof(UINT_64);
		return WLAN_STATUS_INVALID_LENGTH;
	}
#if CFG_ENABLE_STATISTICS_BUFFERING
	if (IsBufferedStatisticsUsable(prAdapter) == TRUE) {
		if (u4QueryBufferLen == sizeof(UINT_32)) {
			*pu4QueryInfoLen = sizeof(UINT_32);
			*(PUINT_32) pvQueryBuffer = (UINT_32) prAdapter->rStatStruct.rTransmittedFragmentCount.QuadPart;
		} else {
			*pu4QueryInfoLen = sizeof(UINT_64);
			*(PUINT_64) pvQueryBuffer = (UINT_64) prAdapter->rStatStruct.rTransmittedFragmentCount.QuadPart;
		}

		return WLAN_STATUS_SUCCESS;
	}
#endif

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_GET_STATISTICS,
				   FALSE,
				   TRUE,
				   TRUE,
				   nicCmdEventQueryXmitOk,
				   nicOidCmdTimeoutCommon, 0, NULL, pvQueryBuffer, u4QueryBufferLen);

}				/* wlanoidQueryXmitOk */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the number of the successfully received
*        packets.
*
* \param[in] pvAdapter Pointer to the Adapter structure.
* \param[out] pvQueryBuf A pointer to the buffer that holds the result of
*                           the query.
* \param[in] u4QueryBufLen The length of the query buffer.
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                            bytes written into the query buffer. If the call
*                            failed due to invalid length of the query buffer,
*                            returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryRcvOk(IN P_ADAPTER_T prAdapter,
		  IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	DEBUGFUNC("wlanoidQueryRcvOk");
	DBGLOG(REQ, LOUD, "\n");

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in query receive error! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		*pu4QueryInfoLen = sizeof(UINT_32);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (u4QueryBufferLen < sizeof(UINT_32)
		   || (u4QueryBufferLen > sizeof(UINT_32) && u4QueryBufferLen < sizeof(UINT_64))) {
		*pu4QueryInfoLen = sizeof(UINT_64);
		return WLAN_STATUS_INVALID_LENGTH;
	}
#if CFG_ENABLE_STATISTICS_BUFFERING
	if (IsBufferedStatisticsUsable(prAdapter) == TRUE) {
		if (u4QueryBufferLen == sizeof(UINT_32)) {
			*pu4QueryInfoLen = sizeof(UINT_32);
			*(PUINT_32) pvQueryBuffer = (UINT_32) prAdapter->rStatStruct.rReceivedFragmentCount.QuadPart;
		} else {
			*pu4QueryInfoLen = sizeof(UINT_64);
			*(PUINT_64) pvQueryBuffer = (UINT_64) prAdapter->rStatStruct.rReceivedFragmentCount.QuadPart;
		}

		return WLAN_STATUS_SUCCESS;
	}
#endif

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_GET_STATISTICS,
				   FALSE,
				   TRUE,
				   TRUE,
				   nicCmdEventQueryRecvOk,
				   nicOidCmdTimeoutCommon, 0, NULL, pvQueryBuffer, u4QueryBufferLen);

}				/* wlanoidQueryRcvOk */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the number of frames that the driver
*        fails to transmit.
*
* \param[in] pvAdapter Pointer to the Adapter structure.
* \param[out] pvQueryBuf A pointer to the buffer that holds the result of
*                           the query.
* \param[in] u4QueryBufLen The length of the query buffer.
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                            bytes written into the query buffer. If the call
*                            failed due to invalid length of the query buffer,
*                            returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryXmitError(IN P_ADAPTER_T prAdapter,
		      IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	DEBUGFUNC("wlanoidQueryXmitError");
	DBGLOG(REQ, LOUD, "\n");

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in query receive error! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		*pu4QueryInfoLen = sizeof(UINT_32);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (u4QueryBufferLen < sizeof(UINT_32)
		   || (u4QueryBufferLen > sizeof(UINT_32) && u4QueryBufferLen < sizeof(UINT_64))) {
		*pu4QueryInfoLen = sizeof(UINT_64);
		return WLAN_STATUS_INVALID_LENGTH;
	}
#if CFG_ENABLE_STATISTICS_BUFFERING
	if (IsBufferedStatisticsUsable(prAdapter) == TRUE) {
		if (u4QueryBufferLen == sizeof(UINT_32)) {
			*pu4QueryInfoLen = sizeof(UINT_32);
			*(PUINT_32) pvQueryBuffer = (UINT_32) prAdapter->rStatStruct.rFailedCount.QuadPart;
		} else {
			*pu4QueryInfoLen = sizeof(UINT_64);
			*(PUINT_64) pvQueryBuffer = (UINT_64) prAdapter->rStatStruct.rFailedCount.QuadPart;
		}

		return WLAN_STATUS_SUCCESS;
	}
#endif

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_GET_STATISTICS,
				   FALSE,
				   TRUE,
				   TRUE,
				   nicCmdEventQueryXmitError,
				   nicOidCmdTimeoutCommon, 0, NULL, pvQueryBuffer, u4QueryBufferLen);

}				/* wlanoidQueryXmitError */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the number of frames successfully
*        transmitted after exactly one collision.
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
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryXmitOneCollision(IN P_ADAPTER_T prAdapter,
			     IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	DEBUGFUNC("wlanoidQueryXmitOneCollision");
	DBGLOG(REQ, LOUD, "\n");

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in query receive error! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		*pu4QueryInfoLen = sizeof(UINT_32);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (u4QueryBufferLen < sizeof(UINT_32)
		   || (u4QueryBufferLen > sizeof(UINT_32) && u4QueryBufferLen < sizeof(UINT_64))) {
		*pu4QueryInfoLen = sizeof(UINT_64);
		return WLAN_STATUS_INVALID_LENGTH;
	}
#if CFG_ENABLE_STATISTICS_BUFFERING
	if (IsBufferedStatisticsUsable(prAdapter) == TRUE) {
		if (u4QueryBufferLen == sizeof(UINT_32)) {
			*pu4QueryInfoLen = sizeof(UINT_32);
			*(PUINT_32) pvQueryBuffer = (UINT_32)
			    (prAdapter->rStatStruct.rMultipleRetryCount.QuadPart -
			     prAdapter->rStatStruct.rRetryCount.QuadPart);
		} else {
			*pu4QueryInfoLen = sizeof(UINT_64);
			*(PUINT_64) pvQueryBuffer = (UINT_64)
			    (prAdapter->rStatStruct.rMultipleRetryCount.QuadPart -
			     prAdapter->rStatStruct.rRetryCount.QuadPart);
		}

		return WLAN_STATUS_SUCCESS;
	}
#endif

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_GET_STATISTICS,
				   FALSE,
				   TRUE,
				   TRUE,
				   nicCmdEventQueryXmitOneCollision,
				   nicOidCmdTimeoutCommon, 0, NULL, pvQueryBuffer, u4QueryBufferLen);

}				/* wlanoidQueryXmitOneCollision */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the number of frames successfully
*        transmitted after more than one collision.
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
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryXmitMoreCollisions(IN P_ADAPTER_T prAdapter,
			       IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	DEBUGFUNC("wlanoidQueryXmitMoreCollisions");
	DBGLOG(REQ, LOUD, "\n");

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in query receive error! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		*pu4QueryInfoLen = sizeof(UINT_32);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (u4QueryBufferLen < sizeof(UINT_32)
		   || (u4QueryBufferLen > sizeof(UINT_32) && u4QueryBufferLen < sizeof(UINT_64))) {
		*pu4QueryInfoLen = sizeof(UINT_64);
		return WLAN_STATUS_INVALID_LENGTH;
	}
#if CFG_ENABLE_STATISTICS_BUFFERING
	if (IsBufferedStatisticsUsable(prAdapter) == TRUE) {
		if (u4QueryBufferLen == sizeof(UINT_32)) {
			*pu4QueryInfoLen = sizeof(UINT_32);
			*(PUINT_32) pvQueryBuffer = (UINT_32) (prAdapter->rStatStruct.rMultipleRetryCount.QuadPart);
		} else {
			*pu4QueryInfoLen = sizeof(UINT_64);
			*(PUINT_64) pvQueryBuffer = (UINT_64) (prAdapter->rStatStruct.rMultipleRetryCount.QuadPart);
		}

		return WLAN_STATUS_SUCCESS;
	}
#endif

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_GET_STATISTICS,
				   FALSE,
				   TRUE,
				   TRUE,
				   nicCmdEventQueryXmitMoreCollisions,
				   nicOidCmdTimeoutCommon, 0, NULL, pvQueryBuffer, u4QueryBufferLen);

}				/* wlanoidQueryXmitMoreCollisions */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the number of frames
*                not transmitted due to excessive collisions.
*
* \param[in] prAdapter          Pointer to the Adapter structure.
* \param[in] pvQueryBuffer      Pointer to the buffer that holds the result of
*                               the query.
* \param[in] u4QueryBufferLen   The length of the query buffer.
* \param[out] pu4QueryInfoLen   If the call is successful, returns the number of
*                               bytes written into the query buffer. If the call
*                               failed due to invalid length of the query buffer,
*                               returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryXmitMaxCollisions(IN P_ADAPTER_T prAdapter,
			      IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	DEBUGFUNC("wlanoidQueryXmitMaxCollisions");
	DBGLOG(REQ, LOUD, "\n");

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in query receive error! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		*pu4QueryInfoLen = sizeof(UINT_32);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (u4QueryBufferLen < sizeof(UINT_32)
		   || (u4QueryBufferLen > sizeof(UINT_32) && u4QueryBufferLen < sizeof(UINT_64))) {
		*pu4QueryInfoLen = sizeof(UINT_64);
		return WLAN_STATUS_INVALID_LENGTH;
	}
#if CFG_ENABLE_STATISTICS_BUFFERING
	if (IsBufferedStatisticsUsable(prAdapter) == TRUE) {
		if (u4QueryBufferLen == sizeof(UINT_32)) {
			*pu4QueryInfoLen = sizeof(UINT_32);
			*(PUINT_32) pvQueryBuffer = (UINT_32) prAdapter->rStatStruct.rFailedCount.QuadPart;
		} else {
			*pu4QueryInfoLen = sizeof(UINT_64);
			*(PUINT_64) pvQueryBuffer = (UINT_64) prAdapter->rStatStruct.rFailedCount.QuadPart;
		}

		return WLAN_STATUS_SUCCESS;
	}
#endif

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_GET_STATISTICS,
				   FALSE,
				   TRUE,
				   TRUE,
				   nicCmdEventQueryXmitMaxCollisions,
				   nicOidCmdTimeoutCommon, 0, NULL, pvQueryBuffer, u4QueryBufferLen);

}				/* wlanoidQueryXmitMaxCollisions */

#define MTK_CUSTOM_OID_INTERFACE_VERSION     0x00006620	/* for WPDWifi DLL */
/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query current the OID interface version,
*        which is the interface between the application and driver.
*
* \param[in] prAdapter          Pointer to the Adapter structure.
* \param[in] pvQueryBuffer      Pointer to the buffer that holds the result of
*                               the query.
* \param[in] u4QueryBufferLen   The length of the query buffer.
* \param[out] pu4QueryInfoLen   If the call is successful, returns the number of
*                               bytes written into the query buffer. If the call
*                               failed due to invalid length of the query buffer,
*                               returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryOidInterfaceVersion(IN P_ADAPTER_T prAdapter,
				IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	DEBUGFUNC("wlanoidQueryOidInterfaceVersion");

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	*(PUINT_32) pvQueryBuffer = MTK_CUSTOM_OID_INTERFACE_VERSION;
	*pu4QueryInfoLen = sizeof(UINT_32);

	DBGLOG(REQ, WARN, "Custom OID interface version: %#08X\n",
			*(PUINT_32) pvQueryBuffer);

	return WLAN_STATUS_SUCCESS;
}				/* wlanoidQueryOidInterfaceVersion */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query current Multicast Address List.
*
* \param[in] prAdapter          Pointer to the Adapter structure.
* \param[in] pvQueryBuffer      Pointer to the buffer that holds the result of
*                               the query.
* \param[in] u4QueryBufferLen   The length of the query buffer.
* \param[out] pu4QueryInfoLen   If the call is successful, returns the number of
*                               bytes written into the query buffer. If the call
*                               failed due to invalid length of the query buffer,
*                               returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_BUFFER_TOO_SHORT
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryMulticastList(IN P_ADAPTER_T prAdapter,
			  OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
#ifndef LINUX
	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_MAC_MCAST_ADDR,
				   FALSE,
				   TRUE,
				   TRUE,
				   nicCmdEventQueryMcastAddr,
				   nicOidCmdTimeoutCommon, 0, NULL, pvQueryBuffer, u4QueryBufferLen);
#else
	return WLAN_STATUS_SUCCESS;
#endif
}				/* end of wlanoidQueryMulticastList() */

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
wlanoidSetMulticastList(IN P_ADAPTER_T prAdapter,
			IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	CMD_MAC_MCAST_ADDR rCmdMacMcastAddr;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	/* The data must be a multiple of the Ethernet address size. */
	if ((u4SetBufferLen % MAC_ADDR_LEN)) {
		DBGLOG(REQ, WARN,
			"Invalid MC list length %u\n", u4SetBufferLen);

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
	rCmdMacMcastAddr.ucBssIndex = prAdapter->prAisBssInfo->ucBssIndex;
	kalMemCopy(rCmdMacMcastAddr.arAddress, pvSetBuffer, u4SetBufferLen);

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_MAC_MCAST_ADDR,
				   TRUE,
				   FALSE,
				   g_fgIsOid,
				   nicCmdEventSetCommon,
				   nicOidCmdTimeoutCommon,
				   sizeof(CMD_MAC_MCAST_ADDR),
				   (PUINT_8) &rCmdMacMcastAddr, pvSetBuffer, u4SetBufferLen);
}				/* end of wlanoidSetMulticastList() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set Packet Filter.
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
* \retval WLAN_STATUS_NOT_SUPPORTED
* \retval WLAN_STATUS_ADAPTER_NOT_READY
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetCurrentPacketFilter(IN P_ADAPTER_T prAdapter,
			      IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	UINT_32 u4NewPacketFilter;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	WLAN_STATUS rResult = WLAN_STATUS_FAILURE;
	CMD_RX_PACKET_FILTER rSetRxPacketFilter;

	DBGLOG(REQ, INFO, "wlanoidSetCurrentPacketFilter");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	if (u4SetBufferLen < sizeof(UINT_32)) {
		*pu4SetInfoLen = sizeof(UINT_32);
		return WLAN_STATUS_INVALID_LENGTH;
	}
	ASSERT(pvSetBuffer);

	/* Set the new packet filter. */
	u4NewPacketFilter = *(PUINT_32) pvSetBuffer;

	DBGLOG(REQ, INFO, "New packet filter: %#08x\n", u4NewPacketFilter);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in set current packet filter! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	do {
		/* Verify the bits of the new packet filter. If any bits are set that
		 *  we don't support, leave.
		 */
		if (u4NewPacketFilter & ~(PARAM_PACKET_FILTER_SUPPORTED)) {
			rStatus = WLAN_STATUS_NOT_SUPPORTED;
			DBGLOG(REQ, WARN, "some flags we don't support\n");
			break;
		}
#if DBG
		/* Need to enable or disable promiscuous support depending on the new
		 *  filter.
		 */
		if (u4NewPacketFilter & PARAM_PACKET_FILTER_PROMISCUOUS)
			DBGLOG(REQ, INFO, "Enable promiscuous mode\n");
		else
			DBGLOG(REQ, INFO, "Disable promiscuous mode\n");

		if (u4NewPacketFilter & PARAM_PACKET_FILTER_ALL_MULTICAST)
			DBGLOG(REQ, INFO, "Enable all-multicast mode\n");
		else if (u4NewPacketFilter & PARAM_PACKET_FILTER_MULTICAST)
			DBGLOG(REQ, INFO, "Enable multicast\n");
		else
			DBGLOG(REQ, INFO, "Disable multicast\n");

		if (u4NewPacketFilter & PARAM_PACKET_FILTER_BROADCAST)
			DBGLOG(REQ, INFO, "Enable Broadcast\n");
		else
			DBGLOG(REQ, INFO, "Disable Broadcast\n");
#endif

		prAdapter->fgAllMulicastFilter = FALSE;
		if (u4NewPacketFilter & PARAM_PACKET_FILTER_ALL_MULTICAST)
			prAdapter->fgAllMulicastFilter = TRUE;
	} while (FALSE);

	if (rStatus == WLAN_STATUS_SUCCESS) {
		/* Store the packet filter */

		prAdapter->u4OsPacketFilter &= PARAM_PACKET_FILTER_P2P_MASK;
		prAdapter->u4OsPacketFilter |= u4NewPacketFilter;

		rSetRxPacketFilter.u4RxPacketFilter = prAdapter->u4OsPacketFilter;

		rResult = wlanSendSetQueryCmd(prAdapter,
					   CMD_ID_SET_RX_FILTER,
					   TRUE,
					   FALSE,
					   g_fgIsOid,
					   nicCmdEventSetCommon,
					   nicOidCmdTimeoutCommon,
					   sizeof(CMD_RX_PACKET_FILTER),
					   (PUINT_8) &rSetRxPacketFilter, pvSetBuffer, u4SetBufferLen);
		prAdapter->u4OsPacketFilter = rSetRxPacketFilter.u4RxPacketFilter;
		return rResult;
	} else {
		return rStatus;
	}
}				/* wlanoidSetCurrentPacketFilter */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query current packet filter.
*
* \param[in] prAdapter          Pointer to the Adapter structure.
* \param[in] pvQueryBuffer      Pointer to the buffer that holds the result of
*                               the query.
* \param[in] u4QueryBufferLen   The length of the query buffer.
* \param[out] pu4QueryInfoLen   If the call is successful, returns the number of
*                               bytes written into the query buffer. If the call
*                               failed due to invalid length of the query buffer,
*                               returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryCurrentPacketFilter(IN P_ADAPTER_T prAdapter,
				OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	DEBUGFUNC("wlanoidQueryCurrentPacketFilter");
	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);

	*pu4QueryInfoLen = sizeof(UINT_32);

	if (u4QueryBufferLen >= sizeof(UINT_32)) {
		ASSERT(pvQueryBuffer);
		*(PUINT_32) pvQueryBuffer = prAdapter->u4OsPacketFilter;
	}

	return WLAN_STATUS_SUCCESS;
}				/* wlanoidQueryCurrentPacketFilter */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query ACPI device power state.
*
* \param[in] prAdapter          Pointer to the Adapter structure.
* \param[in] pvQueryBuffer      Pointer to the buffer that holds the result of
*                               the query.
* \param[in] u4QueryBufferLen   The length of the query buffer.
* \param[out] pu4QueryInfoLen   If the call is successful, returns the number of
*                               bytes written into the query buffer. If the call
*                               failed due to invalid length of the query buffer,
*                               returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryAcpiDevicePowerState(IN P_ADAPTER_T prAdapter,
				 IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
#if DBG
	PPARAM_DEVICE_POWER_STATE prPowerState;
#endif

	DEBUGFUNC("wlanoidQueryAcpiDevicePowerState");
	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(PARAM_DEVICE_POWER_STATE);

#if DBG
	prPowerState = (PPARAM_DEVICE_POWER_STATE) pvQueryBuffer;
	switch (*prPowerState) {
	case ParamDeviceStateD0:
		DBGLOG(REQ, INFO, "Query Power State: D0\n");
		break;
	case ParamDeviceStateD1:
		DBGLOG(REQ, INFO, "Query Power State: D1\n");
		break;
	case ParamDeviceStateD2:
		DBGLOG(REQ, INFO, "Query Power State: D2\n");
		break;
	case ParamDeviceStateD3:
		DBGLOG(REQ, INFO, "Query Power State: D3\n");
		break;
	default:
		break;
	}
#endif

	/* Since we will disconnect the newwork, therefore we do not
	 *  need to check queue empty
	 */
	*(PPARAM_DEVICE_POWER_STATE) pvQueryBuffer = ParamDeviceStateD3;
	/* WARNLOG(("Ready to transition to D3\n")); */
	return WLAN_STATUS_SUCCESS;

}				/* pwrmgtQueryPower */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set ACPI device power state.
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
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetAcpiDevicePowerState(IN P_ADAPTER_T prAdapter,
			       IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	PPARAM_DEVICE_POWER_STATE prPowerState;
	BOOLEAN fgRetValue = TRUE;

	DEBUGFUNC("wlanoidSetAcpiDevicePowerState");
	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(PARAM_DEVICE_POWER_STATE);

	ASSERT(pvSetBuffer);
	prPowerState = (PPARAM_DEVICE_POWER_STATE) pvSetBuffer;
	switch (*prPowerState) {
	case ParamDeviceStateD0:
		DBGLOG(REQ, INFO, "Set Power State: D0\n");
		kalDevSetPowerState(prAdapter->prGlueInfo, (UINT_32) ParamDeviceStateD0);
		fgRetValue = nicpmSetAcpiPowerD0(prAdapter);
		break;
	case ParamDeviceStateD1:
		DBGLOG(REQ, INFO, "Set Power State: D1\n");
		/* no break here */
	case ParamDeviceStateD2:
		DBGLOG(REQ, INFO, "Set Power State: D2\n");
		/* no break here */
	case ParamDeviceStateD3:
		DBGLOG(REQ, INFO, "Set Power State: D3\n");
		fgRetValue = nicpmSetAcpiPowerD3(prAdapter);
		kalDevSetPowerState(prAdapter->prGlueInfo, (UINT_32) ParamDeviceStateD3);
		break;
	default:
		break;
	}

	if (fgRetValue == TRUE)
		return WLAN_STATUS_SUCCESS;
	else
		return WLAN_STATUS_FAILURE;
}				/* end of wlanoidSetAcpiDevicePowerState() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the current fragmentation threshold.
*
* \param[in] pvAdapter Pointer to the Adapter structure.
* \param[out] pvQueryBuffer A pointer to the buffer that holds the result of
*                           the query.
* \param[in] u4QueryBufferLen The length of the query buffer.
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                            bytes written into the query buffer. If the call
*                            failed due to invalid length of the query buffer,
*                            returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_BUFFER_TOO_SHORT
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryFragThreshold(IN P_ADAPTER_T prAdapter,
			  OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	DEBUGFUNC("wlanoidQueryFragThreshold");

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	DBGLOG(REQ, LOUD, "\n");

#if CFG_TX_FRAGMENT

	return WLAN_STATUS_SUCCESS;

#else

	return WLAN_STATUS_NOT_SUPPORTED;
#endif /* CFG_TX_FRAGMENT */

}				/* end of wlanoidQueryFragThreshold() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set a new fragmentation threshold to the
*        driver.
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
* \retval WLAN_STATUS_INVALID_DATA
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetFragThreshold(IN P_ADAPTER_T prAdapter,
			IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
#if CFG_TX_FRAGMENT

	return WLAN_STATUS_SUCCESS;

#else

	return WLAN_STATUS_NOT_SUPPORTED;
#endif /* CFG_TX_FRAGMENT */

}				/* end of wlanoidSetFragThreshold() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the current RTS threshold.
*
* \param[in] pvAdapter Pointer to the Adapter structure.
* \param[out] pvQueryBuffer A pointer to the buffer that holds the result of
*                           the query.
* \param[in] u4QueryBufferLen The length of the query buffer.
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                            bytes written into the query buffer. If the call
*                            failed due to invalid length of the query buffer,
*                            returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_BUFFER_TOO_SHORT
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryRtsThreshold(IN P_ADAPTER_T prAdapter,
			 OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	DEBUGFUNC("wlanoidQueryRtsThreshold");

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	DBGLOG(REQ, LOUD, "\n");

	if (u4QueryBufferLen < sizeof(PARAM_RTS_THRESHOLD)) {
		*pu4QueryInfoLen = sizeof(PARAM_RTS_THRESHOLD);
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}

	*((PARAM_RTS_THRESHOLD *) pvQueryBuffer) = prAdapter->rWlanInfo.eRtsThreshold;

	return WLAN_STATUS_SUCCESS;

}				/* wlanoidQueryRtsThreshold */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set a new RTS threshold to the driver.
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
* \retval WLAN_STATUS_INVALID_DATA
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetRtsThreshold(IN P_ADAPTER_T prAdapter,
		       IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	PARAM_RTS_THRESHOLD *prRtsThreshold;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(PARAM_RTS_THRESHOLD);
	if (u4SetBufferLen < sizeof(PARAM_RTS_THRESHOLD)) {
		DBGLOG(REQ, WARN, "Invalid length %u\n", u4SetBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	prRtsThreshold = (PARAM_RTS_THRESHOLD *) pvSetBuffer;
	*prRtsThreshold = prAdapter->rWlanInfo.eRtsThreshold;

	return WLAN_STATUS_SUCCESS;

}				/* wlanoidSetRtsThreshold */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to turn radio off.
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
* \retval WLAN_STATUS_ADAPTER_NOT_READY
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetDisassociate(IN P_ADAPTER_T prAdapter,
		       IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_MSG_AIS_ABORT_T prAisAbortMsg;

	DEBUGFUNC("wlanoidSetDisassociate");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = 0;

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in set disassociate! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	/* prepare message to AIS */
	prAdapter->rWifiVar.rConnSettings.fgIsConnReqIssued = FALSE;

	/* Send AIS Abort Message */
	prAisAbortMsg = (P_MSG_AIS_ABORT_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_AIS_ABORT_T));
	if (!prAisAbortMsg) {
		DBGLOG(REQ, ERROR, "Fail in creating AisAbortMsg.\n");
		return WLAN_STATUS_FAILURE;
	}

	prAisAbortMsg->rMsgHdr.eMsgId = MID_OID_AIS_FSM_JOIN_REQ;
	prAisAbortMsg->ucReasonOfDisconnect = DISCONNECT_REASON_CODE_NEW_CONNECTION;
	prAisAbortMsg->fgDelayIndication = FALSE;

#if CFG_DISCONN_DEBUG_FEATURE
	/* used to disconnect debug capability */
	g_rDisconnInfoTemp.ucTrigger = DISCONNECT_TRIGGER_ACTIVE;
#endif

	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prAisAbortMsg, MSG_SEND_METHOD_BUF);

	/* indicate for disconnection */
	if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) == PARAM_MEDIA_STATE_CONNECTED)
		kalIndicateStatusAndComplete(prAdapter->prGlueInfo, WLAN_STATUS_MEDIA_DISCONNECT_LOCALLY, NULL, 0);
#if !defined(LINUX)
	prAdapter->fgIsRadioOff = TRUE;
#endif

	return WLAN_STATUS_SUCCESS;
}				/* wlanoidSetDisassociate */

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
wlanoidQuery802dot11PowerSaveProfile(IN P_ADAPTER_T prAdapter,
				     IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	DEBUGFUNC("wlanoidQuery802dot11PowerSaveProfile");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);

	if (u4QueryBufferLen != 0) {
		ASSERT(pvQueryBuffer);

/* *(PPARAM_POWER_MODE) pvQueryBuffer = (PARAM_POWER_MODE)(prAdapter->rWlanInfo.ePowerSaveMode.ucPsProfile); */
		*(PPARAM_POWER_MODE) pvQueryBuffer =
		    (PARAM_POWER_MODE) (prAdapter->rWlanInfo.arPowerSaveMode[prAdapter->prAisBssInfo->ucBssIndex].
					ucPsProfile);
		*pu4QueryInfoLen = sizeof(PARAM_POWER_MODE);

		/* hack for CTIA power mode setting function */
		if (prAdapter->fgEnCtiaPowerMode) {
			/* set to non-zero value (to prevent MMI query 0, */
			/* before it intends to set 0, which will skip its following state machine) */
			*(PPARAM_POWER_MODE) pvQueryBuffer = (PARAM_POWER_MODE) 2;
		}
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
wlanoidSet802dot11PowerSaveProfile(IN P_ADAPTER_T prAdapter,
				   IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	WLAN_STATUS status;
	P_PARAM_POWER_MODE_T prPowerMode;
	P_BSS_INFO_T prBssInfo;

	const PUINT_8 apucPsMode[Param_PowerModeMax] = {
		(PUINT_8) "CAM",
		(PUINT_8) "MAX PS",
		(PUINT_8) "FAST PS"
	};

	DEBUGFUNC("wlanoidSet802dot11PowerSaveProfile");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(PARAM_POWER_MODE_T);
	prPowerMode = (P_PARAM_POWER_MODE_T) pvSetBuffer;

	if (u4SetBufferLen < sizeof(PARAM_POWER_MODE_T)) {
		DBGLOG(REQ, WARN,
			"Set power mode error: Invalid length %u\n",
			u4SetBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	} else if (prPowerMode->ePowerMode >= Param_PowerModeMax) {
		DBGLOG(REQ, WARN, "Set power mode error: Invalid power mode(%u)\n", prPowerMode->ePowerMode);
		return WLAN_STATUS_INVALID_DATA;
	} else if (prPowerMode->ucBssIdx >= BSS_INFO_NUM) {
		DBGLOG(REQ, WARN, "Set power mode error: Invalid BSS index(%u)\n", prPowerMode->ucBssIdx);
		return WLAN_STATUS_INVALID_DATA;
	}

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prPowerMode->ucBssIdx);

	if (prAdapter->fgEnCtiaPowerMode) {
		if (prPowerMode->ePowerMode != Param_PowerModeCAM) {
			/* User setting to PS mode (Param_PowerModeMAX_PSP or Param_PowerModeFast_PSP) */

			if (prAdapter->u4CtiaPowerMode == 0)
				/* force to keep in CAM mode */
				prPowerMode->ePowerMode = Param_PowerModeCAM;
			else if (prAdapter->u4CtiaPowerMode == 1)
				prPowerMode->ePowerMode = Param_PowerModeMAX_PSP;
			else if (prAdapter->u4CtiaPowerMode == 2)
				prPowerMode->ePowerMode = Param_PowerModeFast_PSP;
		}
	}

	/* only CAM mode allowed when TP/Sigma on */
	if ((prAdapter->rWifiVar.ucTpTestMode == ENUM_TP_TEST_MODE_THROUGHPUT) ||
		(prAdapter->rWifiVar.ucTpTestMode == ENUM_TP_TEST_MODE_SIGMA_AC_N_PMF))
		prPowerMode->ePowerMode = Param_PowerModeCAM;

	/* for WMM PS Sigma certification, keep WiFi in ps mode continuously */
	/* force PS == Param_PowerModeMAX_PSP */
	if ((prAdapter->rWifiVar.ucTpTestMode == ENUM_TP_TEST_MODE_SIGMA_WMM_PS) &&
		(prPowerMode->ePowerMode >= Param_PowerModeMAX_PSP))
		prPowerMode->ePowerMode = Param_PowerModeMAX_PSP;

	status = nicConfigPowerSaveProfile(prAdapter,
		prPowerMode->ucBssIdx, prPowerMode->ePowerMode, g_fgIsOid);

	if (prPowerMode->ePowerMode < Param_PowerModeMax) {
		DBGLOG(INIT, INFO, "Set %s Network BSS(%u) PS mode to %s (%d)\n",
		       apucNetworkType[prBssInfo->eNetworkType],
		       prPowerMode->ucBssIdx, apucPsMode[prPowerMode->ePowerMode], prPowerMode->ePowerMode);
	} else {
		DBGLOG(INIT, INFO, "Invalid PS mode setting (%d) for %s Network BSS(%u)\n",
		       prPowerMode->ePowerMode, apucNetworkType[prBssInfo->eNetworkType], prPowerMode->ucBssIdx);
	}

	return status;

}				/* end of wlanoidSetAcpiDevicePowerStateMode() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query current status of AdHoc Mode.
*
* \param[in] prAdapter          Pointer to the Adapter structure.
* \param[in] pvQueryBuffer      Pointer to the buffer that holds the result of
*                               the query.
* \param[in] u4QueryBufferLen   The length of the query buffer.
* \param[out] pu4QueryInfoLen   If the call is successful, returns the number of
*                               bytes written into the query buffer. If the call
*                               failed due to invalid length of the query buffer,
*                               returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_BUFFER_TOO_SHORT
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryAdHocMode(IN P_ADAPTER_T prAdapter,
		      OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	return WLAN_STATUS_SUCCESS;
}				/* end of wlanoidQueryAdHocMode() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set AdHoc Mode.
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
* \retval WLAN_STATUS_INVALID_DATA
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetAdHocMode(IN P_ADAPTER_T prAdapter,
		    IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	return WLAN_STATUS_SUCCESS;
}				/* end of wlanoidSetAdHocMode() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query RF frequency.
*
* \param[in] prAdapter          Pointer to the Adapter structure.
* \param[in] pvQueryBuffer      Pointer to the buffer that holds the result of
*                               the query.
* \param[in] u4QueryBufferLen   The length of the query buffer.
* \param[out] pu4QueryInfoLen   If the call is successful, returns the number of
*                               bytes written into the query buffer. If the call
*                               failed due to invalid length of the query buffer,
*                               returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_BUFFER_TOO_SHORT
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryFrequency(IN P_ADAPTER_T prAdapter,
		      OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	DEBUGFUNC("wlanoidQueryFrequency");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (u4QueryBufferLen < sizeof(UINT_32))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (prAdapter->rWifiVar.rConnSettings.eOPMode == NET_TYPE_INFRA) {
		if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) == PARAM_MEDIA_STATE_CONNECTED)
			*(PUINT_32) pvQueryBuffer = nicChannelNum2Freq(prAdapter->prAisBssInfo->ucPrimaryChannel);
		else
			*(PUINT_32) pvQueryBuffer = 0;
	} else
		*(PUINT_32) pvQueryBuffer = nicChannelNum2Freq(prAdapter->rWifiVar.rConnSettings.ucAdHocChannelNum);

	return WLAN_STATUS_SUCCESS;
}				/* end of wlanoidQueryFrequency() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set RF frequency by User Settings.
*
* \param[in] prAdapter          Pointer to the Adapter structure.
* \param[in] pvQueryBuffer      Pointer to the buffer that holds the result of
*                               the query.
* \param[in] u4QueryBufferLen   The length of the query buffer.
* \param[out] pu4QueryInfoLen   If the call is successful, returns the number of
*                               bytes written into the query buffer. If the call
*                               failed due to invalid length of the query buffer,
*                               returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
* \retval WLAN_STATUS_INVALID_DATA
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetFrequency(IN P_ADAPTER_T prAdapter,
		    IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	PUINT_32 pu4FreqInKHz;

	DEBUGFUNC("wlanoidSetFrequency");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(UINT_32);

	if (u4SetBufferLen < sizeof(UINT_32))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);
	pu4FreqInKHz = (PUINT_32) pvSetBuffer;

	prAdapter->rWifiVar.rConnSettings.ucAdHocChannelNum = (UINT_8) nicFreq2ChannelNum(*pu4FreqInKHz);
	prAdapter->rWifiVar.rConnSettings.eAdHocBand = *pu4FreqInKHz < 5000000 ? BAND_2G4 : BAND_5G;

	return WLAN_STATUS_SUCCESS;
}				/* end of wlanoidSetFrequency() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set 802.11 channel of the radio frequency.
*        This is a proprietary function call to Lunux currently.
*
* \param[in] prAdapter      Pointer to the Adapter structure.
* \param[in] pvSetBuffer A pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                          bytes read from the set buffer. If the call failed
*                          due to invalid length of the set buffer, returns
*                          the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
* \retval WLAN_STATUS_INVALID_DATA
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetChannel(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	ASSERT(0);		/* // */

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the Beacon Interval from User Settings.
*
* \param[in] prAdapter          Pointer to the Adapter structure.
* \param[in] pvQueryBuffer      Pointer to the buffer that holds the result of
*                               the query.
* \param[in] u4QueryBufferLen   The length of the query buffer.
* \param[out] pu4QueryInfoLen   If the call is successful, returns the number of
*                               bytes written into the query buffer. If the call
*                               failed due to invalid length of the query buffer,
*                               returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_BUFFER_TOO_SHORT
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryBeaconInterval(IN P_ADAPTER_T prAdapter,
			   OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	DEBUGFUNC("wlanoidQueryBeaconInterval");
	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(UINT_32);

	if (u4QueryBufferLen < sizeof(UINT_32))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) == PARAM_MEDIA_STATE_CONNECTED) {
		if (prAdapter->rWifiVar.rConnSettings.eOPMode == NET_TYPE_INFRA)
			*(PUINT_32) pvQueryBuffer = prAdapter->rWlanInfo.rCurrBssId.rConfiguration.u4BeaconPeriod;
		else
			*(PUINT_32) pvQueryBuffer = (UINT_32) prAdapter->rWlanInfo.u2BeaconPeriod;
	} else {
		if (prAdapter->rWifiVar.rConnSettings.eOPMode == NET_TYPE_INFRA)
			*(PUINT_32) pvQueryBuffer = 0;
		else
			*(PUINT_32) pvQueryBuffer = (UINT_32) prAdapter->rWlanInfo.u2BeaconPeriod;
	}

	return WLAN_STATUS_SUCCESS;
}				/* end of wlanoidQueryBeaconInterval() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set the Beacon Interval to User Settings.
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
* \retval WLAN_STATUS_INVALID_DATA
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetBeaconInterval(IN P_ADAPTER_T prAdapter,
			 IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	PUINT_32 pu4BeaconInterval;

	DEBUGFUNC("wlanoidSetBeaconInterval");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(UINT_32);
	if (u4SetBufferLen < sizeof(UINT_32))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);
	pu4BeaconInterval = (PUINT_32) pvSetBuffer;

	if ((*pu4BeaconInterval < DOT11_BEACON_PERIOD_MIN) || (*pu4BeaconInterval > DOT11_BEACON_PERIOD_MAX)) {
		DBGLOG(REQ, TRACE, "Invalid Beacon Interval = %u\n",
				*pu4BeaconInterval);
		return WLAN_STATUS_INVALID_DATA;
	}

	prAdapter->rWlanInfo.u2BeaconPeriod = (UINT_16) *pu4BeaconInterval;

	DBGLOG(REQ, INFO, "Set beacon interval: %d\n", prAdapter->rWlanInfo.u2BeaconPeriod);

	return WLAN_STATUS_SUCCESS;
}				/* end of wlanoidSetBeaconInterval() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the ATIM window from User Settings.
*
* \param[in] prAdapter          Pointer to the Adapter structure.
* \param[in] pvQueryBuffer      Pointer to the buffer that holds the result of
*                               the query.
* \param[in] u4QueryBufferLen   The length of the query buffer.
* \param[out] pu4QueryInfoLen   If the call is successful, returns the number of
*                               bytes written into the query buffer. If the call
*                               failed due to invalid length of the query buffer,
*                               returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_BUFFER_TOO_SHORT
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryAtimWindow(IN P_ADAPTER_T prAdapter,
		       OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	DEBUGFUNC("wlanoidQueryAtimWindow");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(UINT_32);

	if (u4QueryBufferLen < sizeof(UINT_32))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (prAdapter->rWifiVar.rConnSettings.eOPMode == NET_TYPE_INFRA)
		*(PUINT_32) pvQueryBuffer = 0;
	else
		*(PUINT_32) pvQueryBuffer = (UINT_32) prAdapter->rWlanInfo.u2AtimWindow;

	return WLAN_STATUS_SUCCESS;

}				/* end of wlanoidQueryAtimWindow() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set the ATIM window to User Settings.
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
* \retval WLAN_STATUS_INVALID_DATA
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetAtimWindow(IN P_ADAPTER_T prAdapter,
		     IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	PUINT_32 pu4AtimWindow;

	DEBUGFUNC("wlanoidSetAtimWindow");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(UINT_32);

	if (u4SetBufferLen < sizeof(UINT_32))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);
	pu4AtimWindow = (PUINT_32) pvSetBuffer;

	prAdapter->rWlanInfo.u2AtimWindow = (UINT_16) *pu4AtimWindow;

	return WLAN_STATUS_SUCCESS;
}				/* end of wlanoidSetAtimWindow() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to Set the MAC address which is currently used by the NIC.
*
* \param[in] prAdapter      Pointer to the Adapter structure.
* \param[in] pvSetBuffer    Pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                           bytes read from the set buffer. If the call failed
*                           due to invalid length of the set buffer, returns
*                           the amount of storage needed.
*
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
* \retval WLAN_STATUS_INVALID_DATA
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetCurrentAddr(IN P_ADAPTER_T prAdapter,
		      IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	ASSERT(0);		/* // */

	return WLAN_STATUS_SUCCESS;
}				/* end of wlanoidSetCurrentAddr() */

#if CFG_TCP_IP_CHKSUM_OFFLOAD
/*----------------------------------------------------------------------------*/
/*!
* \brief Setting the checksum offload function.
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
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetCSUMOffload(IN P_ADAPTER_T prAdapter,
		      IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	UINT_32 u4CSUMFlags;
	CMD_BASIC_CONFIG_T rCmdBasicConfig;
	P_WIFI_VAR_T prWifiVar = &prAdapter->rWifiVar;

	DEBUGFUNC("wlanoidSetCSUMOffload");
	DBGLOG(INIT, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(UINT_32);

	if (u4SetBufferLen < sizeof(UINT_32))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);
	u4CSUMFlags = *(PUINT_32) pvSetBuffer;

	kalMemZero(&rCmdBasicConfig, sizeof(CMD_BASIC_CONFIG_T));

	rCmdBasicConfig.ucNative80211 = 0;	/* @FIXME: for Vista */

	if (u4CSUMFlags & CSUM_OFFLOAD_EN_TX_TCP)
		rCmdBasicConfig.rCsumOffload.u2TxChecksum |= BIT(2);

	if (u4CSUMFlags & CSUM_OFFLOAD_EN_TX_UDP)
		rCmdBasicConfig.rCsumOffload.u2TxChecksum |= BIT(1);

	if (u4CSUMFlags & CSUM_OFFLOAD_EN_TX_IP)
		rCmdBasicConfig.rCsumOffload.u2TxChecksum |= BIT(0);

	if (u4CSUMFlags & CSUM_OFFLOAD_EN_RX_TCP)
		rCmdBasicConfig.rCsumOffload.u2RxChecksum |= BIT(2);

	if (u4CSUMFlags & CSUM_OFFLOAD_EN_RX_UDP)
		rCmdBasicConfig.rCsumOffload.u2RxChecksum |= BIT(1);

	if (u4CSUMFlags & (CSUM_OFFLOAD_EN_RX_IPv4 | CSUM_OFFLOAD_EN_RX_IPv6))
		rCmdBasicConfig.rCsumOffload.u2RxChecksum |= BIT(0);

	prAdapter->u4CSUMFlags = u4CSUMFlags;
	rCmdBasicConfig.ucCtrlFlagAssertPath = prWifiVar->ucCtrlFlagAssertPath;
	rCmdBasicConfig.ucCtrlFlagDebugLevel = prWifiVar->ucCtrlFlagDebugLevel;

	 wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_BASIC_CONFIG,
				   TRUE,
				   FALSE,
				   g_fgIsOid,
				   NULL,
				   nicOidCmdTimeoutCommon,
				   sizeof(CMD_BASIC_CONFIG_T),
				   (PUINT_8) &rCmdBasicConfig, pvSetBuffer, u4SetBufferLen);

	 return WLAN_STATUS_SUCCESS;
}
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */

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
wlanoidSetNetworkAddress(IN P_ADAPTER_T prAdapter,
			 IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 i, u4IPv4AddrIdx;
	P_CMD_SET_NETWORK_ADDRESS_LIST prCmdNetworkAddressList;
	P_PARAM_NETWORK_ADDRESS_LIST prNetworkAddressList = (P_PARAM_NETWORK_ADDRESS_LIST) pvSetBuffer;
	P_PARAM_NETWORK_ADDRESS prNetworkAddress;
	UINT_32 u4IPv4AddrCount, u4CmdSize;
#if CFG_ENABLE_GTK_FRAME_FILTER
	UINT_32 u4IpV4AddrListSize;
	P_BSS_INFO_T prBssInfo = &prAdapter->rWifiVar.arBssInfoPool[KAL_NETWORK_TYPE_AIS_INDEX];
#endif

	DEBUGFUNC("wlanoidSetNetworkAddress");
	DBGLOG(INIT, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = 4;

	if (u4SetBufferLen < OFFSET_OF(PARAM_NETWORK_ADDRESS_LIST, arAddress))
		return WLAN_STATUS_INVALID_DATA;

	*pu4SetInfoLen = 0;
	u4IPv4AddrCount = 0;

	/* 4 <1.1> Get IPv4 address count */
	/* We only suppot IPv4 address setting */
	prNetworkAddress = prNetworkAddressList->arAddress;
	for (i = 0; i < prNetworkAddressList->u4AddressCount; i++) {
		if ((prNetworkAddress->u2AddressType == PARAM_PROTOCOL_ID_TCP_IP) &&
		    (prNetworkAddress->u2AddressLength == IPV4_ADDR_LEN)) {
			u4IPv4AddrCount++;
		}

		prNetworkAddress = (P_PARAM_NETWORK_ADDRESS) ((ULONG) prNetworkAddress +
							      (ULONG) (prNetworkAddress->u2AddressLength +
								       OFFSET_OF(PARAM_NETWORK_ADDRESS, aucAddress)));
	}

	/* 4 <2> Calculate command buffer size */
	/* construct payload of command packet */
	if (u4IPv4AddrCount == 0)
		u4CmdSize = sizeof(CMD_SET_NETWORK_ADDRESS_LIST);
	else
		u4CmdSize = OFFSET_OF(CMD_SET_NETWORK_ADDRESS_LIST, arNetAddress) +
		    (sizeof(IPV4_NETWORK_ADDRESS) * u4IPv4AddrCount);

	/* 4 <3> Allocate command buffer */
	prCmdNetworkAddressList = (P_CMD_SET_NETWORK_ADDRESS_LIST) kalMemAlloc(u4CmdSize, VIR_MEM_TYPE);

	if (prCmdNetworkAddressList == NULL)
		return WLAN_STATUS_FAILURE;

#if CFG_ENABLE_GTK_FRAME_FILTER
	u4IpV4AddrListSize = OFFSET_OF(IPV4_NETWORK_ADDRESS_LIST, arNetAddr) +
	    (u4IPv4AddrCount * sizeof(IPV4_NETWORK_ADDRESS));
	if (prBssInfo->prIpV4NetAddrList)
		FREE_IPV4_NETWORK_ADDR_LIST(prBssInfo->prIpV4NetAddrList);
	prBssInfo->prIpV4NetAddrList = (P_IPV4_NETWORK_ADDRESS_LIST) kalMemAlloc(u4IpV4AddrListSize, VIR_MEM_TYPE);
	prBssInfo->prIpV4NetAddrList->ucAddrCount = (UINT_8) u4IPv4AddrCount;
#endif

	/* 4 <4> Fill P_CMD_SET_NETWORK_ADDRESS_LIST */
	prCmdNetworkAddressList->ucBssIndex = prNetworkAddressList->ucBssIdx;

	/* only to set IP address to FW once ARP filter is enabled */
	if (prAdapter->fgEnArpFilter) {
		prCmdNetworkAddressList->ucAddressCount = (UINT_8) u4IPv4AddrCount;
		prNetworkAddress = prNetworkAddressList->arAddress;

		/* DBGLOG(INIT, INFO, ("%s: u4IPv4AddrCount (%lu)\n", __FUNCTION__, u4IPv4AddrCount)); */

		for (i = 0, u4IPv4AddrIdx = 0; i < prNetworkAddressList->u4AddressCount; i++) {
			if (prNetworkAddress->u2AddressType == PARAM_PROTOCOL_ID_TCP_IP &&
			    prNetworkAddress->u2AddressLength == IPV4_ADDR_LEN) {

				kalMemCopy(prCmdNetworkAddressList->arNetAddress[u4IPv4AddrIdx].aucIpAddr,
					   prNetworkAddress->aucAddress, sizeof(UINT_32));

#if CFG_ENABLE_GTK_FRAME_FILTER
				kalMemCopy(prBssInfo->prIpV4NetAddrList->arNetAddr[u4IPv4AddrIdx].aucIpAddr,
					   prNetworkAddress->aucAddress, sizeof(UINT_32));
#endif

				DBGLOG(INIT, INFO,
				       "%s: IPv4 Addr [%u][" IPV4STR "]\n", __func__,
				       u4IPv4AddrIdx, IPV4TOSTR(prNetworkAddress->aucAddress));

				u4IPv4AddrIdx++;
			}

			prNetworkAddress = (P_PARAM_NETWORK_ADDRESS) ((ULONG) prNetworkAddress +
								      (ULONG) (prNetworkAddress->u2AddressLength +
									       OFFSET_OF(PARAM_NETWORK_ADDRESS,
											 aucAddress)));
		}

	} else {
		prCmdNetworkAddressList->ucAddressCount = 0;
	}

	DBGLOG(INIT, INFO,
	       "%s: Set %u IPv4 address for BSS[%u]\n",
		   __func__, u4IPv4AddrCount,
	       prCmdNetworkAddressList->ucBssIndex);

	/* 4 <5> Send command */
	rStatus = wlanSendSetQueryCmd(prAdapter,
				      CMD_ID_SET_IP_ADDRESS,
				      TRUE,
				      FALSE,
				      g_fgIsOid,
				      nicCmdEventSetIpAddress,
				      nicOidCmdTimeoutCommon,
				      u4CmdSize, (PUINT_8) prCmdNetworkAddressList, pvSetBuffer, u4SetBufferLen);

	kalMemFree(prCmdNetworkAddressList, VIR_MEM_TYPE, u4CmdSize);
	return rStatus;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Set driver to switch into RF test mode
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[in] pvSetBuffer A pointer to the buffer that holds the data to be set,
*                        should be NULL
* \param[in] u4SetBufferLen The length of the set buffer, should be 0
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                           bytes read from the set buffer. If the call failed
*                           due to invalid length of the set buffer, returns
*                           the amount of storage needed.
*
* \return WLAN_STATUS_SUCCESS
* \return WLAN_STATUS_ADAPTER_NOT_READY
* \return WLAN_STATUS_INVALID_DATA
* \return WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidRftestSetTestMode(IN P_ADAPTER_T prAdapter,
			 IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	WLAN_STATUS rStatus;
	CMD_TEST_CTRL_T rCmdTestCtrl;

	DEBUGFUNC("wlanoidRftestSetTestMode");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = 0;

	if (u4SetBufferLen == 0) {
		if ((prAdapter->fgTestMode == FALSE)
		    || (prAdapter->fgIcapMode == TRUE)) {
			/* switch to RF Test mode */
			rCmdTestCtrl.ucAction = 0;	/* Switch mode */
			rCmdTestCtrl.u.u4OpMode = 1;	/* RF test mode */

			rStatus = wlanSendSetQueryCmd(prAdapter,
						      CMD_ID_TEST_CTRL,
						      TRUE,
						      FALSE,
						      g_fgIsOid,
						      nicCmdEventEnterRfTest,
						      nicOidCmdEnterRFTestTimeout,
						      sizeof(CMD_TEST_CTRL_T),
						      (PUINT_8) &rCmdTestCtrl, pvSetBuffer, u4SetBufferLen);
		} else {
			/* already in test mode .. */
			rStatus = WLAN_STATUS_SUCCESS;
		}
	} else {
		rStatus = WLAN_STATUS_INVALID_DATA;
	}

	return rStatus;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Set driver to switch into RF test ICAP mode
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[in] pvSetBuffer A pointer to the buffer that holds the data to be set,
*                        should be NULL
* \param[in] u4SetBufferLen The length of the set buffer, should be 0
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                           bytes read from the set buffer. If the call failed
*                           due to invalid length of the set buffer, returns
*                           the amount of storage needed.
*
* \return WLAN_STATUS_SUCCESS
* \return WLAN_STATUS_ADAPTER_NOT_READY
* \return WLAN_STATUS_INVALID_DATA
* \return WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidRftestSetTestIcapMode(IN P_ADAPTER_T prAdapter,
			     IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	WLAN_STATUS rStatus;
	CMD_TEST_CTRL_T rCmdTestCtrl;

	DEBUGFUNC("wlanoidRftestSetTestIcapMode");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = 0;

	if (u4SetBufferLen == 0) {
		if (prAdapter->fgIcapMode == FALSE) {
			/* switch to RF Test mode */
			rCmdTestCtrl.ucAction = 0;	/* Switch mode */
			rCmdTestCtrl.u.u4OpMode = 2;	/* RF test mode */

			rStatus = wlanSendSetQueryCmd(prAdapter,
						      CMD_ID_TEST_CTRL,
						      TRUE,
						      FALSE,
						      g_fgIsOid,
						      nicCmdEventEnterRfTest,
						      nicOidCmdEnterRFTestTimeout,
						      sizeof(CMD_TEST_CTRL_T),
						      (PUINT_8) &rCmdTestCtrl, pvSetBuffer, u4SetBufferLen);
		} else {
			/* already in ICAP mode .. */
			rStatus = WLAN_STATUS_SUCCESS;
		}
	} else {
		rStatus = WLAN_STATUS_INVALID_DATA;
	}

	return rStatus;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Set driver to switch into normal operation mode from RF test mode
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[in] pvSetBuffer A pointer to the buffer that holds the data to be set
*                        should be NULL
* \param[in] u4SetBufferLen The length of the set buffer, should be 0
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                           bytes read from the set buffer. If the call failed
*                           due to invalid length of the set buffer, returns
*                           the amount of storage needed.
*
* \return WLAN_STATUS_SUCCESS
* \return WLAN_STATUS_ADAPTER_NOT_READY
* \return WLAN_STATUS_INVALID_DATA
* \return WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidRftestSetAbortTestMode(IN P_ADAPTER_T prAdapter,
			      IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	WLAN_STATUS rStatus;
	CMD_TEST_CTRL_T rCmdTestCtrl;

	DEBUGFUNC("wlanoidRftestSetAbortTestMode");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = 0;

	if (u4SetBufferLen == 0) {
		if (prAdapter->fgTestMode == TRUE) {
			/* switch to normal mode */
			rCmdTestCtrl.ucAction = 0;	/* Switch mode */
			rCmdTestCtrl.u.u4OpMode = 0;	/* normal mode */

			rStatus = wlanSendSetQueryCmd(prAdapter,
						      CMD_ID_TEST_CTRL,
						      TRUE,
						      FALSE,
						      g_fgIsOid,
						      nicCmdEventLeaveRfTest,
						      nicOidCmdTimeoutCommon,
						      sizeof(CMD_TEST_CTRL_T),
						      (PUINT_8) &rCmdTestCtrl, pvSetBuffer, u4SetBufferLen);
		} else {
			/* already in normal mode .. */
			rStatus = WLAN_STATUS_SUCCESS;
		}
	} else {
		rStatus = WLAN_STATUS_INVALID_DATA;
	}

	return rStatus;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief query for RF test parameter
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[in] pvQueryBuffer      Pointer to the buffer that holds the result of
*                               the query.
* \param[in] u4QueryBufferLen   The length of the query buffer.
* \param[out] pu4QueryInfoLen   If the call is successful, returns the number of
*                               bytes written into the query buffer. If the call
*                               failed due to invalid length of the query buffer,
*                               returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_BUFFER_TOO_SHORT
* \retval WLAN_STATUS_NOT_SUPPORTED
* \retval WLAN_STATUS_NOT_ACCEPTED
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidRftestQueryAutoTest(IN P_ADAPTER_T prAdapter,
			   OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	P_PARAM_MTK_WIFI_TEST_STRUCT_T prRfATInfo;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidRftestQueryAutoTest");

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	*pu4QueryInfoLen = sizeof(PARAM_MTK_WIFI_TEST_STRUCT_T);

	if (u4QueryBufferLen != sizeof(PARAM_MTK_WIFI_TEST_STRUCT_T)) {
		DBGLOG(REQ, ERROR, "Invalid data. QueryBufferLen: %u.\n",
				u4QueryBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	prRfATInfo = (P_PARAM_MTK_WIFI_TEST_STRUCT_T) pvQueryBuffer;
	rStatus = rftestQueryATInfo(prAdapter,
				    prRfATInfo->u4FuncIndex, prRfATInfo->u4FuncData, pvQueryBuffer, u4QueryBufferLen);

	return rStatus;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Set RF test parameter
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
wlanoidRftestSetAutoTest(IN P_ADAPTER_T prAdapter,
			 OUT PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_PARAM_MTK_WIFI_TEST_STRUCT_T prRfATInfo;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidRftestSetAutoTest");

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(PARAM_MTK_WIFI_TEST_STRUCT_T);

	if (u4SetBufferLen != sizeof(PARAM_MTK_WIFI_TEST_STRUCT_T)) {
		DBGLOG(REQ, ERROR, "Invalid data. SetBufferLen: %u.\n",
				u4SetBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	prRfATInfo = (P_PARAM_MTK_WIFI_TEST_STRUCT_T) pvSetBuffer;
	rStatus = rftestSetATInfo(prAdapter, prRfATInfo->u4FuncIndex, prRfATInfo->u4FuncData);

	return rStatus;
}

/* RF test OID set handler */
WLAN_STATUS rftestSetATInfo(IN P_ADAPTER_T prAdapter, UINT_32 u4FuncIndex, UINT_32 u4FuncData)
{
	P_GLUE_INFO_T prGlueInfo;
	P_CMD_INFO_T prCmdInfo;
	P_WIFI_CMD_T prWifiCmd;
	P_CMD_TEST_CTRL_T pCmdTestCtrl;
	UINT_8 ucCmdSeqNum;

	ASSERT(prAdapter);

	prGlueInfo = prAdapter->prGlueInfo;
	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, (CMD_HDR_SIZE + sizeof(CMD_TEST_CTRL_T)));

	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}
	/* increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

	/* Setup common CMD Info Packet */
	prCmdInfo->eCmdType = COMMAND_TYPE_GENERAL_IOCTL;
	prCmdInfo->u2InfoBufLen = CMD_HDR_SIZE + sizeof(CMD_TEST_CTRL_T);
	prCmdInfo->pfCmdDoneHandler = nicCmdEventSetCommon;
	prCmdInfo->pfCmdTimeoutHandler = nicOidCmdTimeoutCommon;
	prCmdInfo->fgIsOid = g_fgIsOid;
	prCmdInfo->ucCID = CMD_ID_TEST_CTRL;
	prCmdInfo->fgSetQuery = TRUE;
	prCmdInfo->fgNeedResp = FALSE;
	prCmdInfo->ucCmdSeqNum = ucCmdSeqNum;
	prCmdInfo->u4SetInfoLen = sizeof(CMD_TEST_CTRL_T);
	prCmdInfo->pvInformationBuffer = NULL;
	prCmdInfo->u4InformationBufferLength = 0;

	/* Setup WIFI_CMD_T (payload = CMD_TEST_CTRL_T) */
	prWifiCmd = (P_WIFI_CMD_T) (prCmdInfo->pucInfoBuffer);
	prWifiCmd->u2TxByteCount = prCmdInfo->u2InfoBufLen;
	prWifiCmd->u2PQ_ID = CMD_PQ_ID;
	prWifiCmd->ucPktTypeID = CMD_PACKET_TYPE_ID;
	prWifiCmd->ucCID = prCmdInfo->ucCID;
	prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
	prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;

	pCmdTestCtrl = (P_CMD_TEST_CTRL_T) (prWifiCmd->aucBuffer);
	pCmdTestCtrl->ucAction = 1;	/* Set ATInfo */
	pCmdTestCtrl->u.rRfATInfo.u4FuncIndex = u4FuncIndex;
	pCmdTestCtrl->u.rRfATInfo.u4FuncData = u4FuncData;

	if ((u4FuncIndex == RF_AT_FUNCID_COMMAND) && (u4FuncData == RF_AT_COMMAND_ICAP)) {
		g_bIcapEnable = TRUE;
		g_bCaptureDone = FALSE;
	}
	/* ICAP dump name Reset */
	if ((u4FuncIndex == RF_AT_FUNCID_COMMAND) && (u4FuncData == RF_AT_COMMAND_RESET_DUMP_NAME))
		g_u2DumpIndex = 0;
	/* insert into prCmdQueue */
	kalEnqueueCommand(prGlueInfo, (P_QUE_ENTRY_T) prCmdInfo);

	/* wakeup txServiceThread later */
	GLUE_SET_EVENT(prAdapter->prGlueInfo);

	return WLAN_STATUS_PENDING;
}

WLAN_STATUS
rftestQueryATInfo(IN P_ADAPTER_T prAdapter,
		  UINT_32 u4FuncIndex, UINT_32 u4FuncData, OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen)
{
	P_GLUE_INFO_T prGlueInfo;
	P_CMD_INFO_T prCmdInfo;
	P_WIFI_CMD_T prWifiCmd;
	P_CMD_TEST_CTRL_T pCmdTestCtrl;
	UINT_8 ucCmdSeqNum;
	P_EVENT_TEST_STATUS prTestStatus;

	ASSERT(prAdapter);

	prGlueInfo = prAdapter->prGlueInfo;

	if (u4FuncIndex == RF_AT_FUNCID_FW_INFO) {
		/* driver implementation */
		prTestStatus = (P_EVENT_TEST_STATUS) pvQueryBuffer;

		prTestStatus->rATInfo.u4FuncData =
		    (prAdapter->rVerInfo.u2FwProductID << 16) | (prAdapter->rVerInfo.u2FwOwnVersion);
		u4QueryBufferLen = sizeof(EVENT_TEST_STATUS);

		return WLAN_STATUS_SUCCESS;
	} else if (u4FuncIndex == RF_AT_FUNCID_DRV_INFO) {
		/* driver implementation */
		prTestStatus = (P_EVENT_TEST_STATUS) pvQueryBuffer;

		prTestStatus->rATInfo.u4FuncData = CFG_DRV_OWN_VERSION;
		u4QueryBufferLen = sizeof(EVENT_TEST_STATUS);

		return WLAN_STATUS_SUCCESS;
	} else if (u4FuncIndex == RF_AT_FUNCID_QUERY_ICAP_DUMP_FILE) {
		/* driver implementation */
		prTestStatus = (P_EVENT_TEST_STATUS) pvQueryBuffer;

		prTestStatus->rATInfo.u4FuncData = g_u2DumpIndex;
		u4QueryBufferLen = sizeof(EVENT_TEST_STATUS);

		return WLAN_STATUS_SUCCESS;
	}
	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, (CMD_HDR_SIZE + sizeof(CMD_TEST_CTRL_T)));

	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}
	/* increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

	/* Setup common CMD Info Packet */
	prCmdInfo->eCmdType = COMMAND_TYPE_GENERAL_IOCTL;
	prCmdInfo->u2InfoBufLen = CMD_HDR_SIZE + sizeof(CMD_TEST_CTRL_T);
	prCmdInfo->pfCmdDoneHandler = nicCmdEventQueryRfTestATInfo;
	prCmdInfo->pfCmdTimeoutHandler = nicOidCmdTimeoutCommon;
	prCmdInfo->fgIsOid = g_fgIsOid;
	prCmdInfo->ucCID = CMD_ID_TEST_CTRL;
	prCmdInfo->fgSetQuery = FALSE;
	prCmdInfo->fgNeedResp = TRUE;
	prCmdInfo->ucCmdSeqNum = ucCmdSeqNum;
	prCmdInfo->u4SetInfoLen = sizeof(CMD_TEST_CTRL_T);
	prCmdInfo->pvInformationBuffer = pvQueryBuffer;
	prCmdInfo->u4InformationBufferLength = u4QueryBufferLen;

	/* Setup WIFI_CMD_T (payload = CMD_TEST_CTRL_T) */
	prWifiCmd = (P_WIFI_CMD_T) (prCmdInfo->pucInfoBuffer);
	prWifiCmd->u2TxByteCount = prCmdInfo->u2InfoBufLen;
	prWifiCmd->u2PQ_ID = CMD_PQ_ID;
	prWifiCmd->ucPktTypeID = CMD_PACKET_TYPE_ID;
	prWifiCmd->ucCID = prCmdInfo->ucCID;
	prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
	prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;

	pCmdTestCtrl = (P_CMD_TEST_CTRL_T) (prWifiCmd->aucBuffer);
	pCmdTestCtrl->ucAction = 2;	/* Get ATInfo */
	pCmdTestCtrl->u.rRfATInfo.u4FuncIndex = u4FuncIndex;
	pCmdTestCtrl->u.rRfATInfo.u4FuncData = u4FuncData;

	/* insert into prCmdQueue */
	kalEnqueueCommand(prGlueInfo, (P_QUE_ENTRY_T) prCmdInfo);

	/* wakeup txServiceThread later */
	GLUE_SET_EVENT(prAdapter->prGlueInfo);

	return WLAN_STATUS_PENDING;

}

WLAN_STATUS rftestSetFrequency(IN P_ADAPTER_T prAdapter, IN UINT_32 u4FreqInKHz, IN PUINT_32 pu4SetInfoLen)
{
	CMD_TEST_CTRL_T rCmdTestCtrl;

	ASSERT(prAdapter);

	rCmdTestCtrl.ucAction = 5;	/* Set Channel Frequency */
	rCmdTestCtrl.u.u4ChannelFreq = u4FreqInKHz;

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_TEST_CTRL,
				   TRUE,
				   FALSE,
				   TRUE,
				   nicCmdEventSetCommon,
				   nicOidCmdTimeoutCommon, sizeof(CMD_TEST_CTRL_T), (PUINT_8) &rCmdTestCtrl, NULL, 0);
}

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
wlanSendSetQueryCmd(IN P_ADAPTER_T prAdapter,
		    UINT_8 ucCID,
		    BOOLEAN fgSetQuery,
		    BOOLEAN fgNeedResp,
		    BOOLEAN fgIsOid,
		    PFN_CMD_DONE_HANDLER pfCmdDoneHandler,
		    PFN_CMD_TIMEOUT_HANDLER pfCmdTimeoutHandler,
		    UINT_32 u4SetQueryInfoLen,
		    PUINT_8 pucInfoBuffer, OUT PVOID pvSetQueryBuffer, IN UINT_32 u4SetQueryBufferLen)
{
	P_GLUE_INFO_T prGlueInfo;
	P_CMD_INFO_T prCmdInfo;
	P_WIFI_CMD_T prWifiCmd;
	UINT_8 ucCmdSeqNum;

	prGlueInfo = prAdapter->prGlueInfo;
	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, (CMD_HDR_SIZE + u4SetQueryInfoLen));

	DEBUGFUNC("wlanSendSetQueryCmd");

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
	prWifiCmd->u2PQ_ID = CMD_PQ_ID;
	prWifiCmd->u2Length = prCmdInfo->u2InfoBufLen - (UINT_16) OFFSET_OF(WIFI_CMD_T, u2Length);
	prWifiCmd->u2PqId = CMD_PQ_ID;
	prWifiCmd->ucPktTypeID = CMD_PACKET_TYPE_ID;
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

#if CFG_SUPPORT_WAPI
/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called by WAPI ui to set wapi mode, which is needed to info the the driver
*          to operation at WAPI mode while driver initialize.
*
* \param[in] prAdapter Pointer to the Adapter structure
* \param[in] pvSetBuffer A pointer to the buffer that holds the data to be set
* \param[in] u4SetBufferLen The length of the set buffer
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*   bytes read from the set buffer. If the call failed due to invalid length of
*   the set buffer, returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_DATA If new setting value is wrong.
* \retval WLAN_STATUS_INVALID_LENGTH
*
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetWapiMode(IN P_ADAPTER_T prAdapter,
		   IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	DEBUGFUNC("wlanoidSetWapiMode");
	DBGLOG(REQ, LOUD, "\r\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	/* Todo:: For support WAPI and Wi-Fi at same driver, use the set wapi assoc ie at the check point */
	/*        The Adapter Connection setting fgUseWapi will cleat whil oid set mode (infra),          */
	/*        And set fgUseWapi True while set wapi assoc ie                                          */
	/*        policay selection, add key all depend on this flag,                                     */
	/*        The fgUseWapi may remove later                                                          */
	if (*(PUINT_32) pvSetBuffer)
		prAdapter->fgUseWapi = TRUE;
	else
		prAdapter->fgUseWapi = FALSE;

#if 0
	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, (CMD_HDR_SIZE + 4));

	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}
	/* increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

	/* compose CMD_BUILD_CONNECTION cmd pkt */
	prCmdInfo->eCmdType = COMMAND_TYPE_NETWORK_IOCTL;
	prCmdInfo->ucBssIndex = prAdapter->prAisBssInfo->ucBssIndex;
	prCmdInfo->u2InfoBufLen = CMD_HDR_SIZE + 4;
	prCmdInfo->pfCmdDoneHandler = nicCmdEventSetCommon;
	prCmdInfo->pfCmdTimeoutHandler = NULL;
	prCmdInfo->fgIsOid = TRUE;
	prCmdInfo->ucCID = CMD_ID_WAPI_MODE;
	prCmdInfo->fgSetQuery = TRUE;
	prCmdInfo->fgNeedResp = FALSE;
	prCmdInfo->fgDriverDomainMCR = FALSE;
	prCmdInfo->ucCmdSeqNum = ucCmdSeqNum;
	prCmdInfo->u4SetInfoLen = u4SetBufferLen;
	prCmdInfo->pvInformationBuffer = pvSetBuffer;
	prCmdInfo->u4InformationBufferLength = u4SetBufferLen;

	/* Setup WIFI_CMD_T */
	prWifiCmd = (P_WIFI_CMD_T) (prCmdInfo->pucInfoBuffer);
	prWifiCmd->u2TxByteCount = prCmdInfo->u2InfoBufLen;
	prWifiCmd->u2PQ_ID = CMD_PQ_ID;
	prWifiCmd->ucPktTypeID = CMD_PACKET_TYPE_ID;
	prWifiCmd->ucCID = prCmdInfo->ucCID;
	prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
	prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;

	cp = (PUINT_8) (prWifiCmd->aucBuffer);

	kalMemCopy(cp, (PUINT_8) pvSetBuffer, 4);

	/* insert into prCmdQueue */
	kalEnqueueCommand(prGlueInfo, (P_QUE_ENTRY_T) prCmdInfo);

	/* wakeup txServiceThread later */
	GLUE_SET_EVENT(prGlueInfo);

	return WLAN_STATUS_PENDING;
#else
	return WLAN_STATUS_SUCCESS;
#endif
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called by WAPI to set the assoc info, which is needed to add to
*          Association request frame while join WAPI AP.
*
* \param[in] prAdapter Pointer to the Adapter structure
* \param[in] pvSetBuffer A pointer to the buffer that holds the data to be set
* \param[in] u4SetBufferLen The length of the set buffer
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*   bytes read from the set buffer. If the call failed due to invalid length of
*   the set buffer, returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_DATA If new setting value is wrong.
* \retval WLAN_STATUS_INVALID_LENGTH
*
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetWapiAssocInfo(IN P_ADAPTER_T prAdapter,
			IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_WAPI_INFO_ELEM_T prWapiInfo;
	PUINT_8 cp;
	UINT_16 u2AuthSuiteCount = 0;
	UINT_16 u2PairSuiteCount = 0;
	UINT_32 u4AuthKeyMgtSuite = 0;
	UINT_32 u4PairSuite = 0;
	UINT_32 u4GroupSuite = 0;
	UINT_16 u2IeLength = 0;

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	DEBUGFUNC("wlanoidSetWapiAssocInfo");
	DBGLOG(REQ, LOUD, "\r\n");

	prAdapter->rWifiVar.rConnSettings.fgWapiMode = FALSE;

	if (u4SetBufferLen < 20 /* From EID to Group cipher */)
		return WLAN_STATUS_INVALID_LENGTH;

	if (!wextSrchDesiredWAPIIE((PUINT_8) pvSetBuffer, u4SetBufferLen, (PUINT_8 *) &prWapiInfo))
		return WLAN_STATUS_INVALID_LENGTH;

	if (!prWapiInfo || prWapiInfo->ucLength < 18)
		return WLAN_STATUS_INVALID_LENGTH;

	u2IeLength = prWapiInfo->ucLength + 2;

	/* Skip Version check */
	cp = (PUINT_8) &prWapiInfo->u2AuthKeyMgtSuiteCount;

	WLAN_GET_FIELD_16(cp, &u2AuthSuiteCount);

	if (u2AuthSuiteCount > 1)
		return WLAN_STATUS_INVALID_LENGTH;

	cp += 2;
	WLAN_GET_FIELD_32(cp, &u4AuthKeyMgtSuite);

	DBGLOG(SEC, TRACE, "WAPI: Assoc Info auth mgt suite [%d]: %02x-%02x-%02x-%02x\n",
	       u2AuthSuiteCount,
	       (UCHAR) (u4AuthKeyMgtSuite & 0x000000FF),
	       (UCHAR) ((u4AuthKeyMgtSuite >> 8) & 0x000000FF),
	       (UCHAR) ((u4AuthKeyMgtSuite >> 16) & 0x000000FF), (UCHAR) ((u4AuthKeyMgtSuite >> 24) & 0x000000FF));

	if (u4AuthKeyMgtSuite != WAPI_AKM_SUITE_802_1X && u4AuthKeyMgtSuite != WAPI_AKM_SUITE_PSK)
		ASSERT(FALSE);

	cp += 4;
	WLAN_GET_FIELD_16(cp, &u2PairSuiteCount);
	if (u2PairSuiteCount > 1)
		return WLAN_STATUS_INVALID_LENGTH;

	cp += 2;
	WLAN_GET_FIELD_32(cp, &u4PairSuite);
	DBGLOG(SEC, TRACE, "WAPI: Assoc Info pairwise cipher suite [%d]: %02x-%02x-%02x-%02x\n",
	       u2PairSuiteCount,
	       (UCHAR) (u4PairSuite & 0x000000FF),
	       (UCHAR) ((u4PairSuite >> 8) & 0x000000FF),
	       (UCHAR) ((u4PairSuite >> 16) & 0x000000FF), (UCHAR) ((u4PairSuite >> 24) & 0x000000FF));

	if (u4PairSuite != WAPI_CIPHER_SUITE_WPI)
		ASSERT(FALSE);

	cp += 4;
	WLAN_GET_FIELD_32(cp, &u4GroupSuite);
	DBGLOG(SEC, TRACE, "WAPI: Assoc Info group cipher suite : %02x-%02x-%02x-%02x\n",
	       (UCHAR) (u4GroupSuite & 0x000000FF),
	       (UCHAR) ((u4GroupSuite >> 8) & 0x000000FF),
	       (UCHAR) ((u4GroupSuite >> 16) & 0x000000FF), (UCHAR) ((u4GroupSuite >> 24) & 0x000000FF));

	if (u4GroupSuite != WAPI_CIPHER_SUITE_WPI)
		ASSERT(FALSE);

	prAdapter->rWifiVar.rConnSettings.u4WapiSelectedAKMSuite = u4AuthKeyMgtSuite;
	prAdapter->rWifiVar.rConnSettings.u4WapiSelectedPairwiseCipher = u4PairSuite;
	prAdapter->rWifiVar.rConnSettings.u4WapiSelectedGroupCipher = u4GroupSuite;

	kalMemCopy(prAdapter->prGlueInfo->aucWapiAssocInfoIEs, prWapiInfo, u2IeLength);
	prAdapter->prGlueInfo->u2WapiAssocInfoIESz = u2IeLength;
	DBGLOG(SEC, TRACE, "Assoc Info IE sz %u\n", u2IeLength);

	prAdapter->rWifiVar.rConnSettings.fgWapiMode = TRUE;

	return WLAN_STATUS_SUCCESS;

}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set the wpi key to the driver.
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
*
* \note The setting buffer P_PARAM_WPI_KEY, which is set by NDIS, is unpacked.
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetWapiKey(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_GLUE_INFO_T prGlueInfo;
	P_CMD_INFO_T prCmdInfo;
	P_WIFI_CMD_T prWifiCmd;
	P_PARAM_WPI_KEY_T prNewKey;
	P_CMD_802_11_KEY prCmdKey;
	PUINT_8 pc;
	UINT_8 ucCmdSeqNum;
	P_STA_RECORD_T prStaRec;
	P_BSS_INFO_T prBssInfo;

	DEBUGFUNC("wlanoidSetWapiKey");
	DBGLOG(REQ, LOUD, "\r\n");

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in set add key! (Adapter not ready). ACPI=D%d, Radio=%d\r\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	prNewKey = (P_PARAM_WPI_KEY_T) pvSetBuffer;

	DBGLOG_MEM8(REQ, TRACE, (PUINT_8) pvSetBuffer, 560);
	pc = (PUINT_8) pvSetBuffer;

	*pu4SetInfoLen = u4SetBufferLen;

	/* Todo:: WAPI AP mode !!!!! */
	prBssInfo = prAdapter->prAisBssInfo;

	prNewKey->ucKeyID = prNewKey->ucKeyID & BIT(0);

	/* Dump P_PARAM_WPI_KEY_T content. */
	DBGLOG(REQ, TRACE, "Set: Dump P_PARAM_WPI_KEY_T content\r\n");
	DBGLOG(REQ, TRACE, "TYPE      : %d\r\n", prNewKey->eKeyType);
	DBGLOG(REQ, TRACE, "Direction : %d\r\n", prNewKey->eDirection);
	DBGLOG(REQ, TRACE, "KeyID     : %d\r\n", prNewKey->ucKeyID);
	DBGLOG(REQ, TRACE, "AddressIndex:\r\n");
	DBGLOG_MEM8(REQ, TRACE, prNewKey->aucAddrIndex, 12);
	prNewKey->u4LenWPIEK = 16;

	DBGLOG_MEM8(REQ, TRACE, (PUINT_8) prNewKey->aucWPIEK, (UINT_8) prNewKey->u4LenWPIEK);
	prNewKey->u4LenWPICK = 16;

	DBGLOG(REQ, TRACE, "CK Key(%d):\r\n", (UINT_8) prNewKey->u4LenWPICK);
	DBGLOG_MEM8(REQ, TRACE, (PUINT_8) prNewKey->aucWPICK, (UINT_8) prNewKey->u4LenWPICK);
	DBGLOG(REQ, TRACE, "PN:\r\n");
	if (prNewKey->eKeyType == 0) {
		prNewKey->aucPN[0] = 0x5c;
		prNewKey->aucPN[1] = 0x36;
		prNewKey->aucPN[2] = 0x5c;
		prNewKey->aucPN[3] = 0x36;
		prNewKey->aucPN[4] = 0x5c;
		prNewKey->aucPN[5] = 0x36;
		prNewKey->aucPN[6] = 0x5c;
		prNewKey->aucPN[7] = 0x36;
		prNewKey->aucPN[8] = 0x5c;
		prNewKey->aucPN[9] = 0x36;
		prNewKey->aucPN[10] = 0x5c;
		prNewKey->aucPN[11] = 0x36;
		prNewKey->aucPN[12] = 0x5c;
		prNewKey->aucPN[13] = 0x36;
		prNewKey->aucPN[14] = 0x5c;
		prNewKey->aucPN[15] = 0x36;
	}

	DBGLOG_MEM8(REQ, TRACE, (PUINT_8) prNewKey->aucPN, 16);

	prGlueInfo = prAdapter->prGlueInfo;

	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, (CMD_HDR_SIZE + u4SetBufferLen));

	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}
	/* increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

	/* compose CMD_ID_ADD_REMOVE_KEY cmd pkt */
	prCmdInfo->eCmdType = COMMAND_TYPE_NETWORK_IOCTL;
	prCmdInfo->u2InfoBufLen = CMD_HDR_SIZE + sizeof(CMD_802_11_KEY);
	prCmdInfo->pfCmdDoneHandler = nicCmdEventSetCommon;
	prCmdInfo->pfCmdTimeoutHandler = nicOidCmdTimeoutCommon;
	prCmdInfo->fgIsOid = g_fgIsOid;
	prCmdInfo->ucCID = CMD_ID_ADD_REMOVE_KEY;
	prCmdInfo->fgSetQuery = TRUE;
	prCmdInfo->fgNeedResp = FALSE;
	prCmdInfo->ucCmdSeqNum = ucCmdSeqNum;
	prCmdInfo->u4SetInfoLen = u4SetBufferLen;
	prCmdInfo->pvInformationBuffer = pvSetBuffer;
	prCmdInfo->u4InformationBufferLength = u4SetBufferLen;

	/* Setup WIFI_CMD_T */
	prWifiCmd = (P_WIFI_CMD_T) (prCmdInfo->pucInfoBuffer);
	prWifiCmd->u2TxByteCount = prCmdInfo->u2InfoBufLen;
	prWifiCmd->u2PQ_ID = CMD_PQ_ID;
	prWifiCmd->ucPktTypeID = CMD_PACKET_TYPE_ID;
	prWifiCmd->ucCID = prCmdInfo->ucCID;
	prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
	prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;

	prCmdKey = (P_CMD_802_11_KEY) (prWifiCmd->aucBuffer);

	kalMemZero(prCmdKey, sizeof(CMD_802_11_KEY));

	prCmdKey->ucAddRemove = 1;	/* Add */

	if (prNewKey->eKeyType == ENUM_WPI_PAIRWISE_KEY) {
		prCmdKey->ucTxKey = 1;
		prCmdKey->ucKeyType = 1;
	}
	kalMemCopy(prCmdKey->aucPeerAddr, (PUINT_8) prNewKey->aucAddrIndex, MAC_ADDR_LEN);
	if ((prCmdKey->aucPeerAddr[0] & prCmdKey->aucPeerAddr[1] & prCmdKey->aucPeerAddr[2] &
	     prCmdKey->aucPeerAddr[3] & prCmdKey->aucPeerAddr[4] & prCmdKey->aucPeerAddr[5]) == 0xFF) {
		prStaRec = cnmGetStaRecByAddress(prAdapter, prBssInfo->ucBssIndex, prBssInfo->aucBSSID);
		ASSERT(prStaRec);	/* AIS RSN Group key, addr is BC addr */
		kalMemCopy(prCmdKey->aucPeerAddr, prStaRec->aucMacAddr, MAC_ADDR_LEN);
	} else {
		prStaRec = cnmGetStaRecByAddress(prAdapter, prBssInfo->ucBssIndex, prCmdKey->aucPeerAddr);
	}

	prCmdKey->ucBssIdx = prAdapter->prAisBssInfo->ucBssIndex;	/* AIS */

	prCmdKey->ucKeyId = prNewKey->ucKeyID;

	prCmdKey->ucKeyLen = 32;

	prCmdKey->ucAlgorithmId = CIPHER_SUITE_WPI;

	kalMemCopy(prCmdKey->aucKeyMaterial, (PUINT_8) prNewKey->aucWPIEK, 16);

	kalMemCopy(prCmdKey->aucKeyMaterial + 16, (PUINT_8) prNewKey->aucWPICK, 16);

	kalMemCopy(prCmdKey->aucKeyRsc, (PUINT_8) prNewKey->aucPN, 16);

	if (prCmdKey->ucTxKey) {
		if (prStaRec) {
			if (prCmdKey->ucKeyType) {	/* AIS RSN STA */
				prCmdKey->ucWlanIndex = prStaRec->ucWlanIndex;
				prStaRec->fgTransmitKeyExist = TRUE;	/* wait for CMD Done ? */
			} else {
				ASSERT(FALSE);
			}
		}
#if 0
		if (fgAddTxBcKey || !prStaRec) {

			if ((prCmdKey->aucPeerAddr[0] & prCmdKey->aucPeerAddr[1] & prCmdKey->
			     aucPeerAddr[2] & prCmdKey->aucPeerAddr[3] & prCmdKey->aucPeerAddr[4] & prCmdKey->
			     aucPeerAddr[5]) == 0xFF) {
				prCmdKey->ucWlanIndex = 255;	/* AIS WEP Tx key */
			} else {	/* Exist this case ? */
				ASSERT(FALSE);
				/* prCmdKey->ucWlanIndex = */
				/* secPrivacySeekForBcEntry(prAdapter, */
				/* prBssInfo->ucBssIndex, */
				/* NETWORK_TYPE_AIS, */
				/* prCmdKey->aucPeerAddr, */
				/* prCmdKey->ucAlgorithmId, */
				/* prCmdKey->ucKeyId, */
			}

			prBssInfo->fgBcDefaultKeyExist = TRUE;
			prBssInfo->ucBMCWlanIndex = prCmdKey->ucWlanIndex;	/* Saved for AIS WEP */
			prBssInfo->ucTxBcDefaultIdx = prCmdKey->ucKeyId;
		}
#endif
	} else {
		/* Including IBSS RSN Rx BC key ? */
		if ((prCmdKey->aucPeerAddr[0] & prCmdKey->aucPeerAddr[1] &
		     prCmdKey->aucPeerAddr[2] & prCmdKey->aucPeerAddr[3] &
		     prCmdKey->aucPeerAddr[4] & prCmdKey->aucPeerAddr[5]) == 0xFF) {
			prCmdKey->ucWlanIndex = WTBL_RESERVED_ENTRY;	/* AIS WEP, should not have this case!! */
		} else {
			if (prStaRec) {	/* AIS RSN Group key but addr is BSSID */
				/* ASSERT(prStaRec->ucBMCWlanIndex < WTBL_SIZE) */
				prCmdKey->ucWlanIndex =
				    secPrivacySeekForBcEntry(prAdapter, prStaRec->ucBssIndex,
							     prStaRec->aucMacAddr,
							     prStaRec->ucIndex,
							     prCmdKey->ucAlgorithmId, prCmdKey->ucKeyId);
				prStaRec->ucWlanIndex = prCmdKey->ucWlanIndex;
			} else {	/* Exist this case ? */
				ASSERT(FALSE);
				/* prCmdKey->ucWlanIndex = */
				/* secPrivacySeekForBcEntry(prAdapter, */
				/* prBssInfo->ucBssIndex, */
				/* NETWORK_TYPE_AIS, */
				/* prCmdKey->aucPeerAddr, */
				/* prCmdKey->ucAlgorithmId, */
				/* prCmdKey->ucKeyId, */
			}
		}
	}

	/* insert into prCmdQueue */
	kalEnqueueCommand(prGlueInfo, (P_QUE_ENTRY_T) prCmdInfo);

	/* wakeup txServiceThread later */
	GLUE_SET_EVENT(prGlueInfo);

	return WLAN_STATUS_PENDING;
}				/* wlanoidSetAddKey */
#endif

#if CFG_SUPPORT_WPS2
/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called by WSC to set the assoc info, which is needed to add to
*          Association request frame while join WPS AP.
*
* \param[in] prAdapter Pointer to the Adapter structure
* \param[in] pvSetBuffer A pointer to the buffer that holds the data to be set
* \param[in] u4SetBufferLen The length of the set buffer
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*   bytes read from the set buffer. If the call failed due to invalid length of
*   the set buffer, returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_DATA If new setting value is wrong.
* \retval WLAN_STATUS_INVALID_LENGTH
*
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetWSCAssocInfo(IN P_ADAPTER_T prAdapter,
		       IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	DEBUGFUNC("wlanoidSetWSCAssocInfo");
	DBGLOG(REQ, LOUD, "\r\n");

	if (u4SetBufferLen == 0)
		return WLAN_STATUS_INVALID_LENGTH;

	*pu4SetInfoLen = u4SetBufferLen;

	kalMemCopy(prAdapter->prGlueInfo->aucWSCAssocInfoIE, pvSetBuffer, u4SetBufferLen);
	prAdapter->prGlueInfo->u2WSCAssocInfoIELen = (UINT_16) u4SetBufferLen;
	DBGLOG(SEC, TRACE, "Assoc Info IE sz %u\n", u4SetBufferLen);

	return WLAN_STATUS_SUCCESS;

}
#endif

#if CFG_ENABLE_WAKEUP_ON_LAN
WLAN_STATUS
wlanoidSetAddWakeupPattern(IN P_ADAPTER_T prAdapter,
			   IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_PARAM_PM_PACKET_PATTERN prPacketPattern;

	DEBUGFUNC("wlanoidSetAddWakeupPattern");
	DBGLOG(REQ, LOUD, "\r\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(PARAM_PM_PACKET_PATTERN);

	if (u4SetBufferLen < sizeof(PARAM_PM_PACKET_PATTERN))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prPacketPattern = (P_PARAM_PM_PACKET_PATTERN) pvSetBuffer;

	/* FIXME: Send the struct to firmware */

	return WLAN_STATUS_FAILURE;
}

WLAN_STATUS
wlanoidSetRemoveWakeupPattern(IN P_ADAPTER_T prAdapter,
			      IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_PARAM_PM_PACKET_PATTERN prPacketPattern;

	DEBUGFUNC("wlanoidSetAddWakeupPattern");
	DBGLOG(REQ, LOUD, "\r\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(PARAM_PM_PACKET_PATTERN);

	if (u4SetBufferLen < sizeof(PARAM_PM_PACKET_PATTERN))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prPacketPattern = (P_PARAM_PM_PACKET_PATTERN) pvSetBuffer;

	/* FIXME: Send the struct to firmware */

	return WLAN_STATUS_FAILURE;
}

WLAN_STATUS
wlanoidQueryEnableWakeup(IN P_ADAPTER_T prAdapter,
			 OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	PUINT_32 pu4WakeupEventEnable;

	DEBUGFUNC("wlanoidQueryEnableWakeup");
	DBGLOG(REQ, LOUD, "\r\n");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(UINT_32);

	if (u4QueryBufferLen < sizeof(UINT_32))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	pu4WakeupEventEnable = (PUINT_32) pvQueryBuffer;

	*pu4WakeupEventEnable = prAdapter->u4WakeupEventEnable;

	return WLAN_STATUS_SUCCESS;
}

WLAN_STATUS
wlanoidSetEnableWakeup(IN P_ADAPTER_T prAdapter,
		       IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	PUINT_32 pu4WakeupEventEnable;

	DEBUGFUNC("wlanoidSetEnableWakup");
	DBGLOG(REQ, LOUD, "\r\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(UINT_32);

	if (u4SetBufferLen < sizeof(UINT_32))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	pu4WakeupEventEnable = (PUINT_32) pvSetBuffer;
	prAdapter->u4WakeupEventEnable = *pu4WakeupEventEnable;

	/* FIXME: Send Command Event for setting wakeup-pattern / Magic Packet to firmware */

	return WLAN_STATUS_FAILURE;
}
#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to configure PS related settings for WMM-PS test.
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[in] pvSetBuffer A pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                           bytes read from the set buffer. If the call failed
*                           due to invalid length of the set buffer, returns
*                           the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetWiFiWmmPsTest(IN P_ADAPTER_T prAdapter,
			IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_PARAM_CUSTOM_WMM_PS_TEST_STRUCT_T prWmmPsTestInfo;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	CMD_SET_WMM_PS_TEST_STRUCT_T rSetWmmPsTestParam;
	UINT_16 u2CmdBufLen;
	P_PM_PROFILE_SETUP_INFO_T prPmProfSetupInfo;
	P_BSS_INFO_T prBssInfo;

	DEBUGFUNC("wlanoidSetWiFiWmmPsTest");

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(PARAM_CUSTOM_WMM_PS_TEST_STRUCT_T);

	prWmmPsTestInfo = (P_PARAM_CUSTOM_WMM_PS_TEST_STRUCT_T) pvSetBuffer;

	rSetWmmPsTestParam.ucBssIndex = prAdapter->prAisBssInfo->ucBssIndex;
	rSetWmmPsTestParam.bmfgApsdEnAc = prWmmPsTestInfo->bmfgApsdEnAc;
	rSetWmmPsTestParam.ucIsEnterPsAtOnce = prWmmPsTestInfo->ucIsEnterPsAtOnce;
	rSetWmmPsTestParam.ucIsDisableUcTrigger = prWmmPsTestInfo->ucIsDisableUcTrigger;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, rSetWmmPsTestParam.ucBssIndex);
	prPmProfSetupInfo = &prBssInfo->rPmProfSetupInfo;
	prPmProfSetupInfo->ucBmpDeliveryAC = (rSetWmmPsTestParam.bmfgApsdEnAc >> 4) & BITS(0, 3);
	prPmProfSetupInfo->ucBmpTriggerAC = rSetWmmPsTestParam.bmfgApsdEnAc & BITS(0, 3);

	u2CmdBufLen = sizeof(CMD_SET_WMM_PS_TEST_STRUCT_T);

#if 0
	/* it will apply the disable trig or not immediately */
	if (prPmInfo->ucWmmPsDisableUcPoll && prPmInfo->ucWmmPsConnWithTrig)
		NIC_PM_WMM_PS_DISABLE_UC_TRIG(prAdapter, TRUE);
	else
		NIC_PM_WMM_PS_DISABLE_UC_TRIG(prAdapter, FALSE);
#endif

	rStatus = wlanSendSetQueryCmd(prAdapter, CMD_ID_SET_WMM_PS_TEST_PARMS,
				TRUE, FALSE, g_fgIsOid,
				nicCmdEventSetCommon,/* TODO? */
				nicCmdTimeoutCommon, u2CmdBufLen,
				(PUINT_8)(&rSetWmmPsTestParam), NULL, 0);

	return rStatus;
}				/* wlanoidSetWiFiWmmPsTest */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to configure enable/disable TX A-MPDU feature.
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[in] pvSetBuffer A pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                           bytes read from the set buffer. If the call failed
*                           due to invalid length of the set buffer, returns
*                           the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetTxAmpdu(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	CMD_TX_AMPDU_T rTxAmpdu;
	UINT_16 u2CmdBufLen;
	PBOOLEAN pfgEnable;

	DEBUGFUNC("wlanoidSetTxAmpdu");

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(BOOLEAN);

	pfgEnable = (PBOOLEAN) pvSetBuffer;

	rTxAmpdu.fgEnable = *pfgEnable;

	u2CmdBufLen = sizeof(CMD_TX_AMPDU_T);

	rStatus = wlanSendSetQueryCmd(prAdapter,
				      CMD_ID_TX_AMPDU,
				      TRUE, FALSE, TRUE, NULL, NULL, u2CmdBufLen, (PUINT_8) &rTxAmpdu, NULL, 0);

	return rStatus;
}				/* wlanoidSetTxAmpdu */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to configure reject/accept ADDBA Request.
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[in] pvSetBuffer A pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                           bytes read from the set buffer. If the call failed
*                           due to invalid length of the set buffer, returns
*                           the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetAddbaReject(IN P_ADAPTER_T prAdapter,
		      IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	CMD_ADDBA_REJECT_T rAddbaReject;
	UINT_16 u2CmdBufLen;
	PBOOLEAN pfgEnable;

	DEBUGFUNC("wlanoidSetAddbaReject");

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(BOOLEAN);

	pfgEnable = (PBOOLEAN) pvSetBuffer;

	rAddbaReject.fgEnable = *pfgEnable;

	u2CmdBufLen = sizeof(CMD_ADDBA_REJECT_T);

	rStatus = wlanSendSetQueryCmd(prAdapter,
				      CMD_ID_ADDBA_REJECT,
				      TRUE, FALSE, TRUE, NULL, NULL, u2CmdBufLen, (PUINT_8) &rAddbaReject, NULL, 0);

	return rStatus;
}				/* wlanoidSetAddbaReject */

#if CFG_SLT_SUPPORT

WLAN_STATUS
wlanoidQuerySLTStatus(IN P_ADAPTER_T prAdapter,
		      OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;
	P_PARAM_MTK_SLT_TEST_STRUCT_T prMtkSltInfo = (P_PARAM_MTK_SLT_TEST_STRUCT_T) NULL;
	P_SLT_INFO_T prSltInfo = (P_SLT_INFO_T) NULL;

	DEBUGFUNC("wlanoidQuerySLTStatus");
	DBGLOG(REQ, LOUD, "\r\n");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);

	*pu4QueryInfoLen = sizeof(PARAM_MTK_SLT_TEST_STRUCT_T);

	if (u4QueryBufferLen < sizeof(PARAM_MTK_SLT_TEST_STRUCT_T))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvQueryBuffer);

	prMtkSltInfo = (P_PARAM_MTK_SLT_TEST_STRUCT_T) pvQueryBuffer;

	prSltInfo = &(prAdapter->rWifiVar.rSltInfo);

	switch (prMtkSltInfo->rSltFuncIdx) {
	case ENUM_MTK_SLT_FUNC_LP_SET:
		{
			P_PARAM_MTK_SLT_LP_TEST_STRUCT_T prLpSetting = (P_PARAM_MTK_SLT_LP_TEST_STRUCT_T) NULL;

			ASSERT(prMtkSltInfo->u4FuncInfoLen == sizeof(PARAM_MTK_SLT_LP_TEST_STRUCT_T));

			prLpSetting = (P_PARAM_MTK_SLT_LP_TEST_STRUCT_T) &prMtkSltInfo->unFuncInfoContent;

			prLpSetting->u4BcnRcvNum = prSltInfo->u4BeaconReceiveCnt;
		}
		break;
	default:
		/* TBD... */
		break;
	}

	return rWlanStatus;
}				/* wlanoidQuerySLTStatus */

WLAN_STATUS
wlanoidUpdateSLTMode(IN P_ADAPTER_T prAdapter,
		     IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;
	P_PARAM_MTK_SLT_TEST_STRUCT_T prMtkSltInfo = (P_PARAM_MTK_SLT_TEST_STRUCT_T) NULL;
	P_SLT_INFO_T prSltInfo = (P_SLT_INFO_T) NULL;
	P_BSS_DESC_T prBssDesc = (P_BSS_DESC_T) NULL;
	P_STA_RECORD_T prStaRec = (P_STA_RECORD_T) NULL;
	P_BSS_INFO_T prBssInfo = (P_BSS_INFO_T) NULL;

	/* 1. Action: Update or Initial Set
	 * 2. Role.
	 * 3. Target MAC address.
	 * 4. RF BW & Rate Settings
	 */

	DEBUGFUNC("wlanoidUpdateSLTMode");
	DBGLOG(REQ, LOUD, "\r\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(PARAM_MTK_SLT_TEST_STRUCT_T);

	if (u4SetBufferLen < sizeof(PARAM_MTK_SLT_TEST_STRUCT_T))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prMtkSltInfo = (P_PARAM_MTK_SLT_TEST_STRUCT_T) pvSetBuffer;

	prSltInfo = &(prAdapter->rWifiVar.rSltInfo);
	prBssInfo = prAdapter->prAisBssInfo;

	switch (prMtkSltInfo->rSltFuncIdx) {
	case ENUM_MTK_SLT_FUNC_INITIAL:	/* Initialize */
		{
			P_PARAM_MTK_SLT_INITIAL_STRUCT_T prMtkSltInit = (P_PARAM_MTK_SLT_INITIAL_STRUCT_T) NULL;

			ASSERT(prMtkSltInfo->u4FuncInfoLen == sizeof(PARAM_MTK_SLT_INITIAL_STRUCT_T));

			prMtkSltInit = (P_PARAM_MTK_SLT_INITIAL_STRUCT_T) &prMtkSltInfo->unFuncInfoContent;

			if (prSltInfo->prPseudoStaRec != NULL) {
				/* The driver has been initialized. */
				prSltInfo->prPseudoStaRec = NULL;
			}

			prSltInfo->prPseudoBssDesc = scanSearchExistingBssDesc(prAdapter,
									       BSS_TYPE_IBSS,
									       prMtkSltInit->aucTargetMacAddr,
									       prMtkSltInit->aucTargetMacAddr);

			prSltInfo->u2SiteID = prMtkSltInit->u2SiteID;

			/* Bandwidth 2.4G: Channel 1~14
			 * Bandwidth 5G: *36, 40, 44, 48, 52, 56, 60, 64,
			 *                       *100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140,
			 *                       149, 153, *157, 161,
			 *                       184, 188, 192, 196, 200, 204, 208, 212, *216
			 */
			prSltInfo->ucChannel2G4 = 1 + (prSltInfo->u2SiteID % 4) * 5;

			switch (prSltInfo->ucChannel2G4) {
			case 1:
				prSltInfo->ucChannel5G = 36;
				break;
			case 6:
				prSltInfo->ucChannel5G = 52;
				break;
			case 11:
				prSltInfo->ucChannel5G = 104;
				break;
			case 16:
				prSltInfo->ucChannel2G4 = 14;
				prSltInfo->ucChannel5G = 161;
				break;
			default:
				ASSERT(FALSE);
			}

			if (prSltInfo->prPseudoBssDesc == NULL) {
				do {
					prSltInfo->prPseudoBssDesc = scanAllocateBssDesc(prAdapter);

					if (prSltInfo->prPseudoBssDesc == NULL) {
						rWlanStatus = WLAN_STATUS_FAILURE;
						break;
					}
					prBssDesc = prSltInfo->prPseudoBssDesc;

				} while (FALSE);
			} else {
				prBssDesc = prSltInfo->prPseudoBssDesc;
			}

			if (prBssDesc) {
				prBssDesc->eBSSType = BSS_TYPE_IBSS;

				COPY_MAC_ADDR(prBssDesc->aucSrcAddr, prMtkSltInit->aucTargetMacAddr);
				COPY_MAC_ADDR(prBssDesc->aucBSSID, prBssInfo->aucOwnMacAddr);

				prBssDesc->u2BeaconInterval = 100;
				prBssDesc->u2ATIMWindow = 0;
				prBssDesc->ucDTIMPeriod = 1;

				prBssDesc->u2IELength = 0;

				prBssDesc->fgIsERPPresent = TRUE;
				prBssDesc->fgIsHTPresent = TRUE;

				prBssDesc->u2OperationalRateSet = BIT(RATE_36M_INDEX);
				prBssDesc->u2BSSBasicRateSet = BIT(RATE_36M_INDEX);
				prBssDesc->fgIsUnknownBssBasicRate = FALSE;

				prBssDesc->fgIsLargerTSF = TRUE;

				prBssDesc->eBand = BAND_2G4;

				prBssDesc->ucChannelNum = prSltInfo->ucChannel2G4;

				prBssDesc->ucPhyTypeSet = PHY_TYPE_SET_802_11ABGN;

				GET_CURRENT_SYSTIME(&prBssDesc->rUpdateTime);
			}
		}
		break;
	case ENUM_MTK_SLT_FUNC_RATE_SET:	/* Update RF Settings. */
		if (prSltInfo->prPseudoStaRec == NULL) {
			rWlanStatus = WLAN_STATUS_FAILURE;
		} else {
			P_PARAM_MTK_SLT_TR_TEST_STRUCT_T prTRSetting = (P_PARAM_MTK_SLT_TR_TEST_STRUCT_T) NULL;

			ASSERT(prMtkSltInfo->u4FuncInfoLen == sizeof(PARAM_MTK_SLT_TR_TEST_STRUCT_T));

			prStaRec = prSltInfo->prPseudoStaRec;
			prTRSetting = (P_PARAM_MTK_SLT_TR_TEST_STRUCT_T) &prMtkSltInfo->unFuncInfoContent;

			if (prTRSetting->rNetworkType == PARAM_NETWORK_TYPE_OFDM5) {
				prBssInfo->eBand = BAND_5G;
				prBssInfo->ucPrimaryChannel = prSltInfo->ucChannel5G;
			}
			if (prTRSetting->rNetworkType == PARAM_NETWORK_TYPE_OFDM24) {
				prBssInfo->eBand = BAND_2G4;
				prBssInfo->ucPrimaryChannel = prSltInfo->ucChannel2G4;
			}

			if ((prTRSetting->u4FixedRate & FIXED_BW_DL40) != 0) {
				/* RF 40 */
				/* It would controls RFBW capability in WTBL. */
				prStaRec->u2HtCapInfo |= HT_CAP_INFO_SUP_CHNL_WIDTH;
				/* This controls RF BW, RF BW would be 40 only if
				 * 1. PHY_TYPE_BIT_HT is TRUE.
				 * 2. SCO is SCA/SCB.
				 */
				prStaRec->ucDesiredPhyTypeSet = PHY_TYPE_BIT_HT;

				/* U20/L20 Control. */
				switch (prTRSetting->u4FixedRate & 0xC000) {
				case FIXED_EXT_CHNL_U20:
					prBssInfo->eBssSCO = CHNL_EXT_SCB;	/* +2 */
					if (prTRSetting->rNetworkType == PARAM_NETWORK_TYPE_OFDM5) {
						prBssInfo->ucPrimaryChannel += 2;
					} else {
						/* For channel 1, testing L20 at channel 8. AOSP */
						SetTestChannel(&prBssInfo->ucPrimaryChannel);
					}
					break;
				case FIXED_EXT_CHNL_L20:
				default:	/* 40M */
					prBssInfo->eBssSCO = CHNL_EXT_SCA;	/* -2 */
					if (prTRSetting->rNetworkType == PARAM_NETWORK_TYPE_OFDM5) {
						prBssInfo->ucPrimaryChannel -= 2;
					} else {
						/* For channel 11 / 14. testing U20 at channel 3. AOSP */
						SetTestChannel(&prBssInfo->ucPrimaryChannel);
					}
					break;
				}
			} else {
				/* RF 20 */
				prStaRec->u2HtCapInfo &= ~HT_CAP_INFO_SUP_CHNL_WIDTH;
				prBssInfo->eBssSCO = CHNL_EXT_SCN;
			}

			prBssInfo->fgErpProtectMode = FALSE;
			prBssInfo->eHtProtectMode = HT_PROTECT_MODE_NONE;
			prBssInfo->eGfOperationMode = GF_MODE_NORMAL;

			nicUpdateBss(prAdapter, prBssInfo->ucNetTypeIndex);

			prStaRec->u2HtCapInfo &= ~(HT_CAP_INFO_SHORT_GI_20M | HT_CAP_INFO_SHORT_GI_40M);

			switch (prTRSetting->u4FixedRate & 0xFF) {
			case RATE_OFDM_54M:
				prStaRec->u2DesiredNonHTRateSet = BIT(RATE_54M_SW_INDEX);
				break;
			case RATE_OFDM_48M:
				prStaRec->u2DesiredNonHTRateSet = BIT(RATE_48M_SW_INDEX);
				break;
			case RATE_OFDM_36M:
				prStaRec->u2DesiredNonHTRateSet = BIT(RATE_36M_SW_INDEX);
				break;
			case RATE_OFDM_24M:
				prStaRec->u2DesiredNonHTRateSet = BIT(RATE_24M_SW_INDEX);
				break;
			case RATE_OFDM_6M:
				prStaRec->u2DesiredNonHTRateSet = BIT(RATE_6M_SW_INDEX);
				break;
			case RATE_CCK_11M_LONG:
				prStaRec->u2DesiredNonHTRateSet = BIT(RATE_11M_SW_INDEX);
				break;
			case RATE_CCK_1M_LONG:
				prStaRec->u2DesiredNonHTRateSet = BIT(RATE_1M_SW_INDEX);
				break;
			case RATE_GF_MCS_0:
				prStaRec->u2DesiredNonHTRateSet = BIT(RATE_HT_PHY_SW_INDEX);
				prStaRec->u2HtCapInfo |= HT_CAP_INFO_HT_GF;
				break;
			case RATE_MM_MCS_7:
				prStaRec->u2DesiredNonHTRateSet = BIT(RATE_HT_PHY_SW_INDEX);
				prStaRec->u2HtCapInfo &= ~HT_CAP_INFO_HT_GF;
#if 0				/* Only for Current Measurement Mode. */
				prStaRec->u2HtCapInfo |= (HT_CAP_INFO_SHORT_GI_20M | HT_CAP_INFO_SHORT_GI_40M);
#endif
				break;
			case RATE_GF_MCS_7:
				prStaRec->u2DesiredNonHTRateSet = BIT(RATE_HT_PHY_SW_INDEX);
				prStaRec->u2HtCapInfo |= HT_CAP_INFO_HT_GF;
				break;
			default:
				prStaRec->u2DesiredNonHTRateSet = BIT(RATE_36M_SW_INDEX);
				break;
			}

			cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_1);

			cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_3);

		}
		break;
	case ENUM_MTK_SLT_FUNC_LP_SET:	/* Reset LP Test Result. */
		{
			P_PARAM_MTK_SLT_LP_TEST_STRUCT_T prLpSetting = (P_PARAM_MTK_SLT_LP_TEST_STRUCT_T) NULL;

			ASSERT(prMtkSltInfo->u4FuncInfoLen == sizeof(PARAM_MTK_SLT_LP_TEST_STRUCT_T));

			prLpSetting = (P_PARAM_MTK_SLT_LP_TEST_STRUCT_T) &prMtkSltInfo->unFuncInfoContent;

			if (prSltInfo->prPseudoBssDesc == NULL) {
				/* Please initial SLT Mode first. */
				break;
			}
			prBssDesc = prSltInfo->prPseudoBssDesc;

			switch (prLpSetting->rLpTestMode) {
			case ENUM_MTK_LP_TEST_NORMAL:
				/* In normal mode, we would use target MAC address to be the BSSID. */
				COPY_MAC_ADDR(prBssDesc->aucBSSID, prBssInfo->aucOwnMacAddr);
				prSltInfo->fgIsDUT = FALSE;
				break;
			case ENUM_MTK_LP_TEST_GOLDEN_SAMPLE:
				/* 1. Lower AIFS of BCN queue.
				 * 2. Fixed Random Number tobe 0.
				 */
				prSltInfo->fgIsDUT = FALSE;
				/* In LP test mode, we would use MAC address of Golden Sample to be the BSSID. */
				COPY_MAC_ADDR(prBssDesc->aucBSSID, prBssInfo->aucOwnMacAddr);
				break;
			case ENUM_MTK_LP_TEST_DUT:
				/* 1. Enter Sleep Mode.
				 * 2. Fix random number a large value & enlarge AIFN of BCN queue.
				 */
				COPY_MAC_ADDR(prBssDesc->aucBSSID, prBssDesc->aucSrcAddr);
				prSltInfo->u4BeaconReceiveCnt = 0;
				prSltInfo->fgIsDUT = TRUE;
				break;
			}

		}

		break;
	default:
		break;
	}

	return WLAN_STATUS_FAILURE;

	return rWlanStatus;
}				/* wlanoidUpdateSLTMode */
#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query NVRAM value.
*
* \param[in] pvAdapter Pointer to the Adapter structure.
* \param[out] pvQueryBuf A pointer to the buffer that holds the result of
*                           the query.
* \param[in] u4QueryBufLen The length of the query buffer.
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                            bytes written into the query buffer. If the call
*                            failed due to invalid length of the query buffer,
*                            returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_FAILURE
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryNvramRead(IN P_ADAPTER_T prAdapter,
		      OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	P_PARAM_CUSTOM_NVRAM_RW_STRUCT_T prNvramRwInfo;
	UINT_16 u2Data;
	BOOLEAN fgStatus;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidQueryNvramRead");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(PARAM_CUSTOM_NVRAM_RW_STRUCT_T);

	if (u4QueryBufferLen < sizeof(PARAM_CUSTOM_NVRAM_RW_STRUCT_T))
		return WLAN_STATUS_INVALID_LENGTH;

	prNvramRwInfo = (P_PARAM_CUSTOM_NVRAM_RW_STRUCT_T) pvQueryBuffer;

	if (prNvramRwInfo->ucEepromMethod == PARAM_EEPROM_READ_METHOD_READ) {
		fgStatus = kalCfgDataRead16(prAdapter->prGlueInfo,
						prNvramRwInfo->ucEepromIndex << 1,	/* change to byte offset */
					    &u2Data);

		if (fgStatus) {
			prNvramRwInfo->u2EepromData = u2Data;
			DBGLOG(REQ, INFO, "NVRAM Read: index=%#X, data=%#02X\r\n",
			       prNvramRwInfo->ucEepromIndex, u2Data);
		} else {
			DBGLOG(REQ, ERROR, "NVRAM Read Failed: index=%#x.\r\n", prNvramRwInfo->ucEepromIndex);
			rStatus = WLAN_STATUS_FAILURE;
		}
	} else if (prNvramRwInfo->ucEepromMethod == PARAM_EEPROM_READ_METHOD_GETSIZE) {
		prNvramRwInfo->u2EepromData = CFG_FILE_WIFI_REC_SIZE;
		DBGLOG(REQ, INFO, "EEPROM size =%d\r\n", prNvramRwInfo->u2EepromData);
	}

	*pu4QueryInfoLen = sizeof(PARAM_CUSTOM_EEPROM_RW_STRUCT_T);

	return rStatus;
}				/* wlanoidQueryNvramRead */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to write NVRAM value.
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[in] pvSetBuffer A pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                           bytes read from the set buffer. If the call failed
*                           due to invalid length of the set buffer, returns
*                           the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_FAILURE
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetNvramWrite(IN P_ADAPTER_T prAdapter,
		     IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_PARAM_CUSTOM_NVRAM_RW_STRUCT_T prNvramRwInfo;
	BOOLEAN fgStatus;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidSetNvramWrite");
	DBGLOG(INIT, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(PARAM_CUSTOM_NVRAM_RW_STRUCT_T);

	if (u4SetBufferLen < sizeof(PARAM_CUSTOM_NVRAM_RW_STRUCT_T))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prNvramRwInfo = (P_PARAM_CUSTOM_NVRAM_RW_STRUCT_T) pvSetBuffer;

	fgStatus = kalCfgDataWrite16(prAdapter->prGlueInfo,
					prNvramRwInfo->ucEepromIndex << 1,	/* change to byte offset */
				     prNvramRwInfo->u2EepromData);

	if (fgStatus == FALSE) {
		DBGLOG(REQ, ERROR, "NVRAM Write Failed.\r\n");
		rStatus = WLAN_STATUS_FAILURE;
	}

	return rStatus;
}				/* wlanoidSetNvramWrite */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to get the config data source type.
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[in] pvSetBuffer A pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                           bytes read from the set buffer. If the call failed
*                           due to invalid length of the set buffer, returns
*                           the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_FAILURE
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryCfgSrcType(IN P_ADAPTER_T prAdapter,
		       OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	ASSERT(prAdapter);

	*pu4QueryInfoLen = sizeof(ENUM_CFG_SRC_TYPE_T);

	if (kalIsConfigurationExist(prAdapter->prGlueInfo) == TRUE)
		*(P_ENUM_CFG_SRC_TYPE_T) pvQueryBuffer = CFG_SRC_TYPE_NVRAM;
	else
		*(P_ENUM_CFG_SRC_TYPE_T) pvQueryBuffer = CFG_SRC_TYPE_EEPROM;

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to get the config data source type.
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[in] pvSetBuffer A pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                           bytes read from the set buffer. If the call failed
*                           due to invalid length of the set buffer, returns
*                           the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_FAILURE
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryEepromType(IN P_ADAPTER_T prAdapter,
		       OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	ASSERT(prAdapter);

	*pu4QueryInfoLen = sizeof(P_ENUM_EEPROM_TYPE_T);

#if CFG_SUPPORT_NIC_CAPABILITY
	if (prAdapter->fgIsEepromUsed == TRUE)
		*(P_ENUM_EEPROM_TYPE_T) pvQueryBuffer = EEPROM_TYPE_PRESENT;
	else
		*(P_ENUM_EEPROM_TYPE_T) pvQueryBuffer = EEPROM_TYPE_NO;
#else
	*(P_ENUM_EEPROM_TYPE_T) pvQueryBuffer = EEPROM_TYPE_NO;
#endif

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to get the config data source type.
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[in] pvSetBuffer A pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                           bytes read from the set buffer. If the call failed
*                           due to invalid length of the set buffer, returns
*                           the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_FAILURE
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetCountryCode(IN P_ADAPTER_T prAdapter,
		      IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	PUINT_8 pucCountry;

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);

	if (regd_is_single_sku_en()) {
		rlmDomainOidSetCountry(prAdapter, pvSetBuffer, u4SetBufferLen);
		*pu4SetInfoLen = u4SetBufferLen;
		return WLAN_STATUS_SUCCESS;
	}

	ASSERT(u4SetBufferLen == 2);

	*pu4SetInfoLen = 2;

	pucCountry = pvSetBuffer;

	prAdapter->rWifiVar.rConnSettings.u2CountryCode = (((UINT_16) pucCountry[0]) << 8) | ((UINT_16) pucCountry[1]);

	/* Force to re-search country code in country domains */
	prAdapter->prDomainInfo = NULL;
	rlmDomainSendCmd(prAdapter, TRUE);

	/* Update supported channel list in channel table based on current country domain */
	wlanUpdateChannelTable(prAdapter->prGlueInfo);

	return WLAN_STATUS_SUCCESS;
}

#if 0
WLAN_STATUS
wlanoidSetNoaParam(IN P_ADAPTER_T prAdapter,
		   IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_PARAM_CUSTOM_NOA_PARAM_STRUCT_T prNoaParam;
	CMD_CUSTOM_NOA_PARAM_STRUCT_T rCmdNoaParam;

	DEBUGFUNC("wlanoidSetNoaParam");
	DBGLOG(INIT, LOUD, "\n");

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

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_SET_NOA_PARAM,
				   TRUE,
				   FALSE,
				   TRUE,
				   nicCmdEventSetCommon,
				   nicOidCmdTimeoutCommon,
				   sizeof(CMD_CUSTOM_NOA_PARAM_STRUCT_T),
				   (PUINT_8) &rCmdNoaParam, pvSetBuffer, u4SetBufferLen);
}

WLAN_STATUS
wlanoidSetOppPsParam(IN P_ADAPTER_T prAdapter,
		     IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_PARAM_CUSTOM_OPPPS_PARAM_STRUCT_T prOppPsParam;
	CMD_CUSTOM_OPPPS_PARAM_STRUCT_T rCmdOppPsParam;

	DEBUGFUNC("wlanoidSetOppPsParam");
	DBGLOG(INIT, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(PARAM_CUSTOM_OPPPS_PARAM_STRUCT_T);

	if (u4SetBufferLen < sizeof(PARAM_CUSTOM_OPPPS_PARAM_STRUCT_T))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prOppPsParam = (P_PARAM_CUSTOM_OPPPS_PARAM_STRUCT_T) pvSetBuffer;

	kalMemZero(&rCmdOppPsParam, sizeof(CMD_CUSTOM_OPPPS_PARAM_STRUCT_T));
	rCmdOppPsParam.u4CTwindowMs = prOppPsParam->u4CTwindowMs;

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_SET_OPPPS_PARAM,
				   TRUE,
				   FALSE,
				   TRUE,
				   nicCmdEventSetCommon,
				   nicOidCmdTimeoutCommon,
				   sizeof(CMD_CUSTOM_OPPPS_PARAM_STRUCT_T),
				   (PUINT_8) &rCmdOppPsParam, pvSetBuffer, u4SetBufferLen);
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
	DBGLOG(INIT, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(PARAM_CUSTOM_UAPSD_PARAM_STRUCT_T);

	if (u4SetBufferLen < sizeof(PARAM_CUSTOM_UAPSD_PARAM_STRUCT_T))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX]);
	prPmProfSetupInfo = &prBssInfo->rPmProfSetupInfo;

	prUapsdParam = (P_PARAM_CUSTOM_UAPSD_PARAM_STRUCT_T) pvSetBuffer;

	kalMemZero(&rCmdUapsdParam, sizeof(CMD_CUSTOM_OPPPS_PARAM_STRUCT_T));
	rCmdUapsdParam.fgEnAPSD = prUapsdParam->fgEnAPSD;
	prAdapter->rWifiVar.fgSupportUAPSD = prUapsdParam->fgEnAPSD;

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

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_SET_UAPSD_PARAM,
				   TRUE,
				   FALSE,
				   TRUE,
				   nicCmdEventSetCommon,
				   nicOidCmdTimeoutCommon,
				   sizeof(CMD_CUSTOM_OPPPS_PARAM_STRUCT_T),
				   (PUINT_8) &rCmdUapsdParam, pvSetBuffer, u4SetBufferLen);
}
#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set BT profile or BT information and the
*        driver will set the built-in PTA configuration into chip.
*
*
* \param[in] prAdapter      Pointer to the Adapter structure.
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
wlanoidSetBT(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{

	P_PTA_IPC_T prPtaIpc;

	DEBUGFUNC("wlanoidSetBT.\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(PTA_IPC_T);
	if (u4SetBufferLen != sizeof(PTA_IPC_T)) {
		/* WARNLOG(("Invalid length %ld\n", u4SetBufferLen)); */
		return WLAN_STATUS_INVALID_LENGTH;
	}

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN, "Fail to set BT profile because of ACPI_D3\n");
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	ASSERT(pvSetBuffer);
	prPtaIpc = (P_PTA_IPC_T) pvSetBuffer;

#if CFG_SUPPORT_BCM && CFG_SUPPORT_BCM_BWCS && CFG_SUPPORT_BCM_BWCS_DEBUG
	DBGLOG(INIT, INFO, "BCM BWCS CMD: BWCS CMD = %02x%02x%02x%02x\n",
	       prPtaIpc->u.aucBTPParams[0], prPtaIpc->u.aucBTPParams[1],
	       prPtaIpc->u.aucBTPParams[2], prPtaIpc->u.aucBTPParams[3]);

	DBGLOG(INIT, INFO,
	       "BCM BWCS CMD: aucBTPParams[0]=%02x, aucBTPParams[1]=%02x, aucBTPParams[2]=%02x, aucBTPParams[3]=%02x.\n",
	       prPtaIpc->u.aucBTPParams[0], prPtaIpc->u.aucBTPParams[1],
	       prPtaIpc->u.aucBTPParams[2], prPtaIpc->u.aucBTPParams[3]);

#endif

	wlanSendSetQueryCmd(prAdapter,
			    CMD_ID_SET_BWCS,
			    TRUE, FALSE, FALSE, NULL, NULL, sizeof(PTA_IPC_T), (PUINT_8) prPtaIpc, NULL, 0);

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query current BT profile and BTCR values
*
* \param[in] prAdapter          Pointer to the Adapter structure.
* \param[in] pvQueryBuffer      Pointer to the buffer that holds the result of
*                               the query.
* \param[in] u4QueryBufferLen   The length of the query buffer.
* \param[out] pu4QueryInfoLen   If the call is successful, returns the number of
*                               bytes written into the query buffer. If the call
*                               failed due to invalid length of the query buffer,
*                               returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryBT(IN P_ADAPTER_T prAdapter,
	       OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
/* P_PARAM_PTA_IPC_T prPtaIpc; */
/* UINT_32 u4QueryBuffLen; */

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(PTA_IPC_T);

	/* Check for query buffer length */
	if (u4QueryBufferLen != sizeof(PTA_IPC_T)) {
		DBGLOG(REQ, WARN, "Invalid length %u\n", u4QueryBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	ASSERT(pvQueryBuffer);
/* prPtaIpc = (P_PTA_IPC_T)pvQueryBuffer; */
/* prPtaIpc->ucCmd = BT_CMD_PROFILE; */
/* prPtaIpc->ucLen = sizeof(prPtaIpc->u); */
/* nicPtaGetProfile(prAdapter, (PUINT_8)&prPtaIpc->u, &u4QueryBuffLen); */

	return WLAN_STATUS_SUCCESS;
}

#if 0
WLAN_STATUS
wlanoidQueryBtSingleAntenna(IN P_ADAPTER_T prAdapter,
			    OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	P_PTA_INFO_T prPtaInfo;
	PUINT_32 pu4SingleAntenna;

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(UINT_32);

	/* Check for query buffer length */
	if (u4QueryBufferLen != sizeof(UINT_32)) {
		DBGLOG(REQ, WARN, "Invalid length %lu\n", u4QueryBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	ASSERT(pvQueryBuffer);

	prPtaInfo = &prAdapter->rPtaInfo;
	pu4SingleAntenna = (PUINT_32) pvQueryBuffer;

	if (prPtaInfo->fgSingleAntenna) {
		/* DBGLOG(INIT, INFO, (KERN_WARNING DRV_NAME"Q Single Ant = 1\r\n")); */
		*pu4SingleAntenna = 1;
	} else {
		/* DBGLOG(INIT, INFO, (KERN_WARNING DRV_NAME"Q Single Ant = 0\r\n")); */
		*pu4SingleAntenna = 0;
	}

	return WLAN_STATUS_SUCCESS;
}

WLAN_STATUS
wlanoidSetBtSingleAntenna(IN P_ADAPTER_T prAdapter,
			  IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{

	PUINT_32 pu4SingleAntenna;
	UINT_32 u4SingleAntenna;
	P_PTA_INFO_T prPtaInfo;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	prPtaInfo = &prAdapter->rPtaInfo;

	*pu4SetInfoLen = sizeof(UINT_32);
	if (u4SetBufferLen != sizeof(UINT_32))
		return WLAN_STATUS_INVALID_LENGTH;

	if (IS_ARB_IN_RFTEST_STATE(prAdapter))
		return WLAN_STATUS_SUCCESS;

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN, "Fail to set antenna because of ACPI_D3\n");
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	ASSERT(pvSetBuffer);
	pu4SingleAntenna = (PUINT_32) pvSetBuffer;
	u4SingleAntenna = *pu4SingleAntenna;

	if (u4SingleAntenna == 0) {
		/* DBGLOG(INIT, INFO, (KERN_WARNING DRV_NAME"Set Single Ant = 0\r\n")); */
		prPtaInfo->fgSingleAntenna = FALSE;
	} else {
		/* DBGLOG(INIT, INFO, (KERN_WARNING DRV_NAME"Set Single Ant = 1\r\n")); */
		prPtaInfo->fgSingleAntenna = TRUE;
	}
	ptaFsmRunEventSetConfig(prAdapter, &prPtaInfo->rPtaParam);

	return WLAN_STATUS_SUCCESS;
}

#if CFG_SUPPORT_BCM && CFG_SUPPORT_BCM_BWCS
WLAN_STATUS
wlanoidQueryPta(IN P_ADAPTER_T prAdapter,
		OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	P_PTA_INFO_T prPtaInfo;
	PUINT_32 pu4Pta;

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(UINT_32);

	/* Check for query buffer length */
	if (u4QueryBufferLen != sizeof(UINT_32)) {
		DBGLOG(REQ, WARN, "Invalid length %lu\n", u4QueryBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	ASSERT(pvQueryBuffer);

	prPtaInfo = &prAdapter->rPtaInfo;
	pu4Pta = (PUINT_32) pvQueryBuffer;

	if (prPtaInfo->fgEnabled) {
		/* DBGLOG(INIT, INFO, (KERN_WARNING DRV_NAME"PTA = 1\r\n")); */
		*pu4Pta = 1;
	} else {
		/* DBGLOG(INIT, INFO, (KERN_WARNING DRV_NAME"PTA = 0\r\n")); */
		*pu4Pta = 0;
	}

	return WLAN_STATUS_SUCCESS;
}

WLAN_STATUS
wlanoidSetPta(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	PUINT_32 pu4PtaCtrl;
	UINT_32 u4PtaCtrl;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(UINT_32);
	if (u4SetBufferLen != sizeof(UINT_32))
		return WLAN_STATUS_INVALID_LENGTH;

	if (IS_ARB_IN_RFTEST_STATE(prAdapter))
		return WLAN_STATUS_SUCCESS;

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN, "Fail to set BT setting because of ACPI_D3\n");
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	ASSERT(pvSetBuffer);
	pu4PtaCtrl = (PUINT_32) pvSetBuffer;
	u4PtaCtrl = *pu4PtaCtrl;

	if (u4PtaCtrl == 0) {
		/* DBGLOG(INIT, INFO, (KERN_WARNING DRV_NAME"Set Pta= 0\r\n")); */
		nicPtaSetFunc(prAdapter, FALSE);
	} else {
		/* DBGLOG(INIT, INFO, (KERN_WARNING DRV_NAME"Set Pta= 1\r\n")); */
		nicPtaSetFunc(prAdapter, TRUE);
	}

	return WLAN_STATUS_SUCCESS;
}
#endif

#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set Tx power profile.
*
*
* \param[in] prAdapter      Pointer to the Adapter structure.
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
wlanoidSetTxPower(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_SET_TXPWR_CTRL_T pTxPwr = (P_SET_TXPWR_CTRL_T) pvSetBuffer;
	P_SET_TXPWR_CTRL_T prCmd;
	UINT_32 i;
	WLAN_STATUS rStatus;

	DEBUGFUNC("wlanoidSetTxPower");
	DBGLOG(REQ, LOUD, "\r\n");

	prCmd = cnmMemAlloc(prAdapter, RAM_TYPE_BUF, sizeof(SET_TXPWR_CTRL_T));
	kalMemZero(prCmd, sizeof(SET_TXPWR_CTRL_T));
	prCmd->c2GLegacyStaPwrOffset = pTxPwr->c2GLegacyStaPwrOffset;
	prCmd->c2GHotspotPwrOffset = pTxPwr->c2GHotspotPwrOffset;
	prCmd->c2GP2pPwrOffset = pTxPwr->c2GP2pPwrOffset;
	prCmd->c2GBowPwrOffset = pTxPwr->c2GBowPwrOffset;
	prCmd->c5GLegacyStaPwrOffset = pTxPwr->c5GLegacyStaPwrOffset;
	prCmd->c5GHotspotPwrOffset = pTxPwr->c5GHotspotPwrOffset;
	prCmd->c5GP2pPwrOffset = pTxPwr->c5GP2pPwrOffset;
	prCmd->c5GBowPwrOffset = pTxPwr->c5GBowPwrOffset;
	prCmd->ucConcurrencePolicy = pTxPwr->ucConcurrencePolicy;
	for (i = 0; i < 14; i++)
		prCmd->acTxPwrLimit2G[i] = pTxPwr->acTxPwrLimit2G[i];

	for (i = 0; i < 4; i++)
		prCmd->acTxPwrLimit5G[i] = pTxPwr->acTxPwrLimit5G[i];

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);

#if 0
	DBGLOG(INIT, INFO, "c2GLegacyStaPwrOffset=%d\n", pTxPwr->c2GLegacyStaPwrOffset);
	DBGLOG(INIT, INFO, "c2GHotspotPwrOffset=%d\n", pTxPwr->c2GHotspotPwrOffset);
	DBGLOG(INIT, INFO, "c2GP2pPwrOffset=%d\n", pTxPwr->c2GP2pPwrOffset);
	DBGLOG(INIT, INFO, "c2GBowPwrOffset=%d\n", pTxPwr->c2GBowPwrOffset);
	DBGLOG(INIT, INFO, "c5GLegacyStaPwrOffset=%d\n", pTxPwr->c5GLegacyStaPwrOffset);
	DBGLOG(INIT, INFO, "c5GHotspotPwrOffset=%d\n", pTxPwr->c5GHotspotPwrOffset);
	DBGLOG(INIT, INFO, "c5GP2pPwrOffset=%d\n", pTxPwr->c5GP2pPwrOffset);
	DBGLOG(INIT, INFO, "c5GBowPwrOffset=%d\n", pTxPwr->c5GBowPwrOffset);
	DBGLOG(INIT, INFO, "ucConcurrencePolicy=%d\n", pTxPwr->ucConcurrencePolicy);

	for (i = 0; i < 14; i++)
		DBGLOG(INIT, INFO, "acTxPwrLimit2G[%d]=%d\n", i, pTxPwr->acTxPwrLimit2G[i]);

	for (i = 0; i < 4; i++)
		DBGLOG(INIT, INFO, "acTxPwrLimit5G[%d]=%d\n", i, pTxPwr->acTxPwrLimit5G[i]);
#endif

	rStatus = wlanSendSetQueryCmd(prAdapter,	/* prAdapter */
				      CMD_ID_SET_TXPWR_CTRL,	/* ucCID */
				      TRUE,	/* fgSetQuery */
				      FALSE,	/* fgNeedResp */
				      g_fgIsOid,	/* fgIsOid */
				      nicCmdEventSetCommon, nicOidCmdTimeoutCommon,
					  sizeof(SET_TXPWR_CTRL_T),	/* u4SetQueryInfoLen */
				      (PUINT_8) prCmd,	/* pucInfoBuffer */
				      NULL,	/* pvSetQueryBuffer */
				      0	/* u4SetQueryBufferLen */
	    );

	/* ASSERT(rStatus == WLAN_STATUS_PENDING); */
	cnmMemFree(prAdapter, prCmd);

	return rStatus;

}

WLAN_STATUS wlanSendMemDumpCmd(IN P_ADAPTER_T prAdapter, IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen)
{
	P_PARAM_CUSTOM_MEM_DUMP_STRUCT_T prMemDumpInfo;
	P_CMD_DUMP_MEM prCmdDumpMem;
	CMD_DUMP_MEM rCmdDumpMem;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4MemSize = PARAM_MEM_DUMP_MAX_SIZE;

	UINT_32 u4RemainLeng = 0;
	UINT_32 u4CurAddr = 0;
	UINT_8 ucFragNum = 0;

	prCmdDumpMem = &rCmdDumpMem;
	prMemDumpInfo = (P_PARAM_CUSTOM_MEM_DUMP_STRUCT_T) pvQueryBuffer;

	u4RemainLeng = prMemDumpInfo->u4RemainLength;
	u4CurAddr = prMemDumpInfo->u4Address + prMemDumpInfo->u4Length;
	ucFragNum = prMemDumpInfo->ucFragNum + 1;

	/* Query. If request length is larger than max length, do it as ping pong.
	 * Send a command and wait for a event. Send next command while the event is received.
	 *
	 */
	do {
		UINT_32 u4CurLeng = 0;

		if (u4RemainLeng > u4MemSize) {
			u4CurLeng = u4MemSize;
			u4RemainLeng -= u4MemSize;
		} else {
			u4CurLeng = u4RemainLeng;
			u4RemainLeng = 0;
		}

		prCmdDumpMem->u4Address = u4CurAddr;
		prCmdDumpMem->u4Length = u4CurLeng;
		prCmdDumpMem->u4RemainLength = u4RemainLeng;
		prCmdDumpMem->ucFragNum = ucFragNum;
#if CFG_SUPPORT_QA_TOOL
		prCmdDumpMem->u4IcapContent = prMemDumpInfo->u4IcapContent;
#endif /* CFG_SUPPORT_QA_TOOL */

		DBGLOG(REQ, TRACE, "[%d] 0x%X, len %u, remain len %u\n",
		       ucFragNum, prCmdDumpMem->u4Address, prCmdDumpMem->u4Length, prCmdDumpMem->u4RemainLength);

		rStatus = wlanSendSetQueryCmd(prAdapter,
					      CMD_ID_DUMP_MEM,
					      FALSE,
					      TRUE,
					      TRUE,
					      nicCmdEventQueryMemDump,
					      nicOidCmdTimeoutCommon,
					      sizeof(CMD_DUMP_MEM),
					      (PUINT_8) prCmdDumpMem, pvQueryBuffer, u4QueryBufferLen);

	} while (FALSE);

	return rStatus;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to dump memory.
*
* \param[in] pvAdapter Pointer to the Adapter structure.
* \param[out] pvQueryBuf A pointer to the buffer that holds the result of
*                           the query.
* \param[in] u4QueryBufLen The length of the query buffer.
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                            bytes written into the query buffer. If the call
*                            failed due to invalid length of the query buffer,
*                            returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryMemDump(IN P_ADAPTER_T prAdapter,
		    IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	P_PARAM_CUSTOM_MEM_DUMP_STRUCT_T prMemDumpInfo;

	DEBUGFUNC("wlanoidQueryMemDump");
	DBGLOG(INIT, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(UINT_32);

	prMemDumpInfo = (P_PARAM_CUSTOM_MEM_DUMP_STRUCT_T) pvQueryBuffer;
	DBGLOG(REQ, TRACE, "Dump 0x%X, len %u\n",
			prMemDumpInfo->u4Address, prMemDumpInfo->u4Length);

	prMemDumpInfo->u4RemainLength = prMemDumpInfo->u4Length;
	prMemDumpInfo->u4Length = 0;
	prMemDumpInfo->ucFragNum = 0;

	return wlanSendMemDumpCmd(prAdapter, pvQueryBuffer, u4QueryBufferLen);

}				/* end of wlanoidQueryMcrRead() */

#if CFG_ENABLE_WIFI_DIRECT
/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to set the p2p mode.
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
wlanoidSetP2pMode(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	WLAN_STATUS status = WLAN_STATUS_SUCCESS;
	P_PARAM_CUSTOM_P2P_SET_STRUCT_T prSetP2P = (P_PARAM_CUSTOM_P2P_SET_STRUCT_T) NULL;
	/* P_MSG_P2P_NETDEV_REGISTER_T prP2pNetdevRegMsg = (P_MSG_P2P_NETDEV_REGISTER_T)NULL; */
	DEBUGFUNC("wlanoidSetP2pMode");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(PARAM_CUSTOM_P2P_SET_STRUCT_T);
	if (u4SetBufferLen < sizeof(PARAM_CUSTOM_P2P_SET_STRUCT_T)) {
		DBGLOG(REQ, WARN, "Invalid length %u\n", u4SetBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	prSetP2P = (P_PARAM_CUSTOM_P2P_SET_STRUCT_T) pvSetBuffer;

	DBGLOG(P2P, INFO, "Set P2P enable[%d] mode[%d]\n",
			prSetP2P->u4Enable, prSetP2P->u4Mode);

	/*
	 *    enable = 1, mode = 0  => init P2P network
	 *    enable = 1, mode = 1  => init Soft AP network
	 *    enable = 0            => uninit P2P/AP network
	 *    enable = 1, mode = 2  => init dual Soft AP network
	 *    enable = 1, mode = 3  => init AP+P2P network
	 */


	DBGLOG(P2P, INFO, "P2P Compile as (%d)p2p-like interface\n", KAL_P2P_NUM);

	if (prSetP2P->u4Mode >= RUNNING_P2P_MODE_NUM) {
		DBGLOG(P2P, ERROR, "P2P interface mode(%d) is wrong\n", prSetP2P->u4Mode);
		ASSERT(0);
	}

	if (prSetP2P->u4Enable) {
		p2pSetMode(prSetP2P->u4Mode);

		if (p2pLaunch(prAdapter->prGlueInfo)) {
			/* ToDo:: ASSERT */
			ASSERT(prAdapter->fgIsP2PRegistered);
		} else {
			status = WLAN_STATUS_FAILURE;
		}

	} else {
		if (prAdapter->fgIsP2PRegistered)
			p2pRemove(prAdapter->prGlueInfo);

	}

#if 0
	prP2pNetdevRegMsg = (P_MSG_P2P_NETDEV_REGISTER_T) cnmMemAlloc(prAdapter,
								      RAM_TYPE_MSG,
								      (sizeof(MSG_P2P_NETDEV_REGISTER_T)));

	if (prP2pNetdevRegMsg == NULL) {
		ASSERT(FALSE);
		status = WLAN_STATUS_RESOURCES;
		return status;
	}

	prP2pNetdevRegMsg->rMsgHdr.eMsgId = MID_MNY_P2P_NET_DEV_REGISTER;
	prP2pNetdevRegMsg->fgIsEnable = (prSetP2P->u4Enable == 1) ? TRUE : FALSE;
	prP2pNetdevRegMsg->ucMode = (UINT_8) prSetP2P->u4Mode;

	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prP2pNetdevRegMsg, MSG_SEND_METHOD_BUF);
#endif

	return status;

}
#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set the GTK rekey data
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[in] pvSetBuffer A pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                           bytes read from the set buffer. If the call failed
*                           due to invalid length of the set buffer, returns
*                           the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_BUFFER_TOO_SHORT
* \retval WLAN_STATUS_INVALID_DATA
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetGtkRekeyData(IN P_ADAPTER_T prAdapter,
		       IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_GLUE_INFO_T prGlueInfo;
	P_CMD_INFO_T prCmdInfo;
	P_WIFI_CMD_T prWifiCmd;
	UINT_8 ucCmdSeqNum;
	P_BSS_INFO_T prBssInfo;

	DBGLOG(REQ, INFO, "wlanoidSetGtkRekeyData\n");

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(RSN, WARN, "Fail in set rekey! (Adapter not ready). ACPI=D%d, Radio=%d\n",
				   prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	prBssInfo = prAdapter->prAisBssInfo;

	*pu4SetInfoLen = u4SetBufferLen;

	prGlueInfo = prAdapter->prGlueInfo;
	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, (CMD_HDR_SIZE + sizeof(PARAM_GTK_REKEY_DATA)));

	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}
	/* increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);
	DBGLOG(REQ, INFO, "ucCmdSeqNum = %d\n", ucCmdSeqNum);

	/* compose PARAM_GTK_REKEY_DATA cmd pkt */
	prCmdInfo->eCmdType = COMMAND_TYPE_NETWORK_IOCTL;
	prCmdInfo->u2InfoBufLen = CMD_HDR_SIZE + sizeof(PARAM_GTK_REKEY_DATA);
	prCmdInfo->pfCmdDoneHandler = nicCmdEventSetCommon;
	prCmdInfo->pfCmdTimeoutHandler = nicOidCmdTimeoutCommon;
	prCmdInfo->fgIsOid = g_fgIsOid;
	prCmdInfo->ucCID = CMD_ID_SET_GTK_REKEY_DATA;
	prCmdInfo->fgSetQuery = TRUE;
	prCmdInfo->fgNeedResp = FALSE;
	prCmdInfo->ucCmdSeqNum = ucCmdSeqNum;
	prCmdInfo->u4SetInfoLen = u4SetBufferLen;
	prCmdInfo->pvInformationBuffer = pvSetBuffer;
	prCmdInfo->u4InformationBufferLength = u4SetBufferLen;

	/* Setup WIFI_CMD_T */
	prWifiCmd = (P_WIFI_CMD_T) (prCmdInfo->pucInfoBuffer);
	prWifiCmd->u2TxByteCount = prCmdInfo->u2InfoBufLen;
	prWifiCmd->u2PQ_ID = CMD_PQ_ID;
	prWifiCmd->ucPktTypeID = CMD_PACKET_TYPE_ID;
	prWifiCmd->ucCID = prCmdInfo->ucCID;
	prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
	prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;

	kalMemCopy(prWifiCmd->aucBuffer, (PUINT_8) pvSetBuffer, u4SetBufferLen);

	/* insert into prCmdQueue */
	kalEnqueueCommand(prGlueInfo, (P_QUE_ENTRY_T) prCmdInfo);

	/* wakeup txServiceThread later */
	GLUE_SET_EVENT(prGlueInfo);

	return WLAN_STATUS_PENDING;

}				/* wlanoidSetGtkRekeyData */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to request starting of schedule scan
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
*
* \note The setting buffer PARAM_SCHED_SCAN_REQUEST_EXT_T
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetStartSchedScan(IN P_ADAPTER_T prAdapter,
			 IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_PARAM_SCHED_SCAN_REQUEST prSchedScanRequest;

	DEBUGFUNC("wlanoidSetStartSchedScan()");

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in set scheduled scan! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	ASSERT(pu4SetInfoLen);
	*pu4SetInfoLen = 0;

	if (u4SetBufferLen != sizeof(PARAM_SCHED_SCAN_REQUEST))
		return WLAN_STATUS_INVALID_LENGTH;
	else if (pvSetBuffer == NULL)
		return WLAN_STATUS_INVALID_DATA;
	else if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) == PARAM_MEDIA_STATE_CONNECTED
		 && prAdapter->fgEnOnlineScan == FALSE)
		return WLAN_STATUS_FAILURE;

	if (prAdapter->fgIsRadioOff) {
		DBGLOG(REQ, WARN, "Return from BSSID list scan! (radio off). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_SUCCESS;
	}

	prSchedScanRequest = (P_PARAM_SCHED_SCAN_REQUEST) pvSetBuffer;

	if (scnFsmSchedScanRequest(prAdapter,
				   (UINT_8) (prSchedScanRequest->u4SsidNum),
				   prSchedScanRequest->arSsid,
				   prSchedScanRequest->u4IELength,
				   prSchedScanRequest->pucIE, prSchedScanRequest->u2ScanInterval) == TRUE)
		return WLAN_STATUS_PENDING;
	else
		return WLAN_STATUS_FAILURE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to request termination of schedule scan
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
*
* \note The setting buffer PARAM_SCHED_SCAN_REQUEST_EXT_T
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetStopSchedScan(IN P_ADAPTER_T prAdapter,
			IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	ASSERT(prAdapter);

	/* ask SCN module to stop scan request */
	if (scnFsmSchedScanStopRequest(prAdapter) == TRUE)
		return WLAN_STATUS_PENDING;
	else
		return WLAN_STATUS_FAILURE;
}

#if CFG_M0VE_BA_TO_DRIVER
/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to reset BA scoreboard.
*
*
* \param[in] prAdapter      Pointer to the Adapter structure.
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
WLAN_STATUS wlanoidResetBAScoreboard(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen)
{
	WLAN_STATUS rStatus;

	DEBUGFUNC("wlanoidResetBAScoreboard");
	DBGLOG(REQ, WARN, "[Puff]wlanoidResetBAScoreboard\n");

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);

	rStatus = wlanSendSetQueryCmd(prAdapter,	/* prAdapter */
				      CMD_ID_RESET_BA_SCOREBOARD,	/* ucCID */
				      TRUE,	/* fgSetQuery */
				      FALSE,	/* fgNeedResp */
				      TRUE,	/* fgIsOid */
				      NULL,	/* pfCmdDoneHandler */
				      NULL,	/* pfCmdTimeoutHandler */
				      u4SetBufferLen,	/* u4SetQueryInfoLen */
				      (PUINT_8) pvSetBuffer,	/* pucInfoBuffer */
				      NULL,	/* pvSetQueryBuffer */
				      0	/* u4SetQueryBufferLen */
	    );

	/* ASSERT(rStatus == WLAN_STATUS_PENDING); */

	return rStatus;

}

#endif

#if CFG_SUPPORT_BATCH_SCAN

#define CMD_WLS_BATCHING        "WLS_BATCHING"

#define BATCHING_SET            "SET"
#define BATCHING_GET            "GET"
#define BATCHING_STOP           "STOP"

#define PARAM_SCANFREQ          "SCANFREQ"
#define PARAM_MSCAN             "MSCAN"
#define PARAM_BESTN             "BESTN"
#define PARAM_CHANNEL           "CHANNEL"
#define PARAM_RTT               "RTT"

WLAN_STATUS
batchSetCmd(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4WritenLen)
{
	P_CHANNEL_INFO_T prRfChannelInfo;
	CMD_BATCH_REQ_T rCmdBatchReq;

	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	PCHAR head, p, p2;
	UINT_32 tokens;
	INT_32 scanfreq, mscan, bestn, rtt;
	char *pcTemp;
	/* CHAR c_scanfreq[4], c_mscan[4], c_bestn[4], c_rtt[4], c_channel[100]; */
	/* INT32 ch_type; */
	UINT_32 u4Value = 0;
	INT_32 i4Ret = 0;

	DBGLOG(SCN, TRACE, "[BATCH] command=%s, len=%u\n",
			(char *)pvSetBuffer, u4SetBufferLen);

	if (!pu4WritenLen)
		return -EINVAL;
	*pu4WritenLen = 0;

	if (u4SetBufferLen < kalStrLen(CMD_WLS_BATCHING)) {
		DBGLOG(SCN, TRACE, "[BATCH] invalid len %d\n", u4SetBufferLen);
		return -EINVAL;
	}

	head = pvSetBuffer + kalStrLen(CMD_WLS_BATCHING) + 1;
	kalMemSet(&rCmdBatchReq, 0, sizeof(CMD_BATCH_REQ_T));

	if (!kalStrnCmp(head, BATCHING_SET, kalStrLen(BATCHING_SET))) {

		DBGLOG(SCN, TRACE, "XXX Start Batch Scan XXX\n");

		head += kalStrLen(BATCHING_SET) + 1;

		/* SCANFREQ, MSCAN, BESTN */
		tokens = sscanf(head, "SCANFREQ=%d MSCAN=%d BESTN=%d",
				&scanfreq, &mscan, &bestn);
		if (tokens != 3) {
			DBGLOG(SCN, TRACE,
			       "[BATCH] Parse fail: tokens=%d, SCANFREQ=%d MSCAN=%d BESTN=%d\n",
			       tokens, scanfreq, mscan, bestn);
			return -EINVAL;
		}
		/* RTT */
		p = kalStrStr(head, PARAM_RTT);
		if (!p) {
			DBGLOG(SCN, TRACE, "[BATCH] Parse RTT fail. head=%s\n", head);
			return -EINVAL;
		}
		tokens = sscanf(p, "RTT=%d", &rtt);
		if (tokens != 1) {
			DBGLOG(SCN, TRACE, "[BATCH] Parse fail: tokens=%d, rtt=%d\n", tokens, rtt);
			return -EINVAL;
		}
		/* CHANNEL */
		p = kalStrStr(head, PARAM_CHANNEL);
		if (!p) {
			DBGLOG(SCN, TRACE, "[BATCH] Parse CHANNEL fail(1)\n");
			return -EINVAL;
		}
		head = p;
		p = kalStrChr(head, '>');
		if (!p) {
			DBGLOG(SCN, TRACE, "[BATCH] Parse CHANNEL fail(2)\n");
			return -EINVAL;
		}
		/* else {
		 *p = '.'; // remove '>' because sscanf can not parse <%s>
		 *}
		 */
		/*tokens = sscanf(head, "CHANNEL=<%s", c_channel);
		 *  if (tokens != 1) {
		 *  DBGLOG(SCN, TRACE, "[BATCH] Parse fail: tokens=%d, CHANNEL=<%s>\n",
		 *  tokens, c_channel);
		 *  return -EINVAL;
		 *  }
		 */
		rCmdBatchReq.ucChannelType = SCAN_CHANNEL_SPECIFIED;
		rCmdBatchReq.ucChannelListNum = 0;
		prRfChannelInfo = &rCmdBatchReq.arChannelList[0];
		p = head + kalStrLen(PARAM_CHANNEL) + 2;	/* c_channel; */
		pcTemp = (char *)p;
		while ((p2 = kalStrSep(&pcTemp, ",")) != NULL) {
			if (p2 == NULL || *p2 == 0)
				break;
			if (*p2 == '\0')
				continue;
			if (*p2 == 'A') {
				rCmdBatchReq.ucChannelType =
				    rCmdBatchReq.ucChannelType ==
				    SCAN_CHANNEL_2G4 ? SCAN_CHANNEL_FULL : SCAN_CHANNEL_5G;
			} else if (*p2 == 'B') {
				rCmdBatchReq.ucChannelType =
				    rCmdBatchReq.ucChannelType ==
				    SCAN_CHANNEL_5G ? SCAN_CHANNEL_FULL : SCAN_CHANNEL_2G4;
			} else {

				/* Translate Freq from MHz to channel number. */
				/* prRfChannelInfo->ucChannelNum = kalStrtol(p2, NULL, 0); */
				i4Ret = kalkStrtou32(p2, 0, &u4Value);
				if (i4Ret)
					DBGLOG(SCN, TRACE, "parse ucChannelNum error i4Ret=%d\n", i4Ret);
				prRfChannelInfo->ucChannelNum = (UINT_8) u4Value;
				DBGLOG(SCN, TRACE, "Scanning Channel:%d,  freq: %d\n",
				       prRfChannelInfo->ucChannelNum,
				       nicChannelNum2Freq(prRfChannelInfo->ucChannelNum));
				prRfChannelInfo->ucBand = prRfChannelInfo->ucChannelNum < 15 ? BAND_2G4 : BAND_5G;

				rCmdBatchReq.ucChannelListNum++;
				if (rCmdBatchReq.ucChannelListNum >= 32)
					break;
				prRfChannelInfo++;
			}
		}

		/* set channel for test */
#if 0
		rCmdBatchReq.ucChannelType = 4;	/* SCAN_CHANNEL_SPECIFIED; */
		rCmdBatchReq.ucChannelListNum = 0;
		prRfChannelInfo = &rCmdBatchReq.arChannelList[0];
		for (i = 1; i <= 14; i++) {

			/* filter out some */
			if (i == 1 || i == 5 || i == 11)
				continue;

			/* Translate Freq from MHz to channel number. */
			prRfChannelInfo->ucChannelNum = i;
			DBGLOG(SCN, TRACE, "Scanning Channel:%d,  freq: %d\n",
			       prRfChannelInfo->ucChannelNum, nicChannelNum2Freq(prRfChannelInfo->ucChannelNum));
			prRfChannelInfo->ucBand = BAND_2G4;

			rCmdBatchReq.ucChannelListNum++;
			prRfChannelInfo++;
		}
#endif
#if 0
		rCmdBatchReq.ucChannelType = 0;	/* SCAN_CHANNEL_FULL; */
#endif

		rCmdBatchReq.u4Scanfreq = scanfreq;
		rCmdBatchReq.ucMScan = mscan > CFG_BATCH_MAX_MSCAN ? CFG_BATCH_MAX_MSCAN : mscan;
		rCmdBatchReq.ucBestn = bestn;
		rCmdBatchReq.ucRtt = rtt;
		DBGLOG(SCN, TRACE, "[BATCH] SCANFREQ=%d MSCAN=%d BESTN=%d RTT=%d\n",
		       rCmdBatchReq.u4Scanfreq, rCmdBatchReq.ucMScan, rCmdBatchReq.ucBestn, rCmdBatchReq.ucRtt);

		if (rCmdBatchReq.ucChannelType != SCAN_CHANNEL_SPECIFIED) {
			DBGLOG(SCN, TRACE, "[BATCH] CHANNELS = %s\n",
			       rCmdBatchReq.ucChannelType ==
			       SCAN_CHANNEL_FULL ? "FULL" : rCmdBatchReq.ucChannelType ==
			       SCAN_CHANNEL_2G4 ? "2.4G all" : "5G all");
		} else {
			DBGLOG(SCN, TRACE, "[BATCH] CHANNEL list\n");
			prRfChannelInfo = &rCmdBatchReq.arChannelList[0];
			for (tokens = 0; tokens < rCmdBatchReq.ucChannelListNum; tokens++) {
				DBGLOG(SCN, TRACE, "[BATCH] %s, %d\n",
				       prRfChannelInfo->ucBand ==
				       BAND_2G4 ? "2.4G" : "5G", prRfChannelInfo->ucChannelNum);
				prRfChannelInfo++;
			}
		}

		rCmdBatchReq.ucSeqNum = 1;
		rCmdBatchReq.ucNetTypeIndex = KAL_NETWORK_TYPE_AIS_INDEX;
		rCmdBatchReq.ucCmd = SCAN_BATCH_REQ_START;

		*pu4WritenLen = kalSnprintf(pvSetBuffer, 3, "%d", rCmdBatchReq.ucMScan);

	} else if (!kalStrnCmp(head, BATCHING_STOP, kalStrLen(BATCHING_STOP))) {

		DBGLOG(SCN, TRACE, "XXX Stop Batch Scan XXX\n");

		rCmdBatchReq.ucSeqNum = 1;
		rCmdBatchReq.ucNetTypeIndex = KAL_NETWORK_TYPE_AIS_INDEX;
		rCmdBatchReq.ucCmd = SCAN_BATCH_REQ_STOP;
	} else {
		return -EINVAL;
	}

	wlanSendSetQueryCmd(prAdapter,
			    CMD_ID_SET_BATCH_REQ,
			    TRUE, FALSE, g_fgIsOid, NULL, NULL,
			    sizeof(CMD_BATCH_REQ_T),
			    (PUINT_8)(&rCmdBatchReq), NULL, 0);

	/* kalMemSet(pvSetBuffer, 0, u4SetBufferLen); */
	/* rStatus = kalSnprintf(pvSetBuffer, 2, "%s", "OK"); */

/* exit: */
	return rStatus;
}

WLAN_STATUS
batchGetCmd(IN P_ADAPTER_T prAdapter,
	    OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	CMD_BATCH_REQ_T rCmdBatchReq;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	P_EVENT_BATCH_RESULT_T prEventBatchResult;
	/* UINT_32 i; */

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	prEventBatchResult = (P_EVENT_BATCH_RESULT_T) pvQueryBuffer;

	DBGLOG(SCN, TRACE, "XXX Get Batch Scan Result (%d) XXX\n", prEventBatchResult->ucScanCount);

	*pu4QueryInfoLen = sizeof(EVENT_BATCH_RESULT_T);

	rCmdBatchReq.ucSeqNum = 2;
	rCmdBatchReq.ucCmd = SCAN_BATCH_REQ_RESULT;
	rCmdBatchReq.ucMScan = prEventBatchResult->ucScanCount;	/* Get which round result */

	rStatus = wlanSendSetQueryCmd(prAdapter,
				      CMD_ID_SET_BATCH_REQ,
				      FALSE,
				      TRUE,
				      g_fgIsOid,
				      nicCmdEventBatchScanResult,
				      nicOidCmdTimeoutCommon,
				      sizeof(CMD_BATCH_REQ_T),
				      (PUINT_8) &rCmdBatchReq, (PVOID) pvQueryBuffer, u4QueryBufferLen);

	return rStatus;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in] prAdapter Pointer to the Adapter structure
* \param[in] pvSetBuffer A pointer to the buffer that holds the data to be set
* \param[in] u4SetBufferLen The length of the set buffer
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*   bytes read from the set buffer. If the call failed due to invalid length of
*   the set buffer, returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_DATA If new setting value is wrong.
* \retval WLAN_STATUS_INVALID_LENGTH
*
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetBatchScanReq(IN P_ADAPTER_T prAdapter,
		       IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	return batchSetCmd(prAdapter, pvSetBuffer, u4SetBufferLen, pu4SetInfoLen);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in] pvAdapter Pointer to the Adapter structure.
* \param[out] pvQueryBuffer A pointer to the buffer that holds the result of
*                           the query.
* \param[in] u4QueryBufferLen The length of the query buffer.
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                            bytes written into the query buffer. If the call
*                            failed due to invalid length of the query buffer,
*                            returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryBatchScanResult(IN P_ADAPTER_T prAdapter,
			    OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	return batchGetCmd(prAdapter, pvQueryBuffer, u4QueryBufferLen, pu4QueryInfoLen);

}				/* end of wlanoidQueryBatchScanResult() */

#endif /* CFG_SUPPORT_BATCH_SCAN */

#if CFG_SUPPORT_PASSPOINT
/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called by HS2.0 to set the assoc info, which is needed to add to
*          Association request frame while join HS2.0 AP.
*
* \param[in] prAdapter Pointer to the Adapter structure
* \param[in] pvSetBuffer A pointer to the buffer that holds the data to be set
* \param[in] u4SetBufferLen The length of the set buffer
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*   bytes read from the set buffer. If the call failed due to invalid length of
*   the set buffer, returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_DATA If new setting value is wrong.
* \retval WLAN_STATUS_INVALID_LENGTH
*
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetHS20Info(IN P_ADAPTER_T prAdapter,
		   IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_IE_HS20_INDICATION_T prHS20IndicationIe;

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	DEBUGFUNC("wlanoidSetHS20AssocInfo");
	DBGLOG(REQ, LOUD, "\r\n");

	if (u4SetBufferLen == 0)
		return WLAN_STATUS_INVALID_LENGTH;

	*pu4SetInfoLen = u4SetBufferLen;

	prHS20IndicationIe = (P_IE_HS20_INDICATION_T) pvSetBuffer;

	prAdapter->prGlueInfo->ucHotspotConfig = prHS20IndicationIe->ucHotspotConfig;
	prAdapter->prGlueInfo->fgConnectHS20AP = TRUE;

	DBGLOG(SEC, TRACE, "HS20 IE sz %ld\n", u4SetBufferLen);

	kalMemCopy(prAdapter->prGlueInfo->aucHS20AssocInfoIE, pvSetBuffer, u4SetBufferLen);
	prAdapter->prGlueInfo->u2HS20AssocInfoIELen = (UINT_16) u4SetBufferLen;
	DBGLOG(SEC, TRACE, "HS20 Assoc Info IE sz %ld\n", u4SetBufferLen);

	return WLAN_STATUS_SUCCESS;

}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called by WSC to set the assoc info, which is needed to add to
*          Association request frame while join WPS AP.
*
* \param[in] prAdapter Pointer to the Adapter structure
* \param[in] pvSetBuffer A pointer to the buffer that holds the data to be set
* \param[in] u4SetBufferLen The length of the set buffer
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*   bytes read from the set buffer. If the call failed due to invalid length of
*   the set buffer, returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_DATA If new setting value is wrong.
* \retval WLAN_STATUS_INVALID_LENGTH
*
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetInterworkingInfo(IN P_ADAPTER_T prAdapter,
			   IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
#if 0
	P_HS20_INFO_T prHS20Info = NULL;
	P_IE_INTERWORKING_T prInterWorkingIe;

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	prHS20Info = &(prAdapter->rWifiVar.rHS20Info);

	DEBUGFUNC("wlanoidSetInterworkingInfo");
	DBGLOG(REQ, TRACE, "\r\n");

	if (u4SetBufferLen == 0)
		return WLAN_STATUS_INVALID_LENGTH;

	*pu4SetInfoLen = u4SetBufferLen;
	prInterWorkingIe = (P_IE_INTERWORKING_T) pvSetBuffer;

	prHS20Info->ucAccessNetworkOptions = prInterWorkingIe->ucAccNetOpt;
	prHS20Info->ucVenueGroup = prInterWorkingIe->ucVenueGroup;
	prHS20Info->ucVenueType = prInterWorkingIe->ucVenueType;
	COPY_MAC_ADDR(prHS20Info->aucHESSID, prInterWorkingIe->aucHESSID);

	DBGLOG(SEC, TRACE, "IW IE sz %ld\n", u4SetBufferLen);
#endif
	return WLAN_STATUS_SUCCESS;

}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called by WSC to set the Roaming Consortium IE info, which is needed to
*          add to Association request frame while join WPS AP.
*
* \param[in] prAdapter Pointer to the Adapter structure
* \param[in] pvSetBuffer A pointer to the buffer that holds the data to be set
* \param[in] u4SetBufferLen The length of the set buffer
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*   bytes read from the set buffer. If the call failed due to invalid length of
*   the set buffer, returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_DATA If new setting value is wrong.
* \retval WLAN_STATUS_INVALID_LENGTH
*
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetRoamingConsortiumIEInfo(IN P_ADAPTER_T prAdapter,
				  IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
#if 0
	P_HS20_INFO_T prHS20Info = NULL;
	P_PARAM_HS20_ROAMING_CONSORTIUM_INFO prRCInfo;

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	prHS20Info = &(prAdapter->rWifiVar.rHS20Info);

	/* DEBUGFUNC("wlanoidSetRoamingConsortiumInfo"); */
	/* DBGLOG(HS2, TRACE, ("\r\n")); */

	if (u4SetBufferLen == 0)
		return WLAN_STATUS_INVALID_LENGTH;

	*pu4SetInfoLen = u4SetBufferLen;
	prRCInfo = (P_PARAM_HS20_ROAMING_CONSORTIUM_INFO) pvSetBuffer;

	kalMemCopy(&(prHS20Info->rRCInfo), prRCInfo, sizeof(PARAM_HS20_ROAMING_CONSORTIUM_INFO));

	/* DBGLOG(HS2, TRACE, ("RoamingConsortium IE sz %ld\n", u4SetBufferLen)); */
#endif
	return WLAN_STATUS_SUCCESS;

}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set_bssid_pool
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
wlanoidSetHS20BssidPool(IN P_ADAPTER_T prAdapter,
			IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	if (u4SetBufferLen)
		ASSERT(pvSetBuffer);

	if (u4SetBufferLen < sizeof(PARAM_HS20_SET_BSSID_POOL)) {
		*pu4SetInfoLen = sizeof(PARAM_HS20_SET_BSSID_POOL);
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}

	rWlanStatus = hs20SetBssidPool(prAdapter, pvSetBuffer, KAL_NETWORK_TYPE_AIS_INDEX);

	return rWlanStatus;
}				/* end of wlanoidSendHS20GASRequest() */

#endif /* CFG_SUPPORT_PASSPOINT */

#if CFG_SUPPORT_SNIFFER
WLAN_STATUS
wlanoidSetMonitor(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_PARAM_CUSTOM_MONITOR_SET_STRUCT_T prMonitorSetInfo;
	CMD_MONITOR_SET_INFO_T rCmdMonitorSetInfo;
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidSetMonitor");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(PARAM_CUSTOM_MONITOR_SET_STRUCT_T);

	if (u4SetBufferLen < sizeof(PARAM_CUSTOM_MONITOR_SET_STRUCT_T))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prMonitorSetInfo = (P_PARAM_CUSTOM_MONITOR_SET_STRUCT_T) pvSetBuffer;

	rCmdMonitorSetInfo.ucEnable = prMonitorSetInfo->ucEnable;
	rCmdMonitorSetInfo.ucBand = prMonitorSetInfo->ucBand;
	rCmdMonitorSetInfo.ucPriChannel = prMonitorSetInfo->ucPriChannel;
	rCmdMonitorSetInfo.ucSco = prMonitorSetInfo->ucSco;
	rCmdMonitorSetInfo.ucChannelWidth = prMonitorSetInfo->ucChannelWidth;
	rCmdMonitorSetInfo.ucChannelS1 = prMonitorSetInfo->ucChannelS1;
	rCmdMonitorSetInfo.ucChannelS2 = prMonitorSetInfo->ucChannelS2;

	rWlanStatus = wlanSendSetQueryCmd(prAdapter,
					  CMD_ID_SET_MONITOR,
					  TRUE,
					  FALSE,
					  g_fgIsOid,
					  nicCmdEventSetCommon,
					  nicOidCmdTimeoutCommon,
					  sizeof(CMD_MONITOR_SET_INFO_T),
					  (PUINT_8) &rCmdMonitorSetInfo, pvSetBuffer, u4SetBufferLen);

	return rWlanStatus;
}
#endif

#if CFG_SUPPORT_ADVANCE_CONTROL
WLAN_STATUS
wlanoidAdvCtrl(IN P_ADAPTER_T prAdapter,
	OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	return _wlanoidAdvCtrl(prAdapter,
				pvQueryBuffer,
				u4QueryBufferLen,
				pu4QueryInfoLen,
				g_fgIsOid);
}

WLAN_STATUS
_wlanoidAdvCtrl(IN P_ADAPTER_T prAdapter,
	OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen,
	OUT PUINT_32 pu4QueryInfoLen, IN BOOLEAN fgIsOid)
{
	P_CMD_ADV_CONFIG_HEADER_T cmd;
	UINT_16 type = 0;
	UINT_32 len;
	BOOLEAN fgSetQuery = FALSE;
	BOOLEAN fgNeedResp = TRUE;

	DBGLOG(REQ, LOUD, "%s>\n", __func__);

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in query receive error! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		*pu4QueryInfoLen = sizeof(UINT_32);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (u4QueryBufferLen < sizeof(*cmd)) {
		DBGLOG(REQ, WARN, "Too short length %u\n", u4QueryBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	cmd = (P_CMD_ADV_CONFIG_HEADER_T)pvQueryBuffer;

	if (cmd->u2Type & CMD_ADV_CONTROL_SET) {
		fgSetQuery = TRUE;
		fgNeedResp = FALSE;
	}

	type = cmd->u2Type;
	type &= ~CMD_ADV_CONTROL_SET;
	DBGLOG(RSN, INFO, "%s cmd type %d\n", __func__, cmd->u2Type);
	switch (type) {
	case CMD_PTA_CONFIG_TYPE:
		*pu4QueryInfoLen = sizeof(CMD_PTA_CONFIG_T);
		len = sizeof(CMD_PTA_CONFIG_T);
		break;
	case CMD_GET_REPORT_TYPE:
		*pu4QueryInfoLen = sizeof(struct CMD_GET_TRAFFIC_REPORT);
		len = sizeof(struct CMD_GET_TRAFFIC_REPORT);
		break;
	case CMD_NOISE_HISTOGRAM_TYPE:
		*pu4QueryInfoLen = sizeof(struct CMD_NOISE_HISTOGRAM_REPORT);
		len = sizeof(struct CMD_NOISE_HISTOGRAM_REPORT);
		break;
	default:
		return WLAN_STATUS_INVALID_LENGTH;
	}

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_ADV_CONTROL,
				   fgSetQuery,
				   fgNeedResp,
				   fgIsOid,
				   nicCmdEventQueryAdvCtrl,
				   nicOidCmdTimeoutCommon,
				   len, (PUINT_8)cmd,
				   pvQueryBuffer, u4QueryBufferLen);
}
#endif

#if CFG_SUPPORT_MSP
WLAN_STATUS
wlanoidQueryWlanInfo(IN P_ADAPTER_T prAdapter,
		       IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	return _wlanoidQueryWlanInfo(prAdapter,
				pvQueryBuffer,
				u4QueryBufferLen,
				pu4QueryInfoLen,
				g_fgIsOid);
}

WLAN_STATUS
_wlanoidQueryWlanInfo(IN P_ADAPTER_T prAdapter,
		IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen,
		OUT PUINT_32 pu4QueryInfoLen, IN BOOLEAN fgIsOid)
{
	P_PARAM_HW_WLAN_INFO_T  prHwWlanInfo;

	DEBUGFUNC("wlanoidQueryWlanInfo");
	DBGLOG(REQ, LOUD, "\n");

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	*pu4QueryInfoLen = sizeof(PARAM_HW_WLAN_INFO_T);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in query receive error! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		*pu4QueryInfoLen = sizeof(UINT_32);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (u4QueryBufferLen < sizeof(PARAM_HW_WLAN_INFO_T)) {
		DBGLOG(REQ, WARN, "Too short length %u\n", u4QueryBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	prHwWlanInfo = (P_PARAM_HW_WLAN_INFO_T)pvQueryBuffer;
	DBGLOG(RSN, INFO, "MT6632 : wlanoidQueryWlanInfo index = %d\n", prHwWlanInfo->u4Index);

	/*  *pu4QueryInfoLen = 8 + prRxStatistics->u4TotalNum; */

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_WLAN_INFO,
				   FALSE,
				   TRUE,
				   fgIsOid,
				   nicCmdEventQueryWlanInfo,
				   nicOidCmdTimeoutCommon,
				   sizeof(PARAM_HW_WLAN_INFO_T), (PUINT_8)prHwWlanInfo,
				   pvQueryBuffer, u4QueryBufferLen);

}				/* wlanoidQueryWlanInfo */


WLAN_STATUS
wlanoidQueryMibInfo(IN P_ADAPTER_T prAdapter,
		       IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	P_PARAM_HW_MIB_INFO_T  prHwMibInfo;

	DEBUGFUNC("wlanoidQueryMibInfo");
	DBGLOG(REQ, LOUD, "\n");

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	*pu4QueryInfoLen = sizeof(PARAM_HW_MIB_INFO_T);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in query receive error! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		*pu4QueryInfoLen = sizeof(UINT_32);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (u4QueryBufferLen < sizeof(PARAM_HW_MIB_INFO_T)) {
		DBGLOG(REQ, WARN, "Too short length %u\n", u4QueryBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	prHwMibInfo = (P_PARAM_HW_MIB_INFO_T)pvQueryBuffer;
	DBGLOG(RSN, INFO, "MT6632 : wlanoidQueryMibInfo index = %d\n", prHwMibInfo->u4Index);

	/* *pu4QueryInfoLen = 8 + prRxStatistics->u4TotalNum; */

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_MIB_INFO,
				   FALSE,
				   TRUE,
				   g_fgIsOid,
				   nicCmdEventQueryMibInfo,
				   nicOidCmdTimeoutCommon,
				   sizeof(PARAM_HW_MIB_INFO_T), (PUINT_8)prHwMibInfo,
				   pvQueryBuffer, u4QueryBufferLen);

}				/* wlanoidQueryMibInfo */
#endif

#if CFG_SUPPORT_LAST_SEC_MCS_INFO
WLAN_STATUS
wlanoidTxMcsInfo(IN P_ADAPTER_T prAdapter,
		       IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	struct PARAM_TX_MCS_INFO *prMcsInfo;

	DEBUGFUNC("wlanoidQueryWlanInfo");
	DBGLOG(REQ, LOUD, "\n");

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	*pu4QueryInfoLen = sizeof(struct PARAM_TX_MCS_INFO);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in query receive error! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		*pu4QueryInfoLen = sizeof(UINT_32);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (u4QueryBufferLen < sizeof(struct PARAM_TX_MCS_INFO)) {
		DBGLOG(REQ, WARN, "Too short length %u\n", u4QueryBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	prMcsInfo = (struct PARAM_TX_MCS_INFO *)pvQueryBuffer;

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_TX_MCS_INFO,
				   FALSE,
				   TRUE,
				   TRUE,
				   nicCmdEventTxMcsInfo,
				   nicOidCmdTimeoutCommon,
				   sizeof(struct PARAM_TX_MCS_INFO), (PUINT_8)prMcsInfo,
				   pvQueryBuffer, u4QueryBufferLen);

}
#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set FW log to Host.
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
* \retval WLAN_STATUS_NOT_SUPPORTED
* \retval WLAN_STATUS_ADAPTER_NOT_READY
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetFwLog2Host(
	IN P_ADAPTER_T prAdapter,
	IN PVOID pvSetBuffer,
	IN UINT_32 u4SetBufferLen,
	OUT PUINT_32 pu4SetInfoLen)
{
	P_CMD_FW_LOG_2_HOST_CTRL_T prFwLog2HostCtrl;
	UINT_64 ts = KAL_GET_HOST_CLOCK();

	DEBUGFUNC("wlanoidSetFwLog2Host");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(CMD_FW_LOG_2_HOST_CTRL_T);

	if (u4SetBufferLen)
		ASSERT(pvSetBuffer);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in set FW log to Host! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (u4SetBufferLen < sizeof(CMD_FW_LOG_2_HOST_CTRL_T)) {
		DBGLOG(REQ, WARN, "Too short length %u\n", u4SetBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	prFwLog2HostCtrl = (P_CMD_FW_LOG_2_HOST_CTRL_T)pvSetBuffer;
	prFwLog2HostCtrl->u4HostTimeMSec = (UINT_32)(do_div(ts, 1000000000) / 1000);
	prFwLog2HostCtrl->u4HostTimeSec = (UINT_32)ts;

#if CFG_SUPPORT_FW_DBG_LEVEL_CTRL
	DBGLOG(REQ, INFO, "McuDest %d, LogType %d, (FwLogLevel %d)\n", prFwLog2HostCtrl->ucMcuDest,
		prFwLog2HostCtrl->ucFwLog2HostCtrl, prFwLog2HostCtrl->ucFwLogLevel);
#else
	DBGLOG(REQ, INFO, "McuDest %d, LogType %d\n", prFwLog2HostCtrl->ucMcuDest,
		prFwLog2HostCtrl->ucFwLog2HostCtrl);
#endif

	return wlanSendSetQueryCmd(prAdapter,
				CMD_ID_FW_LOG_2_HOST,
				TRUE,
				FALSE,
				g_fgIsOid,
				nicCmdEventSetCommon,
				nicOidCmdTimeoutCommon,
				sizeof(CMD_FW_LOG_2_HOST_CTRL_T),
				(PUINT_8)prFwLog2HostCtrl, pvSetBuffer, u4SetBufferLen);
}

WLAN_STATUS
wlanoidNotifyFwSuspend(
		IN P_ADAPTER_T prAdapter,
		IN PVOID pvSetBuffer,
		IN UINT_32 u4SetBufferLen,
		OUT PUINT_32 pu4SetInfoLen)
{
	P_CMD_SUSPEND_MODE_SETTING_T prSuspendCmd;

	if (!prAdapter || !pvSetBuffer)
		return WLAN_STATUS_INVALID_DATA;

	prSuspendCmd = (P_CMD_SUSPEND_MODE_SETTING_T)pvSetBuffer;

	return wlanSendSetQueryCmd(prAdapter,
					CMD_ID_SET_SUSPEND_MODE,
					TRUE,
					FALSE,
					g_fgIsOid,
					nicCmdEventSetCommon,
					nicOidCmdTimeoutCommon,
					sizeof(CMD_SUSPEND_MODE_SETTING_T),
					(PUINT_8)prSuspendCmd,
					NULL,
					0);
}
#if CFG_SUPPORT_DBDC
WLAN_STATUS
wlanoidSetDbdcEnable(
		IN P_ADAPTER_T prAdapter,
		IN PVOID pvSetBuffer,
		IN UINT_32 u4SetBufferLen,
		OUT PUINT_32 pu4SetInfoLen)
{
	UINT_8 ucDBDCEnable;

	if (!prAdapter || !pvSetBuffer)
		return WLAN_STATUS_INVALID_DATA;

	kalMemCopy(&ucDBDCEnable, pvSetBuffer, 1);
	cnmUpdateDbdcSetting(prAdapter, ucDBDCEnable);

	return WLAN_STATUS_SUCCESS;
}
#endif /*#if CFG_SUPPORT_DBDC*/


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set tx target power base.
*
* \param[in] pvAdapter Pointer to the Adapter structure.
* \param[out] pvQueryBuf A pointer to the buffer that holds the result of
*                           the query.
* \param[in] u4QueryBufLen The length of the query buffer.
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                            bytes written into the query buffer. If the call
*                            failed due to invalid length of the query buffer,
*                            returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQuerySetTxTargetPower(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen,
			 OUT PUINT_32 pu4SetInfoLen)
{
	P_PARAM_CUSTOM_SET_TX_TARGET_POWER_T prSetTxTargetPowerInfo;
	CMD_SET_TX_TARGET_POWER_T rCmdSetTxTargetPower;
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidQuerySetTxTargetPower");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(P_PARAM_CUSTOM_SET_TX_TARGET_POWER_T);

	if (u4SetBufferLen < sizeof(P_PARAM_CUSTOM_SET_TX_TARGET_POWER_T))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prSetTxTargetPowerInfo = (P_PARAM_CUSTOM_SET_TX_TARGET_POWER_T) pvSetBuffer;

	kalMemSet(&rCmdSetTxTargetPower, 0, sizeof(CMD_SET_TX_TARGET_POWER_T));

	rCmdSetTxTargetPower.ucTxTargetPwr = prSetTxTargetPowerInfo->ucTxTargetPwr;

	DBGLOG(INIT, INFO, "MT6632 : wlanoidQuerySetTxTargetPower =%x dbm\n", rCmdSetTxTargetPower.ucTxTargetPwr);

	rWlanStatus = wlanSendSetQueryCmd(prAdapter,
					CMD_ID_SET_TX_PWR,
					TRUE,   /* fgSetQuery Bit:  True->write  False->read*/
					FALSE,   /* fgNeedResp */
					g_fgIsOid,   /* fgIsOid*/
					nicCmdEventSetCommon, /* REF: wlanoidSetDbdcEnable */
					nicOidCmdTimeoutCommon,
					sizeof(CMD_ACCESS_EFUSE_T),
					(PUINT_8) (&rCmdSetTxTargetPower), pvSetBuffer, u4SetBufferLen);

	return rWlanStatus;
}

#if (CFG_SUPPORT_DFS_MASTER == 1)
/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set rdd report.
*
* \param[in] pvAdapter Pointer to the Adapter structure.
* \param[out] pvQueryBuf A pointer to the buffer that holds the result of
*                           the query.
* \param[in] u4QueryBufLen The length of the query buffer.
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                            bytes written into the query buffer. If the call
*                            failed due to invalid length of the query buffer,
*                            returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQuerySetRddReport(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen,
			 OUT PUINT_32 pu4SetInfoLen)
{
	P_PARAM_CUSTOM_SET_RDD_REPORT_T prSetRddReport;
	P_CMD_RDD_ON_OFF_CTRL_T prCmdRddOnOffCtrl;
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidQuerySetRddReport");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(P_PARAM_CUSTOM_SET_RDD_REPORT_T);

	ASSERT(pvSetBuffer);

	prSetRddReport = (P_PARAM_CUSTOM_SET_RDD_REPORT_T) pvSetBuffer;

	prCmdRddOnOffCtrl = (P_CMD_RDD_ON_OFF_CTRL_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
					sizeof(*prCmdRddOnOffCtrl));

	ASSERT(prCmdRddOnOffCtrl);
	if (prCmdRddOnOffCtrl == NULL) {
		DBGLOG(INIT, ERROR, "prCmdRddOnOffCtrl is NULL");
		return WLAN_STATUS_FAILURE;
	}

	prCmdRddOnOffCtrl->ucDfsCtrl = RDD_RADAR_EMULATE;

	prCmdRddOnOffCtrl->ucRddIdx = prSetRddReport->ucDbdcIdx;

	if (prCmdRddOnOffCtrl->ucRddIdx)
		prCmdRddOnOffCtrl->ucRddInSel = RDD_IN_SEL_1;
	else
		prCmdRddOnOffCtrl->ucRddInSel = RDD_IN_SEL_0;

	DBGLOG(INIT, INFO, "MT6632 : wlanoidQuerySetRddReport -  DFS ctrl: %.d, RDD index: %d\n",
	prCmdRddOnOffCtrl->ucDfsCtrl, prCmdRddOnOffCtrl->ucRddIdx);

	rWlanStatus = wlanSendSetQueryCmd(prAdapter,
					CMD_ID_RDD_ON_OFF_CTRL,
					TRUE,   /* fgSetQuery Bit:  True->write  False->read*/
					FALSE,   /* fgNeedResp */
					g_fgIsOid,   /* fgIsOid*/
					nicCmdEventSetCommon, /* REF: wlanoidSetDbdcEnable */
					nicOidCmdTimeoutCommon,
					sizeof(*prCmdRddOnOffCtrl),
					(PUINT_8) (prCmdRddOnOffCtrl), pvSetBuffer, u4SetBufferLen);

	cnmMemFree(prAdapter, prCmdRddOnOffCtrl);

	return rWlanStatus;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set rdd report.
*
* \param[in] pvAdapter Pointer to the Adapter structure.
* \param[out] pvQueryBuf A pointer to the buffer that holds the result of
*                           the query.
* \param[in] u4QueryBufLen The length of the query buffer.
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                            bytes written into the query buffer. If the call
*                            failed due to invalid length of the query buffer,
*                            returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQuerySetRadarDetectMode(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen,
			 OUT PUINT_32 pu4SetInfoLen)
{
	struct PARAM_CUSTOM_SET_RADAR_DETECT_MODE *prSetRadarDetectMode;
	P_CMD_RDD_ON_OFF_CTRL_T prCmdRddOnOffCtrl;
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidQuerySetRadarDetectMode");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(struct PARAM_CUSTOM_SET_RADAR_DETECT_MODE *);

	ASSERT(pvSetBuffer);

	prSetRadarDetectMode = (struct PARAM_CUSTOM_SET_RADAR_DETECT_MODE *) pvSetBuffer;

	prCmdRddOnOffCtrl = (P_CMD_RDD_ON_OFF_CTRL_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
					sizeof(*prCmdRddOnOffCtrl));

	ASSERT(prCmdRddOnOffCtrl);
	if (prCmdRddOnOffCtrl == NULL) {
		DBGLOG(INIT, ERROR, "prCmdRddOnOffCtrl is NULL");
		return WLAN_STATUS_FAILURE;
	}

	prCmdRddOnOffCtrl->ucDfsCtrl = RDD_DET_MODE;

	prCmdRddOnOffCtrl->ucRadarDetectMode = prSetRadarDetectMode->ucRadarDetectMode;

	DBGLOG(INIT, INFO, "MT6632 : wlanoidQuerySetRadarDetectMode -  DFS ctrl: %.d, Radar Detect Mode: %d\n",
	prCmdRddOnOffCtrl->ucDfsCtrl, prCmdRddOnOffCtrl->ucRadarDetectMode);

	rWlanStatus = wlanSendSetQueryCmd(prAdapter,
					CMD_ID_RDD_ON_OFF_CTRL,
					TRUE,   /* fgSetQuery Bit:  True->write  False->read*/
					FALSE,   /* fgNeedResp */
					g_fgIsOid,   /* fgIsOid*/
					nicCmdEventSetCommon, /* REF: wlanoidSetDbdcEnable */
					nicOidCmdTimeoutCommon,
					sizeof(*prCmdRddOnOffCtrl),
					(PUINT_8) (prCmdRddOnOffCtrl), pvSetBuffer, u4SetBufferLen);

	cnmMemFree(prAdapter, prCmdRddOnOffCtrl);

	return rWlanStatus;
}
#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to turn radio off.
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
* \retval WLAN_STATUS_ADAPTER_NOT_READY
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidLinkDown(IN P_ADAPTER_T prAdapter,
		       IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	DEBUGFUNC("wlanoidSetDisassociate");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = 0;

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in set link down! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

		aisBssLinkDown(prAdapter);

	return WLAN_STATUS_SUCCESS;
}				/* wlanoidSetDisassociate */

#if CFG_SUPPORT_CSI
WLAN_STATUS
wlanoidSetCSIControl(
	IN P_ADAPTER_T prAdapter,
	IN PVOID pvSetBuffer,
	IN UINT_32 u4SetBufferLen,
	OUT PUINT_32 pu4SetInfoLen)
{
	struct CMD_CSI_CONTROL_T *pCSICtrl;

	DEBUGFUNC("wlanoidSetCSIControl");

	*pu4SetInfoLen = sizeof(struct CMD_CSI_CONTROL_T);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
			   "Fail in set CSI control! (Adapter not ready). ACPI=D%d, Radio=%d\n",
			   prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (u4SetBufferLen < sizeof(struct CMD_CSI_CONTROL_T)) {
		DBGLOG(REQ, WARN, "Too short length %u\n", u4SetBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	pCSICtrl = (struct CMD_CSI_CONTROL_T *)pvSetBuffer;

	return wlanSendSetQueryCmd(prAdapter,
				CMD_ID_CSI_CONTROL,
				TRUE,
				FALSE,
				TRUE,
				nicCmdEventSetCommon,
				nicOidCmdTimeoutCommon,
				sizeof(struct CMD_CSI_CONTROL_T),
				(PUINT_8)pCSICtrl, pvSetBuffer, u4SetBufferLen);
}
#endif

WLAN_STATUS
wlanoidGetTxPwrTbl(IN P_ADAPTER_T prAdapter,
		   IN PVOID pvQueryBuffer,
		   IN UINT_32 u4QueryBufferLen,
		   OUT PUINT_32 pu4QueryInfoLen)
{
	struct CMD_GET_TXPWR_TBL CmdPwrTbl;
	struct PARAM_CMD_GET_TXPWR_TBL *prPwrTbl = NULL;

	DEBUGFUNC("wlanoidGetTxPwrTbl");
	DBGLOG(REQ, LOUD, "\n");

	if (!prAdapter || (!pvQueryBuffer && u4QueryBufferLen) ||
	    !pu4QueryInfoLen)
		return WLAN_STATUS_INVALID_DATA;

	*pu4QueryInfoLen = sizeof(struct PARAM_CMD_GET_TXPWR_TBL);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in query receive error! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		*pu4QueryInfoLen = sizeof(UINT_32);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (u4QueryBufferLen < sizeof(struct PARAM_CMD_GET_TXPWR_TBL)) {
		DBGLOG(REQ, WARN, "Too short length %u\n", u4QueryBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	prPwrTbl = (struct PARAM_CMD_GET_TXPWR_TBL *)pvQueryBuffer;
	CmdPwrTbl.ucDbdcIdx = prPwrTbl->ucDbdcIdx;

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_GET_TXPWR_TBL,
				   FALSE,
				   TRUE,
				   g_fgIsOid,
				   nicCmdEventGetTxPwrTbl,
				   nicOidCmdTimeoutCommon,
				   sizeof(struct CMD_GET_TXPWR_TBL),
				   (PUINT_8)&CmdPwrTbl,
				   pvQueryBuffer,
				   u4QueryBufferLen);

}

WLAN_STATUS
wlanoidEnableRoaming(IN P_ADAPTER_T prAdapter,
		   IN PVOID pvSetBuffer,
		   IN UINT_32 u4SetBufferLen,
		   OUT PUINT_32 pu4SetInfoLen)
{
	DBGLOG(OID, INFO, "enable roaming\n");

	aisRemoveBlacklistBySource(prAdapter, AIS_BLACK_LIST_FROM_FWK);

	return WLAN_STATUS_SUCCESS;
}

WLAN_STATUS
wlanoidConfigRoaming(IN P_ADAPTER_T prAdapter,
		   IN PVOID pvSetBuffer,
		   IN UINT_32 u4SetBufferLen,
		   OUT PUINT_32 pu4SetInfoLen)
{
	struct nlattr *attrlist;
	struct AIS_BLACKLIST_ITEM *prBlackList;
	UINT_32 len_shift = 0;
	UINT_32 numOfList[2] = { 0 };
	int i;

	attrlist = (struct nlattr *)pvSetBuffer;

	/* get the number of blacklist and copy those mac addresses from HAL */
	if (attrlist->nla_type == WIFI_ATTRIBUTE_ROAMING_BLACKLIST_NUM) {
		numOfList[0] = nla_get_u32(attrlist);
		len_shift += NLA_ALIGN(attrlist->nla_len);
	}
	DBGLOG(REQ, INFO, "Get the number of blacklist=%d\n", numOfList[0]);

	if (numOfList[0] >= 0 && numOfList[0]
		<= MAX_FW_ROAMING_BLACKLIST_SIZE) {
		/*Refresh all the FWKBlacklist */
		aisRemoveBlacklistBySource(prAdapter, AIS_BLACK_LIST_FROM_FWK);

		/* Start to receive blacklist mac addresses
		* and set to FWK blacklist
		*/
		attrlist = (struct nlattr *)((UINT_8 *) pvSetBuffer
			+ len_shift);
		for (i = 0; i < numOfList[0]; i++) {
			if (attrlist->nla_type
				== WIFI_ATTRIBUTE_ROAMING_BLACKLIST_BSSID)
				prBlackList =
				aisAddBlacklistByBssid(prAdapter,
						nla_data(attrlist),
						AIS_BLACK_LIST_FROM_FWK);
			len_shift += NLA_ALIGN(attrlist->nla_len);
			attrlist = (struct nlattr *)((UINT_8 *) pvSetBuffer
				+ len_shift);
		}
	}

	return WLAN_STATUS_SUCCESS;
}
