/* Copyright (c) 2012-2016, The Linux Foundation. All rights reserved.
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
	if ((sbuff == NULL) || (data_buffers == NULL) ||
		(packet_buff == NULL) || (data_buff_num == 0))
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
	} else {
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
	sbuff->cb = NULL;
	dvb_ringbuffer_init(&sbuff->packet_data, packet_buff, packet_buff_size);

	return 0;
}
EXPORT_SYMBOL(mpq_streambuffer_init);

void mpq_streambuffer_terminate(struct mpq_streambuffer *sbuff)
{
	spin_lock(&sbuff->packet_data.lock);
	spin_lock(&sbuff->raw_data.lock);
	sbuff->packet_data.error = -ENODEV;
	sbuff->raw_data.error = -ENODEV;
	spin_unlock(&sbuff->raw_data.lock);
	spin_unlock(&sbuff->packet_data.lock);

	wake_up_all(&sbuff->raw_data.queue);
	wake_up_all(&sbuff->packet_data.queue);
}
EXPORT_SYMBOL(mpq_streambuffer_terminate);

ssize_t mpq_streambuffer_pkt_next(
		struct mpq_streambuffer *sbuff,
		ssize_t idx, size_t *pktlen)
{
	ssize_t packet_idx;

	spin_lock(&sbuff->packet_data.lock);

	/* buffer was released, return no packet available */
	if (sbuff->packet_data.error == -ENODEV) {
		spin_unlock(&sbuff->packet_data.lock);
		return -ENODEV;
	}

	packet_idx = dvb_ringbuffer_pkt_next(&sbuff->packet_data, idx, pktlen);
	spin_unlock(&sbuff->packet_data.lock);

	return packet_idx;
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

	spin_lock(&sbuff->packet_data.lock);

	/* buffer was released, return no packet available */
	if (sbuff->packet_data.error == -ENODEV) {
		spin_unlock(&sbuff->packet_data.lock);
		return -ENODEV;
	}

	/* read-out the packet header first */
	ret = dvb_ringbuffer_pkt_read(
				&sbuff->packet_data, idx, 0,
				(u8 *)packet,
				sizeof(struct mpq_streambuffer_packet_header));

	/* verify length, at least packet header should exist */
	if (ret != sizeof(struct mpq_streambuffer_packet_header)) {
		spin_unlock(&sbuff->packet_data.lock);
		return -EINVAL;
	}

	read_len = ret;

	/* read-out private user-data if there are such */
	if ((packet->user_data_len) && (user_data != NULL)) {
		ret = dvb_ringbuffer_pkt_read(
				&sbuff->packet_data,
				idx,
				sizeof(struct mpq_streambuffer_packet_header),
				user_data,
				packet->user_data_len);

		if (ret < 0) {
			spin_unlock(&sbuff->packet_data.lock);
			return ret;
		}

		read_len += ret;
	}

	spin_unlock(&sbuff->packet_data.lock);

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

	if (sbuff == NULL)
		return -EINVAL;

	spin_lock(&sbuff->packet_data.lock);

	/* check if buffer was released */
	if (sbuff->packet_data.error == -ENODEV) {
		spin_unlock(&sbuff->packet_data.lock);
		return -ENODEV;
	}

	/* read-out the packet header first */
	ret = dvb_ringbuffer_pkt_read(&sbuff->packet_data, idx,
			0,
			(u8 *)&packet,
			sizeof(struct mpq_streambuffer_packet_header));

	spin_unlock(&sbuff->packet_data.lock);

	if (ret != sizeof(struct mpq_streambuffer_packet_header))
		return -EINVAL;

	if ((sbuff->mode == MPQ_STREAMBUFFER_BUFFER_MODE_LINEAR) ||
		(dispose_data)) {
		/* Advance the read pointer in the raw-data buffer first */
		ret = mpq_streambuffer_data_read_dispose(sbuff,
			packet.raw_data_len);
		if (ret != 0)
			return ret;
	}

	spin_lock(&sbuff->packet_data.lock);
	spin_lock(&sbuff->raw_data.lock);

	/* check if buffer was released */
	if ((sbuff->packet_data.error == -ENODEV) ||
		(sbuff->raw_data.error == -ENODEV)) {
		spin_unlock(&sbuff->raw_data.lock);
		spin_unlock(&sbuff->packet_data.lock);
		return -ENODEV;
	}

	/* Move read pointer to the next linear buffer for subsequent reads */
	if ((sbuff->mode == MPQ_STREAMBUFFER_BUFFER_MODE_LINEAR) &&
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

	spin_unlock(&sbuff->raw_data.lock);
	spin_unlock(&sbuff->packet_data.lock);

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

	if ((sbuff == NULL) || (packet == NULL))
		return -EINVAL;

	spin_lock(&sbuff->packet_data.lock);

	/* check if buffer was released */
	if (sbuff->packet_data.error == -ENODEV) {
		spin_unlock(&sbuff->packet_data.lock);
		return -ENODEV;
	}

	/* Make sure we can go to the next linear buffer */
	if (sbuff->mode == MPQ_STREAMBUFFER_BUFFER_MODE_LINEAR &&
		sbuff->pending_buffers_count == sbuff->buffers_num &&
		packet->raw_data_len) {
		spin_unlock(&sbuff->packet_data.lock);
		return -ENOSPC;
	}

	len = sizeof(struct mpq_streambuffer_packet_header) +
		packet->user_data_len;

	/* Make sure enough space available for packet header */
	if (dvb_ringbuffer_free(&sbuff->packet_data) <
		(len + DVB_RINGBUFFER_PKTHDRSIZE)) {
		spin_unlock(&sbuff->packet_data.lock);
		return -ENOSPC;
	}

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
	if (sbuff->mode == MPQ_STREAMBUFFER_BUFFER_MODE_LINEAR &&
		packet->raw_data_len) {
		DVB_RINGBUFFER_PUSH(&sbuff->raw_data,
				sizeof(struct mpq_streambuffer_buffer_desc));
		sbuff->pending_buffers_count++;
	}

	spin_unlock(&sbuff->packet_data.lock);
	wake_up_all(&sbuff->packet_data.queue);

	return idx;
}
EXPORT_SYMBOL(mpq_streambuffer_pkt_write);

ssize_t mpq_streambuffer_data_write(
			struct mpq_streambuffer *sbuff,
			const u8 *buf, size_t len)
{
	int res;

	if ((sbuff == NULL) || (buf == NULL))
		return -EINVAL;

	spin_lock(&sbuff->raw_data.lock);

	/* check if buffer was released */
	if (sbuff->raw_data.error == -ENODEV) {
		spin_unlock(&sbuff->raw_data.lock);
		return -ENODEV;
	}

	if (sbuff->mode == MPQ_STREAMBUFFER_BUFFER_MODE_RING) {
		if (unlikely(dvb_ringbuffer_free(&sbuff->raw_data) < len)) {
			spin_unlock(&sbuff->raw_data.lock);
			return -ENOSPC;
		}
		/*
		 * Secure buffers are not permitted to be mapped into kernel
		 * memory, and so buffer base address may be NULL
		 */
		if (sbuff->raw_data.data == NULL) {
			spin_unlock(&sbuff->raw_data.lock);
			return -EPERM;
		}
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
		if (desc->base == NULL) {
			spin_unlock(&sbuff->raw_data.lock);
			return -EPERM;
		}

		if ((sbuff->pending_buffers_count == sbuff->buffers_num) ||
			((desc->size - desc->write_ptr) < len)) {
			MPQ_DVB_DBG_PRINT(
				"%s: No space available! %d pending buffers out of %d total buffers. write_ptr=%d, size=%d\n",
				__func__,
				sbuff->pending_buffers_count,
				sbuff->buffers_num,
				desc->write_ptr,
				desc->size);
			spin_unlock(&sbuff->raw_data.lock);
			return -ENOSPC;
		}
		memcpy(desc->base + desc->write_ptr, buf, len);
		desc->write_ptr += len;
		res = len;
	}

	spin_unlock(&sbuff->raw_data.lock);
	return res;
}
EXPORT_SYMBOL(mpq_streambuffer_data_write);


int mpq_streambuffer_data_write_deposit(
				struct mpq_streambuffer *sbuff,
				size_t len)
{
	if (sbuff == NULL)
		return -EINVAL;

	spin_lock(&sbuff->raw_data.lock);

	/* check if buffer was released */
	if (sbuff->raw_data.error == -ENODEV) {
		spin_unlock(&sbuff->raw_data.lock);
		return -ENODEV;
	}

	if (sbuff->mode == MPQ_STREAMBUFFER_BUFFER_MODE_RING) {
		if (unlikely(dvb_ringbuffer_free(&sbuff->raw_data) < len)) {
			spin_unlock(&sbuff->raw_data.lock);
			return -ENOSPC;
		}

		DVB_RINGBUFFER_PUSH(&sbuff->raw_data, len);
		wake_up_all(&sbuff->raw_data.queue);
	} else {
		/* Linear buffer group */
		struct mpq_streambuffer_buffer_desc *desc =
			(struct mpq_streambuffer_buffer_desc *)
			&sbuff->raw_data.data[sbuff->raw_data.pwrite];

		if ((sbuff->pending_buffers_count == sbuff->buffers_num) ||
			 ((desc->size - desc->write_ptr) < len)) {
			MPQ_DVB_ERR_PRINT(
				"%s: No space available!\n",
				__func__);
			spin_unlock(&sbuff->raw_data.lock);
			return -ENOSPC;
		}
		desc->write_ptr += len;
	}

	spin_unlock(&sbuff->raw_data.lock);
	return 0;
}
EXPORT_SYMBOL(mpq_streambuffer_data_write_deposit);


ssize_t mpq_streambuffer_data_read(
				struct mpq_streambuffer *sbuff,
				u8 *buf, size_t len)
{
	ssize_t actual_len = 0;
	u32 offset;

	if ((sbuff == NULL) || (buf == NULL))
		return -EINVAL;

	spin_lock(&sbuff->raw_data.lock);

	/* check if buffer was released */
	if (sbuff->raw_data.error == -ENODEV) {
		spin_unlock(&sbuff->raw_data.lock);
		return -ENODEV;
	}

	if (sbuff->mode == MPQ_STREAMBUFFER_BUFFER_MODE_RING) {
		/*
		 * Secure buffers are not permitted to be mapped into kernel
		 * memory, and so buffer base address may be NULL
		 */
		if (sbuff->raw_data.data == NULL) {
			spin_unlock(&sbuff->raw_data.lock);
			return -EPERM;
		}

		offset = sbuff->raw_data.pread;
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
		if (desc->base == NULL) {
			spin_unlock(&sbuff->raw_data.lock);
			return -EPERM;
		}

		actual_len = (desc->write_ptr - desc->read_ptr);
		if (actual_len < len)
			len = actual_len;
		memcpy(buf, desc->base + desc->read_ptr, len);
		offset = desc->read_ptr;
		desc->read_ptr += len;
	}

	spin_unlock(&sbuff->raw_data.lock);

	if (sbuff->cb)
		sbuff->cb(sbuff, offset, len, sbuff->cb_user_data);

	return len;
}
EXPORT_SYMBOL(mpq_streambuffer_data_read);


ssize_t mpq_streambuffer_data_read_user(
		struct mpq_streambuffer *sbuff,
		u8 __user *buf, size_t len)
{
	ssize_t actual_len = 0;
	u32 offset;

	if ((sbuff == NULL) || (buf == NULL))
		return -EINVAL;

	/* check if buffer was released */
	if (sbuff->raw_data.error == -ENODEV)
		return -ENODEV;

	if (sbuff->mode == MPQ_STREAMBUFFER_BUFFER_MODE_RING) {
		/*
		 * Secure buffers are not permitted to be mapped into kernel
		 * memory, and so buffer base address may be NULL
		 */
		if (sbuff->raw_data.data == NULL)
			return -EPERM;

		offset = sbuff->raw_data.pread;
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
		if (desc->base == NULL)
			return -EPERM;

		actual_len = (desc->write_ptr - desc->read_ptr);
		if (actual_len < len)
			len = actual_len;
		if (copy_to_user(buf, desc->base + desc->read_ptr, len))
			return -EFAULT;

		offset = desc->read_ptr;
		desc->read_ptr += len;
	}

	if (sbuff->cb)
		sbuff->cb(sbuff, offset, len, sbuff->cb_user_data);

	return len;
}
EXPORT_SYMBOL(mpq_streambuffer_data_read_user);

int mpq_streambuffer_data_read_dispose(
			struct mpq_streambuffer *sbuff,
			size_t len)
{
	u32 offset;

	if (sbuff == NULL)
		return -EINVAL;

	spin_lock(&sbuff->raw_data.lock);

	/* check if buffer was released */
	if (sbuff->raw_data.error == -ENODEV) {
		spin_unlock(&sbuff->raw_data.lock);
		return -ENODEV;
	}

	if (sbuff->mode == MPQ_STREAMBUFFER_BUFFER_MODE_RING) {
		if (unlikely(dvb_ringbuffer_avail(&sbuff->raw_data) < len)) {
			spin_unlock(&sbuff->raw_data.lock);
			return -EINVAL;
		}

		offset = sbuff->raw_data.pread;
		DVB_RINGBUFFER_SKIP(&sbuff->raw_data, len);
		wake_up_all(&sbuff->raw_data.queue);
	} else {
		struct mpq_streambuffer_buffer_desc *desc;

		desc = (struct mpq_streambuffer_buffer_desc *)
				&sbuff->raw_data.data[sbuff->raw_data.pread];
		offset = desc->read_ptr;

		if ((desc->read_ptr + len) > desc->size)
			desc->read_ptr = desc->size;
		else
			desc->read_ptr += len;
	}

	spin_unlock(&sbuff->raw_data.lock);

	if (sbuff->cb)
		sbuff->cb(sbuff, offset, len, sbuff->cb_user_data);

	return 0;
}
EXPORT_SYMBOL(mpq_streambuffer_data_read_dispose);


int mpq_streambuffer_get_buffer_handle(
	struct mpq_streambuffer *sbuff,
	int read_buffer,
	int *handle)
{
	struct mpq_streambuffer_buffer_desc *desc = NULL;

	if ((sbuff == NULL) || (handle == NULL))
		return -EINVAL;

	spin_lock(&sbuff->raw_data.lock);

	/* check if buffer was released */
	if (sbuff->raw_data.error == -ENODEV) {
		spin_unlock(&sbuff->raw_data.lock);
		return -ENODEV;
	}

	if (sbuff->mode == MPQ_STREAMBUFFER_BUFFER_MODE_RING) {
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

	spin_unlock(&sbuff->raw_data.lock);

	return 0;
}
EXPORT_SYMBOL(mpq_streambuffer_get_buffer_handle);


int mpq_streambuffer_register_data_dispose(
	struct mpq_streambuffer *sbuff,
	mpq_streambuffer_dispose_cb cb_func,
	void *user_data)
{
	if ((sbuff == NULL) || (cb_func == NULL))
		return -EINVAL;

	sbuff->cb = cb_func;
	sbuff->cb_user_data = user_data;

	return 0;
}
EXPORT_SYMBOL(mpq_streambuffer_register_data_dispose);


ssize_t mpq_streambuffer_data_free(
	struct mpq_streambuffer *sbuff)
{
	struct mpq_streambuffer_buffer_desc *desc;

	if (sbuff == NULL)
		return -EINVAL;

	spin_lock(&sbuff->raw_data.lock);

	/* check if buffer was released */
	if (sbuff->raw_data.error == -ENODEV) {
		spin_unlock(&sbuff->raw_data.lock);
		return -ENODEV;
	}

	if (sbuff->mode == MPQ_STREAMBUFFER_BUFFER_MODE_RING) {
		spin_unlock(&sbuff->raw_data.lock);
		return dvb_ringbuffer_free(&sbuff->raw_data);
	}

	if (sbuff->pending_buffers_count == sbuff->buffers_num) {
		spin_unlock(&sbuff->raw_data.lock);
		return 0;
	}

	desc = (struct mpq_streambuffer_buffer_desc *)
		&sbuff->raw_data.data[sbuff->raw_data.pwrite];

	spin_unlock(&sbuff->raw_data.lock);

	return desc->size - desc->write_ptr;
}
EXPORT_SYMBOL(mpq_streambuffer_data_free);


ssize_t mpq_streambuffer_data_avail(
	struct mpq_streambuffer *sbuff)
{
	struct mpq_streambuffer_buffer_desc *desc;

	if (sbuff == NULL)
		return -EINVAL;

	spin_lock(&sbuff->raw_data.lock);

	/* check if buffer was released */
	if (sbuff->raw_data.error == -ENODEV) {
		spin_unlock(&sbuff->raw_data.lock);
		return -ENODEV;
	}

	if (sbuff->mode == MPQ_STREAMBUFFER_BUFFER_MODE_RING) {
		ssize_t avail = dvb_ringbuffer_avail(&sbuff->raw_data);

		spin_unlock(&sbuff->raw_data.lock);
		return avail;
	}

	desc = (struct mpq_streambuffer_buffer_desc *)
		&sbuff->raw_data.data[sbuff->raw_data.pread];

	spin_unlock(&sbuff->raw_data.lock);

	return desc->write_ptr - desc->read_ptr;
}
EXPORT_SYMBOL(mpq_streambuffer_data_avail);

int mpq_streambuffer_get_data_rw_offset(
	struct mpq_streambuffer *sbuff,
	u32 *read_offset,
	u32 *write_offset)
{
	if (sbuff == NULL)
		return -EINVAL;

	spin_lock(&sbuff->raw_data.lock);

	/* check if buffer was released */
	if (sbuff->raw_data.error == -ENODEV) {
		spin_unlock(&sbuff->raw_data.lock);
		return -ENODEV;
	}

	if (sbuff->mode == MPQ_STREAMBUFFER_BUFFER_MODE_RING) {
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

	spin_unlock(&sbuff->raw_data.lock);

	return 0;
}
EXPORT_SYMBOL(mpq_streambuffer_get_data_rw_offset);

ssize_t mpq_streambuffer_metadata_free(struct mpq_streambuffer *sbuff)
{
	ssize_t free;

	if (sbuff == NULL)
		return -EINVAL;

	spin_lock(&sbuff->packet_data.lock);

	/* check if buffer was released */
	if (sbuff->packet_data.error == -ENODEV) {
		spin_unlock(&sbuff->packet_data.lock);
		return -ENODEV;
	}

	free = dvb_ringbuffer_free(&sbuff->packet_data);

	spin_unlock(&sbuff->packet_data.lock);

	return free;
}
EXPORT_SYMBOL(mpq_streambuffer_metadata_free);

int mpq_streambuffer_flush(struct mpq_streambuffer *sbuff)
{
	struct mpq_streambuffer_buffer_desc *desc;
	size_t len;
	int idx;
	int ret = 0;

	if (sbuff == NULL)
		return -EINVAL;

	spin_lock(&sbuff->packet_data.lock);
	spin_lock(&sbuff->raw_data.lock);

	/* Check if buffer was released */
	if (sbuff->packet_data.error == -ENODEV ||
		sbuff->raw_data.error == -ENODEV) {
		ret = -ENODEV;
		goto end;
	}

	if (sbuff->mode == MPQ_STREAMBUFFER_BUFFER_MODE_LINEAR)
		while (sbuff->pending_buffers_count) {
			desc = (struct mpq_streambuffer_buffer_desc *)
				&sbuff->raw_data.data[sbuff->raw_data.pread];
			desc->write_ptr = 0;
			desc->read_ptr = 0;
			DVB_RINGBUFFER_SKIP(&sbuff->raw_data,
				sizeof(struct mpq_streambuffer_buffer_desc));
			sbuff->pending_buffers_count--;
		}
	else
		dvb_ringbuffer_flush(&sbuff->raw_data);

	/*
	 * Dispose all packets (simply flushing is not enough since we want
	 * the packets' status to move to disposed).
	 */
	do {
		idx = dvb_ringbuffer_pkt_next(&sbuff->packet_data, -1, &len);
		if (idx >= 0)
			dvb_ringbuffer_pkt_dispose(&sbuff->packet_data, idx);
	} while (idx >= 0);

end:
	spin_unlock(&sbuff->raw_data.lock);
	spin_unlock(&sbuff->packet_data.lock);
	return ret;
}
EXPORT_SYMBOL(mpq_streambuffer_flush);
