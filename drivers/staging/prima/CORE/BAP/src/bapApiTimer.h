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

#ifndef WLAN_QCT_WLANBAP_API_TIMER_H
#define WLAN_QCT_WLANBAP_API_TIMER_H

/*===========================================================================

               W L A N   B T - A M P  P A L   L A Y E R 
                    T I M E R  S E R V I C E S  A P I
                
                   
DESCRIPTION
  This file contains the timer APIs used by the wlan BT-AMP PAL layer 
  module.
  
      
  Copyright (c) 2008 QUALCOMM Incorporated. All Rights Reserved.
  Qualcomm Confidential and Proprietary
===========================================================================*/


/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


  $Header: /cygdrive/e/Builds/M7201JSDCAAPAD52240B/WM/platform/msm7200/Src/Drivers/SD/ClientDrivers/WLAN/QCT/CORE/BAP/src/bapApiTimer.h,v 1.1 2008/11/21 20:30:20 jzmuda Exp jzmuda $ $DateTime: $ $Author: jzmuda $


when        who    what, where, why
--------    ---    ----------------------------------------------------------
10/23/08    jez     Created module.

===========================================================================*/



/*===========================================================================

                          INCLUDE FILES FOR MODULE

===========================================================================*/

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
// Pick up all the BT-AMP internal definitions 
// And underlying supporting types. (Including VOSS, CSR, and...)
#include "bapInternal.h" 

/* Pick up the SIRIUS and HAL types */ 
// Already taken care of, above 
//#include "sirApi.h"
//#include "halTypes.h"

/* Pick up the CCM API def'n */ 
#include "ccmApi.h"

/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/
 #ifdef __cplusplus
 extern "C" {
 #endif 
 

/*----------------------------------------------------------------------------
 *  Defines
 * -------------------------------------------------------------------------*/
// Temporary 
//#define BAP_DEBUG


/*----------------------------------------------------------------------------
 *  Typedefs
 * -------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
 *  External declarations for global context 
 * -------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
 *  Function prototypes 
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 *  Utility Function prototypes 
 * -------------------------------------------------------------------------*/

#if 0
/*==========================================================================

  FUNCTION    WLANBAP_StartConnectionAcceptTimer

  DESCRIPTION 
    Clear out all fields in the BAP context.
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    pBtampCtx:   pointer to the BAP control block
    interval:    time interval.
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  access would cause a page fault  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS 
WLANBAP_StartConnectionAcceptTimer
( 
  ptBtampContext  pBtampCtx,
  v_U32_t interval
);
#endif // 0

/* Connection Accept timer*/
VOS_STATUS WLANBAP_InitConnectionAcceptTimer 
    ( ptBtampContext  pBtampCtx);

VOS_STATUS WLANBAP_DeinitConnectionAcceptTimer
    ( ptBtampContext  pBtampCtx);

VOS_STATUS WLANBAP_StartConnectionAcceptTimer 
    (ptBtampContext  pBtampCtx, v_U32_t interval);

VOS_STATUS WLANBAP_StopConnectionAcceptTimer 
    ( ptBtampContext  pBtampCtx);

v_VOID_t WLANBAP_ConnectionAcceptTimerHandler 
    ( v_PVOID_t userData );

/* Link Supervision timer*/
VOS_STATUS WLANBAP_InitLinkSupervisionTimer 
    ( ptBtampContext  pBtampCtx);

VOS_STATUS WLANBAP_DeinitLinkSupervisionTimer 
    ( ptBtampContext  pBtampCtx);

VOS_STATUS WLANBAP_StartLinkSupervisionTimer 
    (ptBtampContext  pBtampCtx, v_U32_t interval);

VOS_STATUS WLANBAP_StopLinkSupervisionTimer 
    ( ptBtampContext  pBtampCtx);

v_VOID_t WLANBAP_LinkSupervisionTimerHandler 
    ( v_PVOID_t userData );

/* Logical Link Accept timer*/
VOS_STATUS WLANBAP_InitLogicalLinkAcceptTimer 
    ( ptBtampContext  pBtampCtx);

VOS_STATUS WLANBAP_DeinitLogicalLinkAcceptTimer 
    ( ptBtampContext  pBtampCtx);

VOS_STATUS WLANBAP_StartLogicalLinkAcceptTimer 
    (ptBtampContext  pBtampCtx, v_U32_t interval);

VOS_STATUS WLANBAP_StopLogicalLinkAcceptTimer 
    ( ptBtampContext  pBtampCtx);

v_VOID_t WLANBAP_LogicalLinkAcceptTimerHandler 
    ( v_PVOID_t userData );

/* Best Effort Flush timer*/
VOS_STATUS WLANBAP_InitBEFlushTimer 
    ( ptBtampContext  pBtampCtx);

VOS_STATUS WLANBAP_DeinitBEFlushTimer 
    ( ptBtampContext  pBtampCtx);

VOS_STATUS WLANBAP_StartBEFlushTimer 
    (ptBtampContext  pBtampCtx, v_U32_t interval);

VOS_STATUS WLANBAP_StopBEFlushTimer 
    ( ptBtampContext  pBtampCtx);

v_VOID_t WLANBAP_BEFlushTimerHandler 
    ( v_PVOID_t userData );

/* Tx Packet monitor timer handler */
v_VOID_t 
WLANBAP_TxPacketMonitorHandler
( 
  v_PVOID_t userData 
);

/* Tx Packet monitor start timer */
VOS_STATUS 
WLANBAP_StartTxPacketMonitorTimer
( 
  ptBtampContext  pBtampCtx
);

/* Tx Packet monitor stop timer */
VOS_STATUS 
WLANBAP_StopTxPacketMonitorTimer 
( 
  ptBtampContext  pBtampCtx
);

#ifdef __cplusplus
 }
#endif 


#endif /* #ifndef WLAN_QCT_WLANBAP_API_TIMER_H */

