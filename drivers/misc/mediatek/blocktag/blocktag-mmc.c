// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */
#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>
#include <linux/tick.h>
#include <linux/time.h>
#include <linux/vmalloc.h>
#include "queue.h"
#include "mtk_blocktag.h"
#include "blocktag-mmc.h"

/* ring trace for debugfs, eMMC & SD */
struct mtk_blocktag *mmc_mtk_btag;

static struct mmc_mtk_bio_context_task *mmc_mtk_bio_get_task(
	struct mmc_mtk_bio_context *ctx, __u16 task_id)
{
	struct mmc_mtk_bio_context_task *tsk = NULL;

	if (!ctx)
		return NULL;

	if (task_id >= MMC_BIOLOG_CONTEXT_TASKS) {
		pr_notice("[BLOCK_TAG] %s: invalid task id %d\n",
			__func__, task_id);
		return NULL;
	}

	tsk = &ctx->task[task_id];

	return tsk;
}

static struct mmc_mtk_bio_context *mmc_mtk_bio_curr_ctx(bool is_sd)
{
	struct mmc_mtk_bio_context *ctx = BTAG_CTX(mmc_mtk_btag);

	if (is_sd)
		return ctx ? &ctx[1] : NULL;
	else
		return ctx ? &ctx[0] : NULL;
}

static struct mmc_mtk_bio_context_task *mmc_mtk_bio_curr_task(
	__u16 task_id, struct mmc_mtk_bio_context **curr_ctx, bool is_sd)
{
	struct mmc_mtk_bio_context *ctx;

	ctx = mmc_mtk_bio_curr_ctx(is_sd);
	if (curr_ctx)
		*curr_ctx = ctx;
	return mmc_mtk_bio_get_task(ctx, task_id);
}

int mtk_btag_pidlog_add_mmc(bool is_sd, bool write, __u32 total_len,
			    __u32 top_len, struct tmp_proc_pidlogger *tmplog)
{
	unsigned long flags;
	struct mmc_mtk_bio_context *ctx;

	ctx = mmc_mtk_bio_curr_ctx(is_sd);
	if (!ctx)
		return 0;

	spin_lock_irqsave(&ctx->lock, flags);
	mtk_btag_pidlog_insert(&ctx->pidlog, write, tmplog);
	mtk_btag_mictx_eval_req(mmc_mtk_btag, 0, write, total_len, top_len);
	spin_unlock_irqrestore(&ctx->lock, flags);

	return 1;
}

void mmc_mtk_biolog_send_command(__u16 task_id, struct mmc_request *mrq)
{
	struct mmc_queue_req *mqrq;
	struct request *req;
	unsigned long flags;
	struct mmc_host *mmc;
	struct mmc_mtk_bio_context *ctx;
	struct mmc_mtk_bio_context_task *tsk;
	bool is_sd;

	if (!mrq)
		return;

	mmc = mrq->host;
	if (!mmc)
		return;

	req = NULL;

	if (!(mmc->caps2 & MMC_CAP2_NO_MMC))
		is_sd = false;
	else if (!(mmc->caps2 & MMC_CAP2_NO_SD))
		is_sd = true;
	else
		return;

	tsk = mmc_mtk_bio_curr_task(task_id, &ctx, is_sd);
	if (!tsk)
		return;

	if (!is_sd && !mrq->cmd) { /* eMMC CQHCI */
		mqrq = container_of(mrq, struct mmc_queue_req, brq.mrq);
		req = blk_mq_rq_from_pdu(mqrq);
	/* SD non-cqhci */
	} else if (is_sd &&
		(mrq->cmd->opcode == MMC_READ_SINGLE_BLOCK ||
		mrq->cmd->opcode == MMC_READ_MULTIPLE_BLOCK ||
		mrq->cmd->opcode == MMC_WRITE_BLOCK ||
		mrq->cmd->opcode == MMC_WRITE_MULTIPLE_BLOCK)) {
		/* skip ioctl path such as RPMB test */
		if (PTR_ERR(mrq->completion.wait.task_list.next)
			&& PTR_ERR(mrq->completion.wait.task_list.prev))
			return;
		mqrq = container_of(mrq, struct mmc_queue_req, brq.mrq);
		req = blk_mq_rq_from_pdu(mqrq);
	} else
		return;

	if (req)
		mtk_btag_commit_req(0, req, is_sd);

	if (is_sd && mrq->cmd->data) {
		tsk->len = mrq->cmd->data->blksz * mrq->cmd->data->blocks;
		tsk->dir = MMC_DATA_DIR(!!(mrq->cmd->data->flags & MMC_DATA_READ));
	} else if (!is_sd && mrq->data) {
		tsk->len = mrq->data->blksz * mrq->data->blocks;
		tsk->dir = MMC_DATA_DIR(!!(mrq->data->flags & MMC_DATA_READ));
	}

	spin_lock_irqsave(&ctx->lock, flags);

	tsk->t[tsk_send_cmd] = sched_clock();
	tsk->t[tsk_req_compl] = 0;

	if (!ctx->period_start_t)
		ctx->period_start_t = tsk->t[tsk_send_cmd];

	ctx->q_depth++;
	mtk_btag_mictx_update(mmc_mtk_btag, 0, ctx->q_depth, 0);

	spin_unlock_irqrestore(&ctx->lock, flags);
}
EXPORT_SYMBOL_GPL(mmc_mtk_biolog_send_command);

void mmc_mtk_biolog_transfer_req_compl(struct mmc_host *mmc,
	__u16 task_id, unsigned long req_mask)
{
	struct mmc_mtk_bio_context *ctx;
	struct mmc_mtk_bio_context_task *tsk;
	struct mtk_btag_throughput_rw *tp = NULL;
	unsigned long flags;
	bool write;
	__u64 busy_time;
	__u32 size;
	bool is_sd;

	if (!(mmc->caps2 & MMC_CAP2_NO_MMC))
		is_sd = false;
	else if (!(mmc->caps2 & MMC_CAP2_NO_SD))
		is_sd = true;
	else
		return;

	tsk = mmc_mtk_bio_curr_task(task_id, &ctx, is_sd);
	if (!tsk)
		return;

	/* return if there's no on-going request  */
	if (!tsk->t[tsk_send_cmd])
		return;

	spin_lock_irqsave(&ctx->lock, flags);

	tsk->t[tsk_req_compl] = sched_clock();

	if (tsk->dir) {
		write = false;
		tp = &ctx->throughput.r;
	} else {
		write = true;
		tp = &ctx->throughput.w;
	}

	/* throughput usage := duration of handling this request */
	busy_time = tsk->t[tsk_req_compl] - tsk->t[tsk_send_cmd];

	/* workload statistics */
	ctx->workload.count++;

	if (tp) {
		size = tsk->len;
		tp->usage += busy_time;
		tp->size += size;
		mtk_btag_mictx_eval_tp(mmc_mtk_btag, 0, write, busy_time,
				       size);
	}

	if (!req_mask)
		ctx->q_depth = 0;
	else
		ctx->q_depth--;
	mtk_btag_mictx_update(mmc_mtk_btag, 0, ctx->q_depth, 0);
	mtk_btag_mictx_accumulate_weight_qd(mmc_mtk_btag, 0,
					    tsk->t[tsk_send_cmd],
					    tsk->t[tsk_req_compl]);

	/* clear this task */
	tsk->t[tsk_send_cmd] = tsk->t[tsk_req_compl] = 0;

	spin_unlock_irqrestore(&ctx->lock, flags);
}
EXPORT_SYMBOL_GPL(mmc_mtk_biolog_transfer_req_compl);

/* evaluate throughput and workload of given context */
static void mmc_mtk_bio_context_eval(struct mmc_mtk_bio_context *ctx)
{
	uint64_t period;

	ctx->workload.usage = ctx->period_usage;

	if (ctx->workload.period > (ctx->workload.usage * 100)) {
		ctx->workload.percent = 1;
	} else {
		period = ctx->workload.period;
		do_div(period, 100);
		ctx->workload.percent =
			(__u32)ctx->workload.usage / (__u32)period;
	}
	mtk_btag_throughput_eval(&ctx->throughput);
}

/* print context to trace ring buffer */
static struct mtk_btag_trace *mmc_mtk_bio_print_trace(
	struct mmc_mtk_bio_context *ctx)
{
	struct mtk_btag_ringtrace *rt = BTAG_RT(mmc_mtk_btag);
	struct mtk_btag_trace *tr;
	unsigned long flags;

	if (!rt)
		return NULL;

	spin_lock_irqsave(&rt->lock, flags);
	tr = mtk_btag_curr_trace(rt);

	if (!tr)
		goto out;

	memset(tr, 0, sizeof(struct mtk_btag_trace));
	tr->pid = ctx->pid;
	tr->qid = ctx->qid;
	mtk_btag_pidlog_eval(&tr->pidlog, &ctx->pidlog);
	mtk_btag_vmstat_eval(&tr->vmstat);
	mtk_btag_cpu_eval(&tr->cpu);
	memcpy(&tr->throughput, &ctx->throughput,
		sizeof(struct mtk_btag_throughput));
	memcpy(&tr->workload, &ctx->workload, sizeof(struct mtk_btag_workload));

	tr->time = sched_clock();
	mtk_btag_next_trace(rt);
out:
	spin_unlock_irqrestore(&rt->lock, flags);
	return tr;
}

static void mmc_mtk_bio_ctx_count_usage(struct mmc_mtk_bio_context *ctx,
	__u64 start, __u64 end)
{
	__u64 busy_in_period;

	if (start < ctx->period_start_t)
		busy_in_period = end - ctx->period_start_t;
	else
		busy_in_period = end - start;

	ctx->period_usage += busy_in_period;
}

/* Check requests after set/clear mask. */
void mmc_mtk_biolog_check(struct mmc_host *mmc, unsigned long req_mask)
{
	struct mmc_mtk_bio_context *ctx;
	struct mtk_btag_trace *tr = NULL;
	__u64 end_time, period_time;
	unsigned long flags;
	bool is_sd;

	if (!(mmc->caps2 & MMC_CAP2_NO_MMC))
		is_sd = false;
	else if (!(mmc->caps2 & MMC_CAP2_NO_SD))
		is_sd = true;
	else
		return;

	ctx = mmc_mtk_bio_curr_ctx(is_sd);
	if (!ctx)
		return;

	end_time = sched_clock();

	spin_lock_irqsave(&ctx->lock, flags);

	if (ctx->busy_start_t)
		mmc_mtk_bio_ctx_count_usage(ctx, ctx->busy_start_t, end_time);

	ctx->busy_start_t = (req_mask) ? end_time : 0;

	period_time = end_time - ctx->period_start_t;

	if (period_time >= MMC_MTK_BIO_TRACE_LATENCY) {
		ctx->period_end_t = end_time;
		ctx->workload.period = period_time;
		mmc_mtk_bio_context_eval(ctx);
		tr = mmc_mtk_bio_print_trace(ctx);
		ctx->period_start_t = end_time;
		ctx->period_end_t = 0;
		ctx->period_usage = 0;
		memset(&ctx->throughput, 0, sizeof(struct mtk_btag_throughput));
		memset(&ctx->workload, 0, sizeof(struct mtk_btag_workload));
	}
	spin_unlock_irqrestore(&ctx->lock, flags);
}
EXPORT_SYMBOL_GPL(mmc_mtk_biolog_check);

/*
 * snprintf may return a value of size or "more" to indicate
 * that the output was truncated, thus be careful of "more"
 * case.
 */
#define SPREAD_PRINTF(buff, size, evt, fmt, args...) \
do { \
	if (buff && size && *(size)) { \
		unsigned long var = snprintf(*(buff), *(size), fmt, ##args); \
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

static size_t mmc_mtk_bio_seq_debug_show_info(char **buff, unsigned long *size,
	struct seq_file *seq)
{
	int i;
	struct mmc_mtk_bio_context *ctx = BTAG_CTX(mmc_mtk_btag);

	if (!ctx)
		return 0;

	for (i = 0; i < MMC_BIOLOG_CONTEXTS; i++)	{
		if (ctx[i].pid == 0)
			continue;
		SPREAD_PRINTF(buff, size, seq,
			"ctx[%d]=ctx_map[%d],pid:%4d,q:%d\n",
			i,
			ctx[i].id,
			ctx[i].pid,
			ctx[i].qid);
	}

	return 0;
}

static void mmc_mtk_bio_init_ctx(struct mmc_mtk_bio_context *ctx)
{
	int i;

	for (i = 0; i < MMC_BIOLOG_CONTEXTS; i++) {
		memset(ctx + i, 0, sizeof(struct mmc_mtk_bio_context));
		spin_lock_init(&(ctx + i)->lock);
		(ctx + i)->period_start_t = sched_clock();
		(ctx + i)->qid = i;
	}
}

static struct mtk_btag_vops mmc_mtk_btag_vops = {
	.seq_show       = mmc_mtk_bio_seq_debug_show_info,
};

int mmc_mtk_biolog_init(struct mmc_host *mmc)
{
	struct mtk_blocktag *btag;
	struct mmc_mtk_bio_context *ctx;
	struct device_node *np;
	struct device_node *boot_node = NULL;
	struct tag_bootmode *tag = NULL;

	if (!mmc)
		return -EINVAL;

	np = mmc->class_dev.of_node;
	boot_node = of_parse_phandle(np, "bootmode", 0);

	if (boot_node) {
		tag = (struct tag_bootmode *)of_get_property(boot_node,
							"atag,boot", NULL);
		if (tag) {
			if (tag->boottype == 1)
				mmc_mtk_btag_vops.boot_device = true;
		}
	}

	btag = mtk_btag_alloc("mmc",
				BTAG_STORAGE_MMC,
				MMC_BIOLOG_RINGBUF_MAX,
				sizeof(struct mmc_mtk_bio_context),
				MMC_BIOLOG_CONTEXTS,
				&mmc_mtk_btag_vops);

	if (btag) {
		mmc_mtk_btag = btag;
		ctx = BTAG_CTX(mmc_mtk_btag);
		mmc_mtk_bio_init_ctx(ctx);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(mmc_mtk_biolog_init);

int mmc_mtk_biolog_exit(void)
{
	mtk_btag_free(mmc_mtk_btag);
	return 0;
}
EXPORT_SYMBOL_GPL(mmc_mtk_biolog_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Mediatek MMC Block IO Tracer");
