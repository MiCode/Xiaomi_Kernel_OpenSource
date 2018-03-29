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

#ifndef __DISP_DRV_PLATFORM_H__
#define __DISP_DRV_PLATFORM_H__

#include <linux/dma-mapping.h>
#include <linux/types.h>
#include <mt-plat/mt_gpio.h>
#include "m4u.h"
/* #include <mach/mt6585_pwm.h> */
/* #include <mach/mt_reg_base.h> */
#include <linux/clk.h>
/* #include <mach/mt_irq.h> */
/* #include <mach/boot.h> */
/* #include <board-custom.h> */
#include "disp_assert_layer.h"
#include "../dispsys/ddp_hal.h"
#include "../dispsys/ddp_drv.h"
#include "../dispsys/ddp_path.h"
#include "../dispsys/ddp_rdma.h"

#include <mt-plat/sync_write.h>

#ifdef CONFIG_MTK_TC8_TABLET_RELEASE_ONLY
#define MTK_DISP_IDLE_LP
#endif

#define MTKFB_NO_M4U
/* #define MTK_LCD_HW_3D_SUPPORT */
#define ALIGN_TO(x, n)  \
	(((x) + ((n) - 1)) & ~((n) - 1))
#define MTK_FB_ALIGNMENT 32
#define MTK_FB_START_DSI_ISR
#define MTK_FB_OVERLAY_SUPPORT
#define MTK_FB_SYNC_SUPPORT
#define MTK_FB_ION_SUPPORT
#define HW_OVERLAY_COUNT                 (4)
#define RESERVED_LAYER_COUNT             (2)
#define VIDEO_LAYER_COUNT                (HW_OVERLAY_COUNT - RESERVED_LAYER_COUNT)

#define MTK_FB_DFO_DISABLE
#define DFO_USE_NEW_API
#define MTKFB_FPGA_ONLY
#define DISP_SUPPORT_CMDQ
/* new macro definition for display driver platform dependency options */

#define PRIMARY_DISPLAY_HW_OVERLAY_LAYER_NUMBER		(4)
#define PRIMARY_DISPLAY_HW_OVERLAY_ENGINE_COUNT		(2)
/* if use 2 ovl 2 times in/out, this count could be 1.75 */
#define PRIMARY_DISPLAY_HW_OVERLAY_CASCADE_COUNT	(1)
#define PRIMARY_DISPLAY_SESSION_LAYER_COUNT \
	(PRIMARY_DISPLAY_HW_OVERLAY_LAYER_NUMBER*PRIMARY_DISPLAY_HW_OVERLAY_CASCADE_COUNT)

#define EXTERNAL_DISPLAY_SESSION_LAYER_COUNT			(PRIMARY_DISPLAY_HW_OVERLAY_LAYER_NUMBER)

#define DISP_SESSION_OVL_TIMELINE_ID(x)			(x)
#define DISP_SESSION_OUTPUT_TIMELINE_ID		(PRIMARY_DISPLAY_SESSION_LAYER_COUNT)
#define DISP_SESSION_PRESENT_TIMELINE_ID	(PRIMARY_DISPLAY_SESSION_LAYER_COUNT+1)
#define DISP_SESSION_OUTPUT_INTERFACE_TIMELINE_ID (PRIMARY_DISPLAY_SESSION_LAYER_COUNT+2)
#define DISP_SESSION_TIMELINE_COUNT			(DISP_SESSION_OUTPUT_INTERFACE_TIMELINE_ID+1)	/* 6 for ROME */
#define MAX_SESSION_COUNT					5
/* #define DISP_SWITCH_DST_MODE */
#define HDMI_MAIN_PATH (0)
#define MAIN_PATH_DISABLE_LCM (0)
#define HDMI_SUB_PATH_PROB_V2 (0)
#define HDMI_SUB_PATH_RECORD (0)
#define RELEASE_PRESENT_FENCE_WITH_IRQ_HANDLE (1)
#define HDMI_SUB_PATH_BOOT (0)

/*for MTK_HDMI_MAIN_PATH*/
extern unsigned int ddp_dbg_level;
#define DDP_FUNC_LOG (1 << 0)
#define DDP_FLOW_LOG (1 << 1)
#define DDP_COLOR_FORMAT_LOG (1 << 2)
#define DDP_FB_FLOW_LOG (1 << 3)
#define DDP_RESOLUTION_LOG (1 << 4)
#define DDP_OVL_FB_LOG (1 << 5)
#define DDP_FENCE_LOG (1 << 6)
#define DDP_TVE_FENCE_LOG (1 << 7)
#define DDP_FENCE1_LOG (1 << 8)
#define DDP_FENCE2_LOG (1 << 9)
#define DDP_CAPTURE_LOG (1 << 10)
#define DDP_VYSNC_LOG (1 << 11)

#define DISP_PRINTF(level, string, args...) do { \
	if (ddp_dbg_level & (level)) { \
		pr_debug("[DISP] "string, ##args); \
	} \
} while (0)

extern bool boot_up_with_facotry_mode(void);

#define BOOT_RES_480P  0
#define BOOT_RES_720P  1
#define BOOT_RES_1080P  2
#define BOOT_RES_2160P  3
#define BOOT_RES_2161P  4

/*
#define BOOT_RES  BOOT_RES_1080P
#define BOOT_RES  BOOT_RES_720P
*/
#define BOOT_RES  BOOT_RES_1080P

#if BOOT_RES == BOOT_RES_480P
#define HDMI_DISP_WIDTH 720
#define HDMI_DISP_HEIGHT 480
#define BOX_FIX_RES 0x0

#elif BOOT_RES == BOOT_RES_720P
#define HDMI_DISP_WIDTH 1280
#define HDMI_DISP_HEIGHT 720
#define BOX_FIX_RES 0x2

#elif BOOT_RES == BOOT_RES_1080P
#define HDMI_DISP_WIDTH 1920
#define HDMI_DISP_HEIGHT 1080
#define BOX_FIX_RES 0xb

#elif BOOT_RES == BOOT_RES_2160P
#define HDMI_DISP_WIDTH 3840
#define HDMI_DISP_HEIGHT 2160
#define BOX_FIX_RES 0x17

#elif BOOT_RES == BOOT_RES_2161P
#define HDMI_DISP_WIDTH 4096
#define HDMI_DISP_HEIGHT 2160
#define BOX_FIX_RES 0x19
#endif

#if defined(MTK_ALPS_BOX_SUPPORT)
#else
#define DISP_ENABLE_SODI
#endif

#endif				/* __DISP_DRV_PLATFORM_H__ */
