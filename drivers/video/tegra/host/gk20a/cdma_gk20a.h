/*
 * drivers/video/tegra/host/gk20a/cdma_gk20a.h
 *
 * Tegra Graphics Host Command DMA
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */
#ifndef __CDMA_GK20A_H__
#define __CDMA_GK20A_H__

#include "mm_gk20a.h"

struct priv_cmd_entry {
	u32 *ptr;
	u64 gva;
	u16 get;	/* start of entry in queue */
	u16 size;	/* in words */
	u32 gp_get;	/* gp_get when submitting last priv cmd */
	u32 gp_put;	/* gp_put when submitting last priv cmd */
	u32 gp_wrap;	/* wrap when submitting last priv cmd */
	bool pre_alloc;	/* prealloc entry, free to free list */
	struct list_head list;	/* node for lists */
};

struct priv_cmd_queue {
	struct priv_cmd_queue_mem_desc mem;
	u64 base_gpuva;	/* gpu_va base */
	u16 size;	/* num of entries in words */
	u16 put;	/* put for priv cmd queue */
	u16 get;	/* get for priv cmd queue */
	struct list_head free;	/* list of pre-allocated free entries */
	struct list_head head;	/* list of used entries */
};

void gk20a_push_buffer_reset(struct push_buffer *pb);
int gk20a_push_buffer_init(struct push_buffer *pb);
void gk20a_push_buffer_destroy(struct push_buffer *pb);
void gk20a_push_buffer_push_to(struct push_buffer *pb,
			       struct mem_mgr *memmgr,
			       struct mem_handle *handle, u32 op1, u32 op2);
void gk20a_push_buffer_pop_from(struct push_buffer *pb, unsigned int slots);
u32 gk20a_push_buffer_space(struct push_buffer *pb);
u32 gk20a_push_buffer_putptr(struct push_buffer *pb);
void gk20a_cdma_start(struct nvhost_cdma *cdma);
void gk20a_cdma_kick(struct nvhost_cdma *cdma);
void gk20a_cdma_stop(struct nvhost_cdma *cdma);

#endif
