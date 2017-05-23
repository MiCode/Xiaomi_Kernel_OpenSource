/* Copyright (c) 2016 The Linux Foundation. All rights reserved.
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

#ifndef MSM_SNAPSHOT_H_
#define MSM_SNAPSHOT_H_

#include <linux/string.h>
#include <linux/seq_buf.h>
#include "msm_snapshot_api.h"

struct msm_snapshot {
	void *ptr;
	struct seq_buf buf;
	phys_addr_t physaddr;
	uint32_t index;
	uint32_t remain;
	unsigned long timestamp;
	void *priv;
};

/* Write a uint32_t value to the next position in the snapshot buffer */
static inline void SNAPSHOT_WRITE_U32(struct msm_snapshot *snapshot,
		uint32_t value)
{
	seq_buf_putmem(&snapshot->buf, &value, sizeof(value));
}

/* Copy a block of memory to the next position in the snapshot buffer */
static inline void SNAPSHOT_MEMCPY(struct msm_snapshot *snapshot, void *src,
		uint32_t size)
{
	if (size)
		seq_buf_putmem(&snapshot->buf, src, size);
}

static inline bool _snapshot_header(struct msm_snapshot *snapshot,
		struct msm_snapshot_section_header *header,
		u32 headsz, u32 datasz, u32 id)
{
	u32 size = headsz + datasz;

	if (seq_buf_buffer_left(&snapshot->buf) <= size)
		return false;

	/* Write the section header */
	header->magic = SNAPSHOT_SECTION_MAGIC;
	header->id = id;
	header->size = headsz + datasz;

	/* Write the section header */
	seq_buf_putmem(&snapshot->buf, header, headsz);

	/* The caller will fill in the data from here */
	return true;
}

/* SNAPSHOT_HEADER
 * _snapshot: pointer to struct msm_snapshot
 * _header: Local variable containing the sub-section header
 * _id: Section ID to write
 * _dword: Size of the data section (in dword)
 */
#define SNAPSHOT_HEADER(_snapshot, _header, _id, _dwords) \
	_snapshot_header((_snapshot), \
		(struct msm_snapshot_section_header *) &(_header), \
		sizeof(_header), (_dwords) << 2, (_id))

struct msm_gpu;

struct msm_snapshot *msm_snapshot_new(struct msm_gpu *gpu);
void msm_snapshot_destroy(struct msm_gpu *gpu, struct msm_snapshot *snapshot);
int msm_gpu_snapshot(struct msm_gpu *gpu, struct msm_snapshot *snapshot);
int msm_snapshot_write(struct msm_gpu *gpu, struct seq_file *m);

#endif

