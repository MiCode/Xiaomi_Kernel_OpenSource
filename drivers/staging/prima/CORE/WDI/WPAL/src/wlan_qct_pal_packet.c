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
  
  \file  wlan_qct_pal_packet.c
  
  \brief Implementation for PAL packet. wpt = (Wlan Pal Type) wpal = (Wlan PAL)
               
   Definitions for platform with VOSS packet support and LA.
  
   Copyright 2010 (c) Qualcomm, Incorporated.  All Rights Reserved.
   
   Qualcomm Confidential and Proprietary.
  
  ========================================================================*/

#include "wlan_qct_pal_packet.h"
#include "wlan_qct_pal_api.h"
#include "wlan_qct_pal_trace.h"
#include "wlan_qct_os_status.h"
#include "vos_packet.h"
#include "vos_trace.h"
#include "vos_list.h"

#include <linux/skbuff.h>
#include "dma-mapping.h"
#include <linux/wcnss_wlan.h>

/*Per spec definition*/
#define WPAL_ETHERNET_PAKCET_HEADER_SIZE     14

/*Per spec definition - not including QOS field*/
#define WPAL_802_11_PACKET_HEADER_SIZE    24 

/*p is a pointer to wpt_packet*/
#define WPAL_TO_VOS_PKT(p) ((vos_pkt_t *)(p))


typedef struct
{
  void*      pPhyAddr;
  wpt_uint32 uLen;
}wpt_iterator_info;

/* Storage for DXE CB function pointer */
static wpalPacketLowPacketCB wpalPacketAvailableCB;

/* Temp storage for transport channel DIAG/LOG information
 * Each channel will update information with different context
 * Before send stored date to DIAG,
 * temporary it should be stored */
static wpt_log_data_stall_type wpalTrasportStallInfo;

/*
   wpalPacketInit is no-op for VOSS-support wpt_packet
*/
wpt_status wpalPacketInit(void *pPalContext)
{
   return eWLAN_PAL_STATUS_SUCCESS;
}


/*
   wpalPacketClose is no-op for VOSS-support wpt_packet
*/
wpt_status wpalPacketClose(void *pPalContext)
{
   return eWLAN_PAL_STATUS_SUCCESS;
}

/*---------------------------------------------------------------------------
    wpalPacketRXLowResourceCB – RX RAW packer CB function
    Param: 
        pPacket – Available RX packet
        userData - PAL Client Context, DXE
    Return:
        Status
---------------------------------------------------------------------------*/
VOS_STATUS wpalPacketRXLowResourceCB(vos_pkt_t *pPacket, v_VOID_t *userData)
{
   VOS_STATUS   vosStatus = VOS_STATUS_E_FAILURE;
   void*        pData     = NULL;

   if (NULL == pPacket)
   {
      WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_ERROR, 
                  "Get new RX PAL packet fail");
      return VOS_STATUS_E_FAILURE;
   }
   vosStatus = vos_pkt_reserve_head_fast( pPacket, &pData,
                                          VPKT_SIZE_BUFFER );
   if(VOS_STATUS_SUCCESS != vosStatus)
   {
      WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_ERROR, 
                  "Prepare RX packet for DXE fail");
      return VOS_STATUS_E_FAILURE;
   }

   if((NULL == wpalPacketAvailableCB) || (NULL == userData))
   {
      WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_ERROR, 
                  "Invalid ARG for new RX packet");
      return VOS_STATUS_E_FAILURE;
   }

   wpalPacketAvailableCB( (wpt_packet *)pPacket, userData );

   return VOS_STATUS_SUCCESS;
}

/*---------------------------------------------------------------------------
    wpalPacketAlloc – Allocate a wpt_packet from PAL.
    Param: 
        pktType – specify the type of wpt_packet to allocate
        nPktSize - packet size
    Return:
        A pointer to the wpt_packet. NULL means fail.
---------------------------------------------------------------------------*/
wpt_packet * wpalPacketAlloc(wpt_packet_type pktType, wpt_uint32 nPktSize,
                             wpalPacketLowPacketCB rxLowCB, void *usrData)
{
   VOS_STATUS   vosStatus = VOS_STATUS_E_FAILURE;
   wpt_packet*  pPkt      = NULL;
   vos_pkt_t*   pVosPkt   = NULL;
   void*        pData     = NULL;
   v_U16_t      allocLen;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   switch (pktType)
   {
   case eWLAN_PAL_PKT_TYPE_TX_802_11_MGMT:
      vosStatus = vos_pkt_get_packet(&pVosPkt, VOS_PKT_TYPE_TX_802_11_MGMT,
                                       nPktSize, 1, VOS_FALSE, 
                                       NULL, NULL /*no callback*/);
      break;

   case eWLAN_PAL_PKT_TYPE_RX_RAW:
      /* Set the wpalPacketAvailableCB before we try to get a VOS
       * packet from the 'free list' and reset it if vos_pkt_get_packet()
       * returns a valid packet. This order is required to avoid the
       * race condition:
       * 1. The below call to vos_pkt_get_packet() in RX_Thread determines
       *    that no more packets are available in the 'free list' and sets
       *    the low resource callbacks.
       * 2. in parallel vos_pkt_return_packet() is called in MC_Thread for a
       *    Management frame before wpalPacketAlloc() gets a chance to set
       *    wpalPacketAvailableCB and since the 'low resource callbacks'
       *    are set the callback function - wpalPacketRXLowResourceCB is
       *    executed,but since wpalPacketAvailableCB is still NULL the low
       *    resource recovery fails.
       */
      wpalPacketAvailableCB = rxLowCB;

      vosStatus = vos_pkt_get_packet(&pVosPkt, VOS_PKT_TYPE_RX_RAW,
                                       nPktSize, 1, VOS_FALSE, 
                                       wpalPacketRXLowResourceCB, usrData);

#ifndef FEATURE_R33D
      /* Reserve the entire raw rx buffer for DXE */
      if( vosStatus == VOS_STATUS_SUCCESS )
      {
        wpalPacketAvailableCB = NULL;
        vosStatus =  vos_pkt_reserve_head_fast( pVosPkt, &pData, nPktSize ); 
      }
#endif /* FEATURE_R33D */
      if((NULL != pVosPkt) && (VOS_STATUS_E_RESOURCES != vosStatus))
      {
         vos_pkt_get_packet_length(pVosPkt, &allocLen);
         if (nPktSize != allocLen)
         {
            WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_ERROR,
                       "RX packet alloc has problem, discard this frame, Len %d", allocLen);
            vos_pkt_return_packet(pVosPkt);
            return NULL;
         }
      }
      break;

   default:
      WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_ERROR, 
                  " try to allocate unsupported packet type (%d)", pktType);
      break;
   }

   if(VOS_IS_STATUS_SUCCESS(vosStatus))
   {
      pPkt = (wpt_packet *)pVosPkt;
   }


   return pPkt;
}/*wpalPacketAlloc*/



/*---------------------------------------------------------------------------
    wpalPacketFree – Free a wpt_packet chain for one particular type.
    For our legacy UMAC, it is not needed because vos_packet contains pal_packet.
    Param: 
        pPkt – pointer to a wpt_packet
    Return:
        eWLAN_PAL_STATUS_SUCCESS - success
---------------------------------------------------------------------------*/
wpt_status wpalPacketFree(wpt_packet *pPkt)
{
   VOS_STATUS vosStatus;

   if(NULL != pPkt->pInternalData)
   {
      wpalMemoryFree(pPkt->pInternalData);
   }
   vosStatus = vos_pkt_return_packet(WPAL_TO_VOS_PKT(pPkt));

   //With VOSS support, we can cast between wpt_status and VOS_STATUS
   return (wpt_status)vosStatus;
}/*wpalPacketFree*/


/*---------------------------------------------------------------------------
    wpalPacketGetLength – Get number of bytes in a wpt_packet. It include the 
    bytes in a BD if it exist.
    Param: 
        pPkt - pointer to a packet to be freed.
    Return:
        Length of the data include layer-2 headers. For example, if the frame
        is 802.3, the length includes the ethernet header.
---------------------------------------------------------------------------*/
wpt_uint32 wpalPacketGetLength(wpt_packet *pPkt)
{
   v_U16_t len = 0, pktLen = 0;

   // Validate the parameter pointers
   if (unlikely(NULL == pPkt))
   {
      WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_ERROR,
                "%s : NULL packet pointer", __func__);
      return eWLAN_PAL_STATUS_E_INVAL;
   }


   if( WPAL_PACKET_GET_BD_POINTER(pPkt) )
   {
      len = WPAL_PACKET_GET_BD_LENGTH(pPkt);
   }
   if( VOS_IS_STATUS_SUCCESS(vos_pkt_get_packet_length(WPAL_TO_VOS_PKT(pPkt), &pktLen)) )
   {
      len += pktLen;
   }
   else
   {
      WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_ERROR, "%s  failed",
         __func__);
   }

   return ((wpt_uint32)len);
}/*wpalPacketGetLength*/


/*---------------------------------------------------------------------------
    wpalPacketRawTrimHead – Move the starting offset and return the head pointer
          before the moving. The function can only be used with raw packets,
          whose buffer is one piece and allocated by WLAN driver. This also
          reduce the length of the packet.
    Param: 
        pPkt - pointer to a wpt_packet.
        size – number of bytes to take off the head.
    Return:
        A pointer to the original buffer head before the trimming.
---------------------------------------------------------------------------*/
wpt_status wpalPacketRawTrimHead(wpt_packet *pPkt, wpt_uint32 size)
{
   wpt_status status = eWLAN_PAL_STATUS_SUCCESS;

   // Validate the parameter pointers
   if (unlikely(NULL == pPkt))
   {
      WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_ERROR,
                "%s : NULL packet pointer", __func__);
      return eWLAN_PAL_STATUS_E_INVAL;
   }

   if ((eWLAN_PAL_PKT_TYPE_TX_802_11_MGMT == WPAL_PACKET_GET_TYPE(pPkt)) ||
               (eWLAN_PAL_PKT_TYPE_RX_RAW == WPAL_PACKET_GET_TYPE(pPkt)))
   {
       // Continue to trim the packet
   }
   else
   {
      WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_ERROR,
                "%s : neither 80211 managment packet nor RAW packet", __func__);
      VOS_ASSERT(0);
      return eWLAN_PAL_STATUS_E_INVAL;
   }

   if( !VOS_IS_STATUS_SUCCESS(vos_pkt_trim_head(WPAL_TO_VOS_PKT(pPkt), (v_SIZE_t)size)) )
   {
      WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_ERROR, "%s  Invalid trim(%d)",
         __func__, size);
      status = eWLAN_PAL_STATUS_E_INVAL;
   }

   return status;
}/*wpalPacketRawTrimHead*/

/*---------------------------------------------------------------------------
    wpalPacketRawTrimTail – reduce the length of the packet.
    Param: 
        pPkt - pointer to a wpt_packet.
        size – number of bytes to take of the packet length
    Return:
        eWLAN_PAL_STATUS_SUCCESS – success. Otherwise fail.
---------------------------------------------------------------------------*/
wpt_status wpalPacketRawTrimTail(wpt_packet *pPkt, wpt_uint32 size)
{
   wpt_status status = eWLAN_PAL_STATUS_SUCCESS;

   // Validate the parameter pointers
   if (unlikely(NULL == pPkt))
   {
      WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_ERROR,
                "%s : NULL packet pointer", __func__);
      return eWLAN_PAL_STATUS_E_INVAL;
   }

   if ((eWLAN_PAL_PKT_TYPE_TX_802_11_MGMT == WPAL_PACKET_GET_TYPE(pPkt)) ||
               (eWLAN_PAL_PKT_TYPE_RX_RAW == WPAL_PACKET_GET_TYPE(pPkt)))
   {
       // Continue to trim the packet
   }
   else
   {
      WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_ERROR,
                "%s : neither 80211 managment packet nor RAW packet", __func__);
      VOS_ASSERT(0);
      return eWLAN_PAL_STATUS_E_INVAL;
   }

   if( !VOS_IS_STATUS_SUCCESS(vos_pkt_trim_tail(WPAL_TO_VOS_PKT(pPkt), (v_SIZE_t)size)) )
   {
      WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_ERROR, "%s  Invalid trim(%d)",
         __func__, size);
      status = eWLAN_PAL_STATUS_E_INVAL;
   }

   return status;
}/*wpalPacketRawTrimTail*/


/*---------------------------------------------------------------------------
    wpalPacketGetRawBuf – Return the starting buffer virtual address for the RAW flat buffer
    It is inline in hope of faster implementation for certain platform. For Winxp, it 
    will be slow.
    Param: 
        pPkt - pointer to a wpt_packet.
    Return:
        NULL - fail.
        Otherwise the address of the starting of the buffer
---------------------------------------------------------------------------*/
wpt_uint8 *wpalPacketGetRawBuf(wpt_packet *pPkt)
{
   wpt_uint8 *pRet = NULL;

   // Validate the parameter pointers
   if (unlikely(NULL == pPkt))
   {
      WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_ERROR,
                "%s : NULL packet pointer", __func__);
      return NULL;
   }

   //Since it is a flat buffer, all we need is to get one byte of offset 0
   if( (eWLAN_PAL_PKT_TYPE_RX_RAW == WPAL_PACKET_GET_TYPE(pPkt)) ||
       (eWLAN_PAL_PKT_TYPE_TX_802_11_MGMT == WPAL_PACKET_GET_TYPE(pPkt)) )
   {
      vos_pkt_peek_data(WPAL_TO_VOS_PKT(pPkt), 0, (v_VOID_t**)&pRet, 1);
      WPAL_ASSERT(NULL != pRet);
   }            

   return pRet;
}/*wpalPacketGetRawBuf*/


/*---------------------------------------------------------------------------
    wpalPacketSetRxLength – Set the valid data length on a RX packet. This function must 
    be called once per RX packet per receiving. It indicates the available data length from
    the start of the buffer.
    Param: 
        pPkt - pointer to a wpt_packet.
    Return:
        NULL - fail.
        Otherwise the address of the starting of the buffer
---------------------------------------------------------------------------*/
wpt_status wpalPacketSetRxLength(wpt_packet *pPkt, wpt_uint32 len)
{
   // Validate the parameter pointers
   if (unlikely(NULL == pPkt))
   {
      WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_ERROR,
                "%s : NULL packet pointer", __func__);
      return eWLAN_PAL_STATUS_E_INVAL;
   }

   /*Only allowed for RX Raw packets */
   if( (eWLAN_PAL_PKT_TYPE_RX_RAW != WPAL_PACKET_GET_TYPE(pPkt)))
   {
     WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_ERROR, 
                "%s  Invalid packet type(%d)",  __func__,
                WPAL_PACKET_GET_TYPE(pPkt));
     return eWLAN_PAL_STATUS_E_INVAL;
   }

   if(VOS_IS_STATUS_SUCCESS(vos_pkt_set_rx_length(WPAL_TO_VOS_PKT(pPkt), len)))
   {
      return eWLAN_PAL_STATUS_SUCCESS;
   }
   else
   {
      return eWLAN_PAL_STATUS_E_INVAL;
   }
}/*wpalPacketSetRxLength*/

/*
  Set of helper functions that will prepare packet for DMA transfer,
  based on the type of transfer : - to and from the device
  - following these calls the packet will be locked for DMA only,
  CPU will not be able to modify it => the packet must be explicitly returned to
  the CPU once the DMA transfer is complete
*/
WPT_STATIC WPT_INLINE void* itGetOSPktAddrForDevice( wpt_packet *pPacket )
{
   struct sk_buff *skb;
   struct device *wcnss_device = (struct device *)gContext.devHandle;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   if ( VOS_STATUS_SUCCESS != 
        vos_pkt_get_os_packet(WPAL_TO_VOS_PKT(pPacket), (void**)&skb, VOS_FALSE ))
   {
     return NULL;
   }
   else
   {
     /*Map skb data into dma-able memory 
       (changes will be commited from cache) */
     return (void*)dma_map_single( wcnss_device, skb->data, skb->len, DMA_TO_DEVICE );
   }
}/*itGetOSPktAddrForDevice*/

WPT_STATIC WPT_INLINE void* itGetOSPktAddrFromDevice( wpt_packet *pPacket )
{
   struct sk_buff *skb;
   struct device *wcnss_device = (struct device *)gContext.devHandle;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   if ( VOS_STATUS_SUCCESS != 
        vos_pkt_get_os_packet(WPAL_TO_VOS_PKT(pPacket), (void**)&skb, VOS_FALSE ))
   {
     return NULL;
   }
   else
   {
     if((uintptr_t)skb->data == (uintptr_t)skb->tail)
     {
#ifdef WLAN_BUG_ON_SKB_ERROR
       wpalDevicePanic();
#else
       WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_FATAL,
                "%s: skb->data == skb->tail. Attempting recovery "
                "skb:%p, head:%p, tail:%p, data:%p",
                  __func__, skb, skb->head, skb->tail, skb->data);

      skb->data = skb->head;
#endif
     }
     /*Map skb data into dma-able memory 
       (changes will be commited from cache) */
     return (void*)dma_map_single( wcnss_device, skb->data, skb->len, DMA_FROM_DEVICE );
   }
}/*itGetOSPktAddrFromDevice*/

/*
  Set of helper functions that will return a DMA-ed packet to the CPU,
  based on the type of transfer : - to and from the device
*/
WPT_STATIC WPT_INLINE void itReturnOSPktAddrForDevice( wpt_packet *pPacket,  void* addr, wpt_uint32 size )
{
   struct device *wcnss_device = (struct device *)gContext.devHandle;

   dma_unmap_single( wcnss_device, (dma_addr_t)addr, size, DMA_TO_DEVICE );
}

WPT_STATIC WPT_INLINE void itReturnOSPktAddrFromDevice( wpt_packet *pPacket, void* addr, wpt_uint32 size  )
{
   struct device *wcnss_device = (struct device *)gContext.devHandle;

   dma_unmap_single( wcnss_device, (dma_addr_t)addr, size, DMA_FROM_DEVICE );
}


/*---------------------------------------------------------------------------
    wpalIteratorInit – Initialize an interator by updating pCur to first item.
    Param: 
        pIter – pointer to a caller allocated wpt_iterator
        pPacket – pointer to a wpt_packet
    Return:
        eWLAN_PAL_STATUS_SUCCESS - success
---------------------------------------------------------------------------*/
wpt_status wpalIteratorInit(wpt_iterator *pIter, wpt_packet *pPacket)
{
   wpt_status         status     = eWLAN_PAL_STATUS_SUCCESS;
   wpt_iterator_info* pCurInfo   = NULL;
   wpt_iterator_info* pNextInfo  = NULL;
   wpt_iterator_info* pPktInfo   = NULL;

   // Validate the parameter pointers
   if (unlikely((NULL == pPacket)||(NULL==pIter)))
   {
      WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_ERROR,
                "%s : NULL input pointers %p %p", __func__, pPacket, pIter);
      return eWLAN_PAL_STATUS_E_INVAL;
   }

   pPktInfo = (wpt_iterator_info*)pPacket->pInternalData;
   if (unlikely(NULL == pPktInfo))
   {
      WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_ERROR,
                "%s : Invalid Packet Info", __func__);
      return eWLAN_PAL_STATUS_E_INVAL;
   }

   // if there is NO BD on this frame, then initialize the next pointer to
   // point the first fragment.
   if ( NULL == WPAL_PACKET_GET_BD_PHYS(pPacket) )
   {
     pCurInfo   = pPktInfo;
     pNextInfo           = NULL;
   }
   else
   {
     /*Allocate memory for the current info*/
     pCurInfo = wpalMemoryAllocate( sizeof(wpt_iterator_info) );

     // Validate the memory allocation
     if (unlikely(NULL == pCurInfo))
     {
        WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "%s : Failed to allocate memory ", __func__);
        return eWLAN_PAL_STATUS_E_INVAL;
     }

     pCurInfo->pPhyAddr = WPAL_PACKET_GET_BD_PHYS(pPacket);
     pCurInfo->uLen     = WPAL_PACKET_GET_BD_LENGTH(pPacket);

     pNextInfo           = pPktInfo;
   }      

   pIter->pCur     = (void*)pCurInfo; 
   pIter->pNext    = (void*)pNextInfo;
   pIter->pContext = NULL;

   return status;
}/*wpalIteratorInit*/

/*---------------------------------------------------------------------------
    wpalIteratorNext – Get the address for the next item
    Param: 
        pIter – pointer to a caller allocated wpt_iterator
        pPacket – pointer to a wpt_packet
        ppAddr – Caller allocated pointer to return the address of the item.
        For DMA-able devices, this is the physical address of the item.
        pLen – To return the number of bytes in the item.
    Return:
        eWLAN_PAL_STATUS_SUCCESS - success
---------------------------------------------------------------------------*/
wpt_status wpalIteratorNext(wpt_iterator *pIter, wpt_packet *pPacket, void **ppAddr, wpt_uint32 *pLen)
{
   wpt_iterator_info* pCurInfo  = NULL;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
   
   /*-------------------------------------------------------------------------
     Sanity check
   -------------------------------------------------------------------------*/
   if (unlikely(( NULL == pIter )||( NULL == pPacket ) || 
      ( NULL == ppAddr ) || ( NULL == pLen )))
   {
     WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_ERROR, 
                "%s  Invalid input parameters",  __func__ );
     return eWLAN_PAL_STATUS_E_INVAL;
   }

   pCurInfo = (wpt_iterator_info*)pIter->pCur; 
   /*-------------------------------------------------------------------------
     If current pointer is NULL - there is no data in the packet - return
   -------------------------------------------------------------------------*/
   if( pIter->pCur == NULL )
   {
      *ppAddr = NULL; 
      *pLen   = 0;
      return eWLAN_PAL_STATUS_SUCCESS;
   }

   /*Address and length are kept in the current field*/
   *ppAddr = pCurInfo->pPhyAddr; 
   *pLen   = pCurInfo->uLen;
    
   if( NULL == pIter->pNext )
   {
     /*Save the iterator for cleanup*/
     pPacket->pInternalData = pIter->pCur; 
     pIter->pCur            = NULL; 
   }
   else
   {
     /*Release the memory saved for storing the BD information*/
     wpalMemoryFree(pCurInfo); 
  
     /*For LA - the packet is represented by maximum 2 fields of data 
       - BD and actual data from sk buff */
     pIter->pCur     = pIter->pNext;
     pIter->pNext    = NULL;
   }
   
   return eWLAN_PAL_STATUS_SUCCESS;
}

/*---------------------------------------------------------------------------
    wpalLockPacketForTransfer – Map the data buffer from dma so that the
                         data is commited from cache and the cpu relinquishes
                         ownership of the buffer
 
    Param: 
        pPacket – pointer to a wpt_packet
 
    Return:
        eWLAN_PAL_STATUS_SUCCESS - success
---------------------------------------------------------------------------*/
wpt_status wpalLockPacketForTransfer( wpt_packet *pPacket)
{
   void*              pPhyData   = NULL;
   wpt_iterator_info* pInfo      = NULL;
   v_U16_t            uLenData   = 0;

   // Validate the parameter pointers
   if (unlikely(NULL == pPacket))
   {
      WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_ERROR,
                "%s : NULL input pointer", __func__);
      return eWLAN_PAL_STATUS_E_INVAL;
   }

   switch(WPAL_PACKET_GET_TYPE(pPacket))
   {
      /* For management frames, BD is allocated by WDI, header is in raw buffer,
         rest of the frame is also in raw buffer */
   case eWLAN_PAL_PKT_TYPE_TX_802_11_MGMT:
      {
         /*TX Packets need to be DMA-ed to the device, perform DMA mapping 
           accordingly */
         pPhyData = (void*)itGetOSPktAddrForDevice( pPacket );   
      }
      break;
         /* Data packets - BD (allocated by WDI), header (in VOSS header),
            rest of the packet (DSM items) */
   case eWLAN_PAL_PKT_TYPE_TX_802_11_DATA:
   case eWLAN_PAL_PKT_TYPE_TX_802_3_DATA:
      {
         /*TX Packets need to be DMA-ed to the device, perform DMA mapping 
           accordingly */
         pPhyData = (void*)itGetOSPktAddrForDevice( pPacket );
      }
      break;

      /* For Raw RX, BD + header + rest of the packet is all contained in the raw
         buffer */
   case eWLAN_PAL_PKT_TYPE_RX_RAW:
      {
         /*RX Packets need to be DMA-ed from the device, perform DMA mapping 
           accordingly */
         pPhyData = (void*)itGetOSPktAddrFromDevice( pPacket );
      }
      break;

   default:
      {
         WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_ERROR, 
                    " WLAN_PAL: %s: Invalid packet type %d!",  __func__, 
                    WPAL_PACKET_GET_TYPE(pPacket) ); 
         WPAL_ASSERT(0); 
         return eWLAN_PAL_STATUS_E_FAILURE;
      }
   }

   /*Get packet length*/
   vos_pkt_get_packet_length(WPAL_TO_VOS_PKT(pPacket),&uLenData);

    /*Allocate memory for the current info*/
   pInfo = wpalMemoryAllocate( sizeof(wpt_iterator_info) );

   // Validate the memory allocation
   if (unlikely(NULL == pInfo))
   {
      WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_ERROR,
                "%s : Failed to allocate memory ", __func__);
      return eWLAN_PAL_STATUS_E_INVAL;
   }

   pInfo->pPhyAddr = pPhyData;
   pInfo->uLen     = uLenData;

   pPacket->pInternalData = pInfo;
   return eWLAN_PAL_STATUS_SUCCESS;
}/*wpalLockPacketForTransfer*/

/*---------------------------------------------------------------------------
    wpalUnlockPacket – Unmap the data buffer from dma so that cpu can regain
                          ownership on it
    Param: 
        pPacket – pointer to a wpt_packet
 
    Return:
        eWLAN_PAL_STATUS_SUCCESS - success
---------------------------------------------------------------------------*/
wpt_status wpalUnlockPacket( wpt_packet *pPacket)
{

   wpt_iterator_info* pInfo;

   // Validate the parameter pointers
   if (unlikely(NULL == pPacket))
   {
      WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_ERROR,
                "%s : NULL input pointer pPacket", __func__);
      return eWLAN_PAL_STATUS_E_INVAL;
   }

   pInfo  = (wpt_iterator_info*)pPacket->pInternalData;

   // Validate pInfo
   if (unlikely(NULL == pInfo))
   {
      WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_FATAL,
                "%s : NULL input pointer pInfo", __func__);
      return eWLAN_PAL_STATUS_E_INVAL;
   }

   switch(WPAL_PACKET_GET_TYPE(pPacket))
   {
      /* For management frames, BD is allocated by WDI, header is in raw buffer,
         rest of the frame is also in raw buffer */
   case eWLAN_PAL_PKT_TYPE_TX_802_11_MGMT:
      {
         /*TX Packets need to be DMA-ed to the device, perform DMA mapping 
           accordingly */
        itReturnOSPktAddrForDevice(pPacket, pInfo->pPhyAddr, pInfo->uLen);   
      }
      break;
         /* Data packets - BD (allocated by WDI), header (in VOSS header),
            rest of the packet (DSM items) */
   case eWLAN_PAL_PKT_TYPE_TX_802_11_DATA:
   case eWLAN_PAL_PKT_TYPE_TX_802_3_DATA:
      {
         /*TX Packets need to be DMA-ed to the device, perform DMA mapping 
           accordingly */
         itReturnOSPktAddrForDevice(pPacket, pInfo->pPhyAddr, pInfo->uLen);   
      }
      break;

      /* For Raw RX, BD + header + rest of the packet is all contained in the raw
         buffer */
   case eWLAN_PAL_PKT_TYPE_RX_RAW:
      {
         /*RX Packets need to be DMA-ed from the device, perform DMA mapping 
           accordingly */
         if(NULL == pInfo->pPhyAddr)
         {
            WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_ERROR, 
                       " WLAN_PAL: %s: RX frame was not locked properly",  __func__); 
         }
         else
         {
            itReturnOSPktAddrFromDevice(pPacket, pInfo->pPhyAddr, pInfo->uLen);   
         }
      }
      break;

   default:
      {
         WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_ERROR, 
                    " WLAN_PAL: %s: Invalid packet type %d!",  __func__, 
                    WPAL_PACKET_GET_TYPE(pPacket) ); 
         WPAL_ASSERT(0); 
         return eWLAN_PAL_STATUS_E_FAILURE;
      }
   }

  wpalMemoryFree(pInfo);
  pPacket->pInternalData = NULL;
  return eWLAN_PAL_STATUS_SUCCESS;
}/*wpalUnlockPacket*/

/*---------------------------------------------------------------------------
    wpalIsPacketLocked –  Check whether the Packet is locked for DMA.
    Param: 
        pPacket – pointer to a wpt_packet
 
    Return:
        eWLAN_PAL_STATUS_SUCCESS
        eWLAN_PAL_STATUS_E_FAILURE
        eWLAN_PAL_STATUS_E_INVAL
---------------------------------------------------------------------------*/
wpt_status wpalIsPacketLocked( wpt_packet *pPacket)
{

   wpt_iterator_info* pInfo;

   /* Validate the parameter pointers */
   if (NULL == pPacket)
   {
      WPAL_TRACE(eWLAN_MODULE_PAL, eWLAN_PAL_TRACE_LEVEL_WARN,
                "%s : NULL input pointer", __func__);
      return eWLAN_PAL_STATUS_E_INVAL;
   }

   /* Validate pInternalData */
   pInfo  = (wpt_iterator_info*)pPacket->pInternalData;
   return (NULL == pInfo)? eWLAN_PAL_STATUS_E_FAILURE : 
                    eWLAN_PAL_STATUS_SUCCESS;
}/*wpalIsPacketLocked*/

/*---------------------------------------------------------------------------
   wpalGetNumRxRawPacket   Query available RX RAW total buffer count
   param:
       numRxResource  pointer of queried value

   return:
       eWLAN_PAL_STATUS_SUCCESS
---------------------------------------------------------------------------*/
wpt_status wpalGetNumRxRawPacket(wpt_uint32 *numRxResource)
{
   *numRxResource = (wpt_uint32)vos_pkt_get_num_of_rx_raw_pkts();

   return eWLAN_PAL_STATUS_SUCCESS;
}

/*---------------------------------------------------------------------------
   wpalGetNumRxPacketAllocFailures   Get number of times packet alloc failed
       numRxResource  pointer of queried value

   return:
       eWLAN_PAL_STATUS_SUCCESS
---------------------------------------------------------------------------*/
wpt_status wpalGetNumRxPacketAllocFailures(wpt_uint32 *numRxResource)
{
   *numRxResource = (wpt_uint32)vos_pkt_get_num_of_rx_pkt_alloc_failures();

   return eWLAN_PAL_STATUS_SUCCESS;
}
/*---------------------------------------------------------------------------
   wpalGetNumRxFreePacket   Query available RX Free buffer count
   param:
       numRxResource  pointer of queried value

   return:
       WPT_STATUS
---------------------------------------------------------------------------*/
wpt_status wpalGetNumRxFreePacket(wpt_uint32 *numRxResource)
{
   VOS_STATUS status;

   status = vos_pkt_get_available_buffer_pool(VOS_PKT_TYPE_RX_RAW,
                                              (v_SIZE_t *)numRxResource);

   return WPAL_VOS_TO_WPAL_STATUS(status);
}

/*---------------------------------------------------------------------------
    wpalPacketStallUpdateInfo – Update each channel information when stall
       detected, also power state and free resource count

    Param:
       powerState  ? WLAN system power state when stall detected
       numFreeBd   ? Number of free resource count in HW
       channelInfo ? Each channel specific information when stall happen
       channelNum  ? Channel number update information

    Return:
       NONE

---------------------------------------------------------------------------*/
void wpalPacketStallUpdateInfo
(
   v_U32_t                         *powerState,
   v_U32_t                         *numFreeBd,
   wpt_log_data_stall_channel_type *channelInfo,
   v_U8_t                           channelNum
)
{
   /* Update power state when stall detected */
   if(NULL != powerState)
   {
      wpalTrasportStallInfo.PowerState = *powerState;
   }

   /* Update HW free resource count */
   if(NULL != numFreeBd)
   {
      wpalTrasportStallInfo.numFreeBd  = *numFreeBd;
   }

   /* Update channel information */
   if(NULL != channelInfo)
   {
      wpalMemoryCopy(&wpalTrasportStallInfo.dxeChannelInfo[channelNum],
                     channelInfo,
                     sizeof(wpt_log_data_stall_channel_type));
   }

   return;
}

#ifdef FEATURE_WLAN_DIAG_SUPPORT
/*---------------------------------------------------------------------------
    wpalPacketStallDumpLog – Trigger to send log packet to DIAG
       Updated transport system information will be sent to DIAG

    Param:
        NONE

    Return:
        NONE

---------------------------------------------------------------------------*/
void wpalPacketStallDumpLog
(
   void
)
{
   vos_log_data_stall_type  *log_ptr = NULL;

   WLAN_VOS_DIAG_LOG_ALLOC(log_ptr, vos_log_data_stall_type, LOG_TRSP_DATA_STALL_C);
   if(log_ptr)
   {
      log_ptr->PowerState = wpalTrasportStallInfo.PowerState;
      log_ptr->numFreeBd  = wpalTrasportStallInfo.numFreeBd;
      wpalMemoryCopy(&log_ptr->dxeChannelInfo[0],
                     &wpalTrasportStallInfo.dxeChannelInfo[0],
                     WPT_NUM_TRPT_CHANNEL * sizeof(vos_log_data_stall_channel_type));
      pr_info("Stall log dump");
      WLAN_VOS_DIAG_LOG_REPORT(log_ptr);
   }

   return;
}
#endif /* FEATURE_WLAN_DIAG_SUPPORT */
