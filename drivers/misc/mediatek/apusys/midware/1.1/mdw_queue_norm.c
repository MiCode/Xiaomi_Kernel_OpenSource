// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
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
#include "mdw_cmd.h"
#include "mdw_rsc.h"
#include "mdw_queue.h"


struct mdw_pid {
	pid_t pid;
	struct mdw_prio {
		unsigned long bmp[BITS_TO_LONGS(MDW_CMD_PRIO_MAX)];
		struct list_head list[MDW_CMD_PRIO_MAX];
	} q;
	atomic_t ref;
	struct list_head q_item; //to norm queue
	struct list_head pi_list;
};

struct mdw_pid_item {
	struct mdw_pid *pid;
	struct list_head q_item; //to norm queue
	struct list_head p_item;
};

void mdw_queue_norm_pid_destroy(struct mdw_pid *p)
{
	mdw_flw_debug("pid(%d) delete\n", p->pid);
	list_del(&p->q_item);
	vfree(p);
}

static struct mdw_pid *mdw_queue_norm_pid_create(void)
{
	int i = 0;
	struct mdw_pid *p = NULL;

	p = vzalloc(sizeof(struct mdw_pid));
	if (!p)
		return NULL;

	/* init priority queue */
	for (i = 0; i < MDW_CMD_PRIO_MAX; i++)
		INIT_LIST_HEAD(&p->q.list[i]);

	atomic_set(&p->ref, 0);
	INIT_LIST_HEAD(&p->pi_list);

	return p;
}

static void mdw_queue_norm_pid_put(struct mdw_pid *p)
{
	if (atomic_dec_and_test(&p->ref))
		mdw_queue_norm_pid_destroy(p);
}

static struct mdw_pid *mdw_queue_norm_pid_get(struct mdw_queue_norm *nq,
	pid_t pid)
{
	struct mdw_pid *p = NULL;
	struct list_head *tmp = NULL, *list_ptr = NULL;

	/* 1. find mdw_pid struct from nq's pid list */
	list_for_each_safe(list_ptr, tmp, &nq->p_list) {
		p = list_entry(list_ptr, struct mdw_pid, q_item);
		mdw_flw_debug("cmd(%d/%u) matching...\n", p->pid, pid);
		if (p->pid == pid)
			break;
		p = NULL;
	}
	if (!p) {
		p = mdw_queue_norm_pid_create();
		if (!p)
			goto out;

		p->pid = (pid_t)pid;
		list_add_tail(&p->q_item, &nq->p_list);
		mdw_flw_debug("pid(%d) create\n", p->pid);
	}

	/* ref cnt++ */
	atomic_inc(&p->ref);

out:
	return p;
}
//--------------------------------------
static int mdw_queue_norm_task_start(struct mdw_apu_sc *sc, void *q)
{
	mdw_flw_debug("\n");
	return 0;
}

static int mdw_queue_norm_task_end(struct mdw_apu_sc *sc, void *q)
{
	mdw_flw_debug("\n");
	return 0;
}

static struct mdw_apu_sc *mdw_queue_norm_pop(void *q)
{
	struct mdw_apu_sc *sc = NULL;
	struct mdw_queue_norm *nq = (struct mdw_queue_norm *)q;
	struct mdw_pid *p = NULL;
	struct mdw_pid_item *pi = NULL;
	int prio = 0;

	mdw_flw_debug("\n");
	mutex_lock(&nq->mtx);

	/* get p item from nq list */
	pi = list_first_entry_or_null(&nq->pi_list,
		struct mdw_pid_item, q_item);
	if (!pi)
		goto out;

	list_del(&pi->q_item);
	list_del(&pi->p_item);
	/* get pid from p item */
	p = pi->pid;
	mdw_flw_debug("vfree pi(%p)\n", pi);
	vfree(pi);

	/* get prio from pid's q bitmap */
	prio = find_first_bit(p->q.bmp, MDW_CMD_PRIO_MAX);
	if (prio >= MDW_CMD_PRIO_MAX)
		goto fail_pop_prio;

	/* get sc from pid prio q */
	sc = list_first_entry_or_null(&p->q.list[prio],
		struct mdw_apu_sc, q_item);
	if (!sc)
		goto fail_get_sc;
	mdw_flw_debug("sc(0x%llx-#%d)\n", sc->parent->kid, sc->idx);
	list_del(&sc->q_item);

	/* check pid's q bitmap */
	if (list_empty(&p->q.list[prio]))
		bitmap_clear(p->q.bmp, prio, 1);

	/* dec norm q's count */
	nq->cnt--;

	/* put pid */
	mdw_queue_norm_pid_put(p);

	/* update mdw q's bitmap */
	mdw_rsc_update_avl_bmp(sc->type);

	goto out;

fail_get_sc:
fail_pop_prio:
	mdw_drv_err("pid(%d) no sc, pi/pq not sync\n", p->pid);
out:
	mutex_unlock(&nq->mtx);
	return sc;
}

static int mdw_queue_norm_insert(struct mdw_apu_sc *sc, void *q, int is_front)
{
	int ret = 0, prio = 0;
	struct mdw_queue_norm *nq = (struct mdw_queue_norm *)q;
	struct mdw_pid_item *pi;
	struct mdw_pid *p = NULL;

	mdw_flw_debug("sc(0x%llx-#%d)\n", sc->parent->kid, sc->idx);

	mutex_lock(&nq->mtx);

	prio = (int)sc->parent->hdr->priority;

	/* find mdw_pid struct from nq's pid list */
	p = mdw_queue_norm_pid_get(nq, sc->parent->hdr->pid);
	if (!p) {
		ret = -EINVAL;
		goto fail_get_pid;
	}

	/* alloc pid item to ptr pid */
	pi = vzalloc(sizeof(struct mdw_pid_item));
	if (!pi) {
		ret = -ENOMEM;
		goto fail_alloc_pitem;
	}

	/* add pitem to normal task queue/pid pi queue */
	mdw_flw_debug("vzalloc pi(%p)\n", pi);
	pi->pid = p;
	if (is_front) {
		list_add(&pi->q_item, &nq->pi_list);
		list_add(&pi->p_item, &p->pi_list);

		/* add sc to pid's prority queue */
		list_add(&sc->q_item, &p->q.list[prio]);
	} else {
		if (pi->pid == 0)
			list_add(&pi->q_item, &nq->pi_list);
		else
			list_add_tail(&pi->q_item, &nq->pi_list);
		list_add_tail(&pi->p_item, &p->pi_list);

		/* add sc to pid's prority queue */
		list_add_tail(&sc->q_item, &p->q.list[prio]);
	}
	bitmap_set(p->q.bmp, prio, 1);

	/* add count of normal queue */
	nq->cnt++;

	/* update mdw q's bitmap */
	mdw_rsc_update_avl_bmp(sc->type);

	goto out;

fail_alloc_pitem:
	mdw_queue_norm_pid_put(p);
fail_get_pid:
out:
	mutex_unlock(&nq->mtx);
	return ret;
}

static int mdw_queue_norm_len(void *q)
{
	struct mdw_queue_norm *nq = (struct mdw_queue_norm *)q;

	return nq->cnt;
}

static int mdw_queue_norm_delete(struct mdw_apu_sc *sc, void *q)
{
	int ret = 0, prio = 0;
	struct mdw_queue_norm *nq = (struct mdw_queue_norm *)q;
	struct mdw_pid *p = NULL;
	struct mdw_pid_item *pi = NULL;

	mdw_flw_debug("sc(0x%llx-#%d)\n", sc->parent->kid, sc->idx);

	mutex_lock(&nq->mtx);
	/* get pid by sc's pid */
	p = mdw_queue_norm_pid_get(nq, sc->parent->hdr->pid);
	if (!p) {
		ret = -ENODATA;
		goto out;
	}

	/* get p item from the last of pid's pi list and delete it */
	if (list_empty(&p->pi_list))
		goto fail_pi_empty;
	pi = list_last_entry(&p->pi_list, struct mdw_pid_item, p_item);

	list_del(&pi->p_item);
	list_del(&pi->q_item);
	mdw_flw_debug("vfree pi(%p)\n", pi);
	vfree(pi);

	/* delete sc from pid's priority q */
	prio = (int)sc->parent->hdr->priority;
	list_del(&sc->q_item);

	/* check pid's priority and update bitmap */
	if (list_empty(&p->q.list[prio]))
		bitmap_clear(p->q.bmp, prio, 1);

	/* put pid */
	mdw_queue_norm_pid_put(p);

	/* dec nq's task count */
	nq->cnt--;

	/* update mdw q's bitmap */
	mdw_rsc_update_avl_bmp(sc->type);

fail_pi_empty:
	mdw_drv_warn("no pid item in p(%d)\n", p->pid);
	mdw_queue_norm_pid_put(p);
out:
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

	mdw_flw_debug("\n");

	nq->ops.task_start = mdw_queue_norm_task_start;
	nq->ops.task_end = mdw_queue_norm_task_end;
	nq->ops.pop = mdw_queue_norm_pop;
	nq->ops.insert = mdw_queue_norm_insert;
	nq->ops.delete = mdw_queue_norm_delete;
	nq->ops.len = mdw_queue_norm_len;
	nq->ops.destroy = mdw_queue_norm_destroy;
	mutex_init(&nq->mtx);
	nq->cnt = 0;
	INIT_LIST_HEAD(&nq->p_list);
	INIT_LIST_HEAD(&nq->pi_list);

	tmp = debugfs_create_dir("normal_queue", tab->dbg_dir);
	debugfs_create_u32("length", 0444, tmp, &nq->cnt);

	return 0;
}
