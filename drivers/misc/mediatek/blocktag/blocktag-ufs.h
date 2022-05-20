/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef _UFS_MEDIATEK_TRACER_H
#define _UFS_MEDIATEK_TRACER_H

#include <linux/types.h>
#include "mtk_blocktag.h"

#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_TRACER)

#define UFS_BIOLOG_RINGBUF_MAX    120
#define UFS_BIOLOG_CONTEXT_TASKS  32
#define UFS_BIOLOG_CONTEXTS       1

enum {
	tsk_send_cmd = 0,
	tsk_req_compl,
	tsk_max
};

struct ufs_mtk_bio_context_task {
	__u16 cmd;
	__u16 len;
	__u32 lba;
	__u64 t[tsk_max];
};

/* Context of Request Queue */
struct ufs_mtk_bio_context {
	spinlock_t lock;
	__u64 busy_start_t;
	__u64 period_start_t;
	__u64 period_end_t;
	__u64 period_usage;
	__u64 sum_of_inflight_start;
	__u16 q_depth;
	struct ufs_mtk_bio_context_task task[UFS_BIOLOG_CONTEXT_TASKS];
	struct mtk_btag_workload workload;
	struct mtk_btag_throughput throughput;
	struct mtk_btag_proc_pidlogger pidlog;
};

#endif

#endif
