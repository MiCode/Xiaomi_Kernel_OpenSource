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

#ifdef FW_CFG_SUPPORT
#include "fwcfg.h"
#endif
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
/* tx power scenario enum:
 * declared in /hardware/libhardware_legacy/include/hardware_legacy/wifi_hal.h
 * enum ENUM_TX_POWER_SCENARIO {
 *	WIFI_POWER_SCENARIO_VOICE_CALL       = 0,
 *	WIFI_POWER_SCENARIO_ON_HEAD_CELL_OFF = 1,
 *	WIFI_POWER_SCENARIO_ON_HEAD_CELL_ON  = 2,
 *	WIFI_POWER_SCENARIO_ON_BODY_CELL_OFF = 3,
 *	WIFI_POWER_SCENARIO_ON_BODY_CELL_ON  = 4,
 * };
 */
static struct TX_POWER_SCENARIO_ENTRY g_txPowerScenario[] = {
	{ TRUE, 12, TRUE, 12 }, /* WIFI_POWER_SCENARIO_VOICE_CALL */
	{ TRUE, 12, TRUE, 12 }, /* WIFI_POWER_SCENARIO_ON_HEAD_CELL_OFF */
	{ TRUE, 12, TRUE, 12 }, /* WIFI_POWER_SCENARIO_ON_HEAD_CELL_ON */
	{ TRUE, 12, TRUE, 12 }, /* WIFI_POWER_SCENARIO_ON_BODY_CELL_OFF */
	{ TRUE, 12, TRUE, 12 }, /* WIFI_POWER_SCENARIO_ON_BODY_CELL_ON */
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

	DBGLOG(OID, TRACE, "NDIS supported network type list: %u\n", prSupported->NumberOfItems);
	DBGLOG_MEM8(OID, TRACE, prSupported, *pu4QueryInfoLen);

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

	DBGLOG(OID, TRACE, "Network type in use: %d\n", rCurrentNetworkTypeInUse);

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

	DBGLOG(OID, INFO, "New network type: %d mode\n", eNewNetworkType);

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
		DBGLOG(OID, INFO, "Not support network type: %d\n", eNewNetworkType);
		rStatus = WLAN_STATUS_NOT_SUPPORTED;
		break;

	default:
		DBGLOG(OID, INFO, "Unknown network type: %d\n", eNewNetworkType);
		rStatus = WLAN_STATUS_INVALID_DATA;
		break;
	}

	/* Verify if we support the new network type. */
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(OID, WARN, "Unknown network type: %d\n", eNewNetworkType);

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
		DBGLOG(OID, WARN, "Fail in qeury BSSID list! (Adapter not ready). ACPI=D%d, Radio=%d\n",
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
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidSetBssidListScan()");

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(OID, WARN, "Fail in set BSSID list scan! (Adapter not ready). ACPI=D%d, Radio=%d\n",
				   prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	ASSERT(pu4SetInfoLen);
	*pu4SetInfoLen = 0;

	if (prAdapter->fgIsRadioOff) {
		DBGLOG(OID, WARN, "Return from BSSID list scan! (radio off). ACPI=D%d, Radio=%d\n",
				   prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_SUCCESS;
	}

	DBGLOG(OID, TRACE, "Scan\n");

	if (pvSetBuffer != NULL && u4SetBufferLen != 0) {
		COPY_SSID(rSsid.aucSsid, rSsid.u4SsidLen, pvSetBuffer, u4SetBufferLen);
		prSsid = &rSsid;
	} else {
		prSsid = NULL;
	}

#if CFG_SUPPORT_RDD_TEST_MODE
	if (prAdapter->prGlueInfo->rRegInfo.u4RddTestMode) {
		if ((prAdapter->fgEnOnlineScan == TRUE) && (prAdapter->ucRddStatus)) {
			if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) != PARAM_MEDIA_STATE_CONNECTED) {
				aisFsmScanRequest(prAdapter, prSsid, NULL, 0);
			} else {
				/* reject the scan request */
				rStatus = WLAN_STATUS_FAILURE;
			}
		} else {
			/* reject the scan request */
			rStatus = WLAN_STATUS_FAILURE;
		}
	} else
#endif
	{
		if (prAdapter->fgEnOnlineScan == TRUE) {
			aisFsmScanRequest(prAdapter, prSsid, NULL, 0);
		} else if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) != PARAM_MEDIA_STATE_CONNECTED) {
			aisFsmScanRequest(prAdapter, prSsid, NULL, 0);
		} else {
			/* reject the scan request */
			rStatus = WLAN_STATUS_FAILURE;
		}
	}

	return rStatus;
}				/* wlanoidSetBssidListScan */

/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
BOOLEAN wlanoidGetChannelInfo(IN P_ADAPTER_T prAdapter, IN PUINT_8 puPartialScanReq)
{
	struct cfg80211_scan_request *scan_req_t = NULL;
	struct ieee80211_channel *channel_tmp = NULL;
	P_PARTIAL_SCAN_INFO	PartialScanChannel = NULL;
	int i = 0;
	int j = 0;
	UINT_8 channel_num = 0;
	UINT_8 channel_counts = 0;

	if ((prAdapter == NULL) || (prAdapter->prGlueInfo == NULL) ||
		(puPartialScanReq == NULL))
		return FALSE;

	scan_req_t = (struct cfg80211_scan_request *)puPartialScanReq;
	if (scan_req_t->n_channels != 0) {

		channel_counts = scan_req_t->n_channels;
		DBGLOG(OID, TRACE, "scan channel number: n_channels=%d\n", channel_counts);
		if (channel_counts > MAXIMUM_OPERATION_CHANNEL_LIST)
			return TRUE;
		/*
		 * if (scan_req_t->n_channels > MAXIMUM_OPERATION_CHANNEL_LIST) {
		 * DBGLOG(REQ, TRACE, "request channel great max num, reset channel num\n");
		 * }
		 */
		PartialScanChannel = (P_PARTIAL_SCAN_INFO) kalMemAlloc(sizeof(PARTIAL_SCAN_INFO), VIR_MEM_TYPE);
		if (PartialScanChannel == NULL) {
			DBGLOG(OID, ERROR, "alloc PartialScanChannel fail\n");
			return FALSE;
		}
		kalMemSet(PartialScanChannel, 0, sizeof(PARTIAL_SCAN_INFO));
		while (j < channel_counts) {
			channel_tmp = scan_req_t->channels[j];

			DBGLOG(OID, TRACE, "set channel band=%d\n", channel_tmp->band);
			if (channel_tmp->band >= IEEE80211_BAND_60GHZ) {
				j++;
				continue;
			}

			if (i >= MAXIMUM_OPERATION_CHANNEL_LIST)
				break;
			if (channel_tmp->band == IEEE80211_BAND_2GHZ)
				PartialScanChannel->arChnlInfoList[i].eBand = BAND_2G4;
			else if (channel_tmp->band == IEEE80211_BAND_5GHZ)
				PartialScanChannel->arChnlInfoList[i].eBand = BAND_5G;

			DBGLOG(OID, TRACE, "set channel channel_center_freq =%d\n",
				channel_tmp->center_freq);

			channel_num = (UINT_8)nicFreq2ChannelNum(
				channel_tmp->center_freq * 1000);

			DBGLOG(OID, TRACE, "set channel channel_num=%d\n",
				channel_num);
			PartialScanChannel->arChnlInfoList[i].ucChannelNum = channel_num;

			j++;
			i++;
		}
	}
	DBGLOG(OID, INFO, "Partial Scan: set channel i=%d\n", i);
	if (i > 0) {
		PartialScanChannel->ucChannelListNum = i;
		/*ScanReqMsg->eScanChannel = SCAN_CHANNEL_SPECIFIED;*/
		prAdapter->prGlueInfo->puScanChannel = (PUINT_8)PartialScanChannel;
		return TRUE;
	}

	kalMemFree(PartialScanChannel, VIR_MEM_TYPE, sizeof(PARTIAL_SCAN_INFO));
	return FALSE;

}

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
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_PARAM_SSID_T prSsid;
	PUINT_8 pucIe;
	UINT_32 u4IeLength;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_8 ucScanTime = AIS_SCN_DONE_TIMEOUT_SEC;
	BOOLEAN	partial_result = FALSE;

	DEBUGFUNC("wlanoidSetBssidListScanExt()");

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(OID, ERROR, "Fail in set BSSID list scan! (Adapter not ready). ACPI=D%d, Radio=%d\n",
				   prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (prAdapter->fgTestMode) {
		DBGLOG(OID, WARN, "didn't support Scan in test mode\n");
		return WLAN_STATUS_FAILURE;
	}

	ASSERT(pu4SetInfoLen);
	*pu4SetInfoLen = 0;

	if (u4SetBufferLen != sizeof(PARAM_SCAN_REQUEST_EXT_T)) {
		DBGLOG(OID, ERROR, "u4SetBufferLen != sizeof(PARAM_SCAN_REQUEST_EXT_T)\n");
		return WLAN_STATUS_INVALID_LENGTH;
	}

	if (prAdapter->fgIsRadioOff) {
		DBGLOG(OID, INFO, "Return from BSSID list scan! (radio off). ACPI=D%d, Radio=%d\n",
				   prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_SUCCESS;
	}

	DBGLOG(OID, TRACE, "ScanEx\n");

	/* clear old scan backup results if exists */
	{
		P_SCAN_INFO_T prScanInfo;
		P_LINK_T prBSSDescList;
		P_BSS_DESC_T prBssDesc;

		prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
		prBSSDescList = &prScanInfo->rBSSDescList;
		LINK_FOR_EACH_ENTRY(prBssDesc, prBSSDescList, rLinkEntry, BSS_DESC_T) {
			if (prBssDesc->eBSSType == BSS_TYPE_INFRASTRUCTURE) {
				kalMemZero(prBssDesc->aucRawBuf, CFG_RAW_BUFFER_SIZE);
				prBssDesc->u2RawLength = 0;
			}
		}
	}

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

/* P_AIS_FSM_INFO_T prAisFsmInfo; */
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

/* #if CFG_SUPPORT_WFD */
#if 0
	if ((prAdapter->rWifiVar.prP2pFsmInfo->rWfdConfigureSettings.ucWfdEnable) &&
	    ((prAdapter->rWifiVar.prP2pFsmInfo->rWfdConfigureSettings.u4WfdFlag & WFD_FLAGS_DEV_INFO_VALID))) {

		if (prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX].eConnectionState ==
		    PARAM_MEDIA_STATE_CONNECTED) {
			DBGLOG(OID, TRACE, "Twice the Scan Time for WFD\n");
			ucScanTime *= 2;
		}
	}
#endif /* CFG_SUPPORT_WFD */
	cnmTimerStartTimer(prAdapter, &prAisFsmInfo->rScanDoneTimer, SEC_TO_MSEC(ucScanTime));

#if CFG_SUPPORT_RDD_TEST_MODE
	if (prAdapter->prGlueInfo->rRegInfo.u4RddTestMode) {
		if ((prAdapter->fgEnOnlineScan == TRUE) && (prAdapter->ucRddStatus)) {
			if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) != PARAM_MEDIA_STATE_CONNECTED) {
				partial_result = wlanoidGetChannelInfo(prAdapter, prScanRequest->puPartialScanReq);
				if (partial_result == FALSE)
					return WLAN_STATUS_FAILURE;
				aisFsmScanRequest(prAdapter, prSsid, pucIe, u4IeLength);
			} else {
				/* reject the scan request */
				cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rScanDoneTimer);
				rStatus = WLAN_STATUS_FAILURE;
			}
		} else {
			/* reject the scan request */
			cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rScanDoneTimer);
			rStatus = WLAN_STATUS_FAILURE;
		}
	} else
#endif
	{
		if (prAdapter->fgEnOnlineScan == TRUE) {
			if (prScanRequest == NULL)
				return WLAN_STATUS_FAILURE;
			partial_result = wlanoidGetChannelInfo(prAdapter, prScanRequest->puPartialScanReq);
			if (partial_result == FALSE)
				return WLAN_STATUS_FAILURE;
			aisFsmScanRequest(prAdapter, prSsid, pucIe, u4IeLength);
		} else if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) != PARAM_MEDIA_STATE_CONNECTED) {
			if (prScanRequest == NULL)
				return WLAN_STATUS_FAILURE;
			partial_result = wlanoidGetChannelInfo(prAdapter, prScanRequest->puPartialScanReq);
			if (partial_result == FALSE)
				return WLAN_STATUS_FAILURE;
			aisFsmScanRequest(prAdapter, prSsid, pucIe, u4IeLength);
		} else {
			/* reject the scan request */
			cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rScanDoneTimer);
			rStatus = WLAN_STATUS_FAILURE;
			DBGLOG(OID, WARN, "ScanEx fail %d!\n", prAdapter->fgEnOnlineScan);
		}
	}

	return rStatus;
}				/* wlanoidSetBssidListScanWithIE */

#if CFG_MULTI_SSID_SCAN
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
WLAN_STATUS wlanoidSetBssidListScanAdv(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer,
			IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_PARAM_SCAN_REQUEST_ADV_T prScanRequest;
	PARAM_SSID_T rSsid[CFG_SCAN_SSID_MAX_NUM];
	PUINT_8 pucIe;
	UINT_8 ucSsidNum;
	UINT_32 i, u4IeLength;
	BOOLEAN	partial_result = FALSE;
	P_AIS_FSM_INFO_T prAisFsmInfo;

	DEBUGFUNC("wlanoidSetBssidListScanAdv()");

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN, "Fail in set BSSID list scan! (Adapter not ready). ACPI=D%d, Radio=%d\n",
			prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (prAdapter->fgTestMode) {
		DBGLOG(OID, WARN, "didn't support Scan in test mode\n");
		return WLAN_STATUS_FAILURE;
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

	prScanRequest = (P_PARAM_SCAN_REQUEST_ADV_T)pvSetBuffer;
	/* P_AIS_FSM_INFO_T prAisFsmInfo; */
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	ucSsidNum = (UINT_8)(prScanRequest->u4SsidNum);
	for (i = 0; i < prScanRequest->u4SsidNum; i++) {
		COPY_SSID(rSsid[i].aucSsid, rSsid[i].u4SsidLen, prScanRequest->rSsid[i].aucSsid,
			prScanRequest->rSsid[i].u4SsidLen);
	}

	pucIe = prScanRequest->pucIE;
	u4IeLength = prScanRequest->u4IELength;

#if CFG_SUPPORT_RDD_TEST_MODE
	if (prAdapter->prGlueInfo->rRegInfo.u4RddTestMode) {
		if ((prAdapter->fgEnOnlineScan == TRUE) && (prAdapter->ucRddStatus)) {
			if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) != PARAM_MEDIA_STATE_CONNECTED) {
				partial_result = wlanoidGetChannelInfo(prAdapter, prScanRequest->puPartialScanReq);
				if (partial_result == FALSE)
					return WLAN_STATUS_FAILURE;
				aisFsmScanRequestAdv(prAdapter, ucSsidNum, rSsid, pucIe, u4IeLength,
					prScanRequest->aucRandomMac);
			} else
				return WLAN_STATUS_FAILURE;
		} else {
			return WLAN_STATUS_FAILURE;
		}
	} else
#endif
	{
		if (prAdapter->fgEnOnlineScan == TRUE) {
			partial_result = wlanoidGetChannelInfo(prAdapter, prScanRequest->puPartialScanReq);
			if (partial_result == FALSE)
				return WLAN_STATUS_FAILURE;
			aisFsmScanRequestAdv(prAdapter, ucSsidNum, rSsid, pucIe, u4IeLength,
				prScanRequest->aucRandomMac);
		} else if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) != PARAM_MEDIA_STATE_CONNECTED) {
			partial_result = wlanoidGetChannelInfo(prAdapter, prScanRequest->puPartialScanReq);
			if (partial_result == FALSE)
				return WLAN_STATUS_FAILURE;
			aisFsmScanRequestAdv(prAdapter, ucSsidNum, rSsid, pucIe, u4IeLength,
				prScanRequest->aucRandomMac);
		} else
			return WLAN_STATUS_FAILURE;
	}
	cnmTimerStartTimer(prAdapter, &prAisFsmInfo->rScanDoneTimer
		, SEC_TO_MSEC(AIS_SCN_DONE_TIMEOUT_SEC));

	return WLAN_STATUS_SUCCESS;
} /* wlanoidSetBssidListScanAdv */
#endif

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
		DBGLOG(OID, WARN, "Fail in set ssid! (Adapter not ready). ACPI=D%d, Radio=%d\n",
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
			DBGLOG(OID, TRACE, "DisByBssid\n");
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
		ASSERT(0);
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

	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prAisAbortMsg, MSG_SEND_METHOD_BUF);

	DBGLOG(OID, INFO, "SetBssid\n");
	return WLAN_STATUS_SUCCESS;
}				/* end of wlanoidSetBssid() */

WLAN_STATUS
wlanoidSetConnect(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_GLUE_INFO_T prGlueInfo;
	P_PARAM_CONNECT_T pParamConn;
	P_CONNECTION_SETTINGS_T prConnSettings;
	UINT_32 i;
	/*INT_32 i4Idx = -1, i4MaxRSSI = INT_MIN;*/
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
		DBGLOG(OID, WARN, "Fail in set ssid! (Adapter not ready). ACPI=D%d, Radio=%d\n",
				   prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}
	prAisAbortMsg = (P_MSG_AIS_ABORT_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_AIS_ABORT_T));
	if (!prAisAbortMsg) {
		ASSERT(0);
		return WLAN_STATUS_FAILURE;
	}
	prAisAbortMsg->rMsgHdr.eMsgId = MID_OID_AIS_FSM_JOIN_REQ;

	pParamConn = (P_PARAM_CONNECT_T) pvSetBuffer;
	prConnSettings = &prAdapter->rWifiVar.rConnSettings;

	if (pParamConn->u4SsidLen > 32) {
		cnmMemFree(prAdapter, prAisAbortMsg);
		return WLAN_STATUS_INVALID_LENGTH;
	} else if (!pParamConn->pucBssid && !pParamConn->pucSsid) {
		cnmMemFree(prAdapter, prAisAbortMsg);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	prGlueInfo = prAdapter->prGlueInfo;
	kalMemZero(prConnSettings->aucSSID, sizeof(prConnSettings->aucSSID));
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
			DBGLOG(OID, TRACE, "wrong bssid %pM to connect\n", pParamConn->pucBssid);
	} else
		DBGLOG(OID, TRACE, "No Bssid set\n");
	prConnSettings->u4FreqInKHz = pParamConn->u4CenterFreq;

	/* prepare for CMD_BUILD_CONNECTION & CMD_GET_CONNECTION_STATUS */
	/* re-association check */
	if (kalGetMediaStateIndicated(prGlueInfo) == PARAM_MEDIA_STATE_CONNECTED) {
		if (fgEqualSsid) {
			prAisAbortMsg->ucReasonOfDisconnect = DISCONNECT_REASON_CODE_ROAMING;
			if (fgEqualBssid) {
				kalSetMediaStateIndicated(prGlueInfo, PARAM_MEDIA_STATE_TO_BE_INDICATED);
				prAisAbortMsg->ucReasonOfDisconnect = DISCONNECT_REASON_CODE_REASSOCIATION;
			}
		} else {
			DBGLOG(OID, TRACE, "DisBySsid\n");
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

			for (i = 0; i < ELEM_MAX_LEN_SSID; i++) {
				if (pParamConn->pucSsid) {
					if (!((pParamConn->pucSsid[i] > 0) && (pParamConn->pucSsid[i] <= 0x1F))) {
						fgIsValidSsid = TRUE;
						break;
					}
				}
			}
		}
	}

	/* Set Connection Request Issued Flag */
	if (fgIsValidSsid)
		prConnSettings->fgIsConnReqIssued = TRUE;
	else {
		prConnSettings->eReConnectLevel = RECONNECT_LEVEL_USER_SET;
		prConnSettings->fgIsConnReqIssued = FALSE;
	}

	if (fgEqualSsid || fgEqualBssid)
		prAisAbortMsg->fgDelayIndication = TRUE;
	else
		/* Update the information to CONNECTION_SETTINGS_T */
		prAisAbortMsg->fgDelayIndication = FALSE;

	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prAisAbortMsg, MSG_SEND_METHOD_BUF);

	cnmTimerStopTimer(prAdapter, &prAdapter->rScanNloTimeoutTimer);

	DBGLOG(OID, INFO, "ssid %s, bssid %pM, conn policy %d, disc reason %d\n",
			     HIDE(prConnSettings->aucSSID), prConnSettings->aucBSSID,
			     prConnSettings->eConnectionPolicy, prAisAbortMsg->ucReasonOfDisconnect);
	return WLAN_STATUS_SUCCESS;
}

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

	if (u4SetBufferLen < sizeof(PARAM_SSID_T) || u4SetBufferLen > sizeof(PARAM_SSID_T)) {
		return WLAN_STATUS_INVALID_LENGTH;
	} else if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(OID, WARN, "Fail in set ssid! (Adapter not ready). ACPI=D%d, Radio=%d\n",
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
		} else {
			DBGLOG(OID, TRACE, "DisBySsid\n");
			kalIndicateStatusAndComplete(prGlueInfo, WLAN_STATUS_MEDIA_DISCONNECT, NULL, 0);
		}
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
				if (!((pParamSsid->aucSsid[i] > 0) && (pParamSsid->aucSsid[i] <= 0x1F))) {
					fgIsValidSsid = TRUE;
					break;
				}
			}
		}
	}

	/* Set Connection Request Issued Flag */
	if (fgIsValidSsid) {
		prAdapter->rWifiVar.rConnSettings.fgIsConnReqIssued = TRUE;

		if (pParamSsid->u4SsidLen) {
			prAdapter->rWifiVar.rConnSettings.eConnectionPolicy = CONNECT_BY_SSID_BEST_RSSI;
		} else {
			/* wildcard SSID */
			prAdapter->rWifiVar.rConnSettings.eConnectionPolicy = CONNECT_BY_SSID_ANY;
		}
	} else {
		prAdapter->rWifiVar.rConnSettings.fgIsConnReqIssued = FALSE;
	}

	/* Send AIS Abort Message */
	prAisAbortMsg = (P_MSG_AIS_ABORT_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_AIS_ABORT_T));
	if (!prAisAbortMsg) {
		ASSERT(0);
		return WLAN_STATUS_FAILURE;
	}

	prAisAbortMsg->rMsgHdr.eMsgId = MID_OID_AIS_FSM_JOIN_REQ;
	prAisAbortMsg->ucReasonOfDisconnect = DISCONNECT_REASON_CODE_NEW_CONNECTION;

	COPY_SSID(prAdapter->rWifiVar.rConnSettings.aucSSID,
		  prAdapter->rWifiVar.rConnSettings.ucSSIDLen, pParamSsid->aucSsid, (UINT_8) pParamSsid->u4SsidLen);

#if !defined(CFG_MULTI_SSID_SCAN)
	prAdapter->rWifiVar.rConnSettings.u4FreqInKHz = pParamSsid->u4CenterFreq;
#endif
	if (EQUAL_SSID(prAdapter->rWlanInfo.rCurrBssId.rSsid.aucSsid,
		       prAdapter->rWlanInfo.rCurrBssId.rSsid.u4SsidLen, pParamSsid->aucSsid, pParamSsid->u4SsidLen)) {
		prAisAbortMsg->fgDelayIndication = TRUE;
	} else {
		/* Update the information to CONNECTION_SETTINGS_T */
		prAisAbortMsg->fgDelayIndication = FALSE;
	}
	DBGLOG(OID, INFO, "u4ScanResultNum=%d, SSID=%s, ucSSIDLen=%d\n",
		prAdapter->rWlanInfo.u4ScanResultNum,
		HIDE(prAdapter->rWifiVar.rConnSettings.aucSSID),
		prAdapter->rWifiVar.rConnSettings.ucSSIDLen);

	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prAisAbortMsg, MSG_SEND_METHOD_BUF);

	return WLAN_STATUS_SUCCESS;

}				/* end of wlanoidSetSsid() */

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
		DBGLOG(OID, WARN, "Invalid length %u\n", u4QueryBufferLen);
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

		DBGLOG(OID, TRACE, "Null SSID\n");
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
		DBGLOG(OID, INFO, "IBSS mode\n");
		break;
	case NET_TYPE_INFRA:
		DBGLOG(OID, INFO, "Infrastructure mode\n");
		break;
	default:
		DBGLOG(OID, INFO, "Automatic mode\n");
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

	DEBUGFUNC("wlanoidSetInfrastructureMode");

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	prGlueInfo = prAdapter->prGlueInfo;

	if (u4SetBufferLen < sizeof(ENUM_PARAM_OP_MODE_T))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	*pu4SetInfoLen = sizeof(ENUM_PARAM_OP_MODE_T);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(OID, WARN, "Fail in set infrastructure mode! (Adapter not ready). ACPI=D%d, Radio=%d\n",
				   prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	eOpMode = *(P_ENUM_PARAM_OP_MODE_T) pvSetBuffer;
	/* Verify the new infrastructure mode. */
	if (eOpMode >= NET_TYPE_NUM) {
		DBGLOG(OID, INFO, "Invalid mode value %d\n", eOpMode);
		return WLAN_STATUS_INVALID_DATA;
	}

	/* check if possible to switch to AdHoc mode */
	if (eOpMode == NET_TYPE_IBSS || eOpMode == NET_TYPE_DEDICATED_IBSS) {
		if (cnmAisIbssIsPermitted(prAdapter) == FALSE) {
			DBGLOG(OID, INFO, "Mode value %d unallowed\n", eOpMode);
			return WLAN_STATUS_FAILURE;
		}
	}

	/* Save the new infrastructure mode setting. */
	prAdapter->rWifiVar.rConnSettings.eOPMode = eOpMode;

	/* Clean up the Tx key flag */
	prAdapter->rWifiVar.rAisSpecificBssInfo.fgTransmitKeyExist = FALSE;

	prAdapter->rWifiVar.rConnSettings.fgWapiMode = FALSE;

#if CFG_SUPPORT_802_11W
	prAdapter->rWifiVar.rAisSpecificBssInfo.fgMgmtProtection = FALSE;
	prAdapter->rWifiVar.rAisSpecificBssInfo.fgBipKeyInstalled = FALSE;
#endif

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_INFRASTRUCTURE,
				   TRUE,
				   FALSE,
				   TRUE,
				   nicCmdEventSetCommon, nicOidCmdTimeoutCommon,
				   0, NULL, pvSetBuffer, u4SetBufferLen);

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
		DBGLOG(OID, INFO, "Current auth mode: Open\n");
		break;

	case AUTH_MODE_SHARED:
		DBGLOG(OID, INFO, "Current auth mode: Shared\n");
		break;

	case AUTH_MODE_AUTO_SWITCH:
		DBGLOG(OID, INFO, "Current auth mode: Auto-switch\n");
		break;

	case AUTH_MODE_WPA:
		DBGLOG(OID, INFO, "Current auth mode: WPA\n");
		break;

	case AUTH_MODE_WPA_PSK:
		DBGLOG(OID, INFO, "Current auth mode: WPA PSK\n");
		break;

	case AUTH_MODE_WPA_NONE:
		DBGLOG(OID, INFO, "Current auth mode: WPA None\n");
		break;

	case AUTH_MODE_WPA2:
		DBGLOG(OID, INFO, "Current auth mode: WPA2\n");
		break;

	case AUTH_MODE_WPA2_PSK:
		DBGLOG(OID, INFO, "Current auth mode: WPA2 PSK\n");
		break;

	default:
		DBGLOG(OID, INFO, "Current auth mode: %d\n", *(P_ENUM_PARAM_AUTH_MODE_T) pvQueryBuffer);
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
		DBGLOG(OID, WARN, "Fail in set Authentication mode! (Adapter not ready). ACPI=D%d, Radio=%d\n",
				   prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	/* Check if the new authentication mode is valid. */
	if (*(P_ENUM_PARAM_AUTH_MODE_T) pvSetBuffer >= AUTH_MODE_NUM) {
		DBGLOG(OID, TRACE, "Invalid auth mode %d\n", *(P_ENUM_PARAM_AUTH_MODE_T) pvSetBuffer);
		return WLAN_STATUS_INVALID_DATA;
	}

	switch (*(P_ENUM_PARAM_AUTH_MODE_T) pvSetBuffer) {
#if CFG_SUPPORT_HOTSPOT_2_0
	case AUTH_MODE_WPA_OSEN:
#endif
	case AUTH_MODE_WPA:
	case AUTH_MODE_WPA_PSK:
	case AUTH_MODE_WPA2:
	case AUTH_MODE_WPA2_PSK:
	case AUTH_MODE_WPA2_FT:
	case AUTH_MODE_WPA2_FT_PSK:
	case AUTH_MODE_WPA3_SAE:
	case AUTH_MODE_WPA3_OWE:
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

#if DBG
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

	case AUTH_MODE_WPA3_SAE:
		DBGLOG(RSN, INFO, "New auth mode: SAE\n");
		break;

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
#if CFG_SUPPORT_HOTSPOT_2_0
		case AUTH_MODE_WPA_OSEN:
			u4AkmSuite = WFA_AKM_SUITE_OSEN;
			break;
#endif
		case AUTH_MODE_WPA2_FT:
			u4AkmSuite = RSN_AKM_SUITE_FT_802_1X;
			break;

		case AUTH_MODE_WPA2_FT_PSK:
			u4AkmSuite = RSN_AKM_SUITE_FT_PSK;
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
		DBGLOG(OID, INFO, "Current privacy mode: open mode\n");
		break;

	case PRIVACY_FILTER_8021xWEP:
		DBGLOG(OID, INFO, "Current privacy mode: filtering mode\n");
		break;

	default:
		DBGLOG(OID, INFO, "Current auth mode: %d\n", *(P_ENUM_PARAM_AUTH_MODE_T) pvQueryBuffer);
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
		DBGLOG(OID, WARN, "Fail in set Authentication mode! (Adapter not ready). ACPI=D%d, Radio=%d\n",
				   prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	/* Check if the new authentication mode is valid. */
	if (*(P_ENUM_PARAM_PRIVACY_FILTER_T) pvSetBuffer >= PRIVACY_FILTER_NUM) {
		DBGLOG(OID, TRACE, "Invalid privacy filter %d\n", *(P_ENUM_PARAM_PRIVACY_FILTER_T) pvSetBuffer);
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
		DBGLOG(OID, WARN, "Fail in set Reload default! (Adapter not ready). ACPI=D%d, Radio=%d\n",
				   prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	ASSERT(pvSetBuffer);
	/* Verify the available reload options and reload the settings. */
	switch (*(P_PARAM_RELOAD_DEFAULTS) pvSetBuffer) {
	case ENUM_RELOAD_WEP_KEYS:
		/*
		 * Reload available default WEP keys from the permanent
		 * storage.
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
				DBGLOG(OID, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
				return WLAN_STATUS_FAILURE;
			}
			/* increase command sequence number */
			ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

			/* compose CMD_802_11_KEY cmd pkt */
			prCmdInfo->eCmdType = COMMAND_TYPE_NETWORK_IOCTL;
			prCmdInfo->eNetworkType = NETWORK_TYPE_AIS_INDEX;
			prCmdInfo->u2InfoBufLen = CMD_HDR_SIZE + sizeof(CMD_802_11_KEY);
			prCmdInfo->pfCmdDoneHandler = nicCmdEventSetCommon;
			prCmdInfo->pfCmdTimeoutHandler = nicOidCmdTimeoutCommon;
			prCmdInfo->fgIsOid = TRUE;
			prCmdInfo->ucCID = CMD_ID_ADD_REMOVE_KEY;
			prCmdInfo->fgSetQuery = TRUE;
			prCmdInfo->fgNeedResp = FALSE;
			prCmdInfo->fgDriverDomainMCR = FALSE;
			prCmdInfo->ucCmdSeqNum = ucCmdSeqNum;
			prCmdInfo->u4SetInfoLen = sizeof(PARAM_REMOVE_KEY_T);
			prCmdInfo->pvInformationBuffer = pvSetBuffer;
			prCmdInfo->u4InformationBufferLength = u4SetBufferLen;

			/* Setup WIFI_CMD_T */
			prWifiCmd = (P_WIFI_CMD_T) (prCmdInfo->pucInfoBuffer);
			prWifiCmd->u2TxByteCount_UserPriority = prCmdInfo->u2InfoBufLen;
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
		DBGLOG(OID, TRACE, "Invalid reload option %d\n", *(P_PARAM_RELOAD_DEFAULTS) pvSetBuffer);
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
		DBGLOG(OID, WARN, "Fail in set add WEP! (Adapter not ready). ACPI=D%d, Radio=%d\n",
				   prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	prNewWepKey = (P_PARAM_WEP_T) pvSetBuffer;

	/* Verify the total buffer for minimum length. */
	if (u4SetBufferLen < OFFSET_OF(PARAM_WEP_T, aucKeyMaterial) + prNewWepKey->u4KeyLength) {
		DBGLOG(OID, WARN, "Invalid total buffer length (%d) than minimum length (%d)\n",
				   (UINT_8) u4SetBufferLen, (UINT_8) OFFSET_OF(PARAM_WEP_T, aucKeyMaterial));

		*pu4SetInfoLen = OFFSET_OF(PARAM_WEP_T, aucKeyMaterial);
		return WLAN_STATUS_INVALID_DATA;
	}

	/* Verify the key structure length. */
	if (prNewWepKey->u4Length > u4SetBufferLen) {
		DBGLOG(OID, WARN, "Invalid key structure length (%d) greater than total buffer length (%d)\n",
				   (UINT_8) prNewWepKey->u4Length, (UINT_8) u4SetBufferLen);

		*pu4SetInfoLen = u4SetBufferLen;
		return WLAN_STATUS_INVALID_DATA;
	}

	/* Verify the key material length for maximum key material length:16 */
	if (prNewWepKey->u4KeyLength > 16 /* LEGACY_KEY_MAX_LEN */) {
		DBGLOG(OID, WARN, "Invalid key material length (%d) greater than maximum key material length (16)\n",
				   (UINT_8) prNewWepKey->u4KeyLength);

		*pu4SetInfoLen = u4SetBufferLen;
		return WLAN_STATUS_INVALID_DATA;
	}

	*pu4SetInfoLen = u4SetBufferLen;

	u4KeyId = prNewWepKey->u4KeyIndex & BITS(0, 29) /* WEP_KEY_ID_FIELD */;

	/*
	 * Verify whether key index is valid or not, current version
	 * driver support only 4 global WEP keys setting by this OID
	 */
	if (u4KeyId > MAX_KEY_NUM - 1) {
		DBGLOG(OID, ERROR, "Error, invalid WEP key ID: %d\n", (UINT_8) u4KeyId);
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
	DBGLOG(OID, INFO, "Set: Dump PARAM_KEY_INDEX content\n");
	DBGLOG(OID, INFO, "Index : 0x%08x\n", u4KeyId);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(OID, WARN, "Fail in set remove WEP! (Adapter not ready). ACPI=D%d, Radio=%d\n",
				   prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	if (u4KeyId & IS_TRANSMIT_KEY) {
		/* Bit 31 should not be set */
		DBGLOG(OID, ERROR, "Invalid WEP key index: 0x%08x\n", u4KeyId);
		return WLAN_STATUS_INVALID_DATA;
	}

	u4KeyId &= BITS(0, 7);

	/*
	 * Verify whether key index is valid or not. Current version
	 * driver support only 4 global WEP keys.
	 */
	if (u4KeyId > MAX_KEY_NUM - 1) {
		DBGLOG(OID, ERROR, "invalid WEP key ID %u\n", u4KeyId);
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
_wlanoidSetAddKey(IN P_ADAPTER_T prAdapter,
		  IN PVOID pvSetBuffer,
		  IN UINT_32 u4SetBufferLen, IN BOOLEAN fgIsOid, IN UINT_8 ucAlgorithmId, OUT PUINT_32 pu4SetInfoLen)
{
	P_GLUE_INFO_T prGlueInfo;
	P_CMD_INFO_T prCmdInfo;
	P_WIFI_CMD_T prWifiCmd;
	P_PARAM_KEY_T prNewKey;
	P_CMD_802_11_KEY prCmdKey;
	UINT_8 ucCmdSeqNum;

#if 0
	DEBUGFUNC("wlanoidSetAddKey");
	DBGLOG(OID, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(OID, WARN, "Fail in set add key! (Adapter not ready). ACPI=D%d, Radio=%d\n",
				   prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}
#endif

	prNewKey = (P_PARAM_KEY_T) pvSetBuffer;

#if 0
	/* Verify the key structure length. */
	if (prNewKey->u4Length > u4SetBufferLen) {
		DBGLOG(OID, WARN, "Invalid key structure length (%d) greater than total buffer length (%d)\n",
				   (UINT_8) prNewKey->u4Length, (UINT_8) u4SetBufferLen);

		*pu4SetInfoLen = u4SetBufferLen;
		return WLAN_STATUS_INVALID_LENGTH;
	}

	/* Verify the key material length for key material buffer */
	if (prNewKey->u4KeyLength > prNewKey->u4Length - OFFSET_OF(PARAM_KEY_T, aucKeyMaterial)) {
		DBGLOG(OID, WARN, "Invalid key material length (%d)\n", (UINT_8) prNewKey->u4KeyLength);
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
		if (((prNewKey->u4KeyIndex & 0xff) != 0) ||
		    ((prNewKey->arBSSID[0] == 0xff) && (prNewKey->arBSSID[1] == 0xff) && (prNewKey->arBSSID[2] == 0xff)
		     && (prNewKey->arBSSID[3] == 0xff) && (prNewKey->arBSSID[4] == 0xff)
		     && (prNewKey->arBSSID[5] == 0xff))) {
			return WLAN_STATUS_INVALID_DATA;
		}
	}

	*pu4SetInfoLen = u4SetBufferLen;
#endif

	/* Dump PARAM_KEY content. */
	DBGLOG(OID, TRACE, "Set: PARAM_KEY Length: 0x%08x, Key Index: 0x%08x, Key Length: 0x%08x\n",
		prNewKey->u4Length, prNewKey->u4KeyIndex, prNewKey->u4KeyLength);
	DBGLOG(OID, TRACE, "BSSID:\n");
	DBGLOG_MEM8(OID, TRACE, prNewKey->arBSSID, sizeof(PARAM_MAC_ADDRESS));
	DBGLOG(OID, TRACE, "Key RSC:\n");
	DBGLOG_MEM8(OID, TRACE, &prNewKey->rKeyRSC, sizeof(PARAM_KEY_RSC));
	DBGLOG(OID, TRACE, "Key Material:\n");
	DBGLOG_MEM8(OID, TRACE, prNewKey->aucKeyMaterial, prNewKey->u4KeyLength);

	if (prAdapter->rWifiVar.rConnSettings.eAuthMode < AUTH_MODE_WPA) {
		/* Todo:: Store the legacy wep key for OID_802_11_RELOAD_DEFAULTS */
		/* Todo:: Nothing */
	}

	if (prNewKey->u4KeyIndex & IS_TRANSMIT_KEY)
		prAdapter->rWifiVar.rAisSpecificBssInfo.fgTransmitKeyExist = TRUE;

	prGlueInfo = prAdapter->prGlueInfo;
	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, (CMD_HDR_SIZE + sizeof(CMD_802_11_KEY)));

	if (!prCmdInfo) {
		DBGLOG(OID, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}
	/* increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);
	DBGLOG(OID, TRACE, "ucCmdSeqNum = %d\n", ucCmdSeqNum);

	/* compose CMD_802_11_KEY cmd pkt */
	prCmdInfo->eCmdType = COMMAND_TYPE_NETWORK_IOCTL;
	prCmdInfo->eNetworkType = NETWORK_TYPE_AIS_INDEX;
	prCmdInfo->u2InfoBufLen = CMD_HDR_SIZE + sizeof(CMD_802_11_KEY);
	prCmdInfo->pfCmdDoneHandler = nicCmdEventSetCommon;
	prCmdInfo->pfCmdTimeoutHandler = nicOidCmdTimeoutCommon;
	prCmdInfo->fgIsOid = fgIsOid;
	prCmdInfo->ucCID = CMD_ID_ADD_REMOVE_KEY;
	prCmdInfo->fgSetQuery = TRUE;
	prCmdInfo->fgNeedResp = FALSE;
	prCmdInfo->fgDriverDomainMCR = FALSE;
	prCmdInfo->ucCmdSeqNum = ucCmdSeqNum;
	prCmdInfo->u4SetInfoLen = u4SetBufferLen;
	prCmdInfo->pvInformationBuffer = pvSetBuffer;
	prCmdInfo->u4InformationBufferLength = u4SetBufferLen;

	/* Setup WIFI_CMD_T */
	prWifiCmd = (P_WIFI_CMD_T) (prCmdInfo->pucInfoBuffer);
	prWifiCmd->u2TxByteCount_UserPriority = prCmdInfo->u2InfoBufLen;
	prWifiCmd->ucCID = prCmdInfo->ucCID;
	prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
	prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;

	prCmdKey = (P_CMD_802_11_KEY) (prWifiCmd->aucBuffer);

	kalMemZero(prCmdKey, sizeof(CMD_802_11_KEY));

	prCmdKey->ucAddRemove = 1;	/* Add */

	prCmdKey->ucTxKey = ((prNewKey->u4KeyIndex & IS_TRANSMIT_KEY) == IS_TRANSMIT_KEY) ? 1 : 0;
	prCmdKey->ucKeyType = ((prNewKey->u4KeyIndex & IS_UNICAST_KEY) == IS_UNICAST_KEY) ? 1 : 0;
	prCmdKey->ucIsAuthenticator = ((prNewKey->u4KeyIndex & IS_AUTHENTICATOR) == IS_AUTHENTICATOR) ? 1 : 0;

	kalMemCopy(prCmdKey->aucPeerAddr, (PUINT_8) prNewKey->arBSSID, MAC_ADDR_LEN);

	prCmdKey->ucNetType = 0;	/* AIS */

	prCmdKey->ucKeyId = (UINT_8) (prNewKey->u4KeyIndex & 0xff);

	/* Note: adjust the key length for WPA-None */
	prCmdKey->ucKeyLen = (UINT_8) prNewKey->u4KeyLength;

	kalMemCopy(prCmdKey->aucKeyMaterial, (PUINT_8) prNewKey->aucKeyMaterial, prCmdKey->ucKeyLen);

	prCmdKey->ucAlgorithmId = ucAlgorithmId;
	if (prNewKey->u4KeyLength == 5) {
		prCmdKey->ucAlgorithmId = CIPHER_SUITE_WEP40;
	} else if (prNewKey->u4KeyLength == 13) {
		prCmdKey->ucAlgorithmId = CIPHER_SUITE_WEP104;
	} else if (prNewKey->u4KeyLength == 16) {
		if ((ucAlgorithmId != CIPHER_SUITE_CCMP) &&
			/*Notes: WPA3 will use BIP for management pkts and WPA3 reconnect will use OPEN auth mode */
			(ucAlgorithmId != CIPHER_SUITE_BIP) &&
			(prAdapter->rWifiVar.rConnSettings.eAuthMode < AUTH_MODE_WPA))
			prCmdKey->ucAlgorithmId = CIPHER_SUITE_WEP128;
		else {
#if CFG_SUPPORT_802_11W
			if (prCmdKey->ucKeyId >= 4) {
				P_AIS_SPECIFIC_BSS_INFO_T prAisSpecBssInfo;

				prCmdKey->ucAlgorithmId = CIPHER_SUITE_BIP;
				prAisSpecBssInfo = &prAdapter->rWifiVar.rAisSpecificBssInfo;
				prAisSpecBssInfo->fgBipKeyInstalled = TRUE;
			} else
#endif
				prCmdKey->ucAlgorithmId = CIPHER_SUITE_CCMP;
#if (CFG_REFACTORY_PMKSA == 0)
			if (rsnCheckPmkidCandicate(prAdapter)) {
				P_AIS_SPECIFIC_BSS_INFO_T prAisSpecBssInfo;

				prAisSpecBssInfo = &prAdapter->rWifiVar.rAisSpecificBssInfo;
				DBGLOG(RSN, TRACE,
				       "Add key: Prepare a timer to indicate candidate PMKID Candidate\n");
				cnmTimerStopTimer(prAdapter, &prAisSpecBssInfo->rPreauthenticationTimer);
				cnmTimerStartTimer(prAdapter, &prAisSpecBssInfo->rPreauthenticationTimer,
						   SEC_TO_MSEC(WAIT_TIME_IND_PMKID_CANDICATE_SEC));
			}
#endif
		}
	} else if (prNewKey->u4KeyLength == 32) {
		if (prAdapter->rWifiVar.rConnSettings.eAuthMode == AUTH_MODE_WPA_NONE) {
			if (prAdapter->rWifiVar.rConnSettings.eEncStatus == ENUM_ENCRYPTION2_ENABLED)
				prCmdKey->ucAlgorithmId = CIPHER_SUITE_TKIP;
			else if (prAdapter->rWifiVar.rConnSettings.eEncStatus == ENUM_ENCRYPTION3_ENABLED) {
				prCmdKey->ucAlgorithmId = CIPHER_SUITE_CCMP;
				prCmdKey->ucKeyLen = CCMP_KEY_LEN;
			}
		} else
			prCmdKey->ucAlgorithmId = CIPHER_SUITE_TKIP;
	}
	prAdapter->rWifiVar.rAisSpecificBssInfo.ucKeyAlgorithmId = prCmdKey->ucAlgorithmId;

	DBGLOG(RSN, TRACE, "prCmdKey->ucAlgorithmId=%d, key len=%d\n",
			    prCmdKey->ucAlgorithmId, (UINT32) prNewKey->u4KeyLength);

	/* insert into prCmdQueue */
	kalEnqueueCommand(prGlueInfo, (P_QUE_ENTRY_T) prCmdInfo);

	/* wakeup txServiceThread later */
	GLUE_SET_EVENT(prGlueInfo);

	return WLAN_STATUS_PENDING;
}

WLAN_STATUS
wlanoidSetAddKey(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_PARAM_KEY_T prNewKey;

	DEBUGFUNC("wlanoidSetAddKey");
	DBGLOG(OID, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(OID, WARN, "Fail in set add key! (Adapter not ready). ACPI=D%d, Radio=%d\n",
				   prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	prNewKey = (P_PARAM_KEY_T) pvSetBuffer;

	/* Verify the key structure length. */
	if (prNewKey->u4Length > u4SetBufferLen) {
		DBGLOG(OID, WARN, "Invalid key structure length (%d) greater than total buffer length (%d)\n",
				   (UINT_8) prNewKey->u4Length, (UINT_8) u4SetBufferLen);

		*pu4SetInfoLen = u4SetBufferLen;
		return WLAN_STATUS_INVALID_LENGTH;
	}

	/* Verify the key material length for key material buffer */
	if (prNewKey->u4KeyLength > prNewKey->u4Length - OFFSET_OF(PARAM_KEY_T, aucKeyMaterial)) {
		DBGLOG(OID, WARN, "Invalid key material length (%d)\n", (UINT_8) prNewKey->u4KeyLength);
		*pu4SetInfoLen = u4SetBufferLen;
		return WLAN_STATUS_INVALID_DATA;
	}

	/* Exception check */
	if (prNewKey->u4KeyIndex & 0x0fffff00)
		return WLAN_STATUS_INVALID_DATA;

	/* Exception check, pairwise key must with transmit bit enabled */
	if ((prNewKey->u4KeyIndex & BITS(30, 31)) == BITS(30, 31)) {
		if (EQUAL_MAC_ADDR(prNewKey->arBSSID, "\xff\xff\xff\xff\xff\xff"))
			return WLAN_STATUS_INVALID_DATA;
		if (prNewKey->u4KeyLength == CCMP_KEY_LEN || prNewKey->u4KeyLength == TKIP_KEY_LEN) {
			if ((prNewKey->u4KeyIndex & 0xff) != 0)
				return WLAN_STATUS_INVALID_DATA;
#if 0
			else {
				P_STA_RECORD_T prStaRec =
					cnmGetStaRecByAddress(prAdapter, NETWORK_TYPE_AIS_INDEX, prNewKey->arBSSID);
				if (!prStaRec || prStaRec->ucStaState != STA_STATE_3) {
					DBGLOG(OID, WARN, "station record is null[%d] or state is not 3\n", !prStaRec);
					return WLAN_STATUS_INVALID_DATA;
				}
			}
#endif
		}
	} else if ((prNewKey->u4KeyIndex & BITS(30, 31)) == IS_UNICAST_KEY)
		return WLAN_STATUS_INVALID_DATA;

	if (!(prNewKey->u4KeyLength == WEP_40_LEN || prNewKey->u4KeyLength == WEP_104_LEN ||
	      prNewKey->u4KeyLength == CCMP_KEY_LEN || prNewKey->u4KeyLength == TKIP_KEY_LEN)) {
		return WLAN_STATUS_INVALID_DATA;
	}

	*pu4SetInfoLen = u4SetBufferLen;

#if (CFG_SUPPORT_TDLS == 1)
	/*
	 * supplicant will set key before updating station & enabling the link so we need to
	 * backup the key information and set key when link is enabled
	 */
	if (TdlsexKeyHandle(prAdapter, prNewKey, NETWORK_TYPE_AIS_INDEX) == TDLS_STATUS_SUCCESS)
		return WLAN_STATUS_SUCCESS;
#endif /* CFG_SUPPORT_TDLS */

	return _wlanoidSetAddKey(prAdapter, pvSetBuffer, u4SetBufferLen, TRUE, prNewKey->ucCipher, pu4SetInfoLen);
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

	DEBUGFUNC("wlanoidSetRemoveKey");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(PARAM_REMOVE_KEY_T);

	if (u4SetBufferLen < sizeof(PARAM_REMOVE_KEY_T))
		return WLAN_STATUS_INVALID_LENGTH;

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(OID, WARN, "Fail in set remove key! (Adapter not ready). ACPI=D%d, Radio=%d\n",
				   prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	ASSERT(pvSetBuffer);
	prRemovedKey = (P_PARAM_REMOVE_KEY_T) pvSetBuffer;

	/* Dump PARAM_REMOVE_KEY content. */
	DBGLOG(OID, TRACE, "Set: Dump PARAM_REMOVE_KEY content\n");
	DBGLOG(OID, TRACE, "Length    : 0x%08x\n", prRemovedKey->u4Length);
	DBGLOG(OID, TRACE, "Key Index : 0x%08x\n", prRemovedKey->u4KeyIndex);
	DBGLOG(OID, TRACE, "BSSID:\n");
	DBGLOG_MEM8(OID, TRACE, prRemovedKey->arBSSID, MAC_ADDR_LEN);

	/* Check bit 31: this bit should always 0 */
	if (prRemovedKey->u4KeyIndex & IS_TRANSMIT_KEY) {
		/* Bit 31 should not be set */
		DBGLOG(OID, ERROR, "invalid key index: 0x%08x\n", prRemovedKey->u4KeyIndex);
		return WLAN_STATUS_INVALID_DATA;
	}

	/* Check bits 8 ~ 29 should always be 0 */
	if (prRemovedKey->u4KeyIndex & BITS(8, 29)) {
		/* Bit 31 should not be set */
		DBGLOG(OID, ERROR, "invalid key index: 0x%08x\n", prRemovedKey->u4KeyIndex);
		return WLAN_STATUS_INVALID_DATA;
	}

	/* Clean up the Tx key flag */
	if (prRemovedKey->u4KeyIndex & IS_UNICAST_KEY)
		prAdapter->rWifiVar.rAisSpecificBssInfo.fgTransmitKeyExist = FALSE;

	prGlueInfo = prAdapter->prGlueInfo;
	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, (CMD_HDR_SIZE + sizeof(CMD_802_11_KEY)));

	if (!prCmdInfo) {
		DBGLOG(OID, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}
	/* increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

	/* compose CMD_802_11_KEY cmd pkt */
	prCmdInfo->eCmdType = COMMAND_TYPE_NETWORK_IOCTL;
	prCmdInfo->eNetworkType = NETWORK_TYPE_AIS_INDEX;
	prCmdInfo->u2InfoBufLen = CMD_HDR_SIZE + sizeof(CMD_802_11_KEY);
	prCmdInfo->pfCmdDoneHandler = nicCmdEventSetCommon;
	prCmdInfo->pfCmdTimeoutHandler = nicOidCmdTimeoutCommon;
	prCmdInfo->fgIsOid = TRUE;
	prCmdInfo->ucCID = CMD_ID_ADD_REMOVE_KEY;
	prCmdInfo->fgSetQuery = TRUE;
	prCmdInfo->fgNeedResp = FALSE;
	prCmdInfo->fgDriverDomainMCR = FALSE;
	prCmdInfo->ucCmdSeqNum = ucCmdSeqNum;
	prCmdInfo->u4SetInfoLen = sizeof(PARAM_REMOVE_KEY_T);
	prCmdInfo->pvInformationBuffer = pvSetBuffer;
	prCmdInfo->u4InformationBufferLength = u4SetBufferLen;

	/* Setup WIFI_CMD_T */
	prWifiCmd = (P_WIFI_CMD_T) (prCmdInfo->pucInfoBuffer);
	prWifiCmd->u2TxByteCount_UserPriority = prCmdInfo->u2InfoBufLen;
	prWifiCmd->ucCID = prCmdInfo->ucCID;
	prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
	prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;

	prCmdKey = (P_CMD_802_11_KEY) (prWifiCmd->aucBuffer);

	kalMemZero((PUINT_8) prCmdKey, sizeof(CMD_802_11_KEY));

	prCmdKey->ucAddRemove = 0;	/* Remove */
	prCmdKey->ucKeyId = (UINT_8) (prRemovedKey->u4KeyIndex & 0x000000ff);
	kalMemCopy(prCmdKey->aucPeerAddr, (PUINT_8) prRemovedKey->arBSSID, MAC_ADDR_LEN);

#if CFG_SUPPORT_802_11W
	ASSERT(prCmdKey->ucKeyId < MAX_KEY_NUM + 2);
#else
	/* ASSERT(prCmdKey->ucKeyId < MAX_KEY_NUM); */
#endif

	if (prRemovedKey->u4KeyIndex & IS_UNICAST_KEY)
		prCmdKey->ucKeyType = 1;
	/* insert into prCmdQueue */
	kalEnqueueCommand(prGlueInfo, (P_QUE_ENTRY_T) prCmdInfo);

	/* wakeup txServiceThread later */
	GLUE_SET_EVENT(prGlueInfo);

	return WLAN_STATUS_PENDING;
}				/* wlanoidSetRemoveKey */

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

	fgTransmitKeyAvailable = prAdapter->rWifiVar.rAisSpecificBssInfo.fgTransmitKeyExist;

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
		DBGLOG(OID, ERROR, "Unknown Encryption Status Setting:%d\n",
				    prAdapter->rWifiVar.rConnSettings.eEncStatus);
	}

#if DBG
	DBGLOG(OID, INFO,
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
		DBGLOG(OID, WARN, "Fail in set encryption status! (Adapter not ready). ACPI=D%d, Radio=%d\n",
				   prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	eEewEncrypt = *(P_ENUM_PARAM_ENCRYPTION_STATUS_T) pvSetBuffer;
	DBGLOG(OID, TRACE, "ENCRYPTION_STATUS %d\n", eEewEncrypt);

	switch (eEewEncrypt) {
	case ENUM_ENCRYPTION_DISABLED:	/* Disable WEP, TKIP, AES */
		DBGLOG(RSN, TRACE, "Disable Encryption\n");
		secSetCipherSuite(prAdapter, CIPHER_FLAG_WEP40 | CIPHER_FLAG_WEP104 | CIPHER_FLAG_WEP128);
		break;

	case ENUM_ENCRYPTION1_ENABLED:	/* Enable WEP. Disable TKIP, AES */
		DBGLOG(RSN, TRACE, "Enable Encryption1\n");
		secSetCipherSuite(prAdapter, CIPHER_FLAG_WEP40 | CIPHER_FLAG_WEP104 | CIPHER_FLAG_WEP128);
		break;

	case ENUM_ENCRYPTION2_ENABLED:	/* Enable WEP, TKIP. Disable AES */
		secSetCipherSuite(prAdapter,
				  CIPHER_FLAG_WEP40 | CIPHER_FLAG_WEP104 | CIPHER_FLAG_WEP128 | CIPHER_FLAG_TKIP);
		DBGLOG(RSN, TRACE, "Enable Encryption2\n");
		break;

	case ENUM_ENCRYPTION3_ENABLED:	/* Enable WEP, TKIP, AES */
		secSetCipherSuite(prAdapter,
				  CIPHER_FLAG_WEP40 |
				  CIPHER_FLAG_WEP104 | CIPHER_FLAG_WEP128 | CIPHER_FLAG_TKIP | CIPHER_FLAG_CCMP);
		DBGLOG(RSN, TRACE, "Enable Encryption3\n");
		break;

	default:
		DBGLOG(RSN, WARN, "Unacceptible encryption status: %d\n",
				   *(P_ENUM_PARAM_ENCRYPTION_STATUS_T) pvSetBuffer);

		rStatus = WLAN_STATUS_NOT_SUPPORTED;
	}

	if (rStatus == WLAN_STATUS_SUCCESS) {
		/* Save the new encryption status. */
		prAdapter->rWifiVar.rConnSettings.eEncStatus = *(P_ENUM_PARAM_ENCRYPTION_STATUS_T) pvSetBuffer;

		DBGLOG(RSN, TRACE, "wlanoidSetEncryptionStatus to %d\n",
				    prAdapter->rWifiVar.rConnSettings.eEncStatus);
	}

	return rStatus;
}				/* wlanoidSetEncryptionStatus */
#if (CFG_REFACTORY_PMKSA == 0)
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

	DBGLOG(OID, TRACE, "Test - Type %u\n", prTest->u4Type);

	switch (prTest->u4Type) {
	case 1:		/* Type 1: generate an authentication event */
		pvTestData = (PVOID) &prTest->u.AuthenticationEvent;
		pvStatusBuffer = (PVOID) prAdapter->aucIndicationEventBuffer;
		u4StatusBufferSize = prTest->u4Length - 8;
		if (u4StatusBufferSize > sizeof(PARAM_AUTH_EVENT_T)) {
			DBGLOG(OID, TRACE, "prTest->u4Length error %u\n", u4StatusBufferSize);
			ASSERT(FALSE);
			return WLAN_STATUS_INVALID_LENGTH;
		}
		break;

	case 2:		/* Type 2: generate an RSSI status indication */
		pvTestData = (PVOID) &prTest->u.RssiTrigger;
		pvStatusBuffer = (PVOID) &prAdapter->rWlanInfo.rCurrBssId.rRssi;
		u4StatusBufferSize = sizeof(PARAM_RSSI);
		break;

	default:
		return WLAN_STATUS_INVALID_DATA;
	}

	ASSERT(u4StatusBufferSize <= 180);
	if (u4StatusBufferSize > 180)
		return WLAN_STATUS_INVALID_LENGTH;

	/* Get the contents of the StatusBuffer from the test structure. */
	kalMemCopy(pvStatusBuffer, pvTestData, u4StatusBufferSize);

	kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
				     WLAN_STATUS_MEDIA_SPECIFIC_INDICATION, pvStatusBuffer, u4StatusBufferSize);

	return WLAN_STATUS_SUCCESS;
}				/* wlanoidSetTest */
#endif
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
#if (CFG_REFACTORY_PMKSA == 0)
	prCap->u4NoOfPMKIDs = CFG_MAX_PMKID_CACHE;
#endif
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
#if (CFG_REFACTORY_PMKSA == 0)
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
#endif
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
#if (CFG_REFACTORY_PMKSA == 0)
	UINT_32 i, j = 0;
	P_PARAM_PMKID_T prPmkid;
	P_AIS_SPECIFIC_BSS_INFO_T prAisSpecBssInfo;

	DEBUGFUNC("wlanoidSetPmkid");

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

	DBGLOG(OID, TRACE, "Count %u\n", prPmkid->u4BSSIDInfoCount);

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
	 * The driver can only clear its PMKID cache whenever it make a media disconnect
	 * indication. Otherwise, it must change the PMKID cache only when set through this OID.
	 */
#if CFG_RSN_MIGRATION
	for (i = 0; i < prPmkid->u4BSSIDInfoCount; i++) {
		/*
		 * Search for desired BSSID. If desired BSSID is found,
		 * then set the PMKID
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
			DBGLOG(RSN, TRACE, "Add BSSID %pM idx=%d PMKID value %pM\n",
					    (prAisSpecBssInfo->arPmkidCache[j].rBssidInfo.arBSSID), (UINT_32) j,
					    (prAisSpecBssInfo->arPmkidCache[j].rBssidInfo.arPMKID));
			prAisSpecBssInfo->arPmkidCache[j].fgPmkidExist = TRUE;
		}
	}
#endif
	if (prAdapter->rWifiVar.rConnSettings.fgOkcEnabled &&
	    prPmkid->u4BSSIDInfoCount > 0) {
		P_BSS_DESC_T prBssDesc = prAdapter->rWifiVar.rAisFsmInfo.prTargetBssDesc;
		P_UINT_8 pucPmkID = NULL;

		if ((prPmkid->u4Length & BIT(31)) || (prBssDesc &&
				EQUAL_MAC_ADDR(prPmkid->arBSSIDInfo[0].arBSSID, prBssDesc->aucBSSID))) {
			if (j == CFG_MAX_PMKID_CACHE) {
				j = 0;
				kalMemCopy(prAisSpecBssInfo->arPmkidCache[0].rBssidInfo.arBSSID,
					   prPmkid->arBSSIDInfo[0].arBSSID, sizeof(PARAM_MAC_ADDRESS));
				kalMemCopy(prAisSpecBssInfo->arPmkidCache[0].rBssidInfo.arPMKID,
				   prPmkid->arBSSIDInfo[0].arPMKID, sizeof(PARAM_PMKID_VALUE));
				prAisSpecBssInfo->arPmkidCache[0].fgPmkidExist = TRUE;
			}
			pucPmkID = prAisSpecBssInfo->arPmkidCache[j].rBssidInfo.arPMKID;
			DBGLOG(RSN, INFO,
				"%pM OKC PMKID %02x%02x%02x%02x%02x%02x%02x%02x...\n",
				prAisSpecBssInfo->arPmkidCache[j].rBssidInfo.arBSSID,
				pucPmkID[0], pucPmkID[1], pucPmkID[2], pucPmkID[3],
				pucPmkID[4], pucPmkID[5], pucPmkID[6], pucPmkID[7]);
		}
		aisFsmRunEventSetOkcPmk(prAdapter);
	}

	return WLAN_STATUS_SUCCESS;
#else
	P_PARAM_PMKID_T prPmkid;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = u4SetBufferLen;
	prPmkid = (P_PARAM_PMKID_T) pvSetBuffer;
	if (u4SetBufferLen < sizeof(PARAM_PMKID_T))
		return WLAN_STATUS_INVALID_DATA;
	return rsnSetPmkid(prAdapter, prPmkid);
#endif
}				/* wlanoidSetPmkid */

#if (CFG_REFACTORY_PMKSA == 1)
/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to delete the PMKID in the PMK cache.
 *
 * \param[in] prAdapter Pointer to the Adapter structure.
 * \param[in] pvSetBuffer A pointer to the buffer that holds the data to be set.
 * \param[in] u4SetBufferLen The length of the set buffer.
 * \param[out] pu4SetInfoLen If the call is successful, returns the number of
 *                           bytes read from the set buffer. If the call failed
 *                           due to invalid length of the set buffer, returns
 *                           the amount of storage needed.
 *
 * \retval status
 */
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidDelPmkid(IN P_ADAPTER_T prAdapter,
		IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen,
		OUT PUINT_32 pu4SetInfoLen)
{
	P_PARAM_PMKID_T prPmkid;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = u4SetBufferLen;
	prPmkid = (P_PARAM_PMKID_T) pvSetBuffer;

	if (u4SetBufferLen < sizeof(PARAM_PMKID_T))
		return WLAN_STATUS_INVALID_DATA;
	return rsnDelPmkid(prAdapter, prPmkid);
} /* wlanoidDelPmkid */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to delete all the PMKIDs in the PMK cache.
 *
 * \param[in] prAdapter Pointer to the Adapter structure.
 * \param[in] pvSetBuffer A pointer to the buffer that holds the data to be set.
 * \param[in] u4SetBufferLen The length of the set buffer.
 * \param[out] pu4SetInfoLen If the call is successful, returns the number of
 *                           bytes read from the set buffer. If the call failed
 *                           due to invalid length of the set buffer, returns
 *                           the amount of storage needed.
 *
 * \retval status
 */
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidFlushPmkid(IN P_ADAPTER_T prAdapter,
		IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen,
		OUT PUINT_32 pu4SetInfoLen)
{
	ASSERT(prAdapter);

	return rsnFlushPmkid(prAdapter);
} /* wlanoidFlushPmkid */
#endif

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
		DBGLOG(OID, WARN, "Invalid length %u\n", u4QueryBufferLen);
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
		DBGLOG(OID, WARN, "Invalid length %u\n", u4QueryBufferLen);
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
		DBGLOG(OID, WARN, "Invalid length %u\n", u4SetBufferLen);
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
	DBGLOG(OID, LOUD, "Vendor ID=%02x-%02x-%02x-%02x\n", cp[0], cp[1], cp[2], cp[3]);
#endif

	return WLAN_STATUS_SUCCESS;
}				/* wlanoidQueryVendorId */

WLAN_STATUS
wlanoidRssiMonitor(IN P_ADAPTER_T prAdapter, OUT PVOID pvQueryBuffer,
		   IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	PARAM_RSSI_MONITOR_T rRssi;
	WLAN_STATUS rStatus1 = WLAN_STATUS_SUCCESS;
	WLAN_STATUS rStatus2;

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(PARAM_RSSI_MONITOR_T);

	/* Check for query buffer length */
	if (u4QueryBufferLen < *pu4QueryInfoLen) {
		DBGLOG(OID, WARN, "Too short length %u\n", u4QueryBufferLen);
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}

	if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) ==
	    PARAM_MEDIA_STATE_DISCONNECTED)
		rStatus1 = WLAN_STATUS_ADAPTER_NOT_READY;

	kalMemZero(&rRssi, sizeof(PARAM_RSSI_MONITOR_T));
	kalMemCopy(&rRssi, pvQueryBuffer, sizeof(PARAM_RSSI_MONITOR_T));
	if (rRssi.enable) {
		if (rRssi.max_rssi_value > PARAM_WHQL_RSSI_MAX_DBM)
			rRssi.max_rssi_value = PARAM_WHQL_RSSI_MAX_DBM;
		if (rRssi.min_rssi_value < -120)
			rRssi.min_rssi_value = -120;
	} else {
		rRssi.max_rssi_value = 0;
		rRssi.min_rssi_value = 0;
	}

	DBGLOG(OID, INFO, "enable=%d, max_rssi_value=%d, min_rssi_value=%d\n",
		rRssi.enable, rRssi.max_rssi_value, rRssi.min_rssi_value);

	/*
	 * If status == WLAN_STATUS_ADAPTER_NOT_READY
	 * driver needs to info FW to stop mointor but set oid flag to false
	 * to prevent from multiple complete
	 */
	rStatus2 = wlanSendSetQueryCmd(prAdapter,
			   CMD_ID_RSSI_MONITOR,
			   TRUE,
			   FALSE,
			   (rStatus1 != WLAN_STATUS_ADAPTER_NOT_READY),
			   nicCmdEventSetCommon,
			   nicOidCmdTimeoutCommon,
			   sizeof(PARAM_RSSI_MONITOR_T),
			   (PUINT_8)&rRssi, NULL, 0);

	return (rStatus1 == WLAN_STATUS_ADAPTER_NOT_READY) ?
		rStatus1 : rStatus2;
}

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
	DEBUGFUNC("wlanoidQueryRssi");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(PARAM_RSSI);

	/* Check for query buffer length */
	if (u4QueryBufferLen < *pu4QueryInfoLen) {
		DBGLOG(OID, WARN, "Too short length %u\n", u4QueryBufferLen);
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
		DBGLOG(OID, WARN, "Too short length %u\n", u4QueryBufferLen);
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}

	*(PARAM_RSSI *) pvQueryBuffer = prAdapter->rWlanInfo.rRssiTriggerValue;
	DBGLOG(OID, INFO, "RSSI trigger: %d dBm\n", *(PARAM_RSSI *) pvQueryBuffer);

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

	/*
	 * If the RSSI trigger value is equal to the current RSSI value, the
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
	DBGLOG(OID, LOUD, "\n");

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(OID, WARN, "Fail in query receive error! (Adapter not ready). ACPI=D%d, Radio=%d\n",
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
	DBGLOG(OID, LOUD, "\n");

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(OID, WARN, "Fail in query receive error! (Adapter not ready). ACPI=D%d, Radio=%d\n",
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
	DBGLOG(OID, LOUD, "\n");

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(OID, WARN, "Fail in query receive error! (Adapter not ready). ACPI=D%d, Radio=%d\n",
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
wlanoidQueryStatisticsPL(IN P_ADAPTER_T prAdapter,
			 IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	DBGLOG(OID, LOUD, "\n");

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	*pu4QueryInfoLen = sizeof(PARAM_802_11_STATISTICS_STRUCT_T);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(OID, WARN, "Fail in query receive error! (Adapter not ready). ACPI=D%d, Radio=%d\n",
				   prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		*pu4QueryInfoLen = sizeof(UINT_32);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (u4QueryBufferLen < sizeof(PARAM_802_11_STATISTICS_STRUCT_T)) {
		DBGLOG(OID, WARN, "Too short length %u\n", u4QueryBufferLen);
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
				   CMD_ID_GET_STATISTICS_PL,
				   FALSE,
				   TRUE,
				   TRUE,
				   nicCmdEventQueryStatistics,
				   nicOidCmdTimeoutCommon, 0, NULL, pvQueryBuffer, u4QueryBufferLen);

}				/* wlanoidQueryStatistics */

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
	DEBUGFUNC("wlanoidQueryStatistics");
	DBGLOG(OID, LOUD, "\n");

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	*pu4QueryInfoLen = sizeof(PARAM_802_11_STATISTICS_STRUCT_T);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(OID, WARN, "Fail in query receive error! (Adapter not ready). ACPI=D%d, Radio=%d\n",
				   prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		*pu4QueryInfoLen = sizeof(UINT_32);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (u4QueryBufferLen < sizeof(PARAM_802_11_STATISTICS_STRUCT_T)) {
		DBGLOG(OID, WARN, "Too short length %u\n", u4QueryBufferLen);
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
				   TRUE,
				   nicCmdEventQueryStatistics,
				   nicOidCmdTimeoutCommon, 0, NULL, pvQueryBuffer, u4QueryBufferLen);

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
		DBGLOG(OID, WARN, "Invalid length %u\n", u4QueryBufferLen);
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
		DBGLOG(OID, WARN, "Invalid length %u\n", u4SetBufferLen);
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
	DBGLOG(OID, LOUD, "\n");

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
	CMD_BASIC_CONFIG rCmdBasicConfig;

	DEBUGFUNC("wlanoidQueryCurrentAddr");
	DBGLOG(OID, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (u4QueryBufferLen < MAC_ADDR_LEN)
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	kalMemZero(&rCmdBasicConfig, sizeof(CMD_BASIC_CONFIG));

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_BASIC_CONFIG,
				   FALSE,
				   TRUE,
				   TRUE,
				   nicCmdEventQueryAddress,
				   nicOidCmdTimeoutCommon,
				   sizeof(CMD_BASIC_CONFIG),
				   (PUINT_8) &rCmdBasicConfig, pvQueryBuffer, u4QueryBufferLen);

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
					   TRUE,
					   nicCmdEventQueryLinkSpeed,
					   nicOidCmdTimeoutCommon, 0, NULL, pvQueryBuffer, u4QueryBufferLen);
	}
}				/* end of wlanoidQueryLinkSpeed() */

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
	DBGLOG(OID, LOUD, "\n");

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
					   TRUE,
					   nicCmdEventQueryMcrRead,
					   nicOidCmdTimeoutCommon,
					   sizeof(CMD_ACCESS_REG),
					   (PUINT_8) &rCmdAccessReg, pvQueryBuffer, u4QueryBufferLen);
	} else {
		HAL_MCR_RD(prAdapter, prMcrRdInfo->u4McrOffset & BITS(2, 31),	/* address is in DWORD unit */
			   &prMcrRdInfo->u4McrData);

		DBGLOG(OID, TRACE, "MCR Read: Offset = %#08x, Data = %#08x\n",
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
	P_BSS_INFO_T prBssInfo = &(prAdapter->rWifiVar.arBssInfo[(NETWORK_TYPE_AIS_INDEX)]);
	P_STA_RECORD_T prStaRec = prBssInfo->prStaRecOfAP;
	UINT_32 u4McrOffset, u4McrData;
#endif

	DEBUGFUNC("wlanoidSetMcrWrite");
	DBGLOG(OID, LOUD, "\n");

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
		DBGLOG(OID, TRACE,
		       "[Stress Test] Rate is Changed to index %d...\n", prAdapter->rWifiVar.eRateSetting);
	}
	/* 0xFFFFFFFD for Switch Channel */
	else if (prMcrWrInfo->u4McrOffset == 0xFFFFFFFD) {
		if (prMcrWrInfo->u4McrData <= 11 && prMcrWrInfo->u4McrData >= 1)
			prBssInfo->ucPrimaryChannel = prMcrWrInfo->u4McrData;
		nicUpdateBss(prAdapter, prBssInfo->ucNetTypeIndex);
		DBGLOG(OID, TRACE, "[Stress Test] Channel is switched to %d ...\n", prBssInfo->ucPrimaryChannel);

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

		if (prMcrWrInfo->u4McrData & 0x00000000) {
			prStaRec->u2HtCapInfo &= ~HT_CAP_INFO_SUP_CHNL_WIDTH;
			prBssInfo->eBssSCO = CHNL_EXT_SCN;
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
					   TRUE,
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
					   TRUE,
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
					   TRUE,
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
					   TRUE,
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
						   TRUE,
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
						   TRUE,
						   nicCmdEventSetCommon,
						   nicOidCmdTimeoutCommon,
						   sizeof(CMD_ACCESS_REG),
						   (PUINT_8) &rCmdAccessReg, pvSetBuffer, u4SetBufferLen);
		}
#endif
		/* fill command */
		rCmdAccessReg.u4Address = prMcrWrInfo->u4McrOffset;
		rCmdAccessReg.u4Data = prMcrWrInfo->u4McrData;

		return wlanSendSetQueryCmd(prAdapter,
					   CMD_ID_ACCESS_REG,
					   TRUE,
					   FALSE,
					   TRUE,
					   nicCmdEventSetCommon,
					   nicOidCmdTimeoutCommon,
					   sizeof(CMD_ACCESS_REG),
					   (PUINT_8) &rCmdAccessReg, pvSetBuffer, u4SetBufferLen);
	} else {
		HAL_MCR_WR(prAdapter, (prMcrWrInfo->u4McrOffset & BITS(2, 31)),	/* address is in DWORD unit */
			   prMcrWrInfo->u4McrData);

		DBGLOG(OID, TRACE, "MCR Write: Offset = %#08x, Data = %#08x\n",
				     prMcrWrInfo->u4McrOffset, prMcrWrInfo->u4McrData);

		return WLAN_STATUS_SUCCESS;
	}
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
	DBGLOG(OID, LOUD, "\n");

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

	case 0x9000:
	default:
		{
			rCmdSwCtrl.u4Id = prSwCtrlInfo->u4Id;
			rCmdSwCtrl.u4Data = 0;
			rWlanStatus = wlanSendSetQueryCmd(prAdapter,
							  CMD_ID_SW_DBG_CTRL,
							  FALSE,
							  TRUE,
							  TRUE,
							  nicCmdEventQuerySwCtrlRead,
							  nicOidCmdTimeoutCommon,
							  sizeof(CMD_SW_DBG_CTRL_T),
							  (PUINT_8) &rCmdSwCtrl, pvQueryBuffer, u4QueryBufferLen);
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
	WLAN_STATUS rWlanStatus;
	UINT_16 u2Id, u2SubId;
	UINT_32 u4Data;
#if CFG_SUPPORT_HOTSPOT_OPTIMIZATION
	P_GLUE_INFO_T prGlueInfo;
	CMD_HOTSPOT_OPTIMIZATION_CONFIG arHotspotOptimizationCfg;
#endif

	DEBUGFUNC("wlanoidSetSwCtrlWrite");
	DBGLOG(OID, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

#if CFG_SUPPORT_HOTSPOT_OPTIMIZATION
	prGlueInfo = prAdapter->prGlueInfo;
#endif

	*pu4SetInfoLen = sizeof(PARAM_CUSTOM_SW_CTRL_STRUCT_T);

	if (u4SetBufferLen < sizeof(PARAM_CUSTOM_SW_CTRL_STRUCT_T))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prSwCtrlInfo = (P_PARAM_CUSTOM_SW_CTRL_STRUCT_T) pvSetBuffer;

	u2Id = (UINT_16) (prSwCtrlInfo->u4Id >> 16);
	u2SubId = (UINT_16) (prSwCtrlInfo->u4Id & BITS(0, 15));
	u4Data = prSwCtrlInfo->u4Data;
	rWlanStatus = WLAN_STATUS_SUCCESS;

	switch (u2Id) {

		/* 0x9000 - 0x9EFF reserved for FW */
		/* 0xFFFE          reserved for FW */

#if CFG_SUPPORT_SWCR
	case 0x9F00:
		swCrReadWriteCmd(prAdapter, SWCR_WRITE, (UINT_16) u2SubId, &u4Data);
		break;
#endif /* CFG_SUPPORT_SWCR */

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

				rWlanStatus = nicConfigPowerSaveProfile(prAdapter,
									NETWORK_TYPE_AIS_INDEX, ePowerMode, TRUE);
			}
		}
		break;
	case 0x1001:
		if (u2SubId == 0x0)
			prAdapter->fgEnOnlineScan = (BOOLEAN) u4Data;
		else if (u2SubId == 0x1)
			prAdapter->fgDisBcnLostDetection = (BOOLEAN) u4Data;
		else if (u2SubId == 0x2)
			prAdapter->rWifiVar.fgSupportUAPSD = (BOOLEAN) u4Data;
		else if (u2SubId == 0x3) {
			prAdapter->u4UapsdAcBmp = u4Data & BITS(0, 15);
			prAdapter->rWifiVar.arBssInfo[u4Data >> 16].rPmProfSetupInfo.ucBmpDeliveryAC =
			    (UINT_8) prAdapter->u4UapsdAcBmp;
			prAdapter->rWifiVar.arBssInfo[u4Data >> 16].rPmProfSetupInfo.ucBmpTriggerAC =
			    (UINT_8) prAdapter->u4UapsdAcBmp;
		} else if (u2SubId == 0x4)
			prAdapter->fgDisStaAgingTimeoutDetection = (BOOLEAN) u4Data;
		else if (u2SubId == 0x5)
			prAdapter->rWifiVar.rConnSettings.uc2G4BandwidthMode = (UINT_8) u4Data;
#if CFG_RX_BA_REORDERING_ENHANCEMENT
		else if (u2SubId == 0x6)
			prAdapter->rWifiVar.fgEnableReportIndependentPkt = (BOOLEAN) u4Data;
#endif
		else if (u2SubId == 0x0100)
			prAdapter->rWifiVar.u8SupportRxGf = (UINT_8) u4Data;
		else if (u2SubId == 0x0101) {
			prAdapter->rWifiVar.u8SupportRxSgi20 = (UINT_8) u4Data;
			prAdapter->rWifiVar.u8SupportRxSgi40 = (UINT_8) u4Data;
		} else if (u2SubId == 0x0102)
			prAdapter->rWifiVar.u8SupportRxSTBC = (UINT_8) u4Data;
		else if (u2SubId == 0x0103) { /* AP Mode WMMPS */
			DBGLOG(OID, INFO, "ApUapsd 0x10010103 cmd received: %d\n", u4Data);
			if ((BOOLEAN)u4Data) {
				PARAM_CUSTOM_UAPSD_PARAM_STRUCT_T rUapsdParams;

				kalMemZero(&rUapsdParams, sizeof(PARAM_CUSTOM_UAPSD_PARAM_STRUCT_T));
				prAdapter->rWifiVar.fgSupportUAPSD = TRUE;
				rUapsdParams.fgEnAPSD = 1;
				rUapsdParams.fgEnAPSD_AcBe = 1;
				rUapsdParams.fgEnAPSD_AcBk = 1;
				rUapsdParams.fgEnAPSD_AcVi = 1;
				rUapsdParams.fgEnAPSD_AcVo = 1;
				rUapsdParams.ucMaxSpLen = 0; /* default:0, Do not limit delivery pkt num */

				nicSetUApsdParam(prAdapter, rUapsdParams, NETWORK_TYPE_P2P_INDEX);
			} else {
				prAdapter->rWifiVar.fgSupportUAPSD = FALSE;
			}
		}
		break;

#if CFG_SUPPORT_SWCR
	case 0x1002:
		if (u2SubId == 0x0) {
			if (u4Data)
				u4Data = BIT(HIF_RX_PKT_TYPE_MANAGEMENT);
			swCrFrameCheckEnable(prAdapter, u4Data);
		} else if (u2SubId == 0x1) {
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
	case 0x1003: /* for debug switches */
		switch (u2SubId) {
		case 1:
			DBGLOG(OID, INFO, "Enable VoE 5.7 Packet Jitter test\n");
			prAdapter->rDebugInfo.fgVoE5_7Test = !!u4Data;
			break;
		case 0x02:
			if (prAdapter->prGlueInfo != NULL) {
				DBGLOG(OID, INFO, "usleep 0x1003002 cmd received: %d\n", u4Data);
				prAdapter->prGlueInfo->rHifInfo.fgDmaUsleepEnable = (BOOLEAN) u4Data;
		    }
		    break;
		default:
			break;
		}
		break;
#if CFG_SUPPORT_802_11W
	case 0x2000:
		DBGLOG(RSN, TRACE, "802.11w test 0x%x\n", u2SubId);
		if (u2SubId == 0x0)
			rsnStartSaQuery(prAdapter);
		if (u2SubId == 0x1)
			rsnStopSaQuery(prAdapter);
		if (u2SubId == 0x2)
			rsnSaQueryRequest(prAdapter, NULL);
		if (u2SubId == 0x3) {
			P_BSS_INFO_T prBssInfo = &(prAdapter->rWifiVar.arBssInfo[(NETWORK_TYPE_AIS_INDEX)]);

			authSendDeauthFrame(prAdapter, prBssInfo->prStaRecOfAP, NULL, 7, NULL);
		}
		if (u2SubId == 0x4) {
			P_BSS_INFO_T prBssInfo = &(prAdapter->rWifiVar.arBssInfo[(NETWORK_TYPE_AIS_INDEX)]);

			DBGLOG(RSN, INFO, "Send deauth\n");
			authSendDeauthFrame(prAdapter, prBssInfo->prStaRecOfAP, NULL, 1, NULL);
		}
		/* wext_set_mode */
		/*
		 * if (u2SubId == 0x3) {
		 * prAdapter->prGlueInfo->rWpaInfo.u4Mfp = RSN_AUTH_MFP_DISABLED;
		 * }
		 * if (u2SubId == 0x4) {
		 * //prAdapter->rWifiVar.rAisSpecificBssInfo.fgMgmtProtection = TRUE;
		 * prAdapter->prGlueInfo->rWpaInfo.u4Mfp = RSN_AUTH_MFP_OPTIONAL;
		 * }
		 * if (u2SubId == 0x5) {
		 * //prAdapter->rWifiVar.rAisSpecificBssInfo.fgMgmtProtection = TRUE;
		 * prAdapter->prGlueInfo->rWpaInfo.u4Mfp = RSN_AUTH_MFP_REQUIRED;
		 * }
		 */
		break;
#endif
	case 0xFFFF:
		{
/* CMD_ACCESS_REG rCmdAccessReg; */
#if 1				/* CFG_MT6573_SMT_TEST */
			if (u2SubId == 0x0123) {

				DBGLOG(HAL, TRACE, "set smt fixed rate: %u\n", u4Data);

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
				DBGLOG(OID, TRACE, "DisBySwC\n");
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
#if CFG_SUPPORT_HOTSPOT_OPTIMIZATION
			else if (u2SubId == 0x1240) {
				DBGLOG(P2P, TRACE, "Disable Hotspot Optimization!\n");

				arHotspotOptimizationCfg.fgHotspotOptimizationEn = FALSE;
				arHotspotOptimizationCfg.u4Level = 0;
				wlanoidSendSetQueryP2PCmd(prAdapter,
							  CMD_ID_SET_HOTSPOT_OPTIMIZATION,
							  TRUE,
							  FALSE,
							  TRUE,
							  NULL,
							  NULL,
							  sizeof(CMD_HOTSPOT_OPTIMIZATION_CONFIG),
							  (PUINT_8) &arHotspotOptimizationCfg, NULL, 0);
			} else if (u2SubId == 0x1241) {
				DBGLOG(P2P, TRACE, "Enable Hotspot Optimization!\n");

				arHotspotOptimizationCfg.fgHotspotOptimizationEn = TRUE;
				arHotspotOptimizationCfg.u4Level = 5;
				wlanoidSendSetQueryP2PCmd(prAdapter,
							  CMD_ID_SET_HOTSPOT_OPTIMIZATION,
							  TRUE,
							  FALSE,
							  TRUE,
							  NULL,
							  NULL,
							  sizeof(CMD_HOTSPOT_OPTIMIZATION_CONFIG),
							  (PUINT_8) &arHotspotOptimizationCfg, NULL, 0);
			}
#endif /* CFG_SUPPORT_HOTSPOT_OPTIMIZATION */
			else if (u2SubId == 0x1250) {
				DBGLOG(OID, TRACE, "LTE_COEX: SW SET DUAL BAND\n");
				prAdapter->aePreferBand[NETWORK_TYPE_AIS_INDEX] = BAND_NULL;
			} else if (u2SubId == 0x1251) {
				DBGLOG(OID, TRACE, "LTE_COEX: SW SET 2.4G BAND\n");
				prAdapter->aePreferBand[NETWORK_TYPE_AIS_INDEX] = BAND_2G4;
			} else if (u2SubId == 0x1252) {
				DBGLOG(OID, TRACE, "LTE_COEX: SW SET 5G BAND\n");
				prAdapter->aePreferBand[NETWORK_TYPE_AIS_INDEX] = BAND_5G;
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
							  TRUE,
							  nicCmdEventSetCommon,
							  nicOidCmdTimeoutCommon,
							  sizeof(CMD_SW_DBG_CTRL_T),
							  (PUINT_8) &rCmdSwCtrl, pvSetBuffer, u4SetBufferLen);
		}
	}			/* switch(u2Id)  */

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
				   TRUE,
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
	DBGLOG(OID, LOUD, "\n");

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
	DBGLOG(OID, LOUD, "\n");

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(OID, WARN, "Fail in query receive error! (Adapter not ready). ACPI=D%d, Radio=%d\n",
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
	DBGLOG(OID, LOUD, "\n");

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(OID, WARN, "Fail in query receive error! (Adapter not ready). ACPI=D%d, Radio=%d\n",
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
	DBGLOG(OID, LOUD, "\n");

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(OID, WARN, "Fail in query receive error! (Adapter not ready). ACPI=D%d, Radio=%d\n",
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
	DBGLOG(OID, LOUD, "\n");

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(OID, WARN, "Fail in query receive error! (Adapter not ready). ACPI=D%d, Radio=%d\n",
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
	DBGLOG(OID, LOUD, "\n");

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(OID, WARN, "Fail in query receive error! (Adapter not ready). ACPI=D%d, Radio=%d\n",
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
	DBGLOG(OID, LOUD, "\n");

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(OID, WARN, "Fail in query receive error! (Adapter not ready). ACPI=D%d, Radio=%d\n",
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

	DBGLOG(OID, WARN, "Custom OID interface version: %#08X\n", *(PUINT_32) pvQueryBuffer);

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
	UINT_8 ucNetTypeIndex = NETWORK_TYPE_AIS_INDEX;	/* Caller should provide this information */
	CMD_MAC_MCAST_ADDR rCmdMacMcastAddr;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	/* The data must be a multiple of the Ethernet address size. */
	if ((u4SetBufferLen % MAC_ADDR_LEN)) {
		DBGLOG(OID, WARN, "Invalid MC list length %u\n", u4SetBufferLen);

		*pu4SetInfoLen = (((u4SetBufferLen + MAC_ADDR_LEN) - 1) / MAC_ADDR_LEN) * MAC_ADDR_LEN;

		return WLAN_STATUS_INVALID_LENGTH;
	}

	*pu4SetInfoLen = u4SetBufferLen;

	/* Verify if we can support so many multicast addresses. */
	if (u4SetBufferLen > MAC_ADDR_LEN * MAX_NUM_GROUP_ADDR) {
		DBGLOG(OID, WARN, "Too many MC addresses\n");

		return WLAN_STATUS_MULTICAST_FULL;
	}

	/* NOTE(Kevin): Windows may set u4SetBufferLen == 0 &&
	 * pvSetBuffer == NULL to clear exist Multicast List.
	 */
	if (u4SetBufferLen)
		ASSERT(pvSetBuffer);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(OID, WARN, "Fail in set multicast list! (Adapter not ready). ACPI=D%d, Radio=%d\n",
				   prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	rCmdMacMcastAddr.u4NumOfGroupAddr = u4SetBufferLen / MAC_ADDR_LEN;
	rCmdMacMcastAddr.ucNetTypeIndex = ucNetTypeIndex;
	kalMemCopy(rCmdMacMcastAddr.arAddress, pvSetBuffer, u4SetBufferLen);

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_MAC_MCAST_ADDR,
				   TRUE,
				   FALSE,
				   TRUE,
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
	UINT_32 u4RealPacketFilter;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	if (u4SetBufferLen < sizeof(UINT_32)) {
		*pu4SetInfoLen = sizeof(UINT_32);
		DBGLOG(OID, INFO, "iput buffer is too small");
		return WLAN_STATUS_INVALID_LENGTH;
	}
	ASSERT(pvSetBuffer);

	/* Set the new packet filter. */
	u4NewPacketFilter = *(PUINT_32) pvSetBuffer;

	DBGLOG(OID, TRACE, "New packet filter: %#08x\n", u4NewPacketFilter);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(OID, WARN, "Fail in set current packet filter! (Adapter not ready). ACPI=D%d, Radio=%d\n",
				   prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	do {
		/*
		 * Verify the bits of the new packet filter. If any bits are set that
		 * we don't support, leave.
		 */
		if (u4NewPacketFilter & ~(PARAM_PACKET_FILTER_SUPPORTED)) {
			rStatus = WLAN_STATUS_NOT_SUPPORTED;
			break;
		}
#if DBG
		/*
		 * Need to enable or disable promiscuous support depending on the new
		 * filter.
		 */
		if (u4NewPacketFilter & PARAM_PACKET_FILTER_PROMISCUOUS)
			DBGLOG(OID, TRACE, "Enable promiscuous mode\n");
		else
			DBGLOG(OID, TRACE, "Disable promiscuous mode\n");

		if (u4NewPacketFilter & PARAM_PACKET_FILTER_ALL_MULTICAST)
			DBGLOG(OID, TRACE, "Enable all-multicast mode\n");
		else if (u4NewPacketFilter & PARAM_PACKET_FILTER_MULTICAST)
			DBGLOG(OID, TRACE, "Enable multicast\n");
		else
			DBGLOG(OID, TRACE, "Disable multicast\n");

		if (u4NewPacketFilter & PARAM_PACKET_FILTER_BROADCAST)
			DBGLOG(OID, TRACE, "Enable Broadcast\n");
		else
			DBGLOG(OID, TRACE, "Disable Broadcast\n");
#endif
	} while (FALSE);

	if (rStatus == WLAN_STATUS_SUCCESS) {
		/* Store the packet filter */

		prAdapter->u4OsPacketFilter &= PARAM_PACKET_FILTER_P2P_MASK;
		prAdapter->u4OsPacketFilter |= u4NewPacketFilter;
		u4RealPacketFilter = prAdapter->u4OsPacketFilter | swCrGetDNSRxFilter();

		rStatus = wlanoidSetPacketFilter(prAdapter, u4RealPacketFilter,
					TRUE, pvSetBuffer, u4SetBufferLen);
	}
	DBGLOG(REQ, INFO, "[MC debug] u4OsPacketFilter=%x\n", prAdapter->u4OsPacketFilter);
	return rStatus;
}				/* wlanoidSetCurrentPacketFilter */

WLAN_STATUS wlanoidSetPacketFilter(P_ADAPTER_T prAdapter, UINT_32 u4PacketFilter,
				BOOLEAN fgIsOid, PVOID pvSetBuffer, UINT_32 u4SetBufferLen)
{
#if CFG_SUPPORT_DROP_MC_PACKET
	/* Note:
	*	If PARAM_PACKET_FILTER_ALL_MULTICAST is set in PacketFilter,
	*	Firmware will pass multicast frame.
	*	Else if PARAM_PACKET_FILTER_MULTICAST is set in PacketFilter,
	*	Firmware will pass some multicast frame in multicast table.
	*	Else firmware will drop all multicast frame.
	*/
	if (fgIsUnderSuspend)
		u4PacketFilter &= ~(PARAM_PACKET_FILTER_MULTICAST | PARAM_PACKET_FILTER_ALL_MULTICAST);
#endif

	DBGLOGLIMITED(REQ, INFO, "[MC debug] u4PacketFilter=%x, IsSuspend=%d\n", u4PacketFilter, fgIsUnderSuspend);
	return wlanSendSetQueryCmd(prAdapter,
						   CMD_ID_SET_RX_FILTER,
						   TRUE,
						   FALSE,
						   fgIsOid,
						   nicCmdEventSetCommon,
						   nicOidCmdTimeoutCommon,
						   sizeof(UINT_32),
						   (PUINT_8)&u4PacketFilter,
						   pvSetBuffer, u4SetBufferLen);
}

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
		DBGLOG(OID, INFO, "Query Power State: D0\n");
		break;
	case ParamDeviceStateD1:
		DBGLOG(OID, INFO, "Query Power State: D1\n");
		break;
	case ParamDeviceStateD2:
		DBGLOG(OID, INFO, "Query Power State: D2\n");
		break;
	case ParamDeviceStateD3:
		DBGLOG(OID, INFO, "Query Power State: D3\n");
		break;
	default:
		break;
	}
#endif

	/*
	 * Since we will disconnect the newwork, therefore we do not
	 * need to check queue empty
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
		DBGLOG(OID, INFO, "Set Power State: D0\n");
		kalDevSetPowerState(prAdapter->prGlueInfo, (UINT_32) ParamDeviceStateD0);
		fgRetValue = nicpmSetAcpiPowerD0(prAdapter);
		break;
	case ParamDeviceStateD1:
		DBGLOG(OID, INFO, "Set Power State: D1\n");
		/* no break here */
	case ParamDeviceStateD2:
		DBGLOG(OID, INFO, "Set Power State: D2\n");
		/* no break here */
	case ParamDeviceStateD3:
		DBGLOG(OID, INFO, "Set Power State: D3\n");
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

	DBGLOG(OID, LOUD, "\n");

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

	DBGLOG(OID, LOUD, "\n");

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
		DBGLOG(OID, WARN, "Invalid length %u\n", u4SetBufferLen);
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
		DBGLOG(OID, WARN, "Fail in set disassociate! (Adapter not ready). ACPI=D%d, Radio=%d\n",
				   prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	/* prepare message to AIS */
	prAdapter->rWifiVar.rConnSettings.fgIsConnReqIssued = FALSE;
	prAdapter->rWifiVar.rConnSettings.eReConnectLevel = RECONNECT_LEVEL_USER_SET;

	/* Send AIS Abort Message */
	prAisAbortMsg = (P_MSG_AIS_ABORT_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_AIS_ABORT_T));
	if (!prAisAbortMsg) {
		ASSERT(0);
		return WLAN_STATUS_FAILURE;
	}

	prAisAbortMsg->rMsgHdr.eMsgId = MID_OID_AIS_FSM_JOIN_REQ;
	prAisAbortMsg->ucReasonOfDisconnect = DISCONNECT_REASON_CODE_NEW_CONNECTION;
	prAisAbortMsg->fgDelayIndication = FALSE;

	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prAisAbortMsg, MSG_SEND_METHOD_BUF);

	/* indicate for disconnection */
	if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) == PARAM_MEDIA_STATE_CONNECTED) {
		DBGLOG(OID, INFO, "DisconnectByOid\n");
		kalIndicateStatusAndComplete(prAdapter->prGlueInfo, WLAN_STATUS_MEDIA_DISCONNECT_LOCALLY, NULL, 0);
	}
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

		/*
		 * *(PPARAM_POWER_MODE) pvQueryBuffer =
		 * (PARAM_POWER_MODE)(prAdapter->rWlanInfo.ePowerSaveMode.ucPsProfile);
		 */
		*(PPARAM_POWER_MODE) pvQueryBuffer =
		    (PARAM_POWER_MODE) (prAdapter->rWlanInfo.arPowerSaveMode[NETWORK_TYPE_AIS_INDEX].ucPsProfile);
		*pu4QueryInfoLen = sizeof(PARAM_POWER_MODE);

		/* hack for CTIA power mode setting function */
		if (prAdapter->fgEnCtiaPowerMode) {
			/* set to non-zero value (to prevent MMI query 0, before it intends to set 0, */
			/* which will skip its following state machine) */
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
	PARAM_POWER_MODE ePowerMode;

	DEBUGFUNC("wlanoidSet802dot11PowerSaveProfile");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(PARAM_POWER_MODE);
	if (u4SetBufferLen < sizeof(PARAM_POWER_MODE)) {
		DBGLOG(OID, WARN, "Invalid length %u\n", u4SetBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	} else if (*(PPARAM_POWER_MODE) pvSetBuffer >= Param_PowerModeMax) {
		/* WARNLOG(("Invalid power mode %d\n", */
		/* *(PPARAM_POWER_MODE) pvSetBuffer)); */
		return WLAN_STATUS_INVALID_DATA;
	}

	ePowerMode = *(PPARAM_POWER_MODE) pvSetBuffer;

	if (prAdapter->fgEnCtiaPowerMode) {
		if (ePowerMode == Param_PowerModeCAM)
			;
		else {
			/* User setting to PS mode (Param_PowerModeMAX_PSP or Param_PowerModeFast_PSP) */

			if (prAdapter->u4CtiaPowerMode == 0)
				/* force to keep in CAM mode */
				ePowerMode = Param_PowerModeCAM;
			else if (prAdapter->u4CtiaPowerMode == 1)
				ePowerMode = Param_PowerModeMAX_PSP;
			else if (prAdapter->u4CtiaPowerMode == 2)
				ePowerMode = Param_PowerModeFast_PSP;
		}
	}

	status = nicConfigPowerSaveProfile(prAdapter, NETWORK_TYPE_AIS_INDEX, ePowerMode, TRUE);

	switch (ePowerMode) {
	case Param_PowerModeCAM:
		DBGLOG(OID, INFO, "Set Wi-Fi PS mode to CAM (%d)\n", ePowerMode);
		break;
	case Param_PowerModeMAX_PSP:
		DBGLOG(OID, INFO, "Set Wi-Fi PS mode to MAX PS (%d)\n", ePowerMode);
		break;
	case Param_PowerModeFast_PSP:
		DBGLOG(OID, INFO, "Set Wi-Fi PS mode to FAST PS (%d)\n", ePowerMode);
		break;
	default:
		DBGLOG(OID, INFO, "invalid Wi-Fi PS mode setting (%d)\n", ePowerMode);
		break;
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
		if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) == PARAM_MEDIA_STATE_CONNECTED) {
			*(PUINT_32) pvQueryBuffer =
			    nicChannelNum2Freq(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX].ucPrimaryChannel);
		} else {
			*(PUINT_32) pvQueryBuffer = 0;
		}
	} else {
		*(PUINT_32) pvQueryBuffer = nicChannelNum2Freq(prAdapter->rWifiVar.rConnSettings.ucAdHocChannelNum);
	}

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
		DBGLOG(OID, TRACE, "Invalid Beacon Interval = %u\n", *pu4BeaconInterval);
		return WLAN_STATUS_INVALID_DATA;
	}

	prAdapter->rWlanInfo.u2BeaconPeriod = (UINT_16) *pu4BeaconInterval;

	DBGLOG(OID, INFO, "Set beacon interval: %d\n", prAdapter->rWlanInfo.u2BeaconPeriod);

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
	UINT_32 i, u4CSUMFlags;
	CMD_BASIC_CONFIG rCmdBasicConfig;

	DEBUGFUNC("wlanoidSetCSUMOffload");
	DBGLOG(OID, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(UINT_32);

	if (u4SetBufferLen < sizeof(UINT_32))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);
	u4CSUMFlags = *(PUINT_32) pvSetBuffer;

	kalMemZero(&rCmdBasicConfig, sizeof(CMD_BASIC_CONFIG));

	for (i = 0; i < 6; i++) {	/* set to broadcast address for not-specified */
		rCmdBasicConfig.rMyMacAddr[i] = 0xff;
	}

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

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_BASIC_CONFIG,
				   TRUE,
				   FALSE,
				   TRUE,
				   nicCmdEventSetCommon,
				   nicOidCmdTimeoutCommon,
				   sizeof(CMD_BASIC_CONFIG), (PUINT_8) &rCmdBasicConfig, pvSetBuffer, u4SetBufferLen);
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
	UINT_32 i, j;
	P_CMD_SET_NETWORK_ADDRESS_LIST prCmdNetworkAddressList;
	P_PARAM_NETWORK_ADDRESS_LIST prNetworkAddressList = (P_PARAM_NETWORK_ADDRESS_LIST) pvSetBuffer;
	P_PARAM_NETWORK_ADDRESS prNetworkAddress;
	P_PARAM_NETWORK_ADDRESS_IP prNetAddrIp;
	UINT_32 u4IpAddressCount, u4CmdSize;
	PUINT_8 pucBuf = (PUINT_8) pvSetBuffer;
#if CFG_ENABLE_GTK_FRAME_FILTER
	UINT_32 u4IpV4AddrListSize;
	P_BSS_INFO_T prBssInfo = &prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX];
#endif

	DEBUGFUNC("wlanoidSetNetworkAddress");
	DBGLOG(OID, LOUD, "\n");

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

		prNetworkAddress = (P_PARAM_NETWORK_ADDRESS) (prNetworkAddress +
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

#if CFG_ENABLE_GTK_FRAME_FILTER
	u4IpV4AddrListSize = OFFSET_OF(IPV4_NETWORK_ADDRESS_LIST, arNetAddr) +
	    (u4IpAddressCount * sizeof(IPV4_NETWORK_ADDRESS));
	if (prBssInfo->prIpV4NetAddrList)
		FREE_IPV4_NETWORK_ADDR_LIST(prBssInfo->prIpV4NetAddrList);
	prBssInfo->prIpV4NetAddrList = (P_IPV4_NETWORK_ADDRESS_LIST) kalMemAlloc(u4IpV4AddrListSize, VIR_MEM_TYPE);
	if (prBssInfo->prIpV4NetAddrList == NULL) {
		kalMemFree(prCmdNetworkAddressList, VIR_MEM_TYPE, u4CmdSize);
		return WLAN_STATUS_FAILURE;
	}
	prBssInfo->prIpV4NetAddrList->ucAddrCount = (UINT_8) u4IpAddressCount;
#endif

	/* fill P_CMD_SET_NETWORK_ADDRESS_LIST */
	prCmdNetworkAddressList->ucNetTypeIndex = NETWORK_TYPE_AIS_INDEX;

	/* only to set IP address to FW once ARP filter is enabled */
	if (prAdapter->fgEnArpFilter) {
		prCmdNetworkAddressList->ucAddressCount = (UINT_8) u4IpAddressCount;
		prNetworkAddress = prNetworkAddressList->arAddress;

		DBGLOG(OID, INFO, "u4IpAddressCount (%u)\n", u4IpAddressCount);

		for (i = 0, j = 0; i < prNetworkAddressList->u4AddressCount; i++) {
			if (prNetworkAddress->u2AddressType == PARAM_PROTOCOL_ID_TCP_IP &&
			    prNetworkAddress->u2AddressLength == sizeof(PARAM_NETWORK_ADDRESS_IP)) {
				prNetAddrIp = (P_PARAM_NETWORK_ADDRESS_IP) prNetworkAddress->aucAddress;

				kalMemCopy(prCmdNetworkAddressList->arNetAddress[j].aucIpAddr,
					   &(prNetAddrIp->in_addr), sizeof(UINT_32));

#if CFG_ENABLE_GTK_FRAME_FILTER
				kalMemCopy(prBssInfo->prIpV4NetAddrList->arNetAddr[j].aucIpAddr,
					   &(prNetAddrIp->in_addr), sizeof(UINT_32));
#endif

				j++;

				pucBuf = (PUINT_8) &prNetAddrIp->in_addr;
				DBGLOG(OID, INFO,
				       "prNetAddrIp->in_addr:%d:%d:%d:%d\n", pucBuf[0], pucBuf[1], pucBuf[2],
					pucBuf[3]);
			}

			prNetworkAddress = (P_PARAM_NETWORK_ADDRESS) (prNetworkAddress +
								      (ULONG) (prNetworkAddress->u2AddressLength +
									       OFFSET_OF(PARAM_NETWORK_ADDRESS,
											 aucAddress)));
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
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = 0;

	if (u4SetBufferLen == 0) {
		if (prAdapter->fgTestMode == FALSE) {
			/* switch to RF Test mode */
			rCmdTestCtrl.ucAction = 0;	/* Switch mode */
			rCmdTestCtrl.u.u4OpMode = 1;	/* RF test mode */

			rStatus = wlanSendSetQueryCmd(prAdapter,
						      CMD_ID_TEST_MODE,
						      TRUE,
						      TRUE,
						      TRUE,
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
	DBGLOG(OID, INFO, "Enter TestMode, setBufLen %u, InTestMode %d, rStatus %u\n",
				u4SetBufferLen, prAdapter->fgTestMode, rStatus);
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

	DEBUGFUNC("wlanoidRftestSetTestMode");

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = 0;

	if (u4SetBufferLen == 0) {
		if (prAdapter->fgTestMode == TRUE) {
			/* switch to normal mode */
			rCmdTestCtrl.ucAction = 0;	/* Switch mode */
			rCmdTestCtrl.u.u4OpMode = 0;	/* normal mode */

			rStatus = wlanSendSetQueryCmd(prAdapter,
						      CMD_ID_TEST_MODE,
						      TRUE,
						      FALSE,
						      TRUE,
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
	DBGLOG(OID, INFO, "Abort TestMode, setBufLen %u, InTestMode %d, rStatus %u\n",
					u4SetBufferLen, prAdapter->fgTestMode, rStatus);
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

	/*pu4QueryInfoLen is depended on upper-layer*/
	*pu4QueryInfoLen = u4QueryBufferLen;

	if (u4QueryBufferLen != sizeof(PARAM_MTK_WIFI_TEST_STRUCT_T))
		DBGLOG(OID, WARN, "Invalid data. QueryBufferLen: %u.\n", u4QueryBufferLen);

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

	if (u4SetBufferLen != sizeof(PARAM_MTK_WIFI_TEST_STRUCT_T))
		DBGLOG(OID, WARN, "Invalid data. SetBufferLen: %u.\n", u4SetBufferLen);


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
		DBGLOG(OID, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}
	/* increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

	/* Setup common CMD Info Packet */
	prCmdInfo->eCmdType = COMMAND_TYPE_GENERAL_IOCTL;
	prCmdInfo->u2InfoBufLen = CMD_HDR_SIZE + sizeof(CMD_TEST_CTRL_T);
	prCmdInfo->pfCmdDoneHandler = nicCmdEventSetCommon;
	prCmdInfo->pfCmdTimeoutHandler = nicOidCmdTimeoutCommon;
	prCmdInfo->fgIsOid = TRUE;
	prCmdInfo->ucCID = CMD_ID_TEST_MODE;
	prCmdInfo->fgSetQuery = TRUE;
	prCmdInfo->fgNeedResp = FALSE;
	prCmdInfo->fgDriverDomainMCR = FALSE;
	prCmdInfo->ucCmdSeqNum = ucCmdSeqNum;
	prCmdInfo->u4SetInfoLen = sizeof(CMD_TEST_CTRL_T);
	prCmdInfo->pvInformationBuffer = NULL;
	prCmdInfo->u4InformationBufferLength = 0;

	/* Setup WIFI_CMD_T (payload = CMD_TEST_CTRL_T) */
	prWifiCmd = (P_WIFI_CMD_T) (prCmdInfo->pucInfoBuffer);
	prWifiCmd->u2TxByteCount_UserPriority = prCmdInfo->u2InfoBufLen;
	prWifiCmd->ucCID = prCmdInfo->ucCID;
	prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
	prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;

	pCmdTestCtrl = (P_CMD_TEST_CTRL_T) (prWifiCmd->aucBuffer);
	pCmdTestCtrl->ucAction = 1;	/* Set ATInfo */
	pCmdTestCtrl->u.rRfATInfo.u4FuncIndex = u4FuncIndex;
	pCmdTestCtrl->u.rRfATInfo.u4FuncData = u4FuncData;

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
		if (u4QueryBufferLen > 8) {
			/*support FW version extended*/
			prTestStatus->rATInfo.u4FuncData2 = prAdapter->rVerInfo.u2FwOwnVersionExtend;

			DBGLOG(OID, INFO, "<wifi> version: 0x%x ,extended : 0x%x\n"
				, prTestStatus->rATInfo.u4FuncData
				, prTestStatus->rATInfo.u4FuncData2);
		} else
			DBGLOG(OID, INFO, "<wifi> version: 0x%x\n"
				, prTestStatus->rATInfo.u4FuncData);

		return WLAN_STATUS_SUCCESS;
	} else if (u4FuncIndex == RF_AT_FUNCID_DRV_INFO) {
		/* driver implementation */
		prTestStatus = (P_EVENT_TEST_STATUS) pvQueryBuffer;
		prTestStatus->rATInfo.u4FuncData = CFG_DRV_OWN_VERSION;

		return WLAN_STATUS_SUCCESS;
	}

	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, (CMD_HDR_SIZE + sizeof(CMD_TEST_CTRL_T)));

	if (!prCmdInfo) {
		DBGLOG(OID, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}
	/* increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

	/* Setup common CMD Info Packet */
	prCmdInfo->eCmdType = COMMAND_TYPE_GENERAL_IOCTL;
	prCmdInfo->u2InfoBufLen = CMD_HDR_SIZE + sizeof(CMD_TEST_CTRL_T);
	prCmdInfo->pfCmdDoneHandler = nicCmdEventQueryRfTestATInfo;
	prCmdInfo->pfCmdTimeoutHandler = nicOidCmdTimeoutCommon;
	prCmdInfo->fgIsOid = TRUE;
	prCmdInfo->ucCID = CMD_ID_TEST_MODE;
	prCmdInfo->fgSetQuery = FALSE;
	prCmdInfo->fgNeedResp = TRUE;
	prCmdInfo->fgDriverDomainMCR = FALSE;
	prCmdInfo->ucCmdSeqNum = ucCmdSeqNum;
	prCmdInfo->u4SetInfoLen = sizeof(CMD_TEST_CTRL_T);
	prCmdInfo->pvInformationBuffer = pvQueryBuffer;
	prCmdInfo->u4InformationBufferLength = u4QueryBufferLen;

	/* Setup WIFI_CMD_T (payload = CMD_TEST_CTRL_T) */
	prWifiCmd = (P_WIFI_CMD_T) (prCmdInfo->pucInfoBuffer);
	prWifiCmd->u2TxByteCount_UserPriority = prCmdInfo->u2InfoBufLen;
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
				   CMD_ID_TEST_MODE,
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
		DBGLOG(OID, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}
	/* increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);
	DBGLOG(OID, TRACE, "ucCmdSeqNum =%d, ucCID =%d\n", ucCmdSeqNum, ucCID);

	/* Setup common CMD Info Packet */
	prCmdInfo->eCmdType = COMMAND_TYPE_NETWORK_IOCTL;
	prCmdInfo->eNetworkType = NETWORK_TYPE_AIS_INDEX;
	prCmdInfo->u2InfoBufLen = (UINT_16) (CMD_HDR_SIZE + u4SetQueryInfoLen);
	prCmdInfo->pfCmdDoneHandler = pfCmdDoneHandler;
	prCmdInfo->pfCmdTimeoutHandler = pfCmdTimeoutHandler;
	prCmdInfo->fgIsOid = fgIsOid;
	prCmdInfo->ucCID = ucCID;
	prCmdInfo->fgSetQuery = fgSetQuery;
	prCmdInfo->fgNeedResp = fgNeedResp;
	prCmdInfo->fgDriverDomainMCR = FALSE;
	prCmdInfo->ucCmdSeqNum = ucCmdSeqNum;
	prCmdInfo->u4SetInfoLen = u4SetQueryInfoLen;
	prCmdInfo->pvInformationBuffer = pvSetQueryBuffer;
	prCmdInfo->u4InformationBufferLength = u4SetQueryBufferLen;

	/* Setup WIFI_CMD_T (no payload) */
	prWifiCmd = (P_WIFI_CMD_T) (prCmdInfo->pucInfoBuffer);
	prWifiCmd->u2TxByteCount_UserPriority = prCmdInfo->u2InfoBufLen;
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
	DBGLOG(OID, LOUD, "\r\n");

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
		DBGLOG(OID, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}
	/* increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

	/* compose CMD_BUILD_CONNECTION cmd pkt */
	prCmdInfo->eCmdType = COMMAND_TYPE_NETWORK_IOCTL;
	prCmdInfo->eNetworkType = NETWORK_TYPE_AIS_INDEX;
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
	prWifiCmd->u2TxByteCount_UserPriority = prCmdInfo->u2InfoBufLen;
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

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	DEBUGFUNC("wlanoidSetWapiAssocInfo");
	DBGLOG(OID, LOUD, "\r\n");

	if (u4SetBufferLen < 20 /* From EID to Group cipher */) {
		prAdapter->rWifiVar.rConnSettings.fgWapiMode = FALSE;
		DBGLOG(SEC, INFO, "fgWapiMode = FALSE due to u4SetBufferLen %u < 20!\n", u4SetBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	if (u4SetBufferLen > sizeof(prAdapter->prGlueInfo->aucWapiAssocInfoIEs))
		return WLAN_STATUS_INVALID_LENGTH;

	prAdapter->rWifiVar.rConnSettings.fgWapiMode = TRUE;

	/* if (prWapiInfo->ucElemId != ELEM_ID_WAPI) */
	/* DBGLOG(SEC, TRACE, ("Not WAPI IE ?!\n")); */

	/* if (prWapiInfo->ucLength < 18) */
	/* return WLAN_STATUS_INVALID_LENGTH; */

	*pu4SetInfoLen = u4SetBufferLen;

	prWapiInfo = (P_WAPI_INFO_ELEM_T) pvSetBuffer;

	if (prWapiInfo->ucElemId != ELEM_ID_WAPI) {
		DBGLOG(SEC, INFO, "Not WAPI IE ?! u4SetBufferLen = %u\n", u4SetBufferLen);
		prAdapter->rWifiVar.rConnSettings.fgWapiMode = FALSE;
		return WLAN_STATUS_INVALID_LENGTH;
	}

	if (prWapiInfo->ucLength < 18)
		return WLAN_STATUS_INVALID_LENGTH;

	/* Skip Version check */
	cp = (PUINT_8) &prWapiInfo->u2AuthKeyMgtSuiteCount;

	WLAN_GET_FIELD_16(cp, &u2AuthSuiteCount);

	if (u2AuthSuiteCount > 1)
		return WLAN_STATUS_INVALID_LENGTH;

	cp = (PUINT_8) &prWapiInfo->aucAuthKeyMgtSuite1[0];
	WLAN_GET_FIELD_32(cp, &u4AuthKeyMgtSuite);

	DBGLOG(SEC, TRACE, "WAPI: Assoc Info auth mgt suite [%d]: %02x-%02x-%02x-%02x\n",
			    u2AuthSuiteCount,
			    (UCHAR) (u4AuthKeyMgtSuite & 0x000000FF),
			    (UCHAR) ((u4AuthKeyMgtSuite >> 8) & 0x000000FF),
			    (UCHAR) ((u4AuthKeyMgtSuite >> 16) & 0x000000FF),
			    (UCHAR) ((u4AuthKeyMgtSuite >> 24) & 0x000000FF));

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

	DEBUGFUNC("wlanoidSetWapiKey");
	DBGLOG(OID, LOUD, "\r\n");

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(OID, WARN, "Fail in set add key! (Adapter not ready). ACPI=D%d, Radio=%d\r\n",
				   prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	prNewKey = (P_PARAM_WPI_KEY_T) pvSetBuffer;

	DBGLOG_MEM8(OID, TRACE, (PUINT_8) pvSetBuffer, 560);
	pc = (PUINT_8) pvSetBuffer;

	*pu4SetInfoLen = u4SetBufferLen;

	/* Exception check */
	if (prNewKey->ucKeyID != 0x1 && prNewKey->ucKeyID != 0x0) {
		prNewKey->ucKeyID = prNewKey->ucKeyID & BIT(0);
		/* DBGLOG(SEC, INFO, ("Invalid WAPI key ID (%d)\r\n", prNewKey->ucKeyID)); */
	}

	/* Dump P_PARAM_WPI_KEY_T content. */
	DBGLOG(OID, TRACE, "Set: Dump P_PARAM_WPI_KEY_T content\r\n");
	DBGLOG(OID, TRACE, "TYPE      : %d\r\n", prNewKey->eKeyType);
	DBGLOG(OID, TRACE, "Direction : %d\r\n", prNewKey->eDirection);
	DBGLOG(OID, TRACE, "KeyID     : %d\r\n", prNewKey->ucKeyID);
	DBGLOG(OID, TRACE, "AddressIndex:\r\n");
	DBGLOG_MEM8(OID, TRACE, prNewKey->aucAddrIndex, 12);
	prNewKey->u4LenWPIEK = 16;

	DBGLOG_MEM8(OID, TRACE, (PUINT_8) prNewKey->aucWPIEK, (UINT_8) prNewKey->u4LenWPIEK);
	prNewKey->u4LenWPICK = 16;

	DBGLOG(OID, TRACE, "CK Key(%d):\r\n", (UINT_8) prNewKey->u4LenWPICK);
	DBGLOG_MEM8(OID, TRACE, (PUINT_8) prNewKey->aucWPICK, (UINT_8) prNewKey->u4LenWPICK);
	DBGLOG(OID, TRACE, "PN:\r\n");
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

	DBGLOG_MEM8(OID, TRACE, (PUINT_8) prNewKey->aucPN, 16);

	prGlueInfo = prAdapter->prGlueInfo;

	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, (CMD_HDR_SIZE + u4SetBufferLen));

	if (!prCmdInfo) {
		DBGLOG(OID, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}
	/* increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

	/* compose CMD_ID_ADD_REMOVE_KEY cmd pkt */
	prCmdInfo->eCmdType = COMMAND_TYPE_NETWORK_IOCTL;
	prCmdInfo->eNetworkType = NETWORK_TYPE_AIS_INDEX;
	prCmdInfo->u2InfoBufLen = CMD_HDR_SIZE + sizeof(CMD_802_11_KEY);
	prCmdInfo->pfCmdDoneHandler = nicCmdEventSetCommon;
	prCmdInfo->pfCmdTimeoutHandler = nicOidCmdTimeoutCommon;
	prCmdInfo->fgIsOid = TRUE;
	prCmdInfo->ucCID = CMD_ID_ADD_REMOVE_KEY;
	prCmdInfo->fgSetQuery = TRUE;
	prCmdInfo->fgNeedResp = FALSE;
	prCmdInfo->fgDriverDomainMCR = FALSE;
	prCmdInfo->ucCmdSeqNum = ucCmdSeqNum;
	prCmdInfo->u4SetInfoLen = u4SetBufferLen;
	prCmdInfo->pvInformationBuffer = pvSetBuffer;
	prCmdInfo->u4InformationBufferLength = u4SetBufferLen;

	/* Setup WIFI_CMD_T */
	prWifiCmd = (P_WIFI_CMD_T) (prCmdInfo->pucInfoBuffer);
	prWifiCmd->u2TxByteCount_UserPriority = prCmdInfo->u2InfoBufLen;
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

	prCmdKey->ucNetType = 0;	/* AIS */

	prCmdKey->ucKeyId = prNewKey->ucKeyID;

	prCmdKey->ucKeyLen = 32;

	prCmdKey->ucAlgorithmId = CIPHER_SUITE_WPI;

	kalMemCopy(prCmdKey->aucKeyMaterial, (PUINT_8) prNewKey->aucWPIEK, 16);

	kalMemCopy(prCmdKey->aucKeyMaterial + 16, (PUINT_8) prNewKey->aucWPICK, 16);

	kalMemCopy(prCmdKey->aucKeyRsc, (PUINT_8) prNewKey->aucPN, 16);

	/* insert into prCmdQueue */
	kalEnqueueCommand(prGlueInfo, (P_QUE_ENTRY_T) prCmdInfo);

	/* wakeup txServiceThread later */
	GLUE_SET_EVENT(prGlueInfo);

	return WLAN_STATUS_PENDING;
}				/* wlanoidSetAddKey */
#endif

#if CFG_ENABLE_WAKEUP_ON_LAN
WLAN_STATUS
wlanoidSetAddWakeupPattern(IN P_ADAPTER_T prAdapter,
			   IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_PARAM_PM_PACKET_PATTERN prPacketPattern;

	DEBUGFUNC("wlanoidSetAddWakeupPattern");
	DBGLOG(OID, LOUD, "\r\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(PARAM_PM_PACKET_PATTERN);

	if (u4SetBufferLen < sizeof(PARAM_PM_PACKET_PATTERN))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prPacketPattern = (P_PARAM_PM_PACKET_PATTERN) pvSetBuffer;

	/*
	 * FIXME:
	 * Send the struct to firmware
	 */

	return WLAN_STATUS_FAILURE;
}

WLAN_STATUS
wlanoidSetRemoveWakeupPattern(IN P_ADAPTER_T prAdapter,
			      IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_PARAM_PM_PACKET_PATTERN prPacketPattern;

	DEBUGFUNC("wlanoidSetAddWakeupPattern");
	DBGLOG(OID, LOUD, "\r\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(PARAM_PM_PACKET_PATTERN);

	if (u4SetBufferLen < sizeof(PARAM_PM_PACKET_PATTERN))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prPacketPattern = (P_PARAM_PM_PACKET_PATTERN) pvSetBuffer;

	/*
	 * FIXME:
	 * Send the struct to firmware
	 */

	return WLAN_STATUS_FAILURE;
}

WLAN_STATUS
wlanoidQueryEnableWakeup(IN P_ADAPTER_T prAdapter,
			 OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	PUINT_32 pu4WakeupEventEnable;

	DEBUGFUNC("wlanoidQueryEnableWakeup");
	DBGLOG(OID, LOUD, "\r\n");

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
	DBGLOG(OID, LOUD, "\r\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(UINT_32);

	if (u4SetBufferLen < sizeof(UINT_32))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	pu4WakeupEventEnable = (PUINT_32) pvSetBuffer;
	prAdapter->u4WakeupEventEnable = *pu4WakeupEventEnable;

	/*
	 * FIXME:
	 * Send Command Event for setting wakeup-pattern / Magic Packet to firmware
	 */

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

	rSetWmmPsTestParam.ucNetTypeIndex = NETWORK_TYPE_AIS_INDEX;
	rSetWmmPsTestParam.bmfgApsdEnAc = prWmmPsTestInfo->bmfgApsdEnAc;
	rSetWmmPsTestParam.ucIsEnterPsAtOnce = prWmmPsTestInfo->ucIsEnterPsAtOnce;
	rSetWmmPsTestParam.ucIsDisableUcTrigger = prWmmPsTestInfo->ucIsDisableUcTrigger;

	prBssInfo = &(prAdapter->rWifiVar.arBssInfo[rSetWmmPsTestParam.ucNetTypeIndex]);
	prPmProfSetupInfo = &prBssInfo->rPmProfSetupInfo;
	prPmProfSetupInfo->ucBmpDeliveryAC = (rSetWmmPsTestParam.bmfgApsdEnAc >> 4) & BITS(0, 3);
	prPmProfSetupInfo->ucBmpTriggerAC = rSetWmmPsTestParam.bmfgApsdEnAc & BITS(0, 3);

	u2CmdBufLen = sizeof(CMD_SET_WMM_PS_TEST_STRUCT_T);

#if 0
	/* it will apply the disable trig or not immediately */
	if (prPmInfo->ucWmmPsDisableUcPoll && prPmInfo->ucWmmPsConnWithTrig)
		; /* NIC_PM_WMM_PS_DISABLE_UC_TRIG(prAdapter, TRUE); */
	else
		; /* NIC_PM_WMM_PS_DISABLE_UC_TRIG(prAdapter, FALSE); */
#endif

	rStatus = wlanSendSetQueryCmd(prAdapter, CMD_ID_SET_WMM_PS_TEST_PARMS, TRUE, FALSE, TRUE, NULL,	/* TODO? */
				      NULL, u2CmdBufLen, (PUINT_8) &rSetWmmPsTestParam, NULL, 0);

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
	DBGLOG(OID, LOUD, "\r\n");

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
	DBGLOG(OID, LOUD, "\r\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(PARAM_MTK_SLT_TEST_STRUCT_T);

	if (u4SetBufferLen < sizeof(PARAM_MTK_SLT_TEST_STRUCT_T))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prMtkSltInfo = (P_PARAM_MTK_SLT_TEST_STRUCT_T) pvSetBuffer;

	prSltInfo = &(prAdapter->rWifiVar.rSltInfo);
	prBssInfo = &prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX];

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
			break;
		}

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
			/* This controls RF BW, RF BW would be 40 only if */
			/* 1. PHY_TYPE_BIT_HT is TRUE. */
			/* 2. SCO is SCA/SCB. */
			prStaRec->ucDesiredPhyTypeSet = PHY_TYPE_BIT_HT;

			/* U20/L20 Control. */
			switch (prTRSetting->u4FixedRate & 0xC000) {
			case FIXED_EXT_CHNL_U20:
				prBssInfo->eBssSCO = CHNL_EXT_SCB;	/* +2 */
				if (prTRSetting->rNetworkType == PARAM_NETWORK_TYPE_OFDM5)
					prBssInfo->ucPrimaryChannel += 2;
				else {
					/* For channel 1, testing L20 at channel 8. */
					if (prBssInfo->ucPrimaryChannel < 5)
						prBssInfo->ucPrimaryChannel = 8;
				}
				break;
			case FIXED_EXT_CHNL_L20:
			default:	/* 40M */
				prBssInfo->eBssSCO = CHNL_EXT_SCA;	/* -2 */
				if (prTRSetting->rNetworkType == PARAM_NETWORK_TYPE_OFDM5) {
					prBssInfo->ucPrimaryChannel -= 2;
				} else {
					/* For channel 11 / 14. testing U20 at channel 3. */
					if (prBssInfo->ucPrimaryChannel > 10)
						prBssInfo->ucPrimaryChannel = 3;
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
			prStaRec->u2DesiredNonHTRateSet = BIT(RATE_54M_INDEX);
			break;
		case RATE_OFDM_48M:
			prStaRec->u2DesiredNonHTRateSet = BIT(RATE_48M_INDEX);
			break;
		case RATE_OFDM_36M:
			prStaRec->u2DesiredNonHTRateSet = BIT(RATE_36M_INDEX);
			break;
		case RATE_OFDM_24M:
			prStaRec->u2DesiredNonHTRateSet = BIT(RATE_24M_INDEX);
			break;
		case RATE_OFDM_6M:
			prStaRec->u2DesiredNonHTRateSet = BIT(RATE_6M_INDEX);
			break;
		case RATE_CCK_11M_LONG:
			prStaRec->u2DesiredNonHTRateSet = BIT(RATE_11M_INDEX);
			break;
		case RATE_CCK_1M_LONG:
			prStaRec->u2DesiredNonHTRateSet = BIT(RATE_1M_INDEX);
			break;
		case RATE_GF_MCS_0:
			prStaRec->u2DesiredNonHTRateSet = BIT(RATE_HT_PHY_INDEX);
			prStaRec->u2HtCapInfo |= HT_CAP_INFO_HT_GF;
			break;
		case RATE_MM_MCS_7:
			prStaRec->u2DesiredNonHTRateSet = BIT(RATE_HT_PHY_INDEX);
			prStaRec->u2HtCapInfo &= ~HT_CAP_INFO_HT_GF;
#if 0				/* Only for Current Measurement Mode. */
			prStaRec->u2HtCapInfo |= (HT_CAP_INFO_SHORT_GI_20M | HT_CAP_INFO_SHORT_GI_40M);
#endif
			break;
		case RATE_GF_MCS_7:
			prStaRec->u2DesiredNonHTRateSet = BIT(RATE_HT_PHY_INDEX);
			prStaRec->u2HtCapInfo |= HT_CAP_INFO_HT_GF;
			break;
		default:
			prStaRec->u2DesiredNonHTRateSet = BIT(RATE_36M_INDEX);
			break;
		}

		cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_1);
		cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_3);

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
		/* change to byte offset */
		fgStatus = kalCfgDataRead16(prAdapter->prGlueInfo,
						prNvramRwInfo->ucEepromIndex << 1,
						&u2Data);

		if (fgStatus) {
			prNvramRwInfo->u2EepromData = u2Data;
			DBGLOG(OID, INFO, "NVRAM Read: index=%#X, data=%#02X\r\n",
					   prNvramRwInfo->ucEepromIndex, u2Data);
		} else {
			DBGLOG(OID, ERROR, "NVRAM Read Failed: index=%#x.\r\n", prNvramRwInfo->ucEepromIndex);
			rStatus = WLAN_STATUS_FAILURE;
		}
	} else if (prNvramRwInfo->ucEepromMethod == PARAM_EEPROM_READ_METHOD_GETSIZE) {
		prNvramRwInfo->u2EepromData = CFG_FILE_WIFI_REC_SIZE;
		DBGLOG(OID, INFO, "EEPROM size =%d\r\n", prNvramRwInfo->u2EepromData);
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
	DBGLOG(OID, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(PARAM_CUSTOM_NVRAM_RW_STRUCT_T);

	if (u4SetBufferLen < sizeof(PARAM_CUSTOM_NVRAM_RW_STRUCT_T))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prNvramRwInfo = (P_PARAM_CUSTOM_NVRAM_RW_STRUCT_T) pvSetBuffer;

	/* change to byte offset */
	fgStatus = kalCfgDataWrite16(prAdapter->prGlueInfo,
					prNvramRwInfo->ucEepromIndex << 1,
					prNvramRwInfo->u2EepromData);

	if (fgStatus == FALSE) {
		DBGLOG(OID, ERROR, "NVRAM Write Failed.\r\n");
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
	UINT_16 u2CountryCode;

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(u4SetBufferLen == 2);

	*pu4SetInfoLen = 2;

	pucCountry = pvSetBuffer;
	u2CountryCode = (((UINT_16) pucCountry[0]) << 8) | ((UINT_16) pucCountry[1]);

	/* previous country code == FF : ignore country code, current country code == FE : resume */
	if (prAdapter->rWifiVar.rConnSettings.u2CountryCodeBakup == COUNTRY_CODE_FF) {
		if (u2CountryCode != COUNTRY_CODE_FE) {
			DBGLOG(OID, INFO, "Skip country code cmd (0x%04x)\n", u2CountryCode);
			return WLAN_STATUS_SUCCESS;
		}
		DBGLOG(OID, INFO, "Resume handle country code cmd (0x%04x)\n", u2CountryCode);
	}

	prAdapter->rWifiVar.rConnSettings.u2CountryCode = u2CountryCode;
	prAdapter->rWifiVar.rConnSettings.u2CountryCodeBakup = prAdapter->rWifiVar.rConnSettings.u2CountryCode;
	DBGLOG(OID, LOUD, "u2CountryCodeBakup=0x%04x\n", prAdapter->rWifiVar.rConnSettings.u2CountryCodeBakup);

	/* Force to re-search country code in country domains */
	prAdapter->prDomainInfo = NULL;
	rlmDomainSendCmd(prAdapter, FALSE);

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
	DBGLOG(OID, LOUD, "\n");

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
	DBGLOG(OID, LOUD, "\n");

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
	DBGLOG(OID, LOUD, "\n");

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

#ifdef CFG_TC1_FEATURE /* for Passive Scan */
/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set Passive Scan mode.
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
wlanoidSetPassiveScan(IN P_ADAPTER_T prAdapter,
			IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	PUINT_8 pucScanType;

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(u4SetBufferLen == 1);

	*pu4SetInfoLen = 1;

	pucScanType = pvSetBuffer;

	if (*pucScanType == 0x2)
		prAdapter->ucScanType = SCAN_TYPE_PASSIVE_SCAN;
	else
		prAdapter->ucScanType = SCAN_TYPE_ACTIVE_SCAN;

	return WLAN_STATUS_SUCCESS;
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
		WARNLOG(("Invalid length %u\n", u4SetBufferLen));
		return WLAN_STATUS_INVALID_LENGTH;
	}

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(OID, WARN, "Fail to set BT profile because of ACPI_D3\n");
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	ASSERT(pvSetBuffer);
	prPtaIpc = (P_PTA_IPC_T) pvSetBuffer;

#if CFG_SUPPORT_BCM && CFG_SUPPORT_BCM_BWCS && CFG_SUPPORT_BCM_BWCS_DEBUG
	DBGLOG(OID, INFO,
		"BCM BWCS CMD: BTPParams[0]=%02x, BTPParams[1]=%02x, BTPParams[2]=%02x, BTPParams[3]=%02x.\n",
		prPtaIpc->u.aucBTPParams[0], prPtaIpc->u.aucBTPParams[1], prPtaIpc->u.aucBTPParams[2],
		prPtaIpc->u.aucBTPParams[3];

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
		DBGLOG(OID, WARN, "Invalid length %u\n", u4QueryBufferLen);
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
		DBGLOG(OID, WARN, "Invalid length %u\n", u4QueryBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	ASSERT(pvQueryBuffer);

	prPtaInfo = &prAdapter->rPtaInfo;
	pu4SingleAntenna = (PUINT_32) pvQueryBuffer;

	if (prPtaInfo->fgSingleAntenna) {
		/* DBGLOG(OID, INFO, (KERN_WARNING DRV_NAME"Q Single Ant = 1\r\n")); */
		*pu4SingleAntenna = 1;
	} else {
		/* DBGLOG(OID, INFO, (KERN_WARNING DRV_NAME"Q Single Ant = 0\r\n")); */
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
		DBGLOG(OID, WARN, "Fail to set antenna because of ACPI_D3\n");
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	ASSERT(pvSetBuffer);
	pu4SingleAntenna = (PUINT_32) pvSetBuffer;
	u4SingleAntenna = *pu4SingleAntenna;

	if (u4SingleAntenna == 0) {
		/* DBGLOG(OID, INFO, (KERN_WARNING DRV_NAME"Set Single Ant = 0\r\n")); */
		prPtaInfo->fgSingleAntenna = FALSE;
	} else {
		/* DBGLOG(OID, INFO, (KERN_WARNING DRV_NAME"Set Single Ant = 1\r\n")); */
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
		DBGLOG(OID, WARN, "Invalid length %u\n", u4QueryBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	ASSERT(pvQueryBuffer);

	prPtaInfo = &prAdapter->rPtaInfo;
	pu4Pta = (PUINT_32) pvQueryBuffer;

	if (prPtaInfo->fgEnabled) {
		/* DBGLOG(OID, INFO, (KERN_WARNING DRV_NAME"PTA = 1\r\n")); */
		*pu4Pta = 1;
	} else {
		/* DBGLOG(OID, INFO, (KERN_WARNING DRV_NAME"PTA = 0\r\n")); */
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
		DBGLOG(OID, WARN, "Fail to set BT setting because of ACPI_D3\n");
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	ASSERT(pvSetBuffer);
	pu4PtaCtrl = (PUINT_32) pvSetBuffer;
	u4PtaCtrl = *pu4PtaCtrl;

	if (u4PtaCtrl == 0) {
		/* DBGLOG(OID, INFO, (KERN_WARNING DRV_NAME"Set Pta= 0\r\n")); */
		nicPtaSetFunc(prAdapter, FALSE);
	} else {
		/* DBGLOG(OID, INFO, (KERN_WARNING DRV_NAME"Set Pta= 1\r\n")); */
		nicPtaSetFunc(prAdapter, TRUE);
	}

	return WLAN_STATUS_SUCCESS;
}
#endif

#endif


WLAN_STATUS
wlanoidSetRxPacketFilterPriv(
	IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32	pu4SetInfoLen)
{

	WLAN_STATUS rStatus;

	DEBUGFUNC("wlanoidSetPacketFilter");

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);

	DBGLOG(OID, TRACE, "wlanoidSetRxPacketFilterPriv\n");

	rStatus = wlanSendSetQueryCmd(
				prAdapter,					/* prAdapter */
				CMD_ID_SET_DROP_PACKET_CFG,	/* ucCID */
				TRUE,						/* fgSetQuery */
				FALSE,						/* fgNeedResp */
				TRUE,						/* fgIsOid */
				nicCmdEventSetCommon,		/* pfCmdDoneHandler*/
				nicOidCmdTimeoutCommon,		/* pfCmdTimeoutHandler */
				u4SetBufferLen,				/* u4SetQueryInfoLen */
				(PUINT_8) pvSetBuffer,		/* pucInfoBuffer */
				NULL,						/* pvSetQueryBuffer */
				0							/* u4SetQueryBufferLen */
				);

	ASSERT(rStatus == WLAN_STATUS_PENDING);

	return rStatus;

}

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
	/* P_SET_TXPWR_CTRL_T pTxPwr = (P_SET_TXPWR_CTRL_T)pvSetBuffer; */
	/* UINT_32 i; */
	WLAN_STATUS rStatus;

	DEBUGFUNC("wlanoidSetTxPower");
	DBGLOG(OID, LOUD, "\r\n");

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);

#if 0
	DBGLOG(OID, INFO, "c2GLegacyStaPwrOffset=%d\n", pTxPwr->c2GLegacyStaPwrOffset);
	DBGLOG(OID, INFO, "c2GHotspotPwrOffset=%d\n", pTxPwr->c2GHotspotPwrOffset);
	DBGLOG(OID, INFO, "c2GP2pPwrOffset=%d\n", pTxPwr->c2GP2pPwrOffset);
	DBGLOG(OID, INFO, "c2GBowPwrOffset=%d\n", pTxPwr->c2GBowPwrOffset);
	DBGLOG(OID, INFO, "c5GLegacyStaPwrOffset=%d\n", pTxPwr->c5GLegacyStaPwrOffset);
	DBGLOG(OID, INFO, "c5GHotspotPwrOffset=%d\n", pTxPwr->c5GHotspotPwrOffset);
	DBGLOG(OID, INFO, "c5GP2pPwrOffset=%d\n", pTxPwr->c5GP2pPwrOffset);
	DBGLOG(OID, INFO, "c5GBowPwrOffset=%d\n", pTxPwr->c5GBowPwrOffset);
	DBGLOG(OID, INFO, "ucConcurrencePolicy=%d\n", pTxPwr->ucConcurrencePolicy);

	for (i = 0; i < 14; i++)
		DBGLOG(OID, INFO, "acTxPwrLimit2G[%d]=%d\n", i, pTxPwr->acTxPwrLimit2G[i]);

	for (i = 0; i < 4; i++)
		DBGLOG(OID, INFO, "acTxPwrLimit5G[%d]=%d\n", i, pTxPwr->acTxPwrLimit5G[i]);
#endif

	rStatus = wlanSendSetQueryCmd(prAdapter,	/* prAdapter */
				      CMD_ID_SET_TXPWR_CTRL,	/* ucCID */
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

	ASSERT(rStatus == WLAN_STATUS_PENDING);

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

		DBGLOG(OID, TRACE, "[%d] 0x%X, len %u, remain len %u\n",
				    ucFragNum,
				    prCmdDumpMem->u4Address, prCmdDumpMem->u4Length, prCmdDumpMem->u4RemainLength);

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
	DBGLOG(OID, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(UINT_32);

	prMemDumpInfo = (P_PARAM_CUSTOM_MEM_DUMP_STRUCT_T) pvQueryBuffer;
	DBGLOG(OID, TRACE, "Dump 0x%X, len %u\n", prMemDumpInfo->u4Address, prMemDumpInfo->u4Length);

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
		DBGLOG(OID, WARN, "Invalid length %u\n", u4SetBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	prSetP2P = (P_PARAM_CUSTOM_P2P_SET_STRUCT_T) pvSetBuffer;

	DBGLOG(P2P, INFO, "Set P2P enable %p [%u] mode[%u]\n", prSetP2P, prSetP2P->u4Enable, prSetP2P->u4Mode);

	/*
	 *    enable = 1, mode = 0  => init P2P network
	 *    enable = 1, mode = 1  => init Soft AP network
	 *    enable = 0                   => uninit P2P/AP network
	 */

	if (prSetP2P->u4Enable) {
		p2pSetMode((prSetP2P->u4Mode == 1) ? TRUE : FALSE);

		if (p2pLaunch(prAdapter->prGlueInfo))
			ASSERT(prAdapter->fgIsP2PRegistered);

		if (prAdapter->rWifiVar.fgSupportUAPSD && prSetP2P->u4Mode == 1) {
			PARAM_CUSTOM_UAPSD_PARAM_STRUCT_T rUapsdParams;

			DBGLOG(OID, INFO, "wlanoidSetP2pMode Default enable ApUapsd\n");

			kalMemZero(&rUapsdParams, sizeof(PARAM_CUSTOM_UAPSD_PARAM_STRUCT_T));
			rUapsdParams.fgEnAPSD = 1;
			rUapsdParams.fgEnAPSD_AcBe = 1;
			rUapsdParams.fgEnAPSD_AcBk = 1;
			rUapsdParams.fgEnAPSD_AcVi = 1;
			rUapsdParams.fgEnAPSD_AcVo = 1;
			rUapsdParams.ucMaxSpLen = 0; /* default:0, Do not limit delivery pkt num */

			nicSetUApsdParam(prAdapter, rUapsdParams, NETWORK_TYPE_P2P_INDEX);
		}

	} else {
		DBGLOG(P2P, TRACE, "prAdapter->fgIsP2PRegistered = %d\n", prAdapter->fgIsP2PRegistered);

		if (prAdapter->fgIsP2PRegistered) {
			DBGLOG(P2P, INFO, "p2pRemove\n");
			p2pRemove(prAdapter->prGlueInfo);
		}

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

#if CFG_SUPPORT_BUILD_DATE_CODE
/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to query build date code information from firmware
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
wlanoidQueryBuildDateCode(IN P_ADAPTER_T prAdapter,
			  OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	CMD_GET_BUILD_DATE_CODE rCmdGetBuildDateCode;

	DEBUGFUNC("wlanoidQueryBuildDateCode");
	DBGLOG(OID, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(UINT_8) * 16;

	if (u4QueryBufferLen < sizeof(UINT_8) * 16)
		return WLAN_STATUS_INVALID_LENGTH;

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_GET_BUILD_DATE_CODE,
				   FALSE,
				   TRUE,
				   TRUE,
				   nicCmdEventBuildDateCode,
				   nicOidCmdTimeoutCommon,
				   sizeof(CMD_GET_BUILD_DATE_CODE),
				   (PUINT_8) &rCmdGetBuildDateCode, pvQueryBuffer, u4QueryBufferLen);

}				/* end of wlanoidQueryBuildDateCode() */
#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to query BSS info from firmware
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
wlanoidQueryBSSInfo(IN P_ADAPTER_T prAdapter,
		    OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	EVENT_AIS_BSS_INFO_T rCmdBSSInfo;

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(EVENT_AIS_BSS_INFO_T);

	if (u4QueryBufferLen < sizeof(EVENT_AIS_BSS_INFO_T))
		return WLAN_STATUS_INVALID_LENGTH;
	kalMemZero(&rCmdBSSInfo, sizeof(EVENT_AIS_BSS_INFO_T));
	/*
	 * rStatus = wlanSendSetQueryCmd(prAdapter,
	 *                            CMD_ID_GET_BSS_INFO,
	 *                            FALSE,
	 *                            TRUE,
	 *                            TRUE,
	 *                            nicCmdEventGetBSSInfo,
	 *                            nicOidCmdTimeoutCommon,
	 *                            sizeof(P_EVENT_AIS_BSS_INFO_T),
	 *                            (PUINT_8) &rCmdBSSInfo, pvQueryBuffer, u4QueryBufferLen);
	 */
	rStatus = wlanSendSetQueryCmd(prAdapter,
				      CMD_ID_GET_BSS_INFO,
				      FALSE,
				      TRUE,
				      TRUE,
				      nicCmdEventGetBSSInfo,
				      nicOidCmdTimeoutCommon,
				      sizeof(EVENT_AIS_BSS_INFO_T),
				      (PUINT_8) &rCmdBSSInfo, pvQueryBuffer, u4QueryBufferLen);

	return rStatus;
}				/* wlanoidSetWiFiWmmPsTest */

#if CFG_SUPPORT_BATCH_SCAN

#define CMD_WLS_BATCHING		"WLS_BATCHING"

#define BATCHING_SET            "SET"
#define BATCHING_GET            "GET"
#define BATCHING_STOP           "STOP"

#define PARAM_SCANFREQ          "SCANFREQ"
#define PARAM_MSCAN             "MSCAN"
#define PARAM_BESTN	            "BESTN"
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

	DBGLOG(SCN, TRACE, "[BATCH] command=%s, len=%u\n", (PCHAR) pvSetBuffer, (UINT_32) u4SetBufferLen);

	if (!pu4WritenLen)
		return -EINVAL;
	*pu4WritenLen = 0;

	if (u4SetBufferLen < kalStrLen(CMD_WLS_BATCHING)) {
		DBGLOG(SCN, TRACE, "[BATCH] invalid len %u\n", (UINT_32) u4SetBufferLen);
		return -EINVAL;
	}

	head = pvSetBuffer + kalStrLen(CMD_WLS_BATCHING) + 1;
	kalMemSet(&rCmdBatchReq, 0, sizeof(CMD_BATCH_REQ_T));

	if (!kalStrnCmp(head, BATCHING_SET, kalStrLen(BATCHING_SET))) {

		DBGLOG(SCN, TRACE, "XXX Start Batch Scan XXX\n");

		head += kalStrLen(BATCHING_SET) + 1;

		/* SCANFREQ, MSCAN, BESTN */
		tokens = kalSScanf(head, "SCANFREQ=%d MSCAN=%d BESTN=%d", &scanfreq, &mscan, &bestn);
		if (tokens != 3) {
			DBGLOG(SCN, TRACE, "[BATCH] Parse fail: tokens=%u, SCANFREQ=%d MSCAN=%d BESTN=%d\n",
					    (UINT_32) tokens, scanfreq, mscan, bestn);
			return -EINVAL;
		}
		/* RTT */
		p = kalStrStr(head, PARAM_RTT);
		if (!p) {
			DBGLOG(SCN, TRACE, "[BATCH] Parse RTT fail. head=%s\n", head);
			return -EINVAL;
		}
		tokens = kalSScanf(p, "RTT=%d", &rtt);
		if (tokens != 1) {
			DBGLOG(SCN, TRACE, "[BATCH] Parse fail: tokens=%u, rtt=%d\n", (UINT_32) tokens, rtt);
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
		/*
		 * else {
		 * *p = '.'; // remove '>' because sscanf can not parse <%s>
		 * }
		 */
		/*
		 * tokens = kalSScanf(head, "CHANNEL=<%s", c_channel);
		 * if (tokens != 1) {
		 * DBGLOG(SCN, TRACE, ("[BATCH] Parse fail: tokens=%d, CHANNEL=<%s>\n",
		 * tokens, c_channel));
		 * return -EINVAL;
		 * }
		 */
		rCmdBatchReq.ucChannelType = SCAN_CHANNEL_SPECIFIED;
		rCmdBatchReq.ucChannelListNum = 0;
		prRfChannelInfo = &rCmdBatchReq.arChannelList[0];
		p = head + kalStrLen(PARAM_CHANNEL) + 2;	/* c_channel; */
		while ((p2 = kalStrSep((char **)&p, ",")) != NULL) {
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
				prRfChannelInfo->ucChannelNum = kalStrtol(p2, NULL, 0);
				DBGLOG(SCN, TRACE, "Scanning Channel:%u,  freq: %d\n",
						    (UINT_32) prRfChannelInfo->ucChannelNum,
						    (UINT_32) nicChannelNum2Freq(prRfChannelInfo->ucChannelNum));
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
					    prRfChannelInfo->ucChannelNum,
					    nicChannelNum2Freq(prRfChannelInfo->ucChannelNum));
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
		DBGLOG(SCN, TRACE, "[BATCH] SCANFREQ=%u MSCAN=%u BESTN=%u RTT=%u\n",
				    (UINT_32) rCmdBatchReq.u4Scanfreq,
				    (UINT_32) rCmdBatchReq.ucMScan,
				    (UINT_32) rCmdBatchReq.ucBestn, (UINT_32) rCmdBatchReq.ucRtt;

		if (rCmdBatchReq.ucChannelType != SCAN_CHANNEL_SPECIFIED) {
			DBGLOG(SCN, TRACE, "[BATCH] CHANNELS = %s\n",
					    rCmdBatchReq.ucChannelType == SCAN_CHANNEL_FULL ? "FULL" :
					    rCmdBatchReq.ucChannelType == SCAN_CHANNEL_2G4 ? "2.4G all" : "5G all");
		} else {
			DBGLOG(SCN, TRACE, "[BATCH] CHANNEL list\n");
			prRfChannelInfo = &rCmdBatchReq.arChannelList[0];
			for (tokens = 0; tokens < rCmdBatchReq.ucChannelListNum; tokens++) {
				DBGLOG(SCN, TRACE, "[BATCH] %s, %d\n",
						    prRfChannelInfo->ucBand == BAND_2G4 ? "2.4G" : "5G",
						    prRfChannelInfo->ucChannelNum);
				prRfChannelInfo++;
			}
		}

		rCmdBatchReq.ucSeqNum = 1;
		rCmdBatchReq.ucNetTypeIndex = NETWORK_TYPE_AIS_INDEX;
		rCmdBatchReq.ucCmd = SCAN_BATCH_REQ_START;

		*pu4WritenLen = kalSnprintf(pvSetBuffer, 3, "%d", rCmdBatchReq.ucMScan);

	} else if (!kalStrnCmp(head, BATCHING_STOP, kalStrLen(BATCHING_STOP))) {

		DBGLOG(SCN, TRACE, "XXX Stop Batch Scan XXX\n");

		rCmdBatchReq.ucSeqNum = 1;
		rCmdBatchReq.ucNetTypeIndex = NETWORK_TYPE_AIS_INDEX;
		rCmdBatchReq.ucCmd = SCAN_BATCH_REQ_STOP;
	} else {
		return -EINVAL;
	}

	rStatus = wlanSendSetQueryCmd(prAdapter,
			    CMD_ID_SET_BATCH_REQ,
			    TRUE, FALSE, TRUE, NULL, NULL, sizeof(CMD_BATCH_REQ_T), (PUINT_8) &rCmdBatchReq, NULL, 0);

	/* kalMemSet(pvSetBuffer, 0, u4SetBufferLen); */
	/* rStatus = kalSnprintf(pvSetBuffer, 2, "%s", "OK"); */

	return rStatus;
}

WLAN_STATUS
batchGetCmd(IN P_ADAPTER_T prAdapter,
	    OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	CMD_BATCH_REQ_T rCmdBatchReq;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	P_EVENT_BATCH_RESULT_T prEventBatchResult;

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	prEventBatchResult = (P_EVENT_BATCH_RESULT_T) pvQueryBuffer;

	DBGLOG(SCN, TRACE, "XXX Get Batch Scan Result (%u) XXX\n", (UINT_32) prEventBatchResult->ucScanCount);

	*pu4QueryInfoLen = sizeof(EVENT_BATCH_RESULT_T);

	rCmdBatchReq.ucSeqNum = 2;
	rCmdBatchReq.ucCmd = SCAN_BATCH_REQ_RESULT;
	rCmdBatchReq.ucMScan = prEventBatchResult->ucScanCount;	/* Get which round result */

	rStatus = wlanSendSetQueryCmd(prAdapter,
				      CMD_ID_SET_BATCH_REQ,
				      FALSE,
				      TRUE,
				      TRUE,
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
	P_SCAN_INFO_T prScanInfo;

	ASSERT(prAdapter);
	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);

	DEBUGFUNC("wlanoidSetStartSchedScan()");

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(SCN, WARN, "Fail in set scheduled scan! (Adapter not ready). ACPI=D%d, Radio=%d\n",
				   prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	ASSERT(pu4SetInfoLen);
	*pu4SetInfoLen = 0;

	if (u4SetBufferLen != sizeof(PARAM_SCHED_SCAN_REQUEST)) {
		return WLAN_STATUS_INVALID_LENGTH;
	} else if (pvSetBuffer == NULL) {
		return WLAN_STATUS_INVALID_DATA;
	} else if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) == PARAM_MEDIA_STATE_CONNECTED &&
		   prAdapter->fgEnOnlineScan == FALSE) {
		return WLAN_STATUS_FAILURE;
	}

	if (prAdapter->fgIsRadioOff) {
		DBGLOG(SCN, WARN, "Return from BSSID list scan! (radio off). ACPI=D%d, Radio=%d\n",
				   prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_SUCCESS;
	}

	prSchedScanRequest = (P_PARAM_SCHED_SCAN_REQUEST) pvSetBuffer;

	/*if schedScanReq is pending ,save it*/
	kalMemCopy(&prScanInfo->rSchedScanRequest, prSchedScanRequest, sizeof(PARAM_SCHED_SCAN_REQUEST));

	if (scnFsmSchedScanRequest(prAdapter) == TRUE)
		return WLAN_STATUS_SUCCESS;
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
		return WLAN_STATUS_SUCCESS;
	else
		return WLAN_STATUS_FAILURE;
}

#if CFG_SUPPORT_GSCN
/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set a periodically PSCN action
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
wlanoidSetGSCNAction(IN P_ADAPTER_T prAdapter,
		     IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_CMD_SET_PSCAN_ENABLE prCmdPscnAction;

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(SCN, ERROR, "Adapter not ready: ACPI=%d, Radio=%d\n",
				   prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	if (u4SetBufferLen != sizeof(CMD_SET_PSCAN_ENABLE)) {
		DBGLOG(SCN, ERROR, "u4SetBufferLen != sizeof(CMD_SET_PSCAN_ENABLE)\n");
		return WLAN_STATUS_INVALID_LENGTH;
	} else if (pvSetBuffer == NULL) {
		DBGLOG(SCN, ERROR, "pvSetBuffer == NULL\n");
		return WLAN_STATUS_INVALID_DATA;
	}

	if (prAdapter->fgIsRadioOff) {
		DBGLOG(SCN, ERROR, "Radio off: ACPI=%d, Radio=%d\n",
				   prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_SUCCESS;
	}

	prCmdPscnAction = (P_CMD_SET_PSCAN_ENABLE) pvSetBuffer;

	if (prCmdPscnAction) {
		DBGLOG(SCN, TRACE, "ucPscanAct=[%d]\n", prCmdPscnAction->ucPscanAct);
		if (prCmdPscnAction->ucPscanAct == PSCAN_ACT_ENABLE) {
			prAdapter->rWifiVar.rScanInfo.fgGScnAction = TRUE;
			scnPSCNFsm(prAdapter, PSCN_SCANNING);
		} else if (prCmdPscnAction->ucPscanAct == PSCAN_ACT_DISABLE) {
			scnCombineParamsIntoPSCN(prAdapter, NULL, NULL, NULL, NULL, FALSE, FALSE, TRUE);
			if (prAdapter->rWifiVar.rScanInfo.prPscnParam->fgNLOScnEnable
				|| prAdapter->rWifiVar.rScanInfo.prPscnParam->fgBatchScnEnable)
				scnPSCNFsm(prAdapter, PSCN_RESET); /* in case there is any PSCN */
			else
				scnPSCNFsm(prAdapter, PSCN_IDLE);
		}
	}

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to configure GScan PARAMs
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
wlanoidSetGSCNParam(IN P_ADAPTER_T prAdapter,
		    IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_PARAM_WIFI_GSCAN_CMD_PARAMS prCmdGscnParam;

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(SCN, ERROR, "Adapter not ready: ACPI=%d, Radio=%d\n",
				   prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}
	if (u4SetBufferLen != sizeof(PARAM_WIFI_GSCAN_CMD_PARAMS)) {
		DBGLOG(SCN, ERROR, "u4SetBufferLen != sizeof(PARAM_WIFI_GSCAN_CMD_PARAMS)\n");
		return WLAN_STATUS_INVALID_LENGTH;
	} else if (pvSetBuffer == NULL) {
		DBGLOG(SCN, ERROR, "pvSetBuffer == NULL\n");
		return WLAN_STATUS_INVALID_DATA;
	}
	if (prAdapter->fgIsRadioOff) {
		DBGLOG(SCN, ERROR, "Radio off: ACPI=%d, Radio=%d\n",
				   prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_SUCCESS;
	}

	prCmdGscnParam = (P_PARAM_WIFI_GSCAN_CMD_PARAMS) pvSetBuffer;

	DBGLOG(SCN, TRACE, "prCmdGscnParam: base_period[%u], num_buckets[%u] band[%d] num_channels[%u]\n",
	prCmdGscnParam->base_period, prCmdGscnParam->num_buckets,
	prCmdGscnParam->buckets[0].band, prCmdGscnParam->buckets[0].num_channels);

	if (scnSetGSCNParam(prAdapter, prCmdGscnParam) == TRUE)
		return WLAN_STATUS_SUCCESS;
	else
		return WLAN_STATUS_FAILURE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to configure GScan PARAMs
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
wlanoidSetGSCNConfig(IN P_ADAPTER_T prAdapter,
		     IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_PARAM_WIFI_GSCAN_CMD_PARAMS prCmdGscnConfigParam;
	CMD_GSCN_SCN_COFIG_T rCmdGscnConfig;

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(SCN, ERROR, "Adapter not ready: ACPI=%d, Radio=%d\n",
				   prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}
	if (u4SetBufferLen != sizeof(PARAM_WIFI_GSCAN_CMD_PARAMS)) {
		DBGLOG(SCN, ERROR, "u4SetBufferLen != sizeof(PARAM_WIFI_GSCAN_CMD_PARAMS)\n");
		return WLAN_STATUS_INVALID_LENGTH;
	} else if (pvSetBuffer == NULL) {
		DBGLOG(SCN, ERROR, "pvSetBuffer == NULL\n");
		return WLAN_STATUS_INVALID_DATA;
	}
	if (prAdapter->fgIsRadioOff) {
		DBGLOG(SCN, ERROR, "Radio off: ACPI=%d, Radio=%d\n",
				   prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_SUCCESS;
	}

	prCmdGscnConfigParam = (P_PARAM_WIFI_GSCAN_CMD_PARAMS) pvSetBuffer;
	kalMemZero(&rCmdGscnConfig, sizeof(CMD_GSCN_SCN_COFIG_T));

	if (prCmdGscnConfigParam) {
		rCmdGscnConfig.u4BufferThreshold = prCmdGscnConfigParam->report_threshold_percent;
		rCmdGscnConfig.ucNumApPerScn = prCmdGscnConfigParam->max_ap_per_scan;
		rCmdGscnConfig.u4NumScnToCache = prCmdGscnConfigParam->report_threshold_num_scans;
	}
	DBGLOG(SCN, TRACE, "rCmdGscnScnConfig: threshold_percent[%d] max_ap_per_scan[%d] num_scans[%d]\n",
			   rCmdGscnConfig.u4BufferThreshold,
			   rCmdGscnConfig.ucNumApPerScn, rCmdGscnConfig.u4NumScnToCache);

	if (scnSetGSCNConfig(prAdapter, &rCmdGscnConfig) == TRUE)
		return WLAN_STATUS_SUCCESS;
	else
		return WLAN_STATUS_FAILURE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to get a GScan result
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
wlanoidGetGSCNResult(IN P_ADAPTER_T prAdapter,
		     IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_PARAM_WIFI_GSCAN_GET_RESULT_PARAMS prGetGscnScnResultParm;
	CMD_GET_GSCAN_RESULT_T rGetGscnScnResultCmd;

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(SCN, ERROR, "Adapter not ready: ACPI=%d, Radio=%d\n",
				   prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	if (u4SetBufferLen != sizeof(PARAM_WIFI_GSCAN_GET_RESULT_PARAMS)) {
		DBGLOG(SCN, ERROR, "u4SetBufferLen != sizeof(PARAM_WIFI_GSCAN_GET_RESULT_PARAMS))\n");
		return WLAN_STATUS_INVALID_LENGTH;
	} else if (pvSetBuffer == NULL) {
		DBGLOG(SCN, ERROR, "pvSetBuffer == NULL\n");
		return WLAN_STATUS_INVALID_DATA;
	}

	if (prAdapter->fgIsRadioOff) {
		DBGLOG(SCN, ERROR, "Radio off: ACPI=D%d, Radio=%d\n",
				   prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_SUCCESS;
	}

	prGetGscnScnResultParm = (P_PARAM_WIFI_GSCAN_GET_RESULT_PARAMS) pvSetBuffer;
	kalMemZero(&rGetGscnScnResultCmd, sizeof(CMD_GET_GSCAN_RESULT_T));

	if (prGetGscnScnResultParm) {
		rGetGscnScnResultCmd.u4Num = prGetGscnScnResultParm->get_num;
		rGetGscnScnResultCmd.ucFlush = prGetGscnScnResultParm->flush;
		rGetGscnScnResultCmd.ucVersion = PSCAN_VERSION;
	}

	if (scnFsmGetGSCNResult(prAdapter, &rGetGscnScnResultCmd, pu4SetInfoLen) == TRUE)
		return WLAN_STATUS_SUCCESS;
	else
		return WLAN_STATUS_FAILURE;
}
#endif /* CFG_SUPPORT_GSCN */


#if CFG_SUPPORT_HOTSPOT_2_0
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
	DBGLOG(OID, LOUD, "\r\n");

	if (u4SetBufferLen == 0)
		return WLAN_STATUS_INVALID_LENGTH;

	*pu4SetInfoLen = u4SetBufferLen;

	prHS20IndicationIe = (P_IE_HS20_INDICATION_T) pvSetBuffer;

	prAdapter->prGlueInfo->ucHotspotConfig = prHS20IndicationIe->ucHotspotConfig;
	prAdapter->prGlueInfo->fgConnectHS20AP = TRUE;

	DBGLOG(SEC, TRACE, "HS20 IE sz %u\n", u4SetBufferLen);

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

	rWlanStatus = hs20SetBssidPool(prAdapter, pvSetBuffer, NETWORK_TYPE_AIS_INDEX);

	return rWlanStatus;
}				/* end of wlanoidSendHS20GASRequest() */

#endif

#if CFG_SUPPORT_ROAMING_ENC
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
wlanoidSetRoamingInfo(IN P_ADAPTER_T prAdapter,
		      IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	CMD_ROAMING_INFO_T *prCmdRoamingInfo;

	DEBUGFUNC("wlanoidSetRoamingInfo");
	DBGLOG(OID, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(CMD_ROAMING_INFO_T);

	if (u4SetBufferLen < sizeof(CMD_ROAMING_INFO_T))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);
	prCmdRoamingInfo = (CMD_ROAMING_INFO_T *) pvSetBuffer;

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_SET_ROAMING_INFO,
				   TRUE,
				   FALSE,
				   TRUE,
				   nicCmdEventSetCommon,
				   nicOidCmdTimeoutCommon,
				   sizeof(CMD_ROAMING_INFO_T), (PUINT_8) prCmdRoamingInfo, NULL, 0);
}
#endif /* CFG_SUPPORT_ROAMING_ENC */

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
	DBGLOG(OID, LOUD, "\n");

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
		DBGLOG(OID, INFO, "Chip config Msg Size %u is not valid (set)\n", rCmdChipConfig.u2MsgSize);
		rCmdChipConfig.u2MsgSize = CHIP_CONFIG_RESP_SIZE;
	}
	kalMemCopy(rCmdChipConfig.aucCmd, prChipConfigInfo->aucCmd, rCmdChipConfig.u2MsgSize);

	DBGLOG(OID, TRACE, "rCmdChipConfig.aucCmd=%s\n", rCmdChipConfig.aucCmd);
#if 1
	rWlanStatus = wlanSendSetQueryCmd(prAdapter,
					  CMD_ID_CHIP_CONFIG,
					  TRUE,
					  FALSE,
					  TRUE,
					  nicCmdEventSetCommon,
					  nicOidCmdTimeoutCommon,
					  sizeof(CMD_CHIP_CONFIG_T),
					  (PUINT_8) &rCmdChipConfig, pvSetBuffer, u4SetBufferLen);
#endif
	return rWlanStatus;
}				/* wlanoidSetChipConfig */
#if CFG_SUPPORT_P2P_ECSA
WLAN_STATUS
wlanoidSetECSAConfig(IN P_ADAPTER_T prAdapter,
		     IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_PARAM_ECSA_CONFIG_STRUCT_T prCsInfo;
	CMD_SET_ECSA_PARAM rCmdCsConfig;
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	prCsInfo = (P_PARAM_ECSA_CONFIG_STRUCT_T) pvSetBuffer;
	kalMemZero(&rCmdCsConfig, sizeof(rCmdCsConfig));

	rCmdCsConfig.ucNetTypeIndex = NETWORK_TYPE_P2P_INDEX,
	rCmdCsConfig.ucOperatingClass = prCsInfo->op_class;
	rCmdCsConfig.ucPrimaryChannel = prCsInfo->channel;
	rCmdCsConfig.ucRfSco = prCsInfo->sco;
	rCmdCsConfig.ucSwitchMode = prCsInfo->mode;
	rCmdCsConfig.ucSwitchTotalCount = prCsInfo->count;

	DBGLOG(OID, INFO, "index:op_class:channel:sco:mode:count %d:%d:%d:%d:%d:%d\n",
			rCmdCsConfig.ucNetTypeIndex,
			rCmdCsConfig.ucOperatingClass,
			rCmdCsConfig.ucPrimaryChannel,
			rCmdCsConfig.ucRfSco,
			rCmdCsConfig.ucSwitchMode,
			rCmdCsConfig.ucSwitchTotalCount);
	rWlanStatus = wlanSendSetQueryCmd(prAdapter,
					  CMD_ID_SET_ECSA_PARAM,
					  TRUE,
					  FALSE,
					  TRUE,
					  nicCmdEventSetCommon,
					  nicOidCmdTimeoutCommon,
					  sizeof(CMD_CHIP_CONFIG_T),
					  (PUINT_8)&rCmdCsConfig,
					  pvSetBuffer, u4SetBufferLen);
	return rWlanStatus;
}
#endif
WLAN_STATUS
wlanoidSetWfdDebugMode(IN P_ADAPTER_T prAdapter,
		       IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_CMD_WFD_DEBUG_MODE_INFO_T prCmdWfdDebugModeInfo;

	DEBUGFUNC("wlanoidSetWFDDebugMode");
	DBGLOG(OID, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(CMD_WFD_DEBUG_MODE_INFO_T);

	if (u4SetBufferLen < sizeof(CMD_WFD_DEBUG_MODE_INFO_T))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);
	prCmdWfdDebugModeInfo = (CMD_WFD_DEBUG_MODE_INFO_T *) pvSetBuffer;

	DBGLOG(OID, INFO, "New WFD Debug: %d mode and period=0x%x\n", prCmdWfdDebugModeInfo->ucDebugMode,
						prCmdWfdDebugModeInfo->u2PeriodInteval);

	prAdapter->rWifiVar.prP2pFsmInfo->rWfdDebugSetting.ucWfdDebugMode = (UINT_8) prCmdWfdDebugModeInfo->ucDebugMode;
	prAdapter->rWifiVar.prP2pFsmInfo->rWfdDebugSetting.u2WfdSNShowPeiroid =
	    (UINT_16) prCmdWfdDebugModeInfo->u2PeriodInteval;

	return WLAN_STATUS_SUCCESS;
}				/*wlanoidSetWfdDebugMode */


#if (CFG_SUPPORT_TXR_ENC == 1)
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
wlanoidSetTxRateInfo(
	IN  P_ADAPTER_T prAdapter,
	IN  PVOID       pvSetBuffer,
	IN  UINT_32     u4SetBufferLen,
	OUT PUINT_32    pu4SetInfoLen)
{
	CMD_RLM_INFO_T *prCmdTxRInfo;

	DEBUGFUNC("wlanoidSetTxRateInfo");
	DBGLOG(OID, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(CMD_RLM_INFO_T);

	if (u4SetBufferLen < sizeof(CMD_RLM_INFO_T))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);
	prCmdTxRInfo = (CMD_RLM_INFO_T *)pvSetBuffer;

	DBGLOG(OID, INFO, "<tar_cmd> command = %u %u %u %u %d %u %u\n",
		prCmdTxRInfo->u4Version,
		prCmdTxRInfo->fgIsErrRatioEnhanceApplied,
		prCmdTxRInfo->ucErrRatio2LimitMinRate,
		prCmdTxRInfo->ucMinLegacyRateIdx,
		prCmdTxRInfo->cMinRssiThreshold,
		prCmdTxRInfo->fgIsRtsApplied,
		prCmdTxRInfo->ucRecoverTime));

	return wlanSendSetQueryCmd(prAdapter,
		CMD_ID_TX_AR_ERR_CONFIG,
		TRUE,
		FALSE,
		TRUE,
		nicCmdEventSetCommon,
		nicOidCmdTimeoutCommon,
		sizeof(CMD_RLM_INFO_T),
		(PUINT_8)prCmdTxRInfo,
		NULL,
		0
		);
}
#endif /* CFG_SUPPORT_TXR_ENC */


WLAN_STATUS
wlanoidSetAlwaysScan(
IN  P_ADAPTER_T prAdapter,
IN  PVOID       pvSetBuffer,
IN  UINT_32     u4SetBufferLen,
OUT PUINT_32    pu4SetInfoLen
)
{
	WLAN_STATUS rStatus;
	UINT_8 u8AlwaysScan;

	DEBUGFUNC("wlanoidSetAlwaysScan");

	if (!prAdapter || !pvSetBuffer)
		return WLAN_STATUS_INVALID_DATA;

	u8AlwaysScan = *(PUINT_8)pvSetBuffer - '0';

	if ((u8AlwaysScan != 0) && (u8AlwaysScan != 1))
		return WLAN_STATUS_INVALID_DATA;

	rStatus = wlanSendSetQueryCmd(
				prAdapter,                  /* prAdapter */
				CMD_ID_SET_ALWAYS_SCAN_PARAM,/* ucCID */
				TRUE,                       /* fgSetQuery */
				FALSE,                      /* fgNeedResp */
				TRUE,                       /* fgIsOid */
				nicCmdEventSetCommon,		/* pfCmdDoneHandler*/
				nicOidCmdTimeoutCommon,		/* pfCmdTimeoutHandler */
				sizeof(u8AlwaysScan),		/* u4SetQueryInfoLen */
				(PUINT_8)&u8AlwaysScan,      /* pucInfoBuffer */
				NULL,                       /* pvSetQueryBuffer */
				0                           /* u4SetQueryBufferLen */
				);
	DBGLOG(OID, INFO, "u8AlwaysScan %d rStatus %x\n", u8AlwaysScan, rStatus);

	ASSERT(rStatus == WLAN_STATUS_PENDING);

	return rStatus;

}

#if CFG_SUPPORT_OSHARE
WLAN_STATUS
wlanoidSetOshareMode(
IN  P_ADAPTER_T prAdapter,
IN  PVOID       pvSetBuffer,
IN  UINT_32     u4SetBufferLen,
OUT PUINT_32    pu4SetInfoLen
)
{
	if (!prAdapter || !pvSetBuffer)
		return WLAN_STATUS_INVALID_DATA;

	DBGLOG(OID, TRACE, "wlanoidSetOshareMode\n");

	return wlanSendSetQueryCmd(
			prAdapter,/* prAdapter */
			CMD_ID_SET_OSHARE_MODE,/* ucCID */
			TRUE,/* fgSetQuery */
			FALSE,/* fgNeedResp */
			TRUE,/* fgIsOid */
			nicCmdEventSetCommon,/* pfCmdDoneHandler*/
			nicOidCmdTimeoutCommon,/* pfCmdTimeoutHandler */
			u4SetBufferLen,/* u4SetQueryInfoLen */
			(PUINT_8) pvSetBuffer,/* pucInfoBuffer */
			NULL,/* pvSetQueryBuffer */
			0/* u4SetQueryBufferLen */
			);
}
#endif

WLAN_STATUS
wlanoidNotifyFwSuspend(IN P_ADAPTER_T prAdapter,
		       IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	WIFI_SYSTEM_SUSPEND_CMD_T rSuspendCmd;

	if (!prAdapter || !pvSetBuffer)
		return WLAN_STATUS_INVALID_DATA;

	rSuspendCmd.fgIsSystemSuspend = *(PBOOLEAN)pvSetBuffer;
	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_SET_SYSTEM_SUSPEND,
				   TRUE,
				   FALSE,
				   TRUE,
				   nicCmdEventSetCommon,
				   nicOidCmdTimeoutCommon,
				   sizeof(BOOLEAN),
				   (PUINT_8)&rSuspendCmd,
				   NULL,
				   0);
}

#if CFG_SUPPORT_TDLS
WLAN_STATUS
wlanoidDisableTdlsPs(IN P_ADAPTER_T prAdapter,
			 IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	struct CMD_TDLS_PS_T rTdlsPs;

	if (!prAdapter || !pvSetBuffer)
		return WLAN_STATUS_INVALID_DATA;

	rTdlsPs.ucIsEnablePs = *(PUINT_8)pvSetBuffer - '0';
	DBGLOG(OID, INFO, "enable tdls ps %d\n", rTdlsPs.ucIsEnablePs);
	return wlanSendSetQueryCmd(prAdapter,
						CMD_ID_TDLS_PS,
						TRUE,
						FALSE,
						TRUE,
						nicCmdEventSetCommon,
						nicOidCmdTimeoutCommon,
						sizeof(rTdlsPs),
						(PUINT_8)&rTdlsPs,
						NULL,
						0);
}
#endif

WLAN_STATUS
wlanoidPacketKeepAlive(IN P_ADAPTER_T prAdapter,
		 IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	P_PARAM_PACKET_KEEPALIVE_T prPacket;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	if (u4SetBufferLen)
		ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(PARAM_PACKET_KEEPALIVE_T);

	/* Check for query buffer length */
	if (u4SetBufferLen < *pu4SetInfoLen) {
		DBGLOG(OID, WARN, "Too short length %u\n", u4SetBufferLen);
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}

	prPacket = (P_PARAM_PACKET_KEEPALIVE_T)kalMemAlloc(sizeof(PARAM_PACKET_KEEPALIVE_T), VIR_MEM_TYPE);
	if (!prPacket) {
		DBGLOG(OID, ERROR, "Can not alloc memory for PARAM_PACKET_KEEPALIVE_T\n");
		return -ENOMEM;
	}
	kalMemCopy(prPacket, pvSetBuffer, sizeof(PARAM_PACKET_KEEPALIVE_T));

	DBGLOG(OID, INFO, "enable=%d, index=%d\r\n", prPacket->enable, prPacket->index);

	rStatus = wlanSendSetQueryCmd(prAdapter,
			   CMD_ID_WFC_KEEP_ALIVE,
			   TRUE,
			   FALSE,
			   TRUE,
			   nicCmdEventSetCommon,
			   nicOidCmdTimeoutCommon,
			   sizeof(PARAM_PACKET_KEEPALIVE_T), (PUINT_8)prPacket, NULL, 0);
	kalMemFree(prPacket, VIR_MEM_TYPE, sizeof(PARAM_PACKET_KEEPALIVE_T));
	return rStatus;
}

#ifdef FW_CFG_SUPPORT
/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query fw cfg info
*
* \param[in]  pvAdapter        Pointer to the Adapter structure.
* \param[out] pvQueryBuffer    A pointer to the buffer that holds the result of
*                              the query.
* \param[in]  u4QueryBufferLen The length of the query buffer.
* \param[out] pu4QueryInfoLen  If the call is successful, returns the number of
*                              bytes written into the query buffer. If the call
*                              failed due to invalid length of the query buffer,
*                              returns the amount of storage needed.
*
* \retval WLAN_STATUS_PENDING
* \retval WLAN_STATUS_FAILURE
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanoidQueryCfgRead(IN P_ADAPTER_T prAdapter,
			   IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	struct _CMD_HEADER_T *prCmdV1Header = (struct _CMD_HEADER_T *)pvQueryBuffer;
	struct _CMD_HEADER_T cmdV1Header;
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);

	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(struct _CMD_HEADER_T);

	if (u4QueryBufferLen < sizeof(struct _CMD_HEADER_T))
		return WLAN_STATUS_INVALID_LENGTH;

	kalMemCopy(&cmdV1Header, prCmdV1Header, sizeof(struct _CMD_HEADER_T));
	rStatus = wlanSendSetQueryCmd(
			prAdapter,
			CMD_ID_GET_SET_CUSTOMER_CFG,
			FALSE,
			TRUE,
			TRUE,
			nicCmdEventQueryCfgRead,
			nicCmdTimeoutCommon,
			sizeof(struct _CMD_HEADER_T),
			(PUINT_8) &cmdV1Header,
			pvQueryBuffer,
			u4QueryBufferLen);
	return rStatus;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to set fw cfg info
 *
 * \param[in] prAdapter      Pointer to the Adapter structure.
 * \param[in] pvSetBuffer    Pointer to the buffer that holds the data to be
 *                           set.
 * \param[in] u4SetBufferLen The length of the set buffer.
 * \param[out] pu4SetInfoLen If the call is successful, returns the number of
 *                           bytes read from the set buffer. If the call failed
 *                           due to invalid length of the set buffer, returns
 *                           the amount of storage needed.
 *
 * \retval WLAN_STATUS_INVALID_DATA
 * \retval WLAN_STATUS_SUCCESS
 */
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanoidSetFwParam(IN P_ADAPTER_T prAdapter,
			      IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen,
			      OUT PUINT_32 pu4SetInfoLen)
{
	ASSERT(prAdapter);

	if (!pvSetBuffer || !u4SetBufferLen) {
		DBGLOG(OID, ERROR, "Buffer is NULL\n");
		return WLAN_STATUS_INVALID_DATA;
	}
	DBGLOG(OID, INFO, "Fw Params: %s\n", (PUINT_8)pvSetBuffer);
	return wlanFwCfgParse(prAdapter, (PUINT_8)pvSetBuffer);
}
#endif

WLAN_STATUS
wlanoidSetDrvRoamingPolicy(IN P_ADAPTER_T prAdapter,
			 IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	UINT_32 u4RoamingPoily;
	P_ROAMING_INFO_T prRoamingFsmInfo;
	P_CONNECTION_SETTINGS_T prConnSettings;
	UINT_32 u4CurConPolicy;

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);

	u4RoamingPoily = *(PUINT_32)pvSetBuffer;

	prRoamingFsmInfo = (P_ROAMING_INFO_T) &(prAdapter->rWifiVar.rRoamingInfo);

	prConnSettings = (P_CONNECTION_SETTINGS_T) &prAdapter->rWifiVar.rConnSettings;
	u4CurConPolicy = prConnSettings->eConnectionPolicy;

	if (u4RoamingPoily == 1) {
		if (((prAdapter->rWifiVar.rAisFsmInfo.eCurrentState == AIS_STATE_NORMAL_TR)
			|| (prAdapter->rWifiVar.rAisFsmInfo.eCurrentState == AIS_STATE_ONLINE_SCAN))
			&& (prRoamingFsmInfo->eCurrentState == ROAMING_STATE_IDLE))
			roamingFsmRunEventStart(prAdapter);

		/*Change Connect by any , avoid to connect by BSSID on roaming or beacon timeout!*/
		prConnSettings->eConnectionPolicy = CONNECT_BY_SSID_ANY;

	} else {
		if (prRoamingFsmInfo->eCurrentState != ROAMING_STATE_IDLE)
			roamingFsmRunEventAbort(prAdapter);
	}
	prRoamingFsmInfo->DrvRoamingAllow = u4RoamingPoily;

	DBGLOG(REQ, INFO, "wlanoidSetDrvRoamingPolicy, RoamingPoily= %d, conn policy= [%d] -> [%d]\n"
		, u4RoamingPoily, u4CurConPolicy, prRoamingFsmInfo->DrvRoamingAllow);

	return WLAN_STATUS_SUCCESS;
}
#if CFG_SUPPORT_EMI_DEBUG
WLAN_STATUS
wlanoidSetEnableDumpEMILog(IN P_ADAPTER_T prAdapter,
				IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	P_CMD_DRIVER_DUMP_EMI_LOG_T pDriverDumpEMI;

	if (!prAdapter || !pvSetBuffer)
		return WLAN_STATUS_INVALID_DATA;

	pDriverDumpEMI = (P_CMD_DRIVER_DUMP_EMI_LOG_T)pvSetBuffer;
	DBGLOG(OID, TRACE, "%s: enable:0x%x", __func__,
	pDriverDumpEMI->fgIsDriverDumpEmiLogEnable);

	wlanSendSetQueryCmd(prAdapter,
						CMD_ID_DRIVER_DUMP_EMI_LOG,
						TRUE,
						FALSE,
						FALSE,
						NULL,
						nicOidCmdTimeoutCommon,
						sizeof(CMD_DRIVER_DUMP_EMI_LOG_T),
						(PUINT_8)pDriverDumpEMI,
						NULL,
						0);
	return WLAN_STATUS_SUCCESS;
}
#endif

WLAN_STATUS
wlanoidUpdateFtIes(P_ADAPTER_T prAdapter, PVOID pvSetBuffer, UINT_32 u4SetBufferLen,
						  PUINT_32 pu4SetInfoLen)
{
	struct FT_IES *prFtIes = NULL;
	UINT_32 u4IeLen = 0;
	PUINT_8 pucIEStart = NULL;
	struct cfg80211_update_ft_ies_params *ftie = NULL;
	P_STA_RECORD_T prStaRec = NULL;
	struct MSG_SAA_FT_CONTINUE *prFtContinueMsg = NULL;

	if (!pvSetBuffer || u4SetBufferLen == 0) {
		DBGLOG(OID, ERROR, "pvSetBuffer is Null %d, Buffer Len %u\n", !pvSetBuffer, u4SetBufferLen);
		return WLAN_STATUS_INVALID_DATA;
	}
	prStaRec = prAdapter->rWifiVar.rAisFsmInfo.prTargetStaRec;
	ftie = (struct cfg80211_update_ft_ies_params *)pvSetBuffer;
	prFtIes = &prAdapter->prGlueInfo->rFtIeForTx;
	if (ftie->ie_len == 0) {
		DBGLOG(OID, WARN, "FT Ies length is 0\n");
		return WLAN_STATUS_SUCCESS;
	}
	if (prFtIes->u4IeLength != ftie->ie_len) {
		kalMemFree(prFtIes->pucIEBuf, VIR_MEM_TYPE, prFtIes->u4IeLength);
		prFtIes->pucIEBuf = kalMemAlloc(ftie->ie_len, VIR_MEM_TYPE);
		prFtIes->u4IeLength = ftie->ie_len;
	}

	if (!prFtIes->pucIEBuf) {
		DBGLOG(OID, ERROR, "FT: memory allocation failed\n");
		return WLAN_STATUS_FAILURE;
	}

	pucIEStart = prFtIes->pucIEBuf;
	u4IeLen = prFtIes->u4IeLength;
	prFtIes->u2MDID = ftie->md;
	prFtIes->prFTIE = NULL;
	prFtIes->prMDIE = NULL;
	prFtIes->prRsnIE = NULL;
	prFtIes->prTIE = NULL;
	if (u4IeLen)
		kalMemCopy(pucIEStart, ftie->ie, u4IeLen);
	while (u4IeLen >= 2) {
		UINT_32 u4InfoElemLen = IE_SIZE(pucIEStart);

		if (u4InfoElemLen > u4IeLen)
			break;
		switch (pucIEStart[0]) {
		case ELEM_ID_MOBILITY_DOMAIN:
			prFtIes->prMDIE = (struct IE_MOBILITY_DOMAIN_T *)pucIEStart;
			break;
		case ELEM_ID_FAST_TRANSITION:
			prFtIes->prFTIE = (struct IE_FAST_TRANSITION_T *)pucIEStart;
			break;
		case ELEM_ID_RESOURCE_INFO_CONTAINER:
			break;
		case ELEM_ID_TIMEOUT_INTERVAL:
			prFtIes->prTIE = (IE_TIMEOUT_INTERVAL_T *)pucIEStart;
			break;
		case ELEM_ID_RSN:
			prFtIes->prRsnIE = (P_RSN_INFO_ELEM_T)pucIEStart;
			break;
		}
		u4IeLen -= u4InfoElemLen;
		pucIEStart += u4InfoElemLen;
	}
	DBGLOG(OID, INFO, "MdId %d IesLen %u, MDIE %d FTIE %d RSN %d TIE %d\n", ftie->md, prFtIes->u4IeLength,
		!!prFtIes->prMDIE, !!prFtIes->prFTIE, !!prFtIes->prRsnIE, !!prFtIes->prTIE);
	/* check if SAA is waiting to send Reassoc req */
	if (!prStaRec || prStaRec->ucAuthTranNum != AUTH_TRANSACTION_SEQ_2 ||
		!prStaRec->fgIsReAssoc || prStaRec->ucStaState != STA_STATE_1)
		return WLAN_STATUS_SUCCESS;

	prFtContinueMsg = (struct MSG_SAA_FT_CONTINUE *) cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
					sizeof(struct MSG_SAA_FT_CONTINUE));
	if (!prFtContinueMsg) {
		DBGLOG(OID, WARN, "failed to allocate Join Req Msg\n");
		return WLAN_STATUS_FAILURE;
	}
	prFtContinueMsg->rMsgHdr.eMsgId = MID_OID_SAA_FSM_CONTINUE;
	prFtContinueMsg->prStaRec = prStaRec;
	/* ToDo: for Resource Request Protocol, we need to check if RIC request is included. */
	if (prFtIes->prMDIE && (prFtIes->prMDIE->ucBitMap & BIT(1)))
		prFtContinueMsg->fgFTRicRequest = TRUE;
	else
		prFtContinueMsg->fgFTRicRequest = FALSE;
	DBGLOG(OID, INFO, "continue to do auth/assoc, Ft Request %d\n", prFtContinueMsg->fgFTRicRequest);
	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prFtContinueMsg, MSG_SEND_METHOD_BUF);
	return WLAN_STATUS_SUCCESS;
}

WLAN_STATUS
wlanoidSendNeighborRequest(P_ADAPTER_T prAdapter, PVOID pvSetBuffer, UINT_32 u4SetBufferLen,
									   PUINT_32 pu4SetInfoLen)
{
	struct SUB_ELEMENT_LIST *prSSIDIE = NULL;
	P_BSS_INFO_T prAisBssInfo = NULL;
	UINT_8 ucSSIDIELen = 0;
	PUINT_8 pucSSID = (PUINT_8)pvSetBuffer;

	if (!prAdapter)
		return WLAN_STATUS_INVALID_DATA;
	prAisBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);
	if (prAisBssInfo->eConnectionState != PARAM_MEDIA_STATE_CONNECTED) {
		DBGLOG(OID, ERROR, "didn't connected any Access Point\n");
		return WLAN_STATUS_FAILURE;
	}
	if (u4SetBufferLen == 0 || !pucSSID) {
		rlmTxNeighborReportRequest(prAdapter, prAisBssInfo->prStaRecOfAP, NULL);
		return WLAN_STATUS_SUCCESS;
	}

	ucSSIDIELen = (UINT_8)(u4SetBufferLen + sizeof(*prSSIDIE));
	prSSIDIE = kalMemAlloc(ucSSIDIELen, PHY_MEM_TYPE);
	if (!prSSIDIE) {
		DBGLOG(OID, ERROR, "No Memory\n");
		return WLAN_STATUS_FAILURE;
	}
	prSSIDIE->prNext = NULL;
	prSSIDIE->rSubIE.ucSubID = ELEM_ID_SSID;
	prSSIDIE->rSubIE.ucLength = (UINT_8)u4SetBufferLen;
	kalMemCopy(&prSSIDIE->rSubIE.aucOptInfo[0], pucSSID, (UINT_8)u4SetBufferLen);
	DBGLOG(OID, INFO, "Send Neighbor Request, SSID=%s\n", HIDE(pucSSID));
	rlmTxNeighborReportRequest(prAdapter, prAisBssInfo->prStaRecOfAP, prSSIDIE);
	kalMemFree(prSSIDIE, PHY_MEM_TYPE, ucSSIDIELen);
	return WLAN_STATUS_SUCCESS;
}

WLAN_STATUS
wlanoidSync11kCapbilities(P_ADAPTER_T prAdapter, PVOID pvSetBuffer, UINT_32 u4SetBufferLen,
								   PUINT_32 pu4SetInfoLen)
{
	struct CMD_SET_RRM_CAPABILITY rCmdRrmCapa;

	kalMemZero(&rCmdRrmCapa, sizeof(rCmdRrmCapa));
	rCmdRrmCapa.ucDot11RadioMeasurementEnabled = 1;
	rlmFillRrmCapa(&rCmdRrmCapa.aucCapabilities[0]);
	return wlanSendSetQueryCmd(prAdapter,
					  CMD_ID_SET_RRM_CAPABILITY,
					  TRUE,
					  FALSE,
					  TRUE,
					  nicCmdEventSetCommon,
					  nicOidCmdTimeoutCommon,
					  sizeof(struct CMD_SET_RRM_CAPABILITY),
					  (PUINT_8)&rCmdRrmCapa, pvSetBuffer, u4SetBufferLen);
}

WLAN_STATUS
wlanoidSendBTMQuery(P_ADAPTER_T prAdapter, PVOID pvSetBuffer, UINT_32 u4SetBufferLen,
							   PUINT_32 pu4SetInfoLen)
{
	P_STA_RECORD_T prStaRec = prAdapter->rWifiVar.rAisFsmInfo.prTargetStaRec;
	struct BSS_TRANSITION_MGT_PARAM_T *prBtmMgt = NULL;

	if (prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX].eConnectionState !=
		PARAM_MEDIA_STATE_CONNECTED || !prStaRec) {
		DBGLOG(OID, INFO, "Didn't Connect, Target StaRec is NULL %d\n", !prStaRec);
		return WLAN_STATUS_FAILURE;
	}
	if (!prStaRec->fgSupportBTM) {
		DBGLOG(OID, INFO, "Target BSS didn't support Bss Transition Management\n");
		return WLAN_STATUS_FAILURE;
	}
	prBtmMgt = &prAdapter->rWifiVar.rAisSpecificBssInfo.rBTMParam;
	prBtmMgt->ucDialogToken = wnmGetBtmToken();
	prBtmMgt->ucQueryReason = pvSetBuffer ? (*(PUINT_8)pvSetBuffer - '0'):BSS_TRANSITION_LOW_RSSI;
	DBGLOG(OID, INFO, "Send BssTransitionManagementQuery, Reason %d\n", prBtmMgt->ucQueryReason);
	wnmSendBTMQueryFrame(prAdapter, prStaRec);
	return WLAN_STATUS_SUCCESS;
}

/*
 * This func is mainly from bionic's strtok.c
 */
static CHAR *strtok_r(CHAR *s, const CHAR *delim, CHAR **last)
{
	char *spanp;
	int c, sc;
	char *tok;


	if (s == NULL) {
		s = *last;
		if (s == 0)
			return NULL;
	}
cont:
	c = *s++;
	for (spanp = (char *)delim; (sc = *spanp++) != 0;) {
		if (c == sc)
			goto cont;
	}

	if (c == 0) {		/* no non-delimiter characters */
		*last = NULL;
		return NULL;
	}
	tok = s - 1;

	for (;;) {
		c = *s++;
		spanp = (char *)delim;
		do {
			sc = *spanp++;
			if (sc == c) {
				if (c == 0)
					s = NULL;
				else
					s[-1] = 0;
				*last = s;
				return tok;
			}
		} while (sc != 0);
	}
}

WLAN_STATUS wlanoidTspecOperation(P_ADAPTER_T prAdapter, PVOID pvBuffer, UINT_32 u4BufferLen,
										   PUINT_32 pu4InfoLen)
{
	P_PARAM_QOS_TSPEC prTspecParam = NULL;
	struct MSG_TS_OPERATE *prMsgTsOperate = NULL;
	PUINT_8 pucCmd = (PUINT_8)pvBuffer;
	PUINT_8 pucSavedPtr = NULL;
	PUINT_8 pucItem = NULL;
	UINT_32 u4Ret = 1;
	UINT_8 ucApsdSetting = 2; /* 0: legacy; 1: u-apsd; 2: not set yet */
	enum TSPEC_OP_CODE eTsOp;

#if !CFG_SUPPORT_WMM_AC
	DBGLOG(OID, INFO, "WMM AC is not supported\n");
	return WLAN_STATUS_FAILURE;
#endif
	if (kalStrniCmp(pucCmd, "dumpts", 6) == 0) {
		*pu4InfoLen = kalSnprintf(pucCmd, u4BufferLen, "%s", "\nAll Active Tspecs:\n");
		u4BufferLen -= *pu4InfoLen;
		pucCmd += *pu4InfoLen;
		*pu4InfoLen += wmmDumpActiveTspecs(prAdapter, pucCmd, u4BufferLen);
		return WLAN_STATUS_SUCCESS;
	}

	if (kalStrniCmp(pucCmd, "addts", 5) == 0)
		eTsOp = TX_ADDTS_REQ;
	else if (kalStrniCmp(pucCmd, "delts", 5) == 0)
		eTsOp = TX_DELTS_REQ;
	else {
		DBGLOG(OID, INFO, "wrong operation %s\n", pucCmd);
		return WLAN_STATUS_FAILURE;
	}
	/* addts token n,tid n,dir n,psb n,up n,fixed n,size n,maxsize n,maxsrvint n, minsrvint n,
	** inact n, suspension n, srvstarttime n, minrate n,meanrate n,peakrate n,burst n,delaybound n,
	** phyrate n,SBA n,mediumtime n
	*/
	prMsgTsOperate = (struct MSG_TS_OPERATE *)cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(struct MSG_TS_OPERATE));
	if (!prMsgTsOperate)
		return WLAN_STATUS_FAILURE;

	kalMemZero(prMsgTsOperate, sizeof(struct MSG_TS_OPERATE));
	prMsgTsOperate->rMsgHdr.eMsgId = MID_OID_WMM_TSPEC_OPERATE;
	prMsgTsOperate->eOpCode = eTsOp;
	prTspecParam = &prMsgTsOperate->rTspecParam;
	pucCmd += 6;
	pucItem = (PUINT_8)strtok_r((CHAR *)pucCmd, ",", (CHAR **)&pucSavedPtr);
	while (pucItem) {
		if (kalStrniCmp(pucItem, "token ", 6) == 0)
			u4Ret = kstrtou8(pucItem+6, 0, &prTspecParam->ucDialogToken);
		else if (kalStrniCmp(pucItem, "tid ", 4) == 0) {
			u4Ret = kstrtou8(pucItem+4, 0, &prMsgTsOperate->ucTid);
			prTspecParam->rTsInfo.ucTid = prMsgTsOperate->ucTid;
		} else if (kalStrniCmp(pucItem, "dir ", 4) == 0)
			u4Ret = kstrtou8(pucItem+4, 0, &prTspecParam->rTsInfo.ucDirection);
		else if (kalStrniCmp(pucItem, "psb ", 4) == 0)
			u4Ret = kstrtou8(pucItem+4, 0, &ucApsdSetting);
		else if (kalStrniCmp(pucItem, "up ", 3) == 0)
			u4Ret = kstrtou8(pucItem+3, 0, &prTspecParam->rTsInfo.ucuserPriority);
		else if (kalStrniCmp(pucItem, "size ", 5) == 0) {
			UINT_16 u2Size = 0;

			u4Ret = kstrtou16(pucItem+5, 0, &u2Size);
			prTspecParam->u2NominalMSDUSize |= u2Size;
		} else if (kalStrniCmp(pucItem, "fixed ", 6) == 0) {
			UINT_8 ucFixed = 0;

			u4Ret = kstrtou8(pucItem+6, 0, &ucFixed);
			if (ucFixed)
				prTspecParam->u2NominalMSDUSize |= BIT(15);
		} else if (kalStrniCmp(pucItem, "maxsize ", 8) == 0)
			u4Ret = kstrtou16(pucItem+8, 0, &prTspecParam->u2MaxMSDUsize);
		else if (kalStrniCmp(pucItem, "maxsrvint ", 10) == 0)
			u4Ret = kalkStrtou32(pucItem+10, 0, &prTspecParam->u4MaxSvcIntv);
		else if (kalStrniCmp(pucItem, "minsrvint ", 10) == 0)
			u4Ret = kalkStrtou32(pucItem+10, 0, &prTspecParam->u4MinSvcIntv);
		else if (kalStrniCmp(pucItem, "inact ", 6) == 0)
			u4Ret = kalkStrtou32(pucItem+6, 0, &prTspecParam->u4InactIntv);
		else if (kalStrniCmp(pucItem, "suspension ", 11) == 0)
			u4Ret = kalkStrtou32(pucItem+11, 0, &prTspecParam->u4SpsIntv);
		else if (kalStrniCmp(pucItem, "srvstarttime ", 13) == 0)
			u4Ret = kalkStrtou32(pucItem+13, 0, &prTspecParam->u4SvcStartTime);
		else if (kalStrniCmp(pucItem, "minrate ", 8) == 0)
			u4Ret = kalkStrtou32(pucItem+8, 0, &prTspecParam->u4MinDataRate);
		else if (kalStrniCmp(pucItem, "meanrate ", 9) == 0)
			u4Ret = kalkStrtou32(pucItem+9, 0, &prTspecParam->u4MeanDataRate);
		else if (kalStrniCmp(pucItem, "peakrate ", 9) == 0)
			u4Ret = kalkStrtou32(pucItem+9, 0, &prTspecParam->u4PeakDataRate);
		else if (kalStrniCmp(pucItem, "burst ", 6) == 0)
			u4Ret = kalkStrtou32(pucItem+6, 0, &prTspecParam->u4MaxBurstSize);
		else if (kalStrniCmp(pucItem, "delaybound ", 11) == 0)
			u4Ret = kalkStrtou32(pucItem+11, 0, &prTspecParam->u4DelayBound);
		else if (kalStrniCmp(pucItem, "phyrate ", 8) == 0)
			u4Ret = kalkStrtou32(pucItem+8, 0, &prTspecParam->u4MinPHYRate);
		else if (kalStrniCmp(pucItem, "sba ", 4) == 0)
			u4Ret = wlanDecimalStr2Hexadecimals(pucItem+4, &prTspecParam->u2Sba);
		else if (kalStrniCmp(pucItem, "mediumtime ", 11) == 0)
			u4Ret = kstrtou16(pucItem+11, 0, &prTspecParam->u2MediumTime);

		if (u4Ret) {
			DBGLOG(OID, ERROR, "Parse %s error\n", pucItem);
			cnmMemFree(prAdapter, prMsgTsOperate);
			return WLAN_STATUS_FAILURE;
		}
		pucItem = (PUINT_8)strtok_r(NULL, ",", (CHAR **)&pucSavedPtr);
	}
	/* if APSD is not set in addts request, use global wmmps settings */
	if (ucApsdSetting == 2) {
		P_PM_PROFILE_SETUP_INFO_T prPmProf =
			&prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX].rPmProfSetupInfo;
		ENUM_ACI_T eAc = aucUp2ACIMap[prTspecParam->rTsInfo.ucuserPriority];

		switch (prTspecParam->rTsInfo.ucDirection) {
		case UPLINK_TS: /* UpLink*/
			if (prPmProf->ucBmpTriggerAC & BIT(eAc))
				prTspecParam->rTsInfo.ucApsd = 1;
			break;
		case DOWNLINK_TS:/* DownLink */
			if (prPmProf->ucBmpDeliveryAC & BIT(eAc))
				prTspecParam->rTsInfo.ucApsd = 1;
			break;
		case BI_DIR_TS: /* Bi-directional */
			if ((prPmProf->ucBmpTriggerAC & BIT(eAc)) &&
				(prPmProf->ucBmpDeliveryAC & BIT(eAc)))
				prTspecParam->rTsInfo.ucApsd = 1;
			break;
		}
	} else
		prTspecParam->rTsInfo.ucApsd = ucApsdSetting;
	*(--pucCmd) = 0;
	pucCmd -= 5;
	DBGLOG(OID, INFO, "%s %d %d %d %d %d %d %d %u %u %u %u %u %u %u %u %u %u %u 0x%04x %d\n", pucCmd,
		prTspecParam->ucDialogToken, prTspecParam->rTsInfo.ucTid, prTspecParam->rTsInfo.ucDirection,
		prTspecParam->rTsInfo.ucApsd, prTspecParam->rTsInfo.ucuserPriority, prTspecParam->u2NominalMSDUSize,
		prTspecParam->u2MaxMSDUsize, prTspecParam->u4MaxSvcIntv, prTspecParam->u4MinSvcIntv,
		prTspecParam->u4InactIntv, prTspecParam->u4SpsIntv, prTspecParam->u4SvcStartTime,
		prTspecParam->u4MinDataRate, prTspecParam->u4MeanDataRate, prTspecParam->u4PeakDataRate,
		prTspecParam->u4MaxBurstSize, prTspecParam->u4DelayBound, prTspecParam->u4MinPHYRate,
		prTspecParam->u2Sba, prTspecParam->u2MediumTime);
	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T)prMsgTsOperate, MSG_SEND_METHOD_BUF);
	return WLAN_STATUS_SUCCESS;
}
#if CFG_SUPPORT_FCC_POWER_BACK_OFF
WLAN_STATUS
wlanoidSetFccCert(IN P_ADAPTER_T prAdapter,
		  IN PVOID pvSetBuffer,
		  IN UINT_32 u4SetBufferLen,
		  OUT PUINT_32 pu4SetInfoLen)
{
	P_CMD_FCC_TX_PWR_ADJUST prFccTxPwrAdjust = (P_CMD_FCC_TX_PWR_ADJUST)pvSetBuffer;

	if (!pvSetBuffer || u4SetBufferLen != sizeof(CMD_FCC_TX_PWR_ADJUST))
		return WLAN_STATUS_INVALID_DATA;
	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_SET_FCC_TX_PWR_CERT,
				   TRUE,
				   FALSE,
				   TRUE,
				   NULL,
				   NULL,
				   sizeof(CMD_FCC_TX_PWR_ADJUST),
				   (PUINT_8) (prFccTxPwrAdjust),
				   NULL,
				   0);
	}
#endif

/* It's a Integretion Test function for RadioMeasurement. If you found errors during doing Radio Measurement,
** you can run this IT function with iwpriv wlan0 set_str_cmd \"31 RM-IT xx,xx,xx, xx\"
** xx,xx,xx,xx is the RM request frame data
*/
WLAN_STATUS
wlanoidRadioMeasurementIT(P_ADAPTER_T prAdapter, PVOID pvBuffer, UINT_32 u4BufferLen,
									  PUINT_32 pu4InfoLen)
{
	SW_RFB_T rSwRfb;
	UINT_8 aucPacket[200] = {0,};
	PUINT_8 pucSavedPtr = NULL;
	PUINT_8 pucItem = NULL;
	UINT_8 j = 0;
	INT_8 i = 0;
	UINT_8 ucByte;

	if (!pvBuffer) {
		DBGLOG(OID, ERROR, "pvBuffer is NULL\n");
		return WLAN_STATUS_FAILURE;
	}

	pucItem = strtok_r((CHAR *)pvBuffer, ",", (CHAR **)&pucSavedPtr);
	while (pucItem) {
		ucByte = *pucItem;
		i = 0;
		while (ucByte) {
			if (i > 1) {
				DBGLOG(OID, ERROR, "more than 2 char for one byte\n");
				return WLAN_STATUS_FAILURE;
			} else if (i == 1)
				aucPacket[j] <<= 4;
			if (ucByte >= '0' && ucByte <= '9')
				aucPacket[j] |= ucByte - '0';
			else if (ucByte >= 'a' && ucByte <= 'f')
				aucPacket[j] |= ucByte - 'a' + 10;
			else if (ucByte >= 'A' && ucByte <= 'F')
				aucPacket[j] |= ucByte - 'A' + 10;
			else {
				DBGLOG(OID, ERROR, "not a hex char %c\n", ucByte);
				return WLAN_STATUS_FAILURE;
			}
			ucByte = *(++pucItem);
			i++;
		}
		j++;
		pucItem = strtok_r(NULL, ",", (CHAR **)&pucSavedPtr);
	}
	DBGLOG(OID, INFO, "Dump RadioMeasurement IT packet, len %d\n", j);
	dumpMemory8(aucPacket, j);
	if (j < WLAN_MAC_MGMT_HEADER_LEN) {
		DBGLOG(OID, ERROR, "packet length %d less than mac header 24\n", j);
		return WLAN_STATUS_FAILURE;
	}
	rSwRfb.pvHeader = (PVOID)&aucPacket[0];
	rSwRfb.u2PacketLen = j;
	rSwRfb.u2HeaderLen = WLAN_MAC_MGMT_HEADER_LEN;
	rlmProcessRadioMeasurementRequest(prAdapter, &rSwRfb);
	return WLAN_STATUS_SUCCESS;
}

WLAN_STATUS
wlanoidDumpUapsdSetting(P_ADAPTER_T prAdapter, PVOID pvBuffer, UINT_32 u4BufferLen,
									  PUINT_32 pu4InfoLen)
{
	PUINT_8 pucCmd = (PUINT_8)pvBuffer;
	UINT_8 ucFinalSetting = 0;
	UINT_8 ucStaticSetting = 0;
	P_PM_PROFILE_SETUP_INFO_T prPmProf = NULL;

	if (!pvBuffer) {
		DBGLOG(OID, ERROR, "pvBuffer is NULL\n");
		return WLAN_STATUS_FAILURE;
	}
	prPmProf = &prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX].rPmProfSetupInfo;
	ucStaticSetting = (prPmProf->ucBmpDeliveryAC << 4) | prPmProf->ucBmpTriggerAC;
	ucFinalSetting = wmmCalculateUapsdSetting(prAdapter);
	*pu4InfoLen = kalSnprintf(pucCmd, u4BufferLen,
		"\nStatic Uapsd Setting:0x%02x\nFinal Uapsd Setting:0x%02x", ucStaticSetting, ucFinalSetting);
	return WLAN_STATUS_SUCCESS;
}
#if CFG_SUPPORT_NCHO
#define FW_CFG_KEY_NCHO_ENABLE			"NCHOEnable"
#define FW_CFG_KEY_NCHO_ROAM_RCPI		"RoamingRCPIValue"
#define FW_CFG_KEY_NCHO_SCN_CHANNEL_TIME	"NCHOScnChannelTime"
#define FW_CFG_KEY_NCHO_SCN_HOME_TIME		"NCHOScnHomeTime"
#define FW_CFG_KEY_NCHO_SCN_HOME_AWAY_TIME	"NCHOScnHomeAwayTime"
#define FW_CFG_KEY_NCHO_SCN_NPROBES		"NCHOScnNumProbs"
#define FW_CFG_KEY_NCHO_WES_MODE		"NCHOWesMode"
#define FW_CFG_KEY_NCHO_SCAN_DFS_MODE		"NCHOScnDfsMode"

WLAN_STATUS
wlanoidSetNchoHeader(struct _CMD_HEADER_T *prCmdHeader,
				struct _CMD_FORMAT_V1_T *pr_cmd_v1,
				char *pStr, UINT_32 u4Len)
{
	prCmdHeader->cmdVersion = CMD_VER_1_EXT;
	prCmdHeader->cmdType = CMD_TYPE_QUERY;
	prCmdHeader->itemNum = 1;
	prCmdHeader->cmdBufferLen = sizeof(struct _CMD_FORMAT_V1_T);
	kalMemSet(prCmdHeader->buffer, 0, MAX_CMD_BUFFER_LENGTH);

	if (!prCmdHeader || !pStr || u4Len == 0)
		return WLAN_STATUS_FAILURE;

	pr_cmd_v1->itemStringLength = u4Len;
	kalMemCopy(pr_cmd_v1->itemString, pStr, u4Len);

	return WLAN_STATUS_SUCCESS;
}

WLAN_STATUS
wlanoidSetNchoRoamTrigger(IN P_ADAPTER_T prAdapter,
		       IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen) {
	PINT_32 pi4Param = NULL;
	char acCmd[NCHO_CMD_MAX_LENGTH] = {0};
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;

	DEBUGFUNC("wlanoidSetNchoRoamTrigger");
	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(INT_32);

	if (u4SetBufferLen < sizeof(INT_32))
		return WLAN_STATUS_INVALID_LENGTH;

	pi4Param = (PINT_32) pvSetBuffer;
	*pi4Param = dBm_TO_RCPI(*pi4Param);		/* DB to RCPI */
	if (*pi4Param < RCPI_LOW_BOUND || *pi4Param > RCPI_HIGH_BOUND) {
		DBGLOG(INIT, ERROR, "NCHO roam trigger invalid %d\n", *pi4Param);
		return WLAN_STATUS_INVALID_DATA;
	}

	kalSprintf(acCmd, "%s %d", FW_CFG_KEY_NCHO_ROAM_RCPI, *pi4Param);
	rStatus =  wlanFwCfgParse(prAdapter, acCmd);
	if (rStatus == WLAN_STATUS_SUCCESS) {
		prAdapter->rNchoInfo.i4RoamTrigger = RCPI_TO_dBm(*pi4Param);
		DBGLOG(INIT, TRACE, "NCHO roam trigger is %d\n", prAdapter->rNchoInfo.i4RoamTrigger);
	}

	return rStatus;
}

WLAN_STATUS
wlanoidQueryNchoRoamTrigger(IN P_ADAPTER_T prAdapter,
			OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen) {
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;
	struct _CMD_HEADER_T cmdV1Header;
	struct _CMD_HEADER_T *prCmdV1Header = (struct _CMD_HEADER_T *)pvQueryBuffer;
	struct _CMD_FORMAT_V1_T *prCmdV1 = NULL;

	DEBUGFUNC("wlanoidQueryNchoRoamTrigger");
	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(struct _CMD_HEADER_T);

	if (u4QueryBufferLen < sizeof(struct _CMD_HEADER_T))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (prAdapter->rNchoInfo.fgECHOEnabled == FALSE)
		return WLAN_STATUS_INVALID_DATA;

	prCmdV1 = (struct _CMD_FORMAT_V1_T *) prCmdV1Header->buffer;
	rStatus = wlanoidSetNchoHeader(prCmdV1Header,
					prCmdV1,
					FW_CFG_KEY_NCHO_ROAM_RCPI,
					kalStrLen(FW_CFG_KEY_NCHO_ROAM_RCPI));
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "NCHO no enough memory\n");
		return rStatus;
	}
	kalMemCopy(&cmdV1Header, prCmdV1Header, sizeof(struct _CMD_HEADER_T));
	rStatus = wlanSendSetQueryCmd(
			prAdapter,
			CMD_ID_GET_SET_CUSTOMER_CFG,
			FALSE,
			TRUE,
			TRUE,
			nicCmdEventQueryCfgRead,
			nicOidCmdTimeoutCommon,
			sizeof(struct _CMD_HEADER_T),
			(PUINT_8)&cmdV1Header,
			pvQueryBuffer,
			u4QueryBufferLen);
	return rStatus;
}

WLAN_STATUS
wlanoidSetNchoRoamDelta(IN P_ADAPTER_T prAdapter,
			IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen) {
	INT_32 *pi4Param = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;

	DEBUGFUNC("wlanoidSetNchoRoamDelta");
	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(INT_32);

	if (u4SetBufferLen < sizeof(INT_32))
		return WLAN_STATUS_INVALID_LENGTH;

	pi4Param = (PINT_32) pvSetBuffer;
	if (*pi4Param > 100) {
		DBGLOG(INIT, ERROR, "NCHO roam delta invalid %d\n", *pi4Param);
		return WLAN_STATUS_INVALID_DATA;
	}

	prAdapter->rNchoInfo.i4RoamDelta = *pi4Param;
	DBGLOG(INIT, TRACE, "NCHO roam delta is %d\n", *pi4Param);
	rStatus = WLAN_STATUS_SUCCESS;

	return rStatus;
}

WLAN_STATUS
wlanoidQueryNchoRoamDelta(IN P_ADAPTER_T prAdapter,
			OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen) {
	INT_32 *pParam = NULL;

	DEBUGFUNC("wlanoidQueryNchoRoamDelta");
	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (u4QueryBufferLen < sizeof(PINT_32))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (prAdapter->rNchoInfo.fgECHOEnabled == FALSE)
		return WLAN_STATUS_INVALID_DATA;

	pParam = (PINT_32) pvQueryBuffer;
	*pParam = prAdapter->rNchoInfo.i4RoamDelta;
	DBGLOG(INIT, TRACE, "NCHO roam delta is %d\n", *pParam);

	return WLAN_STATUS_SUCCESS;
}

WLAN_STATUS
wlanoidSetNchoRoamScnPeriod(IN P_ADAPTER_T prAdapter,
			IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen) {
	UINT_32 *pParam = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;

	DEBUGFUNC("wlanoidSetNchoRoamScnPeriod");
	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(UINT_32);

	if (u4SetBufferLen < sizeof(UINT_32))
		return WLAN_STATUS_INVALID_LENGTH;

	pParam = (PUINT_32) pvSetBuffer;

	prAdapter->rNchoInfo.u4RoamScanPeriod = *pParam;
	DBGLOG(INIT, TRACE, "NCHO roam scan period is %d\n", *pParam);
	rStatus = WLAN_STATUS_SUCCESS;

	return rStatus;
}

WLAN_STATUS
wlanoidQueryNchoRoamScnPeriod(IN P_ADAPTER_T prAdapter,
			OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen) {
	UINT_32 *pParam = NULL;

	DEBUGFUNC("wlanoidQueryNchoRoamScnPeriod");
	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (u4QueryBufferLen < sizeof(UINT_32))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (prAdapter->rNchoInfo.fgECHOEnabled == FALSE)
		return WLAN_STATUS_INVALID_DATA;

	pParam = (PUINT_32) pvQueryBuffer;
	*pParam = prAdapter->rNchoInfo.u4RoamScanPeriod;
	DBGLOG(INIT, TRACE, "NCHO roam scan period is %d\n", *pParam);

	return WLAN_STATUS_SUCCESS;
}

WLAN_STATUS
wlanoidSetNchoRoamScnChnl(IN P_ADAPTER_T prAdapter,
			IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen) {
	PCFG_NCHO_SCAN_CHNL_T prRoamScnChnl = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;

	DEBUGFUNC("wlanoidSetNchoRoamScnChnl");
	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(CFG_NCHO_SCAN_CHNL_T);

	if (u4SetBufferLen < sizeof(CFG_NCHO_SCAN_CHNL_T))
		return WLAN_STATUS_INVALID_LENGTH;

	prRoamScnChnl = (PCFG_NCHO_SCAN_CHNL_T) pvSetBuffer;

	kalMemCopy(&prAdapter->rNchoInfo.rRoamScnChnl, prRoamScnChnl, *pu4SetInfoLen);
	prAdapter->rNchoInfo.u4RoamScanControl = TRUE;
	DBGLOG(INIT, TRACE, "NCHO set roam scan channel num is %d\n", prRoamScnChnl->ucChannelListNum);
	rStatus = WLAN_STATUS_SUCCESS;


	return rStatus;
}

WLAN_STATUS
wlanoidQueryNchoRoamScnChnl(IN P_ADAPTER_T prAdapter,
			OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen) {
	PCFG_NCHO_SCAN_CHNL_T prRoamScnChnl = NULL;

	DEBUGFUNC("wlanoidQueryNchoRoamScnChnl");
	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (u4QueryBufferLen < sizeof(CFG_NCHO_SCAN_CHNL_T))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (prAdapter->rNchoInfo.fgECHOEnabled == FALSE)
		return WLAN_STATUS_INVALID_DATA;

	prRoamScnChnl = (PCFG_NCHO_SCAN_CHNL_T) pvQueryBuffer;
	kalMemCopy(prRoamScnChnl, &prAdapter->rNchoInfo.rRoamScnChnl, u4QueryBufferLen);
	DBGLOG(INIT, TRACE, "NCHO roam scan channel num is %d\n", prRoamScnChnl->ucChannelListNum);

	return WLAN_STATUS_SUCCESS;
}

WLAN_STATUS
wlanoidSetNchoRoamScnCtrl(IN P_ADAPTER_T prAdapter,
			IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen) {
	UINT_32 *pParam = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;

	DEBUGFUNC("wlanoidSetNchoRoamScnChnl");
	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(UINT_32);

	if (u4SetBufferLen < sizeof(UINT_32))
		return WLAN_STATUS_INVALID_LENGTH;

	pParam = (PUINT_32) pvSetBuffer;
	if (*pParam != TRUE && *pParam != FALSE) {
		DBGLOG(INIT, ERROR, "NCHO roam scan control invalid %d\n", *pParam);
		return WLAN_STATUS_INVALID_DATA;
	}

	prAdapter->rNchoInfo.u4RoamScanControl = *pParam;
	DBGLOG(INIT, TRACE, "NCHO roam scan control is %d\n", *pParam);
	rStatus = WLAN_STATUS_SUCCESS;

	return rStatus;
}

WLAN_STATUS
wlanoidQueryNchoRoamScnCtrl(IN P_ADAPTER_T prAdapter,
			OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen) {
	UINT_32 *pParam = NULL;

	DEBUGFUNC("wlanoidQueryNchoRoamScnCtrl");
	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (u4QueryBufferLen < sizeof(UINT_32))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (prAdapter->rNchoInfo.fgECHOEnabled == FALSE)
		return WLAN_STATUS_INVALID_DATA;

	pParam = (PUINT_32) pvQueryBuffer;
	*pParam = prAdapter->rNchoInfo.u4RoamScanControl;
	DBGLOG(INIT, TRACE, "NCHO roam scan control is %d\n", *pParam);

	return WLAN_STATUS_SUCCESS;
}

WLAN_STATUS
wlanoidSetNchoScnChnlTime(IN P_ADAPTER_T prAdapter,
			IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen) {
	UINT_32 *pParam = NULL;
	char acCmd[NCHO_CMD_MAX_LENGTH] = {0};
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;

	DEBUGFUNC("wlanoidSetNchoScnChnlTime");
	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(UINT_32);

	if (u4SetBufferLen < sizeof(UINT_32))
		return WLAN_STATUS_INVALID_LENGTH;

	pParam = (PUINT_32) pvSetBuffer;
	if (*pParam < 10 && *pParam > 1000) {
		DBGLOG(INIT, ERROR, "NCHO scan channel time invalid %d\n", *pParam);
		return WLAN_STATUS_INVALID_DATA;
	}

	kalSprintf(acCmd, "%s %d", FW_CFG_KEY_NCHO_SCN_CHANNEL_TIME, *pParam);
	rStatus =  wlanFwCfgParse(prAdapter, acCmd);
	if (rStatus == WLAN_STATUS_SUCCESS) {
		prAdapter->rNchoInfo.u4ScanChannelTime = *pParam;
		DBGLOG(INIT, TRACE, "NCHO scan channel time is %d\n", *pParam);
	}

	return rStatus;
}

WLAN_STATUS
wlanoidQueryNchoScnChnlTime(IN P_ADAPTER_T prAdapter,
			OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen) {
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;
	struct _CMD_HEADER_T cmdV1Header;
	struct _CMD_HEADER_T *prCmdV1Header = (struct _CMD_HEADER_T *)pvQueryBuffer;
	struct _CMD_FORMAT_V1_T *prCmdV1 = NULL;

	DEBUGFUNC("wlanoidQueryNchoScnChnlTime");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(struct _CMD_HEADER_T);

	if (u4QueryBufferLen < sizeof(struct _CMD_HEADER_T))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (prAdapter->rNchoInfo.fgECHOEnabled == FALSE)
		return WLAN_STATUS_INVALID_DATA;

	prCmdV1 = (struct _CMD_FORMAT_V1_T *) prCmdV1Header->buffer;
	rStatus = wlanoidSetNchoHeader(prCmdV1Header,
					prCmdV1,
					FW_CFG_KEY_NCHO_SCN_CHANNEL_TIME,
					kalStrLen(FW_CFG_KEY_NCHO_SCN_CHANNEL_TIME));
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "NCHO no enough memory\n");
		return rStatus;
	}
	kalMemCopy(&cmdV1Header, prCmdV1Header, sizeof(struct _CMD_HEADER_T));
	rStatus = wlanSendSetQueryCmd(
			prAdapter,
			CMD_ID_GET_SET_CUSTOMER_CFG,
			FALSE,
			TRUE,
			TRUE,
			nicCmdEventQueryCfgRead,
			nicOidCmdTimeoutCommon,
			sizeof(struct _CMD_HEADER_T),
			(PUINT_8)&cmdV1Header,
			pvQueryBuffer,
			u4QueryBufferLen);
	return rStatus;
}

WLAN_STATUS
wlanoidSetNchoScnHomeTime(IN P_ADAPTER_T prAdapter,
			IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen) {
	UINT_32 *pParam = NULL;
	char acCmd[NCHO_CMD_MAX_LENGTH] = {0};
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;

	DEBUGFUNC("wlanoidSetNchoScnHomeTime");
	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(UINT_32);

	if (u4SetBufferLen < sizeof(UINT_32))
		return WLAN_STATUS_INVALID_LENGTH;

	pParam = (PUINT_32) pvSetBuffer;
	if (*pParam < 10 && *pParam > 1000) {
		DBGLOG(INIT, ERROR, "NCHO scan home time invalid %d\n", *pParam);
		return WLAN_STATUS_INVALID_DATA;
	}

	kalSprintf(acCmd, "%s %d", FW_CFG_KEY_NCHO_SCN_HOME_TIME, *pParam);
	DBGLOG(REQ, TRACE, "NCHO cmd is %s\n", acCmd);
	rStatus =  wlanFwCfgParse(prAdapter, acCmd);
	if (rStatus == WLAN_STATUS_SUCCESS) {
		prAdapter->rNchoInfo.u4ScanHomeTime = *pParam;
		DBGLOG(INIT, TRACE, "NCHO scan home time is %d\n", *pParam);
	}

	return rStatus;
}

WLAN_STATUS
wlanoidQueryNchoScnHomeTime(IN P_ADAPTER_T prAdapter,
			OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen) {
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;
	struct _CMD_HEADER_T cmdV1Header;
	struct _CMD_HEADER_T *prCmdV1Header = (struct _CMD_HEADER_T *)pvQueryBuffer;
	struct _CMD_FORMAT_V1_T *prCmdV1 = NULL;

	DEBUGFUNC("wlanoidQueryNchoScnHomeTime");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(struct _CMD_HEADER_T);

	if (u4QueryBufferLen < sizeof(struct _CMD_HEADER_T))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (prAdapter->rNchoInfo.fgECHOEnabled == FALSE)
		return WLAN_STATUS_INVALID_DATA;

	prCmdV1 = (struct _CMD_FORMAT_V1_T *) prCmdV1Header->buffer;
	rStatus = wlanoidSetNchoHeader(prCmdV1Header,
					prCmdV1,
					FW_CFG_KEY_NCHO_SCN_HOME_TIME,
					kalStrLen(FW_CFG_KEY_NCHO_SCN_HOME_TIME));
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "NCHO no enough memory\n");
		return rStatus;
	}
	kalMemCopy(&cmdV1Header, prCmdV1Header, sizeof(struct _CMD_HEADER_T));
	rStatus = wlanSendSetQueryCmd(
			prAdapter,
			CMD_ID_GET_SET_CUSTOMER_CFG,
			FALSE,
			TRUE,
			TRUE,
			nicCmdEventQueryCfgRead,
			nicOidCmdTimeoutCommon,
			sizeof(struct _CMD_HEADER_T),
			(PUINT_8)&cmdV1Header,
			pvQueryBuffer,
			u4QueryBufferLen);
	return rStatus;
}

WLAN_STATUS
wlanoidSetNchoScnHomeAwayTime(IN P_ADAPTER_T prAdapter,
			IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen) {
	UINT_32 *pParam = NULL;
	char acCmd[NCHO_CMD_MAX_LENGTH] = {0};
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;

	DEBUGFUNC("wlanoidSetNchoScnHomeAwayTime");
	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(UINT_32);

	if (u4SetBufferLen < sizeof(UINT_32))
		return WLAN_STATUS_INVALID_LENGTH;

	pParam = (PUINT_32) pvSetBuffer;
	if (*pParam < 10 && *pParam > 1000) {
		DBGLOG(INIT, ERROR, "NCHO scan home away time invalid %d\n", *pParam);
		return WLAN_STATUS_INVALID_DATA;
	}


	kalSprintf(acCmd, "%s %d", FW_CFG_KEY_NCHO_SCN_HOME_AWAY_TIME, *pParam);
	DBGLOG(REQ, TRACE, "NCHO cmd is %s\n", acCmd);
	rStatus =  wlanFwCfgParse(prAdapter, acCmd);
	if (rStatus == WLAN_STATUS_SUCCESS) {
		prAdapter->rNchoInfo.u4ScanHomeawayTime = *pParam;
		DBGLOG(INIT, TRACE, "NCHO scan home away is %d\n", *pParam);
	}

	return rStatus;
}

WLAN_STATUS
wlanoidQueryNchoScnHomeAwayTime(IN P_ADAPTER_T prAdapter,
			OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen) {
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;
	struct _CMD_HEADER_T cmdV1Header;
	struct _CMD_HEADER_T *prCmdV1Header = (struct _CMD_HEADER_T *)pvQueryBuffer;
	struct _CMD_FORMAT_V1_T *prCmdV1 = NULL;

	DEBUGFUNC("wlanoidQueryNchoScnHomeTime");
	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(struct _CMD_HEADER_T);

	if (u4QueryBufferLen < sizeof(struct _CMD_HEADER_T))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (prAdapter->rNchoInfo.fgECHOEnabled == FALSE)
		return WLAN_STATUS_INVALID_DATA;

	prCmdV1 = (struct _CMD_FORMAT_V1_T *) prCmdV1Header->buffer;
	rStatus = wlanoidSetNchoHeader(prCmdV1Header,
					prCmdV1,
					FW_CFG_KEY_NCHO_SCN_HOME_AWAY_TIME,
					kalStrLen(FW_CFG_KEY_NCHO_SCN_HOME_AWAY_TIME));
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "NCHO no enough memory\n");
		return rStatus;
	}
	kalMemCopy(&cmdV1Header, prCmdV1Header, sizeof(struct _CMD_HEADER_T));
	rStatus = wlanSendSetQueryCmd(
			prAdapter,
			CMD_ID_GET_SET_CUSTOMER_CFG,
			FALSE,
			TRUE,
			TRUE,
			nicCmdEventQueryCfgRead,
			nicOidCmdTimeoutCommon,
			sizeof(struct _CMD_HEADER_T),
			(PUINT_8)&cmdV1Header,
			pvQueryBuffer,
			u4QueryBufferLen);
	return rStatus;
}

WLAN_STATUS
wlanoidSetNchoScnNprobes(IN P_ADAPTER_T prAdapter,
			IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen) {
	UINT_32 *pParam = NULL;
	char acCmd[NCHO_CMD_MAX_LENGTH] = {0};
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;

	DEBUGFUNC("wlanoidSetNchoScnNprobes");
	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(UINT_32);

	if (u4SetBufferLen < sizeof(UINT_32))
		return WLAN_STATUS_INVALID_LENGTH;

	pParam = (PUINT_32) pvSetBuffer;
	if (*pParam > 16) {
		DBGLOG(INIT, ERROR, "NCHO scan Nprobes invalid %d\n", *pParam);
		return WLAN_STATUS_INVALID_DATA;
	}


	kalSprintf(acCmd, "%s %d", FW_CFG_KEY_NCHO_SCN_NPROBES, *pParam);
	rStatus =  wlanFwCfgParse(prAdapter, acCmd);
	if (rStatus == WLAN_STATUS_SUCCESS) {
		prAdapter->rNchoInfo.u4ScanNProbes = *pParam;
		DBGLOG(INIT, TRACE, "NCHO Nprobes is %d\n", *pParam);
	}
	return rStatus;
}

WLAN_STATUS
wlanoidQueryNchoScnNprobes(IN P_ADAPTER_T prAdapter,
			OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen) {
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;
	struct _CMD_HEADER_T cmdV1Header;
	struct _CMD_HEADER_T *prCmdV1Header = (struct _CMD_HEADER_T *)pvQueryBuffer;
	struct _CMD_FORMAT_V1_T *prCmdV1 = NULL;

	DEBUGFUNC("wlanoidQueryNchoScnNprobes");
	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(struct _CMD_HEADER_T);

	if (u4QueryBufferLen < sizeof(struct _CMD_HEADER_T))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (prAdapter->rNchoInfo.fgECHOEnabled == FALSE)
		return WLAN_STATUS_INVALID_DATA;

	prCmdV1 = (struct _CMD_FORMAT_V1_T *) prCmdV1Header->buffer;
	rStatus = wlanoidSetNchoHeader(prCmdV1Header,
					prCmdV1,
					FW_CFG_KEY_NCHO_SCN_NPROBES,
					kalStrLen(FW_CFG_KEY_NCHO_SCN_NPROBES));
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "NCHO no enough memory\n");
		return rStatus;
	}
	kalMemCopy(&cmdV1Header, prCmdV1Header, sizeof(struct _CMD_HEADER_T));
	rStatus = wlanSendSetQueryCmd(
			prAdapter,
			CMD_ID_GET_SET_CUSTOMER_CFG,
			FALSE,
			TRUE,
			TRUE,
			nicCmdEventQueryCfgRead,
			nicOidCmdTimeoutCommon,
			sizeof(struct _CMD_HEADER_T),
			(PUINT_8)&cmdV1Header,
			pvQueryBuffer,
			u4QueryBufferLen);
	return rStatus;
}

WLAN_STATUS
wlanoidGetNchoReassocInfo(IN P_ADAPTER_T prAdapter,
			OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen) {
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;
	P_BSS_DESC_T prBssDesc = NULL;
	P_PARAM_CONNECT_T prParamConn;

	DEBUGFUNC("wlanoidGetNchoReassocInfo");
	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	ASSERT(pvQueryBuffer);

	prParamConn = (P_PARAM_CONNECT_T)pvQueryBuffer;
	if (prAdapter->rNchoInfo.fgECHOEnabled == TRUE) {
		prBssDesc = scanSearchBssDescByBssid(prAdapter, prParamConn->pucBssid);
		if (prBssDesc != NULL) {
			prParamConn->u4SsidLen = prBssDesc->ucSSIDLen;
			COPY_SSID(prParamConn->pucSsid,
				  prParamConn->u4SsidLen,
				  prBssDesc->aucSSID,
				  prBssDesc->ucSSIDLen);
			rStatus = WLAN_STATUS_SUCCESS;
		}
	}
	return rStatus;
}

WLAN_STATUS
wlanoidSendNchoActionFrameStart(IN P_ADAPTER_T prAdapter,
			IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen) {
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;
	P_NCHO_INFO prNchoInfo = NULL;
	P_NCHO_ACTION_FRAME_PARAMS prParamActionFrame = NULL;

	DEBUGFUNC("wlanoidSendNchoActionFrameStart");
	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);


	prNchoInfo = &prAdapter->rNchoInfo;
	prParamActionFrame = (P_NCHO_ACTION_FRAME_PARAMS)pvSetBuffer;
	prNchoInfo->fgIsSendingAF = TRUE;
	prNchoInfo->fgChGranted = FALSE;
	COPY_MAC_ADDR(prNchoInfo->rParamActionFrame.aucBssid, prParamActionFrame->aucBssid);
	prNchoInfo->rParamActionFrame.i4channel = prParamActionFrame->i4channel;
	prNchoInfo->rParamActionFrame.i4DwellTime = prParamActionFrame->i4DwellTime;
	prNchoInfo->rParamActionFrame.i4len = prParamActionFrame->i4len;
	kalMemCopy(prNchoInfo->rParamActionFrame.aucData,
		   prParamActionFrame->aucData,
		   prParamActionFrame->i4len);
	DBGLOG(INIT, TRACE, "NCHO send ncho action frame start\n");
	rStatus = WLAN_STATUS_SUCCESS;

	return rStatus;
}

WLAN_STATUS
wlanoidSendNchoActionFrameEnd(IN P_ADAPTER_T prAdapter,
			IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen) {
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;

	DEBUGFUNC("wlanoidSendNchoActionFrameEnd");
	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	prAdapter->rNchoInfo.fgIsSendingAF = FALSE;
	prAdapter->rNchoInfo.fgChGranted = TRUE;
	DBGLOG(INIT, TRACE, "NCHO send action frame end\n");
	rStatus = WLAN_STATUS_SUCCESS;

	return rStatus;
}

WLAN_STATUS
wlanoidSetNchoWesMode(IN P_ADAPTER_T prAdapter,
			IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen) {
	UINT_32 *pParam = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;

	DEBUGFUNC("wlanoidSetNchoWesMode");
	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(UINT_32);

	if (u4SetBufferLen < sizeof(UINT_32))
		return WLAN_STATUS_INVALID_LENGTH;

	pParam = (PUINT_32) pvSetBuffer;
	if (*pParam != TRUE && *pParam != FALSE) {
		DBGLOG(INIT, ERROR, "NCHO wes mode invalid %d\n", *pParam);
		return WLAN_STATUS_INVALID_DATA;
	}


	prAdapter->rNchoInfo.u4WesMode = *pParam;
	DBGLOG(INIT, TRACE, "NCHO WES mode is %d\n", *pParam);
	rStatus = WLAN_STATUS_SUCCESS;

	return rStatus;
}

WLAN_STATUS
wlanoidQueryNchoWesMode(IN P_ADAPTER_T prAdapter,
			OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen) {
	UINT_32 *pParam = NULL;

	DEBUGFUNC("wlanoidQueryNchoWesMode");
	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (u4QueryBufferLen < sizeof(UINT_32))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (prAdapter->rNchoInfo.fgECHOEnabled == FALSE)
		return WLAN_STATUS_INVALID_DATA;

	pParam = (PUINT_32) pvQueryBuffer;
	*pParam = prAdapter->rNchoInfo.u4WesMode;
	DBGLOG(INIT, TRACE, "NCHO Wes mode is %d\n", *pParam);

	return WLAN_STATUS_SUCCESS;
}

WLAN_STATUS
wlanoidSetNchoBand(IN P_ADAPTER_T prAdapter,
			IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen) {
	UINT_32 *pParam = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;

	DEBUGFUNC("wlanoidSetNchoBand");
	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(UINT_32);

	if (u4SetBufferLen < sizeof(UINT_32))
		return WLAN_STATUS_INVALID_LENGTH;

	pParam = (PUINT_32) pvSetBuffer;

	switch (*pParam) {
	case NCHO_BAND_AUTO:
		prAdapter->aePreferBand[NETWORK_TYPE_AIS_INDEX] = BAND_NULL;
		prAdapter->rNchoInfo.eBand = NCHO_BAND_AUTO;
		rStatus = WLAN_STATUS_SUCCESS;
		break;
	case NCHO_BAND_2G4:
		prAdapter->aePreferBand[NETWORK_TYPE_AIS_INDEX] = BAND_2G4;
		prAdapter->rNchoInfo.eBand = NCHO_BAND_2G4;
		rStatus = WLAN_STATUS_SUCCESS;
		break;
	case NCHO_BAND_5G:
		prAdapter->aePreferBand[NETWORK_TYPE_AIS_INDEX] = BAND_5G;
		prAdapter->rNchoInfo.eBand = NCHO_BAND_5G;
		rStatus = WLAN_STATUS_SUCCESS;
		break;
	default:
		DBGLOG(INIT, ERROR, "NCHO wes mode invalid %d\n", *pParam);
		rStatus = WLAN_STATUS_INVALID_DATA;
		break;
	}

	DBGLOG(INIT, INFO, "NCHO enabled:%d ,band:%d,status:%d\n"
		, prAdapter->rNchoInfo.fgECHOEnabled, *pParam, rStatus);


	return rStatus;
}

WLAN_STATUS
wlanoidQueryNchoBand(IN P_ADAPTER_T prAdapter,
			OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen) {
	UINT_32 *pParam = NULL;

	DEBUGFUNC("wlanoidQueryNchoBand");
	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (u4QueryBufferLen < sizeof(UINT_32))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (prAdapter->rNchoInfo.fgECHOEnabled == FALSE)
		return WLAN_STATUS_INVALID_DATA;

	pParam = (PUINT_32) pvQueryBuffer;
	*pParam = prAdapter->rNchoInfo.eBand;
	DBGLOG(INIT, TRACE, "NCHO band is %d\n", *pParam);

	return WLAN_STATUS_SUCCESS;
}

WLAN_STATUS
wlanoidSetNchoDfsScnMode(IN P_ADAPTER_T prAdapter,
			IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen) {
	UINT_32 *pParam = NULL;
	char acCmd[NCHO_CMD_MAX_LENGTH] = {0};
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;

	DEBUGFUNC("wlanoidSetNchoDfsScnMode");
	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(UINT_32);

	if (u4SetBufferLen < sizeof(UINT_32))
		return WLAN_STATUS_INVALID_LENGTH;

	pParam = (PUINT_32) pvSetBuffer;
	if (*pParam >= NCHO_DFS_SCN_NUM) {
		DBGLOG(INIT, ERROR, "NCHO DFS scan mode invalid %d\n", *pParam);
		return WLAN_STATUS_INVALID_DATA;
	}


	kalSprintf(acCmd, "%s %d", FW_CFG_KEY_NCHO_SCAN_DFS_MODE, *pParam);
	rStatus =  wlanFwCfgParse(prAdapter, acCmd);
	if (rStatus == WLAN_STATUS_SUCCESS) {
		prAdapter->rNchoInfo.eDFSScnMode = *pParam;
		DBGLOG(INIT, TRACE, "NCHO DFS scan mode is %d\n", *pParam);
	}

	return rStatus;
}

WLAN_STATUS
wlanoidQueryNchoDfsScnMode(IN P_ADAPTER_T prAdapter,
			OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen) {
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;
	struct _CMD_HEADER_T cmdV1Header;
	struct _CMD_HEADER_T *prCmdV1Header = (struct _CMD_HEADER_T *)pvQueryBuffer;
	struct _CMD_FORMAT_V1_T *prCmdV1 = NULL;

	DEBUGFUNC("wlanoidQueryNchoDfsScnMode");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(struct _CMD_HEADER_T);

	if (u4QueryBufferLen < sizeof(struct _CMD_HEADER_T))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (prAdapter->rNchoInfo.fgECHOEnabled == FALSE)
		return WLAN_STATUS_INVALID_DATA;

	prCmdV1 = (struct _CMD_FORMAT_V1_T *) prCmdV1Header->buffer;
	rStatus = wlanoidSetNchoHeader(prCmdV1Header,
					prCmdV1,
					FW_CFG_KEY_NCHO_SCAN_DFS_MODE,
					kalStrLen(FW_CFG_KEY_NCHO_SCAN_DFS_MODE));
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "NCHO no enough memory\n");
		return rStatus;
	}
	kalMemCopy(&cmdV1Header, prCmdV1Header, sizeof(struct _CMD_HEADER_T));
	rStatus = wlanSendSetQueryCmd(
			prAdapter,
			CMD_ID_GET_SET_CUSTOMER_CFG,
			FALSE,
			TRUE,
			TRUE,
			nicCmdEventQueryCfgRead,
			nicOidCmdTimeoutCommon,
			sizeof(struct _CMD_HEADER_T),
			(PUINT_8)&cmdV1Header,
			pvQueryBuffer,
			u4QueryBufferLen);
	return rStatus;
}

WLAN_STATUS
wlanoidSetNchoEnable(IN P_ADAPTER_T prAdapter,
			IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen) {
	UINT_32 *pParam = NULL;
	char acCmd[NCHO_CMD_MAX_LENGTH] = { 0 };
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;

	DEBUGFUNC("wlanoidSetNchoEnable");
	DBGLOG(OID, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(UINT_32);

	if (u4SetBufferLen < sizeof(UINT_32))
		return WLAN_STATUS_INVALID_LENGTH;

	pParam = (PUINT_32) pvSetBuffer;
	if (*pParam != 0 && *pParam != 1) {
		DBGLOG(INIT, ERROR, "NCHO DFS scan mode invalid %d\n", *pParam);
		return WLAN_STATUS_INVALID_DATA;
	}

	kalSprintf(acCmd, "%s %d", FW_CFG_KEY_NCHO_ENABLE, *pParam);
	rStatus = wlanFwCfgParse(prAdapter, acCmd);
	if (rStatus == WLAN_STATUS_SUCCESS) {
		prAdapter->rNchoInfo.fgECHOEnabled = *pParam;
		DBGLOG(INIT, INFO, "NCHO enable is %d\n", *pParam);
	}

	return rStatus;
}

WLAN_STATUS
wlanoidQueryNchoEnable(IN P_ADAPTER_T prAdapter,
			OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen) {
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;
	struct _CMD_HEADER_T cmdV1Header;
	struct _CMD_HEADER_T *prCmdV1Header = (struct _CMD_HEADER_T *)pvQueryBuffer;
	struct _CMD_FORMAT_V1_T *prCmdV1 = NULL;

	DEBUGFUNC("wlanoidQueryNchoRoamTrigger");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(struct _CMD_HEADER_T);

	if (u4QueryBufferLen < sizeof(struct _CMD_HEADER_T))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	prCmdV1 = (struct _CMD_FORMAT_V1_T *) prCmdV1Header->buffer;
	rStatus = wlanoidSetNchoHeader(prCmdV1Header,
					prCmdV1,
					FW_CFG_KEY_NCHO_ENABLE,
					kalStrLen(FW_CFG_KEY_NCHO_ENABLE));
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "NCHO no enough memory\n");
		return rStatus;
	}
	kalMemCopy(&cmdV1Header, prCmdV1Header, sizeof(struct _CMD_HEADER_T));
	rStatus = wlanSendSetQueryCmd(
			prAdapter,
			CMD_ID_GET_SET_CUSTOMER_CFG,
			FALSE,
			TRUE,
			TRUE,
			nicCmdEventQueryCfgRead,
			nicOidCmdTimeoutCommon,
			sizeof(struct _CMD_HEADER_T),
			(PUINT_8)&cmdV1Header,
			pvQueryBuffer,
			u4QueryBufferLen);
	return rStatus;
}
#endif /* CFG_SUPPORT_NCHO */
WLAN_STATUS
wlanoidAbortScan(IN P_ADAPTER_T prAdapter,
			OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen,
			OUT PUINT_32 pu4QueryInfoLen) {

	P_AIS_FSM_INFO_T prAisFsmInfo = NULL;

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	if (prAisFsmInfo->eCurrentState == AIS_STATE_SCAN ||
			prAisFsmInfo->eCurrentState == AIS_STATE_ONLINE_SCAN) {
		DBGLOG(OID, INFO,  "wlanoidAbortScan\n");
		aisFsmStateAbort_SCAN(prAdapter);
	}
	return WLAN_STATUS_SUCCESS;
}

#if CFG_SUPPORT_GAMING_MODE
/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to enable/disable gaming mode
*
* \param[in]  pvAdapter       A pointer to the Adapter structure.
* \param[in]  pvSetBuffer     A pointer to the buffer that holds the data to be set.
* \param[in]  u4SetBufferLen  The length of the set buffer.
* \param[out] pu4SetInfoLen   If the call is successful, returns the number of
*                             bytes read from the set buffer. If the call failed
*                             due to invalid length of the set buffer, returns
*                             the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS

*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanoidSetGamingMode(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer,
				 IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	struct CMD_GAMING_MODE_HEADER rGameModeHeader;
	BOOLEAN fgEnable, fgEnScan;
	UINT_32 u4Events;

	DEBUGFUNC("wlanoidSetGamingMode");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	if (u4SetBufferLen != sizeof(UINT_32))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	kalMemCopy(&u4Events, pvSetBuffer, u4SetBufferLen);

	DBGLOG(OID, INFO, "Event change gaming=%d, network:%d whitelist:%d\n",
	       (u4Events & GED_EVENT_GAS),
	       (u4Events & GED_EVENT_NETWORK),
	       (u4Events & GED_EVENT_DOPT_WIFI_SCAN));

	fgEnable = ((u4Events & GED_EVENT_GAS) != 0) && ((u4Events & GED_EVENT_NETWORK) != 0) &&
		(kalGetMediaStateIndicated(prAdapter->prGlueInfo) == PARAM_MEDIA_STATE_CONNECTED);
	fgEnScan = (!fgEnable || ((u4Events & GED_EVENT_DOPT_WIFI_SCAN) != 0));

	if (fgEnScan != prAdapter->fgEnCfg80211Scan || fgEnable != prAdapter->fgEnGamingMode)
		DBGLOG(OID, INFO, "Gaming mode event change Enable=%d EnableScan:%d\n",
		       fgEnable, fgEnScan);

	prAdapter->fgEnCfg80211Scan = fgEnScan;

	if (fgEnable == prAdapter->fgEnGamingMode)
		return WLAN_STATUS_SUCCESS;

	prAdapter->fgEnGamingMode = fgEnable;
	prAdapter->u4QmRxBaMissTimeout = fgEnable ?
		SHORT_QM_RX_BA_ENTRY_MISS_TIMEOUT_MS : DEFAULT_QM_RX_BA_ENTRY_MISS_TIMEOUT_MS;

	rGameModeHeader.ucVersion = GAMING_MODE_CMD_V1;
	rGameModeHeader.ucType = 0;
	rGameModeHeader.ucMagicCode = GAMING_MODE_MAGIC_CODE;
	rGameModeHeader.ucBufferLen = sizeof(struct GAMING_MODE_SETTING);
	rGameModeHeader.rSetting.fgEnable = fgEnable;

	wlanSendSetQueryCmd(prAdapter,	/* prAdapter */
			    CMD_ID_SET_GAMING_MODE,	/* ucCID */
			    TRUE,	/* fgSetQuery */
			    FALSE,	/* fgNeedResp */
			    TRUE,	/* fgIsOid */
			    NULL,	/* pfCmdDoneHandler */
			    NULL,	/* pfCmdTimeoutHandler */
			    sizeof(struct CMD_GAMING_MODE_HEADER),	/* u4SetQueryInfoLen */
			    (PUINT_8)&rGameModeHeader,	/* pucInfoBuffer */
			    NULL,	/* pvSetQueryBuffer */
			    0	/* u4SetQueryBufferLen */
		);
	return WLAN_STATUS_SUCCESS;
}
#endif /* CFG_SUPPORT_GAMING_MODE */

WLAN_STATUS
wlanoidQueryWifiLogLevelSupport(IN P_ADAPTER_T prAdapter,
		IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	struct PARAM_WIFI_LOG_LEVEL_UI *pparam;

	DEBUGFUNC("wlanoidQueryWifiLogLevelSupport");

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	pparam = (struct PARAM_WIFI_LOG_LEVEL_UI *) pvQueryBuffer;

	DBGLOG(OID, INFO, "version: %d, module: %d, enable: %d\n", pparam->u4Version,
			pparam->u4Module, pparam->u4Enable);

	pparam->u4Enable = wlanDbgLevelUiSupport(prAdapter, pparam->u4Version, pparam->u4Module);
	*pu4QueryInfoLen = sizeof(struct PARAM_WIFI_LOG_LEVEL_UI);

	return WLAN_STATUS_SUCCESS;
}

WLAN_STATUS
wlanoidQueryWifiLogLevel(IN P_ADAPTER_T prAdapter,
		IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	struct PARAM_WIFI_LOG_LEVEL *pparam;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidQueryWifiLogLevel");

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	pparam = (struct PARAM_WIFI_LOG_LEVEL *) pvQueryBuffer;

	DBGLOG(OID, INFO, "version: %d, module: %d\n", pparam->u4Version, pparam->u4Module);

	pparam->u4Level = wlanDbgGetLogLevelImpl(prAdapter, pparam->u4Version, pparam->u4Module);
	*pu4QueryInfoLen = sizeof(struct PARAM_WIFI_LOG_LEVEL_UI);

	return rStatus;
}

WLAN_STATUS
wlanoidSetWifiLogLevel(IN P_ADAPTER_T prAdapter,
		IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	struct PARAM_WIFI_LOG_LEVEL *pparam;

	DEBUGFUNC("wlanoidSetWifiLogLevel");

	ASSERT(prAdapter);
	if (u4SetBufferLen)
		ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	pparam = (struct PARAM_WIFI_LOG_LEVEL *) pvSetBuffer;

	DBGLOG(OID, INFO, "version: %d, module: %d, level: %d\n", pparam->u4Version,
			pparam->u4Module, pparam->u4Level);

	wlanDbgSetLogLevelImpl(prAdapter, pparam->u4Version, pparam->u4Module,
			pparam->u4Level);

	return WLAN_STATUS_SUCCESS;
}

uint32_t wlanoidSetTxPowerLimit(IN P_ADAPTER_T prAdapter,
				IN PVOID pvSetBuffer,
				IN UINT_32 u4SetBufferLen,
				OUT PUINT_32 pu4SetInfoLen)
{
	/* TxPwrBackOffParam's 0th byte contains enable/disable TxPowerBackOff for 2G */
	/* TxPwrBackOffParam's 1st byte contains default TxPowerBackOff value for 2G */
	/* TxPwrBackOffParam's 2nd byte contains enable/disable TxPowerBackOff for 5G */
	/* TxPwrBackOffParam's 3rd byte contains default TxPowerBackOff value for 5G */
	unsigned long TxPwrBackOffParam = 0;
	struct PARAM_TX_POWER_LIMIT *prTxPwrLimit;
	bool fgGotData = FALSE;
	uint32_t rStatus;

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);

	prTxPwrLimit = (struct PARAM_TX_POWER_LIMIT *)pvSetBuffer;
	DBGLOG(OID, INFO,
		"scenario=%d, enable2G=%d, txPowerLimit2G=%u, enable5G=%d, txPowerLimit5G=%u\n",
		prTxPwrLimit->iScenario,
		prTxPwrLimit->fgEnable2G, prTxPwrLimit->ucTxPowerLimit2G,
		prTxPwrLimit->fgEnable5G, prTxPwrLimit->ucTxPowerLimit5G);

	if (prTxPwrLimit->iScenario == -1) {
		/* reset tx power limitation */
		TxPwrBackOffParam = 0; /* First byte is start/stop */
		fgGotData = TRUE;
	} else if (prTxPwrLimit->iScenario == -2) {
		/* use the values in prTxPwrLimit */
		TxPwrBackOffParam |= prTxPwrLimit->fgEnable2G;
		TxPwrBackOffParam |= (prTxPwrLimit->ucTxPowerLimit2G * 2) << 8;
		TxPwrBackOffParam |= prTxPwrLimit->fgEnable5G << 16;
		TxPwrBackOffParam |= (unsigned long)
				(prTxPwrLimit->ucTxPowerLimit5G * 2) << 24;
		fgGotData = TRUE;
	} else if (prTxPwrLimit->iScenario >= 0) {
		/* use the global variable g_txPowerScenario
		 * by scenario index
		 */
		uint32_t u4Idx = (uint32_t)prTxPwrLimit->iScenario;
		uint32_t u4IdxMax = sizeof(g_txPowerScenario) /
			sizeof(struct TX_POWER_SCENARIO_ENTRY);

		if (u4IdxMax > 0) {
			if (u4Idx >= u4IdxMax) {
				DBGLOG(OID, ERROR,
				       "invalid index=%u, max=%u\n",
				       u4Idx, u4IdxMax);
				return -EINVAL;
			}

			TxPwrBackOffParam |=
				g_txPowerScenario[u4Idx].fgEnable2G;
			TxPwrBackOffParam |=
				(g_txPowerScenario[u4Idx].ucTxPowerLimit2G * 2)
				<< 8;
			TxPwrBackOffParam |=
				g_txPowerScenario[u4Idx].fgEnable5G << 16;
			TxPwrBackOffParam |= (unsigned long)
				(g_txPowerScenario[u4Idx].ucTxPowerLimit5G * 2)
				<< 24;
			fgGotData = TRUE;
		} else {
			/* if size of g_txPowerScenario == 0,
			 * try to use NVRAM setting
			 */
			DBGLOG(OID, INFO,
				"size of g_txPowerScenario=%u\n", u4IdxMax);
		}
	} else {
		DBGLOG(OID, ERROR, "invalid iScenario=%d\n",
			prTxPwrLimit->iScenario);
		return -EINVAL;
	}

	/* try to use NVRAM setting if values cannot be desided at last step */
	if (!fgGotData) {
		P_REG_INFO_T prRegInfo = &(prAdapter->prGlueInfo->rRegInfo);

		if (!prRegInfo) {
			DBGLOG(OID, INFO, "prRegInfo is NULL\n");
			return -EFAULT;
		}

		if (prRegInfo->bTxPowerLimitEnable2G ||
		    prRegInfo->bTxPowerLimitEnable5G) {
			TxPwrBackOffParam |= prRegInfo->bTxPowerLimitEnable2G;
			TxPwrBackOffParam |= (prRegInfo->cTxBackOffMaxPower2G)
					<< 8;
			TxPwrBackOffParam |= prRegInfo->bTxPowerLimitEnable5G
					<< 16;
			TxPwrBackOffParam |= (unsigned long)
					(prRegInfo->cTxBackOffMaxPower5G)
					<< 24;
			fgGotData = TRUE;
		}
	}

	if (!fgGotData) {
		DBGLOG(OID, ERROR, "fgGotData is FALSE\n");
		return -EFAULT;
	}

	DBGLOG(OID, INFO,
		"set tx power limitation: TxPwrBackOffParam=0x%lx\n",
		TxPwrBackOffParam);

	rStatus = nicTxPowerBackOff(prAdapter, TxPwrBackOffParam);
	if (rStatus == WLAN_STATUS_PENDING)
		rStatus = 0;
	else
		rStatus = -EINVAL;

	return rStatus;
}

WLAN_STATUS wlanoidGetWifiType(IN P_ADAPTER_T prAdapter,
			    IN void *pvSetBuffer,
			    IN uint32_t u4SetBufferLen,
			    OUT uint32_t *pu4SetInfoLen)
{
	struct PARAM_GET_WIFI_TYPE *prParamGetWifiType;
	P_BSS_INFO_T prBssInfo = NULL;
	uint8_t ucBssIdx;
	uint8_t ucPhyType;
	uint8_t ucMaxCopySize;
	uint8_t *pNameBuf;

	*pu4SetInfoLen = 0;

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(OID, ERROR,
		       "Fail in query receive error! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	prParamGetWifiType = (struct PARAM_GET_WIFI_TYPE *)pvSetBuffer;
	if ((gPrDev != NULL) &&
	    (prParamGetWifiType->prNetDev == gPrDev))
		ucBssIdx = (uint8_t)NETWORK_TYPE_AIS_INDEX;
	else if ((g_P2pPrDev != NULL) &&
		 (prParamGetWifiType->prNetDev == g_P2pPrDev))
		ucBssIdx = (uint8_t)NETWORK_TYPE_P2P_INDEX;
	else {
		DBGLOG(OID, ERROR,
		       "network type index error! %p, %p, %p\n",
		       prParamGetWifiType->prNetDev, gPrDev, g_P2pPrDev);
		return WLAN_STATUS_INVALID_DATA;
	}

	DBGLOG(OID, INFO, "bss index=%d\n", ucBssIdx);

	kalMemZero(prParamGetWifiType->arWifiTypeName,
		   sizeof(prParamGetWifiType->arWifiTypeName));
	pNameBuf = &prParamGetWifiType->arWifiTypeName[0];
	ucMaxCopySize = sizeof(prParamGetWifiType->arWifiTypeName) - 1;

	if (ucBssIdx >= NETWORK_TYPE_INDEX_NUM) {
		DBGLOG(OID, ERROR, "invalid bss index: %u\n", ucBssIdx);
		return WLAN_STATUS_INVALID_DATA;
	}

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIdx);
	if ((!prBssInfo) || (!IS_BSS_ACTIVE(prBssInfo))) {
		DBGLOG(OID, ERROR, "invalid BssInfo: %p, %u\n",
		       prBssInfo, ucBssIdx);
		return WLAN_STATUS_INVALID_DATA;
	}

	ucPhyType = prBssInfo->ucPhyTypeSet;
	if (ucPhyType & PHY_TYPE_SET_802_11N)
		kalStrnCpy(pNameBuf, "11N", ucMaxCopySize);
	else if (ucPhyType & PHY_TYPE_SET_802_11B)
		kalStrnCpy(pNameBuf, "11B", ucMaxCopySize);
	else if (ucPhyType & PHY_TYPE_SET_802_11G)
		kalStrnCpy(pNameBuf, "11G", ucMaxCopySize);
	else if (ucPhyType & PHY_TYPE_SET_802_11A)
		kalStrnCpy(pNameBuf, "11A", ucMaxCopySize);
	else
		DBGLOG(OID, INFO,
		       "unknown wifi type, prBssInfo->ucPhyTypeSet: %u\n",
		       ucPhyType);

	*pu4SetInfoLen = kalStrLen(pNameBuf);

	DBGLOG(OID, INFO, "wifi type=[%s](%d), phyType=%u\n",
	       pNameBuf, *pu4SetInfoLen, ucPhyType);

	return WLAN_STATUS_SUCCESS;
}

WLAN_STATUS
wlanoidConfigRoaming(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer,
		     IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	struct nlattr *attrlist;
	struct AIS_BLACKLIST_ITEM *prBlackList;
	P_BSS_DESC_T prBssDesc = NULL;
	UINT_32 len_shift = 0;
	UINT_32 numOfList[2] = { 0 };
	int i;

	attrlist = (struct nlattr *)pvSetBuffer;

	/* get the number of blacklist and copy those mac addresses from HAL */
	if (attrlist->nla_type == WIFI_ATTRIBUTE_ROAMING_BLACKLIST_NUM)
		numOfList[0] = nla_get_u32(attrlist);

	DBGLOG(REQ, INFO, "Get the number of blacklist=%d\n", numOfList[0]);
	/*Refresh all the FWKBlacklist */
	aisRefreshFWKBlacklist(prAdapter);

	if (numOfList[0] < 0 || numOfList[0] > MAX_FW_ROAMING_BLACKLIST_SIZE)
		return -EINVAL;

	/* Start to receive blacklist mac addresses and set to FWK blacklist */
	for (i = 0; i < numOfList[0] && len_shift < u4SetBufferLen; i++) {
		len_shift += NLA_ALIGN(attrlist->nla_len);
		attrlist = (struct nlattr *)((UINT_8 *) pvSetBuffer +
								len_shift);
		if (attrlist->nla_type ==
		    WIFI_ATTRIBUTE_ROAMING_BLACKLIST_BSSID) {
			prBssDesc = scanSearchBssDescByBssid(prAdapter,
					  nla_data(attrlist));

			if (prBssDesc == NULL) {
				DBGLOG(REQ, ERROR,
				       "Cannot find the blacklist BSS=%pM\n",
				       nla_data(attrlist));
				continue;
			}

			prBlackList = aisAddBlacklist(prAdapter, prBssDesc);
			prBlackList->fgIsInFWKBlacklist = TRUE;
			DBGLOG(REQ, INFO,
			       "Receives roaming blacklist SSID=%s addr=%pM len=%d %d\n",
			       prBlackList->aucSSID, prBlackList->aucBSSID,
			       len_shift, u4SetBufferLen);
		}
	}

	return WLAN_STATUS_SUCCESS;
}

uint32_t
wlanoidExternalAuthDone(IN P_ADAPTER_T prAdapter,
			IN void *pvSetBuffer,
			IN uint32_t u4SetBufferLen,
			OUT uint32_t *pu4SetInfoLen)
{
	P_STA_RECORD_T prStaRec = NULL;
	uint8_t ucBssIndex = 0;
	struct PARAM_EXTERNAL_AUTH *params;
	struct MSG_SAA_EXTERNAL_AUTH_DONE *prExternalAuthMsg = NULL;

	params = (struct PARAM_EXTERNAL_AUTH *) pvSetBuffer;
	ucBssIndex = params->ucBssIdx;
	if (!IS_BSS_INDEX_VALID(ucBssIndex)) {
		DBGLOG(REQ, ERROR,
		       "SAE-confirm failed with invalid BssIdx in ndev\n");
		return WLAN_STATUS_INVALID_DATA;
	}

	prExternalAuthMsg = (struct MSG_SAA_EXTERNAL_AUTH_DONE *)cnmMemAlloc(
			    prAdapter, RAM_TYPE_MSG,
			    sizeof(struct MSG_SAA_EXTERNAL_AUTH_DONE));
	if (!prExternalAuthMsg) {
		DBGLOG(OID, WARN,
		       "SAE-confirm failed to allocate Msg\n");
		return WLAN_STATUS_RESOURCES;
	}

	prStaRec = cnmGetStaRecByAddress(prAdapter,
					 (UINT_8) NETWORK_TYPE_AIS_INDEX,
					 params->bssid);
	if (!prStaRec) {
		DBGLOG(REQ, WARN, "SAE-confirm failed with bssid:" MACSTR "\n",
		       params->bssid);
		return WLAN_STATUS_INVALID_DATA;
	}

	prExternalAuthMsg->rMsgHdr.eMsgId = MID_OID_SAA_FSM_EXTERNAL_AUTH;
	prExternalAuthMsg->prStaRec = prStaRec;
	prExternalAuthMsg->status = params->status;

	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T)prExternalAuthMsg,
		    MSG_SEND_METHOD_BUF);

	return WLAN_STATUS_SUCCESS;
}

uint32_t
wlanoidSetScanMacOui(IN P_ADAPTER_T prAdapter,
		IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		OUT uint32_t *pu4SetInfoLen)
{
	P_BSS_INFO_T prBssInfo = NULL;

	ASSERT(prAdapter);
	ASSERT(prAdapter->prGlueInfo);
	ASSERT(u4SetBufferLen == MAC_OUI_LEN);

	prBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);

	if (!pvSetBuffer) {
		DBGLOG(REQ, WARN, "Buffer is NULL!\n");
		return WLAN_STATUS_FAILURE;
	}

	kalMemCopy(prBssInfo->ucScanOui, pvSetBuffer, MAC_OUI_LEN);
	prBssInfo->fgIsScanOuiSet = TRUE;
	*pu4SetInfoLen = MAC_OUI_LEN;

	return WLAN_STATUS_SUCCESS;
}
