// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/kref.h>
#include <linux/kthread.h>

#include "mdw_cmn.h"
#include "mdw_rsc.h"
#include "mdw_cmd.h"
#include "mdw_sched.h"
#include "mdw_pack.h"

struct mdw_pack_item {
	int pack_id;
	int target_sc_num;
	int cur_sc_num;
	struct mdw_apu_cmd *c;

	struct mdw_rsc_req req;
	struct list_head sc_list; // for mdw_apu_sc
	struct list_head c_item; // to mdw_apu_cmd
	struct list_head p_item; //to pack mgr

	struct mutex mtx;
};

struct mdw_pack_mgr {
	struct list_head dp_list; //for pack item(done)
	struct mutex mtx;
};

struct mdw_pack_mgr p_mgr;

static struct mdw_pack_item *mdw_pack_get_item(struct mdw_apu_sc *sc)
{
	struct mdw_pack_item *pki = NULL;
	struct mdw_apu_cmd *c = sc->parent;
	struct list_head *tmp = NULL, *list_ptr = NULL;

	/* check packid */
	if (sc->hdr->pack_id < 0 || sc->hdr->pack_id >= 64)
		return NULL;

	/* get mem from usr list */
	mutex_lock(&c->mtx);
	list_for_each_safe(list_ptr, tmp, &c->pack_list) {
		pki = list_entry(list_ptr, struct mdw_pack_item, c_item);
		if (pki->pack_id == sc->hdr->pack_id)
			break;
		pki = NULL;
	}

	/* alloc new pack item */
	if (pki)
		goto out;

	pki = vzalloc(sizeof(struct mdw_pack_item));
	if (!pki)
		goto out;

	INIT_LIST_HEAD(&pki->sc_list);
	INIT_LIST_HEAD(&pki->req.d_list);
	mutex_init(&pki->mtx);
	pki->pack_id = sc->hdr->pack_id;
	pki->target_sc_num = c->pack_cnt[pki->pack_id];
	pki->c = c;
	list_add_tail(&pki->c_item, &c->pack_list);
	mdw_flw_debug("pki(%p) create\n", pki);

out:
	mutex_unlock(&c->mtx);
	return pki;
}

static void mdw_pack_async_cb(struct mdw_rsc_req *r)
{
	struct mdw_pack_item *pki =
			container_of(r, struct mdw_pack_item, req);

	mdw_flw_debug("pki(%p) done\n", pki);

	/* add to mgr's done list */
	mutex_lock(&p_mgr.mtx);
	list_add_tail(&pki->p_item, &p_mgr.dp_list);
	mutex_unlock(&p_mgr.mtx);

	mdw_sched(NULL);
}

static int mdw_pack_get_dev(struct mdw_pack_item *pki)
{
	pki->req.mode = MDW_DEV_INFO_GET_MODE_ASYNC;
	pki->req.policy = MDW_DEV_INFO_GET_POLICY_SEQ;
	pki->req.cb_async = mdw_pack_async_cb;
	mdw_flw_debug("pki(%p)\n", pki);

	return mdw_rsc_get_dev(&pki->req);
}

static struct mdw_dev_info *mdw_pack_exec_get_dev(int type,
	struct mdw_pack_item *pki)
{
	struct mdw_dev_info *d = NULL;
	struct list_head *tmp = NULL, *list_ptr = NULL;

	list_for_each_safe(list_ptr, tmp, &pki->req.d_list) {
		d = list_entry(list_ptr, struct mdw_dev_info, r_item);
		if (d->type == type) {
			list_del(&d->r_item);
			break;
		}
		d = NULL;
	}

	return d;
}

static int mdw_pack_exec(struct mdw_pack_item *pki)
{
	struct mdw_apu_sc *sc = NULL;
	struct mdw_dev_info *d = NULL;
	struct mdw_apu_cmd *c = pki->c;
	struct list_head *tmp = NULL, *list_ptr = NULL;
	int ret = 0;

	mdw_flw_debug("pki(%p)\n", pki);

	mutex_lock(&pki->mtx);
	list_for_each_safe(list_ptr, tmp, &pki->sc_list) {
		sc = list_entry(list_ptr, struct mdw_apu_sc, pk_item);
		d = mdw_pack_exec_get_dev(sc->type, pki);
		if (d) {
			mutex_lock(&sc->mtx);
			sc->multi_total = 1;
			kref_init(&sc->multi_ref);
			mutex_unlock(&sc->mtx);
			ret = d->exec(d, sc);
		}
		if (!d || ret)
			mdw_drv_err("sc(0x%llx-#%d) fail(%p/%d)\n",
				sc->parent->kid, sc->idx, d, ret);
	}
	mutex_unlock(&pki->mtx);

	mdw_flw_debug("pki(%p) destroy\n", pki);
	mutex_lock(&c->mtx);
	list_del(&pki->c_item);
	vfree(pki);
	mutex_unlock(&c->mtx);

	return ret;
}

int mdw_pack_check(void)
{
	struct mdw_pack_item *pki = NULL;

	mutex_lock(&p_mgr.mtx);
	pki = list_first_entry_or_null(&p_mgr.dp_list,
		struct mdw_pack_item, p_item);
	if (pki)
		list_del(&pki->p_item);
	mutex_unlock(&p_mgr.mtx);

	if (!pki)
		return -ENODATA;

	mdw_flw_debug("\n");
	return mdw_pack_exec(pki);
}

int mdw_pack_dispatch(struct mdw_apu_sc *sc)
{
	struct mdw_pack_item *pki = NULL;
	int ret = 0;

	pki = mdw_pack_get_item(sc);
	if (!pki)
		return -ENODATA;

	mutex_lock(&pki->mtx);
	list_add_tail(&sc->pk_item, &pki->sc_list);
	pki->cur_sc_num++;
	pki->req.num[sc->type]++;
	pki->req.acq_bmp |= (1ULL << sc->type);
	pki->req.total_num++;
	mdw_flw_debug("sc(0x%llx-#%d) pack(%d)(%d/%d)\n", sc->parent->kid,
		sc->idx, sc->hdr->pack_id, pki->cur_sc_num, pki->target_sc_num);

	if (pki->cur_sc_num >= pki->target_sc_num)
		ret = mdw_pack_get_dev(pki);

	mutex_unlock(&pki->mtx);

	return 0;
}

int mdw_pack_init(void)
{
	memset(&p_mgr, 0, sizeof(p_mgr));
	mutex_init(&p_mgr.mtx);
	INIT_LIST_HEAD(&p_mgr.dp_list);

	return 0;
}

void mdw_pack_exit(void)
{
}
