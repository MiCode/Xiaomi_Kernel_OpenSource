/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

/*
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/common/wlan_oid.c#11
*/

/*
 * ! \file wlanoid.c
 * \brief This file contains the WLAN OID processing routines of Windows driver for
 *      MediaTek Inc. 802.11 Wireless LAN Adapters.
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
#include "debug.h"
#include <stddef.h>

#ifdef FW_CFG_SUPPORT
#include "fwcfg.h"
#endif
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
const struct TXPWR_LIMIT_SAR_T g_aucSarTableMainAnt[CFG_MAX_SAR_TABLE_SIZE] = {
/*
*	iwpriv wlan0 driver "O-SAR-ENABLE 0"
*	function off
*
*	iwpriv wlan0 driver "O-SAR-ENABLE 1"
*	function on
*	use g_aucSarTableMainAnt[0] parameter.
*
*	iwpriv wlan0 driver "O-SAR-ENABLE 16"
*	function on
*	use g_aucSarTableMainAnt[15] parameter.
*/
	{0x01,	{0x01, 0x01, 0x01, 0x01}	},/*should reserve for old version*/
	{0x02,	{0x02, 0x03, 0x04, 0x05}	},/*should reserve for old version*/
	{0x26,	{0x15, 0x15, 0x15, 0x15}	},/*already used*/
	{0x20,	{0x13, 0x13, 0x13, 0x13}	},/*already used*/

	{0x00,	{0x00, 0x00, 0x00, 0x00}	},/*already used*/
	{0x26,	{0x1F, 0x1F, 0x1F, 0x1F}	},/*already used*/
	{0x07,	{0x02, 0x03, 0x04, 0x05}	},
	{0x08,	{0x02, 0x03, 0x04, 0x05}	},

	{0x09,	{0x03, 0x03, 0x03, 0x03}	},
	{0x0a,	{0x02, 0x03, 0x04, 0x05}	},
	{0x0b,	{0x02, 0x03, 0x04, 0x05}	},
	{0x0c,	{0x02, 0x03, 0x04, 0x05}	},

	{0x0d,	{0x02, 0x03, 0x04, 0x05}	},
	{0x0e,	{0x02, 0x03, 0x04, 0x05}	},
	{0x0f,	{0x02, 0x03, 0x04, 0x05}	},
	{0x00,	{0x06, 0x06, 0x06, 0x06}	}
};

const struct TXPWR_LIMIT_SAR_T g_aucSarTableAuxiliaryAnt[CFG_MAX_SAR_TABLE_SIZE] = {
/*
*	iwpriv wlan0 driver "O-SAR-ENABLE 0"
*	function off
*
*	iwpriv wlan0 driver "O-SAR-ENABLE 1"
*	function on
*	use g_aucSarTableAuxiliaryAnt[0] parameter.
*
*	iwpriv wlan0 driver "O-SAR-ENABLE 16"
*	function on
*	use g_aucSarTableAuxiliaryAnt[15] parameter.
*/
	{0x01,	{0x01, 0x04, 0x01, 0x01}	},/*should reserve for old version*/
	{0x02,	{0x02, 0x07, 0x04, 0x05}	},/*should reserve for old version*/
	{0x26,	{0x19, 0x19, 0x19, 0x19}	},/*already used*/
	{0x20,	{0x15, 0x15, 0x15, 0x15}	},/*already used*/

	{0x00,	{0x00, 0x00, 0x00, 0x00}	},/*already used*/
	{0x26,	{0x1F, 0x1F, 0x1F, 0x1F}	},/*already used*/
	{0x07,	{0x02, 0x07, 0x04, 0x05}	},
	{0x08,	{0x02, 0x07, 0x04, 0x05}	},

	{0x09,	{0x03, 0x07, 0x03, 0x03}	},
	{0x0a,	{0x02, 0x07, 0x04, 0x05}	},
	{0x0b,	{0x02, 0x07, 0x04, 0x05}	},
	{0x0c,	{0x02, 0x07, 0x04, 0x05}	},

	{0x0d,	{0x02, 0x07, 0x04, 0x05}	},
	{0x0e,	{0x02, 0x07, 0x04, 0x05}	},
	{0x0f,	{0x02, 0x07, 0x04, 0x05}	},
	{0x00,	{0x06, 0x07, 0x06, 0x06}	}
};
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
		DBGLOG(OID, WARN,
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
		DBGLOG(OID, WARN,
		       "Fail in set BSSID list scan! (Adapter not ready). ACPI=D%d, Radio=%d\n",
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
		DBGLOG(OID, ERROR,
		       "Fail in set BSSID list scan! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (prAdapter->fgTestMode) {
		DBGLOG(OID, WARN, "didn't support Scan in test mode\n");
		return WLAN_STATUS_FAILURE;
	}

	ASSERT(pu4SetInfoLen);
	*pu4SetInfoLen = 0;

	if (u4SetBufferLen != sizeof(PARAM_SCAN_REQUEST_EXT_T))
		return WLAN_STATUS_INVALID_LENGTH;

	if (prAdapter->fgIsRadioOff) {
		DBGLOG(OID, INFO, "Return from BSSID list scan! (radio off). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_SUCCESS;
	}
	DBGLOG(OID, TRACE, "ScanEX\n");
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
	struct _PARAM_SCAN_RANDOM_MAC_ADDR_T rScanRandMacAddr;
	PARAM_SSID_T rSsid[CFG_SCAN_SSID_MAX_NUM];
	PUINT_8 pucIe;
	UINT_8 ucSsidNum;
	UINT_32 i, u4IeLength;

	DEBUGFUNC("wlanoidSetBssidListScanAdv()");

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(OID, WARN,
		       "Fail in set BSSID list scan! (Adapter not ready). ACPI=D%d, Radio=%d\n",
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
		DBGLOG(OID, WARN, "Return from BSSID list scan! (radio off). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_SUCCESS;
	}

	prScanRequest = (P_PARAM_SCAN_REQUEST_ADV_T) pvSetBuffer;
	kalMemCopy(&rScanRandMacAddr, &(prScanRequest->rScanRandomMacAddr),
				sizeof(struct _PARAM_SCAN_RANDOM_MAC_ADDR_T));
	ucSsidNum = (UINT_8) (prScanRequest->u4SsidNum);
	for (i = 0; i < prScanRequest->u4SsidNum; i++) {
		if (prScanRequest->rSsid[i].u4SsidLen > ELEM_MAX_LEN_SSID) {
			DBGLOG(OID, ERROR,
			       "[%s] SSID(%s) Length(%u) is over than ELEM_MAX_LEN_SSID(%d)\n",
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
			if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) != PARAM_MEDIA_STATE_CONNECTED) {
				aisFsmScanRequestAdv(prAdapter, ucSsidNum, rSsid, pucIe, u4IeLength,
					prScanRequest->ucSetChannel, &rScanRandMacAddr);

			} else
				return WLAN_STATUS_FAILURE;
		} else
			return WLAN_STATUS_FAILURE;
	} else
#endif
	{
		if (prAdapter->fgEnOnlineScan == TRUE) {
			aisFsmScanRequestAdv(prAdapter, ucSsidNum, rSsid, pucIe, u4IeLength,
				prScanRequest->ucSetChannel, &rScanRandMacAddr);
		} else if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) != PARAM_MEDIA_STATE_CONNECTED) {
			aisFsmScanRequestAdv(prAdapter, ucSsidNum, rSsid, pucIe, u4IeLength,
				prScanRequest->ucSetChannel, &rScanRandMacAddr);
		} else
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
		ASSERT(0);
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
	DBGLOG(OID, INFO, "SSID %s\n", prAdapter->rWifiVar.rConnSettings.aucSSID);

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
	kalMemZero(prConnSettings->aucBSSID, sizeof(prConnSettings->aucBSSID));
	prConnSettings->eConnectionPolicy = CONNECT_BY_SSID_ANY;
	prConnSettings->fgIsConnByBssidIssued = FALSE;

	if (pParamConn->pucSsid) {
		prConnSettings->eConnectionPolicy = CONNECT_BY_SSID_BEST_RSSI;
		COPY_SSID(prConnSettings->aucSSID,
			  prConnSettings->ucSSIDLen, pParamConn->pucSsid, (UINT_8) pParamConn->u4SsidLen);
		if (EQUAL_SSID
		    (prAdapter->rWlanInfo.rCurrBssId.rSsid.aucSsid,
		     prAdapter->rWlanInfo.rCurrBssId.rSsid.u4SsidLen, pParamConn->pucSsid, pParamConn->u4SsidLen))
			fgEqualSsid = TRUE;
	}
	if (pParamConn->pucBssid) {
		if (!EQUAL_MAC_ADDR(aucZeroMacAddr, pParamConn->pucBssid)
		    && IS_UCAST_MAC_ADDR(pParamConn->pucBssid)) {
			prConnSettings->eConnectionPolicy = CONNECT_BY_BSSID;
			prConnSettings->fgIsConnByBssidIssued = TRUE;
			COPY_MAC_ADDR(prConnSettings->aucBSSID, pParamConn->pucBssid);
			if (EQUAL_MAC_ADDR(prAdapter->rWlanInfo.rCurrBssId.arMacAddress, pParamConn->pucBssid))
				fgEqualBssid = TRUE;
		} else
			DBGLOG(OID, INFO, "wrong bssid " MACSTR "to connect\n", MAC2STR(pParamConn->pucBssid));
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
			DBGLOG(OID, INFO, "DisBySsid\n");
			kalIndicateStatusAndComplete(prGlueInfo, WLAN_STATUS_MEDIA_DISCONNECT, NULL, 0);
			prAisAbortMsg->ucReasonOfDisconnect = DISCONNECT_REASON_CODE_NEW_CONNECTION;
			cnmMemFree(prAdapter, prAisAbortMsg);
			/* reject this connect to avoid to install key fail */
			return WLAN_STATUS_FAILURE;
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
		if (pParamConn->u4SsidLen == ELEM_MAX_LEN_SSID && pParamConn->pucSsid) {
			fgIsValidSsid = FALSE;

			for (i = 0; i < ELEM_MAX_LEN_SSID; i++) {
				if (!((pParamConn->pucSsid[i] > 0)
				      && (pParamConn->pucSsid[i] <= 0x1F))) {
					fgIsValidSsid = TRUE;
					break;
				}
			}
		}
	}

	/* Set Connection Request Issued Flag */
	if (fgIsValidSsid) {
		prConnSettings->fgIsConnReqIssued = TRUE;
	} else {
		prConnSettings->eReConnectLevel = RECONNECT_LEVEL_USER_SET;
		prConnSettings->fgIsConnReqIssued = FALSE;
	}

	if (fgEqualSsid || fgEqualBssid)
		prAisAbortMsg->fgDelayIndication = TRUE;
	else
		/* Update the information to CONNECTION_SETTINGS_T */
		prAisAbortMsg->fgDelayIndication = FALSE;

	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prAisAbortMsg, MSG_SEND_METHOD_BUF);

	DBGLOG(OID, INFO, "ssid %s, bssid " MACSTR ", conn policy %d, disc reason %d\n",
	       prConnSettings->aucSSID, MAC2STR(prConnSettings->aucBSSID),
	       prConnSettings->eConnectionPolicy, prAisAbortMsg->ucReasonOfDisconnect);
	return WLAN_STATUS_SUCCESS;
}				/* end of wlanoidSetConnect */

#if CFG_SUPPORT_ABORT_SCAN
/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to do abort scan
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[in] pvSetBuffer A pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                          bytes read from the set buffer. If the call failed
*                          due to invalid length of the set buffer, returns
*                          the amount of storage needed.
*
* \retval Always WLAN_STATUS_SUCCESS
*
* \note The setting buffer NULL
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetAbortScan(IN P_ADAPTER_T prAdapter,
		IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	ASSERT(prAdapter);

	/* Do abort SCAN */
	if (AisFsmGetScanState(prAdapter)) {
		DBGLOG(OID, INFO, "ABORT scan on scanning state by user\n");
		aisFsmStateAbort_SCAN(prAdapter);
	} else {
		DBGLOG(OID, INFO, "ABORT scan need remove cmd from queue by user\n");
		prAdapter->fgAbortScan = TRUE;
	}
	return WLAN_STATUS_SUCCESS;
}
#endif

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
	if (pu4QueryInfoLen == NULL)
		return WLAN_STATUS_FAILURE;

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
		DBGLOG(OID, WARN,
		       "Fail in set infrastructure mode! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	eOpMode = *(P_ENUM_PARAM_OP_MODE_T) pvSetBuffer;
	/* Verify the new infrastructure mode. */
	if (eOpMode >= NET_TYPE_NUM) {
		DBGLOG(OID, TRACE, "Invalid mode value %d\n", eOpMode);
		return WLAN_STATUS_INVALID_DATA;
	}

	/* check if possible to switch to AdHoc mode */
	if (eOpMode == NET_TYPE_IBSS || eOpMode == NET_TYPE_DEDICATED_IBSS) {
		if (cnmAisIbssIsPermitted(prAdapter) == FALSE) {
			DBGLOG(OID, TRACE, "Mode value %d unallowed\n", eOpMode);
			return WLAN_STATUS_FAILURE;
		}
	}

	/* Save the new infrastructure mode setting. */
	prAdapter->rWifiVar.rConnSettings.eOPMode = eOpMode;

	prAdapter->rWifiVar.rConnSettings.fgWapiMode = FALSE;

#if CFG_SUPPORT_802_11W
	prAdapter->rWifiVar.rAisSpecificBssInfo.fgMgmtProtection = FALSE;
	prAdapter->rWifiVar.rAisSpecificBssInfo.fgBipKeyInstalled = FALSE;
#endif

#if 0				/* STA record remove at AIS_ABORT nicUpdateBss and DISCONNECT */
	for (i = 0; i < BSS_INFO_NUM; i++) {
		prBssInfo = prAdapter->aprBssInfo[i];
		if (prBssInfo->eNetworkType == NETWORK_TYPE_AIS)
			cnmStaFreeAllStaByNetwork(prAdapter, prBssInfo->ucBssIndex, 0);
	}
#endif

	/* Clean up the Tx key flag */
	prAdapter->prAisBssInfo->fgTxBcKeyExist = FALSE;
	prAdapter->prAisBssInfo->ucTxDefaultKeyID = 0;
	prAdapter->prAisBssInfo->ucCurrentGtkId = 0;

	/* prWlanTable = prAdapter->rWifiVar.arWtbl; */
	/* prWlanTable[prAdapter->prAisBssInfo->ucBMCWlanIndex].ucKeyId = 0; */

#if DBG
	DBGLOG(RSN, TRACE, "wlanoidSetInfrastructureMode\n");
#endif

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_INFRASTRUCTURE,
				   TRUE,
				   FALSE,
				   TRUE,
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
		DBGLOG(OID, WARN,
		       "Fail in set Authentication mode! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	/* Check if the new authentication mode is valid. */
	if (*(P_ENUM_PARAM_AUTH_MODE_T) pvSetBuffer >= AUTH_MODE_NUM) {
		DBGLOG(OID, TRACE, "Invalid auth mode %d\n", *(P_ENUM_PARAM_AUTH_MODE_T) pvSetBuffer);
		return WLAN_STATUS_INVALID_DATA;
	}

	switch (*(P_ENUM_PARAM_AUTH_MODE_T) pvSetBuffer) {
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
		DBGLOG(OID, WARN,
		       "Fail in set Authentication mode! (Adapter not ready). ACPI=D%d, Radio=%d\n",
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
		DBGLOG(OID, WARN,
		       "Fail in set Reload default! (Adapter not ready). ACPI=D%d, Radio=%d\n",
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
			prCmdInfo->ucBssIndex = prAdapter->prAisBssInfo->ucBssIndex;
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
		DBGLOG(OID, WARN,
		       "Invalid key structure length (%d) greater than total buffer length (%d)\n",
		       (UINT_8) prNewWepKey->u4Length, (UINT_8) u4SetBufferLen);

		*pu4SetInfoLen = u4SetBufferLen;
		return WLAN_STATUS_INVALID_DATA;
	}

	/* Verify the key material length for maximum key material length:16 */
	if (prNewWepKey->u4KeyLength > 16 /* LEGACY_KEY_MAX_LEN */) {
		DBGLOG(OID, WARN,
		       "Invalid key material length (%d) greater than maximum key material length (16)\n",
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
		DBGLOG(OID, WARN,
		       "Fail in set remove WEP! (Adapter not ready). ACPI=D%d, Radio=%d\n",
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
	P_WLAN_TABLE_T prWlanTable;
	P_STA_RECORD_T prStaRec = NULL;
	BOOL fgAddTxBcKey = FALSE;

#if CFG_SUPPORT_TDLS
	P_STA_RECORD_T prTmpStaRec;
#endif

	DEBUGFUNC("wlanoidSetAddKey");

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
		DBGLOG(OID, WARN,
		       "Invalid key structure length (%d) greater than total buffer length (%d)\n",
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
		    ((prNewKey->arBSSID[0] == 0xff) && (prNewKey->arBSSID[1] == 0xff)
		     && (prNewKey->arBSSID[2] == 0xff) && (prNewKey->arBSSID[3] == 0xff)
		     && (prNewKey->arBSSID[4] == 0xff) && (prNewKey->arBSSID[5] == 0xff))) {
			return WLAN_STATUS_INVALID_DATA;
		}
	}

	*pu4SetInfoLen = u4SetBufferLen;

	/* Dump PARAM_KEY content. */
	DBGLOG(OID, INFO, "PARAM_KEY Length: 0x%x, Key Index: 0x%x, Key Length: 0x%x, BSSID: "MACSTR"\n",
	       prNewKey->u4Length, prNewKey->u4KeyIndex, prNewKey->u4KeyLength,
	       MAC2STR(prNewKey->arBSSID));
	DBGLOG(OID, TRACE, "Key RSC:\n");
	DBGLOG_MEM8(OID, TRACE, &prNewKey->rKeyRSC, sizeof(PARAM_KEY_RSC));
	DBGLOG(OID, TRACE, "Key Material:\n");
	DBGLOG_MEM8(OID, TRACE, prNewKey->aucKeyMaterial, prNewKey->u4KeyLength);

	prAisSpecBssInfo = &prAdapter->rWifiVar.rAisSpecificBssInfo;
	prBssInfo = prAdapter->prAisBssInfo;

	if (prAdapter->rWifiVar.rConnSettings.eAuthMode >= AUTH_MODE_WPA &&
	    prAdapter->rWifiVar.rConnSettings.eAuthMode != AUTH_MODE_WPA_NONE) {
		if ((prNewKey->arBSSID[0] & prNewKey->arBSSID[1] & prNewKey->arBSSID[2] &
		     prNewKey->arBSSID[3] & prNewKey->arBSSID[4] & prNewKey->arBSSID[5]) == 0xFF) {
			prStaRec = cnmGetStaRecByAddress(prAdapter, prBssInfo->ucBssIndex, prBssInfo->aucBSSID);
		} else {
			prStaRec = cnmGetStaRecByAddress(prAdapter, prBssInfo->ucBssIndex, prNewKey->arBSSID);
		}
		if (!prStaRec) {	/* Already disconnected ? */
			DBGLOG(OID, INFO, "[wlan] Not set the WPA key while disconnect\n");
			return WLAN_STATUS_SUCCESS;
		}
	}

	prWlanTable = prAdapter->rWifiVar.arWtbl;

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
	prCmdInfo->ucBssIndex = prBssInfo->ucBssIndex;
	prCmdInfo->u2InfoBufLen = CMD_HDR_SIZE + sizeof(CMD_802_11_KEY);
#if CFG_SUPPORT_REPLAY_DETECTION
	prCmdInfo->pfCmdDoneHandler = nicCmdEventSetAddKey;
	prCmdInfo->pfCmdTimeoutHandler = nicOidCmdTimeoutSetAddKey;
#else
	prCmdInfo->pfCmdDoneHandler = nicCmdEventSetCommon;
	prCmdInfo->pfCmdTimeoutHandler = nicOidCmdTimeoutCommon;
#endif
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

	kalMemCopy(prCmdKey->aucPeerAddr, (PUINT_8) prNewKey->arBSSID, MAC_ADDR_LEN);
	kalMemCopy(prCmdKey->aucKeyRsc, (PUINT_8) & prNewKey->rKeyRSC, sizeof(PARAM_KEY_RSC));

	prCmdKey->ucBssIdx = prBssInfo->ucBssIndex;	/* AIS BSS */

	prCmdKey->ucKeyId = (UINT_8) (prNewKey->u4KeyIndex & 0xff);

	/* Note: adjust the key length for WPA-None */
	prCmdKey->ucKeyLen = (UINT_8) prNewKey->u4KeyLength;

	kalMemCopy(prCmdKey->aucKeyMaterial, (PUINT_8) prNewKey->aucKeyMaterial, prCmdKey->ucKeyLen);

	if (prNewKey->u4KeyLength == 5) {
		prCmdKey->ucAlgorithmId = CIPHER_SUITE_WEP40;
	} else if (prNewKey->u4KeyLength == 13) {
		prCmdKey->ucAlgorithmId = CIPHER_SUITE_WEP104;
	} else if (prNewKey->u4KeyLength == 16) {
		if (prAdapter->rWifiVar.rConnSettings.eAuthMode < AUTH_MODE_WPA)
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
			{
				prCmdKey->ucAlgorithmId = CIPHER_SUITE_CCMP;
				if (rsnCheckPmkidCandicate(prAdapter)) {

					DBGLOG(RSN, TRACE,
					       "Add key: Prepare a timer to indicate candidate PMKID Candidate\n");
					cnmTimerStopTimer(prAdapter, &prAisSpecBssInfo->rPreauthenticationTimer);
					cnmTimerStartTimer(prAdapter,
							   &prAisSpecBssInfo->rPreauthenticationTimer,
							   SEC_TO_MSEC(WAIT_TIME_IND_PMKID_CANDICATE_SEC));
				}
			}
		}

	} else if (prNewKey->u4KeyLength == 32) {
		if (prAdapter->rWifiVar.rConnSettings.eAuthMode == AUTH_MODE_WPA_NONE) {
			if (prAdapter->rWifiVar.rConnSettings.eEncStatus == ENUM_ENCRYPTION2_ENABLED)
				prCmdKey->ucAlgorithmId = CIPHER_SUITE_TKIP;
			else if (prAdapter->rWifiVar.rConnSettings.eEncStatus == ENUM_ENCRYPTION3_ENABLED) {
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
					       "Add key: Prepare a timer to indicate candidate PMKID Candidate\n");
					cnmTimerStopTimer(prAdapter, &prAisSpecBssInfo->rPreauthenticationTimer);
					cnmTimerStartTimer(prAdapter,
							   &prAisSpecBssInfo->rPreauthenticationTimer,
							   SEC_TO_MSEC(WAIT_TIME_IND_PMKID_CANDICATE_SEC));
				}
			} else {
				prCmdKey->ucAlgorithmId = CIPHER_SUITE_TKIP;
			}
		}
	}

	{
		if ((prCmdKey->aucPeerAddr[0] & prCmdKey->aucPeerAddr[1] & prCmdKey->aucPeerAddr[2] & prCmdKey->
		     aucPeerAddr[3] & prCmdKey->aucPeerAddr[4] & prCmdKey->aucPeerAddr[5]) == 0xFF) {
			if (prAdapter->rWifiVar.rConnSettings.eAuthMode >= AUTH_MODE_WPA
			    && prAdapter->rWifiVar.rConnSettings.eAuthMode != AUTH_MODE_WPA_NONE
			    && 1 /* Connected */) {
				prStaRec = cnmGetStaRecByAddress(prAdapter, prBssInfo->ucBssIndex, prBssInfo->aucBSSID);
				ASSERT(prStaRec);	/* AIS RSN Group key, addr is BC addr */
				kalMemCopy(prCmdKey->aucPeerAddr, prStaRec->aucMacAddr, MAC_ADDR_LEN);
			} else {
				prStaRec = NULL;
			}
		} else {
			prStaRec = cnmGetStaRecByAddress(prAdapter, prBssInfo->ucBssIndex, prCmdKey->aucPeerAddr);
		}

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
		if (prCmdKey->ucAlgorithmId == CIPHER_SUITE_BIP) {
			ASSERT(prStaRec);
			prCmdKey->ucWlanIndex = prStaRec->ucBMCWlanIndex;
			/* prCmdKey->ucKeyId; */
		} else
#endif
		if (prCmdKey->ucTxKey) {
			if (prStaRec) {
				if (prCmdKey->ucKeyType) {	/* AIS RSN STA */
					prCmdKey->ucWlanIndex = prStaRec->ucWlanIndex;
					prStaRec->fgTransmitKeyExist = TRUE;	/* wait for CMD Done ? */
				} else {
					ASSERT(FALSE);
					/* prCmdKey->ucWlanIndex = */
					/*      secPrivacySeekForBcEntry(prAdapter, */
					/*              prBssInfo->ucBssIndex, */
					/*              NETWORK_TYPE_AIS, */
					/*              prCmdKey->aucPeerAddr, */
					/*              prCmdKey->ucAlgorithmId, */
					/*              prCmdKey->ucKeyId, */
					/*              prStaRec->ucCurrentGtkId, */
					/*              BIT(1)); */
					/* Todo:: Check the prCmdKey->ucKeyType */
					/* for some case, like wep, add bc wep key before sta create, */
					/* so use the rAisSpecificBssInfo to save key setting */
					fgAddTxBcKey = TRUE;
				}
			}
			if (fgAddTxBcKey || !prStaRec) {

				if ((prCmdKey->aucPeerAddr[0] & prCmdKey->aucPeerAddr[1] & prCmdKey->
				     aucPeerAddr[2] & prCmdKey->aucPeerAddr[3] & prCmdKey->aucPeerAddr[4] & prCmdKey->
				     aucPeerAddr[5]) == 0xFF) {
					prCmdKey->ucWlanIndex = 255;	/* AIS WEP Tx key */
				} else {	/* Exist this case ? */
					ASSERT(FALSE);
					/* prCmdKey->ucWlanIndex = */
					/*      secPrivacySeekForBcEntry(prAdapter, */
					/*      prBssInfo->ucBssIndex, */
					/*      NETWORK_TYPE_AIS, */
					/*      prCmdKey->aucPeerAddr, */
					/*      prCmdKey->ucAlgorithmId, */
					/*      prCmdKey->ucKeyId, */
					/*      prBssInfo->ucCurrentGtkId, */
					/*      BIT(1)); */
				}

				prBssInfo->fgTxBcKeyExist = TRUE;
				prBssInfo->ucBMCWlanIndex = prCmdKey->ucWlanIndex;	/* Saved for AIS WEP */
				prBssInfo->ucTxDefaultKeyID = prCmdKey->ucKeyId;

			}
		} else {
			/* Including IBSS RSN Rx BC key ? */
			if ((prCmdKey->aucPeerAddr[0] & prCmdKey->aucPeerAddr[1] & prCmdKey->aucPeerAddr[2] & prCmdKey->
			     aucPeerAddr[3] & prCmdKey->aucPeerAddr[4] & prCmdKey->aucPeerAddr[5]) == 0xFF) {
				prCmdKey->ucWlanIndex = 255;	/* AIS WEP */
			} else {
				if (prStaRec) {	/* AIS RSN Group key but addr is BSSID */
					ASSERT(prStaRec->ucBMCWlanIndex < WTBL_SIZE);
					prCmdKey->ucWlanIndex =
					    secPrivacySeekForBcEntry(prAdapter,
								     prStaRec->ucBssIndex,
								     prStaRec->aucMacAddr,
								     prStaRec->ucIndex,
								     prCmdKey->ucAlgorithmId,
								     prCmdKey->ucKeyId,
								     prStaRec->ucCurrentGtkId, BIT(0));
					prStaRec->ucBMCWlanIndex = prCmdKey->ucWlanIndex;
				} else {	/* Exist this case ? */
					ASSERT(FALSE);
					/* prCmdKey->ucWlanIndex = */
					/*      secPrivacySeekForBcEntry(prAdapter, */
					/*      prBssInfo->ucBssIndex, */
					/*      NETWORK_TYPE_AIS, */
					/*      prCmdKey->aucPeerAddr, */
					/*      prCmdKey->ucAlgorithmId, */
					/*      prCmdKey->ucKeyId, */
					/*      prBssInfo->ucCurrentGtkId, */
					/*      BIT(0)); */
				}
			}
		}

		/* Update Group Key Id after Seek Bc entry */
#if CFG_SUPPORT_802_11W
		if (prCmdKey->ucAlgorithmId == CIPHER_SUITE_BIP)
			;
		else
#endif
		if (!prCmdKey->ucKeyType) {
			if (prStaRec) {
				prStaRec->ucCurrentGtkId = prCmdKey->ucKeyId;
			} else {
				/* AIS WEP */
				prBssInfo->ucCurrentGtkId = prCmdKey->ucKeyId;
			}
		}
#if DBG && 0
		if (prCmdKey->ucWlanIndex < WTBL_SIZE) {
			UINT_8 entry = prCmdKey->ucWlanIndex;
			P_HAL_WTBL_SEC_CONFIG_T prWtblCfg;
			BOOLEAN fgOneTb3 = FALSE;

			/* ASSERT(prWlanTable[prCmdKey->ucWlanIndex].ucUsed == TRUE); */
			/* prWlanTable[prCmdKey->ucWlanIndex].ucBssIndex = prCmdKey->ucBssIdx; */
			/* prWlanTable[prCmdKey->ucWlanIndex].ucKeyId = prCmdKey->ucKeyId; */
			/* kalMemCopy(prWlanTable[prCmdKey->ucWlanIndex].aucMacAddr, */
			/* prCmdKey->aucPeerAddr, */
			/* MAC_ADDR_LEN); */

			prWtblCfg = prAdapter->rWifiVar.arWtblCfg;

			if (prCmdKey->ucAlgorithmId == CIPHER_SUITE_WEP40
			    || prCmdKey->ucAlgorithmId == CIPHER_SUITE_WEP104
			    || prCmdKey->ucAlgorithmId == CIPHER_SUITE_WEP128
			    || prCmdKey->ucAlgorithmId == CIPHER_SUITE_WPI)
				fgOneTb3 = TRUE;

			if (prCmdKey->ucTxKey) {
				if (prStaRec) {
					prWtblCfg[entry].fgRCA2 = 1;
					prWtblCfg[entry].fgRV = 1;
					prWtblCfg[entry].fgIKV = 0;
					prWtblCfg[entry].fgRKV = 1;
					if (fgOneTb3)
						prWtblCfg[entry].fgRCID = 0;
					else
						prWtblCfg[entry].fgRCID = 1;
					prWtblCfg[entry].ucKeyID = prCmdKey->ucKeyId;
					prWtblCfg[entry].fgRCA1 = 1;
#if 0
					if (prCmdKey->ucIsAuthenticator)
						prWtblCfg[entry].fgEvenPN = 0;
					else
#endif
						prWtblCfg[entry].fgEvenPN = 1;
					prWtblCfg[entry].ucMUARIdx = 0x00;	/* Omac */
				} else {
#if 0
					if (prCmdKey->ucIsAuthenticator) {
						prWtblCfg[entry].fgRCA2 = 0;
						prWtblCfg[entry].fgRV = 0;
						prWtblCfg[entry].fgIKV = 0;
						prWtblCfg[entry].fgRKV = 0;
						prWtblCfg[entry].fgRCID = 0;
						prWtblCfg[entry].ucKeyID = prCmdKey->ucKeyId;
						prWtblCfg[entry].fgRCA1 = 0;
						prWtblCfg[entry].fgEvenPN = 0;
					} else
#endif
					{
						prWtblCfg[entry].fgRCA2 = 1;
						prWtblCfg[entry].fgRV = 1;
						prWtblCfg[entry].fgIKV = 0;
						prWtblCfg[entry].fgRKV = 1;

						prCmdKey->ucTxKey =
						    ((prNewKey->u4KeyIndex & IS_TRANSMIT_KEY) ==
						     IS_TRANSMIT_KEY) ? 1 : 0;
						prCmdKey->ucKeyType =
						    ((prNewKey->u4KeyIndex & IS_UNICAST_KEY) == IS_UNICAST_KEY) ? 1 : 0;

						SetRCID(fgOneTb3, &prWtblCfg[entry].fgRCID);
						 /*AOSP*/ prWtblCfg[entry].ucKeyID = prCmdKey->ucKeyId;
						prWtblCfg[entry].fgRCA1 = 0;
						prWtblCfg[entry].fgEvenPN = 1;
					}
				}
			} else {
				prWtblCfg[entry].fgRCA2 = 1;
				prWtblCfg[entry].fgRV = 1;
				prWtblCfg[entry].fgIKV = 0;
				prWtblCfg[entry].fgRKV = 1;
				if (fgOneTb3)
					prWtblCfg[entry].fgRCID = 0;
				else
					prWtblCfg[entry].fgRCID = 1;
				prWtblCfg[entry].ucKeyID = prCmdKey->ucKeyId;
				prWtblCfg[entry].fgRCA1 = 1;
				prWtblCfg[entry].ucMUARIdx = 0x30;
#if 0
				if (prCmdKey->ucIsAuthenticator)
					prWtblCfg[entry].fgEvenPN = 0;
				else
#endif
					prWtblCfg[entry].fgEvenPN = 1;
			}
			secPrivacyDumpWTBL3(prAdapter, entry);
		}
#endif
	}

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
	P_AIS_SPECIFIC_BSS_INFO_T prAisSpecBssInfo;
	P_WLAN_TABLE_T prWlanTable;
	P_STA_RECORD_T prStaRec;
	P_BSS_INFO_T prAisBssInfo;

	DEBUGFUNC("wlanoidSetRemoveKey");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	prAisBssInfo = prAdapter->prAisBssInfo;

	*pu4SetInfoLen = sizeof(PARAM_REMOVE_KEY_T);

	if (u4SetBufferLen < sizeof(PARAM_REMOVE_KEY_T))
		return WLAN_STATUS_INVALID_LENGTH;

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(OID, WARN,
		       "Fail in set remove key! (Adapter not ready). ACPI=D%d, Radio=%d\n",
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

	prGlueInfo = prAdapter->prGlueInfo;
	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, (CMD_HDR_SIZE + sizeof(CMD_802_11_KEY)));

	if (!prCmdInfo) {
		DBGLOG(OID, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}

	prAisSpecBssInfo = &prAdapter->rWifiVar.rAisSpecificBssInfo;
	prWlanTable = prAdapter->rWifiVar.arWtbl;

	/* increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

	/* compose CMD_802_11_KEY cmd pkt */
	prCmdInfo->eCmdType = COMMAND_TYPE_NETWORK_IOCTL;
	prCmdInfo->ucBssIndex = prAdapter->prAisBssInfo->ucBssIndex;
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
	prWifiCmd->u2TxByteCount = prCmdInfo->u2InfoBufLen;
	prWifiCmd->u2PQ_ID = CMD_PQ_ID;
	prWifiCmd->ucPktTypeID = CMD_PACKET_TYPE_ID;
	prWifiCmd->ucCID = prCmdInfo->ucCID;
	prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
	prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;

	prCmdKey = (P_CMD_802_11_KEY) (prWifiCmd->aucBuffer);

	kalMemZero((PUINT_8) prCmdKey, sizeof(CMD_802_11_KEY));

	prCmdKey->ucAddRemove = 0;	/* Remove */
	prCmdKey->ucKeyId = (UINT_8) (prRemovedKey->u4KeyIndex & 0x000000ff);
	kalMemCopy(prCmdKey->aucPeerAddr, (PUINT_8) prRemovedKey->arBSSID, MAC_ADDR_LEN);
	prCmdKey->ucBssIdx = prAdapter->prAisBssInfo->ucBssIndex;

#if CFG_SUPPORT_802_11W
	ASSERT(prCmdKey->ucKeyId < MAX_KEY_NUM + 2);
#else
	/* ASSERT(prCmdKey->ucKeyId < MAX_KEY_NUM); */
#endif

	/* Clean up the Tx key flag */
	prStaRec = cnmGetStaRecByAddress(prAdapter, prAdapter->prAisBssInfo->ucBssIndex, prRemovedKey->arBSSID);

	if (prRemovedKey->u4KeyIndex & IS_UNICAST_KEY) {
		if (prStaRec) {
			prCmdKey->ucKeyType = 1;
			prCmdKey->ucWlanIndex = prStaRec->ucWlanIndex;
			prStaRec->fgTransmitKeyExist = FALSE;
		} else if (prCmdKey->ucKeyId == prAdapter->prAisBssInfo->ucTxDefaultKeyID)
			prAdapter->prAisBssInfo->fgTxBcKeyExist = FALSE;
	} else {
		if (prCmdKey->ucKeyId == prAdapter->prAisBssInfo->ucTxDefaultKeyID)
			prAdapter->prAisBssInfo->fgTxBcKeyExist = FALSE;
	}

	if (!prStaRec) {
		if (prAdapter->rWifiVar.rConnSettings.eAuthMode < AUTH_MODE_WPA)
			prCmdKey->ucWlanIndex = prAdapter->prAisBssInfo->ucBMCWlanIndex;
		else {
			prCmdKey->ucWlanIndex = WTBL_RESERVED_ENTRY;
			/* ASSERT(FALSE); */
		}
	}

	if (prCmdKey->ucAlgorithmId == CIPHER_SUITE_WEP40
	    || prCmdKey->ucAlgorithmId == CIPHER_SUITE_WEP104) {
		/* if (prAdapter->prAisBssInfo->ucTxDefaultKeyID == prCmdKey->ucKeyId) */
		/* secPrivacyFreeForEntry(prAdapter, prCmdKey->ucWlanIndex); */
		/* else */
		/* ; Clear key material only */
	} else {
		DBGLOG(RSN, TRACE, "wlanoidSetRemoveKey\n");
		if (prAisBssInfo->eConnectionState != PARAM_MEDIA_STATE_CONNECTED)
			secPrivacyFreeForEntry(prAdapter, prCmdKey->ucWlanIndex);
	}

	if (prCmdKey->ucKeyId < 4) {	/* BIP */
		/* kalMemZero(prAisSpecBssInfo->aucKeyMaterial[prCmdKey->ucKeyId], 16); */
	}
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

	fgTransmitKeyAvailable = prAdapter->prAisBssInfo->fgTxBcKeyExist;

	switch (prAdapter->rWifiVar.rConnSettings.eEncStatus) {
	case ENUM_ENCRYPTION3_ENABLED:
		if (fgTransmitKeyAvailable)
			eEncStatus = ENUM_ENCRYPTION3_ENABLED;
		else
			eEncStatus = ENUM_ENCRYPTION3_KEY_ABSENT;
		break;

	case ENUM_ENCRYPTION2_ENABLED:
		if (fgTransmitKeyAvailable)
			eEncStatus = ENUM_ENCRYPTION2_ENABLED;
		else
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
		DBGLOG(OID, WARN,
		       "Fail in set encryption status! (Adapter not ready). ACPI=D%d, Radio=%d\n",
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
	}

	return rStatus;
}				/* wlanoidSetEncryptionStatus */

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
	UINT_32 i, j = 0;
	P_PARAM_PMKID_T prPmkid;
	P_AIS_SPECIFIC_BSS_INFO_T prAisSpecBssInfo;

	DEBUGFUNC("wlanoidSetPmkid");

	DBGLOG(OID, TRACE, "wlanoidSetPmkid\n");

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
	for (i = 0; i < prPmkid->u4BSSIDInfoCount; i++) {
		/*
		 * Search for desired BSSID. If desired BSSID is found,
		 * then set the PMKID
		 */
		if (!rsnSearchPmkidEntry(prAdapter, (PUINT_8) prPmkid->arBSSIDInfo[i].arBSSID, &j)) {
			/* No entry found for the specified BSSID, so add one entry */
			if (prAisSpecBssInfo->u4PmkidCacheCount < CFG_MAX_PMKID_CACHE) {
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
			DBGLOG(RSN, TRACE, "Add BSSID " MACSTR " idx=%u PMKID value " MACSTR "\n",
			       MAC2STR(prAisSpecBssInfo->arPmkidCache[j].rBssidInfo.arBSSID), j,
			       MAC2STR(prAisSpecBssInfo->arPmkidCache[j].rBssidInfo.arPMKID));
			prAisSpecBssInfo->arPmkidCache[j].fgPmkidExist = TRUE;
		}
	}
	if (prAdapter->rWifiVar.rConnSettings.fgOkcEnabled) {
		P_BSS_DESC_T prBssDesc = prAdapter->rWifiVar.rAisFsmInfo.prTargetBssDesc;
		P_UINT_8 pucPmkID = NULL;

		if (prPmkid->u4Length & BIT(31)) {
			/* the case that force add pmkid for target Bss Descriptor
			** see mtk_cfg80211_connect
			*/
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
				"set by connect, %pM OKC PMKID %02x%02x%02x%02x%02x%02x%02x%02x...\n",
				prAisSpecBssInfo->arPmkidCache[j].rBssidInfo.arBSSID,
				pucPmkID[0], pucPmkID[1], pucPmkID[2], pucPmkID[3],
				pucPmkID[4], pucPmkID[5], pucPmkID[6], pucPmkID[7]);
			aisFsmRunEventSetOkcPmk(prAdapter);
		} else	if (prBssDesc && rsnSearchPmkidEntry(prAdapter, (PUINT_8)prBssDesc->aucBSSID, &j)) {
			/* the case that normally add pmkid, but just match with target Bss Descriptor */
			pucPmkID = prAisSpecBssInfo->arPmkidCache[j].rBssidInfo.arPMKID;
			DBGLOG(RSN, INFO,
				"%pM OKC PMKID %02x%02x%02x%02x%02x%02x%02x%02x...\n",
				prAisSpecBssInfo->arPmkidCache[j].rBssidInfo.arBSSID,
				pucPmkID[0], pucPmkID[1], pucPmkID[2], pucPmkID[3],
				pucPmkID[4], pucPmkID[5], pucPmkID[6], pucPmkID[7]);
			aisFsmRunEventSetOkcPmk(prAdapter);
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
wlanoidRssiMonitor(IN P_ADAPTER_T prAdapter,
		   OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	PARAM_RSSI_MONITOR_T rRssi;

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

	kalMemZero(&rRssi, sizeof(PARAM_RSSI_MONITOR_T));
#if 0
	if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) == PARAM_MEDIA_STATE_DISCONNECTED)
		return WLAN_STATUS_ADAPTER_NOT_READY;
#endif
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

	return wlanSendSetQueryCmd(prAdapter,
			   CMD_ID_RSSI_MONITOR,
			   TRUE,
			   FALSE,
			   TRUE,
			   nicCmdEventSetCommon,
			   nicOidCmdTimeoutCommon,
			   sizeof(PARAM_RSSI_MONITOR_T), (PUINT_8)&rRssi, NULL, 0);

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
		DBGLOG(OID, WARN,
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

	} else
#endif
	{
		return wlanSendSetQueryCmd(prAdapter,
					   CMD_ID_GET_STATISTICS,
					   FALSE,
					   TRUE,
					   TRUE,
					   nicCmdEventQueryRecvError,
					   nicOidCmdTimeoutCommon, 0, NULL, pvQueryBuffer, u4QueryBufferLen);
	}

	return WLAN_STATUS_SUCCESS;
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
		DBGLOG(OID, WARN,
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

	} else
#endif
	{
		return wlanSendSetQueryCmd(prAdapter,
					   CMD_ID_GET_STATISTICS,
					   FALSE,
					   TRUE,
					   TRUE,
					   nicCmdEventQueryRecvNoBuffer,
					   nicOidCmdTimeoutCommon, 0, NULL, pvQueryBuffer, u4QueryBufferLen);
	}
	return WLAN_STATUS_SUCCESS;
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
		DBGLOG(OID, WARN,
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

	} else
#endif
	{
		return wlanSendSetQueryCmd(prAdapter,
					   CMD_ID_GET_STATISTICS,
					   FALSE,
					   TRUE,
					   TRUE,
					   nicCmdEventQueryRecvCrcError,
					   nicOidCmdTimeoutCommon, 0, NULL, pvQueryBuffer, u4QueryBufferLen);
	}
	return WLAN_STATUS_SUCCESS;
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
	return wlanQueryStatistics(prAdapter, pvQueryBuffer, u4QueryBufferLen, pu4QueryInfoLen, TRUE);

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
	DEBUGFUNC("wlanoidQueryCurrentAddr");
	DBGLOG(OID, LOUD, "\n");

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

#if CFG_SUPPORT_ANT_SWAP
/*----------------------------------------------------------------------------*/
/*! \brief  This routine is called to query antenna swap capability..
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
wlanoidQueryAntSwapCapability(IN P_ADAPTER_T prAdapter,
			IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	DEBUGFUNC("wlanoidQueryAntSwapCapability");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*((PUINT32)pvQueryBuffer) = (UINT_32)prAdapter->fgIsAntSwpSupport;
	*pu4QueryInfoLen = sizeof(UINT_32);

	DBGLOG(INIT, TRACE, "AntSwapCapability Query(%u)-len(%u)\n",
		*((PUINT32)pvQueryBuffer), *pu4QueryInfoLen);

	return WLAN_STATUS_SUCCESS;
} /* wlanoidQueryAntSwapCapability */
#endif

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

#if CFG_SUPPORT_QA_TOOL
#if CFG_SUPPORT_BUFFER_MODE
WLAN_STATUS
wlanoidSetEfusBufferMode(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen,
			 OUT PUINT_32 pu4SetInfoLen)
{
	P_PARAM_CUSTOM_EFUSE_BUFFER_MODE_T prSetEfuseBufModeInfo;
	P_PARAM_CUSTOM_EFUSE_BUFFER_MODE_1_T prSetEfuseBufModeInfo_1;
	CMD_EFUSE_BUFFER_MODE_T rCmdSetEfuseBufModeInfo;
	CMD_EFUSE_BUFFER_MODE_1_T rCmdSetEfuseBufModeInfo_1;
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidSetEfusBufferMode");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	if (prAdapter->rWifiVar.ucEfuseBufferModeCal == TRUE) { /* structure for MT7668 */
		*pu4SetInfoLen = sizeof(PARAM_CUSTOM_EFUSE_BUFFER_MODE_1_T);

		if (u4SetBufferLen < sizeof(PARAM_CUSTOM_EFUSE_BUFFER_MODE_1_T))
			return WLAN_STATUS_INVALID_LENGTH;

	} else {
		*pu4SetInfoLen = sizeof(PARAM_CUSTOM_EFUSE_BUFFER_MODE_T);

		if (u4SetBufferLen < sizeof(PARAM_CUSTOM_EFUSE_BUFFER_MODE_T))
			return WLAN_STATUS_INVALID_LENGTH;
	}

	ASSERT(pvSetBuffer);

	if (prAdapter->rWifiVar.ucEfuseBufferModeCal == TRUE) { /* structure for MT7668 */
		prSetEfuseBufModeInfo_1 = (P_PARAM_CUSTOM_EFUSE_BUFFER_MODE_1_T) pvSetBuffer;
	} else {
		prSetEfuseBufModeInfo = (P_PARAM_CUSTOM_EFUSE_BUFFER_MODE_T) pvSetBuffer;
	}

	if (prAdapter->rWifiVar.ucEfuseBufferModeCal == TRUE) { /* structure for MT7668 */

		rCmdSetEfuseBufModeInfo_1.ucSourceMode = prSetEfuseBufModeInfo_1->ucSourceMode;
		rCmdSetEfuseBufModeInfo_1.ucCount = prSetEfuseBufModeInfo_1->ucCount;
		rCmdSetEfuseBufModeInfo_1.ucCmdType = prSetEfuseBufModeInfo_1->ucCmdType;
		rCmdSetEfuseBufModeInfo_1.ucReserved = prSetEfuseBufModeInfo_1->ucReserved;

		/* kalMemCopy(rCmdSetEfuseBufModeInfo.aBinContent, prSetEfuseBufModeInfo->aBinContent, */
		/*     sizeof(BIN_CONTENT_T) * EFUSE_CONTENT_SIZE); */

		kalMemCopy(rCmdSetEfuseBufModeInfo_1.aBinContent, prSetEfuseBufModeInfo_1->aBinContent,
				sizeof(UINT_8) * EFUSE_CONTENT_SIZE_1);

		rWlanStatus = wlanSendSetQueryExtCmd(prAdapter,
							CMD_ID_LAYER_0_EXT_MAGIC_NUM,
							EXT_CMD_ID_EFUSE_BUFFER_MODE,
							FALSE,
							TRUE,
							TRUE,
							NULL, /* No Tx done function wait until fw ack */
							nicOidCmdTimeoutCommon,
							sizeof(CMD_EFUSE_BUFFER_MODE_1_T),
							(PUINT_8) (&rCmdSetEfuseBufModeInfo_1),
							pvSetBuffer, u4SetBufferLen);
	} else{
		rCmdSetEfuseBufModeInfo.ucSourceMode = prSetEfuseBufModeInfo->ucSourceMode;
		rCmdSetEfuseBufModeInfo.ucCount = prSetEfuseBufModeInfo->ucCount;
		rCmdSetEfuseBufModeInfo.ucReserved[0] = prSetEfuseBufModeInfo->ucReserved[0];
		rCmdSetEfuseBufModeInfo.ucReserved[1] = prSetEfuseBufModeInfo->ucReserved[1];
		kalMemCopy(rCmdSetEfuseBufModeInfo.aBinContent, prSetEfuseBufModeInfo->aBinContent,
			   sizeof(BIN_CONTENT_T) * EFUSE_CONTENT_SIZE);

		rWlanStatus = wlanSendSetQueryExtCmd(prAdapter,
							CMD_ID_LAYER_0_EXT_MAGIC_NUM,
							EXT_CMD_ID_EFUSE_BUFFER_MODE,
							TRUE,
							FALSE,
							TRUE,
							nicCmdEventSetCommon,
							nicOidCmdTimeoutCommon,
							sizeof(CMD_EFUSE_BUFFER_MODE_T),
							(PUINT_8) (&rCmdSetEfuseBufModeInfo),
							pvSetBuffer, u4SetBufferLen);
	}

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

	*pu4SetInfoLen = sizeof(P_PARAM_CUSTOM_ACCESS_EFUSE_T);

	if (u4SetBufferLen < sizeof(P_PARAM_CUSTOM_ACCESS_EFUSE_T))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prSetAccessEfuseInfo = (P_PARAM_CUSTOM_ACCESS_EFUSE_T) pvSetBuffer;

	kalMemSet(&rCmdSetAccessEfuse, 0, sizeof(CMD_ACCESS_EFUSE_T));

	rCmdSetAccessEfuse.u4Address = prSetAccessEfuseInfo->u4Address;
	rCmdSetAccessEfuse.u4Valid = prSetAccessEfuseInfo->u4Valid;


	DBGLOG(INIT, INFO, "wlanoidQueryProcessAccessEfuseRead, address=%d\n", rCmdSetAccessEfuse.u4Address);

	kalMemCopy(rCmdSetAccessEfuse.aucData, prSetAccessEfuseInfo->aucData,
	       sizeof(UINT_8) * 16);

	rWlanStatus = wlanSendSetQueryExtCmd(prAdapter,
					CMD_ID_LAYER_0_EXT_MAGIC_NUM,
					EXT_CMD_ID_EFUSE_ACCESS,
					FALSE,   /* Query Bit:  True->write  False->read*/
					TRUE,
					TRUE,
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
	DBGLOG(INIT, INFO, "wlanoidQueryProcessAccessEfuseWrite\n");


	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(P_PARAM_CUSTOM_ACCESS_EFUSE_T);

	if (u4SetBufferLen < sizeof(P_PARAM_CUSTOM_ACCESS_EFUSE_T))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prSetAccessEfuseInfo = (P_PARAM_CUSTOM_ACCESS_EFUSE_T) pvSetBuffer;

	kalMemSet(&rCmdSetAccessEfuse, 0, sizeof(CMD_ACCESS_EFUSE_T));

	rCmdSetAccessEfuse.u4Address = prSetAccessEfuseInfo->u4Address;
	rCmdSetAccessEfuse.u4Valid = prSetAccessEfuseInfo->u4Valid;

	DBGLOG(INIT, INFO, "wlanoidQueryProcessAccessEfuseWrite, address=%d\n", rCmdSetAccessEfuse.u4Address);


	kalMemCopy(rCmdSetAccessEfuse.aucData, prSetAccessEfuseInfo->aucData,
		sizeof(UINT_8) * 16);

	rWlanStatus = wlanSendSetQueryExtCmd(prAdapter,
					CMD_ID_LAYER_0_EXT_MAGIC_NUM,
					EXT_CMD_ID_EFUSE_ACCESS,
					TRUE,   /* Query Bit:  True->write  False->read*/
					TRUE,
					TRUE,
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

	*pu4SetInfoLen = sizeof(P_PARAM_CUSTOM_EFUSE_FREE_BLOCK_T);

	if (u4SetBufferLen < sizeof(P_PARAM_CUSTOM_EFUSE_FREE_BLOCK_T))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prGetEfuseFreeBlockInfo = (P_PARAM_CUSTOM_EFUSE_FREE_BLOCK_T) pvSetBuffer;

	kalMemSet(&rCmdGetEfuseFreeBlock, 0, sizeof(CMD_EFUSE_FREE_BLOCK_T));


	rWlanStatus = wlanSendSetQueryExtCmd(prAdapter,
					CMD_ID_LAYER_0_EXT_MAGIC_NUM,
					EXT_CMD_ID_EFUSE_FREE_BLOCK,
					TRUE,   /* Query Bit:  True->write  False->read*/
					TRUE,
					TRUE,
					NULL, /* No Tx done function wait until fw ack */
					nicOidCmdTimeoutCommon,
					sizeof(CMD_EFUSE_FREE_BLOCK_T),
					(PUINT_8) (&rCmdGetEfuseFreeBlock), pvSetBuffer, u4SetBufferLen);

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

	DBGLOG(INIT, ERROR, "wlanoidQueryRxStatistics\n");

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
					      TRUE,
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
					     TRUE,
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
					     TRUE,
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
	/*prWifiCmd->u2Length = prCmdInfo->u2InfoBufLen - (UINT_16) OFFSET_OF(WIFI_CMD_T, u2Length);*/
	/*prWifiCmd->u2PqId = CMD_PQ_ID;*/
	prWifiCmd->ucPktTypeID = CMD_PACKET_TYPE_ID;
	prWifiCmd->ucCID = prCmdInfo->ucCID;
	/*prWifiCmd->ucExtenCID = ucExtCID;*/
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
					     TRUE,
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
					     TRUE,
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
					     TRUE,
					     nicCmdEventSetCommon,
					     nicOidCmdTimeoutCommon,
					     (CMD_STAREC_UPDATE_HDR_SIZE + u4SetBufferLen),
					     (PUINT_8) prStaRecManualAssoc, NULL, 0);

	cnmMemFree(prAdapter, prStaRecManualAssoc);

	return rWlanStatus;
}

typedef struct _TXBF_CMD_DONE_HANDLER_T {
	UINT_32 u4TxBfCmdId;
	void (*pFunc)(P_ADAPTER_T, P_CMD_INFO_T, PUINT_8);
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
					     TRUE,
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
					     TRUE,
					     pFunc,
					     nicOidCmdTimeoutCommon,
					     sizeof(CMD_MUMIMO_ACTION_T),
					     (PUINT_8) &rCmdMuMimoActionInfo, pvSetBuffer, u4SetBufferLen);

	return rWlanStatus;
}
#endif /* CFG_SUPPORT_MU_MIMO */
#endif /* CFG_SUPPORT_TX_BF */
#endif /* CFG_SUPPORT_QA_TOOL */
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
#if defined(MT6797)
		UINT32 val = 0x77777777;
#endif
		/* fill command */
		rCmdAccessReg.u4Address = prMcrRdInfo->u4McrOffset;
		rCmdAccessReg.u4Data = 0;
#if defined(MT6797)
		if ((prMcrRdInfo->u4McrOffset & 0xFFFF0000) == 0x180f0000) {

			val = readl((volatile UINT_32 *)((*g_pHifRegBaseAddr) + (prMcrRdInfo->u4McrOffset & 0xffff)));

			DBGLOG(INIT, TRACE, "sarah MCR Read: Offset = %#08lx, Data = %#08lx\n",
						   prMcrRdInfo->u4McrOffset, val);


		} else
#endif
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
		DBGLOG(OID, TRACE, "[Stress Test] Rate is Changed to index %d...\n", prAdapter->rWifiVar.eRateSetting);
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
#if defined(MT6797)
		if ((prMcrWrInfo->u4McrOffset & 0xFFFF0000) == 0x180f0000) {

			writel(prMcrWrInfo->u4McrData, (volatile UINT_32 *)((*g_pHifRegBaseAddr) +
				(prMcrWrInfo->u4McrOffset & 0xffff)));

			DBGLOG(INIT, TRACE, "sarah MCR write: Offset = %#08lx, wData = %#08lx\n",
						   prMcrWrInfo->u4McrOffset, prMcrWrInfo->u4McrData);

			return WLAN_STATUS_SUCCESS;
		}
#endif
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
		swCrReadWriteCmd(prAdapter, SWCR_READ,/* Read */
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
			qmDumpQueueStatus(prAdapter);
			cnmDumpMemoryStatus(prAdapter);
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
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;
	UINT_16 u2Id, u2SubId;
	UINT_32 u4Data;

	DEBUGFUNC("wlanoidSetSwCtrlWrite");
	DBGLOG(OID, LOUD, "\n");

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

				nicPowerSaveInfoMap(prAdapter,
					prAdapter->prAisBssInfo->ucBssIndex,
					ePowerMode, PS_CALLER_SW_WRITE);

				rWlanStatus = nicConfigPowerSaveProfile(prAdapter,
								prAdapter->prAisBssInfo->ucBssIndex,
								ePowerMode, TRUE);
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
#if CFG_RX_BA_REORDERING_ENHANCEMENT
		else if (u2SubId == 0x6)
			prAdapter->rWifiVar.fgEnableReportIndependentPkt = (BOOLEAN) u4Data;
#endif
		else if (u2SubId == 0x0100) {
			if (u4Data == 2)
				prAdapter->rWifiVar.ucRxGf = FEATURE_DISABLED;
			else
				prAdapter->rWifiVar.ucRxGf = FEATURE_ENABLED;
		} else if (u2SubId == 0x0101)
			prAdapter->rWifiVar.ucRxShortGI = (UINT_8) u4Data;
		else if (u2SubId == 0x0103) { /* AP Mode WMMPS */
			PARAM_CUSTOM_UAPSD_PARAM_STRUCT_T rUapsdParams;

			DBGLOG(OID, INFO, "ApUapsd 0x10010103 cmd received: %d\n", u4Data);
			if ((BOOLEAN) u4Data) {
				prAdapter->rWifiVar.ucApUapsd = TRUE;
				rUapsdParams.fgEnAPSD = 1;
				rUapsdParams.fgEnAPSD_AcBe = 1;
				rUapsdParams.fgEnAPSD_AcBk = 1;
				rUapsdParams.fgEnAPSD_AcVi = 1;
				rUapsdParams.fgEnAPSD_AcVo = 1;
				rUapsdParams.ucMaxSpLen = 0; /* default: 0, do not limit delivery pkt number */
			} else {
				prAdapter->rWifiVar.ucApUapsd = FALSE;
				rUapsdParams.fgEnAPSD = 0;
				rUapsdParams.fgEnAPSD_AcBe = 0;
				rUapsdParams.fgEnAPSD_AcBk = 0;
				rUapsdParams.fgEnAPSD_AcVi = 0;
				rUapsdParams.fgEnAPSD_AcVo = 0;
				rUapsdParams.ucMaxSpLen = 0; /* default: 0, do not limit delivery pkt number */
			}
			nicSetUapsdParam(prAdapter, &rUapsdParams, NETWORK_TYPE_P2P);
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
	case 0x1003: /* for debug switches */
		switch (u2SubId) {
		case 1:
			DBGLOG(OID, INFO, "Enable VoE 5.7 Packet Jitter test\n");
			prAdapter->rDebugInfo.fgVoE5_7Test = !!u4Data;
			break;
		case 0x0002:
		{
			CMD_TX_AMPDU_T rTxAmpdu;
			WLAN_STATUS rStatus;

			rTxAmpdu.fgEnable = !!u4Data;

			rStatus = wlanSendSetQueryCmd(prAdapter,
				      CMD_ID_TX_AMPDU,
				      TRUE, FALSE, FALSE, NULL, NULL,
				      sizeof(CMD_TX_AMPDU_T), (PUINT_8)&rTxAmpdu, NULL, 0);
			DBGLOG(OID, INFO, "disable tx ampdu status %u\n", rStatus);
			break;
		}
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
			P_BSS_INFO_T prBssInfo = prAdapter->prAisBssInfo;

			authSendDeauthFrame(prAdapter, prBssInfo, prBssInfo->prStaRecOfAP, NULL, 7, NULL);
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
				else if ((prAdapter->rWifiVar.eRateSetting >=
					  FIXED_RATE_MCS0_20M_400NS
					  && prAdapter->rWifiVar.eRateSetting <= FIXED_RATE_MCS7_20M_400NS)
					 || (prAdapter->rWifiVar.eRateSetting >=
					     FIXED_RATE_MCS0_40M_400NS
					     && prAdapter->rWifiVar.eRateSetting <= FIXED_RATE_MCS32_400NS))
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
			} else if (u2SubId == 0x1260) {
				/* Disable On-Line Scan */
				rWlanStatus = nicEnterCtiaModeOfScan(prAdapter, TRUE, TRUE);
			} else if (u2SubId == 0x1261) {
				/* Enable On-Line Scan */
				rWlanStatus = nicEnterCtiaModeOfScan(prAdapter, FALSE, TRUE);
			} else if (u2SubId == 0x1262) {
				/* Disable Roaming */
				rWlanStatus = nicEnterCtiaModeOfRoaming(prAdapter, TRUE, TRUE);
			} else if (u2SubId == 0x1263) {
				/* Enable Roaming */
				rWlanStatus = nicEnterCtiaModeOfRoaming(prAdapter, FALSE, TRUE);
			} else if (u2SubId == 0x1264) {
				/* Keep at CAM mode */
				rWlanStatus = nicEnterCtiaModeOfCAM(prAdapter, TRUE, TRUE);
			} else if (u2SubId == 0x1265) {
				/* Keep at Fast PS */
				rWlanStatus = nicEnterCtiaModeOfCAM(prAdapter, FALSE, TRUE);
			} else if (u2SubId == 0x1266) {
				/* Disable Beacon Timeout Detection */
				rWlanStatus = nicEnterCtiaModeOfBCNTimeout(prAdapter, TRUE, TRUE);
			} else if (u2SubId == 0x1267) {
				/* Enable Beacon Timeout Detection */
				rWlanStatus = nicEnterCtiaModeOfBCNTimeout(prAdapter, FALSE, TRUE);
			} else if (u2SubId == 0x1268) {
				/* Disalbe auto tx power */
				rWlanStatus = nicEnterCtiaModeOfAutoTxPower(prAdapter, TRUE, TRUE);
			} else if (u2SubId == 0x1269) {
				/* Enable auto tx power */
				rWlanStatus = nicEnterCtiaModeOfAutoTxPower(prAdapter, FALSE, TRUE);
			} else if (u2SubId == 0x1270) {
				/* Disalbe FIFO FULL no ack  */
				rWlanStatus = nicEnterCtiaModeOfFIFOFullNoAck(prAdapter, TRUE, TRUE);
			} else if (u2SubId == 0x1271) {
				/* Enable FIFO FULL no ack */
				rWlanStatus = nicEnterCtiaModeOfFIFOFullNoAck(prAdapter, FALSE, TRUE);
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
							  TRUE,
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
	DBGLOG(OID, LOUD, "\n");

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
		DBGLOG(OID, INFO, "Chip config Msg Size %u is not valid (query)\n", rCmdChipConfig.u2MsgSize);
		rCmdChipConfig.u2MsgSize = CHIP_CONFIG_RESP_SIZE;
	}
	kalMemCopy(rCmdChipConfig.aucCmd, prChipConfigInfo->aucCmd, rCmdChipConfig.u2MsgSize);

	rWlanStatus = wlanSendSetQueryCmd(prAdapter, CMD_ID_CHIP_CONFIG, FALSE, TRUE, TRUE,
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
		DBGLOG(OID, INFO, "Chip config Msg Size %u is not valid (set)\n", rCmdChipConfig.u2MsgSize);
		rCmdChipConfig.u2MsgSize = CHIP_CONFIG_RESP_SIZE;
	}
	kalMemCopy(rCmdChipConfig.aucCmd, prChipConfigInfo->aucCmd, rCmdChipConfig.u2MsgSize);

	rWlanStatus = wlanSendSetQueryCmd(prAdapter,
					  CMD_ID_CHIP_CONFIG,
					  TRUE,
					  FALSE,
					  TRUE,
					  nicCmdEventSetCommon,
					  nicOidCmdTimeoutCommon,
					  sizeof(CMD_CHIP_CONFIG_T),
					  (PUINT_8) &rCmdChipConfig, pvSetBuffer, u4SetBufferLen);

	return rWlanStatus;
}				/* wlanoidSetChipConfig */

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

	wlanCfgSet(prAdapter, prKeyCfgInfo->aucKey, prKeyCfgInfo->aucValue, 0);

	wlanInitFeatureOption(prAdapter);

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
		DBGLOG(OID, WARN,
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

	} else
#endif
	{
		return wlanSendSetQueryCmd(prAdapter,
					   CMD_ID_GET_STATISTICS,
					   FALSE,
					   TRUE,
					   TRUE,
					   nicCmdEventQueryXmitOk,
					   nicOidCmdTimeoutCommon, 0, NULL, pvQueryBuffer, u4QueryBufferLen);
	}

	return WLAN_STATUS_SUCCESS;
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
		DBGLOG(OID, WARN,
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

	} else
#endif
	{
		return wlanSendSetQueryCmd(prAdapter,
					   CMD_ID_GET_STATISTICS,
					   FALSE,
					   TRUE,
					   TRUE,
					   nicCmdEventQueryRecvOk,
					   nicOidCmdTimeoutCommon, 0, NULL, pvQueryBuffer, u4QueryBufferLen);
	}

	return WLAN_STATUS_SUCCESS;
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
		DBGLOG(OID, WARN,
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
	} else
#endif
	{
		return wlanSendSetQueryCmd(prAdapter,
					   CMD_ID_GET_STATISTICS,
					   FALSE,
					   TRUE,
					   TRUE,
					   nicCmdEventQueryXmitError,
					   nicOidCmdTimeoutCommon, 0, NULL, pvQueryBuffer, u4QueryBufferLen);
	}

	return WLAN_STATUS_SUCCESS;
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
		DBGLOG(OID, WARN,
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
	} else
#endif
	{
		return wlanSendSetQueryCmd(prAdapter,
					   CMD_ID_GET_STATISTICS,
					   FALSE,
					   TRUE,
					   TRUE,
					   nicCmdEventQueryXmitOneCollision,
					   nicOidCmdTimeoutCommon, 0, NULL, pvQueryBuffer, u4QueryBufferLen);
	}

	return WLAN_STATUS_SUCCESS;
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
		DBGLOG(OID, WARN,
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
	} else
#endif
	{
		return wlanSendSetQueryCmd(prAdapter,
					   CMD_ID_GET_STATISTICS,
					   FALSE,
					   TRUE,
					   TRUE,
					   nicCmdEventQueryXmitMoreCollisions,
					   nicOidCmdTimeoutCommon, 0, NULL, pvQueryBuffer, u4QueryBufferLen);
	}

	return WLAN_STATUS_SUCCESS;
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
		DBGLOG(OID, WARN,
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
	} else
#endif
	{
		return wlanSendSetQueryCmd(prAdapter,
					   CMD_ID_GET_STATISTICS,
					   FALSE,
					   TRUE,
					   TRUE,
					   nicCmdEventQueryXmitMaxCollisions,
					   nicOidCmdTimeoutCommon, 0, NULL, pvQueryBuffer, u4QueryBufferLen);
	}

	return WLAN_STATUS_SUCCESS;
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

	DBGLOG(OID, WARN, "Custom OID interface version: %#08x\n", *(PUINT_32) pvQueryBuffer);

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
		DBGLOG(OID, WARN,
		       "Fail in set multicast list! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	kalMemZero(&rCmdMacMcastAddr, sizeof(rCmdMacMcastAddr));
	rCmdMacMcastAddr.u4NumOfGroupAddr = u4SetBufferLen / MAC_ADDR_LEN;
	rCmdMacMcastAddr.ucBssIndex = prAdapter->prAisBssInfo->ucBssIndex;
	kalMemCopy(rCmdMacMcastAddr.arAddress, pvSetBuffer, u4SetBufferLen);
	DBGLOG(OID, TRACE,
		"MCAST white list: total=%d MAC0="MACSTR" MAC1="MACSTR" MAC2="MACSTR" MAC3="MACSTR" MAC4="MACSTR"\n",
		rCmdMacMcastAddr.u4NumOfGroupAddr,
		MAC2STR(rCmdMacMcastAddr.arAddress[0]), MAC2STR(rCmdMacMcastAddr.arAddress[1]),
		MAC2STR(rCmdMacMcastAddr.arAddress[2]), MAC2STR(rCmdMacMcastAddr.arAddress[3]),
		MAC2STR(rCmdMacMcastAddr.arAddress[4]));

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
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidSetCurrentPacketFilter");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	if (u4SetBufferLen < sizeof(UINT_32)) {
		*pu4SetInfoLen = sizeof(UINT_32);
		return WLAN_STATUS_INVALID_LENGTH;
	}
	ASSERT(pvSetBuffer);

	/* Set the new packet filter. */
	u4NewPacketFilter = *(PUINT_32) pvSetBuffer;

	DBGLOG(OID, TRACE, "New packet filter: %#08x\n", u4NewPacketFilter);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(OID, WARN,
		       "Fail in set current packet filter! (Adapter not ready). ACPI=D%d, Radio=%d\n",
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

		prAdapter->fgAllMulicastFilter = FALSE;
		if (u4NewPacketFilter & PARAM_PACKET_FILTER_ALL_MULTICAST)
			prAdapter->fgAllMulicastFilter = TRUE;
	} while (FALSE);

	if (rStatus == WLAN_STATUS_SUCCESS) {
		/* Store the packet filter */
		prAdapter->u4OsPacketFilter &= PARAM_PACKET_FILTER_P2P_MASK;
		prAdapter->u4OsPacketFilter |= u4NewPacketFilter;
		rStatus = wlanoidSetPacketFilter(prAdapter, prAdapter->u4OsPacketFilter,
					TRUE, pvSetBuffer, u4SetBufferLen);
	}
	DBGLOG(OID, TRACE, "[MC debug] u4OsPacketFilter=0x%x\n", prAdapter->u4OsPacketFilter);
	return rStatus;
}				/* wlanoidSetCurrentPacketFilter */

WLAN_STATUS wlanoidSetPacketFilter(P_ADAPTER_T prAdapter, UINT_32 u4PacketFilter,
				BOOLEAN fgIsOid, PVOID pvSetBuffer, UINT_32 u4SetBufferLen)
{
#if CFG_SUPPORT_DROP_ALL_MC_PACKET
	if (prAdapter->prGlueInfo->fgIsInSuspendMode)
		u4PacketFilter &= ~(PARAM_PACKET_FILTER_MULTICAST | PARAM_PACKET_FILTER_ALL_MULTICAST);
#else
	if (prAdapter->prGlueInfo->fgIsInSuspendMode) {
		u4PacketFilter &= ~(PARAM_PACKET_FILTER_ALL_MULTICAST);
		u4PacketFilter |= (PARAM_PACKET_FILTER_MULTICAST);
	}
#endif
	DBGLOG(OID, TRACE, "[MC debug] u4PacketFilter=0x%x, IsSuspend=%d\n", u4PacketFilter,
				prAdapter->prGlueInfo->fgIsInSuspendMode);
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
		DBGLOG(OID, WARN,
		       "Fail in set disassociate! (Adapter not ready). ACPI=D%d, Radio=%d\n",
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
		    (PARAM_POWER_MODE) (prAdapter->rWlanInfo.
					arPowerSaveMode[prAdapter->prAisBssInfo->ucBssIndex].ucPsProfile);
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
		DBGLOG(OID, WARN, "Set power mode error: Invalid length %u\n", u4SetBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	} else if (prPowerMode->ePowerMode >= Param_PowerModeMax) {
		DBGLOG(OID, WARN, "Set power mode error: Invalid power mode(%u)\n", prPowerMode->ePowerMode);
		return WLAN_STATUS_INVALID_DATA;
	} else if (prPowerMode->ucBssIdx >= BSS_INFO_NUM) {
		DBGLOG(OID, WARN, "Set power mode error: Invalid BSS index(%d)\n", prPowerMode->ucBssIdx);
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
	} else if (prAdapter->rWifiVar.ePowerMode != Param_PowerModeMax)
		prPowerMode->ePowerMode = prAdapter->rWifiVar.ePowerMode;

	nicPowerSaveInfoMap(prAdapter, prPowerMode->ucBssIdx,
		prPowerMode->ePowerMode, PS_CALLER_COMMON);

	status = nicConfigPowerSaveProfile(prAdapter, prPowerMode->ucBssIdx, prPowerMode->ePowerMode, TRUE);

	if (prPowerMode->ePowerMode < Param_PowerModeMax) {
		if (prBssInfo->eNetworkType >= 0 && prPowerMode->ePowerMode >= 0)
			DBGLOG(OID, INFO, "Set %s Network BSS(%u) PS mode to %s (%d)\n",
		       apucNetworkType[prBssInfo->eNetworkType],
		       prPowerMode->ucBssIdx, apucPsMode[prPowerMode->ePowerMode], prPowerMode->ePowerMode);
	} else {
		if (prBssInfo->eNetworkType >= 0)
			DBGLOG(OID, INFO, "Invalid PS mode setting (%d) for %s Network BSS(%u)\n",
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

	if ((*pu4BeaconInterval < DOT11_BEACON_PERIOD_MIN)
	    || (*pu4BeaconInterval > DOT11_BEACON_PERIOD_MAX)) {
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
	UINT_32 u4CSUMFlags;
	CMD_BASIC_CONFIG_T rCmdBasicConfig;

	DEBUGFUNC("wlanoidSetCSUMOffload");
	DBGLOG(OID, LOUD, "\n");

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

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_BASIC_CONFIG,
				   TRUE,
				   FALSE,
				   TRUE,
				   nicCmdEventSetCommon,
				   nicOidCmdTimeoutCommon,
				   sizeof(CMD_BASIC_CONFIG_T),
				   (PUINT_8) &rCmdBasicConfig, pvSetBuffer, u4SetBufferLen);
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
	DBGLOG(OID, LOUD, "\n");

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

		prNetworkAddress = (P_PARAM_NETWORK_ADDRESS) ((ULONG) prNetworkAddress + (ULONG)
							      (prNetworkAddress->u2AddressLength +
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
	if (prBssInfo->prIpV4NetAddrList == NULL) {
		kalMemFree(prCmdNetworkAddressList, VIR_MEM_TYPE, u4CmdSize);
		return WLAN_STATUS_FAILURE;
	}
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

				DBGLOG(OID, INFO,
				       "%s: IPv4 Addr [%u][" IPV4STR "]\n", __func__,
				       u4IPv4AddrIdx, IPV4TOSTR(prNetworkAddress->aucAddress));

				u4IPv4AddrIdx++;
			}

			prNetworkAddress = (P_PARAM_NETWORK_ADDRESS) ((ULONG) prNetworkAddress + (ULONG)
								      (prNetworkAddress->u2AddressLength +
								       OFFSET_OF(PARAM_NETWORK_ADDRESS, aucAddress)));
		}

	} else {
		prCmdNetworkAddressList->ucAddressCount = 0;
	}

	DBGLOG(OID, INFO,
	       "%s: Set %u IPv4 address for BSS[%d]\n", __func__, u4IPv4AddrCount,
	       prCmdNetworkAddressList->ucBssIndex);

	/* 4 <5> Send command */
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

	DEBUGFUNC("wlanoidRftestSetTestMode");

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
						      TRUE,
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

	DEBUGFUNC("wlanoidRftestSetTestMode");

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
		DBGLOG(OID, ERROR, "Invalid data. QueryBufferLen: %u.\n", u4QueryBufferLen);
		/* return WLAN_STATUS_INVALID_LENGTH; */
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
		DBGLOG(OID, ERROR, "Invalid data. SetBufferLen: %u.\n", u4SetBufferLen);
		/* return WLAN_STATUS_INVALID_LENGTH; */
	}

	prRfATInfo = (P_PARAM_MTK_WIFI_TEST_STRUCT_T) pvSetBuffer;

	DBGLOG(OID, TRACE, "u4FuncIndex(%u)-u4FuncData(%u)\n",
		prRfATInfo->u4FuncIndex, prRfATInfo->u4FuncData);

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
	prCmdInfo->ucCID = CMD_ID_TEST_CTRL;
	prCmdInfo->fgSetQuery = TRUE;
	prCmdInfo->fgNeedResp = FALSE;
	prCmdInfo->fgDriverDomainMCR = FALSE;
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
		prTestStatus->rATInfo.u4FuncData2 = prAdapter->rVerInfo.u4FwOwnVersionExtend;
		u4QueryBufferLen = sizeof(EVENT_TEST_STATUS);

	} else if (u4FuncIndex == RF_AT_FUNCID_DRV_INFO) {
		/* driver implementation */
		prTestStatus = (P_EVENT_TEST_STATUS) pvQueryBuffer;

		prTestStatus->rATInfo.u4FuncData = CFG_DRV_OWN_VERSION;
		u4QueryBufferLen = sizeof(EVENT_TEST_STATUS);

	} else if (u4FuncIndex == RF_AT_FUNCID_QUERY_ICAP_DUMP_FILE) {
		/* driver implementation */
		prTestStatus = (P_EVENT_TEST_STATUS) pvQueryBuffer;

		prTestStatus->rATInfo.u4FuncData = g_u2DumpIndex;
		u4QueryBufferLen = sizeof(EVENT_TEST_STATUS);

	} else {
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
		prCmdInfo->fgIsOid = TRUE;
		prCmdInfo->ucCID = CMD_ID_TEST_CTRL;
		prCmdInfo->fgSetQuery = FALSE;
		prCmdInfo->fgNeedResp = TRUE;
		prCmdInfo->fgDriverDomainMCR = FALSE;
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

	return WLAN_STATUS_SUCCESS;
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

	if (!prAdapter || !prAdapter->prAisBssInfo) {
		DBGLOG(OID, ERROR, "prAdapter or prAisBssInfo is not allocated.\n");
		return WLAN_STATUS_FAILURE;
	}

	prGlueInfo = prAdapter->prGlueInfo;
	if (!prGlueInfo) {
		/* When wlanRemove is called, can't do wlanTriggerStatsLog in auth/assoc tx done */
		return WLAN_STATUS_FAILURE;
	}
	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, (CMD_HDR_SIZE + u4SetQueryInfoLen));

	DEBUGFUNC("wlanSendSetQueryCmd");

	if (!prCmdInfo) {
		DBGLOG(OID, ERROR, "prCmdInfo is not allocated.\n");
		return WLAN_STATUS_FAILURE;
	}

	/* increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);
	DBGLOG(OID, TRACE, "ucCmdSeqNum =%d\n", ucCmdSeqNum);

	/* Setup common CMD Info Packet */
	prCmdInfo->eCmdType = COMMAND_TYPE_NETWORK_IOCTL;
	prCmdInfo->ucBssIndex = prAdapter->prAisBssInfo->ucBssIndex;
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
	prWifiCmd->u2TxByteCount = prCmdInfo->u2InfoBufLen;
	prWifiCmd->u2PQ_ID = CMD_PQ_ID;
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
	P_CONNECTION_SETTINGS_T prConnSettings;

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	DEBUGFUNC("wlanoidSetWapiAssocInfo");
	DBGLOG(OID, LOUD, "\r\n");

	prConnSettings = &prAdapter->rWifiVar.rConnSettings;
	prConnSettings->fgWapiMode = FALSE;

	if (u4SetBufferLen < 20 /* From EID to Group cipher */)
		return WLAN_STATUS_INVALID_LENGTH;

	if (!wextSrchDesiredWAPIIE((PUINT_8) pvSetBuffer, u4SetBufferLen, (PUINT_8 *) &prWapiInfo))
		return WLAN_STATUS_INVALID_LENGTH;

	if (!prWapiInfo || prWapiInfo->ucLength < 18)
		return WLAN_STATUS_INVALID_LENGTH;

	/* Skip Version check */

	/*Cipher suite count check, only one of each for now*/
	if (prWapiInfo->u2AKMSuiteCount > 1 ||
	    prWapiInfo->u2PairSuiteCount > 1)
		return WLAN_STATUS_INVALID_LENGTH;

	DBGLOG(SEC, TRACE, "WAPI: Assoc Info auth mgt suite [%d]: %02x-%02x-%02x-%02x\n",
	       prWapiInfo->u2AKMSuiteCount,
	       (UCHAR) (prWapiInfo->u4AKMSuite & 0x000000FF),
	       (UCHAR) ((prWapiInfo->u4AKMSuite >> 8) & 0x000000FF),
	       (UCHAR) ((prWapiInfo->u4AKMSuite >> 16) & 0x000000FF),
	       (UCHAR) ((prWapiInfo->u4AKMSuite >> 24) & 0x000000FF));

	if (prWapiInfo->u4AKMSuite != WAPI_AKM_SUITE_802_1X &&
	    prWapiInfo->u4AKMSuite != WAPI_AKM_SUITE_PSK)
		return WLAN_STATUS_NOT_SUPPORTED;

	DBGLOG(SEC, TRACE, "WAPI: Assoc Info pairwise cipher suite [%d]: %02x-%02x-%02x-%02x\n",
	       prWapiInfo->u2PairSuiteCount,
	       (UCHAR) (prWapiInfo->u4PairSuite & 0x000000FF),
	       (UCHAR) ((prWapiInfo->u4PairSuite >> 8) & 0x000000FF),
	       (UCHAR) ((prWapiInfo->u4PairSuite >> 16) & 0x000000FF),
	       (UCHAR) ((prWapiInfo->u4PairSuite >> 24) & 0x000000FF));

	if (prWapiInfo->u4PairSuite != WAPI_CIPHER_SUITE_WPI)
		return WLAN_STATUS_NOT_SUPPORTED;

	DBGLOG(SEC, TRACE, "WAPI: Assoc Info group cipher suite : %02x-%02x-%02x-%02x\n",
	       (UCHAR) (prWapiInfo->u4GroupSuite & 0x000000FF),
	       (UCHAR) ((prWapiInfo->u4GroupSuite >> 8) & 0x000000FF),
	       (UCHAR) ((prWapiInfo->u4GroupSuite >> 16) & 0x000000FF),
	       (UCHAR) ((prWapiInfo->u4GroupSuite >> 24) & 0x000000FF));

	if (prWapiInfo->u4GroupSuite != WAPI_CIPHER_SUITE_WPI)
		return WLAN_STATUS_NOT_SUPPORTED;

	prConnSettings->u4WapiSelectedAKMSuite = prWapiInfo->u4AKMSuite;
	prConnSettings->u4WapiSelectedPairwiseCipher = prWapiInfo->u4PairSuite;
	prConnSettings->u4WapiSelectedGroupCipher = prWapiInfo->u4GroupSuite;

	prConnSettings->fgWapiMode = TRUE;

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
	DBGLOG(OID, LOUD, "\r\n");

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(OID, WARN,
		       "Fail in set add key! (Adapter not ready). ACPI=D%d, Radio=%d\r\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	prNewKey = (P_PARAM_WPI_KEY_T) pvSetBuffer;

	DBGLOG_MEM8(OID, TRACE, (PUINT_8) pvSetBuffer, 560);
	pc = (PUINT_8) pvSetBuffer;

	*pu4SetInfoLen = u4SetBufferLen;

	/* Todo:: WAPI AP mode !!!!! */
	prBssInfo = prAdapter->prAisBssInfo;

	prNewKey->ucKeyID = prNewKey->ucKeyID & BIT(0);

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
	prCmdInfo->ucBssIndex = prAdapter->prAisBssInfo->ucBssIndex;
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

			if ((prCmdKey->aucPeerAddr[0] & prCmdKey->aucPeerAddr[1] & prCmdKey->aucPeerAddr[2] & prCmdKey->
			     aucPeerAddr[3] & prCmdKey->aucPeerAddr[4] & prCmdKey->aucPeerAddr[5]) == 0xFF) {
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
				/* prBssInfo->ucCurrentGtkId, */
				/* BIT(1)); */
			}

			prBssInfo->fgTxBcKeyExist = TRUE;
			prBssInfo->ucBMCWlanIndex = prCmdKey->ucWlanIndex;	/* Saved for AIS WEP */
			prBssInfo->ucTxDefaultKeyID = prCmdKey->ucKeyId;
		}
#endif
	} else {
		/* Including IBSS RSN Rx BC key ? */
		if ((prCmdKey->aucPeerAddr[0] & prCmdKey->aucPeerAddr[1] & prCmdKey->aucPeerAddr[2] & prCmdKey->
		     aucPeerAddr[3] & prCmdKey->aucPeerAddr[4] & prCmdKey->aucPeerAddr[5]) == 0xFF) {
			prCmdKey->ucWlanIndex = 255;	/* AIS WEP, should not have this case!! */
		} else {
			if (prStaRec) {	/* AIS RSN Group key but addr is BSSID */
				/* ASSERT(prStaRec->ucBMCWlanIndex < WTBL_SIZE) */
				prCmdKey->ucWlanIndex =
				    secPrivacySeekForBcEntry(prAdapter, prStaRec->ucBssIndex,
							     prStaRec->aucMacAddr,
							     prStaRec->ucIndex,
							     prCmdKey->ucAlgorithmId,
							     prCmdKey->ucKeyId, prStaRec->ucCurrentGtkId, BIT(0));
				prStaRec->ucBMCWlanIndex = prCmdKey->ucWlanIndex;
			} else {	/* Exist this case ? */
				ASSERT(FALSE);
				/* prCmdKey->ucWlanIndex = */
				/* secPrivacySeekForBcEntry(prAdapter, */
				/* prBssInfo->ucBssIndex, */
				/* NETWORK_TYPE_AIS, */
				/* prCmdKey->aucPeerAddr, */
				/* prCmdKey->ucAlgorithmId, */
				/* prCmdKey->ucKeyId, */
				/* prBssInfo->ucCurrentGtkId, */
				/* BIT(0)); */
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
		;		/* NIC_PM_WMM_PS_DISABLE_UC_TRIG(prAdapter, TRUE); */
	else
		;		/* NIC_PM_WMM_PS_DISABLE_UC_TRIG(prAdapter, FALSE); */
#endif

	rStatus = wlanSendSetQueryCmd(prAdapter,
				      CMD_ID_SET_WMM_PS_TEST_PARMS,
				      TRUE,
				      FALSE,
				      TRUE,
				      nicCmdEventSetCommon,
				      nicOidCmdTimeoutCommon,
				      u2CmdBufLen,
				      (PUINT_8) (&rSetWmmPsTestParam),
				      NULL,
				      0);
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

					if (prSltInfo->prPseudoBssDesc == NULL)
						rWlanStatus = WLAN_STATUS_FAILURE;
					else
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

				/* Please initial SLT Mode first. */
			if (prSltInfo->prPseudoBssDesc == NULL)
				break;

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
		fgStatus = kalCfgDataRead16(prAdapter->prGlueInfo, prNvramRwInfo->ucEepromIndex << 1,
					    &u2Data);	/* change to byte offset */

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

	fgStatus = kalCfgDataWrite16(prAdapter->prGlueInfo, prNvramRwInfo->ucEepromIndex << 1,
				     prNvramRwInfo->u2EepromData);	/* change to byte offset */

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

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(u4SetBufferLen == 2);

	*pu4SetInfoLen = 2;

	pucCountry = pvSetBuffer;

	prAdapter->rWifiVar.rConnSettings.u2CountryCode = (((UINT_16) pucCountry[0]) << 8) | ((UINT_16) pucCountry[1]);

	/* Force to re-search country code in regulatory domains */
	prAdapter->prDomainInfo = NULL;
	rlmDomainSendCmd(prAdapter);

	/* Update supported channel list in channel table based on current country domain */
	wlanUpdateChannelTable(prAdapter->prGlueInfo);

	return WLAN_STATUS_SUCCESS;
}

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
		DBGLOG(OID, WARN, "Fail to set BT profile because of ACPI_D3\n");
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	ASSERT(pvSetBuffer);
	prPtaIpc = (P_PTA_IPC_T) pvSetBuffer;

#if CFG_SUPPORT_BCM && CFG_SUPPORT_BCM_BWCS && CFG_SUPPORT_BCM_BWCS_DEBUG
	DBGLOG(OID, INFO, "BCM BWCS CMD: BWCS CMD = %02x%02x%02x%02x\n",
	       prPtaIpc->u.aucBTPParams[0], prPtaIpc->u.aucBTPParams[1],
	       prPtaIpc->u.aucBTPParams[2], prPtaIpc->u.aucBTPParams[3]);

	DBGLOG(OID, INFO,
	       "BCM BWCS CMD: BTPParams[0]=%02x, BTPParams[1]=%02x, BTPParams[2]=%02x, BTPParams[3]=%02x.\n",
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
	DBGLOG(OID, LOUD, "\r\n");

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
				      nicCmdEventSetCommon, nicOidCmdTimeoutCommon, sizeof(SET_TXPWR_CTRL_T),
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

		DBGLOG(RFTEST, INFO, "[ucFragNum = %d] u4Address = 0x%x, len %u, remain len %u\n",
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

	DBGLOG(RFTEST, INFO, "wlanoidQueryMemDump----->\n");

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

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(PARAM_CUSTOM_P2P_SET_STRUCT_T);
	if (u4SetBufferLen < sizeof(PARAM_CUSTOM_P2P_SET_STRUCT_T)) {
		DBGLOG(OID, WARN, "Invalid length %u\n", u4SetBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	prSetP2P = (P_PARAM_CUSTOM_P2P_SET_STRUCT_T) pvSetBuffer;

	DBGLOG(P2P, INFO, "Set P2P enable[%u] mode[%u]\n", prSetP2P->u4Enable, prSetP2P->u4Mode);

	/*
	 *    enable = 1, mode = 0  => init P2P network
	 *    enable = 1, mode = 1  => init Soft AP network
	 *    enable = 0  => uninit P2P/AP network
	 */

	if (prSetP2P->u4Enable) {
		p2pSetMode((prSetP2P->u4Mode == 1) ? TRUE : FALSE);
		if (p2pLaunch(prAdapter->prGlueInfo))
			ASSERT(prAdapter->fgIsP2PRegistered);
		if (prAdapter->rWifiVar.ucApUapsd && prSetP2P->u4Mode == 1) {
			PARAM_CUSTOM_UAPSD_PARAM_STRUCT_T rUapsdParams;

			DBGLOG(OID, INFO, "wlanoidSetP2pMode Default enable ApUapsd\n");
			rUapsdParams.fgEnAPSD = 1;
			rUapsdParams.fgEnAPSD_AcBe = 1;
			rUapsdParams.fgEnAPSD_AcBk = 1;
			rUapsdParams.fgEnAPSD_AcVi = 1;
			rUapsdParams.fgEnAPSD_AcVo = 1;
			rUapsdParams.ucMaxSpLen = 0; /* default:0, Do not limit delivery pkt num */
			nicSetUapsdParam(prAdapter, &rUapsdParams, NETWORK_TYPE_P2P);
		}
	} else {
		if (prAdapter->fgIsP2PRegistered)
			p2pRemove(prAdapter->prGlueInfo);
	}

	return status;

}
#endif

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

	DEBUGFUNC("wlanoidSetDefaultKey");

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(OID, WARN, "Fail in set add key! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	prDefaultKey = (P_PARAM_DEFAULT_KEY_T) pvSetBuffer;

	*pu4SetInfoLen = u4SetBufferLen;

	/* Dump PARAM_DEFAULT_KEY_T content. */
	DBGLOG(OID, INFO, "Key Index : %d, Unicast Key : %d, Multicast Key : %d\n",
	       prDefaultKey->ucKeyID, prDefaultKey->ucUnicast, prDefaultKey->ucMulticast);

	/* prWlanTable = prAdapter->rWifiVar.arWtbl; */
	prGlueInfo = prAdapter->prGlueInfo;
	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, (CMD_HDR_SIZE + sizeof(CMD_DEFAULT_KEY)));

	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}
	/* increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);
	DBGLOG(OID, TRACE, "ucCmdSeqNum = %d\n", ucCmdSeqNum);

	/* compose CMD_802_11_KEY cmd pkt */
	prCmdInfo->eCmdType = COMMAND_TYPE_NETWORK_IOCTL;
	prCmdInfo->ucBssIndex = prAdapter->prAisBssInfo->ucBssIndex;
	prCmdInfo->u2InfoBufLen = CMD_HDR_SIZE + sizeof(CMD_DEFAULT_KEY);
	prCmdInfo->pfCmdDoneHandler = nicCmdEventSetCommon;
	prCmdInfo->pfCmdTimeoutHandler = nicOidCmdTimeoutCommon;
	prCmdInfo->fgIsOid = TRUE;
	prCmdInfo->ucCID = CMD_ID_DEFAULT_KEY_ID;
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

	prCmdDefaultKey = (P_CMD_DEFAULT_KEY) (prWifiCmd->aucBuffer);

	kalMemZero(prCmdDefaultKey, sizeof(CMD_DEFAULT_KEY));

	prCmdDefaultKey->ucBssIdx = prAdapter->prAisBssInfo->ucBssIndex;
	prCmdDefaultKey->ucKeyId = prDefaultKey->ucKeyID;
	prCmdDefaultKey->ucUnicast = prDefaultKey->ucUnicast;
	prCmdDefaultKey->ucMulticast = prDefaultKey->ucMulticast;

	if (prDefaultKey->ucMulticast) {
		prAdapter->prAisBssInfo->fgTxBcKeyExist = TRUE;
		prAdapter->prAisBssInfo->ucTxDefaultKeyID = prDefaultKey->ucKeyID;
		/* prBssInfo->ucBMCWlanIndex = secPrivacySeekForBcEntry(prAdapter, prBssInfo->ucBssIndex, */
		/* NETWORK_TYPE_AIS, prCmdKey->aucPeerAddr, prCmdKey->ucAlgorithmId, prCmdKey->ucKeyId, */
		/* prBssInfo->ucCurrentGtkId, BIT(1)); */
		/* prCmdDefaultKey->ucBMCWlanIndex = prBssInfo->ucBMCWlanIndex; */
	} else {
		ASSERT(FALSE);
	}

	/* insert into prCmdQueue */
	kalEnqueueCommand(prGlueInfo, (P_QUE_ENTRY_T) prCmdInfo);

	/* wakeup txServiceThread later */
	GLUE_SET_EVENT(prGlueInfo);

	return WLAN_STATUS_PENDING;
}				/* wlanoidSetDefaultKey */

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
	CMD_GTK_REKEY_DATA_T rCmdContent;
	WLAN_STATUS rStatus;

	ASSERT(prAdapter);

	kalMemCopy(&rCmdContent, (PUINT_8) pvSetBuffer, u4SetBufferLen);

	rStatus = wlanSendSetQueryCmd(prAdapter,	/* prAdapter */
				      CMD_ID_SET_GTK_REKEY_DATA,	/* ucCID */
				      TRUE,	/* fgSetQuery */
				      FALSE,	/* fgNeedResp */
				      TRUE,	/* fgIsOid */
				      NULL,	/* pfCmdDoneHandler */
				      NULL,	/* pfCmdTimeoutHandler */
				      sizeof(CMD_GTK_REKEY_DATA_T),	/* u4SetQueryInfoLen */
				      (PUINT_8) &rCmdContent,	/* pucInfoBuffer */
				      NULL,	/* pvSetQueryBuffer */
				      0	/* u4SetQueryBufferLen */
	    );

	/* ASSERT(rStatus == WLAN_STATUS_PENDING); */

	return rStatus;
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
		DBGLOG(OID, WARN,
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
		DBGLOG(OID, WARN, "Return from BSSID list scan! (radio off). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_SUCCESS;
	}

	prSchedScanRequest = (P_PARAM_SCHED_SCAN_REQUEST) pvSetBuffer;

	if (scnFsmSchedScanRequest(prAdapter, prSchedScanRequest) == TRUE)
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
	DBGLOG(OID, WARN, "[Puff]wlanoidResetBAScoreboard\n");

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

	DBGLOG(SCN, TRACE, "[BATCH] command=%s, len=%u\n", (PUINT_8)pvSetBuffer, u4SetBufferLen);

	if (!pu4WritenLen)
		return -EINVAL;
	*pu4WritenLen = 0;

	if (u4SetBufferLen < kalStrLen(CMD_WLS_BATCHING)) {
		DBGLOG(SCN, TRACE, "[BATCH] invalid len %u\n", u4SetBufferLen);
		return -EINVAL;
	}

	head = pvSetBuffer + kalStrLen(CMD_WLS_BATCHING) + 1;
	kalMemSet(&rCmdBatchReq, 0, sizeof(CMD_BATCH_REQ_T));

	if (!kalStrnCmp(head, BATCHING_SET, kalStrLen(BATCHING_SET))) {

		DBGLOG(SCN, TRACE, "XXX Start Batch Scan XXX\n");

		head += kalStrLen(BATCHING_SET) + 1;

		/* SCANFREQ, MSCAN, BESTN */
		tokens = sscanf(head, "SCANFREQ=%d MSCAN=%d BESTN=%d", &scanfreq, &mscan, &bestn);
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
		/*
		 * else {
		 * *p = '.'; // remove '>' because sscanf can not parse <%s>
		 * }
		 */
		/*
		 * tokens = sscanf(head, "CHANNEL=<%s", c_channel);
		 * if (tokens != 1) {
		 * DBGLOG(SCN, TRACE, "[BATCH] Parse fail: tokens=%d, CHANNEL=<%s>\n",
		 * tokens, c_channel);
		 * return -EINVAL;
		 * }
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

	rStatus = wlanSendSetQueryCmd(prAdapter,
			    CMD_ID_SET_BATCH_REQ,
			    TRUE, FALSE, TRUE, NULL, NULL, sizeof(CMD_BATCH_REQ_T), (PUINT_8) &rCmdBatchReq, NULL, 0);

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
	}
	if (pvSetBuffer == NULL) {
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

	if (scnSetGSCNParam(prAdapter, prCmdGscnParam))
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
	DBGLOG(OID, LOUD, "\r\n");

	if (u4SetBufferLen == 0)
		return WLAN_STATUS_INVALID_LENGTH;

	*pu4SetInfoLen = u4SetBufferLen;

	prHS20IndicationIe = (P_IE_HS20_INDICATION_T) pvSetBuffer;

	prAdapter->prGlueInfo->ucHotspotConfig = prHS20IndicationIe->ucHotspotConfig;
	prAdapter->prGlueInfo->fgConnectHS20AP = TRUE;

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
	DBGLOG(OID, TRACE, "\r\n");

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
					  TRUE,
					  nicCmdEventSetCommon,
					  nicOidCmdTimeoutCommon,
					  sizeof(CMD_MONITOR_SET_INFO_T),
					  (PUINT_8) &rCmdMonitorSetInfo, pvSetBuffer, u4SetBufferLen);

	return rWlanStatus;
}
#endif

#if CFG_SUPPORT_RSSI_DISCONNECT
/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the latest RSSI value before disconnecting.
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
* \retval WLAN_STATUS_NOT_SUPPORTED
*/
/*----------------------------------------------------------------------------*/

WLAN_STATUS
wlanoidQueryRssiDisconnect(IN P_ADAPTER_T prAdapter,
		      IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	PARAM_RSSI i4cRssi;

	DEBUGFUNC("wlanoidQueryRssiDisconnect");

	if (!prAdapter || !pu4QueryInfoLen)
		return -EPERM;
	if (u4QueryBufferLen && !pvQueryBuffer)
		return -EPERM;

	*pu4QueryInfoLen = sizeof(PARAM_RSSI);

	/* Check for query buffer length */
	if (u4QueryBufferLen < *pu4QueryInfoLen) {
		DBGLOG(OID, WARN, "Too short length %u\n", u4QueryBufferLen);
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}
	DBGLOG(OID, INFO, "fgIsLinkQualityValid = %d, rLinkQualityUpdateTime = %d\n",
		prAdapter->fgIsLinkQualityValid, prAdapter->rLinkQualityUpdateTime);
	if (!prAdapter->rLinkQualityUpdateTime || kalGetMediaStateIndicated(prAdapter->prGlueInfo)
		== PARAM_MEDIA_STATE_CONNECTED)
		return WLAN_STATUS_NOT_SUPPORTED;

	i4cRssi = (PARAM_RSSI) prAdapter->rLinkQuality.cRssi;	/* ranged from (-128 ~ 30) in unit of dBm */

	if (i4cRssi > PARAM_WHQL_RSSI_MAX_DBM)
		i4cRssi = PARAM_WHQL_RSSI_MAX_DBM;
	else if (i4cRssi < PARAM_WHQL_RSSI_MIN_DBM)
		i4cRssi = PARAM_WHQL_RSSI_MIN_DBM;
	DBGLOG(OID, INFO, "i4cRssi = %d\n", i4cRssi);
	kalMemCopy(pvQueryBuffer, &i4cRssi, sizeof(PARAM_RSSI));
	return WLAN_STATUS_SUCCESS;

}				/* end of wlanoidQueryRssiDisconnect() */
#endif

WLAN_STATUS
wlanoidNotifyFwSuspend(IN P_ADAPTER_T prAdapter,
		       IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	CMD_SUSPEND_MODE_SETTING_T rSuspendCmd;

	if (!prAdapter || !pvSetBuffer)
		return WLAN_STATUS_INVALID_DATA;

	rSuspendCmd.fIsEnableSuspendMode = *(PBOOLEAN)pvSetBuffer;
	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_SET_SUSPEND_MODE,
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

WLAN_STATUS
wlanoidPacketKeepAlive(IN P_ADAPTER_T prAdapter,
		       IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	P_PARAM_PACKET_KEEPALIVE_T prPacket;

	DEBUGFUNC("wlanoidPacketKeepAlive");
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

WLAN_STATUS
wlanoidSetRoamingCtrl(
		IN P_ADAPTER_T  prAdapter,
		IN  PVOID    pvSetBuffer,
		IN  UINT_32  u4SetBufferLen,
		OUT PUINT_32 pu4SetInfoLen)
{
	CMD_ROAMING_CTRL_T rRoamingCtrl;
	BOOLEAN fgIsSuspend = *(PUINT_8)pvSetBuffer;

	kalMemZero(&rRoamingCtrl, sizeof(rRoamingCtrl));
	/* fgEnable:  enable roaming detect or not, in suspend, don't trigger roaming */
	rRoamingCtrl.fgEnable = !fgIsSuspend;
	#if 0
	/* u2RcpiLowThr: RCPI threshold to trigger roaming event */
	rRoamingCtrl.u2RcpiLowThr = (fgEnable == TRUE) ? 57:57;
	/* ucRoamingRetryLimit: at most how many times report roaming discovery if roaming failed last time */
	rRoamingCtrl.ucRoamingRetryLimit = 0;
	#endif

	return wlanSendSetQueryCmd(prAdapter,
		CMD_ID_ROAMING_CONTROL,
		TRUE,
		FALSE,
		TRUE,
		nicCmdEventSetCommon,
		nicOidCmdTimeoutCommon,
		sizeof(rRoamingCtrl),
		(PUINT_8)&rRoamingCtrl,
		pvSetBuffer,
		u4SetBufferLen
		);
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
			nicOidCmdTimeoutCommon,
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
* \param[in] pvSetBuffer    Pointer to the buffer that holds the data to be set.
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
			      IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
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
wlanoidDisableTdlsPs(IN P_ADAPTER_T prAdapter,
			 IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	struct CMD_TDLS_PS_T rTdlsPs;

	if (!prAdapter || !pvSetBuffer)
		return WLAN_STATUS_INVALID_DATA;

	rTdlsPs.ucIsEnablePs = *(PUINT_8)pvSetBuffer - '0';
	DBGLOG(OID, INFO, "enable tdls ps %d\n", rTdlsPs.ucIsEnablePs);
	wlanSendSetQueryCmd(prAdapter,
							CMD_ID_TDLS_PS,
							TRUE,
							FALSE,
							FALSE,
							NULL,
							nicOidCmdTimeoutCommon,
							sizeof(rTdlsPs),
							(PUINT_8)&rTdlsPs,
							NULL,
							0);
	return WLAN_STATUS_SUCCESS;
}

WLAN_STATUS
wlanoidSetDrvRoamingPolicy(IN P_ADAPTER_T prAdapter,
			 IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	UINT_32 u4RoamingPoily = 0;
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
		kalMemZero(prConnSettings->aucBSSID, MAC_ADDR_LEN);
	} else {
		if (prRoamingFsmInfo->eCurrentState != ROAMING_STATE_IDLE)
			roamingFsmRunEventAbort(prAdapter);
	}
	prRoamingFsmInfo->fgDrvRoamingAllow = (BOOLEAN)u4RoamingPoily;

	DBGLOG(REQ, INFO, "wlanoidSetDrvRoamingPolicy, RoamingPoily= %d, conn policy= [%d] -> [%d]\n",
			u4RoamingPoily, u4CurConPolicy, prConnSettings->eConnectionPolicy);

	return WLAN_STATUS_SUCCESS;
}

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
		DBGLOG(OID, ERROR, "FT: prFtIes->pucIEBuf memory allocation failed, ft ie_len=%u\n", ftie->ie_len);
		prFtIes->u4IeLength = 0;
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

	if (!prAdapter || !prAdapter->prAisBssInfo)
		return WLAN_STATUS_INVALID_DATA;
	prAisBssInfo = prAdapter->prAisBssInfo;
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
	DBGLOG(OID, INFO, "Send Neighbor Request, SSID=%s\n", pucSSID);
	rlmTxNeighborReportRequest(prAdapter, prAisBssInfo->prStaRecOfAP, prSSIDIE);
	kalMemFree(prSSIDIE, PHY_MEM_TYPE, ucSSIDIELen);
	return WLAN_STATUS_SUCCESS;
}

WLAN_STATUS
wlanoidSync11kCapabilities(P_ADAPTER_T prAdapter, PVOID pvSetBuffer, UINT_32 u4SetBufferLen,
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
					  (PUINT_8) &rCmdRrmCapa, pvSetBuffer, u4SetBufferLen);
}

WLAN_STATUS
wlanoidSendBTMQuery(P_ADAPTER_T prAdapter, PVOID pvSetBuffer, UINT_32 u4SetBufferLen,
							   PUINT_32 pu4SetInfoLen)
{
	P_STA_RECORD_T prStaRec = NULL;
	struct BSS_TRANSITION_MGT_PARAM_T *prBtmMgt = NULL;

	if (!prAdapter->prAisBssInfo || prAdapter->prAisBssInfo->eConnectionState !=
		PARAM_MEDIA_STATE_CONNECTED) {
		DBGLOG(OID, INFO, "Not connected yet\n");
		return WLAN_STATUS_FAILURE;
	}
	prStaRec = prAdapter->prAisBssInfo->prStaRecOfAP;
	if (!prStaRec || !prStaRec->fgSupportBTM) {
		DBGLOG(OID, INFO, "Target BSS(%p) didn't support Bss Transition Management\n", prStaRec);
		return WLAN_STATUS_FAILURE;
	}
	prBtmMgt = &prAdapter->rWifiVar.rAisSpecificBssInfo.rBTMParam;
	prBtmMgt->ucDialogToken = wnmGetBtmToken();
	prBtmMgt->ucQueryReason = pvSetBuffer ? (*(PUINT_8)pvSetBuffer - '0'):BSS_TRANSITION_LOW_RSSI;
	DBGLOG(OID, INFO, "Send BssTransitionManagementQuery, Reason %d\n", prBtmMgt->ucQueryReason);
	wnmSendBTMQueryFrame(prAdapter, prStaRec);
	return WLAN_STATUS_SUCCESS;
}

WLAN_STATUS
wlanoidSendSarEnable(P_ADAPTER_T prAdapter, PVOID pvSetBuffer, UINT_32 u4SetBufferLen,
		     PUINT_32 pu4SetInfoLen)
{
	struct CMD_SAR_ENABLE_T rCmdSarMode;
	UINT_32 u4Para = 0;

	u4Para = *((PUINT_32)pvSetBuffer);

	DBGLOG(OID, INFO, "wlanoidSendSarEnable %u\n", u4Para);

	if (u4Para > CFG_MAX_SAR_TABLE_SIZE) {
		DBGLOG(OID, ERROR,
			"u4Para > CFG_MAX_SAR_TABLE_SIZE(%u).\n",
			CFG_MAX_SAR_TABLE_SIZE);
		return WLAN_STATUS_FAILURE;
	}

	if (prAdapter == NULL) {
		DBGLOG(OID, ERROR, "prAdapter == NULL.\n");
		return WLAN_STATUS_FAILURE;
	}

	kalMemZero(&rCmdSarMode, sizeof(struct CMD_SAR_ENABLE_T));

	rCmdSarMode.u1SAREnable = (u4Para == 0) ? 0 : 1;

	if (u4Para <= 2) {
		/*old flow firmware get data from nvram.*/
		rCmdSarMode.u1CmdVersion = 2;
		rCmdSarMode.u1SAREnable = u4Para;
	} else {
		/*new flow firmware get data from driver.*/
		rCmdSarMode.u1CmdVersion = 3;

		rCmdSarMode.rTxPwrLmtSar[0].acTxPwrLimit2G =
			g_aucSarTableMainAnt[u4Para-1].acTxPwrLimit2G;
		rCmdSarMode.rTxPwrLmtSar[0].acTxPwrLimit5G[0] =
			g_aucSarTableMainAnt[u4Para-1].acTxPwrLimit5G[0];
		rCmdSarMode.rTxPwrLmtSar[0].acTxPwrLimit5G[1] =
			g_aucSarTableMainAnt[u4Para-1].acTxPwrLimit5G[1];
		rCmdSarMode.rTxPwrLmtSar[0].acTxPwrLimit5G[2] =
			g_aucSarTableMainAnt[u4Para-1].acTxPwrLimit5G[2];
		rCmdSarMode.rTxPwrLmtSar[0].acTxPwrLimit5G[3] =
			g_aucSarTableMainAnt[u4Para-1].acTxPwrLimit5G[3];

		rCmdSarMode.rTxPwrLmtSar[1].acTxPwrLimit2G =
			g_aucSarTableAuxiliaryAnt[u4Para-1].acTxPwrLimit2G;
		rCmdSarMode.rTxPwrLmtSar[1].acTxPwrLimit5G[0] =
			g_aucSarTableAuxiliaryAnt[u4Para-1].acTxPwrLimit5G[0];
		rCmdSarMode.rTxPwrLmtSar[1].acTxPwrLimit5G[1] =
			g_aucSarTableAuxiliaryAnt[u4Para-1].acTxPwrLimit5G[1];
		rCmdSarMode.rTxPwrLmtSar[1].acTxPwrLimit5G[2] =
			g_aucSarTableAuxiliaryAnt[u4Para-1].acTxPwrLimit5G[2];
		rCmdSarMode.rTxPwrLmtSar[1].acTxPwrLimit5G[3] =
			g_aucSarTableAuxiliaryAnt[u4Para-1].acTxPwrLimit5G[3];
	}

	DBGLOG(OID, INFO,
		"SAR-E(%u)-V(%u)-M-2G( %u)-5G(%u,%u,%u,%u) A-2G( %u)-5G(%u,%u,%u,%u)\n",
		rCmdSarMode.u1SAREnable,
		rCmdSarMode.u1CmdVersion,
		rCmdSarMode.rTxPwrLmtSar[0].acTxPwrLimit2G,
		rCmdSarMode.rTxPwrLmtSar[0].acTxPwrLimit5G[0],
		rCmdSarMode.rTxPwrLmtSar[0].acTxPwrLimit5G[1],
		rCmdSarMode.rTxPwrLmtSar[0].acTxPwrLimit5G[2],
		rCmdSarMode.rTxPwrLmtSar[0].acTxPwrLimit5G[3],
		rCmdSarMode.rTxPwrLmtSar[1].acTxPwrLimit2G,
		rCmdSarMode.rTxPwrLmtSar[1].acTxPwrLimit5G[0],
		rCmdSarMode.rTxPwrLmtSar[1].acTxPwrLimit5G[1],
		rCmdSarMode.rTxPwrLmtSar[1].acTxPwrLimit5G[2],
		rCmdSarMode.rTxPwrLmtSar[1].acTxPwrLimit5G[3]);

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_SET_SAR_ENABLE,
				   TRUE,
				   FALSE,
				   TRUE,
				   nicCmdEventSetCommon,
				   nicOidCmdTimeoutCommon,
				   sizeof(struct CMD_SAR_ENABLE_T),
				   (PUINT_8)&rCmdSarMode, pvSetBuffer, u4SetBufferLen);

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
	INT_32 i4Ret = 0;
	UINT_32 u4Ret = 1;
	UINT_8 ucApsdSetting = 2; /* 0: legacy; 1: u-apsd; 2: not set yet */
	enum TSPEC_OP_CODE eTsOp;

#if !CFG_SUPPORT_WMM_AC
	DBGLOG(OID, INFO, "WMM AC is not supported\n");
	return WLAN_STATUS_FAILURE;
#endif
	if (kalStrniCmp(pucCmd, "dumpts", 6) == 0) {
		i4Ret = kalSnprintf(pucCmd, u4BufferLen, "%s", "\nAll Active Tspecs:\n");

		if (i4Ret < 0)
			return WLAN_STATUS_FAILURE;

		*pu4InfoLen = (UINT_32)i4Ret;

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
	if (!prAdapter->prAisBssInfo)
		DBGLOG(OID, ERROR, "AisBssInfo is NULL!\n");
	else if (ucApsdSetting == 2) {
		P_PM_PROFILE_SETUP_INFO_T prPmProf = NULL;
		ENUM_ACI_T eAc = aucUp2ACIMap[prTspecParam->rTsInfo.ucuserPriority];

		prPmProf = &prAdapter->prAisBssInfo->rPmProfSetupInfo;
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

/* It's a Integretion Test function for RadioMeasurement. If you found errors during doing Radio Measurement,
** you can run this IT function with iwpriv wlan0 set_str_cmd \"31 RM-IT xx,xx,xx, xx\"
** xx,xx,xx,xx is the RM request frame data
*/
WLAN_STATUS
wlanoidPktProcessIT(P_ADAPTER_T prAdapter, PVOID pvBuffer, UINT_32 u4BufferLen,
		    PUINT_32 pu4InfoLen)
{
	SW_RFB_T rSwRfb;
	static UINT_8 aucPacket[200] = {0,};
	PUINT_8 pucSavedPtr = (CHAR *)pvBuffer;
	PUINT_8 pucItem = NULL;
	UINT_8 j = 0;
	INT_8 i = 0;
	UINT_8 ucByte;
	VOID(*process_func)(P_ADAPTER_T prAdapter, P_SW_RFB_T prSwRfb);

	if (!pvBuffer) {
		DBGLOG(OID, ERROR, "pvBuffer is NULL\n");
		return WLAN_STATUS_FAILURE;
	}

	if (!kalStrniCmp(pucSavedPtr, "RM-IT ", 6)) {
		process_func = rlmProcessRadioMeasurementRequest;
		pucSavedPtr += 6;
	} else if (!kalStrniCmp(pucSavedPtr, "BTM-IT ", 7)) {
		process_func = wnmRecvBTMRequest;
		pucSavedPtr += 7;
	} else {
		pucSavedPtr[10] = 0;
		DBGLOG(OID, ERROR, "IT type %s is not supported\n", pucSavedPtr);
		return WLAN_STATUS_NOT_SUPPORTED;
	}
	kalMemZero(aucPacket, sizeof(aucPacket));
	pucItem = strtok_r(pucSavedPtr, ",", (CHAR **)&pucSavedPtr);
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
	DBGLOG(OID, INFO, "Dump IT packet, len %d\n", j);
	dumpMemory8(aucPacket, j);
	if (j < WLAN_MAC_MGMT_HEADER_LEN) {
		DBGLOG(OID, ERROR, "packet length %d less than mac header 24\n", j);
		return WLAN_STATUS_FAILURE;
	}
	rSwRfb.pvHeader = (PVOID)&aucPacket[0];
	rSwRfb.u2PacketLen = j;
	rSwRfb.u2HeaderLen = WLAN_MAC_MGMT_HEADER_LEN;
	process_func(prAdapter, &rSwRfb);
	return WLAN_STATUS_SUCCESS;
}

/* Firmware Integration Test functions
** This function receives commands that are input by a firmware IT test script
** By using IT test script, RD no need to run IT with a real Access Point
** For example: iwpriv wlan0 driver \"Fw-Event Roaming ....\"
*/
WLAN_STATUS
wlanoidFwEventIT(P_ADAPTER_T prAdapter, PVOID pvBuffer, UINT_32 u4BufferLen, PUINT_32 pu4InfoLen) {
	PUINT_8 pucCmd = (CHAR *)pvBuffer;

	/* Firmware roaming Integration Test case */
	if (!kalStrniCmp(pucCmd, "Roaming", 7)) {
		UINT_8 ucRCPI = 0;
		UINT_8 ucFrameType = 0;
		UINT_32 i = 0;
		P_CMD_INFO_T prCmdInfo;
		P_GLUE_INFO_T prGlueInfo = prAdapter->prGlueInfo;
		P_WLAN_ACTION_FRAME prAction = NULL;
		P_QUE_ENTRY_T prEntry = NULL;
		P_QUE_ENTRY_T prPreEntry = NULL;

		GLUE_SPIN_LOCK_DECLARATION();

		if (prAdapter->rWifiVar.rAisFsmInfo.prTargetBssDesc)
			ucRCPI = prAdapter->rWifiVar.rAisFsmInfo.prTargetBssDesc->ucRCPI;
		roamingFsmRunEventDiscovery(prAdapter, ucRCPI);
		/* Try to find the BTM query frame which is sent by roamingFsmRunEventDiscovery */
		GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);
		for (prEntry = QUEUE_GET_HEAD(&prGlueInfo->rCmdQueue);
			prEntry != NULL; prPreEntry = prEntry,
			prEntry = QUEUE_GET_NEXT_ENTRY(&prCmdInfo->rQueEntry)) {
			prCmdInfo = (P_CMD_INFO_T)prEntry;
			if (!prCmdInfo->prMsduInfo || prCmdInfo->prMsduInfo->eSrc != TX_PACKET_MGMT ||
				!prCmdInfo->prMsduInfo->prPacket)
				continue;
			prAction = (P_WLAN_ACTION_FRAME)prCmdInfo->prMsduInfo->prPacket;
			if (prAction->u2FrameCtrl != MAC_FRAME_ACTION)
				continue;
			if (prAction->ucCategory == CATEGORY_RM_ACTION &&
				prAction->ucAction == ACTION_NEIGHBOR_REPORT_REQ) {
				ucFrameType = 1;
				break;
			}
			if (prAction->ucCategory == CATEGORY_WNM_ACTION &&
				prAction->ucAction == ACTION_WNM_BSS_TRANSITION_MANAGEMENT_QUERY) {
				ucFrameType = 2;
				break;
			}
		}
		if (prEntry) {
			if (prPreEntry) {
				prPreEntry->prNext = prEntry->prNext;
				prGlueInfo->rCmdQueue.u4NumElem--;
			} else
				QUEUE_INITIALIZE(&prGlueInfo->rCmdQueue);
		}
		GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);
		if (ucFrameType == 2) { /* roamingFsmRunEventDiscovery has sent a btm query frame */
			struct ACTION_BTM_QUERY_FRAME_T *prBtmQuery = (struct ACTION_BTM_QUERY_FRAME_T *)prAction;

			/* IT string may be "Roaming <btm request packet string>", to reuse btm it function,
			** we need to replace Roaming with BTM-IT. Length of Roaming is 7 bytes, so pucCmd
			** need to self add 1, and buffer length need to self minus 1, and copy BTM-IT to pucCmd.
			*/
			pucCmd++;
			u4BufferLen--;
			kalMemCopy(pucCmd, "BTM-IT", 6);

			/* Find the diaglogToken string in <btm request packet string>,
			** it follows "BTM-IT ", whose length is 7
			*/
			for (ucRCPI = 0, i = 7; i < u4BufferLen; i++) {
				if (pucCmd[i] == ',')
					ucRCPI++;
				if (ucRCPI == OFFSET_OF(struct ACTION_BTM_QUERY_FRAME_T, ucDialogToken))
					break;
			}
			/* Replace diaglog token string with the token that is in query frame */
			ucRCPI = prBtmQuery->ucDialogToken;
			ucFrameType = (ucRCPI >> 4) & 0xf;
			if (ucFrameType > 9)
				pucCmd[++i] = ucFrameType + 'a' - 10;
			else
				pucCmd[++i] = ucFrameType + '0';
			ucFrameType = ucRCPI & 0xf;
			if (ucFrameType > 9)
				pucCmd[++i] = ucFrameType + 'a' - 10;
			else
				pucCmd[++i] = ucFrameType + '0';
			wlanoidPktProcessIT(prAdapter, (PVOID)pucCmd, u4BufferLen, pu4InfoLen);
		} else if (ucFrameType == 1) {
			/* Not support neighbor ap report request IT now */
		}
	} else {
		DBGLOG(OID, ERROR, "Not supported Fw Event IT type %s\n", pucCmd);
		return WLAN_STATUS_FAILURE;
	}
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
	if (!prAdapter->prAisBssInfo)
		return WLAN_STATUS_FAILURE;
	prPmProf = &prAdapter->prAisBssInfo->rPmProfSetupInfo;
	ucStaticSetting = (prPmProf->ucBmpDeliveryAC << 4) | prPmProf->ucBmpTriggerAC;
	ucFinalSetting = wmmCalculateUapsdSetting(prAdapter);
	*pu4InfoLen = kalSnprintf(pucCmd, u4BufferLen,
		"\nStatic Uapsd Setting:0x%02x\nFinal Uapsd Setting:0x%02x", ucStaticSetting, ucFinalSetting);

	if (*pu4InfoLen < 0)
		return WLAN_STATUS_FAILURE;

	return WLAN_STATUS_SUCCESS;
}

#if CFG_SUPPORT_LOWLATENCY_MODE
/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to enable/disable LowLatency mode
*
* \param[in]  pvAdapter       A pointer to the Adapter structure.
* \param[in]  pvSetBuffer     A pointer to the buffer that holds the data to be set.
* \param[in]  u4SetBufferLen  The length of the set buffer.
* \param[out] pu4SetInfoLen   If the call is successful, returns the number of
*                             bytes read from the set buffer. If the call failed
*                             due to invalid length of the set buffer, returns
*                             the amount of storage needed.
*
* \retval WLAN_STATUS_PENDING
* \retval WLAN_STATUS_FAILURE
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanoidSetLowLatencyMode(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer,
				 IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	struct CMD_LOWLATENCY_MODE_HEADER rLowLatencyModeHeader;
	BOOLEAN fgEnable, fgEnScan, fgLatencyStat;
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;
	UINT_32 u4Events;
	PARAM_POWER_MODE_T rPowerMode;
	UINT_32 u4OldValue;

	DEBUGFUNC("wlanoidSetLowLatencyMode");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	if (u4SetBufferLen < sizeof(UINT_32))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	kalMemCopy(&u4Events, pvSetBuffer, u4SetBufferLen);

	DBGLOG(OID, TRACE, "Event change gaming=%d, network:%d whitelist:%d\n",
	       (u4Events & GED_EVENT_GAS),
	       (u4Events & GED_EVENT_NETWORK),
	       (u4Events & GED_EVENT_DOPT_WIFI_SCAN));

	fgEnable = ((u4Events & GED_EVENT_GAS) != 0) && ((u4Events & GED_EVENT_NETWORK) != 0);

	if (fgEnable && (kalGetMediaStateIndicated(prAdapter->prGlueInfo) != PARAM_MEDIA_STATE_CONNECTED))
		return WLAN_STATUS_SUCCESS;
	fgEnScan = (!fgEnable || ((u4Events & GED_EVENT_DOPT_WIFI_SCAN) != 0));

	rPowerMode.ucBssIdx = prAdapter->prAisBssInfo->ucBssIndex;

	DBGLOG(OID, INFO, "Gaming mode event change Enable=%d EnableScan:%d\n",
	       fgEnable, fgEnScan);

	prAdapter->fgEnCfg80211Scan = fgEnScan;

	u4OldValue = prAdapter->rWlanInfo.u4PowerSaveFlag[rPowerMode.ucBssIdx];
	fgLatencyStat = (u4OldValue & BIT(PS_CALLER_GPU)) ? TRUE : FALSE;
	DBGLOG(OID, INFO, "LowLatency mode fgLatencyStat=%d\n", fgLatencyStat);
	if (fgEnable == fgLatencyStat)
		return WLAN_STATUS_SUCCESS;

	prAdapter->u4QmRxBaMissTimeout = fgEnable ?
		SHORT_QM_RX_BA_ENTRY_MISS_TIMEOUT_MS : DEFAULT_QM_RX_BA_ENTRY_MISS_TIMEOUT_MS;

	rLowLatencyModeHeader.ucVersion = LOWLATENCY_MODE_CMD_V1;
	rLowLatencyModeHeader.ucType = 0;
	rLowLatencyModeHeader.ucMagicCode = LOWLATENCY_MODE_MAGIC_CODE;
	rLowLatencyModeHeader.ucBufferLen = sizeof(struct LOWLATENCY_MODE_SETTING);
	rLowLatencyModeHeader.rSetting.fgEnable = fgEnable;

	DBGLOG(OID, INFO, "Set LowLatency mode enable=%d\n", fgEnable);


	if (fgEnable)
		rPowerMode.ePowerMode = Param_PowerModeCAM;
	else
		rPowerMode.ePowerMode = Param_PowerModeFast_PSP;

	wlanSendSetQueryCmd(prAdapter,	/* prAdapter */
			      CMD_ID_ENABLE_LOW_LATENCEY_MODE,	/* ucCID */
			      TRUE,	/* fgSetQuery */
			      FALSE,	/* fgNeedResp */
			      FALSE,	/* fgIsOid */
			      NULL,	/* pfCmdDoneHandler */
			      NULL,	/* pfCmdTimeoutHandler */
			      sizeof(struct CMD_LOWLATENCY_MODE_HEADER),	/* u4SetQueryInfoLen */
			      (PUINT_8) & rLowLatencyModeHeader,	/* pucInfoBuffer */
			      NULL,	/* pvSetQueryBuffer */
			      0	/* u4SetQueryBufferLen */
	);


	nicPowerSaveInfoMap(prAdapter, rPowerMode.ucBssIdx, rPowerMode.ePowerMode, PS_CALLER_GPU);
	rStatus = nicConfigPowerSaveProfile(prAdapter,
					rPowerMode.ucBssIdx, rPowerMode.ePowerMode,
					FALSE);

	return rStatus;
}
#endif /* CFG_SUPPORT_LOWLATENCY_MODE */

/* Oshare mode enable command */
#if CFG_SUPPORT_OSHARE
WLAN_STATUS wlanoidSetOshareMode(IN P_ADAPTER_T prAdapter,
				 IN PVOID pvSetBuffer,
				 IN UINT_32 u4SetBufferLen,
				 OUT PUINT_32 pu4SetInfoLen)
{
	if (!prAdapter || !pvSetBuffer)
		return WLAN_STATUS_INVALID_DATA;

	DBGLOG(OID, TRACE, "wlanoidSetOshareMode\n");

	return wlanSendSetQueryCmd(prAdapter, /* prAdapter */
				   CMD_ID_SET_OSHARE_MODE, /* ucCID */
				   TRUE, /* fgSetQuery */
				   FALSE, /* fgNeedResp */
				   TRUE, /* fgIsOid */
				   nicCmdEventSetCommon, /* pfCmdDoneHandler*/
				   nicOidCmdTimeoutCommon, /* pfCmdTimeoutHandler */
				   u4SetBufferLen, /* u4SetQueryInfoLen */
				   (PUINT_8) pvSetBuffer,/* pucInfoBuffer */
				   NULL, /* pvSetQueryBuffer */
				   0); /* u4SetQueryBufferLen */
}
#endif

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

WLAN_STATUS
wlanoidEnableRoaming(IN P_ADAPTER_T prAdapter,
		     IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	DBGLOG(OID, INFO, "enable roaming\n");

	aisRemoveBlacklistBySource(prAdapter, AIS_BLACK_LIST_FROM_FWK);

	return WLAN_STATUS_SUCCESS;
}

WLAN_STATUS
wlanoidConfigRoaming(IN P_ADAPTER_T prAdapter,
		     IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
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

	if (numOfList[0] >= 0 && numOfList[0] <= MAX_FW_ROAMING_BLACKLIST_SIZE) {
		/*Refresh all the FWKBlacklist */
		aisRemoveBlacklistBySource(prAdapter, AIS_BLACK_LIST_FROM_FWK);

		/* Start to receive blacklist mac addresses and set to FWK blacklist */
		attrlist = (struct nlattr *)((UINT_8 *) pvSetBuffer + len_shift);
		for (i = 0; i < numOfList[0]; i++) {
			if (attrlist->nla_type == WIFI_ATTRIBUTE_ROAMING_BLACKLIST_BSSID)
				prBlackList = aisAddBlacklistByBssid(prAdapter, nla_data(attrlist),
									AIS_BLACK_LIST_FROM_FWK);
			len_shift += NLA_ALIGN(attrlist->nla_len);
			attrlist = (struct nlattr *)((UINT_8 *) pvSetBuffer + len_shift);
		}
	}

	return WLAN_STATUS_SUCCESS;
}

uint32_t wlanoidGetWifiType(IN P_ADAPTER_T prAdapter,
			    IN void *pvSetBuffer,
			    IN uint32_t u4SetBufferLen,
			    OUT uint32_t *pu4SetInfoLen)
{
	struct PARAM_GET_WIFI_TYPE *prParamGetWifiType;
	P_NETDEV_PRIVATE_GLUE_INFO prNetDevPrivate;
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
	prNetDevPrivate = (P_NETDEV_PRIVATE_GLUE_INFO)
				netdev_priv(prParamGetWifiType->prNetDev);
	ucBssIdx = prNetDevPrivate->ucBssIdx;

	DBGLOG(OID, INFO, "bss index=%d\n", ucBssIdx);

	kalMemZero(prParamGetWifiType->arWifiTypeName,
		   sizeof(prParamGetWifiType->arWifiTypeName));
	pNameBuf = &prParamGetWifiType->arWifiTypeName[0];
	ucMaxCopySize = sizeof(prParamGetWifiType->arWifiTypeName) - 1;

	if (ucBssIdx > HW_BSSID_NUM) {
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
	if (ucPhyType & PHY_TYPE_SET_802_11AC)
		kalStrnCpy(pNameBuf, "11AC", ucMaxCopySize);
	else if (ucPhyType & PHY_TYPE_SET_802_11N)
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
wlanoidSetScanMacOui(IN P_ADAPTER_T prAdapter,
		     IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	struct PARAM_BSS_MAC_OUI *prParamMacOui;
	P_BSS_INFO_T prBssInfo;

	ASSERT(prAdapter);
	ASSERT(prAdapter->prGlueInfo);
	ASSERT(pvSetBuffer);
	ASSERT(u4SetBufferLen == sizeof(struct PARAM_BSS_MAC_OUI));

	prParamMacOui = (struct PARAM_BSS_MAC_OUI *)pvSetBuffer;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prParamMacOui->ucBssIndex);
	if (!prBssInfo) {
		DBGLOG(REQ, ERROR, "Invalid bss info (ind=%u)\n",
			prParamMacOui->ucBssIndex);
		return WLAN_STATUS_FAILURE;
	}

	kalMemCopy(prBssInfo->ucScanOui, prParamMacOui->ucMacOui, MAC_OUI_LEN);
	prBssInfo->fgIsScanOuiSet = TRUE;
	*pu4SetInfoLen = MAC_OUI_LEN;

	return WLAN_STATUS_SUCCESS;
}
WLAN_STATUS
wlanoidStopApRole(P_ADAPTER_T prAdapter, void *pvSetBuffer,
	UINT_32 u4SetBufferLen, UINT_32 *pu4SetInfoLen)
{
	unsigned char u4Idx = 0;
	P_MSG_P2P_SWITCH_OP_MODE_T prP2pSwitchMode = (P_MSG_P2P_SWITCH_OP_MODE_T) NULL;

	if ((prAdapter == NULL) || (pvSetBuffer == NULL)
		|| (pu4SetInfoLen == NULL)) {
		DBGLOG(OID, WARN,
			"(prAdapter == NULL) ||(pvSetBuffer == NULL) ||(pu4SetInfoLen == NULL)\n");
		return WLAN_STATUS_FAILURE;
	}

	*pu4SetInfoLen = sizeof(unsigned char);
	if (u4SetBufferLen < sizeof(unsigned char)) {
		DBGLOG(OID, WARN, "u4SetBufferLen < sizeof(unsigned char)\n");
		return WLAN_STATUS_INVALID_LENGTH;
	}

	u4Idx = *(unsigned char *) pvSetBuffer;

	DBGLOG(OID, INFO, "wlanoidStopApRole ucRoleIdx = %d\n", u4Idx);

	prP2pSwitchMode = cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_P2P_SWITCH_OP_MODE_T));
	if (prP2pSwitchMode == NULL) {
		DBGLOG(OID, WARN, "prP2pSwitchMode == NULL.\n");
		return WLAN_STATUS_FAILURE;
	}

	prP2pSwitchMode->rMsgHdr.eMsgId = MID_MNY_P2P_STOP_AP;
	prP2pSwitchMode->ucRoleIdx = u4Idx;
	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prP2pSwitchMode, MSG_SEND_METHOD_UNBUF);

	DBGLOG(OID, INFO, "done, ucRoleIdx = %d\n", u4Idx);

	return WLAN_STATUS_SUCCESS;

}
#ifdef CFG_SUPPORT_LINK_QUALITY_MONITOR
/* link quality monitor */
UINT_32 wlanoidGetLinkQualityInfo(IN struct _ADAPTER_T *prAdapter,
				   IN void *pvSetBuffer,
				   IN UINT_32 u4SetBufferLen,
				   OUT UINT_32 *pu4SetInfoLen)
{
	struct PARAM_GET_LINK_QUALITY_INFO *prParam;
	struct WIFI_LINK_QUALITY_INFO *prSrcLinkQualityInfo = NULL;
	struct WIFI_LINK_QUALITY_INFO *prDstLinkQualityInfo = NULL;

	prParam = (struct PARAM_GET_LINK_QUALITY_INFO *)pvSetBuffer;
	prSrcLinkQualityInfo = &(prAdapter->rLinkQualityInfo);
	prDstLinkQualityInfo = prParam->prLinkQualityInfo;
	kalMemCopy(prDstLinkQualityInfo, prSrcLinkQualityInfo,
		   sizeof(struct WIFI_LINK_QUALITY_INFO));

	return WLAN_STATUS_SUCCESS;
}
#endif

uint32_t
wlanoidExternalAuthDone(IN struct _ADAPTER_T *prAdapter,
			IN void *pvSetBuffer,
			IN uint32_t u4SetBufferLen,
			OUT uint32_t *pu4SetInfoLen)
{
	struct _STA_RECORD_T *prStaRec;
	struct PARAM_EXTERNAL_AUTH *prParams;
	struct MSG_SAA_EXTERNAL_AUTH_DONE *prExternalAuthMsg = NULL;
	struct _AIS_FSM_INFO_T *prAisFsmInfo;
	UINT_8 aucZeroMacAddr[] = NULL_MAC_ADDR;
	UINT_8 ucBssIndex = 0;

	prParams = (struct PARAM_EXTERNAL_AUTH *) pvSetBuffer;
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	ucBssIndex = prParams->ucBssIdx;
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

	/*In case of sometimes the external auth result*/
	/*did not contain BSSID, we use prTargetBssDesc*/
	if (EQUAL_MAC_ADDR(prParams->bssid, aucZeroMacAddr) &&
	    prAisFsmInfo->prTargetBssDesc != NULL) {
		DBGLOG(OID, WARN, "BSSID is Null, Set as AIS target BSSID\n");
		COPY_MAC_ADDR(prParams->bssid, prAisFsmInfo->prTargetBssDesc->aucBSSID);
	}

	prStaRec = cnmGetStaRecByAddress(prAdapter, ucBssIndex, prParams->bssid);
	if (!prStaRec) {
		DBGLOG(REQ, WARN, "SAE-confirm failed with bssid:" MACSTR "\n",
		       prParams->bssid);
		return WLAN_STATUS_INVALID_DATA;
	}

	prExternalAuthMsg->rMsgHdr.eMsgId = MID_OID_SAA_FSM_EXTERNAL_AUTH;
	prExternalAuthMsg->prStaRec = prStaRec;
	prExternalAuthMsg->status = prParams->status;

	mboxSendMsg(prAdapter, MBOX_ID_0, (struct _MSG_HDR_T *)prExternalAuthMsg,
		    MSG_SEND_METHOD_BUF);

	return WLAN_STATUS_SUCCESS;
}

WLAN_STATUS
wlanoidSetP2pRandomMac(P_ADAPTER_T prAdapter, void *pvSetBuffer,
	UINT_32 u4SetBufferLen, UINT_32 *pu4SetInfoLen)
{
	P_BSS_INFO_T prDevBssInfo = NULL;
	P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo = NULL;
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_BSS_INFO_T prRoleBssInfo = NULL;

	if ((prAdapter == NULL) || (pvSetBuffer == NULL)
		|| (pu4SetInfoLen == NULL)) {
		DBGLOG(OID, WARN,
			"(prAdapter == NULL) ||(pvSetBuffer == NULL) ||(pu4SetInfoLen == NULL)\n");
		return WLAN_STATUS_FAILURE;
	}

	DBGLOG(OID, INFO, "wlanoidSetP2pRandomMac.\n");
	/*Todo: block set random mac when p2p goup work or hotspot work*/

	prDevBssInfo = prAdapter->aprBssInfo[P2P_DEV_BSS_INDEX];
	if (prDevBssInfo == NULL) {
		DBGLOG(OID, WARN, "prDevBssInfo == NULL\n");
		return WLAN_STATUS_FAILURE;
	}

	prP2pRoleFsmInfo = prAdapter->rWifiVar.aprP2pRoleFsmInfo[0];
	if (prP2pRoleFsmInfo == NULL) {
		DBGLOG(OID, WARN,
			"prP2pRoleFsmInfo == NULL\n");
		return WLAN_STATUS_FAILURE;
	}

	if (prP2pRoleFsmInfo->ucBssIndex > BSS_INFO_NUM) {
		DBGLOG(OID, WARN,
			"Invalid Bssindex %u.\n", prP2pRoleFsmInfo->ucBssIndex);
		return WLAN_STATUS_FAILURE;
	}

	prRoleBssInfo = prAdapter->aprBssInfo[prP2pRoleFsmInfo->ucBssIndex];
	if (prRoleBssInfo == NULL) {
		DBGLOG(OID, WARN, "prDevBssInfo == NULL\n");
		return WLAN_STATUS_FAILURE;
	}

	prGlueInfo = prAdapter->prGlueInfo;
	if ((prGlueInfo == NULL) || (prGlueInfo->prP2PInfo == NULL)
		|| (prGlueInfo->prP2PInfo->prDevHandler == NULL)) {
		DBGLOG(OID, WARN, "invalid parameter.\n");
		return WLAN_STATUS_FAILURE;
	}

	/* 1. update dev bss*/
	COPY_MAC_ADDR(prDevBssInfo->aucOwnMacAddr,
		(PUINT_8)pvSetBuffer);

	/* 2. update role bss, role index is always 0 in gen3 */
	 COPY_MAC_ADDR(prRoleBssInfo->aucOwnMacAddr,
		(PUINT_8)pvSetBuffer);

	/* 3. update netdevice addr*/
	COPY_MAC_ADDR(prGlueInfo->prP2PInfo->prDevHandler->dev_addr,
		(PUINT_8)pvSetBuffer);

	/* 4. update dev addr in wifivar*/
	COPY_MAC_ADDR(prAdapter->rWifiVar.aucDeviceAddress,
			(PUINT_8)pvSetBuffer);

	/* 5. update interface addr in wifivar*/
	COPY_MAC_ADDR(prAdapter->rWifiVar.aucInterfaceAddress,
		(PUINT_8)pvSetBuffer);

	DBGLOG(OID, INFO, "Done, Set p2p random mac to"MACSTR"\n",
		MAC2STR((PUINT_8)pvSetBuffer));

	return WLAN_STATUS_SUCCESS;

}

