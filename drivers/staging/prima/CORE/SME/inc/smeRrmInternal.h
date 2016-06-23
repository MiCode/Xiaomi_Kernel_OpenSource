/*
 * Copyright (c) 2012-2014 The Linux Foundation. All rights reserved.
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


#if !defined( __SMERRMINTERNAL_H )
#define __SMERRMINTERNAL_H


/**=========================================================================
  
  \file  smeRrmInternal.h
  
  \brief prototype for SME RRM APIs
  
   Copyright 2008 (c) Qualcomm, Incorporated.  All Rights Reserved.
   
   Qualcomm Confidential and Proprietary.
  
  ========================================================================*/

/* $Header$ */

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include "vos_lock.h"
#include "vos_trace.h"
#include "vos_memory.h"
#include "vos_types.h"
#include "palTimer.h"
#include "rrmGlobal.h"

/*-------------------------------------------------------------------------- 
  Type declarations
  ------------------------------------------------------------------------*/
typedef struct sRrmConfigParam
{
   tANI_U8 rrmEnabled;
   tANI_U8 maxRandnInterval;
}tRrmConfigParam, *tpRrmConfigParam;

typedef struct sRrmNeighborReportDesc
{
   tListElem    List;
   tSirNeighborBssDescription   *pNeighborBssDescription;
   tANI_U32                     roamScore;
} tRrmNeighborReportDesc, *tpRrmNeighborReportDesc;


typedef void (*NeighborReportRspCallback) (void *context, VOS_STATUS vosStatus);

typedef struct sRrmNeighborRspCallbackInfo
{
    tANI_U32                  timeout;  //in ms.. min value is 10 (10ms)
    NeighborReportRspCallback neighborRspCallback;
    void                      *neighborRspCallbackContext;
} tRrmNeighborRspCallbackInfo, *tpRrmNeighborRspCallbackInfo;

typedef struct sRrmNeighborRequestControlInfo
{
    tANI_BOOLEAN    isNeighborRspPending;   //To check whether a neighbor req is already sent and response pending
    vos_timer_t     neighborRspWaitTimer;
    tRrmNeighborRspCallbackInfo neighborRspCallbackInfo;
} tRrmNeighborRequestControlInfo, *tpRrmNeighborRequestControlInfo;

typedef struct sRrmSMEContext
{
   tANI_U16 token;
   tCsrBssid sessionBssId;
   tANI_U8 regClass;
   tCsrChannelInfo channelList; //list of all channels to be measured.
   tANI_U8 currentIndex;
   tAniSSID ssId;  //SSID used in the measuring beacon report.
   tSirMacAddr bssId; //bssid used for beacon report measurement.
   tANI_U16 randnIntvl; //Randomization interval to be used in subsequent measurements.
   tANI_U16 duration[SIR_ESE_MAX_MEAS_IE_REQS];
   tANI_U8 measMode[SIR_ESE_MAX_MEAS_IE_REQS];
   tRrmConfigParam rrmConfig;
   vos_timer_t IterMeasTimer;
   tDblLinkList neighborReportCache;
   tRrmNeighborRequestControlInfo neighborReqControlInfo;

#if defined(FEATURE_WLAN_ESE) && defined(FEATURE_WLAN_ESE_UPLOAD)
   tCsrEseBeaconReq  eseBcnReqInfo;
#endif /* FEATURE_WLAN_ESE && FEATURE_WLAN_ESE_UPLOAD */
   tRrmMsgReqSource msgSource;
}tRrmSMEContext, *tpRrmSMEContext; 

typedef struct sRrmNeighborReq
{
   tANI_U8 no_ssid;
   tSirMacSSid ssid;
}tRrmNeighborReq, *tpRrmNeighborReq;

#endif //#if !defined( __SMERRMINTERNAL_H )
