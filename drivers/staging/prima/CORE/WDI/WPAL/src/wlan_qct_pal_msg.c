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

/**=========================================================================
  
  \file  wlan_qct_pal_msg.c
  
  \brief Implementation message APIs PAL exports. wpt = (Wlan Pal Type) wpal = (Wlan PAL)
               
   Definitions for platform with legacy UMAC support.
  
  
  ========================================================================*/

#include "wlan_qct_pal_msg.h"
#include "wlan_qct_pal_api.h"
#include "wlan_qct_pal_trace.h"
#include "vos_mq.h"



/*---------------------------------------------------------------------------
     wpalPostCtrlMsg - Post a message to control context so it can be processed in that context.
    Param: 
        pPalContext - A PAL context
        pMsg - a pointer to called allocated opaque object;
---------------------------------------------------------------------------*/
wpt_status wpalPostCtrlMsg(void *pPalContext, wpt_msg *pMsg)
{
   wpt_status status = eWLAN_PAL_STATUS_E_FAILURE;
   vos_msg_t msg;

   if (NULL == pMsg)
   {
      WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_ERROR,
                 "%s: NULL message pointer", __func__);
      WPAL_ASSERT(0);
      return status;
   }

   if ((WPAL_MC_MSG_SMD_NOTIF_OPEN_SIG == pMsg->type) ||
       (WPAL_MC_MSG_SMD_NOTIF_DATA_SIG == pMsg->type))
   {
      /* SMD NOTIFY MSG has none 0 vos MSG type
       * If VOS MC MSG flush procedure detect this,
       * Do not free MSG body */
      msg.type = pMsg->type;
   }
   else
   {
      /* Default MSG type
       * VOS MC MSG flush procedure will free MSG body */
      msg.type = 0;
   }
   msg.reserved = 0;
   msg.bodyval = 0;
   msg.bodyptr = pMsg;
   if(VOS_IS_STATUS_SUCCESS(vos_mq_post_message(VOS_MQ_ID_WDI, &msg)))
   {
      status = eWLAN_PAL_STATUS_SUCCESS;
   }
   else
   {
      WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_ERROR, "%s fail to post msg %d",
                  __func__, pMsg->type);
   }

   return status;
}



/*---------------------------------------------------------------------------
     wpalPostTxMsg - Post a message to TX context so it can be processed in that context.
    Param: 
        pPalContext - A PAL context PAL
        pMsg - a pointer to called allocated opaque object;
---------------------------------------------------------------------------*/
wpt_status wpalPostTxMsg(void *pPalContext, wpt_msg *pMsg)
{
   wpt_status status = eWLAN_PAL_STATUS_E_FAILURE;
   vos_msg_t msg;

   if (NULL == pMsg)
   {
      WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_ERROR,
                 "%s: NULL message pointer", __func__);
      WPAL_ASSERT(0);
      return status;
   }

   msg.type = 0; //This field is not used because VOSS doesn't check it.
   msg.reserved = 0;
   msg.bodyval = 0;
   msg.bodyptr = pMsg;
   if(VOS_IS_STATUS_SUCCESS(vos_tx_mq_serialize(VOS_MQ_ID_WDI, &msg)))
   {
      status = eWLAN_PAL_STATUS_SUCCESS;
   }
   else
   {
      WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_ERROR, "%s fail to post msg %d",
                  __func__, pMsg->type);
   }

   return status;
}


/*---------------------------------------------------------------------------
     wpalPostRxMsg - Post a message to RX context so it can be processed in that context.
    Param: 
        pPalContext - A PAL context
        pMsg - a pointer to called allocated opaque object;
---------------------------------------------------------------------------*/
wpt_status wpalPostRxMsg(void *pPalContext, wpt_msg *pMsg)
{
   wpt_status status = eWLAN_PAL_STATUS_E_FAILURE;
   vos_msg_t msg;

   if (NULL == pMsg)
   {
      WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_ERROR,
                 "%s: NULL message pointer", __func__);
      WPAL_ASSERT(0);
      return status;
   }

   msg.type = 0; //This field is not used because VOSS doesn't check it.
   msg.reserved = 0;
   msg.bodyval = 0;
   msg.bodyptr = pMsg;
   if(VOS_IS_STATUS_SUCCESS(vos_rx_mq_serialize(VOS_MQ_ID_WDI, &msg)))
   {
      status = eWLAN_PAL_STATUS_SUCCESS;
   }
   else
   {
      WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_ERROR, "%s fail to post msg %d",
                  __func__, pMsg->type);
   }

   return status;
}

