/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef _MMC_MEDIATEK_TRACER_H
#define _MMC_MEDIATEK_TRACER_H

#include <linux/cpufreq.h>
#include <linux/types.h>
#include <linux/mmc/core.h>
#include <linux/mmc/mmc.h>
#include "mtk_blocktag.h"

#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_TRACER)
#define MMC_BIOLOG_RINGBUF_MAX    120
#define MMC_BIOLOG_CONTEXT_TASKS  32
#define MMC_BIOLOG_CONTEXTS       2
#define MMC_SECTOR_SHIFT 9
#define MMC_MTK_BIO_TRACE_LATENCY ((unsigned long long)(1000000000))

#define MMC_DATA_DIR(x)		(((x) & 1) << 12)

#define MAX_CPU_CLUSTER 3
static int s_final_cpu_freq[MAX_CPU_CLUSTER] = {2000000, 2000000, 1500000};
static int s_free_cpu_freq[MAX_CPU_CLUSTER] = {-1, -1, -1};
static unsigned int s_cluster_num;
static unsigned int s_cluster_freq_rdy;
static int *s_target_freq;
static struct freq_qos_request *s_tchbst_rq;
static bool mmc_qos_enable;

enum {
	tsk_send_cmd = 0,
	tsk_req_compl,
	tsk_max
};

enum {
	B_STORAGE_MMC_T     = 0,
	B_STORAGE_SD_T      = 1
};

struct mmc_mtk_bio_context_task {
	__u32 qid;
	__u32 dir;
	__u32 len;
	__u32 lba;
	uint64_t t[tsk_max];
};

/* Context of Request Queue */
struct mmc_mtk_bio_context {
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
	uint32_t last_workload_percent;
	struct mmc_mtk_bio_context_task task[MMC_BIOLOG_CONTEXT_TASKS];
	struct mtk_btag_workload workload;
	struct mtk_btag_throughput throughput;
	struct mtk_btag_pidlogger pidlog;
};

#endif

#endif
