/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _MPQ_STREAM_BUFFER_H
#define _MPQ_STREAM_BUFFER_H

#include "dvb_ringbuffer.h"


/**
 * DOC: MPQ Stream Buffer
 *
 * A stream buffer implmenetation used to transfer data between two units
 * such as demux and decoders. The implementation relies on dvb_ringbuffer
 * implementation. Refer to dvb_ringbuffer.h for details.
 *
 * The implementation uses two dvb_ringbuffers, one to pass the
 * raw-data (PES payload for example) and the other to pass
 * meta-data (information from PES header for example).
 *
 * The meta-data uses dvb_ringbuffer packet interface. Each meta-data
 * packet hold the address and size of raw-data described by the
 * meta-data packet, in addition to user's own parameters if any required.
 *
 * Contrary to dvb_ringbuffer implementation, this API makes sure there's
 * enough data to read/write when making read/write operations.
 * Users interested to flush/reset specific buffer, check for bytes
 * ready or space available for write should use the respective services
 * in dvb_ringbuffer (dvb_ringbuffer_avail, dvb_ringbuffer_free,
 * dvb_ringbuffer_reset, dvb_ringbuffer_flush,
 * dvb_ringbuffer_flush_spinlock_wakeup).
 *
 * Concurrency protection is handled in the same manner as in
 * dvb_ringbuffer implementation.
 *
 * Typical call flow from producer:
 *
 * - Start writting the raw-data of new packet, the following call is
 *   repeated until end of data of the specific packet
 *
 *     mpq_streambuffer_data_write(...)
 *
 * - Now write a new packet describing the new available raw-data
 *     mpq_streambuffer_pkt_write(...)
 *
 * Typical call flow from consumer:
 *
 * - Poll for next available packet:
 *      mpq_streambuffer_pkt_next(&streambuff,-1)
 *
 *   In different approach, consumer can wait on event for new data and then
 *   call mpq_streambuffer_pkt_next, waiting for data can be done as follows:
 *
 *      wait_event_interruptible(
 *			streambuff->packet_data->queue,
 *			!dvb_ringbuffer_empty(&streambuff->packet_data) ||
 *			(streambuff->packet_data.error != 0);
 *
 * - Get the new packet information:
 *      mpq_streambuffer_pkt_read(..)
 *
 * - Read the raw-data of the new packet. Here you can use two methods:
 *
 *   1. Read the data to a user supplied buffer:
 *         mpq_streambuffer_data_read()
 *
 *      In this case memory copy is done, read pointer is updated in the raw
 *      data buffer, the amount of raw-data is provided part of the
 *      packet's information. User should then call mpq_streambuffer_pkt_dispose
 *      with dispose_data set to 0 as the raw-data was already disposed.
 *
 *   2. Access the data directly using the raw-data address. The address
 *      of the raw data is provided part of the packet's information. User
 *      then should call mpq_streambuffer_pkt_dispose with dispose_data set
 *      to 1 to dispose the packet along with it's raw-data.
 */

/**
 * struct mpq_streambuffer - mpq stream buffer representation
 *
 * @raw_data: The buffer used to hold the raw-data
 * @packet_data: The buffer user to hold the meta-data
 */
struct mpq_streambuffer {
	struct dvb_ringbuffer raw_data;
	struct dvb_ringbuffer packet_data;
};

/**
 * struct mpq_streambuffer_packet_header - packet header saved in packet buffer
 * @user_data_len: length of private user (meta) data
 * @raw_data_addr: raw-data address in the raw-buffer described by the packet
 * @raw_data_len: size of raw-data in the raw-data buffer (can be 0)
 *
 * The packet structure that is saved in each packet-buffer:
 * user_data_len
 * raw_data_addr
 * raw_data_len
 * private user-data bytes
 */
struct mpq_streambuffer_packet_header {
	u32 user_data_len;
	u32	raw_data_addr;
	u32	raw_data_len;
} __packed;

/**
 * mpq_streambuffer_init - Initialize a new stream buffer
 *
 * @sbuff: The buffer to initialize
 * @data_buff: The buffer holding raw-data
 * @data_buff_len: Size of raw-data buffer
 * @packet_buff: The buffer holding meta-data
 * @packet_buff_size: Size of meta-data buffer
 */
void mpq_streambuffer_init(
		struct mpq_streambuffer *sbuff,
		void *data_buff, size_t data_buff_len,
		void *packet_buff, size_t packet_buff_size);

/**
 * mpq_streambuffer_packet_next - Returns index of next avaialble packet.
 *
 * @sbuff: The stream buffer
 * @idx: Previous packet index or -1 to return index of the the first
 *       available packet.
 * @pktlen: The length of the ready packet
 *
 * Return index to the packet-buffer, -1 if buffer is empty
 *
 * After getting the index, the user of this function can either
 * access the packet buffer directly using the returned index
 * or ask to read the data back from the buffer using mpq_ringbuffer_pkt_read
 */
ssize_t mpq_streambuffer_pkt_next(
		struct mpq_streambuffer *sbuff,
		ssize_t idx, size_t *pktlen);

/**
 * mpq_streambuffer_pkt_read - Reads out the packet from the provided index.
 *
 * @sbuff: The stream buffer
 * @idx: The index of the packet to be read
 * @packet: The read packet's header
 * @user_data: The read private user data
 *
 * Return  The actual number of bytes read, -EINVAL if the packet is
 * already disposed or the packet-data is invalid.
 *
 * The packet is not disposed after this function is called, to dispose it
 * along with the raw-data it points to use mpq_streambuffer_pkt_dispose.
 * If there are no private user-data, the user-data pointer can be NULL.
 * The caller of this function must make sure that the private user-data
 * buffer has enough space for the private user-data length
 */
ssize_t mpq_streambuffer_pkt_read(
		struct mpq_streambuffer *sbuff,
		size_t idx,
		struct mpq_streambuffer_packet_header *packet,
		u8 *user_data);

/**
 * mpq_streambuffer_pkt_dispose - Disposes a packet from the packet buffer
 *
 * @sbuff: The stream buffer
 * @idx: The index of the packet to be disposed
 * @dispose_data: Indicates whether to update the read pointer inside the
 * raw-data buffer for the respective data pointed by the packet.
 *
 * Return  error status, -EINVAL if the packet-data is invalid
 *
 * The function updates the read pointer inside the raw-data buffer
 * for the respective data pointed by the packet if dispose_data is set.
 */
int mpq_streambuffer_pkt_dispose(
		struct mpq_streambuffer *sbuff,
		size_t idx,
		int dispose_data);

/**
 * mpq_streambuffer_pkt_write - Write a new packet to the packet buffer.
 *
 * @sbuff: The stream buffer
 * @packet: The packet header to write
 * @user_data: The private user-data to be written
 *
 * Return  error status, -ENOSPC if there's no space to write the packet
 */
int mpq_streambuffer_pkt_write(
		struct mpq_streambuffer *sbuff,
		struct mpq_streambuffer_packet_header *packet,
		u8 *user_data);

/**
 * mpq_streambuffer_data_write - Write data to raw-data buffer
 *
 * @sbuff: The stream buffer
 * @buf: The buffer holding the data to be written
 * @len: The length of the data buffer
 *
 * Return  The actual number of bytes written or -ENOSPC if
 *			no space to write the data
 */
ssize_t mpq_streambuffer_data_write(
		struct mpq_streambuffer *sbuff,
		const u8 *buf, size_t len);

/**
 * mpq_streambuffer_data_write_deposit - Advances the raw-buffer write pointer.
 * Assumes the raw-data was written by the user directly
 *
 * @sbuff: The stream buffer
 * @len: The length of the raw-data that was already written
 *
 * Return  error status
 */
int mpq_streambuffer_data_write_deposit(
		struct mpq_streambuffer *sbuff,
		size_t len);

/**
 * mpq_streambuffer_data_read - Reads out raw-data to the provided buffer.
 *
 * @sbuff: The stream buffer
 * @buf: The buffer to read the raw-data data to
 * @len: The length of the buffer that will hold the raw-data
 *
 * Return  The actual number of bytes read
 *
 * This fucntion copies the data from the ring-buffer to the
 * provided buf parameter. The user can save the extra copy by accessing
 * the data pointer directly and reading from it, then update the
 * read pointer by the amount of data that was read using
 * mpq_streambuffer_data_read_dispose
 */
size_t mpq_streambuffer_data_read(
		struct mpq_streambuffer *sbuff,
		u8 *buf, size_t len);

/**
 * mpq_streambuffer_data_read_dispose - Advances the raw-buffer read pointer.
 * Assumes the raw-data was read by the user directly.
 *
 * @sbuff: The stream buffer
 * @len: The length of the raw-data to be disposed
 *
 * Return  error status, -EINVAL if buffer there's no enough data to
 *			be disposed
 *
 * The user can instead dipose a packet along with the data in the
 * raw-data buffer using mpq_streambuffer_pkt_dispose.
 */
int mpq_streambuffer_data_read_dispose(
		struct mpq_streambuffer *sbuff,
		size_t len);



#endif /* _MPQ_STREAM_BUFFER_H */

