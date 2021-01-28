// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#define DEBUG 1

#include <linux/debugfs.h>
#include <linux/blkdev.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/tick.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>

#include <linux/vmalloc.h>
#include <linux/blk_types.h>
#include <linux/mmc/core.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/module.h>

#ifdef CONFIG_MTK_USE_RESERVED_EXT_MEM
#include <linux/exm_driver.h>
#endif

#include "queue.h"
#include "mtk_mmc_block.h"
#include <mt-plat/mtk_blocktag.h>

#define SECTOR_SHIFT 9

/* ring trace for debugfs */
struct mtk_blocktag *mtk_btag_mmc;

/* context buffer of each request queue */
struct mt_bio_context *mt_ctx_map[MMC_BIOLOG_CONTEXTS] = { 0 };

/* context index for mt_ctx_map */
enum {
	CTX_MMCQD0 = 0,
	CTX_MMCQD1 = 1,
	CTX_MMCQD0_BOOT0 = 2,
	CTX_MMCQD0_BOOT1 = 3,
	CTX_MMCQD0_RPMB  = 4,
	CTX_MMCCMDQD0 = 5,
	CTX_EXECQ  = 9
};

/* context state for command queue */
enum {
	CMDQ_CTX_NOT_DMA = 0,
	CMDQ_CTX_IN_DMA = 1,
	CMDQ_CTX_QUEUE = 2
};

/* context state for mmcqd */
enum {
	MMCQD_NORMAL = 0,
	MMCQD_CMDQ_MODE_EN = 1
};

#define MT_BIO_TRACE_LATENCY (unsigned long long)(1000000000)

#define REQ_EXECQ  "exe_cq"
#define REQ_MMCQD0 "kworker"
#define REQ_MMCQD0_BOOT0 "mmcqd/0boot0"
#define REQ_MMCQD0_BOOT1 "mmcqd/0boot1"
#define REQ_MMCQD0_RPMB  "mmcqd/0rpmb"
#define REQ_MMCCMDQD0 "mmc-cmdqd/0"
#define REQ_MMCQD1 "kworker"

static void mt_bio_ctx_count_usage(struct mt_bio_context *ctx,
	__u64 start, __u64 end);
static uint64_t mt_bio_get_period_busy(struct mt_bio_context *ctx);

/* queue id:
 * 0=internal storage (emmc:mmcqd0/exe_cq),
 * 1=external storage (t-card:mmcqd1)
 */
static int get_qid_by_name(const char *str)
{
	if (strncmp(str, REQ_EXECQ, strlen(REQ_EXECQ)) == 0)
		return BTAG_STORAGE_EMBEDDED;
	if (strncmp(str, REQ_MMCQD0, strlen(REQ_MMCQD0)) == 0)
		return BTAG_STORAGE_EMBEDDED;  /* this includes boot0, boot1 */
	if (strncmp(str, REQ_MMCCMDQD0, strlen(REQ_MMCCMDQD0)) == 0)
		return BTAG_STORAGE_EMBEDDED;
	if (strncmp(str, REQ_MMCQD1, strlen(REQ_MMCQD1)) == 0)
		return BTAG_STORAGE_EXTERNAL;
	return 99;
}

/* get context id to mt_ctx_map[] by name */
static int get_ctxid_by_name(const char *str)
{
	if (strncmp(str, REQ_EXECQ, strlen(REQ_EXECQ)) == 0)
		return CTX_EXECQ;
	if (strncmp(str, REQ_MMCQD0_RPMB, strlen(REQ_MMCQD0_RPMB)) == 0)
		return CTX_MMCQD0_RPMB;
	if (strncmp(str, REQ_MMCQD0_BOOT0, strlen(REQ_MMCQD0_BOOT0)) == 0)
		return CTX_MMCQD0_BOOT0;
	if (strncmp(str, REQ_MMCQD0_BOOT1, strlen(REQ_MMCQD0_BOOT1)) == 0)
		return CTX_MMCQD0_BOOT1;
	if (strncmp(str, REQ_MMCCMDQD0, strlen(REQ_MMCCMDQD0)) == 0)
		return CTX_MMCCMDQD0;
	if (strncmp(str, REQ_MMCQD0, strlen(REQ_MMCQD0)) == 0)
		return CTX_MMCQD0;
	if (strncmp(str, REQ_MMCQD1, strlen(REQ_MMCQD1)) == 0)
		return CTX_MMCQD1;
	return -1;
}

static void mt_bio_init_task(struct mt_bio_context_task *tsk)
{
	int i;

	tsk->task_id = -1;
	tsk->arg = 0;
	for (i = 0; i < tsk_max; i++)
		tsk->t[i] = 0;
}

static void mt_bio_init_ctx(struct mt_bio_context *ctx,
	struct task_struct *thread, struct request_queue *q,
	bool ext_sd)
{
	int i;

	ctx->q = q;
	ctx->pid = task_pid_nr(thread);
	get_task_comm(ctx->comm, thread);
	ctx->qid = get_qid_by_name(ctx->comm);
	if (ext_sd)
		ctx->qid = BTAG_STORAGE_EXTERNAL;
	spin_lock_init(&ctx->lock);
	ctx->id = get_ctxid_by_name(ctx->comm);
	if (ext_sd)
		ctx->id = CTX_MMCQD1;
	if (ctx->id >= 0)
		mt_ctx_map[ctx->id] = ctx;
	ctx->period_start_t = sched_clock();

	for (i = 0; i < MMC_BIOLOG_CONTEXT_TASKS; i++)
		mt_bio_init_task(&ctx->task[i]);
}

void mt_bio_queue_alloc(struct task_struct *thread, struct request_queue *q,
	bool ext_sd)
{
	int i;
	pid_t pid;
	static bool ext_sd_setup_done;
	struct mt_bio_context *ctx = BTAG_CTX(mtk_btag_mmc);

	if (!ctx)
		return;

	pid = task_pid_nr(thread);

	for (i = 0; i < MMC_BIOLOG_CONTEXTS; i++)	{
		if (ctx[i].pid == pid)
			break;
		/* bypass more SD wokers */
		if (ext_sd_setup_done)
			break;
		/* bypass more emmc wokers */
		#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
		/* SWCQ: 2 thread - exe_cq and kworker */
		if ((i > 1) && !ext_sd)
			break;
		#else
		/* CQHCI: only one thread - kworker */
		if ((i > 0) && !ext_sd)
			break;
		#endif
		if (ctx[i].pid == 0) {
			if (ext_sd) {
				ext_sd_setup_done = true;
				mt_bio_init_ctx(&ctx[i], thread, q, true);
			} else
				mt_bio_init_ctx(&ctx[i], thread, q, false);
			break;
		}
	}
}

void mt_bio_queue_free(struct task_struct *thread)
{
	int i;
	pid_t pid;
	struct mt_bio_context *ctx = BTAG_CTX(mtk_btag_mmc);

	if (!ctx)
		return;

	pid = task_pid_nr(thread);

	for (i = 0; i < MMC_BIOLOG_CONTEXTS; i++)	{
		if (ctx[i].pid == pid) {
			mt_ctx_map[ctx[i].id] = NULL;
			memset(&ctx[i], 0, sizeof(struct mt_bio_context));
			break;
		}
	}
}

static struct mt_bio_context *mt_bio_curr_queue(struct request_queue *q,
	bool ext_sd)
{
	int i;
	pid_t qd_pid;
	struct mt_bio_context *ctx = BTAG_CTX(mtk_btag_mmc);

	if (!ctx)
		return NULL;

	qd_pid = task_pid_nr(current);

	for (i = 0; i < MMC_BIOLOG_CONTEXTS; i++)	{
		if (ctx[i].pid == 0)
			continue;
		if (!ext_sd && (!strncmp(ctx[i].comm, REQ_MMCQD0, strlen(REQ_MMCQD0)) ||
		(qd_pid == ctx[i].pid) || (ctx[i].q && ctx[i].q == q)))
			return &ctx[i];
		/* It means hardcore ctx[3] as SD card, it's not elegant */
		else if (ext_sd && (!strncmp(ctx[i+2].comm, REQ_MMCQD0, strlen(REQ_MMCQD0)) ||
		(qd_pid == ctx[i+2].pid) || (ctx[i+2].q && ctx[i+2].q == q)))
			return &ctx[i+2];
	}
	return NULL;
}

/* get context correspond to current process */
static inline struct mt_bio_context *mt_bio_curr_ctx(bool ext_sd)
{
	return mt_bio_curr_queue(NULL, ext_sd);
}

/* get other queue's context by context id */
static struct mt_bio_context *mt_bio_get_ctx(int id)
{
	if (id < 0 || id >= MMC_BIOLOG_CONTEXTS)
		return NULL;

	return mt_ctx_map[id];
}

/* append a pidlog to given context */
int mtk_btag_pidlog_add_mmc(struct request_queue *q, pid_t pid, __u32 len,
	int write)
{
	unsigned long flags;
	struct mt_bio_context *ctx;
	struct mmc_queue *mq;
	struct mmc_host *host;

	if (q && q->queuedata) {
		mq = (struct mmc_queue *)(q->queuedata);
		host = mq->card->host;
	}

	if (!host)
		return 0;

	if (host->caps2 & MMC_CAP2_NO_SD) {
		ctx = mt_bio_curr_queue(q, false);
		if (!ctx)
			return 0;
		spin_lock_irqsave(&ctx->lock, flags);
		mtk_mq_btag_pidlog_insert(&ctx->pidlog, pid, len, write, false);
	} else if (host->caps2 & MMC_CAP2_NO_MMC) {
		ctx = mt_bio_curr_queue(q, true);
		if (!ctx)
			return 0;
		spin_lock_irqsave(&ctx->lock, flags);
		mtk_mq_btag_pidlog_insert(&ctx->pidlog, pid, len, write, true);
	} else
		spin_lock_irqsave(&ctx->lock, flags);

	if (ctx->qid == BTAG_STORAGE_EMBEDDED)
		mtk_btag_mictx_eval_req(write, 1, len);

	spin_unlock_irqrestore(&ctx->lock, flags);

	return 1;
}
EXPORT_SYMBOL_GPL(mtk_btag_pidlog_add_mmc);

/* evaluate throughput and workload of given context */
static void mt_bio_context_eval(struct mt_bio_context *ctx)
{
	struct mt_bio_context_task *tsk;
	uint64_t min, period, tsk_start;
	int i;

	min = ctx->period_end_t;

	/* for all tasks if there is an on-going request */
	for (i = 0; i < MMC_BIOLOG_CONTEXT_TASKS; i++) {
		tsk = &ctx->task[i];
		if (tsk->task_id >= 0) {
			tsk_start = tsk->t[tsk_req_start];
			if (tsk_start &&
				tsk_start >= ctx->period_start_t &&
				tsk_start < min)
				min = tsk_start;
		}
	}

	mt_bio_ctx_count_usage(ctx, min, ctx->period_end_t);
	ctx->workload.usage = mt_bio_get_period_busy(ctx);

	if (ctx->workload.period > (ctx->workload.usage * 100)) {
		ctx->workload.percent = 1;
	} else {
		period = ctx->workload.period;
		ctx->workload.percent =
		((__u32)ctx->workload.usage * 100) / (__u32)period;
	}

	mtk_btag_throughput_eval(&ctx->throughput);
}

/* print context to trace ring buffer */
static void mt_bio_print_trace(struct mt_bio_context *ctx)
{
	struct mtk_btag_ringtrace *rt = BTAG_RT(mtk_btag_mmc);
	struct mtk_btag_trace *tr;
	struct mt_bio_context *pid_ctx = ctx;
	unsigned long flags;

	if (!rt)
		return;

	if (ctx->id == CTX_EXECQ)
		pid_ctx = mt_bio_get_ctx(CTX_MMCQD0);

	spin_lock_irqsave(&rt->lock, flags);
	tr = mtk_btag_curr_trace(rt);

	if (!tr)
		goto out;

	memset(tr, 0, sizeof(struct mtk_btag_trace));
	tr->pid = ctx->pid;
	tr->qid = ctx->qid;
	memcpy(&tr->throughput, &ctx->throughput,
		sizeof(struct mtk_btag_throughput));
	memcpy(&tr->workload, &ctx->workload, sizeof(struct mtk_btag_workload));

	if (pid_ctx)
		mtk_btag_pidlog_eval(&tr->pidlog, &pid_ctx->pidlog);

	mtk_btag_vmstat_eval(&tr->vmstat);
	mtk_btag_cpu_eval(&tr->cpu);

	tr->time = sched_clock();

	mtk_btag_klog(mtk_btag_mmc, tr);
	mtk_btag_next_trace(rt);
out:
	spin_unlock_irqrestore(&rt->lock, flags);
}


static struct mt_bio_context_task *mt_bio_get_task(struct mt_bio_context *ctx,
	unsigned int task_id)
{
	struct mt_bio_context_task *tsk;

	if (!ctx)
		return NULL;

	if (task_id >= MMC_BIOLOG_CONTEXT_TASKS) {
		pr_notice("%s: invalid task id %d\n",
			__func__, task_id);
		return NULL;
	}

	tsk = &ctx->task[task_id];
	tsk->task_id = task_id;

	return tsk;
}

static struct mt_bio_context_task *mt_bio_curr_task_by_ctx_id(
	unsigned int task_id,
	struct mt_bio_context **curr_ctx,
	int mt_ctx_map,
	bool ext_sd)
{
	struct mt_bio_context *ctx;

	if (mt_ctx_map == -1)
		/* get ctx by current pid */
		ctx = mt_bio_curr_ctx(ext_sd);
	else
		/* get ctx by ctx map id */
		ctx = mt_bio_get_ctx(mt_ctx_map);
	if (curr_ctx)
		*curr_ctx = ctx;
	return mt_bio_get_task(ctx, task_id);
}

static struct mt_bio_context_task *mt_bio_curr_task(unsigned int task_id,
	struct mt_bio_context **curr_ctx, bool ext_sd)
{
	/* get ctx by current pid */
	return mt_bio_curr_task_by_ctx_id(task_id, curr_ctx, -1, ext_sd);
}

static const char *task_name[tsk_max] = {
	"req_start", "dma_start", "dma_end", "isdone_start", "isdone_end"};

static void mt_pr_cmdq_tsk(struct mt_bio_context_task *tsk, int stage)
{
	int rw;
	int klogen = BTAG_KLOGEN(mtk_btag_mmc);
	__u32 bytes;
	char buf[256];

	if (!((klogen == 2 && stage == tsk_isdone_end) || (klogen == 3)))
		return;

	rw = tsk->arg & (1 << 30);  /* write: 0, read: 1 */
	bytes = (tsk->arg & 0xFFFF) << SECTOR_SHIFT;

	mtk_btag_task_timetag(buf, 256, stage, tsk_max, task_name, tsk->t,
		bytes);

	pr_debug("[BLOCK_TAG] cmdq: tsk[%d],%d,%s,len=%d%s\n",
		tsk->task_id, stage+1, (rw)?"r":"w", bytes, buf);
}

void mt_biolog_cmdq_check(void)
{
	struct mt_bio_context *ctx;
	__u64 end_time, period_time;
	unsigned long flags;

	ctx = mt_bio_curr_ctx(false);
	if (!ctx)
		return;

	spin_lock_irqsave(&ctx->lock, flags);
	end_time = sched_clock();
	period_time = end_time - ctx->period_start_t;

	if (period_time >= MT_BIO_TRACE_LATENCY) {
		ctx->period_end_t = end_time;
		ctx->workload.period = period_time;
		mt_bio_context_eval(ctx);
		mt_bio_print_trace(ctx);
		ctx->period_start_t = end_time;
		ctx->period_end_t = 0;
		ctx->wl.period_busy = 0;
		ctx->wl.period_left_window_end_t = 0;
		ctx->wl.period_right_window_end_t = 0;
		ctx->wl.period_right_window_start_t = 0;
		memset(&ctx->throughput, 0, sizeof(struct mtk_btag_throughput));
		memset(&ctx->workload, 0, sizeof(struct mtk_btag_workload));
	}
	spin_unlock_irqrestore(&ctx->lock, flags);
}

/* Command Queue Hook: stage1: queue task */
void mt_biolog_cmdq_queue_task(unsigned int task_id, struct mmc_request *req)
{
	struct mt_bio_context *ctx;
	struct mt_bio_context_task *tsk;
	int i;

	if (!req)
		return;
	if (!req->sbc)
		return;

	tsk = mt_bio_curr_task(task_id, &ctx, false);
	if (!tsk)
		return;

	if (ctx->state == CMDQ_CTX_NOT_DMA)
		ctx->state = CMDQ_CTX_QUEUE;

	tsk->arg = req->sbc->arg;
	tsk->t[tsk_req_start] = sched_clock();

	ctx->q_depth++;
	mtk_btag_mictx_update_ctx(ctx->q_depth);

	for (i = tsk_dma_start; i < tsk_max; i++)
		tsk->t[i] = 0;

	if (!ctx->period_start_t)
		ctx->period_start_t = tsk->t[tsk_req_start];

	mt_pr_cmdq_tsk(tsk, tsk_req_start);
}

static void mt_bio_ctx_count_usage(struct mt_bio_context *ctx,
	__u64 start, __u64 end)
{
	if (start <= ctx->period_start_t) {
		/*
		 * if 'start' is located in previous period,
		 * reset right window and period_busy,
		 * and finally only left window will be existed.
		 */
		ctx->wl.period_left_window_end_t = end;
		ctx->wl.period_right_window_start_t =
		ctx->wl.period_right_window_end_t =
		ctx->wl.period_busy = 0;
	} else {
		/* if left window is existed */
		if (ctx->wl.period_left_window_end_t) {
			if (start < ctx->wl.period_left_window_end_t) {
				/*
				 * if 'start' is located inside left window,
				 * reset right window and period_busy,
				 * and finally only left window will be existed.
				 */
				ctx->wl.period_left_window_end_t = end;
				ctx->wl.period_right_window_start_t =
				ctx->wl.period_right_window_end_t =
				ctx->wl.period_busy = 0;
			} else
				goto new_window;
		} else
			goto new_window;
	}

	goto out;

new_window:

	if (ctx->wl.period_right_window_start_t) {
		if (start > ctx->wl.period_right_window_end_t) {
			ctx->wl.period_busy +=
			(ctx->wl.period_right_window_end_t -
			ctx->wl.period_right_window_start_t);
			ctx->wl.period_right_window_start_t = start;
		}
		ctx->wl.period_right_window_end_t = end;
	} else {
		ctx->wl.period_right_window_start_t = start;
		ctx->wl.period_right_window_end_t = end;
	}

out:
	return;
}

static uint64_t mt_bio_get_period_busy(struct mt_bio_context *ctx)
{
	uint64_t busy;

	busy = ctx->wl.period_busy;

	if (ctx->wl.period_left_window_end_t) {
		busy +=
			(ctx->wl.period_left_window_end_t -
			ctx->period_start_t);
	}

	if (ctx->wl.period_right_window_start_t) {
		busy +=
		(ctx->wl.period_right_window_end_t -
		ctx->wl.period_right_window_start_t);
	}

	return busy;
}

/* Command Queue Hook: stage2: dma start */
void mt_biolog_cmdq_dma_start(unsigned int task_id)
{
	struct mt_bio_context_task *tsk;
	struct mt_bio_context *ctx;

	tsk = mt_bio_curr_task(task_id, &ctx, false);
	if (!tsk)
		return;
	tsk->t[tsk_dma_start] = sched_clock();

	/* count first queue task time in workload usage,
	 * if it was not overlapped with DMA
	 */
	if (ctx->state == CMDQ_CTX_QUEUE)
		mt_bio_ctx_count_usage(ctx, tsk->t[tsk_req_start],
			tsk->t[tsk_dma_start]);
	ctx->state = CMDQ_CTX_IN_DMA;
	mt_pr_cmdq_tsk(tsk, tsk_dma_start);
}

/* Command Queue Hook: stage3: dma end */
void mt_biolog_cmdq_dma_end(unsigned int task_id)
{
	struct mt_bio_context_task *tsk;
	struct mt_bio_context *ctx;

	tsk = mt_bio_curr_task(task_id, &ctx, false);
	if (!tsk)
		return;
	tsk->t[tsk_dma_end] = sched_clock();
	ctx->state = CMDQ_CTX_NOT_DMA;
	mt_pr_cmdq_tsk(tsk, tsk_dma_end);
}

/* Command Queue Hook: stage4: isdone start */
void mt_biolog_cmdq_isdone_start(unsigned int task_id, struct mmc_request *req)
{
	struct mt_bio_context_task *tsk;

	tsk = mt_bio_curr_task(task_id, NULL, false);
	if (!tsk)
		return;
	tsk->t[tsk_isdone_start] = sched_clock();
	mt_pr_cmdq_tsk(tsk, tsk_isdone_start);
}

/* Command Queue Hook: stage5: isdone end */
void mt_biolog_cmdq_isdone_end(unsigned int task_id)
{
	int write, i;
	__u32 bytes;
	__u64 end_time, busy_time;
	struct mt_bio_context *ctx;
	struct mt_bio_context_task *tsk;
	struct mtk_btag_throughput_rw *tp;

	tsk = mt_bio_curr_task(task_id, &ctx, false);
	if (!tsk)
		return;

	/* return if there's no on-going request  */
	for (i = 0; i < tsk_isdone_end; i++)
		if (!tsk->t[i])
			return;

	tsk->t[tsk_isdone_end] = end_time = sched_clock();

	ctx->q_depth--;
	mtk_btag_mictx_update_ctx(ctx->q_depth);

	/* throughput usage := duration of handling this request */

	/* tsk->arg & (1 << 30): 0 means write */
	write = tsk->arg & (1 << 30);
	write = (write) ? 0 : 1;

	bytes = tsk->arg & 0xFFFF;
	bytes = bytes << SECTOR_SHIFT;
	busy_time = end_time - tsk->t[tsk_req_start];
	tp = (write) ? &ctx->throughput.w : &ctx->throughput.r;
	tp->usage += busy_time;
	tp->size += bytes;

	mtk_btag_mictx_eval_tp(write, busy_time, bytes);

	/* workload statistics */
	ctx->workload.count++;

	/* count DMA time in workload usage */

	/* count isdone time in workload usage,
	 * if it was not overlapped with DMA
	 */
	if (ctx->state == CMDQ_CTX_NOT_DMA)
		mt_bio_ctx_count_usage(ctx, tsk->t[tsk_dma_start], end_time);
	else
		mt_bio_ctx_count_usage(ctx, tsk->t[tsk_dma_start],
			tsk->t[tsk_dma_end]);

	mt_pr_cmdq_tsk(tsk, tsk_isdone_end);

	mt_bio_init_task(tsk);
}

void mt_biolog_cqhci_check(void)
{
	mt_biolog_cmdq_check();
}

/*
 * parameter "host" needed due to req->host
 * doesn't initial when enter here
 */
void mt_biolog_cqhci_queue_task(struct mmc_host *host,
	unsigned int task_id, struct mmc_request *req)
{
	struct mt_bio_context *ctx;
	struct mt_bio_context_task *tsk;
	u32 req_flags;
	unsigned long flags;

	if (!req || !req->data)
		return;

	req_flags = req->data->flags;

	tsk = mt_bio_curr_task_by_ctx_id(task_id,
		&ctx, -1, false);
	if (!tsk)
		return;

	spin_lock_irqsave(&ctx->lock, flags);
	/* CANNOT used req->host here, it doesn't been initial when go here */
	if (host && (host->caps2 & MMC_CAP2_CQE)) {
		/* convert cqhci to legacy sbc arg */
		if (req_flags & MMC_DATA_READ)
			tsk->arg = 1 << 30 | (req->data->blocks & 0xFFFF);
		else if (req_flags & MMC_DATA_WRITE) {
			tsk->arg = (req->data->blocks & 0xFFFF);
			tsk->arg = tsk->arg & ~(1 << 30);
		}
	} else {
		if (req->sbc)
			tsk->arg = req->sbc->arg;
	}

	tsk->t[tsk_req_start] = sched_clock();

	ctx->q_depth++;
	mtk_btag_mictx_update_ctx(ctx->q_depth);

	if (!ctx->period_start_t)
		ctx->period_start_t = tsk->t[tsk_req_start];

	spin_unlock_irqrestore(&ctx->lock, flags);

	mt_pr_cmdq_tsk(tsk, tsk_req_start);
}

void mt_biolog_cqhci_complete(unsigned int task_id)
{
	int write;
	__u32 bytes;
	__u64 end_time, busy_time;
	struct mt_bio_context *ctx;
	struct mt_bio_context_task *tsk;
	struct mtk_btag_throughput_rw *tp;
	unsigned long flags;

	tsk = mt_bio_curr_task_by_ctx_id(task_id,
		&ctx, -1, false);
	if (!tsk)
		return;

	spin_lock_irqsave(&ctx->lock, flags);

	tsk->t[tsk_isdone_end] = end_time = sched_clock();

	ctx->q_depth--;
	mtk_btag_mictx_update_ctx(ctx->q_depth);

	/* throughput usage := duration of handling this request */

	/* tsk->arg & (1 << 30): 0 means write */
	write = tsk->arg & (1 << 30);
	write = (write) ? 0 : 1;

	bytes = tsk->arg & 0xFFFF;
	bytes = bytes << SECTOR_SHIFT;
	busy_time = end_time - tsk->t[tsk_req_start];

	/*
	 * workaround: skip the IO which size is 0, please noted
	 * that this issue doesn't exist in non-standard CQHCI driver,
	 * it only happened in standard CQHCI driver.
	 */
	if (!bytes)
		goto end;

	tp = (write) ? &ctx->throughput.w : &ctx->throughput.r;
	tp->usage += busy_time;
	tp->size += bytes;

	mtk_btag_mictx_eval_tp(write, busy_time, bytes);

	/* workload statistics */
	ctx->workload.count++;

	/* count doorbell to complete time in workload usage */
	mt_bio_ctx_count_usage(ctx, tsk->t[tsk_req_start], end_time);
end:
	mt_pr_cmdq_tsk(tsk, tsk_isdone_end);

	mt_bio_init_task(tsk);
	spin_unlock_irqrestore(&ctx->lock, flags);
}

/* MMC Queue Hook: check function at mmc_blk_issue_rw_rq() */
void mt_biolog_mmcqd_req_check(bool ext_sd)
{
	struct mt_bio_context *ctx;
	__u64 end_time, period_time;

	ctx = mt_bio_curr_ctx(ext_sd);
	if (!ctx)
		return;

	/* skip mmcqd0, if command queue is applied */
	if ((ctx->id == CTX_MMCQD0) && (ctx->state == MMCQD_CMDQ_MODE_EN))
		return;

	end_time = sched_clock();
	period_time = end_time - ctx->period_start_t;

	if (period_time >= MT_BIO_TRACE_LATENCY) {
		ctx->period_end_t = end_time;
		ctx->workload.period = period_time;
		mt_bio_context_eval(ctx);
		mt_bio_print_trace(ctx);
		ctx->period_start_t = end_time;
		ctx->period_end_t = 0;
		ctx->wl.period_busy = 0;
		ctx->wl.period_left_window_end_t = 0;
		ctx->wl.period_right_window_end_t = 0;
		ctx->wl.period_right_window_start_t = 0;
		memset(&ctx->throughput, 0, sizeof(struct mtk_btag_throughput));
		memset(&ctx->workload, 0, sizeof(struct mtk_btag_workload));
	}
}

/* MMC Queue Hook: request start function at mmc_start_req() */
void mt_biolog_mmcqd_req_start(struct mmc_host *host, bool ext_sd)
{
	struct mt_bio_context *ctx;
	struct mt_bio_context_task *tsk;

	tsk = mt_bio_curr_task(0, &ctx, ext_sd);
	if (!tsk)
		return;
	tsk->t[tsk_req_start] = sched_clock();

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	if ((ctx->id == CTX_MMCQD0) &&
		(ctx->state == MMCQD_NORMAL) &&
		host->card->ext_csd.cmdq_en)
		ctx->state = MMCQD_CMDQ_MODE_EN;

	/*
	 * CMDQ mode, embedded eMMC mictx will be
	 * updated in mt_biolog_cmdq_*, so bypass it here.
	 */
#else
	/* Legacy mode. Update mictx for embedded eMMC only */
	if (ctx->qid == BTAG_STORAGE_EMBEDDED)
		mtk_btag_mictx_update_ctx(1);
#endif
}

/* MMC Queue Hook: request end function at mmc_start_req() */
void mt_biolog_mmcqd_req_end(struct mmc_data *data, bool ext_sd)
{
	int rw;
	__u32 size;
	struct mt_bio_context *ctx;
	struct mt_bio_context_task *tsk;
	struct mtk_btag_throughput_rw *tp;
	__u64 end_time, busy_time;

	end_time = sched_clock();

	if (!data)
		return;

	if (data->flags == MMC_DATA_WRITE)
		rw = 0;
	else if (data->flags == MMC_DATA_READ)
		rw = 1;
	else
		return;

	tsk = mt_bio_curr_task(0, &ctx, ext_sd);
	if (!tsk)
		return;

	/* return if there's no on-going request */
	if (!tsk->t[tsk_req_start])
		return;

	size = (data->blocks * data->blksz);
	busy_time = end_time - tsk->t[tsk_req_start];

	/* workload statistics */
	ctx->workload.count++;

	/* count request handling time in workload usage */
	mt_bio_ctx_count_usage(ctx, tsk->t[tsk_req_start], end_time);

	/* throughput statistics */
	/* write: 0, read: 1 */
	tp = (rw) ? &ctx->throughput.r : &ctx->throughput.w;
	tp->usage += busy_time;
	tp->size += size;

	/* update mictx for embedded eMMC only */
	if (ctx->qid == BTAG_STORAGE_EMBEDDED) {
		mtk_btag_mictx_eval_tp(!rw, busy_time, size);
		mtk_btag_mictx_update_ctx(0);
	}

	/* re-init task to indicate no on-going request */
	mt_bio_init_task(tsk);
}

/*
 * snprintf may return a value of size or "more" to indicate
 * that the output was truncated, thus be careful of "more"
 * case.
 */
#define SPREAD_PRINTF(buff, size, evt, fmt, args...) \
	do { \
		if (buff && size && *(size)) { \
			unsigned long var = snprintf(*(buff), *(size),\
				fmt, ##args); \
			if (var > 0) { \
				if (var > *(size)) \
					var = *(size); \
				*(size) -= var; \
				*(buff) += var; \
			} \
		} \
		if (evt) \
			seq_printf(evt, fmt, ##args); \
		if (!buff && !evt) { \
			pr_info(fmt, ##args); \
		} \
	} while (0)

static size_t mt_bio_seq_debug_show_info(char **buff, unsigned long *size,
	struct seq_file *seq)
{
	int i;
	struct mt_bio_context *ctx = BTAG_CTX(mtk_btag_mmc);

	if (!ctx)
		return 0;

	for (i = 0; i < MMC_BIOLOG_CONTEXTS; i++)	{
		if (ctx[i].pid == 0)
			continue;
		SPREAD_PRINTF(buff, size, seq,
			"ctx[%d]=ctx_map[%d]=%s,pid:%4d,q:%d\n",
			i,
			ctx[i].id,
			ctx[i].comm,
			ctx[i].pid,
			ctx[i].qid);
	}

	return 0;
}

int mt_mmc_biolog_init(void)
{
	struct mtk_blocktag *btag;

	btag = mtk_btag_alloc("mmc",
		MMC_BIOLOG_RINGBUF_MAX,
		sizeof(struct mt_bio_context),
		MMC_BIOLOG_CONTEXTS,
		mt_bio_seq_debug_show_info);

	if (btag)
		mtk_btag_mmc = btag;

	return 0;
}
EXPORT_SYMBOL_GPL(mt_mmc_biolog_init);

int mt_mmc_biolog_exit(void)
{
	mtk_btag_free(mtk_btag_mmc);
	return 0;
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Mediatek MMC Block IO Log");

