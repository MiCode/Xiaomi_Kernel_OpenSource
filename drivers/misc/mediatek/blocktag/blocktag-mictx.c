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

#define MICTX_RESET_NS 1000000000

void mtk_btag_mictx_eval_tp(struct mtk_blocktag *btag, __u32 idx, bool write,
			    __u64 usage, __u32 size)
{
	struct mtk_btag_mictx *mictx;
	struct mtk_btag_throughput_rw *tprw;
	unsigned long flags;
	__u64 cur_time = sched_clock();
	__u64 req_begin_time;

	if (idx >= btag->ctx.count)
		return;

	rcu_read_lock();
	list_for_each_entry_rcu(mictx, &btag->ctx.mictx.list, list) {
		struct mtk_btag_mictx_data *data = mictx->data;

		spin_lock_irqsave(&data[idx].lock, flags);
		tprw = (write) ? &data[idx].tp.w : &data[idx].tp.r;
		tprw->size += size;
		tprw->usage += usage;
		data[idx].tp_max_time = cur_time;
		req_begin_time = cur_time - usage;
		if (!data[idx].tp_min_time || req_begin_time < data[idx].tp_min_time)
			data[idx].tp_min_time = req_begin_time;
		spin_unlock_irqrestore(&data[idx].lock, flags);
	}
	rcu_read_unlock();
}

void mtk_btag_mictx_eval_req(struct mtk_blocktag *btag, __u32 idx, bool write,
			     __u32 total_len, __u32 top_len)
{
	struct mtk_btag_mictx *mictx;
	struct mtk_btag_req_rw *reqrw;
	unsigned long flags;

	if (idx >= btag->ctx.count)
		return;

	rcu_read_lock();
	list_for_each_entry_rcu(mictx, &btag->ctx.mictx.list, list) {
		struct mtk_btag_mictx_data *data = mictx->data;

		spin_lock_irqsave(&data[idx].lock, flags);
		reqrw = (write) ? &data[idx].req.w : &data[idx].req.r;
		reqrw->count++;
		reqrw->size += total_len;
		reqrw->size_top += top_len;
		spin_unlock_irqrestore(&data[idx].lock, flags);
	}
	rcu_read_unlock();

	if (top_len && btag->vops->earaio_enabled)
		mtk_btag_earaio_update_pwd(write, top_len);
}

void mtk_btag_mictx_accumulate_weight_qd(struct mtk_blocktag *btag, __u32 idx,
					 __u64 t_begin, __u64 t_cur)
{
	struct mtk_btag_mictx *mictx;
	unsigned long flags;

	if (idx >= btag->ctx.count)
		return;

	rcu_read_lock();
	list_for_each_entry_rcu(mictx, &btag->ctx.mictx.list, list) {
		struct mtk_btag_mictx_data *data = mictx->data;

		spin_lock_irqsave(&data[idx].lock, flags);
		t_begin = max_t(u64, data[idx].window_begin, t_begin);
		data[idx].weighted_qd += t_cur - t_begin;
		spin_unlock_irqrestore(&data[idx].lock, flags);
	}
	rcu_read_unlock();
}

void mtk_btag_mictx_update(struct mtk_blocktag *btag, __u32 idx,
			   __u32 q_depth, __u64 sum_of_inflight_start)
{
	struct mtk_btag_mictx *mictx;
	unsigned long flags;
	__u64 t_cur = sched_clock();

	if (idx > btag->ctx.count)
		return;

	rcu_read_lock();
	list_for_each_entry_rcu(mictx, &btag->ctx.mictx.list, list) {
		struct mtk_btag_mictx_data *data = mictx->data;

		spin_lock_irqsave(&data[idx].lock, flags);
		data[idx].sum_of_inflight_start = sum_of_inflight_start;
		data[idx].q_depth = q_depth;
		if (!data[idx].q_depth) {
			data[idx].idle_begin = t_cur;
		} else {
			if (data[idx].idle_begin) {
				data[idx].idle_total += t_cur - data[idx].idle_begin;
				data[idx].idle_begin = 0;
			}
		}
		spin_unlock_irqrestore(&data[idx].lock, flags);
	}
	rcu_read_unlock();

	/*
	 * Peek if I/O workload exceeds the threshold to send boosting
	 * notification during this window
	 */
	if (q_depth && btag->vops->earaio_enabled)
		mtk_btag_earaio_check_pwd();
}

static void mtk_btag_mictx_reset(struct mtk_btag_mictx_data *data,
				 __u64 window_begin)
{
	data->window_begin = window_begin;

	if (!data->q_depth)
		data->idle_begin = data->window_begin;
	else
		data->idle_begin = 0;

	data->idle_total = 0;
	data->tp_min_time = data->tp_max_time = 0;
	data->weighted_qd = 0;
	memset(&data->tp, 0, sizeof(struct mtk_btag_throughput));
	memset(&data->req, 0, sizeof(struct mtk_btag_req));
}

static struct mtk_btag_mictx *mtk_btag_mictx_find(struct mtk_blocktag *btag,
						  __s8 id)
{
	struct mtk_btag_mictx *mictx;

	list_for_each_entry_rcu(mictx, &btag->ctx.mictx.list, list)
		if (mictx->id == id)
			return mictx;
	return NULL;
}

void mtk_btag_mictx_check_window(struct mtk_btag_mictx_id mictx_id)
{
	struct mtk_blocktag *btag;
	struct mtk_btag_mictx *mictx;
	unsigned long flags;
	__u64 t_cur = sched_clock();
	int idx;

	btag = mtk_btag_find_by_type(mictx_id.storage);
	if (!btag)
		return;

	rcu_read_lock();
	mictx = mtk_btag_mictx_find(btag, mictx_id.id);
	if (!mictx) {
		rcu_read_unlock();
		return;
	}

	for (idx = 0; idx < btag->ctx.count; idx++) {
		struct mtk_btag_mictx_data *data = mictx->data;

		spin_lock_irqsave(&data[idx].lock, flags);
		if ((t_cur - data[idx].window_begin) > MICTX_RESET_NS)
			mtk_btag_mictx_reset(data + idx, t_cur);
		spin_unlock_irqrestore(&data[idx].lock, flags);
	}
	rcu_read_unlock();
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

int mtk_btag_mictx_get_data(struct mtk_btag_mictx_id mictx_id,
			    struct mtk_btag_mictx_iostat_struct *iostat)
{
	struct mtk_blocktag *btag;
	struct mtk_btag_mictx *mictx;
	unsigned long flags;
	__u64 dur = 0, idle_total = 0;
	__u64 top_total = 0, rw_total = 0;
	__u64 time_cur = sched_clock();
	int i;

	if (!iostat)
		return -1;

	btag = mtk_btag_find_by_type(mictx_id.storage);
	if (!btag)
		return -1;

	rcu_read_lock();
	mictx = mtk_btag_mictx_find(btag, mictx_id.id);
	if (!mictx) {
		rcu_read_unlock();
		return -1;
	}

	memset(iostat, 0, sizeof(struct mtk_btag_mictx_iostat_struct));
	for (i = 0; i < btag->ctx.count; i++) {
		struct mtk_btag_mictx_data tmp_data, *data = mictx->data;
		__u64 tp_dur;

		spin_lock_irqsave(&data[i].lock, flags);
		memcpy(&tmp_data, &data[i], sizeof(struct mtk_btag_mictx_data));
		mtk_btag_mictx_reset(data + i, time_cur);
		spin_unlock_irqrestore(&data[i].lock, flags);

		dur = time_cur - tmp_data.window_begin;

		/* calculate throughput (per-request) */
		iostat->tp_req_r += mtk_btag_eval_tp_speed(
			tmp_data.tp.r.size, tmp_data.tp.r.usage);
		iostat->tp_req_w += mtk_btag_eval_tp_speed(
			tmp_data.tp.w.size, tmp_data.tp.w.usage);

		/* calculate throughput (overlapped, not 100% precise) */
		tp_dur = tmp_data.tp_max_time - tmp_data.tp_min_time;
		iostat->tp_all_r += mtk_btag_eval_tp_speed(
			tmp_data.tp.r.size, tp_dur);
		iostat->tp_all_w += mtk_btag_eval_tp_speed(
			tmp_data.tp.w.size, tp_dur);

		/* provide request count and size */
		iostat->reqcnt_r += tmp_data.req.r.count;
		iostat->reqsize_r += tmp_data.req.r.size;
		iostat->reqcnt_w += tmp_data.req.w.count;
		iostat->reqsize_w += tmp_data.req.w.size;

		/* calculate idle total */
		if (tmp_data.idle_begin)
			tmp_data.idle_total += (time_cur - tmp_data.idle_begin);
		idle_total += tmp_data.idle_total;

		/* calculate top_total and rw_total */
		if (tmp_data.req.r.size || tmp_data.req.w.size) {
			tmp_data.req.r.size >>= 12;
			tmp_data.req.w.size >>= 12;
			tmp_data.req.r.size_top >>= 12;
			tmp_data.req.w.size_top >>= 12;
			top_total += tmp_data.req.r.size_top + tmp_data.req.w.size_top;
			rw_total += tmp_data.req.r.size + tmp_data.req.w.size;
		}

		/* fill-in cmdq depth */
		if (btag->vops->mictx_eval_wqd) {
			iostat->q_depth +=
				btag->vops->mictx_eval_wqd(&tmp_data, time_cur);
		} else {
			iostat->q_depth += tmp_data.q_depth;
		}
	}
	rcu_read_unlock();

	/* fill-in duration */
	iostat->duration = dur;

	/* calculate workload */
	iostat->wl = 100 - div64_u64(idle_total * 100, dur * btag->ctx.count);

	/* calculate top ratio */
	if (!rw_total)
		iostat->top = 0;
	else
		iostat->top = top_total * 100 / rw_total;

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_btag_mictx_get_data);

static int mtk_btag_mictx_alloc(enum mtk_btag_storage_type type)
{
	struct mtk_blocktag *btag;
	struct mtk_btag_mictx *mictx;
	struct mtk_btag_mictx_data *data;
	unsigned long flags;
	__u64 time = sched_clock();
	int i;

	btag = mtk_btag_find_by_type(type);
	if (!btag)
		return -1;

	mictx = kzalloc(sizeof(struct mtk_btag_mictx), GFP_NOFS);
	if (!mictx)
		return -1;

	data = kcalloc(btag->ctx.count, sizeof(struct mtk_btag_mictx_data),
		       GFP_NOFS);
	if (!data) {
		kfree(mictx);
		return -1;
	}

	for (i = 0; i < btag->ctx.count; i++) {
		data[i].window_begin = time;
		spin_lock_init(&data[i].lock);
	}
	mictx->data = data;

	spin_lock_irqsave(&btag->ctx.mictx.list_lock, flags);
	mictx->id = btag->ctx.mictx.last_unused_id;
	btag->ctx.mictx.nr_list++;
	btag->ctx.mictx.last_unused_id++;
	list_add_tail_rcu(&mictx->list, &btag->ctx.mictx.list);
	spin_unlock_irqrestore(&btag->ctx.mictx.list_lock, flags);

	return mictx->id;
}

static void mtk_btag_mictx_free(struct mtk_btag_mictx_id *mictx_id)
{
	struct mtk_blocktag *btag;
	struct mtk_btag_mictx *mictx;
	unsigned long flags;

	btag = mtk_btag_find_by_type(mictx_id->storage);
	if (!btag)
		return;

	spin_lock_irqsave(&btag->ctx.mictx.list_lock, flags);
	list_for_each_entry(mictx, &btag->ctx.mictx.list, list)
		if (mictx->id == mictx_id->id)
			goto found;
	spin_unlock_irqrestore(&btag->ctx.mictx.list_lock, flags);
	return;

found:
	list_del_rcu(&mictx->list);
	btag->ctx.mictx.nr_list--;
	spin_unlock_irqrestore(&btag->ctx.mictx.list_lock, flags);

	synchronize_rcu();
	kfree(mictx->data);
	kfree(mictx);
}

void mtk_btag_mictx_free_all(struct mtk_blocktag *btag)
{
	struct mtk_btag_mictx *mictx, *n;
	unsigned long flags;
	LIST_HEAD(free_list);

	spin_lock_irqsave(&btag->ctx.mictx.list_lock, flags);
	list_splice_init(&btag->ctx.mictx.list, &free_list);
	btag->ctx.mictx.nr_list = 0;
	spin_unlock_irqrestore(&btag->ctx.mictx.list_lock, flags);

	synchronize_rcu();
	list_for_each_entry_safe(mictx, n, &free_list, list) {
		list_del(&mictx->list);
		kfree(mictx->data);
		kfree(mictx);
	}
}

void mtk_btag_mictx_enable(struct mtk_btag_mictx_id *mictx_id, bool enable)
{
	if (enable)
		mictx_id->id = mtk_btag_mictx_alloc(mictx_id->storage);
	else
		mtk_btag_mictx_free(mictx_id);
}
EXPORT_SYMBOL_GPL(mtk_btag_mictx_enable);

void mtk_btag_mictx_init(struct mtk_blocktag *btag)
{
	spin_lock_init(&btag->ctx.mictx.list_lock);
	btag->ctx.mictx.nr_list = 0;
	INIT_LIST_HEAD(&btag->ctx.mictx.list);
}
