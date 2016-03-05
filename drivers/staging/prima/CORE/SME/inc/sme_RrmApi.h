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


#if !defined( __SMERRMAPI_H )
#define __SMERRMAPI_H


/**=========================================================================
  
  \file  sme_RrmApi.h
  
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
#include "aniGlobal.h"
#include "sirApi.h"
#include "smeInternal.h"
#include "smeRrmInternal.h"


//APIs
eHalStatus sme_RrmMsgProcessor( tpAniSirGlobal pMac,  v_U16_t msg_type, 
                                void *pMsgBuf);

VOS_STATUS rrmClose (tpAniSirGlobal pMac);
VOS_STATUS rrmReady (tpAniSirGlobal pMac);
VOS_STATUS rrmOpen (tpAniSirGlobal pMac);
VOS_STATUS rrmChangeDefaultConfigParam(tpAniSirGlobal pMac, tpRrmConfigParam pRrmConfig);
VOS_STATUS sme_RrmNeighborReportRequest(tpAniSirGlobal pMac, tANI_U8 sessionId, tpRrmNeighborReq pNeighborReq, tpRrmNeighborRspCallbackInfo callbackInfo);

tRrmNeighborReportDesc* smeRrmGetFirstBssEntryFromNeighborCache( tpAniSirGlobal pMac);
tRrmNeighborReportDesc* smeRrmGetNextBssEntryFromNeighborCache( tpAniSirGlobal pMac, tpRrmNeighborReportDesc pBssEntry);
void sme_RrmProcessBeaconReportReqInd(tpAniSirGlobal pMac, void *pMsgBuf);


#endif
