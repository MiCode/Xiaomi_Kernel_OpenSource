// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mdw_rv.h"
#include "mdw_cmn.h"

static void mdw_rv_cmd_print(struct mdw_rv_cmd_v2 *rc)
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
	mdw_cmd_debug(" cmdbufs_offset = 0x%x\n", rc->cmdbufs_offset);
	mdw_cmd_debug(" adj_matrix_offset = 0x%x\n", rc->adj_matrix_offset);
	mdw_cmd_debug(" exec_infos = 0x%llx\n", rc->exec_infos);
	mdw_cmd_debug("-------------------------\n");
}

static void mdw_rv_sc_print(struct mdw_rv_subcmd_v2 *rsc,
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

static struct mdw_rv_cmd *mdw_rv_cmd_create(struct mdw_fpriv *mpriv,
	struct mdw_cmd *c)
{
	struct mdw_rv_cmd *rc = NULL;
	uint32_t cb_size = 0, acc_cb = 0, i = 0, j = 0;
	uint32_t subcmds_offset = 0, cmdbufs_offset = 0, adj_matrix_offset = 0;
	struct mdw_rv_cmd_v2 *rc_v2 = NULL;
	struct mdw_rv_subcmd_v2 *rsubcmds_v2 = NULL;
	struct mdw_rv_cmdbuf_v2 *rcb_v2 = NULL;

	rc = vzalloc(sizeof(*rc));
	if (!rc)
		return NULL;

	init_completion(&rc->s_msg.cmplt);

	/* calc size and offset */
	rc->c = c;
	cb_size += sizeof(struct mdw_rv_cmd_v2);
	cb_size = MDW_ALIGN(cb_size, MDW_DEFAULT_ALIGN);
	adj_matrix_offset = cb_size;
	cb_size += (c->num_subcmds * c->num_subcmds * sizeof(uint8_t));
	cb_size = MDW_ALIGN(cb_size, MDW_DEFAULT_ALIGN);
	subcmds_offset = cb_size;
	cb_size += (c->num_subcmds * sizeof(struct mdw_rv_subcmd_v2));
	cb_size = MDW_ALIGN(cb_size, MDW_DEFAULT_ALIGN);
	cmdbufs_offset = cb_size;
	cb_size += (c->num_cmdbufs * sizeof(struct mdw_rv_cmdbuf_v2));

	/* allocate communicate buffer */
	rc->cb = mdw_mem_alloc(mpriv, cb_size, MDW_DEFAULT_ALIGN,
		(1ULL << MDW_MEM_IOCTL_ALLOC_CACHEABLE), MDW_MEM_TYPE_INTERNAL);
	if (!rc->cb) {
		mdw_drv_err("c(0x%llx) alloc cb size(%u) fail\n",
			c->kid, cb_size);
		goto free_rc;
	}

	/* assign cmd info */
	rc_v2 = (struct mdw_rv_cmd_v2 *)rc->cb->vaddr;
	rc_v2->session_id = (uint64_t)c->mpriv;
	rc_v2->cmd_id = c->kid;
	rc_v2->exec_infos = c->exec_infos->device_va;
	rc_v2->exec_size = c->exec_infos->size;
	rc_v2->priority = c->priority;
	rc_v2->hardlimit = c->hardlimit;
	rc_v2->softlimit = c->softlimit;
	rc_v2->power_save = c->power_save;
	rc_v2->power_plcy = c->power_plcy;
	rc_v2->power_dtime = c->power_dtime;
	rc_v2->app_type = c->app_type;
	rc_v2->num_subcmds = c->num_subcmds;
	rc_v2->num_cmdbufs = c->num_cmdbufs;
	rc_v2->subcmds_offset = subcmds_offset;
	rc_v2->cmdbufs_offset = cmdbufs_offset;
	rc_v2->adj_matrix_offset = adj_matrix_offset;
	mdw_rv_cmd_print(rc_v2);

	/* copy adj matrix */
	memcpy((void *)rc_v2 + rc_v2->adj_matrix_offset, c->adj_matrix,
		c->num_subcmds * c->num_subcmds * sizeof(uint8_t));

	/* assign subcmds info */
	rsubcmds_v2 = (void *)rc_v2 + rc_v2->subcmds_offset;
	rcb_v2 = (void *)rc_v2 + rc_v2->cmdbufs_offset;
	for (i = 0; i < c->num_subcmds; i++) {
		rsubcmds_v2[i].type = c->subcmds[i].type;
		rsubcmds_v2[i].suggest_time = c->subcmds[i].suggest_time;
		rsubcmds_v2[i].vlm_usage = c->subcmds[i].vlm_usage;
		rsubcmds_v2[i].vlm_ctx_id = c->subcmds[i].vlm_ctx_id;
		rsubcmds_v2[i].vlm_force = c->subcmds[i].vlm_force;
		rsubcmds_v2[i].boost = c->subcmds[i].boost;
		rsubcmds_v2[i].ip_time = c->subcmds[i].ip_time;
		rsubcmds_v2[i].driver_time = c->subcmds[i].driver_time;
		rsubcmds_v2[i].bw = c->subcmds[i].bw;
		rsubcmds_v2[i].turbo_boost = c->subcmds[i].turbo_boost;
		rsubcmds_v2[i].min_boost = c->subcmds[i].min_boost;
		rsubcmds_v2[i].max_boost = c->subcmds[i].max_boost;
		rsubcmds_v2[i].hse_en = c->subcmds[i].hse_en;
		rsubcmds_v2[i].pack_id = c->subcmds[i].pack_id;
		rsubcmds_v2[i].num_cmdbufs = c->subcmds[i].num_cmdbufs;
		rsubcmds_v2[i].cmdbuf_start_idx = acc_cb;
		mdw_rv_sc_print(&rsubcmds_v2[i], rc_v2->cmd_id, i);

		for (j = 0; j < rsubcmds_v2[i].num_cmdbufs; j++) {
			rcb_v2[acc_cb + j].size =
				c->ksubcmds[i].cmdbufs[j].size;
			rcb_v2[acc_cb + j].device_va =
				c->ksubcmds[i].daddrs[j];
			mdw_cmd_debug("sc(%u) #%u-cmdbufs 0x%llx/%u\n",
				i, j,
				rcb_v2[acc_cb + j].device_va,
				rcb_v2[acc_cb + j].size);
		}
		acc_cb += c->subcmds[i].num_cmdbufs;
	}

	goto out;

free_rc:
	vfree(rc);
	rc = NULL;
out:
	return rc;
}

static int mdw_rv_cmd_delete(struct mdw_rv_cmd *rc)
{
	if (!rc)
		return -EINVAL;

	mdw_mem_free(rc->c->mpriv, rc->cb);
	vfree(rc);

	return 0;
}

int mdw_rv_cmd_exec(struct mdw_fpriv *mpriv, struct mdw_cmd *c)
{
	struct mdw_rv_cmd *rc = NULL;
	int ret = 0;

	mdw_flw_debug("\n");
	rc = mdw_rv_cmd_create(mpriv, c);
	if (!rc)
		return -ENOMEM;
	mdw_flw_debug("\n");
	ret = mdw_rv_dev_run_cmd(rc);
	mdw_flw_debug("\n");
	return ret;
}

void mdw_rv_cmd_done(struct mdw_rv_cmd *rc, int ret)
{
	struct mdw_cmd *c = rc->c;

	mdw_flw_debug("\n");
	mdw_rv_cmd_delete(rc);
	c->complete(c, ret);
}
