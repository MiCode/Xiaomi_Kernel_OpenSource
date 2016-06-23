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

#if !defined( __WLAN_QCT_PAL_MSG_H )
#define __WLAN_QCT_PAL_MSG_H

/**=========================================================================
  
  \file  wlan_qct_pal_msg.h
  
  \brief define general message APIs PAL exports to support legacy UMAC. 
   wpt = (Wlan Pal Type) wpal = (Wlan PAL)
               
   Definitions for platform dependent. Only work with legacy UMAC.
  
   Copyright 2010 (c) Qualcomm, Incorporated.  All Rights Reserved.
   
   Qualcomm Confidential and Proprietary.
  
  ========================================================================*/

#include "wlan_qct_pal_type.h"
#include "wlan_qct_pal_status.h"

/* Random signature to detect SMD OPEN NOTIFY */
#define WPAL_MC_MSG_SMD_NOTIF_OPEN_SIG   0x09E2

/* Random signature to detect SMD DATA NOTIFY */
#define WPAL_MC_MSG_SMD_NOTIF_DATA_SIG   0xDA7A

typedef struct swpt_msg wpt_msg;

typedef void (*wpal_msg_callback)(wpt_msg *pMsg);

struct swpt_msg
{
   wpt_uint16 type;
   wpt_uint16 reserved;
   void *ptr;
   wpt_uint32 val;
   wpal_msg_callback callback;
   void *pContext;
}; 


/*---------------------------------------------------------------------------
   wpalPostCtrlMsg – Post a message to control context so it can 
                           be processed in that context.
   Param: 
      pPalContext – A PAL context
      pMsg – a pointer to called allocated object; Caller retain the ownership 
             after this API returns.
---------------------------------------------------------------------------*/
wpt_status wpalPostCtrlMsg(void *pPalContext, wpt_msg *pMsg);


/*---------------------------------------------------------------------------
   wpalPostTxMsg – Post a message to TX context so it can be processed in that context.
   Param: 
      pPalContext – A PAL context
      pMsg – a pointer to called allocated object; Caller retain the ownership 
             after this API returns.
---------------------------------------------------------------------------*/
wpt_status wpalPostTxMsg(void *pPalContext, wpt_msg *pMsg);

/*---------------------------------------------------------------------------
   wpalPostRxMsg – Post a message to RX context so it can be processed in that context.
   Param: 
      pPalContext – A PAL context
      pMsg – a pointer to called allocated object; Caller retain the ownership 
             after this API returns.
---------------------------------------------------------------------------*/
wpt_status wpalPostRxMsg(void *pPalContext, wpt_msg *pMsg);



#endif // __WLAN_QCT_PAL_API_H
