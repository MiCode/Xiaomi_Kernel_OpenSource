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

#ifndef _DDP_WDMA_EX_H_
#define _DDP_WDMA_EX_H_

#include "ddp_hal.h"
#include "ddp_info.h"

#define WDMA_INSTANCES  2
#define WDMA_MAX_WIDTH  1920
#define WDMA_MAX_HEIGHT 1080

enum WDMA_INPUT_FORMAT {
	WDMA_INPUT_FORMAT_ARGB = 0x00,	/* from overlay */
	WDMA_INPUT_FORMAT_YUV444 = 0x01,	/* from direct link */
};

#endif
