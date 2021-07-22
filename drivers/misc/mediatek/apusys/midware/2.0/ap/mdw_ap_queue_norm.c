// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/atomic.h>
#include <linux/bitops.h>
#include <linux/debugfs.h>
#include <linux/bitmap.h>

#include "mdw_cmn.h"
#include "mdw_ap.h"

//--------------------------------------
static int mdw_queue_norm_task_start(struct mdw_ap_sc *sc, void *q)
{
	mdw_flw_debug("\n");
	return 0;
}

static int mdw_queue_norm_task_end(struct mdw_ap_sc *sc, void *q)
{
	mdw_flw_debug("\n");
	return 0;
}

static struct mdw_ap_sc *mdw_queue_norm_pop(void *q)
{
	struct mdw_ap_sc *sc = NULL;
	struct mdw_queue_norm *nq = (struct mdw_queue_norm *)q;
	uint32_t prio = 0;

	mdw_flw_debug("\n");
	mutex_lock(&nq->mtx);

	/* get prio from pid's q bitmap */
	prio = find_first_bit(nq->bmp, MDW_PRIORITY_MAX);
	if (prio >= MDW_PRIORITY_MAX)
		goto fail_pop_prio;

	/* get sc from pid prio q */
	sc = list_first_entry_or_null(&nq->list[prio],
		struct mdw_ap_sc, q_item);
	if (!sc)
		goto fail_get_sc;

	mdw_flw_debug("sc(0x%llx-#%d)\n", sc->parent->c->kid, sc->idx);
	list_del(&sc->q_item);

	/* check pid's q bitmap */
	if (list_empty(&nq->list[prio]))
		bitmap_clear(nq->bmp, prio, 1);

	/* dec norm q's count */
	nq->cnt--;

	/* update mdw q's bitmap */
	mdw_rsc_update_avl_bmp(sc->type);

	goto out;

fail_pop_prio:
fail_get_sc:
out:
	mutex_unlock(&nq->mtx);
	return sc;
}

static int mdw_queue_norm_insert(struct mdw_ap_sc *sc, void *q, int is_front)
{
	int ret = 0, prio = 0;
	struct mdw_queue_norm *nq = (struct mdw_queue_norm *)q;

	mdw_flw_debug("sc(0x%llx-#%d)\n", sc->parent->c->kid, sc->idx);

	mutex_lock(&nq->mtx);

	prio = (int)sc->parent->c->priority;

	/* add pitem to normal task queue/pid pi queue */
	if (is_front)
		/* add sc to pid's prority queue */
		list_add(&sc->q_item, &nq->list[prio]);
	else
		list_add_tail(&sc->q_item, &nq->list[prio]);

	bitmap_set(nq->bmp, prio, 1);

	/* add count of normal queue */
	nq->cnt++;

	/* update mdw q's bitmap */
	mdw_rsc_update_avl_bmp(sc->type);

	mutex_unlock(&nq->mtx);
	return ret;
}

static int mdw_queue_norm_len(void *q)
{
	struct mdw_queue_norm *nq = (struct mdw_queue_norm *)q;

	return (int)nq->cnt;
}

static int mdw_queue_norm_delete(struct mdw_ap_sc *sc, void *q)
{
	int ret = 0, prio = 0;
	struct mdw_queue_norm *nq = (struct mdw_queue_norm *)q;

	mdw_flw_debug("sc(0x%llx-#%d)\n", sc->parent->c->kid, sc->idx);

	mutex_lock(&nq->mtx);

	/* delete sc from pid's priority q */
	prio = (int)sc->parent->c->priority;
	list_del(&sc->q_item);

	/* check pid's priority and update bitmap */
	if (list_empty(&nq->list[prio]))
		bitmap_clear(nq->bmp, prio, 1);

	/* dec nq's task count */
	nq->cnt--;

	/* update mdw q's bitmap */
	mdw_rsc_update_avl_bmp(sc->type);

	mutex_unlock(&nq->mtx);
	return ret;
}

static void mdw_queue_norm_destroy(void *q)
{
	mdw_flw_debug("\n");
}

int mdw_queue_norm_init(struct mdw_queue_norm *nq)
{
	struct dentry *tmp;
	struct mdw_queue *mq =
			container_of(nq, struct mdw_queue, norm);
	struct mdw_rsc_tab *tab =
			container_of(mq, struct mdw_rsc_tab, q);
	unsigned int i = 0;

	mdw_flw_debug("\n");
	memset(nq, 0, sizeof(*nq));

	for (i = 0; i < MDW_PRIORITY_MAX; i++)
		INIT_LIST_HEAD(&nq->list[i]);

	nq->ops.task_start = mdw_queue_norm_task_start;
	nq->ops.task_end = mdw_queue_norm_task_end;
	nq->ops.pop = mdw_queue_norm_pop;
	nq->ops.insert = mdw_queue_norm_insert;
	nq->ops.delete = mdw_queue_norm_delete;
	nq->ops.len = mdw_queue_norm_len;
	nq->ops.destroy = mdw_queue_norm_destroy;
	mutex_init(&nq->mtx);
	nq->cnt = 0;

	tmp = debugfs_create_dir("normal_queue", tab->dbg_dir);
	debugfs_create_u32("length", 0444, tmp, &nq->cnt);

	return 0;
}
