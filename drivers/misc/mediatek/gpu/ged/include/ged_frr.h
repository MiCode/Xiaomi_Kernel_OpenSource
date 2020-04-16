/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef __GED_FRAME_SYNC_H__
#define __GED_FRAME_SYNC_H__

#include "ged_type.h"

#define GED_FRR_FENCE2CONTEXT_TABLE_SIZE 5

GED_ERROR ged_frr_system_init(void);

GED_ERROR ged_frr_system_exit(void);

int ged_frr_get_fps(int targetPid, uint64_t targetCid);

GED_ERROR ged_frr_fence2context_table_update(int pid, uint64_t cid, int fenceFd);

GED_ERROR ged_frr_fence2context_table_get_cid(int pid, void *fid, uint64_t *cid);

GED_ERROR ged_frr_wait_hw_vsync(void);
#endif
