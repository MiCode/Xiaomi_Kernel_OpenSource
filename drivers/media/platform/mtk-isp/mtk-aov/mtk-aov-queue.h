/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_AOV_QUEUE_H__
#define __MTK_AOV_QUEUE_H__

#include <linux/types.h>
#include <linux/string.h>
#include <linux/atomic.h>

#define QUEUE_MAX_SIZE    (32)

#if defined(CONFIG_HAVE_CMPXCHG_DOUBLE)
struct queue_node {
		uintptr_t ptr;
		uintptr_t ref;
} __aligned(16);

struct queue {
		struct queue_node node[QUEUE_MAX_SIZE];

		atomic_t rear;
		atomic_t front;
		atomic_t size;
		bool init;
};
#else
struct queue {
		bool init;
		uintptr_t data[QUEUE_MAX_SIZE];
		ssize_t rear;
		ssize_t front;
		ssize_t size;
		spinlock_t lock;
};
#endif

#ifdef __cplusplus
extern "C" {
#endif

int32_t queue_init(struct queue *queue);

int32_t queue_push(struct queue *queue, void *data);

void *queue_pop(struct queue *queue);

int32_t queue_size(struct queue *queue);

bool queue_empty(struct queue *queue);

int32_t queue_deinit(struct queue *queue);

#ifdef __cplusplus
}
#endif

#endif  // __MTK_AOV_QUEUE_H__
