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

/** ------------------------------------------------------------------------- * 
    ------------------------------------------------------------------------- *  
    \file ccmApi.h
  
    \brief Exports and types for the Common Config Module (CCM)
  
    $Id$ 
  
  
    Copyright (C) 2006 Airgo Networks, Incorporated

    This file contains all the interfaces for thge Platform Abstration Layer
    functions.  It is intended to be included in all modules that are using 
    the PAL interfaces.
  
   ========================================================================== */
#ifndef CCMAPI_H__
#define CCMAPI_H__

//#include "wniCfgAp.h" /* CFG_PARAM_MAX_NUM */
#include "wniCfgSta.h"
#include "halTypes.h"

#define CCM_11B_CHANNEL_END             14

#define CCM_IS_RESULT_SUCCESS(result)   (WNI_CFG_SUCCESS == (result) ||\
                                         WNI_CFG_NEED_RESTART == (result) || \
                                         WNI_CFG_NEED_RELOAD == (result))

#define CCM_INTEGER_TYPE                0
#define CCM_STRING_TYPE                 1

typedef void (*tCcmCfgSetCallback)(tHalHandle hHal, tANI_S32 result) ;

typedef enum {
    eCCM_STOPPED,
    eCCM_STARTED,
    eCCM_REQ_SENT,
    eCCM_REQ_QUEUED,
    eCCM_REQ_DONE,
} eCcmState ;

/* We do not use Linux's list facility */
typedef struct cfgreq {
    struct cfgreq       *next ;
    tANI_U16            cfgId ;
    tANI_U8             type ;
    tANI_U8             state : 7 ;
    tANI_U8             toBeSaved : 1 ;
    tANI_S32            length ;
    void                *ccmPtr;
    tANI_U32            ccmValue;
    tCcmCfgSetCallback  callback;
    void                *done ;
} tCfgReq ;

typedef struct {
    tANI_U16            started : 1 ;
    tANI_U16            in_progress : 1 ;
    tANI_U16            reserved : 14 ;
    tANI_S16            nr_param ;
    tANI_U32            result ;
    tCcmCfgSetCallback  callback ;
    void                *done ;
} tCfgReplay ;

struct ccmlink {
    tCfgReq *head;
    tCfgReq *tail;
} ;

typedef struct {
    struct ccmlink      reqQ ;
    eCcmState           state ;
    tCfgReq *           comp[CFG_PARAM_MAX_NUM] ;
    tCfgReplay          replay ;
    void                *lock;
} tCcm ;

void ccmCfgCnfMsgHandler(tHalHandle hHal, void *msg) ;
eHalStatus ccmOpen(tHalHandle hHal) ;
eHalStatus ccmClose(tHalHandle hHal) ;
void ccmStart(tHalHandle hHal) ;
void ccmStop(tHalHandle hHal) ;
//If callback is NULL, the API is not serialized for the CFGs
eHalStatus ccmCfgSetInt(tHalHandle hHal, tANI_U32 cfgId, tANI_U32 ccmValue, tCcmCfgSetCallback callback, eAniBoolean toBeSaved) ;
//If callback is NULL, the API is not serialized for the CFGs
eHalStatus ccmCfgSetStr(tHalHandle hHal, tANI_U32 cfgId, tANI_U8 *pStr, tANI_U32 length, tCcmCfgSetCallback callback, eAniBoolean toBeSaved) ;
eHalStatus ccmCfgUpdate(tHalHandle hHal, tCcmCfgSetCallback callback) ;
eHalStatus ccmCfgGetInt(tHalHandle hHal, tANI_U32 cfgId, tANI_U32 *pValue) ;
eHalStatus ccmCfgGetStr(tHalHandle hHal, tANI_U32 cfgId, tANI_U8 *pBuf, tANI_U32 *pLength) ;

void ccmDumpInit(tHalHandle hHal);

#endif /*CCMAPI_H__*/
