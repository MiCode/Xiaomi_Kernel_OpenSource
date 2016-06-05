/*
 * Copyright (c) 2012-2013 The Linux Foundation. All rights reserved.
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
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

/*
 *
 * limSendMessages.h: Provides functions to send messages or Indications to HAL.
 * Author:    Sunit Bhatia
 * Date:       09/21/2006
 * History:-
 * Date        Modified by            Modification Information
 *
 * --------------------------------------------------------------------------
 *
 */
#ifndef __LIM_SEND_MESSAGES_H
#define __LIM_SEND_MESSAGES_H

#include "aniGlobal.h"
#include "limTypes.h"
#include "halMsgApi.h"
#include "sirParams.h"    
tSirRetStatus limSendCFParams(tpAniSirGlobal pMac, tANI_U8 bssIdx, tANI_U8 cfpCount, tANI_U8 cfpPeriod);
tSirRetStatus limSendBeaconParams(tpAniSirGlobal pMac, 
                                  tpUpdateBeaconParams pUpdatedBcnParams,
                                  tpPESession  psessionEntry );
//tSirRetStatus limSendBeaconParams(tpAniSirGlobal pMac, tpUpdateBeaconParams pUpdatedBcnParams);
tSirRetStatus limSendModeUpdate(tpAniSirGlobal pMac, 
                                tUpdateVHTOpMode *tempParam,
                                tpPESession  psessionEntry );

#ifdef WLAN_FEATURE_11AC
tANI_U32 limGetCenterChannel(tpAniSirGlobal pMac,
                             tANI_U8 primarychanNum,
                             ePhyChanBondState secondaryChanOffset, 
                             tANI_U8 chanWidth);
#endif
#if defined WLAN_FEATURE_VOWIFI  
tSirRetStatus limSendSwitchChnlParams(tpAniSirGlobal pMac, tANI_U8 chnlNumber, 
                                      ePhyChanBondState secondaryChnlOffset, 
                                      tPowerdBm maxTxPower,tANI_U8 peSessionId);
#else
tSirRetStatus limSendSwitchChnlParams(tpAniSirGlobal pMac, tANI_U8 chnlNumber, 
                                      ePhyChanBondState secondaryChnlOffset, 
                                      tANI_U8 localPwrConstraint,tANI_U8 peSessionId);
#endif
tSirRetStatus limSendEdcaParams(tpAniSirGlobal pMac, tSirMacEdcaParamRecord *pUpdatedEdcaParams, tANI_U16 bssIdx, tANI_BOOLEAN highPerformance);
tSirRetStatus limSetLinkState(tpAniSirGlobal pMac, tSirLinkState state,  tSirMacAddr bssId, 
                              tSirMacAddr selfMac, tpSetLinkStateCallback callback,
                              void *callbackArg);
#ifdef WLAN_FEATURE_VOWIFI_11R
extern tSirRetStatus limSetLinkStateFT(tpAniSirGlobal pMac, tSirLinkState 
state,tSirMacAddr bssId, tSirMacAddr selfMacAddr, int ft, tpPESession psessionEntry);
#endif
tSirRetStatus limSendSetTxPowerReq(tpAniSirGlobal pMac, tANI_U32 *pTxPowerReq);
tSirRetStatus limSendGetTxPowerReq(tpAniSirGlobal pMac, tpSirGetTxPowerReq pTxPowerReq);
void limSetActiveEdcaParams(tpAniSirGlobal pMac, tSirMacEdcaParamRecord *plocalEdcaParams, tpPESession psessionEntry);
tSirRetStatus limSendHT40OBSSScanInd(tpAniSirGlobal pMac,
                                               tpPESession psessionEntry);
tSirRetStatus limSendHT40OBSSStopScanInd(tpAniSirGlobal pMac,
                                                    tpPESession psessionEntry);

#define CAPABILITY_FILTER_MASK  0x73CF
#define ERP_FILTER_MASK         0xF8
#define EDCA_FILTER_MASK        0xF0
#define QOS_FILTER_MASK         0xF0
#define HT_BYTE0_FILTER_MASK    0x0
#define HT_BYTE1_FILTER_MASK    0xF8
#define HT_BYTE2_FILTER_MASK    0xEB
#define HT_BYTE5_FILTER_MASK    0xFD
#define DS_PARAM_CHANNEL_MASK   0x0
#define VHTOP_CHWIDTH_MASK      0xFC

tSirRetStatus limSendBeaconFilterInfo(tpAniSirGlobal pMac, tpPESession psessionEntry);

#ifdef WLAN_FEATURE_11W
tSirRetStatus limSendExcludeUnencryptInd(tpAniSirGlobal pMac,
                                         tANI_BOOLEAN excludeUnenc,
                                         tpPESession  psessionEntry );
#endif
#endif
