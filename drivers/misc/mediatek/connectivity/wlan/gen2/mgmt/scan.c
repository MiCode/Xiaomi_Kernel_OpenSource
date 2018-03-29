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

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#define REPLICATED_BEACON_TIME_THRESHOLD        (3000)
#define REPLICATED_BEACON_FRESH_PERIOD          (10000)
#define REPLICATED_BEACON_STRENGTH_THRESHOLD    (32)

#define ROAMING_NO_SWING_RCPI_STEP              (10)

#define RSSI_HI_5GHZ	(-60)
#define RSSI_MED_5GHZ	(-70)
#define RSSI_LO_5GHZ	(-80)

#define PREF_HI_5GHZ	(20)
#define PREF_MED_5GHZ	(15)
#define PREF_LO_5GHZ	(10)

INT_32 rssiRangeHi = RSSI_HI_5GHZ;
INT_32 rssiRangeMed = RSSI_MED_5GHZ;
INT_32 rssiRangeLo = RSSI_LO_5GHZ;
UINT_8 pref5GhzHi = PREF_HI_5GHZ;
UINT_8 pref5GhzMed = PREF_MED_5GHZ;
UINT_8 pref5GhzLo = PREF_LO_5GHZ;

/*
definition for AP selection algrithm
*/
#define BSS_FULL_SCORE			100
#define CHNL_BSS_NUM_THRESOLD	100
#define BSS_STA_CNT_THRESOLD	30
#define SCORE_PER_AP			1
#define ROAMING_NO_SWING_SCORE_STEP 10
#define HARD_TO_CONNECT_RSSI_THRESOLD -80

#define WEIGHT_IDX_CHNL_UTIL		2
#define WEIGHT_IDX_SNR			3
#define WEIGHT_IDX_RSSI			4
#define WEIGHT_IDX_SCN_MISS_CNT	2
#define WEIGHT_IDX_PROBE_RSP		1
#define WEIGHT_IDX_CLIENT_CNT	3
#define WEIGHT_IDX_AP_NUM		2
#define WEIGHT_IDX_5G_BAND		2
#define WEIGHT_IDX_BAND_WIDTH	1
#define WEIGHT_IDX_STBC			1
#define WEIGHT_IDX_DEAUTH_LAST	1
#define WEIGHT_IDX_BLACK_LIST	2

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
* @brief This function is used by SCN to initialize its variables
*
* @param (none)
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID scnInit(IN P_ADAPTER_T prAdapter)
{
	P_SCAN_INFO_T prScanInfo;
	P_BSS_DESC_T prBSSDesc;
	P_ROAM_BSS_DESC_T prRoamBSSDesc;
	PUINT_8 pucBSSBuff;
	PUINT_8 pucRoamBSSBuff;
	UINT_32 i;

	ASSERT(prAdapter);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	pucBSSBuff = &prScanInfo->aucScanBuffer[0];
	pucRoamBSSBuff = &prScanInfo->aucScanRoamBuffer[0];

	DBGLOG(SCN, INFO, "->scnInit()\n");

	/* 4 <1> Reset STATE and Message List */
	prScanInfo->eCurrentState = SCAN_STATE_IDLE;

	prScanInfo->rLastScanCompletedTime = (OS_SYSTIME) 0;

	LINK_INITIALIZE(&prScanInfo->rPendingMsgList);

	/* 4 <2> Reset link list of BSS_DESC_T */
	kalMemZero((PVOID) pucBSSBuff, SCN_MAX_BUFFER_SIZE);
	kalMemZero((PVOID) pucRoamBSSBuff, SCN_ROAM_MAX_BUFFER_SIZE);

	LINK_INITIALIZE(&prScanInfo->rFreeBSSDescList);
	LINK_INITIALIZE(&prScanInfo->rBSSDescList);
	LINK_INITIALIZE(&prScanInfo->rRoamFreeBSSDescList);
	LINK_INITIALIZE(&prScanInfo->rRoamBSSDescList);

	for (i = 0; i < CFG_MAX_NUM_BSS_LIST; i++) {

		prBSSDesc = (P_BSS_DESC_T) pucBSSBuff;

		LINK_INSERT_TAIL(&prScanInfo->rFreeBSSDescList, &prBSSDesc->rLinkEntry);

		pucBSSBuff += ALIGN_4(sizeof(BSS_DESC_T));
	}
	/* Check if the memory allocation consist with this initialization function */
	ASSERT(((ULONG) pucBSSBuff - (ULONG)&prScanInfo->aucScanBuffer[0]) == SCN_MAX_BUFFER_SIZE);

	for (i = 0; i < CFG_MAX_NUM_ROAM_BSS_LIST; i++) {
		prRoamBSSDesc = (P_ROAM_BSS_DESC_T) pucRoamBSSBuff;

		LINK_INSERT_TAIL(&prScanInfo->rRoamFreeBSSDescList, &prRoamBSSDesc->rLinkEntry);

		pucRoamBSSBuff += ALIGN_4(sizeof(ROAM_BSS_DESC_T));
	}
	ASSERT(((ULONG) pucRoamBSSBuff - (ULONG) & prScanInfo->aucScanRoamBuffer[0]) == SCN_ROAM_MAX_BUFFER_SIZE);

	/* reset freest channel information */
	prScanInfo->fgIsSparseChannelValid = FALSE;

	/* reset NLO state */
	prScanInfo->fgNloScanning = FALSE;
#if CFG_SUPPORT_SCN_PSCN
	prScanInfo->fgPscnOngoing = FALSE;
	prScanInfo->fgGScnConfigSet = FALSE;
	prScanInfo->fgGScnParamSet = FALSE;
	prScanInfo->prPscnParam = kalMemAlloc(sizeof(CMD_SET_PSCAN_PARAM), VIR_MEM_TYPE);
	if (prScanInfo->prPscnParam)
		kalMemZero(prScanInfo->prPscnParam, sizeof(CMD_SET_PSCAN_PARAM));

	prScanInfo->eCurrentPSCNState = PSCN_IDLE;
#endif

#if CFG_SUPPORT_GSCN
	prScanInfo->prGscnFullResult = kalMemAlloc(offsetof(PARAM_WIFI_GSCAN_FULL_RESULT, ie_data)
			+ CFG_IE_BUFFER_SIZE, VIR_MEM_TYPE);
	if (prScanInfo->prGscnFullResult)
		kalMemZero(prScanInfo->prGscnFullResult,
			offsetof(PARAM_WIFI_GSCAN_FULL_RESULT, ie_data) + CFG_IE_BUFFER_SIZE);
#endif

	prScanInfo->u4ScanUpdateIdx = 0;
}				/* end of scnInit() */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is used by SCN to uninitialize its variables
*
* @param (none)
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID scnUninit(IN P_ADAPTER_T prAdapter)
{
	P_SCAN_INFO_T prScanInfo;

	ASSERT(prAdapter);
	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);

	DBGLOG(SCN, INFO, "->scnUninit()\n");

	/* 4 <1> Reset STATE and Message List */
	prScanInfo->eCurrentState = SCAN_STATE_IDLE;

	prScanInfo->rLastScanCompletedTime = (OS_SYSTIME) 0;

	/* NOTE(Kevin): Check rPendingMsgList ? */

	/* 4 <2> Reset link list of BSS_DESC_T */
	LINK_INITIALIZE(&prScanInfo->rFreeBSSDescList);
	LINK_INITIALIZE(&prScanInfo->rBSSDescList);
	LINK_INITIALIZE(&prScanInfo->rRoamFreeBSSDescList);
	LINK_INITIALIZE(&prScanInfo->rRoamBSSDescList);
#if CFG_SUPPORT_SCN_PSCN
	kalMemFree(prScanInfo->prPscnParam, VIR_MEM_TYPE, sizeof(CMD_SET_PSCAN_PARAM));

	prScanInfo->eCurrentPSCNState = PSCN_IDLE;
#endif

#if CFG_SUPPORT_GSCN
	kalMemFree(prScanInfo->prGscnFullResult, VIR_MEM_TYPE,
		offsetof(PARAM_WIFI_GSCAN_FULL_RESULT, ie_data) + CFG_IE_BUFFER_SIZE);
#endif

}				/* end of scnUninit() */

/*----------------------------------------------------------------------------*/
/*!
* @brief Find the corresponding BSS Descriptor according to given BSSID
*
* @param[in] prAdapter          Pointer to the Adapter structure.
* @param[in] aucBSSID           Given BSSID.
*
* @return   Pointer to BSS Descriptor, if found. NULL, if not found
*/
/*----------------------------------------------------------------------------*/
P_BSS_DESC_T scanSearchBssDescByBssid(IN P_ADAPTER_T prAdapter, IN UINT_8 aucBSSID[])
{
	return scanSearchBssDescByBssidAndSsid(prAdapter, aucBSSID, FALSE, NULL);
}

/*----------------------------------------------------------------------------*/
/*!
* @brief Find the corresponding BSS Descriptor according to given BSSID
*
* @param[in] prAdapter          Pointer to the Adapter structure.
* @param[in] aucBSSID           Given BSSID.
* @param[in] fgCheckSsid        Need to check SSID or not. (for multiple SSID with single BSSID cases)
* @param[in] prSsid             Specified SSID
*
* @return   Pointer to BSS Descriptor, if found. NULL, if not found
*/
/*----------------------------------------------------------------------------*/
P_BSS_DESC_T
scanSearchBssDescByBssidAndSsid(IN P_ADAPTER_T prAdapter,
				IN UINT_8 aucBSSID[], IN BOOLEAN fgCheckSsid, IN P_PARAM_SSID_T prSsid)
{
	P_SCAN_INFO_T prScanInfo;
	P_LINK_T prBSSDescList;
	P_BSS_DESC_T prBssDesc;
	P_BSS_DESC_T prDstBssDesc = (P_BSS_DESC_T) NULL;

	ASSERT(prAdapter);
	ASSERT(aucBSSID);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);

	prBSSDescList = &prScanInfo->rBSSDescList;

	/* Search BSS Desc from current SCAN result list. */
	LINK_FOR_EACH_ENTRY(prBssDesc, prBSSDescList, rLinkEntry, BSS_DESC_T) {

		if (EQUAL_MAC_ADDR(prBssDesc->aucBSSID, aucBSSID)) {
			if (fgCheckSsid == FALSE || prSsid == NULL)
				return prBssDesc;

			if (EQUAL_SSID(prBssDesc->aucSSID,
				       prBssDesc->ucSSIDLen, prSsid->aucSsid, prSsid->u4SsidLen)) {
				return prBssDesc;
			} else if (prDstBssDesc == NULL && prBssDesc->fgIsHiddenSSID == TRUE) {
				prDstBssDesc = prBssDesc;
			} else {
				/* 20120206 frog: Equal BSSID but not SSID, SSID not hidden,
				 * SSID must be updated. */
				COPY_SSID(prBssDesc->aucSSID,
					  prBssDesc->ucSSIDLen, prSsid->aucSsid, prSsid->u4SsidLen);
				return prBssDesc;
			}
		}
	}

	return prDstBssDesc;

}				/* end of scanSearchBssDescByBssid() */

/*----------------------------------------------------------------------------*/
/*!
* @brief Find the corresponding BSS Descriptor according to given Transmitter Address.
*
* @param[in] prAdapter          Pointer to the Adapter structure.
* @param[in] aucSrcAddr         Given Source Address(TA).
*
* @return   Pointer to BSS Descriptor, if found. NULL, if not found
*/
/*----------------------------------------------------------------------------*/
P_BSS_DESC_T scanSearchBssDescByTA(IN P_ADAPTER_T prAdapter, IN UINT_8 aucSrcAddr[])
{
	return scanSearchBssDescByTAAndSsid(prAdapter, aucSrcAddr, FALSE, NULL);
}

/*----------------------------------------------------------------------------*/
/*!
* @brief Find the corresponding BSS Descriptor according to given Transmitter Address.
*
* @param[in] prAdapter          Pointer to the Adapter structure.
* @param[in] aucSrcAddr         Given Source Address(TA).
* @param[in] fgCheckSsid        Need to check SSID or not. (for multiple SSID with single BSSID cases)
* @param[in] prSsid             Specified SSID
*
* @return   Pointer to BSS Descriptor, if found. NULL, if not found
*/
/*----------------------------------------------------------------------------*/
P_BSS_DESC_T
scanSearchBssDescByTAAndSsid(IN P_ADAPTER_T prAdapter,
			     IN UINT_8 aucSrcAddr[], IN BOOLEAN fgCheckSsid, IN P_PARAM_SSID_T prSsid)
{
	P_SCAN_INFO_T prScanInfo;
	P_LINK_T prBSSDescList;
	P_BSS_DESC_T prBssDesc;
	P_BSS_DESC_T prDstBssDesc = (P_BSS_DESC_T) NULL;

	ASSERT(prAdapter);
	ASSERT(aucSrcAddr);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);

	prBSSDescList = &prScanInfo->rBSSDescList;

	/* Search BSS Desc from current SCAN result list. */
	LINK_FOR_EACH_ENTRY(prBssDesc, prBSSDescList, rLinkEntry, BSS_DESC_T) {

		if (EQUAL_MAC_ADDR(prBssDesc->aucSrcAddr, aucSrcAddr)) {
			if (fgCheckSsid == FALSE || prSsid == NULL)
				return prBssDesc;

			if (EQUAL_SSID(prBssDesc->aucSSID,
				       prBssDesc->ucSSIDLen, prSsid->aucSsid, prSsid->u4SsidLen)) {
				return prBssDesc;
			} else if (prDstBssDesc == NULL && prBssDesc->fgIsHiddenSSID == TRUE) {
				prDstBssDesc = prBssDesc;
			}

		}
	}

	return prDstBssDesc;

}				/* end of scanSearchBssDescByTA() */

#if CFG_SUPPORT_HOTSPOT_2_0
/*----------------------------------------------------------------------------*/
/*!
* @brief Find the corresponding BSS Descriptor according to given BSSID
*
* @param[in] prAdapter          Pointer to the Adapter structure.
* @param[in] aucBSSID           Given BSSID.
* @param[in] fgCheckSsid        Need to check SSID or not. (for multiple SSID with single BSSID cases)
* @param[in] prSsid             Specified SSID
*
* @return   Pointer to BSS Descriptor, if found. NULL, if not found
*/
/*----------------------------------------------------------------------------*/
P_BSS_DESC_T scanSearchBssDescByBssidAndLatestUpdateTime(IN P_ADAPTER_T prAdapter, IN UINT_8 aucBSSID[])
{
	P_SCAN_INFO_T prScanInfo;
	P_LINK_T prBSSDescList;
	P_BSS_DESC_T prBssDesc;
	P_BSS_DESC_T prDstBssDesc = (P_BSS_DESC_T) NULL;
	OS_SYSTIME rLatestUpdateTime = 0;

	ASSERT(prAdapter);
	ASSERT(aucBSSID);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);

	prBSSDescList = &prScanInfo->rBSSDescList;

	/* Search BSS Desc from current SCAN result list. */
	LINK_FOR_EACH_ENTRY(prBssDesc, prBSSDescList, rLinkEntry, BSS_DESC_T) {

		if (EQUAL_MAC_ADDR(prBssDesc->aucBSSID, aucBSSID)) {
			if (!rLatestUpdateTime || CHECK_FOR_EXPIRATION(prBssDesc->rUpdateTime, rLatestUpdateTime)) {
				prDstBssDesc = prBssDesc;
				COPY_SYSTIME(rLatestUpdateTime, prBssDesc->rUpdateTime);
			}
		}
	}

	return prDstBssDesc;

}				/* end of scanSearchBssDescByBssid() */
#endif

/*----------------------------------------------------------------------------*/
/*!
* @brief Find the corresponding BSS Descriptor according to
*        given eBSSType, BSSID and Transmitter Address
*
* @param[in] prAdapter  Pointer to the Adapter structure.
* @param[in] eBSSType   BSS Type of incoming Beacon/ProbeResp frame.
* @param[in] aucBSSID   Given BSSID of Beacon/ProbeResp frame.
* @param[in] aucSrcAddr Given source address (TA) of Beacon/ProbeResp frame.
*
* @return   Pointer to BSS Descriptor, if found. NULL, if not found
*/
/*----------------------------------------------------------------------------*/
P_BSS_DESC_T
scanSearchExistingBssDesc(IN P_ADAPTER_T prAdapter,
			  IN ENUM_BSS_TYPE_T eBSSType, IN UINT_8 aucBSSID[], IN UINT_8 aucSrcAddr[])
{
	return scanSearchExistingBssDescWithSsid(prAdapter, eBSSType, aucBSSID, aucSrcAddr, FALSE, NULL);
}

VOID scanRemoveRoamBssDescsByTime(IN P_ADAPTER_T prAdapter, IN UINT_32 u4RemoveTime)
{
	P_SCAN_INFO_T prScanInfo;
	P_LINK_T prRoamBSSDescList;
	P_LINK_T prRoamFreeBSSDescList;
	P_ROAM_BSS_DESC_T prRoamBssDesc;
	P_ROAM_BSS_DESC_T prRoamBSSDescNext;
	OS_SYSTIME rCurrentTime;

	ASSERT(prAdapter);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prRoamBSSDescList = &prScanInfo->rRoamBSSDescList;
	prRoamFreeBSSDescList = &prScanInfo->rRoamFreeBSSDescList;

	GET_CURRENT_SYSTIME(&rCurrentTime);

	LINK_FOR_EACH_ENTRY_SAFE(prRoamBssDesc, prRoamBSSDescNext, prRoamBSSDescList, rLinkEntry,
				 ROAM_BSS_DESC_T) {

		if (CHECK_FOR_TIMEOUT(rCurrentTime, prRoamBssDesc->rUpdateTime,
				      SEC_TO_SYSTIME(u4RemoveTime))) {

			LINK_REMOVE_KNOWN_ENTRY(prRoamBSSDescList, prRoamBssDesc);
			LINK_INSERT_TAIL(prRoamFreeBSSDescList, &prRoamBssDesc->rLinkEntry);
		}
	}
}

P_ROAM_BSS_DESC_T
scanSearchRoamBssDescBySsid(IN P_ADAPTER_T prAdapter, IN P_BSS_DESC_T prBssDesc)
{
	P_SCAN_INFO_T prScanInfo;
	P_LINK_T prRoamBSSDescList;
	P_ROAM_BSS_DESC_T prRoamBssDesc;

	ASSERT(prAdapter);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);

	prRoamBSSDescList = &prScanInfo->rRoamBSSDescList;

	/* Search BSS Desc from current SCAN result list. */
	LINK_FOR_EACH_ENTRY(prRoamBssDesc, prRoamBSSDescList, rLinkEntry, ROAM_BSS_DESC_T) {
		if (EQUAL_SSID(prRoamBssDesc->aucSSID, prRoamBssDesc->ucSSIDLen,
				       prBssDesc->aucSSID, prBssDesc->ucSSIDLen)) {
				return prRoamBssDesc;
		}
	}

	return NULL;
}

P_ROAM_BSS_DESC_T scanAllocateRoamBssDesc(IN P_ADAPTER_T prAdapter)
{
	P_SCAN_INFO_T prScanInfo;
	P_LINK_T prRoamFreeBSSDescList;
	P_ROAM_BSS_DESC_T prRoamBssDesc = NULL;

	ASSERT(prAdapter);
	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);

	prRoamFreeBSSDescList = &prScanInfo->rRoamFreeBSSDescList;

	LINK_REMOVE_HEAD(prRoamFreeBSSDescList, prRoamBssDesc, P_ROAM_BSS_DESC_T);

	if (prRoamBssDesc) {
		P_LINK_T prRoamBSSDescList;

		kalMemZero(prRoamBssDesc, sizeof(ROAM_BSS_DESC_T));

		prRoamBSSDescList = &prScanInfo->rRoamBSSDescList;

		LINK_INSERT_HEAD(prRoamBSSDescList, &prRoamBssDesc->rLinkEntry);
	}

	return prRoamBssDesc;
}

VOID scanAddToRoamBssDesc(IN P_ADAPTER_T prAdapter, IN P_BSS_DESC_T prBssDesc)
{
	P_ROAM_BSS_DESC_T prRoamBssDesc;

	prRoamBssDesc = scanSearchRoamBssDescBySsid(prAdapter, prBssDesc);

	if (prRoamBssDesc == NULL) {
		UINT_32 u4RemoveTime = REMOVE_TIMEOUT_TWO_DAY;

		do {
			prRoamBssDesc = scanAllocateRoamBssDesc(prAdapter);
			if (prRoamBssDesc)
				break;
			scanRemoveRoamBssDescsByTime(prAdapter, u4RemoveTime);
			u4RemoveTime = u4RemoveTime / 2;
		} while (u4RemoveTime > 0);

		if (prRoamBssDesc != NULL) {
			COPY_SSID(prRoamBssDesc->aucSSID, prRoamBssDesc->ucSSIDLen,
				prBssDesc->aucSSID, prBssDesc->ucSSIDLen);
		}
	}

	GET_CURRENT_SYSTIME(&prRoamBssDesc->rUpdateTime);
}

VOID scanSearchBssDescOfRoamSsid(IN P_ADAPTER_T prAdapter)
{
#define SSID_ONLY_EXIST_ONE_AP      1    /* If only exist one same ssid AP, avoid unnecessary scan */

	P_SCAN_INFO_T prScanInfo;
	P_LINK_T prBSSDescList;
	P_BSS_DESC_T prBssDesc;
	P_BSS_INFO_T prAisBssInfo;
	UINT_32	u4SameSSIDCount = 0;

	prAisBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);
	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prBSSDescList = &prScanInfo->rBSSDescList;

	if (prAisBssInfo->eConnectionState != PARAM_MEDIA_STATE_CONNECTED)
		return;

	LINK_FOR_EACH_ENTRY(prBssDesc, prBSSDescList, rLinkEntry, BSS_DESC_T) {
		if (EQUAL_SSID(prBssDesc->aucSSID, prBssDesc->ucSSIDLen,
				       prAisBssInfo->aucSSID, prAisBssInfo->ucSSIDLen)) {
			u4SameSSIDCount++;
			if (u4SameSSIDCount > SSID_ONLY_EXIST_ONE_AP) {
				scanAddToRoamBssDesc(prAdapter, prBssDesc);
				break;
			}
		}
	}
}

/*----------------------------------------------------------------------------*/
/*!
* @brief Find the corresponding BSS Descriptor according to
*        given eBSSType, BSSID and Transmitter Address
*
* @param[in] prAdapter  Pointer to the Adapter structure.
* @param[in] eBSSType   BSS Type of incoming Beacon/ProbeResp frame.
* @param[in] aucBSSID   Given BSSID of Beacon/ProbeResp frame.
* @param[in] aucSrcAddr Given source address (TA) of Beacon/ProbeResp frame.
* @param[in] fgCheckSsid Need to check SSID or not. (for multiple SSID with single BSSID cases)
* @param[in] prSsid     Specified SSID
*
* @return   Pointer to BSS Descriptor, if found. NULL, if not found
*/
/*----------------------------------------------------------------------------*/
P_BSS_DESC_T
scanSearchExistingBssDescWithSsid(IN P_ADAPTER_T prAdapter,
				  IN ENUM_BSS_TYPE_T eBSSType,
				  IN UINT_8 aucBSSID[],
				  IN UINT_8 aucSrcAddr[], IN BOOLEAN fgCheckSsid, IN P_PARAM_SSID_T prSsid)
{
	P_SCAN_INFO_T prScanInfo;
	P_BSS_DESC_T prBssDesc, prIBSSBssDesc;
	P_LINK_T prBSSDescList;
	P_LINK_T prFreeBSSDescList;


	ASSERT(prAdapter);
	ASSERT(aucSrcAddr);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);

	if ((eBSSType == BSS_TYPE_P2P_DEVICE) || (eBSSType == BSS_TYPE_INFRASTRUCTURE) ||
			(eBSSType == BSS_TYPE_BOW_DEVICE)) {

		if (eBSSType == BSS_TYPE_P2P_DEVICE)
			fgCheckSsid = FALSE;

		if (eBSSType != BSS_TYPE_BOW_DEVICE)
			scanSearchBssDescOfRoamSsid(prAdapter);

		prBssDesc = scanSearchBssDescByBssidAndSsid(prAdapter, aucBSSID, fgCheckSsid, prSsid);

		return prBssDesc;

	} else if (eBSSType == BSS_TYPE_IBSS) {
		prIBSSBssDesc = scanSearchBssDescByBssidAndSsid(prAdapter, aucBSSID, fgCheckSsid, prSsid);
		prBssDesc = scanSearchBssDescByTAAndSsid(prAdapter, aucSrcAddr, fgCheckSsid, prSsid);

		/* NOTE(Kevin):
			 * Rules to maintain the SCAN Result:
			 * For AdHoc -
			 *    CASE I    We have TA1(BSSID1), but it change its BSSID to BSSID2
			 *              -> Update TA1 entry's BSSID.
			 *    CASE II   We have TA1(BSSID1), and get TA1(BSSID1) again
			 *              -> Update TA1 entry's contain.
			 *    CASE III  We have a SCAN result TA1(BSSID1), and TA2(BSSID2). Sooner or
			 *               later, TA2 merge into TA1, we get TA2(BSSID1)
			 *              -> Remove TA2 first and then replace TA1 entry's TA with TA2,
			 *                 Still have only one entry of BSSID.
			 *    CASE IV   We have a SCAN result TA1(BSSID1), and another TA2 also merge into BSSID1.
			 *              -> Replace TA1 entry's TA with TA2, Still have only one entry.
			 *    CASE V    New IBSS
			 *              -> Add this one to SCAN result.
			 */
		if (prBssDesc) {
			if ((!prIBSSBssDesc) ||	/* CASE I */
				(prBssDesc == prIBSSBssDesc)) {	/* CASE II */

				return prBssDesc;
			}	/* CASE III */

			prBSSDescList = &prScanInfo->rBSSDescList;
			prFreeBSSDescList = &prScanInfo->rFreeBSSDescList;

			/* Remove this BSS Desc from the BSS Desc list */
			LINK_REMOVE_KNOWN_ENTRY(prBSSDescList, prBssDesc);

			/* Return this BSS Desc to the free BSS Desc list. */
			LINK_INSERT_TAIL(prFreeBSSDescList, &prBssDesc->rLinkEntry);

			return prIBSSBssDesc;
		}

		if (prIBSSBssDesc) {	/* CASE IV */
			return prIBSSBssDesc;
		}
			/* reture NULL CASE V */
	}

	return (P_BSS_DESC_T) NULL;
}				/* end of scanSearchExistingBssDesc() */

/*----------------------------------------------------------------------------*/
/*!
* @brief Delete BSS Descriptors from current list according to given Remove Policy.
*
* @param[in] u4RemovePolicy     Remove Policy.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID scanRemoveBssDescsByPolicy(IN P_ADAPTER_T prAdapter, IN UINT_32 u4RemovePolicy)
{
	P_CONNECTION_SETTINGS_T prConnSettings;
	P_SCAN_INFO_T prScanInfo;
	P_LINK_T prBSSDescList;
	P_LINK_T prFreeBSSDescList;
	P_BSS_DESC_T prBssDesc;
	P_LINK_T prEssList = &prAdapter->rWifiVar.rAisSpecificBssInfo.rCurEssLink;

	ASSERT(prAdapter);

	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);
	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prBSSDescList = &prScanInfo->rBSSDescList;
	prFreeBSSDescList = &prScanInfo->rFreeBSSDescList;

	/* DBGLOG(SCN, TRACE, ("Before Remove - Number Of SCAN Result = %ld\n", */
	/* prBSSDescList->u4NumElem)); */

	if (u4RemovePolicy & SCN_RM_POLICY_TIMEOUT) {
		P_BSS_DESC_T prBSSDescNext;
		OS_SYSTIME rCurrentTime;

		GET_CURRENT_SYSTIME(&rCurrentTime);

		/* Search BSS Desc from current SCAN result list. */
		LINK_FOR_EACH_ENTRY_SAFE(prBssDesc, prBSSDescNext, prBSSDescList, rLinkEntry, BSS_DESC_T) {

			if ((u4RemovePolicy & SCN_RM_POLICY_EXCLUDE_CONNECTED) &&
			    (prBssDesc->fgIsConnected || prBssDesc->fgIsConnecting)) {
				/* Don't remove the one currently we are connected. */
				continue;
			}

			if (CHECK_FOR_TIMEOUT(rCurrentTime, prBssDesc->rUpdateTime,
					      SEC_TO_SYSTIME(SCN_BSS_DESC_REMOVE_TIMEOUT_SEC))) {

				/* DBGLOG(SCN, TRACE, ("Remove TIMEOUT BSS DESC(%#x):
				 * MAC: %pM, Current Time = %08lx, Update Time = %08lx\n", */
				/* prBssDesc, prBssDesc->aucBSSID, rCurrentTime, prBssDesc->rUpdateTime)); */
				if (!prBssDesc->prBlack)
					aisQueryBlackList(prAdapter, prBssDesc);
				if (prBssDesc->prBlack)
					prBssDesc->prBlack->u4DisapperTime = (UINT_32)kalGetBootTime();

				/* Remove this BSS Desc from the BSS Desc list */
				LINK_REMOVE_KNOWN_ENTRY(prBSSDescList, prBssDesc);
				/* Remove this BSS Desc from the Ess Desc List */
				if (LINK_ENTRY_IS_VALID(&prBssDesc->rLinkEntryEss))
					LINK_REMOVE_KNOWN_ENTRY(prEssList, &prBssDesc->rLinkEntryEss);

				/* Return this BSS Desc to the free BSS Desc list. */
				LINK_INSERT_TAIL(prFreeBSSDescList, &prBssDesc->rLinkEntry);
			}
		}
	} else if (u4RemovePolicy & SCN_RM_POLICY_OLDEST_HIDDEN) {
		P_BSS_DESC_T prBssDescOldest = (P_BSS_DESC_T) NULL;

		/* Search BSS Desc from current SCAN result list. */
		LINK_FOR_EACH_ENTRY(prBssDesc, prBSSDescList, rLinkEntry, BSS_DESC_T) {

			if ((u4RemovePolicy & SCN_RM_POLICY_EXCLUDE_CONNECTED) &&
			    (prBssDesc->fgIsConnected || prBssDesc->fgIsConnecting)) {
				/* Don't remove the one currently we are connected. */
				continue;
			}

			if (!prBssDesc->fgIsHiddenSSID)
				continue;

			if (!prBssDescOldest) {	/* 1st element */
				prBssDescOldest = prBssDesc;
				continue;
			}

			if (TIME_BEFORE(prBssDesc->rUpdateTime, prBssDescOldest->rUpdateTime))
				prBssDescOldest = prBssDesc;
		}

		if (prBssDescOldest) {

			/* DBGLOG(SCN, TRACE, ("Remove OLDEST HIDDEN BSS DESC(%#x):
			 * MAC: %pM, Update Time = %08lx\n", */
			/* prBssDescOldest, prBssDescOldest->aucBSSID, prBssDescOldest->rUpdateTime)); */

			/* Remove this BSS Desc from the BSS Desc list */
			LINK_REMOVE_KNOWN_ENTRY(prBSSDescList, prBssDescOldest);

			/* Return this BSS Desc to the free BSS Desc list. */
			LINK_INSERT_TAIL(prFreeBSSDescList, &prBssDescOldest->rLinkEntry);
		}
	} else if (u4RemovePolicy & SCN_RM_POLICY_SMART_WEAKEST) {
		P_BSS_DESC_T prBssDescWeakest = (P_BSS_DESC_T) NULL;
		P_BSS_DESC_T prBssDescWeakestSameSSID = (P_BSS_DESC_T) NULL;
		UINT_32 u4SameSSIDCount = 0;

		/* Search BSS Desc from current SCAN result list. */
		LINK_FOR_EACH_ENTRY(prBssDesc, prBSSDescList, rLinkEntry, BSS_DESC_T) {

			if ((u4RemovePolicy & SCN_RM_POLICY_EXCLUDE_CONNECTED) &&
			    (prBssDesc->fgIsConnected || prBssDesc->fgIsConnecting)) {
				/* Don't remove the one currently we are connected. */
				continue;
			}

			if ((!prBssDesc->fgIsHiddenSSID) &&
			    (EQUAL_SSID(prBssDesc->aucSSID,
					prBssDesc->ucSSIDLen, prConnSettings->aucSSID, prConnSettings->ucSSIDLen))) {
				u4SameSSIDCount++;

				if (!prBssDescWeakestSameSSID)
					prBssDescWeakestSameSSID = prBssDesc;
				else if (prBssDesc->ucRCPI < prBssDescWeakestSameSSID->ucRCPI)
					prBssDescWeakestSameSSID = prBssDesc;
				if (u4SameSSIDCount < SCN_BSS_DESC_SAME_SSID_THRESHOLD)
					continue;
			}

			if (!prBssDescWeakest) {	/* 1st element */
				prBssDescWeakest = prBssDesc;
				continue;
			}

			if (prBssDesc->ucRCPI < prBssDescWeakest->ucRCPI)
				prBssDescWeakest = prBssDesc;

		}
		if ((u4SameSSIDCount >= SCN_BSS_DESC_SAME_SSID_THRESHOLD) && (prBssDescWeakestSameSSID))
			prBssDescWeakest = prBssDescWeakestSameSSID;

		if (prBssDescWeakest) {

			/* DBGLOG(SCN, TRACE, ("Remove WEAKEST BSS DESC(%#x): MAC: %pM, Update Time = %08lx\n", */
			/* prBssDescOldest, prBssDescOldest->aucBSSID, prBssDescOldest->rUpdateTime)); */
			if (!prBssDescWeakest->prBlack)
				aisQueryBlackList(prAdapter, prBssDescWeakest);
			if (prBssDescWeakest->prBlack)
				prBssDescWeakest->prBlack->u4DisapperTime = (UINT_32)kalGetBootTime();

			/* Remove this BSS Desc from the BSS Desc list */
			LINK_REMOVE_KNOWN_ENTRY(prBSSDescList, prBssDescWeakest);
			/* Remove this BSS Desc from the Ess Desc List */
			if (LINK_ENTRY_IS_VALID(&prBssDescWeakest->rLinkEntryEss))
				LINK_REMOVE_KNOWN_ENTRY(prEssList, &prBssDescWeakest->rLinkEntryEss);

			/* Return this BSS Desc to the free BSS Desc list. */
			LINK_INSERT_TAIL(prFreeBSSDescList, &prBssDescWeakest->rLinkEntry);
		}
	} else if (u4RemovePolicy & SCN_RM_POLICY_ENTIRE) {
		P_BSS_DESC_T prBSSDescNext;
		UINT_32 u4Current = (UINT_32)kalGetBootTime();

		LINK_FOR_EACH_ENTRY_SAFE(prBssDesc, prBSSDescNext, prBSSDescList, rLinkEntry, BSS_DESC_T) {

			if ((u4RemovePolicy & SCN_RM_POLICY_EXCLUDE_CONNECTED) &&
			    (prBssDesc->fgIsConnected || prBssDesc->fgIsConnecting)) {
				/* Don't remove the one currently we are connected. */
				continue;
			}
			if (!prBssDesc->prBlack)
				aisQueryBlackList(prAdapter, prBssDesc);
			if (prBssDesc->prBlack)
				prBssDesc->prBlack->u4DisapperTime = u4Current;

			/* Remove this BSS Desc from the BSS Desc list */
			LINK_REMOVE_KNOWN_ENTRY(prBSSDescList, prBssDesc);
			/* Remove this BSS Desc from the Ess Desc List */
			if (LINK_ENTRY_IS_VALID(&prBssDesc->rLinkEntryEss))
				LINK_REMOVE_KNOWN_ENTRY(prEssList, &prBssDesc->rLinkEntryEss);

			/* Return this BSS Desc to the free BSS Desc list. */
			LINK_INSERT_TAIL(prFreeBSSDescList, &prBssDesc->rLinkEntry);
		}

	}

	return;

}				/* end of scanRemoveBssDescsByPolicy() */

/*----------------------------------------------------------------------------*/
/*!
* @brief Delete BSS Descriptors from current list according to given BSSID.
*
* @param[in] prAdapter  Pointer to the Adapter structure.
* @param[in] aucBSSID   Given BSSID.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID scanRemoveBssDescByBssid(IN P_ADAPTER_T prAdapter, IN UINT_8 aucBSSID[])
{
	P_SCAN_INFO_T prScanInfo;
	P_LINK_T prBSSDescList;
	P_LINK_T prFreeBSSDescList;
	P_BSS_DESC_T prBssDesc = (P_BSS_DESC_T) NULL;
	P_BSS_DESC_T prBSSDescNext;
	P_LINK_T prEssList = NULL;

	ASSERT(prAdapter);
	ASSERT(aucBSSID);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prBSSDescList = &prScanInfo->rBSSDescList;
	prFreeBSSDescList = &prScanInfo->rFreeBSSDescList;
	prEssList = &prAdapter->rWifiVar.rAisSpecificBssInfo.rCurEssLink;

	/* Check if such BSS Descriptor exists in a valid list */
	LINK_FOR_EACH_ENTRY_SAFE(prBssDesc, prBSSDescNext, prBSSDescList, rLinkEntry, BSS_DESC_T) {

		if (EQUAL_MAC_ADDR(prBssDesc->aucBSSID, aucBSSID)) {
			if (!prBssDesc->prBlack)
				aisQueryBlackList(prAdapter, prBssDesc);
			if (prBssDesc->prBlack)
				prBssDesc->prBlack->u4DisapperTime = (UINT_32)kalGetBootTime();

			/* Remove this BSS Desc from the BSS Desc list */
			LINK_REMOVE_KNOWN_ENTRY(prBSSDescList, prBssDesc);
			/* Remove this BSS Desc from the Ess Desc List */
			if (LINK_ENTRY_IS_VALID(&prBssDesc->rLinkEntryEss))
				LINK_REMOVE_KNOWN_ENTRY(prEssList, &prBssDesc->rLinkEntryEss);

			/* Return this BSS Desc to the free BSS Desc list. */
			LINK_INSERT_TAIL(prFreeBSSDescList, &prBssDesc->rLinkEntry);

			/* BSSID is not unique, so need to traverse whols link-list */
		}
	}

}				/* end of scanRemoveBssDescByBssid() */

/*----------------------------------------------------------------------------*/
/*!
* @brief Delete BSS Descriptors from current list according to given band configuration
*
* @param[in] prAdapter  Pointer to the Adapter structure.
* @param[in] eBand      Given band
* @param[in] eNetTypeIndex  AIS - Remove IBSS/Infrastructure BSS
*                           BOW - Remove BOW BSS
*                           P2P - Remove P2P BSS
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID
scanRemoveBssDescByBandAndNetwork(IN P_ADAPTER_T prAdapter,
				  IN ENUM_BAND_T eBand, IN ENUM_NETWORK_TYPE_INDEX_T eNetTypeIndex)
{
	P_SCAN_INFO_T prScanInfo;
	P_LINK_T prBSSDescList;
	P_LINK_T prFreeBSSDescList;
	P_BSS_DESC_T prBssDesc = (P_BSS_DESC_T) NULL;
	P_BSS_DESC_T prBSSDescNext;
	BOOLEAN fgToRemove;

	ASSERT(prAdapter);
	ASSERT(eBand <= BAND_NUM);
	ASSERT(eNetTypeIndex <= NETWORK_TYPE_INDEX_NUM);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prBSSDescList = &prScanInfo->rBSSDescList;
	prFreeBSSDescList = &prScanInfo->rFreeBSSDescList;

	if (eBand == BAND_NULL)
		return;		/* no need to do anything, keep all scan result */

	/* Check if such BSS Descriptor exists in a valid list */
	LINK_FOR_EACH_ENTRY_SAFE(prBssDesc, prBSSDescNext, prBSSDescList, rLinkEntry, BSS_DESC_T) {
		fgToRemove = FALSE;

		if (prBssDesc->eBand == eBand) {
			switch (eNetTypeIndex) {
			case NETWORK_TYPE_AIS_INDEX:
				if ((prBssDesc->eBSSType == BSS_TYPE_INFRASTRUCTURE)
				    || (prBssDesc->eBSSType == BSS_TYPE_IBSS)) {
					fgToRemove = TRUE;
				}
				break;

			case NETWORK_TYPE_P2P_INDEX:
				if (prBssDesc->eBSSType == BSS_TYPE_P2P_DEVICE)
					fgToRemove = TRUE;
				break;

			case NETWORK_TYPE_BOW_INDEX:
				if (prBssDesc->eBSSType == BSS_TYPE_BOW_DEVICE)
					fgToRemove = TRUE;
				break;

			default:
				ASSERT(0);
				break;
			}
		}

		if (fgToRemove == TRUE) {
			P_LINK_T prEssList = &prAdapter->rWifiVar.rAisSpecificBssInfo.rCurEssLink;

			if (!prBssDesc->prBlack)
				aisQueryBlackList(prAdapter, prBssDesc);
			if (prBssDesc->prBlack)
				prBssDesc->prBlack->u4DisapperTime = (UINT_32)kalGetBootTime();

			/* Remove this BSS Desc from the BSS Desc list */
			LINK_REMOVE_KNOWN_ENTRY(prBSSDescList, prBssDesc);
			/* Remove this BSS Desc from the Ess Desc List */
			if (LINK_ENTRY_IS_VALID(&prBssDesc->rLinkEntryEss))
				LINK_REMOVE_KNOWN_ENTRY(prEssList, &prBssDesc->rLinkEntryEss);

			/* Return this BSS Desc to the free BSS Desc list. */
			LINK_INSERT_TAIL(prFreeBSSDescList, &prBssDesc->rLinkEntry);
		}
	}

}				/* end of scanRemoveBssDescByBand() */

/*----------------------------------------------------------------------------*/
/*!
* @brief Clear the CONNECTION FLAG of a specified BSS Descriptor.
*
* @param[in] aucBSSID   Given BSSID.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID scanRemoveConnFlagOfBssDescByBssid(IN P_ADAPTER_T prAdapter, IN UINT_8 aucBSSID[])
{
	P_SCAN_INFO_T prScanInfo;
	P_LINK_T prBSSDescList;
	P_BSS_DESC_T prBssDesc = (P_BSS_DESC_T) NULL;

	ASSERT(prAdapter);
	ASSERT(aucBSSID);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prBSSDescList = &prScanInfo->rBSSDescList;

	/* Search BSS Desc from current SCAN result list. */
	LINK_FOR_EACH_ENTRY(prBssDesc, prBSSDescList, rLinkEntry, BSS_DESC_T) {

		if (EQUAL_MAC_ADDR(prBssDesc->aucBSSID, aucBSSID)) {
			prBssDesc->fgIsConnected = FALSE;
			prBssDesc->fgIsConnecting = FALSE;

			/* BSSID is not unique, so need to traverse whols link-list */
		}
	}

	return;

}				/* end of scanRemoveConnectionFlagOfBssDescByBssid() */

/*----------------------------------------------------------------------------*/
/*!
* @brief Allocate new BSS_DESC_T
*
* @param[in] prAdapter          Pointer to the Adapter structure.
*
* @return   Pointer to BSS Descriptor, if has free space. NULL, if has no space.
*/
/*----------------------------------------------------------------------------*/
P_BSS_DESC_T scanAllocateBssDesc(IN P_ADAPTER_T prAdapter)
{
	P_SCAN_INFO_T prScanInfo;
	P_LINK_T prFreeBSSDescList;
	P_BSS_DESC_T prBssDesc;

	ASSERT(prAdapter);
	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);

	prFreeBSSDescList = &prScanInfo->rFreeBSSDescList;

	LINK_REMOVE_HEAD(prFreeBSSDescList, prBssDesc, P_BSS_DESC_T);

	if (prBssDesc) {
		P_LINK_T prBSSDescList;

		kalMemZero(prBssDesc, sizeof(BSS_DESC_T));

#if CFG_ENABLE_WIFI_DIRECT
		LINK_INITIALIZE(&(prBssDesc->rP2pDeviceList));
		prBssDesc->fgIsP2PPresent = FALSE;
#endif /* CFG_ENABLE_WIFI_DIRECT */

		prBSSDescList = &prScanInfo->rBSSDescList;

		/* NOTE(Kevin): In current design, this new empty BSS_DESC_T will be
		 * inserted to BSSDescList immediately.
		 */
		LINK_INSERT_TAIL(prBSSDescList, &prBssDesc->rLinkEntry);
	}

	return prBssDesc;

}				/* end of scanAllocateBssDesc() */

/*----------------------------------------------------------------------------*/
/*!
* @brief This API parses Beacon/ProbeResp frame and insert extracted BSS_DESC_T
*        with IEs into prAdapter->rWifiVar.rScanInfo.aucScanBuffer
*
* @param[in] prAdapter      Pointer to the Adapter structure.
* @param[in] prSwRfb        Pointer to the receiving frame buffer.
*
* @return   Pointer to BSS Descriptor
*           NULL if the Beacon/ProbeResp frame is invalid
*/
/*----------------------------------------------------------------------------*/
P_BSS_DESC_T scanAddToBssDesc(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
	P_BSS_DESC_T prBssDesc = NULL;
	UINT_16 u2CapInfo;
	ENUM_BSS_TYPE_T eBSSType = BSS_TYPE_INFRASTRUCTURE;

	PUINT_8 pucIE;
	UINT_16 u2IELength;
	UINT_16 u2Offset = 0;

	P_WLAN_BEACON_FRAME_T prWlanBeaconFrame = (P_WLAN_BEACON_FRAME_T) NULL;
	P_IE_SSID_T prIeSsid = (P_IE_SSID_T) NULL;
	P_IE_SUPPORTED_RATE_T prIeSupportedRate = (P_IE_SUPPORTED_RATE_T) NULL;
	P_IE_EXT_SUPPORTED_RATE_T prIeExtSupportedRate = (P_IE_EXT_SUPPORTED_RATE_T) NULL;
	P_HIF_RX_HEADER_T prHifRxHdr;
	UINT_8 ucHwChannelNum = 0;
	UINT_8 ucIeDsChannelNum = 0;
	UINT_8 ucIeHtChannelNum = 0;
	BOOLEAN fgIsValidSsid = FALSE, fgEscape = FALSE;
	PARAM_SSID_T rSsid;
	UINT_64 u8Timestamp;
	BOOLEAN fgIsNewBssDesc = FALSE;

	UINT_32 i;
	UINT_8 ucSSIDChar;

	UINT_8 ucOuiType;
	UINT_16 u2SubTypeVersion;

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prWlanBeaconFrame = (P_WLAN_BEACON_FRAME_T) prSwRfb->pvHeader;

	WLAN_GET_FIELD_16(&prWlanBeaconFrame->u2CapInfo, &u2CapInfo);
	WLAN_GET_FIELD_64(&prWlanBeaconFrame->au4Timestamp[0], &u8Timestamp);

	/* decide BSS type */
	switch (u2CapInfo & CAP_INFO_BSS_TYPE) {
	case CAP_INFO_ESS:
		/* It can also be Group Owner of P2P Group. */
		eBSSType = BSS_TYPE_INFRASTRUCTURE;
		break;

	case CAP_INFO_IBSS:
		eBSSType = BSS_TYPE_IBSS;
		break;
	case 0:
		/* The P2P Device shall set the ESS bit of the Capabilities field
		 * in the Probe Response fame to 0 and IBSS bit to 0. (3.1.2.1.1) */
		eBSSType = BSS_TYPE_P2P_DEVICE;
		break;

#if CFG_ENABLE_BT_OVER_WIFI
		/* @TODO: add rule to identify BOW beacons */
#endif

	default:
		 DBGLOG(SCN, ERROR, "wrong bss type %d\n", (INT_32)(u2CapInfo & CAP_INFO_BSS_TYPE));
		return NULL;
	}

	/* 4 <1.1> Pre-parse SSID IE */
	pucIE = prWlanBeaconFrame->aucInfoElem;
	u2IELength = (prSwRfb->u2PacketLen - prSwRfb->u2HeaderLen) -
	    (UINT_16) OFFSET_OF(WLAN_BEACON_FRAME_BODY_T, aucInfoElem[0]);

	if (u2IELength > CFG_IE_BUFFER_SIZE)
		u2IELength = CFG_IE_BUFFER_SIZE;
	kalMemZero(&rSsid, sizeof(rSsid));
	IE_FOR_EACH(pucIE, u2IELength, u2Offset) {
		switch (IE_ID(pucIE)) {
		case ELEM_ID_SSID:
			if (IE_LEN(pucIE) <= ELEM_MAX_LEN_SSID) {
				ucSSIDChar = '\0';

				/* D-Link DWL-900AP+ */
				if (IE_LEN(pucIE) == 0)
					fgIsValidSsid = FALSE;
				/* Cisco AP1230A - (IE_LEN(pucIE) == 1) && (SSID_IE(pucIE)->aucSSID[0] == '\0') */
				/* Linksys WRK54G/WL520g - (IE_LEN(pucIE) == n) &&
				 * (SSID_IE(pucIE)->aucSSID[0~(n-1)] == '\0') */
				else {
					for (i = 0; i < IE_LEN(pucIE); i++)
						ucSSIDChar |= SSID_IE(pucIE)->aucSSID[i];

					if (ucSSIDChar)
						fgIsValidSsid = TRUE;
				}

				/* Update SSID to BSS Descriptor only if SSID is not hidden. */
				if (fgIsValidSsid == TRUE) {
					COPY_SSID(rSsid.aucSsid,
						  rSsid.u4SsidLen, SSID_IE(pucIE)->aucSSID, SSID_IE(pucIE)->ucLength);
				}
			}
			fgEscape = TRUE;
			break;
		default:
			break;
		}

		if (fgEscape == TRUE)
			break;
	}
	if (fgIsValidSsid)
		DBGLOG(SCN, EVENT, "%s %pM channel %d\n", rSsid.aucSsid, prWlanBeaconFrame->aucBSSID,
				HIF_RX_HDR_GET_CHNL_NUM(prSwRfb->prHifRxHdr));
	else
		DBGLOG(SCN, EVENT, "hidden ssid, %pM channel %d\n", prWlanBeaconFrame->aucBSSID,
				HIF_RX_HDR_GET_CHNL_NUM(prSwRfb->prHifRxHdr));
	/* 4 <1.2> Replace existing BSS_DESC_T or allocate a new one */
	prBssDesc = scanSearchExistingBssDescWithSsid(prAdapter,
						      eBSSType,
						      (PUINT_8) prWlanBeaconFrame->aucBSSID,
						      (PUINT_8) prWlanBeaconFrame->aucSrcAddr,
						      fgIsValidSsid, fgIsValidSsid == TRUE ? &rSsid : NULL);

	if (prBssDesc == (P_BSS_DESC_T) NULL) {
		fgIsNewBssDesc = TRUE;

		do {
			/* 4 <1.2.1> First trial of allocation */
			prBssDesc = scanAllocateBssDesc(prAdapter);
			if (prBssDesc)
				break;
			/* 4 <1.2.2> Hidden is useless, remove the oldest hidden ssid. (for passive scan) */
			scanRemoveBssDescsByPolicy(prAdapter,
						   (SCN_RM_POLICY_EXCLUDE_CONNECTED |
							SCN_RM_POLICY_OLDEST_HIDDEN |
							SCN_RM_POLICY_TIMEOUT));

			/* 4 <1.2.3> Second tail of allocation */
			prBssDesc = scanAllocateBssDesc(prAdapter);
			if (prBssDesc)
				break;
			/* 4 <1.2.4> Remove the weakest one */
			/* If there are more than half of BSS which has the same ssid as connection
			 * setting, remove the weakest one from them.
			 * Else remove the weakest one.
			 */
			scanRemoveBssDescsByPolicy(prAdapter,
						   (SCN_RM_POLICY_EXCLUDE_CONNECTED | SCN_RM_POLICY_SMART_WEAKEST));

			/* 4 <1.2.5> reallocation */
			prBssDesc = scanAllocateBssDesc(prAdapter);
			if (prBssDesc)
				break;
			/* 4 <1.2.6> no space, should not happen */
			DBGLOG(SCN, ERROR, "no bss desc available after remove policy\n");
			return NULL;

		} while (FALSE);

	} else {
		OS_SYSTIME rCurrentTime;

		/* WCXRP00000091 */
		/* if the received strength is much weaker than the original one, */
		/* ignore it due to it might be received on the folding frequency */

		GET_CURRENT_SYSTIME(&rCurrentTime);

		if (prBssDesc->eBSSType != eBSSType) {
			prBssDesc->eBSSType = eBSSType;
		} else if (HIF_RX_HDR_GET_CHNL_NUM(prSwRfb->prHifRxHdr) != prBssDesc->ucChannelNum &&
			   prBssDesc->ucRCPI > prSwRfb->prHifRxHdr->ucRcpi) {
			/* for signal strength is too much weaker and previous beacon is not stale */
			if ((prBssDesc->ucRCPI - prSwRfb->prHifRxHdr->ucRcpi) >= REPLICATED_BEACON_STRENGTH_THRESHOLD &&
				(rCurrentTime - prBssDesc->rUpdateTime) <= REPLICATED_BEACON_FRESH_PERIOD) {
				DBGLOG(SCN, EVENT, "rssi is too much weaker and previous one is fresh\n");
				return prBssDesc;
			}
			/* for received beacons too close in time domain */
			else if (rCurrentTime - prBssDesc->rUpdateTime <= REPLICATED_BEACON_TIME_THRESHOLD) {
				DBGLOG(SCN, EVENT, "receive beacon/probe reponses too close\n");
				return prBssDesc;
			}
		}

		/* if Timestamp has been reset, re-generate BSS DESC 'cause AP should have reset itself */
		if (prBssDesc->eBSSType == BSS_TYPE_INFRASTRUCTURE && u8Timestamp < prBssDesc->u8TimeStamp.QuadPart) {
			BOOLEAN fgIsConnected, fgIsConnecting;

			/* set flag for indicating this is a new BSS-DESC */
			fgIsNewBssDesc = TRUE;

			/* backup 2 flags for APs which reset timestamp unexpectedly */
			fgIsConnected = prBssDesc->fgIsConnected;
			fgIsConnecting = prBssDesc->fgIsConnecting;
			scanRemoveBssDescByBssid(prAdapter, prBssDesc->aucBSSID);

			prBssDesc = scanAllocateBssDesc(prAdapter);
			if (!prBssDesc)
				return NULL;

			/* restore */
			prBssDesc->fgIsConnected = fgIsConnected;
			prBssDesc->fgIsConnecting = fgIsConnecting;
		}
	}
#if 1

	prBssDesc->u2RawLength = prSwRfb->u2PacketLen;
	if (prBssDesc->u2RawLength > CFG_RAW_BUFFER_SIZE)
		prBssDesc->u2RawLength = CFG_RAW_BUFFER_SIZE;
	kalMemCopy(prBssDesc->aucRawBuf, prWlanBeaconFrame, prBssDesc->u2RawLength);
#endif

	/* NOTE: Keep consistency of Scan Record during JOIN process */
	if ((fgIsNewBssDesc == FALSE) && prBssDesc->fgIsConnecting) {
		DBGLOG(SCN, INFO, "we're connecting this BSS(%pM) now, don't update it\n",
				prBssDesc->aucBSSID);
		return prBssDesc;
	}
	/* 4 <2> Get information from Fixed Fields */
	prBssDesc->eBSSType = eBSSType;	/* Update the latest BSS type information. */

	COPY_MAC_ADDR(prBssDesc->aucSrcAddr, prWlanBeaconFrame->aucSrcAddr);

	COPY_MAC_ADDR(prBssDesc->aucBSSID, prWlanBeaconFrame->aucBSSID);

	prBssDesc->u8TimeStamp.QuadPart = u8Timestamp;

	WLAN_GET_FIELD_16(&prWlanBeaconFrame->u2BeaconInterval, &prBssDesc->u2BeaconInterval);

	prBssDesc->u2CapInfo = u2CapInfo;

	/* 4 <2.1> Retrieve IEs for later parsing */
	u2IELength = (prSwRfb->u2PacketLen - prSwRfb->u2HeaderLen) -
	    (UINT_16) OFFSET_OF(WLAN_BEACON_FRAME_BODY_T, aucInfoElem[0]);

	if (u2IELength > CFG_IE_BUFFER_SIZE) {
		u2IELength = CFG_IE_BUFFER_SIZE;
		prBssDesc->fgIsIEOverflow = TRUE;
	} else {
		prBssDesc->fgIsIEOverflow = FALSE;
	}
	prBssDesc->u2IELength = u2IELength;

	kalMemCopy(prBssDesc->aucIEBuf, prWlanBeaconFrame->aucInfoElem, u2IELength);

	/* 4 <2.2> reset prBssDesc variables in case that AP has been reconfigured */
	prBssDesc->fgIsERPPresent = FALSE;
	prBssDesc->fgIsHTPresent = FALSE;
	prBssDesc->eSco = CHNL_EXT_SCN;
	prBssDesc->fgIEWAPI = FALSE;
#if CFG_RSN_MIGRATION
	prBssDesc->fgIERSN = FALSE;
#endif
#if CFG_PRIVACY_MIGRATION
	prBssDesc->fgIEWPA = FALSE;
#endif
	prBssDesc->fgExsitBssLoadIE = FALSE;
	prBssDesc->fgMultiAnttenaAndSTBC = FALSE;

	/* 4 <3.1> Full IE parsing on SW_RFB_T */
	pucIE = prWlanBeaconFrame->aucInfoElem;

	IE_FOR_EACH(pucIE, u2IELength, u2Offset) {

		switch (IE_ID(pucIE)) {
		case ELEM_ID_SSID:
			if ((!prIeSsid) &&	/* NOTE(Kevin): for Atheros IOT #1 */
			    (IE_LEN(pucIE) <= ELEM_MAX_LEN_SSID)) {
				BOOLEAN fgIsHiddenSSID = FALSE;

				ucSSIDChar = '\0';

				prIeSsid = (P_IE_SSID_T) pucIE;

				/* D-Link DWL-900AP+ */
				if (IE_LEN(pucIE) == 0)
					fgIsHiddenSSID = TRUE;
				/* Cisco AP1230A - (IE_LEN(pucIE) == 1) && (SSID_IE(pucIE)->aucSSID[0] == '\0') */
				/* Linksys WRK54G/WL520g - (IE_LEN(pucIE) == n) &&
				 * (SSID_IE(pucIE)->aucSSID[0~(n-1)] == '\0') */
				else {
					for (i = 0; i < IE_LEN(pucIE); i++)
						ucSSIDChar |= SSID_IE(pucIE)->aucSSID[i];

					if (!ucSSIDChar)
						fgIsHiddenSSID = TRUE;
				}

				/* Update SSID to BSS Descriptor only if SSID is not hidden. */
				if (!fgIsHiddenSSID) {
					COPY_SSID(prBssDesc->aucSSID,
						  prBssDesc->ucSSIDLen,
						  SSID_IE(pucIE)->aucSSID, SSID_IE(pucIE)->ucLength);
				}
#if 0
				/*
				   After we connect to a hidden SSID, prBssDesc->aucSSID[] will
				   not be empty and prBssDesc->ucSSIDLen will not be 0,
				   so maybe we need to empty prBssDesc->aucSSID[] and set
				   prBssDesc->ucSSIDLen to 0 in prBssDesc to avoid that
				   UI still displays hidden SSID AP in scan list after
				   we disconnect the hidden SSID AP.
				 */
				else {
					prBssDesc->aucSSID[0] = '\0';
					prBssDesc->ucSSIDLen = 0;
				}
#endif

			}
			break;

		case ELEM_ID_SUP_RATES:
			/* NOTE(Kevin): Buffalo WHR-G54S's supported rate set IE exceed 8.
			 * IE_LEN(pucIE) == 12, "1(B), 2(B), 5.5(B), 6(B), 9(B), 11(B),
			 * 12(B), 18(B), 24(B), 36(B), 48(B), 54(B)"
			 */
			/* TP-LINK will set extra and incorrect ie with ELEM_ID_SUP_RATES */
			if ((!prIeSupportedRate) && (IE_LEN(pucIE) <= RATE_NUM))
				prIeSupportedRate = SUP_RATES_IE(pucIE);
			break;

		case ELEM_ID_DS_PARAM_SET:
			if (IE_LEN(pucIE) == ELEM_MAX_LEN_DS_PARAMETER_SET)
				ucIeDsChannelNum = DS_PARAM_IE(pucIE)->ucCurrChnl;
			break;

		case ELEM_ID_TIM:
			if (IE_LEN(pucIE) <= ELEM_MAX_LEN_TIM)
				prBssDesc->ucDTIMPeriod = TIM_IE(pucIE)->ucDTIMPeriod;
			break;

		case ELEM_ID_IBSS_PARAM_SET:
			if (IE_LEN(pucIE) == ELEM_MAX_LEN_IBSS_PARAMETER_SET)
				prBssDesc->u2ATIMWindow = IBSS_PARAM_IE(pucIE)->u2ATIMWindow;
			break;

#if 0				/* CFG_SUPPORT_802_11D */
		case ELEM_ID_COUNTRY_INFO:
			prBssDesc->prIECountry = (P_IE_COUNTRY_T) pucIE;
			break;
#endif

		case ELEM_ID_ERP_INFO:
			if (IE_LEN(pucIE) == ELEM_MAX_LEN_ERP)
				prBssDesc->fgIsERPPresent = TRUE;
			break;

		case ELEM_ID_EXTENDED_SUP_RATES:
			if (!prIeExtSupportedRate)
				prIeExtSupportedRate = EXT_SUP_RATES_IE(pucIE);
			break;

#if CFG_RSN_MIGRATION
		case ELEM_ID_RSN:
			if (rsnParseRsnIE(prAdapter, RSN_IE(pucIE), &prBssDesc->rRSNInfo)) {
				prBssDesc->fgIERSN = TRUE;
				prBssDesc->u2RsnCap = prBssDesc->rRSNInfo.u2RsnCap;
				if (prAdapter->rWifiVar.rConnSettings.eAuthMode == AUTH_MODE_WPA2)
					rsnCheckPmkidCache(prAdapter, prBssDesc);
			}
			break;
#endif

		case ELEM_ID_HT_CAP:
		{
			P_IE_HT_CAP_T prHtCap = (P_IE_HT_CAP_T)pucIE;
			UINT_8 ucSpatial = 0;
			UINT_8 i = 0;

			prBssDesc->fgIsHTPresent = TRUE;
			if (prBssDesc->fgMultiAnttenaAndSTBC)
				break;
			for (; i < 4; i++) {
				if (prHtCap->rSupMcsSet.aucRxMcsBitmask[i] > 0)
					ucSpatial++;
			}
			prBssDesc->fgMultiAnttenaAndSTBC =
				((ucSpatial > 1) && (prHtCap->u2HtCapInfo & HT_CAP_INFO_TX_STBC));
			break;
		}
		case ELEM_ID_HT_OP:
			if (IE_LEN(pucIE) != (sizeof(IE_HT_OP_T) - 2))
				break;

			if ((((P_IE_HT_OP_T) pucIE)->ucInfo1 & HT_OP_INFO1_SCO) != CHNL_EXT_RES) {
				prBssDesc->eSco = (ENUM_CHNL_EXT_T)
				    (((P_IE_HT_OP_T) pucIE)->ucInfo1 & HT_OP_INFO1_SCO);
			}
			ucIeHtChannelNum = ((P_IE_HT_OP_T) pucIE)->ucPrimaryChannel;

			break;

#if CFG_SUPPORT_WAPI
		case ELEM_ID_WAPI:
			if (wapiParseWapiIE(WAPI_IE(pucIE), &prBssDesc->rIEWAPI))
				prBssDesc->fgIEWAPI = TRUE;
			break;
#endif
		case ELEM_ID_BSS_LOAD:
		{
			struct IE_BSS_LOAD *prBssLoad = (struct IE_BSS_LOAD *)pucIE;

			prBssDesc->u2StaCnt = prBssLoad->u2StaCnt;
			prBssDesc->ucChnlUtilization = prBssLoad->ucChnlUtilizaion;
			prBssDesc->u2AvaliableAC = prBssLoad->u2AvailabeAC;
			prBssDesc->fgExsitBssLoadIE = TRUE;
			break;
		}
		case ELEM_ID_VENDOR:	/* ELEM_ID_P2P, ELEM_ID_WMM */
#if CFG_PRIVACY_MIGRATION
			if (rsnParseCheckForWFAInfoElem(prAdapter, pucIE, &ucOuiType, &u2SubTypeVersion)) {
				if ((ucOuiType == VENDOR_OUI_TYPE_WPA) && (u2SubTypeVersion == VERSION_WPA)) {

					if (rsnParseWpaIE(prAdapter, WPA_IE(pucIE), &prBssDesc->rWPAInfo))
						prBssDesc->fgIEWPA = TRUE;
				}
			}
#endif

#if CFG_ENABLE_WIFI_DIRECT
			if (prAdapter->fgIsP2PRegistered) {
				if (p2pFuncParseCheckForP2PInfoElem(prAdapter, pucIE, &ucOuiType)) {
					if (ucOuiType == VENDOR_OUI_TYPE_P2P)
						prBssDesc->fgIsP2PPresent = TRUE;
				}
			}
#endif /* CFG_ENABLE_WIFI_DIRECT */
			break;

			/* no default */
		}
	}

	/* 4 <3.2> Save information from IEs - SSID */
	/* Update Flag of Hidden SSID for used in SEARCH STATE. */

	/* NOTE(Kevin): in current driver, the ucSSIDLen == 0 represent
	 * all cases of hidden SSID.
	 * If the fgIsHiddenSSID == TRUE, it means we didn't get the ProbeResp with
	 * valid SSID.
	 */
	if (prBssDesc->ucSSIDLen == 0)
		prBssDesc->fgIsHiddenSSID = TRUE;
	else
		prBssDesc->fgIsHiddenSSID = FALSE;

	/* 4 <3.3> Check rate information in related IEs. */
	if (prIeSupportedRate || prIeExtSupportedRate) {
		rateGetRateSetFromIEs(prIeSupportedRate,
				      prIeExtSupportedRate,
				      &prBssDesc->u2OperationalRateSet,
				      &prBssDesc->u2BSSBasicRateSet, &prBssDesc->fgIsUnknownBssBasicRate);
	}
	/* 4 <4> Update information from HIF RX Header */
	{
		prHifRxHdr = prSwRfb->prHifRxHdr;

		ASSERT(prHifRxHdr);

		/* 4 <4.1> Get TSF comparison result */
		prBssDesc->fgIsLargerTSF = HIF_RX_HDR_GET_TCL_FLAG(prHifRxHdr);

		/* 4 <4.2> Get Band information */
		prBssDesc->eBand = HIF_RX_HDR_GET_RF_BAND(prHifRxHdr);

		/* 4 <4.2> Get channel and RCPI information */
		ucHwChannelNum = HIF_RX_HDR_GET_CHNL_NUM(prHifRxHdr);

		if (BAND_2G4 == prBssDesc->eBand) {

			/* Update RCPI if in right channel */
			if (ucIeDsChannelNum >= 1 && ucIeDsChannelNum <= 14) {

				/* Receive Beacon/ProbeResp frame from adjacent channel. */
				if ((ucIeDsChannelNum == ucHwChannelNum) || (prHifRxHdr->ucRcpi > prBssDesc->ucRCPI))
					prBssDesc->ucRCPI = prHifRxHdr->ucRcpi;
				/* trust channel information brought by IE */
				prBssDesc->ucChannelNum = ucIeDsChannelNum;
			} else if (ucIeHtChannelNum >= 1 && ucIeHtChannelNum <= 14) {
				/* Receive Beacon/ProbeResp frame from adjacent channel. */
				if ((ucIeHtChannelNum == ucHwChannelNum) || (prHifRxHdr->ucRcpi > prBssDesc->ucRCPI))
					prBssDesc->ucRCPI = prHifRxHdr->ucRcpi;
				/* trust channel information brought by IE */
				prBssDesc->ucChannelNum = ucIeHtChannelNum;
			} else {
				prBssDesc->ucRCPI = prHifRxHdr->ucRcpi;

				prBssDesc->ucChannelNum = ucHwChannelNum;
			}
		}
		/* 5G Band */
		else {
			if (ucIeHtChannelNum >= 1 && ucIeHtChannelNum < 200) {
				/* Receive Beacon/ProbeResp frame from adjacent channel. */
				if ((ucIeHtChannelNum == ucHwChannelNum) || (prHifRxHdr->ucRcpi > prBssDesc->ucRCPI))
					prBssDesc->ucRCPI = prHifRxHdr->ucRcpi;
				/* trust channel information brought by IE */
				prBssDesc->ucChannelNum = ucIeHtChannelNum;
			} else {
				/* Always update RCPI */
				prBssDesc->ucRCPI = prHifRxHdr->ucRcpi;

				prBssDesc->ucChannelNum = ucHwChannelNum;
			}
		}
	}

	/* 4 <5> PHY type setting */
	prBssDesc->ucPhyTypeSet = 0;

	if (BAND_2G4 == prBssDesc->eBand) {
		/* check if support 11n */
		if (prBssDesc->fgIsHTPresent)
			prBssDesc->ucPhyTypeSet |= PHY_TYPE_BIT_HT;

		/* if not 11n only */
		if (!(prBssDesc->u2BSSBasicRateSet & RATE_SET_BIT_HT_PHY)) {
			/* check if support 11g */
			if ((prBssDesc->u2OperationalRateSet & RATE_SET_OFDM) || prBssDesc->fgIsERPPresent)
				prBssDesc->ucPhyTypeSet |= PHY_TYPE_BIT_ERP;

			/* if not 11g only */
			if (!(prBssDesc->u2BSSBasicRateSet & RATE_SET_OFDM)) {
				/* check if support 11b */
				if ((prBssDesc->u2OperationalRateSet & RATE_SET_HR_DSSS))
					prBssDesc->ucPhyTypeSet |= PHY_TYPE_BIT_HR_DSSS;
			}
		}
	} else {		/* (BAND_5G == prBssDesc->eBande) */
		/* check if support 11n */
		if (prBssDesc->fgIsHTPresent)
			prBssDesc->ucPhyTypeSet |= PHY_TYPE_BIT_HT;

		/* if not 11n only */
		if (!(prBssDesc->u2BSSBasicRateSet & RATE_SET_BIT_HT_PHY)) {
			/* Support 11a definitely */
			prBssDesc->ucPhyTypeSet |= PHY_TYPE_BIT_OFDM;

			ASSERT(!(prBssDesc->u2OperationalRateSet & RATE_SET_HR_DSSS));
		}
	}
	aisRemoveBeaconTimeoutEntry(prAdapter, prBssDesc);
	/* update update-index and reset seen-probe-response */
	if (prBssDesc->u4UpdateIdx != prAdapter->rWifiVar.rScanInfo.u4ScanUpdateIdx) {
		prBssDesc->fgSeenProbeResp = FALSE;
		prBssDesc->u4UpdateIdx = prAdapter->rWifiVar.rScanInfo.u4ScanUpdateIdx;
	}
	/* check if it is a probe response frame */
	if ((prWlanBeaconFrame->u2FrameCtrl & 0x50) == 0x50)
		prBssDesc->fgSeenProbeResp = TRUE;

	/* 4 <6> Update BSS_DESC_T's Last Update TimeStamp. */
	GET_CURRENT_SYSTIME(&prBssDesc->rUpdateTime);

	return prBssDesc;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief Convert the Beacon or ProbeResp Frame in SW_RFB_T to scan result for query
*
* @param[in] prSwRfb            Pointer to the receiving SW_RFB_T structure.
*
* @retval WLAN_STATUS_SUCCESS   It is a valid Scan Result and been sent to the host.
* @retval WLAN_STATUS_FAILURE   It is not a valid Scan Result.
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS scanAddScanResult(IN P_ADAPTER_T prAdapter, IN P_BSS_DESC_T prBssDesc, IN P_SW_RFB_T prSwRfb)
{
	P_SCAN_INFO_T prScanInfo;
	UINT_8 aucRatesEx[PARAM_MAX_LEN_RATES_EX];
	P_WLAN_BEACON_FRAME_T prWlanBeaconFrame;
	PARAM_MAC_ADDRESS rMacAddr;
	PARAM_SSID_T rSsid;
	ENUM_PARAM_NETWORK_TYPE_T eNetworkType;
	PARAM_802_11_CONFIG_T rConfiguration;
	ENUM_PARAM_OP_MODE_T eOpMode;
	UINT_8 ucRateLen = 0;
	UINT_32 i;

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);

	if (prBssDesc->eBand == BAND_2G4) {
		if ((prBssDesc->u2OperationalRateSet & RATE_SET_OFDM)
		    || prBssDesc->fgIsERPPresent) {
			eNetworkType = PARAM_NETWORK_TYPE_OFDM24;
		} else {
			eNetworkType = PARAM_NETWORK_TYPE_DS;
		}
	} else {
		ASSERT(prBssDesc->eBand == BAND_5G);
		eNetworkType = PARAM_NETWORK_TYPE_OFDM5;
	}

	if (prBssDesc->eBSSType == BSS_TYPE_P2P_DEVICE) {
		/* NOTE(Kevin): Not supported by WZC(TBD) */
		return WLAN_STATUS_FAILURE;
	}

	prWlanBeaconFrame = (P_WLAN_BEACON_FRAME_T) prSwRfb->pvHeader;
	COPY_MAC_ADDR(rMacAddr, prWlanBeaconFrame->aucBSSID);
	COPY_SSID(rSsid.aucSsid, rSsid.u4SsidLen, prBssDesc->aucSSID, prBssDesc->ucSSIDLen);

	rConfiguration.u4Length = sizeof(PARAM_802_11_CONFIG_T);
	rConfiguration.u4BeaconPeriod = (UINT_32) prWlanBeaconFrame->u2BeaconInterval;
	rConfiguration.u4ATIMWindow = prBssDesc->u2ATIMWindow;
	rConfiguration.u4DSConfig = nicChannelNum2Freq(prBssDesc->ucChannelNum);
	rConfiguration.rFHConfig.u4Length = sizeof(PARAM_802_11_CONFIG_FH_T);

	rateGetDataRatesFromRateSet(prBssDesc->u2OperationalRateSet, 0, aucRatesEx, &ucRateLen);

	/* NOTE(Kevin): Set unused entries, if any, at the end of the array to 0.
	 * from OID_802_11_BSSID_LIST
	 */
	for (i = ucRateLen; i < sizeof(aucRatesEx) / sizeof(aucRatesEx[0]); i++)
		aucRatesEx[i] = 0;

	switch (prBssDesc->eBSSType) {
	case BSS_TYPE_IBSS:
		eOpMode = NET_TYPE_IBSS;
		break;

	case BSS_TYPE_INFRASTRUCTURE:
	case BSS_TYPE_P2P_DEVICE:
	case BSS_TYPE_BOW_DEVICE:
	default:
		eOpMode = NET_TYPE_INFRA;
		break;
	}

	DBGLOG(SCN, TRACE, "ind %s %d\n", prBssDesc->aucSSID, prBssDesc->ucChannelNum);

#if (CFG_SUPPORT_TDLS == 1)
	{
		if (flgTdlsTestExtCapElm == TRUE) {
			/* only for RALINK AP */
			UINT8 *pucElm = (UINT8 *) (prSwRfb->pvHeader + prSwRfb->u2PacketLen);

			kalMemCopy(pucElm - 9, aucTdlsTestExtCapElm, 7);
			prSwRfb->u2PacketLen -= 2;
/* prSwRfb->u2PacketLen += 7; */

			DBGLOG(TDLS, INFO,
			       "<tdls> %s: append ext cap element to %pM\n",
				__func__, prBssDesc->aucBSSID);
		}
	}
#endif /* CFG_SUPPORT_TDLS */

	if (prAdapter->rWifiVar.rScanInfo.fgNloScanning &&
				test_bit(SUSPEND_FLAG_CLEAR_WHEN_RESUME, &prAdapter->ulSuspendFlag)) {
			UINT_8 i = 0;
			P_BSS_DESC_T *pprPendBssDesc = &prScanInfo->rNloParam.aprPendingBssDescToInd[0];

			for (; i < SCN_SSID_MATCH_MAX_NUM; i++) {
				if (pprPendBssDesc[i])
					continue;
				DBGLOG(SCN, INFO,
					"indicate bss[%pM] before wiphy resume, need to indicate again after wiphy resume\n",
					prBssDesc->aucBSSID);
				pprPendBssDesc[i] = prBssDesc;
				break;
			}
	}

	kalIndicateBssInfo(prAdapter->prGlueInfo,
			   (PUINT_8) prSwRfb->pvHeader,
			   prSwRfb->u2PacketLen, prBssDesc->ucChannelNum, RCPI_TO_dBm(prBssDesc->ucRCPI));

	nicAddScanResult(prAdapter,
			 rMacAddr,
			 &rSsid,
			 prWlanBeaconFrame->u2CapInfo,
			 RCPI_TO_dBm(prBssDesc->ucRCPI),
			 eNetworkType,
			 &rConfiguration,
			 eOpMode,
			 aucRatesEx,
			 prSwRfb->u2PacketLen - prSwRfb->u2HeaderLen,
			 (PUINT_8) ((ULONG) (prSwRfb->pvHeader) + WLAN_MAC_MGMT_HEADER_LEN));

	return WLAN_STATUS_SUCCESS;

}				/* end of scanAddScanResult() */

BOOLEAN scanCheckBssIsLegal(IN P_ADAPTER_T prAdapter, P_BSS_DESC_T prBssDesc)
{
	BOOLEAN fgAddToScanResult = FALSE;
	ENUM_BAND_T eBand = 0;
	UINT_8 ucChannel = 0;

	ASSERT(prAdapter);
	/* check the channel is in the legal doamin */
	if (rlmDomainIsLegalChannel(prAdapter, prBssDesc->eBand, prBssDesc->ucChannelNum) == TRUE) {
		/* check ucChannelNum/eBand for adjacement channel filtering */
		if (cnmAisInfraChannelFixed(prAdapter, &eBand, &ucChannel) == TRUE &&
		    (eBand != prBssDesc->eBand || ucChannel != prBssDesc->ucChannelNum)) {
			fgAddToScanResult = FALSE;
		} else {
			fgAddToScanResult = TRUE;
		}
	}
	return fgAddToScanResult;

}

VOID scanReportBss2Cfg80211(IN P_ADAPTER_T prAdapter, IN ENUM_BSS_TYPE_T eBSSType, IN P_BSS_DESC_T prSpecificBssDesc)
{
	P_LINK_T prBSSDescList = (P_LINK_T) NULL;
	P_BSS_DESC_T prBssDesc = (P_BSS_DESC_T) NULL;
	RF_CHANNEL_INFO_T rChannelInfo;

	ASSERT(prAdapter);

	DBGLOG(SCN, TRACE, "scanReportBss2Cfg80211\n");

	if (prSpecificBssDesc) {

		/* Check BSSID is legal channel */
		if (!scanCheckBssIsLegal(prAdapter, prSpecificBssDesc)) {
			DBGLOG(SCN, TRACE, "Remove specific SSID[%s] on channel %d\n",
			       prSpecificBssDesc->aucSSID, prSpecificBssDesc->ucChannelNum);
			return;
		}

		DBGLOG(SCN, TRACE, "Report specific SSID[%s]\n", prSpecificBssDesc->aucSSID);

		if (eBSSType == BSS_TYPE_INFRASTRUCTURE) {
			kalIndicateBssInfo(prAdapter->prGlueInfo,
					   (PUINT_8) prSpecificBssDesc->aucRawBuf,
					   prSpecificBssDesc->u2RawLength,
					   prSpecificBssDesc->ucChannelNum,
					   RCPI_TO_dBm(prSpecificBssDesc->ucRCPI));
		} else {
			rChannelInfo.ucChannelNum = prSpecificBssDesc->ucChannelNum;
			rChannelInfo.eBand = prSpecificBssDesc->eBand;
			kalP2PIndicateBssInfo(prAdapter->prGlueInfo,
					      (PUINT_8) prSpecificBssDesc->aucRawBuf,
					      prSpecificBssDesc->u2RawLength,
					      &rChannelInfo,
					      RCPI_TO_dBm(prSpecificBssDesc->ucRCPI));
		}

#if CFG_ENABLE_WIFI_DIRECT
		prSpecificBssDesc->fgIsP2PReport = FALSE;
#endif

	} else {
		/* Search BSS Desc from current SCAN result list. */
		prBSSDescList = &(prAdapter->rWifiVar.rScanInfo.rBSSDescList);

		LINK_FOR_EACH_ENTRY(prBssDesc, prBSSDescList, rLinkEntry, BSS_DESC_T) {

#if CFG_AUTO_CHANNEL_SEL_SUPPORT
			/* Record channel loading with channel's AP number */
			UINT_8 ucIdx = 0;

			if (prBssDesc->ucChannelNum <= 14)
				ucIdx = prBssDesc->ucChannelNum - 1;
			else if (prBssDesc->ucChannelNum >= 36 && prBssDesc->ucChannelNum <= 64)
				ucIdx = 14 + (prBssDesc->ucChannelNum - 36) / 4;
			else if (prBssDesc->ucChannelNum >= 100 && prBssDesc->ucChannelNum <= 144)
				ucIdx = 14 + 8 + (prBssDesc->ucChannelNum - 100) / 4;
			else if (prBssDesc->ucChannelNum >= 149)
				ucIdx = 14 + 8 + 12 + (prBssDesc->ucChannelNum - 149) / 4;

			if (ucIdx < MAX_CHN_NUM) {
				prAdapter->rWifiVar.rChnLoadInfo.rEachChnLoad[ucIdx].ucChannel =
					prBssDesc->ucChannelNum;
				prAdapter->rWifiVar.rChnLoadInfo.rEachChnLoad[ucIdx].u2APNum++;
			}
#endif

			/* Check BSSID is legal channel */
			if (!scanCheckBssIsLegal(prAdapter, prBssDesc)) {
				DBGLOG(SCN, TRACE, "Remove SSID[%s] on channel %d\n",
				       prBssDesc->aucSSID, prBssDesc->ucChannelNum);
				continue;
			}

			if ((prBssDesc->eBSSType == eBSSType)
#if CFG_ENABLE_WIFI_DIRECT
			    || ((eBSSType == BSS_TYPE_P2P_DEVICE) && (prBssDesc->fgIsP2PReport == TRUE))
#endif
			    ) {

				DBGLOG(SCN, TRACE, "Report SSID[%s]\n", prBssDesc->aucSSID);

				if (eBSSType == BSS_TYPE_INFRASTRUCTURE) {
					if (prBssDesc->u2RawLength != 0) {
						kalIndicateBssInfo(prAdapter->prGlueInfo,
								   (PUINT_8) prBssDesc->aucRawBuf,
								   prBssDesc->u2RawLength,
								   prBssDesc->ucChannelNum,
								   RCPI_TO_dBm(prBssDesc->ucRCPI));
						kalMemZero(prBssDesc->aucRawBuf, CFG_RAW_BUFFER_SIZE);
						prBssDesc->u2RawLength = 0;
#if CFG_ENABLE_WIFI_DIRECT
						prBssDesc->fgIsP2PReport = FALSE;
#endif
					}
				} else {
#if CFG_ENABLE_WIFI_DIRECT
					if (prBssDesc->fgIsP2PReport == TRUE) {
#endif
						rChannelInfo.ucChannelNum = prBssDesc->ucChannelNum;
						rChannelInfo.eBand = prBssDesc->eBand;

						kalP2PIndicateBssInfo(prAdapter->prGlueInfo,
								      (PUINT_8) prBssDesc->aucRawBuf,
								      prBssDesc->u2RawLength,
								      &rChannelInfo,
								      RCPI_TO_dBm(prBssDesc->ucRCPI));

						/* Do not clear it then we can pass the bss in Specific report */
						/* kalMemZero(prBssDesc->aucRawBuf,CFG_RAW_BUFFER_SIZE); */

						/* The BSS entry will not be cleared after scan done.
						 * So if we dont receive the BSS in next scan, we cannot
						 * pass it. We use u2RawLength for the purpose.
						 */
						/* prBssDesc->u2RawLength=0; */
#if CFG_ENABLE_WIFI_DIRECT
						prBssDesc->fgIsP2PReport = FALSE;
					}
#endif
				}
			}
		}

#if CFG_AUTO_CHANNEL_SEL_SUPPORT
		prAdapter->rWifiVar.rChnLoadInfo.fgDataReadyBit = TRUE;
#endif
	}

}

/*----------------------------------------------------------------------------*/
/*!
* @brief Parse channel number to array index.
*
* @param[in] u4ChannelNum            channel number.
*
* @retval index           array index
*/
/*----------------------------------------------------------------------------*/
UINT_8 nicChannelNum2Index(IN UINT_8 ucChannelNum)
{
	UINT_8 ucindex;

	/*Full2Partial*/
	if (ucChannelNum >= 1 && ucChannelNum <= 14)
		/*1---14*/
		ucindex = ucChannelNum;
	else if (ucChannelNum >= 36 && ucChannelNum <= 64)
		/*15---22*/
		ucindex = 6 + (ucChannelNum >> 2);
	else if (ucChannelNum >= 100 && ucChannelNum <= 144)
		/*23---34*/
		ucindex = (ucChannelNum >> 2) - 2;
	else if (ucChannelNum >= 149 && ucChannelNum <= 165) {
		/*35---39*/
		ucChannelNum = ucChannelNum - 1;
		ucindex = (ucChannelNum >> 2) - 2;
	} else
		ucindex = 0;

		return ucindex;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief Parse the content of given Beacon or ProbeResp Frame.
*
* @param[in] prSwRfb            Pointer to the receiving SW_RFB_T structure.
*
* @retval WLAN_STATUS_SUCCESS           if not report this SW_RFB_T to host
* @retval WLAN_STATUS_PENDING           if report this SW_RFB_T to host as scan result
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS scanProcessBeaconAndProbeResp(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
	P_CONNECTION_SETTINGS_T prConnSettings;
	P_BSS_DESC_T prBssDesc = (P_BSS_DESC_T) NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	P_BSS_INFO_T prAisBssInfo;
	P_WLAN_BEACON_FRAME_T prWlanBeaconFrame = (P_WLAN_BEACON_FRAME_T) NULL;
#if CFG_SLT_SUPPORT
	P_SLT_INFO_T prSltInfo = (P_SLT_INFO_T) NULL;
#endif

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	/* 4 <0> Ignore invalid Beacon Frame */
	if ((prSwRfb->u2PacketLen - prSwRfb->u2HeaderLen) <
		(TIMESTAMP_FIELD_LEN + BEACON_INTERVAL_FIELD_LEN + CAP_INFO_FIELD_LEN)) {
		/* to debug beacon length too small issue */
		UINT_32 u4MailBox0;

		nicGetMailbox(prAdapter, 0, &u4MailBox0);
		DBGLOG(SCN, WARN, "if conn sys also get less length (0x5a means yes) %x\n", (UINT_32) u4MailBox0);
		DBGLOG(SCN, WARN, "u2PacketLen %d, u2HeaderLen %d, payloadLen %d\n",
			prSwRfb->u2PacketLen, prSwRfb->u2HeaderLen,
			prSwRfb->u2PacketLen - prSwRfb->u2HeaderLen);
		/* dumpMemory8(prSwRfb->pvHeader, prSwRfb->u2PacketLen); */

#ifndef _lint
		ASSERT(0);
#endif /* _lint */
		return rStatus;
	}
#if CFG_SLT_SUPPORT
	prSltInfo = &prAdapter->rWifiVar.rSltInfo;

	if (prSltInfo->fgIsDUT) {
		DBGLOG(SCN, INFO, "\n\rBCN: RX\n");
		prSltInfo->u4BeaconReceiveCnt++;
		return WLAN_STATUS_SUCCESS;
	} else {
		return WLAN_STATUS_SUCCESS;
	}
#endif

	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);
	prAisBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);
	prWlanBeaconFrame = (P_WLAN_BEACON_FRAME_T) prSwRfb->pvHeader;

	/*ALPS01475157: don't show SSID on scan list for multicast MAC AP */
	if (IS_BMCAST_MAC_ADDR(prWlanBeaconFrame->aucSrcAddr)) {
		DBGLOG(SCN, WARN, "received beacon/probe response from multicast AP\n");
		return rStatus;
	}

	/* 4 <1> Parse and add into BSS_DESC_T */
	prBssDesc = scanAddToBssDesc(prAdapter, prSwRfb);

	if (prBssDesc) {
		/*Full2Partial at here, we should save channel info*/
		if (prAdapter->prGlueInfo->ucTrScanType == 1) {
			UINT_8 ucindex;

			ucindex = nicChannelNum2Index(prBssDesc->ucChannelNum);
			DBGLOG(SCN, TRACE, "Full2Partial ucChannelNum=%d, ucindex=%d\n",
				prBssDesc->ucChannelNum, ucindex);

			/*prAdapter->prGlueInfo->ucChannelListNum++;*/
			prAdapter->prGlueInfo->ucChannelNum[ucindex] = 1;
		}

		/* 4 <1.1> Beacon Change Detection for Connected BSS */
		if (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED &&
		    ((prBssDesc->eBSSType == BSS_TYPE_INFRASTRUCTURE && prConnSettings->eOPMode != NET_TYPE_IBSS)
		     || (prBssDesc->eBSSType == BSS_TYPE_IBSS && prConnSettings->eOPMode != NET_TYPE_INFRA)) &&
		    EQUAL_MAC_ADDR(prBssDesc->aucBSSID, prAisBssInfo->aucBSSID) &&
		    EQUAL_SSID(prBssDesc->aucSSID, prBssDesc->ucSSIDLen, prAisBssInfo->aucSSID,
			       prAisBssInfo->ucSSIDLen)) {
			BOOLEAN fgNeedDisconnect = FALSE;

#if CFG_SUPPORT_BEACON_CHANGE_DETECTION
			/* <1.1.2> check if supported rate differs */
			if (prAisBssInfo->u2OperationalRateSet != prBssDesc->u2OperationalRateSet)
				fgNeedDisconnect = TRUE;
#endif
#if CFG_SUPPORT_DETECT_SECURITY_MODE_CHANGE
			if (
#if CFG_SUPPORT_WAPI
			(prAdapter->rWifiVar.rConnSettings.fgWapiMode == TRUE &&
			!wapiPerformPolicySelection(prAdapter, prBssDesc)) ||
#endif
			rsnCheckSecurityModeChanged(prAdapter, prAisBssInfo, prBssDesc)) {
				DBGLOG(SCN, INFO, "Beacon security mode change detected\n");
				fgNeedDisconnect = FALSE;
				aisBssSecurityChanged(prAdapter);
			}
#endif

			/* <1.1.3> beacon content change detected, disconnect immediately */
			if (fgNeedDisconnect == TRUE)
				aisBssBeaconTimeout(prAdapter);
		}
		/* 4 <1.1> Update AIS_BSS_INFO */
		if (((prBssDesc->eBSSType == BSS_TYPE_INFRASTRUCTURE && prConnSettings->eOPMode != NET_TYPE_IBSS)
		     || (prBssDesc->eBSSType == BSS_TYPE_IBSS && prConnSettings->eOPMode != NET_TYPE_INFRA))) {
			if (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED) {
				/* *not* checking prBssDesc->fgIsConnected anymore,
				 * due to Linksys AP uses " " as hidden SSID, and would have different BSS descriptor */
				if ((!prAisBssInfo->ucDTIMPeriod) &&
				    EQUAL_MAC_ADDR(prBssDesc->aucBSSID, prAisBssInfo->aucBSSID) &&
				    (prAisBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE) &&
				    ((prWlanBeaconFrame->u2FrameCtrl & MASK_FRAME_TYPE) == MAC_FRAME_BEACON)) {

					prAisBssInfo->ucDTIMPeriod = prBssDesc->ucDTIMPeriod;

					/* sync with firmware for beacon information */
					nicPmIndicateBssConnected(prAdapter, NETWORK_TYPE_AIS_INDEX);
				}
			}
#if CFG_SUPPORT_ADHOC
			if (EQUAL_SSID(prBssDesc->aucSSID,
				prBssDesc->ucSSIDLen,
				prConnSettings->aucSSID,
				prConnSettings->ucSSIDLen) &&
			    (prBssDesc->eBSSType == BSS_TYPE_IBSS) && (prAisBssInfo->eCurrentOPMode == OP_MODE_IBSS)) {
				ibssProcessMatchedBeacon(prAdapter, prAisBssInfo, prBssDesc,
							 prSwRfb->prHifRxHdr->ucRcpi);
			}
#endif /* CFG_SUPPORT_ADHOC */
		}

		rlmProcessBcn(prAdapter,
			      prSwRfb,
			      ((P_WLAN_BEACON_FRAME_T) (prSwRfb->pvHeader))->aucInfoElem,
			      (prSwRfb->u2PacketLen - prSwRfb->u2HeaderLen) -
			      (UINT_16) (OFFSET_OF(WLAN_BEACON_FRAME_BODY_T, aucInfoElem[0])));

		/* 4 <3> Send SW_RFB_T to HIF when we perform SCAN for HOST */
		if (prBssDesc->eBSSType == BSS_TYPE_INFRASTRUCTURE || prBssDesc->eBSSType == BSS_TYPE_IBSS) {
			/* for AIS, send to host */
			if (prConnSettings->fgIsScanReqIssued || prAdapter->rWifiVar.rScanInfo.fgNloScanning
#if CFG_SUPPORT_SCN_PSCN
			|| prAdapter->rWifiVar.rScanInfo.fgPscnOngoing
#endif
			) {
				BOOLEAN fgAddToScanResult;

				fgAddToScanResult = scanCheckBssIsLegal(prAdapter, prBssDesc);

				if (fgAddToScanResult == TRUE)
					rStatus = scanAddScanResult(prAdapter, prBssDesc, prSwRfb);
			}
		}
#if CFG_ENABLE_WIFI_DIRECT
		if (prAdapter->fgIsP2PRegistered)
			scanP2pProcessBeaconAndProbeResp(prAdapter, prSwRfb, &rStatus, prBssDesc, prWlanBeaconFrame);
#endif
	}

	return rStatus;

}				/* end of scanProcessBeaconAndProbeResp() */

INT_32 scanResultsAdjust5GPref(P_BSS_DESC_T prBssDesc)
{
	INT_32 rssi = RCPI_TO_dBm(prBssDesc->ucRCPI);
	INT_32 orgRssi = rssi;

	if (prBssDesc->eBand == BAND_5G) {
		if (rssi >= rssiRangeHi)
			rssi += pref5GhzHi;
		else if (rssi >= rssiRangeMed)
			rssi += pref5GhzMed;
		else if (rssi >= rssiRangeLo)
			rssi += pref5GhzLo;
	}
	/* Reduce chances of roam ping-pong */
	if (prBssDesc->fgIsConnected)
		rssi += (ROAMING_NO_SWING_RCPI_STEP >> 1);

	if (prBssDesc->eBand == BAND_5G || prBssDesc->fgIsConnected)
		DBGLOG(SCN, TRACE, "Adjust 5G band RSSI: " MACSTR " band=%d, orgRssi=%d afterRssi=%d\n",
			MAC2STR(prBssDesc->aucBSSID), prBssDesc->eBand, orgRssi, rssi);
	return rssi;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Search the Candidate of BSS Descriptor for JOIN(Infrastructure) or
*        MERGE(AdHoc) according to current Connection Policy.
*
* \return   Pointer to BSS Descriptor, if found. NULL, if not found
*/
/*----------------------------------------------------------------------------*/
P_BSS_DESC_T scanSearchBssDescByPolicy(IN P_ADAPTER_T prAdapter, IN ENUM_NETWORK_TYPE_INDEX_T eNetTypeIndex)
{
	P_CONNECTION_SETTINGS_T prConnSettings;
	P_BSS_INFO_T prBssInfo;
	P_AIS_SPECIFIC_BSS_INFO_T prAisSpecBssInfo;
	P_SCAN_INFO_T prScanInfo;

	P_LINK_T prBSSDescList;

	P_BSS_DESC_T prBssDesc = (P_BSS_DESC_T) NULL;
	P_BSS_DESC_T prPrimaryBssDesc = (P_BSS_DESC_T) NULL;
	P_BSS_DESC_T prCandidateBssDesc = (P_BSS_DESC_T) NULL;

	P_STA_RECORD_T prStaRec = (P_STA_RECORD_T) NULL;
	P_STA_RECORD_T prPrimaryStaRec;
	P_STA_RECORD_T prCandidateStaRec = (P_STA_RECORD_T) NULL;

	OS_SYSTIME rCurrentTime;

	/* The first one reach the check point will be our candidate */
	BOOLEAN fgIsFindFirst = (BOOLEAN) FALSE;

	BOOLEAN fgIsFindBestRSSI = (BOOLEAN) FALSE;
	BOOLEAN fgIsFindBestEncryptionLevel = (BOOLEAN) FALSE;
	/* BOOLEAN fgIsFindMinChannelLoad = (BOOLEAN)FALSE; */

	/* TODO(Kevin): Support Min Channel Load */
	/* UINT_8 aucChannelLoad[CHANNEL_NUM] = {0}; */

	BOOLEAN fgIsFixedChannel;
	ENUM_BAND_T eBand = 0;
	UINT_8 ucChannel = 0;

	ASSERT(prAdapter);

	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);
	prBssInfo = &(prAdapter->rWifiVar.arBssInfo[eNetTypeIndex]);

	prAisSpecBssInfo = &(prAdapter->rWifiVar.rAisSpecificBssInfo);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prBSSDescList = &prScanInfo->rBSSDescList;

	GET_CURRENT_SYSTIME(&rCurrentTime);

	/* check for fixed channel operation */
	if (eNetTypeIndex == NETWORK_TYPE_AIS_INDEX) {
#if CFG_P2P_LEGACY_COEX_REVISE
		fgIsFixedChannel = cnmAisDetectP2PChannel(prAdapter, &eBand, &ucChannel);
#else
		fgIsFixedChannel = cnmAisInfraChannelFixed(prAdapter, &eBand, &ucChannel);
#endif
	} else {
		fgIsFixedChannel = FALSE;
	}

#if DBG
	if (prConnSettings->ucSSIDLen < ELEM_MAX_LEN_SSID)
		prConnSettings->aucSSID[prConnSettings->ucSSIDLen] = '\0';
#endif

	DBGLOG(SCN, INFO, "SEARCH: Bss Num: %d, Look for SSID: %s, %pM Band=%d, channel=%d\n",
			   (UINT_32) prBSSDescList->u4NumElem, prConnSettings->aucSSID,
			   (prConnSettings->aucBSSID), eBand, ucChannel);

	/* 4 <1> The outer loop to search for a candidate. */
	LINK_FOR_EACH_ENTRY(prBssDesc, prBSSDescList, rLinkEntry, BSS_DESC_T) {

		/* TODO(Kevin): Update Minimum Channel Load Information here */

		DBGLOG(SCN, TRACE, "SEARCH: [ %pM ], SSID:%s\n",
				   prBssDesc->aucBSSID, prBssDesc->aucSSID);

		/* 4 <2> Check PHY Type and attributes */
		/* 4 <2.1> Check Unsupported BSS PHY Type */
		if (!(prBssDesc->ucPhyTypeSet & (prAdapter->rWifiVar.ucAvailablePhyTypeSet))) {
			DBGLOG(SCN, TRACE, "SEARCH: Ignore unsupported ucPhyTypeSet = %x\n", prBssDesc->ucPhyTypeSet);
			continue;
		}
		/* 4 <2.2> Check if has unknown NonHT BSS Basic Rate Set. */
		if (prBssDesc->fgIsUnknownBssBasicRate)
			continue;
		/* 4 <2.3> Check if fixed operation cases should be aware */
		if (fgIsFixedChannel == TRUE && (prBssDesc->eBand != eBand || prBssDesc->ucChannelNum != ucChannel))
			continue;
		/* 4 <2.4> Check if the channel is legal under regulatory domain */
		if (rlmDomainIsLegalChannel(prAdapter, prBssDesc->eBand, prBssDesc->ucChannelNum) == FALSE)
			continue;
		/* 4 <2.5> Check if this BSS_DESC_T is stale */
#if CFG_SUPPORT_RN
	if (prBssInfo->fgDisConnReassoc == FALSE)
#endif
		if (CHECK_FOR_TIMEOUT(rCurrentTime, prBssDesc->rUpdateTime,
				      SEC_TO_SYSTIME(SCN_BSS_DESC_REMOVE_TIMEOUT_SEC))) {

			BOOLEAN fgIsNeedToCheckTimeout = TRUE;

#if CFG_SUPPORT_ROAMING
			P_ROAMING_INFO_T prRoamingFsmInfo;

			prRoamingFsmInfo = (P_ROAMING_INFO_T) &(prAdapter->rWifiVar.rRoamingInfo);
			if ((prRoamingFsmInfo->eCurrentState == ROAMING_STATE_DISCOVERY) ||
			    (prRoamingFsmInfo->eCurrentState == ROAMING_STATE_ROAM)) {
				if (++prRoamingFsmInfo->RoamingEntryTimeoutSkipCount <
				    ROAMING_ENTRY_TIMEOUT_SKIP_COUNT_MAX) {
					fgIsNeedToCheckTimeout = FALSE;
					DBGLOG(SCN, INFO, "SEARCH: Romaing skip SCN_BSS_DESC_REMOVE_TIMEOUT_SEC\n");
				}
			}
#endif

			if (fgIsNeedToCheckTimeout == TRUE) {
				DBGLOG(SCN, TRACE, "Ignore stale bss %pM\n", prBssDesc->aucBSSID);
				continue;
			}
		}
		/* 4 <3> Check if reach the excessive join retry limit */
		/* NOTE(Kevin): STA_RECORD_T is recorded by TA. */
		prStaRec = cnmGetStaRecByAddress(prAdapter, (UINT_8) eNetTypeIndex, prBssDesc->aucSrcAddr);

		if (prStaRec) {
			/* NOTE(Kevin):
			 * The Status Code is the result of a Previous Connection Request,
			 * we use this as SCORE for choosing a proper
			 * candidate (Also used for compare see <6>)
			 * The Reason Code is an indication of the reason why AP reject us,
			 * we use this Code for "Reject"
			 * a SCAN result to become our candidate(Like a blacklist).
			 */
#if 0				/* TODO(Kevin): */
			if (prStaRec->u2ReasonCode != REASON_CODE_RESERVED) {
				DBGLOG(SCN, INFO, "SEARCH: Ignore BSS with previous Reason Code = %d\n",
						   prStaRec->u2ReasonCode);
				continue;
			} else
#endif
			if (prStaRec->u2StatusCode != STATUS_CODE_SUCCESSFUL) {
				/* NOTE(Kevin): greedy association - after timeout, we'll still
				 * try to associate to the AP whose STATUS of conection attempt
				 * was not success.
				 * We may also use (ucJoinFailureCount x JOIN_RETRY_INTERVAL_SEC) for
				 * time bound.
				 */
				if ((prStaRec->ucJoinFailureCount < JOIN_MAX_RETRY_FAILURE_COUNT) ||
				    (CHECK_FOR_TIMEOUT(rCurrentTime,
						       prStaRec->rLastJoinTime,
						       SEC_TO_SYSTIME(JOIN_RETRY_INTERVAL_SEC)))) {

					/* NOTE(Kevin): Every JOIN_RETRY_INTERVAL_SEC interval, we can retry
					 * JOIN_MAX_RETRY_FAILURE_COUNT times.
					 */
					if (prStaRec->ucJoinFailureCount >= JOIN_MAX_RETRY_FAILURE_COUNT)
						prStaRec->ucJoinFailureCount = 0;
					DBGLOG(SCN, INFO,
					       "SEARCH: Try to join BSS again,Status Code=%d (Curr=%u/Last Join=%u)\n",
						prStaRec->u2StatusCode, rCurrentTime, prStaRec->rLastJoinTime);
				} else {
					DBGLOG(SCN, INFO,
					       "SEARCH: Ignore BSS which reach maximum Join Retry Count = %d\n",
						JOIN_MAX_RETRY_FAILURE_COUNT);
					continue;
				}

			}
		}
		/* 4 <4> Check for various NETWORK conditions */
		if (eNetTypeIndex == NETWORK_TYPE_AIS_INDEX) {

			/* 4 <4.1> Check BSS Type for the corresponding Operation Mode in Connection Setting */
			/* NOTE(Kevin): For NET_TYPE_AUTO_SWITCH, we will always pass following check. */
			if (((prConnSettings->eOPMode == NET_TYPE_INFRA) &&
			     (prBssDesc->eBSSType != BSS_TYPE_INFRASTRUCTURE))
#if CFG_SUPPORT_ADHOC
			     || ((prConnSettings->eOPMode == NET_TYPE_IBSS
			      || prConnSettings->eOPMode == NET_TYPE_DEDICATED_IBSS)
			     && (prBssDesc->eBSSType != BSS_TYPE_IBSS))
#endif
			     ) {

				DBGLOG(SCN, TRACE, "Cur OPMode %d, Ignore eBSSType = %d\n",
						prConnSettings->eOPMode, prBssDesc->eBSSType);
				continue;
			}
			/* 4 <4.2> Check AP's BSSID if OID_802_11_BSSID has been set. */
			if ((prConnSettings->fgIsConnByBssidIssued) &&
				(prBssDesc->eBSSType == BSS_TYPE_INFRASTRUCTURE)) {

				if (UNEQUAL_MAC_ADDR(prConnSettings->aucBSSID, prBssDesc->aucBSSID)) {

					DBGLOG(SCN, TRACE, "SEARCH: Ignore due to BSSID was not matched!\n");
					continue;
				}
			}
#if CFG_SUPPORT_ADHOC
			/* 4 <4.3> Check for AdHoc Mode */
			if (prBssDesc->eBSSType == BSS_TYPE_IBSS) {
				OS_SYSTIME rCurrentTime;

				/* 4 <4.3.1> Check if this SCAN record has been updated recently for IBSS. */
				/* NOTE(Kevin): Because some STA may change its BSSID frequently after it
				 * create the IBSS - e.g. IPN2220, so we need to make sure we get the new one.
				 * For BSS, if the old record was matched, however it won't be able to pass
				 * the Join Process later.
				 */
				GET_CURRENT_SYSTIME(&rCurrentTime);
				if (CHECK_FOR_TIMEOUT(rCurrentTime, prBssDesc->rUpdateTime,
						      SEC_TO_SYSTIME(SCN_ADHOC_BSS_DESC_TIMEOUT_SEC))) {
					DBGLOG(SCN, LOUD,
						"SEARCH: Skip old record of BSS Descriptor - BSSID:[%pM]\n\n",
						prBssDesc->aucBSSID);
					continue;
				}
				/* 4 <4.3.2> Check Peer's capability */
				if (ibssCheckCapabilityForAdHocMode(prAdapter, prBssDesc) == WLAN_STATUS_FAILURE) {

					if (prPrimaryBssDesc)
						DBGLOG(SCN, INFO,
							"SEARCH: BSS DESC MAC: %pM, not supported AdHoc Mode.\n",
							prPrimaryBssDesc->aucBSSID);

					continue;
				}
				/* 4 <4.3.3> Compare TSF */
				if (prBssInfo->fgIsBeaconActivated &&
				    UNEQUAL_MAC_ADDR(prBssInfo->aucBSSID, prBssDesc->aucBSSID)) {

					DBGLOG(SCN, LOUD,
					       "SEARCH: prBssDesc->fgIsLargerTSF = %d\n", prBssDesc->fgIsLargerTSF);

					if (!prBssDesc->fgIsLargerTSF) {
						DBGLOG(SCN, INFO,
						       "SEARCH: Ignore BSS DESC MAC: [ %pM ], Smaller TSF\n",
							prBssDesc->aucBSSID);
						continue;
					}
				}
			}
#endif /* CFG_SUPPORT_ADHOC */

		}
#if 0				/* TODO(Kevin): For IBSS */
		/* 4 <2.c> Check if this SCAN record has been updated recently for IBSS. */
		/* NOTE(Kevin): Because some STA may change its BSSID frequently after it
		 * create the IBSS, so we need to make sure we get the new one.
		 * For BSS, if the old record was matched, however it won't be able to pass
		 * the Join Process later.
		 */
		if (prBssDesc->eBSSType == BSS_TYPE_IBSS) {
			OS_SYSTIME rCurrentTime;

			GET_CURRENT_SYSTIME(&rCurrentTime);
			if (CHECK_FOR_TIMEOUT(rCurrentTime, prBssDesc->rUpdateTime,
					      SEC_TO_SYSTIME(BSS_DESC_TIMEOUT_SEC))) {
				DBGLOG(SCAN, TRACE, "Skip old record of BSS Descriptor - BSSID:[%pM]\n\n",
						     prBssDesc->aucBSSID);
				continue;
			}
		}

		if ((prBssDesc->eBSSType == BSS_TYPE_INFRASTRUCTURE) &&
		    (prAdapter->eConnectionState == MEDIA_STATE_CONNECTED)) {
			OS_SYSTIME rCurrentTime;

			GET_CURRENT_SYSTIME(&rCurrentTime);
			if (CHECK_FOR_TIMEOUT(rCurrentTime, prBssDesc->rUpdateTime,
					      SEC_TO_SYSTIME(BSS_DESC_TIMEOUT_SEC))) {
				DBGLOG(SCAN, TRACE, "Skip old record of BSS Descriptor - BSSID:[%pM]\n\n",
						     (prBssDesc->aucBSSID));
				continue;
			}
		}
		/* 4 <4B> Check for IBSS AdHoc Mode. */
		/* Skip if one or more BSS Basic Rate are not supported by current AdHocMode */
		if (prPrimaryBssDesc->eBSSType == BSS_TYPE_IBSS) {
			/* 4 <4B.1> Check if match the Capability of current IBSS AdHoc Mode. */
			if (ibssCheckCapabilityForAdHocMode(prAdapter, prPrimaryBssDesc) == WLAN_STATUS_FAILURE) {

				DBGLOG(SCAN, TRACE,
				       "Ignore BSS DESC MAC: %pM, Capability not supported for AdHoc Mode.\n",
					prPrimaryBssDesc->aucBSSID);

				continue;
			}
			/* 4 <4B.2> IBSS Merge Decision Flow for SEARCH STATE. */
			if (prAdapter->fgIsIBSSActive &&
			    UNEQUAL_MAC_ADDR(prBssInfo->aucBSSID, prPrimaryBssDesc->aucBSSID)) {

				if (!fgIsLocalTSFRead) {
					NIC_GET_CURRENT_TSF(prAdapter, &rCurrentTsf);

					DBGLOG(SCAN, TRACE,
					       "\n\nCurrent TSF : %08lx-%08lx\n\n",
						rCurrentTsf.u.HighPart, rCurrentTsf.u.LowPart);
				}

				if (rCurrentTsf.QuadPart > prPrimaryBssDesc->u8TimeStamp.QuadPart) {
					DBGLOG(SCAN, TRACE,
					       "Ignore BSS DESC MAC: [%pM], Current BSSID: [%pM].\n",
						prPrimaryBssDesc->aucBSSID, prBssInfo->aucBSSID);

					DBGLOG(SCAN, TRACE,
					       "\n\nBSS's TSF : %08lx-%08lx\n\n",
						prPrimaryBssDesc->u8TimeStamp.u.HighPart,
						prPrimaryBssDesc->u8TimeStamp.u.LowPart);

					prPrimaryBssDesc->fgIsLargerTSF = FALSE;
					continue;
				} else {
					prPrimaryBssDesc->fgIsLargerTSF = TRUE;
				}

			}
		}
		/* 4 <5> Check the Encryption Status. */
		if (rsnPerformPolicySelection(prPrimaryBssDesc)) {

			if (prPrimaryBssDesc->ucEncLevel > 0) {
				fgIsFindBestEncryptionLevel = TRUE;

				fgIsFindFirst = FALSE;
			}
		} else {
			/* Can't pass the Encryption Status Check, get next one */
			continue;
		}

		/* For RSN Pre-authentication, update the PMKID canidate list for
		   same SSID and encrypt status */
		/* Update PMKID candicate list. */
		if (prAdapter->rWifiVar.rConnSettings.eAuthMode == AUTH_MODE_WPA2) {
			rsnUpdatePmkidCandidateList(prPrimaryBssDesc);
			if (prAdapter->rWifiVar.rAisBssInfo.u4PmkidCandicateCount)
				prAdapter->rWifiVar.rAisBssInfo.fgIndicatePMKID = rsnCheckPmkidCandicate();
		}
#endif

		prPrimaryBssDesc = (P_BSS_DESC_T) NULL;

		/* 4 <6> Check current Connection Policy. */
		switch (prConnSettings->eConnectionPolicy) {
		case CONNECT_BY_SSID_BEST_RSSI:
			/* Choose Hidden SSID to join only if the `fgIsEnableJoin...` is TRUE */
			if (prAdapter->rWifiVar.fgEnableJoinToHiddenSSID && prBssDesc->fgIsHiddenSSID) {
				/* NOTE(Kevin): following if () statement means that
				 * If Target is hidden, then we won't connect when user specify SSID_ANY policy.
				 */
				if (prConnSettings->ucSSIDLen) {
					prPrimaryBssDesc = prBssDesc;
					fgIsFindBestRSSI = TRUE;
				}

			} else if (EQUAL_SSID(prBssDesc->aucSSID,
					      prBssDesc->ucSSIDLen,
					      prConnSettings->aucSSID, prConnSettings->ucSSIDLen)) {
				prPrimaryBssDesc = prBssDesc;
				fgIsFindBestRSSI = TRUE;

				DBGLOG(SCN, TRACE, "SEARCH: fgIsFindBestRSSI=TRUE, %d, prPrimaryBssDesc=[ %pM ]\n",
						   prBssDesc->ucRCPI, prPrimaryBssDesc->aucBSSID);
			}
			break;

		case CONNECT_BY_SSID_ANY:
			/* NOTE(Kevin): In this policy, we don't know the desired
			 * SSID from user, so we should exclude the Hidden SSID from scan list.
			 * And because we refuse to connect to Hidden SSID node at the beginning, so
			 * when the JOIN Module deal with a BSS_DESC_T which has fgIsHiddenSSID == TRUE,
			 * then the Connection Settings must be valid without doubt.
			 */
			if (!prBssDesc->fgIsHiddenSSID) {
				prPrimaryBssDesc = prBssDesc;
				fgIsFindFirst = TRUE;
			}
			break;

		case CONNECT_BY_BSSID:
			if (EQUAL_MAC_ADDR(prBssDesc->aucBSSID, prConnSettings->aucBSSID))
				prPrimaryBssDesc = prBssDesc;
			break;

		default:
			break;
		}

		/* Primary Candidate was not found */
		if (prPrimaryBssDesc == NULL)
			continue;
		/* 4 <7> Check the Encryption Status. */
		if (prPrimaryBssDesc->eBSSType == BSS_TYPE_INFRASTRUCTURE) {
#if CFG_SUPPORT_WAPI
			if (prAdapter->rWifiVar.rConnSettings.fgWapiMode) {
				DBGLOG(SCN, TRACE, "SEARCH: fgWapiMode == 1\n");

				if (wapiPerformPolicySelection(prAdapter, prPrimaryBssDesc)) {
					fgIsFindFirst = TRUE;
				} else {
					/* Can't pass the Encryption Status Check, get next one */
					DBGLOG(SCN, TRACE, "SEARCH: WAPI cannot pass the Encryption Status Check!\n");
					continue;
				}
			} else
#endif
#if CFG_RSN_MIGRATION
			if (rsnPerformPolicySelection(prAdapter, prPrimaryBssDesc)) {
				if (prAisSpecBssInfo->fgCounterMeasure) {
					DBGLOG(RSN, INFO, "Skip while at counter measure period!!!\n");
					continue;
				}

				if (prPrimaryBssDesc->ucEncLevel > 0) {
					fgIsFindBestEncryptionLevel = TRUE;
					fgIsFindFirst = FALSE;
				}
#if 0
			/* Update PMKID candicate list. */
			if (prAdapter->rWifiVar.rConnSettings.eAuthMode == AUTH_MODE_WPA2) {
				rsnUpdatePmkidCandidateList(prPrimaryBssDesc);
				if (prAisSpecBssInfo->u4PmkidCandicateCount) {
					if (rsnCheckPmkidCandicate()) {
						DBGLOG(RSN, WARN,
						       "Prepare a timer to indicate candidate %pM\n",
							(prAisSpecBssInfo->arPmkidCache
								[prAisSpecBssInfo->u4PmkidCacheCount].
								rBssidInfo.aucBssid)));
						cnmTimerStopTimer(&prAisSpecBssInfo->rPreauthenticationTimer);
						cnmTimerStartTimer(&prAisSpecBssInfo->rPreauthenticationTimer,
								   SEC_TO_MSEC
								   (WAIT_TIME_IND_PMKID_CANDICATE_SEC));
					}
				}
			}
#endif
			} else {
				/* Can't pass the Encryption Status Check, get next one */
				continue;
			}
#endif
		} else {
			/* Todo:: P2P and BOW Policy Selection */
		}

		prPrimaryStaRec = prStaRec;

		/* 4 <8> Compare the Candidate and the Primary Scan Record. */
		if (!prCandidateBssDesc) {
			prCandidateBssDesc = prPrimaryBssDesc;
			prCandidateStaRec = prPrimaryStaRec;

			/* 4 <8.1> Condition - Get the first matched one. */
			if (fgIsFindFirst)
				break;
		} else {
#if 0				/* TODO(Kevin): For security(TBD) */
	/* 4 <6B> Condition - Choose the one with best Encryption Score. */
	if (fgIsFindBestEncryptionLevel) {
		if (prCandidateBssDesc->ucEncLevel < prPrimaryBssDesc->ucEncLevel) {

			prCandidateBssDesc = prPrimaryBssDesc;
			prCandidateStaRec = prPrimaryStaRec;
			continue;
		}
	}

	/* If reach here, that means they have the same Encryption Score.
	 */

	/* 4 <6C> Condition - Give opportunity to the one we didn't connect before. */
	/* For roaming, only compare the candidates other than current associated BSSID. */
	if (!prCandidateBssDesc->fgIsConnected && !prPrimaryBssDesc->fgIsConnected) {
		if ((prCandidateStaRec != (P_STA_RECORD_T) NULL) &&
		    (prCandidateStaRec->u2StatusCode != STATUS_CODE_SUCCESSFUL)) {

			DBGLOG(SCAN, TRACE,
			       "So far -BSS DESC MAC: %pM has nonzero Status Code = %d\n",
				prCandidateBssDesc->aucBSSID,
				prCandidateStaRec->u2StatusCode);

			if (prPrimaryStaRec != (P_STA_RECORD_T) NULL) {
				if (prPrimaryStaRec->u2StatusCode != STATUS_CODE_SUCCESSFUL) {

					/* Give opportunity to the one with smaller rLastJoinTime */
					if (TIME_BEFORE(prCandidateStaRec->rLastJoinTime,
							prPrimaryStaRec->rLastJoinTime)) {
						continue;
					}
					/* We've connect to CANDIDATE recently,
					 * let us try PRIMARY now */
					else {
						prCandidateBssDesc = prPrimaryBssDesc;
						prCandidateStaRec = prPrimaryStaRec;
						continue;
					}
				}
				/* PRIMARY's u2StatusCode = 0 */
				else {
					prCandidateBssDesc = prPrimaryBssDesc;
					prCandidateStaRec = prPrimaryStaRec;
					continue;
				}
			}
			/* PRIMARY has no StaRec - We didn't connet to PRIMARY before */
			else {
				prCandidateBssDesc = prPrimaryBssDesc;
				prCandidateStaRec = prPrimaryStaRec;
				continue;
			}
		} else {
			if ((prPrimaryStaRec != (P_STA_RECORD_T) NULL) &&
			    (prPrimaryStaRec->u2StatusCode != STATUS_CODE_SUCCESSFUL)) {
				continue;
			}
		}
	}
#endif

			/* 4 <6D> Condition - Visible SSID win Hidden SSID. */
			if (prCandidateBssDesc->fgIsHiddenSSID) {
				if (!prPrimaryBssDesc->fgIsHiddenSSID) {
					prCandidateBssDesc = prPrimaryBssDesc;	/* The non Hidden SSID win. */
					prCandidateStaRec = prPrimaryStaRec;
					continue;
				}
			} else {
				if (prPrimaryBssDesc->fgIsHiddenSSID)
					continue;
			}

			/* 4 <6E> Condition - Choose the one with better RCPI(RSSI). */
			if (fgIsFindBestRSSI) {
				/* TODO(Kevin): We shouldn't compare the actual value, we should
				 * allow some acceptable tolerance of some RSSI percentage here.
				 */
				INT_32 u4PrimAdjRssi = scanResultsAdjust5GPref(prPrimaryBssDesc);
				INT_32 u4CandAdjRssi = scanResultsAdjust5GPref(prCandidateBssDesc);

				DBGLOG(SCN, TRACE,
					"Candidate [" MACSTR "]: RCPI=%d, RSSI=%d, joinFailCnt=%d, Primary ["
					MACSTR "]: RCPI=%d, RSSI=%d, joinFailCnt=%d\n",
					MAC2STR(prCandidateBssDesc->aucBSSID),
					prCandidateBssDesc->ucRCPI, u4CandAdjRssi,
					prCandidateBssDesc->ucJoinFailureCount,
					MAC2STR(prPrimaryBssDesc->aucBSSID),
					prPrimaryBssDesc->ucRCPI, u4PrimAdjRssi,
					prPrimaryBssDesc->ucJoinFailureCount);

				ASSERT(!(prCandidateBssDesc->fgIsConnected && prPrimaryBssDesc->fgIsConnected));
				if (prPrimaryBssDesc->ucJoinFailureCount >= SCN_BSS_JOIN_FAIL_THRESOLD) {
					/* give a chance to do join if join fail before
					 * SCN_BSS_DECRASE_JOIN_FAIL_CNT_SEC seconds
					 */
					if (CHECK_FOR_TIMEOUT(rCurrentTime, prBssDesc->rJoinFailTime,
							SEC_TO_SYSTIME(SCN_BSS_JOIN_FAIL_CNT_RESET_SEC))) {
						prBssDesc->ucJoinFailureCount = SCN_BSS_JOIN_FAIL_THRESOLD -
										SCN_BSS_JOIN_FAIL_RESET_STEP;
						DBGLOG(SCN, INFO,
						"decrease join fail count for Bss %pM to %u, timeout second %d\n",
							prBssDesc->aucBSSID, prBssDesc->ucJoinFailureCount,
							SCN_BSS_JOIN_FAIL_CNT_RESET_SEC);
					}
				}

				/* NOTE: To prevent SWING,
				 * we do roaming only if target AP has at least 5dBm larger than us. */
				if (prCandidateBssDesc->fgIsConnected) {
					if (u4CandAdjRssi < u4PrimAdjRssi &&
						prPrimaryBssDesc->ucJoinFailureCount < SCN_BSS_JOIN_FAIL_THRESOLD) {
						prCandidateBssDesc = prPrimaryBssDesc;
						prCandidateStaRec = prPrimaryStaRec;
						continue;
					}
				} else if (prPrimaryBssDesc->fgIsConnected) {
					if (u4CandAdjRssi < u4PrimAdjRssi ||
						(prCandidateBssDesc->ucJoinFailureCount >=
						SCN_BSS_JOIN_FAIL_THRESOLD)) {
						prCandidateBssDesc = prPrimaryBssDesc;
						prCandidateStaRec = prPrimaryStaRec;
						continue;
					}
				} else if (prPrimaryBssDesc->ucJoinFailureCount >= SCN_BSS_JOIN_FAIL_THRESOLD)
					continue;
				else if (prCandidateBssDesc->ucJoinFailureCount >= SCN_BSS_JOIN_FAIL_THRESOLD ||
					u4CandAdjRssi < u4PrimAdjRssi) {
					prCandidateBssDesc = prPrimaryBssDesc;
					prCandidateStaRec = prPrimaryStaRec;
					continue;
				}
			}
#if 0
			/* If reach here, that means they have the same Encryption Score, and
			 * both RSSI value are close too.
			 */
			/* 4 <6F> Seek the minimum Channel Load for less interference. */
			if (fgIsFindMinChannelLoad) {
				/* Do nothing */
				/* TODO(Kevin): Check which one has minimum channel load in its channel */
			}
#endif
		}
	}


	if (prCandidateBssDesc != NULL) {
		DBGLOG(SCN, INFO,
		       "SEARCH: Candidate BSS: %pM\n", prCandidateBssDesc->aucBSSID);
	}

	return prCandidateBssDesc;

} /* end of scanSearchBssDescByPolicy() */

#if CFG_SUPPORT_AGPS_ASSIST
VOID scanReportScanResultToAgps(P_ADAPTER_T prAdapter)
{
	P_LINK_T prBSSDescList = &prAdapter->rWifiVar.rScanInfo.rBSSDescList;
	P_BSS_DESC_T prBssDesc = NULL;
	P_AGPS_AP_LIST_T prAgpsApList;
	P_AGPS_AP_INFO_T prAgpsInfo;
	P_SCAN_INFO_T prScanInfo = &prAdapter->rWifiVar.rScanInfo;
	UINT_8 ucIndex = 0;

	prAgpsApList = kalMemAlloc(sizeof(AGPS_AP_LIST_T), VIR_MEM_TYPE);
	if (!prAgpsApList)
		return;

	prAgpsInfo = &prAgpsApList->arApInfo[0];
	LINK_FOR_EACH_ENTRY(prBssDesc, prBSSDescList, rLinkEntry, BSS_DESC_T) {
		if (prBssDesc->rUpdateTime < prScanInfo->rLastScanCompletedTime)
			continue;
		COPY_MAC_ADDR(prAgpsInfo->aucBSSID, prBssDesc->aucBSSID);
		prAgpsInfo->ePhyType = AGPS_PHY_G;
		prAgpsInfo->u2Channel = prBssDesc->ucChannelNum;
		prAgpsInfo->i2ApRssi = RCPI_TO_dBm(prBssDesc->ucRCPI);
		prAgpsInfo++;
		ucIndex++;
		if (ucIndex == 32)
			break;
	}
	prAgpsApList->ucNum = ucIndex;
	GET_CURRENT_SYSTIME(&prScanInfo->rLastScanCompletedTime);
	/* DBGLOG(SCN, INFO, ("num of scan list:%d\n", ucIndex)); */
	kalIndicateAgpsNotify(prAdapter, AGPS_EVENT_WLAN_AP_LIST, (PUINT_8) prAgpsApList, sizeof(AGPS_AP_LIST_T));
	kalMemFree(prAgpsApList, VIR_MEM_TYPE, sizeof(AGPS_AP_LIST_T));
}
#endif

VOID scanGetCurrentEssChnlList(P_ADAPTER_T prAdapter)
{
	P_BSS_DESC_T prBssDesc = NULL;
	P_LINK_T prBSSDescList = &prAdapter->rWifiVar.rScanInfo.rBSSDescList;
	P_CONNECTION_SETTINGS_T prConnSettings = &prAdapter->rWifiVar.rConnSettings;
	struct ESS_CHNL_INFO *prEssChnlInfo = &prAdapter->rWifiVar.rAisSpecificBssInfo.arCurEssChnlInfo[0];
	P_LINK_T prCurEssLink = &prAdapter->rWifiVar.rAisSpecificBssInfo.rCurEssLink;
	UINT_8 aucChnlBitMap[30] = {0,};
	UINT_8 aucChnlApNum[215] = {0,};
	UINT_8 aucChnlUtil[215] = {0,};
	UINT_8 ucByteNum = 0;
	UINT_8 ucBitNum = 0;
	UINT_8 ucChnlCount = 0;
	UINT_8 j = 0;
	/*UINT_8 i = 0;*/

	if (prConnSettings->ucSSIDLen == 0) {
		DBGLOG(SCN, INFO, "No Ess are expected to connect\n");
		return;
	}
	kalMemZero(prEssChnlInfo, CFG_MAX_NUM_OF_CHNL_INFO * sizeof(struct ESS_CHNL_INFO));
	while (!LINK_IS_EMPTY(prCurEssLink)) {
		prBssDesc = LINK_PEEK_HEAD(prCurEssLink, BSS_DESC_T, rLinkEntryEss);
		LINK_REMOVE_KNOWN_ENTRY(prCurEssLink, &prBssDesc->rLinkEntryEss);
	}
	LINK_FOR_EACH_ENTRY(prBssDesc, prBSSDescList, rLinkEntry, BSS_DESC_T) {
		if (prBssDesc->ucChannelNum > 214)
			continue;
		/* Statistic AP num for each channel */
		if (aucChnlApNum[prBssDesc->ucChannelNum] < 255)
			aucChnlApNum[prBssDesc->ucChannelNum]++;
		if (aucChnlUtil[prBssDesc->ucChannelNum] < prBssDesc->ucChnlUtilization)
			aucChnlUtil[prBssDesc->ucChannelNum] = prBssDesc->ucChnlUtilization;
		if (!EQUAL_SSID(prConnSettings->aucSSID, prConnSettings->ucSSIDLen,
			prBssDesc->aucSSID, prBssDesc->ucSSIDLen))
			continue;
		/* Record same BSS list */
		LINK_INSERT_HEAD(prCurEssLink, &prBssDesc->rLinkEntryEss);
		ucByteNum = prBssDesc->ucChannelNum / 8;
		ucBitNum = prBssDesc->ucChannelNum % 8;
		if (aucChnlBitMap[ucByteNum] & BIT(ucBitNum))
			continue;
		aucChnlBitMap[ucByteNum] |= BIT(ucBitNum);
		prEssChnlInfo[ucChnlCount].ucChannel = prBssDesc->ucChannelNum;
		ucChnlCount++;
		if (ucChnlCount >= CFG_MAX_NUM_OF_CHNL_INFO)
			break;
	}
	prAdapter->rWifiVar.rAisSpecificBssInfo.ucCurEssChnlInfoNum = ucChnlCount;
	for (j = 0; j < ucChnlCount; j++) {
		UINT_8 ucChnl = prEssChnlInfo[j].ucChannel;

		prEssChnlInfo[j].ucApNum = aucChnlApNum[ucChnl];
		prEssChnlInfo[j].ucUtilization = aucChnlUtil[ucChnl];
	}
#if 0
	/* Sort according to AP number */
	for (j = 0; j < ucChnlCount; j++) {
		for (i = j + 1; i < ucChnlCount; i++)
			if (prEssChnlInfo[j].ucApNum > prEssChnlInfo[i].ucApNum) {
				struct ESS_CHNL_INFO rTemp = prEssChnlInfo[j];

				prEssChnlInfo[j] = prEssChnlInfo[i];
				prEssChnlInfo[i] = rTemp;
			}
	}
#endif
	DBGLOG(SCN, INFO, "Find %s in %d BSSes, result %d\n",
		prConnSettings->aucSSID, prBSSDescList->u4NumElem, prCurEssLink->u4NumElem);
}

#define CALCULATE_SCORE_BY_PROBE_RSP(prBssDesc) \
	(WEIGHT_IDX_PROBE_RSP * (prBssDesc->fgSeenProbeResp ? BSS_FULL_SCORE : 0))

#define CALCULATE_SCORE_BY_MISS_CNT(prAdapter, prBssDesc) \
	(WEIGHT_IDX_SCN_MISS_CNT * \
	(prAdapter->rWifiVar.rScanInfo.u4ScanUpdateIdx - prBssDesc->u4UpdateIdx > 3 ? 0 : \
	(BSS_FULL_SCORE - (prAdapter->rWifiVar.rScanInfo.u4ScanUpdateIdx - prBssDesc->u4UpdateIdx) * 25)))

#define CALCULATE_SCORE_BY_BAND(prAdapter, prBssDesc, cRssi) \
	(WEIGHT_IDX_5G_BAND * \
	((prBssDesc->eBand == BAND_5G && prAdapter->fgEnable5GBand && cRssi > -70) ? BSS_FULL_SCORE : 0))

#define CALCULATE_SCORE_BY_STBC(prAdapter, prBssDesc) \
	(WEIGHT_IDX_STBC * \
	((prBssDesc->fgMultiAnttenaAndSTBC && prAdapter->rWifiVar.ucRxStbc) ? BSS_FULL_SCORE:0))

#define CALCULATE_SCORE_BY_DEAUTH(prBssDesc) \
	(WEIGHT_IDX_DEAUTH_LAST * (prBssDesc->fgDeauthLastTime ? 0:BSS_FULL_SCORE))

#if 0/* we don't take it into account now */
/* Channel Utilization: weight index will be */
static UINT_16 scanCalculateScoreByChnlInfo(
	P_AIS_SPECIFIC_BSS_INFO_T prAisSpecificBssInfo, UINT_8 ucChannel)
{
	struct ESS_CHNL_INFO *prEssChnlInfo = &prAisSpecificBssInfo->arCurEssChnlInfo[0];
	UINT_8 i = 0;
	UINT_16 u2Score = 0;

	for (; i < prAisSpecificBssInfo->ucCurEssChnlInfoNum; i++) {
		if (ucChannel == prEssChnlInfo[i].ucChannel) {
#if 0	/* currently, we don't take channel utilization into account */
			/* the channel utilization max value is 255. great utilization means little weight value.
			the step of weight value is 2.6 */
			u2Score = WEIGHT_IDX_CHNL_UTIL *
				(BSS_FULL_SCORE - (prEssChnlInfo[i].ucUtilization * 10 / 26));
#endif
			/* if AP num on this channel is greater than 100, the weight will be 0.
			otherwise, the weight value decrease 1 if AP number increase 1 */
			if (prEssChnlInfo[i].ucApNum <= CHNL_BSS_NUM_THRESOLD)
				u2Score += WEIGHT_IDX_AP_NUM *
					(BSS_FULL_SCORE - prEssChnlInfo[i].ucApNum * SCORE_PER_AP);
			DBGLOG(SCN, INFO, "channel %d, AP num %d\n", ucChannel, prEssChnlInfo[i].ucApNum);
			break;
		}
	}
	return u2Score;
}

static UINT_16 scanCalculateScoreByBandwidth(P_ADAPTER_T prAdapter, P_BSS_DESC_T prBssDesc)
{
	UINT_16 u2Score = 0;
	ENUM_CHANNEL_WIDTH_T eChannelWidth = prBssDesc->eChannelWidth;
	UINT_8 ucSta5GBW = prAdapter->rWifiVar.ucSta5gBandwidth;
	UINT_8 ucSta2GBW = prAdapter->rWifiVar.ucSta2gBandwidth;
	UINT_8 ucStaBW = prAdapter->rWifiVar.ucStaBandwidth;

	if (prBssDesc->fgIsVHTPresent && prAdapter->fgEnable5GBand) {
		if (ucSta5GBW > ucStaBW)
			ucSta5GBW = ucStaBW;
		switch (ucSta5GBW) {
		case MAX_BW_20MHZ:
		case MAX_BW_40MHZ:
			eChannelWidth = CW_20_40MHZ;
			break;
		case MAX_BW_80MHZ:
			eChannelWidth = CW_80MHZ;
			break;
		}
		switch (eChannelWidth) {
		case CW_20_40MHZ:
			u2Score = 60;
			break;
		case CW_80MHZ:
			u2Score = 80;
			break;
		case CW_160MHZ:
		case CW_80P80MHZ:
			u2Score = BSS_FULL_SCORE;
			break;
		}
	} else if (prBssDesc->fgIsHTPresent) {
		if (prBssDesc->eBand == BAND_2G4) {
			if (ucSta2GBW > ucStaBW)
				ucSta2GBW = ucStaBW;
			u2Score = (prBssDesc->eSco == 0 || ucSta2GBW == MAX_BW_20MHZ) ? 40:60;
		} else if (prBssDesc->eBand == BAND_5G) {
			if (ucSta5GBW > ucStaBW)
				ucSta5GBW = ucStaBW;
			u2Score = (prBssDesc->eSco == 0 || ucSta5GBW == MAX_BW_20MHZ) ? 40:60;
		}
	} else if (prBssDesc->u2BSSBasicRateSet & RATE_SET_OFDM)
		u2Score = 20;
	else
		u2Score = 10;
	/*DBGLOG(SCN, INFO, "eCW %d, vht %d, ht %d, eband %d, esco %d, u2Score %d\n",
		eChannelWidth, prBssDesc->fgIsVHTPresent,
		prBssDesc->fgIsHTPresent, prBssDesc->eBand, prBssDesc->eSco, u2Score);*/
	return u2Score * WEIGHT_IDX_BAND_WIDTH;
}

static UINT_16 scanCalculateScoreByClientCnt(P_BSS_DESC_T prBssDesc)
{
	UINT_16 u2Score = 0;

	if (!prBssDesc->fgExsitBssLoadIE || prBssDesc->u2StaCnt > BSS_STA_CNT_THRESOLD) {
		DBGLOG(SCN, TRACE, "exist bss load %d, sta cnt %d\n",
			prBssDesc->fgExsitBssLoadIE, prBssDesc->u2StaCnt);
		return 0;
	}
	u2Score = BSS_FULL_SCORE - prBssDesc->u2StaCnt * 3;
	return u2Score * WEIGHT_IDX_CLIENT_CNT;
}

static UINT_16 scanCalculateScoreBySnrRssi(P_BSS_DESC_T prBssDesc)
{
	UINT_16 u2Score = 0;
	INT_8 cRssi = RCPI_TO_dBm(prBssDesc->ucRCPI);
	/*UINT_8 ucSNR = prBssDesc->ucSNR;*/

	DBGLOG(SCN, INFO, "cRSSI %d\n", cRssi);
	if (cRssi >= -20)
		u2Score = 60;
	else if (cRssi <= -70 && cRssi > HARD_TO_CONNECT_RSSI_THRESOLD)
		u2Score = 20;
	else if (cRssi <= HARD_TO_CONNECT_RSSI_THRESOLD)
		u2Score = 0;
	u2Score = 8 * ((cRssi + 69)/5) + 28;
	u2Score *= WEIGHT_IDX_RSSI;

	/* TODO: we don't know the valid value for SNR, so don't take it into account */
	return u2Score;
}
#endif
static BOOLEAN scanSanityCheckBssDesc(P_ADAPTER_T prAdapter,
	P_BSS_DESC_T prBssDesc, ENUM_BAND_T eBand, UINT_8 ucChannel, BOOLEAN fgIsFixedChannel)
{
	if (!(prBssDesc->ucPhyTypeSet & (prAdapter->rWifiVar.ucAvailablePhyTypeSet))) {
		DBGLOG(SCN, WARN, "SEARCH: Ignore unsupported ucPhyTypeSet = %x\n",
			prBssDesc->ucPhyTypeSet);
		return FALSE;
	}
	if (prBssDesc->fgIsUnknownBssBasicRate)
		return FALSE;
	if (fgIsFixedChannel &&
		(eBand != prBssDesc->eBand || ucChannel != prBssDesc->ucChannelNum)) {
		DBGLOG(SCN, INFO, "Fix channel required band %d, channel %d\n", eBand, ucChannel);
		return FALSE;
	}
	if (!rlmDomainIsLegalChannel(prAdapter, prBssDesc->eBand, prBssDesc->ucChannelNum)) {
		DBGLOG(SCN, WARN, "Band %d channel %d is not legal\n",
			prBssDesc->eBand, prBssDesc->ucChannelNum);
		return FALSE;
	}
#if CFG_SUPPORT_WAPI
	if (prAdapter->rWifiVar.rConnSettings.fgWapiMode) {
		if (!wapiPerformPolicySelection(prAdapter, prBssDesc))
			return FALSE;
	} else
#endif
	if (!rsnPerformPolicySelection(prAdapter, prBssDesc))
		return FALSE;
	if (prAdapter->rWifiVar.rAisSpecificBssInfo.fgCounterMeasure) {
		DBGLOG(SCN, WARN, "Skip while at counter measure period!!!\n");
		return FALSE;
	}
	return TRUE;
}
static UINT_16 scanCalculateScoreByRssi(P_BSS_DESC_T prBssDesc)
{
	UINT_16 u2Score = 0;
	INT_8 cRssi = RCPI_TO_dBm(prBssDesc->ucRCPI);

	if (cRssi >= -20)
		u2Score = BSS_FULL_SCORE;
	else if (cRssi >= -55)
		u2Score = 95;
	else if (cRssi >= -65)
		u2Score = 90;
	else if (cRssi >= -70)
		u2Score = 85;
	else if (cRssi >= -77)
		u2Score = 30;
	else if (cRssi <= -88 && cRssi > -100)
		u2Score = 20;
	else if (cRssi <= -100)
		u2Score = 0;

	u2Score *= WEIGHT_IDX_RSSI;

	return u2Score;
}
/*****
Bss Characteristics to be taken into account when calculate Score:
Channel Loading Group:
1. Client Count (in BSS Load IE).
2. AP number on the Channel.

RF Group:
1. Channel utilization.
2. SNR.
3. RSSI.

Misc Group:
1. Deauth Last time.
2. Scan Missing Count.
3. Has probe response in scan result.

Capability Group:
1. Prefer 5G band.
2. Bandwidth.
3. STBC and Multi Anttena.
*/
P_BSS_DESC_T scanSearchBssDescByScoreForAis(P_ADAPTER_T prAdapter)
{
	P_AIS_SPECIFIC_BSS_INFO_T prAisSpecificBssInfo = NULL;
	P_LINK_T prEssLink = NULL;
	P_CONNECTION_SETTINGS_T prConnSettings = NULL;
	P_BSS_DESC_T prBssDesc = NULL;
	P_BSS_DESC_T prCandBssDesc = NULL;
	P_BSS_DESC_T prCandBssDescForLowRssi = NULL;
	UINT_16 u2ScoreBand = 0;
	UINT_16 u2ScoreChnlInfo = 0;
	UINT_16 u2ScoreStaCnt = 0;
	UINT_16 u2ScoreProbeRsp = 0;
	UINT_16 u2ScoreScanMiss = 0;
	UINT_16 u2ScoreBandwidth = 0;
	UINT_16 u2ScoreSTBC = 0;
	UINT_16 u2ScoreDeauth = 0;
	UINT_16 u2ScoreSnrRssi = 0;
	UINT_16 u2ScoreTotal = 0;
	UINT_16 u2CandBssScore = 0;
	UINT_16 u2CandBssScoreForLowRssi = 0;
	UINT_16 u2BlackListScore = 0;
	BOOLEAN fgSearchBlackList = FALSE;
	BOOLEAN fgIsFixedChannel = FALSE;
	ENUM_BAND_T eBand = BAND_2G4;
	UINT_8 ucChannel = 0;
	INT_8 cRssi = -128;
#if CFG_SELECT_BSS_BASE_ON_RSSI
	INT_8 cMaxRssi = HARD_TO_CONNECT_RSSI_THRESOLD;
#endif

	if (!prAdapter) {
		DBGLOG(SCN, ERROR, "prAdapter is NULL!\n");
		return NULL;
	}
	prAisSpecificBssInfo = &prAdapter->rWifiVar.rAisSpecificBssInfo;
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);
	prEssLink = &prAisSpecificBssInfo->rCurEssLink;

#if CFG_SUPPORT_CHNL_CONFLICT_REVISE
	fgIsFixedChannel = cnmAisDetectP2PChannel(prAdapter, &eBand, &ucChannel);
#else
	fgIsFixedChannel = cnmAisInfraChannelFixed(prAdapter, &eBand, &ucChannel);
#endif
	aisRemoveTimeoutBlacklist(prAdapter);
#if CFG_SELECT_BSS_BASE_ON_RSSI
	if (prConnSettings->eConnectionPolicy != CONNECT_BY_BSSID) {
		LINK_FOR_EACH_ENTRY(prBssDesc, prEssLink, rLinkEntryEss, BSS_DESC_T) {
			if (!scanSanityCheckBssDesc(prAdapter, prBssDesc, eBand, ucChannel, fgIsFixedChannel))
				continue;

			cRssi = RCPI_TO_dBm(prBssDesc->ucRCPI);
			if (cRssi > cMaxRssi)
				cMaxRssi = cRssi;
		}
	}
	DBGLOG(SCN, INFO, "Max RSSI %d, ConnectionPolicy =%d\n",
		cMaxRssi,
		prConnSettings->eConnectionPolicy);
#endif


try_again:
	LINK_FOR_EACH_ENTRY(prBssDesc, prEssLink, rLinkEntryEss, BSS_DESC_T) {
		if (prConnSettings->eConnectionPolicy == CONNECT_BY_BSSID &&
			EQUAL_MAC_ADDR(prBssDesc->aucBSSID, prConnSettings->aucBSSID)) {
			if (!scanSanityCheckBssDesc(prAdapter, prBssDesc, eBand, ucChannel, fgIsFixedChannel))
				continue;

			prCandBssDesc = prBssDesc;
			break;
		}
		if (!fgSearchBlackList) {
			prBssDesc->prBlack = aisQueryBlackList(prAdapter, prBssDesc);
			if (prBssDesc->prBlack)
				continue;
		} else if (!prBssDesc->prBlack)
			continue;
		else
			u2BlackListScore = WEIGHT_IDX_BLACK_LIST *
				aisCalculateBlackListScore(prAdapter, prBssDesc);

		cRssi = RCPI_TO_dBm(prBssDesc->ucRCPI);
		DBGLOG(SCN, INFO, "cRSSI %d, %pM\n", cRssi, prBssDesc->aucBSSID);
#if CFG_SELECT_BSS_BASE_ON_RSSI
		if (cMaxRssi >= -55) {
			if (cRssi < -55)
				continue;
		} else if (cMaxRssi >= -65) {
			if (cRssi < -65)
				continue;
		} else if (cMaxRssi >= -77) {
			if (cRssi < -77)
				continue;
		} else if (cMaxRssi >= -88) {
			if (cRssi < -88)
				continue;
		} else if (cMaxRssi >= -100) {
			if (cRssi < -100)
				continue;
		}
#endif
		if (!scanSanityCheckBssDesc(prAdapter, prBssDesc, eBand, ucChannel, fgIsFixedChannel))
			continue;

#if 0	/* currently, we don't take these factors into account */
		u2ScoreBandwidth = scanCalculateScoreByBandwidth(prAdapter, prBssDesc);
		u2ScoreStaCnt = scanCalculateScoreByClientCnt(prBssDesc);
		u2ScoreSTBC = CALCULATE_SCORE_BY_STBC(prAdapter, prBssDesc);
		u2ScoreChnlInfo = scanCalculateScoreByChnlInfo(prAisSpecificBssInfo, prBssDesc->ucChannelNum);
		u2ScoreSnrRssi = scanCalculateScoreBySnrRssi(prBssDesc);
#endif
		u2ScoreSnrRssi = scanCalculateScoreByRssi(prBssDesc);
		u2ScoreDeauth = CALCULATE_SCORE_BY_DEAUTH(prBssDesc);
		u2ScoreProbeRsp = CALCULATE_SCORE_BY_PROBE_RSP(prBssDesc);
		u2ScoreScanMiss = CALCULATE_SCORE_BY_MISS_CNT(prAdapter, prBssDesc);
		u2ScoreBand = CALCULATE_SCORE_BY_BAND(prAdapter, prBssDesc, cRssi);
		u2ScoreTotal = u2ScoreBandwidth + u2ScoreChnlInfo + u2ScoreDeauth + u2ScoreProbeRsp +
			u2ScoreScanMiss + u2ScoreSnrRssi + u2ScoreStaCnt + u2ScoreSTBC + u2ScoreBand + u2BlackListScore;

		DBGLOG(SCN, INFO,
			"%pM Score, Total %d: BW[%d], CI[%d], DE[%d], PR[%d], SM[%d], SC[%d], SR[%d], ST[%d], BD[%d]\n",
			prBssDesc->aucBSSID, u2ScoreTotal, u2ScoreBandwidth, u2ScoreChnlInfo, u2ScoreDeauth,
			u2ScoreProbeRsp, u2ScoreScanMiss, u2ScoreStaCnt, u2ScoreSnrRssi, u2ScoreSTBC, u2ScoreBand);
		/*if (cRssi < HARD_TO_CONNECT_RSSI_THRESOLD) {
			if (!prCandBssDescForLowRssi) {
				prCandBssDescForLowRssi = prBssDesc;
				u2CandBssScoreForLowRssi = u2ScoreTotal;
			} else if (prCandBssDescForLowRssi->fgIsConnected) {
				if ((u2CandBssScoreForLowRssi + ROAMING_NO_SWING_SCORE_STEP) < u2ScoreTotal) {
					prCandBssDescForLowRssi = prBssDesc;
					u2CandBssScoreForLowRssi = u2ScoreTotal;
				}
			} else if (prBssDesc->fgIsConnected) {
				if (u2CandBssScoreForLowRssi > (u2ScoreTotal + ROAMING_NO_SWING_SCORE_STEP)) {
					prCandBssDescForLowRssi = prBssDesc;
					u2CandBssScoreForLowRssi = u2ScoreTotal;
				}
			} else if (u2CandBssScoreForLowRssi < u2ScoreTotal) {
				prCandBssDescForLowRssi = prBssDesc;
				u2CandBssScoreForLowRssi = u2ScoreTotal;
			}
		} else */if (!prCandBssDesc) {
			prCandBssDesc = prBssDesc;
			u2CandBssScore = u2ScoreTotal;
		} else if (prCandBssDesc->fgIsConnected) {
			if ((u2CandBssScore + ROAMING_NO_SWING_SCORE_STEP) < u2ScoreTotal) {
				prCandBssDesc = prBssDesc;
				u2CandBssScore = u2ScoreTotal;
			}
		} else if (prBssDesc->fgIsConnected) {
			if (u2CandBssScore <= (u2ScoreTotal + ROAMING_NO_SWING_SCORE_STEP)) {
				prCandBssDesc = prBssDesc;
				u2CandBssScore = u2ScoreTotal;
			}
		} else if (u2CandBssScore < u2ScoreTotal) {
			prCandBssDesc = prBssDesc;
			u2CandBssScore = u2ScoreTotal;
		}
	}

	if (prCandBssDesc) {
		if (prCandBssDesc->fgIsConnected && !fgSearchBlackList && prEssLink->u4NumElem > 0) {
			fgSearchBlackList = TRUE;
			DBGLOG(SCN, INFO, "Can't roam out, try blacklist\n");
			goto try_again;
		}
		if (prConnSettings->eConnectionPolicy == CONNECT_BY_BSSID)
			DBGLOG(SCN, INFO,
				"Selected %pM base on bssid, when find %s, %pM in %d BSSes, fix channel %d.\n",
				prCandBssDesc->aucBSSID, prConnSettings->aucSSID,
				prConnSettings->aucBSSID, prEssLink->u4NumElem, ucChannel);
		else
			DBGLOG(SCN, INFO,
				"Selected %pM, Score %d when find %s, %pM in %d BSSes, fix channel %d.\n",
				prCandBssDesc->aucBSSID, u2CandBssScore,
				prConnSettings->aucSSID, prConnSettings->aucBSSID, prEssLink->u4NumElem, ucChannel);
		return prCandBssDesc;
	} else if (prCandBssDescForLowRssi) {
		DBGLOG(SCN, INFO, "Selected %pM, Score %d when find %s, %pM in %d BSSes, fix channel %d.\n",
			prCandBssDescForLowRssi->aucBSSID, u2CandBssScoreForLowRssi,
			prConnSettings->aucSSID, prConnSettings->aucBSSID, prEssLink->u4NumElem, ucChannel);
		return prCandBssDescForLowRssi;
	}

	/* if No Candidate BSS is found, try BSSes which are in blacklist */
	if (!fgSearchBlackList && prEssLink->u4NumElem > 0) {
		fgSearchBlackList = TRUE;
		DBGLOG(SCN, INFO, "No Bss is found, Try blacklist\n");
		goto try_again;
	}

	DBGLOG(SCN, INFO, "Selected None when find %s, %pM in %d BSSes, fix channel %d.\n",
				prConnSettings->aucSSID, prConnSettings->aucBSSID, prEssLink->u4NumElem, ucChannel);
	return NULL;
}


