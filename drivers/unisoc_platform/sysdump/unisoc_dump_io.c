// SPDX-License-Identifier: GPL-2.0
/*
 * Sysdump sched stats for Spreadtrum SoCs
 *
 * Copyright (C) 2021 Spreadtrum corporation. http://www.unisoc.com
 */

#define pr_fmt(fmt) "sprd-sysdump-io: " fmt

#include <linux/bio.h>
#include <linux/blk-mq.h>
#include <linux/blkdev.h>
#include <linux/device.h>
#include <linux/elevator.h>
#include <linux/genhd.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/panic_notifier.h>
#include <linux/platform_device.h>
#include <linux/sched/signal.h>
#include <linux/timekeeping.h>
#include <../block/blk-mq.h>
#include <../block/blk-mq-tag.h>
#include <../drivers/mmc/core/queue.h>
#include <../drivers/base/base.h>
#include "unisoc_dump_io.h"
#include "unisoc_sysdump.h"
#include "unisoc_dump_info.h"

#ifdef CONFIG_ARM64
#include "sysdump64.h"
#endif

#define SPRD_DUMP_IO_SIZE       (9 * 4096)

static struct seq_buf *sprd_io_seq_buf;
static const char *blk_dev[] = {"mmcblk0", "mmcblk1", "sda", "sdb", ""};

static void print_req_contents(void)
{
	SEQ_printf(sprd_io_seq_buf, "%12s %3s %12s %8s %20s %20s %20s %5s %20s %20s %20s %20s "
			"%20s\n", " __data_len", "tag", "internal_tag", "rq_flags", "start_time_ns",
			"fifo_time", "io_start_time_ns", "state", "deadline", "now",
			"nowjiffies", "start2issue", "issue2now");
}

static void print_req_info(struct request *rq, u64 now)
{
	if (!rq) {
		SEQ_printf(sprd_io_seq_buf, "request is null\n");
		return;
	}
	SEQ_printf(sprd_io_seq_buf, "%12d %3d %12d %8x %20llu %20llu %20llu %5d %20lu %20llu "
		"%20lu %20llu %20llu\n", rq->__data_len, rq->tag, rq->internal_tag, rq->rq_flags,
		rq->start_time_ns, rq->fifo_time, rq->io_start_time_ns, rq->state,
		rq->deadline, now, jiffies, rq->io_start_time_ns - rq->start_time_ns,
		now - rq->io_start_time_ns);
}

static struct request_queue *sprd_get_queue(struct device *dev)
{
	struct gendisk *disk;
	struct request_queue *q = NULL;
	int i;

	if (!dev) {
		SEQ_printf(sprd_io_seq_buf, "dev is null when sprd_get_queue!\n");
		return NULL;
	}
	for (i = 0; strcmp("", blk_dev[i]); i++) {
		if (!strcmp(blk_dev[i], dev->kobj.name)) {
			disk = dev_to_disk(dev);
			if (!disk) {
				SEQ_printf(sprd_io_seq_buf, "cannot get disk !\n");
				return NULL;
			}
			q = disk->queue;
			if (!q) {
				SEQ_printf(sprd_io_seq_buf, "disk->queue is NULL !\n");
				return NULL;
			}
			return q;
		}
	}
	return NULL;
}

static int sprd_dump_request_in_flight(struct request_queue *q, u64 now)
{
	struct blk_mq_hw_ctx *hctx;
	int i = 0;
	int j = 0;
	int reqs_inflight = 0;
	struct request *rq;
	struct blk_mq_tags *sched_tags;

	SEQ_printf(sprd_io_seq_buf, "****************all requests in flight********************\n");
	print_req_contents();
	if (!q) {
		SEQ_printf(sprd_io_seq_buf, "request_queue is null !\n");
		return 0;
	}
	queue_for_each_hw_ctx(q, hctx, i) {
		if (!hctx) {
			SEQ_printf(sprd_io_seq_buf, "hctx is null !\n");
			return 0;
		}
		sched_tags = hctx->sched_tags;
		for (j = 0; j < sched_tags->nr_tags; j++) {
			rq = sched_tags->static_rqs[j];
			if (!rq) {
				SEQ_printf(sprd_io_seq_buf, "static_rqs[%d] is null !\n", j);
				continue;
			}
			if (rq->state != 0) {
				reqs_inflight++;
				print_req_info(rq, now);
			}
		}
	}
	return reqs_inflight;
}

static void sprd_dump_process_bio_plug(u64 now)
{
	struct task_struct *p;
	struct list_head *pos;
	struct bio *bio;
	struct blk_plug *plug;
	struct request *rq;
	int i;

	SEQ_printf(sprd_io_seq_buf, "**************bio in process and plug requests************\n");
	SEQ_printf(sprd_io_seq_buf, "%10s", "pid");
	print_req_contents();
	for_each_process(p) {
		if (p->bio_list) {
			for (i = 0; i < 2; i++) {
				if (!bio_list_empty(&p->bio_list[i])) {
					bio_list_for_each(bio, &p->bio_list[i]) {
						SEQ_printf(sprd_io_seq_buf, "pid = %d,"\
								"bi_flags =%x\n", p->pid,
								bio->bi_flags);
					}
				}
			}
		}
		plug = p->plug;
		if (plug && !list_empty(&plug->mq_list)) {
			list_for_each(pos, &plug->mq_list) {
				rq = list_entry(pos, struct request, queuelist);
				SEQ_printf(sprd_io_seq_buf, "%10d", p->pid);
				print_req_info(rq, now);
			}
		}
	}
	SEQ_printf(sprd_io_seq_buf, "*****************bio in process and plug requests*********\n");
}

static int sprd_dump_requests_in_use(struct request_queue *q, u64 now)
{
	struct blk_mq_hw_ctx *hctx = NULL;
	int i, j;
	int total_reqs = 0;
	struct request *rq = NULL;
	struct blk_mq_tags *sched_tags = NULL;

	SEQ_printf(sprd_io_seq_buf, "*******************all requests in use********************\n");
	print_req_contents();
	if (!q) {
		SEQ_printf(sprd_io_seq_buf, "request_queue is null !\n");
		return 0;
	}
	queue_for_each_hw_ctx(q, hctx, i) {
		if (q->elevator) {
			if (!hctx) {
				SEQ_printf(sprd_io_seq_buf, "hctx is null !\n");
				return 0;
			}
			sched_tags = hctx->sched_tags;
			for (j = 0; j < sched_tags->nr_tags; j++) {
				rq = sched_tags->static_rqs[j];
				if (!rq) {
					SEQ_printf(sprd_io_seq_buf, "static_rqs[%d] is null !\n",
							j);
					continue;
				}
				if (rq->mq_hctx == hctx) {
					total_reqs++;
					print_req_info(rq, now);
				}
			}
		}
	}
	return total_reqs;
}

void sprd_dump_io(void)
{
	struct device *dev;
	struct kset *set;
	struct kobject *obj;
	struct request_queue *q;
	struct mmc_queue *mq;
	int total_reqs, inflight_reqs;
	u64 now;
	struct list_head *pos;

	if (!sprd_io_seq_buf)
		return;
	now = ktime_get_ns();
	obj = &platform_bus.kobj;
	if (!obj) {
		SEQ_printf(sprd_io_seq_buf, "platform_bus.kobj is NULL\n");
		return;
	}
	set = obj->kset;
	if (!set) {
		SEQ_printf(sprd_io_seq_buf, "obj->kset is NULL\n");
		return;
	}
	list_for_each(pos, &set->list) {
		dev = list_entry(pos, struct device, kobj.entry);
		if (!dev) {
			SEQ_printf(sprd_io_seq_buf, "dev is null !\n");
			return;
		}
		q = sprd_get_queue(dev);
		if (!q)
			continue;
		mq = q->queuedata;
		total_reqs = sprd_dump_requests_in_use(q, now);
		inflight_reqs = sprd_dump_request_in_flight(q, now);
		if (!mq) {
			SEQ_printf(sprd_io_seq_buf, "mmc_queue is null !\n");
			total_reqs = 0;
			continue;
		}
		SEQ_printf(sprd_io_seq_buf, "devices=%s queue_flags=%lx total_reqs=%d "
					"in_flight[MMC_ISSUE_SYNC]=%d "
					"in_flight[MMC_ISSUE_DCMD]=%d "
					"in_flight[MMC_ISSUE_ASYNC]=%d\n",
					dev->kobj.name, q->queue_flags, total_reqs,
					mq->in_flight[0], mq->in_flight[1], mq->in_flight[2]);
		total_reqs = 0;
	}
	sprd_dump_process_bio_plug(now);
}
EXPORT_SYMBOL_GPL(sprd_dump_io);

static int sprd_io_event(struct notifier_block *self, unsigned long val, void *reason)
{
	sprd_dump_io();
	return NOTIFY_DONE;
}

static struct notifier_block sprd_io_event_nb = {
	.notifier_call = sprd_io_event,
	.priority      = INT_MAX - 1,
};

int __init sprd_dump_io_init(void)
{
	int ret;

	ret = minidump_add_section("io", SPRD_DUMP_IO_SIZE, &sprd_io_seq_buf);
	if (ret)
		return ret;

	/* register io panic notifier */
	atomic_notifier_chain_register(&panic_notifier_list,
					&sprd_io_event_nb);

	pr_info("sprd: io_panic_nofifier_register success\n");

	return 0;

}

void __exit sprd_dump_io_exit(void)
{
	atomic_notifier_chain_unregister(&panic_notifier_list,
						&sprd_io_event_nb);

	minidump_release_section("io", sprd_io_seq_buf);
	sprd_io_seq_buf = NULL;
}
