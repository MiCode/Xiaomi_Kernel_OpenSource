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
	unsigned short pid;
};

#ifdef CONFIG_MTK_USE_RESERVED_EXT_MEM
extern void *extmem_malloc_page_align(size_t bytes);
#endif

enum {
	PIDLOG_MODE_BLK_SUBMIT_BIO = 0,
	PIDLOG_MODE_MM_FS
};

enum mtk_btag_storage_type {
	BTAG_STORAGE_EMBEDDED = 0,
	BTAG_STORAGE_EXTERNAL
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
	__u32 size; /* bytes */
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
	__u64 idle_total;
	__u32 q_depth;
	spinlock_t lock;
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

typedef size_t (*mtk_btag_seq_f) (char **, unsigned long *, struct seq_file *);

/* BlockTag */
struct mtk_blocktag {
	char name[BLOCKTAG_NAME_LEN];
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

	mtk_btag_seq_f seq_show;

	unsigned int klog_enable;
	unsigned int used_mem;

	struct list_head list;
};

struct mtk_blocktag *mtk_btag_alloc(const char *name,
	unsigned int ringtrace_count, size_t ctx_size, unsigned int ctx_count,
	mtk_btag_seq_f seq_show);
void mtk_btag_free(struct mtk_blocktag *btag);

struct mtk_btag_trace *mtk_btag_curr_trace(struct mtk_btag_ringtrace *rt);
struct mtk_btag_trace *mtk_btag_next_trace(struct mtk_btag_ringtrace *rt);

int mtk_btag_pidlog_add_mmc(struct request_queue *q, pid_t pid, __u32 len,
	int rw);
int mtk_btag_pidlog_add_ufs(struct request_queue *q, pid_t pid, __u32 len,
	int rw);
void mtk_btag_pidlog_insert(struct mtk_btag_pidlogger *pidlog, pid_t pid,
__u32 len, int rw);
void mtk_mq_btag_pidlog_insert(struct mtk_btag_pidlogger *pidlog, pid_t pid,
	__u32 len, int write, bool ext_sd);
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
void mtk_btag_pidlog_submit_bio(struct bio *bio);
void mtk_btag_pidlog_set_pid(struct page *p);

void mtk_btag_mictx_enable(int enable);
void mtk_btag_mictx_eval_tp(
	unsigned int rw, __u64 usage, __u32 size);
void mtk_btag_mictx_eval_req(
	unsigned int rw, __u32 cnt, __u32 size);
int mtk_btag_mictx_get_data(
	struct mtk_btag_mictx_iostat_struct *iostat);
void mtk_btag_mictx_update_ctx(__u32 q_depth);

#else

#define mtk_btag_pidlog_copy_pid(...)
#define mtk_btag_pidlog_map_sg(...)
#define mtk_btag_pidlog_submit_bio(...)
#define mtk_btag_pidlog_set_pid(...)

#define mtk_btag_mictx_enable(...)
#define mtk_btag_mictx_eval_tp(...)
#define mtk_btag_mictx_eval_req(...)
#define mtk_btag_mictx_get_data(...)
#define mtk_btag_mictx_update_ctx(...)

#endif

#endif

