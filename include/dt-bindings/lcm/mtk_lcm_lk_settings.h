/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Device Tree defines for LCM settings
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef MTK_LCM_LK_SETTINGS_H
#define MTK_LCM_LK_SETTINGS_H

#define MAX_UINT32 ((u32)~0U)
#define MAX_INT32 ((s32)(MAX_UINT32 >> 1))

/* redefine of enum LCM_DSI_MODE_CON */
#define MTK_LK_CMD_MODE (0)
#define MTK_LK_SYNC_PULSE_VDO_MODE (1)
#define MTK_LK_SYNC_EVENT_VDO_MODE (2)
#define MTK_LK_BURST_VDO_MODE (3)

/*redefine of enum LCM_COLOR_ORDER*/
#define MTK_LCM_COLOR_ORDER_RGB (0)
#define MTK_LCM_COLOR_ORDER_BGR (1)

/* redefine of enum LCM_DSI_FORMAT*/
#define MTK_LCM_DSI_FORMAT_RGB565 (0)
#define MTK_LCM_DSI_FORMAT_RGB666 (1)
#define MTK_LCM_DSI_FORMAT_RGB888 (2)

/* redefine of enum LCM_DSI_TRANS_SEQ*/
#define MTK_LCM_DSI_TRANS_SEQ_MSB_FIRST (0)
#define MTK_LCM_DSI_TRANS_SEQ_LSB_FIRST (1)

/* redefine of enum LCM_DSI_PADDING*/
#define MTK_LCM_DSI_PADDING_ON_LSB (0)
#define MTK_LCM_DSI_PADDING_ON_MSB (1)

/* redefine of enum LCM_PS_TYPE*/
#define MTK_LCM_PACKED_PS_16BIT_RGB565 (0)
#define MTK_LCM_LOOSELY_PS_18BIT_RGB666 (1)
#define MTK_LCM_PACKED_PS_24BIT_RGB888 (2)
#define MTK_LCM_PACKED_PS_18BIT_RGB666 (3)

/* redefine of DUAL_DSI_TYPE */
#define DUAL_DSI_NONE (0x0)
#define DUAL_DSI_CMD (0x1)
#define DUAL_DSI_VDO (0x2)
#endif
