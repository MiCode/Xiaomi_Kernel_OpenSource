/*
 * Copyright (C) 2013 Red Hat
 * Author: Rob Clark <robdclark@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __MSM_RINGBUFFER_H__
#define __MSM_RINGBUFFER_H__

#include "msm_drv.h"

#define rbmemptr(ring, member) \
	((ring)->memptrs_iova + offsetof(struct msm_memptrs, member))

struct msm_memptr_ticks {
	uint64_t started;
	uint64_t retired;
};

struct msm_memptrs {
	volatile uint32_t rptr;
	volatile uint32_t fence;
	volatile uint64_t ttbr0;
	volatile unsigned int contextidr;
	struct msm_memptr_ticks ticks[128];
};

#define RING_TICKS_IOVA(ring, index, field) \
	((ring)->memptrs_iova + offsetof(struct msm_memptrs, ticks) + \
	 ((index) * sizeof(struct msm_memptr_ticks)) + \
	 offsetof(struct msm_memptr_ticks, field))

struct msm_ringbuffer {
	struct msm_gpu *gpu;
	int id;
	struct drm_gem_object *bo;
	uint32_t *start, *end, *cur, *next;
	uint64_t iova;
	uint32_t seqno;
	uint32_t submitted_fence;
	spinlock_t lock;
	struct list_head submits;
	uint32_t hangcheck_fence;

	struct msm_memptrs *memptrs;
	uint64_t memptrs_iova;
	int tick_index;
};

struct msm_ringbuffer *msm_ringbuffer_new(struct msm_gpu *gpu, int id,
		struct msm_memptrs *memptrs, uint64_t memptrs_iova);
void msm_ringbuffer_destroy(struct msm_ringbuffer *ring);

/* ringbuffer helpers (the parts that are same for a3xx/a2xx/z180..) */

static inline void
OUT_RING(struct msm_ringbuffer *ring, uint32_t data)
{
	/*
	 * ring->next points to the current command being written - it won't be
	 * committed as ring->cur until the flush
	 */
	if (ring->next == ring->end)
		ring->next = ring->start;
	*(ring->next++) = data;
}

#endif /* __MSM_RINGBUFFER_H__ */
