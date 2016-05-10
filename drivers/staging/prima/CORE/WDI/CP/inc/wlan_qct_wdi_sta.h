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

#ifndef WLAN_QCT_DAL_STA_H
#define WLAN_QCT_DAL_STA_H

/*===========================================================================

         W L A N   D E V I C E   A B S T R A C T I O N   L A Y E R 
              I N T E R N A L     A P I       F O R    T H E
                       S T A T I O N    M G M T
                
                   
DESCRIPTION
  This file contains the internal API exposed by the STA Management entity to
  be used by the DAL Control Path Core . 
  
      
  Copyright (c) 2010 QUALCOMM Incorporated. All Rights Reserved.
  Qualcomm Confidential and Proprietary
===========================================================================*/


/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


  $Header:$ $DateTime: $ $Author: $


when        who    what, where, why
--------    ---    ----------------------------------------------------------
08/19/10    lti     Created module.

===========================================================================*/

#include "wlan_qct_pal_type.h"
#include "wlan_qct_pal_api.h"

/*----------------------------------------------------------------------------
     Preprocesor definitions and macros 
  -------------------------------------------------------------------------*/
/*Invalid station index */
#define WDI_STA_INVALID_IDX 0xFF

/*----------------------------------------------------------------------------
  WDI_AddStaParams
  -------------------------------------------------------------------------*/
typedef struct 
{
  wpt_uint8    ucSTAIdx; 
  wpt_uint8    ucWmmEnabled;
  wpt_uint8    ucHTCapable; 

  /* MAC Address of STA */
  wpt_macAddr staMacAddr;

  /*MAC Address of the BSS*/
  wpt_macAddr  macBSSID;

  /* Field to indicate if this is sta entry for itself STA adding entry for itself
     or remote (AP adding STA after successful association.
     This may or may not be required in production driver.
     0 - Self, 1 other/remote, 2 - bssid */
  wpt_uint8   ucStaType;       


  /*DPU Information*/
  wpt_uint8   dpuIndex;                      // DPU table index
  wpt_uint8   dpuSig;                        // DPU signature
  wpt_uint8   bcastDpuIndex;
  wpt_uint8   bcastDpuSignature;
  wpt_uint8   bcastMgmtDpuIndex;
  wpt_uint8   bcastMgmtDpuSignature;


  /*RMF enabled/disabled*/
  wpt_uint8   ucRmfEnabled;

  /* Index into the BSS Session table */
  wpt_uint8   ucBSSIdx;

}WDI_AddStaParams; 

/*----------------------------------------------------------------------------
  WDI_StaStruct
  -------------------------------------------------------------------------*/
typedef struct
{
  wpt_macAddr staAddr;                // Sta Addr
     
  wpt_uint8 valid:1;                           // Used/free flag    
  wpt_uint8 rmfEnabled:1;
  wpt_uint8 htEnabled:1;

  /* 11e or WMM enabled, flag used for header length*/
  wpt_uint8 qosEnabled:1;         

  wpt_uint8 bssIdx;                         // BSS Index
  wpt_uint8 staId;

  wpt_macAddr macBSSID;
  // Field to indicate if this is sta entry for itself STA adding entry for itself
  // or remote (AP adding STA after successful association.
  // This may or may not be required in production driver.
  // 0 - Self, 1 other/remote, 2 - bssid
  wpt_uint8   ucStaType;       


  /*DPU Information*/
  wpt_uint8 dpuIndex;                      // DPU table index
  wpt_uint8 dpuSig;                        // DPU signature
  wpt_uint8 bcastDpuIndex;
  wpt_uint8 bcastDpuSignature;
  wpt_uint8 bcastMgmtDpuIndex;
  wpt_uint8 bcastMgmtDpuSignature;
     
} WDI_StaStruct;

/**
 @brief WDI_STATableInit - Initializes the STA tables. 
        Allocates the necesary memory.

 
 @param  pWDICtx:         pointer to the WLAN DAL context 
  
 @see
 @return Result of the function call
*/
WDI_Status WDI_STATableInit
(
   WDI_ControlBlockType*  pWDICtx
);

/**
 @brief WDI_STATableStart - resets the max and number values of 
        STAtions

 
 @param  pWDICtx:         pointer to the WLAN DAL context 
  
 @see
 @return Result of the function call
*/
WDI_Status 
WDI_STATableStart
(
    WDI_ControlBlockType*  pWDICtx
);

/**
 @brief WDI_STATableStop - clears the sta table

 
 @param  pWDICtx:         pointer to the WLAN DAL context 
  
 @see
 @return Result of the function call
*/
WDI_Status 
WDI_STATableStop
(
    WDI_ControlBlockType*  pWDICtx
);

/**
 @brief WDI_STATableClose - frees the resources used by the STA 
        table.

 
 @param  pWDICtx:         pointer to the WLAN DAL context 
  
 @see
 @return Result of the function call
*/
WDI_Status 
WDI_STATableClose
(
  WDI_ControlBlockType*  pWDICtx
);


/**
 @brief WDI_STATableAddSta - Function to Add Station

 
 @param  pWDICtx:     pointer to the WLAN DAL context 
         pwdiParam:   station parameters  
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_STATableAddSta
(
    WDI_ControlBlockType*  pWDICtx,
    WDI_AddStaParams*      pwdiParam
);

/**
 @brief WDI_STATableDelSta - Function to Delete a Station

 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         ucSTAIdx:        station to be deleted
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_STATableDelSta
(
    WDI_ControlBlockType*  pWDICtx,
    wpt_uint8              ucSTAIdx
);

/**
 @brief WDI_STATableBSSDelSta - Function to Delete Stations in this BSS

 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         bssIdx:        BSS index 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_STATableBSSDelSta
(
    WDI_ControlBlockType*  pWDICtx,
    wpt_uint8              ucBssIdx
);

/**
 @brief WDI_STATableGetStaBSSIDAddr - Gets the BSSID associated 
        with this station

 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         ucSTAIdx:        station index
         pmacBSSID:      out BSSID for this STA
  
 @see
 @return Result of the function call
*/
WDI_Status 
WDI_STATableGetStaBSSIDAddr
(
    WDI_ControlBlockType*  pWDICtx,  
    wpt_uint8              ucSTAIdx, 
    wpt_macAddr*           pmacBSSID
);
/**
 @brief WDI_STATableGetStaQosEnabled - Gets is qos is enabled 
        for a sta

 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         ucSTAIdx:        station index
         qosEnabled:      out qos enabled
  
 @see
 @return Result of the function call
*/
WDI_Status 
WDI_STATableGetStaQosEnabled
(
    WDI_ControlBlockType*  pWDICtx,  
    wpt_uint8              ucSTAIdx, 
    wpt_uint8*             qosEnabled
);

/**
 @brief WDI_STATableSetStaQosEnabled - set qos mode for STA

 
 @param  pWDICtx:    pointer to the WLAN DAL context 
         ucSTAIdx:   station index
         qosEnabled: qos enabled
  
 @see
 @return Result of the function call
*/
WDI_Status 
WDI_STATableSetStaQosEnabled
(
    WDI_ControlBlockType*  pWDICtx,  
    wpt_uint8              ucSTAIdx, 
    wpt_uint8              qosEnabled
);

/**
 @brief WDI_STATableGetStaType - get sta type for STA

 
 @param  pWDICtx:   pointer to the WLAN DAL context 
         ucSTAIdx:  station index
         pStaType:  qos enabled
  
 @see
 @return Result of the function call
*/
WDI_Status 
WDI_STATableGetStaType
(
    WDI_ControlBlockType*  pWDICtx,  
    wpt_uint8              ucSTAIdx, 
    wpt_uint8*             pStaType
);

/**
 @brief WDI_STATableSetStaType - sets sta type for STA

 
 @param  pWDICtx:   pointer to the WLAN DAL context 
         ucSTAIdx:  station index
         staType:   sta type
  
 @see
 @return Result of the function call
*/
WDI_Status 
WDI_STATableSetStaType
(
    WDI_ControlBlockType*  pWDICtx,  
    wpt_uint8              ucSTAIdx, 
    wpt_uint8              staType
);


/**
 @brief WDI_STATableFindStaidByAddr - Given a station mac address, search
        for the corresponding station index from the Station Table.
 
 @param  pWDICtx:  WDI Context pointer
         staAddr:  station address
         pucStaId: output station id 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_STATableFindStaidByAddr
(
    WDI_ControlBlockType*  pWDICtx, 
    wpt_macAddr            staAddr, 
    wpt_uint8*             pucStaId
);

/**
 @brief WDI_STATableGetStaAddr - get station address
 
 @param  pWDICtx:  WDI Context pointer
         ucSTAIdx:  station index
         pStaAddr: output station address 
  
 @see
 @return Result of the function call
*/
WDI_Status 
WDI_STATableGetStaAddr
(
    WDI_ControlBlockType*  pWDICtx,  
    wpt_uint8              ucSTAIdx, 
    wpt_uint8**            pStaAddr
);

/**
 @brief WDI_STATableSetStaAddr - set station address
 
 @param  pWDICtx:  WDI Context pointer
         ucSTAIdx:   station index
         pStaAddr: output station address 
  
 @see
 @return Result of the function call
*/
WDI_Status 
WDI_STATableSetStaAddr
(
    WDI_ControlBlockType*  pWDICtx,  
    wpt_uint8              ucSTAIdx, 
    wpt_macAddr            staAddr
);

/**
 @brief WDI_STATableSetBSSID - set station corresponding BSSID
 
 @param  pWDICtx:  WDI Context pointer
         ucSTAIdx:   station index
         pStaAddr: output station address 
  
 @see
 @return Result of the function call
*/
WDI_Status 
WDI_STATableSetBSSID
(
    WDI_ControlBlockType*  pWDICtx,  
    wpt_uint8              ucSTAIdx, 
    wpt_macAddr            macBSSID
);

/**
 @brief WDI_STATableSetBSSIdx - set station corresponding BSS index
 
 @param  pWDICtx:  WDI Context pointer
         ucSTAIdx:   station index
         bssIdx:   BSS index 
  
 @see
 @return Result of the function call
*/
WDI_Status 
WDI_STATableSetBSSIdx
(
    WDI_ControlBlockType*  pWDICtx,  
    wpt_uint8              ucSTAIdx, 
    wpt_uint8              ucBSSIdx
);

#endif /*WLAN_QCT_WDI_STA_H*/

