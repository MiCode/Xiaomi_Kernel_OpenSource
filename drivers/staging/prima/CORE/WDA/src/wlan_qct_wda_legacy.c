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

/*===========================================================================

                       wlan_qct_wda_legacy.c

  OVERVIEW:

  This software unit holds the implementation of the WLAN Device Adaptation
  Layer for the legacy functionalities that were part of the old HAL.

  The functions externalized by this module are to be called ONLY by other
  WLAN modules that properly register with the Transport Layer initially.

  DEPENDENCIES:

  Are listed for each API below.


  Copyright (c) 2008 QUALCOMM Incorporated.
  All Rights Reserved.
  Qualcomm Confidential and Proprietary
===========================================================================*/

/* Standard include files */
/* Application Specific include files */
#include "limApi.h"
#include "pmmApi.h"
#include "cfgApi.h"
#include "wlan_qct_wda_debug.h"

/* Locally used Defines */

#define HAL_MMH_MB_MSG_TYPE_MASK    0xFF00

// -------------------------------------------------------------
/**
 * wdaPostCtrlMsg
 *
 * FUNCTION:
 *     Posts WDA messages to MC thread
 *
 * LOGIC:
 *
 * ASSUMPTIONS:pl
 *
 *
 * NOTE:
 *
 * @param tpAniSirGlobal MAC parameters structure
 * @param pMsg pointer with message
 * @return Success or Failure
 */

tSirRetStatus
wdaPostCtrlMsg(tpAniSirGlobal pMac, tSirMsgQ *pMsg)
{
   if(VOS_STATUS_SUCCESS != vos_mq_post_message(VOS_MQ_ID_WDA, (vos_msg_t *) pMsg))
      return eSIR_FAILURE;
   else
      return eSIR_SUCCESS;
} // halPostMsg()

/**
 * wdaPostCfgMsg
 *
 * FUNCTION:
 *     Posts MNT messages to gSirMntMsgQ
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 *
 * NOTE:
 *
 * @param tpAniSirGlobal MAC parameters structure
 * @param pMsg A pointer to the msg
 * @return Success or Failure
 */

tSirRetStatus
wdaPostCfgMsg(tpAniSirGlobal pMac, tSirMsgQ *pMsg)
{
   tSirRetStatus rc = eSIR_SUCCESS;

   do
   {
      // For Windows based MAC, instead of posting message to different
      // queues we will call the handler routines directly

      cfgProcessMbMsg(pMac, (tSirMbMsg*)pMsg->bodyptr);
      rc = eSIR_SUCCESS;
   } while (0);

   return rc;
} // halMntPostMsg()


// -------------------------------------------------------------
/**
 * uMacPostCtrlMsg
 *
 * FUNCTION:
 *     Forwards the completely received message to the respective
 *    modules for further processing.
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *    Freeing up of the message buffer is left to the destination module.
 *
 * NOTE:
 *  This function has been moved to the API file because for MAC running
 *  on Windows host, the host module will call this routine directly to
 *  send any mailbox messages. Making this function an API makes sure that
 *  outside world (any module outside MMH) only calls APIs to use MMH
 *  services and not an internal function.
 *
 * @param pMb A pointer to the maibox message
 * @return NONE
 */

tSirRetStatus uMacPostCtrlMsg(void* pSirGlobal, tSirMbMsg* pMb)
{
   tSirMsgQ msg;
   tpAniSirGlobal pMac = (tpAniSirGlobal)pSirGlobal;


   tSirMbMsg* pMbLocal;
   msg.type = pMb->type;
   msg.bodyval = 0;

   WDALOG3(wdaLog(pMac, LOG3, FL("msgType %d, msgLen %d" ),
        pMb->type, pMb->msgLen));

   // copy the message from host buffer to firmware buffer
   // this will make sure that firmware allocates, uses and frees
   // it's own buffers for mailbox message instead of working on
   // host buffer

   // second parameter, 'wait option', to palAllocateMemory is ignored on Windows
   pMbLocal = vos_mem_malloc(pMb->msgLen);
   if ( NULL == pMbLocal )
   {
      WDALOGE( wdaLog(pMac, LOGE, FL("Buffer Allocation failed!")));
      return eSIR_FAILURE;
   }

   vos_mem_copy((void *)pMbLocal, (void *)pMb, pMb->msgLen);
   msg.bodyptr = pMbLocal;

   switch (msg.type & HAL_MMH_MB_MSG_TYPE_MASK)
   {
   case WDA_MSG_TYPES_BEGIN:    // Posts a message to the HAL MsgQ
      wdaPostCtrlMsg(pMac, &msg);
      break;

   case SIR_LIM_MSG_TYPES_BEGIN:    // Posts a message to the LIM MsgQ
      limPostMsgApi(pMac, &msg);
      break;

   case SIR_CFG_MSG_TYPES_BEGIN:    // Posts a message to the CFG MsgQ
      wdaPostCfgMsg(pMac, &msg);
      break;

   case SIR_PMM_MSG_TYPES_BEGIN:    // Posts a message to the PMM MsgQ
      pmmPostMessage(pMac, &msg);
      break;

   case SIR_PTT_MSG_TYPES_BEGIN:
      WDALOGW( wdaLog(pMac, LOGW, FL("%s:%d: message type = 0x%X"),
               __func__, __LINE__, msg.type));
      vos_mem_free(msg.bodyptr);
      break;


   default:
      WDALOGW( wdaLog(pMac, LOGW, FL("Unknown message type = "
             "0x%X"),
             msg.type));

      // Release the memory.
      vos_mem_free(msg.bodyptr);
      break;
   }

   return eSIR_SUCCESS;

} // uMacPostCtrlMsg()


/* ---------------------------------------------------------
 * FUNCTION:  wdaGetGlobalSystemRole()
 *
 * Get the global HAL system role. 
 * ---------------------------------------------------------
 */
tBssSystemRole wdaGetGlobalSystemRole(tpAniSirGlobal pMac)
{
   v_VOID_t * pVosContext = vos_get_global_context(VOS_MODULE_ID_WDA, NULL);
   tWDA_CbContext *wdaContext = 
                       vos_get_context(VOS_MODULE_ID_WDA, pVosContext);
   if(NULL == wdaContext)
   {
      VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_ERROR,
                           "%s:WDA context is NULL", __func__); 
      VOS_ASSERT(0);
      return eSYSTEM_UNKNOWN_ROLE;
   }
   return  wdaContext->wdaGlobalSystemRole;
}

