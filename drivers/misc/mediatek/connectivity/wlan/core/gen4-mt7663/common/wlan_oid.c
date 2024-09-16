/*******************************************************************************
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
 ******************************************************************************/
/*
 ** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/common
 *      /wlan_oid.c#11
 */

/*! \file wlanoid.c
 *   \brief This file contains the WLAN OID processing routines of Windows
 *          driver for MediaTek Inc. 802.11 Wireless LAN Adapters.
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
#include "mgmt/rsn.h"
#include "gl_wext.h"
#include "debug.h"
#include <stddef.h>

/******************************************************************************
 *                              C O N S T A N T S
 ******************************************************************************
 */

/******************************************************************************
 *                             D A T A   T Y P E S
 ******************************************************************************
 */

/******************************************************************************
 *                            P U B L I C   D A T A
 ******************************************************************************
 */
struct PARAM_CUSTOM_KEY_CFG_STRUCT g_rDefaulteSetting[] = {
	/*format :
	 *: {"firmware config parameter", "firmware config value"}
	 */
	{"AdapScan", "0x0"}
};


/******************************************************************************
 *                           P R I V A T E   D A T A
 ******************************************************************************
 */

/******************************************************************************
 *                                 M A C R O S
 ******************************************************************************
 */

/******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 ******************************************************************************
 */
#if DBG && 0
static void SetRCID(u_int8_t fgOneTb3, u_int8_t *fgRCID);
#endif

#if CFG_SLT_SUPPORT
static void SetTestChannel(uint8_t *pucPrimaryChannel);
#endif

/******************************************************************************
 *                              F U N C T I O N S
 ******************************************************************************
 */
static void setApUapsdEnable(struct ADAPTER *prAdapter,
			     u_int8_t enable)
{
	struct PARAM_CUSTOM_UAPSD_PARAM_STRUCT rUapsdParams;
	uint32_t u4SetInfoLen = 0;
	uint8_t ucBssIdx;

	/* FIX ME: Add p2p role index selection */
	if (p2pFuncRoleToBssIdx(
		    prAdapter, 0, &ucBssIdx) != WLAN_STATUS_SUCCESS)
		return;

	DBGLOG(OID, INFO, "setApUapsdEnable: %d, ucBssIdx: %d\n",
	       enable, ucBssIdx);

	rUapsdParams.ucBssIdx = ucBssIdx;

	if (enable) {
		prAdapter->rWifiVar.ucApUapsd = TRUE;
		rUapsdParams.fgEnAPSD = 1;
		rUapsdParams.fgEnAPSD_AcBe = 1;
		rUapsdParams.fgEnAPSD_AcBk = 1;
		rUapsdParams.fgEnAPSD_AcVi = 1;
		rUapsdParams.fgEnAPSD_AcVo = 1;
		/* default: 0, do not limit delivery pkt number */
		rUapsdParams.ucMaxSpLen = 0;
	} else {
		prAdapter->rWifiVar.ucApUapsd = FALSE;
		rUapsdParams.fgEnAPSD = 0;
		rUapsdParams.fgEnAPSD_AcBe = 0;
		rUapsdParams.fgEnAPSD_AcBk = 0;
		rUapsdParams.fgEnAPSD_AcVi = 0;
		rUapsdParams.fgEnAPSD_AcVo = 0;
		/* default: 0, do not limit delivery pkt number */
		rUapsdParams.ucMaxSpLen = 0;
	}
	wlanoidSetUApsdParam(prAdapter,
			     &rUapsdParams,
			     sizeof(struct PARAM_CUSTOM_UAPSD_PARAM_STRUCT),
			     &u4SetInfoLen);
}

#if CFG_ENABLE_STATISTICS_BUFFERING
static u_int8_t IsBufferedStatisticsUsable(
	struct ADAPTER *prAdapter)
{
	ASSERT(prAdapter);

	if (prAdapter->fgIsStatValid == TRUE &&
	    (kalGetTimeTick() - prAdapter->rStatUpdateTime) <=
	    CFG_STATISTICS_VALID_CYCLE)
		return TRUE;
	else
		return FALSE;
}
#endif

#if DBG && 0
static void SetRCID(u_int8_t fgOneTb3, u_int8_t *fgRCID)
{
	if (fgOneTb3)
		*fgRCID = 0;
	else
		*fgRCID = 1;
}
#endif

#if CFG_SLT_SUPPORT
static void SetTestChannel(uint8_t *pucPrimaryChannel)
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
uint32_t
wlanoidQueryNetworkTypesSupported(IN struct ADAPTER
				  *prAdapter,
				  OUT void *pvQueryBuffer,
				  IN uint32_t u4QueryBufferLen,
				  OUT uint32_t *pu4QueryInfoLen)
{
	uint32_t u4NumItem = 0;
	enum ENUM_PARAM_NETWORK_TYPE
	eSupportedNetworks[PARAM_NETWORK_TYPE_NUM];
	struct PARAM_NETWORK_TYPE_LIST *prSupported;

	/* The array of all physical layer network subtypes that the driver
	 * supports.
	 */

	DEBUGFUNC("wlanoidQueryNetworkTypesSupported");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	/* Init. */
	for (u4NumItem = 0; u4NumItem < PARAM_NETWORK_TYPE_NUM;
	     u4NumItem++)
		eSupportedNetworks[u4NumItem] = 0;

	u4NumItem = 0;

	eSupportedNetworks[u4NumItem] = PARAM_NETWORK_TYPE_DS;
	u4NumItem++;

	eSupportedNetworks[u4NumItem] = PARAM_NETWORK_TYPE_OFDM24;
	u4NumItem++;

	*pu4QueryInfoLen =
		(uint32_t) OFFSET_OF(struct PARAM_NETWORK_TYPE_LIST,
				     eNetworkType) +
		(u4NumItem * sizeof(enum ENUM_PARAM_NETWORK_TYPE));

	if (u4QueryBufferLen < *pu4QueryInfoLen)
		return WLAN_STATUS_INVALID_LENGTH;

	prSupported = (struct PARAM_NETWORK_TYPE_LIST *)
		      pvQueryBuffer;
	prSupported->NumberOfItems = u4NumItem;
	kalMemCopy(prSupported->eNetworkType, eSupportedNetworks,
		   u4NumItem * sizeof(enum ENUM_PARAM_NETWORK_TYPE));

	DBGLOG(REQ, TRACE, "NDIS supported network type list: %u\n",
	       prSupported->NumberOfItems);
	DBGLOG_MEM8(REQ, INFO, prSupported, *pu4QueryInfoLen);

	return WLAN_STATUS_SUCCESS;
} /* wlanoidQueryNetworkTypesSupported */

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
uint32_t
wlanoidQueryNetworkTypeInUse(IN struct ADAPTER *prAdapter,
			     OUT void *pvQueryBuffer,
			     IN uint32_t u4QueryBufferLen,
			     OUT uint32_t *pu4QueryInfoLen)
{
	/* TODO: need to check the OID handler content again!! */

	enum ENUM_PARAM_NETWORK_TYPE rCurrentNetworkTypeInUse =
		PARAM_NETWORK_TYPE_OFDM24;

	DEBUGFUNC("wlanoidQueryNetworkTypeInUse");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (u4QueryBufferLen < sizeof(enum
				      ENUM_PARAM_NETWORK_TYPE)) {
		*pu4QueryInfoLen = sizeof(enum ENUM_PARAM_NETWORK_TYPE);
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}

	if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) ==
	    PARAM_MEDIA_STATE_CONNECTED)
		rCurrentNetworkTypeInUse = (enum ENUM_PARAM_NETWORK_TYPE) (
				prAdapter->rWlanInfo.ucNetworkType);
	else
		rCurrentNetworkTypeInUse = (enum ENUM_PARAM_NETWORK_TYPE) (
				prAdapter->rWlanInfo.ucNetworkTypeInUse);

	*(enum ENUM_PARAM_NETWORK_TYPE *) pvQueryBuffer =
		rCurrentNetworkTypeInUse;
	*pu4QueryInfoLen = sizeof(enum ENUM_PARAM_NETWORK_TYPE);

	DBGLOG(REQ, TRACE, "Network type in use: %d\n",
	       rCurrentNetworkTypeInUse);

	return WLAN_STATUS_SUCCESS;
} /* wlanoidQueryNetworkTypeInUse */

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
uint32_t
wlanoidSetNetworkTypeInUse(IN struct ADAPTER *prAdapter,
			   IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			   OUT uint32_t *pu4SetInfoLen)
{
	/* TODO: need to check the OID handler content again!! */

	enum ENUM_PARAM_NETWORK_TYPE eNewNetworkType;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidSetNetworkTypeInUse");

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	if (u4SetBufferLen < sizeof(enum ENUM_PARAM_NETWORK_TYPE)) {
		*pu4SetInfoLen = sizeof(enum ENUM_PARAM_NETWORK_TYPE);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	eNewNetworkType = *(enum ENUM_PARAM_NETWORK_TYPE *)
			  pvSetBuffer;
	*pu4SetInfoLen = sizeof(enum ENUM_PARAM_NETWORK_TYPE);

	DBGLOG(REQ, INFO, "New network type: %d mode\n",
	       eNewNetworkType);

	switch (eNewNetworkType) {

	case PARAM_NETWORK_TYPE_DS:
		prAdapter->rWlanInfo.ucNetworkTypeInUse =
			(uint8_t) PARAM_NETWORK_TYPE_DS;
		break;

	case PARAM_NETWORK_TYPE_OFDM5:
		prAdapter->rWlanInfo.ucNetworkTypeInUse =
			(uint8_t) PARAM_NETWORK_TYPE_OFDM5;
		break;

	case PARAM_NETWORK_TYPE_OFDM24:
		prAdapter->rWlanInfo.ucNetworkTypeInUse =
			(uint8_t) PARAM_NETWORK_TYPE_OFDM24;
		break;

	case PARAM_NETWORK_TYPE_AUTOMODE:
		prAdapter->rWlanInfo.ucNetworkTypeInUse =
			(uint8_t) PARAM_NETWORK_TYPE_AUTOMODE;
		break;

	case PARAM_NETWORK_TYPE_FH:
		DBGLOG(REQ, INFO, "Not support network type: %d\n",
		       eNewNetworkType);
		rStatus = WLAN_STATUS_NOT_SUPPORTED;
		break;

	default:
		DBGLOG(REQ, INFO, "Unknown network type: %d\n",
		       eNewNetworkType);
		rStatus = WLAN_STATUS_INVALID_DATA;
		break;
	}

	/* Verify if we support the new network type. */
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(REQ, WARN, "Unknown network type: %d\n",
		       eNewNetworkType);

	return rStatus;
} /* wlanoidSetNetworkTypeInUse */

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
uint32_t
wlanoidQueryBssid(IN struct ADAPTER *prAdapter,
		  OUT void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
		  OUT uint32_t *pu4QueryInfoLen)
{
	uint32_t rStatus = WLAN_STATUS_SUCCESS;

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

	if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) ==
	    PARAM_MEDIA_STATE_CONNECTED)
		kalMemCopy(pvQueryBuffer,
			   prAdapter->rWlanInfo.rCurrBssId.arMacAddress,
			   MAC_ADDR_LEN);
	else if (prAdapter->rWifiVar.rConnSettings.eOPMode ==
		 NET_TYPE_IBSS) {
		uint8_t aucTemp[PARAM_MAC_ADDR_LEN];	/*!< BSSID */

		COPY_MAC_ADDR(aucTemp,
			      prAdapter->rWlanInfo.rCurrBssId.arMacAddress);
		aucTemp[0] &= ~BIT(0);
		aucTemp[1] |= BIT(1);
		COPY_MAC_ADDR(pvQueryBuffer, aucTemp);
	} else
		rStatus = WLAN_STATUS_ADAPTER_NOT_READY;

	*pu4QueryInfoLen = MAC_ADDR_LEN;
	return rStatus;
} /* wlanoidQueryBssid */

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
uint32_t
wlanoidQueryBssidList(IN struct ADAPTER *prAdapter,
		      OUT void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
		      OUT uint32_t *pu4QueryInfoLen)
{
	struct GLUE_INFO *prGlueInfo;
	uint32_t i, u4BssidListExLen;
	struct PARAM_BSSID_LIST_EX *prList;
	struct PARAM_BSSID_EX *prBssidEx;
	uint8_t *cp;

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
			u4BssidListExLen += ALIGN_4(
				prAdapter->rWlanInfo.arScanResult[i].u4Length);
	}

	if (u4BssidListExLen)
		u4BssidListExLen += 4;	/* u4NumberOfItems. */
	else
		u4BssidListExLen = sizeof(struct PARAM_BSSID_LIST_EX);

	*pu4QueryInfoLen = u4BssidListExLen;

	if (u4QueryBufferLen < *pu4QueryInfoLen)
		return WLAN_STATUS_INVALID_LENGTH;

	/* Clear the buffer */
	kalMemZero(pvQueryBuffer, u4BssidListExLen);

	prList = (struct PARAM_BSSID_LIST_EX *) pvQueryBuffer;
	cp = (uint8_t *) &prList->arBssid[0];

	if (prAdapter->fgIsRadioOff == FALSE
	    && prAdapter->rWlanInfo.u4ScanResultNum > 0) {
		/* fill up for each entry */
		for (i = 0; i < prAdapter->rWlanInfo.u4ScanResultNum; i++) {
			prBssidEx = (struct PARAM_BSSID_EX *) cp;

			/* copy structure */
			kalMemCopy(prBssidEx,
				   &(prAdapter->rWlanInfo.arScanResult[i]),
				   OFFSET_OF(struct PARAM_BSSID_EX, aucIEs));

			/* For WHQL test, Rssi should be
			 * in range -10 ~ -200 dBm
			 */
			if (prBssidEx->rRssi > PARAM_WHQL_RSSI_MAX_DBM)
				prBssidEx->rRssi = PARAM_WHQL_RSSI_MAX_DBM;

			if (prAdapter->rWlanInfo.arScanResult[i].u4IELength
			    > 0) {
				/* copy IEs */
				kalMemCopy(prBssidEx->aucIEs,
				    prAdapter->rWlanInfo.apucScanResultIEs[i],
				    prAdapter->rWlanInfo.arScanResult[i]
				    .u4IELength);
			}
			/* 4-bytes alignement */
			prBssidEx->u4Length = ALIGN_4(prBssidEx->u4Length);

			cp += prBssidEx->u4Length;
			prList->u4NumberOfItems++;
		}
	}

	return WLAN_STATUS_SUCCESS;
} /* wlanoidQueryBssidList */

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
uint32_t
wlanoidSetBssidListScan(IN struct ADAPTER *prAdapter,
			IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			OUT uint32_t *pu4SetInfoLen)
{
	struct PARAM_SSID *prSsid;
	struct PARAM_SSID rSsid;

	DEBUGFUNC("wlanoidSetBssidListScan()");

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

	if (prAdapter->fgIsRadioOff) {
		DBGLOG(OID, WARN,
		       "Return from BSSID list scan! (radio off). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_SUCCESS;
	}

	if (pvSetBuffer != NULL && u4SetBufferLen != 0) {
		COPY_SSID(rSsid.aucSsid, rSsid.u4SsidLen, pvSetBuffer,
			  u4SetBufferLen);
		prSsid = &rSsid;
	} else {
		prSsid = NULL;
	}

#if CFG_SUPPORT_RDD_TEST_MODE
	if (prAdapter->prGlueInfo->rRegInfo.u4RddTestMode) {
		if ((prAdapter->fgEnOnlineScan == TRUE)
		    && (prAdapter->ucRddStatus)) {
			if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) !=
			    PARAM_MEDIA_STATE_CONNECTED)
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
		else if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) !=
			 PARAM_MEDIA_STATE_CONNECTED)
			aisFsmScanRequest(prAdapter, prSsid, NULL, 0);
		else
			return WLAN_STATUS_FAILURE;
	}

	return WLAN_STATUS_SUCCESS;
} /* wlanoidSetBssidListScan */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to request the driver to perform
 *        scanning with attaching information elements(IEs) specified from user
 *        space
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
uint32_t
wlanoidSetBssidListScanExt(IN struct ADAPTER *prAdapter,
			   IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			   OUT uint32_t *pu4SetInfoLen)
{
	struct PARAM_SCAN_REQUEST_EXT *prScanRequest;
	struct PARAM_SSID *prSsid;
	uint8_t *pucIe;
	uint32_t u4IeLength;

	DEBUGFUNC("wlanoidSetBssidListScanExt()");

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

	if (u4SetBufferLen != sizeof(struct PARAM_SCAN_REQUEST_EXT))
		return WLAN_STATUS_INVALID_LENGTH;

	if (prAdapter->fgIsRadioOff) {
		DBGLOG(OID, WARN,
		       "Return from BSSID list scan! (radio off). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_SUCCESS;
	}
	DBGLOG(OID, TRACE, "ScanEX\n");
	if (pvSetBuffer != NULL && u4SetBufferLen != 0) {
		prScanRequest = (struct PARAM_SCAN_REQUEST_EXT *)
				pvSetBuffer;
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
		if ((prAdapter->fgEnOnlineScan == TRUE)
		    && (prAdapter->ucRddStatus)) {
			if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) !=
			    PARAM_MEDIA_STATE_CONNECTED)
				aisFsmScanRequest(prAdapter, prSsid, pucIe,
						  u4IeLength);
			else
				return WLAN_STATUS_FAILURE;
		} else
			return WLAN_STATUS_FAILURE;
	} else
#endif
	{
		if (prAdapter->fgEnOnlineScan == TRUE)
			aisFsmScanRequest(prAdapter, prSsid, pucIe, u4IeLength);
		else if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) !=
			 PARAM_MEDIA_STATE_CONNECTED)
			aisFsmScanRequest(prAdapter, prSsid, pucIe, u4IeLength);
		else
			return WLAN_STATUS_FAILURE;
	}

	return WLAN_STATUS_SUCCESS;
} /* wlanoidSetBssidListScanWithIE */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to request the driver to perform
 *        scanning with attaching information elements(IEs) specified from user
 *        space and multiple SSID
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
uint32_t
wlanoidSetBssidListScanAdv(IN struct ADAPTER *prAdapter,
			   IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			   OUT uint32_t *pu4SetInfoLen)
{
	struct PARAM_SCAN_REQUEST_ADV *prScanRequest;

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

	if (u4SetBufferLen != sizeof(struct PARAM_SCAN_REQUEST_ADV))
		return WLAN_STATUS_INVALID_LENGTH;
	else if (pvSetBuffer == NULL)
		return WLAN_STATUS_INVALID_DATA;

	if (prAdapter->fgIsRadioOff) {
		DBGLOG(OID, WARN,
		       "Return from BSSID list scan! (radio off). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_SUCCESS;
	}

	prScanRequest = (struct PARAM_SCAN_REQUEST_ADV *)
			pvSetBuffer;

#if CFG_SUPPORT_RDD_TEST_MODE
	if (prAdapter->prGlueInfo->rRegInfo.u4RddTestMode) {
		if ((prAdapter->fgEnOnlineScan == TRUE)
		    && (prAdapter->ucRddStatus)) {
			if (kalGetMediaStateIndicated(prAdapter->prGlueInfo)
					!= PARAM_MEDIA_STATE_CONNECTED) {
				aisFsmScanRequestAdv(prAdapter, prScanRequest);
			} else
				return WLAN_STATUS_FAILURE;
		} else
			return WLAN_STATUS_FAILURE;
	} else
#endif
	{
		if (prAdapter->fgEnOnlineScan == TRUE) {
			aisFsmScanRequestAdv(prAdapter, prScanRequest);
		} else if (kalGetMediaStateIndicated(prAdapter->prGlueInfo)
				!= PARAM_MEDIA_STATE_CONNECTED) {
			aisFsmScanRequestAdv(prAdapter, prScanRequest);
		} else
			return WLAN_STATUS_FAILURE;
	}
	cnmTimerStartTimer(prAdapter,
			   &prAdapter->rWifiVar.rAisFsmInfo.rScanDoneTimer,
			   SEC_TO_MSEC(AIS_SCN_DONE_TIMEOUT_SEC));
	return WLAN_STATUS_SUCCESS;
} /* wlanoidSetBssidListScanAdv */

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
uint32_t
wlanoidSetBssid(IN struct ADAPTER *prAdapter,
		IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		OUT uint32_t *pu4SetInfoLen)
{
	struct GLUE_INFO *prGlueInfo;
	uint8_t *pAddr;
	uint32_t i;
	int32_t i4Idx = -1;
	struct MSG_AIS_ABORT *prAisAbortMsg;
	uint8_t ucReasonOfDisconnect;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = MAC_ADDR_LEN;
	if (u4SetBufferLen != MAC_ADDR_LEN) {
		*pu4SetInfoLen = MAC_ADDR_LEN;
		return WLAN_STATUS_INVALID_LENGTH;
	} else if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in set ssid! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	prGlueInfo = prAdapter->prGlueInfo;
	pAddr = (uint8_t *) pvSetBuffer;

	/* re-association check */
	if (kalGetMediaStateIndicated(prGlueInfo) ==
	    PARAM_MEDIA_STATE_CONNECTED) {
		if (EQUAL_MAC_ADDR(
		    prAdapter->rWlanInfo.rCurrBssId.arMacAddress, pAddr)) {
			kalSetMediaStateIndicated(prGlueInfo,
					PARAM_MEDIA_STATE_TO_BE_INDICATED);
			ucReasonOfDisconnect =
					DISCONNECT_REASON_CODE_REASSOCIATION;
		} else {
			kalIndicateStatusAndComplete(prGlueInfo,
					WLAN_STATUS_MEDIA_DISCONNECT, NULL, 0);
			ucReasonOfDisconnect =
					DISCONNECT_REASON_CODE_NEW_CONNECTION;
		}
	} else {
		ucReasonOfDisconnect =
			DISCONNECT_REASON_CODE_NEW_CONNECTION;
	}

	/* check if any scanned result matchs with the BSSID */
	for (i = 0; i < prAdapter->rWlanInfo.u4ScanResultNum; i++) {
		if (EQUAL_MAC_ADDR(
		    prAdapter->rWlanInfo.arScanResult[i].arMacAddress, pAddr)) {
			i4Idx = (int32_t) i;
			break;
		}
	}

	/* prepare message to AIS */
	if (prAdapter->rWifiVar.rConnSettings.eOPMode ==
	    NET_TYPE_IBSS
	    || prAdapter->rWifiVar.rConnSettings.eOPMode ==
	    NET_TYPE_DEDICATED_IBSS) {
		/* IBSS *//* beacon period */
		prAdapter->rWifiVar.rConnSettings.u2BeaconPeriod =
			prAdapter->rWlanInfo.u2BeaconPeriod;
		prAdapter->rWifiVar.rConnSettings.u2AtimWindow =
			prAdapter->rWlanInfo.u2AtimWindow;
	}

	/* Set Connection Request Issued Flag */
	prAdapter->rWifiVar.rConnSettings.fgIsConnReqIssued = TRUE;
	prAdapter->rWifiVar.rConnSettings.eConnectionPolicy =
		CONNECT_BY_BSSID;

	/* Send AIS Abort Message */
	prAisAbortMsg = (struct MSG_AIS_ABORT *) cnmMemAlloc(
			prAdapter, RAM_TYPE_MSG, sizeof(struct MSG_AIS_ABORT));
	if (!prAisAbortMsg) {
		DBGLOG(REQ, ERROR, "Fail in allocating AisAbortMsg.\n");
		return WLAN_STATUS_FAILURE;
	}

	prAisAbortMsg->rMsgHdr.eMsgId = MID_OID_AIS_FSM_JOIN_REQ;
	prAisAbortMsg->ucReasonOfDisconnect = ucReasonOfDisconnect;

	/* Update the information to CONNECTION_SETTINGS_T */
	prAdapter->rWifiVar.rConnSettings.ucSSIDLen = 0;
	prAdapter->rWifiVar.rConnSettings.aucSSID[0] = '\0';
	COPY_MAC_ADDR(prAdapter->rWifiVar.rConnSettings.aucBSSID,
		      pAddr);

	if (EQUAL_MAC_ADDR(
		    prAdapter->rWlanInfo.rCurrBssId.arMacAddress, pAddr))
		prAisAbortMsg->fgDelayIndication = TRUE;
	else
		prAisAbortMsg->fgDelayIndication = FALSE;

#if CFG_DISCONN_DEBUG_FEATURE
	/* used to disconnect debug capability */
	g_rDisconnInfoTemp.ucTrigger = DISCONNECT_TRIGGER_ACTIVE;
#endif

	mboxSendMsg(prAdapter, MBOX_ID_0,
		    (struct MSG_HDR *) prAisAbortMsg, MSG_SEND_METHOD_BUF);

	return WLAN_STATUS_SUCCESS;
} /* end of wlanoidSetBssid() */

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
uint32_t
wlanoidSetSsid(IN struct ADAPTER *prAdapter,
	       IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
	       OUT uint32_t *pu4SetInfoLen)
{
	struct GLUE_INFO *prGlueInfo;
	struct PARAM_SSID *pParamSsid;
	uint32_t i;
	int32_t i4Idx = -1, i4MaxRSSI = INT_MIN;
	struct MSG_AIS_ABORT *prAisAbortMsg;
	u_int8_t fgIsValidSsid = TRUE;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	/* MSDN:
	 * Powering on the radio if the radio is powered off through a setting
	 * of OID_802_11_DISASSOCIATE
	 */
	if (prAdapter->fgIsRadioOff == TRUE)
		prAdapter->fgIsRadioOff = FALSE;

	if (u4SetBufferLen < sizeof(struct PARAM_SSID)
	    || u4SetBufferLen > sizeof(struct PARAM_SSID))
		return WLAN_STATUS_INVALID_LENGTH;
	else if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in set ssid! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	pParamSsid = (struct PARAM_SSID *) pvSetBuffer;

	if (pParamSsid->u4SsidLen > 32)
		return WLAN_STATUS_INVALID_LENGTH;

	prGlueInfo = prAdapter->prGlueInfo;

	/* prepare for CMD_BUILD_CONNECTION & CMD_GET_CONNECTION_STATUS */
	/* re-association check */
	if (kalGetMediaStateIndicated(prGlueInfo) ==
	    PARAM_MEDIA_STATE_CONNECTED) {
		if (EQUAL_SSID(
			    prAdapter->rWlanInfo.rCurrBssId.rSsid.aucSsid,
			    prAdapter->rWlanInfo.rCurrBssId.rSsid.u4SsidLen,
			    pParamSsid->aucSsid, pParamSsid->u4SsidLen)) {
			kalSetMediaStateIndicated(prGlueInfo,
					PARAM_MEDIA_STATE_TO_BE_INDICATED);
		} else
			kalIndicateStatusAndComplete(prGlueInfo,
					WLAN_STATUS_MEDIA_DISCONNECT, NULL, 0);
	}
	/* check if any scanned result matchs with the SSID */
	for (i = 0; i < prAdapter->rWlanInfo.u4ScanResultNum; i++) {
		uint8_t *aucSsid =
			prAdapter->rWlanInfo.arScanResult[i].rSsid.aucSsid;
		uint8_t ucSsidLength = (uint8_t)
			prAdapter->rWlanInfo.arScanResult[i].rSsid.u4SsidLen;
		int32_t i4RSSI = prAdapter->rWlanInfo.arScanResult[i].rRssi;

		if (EQUAL_SSID(aucSsid, ucSsidLength, pParamSsid->aucSsid,
			       pParamSsid->u4SsidLen) &&
		    i4RSSI >= i4MaxRSSI) {
			i4Idx = (int32_t) i;
			i4MaxRSSI = i4RSSI;
		}
	}

	/* prepare message to AIS */
	if (prAdapter->rWifiVar.rConnSettings.eOPMode ==
	    NET_TYPE_IBSS
	    || prAdapter->rWifiVar.rConnSettings.eOPMode ==
	    NET_TYPE_DEDICATED_IBSS) {
		/* IBSS *//* beacon period */
		prAdapter->rWifiVar.rConnSettings.u2BeaconPeriod =
			prAdapter->rWlanInfo.u2BeaconPeriod;
		prAdapter->rWifiVar.rConnSettings.u2AtimWindow =
			prAdapter->rWlanInfo.u2AtimWindow;
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
			prAdapter->rWifiVar.rConnSettings.eConnectionPolicy =
				CONNECT_BY_SSID_BEST_RSSI;
		else
			/* wildcard SSID */
			prAdapter->rWifiVar.rConnSettings.eConnectionPolicy =
				CONNECT_BY_SSID_ANY;
	} else
		prAdapter->rWifiVar.rConnSettings.fgIsConnReqIssued = FALSE;

	/* Send AIS Abort Message */
	prAisAbortMsg = (struct MSG_AIS_ABORT *) cnmMemAlloc(
			prAdapter, RAM_TYPE_MSG, sizeof(struct MSG_AIS_ABORT));
	if (!prAisAbortMsg) {
		DBGLOG(REQ, ERROR, "Fail in allocating AisAbortMsg.\n");
		return WLAN_STATUS_FAILURE;
	}

	prAisAbortMsg->rMsgHdr.eMsgId = MID_OID_AIS_FSM_JOIN_REQ;
	prAisAbortMsg->ucReasonOfDisconnect =
		DISCONNECT_REASON_CODE_NEW_CONNECTION;
	COPY_SSID(prAdapter->rWifiVar.rConnSettings.aucSSID,
		  prAdapter->rWifiVar.rConnSettings.ucSSIDLen,
		  pParamSsid->aucSsid, (uint8_t) pParamSsid->u4SsidLen);

	if (EQUAL_SSID(
		    prAdapter->rWlanInfo.rCurrBssId.rSsid.aucSsid,
		    prAdapter->rWlanInfo.rCurrBssId.rSsid.u4SsidLen,
		    pParamSsid->aucSsid, pParamSsid->u4SsidLen)) {
		prAisAbortMsg->fgDelayIndication = TRUE;
	} else {
		/* Update the information to CONNECTION_SETTINGS_T */
		prAisAbortMsg->fgDelayIndication = FALSE;
	}
	DBGLOG(SCN, INFO, "SSID %s\n",
	       prAdapter->rWifiVar.rConnSettings.aucSSID);

#if CFG_DISCONN_DEBUG_FEATURE
	/* used to disconnect debug capability */
	g_rDisconnInfoTemp.ucTrigger = DISCONNECT_TRIGGER_ACTIVE;
#endif

	mboxSendMsg(prAdapter, MBOX_ID_0,
		    (struct MSG_HDR *) prAisAbortMsg, MSG_SEND_METHOD_BUF);

	return WLAN_STATUS_SUCCESS;

} /* end of wlanoidSetSsid() */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine will initiate the join procedure to attempt
 *        to associate with the new BSS, base on given SSID, BSSID, and
 *        freqency.
 *	  If the target connecting BSS is in the same ESS as current connected
 *        BSS, roaming will be performed.
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
uint32_t
wlanoidSetConnect(IN struct ADAPTER *prAdapter,
		  IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		  OUT uint32_t *pu4SetInfoLen)
{
	struct GLUE_INFO *prGlueInfo;
	struct PARAM_CONNECT *pParamConn;
	struct CONNECTION_SETTINGS *prConnSettings;
	uint32_t i;
	struct MSG_AIS_ABORT *prAisAbortMsg;
	u_int8_t fgIsValidSsid = TRUE;
	u_int8_t fgEqualSsid = FALSE;
	u_int8_t fgEqualBssid = FALSE;
	const uint8_t aucZeroMacAddr[] = NULL_MAC_ADDR;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	/* MSDN:
	 * Powering on the radio if the radio is powered off through a setting
	 * of OID_802_11_DISASSOCIATE
	 */
	if (prAdapter->fgIsRadioOff == TRUE)
		prAdapter->fgIsRadioOff = FALSE;

	if (u4SetBufferLen != sizeof(struct PARAM_CONNECT))
		return WLAN_STATUS_INVALID_LENGTH;
	else if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in set ssid! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}
	prAisAbortMsg = (struct MSG_AIS_ABORT *) cnmMemAlloc(
			prAdapter, RAM_TYPE_MSG, sizeof(struct MSG_AIS_ABORT));
	if (!prAisAbortMsg) {
		DBGLOG(REQ, ERROR, "Fail in allocating AisAbortMsg.\n");
		return WLAN_STATUS_FAILURE;
	}
	prAisAbortMsg->rMsgHdr.eMsgId = MID_OID_AIS_FSM_JOIN_REQ;

	pParamConn = (struct PARAM_CONNECT *) pvSetBuffer;
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
	kalMemZero(prConnSettings->aucSSID,
		   sizeof(prConnSettings->aucSSID));
	prConnSettings->ucSSIDLen = 0;
	kalMemZero(prConnSettings->aucBSSID,
		   sizeof(prConnSettings->aucBSSID));
	kalMemZero(prConnSettings->aucBSSIDHint,
			sizeof(prConnSettings->aucBSSIDHint));
	prConnSettings->eConnectionPolicy = CONNECT_BY_SSID_ANY;
	prConnSettings->fgIsConnByBssidIssued = FALSE;

	if (pParamConn->pucSsid) {
		prConnSettings->eConnectionPolicy =
			CONNECT_BY_SSID_BEST_RSSI;
		prConnSettings->ucSSIDLen = pParamConn->u4SsidLen;
		COPY_SSID(prConnSettings->aucSSID,
			  prConnSettings->ucSSIDLen, pParamConn->pucSsid,
			  (uint8_t) pParamConn->u4SsidLen);
		if (EQUAL_SSID(
			    prAdapter->rWlanInfo.rCurrBssId.rSsid.aucSsid,
			    prAdapter->rWlanInfo.rCurrBssId.rSsid.u4SsidLen,
			    pParamConn->pucSsid, pParamConn->u4SsidLen))
			fgEqualSsid = TRUE;
	}
	if (pParamConn->pucBssid) {
		if (!EQUAL_MAC_ADDR(aucZeroMacAddr, pParamConn->pucBssid)
		    && IS_UCAST_MAC_ADDR(pParamConn->pucBssid)) {
			prConnSettings->eConnectionPolicy = CONNECT_BY_BSSID;
			prConnSettings->fgIsConnByBssidIssued = TRUE;
			COPY_MAC_ADDR(prConnSettings->aucBSSID,
				      pParamConn->pucBssid);
			if (EQUAL_MAC_ADDR(
			    prAdapter->rWlanInfo.rCurrBssId.arMacAddress,
			    pParamConn->pucBssid))
				fgEqualBssid = TRUE;
		} else
			DBGLOG(INIT, INFO, "wrong bssid " MACSTR "to connect\n",
			       MAC2STR(pParamConn->pucBssid));
	} else if (pParamConn->pucBssidHint) {
		if (!EQUAL_MAC_ADDR(aucZeroMacAddr, pParamConn->pucBssidHint)
			&& IS_UCAST_MAC_ADDR(pParamConn->pucBssidHint)) {
			prConnSettings->eConnectionPolicy =
				CONNECT_BY_BSSID_HINT;
			COPY_MAC_ADDR(prConnSettings->aucBSSIDHint,
				pParamConn->pucBssidHint);
			if (EQUAL_MAC_ADDR(
				prAdapter->rWlanInfo.rCurrBssId.arMacAddress,
				pParamConn->pucBssidHint))
				fgEqualBssid = TRUE;
		}
	} else
		DBGLOG(INIT, INFO, "No Bssid set\n");
	prConnSettings->u4FreqInKHz = pParamConn->u4CenterFreq;

	/* prepare for CMD_BUILD_CONNECTION & CMD_GET_CONNECTION_STATUS */
	/* re-association check */
	if (kalGetMediaStateIndicated(prGlueInfo) ==
	    PARAM_MEDIA_STATE_CONNECTED) {
		if (fgEqualSsid) {
			prAisAbortMsg->ucReasonOfDisconnect =
				DISCONNECT_REASON_CODE_ROAMING;
			if (fgEqualBssid) {
				kalSetMediaStateIndicated(prGlueInfo,
					PARAM_MEDIA_STATE_TO_BE_INDICATED);
				prAisAbortMsg->ucReasonOfDisconnect =
					DISCONNECT_REASON_CODE_REASSOCIATION;
			}
		} else {
			DBGLOG(INIT, INFO, "DisBySsid\n");
			kalIndicateStatusAndComplete(prGlueInfo,
					WLAN_STATUS_MEDIA_DISCONNECT, NULL, 0);
			prAisAbortMsg->ucReasonOfDisconnect =
					DISCONNECT_REASON_CODE_NEW_CONNECTION;
		}
	} else
		prAisAbortMsg->ucReasonOfDisconnect =
			DISCONNECT_REASON_CODE_NEW_CONNECTION;
#if 0
	/* check if any scanned result matchs with the SSID */
	for (i = 0; i < prAdapter->rWlanInfo.u4ScanResultNum; i++) {
		uint8_t *aucSsid =
			prAdapter->rWlanInfo.arScanResult[i].rSsid.aucSsid;
		uint8_t ucSsidLength = (uint8_t)
			prAdapter->rWlanInfo.arScanResult[i].rSsid.u4SsidLen;
		int32_t i4RSSI = prAdapter->rWlanInfo.arScanResult[i].rRssi;

		if (EQUAL_SSID(aucSsid, ucSsidLength, pParamConn->pucSsid,
			       pParamConn->u4SsidLen) &&
		    i4RSSI >= i4MaxRSSI) {
			i4Idx = (int32_t) i;
			i4MaxRSSI = i4RSSI;
		}
		if (EQUAL_MAC_ADDR(
		    prAdapter->rWlanInfo.arScanResult[i].arMacAddress, pAddr)) {
			i4Idx = (int32_t) i;
			break;
		}
	}
#endif
	/* prepare message to AIS */
	if (prConnSettings->eOPMode == NET_TYPE_IBSS
	    || prConnSettings->eOPMode == NET_TYPE_DEDICATED_IBSS) {
		/* IBSS *//* beacon period */
		prConnSettings->u2BeaconPeriod =
			prAdapter->rWlanInfo.u2BeaconPeriod;
		prConnSettings->u2AtimWindow =
			prAdapter->rWlanInfo.u2AtimWindow;
	}

	if (prAdapter->rWifiVar.fgSupportWZCDisassociation) {
		if (pParamConn->u4SsidLen == ELEM_MAX_LEN_SSID) {
			fgIsValidSsid = FALSE;

			if (pParamConn->pucSsid) {
				for (i = 0; i < ELEM_MAX_LEN_SSID; i++) {
					if (!((pParamConn->pucSsid[i] > 0) &&
					    (pParamConn->pucSsid[i] <= 0x1F))) {
						fgIsValidSsid = TRUE;
						break;
					}
				}
			} else {
				DBGLOG(INIT, ERROR,
				       "pParamConn->pucSsid is NULL\n");
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

#if CFG_DISCONN_DEBUG_FEATURE
	/* used to disconnect debug capability */
	g_rDisconnInfoTemp.ucTrigger = DISCONNECT_TRIGGER_ACTIVE;
#endif

	mboxSendMsg(prAdapter, MBOX_ID_0,
		    (struct MSG_HDR *) prAisAbortMsg, MSG_SEND_METHOD_BUF);

	DBGLOG(INIT, INFO, "ssid %s, bssid " MACSTR
		", bssid_hint " MACSTR ", conn policy %d, disc reason %d\n",
		prConnSettings->aucSSID, MAC2STR(prConnSettings->aucBSSID),
		MAC2STR(prConnSettings->aucBSSIDHint),
		prConnSettings->eConnectionPolicy,
		prAisAbortMsg->ucReasonOfDisconnect);
	return WLAN_STATUS_SUCCESS;
} /* end of wlanoidSetConnect */

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
uint32_t
wlanoidQuerySsid(IN struct ADAPTER *prAdapter,
		 OUT void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
		 OUT uint32_t *pu4QueryInfoLen)
{
	struct PARAM_SSID *prAssociatedSsid;

	DEBUGFUNC("wlanoidQuerySsid");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);

	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(struct PARAM_SSID);

	/* Check for query buffer length */
	if (u4QueryBufferLen < *pu4QueryInfoLen) {
		DBGLOG(REQ, WARN, "Invalid length %u\n", u4QueryBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	prAssociatedSsid = (struct PARAM_SSID *) pvQueryBuffer;

	kalMemZero(prAssociatedSsid->aucSsid,
		   sizeof(prAssociatedSsid->aucSsid));

	if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) ==
	    PARAM_MEDIA_STATE_CONNECTED) {
		prAssociatedSsid->u4SsidLen =
			prAdapter->rWlanInfo.rCurrBssId.rSsid.u4SsidLen;

		if (prAssociatedSsid->u4SsidLen) {
			kalMemCopy(prAssociatedSsid->aucSsid,
				prAdapter->rWlanInfo.rCurrBssId.rSsid.aucSsid,
				prAssociatedSsid->u4SsidLen);
		}
	} else {
		prAssociatedSsid->u4SsidLen = 0;

		DBGLOG(REQ, TRACE, "Null SSID\n");
	}

	return WLAN_STATUS_SUCCESS;
} /* wlanoidQuerySsid */

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
uint32_t
wlanoidQueryInfrastructureMode(IN struct ADAPTER *prAdapter,
			       OUT void *pvQueryBuffer,
			       IN uint32_t u4QueryBufferLen,
			       OUT uint32_t *pu4QueryInfoLen)
{
	DEBUGFUNC("wlanoidQueryInfrastructureMode");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);

	*pu4QueryInfoLen = sizeof(enum ENUM_PARAM_OP_MODE);

	if (u4QueryBufferLen < sizeof(enum ENUM_PARAM_OP_MODE))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*(enum ENUM_PARAM_OP_MODE *) pvQueryBuffer =
		prAdapter->rWifiVar.rConnSettings.eOPMode;

	/*
	 ** According to OID_802_11_INFRASTRUCTURE_MODE
	 ** If there is no prior OID_802_11_INFRASTRUCTURE_MODE,
	 ** NDIS_STATUS_ADAPTER_NOT_READY shall be returned.
	 */
#if DBG
	switch (*(enum ENUM_PARAM_OP_MODE *) pvQueryBuffer) {
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
} /* wlanoidQueryInfrastructureMode */

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
uint32_t
wlanoidSetInfrastructureMode(IN struct ADAPTER *prAdapter,
			     IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			     OUT uint32_t *pu4SetInfoLen)
{
	struct GLUE_INFO *prGlueInfo;
	enum ENUM_PARAM_OP_MODE eOpMode;
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

	if (u4SetBufferLen < sizeof(enum ENUM_PARAM_OP_MODE))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	*pu4SetInfoLen = sizeof(enum ENUM_PARAM_OP_MODE);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in set infrastructure mode! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	eOpMode = *(enum ENUM_PARAM_OP_MODE *) pvSetBuffer;
	/* Verify the new infrastructure mode. */
	if (eOpMode >= NET_TYPE_NUM) {
		DBGLOG(REQ, TRACE, "Invalid mode value %d\n", eOpMode);
		return WLAN_STATUS_INVALID_DATA;
	}

	/* check if possible to switch to AdHoc mode */
	if (eOpMode == NET_TYPE_IBSS
	    || eOpMode == NET_TYPE_DEDICATED_IBSS) {
		if (cnmAisIbssIsPermitted(prAdapter) == FALSE) {
			DBGLOG(REQ, TRACE, "Mode value %d unallowed\n",
			       eOpMode);
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
	prAdapter->rWifiVar.rAisSpecificBssInfo.fgMgmtProtection =
		FALSE;
	prAdapter->rWifiVar.rAisSpecificBssInfo.fgBipKeyInstalled =
		FALSE;
#endif

#if CFG_SUPPORT_WPS2
	kalMemZero(&prAdapter->prGlueInfo->aucWSCAssocInfoIE, 200);
	prAdapter->prGlueInfo->u2WSCAssocInfoIELen = 0;
#endif

#if 0 /* STA record remove at AIS_ABORT nicUpdateBss and DISCONNECT */
	for (i = 0; i < prAdapter->ucHwBssIdNum; i++) {
		prBssInfo = prAdapter->aprBssInfo[i];
		if (prBssInfo->eNetworkType == NETWORK_TYPE_AIS)
			cnmStaFreeAllStaByNetwork(prAdapter,
						  prBssInfo->ucBssIndex, 0);
	}
#endif

	/* Clean up the Tx key flag */
	if (prAdapter->prAisBssInfo != NULL) {
		prAdapter->prAisBssInfo->fgBcDefaultKeyExist = FALSE;
		prAdapter->prAisBssInfo->ucBcDefaultKeyIdx = 0xFF;
	}

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
				   nicCmdEventSetCommon, nicOidCmdTimeoutCommon,
				   0, NULL, pvSetBuffer, u4SetBufferLen);
} /* wlanoidSetInfrastructureMode */

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
uint32_t
wlanoidQueryAuthMode(IN struct ADAPTER *prAdapter,
		     OUT void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
		     OUT uint32_t *pu4QueryInfoLen)
{
	DEBUGFUNC("wlanoidQueryAuthMode");

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	*pu4QueryInfoLen = sizeof(enum ENUM_PARAM_AUTH_MODE);

	if (u4QueryBufferLen < sizeof(enum ENUM_PARAM_AUTH_MODE))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	*(enum ENUM_PARAM_AUTH_MODE *) pvQueryBuffer =
		prAdapter->rWifiVar.rConnSettings.eAuthMode;

#if DBG
	switch (*(enum ENUM_PARAM_AUTH_MODE *) pvQueryBuffer) {
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
		DBGLOG(REQ, INFO, "Current auth mode: %d\n",
		       *(enum ENUM_PARAM_AUTH_MODE *) pvQueryBuffer);
		break;
	}
#endif
	return WLAN_STATUS_SUCCESS;
} /* wlanoidQueryAuthMode */

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
uint32_t
wlanoidSetAuthMode(IN struct ADAPTER *prAdapter,
		   IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		   OUT uint32_t *pu4SetInfoLen)
{
	struct GLUE_INFO *prGlueInfo;
	/* UINT_32       i, u4AkmSuite; */
	/* P_DOT11_RSNA_CONFIG_AUTHENTICATION_SUITES_ENTRY prEntry; */

	DEBUGFUNC("wlanoidSetAuthMode");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	prGlueInfo = prAdapter->prGlueInfo;

	*pu4SetInfoLen = sizeof(enum ENUM_PARAM_AUTH_MODE);

	if (u4SetBufferLen < sizeof(enum ENUM_PARAM_AUTH_MODE))
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
	if (*(enum ENUM_PARAM_AUTH_MODE *) pvSetBuffer >=
	    AUTH_MODE_NUM) {
		DBGLOG(REQ, TRACE, "Invalid auth mode %d\n",
		       *(enum ENUM_PARAM_AUTH_MODE *) pvSetBuffer);
		return WLAN_STATUS_INVALID_DATA;
	}

	switch (*(enum ENUM_PARAM_AUTH_MODE *) pvSetBuffer) {
	case AUTH_MODE_WPA:
	case AUTH_MODE_WPA_PSK:
	case AUTH_MODE_WPA2:
	case AUTH_MODE_WPA2_PSK:
	case AUTH_MODE_WPA2_FT:
	case AUTH_MODE_WPA2_FT_PSK:
		/* infrastructure mode only */
		if (prAdapter->rWifiVar.rConnSettings.eOPMode !=
		    NET_TYPE_INFRA)
			return WLAN_STATUS_NOT_ACCEPTED;
		break;

	case AUTH_MODE_WPA_NONE:
		/* ad hoc mode only */
		if (prAdapter->rWifiVar.rConnSettings.eOPMode !=
		    NET_TYPE_IBSS)
			return WLAN_STATUS_NOT_ACCEPTED;
		break;

	default:
		break;
	}

	/* Save the new authentication mode. */
	prAdapter->rWifiVar.rConnSettings.eAuthMode = *
			(enum ENUM_PARAM_AUTH_MODE *) pvSetBuffer;

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
		DBGLOG(RSN, TRACE, "New auth mode: unknown (%d)\n",
		       prAdapter->rWifiVar.rConnSettings.eAuthMode);
	}
#endif

#if 0
	if (prAdapter->rWifiVar.rConnSettings.eAuthMode >=
	    AUTH_MODE_WPA) {
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
		prEntry = &prAdapter->rMib
				.dot11RSNAConfigAuthenticationSuitesTable[i];

		if (prEntry->dot11RSNAConfigAuthenticationSuite ==
		    u4AkmSuite)
			prEntry->dot11RSNAConfigAuthenticationSuiteEnabled =
									TRUE;
		else
			prEntry->dot11RSNAConfigAuthenticationSuiteEnabled =
									FALSE;
#if CFG_SUPPORT_802_11W
		if (kalGetMfpSetting(prAdapter->prGlueInfo) !=
		    RSN_AUTH_MFP_DISABLED) {
			if ((u4AkmSuite == RSN_AKM_SUITE_PSK) &&
			    prEntry->dot11RSNAConfigAuthenticationSuite ==
			    RSN_AKM_SUITE_PSK_SHA256) {
				DBGLOG(RSN, TRACE,
				       "Enable RSN_AKM_SUITE_PSK_SHA256 AKM support\n");
				prEntry->
				dot11RSNAConfigAuthenticationSuiteEnabled =
									TRUE;

			}
			if ((u4AkmSuite == RSN_AKM_SUITE_802_1X) &&
			    prEntry->dot11RSNAConfigAuthenticationSuite ==
			    RSN_AKM_SUITE_802_1X_SHA256) {
				DBGLOG(RSN, TRACE,
				       "Enable RSN_AKM_SUITE_802_1X_SHA256 AKM support\n");
				prEntry->
				dot11RSNAConfigAuthenticationSuiteEnabled =
									TRUE;
			}
		}
#endif
	}
#endif

	return WLAN_STATUS_SUCCESS;

} /* wlanoidSetAuthMode */

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
uint32_t
wlanoidQueryPrivacyFilter(IN struct ADAPTER *prAdapter,
			  OUT void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
			  OUT uint32_t *pu4QueryInfoLen)
{
	DEBUGFUNC("wlanoidQueryPrivacyFilter");

	ASSERT(prAdapter);

	ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	*pu4QueryInfoLen = sizeof(enum ENUM_PARAM_PRIVACY_FILTER);

	if (u4QueryBufferLen < sizeof(enum
				      ENUM_PARAM_PRIVACY_FILTER))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	*(enum ENUM_PARAM_PRIVACY_FILTER *) pvQueryBuffer =
		prAdapter->rWlanInfo.ePrivacyFilter;

#if DBG
	switch (*(enum ENUM_PARAM_PRIVACY_FILTER *) pvQueryBuffer) {
	case PRIVACY_FILTER_ACCEPT_ALL:
		DBGLOG(REQ, INFO, "Current privacy mode: open mode\n");
		break;

	case PRIVACY_FILTER_8021xWEP:
		DBGLOG(REQ, INFO, "Current privacy mode: filtering mode\n");
		break;

	default:
		DBGLOG(REQ, INFO, "Current auth mode: %d\n",
		       *(enum ENUM_PARAM_AUTH_MODE *) pvQueryBuffer);
	}
#endif
	return WLAN_STATUS_SUCCESS;
} /* wlanoidQueryPrivacyFilter */

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
uint32_t
wlanoidSetPrivacyFilter(IN struct ADAPTER *prAdapter,
			IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			OUT uint32_t *pu4SetInfoLen)
{
	struct GLUE_INFO *prGlueInfo;

	DEBUGFUNC("wlanoidSetPrivacyFilter");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	prGlueInfo = prAdapter->prGlueInfo;

	*pu4SetInfoLen = sizeof(enum ENUM_PARAM_PRIVACY_FILTER);

	if (u4SetBufferLen < sizeof(enum ENUM_PARAM_PRIVACY_FILTER))
		return WLAN_STATUS_INVALID_LENGTH;

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in set Authentication mode! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	/* Check if the new authentication mode is valid. */
	if (*(enum ENUM_PARAM_PRIVACY_FILTER *) pvSetBuffer >=
	    PRIVACY_FILTER_NUM) {
		DBGLOG(REQ, TRACE, "Invalid privacy filter %d\n",
		       *(enum ENUM_PARAM_PRIVACY_FILTER *) pvSetBuffer);
		return WLAN_STATUS_INVALID_DATA;
	}

	switch (*(enum ENUM_PARAM_PRIVACY_FILTER *) pvSetBuffer) {
	default:
		break;
	}

	/* Save the new authentication mode. */
	prAdapter->rWlanInfo.ePrivacyFilter =
				*(enum ENUM_PARAM_PRIVACY_FILTER) pvSetBuffer;

	return WLAN_STATUS_SUCCESS;

} /* wlanoidSetPrivacyFilter */
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
uint32_t
wlanoidSetReloadDefaults(IN struct ADAPTER *prAdapter,
			 IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			 OUT uint32_t *pu4SetInfoLen)
{
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	enum ENUM_PARAM_NETWORK_TYPE eNetworkType;
	uint32_t u4Len;
	uint8_t ucCmdSeqNum;

	DEBUGFUNC("wlanoidSetReloadDefaults");

	ASSERT(prAdapter);

	ASSERT(pu4SetInfoLen);
	*pu4SetInfoLen = sizeof(enum ENUM_RELOAD_DEFAULTS);

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
	switch (*(enum ENUM_RELOAD_DEFAULTS *) pvSetBuffer) {
	case ENUM_RELOAD_WEP_KEYS:
		/* Reload available default WEP keys from the permanent
		 *  storage.
		 */
		prAdapter->rWifiVar.rConnSettings.eAuthMode =
			AUTH_MODE_OPEN;
		/* ENUM_ENCRYPTION_DISABLED; */
		prAdapter->rWifiVar.rConnSettings.eEncStatus =
			ENUM_ENCRYPTION1_KEY_ABSENT;
		{
			struct GLUE_INFO *prGlueInfo;
			struct CMD_INFO *prCmdInfo;
			struct WIFI_CMD *prWifiCmd;
			struct CMD_802_11_KEY *prCmdKey;
			uint8_t aucBCAddr[] = BC_MAC_ADDR;

			prGlueInfo = prAdapter->prGlueInfo;
			prCmdInfo = cmdBufAllocateCmdInfo(prAdapter,
				(CMD_HDR_SIZE + sizeof(struct CMD_802_11_KEY)));

			if (!prCmdInfo) {
				DBGLOG(INIT, ERROR,
					"Allocate CMD_INFO_T ==> FAILED.\n");
				return WLAN_STATUS_FAILURE;
			}
			/* increase command sequence number */
			ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

			/* compose CMD_802_11_KEY cmd pkt */
			prCmdInfo->eCmdType = COMMAND_TYPE_NETWORK_IOCTL;
			prCmdInfo->u2InfoBufLen = CMD_HDR_SIZE +
						sizeof(struct CMD_802_11_KEY);
			prCmdInfo->pfCmdDoneHandler = nicCmdEventSetCommon;
			prCmdInfo->pfCmdTimeoutHandler = nicOidCmdTimeoutCommon;
			prCmdInfo->fgIsOid = g_fgIsOid;
			prCmdInfo->ucCID = CMD_ID_ADD_REMOVE_KEY;
			prCmdInfo->fgSetQuery = TRUE;
			prCmdInfo->fgNeedResp = FALSE;
			prCmdInfo->ucCmdSeqNum = ucCmdSeqNum;
			prCmdInfo->u4SetInfoLen =
						sizeof(struct PARAM_REMOVE_KEY);
			prCmdInfo->pvInformationBuffer = pvSetBuffer;
			prCmdInfo->u4InformationBufferLength = u4SetBufferLen;

			/* Setup WIFI_CMD_T */
			prWifiCmd =
				(struct WIFI_CMD *) (prCmdInfo->pucInfoBuffer);
			prWifiCmd->u2TxByteCount = prCmdInfo->u2InfoBufLen;
			prWifiCmd->u2PQ_ID = CMD_PQ_ID;
			prWifiCmd->ucPktTypeID = CMD_PACKET_TYPE_ID;
			prWifiCmd->ucCID = prCmdInfo->ucCID;
			prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
			prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;

			prCmdKey =
				(struct CMD_802_11_KEY *)(prWifiCmd->aucBuffer);

			kalMemZero((uint8_t *) prCmdKey,
				   sizeof(struct CMD_802_11_KEY));

			prCmdKey->ucAddRemove = 0; /* Remove */
			prCmdKey->ucKeyId =
				0; /* (UINT_8)(prRemovedKey->u4KeyIndex &
				    * 0x000000ff);
				    */
			kalMemCopy(prCmdKey->aucPeerAddr, aucBCAddr,
				   MAC_ADDR_LEN);

			ASSERT(prCmdKey->ucKeyId < MAX_KEY_NUM);

			prCmdKey->ucKeyType = 0;

			/* insert into prCmdQueue */
			kalEnqueueCommand(prGlueInfo,
					  (struct QUE_ENTRY *) prCmdInfo);

			/* wakeup txServiceThread later */
			GLUE_SET_EVENT(prGlueInfo);

			return WLAN_STATUS_PENDING;
		}

		break;

	default:
		DBGLOG(REQ, TRACE, "Invalid reload option %d\n",
		       *(enum ENUM_RELOAD_DEFAULTS *) pvSetBuffer);
		rStatus = WLAN_STATUS_INVALID_DATA;
	}

	/* OID_802_11_RELOAD_DEFAULTS requiest to reset to auto mode */
	eNetworkType = PARAM_NETWORK_TYPE_AUTOMODE;
	wlanoidSetNetworkTypeInUse(prAdapter, &eNetworkType,
				   sizeof(eNetworkType), &u4Len);

	return rStatus;
} /* wlanoidSetReloadDefaults */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to set a WEP key to the driver.
 *
 * \param[in]  prAdapter Pointer to the Adapter structure.
 * \param[in]  pvSetBuffer A pointer to the buffer that holds the data to be
 *             set.
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
uint8_t keyBuffer[sizeof(struct PARAM_KEY) +
				16 /* LEGACY_KEY_MAX_LEN */];
uint8_t aucBCAddr[] = BC_MAC_ADDR;
#endif
uint32_t
wlanoidSetAddWep(IN struct ADAPTER *prAdapter,
		 IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		 OUT uint32_t *pu4SetInfoLen)
{
#ifndef LINUX
	uint8_t keyBuffer[sizeof(struct PARAM_KEY) +
					16 /* LEGACY_KEY_MAX_LEN */];
	uint8_t aucBCAddr[] = BC_MAC_ADDR;
#endif
	struct PARAM_WEP *prNewWepKey;
	struct PARAM_KEY *prParamKey = (struct PARAM_KEY *)
				       keyBuffer;
	uint32_t u4KeyId, u4SetLen;

	DEBUGFUNC("wlanoidSetAddWep");

	ASSERT(prAdapter);

	*pu4SetInfoLen = OFFSET_OF(struct PARAM_WEP,
				   aucKeyMaterial);

	if (u4SetBufferLen < OFFSET_OF(struct PARAM_WEP,
				       aucKeyMaterial)) {
		ASSERT(pu4SetInfoLen);
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}

	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in set add WEP! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	prNewWepKey = (struct PARAM_WEP *) pvSetBuffer;

	/* Verify the total buffer for minimum length. */
	if (u4SetBufferLen < OFFSET_OF(struct PARAM_WEP,
	    aucKeyMaterial) + prNewWepKey->u4KeyLength) {
		DBGLOG(REQ, WARN,
		       "Invalid total buffer length (%d) than minimum length (%d)\n",
		       (uint8_t) u4SetBufferLen,
		       (uint8_t) OFFSET_OF(struct PARAM_WEP, aucKeyMaterial));

		*pu4SetInfoLen = OFFSET_OF(struct PARAM_WEP,
					   aucKeyMaterial);
		return WLAN_STATUS_INVALID_DATA;
	}

	/* Verify the key structure length. */
	if (prNewWepKey->u4Length > u4SetBufferLen) {
		DBGLOG(REQ, WARN,
		       "Invalid key structure length (%d) greater than total buffer length (%d)\n",
		       (uint8_t) prNewWepKey->u4Length,
		       (uint8_t) u4SetBufferLen);

		*pu4SetInfoLen = u4SetBufferLen;
		return WLAN_STATUS_INVALID_DATA;
	}

	/* Verify the key material length for maximum key material length:16 */
	if (prNewWepKey->u4KeyLength >
	    16 /* LEGACY_KEY_MAX_LEN */) {
		DBGLOG(REQ, WARN,
		       "Invalid key material length (%d) greater than maximum key material length (16)\n",
		       (uint8_t) prNewWepKey->u4KeyLength);

		*pu4SetInfoLen = u4SetBufferLen;
		return WLAN_STATUS_INVALID_DATA;
	}

	*pu4SetInfoLen = u4SetBufferLen;

	u4KeyId = prNewWepKey->u4KeyIndex & BITS(0,
			29) /* WEP_KEY_ID_FIELD */;

	/* Verify whether key index is valid or not, current version
	 *  driver support only 4 global WEP keys setting by this OID
	 */
	if (u4KeyId > MAX_KEY_NUM - 1) {
		DBGLOG(REQ, ERROR, "Error, invalid WEP key ID: %d\n",
		       (uint8_t) u4KeyId);
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

	kalMemCopy(prParamKey->aucKeyMaterial,
		   prNewWepKey->aucKeyMaterial, prNewWepKey->u4KeyLength);

	prParamKey->ucBssIdx = prAdapter->prAisBssInfo->ucBssIndex;

	if (prParamKey->u4KeyLength == WEP_40_LEN)
		prParamKey->ucCipher = CIPHER_SUITE_WEP40;
	else if (prParamKey->u4KeyLength == WEP_104_LEN)
		prParamKey->ucCipher = CIPHER_SUITE_WEP104;
	else if (prParamKey->u4KeyLength == WEP_128_LEN)
		prParamKey->ucCipher = CIPHER_SUITE_WEP128;

	prParamKey->u4Length = OFFSET_OF(
		struct PARAM_KEY, aucKeyMaterial) + prNewWepKey->u4KeyLength;

	wlanoidSetAddKey(prAdapter, (void *) prParamKey,
			 prParamKey->u4Length, &u4SetLen);

	return WLAN_STATUS_PENDING;
} /* wlanoidSetAddWep */

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
uint32_t
wlanoidSetRemoveWep(IN struct ADAPTER *prAdapter,
		    IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		    OUT uint32_t *pu4SetInfoLen)
{
	uint32_t u4KeyId, u4SetLen;
	struct PARAM_REMOVE_KEY rRemoveKey;
	uint8_t aucBCAddr[] = BC_MAC_ADDR;

	DEBUGFUNC("wlanoidSetRemoveWep");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(uint32_t);

	if (u4SetBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);
	u4KeyId = *(uint32_t *) pvSetBuffer;

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

	rRemoveKey.u4Length = sizeof(struct PARAM_REMOVE_KEY);
	rRemoveKey.u4KeyIndex = *(uint32_t *) pvSetBuffer;

	kalMemCopy(rRemoveKey.arBSSID, aucBCAddr, MAC_ADDR_LEN);

	wlanoidSetRemoveKey(prAdapter, (void *)&rRemoveKey,
			    sizeof(struct PARAM_REMOVE_KEY), &u4SetLen);

	return WLAN_STATUS_PENDING;
} /* wlanoidSetRemoveWep */

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
uint32_t
wlanoidSetAddKey(IN struct ADAPTER *prAdapter, IN void *pvSetBuffer,
		 IN uint32_t u4SetBufferLen, OUT uint32_t *pu4SetInfoLen)
{
	struct GLUE_INFO *prGlueInfo;
	struct CMD_INFO *prCmdInfo;
	struct WIFI_CMD *prWifiCmd;
	struct PARAM_KEY *prNewKey;
	struct CMD_802_11_KEY *prCmdKey;
	uint8_t ucCmdSeqNum;
	struct BSS_INFO *prBssInfo;
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecBssInfo;
	struct STA_RECORD *prStaRec = NULL;
	u_int8_t fgNoHandshakeSec = FALSE;
#if CFG_SUPPORT_TDLS
	struct STA_RECORD *prTmpStaRec;
#endif
	DEBUGFUNC("wlanoidSetAddKey");
	DBGLOG(REQ, LOUD, "\n");
	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);
	DBGLOG(RSN, TRACE, "wlanoidSetAddKey\n");
	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(RSN, WARN,
			"Fail in set add key! (Adapter not ready). ACPI=D%d, Radio=%d\n",
			prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}
	prNewKey = (struct PARAM_KEY *) pvSetBuffer;
	/* Verify the key structure length. */
	if (prNewKey->u4Length > u4SetBufferLen) {
		DBGLOG(RSN, WARN,
		       "Invalid key structure length (%d) greater than total buffer length (%d)\n",
		       (uint8_t) prNewKey->u4Length, (uint8_t) u4SetBufferLen);
		*pu4SetInfoLen = u4SetBufferLen;
		return WLAN_STATUS_INVALID_LENGTH;
	}
	/* Verify the key material length for key material buffer */
	if (prNewKey->u4KeyLength > prNewKey->u4Length -
	    OFFSET_OF(struct PARAM_KEY, aucKeyMaterial)) {
		DBGLOG(RSN, WARN, "Invalid key material length (%d)\n",
			(uint8_t) prNewKey->u4KeyLength);
		*pu4SetInfoLen = u4SetBufferLen;
		return WLAN_STATUS_INVALID_DATA;
	}
	/* Exception check */
	if (prNewKey->u4KeyIndex & 0x0fffff00)
		return WLAN_STATUS_INVALID_DATA;
	/* Exception check, pairwise key must with transmit bit enabled */
	if ((prNewKey->u4KeyIndex & BITS(30, 31)) == IS_UNICAST_KEY)
		return WLAN_STATUS_INVALID_DATA;
	if (!(prNewKey->u4KeyLength == WEP_40_LEN ||
	    prNewKey->u4KeyLength == WEP_104_LEN ||
	    prNewKey->u4KeyLength == CCMP_KEY_LEN ||
	    prNewKey->u4KeyLength == TKIP_KEY_LEN)) {
		return WLAN_STATUS_INVALID_DATA;
	}
	/* Exception check, pairwise key must with transmit bit enabled */
	if ((prNewKey->u4KeyIndex & BITS(30, 31)) == BITS(30, 31)) {
		if (/* ((prNewKey->u4KeyIndex & 0xff) != 0) || */
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

	/* Dump PARAM_KEY content. */
	DBGLOG(RSN, TRACE, "Set: Dump PARAM_KEY content, Len: 0x%08x, BSSID: "
		MACSTR
		", KeyIdx: 0x%08x, KeyLen: 0x%08x, Cipher: %d, Material:\n",
		prNewKey->u4Length, MAC2STR(prNewKey->arBSSID),
		prNewKey->u4KeyIndex, prNewKey->u4KeyLength,
		prNewKey->ucCipher);
	DBGLOG_MEM8(RSN, TRACE, prNewKey->aucKeyMaterial,
		    prNewKey->u4KeyLength);
	DBGLOG(RSN, TRACE, "Key RSC:\n");
	DBGLOG_MEM8(RSN, TRACE, &prNewKey->rKeyRSC, sizeof(uint64_t));

	prGlueInfo = prAdapter->prGlueInfo;
	prAisSpecBssInfo = &prAdapter->rWifiVar.rAisSpecificBssInfo;
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prNewKey->ucBssIdx);
	if (!prBssInfo) {
		DBGLOG(REQ, INFO, "BSS Info not exist !!\n");
		return WLAN_STATUS_SUCCESS;
	}
	/*         Tx  Rx KeyType addr
	 *  STA, GC:
	 *  case1: 1   1   0  BC addr (no sta record of AP at this moment)  WEP,
	 *  notice: tx at default key setting WEP key now save to BSS_INFO
	 *  case2: 0   1   0  BSSID (sta record of AP)        RSN BC key
	 *  case3: 1   1   1  AP addr (sta record of AP)      RSN STA key
	 *
	 *  GO:
	 *  case1: 1   1   0  BSSID (no sta record)           WEP -- Not support
	 *  case2: 1   0   0  BSSID (no sta record)           RSN BC key
	 *  case3: 1   1   1  STA addr                        STA key
	 */
	if (prNewKey->ucCipher == CIPHER_SUITE_WEP40 ||
	    prNewKey->ucCipher == CIPHER_SUITE_WEP104 ||
	    prNewKey->ucCipher == CIPHER_SUITE_WEP128) {
		/* check if the key no need handshake, then save to bss wep key
		 * for global usage
		 */
		fgNoHandshakeSec = TRUE;
	}
	if (fgNoHandshakeSec) {
#if DBG
		if (IS_BSS_AIS(prBssInfo)) {
			if (prAdapter->rWifiVar.rConnSettings.eAuthMode
			    >= AUTH_MODE_WPA &&
			    prAdapter->rWifiVar.rConnSettings.eAuthMode !=
			    AUTH_MODE_WPA_NONE) {
				DBGLOG(RSN, WARN,
					"Set wep at not open/shared setting\n");
				return WLAN_STATUS_SUCCESS;
			}
		}
#endif
	}
	if ((prNewKey->u4KeyIndex & IS_UNICAST_KEY) == IS_UNICAST_KEY) {
		prStaRec = cnmGetStaRecByAddress(prAdapter,
				prBssInfo->ucBssIndex, prNewKey->arBSSID);
		if (!prStaRec) {	/* Already disconnected ? */
			DBGLOG(REQ, INFO,
				"[wlan] Not set the peer key while disconnect\n");
			return WLAN_STATUS_SUCCESS;
		}
	}
	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter,
				(CMD_HDR_SIZE + sizeof(struct CMD_802_11_KEY)));
	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}
	/* increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);
	DBGLOG(RSN, TRACE, "ucCmdSeqNum = %d\n", ucCmdSeqNum);
	/* compose CMD_802_11_KEY cmd pkt */
	prCmdInfo->eCmdType = COMMAND_TYPE_NETWORK_IOCTL;
	prCmdInfo->u2InfoBufLen = CMD_HDR_SIZE + sizeof(struct CMD_802_11_KEY);
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
	prWifiCmd = (struct WIFI_CMD *) (prCmdInfo->pucInfoBuffer);
	prWifiCmd->u2TxByteCount = prCmdInfo->u2InfoBufLen;
	prWifiCmd->u2PQ_ID = CMD_PQ_ID;
	prWifiCmd->ucPktTypeID = CMD_PACKET_TYPE_ID;
	prWifiCmd->ucCID = prCmdInfo->ucCID;
	prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
	prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;
	prCmdKey = (struct CMD_802_11_KEY *) (prWifiCmd->aucBuffer);
	kalMemZero(prCmdKey, sizeof(struct CMD_802_11_KEY));
	prCmdKey->ucAddRemove = 1; /* Add */
	prCmdKey->ucTxKey =
		((prNewKey->u4KeyIndex & IS_TRANSMIT_KEY) == IS_TRANSMIT_KEY)
		? 1 : 0;
	prCmdKey->ucKeyType =
		((prNewKey->u4KeyIndex & IS_UNICAST_KEY) == IS_UNICAST_KEY)
		? 1 : 0;
	prCmdKey->ucIsAuthenticator =
		((prNewKey->u4KeyIndex & IS_AUTHENTICATOR) == IS_AUTHENTICATOR)
		? 1 : 0;
	/* Copy the addr of the key */
	if ((prNewKey->u4KeyIndex & IS_UNICAST_KEY) == IS_UNICAST_KEY) {
		if (prStaRec) {
			/* Overwrite the fgNoHandshakeSec in case */
			fgNoHandshakeSec = FALSE; /* Legacy 802.1x wep case ? */
			/* ASSERT(FALSE); */
		}
	} else {
		if (!IS_BSS_ACTIVE(prBssInfo))
			DBGLOG(REQ, INFO,
				"[wlan] BSS info (%d) not active yet!",
				prNewKey->ucBssIdx);
	}
	prCmdKey->ucBssIdx = prBssInfo->ucBssIndex;
	prCmdKey->ucKeyId = (uint8_t) (prNewKey->u4KeyIndex & 0xff);
	/* Note: the key length may not correct for WPA-None */
	prCmdKey->ucKeyLen = (uint8_t) prNewKey->u4KeyLength;
	kalMemCopy(prCmdKey->aucKeyMaterial,
		(uint8_t *)prNewKey->aucKeyMaterial, prCmdKey->ucKeyLen);
	if (prNewKey->ucCipher) {
		prCmdKey->ucAlgorithmId = prNewKey->ucCipher;
		if (IS_BSS_AIS(prBssInfo)) {
#if CFG_SUPPORT_802_11W
			if (prCmdKey->ucAlgorithmId == CIPHER_SUITE_BIP) {
				if (prCmdKey->ucKeyId >= 4) {
					struct AIS_SPECIFIC_BSS_INFO
							*prAisSpecBssInfo;

					prAisSpecBssInfo =
						&prAdapter->rWifiVar
						.rAisSpecificBssInfo;
					prAisSpecBssInfo->fgBipKeyInstalled =
						TRUE;
				}
			}
#endif
			if ((prCmdKey->ucAlgorithmId == CIPHER_SUITE_CCMP) &&
			    rsnCheckPmkidCandicate(prAdapter)) {
				DBGLOG(RSN, TRACE,
					"Add key: Prepare a timer to indicate candidate PMKID Candidate\n");
				cnmTimerStopTimer(prAdapter,
				  &prAisSpecBssInfo->rPreauthenticationTimer);
				cnmTimerStartTimer(prAdapter,
				  &prAisSpecBssInfo->rPreauthenticationTimer,
				  SEC_TO_MSEC(
					WAIT_TIME_IND_PMKID_CANDICATE_SEC));
			}

			if (prCmdKey->ucAlgorithmId == CIPHER_SUITE_TKIP) {
				/* Todo:: Support AP mode defragment */
				/* for pairwise key only */
				if ((prNewKey->u4KeyIndex & BITS(30, 31)) ==
				    ((IS_UNICAST_KEY) | (IS_TRANSMIT_KEY))) {
					kalMemCopy(
					  prAdapter->rWifiVar
					  .rAisSpecificBssInfo.aucRxMicKey,
					  &prCmdKey->aucKeyMaterial[16],
					  MIC_KEY_LEN);
					kalMemCopy(
					  prAdapter->rWifiVar
					  .rAisSpecificBssInfo.aucTxMicKey,
					  &prCmdKey->aucKeyMaterial[24],
					  MIC_KEY_LEN);
				}
			}
		} else {
#if CFG_SUPPORT_802_11W
			/* AP PMF */
			if ((prCmdKey->ucKeyId >= 4 && prCmdKey->ucKeyId <= 5)
			    && (prCmdKey->ucAlgorithmId == CIPHER_SUITE_BIP)) {
				DBGLOG(RSN, INFO, "AP mode set BIP\n");
				prBssInfo->rApPmfCfg.fgBipKeyInstalled = TRUE;
			}
#endif
		}
	} else { /* Legacy windows NDIS no cipher info */
#if 0
		if (prNewKey->u4KeyLength == 5) {
			prCmdKey->ucAlgorithmId = CIPHER_SUITE_WEP40;
		} else if (prNewKey->u4KeyLength == 13) {
			prCmdKey->ucAlgorithmId = CIPHER_SUITE_WEP104;
		} else if (prNewKey->u4KeyLength == 16) {
			if (prAdapter->rWifiVar.rConnSettings.eAuthMode <
			    AUTH_MODE_WPA)
				prCmdKey->ucAlgorithmId = CIPHER_SUITE_WEP128;
			else {
				if (IS_BSS_AIS(prBssInfo)) {
#if CFG_SUPPORT_802_11W
					if (prCmdKey->ucKeyId >= 4) {
						struct AIS_SPECIFIC_BSS_INFO
							*prAisSpecBssInfo;

						prCmdKey->ucAlgorithmId =
							CIPHER_SUITE_BIP;
						prAisSpecBssInfo =
							&prAdapter->rWifiVar
							.rAisSpecificBssInfo;
						prAisSpecBssInfo
							->fgBipKeyInstalled =
							TRUE;
					} else
#endif
					{
				prCmdKey->ucAlgorithmId = CIPHER_SUITE_CCMP;
				if (rsnCheckPmkidCandicate(prAdapter)) {
					DBGLOG(RSN, TRACE,
					  "Add key: Prepare a timer to indicate candidate PMKID\n");
					cnmTimerStopTimer(prAdapter,
					  &prAisSpecBssInfo
						->rPreauthenticationTimer);
					cnmTimerStartTimer(prAdapter,
					  &prAisSpecBssInfo
						->rPreauthenticationTimer,
					SEC_TO_MSEC(
					  WAIT_TIME_IND_PMKID_CANDICATE_SEC));
					}
				}
					}
			}
		} else if (prNewKey->u4KeyLength == 32) {
			if (IS_BSS_AIS(prBssInfo)) {
				if (prAdapter->rWifiVar.rConnSettings.eAuthMode
				    == AUTH_MODE_WPA_NONE) {
					if (prAdapter->rWifiVar.rConnSettings
						.eEncStatus ==
						ENUM_ENCRYPTION2_ENABLED) {
						prCmdKey->ucAlgorithmId =
							CIPHER_SUITE_TKIP;
					} else if (prAdapter->rWifiVar
						.rConnSettings.eEncStatus ==
						ENUM_ENCRYPTION3_ENABLED) {
						prCmdKey->ucAlgorithmId =
							CIPHER_SUITE_CCMP;
						prCmdKey->ucKeyLen =
							CCMP_KEY_LEN;
					}
				} else {
					prCmdKey->ucAlgorithmId =
						CIPHER_SUITE_TKIP;
					kalMemCopy(
						prAdapter->rWifiVar
							.rAisSpecificBssInfo
							.aucRxMicKey,
						&prCmdKey->aucKeyMaterial[16],
						MIC_KEY_LEN);
					kalMemCopy(
						prAdapter->rWifiVar
							.rAisSpecificBssInfo
							.aucTxMicKey,
						&prCmdKey->aucKeyMaterial[24],
						MIC_KEY_LEN);
			if (0 /* Todo::GCMP & GCMP-BIP ? */) {
				if (rsnCheckPmkidCandicate(prAdapter)) {
					DBGLOG(RSN, TRACE,
					  "Add key: Prepare a timer to indicate candidate PMKID\n");
					cnmTimerStopTimer(prAdapter,
					  &prAisSpecBssInfo->
						rPreauthenticationTimer);
					cnmTimerStartTimer(prAdapter,
					  &prAisSpecBssInfo->
						rPreauthenticationTimer,
					  SEC_TO_MSEC(
					    WAIT_TIME_IND_PMKID_CANDICATE_SEC));
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
		prTmpStaRec = cnmGetStaRecByAddress(prAdapter,
				prBssInfo->ucBssIndex, prNewKey->arBSSID);
		if (prTmpStaRec) {
			if (IS_DLS_STA(prTmpStaRec)) {
				prStaRec = prTmpStaRec;

				/*128 ,TODO  GCMP 256 */
				prCmdKey->ucAlgorithmId = CIPHER_SUITE_CCMP;

				kalMemCopy(prCmdKey->aucPeerAddr,
					prStaRec->aucMacAddr, MAC_ADDR_LEN);
			}
		}
#endif

#if CFG_SUPPORT_802_11W
		/* AP PMF */
		if (prCmdKey->ucAlgorithmId == CIPHER_SUITE_BIP) {
			if (prCmdKey->ucIsAuthenticator) {
				DBGLOG(RSN, INFO,
				"Authenticator BIP bssid:%d\n",
				prBssInfo->ucBssIndex);

				prCmdKey->ucWlanIndex =
					secPrivacySeekForBcEntry(prAdapter,
						prBssInfo->ucBssIndex,
						prBssInfo->aucOwnMacAddr,
						STA_REC_INDEX_NOT_FOUND,
						prCmdKey->ucAlgorithmId,
						prCmdKey->ucKeyId);
			} else {
				prCmdKey->ucWlanIndex =
				    secPrivacySeekForBcEntry(prAdapter,
					    prBssInfo->ucBssIndex,
					    prBssInfo->prStaRecOfAP->aucMacAddr,
					    prBssInfo->prStaRecOfAP->ucIndex,
					    prCmdKey->ucAlgorithmId,
					    prCmdKey->ucKeyId);
			}

			DBGLOG(RSN, INFO, "BIP BC wtbl index:%d\n",
				prCmdKey->ucWlanIndex);
		} else
#endif
		if (1) {
			if (prStaRec) {
				if (prCmdKey->ucKeyType) {	/* RSN STA */
					struct WLAN_TABLE *prWtbl;

					prWtbl = prAdapter->rWifiVar.arWtbl;
					prWtbl[prStaRec->ucWlanIndex].ucKeyId =
						prCmdKey->ucKeyId;
					prCmdKey->ucWlanIndex =
						prStaRec->ucWlanIndex;

					/* wait for CMD Done ? */
					prStaRec->fgTransmitKeyExist = TRUE;

					kalMemCopy(prCmdKey->aucPeerAddr,
						prNewKey->arBSSID,
						MAC_ADDR_LEN);
#if CFG_SUPPORT_802_11W
					/* AP PMF */
					DBGLOG(RSN, INFO,
						"Assign client PMF flag = %d\n",
						prStaRec->rPmfCfg.fgApplyPmf);
					prCmdKey->ucMgmtProtection =
						prStaRec->rPmfCfg.fgApplyPmf;
#endif
				} else {
					ASSERT(FALSE);
				}
			} else { /* Overwrite the old one for AP and STA WEP */
				if (prBssInfo->prStaRecOfAP) {
					DBGLOG(RSN, INFO, "AP REC\n");
					prCmdKey->ucWlanIndex =
					    secPrivacySeekForBcEntry(
						prAdapter,
						prBssInfo->ucBssIndex,
						prBssInfo->prStaRecOfAP
						    ->aucMacAddr,
						prBssInfo->prStaRecOfAP
						    ->ucIndex,
						prCmdKey->ucAlgorithmId,
						prCmdKey->ucKeyId);
					kalMemCopy(prCmdKey->aucPeerAddr,
						   prBssInfo->prStaRecOfAP
						   ->aucMacAddr,
						   MAC_ADDR_LEN);
				} else {
					DBGLOG(RSN, INFO, "!AP && !STA REC\n");
					prCmdKey->ucWlanIndex =
						secPrivacySeekForBcEntry(
						prAdapter,
						prBssInfo->ucBssIndex,
						prBssInfo->aucOwnMacAddr,
						STA_REC_INDEX_NOT_FOUND,
						prCmdKey->ucAlgorithmId,
						prCmdKey->ucKeyId);
					kalMemCopy(prCmdKey->aucPeerAddr,
						prBssInfo->aucOwnMacAddr,
						MAC_ADDR_LEN);
				}
				if (prCmdKey->ucKeyId >= MAX_KEY_NUM) {
					DBGLOG(RSN, ERROR,
						"prCmdKey->ucKeyId [%u] overrun\n",
						prCmdKey->ucKeyId);
					return WLAN_STATUS_FAILURE;
				}
				if (fgNoHandshakeSec) {
					/* WEP: STA and AP */
					prBssInfo->wepkeyWlanIdx =
						prCmdKey->ucWlanIndex;
					prBssInfo->wepkeyUsed[
						prCmdKey->ucKeyId] = TRUE;
				} else if (!prBssInfo->prStaRecOfAP) {
					/* AP WPA/RSN */
					prBssInfo->ucBMCWlanIndexS[
						prCmdKey->ucKeyId] =
						prCmdKey->ucWlanIndex;
					prBssInfo->ucBMCWlanIndexSUsed[
						prCmdKey->ucKeyId] = TRUE;
				} else {
					/* STA WPA/RSN, should not have tx but
					 * no sta record
					 */
					prBssInfo->ucBMCWlanIndexS[
						prCmdKey->ucKeyId] =
						prCmdKey->ucWlanIndex;
					prBssInfo->ucBMCWlanIndexSUsed[
						prCmdKey->ucKeyId] = TRUE;
					DBGLOG(RSN, INFO,
					       "BMCWlanIndex kid = %d, index = %d\n",
					       prCmdKey->ucKeyId,
					       prCmdKey->ucWlanIndex);
				}
				if (prCmdKey->ucTxKey) { /* */
					prBssInfo->fgBcDefaultKeyExist = TRUE;
					prBssInfo->ucBcDefaultKeyIdx =
							prCmdKey->ucKeyId;
				}
			}
		}
	}
#if 1
	DBGLOG(RSN, INFO, "Add key cmd to wlan index %d:",
	       prCmdKey->ucWlanIndex);
	DBGLOG(RSN, INFO, "(BSS = %d) " MACSTR "\n", prCmdKey->ucBssIdx,
	       MAC2STR(prCmdKey->aucPeerAddr));
	DBGLOG(RSN, INFO, "Tx = %d type = %d Auth = %d\n", prCmdKey->ucTxKey,
	       prCmdKey->ucKeyType,
	       prCmdKey->ucIsAuthenticator);
	DBGLOG(RSN, INFO, "cipher = %d keyid = %d keylen = %d\n",
	       prCmdKey->ucAlgorithmId, prCmdKey->ucKeyId,
	       prCmdKey->ucKeyLen);
	DBGLOG_MEM8(RSN, INFO, prCmdKey->aucKeyMaterial, prCmdKey->ucKeyLen);
	if (prCmdKey->ucKeyId < MAX_KEY_NUM) {
		DBGLOG(RSN, INFO, "wepkeyUsed = %d\n",
			prBssInfo->wepkeyUsed[prCmdKey->ucKeyId]);
		DBGLOG(RSN, INFO, "wepkeyWlanIdx = %d:",
			prBssInfo->wepkeyWlanIdx);
		DBGLOG(RSN, INFO, "ucBMCWlanIndexSUsed = %d\n",
			prBssInfo->ucBMCWlanIndexSUsed[prCmdKey->ucKeyId]);
		DBGLOG(RSN, INFO, "ucBMCWlanIndexS = %d:",
			prBssInfo->ucBMCWlanIndexS[prCmdKey->ucKeyId]);
	} else
		DBGLOG(RSN, WARN, "invalid prCmdKey->ucKeyId(%d)\n",
		       prCmdKey->ucKeyId);
#endif
	prAdapter->rWifiVar.rAisSpecificBssInfo.ucKeyAlgorithmId =
							prCmdKey->ucAlgorithmId;
	/* insert into prCmdQueue */
	kalEnqueueCommand(prGlueInfo, (struct QUE_ENTRY *) prCmdInfo);
	/* wakeup txServiceThread later */
	GLUE_SET_EVENT(prGlueInfo);
	return WLAN_STATUS_PENDING;
} /* wlanoidSetAddKey */

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
uint32_t
wlanoidSetRemoveKey(IN struct ADAPTER *prAdapter,
		    IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		    OUT uint32_t *pu4SetInfoLen) {
	struct GLUE_INFO *prGlueInfo;
	struct CMD_INFO *prCmdInfo;
	struct WIFI_CMD *prWifiCmd;
	struct PARAM_REMOVE_KEY *prRemovedKey;
	struct CMD_802_11_KEY *prCmdKey;
	uint8_t ucCmdSeqNum;
	struct WLAN_TABLE *prWlanTable;
	struct STA_RECORD *prStaRec = NULL;
	struct BSS_INFO *prBssInfo;
	/*	UINT_8 i = 0;	*/
	u_int8_t fgRemoveWepKey = FALSE;
	u_int8_t fgRemoveBCKey = FALSE;
	uint32_t ucRemoveBCKeyAtIdx = WTBL_RESERVED_ENTRY;
	uint32_t u4KeyIndex;

	DEBUGFUNC("wlanoidSetRemoveKey");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	DBGLOG(RSN, INFO, "wlanoidSetRemoveKey\n");

	prWlanTable = prAdapter->rWifiVar.arWtbl;
	*pu4SetInfoLen = sizeof(struct PARAM_REMOVE_KEY);

	if (u4SetBufferLen < sizeof(struct PARAM_REMOVE_KEY))
		return WLAN_STATUS_INVALID_LENGTH;

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in set remove key! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	ASSERT(pvSetBuffer);
	prRemovedKey = (struct PARAM_REMOVE_KEY *) pvSetBuffer;

	/* Dump PARAM_REMOVE_KEY content. */
	DBGLOG(RSN, INFO, "Set: Dump PARAM_REMOVE_KEY content\n");
	DBGLOG(RSN, INFO, "Length    : 0x%08x\n",
	       prRemovedKey->u4Length);
	DBGLOG(RSN, INFO, "Key Index : 0x%08x\n",
	       prRemovedKey->u4KeyIndex);
	DBGLOG(RSN, INFO, "BSS_INDEX : %d\n",
	       prRemovedKey->ucBssIdx);
	DBGLOG(RSN, INFO, "BSSID:\n");
	DBGLOG_MEM8(RSN, INFO, prRemovedKey->arBSSID, MAC_ADDR_LEN);

	prGlueInfo = prAdapter->prGlueInfo;
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
					  prRemovedKey->ucBssIdx);
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
		prStaRec = cnmGetStaRecByAddress(prAdapter,
				prRemovedKey->ucBssIdx, prRemovedKey->arBSSID);
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
			DBGLOG(RSN, INFO, "Remove group key id = %d",
			       u4KeyIndex);

			if (prBssInfo->ucBMCWlanIndexSUsed[u4KeyIndex]) {
				if (prBssInfo->fgBcDefaultKeyExist &&
				    prBssInfo->ucBcDefaultKeyIdx ==
				    u4KeyIndex) {
					prBssInfo->fgBcDefaultKeyExist = FALSE;
					prBssInfo->ucBcDefaultKeyIdx = 0xff;
				}
				if (u4KeyIndex != 0)
					ASSERT(prBssInfo->ucBMCWlanIndexS[
						u4KeyIndex] < WTBL_SIZE);
				ucRemoveBCKeyAtIdx =
					prBssInfo->ucBMCWlanIndexS[u4KeyIndex];
				prBssInfo->ucBMCWlanIndexSUsed[u4KeyIndex] =
					FALSE;
				prBssInfo->ucBMCWlanIndexS[u4KeyIndex] =
					WTBL_RESERVED_ENTRY;
				fgRemoveBCKey = TRUE;
			}
		}

		/* Change the wtbl to not used state */
		if (fgRemoveBCKey)
			prWlanTable[ucRemoveBCKeyAtIdx].ucUsed = FALSE;

		DBGLOG(RSN, INFO, "ucRemoveBCKeyAtIdx = %d",
		       ucRemoveBCKeyAtIdx);

		if (ucRemoveBCKeyAtIdx >= WTBL_SIZE)
			return WLAN_STATUS_SUCCESS;
	}

	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter,
			  (CMD_HDR_SIZE + sizeof(struct CMD_802_11_KEY)));

	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
					  prRemovedKey->ucBssIdx);

	/* increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

	/* compose CMD_802_11_KEY cmd pkt */
	prCmdInfo->eCmdType = COMMAND_TYPE_NETWORK_IOCTL;
	/* prCmdInfo->ucBssIndex = prRemovedKey->ucBssIdx; */
	prCmdInfo->u2InfoBufLen = CMD_HDR_SIZE + sizeof(
					  struct CMD_802_11_KEY);
	prCmdInfo->pfCmdDoneHandler = nicCmdEventSetCommon;
	prCmdInfo->pfCmdTimeoutHandler = nicOidCmdTimeoutCommon;
	prCmdInfo->fgIsOid = g_fgIsOid;
	prCmdInfo->ucCID = CMD_ID_ADD_REMOVE_KEY;
	prCmdInfo->fgSetQuery = TRUE;
	prCmdInfo->fgNeedResp = FALSE;
	/* prCmdInfo->fgDriverDomainMCR = FALSE; */
	prCmdInfo->ucCmdSeqNum = ucCmdSeqNum;
	prCmdInfo->u4SetInfoLen = sizeof(struct PARAM_REMOVE_KEY);
	prCmdInfo->pvInformationBuffer = pvSetBuffer;
	prCmdInfo->u4InformationBufferLength = u4SetBufferLen;

	/* Setup WIFI_CMD_T */
	prWifiCmd = (struct WIFI_CMD *) (prCmdInfo->pucInfoBuffer);
	prWifiCmd->u2TxByteCount = prCmdInfo->u2InfoBufLen;
	prWifiCmd->u2PQ_ID = CMD_PQ_ID;
	prWifiCmd->ucPktTypeID = CMD_PACKET_TYPE_ID;
	prWifiCmd->ucCID = prCmdInfo->ucCID;
	prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
	prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;

	prCmdKey = (struct CMD_802_11_KEY *) (prWifiCmd->aucBuffer);

	kalMemZero((uint8_t *) prCmdKey,
		   sizeof(struct CMD_802_11_KEY));

	prCmdKey->ucAddRemove = 0;	/* Remove */
	prCmdKey->ucKeyId = (uint8_t) u4KeyIndex;
	kalMemCopy(prCmdKey->aucPeerAddr,
		   (uint8_t *) prRemovedKey->arBSSID, MAC_ADDR_LEN);
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
	kalEnqueueCommand(prGlueInfo,
			  (struct QUE_ENTRY *) prCmdInfo);

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
uint32_t
wlanoidSetDefaultKey(IN struct ADAPTER *prAdapter,
		     IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		     OUT uint32_t *pu4SetInfoLen) {
	struct GLUE_INFO *prGlueInfo;
	struct CMD_INFO *prCmdInfo;
	struct WIFI_CMD *prWifiCmd;
	struct PARAM_DEFAULT_KEY *prDefaultKey;
	struct CMD_DEFAULT_KEY *prCmdDefaultKey;
	uint8_t ucCmdSeqNum;
	struct BSS_INFO *prBssInfo;
	u_int8_t fgSetWepKey = FALSE;
	uint8_t ucWlanIndex = WTBL_RESERVED_ENTRY;

	DEBUGFUNC("wlanoidSetDefaultKey");

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in set add key! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	prDefaultKey = (struct PARAM_DEFAULT_KEY *) pvSetBuffer;

	*pu4SetInfoLen = u4SetBufferLen;

	/* Dump PARAM_DEFAULT_KEY_T content. */
	DBGLOG(RSN, INFO,
	       "Key Index : %d, Unicast Key : %d, Multicast Key : %d\n",
	       prDefaultKey->ucKeyID, prDefaultKey->ucUnicast,
	       prDefaultKey->ucMulticast);

	/* prWlanTable = prAdapter->rWifiVar.arWtbl; */
	prGlueInfo = prAdapter->prGlueInfo;

	if (prDefaultKey->ucBssIdx > HW_BSSID_NUM)
		return WLAN_STATUS_FAILURE;
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
					  prDefaultKey->ucBssIdx);

	DBGLOG(RSN, INFO, "WlanIdx = %d\n",
	       prBssInfo->wepkeyWlanIdx);

	if (prDefaultKey->ucMulticast) {
		if (prBssInfo->prStaRecOfAP) {	/* Actually GC not have wep */
			if (prBssInfo->wepkeyUsed[prDefaultKey->ucKeyID]) {
				prBssInfo->ucBcDefaultKeyIdx =
							prDefaultKey->ucKeyID;
				prBssInfo->fgBcDefaultKeyExist = TRUE;
				ucWlanIndex = prBssInfo->wepkeyWlanIdx;
			} else {
				if (prDefaultKey->ucUnicast) {
					DBGLOG(RSN, ERROR,
					       "Set STA Unicast default key");
					return WLAN_STATUS_SUCCESS;
				}
				ASSERT(FALSE);
			}
		} else {	/* For AP mode only */

			if (prBssInfo->wepkeyUsed[prDefaultKey->ucKeyID]
			    == TRUE)
				fgSetWepKey = TRUE;

			if (fgSetWepKey) {
				ucWlanIndex = prBssInfo->wepkeyWlanIdx;
			} else {
				if (!prBssInfo->ucBMCWlanIndexSUsed[
				    prDefaultKey->ucKeyID]) {
					DBGLOG(RSN, ERROR,
					       "Set AP wep default but key not exist!");
					return WLAN_STATUS_SUCCESS;
				}
				ucWlanIndex = prBssInfo->ucBMCWlanIndexS[
							prDefaultKey->ucKeyID];
			}
			prBssInfo->ucBcDefaultKeyIdx = prDefaultKey->ucKeyID;
			prBssInfo->fgBcDefaultKeyExist = TRUE;
		}
		if (ucWlanIndex > WTBL_SIZE)
			ASSERT(FALSE);

	} else {
		DBGLOG(RSN, ERROR,
		       "Check the case set unicast default key!");
		ASSERT(FALSE);
	}

	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter,
			(CMD_HDR_SIZE + sizeof(struct CMD_DEFAULT_KEY)));

	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}
	/* increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);
	DBGLOG(RSN, TRACE,
	       "ucCmdSeqNum = %d, CMD_ID_DEFAULT_KEY_ID (%d) with wlan idx = %d\n",
	       ucCmdSeqNum, prDefaultKey->ucKeyID, ucWlanIndex);

	/* compose CMD_802_11_KEY cmd pkt */
	prCmdInfo->eCmdType = COMMAND_TYPE_NETWORK_IOCTL;
	prCmdInfo->u2InfoBufLen = CMD_HDR_SIZE + sizeof(
					  struct CMD_DEFAULT_KEY);
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
	prWifiCmd = (struct WIFI_CMD *) (prCmdInfo->pucInfoBuffer);
	prWifiCmd->u2TxByteCount = prCmdInfo->u2InfoBufLen;
	prWifiCmd->u2PQ_ID = CMD_PQ_ID;
	prWifiCmd->ucPktTypeID = CMD_PACKET_TYPE_ID;
	prWifiCmd->ucCID = prCmdInfo->ucCID;
	prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
	prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;

	prCmdDefaultKey = (struct CMD_DEFAULT_KEY *) (
				  prWifiCmd->aucBuffer);

	kalMemZero(prCmdDefaultKey, sizeof(struct CMD_DEFAULT_KEY));

	prCmdDefaultKey->ucBssIdx = prDefaultKey->ucBssIdx;
	prCmdDefaultKey->ucKeyId = prDefaultKey->ucKeyID;
	prCmdDefaultKey->ucWlanIndex = ucWlanIndex;
	prCmdDefaultKey->ucMulticast = prDefaultKey->ucMulticast;

	DBGLOG(RSN, INFO,
	       "CMD_ID_DEFAULT_KEY_ID (%d) with wlan idx = %d\n",
	       prDefaultKey->ucKeyID, ucWlanIndex);

	/* insert into prCmdQueue */
	kalEnqueueCommand(prGlueInfo,
			  (struct QUE_ENTRY *) prCmdInfo);

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
uint32_t
wlanoidQueryEncryptionStatus(IN struct ADAPTER *prAdapter,
			     IN void *pvQueryBuffer,
			     IN uint32_t u4QueryBufferLen,
			     OUT uint32_t *pu4QueryInfoLen) {
	u_int8_t fgTransmitKeyAvailable = TRUE;
	enum ENUM_WEP_STATUS eEncStatus = 0;

	DEBUGFUNC("wlanoidQueryEncryptionStatus");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(enum ENUM_WEP_STATUS);

	fgTransmitKeyAvailable =
		prAdapter->prAisBssInfo->fgBcDefaultKeyExist;

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
	       "Encryption status: %d Return:%d\n",
	       prAdapter->rWifiVar.rConnSettings.eEncStatus, eEncStatus);
#endif

	*(enum ENUM_WEP_STATUS *) pvQueryBuffer = eEncStatus;

	return WLAN_STATUS_SUCCESS;
} /* wlanoidQueryEncryptionStatus */

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
uint32_t
wlanoidSetEncryptionStatus(IN struct ADAPTER *prAdapter,
			   IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			   OUT uint32_t *pu4SetInfoLen) {
	struct GLUE_INFO *prGlueInfo;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	enum ENUM_WEP_STATUS eEewEncrypt;

	DEBUGFUNC("wlanoidSetEncryptionStatus");

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	prGlueInfo = prAdapter->prGlueInfo;

	*pu4SetInfoLen = sizeof(enum ENUM_WEP_STATUS);

	/* if (IS_ARB_IN_RFTEST_STATE(prAdapter)) { */
	/* return WLAN_STATUS_SUCCESS; */
	/* } */

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in set encryption status! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	eEewEncrypt = *(enum ENUM_WEP_STATUS *) pvSetBuffer;
	DBGLOG(REQ, INFO, "ENCRYPTION_STATUS %d\n", eEewEncrypt);

	switch (eEewEncrypt) {
	case ENUM_ENCRYPTION_DISABLED:	/* Disable WEP, TKIP, AES */
		DBGLOG(RSN, INFO, "Disable Encryption\n");
		secSetCipherSuite(prAdapter,
				  CIPHER_FLAG_WEP40 | CIPHER_FLAG_WEP104 |
				  CIPHER_FLAG_WEP128);
		break;

	case ENUM_ENCRYPTION1_ENABLED:	/* Enable WEP. Disable TKIP, AES */
		DBGLOG(RSN, INFO, "Enable Encryption1\n");
		secSetCipherSuite(prAdapter,
				  CIPHER_FLAG_WEP40 | CIPHER_FLAG_WEP104 |
				  CIPHER_FLAG_WEP128);
		break;

	case ENUM_ENCRYPTION2_ENABLED:	/* Enable WEP, TKIP. Disable AES */
		secSetCipherSuite(prAdapter,
				  CIPHER_FLAG_WEP40 | CIPHER_FLAG_WEP104 |
				  CIPHER_FLAG_WEP128 | CIPHER_FLAG_TKIP);
		DBGLOG(RSN, INFO, "Enable Encryption2\n");
		break;

	case ENUM_ENCRYPTION3_ENABLED:	/* Enable WEP, TKIP, AES */
		secSetCipherSuite(prAdapter,
				  CIPHER_FLAG_WEP40 |
				  CIPHER_FLAG_WEP104 | CIPHER_FLAG_WEP128 |
				  CIPHER_FLAG_TKIP | CIPHER_FLAG_CCMP);
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
		       *(enum ENUM_WEP_STATUS *) pvSetBuffer);

		rStatus = WLAN_STATUS_NOT_SUPPORTED;
	}

	if (rStatus == WLAN_STATUS_SUCCESS) {
		/* Save the new encryption status. */
		prAdapter->rWifiVar.rConnSettings.eEncStatus = *
				(enum ENUM_WEP_STATUS *) pvSetBuffer;
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
uint32_t
wlanoidSetTest(IN struct ADAPTER *prAdapter,
	       IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
	       OUT uint32_t *pu4SetInfoLen) {
	struct PARAM_802_11_TEST *prTest;
	void *pvTestData;
	void *pvStatusBuffer;
	uint32_t u4StatusBufferSize;

	DEBUGFUNC("wlanoidSetTest");

	ASSERT(prAdapter);

	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = u4SetBufferLen;

	prTest = (struct PARAM_802_11_TEST *) pvSetBuffer;

	DBGLOG(REQ, TRACE, "Test - Type %u\n", prTest->u4Type);

	switch (prTest->u4Type) {
	case 1:		/* Type 1: generate an authentication event */
		pvTestData = (void *) &prTest->u.AuthenticationEvent;
		pvStatusBuffer = (void *)
				 prAdapter->aucIndicationEventBuffer;
		u4StatusBufferSize = prTest->u4Length - 8;
		if (u4StatusBufferSize > sizeof(
			    prTest->u.AuthenticationEvent))
			return WLAN_STATUS_INVALID_LENGTH;
		break;

	case 2:		/* Type 2: generate an RSSI status indication */
		pvTestData = (void *) &prTest->u.RssiTrigger;
		pvStatusBuffer = (void *)
				 &prAdapter->rWlanInfo.rCurrBssId.rRssi;
		u4StatusBufferSize = sizeof(int32_t);
		break;

	default:
		return WLAN_STATUS_INVALID_DATA;
	}

	/* Get the contents of the StatusBuffer from the test structure. */
	kalMemCopy(pvStatusBuffer, pvTestData, u4StatusBufferSize);

	kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
				     WLAN_STATUS_MEDIA_SPECIFIC_INDICATION,
				     pvStatusBuffer, u4StatusBufferSize);

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
uint32_t
wlanoidQueryCapability(IN struct ADAPTER *prAdapter,
		       OUT void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
		       OUT uint32_t *pu4QueryInfoLen) {
	struct PARAM_CAPABILITY *prCap;
	struct PARAM_AUTH_ENCRYPTION
		*prAuthenticationEncryptionSupported;

	DEBUGFUNC("wlanoidQueryCapability");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = 4 * sizeof(uint32_t) + 14 * sizeof(
				   struct PARAM_AUTH_ENCRYPTION);

	if (u4QueryBufferLen < *pu4QueryInfoLen)
		return WLAN_STATUS_INVALID_LENGTH;

	prCap = (struct PARAM_CAPABILITY *) pvQueryBuffer;

	prCap->u4Length = *pu4QueryInfoLen;
	prCap->u4Version = 2;	/* WPA2 */
	prCap->u4NoOfPMKIDs = CFG_MAX_PMKID_CACHE;
	prCap->u4NoOfAuthEncryptPairsSupported = 14;

	prAuthenticationEncryptionSupported =
		&prCap->arAuthenticationEncryptionSupported[0];

	/* fill 14 entries of supported settings */
	prAuthenticationEncryptionSupported[0].eAuthModeSupported =
		AUTH_MODE_OPEN;

	prAuthenticationEncryptionSupported[0].eEncryptStatusSupported
		= ENUM_ENCRYPTION_DISABLED;

	prAuthenticationEncryptionSupported[1].eAuthModeSupported =
		AUTH_MODE_OPEN;
	prAuthenticationEncryptionSupported[1].eEncryptStatusSupported
		= ENUM_ENCRYPTION1_ENABLED;

	prAuthenticationEncryptionSupported[2].eAuthModeSupported =
		AUTH_MODE_SHARED;
	prAuthenticationEncryptionSupported[2].eEncryptStatusSupported
		= ENUM_ENCRYPTION_DISABLED;

	prAuthenticationEncryptionSupported[3].eAuthModeSupported =
		AUTH_MODE_SHARED;
	prAuthenticationEncryptionSupported[3].eEncryptStatusSupported
		= ENUM_ENCRYPTION1_ENABLED;

	prAuthenticationEncryptionSupported[4].eAuthModeSupported =
		AUTH_MODE_WPA;
	prAuthenticationEncryptionSupported[4].eEncryptStatusSupported
		= ENUM_ENCRYPTION2_ENABLED;

	prAuthenticationEncryptionSupported[5].eAuthModeSupported =
		AUTH_MODE_WPA;
	prAuthenticationEncryptionSupported[5].eEncryptStatusSupported
		= ENUM_ENCRYPTION3_ENABLED;

	prAuthenticationEncryptionSupported[6].eAuthModeSupported =
		AUTH_MODE_WPA_PSK;
	prAuthenticationEncryptionSupported[6].eEncryptStatusSupported
		= ENUM_ENCRYPTION2_ENABLED;

	prAuthenticationEncryptionSupported[7].eAuthModeSupported =
		AUTH_MODE_WPA_PSK;
	prAuthenticationEncryptionSupported[7].eEncryptStatusSupported
		= ENUM_ENCRYPTION3_ENABLED;

	prAuthenticationEncryptionSupported[8].eAuthModeSupported =
		AUTH_MODE_WPA_NONE;
	prAuthenticationEncryptionSupported[8].eEncryptStatusSupported
		= ENUM_ENCRYPTION2_ENABLED;

	prAuthenticationEncryptionSupported[9].eAuthModeSupported =
		AUTH_MODE_WPA_NONE;
	prAuthenticationEncryptionSupported[9].eEncryptStatusSupported
		= ENUM_ENCRYPTION3_ENABLED;

	prAuthenticationEncryptionSupported[10].eAuthModeSupported =
		AUTH_MODE_WPA2;
	prAuthenticationEncryptionSupported[10].eEncryptStatusSupported
		= ENUM_ENCRYPTION2_ENABLED;

	prAuthenticationEncryptionSupported[11].eAuthModeSupported =
		AUTH_MODE_WPA2;
	prAuthenticationEncryptionSupported[11].eEncryptStatusSupported
		= ENUM_ENCRYPTION3_ENABLED;

	prAuthenticationEncryptionSupported[12].eAuthModeSupported =
		AUTH_MODE_WPA2_PSK;
	prAuthenticationEncryptionSupported[12].eEncryptStatusSupported
		= ENUM_ENCRYPTION2_ENABLED;

	prAuthenticationEncryptionSupported[13].eAuthModeSupported =
		AUTH_MODE_WPA2_PSK;
	prAuthenticationEncryptionSupported[13].eEncryptStatusSupported
		= ENUM_ENCRYPTION3_ENABLED;

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
uint32_t
wlanoidQueryPmkid(IN struct ADAPTER *prAdapter,
		  OUT void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
		  OUT uint32_t *pu4QueryInfoLen) {
	uint32_t i;
	struct PARAM_PMKID *prPmkid;
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecBssInfo;

	DEBUGFUNC("wlanoidQueryPmkid");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	prAisSpecBssInfo = &prAdapter->rWifiVar.rAisSpecificBssInfo;

	*pu4QueryInfoLen = OFFSET_OF(struct PARAM_PMKID,
				     arBSSIDInfo) +
			   prAisSpecBssInfo->u4PmkidCacheCount * sizeof(
				   struct PARAM_BSSID_INFO);

	if (u4QueryBufferLen < *pu4QueryInfoLen)
		return WLAN_STATUS_INVALID_LENGTH;

	prPmkid = (struct PARAM_PMKID *) pvQueryBuffer;

	prPmkid->u4Length = *pu4QueryInfoLen;
	prPmkid->u4BSSIDInfoCount =
		prAisSpecBssInfo->u4PmkidCacheCount;

	for (i = 0; i < prAisSpecBssInfo->u4PmkidCacheCount; i++) {
		kalMemCopy(prPmkid->arBSSIDInfo[i].arBSSID,
			   prAisSpecBssInfo->arPmkidCache[i].rBssidInfo.arBSSID,
			   (sizeof(uint8_t) * PARAM_MAC_ADDR_LEN));
		kalMemCopy(prPmkid->arBSSIDInfo[i].arPMKID,
			   prAisSpecBssInfo->arPmkidCache[i].rBssidInfo.arPMKID,
			   (sizeof(uint8_t) * 16));
	}

	return WLAN_STATUS_SUCCESS;

}				/* wlanoidQueryPmkid */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to set the PMKID to the PMK cache in the
 *        driver.
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
uint32_t
wlanoidSetPmkid(IN struct ADAPTER *prAdapter,
		IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		OUT uint32_t *pu4SetInfoLen) {
	uint32_t i, j;
	struct PARAM_PMKID *prPmkid;
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecBssInfo;

	DEBUGFUNC("wlanoidSetPmkid");

	DBGLOG(REQ, INFO, "wlanoidSetPmkid\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = u4SetBufferLen;

	/* It's possibble BSSIDInfoCount is zero, because OS wishes to clean
	 * PMKID
	 */
	if (u4SetBufferLen < OFFSET_OF(struct PARAM_PMKID,
				       arBSSIDInfo))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	ASSERT(pvSetBuffer);
	prPmkid = (struct PARAM_PMKID *) pvSetBuffer;

	if (u4SetBufferLen <
	    ((prPmkid->u4BSSIDInfoCount * sizeof(struct
			    PARAM_BSSID_INFO)) + OFFSET_OF(struct PARAM_PMKID,
					    arBSSIDInfo)))
		return WLAN_STATUS_INVALID_DATA;

	if (prPmkid->u4BSSIDInfoCount > CFG_MAX_PMKID_CACHE)
		return WLAN_STATUS_INVALID_DATA;

	DBGLOG(REQ, INFO, "Count %u\n", prPmkid->u4BSSIDInfoCount);

	prAisSpecBssInfo = &prAdapter->rWifiVar.rAisSpecificBssInfo;

	/* This OID replace everything in the PMKID cache. */
	if (prPmkid->u4BSSIDInfoCount == 0) {
		prAisSpecBssInfo->u4PmkidCacheCount = 0;
		kalMemZero(prAisSpecBssInfo->arPmkidCache,
			   sizeof(struct PMKID_ENTRY) * CFG_MAX_PMKID_CACHE);
	}
	if ((prAisSpecBssInfo->u4PmkidCacheCount +
	     prPmkid->u4BSSIDInfoCount > CFG_MAX_PMKID_CACHE)) {
		prAisSpecBssInfo->u4PmkidCacheCount = 0;
		kalMemZero(prAisSpecBssInfo->arPmkidCache,
			   sizeof(struct PMKID_ENTRY) * CFG_MAX_PMKID_CACHE);
	}

	/*
	 *  The driver can only clear its PMKID cache whenever it make a media
	 *  disconnect indication. Otherwise, it must change the PMKID cache
	 *  only when set through this OID.
	 */
	for (i = 0; i < prPmkid->u4BSSIDInfoCount; i++) {
		/* Search for desired BSSID. If desired BSSID is found,
		 *  then set the PMKID
		 */
		if (!rsnSearchPmkidEntry(prAdapter,
		    (uint8_t *) prPmkid->arBSSIDInfo[i].arBSSID, &j)) {
			/* No entry found for the specified BSSID, so add one
			 * entry
			 */
			if (prAisSpecBssInfo->u4PmkidCacheCount <
			    CFG_MAX_PMKID_CACHE - 1) {
				j = prAisSpecBssInfo->u4PmkidCacheCount;
				kalMemCopy(
					prAisSpecBssInfo->arPmkidCache[j]
					.rBssidInfo.arBSSID,
					prPmkid->arBSSIDInfo[i].arBSSID,
					(sizeof(uint8_t) * PARAM_MAC_ADDR_LEN));
				prAisSpecBssInfo->u4PmkidCacheCount++;
			} else {
				j = CFG_MAX_PMKID_CACHE;
			}
		}

		if (j < CFG_MAX_PMKID_CACHE) {
			kalMemCopy(
				prAisSpecBssInfo->arPmkidCache[j].rBssidInfo
				.arPMKID,
				prPmkid->arBSSIDInfo[i].arPMKID,
				(sizeof(uint8_t) * 16));
			DBGLOG(RSN, TRACE,
			       "Add BSSID " MACSTR " idx=%u PMKID value " MACSTR
			       "\n",
			       MAC2STR(prAisSpecBssInfo->arPmkidCache[j]
			       .rBssidInfo.arBSSID),
			       j,
			       MAC2STR(prAisSpecBssInfo->arPmkidCache[j]
			       .rBssidInfo.arPMKID));
			prAisSpecBssInfo->arPmkidCache[j].fgPmkidExist = TRUE;
		}
	}

	if (prAdapter->rWifiVar.rConnSettings.fgOkcEnabled) {
		struct BSS_DESC *prBssDesc =
				prAdapter->rWifiVar.rAisFsmInfo.prTargetBssDesc;
		uint8_t *pucPmkID = NULL;

		if ((prPmkid->u4Length & BIT(31)) ||
		    (prBssDesc && EQUAL_MAC_ADDR(
		    prPmkid->arBSSIDInfo[0].arBSSID, prBssDesc->aucBSSID))) {
			if (j == CFG_MAX_PMKID_CACHE) {
				j = 0;
				kalMemCopy(
					prAisSpecBssInfo->arPmkidCache[0]
					.rBssidInfo.arBSSID,
					prPmkid->arBSSIDInfo[0].arBSSID,
					(sizeof(uint8_t) * PARAM_MAC_ADDR_LEN));
				kalMemCopy(
					prAisSpecBssInfo->arPmkidCache[0]
					.rBssidInfo.arPMKID,
					prPmkid->arBSSIDInfo[0].arPMKID,
					(sizeof(uint8_t) * 16));
				prAisSpecBssInfo->arPmkidCache[0].fgPmkidExist
									= TRUE;
			}
			pucPmkID = prAisSpecBssInfo->arPmkidCache[j].rBssidInfo
				   .arPMKID;
			log_dbg(RSN, INFO, MACSTR " OKC PMKID %02x%02x%02x%02x%02x%02x%02x%02x...\n",
				MAC2STR(prAisSpecBssInfo->
				arPmkidCache[j].rBssidInfo.arBSSID),
				pucPmkID[0], pucPmkID[1],
				pucPmkID[2], pucPmkID[3],
				pucPmkID[4], pucPmkID[5],
				pucPmkID[6], pucPmkID[7]);
		}
		aisFsmRunEventSetOkcPmk(prAdapter);
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
uint32_t
wlanoidQuerySupportedRates(IN struct ADAPTER *prAdapter,
			   OUT void *pvQueryBuffer,
			   IN uint32_t u4QueryBufferLen,
			   OUT uint32_t *pu4QueryInfoLen) {
	uint8_t eRate[PARAM_MAX_LEN_RATES] = {
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

	*pu4QueryInfoLen = (sizeof(uint8_t) *
			    PARAM_MAX_LEN_RATES_EX);

	if (u4QueryBufferLen < *pu4QueryInfoLen) {
		DBGLOG(REQ, WARN, "Invalid length %u\n", u4QueryBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	kalMemCopy(pvQueryBuffer, (void *) &eRate,
		   (sizeof(uint8_t) * PARAM_MAX_LEN_RATES));

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
 * \param[out] pu4QueryInfoLen   If the call is successful, returns the number
 *				 of bytes written into the query buffer. If the
 *				 call failed due to invalid length of the query
 *				 buffer, returns the amount of storage needed.
 *
 * \retval WLAN_STATUS_SUCCESS
 * \retval WLAN_STATUS_INVALID_LENGTH
 */
/*----------------------------------------------------------------------------*/
uint32_t
wlanoidQueryDesiredRates(IN struct ADAPTER *prAdapter,
			 OUT void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
			 OUT uint32_t *pu4QueryInfoLen) {
	DEBUGFUNC("wlanoidQueryDesiredRates");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = (sizeof(uint8_t) *
			    PARAM_MAX_LEN_RATES_EX);

	if (u4QueryBufferLen < *pu4QueryInfoLen) {
		DBGLOG(REQ, WARN, "Invalid length %u\n", u4QueryBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	kalMemCopy(pvQueryBuffer,
		   (void *) &(prAdapter->rWlanInfo.eDesiredRates),
		   (sizeof(uint8_t) * PARAM_MAX_LEN_RATES));

	return WLAN_STATUS_SUCCESS;

}				/* end of wlanoidQueryDesiredRates() */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to Set the desired rates.
 *
 * \param[in] prAdapter      Pointer to the Adapter structure.
 * \param[in] pvSetBuffer    Pointer to the buffer that holds the data to be
 *			     set.
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
uint32_t
wlanoidSetDesiredRates(IN struct ADAPTER *prAdapter,
		       IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		       OUT uint32_t *pu4SetInfoLen) {
	uint32_t i;

	DEBUGFUNC("wlanoidSetDesiredRates");

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	if (u4SetBufferLen < (sizeof(uint8_t) *
			      PARAM_MAX_LEN_RATES)) {
		DBGLOG(REQ, WARN, "Invalid length %u\n", u4SetBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	*pu4SetInfoLen = (sizeof(uint8_t) * PARAM_MAX_LEN_RATES);

	if (u4SetBufferLen < (sizeof(uint8_t) *
			      PARAM_MAX_LEN_RATES))
		return WLAN_STATUS_INVALID_LENGTH;

	kalMemCopy((void *) &(prAdapter->rWlanInfo.eDesiredRates),
		   pvSetBuffer, (sizeof(uint8_t) * PARAM_MAX_LEN_RATES));

	prAdapter->rWlanInfo.eLinkAttr.ucDesiredRateLen =
		PARAM_MAX_LEN_RATES;
	for (i = 0; i < PARAM_MAX_LEN_RATES; i++)
		prAdapter->rWlanInfo.eLinkAttr.u2DesiredRate[i] =
			(uint16_t) (prAdapter->rWlanInfo.eDesiredRates[i]);

	return wlanSendSetQueryCmd(prAdapter,
				CMD_ID_LINK_ATTRIB,
				TRUE,
				FALSE,
				g_fgIsOid,
				nicCmdEventSetCommon,
				nicOidCmdTimeoutCommon,
				sizeof(struct CMD_LINK_ATTRIB),
				(uint8_t *) &(prAdapter->rWlanInfo.eLinkAttr),
				pvSetBuffer,
				u4SetBufferLen);

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
uint32_t
wlanoidQueryMaxFrameSize(IN struct ADAPTER *prAdapter,
			 OUT void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
			 OUT uint32_t *pu4QueryInfoLen) {
	DEBUGFUNC("wlanoidQueryMaxFrameSize");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (u4QueryBufferLen < sizeof(uint32_t)) {
		*pu4QueryInfoLen = sizeof(uint32_t);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	*(uint32_t *) pvQueryBuffer = ETHERNET_MAX_PKT_SZ -
				      ETHERNET_HEADER_SZ;
	*pu4QueryInfoLen = sizeof(uint32_t);

	return WLAN_STATUS_SUCCESS;
} /* wlanoidQueryMaxFrameSize */

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
uint32_t
wlanoidQueryMaxTotalSize(IN struct ADAPTER *prAdapter,
			 OUT void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
			 OUT uint32_t *pu4QueryInfoLen) {
	DEBUGFUNC("wlanoidQueryMaxTotalSize");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (u4QueryBufferLen < sizeof(uint32_t)) {
		*pu4QueryInfoLen = sizeof(uint32_t);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	*(uint32_t *) pvQueryBuffer = ETHERNET_MAX_PKT_SZ;
	*pu4QueryInfoLen = sizeof(uint32_t);

	return WLAN_STATUS_SUCCESS;
} /* wlanoidQueryMaxTotalSize */

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
uint32_t
wlanoidQueryVendorId(IN struct ADAPTER *prAdapter,
		     OUT void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
		     OUT uint32_t *pu4QueryInfoLen) {
#if DBG
	uint8_t *cp;
#endif
	DEBUGFUNC("wlanoidQueryVendorId");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (u4QueryBufferLen < sizeof(uint32_t)) {
		*pu4QueryInfoLen = sizeof(uint32_t);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	kalMemCopy(pvQueryBuffer, prAdapter->aucMacAddress, 3);
	*((uint8_t *) pvQueryBuffer + 3) = 1;
	*pu4QueryInfoLen = sizeof(uint32_t);

#if DBG
	cp = (uint8_t *) pvQueryBuffer;
	DBGLOG(REQ, LOUD, "Vendor ID=%02x-%02x-%02x-%02x\n", cp[0],
	       cp[1], cp[2], cp[3]);
#endif

	return WLAN_STATUS_SUCCESS;
} /* wlanoidQueryVendorId */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to query the current RSSI value.
 *
 * \param[in] prAdapter Pointer to the Adapter structure.
 * \param[in] pvQueryBuffer Pointer to the buffer that holds the result of the
 *	      query.
 * \param[in] u4QueryBufferLen The length of the query buffer.
 * \param[out] pu4QueryInfoLen If the call is successful, returns the number of
 *   bytes written into the query buffer. If the call failed due to invalid
 *   length of the query buffer, returns the amount of storage needed.
 *
 * \retval WLAN_STATUS_SUCCESS
 * \retval WLAN_STATUS_BUFFER_TOO_SHORT
 * \retval WLAN_STATUS_ADAPTER_NOT_READY
 */
/*----------------------------------------------------------------------------*/
uint32_t
wlanoidQueryRssi(IN struct ADAPTER *prAdapter,
		 OUT void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
		 OUT uint32_t *pu4QueryInfoLen) {

	return wlanQueryRssi(prAdapter,
			pvQueryBuffer,
			u4QueryBufferLen,
			pu4QueryInfoLen,
			g_fgIsOid);
}

uint32_t
wlanQueryRssi(IN struct ADAPTER *prAdapter,
			OUT void *pvQueryBuffer,
			IN uint32_t u4QueryBufferLen,
			OUT uint32_t *pu4QueryInfoLen,
			IN uint8_t fgIsOid) {

	DEBUGFUNC("wlanoidQueryRssi");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (prAdapter->fgIsEnableLpdvt)
		return WLAN_STATUS_NOT_SUPPORTED;

	*pu4QueryInfoLen = sizeof(int32_t);

	/* Check for query buffer length */
	if (u4QueryBufferLen < *pu4QueryInfoLen) {
		DBGLOG(REQ, WARN, "Too short length %u\n",
		       u4QueryBufferLen);
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}

	if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) ==
	    PARAM_MEDIA_STATE_DISCONNECTED) {
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (prAdapter->fgIsLinkQualityValid == TRUE &&
		   (kalGetTimeTick() - prAdapter->rLinkQualityUpdateTime) <=
		   CFG_LINK_QUALITY_VALID_PERIOD) {
		int32_t rRssi;

		/* ranged from (-128 ~ 30) in unit of dBm */
		rRssi = (int32_t) prAdapter->rLinkQuality.cRssi;

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
				   fgIsOid,
				   nicCmdEventQueryLinkQuality,
				   nicOidCmdTimeoutCommon,
				   *pu4QueryInfoLen, pvQueryBuffer,
				   pvQueryBuffer,
				   u4QueryBufferLen);
#else
	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_GET_LINK_QUALITY,
				   FALSE,
				   TRUE,
				   fgIsOid,
				   nicCmdEventQueryLinkQuality,
				   nicOidCmdTimeoutCommon, 0, NULL,
				   pvQueryBuffer,
				   u4QueryBufferLen);

#endif
} /* end of wlanoidQueryRssi() */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to query the current RSSI trigger value.
 *
 * \param[in] prAdapter Pointer to the Adapter structure.
 * \param[in] pvQueryBuffer Pointer to the buffer that holds the result of the
 *	      query.
 * \param[in] u4QueryBufferLen The length of the query buffer.
 * \param[out] pu4QueryInfoLen If the call is successful, returns the number of
 *   bytes written into the query buffer. If the call failed due to invalid
 *   length of the query buffer, returns the amount of storage needed.
 *
 * \retval WLAN_STATUS_SUCCESS
 * \retval WLAN_STATUS_BUFFER_TOO_SHORT
 */
/*----------------------------------------------------------------------------*/
uint32_t
wlanoidQueryRssiTrigger(IN struct ADAPTER *prAdapter,
			OUT void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
			OUT uint32_t *pu4QueryInfoLen) {
	DEBUGFUNC("wlanoidQueryRssiTrigger");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (prAdapter->rWlanInfo.eRssiTriggerType ==
	    ENUM_RSSI_TRIGGER_NONE)
		return WLAN_STATUS_ADAPTER_NOT_READY;

	*pu4QueryInfoLen = sizeof(int32_t);

	/* Check for query buffer length */
	if (u4QueryBufferLen < *pu4QueryInfoLen) {
		DBGLOG(REQ, WARN, "Too short length %u\n",
		       u4QueryBufferLen);
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}

	*(int32_t *) pvQueryBuffer =
		prAdapter->rWlanInfo.rRssiTriggerValue;
	DBGLOG(REQ, INFO, "RSSI trigger: %d dBm\n",
	       *(int32_t *) pvQueryBuffer);

	return WLAN_STATUS_SUCCESS;
} /* wlanoidQueryRssiTrigger */

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
uint32_t
wlanoidSetRssiTrigger(IN struct ADAPTER *prAdapter,
		      IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		      OUT uint32_t *pu4SetInfoLen) {
	int32_t rRssiTriggerValue;

	DEBUGFUNC("wlanoidSetRssiTrigger");

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(int32_t);
	rRssiTriggerValue = *(int32_t *) pvSetBuffer;

	if (rRssiTriggerValue > PARAM_WHQL_RSSI_MAX_DBM
	    || rRssiTriggerValue < PARAM_WHQL_RSSI_MIN_DBM)
		return
			/* Save the RSSI trigger value to the Adapter structure
			 */
			prAdapter->rWlanInfo.rRssiTriggerValue =
							rRssiTriggerValue;

	/* If the RSSI trigger value is equal to the current RSSI value, the
	 * indication triggers immediately. We need to indicate the protocol
	 * that an RSSI status indication event triggers.
	 */
	if (rRssiTriggerValue == (int32_t) (
		    prAdapter->rLinkQuality.cRssi)) {
		prAdapter->rWlanInfo.eRssiTriggerType =
			ENUM_RSSI_TRIGGER_TRIGGERED;

		kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
			     WLAN_STATUS_MEDIA_SPECIFIC_INDICATION,
			     (void *) &prAdapter->rWlanInfo.rRssiTriggerValue,
			     sizeof(int32_t));
	} else if (rRssiTriggerValue < (int32_t) (
			   prAdapter->rLinkQuality.cRssi))
		prAdapter->rWlanInfo.eRssiTriggerType =
			ENUM_RSSI_TRIGGER_GREATER;
	else if (rRssiTriggerValue > (int32_t) (
			 prAdapter->rLinkQuality.cRssi))
		prAdapter->rWlanInfo.eRssiTriggerType =
			ENUM_RSSI_TRIGGER_LESS;

	return WLAN_STATUS_SUCCESS;
} /* wlanoidSetRssiTrigger */

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
uint32_t
wlanoidSetCurrentLookahead(IN struct ADAPTER *prAdapter,
			   IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			   OUT uint32_t *pu4SetInfoLen) {
	DEBUGFUNC("wlanoidSetCurrentLookahead");

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	if (u4SetBufferLen < sizeof(uint32_t)) {
		*pu4SetInfoLen = sizeof(uint32_t);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	*pu4SetInfoLen = sizeof(uint32_t);
	return WLAN_STATUS_SUCCESS;
} /* wlanoidSetCurrentLookahead */

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
uint32_t
wlanoidQueryRcvError(IN struct ADAPTER *prAdapter,
		     IN void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
		     OUT uint32_t *pu4QueryInfoLen) {
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
		*pu4QueryInfoLen = sizeof(uint32_t);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (u4QueryBufferLen < sizeof(uint32_t)
		   || (u4QueryBufferLen > sizeof(uint32_t)
		       && u4QueryBufferLen < sizeof(uint64_t))) {
		*pu4QueryInfoLen = sizeof(uint64_t);
		return WLAN_STATUS_INVALID_LENGTH;
	}
#if CFG_ENABLE_STATISTICS_BUFFERING
	if (IsBufferedStatisticsUsable(prAdapter) == TRUE) {
		/* @FIXME, RX_ERROR_DROP_COUNT/RX_FIFO_FULL_DROP_COUNT is not
		 * calculated
		 */
		if (u4QueryBufferLen == sizeof(uint32_t)) {
			*pu4QueryInfoLen = sizeof(uint32_t);
			*(uint32_t *) pvQueryBuffer = (uint32_t)
				prAdapter->rStatStruct.rFCSErrorCount.QuadPart;
		} else {
			*pu4QueryInfoLen = sizeof(uint64_t);
			*(uint64_t *) pvQueryBuffer = (uint64_t)
				prAdapter->rStatStruct.rFCSErrorCount.QuadPart;
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
				   nicOidCmdTimeoutCommon, 0, NULL,
				   pvQueryBuffer,
				   u4QueryBufferLen);

} /* wlanoidQueryRcvError */

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
uint32_t
wlanoidQueryRcvNoBuffer(IN struct ADAPTER *prAdapter,
			IN void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
			OUT uint32_t *pu4QueryInfoLen) {
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
		*pu4QueryInfoLen = sizeof(uint32_t);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (u4QueryBufferLen < sizeof(uint32_t)
		   || (u4QueryBufferLen > sizeof(uint32_t)
		       && u4QueryBufferLen < sizeof(uint64_t))) {
		*pu4QueryInfoLen = sizeof(uint64_t);
		return WLAN_STATUS_INVALID_LENGTH;
	}
#if CFG_ENABLE_STATISTICS_BUFFERING
	if (IsBufferedStatisticsUsable(prAdapter) == TRUE) {
		if (u4QueryBufferLen == sizeof(uint32_t)) {
			*pu4QueryInfoLen = sizeof(uint32_t);
			*(uint32_t *) pvQueryBuffer = (uint32_t) 0; /* @FIXME */
		} else {
			*pu4QueryInfoLen = sizeof(uint64_t);
			*(uint64_t *) pvQueryBuffer = (uint64_t) 0; /* @FIXME */
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
				   nicOidCmdTimeoutCommon, 0, NULL,
				   pvQueryBuffer,
				   u4QueryBufferLen);

} /* wlanoidQueryRcvNoBuffer */

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
uint32_t
wlanoidQueryRcvCrcError(IN struct ADAPTER *prAdapter,
			IN void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
			OUT uint32_t *pu4QueryInfoLen) {
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
		*pu4QueryInfoLen = sizeof(uint32_t);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (u4QueryBufferLen < sizeof(uint32_t)
		   || (u4QueryBufferLen > sizeof(uint32_t)
		       && u4QueryBufferLen < sizeof(uint64_t))) {
		*pu4QueryInfoLen = sizeof(uint64_t);
		return WLAN_STATUS_INVALID_LENGTH;
	}
#if CFG_ENABLE_STATISTICS_BUFFERING
	if (IsBufferedStatisticsUsable(prAdapter) == TRUE) {
		if (u4QueryBufferLen == sizeof(uint32_t)) {
			*pu4QueryInfoLen = sizeof(uint32_t);
			*(uint32_t *) pvQueryBuffer = (uint32_t)
				prAdapter->rStatStruct.rFCSErrorCount.QuadPart;
		} else {
			*pu4QueryInfoLen = sizeof(uint64_t);
			*(uint64_t *) pvQueryBuffer = (uint64_t)
				prAdapter->rStatStruct.rFCSErrorCount.QuadPart;
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
				   nicOidCmdTimeoutCommon, 0, NULL,
				   pvQueryBuffer,
				   u4QueryBufferLen);

} /* wlanoidQueryRcvCrcError */

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
uint32_t
wlanoidQueryStatistics(IN struct ADAPTER *prAdapter,
		       IN void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
		       OUT uint32_t *pu4QueryInfoLen) {
	struct PARAM_802_11_STATISTICS_STRUCT  rStatistics;

	DEBUGFUNC("wlanoidQueryStatistics");
	DBGLOG(REQ, LOUD, "\n");

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	*pu4QueryInfoLen = sizeof(struct
				  PARAM_802_11_STATISTICS_STRUCT);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in query receive error! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		*pu4QueryInfoLen = sizeof(uint32_t);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (u4QueryBufferLen < sizeof(struct
					     PARAM_802_11_STATISTICS_STRUCT)) {
		DBGLOG(REQ, WARN, "Too short length %u\n",
		       u4QueryBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}
#if CFG_ENABLE_STATISTICS_BUFFERING
	if (IsBufferedStatisticsUsable(prAdapter) == TRUE) {
		struct PARAM_802_11_STATISTICS_STRUCT *prStatistics;

		*pu4QueryInfoLen = sizeof(struct
					  PARAM_802_11_STATISTICS_STRUCT);
		prStatistics = (struct PARAM_802_11_STATISTICS_STRUCT *)
			       pvQueryBuffer;

		prStatistics->u4Length = sizeof(struct
						PARAM_802_11_STATISTICS_STRUCT);
		prStatistics->rTransmittedFragmentCount =
			prAdapter->rStatStruct.rTransmittedFragmentCount;
		prStatistics->rMulticastTransmittedFrameCount =
			prAdapter->rStatStruct.rMulticastTransmittedFrameCount;
		prStatistics->rFailedCount =
			prAdapter->rStatStruct.rFailedCount;
		prStatistics->rRetryCount =
			prAdapter->rStatStruct.rRetryCount;
		prStatistics->rMultipleRetryCount =
			prAdapter->rStatStruct.rMultipleRetryCount;
		prStatistics->rRTSSuccessCount =
			prAdapter->rStatStruct.rRTSSuccessCount;
		prStatistics->rRTSFailureCount =
			prAdapter->rStatStruct.rRTSFailureCount;
		prStatistics->rACKFailureCount =
			prAdapter->rStatStruct.rACKFailureCount;
		prStatistics->rFrameDuplicateCount =
			prAdapter->rStatStruct.rFrameDuplicateCount;
		prStatistics->rReceivedFragmentCount =
			prAdapter->rStatStruct.rReceivedFragmentCount;
		prStatistics->rMulticastReceivedFrameCount =
			prAdapter->rStatStruct.rMulticastReceivedFrameCount;
		prStatistics->rFCSErrorCount =
			prAdapter->rStatStruct.rFCSErrorCount;
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
				sizeof(struct PARAM_802_11_STATISTICS_STRUCT),
				(uint8_t *)&rStatistics,
				pvQueryBuffer, u4QueryBufferLen);

} /* wlanoidQueryStatistics */

uint32_t
wlanoidQueryBugReport(IN struct ADAPTER *prAdapter,
		      IN void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
		      OUT uint32_t *pu4QueryInfoLen) {
	DEBUGFUNC("wlanoidQueryBugReport");

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	*pu4QueryInfoLen = sizeof(struct _EVENT_BUG_REPORT_T);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(OID, WARN,
		       "Fail in query receive error! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		*pu4QueryInfoLen = sizeof(uint32_t);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (u4QueryBufferLen < sizeof(struct
					     _EVENT_BUG_REPORT_T)) {
		DBGLOG(OID, WARN, "Too short length %u\n",
		       u4QueryBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_GET_BUG_REPORT,
				   FALSE,
				   TRUE,
				   g_fgIsOid,
				   nicCmdEventQueryBugReport,
				   nicOidCmdTimeoutCommon,
				   0, NULL, pvQueryBuffer, u4QueryBufferLen);
}				/* wlanoidQueryBugReport */

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
uint32_t
wlanoidQueryMediaStreamMode(IN struct ADAPTER *prAdapter,
			    IN void *pvQueryBuffer,
			    IN uint32_t u4QueryBufferLen,
			    OUT uint32_t *pu4QueryInfoLen) {
	DEBUGFUNC("wlanoidQueryMediaStreamMode");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(enum ENUM_MEDIA_STREAM_MODE);

	if (u4QueryBufferLen < *pu4QueryInfoLen) {
		DBGLOG(REQ, WARN, "Invalid length %u\n", u4QueryBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	*(enum ENUM_MEDIA_STREAM_MODE *) pvQueryBuffer =
		prAdapter->rWlanInfo.eLinkAttr.ucMediaStreamMode == 0 ?
		ENUM_MEDIA_STREAM_OFF : ENUM_MEDIA_STREAM_ON;

	return WLAN_STATUS_SUCCESS;

} /* wlanoidQueryMediaStreamMode */

/*----------------------------------------------------------------------------*/
/*! \brief  This routine is called to enter media streaming mode or exit media
 *          streaming mode
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
uint32_t
wlanoidSetMediaStreamMode(IN struct ADAPTER *prAdapter,
			  IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			  OUT uint32_t *pu4SetInfoLen) {
	enum ENUM_MEDIA_STREAM_MODE eStreamMode;

	DEBUGFUNC("wlanoidSetMediaStreamMode");

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	if (u4SetBufferLen < sizeof(enum ENUM_MEDIA_STREAM_MODE)) {
		DBGLOG(REQ, WARN, "Invalid length %u\n", u4SetBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	*pu4SetInfoLen = sizeof(enum ENUM_MEDIA_STREAM_MODE);

	eStreamMode = *(enum ENUM_MEDIA_STREAM_MODE *) pvSetBuffer;

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
				sizeof(struct CMD_LINK_ATTRIB),
				(uint8_t *) &(prAdapter->rWlanInfo.eLinkAttr),
				pvSetBuffer, u4SetBufferLen);
} /* wlanoidSetMediaStreamMode */

/*----------------------------------------------------------------------------*/
/*! \brief  This routine is called to query the permanent MAC address of the
 *	    NIC.
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
uint32_t
wlanoidQueryPermanentAddr(IN struct ADAPTER *prAdapter,
			  IN void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
			  OUT uint32_t *pu4QueryInfoLen) {
	DEBUGFUNC("wlanoidQueryPermanentAddr");
	DBGLOG(INIT, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (u4QueryBufferLen < MAC_ADDR_LEN)
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	COPY_MAC_ADDR(pvQueryBuffer,
		      prAdapter->rWifiVar.aucPermanentAddress);
	*pu4QueryInfoLen = MAC_ADDR_LEN;

	return WLAN_STATUS_SUCCESS;
}				/* wlanoidQueryPermanentAddr */

/*----------------------------------------------------------------------------*/
/*! \brief  This routine is called to query the MAC address the NIC is
 *          currently using.
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
uint32_t
wlanoidQueryCurrentAddr(IN struct ADAPTER *prAdapter,
			IN void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
			OUT uint32_t *pu4QueryInfoLen) {
	DEBUGFUNC("wlanoidQueryCurrentAddr");
	DBGLOG(INIT, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (u4QueryBufferLen < MAC_ADDR_LEN)
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	COPY_MAC_ADDR(pvQueryBuffer,
		      prAdapter->rWifiVar.aucMacAddress);
	*pu4QueryInfoLen = MAC_ADDR_LEN;

	return WLAN_STATUS_SUCCESS;
} /* wlanoidQueryCurrentAddr */

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
uint32_t
wlanoidQueryLinkSpeed(IN struct ADAPTER *prAdapter,
		      IN void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
		      OUT uint32_t *pu4QueryInfoLen) {
	DEBUGFUNC("wlanoidQueryLinkSpeed");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (prAdapter->fgIsEnableLpdvt)
		return WLAN_STATUS_NOT_SUPPORTED;

	*pu4QueryInfoLen = sizeof(uint32_t);

	if (u4QueryBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) !=
	    PARAM_MEDIA_STATE_CONNECTED) {
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (prAdapter->fgIsLinkRateValid == TRUE &&
		   (kalGetTimeTick() - prAdapter->rLinkRateUpdateTime) <=
		   CFG_LINK_QUALITY_VALID_PERIOD) {
		*(uint32_t *) pvQueryBuffer =
			prAdapter->rLinkQuality.u2LinkSpeed *
			5000;	/* change to unit of 100bps */
		return WLAN_STATUS_SUCCESS;
	} else {
		return wlanSendSetQueryCmd(prAdapter,
					   CMD_ID_GET_LINK_QUALITY,
					   FALSE,
					   TRUE,
					   g_fgIsOid,
					   nicCmdEventQueryLinkSpeed,
					   nicOidCmdTimeoutCommon, 0, NULL,
					   pvQueryBuffer, u4QueryBufferLen);
	}
} /* end of wlanoidQueryLinkSpeed() */

#if CFG_SUPPORT_QA_TOOL
#if CFG_SUPPORT_BUFFER_MODE
uint32_t
wlanoidSetEfusBufferMode(IN struct ADAPTER *prAdapter,
			 IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			 OUT uint32_t *pu4SetInfoLen) {
	struct PARAM_CUSTOM_EFUSE_BUFFER_MODE
		*prSetEfuseBufModeInfo;
	struct CMD_EFUSE_BUFFER_MODE *prCmdSetEfuseBufModeInfo =
			NULL;
	PFN_CMD_DONE_HANDLER pfCmdDoneHandler;
	uint32_t u4EfuseContentSize, u4QueryInfoLen;
	u_int8_t fgSetQuery, fgNeedResp;
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidSetEfusBufferMode");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	/* get the buffer mode info */
	prSetEfuseBufModeInfo =
			(struct PARAM_CUSTOM_EFUSE_BUFFER_MODE *) pvSetBuffer;

	/* copy command header */
	prCmdSetEfuseBufModeInfo = (struct CMD_EFUSE_BUFFER_MODE *)
		kalMemAlloc(sizeof(struct CMD_EFUSE_BUFFER_MODE),
			    VIR_MEM_TYPE);
	if (prCmdSetEfuseBufModeInfo == NULL)
		return WLAN_STATUS_FAILURE;
	kalMemZero(prCmdSetEfuseBufModeInfo,
		   sizeof(struct CMD_EFUSE_BUFFER_MODE));
	prCmdSetEfuseBufModeInfo->ucSourceMode =
		prSetEfuseBufModeInfo->ucSourceMode;
	prCmdSetEfuseBufModeInfo->ucCount      =
		prSetEfuseBufModeInfo->ucCount;
	prCmdSetEfuseBufModeInfo->ucCmdType    =
		prSetEfuseBufModeInfo->ucCmdType;
	prCmdSetEfuseBufModeInfo->ucReserved   =
		prSetEfuseBufModeInfo->ucReserved;

	/* decide content size and SetQuery / NeedResp flag */
	if (prAdapter->fgIsSupportBufferBinSize16Byte == TRUE) {
		u4EfuseContentSize  = sizeof(struct BIN_CONTENT) *
				      EFUSE_CONTENT_SIZE;
		pfCmdDoneHandler = nicCmdEventSetCommon;
		fgSetQuery = TRUE;
		fgNeedResp = FALSE;
	} else {
#if (CFG_FW_Report_Efuse_Address == 1)
		u4EfuseContentSize = (prAdapter->u4EfuseEndAddress) -
				     (prAdapter->u4EfuseStartAddress) + 1;
#else
		u4EfuseContentSize = EFUSE_CONTENT_BUFFER_SIZE;
#endif
		pfCmdDoneHandler = NULL;
		fgSetQuery = FALSE;
		fgNeedResp = TRUE;
	}

	u4QueryInfoLen = OFFSET_OF(struct CMD_EFUSE_BUFFER_MODE,
				   aBinContent) + u4EfuseContentSize;

	if (u4SetBufferLen < u4QueryInfoLen) {
		kalMemFree(prCmdSetEfuseBufModeInfo, VIR_MEM_TYPE,
			   sizeof(struct CMD_EFUSE_BUFFER_MODE));
		return WLAN_STATUS_INVALID_LENGTH;
	}

	*pu4SetInfoLen = u4QueryInfoLen;
	kalMemCopy(prCmdSetEfuseBufModeInfo->aBinContent,
		   prSetEfuseBufModeInfo->aBinContent,
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
				(uint8_t *) (prCmdSetEfuseBufModeInfo),
				pvSetBuffer, u4SetBufferLen);

	kalMemFree(prCmdSetEfuseBufModeInfo, VIR_MEM_TYPE,
		   sizeof(struct CMD_EFUSE_BUFFER_MODE));

	return rWlanStatus;
}

uint32_t
wlanoidConnacSetEfusBufferMode(IN struct ADAPTER *prAdapter,
			       IN void *pvSetBuffer,
			       IN uint32_t u4SetBufferLen,
			       OUT uint32_t *pu4SetInfoLen) {
	struct PARAM_CUSTOM_EFUSE_BUFFER_MODE_CONNAC_T
		*prSetEfuseBufModeInfo;
	struct CMD_EFUSE_BUFFER_MODE_CONNAC_T
		*prCmdSetEfuseBufModeInfo = NULL;
	uint32_t u4EfuseContentSize, u4QueryInfoLen;
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidSetEfusBufferMode");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	DBGLOG(OID, INFO, "u4SetBufferLen = %d\n", u4SetBufferLen);
	/* get the buffer mode info */
	prSetEfuseBufModeInfo =
		(struct PARAM_CUSTOM_EFUSE_BUFFER_MODE_CONNAC_T *) pvSetBuffer;

	/* copy command header */
	prCmdSetEfuseBufModeInfo = (struct CMD_EFUSE_BUFFER_MODE_CONNAC_T *)
		kalMemAlloc(sizeof(struct CMD_EFUSE_BUFFER_MODE_CONNAC_T),
			    VIR_MEM_TYPE);
	if (prCmdSetEfuseBufModeInfo == NULL)
		return WLAN_STATUS_FAILURE;
	kalMemZero(prCmdSetEfuseBufModeInfo,
		   sizeof(struct CMD_EFUSE_BUFFER_MODE_CONNAC_T));
	prCmdSetEfuseBufModeInfo->ucSourceMode =
		prSetEfuseBufModeInfo->ucSourceMode;
	prCmdSetEfuseBufModeInfo->ucContentFormat =
		prSetEfuseBufModeInfo->ucContentFormat;
	prCmdSetEfuseBufModeInfo->u2Count =
		prSetEfuseBufModeInfo->u2Count;

	u4EfuseContentSize = prCmdSetEfuseBufModeInfo->u2Count;

	u4QueryInfoLen = OFFSET_OF(struct
				   CMD_EFUSE_BUFFER_MODE_CONNAC_T,
				   aBinContent) + u4EfuseContentSize;

	if (u4SetBufferLen < u4QueryInfoLen) {
		kalMemFree(prCmdSetEfuseBufModeInfo, VIR_MEM_TYPE,
			   sizeof(struct CMD_EFUSE_BUFFER_MODE_CONNAC_T));
		return WLAN_STATUS_INVALID_LENGTH;
	}

	*pu4SetInfoLen = u4QueryInfoLen;
	kalMemCopy(prCmdSetEfuseBufModeInfo->aBinContent,
		   prSetEfuseBufModeInfo->aBinContent,
		   u4EfuseContentSize);

	rWlanStatus = wlanSendSetQueryExtCmd(prAdapter,
				CMD_ID_LAYER_0_EXT_MAGIC_NUM,
				EXT_CMD_ID_EFUSE_BUFFER_MODE,
				FALSE,
				TRUE,
				g_fgIsOid,
				NULL,
				nicOidCmdTimeoutCommon,
				u4QueryInfoLen,
				(uint8_t *) (prCmdSetEfuseBufModeInfo),
				pvSetBuffer, u4SetBufferLen);

	kalMemFree(prCmdSetEfuseBufModeInfo, VIR_MEM_TYPE,
		   sizeof(struct CMD_EFUSE_BUFFER_MODE_CONNAC_T));

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
uint32_t
wlanoidQueryProcessAccessEfuseRead(IN struct ADAPTER *prAdapter,
				   IN void *pvSetBuffer,
				   IN uint32_t u4SetBufferLen,
				   OUT uint32_t *pu4SetInfoLen) {
	struct PARAM_CUSTOM_ACCESS_EFUSE *prSetAccessEfuseInfo;
	struct CMD_ACCESS_EFUSE rCmdSetAccessEfuse;
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidQueryProcessAccessEfuseRead");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(struct PARAM_CUSTOM_ACCESS_EFUSE);

	if (u4SetBufferLen < sizeof(struct
				    PARAM_CUSTOM_ACCESS_EFUSE))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prSetAccessEfuseInfo = (struct PARAM_CUSTOM_ACCESS_EFUSE *)
			       pvSetBuffer;

	kalMemSet(&rCmdSetAccessEfuse, 0,
		  sizeof(struct CMD_ACCESS_EFUSE));

	rCmdSetAccessEfuse.u4Address =
		prSetAccessEfuseInfo->u4Address;
	rCmdSetAccessEfuse.u4Valid = prSetAccessEfuseInfo->u4Valid;


	DBGLOG(INIT, INFO,
	       "MT6632 : wlanoidQueryProcessAccessEfuseRead, address=%d\n",
	       rCmdSetAccessEfuse.u4Address);

	kalMemCopy(rCmdSetAccessEfuse.aucData,
		   prSetAccessEfuseInfo->aucData,
		   sizeof(uint8_t) * 16);

	rWlanStatus = wlanSendSetQueryExtCmd(prAdapter,
			     CMD_ID_LAYER_0_EXT_MAGIC_NUM,
			     EXT_CMD_ID_EFUSE_ACCESS,
			     FALSE, /* Query Bit: True->write False->read */
			     TRUE,
			     g_fgIsOid,
			     NULL, /* No Tx done function wait until fw ack */
			     nicOidCmdTimeoutCommon,
			     sizeof(struct CMD_ACCESS_EFUSE),
			     (uint8_t *) (&rCmdSetAccessEfuse), pvSetBuffer,
			     u4SetBufferLen);

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
uint32_t
wlanoidQueryProcessAccessEfuseWrite(IN struct ADAPTER *prAdapter,
				    IN void *pvSetBuffer,
				    IN uint32_t u4SetBufferLen,
				    OUT uint32_t *pu4SetInfoLen) {
	struct PARAM_CUSTOM_ACCESS_EFUSE *prSetAccessEfuseInfo;
	struct CMD_ACCESS_EFUSE rCmdSetAccessEfuse;
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidQueryProcessAccessEfuseWrite");
	DBGLOG(INIT, INFO,
	       "MT6632 : wlanoidQueryProcessAccessEfuseWrite\n");


	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(struct PARAM_CUSTOM_ACCESS_EFUSE);

	if (u4SetBufferLen < sizeof(struct
				    PARAM_CUSTOM_ACCESS_EFUSE))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prSetAccessEfuseInfo = (struct PARAM_CUSTOM_ACCESS_EFUSE *)
			       pvSetBuffer;

	kalMemSet(&rCmdSetAccessEfuse, 0,
		  sizeof(struct CMD_ACCESS_EFUSE));

	rCmdSetAccessEfuse.u4Address =
		prSetAccessEfuseInfo->u4Address;
	rCmdSetAccessEfuse.u4Valid = prSetAccessEfuseInfo->u4Valid;

	DBGLOG(INIT, INFO,
	       "MT6632 : wlanoidQueryProcessAccessEfuseWrite, address=%d\n",
	       rCmdSetAccessEfuse.u4Address);


	kalMemCopy(rCmdSetAccessEfuse.aucData,
		   prSetAccessEfuseInfo->aucData,
		   sizeof(uint8_t) * 16);

	rWlanStatus = wlanSendSetQueryExtCmd(prAdapter,
			CMD_ID_LAYER_0_EXT_MAGIC_NUM,
			EXT_CMD_ID_EFUSE_ACCESS,
			TRUE, /* Query Bit: True->write False->read*/
			TRUE,
			g_fgIsOid,
			NULL, /* No Tx done function wait until fw ack */
			nicOidCmdTimeoutCommon,
			sizeof(struct CMD_ACCESS_EFUSE),
			(uint8_t *) (&rCmdSetAccessEfuse), pvSetBuffer,
			u4SetBufferLen);

	return rWlanStatus;
}




uint32_t
wlanoidQueryEfuseFreeBlock(IN struct ADAPTER *prAdapter,
			   IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			   OUT uint32_t *pu4SetInfoLen) {
	struct PARAM_CUSTOM_EFUSE_FREE_BLOCK
		*prGetEfuseFreeBlockInfo;
	struct CMD_EFUSE_FREE_BLOCK rCmdGetEfuseFreeBlock;
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidQueryEfuseFreeBlock");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(struct
				PARAM_CUSTOM_EFUSE_FREE_BLOCK);

	if (u4SetBufferLen < sizeof(struct
				    PARAM_CUSTOM_EFUSE_FREE_BLOCK))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prGetEfuseFreeBlockInfo = (struct
				   PARAM_CUSTOM_EFUSE_FREE_BLOCK *) pvSetBuffer;

	kalMemSet(&rCmdGetEfuseFreeBlock, 0,
		  sizeof(struct CMD_EFUSE_FREE_BLOCK));

	rCmdGetEfuseFreeBlock.ucVersion = 1;/*1:new version, 0:old version*/
	rCmdGetEfuseFreeBlock.ucDieIndex = 0;/*0:D Die,  1: A die */


	rWlanStatus = wlanSendSetQueryExtCmd(prAdapter,
			CMD_ID_LAYER_0_EXT_MAGIC_NUM,
			EXT_CMD_ID_EFUSE_FREE_BLOCK,
			FALSE, /* Query Bit: True->write False->read */
			TRUE,
			g_fgIsOid,
			NULL, /* No Tx done function wait until fw ack */
			nicOidCmdTimeoutCommon,
			sizeof(struct CMD_EFUSE_FREE_BLOCK),
			(uint8_t *) (&rCmdGetEfuseFreeBlock), pvSetBuffer,
			u4SetBufferLen);

	return rWlanStatus;
}

uint32_t
wlanoidQueryGetTxPower(IN struct ADAPTER *prAdapter,
		       IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		       OUT uint32_t *pu4SetInfoLen) {
	struct PARAM_CUSTOM_GET_TX_POWER *prGetTxPowerInfo;
	struct CMD_GET_TX_POWER rCmdGetTxPower;
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidQueryGetTxPower");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(struct PARAM_CUSTOM_GET_TX_POWER *);

	if (u4SetBufferLen < sizeof(struct PARAM_CUSTOM_GET_TX_POWER
				    *))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prGetTxPowerInfo = (struct PARAM_CUSTOM_GET_TX_POWER *)
			   pvSetBuffer;

	kalMemSet(&rCmdGetTxPower, 0,
		  sizeof(struct CMD_GET_TX_POWER));

	rCmdGetTxPower.ucTxPwrType = EXT_EVENT_TARGET_TX_POWER;
	rCmdGetTxPower.ucCenterChannel =
		prGetTxPowerInfo->ucCenterChannel;
	rCmdGetTxPower.ucDbdcIdx = prGetTxPowerInfo->ucDbdcIdx;
	rCmdGetTxPower.ucBand = prGetTxPowerInfo->ucBand;


	rWlanStatus = wlanSendSetQueryExtCmd(prAdapter,
			CMD_ID_LAYER_0_EXT_MAGIC_NUM,
			EXT_CMD_ID_GET_TX_POWER,
			FALSE, /* Query Bit: True->write False->read*/
			TRUE,
			g_fgIsOid,
			NULL, /* No Tx done function wait until fw ack */
			nicOidCmdTimeoutCommon,
			sizeof(struct CMD_GET_TX_POWER),
			(uint8_t *) (&rCmdGetTxPower),
			pvSetBuffer, u4SetBufferLen);

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
uint32_t
wlanoidQueryRxStatistics(IN struct ADAPTER *prAdapter,
			 IN void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
			 OUT uint32_t *pu4QueryInfoLen) {
	struct PARAM_CUSTOM_ACCESS_RX_STAT *prRxStatistics;
	struct CMD_ACCESS_RX_STAT *prCmdAccessRxStat;
	struct CMD_ACCESS_RX_STAT rCmdAccessRxStat;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	/*	UINT_32 u4MemSize = PARAM_MEM_DUMP_MAX_SIZE; */
	uint32_t u4SeqNum = 0;
	uint32_t u4TotalNum = 0;

	prCmdAccessRxStat = &rCmdAccessRxStat;

	DEBUGFUNC("wlanoidQueryRxStatistics");
	DBGLOG(INIT, LOUD, "\n");

	DBGLOG(INIT, ERROR, "MT6632 : wlanoidQueryRxStatistics\n");

	prRxStatistics = (struct PARAM_CUSTOM_ACCESS_RX_STAT *)
			 pvQueryBuffer;

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
			      sizeof(struct CMD_ACCESS_RX_STAT),
			      (uint8_t *) prCmdAccessRxStat, pvQueryBuffer,
			      u4QueryBufferLen);
	} while (FALSE);

	return rStatus;
}

#if CFG_SUPPORT_TX_BF

uint32_t
wlanoidStaRecUpdate(IN struct ADAPTER *prAdapter,
		    IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		    OUT uint32_t *pu4SetInfoLen) {
	struct CMD_STAREC_UPDATE *prStaRecUpdateInfo;
	struct STAREC_COMMON *prStaRecCmm;
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidStaRecUpdate");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(struct STAREC_COMMON);
	if (u4SetBufferLen < sizeof(struct STAREC_COMMON))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prStaRecUpdateInfo =
		(struct CMD_STAREC_UPDATE *) cnmMemAlloc(prAdapter,
				RAM_TYPE_MSG, (CMD_STAREC_UPDATE_HDR_SIZE +
					       u4SetBufferLen));
	if (!prStaRecUpdateInfo) {
		DBGLOG(INIT, ERROR,
		       "Allocate P_CMD_DEV_INFO_UPDATE_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}

	/* fix me: configurable ucBssIndex */
	prStaRecCmm = (struct STAREC_COMMON *) pvSetBuffer;
	prStaRecUpdateInfo->ucBssIndex = 0;
	prStaRecUpdateInfo->ucWlanIdx = prStaRecCmm->u2Reserve1;
	prStaRecUpdateInfo->u2TotalElementNum = 1;
	kalMemCopy(prStaRecUpdateInfo->aucBuffer, pvSetBuffer,
		   u4SetBufferLen);

	rWlanStatus = wlanSendSetQueryExtCmd(prAdapter,
			     CMD_ID_LAYER_0_EXT_MAGIC_NUM,
			     EXT_CMD_ID_STAREC_UPDATE,
			     TRUE,
			     FALSE,
			     g_fgIsOid,
			     nicCmdEventSetCommon,
			     nicOidCmdTimeoutCommon,
			     (CMD_STAREC_UPDATE_HDR_SIZE + u4SetBufferLen),
			     (uint8_t *) prStaRecUpdateInfo, NULL, 0);

	cnmMemFree(prAdapter, prStaRecUpdateInfo);

	return rWlanStatus;
}

uint32_t
wlanoidStaRecBFUpdate(IN struct ADAPTER *prAdapter,
		      IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		      OUT uint32_t *pu4SetInfoLen) {
	struct CMD_STAREC_UPDATE *prStaRecUpdateInfo;
	struct CMD_STAREC_BF *prStaRecBF;
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidStaRecBFUpdate");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(struct CMD_STAREC_BF);
	if (u4SetBufferLen < sizeof(struct CMD_STAREC_BF))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prStaRecUpdateInfo =
		(struct CMD_STAREC_UPDATE *) cnmMemAlloc(prAdapter,
				RAM_TYPE_MSG, (CMD_STAREC_UPDATE_HDR_SIZE +
					       u4SetBufferLen));
	if (!prStaRecUpdateInfo) {
		DBGLOG(INIT, ERROR,
		       "Allocate P_CMD_DEV_INFO_UPDATE_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}

	/* fix me: configurable ucBssIndex */
	prStaRecBF = (struct CMD_STAREC_BF *) pvSetBuffer;
	prStaRecUpdateInfo->ucBssIndex = prStaRecBF->ucReserved[0];
	prStaRecUpdateInfo->ucWlanIdx = prStaRecBF->ucReserved[1];
	prStaRecUpdateInfo->u2TotalElementNum = 1;
	kalMemCopy(prStaRecUpdateInfo->aucBuffer, pvSetBuffer,
		   u4SetBufferLen);

	rWlanStatus = wlanSendSetQueryExtCmd(prAdapter,
			     CMD_ID_LAYER_0_EXT_MAGIC_NUM,
			     EXT_CMD_ID_STAREC_UPDATE,
			     TRUE,
			     FALSE,
			     g_fgIsOid,
			     nicCmdEventSetCommon,
			     nicOidCmdTimeoutCommon,
			     (CMD_STAREC_UPDATE_HDR_SIZE + u4SetBufferLen),
			     (uint8_t *) prStaRecUpdateInfo, NULL, 0);

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
uint32_t
wlanSendSetQueryExtCmd(IN struct ADAPTER *prAdapter,
		       uint8_t ucCID,
		       uint8_t ucExtCID,
		       u_int8_t fgSetQuery,
		       u_int8_t fgNeedResp,
		       u_int8_t fgIsOid,
		       PFN_CMD_DONE_HANDLER pfCmdDoneHandler,
		       PFN_CMD_TIMEOUT_HANDLER pfCmdTimeoutHandler,
		       uint32_t u4SetQueryInfoLen,
		       uint8_t *pucInfoBuffer, OUT void *pvSetQueryBuffer,
		       IN uint32_t u4SetQueryBufferLen) {
	struct GLUE_INFO *prGlueInfo;
	struct CMD_INFO *prCmdInfo;
	struct WIFI_CMD *prWifiCmd;
	uint8_t ucCmdSeqNum;

	prGlueInfo = prAdapter->prGlueInfo;
	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter,
					  (CMD_HDR_SIZE + u4SetQueryInfoLen));

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
	prCmdInfo->u2InfoBufLen = (uint16_t) (CMD_HDR_SIZE +
					      u4SetQueryInfoLen);
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
	prWifiCmd->ucExtenCID = ucExtCID;
	prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
	prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;

	if (u4SetQueryInfoLen > 0 && pucInfoBuffer != NULL)
		kalMemCopy(prWifiCmd->aucBuffer, pucInfoBuffer,
			   u4SetQueryInfoLen);
	/* insert into prCmdQueue */
	kalEnqueueCommand(prGlueInfo,
			  (struct QUE_ENTRY *) prCmdInfo);

	/* wakeup txServiceThread later */
	GLUE_SET_EVENT(prGlueInfo);
	return WLAN_STATUS_PENDING;
}

uint32_t
wlanoidBssInfoBasic(IN struct ADAPTER *prAdapter,
		    IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		    OUT uint32_t *pu4SetInfoLen) {
	struct CMD_BSS_INFO_UPDATE *prBssInfoUpdateBasic;
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidManualAssoc");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(struct BSSINFO_BASIC);
	if (u4SetBufferLen < sizeof(struct BSSINFO_BASIC))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prBssInfoUpdateBasic = cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
			   (CMD_BSSINFO_UPDATE_HDR_SIZE + u4SetBufferLen));
	if (!prBssInfoUpdateBasic) {
		DBGLOG(INIT, ERROR,
		       "Allocate P_CMD_DEV_INFO_UPDATE_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}

	/* fix me: configurable ucBssIndex */
	prBssInfoUpdateBasic->ucBssIndex = 0;
	prBssInfoUpdateBasic->u2TotalElementNum = 1;
	kalMemCopy(prBssInfoUpdateBasic->aucBuffer, pvSetBuffer,
		   u4SetBufferLen);

	rWlanStatus = wlanSendSetQueryExtCmd(prAdapter,
			     CMD_ID_LAYER_0_EXT_MAGIC_NUM,
			     EXT_CMD_ID_BSSINFO_UPDATE,
			     TRUE,
			     FALSE,
			     g_fgIsOid,
			     nicCmdEventSetCommon,
			     nicOidCmdTimeoutCommon,
			     (CMD_BSSINFO_UPDATE_HDR_SIZE + u4SetBufferLen),
			     (uint8_t *) prBssInfoUpdateBasic, NULL, 0);

	cnmMemFree(prAdapter, prBssInfoUpdateBasic);

	return rWlanStatus;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to do Coex Isolation Detection.

* \param[in] pvAdapter Pointer to the Adapter structure.
* \param[out] pvQueryBuf A pointer to the buffer that holds the result of
*                                   the query.
* \param[in] u4QueryBufLen The length of the query buffer.
* \param[out] pu4QueryInfoLen If the call is successful, returns the number of
*                             bytes written into the query buffer. If the call
*                             failed due to invalid length of the query buffer,
*                            eturns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
uint32_t
wlanoidQueryCoexIso(IN struct ADAPTER *prAdapter,
		    IN void *pvQueryBuffer,
		    IN uint32_t u4QueryBufferLen,
		    OUT uint32_t *pu4QueryInfoLen)
{
	struct PARAM_COEX_HANDLER *prParaCoexHandler;
	struct PARAM_COEX_ISO_DETECT *prParaCoexIsoDetect;
	struct CMD_COEX_HANDLER rCmdCoexHandler;
	struct CMD_COEX_ISO_DETECT rCmdCoexIsoDetect;

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);

	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(struct PARAM_COEX_HANDLER);

	if (u4QueryBufferLen < sizeof(struct PARAM_COEX_HANDLER))
		return WLAN_STATUS_INVALID_LENGTH;

	prParaCoexHandler =
	(struct PARAM_COEX_HANDLER *) pvQueryBuffer;
	prParaCoexIsoDetect =
	(struct PARAM_COEX_ISO_DETECT *) &prParaCoexHandler->aucBuffer[0];

	rCmdCoexIsoDetect.u4Channel = prParaCoexIsoDetect->u4Channel;
	rCmdCoexIsoDetect.u4IsoPath = prParaCoexIsoDetect->u4IsoPath;
	rCmdCoexIsoDetect.u4Isolation = prParaCoexIsoDetect->u4Isolation;

	rCmdCoexHandler.u4SubCmd = prParaCoexHandler->u4SubCmd;

	/* Copy Memory */
	kalMemCopy(rCmdCoexHandler.aucBuffer,
			&rCmdCoexIsoDetect,
			sizeof(rCmdCoexIsoDetect));

	return wlanSendSetQueryCmd(prAdapter,
				CMD_ID_COEX_CTRL,
				FALSE,
				TRUE,
				TRUE,
				nicCmdEventQueryCoexIso,
				nicOidCmdTimeoutCommon,
				sizeof(struct CMD_COEX_HANDLER),
				(unsigned char *) &rCmdCoexHandler,
				pvQueryBuffer,
				u4QueryBufferLen);

}

uint32_t
wlanoidQueryCoexGetInfo(IN struct ADAPTER *prAdapter,
		    IN void *pvQueryBuffer,
		    IN uint32_t u4QueryBufferLen,
		    OUT uint32_t *pu4QueryInfoLen)
{
	struct PARAM_COEX_HANDLER *prParaCoexHandler;
	struct PARAM_COEX_GET_INFO *prParaCoexGetInfo;
	struct CMD_COEX_HANDLER rCmdCoexHandler;
	struct CMD_COEX_GET_INFO rCmdCoexGetInfo;

	DBGLOG(INIT, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);

	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(struct PARAM_COEX_HANDLER);

	if (u4QueryBufferLen < sizeof(struct PARAM_COEX_HANDLER))
		return WLAN_STATUS_INVALID_LENGTH;

	prParaCoexHandler =
		(struct PARAM_COEX_HANDLER *)pvQueryBuffer;
	prParaCoexGetInfo =
		(struct PARAM_COEX_GET_INFO *)&prParaCoexHandler->aucBuffer[0];

	kalMemZero(rCmdCoexGetInfo.ucCoexInfo,
		sizeof(rCmdCoexGetInfo.ucCoexInfo));

	rCmdCoexHandler.u4SubCmd = prParaCoexHandler->u4SubCmd;

	/* Copy Memory */
	kalMemCopy(rCmdCoexHandler.aucBuffer,
		&rCmdCoexGetInfo,
		sizeof(struct CMD_COEX_GET_INFO));

	return wlanSendSetQueryCmd(prAdapter,
				CMD_ID_COEX_CTRL,
				FALSE,
				TRUE,
				g_fgIsOid,
				nicCmdEventQueryCoexGetInfo,
				nicOidCmdTimeoutCommon,
				sizeof(struct CMD_COEX_HANDLER),
				(unsigned char *) &rCmdCoexHandler,
				pvQueryBuffer,
				u4QueryBufferLen);
}

uint32_t
wlanoidDevInfoActive(IN struct ADAPTER *prAdapter,
		     IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		     OUT uint32_t *pu4SetInfoLen) {
	struct CMD_DEV_INFO_UPDATE *prDevInfoUpdateActive;
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidManualAssoc");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(struct CMD_DEVINFO_ACTIVE);
	if (u4SetBufferLen < sizeof(struct CMD_DEVINFO_ACTIVE))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prDevInfoUpdateActive = cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
			    (CMD_DEVINFO_UPDATE_HDR_SIZE + u4SetBufferLen));
	if (!prDevInfoUpdateActive) {
		DBGLOG(INIT, ERROR,
		       "Allocate P_CMD_DEV_INFO_UPDATE_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}

	/* fix me: configurable ucOwnMacIdx */
	prDevInfoUpdateActive->ucOwnMacIdx = 0;
	prDevInfoUpdateActive->ucAppendCmdTLV = 0;
	prDevInfoUpdateActive->u2TotalElementNum = 1;
	kalMemCopy(prDevInfoUpdateActive->aucBuffer, pvSetBuffer,
		   u4SetBufferLen);

	rWlanStatus = wlanSendSetQueryExtCmd(prAdapter,
			     CMD_ID_LAYER_0_EXT_MAGIC_NUM,
			     EXT_CMD_ID_DEVINFO_UPDATE,
			     TRUE,
			     FALSE,
			     g_fgIsOid,
			     nicCmdEventSetCommon,
			     nicOidCmdTimeoutCommon,
			     (CMD_DEVINFO_UPDATE_HDR_SIZE + u4SetBufferLen),
			     (uint8_t *) prDevInfoUpdateActive, NULL, 0);

	cnmMemFree(prAdapter, prDevInfoUpdateActive);

	return rWlanStatus;
}

uint32_t
wlanoidManualAssoc(IN struct ADAPTER *prAdapter,
		   IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		   OUT uint32_t *pu4SetInfoLen) {
	struct CMD_STAREC_UPDATE *prStaRecManualAssoc;
	struct CMD_MANUAL_ASSOC_STRUCT *prManualAssoc;
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidManualAssoc");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(struct CMD_STAREC_UPDATE);
	if (u4SetBufferLen < sizeof(struct CMD_STAREC_UPDATE))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prStaRecManualAssoc = cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
				(CMD_STAREC_UPDATE_HDR_SIZE + u4SetBufferLen));
	if (!prStaRecManualAssoc) {
		DBGLOG(INIT, ERROR,
		       "Allocate P_CMD_STAREC_UPDATE_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}

	prManualAssoc = (struct CMD_MANUAL_ASSOC_STRUCT *)
			pvSetBuffer;
	prStaRecManualAssoc->ucWlanIdx = prManualAssoc->ucWtbl;
	prStaRecManualAssoc->ucBssIndex = prManualAssoc->ucOwnmac;
	prStaRecManualAssoc->u2TotalElementNum = 1;
	kalMemCopy(prStaRecManualAssoc->aucBuffer, pvSetBuffer,
		   u4SetBufferLen);

	rWlanStatus = wlanSendSetQueryExtCmd(prAdapter,
			     CMD_ID_LAYER_0_EXT_MAGIC_NUM,
			     EXT_CMD_ID_STAREC_UPDATE,
			     TRUE,
			     FALSE,
			     g_fgIsOid,
			     nicCmdEventSetCommon,
			     nicOidCmdTimeoutCommon,
			     (CMD_STAREC_UPDATE_HDR_SIZE + u4SetBufferLen),
			     (uint8_t *) prStaRecManualAssoc, NULL, 0);

	cnmMemFree(prAdapter, prStaRecManualAssoc);

	return rWlanStatus;
}

struct TXBF_CMD_DONE_HANDLER {
	uint32_t u4TxBfCmdId;
	void (*pFunc)(struct ADAPTER *, struct CMD_INFO *,
		      uint8_t *);
};

struct TXBF_CMD_DONE_HANDLER rTxBfCmdDoneHandler[] = {
	{BF_SOUNDING_OFF, nicCmdEventSetCommon},
	{BF_SOUNDING_ON, nicCmdEventSetCommon},
	{BF_DATA_PACKET_APPLY, nicCmdEventSetCommon},
	{BF_PFMU_MEM_ALLOCATE, nicCmdEventSetCommon},
	{BF_PFMU_MEM_RELEASE, nicCmdEventSetCommon},
	{BF_PFMU_TAG_READ, nicCmdEventPfmuTagRead},
	{BF_PFMU_TAG_WRITE, nicCmdEventSetCommon},
	{BF_PROFILE_READ, nicCmdEventPfmuDataRead},
	{BF_PROFILE_WRITE, nicCmdEventSetCommon},
	{BF_PN_READ, nicCmdEventSetCommon},
	{BF_PN_WRITE, nicCmdEventSetCommon},
	{BF_PFMU_MEM_ALLOC_MAP_READ, nicCmdEventSetCommon},
#if CFG_SUPPORT_TX_BF_FPGA
	{BF_PFMU_SW_TAG_WRITE, nicCmdEventSetCommon}
#endif
};

uint32_t
wlanoidTxBfAction(IN struct ADAPTER *prAdapter,
		  IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		  OUT uint32_t *pu4SetInfoLen) {
	union PARAM_CUSTOM_TXBF_ACTION_STRUCT *prTxBfActionInfo;
	union CMD_TXBF_ACTION rCmdTxBfActionInfo;
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;
	u_int8_t fgSetQuery, fgNeedResp;
	uint32_t u4TxBfCmdId;
	uint8_t  ucIdx;

	DEBUGFUNC("wlanoidTxBfAction");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(union
				PARAM_CUSTOM_TXBF_ACTION_STRUCT);

	if (u4SetBufferLen < sizeof(union
				    PARAM_CUSTOM_TXBF_ACTION_STRUCT))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prTxBfActionInfo = (union PARAM_CUSTOM_TXBF_ACTION_STRUCT *)
			   pvSetBuffer;

	memcpy(&rCmdTxBfActionInfo, prTxBfActionInfo,
	       sizeof(union CMD_TXBF_ACTION));

	u4TxBfCmdId =
		rCmdTxBfActionInfo.rProfileTagRead.ucTxBfCategory;
	if (TXBF_CMD_NEED_TO_RESPONSE(u4TxBfCmdId) ==
	    0) {	/* don't need response */
		fgSetQuery = TRUE;
		fgNeedResp = FALSE;
	} else {
		fgSetQuery = FALSE;
		fgNeedResp = TRUE;
	}

	for (ucIdx = 0; ucIdx < ARRAY_SIZE(rTxBfCmdDoneHandler);
	     ucIdx++) {
		if (u4TxBfCmdId == rTxBfCmdDoneHandler[ucIdx].u4TxBfCmdId)
			break;
	}

	if (ucIdx == ARRAY_SIZE(rTxBfCmdDoneHandler)) {
		DBGLOG(RFTEST, ERROR,
		       "ucIdx [%d] overrun of rTxBfCmdDoneHandler\n", ucIdx);
		return WLAN_STATUS_NOT_SUPPORTED;
	}

	rWlanStatus = wlanSendSetQueryExtCmd(prAdapter,
					     CMD_ID_LAYER_0_EXT_MAGIC_NUM,
					     EXT_CMD_ID_BF_ACTION,
					     fgSetQuery,
					     fgNeedResp,
					     g_fgIsOid,
					     rTxBfCmdDoneHandler[ucIdx].pFunc,
					     nicOidCmdTimeoutCommon,
					     sizeof(union CMD_TXBF_ACTION),
					     (uint8_t *) &rCmdTxBfActionInfo,
					     pvSetBuffer,
					     u4SetBufferLen);

	return rWlanStatus;
}

#if CFG_SUPPORT_MU_MIMO
uint32_t
wlanoidMuMimoAction(IN struct ADAPTER *prAdapter,
		    IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		    OUT uint32_t *pu4SetInfoLen) {
	struct PARAM_CUSTOM_MUMIMO_ACTION_STRUCT
		*prMuMimoActionInfo;
	union CMD_MUMIMO_ACTION rCmdMuMimoActionInfo;
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;
	u_int8_t fgSetQuery, fgNeedResp;
	uint32_t u4MuMimoCmdId;
	void (*pFunc)(struct ADAPTER *, struct CMD_INFO *,
		      uint8_t *);

	DEBUGFUNC("wlanoidMuMimoAction");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(struct
				PARAM_CUSTOM_MUMIMO_ACTION_STRUCT);

	if (u4SetBufferLen < sizeof(struct
				    PARAM_CUSTOM_MUMIMO_ACTION_STRUCT))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prMuMimoActionInfo = (struct
			      PARAM_CUSTOM_MUMIMO_ACTION_STRUCT *) pvSetBuffer;

	memcpy(&rCmdMuMimoActionInfo, prMuMimoActionInfo,
	       sizeof(union CMD_MUMIMO_ACTION));

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
					     sizeof(union CMD_MUMIMO_ACTION),
					     (uint8_t *) &rCmdMuMimoActionInfo,
					     pvSetBuffer,
					     u4SetBufferLen);

	return rWlanStatus;
}
#endif /* CFG_SUPPORT_MU_MIMO */
#endif /* CFG_SUPPORT_TX_BF */
#endif /* CFG_SUPPORT_QA_TOOL */

#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST
/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to Trigger FW Cal for Backup Cal Data to Host
 *	  Side.
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
uint32_t
wlanoidSetCalBackup(IN struct ADAPTER *prAdapter,
		    IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		    OUT uint32_t *pu4SetInfoLen) {
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;
	struct PARAM_CAL_BACKUP_STRUCT_V2 *prCalBackupDataV2Info;

	DBGLOG(RFTEST, INFO, "%s\n", __func__);

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(struct PARAM_CAL_BACKUP_STRUCT_V2);

	if (u4SetBufferLen < sizeof(struct
				    PARAM_CAL_BACKUP_STRUCT_V2))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prCalBackupDataV2Info = (struct PARAM_CAL_BACKUP_STRUCT_V2
				 *) pvSetBuffer;

	if (prCalBackupDataV2Info->ucReason == 1
	    && prCalBackupDataV2Info->ucAction == 2) {
		/* Trigger All Cal Function */
		return wlanoidSendCalBackupV2Cmd(prAdapter, pvSetBuffer,
						 u4SetBufferLen);
	} else if (prCalBackupDataV2Info->ucReason == 4
		   && prCalBackupDataV2Info->ucAction == 6) {
		/* For Debug Use, Tell FW Print Cal Data (Rom or Ram) */
		return wlanoidSendCalBackupV2Cmd(prAdapter, pvSetBuffer,
						 u4SetBufferLen);
	} else if (prCalBackupDataV2Info->ucReason == 3
		   && prCalBackupDataV2Info->ucAction == 5) {
		/* Send Cal Data to FW */
		if (prCalBackupDataV2Info->ucRomRam == 0)
			prCalBackupDataV2Info->u4RemainLength =
				g_rBackupCalDataAllV2.u4ValidRomCalDataLength;
		else if (prCalBackupDataV2Info->ucRomRam == 1)
			prCalBackupDataV2Info->u4RemainLength =
				g_rBackupCalDataAllV2.u4ValidRamCalDataLength;

		return wlanoidSendCalBackupV2Cmd(prAdapter, pvSetBuffer,
						 u4SetBufferLen);
	}

	return rWlanStatus;
}

uint32_t wlanoidSendCalBackupV2Cmd(IN struct ADAPTER *prAdapter,
				   IN void *pvQueryBuffer,
				   IN uint32_t u4QueryBufferLen) {
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;
	struct PARAM_CAL_BACKUP_STRUCT_V2 *prCalBackupDataV2Info;
	struct CMD_CAL_BACKUP_STRUCT_V2 *prCmdCalBackupDataV2;
	uint8_t	ucReason, ucAction, ucNeedResp, ucFragNum, ucRomRam;
	uint32_t u4DumpMaxSize = PARAM_CAL_DATA_DUMP_MAX_SIZE;
	uint32_t u4RemainLength, u4CurrAddr, u4CurrLen;

	DBGLOG(RFTEST, INFO, "%s\n", __func__);

	prCmdCalBackupDataV2 = (struct CMD_CAL_BACKUP_STRUCT_V2 *)
			kalMemAlloc(sizeof(struct CMD_CAL_BACKUP_STRUCT_V2),
				    VIR_MEM_TYPE);

	prCalBackupDataV2Info = (struct PARAM_CAL_BACKUP_STRUCT_V2 *)
			pvQueryBuffer;

	ucReason = prCalBackupDataV2Info->ucReason;
	ucAction = prCalBackupDataV2Info->ucAction;
	ucNeedResp = prCalBackupDataV2Info->ucNeedResp;
	ucRomRam = prCalBackupDataV2Info->ucRomRam;

	if (ucAction == 2) {
		/* Trigger All Cal Function */
		prCmdCalBackupDataV2->ucReason = ucReason;
		prCmdCalBackupDataV2->ucAction = ucAction;
		prCmdCalBackupDataV2->ucNeedResp = ucNeedResp;
		prCmdCalBackupDataV2->ucFragNum =
			prCalBackupDataV2Info->ucFragNum;
		prCmdCalBackupDataV2->ucRomRam = ucRomRam;
		prCmdCalBackupDataV2->u4ThermalValue =
			prCalBackupDataV2Info->u4ThermalValue;
		prCmdCalBackupDataV2->u4Address =
			prCalBackupDataV2Info->u4Address;
		prCmdCalBackupDataV2->u4Length =
			prCalBackupDataV2Info->u4Length;
		prCmdCalBackupDataV2->u4RemainLength =
			prCalBackupDataV2Info->u4RemainLength;
#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST_DBGLOG
		DBGLOG(RFTEST, INFO,
		       "=========== Driver Send Query CMD#0 or CMD#1 (Info) ===========\n");
		DBGLOG(RFTEST, INFO, "Reason = %d\n",
		       prCmdCalBackupDataV2->ucReason);
		DBGLOG(RFTEST, INFO, "Action = %d\n",
		       prCmdCalBackupDataV2->ucAction);
		DBGLOG(RFTEST, INFO, "NeedResp = %d\n",
		       prCmdCalBackupDataV2->ucNeedResp);
		DBGLOG(RFTEST, INFO, "FragNum = %d\n",
		       prCmdCalBackupDataV2->ucFragNum);
		DBGLOG(RFTEST, INFO, "RomRam = %d\n",
		       prCmdCalBackupDataV2->ucRomRam);
		DBGLOG(RFTEST, INFO, "ThermalValue = %d\n",
		       prCmdCalBackupDataV2->u4ThermalValue);
		DBGLOG(RFTEST, INFO, "Address = 0x%08x\n",
		       prCmdCalBackupDataV2->u4Address);
		DBGLOG(RFTEST, INFO, "Length = %d\n",
		       prCmdCalBackupDataV2->u4Length);
		DBGLOG(RFTEST, INFO, "RemainLength = %d\n",
		       prCmdCalBackupDataV2->u4RemainLength);
		DBGLOG(RFTEST, INFO,
		       "================================================================\n");
#endif

		rWlanStatus = wlanSendSetQueryCmd(prAdapter,
				CMD_ID_CAL_BACKUP_IN_HOST_V2,
				TRUE,
				FALSE,
				g_fgIsOid,
				nicCmdEventSetCommon,
				NULL,
				sizeof(struct CMD_CAL_BACKUP_STRUCT_V2),
				(uint8_t *) prCmdCalBackupDataV2,
				pvQueryBuffer,
				u4QueryBufferLen);

		kalMemFree(prCmdCalBackupDataV2, VIR_MEM_TYPE,
			   sizeof(struct CMD_CAL_BACKUP_STRUCT_V2));
	} else if (ucAction == 0 || ucAction == 1
		   || ucAction == 6) {
		/* Query CMD#0 and CMD#1. */
		/* For Thermal Value and Total Cal Data Length. */
		prCmdCalBackupDataV2->ucReason = ucReason;
		prCmdCalBackupDataV2->ucAction = ucAction;
		prCmdCalBackupDataV2->ucNeedResp = ucNeedResp;
		prCmdCalBackupDataV2->ucFragNum =
			prCalBackupDataV2Info->ucFragNum;
		prCmdCalBackupDataV2->ucRomRam = ucRomRam;
		prCmdCalBackupDataV2->u4ThermalValue =
			prCalBackupDataV2Info->u4ThermalValue;
		prCmdCalBackupDataV2->u4Address =
			prCalBackupDataV2Info->u4Address;
		prCmdCalBackupDataV2->u4Length =
			prCalBackupDataV2Info->u4Length;
		prCmdCalBackupDataV2->u4RemainLength =
			prCalBackupDataV2Info->u4RemainLength;
#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST_DBGLOG
		DBGLOG(RFTEST, INFO,
		       "=========== Driver Send Query CMD#0 or CMD#1 (Info) ===========\n");
		DBGLOG(RFTEST, INFO, "Reason = %d\n",
		       prCmdCalBackupDataV2->ucReason);
		DBGLOG(RFTEST, INFO, "Action = %d\n",
		       prCmdCalBackupDataV2->ucAction);
		DBGLOG(RFTEST, INFO, "NeedResp = %d\n",
		       prCmdCalBackupDataV2->ucNeedResp);
		DBGLOG(RFTEST, INFO, "FragNum = %d\n",
		       prCmdCalBackupDataV2->ucFragNum);
		DBGLOG(RFTEST, INFO, "RomRam = %d\n",
		       prCmdCalBackupDataV2->ucRomRam);
		DBGLOG(RFTEST, INFO, "ThermalValue = %d\n",
		       prCmdCalBackupDataV2->u4ThermalValue);
		DBGLOG(RFTEST, INFO, "Address = 0x%08x\n",
		       prCmdCalBackupDataV2->u4Address);
		DBGLOG(RFTEST, INFO, "Length = %d\n",
		       prCmdCalBackupDataV2->u4Length);
		DBGLOG(RFTEST, INFO, "RemainLength = %d\n",
		       prCmdCalBackupDataV2->u4RemainLength);
		DBGLOG(RFTEST, INFO,
		       "================================================================\n");
#endif
		rWlanStatus = wlanSendSetQueryCmd(prAdapter,
				  CMD_ID_CAL_BACKUP_IN_HOST_V2,
				  FALSE,
				  TRUE,
				  FALSE,
				  nicCmdEventQueryCalBackupV2,
				  NULL,
				  sizeof(struct CMD_CAL_BACKUP_STRUCT_V2),
				  (uint8_t *) prCmdCalBackupDataV2,
				  pvQueryBuffer,
				  u4QueryBufferLen);

		kalMemFree(prCmdCalBackupDataV2, VIR_MEM_TYPE,
			   sizeof(struct CMD_CAL_BACKUP_STRUCT_V2));
	} else if (ucAction == 4) {
		/* Query  All Cal Data from FW (Rom or Ram). */
		u4RemainLength = prCalBackupDataV2Info->u4RemainLength;
		u4CurrAddr = prCalBackupDataV2Info->u4Address +
			     prCalBackupDataV2Info->u4Length;
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
		prCmdCalBackupDataV2->u4ThermalValue =
			prCalBackupDataV2Info->u4ThermalValue;
		prCmdCalBackupDataV2->u4Address = u4CurrAddr;
		prCmdCalBackupDataV2->u4Length = u4CurrLen;
		prCmdCalBackupDataV2->u4RemainLength = u4RemainLength;
#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST_DBGLOG
		DBGLOG(RFTEST, INFO,
		       "========= Driver Send Query All Cal Data from FW (Info) =========\n");
		DBGLOG(RFTEST, INFO, "Reason = %d\n",
		       prCmdCalBackupDataV2->ucReason);
		DBGLOG(RFTEST, INFO, "Action = %d\n",
		       prCmdCalBackupDataV2->ucAction);
		DBGLOG(RFTEST, INFO, "NeedResp = %d\n",
		       prCmdCalBackupDataV2->ucNeedResp);
		DBGLOG(RFTEST, INFO, "FragNum = %d\n",
		       prCmdCalBackupDataV2->ucFragNum);
		DBGLOG(RFTEST, INFO, "RomRam = %d\n",
		       prCmdCalBackupDataV2->ucRomRam);
		DBGLOG(RFTEST, INFO, "ThermalValue = %d\n",
		       prCmdCalBackupDataV2->u4ThermalValue);
		DBGLOG(RFTEST, INFO, "Address = 0x%08x\n",
		       prCmdCalBackupDataV2->u4Address);
		DBGLOG(RFTEST, INFO, "Length = %d\n",
		       prCmdCalBackupDataV2->u4Length);
		DBGLOG(RFTEST, INFO, "RemainLength = %d\n",
		       prCmdCalBackupDataV2->u4RemainLength);
		DBGLOG(RFTEST, INFO,
		       "=================================================================\n");
#endif
		rWlanStatus = wlanSendSetQueryCmd(prAdapter,
				  CMD_ID_CAL_BACKUP_IN_HOST_V2,
				  FALSE,
				  TRUE,
				  FALSE,
				  nicCmdEventQueryCalBackupV2,
				  NULL,
				  sizeof(struct CMD_CAL_BACKUP_STRUCT_V2),
				  (uint8_t *) prCmdCalBackupDataV2,
				  pvQueryBuffer,
				  u4QueryBufferLen);

		kalMemFree(prCmdCalBackupDataV2, VIR_MEM_TYPE,
			   sizeof(struct CMD_CAL_BACKUP_STRUCT_V2));
	} else if (ucAction == 5) {
		/* Send  All Cal Data to FW (Rom or Ram). */
		u4RemainLength = prCalBackupDataV2Info->u4RemainLength;
		u4CurrAddr = prCalBackupDataV2Info->u4Address +
			     prCalBackupDataV2Info->u4Length;
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
		prCmdCalBackupDataV2->u4ThermalValue =
			prCalBackupDataV2Info->u4ThermalValue;
		prCmdCalBackupDataV2->u4Address = u4CurrAddr;
		prCmdCalBackupDataV2->u4Length = u4CurrLen;
		prCmdCalBackupDataV2->u4RemainLength = u4RemainLength;
#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST_DBGLOG
		DBGLOG(RFTEST, INFO,
		       "========= Driver Send All Cal Data to FW (Info) =========\n");
		DBGLOG(RFTEST, INFO, "Reason = %d\n",
		       prCmdCalBackupDataV2->ucReason);
		DBGLOG(RFTEST, INFO, "Action = %d\n",
		       prCmdCalBackupDataV2->ucAction);
		DBGLOG(RFTEST, INFO, "NeedResp = %d\n",
		       prCmdCalBackupDataV2->ucNeedResp);
		DBGLOG(RFTEST, INFO, "FragNum = %d\n",
		       prCmdCalBackupDataV2->ucFragNum);
		DBGLOG(RFTEST, INFO, "RomRam = %d\n",
		       prCmdCalBackupDataV2->ucRomRam);
		DBGLOG(RFTEST, INFO, "ThermalValue = %d\n",
		       prCmdCalBackupDataV2->u4ThermalValue);
		DBGLOG(RFTEST, INFO, "Address = 0x%08x\n",
		       prCmdCalBackupDataV2->u4Address);
		DBGLOG(RFTEST, INFO, "Length = %d\n",
		       prCmdCalBackupDataV2->u4Length);
		DBGLOG(RFTEST, INFO, "RemainLength = %d\n",
		       prCmdCalBackupDataV2->u4RemainLength);
#endif
		/* Copy Cal Data From Driver to FW */
		if (prCmdCalBackupDataV2->ucRomRam == 0)
			kalMemCopy(
			    (uint8_t *)(prCmdCalBackupDataV2->au4Buffer),
			    (uint8_t *)(g_rBackupCalDataAllV2.au4RomCalData) +
			    prCmdCalBackupDataV2->u4Address,
			    prCmdCalBackupDataV2->u4Length);
		else if (prCmdCalBackupDataV2->ucRomRam == 1)
			kalMemCopy(
			    (uint8_t *)(prCmdCalBackupDataV2->au4Buffer),
			    (uint8_t *)(g_rBackupCalDataAllV2.au4RamCalData) +
			    prCmdCalBackupDataV2->u4Address,
			    prCmdCalBackupDataV2->u4Length);
#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST_DBGLOG
		DBGLOG(RFTEST, INFO,
		       "Check some of elements (0x%08x), (0x%08x), (0x%08x), (0x%08x), (0x%08x)\n",
		       prCmdCalBackupDataV2->au4Buffer[0],
		       prCmdCalBackupDataV2->au4Buffer[1],
		       prCmdCalBackupDataV2->au4Buffer[2],
		       prCmdCalBackupDataV2->au4Buffer[3],
		       prCmdCalBackupDataV2->au4Buffer[4]);
		DBGLOG(RFTEST, INFO,
		       "Check some of elements (0x%08x), (0x%08x), (0x%08x), (0x%08x), (0x%08x)\n",
		       prCmdCalBackupDataV2->au4Buffer[(
						prCmdCalBackupDataV2->u4Length
						/ sizeof(uint32_t)) - 5],
		       prCmdCalBackupDataV2->au4Buffer[(
						prCmdCalBackupDataV2->u4Length
						/ sizeof(uint32_t)) - 4],
		       prCmdCalBackupDataV2->au4Buffer[(
						prCmdCalBackupDataV2->u4Length
						/ sizeof(uint32_t)) - 3],
		       prCmdCalBackupDataV2->au4Buffer[(
						prCmdCalBackupDataV2->u4Length
						/ sizeof(uint32_t)) - 2],
		       prCmdCalBackupDataV2->au4Buffer[(
						prCmdCalBackupDataV2->u4Length
						/ sizeof(uint32_t)) - 1]);

		DBGLOG(RFTEST, INFO,
		       "=================================================================\n");
#endif
		rWlanStatus = wlanSendSetQueryCmd(prAdapter,
					CMD_ID_CAL_BACKUP_IN_HOST_V2,
					FALSE,
					TRUE,
					FALSE,
					nicCmdEventQueryCalBackupV2,
					NULL,
					sizeof(struct CMD_CAL_BACKUP_STRUCT_V2),
					(uint8_t *) prCmdCalBackupDataV2,
					pvQueryBuffer,
					u4QueryBufferLen);

		kalMemFree(prCmdCalBackupDataV2, VIR_MEM_TYPE,
			   sizeof(struct CMD_CAL_BACKUP_STRUCT_V2));
	}

	return rWlanStatus;
}

uint32_t
wlanoidQueryCalBackupV2(IN struct ADAPTER *prAdapter,
			IN void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
			OUT uint32_t *pu4QueryInfoLen) {
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;
	struct PARAM_CAL_BACKUP_STRUCT_V2 *prCalBackupDataV2Info;

	DBGLOG(RFTEST, INFO, "%s\n", __func__);

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(struct CMD_CAL_BACKUP_STRUCT_V2);

	prCalBackupDataV2Info = (struct PARAM_CAL_BACKUP_STRUCT_V2
				 *) pvQueryBuffer;

	if (prCalBackupDataV2Info->ucReason == 0
	    && prCalBackupDataV2Info->ucAction == 0) {
		/* Get Thermal Temp from FW */
		return wlanoidSendCalBackupV2Cmd(prAdapter, pvQueryBuffer,
						 u4QueryBufferLen);
	} else if (prCalBackupDataV2Info->ucReason == 0
		   && prCalBackupDataV2Info->ucAction == 1) {
		/* Get Cal Data Size from FW */
		return wlanoidSendCalBackupV2Cmd(prAdapter, pvQueryBuffer,
						 u4QueryBufferLen);
	} else if (prCalBackupDataV2Info->ucReason == 2
		   && prCalBackupDataV2Info->ucAction == 4) {
		/* Get Cal Data from FW */
		if (prCalBackupDataV2Info->ucRomRam == 0)
			prCalBackupDataV2Info->u4RemainLength =
				g_rBackupCalDataAllV2.u4ValidRomCalDataLength;
		else if (prCalBackupDataV2Info->ucRomRam == 1)
			prCalBackupDataV2Info->u4RemainLength =
				g_rBackupCalDataAllV2.u4ValidRamCalDataLength;

		return wlanoidSendCalBackupV2Cmd(prAdapter, pvQueryBuffer,
						 u4QueryBufferLen);
	} else {
		return rWlanStatus;
	}
}
#endif

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
uint32_t
wlanoidQueryMcrRead(IN struct ADAPTER *prAdapter,
		    IN void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
		    OUT uint32_t *pu4QueryInfoLen) {
	struct PARAM_CUSTOM_MCR_RW_STRUCT *prMcrRdInfo;
	struct CMD_ACCESS_REG rCmdAccessReg;

	DEBUGFUNC("wlanoidQueryMcrRead");
	DBGLOG(INIT, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(struct
				  PARAM_CUSTOM_MCR_RW_STRUCT);

	if (u4QueryBufferLen < sizeof(struct
				      PARAM_CUSTOM_MCR_RW_STRUCT))
		return WLAN_STATUS_INVALID_LENGTH;

	prMcrRdInfo = (struct PARAM_CUSTOM_MCR_RW_STRUCT *)
		      pvQueryBuffer;

	/* 0x9000 - 0x9EFF reserved for FW */
#if CFG_SUPPORT_SWCR
	if ((prMcrRdInfo->u4McrOffset >> 16) == 0x9F00) {
		swCrReadWriteCmd(prAdapter, SWCR_READ,
			 (uint16_t) (prMcrRdInfo->u4McrOffset & BITS(0, 15)),
			 &prMcrRdInfo->u4McrData);
		return WLAN_STATUS_SUCCESS;
	}
#endif /* CFG_SUPPORT_SWCR */

	/* Check if access F/W Domain MCR (due to WiFiSYS is placed from
	 * 0x6000-0000
	 */
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
					   sizeof(struct CMD_ACCESS_REG),
					   (uint8_t *) &rCmdAccessReg,
					   pvQueryBuffer,
					   u4QueryBufferLen);
	} else {
		HAL_MCR_RD(prAdapter, (prMcrRdInfo->u4McrOffset & BITS(2,
				       31)),	/* address is in DWORD unit */
			   &prMcrRdInfo->u4McrData);

		DBGLOG(INIT, TRACE,
		       "MCR Read: Offset = %#08x, Data = %#08x\n",
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
uint32_t
wlanoidSetMcrWrite(IN struct ADAPTER *prAdapter,
		   IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		   OUT uint32_t *pu4SetInfoLen) {
	struct PARAM_CUSTOM_MCR_RW_STRUCT *prMcrWrInfo;
	struct CMD_ACCESS_REG rCmdAccessReg;

#if CFG_STRESS_TEST_SUPPORT
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct BSS_INFO *prBssInfo = prAdapter->prAisBssInfo;
	struct STA_RECORD *prStaRec = prBssInfo->prStaRecOfAP;
	uint32_t u4McrOffset, u4McrData;
#endif

	DEBUGFUNC("wlanoidSetMcrWrite");
	DBGLOG(INIT, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(struct PARAM_CUSTOM_MCR_RW_STRUCT);

	if (u4SetBufferLen < sizeof(struct
				    PARAM_CUSTOM_MCR_RW_STRUCT))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prMcrWrInfo = (struct PARAM_CUSTOM_MCR_RW_STRUCT *)
		      pvSetBuffer;

	/* 0x9000 - 0x9EFF reserved for FW */
	/* 0xFFFE          reserved for FW */

	/* -- Puff Stress Test Begin */
#if CFG_STRESS_TEST_SUPPORT

	/* 0xFFFFFFFE for Control Rate */
	if (prMcrWrInfo->u4McrOffset == 0xFFFFFFFE) {
		if (prMcrWrInfo->u4McrData < FIXED_RATE_NUM
		    && prMcrWrInfo->u4McrData > 0)
			prAdapter->rWifiVar.eRateSetting =
						(enum ENUM_REGISTRY_FIXED_RATE)
						(prMcrWrInfo->u4McrData);
		cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_1);
		cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_3);
		DEBUGFUNC("[Stress Test]Complete Rate is Changed...\n");
		DBGLOG(INIT, TRACE,
		       "[Stress Test] Rate is Changed to index %d...\n",
		       prAdapter->rWifiVar.eRateSetting);
	}
	/* 0xFFFFFFFD for Switch Channel */
	else if (prMcrWrInfo->u4McrOffset == 0xFFFFFFFD) {
		if (prMcrWrInfo->u4McrData <= 11
		    && prMcrWrInfo->u4McrData >= 1)
			prBssInfo->ucPrimaryChannel = prMcrWrInfo->u4McrData;
		nicUpdateBss(prAdapter, prBssInfo->ucNetTypeIndex);
		DBGLOG(INIT, TRACE,
		       "[Stress Test] Channel is switched to %d ...\n",
		       prBssInfo->ucPrimaryChannel);

		return WLAN_STATUS_SUCCESS;
	}
	/* 0xFFFFFFFFC for Control RF Band and SCO */
	else if (prMcrWrInfo->u4McrOffset == 0xFFFFFFFC) {
		/* Band */
		if (prMcrWrInfo->u4McrData & 0x80000000) {
		    /* prBssInfo->eBand = BAND_5G;
		     * prBssInfo->ucPrimaryChannel = 52; // Bond to Channel 52
		     */
		} else {
		    prBssInfo->eBand = BAND_2G4;
		    prBssInfo->ucPrimaryChannel = 8; /* Bond to Channel 6 */
		}

		/* Bandwidth */
		if (prMcrWrInfo->u4McrData & 0x00010000) {
			prStaRec->u2HtCapInfo |= HT_CAP_INFO_SUP_CHNL_WIDTH;
			prStaRec->ucDesiredPhyTypeSet = PHY_TYPE_BIT_HT;

			if (prMcrWrInfo->u4McrData == 0x00010002) {
				prBssInfo->eBssSCO = CHNL_EXT_SCB; /* U20 */
				prBssInfo->ucPrimaryChannel += 2;
			} else if (prMcrWrInfo->u4McrData == 0x00010001) {
				prBssInfo->eBssSCO = CHNL_EXT_SCA; /* L20 */
				prBssInfo->ucPrimaryChannel -= 2;
			} else {
				prBssInfo->eBssSCO = CHNL_EXT_SCA; /* 40 */
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
					   sizeof(struct CMD_ACCESS_REG),
					   (uint8_t *) &rCmdAccessReg,
					   pvSetBuffer, u4SetBufferLen);
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
					   sizeof(struct CMD_ACCESS_REG),
					   (uint8_t *) &rCmdAccessReg,
					   pvSetBuffer, u4SetBufferLen);
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
					   sizeof(struct CMD_ACCESS_REG),
					   (uint8_t *) &rCmdAccessReg,
					   pvSetBuffer, u4SetBufferLen);
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
					   sizeof(struct CMD_ACCESS_REG),
					   (uint8_t *) &rCmdAccessReg,
					   pvSetBuffer, u4SetBufferLen);
	}

	else
#endif
		/* -- Puff Stress Test End */

		/* Check if access F/W Domain MCR */
		if (prMcrWrInfo->u4McrOffset & 0xFFFF0000) {

			/* 0x9000 - 0x9EFF reserved for FW */
#if CFG_SUPPORT_SWCR
			if ((prMcrWrInfo->u4McrOffset >> 16) == 0x9F00) {
				swCrReadWriteCmd(prAdapter, SWCR_WRITE,
					(uint16_t)(prMcrWrInfo->u4McrOffset &
								BITS(0, 15)),
					&prMcrWrInfo->u4McrData);
				return WLAN_STATUS_SUCCESS;
			}
#endif /* CFG_SUPPORT_SWCR */

#if 1
			/* low power test special command */
			if (prMcrWrInfo->u4McrOffset == 0x11111110) {
				uint32_t rStatus = WLAN_STATUS_SUCCESS;
				/* DbgPrint("Enter test mode\n"); */
				prAdapter->fgTestMode = TRUE;
				return rStatus;
			}
			if (prMcrWrInfo->u4McrOffset == 0x11111111) {
				/* DbgPrint("nicpmSetAcpiPowerD3\n"); */

				nicpmSetAcpiPowerD3(prAdapter);
				kalDevSetPowerState(prAdapter->prGlueInfo,
					    (uint32_t) ParamDeviceStateD3);
				return WLAN_STATUS_SUCCESS;
			}
			if (prMcrWrInfo->u4McrOffset == 0x11111112) {

				/* DbgPrint("LP enter sleep\n"); */

				/* fill command */
				rCmdAccessReg.u4Address =
						prMcrWrInfo->u4McrOffset;
				rCmdAccessReg.u4Data =
						prMcrWrInfo->u4McrData;

				return wlanSendSetQueryCmd(prAdapter,
						CMD_ID_ACCESS_REG,
						TRUE,
						FALSE,
						g_fgIsOid,
						nicCmdEventSetCommon,
						nicOidCmdTimeoutCommon,
						sizeof(struct CMD_ACCESS_REG),
						(uint8_t *) &rCmdAccessReg,
						pvSetBuffer, u4SetBufferLen);
			}
#endif

#if 1
			/* low power test special command */
			if (prMcrWrInfo->u4McrOffset == 0x11111110) {
				uint32_t rStatus = WLAN_STATUS_SUCCESS;
				/* DbgPrint("Enter test mode\n"); */
				prAdapter->fgTestMode = TRUE;
				return rStatus;
			}
			if (prMcrWrInfo->u4McrOffset == 0x11111111) {
				/* DbgPrint("nicpmSetAcpiPowerD3\n"); */

				nicpmSetAcpiPowerD3(prAdapter);
				kalDevSetPowerState(prAdapter->prGlueInfo,
						(uint32_t) ParamDeviceStateD3);
				return WLAN_STATUS_SUCCESS;
			}
			if (prMcrWrInfo->u4McrOffset == 0x11111112) {

				/* DbgPrint("LP enter sleep\n"); */

				/* fill command */
				rCmdAccessReg.u4Address =
						prMcrWrInfo->u4McrOffset;
				rCmdAccessReg.u4Data =
						prMcrWrInfo->u4McrData;

				return wlanSendSetQueryCmd(prAdapter,
						CMD_ID_ACCESS_REG,
						TRUE,
						FALSE,
						g_fgIsOid,
						nicCmdEventSetCommon,
						nicOidCmdTimeoutCommon,
						sizeof(struct CMD_ACCESS_REG),
						(uint8_t *) &rCmdAccessReg,
						pvSetBuffer, u4SetBufferLen);
			}
#endif

#if CFG_SUPPORT_SDIO_READ_WRITE_PATTERN
			if (prMcrWrInfo->u4McrOffset == 0x22220000) {
				/* read test mode */
				kalSetSdioTestPattern(prAdapter->prGlueInfo,
								TRUE, TRUE);

				return WLAN_STATUS_SUCCESS;
			}

			if (prMcrWrInfo->u4McrOffset == 0x22220001) {
				/* write test mode */
				kalSetSdioTestPattern(prAdapter->prGlueInfo,
								TRUE, FALSE);

				return WLAN_STATUS_SUCCESS;
			}

			if (prMcrWrInfo->u4McrOffset == 0x22220002) {
				/* leave from test mode */
				kalSetSdioTestPattern(prAdapter->prGlueInfo,
								FALSE, FALSE);

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
					   sizeof(struct CMD_ACCESS_REG),
					   (uint8_t *) &rCmdAccessReg,
					   pvSetBuffer, u4SetBufferLen);
		} else {
			HAL_MCR_WR(prAdapter, (prMcrWrInfo->u4McrOffset &
				BITS(2, 31)),	/* address is in DWORD unit */
				prMcrWrInfo->u4McrData);

			DBGLOG(INIT, TRACE,
			       "MCR Write: Offset = %#08x, Data = %#08x\n",
			       prMcrWrInfo->u4McrOffset,
			       prMcrWrInfo->u4McrData);

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
uint32_t
wlanoidQueryDrvMcrRead(IN struct ADAPTER *prAdapter,
		       IN void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
		       OUT uint32_t *pu4QueryInfoLen) {
	struct PARAM_CUSTOM_MCR_RW_STRUCT *prMcrRdInfo;
	/* CMD_ACCESS_REG rCmdAccessReg; */

	DEBUGFUNC("wlanoidQueryMcrRead");
	DBGLOG(INIT, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(struct
				  PARAM_CUSTOM_MCR_RW_STRUCT);

	if (u4QueryBufferLen < sizeof(struct
				      PARAM_CUSTOM_MCR_RW_STRUCT))
		return WLAN_STATUS_INVALID_LENGTH;

	prMcrRdInfo = (struct PARAM_CUSTOM_MCR_RW_STRUCT *)
		      pvQueryBuffer;

	ACQUIRE_POWER_CONTROL_FROM_PM(prAdapter);
	HAL_MCR_RD(prAdapter, (prMcrRdInfo->u4McrOffset & BITS(2,
			       31)), &prMcrRdInfo->u4McrData);
	RECLAIM_POWER_CONTROL_TO_PM(prAdapter, FALSE);

	DBGLOG(INIT, TRACE,
	       "DRV MCR Read: Offset = %#08x, Data = %#08x\n",
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
uint32_t
wlanoidSetDrvMcrWrite(IN struct ADAPTER *prAdapter,
		      IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		      OUT uint32_t *pu4SetInfoLen) {
	struct PARAM_CUSTOM_MCR_RW_STRUCT *prMcrWrInfo;
	/* CMD_ACCESS_REG rCmdAccessReg;  */

	DEBUGFUNC("wlanoidSetMcrWrite");
	DBGLOG(INIT, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(struct PARAM_CUSTOM_MCR_RW_STRUCT);

	if (u4SetBufferLen < sizeof(struct
				    PARAM_CUSTOM_MCR_RW_STRUCT))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prMcrWrInfo = (struct PARAM_CUSTOM_MCR_RW_STRUCT *)
		      pvSetBuffer;

	ACQUIRE_POWER_CONTROL_FROM_PM(prAdapter);
	HAL_MCR_WR(prAdapter, (prMcrWrInfo->u4McrOffset & BITS(2,
			       31)), prMcrWrInfo->u4McrData);

	DBGLOG(INIT, TRACE,
	       "DRV MCR Write: Offset = %#08x, Data = %#08x\n",
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
uint32_t
wlanoidQuerySwCtrlRead(IN struct ADAPTER *prAdapter,
		       IN void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
		       OUT uint32_t *pu4QueryInfoLen) {
	struct PARAM_CUSTOM_SW_CTRL_STRUCT *prSwCtrlInfo;
	uint32_t rWlanStatus;
	uint16_t u2Id, u2SubId;
	uint32_t u4Data;

	struct CMD_SW_DBG_CTRL rCmdSwCtrl;

	DEBUGFUNC("wlanoidQuerySwCtrlRead");
	DBGLOG(INIT, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(struct
				  PARAM_CUSTOM_SW_CTRL_STRUCT);

	if (u4QueryBufferLen < sizeof(struct
				      PARAM_CUSTOM_SW_CTRL_STRUCT))
		return WLAN_STATUS_INVALID_LENGTH;

	prSwCtrlInfo = (struct PARAM_CUSTOM_SW_CTRL_STRUCT *)
		       pvQueryBuffer;

	u2Id = (uint16_t) (prSwCtrlInfo->u4Id >> 16);
	u2SubId = (uint16_t) (prSwCtrlInfo->u4Id & BITS(0, 15));
	u4Data = 0;
	rWlanStatus = WLAN_STATUS_SUCCESS;

	switch (u2Id) {
		/* 0x9000 - 0x9EFF reserved for FW */
		/* 0xFFFE          reserved for FW */

#if CFG_SUPPORT_SWCR
	case 0x9F00:
		swCrReadWriteCmd(prAdapter, SWCR_READ /* Read */,
				 (uint16_t) u2SubId, &u4Data);
		break;
#endif /* CFG_SUPPORT_SWCR */

	case 0xFFFF: {
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
			cnmDumpStaRec(prAdapter,
					(uint8_t) (u2SubId & BITS(0, 7)));
			break;

		case 0x02:
			/* Dump BSS info by index */
			bssDumpBssInfo(prAdapter,
				       (uint8_t) (u2SubId & BITS(0, 7)));
			break;

		case 0x03:
			/*Dump BSS statistics by index */
			wlanDumpBssStatistics(prAdapter,
					      (uint8_t) (u2SubId & BITS(0, 7)));
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
	default: {
		rCmdSwCtrl.u4Id = prSwCtrlInfo->u4Id;
		rCmdSwCtrl.u4Data = 0;
		rWlanStatus = wlanSendSetQueryCmd(prAdapter,
					CMD_ID_SW_DBG_CTRL,
					FALSE,
					TRUE,
					g_fgIsOid,
					nicCmdEventQuerySwCtrlRead,
					nicOidCmdTimeoutCommon,
					sizeof(struct CMD_SW_DBG_CTRL),
					(uint8_t *) &rCmdSwCtrl,
					pvQueryBuffer, u4QueryBufferLen);
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
uint32_t
wlanoidSetSwCtrlWrite(IN struct ADAPTER *prAdapter,
		      IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		      OUT uint32_t *pu4SetInfoLen) {
	struct PARAM_CUSTOM_SW_CTRL_STRUCT *prSwCtrlInfo;
	struct CMD_SW_DBG_CTRL rCmdSwCtrl;
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;
	uint16_t u2Id, u2SubId;
	uint32_t u4Data;
	uint8_t ucNss;
	uint8_t ucChannelWidth;
	uint8_t ucBssIndex;

	DEBUGFUNC("wlanoidSetSwCtrlWrite");
	DBGLOG(INIT, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(struct PARAM_CUSTOM_SW_CTRL_STRUCT);

	if (u4SetBufferLen < sizeof(struct
				    PARAM_CUSTOM_SW_CTRL_STRUCT))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prSwCtrlInfo = (struct PARAM_CUSTOM_SW_CTRL_STRUCT *)
		       pvSetBuffer;

	u2Id = (uint16_t) (prSwCtrlInfo->u4Id >> 16);
	u2SubId = (uint16_t) (prSwCtrlInfo->u4Id & BITS(0, 15));
	u4Data = prSwCtrlInfo->u4Data;

	switch (u2Id) {

		/* 0x9000 - 0x9EFF reserved for FW */
		/* 0xFFFE          reserved for FW */

#if CFG_SUPPORT_SWCR
	case 0x9F00:
		swCrReadWriteCmd(prAdapter, SWCR_WRITE, (uint16_t) u2SubId,
				 &u4Data);
		break;
#endif /* CFG_SUPPORT_SWCR */

	case 0x2222:
		ucNss = (uint8_t)(u4Data & BITS(0, 3));
		ucChannelWidth = (uint8_t)((u4Data & BITS(4, 7)) >> 4);
		ucBssIndex = (uint8_t) u2SubId;

		if ((u2SubId & BITS(8, 15)) != 0) { /* Debug OP change
						     * parameters
						     */
			DBGLOG(RLM, INFO,
			       "[UT_OP] BSS[%d] IsBwChange[%d] BW[%d] IsNssChange[%d] Nss[%d]\n",
			       ucBssIndex,
			       prAdapter->aprBssInfo[ucBssIndex]->
			       fgIsOpChangeChannelWidth,
			       prAdapter->aprBssInfo[ucBssIndex]->
			       ucOpChangeChannelWidth,
			       prAdapter->aprBssInfo[ucBssIndex]->
			       fgIsOpChangeNss,
			       prAdapter->aprBssInfo[ucBssIndex]->
			       ucOpChangeNss);

			DBGLOG(RLM, INFO,
			       "[UT_OP] current OP mode: w[%d] s1[%d] s2[%d] sco[%d] Nss[%d]\n",
			       prAdapter->aprBssInfo[ucBssIndex]->
			       ucVhtChannelWidth,
			       prAdapter->aprBssInfo[ucBssIndex]->
			       ucVhtChannelFrequencyS1,
			       prAdapter->aprBssInfo[ucBssIndex]->
			       ucVhtChannelFrequencyS2,
			       prAdapter->aprBssInfo[ucBssIndex]->
			       eBssSCO,
			       prAdapter->aprBssInfo[ucBssIndex]->
			       ucNss);
		} else {
			/* ucChannelWidth 0:20MHz, 1:40MHz, 2:80MHz, 3:160MHz
			 *                4:80+80MHz
			 */
			DBGLOG(RLM, INFO,
			       "[UT_OP] Change BSS[%d] OpMode to BW[%d] Nss[%d]\n",
			       ucBssIndex, ucChannelWidth, ucNss);
			rlmChangeOperationMode(prAdapter, ucBssIndex,
				ucChannelWidth, ucNss, rlmDummyChangeOpHandler);
		}
		break;

	case 0x1000:
		if (u2SubId == 0x8000) {
			/* CTIA power save mode setting (code: 0x10008000) */
			prAdapter->u4CtiaPowerMode = u4Data;
			prAdapter->fgEnCtiaPowerMode = TRUE;

			/*  */
			{
				enum PARAM_POWER_MODE ePowerMode;

				if (prAdapter->u4CtiaPowerMode == 0)
					/* force to keep in CAM mode */
					ePowerMode = Param_PowerModeCAM;
				else if (prAdapter->u4CtiaPowerMode == 1)
					ePowerMode = Param_PowerModeMAX_PSP;
				else
					ePowerMode = Param_PowerModeFast_PSP;

				rWlanStatus = nicConfigPowerSaveProfile(
					prAdapter,
					prAdapter->prAisBssInfo->ucBssIndex,
					ePowerMode, g_fgIsOid,
					PS_CALLER_SW_WRITE);
			}
		}
		break;
	case 0x1001:
		if (u2SubId == 0x0)
			prAdapter->fgEnOnlineScan = (u_int8_t) u4Data;
		else if (u2SubId == 0x1)
			prAdapter->fgDisBcnLostDetection = (u_int8_t) u4Data;
		else if (u2SubId == 0x2)
			prAdapter->rWifiVar.ucUapsd = (u_int8_t) u4Data;
		else if (u2SubId == 0x3) {
			prAdapter->u4UapsdAcBmp = u4Data & BITS(0, 15);
			GET_BSS_INFO_BY_INDEX(prAdapter,
			      u4Data >> 16)->rPmProfSetupInfo.ucBmpDeliveryAC =
					      (uint8_t) prAdapter->u4UapsdAcBmp;
			GET_BSS_INFO_BY_INDEX(prAdapter,
			      u4Data >> 16)->rPmProfSetupInfo.ucBmpTriggerAC =
					      (uint8_t) prAdapter->u4UapsdAcBmp;
		} else if (u2SubId == 0x4)
			prAdapter->fgDisStaAgingTimeoutDetection =
				(u_int8_t) u4Data;
		else if (u2SubId == 0x5)
			prAdapter->rWifiVar.rConnSettings.uc2G4BandwidthMode =
				(uint8_t) u4Data;
		else if (u2SubId == 0x0100) {
			if (u4Data == 2)
				prAdapter->rWifiVar.ucRxGf = FEATURE_DISABLED;
			else
				prAdapter->rWifiVar.ucRxGf = FEATURE_ENABLED;
		} else if (u2SubId == 0x0101)
			prAdapter->rWifiVar.ucRxShortGI = (uint8_t) u4Data;
		else if (u2SubId == 0x0103) { /* AP Mode WMMPS */
			DBGLOG(OID, INFO,
			       "ApUapsd 0x10010103 cmd received: %d\n",
			       u4Data);
			setApUapsdEnable(prAdapter, (u_int8_t) u4Data);
		} else if (u2SubId == 0x0110) {
			prAdapter->fgIsEnableLpdvt = (u_int8_t) u4Data;
			prAdapter->fgEnOnlineScan = (u_int8_t) u4Data;
			DBGLOG(INIT, INFO, "--- Enable LPDVT [%d] ---\n",
			       prAdapter->fgIsEnableLpdvt);
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
			u_int8_t fgIsEnable;
			uint8_t ucType;
			uint32_t u4Timeout;

			fgIsEnable = (u_int8_t) (u4Data & 0xff);
			ucType = 0;	/* ((u4Data>>4) & 0xf); */
			u4Timeout = ((u4Data >> 8) & 0xff);
			swCrDebugCheckEnable(prAdapter, fgIsEnable, ucType,
					     u4Timeout);
		}
		break;
#endif
	case 0x1003: /* for debug switches */
		switch (u2SubId) {
		case 1:
			DBGLOG(OID, INFO,
			       "Enable VoE 5.7 Packet Jitter test\n");
			prAdapter->rDebugInfo.fgVoE5_7Test = !!u4Data;
			break;
		case 0x0002:
		{
			struct CMD_TX_AMPDU rTxAmpdu;
			uint32_t rStatus;

			rTxAmpdu.fgEnable = !!u4Data;

			rStatus = wlanSendSetQueryCmd(
				prAdapter, CMD_ID_TX_AMPDU, TRUE, FALSE, FALSE,
				NULL, NULL, sizeof(struct CMD_TX_AMPDU),
				(uint8_t *)&rTxAmpdu, NULL, 0);
			DBGLOG(OID, INFO, "disable tx ampdu status %u\n",
			       rStatus);
			break;
		}
		default:
			break;
		}
		break;

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
			struct BSS_INFO *prBssInfo = prAdapter->prAisBssInfo;

			authSendDeauthFrame(prAdapter, prBssInfo,
					prBssInfo->prStaRecOfAP, NULL, 7, NULL);
		}
		/* wext_set_mode */
		/*
		 *  if (u2SubId == 0x3) {
		 *      prAdapter->prGlueInfo->rWpaInfo.u4Mfp =
		 *                              RSN_AUTH_MFP_DISABLED;
		 *  }
		 *  if (u2SubId == 0x4) {
		 *      //prAdapter->rWifiVar.rAisSpecificBssInfo
		 *      //                      .fgMgmtProtection = TRUE;
		 *      prAdapter->prGlueInfo->rWpaInfo.u4Mfp =
		 *				RSN_AUTH_MFP_OPTIONAL;
		 *  }
		 *  if (u2SubId == 0x5) {
		 *      //prAdapter->rWifiVar.rAisSpecificBssInfo
		 *      //			.fgMgmtProtection = TRUE;
		 *      prAdapter->prGlueInfo->rWpaInfo.u4Mfp =
		 *				RSN_AUTH_MFP_REQUIRED;
		 *  }
		 */
		break;
#endif
	case 0xFFFF: {
		/* CMD_ACCESS_REG rCmdAccessReg; */
#if 1				/* CFG_MT6573_SMT_TEST */
		if (u2SubId == 0x0123) {

			DBGLOG(HAL, INFO, "set smt fixed rate: %u\n", u4Data);

			if ((enum ENUM_REGISTRY_FIXED_RATE) (u4Data) <
			    FIXED_RATE_NUM)
				prAdapter->rWifiVar.eRateSetting =
					(enum ENUM_REGISTRY_FIXED_RATE)(u4Data);
			else
				prAdapter->rWifiVar.eRateSetting =
							FIXED_RATE_NONE;

			if (prAdapter->rWifiVar.eRateSetting == FIXED_RATE_NONE)
				/* Enable Auto (Long/Short) Preamble */
				prAdapter->rWifiVar.ePreambleType =
							PREAMBLE_TYPE_AUTO;
			else if ((prAdapter->rWifiVar.eRateSetting >=
				  FIXED_RATE_MCS0_20M_400NS &&
				  prAdapter->rWifiVar.eRateSetting <=
				  FIXED_RATE_MCS7_20M_400NS)
				 || (prAdapter->rWifiVar.eRateSetting >=
				     FIXED_RATE_MCS0_40M_400NS &&
				     prAdapter->rWifiVar.eRateSetting <=
				     FIXED_RATE_MCS32_400NS))
				/* Force Short Preamble */
				prAdapter->rWifiVar.ePreambleType =
							PREAMBLE_TYPE_SHORT;
			else
				/* Force Long Preamble */
				prAdapter->rWifiVar.ePreambleType =
							PREAMBLE_TYPE_LONG;

			/* abort to re-connect */
#if 1
			kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
					     WLAN_STATUS_MEDIA_DISCONNECT,
					     NULL, 0);
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
			rWlanStatus = nicEnterCtiaMode(prAdapter,
					TRUE, g_fgIsOid);
		} else if (u2SubId == 0x1235) {
			/* 1. Enaable On-Lin Scan */
			/* 3. Enable FIFO FULL no ack */
			/* 4. Enable Roaming */
			/* Enable auto tx power */
			/* 2. Keep at Fast PS */
			/* 5. Enable Beacon Timeout Detection */
			rWlanStatus = nicEnterCtiaMode(prAdapter,
					FALSE, g_fgIsOid);
		} else if (u2SubId == 0x1260) {
			/* Disable On-Line Scan */
			rWlanStatus = nicEnterCtiaModeOfScan(prAdapter,
					TRUE, TRUE);
		} else if (u2SubId == 0x1261) {
			/* Enable On-Line Scan */
			rWlanStatus = nicEnterCtiaModeOfScan(prAdapter,
					FALSE, TRUE);
		} else if (u2SubId == 0x1262) {
			/* Disable Roaming */
			rWlanStatus = nicEnterCtiaModeOfRoaming(prAdapter,
					TRUE, TRUE);
		} else if (u2SubId == 0x1263) {
			/* Enable Roaming */
			rWlanStatus = nicEnterCtiaModeOfRoaming(prAdapter,
					FALSE, TRUE);
		} else if (u2SubId == 0x1264) {
			/* Keep at CAM mode */
			rWlanStatus = nicEnterCtiaModeOfCAM(prAdapter,
					TRUE, g_fgIsOid);
		} else if (u2SubId == 0x1265) {
			/* Keep at Fast PS */
			rWlanStatus = nicEnterCtiaModeOfCAM(prAdapter,
					FALSE, g_fgIsOid);
		} else if (u2SubId == 0x1266) {
			/* Disable Beacon Timeout Detection */
			rWlanStatus = nicEnterCtiaModeOfBCNTimeout(prAdapter,
					TRUE, TRUE);
		} else if (u2SubId == 0x1267) {
			/* Enable Beacon Timeout Detection */
			rWlanStatus = nicEnterCtiaModeOfBCNTimeout(prAdapter,
					FALSE, TRUE);
		} else if (u2SubId == 0x1268) {
			/* Disalbe auto tx power */
			rWlanStatus = nicEnterCtiaModeOfAutoTxPower(prAdapter,
					TRUE, TRUE);
		} else if (u2SubId == 0x1269) {
			/* Enable auto tx power */
			rWlanStatus = nicEnterCtiaModeOfAutoTxPower(prAdapter,
					FALSE, TRUE);
		} else if (u2SubId == 0x1270) {
			/* Disalbe FIFO FULL no ack  */
			rWlanStatus = nicEnterCtiaModeOfFIFOFullNoAck(prAdapter,
					TRUE, TRUE);
		} else if (u2SubId == 0x1271) {
			/* Enable FIFO FULL no ack */
			rWlanStatus = nicEnterCtiaModeOfFIFOFullNoAck(prAdapter,
					FALSE, TRUE);
		}
#endif
#if CFG_MTK_STAGE_SCAN
		else if (u2SubId == 0x1250)
			prAdapter->aePreferBand[KAL_NETWORK_TYPE_AIS_INDEX] =
				BAND_NULL;
		else if (u2SubId == 0x1251)
			prAdapter->aePreferBand[KAL_NETWORK_TYPE_AIS_INDEX] =
				BAND_2G4;
		else if (u2SubId == 0x1252) {
			if (prAdapter->fgEnable5GBand)
				prAdapter->aePreferBand
				[KAL_NETWORK_TYPE_AIS_INDEX] = BAND_5G;
			else
				/* Skip this setting if 5G band is disabled */
				DBGLOG(SCN, INFO,
				       "Skip 5G stage scan request due to 5G is disabled\n");
		}
#endif
	}
	break;

	case 0x9000:
	default: {
		rCmdSwCtrl.u4Id = prSwCtrlInfo->u4Id;
		rCmdSwCtrl.u4Data = prSwCtrlInfo->u4Data;
		rWlanStatus = wlanSendSetQueryCmd(prAdapter,
					  CMD_ID_SW_DBG_CTRL,
					  TRUE,
					  FALSE,
					  g_fgIsOid,
					  nicCmdEventSetCommon,
					  nicOidCmdTimeoutCommon,
					  sizeof(struct CMD_SW_DBG_CTRL),
					  (uint8_t *) &rCmdSwCtrl,
					  pvSetBuffer, u4SetBufferLen);
	}
	}			/* switch(u2Id)  */

	return rWlanStatus;
}				/* wlanoidSetSwCtrlWrite */

uint32_t
wlanoidQueryChipConfig(IN struct ADAPTER *prAdapter,
		       IN void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
		       OUT uint32_t *pu4QueryInfoLen) {
	struct PARAM_CUSTOM_CHIP_CONFIG_STRUCT *prChipConfigInfo;
	struct CMD_CHIP_CONFIG rCmdChipConfig;
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidQuerySwCtrlRead");
	DBGLOG(INIT, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(struct
				  PARAM_CUSTOM_CHIP_CONFIG_STRUCT);

	if (u4QueryBufferLen < sizeof(struct
				      PARAM_CUSTOM_CHIP_CONFIG_STRUCT))
		return WLAN_STATUS_INVALID_LENGTH;

	prChipConfigInfo = (struct PARAM_CUSTOM_CHIP_CONFIG_STRUCT
			    *) pvQueryBuffer;
	kalMemZero(&rCmdChipConfig, sizeof(rCmdChipConfig));

	rCmdChipConfig.u2Id = prChipConfigInfo->u2Id;
	rCmdChipConfig.ucType = prChipConfigInfo->ucType;
	rCmdChipConfig.ucRespType = prChipConfigInfo->ucRespType;
	rCmdChipConfig.u2MsgSize = prChipConfigInfo->u2MsgSize;
	if (rCmdChipConfig.u2MsgSize > CHIP_CONFIG_RESP_SIZE) {
		DBGLOG(REQ, INFO,
		       "Chip config Msg Size %u is not valid (query)\n",
		       rCmdChipConfig.u2MsgSize);
		rCmdChipConfig.u2MsgSize = CHIP_CONFIG_RESP_SIZE;
	}
	kalMemCopy(rCmdChipConfig.aucCmd, prChipConfigInfo->aucCmd,
		   rCmdChipConfig.u2MsgSize);

	rWlanStatus = wlanSendSetQueryCmd(prAdapter,
					  CMD_ID_CHIP_CONFIG, FALSE,
					  TRUE, g_fgIsOid,
					  /*nicCmdEventQuerySwCtrlRead, */
					  nicCmdEventQueryChipConfig,
					  nicOidCmdTimeoutCommon,
					  sizeof(struct CMD_CHIP_CONFIG),
					  (uint8_t *) &rCmdChipConfig,
					  pvQueryBuffer,
					  u4QueryBufferLen);

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
uint32_t
wlanoidSetChipConfig(IN struct ADAPTER *prAdapter,
		     IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		     OUT uint32_t *pu4SetInfoLen) {
	struct PARAM_CUSTOM_CHIP_CONFIG_STRUCT *prChipConfigInfo;
	struct CMD_CHIP_CONFIG rCmdChipConfig;
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;

	DATA_STRUCT_INSPECTING_ASSERT(
		sizeof(prChipConfigInfo->aucCmd) == CHIP_CONFIG_RESP_SIZE);
	DEBUGFUNC("wlanoidSetChipConfig");
	DBGLOG(INIT, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(struct
				PARAM_CUSTOM_CHIP_CONFIG_STRUCT);

	if (u4SetBufferLen < sizeof(struct
				    PARAM_CUSTOM_CHIP_CONFIG_STRUCT))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prChipConfigInfo = (struct PARAM_CUSTOM_CHIP_CONFIG_STRUCT
			    *) pvSetBuffer;
	kalMemZero(&rCmdChipConfig, sizeof(rCmdChipConfig));

	rCmdChipConfig.u2Id = prChipConfigInfo->u2Id;
	rCmdChipConfig.ucType = prChipConfigInfo->ucType;
	rCmdChipConfig.ucRespType = prChipConfigInfo->ucRespType;
	rCmdChipConfig.u2MsgSize = prChipConfigInfo->u2MsgSize;
	if (rCmdChipConfig.u2MsgSize > CHIP_CONFIG_RESP_SIZE) {
		DBGLOG(REQ, INFO,
		       "Chip config Msg Size %u is not valid (set)\n",
		       rCmdChipConfig.u2MsgSize);
		rCmdChipConfig.u2MsgSize = CHIP_CONFIG_RESP_SIZE;
	}
	kalMemCopy(rCmdChipConfig.aucCmd, prChipConfigInfo->aucCmd,
		   rCmdChipConfig.u2MsgSize);

	rWlanStatus = wlanSendSetQueryCmd(prAdapter,
					  CMD_ID_CHIP_CONFIG,
					  TRUE,
					  FALSE,
					  g_fgIsOid,
					  nicCmdEventSetCommon,
					  nicOidCmdTimeoutCommon,
					  sizeof(struct CMD_CHIP_CONFIG),
					  (uint8_t *) &rCmdChipConfig,
					  pvSetBuffer, u4SetBufferLen);

	return rWlanStatus;
} /* wlanoidSetChipConfig */

void
wlanLoadDefaultCustomerSetting(IN struct ADAPTER *
			       prAdapter) {

	uint8_t ucItemNum, i;


	ucItemNum = (sizeof(g_rDefaulteSetting) / sizeof(
			     struct PARAM_CUSTOM_KEY_CFG_STRUCT));
	DBGLOG(INIT, TRACE, "Default firmware setting %d item\n",
	       ucItemNum);


	for (i = 0; i < ucItemNum; i++) {
		wlanCfgSet(prAdapter, g_rDefaulteSetting[i].aucKey,
			   g_rDefaulteSetting[i].aucValue, 0);
		DBGLOG(INIT, TRACE, "%s with %s\n",
		       g_rDefaulteSetting[i].aucKey,
		       g_rDefaulteSetting[i].aucValue);
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
uint32_t
wlanoidSetKeyCfg(IN struct ADAPTER *prAdapter,
		 IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		 OUT uint32_t *pu4SetInfoLen) {
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;
	struct PARAM_CUSTOM_KEY_CFG_STRUCT *prKeyCfgInfo;

	DEBUGFUNC("wlanoidSetKeyCfg");
	DBGLOG(INIT, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(struct PARAM_CUSTOM_KEY_CFG_STRUCT);

	if (u4SetBufferLen < sizeof(struct
				    PARAM_CUSTOM_KEY_CFG_STRUCT))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);
	prKeyCfgInfo = (struct PARAM_CUSTOM_KEY_CFG_STRUCT *)
		       pvSetBuffer;

	if (kalMemCmp(prKeyCfgInfo->aucKey, "reload", 6) == 0)
		wlanGetConfig(prAdapter); /* Reload config file */
	else
		wlanCfgSet(prAdapter, prKeyCfgInfo->aucKey,
			   prKeyCfgInfo->aucValue, 0);

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
uint32_t
wlanoidQueryEepromRead(IN struct ADAPTER *prAdapter,
		       IN void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
		       OUT uint32_t *pu4QueryInfoLen) {
	struct PARAM_CUSTOM_EEPROM_RW_STRUCT *prEepromRwInfo;
	struct CMD_ACCESS_EEPROM rCmdAccessEeprom;

	DEBUGFUNC("wlanoidQueryEepromRead");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(struct
				  PARAM_CUSTOM_EEPROM_RW_STRUCT);

	if (u4QueryBufferLen < sizeof(struct
				      PARAM_CUSTOM_EEPROM_RW_STRUCT))
		return WLAN_STATUS_INVALID_LENGTH;

	prEepromRwInfo = (struct PARAM_CUSTOM_EEPROM_RW_STRUCT *)
			 pvQueryBuffer;

	kalMemZero(&rCmdAccessEeprom,
		   sizeof(struct CMD_ACCESS_EEPROM));
	rCmdAccessEeprom.u2Offset = prEepromRwInfo->ucEepromIndex;

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_ACCESS_EEPROM,
				   FALSE,
				   TRUE,
				   g_fgIsOid,
				   nicCmdEventQueryEepromRead,
				   nicOidCmdTimeoutCommon,
				   sizeof(struct CMD_ACCESS_EEPROM),
				   (uint8_t *) &rCmdAccessEeprom, pvQueryBuffer,
				   u4QueryBufferLen);

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
uint32_t
wlanoidSetEepromWrite(IN struct ADAPTER *prAdapter,
		      IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		      OUT uint32_t *pu4SetInfoLen) {
	struct PARAM_CUSTOM_EEPROM_RW_STRUCT *prEepromRwInfo;
	struct CMD_ACCESS_EEPROM rCmdAccessEeprom;

	DEBUGFUNC("wlanoidSetEepromWrite");
	DBGLOG(INIT, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(struct
				PARAM_CUSTOM_EEPROM_RW_STRUCT);

	if (u4SetBufferLen < sizeof(struct
				    PARAM_CUSTOM_EEPROM_RW_STRUCT))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prEepromRwInfo = (struct PARAM_CUSTOM_EEPROM_RW_STRUCT *)
			 pvSetBuffer;

	kalMemZero(&rCmdAccessEeprom,
		   sizeof(struct CMD_ACCESS_EEPROM));
	rCmdAccessEeprom.u2Offset = prEepromRwInfo->ucEepromIndex;
	rCmdAccessEeprom.u2Data = prEepromRwInfo->u2EepromData;

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_ACCESS_EEPROM,
				   TRUE,
				   FALSE,
				   TRUE,
				   nicCmdEventSetCommon,
				   nicOidCmdTimeoutCommon,
				   sizeof(struct CMD_ACCESS_EEPROM),
				   (uint8_t *) &rCmdAccessEeprom, pvSetBuffer,
				   u4SetBufferLen);

} /* wlanoidSetEepromWrite */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to query the number of the successfully
 *	  transmitted packets.
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
uint32_t
wlanoidQueryXmitOk(IN struct ADAPTER *prAdapter,
		   IN void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
		   OUT uint32_t *pu4QueryInfoLen) {
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
		*pu4QueryInfoLen = sizeof(uint32_t);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (u4QueryBufferLen < sizeof(uint32_t)
		   || (u4QueryBufferLen > sizeof(uint32_t)
		       && u4QueryBufferLen < sizeof(uint64_t))) {
		*pu4QueryInfoLen = sizeof(uint64_t);
		return WLAN_STATUS_INVALID_LENGTH;
	}
#if CFG_ENABLE_STATISTICS_BUFFERING
	if (IsBufferedStatisticsUsable(prAdapter) == TRUE) {
		if (u4QueryBufferLen == sizeof(uint32_t)) {
			*pu4QueryInfoLen = sizeof(uint32_t);
			*(uint32_t *) pvQueryBuffer = (uint32_t)
				prAdapter->rStatStruct
				.rTransmittedFragmentCount.QuadPart;
		} else {
			*pu4QueryInfoLen = sizeof(uint64_t);
			*(uint64_t *) pvQueryBuffer = (uint64_t)
				prAdapter->rStatStruct
				.rTransmittedFragmentCount.QuadPart;
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
				   nicOidCmdTimeoutCommon, 0, NULL,
				   pvQueryBuffer,
				   u4QueryBufferLen);

} /* wlanoidQueryXmitOk */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to query the number of the successfully
 *	  received packets.
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
uint32_t
wlanoidQueryRcvOk(IN struct ADAPTER *prAdapter,
		  IN void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
		  OUT uint32_t *pu4QueryInfoLen) {
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
		*pu4QueryInfoLen = sizeof(uint32_t);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (u4QueryBufferLen < sizeof(uint32_t)
		   || (u4QueryBufferLen > sizeof(uint32_t)
		       && u4QueryBufferLen < sizeof(uint64_t))) {
		*pu4QueryInfoLen = sizeof(uint64_t);
		return WLAN_STATUS_INVALID_LENGTH;
	}
#if CFG_ENABLE_STATISTICS_BUFFERING
	if (IsBufferedStatisticsUsable(prAdapter) == TRUE) {
		if (u4QueryBufferLen == sizeof(uint32_t)) {
			*pu4QueryInfoLen = sizeof(uint32_t);
			*(uint32_t *) pvQueryBuffer = (uint32_t)
				prAdapter->rStatStruct.rReceivedFragmentCount
				.QuadPart;
		} else {
			*pu4QueryInfoLen = sizeof(uint64_t);
			*(uint64_t *) pvQueryBuffer = (uint64_t)
				prAdapter->rStatStruct.rReceivedFragmentCount
				.QuadPart;
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
				   nicOidCmdTimeoutCommon, 0, NULL,
				   pvQueryBuffer,
				   u4QueryBufferLen);

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
uint32_t
wlanoidQueryXmitError(IN struct ADAPTER *prAdapter,
		      IN void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
		      OUT uint32_t *pu4QueryInfoLen) {
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
		*pu4QueryInfoLen = sizeof(uint32_t);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (u4QueryBufferLen < sizeof(uint32_t)
		   || (u4QueryBufferLen > sizeof(uint32_t)
		       && u4QueryBufferLen < sizeof(uint64_t))) {
		*pu4QueryInfoLen = sizeof(uint64_t);
		return WLAN_STATUS_INVALID_LENGTH;
	}

#if CFG_ENABLE_STATISTICS_BUFFERING
	if (IsBufferedStatisticsUsable(prAdapter) == TRUE) {
		if (u4QueryBufferLen == sizeof(uint32_t)) {
			*pu4QueryInfoLen = sizeof(uint32_t);
			*(uint32_t *) pvQueryBuffer = (uint32_t)
				prAdapter->rStatStruct.rFailedCount.QuadPart;
		} else {
			*pu4QueryInfoLen = sizeof(uint64_t);
			*(uint64_t *) pvQueryBuffer = (uint64_t)
				prAdapter->rStatStruct.rFailedCount.QuadPart;
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
				   nicOidCmdTimeoutCommon, 0, NULL,
				   pvQueryBuffer,
				   u4QueryBufferLen);

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
uint32_t
wlanoidQueryXmitOneCollision(IN struct ADAPTER *prAdapter,
			     IN void *pvQueryBuffer,
			     IN uint32_t u4QueryBufferLen,
			     OUT uint32_t *pu4QueryInfoLen) {
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
		*pu4QueryInfoLen = sizeof(uint32_t);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (u4QueryBufferLen < sizeof(uint32_t)
		   || (u4QueryBufferLen > sizeof(uint32_t)
		       && u4QueryBufferLen < sizeof(uint64_t))) {
		*pu4QueryInfoLen = sizeof(uint64_t);
		return WLAN_STATUS_INVALID_LENGTH;
	}

#if CFG_ENABLE_STATISTICS_BUFFERING
	if (IsBufferedStatisticsUsable(prAdapter) == TRUE) {
		if (u4QueryBufferLen == sizeof(uint32_t)) {
			*pu4QueryInfoLen = sizeof(uint32_t);
			*(uint32_t *) pvQueryBuffer = (uint32_t)
				(prAdapter->rStatStruct.rMultipleRetryCount
				.QuadPart -
				prAdapter->rStatStruct.rRetryCount.QuadPart);
		} else {
			*pu4QueryInfoLen = sizeof(uint64_t);
			*(uint64_t *) pvQueryBuffer = (uint64_t)
				(prAdapter->rStatStruct.rMultipleRetryCount
				.QuadPart -
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
				   nicOidCmdTimeoutCommon, 0, NULL,
				   pvQueryBuffer,
				   u4QueryBufferLen);

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
uint32_t
wlanoidQueryXmitMoreCollisions(IN struct ADAPTER *prAdapter,
			       IN void *pvQueryBuffer,
			       IN uint32_t u4QueryBufferLen,
			       OUT uint32_t *pu4QueryInfoLen) {
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
		*pu4QueryInfoLen = sizeof(uint32_t);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (u4QueryBufferLen < sizeof(uint32_t)
		   || (u4QueryBufferLen > sizeof(uint32_t)
		       && u4QueryBufferLen < sizeof(uint64_t))) {
		*pu4QueryInfoLen = sizeof(uint64_t);
		return WLAN_STATUS_INVALID_LENGTH;
	}

#if CFG_ENABLE_STATISTICS_BUFFERING
	if (IsBufferedStatisticsUsable(prAdapter) == TRUE) {
		if (u4QueryBufferLen == sizeof(uint32_t)) {
			*pu4QueryInfoLen = sizeof(uint32_t);
			*(uint32_t *) pvQueryBuffer = (uint32_t) (
				prAdapter->rStatStruct.rMultipleRetryCount
				.QuadPart);
		} else {
			*pu4QueryInfoLen = sizeof(uint64_t);
			*(uint64_t *) pvQueryBuffer = (uint64_t) (
				prAdapter->rStatStruct.rMultipleRetryCount
				.QuadPart);
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
				   nicOidCmdTimeoutCommon, 0, NULL,
				   pvQueryBuffer,
				   u4QueryBufferLen);

} /* wlanoidQueryXmitMoreCollisions */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to query the number of frames
 *                not transmitted due to excessive collisions.
 *
 * \param[in] prAdapter          Pointer to the Adapter structure.
 * \param[in] pvQueryBuffer      Pointer to the buffer that holds the result of
 *                               the query.
 * \param[in] u4QueryBufferLen   The length of the query buffer.
 * \param[out] pu4QueryInfoLen   If the call is successful, returns the number
 *				 of bytes written into the query buffer. If the
 *				 call failed due to invalid length of the query
 *				 buffer, returns the amount of storage needed.
 *
 * \retval WLAN_STATUS_SUCCESS
 * \retval WLAN_STATUS_INVALID_LENGTH
 */
/*----------------------------------------------------------------------------*/
uint32_t
wlanoidQueryXmitMaxCollisions(IN struct ADAPTER *prAdapter,
			      IN void *pvQueryBuffer,
			      IN uint32_t u4QueryBufferLen,
			      OUT uint32_t *pu4QueryInfoLen) {
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
		*pu4QueryInfoLen = sizeof(uint32_t);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (u4QueryBufferLen < sizeof(uint32_t)
		   || (u4QueryBufferLen > sizeof(uint32_t)
		       && u4QueryBufferLen < sizeof(uint64_t))) {
		*pu4QueryInfoLen = sizeof(uint64_t);
		return WLAN_STATUS_INVALID_LENGTH;
	}
#if CFG_ENABLE_STATISTICS_BUFFERING
	if (IsBufferedStatisticsUsable(prAdapter) == TRUE) {
		if (u4QueryBufferLen == sizeof(uint32_t)) {
			*pu4QueryInfoLen = sizeof(uint32_t);
			*(uint32_t *) pvQueryBuffer = (uint32_t)
				prAdapter->rStatStruct.rFailedCount.QuadPart;
		} else {
			*pu4QueryInfoLen = sizeof(uint64_t);
			*(uint64_t *) pvQueryBuffer = (uint64_t)
				prAdapter->rStatStruct.rFailedCount.QuadPart;
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
				   nicOidCmdTimeoutCommon, 0,
				   NULL, pvQueryBuffer,
				   u4QueryBufferLen);

} /* wlanoidQueryXmitMaxCollisions */

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
 * \param[out] pu4QueryInfoLen   If the call is successful, returns the number
 *				 of bytes written into the query buffer. If the
 *				 call failed due to invalid length of the query
 *				 buffer, returns the amount of storage needed.
 *
 * \retval WLAN_STATUS_SUCCESS
 */
/*----------------------------------------------------------------------------*/
uint32_t
wlanoidQueryOidInterfaceVersion(IN struct ADAPTER *
				prAdapter,
				IN void *pvQueryBuffer,
				IN uint32_t u4QueryBufferLen,
				OUT uint32_t *pu4QueryInfoLen) {
	DEBUGFUNC("wlanoidQueryOidInterfaceVersion");

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	*(uint32_t *) pvQueryBuffer =
		MTK_CUSTOM_OID_INTERFACE_VERSION;
	*pu4QueryInfoLen = sizeof(uint32_t);

	DBGLOG(REQ, WARN, "Custom OID interface version: %#08X\n",
	       *(uint32_t *) pvQueryBuffer);

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
 * \param[out] pu4QueryInfoLen   If the call is successful, returns the number
 *				 of bytes written into the query buffer. If the
 *				 call failed due to invalid length of the query
 *				 buffer, returns the amount of storage needed.
 *
 * \retval WLAN_STATUS_SUCCESS
 * \retval WLAN_STATUS_BUFFER_TOO_SHORT
 */
/*----------------------------------------------------------------------------*/
uint32_t
wlanoidQueryMulticastList(IN struct ADAPTER *prAdapter,
			  OUT void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
			  OUT uint32_t *pu4QueryInfoLen) {
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
				   nicOidCmdTimeoutCommon, 0,
				   NULL, pvQueryBuffer,
				   u4QueryBufferLen);
#else
	return WLAN_STATUS_SUCCESS;
#endif
} /* end of wlanoidQueryMulticastList() */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to set Multicast Address List.
 *
 * \param[in] prAdapter      Pointer to the Adapter structure.
 * \param[in] pvSetBuffer    Pointer to the buffer that holds the data to be
 *			     set.
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
wlanoidSetMulticastList(IN struct ADAPTER *prAdapter,
			IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			OUT uint32_t *pu4SetInfoLen) {
	struct CMD_MAC_MCAST_ADDR rCmdMacMcastAddr;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	/* The data must be a multiple of the Ethernet address size. */
	if ((u4SetBufferLen % MAC_ADDR_LEN)) {
		DBGLOG(REQ, WARN, "Invalid MC list length %u\n",
		       u4SetBufferLen);

		*pu4SetInfoLen = (((u4SetBufferLen + MAC_ADDR_LEN) - 1) /
				  MAC_ADDR_LEN) * MAC_ADDR_LEN;

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

	rCmdMacMcastAddr.u4NumOfGroupAddr = u4SetBufferLen /
					    MAC_ADDR_LEN;
	rCmdMacMcastAddr.ucBssIndex =
		prAdapter->prAisBssInfo->ucBssIndex;
	kalMemCopy(rCmdMacMcastAddr.arAddress, pvSetBuffer,
		   u4SetBufferLen);

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_MAC_MCAST_ADDR,
				   TRUE,
				   FALSE,
				   g_fgIsOid,
				   nicCmdEventSetCommon,
				   nicOidCmdTimeoutCommon,
				   sizeof(struct CMD_MAC_MCAST_ADDR),
				   (uint8_t *) &rCmdMacMcastAddr,
				   pvSetBuffer, u4SetBufferLen);
}				/* end of wlanoidSetMulticastList() */

uint32_t
wlanoidRssiMonitor(IN struct ADAPTER *prAdapter,
		   OUT void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
		   OUT uint32_t *pu4QueryInfoLen) {
	struct PARAM_RSSI_MONITOR_T rRssi;
	int8_t orig_max_rssi_value;
	int8_t orig_min_rssi_value;
	uint32_t rStatus1 = WLAN_STATUS_SUCCESS;
	uint32_t rStatus2;

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(struct PARAM_RSSI_MONITOR_T);

	/* Check for query buffer length */
	if (u4QueryBufferLen < *pu4QueryInfoLen) {
		DBGLOG(OID, WARN, "Too short length %u\n",
		       u4QueryBufferLen);
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}

	if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) ==
	    PARAM_MEDIA_STATE_DISCONNECTED)
		rStatus1 = WLAN_STATUS_ADAPTER_NOT_READY;

	kalMemZero(&rRssi, sizeof(struct PARAM_RSSI_MONITOR_T));

	orig_max_rssi_value = rRssi.max_rssi_value;
	orig_min_rssi_value = rRssi.min_rssi_value;

	kalMemCopy(&rRssi, pvQueryBuffer,
		   sizeof(struct PARAM_RSSI_MONITOR_T));
	if (rRssi.enable) {
		if (rRssi.max_rssi_value > PARAM_WHQL_RSSI_MAX_DBM)
			rRssi.max_rssi_value = PARAM_WHQL_RSSI_MAX_DBM;
		if (rRssi.min_rssi_value < -120)
			rRssi.min_rssi_value = -120;
	} else {
		rRssi.max_rssi_value = 0;
		rRssi.min_rssi_value = 0;
	}

	DBGLOG(OID, INFO,
	       "enable=%d, max_rssi_value=%d, min_rssi_value=%d, orig_max_rssi_value=%d, orig_min_rssi_value=%d\n",
	       rRssi.enable, rRssi.max_rssi_value, rRssi.min_rssi_value,
	       orig_max_rssi_value, orig_min_rssi_value);

	rStatus2 = wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_RSSI_MONITOR,
				   TRUE,
				   FALSE,
				   g_fgIsOid,
				   nicCmdEventSetCommon,
				   nicOidCmdTimeoutCommon,
				   sizeof(struct PARAM_RSSI_MONITOR_T),
				   (uint8_t *)&rRssi, NULL, 0);

	return (rStatus1 == WLAN_STATUS_ADAPTER_NOT_READY) ?
		rStatus1 : rStatus2;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to set Packet Filter.
 *
 * \param[in] prAdapter      Pointer to the Adapter structure.
 * \param[in] pvSetBuffer    Pointer to the buffer that holds the data to be
 *			     set.
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
uint32_t
wlanoidSetCurrentPacketFilter(IN struct ADAPTER *prAdapter,
			      IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			      OUT uint32_t *pu4SetInfoLen) {
	uint32_t u4NewPacketFilter;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t rResult = WLAN_STATUS_FAILURE;
	struct CMD_RX_PACKET_FILTER rSetRxPacketFilter;

	DBGLOG(REQ, INFO, "wlanoidSetCurrentPacketFilter");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	if (u4SetBufferLen < sizeof(uint32_t)) {
		*pu4SetInfoLen = sizeof(uint32_t);
		return WLAN_STATUS_INVALID_LENGTH;
	}
	ASSERT(pvSetBuffer);

	/* Set the new packet filter. */
	u4NewPacketFilter = *(uint32_t *) pvSetBuffer;

	DBGLOG(REQ, TRACE, "New packet filter: %#08x\n",
	       u4NewPacketFilter);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in set current packet filter! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	do {
		/* Verify the bits of the new packet filter. If any bits are
		 * set that we don't support, leave.
		 */
		if (u4NewPacketFilter & ~(PARAM_PACKET_FILTER_SUPPORTED)) {
			rStatus = WLAN_STATUS_NOT_SUPPORTED;
			DBGLOG(REQ, WARN, "some flags we don't support\n");
			break;
		}
#if DBG
		/* Need to enable or disable promiscuous support depending on
		 * the new filter.
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

		kalMemZero(&rSetRxPacketFilter, sizeof(rSetRxPacketFilter));
		rSetRxPacketFilter.u4RxPacketFilter =
			prAdapter->u4OsPacketFilter;
		rResult = wlanoidSetPacketFilter(prAdapter,
						 &rSetRxPacketFilter,
						 g_fgIsOid, pvSetBuffer,
						 u4SetBufferLen);
		DBGLOG(OID, TRACE, "[MC debug] u4OsPacketFilter=%x\n",
		       prAdapter->u4OsPacketFilter);
		return rResult;
	} else {
		return rStatus;
	}
}				/* wlanoidSetCurrentPacketFilter */

uint32_t wlanoidSetPacketFilter(struct ADAPTER *prAdapter,
				void *pvPacketFiltr,
				u_int8_t fgIsOid, void *pvSetBuffer,
				uint32_t u4SetBufferLen) {
	struct CMD_RX_PACKET_FILTER *prSetRxPacketFilter = NULL;

	prSetRxPacketFilter = (struct CMD_RX_PACKET_FILTER *)
			      pvPacketFiltr;
#if CFG_SUPPORT_DROP_MC_PACKET
	if (prAdapter->prGlueInfo->fgIsInSuspendMode)
		prSetRxPacketFilter->u4RxPacketFilter &=
			~(PARAM_PACKET_FILTER_MULTICAST |
			  PARAM_PACKET_FILTER_ALL_MULTICAST);
#endif
	DBGLOG(OID, INFO,
	       "[MC debug] u4PacketFilter=%x, IsSuspend=%d\n",
	       prSetRxPacketFilter->u4RxPacketFilter,
	       prAdapter->prGlueInfo->fgIsInSuspendMode);
	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_SET_RX_FILTER,
				   TRUE,
				   FALSE,
				   fgIsOid,
				   nicCmdEventSetCommon,
				   nicOidCmdTimeoutCommon,
				   sizeof(struct CMD_RX_PACKET_FILTER),
				   (uint8_t *)prSetRxPacketFilter,
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
 * \param[out] pu4QueryInfoLen   If the call is successful, returns the number
 *				 of bytes written into the query buffer. If the
 *				 call failed due to invalid length of the query
 *				 buffer, returns the amount of storage needed.
 *
 * \retval WLAN_STATUS_SUCCESS
 */
/*----------------------------------------------------------------------------*/
uint32_t
wlanoidQueryCurrentPacketFilter(IN struct ADAPTER *prAdapter,
				OUT void *pvQueryBuffer,
				IN uint32_t u4QueryBufferLen,
				OUT uint32_t *pu4QueryInfoLen) {
	DEBUGFUNC("wlanoidQueryCurrentPacketFilter");
	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);

	*pu4QueryInfoLen = sizeof(uint32_t);

	if (u4QueryBufferLen >= sizeof(uint32_t)) {
		ASSERT(pvQueryBuffer);
		*(uint32_t *) pvQueryBuffer = prAdapter->u4OsPacketFilter;
	}

	return WLAN_STATUS_SUCCESS;
} /* wlanoidQueryCurrentPacketFilter */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to query ACPI device power state.
 *
 * \param[in] prAdapter          Pointer to the Adapter structure.
 * \param[in] pvQueryBuffer      Pointer to the buffer that holds the result of
 *                               the query.
 * \param[in] u4QueryBufferLen   The length of the query buffer.
 * \param[out] pu4QueryInfoLen   If the call is successful, returns the number
 *				 of bytes written into the query buffer. If the
 *				 call failed due to invalid length of the query
 *				 buffer, returns the amount of storage needed.
 *
 * \retval WLAN_STATUS_SUCCESS
 */
/*----------------------------------------------------------------------------*/
uint32_t
wlanoidQueryAcpiDevicePowerState(IN struct ADAPTER *
				 prAdapter,
				 IN void *pvQueryBuffer,
				 IN uint32_t u4QueryBufferLen,
				 OUT uint32_t *pu4QueryInfoLen) {
#if DBG
	enum PARAM_DEVICE_POWER_STATE *prPowerState;
#endif

	DEBUGFUNC("wlanoidQueryAcpiDevicePowerState");
	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(enum PARAM_DEVICE_POWER_STATE);

#if DBG
	prPowerState = (enum PARAM_DEVICE_POWER_STATE *)
		       pvQueryBuffer;
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
	*(enum PARAM_DEVICE_POWER_STATE *) pvQueryBuffer =
		ParamDeviceStateD3;
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
uint32_t
wlanoidSetAcpiDevicePowerState(IN struct ADAPTER *
			       prAdapter,
			       IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			       OUT uint32_t *pu4SetInfoLen) {
	enum PARAM_DEVICE_POWER_STATE *prPowerState;
	u_int8_t fgRetValue = TRUE;

	DEBUGFUNC("wlanoidSetAcpiDevicePowerState");
	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(enum PARAM_DEVICE_POWER_STATE);

	ASSERT(pvSetBuffer);
	prPowerState = (enum PARAM_DEVICE_POWER_STATE *)
		       pvSetBuffer;
	switch (*prPowerState) {
	case ParamDeviceStateD0:
		DBGLOG(REQ, INFO, "Set Power State: D0\n");
		kalDevSetPowerState(prAdapter->prGlueInfo,
				    (uint32_t) ParamDeviceStateD0);
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
		kalDevSetPowerState(prAdapter->prGlueInfo,
				    (uint32_t) ParamDeviceStateD3);
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
uint32_t
wlanoidQueryFragThreshold(IN struct ADAPTER *prAdapter,
			  OUT void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
			  OUT uint32_t *pu4QueryInfoLen) {
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
uint32_t
wlanoidSetFragThreshold(IN struct ADAPTER *prAdapter,
			IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			OUT uint32_t *pu4SetInfoLen) {
#if CFG_TX_FRAGMENT
	return WLAN_STATUS_SUCCESS;
#else
	return WLAN_STATUS_NOT_SUPPORTED;
#endif /* CFG_TX_FRAGMENT */

} /* end of wlanoidSetFragThreshold() */

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
uint32_t
wlanoidQueryRtsThreshold(IN struct ADAPTER *prAdapter,
			 OUT void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
			 OUT uint32_t *pu4QueryInfoLen) {
	DEBUGFUNC("wlanoidQueryRtsThreshold");

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	DBGLOG(REQ, LOUD, "\n");

	if (u4QueryBufferLen < sizeof(uint32_t)) {
		*pu4QueryInfoLen = sizeof(uint32_t);
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}

	*((uint32_t *) pvQueryBuffer) =
		prAdapter->rWlanInfo.eRtsThreshold;

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
uint32_t
wlanoidSetRtsThreshold(IN struct ADAPTER *prAdapter,
		       IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		       OUT uint32_t *pu4SetInfoLen) {
	uint32_t *prRtsThreshold;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(uint32_t);
	if (u4SetBufferLen < sizeof(uint32_t)) {
		DBGLOG(REQ, WARN, "Invalid length %u\n", u4SetBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	prRtsThreshold = (uint32_t *) pvSetBuffer;
	*prRtsThreshold = prAdapter->rWlanInfo.eRtsThreshold;

	return WLAN_STATUS_SUCCESS;

} /* wlanoidSetRtsThreshold */

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
uint32_t
wlanoidSetDisassociate(IN struct ADAPTER *prAdapter,
		       IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		       OUT uint32_t *pu4SetInfoLen) {
	struct MSG_AIS_ABORT *prAisAbortMsg;

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
	prAdapter->rWifiVar.rConnSettings.eReConnectLevel =
		RECONNECT_LEVEL_USER_SET;

	/* Send AIS Abort Message */
	prAisAbortMsg = (struct MSG_AIS_ABORT *) cnmMemAlloc(
						prAdapter, RAM_TYPE_MSG,
						sizeof(struct MSG_AIS_ABORT));
	if (!prAisAbortMsg) {
		DBGLOG(REQ, ERROR, "Fail in creating AisAbortMsg.\n");
		return WLAN_STATUS_FAILURE;
	}

	prAisAbortMsg->rMsgHdr.eMsgId = MID_OID_AIS_FSM_JOIN_REQ;
	prAisAbortMsg->ucReasonOfDisconnect =
		DISCONNECT_REASON_CODE_NEW_CONNECTION;
	prAisAbortMsg->fgDelayIndication = FALSE;

#if CFG_DISCONN_DEBUG_FEATURE
	/* used to disconnect debug capability */
	g_rDisconnInfoTemp.ucTrigger = DISCONNECT_TRIGGER_ACTIVE;
#endif

	mboxSendMsg(prAdapter, MBOX_ID_0,
		    (struct MSG_HDR *) prAisAbortMsg, MSG_SEND_METHOD_BUF);

	/* indicate for disconnection */
	if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) ==
	    PARAM_MEDIA_STATE_CONNECTED)
		kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
			     WLAN_STATUS_MEDIA_DISCONNECT_LOCALLY, NULL, 0);
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
uint32_t
wlanoidQuery802dot11PowerSaveProfile(IN struct ADAPTER *prAdapter,
				     IN void *pvQueryBuffer,
				     IN uint32_t u4QueryBufferLen,
				     OUT uint32_t *pu4QueryInfoLen) {
	DEBUGFUNC("wlanoidQuery802dot11PowerSaveProfile");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);

	if (u4QueryBufferLen != 0) {
		ASSERT(pvQueryBuffer);

		/* *(PPARAM_POWER_MODE) pvQueryBuffer = (PARAM_POWER_MODE)
		 *	(prAdapter->rWlanInfo.ePowerSaveMode.ucPsProfile);
		 */
		*(enum PARAM_POWER_MODE *) pvQueryBuffer =
			(enum PARAM_POWER_MODE) (
			prAdapter->rWlanInfo.arPowerSaveMode[
			    prAdapter->prAisBssInfo->ucBssIndex].ucPsProfile);
		*pu4QueryInfoLen = sizeof(enum PARAM_POWER_MODE);

		/* hack for CTIA power mode setting function */
		if (prAdapter->fgEnCtiaPowerMode) {
			/* set to non-zero value (to prevent MMI query 0, */
			/* before it intends to set 0, which will skip its
			 * following state machine)
			 */
			*(enum PARAM_POWER_MODE *) pvQueryBuffer =
				(enum PARAM_POWER_MODE) 2;
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
uint32_t
wlanoidSet802dot11PowerSaveProfile(IN struct ADAPTER *
				   prAdapter,
				   IN void *pvSetBuffer,
				   IN uint32_t u4SetBufferLen,
				   OUT uint32_t *pu4SetInfoLen) {
	uint32_t status;
	struct PARAM_POWER_MODE_ *prPowerMode;
	struct BSS_INFO *prBssInfo;

	const uint8_t *apucPsMode[Param_PowerModeMax] = {
		(uint8_t *) "CAM",
		(uint8_t *) "MAX PS",
		(uint8_t *) "FAST PS"
	};

	DEBUGFUNC("wlanoidSet802dot11PowerSaveProfile");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(struct PARAM_POWER_MODE_);
	prPowerMode = (struct PARAM_POWER_MODE_ *) pvSetBuffer;

	if (u4SetBufferLen < sizeof(struct PARAM_POWER_MODE_)) {
		DBGLOG(REQ, WARN,
		       "Set power mode error: Invalid length %u\n",
		       u4SetBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	} else if (prPowerMode->ePowerMode >= Param_PowerModeMax) {
		DBGLOG(REQ, WARN,
		       "Set power mode error: Invalid power mode(%u)\n",
		       prPowerMode->ePowerMode);
		return WLAN_STATUS_INVALID_DATA;
	} else if (prPowerMode->ucBssIdx >=
		   prAdapter->ucHwBssIdNum) {
		DBGLOG(REQ, WARN,
		       "Set power mode error: Invalid BSS index(%u)\n",
		       prPowerMode->ucBssIdx);
		return WLAN_STATUS_INVALID_DATA;
	}

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
					  prPowerMode->ucBssIdx);

	if (prAdapter->fgEnCtiaPowerMode) {
		if (prPowerMode->ePowerMode != Param_PowerModeCAM) {
			/* User setting to PS mode (Param_PowerModeMAX_PSP or
			 * Param_PowerModeFast_PSP)
			 */

			if (prAdapter->u4CtiaPowerMode == 0)
				/* force to keep in CAM mode */
				prPowerMode->ePowerMode = Param_PowerModeCAM;
			else if (prAdapter->u4CtiaPowerMode == 1)
				prPowerMode->ePowerMode =
							Param_PowerModeMAX_PSP;
			else if (prAdapter->u4CtiaPowerMode == 2)
				prPowerMode->ePowerMode =
							Param_PowerModeFast_PSP;
		}
	}

	/* only CAM mode allowed when TP/Sigma on */
	if ((prAdapter->rWifiVar.ucTpTestMode ==
	     ENUM_TP_TEST_MODE_THROUGHPUT) ||
	    (prAdapter->rWifiVar.ucTpTestMode ==
	     ENUM_TP_TEST_MODE_SIGMA_AC_N_PMF))
		prPowerMode->ePowerMode = Param_PowerModeCAM;
	else if (prAdapter->rWifiVar.ePowerMode !=
		 Param_PowerModeMax)
		prPowerMode->ePowerMode = prAdapter->rWifiVar.ePowerMode;

	/* for WMM PS Sigma certification, keep WiFi in ps mode continuously */
	/* force PS == Param_PowerModeMAX_PSP */
	if ((prAdapter->rWifiVar.ucTpTestMode ==
	     ENUM_TP_TEST_MODE_SIGMA_WMM_PS) &&
	    (prPowerMode->ePowerMode >= Param_PowerModeMAX_PSP))
		prPowerMode->ePowerMode = Param_PowerModeMAX_PSP;

	status = nicConfigPowerSaveProfile(prAdapter, prPowerMode->ucBssIdx,
					   prPowerMode->ePowerMode,
					   g_fgIsOid, PS_CALLER_COMMON);

	if (prPowerMode->ePowerMode < Param_PowerModeMax) {
		DBGLOG(INIT, TRACE,
		       "Set %s Network BSS(%u) PS mode to %s (%d)\n",
		       apucNetworkType[prBssInfo->eNetworkType],
		       prPowerMode->ucBssIdx,
		       apucPsMode[prPowerMode->ePowerMode],
		       prPowerMode->ePowerMode);
	} else {
		DBGLOG(INIT, TRACE,
		       "Invalid PS mode setting (%d) for %s Network BSS(%u)\n",
		       prPowerMode->ePowerMode,
		       apucNetworkType[prBssInfo->eNetworkType],
		       prPowerMode->ucBssIdx);
	}

	return status;

} /* end of wlanoidSetAcpiDevicePowerStateMode() */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to query current status of AdHoc Mode.
 *
 * \param[in] prAdapter          Pointer to the Adapter structure.
 * \param[in] pvQueryBuffer      Pointer to the buffer that holds the result of
 *                               the query.
 * \param[in] u4QueryBufferLen   The length of the query buffer.
 * \param[out] pu4QueryInfoLen   If the call is successful, returns the number
 *				 of bytes written into the query buffer. If the
 *				 call failed due to invalid length of the query
 *				 buffer, returns the amount of storage needed.
 *
 * \retval WLAN_STATUS_SUCCESS
 * \retval WLAN_STATUS_BUFFER_TOO_SHORT
 */
/*----------------------------------------------------------------------------*/
uint32_t
wlanoidQueryAdHocMode(IN struct ADAPTER *prAdapter,
		      OUT void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
		      OUT uint32_t *pu4QueryInfoLen) {
	return WLAN_STATUS_SUCCESS;
}				/* end of wlanoidQueryAdHocMode() */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to set AdHoc Mode.
 *
 * \param[in] prAdapter      Pointer to the Adapter structure.
 * \param[in] pvSetBuffer    Pointer to the buffer that holds the data to be
 *			     set.
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
uint32_t
wlanoidSetAdHocMode(IN struct ADAPTER *prAdapter,
		    IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		    OUT uint32_t *pu4SetInfoLen) {
	return WLAN_STATUS_SUCCESS;
} /* end of wlanoidSetAdHocMode() */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to query RF frequency.
 *
 * \param[in] prAdapter          Pointer to the Adapter structure.
 * \param[in] pvQueryBuffer      Pointer to the buffer that holds the result of
 *                               the query.
 * \param[in] u4QueryBufferLen   The length of the query buffer.
 * \param[out] pu4QueryInfoLen   If the call is successful, returns the number
 *				 of bytes written into the query buffer. If the
 *				 call failed due to invalid length of the query
 *				 buffer, returns the amount of storage needed.
 *
 * \retval WLAN_STATUS_SUCCESS
 * \retval WLAN_STATUS_BUFFER_TOO_SHORT
 */
/*----------------------------------------------------------------------------*/
uint32_t
wlanoidQueryFrequency(IN struct ADAPTER *prAdapter,
		      OUT void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
		      OUT uint32_t *pu4QueryInfoLen) {
	DEBUGFUNC("wlanoidQueryFrequency");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (u4QueryBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (prAdapter->rWifiVar.rConnSettings.eOPMode ==
	    NET_TYPE_INFRA) {
		if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) ==
		    PARAM_MEDIA_STATE_CONNECTED)
			*(uint32_t *) pvQueryBuffer = nicChannelNum2Freq(
				prAdapter->prAisBssInfo->ucPrimaryChannel);
		else
			*(uint32_t *) pvQueryBuffer = 0;
	} else
		*(uint32_t *) pvQueryBuffer = nicChannelNum2Freq(
			prAdapter->rWifiVar.rConnSettings.ucAdHocChannelNum);

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
 * \param[out] pu4QueryInfoLen   If the call is successful, returns the number
 *				 of bytes written into the query buffer. If the
 *				 call failed due to invalid length of the query
 *				 buffer, returns the amount of storage needed.
 *
 * \retval WLAN_STATUS_SUCCESS
 * \retval WLAN_STATUS_INVALID_LENGTH
 * \retval WLAN_STATUS_INVALID_DATA
 */
/*----------------------------------------------------------------------------*/
uint32_t
wlanoidSetFrequency(IN struct ADAPTER *prAdapter,
		    IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		    OUT uint32_t *pu4SetInfoLen) {
	uint32_t *pu4FreqInKHz;

	DEBUGFUNC("wlanoidSetFrequency");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(uint32_t);

	if (u4SetBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);
	pu4FreqInKHz = (uint32_t *) pvSetBuffer;

	prAdapter->rWifiVar.rConnSettings.ucAdHocChannelNum =
		(uint8_t) nicFreq2ChannelNum(*pu4FreqInKHz);
	prAdapter->rWifiVar.rConnSettings.eAdHocBand = *pu4FreqInKHz
			< 5000000 ? BAND_2G4 : BAND_5G;

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
uint32_t
wlanoidSetChannel(IN struct ADAPTER *prAdapter,
		  IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		  OUT uint32_t *pu4SetInfoLen) {
	ASSERT(0);		/* // */

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to query the Beacon Interval from User
 *	  Settings.
 *
 * \param[in] prAdapter          Pointer to the Adapter structure.
 * \param[in] pvQueryBuffer      Pointer to the buffer that holds the result of
 *                               the query.
 * \param[in] u4QueryBufferLen   The length of the query buffer.
 * \param[out] pu4QueryInfoLen   If the call is successful, returns the number
 *				 of bytes written into the query buffer. If the
 *				 call failed due to invalid length of the query
 *				 buffer, returns the amount of storage needed.
 *
 * \retval WLAN_STATUS_SUCCESS
 * \retval WLAN_STATUS_BUFFER_TOO_SHORT
 */
/*----------------------------------------------------------------------------*/
uint32_t
wlanoidQueryBeaconInterval(IN struct ADAPTER *prAdapter,
			   OUT void *pvQueryBuffer,
			   IN uint32_t u4QueryBufferLen,
			   OUT uint32_t *pu4QueryInfoLen) {
	DEBUGFUNC("wlanoidQueryBeaconInterval");
	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(uint32_t);

	if (u4QueryBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) ==
	    PARAM_MEDIA_STATE_CONNECTED) {
		if (prAdapter->rWifiVar.rConnSettings.eOPMode ==
		    NET_TYPE_INFRA)
			*(uint32_t *) pvQueryBuffer =
				prAdapter->rWlanInfo.rCurrBssId.rConfiguration
				.u4BeaconPeriod;
		else
			*(uint32_t *) pvQueryBuffer =
				(uint32_t)prAdapter->rWlanInfo.u2BeaconPeriod;
	} else {
		if (prAdapter->rWifiVar.rConnSettings.eOPMode ==
		    NET_TYPE_INFRA)
			*(uint32_t *) pvQueryBuffer = 0;
		else
			*(uint32_t *) pvQueryBuffer =
				(uint32_t)prAdapter->rWlanInfo.u2BeaconPeriod;
	}

	return WLAN_STATUS_SUCCESS;
}				/* end of wlanoidQueryBeaconInterval() */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to set the Beacon Interval to User Settings.
 *
 * \param[in] prAdapter      Pointer to the Adapter structure.
 * \param[in] pvSetBuffer    Pointer to the buffer that holds the data to be
 *			     set.
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
uint32_t
wlanoidSetBeaconInterval(IN struct ADAPTER *prAdapter,
			 IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			 OUT uint32_t *pu4SetInfoLen) {
	uint32_t *pu4BeaconInterval;

	DEBUGFUNC("wlanoidSetBeaconInterval");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(uint32_t);
	if (u4SetBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);
	pu4BeaconInterval = (uint32_t *) pvSetBuffer;

	if ((*pu4BeaconInterval < DOT11_BEACON_PERIOD_MIN)
	    || (*pu4BeaconInterval > DOT11_BEACON_PERIOD_MAX)) {
		DBGLOG(REQ, TRACE, "Invalid Beacon Interval = %u\n",
		       *pu4BeaconInterval);
		return WLAN_STATUS_INVALID_DATA;
	}

	prAdapter->rWlanInfo.u2BeaconPeriod = (uint16_t) *
					      pu4BeaconInterval;

	DBGLOG(REQ, INFO, "Set beacon interval: %d\n",
	       prAdapter->rWlanInfo.u2BeaconPeriod);

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
 * \param[out] pu4QueryInfoLen   If the call is successful, returns the number
 *				 of bytes written into the query buffer. If the
 *				 call failed due to invalid length of the query
 *				 buffer, returns the amount of storage needed.
 *
 * \retval WLAN_STATUS_SUCCESS
 * \retval WLAN_STATUS_BUFFER_TOO_SHORT
 */
/*----------------------------------------------------------------------------*/
uint32_t
wlanoidQueryAtimWindow(IN struct ADAPTER *prAdapter,
		       OUT void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
		       OUT uint32_t *pu4QueryInfoLen) {
	DEBUGFUNC("wlanoidQueryAtimWindow");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(uint32_t);

	if (u4QueryBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (prAdapter->rWifiVar.rConnSettings.eOPMode ==
	    NET_TYPE_INFRA)
		*(uint32_t *) pvQueryBuffer = 0;
	else
		*(uint32_t *) pvQueryBuffer = (uint32_t)
					      prAdapter->rWlanInfo.u2AtimWindow;

	return WLAN_STATUS_SUCCESS;

}				/* end of wlanoidQueryAtimWindow() */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to set the ATIM window to User Settings.
 *
 * \param[in] prAdapter      Pointer to the Adapter structure.
 * \param[in] pvSetBuffer    Pointer to the buffer that holds the data to be
 *			     set.
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
uint32_t
wlanoidSetAtimWindow(IN struct ADAPTER *prAdapter,
		     IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		     OUT uint32_t *pu4SetInfoLen) {
	uint32_t *pu4AtimWindow;

	DEBUGFUNC("wlanoidSetAtimWindow");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(uint32_t);

	if (u4SetBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);
	pu4AtimWindow = (uint32_t *) pvSetBuffer;

	prAdapter->rWlanInfo.u2AtimWindow = (uint16_t) *
					    pu4AtimWindow;

	return WLAN_STATUS_SUCCESS;
}				/* end of wlanoidSetAtimWindow() */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to Set the MAC address which is currently used
 *	  by the NIC.
 *
 * \param[in] prAdapter      Pointer to the Adapter structure.
 * \param[in] pvSetBuffer    Pointer to the buffer that holds the data to be
 *			     set.
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
uint32_t
wlanoidSetCurrentAddr(IN struct ADAPTER *prAdapter,
		      IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		      OUT uint32_t *pu4SetInfoLen) {
	ASSERT(0);		/* // */

	return WLAN_STATUS_SUCCESS;
}				/* end of wlanoidSetCurrentAddr() */

#if CFG_TCP_IP_CHKSUM_OFFLOAD
/*----------------------------------------------------------------------------*/
/*!
 * \brief Setting the checksum offload function.
 *
 * \param[in] prAdapter      Pointer to the Adapter structure.
 * \param[in] pvSetBuffer    Pointer to the buffer that holds the data to be
 *			     set.
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
uint32_t
wlanoidSetCSUMOffload(IN struct ADAPTER *prAdapter,
		      IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		      OUT uint32_t *pu4SetInfoLen) {
	uint32_t u4CSUMFlags;
	struct CMD_BASIC_CONFIG rCmdBasicConfig;
	struct WIFI_VAR *prWifiVar = &prAdapter->rWifiVar;

	DEBUGFUNC("wlanoidSetCSUMOffload");
	DBGLOG(INIT, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(uint32_t);

	if (u4SetBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);
	u4CSUMFlags = *(uint32_t *) pvSetBuffer;

	kalMemZero(&rCmdBasicConfig,
		   sizeof(struct CMD_BASIC_CONFIG));

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

	if (u4CSUMFlags & (CSUM_OFFLOAD_EN_RX_IPv4 |
			   CSUM_OFFLOAD_EN_RX_IPv6))
		rCmdBasicConfig.rCsumOffload.u2RxChecksum |= BIT(0);

	prAdapter->u4CSUMFlags = u4CSUMFlags;
	rCmdBasicConfig.ucCtrlFlagAssertPath =
		prWifiVar->ucCtrlFlagAssertPath;
	rCmdBasicConfig.ucCtrlFlagDebugLevel =
		prWifiVar->ucCtrlFlagDebugLevel;

	wlanSendSetQueryCmd(prAdapter,
			    CMD_ID_BASIC_CONFIG,
			    TRUE,
			    FALSE,
			    g_fgIsOid,
			    NULL,
			    nicOidCmdTimeoutCommon,
			    sizeof(struct CMD_BASIC_CONFIG),
			    (uint8_t *) &rCmdBasicConfig,
			    pvSetBuffer, u4SetBufferLen);

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
uint32_t
wlanoidSetNetworkAddress(IN struct ADAPTER *prAdapter,
			 IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			 OUT uint32_t *pu4SetInfoLen) {
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t i, u4IPv4AddrIdx;
	struct CMD_SET_NETWORK_ADDRESS_LIST
		*prCmdNetworkAddressList;
	struct PARAM_NETWORK_ADDRESS_LIST *prNetworkAddressList =
		(struct PARAM_NETWORK_ADDRESS_LIST *) pvSetBuffer;
	struct PARAM_NETWORK_ADDRESS *prNetworkAddress;
	uint32_t u4IPv4AddrCount, u4CmdSize;
#if CFG_ENABLE_GTK_FRAME_FILTER
	uint32_t u4IpV4AddrListSize;
	struct BSS_INFO *prBssInfo =
		&prAdapter->rWifiVar.arBssInfoPool[KAL_NETWORK_TYPE_AIS_INDEX];
#endif

	DEBUGFUNC("wlanoidSetNetworkAddress");
	DBGLOG(INIT, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = 4;

	if (u4SetBufferLen < OFFSET_OF(struct
				       PARAM_NETWORK_ADDRESS_LIST, arAddress))
		return WLAN_STATUS_INVALID_DATA;

	*pu4SetInfoLen = 0;
	u4IPv4AddrCount = 0;

	/* 4 <1.1> Get IPv4 address count */
	/* We only suppot IPv4 address setting */
	prNetworkAddress = prNetworkAddressList->arAddress;
	for (i = 0; i < prNetworkAddressList->u4AddressCount; i++) {
		if ((prNetworkAddress->u2AddressType ==
		     PARAM_PROTOCOL_ID_TCP_IP) &&
		    (prNetworkAddress->u2AddressLength == IPV4_ADDR_LEN)) {
			u4IPv4AddrCount++;
		}

		prNetworkAddress = (struct PARAM_NETWORK_ADDRESS *)
			((unsigned long) prNetworkAddress +
			(unsigned long) (prNetworkAddress->u2AddressLength +
			OFFSET_OF(struct PARAM_NETWORK_ADDRESS, aucAddress)));
	}

	/* 4 <2> Calculate command buffer size */
	/* construct payload of command packet */
	if (u4IPv4AddrCount == 0)
		u4CmdSize = sizeof(struct CMD_SET_NETWORK_ADDRESS_LIST);
	else
		u4CmdSize =
			OFFSET_OF(struct CMD_SET_NETWORK_ADDRESS_LIST,
				  arNetAddress) +
			(sizeof(struct IPV4_NETWORK_ADDRESS) *
			u4IPv4AddrCount);

	/* 4 <3> Allocate command buffer */
	prCmdNetworkAddressList = (struct CMD_SET_NETWORK_ADDRESS_LIST *)
					kalMemAlloc(u4CmdSize, VIR_MEM_TYPE);

	if (prCmdNetworkAddressList == NULL)
		return WLAN_STATUS_FAILURE;

#if CFG_ENABLE_GTK_FRAME_FILTER
	u4IpV4AddrListSize =
			OFFSET_OF(struct IPV4_NETWORK_ADDRESS_LIST, arNetAddr) +
			(u4IPv4AddrCount * sizeof(struct IPV4_NETWORK_ADDRESS));
	if (prBssInfo->prIpV4NetAddrList)
		FREE_IPV4_NETWORK_ADDR_LIST(prBssInfo->prIpV4NetAddrList);
	prBssInfo->prIpV4NetAddrList =
			(struct IPV4_NETWORK_ADDRESS_LIST *)
			kalMemAlloc(u4IpV4AddrListSize,
				    VIR_MEM_TYPE);
	prBssInfo->prIpV4NetAddrList->ucAddrCount =
			(uint8_t) u4IPv4AddrCount;
#endif

	/* 4 <4> Fill P_CMD_SET_NETWORK_ADDRESS_LIST */
	prCmdNetworkAddressList->ucBssIndex =
		prNetworkAddressList->ucBssIdx;

	/* only to set IP address to FW once ARP filter is enabled */
	if (prAdapter->fgEnArpFilter) {
		prCmdNetworkAddressList->ucAddressCount =
			(uint8_t) u4IPv4AddrCount;
		prNetworkAddress = prNetworkAddressList->arAddress;

		/* DBGLOG(INIT, INFO, ("%s: u4IPv4AddrCount (%lu)\n",
		 *        __FUNCTION__, u4IPv4AddrCount));
		 */

		for (i = 0, u4IPv4AddrIdx = 0;
		     i < prNetworkAddressList->u4AddressCount; i++) {
			if (prNetworkAddress->u2AddressType ==
			    PARAM_PROTOCOL_ID_TCP_IP &&
			    prNetworkAddress->u2AddressLength ==
			    IPV4_ADDR_LEN) {

				kalMemCopy(prCmdNetworkAddressList->
					arNetAddress[u4IPv4AddrIdx].aucIpAddr,
					prNetworkAddress->aucAddress,
					sizeof(uint32_t));

#if CFG_ENABLE_GTK_FRAME_FILTER
				kalMemCopy(prBssInfo->prIpV4NetAddrList->
					arNetAddr[u4IPv4AddrIdx].aucIpAddr,
					prNetworkAddress->aucAddress,
					sizeof(uint32_t));
#endif

				DBGLOG(INIT, INFO,
				       "%s: IPv4 Addr [%u][" IPV4STR "]\n",
				       __func__, u4IPv4AddrIdx,
				       IPV4TOSTR(prNetworkAddress->aucAddress));

				u4IPv4AddrIdx++;
			}

			prNetworkAddress = (struct PARAM_NETWORK_ADDRESS *)
			    ((unsigned long)prNetworkAddress +
			    (unsigned long)(prNetworkAddress->u2AddressLength +
			    OFFSET_OF(struct PARAM_NETWORK_ADDRESS,
				      aucAddress)));
		}

	} else {
		prCmdNetworkAddressList->ucAddressCount = 0;
	}

	DBGLOG(INIT, INFO,
	       "%s: Set %u IPv4 address for BSS[%u]\n", __func__,
	       u4IPv4AddrCount,
	       prCmdNetworkAddressList->ucBssIndex);

	/* 4 <5> Send command */
	rStatus = wlanSendSetQueryCmd(prAdapter,
				      CMD_ID_SET_IP_ADDRESS,
				      TRUE,
				      FALSE,
				      g_fgIsOid,
				      nicCmdEventSetIpAddress,
				      nicOidCmdTimeoutCommon,
				      u4CmdSize,
				      (uint8_t *) prCmdNetworkAddressList,
				      pvSetBuffer,
				      u4SetBufferLen);

	kalMemFree(prCmdNetworkAddressList, VIR_MEM_TYPE,
		   u4CmdSize);
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
uint32_t
wlanoidRftestSetTestMode(IN struct ADAPTER *prAdapter,
			 IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			 OUT uint32_t *pu4SetInfoLen) {
	uint32_t rStatus;
	struct CMD_TEST_CTRL rCmdTestCtrl;

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
						sizeof(struct CMD_TEST_CTRL),
						(uint8_t *) &rCmdTestCtrl,
						pvSetBuffer, u4SetBufferLen);
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
uint32_t
wlanoidRftestSetTestIcapMode(IN struct ADAPTER *prAdapter,
			     IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			     OUT uint32_t *pu4SetInfoLen) {
	uint32_t rStatus;
	struct CMD_TEST_CTRL rCmdTestCtrl;

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
					      sizeof(struct CMD_TEST_CTRL),
					      (uint8_t *) &rCmdTestCtrl,
					      pvSetBuffer, u4SetBufferLen);
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
uint32_t
wlanoidRftestSetAbortTestMode(IN struct ADAPTER *prAdapter,
			      IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			      OUT uint32_t *pu4SetInfoLen) {
	uint32_t rStatus;
	struct CMD_TEST_CTRL rCmdTestCtrl;

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
						sizeof(struct CMD_TEST_CTRL),
						(uint8_t *) &rCmdTestCtrl,
						pvSetBuffer, u4SetBufferLen);
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
 * \param[out] pu4QueryInfoLen   If the call is successful, returns the number
 *				 of bytes written into the query buffer. If the
 *				 call failed due to invalid length of the query
 *				 buffer, returns the amount of storage needed.
 *
 * \retval WLAN_STATUS_SUCCESS
 * \retval WLAN_STATUS_BUFFER_TOO_SHORT
 * \retval WLAN_STATUS_NOT_SUPPORTED
 * \retval WLAN_STATUS_NOT_ACCEPTED
 */
/*----------------------------------------------------------------------------*/
uint32_t
wlanoidRftestQueryAutoTest(IN struct ADAPTER *prAdapter,
			   OUT void *pvQueryBuffer,
			   IN uint32_t u4QueryBufferLen,
			   OUT uint32_t *pu4QueryInfoLen) {
	struct PARAM_MTK_WIFI_TEST_STRUCT *prRfATInfo;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidRftestQueryAutoTest");

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	*pu4QueryInfoLen = sizeof(struct
				  PARAM_MTK_WIFI_TEST_STRUCT);

#if 0 /* PeiHsuan Temp Remove this check for workaround Gen2/Gen3 EM Mode
       * Modification
       */
	if (u4QueryBufferLen != sizeof(struct PARAM_MTK_WIFI_TEST_STRUCT)) {
		DBGLOG(REQ, ERROR, "Invalid data. QueryBufferLen: %ld.\n",
		       u4QueryBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}
#endif

	prRfATInfo = (struct PARAM_MTK_WIFI_TEST_STRUCT *)
		     pvQueryBuffer;

	DBGLOG(RFTEST, INFO,
	       "Get AT_CMD BufferLen = %d, AT Index = %d, Data = %d\n",
	       u4QueryBufferLen,
	       prRfATInfo->u4FuncIndex,
	       prRfATInfo->u4FuncData);

	rStatus = rftestQueryATInfo(prAdapter,
				    prRfATInfo->u4FuncIndex,
				    prRfATInfo->u4FuncData,
				    pvQueryBuffer, u4QueryBufferLen);

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
uint32_t
wlanoidRftestSetAutoTest(IN struct ADAPTER *prAdapter,
			 OUT void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			 OUT uint32_t *pu4SetInfoLen) {
	struct PARAM_MTK_WIFI_TEST_STRUCT *prRfATInfo;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidRftestSetAutoTest");

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(struct PARAM_MTK_WIFI_TEST_STRUCT);

#if 0 /* PeiHsuan Temp Remove this check for workaround Gen2/Gen3 EM Mode
       * Modification
       */
	if (u4SetBufferLen != sizeof(struct
				     PARAM_MTK_WIFI_TEST_STRUCT)) {
		DBGLOG(REQ, ERROR, "Invalid data. SetBufferLen: %ld.\n",
		       u4SetBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}
#endif

	prRfATInfo = (struct PARAM_MTK_WIFI_TEST_STRUCT *)
		     pvSetBuffer;

	DBGLOG(RFTEST, INFO,
	       "Set AT_CMD BufferLen = %d, AT Index = %d, Data = %d\n",
	       u4SetBufferLen,
	       prRfATInfo->u4FuncIndex,
	       prRfATInfo->u4FuncData);

	rStatus = rftestSetATInfo(prAdapter,
			  prRfATInfo->u4FuncIndex, prRfATInfo->u4FuncData);

	return rStatus;
}

/* RF test OID set handler */
uint32_t rftestSetATInfo(IN struct ADAPTER *prAdapter,
			 uint32_t u4FuncIndex, uint32_t u4FuncData) {
	struct GLUE_INFO *prGlueInfo;
	struct CMD_INFO *prCmdInfo;
	struct WIFI_CMD *prWifiCmd;
	struct CMD_TEST_CTRL *pCmdTestCtrl;
	uint8_t ucCmdSeqNum;

	ASSERT(prAdapter);

	prGlueInfo = prAdapter->prGlueInfo;
	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter,
				(CMD_HDR_SIZE + sizeof(struct CMD_TEST_CTRL)));

	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}
	/* increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

	/* Setup common CMD Info Packet */
	prCmdInfo->eCmdType = COMMAND_TYPE_GENERAL_IOCTL;
	prCmdInfo->u2InfoBufLen = CMD_HDR_SIZE + sizeof(
					  struct CMD_TEST_CTRL);
	prCmdInfo->pfCmdDoneHandler = nicCmdEventSetCommon;
	prCmdInfo->pfCmdTimeoutHandler = nicOidCmdTimeoutCommon;
	prCmdInfo->fgIsOid = g_fgIsOid;
	prCmdInfo->ucCID = CMD_ID_TEST_CTRL;
	prCmdInfo->fgSetQuery = TRUE;
	prCmdInfo->fgNeedResp = FALSE;
	prCmdInfo->ucCmdSeqNum = ucCmdSeqNum;
	prCmdInfo->u4SetInfoLen = sizeof(struct CMD_TEST_CTRL);
	prCmdInfo->pvInformationBuffer = NULL;
	prCmdInfo->u4InformationBufferLength = 0;

	/* Setup WIFI_CMD_T (payload = CMD_TEST_CTRL_T) */
	prWifiCmd = (struct WIFI_CMD *) (prCmdInfo->pucInfoBuffer);
	prWifiCmd->u2TxByteCount = prCmdInfo->u2InfoBufLen;
	prWifiCmd->u2PQ_ID = CMD_PQ_ID;
	prWifiCmd->ucPktTypeID = CMD_PACKET_TYPE_ID;
	prWifiCmd->ucCID = prCmdInfo->ucCID;
	prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
	prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;

	pCmdTestCtrl = (struct CMD_TEST_CTRL *) (
			       prWifiCmd->aucBuffer);
	pCmdTestCtrl->ucAction = 1;	/* Set ATInfo */
	pCmdTestCtrl->u.rRfATInfo.u4FuncIndex = u4FuncIndex;
	pCmdTestCtrl->u.rRfATInfo.u4FuncData = u4FuncData;

	if ((u4FuncIndex == RF_AT_FUNCID_COMMAND)
	    && (u4FuncData == RF_AT_COMMAND_ICAP)) {
		prAdapter->rIcapInfo.fgIcapEnable = TRUE;
		prAdapter->rIcapInfo.fgCaptureDone = FALSE;
	}
	/* ICAP dump name Reset */
	if ((u4FuncIndex == RF_AT_FUNCID_COMMAND)
	    && (u4FuncData == RF_AT_COMMAND_RESET_DUMP_NAME))
		prAdapter->rIcapInfo.u2DumpIndex = 0;
	/* insert into prCmdQueue */
	kalEnqueueCommand(prGlueInfo,
			  (struct QUE_ENTRY *) prCmdInfo);

	/* wakeup txServiceThread later */
	GLUE_SET_EVENT(prAdapter->prGlueInfo);

	return WLAN_STATUS_PENDING;
}

uint32_t wlanoidExtRfTestICapStart(IN struct ADAPTER *prAdapter,
				   OUT void *pvSetBuffer,
				   IN uint32_t u4SetBufferLen,
				   OUT uint32_t *pu4SetInfoLen) {
	struct CMD_TEST_CTRL_EXT_T rCmdTestCtrl;
	struct RBIST_CAP_START_T *prCmdICapInfo;
	struct PARAM_MTK_WIFI_TEST_STRUCT_EXT_T *prRfATInfo;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidExtRfTestICapStart");

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(struct
				PARAM_MTK_WIFI_TEST_STRUCT_EXT_T);

	prRfATInfo = (struct PARAM_MTK_WIFI_TEST_STRUCT_EXT_T *)
		     pvSetBuffer;

	DBGLOG(RFTEST, INFO,
	       "Set AT_CMD BufferLen = %d, AT Index = %d\n",
	       u4SetBufferLen,
	       prRfATInfo->u4FuncIndex);

	rCmdTestCtrl.ucAction = ACTION_IN_RFTEST;
	rCmdTestCtrl.u.rRfATInfo.u4FuncIndex =
		SET_ICAP_CAPTURE_START;

	prCmdICapInfo = &(rCmdTestCtrl.u.rRfATInfo.Data.rICapInfo);
	kalMemCopy(prCmdICapInfo, &(prRfATInfo->Data.rICapInfo),
		   sizeof(struct RBIST_CAP_START_T));

	prAdapter->rIcapInfo.fgIcapEnable = TRUE;
	prAdapter->rIcapInfo.fgCaptureDone = FALSE;

	rStatus = wlanSendSetQueryExtCmd(prAdapter,
			 CMD_ID_LAYER_0_EXT_MAGIC_NUM,
			 EXT_CMD_ID_RF_TEST,
			 TRUE, /* Query Bit: True->write False->read */
			 FALSE,
			 g_fgIsOid,
			 nicCmdEventSetCommon,
			 nicOidCmdTimeoutCommon,
			 sizeof(struct CMD_TEST_CTRL_EXT_T),
			 (uint8_t *)&rCmdTestCtrl, pvSetBuffer,
			 u4SetBufferLen);
	return rStatus;
}

uint32_t wlanoidExtRfTestICapStatus(IN struct ADAPTER *prAdapter,
				    OUT void *pvSetBuffer,
				    IN uint32_t u4SetBufferLen,
				    OUT uint32_t *pu4SetInfoLen) {
	struct CMD_TEST_CTRL_EXT_T rCmdTestCtrl;
	struct RBIST_CAP_START_T *prCmdICapInfo;
	struct PARAM_MTK_WIFI_TEST_STRUCT_EXT_T *prRfATInfo;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidExtRfTestICapStatus");

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(struct
				PARAM_MTK_WIFI_TEST_STRUCT_EXT_T);

	prRfATInfo = (struct PARAM_MTK_WIFI_TEST_STRUCT_EXT_T *)
		     pvSetBuffer;

	DBGLOG(RFTEST, INFO,
	       "Set AT_CMD BufferLen = %d, AT Index = %d\n",
	       u4SetBufferLen,
	       prRfATInfo->u4FuncIndex);

	rCmdTestCtrl.ucAction = ACTION_IN_RFTEST;
	rCmdTestCtrl.u.rRfATInfo.u4FuncIndex =
		GET_ICAP_CAPTURE_STATUS;

	prCmdICapInfo = &(rCmdTestCtrl.u.rRfATInfo.Data.rICapInfo);
	kalMemCopy(prCmdICapInfo, &(prRfATInfo->Data.rICapInfo),
		   sizeof(struct RBIST_CAP_START_T));

	rStatus = wlanSendSetQueryExtCmd(prAdapter,
				 CMD_ID_LAYER_0_EXT_MAGIC_NUM,
				 EXT_CMD_ID_RF_TEST,
				 FALSE, /* Query Bit: True->write False->read */
				 TRUE,
				 g_fgIsOid,
				 NULL,
				 nicOidCmdTimeoutCommon,
				 sizeof(struct CMD_TEST_CTRL_EXT_T),
				 (uint8_t *)(&rCmdTestCtrl), pvSetBuffer,
				 u4SetBufferLen);
	return rStatus;
}

void wlanoidRfTestICapRawDataProc(IN struct ADAPTER *
				  prAdapter, uint32_t u4CapStartAddr,
				  uint32_t u4TotalBufferSize) {
	struct CMD_TEST_CTRL_EXT_T rCmdTestCtrl;
	struct PARAM_MTK_WIFI_TEST_STRUCT_EXT_T *prRfATInfo;
	uint32_t u4SetBufferLen = 0;
	void *pvSetBuffer = NULL;
	int32_t rStatus;

	ASSERT(prAdapter);

	prRfATInfo = &(rCmdTestCtrl.u.rRfATInfo);

	rCmdTestCtrl.ucAction = ACTION_IN_RFTEST;
	prRfATInfo->u4FuncIndex = GET_ICAP_RAW_DATA;
	prRfATInfo->Data.rICapDump.u4Address = u4CapStartAddr;
	prRfATInfo->Data.rICapDump.u4AddrOffset = 0x04;
	prRfATInfo->Data.rICapDump.u4Bank = 1;
	prRfATInfo->Data.rICapDump.u4BankSize = u4TotalBufferSize;

	rStatus = wlanSendSetQueryExtCmd(prAdapter,
				 CMD_ID_LAYER_0_EXT_MAGIC_NUM,
				 EXT_CMD_ID_RF_TEST,
				 TRUE, /* Query Bit: True->write False->read */
				 FALSE,
				 FALSE,
				 nicCmdEventSetCommon,
				 nicOidCmdTimeoutCommon,
				 sizeof(struct CMD_TEST_CTRL_EXT_T),
				 (uint8_t *)(&rCmdTestCtrl),
				 pvSetBuffer, u4SetBufferLen);
}

uint32_t
rftestQueryATInfo(IN struct ADAPTER *prAdapter,
		  uint32_t u4FuncIndex, uint32_t u4FuncData,
		  OUT void *pvQueryBuffer, IN uint32_t u4QueryBufferLen) {
	struct GLUE_INFO *prGlueInfo;
	struct CMD_INFO *prCmdInfo;
	struct WIFI_CMD *prWifiCmd;
	struct CMD_TEST_CTRL *pCmdTestCtrl;
	uint8_t ucCmdSeqNum;
	union EVENT_TEST_STATUS *prTestStatus;

	ASSERT(prAdapter);

	prGlueInfo = prAdapter->prGlueInfo;

	if (u4FuncIndex == RF_AT_FUNCID_FW_INFO) {
		/* driver implementation */
		prTestStatus = (union EVENT_TEST_STATUS *) pvQueryBuffer;

		prTestStatus->rATInfo.u4FuncData =
			(prAdapter->rVerInfo.u2FwProductID << 16) |
			(prAdapter->rVerInfo.u2FwOwnVersion);
		u4QueryBufferLen = sizeof(union EVENT_TEST_STATUS);

		return WLAN_STATUS_SUCCESS;
	} else if (u4FuncIndex == RF_AT_FUNCID_DRV_INFO) {
		/* driver implementation */
		prTestStatus = (union EVENT_TEST_STATUS *) pvQueryBuffer;

		prTestStatus->rATInfo.u4FuncData = CFG_DRV_OWN_VERSION;
		u4QueryBufferLen = sizeof(union EVENT_TEST_STATUS);

		return WLAN_STATUS_SUCCESS;
	} else if (u4FuncIndex ==
		   RF_AT_FUNCID_QUERY_ICAP_DUMP_FILE) {
		/* driver implementation */
		prTestStatus = (union EVENT_TEST_STATUS *) pvQueryBuffer;

		prTestStatus->rATInfo.u4FuncData =
			prAdapter->rIcapInfo.u2DumpIndex;
		u4QueryBufferLen = sizeof(union EVENT_TEST_STATUS);

		return WLAN_STATUS_SUCCESS;
	}
	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter,
			  (CMD_HDR_SIZE + sizeof(struct CMD_TEST_CTRL)));

	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}
	/* increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

	/* Setup common CMD Info Packet */
	prCmdInfo->eCmdType = COMMAND_TYPE_GENERAL_IOCTL;
	prCmdInfo->u2InfoBufLen = CMD_HDR_SIZE + sizeof(
					  struct CMD_TEST_CTRL);
	prCmdInfo->pfCmdDoneHandler = nicCmdEventQueryRfTestATInfo;
	prCmdInfo->pfCmdTimeoutHandler = nicOidCmdTimeoutCommon;
	prCmdInfo->fgIsOid = g_fgIsOid;
	prCmdInfo->ucCID = CMD_ID_TEST_CTRL;
	prCmdInfo->fgSetQuery = FALSE;
	prCmdInfo->fgNeedResp = TRUE;
	prCmdInfo->ucCmdSeqNum = ucCmdSeqNum;
	prCmdInfo->u4SetInfoLen = sizeof(struct CMD_TEST_CTRL);
	prCmdInfo->pvInformationBuffer = pvQueryBuffer;
	prCmdInfo->u4InformationBufferLength = u4QueryBufferLen;

	/* Setup WIFI_CMD_T (payload = CMD_TEST_CTRL_T) */
	prWifiCmd = (struct WIFI_CMD *) (prCmdInfo->pucInfoBuffer);
	prWifiCmd->u2TxByteCount = prCmdInfo->u2InfoBufLen;
	prWifiCmd->u2PQ_ID = CMD_PQ_ID;
	prWifiCmd->ucPktTypeID = CMD_PACKET_TYPE_ID;
	prWifiCmd->ucCID = prCmdInfo->ucCID;
	prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
	prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;

	pCmdTestCtrl = (struct CMD_TEST_CTRL *) (
			       prWifiCmd->aucBuffer);
	pCmdTestCtrl->ucAction = 2;	/* Get ATInfo */
	pCmdTestCtrl->u.rRfATInfo.u4FuncIndex = u4FuncIndex;
	pCmdTestCtrl->u.rRfATInfo.u4FuncData = u4FuncData;

	/* insert into prCmdQueue */
	kalEnqueueCommand(prGlueInfo,
			  (struct QUE_ENTRY *) prCmdInfo);

	/* wakeup txServiceThread later */
	GLUE_SET_EVENT(prAdapter->prGlueInfo);

	return WLAN_STATUS_PENDING;

}

uint32_t rftestSetFrequency(IN struct ADAPTER *prAdapter,
			    IN uint32_t u4FreqInKHz,
			    IN uint32_t *pu4SetInfoLen) {
	struct CMD_TEST_CTRL rCmdTestCtrl;

	ASSERT(prAdapter);

	rCmdTestCtrl.ucAction = 5;	/* Set Channel Frequency */
	rCmdTestCtrl.u.u4ChannelFreq = u4FreqInKHz;

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_TEST_CTRL,
				   TRUE,
				   FALSE,
				   TRUE,
				   nicCmdEventSetCommon,
				   nicOidCmdTimeoutCommon,
				   sizeof(struct CMD_TEST_CTRL),
				   (uint8_t *) &rCmdTestCtrl, NULL, 0);
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
uint32_t
wlanSendSetQueryCmd(IN struct ADAPTER *prAdapter,
		    uint8_t ucCID,
		    u_int8_t fgSetQuery,
		    u_int8_t fgNeedResp,
		    u_int8_t fgIsOid,
		    PFN_CMD_DONE_HANDLER pfCmdDoneHandler,
		    PFN_CMD_TIMEOUT_HANDLER pfCmdTimeoutHandler,
		    uint32_t u4SetQueryInfoLen,
		    uint8_t *pucInfoBuffer, OUT void *pvSetQueryBuffer,
		    IN uint32_t u4SetQueryBufferLen) {
	struct GLUE_INFO *prGlueInfo;
	struct CMD_INFO *prCmdInfo;
	struct WIFI_CMD *prWifiCmd;
	uint8_t ucCmdSeqNum;

	if (kalIsResetting()) {
		DBGLOG(INIT, WARN, "Chip resetting, skip\n");
		return WLAN_STATUS_FAILURE;
	}

	prGlueInfo = prAdapter->prGlueInfo;
	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter,
					  (CMD_HDR_SIZE + u4SetQueryInfoLen));

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
	prCmdInfo->u2InfoBufLen = (uint16_t) (CMD_HDR_SIZE +
					      u4SetQueryInfoLen);
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
		kalMemCopy(prWifiCmd->aucBuffer, pucInfoBuffer,
			   u4SetQueryInfoLen);
	/* insert into prCmdQueue */
	kalEnqueueCommand(prGlueInfo,
			  (struct QUE_ENTRY *) prCmdInfo);

	/* wakeup txServiceThread later */
	GLUE_SET_EVENT(prGlueInfo);
	return WLAN_STATUS_PENDING;
}

#if CFG_SUPPORT_WAPI
/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called by WAPI ui to set wapi mode, which is needed to
 *        info the the driver to operation at WAPI mode while driver initialize.
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
uint32_t
wlanoidSetWapiMode(IN struct ADAPTER *prAdapter,
		   IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		   OUT uint32_t *pu4SetInfoLen) {
	DEBUGFUNC("wlanoidSetWapiMode");
	DBGLOG(REQ, LOUD, "\r\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	/* Todo:: For support WAPI and Wi-Fi at same driver, use the set wapi
	 *        assoc ie at the check point
	 *        The Adapter Connection setting fgUseWapi will cleat whil oid
	 *        set mode (infra),
	 *        And set fgUseWapi True while set wapi assoc ie
	 *        policay selection, add key all depend on this flag,
	 *        The fgUseWapi may remove later
	 */
	if (*(uint32_t *) pvSetBuffer)
		prAdapter->fgUseWapi = TRUE;
	else
		prAdapter->fgUseWapi = FALSE;

#if 0
	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter,
					  (CMD_HDR_SIZE + 4));

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
	prWifiCmd = (struct WIFI_CMD *) (prCmdInfo->pucInfoBuffer);
	prWifiCmd->u2TxByteCount = prCmdInfo->u2InfoBufLen;
	prWifiCmd->u2PQ_ID = CMD_PQ_ID;
	prWifiCmd->ucPktTypeID = CMD_PACKET_TYPE_ID;
	prWifiCmd->ucCID = prCmdInfo->ucCID;
	prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
	prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;

	cp = (uint8_t *) (prWifiCmd->aucBuffer);

	kalMemCopy(cp, (uint8_t *) pvSetBuffer, 4);

	/* insert into prCmdQueue */
	kalEnqueueCommand(prGlueInfo,
			  (struct QUE_ENTRY *) prCmdInfo);

	/* wakeup txServiceThread later */
	GLUE_SET_EVENT(prGlueInfo);

	return WLAN_STATUS_PENDING;
#else
	return WLAN_STATUS_SUCCESS;
#endif
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called by WAPI to set the assoc info, which is needed
 *        to add to Association request frame while join WAPI AP.
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
uint32_t
wlanoidSetWapiAssocInfo(IN struct ADAPTER *prAdapter,
			IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			OUT uint32_t *pu4SetInfoLen) {
	struct WAPI_INFO_ELEM *prWapiInfo;
	uint8_t *cp;
	uint16_t u2AuthSuiteCount = 0;
	uint16_t u2PairSuiteCount = 0;
	uint32_t u4AuthKeyMgtSuite = 0;
	uint32_t u4PairSuite = 0;
	uint32_t u4GroupSuite = 0;
	uint16_t u2IeLength = 0;

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	DEBUGFUNC("wlanoidSetWapiAssocInfo");
	DBGLOG(REQ, LOUD, "\r\n");

	prAdapter->rWifiVar.rConnSettings.fgWapiMode = FALSE;

	if (u4SetBufferLen < 20 /* From EID to Group cipher */)
		return WLAN_STATUS_INVALID_LENGTH;

	if (!wextSrchDesiredWAPIIE((uint8_t *) pvSetBuffer,
				   u4SetBufferLen, (uint8_t **) &prWapiInfo))
		return WLAN_STATUS_INVALID_LENGTH;

	if (!prWapiInfo || prWapiInfo->ucLength < 18
	    || prWapiInfo->ucLength > 40)
		return WLAN_STATUS_INVALID_LENGTH;

	u2IeLength = prWapiInfo->ucLength + 2;

	/* Skip Version check */
	cp = (uint8_t *) &prWapiInfo->u2AuthKeyMgtSuiteCount;

	WLAN_GET_FIELD_16(cp, &u2AuthSuiteCount);

	if (u2AuthSuiteCount > 1)
		return WLAN_STATUS_INVALID_LENGTH;

	cp += 2;
	WLAN_GET_FIELD_32(cp, &u4AuthKeyMgtSuite);

	DBGLOG(SEC, TRACE,
	       "WAPI: Assoc Info auth mgt suite [%d]: %02x-%02x-%02x-%02x\n",
	       u2AuthSuiteCount,
	       (uint8_t) (u4AuthKeyMgtSuite & 0x000000FF),
	       (uint8_t) ((u4AuthKeyMgtSuite >> 8) & 0x000000FF),
	       (uint8_t) ((u4AuthKeyMgtSuite >> 16) & 0x000000FF),
	       (uint8_t) ((u4AuthKeyMgtSuite >> 24) & 0x000000FF));

	if (u4AuthKeyMgtSuite != WAPI_AKM_SUITE_802_1X
	    && u4AuthKeyMgtSuite != WAPI_AKM_SUITE_PSK)
		ASSERT(FALSE);

	cp += 4;
	WLAN_GET_FIELD_16(cp, &u2PairSuiteCount);
	if (u2PairSuiteCount > 1)
		return WLAN_STATUS_INVALID_LENGTH;

	cp += 2;
	WLAN_GET_FIELD_32(cp, &u4PairSuite);
	DBGLOG(SEC, TRACE,
	       "WAPI: Assoc Info pairwise cipher suite [%d]: %02x-%02x-%02x-%02x\n",
	       u2PairSuiteCount,
	       (uint8_t) (u4PairSuite & 0x000000FF),
	       (uint8_t) ((u4PairSuite >> 8) & 0x000000FF),
	       (uint8_t) ((u4PairSuite >> 16) & 0x000000FF),
	       (uint8_t) ((u4PairSuite >> 24) & 0x000000FF));

	if (u4PairSuite != WAPI_CIPHER_SUITE_WPI)
		ASSERT(FALSE);

	cp += 4;
	WLAN_GET_FIELD_32(cp, &u4GroupSuite);
	DBGLOG(SEC, TRACE,
	       "WAPI: Assoc Info group cipher suite : %02x-%02x-%02x-%02x\n",
	       (uint8_t) (u4GroupSuite & 0x000000FF),
	       (uint8_t) ((u4GroupSuite >> 8) & 0x000000FF),
	       (uint8_t) ((u4GroupSuite >> 16) & 0x000000FF),
	       (uint8_t) ((u4GroupSuite >> 24) & 0x000000FF));

	if (u4GroupSuite != WAPI_CIPHER_SUITE_WPI)
		ASSERT(FALSE);

	prAdapter->rWifiVar.rConnSettings.u4WapiSelectedAKMSuite =
		u4AuthKeyMgtSuite;
	prAdapter->rWifiVar.rConnSettings.u4WapiSelectedPairwiseCipher
		= u4PairSuite;
	prAdapter->rWifiVar.rConnSettings.u4WapiSelectedGroupCipher
		= u4GroupSuite;

	kalMemCopy(prAdapter->prGlueInfo->aucWapiAssocInfoIEs,
		   prWapiInfo, u2IeLength);
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
uint32_t
wlanoidSetWapiKey(IN struct ADAPTER *prAdapter,
		  IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		  OUT uint32_t *pu4SetInfoLen) {
	struct GLUE_INFO *prGlueInfo;
	struct CMD_INFO *prCmdInfo;
	struct WIFI_CMD *prWifiCmd;
	struct PARAM_WPI_KEY *prNewKey;
	struct CMD_802_11_KEY *prCmdKey;
	uint8_t *pc;
	uint8_t ucCmdSeqNum;
	struct STA_RECORD *prStaRec;
	struct BSS_INFO *prBssInfo;

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

	prNewKey = (struct PARAM_WPI_KEY *) pvSetBuffer;

	DBGLOG_MEM8(REQ, TRACE, (uint8_t *) pvSetBuffer, 560);
	pc = (uint8_t *) pvSetBuffer;

	*pu4SetInfoLen = u4SetBufferLen;

	/* Todo:: WAPI AP mode !!!!! */
	prBssInfo = prAdapter->prAisBssInfo;

	prNewKey->ucKeyID = prNewKey->ucKeyID & BIT(0);

	/* Dump P_PARAM_WPI_KEY_T content. */
	DBGLOG(REQ, TRACE,
	       "Set: Dump P_PARAM_WPI_KEY_T content\r\n");
	DBGLOG(REQ, TRACE, "TYPE      : %d\r\n",
	       prNewKey->eKeyType);
	DBGLOG(REQ, TRACE, "Direction : %d\r\n",
	       prNewKey->eDirection);
	DBGLOG(REQ, TRACE, "KeyID     : %d\r\n", prNewKey->ucKeyID);
	DBGLOG(REQ, TRACE, "AddressIndex:\r\n");
	DBGLOG_MEM8(REQ, TRACE, prNewKey->aucAddrIndex, 12);
	prNewKey->u4LenWPIEK = 16;

	DBGLOG_MEM8(REQ, TRACE, (uint8_t *) prNewKey->aucWPIEK,
		    (uint8_t) prNewKey->u4LenWPIEK);
	prNewKey->u4LenWPICK = 16;

	DBGLOG(REQ, TRACE, "CK Key(%d):\r\n",
	       (uint8_t) prNewKey->u4LenWPICK);
	DBGLOG_MEM8(REQ, TRACE, (uint8_t *) prNewKey->aucWPICK,
		    (uint8_t) prNewKey->u4LenWPICK);
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

	DBGLOG_MEM8(REQ, TRACE, (uint8_t *) prNewKey->aucPN, 16);

	prGlueInfo = prAdapter->prGlueInfo;

	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter,
					  (CMD_HDR_SIZE + u4SetBufferLen));

	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}
	/* increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

	/* compose CMD_ID_ADD_REMOVE_KEY cmd pkt */
	prCmdInfo->eCmdType = COMMAND_TYPE_NETWORK_IOCTL;
	prCmdInfo->u2InfoBufLen = CMD_HDR_SIZE + sizeof(
					  struct CMD_802_11_KEY);
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
	prWifiCmd = (struct WIFI_CMD *) (prCmdInfo->pucInfoBuffer);
	prWifiCmd->u2TxByteCount = prCmdInfo->u2InfoBufLen;
	prWifiCmd->u2PQ_ID = CMD_PQ_ID;
	prWifiCmd->ucPktTypeID = CMD_PACKET_TYPE_ID;
	prWifiCmd->ucCID = prCmdInfo->ucCID;
	prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
	prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;

	prCmdKey = (struct CMD_802_11_KEY *) (prWifiCmd->aucBuffer);

	kalMemZero(prCmdKey, sizeof(struct CMD_802_11_KEY));

	prCmdKey->ucAddRemove = 1;	/* Add */

	if (prNewKey->eKeyType == ENUM_WPI_PAIRWISE_KEY) {
		prCmdKey->ucTxKey = 1;
		prCmdKey->ucKeyType = 1;
	}
	kalMemCopy(prCmdKey->aucPeerAddr,
		   (uint8_t *) prNewKey->aucAddrIndex, MAC_ADDR_LEN);
	if ((prCmdKey->aucPeerAddr[0] & prCmdKey->aucPeerAddr[1] &
	     prCmdKey->aucPeerAddr[2] &
	     prCmdKey->aucPeerAddr[3] & prCmdKey->aucPeerAddr[4] &
	     prCmdKey->aucPeerAddr[5]) == 0xFF) {
		prStaRec = cnmGetStaRecByAddress(prAdapter,
				prBssInfo->ucBssIndex, prBssInfo->aucBSSID);
		ASSERT(prStaRec);	/* AIS RSN Group key, addr is BC addr */
		kalMemCopy(prCmdKey->aucPeerAddr, prStaRec->aucMacAddr,
			   MAC_ADDR_LEN);
	} else {
		prStaRec = cnmGetStaRecByAddress(prAdapter,
				prBssInfo->ucBssIndex, prCmdKey->aucPeerAddr);
	}

	prCmdKey->ucBssIdx =
		prAdapter->prAisBssInfo->ucBssIndex;	/* AIS */

	prCmdKey->ucKeyId = prNewKey->ucKeyID;

	prCmdKey->ucKeyLen = 32;

	prCmdKey->ucAlgorithmId = CIPHER_SUITE_WPI;

	kalMemCopy(prCmdKey->aucKeyMaterial,
		   (uint8_t *) prNewKey->aucWPIEK, 16);

	kalMemCopy(prCmdKey->aucKeyMaterial + 16,
		   (uint8_t *) prNewKey->aucWPICK, 16);

	kalMemCopy(prCmdKey->aucKeyRsc, (uint8_t *) prNewKey->aucPN,
		   16);

	if (prCmdKey->ucTxKey) {
		if (prStaRec) {
			if (prCmdKey->ucKeyType) {	/* AIS RSN STA */
				prCmdKey->ucWlanIndex = prStaRec->ucWlanIndex;
				prStaRec->fgTransmitKeyExist =
					TRUE;	/* wait for CMD Done ? */
			} else {
				ASSERT(FALSE);
			}
		}
#if 0
		if (fgAddTxBcKey || !prStaRec) {

			if ((prCmdKey->aucPeerAddr[0]
			    & prCmdKey->aucPeerAddr[1]
			    & prCmdKey->aucPeerAddr[2]
			    & prCmdKey->aucPeerAddr[3]
			    & prCmdKey->aucPeerAddr[4]
			    & prCmdKey->aucPeerAddr[5]) == 0xFF) {
				prCmdKey->ucWlanIndex =
						255; /* AIS WEP Tx key */
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
			prBssInfo->ucBMCWlanIndex =
				prCmdKey->ucWlanIndex;	/* Saved for AIS WEP */
			prBssInfo->ucTxBcDefaultIdx = prCmdKey->ucKeyId;
		}
#endif
	} else {
		/* Including IBSS RSN Rx BC key ? */
		if ((prCmdKey->aucPeerAddr[0] & prCmdKey->aucPeerAddr[1] &
		     prCmdKey->aucPeerAddr[2] & prCmdKey->aucPeerAddr[3] &
		     prCmdKey->aucPeerAddr[4] & prCmdKey->aucPeerAddr[5]) ==
		    0xFF) {
			prCmdKey->ucWlanIndex =
				WTBL_RESERVED_ENTRY; /* AIS WEP, should not have
						      * this case!!
						      */
		} else {
			if (prStaRec) {	/* AIS RSN Group key but addr is BSSID
					 */
				/* ASSERT(prStaRec->ucBMCWlanIndex < WTBL_SIZE)
				 */
				prCmdKey->ucWlanIndex =
					secPrivacySeekForBcEntry(prAdapter,
							prStaRec->ucBssIndex,
							prStaRec->aucMacAddr,
							prStaRec->ucIndex,
							prCmdKey->ucAlgorithmId,
							prCmdKey->ucKeyId);
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
	kalEnqueueCommand(prGlueInfo,
			  (struct QUE_ENTRY *) prCmdInfo);

	/* wakeup txServiceThread later */
	GLUE_SET_EVENT(prGlueInfo);

	return WLAN_STATUS_PENDING;
}				/* wlanoidSetAddKey */
#endif

#if CFG_SUPPORT_WPS2
/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called by WSC to set the assoc info, which is needed
 *        to add to Association request frame while join WPS AP.
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
uint32_t
wlanoidSetWSCAssocInfo(IN struct ADAPTER *prAdapter,
		       IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		       OUT uint32_t *pu4SetInfoLen) {
	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	DEBUGFUNC("wlanoidSetWSCAssocInfo");
	DBGLOG(REQ, LOUD, "\r\n");

	if (u4SetBufferLen == 0)
		return WLAN_STATUS_INVALID_LENGTH;

	*pu4SetInfoLen = u4SetBufferLen;

	kalMemCopy(prAdapter->prGlueInfo->aucWSCAssocInfoIE,
		   pvSetBuffer, u4SetBufferLen);
	prAdapter->prGlueInfo->u2WSCAssocInfoIELen =
		(uint16_t) u4SetBufferLen;
	DBGLOG(SEC, TRACE, "Assoc Info IE sz %d\n", u4SetBufferLen);

	return WLAN_STATUS_SUCCESS;

}
#endif

#if CFG_ENABLE_WAKEUP_ON_LAN
uint32_t
wlanoidSetAddWakeupPattern(IN struct ADAPTER *prAdapter,
			   IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			   OUT uint32_t *pu4SetInfoLen) {
	struct PARAM_PM_PACKET_PATTERN *prPacketPattern;

	DEBUGFUNC("wlanoidSetAddWakeupPattern");
	DBGLOG(REQ, LOUD, "\r\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(struct PARAM_PM_PACKET_PATTERN);

	if (u4SetBufferLen < sizeof(struct PARAM_PM_PACKET_PATTERN))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prPacketPattern = (struct PARAM_PM_PACKET_PATTERN *)
			  pvSetBuffer;

	/* FIXME: Send the struct to firmware */

	return WLAN_STATUS_FAILURE;
}

uint32_t
wlanoidSetRemoveWakeupPattern(IN struct ADAPTER *prAdapter,
			      IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			      OUT uint32_t *pu4SetInfoLen) {
	struct PARAM_PM_PACKET_PATTERN *prPacketPattern;

	DEBUGFUNC("wlanoidSetAddWakeupPattern");
	DBGLOG(REQ, LOUD, "\r\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(struct PARAM_PM_PACKET_PATTERN);

	if (u4SetBufferLen < sizeof(struct PARAM_PM_PACKET_PATTERN))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prPacketPattern = (struct PARAM_PM_PACKET_PATTERN *)
			  pvSetBuffer;

	/* FIXME: Send the struct to firmware */

	return WLAN_STATUS_FAILURE;
}

uint32_t
wlanoidQueryEnableWakeup(IN struct ADAPTER *prAdapter,
			 OUT void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
			 OUT uint32_t *pu4QueryInfoLen) {
	uint32_t *pu4WakeupEventEnable;

	DEBUGFUNC("wlanoidQueryEnableWakeup");
	DBGLOG(REQ, LOUD, "\r\n");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(uint32_t);

	if (u4QueryBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	pu4WakeupEventEnable = (uint32_t *) pvQueryBuffer;

	*pu4WakeupEventEnable = prAdapter->u4WakeupEventEnable;

	return WLAN_STATUS_SUCCESS;
}

uint32_t
wlanoidSetEnableWakeup(IN struct ADAPTER *prAdapter,
		       IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		       OUT uint32_t *pu4SetInfoLen) {
	uint32_t *pu4WakeupEventEnable;

	DEBUGFUNC("wlanoidSetEnableWakup");
	DBGLOG(REQ, LOUD, "\r\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(uint32_t);

	if (u4SetBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	pu4WakeupEventEnable = (uint32_t *) pvSetBuffer;
	prAdapter->u4WakeupEventEnable = *pu4WakeupEventEnable;

	/* FIXME: Send Command Event for setting
	 *        wakeup-pattern / Magic Packet to firmware
	 */

	return WLAN_STATUS_FAILURE;
}
#endif

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to configure PS related settings for WMM-PS
 *        test.
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
uint32_t
wlanoidSetWiFiWmmPsTest(IN struct ADAPTER *prAdapter,
			IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			OUT uint32_t *pu4SetInfoLen) {
	struct PARAM_CUSTOM_WMM_PS_TEST_STRUCT *prWmmPsTestInfo;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	struct CMD_SET_WMM_PS_TEST_STRUCT rSetWmmPsTestParam;
	uint16_t u2CmdBufLen;
	struct PM_PROFILE_SETUP_INFO *prPmProfSetupInfo;
	struct BSS_INFO *prBssInfo;

	DEBUGFUNC("wlanoidSetWiFiWmmPsTest");

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(struct
				PARAM_CUSTOM_WMM_PS_TEST_STRUCT);

	prWmmPsTestInfo = (struct PARAM_CUSTOM_WMM_PS_TEST_STRUCT *)
			  pvSetBuffer;

	rSetWmmPsTestParam.ucBssIndex =
		prAdapter->prAisBssInfo->ucBssIndex;
	rSetWmmPsTestParam.bmfgApsdEnAc =
		prWmmPsTestInfo->bmfgApsdEnAc;
	rSetWmmPsTestParam.ucIsEnterPsAtOnce =
		prWmmPsTestInfo->ucIsEnterPsAtOnce;
	rSetWmmPsTestParam.ucIsDisableUcTrigger =
		prWmmPsTestInfo->ucIsDisableUcTrigger;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
					  rSetWmmPsTestParam.ucBssIndex);
	prPmProfSetupInfo = &prBssInfo->rPmProfSetupInfo;
	prPmProfSetupInfo->ucBmpDeliveryAC =
		(rSetWmmPsTestParam.bmfgApsdEnAc >> 4) & BITS(0, 3);
	prPmProfSetupInfo->ucBmpTriggerAC =
		rSetWmmPsTestParam.bmfgApsdEnAc & BITS(0, 3);

	u2CmdBufLen = sizeof(struct CMD_SET_WMM_PS_TEST_STRUCT);

#if 0
	/* it will apply the disable trig or not immediately */
	if (prPmInfo->ucWmmPsDisableUcPoll
	    && prPmInfo->ucWmmPsConnWithTrig)
		NIC_PM_WMM_PS_DISABLE_UC_TRIG(prAdapter, TRUE);
	else
		NIC_PM_WMM_PS_DISABLE_UC_TRIG(prAdapter, FALSE);
#endif

	rStatus = wlanSendSetQueryCmd(prAdapter,
				      CMD_ID_SET_WMM_PS_TEST_PARMS,
				      TRUE, FALSE, g_fgIsOid,
				      nicCmdEventSetCommon,/* TODO? */
				      nicCmdTimeoutCommon, u2CmdBufLen,
				      (uint8_t *) &rSetWmmPsTestParam, NULL, 0);

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
uint32_t
wlanoidSetTxAmpdu(IN struct ADAPTER *prAdapter,
		  IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		  OUT uint32_t *pu4SetInfoLen) {
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	struct CMD_TX_AMPDU rTxAmpdu;
	uint16_t u2CmdBufLen;
	u_int8_t *pfgEnable;

	DEBUGFUNC("wlanoidSetTxAmpdu");

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(u_int8_t);

	pfgEnable = (u_int8_t *) pvSetBuffer;

	rTxAmpdu.fgEnable = *pfgEnable;

	u2CmdBufLen = sizeof(struct CMD_TX_AMPDU);

	rStatus = wlanSendSetQueryCmd(prAdapter, CMD_ID_TX_AMPDU,
				      TRUE, FALSE, TRUE, NULL, NULL,
				      u2CmdBufLen,
				      (uint8_t *) &rTxAmpdu, NULL, 0);

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
uint32_t
wlanoidSetAddbaReject(IN struct ADAPTER *prAdapter,
		      IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		      OUT uint32_t *pu4SetInfoLen) {
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	struct CMD_ADDBA_REJECT rAddbaReject;
	uint16_t u2CmdBufLen;
	u_int8_t *pfgEnable;

	DEBUGFUNC("wlanoidSetAddbaReject");

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(u_int8_t);

	pfgEnable = (u_int8_t *) pvSetBuffer;

	rAddbaReject.fgEnable = *pfgEnable;

	u2CmdBufLen = sizeof(struct CMD_ADDBA_REJECT);

	rStatus = wlanSendSetQueryCmd(prAdapter, CMD_ID_ADDBA_REJECT,
				      TRUE, FALSE, TRUE, NULL, NULL,
				      u2CmdBufLen,
				      (uint8_t *) &rAddbaReject, NULL, 0);

	return rStatus;
}				/* wlanoidSetAddbaReject */

#if CFG_SLT_SUPPORT

uint32_t
wlanoidQuerySLTStatus(IN struct ADAPTER *prAdapter,
		      OUT void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
		      OUT uint32_t *pu4QueryInfoLen) {
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;
	struct PARAM_MTK_SLT_TEST_STRUCT *prMtkSltInfo =
		(struct PARAM_MTK_SLT_TEST_STRUCT *) NULL;
	struct SLT_INFO *prSltInfo = (struct SLT_INFO *) NULL;

	DEBUGFUNC("wlanoidQuerySLTStatus");
	DBGLOG(REQ, LOUD, "\r\n");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);

	*pu4QueryInfoLen = sizeof(struct PARAM_MTK_SLT_TEST_STRUCT);

	if (u4QueryBufferLen < sizeof(struct
				      PARAM_MTK_SLT_TEST_STRUCT))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvQueryBuffer);

	prMtkSltInfo = (struct PARAM_MTK_SLT_TEST_STRUCT *)
		       pvQueryBuffer;

	prSltInfo = &(prAdapter->rWifiVar.rSltInfo);

	switch (prMtkSltInfo->rSltFuncIdx) {
	case ENUM_MTK_SLT_FUNC_LP_SET: {
		struct PARAM_MTK_SLT_LP_TEST_STRUCT *prLpSetting =
			(struct PARAM_MTK_SLT_LP_TEST_STRUCT *) NULL;

		ASSERT(prMtkSltInfo->u4FuncInfoLen == sizeof(
			       struct PARAM_MTK_SLT_LP_TEST_STRUCT));

		prLpSetting = (struct PARAM_MTK_SLT_LP_TEST_STRUCT *)
			      &prMtkSltInfo->unFuncInfoContent;

		prLpSetting->u4BcnRcvNum = prSltInfo->u4BeaconReceiveCnt;
	}
	break;
	default:
		/* TBD... */
		break;
	}

	return rWlanStatus;
}				/* wlanoidQuerySLTStatus */

uint32_t
wlanoidUpdateSLTMode(IN struct ADAPTER *prAdapter,
		     IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		     OUT uint32_t *pu4SetInfoLen) {
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;
	struct PARAM_MTK_SLT_TEST_STRUCT *prMtkSltInfo =
		(struct PARAM_MTK_SLT_TEST_STRUCT *) NULL;
	struct SLT_INFO *prSltInfo = (struct SLT_INFO *) NULL;
	struct BSS_DESC *prBssDesc = (struct BSS_DESC *) NULL;
	struct STA_RECORD *prStaRec = (struct STA_RECORD *) NULL;
	struct BSS_INFO *prBssInfo = (struct BSS_INFO *) NULL;

	/* 1. Action: Update or Initial Set
	 * 2. Role.
	 * 3. Target MAC address.
	 * 4. RF BW & Rate Settings
	 */

	DEBUGFUNC("wlanoidUpdateSLTMode");
	DBGLOG(REQ, LOUD, "\r\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(struct PARAM_MTK_SLT_TEST_STRUCT);

	if (u4SetBufferLen < sizeof(struct
				    PARAM_MTK_SLT_TEST_STRUCT))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prMtkSltInfo = (struct PARAM_MTK_SLT_TEST_STRUCT *)
		       pvSetBuffer;

	prSltInfo = &(prAdapter->rWifiVar.rSltInfo);
	prBssInfo = prAdapter->prAisBssInfo;

	switch (prMtkSltInfo->rSltFuncIdx) {
	case ENUM_MTK_SLT_FUNC_INITIAL: {	/* Initialize */
		struct PARAM_MTK_SLT_INITIAL_STRUCT *prMtkSltInit =
			(struct PARAM_MTK_SLT_INITIAL_STRUCT *) NULL;

		ASSERT(prMtkSltInfo->u4FuncInfoLen == sizeof(
			       struct PARAM_MTK_SLT_INITIAL_STRUCT));

		prMtkSltInit = (struct PARAM_MTK_SLT_INITIAL_STRUCT *)
			       &prMtkSltInfo->unFuncInfoContent;

		if (prSltInfo->prPseudoStaRec != NULL) {
			/* The driver has been initialized. */
			prSltInfo->prPseudoStaRec = NULL;
		}

		prSltInfo->prPseudoBssDesc = scanSearchExistingBssDesc(
						prAdapter, BSS_TYPE_IBSS,
						prMtkSltInit->aucTargetMacAddr,
						prMtkSltInit->aucTargetMacAddr);

		prSltInfo->u2SiteID = prMtkSltInit->u2SiteID;

		/* Bandwidth 2.4G: Channel 1~14
		 * Bandwidth 5G: *36, 40, 44, 48, 52, 56, 60, 64,
		 *       *100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140,
		 *       149, 153, *157, 161,
		 *       184, 188, 192, 196, 200, 204, 208, 212, *216
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
				prSltInfo->prPseudoBssDesc =
						scanAllocateBssDesc(prAdapter);

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

			COPY_MAC_ADDR(prBssDesc->aucSrcAddr,
				      prMtkSltInit->aucTargetMacAddr);
			COPY_MAC_ADDR(prBssDesc->aucBSSID,
				      prBssInfo->aucOwnMacAddr);

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
			struct PARAM_MTK_SLT_TR_TEST_STRUCT *prTRSetting =
				(struct PARAM_MTK_SLT_TR_TEST_STRUCT *) NULL;

			ASSERT(prMtkSltInfo->u4FuncInfoLen == sizeof(
				       struct PARAM_MTK_SLT_TR_TEST_STRUCT));

			prStaRec = prSltInfo->prPseudoStaRec;
			prTRSetting = (struct PARAM_MTK_SLT_TR_TEST_STRUCT *)
				      &prMtkSltInfo->unFuncInfoContent;

			if (prTRSetting->rNetworkType ==
			    PARAM_NETWORK_TYPE_OFDM5) {
				prBssInfo->eBand = BAND_5G;
				prBssInfo->ucPrimaryChannel =
							prSltInfo->ucChannel5G;
			}
			if (prTRSetting->rNetworkType ==
			    PARAM_NETWORK_TYPE_OFDM24) {
				prBssInfo->eBand = BAND_2G4;
				prBssInfo->ucPrimaryChannel =
							prSltInfo->ucChannel2G4;
			}

			if ((prTRSetting->u4FixedRate & FIXED_BW_DL40) != 0) {
				/* RF 40 */
				/* It would controls RFBW capability in WTBL. */
				prStaRec->u2HtCapInfo |=
						HT_CAP_INFO_SUP_CHNL_WIDTH;
				/* This controls RF BW, RF BW would be 40
				 * only if
				 * 1. PHY_TYPE_BIT_HT is TRUE.
				 * 2. SCO is SCA/SCB.
				 */
				prStaRec->ucDesiredPhyTypeSet = PHY_TYPE_BIT_HT;

				/* U20/L20 Control. */
				switch (prTRSetting->u4FixedRate & 0xC000) {
				case FIXED_EXT_CHNL_U20:
					prBssInfo->eBssSCO =
							CHNL_EXT_SCB; /* +2 */
					if (prTRSetting->rNetworkType ==
					    PARAM_NETWORK_TYPE_OFDM5) {
						prBssInfo->ucPrimaryChannel
									+= 2;
					} else {
						/* For channel 1, testing L20 at
						 * channel 8. AOSP
						 */
						SetTestChannel(
						&prBssInfo->ucPrimaryChannel);
					}
					break;
				case FIXED_EXT_CHNL_L20:
				default:	/* 40M */
					prBssInfo->eBssSCO =
							CHNL_EXT_SCA; /* -2 */
					if (prTRSetting->rNetworkType ==
					    PARAM_NETWORK_TYPE_OFDM5) {
						prBssInfo->ucPrimaryChannel
									-= 2;
					} else {
						/* For channel 11 / 14. testing
						 * U20 at channel 3. AOSP
						 */
						SetTestChannel(
						&prBssInfo->ucPrimaryChannel);
					}
					break;
				}
			} else {
				/* RF 20 */
				prStaRec->u2HtCapInfo &=
						~HT_CAP_INFO_SUP_CHNL_WIDTH;
				prBssInfo->eBssSCO = CHNL_EXT_SCN;
			}

			prBssInfo->fgErpProtectMode = FALSE;
			prBssInfo->eHtProtectMode = HT_PROTECT_MODE_NONE;
			prBssInfo->eGfOperationMode = GF_MODE_NORMAL;

			nicUpdateBss(prAdapter, prBssInfo->ucNetTypeIndex);

			prStaRec->u2HtCapInfo &= ~(HT_CAP_INFO_SHORT_GI_20M |
						   HT_CAP_INFO_SHORT_GI_40M);

			switch (prTRSetting->u4FixedRate & 0xFF) {
			case RATE_OFDM_54M:
				prStaRec->u2DesiredNonHTRateSet =
						BIT(RATE_54M_SW_INDEX);
				break;
			case RATE_OFDM_48M:
				prStaRec->u2DesiredNonHTRateSet =
						BIT(RATE_48M_SW_INDEX);
				break;
			case RATE_OFDM_36M:
				prStaRec->u2DesiredNonHTRateSet =
						BIT(RATE_36M_SW_INDEX);
				break;
			case RATE_OFDM_24M:
				prStaRec->u2DesiredNonHTRateSet =
						BIT(RATE_24M_SW_INDEX);
				break;
			case RATE_OFDM_6M:
				prStaRec->u2DesiredNonHTRateSet =
						BIT(RATE_6M_SW_INDEX);
				break;
			case RATE_CCK_11M_LONG:
				prStaRec->u2DesiredNonHTRateSet =
						BIT(RATE_11M_SW_INDEX);
				break;
			case RATE_CCK_1M_LONG:
				prStaRec->u2DesiredNonHTRateSet =
						BIT(RATE_1M_SW_INDEX);
				break;
			case RATE_GF_MCS_0:
				prStaRec->u2DesiredNonHTRateSet =
						BIT(RATE_HT_PHY_SW_INDEX);
				prStaRec->u2HtCapInfo |= HT_CAP_INFO_HT_GF;
				break;
			case RATE_MM_MCS_7:
				prStaRec->u2DesiredNonHTRateSet =
						BIT(RATE_HT_PHY_SW_INDEX);
				prStaRec->u2HtCapInfo &= ~HT_CAP_INFO_HT_GF;
#if 0				/* Only for Current Measurement Mode. */
				prStaRec->u2HtCapInfo |=
						(HT_CAP_INFO_SHORT_GI_20M |
						HT_CAP_INFO_SHORT_GI_40M);
#endif
				break;
			case RATE_GF_MCS_7:
				prStaRec->u2DesiredNonHTRateSet =
						BIT(RATE_HT_PHY_SW_INDEX);
				prStaRec->u2HtCapInfo |= HT_CAP_INFO_HT_GF;
				break;
			default:
				prStaRec->u2DesiredNonHTRateSet =
						BIT(RATE_36M_SW_INDEX);
				break;
			}

			cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_1);

			cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_3);

		}
		break;
	case ENUM_MTK_SLT_FUNC_LP_SET: {	/* Reset LP Test Result. */
		struct PARAM_MTK_SLT_LP_TEST_STRUCT *prLpSetting =
			(struct PARAM_MTK_SLT_LP_TEST_STRUCT *) NULL;

		ASSERT(prMtkSltInfo->u4FuncInfoLen == sizeof(
			       struct PARAM_MTK_SLT_LP_TEST_STRUCT));

		prLpSetting = (struct PARAM_MTK_SLT_LP_TEST_STRUCT *)
			      &prMtkSltInfo->unFuncInfoContent;

		if (prSltInfo->prPseudoBssDesc == NULL) {
			/* Please initial SLT Mode first. */
			break;
		}
		prBssDesc = prSltInfo->prPseudoBssDesc;

		switch (prLpSetting->rLpTestMode) {
		case ENUM_MTK_LP_TEST_NORMAL:
			/* In normal mode, we would use target MAC address to be
			 * the BSSID.
			 */
			COPY_MAC_ADDR(prBssDesc->aucBSSID,
				      prBssInfo->aucOwnMacAddr);
			prSltInfo->fgIsDUT = FALSE;
			break;
		case ENUM_MTK_LP_TEST_GOLDEN_SAMPLE:
			/* 1. Lower AIFS of BCN queue.
			 * 2. Fixed Random Number tobe 0.
			 */
			prSltInfo->fgIsDUT = FALSE;
			/* In LP test mode, we would use MAC address of Golden
			 * Sample to be the BSSID.
			 */
			COPY_MAC_ADDR(prBssDesc->aucBSSID,
				      prBssInfo->aucOwnMacAddr);
			break;
		case ENUM_MTK_LP_TEST_DUT:
			/* 1. Enter Sleep Mode.
			 * 2. Fix random number a large value & enlarge AIFN of
			 *    BCN queue.
			 */
			COPY_MAC_ADDR(prBssDesc->aucBSSID,
				      prBssDesc->aucSrcAddr);
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
uint32_t
wlanoidQueryNvramRead(IN struct ADAPTER *prAdapter,
		      OUT void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
		      OUT uint32_t *pu4QueryInfoLen) {
	struct PARAM_CUSTOM_EEPROM_RW_STRUCT *prNvramRwInfo;
	uint16_t u2Data;
	u_int8_t fgStatus;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidQueryNvramRead");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(struct
				  PARAM_CUSTOM_EEPROM_RW_STRUCT);

	if (u4QueryBufferLen < sizeof(struct
				      PARAM_CUSTOM_EEPROM_RW_STRUCT))
		return WLAN_STATUS_INVALID_LENGTH;

	prNvramRwInfo = (struct PARAM_CUSTOM_EEPROM_RW_STRUCT *)
			pvQueryBuffer;

	if (prNvramRwInfo->ucEepromMethod ==
	    PARAM_EEPROM_READ_METHOD_READ) {
		fgStatus = kalCfgDataRead16(prAdapter->prGlueInfo,
					    prNvramRwInfo->ucEepromIndex <<
					    1,	/* change to byte offset */
					    &u2Data);

		if (fgStatus) {
			prNvramRwInfo->u2EepromData = u2Data;
			DBGLOG(REQ, INFO,
			       "NVRAM Read: index=%#X, data=%#02X\r\n",
			       prNvramRwInfo->ucEepromIndex, u2Data);
		} else {
			DBGLOG(REQ, ERROR, "NVRAM Read Failed: index=%#x.\r\n",
			       prNvramRwInfo->ucEepromIndex);
			rStatus = WLAN_STATUS_FAILURE;
		}
	} else if (prNvramRwInfo->ucEepromMethod ==
		   PARAM_EEPROM_READ_METHOD_GETSIZE) {
		prNvramRwInfo->u2EepromData = CFG_FILE_WIFI_REC_SIZE;
		DBGLOG(REQ, INFO, "EEPROM size =%d\r\n",
		       prNvramRwInfo->u2EepromData);
	}

	*pu4QueryInfoLen = sizeof(struct
				  PARAM_CUSTOM_EEPROM_RW_STRUCT);

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
uint32_t
wlanoidSetNvramWrite(IN struct ADAPTER *prAdapter,
		     IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		     OUT uint32_t *pu4SetInfoLen) {
	struct PARAM_CUSTOM_EEPROM_RW_STRUCT *prNvramRwInfo;
	u_int8_t fgStatus;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidSetNvramWrite");
	DBGLOG(INIT, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(struct
				PARAM_CUSTOM_EEPROM_RW_STRUCT);

	if (u4SetBufferLen < sizeof(struct
				    PARAM_CUSTOM_EEPROM_RW_STRUCT))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prNvramRwInfo = (struct PARAM_CUSTOM_EEPROM_RW_STRUCT *)
			pvSetBuffer;

	fgStatus = kalCfgDataWrite16(prAdapter->prGlueInfo,
				     prNvramRwInfo->ucEepromIndex <<
				     1,	/* change to byte offset */
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
uint32_t
wlanoidQueryCfgSrcType(IN struct ADAPTER *prAdapter,
		       OUT void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
		       OUT uint32_t *pu4QueryInfoLen) {
	ASSERT(prAdapter);

	*pu4QueryInfoLen = sizeof(enum ENUM_CFG_SRC_TYPE);

	if (kalIsConfigurationExist(prAdapter->prGlueInfo) == TRUE)
		*(enum ENUM_CFG_SRC_TYPE *) pvQueryBuffer =
			CFG_SRC_TYPE_NVRAM;
	else
		*(enum ENUM_CFG_SRC_TYPE *) pvQueryBuffer =
			CFG_SRC_TYPE_EEPROM;

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
uint32_t
wlanoidQueryEepromType(IN struct ADAPTER *prAdapter,
		       OUT void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
		       OUT uint32_t *pu4QueryInfoLen) {
	ASSERT(prAdapter);

	*pu4QueryInfoLen = sizeof(enum ENUM_EEPROM_TYPE *);

#if CFG_SUPPORT_NIC_CAPABILITY
	if (prAdapter->fgIsEepromUsed == TRUE)
		*(enum ENUM_EEPROM_TYPE *) pvQueryBuffer =
			EEPROM_TYPE_PRESENT;
	else
		*(enum ENUM_EEPROM_TYPE *) pvQueryBuffer = EEPROM_TYPE_NO;
#else
	*(enum ENUM_EEPROM_TYPE *) pvQueryBuffer = EEPROM_TYPE_NO;
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
uint32_t
wlanoidSetCountryCode(IN struct ADAPTER *prAdapter,
		      IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		      OUT uint32_t *pu4SetInfoLen) {
	uint8_t *pucCountry;

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);

	if (regd_is_single_sku_en()) {
		rlmDomainOidSetCountry(prAdapter, pvSetBuffer,
				       u4SetBufferLen);
		*pu4SetInfoLen = u4SetBufferLen;
		return WLAN_STATUS_SUCCESS;
	}

	ASSERT(u4SetBufferLen == 2);

	*pu4SetInfoLen = 2;

	pucCountry = pvSetBuffer;

	prAdapter->rWifiVar.rConnSettings.u2CountryCode =
		(((uint16_t) pucCountry[0]) << 8) | ((uint16_t) pucCountry[1]);

	/* Force to re-search country code in regulatory domains */
	prAdapter->prDomainInfo = NULL;
	rlmDomainSendCmd(prAdapter);

	/* Update supported channel list in channel table based on current
	 * country domain
	 */
	wlanUpdateChannelTable(prAdapter->prGlueInfo);

	return WLAN_STATUS_SUCCESS;
}

uint32_t
wlanoidSetScanMacOui(IN struct ADAPTER *prAdapter,
		IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		OUT uint32_t *pu4SetInfoLen)
{
	struct PARAM_BSS_MAC_OUI *prParamMacOui;
	struct BSS_INFO *prBssInfo;

	ASSERT(prAdapter);
	ASSERT(prAdapter->prGlueInfo);
	ASSERT(pvSetBuffer);
	ASSERT(u4SetBufferLen == sizeof(struct PARAM_BSS_MAC_OUI));

	prParamMacOui = (struct PARAM_BSS_MAC_OUI *)pvSetBuffer;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prParamMacOui->ucBssIndex);
	if (!prBssInfo) {
		log_dbg(REQ, ERROR, "Invalid bss info (ind=%u)\n",
			prParamMacOui->ucBssIndex);
		return WLAN_STATUS_FAILURE;
	}

	kalMemCopy(prBssInfo->ucScanOui, prParamMacOui->ucMacOui, MAC_OUI_LEN);
	prBssInfo->fgIsScanOuiSet = TRUE;
	*pu4SetInfoLen = MAC_OUI_LEN;

	return WLAN_STATUS_SUCCESS;
}

#if 0
uint32_t
wlanoidSetNoaParam(IN struct ADAPTER *prAdapter,
		   IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		   OUT uint32_t *pu4SetInfoLen) {
	struct PARAM_CUSTOM_NOA_PARAM_STRUCT *prNoaParam;
	struct CMD_CUSTOM_NOA_PARAM_STRUCT rCmdNoaParam;

	DEBUGFUNC("wlanoidSetNoaParam");
	DBGLOG(INIT, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(struct
				PARAM_CUSTOM_NOA_PARAM_STRUCT);

	if (u4SetBufferLen < sizeof(struct
				    PARAM_CUSTOM_NOA_PARAM_STRUCT))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prNoaParam = (struct PARAM_CUSTOM_NOA_PARAM_STRUCT *)
		     pvSetBuffer;

	kalMemZero(&rCmdNoaParam,
		   sizeof(struct CMD_CUSTOM_NOA_PARAM_STRUCT));
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
				   sizeof(struct CMD_CUSTOM_NOA_PARAM_STRUCT),
				   (uint8_t *) &rCmdNoaParam, pvSetBuffer,
				   u4SetBufferLen);
}

uint32_t
wlanoidSetOppPsParam(IN struct ADAPTER *prAdapter,
		     IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		     OUT uint32_t *pu4SetInfoLen) {
	struct PARAM_CUSTOM_OPPPS_PARAM_STRUCT *prOppPsParam;
	struct CMD_CUSTOM_OPPPS_PARAM_STRUCT rCmdOppPsParam;

	DEBUGFUNC("wlanoidSetOppPsParam");
	DBGLOG(INIT, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(struct
				PARAM_CUSTOM_OPPPS_PARAM_STRUCT);

	if (u4SetBufferLen < sizeof(struct
				    PARAM_CUSTOM_OPPPS_PARAM_STRUCT))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prOppPsParam = (struct PARAM_CUSTOM_OPPPS_PARAM_STRUCT *)
		       pvSetBuffer;

	kalMemZero(&rCmdOppPsParam,
		   sizeof(struct CMD_CUSTOM_OPPPS_PARAM_STRUCT));
	rCmdOppPsParam.u4CTwindowMs = prOppPsParam->u4CTwindowMs;

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_SET_OPPPS_PARAM,
				   TRUE,
				   FALSE,
				   TRUE,
				   nicCmdEventSetCommon,
				   nicOidCmdTimeoutCommon,
				   sizeof(struct CMD_CUSTOM_OPPPS_PARAM_STRUCT),
				   (uint8_t *) &rCmdOppPsParam, pvSetBuffer,
				   u4SetBufferLen);
}

uint32_t
wlanoidSetUApsdParam(IN struct ADAPTER *prAdapter,
		     IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		     OUT uint32_t *pu4SetInfoLen) {
	struct PARAM_CUSTOM_UAPSD_PARAM_STRUCT *prUapsdParam;
	struct CMD_CUSTOM_UAPSD_PARAM_STRUCT rCmdUapsdParam;
	struct PM_PROFILE_SETUP_INFO *prPmProfSetupInfo;
	struct BSS_INFO *prBssInfo;

	DEBUGFUNC("wlanoidSetUApsdParam");
	DBGLOG(INIT, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(struct
				PARAM_CUSTOM_UAPSD_PARAM_STRUCT);

	if (u4SetBufferLen < sizeof(struct
				    PARAM_CUSTOM_UAPSD_PARAM_STRUCT))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prBssInfo = &
		    (prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX]);
	prPmProfSetupInfo = &prBssInfo->rPmProfSetupInfo;

	prUapsdParam = (struct PARAM_CUSTOM_UAPSD_PARAM_STRUCT *)
		       pvSetBuffer;

	kalMemZero(&rCmdUapsdParam,
		   sizeof(struct CMD_CUSTOM_OPPPS_PARAM_STRUCT));
	rCmdUapsdParam.fgEnAPSD = prUapsdParam->fgEnAPSD;
	prAdapter->rWifiVar.fgSupportUAPSD = prUapsdParam->fgEnAPSD;

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

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_SET_UAPSD_PARAM,
				   TRUE,
				   FALSE,
				   TRUE,
				   nicCmdEventSetCommon,
				   nicOidCmdTimeoutCommon,
				   sizeof(struct CMD_CUSTOM_OPPPS_PARAM_STRUCT),
				   (uint8_t *)&rCmdUapsdParam, pvSetBuffer,
				   u4SetBufferLen);
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
uint32_t
wlanoidSetBT(IN struct ADAPTER *prAdapter,
	     IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
	     OUT uint32_t *pu4SetInfoLen) {

	struct PTA_IPC *prPtaIpc;

	DEBUGFUNC("wlanoidSetBT.\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(struct PTA_IPC);
	if (u4SetBufferLen != sizeof(struct PTA_IPC)) {
		/* WARNLOG(("Invalid length %ld\n", u4SetBufferLen)); */
		return WLAN_STATUS_INVALID_LENGTH;
	}

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail to set BT profile because of ACPI_D3\n");
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	ASSERT(pvSetBuffer);
	prPtaIpc = (struct PTA_IPC *) pvSetBuffer;

#if CFG_SUPPORT_BCM && CFG_SUPPORT_BCM_BWCS && CFG_SUPPORT_BCM_BWCS_DEBUG
	DBGLOG(INIT, INFO,
	       "BCM BWCS CMD: BWCS CMD = %02x%02x%02x%02x\n",
	       prPtaIpc->u.aucBTPParams[0], prPtaIpc->u.aucBTPParams[1],
	       prPtaIpc->u.aucBTPParams[2], prPtaIpc->u.aucBTPParams[3]);

	DBGLOG(INIT, INFO,
	       "BCM BWCS CMD: aucBTPParams[0]=%02x, aucBTPParams[1]=%02x, aucBTPParams[2]=%02x, aucBTPParams[3]=%02x.\n",
	       prPtaIpc->u.aucBTPParams[0], prPtaIpc->u.aucBTPParams[1],
	       prPtaIpc->u.aucBTPParams[2], prPtaIpc->u.aucBTPParams[3]);

#endif

	wlanSendSetQueryCmd(prAdapter, CMD_ID_SET_BWCS, TRUE, FALSE, FALSE,
			    NULL, NULL, sizeof(struct PTA_IPC),
			    (uint8_t *) prPtaIpc, NULL, 0);

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
 * \param[out] pu4QueryInfoLen   If the call is successful, returns the number
 *				 of bytes written into the query buffer. If the
 *				 call failed due to invalid length of the query
 *				 buffer, returns the amount of storage needed.
 *
 * \retval WLAN_STATUS_SUCCESS
 * \retval WLAN_STATUS_INVALID_LENGTH
 */
/*----------------------------------------------------------------------------*/
uint32_t
wlanoidQueryBT(IN struct ADAPTER *prAdapter,
	       OUT void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
	       OUT uint32_t *pu4QueryInfoLen) {
	/* P_PARAM_PTA_IPC_T prPtaIpc; */
	/* UINT_32 u4QueryBuffLen; */

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(struct PTA_IPC);

	/* Check for query buffer length */
	if (u4QueryBufferLen != sizeof(struct PTA_IPC)) {
		DBGLOG(REQ, WARN, "Invalid length %u\n", u4QueryBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	ASSERT(pvQueryBuffer);
	/* prPtaIpc = (P_PTA_IPC_T)pvQueryBuffer; */
	/* prPtaIpc->ucCmd = BT_CMD_PROFILE; */
	/* prPtaIpc->ucLen = sizeof(prPtaIpc->u); */
	/* nicPtaGetProfile(prAdapter, (PUINT_8)&prPtaIpc->u, &u4QueryBuffLen);
	 */

	return WLAN_STATUS_SUCCESS;
}

#if 0
uint32_t
wlanoidQueryBtSingleAntenna(IN struct ADAPTER *prAdapter,
			    OUT void *pvQueryBuffer,
			    IN uint32_t u4QueryBufferLen,
			    OUT uint32_t *pu4QueryInfoLen) {
	P_PTA_INFO_T prPtaInfo;
	uint32_t *pu4SingleAntenna;

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(uint32_t);

	/* Check for query buffer length */
	if (u4QueryBufferLen != sizeof(uint32_t)) {
		DBGLOG(REQ, WARN, "Invalid length %lu\n", u4QueryBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	ASSERT(pvQueryBuffer);

	prPtaInfo = &prAdapter->rPtaInfo;
	pu4SingleAntenna = (uint32_t *) pvQueryBuffer;

	if (prPtaInfo->fgSingleAntenna) {
		/* DBGLOG(INIT, INFO, (KERN_WARNING DRV_NAME
		 *        "Q Single Ant = 1\r\n"));
		 */
		*pu4SingleAntenna = 1;
	} else {
		/* DBGLOG(INIT, INFO, (KERN_WARNING DRV_NAME
		 *        "Q Single Ant = 0\r\n"));
		 */
		*pu4SingleAntenna = 0;
	}

	return WLAN_STATUS_SUCCESS;
}

uint32_t
wlanoidSetBtSingleAntenna(IN struct ADAPTER *prAdapter,
			  IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			  OUT uint32_t *pu4SetInfoLen) {

	uint32_t *pu4SingleAntenna;
	uint32_t u4SingleAntenna;
	P_PTA_INFO_T prPtaInfo;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	prPtaInfo = &prAdapter->rPtaInfo;

	*pu4SetInfoLen = sizeof(uint32_t);
	if (u4SetBufferLen != sizeof(uint32_t))
		return WLAN_STATUS_INVALID_LENGTH;

	if (IS_ARB_IN_RFTEST_STATE(prAdapter))
		return WLAN_STATUS_SUCCESS;

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail to set antenna because of ACPI_D3\n");
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	ASSERT(pvSetBuffer);
	pu4SingleAntenna = (uint32_t *) pvSetBuffer;
	u4SingleAntenna = *pu4SingleAntenna;

	if (u4SingleAntenna == 0) {
		/* DBGLOG(INIT, INFO, (KERN_WARNING DRV_NAME
		 * "Set Single Ant = 0\r\n"));
		 */
		prPtaInfo->fgSingleAntenna = FALSE;
	} else {
		/* DBGLOG(INIT, INFO, (KERN_WARNING DRV_NAME
		 *        "Set Single Ant = 1\r\n"));
		 */
		prPtaInfo->fgSingleAntenna = TRUE;
	}
	ptaFsmRunEventSetConfig(prAdapter, &prPtaInfo->rPtaParam);

	return WLAN_STATUS_SUCCESS;
}

#if CFG_SUPPORT_BCM && CFG_SUPPORT_BCM_BWCS
uint32_t
wlanoidQueryPta(IN struct ADAPTER *prAdapter,
		OUT void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
		OUT uint32_t *pu4QueryInfoLen) {
	P_PTA_INFO_T prPtaInfo;
	uint32_t *pu4Pta;

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(uint32_t);

	/* Check for query buffer length */
	if (u4QueryBufferLen != sizeof(uint32_t)) {
		DBGLOG(REQ, WARN, "Invalid length %lu\n", u4QueryBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	ASSERT(pvQueryBuffer);

	prPtaInfo = &prAdapter->rPtaInfo;
	pu4Pta = (uint32_t *) pvQueryBuffer;

	if (prPtaInfo->fgEnabled) {
		/* DBGLOG(INIT, INFO, (KERN_WARNING DRV_NAME"PTA = 1\r\n")); */
		*pu4Pta = 1;
	} else {
		/* DBGLOG(INIT, INFO, (KERN_WARNING DRV_NAME"PTA = 0\r\n")); */
		*pu4Pta = 0;
	}

	return WLAN_STATUS_SUCCESS;
}

uint32_t
wlanoidSetPta(IN struct ADAPTER *prAdapter,
	      IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
	      OUT uint32_t *pu4SetInfoLen) {
	uint32_t *pu4PtaCtrl;
	uint32_t u4PtaCtrl;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(uint32_t);
	if (u4SetBufferLen != sizeof(uint32_t))
		return WLAN_STATUS_INVALID_LENGTH;

	if (IS_ARB_IN_RFTEST_STATE(prAdapter))
		return WLAN_STATUS_SUCCESS;

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail to set BT setting because of ACPI_D3\n");
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	ASSERT(pvSetBuffer);
	pu4PtaCtrl = (uint32_t *) pvSetBuffer;
	u4PtaCtrl = *pu4PtaCtrl;

	if (u4PtaCtrl == 0) {
		/* DBGLOG(INIT, INFO, (KERN_WARNING DRV_NAME"Set Pta= 0\r\n"));
		 */
		nicPtaSetFunc(prAdapter, FALSE);
	} else {
		/* DBGLOG(INIT, INFO, (KERN_WARNING DRV_NAME"Set Pta= 1\r\n"));
		 */
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
uint32_t
wlanoidSetTxPower(IN struct ADAPTER *prAdapter,
		  IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		  OUT uint32_t *pu4SetInfoLen) {
	struct SET_TXPWR_CTRL *pTxPwr = (struct SET_TXPWR_CTRL *)
					pvSetBuffer;
	struct SET_TXPWR_CTRL *prCmd;
	uint32_t i;
	uint32_t rStatus;

	DEBUGFUNC("wlanoidSetTxPower");
	DBGLOG(REQ, LOUD, "\r\n");

	prCmd = cnmMemAlloc(prAdapter, RAM_TYPE_BUF,
			    sizeof(struct SET_TXPWR_CTRL));
	kalMemZero(prCmd, sizeof(struct SET_TXPWR_CTRL));
	prCmd->c2GLegacyStaPwrOffset =
		pTxPwr->c2GLegacyStaPwrOffset;
	prCmd->c2GHotspotPwrOffset = pTxPwr->c2GHotspotPwrOffset;
	prCmd->c2GP2pPwrOffset = pTxPwr->c2GP2pPwrOffset;
	prCmd->c2GBowPwrOffset = pTxPwr->c2GBowPwrOffset;
	prCmd->c5GLegacyStaPwrOffset =
		pTxPwr->c5GLegacyStaPwrOffset;
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
	DBGLOG(INIT, INFO, "c2GLegacyStaPwrOffset=%d\n",
	       pTxPwr->c2GLegacyStaPwrOffset);
	DBGLOG(INIT, INFO, "c2GHotspotPwrOffset=%d\n",
	       pTxPwr->c2GHotspotPwrOffset);
	DBGLOG(INIT, INFO, "c2GP2pPwrOffset=%d\n",
	       pTxPwr->c2GP2pPwrOffset);
	DBGLOG(INIT, INFO, "c2GBowPwrOffset=%d\n",
	       pTxPwr->c2GBowPwrOffset);
	DBGLOG(INIT, INFO, "c5GLegacyStaPwrOffset=%d\n",
	       pTxPwr->c5GLegacyStaPwrOffset);
	DBGLOG(INIT, INFO, "c5GHotspotPwrOffset=%d\n",
	       pTxPwr->c5GHotspotPwrOffset);
	DBGLOG(INIT, INFO, "c5GP2pPwrOffset=%d\n",
	       pTxPwr->c5GP2pPwrOffset);
	DBGLOG(INIT, INFO, "c5GBowPwrOffset=%d\n",
	       pTxPwr->c5GBowPwrOffset);
	DBGLOG(INIT, INFO, "ucConcurrencePolicy=%d\n",
	       pTxPwr->ucConcurrencePolicy);

	for (i = 0; i < 14; i++)
		DBGLOG(INIT, INFO, "acTxPwrLimit2G[%d]=%d\n", i,
		       pTxPwr->acTxPwrLimit2G[i]);

	for (i = 0; i < 4; i++)
		DBGLOG(INIT, INFO, "acTxPwrLimit5G[%d]=%d\n", i,
		       pTxPwr->acTxPwrLimit5G[i]);
#endif

	rStatus = wlanSendSetQueryCmd(prAdapter,	/* prAdapter */
			CMD_ID_SET_TXPWR_CTRL,	/* ucCID */
			TRUE,	/* fgSetQuery */
			FALSE,	/* fgNeedResp */
			g_fgIsOid,	/* fgIsOid */
			nicCmdEventSetCommon, nicOidCmdTimeoutCommon,
			sizeof(struct SET_TXPWR_CTRL),	/* u4SetQueryInfoLen */
			(uint8_t *) prCmd,	/* pucInfoBuffer */
			NULL,	/* pvSetQueryBuffer */
			0	/* u4SetQueryBufferLen */
			);

	/* ASSERT(rStatus == WLAN_STATUS_PENDING); */
	cnmMemFree(prAdapter, prCmd);

	return rStatus;

}

uint32_t wlanSendMemDumpCmd(IN struct ADAPTER *prAdapter,
			    IN void *pvQueryBuffer,
			    IN uint32_t u4QueryBufferLen) {
	struct PARAM_CUSTOM_MEM_DUMP_STRUCT *prMemDumpInfo;
	struct CMD_DUMP_MEM *prCmdDumpMem;
	struct CMD_DUMP_MEM rCmdDumpMem;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4MemSize = PARAM_MEM_DUMP_MAX_SIZE;

	uint32_t u4RemainLeng = 0;
	uint32_t u4CurAddr = 0;
	uint8_t ucFragNum = 0;

	prCmdDumpMem = &rCmdDumpMem;
	prMemDumpInfo = (struct PARAM_CUSTOM_MEM_DUMP_STRUCT *)
			pvQueryBuffer;

	u4RemainLeng = prMemDumpInfo->u4RemainLength;
	u4CurAddr = prMemDumpInfo->u4Address +
		    prMemDumpInfo->u4Length;
	ucFragNum = prMemDumpInfo->ucFragNum + 1;

	/* Query. If request length is larger than max length, do it as ping
	 * pong. Send a command and wait for a event. Send next command while
	 * the event is received.
	 */
	do {
		uint32_t u4CurLeng = 0;

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
		       ucFragNum, prCmdDumpMem->u4Address,
		       prCmdDumpMem->u4Length, prCmdDumpMem->u4RemainLength);

		rStatus = wlanSendSetQueryCmd(prAdapter,
					      CMD_ID_DUMP_MEM,
					      FALSE,
					      TRUE,
					      TRUE,
					      nicCmdEventQueryMemDump,
					      nicOidCmdTimeoutCommon,
					      sizeof(struct CMD_DUMP_MEM),
					      (uint8_t *) prCmdDumpMem,
					      pvQueryBuffer, u4QueryBufferLen);

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
uint32_t
wlanoidQueryMemDump(IN struct ADAPTER *prAdapter,
		    IN void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
		    OUT uint32_t *pu4QueryInfoLen) {
	struct PARAM_CUSTOM_MEM_DUMP_STRUCT *prMemDumpInfo;

	DEBUGFUNC("wlanoidQueryMemDump");
	DBGLOG(INIT, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(uint32_t);

	prMemDumpInfo = (struct PARAM_CUSTOM_MEM_DUMP_STRUCT *)
			pvQueryBuffer;
	DBGLOG(REQ, TRACE, "Dump 0x%X, len %u\n",
	       prMemDumpInfo->u4Address, prMemDumpInfo->u4Length);

	prMemDumpInfo->u4RemainLength = prMemDumpInfo->u4Length;
	prMemDumpInfo->u4Length = 0;
	prMemDumpInfo->ucFragNum = 0;

	return wlanSendMemDumpCmd(prAdapter, pvQueryBuffer,
				  u4QueryBufferLen);

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
uint32_t
wlanoidSetP2pMode(IN struct ADAPTER *prAdapter,
		  IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		  OUT uint32_t *pu4SetInfoLen) {
	uint32_t status = WLAN_STATUS_SUCCESS;
	struct PARAM_CUSTOM_P2P_SET_STRUCT *prSetP2P =
		(struct PARAM_CUSTOM_P2P_SET_STRUCT *) NULL;
	/* P_MSG_P2P_NETDEV_REGISTER_T prP2pNetdevRegMsg =
	 *				P_MSG_P2P_NETDEV_REGISTER_T)NULL;
	 */
	DEBUGFUNC("wlanoidSetP2pMode");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(struct PARAM_CUSTOM_P2P_SET_STRUCT);
	if (u4SetBufferLen < sizeof(struct
				    PARAM_CUSTOM_P2P_SET_STRUCT)) {
		DBGLOG(REQ, WARN, "Invalid length %u\n", u4SetBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	prSetP2P = (struct PARAM_CUSTOM_P2P_SET_STRUCT *)
		   pvSetBuffer;

	DBGLOG(P2P, TRACE, "Set P2P enable[%d] mode[%d]\n",
	       prSetP2P->u4Enable, prSetP2P->u4Mode);

	/*
	 *    enable = 1, mode = 0  => init P2P network
	 *    enable = 1, mode = 1  => init Soft AP network
	 *    enable = 0            => uninit P2P/AP network
	 *    enable = 1, mode = 2  => init dual Soft AP network
	 *    enable = 1, mode = 3  => init AP+P2P network
	 */


	DBGLOG(P2P, INFO, "P2P Compile as (%d)p2p-like interface\n",
	       KAL_P2P_NUM);

	if (prSetP2P->u4Mode >= RUNNING_P2P_MODE_NUM) {
		DBGLOG(P2P, ERROR, "P2P interface mode(%d) is wrong\n",
		       prSetP2P->u4Mode);
		ASSERT(0);
	}

	if (prSetP2P->u4Enable) {
		p2pSetMode(prSetP2P->u4Mode);

		if (p2pLaunch(prAdapter->prGlueInfo)) {
			/* ToDo:: ASSERT */
			ASSERT(prAdapter->fgIsP2PRegistered);
			if (prAdapter->rWifiVar.ucApUapsd
			    && (prSetP2P->u4Mode != RUNNING_P2P_MODE)) {
				DBGLOG(OID, INFO,
				       "wlanoidSetP2pMode Default enable ApUapsd\n");
				setApUapsdEnable(prAdapter, TRUE);
			}
		} else {
			DBGLOG(P2P, ERROR, "P2P Launch Failed\n");
			status = WLAN_STATUS_FAILURE;
		}

	} else {
		if (prAdapter->fgIsP2PRegistered)
			p2pRemove(prAdapter->prGlueInfo);

	}

#if 0
	prP2pNetdevRegMsg = (struct MSG_P2P_NETDEV_REGISTER *)
				cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
				(sizeof(struct MSG_P2P_NETDEV_REGISTER)));

	if (prP2pNetdevRegMsg == NULL) {
		ASSERT(FALSE);
		status = WLAN_STATUS_RESOURCES;
		return status;
	}

	prP2pNetdevRegMsg->rMsgHdr.eMsgId =
		MID_MNY_P2P_NET_DEV_REGISTER;
	prP2pNetdevRegMsg->fgIsEnable = (prSetP2P->u4Enable == 1) ?
					TRUE : FALSE;
	prP2pNetdevRegMsg->ucMode = (uint8_t) prSetP2P->u4Mode;

	mboxSendMsg(prAdapter, MBOX_ID_0,
		    (struct MSG_HDR *) prP2pNetdevRegMsg, MSG_SEND_METHOD_BUF);
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
uint32_t
wlanoidSetGtkRekeyData(IN struct ADAPTER *prAdapter,
		       IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		       OUT uint32_t *pu4SetInfoLen) {
	struct GLUE_INFO *prGlueInfo;
	struct CMD_INFO *prCmdInfo;
	struct WIFI_CMD *prWifiCmd;
	uint8_t ucCmdSeqNum;
	struct BSS_INFO *prBssInfo;

	DBGLOG(REQ, INFO, "wlanoidSetGtkRekeyData\n");

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(RSN, WARN,
		       "Fail in set rekey! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	prBssInfo = prAdapter->prAisBssInfo;

	*pu4SetInfoLen = u4SetBufferLen;

	prGlueInfo = prAdapter->prGlueInfo;
	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter,
			(CMD_HDR_SIZE + sizeof(struct PARAM_GTK_REKEY_DATA)));

	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}
	/* increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);
	DBGLOG(REQ, INFO, "ucCmdSeqNum = %d\n", ucCmdSeqNum);

	/* compose PARAM_GTK_REKEY_DATA cmd pkt */
	prCmdInfo->eCmdType = COMMAND_TYPE_NETWORK_IOCTL;
	prCmdInfo->u2InfoBufLen = CMD_HDR_SIZE + sizeof(
					  struct PARAM_GTK_REKEY_DATA);
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
	prWifiCmd = (struct WIFI_CMD *) (prCmdInfo->pucInfoBuffer);
	prWifiCmd->u2TxByteCount = prCmdInfo->u2InfoBufLen;
	prWifiCmd->u2PQ_ID = CMD_PQ_ID;
	prWifiCmd->ucPktTypeID = CMD_PACKET_TYPE_ID;
	prWifiCmd->ucCID = prCmdInfo->ucCID;
	prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
	prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;

	if (u4SetBufferLen > 0 && pvSetBuffer != NULL)
		kalMemCopy(prWifiCmd->aucBuffer, (uint8_t *) pvSetBuffer,
			u4SetBufferLen);

	/* insert into prCmdQueue */
	kalEnqueueCommand(prGlueInfo,
			  (struct QUE_ENTRY *) prCmdInfo);

	/* wakeup txServiceThread later */
	GLUE_SET_EVENT(prGlueInfo);

	return WLAN_STATUS_PENDING;

}				/* wlanoidSetGtkRekeyData */

#if CFG_SUPPORT_SCHED_SCAN
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
uint32_t
wlanoidSetStartSchedScan(IN struct ADAPTER *prAdapter,
			 IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			 OUT uint32_t *pu4SetInfoLen) {
	struct PARAM_SCHED_SCAN_REQUEST *prSchedScanRequest;

	DEBUGFUNC("wlanoidSetStartSchedScan()");

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in set scheduled scan! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	ASSERT(pu4SetInfoLen);
	*pu4SetInfoLen = 0;

	if (u4SetBufferLen != sizeof(struct
				     PARAM_SCHED_SCAN_REQUEST))
		return WLAN_STATUS_INVALID_LENGTH;
	else if (pvSetBuffer == NULL)
		return WLAN_STATUS_INVALID_DATA;
	else if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) ==
		 PARAM_MEDIA_STATE_CONNECTED
		 && prAdapter->fgEnOnlineScan == FALSE)
		return WLAN_STATUS_FAILURE;

	if (prAdapter->fgIsRadioOff) {
		DBGLOG(REQ, WARN,
		       "Return from BSSID list scan! (radio off). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_SUCCESS;
	}

	prSchedScanRequest = (struct PARAM_SCHED_SCAN_REQUEST *)
			     pvSetBuffer;

	if (scnFsmSchedScanRequest(prAdapter,
				   prSchedScanRequest) == TRUE)
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
uint32_t
wlanoidSetStopSchedScan(IN struct ADAPTER *prAdapter,
			IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			OUT uint32_t *pu4SetInfoLen) {
	uint32_t ret;

	ASSERT(prAdapter);

	/* ask SCN module to stop scan request */
	if (scnFsmSchedScanStopRequest(prAdapter) == TRUE)
		ret = WLAN_STATUS_SUCCESS;
	else {
		DBGLOG(REQ, WARN, "scnFsmSchedScanStopRequest failed.\n");
		ret = WLAN_STATUS_FAILURE;
	}
	return ret;
}
#endif /* CFG_SUPPORT_SCHED_SCAN */

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
uint32_t wlanoidResetBAScoreboard(IN struct ADAPTER *
				  prAdapter, IN void *pvSetBuffer,
				  IN uint32_t u4SetBufferLen) {
	uint32_t rStatus;

	DEBUGFUNC("wlanoidResetBAScoreboard");
	DBGLOG(REQ, WARN, "[Puff]wlanoidResetBAScoreboard\n");

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);

	rStatus = wlanSendSetQueryCmd(prAdapter,	/* prAdapter */
				CMD_ID_RESET_BA_SCOREBOARD, /* ucCID */
				TRUE,	/* fgSetQuery */
				FALSE,	/* fgNeedResp */
				TRUE,	/* fgIsOid */
				NULL,	/* pfCmdDoneHandler */
				NULL,	/* pfCmdTimeoutHandler */
				u4SetBufferLen,	/* u4SetQueryInfoLen */
				(uint8_t *) pvSetBuffer, /* pucInfoBuffer */
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

uint32_t
batchSetCmd(IN struct ADAPTER *prAdapter,
	    IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
	    OUT uint32_t *pu4WritenLen) {
	struct CHANNEL_INFO *prRfChannelInfo;
	struct CMD_BATCH_REQ rCmdBatchReq;

	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	int8_t *head, *p, *p2;
	uint32_t tokens;
	int32_t scanfreq, mscan, bestn, rtt;
	char *pcTemp;
	/* CHAR c_scanfreq[4], c_mscan[4], c_bestn[4], c_rtt[4], c_channel[100];
	 */
	/* INT_32 ch_type; */
	uint32_t u4Value = 0;
	int32_t i4Ret = 0;

	DBGLOG(SCN, TRACE, "[BATCH] command=%s, len=%u\n",
	       (char *)pvSetBuffer, u4SetBufferLen);

	if (!pu4WritenLen)
		return -EINVAL;
	*pu4WritenLen = 0;

	if (u4SetBufferLen < kalStrLen(CMD_WLS_BATCHING)) {
		DBGLOG(SCN, TRACE, "[BATCH] invalid len %d\n",
		       u4SetBufferLen);
		return -EINVAL;
	}

	head = pvSetBuffer + kalStrLen(CMD_WLS_BATCHING) + 1;
	kalMemSet(&rCmdBatchReq, 0, sizeof(struct CMD_BATCH_REQ));

	if (!kalStrnCmp(head, BATCHING_SET,
			kalStrLen(BATCHING_SET))) {

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
			DBGLOG(SCN, TRACE, "[BATCH] Parse RTT fail. head=%s\n",
			       head);
			return -EINVAL;
		}
		tokens = sscanf(p, "RTT=%d", &rtt);
		if (tokens != 1) {
			DBGLOG(SCN, TRACE,
			       "[BATCH] Parse fail: tokens=%d, rtt=%d\n",
			       tokens, rtt);
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
		 *	p = '.'; // remove '>' because sscanf can not parse <%s>
		 * }
		 */
		/* tokens = sscanf(head, "CHANNEL=<%s", c_channel);
		 * if (tokens != 1) {
		 *         DBGLOG(SCN, TRACE, "[BATCH] Parse fail: tokens=%d,
		 *                CHANNEL=<%s>\n", tokens, c_channel);
		 *         return -EINVAL;
		 * }
		 */
		rCmdBatchReq.ucChannelType = SCAN_CHANNEL_SPECIFIED;
		rCmdBatchReq.ucChannelListNum = 0;
		prRfChannelInfo = &rCmdBatchReq.arChannelList[0];
		p = head + kalStrLen(PARAM_CHANNEL) + 2; /* c_channel; */
		pcTemp = (char *)p;
		while ((p2 = kalStrSep(&pcTemp, ",")) != NULL) {
			if (p2 == NULL || *p2 == 0)
				break;
			if (*p2 == '\0')
				continue;
			if (*p2 == 'A') {
				rCmdBatchReq.ucChannelType =
					rCmdBatchReq.ucChannelType ==
					SCAN_CHANNEL_2G4 ?
					SCAN_CHANNEL_FULL : SCAN_CHANNEL_5G;
			} else if (*p2 == 'B') {
				rCmdBatchReq.ucChannelType =
					rCmdBatchReq.ucChannelType ==
					SCAN_CHANNEL_5G ?
					SCAN_CHANNEL_FULL : SCAN_CHANNEL_2G4;
			} else {

				/* Translate Freq from MHz to channel number. */
				/* prRfChannelInfo->ucChannelNum =
				 *			kalStrtol(p2, NULL, 0);
				 */
				i4Ret = kalkStrtou32(p2, 0, &u4Value);
				if (i4Ret)
					DBGLOG(SCN, TRACE,
					       "parse ucChannelNum error i4Ret=%d\n",
					       i4Ret);
				prRfChannelInfo->ucChannelNum =
							(uint8_t) u4Value;
				DBGLOG(SCN, TRACE,
				       "Scanning Channel:%d, freq: %d\n",
				       prRfChannelInfo->ucChannelNum,
				       nicChannelNum2Freq(
				       prRfChannelInfo->ucChannelNum));
				prRfChannelInfo->ucBand =
					prRfChannelInfo->ucChannelNum < 15
							? BAND_2G4 : BAND_5G;

				rCmdBatchReq.ucChannelListNum++;
				if (rCmdBatchReq.ucChannelListNum >= 32)
					break;
				prRfChannelInfo++;
			}
		}

		/* set channel for test */
#if 0
		rCmdBatchReq.ucChannelType =
			4;	/* SCAN_CHANNEL_SPECIFIED; */
		rCmdBatchReq.ucChannelListNum = 0;
		prRfChannelInfo = &rCmdBatchReq.arChannelList[0];
		for (i = 1; i <= 14; i++) {

			/* filter out some */
			if (i == 1 || i == 5 || i == 11)
				continue;

			/* Translate Freq from MHz to channel number. */
			prRfChannelInfo->ucChannelNum = i;
			DBGLOG(SCN, TRACE, "Scanning Channel:%d, freq: %d\n",
			       prRfChannelInfo->ucChannelNum,
			       nicChannelNum2Freq(
			       prRfChannelInfo->ucChannelNum));
			prRfChannelInfo->ucBand = BAND_2G4;

			rCmdBatchReq.ucChannelListNum++;
			prRfChannelInfo++;
		}
#endif
#if 0
		rCmdBatchReq.ucChannelType = 0;	/* SCAN_CHANNEL_FULL; */
#endif

		rCmdBatchReq.u4Scanfreq = scanfreq;
		rCmdBatchReq.ucMScan = mscan > CFG_BATCH_MAX_MSCAN ?
				       CFG_BATCH_MAX_MSCAN : mscan;
		rCmdBatchReq.ucBestn = bestn;
		rCmdBatchReq.ucRtt = rtt;
		DBGLOG(SCN, TRACE,
		       "[BATCH] SCANFREQ=%d MSCAN=%d BESTN=%d RTT=%d\n",
		       rCmdBatchReq.u4Scanfreq, rCmdBatchReq.ucMScan,
		       rCmdBatchReq.ucBestn, rCmdBatchReq.ucRtt);

		if (rCmdBatchReq.ucChannelType != SCAN_CHANNEL_SPECIFIED) {
			DBGLOG(SCN, TRACE, "[BATCH] CHANNELS = %s\n",
			       rCmdBatchReq.ucChannelType == SCAN_CHANNEL_FULL ?
			       "FULL" : rCmdBatchReq.ucChannelType ==
			       SCAN_CHANNEL_2G4 ? "2.4G all" : "5G all");
		} else {
			DBGLOG(SCN, TRACE, "[BATCH] CHANNEL list\n");
			prRfChannelInfo = &rCmdBatchReq.arChannelList[0];
			for (tokens = 0; tokens < rCmdBatchReq.ucChannelListNum;
			     tokens++) {
				DBGLOG(SCN, TRACE, "[BATCH] %s, %d\n",
				       prRfChannelInfo->ucBand
				       == BAND_2G4 ? "2.4G" : "5G",
				       prRfChannelInfo->ucChannelNum);
				prRfChannelInfo++;
			}
		}

		rCmdBatchReq.ucSeqNum = 1;
		rCmdBatchReq.ucNetTypeIndex = KAL_NETWORK_TYPE_AIS_INDEX;
		rCmdBatchReq.ucCmd = SCAN_BATCH_REQ_START;

		*pu4WritenLen = kalSnprintf(pvSetBuffer, 3, "%d",
					    rCmdBatchReq.ucMScan);

	} else if (!kalStrnCmp(head, BATCHING_STOP,
			       kalStrLen(BATCHING_STOP))) {

		DBGLOG(SCN, TRACE, "XXX Stop Batch Scan XXX\n");

		rCmdBatchReq.ucSeqNum = 1;
		rCmdBatchReq.ucNetTypeIndex = KAL_NETWORK_TYPE_AIS_INDEX;
		rCmdBatchReq.ucCmd = SCAN_BATCH_REQ_STOP;
	} else {
		return -EINVAL;
	}

	rStatus = wlanSendSetQueryCmd(prAdapter, CMD_ID_SET_BATCH_REQ,
				      TRUE, FALSE, g_fgIsOid, NULL, NULL,
				      sizeof(struct CMD_BATCH_REQ),
				      (uint8_t *) &rCmdBatchReq, NULL, 0);

	/* kalMemSet(pvSetBuffer, 0, u4SetBufferLen); */
	/* rStatus = kalSnprintf(pvSetBuffer, 2, "%s", "OK"); */

	/* exit: */
	return rStatus;
}

uint32_t
batchGetCmd(IN struct ADAPTER *prAdapter,
	    OUT void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
	    OUT uint32_t *pu4QueryInfoLen) {
	struct CMD_BATCH_REQ rCmdBatchReq;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	struct EVENT_BATCH_RESULT *prEventBatchResult;
	/* UINT_32 i; */

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	prEventBatchResult = (struct EVENT_BATCH_RESULT *)
			     pvQueryBuffer;

	DBGLOG(SCN, TRACE, "XXX Get Batch Scan Result (%d) XXX\n",
	       prEventBatchResult->ucScanCount);

	*pu4QueryInfoLen = sizeof(struct EVENT_BATCH_RESULT);

	rCmdBatchReq.ucSeqNum = 2;
	rCmdBatchReq.ucCmd = SCAN_BATCH_REQ_RESULT;
	rCmdBatchReq.ucMScan =
		prEventBatchResult->ucScanCount; /* Get which round result */

	rStatus = wlanSendSetQueryCmd(prAdapter,
				      CMD_ID_SET_BATCH_REQ,
				      FALSE,
				      TRUE,
				      g_fgIsOid,
				      nicCmdEventBatchScanResult,
				      nicOidCmdTimeoutCommon,
				      sizeof(struct CMD_BATCH_REQ),
				      (uint8_t *) &rCmdBatchReq,
				      (void *) pvQueryBuffer,
				      u4QueryBufferLen);

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
uint32_t
wlanoidSetBatchScanReq(IN struct ADAPTER *prAdapter,
		       IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		       OUT uint32_t *pu4SetInfoLen) {
	return batchSetCmd(prAdapter, pvSetBuffer, u4SetBufferLen,
			   pu4SetInfoLen);
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
uint32_t
wlanoidQueryBatchScanResult(IN struct ADAPTER *prAdapter,
			    OUT void *pvQueryBuffer,
			    IN uint32_t u4QueryBufferLen,
			    OUT uint32_t *pu4QueryInfoLen) {
	return batchGetCmd(prAdapter, pvQueryBuffer,
			   u4QueryBufferLen, pu4QueryInfoLen);

}				/* end of wlanoidQueryBatchScanResult() */

#endif /* CFG_SUPPORT_BATCH_SCAN */

#if CFG_SUPPORT_PASSPOINT
/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called by HS2.0 to set the assoc info, which is needed
 *	  to add to Association request frame while join HS2.0 AP.
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
uint32_t
wlanoidSetHS20Info(IN struct ADAPTER *prAdapter,
		   IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		   OUT uint32_t *pu4SetInfoLen) {
	struct IE_HS20_INDICATION *prHS20IndicationIe;

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	DEBUGFUNC("wlanoidSetHS20AssocInfo");
	DBGLOG(OID, LOUD, "\r\n");

	if (u4SetBufferLen == 0)
		return WLAN_STATUS_INVALID_LENGTH;

	*pu4SetInfoLen = u4SetBufferLen;

	prHS20IndicationIe = (struct IE_HS20_INDICATION *)
			     pvSetBuffer;

	prAdapter->prGlueInfo->ucHotspotConfig =
		prHS20IndicationIe->ucHotspotConfig;
	prAdapter->prGlueInfo->fgConnectHS20AP = TRUE;

	DBGLOG(SEC, TRACE, "HS20 IE sz %u\n", u4SetBufferLen);

	kalMemCopy(prAdapter->prGlueInfo->aucHS20AssocInfoIE,
		   pvSetBuffer, u4SetBufferLen);
	prAdapter->prGlueInfo->u2HS20AssocInfoIELen =
		(uint16_t) u4SetBufferLen;
	DBGLOG(SEC, TRACE, "HS20 Assoc Info IE sz %u\n",
	       u4SetBufferLen);

	return WLAN_STATUS_SUCCESS;

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called by WSC to set the assoc info, which is needed
 *	  to add to Association request frame while join WPS AP.
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
uint32_t
wlanoidSetInterworkingInfo(IN struct ADAPTER *prAdapter,
			   IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			   OUT uint32_t *pu4SetInfoLen) {
	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called by WSC to set the Roaming Consortium IE info,
 *	  which is needed to add to Association request frame while join WPS AP.
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
uint32_t
wlanoidSetRoamingConsortiumIEInfo(IN struct ADAPTER *
				  prAdapter,
				  IN void *pvSetBuffer,
				  IN uint32_t u4SetBufferLen,
				  OUT uint32_t *pu4SetInfoLen) {
	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to set_bssid_pool
 *
 * \param[in] prAdapter      Pointer to the Adapter structure.
 * \param[in] pvSetBuffer    Pointer to the buffer that holds the data to be
 *			     set.
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
wlanoidSetHS20BssidPool(IN struct ADAPTER *prAdapter,
			IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			OUT uint32_t *pu4SetInfoLen) {
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	if (u4SetBufferLen)
		ASSERT(pvSetBuffer);

	if (u4SetBufferLen < sizeof(struct
				    PARAM_HS20_SET_BSSID_POOL)) {
		*pu4SetInfoLen = sizeof(struct PARAM_HS20_SET_BSSID_POOL);
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}

	rWlanStatus = hs20SetBssidPool(prAdapter, pvSetBuffer,
				       KAL_NETWORK_TYPE_AIS_INDEX);

	return rWlanStatus;
}				/* end of wlanoidSendHS20GASRequest() */

#endif /* CFG_SUPPORT_PASSPOINT */

#if CFG_SUPPORT_SNIFFER
uint32_t
wlanoidSetMonitor(IN struct ADAPTER *prAdapter,
		  IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		  OUT uint32_t *pu4SetInfoLen) {
	struct PARAM_CUSTOM_MONITOR_SET_STRUCT *prMonitorSetInfo;
	struct CMD_MONITOR_SET_INFO rCmdMonitorSetInfo;
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidSetMonitor");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(struct
				PARAM_CUSTOM_MONITOR_SET_STRUCT);

	if (u4SetBufferLen < sizeof(struct
				    PARAM_CUSTOM_MONITOR_SET_STRUCT))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prMonitorSetInfo = (struct PARAM_CUSTOM_MONITOR_SET_STRUCT
			    *) pvSetBuffer;

	rCmdMonitorSetInfo.ucEnable = prMonitorSetInfo->ucEnable;
	rCmdMonitorSetInfo.ucBand = prMonitorSetInfo->ucBand;
	rCmdMonitorSetInfo.ucPriChannel =
		prMonitorSetInfo->ucPriChannel;
	rCmdMonitorSetInfo.ucSco = prMonitorSetInfo->ucSco;
	rCmdMonitorSetInfo.ucChannelWidth =
		prMonitorSetInfo->ucChannelWidth;
	rCmdMonitorSetInfo.ucChannelS1 =
		prMonitorSetInfo->ucChannelS1;
	rCmdMonitorSetInfo.ucChannelS2 =
		prMonitorSetInfo->ucChannelS2;

	rWlanStatus = wlanSendSetQueryCmd(prAdapter,
					  CMD_ID_SET_MONITOR,
					  TRUE,
					  FALSE,
					  g_fgIsOid,
					  nicCmdEventSetCommon,
					  nicOidCmdTimeoutCommon,
					  sizeof(struct CMD_MONITOR_SET_INFO),
					  (uint8_t *) &rCmdMonitorSetInfo,
					  pvSetBuffer,
					  u4SetBufferLen);
#if CFG_SUPPORT_TX_BEACON_STA_MODE
	if (rCmdMonitorSetInfo.ucEnable != 0)
		nicActivateNetwork(prAdapter, 0);
	else
		nicDeactivateNetwork(prAdapter, 0);
#endif
	return rWlanStatus;
}
#endif


#if CFG_SUPPORT_ADVANCE_CONTROL
uint32_t
wlanoidAdvCtrl(IN struct ADAPTER *prAdapter,
	OUT void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
	OUT uint32_t *pu4QueryInfoLen)
{
	return wlanAdvCtrl(prAdapter,
			pvQueryBuffer,
			u4QueryBufferLen,
			pu4QueryInfoLen,
			g_fgIsOid);
}

uint32_t
wlanAdvCtrl(IN struct ADAPTER *prAdapter,
			OUT void *pvQueryBuffer,
			IN uint32_t u4QueryBufferLen,
			OUT uint32_t *pu4QueryInfoLen,
			IN uint8_t fgIsOid)
{
	struct CMD_ADV_CONFIG_HEADER *cmd;
	uint16_t type = 0;
	uint32_t len;
	u_int8_t fgSetQuery = FALSE;
	u_int8_t fgNeedResp = TRUE;

	DEBUGFUNC("wlanoidAdvCtrl");

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in query receive error! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		*pu4QueryInfoLen = sizeof(uint32_t);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (u4QueryBufferLen < sizeof(*cmd)) {
		DBGLOG(REQ, WARN, "Too short length %u\n", u4QueryBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	cmd = (struct CMD_ADV_CONFIG_HEADER *)pvQueryBuffer;

	if (cmd->u2Type & CMD_ADV_CONTROL_SET) {
		fgSetQuery = TRUE;
		fgNeedResp = FALSE;
	}

	type = cmd->u2Type;
	type &= ~CMD_ADV_CONTROL_SET;
	DBGLOG(RSN, INFO, "%s cmd type %d\n", __func__, cmd->u2Type);
	switch (type) {
	case CMD_PTA_CONFIG_TYPE:
		*pu4QueryInfoLen = sizeof(struct CMD_PTA_CONFIG);
		len = sizeof(struct CMD_PTA_CONFIG);
		break;
#ifdef CFG_SUPPORT_EXT_PTA_DEBUG_COMMAND
	case CMD_EXT_PTA_CONFIG_TYPE:
		*pu4QueryInfoLen = sizeof(CMD_PTA_CONFIG_T);
		len = sizeof(CMD_PTA_CONFIG_T);
		break;
#endif
	case CMD_GET_REPORT_TYPE:
		*pu4QueryInfoLen = sizeof(struct CMD_GET_TRAFFIC_REPORT);
		len = sizeof(struct CMD_GET_TRAFFIC_REPORT);
		break;
	case CMD_NOISE_HISTOGRAM_TYPE:
#if CFG_IPI_2CHAIN_SUPPORT
	case CMD_NOISE_HISTOGRAM_TYPE2:
#endif
		*pu4QueryInfoLen = sizeof(struct CMD_NOISE_HISTOGRAM_REPORT);
		len = sizeof(struct CMD_NOISE_HISTOGRAM_REPORT);
		break;
#ifdef CFG_SUPPORT_ADMINCTRL
	case CMD_ADMINCTRL_CONFIG_TYPE:
		*pu4QueryInfoLen = sizeof(struct CMD_ADMIN_CTRL_CONFIG);
		len = sizeof(struct CMD_ADMIN_CTRL_CONFIG);
		break;
#endif
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
				   len, (uint8_t *)cmd,
				   pvQueryBuffer, u4QueryBufferLen);
}
#endif


#if CFG_SUPPORT_MSP
uint32_t
wlanoidQueryWlanInfo(IN struct ADAPTER *prAdapter,
		     IN void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
		     OUT uint32_t *pu4QueryInfoLen) {

	return wlanQueryWlanInfo(prAdapter,
			pvQueryBuffer,
			u4QueryBufferLen,
			pu4QueryInfoLen,
			g_fgIsOid);
}

uint32_t
wlanQueryWlanInfo(IN struct ADAPTER *prAdapter,
			IN void *pvQueryBuffer,
			IN uint32_t u4QueryBufferLen,
			OUT uint32_t *pu4QueryInfoLen,
			IN uint8_t fgIsOid) {
	struct PARAM_HW_WLAN_INFO *prHwWlanInfo;

	DEBUGFUNC("wlanoidQueryWlanInfo");
	DBGLOG(REQ, LOUD, "\n");

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	*pu4QueryInfoLen = sizeof(struct PARAM_HW_WLAN_INFO);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in query receive error! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		*pu4QueryInfoLen = sizeof(uint32_t);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (u4QueryBufferLen < sizeof(struct PARAM_HW_WLAN_INFO)) {
		DBGLOG(REQ, WARN, "Too short length %u\n",
		       u4QueryBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	prHwWlanInfo = (struct PARAM_HW_WLAN_INFO *)pvQueryBuffer;
	DBGLOG(RSN, INFO,
	       "MT6632 : wlanoidQueryWlanInfo index = %d\n",
	       prHwWlanInfo->u4Index);

	/*  *pu4QueryInfoLen = 8 + prRxStatistics->u4TotalNum; */

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_WLAN_INFO,
				   FALSE,
				   TRUE,
				   fgIsOid,
				   nicCmdEventQueryWlanInfo,
				   nicOidCmdTimeoutCommon,
				   sizeof(struct PARAM_HW_WLAN_INFO),
				   (uint8_t *)prHwWlanInfo,
				   pvQueryBuffer, u4QueryBufferLen);

}				/* wlanoidQueryWlanInfo */


uint32_t
wlanoidQueryMibInfo(IN struct ADAPTER *prAdapter,
		    IN void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
		    OUT uint32_t *pu4QueryInfoLen) {
	struct PARAM_HW_MIB_INFO *prHwMibInfo;

	DEBUGFUNC("wlanoidQueryMibInfo");
	DBGLOG(REQ, LOUD, "\n");

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	*pu4QueryInfoLen = sizeof(struct PARAM_HW_MIB_INFO);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in query receive error! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		*pu4QueryInfoLen = sizeof(uint32_t);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (u4QueryBufferLen < sizeof(struct
					     PARAM_HW_MIB_INFO)) {
		DBGLOG(REQ, WARN, "Too short length %u\n",
		       u4QueryBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	prHwMibInfo = (struct PARAM_HW_MIB_INFO *)pvQueryBuffer;
	DBGLOG(RSN, INFO,
	       "MT6632 : wlanoidQueryMibInfo index = %d\n",
	       prHwMibInfo->u4Index);

	/* *pu4QueryInfoLen = 8 + prRxStatistics->u4TotalNum; */

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_MIB_INFO,
				   FALSE,
				   TRUE,
				   g_fgIsOid,
				   nicCmdEventQueryMibInfo,
				   nicOidCmdTimeoutCommon,
				   sizeof(struct PARAM_HW_MIB_INFO),
				   (uint8_t *)prHwMibInfo,
				   pvQueryBuffer, u4QueryBufferLen);

}				/* wlanoidQueryMibInfo */
#endif


/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to set FW log to Host.
 *
 * \param[in] prAdapter      Pointer to the Adapter structure.
 * \param[in] pvSetBuffer    Pointer to the buffer that holds the data to be
 *			     set.
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
uint32_t
wlanoidSetFwLog2Host(
	IN struct ADAPTER *prAdapter,
	IN void *pvSetBuffer,
	IN uint32_t u4SetBufferLen,
	OUT uint32_t *pu4SetInfoLen) {
	struct CMD_FW_LOG_2_HOST_CTRL *prFwLog2HostCtrl;

	DEBUGFUNC("wlanoidSetFwLog2Host");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(struct CMD_FW_LOG_2_HOST_CTRL);

	if (u4SetBufferLen)
		ASSERT(pvSetBuffer);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in set FW log to Host! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (u4SetBufferLen < sizeof(struct
					   CMD_FW_LOG_2_HOST_CTRL)) {
		DBGLOG(REQ, WARN, "Too short length %d\n", u4SetBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	prFwLog2HostCtrl = (struct CMD_FW_LOG_2_HOST_CTRL *)
			   pvSetBuffer;

	DBGLOG(REQ, INFO, "McuDest %d, LogType %d\n",
	       prFwLog2HostCtrl->ucMcuDest,
	       prFwLog2HostCtrl->ucFwLog2HostCtrl);

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_FW_LOG_2_HOST,
				   TRUE,
				   FALSE,
				   g_fgIsOid,
				   nicCmdEventSetCommon,
				   nicOidCmdTimeoutCommon,
				   sizeof(struct CMD_FW_LOG_2_HOST_CTRL),
				   (uint8_t *)prFwLog2HostCtrl,
				   pvSetBuffer, u4SetBufferLen);
}

uint32_t
wlanoidNotifyFwSuspend(
	IN struct ADAPTER *prAdapter,
	IN void *pvSetBuffer,
	IN uint32_t u4SetBufferLen,
	OUT uint32_t *pu4SetInfoLen) {
	struct CMD_SUSPEND_MODE_SETTING *prSuspendCmd;

	if (!prAdapter || !pvSetBuffer)
		return WLAN_STATUS_INVALID_DATA;

	prSuspendCmd = (struct CMD_SUSPEND_MODE_SETTING *)
		       pvSetBuffer;

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_SET_SUSPEND_MODE,
				   TRUE,
				   FALSE,
				   g_fgIsOid,
				   nicCmdEventSetCommon,
				   nicOidCmdTimeoutCommon,
				   sizeof(struct CMD_SUSPEND_MODE_SETTING),
				   (uint8_t *)prSuspendCmd,
				   NULL,
				   0);
}

uint32_t
wlanoidQueryCnm(
	IN struct ADAPTER *prAdapter,
	IN void *pvQueryBuffer,
	IN uint32_t u4QueryBufferLen,
	OUT uint32_t *pu4QueryInfoLen) {
	struct PARAM_GET_CNM_T *prCnmInfo = NULL;

	DEBUGFUNC("wlanoidQueryLinkSpeed");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (prAdapter->fgIsEnableLpdvt)
		return WLAN_STATUS_NOT_SUPPORTED;

	*pu4QueryInfoLen = sizeof(struct PARAM_GET_CNM_T);

	if (u4QueryBufferLen < sizeof(struct PARAM_GET_CNM_T))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	prCnmInfo = (struct PARAM_GET_CNM_T *)pvQueryBuffer;

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_GET_CNM,
				   FALSE,
				   TRUE,
				   g_fgIsOid,
				   nicCmdEventQueryCnmInfo,
				   nicOidCmdTimeoutCommon,
				   sizeof(struct PARAM_GET_CNM_T),
				   (uint8_t *)prCnmInfo,
				   pvQueryBuffer, u4QueryBufferLen);
}

uint32_t
wlanoidPacketKeepAlive(IN struct ADAPTER *prAdapter,
		       IN void *pvSetBuffer,
		       IN uint32_t u4SetBufferLen,
		       OUT uint32_t *pu4SetInfoLen) {
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	struct PARAM_PACKET_KEEPALIVE_T *prPacket;

	DEBUGFUNC("wlanoidPacketKeepAlive");
	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	if (u4SetBufferLen)
		ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(struct PARAM_PACKET_KEEPALIVE_T);

	/* Check for query buffer length */
	if (u4SetBufferLen < *pu4SetInfoLen) {
		DBGLOG(OID, WARN, "Too short length %u\n", u4SetBufferLen);
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}

	prPacket = (struct PARAM_PACKET_KEEPALIVE_T *)
		   kalMemAlloc(sizeof(struct PARAM_PACKET_KEEPALIVE_T),
			       VIR_MEM_TYPE);
	if (!prPacket) {
		DBGLOG(OID, ERROR,
		       "Can not alloc memory for struct PARAM_PACKET_KEEPALIVE_T\n");
		return -ENOMEM;
	}
	kalMemCopy(prPacket, pvSetBuffer,
		   sizeof(struct PARAM_PACKET_KEEPALIVE_T));

	DBGLOG(OID, INFO, "enable=%d, index=%d\r\n",
	       prPacket->enable, prPacket->index);

	rStatus = wlanSendSetQueryCmd(prAdapter,
				      CMD_ID_WFC_KEEP_ALIVE,
				      TRUE,
				      FALSE,
				      g_fgIsOid,
				      nicCmdEventSetCommon,
				      nicOidCmdTimeoutCommon,
				      sizeof(struct PARAM_PACKET_KEEPALIVE_T),
				      (uint8_t *)prPacket, NULL, 0);
	kalMemFree(prPacket, VIR_MEM_TYPE,
		   sizeof(struct PARAM_PACKET_KEEPALIVE_T));
	return rStatus;
}

#if CFG_SUPPORT_DBDC
uint32_t
wlanoidSetDbdcEnable(
	IN struct ADAPTER *prAdapter,
	IN void *pvSetBuffer,
	IN uint32_t u4SetBufferLen,
	OUT uint32_t *pu4SetInfoLen) {
	uint8_t ucDBDCEnable;

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
uint32_t
wlanoidQuerySetTxTargetPower(IN struct ADAPTER *prAdapter,
			     IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			     OUT uint32_t *pu4SetInfoLen) {
	struct PARAM_CUSTOM_SET_TX_TARGET_POWER
		*prSetTxTargetPowerInfo;
	struct CMD_SET_TX_TARGET_POWER rCmdSetTxTargetPower;
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidQuerySetTxTargetPower");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(struct
				PARAM_CUSTOM_SET_TX_TARGET_POWER *);

	if (u4SetBufferLen < sizeof(struct
				    PARAM_CUSTOM_SET_TX_TARGET_POWER *))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);

	prSetTxTargetPowerInfo =
		(struct PARAM_CUSTOM_SET_TX_TARGET_POWER *) pvSetBuffer;

	kalMemSet(&rCmdSetTxTargetPower, 0,
		  sizeof(struct CMD_SET_TX_TARGET_POWER));

	rCmdSetTxTargetPower.ucTxTargetPwr =
		prSetTxTargetPowerInfo->ucTxTargetPwr;

	DBGLOG(INIT, INFO,
	       "MT6632 : wlanoidQuerySetTxTargetPower =%x dbm\n",
	       rCmdSetTxTargetPower.ucTxTargetPwr);

	rWlanStatus = wlanSendSetQueryCmd(prAdapter,
			  CMD_ID_SET_TX_PWR,
			  TRUE,  /* fgSetQuery Bit:  True->write  False->read */
			  FALSE, /* fgNeedResp */
			  g_fgIsOid,  /* fgIsOid*/
			  nicCmdEventSetCommon, /* REF: wlanoidSetDbdcEnable */
			  nicOidCmdTimeoutCommon,
			  sizeof(struct CMD_ACCESS_EFUSE),
			  (uint8_t *) (&rCmdSetTxTargetPower), pvSetBuffer,
			  u4SetBufferLen);

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
uint32_t
wlanoidQuerySetRddReport(IN struct ADAPTER *prAdapter,
			 IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			 OUT uint32_t *pu4SetInfoLen) {
	struct PARAM_CUSTOM_SET_RDD_REPORT *prSetRddReport;
	struct CMD_RDD_ON_OFF_CTRL *prCmdRddOnOffCtrl;
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidQuerySetRddReport");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(struct PARAM_CUSTOM_SET_RDD_REPORT
				*);

	ASSERT(pvSetBuffer);

	prSetRddReport = (struct PARAM_CUSTOM_SET_RDD_REPORT *)
			 pvSetBuffer;

	prCmdRddOnOffCtrl = (struct CMD_RDD_ON_OFF_CTRL *)
			    cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
					sizeof(*prCmdRddOnOffCtrl));

	ASSERT(prCmdRddOnOffCtrl);
	if (prCmdRddOnOffCtrl == NULL) {
		DBGLOG(INIT, ERROR, "prCmdRddOnOffCtrl is NULL");
		return WLAN_STATUS_FAILURE;
	}

	prCmdRddOnOffCtrl->ucDfsCtrl = RDD_RADAR_EMULATE;

	prCmdRddOnOffCtrl->ucRddIdx = prSetRddReport->ucDbdcIdx;

	if (prCmdRddOnOffCtrl->ucRddIdx)
		prCmdRddOnOffCtrl->ucRddRxSel = RDD_IN_SEL_1;
	else
		prCmdRddOnOffCtrl->ucRddRxSel = RDD_IN_SEL_0;

	DBGLOG(INIT, INFO,
	       "MT6632 : wlanoidQuerySetRddReport -  DFS ctrl: %.d, RDD index: %d\n",
	       prCmdRddOnOffCtrl->ucDfsCtrl, prCmdRddOnOffCtrl->ucRddIdx);

	rWlanStatus = wlanSendSetQueryCmd(prAdapter,
			CMD_ID_RDD_ON_OFF_CTRL,
			TRUE,  /* fgSetQuery Bit: True->write False->read */
			FALSE, /* fgNeedResp */
			g_fgIsOid,  /* fgIsOid*/
			nicCmdEventSetCommon, /* REF: wlanoidSetDbdcEnable */
			nicOidCmdTimeoutCommon,
			sizeof(*prCmdRddOnOffCtrl),
			(uint8_t *) (prCmdRddOnOffCtrl), pvSetBuffer,
			u4SetBufferLen);

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
uint32_t
wlanoidQuerySetRadarDetectMode(IN struct ADAPTER *prAdapter,
			       IN void *pvSetBuffer,
			       IN uint32_t u4SetBufferLen,
			       OUT uint32_t *pu4SetInfoLen) {
	struct PARAM_CUSTOM_SET_RADAR_DETECT_MODE
		*prSetRadarDetectMode;
	struct CMD_RDD_ON_OFF_CTRL *prCmdRddOnOffCtrl;
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanoidQuerySetRadarDetectMode");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen =
		sizeof(struct PARAM_CUSTOM_SET_RADAR_DETECT_MODE *);

	ASSERT(pvSetBuffer);

	prSetRadarDetectMode =
		(struct PARAM_CUSTOM_SET_RADAR_DETECT_MODE *) pvSetBuffer;

	prCmdRddOnOffCtrl = (struct CMD_RDD_ON_OFF_CTRL *)cnmMemAlloc(
						prAdapter, RAM_TYPE_MSG,
						sizeof(*prCmdRddOnOffCtrl));

	ASSERT(prCmdRddOnOffCtrl);
	if (prCmdRddOnOffCtrl == NULL) {
		DBGLOG(INIT, ERROR, "prCmdRddOnOffCtrl is NULL");
		return WLAN_STATUS_FAILURE;
	}

	prCmdRddOnOffCtrl->ucDfsCtrl = RDD_DET_MODE;

	prCmdRddOnOffCtrl->ucSetVal =
		prSetRadarDetectMode->ucRadarDetectMode;

	DBGLOG(INIT, INFO,
	       "MT6632 : wlanoidQuerySetRadarDetectMode -  DFS ctrl: %.d, Radar Detect Mode: %d\n",
	       prCmdRddOnOffCtrl->ucDfsCtrl, prCmdRddOnOffCtrl->ucSetVal);

	rWlanStatus = wlanSendSetQueryCmd(prAdapter,
			  CMD_ID_RDD_ON_OFF_CTRL,
			  TRUE,   /* fgSetQuery Bit: True->write False->read */
			  FALSE,   /* fgNeedResp */
			  g_fgIsOid,   /* fgIsOid*/
			  nicCmdEventSetCommon, /* REF: wlanoidSetDbdcEnable */
			  nicOidCmdTimeoutCommon,
			  sizeof(*prCmdRddOnOffCtrl),
			  (uint8_t *) (prCmdRddOnOffCtrl),
			  pvSetBuffer,
			  u4SetBufferLen);

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
uint32_t
wlanoidLinkDown(IN struct ADAPTER *prAdapter,
		IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		OUT uint32_t *pu4SetInfoLen) {
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

	prAdapter->prGlueInfo->u4LinkDownPendFlag = TRUE;

	return WLAN_STATUS_PENDING;
} /* wlanoidSetDisassociate */

uint32_t
wlanoidGetTxPwrTbl(IN struct ADAPTER *prAdapter,
		   IN void *pvQueryBuffer,
		   IN uint32_t u4QueryBufferLen,
		   OUT uint32_t *pu4QueryInfoLen)
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
		*pu4QueryInfoLen = sizeof(uint32_t);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (u4QueryBufferLen < sizeof(struct PARAM_CMD_GET_TXPWR_TBL)) {
		DBGLOG(REQ, WARN, "Too short length %u\n", u4QueryBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	prPwrTbl = (struct PARAM_CMD_GET_TXPWR_TBL *)pvQueryBuffer;

	CmdPwrTbl.ucCmdVer = 0x01;
	CmdPwrTbl.u2CmdLen = sizeof(struct CMD_GET_TXPWR_TBL);
	CmdPwrTbl.ucDbdcIdx = prPwrTbl->ucDbdcIdx;

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_GET_TXPWR_TBL,
				   FALSE,
				   TRUE,
				   g_fgIsOid,
				   nicCmdEventGetTxPwrTbl,
				   nicOidCmdTimeoutCommon,
				   sizeof(struct CMD_GET_TXPWR_TBL),
				   (uint8_t *)&CmdPwrTbl,
				   pvQueryBuffer,
				   u4QueryBufferLen);

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

uint32_t
wlanoidSetNchoHeader(struct CMD_HEADER *prCmdHeader,
		     struct CMD_FORMAT_V1 *pr_cmd_v1,
		     char *pStr, uint32_t u4Len) {
	prCmdHeader->cmdVersion = CMD_VER_1_EXT;
	prCmdHeader->cmdType = CMD_TYPE_QUERY;
	prCmdHeader->itemNum = 1;
	prCmdHeader->cmdBufferLen = sizeof(struct CMD_FORMAT_V1);
	kalMemSet(prCmdHeader->buffer, 0, MAX_CMD_BUFFER_LENGTH);

	if (!prCmdHeader || !pStr || u4Len == 0)
		return WLAN_STATUS_FAILURE;

	pr_cmd_v1->itemStringLength = u4Len;
	kalMemCopy(pr_cmd_v1->itemString, pStr, u4Len);

	return WLAN_STATUS_SUCCESS;
}

uint32_t
wlanoidSetNchoRoamTrigger(IN struct ADAPTER *prAdapter,
			  IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			  OUT uint32_t *pu4SetInfoLen) {
	int32_t *pi4Param = NULL;
	char acCmd[NCHO_CMD_MAX_LENGTH] = {0};
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	DEBUGFUNC("wlanoidSetNchoRoamTrigger");
	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(int32_t);

	if (u4SetBufferLen < sizeof(int32_t))
		return WLAN_STATUS_INVALID_LENGTH;

	pi4Param = (int32_t *) pvSetBuffer;
	*pi4Param = dBm_TO_RCPI(*pi4Param);		/* DB to RCPI */
	if (*pi4Param < RCPI_LOW_BOUND
	    || *pi4Param > RCPI_HIGH_BOUND) {
		DBGLOG(INIT, ERROR, "NCHO roam trigger invalid %d\n",
		       *pi4Param);
		return WLAN_STATUS_INVALID_DATA;
	}

	kalSprintf(acCmd, "%s %d", FW_CFG_KEY_NCHO_ROAM_RCPI,
		   *pi4Param);
	rStatus =  wlanFwCfgParse(prAdapter, acCmd);
	if (rStatus == WLAN_STATUS_SUCCESS) {
		prAdapter->rNchoInfo.i4RoamTrigger = RCPI_TO_dBm(*pi4Param);
		DBGLOG(INIT, TRACE, "NCHO roam trigger is %d\n",
		       prAdapter->rNchoInfo.i4RoamTrigger);
	}

	return rStatus;
}

uint32_t
wlanoidQueryNchoRoamTrigger(IN struct ADAPTER *prAdapter,
			    OUT void *pvQueryBuffer,
			    IN uint32_t u4QueryBufferLen,
			    OUT uint32_t *pu4QueryInfoLen) {
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct CMD_HEADER cmdV1Header;
	struct CMD_HEADER *prCmdV1Header = (struct CMD_HEADER *)
					   pvQueryBuffer;
	struct CMD_FORMAT_V1 *prCmdV1 = NULL;

	DEBUGFUNC("wlanoidQueryNchoRoamTrigger");
	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(struct CMD_HEADER);

	if (u4QueryBufferLen < sizeof(struct CMD_HEADER))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (prAdapter->rNchoInfo.fgECHOEnabled == FALSE)
		return WLAN_STATUS_INVALID_DATA;

	prCmdV1 = (struct CMD_FORMAT_V1 *) prCmdV1Header->buffer;
	rStatus = wlanoidSetNchoHeader(prCmdV1Header,
				       prCmdV1,
				       FW_CFG_KEY_NCHO_ROAM_RCPI,
				       kalStrLen(FW_CFG_KEY_NCHO_ROAM_RCPI));
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "NCHO no enough memory\n");
		return rStatus;
	}
	kalMemCopy(&cmdV1Header, prCmdV1Header,
		   sizeof(struct CMD_HEADER));
	rStatus = wlanSendSetQueryCmd(
			  prAdapter,
			  CMD_ID_GET_SET_CUSTOMER_CFG,
			  FALSE,
			  TRUE,
			  g_fgIsOid,
			  nicCmdEventQueryCfgRead,
			  nicOidCmdTimeoutCommon,
			  sizeof(struct CMD_HEADER),
			  (uint8_t *)&cmdV1Header,
			  pvQueryBuffer,
			  u4QueryBufferLen);
	return rStatus;
}

uint32_t
wlanoidSetNchoRoamDelta(IN struct ADAPTER *prAdapter,
			IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			OUT uint32_t *pu4SetInfoLen) {
	int32_t *pi4Param = NULL;
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	DEBUGFUNC("wlanoidSetNchoRoamDelta");
	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(int32_t);

	if (u4SetBufferLen < sizeof(int32_t))
		return WLAN_STATUS_INVALID_LENGTH;

	pi4Param = (int32_t *) pvSetBuffer;
	if (*pi4Param > 100) {
		DBGLOG(INIT, ERROR, "NCHO roam delta invalid %d\n",
		       *pi4Param);
		return WLAN_STATUS_INVALID_DATA;
	}

	prAdapter->rNchoInfo.i4RoamDelta = *pi4Param;
	DBGLOG(INIT, TRACE, "NCHO roam delta is %d\n", *pi4Param);
	rStatus = WLAN_STATUS_SUCCESS;

	return rStatus;
}

uint32_t
wlanoidQueryNchoRoamDelta(IN struct ADAPTER *prAdapter,
			  OUT void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
			  OUT uint32_t *pu4QueryInfoLen) {
	int32_t *pParam = NULL;

	DEBUGFUNC("wlanoidQueryNchoRoamDelta");
	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (u4QueryBufferLen < sizeof(int32_t *))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (prAdapter->rNchoInfo.fgECHOEnabled == FALSE)
		return WLAN_STATUS_INVALID_DATA;

	pParam = (int32_t *) pvQueryBuffer;
	*pParam = prAdapter->rNchoInfo.i4RoamDelta;
	DBGLOG(INIT, TRACE, "NCHO roam delta is %d\n", *pParam);

	return WLAN_STATUS_SUCCESS;
}

uint32_t
wlanoidSetNchoRoamScnPeriod(IN struct ADAPTER *prAdapter,
			    IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			    OUT uint32_t *pu4SetInfoLen) {
	uint32_t *pParam = NULL;
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	DEBUGFUNC("wlanoidSetNchoRoamScnPeriod");
	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(uint32_t);

	if (u4SetBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_INVALID_LENGTH;

	pParam = (uint32_t *) pvSetBuffer;

	prAdapter->rNchoInfo.u4RoamScanPeriod = *pParam;
	DBGLOG(INIT, TRACE, "NCHO roam scan period is %d\n",
	       *pParam);
	rStatus = WLAN_STATUS_SUCCESS;

	return rStatus;
}

uint32_t
wlanoidQueryNchoRoamScnPeriod(IN struct ADAPTER *prAdapter,
			      OUT void *pvQueryBuffer,
			      IN uint32_t u4QueryBufferLen,
			      OUT uint32_t *pu4QueryInfoLen) {
	uint32_t *pParam = NULL;

	DEBUGFUNC("wlanoidQueryNchoRoamScnPeriod");
	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (u4QueryBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (prAdapter->rNchoInfo.fgECHOEnabled == FALSE)
		return WLAN_STATUS_INVALID_DATA;

	pParam = (uint32_t *) pvQueryBuffer;
	*pParam = prAdapter->rNchoInfo.u4RoamScanPeriod;
	DBGLOG(INIT, TRACE, "NCHO roam scan period is %d\n",
	       *pParam);

	return WLAN_STATUS_SUCCESS;
}

uint32_t
wlanoidSetNchoRoamScnChnl(IN struct ADAPTER *prAdapter,
			  IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			  OUT uint32_t *pu4SetInfoLen) {
	struct _CFG_NCHO_SCAN_CHNL_T *prRoamScnChnl = NULL;
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	DEBUGFUNC("wlanoidSetNchoRoamScnChnl");
	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(struct _CFG_NCHO_SCAN_CHNL_T);

	if (u4SetBufferLen < sizeof(struct _CFG_NCHO_SCAN_CHNL_T))
		return WLAN_STATUS_INVALID_LENGTH;

	prRoamScnChnl = (struct _CFG_NCHO_SCAN_CHNL_T *)
			pvSetBuffer;

	kalMemCopy(&prAdapter->rNchoInfo.rRoamScnChnl,
		   prRoamScnChnl, *pu4SetInfoLen);
	prAdapter->rNchoInfo.u4RoamScanControl = TRUE;
	DBGLOG(INIT, TRACE,
	       "NCHO set roam scan channel num is %d\n",
	       prRoamScnChnl->ucChannelListNum);
	rStatus = WLAN_STATUS_SUCCESS;


	return rStatus;
}

uint32_t
wlanoidQueryNchoRoamScnChnl(IN struct ADAPTER *prAdapter,
			    OUT void *pvQueryBuffer,
			    IN uint32_t u4QueryBufferLen,
			    OUT uint32_t *pu4QueryInfoLen) {
	struct _CFG_NCHO_SCAN_CHNL_T *prRoamScnChnl = NULL;

	DEBUGFUNC("wlanoidQueryNchoRoamScnChnl");
	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (u4QueryBufferLen < sizeof(struct _CFG_NCHO_SCAN_CHNL_T))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (prAdapter->rNchoInfo.fgECHOEnabled == FALSE)
		return WLAN_STATUS_INVALID_DATA;

	prRoamScnChnl = (struct _CFG_NCHO_SCAN_CHNL_T *)
			pvQueryBuffer;
	kalMemCopy(prRoamScnChnl,
		   &prAdapter->rNchoInfo.rRoamScnChnl, u4QueryBufferLen);
	DBGLOG(INIT, TRACE, "NCHO roam scan channel num is %d\n",
	       prRoamScnChnl->ucChannelListNum);

	return WLAN_STATUS_SUCCESS;
}

uint32_t
wlanoidSetNchoRoamScnCtrl(IN struct ADAPTER *prAdapter,
			  IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			  OUT uint32_t *pu4SetInfoLen) {
	uint32_t *pParam = NULL;
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	DEBUGFUNC("wlanoidSetNchoRoamScnChnl");
	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(uint32_t);

	if (u4SetBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_INVALID_LENGTH;

	pParam = (uint32_t *) pvSetBuffer;
	if (*pParam != TRUE && *pParam != FALSE) {
		DBGLOG(INIT, ERROR, "NCHO roam scan control invalid %d\n",
		       *pParam);
		return WLAN_STATUS_INVALID_DATA;
	}

	prAdapter->rNchoInfo.u4RoamScanControl = *pParam;
	DBGLOG(INIT, TRACE, "NCHO roam scan control is %d\n",
	       *pParam);
	rStatus = WLAN_STATUS_SUCCESS;

	return rStatus;
}

uint32_t
wlanoidQueryNchoRoamScnCtrl(IN struct ADAPTER *prAdapter,
			    OUT void *pvQueryBuffer,
			    IN uint32_t u4QueryBufferLen,
			    OUT uint32_t *pu4QueryInfoLen) {
	uint32_t *pParam = NULL;

	DEBUGFUNC("wlanoidQueryNchoRoamScnCtrl");
	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (u4QueryBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (prAdapter->rNchoInfo.fgECHOEnabled == FALSE)
		return WLAN_STATUS_INVALID_DATA;

	pParam = (uint32_t *) pvQueryBuffer;
	*pParam = prAdapter->rNchoInfo.u4RoamScanControl;
	DBGLOG(INIT, TRACE, "NCHO roam scan control is %d\n",
	       *pParam);

	return WLAN_STATUS_SUCCESS;
}

uint32_t
wlanoidSetNchoScnChnlTime(IN struct ADAPTER *prAdapter,
			  IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			  OUT uint32_t *pu4SetInfoLen) {
	uint32_t *pParam = NULL;
	char acCmd[NCHO_CMD_MAX_LENGTH] = {0};
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	DEBUGFUNC("wlanoidSetNchoScnChnlTime");
	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(uint32_t);

	if (u4SetBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_INVALID_LENGTH;

	pParam = (uint32_t *) pvSetBuffer;
	if (*pParam < 10 && *pParam > 1000) {
		DBGLOG(INIT, ERROR, "NCHO scan channel time invalid %d\n",
		       *pParam);
		return WLAN_STATUS_INVALID_DATA;
	}

	kalSprintf(acCmd, "%s %d", FW_CFG_KEY_NCHO_SCN_CHANNEL_TIME,
		   *pParam);
	rStatus =  wlanFwCfgParse(prAdapter, acCmd);
	if (rStatus == WLAN_STATUS_SUCCESS) {
		prAdapter->rNchoInfo.u4ScanChannelTime = *pParam;
		DBGLOG(INIT, TRACE, "NCHO scan channel time is %d\n",
		       *pParam);
	}

	return rStatus;
}

uint32_t
wlanoidQueryNchoScnChnlTime(IN struct ADAPTER *prAdapter,
			    OUT void *pvQueryBuffer,
			    IN uint32_t u4QueryBufferLen,
			    OUT uint32_t *pu4QueryInfoLen) {
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct CMD_HEADER cmdV1Header;
	struct CMD_HEADER *prCmdV1Header = (struct CMD_HEADER *)
					   pvQueryBuffer;
	struct CMD_FORMAT_V1 *prCmdV1 = NULL;

	DEBUGFUNC("wlanoidQueryNchoScnChnlTime");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(struct CMD_HEADER);

	if (u4QueryBufferLen < sizeof(struct CMD_HEADER))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (prAdapter->rNchoInfo.fgECHOEnabled == FALSE)
		return WLAN_STATUS_INVALID_DATA;

	prCmdV1 = (struct CMD_FORMAT_V1 *) prCmdV1Header->buffer;
	rStatus = wlanoidSetNchoHeader(prCmdV1Header, prCmdV1,
			       FW_CFG_KEY_NCHO_SCN_CHANNEL_TIME,
			       kalStrLen(FW_CFG_KEY_NCHO_SCN_CHANNEL_TIME));
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "NCHO no enough memory\n");
		return rStatus;
	}
	kalMemCopy(&cmdV1Header, prCmdV1Header,
		   sizeof(struct CMD_HEADER));
	rStatus = wlanSendSetQueryCmd(
			  prAdapter,
			  CMD_ID_GET_SET_CUSTOMER_CFG,
			  FALSE,
			  TRUE,
			  g_fgIsOid,
			  nicCmdEventQueryCfgRead,
			  nicOidCmdTimeoutCommon,
			  sizeof(struct CMD_HEADER),
			  (uint8_t *)&cmdV1Header,
			  pvQueryBuffer,
			  u4QueryBufferLen);
	return rStatus;
}

uint32_t
wlanoidSetNchoScnHomeTime(IN struct ADAPTER *prAdapter,
			  IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			  OUT uint32_t *pu4SetInfoLen) {
	uint32_t *pParam = NULL;
	char acCmd[NCHO_CMD_MAX_LENGTH] = {0};
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	DEBUGFUNC("wlanoidSetNchoScnHomeTime");
	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(uint32_t);

	if (u4SetBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_INVALID_LENGTH;

	pParam = (uint32_t *) pvSetBuffer;
	if (*pParam < 10 && *pParam > 1000) {
		DBGLOG(INIT, ERROR, "NCHO scan home time invalid %d\n",
		       *pParam);
		return WLAN_STATUS_INVALID_DATA;
	}

	kalSprintf(acCmd, "%s %d", FW_CFG_KEY_NCHO_SCN_HOME_TIME,
		   *pParam);
	DBGLOG(REQ, TRACE, "NCHO cmd is %s\n", acCmd);
	rStatus =  wlanFwCfgParse(prAdapter, acCmd);
	if (rStatus == WLAN_STATUS_SUCCESS) {
		prAdapter->rNchoInfo.u4ScanHomeTime = *pParam;
		DBGLOG(INIT, TRACE, "NCHO scan home time is %d\n", *pParam);
	}

	return rStatus;
}

uint32_t
wlanoidQueryNchoScnHomeTime(IN struct ADAPTER *prAdapter,
			    OUT void *pvQueryBuffer,
			    IN uint32_t u4QueryBufferLen,
			    OUT uint32_t *pu4QueryInfoLen) {
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct CMD_HEADER cmdV1Header;
	struct CMD_HEADER *prCmdV1Header = (struct CMD_HEADER *)
					   pvQueryBuffer;
	struct CMD_FORMAT_V1 *prCmdV1 = NULL;

	DEBUGFUNC("wlanoidQueryNchoScnHomeTime");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(struct CMD_HEADER);

	if (u4QueryBufferLen < sizeof(struct CMD_HEADER))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (prAdapter->rNchoInfo.fgECHOEnabled == FALSE)
		return WLAN_STATUS_INVALID_DATA;

	prCmdV1 = (struct CMD_FORMAT_V1 *) prCmdV1Header->buffer;
	rStatus = wlanoidSetNchoHeader(prCmdV1Header, prCmdV1,
			       FW_CFG_KEY_NCHO_SCN_HOME_TIME,
			       kalStrLen(FW_CFG_KEY_NCHO_SCN_HOME_TIME));
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "NCHO no enough memory\n");
		return rStatus;
	}
	kalMemCopy(&cmdV1Header, prCmdV1Header,
		   sizeof(struct CMD_HEADER));
	rStatus = wlanSendSetQueryCmd(
			  prAdapter,
			  CMD_ID_GET_SET_CUSTOMER_CFG,
			  FALSE,
			  TRUE,
			  g_fgIsOid,
			  nicCmdEventQueryCfgRead,
			  nicOidCmdTimeoutCommon,
			  sizeof(struct CMD_HEADER),
			  (uint8_t *)&cmdV1Header,
			  pvQueryBuffer,
			  u4QueryBufferLen);
	return rStatus;
}

uint32_t
wlanoidSetNchoScnHomeAwayTime(IN struct ADAPTER *prAdapter,
			      IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			      OUT uint32_t *pu4SetInfoLen) {
	uint32_t *pParam = NULL;
	char acCmd[NCHO_CMD_MAX_LENGTH] = {0};
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	DEBUGFUNC("wlanoidSetNchoScnHomeAwayTime");
	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(uint32_t);

	if (u4SetBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_INVALID_LENGTH;

	pParam = (uint32_t *) pvSetBuffer;
	if (*pParam < 10 && *pParam > 1000) {
		DBGLOG(INIT, ERROR, "NCHO scan home away time invalid %d\n",
		       *pParam);
		return WLAN_STATUS_INVALID_DATA;
	}


	kalSprintf(acCmd, "%s %d",
		   FW_CFG_KEY_NCHO_SCN_HOME_AWAY_TIME, *pParam);
	DBGLOG(REQ, TRACE, "NCHO cmd is %s\n", acCmd);
	rStatus =  wlanFwCfgParse(prAdapter, acCmd);
	if (rStatus == WLAN_STATUS_SUCCESS) {
		prAdapter->rNchoInfo.u4ScanHomeawayTime = *pParam;
		DBGLOG(INIT, TRACE, "NCHO scan home away is %d\n", *pParam);
	}

	return rStatus;
}

uint32_t
wlanoidQueryNchoScnHomeAwayTime(IN struct ADAPTER *prAdapter,
				OUT void *pvQueryBuffer,
				IN uint32_t u4QueryBufferLen,
				OUT uint32_t *pu4QueryInfoLen) {
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct CMD_HEADER cmdV1Header;
	struct CMD_HEADER *prCmdV1Header = (struct CMD_HEADER *)
					   pvQueryBuffer;
	struct CMD_FORMAT_V1 *prCmdV1 = NULL;

	DEBUGFUNC("wlanoidQueryNchoScnHomeTime");
	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(struct CMD_HEADER);

	if (u4QueryBufferLen < sizeof(struct CMD_HEADER))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (prAdapter->rNchoInfo.fgECHOEnabled == FALSE)
		return WLAN_STATUS_INVALID_DATA;

	prCmdV1 = (struct CMD_FORMAT_V1 *) prCmdV1Header->buffer;
	rStatus = wlanoidSetNchoHeader(prCmdV1Header, prCmdV1,
			       FW_CFG_KEY_NCHO_SCN_HOME_AWAY_TIME,
			       kalStrLen(FW_CFG_KEY_NCHO_SCN_HOME_AWAY_TIME));
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "NCHO no enough memory\n");
		return rStatus;
	}
	kalMemCopy(&cmdV1Header, prCmdV1Header,
		   sizeof(struct CMD_HEADER));
	rStatus = wlanSendSetQueryCmd(
			  prAdapter,
			  CMD_ID_GET_SET_CUSTOMER_CFG,
			  FALSE,
			  TRUE,
			  g_fgIsOid,
			  nicCmdEventQueryCfgRead,
			  nicOidCmdTimeoutCommon,
			  sizeof(struct CMD_HEADER),
			  (uint8_t *)&cmdV1Header,
			  pvQueryBuffer,
			  u4QueryBufferLen);
	return rStatus;
}

uint32_t
wlanoidSetNchoScnNprobes(IN struct ADAPTER *prAdapter,
			 IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			 OUT uint32_t *pu4SetInfoLen) {
	uint32_t *pParam = NULL;
	char acCmd[NCHO_CMD_MAX_LENGTH] = {0};
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	DEBUGFUNC("wlanoidSetNchoScnNprobes");
	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(uint32_t);

	if (u4SetBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_INVALID_LENGTH;

	pParam = (uint32_t *) pvSetBuffer;
	if (*pParam > 16) {
		DBGLOG(INIT, ERROR, "NCHO scan Nprobes invalid %d\n",
		       *pParam);
		return WLAN_STATUS_INVALID_DATA;
	}


	kalSprintf(acCmd, "%s %d", FW_CFG_KEY_NCHO_SCN_NPROBES,
		   *pParam);
	rStatus =  wlanFwCfgParse(prAdapter, acCmd);
	if (rStatus == WLAN_STATUS_SUCCESS) {
		prAdapter->rNchoInfo.u4ScanNProbes = *pParam;
		DBGLOG(INIT, TRACE, "NCHO Nprobes is %d\n", *pParam);
	}
	return rStatus;
}

uint32_t
wlanoidQueryNchoScnNprobes(IN struct ADAPTER *prAdapter,
			   OUT void *pvQueryBuffer,
			   IN uint32_t u4QueryBufferLen,
			   OUT uint32_t *pu4QueryInfoLen) {
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct CMD_HEADER cmdV1Header;
	struct CMD_HEADER *prCmdV1Header = (struct CMD_HEADER *)
					   pvQueryBuffer;
	struct CMD_FORMAT_V1 *prCmdV1 = NULL;

	DEBUGFUNC("wlanoidQueryNchoScnNprobes");
	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(struct CMD_HEADER);

	if (u4QueryBufferLen < sizeof(struct CMD_HEADER))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (prAdapter->rNchoInfo.fgECHOEnabled == FALSE)
		return WLAN_STATUS_INVALID_DATA;

	prCmdV1 = (struct CMD_FORMAT_V1 *) prCmdV1Header->buffer;
	rStatus = wlanoidSetNchoHeader(prCmdV1Header,
				       prCmdV1,
				       FW_CFG_KEY_NCHO_SCN_NPROBES,
				       kalStrLen(FW_CFG_KEY_NCHO_SCN_NPROBES));
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "NCHO no enough memory\n");
		return rStatus;
	}
	kalMemCopy(&cmdV1Header, prCmdV1Header,
		   sizeof(struct CMD_HEADER));
	rStatus = wlanSendSetQueryCmd(
			  prAdapter,
			  CMD_ID_GET_SET_CUSTOMER_CFG,
			  FALSE,
			  TRUE,
			  g_fgIsOid,
			  nicCmdEventQueryCfgRead,
			  nicOidCmdTimeoutCommon,
			  sizeof(struct CMD_HEADER),
			  (uint8_t *)&cmdV1Header,
			  pvQueryBuffer,
			  u4QueryBufferLen);
	return rStatus;
}

uint32_t
wlanoidGetNchoReassocInfo(IN struct ADAPTER *prAdapter,
			  OUT void *pvQueryBuffer,
			  IN uint32_t u4QueryBufferLen,
			  OUT uint32_t *pu4QueryInfoLen) {
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct BSS_DESC *prBssDesc = NULL;
	struct PARAM_CONNECT *prParamConn;

	DEBUGFUNC("wlanoidGetNchoReassocInfo");
	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	ASSERT(pvQueryBuffer);

	prParamConn = (struct PARAM_CONNECT *)pvQueryBuffer;
	if (prAdapter->rNchoInfo.fgECHOEnabled == TRUE) {
		prBssDesc = scanSearchBssDescByBssid(prAdapter,
						     prParamConn->pucBssid);
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

uint32_t
wlanoidSendNchoActionFrameStart(IN struct ADAPTER *prAdapter,
				IN void *pvSetBuffer,
				IN uint32_t u4SetBufferLen,
				OUT uint32_t *pu4SetInfoLen) {
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct _NCHO_INFO_T *prNchoInfo = NULL;
	struct _NCHO_ACTION_FRAME_PARAMS_T *prParamActionFrame =
			NULL;

	DEBUGFUNC("wlanoidSendNchoActionFrameStart");
	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);


	prNchoInfo = &prAdapter->rNchoInfo;
	prParamActionFrame = (struct _NCHO_ACTION_FRAME_PARAMS_T *)
			     pvSetBuffer;
	prNchoInfo->fgIsSendingAF = TRUE;
	prNchoInfo->fgChGranted = FALSE;
	COPY_MAC_ADDR(prNchoInfo->rParamActionFrame.aucBssid,
		      prParamActionFrame->aucBssid);
	prNchoInfo->rParamActionFrame.i4channel =
		prParamActionFrame->i4channel;
	prNchoInfo->rParamActionFrame.i4DwellTime =
		prParamActionFrame->i4DwellTime;
	prNchoInfo->rParamActionFrame.i4len =
		prParamActionFrame->i4len;
	kalMemCopy(prNchoInfo->rParamActionFrame.aucData,
		   prParamActionFrame->aucData,
		   prParamActionFrame->i4len);
	DBGLOG(INIT, TRACE, "NCHO send ncho action frame start\n");
	rStatus = WLAN_STATUS_SUCCESS;

	return rStatus;
}

uint32_t
wlanoidSendNchoActionFrameEnd(IN struct ADAPTER *prAdapter,
			      IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			      OUT uint32_t *pu4SetInfoLen) {
	uint32_t rStatus = WLAN_STATUS_FAILURE;

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

uint32_t
wlanoidSetNchoWesMode(IN struct ADAPTER *prAdapter,
		      IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		      OUT uint32_t *pu4SetInfoLen) {
	uint32_t *pParam = NULL;
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	DEBUGFUNC("wlanoidSetNchoWesMode");
	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(uint32_t);

	if (u4SetBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_INVALID_LENGTH;

	pParam = (uint32_t *) pvSetBuffer;
	if (*pParam != TRUE && *pParam != FALSE) {
		DBGLOG(INIT, ERROR, "NCHO wes mode invalid %d\n", *pParam);
		return WLAN_STATUS_INVALID_DATA;
	}


	prAdapter->rNchoInfo.u4WesMode = *pParam;
	DBGLOG(INIT, TRACE, "NCHO WES mode is %d\n", *pParam);
	rStatus = WLAN_STATUS_SUCCESS;

	return rStatus;
}

uint32_t
wlanoidQueryNchoWesMode(IN struct ADAPTER *prAdapter,
			OUT void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
			OUT uint32_t *pu4QueryInfoLen) {
	uint32_t *pParam = NULL;

	DEBUGFUNC("wlanoidQueryNchoWesMode");
	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (u4QueryBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (prAdapter->rNchoInfo.fgECHOEnabled == FALSE)
		return WLAN_STATUS_INVALID_DATA;

	pParam = (uint32_t *) pvQueryBuffer;
	*pParam = prAdapter->rNchoInfo.u4WesMode;
	DBGLOG(INIT, TRACE, "NCHO Wes mode is %d\n", *pParam);

	return WLAN_STATUS_SUCCESS;
}

uint32_t
wlanoidSetNchoBand(IN struct ADAPTER *prAdapter,
		   IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		   OUT uint32_t *pu4SetInfoLen) {
	uint32_t *pParam = NULL;
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	DEBUGFUNC("wlanoidSetNchoBand");
	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(uint32_t);

	if (u4SetBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_INVALID_LENGTH;

	pParam = (uint32_t *) pvSetBuffer;

	switch (*pParam) {
	case NCHO_BAND_AUTO:
		prAdapter->aePreferBand[NETWORK_TYPE_AIS] = BAND_NULL;
		prAdapter->rNchoInfo.eBand = NCHO_BAND_AUTO;
		rStatus = WLAN_STATUS_SUCCESS;
		break;
	case NCHO_BAND_2G4:
		prAdapter->aePreferBand[NETWORK_TYPE_AIS] = BAND_2G4;
		prAdapter->rNchoInfo.eBand = NCHO_BAND_2G4;
		rStatus = WLAN_STATUS_SUCCESS;
		break;
	case NCHO_BAND_5G:
		prAdapter->aePreferBand[NETWORK_TYPE_AIS] = BAND_5G;
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

uint32_t
wlanoidQueryNchoBand(IN struct ADAPTER *prAdapter,
		     OUT void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
		     OUT uint32_t *pu4QueryInfoLen) {
	uint32_t *pParam = NULL;

	DEBUGFUNC("wlanoidQueryNchoBand");
	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (u4QueryBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (prAdapter->rNchoInfo.fgECHOEnabled == FALSE)
		return WLAN_STATUS_INVALID_DATA;

	pParam = (uint32_t *) pvQueryBuffer;
	*pParam = prAdapter->rNchoInfo.eBand;
	DBGLOG(INIT, TRACE, "NCHO band is %d\n", *pParam);

	return WLAN_STATUS_SUCCESS;
}

uint32_t
wlanoidSetNchoDfsScnMode(IN struct ADAPTER *prAdapter,
			 IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			 OUT uint32_t *pu4SetInfoLen) {
	uint32_t *pParam = NULL;
	char acCmd[NCHO_CMD_MAX_LENGTH] = {0};
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	DEBUGFUNC("wlanoidSetNchoDfsScnMode");
	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(uint32_t);

	if (u4SetBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_INVALID_LENGTH;

	pParam = (uint32_t *) pvSetBuffer;
	if (*pParam >= NCHO_DFS_SCN_NUM) {
		DBGLOG(INIT, ERROR, "NCHO DFS scan mode invalid %d\n",
		       *pParam);
		return WLAN_STATUS_INVALID_DATA;
	}


	kalSprintf(acCmd, "%s %d", FW_CFG_KEY_NCHO_SCAN_DFS_MODE,
		   *pParam);
	rStatus =  wlanFwCfgParse(prAdapter, acCmd);
	if (rStatus == WLAN_STATUS_SUCCESS) {
		prAdapter->rNchoInfo.eDFSScnMode = *pParam;
		DBGLOG(INIT, TRACE, "NCHO DFS scan mode is %d\n", *pParam);
	}

	return rStatus;
}

uint32_t
wlanoidQueryNchoDfsScnMode(IN struct ADAPTER *prAdapter,
			   OUT void *pvQueryBuffer,
			   IN uint32_t u4QueryBufferLen,
			   OUT uint32_t *pu4QueryInfoLen) {
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct CMD_HEADER cmdV1Header;
	struct CMD_HEADER *prCmdV1Header = (struct CMD_HEADER *)
					   pvQueryBuffer;
	struct CMD_FORMAT_V1 *prCmdV1 = NULL;

	DEBUGFUNC("wlanoidQueryNchoDfsScnMode");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(struct CMD_HEADER);

	if (u4QueryBufferLen < sizeof(struct CMD_HEADER))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (prAdapter->rNchoInfo.fgECHOEnabled == FALSE)
		return WLAN_STATUS_INVALID_DATA;

	prCmdV1 = (struct CMD_FORMAT_V1 *) prCmdV1Header->buffer;
	rStatus = wlanoidSetNchoHeader(prCmdV1Header, prCmdV1,
			       FW_CFG_KEY_NCHO_SCAN_DFS_MODE,
			       kalStrLen(FW_CFG_KEY_NCHO_SCAN_DFS_MODE));
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "NCHO no enough memory\n");
		return rStatus;
	}
	kalMemCopy(&cmdV1Header, prCmdV1Header,
		   sizeof(struct CMD_HEADER));
	rStatus = wlanSendSetQueryCmd(
			  prAdapter,
			  CMD_ID_GET_SET_CUSTOMER_CFG,
			  FALSE,
			  TRUE,
			  g_fgIsOid,
			  nicCmdEventQueryCfgRead,
			  nicOidCmdTimeoutCommon,
			  sizeof(struct CMD_HEADER),
			  (uint8_t *)&cmdV1Header,
			  pvQueryBuffer,
			  u4QueryBufferLen);
	return rStatus;
}

uint32_t
wlanoidSetNchoEnable(IN struct ADAPTER *prAdapter,
		     IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		     OUT uint32_t *pu4SetInfoLen) {
	uint32_t *pParam = NULL;
	char acCmd[NCHO_CMD_MAX_LENGTH] = { 0 };
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	DEBUGFUNC("wlanoidSetNchoEnable");
	DBGLOG(OID, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(uint32_t);

	if (u4SetBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_INVALID_LENGTH;

	pParam = (uint32_t *) pvSetBuffer;
	if (*pParam != 0 && *pParam != 1) {
		DBGLOG(INIT, ERROR, "NCHO DFS scan mode invalid %d\n",
		       *pParam);
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

uint32_t
wlanoidQueryNchoEnable(IN struct ADAPTER *prAdapter,
		       OUT void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
		       OUT uint32_t *pu4QueryInfoLen) {
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct CMD_HEADER cmdV1Header;
	struct CMD_HEADER *prCmdV1Header = (struct CMD_HEADER *)
					   pvQueryBuffer;
	struct CMD_FORMAT_V1 *prCmdV1 = NULL;

	DEBUGFUNC("wlanoidQueryNchoRoamTrigger");

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(struct CMD_HEADER);

	if (u4QueryBufferLen < sizeof(struct CMD_HEADER))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	prCmdV1 = (struct CMD_FORMAT_V1 *) prCmdV1Header->buffer;
	rStatus = wlanoidSetNchoHeader(prCmdV1Header,
				       prCmdV1,
				       FW_CFG_KEY_NCHO_ENABLE,
				       kalStrLen(FW_CFG_KEY_NCHO_ENABLE));
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "NCHO no enough memory\n");
		return rStatus;
	}
	kalMemCopy(&cmdV1Header, prCmdV1Header,
		   sizeof(struct CMD_HEADER));
	rStatus = wlanSendSetQueryCmd(
			  prAdapter,
			  CMD_ID_GET_SET_CUSTOMER_CFG,
			  FALSE,
			  TRUE,
			  g_fgIsOid,
			  nicCmdEventQueryCfgRead,
			  nicOidCmdTimeoutCommon,
			  sizeof(struct CMD_HEADER),
			  (uint8_t *)&cmdV1Header,
			  pvQueryBuffer,
			  u4QueryBufferLen);
	return rStatus;
}
#endif /* CFG_SUPPORT_NCHO */

uint32_t
wlanoidAbortScan(IN struct ADAPTER *prAdapter,
		 OUT void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
		 OUT uint32_t *pu4QueryInfoLen) {

	struct AIS_FSM_INFO *prAisFsmInfo = NULL;

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	if (prAisFsmInfo->eCurrentState == AIS_STATE_SCAN ||
	    prAisFsmInfo->eCurrentState == AIS_STATE_ONLINE_SCAN) {
		DBGLOG(OID, INFO,  "wlanoidAbortScan\n");
		prAisFsmInfo->fgIsScanOidAborted = TRUE;
		aisFsmStateAbort_SCAN(prAdapter);
	}
	return WLAN_STATUS_SUCCESS;
}

uint32_t
wlanoidDisableTdlsPs(IN struct ADAPTER *prAdapter,
		     IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		     OUT uint32_t *pu4SetInfoLen) {
	struct CMD_TDLS_PS_T rTdlsPs;

	if (!prAdapter || !pvSetBuffer)
		return WLAN_STATUS_INVALID_DATA;

	rTdlsPs.ucIsEnablePs = *(uint8_t *)pvSetBuffer - '0';
	DBGLOG(OID, INFO, "enable tdls ps %d\n",
	       rTdlsPs.ucIsEnablePs);
	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_TDLS_PS,
				   TRUE,
				   FALSE,
				   g_fgIsOid,
				   nicCmdEventSetCommon,
				   nicOidCmdTimeoutCommon,
				   sizeof(rTdlsPs),
				   (uint8_t *)&rTdlsPs,
				   NULL,
				   0);
}

uint32_t wlanoidSetSer(IN struct ADAPTER *prAdapter,
		       IN void *pvSetBuffer,
		       IN uint32_t u4SetBufferLen,
		       OUT uint32_t *pu4SetInfoLen) {
	uint32_t u4CmdId;

	if (u4SetBufferLen)
		ASSERT(pvSetBuffer);

	if (u4SetBufferLen != sizeof(uint32_t))
		return WLAN_STATUS_INVALID_LENGTH;

	u4CmdId = *((uint32_t *)pvSetBuffer);

	DBGLOG(OID, INFO, "Set SER CMD[%d]\n", u4CmdId);

	switch (u4CmdId) {
	case SER_USER_CMD_DISABLE:
		wlanoidSerExtCmd(prAdapter, SER_ACTION_SET,
				 SER_SET_DISABLE, 0);
		break;

	case SER_USER_CMD_ENABLE:
		wlanoidSerExtCmd(prAdapter, SER_ACTION_SET, SER_SET_ENABLE, 0);
		break;

	case SER_USER_CMD_QUERY:
		wlanoidSerExtCmd(prAdapter, SER_ACTION_QUERY, 0, 0);
		break;

	case SER_USER_CMD_ENABLE_MASK_TRACKING_ONLY:
		wlanoidSerExtCmd(prAdapter, SER_ACTION_SET_ENABLE_MASK,
				 SER_ENABLE_TRACKING, 0);
		break;

	case SER_USER_CMD_ENABLE_MASK_L1_RECOVER_ONLY:
		wlanoidSerExtCmd(prAdapter, SER_ACTION_SET_ENABLE_MASK,
				 SER_ENABLE_TRACKING | SER_ENABLE_L1_RECOVER,
				 0);
		break;

	case SER_USER_CMD_ENABLE_MASK_L2_RECOVER_ONLY:
		wlanoidSerExtCmd(prAdapter, SER_ACTION_SET_ENABLE_MASK,
				 SER_ENABLE_TRACKING | SER_ENABLE_L2_RECOVER,
				 0);
		break;

	case SER_USER_CMD_ENABLE_MASK_L3_RX_ABORT_ONLY:
		wlanoidSerExtCmd(prAdapter, SER_ACTION_SET_ENABLE_MASK,
				 SER_ENABLE_TRACKING | SER_ENABLE_L3_RX_ABORT,
				 0);
		break;

	case SER_USER_CMD_ENABLE_MASK_L3_TX_ABORT_ONLY:
		wlanoidSerExtCmd(prAdapter, SER_ACTION_SET_ENABLE_MASK,
				 SER_ENABLE_TRACKING | SER_ENABLE_L3_TX_ABORT,
				 0);
		break;

	case SER_USER_CMD_ENABLE_MASK_L3_TX_DISABLE_ONLY:
		wlanoidSerExtCmd(prAdapter, SER_ACTION_SET_ENABLE_MASK,
				 SER_ENABLE_TRACKING |
				 SER_ENABLE_L3_TX_DISABLE, 0);
		break;

	case SER_USER_CMD_ENABLE_MASK_L3_BFRECOVER_ONLY:
		wlanoidSerExtCmd(prAdapter, SER_ACTION_SET_ENABLE_MASK,
				 SER_ENABLE_TRACKING |
				 SER_ENABLE_L3_BF_RECOVER, 0);
		break;

	case SER_USER_CMD_ENABLE_MASK_RECOVER_ALL:
		wlanoidSerExtCmd(prAdapter, SER_ACTION_SET_ENABLE_MASK,
				 (SER_ENABLE_TRACKING |
				  SER_ENABLE_L1_RECOVER |
				  SER_ENABLE_L2_RECOVER |
				  SER_ENABLE_L3_RX_ABORT |
				  SER_ENABLE_L3_TX_ABORT |
				  SER_ENABLE_L3_TX_DISABLE |
				  SER_ENABLE_L3_BF_RECOVER), 0);
		break;

	case SER_USER_CMD_L0_RECOVER:
			wlanoidSerExtCmd(prAdapter, SER_ACTION_RECOVER,
					 SER_SET_L0_RECOVER, 0);
			break;

	case SER_USER_CMD_L1_RECOVER:
		wlanoidSerExtCmd(prAdapter, SER_ACTION_RECOVER,
				 SER_SET_L1_RECOVER, 0);
		break;

	case SER_USER_CMD_L2_BN0_RECOVER:
		wlanoidSerExtCmd(prAdapter, SER_ACTION_RECOVER,
				 SER_SET_L2_RECOVER, ENUM_BAND_0);
		break;

	case SER_USER_CMD_L2_BN1_RECOVER:
		wlanoidSerExtCmd(prAdapter, SER_ACTION_RECOVER,
				 SER_SET_L2_RECOVER, ENUM_BAND_1);
		break;

	case SER_USER_CMD_L3_RX0_ABORT:
		wlanoidSerExtCmd(prAdapter, SER_ACTION_RECOVER,
				 SER_SET_L3_RX_ABORT, ENUM_BAND_0);
		break;

	case SER_USER_CMD_L3_RX1_ABORT:
		wlanoidSerExtCmd(prAdapter, SER_ACTION_RECOVER,
				 SER_SET_L3_RX_ABORT, ENUM_BAND_1);
		break;

	case SER_USER_CMD_L3_TX0_ABORT:
		wlanoidSerExtCmd(prAdapter, SER_ACTION_RECOVER,
				 SER_SET_L3_TX_ABORT, ENUM_BAND_0);
		break;

	case SER_USER_CMD_L3_TX1_ABORT:
		wlanoidSerExtCmd(prAdapter, SER_ACTION_RECOVER,
				 SER_SET_L3_TX_ABORT, ENUM_BAND_1);
		break;

	case SER_USER_CMD_L3_TX0_DISABLE:
		wlanoidSerExtCmd(prAdapter, SER_ACTION_RECOVER,
				 SER_SET_L3_TX_DISABLE, ENUM_BAND_0);
		break;

	case SER_USER_CMD_L3_TX1_DISABLE:
		wlanoidSerExtCmd(prAdapter, SER_ACTION_RECOVER,
				 SER_SET_L3_TX_DISABLE, ENUM_BAND_1);
		break;

	case SER_USER_CMD_L3_BF_RECOVER:
		wlanoidSerExtCmd(prAdapter, SER_ACTION_RECOVER,
				 SER_SET_L3_BF_RECOVER, 0);
		break;

	default:
		DBGLOG(OID, ERROR, "Error SER CMD\n");
	}

	return WLAN_STATUS_SUCCESS;
}

uint32_t wlanoidSerExtCmd(IN struct ADAPTER *prAdapter, uint8_t ucAction,
			 uint8_t ucSerSet, uint8_t ucDbdcIdx) {
	struct EXT_CMD_SER_T rCmdSer = {0};
	uint32_t rStatus = WLAN_STATUS_SUCCESS;

	rCmdSer.ucAction = ucAction;
	rCmdSer.ucSerSet = ucSerSet;
	rCmdSer.ucDbdcIdx = ucDbdcIdx;

	rStatus = wlanSendSetQueryExtCmd(prAdapter,
					 CMD_ID_LAYER_0_EXT_MAGIC_NUM,
					 EXT_CMD_ID_SER,
					 TRUE,
					 FALSE,
					 g_fgIsOid,
					 NULL,
					 nicOidCmdTimeoutCommon,
					 sizeof(struct EXT_CMD_SER_T),
					 (uint8_t *)&rCmdSer, NULL, 0);
	return rStatus;
}

#if (CFG_SUPPORT_TXPOWER_INFO == 1)
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
uint32_t
wlanoidQueryTxPowerInfo(IN struct ADAPTER *prAdapter,
			IN void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
			OUT uint32_t *pu4QueryInfoLen) {
	struct PARAM_TXPOWER_ALL_RATE_POWER_INFO_T *prTxPowerInfo =
			NULL;
	struct CMD_TX_POWER_SHOW_INFO_T rCmdTxPowerShowInfo;
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;

	if (!prAdapter)
		return WLAN_STATUS_FAILURE;
	if (!pvQueryBuffer)
		return WLAN_STATUS_FAILURE;
	if (!pu4QueryInfoLen)
		return WLAN_STATUS_FAILURE;

	if (u4QueryBufferLen <
	    sizeof(struct PARAM_TXPOWER_ALL_RATE_POWER_INFO_T)) {
		*pu4QueryInfoLen = sizeof(struct
					  PARAM_TXPOWER_ALL_RATE_POWER_INFO_T);
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}

	*pu4QueryInfoLen = sizeof(struct
				  PARAM_TXPOWER_ALL_RATE_POWER_INFO_T);

	prTxPowerInfo = (struct PARAM_TXPOWER_ALL_RATE_POWER_INFO_T
			 *) pvQueryBuffer;

	kalMemSet(&rCmdTxPowerShowInfo, 0,
		  sizeof(struct CMD_TX_POWER_SHOW_INFO_T));

	rCmdTxPowerShowInfo.ucPowerCtrlFormatId =
		TX_POWER_SHOW_INFO;
	rCmdTxPowerShowInfo.ucTxPowerInfoCatg =
		prTxPowerInfo->ucTxPowerCategory;
	rCmdTxPowerShowInfo.ucBandIdx = prTxPowerInfo->ucBandIdx;

	rWlanStatus = wlanSendSetQueryExtCmd(prAdapter,
			     CMD_ID_LAYER_0_EXT_MAGIC_NUM,
			     EXT_CMD_ID_TX_POWER_FEATURE_CTRL,
			     FALSE, /* Query Bit: True->write False->read */
			     TRUE,
			     g_fgIsOid,
			     nicCmdEventQueryTxPowerInfo,
			     nicOidCmdTimeoutCommon,
			     sizeof(struct CMD_TX_POWER_SHOW_INFO_T),
			     (uint8_t *) (&rCmdTxPowerShowInfo),
			     pvQueryBuffer,
			     u4QueryBufferLen);

	return rWlanStatus;
}
#endif

uint32_t
wlanoidSetDrvRoamingPolicy(IN struct ADAPTER *prAdapter,
			   IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			   OUT uint32_t *pu4SetInfoLen) {
	uint32_t u4RoamingPoily = 0;
	struct ROAMING_INFO *prRoamingFsmInfo;
	struct CONNECTION_SETTINGS *prConnSettings;
	uint32_t u4CurConPolicy;

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);

	u4RoamingPoily = *(uint32_t *)pvSetBuffer;

	prRoamingFsmInfo = (struct ROAMING_INFO *) &
			   (prAdapter->rWifiVar.rRoamingInfo);

	prConnSettings = (struct CONNECTION_SETTINGS *)
			 &prAdapter->rWifiVar.rConnSettings;
	u4CurConPolicy = prConnSettings->eConnectionPolicy;

	if (u4RoamingPoily == 1) {
		if (((prAdapter->rWifiVar.rAisFsmInfo.eCurrentState ==
		      AIS_STATE_NORMAL_TR)
		     || (prAdapter->rWifiVar.rAisFsmInfo.eCurrentState ==
			 AIS_STATE_ONLINE_SCAN))
		    && (prRoamingFsmInfo->eCurrentState == ROAMING_STATE_IDLE))
			roamingFsmRunEventStart(prAdapter);

		/* Change Connect by any , avoid to connect by BSSID on roaming
		 * or beacon timeout!
		 */
		prConnSettings->eConnectionPolicy = CONNECT_BY_SSID_ANY;

	} else {
		if (prRoamingFsmInfo->eCurrentState != ROAMING_STATE_IDLE)
			roamingFsmRunEventAbort(prAdapter);
	}
	prRoamingFsmInfo->fgDrvRoamingAllow = (u_int8_t)
					      u4RoamingPoily;

	DBGLOG(REQ, INFO,
	       "wlanoidSetDrvRoamingPolicy, RoamingPoily= %d, conn policy= [%d] -> [%d]\n",
	       u4RoamingPoily, u4CurConPolicy,
	       prRoamingFsmInfo->fgDrvRoamingAllow);

	return WLAN_STATUS_SUCCESS;
}

uint32_t wlanoidUpdateFtIes(struct ADAPTER *prAdapter, void *pvSetBuffer,
			    uint32_t u4SetBufferLen, uint32_t *pu4SetInfoLen)
{
	struct FT_IES *prFtIes = NULL;
	uint32_t u4IeLen = 0;
	uint8_t *pucIEStart = NULL;
	struct STA_RECORD *prStaRec = NULL;
#if CFG_SUPPORT_CFG80211_AUTH
	uint16_t u2Offset = 0;
#else
	struct MSG_SAA_FT_CONTINUE *prFtContinueMsg = NULL;
	struct cfg80211_update_ft_ies_params *ftie = NULL;
#endif

	if (!pvSetBuffer || u4SetBufferLen == 0) {
		DBGLOG(OID, ERROR,
		       "FT: pvSetBuffer is Null %d, Buffer Len %u\n",
		       !pvSetBuffer, u4SetBufferLen);
		return WLAN_STATUS_INVALID_DATA;
	}
	prStaRec = prAdapter->rWifiVar.rAisFsmInfo.prTargetStaRec;
	prFtIes = &prAdapter->prGlueInfo->rFtIeForTx;
#if CFG_SUPPORT_CFG80211_AUTH
	pucIEStart = (uint8_t *)pvSetBuffer;
	u4IeLen = u4SetBufferLen;
	DBGLOG(OID, INFO, "u4IeLen %d\n", u4IeLen);
#else
	ftie = (struct cfg80211_update_ft_ies_params *)pvSetBuffer;

	if (ftie->ie_len == 0) {
		DBGLOG(OID, WARN, "FT: FT Ies length is 0\n");
		return WLAN_STATUS_SUCCESS;
	}
	if (prFtIes->u4IeLength != ftie->ie_len) {
		kalMemFree(prFtIes->pucIEBuf, VIR_MEM_TYPE,
			   prFtIes->u4IeLength);
		prFtIes->pucIEBuf = kalMemAlloc(ftie->ie_len, VIR_MEM_TYPE);
		prFtIes->u4IeLength = ftie->ie_len;
	}
	pucIEStart = prFtIes->pucIEBuf;
	u4IeLen = prFtIes->u4IeLength;
	prFtIes->u2MDID = ftie->md;
#endif
	prFtIes->prFTIE = NULL;
	prFtIes->prMDIE = NULL;
	prFtIes->prRsnIE = NULL;
	prFtIes->prTIE = NULL;
#if CFG_SUPPORT_CFG80211_AUTH
	IE_FOR_EACH(pucIEStart, u4IeLen, u2Offset) {
		switch (IE_ID(pucIEStart)) {
		case ELEM_ID_MOBILITY_DOMAIN:
			if (prFtIes->prMDIE == NULL)
				prFtIes->prMDIE = kalMemAlloc(
					IE_SIZE(pucIEStart), VIR_MEM_TYPE);
			COPY_IE((unsigned long)(prFtIes->prMDIE), pucIEStart);
			prFtIes->u4IeLength += IE_SIZE(pucIEStart);
			break;
		case ELEM_ID_FAST_TRANSITION:
			if (prFtIes->prFTIE == NULL)
				prFtIes->prFTIE = kalMemAlloc(IE_SIZE(
					pucIEStart), VIR_MEM_TYPE);
			COPY_IE((unsigned long)(prFtIes->prFTIE), pucIEStart);
			prFtIes->u4IeLength += IE_SIZE(pucIEStart);
			break;
		case ELEM_ID_RESOURCE_INFO_CONTAINER:
			break;
		case ELEM_ID_TIMEOUT_INTERVAL:
			if (prFtIes->prTIE == NULL)
				prFtIes->prTIE = kalMemAlloc(
					IE_SIZE(pucIEStart), VIR_MEM_TYPE);
			COPY_IE((unsigned long)(prFtIes->prTIE), pucIEStart);
			prFtIes->u4IeLength += IE_SIZE(pucIEStart);
			break;
		case ELEM_ID_RSN:
			if (prFtIes->prRsnIE == NULL)
				prFtIes->prRsnIE = kalMemAlloc(
					IE_SIZE(pucIEStart), VIR_MEM_TYPE);
			COPY_IE((unsigned long)(prFtIes->prRsnIE), pucIEStart);
			prFtIes->u4IeLength += IE_SIZE(pucIEStart);
			break;
		}
	}
	DBGLOG(OID, INFO,
	       "FT: IesLen %u, MDIE %d FTIE %d RSN %d TIE %d\n",
			prFtIes->u4IeLength, !!prFtIes->prMDIE,
			!!prFtIes->prFTIE, !!prFtIes->prRsnIE,
			!!prFtIes->prTIE);

#else
	if (u4IeLen)
		kalMemCopy(pucIEStart, ftie->ie, u4IeLen);
	while (u4IeLen >= 2) {
		uint32_t u4InfoElemLen = IE_SIZE(pucIEStart);

		if (u4InfoElemLen > u4IeLen)
			break;
		switch (pucIEStart[0]) {
		case ELEM_ID_MOBILITY_DOMAIN:
			prFtIes->prMDIE =
				(struct IE_MOBILITY_DOMAIN *)pucIEStart;
			break;
		case ELEM_ID_FAST_TRANSITION:
			prFtIes->prFTIE =
				(struct IE_FAST_TRANSITION *)pucIEStart;
			break;
		case ELEM_ID_RESOURCE_INFO_CONTAINER:
			break;
		case ELEM_ID_TIMEOUT_INTERVAL:
			prFtIes->prTIE =
				(struct IE_TIMEOUT_INTERVAL *)pucIEStart;
			break;
		case ELEM_ID_RSN:
			prFtIes->prRsnIE = (struct RSN_INFO_ELEM *)pucIEStart;
			break;
		}
		u4IeLen -= u4InfoElemLen;
		pucIEStart += u4InfoElemLen;
	}
	DBGLOG(OID, INFO,
	       "FT: MdId %d IesLen %u, MDIE %d FTIE %d RSN %d TIE %d\n",
	       ftie->md, prFtIes->u4IeLength, !!prFtIes->prMDIE,
	       !!prFtIes->prFTIE, !!prFtIes->prRsnIE, !!prFtIes->prTIE);
#endif

#if !CFG_SUPPORT_CFG80211_AUTH
	/* check if SAA is waiting to send Reassoc req */
	if (!prStaRec || prStaRec->ucAuthTranNum != AUTH_TRANSACTION_SEQ_2 ||
		!prStaRec->fgIsReAssoc || prStaRec->ucStaState != STA_STATE_1)
		return WLAN_STATUS_SUCCESS;

	prFtContinueMsg = (struct MSG_SAA_FT_CONTINUE *)cnmMemAlloc(
		prAdapter, RAM_TYPE_MSG, sizeof(struct MSG_SAA_FT_CONTINUE));
	if (!prFtContinueMsg) {
		DBGLOG(OID, WARN, "FT: failed to allocate Join Req Msg\n");
		return WLAN_STATUS_FAILURE;
	}
	prFtContinueMsg->rMsgHdr.eMsgId = MID_OID_SAA_FSM_CONTINUE;
	prFtContinueMsg->prStaRec = prStaRec;
	/* ToDo: for Resource Request Protocol, we need to check if RIC request
	** is included.
	*/
	if (prFtIes->prMDIE && (prFtIes->prMDIE->ucBitMap & BIT(1)))
		prFtContinueMsg->fgFTRicRequest = TRUE;
	else
		prFtContinueMsg->fgFTRicRequest = FALSE;
	DBGLOG(OID, INFO, "FT: continue to do auth/assoc, Ft Request %d\n",
	       prFtContinueMsg->fgFTRicRequest);
	mboxSendMsg(prAdapter, MBOX_ID_0, (struct MSG_HDR *)prFtContinueMsg,
		    MSG_SEND_METHOD_BUF);
#endif
	return WLAN_STATUS_SUCCESS;
}

uint32_t wlanoidSendNeighborRequest(struct ADAPTER *prAdapter,
				    void *pvSetBuffer, uint32_t u4SetBufferLen,
				    uint32_t *pu4SetInfoLen)
{
	struct SUB_ELEMENT_LIST *prSSIDIE = NULL;
	struct BSS_INFO *prAisBssInfo = NULL;
	uint8_t ucSSIDIELen = 0;
	uint8_t *pucSSID = (uint8_t *)pvSetBuffer;

	if (!prAdapter || !prAdapter->prAisBssInfo)
		return WLAN_STATUS_INVALID_DATA;
	prAisBssInfo = prAdapter->prAisBssInfo;
	if (prAisBssInfo->eConnectionState != PARAM_MEDIA_STATE_CONNECTED) {
		DBGLOG(OID, ERROR, "didn't connected any Access Point\n");
		return WLAN_STATUS_FAILURE;
	}
	if (u4SetBufferLen == 0 || !pucSSID) {
		rlmTxNeighborReportRequest(prAdapter,
					   prAisBssInfo->prStaRecOfAP, NULL);
		return WLAN_STATUS_SUCCESS;
	}

	ucSSIDIELen = (uint8_t)(u4SetBufferLen + sizeof(*prSSIDIE));
	prSSIDIE = kalMemAlloc(ucSSIDIELen, PHY_MEM_TYPE);
	if (!prSSIDIE) {
		DBGLOG(OID, ERROR, "No Memory\n");
		return WLAN_STATUS_FAILURE;
	}
	prSSIDIE->prNext = NULL;
	prSSIDIE->rSubIE.ucSubID = ELEM_ID_SSID;
	prSSIDIE->rSubIE.ucLength = (uint8_t)u4SetBufferLen;
	kalMemCopy(&prSSIDIE->rSubIE.aucOptInfo[0], pucSSID,
		   (uint8_t)u4SetBufferLen);
	DBGLOG(OID, INFO, "Send Neighbor Request, SSID=%s\n", pucSSID);
	rlmTxNeighborReportRequest(prAdapter, prAisBssInfo->prStaRecOfAP,
				   prSSIDIE);
	kalMemFree(prSSIDIE, PHY_MEM_TYPE, ucSSIDIELen);
	return WLAN_STATUS_SUCCESS;
}

uint32_t wlanoidSync11kCapabilities(struct ADAPTER *prAdapter,
				    void *pvSetBuffer, uint32_t u4SetBufferLen,
				    uint32_t *pu4SetInfoLen)
{
	struct CMD_SET_RRM_CAPABILITY rCmdRrmCapa;

	kalMemZero(&rCmdRrmCapa, sizeof(rCmdRrmCapa));
	rCmdRrmCapa.ucCmdVer = 0x1;
	rCmdRrmCapa.ucRrmEnable = 1;
	rlmFillRrmCapa(&rCmdRrmCapa.ucCapabilities[0]);
	return wlanSendSetQueryCmd(
		prAdapter, CMD_ID_SET_RRM_CAPABILITY, TRUE, FALSE, g_fgIsOid,
		nicCmdEventSetCommon, nicOidCmdTimeoutCommon,
		sizeof(struct CMD_SET_RRM_CAPABILITY), (uint8_t *)&rCmdRrmCapa,
		pvSetBuffer, u4SetBufferLen);
}

static uint8_t pow_r(uint8_t x, uint8_t y)
{
	uint8_t result = 0;
	uint8_t tmp = 0;

	if (y == 0)
		return 1;
	if (y == 1)
		return x;
	tmp = pow_r(x, y/2);
	if ((y & 1) != 0)
		result = x * tmp * tmp;
	else
		result = tmp * tmp;
	return result;

}

uint32_t wlanoidSendBTMQuery(struct ADAPTER *prAdapter, void *pvSetBuffer,
			     uint32_t u4SetBufferLen, uint32_t *pu4SetInfoLen)
{
	struct STA_RECORD *prStaRec = NULL;
	struct BSS_TRANSITION_MGT_PARAM_T *prBtmMgt = NULL;
	uint8_t i = 0;
	uint8_t uReason = 0;

	if (!prAdapter->prAisBssInfo ||
	    prAdapter->prAisBssInfo->eConnectionState !=
		PARAM_MEDIA_STATE_CONNECTED) {
		DBGLOG(OID, INFO, "Not connected yet\n");
		return WLAN_STATUS_FAILURE;
	}
	prStaRec = prAdapter->prAisBssInfo->prStaRecOfAP;
	if (!prStaRec || !prStaRec->fgSupportBTM) {
		DBGLOG(OID, INFO,
		       "Target BSS(%p) didn't support Bss Transition Management\n",
		       prStaRec);
		return WLAN_STATUS_FAILURE;
	}

	if (pvSetBuffer != NULL) {
		for (i = 0; i < strlen(pvSetBuffer); i++) {
			uReason += ((*(uint8_t *)(pvSetBuffer + i) - '0')
				* pow_r(10, (strlen(pvSetBuffer) - i - 1)));
		}
	}
	prBtmMgt = &prAdapter->rWifiVar.rAisSpecificBssInfo.rBTMParam;
	prBtmMgt->ucDialogToken = wnmGetBtmToken();
	prBtmMgt->ucQueryReason = pvSetBuffer ? uReason
					      : BSS_TRANSITION_LOW_RSSI;
	DBGLOG(OID, INFO, "Send BssTransitionManagementQuery, Reason %d\n",
		prBtmMgt->ucQueryReason);
	wnmSendBTMQueryFrame(prAdapter, prStaRec);
	return WLAN_STATUS_SUCCESS;
}

/*
 * This func is mainly from bionic's strtok.c
 */
static int8_t *strtok_r(int8_t *s, const int8_t *delim, int8_t **last)
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

uint32_t wlanoidTspecOperation(struct ADAPTER *prAdapter, void *pvBuffer,
			       uint32_t u4BufferLen, uint32_t *pu4InfoLen)
{
	struct PARAM_QOS_TSPEC *prTspecParam = NULL;
	struct MSG_TS_OPERATE *prMsgTsOperate = NULL;
	uint8_t *pucCmd = (uint8_t *)pvBuffer;
	uint8_t *pucSavedPtr = NULL;
	uint8_t *pucItem = NULL;
	uint32_t u4Ret = 1;
	uint8_t ucApsdSetting = 2; /* 0: legacy; 1: u-apsd; 2: not set yet */
	enum TSPEC_OP_CODE eTsOp;

#if !CFG_SUPPORT_WMM_AC
	DBGLOG(OID, INFO, "WMM AC is not supported\n");
	return WLAN_STATUS_FAILURE;
#endif
	if (kalStrniCmp(pucCmd, "dumpts", 6) == 0) {
		*pu4InfoLen = kalSnprintf(pucCmd, u4BufferLen, "%s",
					  "\nAll Active Tspecs:\n");
		u4BufferLen -= *pu4InfoLen;
		pucCmd += *pu4InfoLen;
		*pu4InfoLen +=
			wmmDumpActiveTspecs(prAdapter, pucCmd, u4BufferLen);
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
	/* addts token n,tid n,dir n,psb n,up n,fixed n,size n,maxsize
	** n,maxsrvint n, minsrvint n,
	** inact n, suspension n, srvstarttime n, minrate n,meanrate n,peakrate
	** n,burst n,delaybound n,
	** phyrate n,SBA n,mediumtime n
	*/
	prMsgTsOperate = (struct MSG_TS_OPERATE *)cnmMemAlloc(
		prAdapter, RAM_TYPE_MSG, sizeof(struct MSG_TS_OPERATE));
	if (!prMsgTsOperate)
		return WLAN_STATUS_FAILURE;

	kalMemZero(prMsgTsOperate, sizeof(struct MSG_TS_OPERATE));
	prMsgTsOperate->rMsgHdr.eMsgId = MID_OID_WMM_TSPEC_OPERATE;
	prMsgTsOperate->eOpCode = eTsOp;
	prTspecParam = &prMsgTsOperate->rTspecParam;
	pucCmd += 6;
	pucItem = (uint8_t *)strtok_r((int8_t *)pucCmd, ",",
				      (int8_t **)&pucSavedPtr);
	while (pucItem) {
		if (kalStrniCmp(pucItem, "token ", 6) == 0)
			u4Ret = kstrtou8(pucItem + 6, 0,
					 &prTspecParam->ucDialogToken);
		else if (kalStrniCmp(pucItem, "tid ", 4) == 0) {
			u4Ret = kstrtou8(pucItem + 4, 0,
					 &prMsgTsOperate->ucTid);
			prTspecParam->rTsInfo.ucTid = prMsgTsOperate->ucTid;
		} else if (kalStrniCmp(pucItem, "dir ", 4) == 0)
			u4Ret = kstrtou8(pucItem + 4, 0,
					 &prTspecParam->rTsInfo.ucDirection);
		else if (kalStrniCmp(pucItem, "psb ", 4) == 0)
			u4Ret = kstrtou8(pucItem+4, 0, &ucApsdSetting);
		else if (kalStrniCmp(pucItem, "up ", 3) == 0)
			u4Ret = kstrtou8(pucItem + 3, 0,
					 &prTspecParam->rTsInfo.ucuserPriority);
		else if (kalStrniCmp(pucItem, "size ", 5) == 0) {
			uint16_t u2Size = 0;

			u4Ret = kstrtou16(pucItem+5, 0, &u2Size);
			prTspecParam->u2NominalMSDUSize |= u2Size;
		} else if (kalStrniCmp(pucItem, "fixed ", 6) == 0) {
			uint8_t ucFixed = 0;

			u4Ret = kstrtou8(pucItem+6, 0, &ucFixed);
			if (ucFixed)
				prTspecParam->u2NominalMSDUSize |= BIT(15);
		} else if (kalStrniCmp(pucItem, "maxsize ", 8) == 0)
			u4Ret = kstrtou16(pucItem + 8, 0,
					  &prTspecParam->u2MaxMSDUsize);
		else if (kalStrniCmp(pucItem, "maxsrvint ", 10) == 0)
			u4Ret = kalkStrtou32(pucItem + 10, 0,
					     &prTspecParam->u4MaxSvcIntv);
		else if (kalStrniCmp(pucItem, "minsrvint ", 10) == 0)
			u4Ret = kalkStrtou32(pucItem + 10, 0,
					     &prTspecParam->u4MinSvcIntv);
		else if (kalStrniCmp(pucItem, "inact ", 6) == 0)
			u4Ret = kalkStrtou32(pucItem + 6, 0,
					     &prTspecParam->u4InactIntv);
		else if (kalStrniCmp(pucItem, "suspension ", 11) == 0)
			u4Ret = kalkStrtou32(pucItem + 11, 0,
					     &prTspecParam->u4SpsIntv);
		else if (kalStrniCmp(pucItem, "srvstarttime ", 13) == 0)
			u4Ret = kalkStrtou32(pucItem + 13, 0,
					     &prTspecParam->u4SvcStartTime);
		else if (kalStrniCmp(pucItem, "minrate ", 8) == 0)
			u4Ret = kalkStrtou32(pucItem + 8, 0,
					     &prTspecParam->u4MinDataRate);
		else if (kalStrniCmp(pucItem, "meanrate ", 9) == 0)
			u4Ret = kalkStrtou32(pucItem + 9, 0,
					     &prTspecParam->u4MeanDataRate);
		else if (kalStrniCmp(pucItem, "peakrate ", 9) == 0)
			u4Ret = kalkStrtou32(pucItem + 9, 0,
					     &prTspecParam->u4PeakDataRate);
		else if (kalStrniCmp(pucItem, "burst ", 6) == 0)
			u4Ret = kalkStrtou32(pucItem + 6, 0,
					     &prTspecParam->u4MaxBurstSize);
		else if (kalStrniCmp(pucItem, "delaybound ", 11) == 0)
			u4Ret = kalkStrtou32(pucItem + 11, 0,
					     &prTspecParam->u4DelayBound);
		else if (kalStrniCmp(pucItem, "phyrate ", 8) == 0)
			u4Ret = kalkStrtou32(pucItem + 8, 0,
					     &prTspecParam->u4MinPHYRate);
		else if (kalStrniCmp(pucItem, "sba ", 4) == 0)
			u4Ret = wlanDecimalStr2Hexadecimals(
				pucItem + 4, &prTspecParam->u2Sba);
		else if (kalStrniCmp(pucItem, "mediumtime ", 11) == 0)
			u4Ret = kstrtou16(pucItem + 11, 0,
					  &prTspecParam->u2MediumTime);

		if (u4Ret) {
			DBGLOG(OID, ERROR, "Parse %s error\n", pucItem);
			cnmMemFree(prAdapter, prMsgTsOperate);
			return WLAN_STATUS_FAILURE;
		}
		pucItem =
			(uint8_t *)strtok_r(NULL, ",", (int8_t **)&pucSavedPtr);
	}
	/* if APSD is not set in addts request, use global wmmps settings */
	if (!prAdapter->prAisBssInfo)
		DBGLOG(OID, ERROR, "AisBssInfo is NULL!\n");
	else if (ucApsdSetting == 2) {
		struct PM_PROFILE_SETUP_INFO *prPmProf = NULL;
		enum ENUM_ACI eAc =
			aucUp2ACIMap[prTspecParam->rTsInfo.ucuserPriority];

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
	DBGLOG(OID, INFO,
	       "%s %d %d %d %d %d %d %d %u %u %u %u %u %u %u %u %u %u %u 0x%04x %d\n",
	       pucCmd, prTspecParam->ucDialogToken, prTspecParam->rTsInfo.ucTid,
	       prTspecParam->rTsInfo.ucDirection, prTspecParam->rTsInfo.ucApsd,
	       prTspecParam->rTsInfo.ucuserPriority,
	       prTspecParam->u2NominalMSDUSize, prTspecParam->u2MaxMSDUsize,
	       prTspecParam->u4MaxSvcIntv, prTspecParam->u4MinSvcIntv,
	       prTspecParam->u4InactIntv, prTspecParam->u4SpsIntv,
	       prTspecParam->u4SvcStartTime, prTspecParam->u4MinDataRate,
	       prTspecParam->u4MeanDataRate, prTspecParam->u4PeakDataRate,
	       prTspecParam->u4MaxBurstSize, prTspecParam->u4DelayBound,
	       prTspecParam->u4MinPHYRate, prTspecParam->u2Sba,
	       prTspecParam->u2MediumTime);
	mboxSendMsg(prAdapter, MBOX_ID_0, (struct MSG_HDR *)prMsgTsOperate,
		    MSG_SEND_METHOD_BUF);
	return WLAN_STATUS_SUCCESS;
}

/* It's a Integretion Test function for RadioMeasurement. If you found errors
** during doing Radio Measurement,
** you can run this IT function with iwpriv wlan0 driver \"RM-IT
** xx,xx,xx, xx\"
** xx,xx,xx,xx is the RM request frame data
*/
uint32_t wlanoidPktProcessIT(struct ADAPTER *prAdapter, void *pvBuffer,
			     uint32_t u4BufferLen, uint32_t *pu4InfoLen)
{
	struct SW_RFB rSwRfb;
	static uint8_t aucPacket[200] = {0,};
	uint8_t *pucSavedPtr = (int8_t *)pvBuffer;
	uint8_t *pucItem = NULL;
	uint8_t j = 0;
	int8_t i = 0;
	uint8_t ucByte;
	u_int8_t fgBTMReq = FALSE;
	void (*process_func)(struct ADAPTER *prAdapter,
			     struct SW_RFB *prSwRfb);

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
		fgBTMReq = TRUE;
	} else {
		pucSavedPtr[10] = 0;
		DBGLOG(OID, ERROR, "IT type %s is not supported\n",
		       pucSavedPtr);
		return WLAN_STATUS_NOT_SUPPORTED;
	}
	kalMemZero(aucPacket, sizeof(aucPacket));
	pucItem = strtok_r(pucSavedPtr, ",", (int8_t **)&pucSavedPtr);
	while (pucItem) {
		ucByte = *pucItem;
		i = 0;
		while (ucByte) {
			if (i > 1) {
				DBGLOG(OID, ERROR,
				       "more than 2 char for one byte\n");
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
				DBGLOG(OID, ERROR, "not a hex char %c\n",
				       ucByte);
				return WLAN_STATUS_FAILURE;
			}
			ucByte = *(++pucItem);
			i++;
		}
		j++;
		pucItem = strtok_r(NULL, ",", (int8_t **)&pucSavedPtr);
	}
	DBGLOG(OID, INFO, "Dump IT packet, len %d\n", j);
	dumpMemory8(aucPacket, j);
	if (j < WLAN_MAC_MGMT_HEADER_LEN) {
		DBGLOG(OID, ERROR, "packet length %d less than mac header 24\n",
		       j);
		return WLAN_STATUS_FAILURE;
	}
	rSwRfb.pvHeader = (void *)&aucPacket[0];
	rSwRfb.u2PacketLen = j;
	rSwRfb.u2HeaderLen = WLAN_MAC_MGMT_HEADER_LEN;
	rSwRfb.ucStaRecIdx = KAL_NETWORK_TYPE_AIS_INDEX;
	if (fgBTMReq) {
		struct HW_MAC_RX_DESC rRxStatus;

		rSwRfb.prRxStatus = (struct HW_MAC_RX_DESC *)&rRxStatus;
		rSwRfb.prRxStatus->ucChanFreq = 6;
		wnmWNMAction(prAdapter, &rSwRfb);
	} else {
		process_func(prAdapter, &rSwRfb);
	}

	return WLAN_STATUS_SUCCESS;
}

/* Firmware Integration Test functions
** This function receives commands that are input by a firmware IT test script
** By using IT test script, RD no need to run IT with a real Access Point
** For example: iwpriv wlan0 driver \"Fw-Event Roaming ....\"
*/
uint32_t wlanoidFwEventIT(struct ADAPTER *prAdapter, void *pvBuffer,
			  uint32_t u4BufferLen, uint32_t *pu4InfoLen)
{
	uint8_t *pucCmd = (int8_t *)pvBuffer;

	/* Firmware roaming Integration Test case */
	if (!kalStrniCmp(pucCmd, "Roaming", 7)) {
		uint8_t ucRCPI = 0;
		uint8_t ucFrameType = 0;
		uint32_t i = 0;
		struct CMD_INFO *prCmdInfo;
		struct GLUE_INFO *prGlueInfo = prAdapter->prGlueInfo;
		struct WLAN_ACTION_FRAME *prAction = NULL;
		struct QUE_ENTRY *prEntry = NULL;
		struct QUE_ENTRY *prPreEntry = NULL;
		struct CMD_ROAMING_TRANSIT rTransit = {0};

		GLUE_SPIN_LOCK_DECLARATION();

		if (prAdapter->rWifiVar.rAisFsmInfo.prTargetBssDesc)
			rTransit.u2Data = prAdapter->rWifiVar.rAisFsmInfo
						  .prTargetBssDesc->ucRCPI;
		rTransit.u2Event = ROAMING_EVENT_DISCOVERY;
		rTransit.eReason = ROAMING_REASON_POOR_RCPI;
		roamingFsmRunEventDiscovery(prAdapter, &rTransit);
		/* Try to find the BTM query frame which is sent by
		** roamingFsmRunEventDiscovery
		*/
		GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);
		for (prEntry = QUEUE_GET_HEAD(&prGlueInfo->rCmdQueue);
			prEntry != NULL; prPreEntry = prEntry,
			prEntry = QUEUE_GET_NEXT_ENTRY(&prCmdInfo->rQueEntry)) {
			prCmdInfo = (struct CMD_INFO *)prEntry;
			if (!prCmdInfo->prMsduInfo ||
			    prCmdInfo->prMsduInfo->eSrc != TX_PACKET_MGMT ||
				!prCmdInfo->prMsduInfo->prPacket)
				continue;
			prAction = (struct WLAN_ACTION_FRAME *)
					   prCmdInfo->prMsduInfo->prPacket;
			if (prAction->u2FrameCtrl != MAC_FRAME_ACTION)
				continue;
			if (prAction->ucCategory == CATEGORY_RM_ACTION &&
				prAction->ucAction ==
				ACTION_NEIGHBOR_REPORT_REQ) {
				ucFrameType = 1;
				break;
			}
			if (prAction->ucCategory == CATEGORY_WNM_ACTION &&
			    prAction->ucAction ==
				ACTION_WNM_BSS_TRANSITION_MANAGEMENT_QUERY) {
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
		/* roamingFsmRunEventDiscovery has sent a btm query frame */
		if (ucFrameType == 2) {
			struct ACTION_BTM_QUERY_FRAME *prBtmQuery =
				(struct ACTION_BTM_QUERY_FRAME *)prAction;

			/* IT string may be "Roaming <btm request packet
			** string>", to reuse btm it function,
			** we need to replace Roaming with BTM-IT. Length of
			** Roaming is 7 bytes, so pucCmd
			** need to self add 1, and buffer length need to self
			** minus 1, and copy BTM-IT to pucCmd.
			*/
			pucCmd++;
			u4BufferLen--;
			kalMemCopy(pucCmd, "BTM-IT", 6);

			/* Find the diaglogToken string in <btm request packet
			** string>, it follows "BTM-IT ", whose length is 7
			*/
			for (ucRCPI = 0, i = 7; i < u4BufferLen; i++) {
				if (pucCmd[i] == ',')
					ucRCPI++;
				if (ucRCPI ==
				    OFFSET_OF(struct ACTION_BTM_QUERY_FRAME,
					      ucDialogToken))
					break;
			}
			/* Replace diaglog token string with the token that is
			** in query frame
			*/
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
			wlanoidPktProcessIT(prAdapter, (void *)pucCmd,
					    u4BufferLen, pu4InfoLen);
		} else if (ucFrameType == 1) {
			/* Not support neighbor ap report request IT now */
		}
	} else {
		DBGLOG(OID, ERROR, "Not supported Fw Event IT type %s\n",
		       pucCmd);
		return WLAN_STATUS_FAILURE;
	}
	return WLAN_STATUS_SUCCESS;
}

uint32_t wlanoidDumpUapsdSetting(struct ADAPTER *prAdapter, void *pvBuffer,
				 uint32_t u4BufferLen, uint32_t *pu4InfoLen)
{
	uint8_t *pucCmd = (uint8_t *)pvBuffer;
	uint8_t ucFinalSetting = 0;
	uint8_t ucStaticSetting = 0;
	struct PM_PROFILE_SETUP_INFO *prPmProf = NULL;

	if (!pvBuffer) {
		DBGLOG(OID, ERROR, "pvBuffer is NULL\n");
		return WLAN_STATUS_FAILURE;
	}
	if (!prAdapter->prAisBssInfo)
		return WLAN_STATUS_FAILURE;
	prPmProf = &prAdapter->prAisBssInfo->rPmProfSetupInfo;
	ucStaticSetting =
		(prPmProf->ucBmpDeliveryAC << 4) | prPmProf->ucBmpTriggerAC;
	ucFinalSetting = wmmCalculateUapsdSetting(prAdapter);
	*pu4InfoLen = kalSnprintf(
		pucCmd, u4BufferLen,
		"\nStatic Uapsd Setting:0x%02x\nFinal Uapsd Setting:0x%02x",
		ucStaticSetting, ucFinalSetting);
	return WLAN_STATUS_SUCCESS;
}

#if CFG_SUPPORT_OSHARE
uint32_t
wlanoidSetOshareMode(IN struct ADAPTER *prAdapter,
		     IN void *pvSetBuffer,
		     IN uint32_t u4SetBufferLen,
		     OUT uint32_t *pu4SetInfoLen) {
	if (!prAdapter || !pvSetBuffer)
		return WLAN_STATUS_INVALID_DATA;

	DBGLOG(OID, TRACE, "wlanoidSetOshareMode\n");

	return wlanSendSetQueryCmd(prAdapter, /* prAdapter */
			   CMD_ID_SET_OSHARE_MODE, /* ucCID */
			   TRUE, /* fgSetQuery */
			   FALSE, /* fgNeedResp */
			   g_fgIsOid, /* fgIsOid */
			   nicCmdEventSetCommon, /* pfCmdDoneHandler*/
			   nicOidCmdTimeoutCommon, /* pfCmdTimeoutHandler */
			   u4SetBufferLen, /* u4SetQueryInfoLen */
			   (uint8_t *) pvSetBuffer,/* pucInfoBuffer */
			   NULL, /* pvSetQueryBuffer */
			   0); /* u4SetQueryBufferLen */
}
#endif

uint32_t
wlanoidQueryWifiLogLevelSupport(IN struct ADAPTER *prAdapter,
				IN void *pvQueryBuffer,
				IN uint32_t u4QueryBufferLen,
				OUT uint32_t *pu4QueryInfoLen) {
	struct PARAM_WIFI_LOG_LEVEL_UI *pparam;

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	pparam = (struct PARAM_WIFI_LOG_LEVEL_UI *) pvQueryBuffer;
	pparam->u4Enable = wlanDbgLevelUiSupport(prAdapter,
			   pparam->u4Version, pparam->u4Module);

	DBGLOG(OID, INFO, "version: %d, module: %d, enable: %d\n",
	       pparam->u4Version,
	       pparam->u4Module,
	       pparam->u4Enable);

	*pu4QueryInfoLen = sizeof(struct PARAM_WIFI_LOG_LEVEL_UI);

	return WLAN_STATUS_SUCCESS;
}

uint32_t
wlanoidQueryWifiLogLevel(IN struct ADAPTER *prAdapter,
			 IN void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
			 OUT uint32_t *pu4QueryInfoLen) {
	struct PARAM_WIFI_LOG_LEVEL *pparam;

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	pparam = (struct PARAM_WIFI_LOG_LEVEL *) pvQueryBuffer;
	pparam->u4Level = wlanDbgGetLogLevelImpl(prAdapter,
			  pparam->u4Version,
			  pparam->u4Module);

	DBGLOG(OID, INFO, "version: %d, module: %d, level: %d\n",
	       pparam->u4Version,
	       pparam->u4Module,
	       pparam->u4Level);

	*pu4QueryInfoLen = sizeof(struct PARAM_WIFI_LOG_LEVEL_UI);

	return WLAN_STATUS_SUCCESS;
}

uint32_t
wlanoidSetWifiLogLevel(IN struct ADAPTER *prAdapter,
		       IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		       OUT uint32_t *pu4SetInfoLen) {
	struct PARAM_WIFI_LOG_LEVEL *pparam;

	ASSERT(prAdapter);
	if (u4SetBufferLen)
		ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	pparam = (struct PARAM_WIFI_LOG_LEVEL *) pvSetBuffer;

	DBGLOG(OID, INFO, "version: %d, module: %d, level: %d\n",
	       pparam->u4Version,
	       pparam->u4Module,
	       pparam->u4Level);

	wlanDbgSetLogLevelImpl(prAdapter,
			       pparam->u4Version,
			       pparam->u4Module,
			       pparam->u4Level);

	return WLAN_STATUS_SUCCESS;
}

uint32_t wlanoidSetDrvSer(IN struct ADAPTER *prAdapter,
			  IN void *pvSetBuffer,
			  IN uint32_t u4SetBufferLen,
			  OUT uint32_t *pu4SetInfoLen)
{
	ASSERT(prAdapter);

	prAdapter->u4HifChkFlag |= HIF_DRV_SER;
	kalSetHifDbgEvent(prAdapter->prGlueInfo);

	return 0;
}

uint32_t wlanoidSetAmsduNum(IN struct ADAPTER *prAdapter,
			    IN void *pvSetBuffer,
			    IN uint32_t u4SetBufferLen,
			    OUT uint32_t *pu4SetInfoLen)
{
	struct mt66xx_chip_info *prChipInfo = NULL;

	ASSERT(prAdapter);
	if (u4SetBufferLen)
		ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	prChipInfo = prAdapter->chip_info;
	prChipInfo->ucMaxSwAmsduNum = (uint8_t)*((uint32_t *)pvSetBuffer);
	DBGLOG(OID, INFO, "Set SW AMSDU Num: %d\n",
	       prChipInfo->ucMaxSwAmsduNum);
	return 0;
}

uint32_t wlanoidSetAmsduSize(IN struct ADAPTER *prAdapter,
			     IN void *pvSetBuffer,
			     IN uint32_t u4SetBufferLen,
			     OUT uint32_t *pu4SetInfoLen)
{
	struct mt66xx_chip_info *prChipInfo = NULL;
	struct WIFI_VAR *prWifiVar = NULL;

	ASSERT(prAdapter);
	if (u4SetBufferLen)
		ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	prChipInfo = prAdapter->chip_info;
	prWifiVar = &prAdapter->rWifiVar;
	prWifiVar->u4TxMaxAmsduInAmpduLen = *((uint32_t *)pvSetBuffer);
	DBGLOG(OID, INFO, "Set SW AMSDU max Size: %d\n",
	       prWifiVar->u4TxMaxAmsduInAmpduLen);
	return 0;
}

uint32_t
wlanoidShowPdmaInfo(IN struct ADAPTER *prAdapter,
		    IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		    OUT uint32_t *pu4SetInfoLen)
{
	prAdapter->u4HifDbgFlag |= DEG_HIF_PDMA;
	kalSetHifDbgEvent(prAdapter->prGlueInfo);

	return 0;
}

uint32_t
wlanoidShowPseInfo(IN struct ADAPTER *prAdapter,
		   IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		   OUT uint32_t *pu4SetInfoLen)
{
	prAdapter->u4HifDbgFlag |= DEG_HIF_PSE;
	kalSetHifDbgEvent(prAdapter->prGlueInfo);

	return 0;
}

uint32_t
wlanoidShowPleInfo(IN struct ADAPTER *prAdapter,
		   IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		   OUT uint32_t *pu4SetInfoLen)
{
	prAdapter->u4HifDbgFlag |= DEG_HIF_PLE;
	kalSetHifDbgEvent(prAdapter->prGlueInfo);

	return 0;
}

uint32_t
wlanoidShowCsrInfo(IN struct ADAPTER *prAdapter,
		   IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		   OUT uint32_t *pu4SetInfoLen)
{
	prAdapter->u4HifDbgFlag |= DEG_HIF_HOST_CSR;
	kalSetHifDbgEvent(prAdapter->prGlueInfo);

	return 0;
}

uint32_t
wlanoidShowDmaschInfo(IN struct ADAPTER *prAdapter,
		      IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		      OUT uint32_t *pu4SetInfoLen)
{
	prAdapter->u4HifDbgFlag |= DEG_HIF_DMASCH;
	kalSetHifDbgEvent(prAdapter->prGlueInfo);

	return 0;
}

#if CFG_SUPPORT_LOWLATENCY_MODE
/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to enable/disable low latency mode
 *
 * \param[in]  prAdapter       A pointer to the Adapter structure.
 * \param[in]  pvSetBuffer     A pointer to the buffer that holds the
 *                             OID-specific data to be set.
 * \param[in]  u4SetBufferLen  The number of bytes the set buffer.
 * \param[out] pu4SetInfoLen   Points to the number of bytes it read or is
 *                             needed
 * \retval WLAN_STATUS_SUCCESS
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanoidSetLowLatencyMode(
	IN struct ADAPTER *prAdapter,
	IN void *pvSetBuffer,
	IN uint32_t u4SetBufferLen,
	OUT uint32_t *pu4SetInfoLen) {
	u_int8_t fgEnMode = FALSE; /* Low Latency Mode */
	u_int8_t fgEnScan = FALSE; /* Scan management */
	u_int8_t fgEnPM = FALSE; /* Power management */
	uint32_t u4Events;
	uint32_t u4PowerFlag;
	struct PARAM_POWER_MODE_ rPowerMode;
	struct WIFI_VAR *prWifiVar = NULL;

	DEBUGFUNC("wlanoidSetLowLatencyMode");

	ASSERT(prAdapter);
	ASSERT(pvSetBuffer);
	if (u4SetBufferLen != sizeof(uint32_t)) {
		*pu4SetInfoLen = sizeof(uint32_t);
		return WLAN_STATUS_INVALID_LENGTH;
	}
	ASSERT(pu4SetInfoLen);

	/* Initialize */
	prWifiVar = &prAdapter->rWifiVar;
	kalMemCopy(&u4Events, pvSetBuffer, u4SetBufferLen);
	DBGLOG(OID, INFO,
		"LowLatency(gaming) event - gas:0x%x, net:0x%x, whitelist:0x%x, scan=%u, reorder=%u, power=%u\n",
		(u4Events & GED_EVENT_GAS),
		(u4Events & GED_EVENT_NETWORK),
		(u4Events & GED_EVENT_DOPT_WIFI_SCAN),
		(uint32_t)prWifiVar->ucLowLatencyModeScan,
		(uint32_t)prWifiVar->ucLowLatencyModeReOrder,
		(uint32_t)prWifiVar->ucLowLatencyModePower);
	rPowerMode.ucBssIdx = prAdapter->prAisBssInfo->ucBssIndex;
	u4PowerFlag =
		prAdapter->rWlanInfo.u4PowerSaveFlag[rPowerMode.ucBssIdx];

	/* Enable/disable low latency mode decision:
	 *
	 * Enable if it's GAS and network event
	 * and the Glue media state is connected.
	 */
	if ((u4Events & GED_EVENT_GAS) != 0
	    && (u4Events & GED_EVENT_NETWORK) != 0
	    && PARAM_MEDIA_STATE_CONNECTED
	    == kalGetMediaStateIndicated(prAdapter->prGlueInfo))
		fgEnMode = TRUE; /* It will enable low latency mode */

	/* Enable/disable scan management decision:
	 *
	 * Enable if it will enable low latency mode.
	 * Or, enable if it is a white list event.
	 */
	if (fgEnMode != TRUE
	    || (u4Events & GED_EVENT_DOPT_WIFI_SCAN) != 0)
		fgEnScan = TRUE; /* It will enable scan management */

	/* Enable/disable power management decision:
	 */
	if (BIT(PS_CALLER_GPU) & u4PowerFlag)
		fgEnPM = TRUE;
	else
		fgEnPM = FALSE;

	/* Debug log for the actions */
	if (fgEnMode != prAdapter->fgEnLowLatencyMode
	    || fgEnScan != prAdapter->fgEnCfg80211Scan
	    || fgEnPM != fgEnMode) {
		DBGLOG(OID, INFO,
		       "LowLatency(gaming) change (m:%d,s:%d,PM:%d,F:0x%x)\n",
		       fgEnMode, fgEnScan, fgEnPM, u4PowerFlag);
	}

	/* Scan management:
	 *
	 * Disable/enable scan
	 */
	if ((prWifiVar->ucLowLatencyModeScan == FEATURE_ENABLED) &&
	    (fgEnScan != prAdapter->fgEnCfg80211Scan))
		prAdapter->fgEnCfg80211Scan = fgEnScan;

	if ((prWifiVar->ucLowLatencyModeReOrder == FEATURE_ENABLED) &&
	    (fgEnMode != prAdapter->fgEnLowLatencyMode)) {
		prAdapter->fgEnLowLatencyMode = fgEnMode;

		/* Queue management:
		 *
		 * Change QM RX BA timeout if the gaming mode state changed
		 */
		if (fgEnMode) {
			prAdapter->u4QmRxBaMissTimeout
				= QM_RX_BA_ENTRY_MISS_TIMEOUT_MS_SHORT;
		} else {
			prAdapter->u4QmRxBaMissTimeout
				= QM_RX_BA_ENTRY_MISS_TIMEOUT_MS;
		}
	}

	/* Power management:
	 *
	 * Set power saving mode profile to FW
	 *
	 * Do if 1. the power saving caller including GPU
	 * and 2. it will disable low latency mode.
	 * Or, do if 1. the power saving caller is not including GPU
	 * and 2. it will enable low latency mode.
	 */
	if ((prWifiVar->ucLowLatencyModePower == FEATURE_ENABLED) &&
	    (fgEnPM != fgEnMode)) {
		if (fgEnMode == TRUE)
			rPowerMode.ePowerMode = Param_PowerModeCAM;
		else
			rPowerMode.ePowerMode = Param_PowerModeFast_PSP;

		nicConfigPowerSaveProfile(prAdapter, rPowerMode.ucBssIdx,
			rPowerMode.ePowerMode, FALSE, PS_CALLER_GPU);
	}

	*pu4SetInfoLen = 0; /* We do not need to read */

	return WLAN_STATUS_SUCCESS;
}
#endif /* CFG_SUPPORT_LOWLATENCY_MODE */

uint32_t
wlanoidGetIpiInfo(IN struct ADAPTER *prAdapter,
		  IN void *pvQueryBuffer,
		  IN uint32_t u4QueryBufferLen,
		  OUT uint32_t *pu4QueryInfoLen) {

	struct PARAM_GET_IPI_INFO_T *prCmdGetIpiInfo;

	*pu4QueryInfoLen = sizeof(struct PARAM_GET_IPI_INFO_T);

	if (u4QueryBufferLen < sizeof(struct PARAM_GET_IPI_INFO_T)) {
		DBGLOG(REQ, WARN, "Too short length %u\n",
		       u4QueryBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	prCmdGetIpiInfo = (struct PARAM_GET_IPI_INFO_T *)pvQueryBuffer;

	return wlanSendSetQueryCmd(prAdapter,
				CMD_ID_IPI_INFO,
				TRUE,
				FALSE,
				g_fgIsOid,
				nicCmdEventSetCommon,
				nicOidCmdTimeoutCommon,
				sizeof(struct PARAM_GET_IPI_INFO_T),
				(uint8_t *) prCmdGetIpiInfo,
				pvQueryBuffer, u4QueryBufferLen);
}

#ifdef CFG_GET_TEMPURATURE

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to get die temperature.
 *
 * \param[in] pvAdapter Pointer to the Adapter structure.
 * \param[out] pvQueryBuf A pointer to the buffer that holds the result of
 *                           the query (temperature)
 * \param[in] u4QueryBufLen The length of the query buffer (integer: 4 bytes)
 * \param[out] pu4QueryInfoLen If the call is successful, returns the number of
 *                            bytes written into the query buffer. If the call
 *                            failed due to invalid length of the query buffer,
 *                            returns the amount of storage needed.
 *
 * \retval WLAN_STATUS_SUCCESS
 * \retval WLAN_STATUS_INVALID_LENGTH
 */
/*----------------------------------------------------------------------------*/

uint32_t
wlanoidGetTemperature(IN struct ADAPTER *prAdapter,
	IN void *pvQueryBuffer,
	IN uint32_t u4QueryBufferLen,
	OUT uint32_t *pu4QueryInfoLen)
{
	struct CMD_THERMAL_SENSOR_INFO	rThermalInfo;
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;

	if (!prAdapter || !pvQueryBuffer || !pu4QueryInfoLen)
		return WLAN_STATUS_INVALID_DATA;

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in query receive error! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		*pu4QueryInfoLen = sizeof(uint32_t);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (u4QueryBufferLen < sizeof(int)) {
		DBGLOG(REQ, WARN, "Too short length %ld\n", u4QueryBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	} else if (prAdapter->fgTestMode == TRUE) {
		/*DBGLOG(REQ, WARN, "Not supported in Test Mode\n");*/
		return WLAN_STATUS_NOT_SUPPORTED;
	}
	kalMemSet(&rThermalInfo, 0,
		  sizeof(struct CMD_THERMAL_SENSOR_INFO));

	rThermalInfo.u1ThermalCtrlFormatId = THERMAL_SENSOR_INFO_GET;
	rThermalInfo.u1ActionIdx = THERMAL_SENSOR_INFO_TEMPERATURE;


	/* Not necessary to use : CMD_ID_GET_TEMPERATURE *
	 * Use new THERMAL SENSOR service instead        *
	 */
	rWlanStatus = wlanSendSetQueryExtCmd(prAdapter,
			     CMD_ID_LAYER_0_EXT_MAGIC_NUM,
			     EXT_CMD_ID_GET_SENSOR_RESULT,
			     FALSE, /* Query Bit: True->write False->read */
			     TRUE,
			     g_fgIsOid,
			     nicCmdEventGetTemperature,
			     nicOidCmdTimeoutCommon,
			     sizeof(struct CMD_THERMAL_SENSOR_INFO),
			     (uint8_t *) (&rThermalInfo),
			     pvQueryBuffer,
			     u4QueryBufferLen);

	return rWlanStatus;
}
#endif


#if CFG_SUPPORT_ANT_DIV
/*----------------------------------------------------------------------------*/
/*!
* \brief antenna diversity config
*
* \param[in] prAdapter Pointer to the Adapter structure.
* \param[in] pvQueryBuffer      Pointer to the buffer that holds the result of
*                               the query.
* \param[in] u4QueryBufferLen  The length of the query buffer.
* \param[out] pu4QueryInfoLen  If the call is successful, returns the number of
*                              bytes written into the query buffer. If the call
*                              failed due to invalid length of the query buffer
*                              returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_BUFFER_TOO_SHORT
* \retval WLAN_STATUS_NOT_SUPPORTED
* \retval WLAN_STATUS_NOT_ACCEPTED
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
uint32_t
wlanoidAntDivCfg(IN struct ADAPTER *prAdapter,
		 IN void *pvSetBuffer,
		 IN uint32_t u4SetBufferLen,
		 OUT uint32_t *pu4SetInfoLen)
{
	struct CMD_ANT_DIV_CTRL *prAntDivInfo;
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;
	u_int8_t fgSetQuery = TRUE;
	u_int8_t fgNeedResp = FALSE;

	DEBUGFUNC("wlanoidSetAntDiv");
	if (prAdapter == NULL)
		return -EFAULT;
	if (pu4SetInfoLen == NULL)
		return -EFAULT;
	if (pvSetBuffer == NULL)
		return -EFAULT;

	*pu4SetInfoLen = sizeof(struct CMD_ANT_DIV_CTRL);
	if (u4SetBufferLen < sizeof(struct CMD_ANT_DIV_CTRL))
		return WLAN_STATUS_INVALID_LENGTH;

	prAntDivInfo = (struct CMD_ANT_DIV_CTRL *) pvSetBuffer;

	/*  GET need to wait for response from FW module */
	switch (prAntDivInfo->ucAction) {
	case ANT_DIV_CMD_GET_ANT:
	case ANT_DIV_CMD_DETC:
		fgSetQuery = FALSE;
		fgNeedResp = TRUE;
	break;
	case ANT_DIV_CMD_SWH:
		fgSetQuery = TRUE;
		fgNeedResp = TRUE;
	break;
	case ANT_DIV_CMD_SET_ANT:
		fgSetQuery = TRUE;
		fgNeedResp = FALSE;
	break;
	default:
		DBGLOG(REQ, WARN, "don't support action = %d\n",
					prAntDivInfo->ucAction);
		return WLAN_STATUS_INVALID_DATA;
	break;
	}

	rWlanStatus = wlanSendSetQueryCmd(prAdapter,
					CMD_ID_ANT_DIV_CTRL,
					fgSetQuery,
					fgNeedResp,
					g_fgIsOid,
					nicCmdEventAntDiv,
					nicOidCmdTimeoutCommon,
					sizeof(struct CMD_ANT_DIV_CTRL),
					(uint8_t *) prAntDivInfo,
					pvSetBuffer, u4SetBufferLen);

	return rWlanStatus;
}
#endif
#if (CFG_SUPPORT_GET_MCS_INFO == 1)
uint32_t
wlanoidTxQueryMcsInfo(IN struct ADAPTER *prAdapter,
		 IN void *pvQueryBuffer,
		 IN uint32_t u4QueryBufferLen,
		 OUT uint32_t *pu4QueryInfoLen)
{
	struct PARAM_TX_MCS_INFO *prMcsInfo;

	DEBUGFUNC("wlanoidQueryWlanInfo");

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Adapter not ready. ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		*pu4QueryInfoLen = sizeof(uint32_t);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (u4QueryBufferLen < sizeof(int)) {
		DBGLOG(REQ, WARN, "Too short length %ld\n", u4QueryBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	} else if (prAdapter->fgTestMode == TRUE) {
		/*DBGLOG(REQ, WARN, "Not supported in Test Mode\n");*/
		return WLAN_STATUS_NOT_SUPPORTED;
	}

	if (prAdapter->prAisBssInfo->prStaRecOfAP == NULL)
		return WLAN_STATUS_FAILURE;

	prMcsInfo = (struct PARAM_TX_MCS_INFO *)pvQueryBuffer;
	prMcsInfo->ucStaIndex = prAdapter->prAisBssInfo->prStaRecOfAP->ucIndex;

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_TX_MCS_INFO,
				   FALSE,
				   TRUE,
				   g_fgIsOid,
				   nicCmdEventQueryTxMcsInfo,
				   nicOidCmdTimeoutCommon,
				   sizeof(struct PARAM_TX_MCS_INFO),
				   (uint8_t *) prMcsInfo,
				   pvQueryBuffer, u4QueryBufferLen);
}
#endif

#ifdef CFG_SUPPORT_TIME_MEASURE
uint32_t wlanoidQueryStartFtm(
			IN struct ADAPTER *prAdapter,
			IN void *pvQueryBuffer,
			IN uint32_t u4QueryBufferLen,
			OUT uint32_t *pu4QueryInfoLen
			)
{
	struct CMD_TM_ACTION_T rCmdTmAction;
	struct PARAM_TM_T *prTmrParam;
	uint8_t fgNeedResp = FALSE;

	DEBUGFUNC("wlanoidQueryStartFtm");

	if (prAdapter == NULL)
		return -EFAULT;
	if (pu4QueryInfoLen == NULL)
		return -EFAULT;
	if ((u4QueryBufferLen > 0) && (pvQueryBuffer == NULL))
		return -EFAULT;

	*pu4QueryInfoLen = sizeof(struct CMD_TM_ACTION_T);
	prTmrParam = (struct PARAM_TM_T *)pvQueryBuffer;

	rCmdTmAction.ucTmCategory = TM_ACTION_START_FTM;
	rCmdTmAction.ucCmdVer = TM_CMD_EVENT_VER;
	rCmdTmAction.u2CmdLen = CMD_TM_ACTION_START_FTM_LEN;
	if (prTmrParam->ucFTMNum != 0 && prTmrParam->ucMinDeltaIn100US != 0 &&
					prTmrParam->ucFTMBandwidth != 0) {
		COPY_MAC_ADDR(rCmdTmAction.aucRttPeerAddr,
						prTmrParam->aucRttPeerAddr);
		rCmdTmAction.ucFTMNum = prTmrParam->ucFTMNum;
		rCmdTmAction.ucMinDeltaIn100US = prTmrParam->ucMinDeltaIn100US;
		rCmdTmAction.ucFTMBandwidth = prTmrParam->ucFTMBandwidth;
		fgNeedResp = (prTmrParam->u4DistanceCm == 0);
	}

	return wlanSendSetQueryCmd(prAdapter,
				CMD_ID_TMR_ACTION,
				TRUE,
				fgNeedResp,
				TRUE,
				nicCmdEventGetTmReport,
				nicOidCmdTimeoutCommon,
				sizeof(struct CMD_TM_ACTION_T),
				(uint8_t *)&rCmdTmAction,
				pvQueryBuffer, u4QueryBufferLen);
}

uint32_t wlanoidQueryFtm(
			IN struct ADAPTER *prAdapter,
			IN void *pvQueryBuffer,
			IN uint32_t u4QueryBufferLen,
			OUT uint32_t *pu4QueryInfoLen
			)
{
	struct CMD_TM_ACTION_T rCmdTmAction;
	struct PARAM_TM_T *prTmrParam;
	struct timespec Ftmtv_raw;

	DEBUGFUNC("wlanoidQueryStartFtm");

	if (prAdapter == NULL)
		return -EFAULT;
	if (pu4QueryInfoLen == NULL)
		return -EFAULT;
	if ((u4QueryBufferLen > 0) && (pvQueryBuffer == NULL))
		return -EFAULT;

	*pu4QueryInfoLen = sizeof(struct CMD_TM_ACTION_T);
	prTmrParam = (struct PARAM_TM_T *)pvQueryBuffer;

	rCmdTmAction.ucTmCategory = prTmrParam->ucTmCategory;
	rCmdTmAction.ucCmdVer = TM_CMD_EVENT_VER;
	rCmdTmAction.u2CmdLen = CMD_TM_ACTION_QUERY_LEN;

	getrawmonotonic(&Ftmtv_raw);
	/* gpio_set_value(A1, 1); */
	/* gpio_set_value(A1, 0); */
	g_u8LastSysClkps = g_u8SysClkps;
	g_u8SysClkps = (uint64_t)(Ftmtv_raw.tv_sec * 1000000000LL
						+ Ftmtv_raw.tv_nsec) * 1000;

	return wlanSendSetQueryCmd(prAdapter,
				CMD_ID_TMR_ACTION,
				TRUE,
				TRUE,
				TRUE,
				nicCmdEventGetTmReport,
				nicOidCmdTimeoutCommon,
				sizeof(struct CMD_TM_ACTION_T),
				(uint8_t *)&rCmdTmAction,
				pvQueryBuffer, u4QueryBufferLen);
}

uint32_t wlanoidSetEnableTmr(
			IN struct ADAPTER *prAdapter,
			IN void *pvSetBuffer,
			IN uint32_t u4SetBufferLen,
			OUT uint32_t *pu4SetInfoLen
			)
{
	struct CMD_TM_ACTION_T rCmdTmAction;
	struct PARAM_TM_T *prTmrParam;

	DEBUGFUNC("wlanoidSetEnableTmr");

	if (prAdapter == NULL)
		return -EFAULT;
	if (pu4SetInfoLen == NULL)
		return -EFAULT;
	if ((u4SetBufferLen > 0) && (pvSetBuffer == NULL))
		return -EFAULT;

	*pu4SetInfoLen = sizeof(struct CMD_TM_ACTION_T);
	prTmrParam = (struct PARAM_TM_T *)pvSetBuffer;

	rCmdTmAction.ucTmCategory = TM_ACTION_TMR_ENABLE;
	rCmdTmAction.ucCmdVer = TM_CMD_EVENT_VER;
	rCmdTmAction.u2CmdLen = CMD_TM_ACTION_START_FTM_LEN;
	rCmdTmAction.fgFtmEnable = prTmrParam->fgFtmEnable;

	return wlanSendSetQueryCmd(prAdapter,
				CMD_ID_TMR_ACTION,
				TRUE,
				FALSE,
				TRUE,
				nicCmdEventGetTmReport,
				nicOidCmdTimeoutCommon,
				sizeof(struct CMD_TM_ACTION_T),
				(uint8_t *)&rCmdTmAction,
				pvSetBuffer, u4SetBufferLen);
}
#endif
