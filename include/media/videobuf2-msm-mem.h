/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * helper functions for physically contiguous PMEM capture buffers
 */

#ifndef _VIDEOBUF2_PMEM_CONTIG_H
#define _VIDEOBUF2_PMEM_CONTIG_H

#include <media/videobuf2-core.h>
#include <mach/msm_subsystem_map.h>

struct videobuf2_mapping {
	unsigned int count;
};

struct videobuf2_contig_pmem {
	u32 magic;
	void *vaddr;
	int phyaddr;
	unsigned long size;
	int is_userptr;
	uint32_t y_off;
	uint32_t cbcr_off;
	int buffer_type;
	struct file *file;
	uint32_t addr_offset;
	int dirty;
	unsigned int count;
	void *alloc_ctx;
	struct msm_mapped_buffer *msm_buffer;
	int subsys_id;
};
void videobuf2_queue_pmem_contig_init(struct vb2_queue *q,
					enum v4l2_buf_type type,
					const struct vb2_ops *ops,
					unsigned int size,
					void *priv);
int videobuf2_pmem_contig_mmap_get(struct videobuf2_contig_pmem *mem,
					uint32_t yoffset,
					uint32_t cbcroffset, int path);
int videobuf2_pmem_contig_user_get(struct videobuf2_contig_pmem *mem,
					uint32_t yoffset, uint32_t cbcroffset,
					uint32_t addr_offset, int path);
void videobuf2_pmem_contig_user_put(struct videobuf2_contig_pmem *mem);
unsigned long videobuf2_to_pmem_contig(struct vb2_buffer *buf,
					unsigned int plane_no);

#endif /* _VIDEOBUF2_PMEM_CONTIG_H */
