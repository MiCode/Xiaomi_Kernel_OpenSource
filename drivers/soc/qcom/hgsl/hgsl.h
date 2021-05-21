/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#ifndef __HGSL_H_
#define __HGSL_H_

#include <linux/types.h>
#include <linux/dma-buf.h>
#include <linux/spinlock.h>
#include <linux/sync_file.h>

#define HGSL_TIMELINE_NAME_LEN 64


struct qcom_hgsl;
struct hgsl_hsync_timeline;

/**
 * HGSL context define
 **/
struct hgsl_context {
	uint32_t context_id;
	struct dma_buf *shadow_dma;
	void *shadow_vbase;
	uint32_t shadow_sop_off;
	uint32_t shadow_eop_off;
	wait_queue_head_t wait_q;
	pid_t pid;
	bool dbq_assigned;

	bool in_destroy;
	struct kref kref;

	uint32_t last_ts;
	struct hgsl_hsync_timeline *timeline;
	uint32_t queued_ts;
	bool is_killed;
};

struct hgsl_priv {
	struct qcom_hgsl *dev;
	uint32_t dbq_idx;
	pid_t pid;

	struct idr isync_timeline_idr;
	spinlock_t isync_timeline_lock;
};


static inline bool hgsl_ts_ge(uint32_t a, uint32_t b)
{
	return a >= b;
}

/**
 * struct hgsl_hsync_timeline - A sync timeline attached under each hgsl context
 * @kref: Refcount to keep the struct alive
 * @name: String to describe this timeline
 * @fence_context: Used by the fence driver to identify fences belonging to
 *		   this context
 * @child_list_head: List head for all fences on this timeline
 * @lock: Spinlock to protect this timeline
 * @last_ts: Last timestamp when signaling fences
 */
struct hgsl_hsync_timeline {
	struct kref kref;
	struct hgsl_context *context;

	char name[HGSL_TIMELINE_NAME_LEN];
	u64 fence_context;

	spinlock_t lock;
	struct list_head fence_list;
	unsigned int last_ts;
};

/**
 * struct hgsl_hsync_fence - A struct containing a fence and other data
 *				associated with it
 * @fence: The fence struct
 * @sync_file: Pointer to the sync file
 * @parent: Pointer to the hgsl sync timeline this fence is on
 * @child_list: List of fences on the same timeline
 * @context_id: hgsl context id
 * @ts: Context timestamp that this fence is associated with
 */
struct hgsl_hsync_fence {
	struct dma_fence fence;
	struct sync_file *sync_file;
	struct hgsl_hsync_timeline *timeline;
	struct list_head child_list;
	u32 context_id;
	unsigned int ts;
};

struct hgsl_isync_timeline {
	struct kref kref;
	struct list_head free_list;
	char name[HGSL_TIMELINE_NAME_LEN];
	int id;
	struct hgsl_priv *priv;
	struct list_head fence_list;
	u64 context;
	spinlock_t lock;
	u32 last_ts;
};

struct hgsl_isync_fence {
	struct dma_fence fence;
	struct hgsl_isync_timeline *timeline;
	struct list_head child_list;
	u32 ts;
};

/* Fence for commands. */
struct hgsl_hsync_fence *hgsl_hsync_fence_create(
					struct hgsl_context *context,
					uint32_t ts);
int hgsl_hsync_fence_create_fd(struct hgsl_context *context,
				uint32_t ts);
int hgsl_hsync_timeline_create(struct hgsl_context *context);
void hgsl_hsync_timeline_signal(struct hgsl_hsync_timeline *timeline,
						unsigned int ts);
void hgsl_hsync_timeline_put(struct hgsl_hsync_timeline *timeline);

/* Fence for process sync. */
int hgsl_isync_timeline_create(struct hgsl_priv *priv,
				    uint32_t *timeline_id);
int hgsl_isync_timeline_destroy(struct hgsl_priv *priv, uint32_t id);
void hgsl_isync_fini(struct hgsl_priv *priv);
int hgsl_isync_fence_create(struct hgsl_priv *priv, uint32_t timeline_id,
						    uint32_t ts, int *fence);
int hgsl_isync_fence_signal(struct hgsl_priv *priv, uint32_t timeline_id,
							       int fence_fd);
int hgsl_isync_forward(struct hgsl_priv *priv, uint32_t timeline_id,
								uint32_t ts);

#endif /* __HGSL_H_ */
