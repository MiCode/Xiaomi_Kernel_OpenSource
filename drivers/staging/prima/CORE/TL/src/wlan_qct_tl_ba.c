/*
 * Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

/*===========================================================================

                       W L A N _ Q C T _ T L _ B A. C
                                               
  OVERVIEW:
  
  This software unit holds the implementation of the WLAN Transport Layer
  Block Ack session support. Also included are the AMSDU de-aggregation 
  completion and MSDU re-ordering functionality. 
  
  The functions externalized by this module are to be called ONLY by the main
  TL module or the HAL layer.

  DEPENDENCIES: 

  Are listed for each API below. 
===========================================================================*/

/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


   $Header$$DateTime$$Author$


  when        who     what, where, why
----------    ---    --------------------------------------------------------
2010-10-xx    dli     Change ucCIndex to point to the slot the next frame to be expected to fwd
2008-08-22    sch     Update based on unit test
2008-07-31    lti     Created module

===========================================================================*/

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
#include "wlan_qct_tl.h" 
#include "wlan_qct_wda.h" 
#include "wlan_qct_tli.h" 
#include "wlan_qct_tli_ba.h" 
#include "wlan_qct_hal.h" 
#include "wlan_qct_tl_trace.h"
#include "vos_trace.h"
#include "vos_types.h"
#include "vos_list.h"
#include "vos_lock.h"
#include "tlDebug.h"
/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/
//#define WLANTL_REORDER_DEBUG_MSG_ENABLE
#define WLANTL_BA_REORDERING_AGING_TIMER   30   /* 30 millisec */
#define WLANTL_BA_MIN_FREE_RX_VOS_BUFFER   0    /* RX VOS buffer low threshold */
#define CSN_WRAP_AROUND_THRESHOLD          3000 /* CSN wrap around threshold */


const v_U8_t  WLANTL_TID_2_AC[WLAN_MAX_TID] = {   WLANTL_AC_BE,
                                                  WLANTL_AC_BK,
                                                  WLANTL_AC_BK,
                                                  WLANTL_AC_BE,
                                                  WLANTL_AC_VI,
                                                  WLANTL_AC_VI,
                                                  WLANTL_AC_VO,
                                                  WLANTL_AC_VO };

/*==========================================================================

   FUNCTION    tlReorderingAgingTimerExpierCB

   DESCRIPTION 
      After aging timer expiered, all Qed frames have to be routed to upper
      layer. Otherwise, there is possibilitied that ahng some frames
    
   PARAMETERS 
      v_PVOID_t  timerUdata    Timer callback user data
                               Has information about where frames should be
                               routed
   
   RETURN VALUE
      VOS_STATUS_SUCCESS       General success
      VOS_STATUS_E_INVAL       Invalid frame handle
  
============================================================================*/
v_VOID_t WLANTL_ReorderingAgingTimerExpierCB
(
   v_PVOID_t  timerUdata
)
{
   WLANTL_TIMER_EXPIER_UDATA_T *expireHandle;
   WLANTL_BAReorderType        *ReorderInfo;
   WLANTL_CbType               *pTLHandle;
   WLANTL_STAClientType*       pClientSTA = NULL;
   vos_pkt_t                   *vosDataBuff;
   VOS_STATUS                   status = VOS_STATUS_SUCCESS;
   v_U8_t                       ucSTAID;
   v_U8_t                       ucTID;
   v_U8_t                       opCode;
   WLANTL_RxMetaInfoType        wRxMetaInfo;
   v_U32_t                      fwIdx = 0;
   WDI_DS_RxMetaInfoType       *pRxMetadata;
   vos_pkt_t                   *pCurrent;
   vos_pkt_t                   *pNext;
   v_S15_t                      seq;
   v_U32_t                      cIndex;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   if(NULL == timerUdata)
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Timer Callback User data NULL"));
      return;
   }

   expireHandle = (WLANTL_TIMER_EXPIER_UDATA_T *)timerUdata;
   ucSTAID      = (v_U8_t)expireHandle->STAID;
   ucTID        = expireHandle->TID;
   if(WLANTL_STA_ID_INVALID(ucSTAID) || WLANTL_TID_INVALID(ucTID))
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"SID %d or TID %d is not valid",
                  ucSTAID, ucTID));
      return;
   }

   pTLHandle    = (WLANTL_CbType *)expireHandle->pTLHandle;
   if(NULL == pTLHandle)
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"TL Control block NULL"));
      return;
   }

   pClientSTA = pTLHandle->atlSTAClients[ucSTAID];
   if( NULL == pClientSTA ){
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "TL:STA Memory not allocated STA ID: %d, %s", ucSTAID, __func__));
      return;
   }

   ReorderInfo = &pClientSTA->atlBAReorderInfo[ucTID];
   if(NULL == ReorderInfo)
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Reorder data NULL, this could not happen SID %d, TID %d", 
                  ucSTAID, ucTID));
      return;
   }

   if(0 == pClientSTA->atlBAReorderInfo[ucTID].ucExists)
   {
       TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Reorder session doesn't exist SID %d, TID %d", 
                   ucSTAID, ucTID));
       return;
   }

   if(!VOS_IS_STATUS_SUCCESS(vos_lock_acquire(&ReorderInfo->reorderLock)))
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"WLANTL_ReorderingAgingTimerExpierCB, Get LOCK Fail"));
      return;
   }

   if( 0 == pClientSTA->atlBAReorderInfo[ucTID].ucExists )
   {
      vos_lock_release(&ReorderInfo->reorderLock);
      return;
   }

   opCode      = WLANTL_OPCODE_FWDALL_DROPCUR;
   vosDataBuff = NULL;

   MTRACE(vos_trace(VOS_MODULE_ID_TL, TRACE_CODE_TL_REORDER_TIMER_EXP_CB,
                      ucSTAID , opCode ));


   TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"BA timeout with %d pending frames, curIdx %d", ReorderInfo->pendingFramesCount, ReorderInfo->ucCIndex));

   if(ReorderInfo->pendingFramesCount == 0)
   {
      if(!VOS_IS_STATUS_SUCCESS(vos_lock_release(&ReorderInfo->reorderLock)))
      {
         TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"WLANTL_ReorderingAgingTimerExpierCB, Release LOCK Fail"));
      }
      return;
   }

   if(0 == ReorderInfo->ucCIndex)
   {
      fwIdx = ReorderInfo->winSize;
   }
   else
   {
      fwIdx = ReorderInfo->ucCIndex - 1;
   }

   /* Do replay check before giving packets to upper layer 
      replay check code : check whether replay check is needed or not */
   if(VOS_TRUE == pClientSTA->ucIsReplayCheckValid)
   {
       v_U64_t    ullpreviousReplayCounter = 0;
       v_U64_t    ullcurrentReplayCounter = 0;
       v_U8_t     ucloopCounter = 0;
       v_BOOL_t   status = 0;

       /*Do replay check for all packets which are in Reorder buffer */
       for(ucloopCounter = 0; ucloopCounter < WLANTL_MAX_WINSIZE; ucloopCounter++)
       {
         /*Get previous reply counter*/
         ullpreviousReplayCounter = pClientSTA->ullReplayCounter[ucTID];

         /*Get current replay counter of packet in reorder buffer*/
         ullcurrentReplayCounter = ReorderInfo->reorderBuffer->ullReplayCounter[ucloopCounter];

         /*Check for holes, if a hole is found in Reorder buffer then
           no need to do replay check on it, skip the current
           hole and do replay check on other packets*/
         if(NULL != (ReorderInfo->reorderBuffer->arrayBuffer[ucloopCounter]))
         {
           status = WLANTL_IsReplayPacket(ullcurrentReplayCounter, ullpreviousReplayCounter); 
           if(VOS_TRUE == status)
           {
               /*Increment the debug counter*/
               pClientSTA->ulTotalReplayPacketsDetected++;

               /*A replay packet found*/
               VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                "WLANTL_ReorderingAgingTimerExpierCB: total dropped replay packets on STA ID %X is [0x%lX]\n",
                ucSTAID, pClientSTA->ulTotalReplayPacketsDetected);

               VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                "WLANTL_ReorderingAgingTimerExpierCB: replay packet found with PN : [0x%llX]\n",
                ullcurrentReplayCounter);

               VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                "WLANTL_ReorderingAgingTimerExpierCB: Drop the replay packet with PN : [0x%llX]\n",
                ullcurrentReplayCounter);

               ReorderInfo->reorderBuffer->arrayBuffer[ucloopCounter] = NULL;
               ReorderInfo->reorderBuffer->ullReplayCounter[ucloopCounter] = 0;
           }
           else
           {
              /*Not a replay packet update previous replay counter*/
              pClientSTA->ullReplayCounter[ucTID] = ullcurrentReplayCounter;
           }
         }
         else
         {
              /* A hole detected in Reorder buffer*/
              //BAMSGERROR("WLANTL_ReorderingAgingTimerExpierCB,hole detected\n",0,0,0);
               
         }
       } 
   }

   cIndex = ReorderInfo->ucCIndex;
   status = WLANTL_ChainFrontPkts(fwIdx, opCode, 
                                  &vosDataBuff, ReorderInfo, NULL);
   ReorderInfo->ucCIndex = cIndex;
   if(!VOS_IS_STATUS_SUCCESS(status))
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Make packet chain fail with Qed frames %d", status));
      if(!VOS_IS_STATUS_SUCCESS(vos_lock_release(&ReorderInfo->reorderLock)))
      {
         TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"WLANTL_ReorderingAgingTimerExpierCB, Release LOCK Fail"));
      }
      return;
   }

   if(NULL == pClientSTA->pfnSTARx)
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Callback function NULL with STAID %d", ucSTAID));
      if(!VOS_IS_STATUS_SUCCESS(vos_lock_release(&ReorderInfo->reorderLock)))
      {
         TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"WLANTL_ReorderingAgingTimerExpierCB, Release LOCK Fail"));
      }
      return;
   }

   if(NULL == vosDataBuff)
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"No pending frames, why triggered timer? "));
      if(!VOS_IS_STATUS_SUCCESS(vos_lock_release(&ReorderInfo->reorderLock)))
      {
         TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"WLANTL_ReorderingAgingTimerExpierCB, Release LOCK Fail"));
      }
      return;
   }

   pCurrent = vosDataBuff;

   while (pCurrent != NULL)
   {
       vos_pkt_walk_packet_chain(pCurrent, &pNext, VOS_FALSE);

       if (NULL == pNext)
       {
           /* This is the last packet, retrieve its sequence number */
           pRxMetadata = WDI_DS_ExtractRxMetaData(VOS_TO_WPAL_PKT(pCurrent));
           seq = WDA_GET_RX_REORDER_CUR_PKT_SEQ_NO(pRxMetadata);
       }
       pCurrent = pNext;
   }
   TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,
          "%s: Sending out Frame no: %d to HDD", __func__, seq));
   ReorderInfo->LastSN = seq;

   if( WLAN_STA_SOFTAP == pClientSTA->wSTADesc.wSTAType)
   {
      WLANTL_FwdPktToHDD( expireHandle->pAdapter, vosDataBuff, ucSTAID);
   }
   else
   {
      wRxMetaInfo.ucUP = ucTID;
      pClientSTA->pfnSTARx(expireHandle->pAdapter,
                                           vosDataBuff, ucSTAID, &wRxMetaInfo);
   }
   if(!VOS_IS_STATUS_SUCCESS(vos_lock_release(&ReorderInfo->reorderLock)))
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"WLANTL_ReorderingAgingTimerExpierCB, Release LOCK Fail"));
   }
   return;
}/*WLANTL_ReorderingAgingTimerExpierCB*/

/*----------------------------------------------------------------------------
    INTERACTION WITH TL Main
 ---------------------------------------------------------------------------*/
/*==========================================================================

   FUNCTION    WLANTL_InitBAReorderBuffer

   DESCRIPTION 
      Init Reorder buffer array
    
   PARAMETERS 
      v_PVOID_t   pvosGCtx Global context

   RETURN VALUE
      NONE
  
============================================================================*/

void WLANTL_InitBAReorderBuffer
(
   v_PVOID_t   pvosGCtx
)
{
   WLANTL_CbType        *pTLCb;
   v_U32_t              idx;
   v_U32_t              pIdx;

   pTLCb = VOS_GET_TL_CB(pvosGCtx);
   if (NULL == pTLCb)
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                       "%s: Invalid TL Control Block", __func__));
      return;
   }

   for(idx = 0; idx < WLANTL_MAX_BA_SESSION; idx++)
   {
      pTLCb->reorderBufferPool[idx].isAvailable = VOS_TRUE;
      for(pIdx = 0; pIdx < WLANTL_MAX_WINSIZE; pIdx++)
      {
         pTLCb->reorderBufferPool[idx].arrayBuffer[pIdx] = NULL;
         pTLCb->reorderBufferPool[idx].ullReplayCounter[pIdx] = 0; 
      }
   }

   TLLOG4(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_LOW,"BA reorder buffer init"));
   return;
}

/*==========================================================================

  FUNCTION    WLANTL_BaSessionAdd

  DESCRIPTION 
    HAL notifies TL when a new Block Ack session is being added. 
    
  DEPENDENCIES 
    A BA session on Rx needs to be added in TL before the response is 
    being sent out 
    
  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's 
                    control block can be extracted from its context 
    ucSTAId:        identifier of the station for which requested the BA 
                    session
    ucTid:          Tspec ID for the new BA session
    uSize:          size of the reordering window

   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_INVAL:      Input parameters are invalid 
    VOS_STATUS_E_FAULT:      Station ID is outside array boundaries or pointer 
                             to TL cb is NULL ; access would cause a page fault  
    VOS_STATUS_E_EXISTS:     Station was not registered or BA session already
                             exists
    VOS_STATUS_E_NOSUPPORT:  Not yet supported
    
  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS
WLANTL_BaSessionAdd 
( 
  v_PVOID_t   pvosGCtx, 
  v_U16_t     sessionID,
  v_U32_t     ucSTAId,
  v_U8_t      ucTid, 
  v_U32_t     uBufferSize,
  v_U32_t     winSize,
  v_U32_t     SSN
)
{
  WLANTL_CbType        *pTLCb = NULL;
  WLANTL_STAClientType *pClientSTA = NULL;
  WLANTL_BAReorderType *reorderInfo;
  v_U32_t               idx;
  VOS_STATUS            status = VOS_STATUS_SUCCESS;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
   ------------------------------------------------------------------------*/
  if ( WLANTL_TID_INVALID(ucTid))
  {
    VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
              "WLAN TL:Invalid parameter sent on WLANTL_BaSessionAdd");
    return VOS_STATUS_E_INVAL;
  }

  if ( WLANTL_STA_ID_INVALID( ucSTAId ) )
  {
    VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
              "WLAN TL:Invalid station id requested on WLANTL_BaSessionAdd");
    return VOS_STATUS_E_FAULT;
  }

  /*------------------------------------------------------------------------
    Extract TL control block and check existance
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb ) 
  {
    VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_BaSessionAdd");
    return VOS_STATUS_E_FAULT;
  }

  pClientSTA  = pTLCb->atlSTAClients[ucSTAId];
  if ( NULL == pClientSTA )
  {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "WLAN TL:Client Memory was not allocated on %s", __func__));
      return VOS_STATUS_E_FAILURE;
  }

  if ( 0 == pClientSTA->ucExists )
  {
    VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "WLAN TL:Station was not yet registered on WLANTL_BaSessionAdd");
    return VOS_STATUS_E_EXISTS;
  }

  reorderInfo = &pClientSTA->atlBAReorderInfo[ucTid];
  if (!VOS_IS_STATUS_SUCCESS(
     vos_lock_acquire(&reorderInfo->reorderLock)))
  {
    TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
           "%s: Release LOCK Fail", __func__));
    return VOS_STATUS_E_FAULT;
  }
  /*------------------------------------------------------------------------
    Verify that BA session was not already added
   ------------------------------------------------------------------------*/
  if ( 0 != reorderInfo->ucExists )
  {
    reorderInfo->ucExists++;
    VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
              "WLAN TL:BA session already exists on WLANTL_BaSessionAdd");
    vos_lock_release(&reorderInfo->reorderLock);
    return VOS_STATUS_E_EXISTS;
  }

  /*------------------------------------------------------------------------
    Initialize new BA session 
   ------------------------------------------------------------------------*/
  for(idx = 0; idx < WLANTL_MAX_BA_SESSION; idx++)
  {
    if(VOS_TRUE == pTLCb->reorderBufferPool[idx].isAvailable)
    {
      pClientSTA->atlBAReorderInfo[ucTid].reorderBuffer =
                                            &(pTLCb->reorderBufferPool[idx]);
      pTLCb->reorderBufferPool[idx].isAvailable = VOS_FALSE;
      TLLOG4(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_LOW,"%dth buffer available, buffer PTR 0x%p",
                  idx,
                  pClientSTA->atlBAReorderInfo[ucTid].reorderBuffer
                  ));
      break;
    }
  }

  
  if (WLAN_STA_SOFTAP == pClientSTA->wSTADesc.wSTAType)
  {
      if (WLANTL_MAX_BA_SESSION == idx)
      {
          VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
              "Number of Add BA request received more than allowed");
          vos_lock_release(&reorderInfo->reorderLock);
          return VOS_STATUS_E_NOSUPPORT;
      }
  }
  reorderInfo->timerUdata.pAdapter     = pvosGCtx;
  reorderInfo->timerUdata.pTLHandle    = (v_PVOID_t)pTLCb;
  reorderInfo->timerUdata.STAID        = ucSTAId;
  reorderInfo->timerUdata.TID          = ucTid;

  /* BA aging timer */
  status = vos_timer_init(&reorderInfo->agingTimer,
                          VOS_TIMER_TYPE_SW,
                          WLANTL_ReorderingAgingTimerExpierCB,
                          (v_PVOID_t)(&reorderInfo->timerUdata));
  if(!VOS_IS_STATUS_SUCCESS(status))
  {
     TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
            "%s: Timer Init Fail", __func__));
     vos_lock_release(&reorderInfo->reorderLock);
     return status;
  }

  pClientSTA->atlBAReorderInfo[ucTid].ucExists++;
  pClientSTA->atlBAReorderInfo[ucTid].usCount   = 0;
  pClientSTA->atlBAReorderInfo[ucTid].ucCIndex  = 0;
  if(0 == winSize)
  {
    pClientSTA->atlBAReorderInfo[ucTid].winSize = WLANTL_MAX_WINSIZE;
  }
  else
  {
    pClientSTA->atlBAReorderInfo[ucTid].winSize   = winSize;
  }
  pClientSTA->atlBAReorderInfo[ucTid].SSN       = SSN;
  pClientSTA->atlBAReorderInfo[ucTid].sessionID = sessionID;
  pClientSTA->atlBAReorderInfo[ucTid].pendingFramesCount = 0;
  pClientSTA->atlBAReorderInfo[ucTid].LastSN = SSN;
  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
             "WLAN TL:New BA session added for STA: %d TID: %d",
             ucSTAId, ucTid));

  if(!VOS_IS_STATUS_SUCCESS(
     vos_lock_release(&reorderInfo->reorderLock)))
  {
    TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
           "%s: Release LOCK Fail", __func__));
    return VOS_STATUS_E_FAULT;
  }
  return VOS_STATUS_SUCCESS;
}/* WLANTL_BaSessionAdd */

/*==========================================================================

  FUNCTION    WLANTL_BaSessionDel

  DESCRIPTION 
    HAL notifies TL when a new Block Ack session is being deleted. 
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's 
                    control block can be extracted from its context 
    ucSTAId:        identifier of the station for which requested the BA 
                    session
    ucTid:          Tspec ID for the new BA session
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_INVAL:      Input parameters are invalid 
    VOS_STATUS_E_FAULT:      Station ID is outside array boundaries or pointer 
                             to TL cb is NULL ; access would cause a page fault  
    VOS_STATUS_E_EXISTS:     Station was not registered or BA session already
                             exists
    VOS_STATUS_E_NOSUPPORT:  Not yet supported
    
  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS
WLANTL_BaSessionDel 
( 
  v_PVOID_t      pvosGCtx, 
  v_U16_t        ucSTAId,
  v_U8_t         ucTid
)
{
  WLANTL_CbType*          pTLCb       = NULL;
  WLANTL_STAClientType    *pClientSTA   = NULL;
  vos_pkt_t*              vosDataBuff = NULL;
  VOS_STATUS              vosStatus   = VOS_STATUS_E_FAILURE;
  VOS_STATUS              lockStatus = VOS_STATUS_E_FAILURE;  
  WLANTL_BAReorderType*   reOrderInfo = NULL;
  WLANTL_RxMetaInfoType   wRxMetaInfo;
  v_U32_t                 fwIdx = 0;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*------------------------------------------------------------------------
    Sanity check
   ------------------------------------------------------------------------*/
  if ( WLANTL_TID_INVALID(ucTid))
  {
    VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
              "WLAN TL:Invalid parameter sent on WLANTL_BaSessionDel");
    return VOS_STATUS_E_INVAL;
  }

   if ( WLANTL_STA_ID_INVALID( ucSTAId ) )
  {
    VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
              "WLAN TL:Invalid station id requested on WLANTL_BaSessionDel");
    return VOS_STATUS_E_FAULT;
  }

  /*------------------------------------------------------------------------
    Extract TL control block and check existance
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb ) 
  {
    VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_BaSessionDel");
    return VOS_STATUS_E_FAULT;
  }

  pClientSTA = pTLCb->atlSTAClients[ucSTAId];

  if ( NULL == pClientSTA )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
        "WLAN TL:Client Memory was not allocated on %s", __func__));
    return VOS_STATUS_E_FAILURE;
  }

  if (( 0 == pClientSTA->ucExists ) &&
      ( 0 == pClientSTA->atlBAReorderInfo[ucTid].ucExists ))
  {
    VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_WARN,
          "WLAN TL:Station was not yet registered on WLANTL_BaSessionDel");
    return VOS_STATUS_E_EXISTS;
  }
  else if(( 0 == pClientSTA->ucExists ) &&
          ( 0 != pClientSTA->atlBAReorderInfo[ucTid].ucExists ))
  {
    VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_WARN,
          "STA was deleted but BA info is still there, just remove BA info");

    reOrderInfo = &pClientSTA->atlBAReorderInfo[ucTid];
    reOrderInfo->reorderBuffer->isAvailable = VOS_TRUE;
    memset(&reOrderInfo->reorderBuffer->arrayBuffer[0],
           0,
           WLANTL_MAX_WINSIZE * sizeof(v_PVOID_t));
    vos_timer_destroy(&reOrderInfo->agingTimer);
    memset(reOrderInfo, 0, sizeof(WLANTL_BAReorderType));

    return VOS_STATUS_SUCCESS;
  }

  /*------------------------------------------------------------------------
    Verify that BA session was added
   ------------------------------------------------------------------------*/
  if ( 0 == pClientSTA->atlBAReorderInfo[ucTid].ucExists )
  {
    VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
               "WLAN TL:BA session does not exists on WLANTL_BaSessionDel");
    return VOS_STATUS_E_EXISTS;
  }

  
  /*------------------------------------------------------------------------
     Send all pending packets to HDD 
   ------------------------------------------------------------------------*/
  reOrderInfo = &pClientSTA->atlBAReorderInfo[ucTid];

  /*------------------------------------------------------------------------
     Invalidate reorder info here. This ensures that no packets are 
     bufferd after  reorder buffer is cleaned.
   */
  lockStatus = vos_lock_acquire(&reOrderInfo->reorderLock);
  if(!VOS_IS_STATUS_SUCCESS(lockStatus))
  {
    TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "Unable to acquire reorder vos lock in %s\n", __func__));
    return lockStatus;
  }
  pClientSTA->atlBAReorderInfo[ucTid].ucExists = 0;

  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
               "WLAN TL: Fwd all packets to HDD on WLANTL_BaSessionDel"));

  if(0 == reOrderInfo->ucCIndex)
  {
     fwIdx = reOrderInfo->winSize;
  }
  else
  {
     fwIdx = reOrderInfo->ucCIndex - 1;
  }

  if(0 != reOrderInfo->pendingFramesCount)
  {
    vosStatus = WLANTL_ChainFrontPkts(fwIdx,
                                      WLANTL_OPCODE_FWDALL_DROPCUR,
                                      &vosDataBuff, reOrderInfo, pTLCb);
  }

  if ((VOS_STATUS_SUCCESS == vosStatus) && (NULL != vosDataBuff))
  {
    TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
             "WLAN TL: Chaining was successful sending all pkts to HDD : %x",
              vosDataBuff ));

    if ( WLAN_STA_SOFTAP == pClientSTA->wSTADesc.wSTAType )
    {
      WLANTL_FwdPktToHDD( pvosGCtx, vosDataBuff, ucSTAId);
    }
    else
    {
      wRxMetaInfo.ucUP = ucTid;
      pClientSTA->pfnSTARx( pvosGCtx, vosDataBuff, ucSTAId,
                                            &wRxMetaInfo );
    }
  }

  /*------------------------------------------------------------------------
     Delete reordering timer
   ------------------------------------------------------------------------*/
  if(VOS_TIMER_STATE_RUNNING == vos_timer_getCurrentState(&reOrderInfo->agingTimer))
  {
    vosStatus = vos_timer_stop(&reOrderInfo->agingTimer);
    if(!VOS_IS_STATUS_SUCCESS(vosStatus))
    { 
       TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Timer stop fail: %d", vosStatus));
       vos_lock_release(&reOrderInfo->reorderLock);
       return vosStatus;
    }
  }

  if(VOS_TIMER_STATE_STOPPED == vos_timer_getCurrentState(&reOrderInfo->agingTimer))
  {
     vosStatus = vos_timer_destroy(&reOrderInfo->agingTimer);
  }
  else
  {
    TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Timer is not stopped state current state is %d",
                vos_timer_getCurrentState(&reOrderInfo->agingTimer)));
  }
  if ( VOS_STATUS_SUCCESS != vosStatus ) 
  {
    VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_WARN,
              "WLAN TL:Failed to destroy reorder timer on WLANTL_BaSessionAdd");
  }

  /*------------------------------------------------------------------------
    Delete session 
   ------------------------------------------------------------------------*/
  pClientSTA->atlBAReorderInfo[ucTid].usCount  = 0;
  pClientSTA->atlBAReorderInfo[ucTid].ucCIndex = 0;
  reOrderInfo->winSize   = 0;
  reOrderInfo->SSN       = 0;
  reOrderInfo->sessionID = 0;
  reOrderInfo->LastSN = 0;

  MTRACE(vos_trace(VOS_MODULE_ID_TL, TRACE_CODE_TL_BA_SESSION_DEL,
                      ucSTAId, ucTid ));

  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
             "WLAN TL: BA session deleted for STA: %d TID: %d",
             ucSTAId, ucTid));

  memset((v_U8_t *)(&reOrderInfo->reorderBuffer->arrayBuffer[0]),
                    0,
                    WLANTL_MAX_WINSIZE * sizeof(v_PVOID_t));
  reOrderInfo->reorderBuffer->isAvailable = VOS_TRUE;

  vos_lock_release(&reOrderInfo->reorderLock);
  return VOS_STATUS_SUCCESS;
}/* WLANTL_BaSessionDel */


/*----------------------------------------------------------------------------
    INTERACTION WITH TL main module
 ---------------------------------------------------------------------------*/

/*==========================================================================
      AMSDU sub-frame processing module
  ==========================================================================*/
/*==========================================================================
  FUNCTION    WLANTL_AMSDUProcess

  DESCRIPTION 
    Process A-MSDU sub-frame. Start of chain if marked as first frame. 
    Linked at the end of the existing AMSDU chain. 

  DEPENDENCIES 
         
  PARAMETERS 

   IN/OUT:
   vosDataBuff: vos packet for the received data
                 outgoing contains the root of the chain for the rx 
                 aggregated MSDU if the frame is marked as last; otherwise 
                 NULL
   
   IN
   pvosGCtx:     pointer to the global vos context; a handle to TL's 
                 control block can be extracted from its context 
   pvBDHeader:   pointer to the BD header
   ucSTAId:      Station ID 
   ucMPDUHLen:   length of the MPDU header
   usMPDULen:    length of the MPDU 
      
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_INVAL:   invalid input parameters
    VOS_STATUS_E_FAULT:   pointer to TL cb is NULL ; access would cause a 
                          page fault  
    VOS_STATUS_SUCCESS:   Everything is good :)

  Other values can be returned as a result of a function call, please check 
  corresponding API for more info. 
  
  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS
WLANTL_AMSDUProcess
( 
  v_PVOID_t   pvosGCtx,
  vos_pkt_t** ppVosDataBuff, 
  v_PVOID_t   pvBDHeader,
  v_U8_t      ucSTAId,
  v_U8_t      ucMPDUHLen,
  v_U16_t     usMPDULen
)
{
  v_U8_t          ucFsf; /* First AMSDU sub frame */
  v_U8_t          ucAef; /* Error in AMSDU sub frame */
  WLANTL_CbType*  pTLCb = NULL; 
  WLANTL_STAClientType *pClientSTA = NULL;
  v_U8_t          MPDUHeaderAMSDUHeader[WLANTL_MPDU_HEADER_LEN + TL_AMSDU_SUBFRM_HEADER_LEN];
  v_U16_t         subFrameLength;
  v_U16_t         paddingSize;
  VOS_STATUS      vStatus = VOS_STATUS_SUCCESS;
  v_U16_t         MPDUDataOffset;
  v_U16_t         packetLength; 
  static v_U32_t  numAMSDUFrames;
  vos_pkt_t*      vosDataBuff;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
  /*------------------------------------------------------------------------
    Sanity check
   ------------------------------------------------------------------------*/
  if (( NULL == ppVosDataBuff ) || (NULL == *ppVosDataBuff) || ( NULL == pvBDHeader ) || 
      ( WLANTL_STA_ID_INVALID(ucSTAId)) )
  {
    VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
               "WLAN TL:Invalid parameter sent on WLANTL_AMSDUProcess");
    return VOS_STATUS_E_INVAL;
  }

  vosDataBuff = *ppVosDataBuff;
  /*------------------------------------------------------------------------
    Extract TL control block 
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb ) 
  {
    VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
         "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_AMSDUProcess");
    return VOS_STATUS_E_FAULT;
  }

  pClientSTA = pTLCb->atlSTAClients[ucSTAId];

  if ( NULL == pClientSTA )
  {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "WLAN TL:Client Memory was not allocated on %s", __func__));
      return VOS_STATUS_E_FAILURE;
  }

  /*------------------------------------------------------------------------
    Check frame
   ------------------------------------------------------------------------*/
  ucAef =  (v_U8_t)WDA_GET_RX_AEF( pvBDHeader );
  ucFsf =  (v_U8_t)WDA_GET_RX_ESF( pvBDHeader );
  /* On Prima, MPDU data offset not includes BD header size */
  MPDUDataOffset = (v_U16_t)WDA_GET_RX_MPDU_DATA_OFFSET(pvBDHeader);

  if ( WLANHAL_RX_BD_AEF_SET == ucAef ) 
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
               "WLAN TL:Error in AMSDU - dropping entire chain"));

    vos_pkt_return_packet(vosDataBuff);
    *ppVosDataBuff = NULL;
    return VOS_STATUS_SUCCESS; /*Not a transport error*/ 
  }

  if((0 != ucMPDUHLen) && ucFsf)
  {
    /*
     * This is first AMSDU sub frame
     * AMSDU Header should be removed
     * MPDU header should be stored into context to recover next frames
     */
    /* Assumed here Address4 is never part of AMSDU received at TL */
    if (ucMPDUHLen > WLANTL_MPDU_HEADER_LEN)
    {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"MPDU Header length (%d) is greater",ucMPDUHLen));
      vos_pkt_return_packet(vosDataBuff);
      *ppVosDataBuff = NULL;
      return VOS_STATUS_SUCCESS; /*Not a transport error*/
    }

    vStatus = vos_pkt_pop_head(vosDataBuff, MPDUHeaderAMSDUHeader, ucMPDUHLen + TL_AMSDU_SUBFRM_HEADER_LEN);
    if(!VOS_IS_STATUS_SUCCESS(vStatus))
    {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Pop MPDU AMSDU Header fail"));
      vos_pkt_return_packet(vosDataBuff);
      *ppVosDataBuff = NULL;
      return VOS_STATUS_SUCCESS; /*Not a transport error*/ 
    }
    pClientSTA->ucMPDUHeaderLen = ucMPDUHLen;
    vos_mem_copy(pClientSTA->aucMPDUHeader, MPDUHeaderAMSDUHeader, ucMPDUHLen);
    /* AMSDU header stored to handle garbage data within next frame */
  }
  else
  {
    /* Trim garbage, size is frameLoop */
    if(MPDUDataOffset > 0)
    {
      vStatus = vos_pkt_trim_head(vosDataBuff, MPDUDataOffset);
    }
    if(!VOS_IS_STATUS_SUCCESS(vStatus))
    {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Trim Garbage Data fail"));
      vos_pkt_return_packet(vosDataBuff);
      *ppVosDataBuff = NULL;
      return VOS_STATUS_SUCCESS; /*Not a transport error*/ 
    }

    /* Remove MPDU header and AMSDU header from the packet */
    vStatus = vos_pkt_pop_head(vosDataBuff, MPDUHeaderAMSDUHeader, ucMPDUHLen + TL_AMSDU_SUBFRM_HEADER_LEN);
    if(!VOS_IS_STATUS_SUCCESS(vStatus))
    {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"AMSDU Header Pop fail"));
      vos_pkt_return_packet(vosDataBuff);
      *ppVosDataBuff = NULL;
      return VOS_STATUS_SUCCESS; /*Not a transport error*/ 
    }
  } /* End of henalding not first sub frame specific */

  /* Put in MPDU header into all the frame */
  vStatus = vos_pkt_push_head(vosDataBuff, pClientSTA->aucMPDUHeader, pClientSTA->ucMPDUHeaderLen);
  if(!VOS_IS_STATUS_SUCCESS(vStatus))
  {
    TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"MPDU Header Push back fail"));
    vos_pkt_return_packet(vosDataBuff);
    *ppVosDataBuff = NULL;
    return VOS_STATUS_SUCCESS; /*Not a transport error*/ 
  }

  /* Find Padding and remove */
  vos_mem_copy(&subFrameLength, MPDUHeaderAMSDUHeader + ucMPDUHLen + WLANTL_AMSDU_SUBFRAME_LEN_OFFSET, sizeof(v_U16_t));
  subFrameLength = vos_be16_to_cpu(subFrameLength);
  paddingSize = usMPDULen - ucMPDUHLen - subFrameLength - TL_AMSDU_SUBFRM_HEADER_LEN;

  vos_pkt_get_packet_length(vosDataBuff, &packetLength);
  if((paddingSize > 0) && (paddingSize < packetLength))
  {
    /* There is padding bits, remove it */
    vos_pkt_trim_tail(vosDataBuff, paddingSize);
  }
  else if(0 == paddingSize)
  {
    /* No Padding bits */
    /* Do Nothing */
  }
  else
  {
    /* Padding size is larger than Frame size, Actually negative */
    /* Not a valid case, not a valid frame, drop it */
    TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Padding Size is negative, no possible %d", paddingSize));
    vos_pkt_return_packet(vosDataBuff);
    *ppVosDataBuff = NULL;
    return VOS_STATUS_SUCCESS; /*Not a transport error*/ 
  }

  numAMSDUFrames++;
  if(0 == (numAMSDUFrames % 5000))
  {
    TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"%lu AMSDU frames arrived", numAMSDUFrames));
  }
  return VOS_STATUS_SUCCESS;
}/* WLANTL_AMSDUProcess */

/*==========================================================================
      Re-ordering module
  ==========================================================================*/

/*==========================================================================
  FUNCTION    WLANTL_MSDUReorder

  DESCRIPTION 
    MSDU reordering 

  DEPENDENCIES 
         
  PARAMETERS 

   IN
   
   vosDataBuff: vos packet for the received data
   pvBDHeader: pointer to the BD header
   ucSTAId:    Station ID 
      
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_SUCCESS:   Everything is good :)

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS WLANTL_MSDUReorder
( 
   WLANTL_CbType    *pTLCb,
   vos_pkt_t        **vosDataBuff, 
   v_PVOID_t        pvBDHeader,
   v_U8_t           ucSTAId,
   v_U8_t           ucTid
)
{
   WLANTL_BAReorderType *currentReorderInfo;
   WLANTL_STAClientType *pClientSTA = NULL;
   vos_pkt_t            *vosPktIdx;
   v_U8_t               ucOpCode; 
   v_U8_t               ucSlotIdx;
   v_U8_t               ucFwdIdx;
   v_U16_t              CSN;
   v_U32_t              ucCIndexOrig;
   VOS_STATUS           status      = VOS_STATUS_SUCCESS;
   VOS_STATUS           lockStatus  = VOS_STATUS_SUCCESS; 
   VOS_STATUS           timerStatus = VOS_STATUS_SUCCESS; 
   VOS_TIMER_STATE      timerState;
   v_SIZE_t             rxFree;
   v_U64_t              ullreplayCounter = 0; /* 48-bit replay counter */
   v_U8_t               ac;
   v_U16_t              reorderTime;
   if((NULL == pTLCb) || (*vosDataBuff == NULL))
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Invalid ARG pTLCb 0x%p, vosDataBuff 0x%p",
                  pTLCb, *vosDataBuff));
      return VOS_STATUS_E_INVAL;
   }

   pClientSTA = pTLCb->atlSTAClients[ucSTAId];

   if ( NULL == pClientSTA )
   {
       TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
           "WLAN TL:Client Memory was not allocated on %s", __func__));
       return VOS_STATUS_E_FAILURE;
   }

   currentReorderInfo = &pClientSTA->atlBAReorderInfo[ucTid];

   lockStatus = vos_lock_acquire(&currentReorderInfo->reorderLock);
   if(!VOS_IS_STATUS_SUCCESS(lockStatus))
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"WLANTL_MSDUReorder, Release LOCK Fail"));
      return lockStatus;
   }

   if( pClientSTA->atlBAReorderInfo[ucTid].ucExists == 0 )
   {
     vos_lock_release(&currentReorderInfo->reorderLock);
     return VOS_STATUS_E_INVAL;
   }
   ucOpCode  = (v_U8_t)WDA_GET_RX_REORDER_OPCODE(pvBDHeader);
   ucSlotIdx = (v_U8_t)WDA_GET_RX_REORDER_SLOT_IDX(pvBDHeader);
   ucFwdIdx  = (v_U8_t)WDA_GET_RX_REORDER_FWD_IDX(pvBDHeader);
   CSN       = (v_U16_t)WDA_GET_RX_REORDER_CUR_PKT_SEQ_NO(pvBDHeader);



#ifdef WLANTL_HAL_VOLANS
   /* Replay check code : check whether replay check is needed or not */
   if(VOS_TRUE == pClientSTA->ucIsReplayCheckValid)
   {
           /* Getting 48-bit replay counter from the RX BD */
           ullreplayCounter = WDA_DS_GetReplayCounter(aucBDHeader);
   }
#endif 

#ifdef WLANTL_REORDER_DEBUG_MSG_ENABLE
   TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"opCode %d SI %d, FI %d, CI %d seqNo %d", ucOpCode, ucSlotIdx, ucFwdIdx, currentReorderInfo->ucCIndex, CSN));
#else
   TLLOG4(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_LOW,"opCode %d SI %d, FI %d, CI %d seqNo %d", ucOpCode, ucSlotIdx, ucFwdIdx, currentReorderInfo->ucCIndex, CSN));
#endif

   // remember our current CI so that later we can tell if it advanced
   ucCIndexOrig = currentReorderInfo->ucCIndex;

   switch(ucOpCode)
   {
      case WLANTL_OPCODE_INVALID:
         /* Do nothing just pass through current frame */
         break;

      case WLANTL_OPCODE_QCUR_FWDBUF:
         if (currentReorderInfo->LastSN > CSN)
         {
             if ((currentReorderInfo->LastSN - CSN) < CSN_WRAP_AROUND_THRESHOLD)
             {
                 //this frame is received after BA timer is expired, discard it
                 TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,
                          "(QCUR_FWDBUF) dropping old frame, SN=%d LastSN=%d",
                          CSN, currentReorderInfo->LastSN));
                 status = vos_pkt_return_packet(*vosDataBuff);
                 if (!VOS_IS_STATUS_SUCCESS(status))
                 {
                      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                             "(QCUR_FWDBUF) drop old frame fail %d", status));
                 }
                 *vosDataBuff = NULL;
                 lockStatus = vos_lock_release(&currentReorderInfo->reorderLock);
                 if (!VOS_IS_STATUS_SUCCESS(lockStatus))
                 {
                     TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                            "WLANTL_MSDUReorder, Release LOCK Fail"));
                     return lockStatus;
                 }
                 return status;
             }
         }
         currentReorderInfo->LastSN = CSN;
         if(0 == currentReorderInfo->pendingFramesCount)
         {
            //This frame will be fwd'ed to the OS. The next slot is the one we expect next
            currentReorderInfo->ucCIndex = (ucSlotIdx + 1) % currentReorderInfo->winSize;
            lockStatus = vos_lock_release(&currentReorderInfo->reorderLock);
            if(!VOS_IS_STATUS_SUCCESS(lockStatus))
            {
               TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"WLANTL_MSDUReorder, Release LOCK Fail"));
               return lockStatus;
            }
            return status;
         }
         status = WLANTL_QueueCurrent(currentReorderInfo,
                                      vosDataBuff,
                                      ucSlotIdx);
         if(VOS_TRUE == pClientSTA->ucIsReplayCheckValid)
         {
             WLANTL_FillReplayCounter(currentReorderInfo,
                               ullreplayCounter, ucSlotIdx);
         }
         if(VOS_STATUS_E_RESOURCES == status)
         {
            /* This is the case slot index is already cycle one route, route all the frames Qed */
            vosPktIdx = NULL;
            status = WLANTL_ChainFrontPkts(ucFwdIdx,
                                           WLANTL_OPCODE_FWDALL_QCUR, 
                                           &vosPktIdx,
                                           currentReorderInfo,
                                           pTLCb);
            if(!VOS_IS_STATUS_SUCCESS(status))
            {
               TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Make frame chain fail %d", status));
               lockStatus = vos_lock_release(&currentReorderInfo->reorderLock);
               if(!VOS_IS_STATUS_SUCCESS(lockStatus))
               {
                  TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"WLANTL_MSDUReorder, Release LOCK Fail"));
                  return lockStatus;
               }
               return status;
            }
            status = vos_pkt_chain_packet(vosPktIdx, *vosDataBuff, 1);
            *vosDataBuff = vosPktIdx;
            currentReorderInfo->pendingFramesCount = 0;
         }
         else
         {
            vosPktIdx = NULL;
            status = WLANTL_ChainFrontPkts(ucFwdIdx,
                                           WLANTL_OPCODE_QCUR_FWDBUF, 
                                           &vosPktIdx,
                                           currentReorderInfo,
                                           pTLCb);
            if(!VOS_IS_STATUS_SUCCESS(status))
            {
               TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Make frame chain fail %d", status));
               lockStatus = vos_lock_release(&currentReorderInfo->reorderLock);
               if(!VOS_IS_STATUS_SUCCESS(lockStatus))
               {
                  TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"WLANTL_MSDUReorder, Release LOCK Fail"));
                  return lockStatus;
               }
               return status;
            }
            currentReorderInfo->ucCIndex = ucFwdIdx;
            *vosDataBuff = vosPktIdx;
         }
         break;

      case WLANTL_OPCODE_FWDBUF_FWDCUR:
         vosPktIdx = NULL;
         status = WLANTL_ChainFrontPkts(ucFwdIdx,
                                        WLANTL_OPCODE_FWDBUF_FWDCUR, 
                                        &vosPktIdx,
                                        currentReorderInfo,
                                        pTLCb);
         if(!VOS_IS_STATUS_SUCCESS(status))
         {
            TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Make frame chain fail %d", status));
            lockStatus = vos_lock_release(&currentReorderInfo->reorderLock);
            if(!VOS_IS_STATUS_SUCCESS(lockStatus))
            {
               TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"WLANTL_MSDUReorder, Release LOCK Fail"));
               return lockStatus;
            }
            return status;
         }

         if(NULL == vosPktIdx)
         {
            TLLOG4(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_LOW,"Nothing to chain, just send current frame\n"));
         }
         else
         {
            status = vos_pkt_chain_packet(vosPktIdx, *vosDataBuff, 1);
            if(!VOS_IS_STATUS_SUCCESS(status))
            {
               TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Make frame chain with CUR frame fail %d",
                           status));
               lockStatus = vos_lock_release(&currentReorderInfo->reorderLock);
               if(!VOS_IS_STATUS_SUCCESS(lockStatus))
               {
                  TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"WLANTL_MSDUReorder, Release LOCK Fail"));
                  return lockStatus;
               }
               return status;
            }
            *vosDataBuff = vosPktIdx;
         }
         //ucFwdIdx is the slot this packet supposes to take but there is a hole there
         //It looks that the chip will put the next packet into the slot ucFwdIdx.
         currentReorderInfo->ucCIndex = ucFwdIdx;
         break;

      case WLANTL_OPCODE_QCUR:
        if (currentReorderInfo->LastSN > CSN)
        {
            if ((currentReorderInfo->LastSN - CSN) < CSN_WRAP_AROUND_THRESHOLD)
            {
                // this frame is received after BA timer is expired, so disard it
                TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,
                                "(QCUR) dropping old frame, SN=%d LastSN=%d",
                                CSN, currentReorderInfo->LastSN));
                status = vos_pkt_return_packet(*vosDataBuff);
                if (!VOS_IS_STATUS_SUCCESS(status))
                {
                    TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                               "*** (QCUR) drop old frame fail %d", status));
                }
                *vosDataBuff = NULL;
                lockStatus = vos_lock_release(&currentReorderInfo->reorderLock);
                if (!VOS_IS_STATUS_SUCCESS(lockStatus))
                {
                    TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                                   "WLANTL_MSDUReorder, Release LOCK Fail"));
                    return lockStatus;
                }
                return status;
            }
        }

         status = WLANTL_QueueCurrent(currentReorderInfo,
                                      vosDataBuff,
                                      ucSlotIdx);
           if(VOS_TRUE == pClientSTA->ucIsReplayCheckValid)
           {
               WLANTL_FillReplayCounter(currentReorderInfo,
                                 ullreplayCounter, ucSlotIdx);
           }
         if(VOS_STATUS_E_RESOURCES == status)
         {
            /* This is the case slot index is already cycle one route, route all the frames Qed */
            vosPktIdx = NULL;
            status = WLANTL_ChainFrontPkts(ucFwdIdx,
                                           WLANTL_OPCODE_FWDALL_QCUR, 
                                           &vosPktIdx,
                                           currentReorderInfo,
                                           pTLCb);
            if(!VOS_IS_STATUS_SUCCESS(status))
            {
               TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Make frame chain fail %d", status));
               lockStatus = vos_lock_release(&currentReorderInfo->reorderLock);
               if(!VOS_IS_STATUS_SUCCESS(lockStatus))
               {
                  TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"WLANTL_MSDUReorder, Release LOCK Fail"));
                  return lockStatus;
               }
               return status;
            }
            status = vos_pkt_chain_packet(vosPktIdx, *vosDataBuff, 1);
            *vosDataBuff = vosPktIdx;
            currentReorderInfo->pendingFramesCount = 0;
         }
         else
         {
            /* Since current Frame is Qed, no frame will be routed */
            *vosDataBuff = NULL; 
         }
         break;

      case WLANTL_OPCODE_FWDBUF_QUEUECUR:
         vosPktIdx = NULL;
         status = WLANTL_ChainFrontPkts(ucFwdIdx,
                                        WLANTL_OPCODE_FWDBUF_QUEUECUR, 
                                        &vosPktIdx,
                                        currentReorderInfo,
                                        pTLCb);
         if(!VOS_IS_STATUS_SUCCESS(status))
         {
            TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Make chain with buffered frame fail %d",
                        status));
            lockStatus = vos_lock_release(&currentReorderInfo->reorderLock);
            if(!VOS_IS_STATUS_SUCCESS(lockStatus))
            {
               TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"WLANTL_MSDUReorder, Release LOCK Fail"));
               return lockStatus;
            }
            return status;
         }
         //This opCode means the window shift. Enforce the current Index
         currentReorderInfo->ucCIndex = ucFwdIdx;

         status = WLANTL_QueueCurrent(currentReorderInfo,
                                      vosDataBuff,
                                      ucSlotIdx);
           if(VOS_TRUE == pClientSTA->ucIsReplayCheckValid)
           {
               WLANTL_FillReplayCounter(currentReorderInfo,
                                 ullreplayCounter, ucSlotIdx);
           }
         if(VOS_STATUS_E_RESOURCES == status)
         {
            vos_pkt_return_packet(vosPktIdx); 
            /* This is the case slot index is already cycle one route, route all the frames Qed */
            vosPktIdx = NULL;
            status = WLANTL_ChainFrontPkts(ucFwdIdx,
                                           WLANTL_OPCODE_FWDALL_QCUR, 
                                           &vosPktIdx,
                                           currentReorderInfo,
                                           pTLCb);
            if(!VOS_IS_STATUS_SUCCESS(status))
            {
               TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Make frame chain fail %d", status));
               lockStatus = vos_lock_release(&currentReorderInfo->reorderLock);
               if(!VOS_IS_STATUS_SUCCESS(lockStatus))
               {
                  TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"WLANTL_MSDUReorder, Release LOCK Fail"));
                  return lockStatus;
               }
               return status;
            }
            status = vos_pkt_chain_packet(vosPktIdx, *vosDataBuff, 1);
            *vosDataBuff = vosPktIdx;
            currentReorderInfo->pendingFramesCount = 0;
         }
         *vosDataBuff = vosPktIdx;
         break;

      case WLANTL_OPCODE_FWDBUF_DROPCUR:
         vosPktIdx = NULL;
         status = WLANTL_ChainFrontPkts(ucFwdIdx,
                                        WLANTL_OPCODE_FWDBUF_DROPCUR, 
                                        &vosPktIdx,
                                        currentReorderInfo,
                                        pTLCb);
         if(!VOS_IS_STATUS_SUCCESS(status))
         {
            TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Make chain with buffered frame fail %d",
                        status));
            lockStatus = vos_lock_release(&currentReorderInfo->reorderLock);
            if(!VOS_IS_STATUS_SUCCESS(lockStatus))
            {
               TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"WLANTL_MSDUReorder, Release LOCK Fail"));
               return lockStatus;
            }
            return status;
         }

         //Since BAR frame received, set the index to the right location
         currentReorderInfo->ucCIndex = ucFwdIdx;

         /* Current frame has to be dropped, BAR frame */
         status = vos_pkt_return_packet(*vosDataBuff);
         if(!VOS_IS_STATUS_SUCCESS(status))
         {
            TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Drop BAR frame fail %d",
                        status));
            lockStatus = vos_lock_release(&currentReorderInfo->reorderLock);
            if(!VOS_IS_STATUS_SUCCESS(lockStatus))
            {
               TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"WLANTL_MSDUReorder, Release LOCK Fail"));
               return lockStatus;
            }
            return status;
         }
         *vosDataBuff = vosPktIdx;
         break;
 
      case WLANTL_OPCODE_FWDALL_DROPCUR:
         vosPktIdx = NULL;
         status = WLANTL_ChainFrontPkts(ucFwdIdx,
                                        WLANTL_OPCODE_FWDALL_DROPCUR, 
                                        &vosPktIdx,
                                        currentReorderInfo,
                                        pTLCb);
         if(!VOS_IS_STATUS_SUCCESS(status))
         {
            TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Make chain with buffered frame fail %d",
                        status));
            lockStatus = vos_lock_release(&currentReorderInfo->reorderLock);
            if(!VOS_IS_STATUS_SUCCESS(lockStatus))
            {
               TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"WLANTL_MSDUReorder, Release LOCK Fail"));
               return lockStatus;
            }
            return status;
         }

         //Since BAR frame received and beyond cur window, set the index to the right location
         currentReorderInfo->ucCIndex = 0;

         status = vos_pkt_return_packet(*vosDataBuff);
         if(!VOS_IS_STATUS_SUCCESS(status))
         {
            TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Drop BAR frame fail %d",
                        status));
            lockStatus = vos_lock_release(&currentReorderInfo->reorderLock);
            if(!VOS_IS_STATUS_SUCCESS(lockStatus))
            {
               TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"WLANTL_MSDUReorder, Release LOCK Fail"));
               return lockStatus;
            }
            return status;
         }

         *vosDataBuff = vosPktIdx;
         break;

      case WLANTL_OPCODE_FWDALL_QCUR:
         vosPktIdx = NULL;
         status = WLANTL_ChainFrontPkts(currentReorderInfo->winSize,
                                        WLANTL_OPCODE_FWDALL_DROPCUR, 
                                        &vosPktIdx,
                                        currentReorderInfo,
                                        pTLCb);
         if(!VOS_IS_STATUS_SUCCESS(status))
         {
            TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Make chain with buffered frame fail %d",
                        status));
            lockStatus = vos_lock_release(&currentReorderInfo->reorderLock);
            if(!VOS_IS_STATUS_SUCCESS(lockStatus))
            {
               TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"WLANTL_MSDUReorder, Release LOCK Fail"));
               return lockStatus;
            }
            return status;
         }
         status = WLANTL_QueueCurrent(currentReorderInfo,
                                      vosDataBuff,
                                      ucSlotIdx);
           if(VOS_TRUE == pClientSTA->ucIsReplayCheckValid)
           {
               WLANTL_FillReplayCounter(currentReorderInfo,
                                 ullreplayCounter, ucSlotIdx);
           }
         if(!VOS_IS_STATUS_SUCCESS(status))
         {
            TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Q Current frame fail %d",
                        status));
            lockStatus = vos_lock_release(&currentReorderInfo->reorderLock);
            if(!VOS_IS_STATUS_SUCCESS(lockStatus))
            {
               TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"WLANTL_MSDUReorder, Release LOCK Fail"));
               return lockStatus;
            }
            return status;
         }
         currentReorderInfo->ucCIndex = ucSlotIdx;
         *vosDataBuff = vosPktIdx;
         break;

      case WLANTL_OPCODE_TEARDOWN:
         // do we have a procedure in place to teardown BA?

         // fall through to drop the current packet
      case WLANTL_OPCODE_DROPCUR:
         vos_pkt_return_packet(*vosDataBuff);
         *vosDataBuff = NULL;
         break;

      default:
         break;
   }

   /* Check the available VOS RX buffer size
    * If remaining VOS RX buffer is too few, have to make space
    * Route all the Qed frames upper layer
    * Otherwise, RX thread could be stall */
   vos_pkt_get_available_buffer_pool(VOS_PKT_TYPE_RX_RAW, &rxFree);
   if(WLANTL_BA_MIN_FREE_RX_VOS_BUFFER >= rxFree)
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO, "RX Free: %d", rxFree));
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO, "RX free buffer count is too low, Pending frame count is %d",
                  currentReorderInfo->pendingFramesCount));
      vosPktIdx = NULL;
      status = WLANTL_ChainFrontPkts(ucFwdIdx,
                                     WLANTL_OPCODE_FWDALL_DROPCUR, 
                                     &vosPktIdx,
                                     currentReorderInfo,
                                     pTLCb);
      if(!VOS_IS_STATUS_SUCCESS(status))
      {
         TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Make frame chain fail %d", status));
         lockStatus = vos_lock_release(&currentReorderInfo->reorderLock);
         if(!VOS_IS_STATUS_SUCCESS(lockStatus))
         {
            TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"WLANTL_MSDUReorder, Release LOCK Fail"));
            return lockStatus;
         }
         return status;
      }
      if(NULL != *vosDataBuff)
      {
         TLLOG4(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_LOW,"Already something, Chain it"));
         vos_pkt_chain_packet(*vosDataBuff, vosPktIdx, 1);
      }
      else
      {
         *vosDataBuff = vosPktIdx;
      }
      currentReorderInfo->pendingFramesCount = 0;
   }

   /*
    * Current aging timer logic:
    * 1) if we forwarded any packets and the timer is running:
    *    stop the timer
    * 2) if there are packets queued and the timer is not running:
    *    start the timer
    * 3) if timer is running and no pending frame:
    *    stop the timer
    */
   timerState = vos_timer_getCurrentState(&currentReorderInfo->agingTimer);
   if ((VOS_TIMER_STATE_RUNNING == timerState) &&
       ((ucCIndexOrig != currentReorderInfo->ucCIndex) ||
        (0 == currentReorderInfo->pendingFramesCount)))
   {
      TLLOG4(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_LOW,"HOLE filled, Pending Frames Count %d",
                 currentReorderInfo->pendingFramesCount));

      // we forwarded some packets so stop aging the current hole
      timerStatus = vos_timer_stop(&currentReorderInfo->agingTimer);
      timerState = VOS_TIMER_STATE_STOPPED;

      // ignore the returned status since there is a race condition
      // whereby between the time we called getCurrentState() and the
      // time we call stop() the timer could have fired.  In that case
      // stop() will return an error, but we don't care since the
      // timer has stopped
   }

   if (currentReorderInfo->pendingFramesCount > 0)
   {
      if (VOS_TIMER_STATE_STOPPED == timerState)
      {
         TLLOG4(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_LOW,"There is a new HOLE, Pending Frames Count %d",
                    currentReorderInfo->pendingFramesCount));
         ac = WLANTL_TID_2_AC[ucTid];
         if (WLANTL_AC_INVALID(ac))
         {
             reorderTime = WLANTL_BA_REORDERING_AGING_TIMER;
             TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Invalid AC %d using default reorder time %d",
                              ac, reorderTime));
         }
         else
         {
             reorderTime = pTLCb->tlConfigInfo.ucReorderAgingTime[ac];
         }
         timerStatus = vos_timer_start(&currentReorderInfo->agingTimer,
                                       reorderTime);
         if(!VOS_IS_STATUS_SUCCESS(timerStatus))
         {
            TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Timer start fail: %d", timerStatus));
            lockStatus = vos_lock_release(&currentReorderInfo->reorderLock);
            if(!VOS_IS_STATUS_SUCCESS(lockStatus))
            {
               TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"WLANTL_MSDUReorder, Release LOCK Fail"));
               return lockStatus;
            }
            return timerStatus;
         }
      }
      else
      {
         // we didn't forward any packets and the timer was already
         // running so we're still aging the same hole
         TLLOG4(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_LOW,"Still HOLE, Pending Frames Count %d",
                    currentReorderInfo->pendingFramesCount));
      }
   }

   lockStatus = vos_lock_release(&currentReorderInfo->reorderLock);
   if(!VOS_IS_STATUS_SUCCESS(lockStatus))
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"WLANTL_MSDUReorder, Release LOCK Fail"));
      return lockStatus;
   }
   return VOS_STATUS_SUCCESS;
}/* WLANTL_MSDUReorder */


/*==========================================================================
     Utility functions 
  ==========================================================================*/

/*==========================================================================

  FUNCTION    WLANTL_QueueCurrent

  DESCRIPTION 
    It will queue a packet at a given slot index in the MSDU reordering list. 
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    pwBaReorder:   pointer to the BA reordering session info 
    vosDataBuff:   data buffer to be queued
    ucSlotIndex:   slot index 
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_SUCCESS:     Everything is OK

    
  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS WLANTL_QueueCurrent
(
   WLANTL_BAReorderType*  pwBaReorder,
   vos_pkt_t**            vosDataBuff,
   v_U8_t                 ucSlotIndex
)
{
   VOS_STATUS  status = VOS_STATUS_SUCCESS;

   TLLOG4(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_LOW,"vos Packet has to be Qed 0x%p",
               *vosDataBuff));
   if(NULL != pwBaReorder->reorderBuffer->arrayBuffer[ucSlotIndex])
   {
      MTRACE(vos_trace(VOS_MODULE_ID_TL, TRACE_CODE_TL_QUEUE_CURRENT,
                      pwBaReorder->sessionID , pwBaReorder->pendingFramesCount ));

      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"One Cycle rounded, lost many frames already, not in Q %d",
                  pwBaReorder->pendingFramesCount));
      return VOS_STATUS_E_RESOURCES;
   }

   pwBaReorder->reorderBuffer->arrayBuffer[ucSlotIndex] =
                                           (v_PVOID_t)(*vosDataBuff);
   pwBaReorder->pendingFramesCount++;
   TLLOG4(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_LOW,"Assigned, Pending Frames %d at slot %d, dataPtr 0x%x",
               pwBaReorder->pendingFramesCount,
               ucSlotIndex,
               pwBaReorder->reorderBuffer->arrayBuffer[ucSlotIndex]));

   return status;
}/*WLANTL_QueueCurrent*/

/*==========================================================================

  FUNCTION    WLANTL_ChainFrontPkts

  DESCRIPTION 
    It will remove all the packets from the front of a vos list and chain 
    them to a vos pkt . 
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    ucCount:       number of packets to extract
    pwBaReorder:   pointer to the BA reordering session info 

    OUT
    vosDataBuff:   data buffer containing the extracted chain of packets
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_SUCCESS:     Everything is OK

    
  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS WLANTL_ChainFrontPkts
( 
   v_U32_t                fwdIndex,
   v_U8_t                 opCode,
   vos_pkt_t              **vosDataBuff,
   WLANTL_BAReorderType   *pwBaReorder,
   WLANTL_CbType          *pTLCb
)
{
   VOS_STATUS          status = VOS_STATUS_SUCCESS;
   v_U32_t             idx; 
   v_PVOID_t           currentDataPtr = NULL;
   int                 negDetect;
#ifdef WLANTL_REORDER_DEBUG_MSG_ENABLE
#define WLANTL_OUT_OF_WINDOW_IDX    65
   v_U32_t frameIdx[2] = {0, 0}, ffidx = fwdIndex, idx2 = WLANTL_OUT_OF_WINDOW_IDX;
   int pending = pwBaReorder->pendingFramesCount, start = WLANTL_OUT_OF_WINDOW_IDX, end;
#endif

   if(pwBaReorder->ucCIndex >= fwdIndex)
   {
      fwdIndex += pwBaReorder->winSize;
   }

   if((WLANTL_OPCODE_FWDALL_DROPCUR == opCode) ||
      (WLANTL_OPCODE_FWDALL_QCUR == opCode))
   {
      fwdIndex = pwBaReorder->ucCIndex + pwBaReorder->winSize;
   }

   TLLOG4(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_LOW,"Current Index %d, FWD Index %d, reorderBuffer 0x%p",
               pwBaReorder->ucCIndex % pwBaReorder->winSize,
               fwdIndex % pwBaReorder->winSize,
               pwBaReorder->reorderBuffer));

   negDetect = pwBaReorder->pendingFramesCount;
   for(idx = pwBaReorder->ucCIndex; idx <= fwdIndex; idx++)
   {
      currentDataPtr = 
      pwBaReorder->reorderBuffer->arrayBuffer[idx % pwBaReorder->winSize];
      if(NULL != currentDataPtr)
      {
#ifdef WLANTL_REORDER_DEBUG_MSG_ENABLE
         idx2 = (idx >=  pwBaReorder->winSize) ? (idx -  pwBaReorder->winSize) : idx;
         frameIdx[idx2 / 32] |= 1 << (idx2 % 32);
         if(start == WLANTL_OUT_OF_WINDOW_IDX) start = idx2;
#endif
         TLLOG4(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_LOW,"There is buffered frame %d",
                     idx % pwBaReorder->winSize));
         if(NULL == *vosDataBuff)
         {
            *vosDataBuff = (vos_pkt_t *)currentDataPtr;
            TLLOG4(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_LOW,"This is new head %d",
                        idx % pwBaReorder->winSize));
         }
         else
         {
            TLLOG4(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_LOW,"There is bufered Just add %d",
                        idx % pwBaReorder->winSize));
            vos_pkt_chain_packet(*vosDataBuff,
                                 (vos_pkt_t *)currentDataPtr,
                                 VOS_TRUE);
         }
         pwBaReorder->reorderBuffer->arrayBuffer[idx  % pwBaReorder->winSize]
                                                                       = NULL;
         pwBaReorder->pendingFramesCount--;
         negDetect--;
         if(negDetect < 0)
         {
            TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"This is not possible, some balance has problem\n"));
            VOS_ASSERT(0);
            return VOS_STATUS_E_FAULT;
         }
         TLLOG4(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_LOW,"Slot Index %d, set as NULL, Pending Frames %d",
                     idx  % pwBaReorder->winSize,
                     pwBaReorder->pendingFramesCount
                     ));
         pwBaReorder->ucCIndex = (idx + 1) % pwBaReorder->winSize;
      }
      else
      {
         TLLOG4(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_LOW,"Empty Array %d",
                     idx % pwBaReorder->winSize));
      }
      TLLOG4(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_LOW,"Current Index %d, winSize %d",
                  pwBaReorder->ucCIndex,
                  pwBaReorder->winSize
                  ));
   }

#ifdef WLANTL_REORDER_DEBUG_MSG_ENABLE
   end = idx2;

   TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Fwd 0x%08X-%08X opCode %d fwdIdx %d windowSize %d pending frame %d fw no. %d from idx %d to %d",
                     frameIdx[1], frameIdx[0], opCode, ffidx, pwBaReorder->winSize, pending, pending - negDetect, start, end));
#endif

   return status; 
}/*WLANTL_ChainFrontPkts*/
/*==========================================================================
 
  FUNCTION    WLANTL_FillReplayCounter
 
  DESCRIPTION 
    It will fill repaly counter at a given slot index in the MSDU reordering list. 
            
  DEPENDENCIES 
                    
  PARAMETERS 

  IN
      pwBaReorder  :   pointer to the BA reordering session info 
      replayCounter:   replay counter to be filled
      ucSlotIndex  :   slot index 
                                      
  RETURN VALUE
     NONE 

                                                 
  SIDE EFFECTS 
     NONE
        
 ============================================================================*/
void WLANTL_FillReplayCounter
(
   WLANTL_BAReorderType*  pwBaReorder,
   v_U64_t                ullreplayCounter,
   v_U8_t                 ucSlotIndex
)
{

   //BAMSGDEBUG("replay counter to be filled in Qed frames %llu",
               //replayCounter, 0, 0);

   pwBaReorder->reorderBuffer->ullReplayCounter[ucSlotIndex] = ullreplayCounter;
   //BAMSGDEBUG("Assigned, replay counter Pending Frames %d at slot %d, replay counter[0x%llX]\n",
               //pwBaReorder->pendingFramesCount,
               //ucSlotIndex,
               //pwBaReorder->reorderBuffer->ullReplayCounter);
   return;
}/*WLANTL_FillReplayCounter*/

