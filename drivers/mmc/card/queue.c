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
#include <linux/bitops.h>

#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include "queue.h"

#define MMC_QUEUE_BOUNCESZ	65536


/*
 * Based on benchmark tests the default num of requests to trigger the write
 * packing was determined, to keep the read latency as low as possible and
 * manage to keep the high write throughput.
 */
#define DEFAULT_NUM_REQS_TO_START_PACK 17

/*
 * Prepare a MMC request. This just filters out odd stuff.
 */
static int mmc_prep_request(struct request_queue *q, struct request *req)
{
	struct mmc_queue *mq = q->queuedata;

	/*
	 * We only like normal block requests and discards.
	 */
	if (req->cmd_type != REQ_TYPE_FS && !(req->cmd_flags & REQ_DISCARD)) {
		blk_dump_rq_flags(req, "MMC bad request");
		return BLKPREP_KILL;
	}

	if (mq && mmc_card_removed(mq->card))
		return BLKPREP_KILL;

	req->cmd_flags |= REQ_DONTPREP;

	return BLKPREP_OK;
}

static int mmc_queue_thread(void *d)
{
	struct mmc_queue *mq = d;
	struct request_queue *q = mq->queue;
	struct mmc_card *card = mq->card;

	current->flags |= PF_MEMALLOC;

	down(&mq->thread_sem);
	do {
		struct mmc_queue_req *tmp;
		struct request *req = NULL;

		spin_lock_irq(q->queue_lock);
		set_current_state(TASK_INTERRUPTIBLE);
		req = blk_fetch_request(q);
		mq->mqrq_cur->req = req;
		spin_unlock_irq(q->queue_lock);

		if (req || mq->mqrq_prev->req) {
			set_current_state(TASK_RUNNING);
			mq->issue_fn(mq, req);
			if (test_bit(MMC_QUEUE_NEW_REQUEST, &mq->flags)) {
				continue; /* fetch again */
			} else if (test_bit(MMC_QUEUE_URGENT_REQUEST,
					&mq->flags) && (mq->mqrq_cur->req &&
					!(mq->mqrq_cur->req->cmd_flags &
						MMC_REQ_NOREINSERT_MASK))) {
				/*
				 * clean current request when urgent request
				 * processing in progress and current request is
				 * not urgent (all existing requests completed
				 * or reinserted to the block layer
				 */
				mq->mqrq_cur->brq.mrq.data = NULL;
				mq->mqrq_cur->req = NULL;
			}

			/*
			 * Current request becomes previous request
			 * and vice versa.
			 */
			mq->mqrq_prev->brq.mrq.data = NULL;
			mq->mqrq_prev->req = NULL;
			tmp = mq->mqrq_prev;
			mq->mqrq_prev = mq->mqrq_cur;
			mq->mqrq_cur = tmp;
		} else {
			if (kthread_should_stop()) {
				set_current_state(TASK_RUNNING);
				break;
			}
			mmc_start_delayed_bkops(card);
			mq->card->host->context_info.is_urgent = false;
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
static void mmc_request(struct request_queue *q)
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

/*
 * mmc_urgent_request() - Urgent MMC request handler.
 * @q: request queue.
 *
 * This is called when block layer has urgent request for delivery.  When mmc
 * context is waiting for the current request to complete, it will be awaken,
 * current request may be interrupted and re-inserted back to block device
 * request queue.  The next fetched request should be urgent request, this
 * will be ensured by block i/o scheduler.
 */
static void mmc_urgent_request(struct request_queue *q)
{
	unsigned long flags;
	struct mmc_queue *mq = q->queuedata;
	struct mmc_context_info *cntx;

	if (!mq) {
		mmc_request(q);
		return;
	}
	cntx = &mq->card->host->context_info;

	/* critical section with mmc_wait_data_done() */
	spin_lock_irqsave(&cntx->lock, flags);

	/* do stop flow only when mmc thread is waiting for done */
	if (mq->mqrq_cur->req || mq->mqrq_prev->req) {
		/*
		 * Urgent request must be executed alone
		 * so disable the write packing
		 */
		mmc_blk_disable_wr_packing(mq);
		cntx->is_urgent = true;
		spin_unlock_irqrestore(&cntx->lock, flags);
		wake_up_interruptible(&cntx->wait);
	} else {
		spin_unlock_irqrestore(&cntx->lock, flags);
		mmc_request(q);
	}
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
	q->limits.max_discard_sectors = max_discard;
	if (card->erased_byte == 0 && !mmc_can_discard(card))
		q->limits.discard_zeroes_data = 1;
	q->limits.discard_granularity = card->pref_erase << 9;
	/* granularity must not be greater than max. discard */
	if (card->pref_erase > max_discard)
		q->limits.discard_granularity = 0;
	if (mmc_can_secure_erase_trim(card))
		queue_flag_set_unlocked(QUEUE_FLAG_SECDISCARD, q);
}

static void mmc_queue_setup_sanitize(struct request_queue *q)
{
	queue_flag_set_unlocked(QUEUE_FLAG_SANITIZE, q);
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
		   spinlock_t *lock, const char *subname)
{
	struct mmc_host *host = card->host;
	u64 limit = BLK_BOUNCE_HIGH;
	int ret;
	struct mmc_queue_req *mqrq_cur = &mq->mqrq[0];
	struct mmc_queue_req *mqrq_prev = &mq->mqrq[1];

	if (mmc_dev(host)->dma_mask && *mmc_dev(host)->dma_mask)
		limit = *mmc_dev(host)->dma_mask;

	mq->card = card;
	mq->queue = blk_init_queue(mmc_request, lock);
	if (!mq->queue)
		return -ENOMEM;

	if ((host->caps2 & MMC_CAP2_STOP_REQUEST) &&
			host->ops->stop_request &&
			mq->card->ext_csd.hpi_en)
		blk_urgent_request(mq->queue, mmc_urgent_request);

	memset(&mq->mqrq_cur, 0, sizeof(mq->mqrq_cur));
	memset(&mq->mqrq_prev, 0, sizeof(mq->mqrq_prev));

	INIT_LIST_HEAD(&mqrq_cur->packed_list);
	INIT_LIST_HEAD(&mqrq_prev->packed_list);

	mq->mqrq_cur = mqrq_cur;
	mq->mqrq_prev = mqrq_prev;
	mq->queue->queuedata = mq;
	mq->num_wr_reqs_to_start_packing =
		min_t(int, (int)card->ext_csd.max_packed_writes,
		     DEFAULT_NUM_REQS_TO_START_PACK);

	blk_queue_prep_rq(mq->queue, mmc_prep_request);
	queue_flag_set_unlocked(QUEUE_FLAG_NONROT, mq->queue);
	if (mmc_can_erase(card))
		mmc_queue_setup_discard(mq->queue, card);

	/* Don't enable Sanitize if HPI is not supported */
	if ((mmc_can_sanitize(card) && (host->caps2 & MMC_CAP2_SANITIZE) &&
	    card->ext_csd.hpi_en))
		mmc_queue_setup_sanitize(mq->queue);

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
			mqrq_cur->bounce_buf = kmalloc(bouncesz, GFP_KERNEL);
			if (!mqrq_cur->bounce_buf) {
				pr_warning("%s: unable to "
					"allocate bounce cur buffer\n",
					mmc_card_name(card));
			}
			mqrq_prev->bounce_buf = kmalloc(bouncesz, GFP_KERNEL);
			if (!mqrq_prev->bounce_buf) {
				pr_warning("%s: unable to "
					"allocate bounce prev buffer\n",
					mmc_card_name(card));
				kfree(mqrq_cur->bounce_buf);
				mqrq_cur->bounce_buf = NULL;
			}
		}

		if (mqrq_cur->bounce_buf && mqrq_prev->bounce_buf) {
			blk_queue_bounce_limit(mq->queue, BLK_BOUNCE_ANY);
			blk_queue_max_hw_sectors(mq->queue, bouncesz / 512);
			blk_queue_max_segments(mq->queue, bouncesz / 512);
			blk_queue_max_segment_size(mq->queue, bouncesz);

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
		}
	}
#endif

	if (!mqrq_cur->bounce_buf && !mqrq_prev->bounce_buf) {
		unsigned int max_segs = host->max_segs;

		blk_queue_bounce_limit(mq->queue, limit);
		blk_queue_max_hw_sectors(mq->queue,
			min(host->max_blk_count, host->max_req_size / 512));
		blk_queue_max_segment_size(mq->queue, host->max_seg_size);
retry:
		blk_queue_max_segments(mq->queue, host->max_segs);

		mqrq_cur->sg = mmc_alloc_sg(host->max_segs, &ret);
		if (ret == -ENOMEM)
			goto cur_sg_alloc_failed;
		else if (ret)
			goto cleanup_queue;

		mqrq_prev->sg = mmc_alloc_sg(host->max_segs, &ret);
		if (ret == -ENOMEM)
			goto prev_sg_alloc_failed;
		else if (ret)
			goto cleanup_queue;

		goto success;

prev_sg_alloc_failed:
		kfree(mqrq_cur->sg);
		mqrq_cur->sg = NULL;
cur_sg_alloc_failed:
		host->max_segs /= 2;
		if (host->max_segs) {
			goto retry;
		} else {
			host->max_segs = max_segs;
			goto cleanup_queue;
		}
	}

success:
	sema_init(&mq->thread_sem, 1);

	mq->thread = kthread_run(mmc_queue_thread, mq, "mmcqd/%d%s",
		host->index, subname ? subname : "");

	if (IS_ERR(mq->thread)) {
		ret = PTR_ERR(mq->thread);
		goto free_bounce_sg;
	}

	return 0;
 free_bounce_sg:
	kfree(mqrq_cur->bounce_sg);
	mqrq_cur->bounce_sg = NULL;
	kfree(mqrq_prev->bounce_sg);
	mqrq_prev->bounce_sg = NULL;

 cleanup_queue:
	kfree(mqrq_cur->sg);
	mqrq_cur->sg = NULL;
	kfree(mqrq_cur->bounce_buf);
	mqrq_cur->bounce_buf = NULL;

	kfree(mqrq_prev->sg);
	mqrq_prev->sg = NULL;
	kfree(mqrq_prev->bounce_buf);
	mqrq_prev->bounce_buf = NULL;

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

	if (!(test_and_set_bit(MMC_QUEUE_SUSPENDED, &mq->flags))) {
		spin_lock_irqsave(q->queue_lock, flags);
		blk_stop_queue(q);
		spin_unlock_irqrestore(q->queue_lock, flags);

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
	return rc;
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

static unsigned int mmc_queue_packed_map_sg(struct mmc_queue *mq,
					    struct mmc_queue_req *mqrq,
					    struct scatterlist *sg)
{
	struct scatterlist *__sg;
	unsigned int sg_len = 0;
	struct request *req;
	enum mmc_packed_cmd cmd;

	cmd = mqrq->packed_cmd;

	if (cmd == MMC_PACKED_WRITE) {
		__sg = sg;
		sg_set_buf(__sg, mqrq->packed_cmd_hdr,
				sizeof(mqrq->packed_cmd_hdr));
		sg_len++;
		__sg->page_link &= ~0x02;
	}

	__sg = sg + sg_len;
	list_for_each_entry(req, &mqrq->packed_list, queuelist) {
		sg_len += blk_rq_map_sg(mq->queue, req, __sg);
		__sg = sg + (sg_len - 1);
		(__sg++)->page_link &= ~0x02;
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
	int i;

	if (!mqrq->bounce_buf) {
		if (!list_empty(&mqrq->packed_list))
			return mmc_queue_packed_map_sg(mq, mqrq, mqrq->sg);
		else
			return blk_rq_map_sg(mq->queue, mqrq->req, mqrq->sg);
	}

	BUG_ON(!mqrq->bounce_sg);

	if (!list_empty(&mqrq->packed_list))
		sg_len = mmc_queue_packed_map_sg(mq, mqrq, mqrq->bounce_sg);
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
