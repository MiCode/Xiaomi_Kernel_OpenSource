/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef _MTK_BLOCKTAG_H
#define _MTK_BLOCKTAG_H

#include <linux/blk_types.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>

#if defined(CONFIG_MTK_BLOCK_TAG)

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
#define BTAG_KLOGEN(btag) (btag ? btag->klog_enable : 0)

struct page_pid_logger {
	short pid;
	short mode;
};

#ifdef CONFIG_MTK_USE_RESERVED_EXT_MEM
extern void *extmem_malloc_page_align(size_t bytes);
#endif

enum {
	PIDLOG_MODE_BLK_RQ_INSERT   = 0,
	PIDLOG_MODE_FS_FUSE         = 1,
	PIDLOG_MODE_FS_WRITE_BEGIN  = 2,
	PIDLOG_MODE_MM_MARK_DIRTY   = 3
};

enum {
	BTAG_STORAGE_EMBEDDED = 0,
	BTAG_STORAGE_EXTERNAL
};

enum {
	BTAG_STORAGE_UFS     = 0,
	BTAG_STORAGE_MMC     = 1,
	BTAG_STORAGE_UNKNOWN = 2
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
	__u16 wl;        /* storage device workload (%) */
	__u16 top;       /* ratio of request (size) by top-app */
	__u16 q_depth;   /* storage cmdq queue depth */
};

/*
 * mini context for integration with
 * other performance analysis tools.
 */
struct mtk_btag_mictx_struct {
	struct mtk_btag_throughput tp;
	struct mtk_btag_req req;
	__u64 window_begin;
	__u64 tp_min_time;
	__u64 tp_max_time;
	__u64 idle_begin;
	__u64 idle_begin_top;
	__u64 idle_total;
	__u64 idle_total_top;
	__u64 weighted_qd;
	__u32 top_r_pages;
	__u32 top_w_pages;
	__u16 q_depth;
	__u16 q_depth_top;
	spinlock_t lock;
	bool enabled;
	bool boosted;
	bool earaio_enabled;
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
	void    (*mictx_eval_wqd)(struct mtk_btag_mictx_struct *mctx,
				  u64 t_cur);
	bool	earaio_enabled;
};

/* BlockTag */
struct mtk_blocktag {
	char name[BLOCKTAG_NAME_LEN];
	struct mtk_btag_mictx_struct mictx;
	struct mtk_btag_ringtrace rt;

	struct prbuf_t {
		spinlock_t lock;
		char buf[BLOCKTAG_PRINT_LEN];
	} prbuf;

	/* lock order: ctx.priv->lock => prbuf.lock */
	struct context_t {
		int count;
		int size;
		void *priv;
	} ctx;

	struct dentry_t {
		struct proc_dir_entry *droot;
		struct proc_dir_entry *dlog;
		struct proc_dir_entry *dlog_mictx;
	} dentry;

	struct mtk_btag_vops *vops;

	unsigned int klog_enable;
	unsigned int used_mem;

	struct list_head list;
};

struct mtk_blocktag *mtk_btag_alloc(const char *name,
	unsigned int ringtrace_count, size_t ctx_size, unsigned int ctx_count,
	struct mtk_btag_vops *vops);
void mtk_btag_free(struct mtk_blocktag *btag);
#if IS_ENABLED(CONFIG_SCHED_TUNE)
void mtk_btag_earaio_boost(bool boost);
#else
#define mtk_btag_earaio_boost(...)
#endif

struct mtk_btag_trace *mtk_btag_curr_trace(struct mtk_btag_ringtrace *rt);
struct mtk_btag_trace *mtk_btag_next_trace(struct mtk_btag_ringtrace *rt);

#ifdef CONFIG_MMC_BLOCK_IO_LOG
int mtk_btag_pidlog_add_mmc(struct request_queue *q, pid_t pid, __u32 len,
	int rw);
#else
#define mtk_btag_pidlog_add_mmc(...)
#endif
#ifdef CONFIG_MTK_UFS_BLOCK_IO_LOG
int mtk_btag_pidlog_add_ufs(struct request_queue *q, short pid, __u32 len,
	int rw);
#else
#define mtk_btag_pidlog_add_ufs(...)
#endif
void mtk_btag_pidlog_insert(struct mtk_btag_pidlogger *pidlog, pid_t pid,
__u32 len, int rw);

void mtk_btag_cpu_eval(struct mtk_btag_cpu *cpu);
void mtk_btag_pidlog_eval(struct mtk_btag_pidlogger *pl,
	struct mtk_btag_pidlogger *ctx_pl);
void mtk_btag_throughput_eval(struct mtk_btag_throughput *tp);
void mtk_btag_vmstat_eval(struct mtk_btag_vmstat *vm);

void mtk_btag_task_timetag(char *buf, unsigned int len, unsigned int stage,
	unsigned int max, const char *name[], uint64_t *t, __u32 bytes);
void mtk_btag_klog(struct mtk_blocktag *btag, struct mtk_btag_trace *tr);

void mtk_btag_pidlog_map_sg(struct request_queue *q, struct bio *bio,
	struct bio_vec *bvec);
void mtk_btag_pidlog_copy_pid(struct page *src, struct page *dst);
int mtk_btag_pidlog_get_mode(struct page *p);
void mtk_btag_pidlog_submit_bio(struct bio *bio);
void mtk_btag_pidlog_set_pid(struct page *p, int mode, bool write);
void mtk_btag_pidlog_set_pid_pages(struct page **page, int page_cnt,
				   int mode, bool write);


void mtk_btag_mictx_enable(int enable);
void mtk_btag_mictx_eval_tp(
	struct mtk_blocktag *btag,
	unsigned int rw, __u64 usage, __u32 size);
void mtk_btag_mictx_eval_req(
	struct mtk_blocktag *btag,
	unsigned int rw, __u32 cnt, __u32 size, bool top);
int mtk_btag_mictx_get_data(
	struct mtk_btag_mictx_iostat_struct *iostat);
void mtk_btag_mictx_update_ctx(struct mtk_blocktag *btag, __u32 q_depth);

#else

#define mtk_btag_pidlog_copy_pid(...)
#define mtk_btag_pidlog_map_sg(...)
#define mtk_btag_pidlog_submit_bio(...)
#define mtk_btag_pidlog_set_pid(...)
#define mtk_btag_pidlog_set_pid_pages(...)

#define mtk_btag_mictx_enable(...)
#define mtk_btag_mictx_eval_tp(...)
#define mtk_btag_mictx_eval_req(...)
#define mtk_btag_mictx_get_data(...)
#define mtk_btag_mictx_update_ctx(...)

#endif

#endif

