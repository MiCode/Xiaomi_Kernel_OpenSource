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
#include "ufs-mediatek.h"

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

struct page_pidlogger {
	__s16 pid;
};

struct mtk_btag_proc_pidlogger_entry_rw {
	__u16 count;
	__u32 length;
};

struct mtk_btag_proc_pidlogger_entry {
	__u16 pid;
	struct mtk_btag_proc_pidlogger_entry_rw r; /* read */
	struct mtk_btag_proc_pidlogger_entry_rw w; /* write */
};

struct mtk_btag_proc_pidlogger {
	struct mtk_btag_proc_pidlogger_entry info[BLOCKTAG_PIDLOG_ENTRIES];
};

struct tmp_proc_pidlogger_entry {
	__u16 pid;
	__u32 length;
};
struct tmp_proc_pidlogger {
	struct tmp_proc_pidlogger_entry info[BLOCKTAG_PIDLOG_ENTRIES];
};

struct mtk_btag_mictx_id {
	enum mtk_btag_storage_type storage;
	__s8 id;
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
struct mtk_btag_mictx_data {
	struct mtk_btag_throughput tp;
	struct mtk_btag_req req;
	__u64 window_begin;
	__u64 tp_min_time;
	__u64 tp_max_time;
	__u64 idle_begin;
	__u64 idle_total;
	__u64 weighted_qd;
	__u64 sum_of_inflight_start;
	__u16 q_depth;
	spinlock_t lock;
};

struct mtk_btag_mictx {
	struct list_head list;
	struct mtk_btag_mictx_data __rcu *data;
	__s8 id;
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
	__u64 time;
	__u32 qid;
	__s16 pid;
	struct mtk_btag_workload workload;
	struct mtk_btag_throughput throughput;
	struct mtk_btag_vmstat vmstat;
	struct mtk_btag_proc_pidlogger pidlog;
	struct mtk_btag_cpu cpu;
};

/* Ring Trace */
struct mtk_btag_ringtrace {
	struct mtk_btag_trace *trace;
	spinlock_t lock;
	__s32 index;
	__s32 max;
};

struct mtk_btag_vops {
	size_t  (*seq_show)(char **buff, unsigned long *size,
			    struct seq_file *seq);
	__u16   (*mictx_eval_wqd)(struct mtk_btag_mictx_data *data,
				  __u64 t_cur);
	bool	boot_device;
	bool	earaio_enabled;
};

/* BlockTag */
struct mtk_blocktag {
	struct list_head list;
	char name[BLOCKTAG_NAME_LEN];
	enum mtk_btag_storage_type storage_type;
	struct mtk_btag_ringtrace rt;

	struct context_t {
		void *priv;
		__u32 count;
		__u32 size;
		struct mictx_t {
			struct list_head list;
			spinlock_t list_lock;
			__u16 nr_list;
			__u16 last_unused_id;
		} mictx;
	} ctx;

	struct dentry_t {
		struct proc_dir_entry *droot;
		struct proc_dir_entry *dlog;
	} dentry;

	struct mtk_btag_vops *vops;
};

struct mtk_blocktag *mtk_btag_find_by_type(
	enum mtk_btag_storage_type storage_type);

void mtk_btag_pidlog_insert(struct mtk_btag_proc_pidlogger *pidlog, bool write,
				struct tmp_proc_pidlogger *tmplog);
int mtk_btag_pidlog_add_ufs(__u16 task_id, bool write, __u32 total_len,
			    __u32 top_len, struct tmp_proc_pidlogger *pidlog);
int mtk_btag_pidlog_add_mmc(bool is_sd, bool write, __u32 total_len,
			    __u32 top_len, struct tmp_proc_pidlogger *pidlog);
void mtk_btag_commit_req(__u16 task_id, struct request *rq, bool is_sd);

void mtk_btag_vmstat_eval(struct mtk_btag_vmstat *vm);
void mtk_btag_pidlog_eval(
	struct mtk_btag_proc_pidlogger *pl,
	struct mtk_btag_proc_pidlogger *ctx_pl);
void mtk_btag_cpu_eval(struct mtk_btag_cpu *cpu);
void mtk_btag_throughput_eval(struct mtk_btag_throughput *tp);

struct mtk_btag_trace *mtk_btag_curr_trace(struct mtk_btag_ringtrace *rt);
struct mtk_btag_trace *mtk_btag_next_trace(struct mtk_btag_ringtrace *rt);

struct mtk_blocktag *mtk_btag_alloc(
	const char *name, enum mtk_btag_storage_type storage_type,
	__u32 ringtrace_count, size_t ctx_size, __u32 ctx_count,
	struct mtk_btag_vops *vops);
void mtk_btag_free(struct mtk_blocktag *btag);

void mtk_btag_get_aee_buffer(unsigned long *vaddr, unsigned long *size);

void mtk_btag_mictx_check_window(struct mtk_btag_mictx_id mictx_id);
void mtk_btag_mictx_eval_tp(struct mtk_blocktag *btag, __u32 idx, bool write,
			    __u64 usage, __u32 size);
void mtk_btag_mictx_eval_req(struct mtk_blocktag *btag, __u32 idx, bool write,
				__u32 total_len, __u32 top_len);
void mtk_btag_mictx_accumulate_weight_qd(struct mtk_blocktag *btag, __u32 idx,
					 __u64 t_begin, __u64 t_cur);
void mtk_btag_mictx_update(struct mtk_blocktag *btag, __u32 idx, __u32 q_depth,
			   __u64 sum_of_start);
int mtk_btag_mictx_get_data(
	struct mtk_btag_mictx_id mictx_id,
	struct mtk_btag_mictx_iostat_struct *iostat);
void mtk_btag_mictx_free_all(struct mtk_blocktag *btag);
void mtk_btag_mictx_enable(struct mtk_btag_mictx_id *mictx_id, bool enable);
void mtk_btag_mictx_init(struct mtk_blocktag *btag);

void mtk_btag_earaio_init_mictx(
	struct mtk_btag_vops *vops,
	enum mtk_btag_storage_type storage_type,
	struct proc_dir_entry *btag_proc_root);
int mtk_btag_earaio_init(void);
void mtk_btag_earaio_boost(bool boost);
void mtk_btag_earaio_check_pwd(void);
void mtk_btag_earaio_update_pwd(bool write, __u32 size);

int mtk_btag_ufs_init(struct ufs_mtk_host *host);
int mtk_btag_ufs_exit(void);
void mtk_btag_ufs_send_command(__u16 task_id, struct scsi_cmnd *cmd);
void mtk_btag_ufs_transfer_req_compl(__u16 task_id, unsigned long req_mask);
void mtk_btag_ufs_check(__u16 task_id, unsigned long req_mask);
void mtk_btag_ufs_clk_gating(bool gated);

int mmc_mtk_biolog_init(struct mmc_host *mmc);
int mmc_mtk_biolog_exit(void);
void mmc_mtk_biolog_send_command(__u16 task_id, struct mmc_request *mrq);
void mmc_mtk_biolog_transfer_req_compl(struct mmc_host *mmc,
				       __u16 task_id,
				       unsigned long req_mask);
void mmc_mtk_biolog_check(struct mmc_host *mmc, unsigned long req_mask);

/* seq file operations */
void *mtk_btag_seq_debug_start(struct seq_file *seq, loff_t *pos);
void *mtk_btag_seq_debug_next(struct seq_file *seq, void *v, loff_t *pos);
void mtk_btag_seq_debug_stop(struct seq_file *seq, void *v);

#else

#define mtk_btag_mictx_enable(...)
#define mtk_btag_mictx_get_data(...)

#define mtk_btag_ufs_init(...)
#define mtk_btag_ufs_exit(...)
#define mtk_btag_ufs_send_command(...)
#define mtk_btag_ufs_transfer_req_compl(...)
#define mtk_btag_ufs_check(...)
#define mtk_btag_ufs_clk_gating(...)

#define mmc_mtk_biolog_send_command(...)
#define mmc_mtk_biolog_transfer_req_compl(...)
#define mmc_mtk_biolog_init(...)
#define mmc_mtk_biolog_exit(...)
#define mmc_mtk_biolog_check(...)

#endif

#endif

