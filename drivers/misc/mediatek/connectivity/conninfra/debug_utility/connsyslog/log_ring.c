// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "log_ring.h"
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/bug.h>



void log_ring_init(void *base, unsigned int max_size, unsigned int read,
	unsigned int write, struct ring *ring)
{
	pr_info("base=0x%lx max_size=%x read=%x write=%x", base, max_size, read, write);

	WARN_ON(!base);
	pr_info("xxxx-1");
	LOG_RING_VALIDATE_SIZE(max_size);
	pr_info("xxx-2");
	/* making sure write largger than read */
	WARN_ON(read > write);
	pr_info("xxx-3");
	ring->base = base;
	ring->read = read;
	ring->write = write;
	ring->max_size = max_size;
}

void log_ring_dump(const char *title, struct ring *ring)
{
	pr_info("[%s] ring:{write=%d, read=%d, max_size=%d}\n",
			title, ring->write, ring->read, ring->max_size);
}

void log_ring_dump_segment(const char *title, struct ring_segment *seg)
{
	pr_info("[%s] seg:{ring_pt=0x%p, data_pos=%d, sz=%d, remain=%d}\n",
			title, seg->ring_pt, seg->data_pos, seg->sz, seg->remain);
}

/*
 * Function prepares the ring_segment and returns the number of valid bytes for read.
 */
unsigned int log_ring_read_prepare(unsigned int sz, struct ring_segment *seg, struct ring *ring)
{
	unsigned int wt = ring->write;
	unsigned int rd = ring->read;

	memset(seg, 0, sizeof(struct ring_segment));
	if (sz > wt - rd)
		sz = wt - rd;
	seg->remain = sz;
	/* log_ring_dump(__func__, ring); */
	/* log_ring_dump_segment(__func__, seg); */
	return seg->remain;
}

/*
 * Function prepares the ring_segment and returns the number of bytes available for write.
 */
unsigned int log_ring_write_prepare(unsigned int sz, struct ring_segment *seg, struct ring *ring)
{
	unsigned int wt = ring->write;
	unsigned int rd = ring->read;

	memset(seg, 0, sizeof(struct ring_segment));
	if (sz > ring->max_size - (wt - rd))
		sz = ring->max_size - (wt - rd);
	seg->remain = sz;
	/* log_ring_dump(__func__, ring); */
	/* log_ring_dump_segment(__func__, seg); */
	return seg->remain;
}

unsigned int log_ring_overwrite_prepare(unsigned int sz, struct ring_segment *seg,
						      struct ring *ring)
{
	unsigned int wt = ring->write;
	unsigned int rd = ring->read;

	memset(seg, 0, sizeof(struct ring_segment));
	if (sz > ring->max_size - (wt - rd))
		ring->read += sz - (ring->max_size - (wt - rd));
	seg->remain = sz;
	/* log_ring_dump(__func__, ring); */
	/* log_ring_dump_segment(__func__, seg); */
	return seg->remain;
}

void __log_ring_segment_prepare(unsigned int from, unsigned int sz, struct ring_segment *seg,
					      struct ring *ring)
{
	unsigned int ring_pos = from;

	if (ring_pos >= ring->max_size)
		ring_pos -= ring->max_size;

	seg->ring_pt = ring->base + ring_pos;
	seg->data_pos = (seg->sz ? seg->data_pos + seg->sz : 0);
	if (ring_pos + sz <= ring->max_size)
		seg->sz = sz;
	else
		seg->sz = ring->max_size - ring_pos;
	seg->remain -= seg->sz;
	/* log_ring_dump(__func__, ring); */
	/* log_ring_dump_segment(__func__, seg); */
}

void _log_ring_segment_prepare(unsigned int from, struct ring_segment *seg, struct ring *ring)
{
	__log_ring_segment_prepare(from, seg->remain, seg, ring);
}

void _log_ring_segment_prepare_item(unsigned int from, struct ring_segment *seg, struct ring *ring)
{
	unsigned int size;

	size = (seg->remain ? 1 : 0);
	__log_ring_segment_prepare(from, size, seg, ring);
}

void _log_ring_read_commit(struct ring_segment *seg, struct ring *ring)
{
	ring->read += seg->sz;
	if (ring->read >= ring->max_size)
		ring->read -= ring->max_size;
	/* log_ring_dump(__func__, ring); */
	/* log_ring_dump_segment(__func__, seg); */
}
void _log_ring_write_commit(struct ring_segment *seg, struct ring *ring)
{
	ring->write += seg->sz;
	if (ring->write >= ring->max_size)
		ring->write -= ring->max_size;

	/* log_ring_dump(__func__, ring); */
	/* log_ring_dump_segment(__func__, seg); */
}

