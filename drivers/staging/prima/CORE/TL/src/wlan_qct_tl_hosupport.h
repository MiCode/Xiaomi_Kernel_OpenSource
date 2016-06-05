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

#ifndef WLAN_QCT_TL_HOSUPPORT_H
#define WLAN_QCT_TL_HOSUPPORT_H

/*===========================================================================

               W L A N   T R A N S P O R T   L A Y E R 
               HO SUPPORT    I N T E R N A L  A P I
                
                   
DESCRIPTION
        
===========================================================================*/


/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


  $Header:$ $DateTime: $ $Author: $


when        who    what, where, why
--------    ---    ----------------------------------------------------------
12/11/08      sch     Initial creation

===========================================================================*/



/*===========================================================================

                          INCLUDE FILES FOR MODULE

===========================================================================*/
#include "wlan_qct_tl.h" 

#ifdef WLAN_FEATURE_LINK_LAYER_STATS
/*==========================================================================

   FUNCTION

   DESCRIPTION

   PARAMETERS

   RETURN VALUE

============================================================================*/
VOS_STATUS WLANTL_HSGetDataRSSI
(
   v_PVOID_t        pAdapter,
   v_PVOID_t        pBDHeader,
   v_U8_t           STAid,
   v_S7_t          *currentAvgRSSI
);
#endif

#if defined WLAN_FEATURE_NEIGHBOR_ROAMING
/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
/*==========================================================================

   FUNCTION

   DESCRIPTION 
    
   PARAMETERS 

   RETURN VALUE

============================================================================*/
VOS_STATUS WLANTL_HSHandleRXFrame
(
   v_PVOID_t        pAdapter,
   v_U8_t           frameType,
   v_PVOID_t        pBDHeader,
   v_U8_t           STAid,
   v_BOOL_t         isBroadcast,
   vos_pkt_t       *dataBuffer
);

/*==========================================================================

   FUNCTION

   DESCRIPTION 
    
   PARAMETERS 

   RETURN VALUE

============================================================================*/
VOS_STATUS WLANTL_HSHandleTXFrame
(
   v_PVOID_t        pAdapter,
   v_U8_t           ac,
   v_U8_t           STAid,
   vos_pkt_t       *dataBuffer,
   v_PVOID_t        bdHeader
);

/*==========================================================================

   FUNCTION

   DESCRIPTION 
    
   PARAMETERS 

   RETURN VALUE

============================================================================*/
VOS_STATUS WLANTL_HSRegRSSIIndicationCB
(
   v_PVOID_t                       pAdapter,
   v_S7_t                          rssiValue,
   v_U8_t                          triggerEvent,
   WLANTL_RSSICrossThresholdCBType crossCBFunction,
   VOS_MODULE_ID                   moduleID,
   v_PVOID_t                       usrCtxt
);

/*==========================================================================

   FUNCTION

   DESCRIPTION 
    
   PARAMETERS 

   RETURN VALUE

============================================================================*/
VOS_STATUS WLANTL_HSDeregRSSIIndicationCB
(
   v_PVOID_t                       pAdapter,
   v_S7_t                          rssiValue,
   v_U8_t                          triggerEvent,
   WLANTL_RSSICrossThresholdCBType crossCBFunction,
   VOS_MODULE_ID                   moduleID
);

/*==========================================================================

   FUNCTION

   DESCRIPTION 
    
   PARAMETERS 

   RETURN VALUE

============================================================================*/
VOS_STATUS WLANTL_HSBMPSRSSIRegionChangedNotification
(
   v_PVOID_t             pAdapter,
   tpSirRSSINotification pRSSINotification
);

/*==========================================================================

   FUNCTION

   DESCRIPTION 
    
   PARAMETERS 

   RETURN VALUE

============================================================================*/
VOS_STATUS WLANTL_HSSetAlpha
(
   v_PVOID_t pAdapter,
   int       valueAlpha
);

/*==========================================================================

   FUNCTION

   DESCRIPTION 
    
   PARAMETERS 

   RETURN VALUE

============================================================================*/
VOS_STATUS WLANTL_HSRegGetTrafficStatus
(
   v_PVOID_t                          pAdapter,
   v_U32_t                            idleThreshold,
   v_U32_t                            period,
   WLANTL_TrafficStatusChangedCBType  trfficStatusCB,
   v_PVOID_t                          usrCtxt
);

/*==========================================================================

   FUNCTION

   DESCRIPTION 
    
   PARAMETERS 

   RETURN VALUE

============================================================================*/
VOS_STATUS WLANTL_HSInit
(
   v_PVOID_t   pAdapter
);


/*==========================================================================

   FUNCTION    WLANTL_HSDeInit

   DESCRIPTION 
    
   PARAMETERS 

   RETURN VALUE

============================================================================*/

VOS_STATUS WLANTL_HSDeInit
(
   v_PVOID_t   pAdapter
);


/*==========================================================================

   FUNCTION

   DESCRIPTION 
    
   PARAMETERS 

   RETURN VALUE

============================================================================*/
VOS_STATUS WLANTL_HSStop
(
   v_PVOID_t   pAdapter
);

VOS_STATUS WLANTL_SetFWRSSIThresholds
(
   v_PVOID_t                       pAdapter
);

/*==========================================================================

   FUNCTION

   DESCRIPTION 
    
   PARAMETERS 

   RETURN VALUE

============================================================================*/
VOS_STATUS WLANTL_HSSerializeTlIndication
(
   v_PVOID_t   pAdapter,
   v_U8_t      rssiNotification,
   v_PVOID_t   pUserCtxt,
   WLANTL_RSSICrossThresholdCBType cbFunction,
   v_U8_t      avgRssi
);

/*==========================================================================

   FUNCTION

   DESCRIPTION 
    
   PARAMETERS 

   RETURN VALUE

============================================================================*/
VOS_STATUS WLANTL_StatHandleTXFrame
(
   v_PVOID_t        pAdapter,
   v_U8_t           STAid,
   vos_pkt_t       *dataBuffer,
   v_PVOID_t        pBDHeader,
   WLANTL_MetaInfoType *txMetaInfo
);

#endif

#endif /* WLAN_QCT_TL_HOSUPPORT_H */
