// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 * Authors:
 *	Perry Hsu <perry.hsu@mediatek.com>
 *	Stanley Chu <stanley.chu@mediatek.com>
 */

#define DEBUG 1
#define SECTOR_SHIFT 12
#define UFS_MTK_BIO_TRACE_LATENCY (unsigned long long)(1000000000)

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
#include <scsi/scsi_proto.h>
#include "mtk_blocktag.h"
#include "blocktag-ufs.h"

/* ring trace for debugfs */
struct mtk_blocktag *ufs_mtk_btag;

static inline __u16 chbe16_to_u16(const char *str)
{
	__u16 ret;

	ret = str[0];
	ret = ret << 8 | str[1];
	return ret;
}

static inline __u32 chbe32_to_u32(const char *str)
{
	__u32 ret;

	ret = str[0];
	ret = ret << 8 | str[1];
	ret = ret << 8 | str[2];
	ret = ret << 8 | str[3];
	return ret;
}

#define scsi_cmnd_lba(cmd)  chbe32_to_u32(&cmd->cmnd[2])
#define scsi_cmnd_len(cmd)  chbe16_to_u16(&cmd->cmnd[7])
#define scsi_cmnd_cmd(cmd)  (cmd->cmnd[0])

static struct ufs_mtk_bio_context_task *ufs_mtk_bio_get_task(
	struct ufs_mtk_bio_context *ctx, int task_id)
{
	struct ufs_mtk_bio_context_task *tsk = NULL;

	if (!ctx)
		return NULL;

	if (task_id >= UFS_BIOLOG_CONTEXT_TASKS) {
		pr_notice("[BLOCK_TAG] %s: invalid task id %d\n",
			__func__, task_id);
		return NULL;
	}

	tsk = &ctx->task[task_id];

	return tsk;
}

static struct ufs_mtk_bio_context *ufs_mtk_bio_curr_ctx(void)
{
	struct ufs_mtk_bio_context *ctx = BTAG_CTX(ufs_mtk_btag);

	return ctx ? &ctx[0] : NULL;
}

static struct ufs_mtk_bio_context_task *ufs_mtk_bio_curr_task(
	int task_id, struct ufs_mtk_bio_context **curr_ctx)
{
	struct ufs_mtk_bio_context *ctx;

	ctx = ufs_mtk_bio_curr_ctx();
	if (curr_ctx)
		*curr_ctx = ctx;
	return ufs_mtk_bio_get_task(ctx, task_id);
}

int mtk_btag_pidlog_add_ufs(bool write, __u32 total_len, __u32 top_len,
				struct tmp_proc_pidlogger *tmplog)
{
	unsigned long flags;
	struct ufs_mtk_bio_context *ctx;

	ctx = ufs_mtk_bio_curr_ctx();
	if (!ctx)
		return 0;

	spin_lock_irqsave(&ctx->lock, flags);
	mtk_btag_pidlog_insert(&ctx->pidlog, write, tmplog);
	mtk_btag_mictx_eval_req(ufs_mtk_btag, write, total_len, top_len);
	spin_unlock_irqrestore(&ctx->lock, flags);

	return 1;
}

void ufs_mtk_biolog_clk_gating(bool clk_on)
{
}
EXPORT_SYMBOL_GPL(ufs_mtk_biolog_clk_gating);

void ufs_mtk_biolog_send_command(int task_id, struct scsi_cmnd *cmd)
{
	unsigned long flags;
	struct ufs_mtk_bio_context *ctx;
	struct ufs_mtk_bio_context_task *tsk;

	if (!cmd)
		return;

	tsk = ufs_mtk_bio_curr_task(task_id, &ctx);
	if (!tsk)
		return;

	if (scsi_cmd_to_rq(cmd))
		mtk_btag_commit_req(scsi_cmd_to_rq(cmd), false);

	tsk->lba = scsi_cmnd_lba(cmd);
	tsk->len = scsi_cmnd_len(cmd);
	tsk->cmd = scsi_cmnd_cmd(cmd);

	spin_lock_irqsave(&ctx->lock, flags);

	tsk->t[tsk_send_cmd] = sched_clock();
	tsk->t[tsk_req_compl] = 0;

	ctx->sum_of_inflight_start += tsk->t[tsk_send_cmd];
	if (!ctx->period_start_t)
		ctx->period_start_t = tsk->t[tsk_send_cmd];

	ctx->q_depth++;
	mtk_btag_mictx_update(ufs_mtk_btag, ctx->q_depth,
			      ctx->sum_of_inflight_start);

	spin_unlock_irqrestore(&ctx->lock, flags);
}
EXPORT_SYMBOL_GPL(ufs_mtk_biolog_send_command);

__u16 ufs_mtk_bio_mictx_eval_wqd(struct mtk_btag_mictx_struct *mictx,
			       u64 t_cur)
{
	__u64 compl = mictx->weighted_qd;
	__u64 inflight = t_cur * mictx->q_depth - mictx->sum_of_inflight_start;
	__u64 dur = t_cur - mictx->window_begin;

	return DIV64_U64_ROUND_UP(compl + inflight, dur);
}

void ufs_mtk_biolog_transfer_req_compl(int task_id,
				       unsigned long req_mask)
{
	struct ufs_mtk_bio_context *ctx;
	struct ufs_mtk_bio_context_task *tsk;
	struct mtk_btag_throughput_rw *tp = NULL;
	unsigned long flags;
	bool write;
	__u64 busy_time;
	__u32 size;

	tsk = ufs_mtk_bio_curr_task(task_id, &ctx);
	if (!tsk)
		return;

	/* return if there's no on-going request  */
	if (!tsk->t[tsk_send_cmd])
		return;

	spin_lock_irqsave(&ctx->lock, flags);

	tsk->t[tsk_req_compl] = sched_clock();

	if (tsk->cmd == READ_6 || tsk->cmd == READ_10 ||
	    tsk->cmd == READ_16) {
		write = false;
		tp = &ctx->throughput.r;
	} else if (tsk->cmd == WRITE_6 || tsk->cmd == WRITE_10 ||
		   tsk->cmd == WRITE_16) {
		write = true;
		tp = &ctx->throughput.w;
	}

	/* throughput usage := duration of handling this request */
	busy_time = tsk->t[tsk_req_compl] - tsk->t[tsk_send_cmd];

	/* workload statistics */
	ctx->workload.count++;

	if (tp) {
		size = tsk->len << SECTOR_SHIFT;
		tp->usage += busy_time;
		tp->size += size;
		mtk_btag_mictx_eval_tp(ufs_mtk_btag, write, busy_time,
				       size);
	}

	ctx->sum_of_inflight_start -= tsk->t[tsk_send_cmd];
	if (!req_mask)
		ctx->q_depth = 0;
	else
		ctx->q_depth--;
	mtk_btag_mictx_update(ufs_mtk_btag, ctx->q_depth, ctx->sum_of_inflight_start);
	mtk_btag_mictx_accumulate_weight_qd(ufs_mtk_btag, tsk->t[tsk_send_cmd],
					    tsk->t[tsk_req_compl]);

	/* clear this task */
	tsk->t[tsk_send_cmd] = tsk->t[tsk_req_compl] = 0;

	spin_unlock_irqrestore(&ctx->lock, flags);
}
EXPORT_SYMBOL_GPL(ufs_mtk_biolog_transfer_req_compl);

/* evaluate throughput and workload of given context */
static void ufs_mtk_bio_context_eval(struct ufs_mtk_bio_context *ctx)
{
	__u64 period;

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
static struct mtk_btag_trace *ufs_mtk_bio_print_trace(
	struct ufs_mtk_bio_context *ctx)
{
	struct mtk_btag_ringtrace *rt = BTAG_RT(ufs_mtk_btag);
	struct mtk_btag_trace *tr;
	unsigned long flags;

	if (!rt)
		return NULL;

	spin_lock_irqsave(&rt->lock, flags);
	tr = mtk_btag_curr_trace(rt);

	if (!tr)
		goto out;

	memset(tr, 0, sizeof(struct mtk_btag_trace));
	tr->pid = 0;
	tr->qid = 0;
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

static void ufs_mtk_bio_ctx_count_usage(struct ufs_mtk_bio_context *ctx,
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
void ufs_mtk_biolog_check(unsigned long req_mask)
{
	struct ufs_mtk_bio_context *ctx;
	struct mtk_btag_trace *tr = NULL;
	__u64 end_time, period_time;
	unsigned long flags;

	ctx = ufs_mtk_bio_curr_ctx();
	if (!ctx)
		return;

	end_time = sched_clock();

	spin_lock_irqsave(&ctx->lock, flags);

	if (ctx->busy_start_t)
		ufs_mtk_bio_ctx_count_usage(ctx, ctx->busy_start_t, end_time);

	ctx->busy_start_t = (req_mask) ? end_time : 0;

	period_time = end_time - ctx->period_start_t;

	if (period_time >= UFS_MTK_BIO_TRACE_LATENCY) {
		ctx->period_end_t = end_time;
		ctx->workload.period = period_time;
		ufs_mtk_bio_context_eval(ctx);
		tr = ufs_mtk_bio_print_trace(ctx);
		ctx->period_start_t = end_time;
		ctx->period_end_t = 0;
		ctx->period_usage = 0;
		memset(&ctx->throughput, 0, sizeof(struct mtk_btag_throughput));
		memset(&ctx->workload, 0, sizeof(struct mtk_btag_workload));
	}
	spin_unlock_irqrestore(&ctx->lock, flags);
}
EXPORT_SYMBOL_GPL(ufs_mtk_biolog_check);

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

static size_t ufs_mtk_bio_seq_debug_show_info(char **buff, unsigned long *size,
	struct seq_file *seq)
{
	return 0;
}

static void ufs_mtk_bio_init_ctx(struct ufs_mtk_bio_context *ctx)
{
	memset(ctx, 0, sizeof(struct ufs_mtk_bio_context));
	spin_lock_init(&ctx->lock);
	ctx->period_start_t = sched_clock();
}

static struct mtk_btag_vops ufs_mtk_btag_vops = {
	.seq_show       = ufs_mtk_bio_seq_debug_show_info,
	.mictx_eval_wqd = ufs_mtk_bio_mictx_eval_wqd,
};

int ufs_mtk_biolog_init(bool qos_allowed, bool boot_device)
{
	struct mtk_blocktag *btag;

	if (qos_allowed)
		ufs_mtk_btag_vops.earaio_enabled = true;

	if (boot_device)
		ufs_mtk_btag_vops.boot_device = true;

	btag = mtk_btag_alloc("ufs",
				BTAG_STORAGE_UFS,
				UFS_BIOLOG_RINGBUF_MAX,
				sizeof(struct ufs_mtk_bio_context),
				UFS_BIOLOG_CONTEXTS,
				&ufs_mtk_btag_vops);

	if (btag) {
		struct ufs_mtk_bio_context *ctx;

		ufs_mtk_btag = btag;
		ctx = BTAG_CTX(ufs_mtk_btag);
		ufs_mtk_bio_init_ctx(&ctx[0]);
	}
	return 0;
}
EXPORT_SYMBOL_GPL(ufs_mtk_biolog_init);

int ufs_mtk_biolog_exit(void)
{
	mtk_btag_free(ufs_mtk_btag);
	return 0;
}
EXPORT_SYMBOL_GPL(ufs_mtk_biolog_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Mediatek UFS Block IO Tracer");
MODULE_AUTHOR("Perry Hsu <perry.hsu@mediatek.com>");
MODULE_AUTHOR("Stanley Chu <stanley chu@mediatek.com>");

