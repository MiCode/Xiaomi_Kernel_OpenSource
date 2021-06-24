// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mdw_rv.h"
#include "mdw_cmn.h"

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
	rc_v2->priority = c->priority;
	rc_v2->hardlimit = c->hardlimit;
	rc_v2->softlimit = c->softlimit;
	rc_v2->power_save = c->power_save;
	rc_v2->num_subcmds = c->num_subcmds;
	rc_v2->num_cmdbufs = c->num_cmdbufs;
	rc_v2->subcmds_offset = subcmds_offset;
	rc_v2->cmdbufs_offset = cmdbufs_offset;
	rc_v2->adj_matrix_offset = adj_matrix_offset;

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
		rsubcmds_v2[i].pack_id = c->subcmds[i].pack_id;
		rsubcmds_v2[i].num_cmdbufs = c->subcmds[i].num_cmdbufs;
		rsubcmds_v2[i].cmdbuf_start_idx = acc_cb;
		mdw_cmd_debug("sc(%u) cmdbufs(%u/%u|%u)\n",
			i,
			rsubcmds_v2[i].cmdbuf_start_idx,
			rsubcmds_v2[i].num_cmdbufs,
			rc_v2->num_cmdbufs);
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

	//c->priv = rc;

	goto out;

free_rc:
	vfree(rc);
	rc = NULL;
out:
	return rc;
}

static int mdw_rv_cmd_delete(struct mdw_fpriv *mpriv, struct mdw_rv_cmd *rc)
{
	if (!rc)
		return -EINVAL;

	mdw_mem_free(mpriv, rc->cb);
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
	ret = mdw_rv_cmd_delete(mpriv, rc);
	mdw_flw_debug("\n");
	return ret;
}
