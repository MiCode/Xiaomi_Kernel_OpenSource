/*
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
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include "precomp.h"

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
#define REPLICATED_BEACON_STRENGTH_THRESHOLD    (32)
#define ROAMING_NO_SWING_RCPI_STEP              (10)
#define REPLICATED_BEACON_FRESH_PERIOD          (10000)
#define REPLICATED_BEACON_TIME_THRESHOLD        (3000)

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */
/* The order of aucScanLogPrefix should be aligned the order
 * of enum ENUM_SCAN_LOG_PREFIX
 */
const char aucScanLogPrefix[][SCAN_LOG_PREFIX_MAX_LEN] = {
	/* Scan */
	"[SCN:100:K2D]",	/* LOG_SCAN_REQ_K2D */
	"[SCN:200:D2F]",	/* LOG_SCAN_REQ_D2F */
	"[SCN:300:F2D]",	/* LOG_SCAN_RESULT_F2D */
	"[SCN:400:D2K]",	/* LOG_SCAN_RESULT_D2K */
	"[SCN:500:F2D]",	/* LOG_SCAN_DONE_F2D */
	"[SCN:600:D2K]",	/* LOG_SCAN_DONE_D2K */

	/* Sched scan */
	"[SCN:700:K2D]",	/* LOG_SCHED_SCAN_REQ_START_K2D */
	"[SCN:800:D2F]",        /* LOG_SCHED_SCAN_REQ_START_D2F */
	"[SCN:750:K2D]",	/* LOG_SCHED_SCAN_REQ_STOP_K2D */
	"[SCN:850:D2F]",	/* LOG_SCHED_SCAN_REQ_STOP_D2F */
	"[SCN:900:F2D]",	/* LOG_SCHED_SCAN_DONE_F2D */
	"[SCN:1000:D2K]",	/* LOG_SCHED_SCAN_DONE_D2K */

	/* Scan abort */
	"[SCN:1100:K2D]",	/* LOG_SCAN_ABORT_REQ_K2D */
	"[SCN:1200:D2F]",	/* LOG_SCAN_ABORT_REQ_D2F */
	"[SCN:1300:D2K]",	/* LOG_SCAN_ABORT_DONE_D2K */

	/* Driver only */
	"[SCN:0:D2D]",		/* LOG_SCAN_D2D */

	/* Last one */
	""			/* LOG_SCAN_MAX */
};

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */
/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
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
void scnInit(IN struct ADAPTER *prAdapter)
{
	struct SCAN_INFO *prScanInfo;
	struct BSS_DESC *prBSSDesc;
	uint8_t *pucBSSBuff;
	uint32_t i;

	ASSERT(prAdapter);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	pucBSSBuff = &prScanInfo->aucScanBuffer[0];

	log_dbg(SCN, TRACE, "->scnInit()\n");

	/* 4 <1> Reset STATE and Message List */
	prScanInfo->eCurrentState = SCAN_STATE_IDLE;

	prScanInfo->rLastScanCompletedTime = (OS_SYSTIME) 0;

	LINK_INITIALIZE(&prScanInfo->rPendingMsgList);

	/* 4 <2> Reset link list of BSS_DESC_T */
	kalMemZero((void *) pucBSSBuff, SCN_MAX_BUFFER_SIZE);

	LINK_INITIALIZE(&prScanInfo->rFreeBSSDescList);
	LINK_INITIALIZE(&prScanInfo->rBSSDescList);

	for (i = 0; i < CFG_MAX_NUM_BSS_LIST; i++) {

		prBSSDesc = (struct BSS_DESC *) pucBSSBuff;

		scanInsertBssDescToList(&prScanInfo->rFreeBSSDescList,
			prBSSDesc,
			FALSE);

		pucBSSBuff += ALIGN_4(sizeof(struct BSS_DESC));
	}
	/* Check if the memory allocation consist with
	 * this initialization function
	 */
	ASSERT(((unsigned long) pucBSSBuff
		- (unsigned long)&prScanInfo->aucScanBuffer[0])
		== SCN_MAX_BUFFER_SIZE);

	/* reset freest channel information */
	prScanInfo->fgIsSparseChannelValid = FALSE;

	prScanInfo->fgIsScanForFull2Partial = FALSE;

	/* reset Sched scan state */
	prScanInfo->fgSchedScanning = FALSE;
	/*Support AP Selection */
	prScanInfo->u4ScanUpdateIdx = 0;
}	/* end of scnInit() */

void scnFreeAllPendingScanRquests(IN struct ADAPTER *prAdapter)
{
	struct SCAN_INFO *prScanInfo;
	struct MSG_HDR *prMsgHdr;
	struct MSG_SCN_SCAN_REQ *prScanReqMsg;

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);

	/* check for pending scanning requests */
	while (!LINK_IS_EMPTY(&(prScanInfo->rPendingMsgList))) {

		/* load next message from pending list as scan parameters */
		LINK_REMOVE_HEAD(&(prScanInfo->rPendingMsgList),
			prMsgHdr, struct MSG_HDR *);
		if (prMsgHdr) {
			prScanReqMsg = (struct MSG_SCN_SCAN_REQ *) prMsgHdr;

			log_dbg(SCN, INFO, "Free scan request eMsgId[%d] ucSeqNum [%d] BSSID[%d]!!\n",
				prMsgHdr->eMsgId,
				prScanReqMsg->ucSeqNum,
				prScanReqMsg->ucBssIndex);

			cnmMemFree(prAdapter, prMsgHdr);
		} else {
			/* should not deliver to this function */
			ASSERT(0);
		}
		/* switch to next state */
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is used by SCN to uninitialize its variables
 *
 * @param (none)
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void scnUninit(IN struct ADAPTER *prAdapter)
{
	struct SCAN_INFO *prScanInfo;

	ASSERT(prAdapter);
	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);

	log_dbg(SCN, INFO, "%s()\n", __func__);

	scnFreeAllPendingScanRquests(prAdapter);

	/* 4 <1> Reset STATE and Message List */
	prScanInfo->eCurrentState = SCAN_STATE_IDLE;

	prScanInfo->rLastScanCompletedTime = (OS_SYSTIME) 0;

	/* NOTE(Kevin): Check rPendingMsgList ? */

	/* 4 <2> Reset link list of BSS_DESC_T */
	LINK_INITIALIZE(&prScanInfo->rFreeBSSDescList);
	LINK_INITIALIZE(&prScanInfo->rBSSDescList);
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
struct BSS_DESC *scanSearchBssDescByBssid(IN struct ADAPTER *prAdapter,
	IN uint8_t aucBSSID[])
{
	return scanSearchBssDescByBssidAndSsid(prAdapter, aucBSSID,
		FALSE, NULL);
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief Check if the bit of the given bitmap is set
 * The bitmap should be unsigned int based, which means that this function
 * doesn't support other format bitmap, e.g. char array or short array
 *
 * @param[in] bit          which bit to check.
 * @param[in] bitMap       bitmap array
 * @param[in] bitMapSize   bytes of bitmap
 *
 * @return   TRUE if the bit of the given bitmap is set, FALSE otherwise
 */
/*----------------------------------------------------------------------------*/
u_int8_t scanIsBitSet(IN uint32_t bit, IN uint32_t bitMap[],
		IN uint32_t bitMapSize)
{
	if (bit >= bitMapSize * BITS_OF_BYTE) {
		log_dbg(SCN, WARN, "bit %u is out of array range(%u bits)\n",
			bit, bitMapSize * BITS_OF_BYTE);
		return FALSE;
	} else {
		return (bitMap[bit/BITS_OF_UINT] &
			(1 << (bit % BITS_OF_UINT))) ? TRUE : FALSE;
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief Set the bit of the given bitmap.
 * The bitmap should be unsigned int based, which means that this function
 * doesn't support other format bitmap, e.g. char array or short array
 *
 * @param[in] bit          which bit to set.
 * @param[out] bitMap      bitmap array
 * @param[in] bitMapSize   bytes of bitmap
 *
 * @return   void
 */
/*----------------------------------------------------------------------------*/
void scanSetBit(IN uint32_t bit, OUT uint32_t bitMap[], IN uint32_t bitMapSize)
{
	if (bit >= bitMapSize * BITS_OF_BYTE) {
		log_dbg(SCN, WARN, "set bit %u to array(%u bits) failed\n",
			bit, bitMapSize * BITS_OF_BYTE);
	} else
		bitMap[bit/BITS_OF_UINT] |= 1 << (bit % BITS_OF_UINT);
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief Return number of bit which is set to 1 in the given bitmap.
 * The bitmap should be unsigned int based, which means that this function
 * doesn't support other format bitmap, e.g. char array or short array
 *
 * @param[in] bitMap       bitmap array
 * @param[in] bitMapSize   bytes of bitmap
 *
 * @return   number of bit which is set to 1
 */
/*----------------------------------------------------------------------------*/

uint32_t scanCountBits(IN uint32_t bitMap[], IN uint32_t bitMapSize)
{
	uint32_t count = 0;
	uint32_t value;
	int32_t arrayLen = bitMapSize/sizeof(uint32_t);
	int32_t i;

	for (i = arrayLen - 1; i >= 0; i--) {
		value = bitMap[i];
		log_dbg(SCN, TRACE, "array[%d]:%08X\n", i, value);
		while (value) {
			count += (value & 1);
			value >>= 1;
		}
	}
	return count;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief Set scan channel to scanReqMsg.
 *
 * @param[in]  prAdapter           Pointer to the Adapter structure.
 * @param[in]  u4ScanChannelNum    number of input channels
 * @param[in]  arChannel           channel list
 * @param[in]  fgIsOnlineScan      online scan or not
 * @param[out] prScanReqMsg        scan request msg. Set channel number and
 *                                 channel list for output
 *
 * @return
 */
/*----------------------------------------------------------------------------*/
void scanSetRequestChannel(IN struct ADAPTER *prAdapter,
		IN uint32_t u4ScanChannelNum,
		IN struct RF_CHANNEL_INFO arChannel[],
		IN uint32_t u4ScanFlags,
		IN uint8_t fgIsOnlineScan,
		OUT struct MSG_SCN_SCAN_REQ_V2 *prScanReqMsg)
{
	uint32_t i, u4Channel, eBand, u4Index;
	/*print channel info for debugging */
	uint32_t au4ChannelBitMap[SCAN_CHANNEL_BITMAP_ARRAY_LEN];
	struct SCAN_INFO *prScanInfo;
	bool fgIsLowSpanScan = FALSE;
#if CFG_SUPPORT_FULL2PARTIAL_SCAN
	uint8_t fgIsFull2Partial = FALSE;
	OS_SYSTIME rCurrentTime;

	GET_CURRENT_SYSTIME(&rCurrentTime);
#endif /* CFG_SUPPORT_FULL2PARTIAL_SCAN */

	ASSERT(u4ScanChannelNum <= MAXIMUM_OPERATION_CHANNEL_LIST);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	i = u4Index = 0;
	kalMemZero(au4ChannelBitMap, sizeof(au4ChannelBitMap));
	fgIsLowSpanScan = (u4ScanFlags & NL80211_SCAN_FLAG_LOW_SPAN) >> 8;

#if CFG_SUPPORT_FULL2PARTIAL_SCAN
	/* fgIsCheckingFull2Partial should be true if it's an online scan.
	 * Next, enable full2partial if channel number to scan is
	 * larger than SCAN_FULL2PARTIAL_CHANNEL_NUM
	 */
	if (fgIsOnlineScan && (u4ScanChannelNum == 0 ||
		u4ScanChannelNum > SCAN_FULL2PARTIAL_CHANNEL_NUM)) {

		/* Do full scan when
		 * 1. did not do full scan yet OR
		 * 2. not APP scan and it's been 60s after last full scan
		*/
		if (prScanInfo->u4LastFullScanTime == 0 ||
			(!fgIsLowSpanScan &&
			(CHECK_FOR_TIMEOUT(rCurrentTime,
			prScanInfo->u4LastFullScanTime,
			SEC_TO_SYSTIME(CFG_SCAN_FULL2PARTIAL_PERIOD))))) {
			prScanInfo->fgIsScanForFull2Partial = TRUE;
			prScanInfo->ucFull2PartialSeq = prScanReqMsg->ucSeqNum;
			prScanInfo->u4LastFullScanTime = rCurrentTime;
			kalMemZero(prScanInfo->au4ChannelBitMap,
				sizeof(prScanInfo->au4ChannelBitMap));
			log_dbg(SCN, INFO,
				"Full2partial: 1st full scan start, low span=%d\n",
				fgIsLowSpanScan);
		} else {
			log_dbg(SCN, INFO,
				"Full2partial: enable full2partial, low span=%d\n",
				fgIsLowSpanScan);
			fgIsFull2Partial = TRUE;
		}
	}

	if (u4ScanChannelNum == 0) {
		/* We don't have channel info when u4ScanChannelNum is 0.
		 * check full2partial bitmap and set scan channels
		 */
		uint32_t start = 1;
		uint32_t end = HW_CHNL_NUM_MAX_4G_5G;

		if (prScanReqMsg->eScanChannel == SCAN_CHANNEL_2G4)
			end = HW_CHNL_NUM_MAX_2G4;
		else if (prScanReqMsg->eScanChannel == SCAN_CHANNEL_5G)
			start = HW_CHNL_NUM_MAX_2G4 + 1;

		u4Index = 0;
		/* If partial scan list are too old, do full scan */
		if (!CHECK_FOR_TIMEOUT(rCurrentTime,
			prScanInfo->u4LastFullScanTime,
			SEC_TO_SYSTIME(CFG_SCAN_FULL2PARTIAL_PERIOD))) {
			for (u4Channel = start; u4Channel <= end; u4Channel++) {
				if (scanIsBitSet(u4Channel,
					prScanInfo->au4ChannelBitMap,
					sizeof(prScanInfo->au4ChannelBitMap))) {
					eBand = (u4Channel <=
						HW_CHNL_NUM_MAX_2G4) ?
						BAND_2G4 : BAND_5G;
					prScanReqMsg->arChnlInfoList[u4Index].
						ucChannelNum = u4Channel;
					prScanReqMsg->arChnlInfoList[u4Index].
						eBand = eBand;
					scanSetBit(u4Channel, au4ChannelBitMap,
						sizeof(au4ChannelBitMap));
					u4Index++;
				}
			}
		}

		prScanReqMsg->ucChannelListNum = u4Index;
		if (u4Index == 0) {
			log_dbg(SCN, WARN, "No channel to scan, set to full scan\n");
			prScanReqMsg->eScanChannel = SCAN_CHANNEL_FULL;
		} else
			prScanReqMsg->eScanChannel = SCAN_CHANNEL_SPECIFIED;
	} else
#endif /* CFG_SUPPORT_FULL2PARTIAL_SCAN */
	{
		u4Index = 0;
		for (i = 0; i < u4ScanChannelNum; i++) {
			u4Channel = arChannel[i].ucChannelNum;
			eBand = arChannel[i].eBand;
			if (prScanReqMsg->eScanChannel == SCAN_CHANNEL_2G4 &&
				eBand != BAND_2G4)
				continue;
			else if (prScanReqMsg->eScanChannel ==
				SCAN_CHANNEL_5G && eBand != BAND_5G)
				continue;
#if CFG_SUPPORT_FULL2PARTIAL_SCAN
			if (fgIsFull2Partial && !scanIsBitSet(u4Channel,
				prScanInfo->au4ChannelBitMap,
				sizeof(prScanInfo->au4ChannelBitMap)))
				continue;
#endif /* CFG_SUPPORT_FULL2PARTIAL_SCAN */
			kalMemCopy(&prScanReqMsg->arChnlInfoList[u4Index],
					&arChannel[i],
					sizeof(struct RF_CHANNEL_INFO));
			scanSetBit(u4Channel, au4ChannelBitMap,
				sizeof(au4ChannelBitMap));

			u4Index++;
		}
		if (u4Index == 0) {
			log_dbg(SCN, WARN, "No channel to scan, set to full scan\n");
			prScanReqMsg->ucChannelListNum = 0;
			prScanReqMsg->eScanChannel = SCAN_CHANNEL_FULL;
		} else {
			prScanReqMsg->ucChannelListNum = u4Index;
			prScanReqMsg->eScanChannel = SCAN_CHANNEL_SPECIFIED;
		}
	}

	log_dbg(SCN, INFO,
		"channel num(%u=>%u) %08X %08X %08X %08X %08X %08X %08X %08X\n",
		u4ScanChannelNum, prScanReqMsg->ucChannelListNum,
		au4ChannelBitMap[7], au4ChannelBitMap[6],
		au4ChannelBitMap[5], au4ChannelBitMap[4],
		au4ChannelBitMap[3], au4ChannelBitMap[2],
		au4ChannelBitMap[1], au4ChannelBitMap[0]);
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief Find the corresponding BSS Descriptor according to given BSSID
 *
 * @param[in] prAdapter          Pointer to the Adapter structure.
 * @param[in] aucBSSID           Given BSSID.
 * @param[in] fgCheckSsid        Need to check SSID or not. (for multiple SSID
 *                               with single BSSID cases)
 * @param[in] prSsid             Specified SSID
 *
 * @return   Pointer to BSS Descriptor, if found. NULL, if not found
 */
/*----------------------------------------------------------------------------*/
struct BSS_DESC *
scanSearchBssDescByBssidAndSsid(IN struct ADAPTER *prAdapter,
				IN uint8_t aucBSSID[],
				IN u_int8_t fgCheckSsid,
				IN struct PARAM_SSID *prSsid)
{
	struct SCAN_INFO *prScanInfo;
	struct LINK *prBSSDescList;
	struct BSS_DESC *prBssDesc;
	struct BSS_DESC *prDstBssDesc = (struct BSS_DESC *) NULL;

	ASSERT(prAdapter);
	ASSERT(aucBSSID);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);

	prBSSDescList = &prScanInfo->rBSSDescList;

	/* Search BSS Desc from current SCAN result list. */
	LINK_FOR_EACH_ENTRY(prBssDesc, prBSSDescList,
		rLinkEntry, struct BSS_DESC) {

		if (!(EQUAL_MAC_ADDR(prBssDesc->aucBSSID, aucBSSID)))
			continue;
		if (fgCheckSsid == FALSE || prSsid == NULL)
			return prBssDesc;
		if (EQUAL_SSID(prBssDesc->aucSSID, prBssDesc->ucSSIDLen,
				prSsid->aucSsid, prSsid->u4SsidLen)) {
			return prBssDesc;
		}
		if (prDstBssDesc == NULL && prBssDesc->fgIsHiddenSSID == TRUE) {
			prDstBssDesc = prBssDesc;
			continue;
		}
		if (prBssDesc->eBSSType == BSS_TYPE_P2P_DEVICE) {
			/* 20120206 frog: Equal BSSID but not SSID,
			 * SSID not hidden, SSID must be updated.
			 */
			COPY_SSID(prBssDesc->aucSSID, prBssDesc->ucSSIDLen,
				prSsid->aucSsid, (uint8_t) (prSsid->u4SsidLen));
			return prBssDesc;
		}
	}

	return prDstBssDesc;

}	/* end of scanSearchBssDescByBssid() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief Find the corresponding BSS Descriptor according to
 *        given Transmitter Address.
 *
 * @param[in] prAdapter          Pointer to the Adapter structure.
 * @param[in] aucSrcAddr         Given Source Address(TA).
 *
 * @return   Pointer to BSS Descriptor, if found. NULL, if not found
 */
/*----------------------------------------------------------------------------*/
struct BSS_DESC *scanSearchBssDescByTA(IN struct ADAPTER *prAdapter,
	IN uint8_t aucSrcAddr[])
{
	return scanSearchBssDescByTAAndSsid(prAdapter, aucSrcAddr, FALSE, NULL);
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief Find the corresponding BSS Descriptor according to
 *        given Transmitter Address.
 *
 * @param[in] prAdapter          Pointer to the Adapter structure.
 * @param[in] aucSrcAddr         Given Source Address(TA).
 * @param[in] fgCheckSsid        Need to check SSID or not. (for multiple SSID
 *                               with single BSSID cases)
 * @param[in] prSsid             Specified SSID
 *
 * @return   Pointer to BSS Descriptor, if found. NULL, if not found
 */
/*----------------------------------------------------------------------------*/
struct BSS_DESC *
scanSearchBssDescByTAAndSsid(IN struct ADAPTER *prAdapter,
			     IN uint8_t aucSrcAddr[],
			     IN u_int8_t fgCheckSsid,
			     IN struct PARAM_SSID *prSsid)
{
	struct SCAN_INFO *prScanInfo;
	struct LINK *prBSSDescList;
	struct BSS_DESC *prBssDesc;
	struct BSS_DESC *prDstBssDesc = (struct BSS_DESC *) NULL;

	ASSERT(prAdapter);
	ASSERT(aucSrcAddr);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);

	prBSSDescList = &prScanInfo->rBSSDescList;

	/* Search BSS Desc from current SCAN result list. */
	LINK_FOR_EACH_ENTRY(prBssDesc, prBSSDescList,
		rLinkEntry, struct BSS_DESC) {

		if (EQUAL_MAC_ADDR(prBssDesc->aucSrcAddr, aucSrcAddr)) {
			if (fgCheckSsid == FALSE || prSsid == NULL)
				return prBssDesc;
			if (EQUAL_SSID(prBssDesc->aucSSID, prBssDesc->ucSSIDLen,
					prSsid->aucSsid, prSsid->u4SsidLen)) {
				return prBssDesc;
			} else if (prDstBssDesc == NULL
				&& prBssDesc->fgIsHiddenSSID == TRUE) {
				prDstBssDesc = prBssDesc;
			}
		}
	}

	return prDstBssDesc;

}	/* end of scanSearchBssDescByTA() */

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
struct BSS_DESC *
scanSearchExistingBssDesc(IN struct ADAPTER *prAdapter,
			  IN enum ENUM_BSS_TYPE eBSSType,
			  IN uint8_t aucBSSID[],
			  IN uint8_t aucSrcAddr[])
{
	return scanSearchExistingBssDescWithSsid(prAdapter, eBSSType, aucBSSID,
		aucSrcAddr, FALSE, NULL);
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief Find the corresponding BSS Descriptor according to
 *        given eBSSType, BSSID and Transmitter Address
 *
 * @param[in] prAdapter   Pointer to the Adapter structure.
 * @param[in] eBSSType    BSS Type of incoming Beacon/ProbeResp frame.
 * @param[in] aucBSSID    Given BSSID of Beacon/ProbeResp frame.
 * @param[in] aucSrcAddr  Given source address (TA) of Beacon/ProbeResp frame.
 * @param[in] fgCheckSsid Need to check SSID or not. (for multiple SSID with
 *                        single BSSID cases)
 * @param[in] prSsid      Specified SSID
 *
 * @return   Pointer to BSS Descriptor, if found. NULL, if not found
 */
/*----------------------------------------------------------------------------*/
struct BSS_DESC *
scanSearchExistingBssDescWithSsid(IN struct ADAPTER *prAdapter,
				  IN enum ENUM_BSS_TYPE eBSSType,
				  IN uint8_t aucBSSID[],
				  IN uint8_t aucSrcAddr[],
				  IN u_int8_t fgCheckSsid,
				  IN struct PARAM_SSID *prSsid)
{
	struct SCAN_INFO *prScanInfo;
	struct BSS_DESC *prBssDesc, *prIBSSBssDesc;
	/* CASE III */
	struct LINK *prBSSDescList;
	struct LINK *prFreeBSSDescList;

	ASSERT(prAdapter);
	ASSERT(aucSrcAddr);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);

	switch (eBSSType) {
	case BSS_TYPE_P2P_DEVICE:
		fgCheckSsid = FALSE;
		/* fall through */
	case BSS_TYPE_INFRASTRUCTURE:
		/* fall through */
	case BSS_TYPE_BOW_DEVICE:
		prBssDesc = scanSearchBssDescByBssidAndSsid(prAdapter,
			aucBSSID, fgCheckSsid, prSsid);

		/* if (eBSSType == prBssDesc->eBSSType) */

		return prBssDesc;
	case BSS_TYPE_IBSS:
		prIBSSBssDesc = scanSearchBssDescByBssidAndSsid(prAdapter,
			aucBSSID, fgCheckSsid, prSsid);
		prBssDesc = scanSearchBssDescByTAAndSsid(prAdapter,
			aucSrcAddr, fgCheckSsid, prSsid);

		/* NOTE(Kevin):
		 * Rules to maintain the SCAN Result:
		 * For AdHoc -
		 *    CASE I    We have TA1(BSSID1), but it change its
		 *              BSSID to BSSID2
		 *              -> Update TA1 entry's BSSID.
		 *    CASE II   We have TA1(BSSID1), and get TA1(BSSID1) again
		 *              -> Update TA1 entry's contain.
		 *    CASE III  We have a SCAN result TA1(BSSID1), and
		 *              TA2(BSSID2). Sooner or later, TA2 merge into
		 *              TA1, we get TA2(BSSID1)
		 *              -> Remove TA2 first and then replace TA1 entry's
		 *                 TA with TA2, Still have only one entry
		 *                 of BSSID.
		 *    CASE IV   We have a SCAN result TA1(BSSID1), and another
		 *              TA2 also merge into BSSID1.
		 *              -> Replace TA1 entry's TA with TA2, Still have
		 *                 only one entry.
		 *    CASE V    New IBSS
		 *              -> Add this one to SCAN result.
		 */
		if (prBssDesc) {
			if ((!prIBSSBssDesc) ||	/* CASE I */
			    (prBssDesc == prIBSSBssDesc)) {	/* CASE II */

				return prBssDesc;
			}


			prBSSDescList = &prScanInfo->rBSSDescList;
			prFreeBSSDescList = &prScanInfo->rFreeBSSDescList;

			/* Remove this BSS Desc from the BSS Desc list */
			scanRemoveBssDescFromList(prBSSDescList,
				prBssDesc,
				prAdapter);

			/* Return this BSS Desc to the free BSS Desc list. */
			scanInsertBssDescToList(prFreeBSSDescList,
				prBssDesc,
				FALSE);

			return prIBSSBssDesc;
		}

		if (prIBSSBssDesc) {	/* CASE IV */

			return prIBSSBssDesc;
		}
		/* CASE V */
		break;	/* Return NULL; */
	default:
		break;
	}

	return (struct BSS_DESC *) NULL;

}	/* end of scanSearchExistingBssDesc() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief Delete BSS Descriptors from current list according
 * to given Remove Policy.
 *
 * @param[in] u4RemovePolicy     Remove Policy.
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void scanRemoveBssDescsByPolicy(IN struct ADAPTER *prAdapter,
				IN uint32_t u4RemovePolicy)
{
	struct SCAN_INFO *prScanInfo;
	struct LINK *prBSSDescList;
	struct LINK *prFreeBSSDescList;
	struct BSS_DESC *prBssDesc;

	ASSERT(prAdapter);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prBSSDescList = &prScanInfo->rBSSDescList;
	prFreeBSSDescList = &prScanInfo->rFreeBSSDescList;

#if 0 /* TODO: Remove this */
	log_dbg(SCN, TRACE, ("Before Remove - Number Of SCAN Result = %ld\n",
		prBSSDescList->u4NumElem));
#endif

	if (u4RemovePolicy & SCN_RM_POLICY_TIMEOUT) {
		struct BSS_DESC *prBSSDescNext;
		OS_SYSTIME rCurrentTime;

		GET_CURRENT_SYSTIME(&rCurrentTime);

		/* Search BSS Desc from current SCAN result list. */
		LINK_FOR_EACH_ENTRY_SAFE(prBssDesc, prBSSDescNext,
			prBSSDescList, rLinkEntry, struct BSS_DESC) {

			if ((u4RemovePolicy & SCN_RM_POLICY_EXCLUDE_CONNECTED)
				&& (prBssDesc->fgIsConnected
				|| prBssDesc->fgIsConnecting)) {
				/* Don't remove the one currently we
				 * are connected.
				 */
				continue;
			}

			if (CHECK_FOR_TIMEOUT(rCurrentTime,
				prBssDesc->rUpdateTime,
				SEC_TO_SYSTIME(wlanWfdEnabled(prAdapter) ?
					SCN_BSS_DESC_STALE_SEC_WFD :
					SCN_BSS_DESC_STALE_SEC))) {

#if 0 /* TODO: Remove this */
				log_dbg(SCN, TRACE, "Remove TIMEOUT BSS DESC(%#x):MAC: "
				MACSTR
				", Current Time = %08lx, Update Time = %08lx\n",
					prBssDesc,
					MAC2STR(prBssDesc->aucBSSID),
					rCurrentTime, prBssDesc->rUpdateTime));
#endif
				scanRemoveBssDescFromList(prBSSDescList,
					prBssDesc,
					prAdapter);

				/* Return this BSS Desc to the
				 * free BSS Desc list.
				 */
				scanInsertBssDescToList(prFreeBSSDescList,
					prBssDesc,
					FALSE);
			}
		}
	}
	if (u4RemovePolicy & SCN_RM_POLICY_OLDEST_HIDDEN) {
		struct BSS_DESC *prBssDescOldest = (struct BSS_DESC *) NULL;

		/* Search BSS Desc from current SCAN result list. */
		LINK_FOR_EACH_ENTRY(prBssDesc, prBSSDescList,
			rLinkEntry, struct BSS_DESC) {

			if ((u4RemovePolicy & SCN_RM_POLICY_EXCLUDE_CONNECTED)
				&& (prBssDesc->fgIsConnected
				|| prBssDesc->fgIsConnecting)) {
				/* Don't remove the one currently
				 * we are connected.
				 */
				continue;
			}

			if (!prBssDesc->fgIsHiddenSSID)
				continue;

			if (!prBssDescOldest) {	/* 1st element */
				prBssDescOldest = prBssDesc;
				continue;
			}

			if (TIME_BEFORE(prBssDesc->rUpdateTime,
				prBssDescOldest->rUpdateTime))
				prBssDescOldest = prBssDesc;
		}

		if (prBssDescOldest) {
#if 0 /* TODO: Remove this */
			log_dbg(SCN, TRACE, "Remove OLDEST HIDDEN BSS DESC(%#x): MAC: "
			MACSTR
			", Update Time = %08lx\n",
				prBssDescOldest,
				MAC2STR(prBssDescOldest->aucBSSID),
				prBssDescOldest->rUpdateTime);
#endif
			scanRemoveBssDescFromList(prBSSDescList,
				prBssDescOldest,
				prAdapter);

			/* Return this BSS Desc to the free BSS Desc list. */
			scanInsertBssDescToList(prFreeBSSDescList,
				prBssDescOldest,
				FALSE);
		}
	}
	if (u4RemovePolicy & SCN_RM_POLICY_SMART_WEAKEST) {
		struct BSS_DESC *prBssDescWeakest = (struct BSS_DESC *) NULL;
		struct BSS_DESC *prBssDescWeakestSameSSID
			= (struct BSS_DESC *) NULL;
		uint32_t u4SameSSIDCount = 0;
		uint8_t j;
		uint8_t fgIsSameSSID;

		/* Search BSS Desc from current SCAN result list. */
		LINK_FOR_EACH_ENTRY(prBssDesc, prBSSDescList,
			rLinkEntry, struct BSS_DESC) {

			if ((u4RemovePolicy & SCN_RM_POLICY_EXCLUDE_CONNECTED)
				&& (prBssDesc->fgIsConnected
				|| prBssDesc->fgIsConnecting)) {
				/* Don't remove the one currently
				 * we are connected.
				 */
				continue;
			}

			fgIsSameSSID = FALSE;
			for (j = 0; j < KAL_AIS_NUM; j++) {

				struct CONNECTION_SETTINGS *prConnSettings =
					aisGetConnSettings(prAdapter, j);

				if (!prConnSettings)
					continue;

				if ((!prBssDesc->fgIsHiddenSSID) &&
					(EQUAL_SSID(prBssDesc->aucSSID,
					prBssDesc->ucSSIDLen,
					prConnSettings->aucSSID,
					prConnSettings->ucSSIDLen))) {

					u4SameSSIDCount++;

					if (!prBssDescWeakestSameSSID)
						prBssDescWeakestSameSSID =
							prBssDesc;
					else if (prBssDesc->ucRCPI
					< prBssDescWeakestSameSSID->ucRCPI)
						prBssDescWeakestSameSSID =
							prBssDesc;

					fgIsSameSSID = TRUE;
				}
			}

			if (fgIsSameSSID &&
				u4SameSSIDCount
				< SCN_BSS_DESC_SAME_SSID_THRESHOLD)
				continue;

			if (!prBssDescWeakest) {	/* 1st element */
				prBssDescWeakest = prBssDesc;
				continue;
			}

			if (prBssDesc->ucRCPI < prBssDescWeakest->ucRCPI)
				prBssDescWeakest = prBssDesc;

		}

		if ((u4SameSSIDCount >= SCN_BSS_DESC_SAME_SSID_THRESHOLD)
			&& (prBssDescWeakestSameSSID))
			prBssDescWeakest = prBssDescWeakestSameSSID;

		if (prBssDescWeakest) {
#if 0 /* TODO: Remove this */
			log_dbg(SCN, TRACE, "Remove WEAKEST BSS DESC(%#x): MAC: "
			MACSTR
			", Update Time = %08lx\n",
				prBssDescOldest,
				MAC2STR(prBssDescOldest->aucBSSID),
				prBssDescOldest->rUpdateTime);
#endif

			scanRemoveBssDescFromList(prBSSDescList,
				prBssDescWeakest,
				prAdapter);

			/* Return this BSS Desc to the free BSS Desc list. */
			scanInsertBssDescToList(prFreeBSSDescList,
				prBssDescWeakest,
				FALSE);
		}
	}
	if (u4RemovePolicy & SCN_RM_POLICY_ENTIRE) {
		struct BSS_DESC *prBSSDescNext;

		LINK_FOR_EACH_ENTRY_SAFE(prBssDesc, prBSSDescNext,
			prBSSDescList, rLinkEntry, struct BSS_DESC) {

			if ((u4RemovePolicy & SCN_RM_POLICY_EXCLUDE_CONNECTED)
				&& (prBssDesc->fgIsConnected
				|| prBssDesc->fgIsConnecting)) {
				/* Don't remove the one currently
				 * we are connected.
				 */
				continue;
			}

			scanRemoveBssDescFromList(prBSSDescList,
				prBssDesc,
				prAdapter);

			/* Return this BSS Desc to the free BSS Desc list. */
			scanInsertBssDescToList(prFreeBSSDescList,
				prBssDesc,
				FALSE);
		}

	}
}	/* end of scanRemoveBssDescsByPolicy() */

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
void scanRemoveBssDescByBssid(IN struct ADAPTER *prAdapter,
			      IN uint8_t aucBSSID[])
{
	struct SCAN_INFO *prScanInfo;
	struct LINK *prBSSDescList;
	struct LINK *prFreeBSSDescList;
	struct BSS_DESC *prBssDesc = (struct BSS_DESC *) NULL;
	struct BSS_DESC *prBSSDescNext;
	uint8_t ucTargetChNum = 0;
	enum ENUM_BAND eTargetBand = BAND_NULL;

	ASSERT(prAdapter);
	ASSERT(aucBSSID);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prBSSDescList = &prScanInfo->rBSSDescList;
	prFreeBSSDescList = &prScanInfo->rFreeBSSDescList;

	/* Check if such BSS Descriptor exists in a valid list */
	LINK_FOR_EACH_ENTRY_SAFE(prBssDesc, prBSSDescNext, prBSSDescList,
		rLinkEntry, struct BSS_DESC) {

		if (EQUAL_MAC_ADDR(prBssDesc->aucBSSID, aucBSSID)) {
			/* Because BSS descriptor will be cleared in next step,
			 * so we need to keep them temporarily.
			 */
			ucTargetChNum = prBssDesc->ucChannelNum;
			eTargetBand = prBssDesc->eBand;

			/* Clear BSS descriptor */
			scanRemoveBssDescFromList(prBSSDescList,
				prBssDesc,
				prAdapter);

			/* Return this BSS Desc to the free BSS Desc list. */
			scanInsertBssDescToList(prFreeBSSDescList,
				prBssDesc,
				FALSE);

			/* We should notify kernel to unlink BSS */
			kalRemoveBss(
				prAdapter->prGlueInfo,
				aucBSSID,
				ucTargetChNum,
				eTargetBand);

			/* BSSID is not unique, so need to traverse
			 * whols link-list
			 */
		}
	}
}	/* end of scanRemoveBssDescByBssid() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief Delete BSS Descriptors from current list according to given
 * band configuration
 *
 * @param[in] prAdapter  Pointer to the Adapter structure.
 * @param[in] eBand      Given band
 * @param[in] ucBssIndex     AIS - Remove IBSS/Infrastructure BSS
 *                           BOW - Remove BOW BSS
 *                           P2P - Remove P2P BSS
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void scanRemoveBssDescByBandAndNetwork(IN struct ADAPTER *prAdapter,
				       IN enum ENUM_BAND eBand,
				       IN uint8_t ucBssIndex)
{
	struct SCAN_INFO *prScanInfo;
	struct LINK *prBSSDescList;
	struct LINK *prFreeBSSDescList;
	struct BSS_DESC *prBssDesc = (struct BSS_DESC *) NULL;
	struct BSS_DESC *prBSSDescNext;
	u_int8_t fgToRemove;

	ASSERT(prAdapter);
	ASSERT(eBand <= BAND_NUM);
	ASSERT(ucBssIndex <= prAdapter->ucHwBssIdNum);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prBSSDescList = &prScanInfo->rBSSDescList;
	prFreeBSSDescList = &prScanInfo->rFreeBSSDescList;

	if (eBand == BAND_NULL) {
		/* no need to do anything, keep all scan result */
		return;
	}

	/* Check if such BSS Descriptor exists in a valid list */
	LINK_FOR_EACH_ENTRY_SAFE(prBssDesc, prBSSDescNext, prBSSDescList,
		rLinkEntry, struct BSS_DESC) {
		fgToRemove = FALSE;

		if (prBssDesc->eBand == eBand) {
			switch (GET_BSS_INFO_BY_INDEX(
				prAdapter, ucBssIndex)->eNetworkType) {
			case NETWORK_TYPE_AIS:
				if ((prBssDesc->eBSSType
				    == BSS_TYPE_INFRASTRUCTURE)
				    || (prBssDesc->eBSSType == BSS_TYPE_IBSS)) {
					fgToRemove = TRUE;
				}
				break;

			case NETWORK_TYPE_P2P:
				if (prBssDesc->eBSSType == BSS_TYPE_P2P_DEVICE)
					fgToRemove = TRUE;
				break;

			case NETWORK_TYPE_BOW:
				if (prBssDesc->eBSSType == BSS_TYPE_BOW_DEVICE)
					fgToRemove = TRUE;
				break;

			default:
				ASSERT(0);
				break;
			}
		}

		if (fgToRemove == TRUE) {
			scanRemoveBssDescFromList(prBSSDescList,
				prBssDesc,
				prAdapter);

			/* Return this BSS Desc to the free BSS Desc list. */
			scanInsertBssDescToList(prFreeBSSDescList,
				prBssDesc,
				FALSE);
		}
	}
}	/* end of scanRemoveBssDescByBand() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief Clear the CONNECTION FLAG of a specified BSS Descriptor.
 *
 * @param[in] aucBSSID   Given BSSID.
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void scanRemoveConnFlagOfBssDescByBssid(IN struct ADAPTER *prAdapter,
					IN uint8_t aucBSSID[])
{
	struct SCAN_INFO *prScanInfo;
	struct LINK *prBSSDescList;
	struct BSS_DESC *prBssDesc = (struct BSS_DESC *) NULL;

	ASSERT(prAdapter);
	ASSERT(aucBSSID);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prBSSDescList = &prScanInfo->rBSSDescList;

	/* Search BSS Desc from current SCAN result list. */
	LINK_FOR_EACH_ENTRY(prBssDesc, prBSSDescList,
		rLinkEntry, struct BSS_DESC) {

		if (EQUAL_MAC_ADDR(prBssDesc->aucBSSID, aucBSSID)) {
			prBssDesc->fgIsConnected = FALSE;
			prBssDesc->fgIsConnecting = FALSE;

			/* BSSID is not unique, so need to
			 * traverse whols link-list
			 */
		}
	}
}	/* end of scanRemoveConnectionFlagOfBssDescByBssid() */


#if (CFG_SUPPORT_802_11V_MBSSID == 1)
/*----------------------------------------------------------------------------*/
/*!
 * @brief Allocate new BSS_DESC structure
 *
 * @param[in] prAdapter          Pointer to the Adapter structure.
 *
 * @return   Pointer to BSS Descriptor, if has
 *           free space. NULL, if has no space.
 */
/*----------------------------------------------------------------------------*/
void scanParsingMBSSIDSubelement(IN struct ADAPTER *prAdapter,
	IN struct BSS_DESC *prTransBSS, IN struct IE_MBSSID *prMbssidIe)
{
	uint8_t *pucIE;
	uint16_t u2IELength, u2Offset;
	uint8_t *pucProfileIE;
	uint16_t u2ProfileLen, u2ProfileOffset;
	struct BSS_DESC *prBssDesc;
	struct IE_MBSSID_INDEX *prMbssidIdxIe;
	uint8_t aucBSSID[MAC_ADDR_LEN];
	int8_t ucBssidLsb;

	if (prMbssidIe->ucMaxBSSIDIndicator == 0) /* invalid mbssid ie*/
		return;

	prTransBSS->ucMaxBSSIDIndicator = prMbssidIe->ucMaxBSSIDIndicator;
	u2Offset = 0;
	pucIE = &prMbssidIe->ucSubelements[0];
	u2IELength = IE_SIZE(prMbssidIe) -
		OFFSET_OF(struct IE_MBSSID, ucSubelements);
	IE_FOR_EACH(pucIE, u2IELength, u2Offset)
	{
		pucProfileIE = NULL;
		/*search for each nontransmitted bssid profile*/
		if (IE_ID(pucIE) == NON_TX_BSSID_PROFILE) {
			pucProfileIE = &((struct IE_HDR *)pucIE)->aucInfo[0];
			u2ProfileLen = IE_LEN(pucIE);
			u2ProfileOffset = 0;
		}
		if (pucProfileIE == NULL)
			continue;

		/*search for mbssid index ie in each profile*/
		prMbssidIdxIe = NULL;
		IE_FOR_EACH(pucProfileIE, u2ProfileLen, u2ProfileOffset) {
			if (IE_ID(pucProfileIE) == ELEM_ID_MBSSID_INDEX) {
				prMbssidIdxIe =	MBSSID_INDEX_IE(pucProfileIE);
				break;
			}
		}
		if (prMbssidIdxIe == NULL)
			continue;

		/*calculate BSSID of this profile*/
		kalMemCopy(aucBSSID, &prTransBSS->aucBSSID[0], MAC_ADDR_LEN);
		ucBssidLsb = aucBSSID[5] &
				((1 << prMbssidIe->ucMaxBSSIDIndicator) - 1);
		aucBSSID[5] &= ~((1 << prMbssidIe->ucMaxBSSIDIndicator) - 1);
		aucBSSID[5] |= (ucBssidLsb + prMbssidIdxIe->ucBSSIDIndex) %
				  (1 << prMbssidIe->ucMaxBSSIDIndicator);

		prBssDesc = scanSearchBssDescByBssidAndSsid(prAdapter,
					aucBSSID, FALSE, NULL);
		if (prBssDesc) {
			prBssDesc->ucMaxBSSIDIndicator =
				prMbssidIe->ucMaxBSSIDIndicator;
			prBssDesc->ucMBSSIDIndex =
				prMbssidIdxIe->ucBSSIDIndex;
		}
	}
}
#endif

/*----------------------------------------------------------------------------*/
/*!
 * @brief Allocate new BSS_DESC structure
 *
 * @param[in] prAdapter          Pointer to the Adapter structure.
 *
 * @return   Pointer to BSS Descriptor, if has
 *           free space. NULL, if has no space.
 */
/*----------------------------------------------------------------------------*/
struct BSS_DESC *scanAllocateBssDesc(IN struct ADAPTER *prAdapter)
{
	struct SCAN_INFO *prScanInfo;
	struct LINK *prFreeBSSDescList;
	struct BSS_DESC *prBssDesc;

	ASSERT(prAdapter);
	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);

	prFreeBSSDescList = &prScanInfo->rFreeBSSDescList;

	LINK_REMOVE_HEAD(prFreeBSSDescList, prBssDesc, struct BSS_DESC *);

	if (prBssDesc) {
		struct LINK *prBSSDescList;

		prBSSDescList = &prScanInfo->rBSSDescList;

		/* NOTE(Kevin): In current design, this new empty
		 * struct BSS_DESC will be inserted to BSSDescList immediately.
		 */
		scanInsertBssDescToList(prBSSDescList,
			prBssDesc,
			TRUE);
	}

	return prBssDesc;

}	/* end of scanAllocateBssDesc() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This API parses Beacon/ProbeResp frame and insert extracted
 *        BSS_DESC structure with IEs into
 *        prAdapter->rWifiVar.rScanInfo.aucScanBuffer
 *
 * @param[in] prAdapter      Pointer to the Adapter structure.
 * @param[in] prSwRfb        Pointer to the receiving frame buffer.
 *
 * @return   Pointer to BSS Descriptor
 *           NULL if the Beacon/ProbeResp frame is invalid
 */
/*----------------------------------------------------------------------------*/
struct BSS_DESC *scanAddToBssDesc(IN struct ADAPTER *prAdapter,
				  IN struct SW_RFB *prSwRfb)
{
	struct BSS_DESC *prBssDesc = NULL;
	uint16_t u2CapInfo;
	enum ENUM_BSS_TYPE eBSSType = BSS_TYPE_INFRASTRUCTURE;

	uint8_t *pucIE;
	uint16_t u2IELength;
	uint16_t u2Offset = 0;

	struct WLAN_BEACON_FRAME *prWlanBeaconFrame
		= (struct WLAN_BEACON_FRAME *) NULL;
	struct IE_SSID *prIeSsid = (struct IE_SSID *) NULL;
	struct IE_SUPPORTED_RATE_IOT *prIeSupportedRate
		= (struct IE_SUPPORTED_RATE_IOT *) NULL;
	struct IE_EXT_SUPPORTED_RATE *prIeExtSupportedRate
		= (struct IE_EXT_SUPPORTED_RATE *) NULL;
	uint8_t ucHwChannelNum = 0;
	uint8_t ucIeDsChannelNum = 0;
	uint8_t ucIeHtChannelNum = 0;
	u_int8_t fgIsValidSsid = FALSE;
	struct PARAM_SSID rSsid;
	uint64_t u8Timestamp;
	u_int8_t fgIsNewBssDesc = FALSE;

	uint32_t i;
	uint8_t ucSSIDChar;
	/* PUINT_8 pucDumpIE; */
	enum ENUM_BAND eHwBand = BAND_NULL;
	u_int8_t fgBandMismatch = FALSE;
	uint8_t ucSubtype;
	u_int8_t fgIsProbeResp = FALSE;
	u_int8_t ucPowerConstraint = 0;
	struct IE_COUNTRY *prCountryIE = NULL;
	struct RX_DESC_OPS_T *prRxDescOps;
#if ((CFG_SUPPORT_802_11AX == 1) && (CFG_SUPPORT_HE_ER == 1))
	struct _IE_HE_OP_T *prHeOp;
	struct _IE_HE_CAP_T *prHeCap;
#endif

	ASSERT(prAdapter);
	ASSERT(prSwRfb);
	prRxDescOps = prAdapter->chip_info->prRxDescOps;

	RX_STATUS_GET(prRxDescOps, eHwBand, get_rf_band, prSwRfb->prRxStatus);
	prWlanBeaconFrame = (struct WLAN_BEACON_FRAME *) prSwRfb->pvHeader;
	ucSubtype = (*(uint8_t *) (prSwRfb->pvHeader) &
			MASK_FC_SUBTYPE) >> OFFSET_OF_FC_SUBTYPE;

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
		/* The P2P Device shall set the ESS bit of
		 * the Capabilities field in the Probe Response fame to 0
		 * and IBSS bit to 0. (3.1.2.1.1)
		 */
		eBSSType = BSS_TYPE_P2P_DEVICE;
		break;

#if CFG_ENABLE_BT_OVER_WIFI
		/* @TODO: add rule to identify BOW beacons */
#endif

	default:
		log_dbg(SCN, WARN, "Skip unknown bss type(%u)\n", u2CapInfo);
		return NULL;
	}

	/* 4 <1.1> Pre-parse SSID IE and channel info */
	pucIE = prWlanBeaconFrame->aucInfoElem;
	u2IELength = (prSwRfb->u2PacketLen - prSwRfb->u2HeaderLen) -
	    (uint16_t) OFFSET_OF(struct WLAN_BEACON_FRAME_BODY, aucInfoElem[0]);

	if (u2IELength > CFG_IE_BUFFER_SIZE) {
		/* Give an warning msg when IE is going to be
		 * truncated.
		 */
		DBGLOG(SCN, ERROR,
			"IE len(%u) > Max IE buffer size(%u), truncate IE!\n",
			u2IELength, CFG_IE_BUFFER_SIZE);
		u2IELength = CFG_IE_BUFFER_SIZE;
	}
	kalMemZero(&rSsid, sizeof(rSsid));
	IE_FOR_EACH(pucIE, u2IELength, u2Offset) {
		/* Error handling for disorder IE that IE length is 0 */
		if (IE_ID(pucIE) != ELEM_ID_SSID && IE_LEN(pucIE) == 0)
			continue;
		switch (IE_ID(pucIE)) {
		case ELEM_ID_SSID:
			if (IE_LEN(pucIE) <= ELEM_MAX_LEN_SSID) {
				ucSSIDChar = '\0';

				/* D-Link DWL-900AP+ */
				if (IE_LEN(pucIE) == 0)
					fgIsValidSsid = FALSE;
				/* Cisco AP1230A -
				 * (IE_LEN(pucIE) == 1)
				 * && (SSID_IE(pucIE)->aucSSID[0] == '\0')
				 */
				/* Linksys WRK54G/WL520g -
				 * (IE_LEN(pucIE) == n)
				 * && (SSID_IE(pucIE)->aucSSID[0~(n-1)] == '\0')
				 */
				else {
					for (i = 0; i < IE_LEN(pucIE); i++) {
						ucSSIDChar
							|= SSID_IE(pucIE)
								->aucSSID[i];
					}

					if (ucSSIDChar)
						fgIsValidSsid = TRUE;
				}

				/* Update SSID to BSS Descriptor only if
				 * SSID is not hidden.
				 */
				if (fgIsValidSsid == TRUE) {
					COPY_SSID(rSsid.aucSsid,
						  rSsid.u4SsidLen,
						  SSID_IE(pucIE)->aucSSID,
						  SSID_IE(pucIE)->ucLength);
				}
			}
			break;
		case ELEM_ID_DS_PARAM_SET:
			if (IE_LEN(pucIE)
				== ELEM_MAX_LEN_DS_PARAMETER_SET) {
				ucIeDsChannelNum
					= DS_PARAM_IE(pucIE)->ucCurrChnl;
			}
			break;

		case ELEM_ID_HT_OP:
			if (IE_LEN(pucIE) == (sizeof(struct IE_HT_OP) - 2))
				ucIeHtChannelNum = ((struct IE_HT_OP *) pucIE)
					->ucPrimaryChannel;
			break;
		default:
			break;
		}
	}

	/**
	 * Set band mismatch flag if we receive Beacon/ProbeResp in 2.4G band,
	 * but the channel num in IE info is 5G, and vice versa
	 * We can get channel num from different IE info, we select
	 * ELEM_ID_DS_PARAM_SET first, and then ELEM_ID_HT_OP
	 * If we don't have any channel info, we set it as HW channel, which is
	 * the channel we get this Beacon/ProbeResp from.
	 */
	if (ucIeDsChannelNum > 0) {
		if (ucIeDsChannelNum <= HW_CHNL_NUM_MAX_2G4)
			fgBandMismatch = (eHwBand != BAND_2G4);
		else if (ucIeDsChannelNum < HW_CHNL_NUM_MAX_4G_5G)
			fgBandMismatch = (eHwBand != BAND_5G);
	} else if (ucIeHtChannelNum > 0) {
		if (ucIeHtChannelNum <= HW_CHNL_NUM_MAX_2G4)
			fgBandMismatch = (eHwBand != BAND_2G4);
		else if (ucIeHtChannelNum < HW_CHNL_NUM_MAX_4G_5G)
			fgBandMismatch = (eHwBand != BAND_5G);
	}

	if (fgBandMismatch) {
		log_dbg(SCN, INFO, MACSTR "Band mismatch, HW band %d, DS chnl %d, HT chnl %d\n",
		       MAC2STR(prWlanBeaconFrame->aucBSSID), eHwBand,
		       ucIeDsChannelNum, ucIeHtChannelNum);
		return NULL;
	}

	/* 4 <1.2> Replace existing BSS_DESC structure or allocate a new one */
	prBssDesc = scanSearchExistingBssDescWithSsid(
		prAdapter,
		eBSSType,
		(uint8_t *) prWlanBeaconFrame->aucBSSID,
		(uint8_t *) prWlanBeaconFrame->aucSrcAddr,
		fgIsValidSsid, fgIsValidSsid == TRUE ? &rSsid : NULL);

	log_dbg(SCN, TRACE, "Receive type %u in chnl %u %u %u (" MACSTR
		") valid(%u) found(%u)\n",
		ucSubtype, ucIeDsChannelNum, ucIeHtChannelNum,
		prSwRfb->ucChnlNum,
		MAC2STR((uint8_t *)prWlanBeaconFrame->aucBSSID), fgIsValidSsid,
		(prBssDesc != NULL) ? 1 : 0);

	if ((prWlanBeaconFrame->u2FrameCtrl & MASK_FRAME_TYPE)
			== MAC_FRAME_PROBE_RSP)
		fgIsProbeResp = TRUE;

	if (prBssDesc == (struct BSS_DESC *) NULL) {
		fgIsNewBssDesc = TRUE;

		do {
			/* 4 <1.2.1> First trial of allocation */
			prBssDesc = scanAllocateBssDesc(prAdapter);
			if (prBssDesc)
				break;
			/* 4 <1.2.2> Hidden is useless, remove the oldest
			 * hidden ssid. (for passive scan)
			 */
			scanRemoveBssDescsByPolicy(prAdapter,
				(SCN_RM_POLICY_EXCLUDE_CONNECTED
				| SCN_RM_POLICY_OLDEST_HIDDEN
				| SCN_RM_POLICY_TIMEOUT));

			/* 4 <1.2.3> Second tail of allocation */
			prBssDesc = scanAllocateBssDesc(prAdapter);
			if (prBssDesc)
				break;
			/* 4 <1.2.4> Remove the weakest one */
			/* If there are more than half of BSS which has the
			 * same ssid as connection setting, remove the weakest
			 * one from them. Else remove the weakest one.
			 */
			scanRemoveBssDescsByPolicy(prAdapter,
				(SCN_RM_POLICY_EXCLUDE_CONNECTED
				 | SCN_RM_POLICY_SMART_WEAKEST));

			/* 4 <1.2.5> reallocation */
			prBssDesc = scanAllocateBssDesc(prAdapter);
			if (prBssDesc)
				break;
			/* 4 <1.2.6> no space, should not happen */
			log_dbg(SCN, WARN, "alloc new BssDesc for "
				MACSTR " failed\n",
				MAC2STR((uint8_t *)
				prWlanBeaconFrame->aucBSSID));
			return NULL;

		} while (FALSE);

	} else {
		OS_SYSTIME rCurrentTime;

		/* WCXRP00000091 */
		/* if the received strength is much weaker than
		 * the original one, ignore it due to it might be received
		 * on the folding frequency
		 */

		GET_CURRENT_SYSTIME(&rCurrentTime);

		ASSERT(prSwRfb->prRxStatusGroup3);

		if (prBssDesc->eBSSType != eBSSType) {
			prBssDesc->eBSSType = eBSSType;
		} else if (prSwRfb->ucChnlNum !=
			prBssDesc->ucChannelNum
			&& prBssDesc->ucRCPI
			> nicRxGetRcpiValueFromRxv(
				prAdapter, RCPI_MODE_MAX, prSwRfb)) {
			uint8_t ucRcpi = 0;

			/* for signal strength is too much weaker and
			 * previous beacon is not stale
			 */
			ASSERT(prSwRfb->prRxStatusGroup3);
			ucRcpi = nicRxGetRcpiValueFromRxv(prAdapter,
				RCPI_MODE_MAX,
				prSwRfb);
			if ((prBssDesc->ucRCPI - ucRcpi)
			    >= REPLICATED_BEACON_STRENGTH_THRESHOLD
			    && rCurrentTime - prBssDesc->rUpdateTime
			    <= REPLICATED_BEACON_FRESH_PERIOD) {
				log_dbg(SCN, TRACE, "rssi(%u) is too much weaker and previous one(%u) is fresh\n",
					ucRcpi, prBssDesc->ucRCPI);
				return prBssDesc;
			}
			/* for received beacons too close in time domain */
			else if (rCurrentTime - prBssDesc->rUpdateTime
				<= REPLICATED_BEACON_TIME_THRESHOLD) {
				log_dbg(SCN, TRACE, "receive beacon/probe responses too soon(%u:%u)\n",
					prBssDesc->rUpdateTime, rCurrentTime);
				return prBssDesc;
			}
		}

		/* if Timestamp has been reset, re-generate BSS
		 * DESC 'cause AP should have reset itself
		 */
		if (prBssDesc->eBSSType == BSS_TYPE_INFRASTRUCTURE
			&& u8Timestamp < prBssDesc->u8TimeStamp.QuadPart
			&& prBssDesc->fgIsConnecting == FALSE) {
			u_int8_t fgIsConnected, fgIsConnecting;

			if (aisGetTargetBssDesc(prAdapter, AIS_DEFAULT_INDEX)
				== prBssDesc) {
				log_dbg(SCN, TRACE, "Timestamap reset. Reset prTargetBssDesc BSS:"
					MACSTR " connected:%x connecting:%x",
					MAC2STR(prBssDesc->aucBSSID),
					prBssDesc->fgIsConnected,
					prBssDesc->fgIsConnecting);
			}

			/* set flag for indicating this is a new BSS-DESC */
			fgIsNewBssDesc = TRUE;

			/* backup 2 flags for APs which reset
			 * timestamp unexpectedly
			 */
			fgIsConnected = prBssDesc->fgIsConnected;
			fgIsConnecting = prBssDesc->fgIsConnecting;

			/* Connected BSS descriptor still be used by other
			 * functions. Thus, we should re-initialize the BSS_DESC
			 * structure instead of re-allocating the BSS_DESC
			 * structure.
			 */
			scanResetBssDesc(prAdapter, prBssDesc);

			/* restore */
			prBssDesc->fgIsConnected = fgIsConnected;
			prBssDesc->fgIsConnecting = fgIsConnecting;
		}
	}

	prBssDesc->u2RawLength = prSwRfb->u2PacketLen;
	if (prBssDesc->u2RawLength > CFG_RAW_BUFFER_SIZE) {
		prBssDesc->u2RawLength = CFG_RAW_BUFFER_SIZE;
		/* Give an warning msg when content is going to be
		 * truncated.
		 */
		DBGLOG(SCN, WARN,
			"Pkt len(%u) > Max RAW buffer size(%u), truncate it!\n",
			prSwRfb->u2PacketLen, CFG_RAW_BUFFER_SIZE);
	}
	kalMemCopy(prBssDesc->aucRawBuf,
		prWlanBeaconFrame, prBssDesc->u2RawLength);

	/* NOTE: Keep consistency of Scan Record during JOIN process */
	if (fgIsNewBssDesc == FALSE && prBssDesc->fgIsConnecting) {
		log_dbg(SCN, TRACE, "we're connecting this BSS("
			MACSTR ") now, don't update it\n",
			MAC2STR(prBssDesc->aucBSSID));
		return prBssDesc;
	}
	/* 4 <2> Get information from Fixed Fields */
	/* Update the latest BSS type information. */
	prBssDesc->eBSSType = eBSSType;

	COPY_MAC_ADDR(prBssDesc->aucSrcAddr, prWlanBeaconFrame->aucSrcAddr);

	COPY_MAC_ADDR(prBssDesc->aucBSSID, prWlanBeaconFrame->aucBSSID);

	prBssDesc->u8TimeStamp.QuadPart = u8Timestamp;

	WLAN_GET_FIELD_16(&prWlanBeaconFrame->u2BeaconInterval,
		&prBssDesc->u2BeaconInterval);

	prBssDesc->u2CapInfo = u2CapInfo;

	/* 4 <2.1> Retrieve IEs for later parsing */
	u2IELength = (prSwRfb->u2PacketLen - prSwRfb->u2HeaderLen) -
	    (uint16_t) OFFSET_OF(struct WLAN_BEACON_FRAME_BODY, aucInfoElem[0]);

	if (u2IELength > CFG_IE_BUFFER_SIZE) {
		u2IELength = CFG_IE_BUFFER_SIZE;
		prBssDesc->fgIsIEOverflow = TRUE;
		DBGLOG(SCN, WARN, "IE is truncated!\n");
	} else {
		prBssDesc->fgIsIEOverflow = FALSE;
	}
	prBssDesc->u2IELength = u2IELength;

	if (fgIsProbeResp || fgIsValidSsid) {
		kalMemCopy(prBssDesc->aucIEBuf, prWlanBeaconFrame->aucInfoElem,
		u2IELength);
	}
	/* 4 <2.2> reset prBssDesc variables in case that AP
	 * has been reconfigured
	 */
#if (CFG_SUPPORT_HE_ER == 1)
	prBssDesc->fgIsERSUDisable = TRUE;
	prBssDesc->ucDCMMaxConRx = 0;
#endif
	prBssDesc->fgIsERPPresent = FALSE;
	prBssDesc->fgIsHTPresent = FALSE;
	prBssDesc->fgIsVHTPresent = FALSE;
#if (CFG_SUPPORT_802_11AX == 1)
	if (fgEfuseCtrlAxOn == 1)
		prBssDesc->fgIsHEPresent = FALSE;
#endif
	prBssDesc->eSco = CHNL_EXT_SCN;
	prBssDesc->fgIEWAPI = FALSE;
	prBssDesc->fgIERSN = FALSE;
	prBssDesc->fgIEWPA = FALSE;

	/*Reset VHT OP IE relative settings */
	prBssDesc->eChannelWidth = CW_20_40MHZ;

	prBssDesc->ucCenterFreqS1 = 0;
	prBssDesc->ucCenterFreqS2 = 0;

	/* Support AP Selection */
	prBssDesc->fgExsitBssLoadIE = FALSE;
	prBssDesc->fgMultiAnttenaAndSTBC = FALSE;
#if CFG_SUPPORT_MBO
	prBssDesc->fgIsDisallowed = FALSE;
#endif

	if (fgIsProbeResp == FALSE) {
		/* Probe response doesn't have TIM IE. Thus, we should
		 * reset TIM when handling beacon frame only.
		 */
		prBssDesc->fgTIMPresent = FALSE;
		prBssDesc->ucDTIMPeriod = 0;
	}

	/* The cPowerLimit should be set as invalid value while BSS description
	 * is reallocated
	 */
	if (fgIsNewBssDesc) {
		prBssDesc->cPowerLimit = RLM_INVALID_POWER_LIMIT;
		DBGLOG(SCN, LOUD,
			"LM: New reallocated BSSDesc [" MACSTR "]\n",
			MAC2STR(prBssDesc->aucBSSID));
	}

	/* 4 <3.1> Full IE parsing on SW_RFB_T */
	pucIE = prWlanBeaconFrame->aucInfoElem;
	/* pucDumpIE = pucIE; */

	IE_FOR_EACH(pucIE, u2IELength, u2Offset) {
		/* Error handling for disorder IE that IE length is 0*/
		if (IE_ID(pucIE) != ELEM_ID_SSID && IE_LEN(pucIE) == 0)
			continue;
		switch (IE_ID(pucIE)) {
		case ELEM_ID_SSID:
			if ((!prIeSsid) && /* NOTE(Kevin): for Atheros IOT #1 */
			    (IE_LEN(pucIE) <= ELEM_MAX_LEN_SSID)) {
				u_int8_t fgIsHiddenSSID = FALSE;

				ucSSIDChar = '\0';

				prIeSsid = (struct IE_SSID *) pucIE;

				/* D-Link DWL-900AP+ */
				if (IE_LEN(pucIE) == 0)
					fgIsHiddenSSID = TRUE;
				/* Cisco AP1230A -
				 * (IE_LEN(pucIE) == 1)
				 * && (SSID_IE(pucIE)->aucSSID[0] == '\0')
				 */
				/* Linksys WRK54G/WL520g -
				 * (IE_LEN(pucIE) == n)
				 * && (SSID_IE(pucIE)->aucSSID[0~(n-1)] == '\0')
				 */
				else {
					for (i = 0; i < IE_LEN(pucIE); i++) {
						ucSSIDChar
							|= SSID_IE(pucIE)
								->aucSSID[i];
					}

					if (!ucSSIDChar)
						fgIsHiddenSSID = TRUE;
				}

				/* Update SSID to BSS Descriptor only if
				 * SSID is not hidden.
				 */
				if (!fgIsHiddenSSID) {
					COPY_SSID(prBssDesc->aucSSID,
						  prBssDesc->ucSSIDLen,
						  SSID_IE(pucIE)->aucSSID,
						  SSID_IE(pucIE)->ucLength);
				} else if (fgIsProbeResp) {
					/* SSID should be updated
					 * if it is ProbeResp
					 */
					kalMemZero(prBssDesc->aucSSID,
					sizeof(prBssDesc->aucSSID));
					prBssDesc->ucSSIDLen = 0;
				}

			}
			break;

		case ELEM_ID_SUP_RATES:
			/* NOTE(Kevin): Buffalo WHR-G54S's supported rate set
			 * IE exceed 8.
			 * IE_LEN(pucIE) == 12, "1(B), 2(B), 5.5(B), 6(B), 9(B),
			 * 11(B), 12(B), 18(B), 24(B), 36(B), 48(B), 54(B)"
			 */
			/* TP-LINK will set extra and incorrect ie
			 * with ELEM_ID_SUP_RATES
			 */
			if ((!prIeSupportedRate)
				&& (IE_LEN(pucIE) <= RATE_NUM_SW))
				prIeSupportedRate = SUP_RATES_IOT_IE(pucIE);
			break;

		case ELEM_ID_TIM:
			if (IE_LEN(pucIE) <= ELEM_MAX_LEN_TIM) {
				prBssDesc->fgTIMPresent = TRUE;
				prBssDesc->ucDTIMPeriod
					= TIM_IE(pucIE)->ucDTIMPeriod;
			}
			break;

		case ELEM_ID_IBSS_PARAM_SET:
			if (IE_LEN(pucIE) == ELEM_MAX_LEN_IBSS_PARAMETER_SET) {
				prBssDesc->u2ATIMWindow
					= IBSS_PARAM_IE(pucIE)->u2ATIMWindow;
			}
			break;

		case ELEM_ID_COUNTRY_INFO:
			prCountryIE = (struct IE_COUNTRY *) pucIE;
			break;

		case ELEM_ID_ERP_INFO:
			if (IE_LEN(pucIE) == ELEM_MAX_LEN_ERP)
				prBssDesc->fgIsERPPresent = TRUE;
			break;

		case ELEM_ID_EXTENDED_SUP_RATES:
			if (!prIeExtSupportedRate)
				prIeExtSupportedRate = EXT_SUP_RATES_IE(pucIE);
			break;

		case ELEM_ID_RSN:
			if (rsnParseRsnIE(prAdapter, RSN_IE(pucIE),
				&prBssDesc->rRSNInfo)) {
				uint8_t i;

				prBssDesc->fgIERSN = TRUE;
				prBssDesc->u2RsnCap
					= prBssDesc->rRSNInfo.u2RsnCap;

				for (i = 0; i < KAL_AIS_NUM; i++) {
					rsnCheckPmkidCache(prAdapter,
						prBssDesc,
						i);
				}
			}
			break;

		case ELEM_ID_HT_CAP:
		{
			/* Support AP Selection */
			struct IE_HT_CAP *prHtCap = (struct IE_HT_CAP *)pucIE;
			uint8_t ucSpatial = 0;
			uint8_t i = 0;
			/* end Support AP Selection */
			if (IE_LEN(pucIE) != (sizeof(struct IE_HT_CAP) - 2)) {
				DBGLOG(SCN, WARN,
					"HT_CAP wrong length(%d)->(%d)\n",
					(sizeof(struct IE_HT_CAP) - 2),
					IE_LEN(prHtCap));
				break;
			}
			prBssDesc->fgIsHTPresent = TRUE;

			/* Support AP Selection */
			if (prBssDesc->fgMultiAnttenaAndSTBC)
				break;

			for (; i < 4; i++) {
				if (prHtCap->rSupMcsSet.aucRxMcsBitmask[i] > 0)
					ucSpatial++;
			}

			prBssDesc->fgMultiAnttenaAndSTBC =
				((ucSpatial > 1) &&
				(prHtCap->u2HtCapInfo & HT_CAP_INFO_TX_STBC));
			/* end Support AP Selection */

			break;
		}
		case ELEM_ID_HT_OP:
			if (IE_LEN(pucIE) != (sizeof(struct IE_HT_OP) - 2))
				break;

			if ((((struct IE_HT_OP *) pucIE)->ucInfo1
				& HT_OP_INFO1_SCO) != CHNL_EXT_RES) {
				prBssDesc->eSco = (enum ENUM_CHNL_EXT)
				    (((struct IE_HT_OP *) pucIE)->ucInfo1
				    & HT_OP_INFO1_SCO);
			}
			break;
		case ELEM_ID_VHT_CAP:
			scanParseVHTCapIE(pucIE, prBssDesc);
			break;

		case ELEM_ID_VHT_OP:
			scanParseVHTOpIE(pucIE, prBssDesc);
			break;

#if CFG_SUPPORT_WAPI
		case ELEM_ID_WAPI:
			if (wapiParseWapiIE(WAPI_IE(pucIE),
				&prBssDesc->rIEWAPI))
				prBssDesc->fgIEWAPI = TRUE;
			break;
#endif
		/* Support AP Selection */
		case ELEM_ID_BSS_LOAD:
		{
			struct IE_BSS_LOAD *prBssLoad =
				(struct IE_BSS_LOAD *)pucIE;
			if (IE_LEN(prBssLoad) !=
				(sizeof(struct IE_BSS_LOAD) - 2)) {
				DBGLOG(SCN, WARN,
					"HE_CAP IE_LEN err(%d)->(%d)!\n",
					(sizeof(struct IE_BSS_LOAD) - 2),
					IE_LEN(prBssLoad));
				break;
			}

			prBssDesc->u2StaCnt = prBssLoad->u2StaCnt;
			prBssDesc->ucChnlUtilization =
				prBssLoad->ucChnlUtilizaion;
			prBssDesc->u2AvaliableAC = prBssLoad->u2AvailabeAC;
			prBssDesc->fgExsitBssLoadIE = TRUE;
			break;
		}
		/* end Support AP Selection */

		case ELEM_ID_VENDOR:	/* ELEM_ID_P2P, ELEM_ID_WMM */
		{
			uint8_t ucOuiType;
			uint16_t u2SubTypeVersion;

			if (rsnParseCheckForWFAInfoElem(prAdapter,
				pucIE, &ucOuiType, &u2SubTypeVersion)) {
				if ((ucOuiType == VENDOR_OUI_TYPE_WPA)
					&& (u2SubTypeVersion
					== VERSION_WPA)
					&& (rsnParseWpaIE(prAdapter,
						WPA_IE(pucIE),
						&prBssDesc
							->rWPAInfo))) {
					prBssDesc->fgIEWPA = TRUE;
				}
			}

			if (prBssDesc->fgIsVHTPresent == FALSE)
				scanCheckEpigramVhtIE(pucIE, prBssDesc);
#if CFG_SUPPORT_PASSPOINT
			/* since OSEN is mutual exclusion with RSN, so
			 * we reuse RSN here
			 */
			if ((pucIE[1] >= 10)
				&& (kalMemCmp(pucIE+2,
					"\x50\x6f\x9a\x12", 4) == 0)
				&& (rsnParseOsenIE(prAdapter,
					(struct IE_WFA_OSEN *)pucIE,
						&prBssDesc->rRSNInfo)))
				prBssDesc->fgIEOsen = TRUE;
#endif
#if CFG_ENABLE_WIFI_DIRECT
			if (prAdapter->fgIsP2PRegistered) {
				if ((p2pFuncParseCheckForP2PInfoElem(
					prAdapter, pucIE, &ucOuiType))
					&& (ucOuiType
					== VENDOR_OUI_TYPE_P2P)) {
					prBssDesc->fgIsP2PPresent
						= TRUE;
				}
			}
#endif /* CFG_ENABLE_WIFI_DIRECT */
#if CFG_SUPPORT_MBO
			if (pucIE[1] >= 4 &&
			    kalMemCmp(pucIE + 2, "\x50\x6f\x9a\x16", 4) == 0) {
				struct IE_MBO_OCE *mbo =
					(struct IE_MBO_OCE *)pucIE;
				const uint8_t *disallow = NULL;

				disallow = kalFindIeMatchMask(
						MBO_ATTR_ID_ASSOC_DISALLOW,
						mbo->aucSubElements,
						mbo->ucLength - 4,
						NULL, 0, 0, NULL);
				if (disallow && disallow[1] >= 1) {
					prBssDesc->fgIsDisallowed = TRUE;
					DBGLOG(SCN, INFO,
						MACSTR " disallow reason %d\n",
						MAC2STR(prBssDesc->aucBSSID),
						disallow[2]);
				}
			}
#endif
			scanCheckAdaptive11rIE(pucIE, prBssDesc);
			break;
		}
#if (CFG_SUPPORT_802_11AX == 1)
		case ELEM_ID_RESERVED:
			if (fgEfuseCtrlAxOn == 1) {
#if (CFG_SUPPORT_HE_ER == 1)
				if (IE_ID_EXT(pucIE) == ELEM_EXT_ID_HE_CAP) {
					prHeCap = (struct _IE_HE_CAP_T *) pucIE;
					if (IE_SIZE(prHeCap)
					    < (sizeof(struct _IE_HE_CAP_T))) {
						DBGLOG(SCN, WARN,
						    "HE_CAP IE_LEN err(%d)!\n",
						    IE_LEN(prHeCap));
						break;
					}
					prBssDesc->fgIsHEPresent = TRUE;
					prBssDesc->ucDCMMaxConRx =
					HE_GET_PHY_CAP_DCM_MAX_CONSTELLATION_RX(
						prHeCap->ucHePhyCap);
				}
				if (IE_ID_EXT(pucIE) == ELEM_EXT_ID_HE_OP) {
					prHeOp = (struct _IE_HE_OP_T *) pucIE;
					if (IE_SIZE(prHeOp)
					    < (sizeof(struct _IE_HE_OP_T))) {
						DBGLOG(SCN, WARN,
						    "HE_OP IE_LEN err(%d)!\n",
						    IE_LEN(prHeOp));
						break;
					}
					prBssDesc->fgIsERSUDisable =
					HE_IS_ER_SU_DISABLE(
						prHeOp->ucHeOpParams);
				}
				DBGLOG(SCN, INFO,
					"ER: BSSID:" MACSTR
					" SSID:%s,rx:%x, er:%x\n",
					MAC2STR(prBssDesc->aucBSSID),
					prBssDesc->aucSSID,
					prBssDesc->ucDCMMaxConRx,
					prBssDesc->fgIsERSUDisable);
#else
				if (IE_ID_EXT(pucIE) == ELEM_EXT_ID_HE_CAP)
					prBssDesc->fgIsHEPresent = TRUE;
#endif
			}
			break;
#endif
		case ELEM_ID_PWR_CONSTRAINT:
		{
			struct IE_POWER_CONSTRAINT *prPwrConstraint =
				(struct IE_POWER_CONSTRAINT *)pucIE;

			if (IE_LEN(pucIE) != 1)
				break;
			ucPowerConstraint =
				prPwrConstraint->ucLocalPowerConstraint;
			break;
		}
		case ELEM_ID_RRM_ENABLED_CAP:
			if (IE_LEN(pucIE) == 5) {
				/* RRM Capability IE is always 5 bytes */
				kalMemZero(prBssDesc->aucRrmCap,
					   sizeof(prBssDesc->aucRrmCap));
				kalMemCopy(prBssDesc->aucRrmCap, pucIE + 2,
					   sizeof(prBssDesc->aucRrmCap));
			}
			break;
#if (CFG_SUPPORT_802_11V_MBSSID == 1)
		case ELEM_ID_MBSSID:
			if (MBSSID_IE(pucIE)->ucMaxBSSIDIndicator <= 0 ||
				MBSSID_IE(pucIE)->ucMaxBSSIDIndicator > 8) {
				/* invalid Multiple BSSID ie
				* (must in range 1~8)
				*/
				break;
			}

			scanParsingMBSSIDSubelement(prAdapter,
						prBssDesc, MBSSID_IE(pucIE));
			break;
#endif

			/* no default */
		}
	}

	/* 4 <3.2> Save information from IEs - SSID */
	/* Update Flag of Hidden SSID for used in SEARCH STATE. */

	/* NOTE(Kevin): in current driver, the ucSSIDLen == 0 represent
	 * all cases of hidden SSID.
	 * If the fgIsHiddenSSID == TRUE, it means we didn't get the
	 * ProbeResp with valid SSID.
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
				      &prBssDesc->u2BSSBasicRateSet,
				      &prBssDesc->fgIsUnknownBssBasicRate);
	}

	/* 4 <4> Update information from HIF RX Header */
	{
		void *prRxStatus;
		uint8_t ucRxRCPI;

		prRxStatus = prSwRfb->prRxStatus;
		ASSERT(prRxStatus);

		/* 4 <4.1> Get TSF comparison result */
		RX_STATUS_GET(
			prRxDescOps,
			prBssDesc->fgIsLargerTSF,
			get_tcl,
			prRxStatus);

		/* 4 <4.2> Get Band information */
		prBssDesc->eBand = eHwBand;

		/* 4 <4.2> Get channel and RCPI information */
		RX_STATUS_GET(
			prRxDescOps,
			ucHwChannelNum,
			get_ch_num,
			prRxStatus);

		ASSERT(prSwRfb->prRxStatusGroup3);
		ucRxRCPI = nicRxGetRcpiValueFromRxv(prAdapter,
			RCPI_MODE_MAX, prSwRfb);
		if (prBssDesc->eBand == BAND_2G4) {

			/* Update RCPI if in right channel */

			if (ucIeDsChannelNum >= 1 && ucIeDsChannelNum <= 14) {

				/* Receive Beacon/ProbeResp frame
				 * from adjacent channel.
				 */
				if ((ucIeDsChannelNum == ucHwChannelNum)
					|| (ucRxRCPI > prBssDesc->ucRCPI))
					prBssDesc->ucRCPI = ucRxRCPI;
				/* trust channel information brought by IE */
				prBssDesc->ucChannelNum = ucIeDsChannelNum;
			} else if (ucIeHtChannelNum >= 1
				&& ucIeHtChannelNum <= 14) {
				/* Receive Beacon/ProbeResp frame
				 * from adjacent channel.
				 */
				if ((ucIeHtChannelNum == ucHwChannelNum)
					|| (ucRxRCPI > prBssDesc->ucRCPI))
					prBssDesc->ucRCPI = ucRxRCPI;
				/* trust channel information brought by IE */
				prBssDesc->ucChannelNum = ucIeHtChannelNum;
			} else {
				prBssDesc->ucRCPI = ucRxRCPI;

				prBssDesc->ucChannelNum = ucHwChannelNum;
			}
		}
		/* 5G Band */
		else {
			if (ucIeHtChannelNum >= 1 && ucIeHtChannelNum < 200) {
				/* Receive Beacon/ProbeResp frame
				 * from adjacent channel.
				 */
				if ((ucIeHtChannelNum == ucHwChannelNum)
					|| (ucRxRCPI > prBssDesc->ucRCPI))
					prBssDesc->ucRCPI = ucRxRCPI;
				/* trust channel information brought by IE */
				prBssDesc->ucChannelNum = ucIeHtChannelNum;
			} else {
				/* Always update RCPI */
				prBssDesc->ucRCPI = ucRxRCPI;

				prBssDesc->ucChannelNum = ucHwChannelNum;
			}
		}
	}

	/* 4 <5> Check IE information corret or not */
	if (!rlmDomainIsValidRfSetting(prAdapter, prBssDesc->eBand,
				       prBssDesc->ucChannelNum, prBssDesc->eSco,
				       prBssDesc->eChannelWidth,
				       prBssDesc->ucCenterFreqS1,
				       prBssDesc->ucCenterFreqS2)) {
#if 0 /* TODO: Remove this */
		/* Dump IE Inforamtion */
		log_dbg(RLM, WARN, "ScanAddToBssDesc IE Information\n");
		log_dbg(RLM, WARN, "IE Length = %d\n", u2IELength);
		log_mem8_dbg(RLM, WARN, pucDumpIE, u2IELength);
#endif

		/* Error Handling for Non-predicted IE - Fixed to set 20MHz */
		prBssDesc->eChannelWidth = CW_20_40MHZ;
		prBssDesc->ucCenterFreqS1 = 0;
		prBssDesc->ucCenterFreqS2 = 0;
		prBssDesc->eSco = CHNL_EXT_SCN;
	}
#if CFG_SUPPORT_802_11K
	if (prCountryIE && prCountryIE->ucLength ==
			(sizeof(struct IE_COUNTRY) - 2)) {
		uint8_t ucRemainLen = prCountryIE->ucLength - 3;
		struct COUNTRY_INFO_SUBBAND_TRIPLET *prSubBand =
			&prCountryIE->arCountryStr[0];
		const uint8_t ucSubBandSize =
			(uint8_t)sizeof(struct COUNTRY_INFO_SUBBAND_TRIPLET);
		int8_t cNewPwrLimit = RLM_INVALID_POWER_LIMIT;

		DBGLOG(SCN, LOUD,
			   "LM: Country IE of BSSID[" MACSTR "] is present\n",
			   MAC2STR(prBssDesc->aucBSSID));

		/* Try to find a country subband base on our channel */
		while (ucRemainLen >= ucSubBandSize) {
			if (prSubBand->ucFirstChnlNum < 201 &&
			    prBssDesc->ucChannelNum >=
				    prSubBand->ucFirstChnlNum &&
			    prBssDesc->ucChannelNum <=
				    (prSubBand->ucFirstChnlNum +
				     prSubBand->ucNumOfChnl - 1))
				break;
			ucRemainLen -= ucSubBandSize;
			prSubBand++;
		}
		/* Found a right country band */
		if (ucRemainLen >= ucSubBandSize) {
			cNewPwrLimit =
				prSubBand->cMaxTxPwrLv - ucPowerConstraint;
			/* Limit Tx power changed */
			if (prBssDesc->cPowerLimit != cNewPwrLimit) {
				prBssDesc->cPowerLimit = cNewPwrLimit;
				DBGLOG(SCN, TRACE,
				       "LM: Old TxPwrLimit %d,New: CountryMax %d, Constraint %d\n",
				       prBssDesc->cPowerLimit,
				       prSubBand->cMaxTxPwrLv,
				       ucPowerConstraint);
				/* should tell firmware to restrict tx power if
				** connected a BSS
				*/
				if (prBssDesc->fgIsConnected) {
					if (prBssDesc->cPowerLimit !=
					    RLM_INVALID_POWER_LIMIT)
						rlmSetMaxTxPwrLimit(
							prAdapter,
							prBssDesc->cPowerLimit,
							1);
					else
						rlmSetMaxTxPwrLimit(prAdapter,
								    0, 0);
				}
			}
		} else if (prBssDesc->cPowerLimit != RLM_INVALID_POWER_LIMIT) {
			DBGLOG(SCN, LOUD,
				"LM: The channel of BSSID[" MACSTR
				"] doesn't match with country IE, prBssDesc->cPowerLimit=%d\n",
				MAC2STR(prBssDesc->aucBSSID),
				prBssDesc->cPowerLimit);
			prBssDesc->cPowerLimit = RLM_INVALID_POWER_LIMIT;
			rlmSetMaxTxPwrLimit(prAdapter, 0, 0);
		}
	} else if (prBssDesc->cPowerLimit != RLM_INVALID_POWER_LIMIT) {
		DBGLOG(SCN, LOUD,
			"LM: Country IE of BSSID[" MACSTR
			"] isn't present, prBssDesc->cPowerLimit=%d\n",
			MAC2STR(prBssDesc->aucBSSID),
			prBssDesc->cPowerLimit);
		prBssDesc->cPowerLimit = RLM_INVALID_POWER_LIMIT;
		rlmSetMaxTxPwrLimit(prAdapter, 0, 0);
	}
#endif

	/* 4 <6> PHY type setting */
	prBssDesc->ucPhyTypeSet = 0;

	/* check if support 11n */
	if (prBssDesc->fgIsVHTPresent)
		prBssDesc->ucPhyTypeSet |= PHY_TYPE_BIT_VHT;

	if (prBssDesc->fgIsHTPresent)
		prBssDesc->ucPhyTypeSet |= PHY_TYPE_BIT_HT;

#if (CFG_SUPPORT_802_11AX == 1)
	if (fgEfuseCtrlAxOn == 1) {
		if (prBssDesc->fgIsHEPresent)
			prBssDesc->ucPhyTypeSet |= PHY_TYPE_BIT_HE;
	}
#endif

	/* if not 11n only */
	if (!(prBssDesc->u2BSSBasicRateSet & RATE_SET_BIT_HT_PHY)) {
		if (prBssDesc->eBand == BAND_2G4) {
			/* check if support 11g */
			if ((prBssDesc->u2OperationalRateSet & RATE_SET_OFDM)
			    || prBssDesc->fgIsERPPresent)
				prBssDesc->ucPhyTypeSet |= PHY_TYPE_BIT_ERP;

			/* if not 11g only */
			if (!(prBssDesc->u2BSSBasicRateSet & RATE_SET_OFDM)) {
				/* check if support 11b */
				if (prBssDesc->u2OperationalRateSet
				    & RATE_SET_HR_DSSS)
					prBssDesc->ucPhyTypeSet
						|= PHY_TYPE_BIT_HR_DSSS;
			}
		} else {
			prBssDesc->ucPhyTypeSet |= PHY_TYPE_BIT_OFDM;
		}
	}

	/* Support AP Selection */
	/* update update-index and reset seen-probe-response */
	if (prBssDesc->u4UpdateIdx !=
		prAdapter->rWifiVar.rScanInfo.u4ScanUpdateIdx) {
		prBssDesc->fgSeenProbeResp = FALSE;
		prBssDesc->u4UpdateIdx =
			prAdapter->rWifiVar.rScanInfo.u4ScanUpdateIdx;
	}

	/* check if it is a probe response frame */
	if (fgIsProbeResp)
		prBssDesc->fgSeenProbeResp = TRUE;
	/* end Support AP Selection */
	/* 4 <7> Update BSS_DESC_T's Last Update TimeStamp. */
	if (fgIsProbeResp || fgIsValidSsid)
		GET_CURRENT_SYSTIME(&prBssDesc->rUpdateTime);

#if CFG_SUPPORT_802_11K
	if (prBssDesc->fgIsConnected)
		rrmUpdateBssTimeTsf(prAdapter, prBssDesc);
#endif

	return prBssDesc;
}

/* clear all ESS scan result */
void scanInitEssResult(struct ADAPTER *prAdapter)
{
	prAdapter->rWlanInfo.u4ScanResultEssNum = 0;
	prAdapter->rWlanInfo.u4ScanDbgTimes1 = 0;
	prAdapter->rWlanInfo.u4ScanDbgTimes2 = 0;
	prAdapter->rWlanInfo.u4ScanDbgTimes3 = 0;
	prAdapter->rWlanInfo.u4ScanDbgTimes4 = 0;
	kalMemZero(prAdapter->rWlanInfo.arScanResultEss,
		sizeof(prAdapter->rWlanInfo.arScanResultEss));
}
/* print all ESS into log system once scan done
 * it is useful to log that, otherwise, we have no information to
 * identify if hardware has seen a specific AP,
 * if user complained some AP were not found in scan result list
 */
void scanLogEssResult(struct ADAPTER *prAdapter)
{
	struct ESS_SCAN_RESULT_T *prEssResult
		= &prAdapter->rWlanInfo.arScanResultEss[0];
	uint32_t u4ResultNum = prAdapter->rWlanInfo.u4ScanResultEssNum;
	uint32_t u4Index = 0;
	char *strbuf = NULL, *pos = NULL, *end = NULL;
	int slen = 0;
	u_int8_t first = TRUE;

	if (u4ResultNum == 0) {
		scanlog_dbg(LOG_SCAN_DONE_D2K, INFO, "0 Bss is found, %d, %d, %d, %d\n",
			prAdapter->rWlanInfo.u4ScanDbgTimes1,
			prAdapter->rWlanInfo.u4ScanDbgTimes2,
			prAdapter->rWlanInfo.u4ScanDbgTimes3,
			prAdapter->rWlanInfo.u4ScanDbgTimes4);
		return;
	}

	for (u4Index = 0; u4Index < u4ResultNum; u4Index++) {
		if (prEssResult[u4Index].u2SSIDLen > 0)
			slen += prEssResult[u4Index].u2SSIDLen + 2; /* _ssid;*/
	}

	slen = min(slen + 1, SCAN_LOG_MSG_MAX_LEN); /* 1 for null end*/
	pos = strbuf = kalMemAlloc(slen, VIR_MEM_TYPE);
	if (strbuf == NULL) {
		scanlog_dbg(LOG_SCAN_DONE_D2K, INFO, "Can't allocate memory\n");
		return;
	}
	end = strbuf + slen;
	for (u4Index = 0; u4Index < u4ResultNum; u4Index++) {
		uint8_t len = prEssResult[u4Index].u2SSIDLen;
		char ssid[PARAM_MAX_LEN_SSID + 1] = {0};

		if (len == 0)
			continue;
		if (pos + len + 3 > end) { /* _ssid;nul */
			pos = strbuf;
			if (first) {
				scanlog_dbg(LOG_SCAN_DONE_D2K, INFO,
					"Total:%u/%u %s", u4ResultNum,
					prAdapter->rWlanInfo.u4ScanResultNum,
					strbuf);
				first = FALSE;
			} else {
				scanlog_dbg(LOG_SCAN_DONE_D2K, INFO,
					"%s", strbuf);
			}
		}
		kalStrnCpy(ssid, prEssResult[u4Index].aucSSID, sizeof(ssid));
		ssid[sizeof(ssid) - 1] = '\0';
		pos += kalSnprintf(pos, end - pos, " %s;", ssid);
	}
	if (pos != strbuf) {
		if (first)
			scanlog_dbg(LOG_SCAN_DONE_D2K, INFO,
				"Total:%u/%u %s", u4ResultNum,
				prAdapter->rWlanInfo.u4ScanResultNum, strbuf);
		else
			scanlog_dbg(LOG_SCAN_DONE_D2K, INFO, "%s", strbuf);
	}
	kalMemFree(strbuf, VIR_MEM_TYPE, slen);
}

/* record all Scanned ESS, only one BSS was saved for each ESS, and AP who
 * is hidden ssid was excluded.
 */
/* maximum we only support record 64 ESSes */
static void scanAddEssResult(struct ADAPTER *prAdapter,
			     IN struct BSS_DESC *prBssDesc)
{
	struct ESS_SCAN_RESULT_T *prEssResult
		= &prAdapter->rWlanInfo.arScanResultEss[0];
	uint32_t u4Index = 0;

	if (prBssDesc->fgIsHiddenSSID)
		return;
	if (prAdapter->rWlanInfo.u4ScanResultEssNum >= CFG_MAX_NUM_BSS_LIST)
		return;
	for (; u4Index < prAdapter->rWlanInfo.u4ScanResultEssNum; u4Index++) {
		if (EQUAL_SSID(prEssResult[u4Index].aucSSID,
			(uint8_t)prEssResult[u4Index].u2SSIDLen,
			prBssDesc->aucSSID, prBssDesc->ucSSIDLen))
			return;
	}

	COPY_SSID(prEssResult[u4Index].aucSSID, prEssResult[u4Index].u2SSIDLen,
		prBssDesc->aucSSID, prBssDesc->ucSSIDLen);
	COPY_MAC_ADDR(prEssResult[u4Index].aucBSSID, prBssDesc->aucBSSID);
	prAdapter->rWlanInfo.u4ScanResultEssNum++;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief Convert the Beacon or ProbeResp Frame in SW_RFB_T to scan
 * result for query
 *
 * @param[in] prSwRfb            Pointer to the receiving SW_RFB_T structure.
 *
 * @retval WLAN_STATUS_SUCCESS   It is a valid Scan Result and been sent
 *                               to the host.
 * @retval WLAN_STATUS_FAILURE   It is not a valid Scan Result.
 */
/*----------------------------------------------------------------------------*/
uint32_t scanAddScanResult(IN struct ADAPTER *prAdapter,
			   IN struct BSS_DESC *prBssDesc,
			   IN struct SW_RFB *prSwRfb)
{
	struct SCAN_INFO *prScanInfo;
	uint8_t aucRatesEx[PARAM_MAX_LEN_RATES_EX];
	struct WLAN_BEACON_FRAME *prWlanBeaconFrame;
	uint8_t rMacAddr[PARAM_MAC_ADDR_LEN];
	struct PARAM_SSID rSsid;
	enum ENUM_PARAM_NETWORK_TYPE eNetworkType;
	struct PARAM_802_11_CONFIG rConfiguration;
	enum ENUM_PARAM_OP_MODE eOpMode;
	uint8_t ucRateLen = 0;
	uint32_t i;

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
		log_dbg(SCN, INFO, "Bss Desc type is P2P\n");
		return WLAN_STATUS_FAILURE;
	}

	prWlanBeaconFrame = (struct WLAN_BEACON_FRAME *) prSwRfb->pvHeader;
	COPY_MAC_ADDR(rMacAddr, prWlanBeaconFrame->aucBSSID);
	COPY_SSID(rSsid.aucSsid, rSsid.u4SsidLen,
		prBssDesc->aucSSID, prBssDesc->ucSSIDLen);

	rConfiguration.u4Length = sizeof(struct PARAM_802_11_CONFIG);
	rConfiguration.u4BeaconPeriod
		= (uint32_t) prWlanBeaconFrame->u2BeaconInterval;
	rConfiguration.u4ATIMWindow = prBssDesc->u2ATIMWindow;
	rConfiguration.u4DSConfig = nicChannelNum2Freq(prBssDesc->ucChannelNum);
	rConfiguration.rFHConfig.u4Length
		= sizeof(struct PARAM_802_11_CONFIG_FH);

	rateGetDataRatesFromRateSet(prBssDesc->u2OperationalRateSet, 0,
		aucRatesEx, &ucRateLen);

	/* NOTE(Kevin): Set unused entries, if any, at the end of the
	 * array to 0 from OID_802_11_BSSID_LIST
	 */
	for (i = ucRateLen; i < ARRAY_SIZE(aucRatesEx); i++)
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

	log_dbg(SCN, LOUD, "ind %s %d %d\n", prBssDesc->aucSSID,
		prBssDesc->ucChannelNum, prBssDesc->ucRCPI);

	scanAddEssResult(prAdapter, prBssDesc);
	if (prAdapter->rWifiVar.rScanInfo.fgSchedScanning &&
		test_bit(SUSPEND_FLAG_CLEAR_WHEN_RESUME,
			&prAdapter->ulSuspendFlag)) {
		uint8_t i = 0;
		struct BSS_DESC **pprPendBssDesc
			= &prScanInfo->rSchedScanParam.
				aprPendingBssDescToInd[0];

		for (; i < SCN_SSID_MATCH_MAX_NUM; i++) {
			if (pprPendBssDesc[i])
				continue;
			log_dbg(SCN, INFO, "indicate bss["
				MACSTR
				"] before wiphy resume, need to indicate again after wiphy resume\n",
				MAC2STR(prBssDesc->aucBSSID));
			pprPendBssDesc[i] = prBssDesc;
			break;
		}
	}

	if (prBssDesc->u2RawLength != 0) {
		kalIndicateBssInfo(prAdapter->prGlueInfo,
			   prBssDesc->aucRawBuf,
			   prBssDesc->u2RawLength,
			   prBssDesc->ucChannelNum,
			   RCPI_TO_dBm(prBssDesc->ucRCPI));
	}

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
		(uint8_t *) ((unsigned long) (prSwRfb->pvHeader)
			+ WLAN_MAC_MGMT_HEADER_LEN));

	return WLAN_STATUS_SUCCESS;

}	/* end of scanAddScanResult() */

u_int8_t scanCheckBssIsLegal(IN struct ADAPTER *prAdapter,
			     struct BSS_DESC *prBssDesc)
{
	u_int8_t fgAddToScanResult = FALSE;
	enum ENUM_BAND eBand;
	uint8_t ucChannel;

	ASSERT(prAdapter);
	/* check the channel is in the legal doamin */
	if (rlmDomainIsLegalChannel(prAdapter, prBssDesc->eBand,
		prBssDesc->ucChannelNum) == TRUE) {
		/* check ucChannelNum/eBand for adjacement channel filtering */
		if (cnmAisInfraChannelFixed(prAdapter,
			&eBand, &ucChannel) == TRUE &&
			(eBand != prBssDesc->eBand
			|| ucChannel != prBssDesc->ucChannelNum)) {
			fgAddToScanResult = FALSE;
		} else {
			fgAddToScanResult = TRUE;
		}
	}

	return fgAddToScanResult;

}

/*----------------------------------------------------------------------------*/
/*!
 * @brief Parse the content of given Beacon or ProbeResp Frame.
 *
 * @param[in] prSwRfb            Pointer to the receiving SW_RFB_T structure.
 *
 * @retval WLAN_STATUS_SUCCESS   if not report this SW_RFB_T to host
 * @retval WLAN_STATUS_PENDING   if report this SW_RFB_T to host as scan result
 */
/*----------------------------------------------------------------------------*/
uint32_t scanProcessBeaconAndProbeResp(IN struct ADAPTER *prAdapter,
				       IN struct SW_RFB *prSwRfb)
{
	struct SCAN_INFO *prScanInfo;
	struct BSS_DESC *prBssDesc = (struct BSS_DESC *) NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	struct WLAN_BEACON_FRAME *prWlanBeaconFrame
		= (struct WLAN_BEACON_FRAME *) NULL;
#if CFG_SLT_SUPPORT
	struct SLT_INFO *prSltInfo = (struct SLT_INFO *) NULL;
#endif
	uint32_t u4Idx = 0;

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);

	/* 4 <0> Ignore invalid Beacon Frame */
	if ((prSwRfb->u2PacketLen - prSwRfb->u2HeaderLen) <
		(TIMESTAMP_FIELD_LEN + BEACON_INTERVAL_FIELD_LEN
		+ CAP_INFO_FIELD_LEN)) {
		log_dbg(SCN, ERROR, "Ignore invalid Beacon Frame\n");
		return rStatus;
	}

	scanResultLog(prAdapter, prSwRfb);

#if CFG_SLT_SUPPORT
	prSltInfo = &prAdapter->rWifiVar.rSltInfo;

	if (prSltInfo->fgIsDUT) {
		log_dbg(P2P, INFO, "\n\rBCN: RX\n");
		prSltInfo->u4BeaconReceiveCnt++;
		return WLAN_STATUS_SUCCESS;
	} else {
		return WLAN_STATUS_SUCCESS;
	}
#endif

	prWlanBeaconFrame = (struct WLAN_BEACON_FRAME *) prSwRfb->pvHeader;

	/* 4 <1> Parse and add into BSS_DESC_T */
	prBssDesc = scanAddToBssDesc(prAdapter, prSwRfb);
	prAdapter->rWlanInfo.u4ScanDbgTimes1++;

	if (prBssDesc) {
		/* Full2Partial: save channel info for later scan */
		if (prScanInfo->fgIsScanForFull2Partial) {
			log_dbg(SCN, TRACE, "Full2Partial: set channel=%d\n",
					prBssDesc->ucChannelNum);
			scanSetBit(prBssDesc->ucChannelNum,
				prScanInfo->au4ChannelBitMap,
				sizeof(prScanInfo->au4ChannelBitMap));
		}

	for (u4Idx = 0; u4Idx < KAL_AIS_NUM; u4Idx++) {
		struct CONNECTION_SETTINGS *prConnSettings =
			aisGetConnSettings(prAdapter, u4Idx);
		struct BSS_INFO *prAisBssInfo =
			aisGetAisBssInfo(prAdapter, u4Idx);

		/* 4 <1.1> Beacon Change Detection for Connected BSS */
		if ((prAisBssInfo != NULL) &&
		    (prAisBssInfo->eConnectionState ==
		     MEDIA_STATE_CONNECTED) &&
		    ((prBssDesc->eBSSType == BSS_TYPE_INFRASTRUCTURE
		    && prConnSettings->eOPMode != NET_TYPE_IBSS)
		    || (prBssDesc->eBSSType == BSS_TYPE_IBSS
		    && prConnSettings->eOPMode != NET_TYPE_INFRA))
		    && EQUAL_MAC_ADDR(prBssDesc->aucBSSID,
		    prAisBssInfo->aucBSSID)
		    && EQUAL_SSID(prBssDesc->aucSSID, prBssDesc->ucSSIDLen,
		    prAisBssInfo->aucSSID, prAisBssInfo->ucSSIDLen)) {

#if CFG_SUPPORT_DETECT_SECURITY_MODE_CHANGE
			if (rsnCheckSecurityModeChanged(prAdapter,
				prAisBssInfo, prBssDesc)
#if CFG_SUPPORT_WAPI
				|| (aisGetWapiMode(prAdapter,
				u4Idx) &&
				!wapiPerformPolicySelection(prAdapter,
					prBssDesc, u4Idx))
#endif
				) {

				log_dbg(SCN, INFO, "Beacon security mode change detected\n");
				log_mem8_dbg(SCN, INFO,
					prSwRfb->pvHeader,
					prSwRfb->u2PacketLen);
				if (!prConnSettings
					->fgSecModeChangeStartTimer) {
					cnmTimerStartTimer(prAdapter,
						aisGetSecModeChangeTimer(
						prAdapter,
						u4Idx),
						SEC_TO_MSEC(3));
					prConnSettings
						->fgSecModeChangeStartTimer
							= TRUE;
				}
			} else {
				if (prConnSettings->fgSecModeChangeStartTimer) {
					cnmTimerStopTimer(prAdapter,
						aisGetSecModeChangeTimer(
						prAdapter,
						u4Idx));
					prConnSettings
						->fgSecModeChangeStartTimer
							= FALSE;
				}
			}
#endif
		}
		/* 4 <1.1> Update AIS_BSS_INFO */
		if ((prAisBssInfo != NULL) &&
		    ((prBssDesc->eBSSType == BSS_TYPE_INFRASTRUCTURE &&
		      prConnSettings->eOPMode != NET_TYPE_IBSS)
		     || (prBssDesc->eBSSType == BSS_TYPE_IBSS
		     && prConnSettings->eOPMode != NET_TYPE_INFRA))) {
			if (prAisBssInfo->eConnectionState
				== MEDIA_STATE_CONNECTED) {

				/* *not* checking prBssDesc->fgIsConnected
				 * anymore, due to Linksys AP uses " " as
				 * hidden SSID, and would have different
				 * BSS descriptor
				 */
				log_dbg(SCN, TRACE, "DTIMPeriod[%u] Present[%u] BSSID["
					MACSTR "]\n",
				       prAisBssInfo->ucDTIMPeriod,
				       prAisBssInfo->fgTIMPresent,
				       MAC2STR(prBssDesc->aucBSSID));
				if ((!prAisBssInfo->ucDTIMPeriod) &&
					prAisBssInfo->fgTIMPresent &&
					EQUAL_MAC_ADDR(prBssDesc->aucBSSID,
						prAisBssInfo->aucBSSID) &&
					(prAisBssInfo->eCurrentOPMode
					== OP_MODE_INFRASTRUCTURE) &&
					((prWlanBeaconFrame->u2FrameCtrl
					& MASK_FRAME_TYPE)
					== MAC_FRAME_BEACON)) {
					prAisBssInfo->ucDTIMPeriod
						= prBssDesc->ucDTIMPeriod;
					prAisBssInfo->fgTIMPresent
						= prBssDesc->fgTIMPresent;

					/* Handle No TIM IE information case */
					if (!prAisBssInfo->fgTIMPresent) {
						enum PARAM_POWER_MODE ePwrMode
							= Param_PowerModeCAM;

						log_dbg(SCN, WARN, "IE TIM absence, set to CAM mode!\n");
						nicConfigPowerSaveProfile(
							prAdapter,
							prAisBssInfo->
							ucBssIndex, ePwrMode,
							FALSE,
							PS_CALLER_NO_TIM);
					}
					/* sync with firmware for
					 * beacon information
					 */
					nicPmIndicateBssConnected(prAdapter,
						prAisBssInfo->ucBssIndex);
				}
			}
#if CFG_SUPPORT_ADHOC
			if (EQUAL_SSID(prBssDesc->aucSSID,
				prBssDesc->ucSSIDLen,
				prConnSettings->aucSSID,
				prConnSettings->ucSSIDLen) &&
				(prBssDesc->eBSSType == BSS_TYPE_IBSS)
				&& (prAisBssInfo->eCurrentOPMode
				== OP_MODE_IBSS)) {

				ASSERT(prSwRfb->prRxStatusGroup3);

				ibssProcessMatchedBeacon(prAdapter,
					prAisBssInfo,
					prBssDesc,
					nicRxGetRcpiValueFromRxv(
						prAdapter,
						RCPI_MODE_MAX,
						prSwRfb));
			}
#endif /* CFG_SUPPORT_ADHOC */
		}
	}

		rlmProcessBcn(prAdapter,
			prSwRfb,
			((struct WLAN_BEACON_FRAME *) (prSwRfb->pvHeader))
				->aucInfoElem,
			(prSwRfb->u2PacketLen - prSwRfb->u2HeaderLen) -
			(uint16_t) (OFFSET_OF(struct WLAN_BEACON_FRAME_BODY,
				aucInfoElem[0])));

		mqmProcessBcn(prAdapter,
			prSwRfb,
			((struct WLAN_BEACON_FRAME *) (prSwRfb->pvHeader))
				->aucInfoElem,
			(prSwRfb->u2PacketLen - prSwRfb->u2HeaderLen) -
			(uint16_t) (OFFSET_OF(struct WLAN_BEACON_FRAME_BODY,
				aucInfoElem[0])));

		prAdapter->rWlanInfo.u4ScanDbgTimes2++;

		/* 4 <3> Send SW_RFB_T to HIF when we perform SCAN for HOST */
		if (prBssDesc->eBSSType == BSS_TYPE_INFRASTRUCTURE
			|| prBssDesc->eBSSType == BSS_TYPE_IBSS) {
			/* for AIS, send to host */
			prAdapter->rWlanInfo.u4ScanDbgTimes3++;
			if (prScanInfo->eCurrentState == SCAN_STATE_SCANNING
				|| prScanInfo->fgSchedScanning) {
				u_int8_t fgAddToScanResult = FALSE;

				fgAddToScanResult
					= scanCheckBssIsLegal(prAdapter,
						prBssDesc);
				prAdapter->rWlanInfo.u4ScanDbgTimes4++;

				if (fgAddToScanResult == TRUE) {
					rStatus = scanAddScanResult(prAdapter,
						prBssDesc, prSwRfb);
				}
			}
		}
#if CFG_ENABLE_WIFI_DIRECT
		if (prAdapter->fgIsP2PRegistered) {
			scanP2pProcessBeaconAndProbeResp(prAdapter, prSwRfb,
				&rStatus, prBssDesc, prWlanBeaconFrame);
		}
#endif
	}

	return rStatus;
}	/* end of scanProcessBeaconAndProbeResp() */

/*----------------------------------------------------------------------------*/
/*!
 * \brief Search the Candidate of BSS Descriptor for JOIN(Infrastructure) or
 *        MERGE(AdHoc) according to current Connection Policy.
 *
 * \return   Pointer to BSS Descriptor, if found. NULL, if not found
 */
/*----------------------------------------------------------------------------*/
struct BSS_DESC *scanSearchBssDescByPolicy(
	IN struct ADAPTER *prAdapter, IN uint8_t ucBssIndex)
{
	struct CONNECTION_SETTINGS *prConnSettings;
	struct BSS_INFO *prBssInfo;
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecBssInfo;
	struct SCAN_INFO *prScanInfo;

	struct LINK *prBSSDescList;

	struct BSS_DESC *prBssDesc = (struct BSS_DESC *) NULL;
	struct BSS_DESC *prPrimaryBssDesc = (struct BSS_DESC *) NULL;
	struct BSS_DESC *prCandidateBssDesc = (struct BSS_DESC *) NULL;

	struct STA_RECORD *prStaRec = (struct STA_RECORD *) NULL;
	struct STA_RECORD *prPrimaryStaRec;
	struct STA_RECORD *prCandidateStaRec = (struct STA_RECORD *) NULL;

	OS_SYSTIME rCurrentTime;

	/* The first one reach the check point will be our candidate */
	u_int8_t fgIsFindFirst = (u_int8_t) FALSE;

	u_int8_t fgIsFindBestRSSI = (u_int8_t) FALSE;
	u_int8_t fgIsFindBestEncryptionLevel = (u_int8_t) FALSE;
	/* u_int8_t fgIsFindMinChannelLoad = (u_int8_t) FALSE; */

	/* TODO(Kevin): Support Min Channel Load */
	/* uint8_t aucChannelLoad[CHANNEL_NUM] = {0}; */

	u_int8_t fgIsFixedChannel = (u_int8_t) FALSE;
	enum ENUM_BAND eBand = BAND_2G4;
	uint8_t ucChannel = 0;
	uint32_t u4ScnAdhocBssDescTimeout = 0;
#if CFG_SUPPORT_NCHO
	uint8_t ucRCPIStep = ROAMING_NO_SWING_RCPI_STEP;
#endif

	ASSERT(prAdapter);

	prConnSettings =
		aisGetConnSettings(prAdapter, ucBssIndex);
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

	prAisSpecBssInfo =
		aisGetAisSpecBssInfo(prAdapter, ucBssIndex);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prBSSDescList = &prScanInfo->rBSSDescList;

	GET_CURRENT_SYSTIME(&rCurrentTime);

	/* check for fixed channel operation */
	if (prBssInfo->eNetworkType == NETWORK_TYPE_AIS) {
#if CFG_SUPPORT_CHNL_CONFLICT_REVISE
		fgIsFixedChannel =
			cnmAisDetectP2PChannel(prAdapter, &eBand, &ucChannel);
#else
		fgIsFixedChannel =
			cnmAisInfraChannelFixed(prAdapter, &eBand, &ucChannel);
#endif
	} else
		fgIsFixedChannel = FALSE;

#if DBG
	if (prConnSettings->ucSSIDLen < ELEM_MAX_LEN_SSID)
		prConnSettings->aucSSID[prConnSettings->ucSSIDLen] = '\0';
#endif

	log_dbg(SCN, INFO, "SEARCH: Bss Num: %d, Look for SSID: %s, "
		MACSTR " Band=%d, channel=%d\n",
		(uint32_t) prBSSDescList->u4NumElem,
		HIDE(prConnSettings->aucSSID),
		MAC2STR(prConnSettings->aucBSSID), eBand, ucChannel);

	/* 4 <1> The outer loop to search for a candidate. */
	LINK_FOR_EACH_ENTRY(
		prBssDesc, prBSSDescList, rLinkEntry, struct BSS_DESC) {

		/* TODO(Kevin): Update Minimum Channel Load Information here */

#if 0
		log_dbg(SCN, INFO, "SEARCH: [" MACSTR "], SSID:%s\n",
			MAC2STR(prBssDesc->aucBSSID), prBssDesc->aucSSID);
#endif

		/* 4 <2> Check PHY Type and attributes */
		/* 4 <2.1> Check Unsupported BSS PHY Type */
		if (!(prBssDesc->ucPhyTypeSet
			& (prAdapter->rWifiVar.ucAvailablePhyTypeSet))) {
			log_dbg(SCN, INFO, "SEARCH: Ignore unsupported ucPhyTypeSet = %x\n",
				prBssDesc->ucPhyTypeSet);
			continue;
		}
		/* 4 <2.2> Check if has unknown NonHT BSS Basic Rate Set. */
		if (prBssDesc->fgIsUnknownBssBasicRate) {
			log_dbg(SCN, LOUD, "SEARCH: Ignore Unknown Bss Basic Rate\n");
			continue;
		}
		/* 4 <2.3> Check if fixed operation cases should be aware */
		if (fgIsFixedChannel == TRUE
			&& (prBssDesc->eBand != eBand
				|| prBssDesc->ucChannelNum != ucChannel)) {
			log_dbg(SCN, LOUD, "SEARCH: Ignore BssBand[%d] != FixBand[%d] or BssCH[%d] != FixCH[%d]\n",
				prBssDesc->eBand, eBand,
				prBssDesc->ucChannelNum, ucChannel);
			continue;
			}
		/* 4 <2.4> Check if the channel is legal under regulatory
		 * domain
		 */
		if (rlmDomainIsLegalChannel(prAdapter, prBssDesc->eBand,
			prBssDesc->ucChannelNum) == FALSE) {
			log_dbg(SCN, LOUD, "SEARCH: Ignore illegal CH Band[%d] CH[%d]\n",
				prBssDesc->eBand, prBssDesc->ucChannelNum);
			continue;
		}
		/* 4 <2.5> Check if this BSS_DESC_T is stale */
		u4ScnAdhocBssDescTimeout = SCN_BSS_DESC_STALE_SEC;
#if CFG_ENABLE_WIFI_DIRECT
#if CFG_SUPPORT_WFD
		if (prAdapter->rWifiVar.rWfdConfigureSettings.ucWfdEnable)
			u4ScnAdhocBssDescTimeout = SCN_BSS_DESC_STALE_SEC_WFD;
#endif
#endif
		if (CHECK_FOR_TIMEOUT(rCurrentTime, prBssDesc->rUpdateTime,
			SEC_TO_SYSTIME(u4ScnAdhocBssDescTimeout))) {
			log_dbg(SCN, LOUD, "SEARCH: Ignore stale Bss, CurrTime[%u] BssUpdateTime[%u]\n",
				rCurrentTime, prBssDesc->rUpdateTime);
			continue;
		}
		/* 4 <3> Check if reach the excessive join retry limit */
		/* NOTE(Kevin): STA_RECORD_T is recorded by TA. */
		prStaRec = cnmGetStaRecByAddress(
			prAdapter, ucBssIndex, prBssDesc->aucSrcAddr);

		/* NOTE(Kevin):
		 * The Status Code is the result of a Previous Connection
		 * Request,we use this as SCORE for choosing a proper candidate
		 * (Also used for compare see <6>) The Reason Code is an
		 * indication of the reason why AP reject us, we use this Code
		 * for "Reject" a SCAN result to become our candidate(Like a
		 *  blacklist).
		 */
#if 0		/* TODO(Kevin): */
		if (prStaRec
			&& prStaRec->u2ReasonCode != REASON_CODE_RESERVED) {
			log_dbg(SCN, INFO, "SEARCH: Ignore BSS with previous Reason Code = %d\n",
				prStaRec->u2ReasonCode);
			continue;
		} else
#endif
		if (prStaRec
			&& prStaRec->u2StatusCode != STATUS_CODE_SUCCESSFUL) {
			/* NOTE(Kevin): greedy association - after timeout,
			 * we'll still try to associate to the AP whose STATUS
			 * of conection attempt was not success. We may also use
			 * (ucJoinFailureCount x JOIN_RETRY_INTERVAL_SEC) for
			 * time bound.
			 */
			if ((prStaRec->ucJoinFailureCount
				< JOIN_MAX_RETRY_FAILURE_COUNT)
				|| (CHECK_FOR_TIMEOUT(rCurrentTime,
					prStaRec->rLastJoinTime,
					SEC_TO_SYSTIME(JOIN_RETRY_INTERVAL_SEC)
				))) {

				/* NOTE(Kevin): Every JOIN_RETRY_INTERVAL_SEC
				 * interval, we can retry
				 * JOIN_MAX_RETRY_FAILURE_COUNT times.
				 */
				if (prStaRec->ucJoinFailureCount
					>= JOIN_MAX_RETRY_FAILURE_COUNT) {
					prStaRec->ucJoinFailureCount = 0;
				}

				log_dbg(SCN, INFO, "SEARCH:Try to join BSS again,Status Code=%u(Curr=%u/Last Join=%u)\n",
					prStaRec->u2StatusCode, rCurrentTime,
					prStaRec->rLastJoinTime);
			} else {
				log_dbg(SCN, INFO, "SEARCH: Ignore BSS which reach maximum Join Retry Count = %d\n",
					JOIN_MAX_RETRY_FAILURE_COUNT);
				continue;
			}
		}

		/* 4 <4> Check for various NETWORK conditions */
		if (prBssInfo->eNetworkType == NETWORK_TYPE_AIS) {
			enum ENUM_BSS_TYPE eBSSType =
				prBssDesc->eBSSType;
			enum ENUM_PARAM_OP_MODE eOPMode =
				prConnSettings->eOPMode;
			/* 4 <4.1> Check BSS Type for the corresponding
			 * Operation Mode in Connection Setting
			 */
			/* NOTE(Kevin): For NET_TYPE_AUTO_SWITCH, we will always
			 * pass following check.
			 */
			if (eOPMode == NET_TYPE_INFRA
				&& eBSSType != BSS_TYPE_INFRASTRUCTURE) {
				log_dbg(SCN, INFO, "SEARCH: Ignore eBSSType = IBSS\n");
				continue;
			}
			if ((eOPMode == NET_TYPE_IBSS
				|| eOPMode == NET_TYPE_DEDICATED_IBSS)
				&& eBSSType != BSS_TYPE_IBSS) {
				log_dbg(SCN, INFO, "SEARCH: Ignore eBSSType = INFRASTRUCTURE\n");
				continue;
			}
			/* 4 <4.2> Check AP's BSSID if OID_802_11_BSSID has been
			 * set.
			 */
			if (prConnSettings->fgIsConnByBssidIssued &&
				eBSSType == BSS_TYPE_INFRASTRUCTURE) {
				if (UNEQUAL_MAC_ADDR(prConnSettings->aucBSSID,
					prBssDesc->aucBSSID)) {
					log_dbg(SCN, TRACE, "SEARCH: Ignore due to BSSID was not matched!\n");
					continue;
				}
			}
#if CFG_SUPPORT_ADHOC
			/* 4 <4.3> Check for AdHoc Mode */
			if (eBSSType == BSS_TYPE_IBSS) {
				OS_SYSTIME rCurrentTime;

				u4ScnAdhocBssDescTimeout =
					SCN_ADHOC_BSS_DESC_TIMEOUT_SEC;

				/* 4 <4.3.1> Check if this SCAN record has been
				 * updated recently for IBSS.
				 */
				/* NOTE(Kevin): Because some STA may change its
				 * BSSID frequently after it create the IBSS -
				 * e.g. IPN2220, so we need to make sure we get
				 * the new one. For BSS, if the old record was
				 * matched, however it won't be able to pass the
				 * Join Process later.
				 */
				GET_CURRENT_SYSTIME(&rCurrentTime);
#if CFG_ENABLE_WIFI_DIRECT
#if CFG_SUPPORT_WFD
				if (prAdapter->rWifiVar
					.rWfdConfigureSettings.ucWfdEnable) {
#define __LOCAL_VAR__ SCN_ADHOC_BSS_DESC_TIMEOUT_SEC_WFD
					u4ScnAdhocBssDescTimeout
						= __LOCAL_VAR__;
#undef __LOCAL_VAR__
				}
#endif
#endif
				if (CHECK_FOR_TIMEOUT(rCurrentTime,
					prBssDesc->rUpdateTime,
					SEC_TO_SYSTIME(
						u4ScnAdhocBssDescTimeout))) {
					log_dbg(SCN, LOUD, "SEARCH: Now(%u) Skip old record of BSS Descriptor(%u) - BSSID:["
						MACSTR "]\n",
						rCurrentTime,
						prBssDesc->rUpdateTime,
						MAC2STR(prBssDesc->aucBSSID));
					continue;
				}

				/* 4 <4.3.2> Check Peer's capability */
				if (ibssCheckCapabilityForAdHocMode(prAdapter,
					prBssDesc, ucBssIndex)
					== WLAN_STATUS_FAILURE) {

					log_dbg(SCN, INFO, "SEARCH: Ignore BSS DESC MAC: "
						MACSTR
						", Capability is not supported for current AdHoc Mode.\n",
						MAC2STR(prPrimaryBssDesc
							->aucBSSID));

					continue;
				}

				/* 4 <4.3.3> Compare TSF */
				if (prBssInfo->fgIsBeaconActivated &&
					UNEQUAL_MAC_ADDR(prBssInfo->aucBSSID,
					prBssDesc->aucBSSID)) {

					log_dbg(SCN, LOUD, "SEARCH: prBssDesc->fgIsLargerTSF = %d\n",
						prBssDesc->fgIsLargerTSF);

					if (!prBssDesc->fgIsLargerTSF) {
						log_dbg(SCN, INFO, "SEARCH: Ignore BSS DESC MAC: ["
							MACSTR
							"], Smaller TSF\n",
							MAC2STR(prBssDesc
								->aucBSSID));
						continue;
					}
				}
			}
#endif /* CFG_SUPPORT_ADHOC */

		}
#if 0		/* TODO(Kevin): For IBSS */
		/* 4 <2.c> Check if this SCAN record has been updated recently
		 * for IBSS.
		 */
		/* NOTE(Kevin): Because some STA may change its BSSID frequently
		 * after it create the IBSS, so we need to make sure we get the
		 * new one. For BSS, if the old record was matched, however it
		 * won't be able to pass the Join Process later.
		 */
		if (prBssDesc->eBSSType == BSS_TYPE_IBSS) {
			OS_SYSTIME rCurrentTime;

			GET_CURRENT_SYSTIME(&rCurrentTime);
			if (CHECK_FOR_TIMEOUT(rCurrentTime,
						prBssDesc->rUpdateTime,
						SEC_TO_SYSTIME(
							BSS_DESC_TIMEOUT_SEC)
						)){
				log_dbg(SCAN, TRACE, "Skip old record of BSS Descriptor - BSSID:["
					MACSTR "]\n\n",
					MAC2STR(prBssDesc->aucBSSID));
				continue;
			}
		}

		if ((prBssDesc->eBSSType == BSS_TYPE_INFRASTRUCTURE) &&
			(prAdapter->eConnectionState
				== MEDIA_STATE_CONNECTED)) {
			OS_SYSTIME rCurrentTime;

			GET_CURRENT_SYSTIME(&rCurrentTime);
			if (CHECK_FOR_TIMEOUT(rCurrentTime,
					prBssDesc->rUpdateTime,
					SEC_TO_SYSTIME(BSS_DESC_TIMEOUT_SEC))) {
				log_dbg(SCAN, TRACE, "Skip old record of BSS Descriptor - BSSID:["
					MACSTR "]\n\n",
					MAC2STR(prBssDesc->aucBSSID));
				continue;
			}
		}

		/* 4 <4B> Check for IBSS AdHoc Mode. */
		/* Skip if one or more BSS Basic Rate are not supported by
		 * current AdHocMode
		 */
		if (prPrimaryBssDesc->eBSSType == BSS_TYPE_IBSS) {
			/* 4 <4B.1> Check if match the Capability of current
			 * IBSS AdHoc Mode.
			 */
			if (ibssCheckCapabilityForAdHocMode(prAdapter,
				prPrimaryBssDesc) == WLAN_STATUS_FAILURE) {

				log_dbg(SCAN, TRACE, "Ignore BSS DESC MAC: "
					MACSTR
					", Capability is not supported for current AdHoc Mode.\n",
					MAC2STR(prPrimaryBssDesc->aucBSSID));

				continue;
			}

			/* 4 <4B.2> IBSS Merge Decision Flow for SEARCH STATE.
			 */
			if (prAdapter->fgIsIBSSActive &&
				UNEQUAL_MAC_ADDR(prBssInfo->aucBSSID,
				prPrimaryBssDesc->aucBSSID)) {

				if (!fgIsLocalTSFRead) {
					NIC_GET_CURRENT_TSF(prAdapter,
						&rCurrentTsf);

					log_dbg(SCAN, TRACE, "\n\nCurrent TSF : %08lx-%08lx\n\n",
						rCurrentTsf.u.HighPart,
						rCurrentTsf.u.LowPart);
				}

				if (rCurrentTsf.QuadPart
					> prPrimaryBssDesc
						->u8TimeStamp.QuadPart) {
					log_dbg(SCAN, TRACE, "Ignore BSS DESC MAC: ["
						MACSTR"], Current BSSID: ["
						MACSTR "].\n",
						MAC2STR(prPrimaryBssDesc
							->aucBSSID),
						MAC2STR(prBssInfo->aucBSSID));

					log_dbg(SCAN, TRACE, "\n\nBSS's TSF : %08lx-%08lx\n\n",
						prPrimaryBssDesc
							->u8TimeStamp
								.u.HighPart,
						prPrimaryBssDesc
							->u8TimeStamp
								.u.LowPart);

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
			/* Can't pass the Encryption Status
			 * Check, get next one
			 */
			continue;
		}

		/* For RSN Pre-authentication, update the PMKID canidate
		 * list for same SSID and encrypt status
		 */
		/* Update PMKID candicate list. */
		if (prAdapter->rWifiVar.rConnSettings.eAuthMode
			== AUTH_MODE_WPA2) {
			rsnUpdatePmkidCandidateList(prPrimaryBssDesc);
			if (prAdapter->rWifiVar.rAisBssInfo
				.u4PmkidCandicateCount) {
				prAdapter->rWifiVar
					.rAisBssInfo
					.fgIndicatePMKID
						= rsnCheckPmkidCandicate();
			}
		}
#endif

		prPrimaryBssDesc = (struct BSS_DESC *) NULL;

		/* 4 <6> Check current Connection Policy. */
		switch (prConnSettings->eConnectionPolicy) {
		case CONNECT_BY_SSID_BEST_RSSI:
			/* Choose Hidden SSID to join only if
			 * the `fgIsEnableJoin...` is TRUE
			 */
			if (prAdapter->rWifiVar.fgEnableJoinToHiddenSSID
				&& prBssDesc->fgIsHiddenSSID) {
				/* NOTE(Kevin): following if () statement
				 * means that If Target is hidden, then we
				 * won't connect when user specify
				 * SSID_ANY policy.
				 */
				if (prConnSettings->ucSSIDLen) {
					prPrimaryBssDesc = prBssDesc;

					fgIsFindBestRSSI = TRUE;
				}

			} else if (EQUAL_SSID(prBssDesc->aucSSID,
					      prBssDesc->ucSSIDLen,
					      prConnSettings->aucSSID,
					      prConnSettings->ucSSIDLen)) {
				prPrimaryBssDesc = prBssDesc;

				fgIsFindBestRSSI = TRUE;

				log_dbg(SCN, LOUD, "SEARCH: Found BSS by SSID, ["
					MACSTR "], SSID:%s\n",
					MAC2STR(prBssDesc->aucBSSID),
					HIDE(prBssDesc->aucSSID));
			}
			break;

		case CONNECT_BY_SSID_ANY:
			/* NOTE(Kevin): In this policy, we don't know the
			 * desired SSID from user, so we should exclude the
			 * Hidden SSID from scan list. And because we refuse
			 * to connect to Hidden SSID node at the beginning, so
			 * when the JOIN Module deal with a struct BSS_DESC
			 * which has fgIsHiddenSSID == TRUE, then the
			 * Connection Settings must be valid without doubt.
			 */
			if (!prBssDesc->fgIsHiddenSSID) {
				prPrimaryBssDesc = prBssDesc;

				fgIsFindFirst = TRUE;
			}
			break;

		case CONNECT_BY_BSSID:
			if (EQUAL_MAC_ADDR(prBssDesc->aucBSSID,
				prConnSettings->aucBSSID)) {

				/* Make sure to match with SSID if supplied.
				 * Some old dualband APs share a single BSSID
				 * among different BSSes.
				 */
				if ((prBssDesc->ucSSIDLen > 0 &&
					prConnSettings->ucSSIDLen > 0 &&
					EQUAL_SSID(prBssDesc->aucSSID,
						prBssDesc->ucSSIDLen,
						prConnSettings->aucSSID,
						prConnSettings->ucSSIDLen)) ||
					prConnSettings->ucSSIDLen == 0) {
					log_dbg(SCN, LOUD, "%s: BSSID/SSID pair matched\n",
							__func__);
					prPrimaryBssDesc = prBssDesc;
				} else
					log_dbg(SCN, ERROR, "%s: BSSID/SSID pair unmatched ("
						MACSTR
						")\n", __func__,
						MAC2STR(prBssDesc->aucBSSID));
			}
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
			if (aisGetWapiMode(prAdapter, ucBssIndex)) {
				if (wapiPerformPolicySelection(prAdapter,
					prPrimaryBssDesc, ucBssIndex)) {
					fgIsFindFirst = TRUE;
				} else {
					/* Can't pass the Encryption Status
					 * Check, get next one
					 */
					log_dbg(RSN, INFO, "Ignore BSS can't pass WAPI policy selection\n");
					continue;
				}
			} else
#endif
			if (rsnPerformPolicySelection(prAdapter,
				prPrimaryBssDesc, ucBssIndex)) {
				if (prAisSpecBssInfo->fgCounterMeasure) {
					log_dbg(RSN, INFO, "Skip while at counter measure period!!!\n");
					continue;
				}

				if (prPrimaryBssDesc->ucEncLevel > 0) {
					fgIsFindBestEncryptionLevel = TRUE;

					fgIsFindFirst = FALSE;
				}
			} else {
				/* Can't pass the Encryption Status Check,
				 * get next one
				 */
				log_dbg(RSN, INFO, "Ignore BSS can't pass Encryption Status Check\n");
				continue;
			}
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
			/* 4 <6D> Condition - Visible SSID win Hidden SSID. */
			if (prCandidateBssDesc->fgIsHiddenSSID) {
				if (!prPrimaryBssDesc->fgIsHiddenSSID) {
					/* The non Hidden SSID win. */
					prCandidateBssDesc = prPrimaryBssDesc;

					prCandidateStaRec = prPrimaryStaRec;
					continue;
				}
			} else {
				if (prPrimaryBssDesc->fgIsHiddenSSID)
					continue;
			}

			/* 4 <6E> Condition - Choose the one with
			 * better RCPI(RSSI).
			 */
			if (fgIsFindBestRSSI) {
				/* TODO(Kevin): We shouldn't compare the actual
				 * value, we should allow some acceptable
				 * tolerance of some RSSI percentage here.
				 */
				log_dbg(SCN, TRACE, "Candidate ["
				MACSTR
				"]: uint8_t = %d, joinFailCnt=%d, Primary ["
				MACSTR "]: uint8_t = %d, joinFailCnt=%d\n",
					MAC2STR(prCandidateBssDesc->aucBSSID),
					prCandidateBssDesc->ucRCPI,
					prCandidateBssDesc->ucJoinFailureCount,
					MAC2STR(prPrimaryBssDesc->aucBSSID),
					prPrimaryBssDesc->ucRCPI,
					prPrimaryBssDesc->ucJoinFailureCount);

				ASSERT(!(prCandidateBssDesc->fgIsConnected
					&& prPrimaryBssDesc->fgIsConnected));
				if (prPrimaryBssDesc->ucJoinFailureCount
					> SCN_BSS_JOIN_FAIL_THRESOLD) {
					/* give a chance to do join if join
					 * fail before
					 * SCN_BSS_DECRASE_JOIN_FAIL_CNT_SEC
					 * seconds
					 */
#define __LOCAL_VAR__ \
SCN_BSS_JOIN_FAIL_CNT_RESET_SEC
					if (CHECK_FOR_TIMEOUT(rCurrentTime,
						prBssDesc->rJoinFailTime,
						SEC_TO_SYSTIME(
							__LOCAL_VAR__))) {
#define __LOCAL_VAR2__ \
SCN_BSS_JOIN_FAIL_RESET_STEP

						prBssDesc->ucJoinFailureCount
							-= __LOCAL_VAR2__;
#undef __LOCAL_VAR2__

						log_dbg(AIS, INFO, "decrease join fail count for Bss "
						MACSTR
						" to %u, timeout second %d\n",
							MAC2STR(
							prBssDesc->aucBSSID),
							prBssDesc
							->ucJoinFailureCount,
							__LOCAL_VAR__);
					}
#undef __LOCAL_VAR__
				}
				/* NOTE: To prevent SWING, we do roaming only
				 * if target AP has at least 5dBm larger
				 * than us.
				 */
#if CFG_SUPPORT_NCHO
				if (prAdapter->rNchoInfo.fgNCHOEnabled
					== TRUE) {
					ucRCPIStep = 2 * prAdapter
						->rNchoInfo.i4RoamDelta;
				}
#endif
				if (prCandidateBssDesc->fgIsConnected) {
					if ((prCandidateBssDesc->ucRCPI
					     + ROAMING_NO_SWING_RCPI_STEP <=
					     prPrimaryBssDesc->ucRCPI)
					    && prPrimaryBssDesc
					    ->ucJoinFailureCount
					    <= SCN_BSS_JOIN_FAIL_THRESOLD) {

						prCandidateBssDesc
							= prPrimaryBssDesc;
						prCandidateStaRec
							= prPrimaryStaRec;
						continue;
					}
				} else if (prPrimaryBssDesc->fgIsConnected) {
					if ((prCandidateBssDesc->ucRCPI <
					     prPrimaryBssDesc->ucRCPI
					    + ROAMING_NO_SWING_RCPI_STEP)
					    || (prCandidateBssDesc
					    ->ucJoinFailureCount
					    > SCN_BSS_JOIN_FAIL_THRESOLD)) {

						prCandidateBssDesc
							= prPrimaryBssDesc;
						prCandidateStaRec
							= prPrimaryStaRec;
						continue;
					}
				} else if (prPrimaryBssDesc
						->ucJoinFailureCount
						> SCN_BSS_JOIN_FAIL_THRESOLD)
					continue;
				else if (prCandidateBssDesc
						->ucJoinFailureCount
					 > SCN_BSS_JOIN_FAIL_THRESOLD ||
					 prCandidateBssDesc->ucRCPI
					 < prPrimaryBssDesc->ucRCPI) {

					prCandidateBssDesc = prPrimaryBssDesc;
					prCandidateStaRec = prPrimaryStaRec;
					continue;
				}
			}
#if 0
			/* If reach here, that means they have the same
			 * Encryption Score, and both RSSI value are close too.
			 */
			/* 4 <6F> Seek the minimum Channel Load for less
			 * interference.
			 */
			if (fgIsFindMinChannelLoad) {
				/* ToDo:: Nothing */
				/* TODO(Kevin): Check which one has minimum
				 * channel load in its channel
				 */
			}
#endif
		}
	}

	return prCandidateBssDesc;

}	/* end of scanSearchBssDescByPolicy() */

void scanReportBss2Cfg80211(IN struct ADAPTER *prAdapter,
			    IN enum ENUM_BSS_TYPE eBSSType,
			    IN struct BSS_DESC *SpecificprBssDesc)
{
	struct SCAN_INFO *prScanInfo = NULL;
	struct LINK *prBSSDescList = NULL;
	struct BSS_DESC *prBssDesc = NULL;
	struct RF_CHANNEL_INFO rChannelInfo;

	ASSERT(prAdapter);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);

	prBSSDescList = &prScanInfo->rBSSDescList;

	log_dbg(SCN, TRACE, "eBSSType: %d\n", eBSSType);

	if (SpecificprBssDesc) {
		{
			/* check BSSID is legal channel */
			if (!scanCheckBssIsLegal(prAdapter,
				SpecificprBssDesc)) {
				log_dbg(SCN, TRACE,
					"Remove specific SSID[%s %d]\n",
					HIDE(SpecificprBssDesc->aucSSID),
					SpecificprBssDesc->ucChannelNum);
				return;
			}

			log_dbg(SCN, TRACE, "Report specific SSID[%s]\n",
				SpecificprBssDesc->aucSSID);

			if (eBSSType == BSS_TYPE_INFRASTRUCTURE) {
				kalIndicateBssInfo(
					prAdapter->prGlueInfo,
					(uint8_t *)
					SpecificprBssDesc->aucRawBuf,
					SpecificprBssDesc->u2RawLength,
					SpecificprBssDesc->ucChannelNum,
					RCPI_TO_dBm(
					SpecificprBssDesc->ucRCPI));
			} else {

				rChannelInfo.ucChannelNum
					= SpecificprBssDesc->ucChannelNum;
				rChannelInfo.eBand = SpecificprBssDesc->eBand;
				kalP2PIndicateBssInfo(prAdapter->prGlueInfo,
					(uint8_t *)
						SpecificprBssDesc->aucRawBuf,
					SpecificprBssDesc->u2RawLength,
					&rChannelInfo,
					RCPI_TO_dBm(SpecificprBssDesc->ucRCPI));

			}

#if CFG_ENABLE_WIFI_DIRECT
			SpecificprBssDesc->fgIsP2PReport = FALSE;
#endif
		}
	} else {
		/* Search BSS Desc from current SCAN result list. */
		LINK_FOR_EACH_ENTRY(prBssDesc, prBSSDescList,
			rLinkEntry, struct BSS_DESC) {
			/* check BSSID is legal channel */
			if (!scanCheckBssIsLegal(prAdapter, prBssDesc)) {
				log_dbg(SCN, TRACE, "Remove SSID[%s %d]\n",
					HIDE(prBssDesc->aucSSID),
					prBssDesc->ucChannelNum);
				continue;
			}

			if ((prBssDesc->eBSSType == eBSSType)
#if CFG_ENABLE_WIFI_DIRECT
			    || ((eBSSType == BSS_TYPE_P2P_DEVICE)
			    && (prBssDesc->fgIsP2PReport == TRUE))
#endif
			    ) {
#define TEMP_LOG_TEMPLATE "Report " MACSTR " SSID[%s %u] eBSSType[%d] " \
		"u2RawLength[%d] fgIsP2PReport[%d]\n"
				log_dbg(SCN, TRACE, TEMP_LOG_TEMPLATE,
						MAC2STR(prBssDesc->aucBSSID),
						HIDE(prBssDesc->aucSSID),
						prBssDesc->ucChannelNum,
						prBssDesc->eBSSType,
						prBssDesc->u2RawLength,
						prBssDesc->fgIsP2PReport);
#undef TEMP_LOG_TEMPLATE

				if (eBSSType == BSS_TYPE_INFRASTRUCTURE) {
					if (prBssDesc->u2RawLength != 0) {
						kalIndicateBssInfo(
							prAdapter->prGlueInfo,
							(uint8_t *)
							prBssDesc->aucRawBuf,
							prBssDesc->u2RawLength,
							prBssDesc->ucChannelNum,
							RCPI_TO_dBm(
							prBssDesc->ucRCPI));
					}
					kalMemZero(prBssDesc->aucRawBuf,
						CFG_RAW_BUFFER_SIZE);
					prBssDesc->u2RawLength = 0;
#if CFG_ENABLE_WIFI_DIRECT
					prBssDesc->fgIsP2PReport = FALSE;
#endif
				} else {
#if CFG_ENABLE_WIFI_DIRECT
					if ((prBssDesc->fgIsP2PReport == TRUE)
					    && prBssDesc->u2RawLength != 0) {
#endif
						rChannelInfo.ucChannelNum
							= prBssDesc
								->ucChannelNum;
						rChannelInfo.eBand
							 = prBssDesc->eBand;

						kalP2PIndicateBssInfo(
							prAdapter->prGlueInfo,
							(uint8_t *)
							prBssDesc->aucRawBuf,
							prBssDesc->u2RawLength,
							&rChannelInfo,
							RCPI_TO_dBm(
							prBssDesc->ucRCPI));

						/* do not clear it then we can
						 * pass the bss in
						 * Specific report
						 */
#if 0 /* TODO: Remove this */
						kalMemZero(prBssDesc->aucRawBuf,
							CFG_RAW_BUFFER_SIZE);
#endif

						/* the BSS entry will not be
						 * cleared after scan done.
						 * So if we dont receive the BSS
						 * in next scan, we cannot pass
						 * it. We use u2RawLength for
						 * the purpose.
						 */
#if 0
						prBssDesc->u2RawLength = 0;
#endif

#if CFG_ENABLE_WIFI_DIRECT
						prBssDesc->fgIsP2PReport
							= FALSE;
					}
#endif
				}
			}

		}
		p2pFunCalAcsChnScores(prAdapter);
	}

}

#if CFG_SUPPORT_PASSPOINT
/*----------------------------------------------------------------------------*/
/*!
 * @brief Find the corresponding BSS Descriptor according to given BSSID
 *
 * @param[in] prAdapter          Pointer to the Adapter structure.
 * @param[in] aucBSSID           Given BSSID.
 * @param[in] fgCheckSsid        Need to check SSID or not. (for multiple SSID
 *                               with single BSSID cases)
 * @param[in] prSsid             Specified SSID
 *
 * @return   Pointer to BSS Descriptor, if found. NULL, if not found
 */
/*----------------------------------------------------------------------------*/
struct BSS_DESC *scanSearchBssDescByBssidAndLatestUpdateTime(
	IN struct ADAPTER *prAdapter, IN uint8_t aucBSSID[])
{
	struct SCAN_INFO *prScanInfo;
	struct LINK *prBSSDescList;
	struct BSS_DESC *prBssDesc;
	struct BSS_DESC *prDstBssDesc = (struct BSS_DESC *) NULL;
	OS_SYSTIME rLatestUpdateTime = 0;

	ASSERT(prAdapter);
	ASSERT(aucBSSID);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);

	prBSSDescList = &prScanInfo->rBSSDescList;

	/* Search BSS Desc from current SCAN result list. */
	LINK_FOR_EACH_ENTRY(
		prBssDesc, prBSSDescList, rLinkEntry, struct BSS_DESC) {

		if (EQUAL_MAC_ADDR(prBssDesc->aucBSSID, aucBSSID)) {
			if (!rLatestUpdateTime
				|| CHECK_FOR_EXPIRATION(prBssDesc->rUpdateTime,
					rLatestUpdateTime)) {
				prDstBssDesc = prBssDesc;
				COPY_SYSTIME(rLatestUpdateTime,
					prBssDesc->rUpdateTime);
			}
		}
	}

	return prDstBssDesc;

}	/* end of scanSearchBssDescByBssid() */

#endif /* CFG_SUPPORT_PASSPOINT */

#if CFG_SUPPORT_AGPS_ASSIST
void scanReportScanResultToAgps(struct ADAPTER *prAdapter)
{
	struct LINK *prBSSDescList =
			&prAdapter->rWifiVar.rScanInfo.rBSSDescList;
	struct BSS_DESC *prBssDesc = NULL;
	struct AGPS_AP_LIST *prAgpsApList =
			kalMemAlloc(sizeof(struct AGPS_AP_LIST), VIR_MEM_TYPE);
	struct AGPS_AP_INFO *prAgpsInfo = &prAgpsApList->arApInfo[0];
	struct SCAN_INFO *prScanInfo = &prAdapter->rWifiVar.rScanInfo;
	uint8_t ucIndex = 0;

	LINK_FOR_EACH_ENTRY(
		prBssDesc, prBSSDescList, rLinkEntry, struct BSS_DESC) {

		if (prBssDesc->rUpdateTime < prScanInfo->rLastScanCompletedTime)
			continue;
		COPY_MAC_ADDR(prAgpsInfo->aucBSSID, prBssDesc->aucBSSID);
		prAgpsInfo->ePhyType = AGPS_PHY_G;
		prAgpsInfo->u2Channel = prBssDesc->ucChannelNum;
		prAgpsInfo->i2ApRssi = RCPI_TO_dBm(prBssDesc->ucRCPI);
		prAgpsInfo++;
		ucIndex++;
		if (ucIndex == SCN_AGPS_AP_LIST_MAX_NUM)
			break;
	}
	prAgpsApList->ucNum = ucIndex;
	GET_CURRENT_SYSTIME(&prScanInfo->rLastScanCompletedTime);
	/* log_dbg(SCN, INFO, ("num of scan list:%d\n", ucIndex)); */
	kalIndicateAgpsNotify(prAdapter, AGPS_EVENT_WLAN_AP_LIST,
		(uint8_t *) prAgpsApList, sizeof(struct AGPS_AP_LIST));
	kalMemFree(prAgpsApList, VIR_MEM_TYPE, sizeof(struct AGPS_AP_LIST));
}
#endif /* CFG_SUPPORT_AGPS_ASSIST */

void scanReqLog(struct CMD_SCAN_REQ_V2 *prCmdScanReq)
{
	struct CMD_SCAN_REQ_V2 *req = prCmdScanReq;
	char *strbuf = NULL, *pos = NULL, *end = NULL;
	uint32_t slen = 0;
	int i, j;
	int snum[2] = {req->ucSSIDNum, req->ucSSIDExtNum};
	struct PARAM_SSID *slist[2] = {req->arSSID, req->arSSIDExtend};
	int cnum[2] = {req->ucChannelListNum, req->ucChannelListExtNum};
	struct CHANNEL_INFO *clist[2] =	{
		req->arChannelList, req->arChannelListExtend};

	/* ssid and space */
	for (i = 0; i < 2; ++i)
		for (j = 0; j < snum[i]; ++j)
			slen += slist[i][j].u4SsidLen + 1;

	/* The length should be added 6 + 10 + 4 + 8 + 1 for the format
	 * ",Ssid:", * ",Ext ssid:", ",Ch:", ",Ext Ch:" and null byte.
	 */
	slen += 29 + 4 * (req->ucChannelListNum + req->ucChannelListExtNum);
	pos = strbuf = kalMemAlloc(slen, VIR_MEM_TYPE);
	if (strbuf == NULL) {
		scanlog_dbg(LOG_SCAN_REQ_K2D, INFO, "Can't allocate memory\n");
		return;
	}
	end = strbuf + slen;

	for (i = 0; i < 2; ++i) {
		if (snum[i] > 0) {
			pos += kalSnprintf(pos, end - pos, "%s",
				i == 0 ? ",Ssid:" : ",Ext Ssid:");
			for (j = 0; j < snum[i]; ++j) {
				char ssid[PARAM_MAX_LEN_SSID + 1] = {0};

				kalStrnCpy(ssid,
					slist[i][j].aucSsid, sizeof(ssid));
				ssid[sizeof(ssid) - 1] = '\0';
				pos += kalSnprintf(pos, end - pos, " %s", ssid);
			}
		}
	}

	for (i = 0; i < 2; ++i) {
		if (cnum[i] > 0) {
			pos += kalSnprintf(pos, end - pos, "%s",
				i == 0 ? ",Ch:" : ",Ext Ch:");
			for (j = 0; j < cnum[i]; ++j)
				pos += kalSnprintf(pos, end - pos, " %u",
					clist[i][j].ucChannelNum % 1000);
		}
	}
#define TEMP_LOG_TEMPLATE \
	"ScanReqV2: ScanType=%d,BSS=%u,SSIDType=%d,Num=%u,Ext=%u," \
	"ChannelType=%d,Num=%d,Ext=%u,Seq=%u,Ver=%u,Dw=%u,Min=%u," \
	"Func=0x%X,Mac="MACSTR",BSSID="MACSTR",%s\n"
	scanlog_dbg(LOG_SCAN_REQ_D2F, INFO, TEMP_LOG_TEMPLATE,
		prCmdScanReq->ucScanType,
		prCmdScanReq->ucBssIndex,
		prCmdScanReq->ucSSIDType,
		prCmdScanReq->ucSSIDNum,
		prCmdScanReq->ucSSIDExtNum,
		prCmdScanReq->ucChannelType,
		prCmdScanReq->ucChannelListNum,
		prCmdScanReq->ucChannelListExtNum,
		prCmdScanReq->ucSeqNum, prCmdScanReq->auVersion[0],
		prCmdScanReq->u2ChannelDwellTime,
		prCmdScanReq->u2ChannelMinDwellTime,
		prCmdScanReq->ucScnFuncMask,
		MAC2STR(prCmdScanReq->aucRandomMac),
		MAC2STR(prCmdScanReq->aucBSSID),
		strbuf != pos ? strbuf : "");
#undef TEMP_LOG_TEMPLATE
	kalMemFree(strbuf, VIR_MEM_TYPE, slen);
}

void scanResultLog(struct ADAPTER *prAdapter,
	struct SW_RFB *prSwRfb)
{
	struct WLAN_BEACON_FRAME *pFrame =
		(struct WLAN_BEACON_FRAME *) prSwRfb->pvHeader;
	KAL_SPIN_LOCK_DECLARATION();

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_BSSLIST_FW);
	scanLogCacheAddBSS(
		&(prAdapter->rWifiVar.rScanInfo.rScanLogCache.rBSSListFW),
		prAdapter->rWifiVar.rScanInfo.rScanLogCache.arBSSListBufFW,
		LOG_SCAN_RESULT_F2D,
		pFrame->aucBSSID,
		pFrame->u2SeqCtrl);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_BSSLIST_FW);
}

void scanLogCacheAddBSS(struct LINK *prList,
	struct SCAN_LOG_ELEM_BSS *prListBuf,
	enum ENUM_SCAN_LOG_PREFIX prefix,
	uint8_t bssId[], uint16_t seq)
{
	struct SCAN_LOG_ELEM_BSS *pSavedBss = NULL;
	struct SCAN_LOG_ELEM_BSS *pBss = NULL;

	if (LINK_IS_INVALID(prList)) {
		LINK_INITIALIZE(prList);
	}

	LINK_FOR_EACH_ENTRY(pSavedBss, prList,
		rLinkEntry, struct SCAN_LOG_ELEM_BSS) {
		if (pSavedBss && bssId) {
			if (EQUAL_MAC_ADDR(pSavedBss->aucBSSID, bssId))
				return;
		} else {
			scanlog_dbg(prefix, ERROR,
				"pSavedBss(0x%x) or bssid(0x%x) is NULL\n",
				pSavedBss, bssId);
			return;
		}
	}

	if (prList->u4NumElem < SCAN_LOG_BUFF_SIZE) {
		if (prListBuf != NULL) {
			pBss = &(prListBuf[prList->u4NumElem]);
		} else {
			scanlog_dbg(prefix, INFO, "Buffer is NULL\n");
			return;
		}
	} else {
		scanlog_dbg(prefix, INFO, "Need more buffer\n");
		return;
	}
	kalMemZero(pBss, sizeof(struct SCAN_LOG_ELEM_BSS));

	COPY_MAC_ADDR(pBss->aucBSSID, bssId);
	pBss->u2SeqCtrl = seq;

	LINK_INSERT_TAIL(prList, &(pBss->rLinkEntry));
}

void scanLogCacheFlushBSS(struct LINK *prList, enum ENUM_SCAN_LOG_PREFIX prefix)
{
	char arlogBuf[SCAN_LOG_MSG_MAX_LEN];
	uint32_t idx = 0;
	struct SCAN_LOG_ELEM_BSS *pBss = NULL;
#if CFG_SHOW_FULL_MACADDR
	/* XXXXXXXXXXXX */
	const uint8_t dataLen = 12;
#else
	/* XXXXsumXX */
	const uint8_t dataLen = 9;
#endif

	if (LINK_IS_INVALID(prList)) {
		LINK_INITIALIZE(prList);
	}

	if (LINK_IS_EMPTY(prList))
		return;

	kalMemZero(arlogBuf, SCAN_LOG_MSG_MAX_LEN);
	/* The maximum characters of uint32_t could be 10. Thus, the
	 * mininum size should be 10+3 for the format "%u: ".
	 */
	if (dataLen + 1 > SCAN_LOG_MSG_MAX_LEN) {
		scanlog_dbg(prefix, INFO, "Scan log buffer is too small.\n");
		while (!LINK_IS_EMPTY(prList)) {
			LINK_REMOVE_HEAD(prList,
				pBss, struct SCAN_LOG_ELEM_BSS *);
		}
		return;
	}
	idx += kalSnprintf(arlogBuf, SCAN_LOG_MSG_MAX_LEN, "%u: ",
			prList->u4NumElem);

	while (!LINK_IS_EMPTY(prList)) {
		if (idx+dataLen+1 > SCAN_LOG_MSG_MAX_LEN) {
			arlogBuf[idx] = 0; /* terminating null byte */
			if (prefix != LOG_SCAN_D2D)
				scanlog_dbg(prefix, INFO, "%s\n", arlogBuf);
			idx = 0;
		}

		LINK_REMOVE_HEAD(prList,
			pBss, struct SCAN_LOG_ELEM_BSS *);

#if CFG_SHOW_FULL_MACADDR
		idx += kalSnprintf(arlogBuf+idx, dataLen+1,
			"%02x%02x%02x%02x%02x%02x",
			((uint8_t *)pBss->aucBSSID)[0],
			((uint8_t *)pBss->aucBSSID)[1],
			((uint8_t *)pBss->aucBSSID)[2],
			((uint8_t *)pBss->aucBSSID)[3],
			((uint8_t *)pBss->aucBSSID)[4],
			((uint8_t *)pBss->aucBSSID)[5]);
#else
		idx += kalSnprintf(arlogBuf+idx, dataLen+1,
			"%02x%02x%03x%02x",
			((uint8_t *)pBss->aucBSSID)[0],
			((uint8_t *)pBss->aucBSSID)[1],
			((uint8_t *)pBss->aucBSSID)[2] +
			((uint8_t *)pBss->aucBSSID)[3] +
			((uint8_t *)pBss->aucBSSID)[4],
			((uint8_t *)pBss->aucBSSID)[5]);
#endif

	}
	if (idx != 0) {
		arlogBuf[idx] = 0; /* terminating null byte */
		if (prefix != LOG_SCAN_D2D)
			scanlog_dbg(prefix, INFO, "%s\n", arlogBuf);
		idx = 0;
	}
}

void scanLogCacheFlushAll(struct ADAPTER *prAdapter,
	struct SCAN_LOG_CACHE *prScanLogCache,
	enum ENUM_SCAN_LOG_PREFIX prefix)
{
	KAL_SPIN_LOCK_DECLARATION();

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_BSSLIST_FW);
	scanLogCacheFlushBSS(&(prScanLogCache->rBSSListFW),
		prefix);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_BSSLIST_FW);

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_BSSLIST_CFG);
	scanLogCacheFlushBSS(&(prScanLogCache->rBSSListCFG),
		prefix);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_BSSLIST_CFG);
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief Remove and clean BSS Descriptors from the list.
 *
 * @param[in] prBSSDescList Pointer to the LINK structure.
 * @param[in] prBssDesc     Pointer to the BSS_DESC structure.
 * @param[in] prAdapter     Pointer to the Adapter structure.
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void scanRemoveBssDescFromList(IN struct LINK *prBSSDescList,
			       IN struct BSS_DESC *prBssDesc,
			       IN struct ADAPTER *prAdapter)
{
	if (prAdapter != NULL && prBssDesc != NULL) {
		uint8_t j;

		/* Support AP Selection */
		if (!prBssDesc->prBlack)
			aisQueryBlackList(prAdapter, prBssDesc);

		/* Remove this BSS Desc from the Ess Desc List */
		for (j = 0; j < KAL_AIS_NUM; j++) {
			struct AIS_SPECIFIC_BSS_INFO *prSpecBssInfo =
				aisGetAisSpecBssInfo(
				prAdapter, j);
			struct LINK *prEssList;

			if (!prSpecBssInfo)
				continue;
			prEssList =
				&prSpecBssInfo->rCurEssLink;
			if (!prEssList)
				continue;

			if (!LINK_ENTRY_IS_VALID(&prBssDesc->rLinkEntryEss[j]))
				continue;

			LINK_REMOVE_KNOWN_ENTRY(prEssList,
				&prBssDesc->rLinkEntryEss[j]);
		}
		/* end Support AP Selection */

		/* Remove this BSS Desc from the BSS Desc list */
		if (prBSSDescList != NULL)
			LINK_REMOVE_KNOWN_ENTRY(prBSSDescList, prBssDesc);
	}
}	/* end of scanRemoveBssDescFromList() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief Insert and initialize the BSS Descriptors to the list.
 *
 * @param[in] prBSSDescList Pointer to the LINK structure.
 * @param[in] prBssDesc     Pointer to the BSS_DESC structure.
 * @param[in] init          TRUE for initializing the BSS_DESC.
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void scanInsertBssDescToList(IN struct LINK *prBSSDescList,
			     IN struct BSS_DESC *prBssDesc,
			     IN u_int8_t init)
{
	if (prBssDesc != NULL) {
		if (init == TRUE) {
			/* This will reset the link relationship */
			kalMemZero(prBssDesc, sizeof(struct BSS_DESC));

#if CFG_ENABLE_WIFI_DIRECT
			LINK_INITIALIZE(&(prBssDesc->rP2pDeviceList));
			prBssDesc->fgIsP2PPresent = FALSE;
#endif /* CFG_ENABLE_WIFI_DIRECT */
		}

		/* Insert this BSS Desc to the BSS Desc list */
		if (prBSSDescList != NULL)
			LINK_INSERT_TAIL(prBSSDescList, &prBssDesc->rLinkEntry);
	}
}	/* end of scanInsertBssDescToList() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief Reset the BSS Descriptors.
 *
 * @param[in] prAdapter  Pointer to the Adapter structure.
 * @param[in] prBssDesc  Pointer to the BSS_DESC structure.
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void scanResetBssDesc(IN struct ADAPTER *prAdapter,
		      IN struct BSS_DESC *prBssDesc)
{
	struct LINK *prBSSDescList =
		&prAdapter->rWifiVar.rScanInfo.rBSSDescList;

	scanRemoveBssDescFromList(prBSSDescList,
		prBssDesc,
		prAdapter);
	scanInsertBssDescToList(prBSSDescList,
		prBssDesc,
		TRUE);
}	/* end of scanResetBssDesc() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief Check if VHT IE exists in Vendor Epigram IE.
 *
 * @param[in] pucBuf     Pointer to the Vendor IE.
 * @param[in] prBssDesc  Pointer to the BSS_DESC structure.
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void scanCheckEpigramVhtIE(IN uint8_t *pucBuf, IN struct BSS_DESC *prBssDesc)
{
	uint32_t u4EpigramOui;
	uint16_t u2EpigramVendorType;
	struct IE_VENDOR_EPIGRAM_IE *prEpiIE;
	uint8_t *pucIE;
	uint16_t u2IELength;
	uint16_t u2Offset = 0;

	if (pucBuf == NULL) {
		DBGLOG(RLM, WARN, "[Epigram] pucBuf is NULL, skip!\n");
		return;
	}
	if (prBssDesc == NULL) {
		DBGLOG(RLM, WARN, "[Epigram] prBssDesc is NULL, skip!\n");
		return;
	}

	prEpiIE = (struct IE_VENDOR_EPIGRAM_IE *) pucBuf;
	u2IELength = prEpiIE->ucLength;
	WLAN_GET_FIELD_BE24(prEpiIE->aucOui, &u4EpigramOui);
	WLAN_GET_FIELD_BE16(prEpiIE->aucVendorType, &u2EpigramVendorType);
	if (u4EpigramOui != VENDOR_IE_EPIGRAM_OUI)
		return;
	if (u2EpigramVendorType != VENDOR_IE_EPIGRAM_VHTTYPE1 &&
	    u2EpigramVendorType != VENDOR_IE_EPIGRAM_VHTTYPE2 &&
	    u2EpigramVendorType != VENDOR_IE_EPIGRAM_VHTTYPE3)
		return;

	pucIE = prEpiIE->pucData;
	IE_FOR_EACH(pucIE, u2IELength, u2Offset) {
		switch (IE_ID(pucIE)) {
		case ELEM_ID_VHT_CAP:
			scanParseVHTCapIE(pucIE, prBssDesc);
			break;
		case ELEM_ID_VHT_OP:
			scanParseVHTOpIE(pucIE, prBssDesc);
			break;
		default:
			break;
		}
	}
}

void scanParseVHTCapIE(IN uint8_t *pucIE, IN struct BSS_DESC *prBssDesc)
{
	struct IE_VHT_CAP *prVhtCap = NULL;
	uint16_t u2TxMcsSet = 0;
	uint8_t ucSpatial = 0;
	uint8_t j = 0;

	prVhtCap = (struct IE_VHT_CAP *) pucIE;
	/* Error handling */
	if (IE_LEN(prVhtCap) != (sizeof(struct IE_VHT_CAP) - 2)) {
		DBGLOG(SCN, WARN,
			"VhtCap wrong length!(%d)->(%d)\n",
			(sizeof(struct IE_VHT_CAP) - 2),
			IE_LEN(prVhtCap));
		return;
	}

	u2TxMcsSet = prVhtCap->rVhtSupportedMcsSet.u2TxMcsMap;
	prBssDesc->fgIsVHTPresent = TRUE;
#if CFG_SUPPORT_BFEE
#define __LOCAL_VAR__ \
VHT_CAP_INFO_NUMBER_OF_SOUNDING_DIMENSIONS_OFFSET

	prBssDesc->ucVhtCapNumSoundingDimensions =
		(prVhtCap->u4VhtCapInfo
		& VHT_CAP_INFO_NUMBER_OF_SOUNDING_DIMENSIONS)
		>> __LOCAL_VAR__;
#undef __LOCAL_VAR__
#endif
	/* Support AP Selection*/
	if (prBssDesc->fgMultiAnttenaAndSTBC)
		return;

	for (; j < 8; j++) {
		if ((u2TxMcsSet & BITS(2 * j, 2 * j + 1)) != 3)
			ucSpatial++;
	}
	prBssDesc->fgMultiAnttenaAndSTBC =
		((ucSpatial > 1) && (prVhtCap->u4VhtCapInfo &
			VHT_CAP_INFO_TX_STBC));
}

void scanParseVHTOpIE(IN uint8_t *pucIE, IN struct BSS_DESC *prBssDesc)
{
	struct IE_VHT_OP *prVhtOp = NULL;

	prVhtOp = (struct IE_VHT_OP *) pucIE;
	if (IE_LEN(prVhtOp) != (sizeof(struct IE_VHT_OP) - 2))
		return;
	prBssDesc->eChannelWidth = (enum ENUM_CHANNEL_WIDTH)
		(prVhtOp->ucVhtOperation[0]);
	prBssDesc->ucCenterFreqS1 = (enum ENUM_CHANNEL_WIDTH)
		(prVhtOp->ucVhtOperation[1]);
	prBssDesc->ucCenterFreqS2 = (enum ENUM_CHANNEL_WIDTH)
		(prVhtOp->ucVhtOperation[2]);

	/*add IEEE BW160 patch*/
	rlmModifyVhtBwPara(&prBssDesc->ucCenterFreqS1,
			   &prBssDesc->ucCenterFreqS2,
			   (uint8_t *)&prBssDesc->eChannelWidth);
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief Check if Adaptive 11r  IE exists in Vendor Cisco IE.
 *
 * @param[in] pucBuf     Pointer to the Vendor IE.
 * @param[in] prBssDesc  Pointer to the BSS_DESC structure.
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void scanCheckAdaptive11rIE(IN uint8_t *pucBuf, IN struct BSS_DESC *prBssDesc)
{
	uint32_t oui;
	struct IE_VENDOR_ADAPTIVE_11R_IE *ie;
	uint8_t data;
	uint16_t len;

	if (pucBuf == NULL || prBssDesc == NULL) {
		DBGLOG(SCN, WARN, "adp11r pucBuf %p, prBssDesc %p, skip!\n",
			pucBuf, prBssDesc);
		return;
	}

	ie = (struct IE_VENDOR_ADAPTIVE_11R_IE *) pucBuf;
	len = ie->ucLength;
	if (len < 5 || len > 8)
		return;

	WLAN_GET_FIELD_BE24(ie->aucOui, &oui);
	if (oui != VENDOR_IE_CISCO_OUI ||
	    *(ie->aucVendorType) != VENDOR_IE_CISCO_TYPE)
		return;

	data = *(ie->pucData);
	prBssDesc->ucIsAdaptive11r = data & BIT(0);
	DBGLOG(SCN, TRACE, "BSSDesc [" MACSTR "] adaptive11r = %d\n",
		MAC2STR(prBssDesc->aucBSSID), prBssDesc->ucIsAdaptive11r);
}

