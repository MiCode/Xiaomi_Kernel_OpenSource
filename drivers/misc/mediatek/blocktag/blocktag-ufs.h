/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef _UFS_MEDIATEK_TRACER_H
#define _UFS_MEDIATEK_TRACER_H

#include <linux/types.h>
#include "mtk_blocktag.h"

#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_TRACER)

#define BTAG_UFS_RINGBUF_MAX    120

#define BTAG_UFS_QD_LOG         5
#define BTAG_UFS_QD             (1UL << (BTAG_UFS_QD_LOG))

#define BTAG_UFS_TAG_ID(TASK_ID) \
	((TASK_ID) & (BTAG_UFS_QD - 1))
#define BTAG_UFS_QUEUE_ID(TASK_ID) \
	((TASK_ID) >> BTAG_UFS_QD_LOG)

enum {
	tsk_send_cmd = 0,
	tsk_req_compl,
	tsk_max
};

struct mtk_btag_ufs_task {
	__u16 cmd;
	__u16 len;
	__u32 lba;
	__u64 t[tsk_max];
};

/* Context of Request Queue */
struct mtk_btag_ufs_ctx {
	spinlock_t lock;
	__u64 busy_start_t;
	__u64 period_start_t;
	__u64 period_end_t;
	__u64 period_usage;
	__u64 sum_of_inflight_start;
	__u16 q_depth;
	struct mtk_btag_ufs_task task[BTAG_UFS_QD];
	struct mtk_btag_workload workload;
	struct mtk_btag_throughput throughput;
	struct mtk_btag_proc_pidlogger pidlog;
};

#endif

#endif
