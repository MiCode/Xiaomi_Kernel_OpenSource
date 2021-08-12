// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/kref.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>

#include "mdw_cmn.h"
#include "mdw_trace.h"
#include "mdw_ap.h"
#include "mdw_ap_tag.h"
#define CREATE_TRACE_POINTS
#include "mdw_ap_events.h"

struct mdw_sched_mgr {
	struct task_struct *task;
	struct completion cmplt;

	struct list_head ds_list; //done sc list
	struct mutex mtx;

	bool pause;
	bool stop;
};

static struct mdw_sched_mgr ms_mgr;

#define MDW_EXEC_PRINT " pid(%d/%d) cmd(%p/0x%llx/0x%llx-#%u/%u)"\
	" dev(%d/%s-#%d) pack(%u) sched(%u/%u/%u/%u/%u/%u)"\
	" mem(%u/%u/0x%x/0x%x) boost(%u) bw(0x%x) time(%u/%u)"

static void mdw_sched_trace(struct mdw_ap_sc *sc,
	struct mdw_dev_info *d, struct apusys_cmd_handle *h, int ret, int done)
{
	struct mdw_tag_pack {
		union {
			uint64_t val;
			struct {
				uint16_t sc_idx;
				uint16_t num_sc;
				uint16_t dev_type;
				uint16_t dev_idx;
			} __packed s;
			struct {
				uint32_t pack_id;
				uint32_t multi_num;
			} __packed m;
			struct {
				uint16_t prio;
				uint16_t soft_limit;
				uint16_t hard_limit;
				uint16_t suggest_time;
			} __packed e;
			struct {
				uint16_t ctx;
				uint16_t vlm_ctx;
				uint16_t vlm_usage;
				uint16_t tcm_real_size;
			} __packed t;
		};
	};
	struct mdw_tag_pack sc_info, multi_info, exec_info, tcm_info;
	char state[16];

	/* prefix */
	memset(state, 0, sizeof(state));
	if (!done) {
		if (snprintf(state, sizeof(state)-1, "start :") < 0)
			return;
	} else {
		if (ret) {
			if (snprintf(state, sizeof(state)-1, "fail :") < 0)
				return;
		} else {
			if (snprintf(state, sizeof(state)-1, "done :") < 0)
				return;
		}
	}

	/* if err, use mdw_drv_err */
	if (ret) {
		mdw_drv_err("%s"MDW_EXEC_PRINT" ret(%d)\n",
			state,
			sc->parent->pid,
			sc->parent->tgid,
			sc->parent->c->mpriv,
			sc->parent->c->uid,
			sc->parent->c->kid,
			sc->idx,
			sc->parent->c->num_subcmds,
			d->type,
			d->name,
			d->idx,
			sc->hdr->info->pack_id,
			sc->parent->c->priority,
			sc->parent->c->softlimit,
			sc->parent->c->hardlimit,
			0, //sc->hdr->info->ip_time,
			sc->hdr->info->suggest_time,
			sc->parent->c->power_save,
			sc->vlm_ctx,
			sc->hdr->info->vlm_force,
			sc->hdr->info->vlm_usage,
			sc->tcm_real_size,
			h->boost,
			sc->bw,
			h->ip_time,
			sc->driver_time,
			ret);
	} else {
		mdw_drv_debug("%s"MDW_EXEC_PRINT" ret(%d)\n",
			state,
			sc->parent->pid,
			sc->parent->tgid,
			sc->parent->c->mpriv,
			sc->parent->c->uid,
			sc->parent->c->kid,
			sc->idx,
			sc->parent->c->num_subcmds,
			d->type,
			d->name,
			d->idx,
			sc->hdr->info->pack_id,
			sc->parent->c->priority,
			sc->parent->c->softlimit,
			sc->parent->c->hardlimit,
			0, //sc->hdr->info->ip_time,
			sc->hdr->info->suggest_time,
			sc->parent->c->power_save,
			sc->vlm_ctx,
			sc->hdr->info->vlm_force,
			sc->hdr->info->vlm_usage,
			sc->tcm_real_size,
			h->boost,
			sc->bw,
			h->ip_time,
			sc->driver_time,
			ret);
	}

	/* encode info for 12 args limitation */
	sc_info.s.sc_idx = sc->idx;
	sc_info.s.num_sc = sc->parent->c->num_subcmds;
	sc_info.s.dev_type = d->type;
	sc_info.s.dev_idx = d->idx;

	multi_info.m.pack_id = sc->hdr->info->pack_id;
	multi_info.m.multi_num = sc->multi_total;

	exec_info.e.prio = sc->parent->c->priority;
	exec_info.e.soft_limit = sc->parent->c->softlimit;
	exec_info.e.hard_limit = sc->parent->c->hardlimit;
	exec_info.e.suggest_time = sc->hdr->info->suggest_time;

	tcm_info.t.ctx = sc->vlm_ctx;
	tcm_info.t.vlm_ctx = sc->hdr->info->vlm_ctx_id;
	tcm_info.t.vlm_usage = sc->hdr->info->vlm_usage;
	tcm_info.t.tcm_real_size = sc->tcm_real_size;

	/* trace cmd end */
	trace_mdw_ap_cmd(done,
		sc->parent->pid,
		sc->parent->tgid,
		sc->parent->c->kid,
		sc_info.val,
		d->name,
		multi_info.val,
		exec_info.val,
		tcm_info.val,
		h->boost,
		h->ip_time,
		ret);
}
#undef MDW_EXEC_PRINT

static int mdw_sched_sc_done(void)
{
	struct mdw_ap_cmd *ac = NULL;
	struct mdw_ap_sc *sc = NULL, *s = NULL;
	int ret = 0;

	mdw_flw_debug("\n");

	mdw_trace_begin("check done list|%s", __func__);

	/* get done sc from done sc list */
	mutex_lock(&ms_mgr.mtx);
	s = list_first_entry_or_null(&ms_mgr.ds_list,
		struct mdw_ap_sc, ds_item);
	if (s)
		list_del(&s->ds_item);
	mutex_unlock(&ms_mgr.mtx);
	if (!s) {
		ret = -ENODATA;
		goto out;
	}
	/* recv finished subcmd */
	while (1) {
		ac = s->parent;
		ret = mdw_ap_parser.end_sc(s, &sc);
		mdw_flw_debug("\n");
		/* check return value */
		if (ret) {
			mdw_drv_err("parse done sc fail(%d)\n", ret);
			ac->c->complete(ac->c, ret);
			break;
		}
		/* check parsed sc */
		if (sc) {
		/*
		 * finished sc parse ok, should be call
		 * again because residual sc
		 */
			mdw_flw_debug("sc(0x%llx-#%d)\n",
				sc->parent->c->kid, sc->idx);
			mdw_sched(sc);
		} else {
		/* finished sc parse done, break loop */
			mdw_sched(NULL);
			break;
		}
	};

out:
	mdw_trace_end("check done list|%s", __func__);
	return ret;
}

static void mdw_sched_enque_done_sc(struct mdw_ap_sc *sc)
{
	mdw_flw_debug("sc(0x%llx-#%d)\n", sc->parent->c->kid, sc->idx);
	mdw_ap_parser.put_ctx(sc);

	mutex_lock(&ms_mgr.mtx);
	list_add_tail(&sc->ds_item, &ms_mgr.ds_list);
	mutex_unlock(&ms_mgr.mtx);

	mdw_sched(NULL);
}

int mdw_sched_dev_routine(void *arg)
{
	int ret = 0;
	struct mdw_dev_info *d = (struct mdw_dev_info *)arg;
	struct apusys_cmd_handle h;
	struct mdw_ap_sc *sc = NULL;

	/* execute */
	while (!kthread_should_stop() && d->stop == false) {
		mdw_flw_debug("\n");
		ret = wait_for_completion_interruptible(&d->cmplt);
		if (ret)
			goto next;

		mdw_trace_begin("dev(%s-%d) routine", d->name, d->idx);

		sc = (struct mdw_ap_sc *)d->sc;
		if (!sc) {
			mdw_drv_warn("no sc to exec\n");
			goto next;
		}

		/* get mem ctx */
		if (mdw_ap_parser.get_ctx(sc)) {
			mdw_drv_err("cmd(0x%llx-#%d) get ctx fail\n",
				sc->parent->c->kid, sc->idx);
			goto next;
		}

		/* construct cmd hnd */
		mdw_queue_boost(sc);
		ret = mdw_ap_parser.set_hnd(sc, d->idx, &h);
		if (ret) {
			mdw_drv_err("set hnd(0x%llx-%d) fail\n",
				sc->parent->c->kid, sc->idx);
			sc->ret = ret;
			mdw_sched_enque_done_sc(sc);
			goto next;
		}

		/*
		 * Execute reviser to switch VLM:
		 * Skip set context on preemptive command,
		 * context should be set by engine driver itself.
		 * Give engine a callback to set context id.
		 */
		if (d->type != APUSYS_DEVICE_MDLA &&
			d->type != APUSYS_DEVICE_MDLA_RT) {
			mdw_rvs_set_ctx(d->type,
					d->idx, sc->vlm_ctx);
		}

		/* count qos start */
		mdw_qos_cmd_start(sc->parent->c->kid, sc->idx,
			sc->type, d->idx, h.boost);

		/* execute */
		mdw_sched_trace(sc, d, &h, ret, 0);

		mdw_trace_begin("dev(%s-%d) exec|sc(0x%llx-%d) boost(%u/%u)",
			d->name, d->idx, sc->parent->c->kid,
			sc->idx, h.boost, sc->boost);
		ktime_get_ts64(&sc->ts_start);
		ret = d->dev->send_cmd(APUSYS_CMD_EXECUTE, &h, d->dev);
		ktime_get_ts64(&sc->ts_end);
		sc->driver_time = mdw_cmn_get_time_diff(&sc->ts_start,
			&sc->ts_end);
		mdw_trace_end("dev(%s-%d) exec|sc(0x%llx-%d) time(%u/%u)",
			d->name, d->idx, sc->parent->c->kid,
			sc->idx, sc->driver_time, sc->ip_time);

		mdw_sched_trace(sc, d, &h, ret, 1);

		/* count qos end */
		sc->bw += mdw_qos_cmd_end(sc->parent->c->kid, sc->idx,
			sc->type, d->idx);
		sc->ip_time = sc->ip_time > h.ip_time ? sc->ip_time : h.ip_time;
		sc->boost = h.boost;
		sc->ret = ret;

		mdw_ap_parser.clear_hnd(&h);

		mdw_trace_begin("dev(%s-%d) put dev", d->name, d->idx);
		/* put device */
		if (mdw_rsc_put_dev(d))
			mdw_drv_err("put dev(%d-#%d) fail\n",
				d->type, d->dev->idx);
		mdw_trace_end("dev(%s-%d) put dev", d->name, d->idx);
		mdw_trace_begin("dev(%s-%d) done sc", d->name, d->idx);
		mdw_sched_enque_done_sc(sc);
		mdw_trace_end("dev(%s-%d) done sc", d->name, d->idx);

next:
		mdw_flw_debug("done\n");
		mdw_trace_end("dev(%s-%d) routine", d->name, d->idx);
		continue;
	}

	complete(&d->thd_done);

	return 0;
}

static int mdw_sched_get_type(uint64_t bmp)
{
	unsigned long tmp[BITS_TO_LONGS(APUSYS_DEVICE_MAX)];

	memset(&tmp, 0, sizeof(tmp));
	bitmap_from_arr32(tmp, (const uint32_t *)&bmp, APUSYS_DEVICE_MAX);

	return find_last_bit((unsigned long *)&tmp, APUSYS_DEVICE_MAX);
}

static int mdw_sched_dispatch(struct mdw_ap_sc *sc)
{
	int ret = 0;

	mdw_trace_begin("mdw dispatch|sc(0x%llx-%d) pack(%d)",
		sc->parent->c->kid, sc->idx, sc->hdr->info->pack_id);

	if (sc->hdr->info->pack_id)
		ret = mdw_dispr_pack(sc);
	else
		ret = mdw_dispr_norm(sc);

	mdw_trace_end("mdw dispatch|ret(%d)", ret);

	return ret;
}

static int mdw_sched_routine(void *arg)
{
	int ret = 0;
	struct mdw_ap_sc *sc = NULL;
	uint64_t bmp = 0;
	int t = 0;

	mdw_flw_debug("\n");

	while (!kthread_should_stop() && !ms_mgr.stop) {
		ret = wait_for_completion_interruptible(&ms_mgr.cmplt);
		if (ret)
			mdw_drv_warn("sched ret(%d)\n", ret);

		if (ms_mgr.pause == true)
			continue;

		if (!mdw_sched_sc_done()) {
			mdw_sched(NULL);
			goto next;
		}

		mdw_dispr_check();

		bmp = mdw_rsc_get_avl_bmp();
		t = mdw_sched_get_type(bmp);
		if (t < 0 || t >= APUSYS_DEVICE_MAX) {
			mdw_flw_debug("nothing to sched(%d)\n", t);
			goto next;
		}

		/* get queue */
		sc = mdw_queue_pop(t);
		if (!sc) {
			mdw_drv_err("pop sc(%d) fail\n", t);
			goto fail_pop_sc;
		}
		mdw_flw_debug("pop sc(0x%llx-#%d/%d/%llu)\n",
			sc->parent->c->kid, sc->idx, sc->type, sc->period);

		/* dispatch cmd */
		ret = mdw_sched_dispatch(sc);
		if (ret) {
			mdw_flw_debug("sc(0x%llx-#%d) dispatch fail",
				sc->parent->c->kid, sc->idx);
			goto fail_exec_sc;
		}

		goto next;

fail_exec_sc:
	if (mdw_queue_insert(sc, true)) {
		mdw_drv_err("sc(0x%llx-#%d) insert fail\n",
			sc->parent->c->kid, sc->idx);
	}
fail_pop_sc:
next:
		mdw_flw_debug("\n");
		if (mdw_rsc_get_avl_bmp())
			mdw_sched(NULL);
	}

	mdw_drv_warn("schedule thread end\n");

	return 0;
}

int mdw_sched(struct mdw_ap_sc *sc)
{
	int ret = 0;

	mdw_flw_debug("\n");

	/* no input sc, trigger sched thread only */
	if (!sc) {
		complete(&ms_mgr.cmplt);
		return 0;
	}
	mdw_flw_debug("\n");

	/* insert sc to queue */
	ret = mdw_queue_insert(sc, false);
	if (ret) {
		mdw_drv_err("sc(0x%llx-#%d) enque fail\n",
			sc->parent->c->kid, sc->idx);
		return ret;
	}

	complete(&ms_mgr.cmplt);

	mdw_flw_debug("\n");

	return 0;
}

int mdw_sched_pause(void)
{
	struct mdw_dev_info *d = NULL;
	int type = 0, idx = 0, ret = 0, i = 0;

	if (ms_mgr.pause == true) {
		mdw_drv_warn("pause ready\n");
		return 0;
	}

	ms_mgr.pause = true;

	for (type = 0; type < APUSYS_DEVICE_MAX; type++) {
		for (idx = 0; idx < mdw_rsc_get_dev_num(type); idx++) {
			d = mdw_rsc_get_dinfo(type, idx);
			if (!d)
				continue;
			ret = d->suspend(d);
			if (ret) {
				mdw_drv_err("dev(%s%d) suspend fail(%d)\n",
					d->name, d->idx, ret);
				goto fail_sched_pause;
			}
		}
	}

	mdw_drv_info("pause\n");
	goto out;

fail_sched_pause:
	for (idx -= 1; idx >= 0; idx--) {
		d = mdw_rsc_get_dinfo(type, idx);
		if (!d) {
			mdw_drv_warn("dev(%d-%d) get fail\n",
				type, idx);
			continue;
		}
		if (d->resume(d)) {
			mdw_drv_err("dev(%s%d) resume fail(%d)\n",
				d->name, d->idx, ret);
		}
	}

	for (i = 0; i < type; i++) {
		for (idx = 0; idx < mdw_rsc_get_dev_num(type); idx++) {
			d = mdw_rsc_get_dinfo(type, idx);
			if (!d) {
				mdw_drv_warn("dev(%d-%d) get fail\n",
					type, idx);
				continue;
			}
			if (d->resume(d)) {
				mdw_drv_err("dev(%s%d) resume fail(%d)\n",
					d->name, d->idx, ret);
			}
		}
	}

	ms_mgr.pause = false;
	mdw_drv_warn("resume\n");
out:
	return ret;
}

void mdw_sched_restart(void)
{
	struct mdw_dev_info *d = NULL;
	int type = 0, idx = 0, ret = 0;

	if (ms_mgr.pause == false)
		mdw_drv_warn("resume ready\n");
	else
		mdw_drv_info("resume\n");

	for (type = 0; type < APUSYS_DEVICE_MAX; type++) {
		for (idx = 0; idx < mdw_rsc_get_dev_num(type); idx++) {
			d = mdw_rsc_get_dinfo(type, idx);
			if (!d)
				continue;
			ret = d->resume(d);
			if (ret)
				mdw_drv_err("dev(%s%d) resume fail(%d)\n",
				d->name, d->idx, ret);
		}
	}

	ms_mgr.pause = false;
	mdw_sched(NULL);
}

int mdw_sched_init(void)
{
	memset(&ms_mgr, 0, sizeof(ms_mgr));
	ms_mgr.pause = false;
	ms_mgr.stop = false;
	init_completion(&ms_mgr.cmplt);
	INIT_LIST_HEAD(&ms_mgr.ds_list);
	mutex_init(&ms_mgr.mtx);

	ms_mgr.task = kthread_run(mdw_sched_routine,
		NULL, "apusys_sched");
	if (!ms_mgr.task) {
		mdw_drv_err("create kthread(sched) fail\n");
		return -ENOMEM;
	}

	mdw_dispr_init();

	return 0;
}

void mdw_sched_deinit(void)
{
	ms_mgr.stop = true;
	mdw_sched(NULL);
	mdw_dispr_deinit();
}
