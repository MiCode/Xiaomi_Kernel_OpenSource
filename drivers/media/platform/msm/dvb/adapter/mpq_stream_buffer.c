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
#include <linux/uaccess.h>
#include "mpq_dvb_debug.h"
#include "mpq_stream_buffer.h"




int mpq_streambuffer_init(
		struct mpq_streambuffer *sbuff,
		enum mpq_streambuffer_mode mode,
		struct mpq_streambuffer_buffer_desc *data_buffers,
		u32 data_buff_num,
		void *packet_buff,
		size_t packet_buff_size)
{
	if ((NULL == sbuff) || (NULL == data_buffers) || (NULL == packet_buff))
		return -EINVAL;

	if (data_buff_num > 1) {
		if (mode != MPQ_STREAMBUFFER_BUFFER_MODE_LINEAR)
			return -EINVAL;
		/* Linear buffer group */
		dvb_ringbuffer_init(
			&sbuff->raw_data,
			data_buffers,
			data_buff_num *
			sizeof(struct mpq_streambuffer_buffer_desc));
	} else if (data_buff_num == 1) {
		if (mode != MPQ_STREAMBUFFER_BUFFER_MODE_RING)
			return -EINVAL;
		/* Single ring-buffer */
		dvb_ringbuffer_init(&sbuff->raw_data,
			data_buffers[0].base, data_buffers[0].size);
	}
	sbuff->mode = mode;
	sbuff->buffers = data_buffers;
	sbuff->pending_buffers_count = 0;
	sbuff->buffers_num = data_buff_num;
	dvb_ringbuffer_init(&sbuff->packet_data, packet_buff, packet_buff_size);

	return 0;
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

	if (NULL == sbuff)
		return -EINVAL;

	/* read-out the packet header first */
	ret = dvb_ringbuffer_pkt_read(&sbuff->packet_data, idx,
			0,
			(u8 *)&packet,
			sizeof(struct mpq_streambuffer_packet_header));

	if (ret != sizeof(struct mpq_streambuffer_packet_header))
		return -EINVAL;

	if ((MPQ_STREAMBUFFER_BUFFER_MODE_LINEAR == sbuff->mode) ||
		(dispose_data)) {
		/* Advance the read pointer in the raw-data buffer first */
		ret = mpq_streambuffer_data_read_dispose(sbuff,
				packet.raw_data_len);
		if (ret != 0)
			return ret;
	}

	/* Move read pointer to the next linear buffer for subsequent reads */
	if ((MPQ_STREAMBUFFER_BUFFER_MODE_LINEAR == sbuff->mode) &&
		(packet.raw_data_len > 0)) {
		struct mpq_streambuffer_buffer_desc *desc;

		desc = (struct mpq_streambuffer_buffer_desc *)
				&sbuff->raw_data.data[sbuff->raw_data.pread];

		desc->write_ptr = 0;
		desc->read_ptr = 0;

		DVB_RINGBUFFER_SKIP(&sbuff->raw_data,
				sizeof(struct mpq_streambuffer_buffer_desc));
		sbuff->pending_buffers_count--;

		wake_up_all(&sbuff->raw_data.queue);
	}

	/* Now clear the packet from the packet header */
	dvb_ringbuffer_pkt_dispose(&sbuff->packet_data, idx);

	if (sbuff->cb)
		sbuff->cb(sbuff, sbuff->cb_user_data);

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

	if ((NULL == sbuff) || (NULL == packet))
		return -EINVAL;

	MPQ_DVB_DBG_PRINT(
		"%s: handle=%d, offset=%d, len=%d\n",
		__func__,
		packet->raw_data_handle,
		packet->raw_data_offset,
		packet->raw_data_len);

	len = sizeof(struct mpq_streambuffer_packet_header) +
		packet->user_data_len;

	/* Make sure enough space available for packet header */
	if (dvb_ringbuffer_free(&sbuff->packet_data) < len)
		return -ENOSPC;

	/* Starting writing packet header */
	idx = dvb_ringbuffer_pkt_start(&sbuff->packet_data, len);

	/* Write non-user private data header */
	dvb_ringbuffer_write(&sbuff->packet_data,
		(u8 *)packet,
		sizeof(struct mpq_streambuffer_packet_header));

	/* Write user's own private data header */
	dvb_ringbuffer_write(&sbuff->packet_data,
		user_data,
		packet->user_data_len);

	dvb_ringbuffer_pkt_close(&sbuff->packet_data, idx);

	/* Move write pointer to next linear buffer for subsequent writes */
	if ((MPQ_STREAMBUFFER_BUFFER_MODE_LINEAR == sbuff->mode) &&
		(packet->raw_data_len > 0)) {
		if (sbuff->pending_buffers_count == sbuff->buffers_num)
			return -ENOSPC;
		DVB_RINGBUFFER_PUSH(&sbuff->raw_data,
				sizeof(struct mpq_streambuffer_buffer_desc));
		sbuff->pending_buffers_count++;
	}

	wake_up_all(&sbuff->packet_data.queue);

	return 0;
}
EXPORT_SYMBOL(mpq_streambuffer_pkt_write);


ssize_t mpq_streambuffer_data_write(
			struct mpq_streambuffer *sbuff,
			const u8 *buf, size_t len)
{
	int res;

	if ((NULL == sbuff) || (NULL == buf))
		return -EINVAL;

	if (MPQ_STREAMBUFFER_BUFFER_MODE_RING == sbuff->mode) {
		if (unlikely(dvb_ringbuffer_free(&sbuff->raw_data) < len))
			return -ENOSPC;
		/*
		 * Secure buffers are not permitted to be mapped into kernel
		 * memory, and so buffer base address may be NULL
		 */
		if (NULL == sbuff->raw_data.data)
			return -EPERM;
		res = dvb_ringbuffer_write(&sbuff->raw_data, buf, len);
		wake_up_all(&sbuff->raw_data.queue);
	} else {
		/* Linear buffer group */
		struct mpq_streambuffer_buffer_desc *desc;

		desc = (struct mpq_streambuffer_buffer_desc *)
				&sbuff->raw_data.data[sbuff->raw_data.pwrite];

		/*
		 * Secure buffers are not permitted to be mapped into kernel
		 * memory, and so buffer base address may be NULL
		 */
		if (NULL == desc->base)
			return -EPERM;

		if ((sbuff->pending_buffers_count == sbuff->buffers_num) ||
			((desc->size - desc->write_ptr) < len)) {
			MPQ_DVB_ERR_PRINT(
				"%s: No space available! %d pending buffers out of %d total buffers. write_ptr=%d, size=%d\n",
				__func__,
				sbuff->pending_buffers_count,
				sbuff->buffers_num,
				desc->write_ptr,
				desc->size);
			return -ENOSPC;
		}
		memcpy(desc->base + desc->write_ptr, buf, len);
		desc->write_ptr += len;
		MPQ_DVB_DBG_PRINT(
			"%s: copied %d data bytes. handle=%d, write_ptr=%d\n",
			__func__, len, desc->handle, desc->write_ptr);
		res = len;
	}

	return res;
}
EXPORT_SYMBOL(mpq_streambuffer_data_write);


int mpq_streambuffer_data_write_deposit(
				struct mpq_streambuffer *sbuff,
				size_t len)
{
	if (NULL == sbuff)
		return -EINVAL;

	if (unlikely(dvb_ringbuffer_free(&sbuff->raw_data) < len))
		return -ENOSPC;

	if (MPQ_STREAMBUFFER_BUFFER_MODE_RING == sbuff->mode) {
		DVB_RINGBUFFER_PUSH(&sbuff->raw_data, len);
		wake_up_all(&sbuff->raw_data.queue);
	} else {
		/* Linear buffer group */
		struct mpq_streambuffer_buffer_desc *desc;
		desc = (struct mpq_streambuffer_buffer_desc *)
				&sbuff->raw_data.data[sbuff->raw_data.pwrite];

		if ((sbuff->pending_buffers_count == sbuff->buffers_num) ||
			 ((desc->size - desc->write_ptr) < len)) {
			MPQ_DVB_ERR_PRINT(
				"%s: No space available!\n",
				__func__);
			return -ENOSPC;
		}
		desc->write_ptr += len;
	}

	return 0;
}
EXPORT_SYMBOL(mpq_streambuffer_data_write_deposit);


ssize_t mpq_streambuffer_data_read(
				struct mpq_streambuffer *sbuff,
				u8 *buf, size_t len)
{
	ssize_t actual_len = 0;

	if ((NULL == sbuff) || (NULL == buf))
		return -EINVAL;

	if (MPQ_STREAMBUFFER_BUFFER_MODE_RING == sbuff->mode) {
		/*
		 * Secure buffers are not permitted to be mapped into kernel
		 * memory, and so buffer base address may be NULL
		 */
		if (NULL == sbuff->raw_data.data)
			return -EPERM;

		actual_len = dvb_ringbuffer_avail(&sbuff->raw_data);
		if (actual_len < len)
			len = actual_len;
		if (len)
			dvb_ringbuffer_read(&sbuff->raw_data, buf, len);

		wake_up_all(&sbuff->raw_data.queue);
	} else {
		/* Linear buffer group */
		struct mpq_streambuffer_buffer_desc *desc;

		desc = (struct mpq_streambuffer_buffer_desc *)
				&sbuff->raw_data.data[sbuff->raw_data.pread];

		/*
		 * Secure buffers are not permitted to be mapped into kernel
		 * memory, and so buffer base address may be NULL
		 */
		if (NULL == desc->base)
			return -EPERM;

		actual_len = (desc->write_ptr - desc->read_ptr);
		if (actual_len < len)
			len = actual_len;
		memcpy(buf, desc->base + desc->read_ptr, len);
		desc->read_ptr += len;
	}

	return len;
}
EXPORT_SYMBOL(mpq_streambuffer_data_read);


ssize_t mpq_streambuffer_data_read_user(
		struct mpq_streambuffer *sbuff,
		u8 __user *buf, size_t len)
{
	ssize_t actual_len = 0;

	if ((NULL == sbuff) || (NULL == buf))
		return -EINVAL;

	if (MPQ_STREAMBUFFER_BUFFER_MODE_RING == sbuff->mode) {
		/*
		 * Secure buffers are not permitted to be mapped into kernel
		 * memory, and so buffer base address may be NULL
		 */
		if (NULL == sbuff->raw_data.data)
			return -EPERM;

		actual_len = dvb_ringbuffer_avail(&sbuff->raw_data);
		if (actual_len < len)
			len = actual_len;
		if (len)
			dvb_ringbuffer_read_user(&sbuff->raw_data, buf, len);
		wake_up_all(&sbuff->raw_data.queue);
	} else {
		/* Linear buffer group */
		struct mpq_streambuffer_buffer_desc *desc;

		desc = (struct mpq_streambuffer_buffer_desc *)
				&sbuff->raw_data.data[sbuff->raw_data.pread];

		/*
		 * Secure buffers are not permitted to be mapped into kernel
		 * memory, and so buffer base address may be NULL
		 */
		if (NULL == desc->base)
			return -EPERM;

		actual_len = (desc->write_ptr - desc->read_ptr);
		if (actual_len < len)
			len = actual_len;
		if (copy_to_user(buf, desc->base + desc->read_ptr, len))
			return -EFAULT;
		desc->read_ptr += len;
	}

	return len;
}
EXPORT_SYMBOL(mpq_streambuffer_data_read_user);


int mpq_streambuffer_data_read_dispose(
			struct mpq_streambuffer *sbuff,
			size_t len)
{
	if (NULL == sbuff)
		return -EINVAL;

	if (MPQ_STREAMBUFFER_BUFFER_MODE_RING == sbuff->mode) {
		if (unlikely(dvb_ringbuffer_avail(&sbuff->raw_data) < len))
			return -EINVAL;

		DVB_RINGBUFFER_SKIP(&sbuff->raw_data, len);
		wake_up_all(&sbuff->raw_data.queue);
	} else {
		struct mpq_streambuffer_buffer_desc *desc;

		desc = (struct mpq_streambuffer_buffer_desc *)
				&sbuff->raw_data.data[sbuff->raw_data.pread];
		if ((desc->read_ptr + len) > desc->size)
			desc->read_ptr = desc->size;
		else
			desc->read_ptr += len;
	}

	return 0;
}
EXPORT_SYMBOL(mpq_streambuffer_data_read_dispose);


int mpq_streambuffer_get_buffer_handle(
	struct mpq_streambuffer *sbuff,
	int read_buffer,
	int *handle)
{
	struct mpq_streambuffer_buffer_desc *desc = NULL;

	if ((NULL == sbuff) || (NULL == handle))
		return -EINVAL;

	if (MPQ_STREAMBUFFER_BUFFER_MODE_RING == sbuff->mode) {
		*handle = sbuff->buffers[0].handle;
	} else {
		if (read_buffer)
			desc = (struct mpq_streambuffer_buffer_desc *)
				&sbuff->raw_data.data[sbuff->raw_data.pread];
		else
			desc = (struct mpq_streambuffer_buffer_desc *)
				&sbuff->raw_data.data[sbuff->raw_data.pwrite];
		*handle = desc->handle;
	}
	return 0;
}
EXPORT_SYMBOL(mpq_streambuffer_get_buffer_handle);


int mpq_streambuffer_register_pkt_dispose(
	struct mpq_streambuffer *sbuff,
	mpq_streambuffer_pkt_dispose_cb cb_func,
	void *user_data)
{
	if ((NULL == sbuff) || (NULL == cb_func))
		return -EINVAL;

	sbuff->cb = cb_func;
	sbuff->cb_user_data = user_data;

	return 0;
}
EXPORT_SYMBOL(mpq_streambuffer_register_pkt_dispose);


ssize_t mpq_streambuffer_data_free(
	struct mpq_streambuffer *sbuff)
{
	struct mpq_streambuffer_buffer_desc *desc;

	if (NULL == sbuff)
		return -EINVAL;

	if (MPQ_STREAMBUFFER_BUFFER_MODE_RING == sbuff->mode)
		return dvb_ringbuffer_free(&sbuff->raw_data);

	if (sbuff->pending_buffers_count == sbuff->buffers_num)
		return 0;

	desc = (struct mpq_streambuffer_buffer_desc *)
		&sbuff->raw_data.data[sbuff->raw_data.pwrite];

	return desc->size - desc->write_ptr;
}
EXPORT_SYMBOL(mpq_streambuffer_data_free);


ssize_t mpq_streambuffer_data_avail(
	struct mpq_streambuffer *sbuff)
{
	struct mpq_streambuffer_buffer_desc *desc;

	if (NULL == sbuff)
		return -EINVAL;

	if (MPQ_STREAMBUFFER_BUFFER_MODE_RING == sbuff->mode)
		return dvb_ringbuffer_avail(&sbuff->raw_data);

	desc = (struct mpq_streambuffer_buffer_desc *)
		&sbuff->raw_data.data[sbuff->raw_data.pread];

	return desc->write_ptr - desc->read_ptr;
}
EXPORT_SYMBOL(mpq_streambuffer_data_avail);

int mpq_streambuffer_get_data_rw_offset(
	struct mpq_streambuffer *sbuff,
	u32 *read_offset,
	u32 *write_offset)
{
	if (NULL == sbuff)
		return -EINVAL;

	if (MPQ_STREAMBUFFER_BUFFER_MODE_RING == sbuff->mode) {
		if (read_offset)
			*read_offset = sbuff->raw_data.pread;
		if (write_offset)
			*write_offset = sbuff->raw_data.pwrite;
	} else {
		struct mpq_streambuffer_buffer_desc *desc;

		if (read_offset) {
			desc = (struct mpq_streambuffer_buffer_desc *)
				&sbuff->raw_data.data[sbuff->raw_data.pread];
			*read_offset = desc->read_ptr;
		}
		if (write_offset) {
			desc = (struct mpq_streambuffer_buffer_desc *)
				&sbuff->raw_data.data[sbuff->raw_data.pwrite];
			*write_offset = desc->write_ptr;
		}
	}

	return 0;
}
EXPORT_SYMBOL(mpq_streambuffer_get_data_rw_offset);
