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

#ifndef __EXTDISP_DRV_PLATFORM_H__
#define __EXTDISP_DRV_PLATFORM_H__

#include <linux/dma-mapping.h>
#include <linux/types.h>
#include <mt-plat/mt_gpio.h>
#include "m4u.h"
/* #include <mach/mt6585_pwm.h> */
/*#include <mach/mt_reg_base.h> */
/* #include <mach/mt_clkmgr.h> */
#include <linux/clk.h>

/* #include <mach/mt_irq.h> */
/* #include <mach/boot.h> */
/* #include <board-custom.h> */
#include "disp_assert_layer.h"
#include "ddp_hal.h"

#if 0
#include "ddp_drv.h"
#include "ddp_path.h"
#include "ddp_rdma.h"

#include <mach/sync_write.h>
#ifdef OUTREG32
#undef OUTREG32
#define OUTREG32(x, y) mt65xx_reg_sync_writel(y, x)
#endif

#ifndef OUTREGBIT
#define OUTREGBIT(TYPE, REG, bit, value)  \
	do {    \
		TYPE r = *((TYPE *)&INREG32(&REG));    \
		r.bit = value;    \
		OUTREG32(&REG, AS_UINT32(&r));    \
	} while (0)
#endif
#endif


#define MAX_SESSION_COUNT		5

/* #define MTK_LCD_HW_3D_SUPPORT */
#define ALIGN_TO(x, n)  \
	(((x) + ((n) - 1)) & ~((n) - 1))

#define MTK_EXT_DISP_ALIGNMENT 32
#define MTK_EXT_DISP_START_DSI_ISR
#define MTK_EXT_DISP_OVERLAY_SUPPORT
#define MTK_EXT_DISP_SYNC_SUPPORT
#define MTK_EXT_DISP_ION_SUPPORT

/* /#define EXTD_DBG_USE_INNER_BUF */

#define HW_OVERLAY_COUNT                 (4)


#endif				/* __DISP_DRV_PLATFORM_H__ */
