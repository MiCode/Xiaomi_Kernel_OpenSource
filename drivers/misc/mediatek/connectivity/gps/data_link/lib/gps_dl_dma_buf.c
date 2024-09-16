/*
 * Copyright (C) 2019 MediaTek Inc.
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
#include "gps_dl_config.h"
#include "gps_dl_context.h"

#include "gps_dl_dma_buf.h"
#if GPS_DL_ON_LINUX
#include "asm/barrier.h"
#endif

#define GDL_COUNT_FREE(r, w, l)\
	((w >= r) ? (l + r - w) : (r - w))

#define GDL_COUNT_DATA(r, w, l)\
	((w >= r) ? (w - r) : (l + w - r))

void gps_dma_buf_reset(struct gps_dl_dma_buf *p_dma)
{
	gps_each_link_spin_lock_take(p_dma->dev_index, GPS_DL_SPINLOCK_FOR_DMA_BUF);
	p_dma->read_index = 0;
	p_dma->reader_working = 0;
	p_dma->write_index = 0;
	p_dma->writer_working = 0;
	p_dma->dma_working_entry.is_valid = false;
	p_dma->entry_r = 0;
	p_dma->entry_w = 0;
	memset(&p_dma->data_entries[0], 0, sizeof(p_dma->data_entries));
	gps_each_link_spin_lock_give(p_dma->dev_index, GPS_DL_SPINLOCK_FOR_DMA_BUF);

	GDL_LOGXD(p_dma->dev_index, "dir = %d", p_dma->dir);
}

void gps_dma_buf_show(struct gps_dl_dma_buf *p_dma, bool is_warning)
{
	unsigned int ri, wi, fl, re, we, fe;
	bool r_working, w_working;

	gps_each_link_spin_lock_take(p_dma->dev_index, GPS_DL_SPINLOCK_FOR_DMA_BUF);
	ri = p_dma->read_index;
	r_working = p_dma->reader_working;
	wi = p_dma->write_index;
	w_working = p_dma->writer_working;
	fl = GDL_COUNT_FREE(p_dma->read_index, p_dma->writer_working, p_dma->len);
	re = p_dma->entry_r;
	we = p_dma->entry_w;
	fe = GDL_COUNT_FREE(p_dma->entry_r, p_dma->entry_w, GPS_DL_DMA_BUF_ENTRY_MAX);
	gps_each_link_spin_lock_give(p_dma->dev_index, GPS_DL_SPINLOCK_FOR_DMA_BUF);

	if (is_warning) {
		GDL_LOGXW_DRW(p_dma->dev_index,
			"dir = %d, l = %d, r = %d(%d), w = %d(%d), fl = %d, re = %d, we = %d, fe = %d",
			p_dma->dir, p_dma->len, ri, r_working, wi, w_working, fl, re, we, fe);
	} else {
		GDL_LOGXD_DRW(p_dma->dev_index,
			"dir = %d, l = %d, r = %d(%d), w = %d(%d), fl = %d, re = %d, we = %d, fe = %d",
			p_dma->dir, p_dma->len, ri, r_working, wi, w_working, fl, re, we, fe);
	}
}

void gps_dma_buf_align_as_byte_mode(struct gps_dl_dma_buf *p_dma)
{
	unsigned int ri, wi;
	unsigned int ri_new, wi_new;

	gps_each_link_spin_lock_take(p_dma->dev_index, GPS_DL_SPINLOCK_FOR_DMA_BUF);
	ri = p_dma->read_index;
	wi = p_dma->write_index;

	if (!gps_dl_is_1byte_mode()) {
		p_dma->read_index = ((p_dma->read_index + 3) / 4) * 4;
		if (p_dma->read_index >= p_dma->len)
			p_dma->read_index -= p_dma->len;
		p_dma->reader_working = 0;

		p_dma->write_index = ((p_dma->write_index + 3) / 4) * 4;
		if (p_dma->write_index >= p_dma->len)
			p_dma->write_index -= p_dma->len;
		p_dma->writer_working = 0;
		p_dma->dma_working_entry.is_valid = false;
	}

	ri_new = p_dma->read_index;
	wi_new = p_dma->write_index;

	/* clear it anyway */
	p_dma->read_index = p_dma->write_index;
	gps_each_link_spin_lock_give(p_dma->dev_index, GPS_DL_SPINLOCK_FOR_DMA_BUF);

	GDL_LOGXD(p_dma->dev_index, "is_1byte = %d, ri: %u -> %u, wi: %u -> %u",
		gps_dl_is_1byte_mode(), ri, ri_new, wi, wi_new);
}

#if 0
enum GDL_RET_STATUS gdl_dma_buf_init(struct gps_dl_dma_buf *p_dma_buf)
{
	return GDL_OKAY;
}

enum GDL_RET_STATUS gdl_dma_buf_deinit(struct gps_dl_dma_buf *p_dma_buf)
{
	return GDL_OKAY;
}
#endif

bool gps_dma_buf_is_empty(struct gps_dl_dma_buf *p_dma)
{
	bool is_empty;

	gps_each_link_spin_lock_take(p_dma->dev_index, GPS_DL_SPINLOCK_FOR_DMA_BUF);
	is_empty = (p_dma->read_index == p_dma->write_index);
	gps_each_link_spin_lock_give(p_dma->dev_index, GPS_DL_SPINLOCK_FOR_DMA_BUF);

	return is_empty;
}

enum GDL_RET_STATUS gdl_dma_buf_put(struct gps_dl_dma_buf *p_dma,
	const unsigned char *p_buf, unsigned int buf_len)
{
	struct gdl_dma_buf_entry entry;
	struct gdl_dma_buf_entry *p_entry = &entry;

	/* unsigned int free_len; */
	/* unsigned int wrap_len; */
	enum GDL_RET_STATUS gdl_ret;

	ASSERT_NOT_NULL(p_dma, GDL_FAIL_ASSERT);
	ASSERT_NOT_NULL(p_buf, GDL_FAIL_ASSERT);

	gdl_ret = gdl_dma_buf_get_free_entry(p_dma, p_entry, false);

	if (GDL_OKAY != gdl_ret)
		return gdl_ret;

#if 0
	free_len = GDL_COUNT_FREE(p_entry->read_index,
		p_entry->write_index, p_entry->buf_length);
	GDL_LOGD("r=%u, w=%u, l=%u, f=%u", p_entry->read_index,
		p_entry->write_index, p_entry->buf_length, free_len);

	if (free_len < buf_len) {
		gdl_dma_buf_set_free_entry(p_dma, NULL);
		return GDL_FAIL_NOSPACE;
	}

	wrap_len = p_entry->buf_length - p_entry->write_index;
	if (wrap_len >= buf_len) {
		memcpy(((unsigned char *)p_entry->vir_addr) + p_entry->write_index,
			p_buf, buf_len);

		p_entry->write_index += buf_len;
		if (p_entry->write_index >= p_entry->buf_length)
			p_entry->write_index = 0;
	} else {
		memcpy(((unsigned char *)p_entry->vir_addr) + p_entry->write_index,
			p_buf, wrap_len);

		memcpy(((unsigned char *)p_entry->vir_addr) + 0,
			p_buf + wrap_len, buf_len - wrap_len);

		p_entry->write_index = buf_len - wrap_len;
	}
#endif
	gdl_ret = gdl_dma_buf_buf_to_entry(p_entry, p_buf, buf_len,
		&p_entry->write_index);

	if (GDL_OKAY != gdl_ret)
		return gdl_ret;

	/* TODO: make a data entry */

	GDL_LOGD("new_w=%u", p_entry->write_index);
	gdl_dma_buf_set_free_entry(p_dma, p_entry);

	return GDL_OKAY;
}

enum GDL_RET_STATUS gdl_dma_buf_get(struct gps_dl_dma_buf *p_dma,
	unsigned char *p_buf, unsigned int buf_len, unsigned int *p_data_len,
	bool *p_is_nodata)
{
	struct gdl_dma_buf_entry entry;
	struct gdl_dma_buf_entry *p_entry = &entry;

	/* unsigned int data_len; */
	/* unsigned int wrap_len; */
	enum GDL_RET_STATUS gdl_ret;

	ASSERT_NOT_NULL(p_entry, GDL_FAIL_ASSERT);
	ASSERT_NOT_NULL(p_buf, GDL_FAIL_ASSERT);
	ASSERT_NOT_ZERO(buf_len, GDL_FAIL_ASSERT);
	ASSERT_NOT_NULL(p_data_len, GDL_FAIL_ASSERT);

	gdl_ret = gdl_dma_buf_get_data_entry(p_dma, p_entry);

	if (GDL_OKAY != gdl_ret)
		return gdl_ret;

#if 0
	data_len = GDL_COUNT_DATA(p_entry->read_index,
		p_entry->write_index, p_entry->buf_length);
	GDL_LOGD("r=%u, w=%u, l=%u, d=%u", p_entry->read_index,
			p_entry->write_index, p_entry->buf_length, data_len);

	/* assert data_len > 0 */

	if (data_len > buf_len) {
		/* TODO: improve it */
		gdl_dma_buf_set_data_entry(p_dma, p_entry);
		return GDL_FAIL_NOSPACE;
	}

	if (p_entry->write_index > p_entry->read_index) {
		memcpy(p_buf, ((unsigned char *)p_entry->vir_addr) + p_entry->read_index,
			data_len);
	} else {
		wrap_len = p_entry->buf_length - p_entry->read_index;

		memcpy(p_buf, ((unsigned char *)p_entry->vir_addr) + p_entry->read_index,
			wrap_len);

		memcpy(p_buf + wrap_len, ((unsigned char *)p_entry->vir_addr) + 0,
			data_len - wrap_len);
	}
#endif
	gdl_ret = gdl_dma_buf_entry_to_buf(p_entry, p_buf, buf_len, p_data_len);

	if (GDL_OKAY != gdl_ret)
		return gdl_ret;

	/* Todo: Case1: buf < data in entry */
	/* Note: we can limit the rx transfer max to 512, then case1 should not be happened */

	/* Todo: Case2: buf > data in entry, need to combine multiple entry until no data entry? */

	if (p_is_nodata)
		*p_is_nodata = p_entry->is_nodata;

	/* *p_data_len = data_len; */
	p_entry->read_index = p_entry->write_index;
	gdl_dma_buf_set_data_entry(p_dma, p_entry);

	return GDL_OKAY;
}

static enum GDL_RET_STATUS gdl_dma_buf_get_data_entry_inner(struct gps_dl_dma_buf *p_dma,
	struct gdl_dma_buf_entry *p_entry)
{
	struct gdl_dma_buf_entry *p_data_entry;
	unsigned int data_len;

	if (p_dma->reader_working)
		return GDL_FAIL_BUSY;

	p_dma->reader_working = true;

	if (p_dma->read_index == p_dma->write_index) {
		p_dma->reader_working = false;
		return GDL_FAIL_NODATA;
	}

	if (p_dma->entry_r == p_dma->entry_w) {
		/* impossible: has data but no data entry */
		p_dma->reader_working = false;
		return GDL_FAIL_NOENTRY;
	}

	p_data_entry = &p_dma->data_entries[p_dma->entry_r];
	p_entry->write_index = p_data_entry->write_index;
	p_entry->is_nodata = p_data_entry->is_nodata;
	if ((p_dma->transfer_max > 0) && (p_dma->dir == GDL_DMA_A2D)) {
		data_len = GDL_COUNT_DATA(p_data_entry->read_index,
			p_data_entry->write_index, p_data_entry->buf_length);

		if (data_len > p_dma->transfer_max) {
			p_entry->write_index = p_data_entry->read_index + p_dma->transfer_max;
			if (p_entry->write_index >= p_data_entry->buf_length)
				p_entry->write_index -= p_data_entry->buf_length;
			p_entry->is_nodata = false;
		}
	}
	p_entry->read_index = p_data_entry->read_index;
	p_entry->buf_length = p_data_entry->buf_length;
	p_entry->phy_addr = p_data_entry->phy_addr;
	p_entry->vir_addr = p_data_entry->vir_addr;
	p_entry->is_valid = true;
	return GDL_OKAY;
}
enum GDL_RET_STATUS gdl_dma_buf_get_data_entry(struct gps_dl_dma_buf *p_dma,
	struct gdl_dma_buf_entry *p_entry)
{
	enum GDL_RET_STATUS ret;

	ASSERT_NOT_NULL(p_dma, GDL_FAIL_ASSERT);
	ASSERT_NOT_NULL(p_entry, GDL_FAIL_ASSERT);

	gps_each_link_spin_lock_take(p_dma->dev_index, GPS_DL_SPINLOCK_FOR_DMA_BUF);
	ret = gdl_dma_buf_get_data_entry_inner(p_dma, p_entry);
	gps_each_link_spin_lock_give(p_dma->dev_index, GPS_DL_SPINLOCK_FOR_DMA_BUF);

	return ret;
}


static enum GDL_RET_STATUS gdl_dma_buf_set_data_entry_inner(struct gps_dl_dma_buf *p_dma,
	struct gdl_dma_buf_entry *p_entry)
{
	struct gdl_dma_buf_entry *p_data_entry;

	if (!p_dma->reader_working)
		return GDL_FAIL_STATE_MISMATCH;

	if (NULL == p_entry) {
		p_dma->reader_working = false;
		return GDL_OKAY;
	}

	if (p_dma->entry_r == p_dma->entry_w) {
		/* impossible due to get_data_entry already check it */
		p_dma->writer_working = false;
		return GDL_FAIL_NOENTRY2;
	}

	p_data_entry = &p_dma->data_entries[p_dma->entry_r];
	if (p_entry->write_index == p_data_entry->write_index) {
		p_data_entry->is_valid = false;
		p_dma->entry_r++;
		if (p_dma->entry_r >= GPS_DL_DMA_BUF_ENTRY_MAX)
			p_dma->entry_r = 0;
	} else
		p_data_entry->read_index = p_entry->write_index;

	p_dma->read_index = p_entry->write_index;
	p_dma->reader_working = false;
	return GDL_OKAY;
}

enum GDL_RET_STATUS gdl_dma_buf_set_data_entry(struct gps_dl_dma_buf *p_dma,
	struct gdl_dma_buf_entry *p_entry)
{
	enum GDL_RET_STATUS ret;

	ASSERT_NOT_NULL(p_dma, GDL_FAIL_ASSERT);

	gps_each_link_spin_lock_take(p_dma->dev_index, GPS_DL_SPINLOCK_FOR_DMA_BUF);
	ret = gdl_dma_buf_set_data_entry_inner(p_dma, p_entry);
	gps_each_link_spin_lock_give(p_dma->dev_index, GPS_DL_SPINLOCK_FOR_DMA_BUF);

	return ret;
}


static enum GDL_RET_STATUS gdl_dma_buf_get_free_entry_inner(struct gps_dl_dma_buf *p_dma,
	struct gdl_dma_buf_entry *p_entry)
{
	unsigned int free_len;

	if (p_dma->writer_working)
		return GDL_FAIL_BUSY;

	p_dma->writer_working = true;

	if (GDL_COUNT_FREE(p_dma->read_index, p_dma->write_index, p_dma->len) <= 1) {
		/* dma buf is full */
		p_dma->writer_working = false;
		return GDL_FAIL_NOSPACE;
	}

	if (GDL_COUNT_FREE(p_dma->entry_r, p_dma->entry_w, GPS_DL_DMA_BUF_ENTRY_MAX) <= 1) {
		/* entries is all used (not use the last one) */
		p_dma->writer_working = false;
		return GDL_FAIL_NOENTRY;
	}

	p_entry->read_index = p_dma->read_index;
	if ((p_dma->transfer_max > 0) && (p_dma->dir == GDL_DMA_D2A)) {
		/* the free space is between write_index to read_index,
		 * if transfer_max set and free_len > it,
		 * limit the free space from write_index to write_index + transfer_max
		 */
		free_len = GDL_COUNT_FREE(p_dma->read_index, p_dma->write_index, p_dma->len);
		if (free_len > p_dma->transfer_max) {
			p_entry->read_index = p_dma->write_index + p_dma->transfer_max;
			if (p_entry->read_index >= p_dma->len)
				p_entry->read_index -= p_dma->len;
		}
	}
	p_entry->write_index = p_dma->write_index;
	p_entry->buf_length = p_dma->len;
	p_entry->phy_addr = p_dma->phy_addr;
	p_entry->vir_addr = p_dma->vir_addr;
	p_entry->is_valid = true;

	/* This field not used for free entry, just make static analysis tool happy. */
	p_entry->is_nodata = false;
	return GDL_OKAY;
}

enum GDL_RET_STATUS gdl_dma_buf_get_free_entry(struct gps_dl_dma_buf *p_dma,
	struct gdl_dma_buf_entry *p_entry, bool nospace_set_pending_rx)
{
	enum GDL_RET_STATUS ret;

	ASSERT_NOT_NULL(p_dma, GDL_FAIL_ASSERT);
	ASSERT_NOT_NULL(p_entry, GDL_FAIL_ASSERT);

	gps_each_link_spin_lock_take(p_dma->dev_index, GPS_DL_SPINLOCK_FOR_DMA_BUF);
	ret = gdl_dma_buf_get_free_entry_inner(p_dma, p_entry);
	if (nospace_set_pending_rx &&
		(ret == GDL_FAIL_NOSPACE || ret == GDL_FAIL_NOENTRY)) {
		p_dma->has_pending_rx = true;
		ret = GDL_FAIL_NOSPACE_PENDING_RX;
	}
	gps_each_link_spin_lock_give(p_dma->dev_index, GPS_DL_SPINLOCK_FOR_DMA_BUF);

	return ret;
}

static enum GDL_RET_STATUS gdl_dma_buf_set_free_entry_inner(struct gps_dl_dma_buf *p_dma,
	struct gdl_dma_buf_entry *p_entry)
{
	struct gdl_dma_buf_entry *p_data_entry;

	if (!p_dma->writer_working)
		return GDL_FAIL_STATE_MISMATCH;

	if (GDL_COUNT_FREE(p_dma->entry_r, p_dma->entry_w, GPS_DL_DMA_BUF_ENTRY_MAX) <= 1) {
		/* impossible due to get_free_entry already check it */
		p_dma->writer_working = false;
		return GDL_FAIL_NOENTRY2;
	}

	p_data_entry = &p_dma->data_entries[p_dma->entry_w];
	p_dma->entry_w++;
	if (p_dma->entry_w >= GPS_DL_DMA_BUF_ENTRY_MAX)
		p_dma->entry_w = 0;

	p_data_entry->read_index = p_dma->write_index;
	p_data_entry->write_index = p_entry->write_index;
	p_data_entry->buf_length = p_dma->len;
	p_data_entry->phy_addr = p_dma->phy_addr;
	p_data_entry->vir_addr = p_dma->vir_addr;
	p_data_entry->is_valid = true;
	p_data_entry->is_nodata = p_entry->is_nodata;

	p_dma->write_index = p_entry->write_index;
	p_dma->writer_working = false;

	return GDL_OKAY;
}

enum GDL_RET_STATUS gdl_dma_buf_set_free_entry(struct gps_dl_dma_buf *p_dma,
	struct gdl_dma_buf_entry *p_entry)
{
	enum GDL_RET_STATUS ret;

	ASSERT_NOT_NULL(p_dma, GDL_FAIL_ASSERT);
	ASSERT_NOT_NULL(p_entry, GDL_FAIL_ASSERT);

	gps_each_link_spin_lock_take(p_dma->dev_index, GPS_DL_SPINLOCK_FOR_DMA_BUF);
	ret = gdl_dma_buf_set_free_entry_inner(p_dma, p_entry);
	gps_each_link_spin_lock_give(p_dma->dev_index, GPS_DL_SPINLOCK_FOR_DMA_BUF);

	return ret;
}


void gps_dma_buf_memcpy_from_rx(void *p_dst, const void *p_src, unsigned int len)
{
	/* TODO:
	 * __dma_unmap_area((void *)p_dst, len, DMA_FROM_DEVICE);
	 * dma_sync_single_for_cpu(DMA_FROM_DEVICE);
	 */
#if GPS_DL_ON_LINUX
	memcpy_fromio(p_dst, p_src, len);
#elif GPS_DL_ON_CTP
	/* gps_dl_ctp_memcpy((unsigned char *)p_dst, (const unsigned char *)p_src, len); */
	memcpy(p_dst, p_src, len);
#else
	memcpy(p_dst, p_src, len);
#endif
}

void gps_dma_buf_memcpy_to_tx(void *p_dst, const void *p_src, unsigned int len)
{
#if GPS_DL_ON_LINUX
	memcpy_toio(p_dst, p_src, len);
#elif GPS_DL_ON_CTP
	/* gps_dl_ctp_memcpy((unsigned char *)p_dst, (const unsigned char *)p_src, len); */
	memcpy(p_dst, p_src, len);
#else
	memcpy(p_dst, p_src, len);
#endif
	/* Use mb to make sure memcpy is done by CPU, and then DMA can be started.  */
	mb();
	/* TODO:
	 * __dma_flush_area((void *)p_dst, len);
	 * dma_sync_single_for_device(DMA_TO_DEVICE);
	 */
}

void gps_dma_buf_memset_io(void *p_dst, unsigned char val, unsigned int len)
{
#if GPS_DL_ON_LINUX
	memset_io(p_dst, val, len);
#elif GPS_DL_ON_CTP
	gps_dl_ctp_memset((unsigned char *)p_dst, val, len);
#else
	memset(p_dst, val, len);
#endif
	/* Use mb to make sure memcpy is done by CPU, and then DMA can be started.  */
	mb();
	/* TODO:
	 * __dma_flush_area((void *)p_dst, len);
	 * dma_sync_single_for_device(DMA_TO_DEVICE);
	 */
}

enum GDL_RET_STATUS gdl_dma_buf_entry_to_buf(const struct gdl_dma_buf_entry *p_entry,
	unsigned char *p_buf, unsigned int buf_len, unsigned int *p_data_len)
{
	unsigned int data_len;
	unsigned int wrap_len;
	unsigned char *p_src;

	ASSERT_NOT_NULL(p_entry, GDL_FAIL_ASSERT);
	ASSERT_NOT_NULL(p_buf, GDL_FAIL_ASSERT);
	ASSERT_NOT_ZERO(buf_len, GDL_FAIL_ASSERT);
	ASSERT_NOT_NULL(p_data_len, GDL_FAIL_ASSERT);

	data_len = GDL_COUNT_DATA(p_entry->read_index,
		p_entry->write_index, p_entry->buf_length);
	GDL_LOGD("r=%u, w=%u, l=%u, d=%u", p_entry->read_index,
			p_entry->write_index, p_entry->buf_length, data_len);

	if (data_len > buf_len) {
		*p_data_len = 0;
		return GDL_FAIL_NOSPACE;
	}

	if (p_entry->write_index > p_entry->read_index) {
		p_src = ((unsigned char *)p_entry->vir_addr) + p_entry->read_index;
		gps_dma_buf_memcpy_from_rx(p_buf, p_src, data_len);
	} else {
		wrap_len = p_entry->buf_length - p_entry->read_index;

		p_src = ((unsigned char *)p_entry->vir_addr) + p_entry->read_index;
		gps_dma_buf_memcpy_from_rx(p_buf, p_src, wrap_len);

		p_src = ((unsigned char *)p_entry->vir_addr) + 0;
		gps_dma_buf_memcpy_from_rx(p_buf + wrap_len, p_src, data_len - wrap_len);
	}

	*p_data_len = data_len;
	return GDL_OKAY;
}

enum GDL_RET_STATUS gdl_dma_buf_buf_to_entry(const struct gdl_dma_buf_entry *p_entry,
	const unsigned char *p_buf, unsigned int data_len, unsigned int *p_write_index)
{
	unsigned int free_len;
	unsigned int wrap_len;
	unsigned int write_index;
	unsigned int alligned_data_len;
	unsigned int fill_zero_len;
	unsigned char *p_dst;

	if (gps_dl_is_1byte_mode()) {
		alligned_data_len = data_len;
		fill_zero_len = 0;
	} else {
		alligned_data_len = ((data_len + 3) / 4) * 4;
		fill_zero_len = alligned_data_len - data_len;
		GDL_LOGD("data_len = %u, alligned = %u", data_len, alligned_data_len);
	}

	ASSERT_NOT_NULL(p_entry, GDL_FAIL_ASSERT);
	ASSERT_NOT_NULL(p_buf, GDL_FAIL_ASSERT);
	ASSERT_NOT_ZERO(data_len, GDL_FAIL_ASSERT);
	ASSERT_NOT_NULL(p_write_index, GDL_FAIL_ASSERT);

	/* TODO: make dma done event */

	free_len = GDL_COUNT_FREE(p_entry->read_index,
		p_entry->write_index, p_entry->buf_length);

	GDL_LOGD("r=%u, w=%u, l=%u, f=%u", p_entry->read_index,
		p_entry->write_index, p_entry->buf_length, free_len);

	if (free_len < alligned_data_len)
		return GDL_FAIL_NOSPACE;

	wrap_len = p_entry->buf_length - p_entry->write_index;
	if (wrap_len >= data_len) {
		p_dst = ((unsigned char *)p_entry->vir_addr) + p_entry->write_index;
		gps_dma_buf_memcpy_to_tx(p_dst, p_buf, data_len);

		write_index = p_entry->write_index + data_len;
		if (write_index >= p_entry->buf_length)
			write_index = 0;
	} else {
		p_dst = ((unsigned char *)p_entry->vir_addr) + p_entry->write_index;
		gps_dma_buf_memcpy_to_tx(p_dst, p_buf, wrap_len);

		p_dst = ((unsigned char *)p_entry->vir_addr) + 0;
		gps_dma_buf_memcpy_to_tx(p_dst, p_buf + wrap_len, data_len - wrap_len);

		write_index = data_len - wrap_len;
	}

	/* fill it to allignment */
	if (fill_zero_len > 0) {
		wrap_len = p_entry->buf_length - write_index;
		if (wrap_len >= fill_zero_len) {
			p_dst = ((unsigned char *)p_entry->vir_addr) + write_index;
			gps_dma_buf_memset_io(p_dst, 0, fill_zero_len);

			write_index += fill_zero_len;
			if (write_index >= p_entry->buf_length)
				write_index = 0;
		} else {
			/* impossible case when buf_len is an integral multiple of 4byte */
			p_dst = ((unsigned char *)p_entry->vir_addr) + write_index;
			gps_dma_buf_memset_io(p_dst, 0, wrap_len);

			p_dst = ((unsigned char *)p_entry->vir_addr) + 0;
			gps_dma_buf_memset_io(p_dst, 0, fill_zero_len - wrap_len);

			write_index = fill_zero_len - wrap_len;
		}
	}

	GDL_LOGD("new_w=%u", write_index);
	*p_write_index = write_index;

	return GDL_OKAY;
}

enum GDL_RET_STATUS gdl_dma_buf_entry_to_transfer(
	const struct gdl_dma_buf_entry *p_entry,
	struct gdl_hw_dma_transfer *p_transfer, bool is_tx)
{
	ASSERT_NOT_NULL(p_entry, GDL_FAIL_ASSERT);
	ASSERT_NOT_NULL(p_transfer, GDL_FAIL_ASSERT);

	p_transfer->buf_start_addr = (unsigned int)p_entry->phy_addr;
	if (is_tx) {
		p_transfer->transfer_start_addr =
			(unsigned int)p_entry->phy_addr + p_entry->read_index;
		p_transfer->len_to_wrap = p_entry->buf_length - p_entry->read_index;
		p_transfer->transfer_max_len = GDL_COUNT_DATA(
			p_entry->read_index, p_entry->write_index, p_entry->buf_length);

		if (!gps_dl_is_1byte_mode()) {
			p_transfer->len_to_wrap /= 4;
			p_transfer->transfer_max_len /= 4;
		}
	} else {
		p_transfer->transfer_start_addr =
			(unsigned int)p_entry->phy_addr + p_entry->write_index;
		p_transfer->len_to_wrap = p_entry->buf_length - p_entry->write_index;
		p_transfer->transfer_max_len = GDL_COUNT_FREE(
			p_entry->read_index, p_entry->write_index, p_entry->buf_length);

		if (!gps_dl_is_1byte_mode()) {
			p_transfer->len_to_wrap /= 4;
			p_transfer->transfer_max_len /= 4;
		}
	}

	GDL_LOGD("r=%u, w=%u, l=%u, is_tx=%d, transfer: ba=0x%08x, ta=0x%08x, wl=%d, tl=%d",
		p_entry->read_index, p_entry->write_index, p_entry->buf_length, is_tx,
		p_transfer->buf_start_addr, p_transfer->buf_start_addr,
		p_transfer->len_to_wrap, p_transfer->transfer_max_len);

	ASSERT_NOT_ZERO(p_transfer->buf_start_addr, GDL_FAIL_ASSERT);
	ASSERT_NOT_ZERO(p_transfer->transfer_start_addr, GDL_FAIL_ASSERT);
	ASSERT_NOT_ZERO(p_transfer->len_to_wrap, GDL_FAIL_ASSERT);
	ASSERT_NOT_ZERO(p_transfer->transfer_max_len, GDL_FAIL_ASSERT);

	return GDL_OKAY;
}

enum GDL_RET_STATUS gdl_dma_buf_entry_transfer_left_to_write_index(
	const struct gdl_dma_buf_entry *p_entry,
	unsigned int left_len, unsigned int *p_write_index)
{
	unsigned int free_len;
	unsigned int new_write_index;

	ASSERT_NOT_NULL(p_entry, GDL_FAIL_ASSERT);
	ASSERT_NOT_NULL(p_write_index, GDL_FAIL_ASSERT);

	free_len = GDL_COUNT_FREE(p_entry->read_index,
		p_entry->write_index, p_entry->buf_length);

	GDL_ASSERT(free_len > left_len, GDL_FAIL_ASSERT, "");

	new_write_index = p_entry->write_index + free_len - left_len;
	if (new_write_index >= p_entry->buf_length)
		new_write_index -= p_entry->buf_length;

	GDL_LOGD("r=%u, w=%u, l=%u, left=%d, new_w=%d",
		p_entry->read_index, p_entry->write_index, p_entry->buf_length,
		left_len, new_write_index);
	GDL_ASSERT(new_write_index < p_entry->buf_length, GDL_FAIL_ASSERT, "");

	*p_write_index = new_write_index;

	return GDL_OKAY;
}


