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
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
#include <linux/delay.h>
#endif

#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/sched/rt.h>
#include <mt-plat/mtk_io_boost.h>

#include "queue.h"
#include "block.h"
#include "mtk_mmc_block.h"

#define MMC_QUEUE_BOUNCESZ	65536

/*
 * Prepare a MMC request. This just filters out odd stuff.
 */
static int mmc_prep_request(struct request_queue *q, struct request *req)
{
	struct mmc_queue *mq = q->queuedata;

	/*
	 * We only like normal block requests and discards.
	 */
	if (req->cmd_type != REQ_TYPE_FS && req_op(req) != REQ_OP_DISCARD &&
	    req_op(req) != REQ_OP_SECURE_ERASE) {
		blk_dump_rq_flags(req, "MMC bad request");
		return BLKPREP_KILL;
	}

	if (mq && (mmc_card_removed(mq->card) || mmc_access_rpmb(mq)))
		return BLKPREP_KILL;

	req->cmd_flags |= REQ_DONTPREP;

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

static void mmc_cmdq_set_active(struct mmc_cmdq_context_info *ctx, bool en)
{
	if (en)
		set_bit(CMDQ_STATE_FETCH_QUEUE, &ctx->curr_state);
	else
		clear_bit(CMDQ_STATE_FETCH_QUEUE, &ctx->curr_state);
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

	/*
	 * Set fetch queue flag before check error state.
	 * This is to prevent when error occurs after error check
	 * and the request is not issue at LLD.
	 */
	mmc_cmdq_set_active(ctx, true);

	if (!test_bit(CMDQ_STATE_ERR, &ctx->curr_state)
		&& !mmc_check_blk_queue_start_tag(q, mq->cmdq_req_peeked))
		return true;

	mmc_cmdq_set_active(ctx, false);

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
	struct mmc_cmdq_context_info *ctx = &host->cmdq_ctx;
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

		ret = mq->cmdq_issue_fn(mq, mq->cmdq_req_peeked);
		mmc_cmdq_set_active(ctx, false);
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
	struct sched_param scheduler_params = {0};

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	int cmdq_full = 0;
	unsigned int tmo;
#endif
	bool part_cmdq_en = false;

	/*
	 * WARNING: Should remove this if cmdq is disable, otherwise ANR will
	 * happen because hw operation maybe blocking cpu/rt thread schedule.
	 */
	scheduler_params.sched_priority = 1;
	sched_setscheduler(current, SCHED_FIFO, &scheduler_params);

	current->flags |= PF_MEMALLOC;

	down(&mq->thread_sem);
	mt_bio_queue_alloc(current, q);

	mtk_iobst_register_tid(current->pid);

	do {
		struct request *req = NULL;

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
		if (!part_cmdq_en)
			mq->mqrq_cur->req = req;
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
fetch_done:
#endif
		spin_unlock_irq(q->queue_lock);

		if (req || (!part_cmdq_en && mq->mqrq_prev->req)) {
			bool req_is_special = mmc_req_is_special(req);

			set_current_state(TASK_RUNNING);
			mmc_blk_issue_rq(mq, req);
			cond_resched();
			if (test_and_clear_bit(MMC_QUEUE_NEW_REQUEST,
				&mq->flags))
				continue; /* fetch again */

			if (!part_cmdq_en) {
				/*
				 * Current request becomes previous request
				 * and vice versa.
				 * In case of special requests, current request
				 * has been finished. Do not assign it to
				 * previous request.
				 */
				if (req_is_special)
					mq->mqrq_cur->req = NULL;

				mq->mqrq_prev->brq.mrq.data = NULL;
				mq->mqrq_prev->req = NULL;
				swap(mq->mqrq_prev, mq->mqrq_cur);
			}
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
	unsigned long flags;
	struct mmc_context_info *cntx;

	if (!mq) {
		while ((req = blk_fetch_request(q)) != NULL) {
			req->cmd_flags |= REQ_QUIET;
			__blk_end_request_all(req, -EIO);
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
	if (!mq->mqrq_cur->req && mq->mqrq_prev->req) {
		/*
		 * New MMC request arrived when MMC thread may be
		 * blocked on the previous request to be complete
		 * with no current request fetched
		 */
		spin_lock_irqsave(&cntx->lock, flags);
		if (cntx->is_waiting_last_req) {
			cntx->is_new_req = true;
			wake_up_interruptible(&cntx->wait);
		}
		spin_unlock_irqrestore(&cntx->lock, flags);
	} else if (!mq->mqrq_cur->req && !mq->mqrq_prev->req)
		wake_up_process(mq->thread);
}

static struct scatterlist *mmc_alloc_sg(int sg_len, int *err)
{
	struct scatterlist *sg;

	sg = kmalloc(sizeof(struct scatterlist)*sg_len, GFP_KERNEL);
	if (!sg)
		*err = -ENOMEM;
	else {
		*err = 0;
		sg_init_table(sg, sg_len);
	}

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
	if (card->erased_byte == 0 && !mmc_can_discard(card))
		q->limits.discard_zeroes_data = 1;
	q->limits.discard_granularity = card->pref_erase << 9;
	/* granularity must not be greater than max. discard */
	if (card->pref_erase > max_discard)
		q->limits.discard_granularity = 0;
	if (mmc_can_secure_erase_trim(card))
		queue_flag_set_unlocked(QUEUE_FLAG_SECERASE, q);
}

#ifdef CONFIG_MTK_EMMC_HW_CQ
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
#endif

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
	int ret;
#if defined(CONFIG_MTK_EMMC_CQ_SUPPORT)
	int i;
#endif
	struct mmc_queue_req *mqrq_cur = &mq->mqrq[0];
	struct mmc_queue_req *mqrq_prev = &mq->mqrq[1];

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
			mq->queue = blk_init_queue(mmc_cmdq_dispatch_req, lock);
			if (!mq->queue)
				return -ENOMEM;
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
				mq->queue->queuedata = mq;
				mq->thread = kthread_run(mmc_cmdq_thread, mq,
					"mmc-cmdqd/%d%s",
					host->index,
					subname ? subname : "");
				if (IS_ERR(mq->thread)) {
					pr_notice("%s: %d: cmdq: failed to start mmc-cmdqd thread\n",
						mmc_hostname(host), ret);
					ret = PTR_ERR(mq->thread);
				}

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
			host->is_data_dma = 0;
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

	mq->queue = blk_init_queue(mmc_request_fn, lock);
	if (!mq->queue)
		return -ENOMEM;

	mq->mqrq_cur = mqrq_cur;
	mq->mqrq_prev = mqrq_prev;
	mq->queue->queuedata = mq;

	blk_queue_prep_rq(mq->queue, mmc_prep_request);
	queue_flag_set_unlocked(QUEUE_FLAG_NONROT, mq->queue);
	queue_flag_clear_unlocked(QUEUE_FLAG_ADD_RANDOM, mq->queue);
	if (mmc_can_erase(card))
		mmc_queue_setup_discard(mq->queue, card);

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	blk_queue_softirq_done(mq->queue, mmc_queue_softirq_done);
#endif

#ifdef CONFIG_MMC_BLOCK_BOUNCE
	if (host->max_segs == 1) {
		unsigned int bouncesz;

		bouncesz = MMC_QUEUE_BOUNCESZ;

		if (bouncesz > host->max_req_size)
			bouncesz = host->max_req_size;
		if (bouncesz > host->max_seg_size)
			bouncesz = host->max_seg_size;
		if (bouncesz > (host->max_blk_count * 512))
			bouncesz = host->max_blk_count * 512;

		if (bouncesz > 512) {
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
			if (mmc_card_mmc(card)) {
				for (i = 0; i < card->ext_csd.cmdq_depth; i++) {
					mq->mqrq[i].bounce_buf =
						kmalloc(bouncesz, GFP_KERNEL);
					if (!mq->mqrq[i].bounce_buf) {
						pr_warn(
			"%s: unable to allocate bounce cur buffer [%d]\n",
							mmc_card_name(card), i);
					}
				}
			}
#endif

			mqrq_cur->bounce_buf = kmalloc(bouncesz, GFP_KERNEL);
			if (!mqrq_cur->bounce_buf) {
				/* no need print any thing. */
			} else {
				mqrq_prev->bounce_buf =
						kmalloc(bouncesz, GFP_KERNEL);
				if (!mqrq_prev->bounce_buf) {
					pr_warn(
			"%s: unable to allocate bounce prev buffer\n",
						mmc_card_name(card));
					kfree(mqrq_cur->bounce_buf);
					mqrq_cur->bounce_buf = NULL;
				}
			}
		}

		if (mqrq_cur->bounce_buf && mqrq_prev->bounce_buf) {
			blk_queue_bounce_limit(mq->queue, BLK_BOUNCE_ANY);
			blk_queue_max_hw_sectors(mq->queue, bouncesz / 512);
			blk_queue_max_segments(mq->queue, bouncesz / 512);
			blk_queue_max_segment_size(mq->queue, bouncesz);

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
			if (mmc_card_mmc(card)) {
				for (i = 0; i < card->ext_csd.cmdq_depth; i++) {
					mq->mqrq[i].sg = mmc_alloc_sg(1, &ret);
					if (ret)
						goto cleanup_queue;

					mq->mqrq[i].bounce_sg =
						mmc_alloc_sg(bouncesz / 512,
						&ret);
					if (ret)
						goto cleanup_queue;
				}
			} else {
#endif

			mqrq_cur->sg = mmc_alloc_sg(1, &ret);
			if (ret)
				goto cleanup_queue;

			mqrq_cur->bounce_sg =
				mmc_alloc_sg(bouncesz / 512, &ret);
			if (ret)
				goto cleanup_queue;

			mqrq_prev->sg = mmc_alloc_sg(1, &ret);
			if (ret)
				goto cleanup_queue;

			mqrq_prev->bounce_sg =
				mmc_alloc_sg(bouncesz / 512, &ret);
			if (ret)
				goto cleanup_queue;
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
			}
#endif
		}
	}
#endif

	if (!mqrq_cur->bounce_buf && !mqrq_prev->bounce_buf) {
		blk_queue_bounce_limit(mq->queue, limit);
		blk_queue_max_hw_sectors(mq->queue,
			min(host->max_blk_count, host->max_req_size / 512));
		blk_queue_max_segments(mq->queue, host->max_segs);
		blk_queue_max_segment_size(mq->queue, host->max_seg_size);

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
		if (mmc_card_mmc(card)) {
			for (i = 0; i < card->ext_csd.cmdq_depth; i++) {
				mq->mqrq[i].sg = mmc_alloc_sg(host->max_segs,
					&ret);
				if (ret)
					goto cleanup_queue;
			}
		} else {
#endif

			mqrq_cur->sg = mmc_alloc_sg(host->max_segs, &ret);
			if (ret)
				goto cleanup_queue;


		mqrq_prev->sg = mmc_alloc_sg(host->max_segs, &ret);
		if (ret)
			goto cleanup_queue;
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
		}
#endif
	}

	sema_init(&mq->thread_sem, 1);

	mq->thread = kthread_run(mmc_queue_thread, mq, "mmcqd/%d%s",
		host->index, subname ? subname : "");

	if (IS_ERR(mq->thread)) {
		ret = PTR_ERR(mq->thread);
		goto free_bounce_sg;
	}

	return 0;
 free_bounce_sg:

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	if (mmc_card_mmc(card)) {
		for (i = 0; i < card->ext_csd.cmdq_depth; i++) {
			kfree(mq->mqrq[i].bounce_sg);
			mq->mqrq[i].bounce_sg = NULL;
		}
	} else {
#endif

		kfree(mqrq_cur->bounce_sg);
		mqrq_cur->bounce_sg = NULL;
		kfree(mqrq_prev->bounce_sg);
		mqrq_prev->bounce_sg = NULL;

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	}
#endif

 cleanup_queue:
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	if (mmc_card_mmc(card)) {
		for (i = 0; i < card->ext_csd.cmdq_depth; i++) {
			kfree(mq->mqrq[i].sg);
			mq->mqrq[i].sg = NULL;
			kfree(mq->mqrq[i].bounce_buf);
			mq->mqrq[i].bounce_buf = NULL;
		}
	} else {
#endif
		kfree(mqrq_cur->sg);
		mqrq_cur->sg = NULL;
		kfree(mqrq_cur->bounce_buf);
		mqrq_cur->bounce_buf = NULL;

	kfree(mqrq_prev->sg);
	mqrq_prev->sg = NULL;
	kfree(mqrq_prev->bounce_buf);
	mqrq_prev->bounce_buf = NULL;
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	}
#endif

	blk_cleanup_queue(mq->queue);
	return ret;
}

void mmc_cleanup_queue(struct mmc_queue *mq)
{
	struct request_queue *q = mq->queue;
	unsigned long flags;
	struct mmc_queue_req *mqrq_cur = mq->mqrq_cur;
	struct mmc_queue_req *mqrq_prev = mq->mqrq_prev;

	/* Make sure the queue isn't suspended, as that will deadlock */
	mmc_queue_resume(mq);

	/* Then terminate our worker thread */
	kthread_stop(mq->thread);

	/* Empty the queue */
	spin_lock_irqsave(q->queue_lock, flags);
	q->queuedata = NULL;
	blk_start_queue(q);
	spin_unlock_irqrestore(q->queue_lock, flags);

	kfree(mqrq_cur->bounce_sg);
	mqrq_cur->bounce_sg = NULL;

	kfree(mqrq_cur->sg);
	mqrq_cur->sg = NULL;

	kfree(mqrq_cur->bounce_buf);
	mqrq_cur->bounce_buf = NULL;

	kfree(mqrq_prev->bounce_sg);
	mqrq_prev->bounce_sg = NULL;

	kfree(mqrq_prev->sg);
	mqrq_prev->sg = NULL;

	kfree(mqrq_prev->bounce_buf);
	mqrq_prev->bounce_buf = NULL;

	mq->card = NULL;
}
EXPORT_SYMBOL(mmc_cleanup_queue);

int mmc_packed_init(struct mmc_queue *mq, struct mmc_card *card)
{
	struct mmc_queue_req *mqrq_cur = &mq->mqrq[0];
	struct mmc_queue_req *mqrq_prev = &mq->mqrq[1];
	int ret = 0;


	mqrq_cur->packed = kzalloc(sizeof(struct mmc_packed), GFP_KERNEL);
	if (!mqrq_cur->packed) {
		pr_warn("%s: unable to allocate packed cmd for mqrq_cur\n",
			mmc_card_name(card));
		ret = -ENOMEM;
		goto out;
	}

	mqrq_prev->packed = kzalloc(sizeof(struct mmc_packed), GFP_KERNEL);
	if (!mqrq_prev->packed) {
		pr_warn("%s: unable to allocate packed cmd for mqrq_prev\n",
			mmc_card_name(card));
		kfree(mqrq_cur->packed);
		mqrq_cur->packed = NULL;
		ret = -ENOMEM;
		goto out;
	}

	INIT_LIST_HEAD(&mqrq_cur->packed->list);
	INIT_LIST_HEAD(&mqrq_prev->packed->list);

out:
	return ret;
}

void mmc_packed_clean(struct mmc_queue *mq)
{
	struct mmc_queue_req *mqrq_cur = &mq->mqrq[0];
	struct mmc_queue_req *mqrq_prev = &mq->mqrq[1];

	kfree(mqrq_cur->packed);
	mqrq_cur->packed = NULL;
	kfree(mqrq_prev->packed);
	mqrq_prev->packed = NULL;
}

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

	pr_notice("%s: request with tag: %d flags: 0x%llx timed out\n",
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

	mq->mqrq_cmdq = kzalloc(
			sizeof(struct mmc_queue_req) * q_depth, GFP_KERNEL);
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
		mq->mqrq_cmdq[i].sg = mmc_alloc_sg(card->host->max_segs, &ret);
		if (ret) {
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

/**
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

	if (!(test_and_set_bit(MMC_QUEUE_SUSPENDED, &mq->flags))) {

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

	if (test_and_clear_bit(MMC_QUEUE_SUSPENDED, &mq->flags)) {

		up(&mq->thread_sem);

		spin_lock_irqsave(q->queue_lock, flags);
		blk_start_queue(q);
		spin_unlock_irqrestore(q->queue_lock, flags);
	}
}
#endif

static unsigned int mmc_queue_packed_map_sg(struct mmc_queue *mq,
					    struct mmc_packed *packed,
					    struct scatterlist *sg,
					    enum mmc_packed_type cmd_type)
{
	struct scatterlist *__sg = sg;
	unsigned int sg_len = 0;
	struct request *req;

	if (mmc_packed_wr(cmd_type)) {
		unsigned int hdr_sz = mmc_large_sector(mq->card) ? 4096 : 512;
		unsigned int max_seg_sz = queue_max_segment_size(mq->queue);
		unsigned int len, remain, offset = 0;
		u8 *buf = (u8 *)packed->cmd_hdr;

		remain = hdr_sz;
		do {
			len = min(remain, max_seg_sz);
			sg_set_buf(__sg, buf + offset, len);
			offset += len;
			remain -= len;
			sg_unmark_end(__sg++);
			sg_len++;
		} while (remain);
	}

	list_for_each_entry(req, &packed->list, queuelist) {
		sg_len += blk_rq_map_sg(mq->queue, req, __sg);
		__sg = sg + (sg_len - 1);
		sg_unmark_end(__sg++);
	}
	sg_mark_end(sg + (sg_len - 1));
	return sg_len;
}

/*
 * Prepare the sg list(s) to be handed of to the host driver
 */
unsigned int mmc_queue_map_sg(struct mmc_queue *mq, struct mmc_queue_req *mqrq)
{
	unsigned int sg_len;
	size_t buflen;
	struct scatterlist *sg;
	enum mmc_packed_type cmd_type;
	int i;

	cmd_type = mqrq->cmd_type;

	if (!mqrq->bounce_buf) {
		if (mmc_packed_cmd(cmd_type))
			return mmc_queue_packed_map_sg(mq, mqrq->packed,
						       mqrq->sg, cmd_type);
		else
			return blk_rq_map_sg(mq->queue, mqrq->req, mqrq->sg);
	}

	BUG_ON(!mqrq->bounce_sg);

	if (mmc_packed_cmd(cmd_type))
		sg_len = mmc_queue_packed_map_sg(mq, mqrq->packed,
						 mqrq->bounce_sg, cmd_type);
	else
		sg_len = blk_rq_map_sg(mq->queue, mqrq->req, mqrq->bounce_sg);

	mqrq->bounce_sg_len = sg_len;

	buflen = 0;
	for_each_sg(mqrq->bounce_sg, sg, sg_len, i)
		buflen += sg->length;

	sg_init_one(mqrq->sg, mqrq->bounce_buf, buflen);

	return 1;
}

/*
 * If writing, bounce the data to the buffer before the request
 * is sent to the host driver
 */
void mmc_queue_bounce_pre(struct mmc_queue_req *mqrq)
{
	if (!mqrq->bounce_buf)
		return;

	if (rq_data_dir(mqrq->req) != WRITE)
		return;

	sg_copy_to_buffer(mqrq->bounce_sg, mqrq->bounce_sg_len,
		mqrq->bounce_buf, mqrq->sg[0].length);
}

/*
 * If reading, bounce the data from the buffer after the request
 * has been handled by the host driver
 */
void mmc_queue_bounce_post(struct mmc_queue_req *mqrq)
{
	if (!mqrq->bounce_buf)
		return;

	if (rq_data_dir(mqrq->req) != READ)
		return;

	sg_copy_from_buffer(mqrq->bounce_sg, mqrq->bounce_sg_len,
		mqrq->bounce_buf, mqrq->sg[0].length);
}
