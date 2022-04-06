// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 * Authors:
 *	Perry Hsu <perry.hsu@mediatek.com>
 *	Stanley Chu <stanley.chu@mediatek.com>
 */

#define DEBUG 1

#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/math64.h>

#include "mtk_blocktag.h"

static struct mtk_blocktag *btag_bootdev;

static inline struct mtk_btag_mictx_struct *mtk_btag_mictx_get(void)
{
	if (btag_bootdev)
		return &btag_bootdev->mictx;
	else
		return NULL;
}

static void mtk_btag_mictx_reset(
	struct mtk_btag_mictx_struct *mictx,
	__u64 window_begin)
{
	if (!window_begin)
		window_begin = sched_clock();
	mictx->window_begin = window_begin;

	if (!mictx->q_depth)
		mictx->idle_begin = mictx->window_begin;
	else
		mictx->idle_begin = 0;

	mictx->idle_total = 0;
	mictx->tp_min_time = mictx->tp_max_time = 0;
	mictx->weighted_qd = 0;
	memset(&mictx->tp, 0, sizeof(struct mtk_btag_throughput));
	memset(&mictx->req, 0, sizeof(struct mtk_btag_req));
}

void mtk_btag_mictx_check_window(void)
{
	struct mtk_btag_mictx_struct *mictx;
	unsigned long flags;

	mictx = mtk_btag_mictx_get();
	if (!mictx || !mictx->enabled)
		return;

	spin_lock_irqsave(&mictx->lock, flags);
	if ((sched_clock() - mictx->window_begin) > 1000000000)
		mtk_btag_mictx_reset(mictx, 0);
	spin_unlock_irqrestore(&mictx->lock, flags);
}

void mtk_btag_mictx_eval_tp(
	struct mtk_blocktag *btag,
	unsigned int write, __u64 usage, __u32 size)
{
	struct mtk_btag_mictx_struct *mictx = &btag->mictx;
	struct mtk_btag_throughput_rw *tprw;
	unsigned long flags;
	__u64 cur_time = sched_clock();
	__u64 req_begin_time;

	if (!mictx->enabled)
		return;

	tprw = (write) ? &mictx->tp.w : &mictx->tp.r;
	spin_lock_irqsave(&mictx->lock, flags);
	tprw->size += size;
	tprw->usage += usage;

	mictx->tp_max_time = cur_time;
	req_begin_time = cur_time - usage;

	if (!mictx->tp_min_time || req_begin_time < mictx->tp_min_time)
		mictx->tp_min_time = req_begin_time;
	spin_unlock_irqrestore(&mictx->lock, flags);
}
EXPORT_SYMBOL_GPL(mtk_btag_mictx_eval_tp);

void mtk_btag_mictx_eval_req(
	struct mtk_blocktag *btag,
	unsigned int write, __u32 cnt, __u32 size, bool top)
{
	struct mtk_btag_mictx_struct *mictx = &btag->mictx;
	struct mtk_btag_req_rw *reqrw;
	unsigned long flags;

	if (!mictx->enabled)
		return;

	reqrw = (write) ? &mictx->req.w : &mictx->req.r;
	spin_lock_irqsave(&mictx->lock, flags);
	reqrw->count += cnt;
	reqrw->size += size;
	if (top)
		reqrw->size_top += size;
	spin_unlock_irqrestore(&mictx->lock, flags);

	if (top)
		mtk_btag_earaio_update_pwd(write, size);
}
EXPORT_SYMBOL_GPL(mtk_btag_mictx_eval_req);

void mtk_btag_mictx_eval_cnt_signle_wqd(
	struct mtk_blocktag *btag, u64 t_begin, u64 t_cur)
{
	struct mtk_btag_mictx_struct *mictx = &btag->mictx;

	if (!mictx->enabled)
		return;

	t_begin = max_t(u64, mictx->window_begin, t_begin);
	mictx->weighted_qd += t_cur - t_begin;
}
EXPORT_SYMBOL_GPL(mtk_btag_mictx_eval_cnt_signle_wqd);

void mtk_btag_mictx_update(
	struct mtk_blocktag *btag,
	__u32 q_depth)
{
	struct mtk_btag_mictx_struct *mictx = &btag->mictx;
	unsigned long flags;

	if (!mictx->enabled)
		return;

	spin_lock_irqsave(&mictx->lock, flags);
	mictx->q_depth = q_depth;

	if (!mictx->q_depth) {
		mictx->idle_begin = sched_clock();
	} else {
		if (mictx->idle_begin) {
			mictx->idle_total +=
				(sched_clock() - mictx->idle_begin);
			mictx->idle_begin = 0;
		}
	}
	spin_unlock_irqrestore(&mictx->lock, flags);

	/*
	 * Peek if I/O workload exceeds the threshold to send boosting
	 * notification during this window
	 */
	if (q_depth)
		mtk_btag_earaio_check_pwd();
}
EXPORT_SYMBOL_GPL(mtk_btag_mictx_update);

static __u32 mtk_btag_eval_tp_speed(__u32 bytes, __u64 duration)
{
	__u32 speed_kbs = 0;

	if (!bytes || !duration)
		return 0;

	/* convert ns to ms */
	do_div(duration, 1000000);

	if (duration) {
		/* bytes/ms */
		speed_kbs = bytes / (__u32)duration;

		/* KB/s */
		speed_kbs = (speed_kbs * 1000) >> 10;
	}

	return speed_kbs;
}

int mtk_btag_mictx_get_data(
	struct mtk_btag_mictx_iostat_struct *iostat)
{
	struct mtk_btag_mictx_struct *mictx;
	struct mtk_blocktag *btag;
	u64 time_cur, dur, tp_dur;
	unsigned long flags;
	u64 top;

	mictx = mtk_btag_mictx_get();
	if (!mictx || !mictx->enabled || !iostat)
		return -1;

	spin_lock_irqsave(&mictx->lock, flags);

	time_cur = sched_clock();
	dur = time_cur - mictx->window_begin;

	/* fill-in duration */
	iostat->duration = dur;

	/* calculate throughput (per-request) */
	iostat->tp_req_r = mtk_btag_eval_tp_speed(
		mictx->tp.r.size, mictx->tp.r.usage);
	iostat->tp_req_w = mtk_btag_eval_tp_speed(
		mictx->tp.w.size, mictx->tp.w.usage);

	/* calculate throughput (overlapped, not 100% precise) */
	tp_dur = mictx->tp_max_time - mictx->tp_min_time;
	iostat->tp_all_r = mtk_btag_eval_tp_speed(
		mictx->tp.r.size, tp_dur);
	iostat->tp_all_w = mtk_btag_eval_tp_speed(
		mictx->tp.w.size, tp_dur);

	/* provide request count and size */
	iostat->reqcnt_r = mictx->req.r.count;
	iostat->reqsize_r = mictx->req.r.size;
	iostat->reqcnt_w = mictx->req.w.count;
	iostat->reqsize_w = mictx->req.w.size;

	/* calculate workload */
	if (mictx->idle_begin)
		mictx->idle_total += (time_cur - mictx->idle_begin);

	iostat->wl = 100 - div64_u64((mictx->idle_total * 100), dur);

	/* calculate top ratio */
	if (mictx->req.r.size || mictx->req.w.size) {
		mictx->req.r.size >>= 12;
		mictx->req.w.size >>= 12;
		mictx->req.r.size_top >>= 12;
		mictx->req.w.size_top >>= 12;

		top = mictx->req.r.size_top + mictx->req.w.size_top;
		top = top * 100 / (mictx->req.r.size + mictx->req.w.size);
	} else {
		top = 0;
	}

	iostat->top = top;

	/* fill-in cmdq depth */
	btag = btag_bootdev;
	if (btag && btag->vops->mictx_eval_wqd) {
		btag->vops->mictx_eval_wqd(mictx, time_cur);
		iostat->q_depth = DIV64_U64_ROUND_UP(mictx->weighted_qd, dur);
	} else
		iostat->q_depth = mictx->q_depth;

	/* everything was provided, now we can reset the mictx */
	mtk_btag_mictx_reset(mictx, time_cur);

	spin_unlock_irqrestore(&mictx->lock, flags);

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_btag_mictx_get_data);

void mtk_btag_mictx_enable(int enable)
{
	struct mtk_btag_mictx_struct *mictx;
	unsigned long flags;

	mictx = mtk_btag_mictx_get();
	if (!mictx)
		return;

	spin_lock_irqsave(&mictx->lock, flags);
	if (enable == mictx->enabled) {
		spin_unlock_irqrestore(&mictx->lock, flags);
		return;
	}
	mictx->enabled = enable;
	if (enable)
		mtk_btag_mictx_reset(mictx, 0);

	spin_unlock_irqrestore(&mictx->lock, flags);
}
EXPORT_SYMBOL_GPL(mtk_btag_mictx_enable);

void mtk_btag_mictx_init(struct mtk_blocktag *btag, struct mtk_btag_vops *vops)
{
	if (!vops->boot_device)
		return;

	btag_bootdev = btag;
	spin_lock_init(&btag->mictx.lock);
}
