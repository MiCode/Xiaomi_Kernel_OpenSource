/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 xiaomi Inc.
 */

#ifndef _MPBE_UFS_H
#define _MPBE_UFS_H

#include <linux/types.h>
#include "mpbe.h"

#if IS_ENABLED(CONFIG_MI_MPBE_SUPPORT)

#define MPBE_UFS_RINGBUF_MAX    120

#define MPBE_UFS_QD_LOG         6  //6: QD=64; 5 QD=32;
#define MPBE_UFS_QD             (1UL << (MPBE_UFS_QD_LOG))

#define MPBE_UFS_TAG_ID(TASK_ID) \
	((TASK_ID) & (MPBE_UFS_QD - 1))
#define MPBE_UFS_QUEUE_ID(TASK_ID) \
	((TASK_ID) >> MPBE_UFS_QD_LOG)

enum {
	tsk_send_cmd = 0,
	tsk_req_compl,
	tsk_max
};

struct mpbe_ufs_task {
	__u16 cmd;
	__u16 len;
	__u32 lba;
	__u64 t[tsk_max];
};

/* Context of Request Queue */
struct mpbe_ufs_ctx {
	spinlock_t lock;
	__u64 busy_start_t;
	__u64 period_start_t;
	__u64 period_end_t;
	__u64 period_usage;
	__u64 sum_of_inflight_start;
	__u16 q_depth;
	struct mpbe_ufs_task task[MPBE_UFS_QD];
	struct mpbe_workload workload;
	struct mpbe_throughput throughput;
};

#endif

#endif
