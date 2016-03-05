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

/*
 * Woodside Networks, Inc proprietary. All rights reserved.
 * File: $File: //depot/software/projects/feature_branches/nova_phase1/ap/apps/asf/aniAsfPacket.c $
 * Contains definitions for packet manipulation routines that make it
 * easy to create and parse multi-layered network frames. This module
 * minimizes buffer copies while adding or removing headers, and
 * adding or removing payload.
 *
 * Author:      Mayank D. Upadhyay
 * Date:        19-June-2002
 * History:-
 * Date         Modified by     Modification Information
 * ------------------------------------------------------
 *
 */
#include "vos_types.h"
#include "vos_trace.h"
#include <bapRsnAsfPacket.h>
#include <bapRsnErrors.h>
#include "vos_memory.h"
#include "vos_packet.h"

/*
 * Allocate one more than required because the last bytes is waste. We
 * waste the last byte because in the adopted model, the tail always
 * points to the next location where data should be stored. In a full
 * buffer we don't want to position the tail to memory we haven't
 * allocated ourself.
 */
#define ANI_INTERNAL_DEFAULT_PACKET_SIZE (ANI_DEFAULT_PACKET_SIZE + 4)

#define TAIL_SPACE(packet) \
    ((packet)->buf + (packet)->size - (packet)->tail)

#define HEAD_SPACE(packet) \
     ((packet)->head - (packet)->buf)

#define ANI_CHECK_RANGE(x , upper) \
                ( (x) <= (upper) )

/**
 * Opaque packet structure with internal storage for raw bytes.
 * Conceptually, a tAniPacket is a pre-allocated buffer that contains
 * data in the middle and free space on either side. The start of the
 * data is called the head. Routines are provided to add data at the
 * front or at the rear. The length of the packet is the total number
 * of valid data bytes contained in it. The size of the packet is the
 * total number of preallocated bytes.
 */
struct tAniPacket {
    v_U8_t *buf;
    v_U32_t size;
    v_U8_t *head;
    v_U8_t *tail;
    v_U8_t *recordHeader;
    v_U32_t len;
};

/**
 * aniAsfPacketAllocate
 *
 * FUNCTION:
 * Create a packet of size 2*ANI_DEFAULT_PACKET_SIZE and positions the
 * head of the packet in the center. The allocated storage can be free
 * with a call to aniAsfPacketFree.
 *
 * LOGIC:
 * Allocates storage for tAniPacket and its internal raw data
 * buffer. Positions the head and tail pointers in the middle of the
 * raw data buffer.
 *
 * @param packetPtr pointer that will be set to newly allocated
 * tAniPacket if the operation succeeds.
 *
 * @return ANI_OK if the operation succeeds; ANI_E_MALLOC_FAILED if
 * memory could not be allocated.
 * @see aniAsfPacketFree
 */
int
aniAsfPacketAllocate(tAniPacket **packetPtr)
{
  return aniAsfPacketAllocateExplicit(packetPtr,
                                   ANI_INTERNAL_DEFAULT_PACKET_SIZE,
                                   ANI_INTERNAL_DEFAULT_PACKET_SIZE/2);
}

/**
 * aniAsfPacketAllocateExplicit
 *
 * FUNCTION:
 * Create a packet of the desired size and position the head of the
 * packet at the desired offset in the internal raw data buffer. An
 * application would normally set this offset to the expected length
 * of the protocol header, then append the payload, and finally,
 * prepend the header. The allocated storage can be free with a call
 * to aniAsfPacketFree.
 *
 * LOGIC:
 * Allocates storage for tAniPacket and its internal raw data
 * buffer. Positions the head and tail pointers at the given offset in
 * the internal raw data buffer.
 *
 * @param packetPtr pointer that will be set to newly allocated
 * tAniPacket if the operation succeeds.
 * @param size the size of the internal raw data buffer
 * @param offset the offset in the internal raw data buffer where the
 * head of the packet will be positioned initially
 *
 * @return ANI_OK if the operation succeeds; ANI_E_MALLOC_FAILED if
 * memory could not be allocated.
 * @see aniAsfPacketFree
 */
int
aniAsfPacketAllocateExplicit(tAniPacket **packetPtr,
                             v_U32_t size,
                             v_U32_t offset)
{
  tAniPacket *packet = NULL;
  v_U32_t maxHead = size;

  *packetPtr = NULL;
  if (size == 0)
    return ANI_E_ILLEGAL_ARG;

  VOS_ASSERT(ANI_CHECK_RANGE(offset, maxHead));
  if (!ANI_CHECK_RANGE(offset, maxHead))
    return ANI_E_ILLEGAL_ARG;

  packet = (tAniPacket *) vos_mem_malloc( sizeof(tAniPacket) );

  if (packet == NULL) 
  {
      VOS_ASSERT( 0 );
      return ANI_E_MALLOC_FAILED;
  }

  // transparently add one to the size since last byte is wasted
  size = (size + 4) & 0xfffffffc;

  packet->buf = (v_U8_t *)vos_mem_malloc( sizeof(v_U8_t) * size );
  if (packet->buf == NULL) 
  {
      vos_mem_free( packet );
      VOS_ASSERT( 0 );
      return ANI_E_MALLOC_FAILED;
  }

  packet->size = size; // Should not be visible to the user
  packet->head = packet->buf + offset;
  packet->tail = packet->head;
  packet->len = 0;

  *packetPtr = packet;
  return ANI_OK;
}

/**
 * aniAsfPacketDuplicate
 *
 * Duplicates a given packet exactly. That is, the contents, the size
 * of the packet, and the positions of the pointers are maintained in
 * the new copy.
 *
 * @param newPacketPtr is set to a newly allocated packet that is a
 * duplicate of oldPacket
 * @param oldPacket the original packet that should be duplicated
 *
 * @return ANI_OK if the operation succeeds; ANI_E_NULL if oldPacket
 * is NULL; 
 */
int
aniAsfPacketDuplicate(tAniPacket **newPacketPtr, tAniPacket *oldPacket)
{
    int retVal;
    int recordPos;
    tAniPacket *packet = NULL;

    if (oldPacket == NULL)
        return ANI_E_NULL_VALUE;

    retVal = aniAsfPacketAllocateExplicit(&packet,
                                          oldPacket->size,
                                          oldPacket->head - oldPacket->buf);
    if (retVal != ANI_OK)
        return retVal;

    retVal = aniAsfPacketAppendBuffer(packet,
                                      oldPacket->head,
                                      oldPacket->len);
    if (retVal != ANI_OK) 
    {
        VOS_ASSERT( 0 );
        aniAsfPacketFree(packet);
        return ANI_E_FAILED;
    }

    if (oldPacket->recordHeader != NULL) 
    {
        recordPos = oldPacket->recordHeader - oldPacket->buf;
        packet->recordHeader = packet->buf + recordPos;
    }
    *newPacketPtr = packet;

    return ANI_OK;
}

/**
 * aniAsfPacketFree
 *
 * FUNCTION:
 * Free a previously allocated tAniPacket and its internal raw data
 * buffer.
 *
 * @param packet the packet to free
 *
 * @return ANI_OK if the operation succeeds; ANI_E_NULL_VALUE if an
 * unexpected NULL pointer is encountered
 */
int
aniAsfPacketFree(tAniPacket *packet)
{
  if (packet == NULL)
    return ANI_E_NULL_VALUE;

  if (packet->buf != NULL)
    vos_mem_free( packet->buf );

  vos_mem_free( packet );

  return ANI_OK;
}


/**
 * aniAsfPacketAppendBuffer
 *
 * FUNCTION:
 * Appends the data contained in buf to the end of the data in
 * destAniPacket. The head of destAniPacket remains unchanged, while its
 * length increases by len.
 *
 * If there isn't enough free space in destAniPacket for all len bytes
 * then the routine fails and the length of destAniPacket remains
 * unchanged.
 *
 * LOGIC:
 * Check that there is enough free space in the packet to append the
 * buffer. If not, bail. Otherwise, copy bytes from the buffer into
 * the packet's internal raw data buffer and increase the value of its
 * length to reflect this.
 *
 * @param packet the packet to append to
 * @param buf the buffer containing data to be appended to the packet
 * @param len the number of bytes to append
 *
 * @return ANI_OK if the operation succeeds; ANI_E_FAILED if the
 * packet does not have enough free space for the complete buffer
 * @see aniAsfPacketPrependBuffer
 */
int
aniAsfPacketAppendBuffer(tAniPacket *destPacket,
                      const v_U8_t *buf,
                      v_U32_t len)
{
  if (aniAsfPacketCanAppendBuffer(destPacket, len) != ANI_OK)
      return ANI_E_FAILED;

  if (buf == NULL)
    return ANI_E_NULL_VALUE;

  vos_mem_copy(destPacket->tail, buf, len);
  destPacket->tail += len;
  destPacket->len += len;
  return ANI_OK;
}

/**
 * aniAsfPacketPrependBuffer
 *
 * FUNCTION:
 * Prepends the data contained in buf to the start of the data in
 * destPacket. The head of destPacket is repositioned and the length
 * of destPacket increases by len.
 *
 * If there isn't enough free space in destPacket for all len bytes
 * then the routine fails and the length of destPacket remains
 * unchanged.
 *
 * LOGIC:
 * Check that there is enough free space in the packet to prepend the
 * buffer. If not, bail. Otherwise, copy bytes from the buffer into
 * the packet's internal raw data buffer and increase the value of its
 * length to reflect this.
 *
 * @param packet the packet to prepend to
 * @param buf the buffer containing data to be prepended to the packet
 * @param len the number of bytes to prepend
 *
 * @return ANI_OK if the operation succeeds; ANI_E_FAILED if the
 * packet does not have enough free space for the complete buffer
 * @see aniAsfPacketAppendBuffer
 */
int
aniAsfPacketPrependBuffer(tAniPacket *destPacket,
                       const v_U8_t *buf,
                       v_U32_t len)
{
  if (aniAsfPacketCanPrependBuffer(destPacket, len) != ANI_OK)
      return ANI_E_FAILED;

  if (buf == NULL)
      return ANI_E_NULL_VALUE;

  destPacket->head -= len;
  destPacket->len += len;
  vos_mem_copy(destPacket->head, buf, len);
  return ANI_OK;

}

/**
 * aniAsfPacketCanAppendBuffer
 *
 * FUNCTION:
 * Determines if len bytes can be safely appended to destPacket
 * without overflowing.
 *
 * LOGIC:
 * Current packet tail plus len of buffer should not exceed packet
 * start plus packet size
 *
 * Note: This does not return a boolean value, but instead an integer
 * code.
 *
 * @param packet the packet to append to
 * @param len the number of bytes to append
 *
 * @return ANI_OK if the append operation would succeed; ANI_E_FAILED
 * otherwise
 */
int
aniAsfPacketCanAppendBuffer(tAniPacket *destPacket,
                         v_U32_t len)
{
  if (destPacket == NULL)
    return ANI_E_FAILED;

  if ((int)len <= TAIL_SPACE(destPacket))
      return ANI_OK;
  else
      return ANI_E_FAILED;
}

/**
 * aniAsfPacketCanPrependBuffer
 *
 * FUNCTION:
 * Determines if len bytes can be safely prepended to destPacket
 * without overflowing.
 *
 * LOGIC:
 * Current packet head minus len of buffer should not be less than
 * start of packet.
 *
 * Note: This does not return a boolean value, but instead an integer
 * code.
 *
 * @param packet the packet to prepend to
 * @param len the number of bytes to prepend
 *
 * @return ANI_OK if the append operation would succeed; ANI_E_FAILED
 * otherwise
 */
int
aniAsfPacketCanPrependBuffer(tAniPacket *destPacket,
                          v_U32_t len)
{
  if (destPacket == NULL)
      return ANI_E_FAILED;

  if (!(len > 0))
      return ANI_E_FAILED;

  if ((int)len <= HEAD_SPACE(destPacket))
      return ANI_OK;
  else
      return ANI_E_FAILED;
}

/**
 * aniAsfPacketTruncateFromFront
 *
 * FUNCTION:
 * Removes len bytes from the front of the packet by moving its
 * head. The length of the packet is decremented by len.
 *
 * @param packet the packet to truncate from the front
 * @param len the number of bytes to truncate
 *
 * @return ANI_OK if the append operation would succeed; ANI_E_FAILED
 * otherwise
 */
int
aniAsfPacketTruncateFromFront(tAniPacket *packet,
                           v_U32_t len)
{
    if (packet == NULL)
        return ANI_E_NULL_VALUE;

    if (!ANI_CHECK_RANGE(len, packet->len))
        return ANI_E_FAILED;

    packet->head += len;
    packet->len -= len;

    return ANI_OK;
}

/**
 * aniAsfPacketTruncateFromRear
 *
 * FUNCTION:
 * Removes len bytes from the rear of the packet by moving its
 * tail. The length of the packet is decremented by len.
 *
 * @param packet the packet to truncate from the rear
 * @param len the number of bytes to truncate
 *
 * @return ANI_OK if the append operation would succeed; ANI_E_FAILED
 * otherwise
 */
int
aniAsfPacketTruncateFromRear(tAniPacket *packet,
                          v_U32_t len)
{
    if (packet == NULL)
        return ANI_E_NULL_VALUE;

    if (!ANI_CHECK_RANGE(len, packet->len))
        return ANI_E_FAILED;

    packet->tail -= len;
    packet->len -= len;

    return ANI_OK;
}

/**
 * aniAsfPacketGetLen
 *
 * FUNCTION:
 * Returns the number of valid data bytes stored in the packet.
 *
 * @param packet the packet whose len we need
 *
 * @return the non-negative number of bytes stored in the packet
 */
int
aniAsfPacketGetLen(tAniPacket *packet)
{
    if (packet == NULL)
        return ANI_E_NULL_VALUE;

    return packet->len;
}

/**
 * aniAsfPacketGetBytes
 *
 * FUNCTION:
 * Returns a pointer to the head of the valid data stored in the
 * packet.
 *
 * @param packet the packet whose bytes we need
 * @param rawBytesPtr the pointer that will be set the start of the
 * raw bytes.
 *
 * @return The non-negative number of bytes stored in the packet if
 * the operation succeeded. That is the same value as what would be
 * returned by aniAsfPacketGetLen.
 */
int
aniAsfPacketGetBytes(tAniPacket *packet, v_U8_t **rawBytesPtr)
{
    if (packet == NULL)
        return ANI_E_NULL_VALUE;

    *rawBytesPtr = packet->head;
    return packet->len;
}

/**
 * aniAsfPacketGetN
 *
 * Returns N bytes from the packet and moves the head of the packet
 * beyond those bytes.
 *
 * @param packet the packet to read from
 * @param n the number of bytes to read
 * @param bytesPtr is set to the start of the octets
 *
 * @return ANI_OK if the operation succeeds; ANI_E_SHORT_PACKET if the
 * packet does not have n bytes.
 */
int
aniAsfPacketGetN(tAniPacket *packet, int n, v_U8_t **bytesPtr)
{
    int retVal;
    v_U8_t *bytes = NULL;

    if (packet == NULL)
        return ANI_E_NULL_VALUE;

    retVal = aniAsfPacketGetBytes(packet, &bytes);
    if (retVal < n)
        return ANI_E_SHORT_PACKET;

    aniAsfPacketTruncateFromFront(packet, n);

    *bytesPtr = bytes;

    return ANI_OK;
}

/**
 * aniAsfPacketEmpty
 *
 * FUNCTION:
 * Re-initializes the packet by positioning the head to the middle and
 * setting the length to zero.
 *
 * @param packet the packet to empty
 *
 * @return ANI_OK if the operation succeeded
 */
int
aniAsfPacketEmpty(tAniPacket *packet)
{
    return aniAsfPacketEmptyExplicit(packet, packet->size/2);
}

/**
 * aniAsfPacketEmptyExplicit
 *
 * FUNCTION:
 * Re-initializes the packet by positioning the head to the desired
 * offset and setting the length to zero.
 *
 * @param packet the packet to empty
 * @param offset the offset that the head of the packet should be set
 * to. An application will be able to prepend and append data relative
 * to this offset.
 *
 * @return ANI_OK if the operation succeeded
 */
int
aniAsfPacketEmptyExplicit(tAniPacket *packet,
                       v_U32_t offset)
{
    if (packet == NULL)
        return ANI_E_NULL_VALUE;

    VOS_ASSERT(ANI_CHECK_RANGE(offset, packet->size));
    if (!ANI_CHECK_RANGE(offset, packet->size))
        return ANI_E_ILLEGAL_ARG;

    packet->head = packet->buf + offset;
    packet->tail = packet->head;
    packet->len = 0;

    return ANI_OK;
}



/**
 * aniAsfPacketPrependHdr
 *
 * FUNCTION:
 * Prepends a tAniHdr at the start of the packet.  All host to network
 * byte order translation is also taken care of.
 *
 * @param packet the packet to write to
 * @param msgType the message type to write as part of the header
 *
 * @return ANI_OK if the operation succeeds
 */
int
aniAsfPacketPrependHdr(tAniPacket *packet, v_U16_t msgType)
{
    int retVal;
    int length;

    if (packet == NULL)
        return ANI_E_NULL_VALUE;

    length = 4;

    length = 2 + 2 + packet->len;

    retVal = aniAsfPacketPrepend16(packet, length);
    if (retVal < 0)
        return retVal;

    retVal = aniAsfPacketPrepend16(packet, msgType);
    if (retVal < 0)
        return retVal;

    return ANI_OK;
}

/**
 * aniAsfPacketGet32
 *
 * FUNCTION:
 * Reads a ANI_U32 out of the packet and returns it. The packet's head
 * is advanced and its length decremented by the appropriate length.
 * All network to host byte order translation is also taken care of.
 *
 * @param packet the packet to read from
 * @param val the value to fill in
 *
 * @return ANI_OK if the operation succeeds
 */
int
aniAsfPacketGet32(tAniPacket *packet, v_U32_t *val)
{
    v_U8_t u32Arr[4];

    if (packet == NULL)
        return ANI_E_NULL_VALUE;

    if (val == NULL)
        return ANI_E_NULL_VALUE;

    if (packet->len < 4)
        return ANI_E_SHORT_PACKET;

    //packet is in network order, make sure it is align
    u32Arr[0] = packet->head[0];
    u32Arr[1] = packet->head[1];
    u32Arr[2] = packet->head[2];
    u32Arr[3] = packet->head[3];
    *val = vos_be32_to_cpu( *(v_U32_t *)u32Arr );
    aniAsfPacketTruncateFromFront(packet, 4);

    return ANI_OK;
}

/**
 * aniAsfPacketAppend32
 *
 * FUNCTION:
 * Appends a ANI_U32 to the end of the packet.
 * All host to network byte order translation is also taken care of.
 *
 * @param packet the packet to write to
 * @param val the value to append
 *
 * @return ANI_OK if the operation succeeds
 */
int
aniAsfPacketAppend32(tAniPacket *packet, v_U32_t val)
{
    v_U8_t *p8;

    if (packet == NULL)
        return ANI_E_NULL_VALUE;

    if (TAIL_SPACE(packet) < 4)
        return ANI_E_FAILED;

    val = vos_cpu_to_be32( val );
    p8 = (v_U8_t *)&val;
    packet->tail[0] =  p8[0];
    packet->tail[1] =  p8[1];
    packet->tail[2] =  p8[2];
    packet->tail[3] =  p8[3];
    aniAsfPacketMoveRight(packet, 4);

    return ANI_OK;
}

/**
 * aniAsfPacketGet16
 *
 * FUNCTION:
 * Reads a ANI_U16 out of the packet and returns it. The packet's head
 * is advanced and its length decremented by the appropriate length.
 * All network to host byte order translation is also taken care of.
 *
 * @param packet the packet to read from
 * @param val the value to fill in
 *
 * @return ANI_OK if the operation succeeds
 */
int
aniAsfPacketGet16(tAniPacket *packet, v_U16_t *val)
{
    v_U8_t u16Arr[2];

    if (packet == NULL)
        return ANI_E_NULL_VALUE;

    if (val == NULL)
        return ANI_E_NULL_VALUE;

    if (packet->len < 2)
        return ANI_E_SHORT_PACKET;

    u16Arr[0] = packet->head[0];
    u16Arr[1] = packet->head[1];
    *val = vos_be16_to_cpu( *(v_U16_t *)u16Arr );
    aniAsfPacketTruncateFromFront(packet, 2);

    return ANI_OK;
}

/**
 * aniAsfPacketPrepend16
 *
 * FUNCTION:
 * Prepends a ANI_U16 to the start of the packet.
 * All host to network byte order translation is also taken care of.
 *
 * @param packet the packet to write to
 * @param val the value to prepend
 *
 * @return ANI_OK if the operation succeeds
 */
int
aniAsfPacketPrepend16(tAniPacket *packet, v_U16_t val)
{
    v_U8_t *p8;

    if (packet == NULL)
        return ANI_E_NULL_VALUE;

    if (HEAD_SPACE(packet) < 2)
        return ANI_E_FAILED;

    aniAsfPacketMoveLeft(packet, 2);
    val = vos_cpu_to_be16( val );
    p8 = (v_U8_t *)&val;
    packet->head[0] =  p8[0];
    packet->head[1] =  p8[1];

    return ANI_OK;
}

/**
 * aniAsfPacketAppend16
 *
 * FUNCTION:
 * Appends a ANI_U16 to the end of the packet.
 * All host to network byte order translation is also taken care of.
 *
 * @param packet the packet to write to
 * @param val the value to append
 *
 * @return ANI_OK if the operation succeeds
 */
int
aniAsfPacketAppend16(tAniPacket *packet, v_U16_t val)
{
    v_U8_t *p8;

    if (packet == NULL)
        return ANI_E_NULL_VALUE;

    if (TAIL_SPACE(packet) < 2)
        return ANI_E_FAILED;

    val = vos_cpu_to_be16( val );
    p8 = (v_U8_t *)&val;
    packet->tail[0] =  p8[0];
    packet->tail[1] =  p8[1];
    aniAsfPacketMoveRight(packet, 2);

    return ANI_OK;
}

/**
 * aniAsfPacketGet8
 *
 * FUNCTION:
 * Reads a ANI_U8 out of the packet and returns it. The packet's head
 * is advanced and its length decremented by the appropriate length.
 * All network to host byte order translation is also taken care of.
 *
 * @param packet the packet to read from
 * @param val the value to fill in
 *
 * @return ANI_OK if the operation succeeds
 */
int
aniAsfPacketGet8(tAniPacket *packet, v_U8_t *val)
{
    if (packet == NULL)
        return ANI_E_NULL_VALUE;

    if (val == NULL)
        return ANI_E_NULL_VALUE;

    if (packet->len < 1)
        return ANI_E_SHORT_PACKET;

    *val = *(packet->head);
    aniAsfPacketTruncateFromFront(packet, 1);

    return ANI_OK;
}

/**
 * aniAsfPacketPrepend8
 *
 * FUNCTION:
 * Prepends a ANI_U8 to the start of the packet.
 * All host to network byte order translation is also taken care of.
 *
 * @param packet the packet to read from
 * @param val the value to prepend
 *
 * @return ANI_OK if the operation succeeds
 */
int
aniAsfPacketPrepend8(tAniPacket *packet, v_U8_t val)
{
    if (packet == NULL)
        return ANI_E_NULL_VALUE;

    VOS_ASSERT(HEAD_SPACE(packet) >= 1);
    if (HEAD_SPACE(packet) < 1)
        return ANI_E_FAILED;

    aniAsfPacketMoveLeft(packet, 1);
    *(packet->head) = val;

    return ANI_OK;
}

/**
 * aniAsfPacketAppend8
 *
 * FUNCTION:
 * Appends a ANI_U8 to the end of the packet.
 * All host to network byte order translation is also taken care of.
 *
 * @param packet the packet to write to
 * @param val the value to append
 *
 * @return ANI_OK if the operation succeeds
 */
int
aniAsfPacketAppend8(tAniPacket *packet, v_U8_t val)
{
    if (packet == NULL)
        return ANI_E_NULL_VALUE;

    if (TAIL_SPACE(packet) < 1)
        return ANI_E_FAILED;

    *(packet->tail) = val;
    aniAsfPacketMoveRight(packet, 1);

    return ANI_OK;
}

/**
 * aniAsfPacketGetMac
 *
 * FUNCTION:
 * Returns a tAniMacAddr from the start of the packet.
 *
 * @param packet the packet to read from
 * @param macAddr the destination to copy the MAC address to
 *
 * @return ANI_OK if the operation succeeds. Also, the packet head
 * pointer is advanced past the MAC address.
 */
int
aniAsfPacketGetMac(tAniPacket *packet, tAniMacAddr macAddr)
{
    if (packet->len < sizeof(tAniMacAddr))
        return ANI_E_SHORT_PACKET;

    vos_mem_copy(macAddr, packet->head, sizeof(tAniMacAddr));

    packet->head += sizeof(tAniMacAddr);
    packet->len -= sizeof(tAniMacAddr);

    return ANI_OK;
}

/**
 * aniAsfPacketMoveLeft
 *
 * FUNCTION:
 * Pretends that a certain number of bytes have been prepended to the
 * packet, without actually copying any bytes in. The packet head and
 * length are appropriately changed. This function is useful while
 * interfacing with other libraries that only support byte array
 * manipulation.
 *
 * WARNING: 
 * Applications are discouraged from using this function
 * because correct usage is a two-step process - one: copy some bytes
 * to the packet's internal buffer, two: move head and length. This
 * violates the encapsulation the packet library aims to provide.
 *
 * @param packet the packet whose head and length needs to be modified
 * @param count the number of bytes to modify by
 *
 * @return ANI_OK if the operation succeeds
 */
int
aniAsfPacketMoveLeft(tAniPacket *packet, v_U32_t count)
{
    if (aniAsfPacketCanPrependBuffer(packet, count) != ANI_OK)
        return ANI_E_FAILED;

    packet->head -= count;
    packet->len += count;

    return ANI_OK;
}

/**
 * aniAsfPacketMoveRight
 *
 * FUNCTION:
 * Pretends that a certain number of bytes have been appended to the
 * packet, without actually copying any bytes in. The packet tail and
 * length are appropriately changed. This function is useful while
 * interfacing with other libraries that only support byte array
 * manipulation.
 *
 * WARNING: 
 * Applications are discouraged from using this function
 * because correct usage is a two-step process - one: copy some bytes
 * to the packet's internal buffer, two: move tail and length. This
 * violates the encapsulation the packet library aims to provide.
 *
 * @param packet the packet whose head and length needs to be modified
 * @param count the number of bytes to modify by
 *
 * @return ANI_OK if the operation succeeds
 */
int
aniAsfPacketMoveRight(tAniPacket *packet, v_U32_t count)
{
    if (aniAsfPacketCanAppendBuffer(packet, count) != ANI_OK)
        return ANI_E_FAILED;

    packet->tail += count;
    packet->len += count;

    return ANI_OK;
}

/**
 * aniAsfPacketGetBytesFromTail
 *
 * FUNCTION:
 * Returns a pointer to the tail of the valid data stored 
 * in the packet.
 *
 * WARNING: 
 * Applications are discouraged from using this function
 * because correct usage is a three-step process - one: call this
 * routine to obtain a pointer to the current tail of the packet. 
 * two: treat this returned pointer like a simple array and copy 
 * some bytes to the packet's internal buffer, and finally 
 * three: move tail and length. This violates the encapsulation 
 * the packet library aims to provide.
 *
 * @param packet the packet whose bytes we need
 * @param rawBytesPtr the pointer that will be set the start of the
 * raw bytes.
 *
 * @return The non-negative number of bytes stored in the packet if
 * the operation succeeded. That is the same value as what would be
 * returned by aniAsfPacketGetLen.
 */
int
aniAsfPacketGetBytesFromTail(tAniPacket *packet, v_U8_t **rawBytesPtr)
{
    if (packet == NULL)
        return ANI_E_NULL_VALUE;

    *rawBytesPtr = packet->tail;
    return 0; // The length of used bytes returned is zero
}

