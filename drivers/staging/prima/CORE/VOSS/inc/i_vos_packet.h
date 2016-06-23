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

#if !defined( __I_VOS_PACKET_H )
#define __I_VOS_PACKET_H

/**=========================================================================

  \file        i_vos_packet.h

  \brief       virtual Operating System Servies (vOSS)

   Network Protocol packet/buffer internal include file

   Copyright 2009 (c) Qualcomm, Incorporated.  All Rights Reserved.

   Qualcomm Confidential and Proprietary.

  ========================================================================*/

/* $Header$ */

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include <vos_types.h>
#include <vos_list.h>
#include <linux/skbuff.h>
#include <linux/list.h>

#include <wlan_qct_pal_packet.h>
#include <wlan_qct_wdi_ds.h>

/*--------------------------------------------------------------------------
  Preprocessor definitions and constants
  ------------------------------------------------------------------------*/

// Definitions for the various VOS packet pools.  Following are defines
// for the SIZE and the NUMBER of vos packets in the vos packet pools.
// Note that all of the code is written to allocate and manage the vos
// packet pools based on these #defines only.  For example, if you want to
// change the number of RX_RAW packets, simply change the #define that
// defines the number of RX_RAW packets and rebuild VOSS.

// the number of Receive vos packets used exclusively for vos packet
// allocations of type VOS_PKT_TYPE_RX_RAW
#define VPKT_NUM_RX_RAW_PACKETS (1024)

// the number of Transmit Management vos packets, used exclusively for
// vos packet allocations of type VOS_PKT_TYPE_TX_802_11_MGMT
#define VPKT_NUM_TX_MGMT_PACKETS (  6 )

// the number of Transmit Data vos packets, used exclusively for
// vos packet allocations of type VOS_PKT_TYPE_TX_802_3_DATA or
// VOS_PKT_TYPE_TX_802_11_DATA
#define VPKT_NUM_TX_DATA_PACKETS ( 128 )

// the number of VOS Packets we need.  This is the memory we need to
// allocate for the vos Packet structures themselves.  We need vos
// packet structures for all of the packet types (RX_RAW, TX_MGMT, and
// TX_DATA).
#define VPKT_NUM_VOS_PKT_BUFFERS \
                ( VPKT_NUM_RX_RAW_PACKETS \
                + VPKT_NUM_TX_MGMT_PACKETS \
                + VPKT_NUM_TX_DATA_PACKETS )

// the number of Receive vos packets that we accumulate in the
// replenish pool before we attempt to replenish them
#define VPKT_RX_REPLENISH_THRESHOLD (  VPKT_NUM_RX_RAW_PACKETS >> 2 )

// magic number which can be used to verify that a structure pointer being
// dereferenced is really referencing a struct vos_pkt_t
#define VPKT_MAGIC_NUMBER 0x56504B54  /* VPKT in ASCII */

// while allocating the skb->data is cache aligned, so the memory allocated
// is more than VPKT_SIZE_BUFFER
#define VPKT_SIZE_BUFFER_ALIGNED SKB_DATA_ALIGN(VPKT_SIZE_BUFFER)
/*--------------------------------------------------------------------------
  Type declarations
  ------------------------------------------------------------------------*/



/// implementation specific vos packet type
struct vos_pkt_t
{

   //palPacket MUST be the first member of vos_pkt_t
   wpt_packet palPacket;

   // Node for linking vos packets into a free list
   struct list_head node;

   // Node for chaining vos packets into a packet chain
   struct vos_pkt_t *pNext;

   // pointer to an OS specific packet descriptor
   struct sk_buff *pSkb;

   // packet type
   VOS_PKT_TYPE packetType;

   // timestamp
   v_TIME_t timestamp;

   // user data pointers
   v_VOID_t *pvUserData[ VOS_PKT_USER_DATA_ID_MAX ];

   // magic number for verifying this is really a struct vos_pkt_t
   v_U32_t magic;
};


// Parameters from the vos_pkt_get_packet() call that needs
// to be saved in 'low resource' conditions.  We need all the
// parameters from the original call to vos_pkt_get_packet()
// to resolve the packet request when one become available.
typedef struct
{
   vos_pkt_get_packet_callback callback;

   v_VOID_t *userData;

   v_SIZE_t dataSize;
   v_SIZE_t numPackets;
   v_BOOL_t zeroBuffer;

} vos_pkt_low_resource_info;



// vOSS Packet Context - all context / internal data needed for the
// vOSS pPacket module.  This consiste of:
// - memory for the vos Packet structures
// - memory for the vos Packet Head / Tail buffers
// - memory for the Rx Raw data buffers
// - memory for the Tx Mgmt data buffers.
typedef struct vos_pkt_context_s
{
   // place to save the vos Context
   v_CONTEXT_t vosContext;

   // the memory for the vos Packet structures....
   struct vos_pkt_t vosPktBuffers[ VPKT_NUM_VOS_PKT_BUFFERS ];

   // These are the lists to keep the constructed VOS packets that are
   // available for allocation.  There are separate free vos packet
   // pools for RX_RAW without attached skb, RX_RAW with attached skb,
   // TX_DATA, and TX_MGMT.
   struct list_head rxReplenishList;
   struct list_head rxRawFreeList;
   struct list_head txDataFreeList;
   struct list_head txMgmtFreeList;

   //Existing list_size opearation traverse the list. Too slow for data path.
   //Add the field to enable faster flow control on tx path
   v_U32_t uctxDataFreeListCount;

   // We keep a separate count of the number of RX_RAW packets
   // waiting to be replenished
   v_SIZE_t rxReplenishListCount;

   // Count for the number of packets that could not be replenished
   // because the memory allocation API failed
   v_SIZE_t rxReplenishFailCount;
   //Existing list_size opearation traverse the list. Too slow for data path.
   //Add the field for a faster rx path
   v_SIZE_t rxRawFreeListCount;

   // Number of RX Raw packets that will be reserved; this is a configurable
   // value to the driver to save the memory usage.
   v_SIZE_t numOfRxRawPackets;

   // These are the structs to keep low-resource callback information.
   // There are separate low-resource callback information blocks for
   // RX_RAW, TX_DATA, and TX_MGMT.
   vos_pkt_low_resource_info rxRawLowResourceInfo;
   vos_pkt_low_resource_info txDataLowResourceInfo;
   vos_pkt_low_resource_info txMgmtLowResourceInfo;

   struct mutex rxReplenishListLock;
   struct mutex rxRawFreeListLock;
   struct mutex txDataFreeListLock;
   struct mutex txMgmtFreeListLock;

   /*Meta Information to be transported with the packet*/
   WDI_DS_TxMetaInfoType txMgmtMetaInfo[VPKT_NUM_TX_MGMT_PACKETS];
   WDI_DS_TxMetaInfoType txDataMetaInfo[VPKT_NUM_TX_DATA_PACKETS];
   WDI_DS_RxMetaInfoType rxMetaInfo[VPKT_NUM_RX_RAW_PACKETS];

} vos_pkt_context_t;



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
                            v_SIZE_t vosPacketContextSize );


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
VOS_STATUS vos_packet_close( v_PVOID_t pVosContext );

#endif  // !defined( __I_VOS_PACKET_H )
