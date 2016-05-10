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
 * File: $File: //depot/software/projects/feature_branches/nova_phase1/ap/apps/include/aniAsfPacket.h $
 * Contains declarations for packet manipulation routines that make it
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
#ifndef _ANI_ASF_PACKET_H_
#define _ANI_ASF_PACKET_H_

#include "vos_types.h"
#include "palTypes.h"

#define ANI_ETH_FRAME_LEN 1516
#define ANI_DEFAULT_PACKET_SIZE (ANI_ETH_FRAME_LEN*2)

/**
 * Opaque packet structure with internal storage for raw bytes.
 * Conceptually, a tPacket is a pre-allocated buffer that contains
 * data in the middle and free space on either side. The start of the
 * data is called the head. Routines are provided to add data at the
 * front or at the rear. The length of the packet is the total number
 * of valid data bytes contained in it. The size of the packet is the
 * total number of preallocated bytes.
 */
typedef struct tAniPacket tAniPacket;

/**
 * aniAsfPacketAllocate
 *
 * FUNCTION:
 * Create a packet of size 2*ANI_DEFAULT_PACKET_SIZE and positions the
 * head of the packet in the center. The allocated storage can be free
 * with a call to aniAsfPacketFree.
 *
 * LOGIC:
 * Allocates storage for tPacket and its internal raw data
 * buffer. Positions the head and tail pointers in the middle of the
 * raw data buffer.
 *
 * @param packetPtr pointer that will be set to newly allocated
 * tPacket if the operation succeeds.
 *
 * @return ANI_OK if the operation succeeds; ANI_E_MALLOC_FAILED if
 * memory could not be allocated.
 * @see aniAsfPacketFree
 */
int
aniAsfPacketAllocate(tAniPacket **packetPtr);

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
 * @return ANI_OK if the operation succeeds
 */
int
aniAsfPacketDuplicate(tAniPacket **newPacketPtr, tAniPacket *oldPacket);

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
 * Allocates storage for tPacket and its internal raw data
 * buffer. Positions the head and tail pointers at the given offset in
 * the internal raw data buffer.
 *
 * @param packetPtr pointer that will be set to newly allocated
 * tPacket if the operation succeeds.
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
                          v_U32_t offset);

/**
 * aniAsfPacketFree
 *
 * FUNCTION:
 * Free a previously allocated tPacket and its internal raw data
 * buffer.
 *
 * @param packet the packet to free
 *
 * @return ANI_OK if the operation succeeds; ANI_E_NULL_VALUE if an
 * unexpected NULL pointer is encountered
 */
int
aniAsfPacketFree(tAniPacket *packet);

/**
 * aniAsfPacket2Str
 *
 * FUNCTION:
 * Returns a printable representation of the data contained in the
 * packet. 
 * Note: This function returns a static buffer used by aniAsfHexStr.
 *
 * @param packet the packet whose contents need to be printed
 */
v_U8_t *aniAsfPacket2Str(tAniPacket *packet);

/**
 * aniAsfPacketAppendBuffer
 *
 * FUNCTION:
 * Appends the data contained in buf to the end of the data in
 * destPacket. The head of destPacket remains unchanged, while its
 * length increases by len.
 *
 * If there isn't enough free space in destPacket for all len bytes
 * then the routine fails and the length of destPacket remains
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
                      v_U32_t len);

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
                       v_U32_t len);

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
                         v_U32_t len);

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
                          v_U32_t len);

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
                              v_U32_t len);

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
                             v_U32_t len);

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
aniAsfPacketGetLen(tAniPacket *packet);

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
aniAsfPacketGetBytes(tAniPacket *packet, v_U8_t **rawBytesPtr);

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
aniAsfPacketGetN(tAniPacket *packet, int n, v_U8_t **bytesPtr);

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
aniAsfPacketEmpty(tAniPacket *packet);

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
                          v_U32_t offset);


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
aniAsfPacketPrependHdr(tAniPacket *packet, v_U16_t msgType);

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
aniAsfPacketGet32(tAniPacket *packet, v_U32_t *val);

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
aniAsfPacketAppend32(tAniPacket *packet, v_U32_t val);

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
aniAsfPacketGet16(tAniPacket *packet, v_U16_t *val);

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
aniAsfPacketPrepend16(tAniPacket *packet, v_U16_t val);

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
aniAsfPacketAppend16(tAniPacket *packet, v_U16_t val);

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
aniAsfPacketGet8(tAniPacket *packet, v_U8_t *val);

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
aniAsfPacketPrepend8(tAniPacket *packet, v_U8_t val);

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
aniAsfPacketAppend8(tAniPacket *packet, v_U8_t val);

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
aniAsfPacketGetMac(tAniPacket *packet, tAniMacAddr macAddr);

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
aniAsfPacketMoveLeft(tAniPacket *packet, v_U32_t count);

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
aniAsfPacketMoveRight(tAniPacket *packet, v_U32_t count);

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
aniAsfPacketGetBytesFromTail(tAniPacket *packet, v_U8_t **rawBytesPtr);


#endif // _ANI_ASF_PACKET_H_
