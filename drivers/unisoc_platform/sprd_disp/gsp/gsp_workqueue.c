// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/slab.h>
#include "gsp_core.h"
#include "gsp_dev.h"
#include "gsp_debug.h"
#include "gsp_kcfg.h"
#include "gsp_sync.h"
#include "gsp_workqueue.h"

#define CREATE_TRACE_POINTS
#include "gsp_trace.h"

void gsp_workqueue_load(struct gsp_kcfg *kcfg, struct gsp_workqueue *wq)
{
	if (IS_ERR_OR_NULL(kcfg) || IS_ERR_OR_NULL(wq)) {
		GSP_ERR("work queue load params error\n");
		return;
	}

	list_add_tail(&kcfg->list, &wq->empty_head);

	/*
	 * set kcfg tag here to ensure
	 * debug conveniently
	 */
	kcfg->tag = wq->empty_cnt;
	wq->empty_cnt++;
	GSP_INFO("workqueue load kcfg[%d]\n", kcfg->tag);
}

int gsp_workqueue_is_exhausted(struct gsp_workqueue *wq)
{
	int cnt = 0;

	mutex_lock(&wq->empty_lock);
	cnt = wq->empty_cnt;
	mutex_unlock(&wq->empty_lock);

	return cnt > 0 ? 0 : 1;
}

int gsp_workqueue_is_filled(struct gsp_workqueue *wq)
{
	int ret = 0;

	mutex_lock(&wq->fill_lock);
	ret = list_empty(&wq->fill_head);
	mutex_unlock(&wq->fill_lock);

	return !ret;
}

struct gsp_core *gsp_workqueue_to_core(struct gsp_workqueue *wq)
{
	return wq->attached_core;
}

void gsp_workqueue_filled_invalidate(struct gsp_workqueue *wq)
{
	struct gsp_kcfg *kcfg = NULL;
	int i;

	if (IS_ERR_OR_NULL(wq)) {
		GSP_ERR("work queue destroy params error\n");
		return;
	}

	if (!gsp_workqueue_is_filled(wq))
		return;

	GSP_DEBUG("wq invalidate fill_cnt: %d\n",
		gsp_workqueue_get_fill_kcfg_num(wq));

	for (i = 0; i < gsp_workqueue_get_fill_kcfg_num(wq); ) {
		kcfg = gsp_workqueue_pull(wq);
		gsp_kcfg_release(kcfg);
		gsp_workqueue_put(kcfg, wq);
	}

}

void gsp_workqueue_free(struct gsp_workqueue *wq)
{
	struct list_head tmp_head;
	struct list_head *tmp;
	struct list_head *l;
	struct gsp_kcfg *kcfg = NULL;
	struct gsp_kcfg *temp = NULL;

	if (IS_ERR_OR_NULL(wq)) {
		GSP_ERR("work queue destroy params error\n");
		return;
	}

	INIT_LIST_HEAD(&tmp_head);

	mutex_lock(&wq->empty_lock);
	list_for_each_safe(l, tmp, &wq->empty_head) {
		list_del_init(l);
		list_add_tail(l, &tmp_head);
	}
	mutex_unlock(&wq->empty_lock);

	list_for_each_entry_safe(kcfg, temp, &tmp_head, list) {
		kfree((void *)kcfg);
	}

	kfree((void *)wq);
}

int gsp_workqueue_init(struct gsp_workqueue *wq, struct gsp_core *core)
{
	struct gsp_kcfg *kcfg = NULL;

	if (IS_ERR_OR_NULL(wq) || gsp_core_verify(core)) {
		GSP_ERR("initialize null work queue\n");
		return -1;
	}

	INIT_LIST_HEAD(&wq->empty_head);
	INIT_LIST_HEAD(&wq->fill_head);
	wq->sep = NULL;

	mutex_init(&wq->empty_lock);
	mutex_init(&wq->fill_lock);

	wq->empty_cnt = 0;
	wq->fill_cnt = 0;

	wq->attached_core = core;
	list_for_each_entry(kcfg, &core->kcfgs, sibling) {
		gsp_kcfg_init(kcfg, core, wq);
		gsp_workqueue_load(kcfg, wq);
	}

	if (wq->empty_cnt != CORE_MAX_KCFG_NUM(core))
		goto free;

	return 0;

free:
	gsp_workqueue_free(wq);
	return -1;
}

struct gsp_kcfg *gsp_workqueue_acquire(struct gsp_workqueue *wq)
{
	struct gsp_kcfg *kcfg = NULL;

	if (IS_ERR_OR_NULL(wq)) {
		GSP_ERR("work queue acquire params error\n");
		return kcfg;
	}

	mutex_lock(&wq->empty_lock);
	if (wq->empty_cnt > 0) {
		kcfg = list_first_entry(&wq->empty_head, struct gsp_kcfg, list);
		if (!gsp_kcfg_verify(kcfg)) {
			list_del_init(&kcfg->list);
			wq->empty_cnt--;
		}
	}
	mutex_unlock(&wq->empty_lock);

	return kcfg;
}

void gsp_workqueue_cancel(struct gsp_kcfg *kcfg, struct gsp_workqueue *wq)
{
	if (gsp_kcfg_verify(kcfg) || IS_ERR_OR_NULL(wq)) {
		GSP_ERR("work queue put params error\n");
		return;
	}

	if (!gsp_kcfg_is_pulled(kcfg)) {
		mutex_lock(&wq->fill_lock);
		list_del_init(&kcfg->list);
		wq->fill_cnt--;
		mutex_unlock(&wq->fill_lock);
	}
}

void gsp_workqueue_put(struct gsp_kcfg *kcfg, struct gsp_workqueue *wq)
{
	if (gsp_kcfg_verify(kcfg) || IS_ERR_OR_NULL(wq)) {
		GSP_ERR("work queue put params error\n");
		return;
	}

	mutex_lock(&wq->empty_lock);
	list_add_tail(&kcfg->list, &wq->empty_head);
	wq->empty_cnt++;
	mutex_unlock(&wq->empty_lock);

	trace_kcfg_put(kcfg);
}

int gsp_workqueue_push(struct gsp_kcfg *kcfg, struct gsp_workqueue *wq)
{
	int ret = -1;
	int cnt = 0;
	struct gsp_core *core = NULL;

	if (gsp_kcfg_verify(kcfg) || IS_ERR_OR_NULL(wq)) {
		GSP_ERR("work queue push params error\n");
		return ret;
	}

	mutex_lock(&wq->fill_lock);
	cnt = wq->fill_cnt;
	mutex_unlock(&wq->fill_lock);
	GSP_DEBUG("fill cnt: %d before workqueue push\n", cnt);

	core = gsp_workqueue_to_core(wq);
	mutex_lock(&wq->fill_lock);
	if (wq->fill_cnt < CORE_MAX_KCFG_NUM(core)) {
		list_add_tail(&kcfg->list, &wq->fill_head);
		wq->fill_cnt++;
		cnt = wq->fill_cnt;
		ret = 0;
	}
	mutex_unlock(&wq->fill_lock);

	trace_kcfg_push(kcfg);
	GSP_DEBUG("fill cnt: %d after workqueue push\n", cnt);
	return ret;
}

struct gsp_kcfg *gsp_workqueue_pull(struct gsp_workqueue *wq)
{
	int cnt = 0;
	struct gsp_kcfg *kcfg = NULL;

	if (IS_ERR_OR_NULL(wq)) {
		GSP_ERR("work queue push params error\n");
		return kcfg;
	}

	mutex_lock(&wq->fill_lock);
	cnt = wq->fill_cnt;
	mutex_unlock(&wq->fill_lock);
	GSP_DEBUG("fill cnt: %d before workqueue pull\n", cnt);

	mutex_lock(&wq->fill_lock);
	if (wq->fill_cnt > 0) {
		kcfg = list_first_entry(&wq->fill_head, struct gsp_kcfg, list);
		GSP_DEBUG("workqueue pull kcfg[%d]\n", gsp_kcfg_to_tag(kcfg));
		if (!gsp_kcfg_verify(kcfg)) {
			list_del_init(&kcfg->list);
			wq->fill_cnt--;
		}
	}
	cnt = wq->fill_cnt;
	mutex_unlock(&wq->fill_lock);

	trace_kcfg_pull(kcfg);
	GSP_DEBUG("fill cnt: %d after workqueue pull\n", cnt);
	return kcfg;
}

int gsp_workqueue_get_empty_kcfg_num(struct gsp_workqueue *wq)
{
	int num = 0;

	mutex_lock(&wq->empty_lock);
	num = wq->empty_cnt;
	mutex_unlock(&wq->empty_lock);

	return num;
}

int gsp_workqueue_get_fill_kcfg_num(struct gsp_workqueue *wq)
{
	int num = 0;

	mutex_lock(&wq->fill_lock);
	num = wq->fill_cnt;
	mutex_unlock(&wq->fill_lock);

	return num;
}
