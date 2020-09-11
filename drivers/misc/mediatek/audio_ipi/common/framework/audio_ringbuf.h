/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef AUDIO_RING_BUF_H
#define AUDIO_RING_BUF_H

#include <linux/types.h>

#ifdef __cplusplus
extern "C" {
#endif


struct audio_ringbuf_t {
	char    *base;
	char    *read;
	char    *write;
	uint32_t size;
};


uint32_t audio_ringbuf_count(const struct audio_ringbuf_t *rb);

uint32_t audio_ringbuf_free_space(const struct audio_ringbuf_t *rb);


void audio_ringbuf_copy_to_linear(
	char *linear,
	struct audio_ringbuf_t *rb,
	uint32_t count);

void audio_ringbuf_copy_from_linear(
	struct audio_ringbuf_t *rb,
	const char *linear,
	uint32_t count);

void audio_ringbuf_copy_from_linear_impl(
	struct audio_ringbuf_t *rb,
	const char *linear,
	uint32_t count);


void audio_ringbuf_copy_from_ringbuf(
	struct audio_ringbuf_t *rb_des,
	struct audio_ringbuf_t *rb_src,
	uint32_t count);

void audio_ringbuf_copy_from_ringbuf_impl(
	struct audio_ringbuf_t *rb_des,
	struct audio_ringbuf_t *rb_src,
	uint32_t count);

void audio_ringbuf_copy_from_ringbuf_all(
	struct audio_ringbuf_t *rb_des,
	struct audio_ringbuf_t *rb_src);


void audio_ringbuf_write_value(
	struct audio_ringbuf_t *rb,
	const uint8_t value,
	const uint32_t count);

void audio_ringbuf_write_zero(struct audio_ringbuf_t *rb, uint32_t count);


void audio_ringbuf_drop_data(struct audio_ringbuf_t *rb, const uint32_t count);

void audio_ringbuf_drop_all(struct audio_ringbuf_t *rb);


void audio_ringbuf_compensate_value(
	struct audio_ringbuf_t *rb,
	const uint8_t value,
	const uint32_t count);

void audio_ringbuf_compensate_value_impl(
	struct audio_ringbuf_t *rb,
	const uint8_t value,
	const uint32_t count);


void audio_ringbuf_rollback(struct audio_ringbuf_t *rb, const uint32_t count);


void dynamic_change_ring_buf_size(
	struct audio_ringbuf_t *rb,
	uint32_t write_size);




#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* end of AUDIO_RING_BUF_H */

