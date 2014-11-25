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

#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include "queue.h"

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
	if (req->cmd_type != REQ_TYPE_FS && !(req->cmd_flags & REQ_DISCARD)) {
		blk_dump_rq_flags(req, "MMC bad request");
		return BLKPREP_KILL;
	}

	if (mq && mmc_card_removed(mq->card))
		return BLKPREP_KILL;

	req->cmd_flags |= REQ_DONTPREP;

	return BLKPREP_OK;
}

static bool mmc_queue_get_free_slot(struct mmc_queue *mq,
		unsigned long *free_slot)
{
	unsigned long slot;
	int i;

	if (!mq || !free_slot)
		return false;

	do {
		slot = find_first_zero_bit(&mq->cmdqslot, mq->qdepth);
		if (slot >= mq->qdepth)
			return false;

		i = test_and_set_bit_lock(slot, &mq->cmdqslot);
	} while (i);

	*free_slot = slot;
	return true;
}

static int mmc_queue_thread(void *d)
{
	struct mmc_queue *mq = d;
	struct request_queue *q = mq->queue;

	current->flags |= PF_MEMALLOC;

	down(&mq->thread_sem);
	do {
		struct request *req = NULL;
		unsigned long i;

		spin_lock_irq(q->queue_lock);
		set_current_state(TASK_INTERRUPTIBLE);
		req = blk_fetch_request(q);
		spin_unlock_irq(q->queue_lock);

		/*
		 * For the request which doesn't have data to transfer,
		 * we don't need to allocate a mqrq slot for it as it doesn't
		 * need the sg to map data
		 */
		if (req && !(req->cmd_flags & (REQ_DISCARD | REQ_FLUSH))) {
			while (!mmc_queue_get_free_slot(mq, &i))
				mq->issue_fn(mq, NULL, true);
			mq->mqrq_cur = &mq->mqrq[i];
			mq->mqrq_cur->req = req;
		}

		if (req || atomic_read(&mq->active_slots)) {
			set_current_state(TASK_RUNNING);
			mq->issue_fn(mq, req, false);
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
	if (atomic_read(&mq->active_slots)) {
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
	} else
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
	int ret, i = 0;
	struct mmc_queue_req *mqrq = mq->mqrq;

	if (mmc_dev(host)->dma_mask && *mmc_dev(host)->dma_mask)
		limit = (u64)dma_max_pfn(mmc_dev(host)) << PAGE_SHIFT;

	mq->card = card;
	mq->queue = blk_init_queue(mmc_request_fn, lock);
	if (!mq->queue)
		return -ENOMEM;

	if (card->ext_csd.cmdq_en)
		mq->qdepth = card->ext_csd.cmdq_depth;
	else
		mq->qdepth = 2;

	mq->mqrq = kzalloc(mq->qdepth * sizeof(struct mmc_queue_req),
			GFP_KERNEL);
	if (!mq->mqrq) {
		ret = -ENOMEM;
		goto out;
	}

	mqrq = mq->mqrq;
	for (i = mq->qdepth; i > 0; i--)
		mqrq[i - 1].task_id = i - 1;

	mq->queue->queuedata = mq;

	blk_queue_prep_rq(mq->queue, mmc_prep_request);
	queue_flag_set_unlocked(QUEUE_FLAG_NONROT, mq->queue);
	if (mmc_can_erase(card))
		mmc_queue_setup_discard(mq->queue, card);

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
			for (i = 0; i < mq->qdepth; i++) {
				mqrq[i].bounce_buf =
					kmalloc(bouncesz, GFP_KERNEL);
				if (!mqrq[i].bounce_buf)
					break;
			}
		}

		if (i != mq->qdepth) {
			for (; i >= 0; i--) {
				kfree(mqrq[i].bounce_buf);
				mqrq[i].bounce_buf = NULL;
			}
		} else {
			blk_queue_bounce_limit(mq->queue, BLK_BOUNCE_ANY);
			blk_queue_max_hw_sectors(mq->queue, bouncesz / 512);
			blk_queue_max_segments(mq->queue, bouncesz / 512);
			blk_queue_max_segment_size(mq->queue, bouncesz);

			for (i = 0; i < mq->qdepth; i++) {
				mqrq[i].sg = mmc_alloc_sg(1, &ret);
				if (ret)
					goto cleanup_queue;
				mqrq[i].bounce_sg =
					mmc_alloc_sg(bouncesz / 512, &ret);
				if (ret)
					goto cleanup_queue;
			}
		}
	}
#endif

	if (i == 0) {
		blk_queue_bounce_limit(mq->queue, limit);
		blk_queue_max_hw_sectors(mq->queue,
			min(host->max_blk_count, host->max_req_size / 512));
		blk_queue_max_segments(mq->queue, host->max_segs);
		blk_queue_max_segment_size(mq->queue, host->max_seg_size);

		for (i = 0; i < mq->qdepth; i++) {
			mqrq[i].sg = mmc_alloc_sg(host->max_segs, &ret);
			if (ret)
				goto cleanup_queue;
		}
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
	for (i = 0; i < mq->qdepth; i++) {
		kfree(mqrq[i].bounce_sg);
		mqrq[i].bounce_sg = NULL;
	}
 cleanup_queue:
	for (i = 0; i < mq->qdepth; i++) {
		kfree(mqrq[i].sg);
		mqrq[i].sg = NULL;
		kfree(mqrq[i].bounce_buf);
		mqrq[i].bounce_buf = NULL;
	}

	kfree(mq->mqrq);
out:
	blk_cleanup_queue(mq->queue);
	return ret;
}

void mmc_cleanup_queue(struct mmc_queue *mq)
{
	struct request_queue *q = mq->queue;
	unsigned long flags;
	int i;

	/* Make sure the queue isn't suspended, as that will deadlock */
	mmc_queue_resume(mq);

	/* Then terminate our worker thread */
	kthread_stop(mq->thread);

	/* Empty the queue */
	spin_lock_irqsave(q->queue_lock, flags);
	q->queuedata = NULL;
	blk_start_queue(q);
	spin_unlock_irqrestore(q->queue_lock, flags);

	for (i = 0; i < mq->qdepth; i++) {
		kfree(mq->mqrq[i].bounce_sg);
		mq->mqrq[i].bounce_sg = NULL;
		kfree(mq->mqrq[i].sg);
		mq->mqrq[i].sg = NULL;
		kfree(mq->mqrq[i].bounce_buf);
		mq->mqrq[i].bounce_buf = NULL;
	}

	mq->card = NULL;
}
EXPORT_SYMBOL(mmc_cleanup_queue);

int mmc_packed_init(struct mmc_queue *mq, struct mmc_card *card)
{
	struct mmc_queue_req *mqrq_cur = &mq->mqrq[0];
	struct mmc_queue_req *mqrq_prev = &mq->mqrq[1];
	int ret = 0;

	/*
	 * the qdepth for PACK CMD is 2
	 */
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

	if (!(mq->flags & MMC_QUEUE_SUSPENDED)) {
		mq->flags |= MMC_QUEUE_SUSPENDED;

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

	if (mq->flags & MMC_QUEUE_SUSPENDED) {
		mq->flags &= ~MMC_QUEUE_SUSPENDED;

		up(&mq->thread_sem);

		spin_lock_irqsave(q->queue_lock, flags);
		blk_start_queue(q);
		spin_unlock_irqrestore(q->queue_lock, flags);
	}
}

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
			(__sg++)->page_link &= ~0x02;
			sg_len++;
		} while (remain);
	}

	list_for_each_entry(req, &packed->list, queuelist) {
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
