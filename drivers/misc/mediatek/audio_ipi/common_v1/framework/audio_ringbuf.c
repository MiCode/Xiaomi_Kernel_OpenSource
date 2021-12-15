// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2018 MediaTek Inc.

#include <audio_ringbuf.h>

#include <linux/string.h>
#include <linux/vmalloc.h>

#include <audio_assert.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "audio_ringbuf"

#define MAX_SIZE_OF_ONE_FRAME (16) /* 32-bits * 4ch */

#define DUMP_RINGBUF(LOG_F, description, rb, count) \
	do { \
		if (rb && description) { \
			LOG_F("%s(), %s, base %p, read %p, write %p, size %u" \
			      ", data %u, free %u, count %u\n", \
			      __func__, \
			      description, \
			      (rb)->base, \
			      (rb)->read, \
			      (rb)->write, \
			      (rb)->size, \
			      audio_ringbuf_count(rb), \
			      audio_ringbuf_free_space(rb), \
			      count); \
		} else { \
			pr_notice("%s(), %uL, %p %p\n", \
				  __func__, __LINE__, rb, description); \
		} \
	} while (0)

uint32_t audio_ringbuf_count(const struct audio_ringbuf_t *rb)
{
	uint32_t count = 0;
	uint32_t w2r = 0;

	if (!rb) {
		AUD_WARNING("null");
		return 0;
	}
	if (!rb->base || !rb->size)
		return 0;

	if (rb->write >= rb->read)
		count = rb->write - rb->read;
	else {
		w2r = rb->read - rb->write;
		if (rb->size > w2r)
			count = rb->size - w2r;
		else
			count = 0;
	}

	return count;
}

uint32_t audio_ringbuf_free_space(const struct audio_ringbuf_t *rb)
{
	uint32_t count = 0;
	uint32_t free_spece = 0;

	if (!rb) {
		AUD_WARNING("null");
		return 0;
	}
	if (!rb->base || !rb->size)
		return 0;

	count = audio_ringbuf_count(rb);
	free_spece = (rb->size > count) ? (rb->size - count) : 0;

	/* avoid to copy data s.t. read == write */
	if (free_spece > MAX_SIZE_OF_ONE_FRAME)
		free_spece -= MAX_SIZE_OF_ONE_FRAME;
	else
		free_spece = 0;

	return free_spece;
}

void audio_ringbuf_copy_to_linear(
	char *linear,
	struct audio_ringbuf_t *rb,
	uint32_t count)
{
	char *end = NULL;
	uint32_t r2e = 0;

	if (!count)
		return;
	if (!linear || !rb) {
		AUD_WARNING("null");
		return;
	}
	if (!rb->base || !rb->size) {
		DUMP_RINGBUF(pr_notice, "no init", rb, count);
		AUD_WARNING("no init");
		return;
	}
	if (count > audio_ringbuf_count(rb)) {
		DUMP_RINGBUF(pr_notice, "underflow", rb, count);
		AUD_WARNING("underflow");
		return;
	}


	if (rb->read <= rb->write) {
		memcpy(linear, rb->read, count);
		rb->read += count;
	} else {
		end = rb->base + rb->size;
		r2e = end - rb->read;
		if (count <= r2e) {
			memcpy(linear, rb->read, count);
			rb->read += count;
			if (rb->read == end)
				rb->read = rb->base;
		} else {
			memcpy(linear, rb->read, r2e);
			memcpy(linear + r2e, rb->base, count - r2e);
			rb->read = rb->base + count - r2e;
		}
	}
}

void audio_ringbuf_copy_from_linear_impl(
	struct audio_ringbuf_t *rb,
	const char *linear,
	uint32_t count)
{
	char *end = NULL;
	uint32_t w2e = 0;

	if (!count)
		return;
	if (!rb || !linear) {
		AUD_WARNING("null");
		return;
	}
	if (!rb->base || !rb->size) {
		DUMP_RINGBUF(pr_notice, "no init", rb, count);
		AUD_WARNING("no init");
		return;
	}
	if (count > audio_ringbuf_free_space(rb)) {
		DUMP_RINGBUF(pr_notice, "overflow", rb, count);
		AUD_WARNING("overflow");
		return;
	}

	end = rb->base + rb->size;

	if (rb->read <= rb->write) {
		w2e = end - rb->write;
		if (count <= w2e) {
			memcpy(rb->write, linear, count);
			rb->write += count;
			if (rb->write == end)
				rb->write = rb->base;
		} else {
			memcpy(rb->write, linear, w2e);
			memcpy(rb->base, linear + w2e, count - w2e);
			rb->write = rb->base + count - w2e;
		}
	} else {
		memcpy(rb->write, linear, count);
		rb->write += count;
	}
}

void audio_ringbuf_copy_from_linear(
	struct audio_ringbuf_t *rb,
	const char *linear,
	uint32_t count)
{
	if (!count)
		return;
	if (!rb || !linear) {
		AUD_WARNING("null");
		return;
	}

	dynamic_change_ring_buf_size(rb, count);
	audio_ringbuf_copy_from_linear_impl(rb, linear, count);
}

void audio_ringbuf_copy_from_ringbuf_impl(
	struct audio_ringbuf_t *rb_des,
	struct audio_ringbuf_t *rb_src,
	uint32_t count)
{
	char *end = NULL;
	uint32_t r2e = 0;

	if (!count)
		return;
	if (!rb_des || !rb_src) {
		AUD_WARNING("null");
		return;
	}
	if (!rb_src->base || !rb_src->size) {
		DUMP_RINGBUF(pr_notice, "no init", rb_src, count);
		AUD_WARNING("no init");
		return;
	}
	if (count > audio_ringbuf_count(rb_src)) {
		DUMP_RINGBUF(pr_notice, "underflow", rb_src, count);
		AUD_WARNING("underflow");
		return;
	}
	if (count > audio_ringbuf_free_space(rb_des)) {
		DUMP_RINGBUF(pr_notice, "overflow", rb_des, count);
		AUD_WARNING("overflow");
		return;
	}

	if (rb_src->read <= rb_src->write) {
		audio_ringbuf_copy_from_linear_impl(
			rb_des,
			rb_src->read,
			count);
		rb_src->read += count;
	} else {
		end = rb_src->base + rb_src->size;
		r2e = end - rb_src->read;
		if (r2e >= count) {
			audio_ringbuf_copy_from_linear_impl(
				rb_des,
				rb_src->read,
				count);
			rb_src->read += count;
			if (rb_src->read == end)
				rb_src->read = rb_src->base;
		} else {
			audio_ringbuf_copy_from_linear_impl(
				rb_des,
				rb_src->read,
				r2e);
			audio_ringbuf_copy_from_linear_impl(
				rb_des,
				rb_src->base,
				count - r2e);
			rb_src->read = rb_src->base + count - r2e;
		}
	}
}

void audio_ringbuf_copy_from_ringbuf(
	struct audio_ringbuf_t *rb_des,
	struct audio_ringbuf_t *rb_src,
	uint32_t count)
{
	if (!count)
		return;
	if (!rb_des || !rb_src) {
		AUD_WARNING("null");
		return;
	}
	if (!rb_src->base || !rb_src->size) {
		DUMP_RINGBUF(pr_notice, "no init", rb_src, count);
		AUD_WARNING("no init");
		return;
	}
	if (count > audio_ringbuf_count(rb_src)) {
		DUMP_RINGBUF(pr_notice, "underflow", rb_src, count);
		AUD_WARNING("underflow");
		return;
	}

	dynamic_change_ring_buf_size(rb_des, count);
	audio_ringbuf_copy_from_ringbuf_impl(rb_des, rb_src, count);
}

void audio_ringbuf_copy_from_ringbuf_all(
	struct audio_ringbuf_t *rb_des,
	struct audio_ringbuf_t *rb_src)
{
	if (!rb_des || !rb_src) {
		AUD_WARNING("null");
		return;
	}
	audio_ringbuf_copy_from_ringbuf(
		rb_des,
		rb_src,
		audio_ringbuf_count(rb_src));
}

void audio_ringbuf_write_value(
	struct audio_ringbuf_t *rb,
	const uint8_t value,
	const uint32_t count)
{
	char *end = NULL;
	uint32_t w2e = 0;

	if (!count)
		return;
	if (!rb) {
		AUD_WARNING("null");
		return;
	}

	dynamic_change_ring_buf_size(rb, count);
	if (!rb->base || !rb->size) {
		DUMP_RINGBUF(pr_notice, "no init", rb, count);
		AUD_WARNING("no init");
		return;
	}
	if (count > audio_ringbuf_free_space(rb)) {
		DUMP_RINGBUF(pr_notice, "overflow", rb, count);
		AUD_WARNING("overflow");
		return;
	}

	end = rb->base + rb->size;

	if (rb->read <= rb->write) {
		w2e = end - rb->write;
		if (count <= w2e) {
			memset(rb->write, value, count);
			rb->write += count;
			if (rb->write == end)
				rb->write = rb->base;
		} else {
			memset(rb->write, value, w2e);
			memset(rb->base, value, count - w2e);
			rb->write = rb->base + count - w2e;
		}
	} else {
		memset(rb->write, value, count);
		rb->write += count;
	}
}

void audio_ringbuf_write_zero(struct audio_ringbuf_t *rb, uint32_t count)
{
	if (!count)
		return;
	if (!rb) {
		AUD_WARNING("null");
		return;
	}

	audio_ringbuf_write_value(rb, 0, count);
}

void audio_ringbuf_drop_data(struct audio_ringbuf_t *rb, const uint32_t count)
{
	char *end = NULL;
	uint32_t r2e = 0;

	if (!count)
		return;
	if (!rb) {
		AUD_WARNING("null");
		return;
	}
	if (count > audio_ringbuf_count(rb)) {
		DUMP_RINGBUF(pr_notice, "underflow", rb, count);
		AUD_WARNING("underflow");
		return;
	}

	if (rb->read <= rb->write)
		rb->read += count;
	else {
		end = rb->base + rb->size;
		r2e = end - rb->read;
		if (count <= r2e) {
			rb->read += count;
			if (rb->read == end)
				rb->read = rb->base;
		} else
			rb->read = rb->base + count - r2e;
	}
}

void audio_ringbuf_drop_all(struct audio_ringbuf_t *rb)
{
	if (!rb) {
		AUD_WARNING("null");
		return;
	}

	rb->read = 0;
	rb->write = 0;
}

void audio_ringbuf_compensate_value_impl(
	struct audio_ringbuf_t *rb,
	const uint8_t value,
	const uint32_t count)
{
	char *end = NULL;

	uint32_t b2r = 0;
	uint32_t left_data = 0;

	if (!count)
		return;
	if (!rb) {
		AUD_WARNING("null");
		return;
	}

	if (!rb->base || !rb->size) {
		DUMP_RINGBUF(pr_notice, "no init", rb, count);
		AUD_WARNING("no init");
		return;
	}
	if (count > audio_ringbuf_free_space(rb)) {
		DUMP_RINGBUF(pr_notice, "overflow", rb, count);
		AUD_WARNING("overflow");
		return;
	}

	end = rb->base + rb->size;

	if (rb->read <= rb->write) {
		b2r = rb->read - rb->base;
		if (b2r >= count) {
			rb->read -= count;
			memset(rb->read, value, count);
		} else {
			if (b2r > 0) { /* in case read == base */
				memset(rb->base, value, b2r);
				left_data = count - b2r;
				rb->read = end - left_data;
				memset(rb->read, value, left_data);
			} else {
				rb->read = end - count;
				memset(rb->read, value, count);
			}
		}
	} else {
		rb->read -= count;
		memset(rb->read, value, count);
	}
}

void audio_ringbuf_compensate_value(
	struct audio_ringbuf_t *rb,
	const uint8_t value,
	const uint32_t count)
{
	if (!count)
		return;
	if (!rb) {
		AUD_WARNING("null");
		return;
	}

	dynamic_change_ring_buf_size(rb, count);
	audio_ringbuf_compensate_value_impl(rb, value, count);
}


void audio_ringbuf_rollback(struct audio_ringbuf_t *rb, const uint32_t count)
{
	char *end = NULL;

	uint32_t b2r = 0;
	uint32_t left_data = 0;

	if (!count)
		return;
	if (!rb) {
		AUD_WARNING("null");
		return;
	}
	if (count > audio_ringbuf_free_space(rb)) {
		AUD_WARNING("overflow");
		return;
	}

	b2r = rb->read - rb->base;
	end = rb->base + rb->size;

	if (rb->read <= rb->write) {
		b2r = rb->read - rb->base;
		if (b2r >= count)
			rb->read -= count;
		else {
			if (b2r > 0) { /* in case read == base */
				left_data = count - b2r;
				rb->read = end - left_data;
			} else
				rb->read = end - count;
		}
	} else
		rb->read -= count;
}

void dynamic_change_ring_buf_size(
	struct audio_ringbuf_t *rb,
	uint32_t write_size)
{
	uint32_t data_count = 0;
	uint32_t free_space = 0;

	uint32_t change_size = 0;
	struct audio_ringbuf_t new_ringbuf;

	if (!rb) {
		AUD_WARNING("null");
		return;
	}
	if (!write_size)
		return;

	memset(&new_ringbuf, 0, sizeof(struct audio_ringbuf_t));

	if (!rb->base || !rb->size) { /* init */
		change_size = (2 * write_size);
		change_size += MAX_SIZE_OF_ONE_FRAME;

		rb->base  = vmalloc(change_size);
		rb->read  = rb->base;
		rb->write = rb->base;
		rb->size  = change_size;
	} else { /* update size */
		data_count = audio_ringbuf_count(rb);
		free_space = audio_ringbuf_free_space(rb);

		if ((free_space  <       write_size) ||
		    (free_space  > (8 * (data_count + write_size)))) {
			change_size  = (2 * (data_count + write_size));
			change_size += MAX_SIZE_OF_ONE_FRAME;

			pr_info("%s(), %p: %u -> %u, data_count %u, write_size %u, free_space %u\n",
				__func__,
				rb->base,
				rb->size,
				change_size,
				data_count,
				write_size,
				free_space);

			new_ringbuf.base  = vmalloc(change_size);
			new_ringbuf.read  = new_ringbuf.base;
			new_ringbuf.write = new_ringbuf.base;
			new_ringbuf.size  = change_size;

			/* copy old data */
			audio_ringbuf_copy_from_ringbuf_impl(
				&new_ringbuf,
				rb,
				data_count);

			/* delete old ringbuf */
			vfree(rb->base);

			/* update info */
			rb->base  = new_ringbuf.base;
			rb->read  = new_ringbuf.read;
			rb->write = new_ringbuf.write;
			rb->size  = new_ringbuf.size;

			memset(&new_ringbuf, 0, sizeof(struct audio_ringbuf_t));
		}
	}
}

