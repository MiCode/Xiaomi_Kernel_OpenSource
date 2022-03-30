// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/slab.h>

#include "apusys_device.h"
#include "mdw_cmn.h"
#include "mdw_ap.h"
#include "mdw_trace.h"

#define MDW_CMD_EMPTY_NUM 0xff

static void mdw_ap_cmd_print(struct mdw_ap_cmd *ac)
{
	mdw_cmd_debug("-------------------------\n");
	mdw_cmd_debug("ac(0x%llx)\n", ac->c->kid);
	mdw_cmd_debug(" uid = 0x%llx\n", ac->c->uid);
	mdw_cmd_debug(" priority = %u\n", ac->c->priority);
	mdw_cmd_debug(" hardlimit = %u\n", ac->c->hardlimit);
	mdw_cmd_debug(" softlimit = %u\n", ac->c->softlimit);
	mdw_cmd_debug(" power_save = %u\n", ac->c->power_save);
	mdw_cmd_debug(" power_plcy = %u\n", ac->c->power_plcy);
	mdw_cmd_debug(" power_dtime = %u\n", ac->c->power_dtime);
	mdw_cmd_debug(" app_type = %u\n", ac->c->app_type);
	mdw_cmd_debug(" sc_bitmask = 0x%llx\n", ac->sc_bitmask);
	mdw_cmd_debug(" adj matrix:\n");
	mdw_cmd_debug("-------------------------\n");
}

static void mdw_ap_sc_print(struct mdw_ap_sc *sc)
{
	mdw_cmd_debug("-------------------------\n");
	mdw_cmd_debug("sc(0x%llx-#%d)\n", sc->parent->c->kid, sc->idx);
	mdw_cmd_debug(" type = %u\n", sc->hdr->info->type);
	mdw_cmd_debug(" suggest_time = %u\n", sc->hdr->info->suggest_time);
	mdw_cmd_debug(" vlm_usage = %u\n", sc->hdr->info->vlm_usage);
	mdw_cmd_debug(" vlm_ctx_id = %u\n", sc->hdr->info->vlm_ctx_id);
	mdw_cmd_debug(" vlm_force = %u\n", sc->hdr->info->vlm_force);
	mdw_cmd_debug(" boost = %u\n", sc->hdr->info->boost);
	mdw_cmd_debug(" turbo_boost = %u\n", sc->hdr->info->turbo_boost);
	mdw_cmd_debug(" min_boost = %u\n", sc->hdr->info->min_boost);
	mdw_cmd_debug(" max_boost = %u\n", sc->hdr->info->max_boost);
	mdw_cmd_debug(" hse_en = %u\n", sc->hdr->info->hse_en);
	mdw_cmd_debug(" pack_id = %u\n", sc->hdr->info->pack_id);
	mdw_cmd_debug(" num_cmdbufs = %u\n", sc->hdr->info->num_cmdbufs);
	mdw_cmd_debug(" driver_time = %u\n", sc->hdr->info->driver_time);
	mdw_cmd_debug(" ip time = %u\n", sc->hdr->info->ip_time);
	mdw_cmd_debug(" runtime = %u\n", sc->runtime);
	mdw_cmd_debug(" deadline = %u\n", sc->deadline);
	mdw_cmd_debug(" period = %u\n", sc->period);
	mdw_cmd_debug("-------------------------\n");
}

static int mdw_ap_sc_ready(struct mdw_ap_sc *sc)
{
	struct mdw_ap_cmd *ac = sc->parent;
	struct mdw_cmd *c = ac->c;
	unsigned int idx = sc->idx;

	while (idx < c->num_subcmds * c->num_subcmds) {
		if (ac->adj_matrix[idx])
			return -EBUSY;
		idx += c->num_subcmds;
	}

	return 0;
}

static void mdw_ap_sc_set_einfo(struct mdw_ap_sc *sc)
{
	struct mdw_subcmd_exec_info *einfo = sc->einfo;
	struct mdw_cmd *c = sc->parent->c;

	einfo->driver_time = sc->driver_time;
	einfo->ip_time = sc->ip_time;
	einfo->bw = sc->bw;
	einfo->boost = sc->boost;
	einfo->tcm_usage = sc->tcm_real_size;
	einfo->ret = sc->ret;
	if (einfo->ret)
		c->einfos->c.sc_rets |= (1ULL << sc->idx);

	mdw_cmd_debug("sc(0x%llx-%u) (%u/%u/%u/%u/%u/%d)\n",
		c->kid, sc->idx,
		einfo->driver_time, einfo->ip_time,
		einfo->bw,  einfo->boost,
		einfo->tcm_usage, einfo->ret);
}

static struct mdw_ap_sc *mdw_ap_cmd_get_avilable_sc(struct mdw_ap_cmd *ac)
{
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct mdw_ap_sc *sc = NULL;

	list_for_each_safe(list_ptr, tmp, &ac->sc_list) {
		sc = list_entry(list_ptr, struct mdw_ap_sc, c_item);
		if (!mdw_ap_sc_ready(sc)) {
			list_del(&sc->c_item);
			break;
		}
		sc = NULL;
	}

	return sc;
}

static struct mdw_ap_cmd *mdw_ap_cmd_create(struct mdw_cmd *c)
{
	struct mdw_ap_cmd *ac = NULL;
	struct mdw_ap_sc *sc = NULL;
	struct mdw_subcmd_exec_info *sc_info = NULL;
	unsigned int i = 0;

	mdw_trace_begin("ap cmd create|c(0x%llx) num_subcmds(%u)",
		c->kid, c->num_subcmds);

	/* alloc ap cmd */
	ac = kzalloc(sizeof(*ac), GFP_KERNEL);
	if (!ac)
		goto out;

	/* init variable */
	mutex_init(&ac->mtx);
	INIT_LIST_HEAD(&ac->sc_list);
	INIT_LIST_HEAD(&ac->di_list);
	ac->c = c;

	if (!c->einfos) {
		mdw_drv_err("invalidc info ptr\n");
		goto free_ac;
	}
	sc_info = &c->einfos->sc;

	/* setup ctx repo */
	memset(ac->ctx_repo, MDW_CMD_EMPTY_NUM, sizeof(ac->ctx_repo));

	/* alloc adjacency matrix */
	ac->adj_matrix = kzalloc(c->num_subcmds *
		c->num_subcmds * sizeof(uint8_t), GFP_KERNEL);
	if (!ac->adj_matrix)
		goto free_ac;

	memcpy(ac->adj_matrix, c->adj_matrix,
		c->num_subcmds * c->num_subcmds * sizeof(uint8_t));

	ac->pid = current->pid;
	ac->tgid = current->tgid;
	ac->sc_bitmask = (1ULL << c->num_subcmds) - 1;
	refcount_set(&ac->ref.refcount, c->num_subcmds);
	mdw_ap_cmd_print(ac);

	/* alloc ap subcmds */
	ac->sc_arr = kcalloc(c->num_subcmds, sizeof(*ac->sc_arr), GFP_KERNEL);
	if (!ac->sc_arr)
		goto free_adj;

	/* setup ap subcmds */
	for (i = 0; i < c->num_subcmds; i++) {
		sc = &ac->sc_arr[i];
		sc->parent = ac;
		sc->idx = i;
		sc->hdr = &c->ksubcmds[i];
		sc->einfo = &sc_info[i];
		sc->boost = sc->hdr->info->boost;
		sc->type = sc->hdr->info->type;
		if (sc->hdr->info->vlm_ctx_id)
			ac->ctx_cnt[sc->hdr->info->vlm_ctx_id]++;
		if (sc->hdr->info->pack_id)
			ac->pack_cnt[sc->hdr->info->pack_id]++;

		if (mdw_rsc_get_dev_num(sc->type + APUSYS_DEVICE_RT) &&
			c->softlimit) {
			sc->type += APUSYS_DEVICE_RT;
			sc->period = (uint64_t)c->softlimit * 1000;
			sc->deadline = jiffies + usecs_to_jiffies(sc->period);
		}

		mdw_ap_sc_print(sc);
		mdw_queue_task_start(sc);
		list_add_tail(&sc->c_item, &ac->sc_list);
	}
	mdw_drv_debug("cmd(%p/0x%llx) create done\n",
		c->mpriv, ac->c->kid);

	goto out;

free_adj:
	kfree(ac->adj_matrix);
free_ac:
	kfree(ac);
	ac = NULL;
out:
	mdw_trace_end("ap cmd create|c(0x%llx) num_subcmds(%u)",
		c->kid, c->num_subcmds);
	return ac;
}

static void mdw_ap_cmd_delete(struct mdw_ap_cmd *ac)
{
	struct mdw_cmd *c = ac->c;

	mdw_drv_debug("cmd(%p/0x%llx) delete\n",
		c->mpriv, c->kid);

	mutex_lock(&c->mtx);
	/* free ap subcmds */
	kfree(ac->sc_arr);
	/* free adj matrix */
	kfree(ac->adj_matrix);
	/* free ap cmd */
	kfree(ac);
	mutex_unlock(&c->mtx);
}

int mdw_ap_cmd_exec(struct mdw_cmd *c)
{
	struct mdw_ap_cmd *ac = NULL;
	struct mdw_ap_sc *sc = NULL;
	int ret = 0;

	mutex_lock(&c->mtx);
	ac = mdw_ap_cmd_create(c);
	if (!ac) {
		ret = -ENOMEM;
		goto out;
	}

	mdw_flw_debug("cmd(0x%llx) execute...\n", ac->c->kid);
	mutex_lock(&ac->mtx);
	while (1) {
		sc = mdw_ap_cmd_get_avilable_sc(ac);
		if (sc)
			mdw_sched(sc);
		else
			break;
	}
	mutex_unlock(&ac->mtx);
	mdw_flw_debug("cmd(0x%llx) parse done\n", ac->c->kid);

out:
	mutex_unlock(&c->mtx);
	return ret;
}

//-------------------------------------------
static void mdw_ap_cmd_done(struct kref *ref)
{
	struct mdw_ap_cmd *ac =
			container_of(ref, struct mdw_ap_cmd, ref);
	struct mdw_cmd *c = ac->c;
	int ret = 0;

	if (ac->sc_bitmask || ac->state) {
		mdw_drv_warn("cmd(0x%llx) abort\n", ac->c->kid);
		mdw_ap_cmd_delete(ac);
	} else {
		mdw_ap_cmd_delete(ac);

		if (c->einfos->c.sc_rets)
			ret = -EFAULT;
		c->complete(c, ret);
	}
}

static int mdw_ap_cmd_end_sc(struct mdw_ap_sc *in, struct mdw_ap_sc **out)
{
	struct mdw_ap_cmd *ac = in->parent;
	struct mdw_cmd *c = ac->c;
	unsigned int idx = 0;
	bool need_clear = false;

	mutex_lock(&ac->mtx);
	if (ac->sc_bitmask & (1ULL << in->idx))
		need_clear = true;

	/* clear dependency */
	mdw_cmd_debug(" sc_bitmask (%u/0x%llx)\n", in->idx, ac->sc_bitmask);
	if (need_clear == true) {
		ac->sc_bitmask &= ~(1ULL << in->idx);
		for (idx = in->idx * c->num_subcmds; idx <
			(in->idx+1) * c->num_subcmds; idx++)
			ac->adj_matrix[idx] = 0;
		mdw_flw_debug("sc(0x%llx-#%d) done, clear dependency\n",
			ac->c->kid, in->idx);
		mdw_ap_sc_set_einfo(in);
		mdw_queue_task_end(in);
	}

	*out = mdw_ap_cmd_get_avilable_sc(ac);
	mutex_unlock(&ac->mtx);

	if (need_clear)
		kref_put(&ac->ref, mdw_ap_cmd_done);

	return 0;
}

static int mdw_ap_cmd_get_ctx(struct mdw_ap_sc *sc)
{
	struct mdw_ap_cmd *ac = sc->parent;
	uint32_t ctx_idx = sc->hdr->info->vlm_ctx_id;
	int ret = 0;

	mutex_lock(&ac->mtx);
	if (!ctx_idx)
		goto get_vlm;

	/* ctx exist in repo */
	if (ac->ctx_repo[ctx_idx] != MDW_CMD_EMPTY_NUM) {
		sc->vlm_ctx = ac->ctx_repo[ctx_idx];
		mdw_flw_debug("sc(0x%llx-%u) re-use ctx(%u)\n",
			sc->parent->c->kid, sc->idx,
			sc->vlm_ctx);
		goto out;
	}

get_vlm:
	ret = mdw_rvs_get_vlm(sc->hdr->info->vlm_usage,
		sc->hdr->info->vlm_force,
		&sc->vlm_ctx,
		&sc->tcm_real_size);

	if (!ret && ac->ctx_repo[ctx_idx] == MDW_CMD_EMPTY_NUM)
		ac->ctx_repo[ctx_idx] = sc->vlm_ctx;

	mdw_flw_debug("sc(0x%llx-%u) get ctx(%u)(%u/%u)\n",
		sc->parent->c->kid, sc->idx,
		sc->vlm_ctx, sc->hdr->info->vlm_usage,
		sc->tcm_real_size);

out:
	mutex_unlock(&ac->mtx);
	return ret;
}

static void mdw_ap_cmd_put_ctx(struct mdw_ap_sc *sc)
{
	struct mdw_ap_cmd *ac = sc->parent;
	uint32_t ctx_idx = sc->hdr->info->vlm_ctx_id;

	mutex_lock(&ac->mtx);
	if (!ctx_idx)
		goto free_ctx;

	ac->ctx_cnt[ctx_idx]--;
	if (ac->ctx_cnt[ctx_idx])
		goto out;
	ac->ctx_repo[ctx_idx] = MDW_CMD_EMPTY_NUM;

free_ctx:
	mdw_rvs_free_vlm(sc->vlm_ctx);
	mdw_flw_debug("sc(0x%llx-%u) put ctx(%u)(%u/%u)\n",
		sc->parent->c->kid, sc->idx,
		sc->vlm_ctx, sc->hdr->info->vlm_usage,
		sc->tcm_real_size);

out:
	mutex_unlock(&ac->mtx);
}

static int mdw_ap_cmd_set_hnd(struct mdw_ap_sc *sc, int d_idx, void *h)
{
	struct apusys_cmd_handle *hnd = (struct apusys_cmd_handle *)h;
	struct mdw_subcmd_kinfo *hdr = sc->hdr;
	unsigned int i = 0;

	hnd->num_cmdbufs = hdr->info->num_cmdbufs;
	hnd->cmdbufs = kcalloc(hnd->num_cmdbufs, sizeof(*hnd->cmdbufs),
		GFP_KERNEL);
	if (!hnd->cmdbufs)
		return -ENOMEM;

	for (i = 0; i < hdr->info->num_cmdbufs; i++) {
		hnd->cmdbufs[i].kva = (void *)hdr->kvaddrs[i];
		hnd->cmdbufs[i].size = hdr->cmdbufs[i].size;
	}

	hnd->kid = sc->parent->c->kid;
	hnd->subcmd_idx = sc->idx;
	hnd->boost = sc->boost;
	hnd->context_callback = mdw_rvs_set_ctx;
	hnd->vlm_ctx = sc->vlm_ctx;
	hnd->cluster_size = sc->cluster_size;
	hnd->multicore_total = sc->multi_total;

	return 0;
}

static void mdw_ap_cmd_clear_hnd(void *h)
{
	struct apusys_cmd_handle *hnd = (struct apusys_cmd_handle *)h;

	kfree(hnd->cmdbufs);
	memset(hnd, 0, sizeof(*hnd));
}

static bool mdw_ap_cmd_is_deadline(struct mdw_ap_sc *sc)
{
	if (sc->period)
		return true;

	return false;
}

struct mdw_parser mdw_ap_parser = {
	.end_sc = mdw_ap_cmd_end_sc,
	.set_hnd = mdw_ap_cmd_set_hnd,
	.clear_hnd = mdw_ap_cmd_clear_hnd,
	.get_ctx = mdw_ap_cmd_get_ctx,
	.put_ctx = mdw_ap_cmd_put_ctx,
	.is_deadline = mdw_ap_cmd_is_deadline,

};
