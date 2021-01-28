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

#ifndef __DDP_DPI_H__
#define __DDP_DPI_H__

/*#include <mach/mt_typedefs.h>*/
#include <linux/types.h>

#include "lcm_drv.h"
#include "ddp_info.h"
#include "cmdq_record.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef FALSE
#define FALSE (0)
#endif

#ifndef TRUE
#define TRUE  (1)
#endif

#ifndef NULL
#define NULL  (0)
#endif

#define DPI_PHY_ADDR 0x14024000

#define DPI_CHECK_RET(expr)  \
	do { \
		enum DPI_STATUS ret = (expr); \
		ASSERT(ret == DPI_STATUS_OK);  \
	} while (0) \

/*for legacy DPI Driver*/
enum LCD_IF_ID {
	LCD_IF_PARALLEL_0 = 0,
	LCD_IF_PARALLEL_1 = 1,
	LCD_IF_PARALLEL_2 = 2,
	LCD_IF_SERIAL_0 = 3,
	LCD_IF_SERIAL_1 = 4,

	LCD_IF_ALL = 0xFF,
};

struct LCD_REG_CMD_ADDR {
	unsigned rsv_0:4;
	unsigned addr:4;
	unsigned rsv_8:24;
};

struct LCD_REG_DAT_ADDR {
	unsigned rsv_0:4;
	unsigned addr:4;
	unsigned rsv_8:24;
};

enum LCD_IF_FMT_COLOR_ORDER {
	LCD_IF_FMT_COLOR_ORDER_RGB = 0,
	LCD_IF_FMT_COLOR_ORDER_BGR = 1,
};


enum LCD_IF_FMT_TRANS_SEQ {
	LCD_IF_FMT_TRANS_SEQ_MSB_FIRST = 0,
	LCD_IF_FMT_TRANS_SEQ_LSB_FIRST = 1,
};


enum LCD_IF_FMT_PADDING {
	LCD_IF_FMT_PADDING_ON_LSB = 0,
	LCD_IF_FMT_PADDING_ON_MSB = 1,
};


enum LCD_IF_FORMAT {
	LCD_IF_FORMAT_RGB332 = 0,
	LCD_IF_FORMAT_RGB444 = 1,
	LCD_IF_FORMAT_RGB565 = 2,
	LCD_IF_FORMAT_RGB666 = 3,
	LCD_IF_FORMAT_RGB888 = 4,
};

enum LCD_IF_WIDTH {
	LCD_IF_WIDTH_8_BITS = 0,
	LCD_IF_WIDTH_9_BITS = 2,
	LCD_IF_WIDTH_16_BITS = 1,
	LCD_IF_WIDTH_18_BITS = 3,
	LCD_IF_WIDTH_24_BITS = 4,
	LCD_IF_WIDTH_32_BITS = 5,
};

enum DPI_STATUS {
	DPI_STATUS_OK = 0,

	DPI_STATUS_ERROR,
};

enum DPI_POLARITY {
	DPI_POLARITY_RISING = 0,
	DPI_POLARITY_FALLING = 1
};

enum DPI_RGB_ORDER {
	DPI_RGB_ORDER_RGB = 0,
	DPI_RGB_ORDER_BGR = 1,
};

enum HDMI_VIDEO_RESOLUTION {
	HDMI_VIDEO_720x480p_60Hz = 0,
	HDMI_VIDEO_1440x480i_60Hz = 1,
	HDMI_VIDEO_1280x720p_60Hz = 2,
	HDMI_VIDEO_1920x1080i_60Hz = 5,
	HDMI_VIDEO_1920x1080p_30Hz = 6,
	HDMI_VIDEO_720x480i_60Hz = 0xD,
	HDMI_VIDEO_1920x1080p_60Hz = 0x0b,
	HDMI_VIDEO_2160p_DSC_30Hz = 0x13,
	HDMI_VIDEO_2160p_DSC_24Hz = 0x14,
	HDMI_VIDEO_RESOLUTION_NUM
};

struct LCD_REG_WROI_CON {
	unsigned RGB_ORDER:1;
	unsigned BYTE_ORDER:1;
	unsigned PADDING:1;
	unsigned DATA_FMT:3;
	unsigned IF_FMT:2;
	unsigned COMMAND:5;
	unsigned rsv_13:2;
	unsigned ENC:1;
	unsigned rsv_16:8;
	unsigned SEND_RES_MODE:1;
	unsigned IF_24:1;
	unsigned rsv_6:6;
};

int ddp_dpi_stop(enum DISP_MODULE_ENUM module, void *cmdq_handle);
int ddp_dpi_power_on(enum DISP_MODULE_ENUM module, void *cmdq_handle);
int ddp_dpi_power_off(enum DISP_MODULE_ENUM module, void *cmdq_handle);
int ddp_dpi_dump(enum DISP_MODULE_ENUM module, int level);
int ddp_dpi_start(enum DISP_MODULE_ENUM module, void *cmdq);
int ddp_dpi_init(enum DISP_MODULE_ENUM module, void *cmdq);
int ddp_dpi_deinit(enum DISP_MODULE_ENUM module, void *cmdq_handle);
int ddp_dpi_config(enum DISP_MODULE_ENUM module,
		struct disp_ddp_path_config *config, void *cmdq_handle);
int ddp_dpi_trigger(enum DISP_MODULE_ENUM module, void *cmdq);
int ddp_dpi_reset(enum DISP_MODULE_ENUM module, void *cmdq_handle);
int ddp_dpi_ioctl(enum DISP_MODULE_ENUM module, void *cmdq_handle,
		enum DDP_IOCTL_NAME ioctl_cmd, void *params);

int _Enable_Interrupt(void);

enum AviColorSpace_e {
	acsRGB = 0, acsYCbCr422 = 1, acsYCbCr444 = 2, acsFuture = 3
};
#ifdef __cplusplus
}
#endif
#endif				/*__DPI_DRV_H__*/
