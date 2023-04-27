// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2019 MediaTek Inc.
 */
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/blk-mq.h>
#include <uapi/linux/sched/types.h>
#include <mt-plat/mtk_blocktag.h>
#include "../core/queue.h"
#include "mtk-mmc-swcqhci-crypto.h"

static int swcq_enable(struct mmc_host *mmc, struct mmc_card *card)
{
	pr_info("%s", __func__);
	mmc->cqe_on = true;
	return 0;

}

static void swcq_off(struct mmc_host *mmc)
{
	mmc->cqe_on = false;
	pr_info("%s", __func__);
}

static void swcq_disable(struct mmc_host *mmc)
{
	mmc->cqe_on = false;
	pr_info("%s", __func__);
}

static void swcq_post_req(struct mmc_host *mmc, struct mmc_request *mrq)
{
	if (mmc->ops->post_req)
		mmc->ops->post_req(mmc, mrq, 0);
}

int swcq_done_task(struct mmc_host *mmc, int task_id)
{
	struct swcq_host *swcq_host = mmc->cqe_private;
	struct mmc_request *mrq = swcq_host->mrq[task_id];

	if (mrq->data->error) {
		dev_err(mmc_dev(mmc), "%s: task%d  data error %d\n",
			__func__, task_id, mrq->data->error);
		return mrq->data->error;
	}

	return 0;
}

int swcq_run_task(struct mmc_host *mmc, int task_id)
{
	struct mmc_command cmd = {0};
	struct mmc_request data_mrq = {0};
	struct swcq_host *swcq_host = mmc->cqe_private;
	struct mmc_request *mrq = swcq_host->mrq[task_id];
	int flags = mrq->data->flags & MMC_DATA_READ ? 1 : 0;
#ifdef CONFIG_MMC_CRYPTO
	int err;
#endif

	cmd.flags = MMC_RSP_R1 | MMC_CMD_AC;
	cmd.opcode = flags ? MMC_EXECUTE_READ_TASK : MMC_EXECUTE_WRITE_TASK;
	cmd.arg =  task_id << 16;

	data_mrq.cmd = &cmd;
	data_mrq.tag = mrq->tag;
	data_mrq.data = mrq->data;

#ifdef CONFIG_MMC_CRYPTO
	data_mrq.crypto_ctx = mrq->crypto_ctx;
	data_mrq.crypto_key_slot = mrq->crypto_key_slot;
	err = swcq_mmc_start_crypto(mmc, &data_mrq, cmd.opcode);
	if (err) {
		pr_info("eMMC crypto start fail %d\n", err);
		WARN_ON(1);
	}
#endif

	atomic_set(&swcq_host->ongoing_task.blksz, (mrq->data ? mrq->data->blocks : 0));
	mmc_wait_for_req(mmc, &data_mrq);
	if (cmd.error) {
		pr_err("%s: cmd%d error %d\n",
			__func__, cmd.opcode, cmd.error);
		return cmd.error;
	}

	return 0;
}

int swcq_set_task(struct mmc_host *mmc, int task_id)
{
	struct mmc_command cmd = {0};
	struct mmc_request pre_mrq = {0};
	struct swcq_host *swcq_host = mmc->cqe_private;
	struct mmc_request *mrq = swcq_host->mrq[task_id];
	int flags;
	int retry = 5;

	WARN_ON(!mrq);
	WARN_ON(!mrq->data);
	flags = mrq->data->flags & MMC_DATA_READ ? 1 : 0;
#if MMC_SWCQ_DEBUG
	dev_info(mmc_dev(mmc), "%s task_mrq[%d]=%08x, %s", __func__, task_id,
		swcq_host->mrq[task_id], flags ? "read" : "write");
#endif

	while (retry--) {
		cmd.flags = MMC_RSP_R1 | MMC_CMD_AC;
		cmd.opcode = MMC_QUE_TASK_PARAMS;
		cmd.arg =  flags << 30 | task_id << 16 | mrq->data->blocks;
		pre_mrq.cmd = &cmd;
		pre_mrq.tag = mrq->tag;

		mmc->ops->request(mmc, &pre_mrq);
		if (cmd.error) {
			dev_info(mmc_dev(mmc), "%s: cmd%d err =%d",
				__func__, cmd.opcode, cmd.error);
			continue;
		}

		memset(&cmd, 0, sizeof(cmd));
		cmd.flags = MMC_RSP_R1 | MMC_CMD_AC;
		cmd.opcode = MMC_QUE_TASK_ADDR;
		cmd.arg = mrq->data->blk_addr;
		pre_mrq.cmd = &cmd;
		pre_mrq.tag = mrq->tag;

		mmc->ops->request(mmc, &pre_mrq);
		if (cmd.error) {
			dev_info(mmc_dev(mmc), "%s: cmd%d err =%d",
				__func__, cmd.opcode, cmd.error);
			continue;
		}
		break;
	}

	if (cmd.error)
		return cmd.error;

	return 0;
}

int swcq_poll_task(struct mmc_host *mmc, u32 *status)
{
	struct mmc_command cmd = {0};
	struct mmc_request chk_mrq = {0};

	cmd.flags = MMC_RSP_R1 | MMC_CMD_AC;
	cmd.opcode = MMC_SEND_STATUS;
	cmd.arg = mmc->card->rca << 16 | 1 << 15;
	chk_mrq.cmd = &cmd;

	mmc->ops->request(mmc, &chk_mrq);
	if (cmd.error) {
		dev_info(mmc_dev(mmc), "%s: cmd%d err =%d", __func__, cmd.opcode, cmd.error);
		return cmd.error;
	}

	*status = cmd.resp[0];

	return 0;
}

void swcq_err_handle(struct mmc_host *mmc, int task_id, int step, int err_type)
{
	struct swcq_host *swcq_host = mmc->cqe_private;
	struct mmc_request *mrq = swcq_host->mrq[task_id];
	struct mmc_queue_req *mqrq = container_of(mrq, struct mmc_queue_req,
						  brq.mrq);
	struct request *req = mmc_queue_req_to_req(mqrq);
	struct request_queue *q = req->q;
	struct mmc_queue *mq = q->queuedata;
	unsigned long flags;
	// 1 means start recovery,  2 means recovery done
	int recovery_step = 0;
	bool in_recovery = false;

	WARN_ON(!mrq);
	swcq_host->ops->dump_info(mmc);

	while (1) {
		spin_lock_irqsave(&q->queue_lock, flags);
		in_recovery = mq->recovery_needed;
		spin_unlock_irqrestore(&q->queue_lock, flags);

		if (!in_recovery && mrq->recovery_notifier) {
			if (++recovery_step == 2)
				break;
			mrq->recovery_notifier(mrq);
		}

		msleep(20);
	}
}

#define	MMC_SWCQ_NONE       (0<<1)
#define	MMC_SWCQ_DONE       (1<<1)
#define MMC_SWCQ_RUN        (1<<2)
#define MMC_SWCQ_SET        (1<<3)
#define MMC_SWCQ_POLL       (1<<4)

static inline int get_step_of_swcq_host(struct swcq_host *swcq_host, int last_step)
{
	int seq = MMC_SWCQ_NONE;

	last_step = last_step << 1;
	if (last_step > MMC_SWCQ_POLL)
		last_step = MMC_SWCQ_NONE;
	switch (last_step) {
	case MMC_SWCQ_NONE:
	case MMC_SWCQ_DONE:
		if (atomic_read(&swcq_host->ongoing_task.done)) {
			seq = MMC_SWCQ_DONE;
			break;
		}
	case MMC_SWCQ_RUN:
		if (atomic_read(&swcq_host->ongoing_task.id) == MMC_SWCQ_TASK_IDLE
				&& swcq_host->rdy_tsks) {
			seq = MMC_SWCQ_RUN;
			break;
		}
	case MMC_SWCQ_SET:
		if (swcq_host->pre_tsks) {
			seq = MMC_SWCQ_SET;
			break;
		}
	case MMC_SWCQ_POLL:
		if (swcq_host->qnd_tsks != swcq_host->rdy_tsks
				&& atomic_read(&swcq_host->ongoing_task.id)
					== MMC_SWCQ_TASK_IDLE) {
			seq = MMC_SWCQ_POLL;
			break;
		}
	}
	return seq;
}

static inline void swcq_wait_trans_compl(struct mmc_host *mmc, int task_id, int step)
{
	u32 tmo = 0;
	u32 chk_q_cnt = 0;
	struct swcq_host *swcq_host = mmc->cqe_private;

	if (swcq_host->pre_tsks || swcq_tskid_idle(swcq_host) || swcq_tskdone(swcq_host))
		return;
	/* for 4k data, skip wait event to improve the performance */
	if (swcq_tskblksz(swcq_host) <= 8)
		return;

	chk_q_cnt = q_cnt(swcq_host);
	/* wait the data transfer complete */
	tmo = wait_event_interruptible_timeout(swcq_host->wait_dat_trans,
			swcq_tskdone(swcq_host) || (q_cnt(swcq_host) > chk_q_cnt),
			10 * HZ);
	if (!tmo) {
		dev_info(mmc_dev(mmc), "[%s]: tmo: task_id=%d chk_q_cnt=%d q_cnt=%d step=%d\n",
			__func__, task_id, chk_q_cnt, q_cnt(swcq_host), step);
		dev_info(mmc_dev(mmc), "[%s]: tmo: ongoing_task=(%d, %d) P=%08x Q=%08x R=%08x\n",
			__func__, swcq_tskid(swcq_host), swcq_tskdone(swcq_host),
			swcq_host->pre_tsks, swcq_host->qnd_tsks, swcq_host->rdy_tsks);
	}
}

int mmc_run_queue_thread(void *data)
{
	struct mmc_host *mmc = data;
	struct swcq_host *swcq_host = mmc->cqe_private;
	int err;
	int step = -1;
	int task_id = -1;
	struct mmc_request *done_mrq;
	struct mmc_request *mrq = NULL;
	// struct sched_param param = { .sched_priority = 1 };

	// sched_setscheduler(current, SCHED_FIFO, &param);

	while (1) {
		step = get_step_of_swcq_host(swcq_host, step);
#if MMC_SWCQ_DEBUG
		if (step)
			dev_info(mmc_dev(mmc), "%s: S%d C%d P%08x Q%08x R%08x T%d,D%d",
				__func__,
				step,
				atomic_read(&swcq_host->q_cnt),
				swcq_host->pre_tsks,
				swcq_host->qnd_tsks,
				swcq_host->rdy_tsks,
				atomic_read(&swcq_host->ongoing_task.id),
				atomic_read(&swcq_host->ongoing_task.done));
#endif
		switch (step) {
		case MMC_SWCQ_DONE:
			task_id = atomic_read(&swcq_host->ongoing_task.id);
			err = swcq_done_task(mmc, task_id);
			if (!err) {
				done_mrq = swcq_host->mrq[task_id];
				atomic_set(&swcq_host->ongoing_task.done, 0);
				atomic_set(&swcq_host->ongoing_task.id, MMC_SWCQ_TASK_IDLE);
				atomic_set(&swcq_host->ongoing_task.blksz, 0);
				swcq_host->mrq[task_id] = NULL;
				atomic_dec(&swcq_host->q_cnt);
#ifdef CONFIG_MMC_CRYPTO
				swcq_mmc_complete_mqr_crypto(mmc);
#endif
				if (!atomic_read(&swcq_host->q_cnt))
					wake_up_interruptible(&swcq_host->wait_cmdq_empty);
#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_TRACER)
				if (done_mrq->data) {
					if (done_mrq->data->error)
						done_mrq->data->bytes_xfered = 0;
					else
						done_mrq->data->bytes_xfered = done_mrq->data->blksz
							* done_mrq->data->blocks;
					mmc_mtk_biolog_transfer_req_compl(mmc, done_mrq->tag, 0);
					mmc_mtk_biolog_check(mmc, q_cnt(swcq_host));
				}
#endif
				mmc_cqe_request_done(mmc, done_mrq);
			} else {
				spin_lock(&swcq_host->lock);
				swcq_host->pre_tsks |= (1 << task_id);
				spin_unlock(&swcq_host->lock);
				goto SWCQ_ERR_HANDLE;
			}
			break;
		case MMC_SWCQ_RUN:
			task_id = ffs(swcq_host->rdy_tsks) - 1;
			atomic_set(&swcq_host->ongoing_task.id, task_id);
			err = swcq_run_task(mmc, task_id);
			if (err)
				goto SWCQ_ERR_HANDLE;

			swcq_host->rdy_tsks &= ~(1<<task_id);
			swcq_host->qnd_tsks &= ~(1<<task_id);
			break;
		case MMC_SWCQ_SET:
			spin_lock(&swcq_host->lock);
			task_id = ffs(swcq_host->pre_tsks) - 1;
			spin_unlock(&swcq_host->lock);
			if (task_id < 0)
				break;
			err = swcq_set_task(mmc, task_id);
			if (!err) {
				spin_lock(&swcq_host->lock);
				swcq_host->pre_tsks &= ~(1<<task_id);
				spin_unlock(&swcq_host->lock);
				swcq_host->qnd_tsks |= (1<<task_id);
			} else {
				mrq = swcq_host->mrq[task_id];
				if (mrq && mrq->cmd)
					dev_info(mmc_dev(mmc), "[%s]: set task error: %d, op: %u, arg: 0x%08x\n",
						__func__, err, mrq->cmd->opcode, mrq->cmd->arg);
				goto SWCQ_ERR_HANDLE;
			}
			break;
		case MMC_SWCQ_POLL:
			err = swcq_poll_task(mmc, &swcq_host->rdy_tsks);
			if (err) {
				task_id = ffs(swcq_host->qnd_tsks) - 1;
				goto SWCQ_ERR_HANDLE;
			}
			break;
		}

		set_current_state(TASK_INTERRUPTIBLE);
		if (atomic_read(&swcq_host->q_cnt) == 0) {
			step = MMC_SWCQ_NONE;
			schedule();
		}
		swcq_wait_trans_compl(mmc, task_id, step);

		set_current_state(TASK_RUNNING);
		if (kthread_should_stop())
			break;

		continue;
SWCQ_ERR_HANDLE:
		if (err) {
			dev_info(mmc_dev(mmc), "[%s]3: error: %d\n", __func__, err);
			swcq_err_handle(mmc, task_id, step, err);
		}
	}

	return 0;
}

static int swcq_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct swcq_host *swcq_host = mmc->cqe_private;
	unsigned long flags;

	if (mrq->data) {
		if (mrq->tag >= mmc->cqe_qdepth) {
			dev_dbg(mmc_dev(mmc), "%s should not issue tag >= %d req.",
				__func__, mmc->cqe_qdepth);
			return -EBUSY;
		}
		spin_lock_irqsave(&swcq_host->lock, flags);
		swcq_host->pre_tsks |= (1 << mrq->tag);
		swcq_host->mrq[mrq->tag] = mrq;
		atomic_inc(&swcq_host->q_cnt);
#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_TRACER)
		mmc_mtk_biolog_send_command(mrq->tag, mrq);
		mmc_mtk_biolog_check(mmc, q_cnt(swcq_host));
#endif
		spin_unlock_irqrestore(&swcq_host->lock, flags);
	} else {
		dev_info(mmc_dev(mmc), "%s should not issue non-data req.", __func__);
		WARN_ON(1);
		return -1;
	}

	wake_up_process(swcq_host->cmdq_thread);

	return 0;
}

static int swcq_wait_for_idle(struct mmc_host *mmc)
{
	struct swcq_host *swcq_host = mmc->cqe_private;

	while (atomic_read(&swcq_host->q_cnt)) {
		wait_event_interruptible(swcq_host->wait_cmdq_empty,
			atomic_read(&swcq_host->q_cnt) == 0);
	}

	return 0;
}

static bool swcq_timeout(struct mmc_host *mmc, struct mmc_request *mrq,
			  bool *recovery_needed)
{
	struct swcq_host *swcq_host = mmc->cqe_private;

	dev_info(mmc_dev(mmc), "%s: C%d P%08x Q%08x R%08x T%d,D%d",
		__func__,
		atomic_read(&swcq_host->q_cnt),
		swcq_host->pre_tsks,
		swcq_host->qnd_tsks,
		swcq_host->rdy_tsks,
		atomic_read(&swcq_host->ongoing_task.id),
		atomic_read(&swcq_host->ongoing_task.done));

	swcq_host->ops->dump_info(mmc);
	*recovery_needed = true;
	return true;

}

static void swcq_reset(struct swcq_host *swcq_host)
{
	int id = atomic_read(&swcq_host->ongoing_task.id);

	spin_lock(&swcq_host->lock);
	swcq_host->pre_tsks |= swcq_host->qnd_tsks | swcq_host->rdy_tsks;
	if (id != MMC_SWCQ_TASK_IDLE)
		swcq_host->pre_tsks |= (1<<id);
	swcq_host->qnd_tsks = 0;
	swcq_host->rdy_tsks = 0;
	spin_unlock(&swcq_host->lock);
	atomic_set(&swcq_host->ongoing_task.done, 0);
	atomic_set(&swcq_host->ongoing_task.id, MMC_SWCQ_TASK_IDLE);
	atomic_set(&swcq_host->ongoing_task.blksz, 0);
}

static void swcq_recovery_start(struct mmc_host *mmc)
{
	struct swcq_host *swcq_host = mmc->cqe_private;

	dev_info(mmc_dev(mmc), "SWCQ recovery start");
	if (swcq_host->ops->err_handle)
		swcq_host->ops->err_handle(mmc);
#if SWCQ_TUNING_CMD
	/* Maybe it's cmd crc error at this time and cmdq not empty,
	 * only cmd13 can be used for tuning.
	 */
	if (swcq_host->ops->prepare_tuning)
		swcq_host->ops->prepare_tuning(mmc);
	if (mmc->ops->execute_tuning)
		mmc->ops->execute_tuning(mmc, MMC_SEND_STATUS);
#endif

}

static void swcq_recovery_finish(struct mmc_host *mmc)
{
	struct swcq_host *swcq_host = mmc->cqe_private;

	swcq_reset(swcq_host);
	if (swcq_host->ops->prepare_tuning)
		swcq_host->ops->prepare_tuning(mmc);
	if (mmc->ops->execute_tuning)
		mmc->ops->execute_tuning(mmc, MMC_SEND_TUNING_BLOCK_HS200);

	dev_info(mmc_dev(mmc), "SWCQ recovery done");
}

static const struct mmc_cqe_ops swcq_ops = {
	.cqe_enable = swcq_enable,
	.cqe_disable = swcq_disable,
	.cqe_request = swcq_request,
	.cqe_post_req = swcq_post_req,
	.cqe_off = swcq_off,
	.cqe_wait_for_idle = swcq_wait_for_idle,
	.cqe_timeout = swcq_timeout,
	.cqe_recovery_start = swcq_recovery_start,
	.cqe_recovery_finish = swcq_recovery_finish,
};

int swcq_init(struct swcq_host *swcq_host, struct mmc_host *mmc)
{
	int err;

	swcq_host->mmc = mmc;
	mmc->cqe_private = swcq_host;
	mmc->cqe_ops = &swcq_ops;
	/*swcmdq not have DCMD*/
	mmc->cqe_qdepth = NUM_SLOTS;
	atomic_set(&swcq_host->ongoing_task.id, MMC_SWCQ_TASK_IDLE);
	atomic_set(&swcq_host->ongoing_task.blksz, 0);
	swcq_host->cmdq_thread = kthread_create(mmc_run_queue_thread, mmc,
				"mmc-swcq%d", mmc->index);

	if (IS_ERR(swcq_host->cmdq_thread)) {
		err = PTR_ERR(swcq_host->cmdq_thread);
		goto out_err;
	}

	spin_lock_init(&swcq_host->lock);
	init_waitqueue_head(&swcq_host->wait_cmdq_empty);
	init_waitqueue_head(&swcq_host->wait_dat_trans);
#ifdef CONFIG_MMC_CRYPTO
	err = swcq_mmc_init_crypto(mmc);
	if (err)
		goto out_err;
#endif
	dev_info(mmc_dev(mmc), "%s: swcq init done", mmc_hostname(mmc));

	return 0;

out_err:
	pr_err("%s: swcq failed to initialize, error %d\n",
	       mmc_hostname(mmc), err);
	return err;
}
EXPORT_SYMBOL_GPL(swcq_init);

MODULE_AUTHOR("Gray Jia <Gray.Jia@mediatek.com>");
MODULE_DESCRIPTION("Software Command Queue Host Controller Interface driver");
MODULE_LICENSE("GPL v2");
