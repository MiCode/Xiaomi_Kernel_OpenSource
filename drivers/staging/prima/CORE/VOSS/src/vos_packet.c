/*
 * Copyright (c) 2012-2016 The Linux Foundation. All rights reserved.
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

  \file        vos_packet.c

  \brief       virtual Operating System Services (vOSS) network Packet APIs

   Network Protocol packet/buffer support interfaces


  ========================================================================*/

/* $Header$ */

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include <vos_packet.h>
#include <i_vos_packet.h>
#include <vos_timer.h>
#include <vos_trace.h>
#include <wlan_hdd_main.h>   
#include <linux/wcnss_wlan.h>

/*--------------------------------------------------------------------------
  Preprocessor definitions and constants
  ------------------------------------------------------------------------*/
/* Protocol specific packet tracking feature */
#define VOS_PKT_PROT_ETH_TYPE_OFFSET 12
#define VOS_PKT_PROT_IP_OFFSET       14
#define VOS_PKT_PROT_IP_HEADER_SIZE  20
#define VOS_PKT_PROT_DHCP_SRV_PORT   67
#define VOS_PKT_PROT_DHCP_CLI_PORT   68
#define VOS_PKT_PROT_EAPOL_ETH_TYPE  0x888E
#define VOS_PKT_PROT_ARP_ETH_TYPE    0x0806
#define VOS_PKT_GET_HEAD(skb)        (skb->head)
#define VOS_PKT_GET_END(skb)         (skb->end)

/*--------------------------------------------------------------------------
  Type declarations
  ------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
  Data definitions
  ------------------------------------------------------------------------*/
static vos_pkt_context_t *gpVosPacketContext;

/*-------------------------------------------------------------------------
  Function declarations and documentation
  ------------------------------------------------------------------------*/

static VOS_STATUS vos_pkti_packet_init( struct vos_pkt_t *pPkt,
                                 VOS_PKT_TYPE pktType )
{
   VOS_STATUS vosStatus = VOS_STATUS_SUCCESS;

   // fixed fields
   pPkt->packetType = pktType;
   pPkt->magic = VPKT_MAGIC_NUMBER;

   // some packet types need an attached skb
   switch (pktType)
   {
   case VOS_PKT_TYPE_RX_RAW:
   case VOS_PKT_TYPE_TX_802_11_MGMT:
      // these need an attached skb.
      // we preallocate a fixed-size skb and reserve the entire buffer
      // as headroom since that is what other components expect
      pPkt->pSkb = alloc_skb(VPKT_SIZE_BUFFER , in_interrupt()? GFP_ATOMIC : GFP_KERNEL);
      if (likely(pPkt->pSkb))
      {
         skb_reserve(pPkt->pSkb, VPKT_SIZE_BUFFER);
      }
      else
      {
         vosStatus = VOS_STATUS_E_NOMEM;
      }

      /* Init PAL Packet */
      WPAL_PACKET_SET_BD_POINTER(&(pPkt->palPacket), NULL);
      WPAL_PACKET_SET_BD_PHYS(&(pPkt->palPacket), NULL);
      WPAL_PACKET_SET_BD_LENGTH(&(pPkt->palPacket), 0);
      WPAL_PACKET_SET_OS_STRUCT_POINTER(&(pPkt->palPacket), NULL);

      break;
   default:
      // no attached skb needed
      break;
   }

   return vosStatus;
}



static VOS_STATUS vos_pkti_list_destroy( struct list_head *pList )
{
   struct vos_pkt_t *pVosPacket;

   if (unlikely(NULL == pList))
   {
      // something is fishy -- don't even bother trying 
      // clean up this list since it is apparently hosed
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: NULL pList", __LINE__);
      VOS_ASSERT(0);
      return VOS_STATUS_E_INVAL;
   }

   list_for_each_entry(pVosPacket, pList, node)
   {

      // is this really an initialized vos packet?
      if (unlikely(VPKT_MAGIC_NUMBER != pVosPacket->magic))
      {
         // no, so don't try any deinitialization on it, and
         // since we can't trust the linkages, stop trying
         // to destroy the list
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                   "VPKT [%d]: Invalid magic", __LINE__);
         VOS_ASSERT(0);
         break;
      }

      // does this vos packet have an skb attached?
      if (pVosPacket->pSkb)
      {
         // yes, so give it back to the kernel
         kfree_skb(pVosPacket->pSkb);
         pVosPacket->pSkb = NULL;
      }

      // the vos packet itself is a static portion of the vos packet context
      // so there is no deallocation we have to do with it.  just clear the
      // magic so we no longer think it is valid
      pVosPacket->magic = 0;

   }

   // all nodes of the list have been processed so reinitialize the list
   INIT_LIST_HEAD(pList);

   return VOS_STATUS_SUCCESS;
}


static void vos_pkti_replenish_raw_pool(void)
{
   struct sk_buff * pSkb;
   struct vos_pkt_t *pVosPacket;
   v_BOOL_t didOne = VOS_FALSE;
   vos_pkt_get_packet_callback callback;

   // if there are no packets in the replenish pool then we can't do anything
   mutex_lock(&gpVosPacketContext->rxReplenishListLock);
   if (likely(0 == gpVosPacketContext->rxReplenishListCount))
   {
      mutex_unlock(&gpVosPacketContext->rxReplenishListLock);
      return;
   }

   // we only replenish if the Rx Raw pool is empty or the Replenish pool
   // reaches a high water mark
   mutex_lock(&gpVosPacketContext->rxRawFreeListLock);

   if ((gpVosPacketContext->rxReplenishListCount <
        gpVosPacketContext->numOfRxRawPackets/4) &&
         (!list_empty(&gpVosPacketContext->rxRawFreeList)))
   {
      mutex_unlock(&gpVosPacketContext->rxRawFreeListLock);
      mutex_unlock(&gpVosPacketContext->rxReplenishListLock);
      return;
   }

   VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
             "VPKT [%d]: Packet replenish activated", __LINE__);

   // try to replenish all of the packets in the replenish pool
   while (gpVosPacketContext->rxReplenishListCount)
   {
      // we preallocate a fixed-size skb and reserve the entire buffer
      // as headroom since that is what other components expect
      pSkb = alloc_skb(VPKT_SIZE_BUFFER, GFP_ATOMIC);
      if (unlikely(NULL == pSkb))
      {
         gpVosPacketContext->rxReplenishFailCount++;
         break;
      }
      skb_reserve(pSkb, VPKT_SIZE_BUFFER);

      // remove a vos packet from the replenish pool
      pVosPacket = list_first_entry(&gpVosPacketContext->rxReplenishList,
                                    struct vos_pkt_t, node);
      list_del(&pVosPacket->node);
      gpVosPacketContext->rxReplenishListCount--;

      // attach the skb to the vos packet
      pVosPacket->pSkb = pSkb;

      // add it to the Rx Raw Free Pool
      list_add_tail(&pVosPacket->node, &gpVosPacketContext->rxRawFreeList);
      gpVosPacketContext->rxRawFreeListCount++;

      didOne = VOS_TRUE;

      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
                "VPKT [%d]: [%p] Packet replenished",
                __LINE__, pVosPacket);

   }

   // if we replenished anything and if there is a callback waiting
   // then invoke the callback
   if ((VOS_TRUE == didOne) &&
       (gpVosPacketContext->rxRawLowResourceInfo.callback))
   {
      // remove the first record from the free pool
      pVosPacket = list_first_entry(&gpVosPacketContext->rxRawFreeList,
                                    struct vos_pkt_t, node);
      list_del(&pVosPacket->node);
      gpVosPacketContext->rxRawFreeListCount--;

      // clear out the User Data pointers in the voss packet..
      memset(&pVosPacket->pvUserData, 0, sizeof(pVosPacket->pvUserData));

      // initialize the 'chain' pointer to NULL.
      pVosPacket->pNext = NULL;

      // timestamp the vos packet.
      pVosPacket->timestamp = vos_timer_get_system_ticks();

      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
                "VPKT [%d]: [%p] Packet replenish callback",
                __LINE__, pVosPacket);

      callback = gpVosPacketContext->rxRawLowResourceInfo.callback;
      gpVosPacketContext->rxRawLowResourceInfo.callback = NULL;
      mutex_unlock(&gpVosPacketContext->rxRawFreeListLock);
      mutex_unlock(&gpVosPacketContext->rxReplenishListLock);
      callback(pVosPacket, gpVosPacketContext->rxRawLowResourceInfo.userData);
   }
   else
   {
      mutex_unlock(&gpVosPacketContext->rxRawFreeListLock);
      mutex_unlock(&gpVosPacketContext->rxReplenishListLock);
   }
}


#if defined( WLAN_DEBUG )
static char *vos_pkti_packet_type_str(VOS_PKT_TYPE pktType)
{
   switch (pktType)
   {
   case VOS_PKT_TYPE_TX_802_11_MGMT:
      return "TX_802_11_MGMT";
      break;
   
   case VOS_PKT_TYPE_TX_802_11_DATA:
      return  "TX_802_11_DATA";
      break;
       
   case VOS_PKT_TYPE_TX_802_3_DATA:
      return "TX_802_3_DATA";
      break;
   
   case VOS_PKT_TYPE_RX_RAW:
      return "RX_RAW";
      break;

   default:
      return "UNKNOWN";
      break;
   } 
}
#endif // defined( WLAN_DEBUG )

/*---------------------------------------------------------------------------

  \brief vos_packet_open() - initialize the vOSS Packet module

  The \a vos_packet_open() function initializes the vOSS Packet
  module.

  \param  pVosContext - pointer to the global vOSS Context

  \param  pVosPacketContext - pointer to a previously allocated
          buffer big enough to hold the vos Packet context.

  \param  vosPacketContextSize - the size allocated for the vos
          packet context.

  \return VOS_STATUS_SUCCESS - vos Packet module was successfully
          initialized and is ready to be used.

          VOS_STATUS_E_RESOURCES - System resources (other than memory)
          are unavailable to initilize the vos Packet module

          VOS_STATUS_E_NOMEM - insufficient memory exists to initialize
          the vos packet module

          VOS_STATUS_E_INVAL - Invalid parameter passed to the vos open
          function

          VOS_STATUS_E_FAILURE - Failure to initialize the vos packet
          module

  \sa vos_packet_close()

  -------------------------------------------------------------------------*/
VOS_STATUS vos_packet_open( v_VOID_t *pVosContext,
                            vos_pkt_context_t *pVosPacketContext,
                            v_SIZE_t vosPacketContextSize )
{
   VOS_STATUS vosStatus = VOS_STATUS_SUCCESS;
   unsigned int freePacketIndex;
   unsigned int idx;
   struct vos_pkt_t *pPkt;
   struct list_head *pFreeList;

   VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO, "Enter:%s",__func__);

   do
   {

      if (NULL == pVosContext)
      {
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                   "VPKT [%d]: NULL pVosContext", __LINE__);
         vosStatus = VOS_STATUS_E_INVAL;
         break;
      }

      if (NULL == pVosPacketContext)
      {
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                   "VPKT [%d]: NULL pVosPacketContext", __LINE__);
         vosStatus = VOS_STATUS_E_INVAL;
         break;
      }

      if (sizeof(vos_pkt_context_t) != vosPacketContextSize)
      {
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                   "VPKT [%d]: invalid vosPacketContextSize, %zu vs %d",
                   __LINE__, sizeof(vos_pkt_context_t), vosPacketContextSize);
         vosStatus = VOS_STATUS_E_INVAL;
         break;
      }

      // clear the vos packet context.  in the process this will
      // initialize the low resource info blocks
      memset(pVosPacketContext, 0, vosPacketContextSize);

      // save a global pointer to the vos packet context.
      gpVosPacketContext = pVosPacketContext;

      // save the vos Context pointer in the vos Packet Context.
      pVosPacketContext->vosContext = pVosContext;

      // initialize the rx Replenish pool (initially empty)
      mutex_init(&gpVosPacketContext->rxReplenishListLock);
      INIT_LIST_HEAD(&pVosPacketContext->rxReplenishList);
      pVosPacketContext->rxReplenishListCount = 0;

      // index into the packet context's vosPktBuffer[] array
      freePacketIndex = 0;

      // initialize the rxRaw free list pool
      mutex_init(&gpVosPacketContext->rxRawFreeListLock);
      pFreeList = &pVosPacketContext->rxRawFreeList;
      pVosPacketContext->rxRawFreeListCount = 0;
      INIT_LIST_HEAD(pFreeList);

      pVosPacketContext->numOfRxRawPackets = vos_pkt_get_num_of_rx_raw_pkts();

      // fill the rxRaw free list
      for (idx = 0; idx < pVosPacketContext->numOfRxRawPackets; idx++)
      {
         pPkt = &pVosPacketContext->vosPktBuffers[freePacketIndex++];
         vosStatus = vos_pkti_packet_init(pPkt, VOS_PKT_TYPE_RX_RAW);

         WPAL_PACKET_SET_METAINFO_POINTER(&(pPkt->palPacket),
                  (void*)&pVosPacketContext->rxMetaInfo[idx]);
         WPAL_PACKET_SET_TYPE(&(pPkt->palPacket), 
                              eWLAN_PAL_PKT_TYPE_RX_RAW);

         if (VOS_STATUS_SUCCESS != vosStatus)
         {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                      "VPKT [%d]: Packet init failure", __LINE__);
            break;
         }
         list_add_tail(&pPkt->node, pFreeList);
         pVosPacketContext->rxRawFreeListCount++;
      }

      // exit if any problems so far
      if (VOS_STATUS_SUCCESS != vosStatus)
      {
         break;
      }

      // initialize the txData free list pool
      mutex_init(&gpVosPacketContext->txDataFreeListLock);
      pFreeList = &pVosPacketContext->txDataFreeList;
      INIT_LIST_HEAD(pFreeList);

      // fill the txData free list
      for (idx = 0; idx < VPKT_NUM_TX_DATA_PACKETS; idx++)
      {
         pPkt = &pVosPacketContext->vosPktBuffers[freePacketIndex++];
         vosStatus = vos_pkti_packet_init(pPkt, VOS_PKT_TYPE_TX_802_3_DATA);
         WPAL_PACKET_SET_METAINFO_POINTER(&(pPkt->palPacket),
               (void*)&pVosPacketContext->txDataMetaInfo[idx]);
         WPAL_PACKET_SET_TYPE(&(pPkt->palPacket), 
                              eWLAN_PAL_PKT_TYPE_TX_802_3_DATA);
         if (VOS_STATUS_SUCCESS != vosStatus)
         {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                      "VPKT [%d]: Packet init failure", __LINE__);
            break;
         }
         list_add_tail(&pPkt->node, pFreeList);
         pVosPacketContext->uctxDataFreeListCount++;
      }

      // exit if any problems so far
      if (VOS_STATUS_SUCCESS != vosStatus)
      {
         break;
      }

      // initialize the txMgmt free list pool
      mutex_init(&gpVosPacketContext->txMgmtFreeListLock);
      pFreeList = &pVosPacketContext->txMgmtFreeList;
      INIT_LIST_HEAD(pFreeList);

      // fill the txMgmt free list
      for (idx = 0; idx < VPKT_NUM_TX_MGMT_PACKETS; idx++)
      {
         pPkt = &pVosPacketContext->vosPktBuffers[freePacketIndex++];

         vosStatus = vos_pkti_packet_init(pPkt, VOS_PKT_TYPE_TX_802_11_MGMT);

         WPAL_PACKET_SET_METAINFO_POINTER(&(pPkt->palPacket),
               (void*)&pVosPacketContext->txMgmtMetaInfo[idx]);
         WPAL_PACKET_SET_TYPE(&(pPkt->palPacket), 
                              eWLAN_PAL_PKT_TYPE_TX_802_11_MGMT);

         if (VOS_STATUS_SUCCESS != vosStatus)
         {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                      "VPKT [%d]: Packet init failure", __LINE__);
            break;
         }
         list_add_tail(&pPkt->node, pFreeList);
      }

      // exit if any problems so far
      if (VOS_STATUS_SUCCESS != vosStatus)
      {
         break;
      }

   } while (0);

   return vosStatus;
}



/*---------------------------------------------------------------------------

  \brief vos_packet_close() - Close the vOSS Packet module

  The \a vos_packet_close() function closes the vOSS Packet module
  Upon successful close all resources allocated from the OS will be
  relinquished.

  \param  pVosContext - pointer to the global vOSS Context

  \return VOS_STATUS_SUCCESS - Packet module was successfully closed.

          VOS_STATUS_E_INVAL - Invalid parameter passed to the packet
          close function

          VOS_STATUS_E_FAILURE - Failure to close the vos Packet module

  \sa vos_packet_open()

  ---------------------------------------------------------------------------*/
VOS_STATUS vos_packet_close( v_PVOID_t pVosContext )
{

   VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO, "Enter:%s",__func__);

   if (unlikely(NULL == pVosContext))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: NULL pVosContext", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   if (unlikely(gpVosPacketContext->vosContext != pVosContext))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: invalid pVosContext", __LINE__);
      return VOS_STATUS_E_INVAL;
   }


   mutex_lock(&gpVosPacketContext->txMgmtFreeListLock);
   (void) vos_pkti_list_destroy(&gpVosPacketContext->txMgmtFreeList);
   mutex_unlock(&gpVosPacketContext->txMgmtFreeListLock);

   mutex_lock(&gpVosPacketContext->txDataFreeListLock);
   (void) vos_pkti_list_destroy(&gpVosPacketContext->txDataFreeList);
   gpVosPacketContext->uctxDataFreeListCount = 0;
   mutex_unlock(&gpVosPacketContext->txDataFreeListLock);

   mutex_lock(&gpVosPacketContext->rxRawFreeListLock);
   (void) vos_pkti_list_destroy(&gpVosPacketContext->rxRawFreeList);
   gpVosPacketContext->rxRawFreeListCount    = 0;
   mutex_unlock(&gpVosPacketContext->rxRawFreeListLock);

   mutex_lock(&gpVosPacketContext->rxReplenishListLock);
   (void) vos_pkti_list_destroy(&gpVosPacketContext->rxReplenishList);
   gpVosPacketContext->rxReplenishListCount  = 0;
   mutex_unlock(&gpVosPacketContext->rxReplenishListLock);



   return VOS_STATUS_SUCCESS;
}


/**--------------------------------------------------------------------------

  \brief vos_pkt_get_packet() - Get a voss Packet

  Gets a voss Packet from an internally managed packet pool.

  \param ppPacket - pointer to location where the voss Packet pointer is
                    returned.  If multiple packets are requested, they
                    will be chained onto this first packet.

  \param pktType - the packet type to be retreived.  Valid packet types are:
     <ul>
       <li> VOS_PKT_TYPE_TX_802_11_MGMT - voss packet is for Transmitting
            802.11 Management frames.

       <li> VOS_PKT_TYPE_RX_RAW - voss Packet contains a buffer for Receiving
            raw frames of unknown type.
     </ul>

  \param dataSize - the Data size needed in the voss Packet.

  \param numPackets - the number of packets requested.

  \param zeroBuffer - parameter that tells the API to zero the data buffer
                      in this voss Packet.
     <ul>
       <li> VOS_TRUE - the API will zero out the entire data buffer.

       <li> VOS_FALSE - the API will not zero out the data buffer.
     </ul>

  \note If enough room for headers to transmit or receive the packet is not
        available, this API will fail.

  \param callback - This callback function, if provided, is invoked in the
         case when resources are not available at the time of the call to
         get packets.  This callback function is invoked when packets are
         available.

  \param userData - This user data is passed back to the caller in the
         callback function, if the callback is invoked.

  \return VOS_STATUS_SUCCESS - the API was able to get a vos_packet for the
          requested type.  *ppPacket contains a pointer to the packet.

          VOS_STATUS_E_INVAL - pktType is not a valid packet type.  This
          API only supports getting vos packets for 802_11_MGMT and
          RX_RAW packet types.  This status is also returned if the
          numPackets or dataSize are invalid.

          VOS_STATUS_E_RESOURCES - unable to get resources needed to get
          a vos packet.  If a callback function is specified and this
          status is returned from the API, the callback will be called
          when resources are available to fulfill the original request.

          Note that the low resources condition is indicated to the caller
          by returning VOS_STATUS_E_RESOURCES.  This status is the only
          non-success status that indicates to the caller that the callback
          will be called when resources are available.  All other status
          indicate failures that are not recoverable and the 'callback'
          will not be called.

          VOS_STATUS_E_FAILURE - The API returns this status when unable
          to get a packet from the packet pool because the pool
          is depleted and the caller did not specify a low resource callback.

          VOS_STATUS_E_ALREADY - This status is returned when the VOS
          packet pool is in a 'low resource' condition and cannot
          accomodate any more calls to retrieve packets from that
          pool.   Note this is a FAILURE and the 'low resource' callback
          will *not* be called.

          VOS_STATUS_E_FAULT - ppPacket does not specify a valid pointer.

  \sa

  ------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_get_packet( vos_pkt_t **ppPacket,
                               VOS_PKT_TYPE pktType,
                               v_SIZE_t dataSize,
                               v_SIZE_t numPackets,
                               v_BOOL_t zeroBuffer,
                               vos_pkt_get_packet_callback callback,
                               v_VOID_t *userData )
{
   struct list_head *pPktFreeList;
   vos_pkt_low_resource_info *pLowResourceInfo;
   struct vos_pkt_t *pVosPacket;
   v_SIZE_t *pCount = NULL;
   struct mutex *mlock;

   // Validate the return parameter pointer
   if (unlikely(NULL == ppPacket))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: NULL ppPacket", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // we only support getting 1 packet at this time (as do WM & AMSS)
   if (unlikely(1 != numPackets))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "VPKT [%d]: invalid numPackets, %d", __LINE__, numPackets);
      return VOS_STATUS_E_INVAL;
   }

   // Validate the dataSize is within range
   if (unlikely((0 == dataSize) || (dataSize > VPKT_SIZE_BUFFER)))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "VPKT [%d]: invalid dataSize, %d", __LINE__, dataSize);
      return VOS_STATUS_E_INVAL;
   }

   // determine which packet pool and low resource block we should use.
   // this API is only valid for TX MGMT and RX RAW packets
   // (TX DATA will use vos_pkt_wrap_data_packet())
   switch (pktType)
   {

   case VOS_PKT_TYPE_RX_RAW:
      pPktFreeList = &gpVosPacketContext->rxRawFreeList;
      pLowResourceInfo = &gpVosPacketContext->rxRawLowResourceInfo;
      mlock = &gpVosPacketContext->rxRawFreeListLock;

      // see if we need to replenish the Rx Raw pool
      vos_pkti_replenish_raw_pool();
      pCount = &gpVosPacketContext->rxRawFreeListCount;

      break;

   case VOS_PKT_TYPE_TX_802_11_MGMT:
      pPktFreeList = &gpVosPacketContext->txMgmtFreeList;
      pLowResourceInfo = &gpVosPacketContext->txMgmtLowResourceInfo;
      mlock = &gpVosPacketContext->txMgmtFreeListLock;
      break;

   default:
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "VPKT [%d]: invalid packet type %d[%s]",
                __LINE__, pktType, vos_pkti_packet_type_str(pktType));
      return VOS_STATUS_E_INVAL;
   }

   // is there already a low resource callback registered for this pool?
   // we only support one callback per pool, so if one is already registered
   // then we know we are already in a low-resource condition
   if (unlikely(pLowResourceInfo->callback))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_WARN,
                "VPKT [%d]: Low resource handler already registered",
                __LINE__);
      return VOS_STATUS_E_ALREADY;
   }

   mutex_lock(mlock);
   // are there vos packets on the associated free pool?
   if (unlikely(list_empty(pPktFreeList)))
   {
      // allocation failed
      // did the caller specify a callback?
      if (unlikely(NULL == callback))
      {
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                   "VPKT [%d]: Low resource condition and no callback provided",
                   __LINE__);
         mutex_unlock(mlock);

         return VOS_STATUS_E_FAILURE;
      }

      // save the low resource information so that we can invoke the
      // callback when a packet becomes available
      pLowResourceInfo->callback   = callback;
      pLowResourceInfo->userData   = userData;
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_WARN,
                "VPKT [%d]: Low resource condition for packet type %d[%s]",
                __LINE__, pktType, vos_pkti_packet_type_str(pktType));
      mutex_unlock(mlock);

      return VOS_STATUS_E_RESOURCES;
   }

   // remove the first record from the free pool
   pVosPacket = list_first_entry(pPktFreeList, struct vos_pkt_t, node);
   list_del(&pVosPacket->node);
   if (NULL != pCount)
   {
      (*pCount)--;
   }
   mutex_unlock(mlock);

   // clear out the User Data pointers in the voss packet..
   memset(&pVosPacket->pvUserData, 0, sizeof(pVosPacket->pvUserData));

   // initialize the 'chain' pointer to NULL.
   pVosPacket->pNext = NULL;

   // set the packet type.
   pVosPacket->packetType = pktType;

   // timestamp the vos packet.
   pVosPacket->timestamp = vos_timer_get_system_ticks();

   // zero the data buffer if the user asked for it to be cleared.
   if (unlikely(zeroBuffer))
   {
      memset(pVosPacket->pSkb->head,
             0,
             skb_end_pointer(pVosPacket->pSkb) - pVosPacket->pSkb->head);
   }

   VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
             "VPKT [%d]: [%p] Packet allocated, type %d[%s]",
             __LINE__, pVosPacket, pktType, vos_pkti_packet_type_str(pktType));

   *ppPacket = pVosPacket;
   return VOS_STATUS_SUCCESS;
}


/**--------------------------------------------------------------------------

  \brief vos_pkt_wrap_data_packets() - Wrap an OS provided data packet in a
         vos packet.

  Takes as input an OS provided data packet and 'wraps' that packet in a
  vos_packet, returning the vos_packet to the caller.

  This function is intended to be called from the HDD to wrap Tx data packets
  from the OS into vos_packets before sending them to TL for transmission.

  \param ppPacket - pointer to location where the voss Packet pointer is
                    returned.  If multiple packets are requested, they
                    will be chained onto this first packet.

  \param pktType - the packet type to be retreived.  Valid packet types are:
     <ul>
       <li> VOS_PKT_TYPE_802_3_DATA - voss packet is for Transmitting 802.3
            data frames.

       <li> VOS_PKT_TYPE_802_11_DATA - voss Packet is for Transmitting 802.11
            data frames.
     </ul>

  \param pOSPacket - a pointer to the Transmit packet provided by the OS.  This
         OS provided packet will be wrapped into a vos_packet_t.  Note this
         OS packet pointer can be NULL.  The OS packet pointer can be inserted
         into a VOS packet of type VOS_PKT_TYPE_802_3_DATA or
         VOS_PKT_TYPE_802_11_DATA through vos_pkt_set_os_packet().

  \note If enough room for headers to transmit or receive the packet is not
        available, this API will fail.

  \param callback - This callback function, if provided, is invoked in the
         case where packets are not available at the time of the call to
         return the packets to the caller (a 'low resource' condition).

         When packets become available, the callback callback function is
         invoked to return a VOS packet to the caller.  Note that the
         OS Packet is *not* inserted into the VOS packet when it is returned
         to the low resource callback.  It is up to the caller to insert
         the OS packet into the VOS packet by calling vos_pkt_set_os_packet()

  \param userData - This user data is passed back to the caller in the
         callback function, if the callback is invoked.

  \return VOS_STATUS_SUCCESS - the API was able to get a vos_packet for the
          requested type.  *ppPacket contains a pointer to the packet.

          VOS_STATUS_E_INVAL - pktType is not a valid packet type.  This
          API only supports getting vos packets for 802_11_MGMT and
          RX_RAW packet types.

          VOS_STATUS_E_RESOURCES - unable to get resources needed to get
          a vos packet.  If a callback function is specified and this
          status is returned from the API, the callback will be called
          when resources are available to fulfill the original request.

          Note that the low resources condition is indicated to the caller
          by returning VOS_STATUS_E_RESOURCES.  This status is the only
          non-success status that indicates to the caller that the callback
          will be called when resources are available.  All other status
          indicate failures that are not recoverable and the 'callback'
          will not be called.

          VOS_STATUS_E_FAILURE - The API returns this status when unable
          to get a packet from the packet pool because the pool
          is depleted and the caller did not specify a low resource callback.

          VOS_STATUS_E_ALREADY - This status is returned when the VOS
          packet pool is in a 'low resource' condition and cannot
          accomodate any more calls to retrieve packets from that
          pool.   Note this is a FAILURE and the 'low resource' callback
          will *not* be called.

          VOS_STATUS_E_FAULT - ppPacket or pOSPacket do not specify valid
          pointers.

  \sa vos_pkt_set_os_packet()

  ------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_wrap_data_packet( vos_pkt_t **ppPacket,
                                     VOS_PKT_TYPE pktType,
                                     v_VOID_t *pOSPacket,
                                     vos_pkt_get_packet_callback callback,
                                     v_VOID_t *userData )
{
   struct list_head *pPktFreeList;
   vos_pkt_low_resource_info *pLowResourceInfo;
   struct vos_pkt_t *pVosPacket;
   struct mutex *mlock;

   // Validate the return parameter pointer
   if (unlikely(NULL == ppPacket))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: NULL ppPacket", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // Validate the packet type.  Only Tx Data packets can have an OS
   // packet attached to them (Tx Mgmt and Rx Raw have OS packets
   // pre-attached to them)
   if (unlikely(VOS_PKT_TYPE_TX_802_3_DATA != pktType))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "VPKT [%d]: invalid pktType %d", __LINE__, pktType);
      return VOS_STATUS_E_INVAL;
   }

   // determine which packet pool and low resource block we should use.
   pPktFreeList = &gpVosPacketContext->txDataFreeList;
   pLowResourceInfo = &gpVosPacketContext->txDataLowResourceInfo;
   mlock = &gpVosPacketContext->txDataFreeListLock;

   mutex_lock(mlock);

   // is there already a low resource callback registered for this pool?
   // we only support one callback per pool, so if one is already registered
   // then we know we are already in a low-resource condition
   if (unlikely(pLowResourceInfo->callback))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_WARN,
                "VPKT [%d]: Low resource handler already registered",
                __LINE__);
      mutex_unlock(mlock);
      return VOS_STATUS_E_ALREADY;
   }

   // are there vos packets on the associated free pool?
   if (unlikely(list_empty(pPktFreeList)))
   {
      // allocation failed
      // did the caller specify a callback?
      if (unlikely(NULL == callback))
      {
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                   "VPKT [%d]: Low resource condition and no callback provided",
                   __LINE__);
         mutex_unlock(mlock);
         return VOS_STATUS_E_FAILURE;
      }

      // save the low resource information so that we can invoke the
      // callback when a packet becomes available
      pLowResourceInfo->callback   = callback;
      pLowResourceInfo->userData   = userData;
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_WARN,
                "VPKT [%d]: Low resource condition for pool %s",
                __LINE__, vos_pkti_packet_type_str(pktType));
      mutex_unlock(mlock);
      return VOS_STATUS_E_RESOURCES;
   }

   // remove the first record from the free pool
   pVosPacket = list_first_entry(pPktFreeList, struct vos_pkt_t, node);
   list_del(&pVosPacket->node);
   gpVosPacketContext->uctxDataFreeListCount --;
   mutex_unlock(mlock);

   // clear out the User Data pointers in the voss packet..
   memset(&pVosPacket->pvUserData, 0, sizeof(pVosPacket->pvUserData));

   // initialize the 'chain' pointer to NULL.
   pVosPacket->pNext = NULL;

   // set the packet type.
   pVosPacket->packetType = pktType;

   // set the skb pointer
   pVosPacket->pSkb = (struct sk_buff *) pOSPacket;

   // timestamp the vos packet.
   pVosPacket->timestamp = vos_timer_get_system_ticks();

   VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
             "VPKT [%d]: [%p] Packet allocated, type %s",
             __LINE__, pVosPacket, vos_pkti_packet_type_str(pktType));

   *ppPacket = pVosPacket;
   return VOS_STATUS_SUCCESS;
}



/*---------------------------------------------------------------------------

  \brief vos_pkt_set_os_packet() - set the OS packet in a VOS data packet

  This API inserts an OS packet into a previously retreived VOS packet.
  This API only applies to VOS packets of type VOS_PKT_TYPE_802_3_DATA or
  VOS_PKT_TYPE_802_11_DATA.

  There are cases where a user will need to get a VOS data packet without
  having the OS packet to insert/wrap into the data packet. This could happen
  if the user calls vos_pkt_wrap_data_packet() without the OS packet.

  Also, when the user hit a 'low resource' situation for data packets, the
  low resource callback is going to return a VOS packet without an OS packet
  attached to it.  The caller who gets the packet through the low resource
  callback uses this API to insert an OS packet into the VOS packet that
  was returned through the low resource callback.

  \param pPacket - the voss Packet to insert the OS packet into.

  \param pOSPacket - a pointer to the Transmit packet provided by the OS.  This
         OS provided packet will be wrapped into a vos_packet_t.  Note this
         OS packet pointer can be NULL.  The OS packet pointer can be inserted
         into a VOS packet of type VOS_PKT_TYPE_802_3_DATA or
         VOS_PKT_TYPE_802_11_DATA through vos_pkt_set_os_packet().

         Caller beware.  If there is a valid OS packet wrapped into this
         VOS packet already, this API will blindly overwrite the OS packet
         with the new one specified on this API call.  If you need to determine
         or retreive the current OS packet from a VOS packet, call
         vos_pkt_get_os_packet() first.

  \return VOS_STATUS_SUCCESS - the API was able to insert the OS packet into
          the vos_packet.

          VOS_STATUS_E_INVAL - pktType is not a valid packet type.  This
          API only supports getting vos packets of type
          VOS_PKT_TYPE_802_3_DATA or VOS_PKT_TYPE_802_11_DATA.

          VOS_STATUS_E_FAULT - pPacket does not specify a valid pointer.

  \sa vos_pkt_get_os_packet()

  ---------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_set_os_packet( vos_pkt_t *pPacket,
                                  v_VOID_t *pOSPacket )
{
   // Validate the input parameter pointers
   if (unlikely((NULL == pPacket)||(NULL == pOSPacket)))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: NULL pointer", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // Validate that this really an initialized vos packet
   if (unlikely(VPKT_MAGIC_NUMBER != pPacket->magic))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: Invalid magic", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // Validate the packet type.  Only Tx Data packets can have an OS
   // packet attached to them (Tx Mgmt and Rx Raw have OS packets
   // pre-attached to them)
   if (unlikely(VOS_PKT_TYPE_TX_802_3_DATA != pPacket->packetType))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "VPKT [%d]: invalid packet type %d[%s]",
                __LINE__, pPacket->packetType,
                vos_pkti_packet_type_str(pPacket->packetType));
      return VOS_STATUS_E_INVAL;
   }

   // Is there already a packet attached?  If so, just warn and continue
   if (unlikely(pPacket->pSkb))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_WARN,
                "VPKT [%d]: Packet previously attached", __LINE__);
   }

   // attach
   pPacket->pSkb = (struct sk_buff *) pOSPacket;
   
   return VOS_STATUS_SUCCESS;
}


/*---------------------------------------------------------------------------

  \brief vos_pkt_get_os_packet() - get the OS packet in a VOS data packet

  This API returns the OS packet that is inserted in a VOS packet.
  This API only applies to VOS packets of type VOS_PKT_TYPE_802_3_DATA or
  VOS_PKT_TYPE_802_11_DATA.

  \param pPacket - the voss Packet to return the OS packet from.

  \param ppOSPacket - a pointer to the location where the OS packet pointer
         retreived from the VOS packet will be returned.  Note this OS packet
         pointer can be NULL, meaning there is no OS packet attached to this
         VOS data packet.

  \param clearOSPacket - a boolean value that tells the API to clear out the
         OS packet pointer from the VOS packet.  Setting this to 'true' will
         essentially remove the OS packet from the VOS packet.  'false' means
         the OS packet remains chained to the VOS packet.  In either case,
         the OS packet pointer is returned to the caller.

  \return VOS_STATUS_SUCCESS - the API was able to retreive the OS packet
          pointer from the VOS packet and return it to the caller in
          *ppOsPacket

          VOS_STATUS_E_INVAL - pktType is not a valid packet type.  This
          API only supports getting vos packets of type
          VOS_PKT_TYPE_802_3_DATA or VOS_PKT_TYPE_802_11_DATA.

          VOS_STATUS_E_FAULT - pPacket or ppOsPacket does not specify a valid
          pointers.

  \sa vos_pkt_set_os_packet(), vos_pkt_wrap_data_packet(),
      vos_pkt_return_packet()

  ---------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_get_os_packet( vos_pkt_t *pPacket,
                                  v_VOID_t **ppOSPacket,
                                  v_BOOL_t clearOSPacket )
{
   // Validate the input and output parameter pointers
   if (unlikely((NULL == pPacket)||(NULL == ppOSPacket)))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: NULL pointer", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // Validate that this really an initialized vos packet
   if (unlikely(VPKT_MAGIC_NUMBER != pPacket->magic))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: Invalid magic", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // get OS packet pointer
   *ppOSPacket = (v_VOID_t *) pPacket->pSkb;

   // clear it?
   if (clearOSPacket)
   {
      pPacket->pSkb = NULL;
   }

   return VOS_STATUS_SUCCESS;
}

/**--------------------------------------------------------------------------

  \brief vos_pkt_get_user_data_ptr() - return a pointer to user data area
                                       of a voss Packet

  This API returns a pointer to a specified user Data area in the voss
  Packet.  User data areas are uniqua areas of the voss Packet that can
  be used by specific components to store private data.  These areas are
  identified by a user "ID" and should only be accessed by the component
  specified.

  \param pPacket - the voss Packet to retreive the user data pointer from.

  \param userID - the identifier for the user data area in the voss Packet
                  to get.

                  User IDs and user data areas in the voss Packet are
                  available for:
                  - Transport Layer (TL)
                  - Bus Abstraction Layer (BAL)
                  - SDIO Services Component (SSC)
                  - Host Device Driver (HDD)

  \param ppUserData - pointer to location to return the pointer to the user
                      data.

  \return - Nothing.

  \sa

  ---------------------------------------------------------------------------*/
v_VOID_t vos_pkt_get_user_data_ptr( vos_pkt_t *pPacket,
                                    VOS_PKT_USER_DATA_ID userID,
                                    v_VOID_t **ppUserData )
{
   // Validate the input and output parameter pointers
   if (unlikely(NULL == pPacket))
   {
       VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
               "VPKT [%d]: NULL pointer", __LINE__);
       if (ppUserData != NULL)
       {
           *ppUserData = NULL;
       }
       return;
   }

   // Validate that this really an initialized vos packet
   if (unlikely(VPKT_MAGIC_NUMBER != pPacket->magic))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: Invalid magic", __LINE__);
      *ppUserData = NULL;
      return;
   }

   // Validate userID
   if (unlikely(userID >= VOS_PKT_USER_DATA_ID_MAX))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "VPKT [%d]: Invalid user ID [%d]", __LINE__, userID);
      *ppUserData = NULL;
      return;
   }

   // retreive the user data pointer from the vos Packet and
   // return it to the caller.
   *ppUserData = pPacket->pvUserData[userID];

   return;
}


/**--------------------------------------------------------------------------

  \brief vos_pkt_set_user_data_ptr() - set the user data pointer of a voss
                                        Packet

  This API sets a pointer in the specified user Data area in the voss
  Packet.  User data areas are uniqua areas of the voss Packet that can
  be used by specific components to store private data.  These areas are
  identified by a user "ID" and should only be accessed by the component
  specified.

  Note:  The size of the user data areas in the voss Packet are fixed.  The
         size of a single pointer is available in each user area.

  \param pPacket - the voss Packet to set the user pointer.

  \param userID - the identifier for the user data area in the voss Packet
                  to set.

                  User IDs and user data areas in the voss Packet are
                  available for:
                  - Transport Layer (TL)
                  - Bus Abstraction Layer (BAL)
                  - SDIO Services Component (SSC)
                  - Host Device Driver (HDD)

  \param pUserData - pointer value to set in the user data area of the voss
                     packet..

  \return - Nothing.

  \sa

  ---------------------------------------------------------------------------*/
v_VOID_t vos_pkt_set_user_data_ptr( vos_pkt_t *pPacket,
                                    VOS_PKT_USER_DATA_ID userID,
                                    v_VOID_t *pUserData )
{
   // Validate the input parameter pointer
   if (unlikely(NULL == pPacket))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: NULL pointer", __LINE__);
      return;
   }

   // Validate that this is really an initialized vos packet
   if (unlikely(VPKT_MAGIC_NUMBER != pPacket->magic))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: Invalid magic", __LINE__);
      return;
   }

   // Validate userID
   if (unlikely(userID >= VOS_PKT_USER_DATA_ID_MAX))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "VPKT [%d]: Invalid user ID [%d]", __LINE__, userID);
      return;
   }

   // retreive the user data pointer from the vos Packet and
   // return it to the caller.
   pPacket->pvUserData[userID] = pUserData;

   return;
}

/**--------------------------------------------------------------------------

  \brief vos_pkt_return_packet() - Return a voss Packet (chain) to vOSS

  This API returns a voss Packet to the internally managed packet pool.

  Note:  If there are multiple packets chained to this packet, the entire
  packet chain is returned to vOSS.  The caller must unchain the
  packets through vos_pkt_get_next_packet() and return them individually
  if all packets in the packet chain are not to be returned.

  \param pPacket - the voss Packet(s) to return.  Note all packets chained
                   to this packet are returned to vOSS.

  \return

  \sa

  ---------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_return_packet( vos_pkt_t *pPacket )
{
   vos_pkt_t *pNext;
   struct list_head *pPktFreeList;
   vos_pkt_low_resource_info *pLowResourceInfo;
   vos_pkt_get_packet_callback callback;
   v_SIZE_t *pCount;
   VOS_PKT_TYPE packetType = VOS_PKT_TYPE_TX_802_3_DATA;
   v_BOOL_t lowResource;
   struct mutex * mlock;

   // Validate the input parameter pointer
   if (unlikely(NULL == pPacket))
   {
      return VOS_STATUS_E_INVAL;
   }

   // iterate though all packets in the chain
   while (pPacket)
   {
      // unlink this packet from the chain
      pNext = pPacket->pNext;
      pPacket->pNext = NULL;

      lowResource = VOS_FALSE;
      // Validate that this really an initialized vos packet
      if (unlikely(VPKT_MAGIC_NUMBER != pPacket->magic))
      {
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                   "VPKT [%d]: Invalid magic", __LINE__);
         return VOS_STATUS_E_INVAL;
      }

      //If an skb is attached then reset the pointers      
      if (pPacket->pSkb)
      {
         pPacket->pSkb->len = 0;
         pPacket->pSkb->data = pPacket->pSkb->head;
         skb_reset_tail_pointer(pPacket->pSkb);
         skb_reserve(pPacket->pSkb, VPKT_SIZE_BUFFER);
      }

      pCount = NULL;
      // determine which packet pool and low resource block we should use.
      switch (pPacket->packetType)
      {
      case VOS_PKT_TYPE_RX_RAW:
         // if this packet still has an skb attached, we can put it
         // back in the free pool, otherwise we need to put it in the
         // replenish pool
         if (pPacket->pSkb)
         {
            pPktFreeList = &gpVosPacketContext->rxRawFreeList;
            pLowResourceInfo = &gpVosPacketContext->rxRawLowResourceInfo;
            pCount = &gpVosPacketContext->rxRawFreeListCount;
            mlock = &gpVosPacketContext->rxRawFreeListLock;
         }
         else
         {
            pPktFreeList = &gpVosPacketContext->rxReplenishList;
            pLowResourceInfo = NULL;
            pCount = &gpVosPacketContext->rxReplenishListCount;
            mlock = &gpVosPacketContext->rxReplenishListLock;
         }
         packetType = VOS_PKT_TYPE_RX_RAW;
         break;

      case VOS_PKT_TYPE_TX_802_11_MGMT:
                
         pPktFreeList = &gpVosPacketContext->txMgmtFreeList;
         pLowResourceInfo = &gpVosPacketContext->txMgmtLowResourceInfo;
         mlock = &gpVosPacketContext->txMgmtFreeListLock;

         break;

      case VOS_PKT_TYPE_TX_802_3_DATA:
         pPktFreeList = &gpVosPacketContext->txDataFreeList;
         pLowResourceInfo = &gpVosPacketContext->txDataLowResourceInfo;
         mlock = &gpVosPacketContext->txDataFreeListLock;
         gpVosPacketContext->uctxDataFreeListCount ++;
         break;

      default:
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                   "VPKT [%d]: invalid packet type %d[%s]",
                   __LINE__, pPacket->packetType,
                   vos_pkti_packet_type_str(pPacket->packetType));

         return VOS_STATUS_E_INVAL;
      }


      // is there a low resource condition pending for this packet type?
      if (pLowResourceInfo && pLowResourceInfo->callback)
      {
         // pLowResourceInfo->callback is modified from threads (different CPU's). 
         // So a mutex is enough to protect is against a race condition.
         // mutex is SMP safe
         mutex_lock(mlock);
         callback = pLowResourceInfo->callback;
         pLowResourceInfo->callback = NULL;
         mutex_unlock(mlock);

         // only one context can get a valid callback
         if(callback)
         {
             // [DEBUG]
             VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,"VPKT [%d]: recycle %p",  __LINE__, pPacket);

             // yes, so rather than placing the packet back in the free pool
             // we will invoke the low resource callback
             VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
                       "VPKT [%d]: [%p] Packet recycled, type %d[%s]",
                       __LINE__, pPacket, pPacket->packetType,
                       vos_pkti_packet_type_str(pPacket->packetType));

             // clear out the User Data pointers in the voss packet..
             memset(&pPacket->pvUserData, 0, sizeof(pPacket->pvUserData));

             // initialize the 'chain' pointer to NULL.
             pPacket->pNext = NULL;

             // timestamp the vos packet.
             pPacket->timestamp = vos_timer_get_system_ticks();

             callback(pPacket, pLowResourceInfo->userData);

             // We did process low resource condition
             lowResource = VOS_TRUE;
         }
      }
      

      if(!lowResource)
      {
         // this packet does not satisfy a low resource condition
         // so put it back in the appropriate free pool
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
                   "VPKT [%d]: [%p] Packet returned, type %d[%s]",
                   __LINE__, pPacket, pPacket->packetType,
                   vos_pkti_packet_type_str(pPacket->packetType));
         mutex_lock(mlock);
         list_add_tail(&pPacket->node, pPktFreeList);

         if (pCount)
         {
            (*pCount)++;
         }
         mutex_unlock(mlock);
      }

      // move to next packet in the chain
      pPacket = pNext;

   } // while (pPacket)

   // see if we need to replenish the Rx Raw pool
   if (VOS_PKT_TYPE_RX_RAW == packetType)
   {
      vos_pkti_replenish_raw_pool();   
   }
   return VOS_STATUS_SUCCESS;
}

/**--------------------------------------------------------------------------

  \brief vos_pkt_chain_packet() - chain a voss Packet to another packet

  This API chains a voss Packet to another voss Packet, creating a packet
  chain.  Packets can be chained before or after the current packet in the
  packet chain.

  \param pPacket - pointer to a voss packet to chain to

  \param pChainPacket - pointer to packet to chain

  \param chainAfter - boolean to specify to chain packet after or before
                      the input packet
  <ul>
    <li> true - chain packet AFTER pPacket (chain behind)
    <li> false - chain packet BEFORE pPacket (chain in front)
  </ul>

  \return

  \sa

  ---------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_chain_packet( vos_pkt_t *pPacket,
                                 vos_pkt_t *pChainPacket,
                                 v_BOOL_t chainAfter )
{
   // Validate the parameter pointers
   if (unlikely((NULL == pPacket) || (NULL == pChainPacket)))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: NULL pointer", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // Validate that these are really initialized vos packets
   if (unlikely((VPKT_MAGIC_NUMBER != pPacket->magic) ||
                (VPKT_MAGIC_NUMBER != pChainPacket->magic)))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: Invalid magic", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // swap pointers if we chain before
   if (unlikely(VOS_FALSE == chainAfter))
   {
      vos_pkt_t *pTmp = pPacket;
      pPacket = pChainPacket;
      pChainPacket = pTmp;
   }

   // find the end of the chain
   while (pPacket->pNext)
   {
      pPacket = pPacket->pNext;
   }

   // attach
   pPacket->pNext = pChainPacket;

   return VOS_STATUS_SUCCESS;
}


/**--------------------------------------------------------------------------

  \brief vos_pkt_walk_packet_chain() - Walk packet chain and (possibly)
                                        unchain packets

  This API will walk the voss Packet and unchain the packet from the chain,
  if specified.  The 'next' packet in the packet chain is returned as the
  packet chain is traversed.

  \param  pPacket - input vos_packet walk

  \param ppChainedPacket - pointer to location to return the 'next' voss
                           packet pointer in the packet chain.
                           NULL means there is was not packet chained
                           to this packet.

  \param unchainPacket - Flag that specifies if the caller wants the packet
                         to be removed from the packet chain.  This is
                         provided to allow the caller to walk the packet chain
                         with or without breaking the chain.

    <ul>
      <li> true - when set 'true' the API will return
                  the 'next' packet pointer in the voss Packte chain and
                  *WILL* unchain the input packet from the packet chain.

      <li> NOT false - when set 'false' the API will return
                       the 'next' packet pointer in the voss Packet chain but
                       *WILL NOT* unchain the packet chain.   This option gives
                       the caller the ability to walk the packet chain without
                       modifying it in the process.
    </ul>

  \note Having the packets chained has an implicaiton on how the return
        packet API (vos_pkt_return_packet() ) operates.

  \return

  \sa

  ---------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_walk_packet_chain( vos_pkt_t *pPacket,
                                      vos_pkt_t **ppChainedPacket,
                                      v_BOOL_t unchainPacket )
{
   // Validate the parameter pointers
   if (unlikely((NULL == pPacket) || (NULL == ppChainedPacket)))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: NULL pointer", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // Validate that this is really an initialized vos packet
   if (unlikely(VPKT_MAGIC_NUMBER != pPacket->magic))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: Invalid magic", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // get next packet
   *ppChainedPacket = pPacket->pNext;

   // if asked to unchain, then unchain it
   if (VOS_FALSE != unchainPacket)
   {
      pPacket->pNext = NULL;
   }

   // if end of the chain, indicate empty to the caller
   if (*ppChainedPacket)
   {
      return VOS_STATUS_SUCCESS;
   }
   else
   {
      return VOS_STATUS_E_EMPTY;
   }
}

/**--------------------------------------------------------------------------

  \brief vos_pkt_get_data_vector() - Get data vectors from a voss Packet

  This API gets the complete set of Vectors (pointer / length pairs) that
  describe all of the data that makes up the voss Packet.

  \param pPacket - pointer to the vOSS Packet to get the pointer/length
                   vector from

  \param pVector - pointer to the vector array where the vectors are returned.

  \param pNumVectors - Number of vector's in the vector array (at pVector).
                       On successful return, *pNumVectors is updated with the
                       number of pointer/length vectors at pVector populated
                       with valid vectors.

  Call with NULL pVector or 0 vectorSize, will return the size of vector
  needed (in *pNumVectors)

  Caller allocates and frees the vector memory.

  \return

  \sa

  ---------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_get_data_vector( vos_pkt_t *pPacket,
                                    vos_pkt_data_vector_t *pVector,
                                    v_SIZE_t *pNumVectors )
{
   // not supported

   VOS_ASSERT(0);
   return VOS_STATUS_E_FAILURE;
}


/**--------------------------------------------------------------------------

  \brief vos_pkt_extract_data() - Extract data from the voss Packet.

  This API extracts data from a voss Packet, copying the data into the
  supplied output buffer.  Note the data is copied from the vos packet
  but the data remains in the vos packet and the size is unaffected.

  \param pPacket - the voss Packet to get the data from.

  \param pktOffset - the offset from the start of the voss Packet to get the
         data.  (i.e. 0 would be the beginning of the voss Packet).

  \param pOutputBuffer - Pointer to the location where the voss Packet data
         will be copied.

  \param pOutputBufferSize - on input, contains the amount of data to extract
         into the output buffer.  Upon return, contains the amount of data
         extracted into the output buffer.

         Note:  an input of 0 in *pOutputBufferSize, means to copy *all*
         data in the voss Packet into the output buffer.  The caller is
         responsible for assuring the output buffer size is big enough
         since the size of the buffer is not being passed in!

  \return

  \sa

  ---------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_extract_data( vos_pkt_t *pPacket,
                                 v_SIZE_t pktOffset,
                                 v_VOID_t *pOutputBuffer,
                                 v_SIZE_t *pOutputBufferSize )
{
   v_SIZE_t len;
   struct sk_buff *skb;

   // Validate the parameter pointers
   if (unlikely((NULL == pPacket) ||
                (NULL == pOutputBuffer) ||
                (NULL == pOutputBufferSize)))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: NULL pointer", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // Validate that this is really an initialized vos packet
   if (unlikely(VPKT_MAGIC_NUMBER != pPacket->magic))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: Invalid magic", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // get pointer to the skb
   skb = pPacket->pSkb;

   // Validate the skb
   if (unlikely(NULL == skb))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: NULL skb", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // get number of bytes requested
   len = *pOutputBufferSize;

   // if 0 is input in the *pOutputBufferSize, then the user wants us to 
   // extract *all* the data in the buffer.  Otherwise, the user has 
   // specified the output buffer size in *pOutputBufferSize.  In the 
   // case where the output buffer size is specified, let's validate that 
   // it is big enough.
   //
   // \note:  i'm not crazy about this.  we should enforce the output
   // buffer size on input so this API is not going to cause crashes
   // because buffers are too small and the caller inputs 0 == don't care
   // to check the size... !!
   if (0 == len)
   {
      len = skb->len - pktOffset;

      // return # of bytes copied
      *pOutputBufferSize = len;
   }
   else
   {
      // make sure we aren't extracting past the end of the packet
      if (len > (skb->len - pktOffset))
      {
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                   "VPKT [%d]: Request overrun, "
                   "req offset %d, req size %d, packet size %d",
                   __LINE__, pktOffset, len, skb->len);
         return VOS_STATUS_E_INVAL;
      }
   }

   // copy the data
   vos_mem_copy(pOutputBuffer, &skb->data[pktOffset], len);

   return VOS_STATUS_SUCCESS;
}

/**--------------------------------------------------------------------------

  \brief vos_pkt_extract_data_chain() - Extract data from a voss Packet chain.

  This API extracts *all* the data from a voss Packet chain, copying the
  data into the supplied output buffer.  Note the data is copied from
  the vos packet chain but the data remains in the vos packet and the
  size of the vos packets are unaffected.

  \param pPacket - the first voss Packet in the voss packet chain to
         extract data.

  \param pOutputBuffer - Pointer to the location where the voss Packet data
         will be copied.

  \param pOutputBufferSize - on input, contains the maximum amount of data
         that can be extracted into the output buffer.  Upon return, contains
         the amount of data extracted from the packet chain.

  \return VOS_STATUS_SUCCESS - the data from the entire packet chain is
          extracted and found at *pOutputBuffer.  *pOutputBufferSize bytes
          were extracted.

          VOS_STATUS_E_FAULT - pPacket, pOutputBuffer, or pOutputBufferSize
          is not a valid pointer.

          VOS_STATUS_E_NOMEM - there is not enough room to extract the data
          from the entire packet chain. *pOutputBufferSize has been updated
          with the size needed to extract the entier packet chain.

  \sa vos_pkt_extract_data()

  ---------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_extract_data_chain( vos_pkt_t *pPacket,
                                       v_VOID_t *pOutputBuffer,
                                       v_SIZE_t *pOutputBufferSize )
{
   VOS_STATUS vosStatus;
   v_SIZE_t len;
   struct sk_buff *skb;

   // Validate the parameter pointers
   if (unlikely((NULL == pPacket) ||
                (NULL == pOutputBuffer) ||
                (NULL == pOutputBufferSize)))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: NULL pointer", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // Validate that this is really an initialized vos packet
   if (unlikely(VPKT_MAGIC_NUMBER != pPacket->magic))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: Invalid magic", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // get the length of the entire packet chain.
   vosStatus = vos_pkt_get_packet_chain_length(pPacket, &len);
   if (unlikely(VOS_STATUS_SUCCESS != vosStatus))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "VPKT [%d]: Unable to get packet chain length", __LINE__);
      return VOS_STATUS_E_FAILURE;
   }

   // if the output buffer size is too small, return NOMEM and update
   // the actual size needed in *pOutputBufferSize
   if (len > *pOutputBufferSize)
   {
      *pOutputBufferSize = len;
      return VOS_STATUS_E_NOMEM;
   }

   // walk through each packet in the chain, copying the data
   while (pPacket)
   {
      // get pointer to the skb
      skb = pPacket->pSkb;

      // Validate the skb
      if (unlikely(NULL == skb))
      {
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                   "VPKT [%d]: NULL skb", __LINE__);
         return VOS_STATUS_E_INVAL;
      }

      vos_mem_copy(pOutputBuffer, skb->data, skb->len);
      pOutputBuffer += skb->len;

      pPacket = pPacket->pNext;
   }

   return VOS_STATUS_SUCCESS;
}

/**--------------------------------------------------------------------------

  \brief vos_pkt_peek_data() - peek into voss Packet at given offset

  This API provides a pointer to a specified offset into a voss Packet,
  allowing the caller to peek at a given number of bytes in the voss Packet.
  Upon successful return, the caller can access "numBytes" of data at
  "pPacketData".

  This API will fail if the data length requested to peek at is not in
  contiguous memory in the voss Packet.  In this case, the caller should
  use vos_pkt_extract_data() to have the data copied into a caller supplied
  buffer.

  \param pPacket - the vOSS Packet to peek into

  \param pktOffset - the offset into the voss Packet data to peek into.

  \param ppPacketData - pointer to the location where the pointer to the
                        packet data at pktOffset will be returned.

  \param numBytes - the number of bytes the caller wishes to peek at.

  \return

  \sa

  ---------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_peek_data( vos_pkt_t *pPacket,
                              v_SIZE_t pktOffset,
                              v_VOID_t **ppPacketData,
                              v_SIZE_t numBytes )
{
   struct sk_buff *skb;

   // Validate the parameter pointers
   if (unlikely((NULL == pPacket) ||
                (NULL == ppPacketData)))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: NULL pointer", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // Validate that this is really an initialized vos packet
   if (unlikely(VPKT_MAGIC_NUMBER != pPacket->magic))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: Invalid magic", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // Validate numBytes
   if (unlikely(0 == numBytes))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: Invalid numBytes", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // get pointer to the skb
   skb = pPacket->pSkb;

   // Validate the skb
   if (unlikely(NULL == skb))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: NULL skb", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // check for overflow
   if (unlikely((pktOffset + numBytes) > skb->len))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "VPKT [%d]: Packet overflow, offset %d size %d len %d",
                __LINE__, pktOffset, numBytes, skb->len);
      return VOS_STATUS_E_INVAL;
   }

   // return pointer to the requested data
   *ppPacketData = &skb->data[pktOffset];
   return VOS_STATUS_SUCCESS;
}


/**--------------------------------------------------------------------------

  \brief vos_pkt_get_packet_type() - Get packet type for a voss Packet

  This API returns the packet Type for a voss Packet.

  \param pPacket - the voss Packet to get the packet type from.

  \param pPacketType - location to return the packet type for the voss Packet

  \return

  \sa

  ---------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_get_packet_type( vos_pkt_t *pPacket,
                                    VOS_PKT_TYPE *pPacketType )
{
   // Validate the parameter pointers
   if (unlikely((NULL == pPacket) ||
                (NULL == pPacketType)))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: NULL pointer", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // Validate that this is really an initialized vos packet
   if (unlikely(VPKT_MAGIC_NUMBER != pPacket->magic))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: Invalid magic", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // return the requested information
   *pPacketType = pPacket->packetType;
   return VOS_STATUS_SUCCESS;
}



/**--------------------------------------------------------------------------

  \brief vos_pkt_get_packet_length() - Get packet length for a voss Packet

  This API returns the total length of the data in a voss Packet.

  \param pPacket - the voss Packet to get the packet length from.

  \param pPacketSize - location to return the total size of the data contained
                       in the voss Packet.
  \return

  \sa

  ---------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_get_packet_length( vos_pkt_t *pPacket,
                                      v_U16_t *pPacketSize )
{
   // Validate the parameter pointers
   if (unlikely((NULL == pPacket) ||
                (NULL == pPacketSize)))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: NULL pointer", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // Validate that this is really an initialized vos packet
   if (unlikely(VPKT_MAGIC_NUMBER != pPacket->magic))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: Invalid magic", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // Validate the skb
   if (unlikely(NULL == pPacket->pSkb))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: NULL skb", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // return the requested information
   *pPacketSize = pPacket->pSkb->len;
   return VOS_STATUS_SUCCESS;
}


/**--------------------------------------------------------------------------

  \brief vos_pkt_get_packet_chain_length() - Get length of a vos packet chain

  This API returns the total length of the data in a voss Packet chain.

  \param pPacket - the voss Packet at the start of the packet chain.  This API
         will calculate the length of data in the packet chain stating with
         this packet.

  \param pPacketSize - location to return the total size of the data contained
                       in the voss Packet.
  \return

  \sa

  ---------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_get_packet_chain_length( vos_pkt_t *pPacketChain,
                                            v_SIZE_t *pPacketChainSize )
{
   v_SIZE_t chainSize = 0;

   // Validate the parameter pointers
   if (unlikely((NULL == pPacketChain) ||
                (NULL == pPacketChainSize)))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: NULL pointer", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // walk through each packet in the chain, adding its length
   while (pPacketChain)
   {

      // Validate that this is really an initialized vos packet
      if (unlikely(VPKT_MAGIC_NUMBER != pPacketChain->magic))
      {
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                   "VPKT [%d]: Invalid magic", __LINE__);
         return VOS_STATUS_E_INVAL;
      }

      // Validate the skb
      if (unlikely(NULL == pPacketChain->pSkb))
      {
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                   "VPKT [%d]: NULL skb", __LINE__);
         return VOS_STATUS_E_INVAL;
      }

      chainSize += pPacketChain->pSkb->len;
      pPacketChain = pPacketChain->pNext;
   }

   // return result
   *pPacketChainSize = chainSize;
   return VOS_STATUS_SUCCESS;
}


/**--------------------------------------------------------------------------

  \brief vos_pkt_push_head() - push data on the front a of a voss Packet

  This API will push data onto the front of a voss Packet.  The data will be
  appended in front of any data already contained in the voss Packet.

  \param pPacket - the voss Packet to modify.

  \param pData - pointer to the data to push onto the head of the voss Packet.

  \param dataSize - the size of the data to put onto the head of the voss
                    Packet.

  \return

  \sa

  ---------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_push_head( vos_pkt_t *pPacket,
                              v_VOID_t *pData,
                              v_SIZE_t dataSize )
{
   struct sk_buff *skb;

   // Validate the parameter pointers
   if (unlikely((NULL == pPacket) || (NULL == pData)))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: NULL pointer", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // Validate that this is really an initialized vos packet
   if (unlikely(VPKT_MAGIC_NUMBER != pPacket->magic))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: Invalid magic", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // get pointer to the skb
   skb = pPacket->pSkb;

   // Validate the skb
   if (unlikely(NULL == skb))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: NULL skb", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // Make sure there is headroom.  As a performance optimization we
   // can omit this check later since skb_push() will also perform the
   // check (except skb_push() will panic the kernel)
   if (unlikely(skb_headroom(skb) < dataSize))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: Insufficient headroom, "
                "head[%p], data[%p], req[%d]",
                __LINE__, skb->head, skb->data, dataSize);
      return VOS_STATUS_E_INVAL;
   }

   // actually push the data
   vos_mem_copy(skb_push(skb, dataSize), pData, dataSize);

   return VOS_STATUS_SUCCESS;
}

/**--------------------------------------------------------------------------

  \brief vos_pkt_reserve_head() - Reserve space at the front of a voss Packet

  This API will reserve space at the front of a voss Packet.  The caller can
  then copy data into this reserved space using memcpy() like functions.  This
  allows the caller to reserve space and build headers directly in this
  reserved space in the voss Packet.

  Upon successful return, the length of the voss Packet is increased by
  dataSize.

  < put a before / after picture here>

  \param pPacket - the voss Packet to modify.

  \param ppData - pointer to the location where the pointer to the reserved
                  space is returned.  Upon successful return, the caller has
                  write / read access to the data space at *ppData for length
                  dataSize to build headers, etc.

  \param dataSize - the size of the data to reserve at the head of the voss
                    Packet.  Upon successful return, the length of the voss
                    Packet is increased by dataSize.

  \return

  \sa

  ---------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_reserve_head( vos_pkt_t *pPacket,
                                 v_VOID_t **ppData,
                                 v_SIZE_t dataSize )
{
   struct sk_buff *skb;
   struct sk_buff *newskb;

   // Validate the parameter pointers
   if (unlikely((NULL == pPacket) || (NULL == ppData)))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: NULL pointer", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // Validate that this is really an initialized vos packet
   if (unlikely(VPKT_MAGIC_NUMBER != pPacket->magic))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: Invalid magic", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // get pointer to the skb
   skb = pPacket->pSkb;

   // Validate the skb
   if (unlikely(NULL == skb))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: NULL skb", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // Make sure there is headroom.  As a performance optimization we
   // can omit this check later since skb_push() will also perform the
   // check (except skb_push() will panic the kernel)
   if (unlikely(skb_headroom(skb) < dataSize))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_WARN,
                "VPKT [%d]: Insufficient headroom, "
                "head[%p], data[%p], req[%d]",
                __LINE__, skb->head, skb->data, dataSize);
    
      if ((newskb = skb_realloc_headroom(skb, dataSize)) == NULL) {
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                   "VPKT [%d]: Failed to realloc headroom", __LINE__);
         return VOS_STATUS_E_INVAL;
      }

      kfree_skb(skb);
      skb = newskb;

      // set the skb pointer
      pPacket->pSkb = newskb;
   }

   // actually allocate the headroom
   *ppData = skb_push(skb, dataSize);
   // Zero out so we dont take the fastpath on Android.
   memset( (void *)*ppData, 0, dataSize );

   return VOS_STATUS_SUCCESS;
}


/**--------------------------------------------------------------------------

  \brief vos_pkt_reserve_head_fast() - Reserve space at the front of a voss Packet

  This API will reserve space at the front of a voss Packet.  The caller can
  then copy data into this reserved space using memcpy() like functions.  This
  allows the caller to reserve space and build headers directly in this
  reserved space in the voss Packet.

  Upon successful return, the length of the voss Packet is increased by
  dataSize.
 
  Same as above APi but no memset to 0 at the end.
 
  < put a before / after picture here>

  \param pPacket - the voss Packet to modify.

  \param ppData - pointer to the location where the pointer to the reserved
                  space is returned.  Upon successful return, the caller has
                  write / read access to the data space at *ppData for length
                  dataSize to build headers, etc.

  \param dataSize - the size of the data to reserve at the head of the voss
                    Packet.  Upon successful return, the length of the voss
                    Packet is increased by dataSize.

  \return

  \sa

  ---------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_reserve_head_fast( vos_pkt_t *pPacket,
                                 v_VOID_t **ppData,
                                 v_SIZE_t dataSize )
{
   struct sk_buff *skb;
   struct sk_buff *newskb;

   // get pointer to the skb
   skb = pPacket->pSkb;

   // Validate the skb
   if (unlikely(NULL == skb))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: NULL skb", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // Make sure there is headroom.  As a performance optimization we
   // can omit this check later since skb_push() will also perform the
   // check (except skb_push() will panic the kernel)
   if (unlikely(skb_headroom(skb) < dataSize))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_WARN,
                "VPKT [%d]: Insufficient headroom, "
                "head[%p], data[%p], req[%d]",
                __LINE__, skb->head, skb->data, dataSize);
    
      if ((newskb = skb_realloc_headroom(skb, dataSize)) == NULL) {
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                   "VPKT [%d]: Failed to realloc headroom", __LINE__);
         return VOS_STATUS_E_INVAL;
      }

      kfree_skb(skb);
      skb = newskb;

      // set the skb pointer
      pPacket->pSkb = newskb;
   }

   // actually allocate the headroom
   *ppData = skb_push(skb, dataSize);
   // Zero out so we dont take the fastpath on Android.
   //memset( (void *)*ppData, 0, dataSize );

   return VOS_STATUS_SUCCESS;
}

/**--------------------------------------------------------------------------

  \brief vos_pkt_pop_head() - Remove data from the front of the voss Packet

  This API removes data from the front of a voss Packet.  The data is
  copied into the output buffer described by pData and pDataSize

  \param pPacket - the voss Packet to operate on.

  \param pData - pointer to the data buffer where the data removed from the
                 voss Packet is placed.

  \param dataSize - The amount of space to remove from the head of the voss
                    Packet.  The output buffer (at *pData) must contain at
                    least this amount of space.

  \return

  \sa

  ---------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_pop_head( vos_pkt_t *pPacket,
                             v_VOID_t *pData,
                             v_SIZE_t dataSize )
{
   struct sk_buff *skb;

   // Validate the parameter pointers
   if (unlikely((NULL == pPacket) || (NULL == pData)))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: NULL pointer", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // Validate that this is really an initialized vos packet
   if (unlikely(VPKT_MAGIC_NUMBER != pPacket->magic))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: Invalid magic", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // get pointer to the skb
   skb = pPacket->pSkb;

   // Validate the skb
   if (unlikely(NULL == skb))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: NULL skb", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // Make sure there is enough data to pop
   if (unlikely(skb->len < dataSize))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_WARN,
                "VPKT [%d]: pop exceeds packet size, len[%d], req[%d]",
                __LINE__, skb->len, dataSize);
      return VOS_STATUS_E_INVAL;
   }

   // copy the data
   vos_mem_copy(pData, skb->data, dataSize);
   skb_pull(skb, dataSize);

   return VOS_STATUS_SUCCESS;
}


/**--------------------------------------------------------------------------

  \brief vos_pkt_trim_head() - Skip over bytes at the front of a voss Packet

  This API moves the pointers at the head of a voss Packet to essentially
  skip over data at the front of a voss Packet.  Upon successful return, the
  length of the voss Packet is reduced by dataSize and the starting pointer
  to the voss Packet is adjusted to eliminate the data from the start of the
  voss Packet.

  This API has the opposite effect of \a vos_pkt_reserve_head().

  \param pPacket - the voss Packet to operate on.

  \param dataSize - The amount of space to skip at the start of the voss
                    Packet.

                    Note that upon return, the data skipped over is
                    inaccessible to the caller.  If the caller needs access
                    to the head data, use vos_pkt_pop_head() or
                    vos_pkt_extract_data() to get a copy of the data.

  \return

  \sa

  ---------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_trim_head( vos_pkt_t *pPacket,
                              v_SIZE_t dataSize )
{
   struct sk_buff *skb;

   // Validate the parameter pointers
   if (unlikely(NULL == pPacket))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: NULL pointer", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // Validate that this is really an initialized vos packet
   if (unlikely(VPKT_MAGIC_NUMBER != pPacket->magic))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: Invalid magic", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // get pointer to the skb
   skb = pPacket->pSkb;

   // Validate the skb
   if (unlikely(NULL == skb))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: NULL skb", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // Make sure there is enough data to trim
   if (unlikely(skb->len < dataSize))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: trim exceeds packet size, len[%d], req[%d]",
                __LINE__, skb->len, dataSize);
      return VOS_STATUS_E_INVAL;
   }

   // adjust the skb
   skb_pull(skb, dataSize);

   return VOS_STATUS_SUCCESS;
}

/**--------------------------------------------------------------------------

  \brief vos_pkt_push_tail() - push data on the end a of a voss Packet

  This API will push data onto the end of a voss Packet.  The data will be
  appended to the end of any data already contained in the voss Packet.

  \param pPacket - the voss Packet to modify.

  \param pData - pointer to the data to push onto the tail of the voss Packet.

  \param dataSize - the size of the data to put onto the tail of the voss Packet.

  \return

  \sa

  ---------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_push_tail( vos_pkt_t *pPacket,
                              v_VOID_t *pData,
                              v_SIZE_t dataSize )
{
   struct sk_buff *skb;

   // Validate the parameter pointers
   if (unlikely((NULL == pPacket) || (NULL == pData)))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: NULL pointer", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // Validate that this is really an initialized vos packet
   if (unlikely(VPKT_MAGIC_NUMBER != pPacket->magic))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: Invalid magic", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // get pointer to the skb
   skb = pPacket->pSkb;

   // Validate the skb
   if (unlikely(NULL == skb))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: NULL skb", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // Make sure there is tailroom.  As a performance optimization we
   // can omit this check later since skb_put() will also perform the
   // check (except skb_put() will panic the kernel)
   if (unlikely(skb_tailroom(skb) < dataSize))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: Insufficient tailroom, "
                "tail[%p], end[%p], req[%d]",
                __LINE__, skb_tail_pointer(skb),
                skb_end_pointer(skb), dataSize);
      return VOS_STATUS_E_INVAL;
   }

   // actually push the data
   vos_mem_copy(skb_put(skb, dataSize), pData, dataSize);

   return VOS_STATUS_SUCCESS;
}


/**--------------------------------------------------------------------------

  \brief vos_pkt_reserve_tail() - Reserve space at the end of a voss Packet

  This API will reserve space at the end of a voss Packet.  The caller can
  then copy data into this reserved space using memcpy() like functions.  This
  allows the caller to reserve space and build headers directly in this
  reserved space in the voss Packet.

  Upon successful return, the length of the voss Packet is increased by
  dataSize.

  \param pPacket - the voss Packet to modify.

  \param ppData - pointer to the location where the pointer to the reserved
                  space is returned.  Upon successful return, the caller has
                  write / read access to the data space at *ppData for length
                  dataSize to build headers, etc.

  \param dataSize - the size of the data to reserve at the head of the voss
                    Packet.  Upon successful return, the length of the voss
                    Packet is increased by dataSize.

  \return

  \sa

  ---------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_reserve_tail( vos_pkt_t *pPacket,
                                 v_VOID_t **ppData,
                                 v_SIZE_t dataSize )
{
   struct sk_buff *skb;

   // Validate the parameter pointers
   if (unlikely((NULL == pPacket) || (NULL == ppData)))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: NULL pointer", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // Validate that this is really an initialized vos packet
   if (unlikely(VPKT_MAGIC_NUMBER != pPacket->magic))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: Invalid magic", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // get pointer to the skb
   skb = pPacket->pSkb;

   // Validate the skb
   if (unlikely(NULL == skb))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: NULL skb", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // Make sure there is tailroom.  As a performance optimization we
   // can omit this check later since skb_put() will also perform the
   // check (except skb_put() will panic the kernel)
   if (unlikely(skb_tailroom(skb) < dataSize))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: Insufficient tailroom, "
                "tail[%p], end[%p], req[%d]",
                __LINE__, skb_tail_pointer(skb),
                skb_end_pointer(skb), dataSize);
      return VOS_STATUS_E_INVAL;
   }

   // actually allocate the space
   *ppData = skb_put(skb, dataSize);

   return VOS_STATUS_SUCCESS;
}



/**--------------------------------------------------------------------------

  \brief vos_pkt_pop_tail() - Remove data from the end of the voss Packet

  This API removes data from the end of a voss Packet.  The data is
  copied into the output buffer described by pData and pDataSize

  \param pPacket - the voss Packet to operate on.

  \param pData - pointer to the data buffer where the data removed from the
                 voss Packet is placed.

  \param dataSize - The amount of space to remove from the end of the voss
                    Packet.  The output buffer (at *pData) must contain at
                    least this amount of space.

  \return

  \sa

  ---------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_pop_tail( vos_pkt_t *pPacket,
                             v_VOID_t *pData,
                             v_SIZE_t dataSize )
{
   struct sk_buff *skb;

   // Validate the parameter pointers
   if (unlikely((NULL == pPacket) || (NULL == pData)))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: NULL pointer", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // Validate that this is really an initialized vos packet
   if (unlikely(VPKT_MAGIC_NUMBER != pPacket->magic))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: Invalid magic", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // get pointer to the skb
   skb = pPacket->pSkb;

   // Validate the skb
   if (unlikely(NULL == skb))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: NULL skb", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // Make sure there is enough data to pop
   if (unlikely(skb->len < dataSize))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_WARN,
                "VPKT [%d]: pop exceeds packet size, len[%d], req[%d]",
                __LINE__, skb->len, dataSize);
      return VOS_STATUS_E_INVAL;
   }

   // adjust pointers (there isn't a native Linux API for this)
   skb->tail -= dataSize;
   skb->len -= dataSize;

   // actually push the data
   vos_mem_copy(pData, skb_tail_pointer(skb), dataSize);

   return VOS_STATUS_SUCCESS;
}

/**--------------------------------------------------------------------------

  \brief vos_pkt_trim_tail() - Skip over bytes at the end of a voss Packet

  This API moves the pointers at the head of a voss Packet to essentially
  skip over data at the end of a voss Packet.  Upon successful return, the
  length of the voss Packet is reduced by dataSize and voss Packet is
  adjusted to eliminate the data from the end of the voss Packet.

  This API has the opposite effect of \a vos_pkt_reserve_tail().

  \param pPacket - the voss Packet to operate on.

  \param dataSize - The amount of space to remove at the end of the voss
                    Packet.

                    Note that upon return, the data skipped over is
                    inaccessible to the caller.  If the caller needs access
                    to the tail data, use vos_pkt_pop_tail() or
                    vos_pkt_extract_data() to get a copy of the data.

  \return

  \sa

  ---------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_trim_tail( vos_pkt_t *pPacket,
                              v_SIZE_t dataSize )
{
   struct sk_buff *skb;

   // Validate the parameter pointers
   if (unlikely(NULL == pPacket))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: NULL pointer", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // Validate that this is really an initialized vos packet
   if (unlikely(VPKT_MAGIC_NUMBER != pPacket->magic))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: Invalid magic", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // get pointer to the skb
   skb = pPacket->pSkb;

   // Validate the skb
   if (unlikely(NULL == skb))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: NULL skb", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // Make sure there is enough data to pop
   if (unlikely(skb->len < dataSize))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_WARN,
                "VPKT [%d]: pop exceeds packet size, len[%d], req[%d]",
                __LINE__, skb->len, dataSize);
      return VOS_STATUS_E_INVAL;
   }

   // adjust pointers (there isn't a native Linux API for this)
   skb->tail -= dataSize;
   skb->len -= dataSize;

   return VOS_STATUS_SUCCESS;
}


/**--------------------------------------------------------------------------

  \brief vos_pkt_get_timestamp() - Retrive the timestamp attribute from the
  specified VOSS packet

  \param pPacket - the voss Packet to operate on.

  \param pTstamp - the timestamp will be returned here.

  \return VOS_STATUS_E_FAULT - invalid parameter(s) specified

          VOS_STATUS_SUCCESS - timestamp retrived successfully

  \sa

  ---------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_get_timestamp( vos_pkt_t *pPacket,
                                  v_TIME_t* pTstamp )
{
   // Validate the parameter pointers
   if (unlikely((NULL == pPacket) ||
                (NULL == pTstamp)))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: NULL pointer", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // Validate that this is really an initialized vos packet
   if (unlikely(VPKT_MAGIC_NUMBER != pPacket->magic))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: Invalid magic", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // return the requested information
   *pTstamp = pPacket->timestamp;
   return VOS_STATUS_SUCCESS;
}


/**--------------------------------------------------------------------------

  \brief vos_pkt_flatten_rx_pkt() - Transform a platform based RX VOSS
  packet into a flat buffer based VOSS packet if needed. This is needed in
  cases where for reasons of efficiency we want the RX packets to be very
  platform specific (for e.g. DSM based on AMSS, etc). However platform
  independent code may rely on making calls on the VOSS packet which can only
  be supported by the flat buffer based implementation of a RX packet. This API
  will allocate a new VOSS packet with flat buffer, extract the data from the
  input VOSS packet and then release the input VOSS packet. The new VOSS packet
  will be returned from this call.

  \param ppPacket - the voss Packet to operate on. On input contains
                    the platform based packet. On output contains the flat
                    buffer based packet. Any applicable resources
                    are freed as part of this call.

  \return VOS_STATUS_E_FAULT - invalid parameter specified

          VOS_STATUS_E_INVAL - packet type not RX_RAW

          VOS_STATUS_SUCCESS - transform successful

          other VOSS status - other errors encountered

  \sa

  ----------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_flatten_rx_pkt( vos_pkt_t **ppPacket )
{
   // Linux/Android skbs are already flat, no work required
   return VOS_STATUS_SUCCESS;
}

/**--------------------------------------------------------------------------
  
  \brief vos_pkt_set_rx_length() - Set the length of a received packet 

  This API set the length of the data inside the packet after a DMA has occurred
  on rx, it will also set the tail pointer to the end of the data.

  \param pPacket - the voss Packet to operate on.

  \param pktLen - The size of the data placed in the Rx packet during DMA.


  \return

  \sa

  ---------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_set_rx_length( vos_pkt_t *pPacket,
                                  v_SIZE_t pktLen )
{
   struct sk_buff *skb;

   // Validate the parameter pointers
   if (unlikely(NULL == pPacket))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: NULL pointer", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // Validate that this is really an initialized vos packet
   if (unlikely(VPKT_MAGIC_NUMBER != pPacket->magic))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: Invalid magic", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // get pointer to the skb
   skb = pPacket->pSkb;

   // Validate the skb
   if (unlikely(NULL == skb))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: NULL skb", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // adjust pointers (there isn't a native Linux API for this)
   // ?? - is this sufficient? 
   skb_set_tail_pointer(skb, pktLen);
   skb->len   = pktLen;

   return VOS_STATUS_SUCCESS; 

}
/**--------------------------------------------------------------------------
  
  \brief vos_pkt_get_available_buffer_pool() - Get avaliable VOS packet size
   VOSS Packet pool is limitted resource
   VOSS Client need to know how many packet pool is still avaliable to control
   the flow
   
  \param  pktType - Packet type want to know free buffer count
                    VOS_PKT_TYPE_TX_802_11_MGMT, management free buffer count,
                    VOS_PKT_TYPE_TX_802_11_DATA
                    VOS_PKT_TYPE_TX_802_3_DATA, TX free buffer count
                    VOS_PKT_TYPE_RX_RAW, RX free buffer count

          vosFreeBuffer - free frame buffer size
  
  \return VOS_STATUS_E_INVAL - invalid input parameter

          VOS_STATUS_SUCCESS - Get size success
    
  \sa
  
  ----------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_get_available_buffer_pool (VOS_PKT_TYPE  pktType,
                                              v_SIZE_t     *vosFreeBuffer)
{
   struct list_head *pList;
   struct list_head *pNode;
   v_SIZE_t count;
   struct mutex *mlock;

   if (NULL == vosFreeBuffer)
   {
      return VOS_STATUS_E_INVAL;
   }

   switch (pktType)
   {
   case VOS_PKT_TYPE_TX_802_11_MGMT:
      pList = &gpVosPacketContext->txMgmtFreeList;
      mlock = &gpVosPacketContext->txMgmtFreeListLock;
      break;

   case VOS_PKT_TYPE_TX_802_11_DATA:
   case VOS_PKT_TYPE_TX_802_3_DATA:
      if (VOS_STA_SAP_MODE == hdd_get_conparam())
      {
         *vosFreeBuffer = gpVosPacketContext->uctxDataFreeListCount;  
          return VOS_STATUS_SUCCESS;
      }
      else
      {
         pList = &gpVosPacketContext->txDataFreeList;
         mlock = &gpVosPacketContext->txDataFreeListLock;
      }
      break;

   case VOS_PKT_TYPE_RX_RAW:
      // if the caller is curious how many raw packets are available
      // then he probably wants as many packets to be available as
      // possible so replenish the raw pool
      vos_pkti_replenish_raw_pool();
      // Return the pre-calculated count 'rxRawFreeListCount'
      *vosFreeBuffer = gpVosPacketContext->rxRawFreeListCount;
      return VOS_STATUS_SUCCESS;
      break;

   default:
      return (VOS_STATUS_E_INVAL);
   }

   count = 0;
   mutex_lock(mlock);
   list_for_each(pNode, pList)
   {
      count++;
   }
   mutex_unlock(mlock);
   *vosFreeBuffer = count;
   return VOS_STATUS_SUCCESS;
}

/**
  @brief vos_pkt_get_num_of_rx_raw_pkts() - Get the number of RX packets
                                       that should be allocated.

  This function is called by VOS packet module to know how many RX raw
  packets it should allocate/reserve. This value can be configured thru
  Kernel device tree to save memory usage.

  @param
       NONE
  @return
       v_SIZE_t the number of packets to allocate

*/
v_SIZE_t vos_pkt_get_num_of_rx_raw_pkts(void)
{
#ifdef HAVE_WCNSS_RX_BUFF_COUNT
    v_SIZE_t buffCount;

    buffCount = wcnss_get_wlan_rx_buff_count();
    return (buffCount > VPKT_NUM_RX_RAW_PACKETS ?
               VPKT_NUM_RX_RAW_PACKETS : buffCount);
#else
    return VPKT_NUM_RX_RAW_PACKETS;
#endif
}

/**
  @brief vos_pkt_get_num_of_rx_raw_pkts() - Get the number of times
         skb allocation failed while replenishing packets


  @param
       NONE
  @return
       v_SIZE_t the number of times packet allocation failed

*/
v_SIZE_t vos_pkt_get_num_of_rx_pkt_alloc_failures(void)
{
   v_SIZE_t failCount;

   mutex_lock(&gpVosPacketContext->rxReplenishListLock);
   mutex_lock(&gpVosPacketContext->rxRawFreeListLock);

   failCount = gpVosPacketContext->rxReplenishFailCount;

   mutex_unlock(&gpVosPacketContext->rxReplenishListLock);
   mutex_unlock(&gpVosPacketContext->rxRawFreeListLock);

   return failCount;
}

v_U8_t vos_pkt_get_proto_type
(
   void  *pskb,
   v_U8_t tracking_map
)
{
   v_U8_t     pkt_proto_type = 0;
   v_U16_t    ether_type;
   v_U16_t    SPort;
   v_U16_t    DPort;
   struct sk_buff *skb = NULL;


   if (NULL == pskb)
   {
      return pkt_proto_type;
   }
   else
   {
      skb = (struct sk_buff *)pskb;
   }

   /* EAPOL Tracking enabled */
   if (VOS_PKT_PROTO_TYPE_EAPOL & tracking_map)
   {
      ether_type = (v_U16_t)(*(v_U16_t *)(skb->data + VOS_PKT_PROT_ETH_TYPE_OFFSET));
      if (VOS_PKT_PROT_EAPOL_ETH_TYPE == VOS_SWAP_U16(ether_type))
      {
         pkt_proto_type |= VOS_PKT_PROTO_TYPE_EAPOL;
      }
   }

   /* ARP Tracking Enabled */
   if (VOS_PKT_PROTO_TYPE_ARP & tracking_map)
   {
      ether_type = (v_U16_t)(*(v_U16_t *)(skb->data + VOS_PKT_PROT_ETH_TYPE_OFFSET));
      if (VOS_PKT_PROT_ARP_ETH_TYPE == VOS_SWAP_U16(ether_type))
      {
         pkt_proto_type |= VOS_PKT_PROTO_TYPE_ARP;
      }
   }

   /* DHCP Tracking enabled */
   if (VOS_PKT_PROTO_TYPE_DHCP & tracking_map)
   {
      SPort = (v_U16_t)(*(v_U16_t *)(skb->data + VOS_PKT_PROT_IP_OFFSET +
                                     VOS_PKT_PROT_IP_HEADER_SIZE));
      DPort = (v_U16_t)(*(v_U16_t *)(skb->data + VOS_PKT_PROT_IP_OFFSET +
                                     VOS_PKT_PROT_IP_HEADER_SIZE + sizeof(v_U16_t)));
      if (((VOS_PKT_PROT_DHCP_SRV_PORT == VOS_SWAP_U16(SPort)) &&
           (VOS_PKT_PROT_DHCP_CLI_PORT == VOS_SWAP_U16(DPort))) ||
          ((VOS_PKT_PROT_DHCP_CLI_PORT == VOS_SWAP_U16(SPort)) &&
           (VOS_PKT_PROT_DHCP_SRV_PORT == VOS_SWAP_U16(DPort))))
      {
         pkt_proto_type |= VOS_PKT_PROTO_TYPE_DHCP;
      }
   }

   /* Protocol type map */
   return pkt_proto_type;
}

v_PVOID_t vos_get_pkt_head(vos_pkt_t *pPacket)
{
   struct sk_buff *skb;

   // Validate the parameter pointers
   if (unlikely(NULL == pPacket))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: NULL pointer", __LINE__);
      return NULL;
   }

   if ( VOS_STATUS_SUCCESS !=
        vos_pkt_get_os_packet(pPacket, (void**)&skb, VOS_FALSE ))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "OS-PKT [%d]: OS PKT pointer is NULL", __LINE__);
      return NULL;
   }

   return VOS_PKT_GET_HEAD(skb);
}

v_PVOID_t vos_get_pkt_end(vos_pkt_t *pPacket)
{
   struct sk_buff *skb;

    // Validate the parameter pointers
   if (unlikely(NULL == pPacket))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: NULL pointer", __LINE__);
      return NULL;
   }

   if ( VOS_STATUS_SUCCESS !=
        vos_pkt_get_os_packet(pPacket, (void**)&skb, VOS_FALSE ))
   {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "OS-PKT [%d]: OS PKT pointer is NULL", __LINE__);
     return NULL;
   }

 /* find end point if skb->end is an offset */
#ifdef NET_SKBUFF_DATA_USES_OFFSET
   return VOS_PKT_GET_HEAD(skb) + VOS_PKT_GET_END(skb);
#else
   return VOS_PKT_GET_END(skb);
#endif
}

v_VOID_t vos_recover_tail(vos_pkt_t *pPacket)
{
   struct skb_shared_info *shinfo;
   struct sk_buff *skb;

   // Validate the parameter pointers
   if (unlikely(NULL == pPacket))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: NULL pointer", __LINE__);
      return;
   }

   if ( VOS_STATUS_SUCCESS !=
        vos_pkt_get_os_packet(pPacket, (void**)&skb, VOS_FALSE ))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "OS-PKT [%d]: OS PKT pointer is NULL", __LINE__);
      return;
   }

   shinfo = skb_shinfo(skb);
   memset(shinfo, 0, sizeof(struct skb_shared_info));
   atomic_set(&shinfo->dataref, 1);
   kmemcheck_annotate_variable(shinfo->destructor_arg);

   return;
}

#ifdef VOS_PACKET_UNIT_TEST
#include "vos_packet_test.c"
#endif
