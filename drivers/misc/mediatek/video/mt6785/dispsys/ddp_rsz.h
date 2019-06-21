/*
 * Copyright (C) 2016 MediaTek Inc.
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

#ifndef _DDP_RSZ_H_
#define _DDP_RSZ_H_

#include "ddp_hal.h"
#include "ddp_info.h"

/* platform dependent */
#define RSZ_TILE_LENGTH 1080
#define RSZ_ALIGNMENT_MARGIN 6 /* for alignment tolerance */
#define RSZ_IN_MAX_HEIGHT 4096
#define MDP_ALIGNMENT_MARGIN 2 /* for MDP alignment tolerance */

void rsz_dump_analysis(enum DISP_MODULE_ENUM module);
void rsz_dump_reg(enum DISP_MODULE_ENUM module);

#endif
