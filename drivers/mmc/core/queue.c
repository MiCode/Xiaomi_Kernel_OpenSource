/*
 *  linux/drivers/mmc/card/queue.c
 *
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
#include <linux/bitops.h>
#include <linux/delay.h>

#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/sched/rt.h>
#include <uapi/linux/sched/types.h>

#include "queue.h"
#include "block.h"
#include "core.h"
#include "crypto.h"
#include "card.h"

/*
 * Prepare a MMC request. This just filters out odd stuff.
 */
static int mmc_prep_request(struct request_queue *q, struct request *req)
{
	struct mmc_queue *mq = q->queuedata;

	if (mq && (mmc_card_removed(mq->card) || mmc_access_rpmb(mq)))
		return BLKPREP_KILL;

	req->rq_flags |= RQF_DONTPREP;

	return BLKPREP_OK;
}

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

static inline void mmc_cmdq_ready_wait(struct mmc_host *host,
					struct mmc_queue *mq)
{
	struct mmc_cmdq_context_info *ctx = &host->cmdq_ctx;
	struct request_queue *q = mq->queue;

	/*
	 * Wait until all of the following conditions are true:
	 * 1. There is a request pending in the block layer queue
	 *    to be processed.
	 * 2. If the peeked request is flush/discard then there shouldn't
	 *    be any other direct command active.
	 * 3. cmdq state should be unhalted.
	 * 4. cmdq state shouldn't be in error state.
	 * 5. There is no outstanding RPMB request pending.
	 * 6. free tag available to process the new request.
	 *    (This must be the last condtion to check)
	 */
	wait_event(ctx->wait, kthread_should_stop()
		|| (mmc_peek_request(mq) &&
		!(((req_op(mq->cmdq_req_peeked) == REQ_OP_FLUSH) ||
		   (req_op(mq->cmdq_req_peeked) == REQ_OP_DISCARD) ||
		   (req_op(mq->cmdq_req_peeked) == REQ_OP_SECURE_ERASE))
		  && test_bit(CMDQ_STATE_DCMD_ACTIVE, &ctx->curr_state))
		&& !(!host->card->part_curr && !mmc_card_suspended(host->card)
		     && mmc_host_halt(host))
		&& !(!host->card->part_curr && mmc_host_cq_disable(host) &&
			!mmc_card_suspended(host->card))
		&& !test_bit(CMDQ_STATE_ERR, &ctx->curr_state)
		&& !atomic_read(&host->rpmb_req_pending)
		&& !mmc_check_blk_queue_start_tag(q, mq->cmdq_req_peeked)));
}

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

	pr_err("%s: request with tag: %d flags: 0x%x timed out\n",
	       mmc_hostname(mq->card->host), req->tag, req->cmd_flags);

	return mq->cmdq_req_timed_out(req);
}
//#endif

static int mmc_cmdq_thread(void *d)
{
	struct mmc_queue *mq = d;
	struct mmc_card *card = mq->card;

	struct mmc_host *host = card->host;

	struct sched_param scheduler_params = {0};
	scheduler_params.sched_priority = 1;
	sched_setscheduler(current, SCHED_FIFO, &scheduler_params);

	current->flags |= PF_MEMALLOC;
	if (card->host->wakeup_on_idle)
		set_wake_up_idle(true);

	while (1) {
		int ret = 0;

		mmc_cmdq_ready_wait(host, mq);
		if (kthread_should_stop())
			break;

		ret = mmc_cmdq_down_rwsem(host, mq->cmdq_req_peeked);
		if (ret) {
			mmc_cmdq_up_rwsem(host);
			continue;
		}
		ret = mq->cmdq_issue_fn(mq, mq->cmdq_req_peeked);
		mmc_cmdq_up_rwsem(host);

		/*
		 * Don't requeue if issue_fn fails.
		 * Recovery will be come by completion softirq
		 * Also we end the request if there is a partition switch
		 * error, so we should not requeue the request here.
		 */
	} /* loop */

	return 0;
}

static void mmc_cmdq_dispatch_req(struct request_queue *q)
{
	struct mmc_queue *mq = q->queuedata;

	wake_up(&mq->card->host->cmdq_ctx.wait);
}

static void mmc_queue_setup_discard(struct request_queue *q,
				    struct mmc_card *card)
{
	unsigned int max_discard;

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

static struct scatterlist *mmc_alloc_sg(int sg_len, gfp_t gfp)
{
	struct scatterlist *sg;

	sg = kmalloc_array(sg_len, sizeof(*sg), gfp);
	if (sg)
		sg_init_table(sg, sg_len);

	return sg;
}

int mmc_cmdq_init(struct mmc_queue *mq, struct mmc_card *card)
{
	int ret = 0;
	/* one slot is reserved for dcmd requests */
	int q_depth = card->ext_csd.cmdq_depth - 1;

	card->cmdq_init = false;
	if (!(card->host->caps2 & MMC_CAP2_CMD_QUEUE)) {
		return -ENOTSUPP;
	}

	init_waitqueue_head(&card->host->cmdq_ctx.queue_empty_wq);
	init_waitqueue_head(&card->host->cmdq_ctx.wait);
	init_rwsem(&card->host->cmdq_ctx.err_rwsem);

	ret = blk_queue_init_tags(mq->queue, q_depth, NULL, BLK_TAG_ALLOC_FIFO);
	if (ret) {
		pr_warn("%s: unable to allocate cmdq tags %d\n",
				mmc_card_name(card), q_depth);
		return ret;
	}

	blk_queue_softirq_done(mq->queue, mmc_cmdq_softirq_done);
	INIT_WORK(&mq->cmdq_err_work, mmc_cmdq_error_work);
	init_completion(&mq->cmdq_shutdown_complete);
	init_completion(&mq->cmdq_pending_req_done);

	blk_queue_rq_timed_out(mq->queue, mmc_cmdq_rq_timed_out);
	blk_queue_rq_timeout(mq->queue, 120 * HZ);
	card->cmdq_init = true;

	return ret;
}

void mmc_cmdq_clean(struct mmc_queue *mq, struct mmc_card *card)
{
	blk_free_tags(mq->queue->queue_tags);
	mq->queue->queue_tags = NULL;
	blk_queue_free_tags(mq->queue);
}

static int mmc_queue_thread(void *d)
{
	struct mmc_queue *mq = d;
	struct request_queue *q = mq->queue;
	struct mmc_context_info *cntx = &mq->card->host->context_info;
	struct sched_param scheduler_params = {0};

	scheduler_params.sched_priority = 1;

	sched_setscheduler(current, SCHED_FIFO, &scheduler_params);

	current->flags |= PF_MEMALLOC;

	down(&mq->thread_sem);
	do {
		struct request *req;

		spin_lock_irq(q->queue_lock);
		set_current_state(TASK_INTERRUPTIBLE);
		req = blk_fetch_request(q);
		mq->asleep = false;
		cntx->is_waiting_last_req = false;
		cntx->is_new_req = false;
		if (!req) {
			/*
			 * Dispatch queue is empty so set flags for
			 * mmc_request_fn() to wake us up.
			 */
			if (mq->qcnt)
				cntx->is_waiting_last_req = true;
			else
				mq->asleep = true;
		}
		spin_unlock_irq(q->queue_lock);

		if (req || mq->qcnt) {
			set_current_state(TASK_RUNNING);
			mmc_blk_issue_rq(mq, req);
			cond_resched();
		} else {
			if (kthread_should_stop()) {
				set_current_state(TASK_RUNNING);
				break;
			}
			up(&mq->thread_sem);
			schedule();
			down(&mq->thread_sem);
		}
	} while (1);
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

	cntx = &mq->card->host->context_info;

	if (cntx->is_waiting_last_req) {
		cntx->is_new_req = true;
		wake_up_interruptible(&cntx->wait);
	}

	if (mq->asleep)
		wake_up_process(mq->thread);
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
	struct mmc_queue *mq = q->queuedata;
	struct mmc_host *host;

	if (!mq)
		return -ENODEV;

	host = mq->card->host;

	mq_rq->sg = mmc_alloc_sg(host->max_segs, gfp);
	if (!mq_rq->sg)
		return -ENOMEM;

	return 0;
}

static void mmc_exit_request(struct request_queue *q, struct request *req)
{
	struct mmc_queue_req *mq_rq = req_to_mmc_queue_req(req);

	kfree(mq_rq->sg);
	mq_rq->sg = NULL;
}

/**
 * mmc_init_queue - initialise a queue structure.
 * @mq: mmc queue
 * @card: mmc card to attach this queue
 * @lock: queue lock
 * @subname: partition subname
 *
 * Initialise a MMC card request queue.
 */
int mmc_init_queue(struct mmc_queue *mq, struct mmc_card *card,
		   spinlock_t *lock, const char *subname, int area_type)
{
	struct mmc_host *host = card->host;
	u64 limit = BLK_BOUNCE_HIGH;
	int ret = -ENOMEM;

	if (mmc_dev(host)->dma_mask && *mmc_dev(host)->dma_mask)
		limit = (u64)dma_max_pfn(mmc_dev(host)) << PAGE_SHIFT;

	mq->card = card;
	if (card->ext_csd.cmdq_support &&
	    (area_type == MMC_BLK_DATA_AREA_MAIN)) {
		mq->queue = blk_alloc_queue(GFP_KERNEL);
		if (!mq->queue)
			return -ENOMEM;
		if (lock)
			mq->queue->queue_lock = lock;
		mq->queue->request_fn = mmc_cmdq_dispatch_req;
		mq->queue->init_rq_fn = mmc_init_request;
		mq->queue->exit_rq_fn = mmc_exit_request;
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
			pr_err("%s: %d: cmdq: unable to set-up\n",
			       mmc_hostname(card->host), ret);
			blk_cleanup_queue(mq->queue);
		} else {
			sema_init(&mq->thread_sem, 1);
			/* hook for pm qos cmdq init */
			if (card->host->cmdq_ops->init)
				card->host->cmdq_ops->init(card->host);
			if (host->cmdq_ops->cqe_crypto_update_queue)
				host->cmdq_ops->cqe_crypto_update_queue(host,
								mq->queue);
			mq->thread = kthread_run(mmc_cmdq_thread, mq,
						 "mmc-cmdqd/%d%s",
						 host->index,
						 subname ? subname : "");
			if (IS_ERR(mq->thread)) {
				pr_err("%s: %d: cmdq: failed to start mmc-cmdqd thread\n",
					mmc_hostname(card->host), ret);
				ret = PTR_ERR(mq->thread);
			}

			return ret;
		}
	}

	mq->queue = blk_alloc_queue(GFP_KERNEL);
	if (!mq->queue)
		return -ENOMEM;
	if (lock)
		mq->queue->queue_lock = lock;
	mq->queue->request_fn = mmc_request_fn;
	mq->queue->init_rq_fn = mmc_init_request;
	mq->queue->exit_rq_fn = mmc_exit_request;
	mq->queue->cmd_size = sizeof(struct mmc_queue_req);
	mq->queue->queuedata = mq;
	mq->qcnt = 0;
	ret = blk_init_allocated_queue(mq->queue);
	if (ret) {
		blk_cleanup_queue(mq->queue);
		return ret;
	}

	blk_queue_prep_rq(mq->queue, mmc_prep_request);
	queue_flag_set_unlocked(QUEUE_FLAG_NONROT, mq->queue);
	queue_flag_clear_unlocked(QUEUE_FLAG_ADD_RANDOM, mq->queue);
	if (mmc_can_erase(card))
		mmc_queue_setup_discard(mq->queue, card);

	blk_queue_bounce_limit(mq->queue, limit);
	blk_queue_max_hw_sectors(mq->queue,
		min(host->max_blk_count, host->max_req_size / 512));
	blk_queue_max_segments(mq->queue, host->max_segs);
	blk_queue_max_segment_size(mq->queue, host->max_seg_size);

	sema_init(&mq->thread_sem, 1);

	mq->thread = kthread_run(mmc_queue_thread, mq, "mmcqd/%d%s",
		host->index, subname ? subname : "");

	if (IS_ERR(mq->thread)) {
		ret = PTR_ERR(mq->thread);
		goto cleanup_queue;
	}

	mmc_crypto_setup_queue(host, mq->queue);
	return 0;

 cleanup_queue:
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

/**
 * mmc_queue_suspend - suspend a MMC request queue
 * @mq: MMC queue to suspend
 * @wait: Wait till MMC request queue is empty
 *
 * Stop the block request queue, and wait for our thread to
 * complete any outstanding requests.  This ensures that we
 * won't suspend while a request is being processed.
 */
int mmc_queue_suspend(struct mmc_queue *mq, int wait)
{
	struct request_queue *q = mq->queue;
	unsigned long flags;
	int rc = 0;
	struct mmc_card *card = mq->card;
	struct request *req;

	if (card->cmdq_init && blk_queue_tagged(q)) {
		struct mmc_host *host = card->host;

		if (test_and_set_bit(MMC_QUEUE_SUSPENDED, &mq->flags))
			goto out;

		if (wait) {

			/*
			 * After blk_cleanup_queue is called, wait for all
			 * active_reqs to complete.
			 * Then wait for cmdq thread to exit before calling
			 * cmdq shutdown to avoid race between issuing
			 * requests and shutdown of cmdq.
			 */
			blk_cleanup_queue(q);

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

	if (!(test_and_set_bit(MMC_QUEUE_SUSPENDED, &mq->flags))) {
		if (!wait) {
			/* suspend/stop the queue in case of suspend */
			spin_lock_irqsave(q->queue_lock, flags);
			blk_stop_queue(q);
			spin_unlock_irqrestore(q->queue_lock, flags);
		} else {
			/* shutdown the queue in case of shutdown/reboot */
			blk_cleanup_queue(q);
		}

		rc = down_trylock(&mq->thread_sem);
		if (rc && !wait) {
			/*
			 * Failed to take the lock so better to abort the
			 * suspend because mmcqd thread is processing requests.
			 */
			clear_bit(MMC_QUEUE_SUSPENDED, &mq->flags);
			spin_lock_irqsave(q->queue_lock, flags);
			blk_start_queue(q);
			spin_unlock_irqrestore(q->queue_lock, flags);
			rc = -EBUSY;
		} else if (rc && wait) {
			down(&mq->thread_sem);
			rc = 0;
		}
	}
out:
	return rc;
}

/**
 * mmc_queue_resume - resume a previously suspended MMC request queue
 * @mq: MMC queue to resume
 */
void mmc_queue_resume(struct mmc_queue *mq)
{
	struct request_queue *q = mq->queue;
	struct mmc_card *card = mq->card;
	unsigned long flags;

	if (test_and_clear_bit(MMC_QUEUE_SUSPENDED, &mq->flags)) {

		if (!(card->cmdq_init && blk_queue_tagged(q)))
			up(&mq->thread_sem);

		spin_lock_irqsave(q->queue_lock, flags);
		blk_start_queue(q);
		spin_unlock_irqrestore(q->queue_lock, flags);
	}
}

/*
 * Prepare the sg list(s) to be handed of to the host driver
 */
unsigned int mmc_queue_map_sg(struct mmc_queue *mq, struct mmc_queue_req *mqrq)
{
	struct request *req = mmc_queue_req_to_req(mqrq);

	return blk_rq_map_sg(mq->queue, req, mqrq->sg);
}
