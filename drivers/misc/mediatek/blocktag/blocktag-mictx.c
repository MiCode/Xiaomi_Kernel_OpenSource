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

void mtk_btag_mictx_check_window(struct mtk_btag_mictx_id mictx_id)
{
	struct mtk_blocktag *btag;
	struct mtk_btag_mictx_struct *mictx, *n;
	unsigned long flags;
	__u64 t_cur = sched_clock();

	btag = mtk_btag_find_by_type(mictx_id.storage);
	if (btag) {
		spin_lock_irqsave(&btag->mictx.list_lock, flags);
		list_for_each_entry_safe(mictx, n, &btag->mictx.list, list) {
			if (mictx->id == mictx_id.id) {
				if ((t_cur - mictx->window_begin) > 1000000000)
					mtk_btag_mictx_reset(mictx, t_cur);
			}
		}
		spin_unlock_irqrestore(&btag->mictx.list_lock, flags);
	}
}

void mtk_btag_mictx_eval_tp(
	struct mtk_blocktag *btag,
	unsigned int write, __u64 usage, __u32 size)
{
	struct mtk_btag_mictx_struct *mictx, *n;
	struct mtk_btag_throughput_rw *tprw;
	unsigned long flags;
	__u64 cur_time = sched_clock();
	__u64 req_begin_time;

	spin_lock_irqsave(&btag->mictx.list_lock, flags);
	list_for_each_entry_safe(mictx, n, &btag->mictx.list, list) {
		tprw = (write) ? &mictx->tp.w : &mictx->tp.r;
		tprw->size += size;
		tprw->usage += usage;

		mictx->tp_max_time = cur_time;
		req_begin_time = cur_time - usage;

		if (!mictx->tp_min_time || req_begin_time < mictx->tp_min_time)
			mictx->tp_min_time = req_begin_time;
	}
	spin_unlock_irqrestore(&btag->mictx.list_lock, flags);
}

void mtk_btag_mictx_eval_req(
	struct mtk_blocktag *btag,
	unsigned int write, __u32 cnt, __u32 size, bool top)
{
	struct mtk_btag_mictx_struct *mictx, *n;
	struct mtk_btag_req_rw *reqrw;
	unsigned long flags;

	spin_lock_irqsave(&btag->mictx.list_lock, flags);
	list_for_each_entry_safe(mictx, n, &btag->mictx.list, list) {
		reqrw = (write) ? &mictx->req.w : &mictx->req.r;
		reqrw->count += cnt;
		reqrw->size += size;
		if (top)
			reqrw->size_top += size;
	}
	spin_unlock_irqrestore(&btag->mictx.list_lock, flags);

	if (top && btag->vops->earaio_enabled)
		mtk_btag_earaio_update_pwd(write, size);
}

void mtk_btag_mictx_eval_cnt_signle_wqd(
	struct mtk_blocktag *btag, u64 t_begin, u64 t_cur)
{
	struct mtk_btag_mictx_struct *mictx, *n;
	unsigned long flags;

	spin_lock_irqsave(&btag->mictx.list_lock, flags);
	list_for_each_entry_safe(mictx, n, &btag->mictx.list, list) {
		t_begin = max_t(u64, mictx->window_begin, t_begin);
		mictx->weighted_qd += t_cur - t_begin;
	}
	spin_unlock_irqrestore(&btag->mictx.list_lock, flags);
}

void mtk_btag_mictx_update(
	struct mtk_blocktag *btag,
	__u32 q_depth)
{
	struct mtk_btag_mictx_struct *mictx, *n;
	unsigned long flags;
	__u64 t_cur = sched_clock();

	spin_lock_irqsave(&btag->mictx.list_lock, flags);
	list_for_each_entry_safe(mictx, n, &btag->mictx.list, list) {
		mictx->q_depth = q_depth;

		if (!mictx->q_depth) {
			mictx->idle_begin = t_cur;
		} else {
			if (mictx->idle_begin) {
				mictx->idle_total += t_cur - mictx->idle_begin;
				mictx->idle_begin = 0;
			}
		}
	}
	spin_unlock_irqrestore(&btag->mictx.list_lock, flags);

	/*
	 * Peek if I/O workload exceeds the threshold to send boosting
	 * notification during this window
	 */
	if (q_depth && btag->vops->earaio_enabled)
		mtk_btag_earaio_check_pwd();
}

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
	struct mtk_btag_mictx_id mictx_id,
	struct mtk_btag_mictx_iostat_struct *iostat)
{
	struct mtk_blocktag *btag;
	struct mtk_btag_mictx_struct *mictx, *n;
	struct mtk_btag_mictx_struct tmp_mictx;
	u64 time_cur, dur, tp_dur;
	unsigned long flags;
	u64 top;
	bool find = false;

	if (!iostat || mictx_id.id == -1)
		return -1;

	time_cur = sched_clock();

	btag = mtk_btag_find_by_type(mictx_id.storage);
	if (btag) {
		spin_lock_irqsave(&btag->mictx.list_lock, flags);
		list_for_each_entry_safe(mictx, n, &btag->mictx.list, list) {
			if (mictx->id == mictx_id.id) {
				find = true;
				memcpy(&tmp_mictx, mictx
					, sizeof(struct mtk_btag_mictx_struct));
				mtk_btag_mictx_reset(mictx, time_cur);
			}
		}
		spin_unlock_irqrestore(&btag->mictx.list_lock, flags);
	}

	if (!find)
		return -1;

	dur = time_cur - tmp_mictx.window_begin;

	/* fill-in duration */
	iostat->duration = dur;

	/* calculate throughput (per-request) */
	iostat->tp_req_r = mtk_btag_eval_tp_speed(
		tmp_mictx.tp.r.size, tmp_mictx.tp.r.usage);
	iostat->tp_req_w = mtk_btag_eval_tp_speed(
		tmp_mictx.tp.w.size, tmp_mictx.tp.w.usage);

	/* calculate throughput (overlapped, not 100% precise) */
	tp_dur = tmp_mictx.tp_max_time - tmp_mictx.tp_min_time;
	iostat->tp_all_r = mtk_btag_eval_tp_speed(
		tmp_mictx.tp.r.size, tp_dur);
	iostat->tp_all_w = mtk_btag_eval_tp_speed(
		tmp_mictx.tp.w.size, tp_dur);

	/* provide request count and size */
	iostat->reqcnt_r = tmp_mictx.req.r.count;
	iostat->reqsize_r = tmp_mictx.req.r.size;
	iostat->reqcnt_w = tmp_mictx.req.w.count;
	iostat->reqsize_w = tmp_mictx.req.w.size;

	/* calculate workload */
	if (tmp_mictx.idle_begin)
		tmp_mictx.idle_total += (time_cur - tmp_mictx.idle_begin);

	iostat->wl = 100 - div64_u64((tmp_mictx.idle_total * 100), dur);

	/* calculate top ratio */
	if (tmp_mictx.req.r.size || tmp_mictx.req.w.size) {
		tmp_mictx.req.r.size >>= 12;
		tmp_mictx.req.w.size >>= 12;
		tmp_mictx.req.r.size_top >>= 12;
		tmp_mictx.req.w.size_top >>= 12;

		top = tmp_mictx.req.r.size_top + tmp_mictx.req.w.size_top;
		top = top * 100 / (tmp_mictx.req.r.size + tmp_mictx.req.w.size);
	} else {
		top = 0;
	}

	iostat->top = top;

	/* fill-in cmdq depth */
	if (btag->vops->mictx_eval_wqd) {
		btag->vops->mictx_eval_wqd(&tmp_mictx, time_cur);
		iostat->q_depth = DIV64_U64_ROUND_UP(tmp_mictx.weighted_qd, dur);
	} else
		iostat->q_depth = tmp_mictx.q_depth;

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_btag_mictx_get_data);

static int mtk_btag_mictx_alloc(enum mtk_btag_storage_type type)
{
	struct mtk_blocktag *btag;
	struct mtk_btag_mictx_struct *mictx;
	unsigned long flags;

	btag = mtk_btag_find_by_type(type);
	if (btag) {
		mictx = kzalloc(sizeof(struct mtk_btag_mictx_struct), GFP_NOFS);
		if (!mictx)
			return -1;

		spin_lock_irqsave(&btag->mictx.list_lock, flags);
		mictx->id = btag->mictx.last_unused_id;
		mictx->window_begin = sched_clock();
		btag->mictx.count++;
		btag->mictx.last_unused_id++;
		list_add(&mictx->list, &btag->mictx.list);
		spin_unlock_irqrestore(&btag->mictx.list_lock, flags);

		return mictx->id;
	}

	return -1;
}

static void mtk_btag_mictx_free(struct mtk_btag_mictx_id *mictx_id)
{
	struct mtk_blocktag *btag;
	struct mtk_btag_mictx_struct *mictx, *n;
	unsigned long flags;

	btag = mtk_btag_find_by_type(mictx_id->storage);
	if (btag) {
		spin_lock_irqsave(&btag->mictx.list_lock, flags);
		list_for_each_entry_safe(mictx, n, &btag->mictx.list, list) {
			if (mictx->id == mictx_id->id) {
				mictx_id->id = -1;
				list_del(&mictx->list);
				btag->mictx.count--;
				kfree(mictx);
				break;
			}
		}
		spin_unlock_irqrestore(&btag->mictx.list_lock, flags);
	}
}

void mtk_btag_mictx_free_btag(struct mtk_blocktag *btag)
{
	struct mtk_btag_mictx_struct *mictx, *n;
	unsigned long flags;

	spin_lock_irqsave(&btag->mictx.list_lock, flags);
	list_for_each_entry_safe(mictx, n, &btag->mictx.list, list) {
		list_del(&mictx->list);
		btag->mictx.count--;
		kfree(mictx);
	}
	spin_unlock_irqrestore(&btag->mictx.list_lock, flags);
}

void mtk_btag_mictx_enable(struct mtk_btag_mictx_id *mictx_id, int enable)
{
	if (enable)
		mictx_id->id = mtk_btag_mictx_alloc(mictx_id->storage);
	else
		mtk_btag_mictx_free(mictx_id);
}
EXPORT_SYMBOL_GPL(mtk_btag_mictx_enable);

void mtk_btag_mictx_init(struct mtk_blocktag *btag, struct mtk_btag_vops *vops)
{
	spin_lock_init(&btag->mictx.list_lock);
	btag->mictx.count = 0;
	INIT_LIST_HEAD(&btag->mictx.list);
}
