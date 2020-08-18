/*
 * Copyright (C) 2019 MediaTek Inc.
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
#include <linux/mutex.h>
#include <linux/slab.h>

#include "mdw_cmn.h"
#include "apusys_drv.h"
#include "cmd_parser.h"

/* specific subgratph information */
int parse_mdla_codebuf_info(struct apusys_subcmd *sc,
	struct apusys_cmd_hnd *hnd)
{
	struct apusys_sc_hdr_mdla *mdla_hdr = NULL;
	struct apusys_cmd *cmd = NULL;

	if (sc->type != APUSYS_DEVICE_MDLA ||
		sc->par_cmd == NULL) {
		return -EINVAL;
	}

	cmd = sc->par_cmd;
	mdla_hdr = &sc->c_hdr->mdla;
	hnd->pmu_kva = (uint64_t)cmd->u_hdr + mdla_hdr->ofs_pmu_info;

	return 0;
}

int set_multimdla_codebuf(struct apusys_subcmd *sc,
	struct apusys_cmd_hnd *hnd, int idx)
{
	struct apusys_sc_hdr_mdla *mdla_hdr = NULL;
	struct apusys_cmd *cmd = NULL;
	uint64_t ofs = 0;

	if (sc == NULL || hnd == NULL)
		return -EINVAL;

	cmd = sc->par_cmd;
	mdla_hdr = &sc->c_hdr->mdla;
	if (idx == 0)
		ofs = mdla_hdr->ofs_cb_info_dual0;
	else if (idx == 1)
		ofs = mdla_hdr->ofs_cb_info_dual1;
	else
		return -EINVAL;

	hnd->kva = (uint64_t)cmd->u_hdr + ofs;
	mdw_drv_debug("set multi-mdla(%d) codebuf(0x%llx/0x%llx/0x%llx)\n",
		idx, hnd->kva, (uint64_t)cmd->u_hdr, ofs);

	return 0;
}

int check_multimdla_support(struct apusys_subcmd *sc)
{
	struct apusys_sc_hdr_mdla *mdla_hdr = NULL;

	if (sc->type != APUSYS_DEVICE_MDLA)
		return 0;

	mdla_hdr = &sc->c_hdr->mdla;
	return (mdla_hdr->ofs_cb_info_dual0 && mdla_hdr->ofs_cb_info_dual1);
}
