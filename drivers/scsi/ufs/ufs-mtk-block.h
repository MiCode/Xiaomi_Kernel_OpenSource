/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _UFS_MTK_BLOCK_H
#define _UFS_MTK_BLOCK_H

#include <linux/types.h>
#include <mt-plat/mtk_blocktag.h>
#include "ufshcd.h"

#if defined(CONFIG_MTK_UFS_BLOCK_IO_LOG)

int ufs_mtk_biolog_init(void);
int ufs_mtk_biolog_exit(void);

void ufs_mtk_biolog_queue_command(unsigned int taski_id, struct scsi_cmnd *cmd);
void ufs_mtk_biolog_send_command(unsigned int taski_id);
void ufs_mtk_biolog_transfer_req_compl(unsigned int taski_id);
void ufs_mtk_biolog_scsi_done_start(unsigned int taski_id);
void ufs_mtk_biolog_scsi_done_end(unsigned int taski_id);
void ufs_mtk_biolog_check(unsigned long req_mask);

#define UFS_BIOLOG_RINGBUF_MAX    120
#define UFS_BIOLOG_CONTEXT_TASKS  32
#define UFS_BIOLOG_CONTEXTS       1

enum {
	tsk_request_start = 0,
	tsk_send_cmd,
	tsk_req_compl,
	tsk_scsi_done_start,
	tsk_scsi_done_end,
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
#define ufs_mtk_biolog_queue_command(...)
#define ufs_mtk_biolog_send_command(...)
#define ufs_mtk_biolog_transfer_req_compl(...)
#define ufs_mtk_biolog_scsi_done_start(...)
#define ufs_mtk_biolog_scsi_done_end(...)
#define ufs_mtk_biolog_check(...)

#endif

#endif
