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


#include <linux/types.h>

#include "lcm_drv.h"
#include "ddp_info.h"
#include "cmdq_record.h"
#include <mt-plat/sync_write.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DPI_CHECK_RET(expr)             \
	do { \
		DPI_STATUS ret = (expr);        \
		ASSERT(DPI_STATUS_OK == ret);   \
	} while (0)

#define DPI_MASKREG32(cmdq, REG, MASK, VALUE)	DISP_REG_MASK((cmdq), (REG), (VALUE), (MASK))

#define DPI_OUTREGBIT(cmdq, TYPE, REG, bit, value)  \
	do { \
		TYPE r;\
		TYPE v;\
		if (cmdq) { \
			*(unsigned int *)(&r) = ((unsigned int)0x00000000); r.bit = ~(r.bit);  \
			*(unsigned int *)(&v) = ((unsigned int)0x00000000); v.bit = value; \
			DISP_REG_MASK(cmdq, (unsigned long)&REG, AS_UINT32(&v), AS_UINT32(&r)); }     \
		else { \
			mt_reg_sync_writel(INREG32(&REG), &r); r.bit = (value);     \
			DISP_REG_SET(cmdq, (unsigned long)&REG, AS_UINT32(&r)); }              \
	} while (0)


/* for legacy DPI Driver */
	typedef enum {
		LCD_IF_PARALLEL_0 = 0,
		LCD_IF_PARALLEL_1 = 1,
		LCD_IF_PARALLEL_2 = 2,
		LCD_IF_SERIAL_0 = 3,
		LCD_IF_SERIAL_1 = 4,

		LCD_IF_ALL = 0xFF,
	} LCD_IF_ID;

	typedef struct {
		unsigned rsv_0:4;
		unsigned addr:4;
		unsigned rsv_8:24;
	} LCD_REG_CMD_ADDR, *PLCD_REG_CMD_ADDR;

	typedef struct {
		unsigned rsv_0:4;
		unsigned addr:4;
		unsigned rsv_8:24;
	} LCD_REG_DAT_ADDR, *PLCD_REG_DAT_ADDR;


	typedef enum {
		LCD_IF_FMT_COLOR_ORDER_RGB = 0,
		LCD_IF_FMT_COLOR_ORDER_BGR = 1,
	} LCD_IF_FMT_COLOR_ORDER;


	typedef enum {
		LCD_IF_FMT_TRANS_SEQ_MSB_FIRST = 0,
		LCD_IF_FMT_TRANS_SEQ_LSB_FIRST = 1,
	} LCD_IF_FMT_TRANS_SEQ;


	typedef enum {
		LCD_IF_FMT_PADDING_ON_LSB = 0,
		LCD_IF_FMT_PADDING_ON_MSB = 1,
	} LCD_IF_FMT_PADDING;


	typedef enum {
		LCD_IF_FORMAT_RGB332 = 0,
		LCD_IF_FORMAT_RGB444 = 1,
		LCD_IF_FORMAT_RGB565 = 2,
		LCD_IF_FORMAT_RGB666 = 3,
		LCD_IF_FORMAT_RGB888 = 4,
	} LCD_IF_FORMAT;

	typedef enum {
		LCD_IF_WIDTH_8_BITS = 0,
		LCD_IF_WIDTH_9_BITS = 2,
		LCD_IF_WIDTH_16_BITS = 1,
		LCD_IF_WIDTH_18_BITS = 3,
		LCD_IF_WIDTH_24_BITS = 4,
		LCD_IF_WIDTH_32_BITS = 5,
	} LCD_IF_WIDTH;


	typedef enum {
		DPI_STATUS_OK = 0,

		DPI_STATUS_ERROR,
	} DPI_STATUS;

	typedef enum {
		DPI_POLARITY_RISING = 0,
		DPI_POLARITY_FALLING = 1
	} DPI_POLARITY;

	typedef enum {
		DPI_RGB_ORDER_RGB = 0,
		DPI_RGB_ORDER_BGR = 1,
	} DPI_RGB_ORDER;

	typedef enum {
		DPI_CLK_480p = 27027,
		DPI_CLK_720p = 74250,
		DPI_CLK_1080p = 148500
	} DPI_CLK_FREQ;

	typedef enum {
		DPI_VIDEO_720x480p_60Hz = 0,	/* 0 */
		DPI_VIDEO_720x576p_50Hz,	/* 1 */
		DPI_VIDEO_1280x720p_60Hz,	/* 2 */
		DPI_VIDEO_1280x720p_50Hz,	/* 3 */
		DPI_VIDEO_1920x1080i_60Hz,	/* 4 */
		DPI_VIDEO_1920x1080i_50Hz,	/* 5 */
		DPI_VIDEO_1920x1080p_30Hz,	/* 6 */
		DPI_VIDEO_1920x1080p_25Hz,	/* 7 */
		DPI_VIDEO_1920x1080p_24Hz,	/* 8 */
		DPI_VIDEO_1920x1080p_23Hz,	/* 9 */
		DPI_VIDEO_1920x1080p_29Hz,	/* a */
		DPI_VIDEO_1920x1080p_60Hz,	/* b */
		DPI_VIDEO_1920x1080p_50Hz,	/* c */

		DPI_VIDEO_1280x720p3d_60Hz,	/* d */
		DPI_VIDEO_1280x720p3d_50Hz,	/* e */
		DPI_VIDEO_1920x1080i3d_60Hz,	/* f */
		DPI_VIDEO_1920x1080i3d_50Hz,	/* 10 */
		DPI_VIDEO_1920x1080p3d_24Hz,	/* 11 */
		DPI_VIDEO_1920x1080p3d_23Hz,	/* 12 */

		/*the 2160 mean 3840x2160 */
		DPI_VIDEO_2160P_23_976HZ,	/* 13 */
		DPI_VIDEO_2160P_24HZ,	/* 14 */
		DPI_VIDEO_2160P_25HZ,	/* 15 */
		DPI_VIDEO_2160P_29_97HZ,	/* 16 */
		DPI_VIDEO_2160P_30HZ,	/* 17 */
		/*the 2161 mean 4096x2160 */
		DPI_VIDEO_2161P_24HZ,	/* 18 */

		DPI_VIDEO_RESOLUTION_NUM
	} DPI_VIDEO_RESOLUTION;

	typedef enum {
		RGB = 0,
		RGB_FULL,
		YCBCR_444,
		YCBCR_422,
		XV_YCC,
		YCBCR_444_FULL,
		YCBCR_422_FULL,
	} COLOR_SPACE_T;

	typedef struct {
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
	} LCD_REG_WROI_CON, *PLCD_REG_WROI_CON;

	int ddp_dpi_stop(DISP_MODULE_ENUM module, void *cmdq_handle);
	int ddp_dpi_power_on(DISP_MODULE_ENUM module, void *cmdq_handle);
	int ddp_dpi_power_off(DISP_MODULE_ENUM module, void *cmdq_handle);
	int ddp_dpi_dump(DISP_MODULE_ENUM module, int level);
	int ddp_dpi_start(DISP_MODULE_ENUM module, void *cmdq);
	int ddp_dpi_init(DISP_MODULE_ENUM module, void *cmdq);
	int ddp_dpi_deinit(DISP_MODULE_ENUM module, void *cmdq_handle);
	int ddp_dpi_config(DISP_MODULE_ENUM module, disp_ddp_path_config *config,
			   void *cmdq_handle);
	int ddp_dpi_trigger(DISP_MODULE_ENUM module, void *cmdq);
	int ddp_dpi_ioctl(DISP_MODULE_ENUM module, void *cmdq_handle, unsigned int ioctl_cmd,
			  unsigned long *params);
	DPI_STATUS ddp_dpi_EnableColorBar(DISP_MODULE_ENUM module);
	unsigned int ddp_dpi_get_cur_addr(bool rdma_mode, int layerid);

	DPI_STATUS ddp_dpi_3d_ctrl(DISP_MODULE_ENUM module, cmdqRecHandle cmdq, bool fg3DFrame);
	DPI_STATUS ddp_dpi_config_colorspace(DISP_MODULE_ENUM module, cmdqRecHandle cmdq,
					     UINT8 ColorSpace, UINT8 HDMI_Res);
	DPI_STATUS ddp_dpi_yuv422_setting(DISP_MODULE_ENUM module, cmdqRecHandle cmdq, UINT32 uvsw);
	DPI_STATUS ddp_dpi_clpf_setting(DISP_MODULE_ENUM module, cmdqRecHandle cmdq, UINT8 clpfType,
					bool roundingEnable, UINT32 clpfen);
	DPI_STATUS ddp_dpi_vsync_lr_enable(DISP_MODULE_ENUM module, cmdqRecHandle cmdq,
					   UINT32 vs_lo_en, UINT32 vs_le_en, UINT32 vs_ro_en,
					   UINT32 vs_re_en);
	DPI_STATUS ddp_dpi_ConfigVsync_LEVEN(DISP_MODULE_ENUM module, cmdqRecHandle cmdq,
					     UINT32 pulseWidth, UINT32 backPorch, UINT32 frontPorch,
					     bool fgInterlace);
	DPI_STATUS ddp_dpi_ConfigVsync_RODD(DISP_MODULE_ENUM module, cmdqRecHandle cmdq,
					    UINT32 pulseWidth, UINT32 backPorch, UINT32 frontPorch);
	DPI_STATUS ddp_dpi_ConfigVsync_REVEN(DISP_MODULE_ENUM module, cmdqRecHandle cmdq,
					     UINT32 pulseWidth, UINT32 backPorch, UINT32 frontPorch,
					     bool fgInterlace);
	void ddp_dpi_lvds_config(DISP_MODULE_ENUM module, LCM_DPI_PARAMS *dpi_config,
				 void *cmdq_handle);
	bool DPI0_IS_TOP_FIELD(void);

#ifdef __cplusplus
}
#endif
#endif				/* __DPI_DRV_H__ */
