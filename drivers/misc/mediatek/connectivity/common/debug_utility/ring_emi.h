/*
 * Copyright (C) 2016 MediaTek Inc.
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
#ifndef _RING_EMI_H_
#define _RING_EMI_H_

#include <linux/io.h>
#define ROUND_REPEAT
#define EMI_READ32(addr) (readl(addr))
#define EMI_WRITE32(addr, data) (writel(data, addr))

struct ring_emi {
	/* addr where ring buffer starts */
	void *base;
	/* addr storing the next writable pos, guaranteed to be >= read except when write overflow, but it's ok. */
	void *write;
	/* addr storing the next readable pos, except when read == write as buffer empty */
	void *read;
	/* must be power of 2 */
	unsigned int max_size;
};

struct ring_emi_segment {
	/* addr points into ring buffer for read/write */
	void *ring_emi_pt;
	/* size to read/write */
	unsigned int sz;
	/* pos in external data buffer to read/write */
	unsigned int data_pos;
	/* the size to be read/write after this segment completed */
	unsigned int remain;
};

void ring_emi_init(void *base, unsigned int max_size, void *read, void *write, struct ring_emi *ring_emi);
unsigned int ring_emi_read_prepare(unsigned int sz, struct ring_emi_segment *seg, struct ring_emi *ring_emi);
#define ring_emi_read_all_prepare(seg, ring_emi)  ring_emi_read_prepare((ring_emi)->max_size, seg, ring_emi)
unsigned int ring_emi_write_prepare(unsigned int sz, struct ring_emi_segment *seg, struct ring_emi *ring_emi);

/* making sure max_size is power of 2 */
#define RING_EMI_VALIDATE_SIZE(max_size) WARN_ON(!max_size || (max_size & (max_size - 1)))

#define RING_EMI_EMPTY(ring_emi) (EMI_READ32((ring_emi)->read) == EMI_READ32((ring_emi)->write))
/* equation works even when write overflow */
#define RING_EMI_SIZE(ring_emi) (EMI_READ32((ring_emi)->write) - EMI_READ32((ring_emi)->read))
#ifdef ROUND_REPEAT
#define RING_EMI_FULL(ring_emi) (((EMI_READ32((ring_emi)->write) + 1) & ((ring_emi)->max_size - 1)) \
	== EMI_READ32((ring_emi)->read))
#else
#define RING_EMI_FULL(ring_emi) (RING_EMI_SIZE(ring_emi) == (ring_emi)->max_size)
#endif

#define RING_EMI_READ_FOR_EACH(_sz, _seg, _ring_emi) \
	for (_ring_emi_segment_prepare(EMI_READ32((_ring_emi)->read), &(_seg), (_ring_emi)); \
		(_seg).sz > 0; \
		_ring_emi_read_commit(&(_seg), (_ring_emi)), \
		_ring_emi_segment_prepare(EMI_READ32((_ring_emi)->read), &(_seg), (_ring_emi)))

#define RING_EMI_READ_ALL_FOR_EACH(seg, ring_emi) RING_EMI_READ_FOR_EACH((ring_emi)->max_size, seg, ring_emi)

#define RING_EMI_WRITE_FOR_EACH(_sz, _seg, _ring_emi) \
	for (_ring_emi_segment_prepare(EMI_READ32((_ring_emi)->write), &(_seg), (_ring_emi)); \
		(_seg).sz > 0; \
		_ring_emi_write_commit(&(_seg), (_ring_emi)), \
		_ring_emi_segment_prepare(EMI_READ32((_ring_emi)->write), &(_seg), (_ring_emi)))

void ring_emi_dump(const char *title, struct ring_emi *ring_emi);
void ring_emi_dump_segment(const char *title, struct ring_emi_segment *seg);


/* Ring Buffer Internal API */
void _ring_emi_segment_prepare(unsigned int from, struct ring_emi_segment *seg, struct ring_emi *ring_emi);
void _ring_emi_read_commit(struct ring_emi_segment *seg, struct ring_emi *ring_emi);
void _ring_emi_write_commit(struct ring_emi_segment *seg, struct ring_emi *ring_emi);

#endif
