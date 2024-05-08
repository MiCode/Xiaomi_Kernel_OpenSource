/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 Xiaomi Inc.
 */

#ifndef _MPBE_H
#define _MPBE_H

#include <linux/blk_types.h>
#include <linux/mmc/core.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <scsi/scsi_cmnd.h>
#include <linux/sched/clock.h>
//#include "mi_ufshcd.h"
#include "ufs-qcom.h"

#if IS_ENABLED(CONFIG_MI_MPBE_SUPPORT)

//#define ENABLE_DEBUG
#ifdef ENABLE_DEBUG
#define MPBE_DBG(format,...)  pr_info("[CTY] %s: "format, __func__,##__VA_ARGS__)
#else
#define MPBE_DBG(format,...) 
#endif
/*
 * MPBE_FEATURE_MICTX_IOSTAT
 *
 * Shall be defined if we can provide iostat
 * produced by mini context.
 *
 * This feature is used to extend kernel
 * trace events to have more I/O information.
 */
#define MPBE_FEATURE_MICTX_IOSTAT

#define MPBE_PIDLOG_ENTRIES 50
#define MPBE_NAME_LEN      16
#define MPBE_PRINT_LEN     4096

#define MPBE_RT(btag)     (btag ? &btag->rt : NULL)
#define MPBE_CTX(btag)    (btag ? btag->ctx.priv : NULL)

enum {
	PIDLOG_MODE_BLK_BIO_QUEUE,
	PIDLOG_MODE_MM_MARK_DIRTY
};

enum mpbe_storage_type {
	MPBE_STORAGE_UFS     = 0,
	MPBE_STORAGE_MMC     = 1,
	MPBE_STORAGE_UNKNOWN = 2
};

struct mpbe_mictx_id {
	enum mpbe_storage_type storage;
	__s8 id;
};

struct mpbe_workload {
	__u64 period;  /* period time (ns) */
	__u64 usage;   /* busy time (ns) */
	__u32 percent; /* workload */
	__u32 count;   /* access count */
};

struct mpbe_throughput_rw {
	__u64 usage;  /* busy time (ns) */
	__u32 size;   /* transferred bytes */
	__u32 speed;  /* KB/s */
};

struct mpbe_throughput {
	struct mpbe_throughput_rw r;  /* read */
	struct mpbe_throughput_rw w;  /* write */
};

struct mpbe_req_rw {
	__u16 count;
	__u64 size; /* bytes */
	__u64 size_top; /* bytes */
};

struct mpbe_req {
	struct mpbe_req_rw r; /* read */
	struct mpbe_req_rw w; /* write */
};

/*
 * public structure to provide IO statistics
 * in a period of time.
 *
 * Make sure MPBE_FEATURE_MICTX_IOSTAT is
 * defined alone with mictx series.
 */
struct mpbe_mictx_iostat_struct {
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
struct mpbe_mictx_data {
	struct mpbe_throughput tp;
	struct mpbe_req req;
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

struct mpbe_mictx {
	struct list_head list;
	struct mpbe_mictx_data __rcu *data;
	__s8 id;
};

struct mpbe_earaio_control {
	spinlock_t lock;
	bool enabled;	/* can send boost to earaio */

	struct mpbe_mictx_id mictx_id;

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

struct mpbe_vmstat {
	__u64 file_pages;
	__u64 file_dirty;
	__u64 dirtied;
	__u64 writeback;
	__u64 written;
	__u64 fmflt;
};

struct mpbe_cpu {
	__u64 user;
	__u64 nice;
	__u64 system;
	__u64 idle;
	__u64 iowait;
	__u64 irq;
	__u64 softirq;
};

/* Trace: entry of the ring buffer */
#define MPBE_TR_READY		(1 << 0)
#define MPBE_TR_NOCLEAR		(1 << 1)

struct mpbe_trace {
	__u64 time;
	__u32 qid;
	__s16 pid;
	__u8 flags;
	struct mpbe_workload workload;
	struct mpbe_throughput throughput;
	struct mpbe_vmstat vmstat;
	struct mpbe_cpu cpu;
};

/* Ring Trace */
struct mpbe_ringtrace {
	struct mpbe_trace *trace;
	spinlock_t lock;
	__s32 index;
	__s32 max;
};

struct mpbe_vops {
	size_t  (*seq_show)(char **buff, unsigned long *size,
			    struct seq_file *seq);
	__u16   (*mictx_eval_wqd)(struct mpbe_mictx_data *data,
				  __u64 t_cur);
	bool	boot_device;
	bool	earaio_enabled;
};

/* MPBE */
struct mpbe {
	struct list_head list;
	char name[MPBE_NAME_LEN];
	enum mpbe_storage_type storage_type;
	struct mpbe_ringtrace rt;

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

	struct mpbe_vops *vops;
};

struct mpbe *mpbe_find_by_type(
	enum mpbe_storage_type storage_type);

int mpbe_trace_add_ufs(__u16 task_id, bool write, __u32 total_len,
			    __u32 top_len);
void mpbe_commit_req(__u16 task_id, struct request *rq, bool is_sd);
void mpbe_vmstat_eval(struct mpbe_vmstat *vm);
void mpbe_cpu_eval(struct mpbe_cpu *cpu);
void mpbe_throughput_eval(struct mpbe_throughput *tp);

struct mpbe_trace *mpbe_curr_trace(struct mpbe_ringtrace *rt);
struct mpbe_trace *mpbe_next_trace(struct mpbe_ringtrace *rt);

struct mpbe *mpbe_alloc(
	const char *name, enum mpbe_storage_type storage_type,
	__u32 ringtrace_count, size_t ctx_size, __u32 ctx_count,
	struct mpbe_vops *vops);
void mpbe_free(struct mpbe *btag);

void mpbe_mictx_check_window(struct mpbe_mictx_id mictx_id);
void mpbe_mictx_eval_tp(struct mpbe *btag, __u32 idx, bool write,
			    __u64 usage, __u32 size);
void mpbe_mictx_eval_req(struct mpbe *btag, __u32 idx, bool write,
				__u32 total_len, __u32 top_len);
void mpbe_mictx_accumulate_weight_qd(struct mpbe *btag, __u32 idx,
					 __u64 t_begin, __u64 t_cur);
void mpbe_mictx_update(struct mpbe *btag, __u32 idx, __u32 q_depth,
			   __u64 sum_of_start, int cmd_rsp);
int mpbe_mictx_get_data(
	struct mpbe_mictx_id mictx_id,
	struct mpbe_mictx_iostat_struct *iostat);
void mpbe_mictx_free_all(struct mpbe *btag);
void mpbe_mictx_enable(struct mpbe_mictx_id *mictx_id, bool enable);
void mpbe_mictx_init(struct mpbe *btag);

void mpbe_earaio_init_mictx(
	struct mpbe_vops *vops,
	enum mpbe_storage_type storage_type,
	struct proc_dir_entry *btag_proc_root);
int mpbe_earaio_init(void);
void mpbe_earaio_boost(bool boost);
void mpbe_earaio_check_pwd(void);
void mpbe_earaio_update_pwd(bool write, __u32 size);
bool mpbe_earaio_enabled(void);

int mpbe_ufs_init(void);
int mpbe_ufs_exit(void);
int mpbe_trace_ufs_cmd(struct ufs_hba *hba, struct ufshcd_lrb *lrbp, bool is_send);
void mpbe_ufs_rq_check(__u16 task_id, unsigned long req_mask);
void mpbe_ufs_clk_gating(bool gated);

/* seq file operations */
void *mpbe_seq_debug_start(struct seq_file *seq, loff_t *pos);
void *mpbe_seq_debug_next(struct seq_file *seq, void *v, loff_t *pos);
void mpbe_seq_debug_stop(struct seq_file *seq, void *v);

#else
#define mpbe_mictx_enable(...)
#define mpbe_mictx_get_data(...)

#define mpbe_ufs_init(...)
#define mpbe_ufs_exit(...)
#define mpbe_ufs_transfer_req_send(...)
#define mpbe_ufs_transfer_req_compl(...)
#define mpbe_ufs_rq_check(...)
#define mpbe_ufs_clk_gating(...)

#endif

#endif
