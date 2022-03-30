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
	__u32 qid;
	__u16 cmd;
	__u16 len;
	__u32 lba;
	uint64_t t[tsk_max];
};

/* Context of Request Queue */
struct ufs_mtk_bio_context {
	int id;
	int state;
	pid_t pid;
	__u16 qid;
	__u16 q_depth;
	__u16 q_depth_top;
	spinlock_t lock;
	uint64_t busy_start_t;
	uint64_t period_start_t;
	uint64_t period_end_t;
	uint64_t period_usage;
	struct ufs_mtk_bio_context_task task[UFS_BIOLOG_CONTEXT_TASKS];
	struct mtk_btag_workload workload;
	struct mtk_btag_throughput throughput;
	struct mtk_btag_pidlogger pidlog;
};

#endif

#endif
