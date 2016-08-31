/*
 * drivers/video/tegra/host/gk20a/cdma_gk20a.c
 *
 * Tegra Graphics Host Command DMA
 *
 * Copyright (c) 2011, NVIDIA CORPORATION.  All rights reserved.
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

#include "../nvhost_cdma.h"
#include "../dev.h"

#include "cdma_gk20a.h"

/*
 * push_buffer
 *
 * The push buffer is a circular array of words to be fetched by command DMA.
 * Note that it works slightly differently to the sync queue; fence == cur
 * means that the push buffer is full, not empty.
 */


/**
 * Reset to empty push buffer
 */
void gk20a_push_buffer_reset(struct push_buffer *pb)
{
}

/**
 * Init push buffer resources
 */
int gk20a_push_buffer_init(struct push_buffer *pb)
{
	return 0;
}

/**
 * Clean up push buffer resources
 */
void gk20a_push_buffer_destroy(struct push_buffer *pb)
{
}

/**
 * Push two words to the push buffer
 * Caller must ensure push buffer is not full
 */
void gk20a_push_buffer_push_to(struct push_buffer *pb,
			       struct mem_mgr *client,
			       struct mem_handle *handle, u32 op1, u32 op2)
{
}

/**
 * Pop a number of two word slots from the push buffer
 * Caller must ensure push buffer is not empty
 */
void gk20a_push_buffer_pop_from(struct push_buffer *pb, unsigned int slots)
{
}

/**
 * Return the number of two word slots free in the push buffer
 */
u32 gk20a_push_buffer_space(struct push_buffer *pb)
{
	return 0;
}

u32 gk20a_push_buffer_putptr(struct push_buffer *pb)
{
	return 0;
}


/**
 * Start channel DMA
 */
void gk20a_cdma_start(struct nvhost_cdma *cdma)
{
}

/**
 * Kick channel DMA into action by writing its PUT offset (if it has changed)
 */
void gk20a_cdma_kick(struct nvhost_cdma *cdma)
{
}

void gk20a_cdma_stop(struct nvhost_cdma *cdma)
{
}
