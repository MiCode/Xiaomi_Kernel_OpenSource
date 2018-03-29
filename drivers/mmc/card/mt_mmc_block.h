/*
 * Copyright (C) 2016 MediaTek Inc.
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

#ifndef _MT_MMC_BLOCK_H
#define _MT_MMC_BLOCK_H

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/mmc/core.h>
#include <linux/mmc/host.h>

#if defined(CONFIG_MMC_BLOCK_IO_LOG)

int mt_mmc_biolog_init(void);
int mt_mmc_biolog_exit(void);

void mt_bio_queue_alloc(struct task_struct *thread);
void mt_bio_queue_free(struct task_struct *thread);

void mt_biolog_mmcqd_req_check(void);
void mt_biolog_mmcqd_req_start(struct mmc_host *host);
void mt_biolog_mmcqd_req_end(struct mmc_data *data);

void mt_biolog_cmdq_check(void);
void mt_biolog_cmdq_queue_task(unsigned int task_id, struct mmc_request *req);
void mt_biolog_cmdq_dma_start(unsigned int task_id);
void mt_biolog_cmdq_dma_end(unsigned int task_id);
void mt_biolog_cmdq_isdone_start(unsigned int task_id, struct mmc_request *req);
void mt_biolog_cmdq_isdone_end(unsigned int task_id);

#define MMC_BIOLOG_PRINT_BUF 4096
#define MMC_BIOLOG_RINGBUF_MAX 120
#define MMC_BIOLOG_PIDLOG_ENTRIES 50
#define MMC_BIOLOG_CONTEXTS 10       /* number of request queues */
#define MMC_BIOLOG_CONTEXT_TASKS 32 /* number concurrent tasks in cmdq */

struct page_pid_logger {
	unsigned short pid1;
	unsigned short pid2;
};

#ifdef CONFIG_MTK_EXTMEM
extern void *extmem_malloc_page_align(size_t bytes);
#endif

struct mt_bio_workload {
	__u64 period;  /* period time (ns) */
	__u64 usage;   /* busy time (ns) */
	__u32 percent; /* workload */
	__u32 count;   /* access count */
};

struct mt_bio_throughput_rw {
	__u64 usage;  /* busy time (ns) */
	__u32 size;   /* transferred sectors */
	__u32 speed;  /* KB/s */
};

struct mt_bio_throughput {
	struct mt_bio_throughput_rw r;  /* read */
	struct mt_bio_throughput_rw w;  /* write */
};

struct mt_bio_vmstat {
	__u64 file_pages;
	__u64 file_dirty;
	__u64 dirtied;
	__u64 writeback;
	__u64 written;
};

struct mt_bio_pidlogger_entry_rw {
	__u16 count;
	__u32 length;
};

struct mt_bio_pidlogger_entry {
	__u16 pid;
	struct mt_bio_pidlogger_entry_rw r; /* read */
	struct mt_bio_pidlogger_entry_rw w; /* write */
};

struct mt_bio_pidlogger {
	__u16 current_pid;
	struct mt_bio_pidlogger_entry info[MMC_BIOLOG_PIDLOG_ENTRIES];
};

struct mt_bio_cpu {
	__u64 user;
	__u64 nice;
	__u64 system;
	__u64 idle;
	__u64 iowait;
	__u64 irq;
	__u64 softirq;
};

struct mt_bio_context_task {
	int task_id;
	u32 arg;
	uint64_t request_start_t;   /* request start time */
	uint64_t transfer_start_t;  /* mmcdqd: not used, cmdq: DMA start */
	uint64_t transfer_end_t;    /* mmcdqd: not used, cmdq: DMA end */
	uint64_t wait_start_t;      /* mmcdqd: not used, cmdq: isdone start */
};

/* Context of Request Queue */
struct mt_bio_context {
	int id;
	int state;
	pid_t pid;
	char comm[TASK_COMM_LEN+1];
	u32 qid;
	spinlock_t lock;
	uint64_t period_start_t;
	uint64_t period_end_t;
	uint64_t period_usage;
	struct mt_bio_context_task task[MMC_BIOLOG_CONTEXT_TASKS];
	struct mt_bio_workload workload;
	struct mt_bio_throughput throughput;
	struct mt_bio_pidlogger pidlog;
};

/* Entry of the ring buffer */
struct mt_bio_trace {
	uint64_t time;
	pid_t pid;
	u32 qid;
	struct mt_bio_workload workload;
	struct mt_bio_throughput throughput;
	struct mt_bio_vmstat vmstat;
	struct mt_bio_pidlogger pidlog;
	struct mt_bio_cpu cpu;
};

#else

#define mt_mmc_biolog_init(...)
#define mt_mmc_biolog_exit(...)

#define mt_bio_queue_alloc(...)
#define mt_bio_queue_free(...)

#define mt_biolog_mmcqd_req_check(...)
#define mt_biolog_mmcqd_req_start(...)
#define mt_biolog_mmcqd_req_end(...)

#define mt_biolog_cmdq_check(...)
#define mt_biolog_cmdq_queue_task(...)
#define mt_biolog_cmdq_dma_start(...)
#define mt_biolog_cmdq_dma_end(...)
#define mt_biolog_cmdq_isdone_start(...)
#define mt_biolog_cmdq_isdone_end(...)

#endif

#endif

