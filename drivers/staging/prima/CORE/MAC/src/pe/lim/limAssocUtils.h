/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file limAssocUtils.h contains the utility definitions
 * LIM uses while processing Re/Association messages.
 * Author:        Chandra Modumudi
 * Date:          02/13/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 * 05/26/10       js             WPA handling in (Re)Assoc frames
 *
 */
#ifndef __LIM_ASSOC_UTILS_H
#define __LIM_ASSOC_UTILS_H

#include "sirApi.h"
#include "sirDebug.h"
#include "cfgApi.h"

#include "limTypes.h"


tANI_U8         limCmpSSid(tpAniSirGlobal, tSirMacSSid *,tpPESession);
tANI_U8         limCompareCapabilities(tpAniSirGlobal,
                                       tSirAssocReq *,
                                       tSirMacCapabilityInfo *,tpPESession);
tANI_U8         limCheckRxBasicRates(tpAniSirGlobal, tSirMacRateSet,tpPESession);
tANI_U8         limCheckRxRSNIeMatch(tpAniSirGlobal, tDot11fIERSN, tpPESession, tANI_U8, tANI_BOOLEAN *);
tANI_U8         limCheckRxWPAIeMatch(tpAniSirGlobal, tDot11fIEWPA, tpPESession, tANI_U8);
tANI_U8         limCheckMCSSet(tpAniSirGlobal pMac, tANI_U8* supportedMCSSet);
void            limPostDummyToTmRing(tpAniSirGlobal, tpDphHashNode);
void            limPostPacketToTdRing(tpAniSirGlobal,
                                      tpDphHashNode,
                                      tANI_U8);
tSirRetStatus   limCleanupRxPath(tpAniSirGlobal, tpDphHashNode,tpPESession);
void            limRejectAssociation(tpAniSirGlobal , tSirMacAddr, tANI_U8,
                                     tANI_U8 , tAniAuthType,
                                     tANI_U16, tANI_U8, tSirResultCodes, tpPESession);

#ifdef WLAN_FEATURE_11AC
tSirRetStatus limPopulatePeerRateSet(tpAniSirGlobal pMac,
                                         tpSirSupportedRates pRates,
                                         tANI_U8* pSupportedMCSSet,
                                         tANI_U8 basicOnly,
                                         tpPESession psessionEntry,
                                         tDot11fIEVHTCaps *pVHTCaps);
#else
tSirRetStatus limPopulatePeerRateSet(tpAniSirGlobal pMac,
                                                                tpSirSupportedRates pRates,
                                                                tANI_U8* pSupportedMCSSet,
                                                                tANI_U8 basicOnly,
                                                                tpPESession psessionEntry);
#endif
#ifdef WLAN_FEATURE_11AC
tSirRetStatus limPopulateOwnRateSet(tpAniSirGlobal pMac,
                                         tpSirSupportedRates pRates,
                                         tANI_U8* pSupportedMCSSet,
                                         tANI_U8 basicOnly,
                                         tpPESession psessionEntry,
                                         tDot11fIEVHTCaps *pVHTCaps);

#else
tSirRetStatus limPopulateOwnRateSet(tpAniSirGlobal pMac,
                                                                tpSirSupportedRates pRates,
                                                                tANI_U8* pSupportedMCSSet,
                                                                tANI_U8 basicOnly,
                                                                tpPESession psessionEntry);
#endif

#ifdef WLAN_FEATURE_11AC
tSirRetStatus
limPopulateMatchingRateSet(tpAniSirGlobal pMac,
                           tpDphHashNode pStaDs,
                           tSirMacRateSet *pOperRateSet,
                           tSirMacRateSet *pExtRateSet,
                           tANI_U8* pSupportedMCSSet,
                           tSirMacPropRateSet *pAniLegRateSet,
                           tpPESession  psessionEntry,
                           tDot11fIEVHTCaps *pVHTCaps);
#else
tSirRetStatus   limPopulateMatchingRateSet(tpAniSirGlobal,
                                           tpDphHashNode,
                                           tSirMacRateSet *,
                                           tSirMacRateSet *,
                                           tANI_U8* pSupportedMCSSet,
                                           tSirMacPropRateSet *, tpPESession);


#endif
tSirRetStatus   limAddSta(tpAniSirGlobal, tpDphHashNode, tANI_U8, tpPESession);
tSirRetStatus   limDelBss(tpAniSirGlobal, tpDphHashNode, tANI_U16, tpPESession);
tSirRetStatus   limDelSta(tpAniSirGlobal, tpDphHashNode, tANI_BOOLEAN, tpPESession);
#ifdef WLAN_FEATURE_VOWIFI_11R
tSirRetStatus   limAddFTStaSelf(tpAniSirGlobal pMac, tANI_U16 assocId,
                    tpPESession psessionEntry);
#endif /* WLAN_FEATURE_VOWIFI_11R */
tSirRetStatus   limAddStaSelf(tpAniSirGlobal, tANI_U16, tANI_U8, tpPESession);
tStaRateMode limGetStaRateMode(tANI_U8 dot11Mode);


void            limTeardownInfraBss(tpAniSirGlobal,tpPESession);
void            limRestorePreReassocState(tpAniSirGlobal,
                                          tSirResultCodes,
                                          tANI_U16,tpPESession); 
void            limPostReassocFailure(tpAniSirGlobal,
                                      tSirResultCodes,
                                      tANI_U16,tpPESession);
eAniBoolean     limIsReassocInProgress(tpAniSirGlobal,tpPESession);
void
limSendDelStaCnf(tpAniSirGlobal pMac, tSirMacAddr staDsAddr,
       tANI_U16 staDsAssocId, tLimMlmStaContext mlmStaContext, tSirResultCodes statusCode,tpPESession psessionEntry);

void            limHandleCnfWaitTimeout(tpAniSirGlobal pMac, tANI_U16 staId);
void            limDeleteDphHashEntry(tpAniSirGlobal, tSirMacAddr, tANI_U16,tpPESession);
void            limCheckAndAnnounceJoinSuccess(tpAniSirGlobal,
                                               tSirProbeRespBeacon *,
                                               tpSirMacMgmtHdr,tpPESession);
void limUpdateReAssocGlobals(tpAniSirGlobal pMac,
                                    tpSirAssocRsp pAssocRsp,tpPESession psessionEntry);

void limUpdateAssocStaDatas(tpAniSirGlobal pMac, 
                                tpDphHashNode pStaDs,tpSirAssocRsp pAssocRsp,tpPESession psessionEntry);
void
limFillSupportedRatesInfo(
    tpAniSirGlobal          pMac,
    tpDphHashNode           pSta,
    tpSirSupportedRates   pRates,
    tpPESession           psessionEntry);

tSirRetStatus limStaSendAddBss(tpAniSirGlobal pMac, tpSirAssocRsp pAssocRsp,
                                    tpSchBeaconStruct pBeaconStruct, tpSirBssDescription bssDescription, tANI_U8 updateEntry, tpPESession psessionEntry);
tSirRetStatus limStaSendAddBssPreAssoc( tpAniSirGlobal pMac, tANI_U8 updateEntry, tpPESession psessionEntry);

void limPrepareAndSendDelStaCnf(tpAniSirGlobal pMac, tpDphHashNode pStaDs, tSirResultCodes statusCode,tpPESession);
tSirRetStatus limExtractApCapabilities(tpAniSirGlobal pMac, tANI_U8 * pIE, tANI_U16 ieLen, tpSirProbeRespBeacon beaconStruct);
void limInitPreAuthTimerTable(tpAniSirGlobal pMac, tpLimPreAuthTable pPreAuthTimerTable);
tpLimPreAuthNode limAcquireFreePreAuthNode(tpAniSirGlobal pMac, tpLimPreAuthTable pPreAuthTimerTable);
tpLimPreAuthNode limGetPreAuthNodeFromIndex(tpAniSirGlobal pMac, tpLimPreAuthTable pAuthTable, tANI_U32 authNodeIdx);

/* Util API to check if the channels supported by STA is within range */
tSirRetStatus limIsDot11hSupportedChannelsValid(tpAniSirGlobal pMac, tSirAssocReq *assoc);

/* Util API to check if the txpower supported by STA is within range */
tSirRetStatus limIsDot11hPowerCapabilitiesInRange(tpAniSirGlobal pMac, tSirAssocReq *assoc,tpPESession);

/* API to re-add the same BSS during re-association */
void limHandleAddBssInReAssocContext(tpAniSirGlobal pMac, tpDphHashNode pStaDs, tpPESession psessionEntry);

/* API to fill in RX Highest Supported data Rate */
void limFillRxHighestSupportedRate(tpAniSirGlobal pMac, tANI_U16 *rxHighestRate, tANI_U8* pSupportedMCSSet);
#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_CCX) || defined(FEATURE_WLAN_LFR)
void limSendRetryReassocReqFrame(tpAniSirGlobal pMac, tLimMlmReassocReq *pMlmReassocReq, tpPESession psessionEntry);
#endif
#ifdef WLAN_FEATURE_11W
void limSendSmeUnprotectedMgmtFrameInd(tpAniSirGlobal pMac, tANI_U8 frameType,
                                       tANI_U8  *frame, tANI_U32 frameLen, tANI_U16 sessionId, tpPESession psessionEntry);
#endif

#if defined(FEATURE_WLAN_CCX) && defined(FEATURE_WLAN_CCX_UPLOAD)
void limSendSmeTsmIEInd( tpAniSirGlobal pMac, tpPESession psessionEntry,
                            tANI_U8 tid, tANI_U8 state, tANI_U16 measInterval);
#endif /* FEATURE_WLAN_CCX && FEATURE_WLAN_CCX_UPLOAD */


#endif /* __LIM_ASSOC_UTILS_H */

