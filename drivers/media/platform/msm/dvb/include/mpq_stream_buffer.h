/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _MPQ_STREAM_BUFFER_H
#define _MPQ_STREAM_BUFFER_H

#include <media/dvb_ringbuffer.h>

/**
 * DOC: MPQ Stream Buffer
 *
 * A stream buffer implementation is used to transfer data between two units
 * such as demux and decoders. The implementation relies on dvb_ringbuffer
 * implementation. Refer to dvb_ringbuffer.h for details.
 *
 * The implementation uses two dvb_ringbuffers, one to pass the
 * raw-data (PES payload for example) and the other to pass
 * meta-data (information from PES header for example).
 *
 * The meta-data uses dvb_ringbuffer packet interface. Each meta-data
 * packet points to the data buffer, and includes the offset to the data in the
 * buffer, the size of raw-data described by the meta-data packet, and also the
 * size of user's own parameters if any required.
 *
 * Data can be managed in two ways: ring-buffer & linear buffers, as specified
 * in initialization when calling the mpq_streambuffer_init function.
 * For managing data as a ring buffer exactly 1 data buffer descriptor must be
 * specified in initialization. For this mode, dvb_ringbuffer is used "as-is".
 * For managing data in several linear buffers, an array of buffer descriptors
 * must be passed.
 * For both modes, data descriptor(s) must be remain valid throughout the life
 * span of the mpq_streambuffer object.
 * Apart from initialization API remains the same for both modes.
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
 * - Start writing the raw-data of new packet, the following call is
 *   repeated until end of data of the specific packet
 *
 *      mpq_streambuffer_data_write(...)
 *
 * - Now write a new packet describing the new available raw-data
 *      mpq_streambuffer_pkt_write(...)
 *
 *   For linear buffer mode, writing a new packet with data size > 0, causes the
 *   current buffer to be marked as pending for reading, and triggers moving to
 *   the next available buffer, that shall now be the current write buffer.
 *
 * Typical call flow from consumer:
 *
 * - Poll for next available packet:
 *      mpq_streambuffer_pkt_next(&streambuff,-1,&len)
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
 *      Note that secure buffer cannot be accessed directly and an error will
 *      occur.
 *
 *   2. Access the data directly using the raw-data address. The address
 *      of the raw data is provided part of the packet's information. User
 *      then should call mpq_streambuffer_pkt_dispose with dispose_data set
 *      to 1 to dispose the packet along with it's raw-data.
 *
 * - Disposal of packets:
 *      mpq_streambuffer_pkt_dispose(...)
 *
 *   For linear buffer mode, disposing of a packet with data size > 0,
 *   regardless of the 'dispose_data' parameter, causes the current buffer's
 *   data to be disposed and marked as free for writing, and triggers moving to
 *   the next available buffer, that shall now be the current read buffer.
 */

struct mpq_streambuffer;
struct mpq_streambuffer_packet_header;

typedef void (*mpq_streambuffer_dispose_cb) (
	struct mpq_streambuffer *sbuff,
	u32 offset,
	size_t len,
	void *user_data);

enum mpq_streambuffer_mode {
	MPQ_STREAMBUFFER_BUFFER_MODE_RING,
	MPQ_STREAMBUFFER_BUFFER_MODE_LINEAR
};

/**
 * struct mpq_streambuffer - mpq stream buffer representation
 *
 * @raw_data: The buffer used to hold raw-data, or linear buffer descriptors
 * @packet_data: The buffer user to hold the meta-data
 * @buffers: array of buffer descriptor(s) holding buffer initial & dynamic
 *	     buffer information
 * @mode: mpq_streambuffer buffer management work mode - Ring-buffer or Linear
 *	  buffers
 * @buffers_num: number of data buffers to manage
 * @pending_buffers_count: for linear buffer management, counts the number of
 * buffer that has been
 */
struct mpq_streambuffer {
	struct dvb_ringbuffer raw_data;
	struct dvb_ringbuffer packet_data;
	struct mpq_streambuffer_buffer_desc *buffers;
	enum mpq_streambuffer_mode mode;
	u32 buffers_num;
	u32 pending_buffers_count;
	mpq_streambuffer_dispose_cb cb;
	void *cb_user_data;
};

/**
 * mpq_streambuffer_linear_desc
 * @handle:	ION handle's file descriptor of buffer
 * @base:	kernel mapped address to start of buffer.
 *		Can be NULL for secured buffers
 * @size:	size of buffer
 * @read_ptr:	initial read pointer value (should normally be 0)
 * @write_ptr:	initial write pointer value (should normally be 0)
 */
struct mpq_streambuffer_buffer_desc {
	int	handle;
	void	*base;
	u32	size;
	u32	read_ptr;
	u32	write_ptr;
};

/**
 * struct mpq_streambuffer_packet_header - packet header saved in packet buffer
 * @user_data_len: length of private user (meta) data
 * @raw_data_handle: ION handle's file descriptor of raw-data buffer
 * @raw_data_offset: offset of raw-data from start of buffer (0 for linear)
 * @raw_data_len: size of raw-data in the raw-data buffer (can be 0)
 *
 * The packet structure that is saved in each packet-buffer:
 * user_data_len
 * raw_data_handle
 * raw_data_offset
 * raw_data_len
 * private user-data bytes
 */
struct mpq_streambuffer_packet_header {
	u32 user_data_len;
	int raw_data_handle;
	u32 raw_data_offset;
	u32 raw_data_len;
} __packed;

/**
 * mpq_streambuffer_init - Initialize a new stream buffer
 *
 * @sbuff: The buffer to initialize
 * @data_buffers: array of data buffer descriptor(s).
 *		  Data descriptor(s) must be remain valid throughout the life
 *		  span of the mpq_streambuffer object
 * @data_buff_num: number of data buffer in array
 * @packet_buff: The buffer holding meta-data
 * @packet_buff_size: Size of meta-data buffer
 *
 * Return	Error status, -EINVAL if any of the arguments are invalid
 *
 * Note:
 * for data_buff_num > 1, mpq_streambuffer object manages these buffers as a
 * separated set of linear buffers. A linear buffer cannot wrap-around and one
 * can only write as many data bytes as the buffer's size. Data will not be
 * written to the next free buffer.
 */
int mpq_streambuffer_init(
		struct mpq_streambuffer *sbuff,
		enum mpq_streambuffer_mode mode,
		struct mpq_streambuffer_buffer_desc *data_buffers,
		u32 data_buff_num,
		void *packet_buff,
		size_t packet_buff_size);

/**
 * mpq_streambuffer_terminate - Terminate stream buffer
 *
 * @sbuff: The buffer to terminate
 *
 * The function sets the the buffers error flags to ENODEV
 * and wakeup any waiting threads on the buffer queues.
 * Threads waiting on the buffer queues should check if
 * error was set.
 */
void mpq_streambuffer_terminate(struct mpq_streambuffer *sbuff);

/**
 * mpq_streambuffer_packet_next - Returns index of next available packet.
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
 * Return  The actual number of bytes read or error code
 *
 * This function copies the data from the ring-buffer to the
 * provided buf parameter. The user can save the extra copy by accessing
 * the data pointer directly and reading from it, then update the
 * read pointer by the amount of data that was read using
 * mpq_streambuffer_data_read_dispose
 */
ssize_t mpq_streambuffer_data_read(
		struct mpq_streambuffer *sbuff,
		u8 *buf, size_t len);

/**
 * mpq_streambuffer_data_read_user
 *
 * Same as mpq_streambuffer_data_read except data can be copied to user-space
 * buffer.
 */
ssize_t mpq_streambuffer_data_read_user(
		struct mpq_streambuffer *sbuff,
		u8 __user *buf, size_t len);

/**
 * mpq_streambuffer_data_read_dispose - Advances the raw-buffer read pointer.
 * Assumes the raw-data was read by the user directly.
 *
 * @sbuff:	The stream buffer
 * @len:	The length of the raw-data to be disposed
 *
 * Return  error status, -EINVAL if buffer there's no enough data to
 *			be disposed
 *
 * The user can instead dispose a packet along with the data in the
 * raw-data buffer using mpq_streambuffer_pkt_dispose.
 */
int mpq_streambuffer_data_read_dispose(
		struct mpq_streambuffer *sbuff,
		size_t len);
/**
 * mpq_streambuffer_get_buffer_handle - Returns the current linear buffer
 * ION handle.
 * @sbuff: The stream buffer
 * @read_buffer: specifies if a read buffer handle is requested (when set),
 *		 or a write buffer handle is requested.
 *		 For linear buffer mode read & write buffers may be different
 *		 buffers. For ring buffer mode, the same (single) buffer handle
 *		 is returned.
 * buffer handle
 * @handle: returned handle
 *
 * Return error status
 * -EINVAL is arguments are invalid.
 * -EPERM if stream buffer specified was not initialized with linear support.
 */
int mpq_streambuffer_get_buffer_handle(
	struct mpq_streambuffer *sbuff,
	int read_buffer,
	int *handle);

/**
 * mpq_streambuffer_data_free - Returns number of free bytes in data buffer.
 * @sbuff: The stream buffer object
 *
 * Note: for linear buffer management this return number of free bytes in the
 * current write buffer only.
 */
ssize_t mpq_streambuffer_data_free(
	struct mpq_streambuffer *sbuff);

/**
 * mpq_streambuffer_data_avail - Returns number of bytes in data buffer that
 * can be read.
 * @sbuff: The stream buffer object
 *
 * Note: for linear buffer management this return number of data bytes in the
 * current read buffer only.
 */
ssize_t mpq_streambuffer_data_avail(
	struct mpq_streambuffer *sbuff);

/**
 * mpq_streambuffer_register_pkt_dispose - Registers a callback to notify on
 * packet disposal events.
 * can be read.
 * @sbuff: The stream buffer object
 * @cb_func: user callback function
 * @user_data: user data to be passed to callback function.
 *
 * Returns error status
 * -EINVAL if arguments are invalid
 */
int mpq_streambuffer_register_data_dispose(
	struct mpq_streambuffer *sbuff,
	mpq_streambuffer_dispose_cb cb_func,
	void *user_data);

/**
 * mpq_streambuffer_data_rw_offset - returns read/write offsets of current data
 * buffer.
 * @sbuff: The stream buffer object
 * @read_offset: returned read offset
 * @write_offset: returned write offset
 *
 * Note: read offset or write offset may be NULL if not required.
 * Returns error status
 * -EINVAL if arguments are invalid
 */
int mpq_streambuffer_get_data_rw_offset(
	struct mpq_streambuffer *sbuff,
	u32 *read_offset,
	u32 *write_offset);

/**
 * mpq_streambuffer_metadata_free - returns number of free bytes in the meta
 * data buffer, or error status.
 * @sbuff: the stream buffer object
 */
ssize_t mpq_streambuffer_metadata_free(struct mpq_streambuffer *sbuff);

/**
 * mpq_streambuffer_flush - flush both pending packets and data in buffer
 *
 * @sbuff: the stream buffer object
 *
 * Returns error status
 */
int mpq_streambuffer_flush(struct mpq_streambuffer *sbuff);

/*
 *  ------------------------------------------------------
 *  Consumer or AV Decoder Stream Interface to Ring Buffer
 *  ------------------------------------------------------
 *  Producer is Demux Driver
 *  ------------------------
 *
 *  call from Audio/Video Decoder Driver to find Audio/Video
 *  streambuffer AV handles, "DMX_PES_AUDIO0 through 3" or
 *  DMX_PES_VIDEO0 through 3" interfaces corresponding to 4 programs.
 */

/* call from Audio/Video Decoder Driver via POLLING to consume
 * Headers and Compressed data from ring buffer using streambuffer handle.
 * hdrdata[] and cdata[] buffers have to be malloc'd by consumer
 *
 *  --------------------------
 *  Consumer Calling Sequence
 *  --------------------------
 *  Find the streambuffer corresponding to a DMX TS PES stream instance.
 *  1. consumer_audio_streambuffer() or consumer_video_streambuffer()
 *  Process the packet headers if required.
 *  2. mpq_read_new_packet_hdr_data()
 *  Process the compressed data by forwarding to AV decoder.
 *  3. mpq_read_new_packet_compressed_data()
 *  Dispose the packet.
 *  4. mpq_dispose_new_packet_read()
 *
 *  The Audio/Video drivers (or consumers) require the stream_buffer information
 *  for consuming packet headers and compressed AV data from the
 *  ring buffer filled by demux driver which is the producer
 */

#endif /* _MPQ_STREAM_BUFFER_H */
