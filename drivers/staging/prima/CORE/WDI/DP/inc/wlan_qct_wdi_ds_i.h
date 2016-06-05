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

#if !defined( __WLAN_QCT_WDI_DS_I_H )
#define __WLAN_QCT_WDI_DS_I_H

/**=========================================================================
 *
 *       \file  wlan_qct_wdi_ds_i.h
 *
 *       \brief define Dataservice API
 *
 * WLAN Device Abstraction layer External API for Dataservice
 * DESCRIPTION
 *   This file contains the external API exposed by the
 *   wlan device abstarction layer module.
 *
 */

#include "wlan_qct_pal_type.h"
#include "wlan_qct_pal_status.h"
#include "wlan_qct_pal_packet.h"
#include "wlan_qct_pal_trace.h"
#include "wlan_qct_wdi_ds.h"
#include "wlan_qct_dxe.h"
#include "wlan_hal_msg.h"

#define WDI_DS_MAX_CHUNK_SIZE 128
#define WDI_802_11_MAX_HEADER_LEN 40


/*The number of resources (BD headers) available for the HI priority DXE
  channel
  DXE will use 1 descriptor for the BD header and 1 for the data =>
  This is true for LA but not EA. EA sends down 3~4 MDL chain per a packet.
  Now we set it the same with free DXE decriptor number*/
#define WDI_DS_HI_PRI_RES_NUM  (WLANDXE_HI_PRI_RES_NUM)

/*The number of resources (BD headers) available for the Low priority DXE
  channel - see above
*/
#define WDI_DS_LO_PRI_RES_NUM  (WLANDXE_LO_PRI_RES_NUM)

/*The number of BD headers available in the system for Tx must match the number
  of DXE descriptors available for actual transmission, otherwise we have to
  manage two diffrent level of resource pools*/

#define WDI_MAC_ADDR_SIZE ( 6 )
/*802.3 header definitions*/
#define  WDI_802_3_HEADER_LEN             14
/* Offset of DA field in a 802.3 header*/
#define  WDI_802_3_HEADER_DA_OFFSET        0
/*802.11 header definitions - header len without QOS ctrl field*/
#define  WDI_802_11_HEADER_LEN            24
/*802.11 header length + QOS ctrl field*/
#define  WDI_MPDU_HEADER_LEN              26
/*802.11 header definitions*/
#define  WDI_802_11_MAX_HEADER_LEN        40
/*802.11 header definitions - qos ctrl field len*/
#define  WDI_802_11_HEADER_QOS_CTL         2
/*802.11 ADDR4 MAC addr field len */
#define  WDI_802_11_HEADER_ADDR4_LEN       WDI_MAC_ADDR_SIZE





typedef enum
{
   DTI_TRACE_LEVEL_FATAL,
   DTI_TRACE_LEVEL_ERROR,
   DTI_TRACE_LEVEL_WARN,
   DTI_TRACE_LEVEL_INFO

} DTI_TRACE_LEVEL;

WPT_STATIC WPT_INLINE void DTI_TRACE ( DTI_TRACE_LEVEL level, ...) { };

/* !!! MAX NUM STA is not identified yet, 16 is correct value,
   but need to get from correct common def
   This should be identified ASAP */
#ifdef WLAN_SOFTAP_VSTA_FEATURE
#define WDI_DS_MAX_STA_ID 41
#else
#define WDI_DS_MAX_STA_ID 16
#endif
/* !!! MAX NUM SUPPORTED BSS is not identified yet, 2 is correct value,
    but need to get from correct common def
   This should be identified ASAP */
#define WDI_DS_MAX_SUPPORTED_BSS   2

#define WDI_DS_INDEX_INVALID       0xFF

/* Mem Pool resorce count per STA data type */
typedef struct {
  wpt_uint8    validIdx;
  wpt_uint8    STAIndex;
  wpt_uint32   numChunkReservedBySTA;
  /* Mutex, is not needed for counter operation
     since all TX Data frame operations will happen only TX thread
     All of the TX data frame operations are serialized, no pre-emption will happen
     This is just for place holder */
  wpt_mutex    resourceCountLock;
} WDI_DS_BdMemPoolSTAType;

typedef struct {
  void *pVirtBaseAddress;
  void *pPhysBaseAddress;
  wpt_uint32 poolSize;
  wpt_uint32 numChunks;
  wpt_uint32 chunkSize;
  wpt_uint32* AllocationBitmap;
  WDI_DS_BdMemPoolSTAType numChunkSTA[WDI_DS_MAX_STA_ID + 1];
} WDI_DS_BdMemPoolType;

/* STA index associated with BSS index data type */
typedef struct
{
   wpt_uint8   isUsed;
   wpt_uint8   bssIdx;
   wpt_uint8   staIdx;
} WDI_DS_staIdxPerBssIdxType;

typedef struct
{
   void *   pLoggingMbVirtAddress;
   void *   pLoggingMbPhysAddress;
   WDI_DS_LoggingSessionType loggingSession;
} WDI_DS_LoggingMbType;

WDI_Status WDI_DS_MemPoolCreate(WDI_DS_BdMemPoolType *memPool, wpt_uint8 chunkSize, wpt_uint8 numChunks);
void *WDI_DS_MemPoolAlloc(WDI_DS_BdMemPoolType *memPool, void **pPhysAddress, WDI_ResPoolType wdiResPool);
void  WDI_DS_MemPoolFree(WDI_DS_BdMemPoolType *memPool, void *pVirtAddress, void *pPhysAddress);
void WDI_DS_MemPoolDestroy(WDI_DS_BdMemPoolType *memPool);

WDI_Status WDI_DS_LoggingMbCreate(WDI_DS_LoggingMbType *pLoggingMailbox, wpt_uint8 size);
void WDI_DS_LoggingMbDestroy(WDI_DS_LoggingMbType *pLoggingMailbox);

typedef struct
{
  void                            *pcontext;
  void                            *pCallbackContext;
  wpt_uint8                        suspend;
  WDI_DS_BdMemPoolType             mgmtMemPool;
  WDI_DS_BdMemPoolType             dataMemPool;
  WDI_DS_RxPacketCallback          receiveFrameCB;
  WDI_DS_TxCompleteCallback        txCompleteCB;
  WDI_DS_TxFlowControlCallback     txResourceCB;
  WDI_DS_RxLogCallback             rxLogCB;
  WDI_DS_staIdxPerBssIdxType       staIdxPerBssIdxTable[WDI_DS_MAX_SUPPORTED_BSS];
  WDI_DS_LoggingMbType             loggingMbContext;
} WDI_DS_ClientDataType;

WPT_STATIC WPT_INLINE void WDI_GetBDPointers(wpt_packet *pFrame, void **pVirt, void **pPhys)
{
  *pVirt = WPAL_PACKET_GET_BD_POINTER(pFrame);
  *pPhys = WPAL_PACKET_GET_BD_PHYS(pFrame);
}


WPT_STATIC WPT_INLINE void WDI_SetBDPointers(wpt_packet *pFrame, void *pVirt, void *pPhys)
{
  WPAL_PACKET_SET_BD_POINTER(pFrame, pVirt);
  WPAL_PACKET_SET_BD_PHYS(pFrame, pPhys);
}


void
WDI_DS_PrepareBDHeader (
  wpt_packet*     palPacket,
  wpt_uint8      ucDisableHWFrmXtl,
  wpt_uint8       alignment
);

/**
 @brief Returns the available number of resources (BD headers)
        available for TX

 @param  pMemPool:         pointer to the BD memory pool

 @see
 @return Result of the function call
*/
wpt_uint32 WDI_DS_GetAvailableResCount(WDI_DS_BdMemPoolType *pMemPool);

/**
 @brief WDI_DS_GetreservedResCountPerSTA
        Returns the Reserved number of resources (BD headers)
        available for TX

 @param  pMemPool:         pointer to the BD memory pool
 @param  staId             STA ID

 @see
 @return Result of the function call
*/
wpt_uint32 WDI_DS_MemPoolGetRsvdResCountPerSTA(WDI_DS_BdMemPoolType *pMemPool, wpt_uint8  staId);

/**
 @brief WDI_DS_MemPoolAddSTA
        Add NEW STA into mempool

 @param  pMemPool:         pointer to the BD memory pool
 @param  staId             STA ID

 @see
 @return Result of the function call
*/
WDI_Status WDI_DS_MemPoolAddSTA(WDI_DS_BdMemPoolType *memPool, wpt_uint8 staIndex);

/**
 @brief WDI_DS_MemPoolAddSTA
        Remove STA from mempool

 @param  pMemPool:         pointer to the BD memory pool
 @param  staId             STA ID

 @see
 @return Result of the function call
*/
WDI_Status WDI_DS_MemPoolDelSTA(WDI_DS_BdMemPoolType *memPool, wpt_uint8 staIndex);

/**
 @brief Increase reserved TX resource count by specific STA

 @param  pMemPool:         pointer to the BD memory pool
 @param  staId             STA ID
 @see
 @return Result of the function call
*/
void WDI_DS_MemPoolIncreaseReserveCount(WDI_DS_BdMemPoolType *memPool, wpt_uint8  staId);

/**
 @brief Decrease reserved TX resource count by specific STA

 @param  pMemPool:         pointer to the BD memory pool
 @param  staId             STA ID
 @see
 @return Result of the function call
*/
void WDI_DS_MemPoolDecreaseReserveCount(WDI_DS_BdMemPoolType *memPool, wpt_uint8  staId);
#endif
