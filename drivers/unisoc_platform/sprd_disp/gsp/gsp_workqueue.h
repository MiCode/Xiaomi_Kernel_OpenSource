/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _GSP_WORKQUEUE_H
#define _GSP_WORKQUEUE_H

#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/wait.h>

struct gsp_core;
struct gsp_dev;

struct gsp_workqueue {
	struct list_head fill_head;
	struct list_head empty_head;

	/*
	 * one workqueue can only owe one separate object
	 * indexed by the processing kcfg
	 */
	struct list_head *sep;

	/*
	 * there is no need to create sep_lock because only
	 * core kthread can access the separate kcfg
	 */
	struct mutex fill_lock;
	struct mutex empty_lock;

	int fill_cnt;
	int empty_cnt;

	wait_queue_head_t empty_wait;
	int init;

	struct gsp_core *attached_core;
};


int gsp_workqueue_init(struct gsp_workqueue *wq, struct gsp_core *core);

struct gsp_kcfg *gsp_workqueue_pull(struct gsp_workqueue *wq);

int gsp_workqueue_push(struct gsp_kcfg *kcfg, struct gsp_workqueue *const wq);

struct gsp_kcfg *gsp_workqueue_acquire(struct gsp_workqueue *wq);

int gsp_workqueue_is_exhausted(struct gsp_workqueue *wq);

void gsp_workqueue_filled_invalidate(struct gsp_workqueue *wq);

void gsp_workqueue_put(struct gsp_kcfg *kcfg, struct gsp_workqueue *wq);

void gsp_workqueue_cancel(struct gsp_kcfg *kcfg, struct gsp_workqueue *wq);

int gsp_workqueue_is_filled(struct gsp_workqueue *wq);

int gsp_workqueue_get_empty_kcfg_num(struct gsp_workqueue *wq);

int gsp_workqueue_get_fill_kcfg_num(struct gsp_workqueue *wq);
#endif
