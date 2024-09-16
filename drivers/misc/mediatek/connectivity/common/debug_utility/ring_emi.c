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
#include "ring_emi.h"
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/bug.h>

void ring_emi_init(void *base, unsigned int max_size, void *read, void *write, struct ring_emi *ring_emi)
{
	WARN_ON(!base || !read || !write);

	/* making sure max_size is power of 2 */
	WARN_ON(!max_size || (max_size & (max_size - 1)));

	/* making sure read & write pointers are 4 bytes aligned */
	WARN_ON(((long)read & 0x3) != 0 || ((long)write & 0x3) != 0);

	ring_emi->base = base;
	ring_emi->read = read;
	ring_emi->write = write;
	if (ring_emi->write)
		EMI_WRITE32(ring_emi->write, 0);
	if (ring_emi->read)
		EMI_WRITE32(ring_emi->read, 0);
	ring_emi->max_size = max_size;
	pr_info("base: %p, read: %p, write: %p, max_size: %d\n", base, read, write, max_size);
}

void ring_emi_dump(const char *title, struct ring_emi *ring_emi)
{
	pr_info("[%s] ring_emi:{base=0x%p, write=%d, read=%d, max_size=%d}\n",
			title, ring_emi->base, EMI_READ32(ring_emi->write),
			EMI_READ32(ring_emi->read), ring_emi->max_size);
}

void ring_emi_dump_segment(const char *title, struct ring_emi_segment *seg)
{
	pr_info("[%s] seg:{ring_emi_pt=0x%p, data_pos=%d, sz=%d, remain=%d}\n",
			title, seg->ring_emi_pt, seg->data_pos, seg->sz, seg->remain);
}

/*
 * Function prepares the ring_emi_segment and returns the number of valid bytes for read.
 */
unsigned int ring_emi_read_prepare(unsigned int sz, struct ring_emi_segment *seg, struct ring_emi *ring_emi)
{
	unsigned int wt = EMI_READ32(ring_emi->write);
	unsigned int rd = EMI_READ32(ring_emi->read);

	memset(seg, 0, sizeof(struct ring_emi_segment));
#ifdef ROUND_REPEAT
	if (wt >= rd) {
		if (sz > wt - rd)
			sz = wt - rd;
		seg->remain = sz;
	} else {
		if (sz > ring_emi->max_size - (rd - wt))
			sz = ring_emi->max_size - (rd - wt);
		seg->remain = sz;
	}
#else
	if (sz > wt - rd)
		sz = wt - rd;
	seg->remain = sz;
#endif
	/* ring_emi_dump(__func__, ring_emi); */
	/* ring_emi_dump_segment(__func__, seg); */
	return seg->remain;
}

/*
 * Function prepares the ring_emi_segment and returns the number of bytes available for write.
 */
unsigned int ring_emi_write_prepare(unsigned int sz, struct ring_emi_segment *seg, struct ring_emi *ring_emi)
{
	unsigned int wt = EMI_READ32(ring_emi->write);
	unsigned int rd = EMI_READ32(ring_emi->read);

	memset(seg, 0, sizeof(struct ring_emi_segment));
#ifdef ROUND_REPEAT
	if (wt >= rd)
		seg->remain = ring_emi->max_size - (wt - rd + 1);
	else
		seg->remain = ring_emi->max_size - (rd - wt + 1);

	if (sz <= seg->remain)
		seg->remain = sz;
#else
	if (sz > ring_emi->max_size - (wt - rd))
		sz = ring_emi->max_size - (wt - rd);
	seg->remain = sz;
#endif
	/* ring_emi_dump(__func__, ring_emi); */
	/* ring_emi_dump_segment(__func__, seg); */
	return seg->remain;
}

void _ring_emi_segment_prepare(unsigned int from, struct ring_emi_segment *seg, struct ring_emi *ring_emi)
{
#ifndef ROUND_REPEAT
	unsigned int ring_emi_pos = from & (ring_emi->max_size - 1);

	seg->ring_emi_pt = ring_emi->base + ring_emi_pos;
#else
	seg->ring_emi_pt = ring_emi->base + from;
#endif
	seg->data_pos = (seg->sz ? seg->data_pos + seg->sz : 0);
	if (from + seg->remain <= ring_emi->max_size)
		seg->sz = seg->remain;
	else
		seg->sz = ring_emi->max_size - from;
	seg->remain -= seg->sz;
	/* ring_emi_dump(__func__, ring_emi); */
	/* ring_emi_dump_segment(__func__, seg); */
}

void _ring_emi_read_commit(struct ring_emi_segment *seg, struct ring_emi *ring_emi)
{
#ifdef ROUND_REPEAT
	EMI_WRITE32(ring_emi->read, (EMI_READ32(ring_emi->read) + seg->sz) & (ring_emi->max_size - 1));
#else
	EMI_WRITE32(ring_emi->read, EMI_READ32(ring_emi->read) + seg->sz);
#endif
	/* *(ring_emi->read) += seg->sz; */
	/* ring_emi_dump(__func__, ring_emi); */
	/* ring_emi_dump_segment(__func__, seg); */
}
void _ring_emi_write_commit(struct ring_emi_segment *seg, struct ring_emi *ring_emi)
{
#ifdef ROUND_REPEAT
	EMI_WRITE32(ring_emi->write, (EMI_READ32(ring_emi->write) + seg->sz) & (ring_emi->max_size - 1));
#else
	EMI_WRITE32(ring_emi->write, EMI_READ32(ring_emi->write) + seg->sz);
#endif
	/* ring_emi_dump(__func__, ring_emi); */
	/* ring_emi_dump_segment(__func__, seg); */
}

