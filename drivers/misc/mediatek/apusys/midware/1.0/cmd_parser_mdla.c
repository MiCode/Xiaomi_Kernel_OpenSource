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

#include "apusys_cmn.h"
#include "apusys_drv.h"
#include "cmd_parser.h"

/* specific subgratph information */
int parse_mdla_sg(struct apusys_subcmd *sc, struct apusys_cmd_hnd *hnd)
{
	struct apusys_sc_hdr_mdla *mdla_hdr = NULL;
	struct apusys_cmd *cmd = NULL;

	if (sc->d_hdr == NULL ||
		sc->type != APUSYS_DEVICE_MDLA ||
		sc->par_cmd == NULL) {
		return -EINVAL;
	}

	cmd = sc->par_cmd;
	mdla_hdr = (struct apusys_sc_hdr_mdla *)sc->d_hdr;
	hnd->pmu_kva = (uint64_t)cmd->hdr + mdla_hdr->ofs_pmu_info;

	return 0;
}
