// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mdw_rv.h"
#include "mdw_cmn.h"
#include "mdw_mem_pool.h"
#include "mdw_trace.h"

#include "apu_ipi.h"

#define MDW_IS_HIGHADDR(addr) ((addr & 0xffffffff00000000) ? true : false)

static void mdw_rv_cmd_print(struct mdw_rv_msg_cmd *rc)
{
	mdw_cmd_debug("-------------------------\n");
	mdw_cmd_debug("rc kid(0x%llx)\n", rc->cmd_id);
	mdw_cmd_debug(" sesion = 0x%llx\n", rc->session_id);
	mdw_cmd_debug(" priority = %u\n", rc->priority);
	mdw_cmd_debug(" hardlimit = %u\n", rc->hardlimit);
	mdw_cmd_debug(" softlimit = %u\n", rc->softlimit);
	mdw_cmd_debug(" power_save = %u\n", rc->power_save);
	mdw_cmd_debug(" power_plcy = %u\n", rc->power_plcy);
	mdw_cmd_debug(" power_dtime = %u\n", rc->power_dtime);
	mdw_cmd_debug(" app_type = %u\n", rc->app_type);
	mdw_cmd_debug(" num_subcmds = %u\n", rc->num_subcmds);
	mdw_cmd_debug(" subcmds_offset = 0x%x\n", rc->subcmds_offset);
	mdw_cmd_debug(" num_cmdbufs = %u\n", rc->num_cmdbufs);
	mdw_cmd_debug(" cmdbuf_infos_offset = 0x%x\n", rc->cmdbuf_infos_offset);
	mdw_cmd_debug(" adj_matrix_offset = 0x%x\n", rc->adj_matrix_offset);
	mdw_cmd_debug(" exec_infos_offset = 0x%x\n", rc->exec_infos_offset);
	mdw_cmd_debug("-------------------------\n");
}

static void mdw_rv_cmd_set_affinity(struct mdw_cmd *c, bool enable)
{
	if (c->power_plcy != MDW_POWERPOLICY_PERFORMANCE)
		return;

	if (enable) {
		mdw_flw_debug("enable affinity\n");
		apu_ipi_affin_enable();
	} else {
		mdw_flw_debug("disable affinity\n");
		apu_ipi_affin_disable();
	}
}

static void mdw_rv_sc_print(struct mdw_rv_msg_sc *rsc,
	uint64_t cmd_id, uint32_t idx)
{
	mdw_cmd_debug("-------------------------\n");
	mdw_cmd_debug("rv subcmd(0x%llx-#%u)\n", cmd_id, idx);
	mdw_cmd_debug(" type = %u\n", rsc->type);
	mdw_cmd_debug(" suggest_time = %u\n", rsc->suggest_time);
	mdw_cmd_debug(" vlm_usage = %u\n", rsc->vlm_usage);
	mdw_cmd_debug(" vlm_ctx_id = %u\n", rsc->vlm_ctx_id);
	mdw_cmd_debug(" vlm_force = %u\n", rsc->vlm_force);
	mdw_cmd_debug(" boost = %u\n", rsc->boost);
	mdw_cmd_debug(" turbo_boost = %u\n", rsc->turbo_boost);
	mdw_cmd_debug(" min_boost = %u\n", rsc->min_boost);
	mdw_cmd_debug(" max_boost = %u\n", rsc->max_boost);
	mdw_cmd_debug(" hse_en = %u\n", rsc->hse_en);
	mdw_cmd_debug(" driver_time = %u\n", rsc->driver_time);
	mdw_cmd_debug(" ip_time = %u\n", rsc->ip_time);
	mdw_cmd_debug(" pack_id = %u\n", rsc->pack_id);
	mdw_cmd_debug(" cmdbuf_start_idx = %u\n", rsc->cmdbuf_start_idx);
	mdw_cmd_debug(" num_cmdbufs = %u\n", rsc->num_cmdbufs);
	mdw_cmd_debug("-------------------------\n");
}

struct mdw_rv_cmd *mdw_rv_cmd_create(struct mdw_fpriv *mpriv,
	struct mdw_cmd *c)
{
	struct mdw_rv_cmd *rc = NULL;
	uint32_t cb_size = 0, acc_cb = 0, i = 0, j = 0;
	uint32_t subcmds_ofs = 0, cmdbuf_infos_ofs = 0, adj_matrix_ofs = 0;
	uint32_t exec_infos_ofs = 0;
	struct mdw_rv_msg_cmd *rmc = NULL;
	struct mdw_rv_msg_sc *rmsc = NULL;
	struct mdw_rv_msg_cb *rmcb = NULL;

	mdw_trace_begin("%s|cmd(0x%llx/0x%llx)", __func__, c->uid, c->kid);
	mutex_lock(&mpriv->mtx);

	/* check mem address for rv */
	if (MDW_IS_HIGHADDR(c->exec_infos->device_va) ||
		MDW_IS_HIGHADDR(c->cmdbufs->device_va)) {
		mdw_drv_err("rv dva high addr(0x%llx/0x%llx)\n",
			c->cmdbufs->device_va, c->exec_infos->device_va);
		goto out;
	}

	rc = kzalloc(sizeof(*rc), GFP_KERNEL);
	if (!rc)
		goto out;

	c->rvid = (uint64_t)&rc->s_msg;
	init_completion(&rc->s_msg.cmplt);
	/* set start timestamp */
	rc->start_ts_ns = c->start_ts.tv_sec * 1000000000 + c->start_ts.tv_nsec;

	/* calc size and offset */
	rc->c = c;
	cb_size += sizeof(struct mdw_rv_msg_cmd);
	cb_size = MDW_ALIGN(cb_size, MDW_DEFAULT_ALIGN);
	adj_matrix_ofs = cb_size;
	cb_size += (c->num_subcmds * c->num_subcmds * sizeof(uint8_t));
	cb_size = MDW_ALIGN(cb_size, MDW_DEFAULT_ALIGN);
	subcmds_ofs = cb_size;
	cb_size += (c->num_subcmds * sizeof(struct mdw_rv_msg_sc));
	cb_size = MDW_ALIGN(cb_size, MDW_DEFAULT_ALIGN);
	cmdbuf_infos_ofs = cb_size;
	cb_size += (c->num_cmdbufs * sizeof(struct mdw_rv_msg_cb));
	exec_infos_ofs = cb_size;
	cb_size += c->exec_infos->size;

	/* allocate communicate buffer */
	rc->cb = mdw_mem_pool_alloc(&mpriv->cmd_buf_pool, cb_size,
		MDW_DEFAULT_ALIGN);
	if (!rc->cb) {
		mdw_drv_err("c(0x%llx) alloc cb size(%u) fail\n",
			c->kid, cb_size);
		goto free_rc;
	}

	/* assign cmd info */
	rmc = (struct mdw_rv_msg_cmd *)rc->cb->vaddr;
	rmc->session_id = (uint64_t)c->mpriv;
	rmc->cmd_id = c->kid;
	rmc->pid = (uint32_t)c->pid;
	rmc->tgid = (uint32_t)c->tgid;
	rmc->priority = c->priority;
	rmc->hardlimit = c->hardlimit;
	rmc->softlimit = c->softlimit;
	rmc->power_save = c->power_save;
	rmc->power_plcy = c->power_plcy;
	rmc->power_dtime = c->power_dtime;
	rmc->app_type = c->app_type;
	rmc->num_subcmds = c->num_subcmds;
	rmc->num_cmdbufs = c->num_cmdbufs;
	rmc->subcmds_offset = subcmds_ofs;
	rmc->cmdbuf_infos_offset = cmdbuf_infos_ofs;
	rmc->adj_matrix_offset = adj_matrix_ofs;
	rmc->exec_infos_offset = exec_infos_ofs;
	mdw_rv_cmd_print(rmc);

	/* copy adj matrix */
	memcpy((void *)rmc + rmc->adj_matrix_offset, c->adj_matrix,
		c->num_subcmds * c->num_subcmds * sizeof(uint8_t));

	/* assign subcmds info */
	rmsc = (void *)rmc + rmc->subcmds_offset;
	rmcb = (void *)rmc + rmc->cmdbuf_infos_offset;
	for (i = 0; i < c->num_subcmds; i++) {
		rmsc[i].type = c->subcmds[i].type;
		rmsc[i].suggest_time = c->subcmds[i].suggest_time;
		rmsc[i].vlm_usage = c->subcmds[i].vlm_usage;
		rmsc[i].vlm_ctx_id = c->subcmds[i].vlm_ctx_id;
		rmsc[i].vlm_force = c->subcmds[i].vlm_force;
		rmsc[i].boost = c->subcmds[i].boost;
		rmsc[i].ip_time = c->subcmds[i].ip_time;
		rmsc[i].driver_time = c->subcmds[i].driver_time;
		rmsc[i].bw = c->subcmds[i].bw;
		rmsc[i].turbo_boost = c->subcmds[i].turbo_boost;
		rmsc[i].min_boost = c->subcmds[i].min_boost;
		rmsc[i].max_boost = c->subcmds[i].max_boost;
		rmsc[i].hse_en = c->subcmds[i].hse_en;
		rmsc[i].pack_id = c->subcmds[i].pack_id;
		rmsc[i].num_cmdbufs = c->subcmds[i].num_cmdbufs;
		rmsc[i].cmdbuf_start_idx = acc_cb;
		mdw_rv_sc_print(&rmsc[i], rmc->cmd_id, i);

		for (j = 0; j < rmsc[i].num_cmdbufs; j++) {
			rmcb[acc_cb + j].size =
				c->ksubcmds[i].cmdbufs[j].size;
			rmcb[acc_cb + j].device_va =
				c->ksubcmds[i].daddrs[j];
			mdw_cmd_debug("sc(%u) #%u-cmdbufs 0x%llx/%u\n",
				i, j,
				rmcb[acc_cb + j].device_va,
				rmcb[acc_cb + j].size);
		}
		acc_cb += c->subcmds[i].num_cmdbufs;
	}

	/* clear exec ret */
	c->einfos->c.ret = 0;
	c->einfos->c.sc_rets = 0;

	if (mdw_mem_flush(mpriv, rc->cb))
		mdw_drv_warn("s(0x%llx) c(0x%llx/0x%llx) flush rv cbs(%u) fail\n",
			(uint64_t)c->mpriv, c->kid, c->rvid, rc->cb->size);

	mdw_rv_cmd_set_affinity(c, true);

	goto out;

free_rc:
	kfree(rc);
	rc = NULL;
out:
	mutex_unlock(&mpriv->mtx);
	mdw_trace_end("%s|cmd(0x%llx/0x%llx)", __func__, c->uid, c->kid);
	return rc;
}

int mdw_rv_cmd_delete(struct mdw_rv_cmd *rc)
{
	struct mdw_cmd *c = NULL;
	struct mdw_rv_msg_cmd *rmc = NULL;
	struct mdw_fpriv *mpriv = NULL;

	if (!rc)
		return -EINVAL;
	c = rc->c;
	mpriv = c->mpriv;
	mdw_rv_cmd_set_affinity(c, false);

	mutex_lock(&mpriv->mtx);
	/* invalidate */
	if (mdw_mem_invalidate(c->mpriv, rc->cb))
		mdw_drv_warn("s(0x%llx) c(0x%llx/0x%llx/0x%llx) invalidate rcbs(%u) fail\n",
			(uint64_t)c->mpriv, c->uid, c->kid,
			c->rvid, rc->cb->size);

	/* copy exec infos */
	rmc = (struct mdw_rv_msg_cmd *)rc->cb->vaddr;
	if (rmc->exec_infos_offset + c->exec_infos->size != rc->cb->size) {
		mdw_drv_warn("c(0x%llx/0x%llx/0x%llx) execinfos size(%u/%u) not matched\n",
			c->uid, c->kid, c->rvid,
			rmc->exec_infos_offset + c->exec_infos->size,
			rc->cb->size);
	} else {
		memcpy(c->exec_infos->vaddr,
			rc->cb->vaddr + rmc->exec_infos_offset,
			c->exec_infos->size);
	}

	mdw_mem_pool_free(rc->cb);
	kfree(rc);
	mutex_unlock(&mpriv->mtx);

	return 0;
}

void mdw_rv_cmd_done(struct mdw_rv_cmd *rc, int ret)
{
	struct mdw_cmd *c = rc->c;

	/* delete rv cmd and complete */
	mdw_rv_cmd_delete(rc);
	c->complete(c, ret);
}
