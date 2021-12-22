/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef _UFS_MTK_BLOCK_H
#define _UFS_MTK_BLOCK_H

#include <linux/types.h>
#include <mt-plat/mtk_blocktag.h>
#include "ufshcd.h"

#if defined(CONFIG_MTK_UFS_BLOCK_IO_LOG)

int ufs_mtk_biolog_init(bool qos_allowed);
int ufs_mtk_biolog_exit(void);

void ufs_mtk_biolog_send_command(unsigned int task_id,
				 struct scsi_cmnd *cmd);
void ufs_mtk_biolog_transfer_req_compl(unsigned int taski_id,
				       unsigned long req_mask);
void ufs_mtk_biolog_check(unsigned long req_mask);
void ufs_mtk_biolog_clk_gating(bool gated);

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

#else

#define ufs_mtk_biolog_init(...)
#define ufs_mtk_biolog_exit(...)
#define ufs_mtk_biolog_send_command(...)
#define ufs_mtk_biolog_transfer_req_compl(...)
#define ufs_mtk_biolog_check(...)
#define ufs_mtk_biolog_clk_gating(...)

#endif

#endif
