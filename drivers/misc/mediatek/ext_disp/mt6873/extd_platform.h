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

#ifndef __EXTD_PLATFORM_H__
#define __EXTD_PLATFORM_H__

#include "ddp_hal.h"


/* #define MTK_LCD_HW_3D_SUPPORT */
#define ALIGN_TO(x, n)  \
	(((x) + ((n) - 1)) & ~((n) - 1))

/* #define EXTD_DEBUG_SUPPORT */

/* /#define EXTD_DBG_USE_INNER_BUF */

#define HW_OVERLAY_COUNT  2

#define EXTD_OVERLAY_CNT  2

#define HW_DPI_VSYNC_SUPPORT 1

#define DISP_MODULE_RDMA DISP_MODULE_RDMA0

#define MM_MHL_DVFS
#define MHL_DYNAMIC_VSYNC_OFFSET

#undef MTK_AUDIO_MULTI_CHANNEL_SUPPORT
#define FIX_EXTD_TO_OVL_PATH EXTD_OVERLAY_CNT

#define MHL_RESOLUTION_LIMIT_1080P_30

/* #define CONFIG_IO_DRIVING */

/* ANX7805 use this define */
/* #define DP_EINT_GPIO_NUMBER 98 */

#endif
