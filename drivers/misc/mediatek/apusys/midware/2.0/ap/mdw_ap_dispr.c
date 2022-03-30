// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/slab.h>

#include "mdw_cmn.h"
#include "mdw_ap.h"

enum {
	MDW_DISPR_NORM,
	MDW_DISPR_PACK,
};

struct mdw_disp_item {
	int type;
	int id; // pack id(pack) or subcmd id(multi)
	int target_sc_num;
	int cur_sc_num;
	struct mdw_ap_cmd *c;

	struct mdw_rsc_req req;
	struct list_head sc_list; // for mdw_ap_sc
	struct list_head c_item; // to mdw_ap_cmd
	struct list_head p_item; //to pack mgr

	int (*exec)(struct mdw_disp_item *di);

	struct mutex mtx;
};

struct mdw_dispr_mgr {
	struct list_head ready_list;
	struct mutex mtx;
};

static struct mdw_dispr_mgr d_mgr;

static int mdw_dispr_item_put(struct mdw_disp_item *di)
{
	struct mdw_ap_cmd *c = di->c;

	mdw_flw_debug("di(%p) destroy\n", di);
	mutex_lock(&c->mtx);
	list_del(&di->c_item);
	kfree(di);
	mutex_unlock(&c->mtx);

	return 0;
}

static struct mdw_disp_item *mdw_dispr_item_get(
	struct mdw_ap_sc *sc, int disp_type)
{
	struct mdw_disp_item *di = NULL;
	struct mdw_ap_cmd *c = sc->parent;
	struct list_head *tmp = NULL, *list_ptr = NULL;

	/* get disp item from cmd list */
	mutex_lock(&c->mtx);
	list_for_each_safe(list_ptr, tmp, &c->di_list) {
		di = list_entry(list_ptr, struct mdw_disp_item, c_item);
		if (di->type == disp_type &&
			disp_type == MDW_DISPR_PACK &&
			di->id == sc->hdr->info->pack_id)
			break;

		di = NULL;
	}

	if (di)
		goto out;

	/* alloc new pack item */
	di = kzalloc(sizeof(*di), GFP_KERNEL);
	if (!di)
		goto out;

	INIT_LIST_HEAD(&di->sc_list);
	INIT_LIST_HEAD(&di->req.d_list);
	mutex_init(&di->mtx);
	di->type = disp_type;
	di->c = c;

	di->id = sc->hdr->info->pack_id;
	di->target_sc_num = c->pack_cnt[sc->hdr->info->pack_id];

	list_add_tail(&di->c_item, &c->di_list);
	mdw_flw_debug("di(%p) create, type(%d) id(%d)\n", di, di->type, di->id);

out:
	mutex_unlock(&c->mtx);
	return di;
}

static void mdw_dispr_async_cb(struct mdw_rsc_req *r)
{
	struct mdw_disp_item *di =
			container_of(r, struct mdw_disp_item, req);

	mdw_flw_debug("di(%p) done\n", di);

	/* add to mgr's done list */
	mutex_lock(&d_mgr.mtx);
	list_add_tail(&di->p_item, &d_mgr.ready_list);
	mutex_unlock(&d_mgr.mtx);

	mdw_sched(NULL);
}

static int mdw_dispr_get_dev(struct mdw_disp_item *di)
{
	int ret = 0;

	di->req.mode = MDW_DEV_INFO_GET_MODE_ASYNC;
	di->req.policy = MDW_DEV_INFO_GET_POLICY_SEQ;
	di->req.cb_async = mdw_dispr_async_cb;
	mdw_flw_debug("di(%p)\n", di);

	ret = mdw_rsc_get_dev(&di->req);

	/* ret = 0 or -EAGAIN are normal */
	if (!ret || ret == -EAGAIN)
		return 0;

	return ret;
}

static int mdw_dispr_pwron_dev(struct mdw_rsc_req *r, struct mdw_ap_sc *sc)
{
	struct mdw_dev_info *d = NULL;
	struct list_head *tmp = NULL, *list_ptr = NULL;
	int ret = 0;

	list_for_each_safe(list_ptr, tmp, &r->d_list) {
		d = list_entry(list_ptr, struct mdw_dev_info, r_item);
		ret = d->pwr_on(d, sc->boost, MDW_RSC_SET_PWR_TIMEOUT);
		if (ret) {
			mdw_drv_err("pwn on(%s-#d) fail\n", d->name, d->idx);
			break;
		}
		//sc->multi_bmp |= (1ULL << d->idx);
		mdw_flw_debug("pwron dev(%s%d)\n", d->name, d->idx);
	}

	return ret;
}

static struct mdw_dev_info *mdw_dispr_pop_dev(int type,
	struct mdw_rsc_req *r)
{
	struct mdw_dev_info *d = NULL;
	struct list_head *tmp = NULL, *list_ptr = NULL;

	list_for_each_safe(list_ptr, tmp, &r->d_list) {
		d = list_entry(list_ptr, struct mdw_dev_info, r_item);
		if (d->type == type) {
			list_del(&d->r_item);
			break;
		}
		d = NULL;
	}

	return d;
}

static int mdw_dispr_exec_pack(struct mdw_disp_item *di)
{
	struct mdw_ap_sc *sc = NULL;
	struct mdw_dev_info *d = NULL;
	struct list_head *tmp = NULL, *list_ptr = NULL;
	int ret = 0;

	mdw_flw_debug("di(%p|%d-%d)\n", di, di->type, di->id);

	mutex_lock(&di->mtx);
	list_for_each_safe(list_ptr, tmp, &di->sc_list) {
		sc = list_entry(list_ptr, struct mdw_ap_sc, di_item);
		d = mdw_dispr_pop_dev(sc->hdr->info->type, &di->req);
		if (d) {
			sc->multi_total = di->req.num[sc->type];
			mdw_flw_debug("sc(0x%llx-#%u) type(%d) multi(%u)\n",
				sc->parent->c->kid, sc->idx,
				sc->type, di->req.num[sc->type]);
			ret = d->exec(d, sc);
		}
		if (!d || ret)
			mdw_drv_err("sc(0x%llx-#%d) fail(%p/%d)\n",
				sc->parent->c->kid, sc->idx, d, ret);
	}
	mutex_unlock(&di->mtx);

	mdw_dispr_item_put(di);

	return ret;
}

//------------------------
void mdw_dispr_check(void)
{
	struct mdw_disp_item *di = NULL;
	struct list_head *tmp = NULL, *list_ptr = NULL;

	mutex_lock(&d_mgr.mtx);
	list_for_each_safe(list_ptr, tmp, &d_mgr.ready_list) {
		di = list_entry(list_ptr, struct mdw_disp_item, p_item);

		list_del(&di->p_item);
		mutex_unlock(&d_mgr.mtx);
		di->exec(di);
		mutex_lock(&d_mgr.mtx);
	}
	mutex_unlock(&d_mgr.mtx);

	mdw_flw_debug("\n");
}

int mdw_dispr_norm(struct mdw_ap_sc *sc)
{
	struct mdw_rsc_req r;
	struct mdw_dev_info *d = NULL;
	struct list_head *tmp = NULL, *list_ptr = NULL;
	int ret = 0;

	/* allocate device */
	memset(&r, 0, sizeof(r));
	r.num[sc->type] = 1;
	r.total_num = 1;
	r.acq_bmp |= (1ULL << sc->type);
	r.mode = MDW_DEV_INFO_GET_MODE_TRY;
	if (mdw_ap_parser.is_deadline(sc))
		r.policy = MDW_DEV_INFO_GET_POLICY_RR;
	else
		r.policy = MDW_DEV_INFO_GET_POLICY_SEQ;

	ret = mdw_rsc_get_dev(&r);
	if (ret)
		goto out;

	/* power on each device if multicore */
	if (r.get_num[sc->type] > 1)
		mdw_dispr_pwron_dev(&r, sc);

	mdw_flw_debug("sc(0x%llx-#%d) #dev(%u/%u)\n",
		sc->parent->c->kid, sc->idx, r.get_num[sc->type],
		r.num[sc->type]);

	/* dispatch cmd */
	list_for_each_safe(list_ptr, tmp, &r.d_list) {
		d = list_entry(list_ptr, struct mdw_dev_info, r_item);
		ret = d->exec(d, sc);
		if (ret)
			goto fail_exec_sc;
		list_del(&d->r_item);
	}

	goto out;

fail_exec_sc:
	list_for_each_safe(list_ptr, tmp, &r.d_list) {
		d = list_entry(list_ptr, struct mdw_dev_info, r_item);
		list_del(&d->r_item);
		mdw_rsc_put_dev(d);
	}
out:
	return ret;
}

int mdw_dispr_pack(struct mdw_ap_sc *sc)
{
	struct mdw_disp_item *di = NULL;
	int ret = 0;

	di = mdw_dispr_item_get(sc, MDW_DISPR_PACK);
	if (!di)
		return -ENODATA;

	mutex_lock(&di->mtx);
	list_add_tail(&sc->di_item, &di->sc_list);
	di->cur_sc_num++;
	di->req.num[sc->type]++;
	di->req.acq_bmp |= (1ULL << sc->type);
	di->req.total_num++;
	di->exec = mdw_dispr_exec_pack;
	mdw_flw_debug("sc(0x%llx-#%d) pack(%d)(%d/%d)\n",
		sc->parent->c->kid,
		sc->idx, sc->hdr->info->pack_id,
		di->cur_sc_num, di->target_sc_num);

	if (di->cur_sc_num >= di->target_sc_num)
		ret = mdw_dispr_get_dev(di);

	mutex_unlock(&di->mtx);

	return ret;
}

int mdw_dispr_init(void)
{
	memset(&d_mgr, 0, sizeof(d_mgr));
	mutex_init(&d_mgr.mtx);
	INIT_LIST_HEAD(&d_mgr.ready_list);

	return 0;
}

void mdw_dispr_deinit(void)
{
}
