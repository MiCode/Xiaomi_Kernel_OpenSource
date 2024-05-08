// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (C) 2023 Xiaomi Inc.
* Authors:
*	Tianyang cao <caotianyang@xiaomi.com>
*/


#define DEBUG 1
#define SECTOR_SHIFT 12
#define MPBE_UFS_TRACE_LATENCY ((unsigned long long)(1000000000))

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
#include "mpbe.h"
#include "mpbe-ufs.h"

/* ring trace for debugfs */
struct mpbe *ufs_mpbe;
struct workqueue_struct *ufs_mpbe_wq;
struct work_struct ufs_mpbe_worker;

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

static struct mpbe_ufs_ctx *mpbe_ufs_curr_ctx(__u16 task_id)
{
	struct mpbe_ufs_ctx *ctx = MPBE_CTX(ufs_mpbe);

	if (!ctx)
		return NULL;

	if (MPBE_UFS_QUEUE_ID(task_id) >= ufs_mpbe->ctx.count) {
		pr_notice("[MPBE] %s: invalid task id %d\n",
			__func__, task_id);
		return NULL;
	}

	return &ctx[MPBE_UFS_QUEUE_ID(task_id)];
}

static struct mpbe_ufs_task *mpbe_ufs_curr_task(__u16 task_id,
		struct mpbe_ufs_ctx **curr_ctx)
{
	struct mpbe_ufs_ctx *ctx;
	struct mpbe_ufs_task *tsk;

	ctx = mpbe_ufs_curr_ctx(task_id);
	if (!ctx)
		return NULL;

	*curr_ctx = ctx;
	tsk = &ctx->task[MPBE_UFS_TAG_ID(task_id)];
	return tsk;
}

int mpbe_trace_add_ufs(__u16 task_id, bool write, __u32 total_len,
			    __u32 top_len)
{
	unsigned long flags;
	struct mpbe_ufs_ctx *ctx;

	ctx = mpbe_ufs_curr_ctx(task_id);
	if (!ctx)
		return 0;

	spin_lock_irqsave(&ctx->lock, flags);
	mpbe_mictx_eval_req(ufs_mpbe, MPBE_UFS_QUEUE_ID(task_id),
				write, total_len, top_len);
	spin_unlock_irqrestore(&ctx->lock, flags);

	return 1;
}
void mpbe_ufs_clk_gating(bool clk_on)
{
}
EXPORT_SYMBOL_GPL(mpbe_ufs_clk_gating);

static void mpbe_ufs_transfer_req_send(__u16 task_id, struct scsi_cmnd *cmd)
{
	struct mpbe_ufs_ctx *ctx;
	struct mpbe_ufs_task *tsk;
	unsigned long flags;

	if (!cmd)
		return;

	tsk = mpbe_ufs_curr_task(task_id, &ctx);
	if (!tsk || !ctx)
		return;

	if (blk_mq_rq_from_pdu(cmd))
		mpbe_commit_req(task_id, blk_mq_rq_from_pdu(cmd), false);

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
	mpbe_mictx_update(ufs_mpbe, MPBE_UFS_QUEUE_ID(task_id),
			      ctx->q_depth, ctx->sum_of_inflight_start, 0);

	spin_unlock_irqrestore(&ctx->lock, flags);
	MPBE_DBG("exit\n");
}

__u16 mpbe_ufs_mictx_eval_wqd(struct mpbe_mictx_data *data,
				  u64 t_cur)
{
	__u64 compl = data->weighted_qd;
	__u64 inflight = t_cur * data->q_depth - data->sum_of_inflight_start;
	__u64 dur = t_cur - data->window_begin;

	return DIV64_U64_ROUND_UP(compl + inflight, dur);
}

static void mpbe_ufs_transfer_req_compl(__u16 task_id, unsigned long req_mask)
{
	struct mpbe_ufs_ctx *ctx;
	struct mpbe_ufs_task *tsk;
	struct mpbe_throughput_rw *tp = NULL;
	unsigned long flags;
	bool write = false;
	__u64 busy_time;
	__u32 size;

	tsk = mpbe_ufs_curr_task(task_id, &ctx);
	if (!tsk || !ctx)
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
		mpbe_mictx_eval_tp(ufs_mpbe, MPBE_UFS_QUEUE_ID(task_id),
				       write, busy_time, size);
	}

	ctx->sum_of_inflight_start -= tsk->t[tsk_send_cmd];
	if (!req_mask)
		ctx->q_depth = 0;
	else
		ctx->q_depth--;

	mpbe_mictx_update(ufs_mpbe, MPBE_UFS_QUEUE_ID(task_id),
			      ctx->q_depth, ctx->sum_of_inflight_start, 1);
	mpbe_mictx_accumulate_weight_qd(ufs_mpbe,
					    MPBE_UFS_QUEUE_ID(task_id),
					    tsk->t[tsk_send_cmd],
					    tsk->t[tsk_req_compl]);

	/* clear this task */
	tsk->t[tsk_send_cmd] = tsk->t[tsk_req_compl] = 0;

	spin_unlock_irqrestore(&ctx->lock, flags);
}

static inline int mpbe_ufs_cmd_direction(u8 opcode)
{
	if (opcode == READ_6 || opcode == READ_10 || opcode == READ_16)
		return READ;
	else if (opcode == WRITE_6 || opcode == WRITE_10 || opcode == WRITE_16)
		return WRITE;
	else
		return -EINVAL;
}

int mpbe_trace_ufs_cmd(struct ufs_hba *hba, struct ufshcd_lrb *lrbp, bool is_send)
{
	int dir = 0, nr_tag = 0;
	unsigned long ongoing_cnt = 0;

	if (mpbe_earaio_enabled()) {
		if (lrbp && lrbp->cmd && lrbp->cmd->cmnd[0]) {
			dir = mpbe_ufs_cmd_direction(*lrbp->cmd->cmnd);
			for_each_set_bit(nr_tag, &hba->outstanding_reqs, hba->nutrs) {
				ongoing_cnt = 1;
				break;
			}
			if (dir == READ || dir == WRITE) {
				if(is_send)
					mpbe_ufs_transfer_req_send(lrbp->task_tag, lrbp->cmd);
				else
					mpbe_ufs_transfer_req_compl(lrbp->task_tag, ongoing_cnt);
				mpbe_ufs_rq_check(lrbp->task_tag, ongoing_cnt);
			}
		}
		return 0;
	}

	return 1;
}
EXPORT_SYMBOL_GPL(mpbe_trace_ufs_cmd);

/* evaluate throughput and workload of given context */
static void mpbe_ufs_ctx_eval(struct mpbe_ufs_ctx *ctx)
{
	__u64 period;

	ctx->workload.usage = ctx->period_usage;

	if (ctx->workload.period > (ctx->workload.usage * 100)) {
		ctx->workload.percent = 1;
	} else {
		period = ctx->workload.period;
		DIV64_U64_ROUND_UP(period, 100);
		ctx->workload.percent =
			(__u32)ctx->workload.usage / (__u32)period;
	}
	mpbe_throughput_eval(&ctx->throughput);
}

/* print context to trace ring buffer */
static void mpbe_ufs_work(struct work_struct *work)
{
	struct mpbe_ringtrace *rt = MPBE_RT(ufs_mpbe);
	struct mpbe_ufs_ctx *ctx;
	struct mpbe_trace *tr;
	unsigned long flags;
	__u64 time;
	__u32 idx;

	if (!rt)
		return;

	for (idx = 0; idx < ufs_mpbe->ctx.count; idx++) {
		spin_lock_irqsave(&rt->lock, flags);
		tr = mpbe_curr_trace(rt);
		if (!tr) {
			spin_unlock_irqrestore(&rt->lock, flags);
			break;
		}
		memset(tr, 0, sizeof(struct mpbe_trace));
		tr->flags |= MPBE_TR_NOCLEAR;
		spin_unlock_irqrestore(&rt->lock, flags);

		ctx = &((struct mpbe_ufs_ctx *)MPBE_CTX(ufs_mpbe))[idx];
		spin_lock_irqsave(&ctx->lock, flags);
		time = sched_clock();
		if (time - ctx->period_start_t < MPBE_UFS_TRACE_LATENCY) {
			spin_unlock_irqrestore(&ctx->lock, flags);
			continue;
		}
		ctx->workload.period = time - ctx->period_start_t;

		tr->pid = 0;
		tr->qid = idx;
		tr->time = time;
		mpbe_ufs_ctx_eval(ctx);
		mpbe_vmstat_eval(&tr->vmstat);
		mpbe_cpu_eval(&tr->cpu);
		memcpy(&tr->throughput, &ctx->throughput,
			sizeof(struct mpbe_throughput));
		memcpy(&tr->workload, &ctx->workload, sizeof(struct mpbe_workload));

		ctx->period_start_t = tr->time;
		ctx->period_end_t = 0;
		ctx->period_usage = 0;
		memset(&ctx->throughput, 0, sizeof(struct mpbe_throughput));
		memset(&ctx->workload, 0, sizeof(struct mpbe_workload));
		spin_unlock_irqrestore(&ctx->lock, flags);

		spin_lock_irqsave(&rt->lock, flags);
		if (tr->flags & MPBE_TR_NOCLEAR) {
			tr->flags |= MPBE_TR_READY;
			mpbe_next_trace(rt);
		} else {
			memset(tr, 0, sizeof(struct mpbe_trace));
		}
		spin_unlock_irqrestore(&rt->lock, flags);
	}
}

static void mpbe_ufs_ctx_count_usage(struct mpbe_ufs_ctx *ctx,
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
void mpbe_ufs_rq_check(__u16 task_id, unsigned long req_mask)
{
	struct mpbe_ufs_ctx *ctx;
	__u64 end_time, period_time;
	unsigned long flags;

	ctx = mpbe_ufs_curr_ctx(task_id);
	if (!ctx)
		return;

	end_time = sched_clock();

	spin_lock_irqsave(&ctx->lock, flags);

	if (ctx->busy_start_t)
		mpbe_ufs_ctx_count_usage(ctx, ctx->busy_start_t, end_time);

	ctx->busy_start_t = (req_mask) ? end_time : 0;

	period_time = end_time - ctx->period_start_t;

	if (period_time >= MPBE_UFS_TRACE_LATENCY) {
		ctx->period_end_t = end_time;
		ctx->workload.period = period_time;
		queue_work(ufs_mpbe_wq, &ufs_mpbe_worker);
	}
	spin_unlock_irqrestore(&ctx->lock, flags);
}

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

static size_t mpbe_ufs_seq_debug_show_info(char **buff, unsigned long *size,
					       struct seq_file *seq)
{
	return 0;
}

static void mpbe_ufs_init_ctx(struct mpbe *btag)
{
	struct mpbe_ufs_ctx *ctx = MPBE_CTX(btag);
	__u64 time = sched_clock();
	int i;

	if (!ctx)
		return;

	memset(ctx, 0, sizeof(struct mpbe_ufs_ctx) * btag->ctx.count);
	for (i = 0; i < btag->ctx.count; i++) {
		spin_lock_init(&ctx[i].lock);
		ctx[i].period_start_t = time;
	}
}

static struct mpbe_vops mpbe_ufs_vops = {
	.seq_show = mpbe_ufs_seq_debug_show_info,
	.mictx_eval_wqd = mpbe_ufs_mictx_eval_wqd,
};

int mpbe_ufs_init(void)
{
	struct mpbe *btag;
	int max_queue = 1;

	mpbe_ufs_vops.earaio_enabled = true;

	mpbe_ufs_vops.boot_device = true;
	ufs_mpbe_wq = alloc_workqueue("ufs_mpbe",
			WQ_FREEZABLE | WQ_UNBOUND, 1);
	INIT_WORK(&ufs_mpbe_worker, mpbe_ufs_work);

	btag = mpbe_alloc("ufs",
			      MPBE_STORAGE_UFS,
			      MPBE_UFS_RINGBUF_MAX,
			      sizeof(struct mpbe_ufs_ctx),
			      max_queue, &mpbe_ufs_vops);

	if (btag) {
		mpbe_ufs_init_ctx(btag);
		ufs_mpbe = btag;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(mpbe_ufs_init);

int mpbe_ufs_exit(void)
{
	mpbe_free(ufs_mpbe);
	return 0;
}
EXPORT_SYMBOL_GPL(mpbe_ufs_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Xiaomi Memory perfomance Booster");
MODULE_AUTHOR("Tianyang Cao <caotianyang@xiaomi.com>");

