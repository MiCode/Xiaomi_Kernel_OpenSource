/*
 * Copyright (C) 2020 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/slab.h>

#include "apusys_device.h"
#include "mdw_cmn.h"
#include "mdw_cmd.h"
#include "apu_cmd.h"
#include "mdw_mem.h"
#include "mdw_dbg.h"
#include "mdw_sched.h"
#include "mdw_rsc.h"
#include "mdw_trace.h"
#include "reviser_export.h"
#include "mdw_fence.h"

#define MDW_CMD_PSR_NUM_ERR 0xffffffff
#define MDW_CMD_SCR_BMP_ERR 0xffffffffffffffff
#define MDW_CMD_EMPTY_NUM 0xff

/* parse apu cmd related functions */
static void mdw_cmd_show_cmd(struct mdw_apu_cmd *c)
{
	struct apu_cmd_hdr *h = c->hdr;

	mdw_cmd_debug("-------------------------\n");
	mdw_cmd_debug(" apusys cmd(0x%llx)\n", c->kid);
	mdw_cmd_debug(" magic            = 0x%llx\n", h->magic);
	mdw_cmd_debug(" uid              = 0x%llx\n", h->uid);
	mdw_cmd_debug(" version          = %u\n", h->version);
	mdw_cmd_debug(" priority         = %u\n", h->priority);
	mdw_cmd_debug(" hard_limit       = %u\n", h->hard_limit);
	mdw_cmd_debug(" soft_limit       = %u\n", h->soft_limit);
	mdw_cmd_debug(" pid              = %u\n", h->pid);
	mdw_cmd_debug(" flags            = 0x%llx\n", h->flags);
	mdw_cmd_debug(" num_sc           = %u\n", h->num_sc);
	mdw_cmd_debug(" ofs_scr_list     = %u\n", h->ofs_scr_list);
	mdw_cmd_debug(" ofs_pdr_cnt_list = %u\n", h->ofs_pdr_cnt_list);
	mdw_cmd_debug(" scofs_list_entry = %u\n", h->scofs_list_entry);
	mdw_cmd_debug("-------------------------\n");
}

static void mdw_cmd_show_sc(struct mdw_apu_sc *sc)
{
	struct apu_sc_hdr_cmn *h = sc->hdr;

	mdw_cmd_debug("-------------------------\n");
	mdw_cmd_debug(" apusys sc(0x%llx-#%d)\n", sc->parent->kid, sc->idx);
	mdw_cmd_debug(" type         = %u\n", h->type);
	mdw_cmd_debug(" driver_time  = %u\n", h->driver_time);
	mdw_cmd_debug(" ip_time      = %u\n", h->ip_time);
	mdw_cmd_debug(" suggest_time = %u\n", h->suggest_time);
	mdw_cmd_debug(" bandwidth    = %u\n", h->bandwidth);
	mdw_cmd_debug(" tcm_usage    = %u\n", h->tcm_usage);
	mdw_cmd_debug(" tcm_force    = %d\n", h->tcm_force);
	mdw_cmd_debug(" boost_val    = %d\n", h->boost_val);
	mdw_cmd_debug(" pack_id      = %d\n", h->pack_id);
	mdw_cmd_debug(" reserved     = %d\n", h->reserved);
	mdw_cmd_debug(" mem_ctx      = %u\n", h->mem_ctx);
	mdw_cmd_debug(" cb_info_size = %u\n", h->cb_info_size);
	mdw_cmd_debug(" ofs_cb_info  = %u\n", h->ofs_cb_info);
	mdw_cmd_debug(" kva          = 0x%llx\n", sc->kva);
	mdw_cmd_debug(" size         = %u\n", sc->size);
	mdw_cmd_debug("-------------------------\n");
}

static void mdw_cmd_show_sc_perf(struct mdw_apu_sc *sc)
{
	mdw_pef_debug("-------------------------\n");
	mdw_pef_debug(" apusys sc(0x%llx-#%d)\n", sc->parent->kid, sc->idx);
	mdw_pef_debug(" parsing time    = %u\n",
		mdw_cmn_get_time_diff(&sc->parent->ts_create, &sc->ts_create));
	mdw_pef_debug(" wait dependency = %u\n",
		mdw_cmn_get_time_diff(&sc->ts_create, &sc->ts_enque));
	mdw_pef_debug(" sched time      = %u\n",
		mdw_cmn_get_time_diff(&sc->ts_enque, &sc->ts_deque));
	mdw_pef_debug(" preset time     = %u\n",
		mdw_cmn_get_time_diff(&sc->ts_deque, &sc->ts_start));
	mdw_pef_debug(" exec time       = %u\n",
		mdw_cmn_get_time_diff(&sc->ts_start, &sc->ts_end));
	mdw_pef_debug(" post time       = %u\n",
		mdw_cmn_get_time_diff(&sc->ts_end, &sc->ts_delete));
	mdw_pef_debug(" life time       = %u\n",
		mdw_cmn_get_time_diff(&sc->ts_create, &sc->ts_delete));
	mdw_pef_debug("-------------------------\n");
}

static void mdw_cmd_show_cmd_perf(struct mdw_apu_cmd *c)
{
	mdw_pef_debug("-------------------------\n");
	mdw_pef_debug(" apusys cmd(0x%llx)\n", c->kid);
	mdw_pef_debug(" life time       = %u\n",
		mdw_cmn_get_time_diff(&c->ts_create, &c->ts_delete));
	mdw_pef_debug("-------------------------\n");
}

static void mdw_cmd_show_hnd(struct apusys_cmd_hnd *h)
{
	mdw_cmd_debug("-------------------------\n");
	mdw_cmd_debug(" kva             = 0x%llx\n", h->kva);
	mdw_cmd_debug(" iova            = 0x%x\n", h->iova);
	mdw_cmd_debug(" size            = %u\n", h->size);
	mdw_cmd_debug(" cmd_id          = 0x%llx\n", h->cmd_id);
	mdw_cmd_debug(" subcmd_idx      = %u\n", h->subcmd_idx);
	mdw_cmd_debug(" priority        = %u\n", h->priority);
	mdw_cmd_debug(" ip_time         = %u\n", h->ip_time);
	mdw_cmd_debug(" boost_val       = %d\n", h->boost_val);
	mdw_cmd_debug(" cluster_size    = %d\n", h->cluster_size);
	mdw_cmd_debug(" multicore_total = %u\n", h->multicore_total);
	mdw_cmd_debug(" multicore_idx   = %u\n", h->multicore_idx);
	mdw_cmd_debug(" pmu_kva         = 0x%llx\n", h->pmu_kva);
	mdw_cmd_debug(" cmd_entry       = 0x%llx\n", h->cmd_entry);
	mdw_cmd_debug(" ctx_id          = %d\n", h->ctx_id);
	mdw_cmd_debug("-------------------------\n");
}

static uint32_t mdw_cmd_get_pdr_num(struct mdw_apu_sc *sc)
{
	struct mdw_apu_cmd *cmd = sc->parent;
	uint32_t pdr_cnt = 0;
	uint32_t *pdr_cnt_list;

	pdr_cnt_list = (uint32_t *)(
		(uint64_t)cmd->u_hdr + cmd->hdr->ofs_pdr_cnt_list);
	pdr_cnt = pdr_cnt_list[sc->idx];
	mdw_cmd_debug("0x%llx/0x%llx/0x%x/%d -> %d\n", (uint64_t)pdr_cnt_list,
		(uint64_t)cmd->u_hdr, cmd->hdr->ofs_pdr_cnt_list,
		sc->idx, pdr_cnt_list[sc->idx]);

	if (pdr_cnt > cmd->hdr->num_sc)
		return MDW_CMD_PSR_NUM_ERR;

	return pdr_cnt;
}

static struct apu_sc_hdr_cmn *mdw_cmd_get_sc_hdr(struct mdw_apu_cmd *cmd,
	int idx)
{
	uint32_t ofs = 0;
	struct apu_cmd_hdr *cmd_hdr = cmd->u_hdr;
	struct apu_sc_hdr_cmn *sh = NULL;

	if ((uint32_t)idx >= cmd_hdr->num_sc)
		return NULL;

	ofs = *(uint32_t *)((uint64_t)&cmd_hdr->scofs_list_entry +
		SIZE_SUBGRAPH_SCOFS_ELEMENT * idx);
	if (ofs + sizeof(struct apu_sc_hdr_cmn) > cmd->size)
		goto fail_size;

	sh = (struct apu_sc_hdr_cmn *)((uint64_t)cmd_hdr + ofs);
	if (sh->type == APUSYS_DEVICE_MDLA &&
		ofs + sizeof(struct apu_sc_hdr_cmn) +
		sizeof(struct apu_mdla_hdr) > cmd->size)
		goto fail_size;

	return (struct apu_sc_hdr_cmn *)((uint64_t)cmd_hdr + ofs);

fail_size:
	mdw_drv_err("sc(0x%llx-#%d) ofs(%u) over size(%d)\n",
		cmd_hdr->uid, idx, ofs, cmd->size);
	return NULL;
}

static int mdw_cmd_parse_flags(struct mdw_apu_cmd *c)
{
	c->multi = (uint8_t)((c->hdr->flags & HDR_FLAG_MASK_MULTI) >> 62);
	if (c->multi > HDR_FLAG_MULTI_MULTI)
		return -EINVAL;

	/* Create Fence FD */
	if (c->hdr->flags & HDR_FLAG_MASK_FENCE_EXEC) {
		c->uf_hdr = (struct apu_fence_hdr *)(
			(uint64_t)c->u_hdr + sizeof(struct apu_cmd_hdr) +
			(c->hdr->num_sc - 1) * sizeof(uint32_t));
		c->file = NULL;
		if (apu_sync_file_create(c) < 0)
			return -EINVAL;
	}
	return 0;
}

static uint64_t mdw_cmd_get_scr(struct mdw_apu_sc *sc)
{
	struct mdw_apu_cmd *cmd = sc->parent;
	uint32_t *scr_list;
	int i = 0, idx = 0, j = 0, num_scr = 0;
	uint64_t scr_bmp = 0;

	scr_list = (uint32_t *)((uint64_t)cmd->u_hdr + cmd->hdr->ofs_scr_list);

	for (i = 0; i < (int)cmd->hdr->num_sc; i++) {
		if (i != sc->idx) {
			idx = idx + scr_list[idx] + 1;
			continue;
		}

		num_scr = (int)scr_list[idx];
		idx++;
		/*
		 * idx start with 1, because 0=num
		 * refer to apusys cmd definition
		 */
		for (j = 0; j < num_scr; j++) {
			mdw_cmd_debug("sc(0x%llx-#%d) add scr(%d/%p), num_scr = %d\n",
				cmd->hdr->uid, sc->idx, scr_list[idx],
				&scr_list[idx], num_scr);
			if (scr_list[idx] >= cmd->hdr->num_sc) {
				mdw_drv_err("sc(0x%llx-#%d) scr idx(%d/%d) invalid\n",
					cmd->hdr->uid, sc->idx,
					scr_list[idx], cmd->hdr->num_sc);
				return MDW_CMD_SCR_BMP_ERR;
			}
			scr_bmp |= (1ULL << scr_list[idx]);
			idx++;
		}
		mdw_cmd_debug("sc(#%d) scr_bmp = 0x%llx\n", sc->idx, scr_bmp);
		break;
	}

	return scr_bmp;
}

static int mdw_cmd_hdr_get_status(struct mdw_apu_cmd *c)
{
	return ((int)(c->u_hdr->flags & HDR_FlAG_MASK_STATUS_BMP)
		>> HDR_FLAG_MASK_STATUS_OFS);
}

static void mdw_cmd_hdr_set_status(struct mdw_apu_cmd *c, int status)
{
	c->u_hdr->flags = (c->u_hdr->flags & ~(HDR_FlAG_MASK_STATUS_BMP))
		| status << HDR_FLAG_MASK_STATUS_OFS;
}

static void mdw_cmd_hdr_update_time(struct mdw_apu_cmd *c)
{
	uint64_t us = 0;

	if (c->uf_hdr) {
		us = (c->end_ts.tv_sec - c->start_ts.tv_sec) * 1000000;
		us += ((c->end_ts.tv_nsec - c->start_ts.tv_nsec) / 1000);
		c->uf_hdr->total_time = us;
	}
}

static void mdw_cmd_set_sc_hdr(struct mdw_apu_sc *sc)
{
	/* execution time */
	sc->u_hdr->driver_time = sc->driver_time;
	/* ip time */
	sc->u_hdr->ip_time = sc->ip_time;
	/* bandwidth */
	sc->u_hdr->bandwidth = sc->bw;
	/* boost val */
	sc->u_hdr->boost_val = sc->boost;
	/* cmd status */
	if (sc->status)
		mdw_cmd_hdr_set_status(sc->parent,
		HDR_FLAG_EXEC_STATUS_HWERROR);
}

static void *mdw_cmd_get_dev_hdr(struct mdw_apu_sc *sc)
{
	return (void *)((uint64_t)sc->u_hdr + sizeof(struct apu_sc_hdr_cmn));
}

static inline int mdw_cmd_valid(struct mdw_apu_cmd *c)
{
	struct apu_cmd_hdr *h = c->hdr;

	if (h->magic != APUSYS_MAGIC_NUMBER ||
		h->version != APUSYS_CMD_VERSION ||
		h->num_sc == 0 ||
		h->num_sc > MDW_CMD_SC_MAX ||
		h->ofs_scr_list > c->size ||
		h->ofs_pdr_cnt_list > c->size ||
		h->priority >= MDW_CMD_PRIO_MAX)
		return -EINVAL;

	return 0;
}

static inline int mdw_cmd_sc_valid(struct mdw_apu_sc *sc)
{
	struct mdw_apu_cmd *c = sc->parent;

	/* check type max */
	if (sc->hdr->type >= APUSYS_DEVICE_RT) {
		mdw_drv_err("sc(0x%llx-#%d) invalid type(%u)\n",
			c->kid, sc->idx, sc->hdr->type);
		return -ENODEV;
	}
	/* check memory context range */
	if (sc->hdr->mem_ctx >= MDW_CMD_SC_MAX) {
		mdw_drv_err("sc(0x%llx-#%d) invalid ctx(%u)\n",
			c->kid, sc->idx, sc->hdr->mem_ctx);
		return -EINVAL;
	}
	/* check pack id */
	if (sc->hdr->pack_id >= MDW_CMD_SC_MAX) {
		mdw_drv_err("sc(0x%llx-#%d) invalid pack(%u)\n",
			c->kid, sc->idx, sc->hdr->pack_id);
		return -EINVAL;
	}
	/* check successor bitmap */
	if (sc->scr_bmp == MDW_CMD_SCR_BMP_ERR) {
		mdw_drv_err("sc(0x%llx-#%d) invalid scr bmp\n",
			c->kid, sc->idx);
		return -EINVAL;
	}
	/* check presuccessor number */
	if (sc->pdr_num == MDW_CMD_PSR_NUM_ERR) {
		mdw_drv_err("sc(0x%llx-#%d) invalid pdr num\n",
			c->kid, sc->idx);
		return -EINVAL;
	}
	/* limit boost value */
	sc->boost = sc->hdr->boost_val < 100 ? sc->hdr->boost_val : 100;

	return 0;
}

static int mdw_cmd_check_sc_ready(struct mdw_apu_sc *sc)
{
	if (sc->idx < 0 || sc->idx >= MDW_CMD_SC_MAX)
		return -EINVAL;

	mdw_cmd_debug("sc(0x%llx-#%d) pdr_num = %u/%u\n", sc->parent->kid,
		sc->idx, sc->pdr_num, sc->parent->pdr_cnt[sc->idx]);
	return sc->pdr_num - sc->parent->pdr_cnt[sc->idx] == 0 ? 0 : -EBADR;
}

static int mdw_cmd_get_pack_ctx(struct mdw_apu_cmd *c)
{
	int i = 0;
	struct apu_sc_hdr_cmn *h = NULL;

	memset(c->ctx_repo, MDW_CMD_EMPTY_NUM, sizeof(c->ctx_repo));

	for (i = 0; i < c->hdr->num_sc; i++) {
		h = mdw_cmd_get_sc_hdr(c, i);
		if (!h)
			continue;

		if (h->mem_ctx >= MDW_CMD_SC_MAX ||
			h->pack_id >= MDW_CMD_SC_MAX) {
			mdw_drv_err("sc(0x%llx-#%d) invalid pack(%u) ctx(%u)\n",
				c->kid, i, h->pack_id, h->mem_ctx);
			return -EINVAL;
		}
		c->ctx_cnt[h->mem_ctx]++;
		c->pack_cnt[h->pack_id]++;
	}

	return 0;
}

static bool mdw_cmd_is_deadline(struct mdw_apu_sc *sc)
{
	if (sc->period)
		return true;
	return false;
}

static struct mdw_apu_cmd *mdw_cmd_create_cmd(int fd,
	uint32_t size, uint32_t ofs, struct mdw_usr *u)
{
	struct mdw_apu_cmd *c;

	if (ofs + sizeof(struct apu_cmd_hdr) > size) {
		mdw_drv_err("hdr ofs overflow(%u/%lu/%u)\n",
			ofs, sizeof(struct apu_cmd_hdr), size);
		goto out;
	}

	/* create cmd */
	c = vzalloc(sizeof(struct mdw_apu_cmd));
	if (!c)
		goto fail_alloc_cmd;

	/* mapping */
	c->cmdbuf = vzalloc(sizeof(struct apusys_kmem));
	if (!c->cmdbuf)
		goto fail_alloc_km;

	c->cmdbuf->fd = fd;
	c->cmdbuf->size = size;
	if (mdw_mem_map_kva(c->cmdbuf))
		goto fail_map_kva;

	/* setup hdr */
	c->hdr = vzalloc(sizeof(struct apu_cmd_hdr));
	if (!c->hdr)
		goto fail_alloc_hdr;

	c->u_hdr = (struct apu_cmd_hdr *)(c->cmdbuf->kva + ofs);
	memcpy(c->hdr, c->u_hdr, sizeof(struct apu_cmd_hdr));
	c->size = size;
	c->kid = (uint64_t)c;
	refcount_set(&c->ref.refcount, c->hdr->num_sc);
	c->usr = u;

	/* init cmd completion */
	init_completion(&c->cmplt);

	/* check basic information */
	if (mdw_cmd_valid(c))
		goto fail_cmd_invalid;

	if (mdw_cmd_parse_flags(c))
		goto fail_parse_flags;

	if (mdw_cmd_get_pack_ctx(c))
		goto fail_get_pack_ctx;

	/* init sc list */
	INIT_LIST_HEAD(&c->sc_list);
	INIT_LIST_HEAD(&c->di_list);

	/* init mutex*/
	mutex_init(&c->mtx);
	getnstimeofday(&c->ts_create);

	ktime_get_ts64(&c->start_ts);

	/* init sc state bmp */
	c->sc_status_bmp = (1ULL << c->hdr->num_sc) - 1;
	mdw_drv_debug("cmd(0x%llx/0x%llx) create\n", c->hdr->uid, c->kid);
	mdw_cmd_debug("cmd sc status bitmap = 0x%llx\n", c->sc_status_bmp);
	mdw_cmd_show_cmd(c);

	return c;

fail_get_pack_ctx:
fail_parse_flags:
fail_cmd_invalid:
	vfree(c->hdr);
fail_alloc_hdr:
	mdw_mem_unmap_kva(c->cmdbuf);
fail_map_kva:
	vfree(c->cmdbuf);
fail_alloc_km:
	vfree(c);
fail_alloc_cmd:
out:
	return NULL;
}

static int mdw_cmd_delete_cmd(struct mdw_apu_cmd *c)
{
	if (kref_read(&c->ref) != 0) {
		mdw_drv_err("cmd(0x%llx/0x%llx) can't destroy\n",
			c->hdr->uid, c->kid);
		return -EBUSY;
	}
	mdw_drv_debug("cmd(0x%llx/0x%llx) destroy\n", c->hdr->uid, c->kid);

	getnstimeofday(&c->ts_delete);

	ktime_get_ts64(&c->end_ts);
	mdw_cmd_hdr_update_time(c);
	mdw_cmd_show_cmd_perf(c);

	mdw_mem_unmap_kva(c->cmdbuf);
	vfree(c->cmdbuf);
	vfree(c->hdr);
	vfree(c);

	return 0;
}

static int mdw_cmd_get_codebuf_info(struct mdw_apu_sc *sc)
{
	int fd = 0, ret = 0;
	struct mdw_apu_cmd *c = sc->parent;

	if (sc->hdr->ofs_cb_info & FLAG_SUBGRAPH_FD_MAP) {
		fd = sc->hdr->ofs_cb_info & ~FLAG_SUBGRAPH_FD_MAP;
		mdw_cmd_debug("sc(0x%llx-#%d) map from fd(%d)",
			c->kid, sc->idx, fd);
		sc->buf.fd = fd;
		sc->buf.size = sc->hdr->cb_info_size;

		ret = mdw_mem_map_kva(&sc->buf);
		if (ret)
			goto out;

		sc->kva = sc->buf.kva;
	} else {
		sc->kva = (uint64_t)c->u_hdr + sc->hdr->ofs_cb_info;
		mdw_cmd_debug("sc(0x%llx-#%d) form ofs(0x%x)",
			c->kid, sc->idx, sc->hdr->ofs_cb_info);

		/* check sc codebuf overflow */
		if (sc->kva + sc->size > ((uint64_t)c->u_hdr + c->size)) {
			mdw_drv_err("sc(0x%llx-#%d) codebuf overflow(0x%llx/%u/0x%llx/%u)\n",
				c->kid, sc->idx, sc->kva, sc->size,
				(uint64_t)c->u_hdr, c->size);
			return -EINVAL;
		}
	}

out:
	return ret;
}

static void mdw_cmd_release_codebuf_info(struct mdw_apu_sc *sc)
{
	if (sc->hdr->ofs_cb_info & FLAG_SUBGRAPH_FD_MAP)
		mdw_mem_unmap_kva(&sc->buf);
}

static void mdw_cmd_done(struct kref *ref)
{
	struct mdw_apu_cmd *c =
			container_of(ref, struct mdw_apu_cmd, ref);
	/* abort, delete cmd direct */
	if (c->sc_status_bmp || c->state == MDW_CMD_STATE_ABORT) {
		mdw_drv_warn("abort, delete c(0x%llx) directly\n", c->kid);
		mdw_cmd_delete_cmd(c);
	} else {
		complete(&c->cmplt);
	}
}

static void mdw_cmd_delete_sc(struct mdw_apu_sc *sc)
{
	struct mdw_queue *mq = NULL;

	mdw_drv_debug("sc(0x%llx-#%d) destroy\n",
		sc->parent->kid, sc->idx);

	mq = mdw_rsc_get_queue(sc->type);
	if (!mq) {
		mdw_drv_err("can't find mq(%d)\n", sc->type);
		return;
	}

	mdw_queue_task_end(sc);

	getnstimeofday(&sc->ts_delete);
	mdw_cmd_show_sc_perf(sc);

	mutex_lock(&sc->mtx);
	mdw_cmd_set_sc_hdr(sc);
	mdw_cmd_release_codebuf_info(sc);
	mutex_unlock(&sc->mtx);
	vfree(sc->hdr);
	vfree(sc);
}

static struct mdw_apu_sc *mdw_cmd_create_sc(struct mdw_apu_cmd *c)
{
	struct mdw_apu_sc *sc = NULL;
	struct mdw_queue *mq = NULL;

	sc = vzalloc(sizeof(struct mdw_apu_sc));
	if (!sc)
		return NULL;

	/* init sc's list item and insert to cmd's list */
	sc->u_hdr = mdw_cmd_get_sc_hdr(c, c->parsed_sc_num);
	if (!sc->u_hdr)
		goto fail_get_sc_hdr;
	sc->hdr = vzalloc(sizeof(struct apu_sc_hdr_cmn));
	if (!sc->hdr)
		goto fail_alloc_hdr;

	memcpy(sc->hdr, sc->u_hdr, sizeof(struct apu_sc_hdr_cmn));
	mutex_init(&sc->mtx);
	sc->parent = c;
	sc->type = sc->hdr->type;
	sc->size = sc->hdr->cb_info_size;
	sc->idx = c->parsed_sc_num;
	sc->pdr_num = mdw_cmd_get_pdr_num(sc);
	sc->scr_bmp = mdw_cmd_get_scr(sc);
	sc->runtime = sc->hdr->ip_time;
	kref_init(&sc->multi_ref);
	sc->d_hdr = (void *)((uint64_t)(sc->u_hdr) +
		sizeof(struct apu_sc_hdr_cmn));
	if (mdw_cmd_get_codebuf_info(sc))
		goto fail_get_codebuf_info;

	if (preemption_support &&
		mdw_rsc_get_dev_num(sc->type + APUSYS_DEVICE_RT) &&
		c->hdr->soft_limit) {
		sc->type += APUSYS_DEVICE_RT;
		sc->period = c->hdr->soft_limit * 1000;
		sc->deadline = jiffies + usecs_to_jiffies(sc->period);
	}

	mdw_cmd_debug("sc(0x%llx-#%d) ctx(%d) pack(%d)\n",
		c->kid, sc->idx, sc->hdr->mem_ctx, sc->hdr->pack_id);

	/* check sc valid */
	if (mdw_cmd_sc_valid(sc))
		goto fail_sc_invalid;

	/* task start */
	mq = mdw_rsc_get_queue(sc->type);
	if (!mq) {
		mdw_drv_err("can't find mq(%d)\n", sc->type);
		goto fail_get_mq;
	}

	mdw_queue_task_start(sc);

	getnstimeofday(&sc->ts_create);

	mdw_drv_debug("sc(0x%llx-#%d) create\n", c->kid, sc->idx);
	mdw_cmd_show_sc(sc);

	return sc;

fail_sc_invalid:
fail_get_mq:
fail_get_codebuf_info:
	vfree(sc->hdr);
fail_alloc_hdr:
fail_get_sc_hdr:
	mdw_flw_debug("\n");
	vfree(sc);
	return NULL;
}

static int mdw_cmd_abort_cmd(struct mdw_apu_cmd *c)
{
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct mdw_apu_sc *sc = NULL;
	int i = 0, cnt = 0;

	mutex_lock(&c->mtx);
	c->state = MDW_CMD_STATE_ABORT;
	if (!mdw_cmd_hdr_get_status(c))
		mdw_cmd_hdr_set_status(c, HDR_FLAG_EXEC_STATUS_ABORT);

	mdw_flw_debug("exec status in flag(%d)\n", mdw_cmd_hdr_get_status(c));

	list_for_each_safe(list_ptr, tmp, &c->sc_list) {
		sc = list_entry(list_ptr, struct mdw_apu_sc, cmd_item);
		list_del(&sc->cmd_item);
		mdw_cmd_delete_sc(sc);
		cnt++;
	}
	mutex_unlock(&c->mtx);

	for (i = 0; i < cnt; i++)
		kref_put(&c->ref, mdw_cmd_done);

	return 0;
}

static int mdw_cmd_parse_cmd(struct mdw_apu_cmd *c, struct mdw_apu_sc **out)
{
	int ret = 0;
	struct mdw_apu_sc *sc;

	*out = NULL;

	if (c->parsed_sc_num >= c->hdr->num_sc)
		return 0;

	sc = mdw_cmd_create_sc(c);
	if (!sc)
		return -EINVAL;

	mutex_lock(&c->mtx);
	if (mdw_cmd_check_sc_ready(sc))
		list_add_tail(&sc->cmd_item, &c->sc_list);
	else
		*out = sc;
	c->parsed_sc_num++;
	ret = c->hdr->num_sc - c->parsed_sc_num;
	mutex_unlock(&c->mtx);

	return ret;
}

static void mdw_cmd_update_scr(struct mdw_apu_sc *sc)
{
	struct mdw_apu_cmd *c = sc->parent;
	int idx = 0;

	while (sc->scr_bmp != 0) {
		if (!(sc->scr_bmp & (1ULL << idx)))
			goto next;
		mdw_flw_debug("sc(0x%llx-#%d) update scr(%d)\n",
			c->kid, sc->idx, idx);
		c->pdr_cnt[idx]++;
		sc->scr_bmp = sc->scr_bmp & ~(1ULL << idx);
next:
		idx++;
		if (idx >= MDW_CMD_SC_MAX)
			break;
	}
}

static int mdw_cmd_end_sc(struct mdw_apu_sc *in, struct mdw_apu_sc **out)
{
	int ret = 0;
	struct mdw_apu_cmd *c;
	struct mdw_apu_sc *sc = NULL;
	struct list_head *tmp = NULL, *list_ptr = NULL;

	*out = NULL;

	c = in->parent;
	mutex_lock(&c->mtx);
	if (c->sc_status_bmp & (1ULL << in->idx)) {
		c->sc_status_bmp &= ~(1ULL << in->idx);
		mdw_cmd_update_scr(in);
	}
	mdw_flw_debug("cmd status = 0x%llx after #%d sc done\n",
		c->sc_status_bmp, in->idx);

	/* check sc list */
	if (list_empty(&c->sc_list)) {
		mdw_flw_debug("cmd(0x%llx) empty\n", c->kid);
		goto out;
	}

	list_for_each_safe(list_ptr, tmp, &c->sc_list) {
		sc = list_entry(list_ptr, struct mdw_apu_sc, cmd_item);
		mdw_flw_debug("sc(0x%llx-#%d) bmp(0x%llx/0x%llx)\n",
			c->kid, sc->idx, sc->scr_bmp, c->sc_status_bmp);

		if (!mdw_cmd_check_sc_ready(sc)) {
			list_del(&sc->cmd_item);
			*out = sc;
			mdw_cmd_debug("sc(0x%llx-#%d) ready(%p)\n",
				c->kid, sc->idx, sc);
			break;
		}
	}

out:
	mutex_unlock(&c->mtx);
	/* if no out, delete sc */
	if (*out == NULL) {
		mdw_flw_debug("sc(0x%llx-#%d) done ref(%d)\n",
			c->kid, in->idx, kref_read(&c->ref));
		mdw_cmd_delete_sc(in);
		kref_put(&c->ref, mdw_cmd_done);
	}

	return ret;
}

int mdw_cmd_get_ctx(struct mdw_apu_sc *sc)
{
	int ret = 0;
	uint32_t tcm_usage = 0;
	struct mdw_apu_cmd *c = sc->parent;

	mdw_trace_begin("get ctx|sc(0x%llx-%d)",
		sc->parent->kid, sc->idx);

	mutex_lock(&c->mtx);
	// if tcm usage from user == 0, set by debug prop
	tcm_usage = sc->hdr->tcm_usage == 0 ?
		mdw_dbg_get_prop(MDW_DBG_PROP_TCM_DEFAULT) : sc->hdr->tcm_usage;

	/* if indicated ctx == NONE, get vlm directly */
	if (sc->hdr->mem_ctx == VALUE_SUBGRAPH_CTX_ID_NONE &&
		sc->multi_total <= 1) {
		ret = reviser_get_vlm(tcm_usage, sc->hdr->tcm_force,
			&sc->ctx, &sc->real_tcm_usage);
		mdw_flw_debug("sc(0x%llx-#%d) get ctx(%lu/%u/%u)\n",
			c->kid, sc->idx, sc->ctx, sc->hdr->mem_ctx, tcm_usage);
		goto out;
	}

	if (c->ctx_repo[sc->hdr->mem_ctx] != MDW_CMD_EMPTY_NUM) {
		sc->ctx = (uint32_t)c->ctx_repo[sc->hdr->mem_ctx];
		goto out;
	}

	ret = reviser_get_vlm(tcm_usage, sc->hdr->tcm_force,
		&sc->ctx, &sc->real_tcm_usage);
	c->ctx_repo[sc->hdr->mem_ctx] = sc->ctx;
	mdw_flw_debug("sc(0x%llx-#%d) get ctx(%lu/%u/%u)\n",
		c->kid, sc->idx, sc->ctx, sc->hdr->mem_ctx, tcm_usage);

out:
	mutex_unlock(&c->mtx);
	mdw_trace_end("get ctx|sc(0x%llx-%d) ctx(%lu)",
		sc->parent->kid, sc->idx, sc->ctx);

	return ret;
}

void mdw_cmd_put_ctx(struct mdw_apu_sc *sc)
{
	struct mdw_apu_cmd *c = sc->parent;

	mdw_trace_begin("put ctx|sc(0x%llx-%d) ctx(%lu)",
		sc->parent->kid, sc->idx, sc->ctx);

	mutex_lock(&c->mtx);
	if (sc->hdr->mem_ctx == VALUE_SUBGRAPH_CTX_ID_NONE &&
		sc->multi_total <= 1) {
		reviser_free_vlm(sc->ctx);
		mdw_flw_debug("sc(0x%llx-#%d) put ctx(%lu/%u)\n",
			c->kid, sc->idx, sc->ctx, sc->hdr->mem_ctx);
		goto out;
	}

	c->ctx_cnt[sc->hdr->mem_ctx]--;
	if (!c->ctx_cnt[sc->hdr->mem_ctx]) {
		reviser_free_vlm(sc->ctx);
		c->ctx_repo[sc->hdr->mem_ctx] = MDW_CMD_EMPTY_NUM;
		mdw_flw_debug("sc(0x%llx-#%d) put ctx(%lu/%u)\n",
			c->kid, sc->idx, sc->ctx, sc->hdr->mem_ctx);
	}
out:
	mutex_unlock(&c->mtx);
	mdw_trace_end("put ctx|sc(0x%llx-%d) ctx(%lu)",
		sc->parent->kid, sc->idx, sc->ctx);
}

static int mdw_cmd_sc_exec_num(struct mdw_apu_sc *sc)
{
	struct apu_mdla_hdr *h = NULL;
	int exec_num = 1;
	int dbg_multi = 0;

	dbg_multi = mdw_dbg_get_prop(MDW_DBG_PROP_MULTICORE);
	if (dbg_multi == HDR_FLAG_MULTI_SINGLE ||
		sc->parent->multi == HDR_FLAG_MULTI_SINGLE)
		exec_num = 1;
	else if (sc->type == APUSYS_DEVICE_MDLA ||
		sc->type == APUSYS_DEVICE_MDLA_RT) {
		h = (struct apu_mdla_hdr *)mdw_cmd_get_dev_hdr(sc);
		if (h->ofs_codebuf_info_dual0 && h->ofs_codebuf_info_dual1)
			exec_num = 2;
	}

	return exec_num;
}

static void mdw_cmd_sc_clr_hnd(struct mdw_apu_sc *sc, void *hnd)
{
	struct apusys_cmd_hnd *h = (struct apusys_cmd_hnd *)hnd;

	if (!h->kva)
		return;

	memcpy((void *)h->m_kva, (void *)h->kva, sc->size);
	kfree((void *)h->kva);
	h->kva = 0;
	h->m_kva = 0;
}

static int mdw_cmd_sc_set_hnd(struct mdw_apu_sc *sc, int d_idx, void *hnd)
{
	struct apusys_cmd_hnd *h = (struct apusys_cmd_hnd *)hnd;
	struct apu_mdla_hdr *m_hdr = NULL;
	struct mdw_apu_cmd *c = sc->parent;
	int ret = 0;

	/* contruct cmd hnd */
	mutex_lock(&sc->mtx);
	memset(h, 0, sizeof(struct apusys_cmd_hnd));
	h->size = sc->size;
	h->cmdbuf = c->cmdbuf;
	h->cmd_id = c->kid;
	h->subcmd_idx = sc->idx;
	h->priority = c->hdr->priority;
	h->ip_time = 0;
	h->boost_val = sc->boost;
	h->multicore_total = sc->multi_total;
	h->multicore_idx = 0;
	h->cmd_entry = c->cmdbuf->kva;
	h->cmd_size = c->size;
	h->ctx_id = sc->ctx;
	h->context_callback = reviser_set_context;
	h->cluster_size = sc->cluster_size;

	switch (sc->type) {
	case APUSYS_DEVICE_MDLA:
	case APUSYS_DEVICE_MDLA_RT:
		/* for mdla pmu */
		m_hdr = (struct apu_mdla_hdr *)sc->d_hdr;
		if (m_hdr->ofs_pmu_info > c->size)
			h->pmu_kva = h->cmd_entry;
		else
			h->pmu_kva = c->cmdbuf->kva + m_hdr->ofs_pmu_info;
		/* multicore */
		if (sc->multi_total <= 1) {
			h->m_kva = sc->kva;
		} else {
			m_hdr = mdw_cmd_get_dev_hdr(sc);
			if (d_idx == 0) {
				h->m_kva = c->cmdbuf->kva +
					m_hdr->ofs_codebuf_info_dual0;
			} else {
				h->m_kva = c->cmdbuf->kva +
					m_hdr->ofs_codebuf_info_dual1;
			}
			h->multicore_idx = d_idx;

			if (h->m_kva + sc->size > c->cmdbuf->kva + c->size) {
				mdw_drv_err("sc over size(0x%llx/%u)(0x%llx/%u)\n",
					h->m_kva, sc->size,
					c->cmdbuf->kva, c->size);
				ret = -EINVAL;
				goto out;
			}
			mdw_flw_debug("multi(%d/%d) kva(0x%llx), offset(%u/%u)\n",
				d_idx, sc->multi_total, h->kva,
				m_hdr->ofs_codebuf_info_dual0,
				m_hdr->ofs_codebuf_info_dual1);
		}
		break;
	default:
		h->m_kva = sc->kva;
		break;
	}

	/* duplicate cmdbuf */
	h->kva = (uint64_t)kzalloc(sc->size, GFP_KERNEL);
	if (!h->kva) {
		ret = -ENOMEM;
		goto out;
	}
	memcpy((void *)h->kva, (void *)h->m_kva, sc->size);
	mdw_cmd_debug("cmd(0x%llx-#%d) duplicate (0x%llx/%u)\n",
		c->kid, sc->idx, h->kva, sc->size);

out:
	mdw_cmd_show_hnd(h);
	mutex_unlock(&sc->mtx);
	return ret;
}

struct mdw_cmd_parser mdw_cmd_parser = {
	.create_cmd = mdw_cmd_create_cmd,
	.delete_cmd = mdw_cmd_delete_cmd,
	.abort_cmd = mdw_cmd_abort_cmd,
	.parse_cmd = mdw_cmd_parse_cmd,
	.end_sc = mdw_cmd_end_sc,
	.get_ctx = mdw_cmd_get_ctx,
	.put_ctx = mdw_cmd_put_ctx,
	.exec_core_num = mdw_cmd_sc_exec_num,
	.set_hnd = mdw_cmd_sc_set_hnd,
	.clr_hnd = mdw_cmd_sc_clr_hnd,
	.is_deadline = mdw_cmd_is_deadline,
};

struct mdw_cmd_parser *mdw_cmd_get_parser(void)
{
	return &mdw_cmd_parser;
}

uint64_t mdw_cmd_get_magic(void)
{
	return APUSYS_MAGIC_NUMBER;
}

uint32_t mdw_cmd_get_ver(void)
{
	return APUSYS_CMD_VERSION;
}
