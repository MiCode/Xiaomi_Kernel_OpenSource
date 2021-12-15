// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/slab.h>

#include <linux/rbtree.h>
#include <linux/debugfs.h>
#include <apusys_device.h>
#include <linux/ktime.h>

#include "mdw_cmn.h"
#include "mdw_cmd.h"
#include "mdw_rsc.h"
#include "mdw_queue.h"

#define MAX_BOOST (100)

static struct mdw_cmd_parser *cmd_parser;

//--------------------------------------
int mdw_queue_task_start(struct mdw_apu_sc *sc)
{
	struct mdw_queue *mq = NULL;
	int ret = 0;

	mdw_flw_debug("\n");

	/* get queue */
	mq = mdw_rsc_get_queue(sc->type);
	if (!mq)
		return -ENODEV;

	/* normal task*/
	if (sc->period == 0) {
		struct deadline_root *rt_root;
		struct mdw_rsc_tab *rt_tab;

		if (sc->type < APUSYS_DEVICE_RT) {
			rt_tab = mdw_rsc_get_tab(sc->type + APUSYS_DEVICE_RT);
			rt_root = &rt_tab->q.deadline;
			if (rt_tab != NULL && rt_root->need_timer)
				sc->cluster_size = INT_MAX;
		}
	}

	if (cmd_parser->is_deadline(sc)) {
		ret = mq->deadline.ops.task_start(sc, &mq->deadline);
		if (!ret) {
			mutex_lock(&mq->mtx);
			mq->deadline_task_num++;
			mutex_unlock(&mq->mtx);
		}
	} else {
		ret = mq->norm.ops.task_start(sc, &mq->norm);
		if (!ret) {
			mutex_lock(&mq->mtx);
			mq->normal_task_num++;
			mutex_unlock(&mq->mtx);
		}
	}

	return ret;
}

int mdw_queue_task_end(struct mdw_apu_sc *sc)
{
	struct mdw_queue *mq = NULL;
	int ret = 0;

	mdw_flw_debug("\n");

	/* get queue */
	mq = mdw_rsc_get_queue(sc->type);
	if (!mq)
		return -ENODEV;

	if (cmd_parser->is_deadline(sc)) {
		ret = mq->deadline.ops.task_end(sc, &mq->deadline);
		if (!ret) {
			mutex_lock(&mq->mtx);
			mq->deadline_task_num--;
			mutex_unlock(&mq->mtx);
		}
	} else {
		ret = mq->norm.ops.task_end(sc, &mq->norm);
		mutex_lock(&mq->mtx);
		mq->normal_task_num--;
		mutex_unlock(&mq->mtx);
	}

	return ret;
}

struct mdw_apu_sc *mdw_queue_pop(int type)
{
	struct mdw_apu_sc *sc = NULL;
	struct mdw_queue *mq = NULL;

	mdw_flw_debug("\n");

	/* get queue */
	mq = mdw_rsc_get_queue(type);
	if (!mq)
		return NULL;

	/* get sc */
	if (mq->deadline.ops.len(&mq->deadline))
		sc = mq->deadline.ops.pop(&mq->deadline);
	else
		sc = mq->norm.ops.pop(&mq->norm);

	if (sc)
		getnstimeofday(&sc->ts_deque);

	return sc;
}

int mdw_queue_insert(struct mdw_apu_sc *sc, int is_front)
{
	struct mdw_queue *mq = NULL;

	mdw_flw_debug("\n");
	getnstimeofday(&sc->ts_enque);

	/* get queue */
	mq = mdw_rsc_get_queue(sc->type);
	if (!mq)
		return -ENODEV;

	if (cmd_parser->is_deadline(sc))
		return mq->deadline.ops.insert(sc, &mq->deadline, is_front);

	return mq->norm.ops.insert(sc, &mq->norm, is_front);
}

int mdw_queue_len(int type, int is_deadline)
{
	struct mdw_queue *mq = NULL;

	mdw_flw_debug("\n");

	/* get queue */
	mq = mdw_rsc_get_queue(type);
	if (!mq)
		return 0;

	if (is_deadline)
		return mq->deadline.ops.len(&mq->deadline);

	return mq->norm.ops.len(&mq->norm);
}

int mdw_queue_delete(struct mdw_apu_sc *sc)
{
	struct mdw_queue *mq = NULL;

	mdw_flw_debug("\n");

	/* get queue */
	mq = mdw_rsc_get_queue(sc->type);
	if (!mq)
		return -ENODEV;

	if (cmd_parser->is_deadline(sc))
		return mq->deadline.ops.delete(sc, &mq->deadline);

	return mq->norm.ops.delete(sc, &mq->norm);
}

int mdw_queue_boost(struct mdw_apu_sc *sc)
{
	struct mdw_rsc_tab *tab = NULL;
	struct deadline_root *root;
	unsigned int suggest_time;

	if (sc == NULL)
		return 0;

	suggest_time = sc->hdr->suggest_time * 1000;

	tab = mdw_rsc_get_tab(sc->type);
	root = &tab->q.deadline;

	if (sc->hdr->suggest_time != 0) {
		if (sc->hdr->driver_time < suggest_time)
			sc->boost -= 10;
		else if (sc->hdr->driver_time > suggest_time)
			sc->boost += 10;

		if (sc->boost > 100)
			sc->boost = 100;
		if (sc->boost < 0)
			sc->boost = 0;
	}

	if (root->load_boost || root->trace_boost)
		return MAX_BOOST;
	else
		return sc->boost;
}

void mdw_queue_destroy(struct mdw_queue *mq)
{
	mdw_flw_debug("\n");
	mq->deadline.ops.destroy(&mq->deadline);
	mq->norm.ops.destroy(&mq->norm);
}

int mdw_queue_init(struct mdw_queue *mq)
{
	int ret = 0;

	mdw_flw_debug("\n");

	cmd_parser = mdw_cmd_get_parser();
	if (!cmd_parser)
		return -ENODEV;

	memset(mq, 0, sizeof(struct mdw_queue));

	mutex_init(&mq->mtx);

	ret = mdw_queue_deadline_init(&mq->deadline);
	if (ret)
		goto out;

	ret = mdw_queue_norm_init(&mq->norm);
	if (ret)
		goto fail_init_norm;

fail_init_norm:
	mq->deadline.ops.destroy(&mq->deadline);
out:
	return ret;
}
