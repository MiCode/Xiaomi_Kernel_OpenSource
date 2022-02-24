// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _DDP_RSZ_H_
#define _DDP_RSZ_H_

#include "ddp_hal.h"
#include "ddp_info.h"

/* platform dependent */
#define RSZ_TILE_LENGTH 736
#define RSZ_ALIGNMENT_MARGIN 6 /* for alignment tolerance */
#define RSZ_IN_MAX_HEIGHT 4096
#define MDP_ALIGNMENT_MARGIN 2 /* for MDP alignment tolerance */

void rsz_dump_analysis(enum DISP_MODULE_ENUM module);
void rsz_dump_reg(enum DISP_MODULE_ENUM module);

#endif
