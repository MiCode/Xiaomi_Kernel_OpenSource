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
 *       \file  wlan_qct_dti_bd.c
 *
 *       \brief Datapath utilities file.
 *
 * WLAN Device Abstraction layer External API for Dataservice
 * DESCRIPTION
 *  This file contains the external API implemntation exposed by the
 *   wlan device abstarction layer module.
 *
 *   Copyright (c) 2008 QUALCOMM Incorporated. All Rights Reserved.
 *   Qualcomm Confidential and Proprietary
 */

#include "wlan_qct_wdi.h"
#include "wlan_qct_wdi_ds.h"
#include "wlan_qct_wdi_ds_i.h"
#include "wlan_qct_wdi_dts.h"
#include "wlan_qct_wdi_dp.h"
#include "wlan_qct_pal_type.h"
#include "wlan_qct_pal_status.h"
#include "wlan_qct_pal_api.h"
#include "wlan_qct_pal_packet.h"



/*==========================================================================
 *
 FUNCTION    WDI_DS_PrepareBDHeader

 DESCRIPTION
 function for preparing BD header before HAL processing.

 PARAMETERS

 IN
palPacket:     PAL packet pointer


RETURN VALUE
No return.

SIDE EFFECTS

============================================================================*/
void
WDI_DS_PrepareBDHeader (wpt_packet* palPacket,
                        wpt_uint8 ucDisableHWFrmXtl, wpt_uint8 alignment)
{
  void*          pvBDHeader;
  wpt_uint8      ucHeaderOffset;
  wpt_uint8      ucHeaderLen;
  wpt_uint8      ucQosEnabled;
  wpt_uint8      ucWDSEnabled;
  wpt_uint32     ucMpduLen;
  wpt_uint32     ucPktLen;
  WDI_DS_TxMetaInfoType     *pTxMetadata;


  /* Extract reuqired information from Metadata */
  pvBDHeader = WPAL_PACKET_GET_BD_POINTER(palPacket);
  pTxMetadata = WDI_DS_ExtractTxMetaData(palPacket);
  ucQosEnabled = pTxMetadata->qosEnabled;
  ucWDSEnabled = pTxMetadata->fenableWDS;

  WPAL_PACKET_SET_BD_LENGTH(palPacket, WDI_TX_BD_HEADER_SIZE);

  /*---------------------------------------------------------------------
    Fill MPDU info fields:
    - MPDU data start offset
    - MPDU header start offset
    - MPDU header length
    - MPDU length - this is a 16b field - needs swapping
    --------------------------------------------------------------------*/

  if ( ucDisableHWFrmXtl ) {
    ucHeaderOffset = WDI_TX_BD_HEADER_SIZE;
    ucHeaderLen = WDI_802_11_HEADER_LEN;
    if ( 0 != ucQosEnabled ) {
      ucHeaderLen += WDI_802_11_HEADER_QOS_CTL;
    }
    if ( 0 != ucWDSEnabled) {
      ucHeaderLen    += WDI_802_11_HEADER_ADDR4_LEN;
    }
  } else {
    ucHeaderOffset = WDI_TX_BD_HEADER_SIZE+WDI_802_11_MAX_HEADER_LEN;
    ucHeaderLen = WDI_802_3_HEADER_LEN;
  }

  WDI_TX_BD_SET_MPDU_HEADER_LEN( pvBDHeader, ucHeaderLen);
  WDI_TX_BD_SET_MPDU_HEADER_OFFSET( pvBDHeader, ucHeaderOffset);
  WDI_TX_BD_SET_MPDU_DATA_OFFSET( pvBDHeader,
      ucHeaderOffset + ucHeaderLen + alignment);

  // pkt length from PAL API. Need to change in case of HW FT used
  ucPktLen  = wpalPacketGetLength( palPacket ); // This includes BD length
  /** This is the length (in number of bytes) of the entire MPDU
      (header and data). Note that the length INCLUDES FCS field. */
  ucMpduLen = ucPktLen - WPAL_PACKET_GET_BD_LENGTH( palPacket );
  WDI_TX_BD_SET_MPDU_LEN( pvBDHeader, ucMpduLen );

  DTI_TRACE(  DTI_TRACE_LEVEL_INFO,
      "WLAN DTI: VALUES ARE HLen=%x Hoff=%x doff=%x len=%x ex=%d",
      ucHeaderLen, ucHeaderOffset,
      (ucHeaderOffset + ucHeaderLen + alignment),
      pTxMetadata->fPktlen, alignment);

}/* WDI_DS_PrepareBDHeader */

/*==========================================================================
 *
 FUNCTIONS    WDI_DS_MemPoolXXX

 DESCRIPTION
  APIs for managing the BD header memory pool
 PARAMETERS

 IN
WDI_DS_BdMemPoolType:     Memory pool pointer



============================================================================*/

/*
 * Create a memory pool which is DMA capabale
 */
WDI_Status WDI_DS_MemPoolCreate(WDI_DS_BdMemPoolType *memPool, wpt_uint8 chunkSize,
                                                                  wpt_uint8 numChunks)
{
  wpt_uint8 staLoop;

  //Allocate all the max size and align them to a double word boundary. The first 8 bytes are control bytes.
  memPool->numChunks = 0;
  memPool->chunkSize = chunkSize + 16 - (chunkSize%8);
  memPool->pVirtBaseAddress = wpalDmaMemoryAllocate((numChunks * memPool->chunkSize),
          &(memPool->pPhysBaseAddress));

  if( memPool->pVirtBaseAddress == 0)
    return WDI_STATUS_E_FAILURE;

  memPool->AllocationBitmap = (wpt_uint32*)wpalMemoryAllocate( (numChunks/32 + 1) * sizeof(wpt_uint32));
  if( NULL == memPool->AllocationBitmap)
     return WDI_STATUS_E_FAILURE;
  wpalMemoryZero(memPool->AllocationBitmap, (numChunks/32+1)*sizeof(wpt_uint32));

  //Initialize resource infor per STA
  for(staLoop = 0; staLoop < WDI_DS_MAX_STA_ID; staLoop++)
  {
    memPool->numChunkSTA[staLoop].STAIndex = 0xFF;
    memPool->numChunkSTA[staLoop].numChunkReservedBySTA = 0;
    memPool->numChunkSTA[staLoop].validIdx = 0;
  }

  return WDI_STATUS_SUCCESS;
}

/*
 * Destroy the memory pool
 */
void WDI_DS_MemPoolDestroy(WDI_DS_BdMemPoolType *memPool)
{
  //Allocate all the max size.
  wpalDmaMemoryFree(memPool->pVirtBaseAddress);
  wpalMemoryFree(memPool->AllocationBitmap);
  wpalMemoryZero(memPool, sizeof(*memPool));
}
/*
 * Allocate chunk memory
 */
WPT_STATIC WPT_INLINE int find_leading_zero_and_setbit(wpt_uint32 *bitmap, wpt_uint32 maxNumPool)
{
  wpt_uint32 i,j, word;
  int ret_val = -1;

  for(i=0; i < (maxNumPool/32 + 1); i++){
    j = 0;
    word = bitmap[i];
    for(j=0; j< 32; j++){
      if((word & 1) == 0) {
        bitmap[i] |= (1 << j);
        return((i<<5) + j);
      }
      word >>= 1;
    }
  }
  return ret_val;
}

void *WDI_DS_MemPoolAlloc(WDI_DS_BdMemPoolType *memPool, void **pPhysAddress,
                               WDI_ResPoolType wdiResPool)
{
  wpt_uint32 index;
  void *pVirtAddress;
  wpt_uint32 maxNumPool;
  switch(wdiResPool)
  {
    case WDI_MGMT_POOL_ID:
      maxNumPool = WDI_DS_HI_PRI_RES_NUM;
      break;
    case WDI_DATA_POOL_ID:
       maxNumPool = WDI_DS_LO_PRI_RES_NUM;
      break;
    default:
      return NULL;
  }

  if(maxNumPool == memPool->numChunks)
  {
     return NULL;
  }
  //Find the leading 0 in the allocation bitmap

  if((index = find_leading_zero_and_setbit(memPool->AllocationBitmap, maxNumPool)) == -EPERM)
  {
     //DbgBreakPoint();
     DTI_TRACE(  DTI_TRACE_LEVEL_INFO, "WDI_DS_MemPoolAlloc: index:%d(NULL), numChunks:%d",
                  index, memPool->numChunks );
     return NULL;
  }
  memPool->numChunks++;
  // The first 8 bytes are reserved for internal use for control bits and hash.
  pVirtAddress  = (wpt_uint8 *)memPool->pVirtBaseAddress + (memPool->chunkSize * index) + 8;
  *pPhysAddress = (wpt_uint8 *)memPool->pPhysBaseAddress + (memPool->chunkSize * index) + 8;

  DTI_TRACE(  DTI_TRACE_LEVEL_INFO, "WDI_DS_MemPoolAlloc: index:%d, numChunks:%d", index, memPool->numChunks );

  return pVirtAddress;

}

/*
 * Free chunk memory
 */
void  WDI_DS_MemPoolFree(WDI_DS_BdMemPoolType *memPool, void *pVirtAddress, void *pPhysAddress)
{
  wpt_uint32 index =
    ((wpt_uint8 *)pVirtAddress - (wpt_uint8 *)memPool->pVirtBaseAddress - 8)/memPool->chunkSize;
  wpt_uint32 word = memPool->AllocationBitmap[index/32];
  word &= ~(1<<(index%32));
  memPool->AllocationBitmap[index/32] = word;
  memPool->numChunks--;

  //DbgPrint( "WDI_DS_MemPoolFree: index:%d, numChunks:%d", index, memPool->numChunks );
}


/**
 @brief Returns the available number of resources (BD headers)
        available for TX

 @param  pMemPool:         pointer to the BD memory pool

 @see
 @return Result of the function call
*/
wpt_uint32 WDI_DS_GetAvailableResCount(WDI_DS_BdMemPoolType *pMemPool)
{
  return pMemPool->numChunks;
}

/**
 @brief WDI_DS_MemPoolAddSTA
        Add NEW STA into mempool

 @param  pMemPool:         pointer to the BD memory pool
 @param  staId             STA ID

 @see
 @return Result of the function call
*/
WDI_Status WDI_DS_MemPoolAddSTA(WDI_DS_BdMemPoolType *memPool, wpt_uint8 staIndex)
{
  if(memPool->numChunkSTA[staIndex].STAIndex != 0xFF)
  {
    /* Already using this slot? Do nothing */
    return WDI_STATUS_SUCCESS;
  }

  memPool->numChunkSTA[staIndex].STAIndex = staIndex;
  memPool->numChunkSTA[staIndex].numChunkReservedBySTA = 0;
  memPool->numChunkSTA[staIndex].validIdx = 1;
  return WDI_STATUS_SUCCESS;
}

/**
 @brief WDI_DS_MemPoolAddSTA
        Remove STA from mempool

 @param  pMemPool:         pointer to the BD memory pool
 @param  staId             STA ID

 @see
 @return Result of the function call
*/
WDI_Status WDI_DS_MemPoolDelSTA(WDI_DS_BdMemPoolType *memPool, wpt_uint8 staIndex)
{
  if(memPool->numChunkSTA[staIndex].STAIndex == 0xFF)
  {
    /* Empty this slot? error, bad argument */
      return WDI_STATUS_E_FAILURE;
  }

  memPool->numChunkSTA[staIndex].STAIndex = 0xFF;
  memPool->numChunkSTA[staIndex].numChunkReservedBySTA = 0;
  memPool->numChunkSTA[staIndex].validIdx = 0;
  return WDI_STATUS_SUCCESS;
}

/**
 @brief Returns the reserved number of resources (BD headers) per STA
        available for TX

 @param  pMemPool:         pointer to the BD memory pool
 @param  staId             STA ID
 @see
 @return Result of the function call
*/
wpt_uint32 WDI_DS_MemPoolGetRsvdResCountPerSTA(WDI_DS_BdMemPoolType *pMemPool, wpt_uint8  staId)
{
  return pMemPool->numChunkSTA[staId].numChunkReservedBySTA;
}

/**
 @brief Increase reserved TX resource count by specific STA

 @param  pMemPool:         pointer to the BD memory pool
 @param  staId             STA ID
 @see
 @return Result of the function call
*/
void WDI_DS_MemPoolIncreaseReserveCount(WDI_DS_BdMemPoolType *memPool, wpt_uint8  staId)
{

  if((memPool->numChunkSTA[staId].validIdx) && (staId < WDI_DS_MAX_STA_ID))
  {
    memPool->numChunkSTA[staId].numChunkReservedBySTA++;
  }
  return;
}

/**
 @brief Decrease reserved TX resource count by specific STA

 @param  pMemPool:         pointer to the BD memory pool
 @param  staId             STA ID
 @see
 @return Result of the function call
*/
void WDI_DS_MemPoolDecreaseReserveCount(WDI_DS_BdMemPoolType *memPool, wpt_uint8  staId)
{
  if(0 == memPool->numChunkSTA[staId].numChunkReservedBySTA)
  {
    DTI_TRACE( DTI_TRACE_LEVEL_ERROR,
               "SAT %d reserved resource count cannot be smaller than 0", staId );
    return;
  }

  if((memPool->numChunkSTA[staId].validIdx) && (staId < WDI_DS_MAX_STA_ID))
  {
    memPool->numChunkSTA[staId].numChunkReservedBySTA--;
  }
  return;
}
