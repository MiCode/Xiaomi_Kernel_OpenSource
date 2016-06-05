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
 *     
 *       \file  wlan_qct_wdi_ds.c
 *          
 *       \brief define Dataservice API 
 *                               
 * WLAN Device Abstraction layer External API for Dataservice
 * DESCRIPTION
 *  This file contains the external API implemntation exposed by the 
 *   wlan device abstarction layer module.
 *
 */


#include "wlan_qct_wdi.h"
#include "wlan_qct_wdi_i.h"
#include "wlan_qct_wdi_ds.h"
#include "wlan_qct_wdi_ds_i.h"
#include "wlan_qct_wdi_dts.h"
#include "wlan_qct_wdi_dp.h"
#include "wlan_qct_wdi_sta.h"




/* DAL registration function. 
 * Parameters:
 *  pContext:Cookie that should be passed back to the caller along 
 *  with the callback.
 *  pfnTxCompleteCallback:Callback function that is to be invoked to return 
 *  packets which have been transmitted.
 *  pfnRxPacketCallback:Callback function that is to be invoked to deliver 
 *  packets which have been received
 *  pfnTxFlowControlCallback:Callback function that is to be invoked to 
 *  indicate/clear congestion. 
 *
 * Return Value: SUCCESS  Completed successfully.
 *     FAILURE_XXX  Request was rejected due XXX Reason.
 *
 */
WDI_Status WDI_DS_Register( void *pContext,
  WDI_DS_TxCompleteCallback pfnTxCompleteCallback,
  WDI_DS_RxPacketCallback pfnRxPacketCallback,
  WDI_DS_TxFlowControlCallback pfnTxFlowControlCallback,
  WDI_DS_RxLogCallback pfnRxLogCallback,
  void *pCallbackContext)
{
  WDI_DS_ClientDataType *pClientData;
  wpt_uint8              bssLoop;

  // Do Sanity checks
  if (NULL == pContext ||
      NULL == pCallbackContext ||
      NULL == pfnTxCompleteCallback ||
      NULL == pfnRxPacketCallback ||
      NULL == pfnTxFlowControlCallback) {
    return WDI_STATUS_E_FAILURE;
  }

  pClientData = (WDI_DS_ClientDataType *)WDI_DS_GetDatapathContext(pContext);
  if (NULL == pClientData)
  {
    return WDI_STATUS_MEM_FAILURE;
  }

  // Store callbacks in client structure
  pClientData->pcontext = pContext;
  pClientData->receiveFrameCB = pfnRxPacketCallback;
  pClientData->txCompleteCB = pfnTxCompleteCallback;
  pClientData->txResourceCB = pfnTxFlowControlCallback;
  pClientData->rxLogCB = pfnRxLogCallback;
  pClientData->pCallbackContext = pCallbackContext;

  for(bssLoop = 0; bssLoop < WDI_DS_MAX_SUPPORTED_BSS; bssLoop++)
  {
    pClientData->staIdxPerBssIdxTable[bssLoop].isUsed = 0;
    pClientData->staIdxPerBssIdxTable[bssLoop].bssIdx = WDI_DS_INDEX_INVALID;
    pClientData->staIdxPerBssIdxTable[bssLoop].staIdx = WDI_DS_INDEX_INVALID;
  }
  return WDI_STATUS_SUCCESS;
}



/* DAL Transmit function.
 * Parameters:
 *  pContext:Cookie that should be passed back to the caller along with the callback.
 *  pFrame:Refernce to PAL frame.
 *  more: Does the invokee have more than one packet pending?
 * Return Value: SUCCESS  Completed successfully.
 *     FAILURE_XXX  Request was rejected due XXX Reason.
 *
 */


WDI_Status WDI_DS_TxPacket(void *pContext,
  wpt_packet *pFrame,
  wpt_boolean more)
{
  WDI_DS_ClientDataType *pClientData;
  wpt_uint8      ucSwFrameTXXlation;
  wpt_uint8      ucUP;
  wpt_uint8      ucTypeSubtype;
  wpt_uint8      isEapol;
  wpt_uint8      alignment;
  wpt_uint32     ucTxFlag;
  wpt_uint8      ucProtMgmtFrame;
  wpt_uint8*     pSTAMACAddress;
  wpt_uint8*     pAddr2MACAddress;
  WDI_DS_TxMetaInfoType     *pTxMetadata;
  void *physBDHeader, *pvBDHeader;
  wpt_uint8      ucType;
  WDI_DS_BdMemPoolType *pMemPool;
  wpt_uint8      ucBdPoolType;
  wpt_uint8      staId;
  WDI_Status wdiStatus;

  // Do Sanity checks
  if (NULL == pContext)
  {
    return WDI_STATUS_E_FAILURE;
  }

  pClientData = (WDI_DS_ClientDataType *) WDI_DS_GetDatapathContext(pContext);
  if (NULL == pClientData || pClientData->suspend)
  {
    return WDI_STATUS_E_FAILURE;
  }

  // extract metadata from PAL packet
  pTxMetadata = WDI_DS_ExtractTxMetaData(pFrame);
  ucSwFrameTXXlation = pTxMetadata->fdisableFrmXlt;
  ucTypeSubtype = pTxMetadata->typeSubtype;
  ucUP = pTxMetadata->fUP;
  isEapol = pTxMetadata->isEapol;
  ucTxFlag = pTxMetadata->txFlags;
  ucProtMgmtFrame = pTxMetadata->fProtMgmtFrame;
  pSTAMACAddress = &(pTxMetadata->fSTAMACAddress[0]);
  pAddr2MACAddress = &(pTxMetadata->addr2MACAddress[0]);

  /*------------------------------------------------------------------------
     Get type and subtype of the frame first 
  ------------------------------------------------------------------------*/
  ucType = (ucTypeSubtype & WDI_FRAME_TYPE_MASK) >> WDI_FRAME_TYPE_OFFSET;
  switch(ucType)
  {
    case WDI_MAC_DATA_FRAME:
#ifdef FEATURE_WLAN_TDLS
       /* I utilizes TDLS mgmt frame always sent at BD_RATE2. (See limProcessTdls.c)
          Assumption here is data frame sent by WDA_TxPacket() <- HalTxFrame/HalTxFrameWithComplete()
          should take managment path. As of today, only TDLS feature has special data frame
          which needs to be treated as mgmt.
        */
       if((!pTxMetadata->isEapol) &&
          ((pTxMetadata->txFlags & WDI_USE_BD_RATE2_FOR_MANAGEMENT_FRAME) != WDI_USE_BD_RATE2_FOR_MANAGEMENT_FRAME))
#else
       if(!pTxMetadata->isEapol)
#endif
       {
       pMemPool = &(pClientData->dataMemPool);
       ucBdPoolType = WDI_DATA_POOL_ID;
       break;
       }
    // intentional fall-through to handle eapol packet as mgmt
    case WDI_MAC_MGMT_FRAME:
       pMemPool = &(pClientData->mgmtMemPool);
       ucBdPoolType = WDI_MGMT_POOL_ID;
    break;
    default:
      return WDI_STATUS_E_FAILURE;
  }

  // Allocate BD header from pool
  pvBDHeader = WDI_DS_MemPoolAlloc(pMemPool, &physBDHeader, ucBdPoolType);
  if(NULL == pvBDHeader)
    return WDI_STATUS_E_FAILURE;

  WDI_SetBDPointers(pFrame, pvBDHeader, physBDHeader);

  alignment = 0;
  WDI_DS_PrepareBDHeader(pFrame, ucSwFrameTXXlation, alignment);
  if (pTxMetadata->isEapol)
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_INFO,
              "Packet Length is %d\n", pTxMetadata->fPktlen);
  }
  wdiStatus = WDI_FillTxBd(pContext, ucTypeSubtype, pSTAMACAddress, pAddr2MACAddress,
    &ucUP, 1, pvBDHeader, ucTxFlag /* No ACK */, ucProtMgmtFrame, 0, isEapol, &staId,
    pTxMetadata->txBdToken);

  if(WDI_STATUS_SUCCESS != wdiStatus)
  {
    WDI_DS_MemPoolFree(pMemPool, pvBDHeader, physBDHeader);
    return wdiStatus;
  }

  pTxMetadata->staIdx = staId;

  // Send packet to transport layer.
  if(eWLAN_PAL_STATUS_SUCCESS !=WDTS_TxPacket(pContext, pFrame)){
    WDI_DS_MemPoolFree(pMemPool, pvBDHeader, physBDHeader);
    return WDI_STATUS_E_FAILURE;
  }  

  /* resource count only for data packet */
  // EAPOL packet doesn't use data mem pool if being treated as higher priority
#ifdef FEATURE_WLAN_TDLS
  /* I utilizes TDLS mgmt frame always sent at BD_RATE2. (See limProcessTdls.c)
     Assumption here is data frame sent by WDA_TxPacket() <- HalTxFrame/HalTxFrameWithComplete()
     should take managment path. As of today, only TDLS feature has special data frame
     which needs to be treated as mgmt.
  */
  if((WDI_MAC_DATA_FRAME == ucType) && (!pTxMetadata->isEapol) && ((pTxMetadata->txFlags & WDI_USE_BD_RATE2_FOR_MANAGEMENT_FRAME) != WDI_USE_BD_RATE2_FOR_MANAGEMENT_FRAME))
#else
  if(WDI_MAC_DATA_FRAME == ucType && (!pTxMetadata->isEapol))
#endif
  {
    WDI_DS_MemPoolIncreaseReserveCount(pMemPool, staId);
  }
  return WDI_STATUS_SUCCESS;  
}
 
 
/* DAL Transmit Complete function. 
 * Parameters:
 *  pContext:Cookie that should be passed back to the caller along with the callback.
 *  ucTxResReq:TX resource number required by TL
 * Return Value: SUCCESS  Completed successfully.
 *     FAILURE_XXX  Request was rejected due XXX Reason.
 *
 */


WDI_Status WDI_DS_TxComplete(void *pContext, wpt_uint32 ucTxResReq)
{
  // Do Sanity checks
  if(NULL == pContext)
    return WDI_STATUS_E_FAILURE;
  
  // Send notification to transport layer.
  if(eWLAN_PAL_STATUS_SUCCESS !=WDTS_CompleteTx(pContext, ucTxResReq))
  {
    return WDI_STATUS_E_FAILURE;
  }  

  return WDI_STATUS_SUCCESS;  
} 

/* DAL Suspend Transmit function. 
 * Parameters:
 *  pContext:Cookie that should be passed back to the caller along with the callback.
 * Return Value: SUCCESS  Completed successfully.
 *     FAILURE_XXX  Request was rejected due XXX Reason.
 *
 */


WDI_Status WDI_DS_TxSuspend(void *pContext)
{
  WDI_DS_ClientDataType *pClientData =  
    (WDI_DS_ClientDataType *) WDI_DS_GetDatapathContext(pContext);
  pClientData->suspend = 1;

  return WDI_STATUS_SUCCESS;  

}


/* DAL Resume Transmit function. 
 * Parameters:
 *  pContext:Cookie that should be passed back to the caller along with the callback.
 * Return Value: SUCCESS  Completed successfully.
 *     FAILURE_XXX  Request was rejected due XXX Reason.
 *
 */


WDI_Status WDI_DS_TxResume(void *pContext)
{
  WDI_DS_ClientDataType *pClientData =  
    (WDI_DS_ClientDataType *) WDI_DS_GetDatapathContext(pContext);

  pClientData->suspend = 0;

  return WDI_STATUS_SUCCESS;  
}

/* DAL Get Available Resource Count. 
 * This is the number of free descririptor in DXE
 * Parameters:
 *  pContext:Cookie that should be passed back to the caller along with the callback.
 *  wdiResPool: - identifier of resource pool
 * Return Value: number of resources available
 *               This is the number of free descririptor in DXE
 *
 */

wpt_uint32 WDI_GetAvailableResCount(void *pContext,WDI_ResPoolType wdiResPool)
{
  WDI_DS_ClientDataType *pClientData =  
    (WDI_DS_ClientDataType *) WDI_DS_GetDatapathContext(pContext);

  switch(wdiResPool)
  {
    case WDI_MGMT_POOL_ID:
      return (WDI_DS_HI_PRI_RES_NUM - 2*WDI_DS_GetAvailableResCount(&pClientData->mgmtMemPool));
    case WDI_DATA_POOL_ID:
      return WDTS_GetFreeTxDataResNumber(pContext);
    default:
      return 0;
  }
}

/* DAL Get resrved Resource Count per STA. 
 * Parameters:
 *  pContext:Cookie that should be passed back to the caller along with the callback.
 *  wdiResPool: - identifier of resource pool
 *  staId: STA ID
 * Return Value: number of resources reserved per STA
 *
 */
wpt_uint32 WDI_DS_GetReservedResCountPerSTA(void *pContext,WDI_ResPoolType wdiResPool, wpt_uint8 staId)
{
  WDI_DS_ClientDataType *pClientData =  
    (WDI_DS_ClientDataType *) WDI_DS_GetDatapathContext(pContext);
  switch(wdiResPool)
  {
    case WDI_MGMT_POOL_ID:
      return WDI_DS_MemPoolGetRsvdResCountPerSTA(&pClientData->mgmtMemPool, staId);
    case WDI_DATA_POOL_ID:
      return WDI_DS_MemPoolGetRsvdResCountPerSTA(&pClientData->dataMemPool, staId);
    default:
      return 0;
  }
}

/* DAL STA info add into memPool. 
 * Parameters:
 *  pContext:Cookie that should be passed back to the caller along with the callback.
 *  staId: STA ID
 * Return Value: number of resources reserved per STA
 *
 */
WDI_Status WDI_DS_AddSTAMemPool(void *pContext, wpt_uint8 staIndex)
{
  WDI_Status status = WDI_STATUS_SUCCESS;
  WDI_DS_ClientDataType *pClientData =  
    (WDI_DS_ClientDataType *) WDI_DS_GetDatapathContext(pContext);

  status = WDI_DS_MemPoolAddSTA(&pClientData->mgmtMemPool, staIndex);
  if(WDI_STATUS_SUCCESS != status)
  {
    /* Add STA into MGMT memPool Fail */
    return status;
  }

  status = WDI_DS_MemPoolAddSTA(&pClientData->dataMemPool, staIndex);
  if(WDI_STATUS_SUCCESS != status)
  {
    /* Add STA into DATA memPool Fail */
    return status;
  }

  return WDI_STATUS_SUCCESS; 
}

/* DAL STA info del from memPool. 
 * Parameters:
 *  pContext:Cookie that should be passed back to the caller along with the callback.
 *  staId: STA ID
 * Return Value: number of resources reserved per STA
 *
 */
WDI_Status WDI_DS_DelSTAMemPool(void *pContext, wpt_uint8 staIndex)
{
  WDI_Status status = WDI_STATUS_SUCCESS;
  WDI_DS_ClientDataType *pClientData =  
    (WDI_DS_ClientDataType *) WDI_DS_GetDatapathContext(pContext);

  status = WDI_DS_MemPoolDelSTA(&pClientData->mgmtMemPool, staIndex);
  if(WDI_STATUS_SUCCESS != status)
  {
    /* Del STA from MGMT memPool Fail */
    return status;
  }
  status = WDI_DS_MemPoolDelSTA(&pClientData->dataMemPool, staIndex);
  if(WDI_STATUS_SUCCESS != status)
  {
    /* Del STA from DATA memPool Fail */
    return status;
  }
  return WDI_STATUS_SUCCESS; 
}

/* DAL Set STA index associated with BSS index. 
 * Parameters:
 *  pContext:Cookie that should be passed back to the caller along with the callback.
 *  bssIdx: BSS index
 *  staId: STA index associated with BSS index
 * Return Status: Found empty slot
 *
 */
WDI_Status WDI_DS_SetStaIdxPerBssIdx(void *pContext, wpt_uint8 bssIdx, wpt_uint8 staIdx)
{
  WDI_DS_ClientDataType *pClientData;
  wpt_uint8              bssLoop;

  pClientData = (WDI_DS_ClientDataType *)WDI_DS_GetDatapathContext(pContext);
  for (bssLoop = 0; bssLoop < WDI_DS_MAX_SUPPORTED_BSS; bssLoop++)
  {
    if ((pClientData->staIdxPerBssIdxTable[bssLoop].isUsed) &&
        (bssIdx == pClientData->staIdxPerBssIdxTable[bssLoop].bssIdx) &&
        (staIdx == pClientData->staIdxPerBssIdxTable[bssLoop].staIdx))
    {
      return WDI_STATUS_SUCCESS;
    }

    if (0 == pClientData->staIdxPerBssIdxTable[bssLoop].isUsed)
    {
      pClientData->staIdxPerBssIdxTable[bssLoop].bssIdx = bssIdx;
      pClientData->staIdxPerBssIdxTable[bssLoop].staIdx = staIdx;
      pClientData->staIdxPerBssIdxTable[bssLoop].isUsed = 1;
      return WDI_STATUS_SUCCESS;
    }
  }

  /* Could not find empty slot */
  return WDI_STATUS_E_FAILURE;
}

/* DAL Get STA index associated with BSS index. 
 * Parameters:
 *  pContext:Cookie that should be passed back to the caller along with the callback.
 *  bssIdx: BSS index
 *  staId: STA index associated with BSS index
 * Return Status: Found empty slot
 *
 */
WDI_Status WDI_DS_GetStaIdxFromBssIdx(void *pContext, wpt_uint8 bssIdx, wpt_uint8 *staIdx)
{
  WDI_DS_ClientDataType *pClientData;
  wpt_uint8              bssLoop;

  pClientData = (WDI_DS_ClientDataType *)WDI_DS_GetDatapathContext(pContext);
  for(bssLoop = 0; bssLoop < WDI_DS_MAX_SUPPORTED_BSS; bssLoop++)
  {
    if(bssIdx == pClientData->staIdxPerBssIdxTable[bssLoop].bssIdx)
    {
      /* Found BSS index from slot */
      *staIdx = pClientData->staIdxPerBssIdxTable[bssLoop].staIdx;
      return WDI_STATUS_SUCCESS;
    }
  }

  /* Could not find associated STA index with BSS index */
  return WDI_STATUS_E_FAILURE;
}

/* DAL Clear STA index associated with BSS index. 
 * Parameters:
 *  pContext:Cookie that should be passed back to the caller along with the callback.
 *  bssIdx: BSS index
 *  staId: STA index associated with BSS index
 * Return Status: Found empty slot
 *
 */
WDI_Status WDI_DS_ClearStaIdxPerBssIdx(void *pContext, wpt_uint8 bssIdx, wpt_uint8 staIdx)
{
  WDI_DS_ClientDataType *pClientData;
  wpt_uint8              bssLoop;

  pClientData = (WDI_DS_ClientDataType *)WDI_DS_GetDatapathContext(pContext);
  for(bssLoop = 0; bssLoop < WDI_DS_MAX_SUPPORTED_BSS; bssLoop++)
  {
    if((bssIdx == pClientData->staIdxPerBssIdxTable[bssLoop].bssIdx) &&
       (staIdx == pClientData->staIdxPerBssIdxTable[bssLoop].staIdx))
    {
      pClientData->staIdxPerBssIdxTable[bssLoop].bssIdx = WDI_DS_INDEX_INVALID;
      pClientData->staIdxPerBssIdxTable[bssLoop].staIdx = WDI_DS_INDEX_INVALID;
      pClientData->staIdxPerBssIdxTable[bssLoop].isUsed = 0;
      return WDI_STATUS_SUCCESS;
    }
  }

  /* Could not find associated STA index with BSS index */
  return WDI_STATUS_E_FAILURE;
}

/* @brief: WDI_DS_GetTrafficStats
 * This function should be invoked to fetch the current stats
  * Parameters:
 *  pStats:Pointer to the collected stats
 *  len: length of buffer pointed to by pStats
 *  Return Status: None
 */
void WDI_DS_GetTrafficStats(WDI_TrafficStatsType** pStats, wpt_uint32 *len)
{
   return WDTS_GetTrafficStats(pStats, len);
}

/* @brief: WDI_DS_DeactivateTrafficStats
 * This function should be invoked to deactivate traffic stats collection
  * Parameters: None
 *  Return Status: None
 */
void WDI_DS_DeactivateTrafficStats(void)
{
   return WDTS_DeactivateTrafficStats();
}

/* @brief: WDI_DS_ActivateTrafficStats
 * This function should be invoked to activate traffic stats collection
  * Parameters: None
 *  Return Status: None
 */
void WDI_DS_ActivateTrafficStats(void)
{
   return WDTS_ActivateTrafficStats();
}

/* @brief: WDI_DS_ClearTrafficStats
 * This function should be invoked to clear all past stats
  * Parameters: None
 *  Return Status: None
 */
void WDI_DS_ClearTrafficStats(void)
{
   return WDTS_ClearTrafficStats();
}

