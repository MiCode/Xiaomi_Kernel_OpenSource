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

#ifndef _DDP_OVL_H_
#define _DDP_OVL_H_
#include "ddp_hal.h"
#include "../videox/DpDataType.h"


#define OVL_MAX_WIDTH  (8191)
#define OVL_MAX_HEIGHT (4095)
#define OVL_LAYER_NUM  (4)

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
extern unsigned int primary_ovl0_handle;
extern int primary_ovl0_handle_size;
#endif

/* start overlay module */
int ovl_start(DISP_MODULE_ENUM module, void *handle);

/* stop overlay module */
int ovl_stop(DISP_MODULE_ENUM module, void *handle);

/* reset overlay module */
int ovl_reset(DISP_MODULE_ENUM module, void *handle);

/* set region of interest */
int ovl_roi(DISP_MODULE_ENUM module, unsigned int bgW, unsigned int bgH,	/* region size */
	    unsigned int bgColor,	/* border color */

	    void *handle);
unsigned int ddp_ovl_get_cur_addr(bool rdma_mode, int layerid);

/* switch layer on/off */
int ovl_layer_switch(DISP_MODULE_ENUM module, unsigned layer, unsigned int en, void *handle
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
		     , unsigned int sec
#endif
	);
/* get ovl input address */
void ovl_get_address(DISP_MODULE_ENUM module, unsigned long *add);

/* configure layer property */
int ovl_layer_config(DISP_MODULE_ENUM module,
		     unsigned layer,
		     unsigned source,
		     DpColorFormat format,
		     unsigned long addr,
		     unsigned int src_x,	/* ROI x offset */
		     unsigned int src_y,	/* ROI y offset */
		     unsigned int src_pitch, unsigned int dst_x,	/* ROI x offset */
		     unsigned int dst_y,	/* ROI y offset */
		     unsigned int dst_w,	/* ROT width */
		     unsigned int dst_h,	/* ROI height */
		     unsigned int keyEn, unsigned int key, unsigned int aen, unsigned char alpha,
		     unsigned int sur_aen, unsigned int src_alpha, unsigned int dst_alpha,
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
		     unsigned int sec, unsigned int is_engine_sec,
#endif
		     unsigned int yuv_range, void *handle);

int ovl_3d_config(DISP_MODULE_ENUM module,
		  unsigned int layer_id,
		  unsigned int en_3d, unsigned int landscape, unsigned int r_first, void *handle);

void ovl_dump_analysis(DISP_MODULE_ENUM module);
void ovl_dump_reg(DISP_MODULE_ENUM module);

void ovl_get_info(int idx, void *data);
#endif
