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

                       W L A N _ Q C T _ W D I _ S T A . C

  OVERVIEW:

  This software unit holds the implementation of the WLAN Device Abstraction     
  Layer Station Table Management Entity.

  The functions externalized by this module are internal APIs for DAL Core
  and can only be called by it. 

  DEPENDENCIES:

  Are listed for each API below.


  Copyright (c) 2008 QUALCOMM Incorporated.
  All Rights Reserved.
  Qualcomm Confidential and Proprietary
===========================================================================*/

/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


   $Header$$DateTime$$Author$


  when        who     what, where, why
----------    ---    --------------------------------------------------------
2010-08-09    lti     Created module

===========================================================================*/

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
#include "wlan_qct_wdi.h" 
#include "wlan_qct_wdi_i.h" 
#include "wlan_qct_wdi_sta.h" 
#include "wlan_qct_pal_api.h"
#include "wlan_qct_pal_trace.h"


/*----------------------------------------------------------------------------
 * Function definition
 * -------------------------------------------------------------------------*/
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
)
{
    wpt_uint8  ucMaxStations;
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/    

    ucMaxStations     = (wpt_uint8) pWDICtx->ucMaxStations;
    
    /*----------------------------------------------------------------------
       Allocate the memory for sta table
    ------------------------------------------------------------------------*/
    pWDICtx->staTable = wpalMemoryAllocate(ucMaxStations * sizeof(WDI_StaStruct));

    if (NULL == pWDICtx->staTable)
    {
            
        WDI_STATableClose(pWDICtx);

        WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "Error allocating memory on WDI_STATableInit"); 
        return WDI_STATUS_E_FAILURE;
    }
    
    wpalMemoryZero( pWDICtx->staTable, ucMaxStations * sizeof( WDI_StaStruct ));

#ifndef HAL_SELF_STA_PER_BSS
    // Initialize the Self STAID to an invalid value
    pWDICtx->ucSelfStaId = WDI_STA_INVALID_IDX;
#endif

    return WDI_STATUS_SUCCESS;
}/*WDI_STATableInit*/

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
)
{
    wpt_uint8 ucMaxStations;
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    
    ucMaxStations     = (wpt_uint8) pWDICtx->ucMaxStations;
 
    return WDI_STATUS_SUCCESS;
}/*WDI_STATableStart*/

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
)
{
    wpt_uint8 ucMaxStations;
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

#ifndef HAL_SELF_STA_PER_BSS
    /* Clean up the Self STAID */
    pWDICtx->ucSelfStaId = WDI_STA_INVALID_IDX;
#endif

    ucMaxStations     = pWDICtx->ucMaxStations;
    
    wpalMemoryZero( (void *) pWDICtx->staTable,
            ucMaxStations * sizeof( WDI_StaStruct ));

    return WDI_STATUS_SUCCESS;
}/*WDI_STATableStop*/

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
)
{
    WDI_Status status = WDI_STATUS_SUCCESS;
        
    // Free memory
    if (pWDICtx->staTable != NULL)
        wpalMemoryFree( pWDICtx->staTable);

    pWDICtx->staTable = NULL;
    return status;
}/*WDI_STATableClose*/


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
)
{
    wpt_uint8        ucSTAIdx  = 0;
    WDI_StaStruct*   pSTATable = (WDI_StaStruct*) pWDICtx->staTable;
    /*- - - -  - - - - - - - - - - - -  - - - - - - - - - - - -  - - - - - */

    /*-----------------------------------------------------------------------
      Sanity check
      - station ids are allocated by the HAL located on RIVA SS - they must
      always be valid 
    -----------------------------------------------------------------------*/
    if (( pwdiParam->ucSTAIdx  == WDI_STA_INVALID_IDX) ||
        ( pwdiParam->ucSTAIdx >= pWDICtx->ucMaxStations ))
    {
      WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
                "Station id sent by HAL is invalid - not OK"); 
      WDI_ASSERT(0); 
      return WDI_STATUS_E_FAILURE; 
    }
    
    ucSTAIdx =  pwdiParam->ucSTAIdx;

    /*Since we are not the allocator of STA Ids but HAL is - just set flag to
      valid*/
    pSTATable[ucSTAIdx].valid = 1;     
    
    
    // Save the STA type - this is used for lookup
    WDI_STATableSetStaType(pWDICtx, ucSTAIdx, pwdiParam->ucStaType);
    WDI_STATableSetStaQosEnabled(pWDICtx, ucSTAIdx, 
          (wpt_uint8)(pwdiParam->ucWmmEnabled | pwdiParam->ucHTCapable) );

#ifdef WLAN_PERF
    pWDICtx->uBdSigSerialNum ++;
#endif
    
    wpalMemoryCopy(pSTATable[ucSTAIdx].macBSSID, 
                   pwdiParam->macBSSID, WDI_MAC_ADDR_LEN);

    /*------------------------------------------------------------------------
      Set DPU Related Information 
    ------------------------------------------------------------------------*/
    pSTATable[ucSTAIdx].dpuIndex              = pwdiParam->dpuIndex; 
    pSTATable[ucSTAIdx].dpuSig                = pwdiParam->dpuSig; 

    pSTATable[ucSTAIdx].bcastDpuIndex         = pwdiParam->bcastDpuIndex; 
    pSTATable[ucSTAIdx].bcastDpuSignature     = pwdiParam->bcastDpuSignature; 

    pSTATable[ucSTAIdx].bcastMgmtDpuIndex     = pwdiParam->bcastMgmtDpuIndex; 
    pSTATable[ucSTAIdx].bcastMgmtDpuSignature = pwdiParam->bcastMgmtDpuSignature; 

    /*Robust Mgmt Frame enabled */
    pSTATable[ucSTAIdx].rmfEnabled            = pwdiParam->ucRmfEnabled;

    pSTATable[ucSTAIdx].bssIdx                = pwdiParam->ucBSSIdx;

    /* Now update the STA entry with the new MAC address */
    if(WDI_STATUS_SUCCESS != WDI_STATableSetStaAddr( pWDICtx, 
                                                     ucSTAIdx, 
                                                     pwdiParam->staMacAddr))
    {
       WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
                 "Failed to update station entry - internal failure");
       WDI_ASSERT(0);
       return WDI_STATUS_E_FAILURE; 
    }

    /* Now update the STA entry with the new BSSID address */
    if(WDI_STATUS_SUCCESS != WDI_STATableSetBSSID( pWDICtx, 
                                                     ucSTAIdx, 
                                                     pwdiParam->macBSSID))
    {
       WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
                 "Failed to update station entry - internal failure");
       WDI_ASSERT(0);
       return WDI_STATUS_E_FAILURE; 
    }

    return WDI_STATUS_SUCCESS;
}/*WDI_AddSta*/

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
)
{
    WDI_StaStruct*   pSTATable = (WDI_StaStruct*) pWDICtx->staTable;
    /*- - - -  - - - - - - - - - - - -  - - - - - - - - - - - -  - - - - - */

    /*-----------------------------------------------------------------------
      Sanity check
      - station ids are allocated by the HAL located on RIVA SS - they must
      always be valid 
    -----------------------------------------------------------------------*/
    if(( ucSTAIdx  == WDI_STA_INVALID_IDX )||
        ( ucSTAIdx >= pWDICtx->ucMaxStations ))
    {
       WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
                 "STA Id invalid on Del STA - internal failure");
       WDI_ASSERT(0);
       return WDI_STATUS_E_FAILURE; 
    }
    
    wpalMemoryZero(&pSTATable[ucSTAIdx], sizeof(pSTATable[ucSTAIdx])); 
    pSTATable->valid = 0; 
    return WDI_STATUS_SUCCESS;
}/*WDI_STATableDelSta*/

/**
 @brief WDI_STATableBSSDelSta - Function to Delete Stations in this BSS

 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         ucBSSIdx:        BSS index 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_STATableBSSDelSta
(
    WDI_ControlBlockType*  pWDICtx,
    wpt_uint8              ucBSSIdx
)
{
    WDI_StaStruct*   pSTATable = (WDI_StaStruct*) pWDICtx->staTable;
    wpt_uint8        ucSTAIdx;
    /*- - - -  - - - - - - - - - - - -  - - - - - - - - - - - -  - - - - - */

    for (ucSTAIdx = 0; (ucSTAIdx < pWDICtx->ucMaxStations); ucSTAIdx++)
    {
        if( (pSTATable[ucSTAIdx].ucStaType == WDI_STA_ENTRY_PEER) && 
                                 (pSTATable[ucSTAIdx].bssIdx == ucBSSIdx))
        {
            WDI_STATableDelSta(pWDICtx, ucSTAIdx);
        }
    }

    return WDI_STATUS_SUCCESS;
}/*WDI_STATableBSSDelSta*/


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
)
{
  WDI_StaStruct* pSTATable = (WDI_StaStruct*) pWDICtx->staTable;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  if ((ucSTAIdx < pWDICtx->ucMaxStations) && (pSTATable[ucSTAIdx].valid))
  {
     wpalMemoryCopy(*pmacBSSID, pSTATable[ucSTAIdx].macBSSID, WDI_MAC_ADDR_LEN);
     return WDI_STATUS_SUCCESS;
  }
  else
     return WDI_STATUS_E_FAILURE;
}/*WDI_STATableGetStaQosEnabled*/


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
)
{
  WDI_StaStruct* pSTATable = (WDI_StaStruct*) pWDICtx->staTable;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  if ((ucSTAIdx < pWDICtx->ucMaxStations) && (pSTATable[ucSTAIdx].valid) && qosEnabled)
  {
     *qosEnabled = pSTATable[ucSTAIdx].qosEnabled;
     return WDI_STATUS_SUCCESS;
  }
  else
     return WDI_STATUS_E_FAILURE;
}/*WDI_STATableGetStaQosEnabled*/

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
)
{
    WDI_StaStruct* pSTATable = (WDI_StaStruct*) pWDICtx->staTable;
    if ((ucSTAIdx < pWDICtx->ucMaxStations) && (pSTATable[ucSTAIdx].valid))
    {
        pSTATable[ucSTAIdx].qosEnabled = qosEnabled;
        return WDI_STATUS_SUCCESS;
    }
    else
        return WDI_STATUS_E_FAILURE;
}/*WDI_STATableSetStaQosEnabled*/

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
)
{
    WDI_StaStruct* pSTATable = (WDI_StaStruct*) pWDICtx->staTable;
    if ((ucSTAIdx < pWDICtx->ucMaxStations) && (pSTATable[ucSTAIdx].valid))
    {
        *pStaType = pSTATable[ucSTAIdx].ucStaType;
        return WDI_STATUS_SUCCESS;
    }
    else
        return WDI_STATUS_E_FAILURE;
}/*WDI_STATableGetStaType*/

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
)
{
    WDI_StaStruct* pSTATable = (WDI_StaStruct*) pWDICtx->staTable;
    if ((ucSTAIdx < pWDICtx->ucMaxStations) && (pSTATable[ucSTAIdx].valid))
    {
        pSTATable[ucSTAIdx].ucStaType = staType;
        return WDI_STATUS_SUCCESS;
    }
    else
        return WDI_STATUS_E_FAILURE;
}/*WDI_STATableSetStaType*/


/**
 @brief WDI_CompareMacAddr - compare the MAC address

 
 @param  addr1: address 1 
         addr2: address 2  
  
 @see
 @return Result of the function call
*/
WPT_STATIC WPT_INLINE wpt_uint8
WDI_CompareMacAddr
(
  wpt_uint8 addr1[], 
  wpt_uint8 addr2[]
)
{
#if defined( _X86_ )
    wpt_uint32 align = (0x3 & ((wpt_uint32) addr1 | (wpt_uint32) addr2 ));

    if( align ==0){
        return ((*((wpt_uint16 *) &(addr1[4])) == *((wpt_uint16 *) &(addr2[4])))&&
                (*((wpt_uint32 *) addr1) == *((wpt_uint32 *) addr2)));
    }else if(align == 2){
        return ((*((wpt_uint16 *) &addr1[4]) == *((wpt_uint16 *) &addr2[4])) &&
            (*((wpt_uint16 *) &addr1[2]) == *((wpt_uint16 *) &addr2[2])) &&
            (*((wpt_uint16 *) &addr1[0]) == *((wpt_uint16 *) &addr2[0])));
    }else{
        return ( (addr1[5]==addr2[5])&&
            (addr1[4]==addr2[4])&&
            (addr1[3]==addr2[3])&&
            (addr1[2]==addr2[2])&&
            (addr1[1]==addr2[1])&&
            (addr1[0]==addr2[0]));
    }
#else
         return ( (addr1[0]==addr2[0])&&
            (addr1[1]==addr2[1])&&
            (addr1[2]==addr2[2])&&
            (addr1[3]==addr2[3])&&
            (addr1[4]==addr2[4])&&
            (addr1[5]==addr2[5]));
#endif
}/*WDI_CompareMacAddr*/


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
)
{
    WDI_Status wdiStatus = WDI_STATUS_E_FAILURE;
    wpt_uint8 i;
    WDI_StaStruct* pSTATable = (WDI_StaStruct*) pWDICtx->staTable;

    for (i=0; i < pWDICtx->ucMaxStations; i++, pSTATable++)
    {
        if ( (pSTATable->valid == 1) && (WDI_CompareMacAddr(pSTATable->staAddr, staAddr)) )
        {
            *pucStaId = i;
            wdiStatus = WDI_STATUS_SUCCESS;
            break;
        }
    }
    return wdiStatus;
}/*WDI_STATableFindStaidByAddr*/

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
)
{
    WDI_StaStruct* pSTATable = (WDI_StaStruct*) pWDICtx->staTable;
    if ((ucSTAIdx < pWDICtx->ucMaxStations) && (pSTATable[ucSTAIdx].valid))
    {
        *pStaAddr = pSTATable[ucSTAIdx].staAddr;
        return WDI_STATUS_SUCCESS;
    }
    else
        return WDI_STATUS_E_FAILURE;
}/*WDI_STATableGetStaAddr*/

/**
 @brief WDI_STATableGetStaMacAddr - get station MAC address

 @param  pWDICtx:  WDI Context pointer
         ucSTAIdx:  station index
         pStaAddr: output station MAC address

 @see
 @return Result of the function call
*/
WDI_Status
WDI_STATableGetStaMacAddr
(
    WDI_ControlBlockType*  pWDICtx,
    wpt_uint8              ucSTAIdx,
    wpt_macAddr*           staMacAddr
)
{
    WDI_StaStruct* pSTATable = (WDI_StaStruct*) pWDICtx->staTable;
    if ((ucSTAIdx < pWDICtx->ucMaxStations) && (pSTATable[ucSTAIdx].valid))
    {
        wpalMemoryCopy(staMacAddr, pSTATable[ucSTAIdx].staAddr,
                WDI_MAC_ADDR_LEN);
        return WDI_STATUS_SUCCESS;
    }
    else
        return WDI_STATUS_E_FAILURE;
}/*WDI_STATableGetStaMacAddr*/

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
)
{
    WDI_StaStruct* pSTATable = (WDI_StaStruct*) pWDICtx->staTable;
    if ((ucSTAIdx < pWDICtx->ucMaxStations) && (pSTATable[ucSTAIdx].valid))
    {
        wpalMemoryCopy (pSTATable[ucSTAIdx].staAddr, staAddr, 6);
        return WDI_STATUS_SUCCESS;
    }
    else
        return WDI_STATUS_E_FAILURE;
}/*WDI_STATableSetStaAddr*/

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
)
{
    WDI_StaStruct* pSTATable = (WDI_StaStruct*) pWDICtx->staTable;
    if ((ucSTAIdx < pWDICtx->ucMaxStations) && (pSTATable[ucSTAIdx].valid))
    {
        wpalMemoryCopy (pSTATable[ucSTAIdx].macBSSID, macBSSID, 6);
        return WDI_STATUS_SUCCESS;
    }
    else
        return WDI_STATUS_E_FAILURE;
}/*WDI_STATableSetBSSID*/

/**
 @brief WDI_STATableSetBSSIdx - set station corresponding BSS index
 
 @param  pWDICtx:  WDI Context pointer
         ucSTAIdx:   station index
         ucBSSIdx:   BSS index
  
 @see
 @return Result of the function call
*/
WDI_Status 
WDI_STATableSetBSSIdx
(
    WDI_ControlBlockType*  pWDICtx,  
    wpt_uint8              ucSTAIdx, 
    wpt_uint8              ucBSSIdx
)
{
    WDI_StaStruct* pSTATable = (WDI_StaStruct*) pWDICtx->staTable;
    if ((ucSTAIdx < pWDICtx->ucMaxStations) && (pSTATable[ucSTAIdx].valid))
    {
        pSTATable[ucSTAIdx].bssIdx = ucBSSIdx;
        return WDI_STATUS_SUCCESS;
    }
    else
        return WDI_STATUS_E_FAILURE;
}/*WDI_STATableSetBSSIdx*/

