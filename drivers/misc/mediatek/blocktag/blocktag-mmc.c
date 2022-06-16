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
#include "blocktag-mmc.h"
#include "queue.h"
#include "mtk_blocktag.h"
#include "mtk-mmc.h"

/* ring trace for debugfs, eMMC & SD */
struct mtk_blocktag *mmc_mtk_btag;

static struct mmc_mtk_bio_context_task *mmc_mtk_bio_get_task(
	struct mmc_mtk_bio_context *ctx, unsigned int task_id)
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
	unsigned int task_id, struct mmc_mtk_bio_context **curr_ctx, bool is_sd)
{
	struct mmc_mtk_bio_context *ctx;

	ctx = mmc_mtk_bio_curr_ctx(is_sd);
	if (curr_ctx)
		*curr_ctx = ctx;
	return mmc_mtk_bio_get_task(ctx, task_id);
}

int mtk_btag_pidlog_add_mmc(struct request_queue *q, short pid,
	__u32 len, int rw, bool is_sd)
{
	unsigned long flags;
	struct mmc_mtk_bio_context *ctx;

	ctx = mmc_mtk_bio_curr_ctx(is_sd);
	if (!ctx)
		return 0;

	spin_lock_irqsave(&ctx->lock, flags);
	mtk_btag_pidlog_insert(&ctx->pidlog, abs(pid), len, rw);
	mtk_btag_mictx_eval_req(mmc_mtk_btag, rw, len >> 12, len,
				pid < 0 ? true : false);
	spin_unlock_irqrestore(&ctx->lock, flags);

	return 1;
}
EXPORT_SYMBOL_GPL(mtk_btag_pidlog_add_mmc);

static const char *task_name[tsk_max] = {
	"send_cmd", "req_compl"};

static void mmc_mtk_pr_tsk(struct mmc_mtk_bio_context_task *tsk,
	unsigned int stage)
{
	const char *rw = "?";
	int klogen = BTAG_KLOGEN(mmc_mtk_btag);
	char buf[256];
	__u32 bytes;

	if (!((klogen == 2 && stage == tsk_req_compl) || (klogen == 3)))
		return;

	if (tsk->dir)
		rw = "r";
	else
		rw = "w";

	bytes = ((__u32)tsk->len) << SECTOR_SHIFT;
	mtk_btag_task_timetag(buf, 256, stage, tsk_max, task_name, tsk->t,
		bytes);
}

void mmc_mtk_biolog_send_command(unsigned int task_id,
				 struct mmc_request *mrq)
{
	struct mmc_queue_req *mqrq;
	struct request *req;
	unsigned long flags;
	struct mmc_host *mmc = mrq->host;
	struct mmc_mtk_bio_context *ctx;
	struct mmc_mtk_bio_context_task *tsk;
	bool is_sd;

	if (!mrq || !mmc)
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
		mtk_btag_commit_req(req, is_sd);

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
	mtk_btag_mictx_update(mmc_mtk_btag, ctx->q_depth);

	spin_unlock_irqrestore(&ctx->lock, flags);

	mmc_mtk_pr_tsk(tsk, tsk_send_cmd);
}
EXPORT_SYMBOL_GPL(mmc_mtk_biolog_send_command);

static void mmc_mtk_bio_mictx_cnt_signle_wqd(
				struct mmc_mtk_bio_context_task *tsk,
				struct mtk_btag_mictx_struct *mictx,
				u64 t_cur)
{
	u64 t, t_begin;

	if (!mictx->enabled)
		return;

	t_begin = max_t(u64, mictx->window_begin,
			tsk->t[tsk_send_cmd]);

	if (tsk->t[tsk_req_compl])
		t = tsk->t[tsk_req_compl] - t_begin;
	else
		t = t_cur - t_begin;

	mictx->weighted_qd += t;
}

void mmc_mtk_biolog_transfer_req_compl(struct mmc_host *mmc,
	unsigned int task_id, unsigned long req_mask)
{
	struct mmc_mtk_bio_context *ctx;
	struct mmc_mtk_bio_context_task *tsk;
	struct mtk_btag_throughput_rw *tp = NULL;
	unsigned long flags;
	int rw = -1;
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
		rw = 0; /* READ */
		tp = &ctx->throughput.r;
	} else {
		rw = 1; /* WRITE */
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
		mtk_btag_mictx_eval_tp(mmc_mtk_btag, rw, busy_time,
				       size);
	}

	if (!req_mask)
		ctx->q_depth = 0;
	else
		ctx->q_depth--;
	mtk_btag_mictx_update(mmc_mtk_btag, ctx->q_depth);
	mmc_mtk_bio_mictx_cnt_signle_wqd(tsk, &mmc_mtk_btag->mictx, 0);

	/* clear this task */
	tsk->t[tsk_send_cmd] = tsk->t[tsk_req_compl] = 0;

	spin_unlock_irqrestore(&ctx->lock, flags);

	/*
	 * FIXME: tsk->t is cleared before so this would output
	 * wrongly.
	 */
	mmc_mtk_pr_tsk(tsk, tsk_req_compl);
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
	ctx->last_workload_percent = ctx->workload.percent;
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

/* CPU frequency adjust relate */
static void cpu_cluster_freq_tbl_init(void)
{
	unsigned int i;
	int cpu;
	struct cpufreq_policy *policy = NULL;

	/* query policy number */
	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);

		if (policy) {
			s_cluster_num++;
			cpu = cpumask_last(policy->related_cpus);
		}
	}

	if (s_cluster_num == 0) {
		pr_info("%s: spd: no policy for cpu", __func__);
		return;
	}
	s_tchbst_rq = kcalloc(s_cluster_num,
		sizeof(struct freq_qos_request), GFP_KERNEL);
	if (s_tchbst_rq == NULL)
		return;

	s_target_freq = kcalloc(s_cluster_num, sizeof(int), GFP_KERNEL);
	if (s_target_freq)
		for (i = 0; i < s_cluster_num; i++)
			s_target_freq[i] = -1;
	else {
		pr_info("%s: spd: s_target_freq fail\n", __func__);
		kfree(s_tchbst_rq);
		return;
	}

	i = 0;
	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);
		if (!policy)
			continue;
		if (i >= s_cluster_num) {
			pr_info("%s: spd: fail:i >= s_cluster_num\n", __func__);
			kfree(s_tchbst_rq);
			return;
		}
		freq_qos_add_request(&policy->constraints,
			&(s_tchbst_rq[i]), FREQ_QOS_MIN, 0);
		cpu = cpumask_last(policy->related_cpus);
		i++;
	}

	s_cluster_freq_rdy = 1;
}

void mtk_mmc_qos_cpu_cluster_freq_update(int freq[], unsigned int num)
{
	unsigned int min_num = s_cluster_num > num ? num : s_cluster_num;
	unsigned int i;
	char buf[256];
	int ret = 0;
	int ret_t[8];
	int update = 0;

	if (!s_cluster_freq_rdy) {
		pr_info("%s: spd: cpu cluster frequency not ready\n", __func__);
		return;
	}

	for (i = 0; i < min_num; i++) {
		ret_t[i] = 0;
		if (s_target_freq[i] != freq[i]) {
			s_target_freq[i] = freq[i];
			ret_t[i] =
				freq_qos_update_request(&s_tchbst_rq[i], s_target_freq[i]);
			update = 1;
		}
		ret += scnprintf(&buf[ret], sizeof(buf) - ret,
				"%d(%d),", freq[i], ret_t[i]);
	}
	if (update)
		pr_info("%s: spd: %s [%u:%u]\n", __func__, buf, num, min_num);
}

void set_mmc_perf_mode(struct mmc_host *mmc, bool boost)
{
	struct msdc_host *host = mmc_priv(mmc);

	if ((mmc->caps2 & MMC_CAP2_NO_SD) || !host->qos_enable)
		return;

	if ((boost && mmc_qos_enable) || (!boost && !mmc_qos_enable))
		return;

	if (boost) {
		if (host->bw_path)
			icc_set_bw(host->bw_path, 0, host->peak_bw);
		mtk_mmc_qos_cpu_cluster_freq_update(s_final_cpu_freq,
			MAX_CPU_CLUSTER);
		mmc_qos_enable = true;
	} else {
		if (host->bw_path)
			icc_set_bw(host->bw_path, 0, 0);
		mtk_mmc_qos_cpu_cluster_freq_update(s_free_cpu_freq,
			MAX_CPU_CLUSTER);
		mmc_qos_enable = false;
	}
}
EXPORT_SYMBOL_GPL(set_mmc_perf_mode);

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

	/* when io loading is heavy,enable mmc performance mode */
	if (ctx->last_workload_percent >= 90 && req_mask) {
		if (!s_cluster_freq_rdy)
			cpu_cluster_freq_tbl_init();
		set_mmc_perf_mode(mmc, true);
	}

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

	mtk_btag_klog(mmc_mtk_btag, tr);
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

void mmc_mtk_biolog_clk_gating(bool clk_on)
{
	if (!clk_on)
		mtk_btag_earaio_boost(clk_on);
}
EXPORT_SYMBOL_GPL(mmc_mtk_biolog_clk_gating);

static struct mtk_btag_vops mmc_mtk_btag_vops = {
	.seq_show       = mmc_mtk_bio_seq_debug_show_info,
};

int mmc_mtk_biolog_init(struct mmc_host *mmc)
{
	struct mtk_blocktag *btag;
	struct mmc_mtk_bio_context *ctx;

	if (!mmc)
		return -EINVAL;

	mmc_mtk_btag_vops.earaio_enabled = true;
	if (mmc_mtk_btag_vops.boot_device[BTAG_STORAGE_MMC])
		return 0;

	mmc_mtk_btag_vops.boot_device[BTAG_STORAGE_MMC] = true;

	btag = mtk_btag_alloc("mmc",
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
