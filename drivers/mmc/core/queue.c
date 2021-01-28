/*
 *  Copyright (C) 2003 Russell King, All Rights Reserved.
 *  Copyright 2006-2007 Pierre Ossman
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/blkdev.h>
#include <linux/freezer.h>
#include <linux/kthread.h>
#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
#include <linux/delay.h>
#endif

#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/sched/rt.h>
#include <uapi/linux/sched/types.h>
#include <mt-plat/mtk_io_boost.h>

#include "mtk_mmc_block.h"
#include "queue.h"
#include "block.h"
#include "core.h"
#include "crypto.h"
#include "card.h"
#include "mmc_crypto.h"
#include "cqhci-crypto.h"

/*
 * Prepare a MMC request. This just filters out odd stuff.
 */
static int mmc_prep_request(struct request_queue *q, struct request *req)
{
	struct mmc_queue *mq = q->queuedata;

	if (mq && mmc_card_removed(mq->card))
		return BLKPREP_KILL;

	req->rq_flags |= RQF_DONTPREP;

	return BLKPREP_OK;
}

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
static void mmc_queue_softirq_done(struct request *req)
{
	blk_end_request_all(req, 0);
}

int mmc_is_cmdq_full(struct mmc_queue *mq, struct request *req)
{
	struct mmc_host *host;
	int cnt, rt;
	u8 cmp_depth;

	host = mq->card->host;
	rt = IS_RT_CLASS_REQ(req);

	cnt = atomic_read(&host->areq_cnt);
	cmp_depth = host->card->ext_csd.cmdq_depth;
	if (!rt &&
		cmp_depth > EMMC_MIN_RT_CLASS_TAG_COUNT)
		cmp_depth -= EMMC_MIN_RT_CLASS_TAG_COUNT;

	if (cnt >= cmp_depth)
		return 1;

	return 0;
}
#endif

#ifdef CONFIG_MTK_EMMC_HW_CQ
static struct request *mmc_peek_request(struct mmc_queue *mq)
{
	struct request_queue *q = mq->queue;

	mq->cmdq_req_peeked = NULL;

	spin_lock_irq(q->queue_lock);
	if (!blk_queue_stopped(q))
		mq->cmdq_req_peeked = blk_peek_request(q);
	spin_unlock_irq(q->queue_lock);

	return mq->cmdq_req_peeked;
}

static bool mmc_check_blk_queue_start_tag(struct request_queue *q,
	struct request *req)
{
	int ret;

	spin_lock_irq(q->queue_lock);
	ret = blk_queue_start_tag(q, req);
	spin_unlock_irq(q->queue_lock);

	return !!ret;
}

static bool mmc_check_blk_queue_start(struct mmc_cmdq_context_info *ctx,
	struct mmc_queue *mq)
{
	struct request_queue *q = mq->queue;

	if (!test_bit(CMDQ_STATE_ERR, &ctx->curr_state)
		&& !mmc_check_blk_queue_start_tag(q, mq->cmdq_req_peeked))
		return true;

	return false;
}

static inline void mmc_cmdq_ready_wait(struct mmc_host *host,
	struct mmc_queue *mq)
{
	struct mmc_cmdq_context_info *ctx = &host->cmdq_ctx;

	/*
	 * Wait until all of the following conditions are true:
	 * 1. There is a request pending in the block layer queue
	 *    to be processed.
	 * 2. If the peeked request is flush/discard then there shouldn't
	 *    be any other direct command active.
	 * 3. cmdq state should be unhalted.
	 * 4. cmdq state shouldn't be in error state.
	 * 5. free tag available to process the new request.
	 */
	wait_event(ctx->wait, kthread_should_stop()
		|| (!test_bit(CMDQ_STATE_DCMD_ACTIVE, &ctx->curr_state)
		&& mmc_peek_request(mq)
		&& ((!(!host->card->part_curr && !mmc_card_suspended(host->card)
			 && mmc_host_halt(host))
		&& !(!host->card->part_curr && mmc_host_cq_disable(host) &&
			!mmc_card_suspended(host->card)))
			|| (host->claimed && host->claimer != current))
		&& mmc_check_blk_queue_start(ctx, mq)));
}

static int mmc_cmdq_thread(void *d)
{
	struct mmc_queue *mq = d;
	struct mmc_card *card = mq->card;
	struct mmc_host *host = card->host;
	struct sched_param scheduler_params = {0};

	scheduler_params.sched_priority = 1;
	sched_setscheduler(current, SCHED_FIFO, &scheduler_params);

	current->flags |= PF_MEMALLOC;

	mt_bio_queue_alloc(current, NULL);

	while (1) {
		int ret = 0;

		mmc_cmdq_ready_wait(host, mq);
		if (kthread_should_stop())
			break;

		mt_biolog_cqhci_check();

		ret = mmc_cmdq_down_rwsem(host, mq->cmdq_req_peeked);
		if (ret) {
			mmc_cmdq_up_rwsem(host);
			continue;
		}

		ret = mq->cmdq_issue_fn(mq, mq->cmdq_req_peeked);
		mmc_cmdq_up_rwsem(host);
		/*
		 * Don't requeue if issue_fn fails, just bug on.
		 * We don't expect failure here and there is no recovery other
		 * than fixing the actual issue if there is any.
		 * Also we end the request if there is a partition switch error,
		 * so we should not requeue the request here.
		 */
	} /* loop */

	mt_bio_queue_free(current);

	return 0;
}

static void mmc_cmdq_dispatch_req(struct request_queue *q)
{
	struct mmc_queue *mq = q->queuedata;

	wake_up(&mq->card->host->cmdq_ctx.wait);
}
#endif

static int mmc_queue_thread(void *d)
{
	struct mmc_queue *mq = d;
	struct request_queue *q = mq->queue;
	struct mmc_context_info *cntx = &mq->card->host->context_info;
	struct sched_param scheduler_params = {0};

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	int cmdq_full = 0;
	unsigned int tmo;
#endif
	bool part_cmdq_en = false;

	scheduler_params.sched_priority = 1;

	sched_setscheduler(current, SCHED_FIFO, &scheduler_params);

	current->flags |= PF_MEMALLOC;

	down(&mq->thread_sem);
	mt_bio_queue_alloc(current, q);

#if defined(CONFIG_MTK_IO_BOOST)
	mtk_iobst_register_tid(current->pid);
#endif

	do {
		struct request *req;

		spin_lock_irq(q->queue_lock);
		set_current_state(TASK_INTERRUPTIBLE);

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
		req = blk_peek_request(q);
		if (!req)
			goto fetch_done;

		part_cmdq_en = mmc_blk_part_cmdq_en(mq);
		if (part_cmdq_en && mmc_is_cmdq_full(mq, req)) {
			req = NULL;
			cmdq_full = 1;
			goto fetch_done;
		}
#endif

		req = blk_fetch_request(q);

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
fetch_done:
#endif
		mq->asleep = false;
		cntx->is_waiting_last_req = false;
		cntx->is_new_req = false;
		if (!req) {
			/*
			 * Dispatch queue is empty so set flags for
			 * mmc_request_fn() to wake us up.
			 */
			if (atomic_read(&mq->qcnt))
				cntx->is_waiting_last_req = true;
			else
				mq->asleep = true;
		}

		spin_unlock_irq(q->queue_lock);

		if (req || (!part_cmdq_en && atomic_read(&mq->qcnt))) {
			set_current_state(TASK_RUNNING);
			mmc_blk_issue_rq(mq, req);
			cond_resched();
		} else {
			if (kthread_should_stop()) {
				set_current_state(TASK_RUNNING);
				break;
			}
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
			if (!cmdq_full) {
				/* no request */
				up(&mq->thread_sem);
				schedule();
				down(&mq->thread_sem);
			} else {
				/* queue full */
				cmdq_full = 0;
				/* wait when queue full */
				tmo = schedule_timeout(HZ);
				if (!tmo)
					pr_info("%s:sched_tmo,areq_cnt=%d\n",
						__func__,
					atomic_read(&mq->card->host->areq_cnt));
			}

#else
			up(&mq->thread_sem);
			schedule();
			down(&mq->thread_sem);
#endif

		}
	} while (1);
	mt_bio_queue_free(current);
	up(&mq->thread_sem);

	return 0;
}

/*
 * Generic MMC request handler.  This is called for any queue on a
 * particular host.  When the host is not busy, we look for a request
 * on any queue on this host, and attempt to issue it.  This may
 * not be the queue we were asked to process.
 */
static void mmc_request_fn(struct request_queue *q)
{
	struct mmc_queue *mq = q->queuedata;
	struct request *req;
	struct mmc_context_info *cntx;

	if (!mq) {
		while ((req = blk_fetch_request(q)) != NULL) {
			req->rq_flags |= RQF_QUIET;
			__blk_end_request_all(req, BLK_STS_IOERR);
		}
		return;
	}

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	/* just wake up thread for cmdq */
	if (mmc_blk_part_cmdq_en(mq)) {
		wake_up_process(mq->thread);
		return;
	}
#endif

	cntx = &mq->card->host->context_info;
	if (cntx->is_waiting_last_req) {
		cntx->is_new_req = true;
		wake_up_interruptible(&cntx->wait);
	}

	if (mq->asleep)
		wake_up_process(mq->thread);
}

static struct scatterlist *mmc_alloc_sg(int sg_len, gfp_t gfp)
{
	struct scatterlist *sg;

	sg = kmalloc_array(sg_len, sizeof(*sg), gfp);
	if (sg)
		sg_init_table(sg, sg_len);

	return sg;
}

static void mmc_queue_setup_discard(struct request_queue *q,
				    struct mmc_card *card)
{
	unsigned max_discard;

	max_discard = mmc_calc_max_discard(card);
	if (!max_discard)
		return;

	queue_flag_set_unlocked(QUEUE_FLAG_DISCARD, q);
	blk_queue_max_discard_sectors(q, max_discard);
	q->limits.discard_granularity = card->pref_erase << 9;
	/* granularity must not be greater than max. discard */
	if (card->pref_erase > max_discard)
		q->limits.discard_granularity = 0;
	if (mmc_can_secure_erase_trim(card))
		queue_flag_set_unlocked(QUEUE_FLAG_SECERASE, q);
}

/**
 * mmc_init_request() - initialize the MMC-specific per-request data
 * @q: the request queue
 * @req: the request
 * @gfp: memory allocation policy
 */
static int mmc_init_request(struct request_queue *q, struct request *req,
			    gfp_t gfp)
{
	struct mmc_queue_req *mq_rq = req_to_mmc_queue_req(req);
	struct mmc_queue *mq;
	struct mmc_card *card;
	struct mmc_host *host;

	/* add "if" to fix the error condition:
	 * STEP 1:remove sdcard call mmc_cleanup_queue,
	 * get queue_lock, then queuedata = NULL, put queue_lock;
	 * STEP 2:generic_make _request call blk_queue_bio,
	 * get queue_lock, then call get_request, mempool_alloc,
	 * alloc_request_size, mmc_init_request, queuedata is NULL
	 * in this time.
	 * STEP 3: null pointer exception.
	 */
	if (q->queuedata)
		mq = q->queuedata;
	else
		return -ENODEV;

	card = mq->card;
	host = card->host;

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	/* cmdq use preallocate sg buffer */
	if (mmc_blk_part_cmdq_en(mq))
		return 0;
#endif

	mq_rq->sg = mmc_alloc_sg(host->max_segs, gfp);
	if (!mq_rq->sg)
		return -ENOMEM;

	return 0;
}

static void mmc_exit_request(struct request_queue *q, struct request *req)
{
	struct mmc_queue_req *mq_rq = req_to_mmc_queue_req(req);

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	/* cmdq use preallocate sg buffer */
	if (q->queuedata &&
		mmc_blk_part_cmdq_en(q->queuedata))
		return;
#endif

	kfree(mq_rq->sg);
	mq_rq->sg = NULL;
}

#ifdef CONFIG_MTK_EMMC_HW_CQ
/*
 * mmc_blk_cmdq_setup_queue
 * @mq: mmc queue
 * @card: card to attach to this queue
 *
 * Setup queue for CMDQ supporting MMC card
 */
void mmc_cmdq_setup_queue(struct mmc_queue *mq, struct mmc_card *card)
{
	u64 limit = BLK_BOUNCE_HIGH;
	struct mmc_host *host = card->host;

	if (mmc_dev(host)->dma_mask && *mmc_dev(host)->dma_mask)
		limit = *mmc_dev(host)->dma_mask;

	queue_flag_set_unlocked(QUEUE_FLAG_NONROT, mq->queue);
	if (mmc_can_erase(card))
		mmc_queue_setup_discard(mq->queue, card);

	blk_queue_bounce_limit(mq->queue, limit);
	blk_queue_max_hw_sectors(mq->queue, min(host->max_blk_count,
						host->max_req_size / 512));
	blk_queue_max_segment_size(mq->queue, host->max_seg_size);
	blk_queue_max_segments(mq->queue, host->max_segs);
}
#endif

/**
 * mmc_init_queue - initialise a queue structure.
 * @mq: mmc queue
 * @card: mmc card to attach this queue
 * @lock: queue lock
 * @subname: partition subname
 * @area_type: eMMC area type for cmdq use
 * Initialise a MMC card request queue.
 */
int mmc_init_queue(struct mmc_queue *mq, struct mmc_card *card,
		   spinlock_t *lock, const char *subname, int area_type)
{
	struct mmc_host *host = card->host;
	u64 limit = BLK_BOUNCE_HIGH;
	int ret = -ENOMEM;
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	int i;
#endif

	if (mmc_dev(host)->dma_mask && *mmc_dev(host)->dma_mask)
		limit = (u64)dma_max_pfn(mmc_dev(host)) << PAGE_SHIFT;

	mq->card = card;

#if defined(CONFIG_MTK_EMMC_CQ_SUPPORT) || defined(CONFIG_MTK_EMMC_HW_CQ)
	if (card->ext_csd.cmdq_support &&
		(area_type == MMC_BLK_DATA_AREA_MAIN)) {
#ifdef CONFIG_MTK_EMMC_HW_CQ
		/* for cqe */
		if (host->caps2 & MMC_CAP2_CQE) {
			pr_notice("%s: init cqhci\n", mmc_hostname(host));
			mq->queue = blk_alloc_queue(GFP_KERNEL);
			if (!mq->queue)
				return -ENOMEM;
			mq->queue->queue_lock = lock;
			mq->queue->request_fn = mmc_cmdq_dispatch_req;
			mq->queue->cmd_size = sizeof(struct mmc_queue_req);
			mq->queue->queuedata = mq;
			ret = blk_init_allocated_queue(mq->queue);
			if (ret) {
				blk_cleanup_queue(mq->queue);
				return ret;
			}

			mmc_cmdq_setup_queue(mq, card);
			ret = mmc_cmdq_init(mq, card);
			if (ret) {
				pr_notice("%s: %d: cmdq: unable to set-up\n",
					mmc_hostname(host), ret);
				blk_cleanup_queue(mq->queue);
			} else {
				sema_init(&mq->thread_sem, 1);
				/* hook for pm qos cmdq init */
				if (card->host->cmdq_ops->init)
					card->host->cmdq_ops->init(host);
				mq->thread = kthread_run(mmc_cmdq_thread, mq,
					"mmc-cmdqd/%d%s",
					host->index,
					subname ? subname : "");
				if (IS_ERR(mq->thread)) {
					pr_notice("%s: %d: cmdq: failed to start mmc-cmdqd thread\n",
						mmc_hostname(host), ret);
					ret = PTR_ERR(mq->thread);
				}
				/* inline crypto */
				mmc_crypto_setup_queue(host, mq->queue);

				return ret;
			}
		}
#endif
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
		if (!(host->caps2 & MMC_CAP2_CQE)) {
			pr_notice("%s: init cq\n", mmc_hostname(host));
			atomic_set(&host->cq_rw, false);
			atomic_set(&host->cq_w, false);
			atomic_set(&host->cq_wait_rdy, 0);
			host->wp_error = 0;
			host->task_id_index = 0;
			atomic_set(&host->is_data_dma, 0);
			host->cur_rw_task = CQ_TASK_IDLE;
			atomic_set(&host->cq_tuning_now, 0);

			for (i = 0; i < EMMC_MAX_QUEUE_DEPTH; i++) {
				host->data_mrq_queued[i] = false;
				atomic_set(&mq->mqrq[i].index, 0);
			}

			host->cmdq_thread = kthread_run(mmc_run_queue_thread,
				host,
				"exe_cq/%d", host->index);
			if (IS_ERR(host->cmdq_thread)) {
				pr_notice("%s: cmdq: failed to start exe_cq thread\n",
					mmc_hostname(host));
			}
		}
#endif
	}
#endif

	mq->queue = blk_alloc_queue(GFP_KERNEL);
	if (!mq->queue)
		return -ENOMEM;
	mq->queue->queue_lock = lock;
	mq->queue->request_fn = mmc_request_fn;
	mq->queue->init_rq_fn = mmc_init_request;
	mq->queue->exit_rq_fn = mmc_exit_request;

	mq->queue->cmd_size = sizeof(struct mmc_queue_req);
	mq->queue->queuedata = mq;
	mq->queue->backing_dev_info->ra_pages = 128;
	atomic_set(&mq->qcnt, 0);
	ret = blk_init_allocated_queue(mq->queue);
	if (ret) {
		blk_cleanup_queue(mq->queue);
		return ret;
	}

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	if (mmc_card_mmc(card)) {
		for (i = 0; i < card->ext_csd.cmdq_depth; i++)
			atomic_set(&mq->mqrq[i].index, 0);
	}
#endif

	blk_queue_prep_rq(mq->queue, mmc_prep_request);
	queue_flag_set_unlocked(QUEUE_FLAG_NONROT, mq->queue);
	queue_flag_clear_unlocked(QUEUE_FLAG_ADD_RANDOM, mq->queue);
	if (mmc_can_erase(card))
		mmc_queue_setup_discard(mq->queue, card);

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	blk_queue_softirq_done(mq->queue, mmc_queue_softirq_done);
#endif

	blk_queue_bounce_limit(mq->queue, limit);
	blk_queue_max_hw_sectors(mq->queue,
		min(host->max_blk_count, host->max_req_size / 512));
	blk_queue_max_segments(mq->queue, host->max_segs);
	blk_queue_max_segment_size(mq->queue, host->max_seg_size);

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	if (mmc_card_mmc(card)) {
		for (i = 0; i < card->ext_csd.cmdq_depth; i++) {
			mq->mqrq[i].sg = mmc_alloc_sg(host->max_segs,
				GFP_KERNEL);
			if (!mq->mqrq[i].sg)
				goto cleanup_queue;
		}
	}
#endif

	sema_init(&mq->thread_sem, 1);

	/* sw-cqhci inline crypto */
	mmc_crypto_setup_queue(host, mq->queue);

	mq->thread = kthread_run(mmc_queue_thread, mq, "mmcqd/%d%s",
		host->index, subname ? subname : "");

	if (IS_ERR(mq->thread)) {
		ret = PTR_ERR(mq->thread);
		goto cleanup_queue;
	}

	return 0;

cleanup_queue:
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	if (mmc_card_mmc(card)) {
		for (i = 0; i < card->ext_csd.cmdq_depth; i++) {
			kfree(mq->mqrq[i].sg);
			mq->mqrq[i].sg = NULL;
		}
	}
#endif

	blk_cleanup_queue(mq->queue);
	return ret;
}

void mmc_cleanup_queue(struct mmc_queue *mq)
{
	struct request_queue *q = mq->queue;
	unsigned long flags;


	/* Make sure the queue isn't suspended, as that will deadlock */
	mmc_queue_resume(mq);

	/* Then terminate our worker thread */
	kthread_stop(mq->thread);

	/* Empty the queue */
	spin_lock_irqsave(q->queue_lock, flags);
	q->queuedata = NULL;
	blk_start_queue(q);
	spin_unlock_irqrestore(q->queue_lock, flags);

	if (likely(!blk_queue_dead(q)))
		blk_cleanup_queue(q);

	mq->card = NULL;
}
EXPORT_SYMBOL(mmc_cleanup_queue);

#ifdef CONFIG_MTK_EMMC_HW_CQ
static void mmc_cmdq_softirq_done(struct request *rq)
{
	struct mmc_queue *mq = rq->q->queuedata;

	mq->cmdq_complete_fn(rq);
}

static void mmc_cmdq_error_work(struct work_struct *work)
{
	struct mmc_queue *mq = container_of(work, struct mmc_queue,
					    cmdq_err_work);

	mq->cmdq_error_fn(mq);
}

enum blk_eh_timer_return mmc_cmdq_rq_timed_out(struct request *req)
{
	struct mmc_queue *mq = req->q->queuedata;

	pr_notice("%s: request with tag: %d flags: 0x%x timed out\n",
	       mmc_hostname(mq->card->host), req->tag, req->cmd_flags);

	return mq->cmdq_req_timed_out(req);
}

int mmc_cmdq_init(struct mmc_queue *mq, struct mmc_card *card)
{
	int i, ret = 0;
	/* one slot is reserved for dcmd requests */
	int q_depth = card->ext_csd.cmdq_depth - 1;

	card->cqe_init = false;
	if (!(card->host->caps2 & MMC_CAP2_CQE)) {
		ret = -ENOTSUPP;
		goto out;
	}

	init_waitqueue_head(&card->host->cmdq_ctx.queue_empty_wq);
	init_waitqueue_head(&card->host->cmdq_ctx.wait);
	init_rwsem(&card->host->cmdq_ctx.err_rwsem);

	mq->mqrq_cmdq = kcalloc(q_depth,
			sizeof(struct mmc_queue_req), GFP_KERNEL);
	if (!mq->mqrq_cmdq) {
		/* mark for check patch */
		/* pr_notice("%s: unable to alloc mqrq's for q_depth %d\n",
		 *	mmc_card_name(card), q_depth);
		 */
		ret = -ENOMEM;
		goto out;
	}

	/* sg is allocated for data request slots only */
	for (i = 0; i < q_depth; i++) {
		mq->mqrq_cmdq[i].sg =
			mmc_alloc_sg(card->host->max_segs, GFP_KERNEL);
		if (mq->mqrq_cmdq[i].sg == NULL) {
			pr_notice("%s: unable to allocate cmdq sg of size %d\n",
				mmc_card_name(card),
				card->host->max_segs);
			goto free_mqrq_sg;
		}
	}

	ret = blk_queue_init_tags(mq->queue, q_depth, NULL, BLK_TAG_ALLOC_FIFO);
	if (ret) {
		pr_notice("%s: unable to allocate cmdq tags %d\n",
				mmc_card_name(card), q_depth);
		goto free_mqrq_sg;
	}

	blk_queue_softirq_done(mq->queue, mmc_cmdq_softirq_done);
	INIT_WORK(&mq->cmdq_err_work, mmc_cmdq_error_work);
	init_completion(&mq->cmdq_shutdown_complete);
	init_completion(&mq->cmdq_pending_req_done);

	blk_queue_rq_timed_out(mq->queue, mmc_cmdq_rq_timed_out);
	blk_queue_rq_timeout(mq->queue, 120 * HZ);
	card->cqe_init = true;

	goto out;

free_mqrq_sg:
	for (i = 0; i < q_depth; i++)
		kfree(mq->mqrq_cmdq[i].sg);
	kfree(mq->mqrq_cmdq);
	mq->mqrq_cmdq = NULL;
out:
	return ret;
}

void mmc_cmdq_clean(struct mmc_queue *mq, struct mmc_card *card)
{
	int i;
	int q_depth = card->ext_csd.cmdq_depth - 1;

	blk_free_tags(mq->queue->queue_tags);
	mq->queue->queue_tags = NULL;
	blk_queue_free_tags(mq->queue);

	for (i = 0; i < q_depth; i++)
		kfree(mq->mqrq_cmdq[i].sg);
	kfree(mq->mqrq_cmdq);
	mq->mqrq_cmdq = NULL;
}
#endif

/*
 * mmc_queue_suspend - suspend a MMC request queue
 * @mq: MMC queue to suspend
 * @wait: Wait till MMC request queue is empty
 *
 * Stop the block request queue, and wait for our thread to
 * complete any outstanding requests.  This ensures that we
 * won't suspend while a request is being processed.
 */

#ifdef CONFIG_MTK_EMMC_HW_CQ
int mmc_queue_suspend(struct mmc_queue *mq, int wait)
{
	struct request_queue *q = mq->queue;
	unsigned long flags;
	int rc = 0;
	struct mmc_card *card = mq->card;
	struct request *req;

	if (card->cqe_init && blk_queue_tagged(q)) {
		struct mmc_host *host = card->host;

		if (test_and_set_bit(MMC_QUEUE_SUSPENDED, &mq->flags))
			goto out;

		if (wait) {
			/*
			 * After blk_stop_queue is called, wait for all
			 * active_reqs to complete.
			 * Then wait for cmdq thread to exit before calling
			 * cmdq shutdown to avoid race between issuing
			 * requests and shutdown of cmdq.
			 */
			spin_lock_irqsave(q->queue_lock, flags);
			blk_stop_queue(q);
			spin_unlock_irqrestore(q->queue_lock, flags);

			if (host->cmdq_ctx.active_reqs)
				wait_for_completion(
						&mq->cmdq_shutdown_complete);
			kthread_stop(mq->thread);
			mq->cmdq_shutdown(mq);
		} else {
			spin_lock_irqsave(q->queue_lock, flags);
			blk_stop_queue(q);
			wake_up(&host->cmdq_ctx.wait);
			req = blk_peek_request(q);
			if (req || mq->cmdq_req_peeked ||
			    host->cmdq_ctx.active_reqs) {
				clear_bit(MMC_QUEUE_SUSPENDED, &mq->flags);
				blk_start_queue(q);
				rc = -EBUSY;
			}
			spin_unlock_irqrestore(q->queue_lock, flags);
		}

		goto out;
	}

	/* non-cq case */
	if (!(test_and_set_bit(MMC_QUEUE_SUSPENDED, &mq->flags))) {
		spin_lock_irqsave(q->queue_lock, flags);
		blk_stop_queue(q);
		spin_unlock_irqrestore(q->queue_lock, flags);

		down(&mq->thread_sem);
		rc = 0;
	}
out:
	return rc;
}

/*
 * mmc_queue_resume - resume a previously suspended MMC request queue
 * @mq: MMC queue to resume
 */
void mmc_queue_resume(struct mmc_queue *mq)
{
	struct request_queue *q = mq->queue;
	struct mmc_card *card = mq->card;
	unsigned long flags;

	if (test_and_clear_bit(MMC_QUEUE_SUSPENDED, &mq->flags)) {
		if (!(card->cqe_init && blk_queue_tagged(q)))
			up(&mq->thread_sem);

		spin_lock_irqsave(q->queue_lock, flags);
		blk_start_queue(q);
		spin_unlock_irqrestore(q->queue_lock, flags);
	}
}
#else

/**
 * mmc_queue_suspend - suspend a MMC request queue
 * @mq: MMC queue to suspend
 *
 * Stop the block request queue, and wait for our thread to
 * complete any outstanding requests.  This ensures that we
 * won't suspend while a request is being processed.
 */
void mmc_queue_suspend(struct mmc_queue *mq)
{
	struct request_queue *q = mq->queue;
	unsigned long flags;

	if (!mq->suspended) {
		mq->suspended |= true;

		spin_lock_irqsave(q->queue_lock, flags);
		blk_stop_queue(q);
		spin_unlock_irqrestore(q->queue_lock, flags);

		down(&mq->thread_sem);
	}
}

/**
 * mmc_queue_resume - resume a previously suspended MMC request queue
 * @mq: MMC queue to resume
 */
void mmc_queue_resume(struct mmc_queue *mq)
{
	struct request_queue *q = mq->queue;
	unsigned long flags;

	if (mq->suspended) {
		mq->suspended = false;

		up(&mq->thread_sem);

		spin_lock_irqsave(q->queue_lock, flags);
		blk_start_queue(q);
		spin_unlock_irqrestore(q->queue_lock, flags);
	}
}
#endif

#ifdef CONFIG_MTK_EMMC_HW_CQ
/*
 * Prepare the sg list(s) to be handed of to the cmdq host driver
 */
unsigned int mmc_cmdq_queue_map_sg(struct mmc_queue *mq,
	struct mmc_queue_req *mqrq)
{
	struct request *req = mqrq->req;

	return blk_rq_map_sg(mq->queue, req, mqrq->sg);
}
#endif
/*
 * Prepare the sg list(s) to be handed of to the host driver
 */
unsigned int mmc_queue_map_sg(struct mmc_queue *mq, struct mmc_queue_req *mqrq)
{
	struct request *req = mmc_queue_req_to_req(mqrq);

	return blk_rq_map_sg(mq->queue, req, mqrq->sg);
}
