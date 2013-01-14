/* Copyright (c) 2011, The Linux Foundation. All rights reserved.
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

#ifndef _VIDEOBUF_PMEM_CONTIG_H
#define _VIDEOBUF_PMEM_CONTIG_H

#include <media/videobuf-core.h>

struct videobuf_contig_pmem {
	u32 magic;
	void *vaddr;
	int phyaddr;
	unsigned long size;
	int is_userptr;
	uint32_t y_off;
	uint32_t cbcr_off;
	int buffer_type;
	struct file *file;
};

void videobuf_queue_pmem_contig_init(struct videobuf_queue *q,
			const struct videobuf_queue_ops *ops,
			struct device *dev,
			spinlock_t *irqlock,
			enum v4l2_buf_type type,
			enum v4l2_field field,
			unsigned int msize,
			void *priv,
			struct mutex *ext_lock);

int videobuf_to_pmem_contig(struct videobuf_buffer *buf);
int videobuf_pmem_contig_free(struct videobuf_queue *q,
			struct videobuf_buffer *buf);

#endif /* _VIDEOBUF_PMEM_CONTIG_H */
