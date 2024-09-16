/*  SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _LOG_RING_H_
#define _LOG_RING_H_

struct ring {
	/* addr where ring buffer starts */
	void *base;
	/* addr storing the next writable pos, guaranteed to be >= read except when write overflow, but it's ok. */
	unsigned int write;
	/* addr storing the next readable pos, except when read == write as buffer empty */
	unsigned int read;
	/* must be power of 2 */
	unsigned int max_size;
};

struct ring_segment {
	/* addr points into ring buffer for read/write */
	void *ring_pt;
	/* size to read/write */
	unsigned int sz;
	/* pos in external data buffer to read/write */
	unsigned int data_pos;
	/* the size to be read/write after this segment completed */
	unsigned int remain;
};

void log_ring_init(void *base, unsigned int max_size, unsigned int read,
	unsigned int write, struct ring *ring);
unsigned int log_ring_read_prepare(unsigned int sz, struct ring_segment *seg, struct ring *ring);
#define log_ring_read_all_prepare(seg, ring)  log_ring_read_prepare((ring)->max_size, seg, ring)
unsigned int log_ring_write_prepare(unsigned int sz, struct ring_segment *seg, struct ring *ring);
unsigned int log_ring_overwrite_prepare(unsigned int sz,
	struct ring_segment *seg, struct ring *ring);

/* making sure max_size is power of 2 */
#define LOG_RING_VALIDATE_SIZE(max_size) WARN_ON(!max_size)

#define LOG_RING_EMPTY(ring) ((ring)->read == (ring)->write)
/* equation works even when write overflow */
#define LOG_RING_SIZE(ring) ((ring)->write - (ring)->read)
#define LOG_RING_FULL(ring) (LOG_RING_SIZE(ring) == (ring)->max_size)
#define LOG_RING_WRITE_REMAIN_SIZE(ring) ((ring)->max_size - LOG_RING_SIZE(ring))

#define LOG_RING_READ_FOR_EACH(_sz, _seg, _ring) \
	for (log_ring_read_prepare(_sz, &(_seg), _ring), \
		_log_ring_segment_prepare((_ring)->read, &(_seg), (_ring)); \
		(_seg).sz > 0; \
		_log_ring_read_commit(&(_seg), (_ring)), _log_ring_segment_prepare((_ring)->read, \
			&(_seg), (_ring)))

#define LOG_RING_READ_ALL_FOR_EACH(seg, ring) LOG_RING_READ_FOR_EACH((ring)->max_size, seg, ring)

#define LOG_RING_READ_FOR_EACH_ITEM(_sz, _seg, _ring) \
	for (log_ring_read_prepare(_sz, &(_seg), _ring), \
		_log_ring_segment_prepare_item((_ring)->read, &(_seg), (_ring)); \
		(_seg).sz > 0; \
		_log_ring_read_commit(&(_seg), (_ring)), _log_ring_segment_prepare_item((_ring)->read, \
			&(_seg), (_ring)))

#define LOG_RING_WRITE_FOR_EACH(_sz, _seg, _ring) \
	for (log_ring_write_prepare(_sz, &(_seg), _ring),\
		_log_ring_segment_prepare((_ring)->write, &(_seg), (_ring)); \
		(_seg).sz > 0; \
		_log_ring_write_commit(&(_seg), (_ring)), _log_ring_segment_prepare((_ring)->write, \
			&(_seg), (_ring)))

#define LOG_RING_OVERWRITE_FOR_EACH(_sz, _seg, _ring) \
	for (log_ring_overwrite_prepare(_sz, &(_seg), _ring), \
		_log_ring_segment_prepare((_ring)->write, &(_seg), (_ring)); \
		(_seg).sz > 0; \
		_log_ring_write_commit(&(_seg), (_ring)), _log_ring_segment_prepare((_ring)->write, \
			&(_seg), (_ring)))

void log_ring_dump(const char *title, struct ring *ring);
void log_ring_dump_segment(const char *title, struct ring_segment *seg);


/* ring Buffer Internal API */
void _log_ring_segment_prepare(unsigned int from, struct ring_segment *seg, struct ring *ring);
void _log_ring_segment_prepare_item(unsigned int from, struct ring_segment *seg, struct ring *ring);
void _log_ring_read_commit(struct ring_segment *seg, struct ring *ring);
void _log_ring_write_commit(struct ring_segment *seg, struct ring *ring);

#endif
