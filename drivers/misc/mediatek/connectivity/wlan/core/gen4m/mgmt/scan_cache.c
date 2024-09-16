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

#if CFG_SUPPORT_SCAN_CACHE_RESULT
/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

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
static OS_SYSTIME getCurrentScanTime(void);

static OS_SYSTIME getLastScanTime(struct GL_SCAN_CACHE_INFO *prScanCache);

static void setLastScanTime(struct GL_SCAN_CACHE_INFO *prScanCache,
	OS_SYSTIME rCurrentTime);

static uint32_t getNumberOfScanChannels(struct GL_SCAN_CACHE_INFO *prScanCache);

static u_int8_t isMediaConnected(struct GL_SCAN_CACHE_INFO *prScanCache);

static u_int8_t isScanCacheChannels(struct GL_SCAN_CACHE_INFO *prScanCache);

static u_int8_t isScanCacheTimeReady(struct GL_SCAN_CACHE_INFO *prScanCache);
static u_int8_t isScanCacheLowSpanScan(struct GL_SCAN_CACHE_INFO *prScanCache);
static u_int8_t isFull2PartialTimeout(struct GL_SCAN_CACHE_INFO *prScanCache);
static u_int8_t isScanCacheTimeOverflow(struct GL_SCAN_CACHE_INFO *prScanCache,
	OS_SYSTIME rCurrentTime);

static u_int8_t doScanCache(struct GL_SCAN_CACHE_INFO *prScanCache);

static void updateScanCacheLastScanTime(struct GL_SCAN_CACHE_INFO *prScanCache,
	OS_SYSTIME rCurrentTime);

static void resetScanCacheLastScanTime(struct GL_SCAN_CACHE_INFO *prScanCache);

static u_int8_t inScanCachePeriod(struct GL_SCAN_CACHE_INFO *prScanCache,
	OS_SYSTIME rCurrentTime);

static u_int8_t matchScanCache(struct GL_SCAN_CACHE_INFO *prScanCache,
	OS_SYSTIME rCurrentTime);

static u_int8_t matchLastScanTimeUpdate(struct GL_SCAN_CACHE_INFO *prScanCache,
	OS_SYSTIME rCurrentTime);

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
static OS_SYSTIME getCurrentScanTime(void)
{
	OS_SYSTIME rCurrentTime = 0;

	GET_CURRENT_SYSTIME(&rCurrentTime);
	return rCurrentTime;
}

static OS_SYSTIME getLastScanTime(struct GL_SCAN_CACHE_INFO *prScanCache)
{
	return prScanCache->u4LastScanTime;
}

static void setLastScanTime(struct GL_SCAN_CACHE_INFO *prScanCache,
	OS_SYSTIME rCurrentTime)
{
	prScanCache->u4LastScanTime = rCurrentTime;
}

static uint32_t getNumberOfScanChannels(struct GL_SCAN_CACHE_INFO *prScanCache)
{
	return (uint32_t) prScanCache->n_channels;
}

static u_int8_t isMediaConnected(struct GL_SCAN_CACHE_INFO *prScanCache)
{
	return MEDIA_STATE_CONNECTED ==
		kalGetMediaStateIndicated(prScanCache->prGlueInfo,
		prScanCache->ucBssIndex);
}

static u_int8_t isScanCacheChannels(struct GL_SCAN_CACHE_INFO *prScanCache)
{
	return getNumberOfScanChannels(prScanCache) >=
		CFG_SCAN_CACHE_MIN_CHANNEL_NUM;
}

static u_int8_t isScanCacheTimeReady(struct GL_SCAN_CACHE_INFO *prScanCache)
{
	return prScanCache->u4LastScanTime != 0;
}

static u_int8_t isScanCacheLowSpanScan(struct GL_SCAN_CACHE_INFO *prScanCache)
{
	return (prScanCache->u4Flags & NL80211_SCAN_FLAG_LOW_SPAN) >> 8;
}

/*
 * @brief This routine is to check the interval of full scan
 *
 * @param prScanCache - pointer of struct GL_SCAN_CACHE_INFO
 *
 * @retval TRUE: time diff between now and last full scan >=
 *		 CFG_SCAN_FULL2PARTIAL_PERIOD
 *         FALSE: time diff between now and last full scan <
 *		 CFG_SCAN_FULL2PARTIAL_PERIOD
 */
static u_int8_t isFull2PartialTimeout(struct GL_SCAN_CACHE_INFO *prScanCache)
{
	struct ADAPTER *prAdapter = NULL;
	struct SCAN_INFO *prScanInfo;
	u_int8_t fgLastFullScanTimeout = FALSE;
	OS_SYSTIME rCurrentTime;

	GET_CURRENT_SYSTIME(&rCurrentTime);

	prAdapter = prScanCache->prGlueInfo->prAdapter;
	if (prAdapter == NULL) {
		DBGLOG(REQ, ERROR, "prScanCache->prGlueInfo->prAdapter NULL");
		return FALSE;
	}

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	if (prScanInfo == NULL) {
		DBGLOG(REQ, ERROR, "prAdapter->rWifiVar.rScanInfo NULL");
		return FALSE;
	}

#if CFG_SUPPORT_FULL2PARTIAL_SCAN
	if (CHECK_FOR_TIMEOUT(rCurrentTime, prScanInfo->u4LastFullScanTime,
		SEC_TO_SYSTIME(CFG_SCAN_FULL2PARTIAL_PERIOD)))
		fgLastFullScanTimeout = TRUE;
#endif

	return fgLastFullScanTimeout;
}

static u_int8_t isScanCacheTimeOverflow(struct GL_SCAN_CACHE_INFO *prScanCache,
	OS_SYSTIME rCurrentTime)
{
	return getLastScanTime(prScanCache) > rCurrentTime;
}

static u_int8_t doScanCache(struct GL_SCAN_CACHE_INFO *prScanCache)
{
	GLUE_SPIN_LOCK_DECLARATION();

	kalUpdateBssTimestamp(prScanCache->prGlueInfo);
	GLUE_ACQUIRE_SPIN_LOCK(prScanCache->prGlueInfo, SPIN_LOCK_NET_DEV);
	kalCfg80211ScanDone(prScanCache->prRequest, FALSE);
	GLUE_RELEASE_SPIN_LOCK(prScanCache->prGlueInfo, SPIN_LOCK_NET_DEV);
	return TRUE;
}

static void updateScanCacheLastScanTime(struct GL_SCAN_CACHE_INFO *prScanCache,
	OS_SYSTIME rCurrentTime)
{
	setLastScanTime(prScanCache, rCurrentTime);
}

static void resetScanCacheLastScanTime(struct GL_SCAN_CACHE_INFO *prScanCache)
{
	setLastScanTime(prScanCache, 0);
}

static u_int8_t inScanCachePeriod(struct GL_SCAN_CACHE_INFO *prScanCache,
	OS_SYSTIME rCurrentTime)
{
	if (isScanCacheTimeReady(prScanCache) == FALSE)
		return FALSE;

	if (isScanCacheTimeOverflow(prScanCache, rCurrentTime) == TRUE)
		return FALSE;

	if (CHECK_FOR_TIMEOUT(rCurrentTime, getLastScanTime(prScanCache),
		CFG_SCAN_CACHE_RESULT_PERIOD) == FALSE)
		return TRUE;

	return FALSE;
}

static u_int8_t matchScanCache(struct GL_SCAN_CACHE_INFO *prScanCache,
	OS_SYSTIME rCurrentTime)
{

	if (isMediaConnected(prScanCache) == TRUE) {
		/* If scan not triggered by APP and it has been >
		 * CFG_SCAN_FULL2PARTIAL_PERIOD for last full scan,
		 * not to do scan cache
		*/
		if (!isScanCacheLowSpanScan(prScanCache) &&
			isFull2PartialTimeout(prScanCache))
			return FALSE;
		else if (inScanCachePeriod(prScanCache, rCurrentTime) == TRUE &&
				isScanCacheChannels(prScanCache) == TRUE)
			return TRUE;
	}
	return FALSE;
}

static u_int8_t matchLastScanTimeUpdate(struct GL_SCAN_CACHE_INFO *prScanCache,
	OS_SYSTIME rCurrentTime)
{
	if (isMediaConnected(prScanCache) == TRUE &&
		inScanCachePeriod(prScanCache, rCurrentTime) == FALSE &&
		isScanCacheChannels(prScanCache) == TRUE)
		return TRUE;

	return FALSE;
}

/*
 * @brief This routine is responsible for checking scan cache
 *
 * @param prScanCache - pointer of struct GL_SCAN_CACHE_INFO
 *
 * @retval TRUE: do scan cache successful
 *         FALSE: didn't report scan cache
 */
u_int8_t isScanCacheDone(struct GL_SCAN_CACHE_INFO *prScanCache)
{
	OS_SYSTIME rCurrentTime = getCurrentScanTime();

	if (matchScanCache(prScanCache, rCurrentTime) == TRUE) {
		log_limited_dbg(REQ, INFO, "SCAN_CACHE: Skip scan too frequently(%u, %u). Call cfg80211_scan_done directly\n",
			rCurrentTime,
			getLastScanTime(prScanCache));

		return doScanCache(prScanCache);
	}

	if (matchLastScanTimeUpdate(prScanCache, rCurrentTime) == TRUE) {
		log_dbg(REQ, INFO, "SCAN_CACHE: set scan cache time (%u)->(%u)\n",
			getLastScanTime(prScanCache),
			rCurrentTime);

		updateScanCacheLastScanTime(prScanCache, rCurrentTime);
	}

	if (isMediaConnected(prScanCache) == FALSE) {
		log_dbg(REQ, TRACE, "SCAN_CACHE: reset scan cache time (%u)->(0)\n",
			getLastScanTime(prScanCache));

		resetScanCacheLastScanTime(prScanCache);
	}
	return FALSE;
}
#endif /* CFG_SUPPORT_SCAN_CACHE_RESULT */
