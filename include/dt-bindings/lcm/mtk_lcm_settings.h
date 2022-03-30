/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Device Tree defines for LCM settings
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mtk_lcm_lk_settings.h"

#ifndef MTK_LCM_SETTINGS_H
#define MTK_LCM_SETTINGS_H

#define MAX_UINT32						((u32)~0U)
#define MAX_INT32						((s32)(MAX_UINT32 >> 1))

#define MTK_PANEL_TABLE_OPS_COUNT				1024
#define MTK_PANEL_COMPARE_ID_LENGTH				10
#define MTK_PANEL_ATA_ID_LENGTH					10

#define BL_PWM_MODE						0
#define BL_I2C_MODE						1

/* LCM PHY TYPE*/
#define MTK_LCM_MIPI_DPHY					0
#define MTK_LCM_MIPI_CPHY					1
#define MTK_LCM_MIPI_PHY_COUNT   				(MTK_LCM_MIPI_CPHY + 1)

/*redefine "mipi_dsi_pixel_format" from enum to macro for dts settings*/
#define MTK_MIPI_DSI_FMT_RGB888					0
#define MTK_MIPI_DSI_FMT_RGB666					1
#define MTK_MIPI_DSI_FMT_RGB666_PACKED				2
#define MTK_MIPI_DSI_FMT_RGB565					3

/*redefine MTK_PANEL_OUTPUT_MODE from enum to macro for dts settings */
#define MTK_LCM_PANEL_SINGLE_PORT				0
#define MTK_LCM_PANEL_DSC_SINGLE_PORT				1
#define MTK_LCM_PANEL_DUAL_PORT					2

/*redefine "mtk_drm_color_mode" from enum to macro for dts settings*/
#define MTK_LCM_COLOR_MODE_NATIVE				0
#define MTK_LCM_COLOR_MODE_STANDARD_BT601_625			1
#define MTK_LCM_COLOR_MODE_STANDARD_BT601_625_UNADJUSTED	2
#define MTK_LCM_COLOR_MODE_STANDARD_BT601_525			3
#define MTK_LCM_COLOR_MODE_STANDARD_BT601_525_UNADJUSTED	4
#define MTK_LCM_COLOR_MODE_STANDARD_BT709			5
#define MTK_LCM_COLOR_MODE_DCI_P3				6
#define MTK_LCM_COLOR_MODE_SRGB					7
#define MTK_LCM_COLOR_MODE_ADOBE_RGB				8
#define MTK_LCM_COLOR_MODE_DISPLAY_P3				9

/*redefine "MIPITX_PHY_LANE_SWAP" from enum to macro for dts settings*/
#define LCM_LANE_0						0
#define LCM_LANE_1						1
#define LCM_LANE_2						2
#define LCM_LANE_3						3
#define LCM_LANE_CK						4
#define LCM_LANE_RX						5

/*redefine DSI mode flags to fix build error*/
/*video mode */
#define MTK_MIPI_DSI_MODE_VIDEO					(1U << 0)
/*video burst mode */
#define MTK_MIPI_DSI_MODE_VIDEO_BURST				(1U << 1)
/*video pulse mode */
#define MTK_MIPI_DSI_MODE_VIDEO_SYNC_PULSE			(1U << 2)
/*enable auto vertical count mode */
#define MTK_MIPI_DSI_MODE_VIDEO_AUTO_VERT			(1U << 3)
/*enable hsync-end packets in vsync-pulse and v-porch area */
#define MTK_MIPI_DSI_MODE_VIDEO_HSE				(1U << 4)
/*disable hfront-porch area */
#define MTK_MIPI_DSI_MODE_VIDEO_HFP				(1U << 5)
/*disable hback-porch area */
#define MTK_MIPI_DSI_MODE_VIDEO_HBP				(1U << 6)
/*disable hsync-active area */
#define MTK_MIPI_DSI_MODE_VIDEO_HSA				(1U << 7)
/*flush display FIFO on vsync pulse */
#define MTK_MIPI_DSI_MODE_VSYNC_FLUSH				(1U << 8)
/*disable EoT packets in HS mode */
#define MTK_MIPI_DSI_MODE_EOT_PACKET				(1U << 9)
/*device supports non-continuous clock behavior (DSI spec 5.6.1) */
#define MTK_MIPI_DSI_CLOCK_NON_CONTINUOUS			(1U << 10)
/*transmit data in low power */
#define MTK_MIPI_DSI_MODE_LPM					(1U << 11)
/*disable BLLP area */
#define MTK_MIPI_DSI_MODE_VIDEO_BLLP				(1U << 12)
/*disable EOF BLLP area */
#define MTK_MIPI_DSI_MODE_VIDEO_EOF_BLLP			(1U << 13)

/* redefine enum TE_TYPE to macro for dts settings*/
#define MTK_LCM_NORMAL_TE					0
#define MTK_LCM_REQUEST_TE					1
#define MTK_LCM_MULTI_TE					2
#define MTK_LCM_TRIGGER_LEVEL_TE  				4

/* LCM_FUNC used for common operation */
#define MTK_LCM_FUNC_DBI					0
#define MTK_LCM_FUNC_DPI					1
#define MTK_LCM_FUNC_DSI					2
#define MTK_LCM_FUNC_END					(MTK_LCM_FUNC_DSI + 1)

/* 0~127: used for common panel operation
 * customer is forbidden to use common panel operation
 * for dtsi settings
 */
#define MTK_LCM_UTIL_TYPE_HEX_START				00
#define MTK_LCM_UTIL_TYPE_HEX_RESET				01
#define MTK_LCM_UTIL_TYPE_HEX_POWER_ON				02
#define MTK_LCM_UTIL_TYPE_HEX_POWER_OFF				03
#define MTK_LCM_UTIL_TYPE_HEX_POWER_VOLTAGE			04
#define MTK_LCM_UTIL_TYPE_HEX_MDELAY				05
#define MTK_LCM_UTIL_TYPE_HEX_UDELAY				06
#define MTK_LCM_UTIL_TYPE_HEX_TDELAY				07
#define MTK_LCM_UTIL_TYPE_HEX_END				0f

#define MTK_LCM_CMD_TYPE_HEX_START				10
#define MTK_LCM_CMD_TYPE_HEX_WRITE_BUFFER			11
#define MTK_LCM_CMD_TYPE_HEX_WRITE_CMD				12
#define MTK_LCM_CMD_TYPE_HEX_READ_BUFFER			13
#define MTK_LCM_CMD_TYPE_HEX_READ_CMD				14
/*mipi dcs write by condition*/
#define MTK_LCM_CMD_TYPE_HEX_WRITE_BUFFER_CONDITION		15
/*mipi dcs write runtime input data*/
#define MTK_LCM_CMD_TYPE_HEX_WRITE_BUFFER_RUNTIME_INPUT		16
#define MTK_LCM_CMD_TYPE_HEX_END				1f

#define MTK_LCM_CB_TYPE_HEX_START				20
/*runtime executed in callback*/
#define MTK_LCM_CB_TYPE_HEX_RUNTIME				21
/*runtime executed with single input*/
#define MTK_LCM_CB_TYPE_HEX_RUNTIME_INPUT			22
/*runtime executed with multiple input*/
#define MTK_LCM_CB_TYPE_HEX_RUNTIME_INPUT_MULTIPLE		23
#define MTK_LCM_CB_TYPE_HEX_END					2f

#define MTK_LCM_GPIO_TYPE_HEX_START				30
#define MTK_LCM_GPIO_TYPE_HEX_MODE				31
#define MTK_LCM_GPIO_TYPE_HEX_DIR_OUTPUT			32
#define MTK_LCM_GPIO_TYPE_HEX_DIR_INPUT				33
#define MTK_LCM_GPIO_TYPE_HEX_OUT				34
#define MTK_LCM_GPIO_TYPE_HEX_END				3f

#define MTK_LCM_LK_TYPE_HEX_START				40
/*lk write dcs data w/ force update*/
#define MTK_LCM_LK_TYPE_HEX_WRITE_PARAM				41
/*lk dcs data count*/
#define MTK_LCM_LK_TYPE_HEX_PREPARE_PARAM_COUNT			42
/*lk fixed dcs data value of 32bit*/
#define MTK_LCM_LK_TYPE_HEX_PREPARE_PARAM			43
/*lk fixed dcs data value of 8bit*/
#define MTK_LCM_LK_TYPE_HEX_PREPARE_PARAM_FIX_BIT		44
#define MTK_LCM_LK_TYPE_HEX_PREPARE_PARAM_X0_MSB_BIT		45
#define MTK_LCM_LK_TYPE_HEX_PREPARE_PARAM_X0_LSB_BIT		46
#define MTK_LCM_LK_TYPE_HEX_PREPARE_PARAM_X1_MSB_BIT		47
#define MTK_LCM_LK_TYPE_HEX_PREPARE_PARAM_X1_LSB_BIT		48
#define MTK_LCM_LK_TYPE_HEX_PREPARE_PARAM_Y0_MSB_BIT		49
#define MTK_LCM_LK_TYPE_HEX_PREPARE_PARAM_Y0_LSB_BIT		4a
#define MTK_LCM_LK_TYPE_HEX_PREPARE_PARAM_Y1_MSB_BIT		4b
#define MTK_LCM_LK_TYPE_HEX_PREPARE_PARAM_Y1_LSB_BIT		4c
#define MTK_LCM_LK_TYPE_HEX_WRITE_PARAM_UNFORCE			4d
#define MTK_LCM_LK_TYPE_HEX_END					4f

/* 128~223: used for customization panel operation
 * customer operation can add operation here
 */
#define MTK_LCM_CUST_TYPE_HEX_START				80
#define MTK_LCM_CUST_TYPE_HEX_END				df

#define MTK_LCM_PHASE_TYPE_HEX_START				f0
#define MTK_LCM_PHASE_TYPE_HEX_END				f1
#define MTK_LCM_TYPE_HEX_END					ff

#define MTK_LCM_UTIL_RESET_LOW					00
#define MTK_LCM_UTIL_RESET_HIGH					01

#define MTK_LCM_PHASE_HEX_KERNEL				01
#define MTK_LCM_PHASE_HEX_LK					02
#define MTK_LCM_PHASE_HEX_LK_DISPLAY_ON_DELAY			04

/*used for runtime input settings */
#define MTK_LCM_INPUT_TYPE_HEX_READBACK				01
#define MTK_LCM_INPUT_TYPE_HEX_CURRENT_FPS			02
#define MTK_LCM_INPUT_TYPE_HEX_CURRENT_BACKLIGHT		03

/* 0~127: used for common panel operation
 * customer is forbidden to use common panel operation
 * for source code
 */
#define MTK_LCM_UTIL_TYPE_START					0x00
#define MTK_LCM_UTIL_TYPE_RESET					0x01
#define MTK_LCM_UTIL_TYPE_POWER_ON				0x02
#define MTK_LCM_UTIL_TYPE_POWER_OFF				0x03
#define MTK_LCM_UTIL_TYPE_POWER_VOLTAGE				0x04
#define MTK_LCM_UTIL_TYPE_MDELAY				0x05
#define MTK_LCM_UTIL_TYPE_UDELAY				0x06
#define MTK_LCM_UTIL_TYPE_TDELAY				0x07
#define MTK_LCM_UTIL_TYPE_END					0x0f

#define MTK_LCM_CMD_TYPE_START					0x10
#define MTK_LCM_CMD_TYPE_WRITE_BUFFER				0x11
#define MTK_LCM_CMD_TYPE_WRITE_CMD				0x12
#define MTK_LCM_CMD_TYPE_READ_BUFFER				0x13
#define MTK_LCM_CMD_TYPE_READ_CMD				0x14
#define MTK_LCM_CMD_TYPE_WRITE_BUFFER_CONDITION			0x15
#define MTK_LCM_CMD_TYPE_WRITE_BUFFER_RUNTIME_INPUT		0x16
#define MTK_LCM_CMD_TYPE_END					0x1f

#define MTK_LCM_CB_TYPE_START					0x20
#define MTK_LCM_CB_TYPE_RUNTIME					0x21
#define MTK_LCM_CB_TYPE_RUNTIME_INPUT				0x22
#define MTK_LCM_CB_TYPE_RUNTIME_INPUT_MULTIPLE			0x23
#define MTK_LCM_CB_TYPE_END					0x2f

#define MTK_LCM_GPIO_TYPE_START					0x30
#define MTK_LCM_GPIO_TYPE_MODE					0x31
#define MTK_LCM_GPIO_TYPE_DIR_OUTPUT				0x32
#define MTK_LCM_GPIO_TYPE_DIR_INPUT				0x33
#define MTK_LCM_GPIO_TYPE_OUT					0x34
#define MTK_LCM_GPIO_TYPE_END					0x3f

#define MTK_LCM_LK_TYPE_START					0x40
#define MTK_LCM_LK_TYPE_WRITE_PARAM				0x41
#define MTK_LCM_LK_TYPE_PREPARE_PARAM_COUNT			0x42
#define MTK_LCM_LK_TYPE_PREPARE_PARAM				0x43
#define MTK_LCM_LK_TYPE_PREPARE_PARAM_FIX_BIT			0x44
#define MTK_LCM_LK_TYPE_PREPARE_PARAM_X0_MSB_BIT		0x45
#define MTK_LCM_LK_TYPE_PREPARE_PARAM_X0_LSB_BIT		0x46
#define MTK_LCM_LK_TYPE_PREPARE_PARAM_X1_MSB_BIT		0x47
#define MTK_LCM_LK_TYPE_PREPARE_PARAM_X1_LSB_BIT		0x48
#define MTK_LCM_LK_TYPE_PREPARE_PARAM_Y0_MSB_BIT		0x49
#define MTK_LCM_LK_TYPE_PREPARE_PARAM_Y0_LSB_BIT		0x4a
#define MTK_LCM_LK_TYPE_PREPARE_PARAM_Y1_MSB_BIT		0x4b
#define MTK_LCM_LK_TYPE_PREPARE_PARAM_Y1_LSB_BIT		0x4c
#define MTK_LCM_LK_TYPE_WRITE_PARAM_UNFORCE			0x4d
#define MTK_LCM_LK_TYPE_END					0x4f

/* 128~223: used for customization panel operation
 * customer operation can add operation here
 */
#define MTK_LCM_CUST_TYPE_START					0x80
#define MTK_LCM_CUST_TYPE_END					0xdf

#define MTK_LCM_PHASE_TYPE_START				0xf0
#define MTK_LCM_PHASE_TYPE_END					0xf1
#define MTK_LCM_TYPE_END					0xff

#define MTK_LCM_PHASE_KERNEL					0x01
#define MTK_LCM_PHASE_LK					0x02
#define MTK_LCM_PHASE_LK_DISPLAY_ON_DELAY			0x04

/*used for runtime input settings */
#define MTK_LCM_INPUT_TYPE_READBACK				0x01
#define MTK_LCM_INPUT_TYPE_CURRENT_FPS				0x02
#define MTK_LCM_INPUT_TYPE_CURRENT_BACKLIGHT			0x03

#endif
