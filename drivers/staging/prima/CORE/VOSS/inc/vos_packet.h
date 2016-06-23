/*
 * Copyright (c) 2011-2014 The Linux Foundation. All rights reserved.
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

#if !defined( __VOS_PKT_H )
#define __VOS_PKT_H

/**=========================================================================
  
  \file        vos_packet.h
  
  \brief       virtual Operating System Services (vOSS) network Packet APIs
               
   Network Protocol packet/buffer support interfaces 
  
   Copyright 2008 (c) Qualcomm, Incorporated.  All Rights Reserved.
   
   Qualcomm Confidential and Proprietary.
  
  ========================================================================*/

/* $Header$ */

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include <vos_types.h>
#include <vos_status.h>

/*-------------------------------------------------------------------------- 
  Preprocessor definitions and constants
  ------------------------------------------------------------------------*/
#define VOS_PKT_PROTO_TYPE_EAPOL   0x02
#define VOS_PKT_PROTO_TYPE_DHCP    0x04
/*-------------------------------------------------------------------------- 
  Type declarations
  ------------------------------------------------------------------------*/
struct vos_pkt_t;
typedef struct vos_pkt_t vos_pkt_t;


/// data vector
typedef struct
{         
   /// address of data
   v_VOID_t *pData;
   
   /// number of bytes at address
   v_U32_t  numBytes;
   
} vos_pkt_data_vector_t;



/// voss Packet Types
typedef enum
{
   /// voss Packet is used to transmit 802.11 Management frames.
   VOS_PKT_TYPE_TX_802_11_MGMT,
   
   /// voss Packet is used to transmit 802.11 Data frames.
   VOS_PKT_TYPE_TX_802_11_DATA,
   
   /// voss Packet is used to transmit 802.3 Data frames.
   VOS_PKT_TYPE_TX_802_3_DATA,
   
   /// voss Packet contains Received data of an unknown frame type
   VOS_PKT_TYPE_RX_RAW,

   /// Invalid sentinel value
   VOS_PKT_TYPE_MAXIMUM

} VOS_PKT_TYPE;

/// user IDs.   These IDs are needed on the vos_pkt_get/set_user_data_ptr()
/// to identify the user area in the voss Packet.
typedef enum
{
   VOS_PKT_USER_DATA_ID_TL =0,  
   VOS_PKT_USER_DATA_ID_BAL,  
   VOS_PKT_USER_DATA_ID_WDA,   
   VOS_PKT_USER_DATA_ID_HDD,
   VOS_PKT_USER_DATA_ID_BAP,
   VOS_PKT_USER_DATA_ID_BSL,

   VOS_PKT_USER_DATA_ID_MAX
   
} VOS_PKT_USER_DATA_ID;

/**------------------------------------------------------------------------
  
  \brief voss asynchronous get_packet callback function

  This is a callback function invoked when vos_pkt_get_packet() cannot
  get the requested packet and the caller specified a callback to be 
  invoked when packets are available. 
  
  \param pPacket - the packet obtained by voss for the caller. 
  
  \param userData - the userData field given on the vos_pkt_get_packet() 
         call
  
  \return
    
  \sa
  
  ------------------------------------------------------------------------*/
typedef VOS_STATUS ( *vos_pkt_get_packet_callback )( vos_pkt_t *pPacket, 
                                                     v_VOID_t *userData );

/*
 * include the OS-specific packet abstraction
 * we include it here since the abstraction probably needs to access the
 * generic types defined above
 */
#include "i_vos_packet.h"

/*------------------------------------------------------------------------- 
  Function declarations and documenation
  ------------------------------------------------------------------------*/
  
/**--------------------------------------------------------------------------
  
  \brief vos_pkt_get_packet() - Get a voss Packets

  Gets a voss Packets from an internally managed packet pool. 
  
  \param ppPacket - pointer to location where the voss Packet pointer is 
                    returned.  If multiple packets are requested, they 
                    will be chained onto this first packet.  
                    
  \param pktType - the packet type to be retreived.  Valid packet types are:
     <ul>
       <li> VOS_PKT_TYPE_TX_802_11_MGMT - voss packet is for Transmitting 802.11 
            Management frames.
     
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
VOS_STATUS vos_pkt_get_packet( vos_pkt_t **ppPacket, VOS_PKT_TYPE pktType, 
                               v_SIZE_t dataSize, v_SIZE_t numPackets,
                               v_BOOL_t zeroBuffer,
                               vos_pkt_get_packet_callback callback,
                               v_VOID_t *userData );
                               

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
VOS_STATUS vos_pkt_wrap_data_packet( vos_pkt_t **ppPacket, VOS_PKT_TYPE pktType, 
                                     v_VOID_t *pOSPacket,
                                     vos_pkt_get_packet_callback callback,
                                     v_VOID_t *userData );


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
  
  ----------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_set_os_packet( vos_pkt_t *pPacket, v_VOID_t *pOSPacket );



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
    
  \sa vos_pkt_set_os_packet(), vos_pkt_wrap_data_packet(), vos_pkt_return_packet()
  
  ----------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_get_os_packet( vos_pkt_t *pPacket, v_VOID_t **ppOSPacket, 
                                  v_BOOL_t clearOSPacket );


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
  
  ----------------------------------------------------------------------------*/
v_VOID_t vos_pkt_get_user_data_ptr( vos_pkt_t *pPacket, VOS_PKT_USER_DATA_ID userID,
                                    v_VOID_t **ppUserData );


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
  
  ----------------------------------------------------------------------------*/
v_VOID_t vos_pkt_set_user_data_ptr( vos_pkt_t *pPacket, VOS_PKT_USER_DATA_ID userID,
                                    v_VOID_t *pUserData );

/**--------------------------------------------------------------------------
  
  \brief vos_pkt_return_packet() - Return a voss Packet (chain) to vOSS

  This API returns a voss Packet to the internally managed packet pool.  
  
  Note:  If there are multiple packets chained to this packet, the entire 
  packet chain is returned to vOSS.  The caller must unchain the 
  packets throgh vos_pkt_get_next_packet() and return them individually 
  if all packets in the packet chain are not to be returned.
  
  \param pPacket - the voss Packet(s) to return.  Note all packets chained
                   to this packet are returned to vOSS.
  
  \return
    
  \sa
  
  ----------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_return_packet( vos_pkt_t *pPacket );


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
  
  ----------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_chain_packet( vos_pkt_t *pPacket, vos_pkt_t *pChainPacket, 
                                 v_BOOL_t chainAfter );


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
  
  ----------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_walk_packet_chain( vos_pkt_t *pPacket, vos_pkt_t **ppChainedPacket,
                                      v_BOOL_t unchainPacket );


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
  
  ----------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_get_data_vector( vos_pkt_t *pPacket, vos_pkt_data_vector_t *pVector, 
                                    v_SIZE_t *pNumVectors );


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
  
  ----------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_extract_data( vos_pkt_t *pPacket, v_SIZE_t pktOffset, 
                                 v_VOID_t *pOutputBuffer, v_SIZE_t *pOutputBufferSize );


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
  
  ----------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_extract_data_chain( vos_pkt_t *pPacket, v_VOID_t *pOutputBuffer, 
                                       v_SIZE_t *pOutputBufferSize );



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
  
  ----------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_peek_data( vos_pkt_t *pPacket, v_SIZE_t pktOffset,
                              v_VOID_t **ppPacketData, v_SIZE_t numBytes );
                        
                        
/**--------------------------------------------------------------------------
  
  \brief vos_pkt_get_packet_type() - Get packet type for a voss Packet 

  This API returns the packet Type for a voss Packet. 
  
  \param pPacket - the voss Packet to get the packet type from.
  
  \param pPacketType - location to return the packet type for the voss Packet
  
  \return
    
  \sa
  
  ----------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_get_packet_type( vos_pkt_t *pPacket, VOS_PKT_TYPE *pPacketType );


/**--------------------------------------------------------------------------
  
  \brief vos_pkt_get_packet_length() - Get packet length for a voss Packet 

  This API returns the total length of the data in a voss Packet. 
  
  \param pPacket - the voss Packet to get the packet length from.
  
  \param pPacketSize - location to return the total size of the data contained
                       in the voss Packet.  
  \return
    
  \sa
  
  ----------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_get_packet_length( vos_pkt_t *pPacket, v_U16_t *pPacketSize );


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
  
  ----------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_get_packet_chain_length( vos_pkt_t *pPacketChain, v_SIZE_t *pPacketChainSize );




/**--------------------------------------------------------------------------
  
  \brief vos_pkt_push_head() - push data on the front a of a voss Packet 

  This API will push data onto the front of a voss Packet.  The data will be 
  appended in front of any data already contained in the voss Packet.
  
  \param pPacket - the voss Packet to modify.
  
  \param pData - pointer to the data to push onto the head of the voss Packet.
  
  \param dataSize - the size of the data to put onto the head of the voss Packet.  
  
  \return
    
  \sa
  
  ----------------------------------------------------------------------------*/ 
VOS_STATUS vos_pkt_push_head( vos_pkt_t *pPacket, v_VOID_t *pData, v_SIZE_t dataSize );


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
  
  ----------------------------------------------------------------------------*/ 
VOS_STATUS vos_pkt_reserve_head( vos_pkt_t *pPacket, v_VOID_t **ppData, 
                                 v_SIZE_t dataSize );

/**--------------------------------------------------------------------------
  
  \brief vos_pkt_reserve_head_fast()- Reserve space at the front of a voss Pkt 

  This API will reserve space at the front of a voss Packet.  The caller can 
  then copy data into this reserved space using memcpy() like functions.  This 
  allows the caller to reserve space and build headers directly in this
  reserved space in the voss Packet.
  
  Upon successful return, the length of the voss Packet is increased by 
  dataSize.
 
  Same as above API but no memset to 0.
 
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
  
  ----------------------------------------------------------------------------*/ 
VOS_STATUS vos_pkt_reserve_head_fast( vos_pkt_t *pPacket,
                                 v_VOID_t **ppData,
                                 v_SIZE_t dataSize );

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
  
  ----------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_pop_head( vos_pkt_t *pPacket, v_VOID_t *pData, v_SIZE_t dataSize );


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
  
  ----------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_trim_head( vos_pkt_t *pPacket, v_SIZE_t dataSize );


/**--------------------------------------------------------------------------
  
  \brief vos_pkt_push_tail() - push data on the end a of a voss Packet 

  This API will push data onto the end of a voss Packet.  The data will be 
  appended to the end of any data already contained in the voss Packet.
    
  \param pPacket - the voss Packet to modify.
  
  \param pData - pointer to the data to push onto the tail of the voss Packet.
  
  \param dataSize - the size of the data to put onto the tail of the voss Packet.  
  
  \return
    
  \sa
  
  ----------------------------------------------------------------------------*/ 
VOS_STATUS vos_pkt_push_tail( vos_pkt_t *pPacket, v_VOID_t *pData, v_SIZE_t dataSize );


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
  
  ----------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_reserve_tail( vos_pkt_t *pPacket, v_VOID_t **ppData, 
                                 v_SIZE_t dataSize );


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
  
  ----------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_pop_tail( vos_pkt_t *pPacket, v_VOID_t *pData, v_SIZE_t dataSize );


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
  
  ----------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_trim_tail( vos_pkt_t *pPacket, v_SIZE_t dataSize );

/**--------------------------------------------------------------------------
  
  \brief vos_pkt_get_timestamp() - Retrive the timestamp attribute from the
  specified VOSS packet

  \param pPacket - the voss Packet to operate on. 

  \param pTstamp - the timestamp will be returned here. 
  
  \return VOS_STATUS_E_FAULT - invalid parameter(s) specified

          VOS_STATUS_SUCCESS - timestamp retrived successfully
    
  \sa
  
  ----------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_get_timestamp( vos_pkt_t *pPacket, v_TIME_t* pTstamp );

/**--------------------------------------------------------------------------
  
  \brief vos_pkt_flatten_rx_pkt() - Transform a platform based RX VOSS
  packet into a flat buffer based VOSS packet if needed. This is needed in cases 
  where for reasons of efficiency we want the RX packets to be very platform specific
  (for e.g. DSM based on AMSS, etc). However platform independent code may rely
  on making calls on the VOSS packet which can only be supported by the flat buffer
  based implementation of a RX packet. This API will allocate a new VOSS packet 
  with flat buffer, extract the data from the input VOSS packet and then release 
  the input VOSS packet. The new VOSS packet will be returned from this call.
 
  \param ppPacket - the voss Packet to operate on. on input contains
                    the platform based packet. On output contains the flat
                    buffer based packet. Any applicable resources
                    are freed as part of this call.
  
  \return VOS_STATUS_E_FAULT - invalid parameter specified

          VOS_STATUS_E_INVAL - packet type not RX_RAW 

          VOS_STATUS_SUCCESS - transform successful

          other VOSS status - other errors encountered
    
  \sa
  
  ----------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_flatten_rx_pkt( vos_pkt_t **ppPacket );

/**--------------------------------------------------------------------------
  
  \brief vos_pkt_set_rx_length() - Set the length of a received packet 

  This API set the length of the data inside the packet after a DMA has occurred
  on rx, it will also set the tail pointer to the end of the data.

  \param pPacket - the voss Packet to operate on.

  \param pktLen - The size of the data placed in the Rx packet during DMA.


  \return

  \sa

  ---------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_set_rx_length( vos_pkt_t *pPacket, v_SIZE_t pktLen );

/**--------------------------------------------------------------------------
  
  \brief vos_pkt_get_available_buffer_pool() - Get avaliable VOS packet size
   VOSS Packet pool is limitted resource
   VOSS Client need to know how many packet pool is still avaliable to control the flow
   
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
VOS_STATUS vos_pkt_get_available_buffer_pool
(
   VOS_PKT_TYPE  pktType,
   v_SIZE_t     *vosFreeBuffer
);

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
v_SIZE_t vos_pkt_get_num_of_rx_raw_pkts(void);

/**
  @brief vos_pkt_get_num_of_rx_pkt_alloc_failures() - Get the number of times
         skb allocation failed while replenishing packets


  @param
       NONE
  @return
       v_SIZE_t the number of times packet allocation failed

*/
v_SIZE_t vos_pkt_get_num_of_rx_pkt_alloc_failures(void);

v_U8_t vos_pkt_get_proto_type
(
   void  *pskb,
   v_U8_t tracking_map
);
#endif  // !defined( __VOS_PKT_H )
