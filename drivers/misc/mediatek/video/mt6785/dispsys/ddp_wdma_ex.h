/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef _DDP_WDMA_EX_H_
#define _DDP_WDMA_EX_H_

#include "ddp_hal.h"
#include "ddp_info.h"

#define WDMA_INSTANCES	2
#define WDMA_MAX_WIDTH	1920
#define WDMA_MAX_HEIGHT	1080

enum WDMA_INPUT_FORMAT {
	WDMA_INPUT_FORMAT_ARGB = 0x00,		/* from overlay */
	WDMA_INPUT_FORMAT_YUV444 = 0x01,	/* from direct link */
};

void wdma_calc_golden_setting(struct golden_setting_context *gsc,
	unsigned int is_primary_flag, unsigned int *gs,
	enum UNIFIED_COLOR_FMT format);

unsigned int MMPathTracePrimaryWDMA(char *str, unsigned int strlen,
	unsigned int n);
#endif /* _DDP_WDMA_EX_H_ */
