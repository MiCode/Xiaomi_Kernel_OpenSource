/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef _MTK_BLOCKTAG_H
#define _MTK_BLOCKTAG_H

#include <linux/blk_types.h>
#include <linux/mmc/core.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include "ufshcd.h"

#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_TRACER)

/*
 * MTK_BTAG_FEATURE_MICTX_IOSTAT
 *
 * Shall be defined if we can provide iostat
 * produced by mini context.
 *
 * This feature is used to extend kernel
 * trace events to have more I/O information.
 */
#define MTK_BTAG_FEATURE_MICTX_IOSTAT

#define BLOCKTAG_PIDLOG_ENTRIES 50
#define BLOCKTAG_NAME_LEN      16
#define BLOCKTAG_PRINT_LEN     4096

#define BTAG_RT(btag)     (btag ? &btag->rt : NULL)
#define BTAG_CTX(btag)    (btag ? btag->ctx.priv : NULL)

struct page_pid_logger {
	short pid;
};

#if IS_ENABLED(CONFIG_MTK_USE_RESERVED_EXT_MEM)
extern void *extmem_malloc_page_align(size_t bytes);
#endif

enum {
	PIDLOG_MODE_BLK_BIO_QUEUE,
	PIDLOG_MODE_MM_MARK_DIRTY
};

enum mtk_btag_storage_type {
	BTAG_STORAGE_UFS     = 0,
	BTAG_STORAGE_MMC     = 1,
	BTAG_STORAGE_UNKNOWN = 2
};

struct mtk_btag_mictx_id {
	enum mtk_btag_storage_type storage;
	int id;
};

struct mtk_btag_workload {
	__u64 period;  /* period time (ns) */
	__u64 usage;   /* busy time (ns) */
	__u32 percent; /* workload */
	__u32 count;   /* access count */
};

struct mtk_btag_throughput_rw {
	__u64 usage;  /* busy time (ns) */
	__u32 size;   /* transferred bytes */
	__u32 speed;  /* KB/s */
};

struct mtk_btag_throughput {
	struct mtk_btag_throughput_rw r;  /* read */
	struct mtk_btag_throughput_rw w;  /* write */
};

struct mtk_btag_req_rw {
	__u16 count;
	__u64 size; /* bytes */
	__u64 size_top; /* bytes */
};

struct mtk_btag_req {
	struct mtk_btag_req_rw r; /* read */
	struct mtk_btag_req_rw w; /* write */
};

/*
 * public structure to provide IO statistics
 * in a period of time.
 *
 * Make sure MTK_BTAG_FEATURE_MICTX_IOSTAT is
 * defined alone with mictx series.
 */
struct mtk_btag_mictx_iostat_struct {
	__u64 duration;  /* duration time for below performance data (ns) */
	__u32 tp_req_r;  /* throughput (per-request): read  (KB/s) */
	__u32 tp_req_w;  /* throughput (per-request): write (KB/s) */
	__u32 tp_all_r;  /* throughput (overlapped) : read  (KB/s) */
	__u32 tp_all_w;  /* throughput (overlapped) : write (KB/s) */
	__u32 reqsize_r; /* request size : read  (Bytes) */
	__u32 reqsize_w; /* request size : write (Bytes) */
	__u32 reqcnt_r;  /* request count: read */
	__u32 reqcnt_w;  /* request count: write */
	__u16 wl;	/* storage device workload (%) */
	__u16 top;       /* ratio of request (size) by top-app */
	__u16 q_depth;   /* storage cmdq queue depth */
};

/*
 * mini context for integration with
 * other performance analysis tools.
 */
struct mtk_btag_mictx_struct {
	int id;

	struct mtk_btag_throughput tp;
	struct mtk_btag_req req;
	__u64 window_begin;
	__u64 tp_min_time;
	__u64 tp_max_time;
	__u64 idle_begin;
	__u64 idle_total;
	__u64 weighted_qd;
	__u64 sum_of_start;

	__u16 q_depth;

	struct list_head list;
};

struct mtk_btag_earaio_control {
	spinlock_t lock;
	bool enabled;	/* can send boost to earaio */

	struct mtk_btag_mictx_id mictx_id;

	/* peeking window */
	__u64 pwd_begin;
	__u32 pwd_top_r_pages;
	__u32 pwd_top_w_pages;

	bool boosted;
	bool uevt_req;
	bool uevt_state;
	struct workqueue_struct *uevt_workq;
	struct work_struct uevt_work;
};

struct mtk_btag_vmstat {
	__u64 file_pages;
	__u64 file_dirty;
	__u64 dirtied;
	__u64 writeback;
	__u64 written;
	__u64 fmflt;
};

struct mtk_btag_pidlogger_entry_rw {
	__u16 count;
	__u32 length;
};

struct mtk_btag_pidlogger_entry {
	__u16 pid;
	char comm[TASK_COMM_LEN];
	struct mtk_btag_pidlogger_entry_rw r; /* read */
	struct mtk_btag_pidlogger_entry_rw w; /* write */
};

struct mtk_btag_pidlogger {
	__u16 current_pid;
	struct mtk_btag_pidlogger_entry info[BLOCKTAG_PIDLOG_ENTRIES];
};

struct mtk_btag_cpu {
	__u64 user;
	__u64 nice;
	__u64 system;
	__u64 idle;
	__u64 iowait;
	__u64 irq;
	__u64 softirq;
};

/* Trace: entry of the ring buffer */
struct mtk_btag_trace {
	uint64_t time;
	pid_t pid;
	u32 qid;
	struct mtk_btag_workload workload;
	struct mtk_btag_throughput throughput;
	struct mtk_btag_vmstat vmstat;
	struct mtk_btag_pidlogger pidlog;
	struct mtk_btag_cpu cpu;
};

/* Ring Trace */
struct mtk_btag_ringtrace {
	struct mtk_btag_trace *trace;
	spinlock_t lock;
	int index;
	int max;
};

struct mtk_btag_vops {
	size_t  (*seq_show)(char **buff, unsigned long *size,
			    struct seq_file *seq);
	__u16   (*mictx_eval_qd)(struct mtk_btag_mictx_struct *mictx,
				 u64 t_cur);
	bool	boot_device;
	bool	earaio_enabled;
};

/* BlockTag */
struct mtk_blocktag {
	char name[BLOCKTAG_NAME_LEN];
	enum mtk_btag_storage_type storage_type;
	struct mtk_btag_ringtrace rt;

	struct mictx_t {
		spinlock_t list_lock;
		int count;
		int last_unused_id;
		struct list_head list;
	} mictx;

	struct context_t {
		int count;
		int size;
		void *priv;
	} ctx;

	struct dentry_t {
		struct proc_dir_entry *droot;
		struct proc_dir_entry *dlog;
	} dentry;

	struct mtk_btag_vops *vops;

	struct list_head list;
};

struct mtk_blocktag *mtk_btag_find_by_type(
	enum mtk_btag_storage_type storage_type);

void mtk_btag_pidlog_insert(
	struct mtk_btag_pidlogger *pidlog, pid_t pid, __u32 len, int rw);
int mtk_btag_pidlog_add_ufs(
	struct request_queue *q, short pid, __u32 len, int rw);
int mtk_btag_pidlog_add_mmc(
	struct request_queue *q, short pid, __u32 len, int rw, bool is_sd);
void mtk_btag_commit_req(struct request *rq, bool is_sd);

void mtk_btag_vmstat_eval(struct mtk_btag_vmstat *vm);
void mtk_btag_pidlog_eval(
	struct mtk_btag_pidlogger *pl, struct mtk_btag_pidlogger *ctx_pl);
void mtk_btag_cpu_eval(struct mtk_btag_cpu *cpu);
void mtk_btag_throughput_eval(struct mtk_btag_throughput *tp);

struct mtk_btag_trace *mtk_btag_curr_trace(struct mtk_btag_ringtrace *rt);
struct mtk_btag_trace *mtk_btag_next_trace(struct mtk_btag_ringtrace *rt);

struct mtk_blocktag *mtk_btag_alloc(
	const char *name, enum mtk_btag_storage_type storage_type,
	unsigned int ringtrace_count, size_t ctx_size, unsigned int ctx_count,
	struct mtk_btag_vops *vops);
void mtk_btag_free(struct mtk_blocktag *btag);

void mtk_btag_get_aee_buffer(unsigned long *vaddr, unsigned long *size);

void mtk_btag_mictx_check_window(struct mtk_btag_mictx_id mictx_id);
void mtk_btag_mictx_eval_tp(
	struct mtk_blocktag *btag,
	unsigned int rw, __u64 usage, __u32 size);
void mtk_btag_mictx_eval_req(
	struct mtk_blocktag *btag,
	unsigned int rw, __u32 cnt, __u32 size, bool top);
void mtk_btag_mictx_accumulate_weight_qd(
	struct mtk_blocktag *btag, u64 t_begin, u64 t_cur);
void mtk_btag_mictx_update(struct mtk_blocktag *btag, __u32 q_depth,
			   __u64 sum_of_start);
int mtk_btag_mictx_get_data(
	struct mtk_btag_mictx_id mictx_id,
	struct mtk_btag_mictx_iostat_struct *iostat);
void mtk_btag_mictx_free_btag(struct mtk_blocktag *btag);
void mtk_btag_mictx_enable(struct mtk_btag_mictx_id *mictx_id, int enable);
void mtk_btag_mictx_init(struct mtk_blocktag *btag, struct mtk_btag_vops *vops);

void mtk_btag_earaio_init_mictx(
	struct mtk_btag_vops *vops,
	enum mtk_btag_storage_type storage_type,
	struct proc_dir_entry *btag_proc_root);
int mtk_btag_earaio_init(void);
void mtk_btag_earaio_boost(bool boost);
void mtk_btag_earaio_check_pwd(void);
void mtk_btag_earaio_update_pwd(unsigned int write, __u32 size);

int ufs_mtk_biolog_init(bool qos_allowed, bool boot_device);
int ufs_mtk_biolog_exit(void);
void ufs_mtk_biolog_send_command(
	unsigned int task_id, struct scsi_cmnd *cmd);
void ufs_mtk_biolog_transfer_req_compl(
	unsigned int taski_id, unsigned long req_mask);
void ufs_mtk_biolog_check(unsigned long req_mask);
void ufs_mtk_biolog_clk_gating(bool gated);

int mmc_mtk_biolog_init(struct mmc_host *mmc);
int mmc_mtk_biolog_exit(void);
void mmc_mtk_biolog_send_command(
	unsigned int task_id, struct mmc_request *mrq);
void mmc_mtk_biolog_transfer_req_compl(
	struct mmc_host *mmc, unsigned int task_id, unsigned long req_mask);
void mmc_mtk_biolog_check(struct mmc_host *mmc, unsigned long req_mask);

#else

#define mtk_btag_mictx_enable(...)
#define mtk_btag_mictx_get_data(...)

#define ufs_mtk_biolog_init(...)
#define ufs_mtk_biolog_exit(...)
#define ufs_mtk_biolog_send_command(...)
#define ufs_mtk_biolog_transfer_req_compl(...)
#define ufs_mtk_biolog_check(...)
#define ufs_mtk_biolog_clk_gating(...)

#define mmc_mtk_biolog_send_command(...)
#define mmc_mtk_biolog_transfer_req_compl(...)
#define mmc_mtk_biolog_init(...)
#define mmc_mtk_biolog_exit(...)
#define mmc_mtk_biolog_check(...)

#endif

#endif

