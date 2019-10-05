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
#include "cmd_format.h"

/* specific subgratph information */

#define TYPE_SUBGRAPH_PMU_OFFSET uint32_t

#define OFFSET_SUBGRAPH_CODEBUF_INFO_OFFSET1 \
	(OFFSET_SUBGRAPH_CODEBUF_INFO_OFFSET +\
	SIZE_SUBGRAPH_CODEBUF_INFO_OFFSET)
#define OFFSET_SUBGRAPH_CODEBUF_INFO_OFFSET2 \
	(OFFSET_SUBGRAPH_CODEBUF_INFO_OFFSET1 +\
	SIZE_SUBGRAPH_CODEBUF_INFO_OFFSET)
#define OFFSET_SUBGRAPH_PMU_OFFSET \
	(OFFSET_SUBGRAPH_CODEBUF_INFO_OFFSET2 +\
	SIZE_SUBGRAPH_CODEBUF_INFO_OFFSET)

static uint32_t _get_pmu_offset_from_subcmd(uint64_t subcmd)
{
	return *(TYPE_SUBGRAPH_PMU_OFFSET *)
		(subcmd + OFFSET_SUBGRAPH_PMU_OFFSET);
}

int parse_mdla_sg(struct apusys_cmd *cmd,
	struct apusys_subcmd *sc, struct apusys_cmd_hnd *hnd)
{
	uint32_t pmu_offset = _get_pmu_offset_from_subcmd((uint64_t)sc->entry);

	LOG_DEBUG("cmd entry(%p/%p), pmu offset(0x%x)\n",
		cmd->entry, (TYPE_SUBGRAPH_PMU_OFFSET *)((uint64_t)sc->entry +
		OFFSET_SUBGRAPH_PMU_OFFSET), pmu_offset);

	hnd->pmu_kva = (uint64_t)cmd->entry + (uint64_t)pmu_offset;

	return 0;
}
