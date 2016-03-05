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

#ifndef WLAN_QCT_TLI_BA_H
#define WLAN_QCT_TLI_BA_H

/*===========================================================================

               W L A N   T R A N S P O R T   L A Y E R 
               B L O C K   A C K    I N T E R N A L  A P I
                
                   
DESCRIPTION
  This file contains the internal declarations used within wlan transport 
  layer module for BA session support, AMSDU de-aggregation and 
  MSDU reordering.
        
  Copyright (c) 2008 QUALCOMM Incorporated. All Rights Reserved.
  Qualcomm Confidential and Proprietary
===========================================================================*/


/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


  $Header:$ $DateTime: $ $Author: $


when        who    what, where, why
--------    ---    ----------------------------------------------------------
08/22/08    sch     Update based on unit test
07/31/08    lti     Created module.

===========================================================================*/



/*===========================================================================

                          INCLUDE FILES FOR MODULE

===========================================================================*/

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
#include "wlan_qct_tli.h" 

/*---------------------------------------------------------------------------
   Re-order opcode filled in by RPE 
   !!! fix me: (check with RPE doc if the codes are correct)
 ---------------------------------------------------------------------------*/
typedef enum
{
  WLANTL_OPCODE_INVALID         = 0,
  WLANTL_OPCODE_QCUR_FWDBUF     = 1,
  WLANTL_OPCODE_FWDBUF_FWDCUR   = 2,
  WLANTL_OPCODE_QCUR            = 3,
  WLANTL_OPCODE_FWDBUF_QUEUECUR = 4,
  WLANTL_OPCODE_FWDBUF_DROPCUR  = 5,
  WLANTL_OPCODE_FWDALL_DROPCUR  = 6,
  WLANTL_OPCODE_FWDALL_QCUR     = 7,
  WLANTL_OPCODE_TEARDOWN        = 8,
  WLANTL_OPCODE_DROPCUR         = 9,
  WLANTL_OPCODE_MAX
}WLANTL_OpCodeEnumType;

void WLANTL_InitBAReorderBuffer
(
   v_PVOID_t   pvosGCtx
);

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
);

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
  v_PVOID_t           pvosGCtx, 
  v_U16_t             ucSTAId,
  v_U8_t              ucTid
);

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
   pAdapter:     pointer to the global adapter context; a handle to TL's 
                 control block can be extracted from its context 
   pvBDHeader:   pointer to the BD header
   ucSTAId:      STAtion ID 
      
  RETURN VALUE
    The result code associated with performing the operation  

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
);

/*==========================================================================
  FUNCTION    WLANTL_MSDUReorder

  DESCRIPTION 
    MSDU reordering 

  DEPENDENCIES 
         
  PARAMETERS 

   IN
   
   vosDataBuff: vos packet for the received data
   pvBDHeader: pointer to the BD header
   ucSTAId:    STAtion ID 
      
  RETURN VALUE
    The result code associated with performing the operation  

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS
WLANTL_MSDUReorder
( 
   WLANTL_CbType    *pTLCb,
   vos_pkt_t        **vosDataBuff, 
   v_PVOID_t        pvBDHeader,
   v_U8_t           ucSTAId,
   v_U8_t           ucTid
);


/*==========================================================================
    Utility functions
  ==========================================================================*/

/*==========================================================================
  FUNCTION    WLANTL_AMSDUCompleteFrame

  DESCRIPTION 
    Complete AMSDU de-aggregation

  DEPENDENCIES 
         
  PARAMETERS 

   IN/OUT:
   vosDataBuff: vos packet for the received data
   
   IN
   pvBDHeader: pointer to the BD header
   ucSTAId:    STAtion ID 
      
  RETURN VALUE
    The result code associated with performing the operation  

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS
WLANTL_AMSDUCompleteFrame
( 
  vos_pkt_t*  vosDataBuff,
  v_U8_t      ucMPDUHLen,
  v_U16_t     usMPDULen
);

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

    VOS_STATUS_E_SUCCESS:     Everything is OK

    
  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS
WLANTL_QueueCurrent
(
  WLANTL_BAReorderType*  pwBaReorder,
  vos_pkt_t**            vosDataBuff,
  v_U8_t                 ucSlotIndex
);

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

    VOS_STATUS_E_SUCCESS:     Everything is OK

    
  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS WLANTL_ChainFrontPkts
( 
   v_U32_t                fwdIndex,
   v_U8_t                 opCode,
   vos_pkt_t              **vosDataBuff,
   WLANTL_BAReorderType   *pwBaReorder,
   WLANTL_CbType          *pTLCb
);

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
   v_U64_t                replayCounter,
   v_U8_t                 ucSlotIndex
);

#endif /* #ifndef WLAN_QCT_TLI_H */
