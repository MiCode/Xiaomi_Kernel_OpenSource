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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include "mpq_dvb_debug.h"
#include "mpq_stream_buffer.h"


void mpq_streambuffer_init(
		struct mpq_streambuffer *sbuff,
		void *data_buff, size_t data_buff_len,
		void *packet_buff, size_t packet_buff_size)
{
	dvb_ringbuffer_init(&sbuff->raw_data, data_buff, data_buff_len);
	dvb_ringbuffer_init(&sbuff->packet_data, packet_buff, packet_buff_size);
}
EXPORT_SYMBOL(mpq_streambuffer_init);


ssize_t mpq_streambuffer_pkt_next(
		struct mpq_streambuffer *sbuff,
		ssize_t idx, size_t *pktlen)
{
	return dvb_ringbuffer_pkt_next(&sbuff->packet_data, idx, pktlen);
}
EXPORT_SYMBOL(mpq_streambuffer_pkt_next);


ssize_t mpq_streambuffer_pkt_read(
		struct mpq_streambuffer *sbuff,
		size_t idx,
		struct mpq_streambuffer_packet_header *packet,
		u8 *user_data)
{
	size_t ret;
	size_t read_len;

	/* read-out the packet header first */
	ret = dvb_ringbuffer_pkt_read(
				&sbuff->packet_data, idx, 0,
				(u8 *)packet,
				sizeof(struct mpq_streambuffer_packet_header));

	/* verify length, at least packet header should exist */
	if (ret != sizeof(struct mpq_streambuffer_packet_header))
		return -EINVAL;

	read_len = ret;

	/* read-out private user-data if there are such */
	if ((packet->user_data_len) && (user_data != NULL)) {
		ret = dvb_ringbuffer_pkt_read(
				&sbuff->packet_data,
				idx,
				sizeof(struct mpq_streambuffer_packet_header),
				user_data,
				packet->user_data_len);

		if (ret < 0)
			return ret;

		read_len += ret;
	}

	return read_len;
}
EXPORT_SYMBOL(mpq_streambuffer_pkt_read);


int mpq_streambuffer_pkt_dispose(
			struct mpq_streambuffer *sbuff,
			size_t idx,
			int dispose_data)
{
	int ret;
	struct mpq_streambuffer_packet_header packet;

	if (dispose_data) {
		/* read-out the packet header first */
		ret = dvb_ringbuffer_pkt_read(
				&sbuff->packet_data,
				idx,
				0,
				(u8 *)&packet,
				sizeof(struct mpq_streambuffer_packet_header));

		if (ret != sizeof(struct mpq_streambuffer_packet_header))
			return -EINVAL;

		/* Advance the read pointer in the raw-data buffer first */
		ret = mpq_streambuffer_data_read_dispose(
							sbuff,
							packet.raw_data_len);
		if (ret != 0)
			return ret;
	}

	/* Now clear the packet from the packet header */
	dvb_ringbuffer_pkt_dispose(&sbuff->packet_data, idx);

	return 0;
}
EXPORT_SYMBOL(mpq_streambuffer_pkt_dispose);


int mpq_streambuffer_pkt_write(
			struct mpq_streambuffer *sbuff,
			struct mpq_streambuffer_packet_header *packet,
			u8 *user_data)
{
	ssize_t idx;
	size_t len;

	len =
		sizeof(struct mpq_streambuffer_packet_header) +
		packet->user_data_len;

	/* Make sure enough space available for packet header */
	if (dvb_ringbuffer_free(&sbuff->packet_data) < len)
		return -ENOSPC;

	/* Starting writting packet header */
	idx = dvb_ringbuffer_pkt_start(&sbuff->packet_data, len);

	/* Write non-user private data header */
	dvb_ringbuffer_write(
				&sbuff->packet_data,
				(u8 *)packet,
				sizeof(struct mpq_streambuffer_packet_header));

	/* Write user's own private data header */
	dvb_ringbuffer_write(&sbuff->packet_data,
						 user_data,
						 packet->user_data_len);

	dvb_ringbuffer_pkt_close(&sbuff->packet_data, idx);

	wake_up_all(&sbuff->packet_data.queue);

	return 0;
}
EXPORT_SYMBOL(mpq_streambuffer_pkt_write);


ssize_t mpq_streambuffer_data_write(
			struct mpq_streambuffer *sbuff,
			const u8 *buf, size_t len)
{
	int res;

	if (unlikely(dvb_ringbuffer_free(&sbuff->raw_data) < len))
		return -ENOSPC;

	res = dvb_ringbuffer_write(&sbuff->raw_data, buf, len);
	wake_up_all(&sbuff->raw_data.queue);

	return res;
}
EXPORT_SYMBOL(mpq_streambuffer_data_write);


int mpq_streambuffer_data_write_deposit(
				struct mpq_streambuffer *sbuff,
				size_t len)
{
	if (unlikely(dvb_ringbuffer_free(&sbuff->raw_data) < len))
		return -ENOSPC;

	sbuff->raw_data.pwrite =
		(sbuff->raw_data.pwrite+len) % sbuff->raw_data.size;

	wake_up_all(&sbuff->raw_data.queue);

	return 0;
}
EXPORT_SYMBOL(mpq_streambuffer_data_write_deposit);


size_t mpq_streambuffer_data_read(
				struct mpq_streambuffer *sbuff,
				u8 *buf, size_t len)
{
	int actual_len;

	actual_len = dvb_ringbuffer_avail(&sbuff->raw_data);
	if (actual_len < len)
		len = actual_len;

	if (actual_len)
		dvb_ringbuffer_read(&sbuff->raw_data, buf, actual_len);

	wake_up_all(&sbuff->raw_data.queue);

	return actual_len;
}
EXPORT_SYMBOL(mpq_streambuffer_data_read);


int mpq_streambuffer_data_read_dispose(
			struct mpq_streambuffer *sbuff,
			size_t len)
{
	if (unlikely(dvb_ringbuffer_avail(&sbuff->raw_data) < len))
		return -EINVAL;

	DVB_RINGBUFFER_SKIP(&sbuff->raw_data, len);

	wake_up_all(&sbuff->raw_data.queue);
	return 0;
}
EXPORT_SYMBOL(mpq_streambuffer_data_read_dispose);

