/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __VPU_POOL_H__
#define __VPU_POOL_H__

#include <linux/types.h>
#include <linux/mutex.h>
#include "vpu_drv.h"

enum vpu_pool_type {
	VPU_POOL,
	VPU_POOL_DEP,
};

/** Type of pools
 * VPU_POOL: list of requests. For "Common Pool" and "Self-Pool"
 *   pool_head
 *      +->req1->req2->req3
 *
 * VPU_POOL_DEP: two-level linked-list. For "MultiProc Pool"
 *   1st level: list of sub-pools that holds dependent requests
 *              for multi-core processing.
 *   2nd level: list of requests that have dependencies.
 *
 *   pool_head
 *      +->sub-pool.A
 *      |      +->req.A1->req.A2->req.A3
 *      +->sub-pool.B
 *      |      +->req.B1->req.B2
 *      +->sub-pool.C
 *             +->req.C1->req.C2->req.C3->req.C4
 */

struct vpu_pool {
	char name[16];
	struct mutex lock;
	struct list_head pool;
	int type;
	int size;
	void *priv;
};

struct vpu_pool_dep {
	struct list_head link;     /* link in the 1st level pool */
	struct list_head sub_pool; /* head of the sub-pool */
	unsigned long *user;
	uint64_t head_id;
	uint64_t tail_id;
};

/**
 * vpu_pool_init - Initialize VPU request pool
 * @p: pointer to allocated pool
 * @name: name of the pool, less than 15 characters.
 * @type: VPU_POOL, or VPU_POOL_DEP
 */
void vpu_pool_init(struct vpu_pool *p, const char *name, int type);

/**
 * vpu_pool_size - Return the number of requests in the pool
 * @p: pointer to the pool
 */
int vpu_pool_size(struct vpu_pool *p);

/**
 * vpu_pool_is_empty - Check if the pool is empty
 * @p: pointer to the pool
 * Returns: Non-Zero, if empty.
 */
int vpu_pool_is_empty(struct vpu_pool *p);

/**
 * vpu_pool_enqueue - Enqueue a request to pool
 * @p: pointer to the vpu request pool
 * @req: vpu request to be enqueued
 * @priority: [Optional] Pointer to the priority counter.
 *            The counter is incremented by 1, after enqueue a request.
 *            Ignored, if pool type is VPU_POOL_DEP.
 */
int vpu_pool_enqueue(struct vpu_pool *p, struct vpu_request *req,
	unsigned int *priority);

/**
 * vpu_pool_dequeue - Dequeue a request from pool
 * @p: pointer to the vpu request pool
 * @priority: [Optional] pointer to the priority counter.
 *            The counter is decremented by 1, after dequeue a request.
 *            Ignored, if pool type is VPU_POOL_DEP.
 * Returns: the pointer to dequeued request.
 */
struct vpu_request *vpu_pool_dequeue(struct vpu_pool *p,
	unsigned int *priority);

#endif

