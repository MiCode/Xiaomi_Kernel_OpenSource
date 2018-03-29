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

#define LOG_TAG "OVL"
#include "ddp_log.h"

#include <linux/clk.h>
#include <linux/delay.h>
#include "ddp_info.h"
#include "ddp_hal.h"
#include "ddp_reg.h"
#include "ddp_ovl.h"
#include "primary_display.h"
#ifdef CONFIG_MTK_HDMI_SUPPORT
#include "extd_ddp.h"
#endif
#include "mtk_ovl.h"

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
#include "m4u_port.h"
#endif

#define OVL_NUM                   (2)
#define OVL_REG_BACK_MAX          (40)
#define OVL_LAYER_OFF_SITE        (0x20)
#define OVL_RDMA_DEBUG_OFF_SITE   (0x4)

enum OVL_COLOR_SPACE {
	OVL_COLOR_SPACE_RGB = 0,
	OVL_COLOR_SPACE_YUV,
};

enum OVL_INPUT_FORMAT {
	OVL_INPUT_FORMAT_BGR565 = 0,
	OVL_INPUT_FORMAT_RGB888 = 1,
	OVL_INPUT_FORMAT_RGBA8888 = 2,
	OVL_INPUT_FORMAT_ARGB8888 = 3,
	OVL_INPUT_FORMAT_VYUY = 4,
	OVL_INPUT_FORMAT_YVYU = 5,

	OVL_INPUT_FORMAT_RGB565 = 6,
	OVL_INPUT_FORMAT_BGR888 = 7,
	OVL_INPUT_FORMAT_BGRA8888 = 8,
	OVL_INPUT_FORMAT_ABGR8888 = 9,
	OVL_INPUT_FORMAT_UYVY = 10,
	OVL_INPUT_FORMAT_YUYV = 11,

	OVL_INPUT_FORMAT_UNKNOWN = 32,
};

struct OVL_REG {
	unsigned long address;
	unsigned int value;
};

static int reg_back_cnt[OVL_NUM];
static struct OVL_REG reg_back[OVL_NUM][OVL_REG_BACK_MAX];

static enum OVL_INPUT_FORMAT ovl_input_fmt_convert(DpColorFormat fmt)
{
	enum OVL_INPUT_FORMAT ovl_fmt = OVL_INPUT_FORMAT_UNKNOWN;

	switch (fmt) {
	case eBGR565:
		ovl_fmt = OVL_INPUT_FORMAT_BGR565;
		break;
	case eRGB565:
		ovl_fmt = OVL_INPUT_FORMAT_RGB565;
		break;
	case eRGB888:
		ovl_fmt = OVL_INPUT_FORMAT_RGB888;
		break;
	case eBGR888:
		ovl_fmt = OVL_INPUT_FORMAT_BGR888;
		break;
	case eRGBA8888:
		ovl_fmt = OVL_INPUT_FORMAT_RGBA8888;
		break;
	case eBGRA8888:
		ovl_fmt = OVL_INPUT_FORMAT_BGRA8888;
		break;
	case eARGB8888:
		ovl_fmt = OVL_INPUT_FORMAT_ARGB8888;
		break;
	case eABGR8888:
		ovl_fmt = OVL_INPUT_FORMAT_ABGR8888;
		break;
	case eVYUY:
		ovl_fmt = OVL_INPUT_FORMAT_VYUY;
		break;
	case eYVYU:
		ovl_fmt = OVL_INPUT_FORMAT_YVYU;
		break;
	case eUYVY:
		ovl_fmt = OVL_INPUT_FORMAT_UYVY;
		break;
	case eYUY2:
		ovl_fmt = OVL_INPUT_FORMAT_YUYV;
		break;
	default:
		DDPERR("ovl_fmt_convert fmt=%d, ovl_fmt=%d\n", fmt, ovl_fmt);
		break;
	}
	return ovl_fmt;
}

static DpColorFormat ovl_input_fmt(enum OVL_INPUT_FORMAT fmt, int swap)
{
	switch (fmt) {
	case OVL_INPUT_FORMAT_BGR565:
		return swap ? eBGR565 : eRGB565;
	case OVL_INPUT_FORMAT_RGB888:
		return swap ? eRGB888 : eBGR888;
	case OVL_INPUT_FORMAT_RGBA8888:
		return swap ? eRGBA8888 : eBGRA8888;
	case OVL_INPUT_FORMAT_ARGB8888:
		return swap ? eARGB8888 : eABGR8888;
	case OVL_INPUT_FORMAT_VYUY:
		return swap ? eVYUY : eUYVY;
	case OVL_INPUT_FORMAT_YVYU:
		return swap ? eYVYU : eYUY2;
	default:
		DDPERR("ovl_input_fmt fmt=%d, swap=%d\n", fmt, swap);
		break;
	}
	return eRGB888;
}

static unsigned int ovl_input_fmt_byte_swap(enum OVL_INPUT_FORMAT fmt)
{
	int input_swap = 0;

	switch (fmt) {
	case OVL_INPUT_FORMAT_BGR565:
	case OVL_INPUT_FORMAT_RGB888:
	case OVL_INPUT_FORMAT_RGBA8888:
	case OVL_INPUT_FORMAT_ARGB8888:
	case OVL_INPUT_FORMAT_VYUY:
	case OVL_INPUT_FORMAT_YVYU:
		input_swap = 1;
		break;
	case OVL_INPUT_FORMAT_RGB565:
	case OVL_INPUT_FORMAT_BGR888:
	case OVL_INPUT_FORMAT_BGRA8888:
	case OVL_INPUT_FORMAT_ABGR8888:
	case OVL_INPUT_FORMAT_UYVY:
	case OVL_INPUT_FORMAT_YUYV:
		input_swap = 0;
		break;
	default:
		DDPERR("unknown input ovl format is %d\n", fmt);
		ASSERT(0);
	}
	return input_swap;
}

static unsigned int ovl_input_fmt_bpp(enum OVL_INPUT_FORMAT fmt)
{
	int bpp = 0;

	switch (fmt) {
	case OVL_INPUT_FORMAT_BGR565:
	case OVL_INPUT_FORMAT_RGB565:
	case OVL_INPUT_FORMAT_VYUY:
	case OVL_INPUT_FORMAT_UYVY:
	case OVL_INPUT_FORMAT_YVYU:
	case OVL_INPUT_FORMAT_YUYV:
		bpp = 2;
		break;
	case OVL_INPUT_FORMAT_RGB888:
	case OVL_INPUT_FORMAT_BGR888:
		bpp = 3;
		break;
	case OVL_INPUT_FORMAT_RGBA8888:
	case OVL_INPUT_FORMAT_BGRA8888:
	case OVL_INPUT_FORMAT_ARGB8888:
	case OVL_INPUT_FORMAT_ABGR8888:
		bpp = 4;
		break;
	default:
		DDPERR("unknown ovl input format = %d\n", fmt);
		ASSERT(0);
	}
	return bpp;
}

static enum OVL_COLOR_SPACE ovl_input_fmt_color_space(enum OVL_INPUT_FORMAT fmt)
{
	enum OVL_COLOR_SPACE space = OVL_COLOR_SPACE_RGB;

	switch (fmt) {
	case OVL_INPUT_FORMAT_BGR565:
	case OVL_INPUT_FORMAT_RGB565:
	case OVL_INPUT_FORMAT_RGB888:
	case OVL_INPUT_FORMAT_BGR888:
	case OVL_INPUT_FORMAT_RGBA8888:
	case OVL_INPUT_FORMAT_BGRA8888:
	case OVL_INPUT_FORMAT_ARGB8888:
	case OVL_INPUT_FORMAT_ABGR8888:
		space = OVL_COLOR_SPACE_RGB;
		break;
	case OVL_INPUT_FORMAT_VYUY:
	case OVL_INPUT_FORMAT_UYVY:
	case OVL_INPUT_FORMAT_YVYU:
	case OVL_INPUT_FORMAT_YUYV:
		space = OVL_COLOR_SPACE_YUV;
		break;
	default:
		DDPERR("unknown ovl input format = %d\n", fmt);
		ASSERT(0);
	}
	return space;
}

static unsigned int ovl_input_fmt_reg_value(enum OVL_INPUT_FORMAT fmt)
{
	int reg_value = 0;

	switch (fmt) {
	case OVL_INPUT_FORMAT_BGR565:
	case OVL_INPUT_FORMAT_RGB565:
		reg_value = 0x0;
		break;
	case OVL_INPUT_FORMAT_RGB888:
	case OVL_INPUT_FORMAT_BGR888:
		reg_value = 0x1;
		break;
	case OVL_INPUT_FORMAT_RGBA8888:
	case OVL_INPUT_FORMAT_BGRA8888:
		reg_value = 0x2;
		break;
	case OVL_INPUT_FORMAT_ARGB8888:
	case OVL_INPUT_FORMAT_ABGR8888:
		reg_value = 0x3;
		break;
	case OVL_INPUT_FORMAT_VYUY:
	case OVL_INPUT_FORMAT_UYVY:
		reg_value = 0x4;
		break;
	case OVL_INPUT_FORMAT_YVYU:
	case OVL_INPUT_FORMAT_YUYV:
		reg_value = 0x5;
		break;
	default:
		DDPERR("unknown ovl input format is %d\n", fmt);
		ASSERT(0);
	}
	return reg_value;
}

static char *ovl_intput_format_name(enum OVL_INPUT_FORMAT fmt, int swap)
{
	switch (fmt) {
	case OVL_INPUT_FORMAT_BGR565:
		return swap ? "eBGR565" : "eRGB565";
	case OVL_INPUT_FORMAT_RGB565:
		return "eRGB565";
	case OVL_INPUT_FORMAT_RGB888:
		return swap ? "eRGB888" : "eBGR888";
	case OVL_INPUT_FORMAT_BGR888:
		return "eBGR888";
	case OVL_INPUT_FORMAT_RGBA8888:
		return swap ? "eRGBA8888" : "eBGRA8888";
	case OVL_INPUT_FORMAT_BGRA8888:
		return "eBGRA8888";
	case OVL_INPUT_FORMAT_ARGB8888:
		return swap ? "eARGB8888" : "eABGR8888";
	case OVL_INPUT_FORMAT_ABGR8888:
		return "eABGR8888";
	case OVL_INPUT_FORMAT_VYUY:
		return swap ? "eVYUY" : "eUYVY";
	case OVL_INPUT_FORMAT_UYVY:
		return "eUYVY";
	case OVL_INPUT_FORMAT_YVYU:
		return swap ? "eYVYU" : "eYUY2";
	case OVL_INPUT_FORMAT_YUYV:
		return "eYUY2";
	default:
		DDPERR("ovl_intput_fmt unknown fmt=%d, swap=%d\n", fmt, swap);
		break;
	}
	return "unknown";
}

static unsigned int ovl_index(DISP_MODULE_ENUM module)
{
	int idx = 0;

	switch (module) {
	case DISP_MODULE_OVL0:
		idx = 0;
		break;
	case DISP_MODULE_OVL1:
		idx = 1;
		break;
	default:
		DDPERR("invalid module=%d\n", module);
		ASSERT(0);
	}
	return idx;
}

int ovl_start(DISP_MODULE_ENUM module, void *handle)
{
	int idx = ovl_index(module);
	int idx_off_site = idx * DISP_OVL_INDEX_OFFSET;

	DISP_REG_SET(handle, idx_off_site + DISP_REG_OVL_EN, 0x01);
	DISP_REG_SET(handle, idx_off_site + DISP_REG_OVL_INTEN, 0x1FFE);
	DISP_REG_SET_FIELD(handle, DATAPATH_CON_FLD_LAYER_SMI_ID_EN,
			   idx_off_site + DISP_REG_OVL_DATAPATH_CON, 0x1);
	return 0;
}

int ovl_stop(DISP_MODULE_ENUM module, void *handle)
{
	int i;
	int idx = ovl_index(module);
	int idx_off_site = idx * DISP_OVL_INDEX_OFFSET;

	DISP_REG_SET(handle, idx_off_site + DISP_REG_OVL_INTEN, 0x00);
	DISP_REG_SET(handle, idx_off_site + DISP_REG_OVL_EN, 0x00);
	DISP_REG_SET(handle, idx_off_site + DISP_REG_OVL_INTSTA, 0x00);

	for (i = 0; i < 4; i++)
		ovl_layer_switch(module, i, 0, handle
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
				 , 0
#endif
		    );



	return 0;
}

int ovl_reset(DISP_MODULE_ENUM module, void *handle)
{
#define OVL_IDLE (0x1)
	int ret = 0;
	unsigned int delay_cnt = 0;
	int idx = ovl_index(module);
	int idx_off_site = idx * DISP_OVL_INDEX_OFFSET;

	DISP_CPU_REG_SET_FIELD(MMSYS_HW_DCM_OVL_SET0_FLD_DCM, DISP_REG_CONFIG_MMSYS_HW_DCM_DIS_SET0, 0x3);
	DISP_CPU_REG_SET(idx_off_site + DISP_REG_OVL_RST, 0x1);
	DISP_CPU_REG_SET(idx_off_site + DISP_REG_OVL_RST, 0x0);
	/*only wait if not cmdq */
	if (handle == NULL) {
		while (!(DISP_REG_GET(idx_off_site + DISP_REG_OVL_FLOW_CTRL_DBG) & OVL_IDLE)) {
			delay_cnt++;
			udelay(10);
			if (delay_cnt > 2000) {
				DDPERR("ovl%d_reset timeout!\n", idx);
				ret = -1;
				break;
			}
		}
	}
	DISP_CPU_REG_SET_FIELD(MMSYS_HW_DCM_OVL_CLR0_FLD_DCM, DISP_REG_CONFIG_MMSYS_HW_DCM_DIS_CLR0, 0x3);
	return ret;
}

int ovl_roi(DISP_MODULE_ENUM module,
	    unsigned int bg_w, unsigned int bg_h, unsigned int bg_color, void *handle)
{
	int idx = ovl_index(module);
	int idx_off_site = idx * DISP_OVL_INDEX_OFFSET;

	if ((bg_w > OVL_MAX_WIDTH) || (bg_h > OVL_MAX_HEIGHT)) {
		DDPERR("ovl_roi,exceed OVL max size, w=%d, h=%d\n", bg_w, bg_h);
		ASSERT(0);
	}

	DISP_REG_SET(handle, idx_off_site + DISP_REG_OVL_ROI_SIZE, bg_h << 16 | bg_w);

	DISP_REG_SET(handle, idx_off_site + DISP_REG_OVL_ROI_BGCLR, bg_color);

	return 0;
}

int ovl_layer_switch(DISP_MODULE_ENUM module, unsigned layer, unsigned int en, void *handle
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
		     , unsigned int sec
#endif
	)
{
	int idx = ovl_index(module);
	int idx_off_site = idx * DISP_OVL_INDEX_OFFSET;

	ASSERT(layer <= 3);
	DDPDBG("ovl%d,layer %d,enable %d\n", idx, layer, en);
	switch (layer) {
	case 0:
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
		/* for CMDQ TZ to determine whether shelter HDMI secure video layer by HDCP status */
		if ((handle != 0) && (idx == 1) && en && sec && !ovl2mem_is_alive()) {
			cmdqRecWriteSecureMask(handle,
					       ddp_addr_convert_va2pa(idx_off_site +
								      DISP_REG_OVL_SRC_CON),
					       CMDQ_SAM_DDP_REG_HDCP,
					       en << REG_FLD_SHIFT(SRC_CON_FLD_L0_EN),
					       REG_FLD_MASK(SRC_CON_FLD_L0_EN));
			cmdqRecWriteSecureMask(handle,
					       ddp_addr_convert_va2pa(idx_off_site +
								      DISP_REG_OVL_RDMA0_CTRL),
					       CMDQ_SAM_DDP_REG_HDCP, en, 0xffffffff);
		} else
#endif
		{
			DISP_REG_SET_FIELD(handle, SRC_CON_FLD_L0_EN,
					   idx_off_site + DISP_REG_OVL_SRC_CON, en);
			DISP_REG_SET(handle, idx_off_site + DISP_REG_OVL_RDMA0_CTRL, en);
		}
		break;
	case 1:
		DISP_REG_SET_FIELD(handle, SRC_CON_FLD_L1_EN,
				   idx_off_site + DISP_REG_OVL_SRC_CON, en);
		DISP_REG_SET(handle, idx_off_site + DISP_REG_OVL_RDMA1_CTRL, en);
		break;
	case 2:
		DISP_REG_SET_FIELD(handle, SRC_CON_FLD_L2_EN,
				   idx_off_site + DISP_REG_OVL_SRC_CON, en);
		DISP_REG_SET(handle, idx_off_site + DISP_REG_OVL_RDMA2_CTRL, en);
		break;
	case 3:
		DISP_REG_SET_FIELD(handle, SRC_CON_FLD_L3_EN,
				   idx_off_site + DISP_REG_OVL_SRC_CON, en);
		DISP_REG_SET(handle, idx_off_site + DISP_REG_OVL_RDMA3_CTRL, en);
		break;
	default:
		DDPERR("invalid layer=%d\n", layer);
		ASSERT(0);
	}

	return 0;
}

int ovl_layer_config(DISP_MODULE_ENUM module, unsigned int layer, unsigned int source, DpColorFormat format,
		     unsigned long addr,
		     unsigned int src_x,	/* ROI x offset */
		     unsigned int src_y,	/* ROI y offset */
		     unsigned int src_pitch, unsigned int dst_x,	/* ROI x offset */
		     unsigned int dst_y,	/* ROI y offset */
		     unsigned int dst_w,	/* ROT width */
		     unsigned int dst_h,	/* ROI height */
		     unsigned int key_en, unsigned int key,	/*color key */
		     unsigned int aen,	/* alpha enable */
		     unsigned char alpha,
		     unsigned int sur_aen, unsigned int src_alpha, unsigned int dst_alpha,
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
		     unsigned int sec, unsigned int is_engine_sec,
#endif
		     unsigned int yuv_range, void *handle)
{

	int idx = ovl_index(module);
	unsigned int value = 0;
	enum OVL_INPUT_FORMAT fmt = ovl_input_fmt_convert(format);
	unsigned int bpp = ovl_input_fmt_bpp(fmt);
	unsigned int input_swap = ovl_input_fmt_byte_swap(fmt);
	unsigned int input_fmt = ovl_input_fmt_reg_value(fmt);
	enum OVL_COLOR_SPACE space = ovl_input_fmt_color_space(fmt);

	/*0100 MTX_JPEG_TO_RGB (YUV FUll TO RGB) */
	int color_matrix = 0x4;

	unsigned int idx_off_site = idx * DISP_OVL_INDEX_OFFSET;
	unsigned int layer_off_site = idx_off_site + layer * OVL_LAYER_OFF_SITE;

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	unsigned int offset = 0;
	unsigned int layer_offset = layer_off_site;
#endif

	ASSERT((dst_w <= OVL_MAX_WIDTH) && (dst_h <= OVL_MAX_HEIGHT) && (layer <= 3));

	if (addr == 0) {
		DDPERR("source from memory, but addr is 0!\n");
		ASSERT(0);
	}
	/* DDPDEBUG_D("O%d L%d ad=0x%x offst(x=%d y=%d) dst(%d %d %d %d) p=%d " */
	/* "fmt=0x%x\n", */
	/* idx, layer, addr, src_x, src_y, dst_x, dst_y, dst_w, dst_h, */
	/* src_pitch, format); */

	switch (yuv_range) {
	case 0:
		color_matrix = 0x4;	/* JPEG_TO_RGB limited -> full */
		break;
	case 1:
		color_matrix = 0x6;	/* BT601_TO_RGB limited -> full */
		break;
	case 2:
		color_matrix = 0x7;	/* BT709_TO_RGB limited -> full */
		break;
	default:
		color_matrix = 0x4;	/* JPEG_TO_RGB */
		break;
	}

	DDPDBG(
		"ovl%d, layer=%d, off(x=%d, y=%d), dst(%d, %d, %d, %d),pitch=%d,color_matrix=%d, fmt=%s, addr=%lu, keyEn=%d, key=%d, aen=%d, alpha=%d,sur_aen=%d,sur_alpha=0x%x\n",
	       idx,
	       layer,
	       src_x,
	       src_y,
	       dst_x,
	       dst_y,
	       dst_w,
	       dst_h,
	       src_pitch,
	       color_matrix,
	       ovl_intput_format_name(fmt, input_swap),
	       addr, key_en, key, aen, alpha, sur_aen, dst_alpha << 2 | src_alpha);

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	DDPDBG("[SVP] sec=%d,ovlsec=%d\n", sec, is_engine_sec);
#endif

	DISP_REG_SET(handle, DISP_REG_OVL_RDMA0_CTRL + layer_off_site, 0x1);

	value = (REG_FLD_VAL((L_CON_FLD_LARC), (0)) |
		 REG_FLD_VAL((L_CON_FLD_CFMT), (input_fmt)) |
		 REG_FLD_VAL((L_CON_FLD_AEN), (aen)) |
		 REG_FLD_VAL((L_CON_FLD_APHA), (alpha)) |
		 REG_FLD_VAL((L_CON_FLD_SKEN), (key_en)) |
		 REG_FLD_VAL((L_CON_FLD_BTSW), (input_swap)));

	if (space == OVL_COLOR_SPACE_YUV)
		value = value | REG_FLD_VAL((L_CON_FLD_MTX), (color_matrix));

	DISP_REG_SET(handle, DISP_REG_OVL_L0_CON + layer_off_site, value);

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	offset = src_x * bpp + src_y * src_pitch;
	if (!is_engine_sec) {
		DISP_REG_SET(handle, DISP_REG_OVL_L0_ADDR + layer_offset, addr + offset);
	} else {
		unsigned int size;
		int m4u_port;

		size = (dst_h - 1) * src_pitch + dst_w * bpp;
		m4u_port = idx == 0 ? M4U_PORT_DISP_OVL0 : M4U_PORT_DISP_OVL1;
		if (!sec) {
			/* ovl is sec but this layer is non-sec */
			/* we need to tell cmdq to help map non-sec mva to sec mva */
			cmdqRecWriteSecure(handle,
					   disp_addr_convert(DISP_REG_OVL_L0_ADDR + layer_offset),
					   CMDQ_SAM_NMVA_2_MVA, addr + offset, 0, size, m4u_port);

		} else {
			if (gDebugSvp == 0)
				/* for sec layer, addr variable stores sec handle */
				/* we need to pass this handle and offset to cmdq driver */
				/* cmdq sec driver will help to convert handle to correct address */
				cmdqRecWriteSecure(handle,
						   disp_addr_convert(DISP_REG_OVL_L0_ADDR +
								     layer_offset),
						   CMDQ_SAM_H_2_MVA, addr, offset, size, m4u_port);
			else if (gDebugSvp == 2 && gDebugSvpOption == 0) {
				/* test normal buffer map to secure MVA */
				DDPMSG("[SVP] test : ovl CMDQ_SAM_NMVA_2_MVA\n");
				cmdqRecWriteSecure(handle,
						   disp_addr_convert(DISP_REG_OVL_L0_ADDR +
								     layer_offset),
						   CMDQ_SAM_NMVA_2_MVA, addr + offset, 0, size,
						   m4u_port);
			} else if (gDebugSvp == 2 && gDebugSvpOption == 1)
				/* test normal buffer replace with internal secure handle */
				cmdqRecWriteSecure(handle,
						   disp_addr_convert(DISP_REG_OVL_L0_ADDR +
								     layer_offset),
						   CMDQ_SAM_H_2_MVA, primary_ovl0_handle, offset,
						   primary_ovl0_handle_size, m4u_port);
			else if (gDebugSvp == 1)
				/* test secure buffer replace with internal secure handle */
				cmdqRecWriteSecure(handle,
						   disp_addr_convert(DISP_REG_OVL_L0_ADDR +
								     layer_offset),
						   CMDQ_SAM_H_2_MVA, primary_ovl0_handle, offset,
						   primary_ovl0_handle_size, m4u_port);
		}
	}
#else
	DISP_REG_SET(handle, DISP_REG_OVL_L0_ADDR + layer_off_site,
		     addr + src_x * bpp + src_y * src_pitch);
#endif
	DISP_REG_SET(handle, DISP_REG_OVL_L0_SRCKEY + layer_off_site, key);
	DISP_REG_SET(handle, DISP_REG_OVL_L0_SRC_SIZE + layer_off_site, dst_h << 16 | dst_w);
	DISP_REG_SET(handle, DISP_REG_OVL_L0_OFFSET + layer_off_site, dst_y << 16 | dst_x);

	value = (((sur_aen & 0x1) << 15) |
		 ((dst_alpha & 0x3) << 6) | ((dst_alpha & 0x3) << 4) |
		 ((src_alpha & 0x3) << 2) | (src_alpha & 0x3));

	value = (REG_FLD_VAL((L_PITCH_FLD_SUR_ALFA), (value)) |
		 REG_FLD_VAL((L_PITCH_FLD_LSP), (src_pitch)));

	DISP_REG_SET(handle, DISP_REG_OVL_L0_PITCH + layer_off_site, value);
	return 0;
}

static void ovl_store_regs(DISP_MODULE_ENUM module)
{
	int i = 0;
	int idx = ovl_index(module);
	unsigned int idx_off_site = idx * DISP_OVL_INDEX_OFFSET;
	unsigned long regs[2];

	regs[0] = DISP_REG_OVL_ROI_SIZE + idx_off_site;
	regs[1] = DISP_REG_OVL_ROI_BGCLR + idx_off_site;
	/*Enable SMI_LARB0 MMU, After use CCF, M4U can not set this register when resume */
	/* regs[2] = DISPSYS_SMI_LARB0_BASE + 0xF00; */

	reg_back_cnt[idx] = sizeof(regs) / sizeof(unsigned long);
	ASSERT(reg_back_cnt[idx] <= OVL_REG_BACK_MAX);


	for (i = 0; i < reg_back_cnt[idx]; i++) {
		reg_back[idx][i].address = regs[i];
		reg_back[idx][i].value = DISP_REG_GET(regs[i]);
	}
	DDPDBG("store %d cnt registers on ovl %d\n", reg_back_cnt[idx], idx);

}

static void ovl_restore_regs(DISP_MODULE_ENUM module, void *handle)
{
	int idx = ovl_index(module);
	int i = reg_back_cnt[idx];

	while (i > 0) {
		i--;
		DISP_REG_SET(handle, reg_back[idx][i].address, reg_back[idx][i].value);
	}
	DDPDBG("restore %d cnt registers on ovl %d\n", reg_back_cnt[idx], idx);
	reg_back_cnt[idx] = 0;
}

int ovl_clock_on(DISP_MODULE_ENUM module, void *handle)
{
	int idx = ovl_index(module);

	DDPDBG("ovl%d_clock_on\n", idx);
	ddp_module_clock_enable(MM_CLK_DISP_OVL0 + idx, true);
	ovl_restore_regs(module, handle);
	return 0;
}

int ovl_clock_off(DISP_MODULE_ENUM module, void *handle)
{
	int idx = ovl_index(module);

	DDPDBG("ovl%d_clock_off\n", idx);
	ovl_store_regs(module);
	ddp_module_clock_enable(MM_CLK_DISP_OVL0 + idx, false);
	return 0;
}

int ovl_resume(DISP_MODULE_ENUM module, void *handle)
{
	int idx = ovl_index(module);

	DDPMSG("ovl%d_resume\n", idx);
	ddp_module_clock_enable(MM_CLK_DISP_OVL0 + idx, true);
	ovl_restore_regs(module, handle);
	return 0;
}

int ovl_suspend(DISP_MODULE_ENUM module, void *handle)
{
	int idx = ovl_index(module);

	DDPMSG("ovl%d_suspend\n", idx);
	ovl_store_regs(module);
	ddp_module_clock_enable(MM_CLK_DISP_OVL0 + idx, false);
	return 0;
}

int ovl_init(DISP_MODULE_ENUM module, void *handle)
{
	int idx = ovl_index(module);

	ddp_module_clock_enable(MM_CLK_DISP_OVL0 + idx, true);
	DDPDBG("olv%d_init open CG 0x%x\n", idx, DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON0));
	return 0;
}

int ovl_deinit(DISP_MODULE_ENUM module, void *handle)
{
	int idx = ovl_index(module);

	DDPDBG("ovl%d_deinit\n", idx);
	ddp_module_clock_enable(MM_CLK_DISP_OVL0 + idx, false);
	return 0;
}

unsigned int ddp_ovl_get_cur_addr(bool rdma_mode, int layerid)
{
	if (rdma_mode)
		return DISP_REG_GET(DISP_REG_RDMA_MEM_START_ADDR);

	if (DISP_REG_GET(DISP_REG_OVL_RDMA0_CTRL + layerid * 0x20) & 0x1)
		return DISP_REG_GET(DISP_REG_OVL_L0_ADDR + layerid * 0x20);

	return 0;
}

void ovl_get_address(DISP_MODULE_ENUM module, unsigned long *add)
{
	int i = 0;
	int idx = ovl_index(module);
	unsigned int idx_off_site = idx * DISP_OVL_INDEX_OFFSET;
	unsigned int layer_off = 0;
	unsigned int src_on = DISP_REG_GET(DISP_REG_OVL_SRC_CON + idx_off_site);

	for (i = 0; i < 4; i++) {
		layer_off = i * OVL_LAYER_OFF_SITE + idx_off_site;
		if (src_on & (0x1 << i))
			add[i] = DISP_REG_GET(layer_off + DISP_REG_OVL_L0_ADDR);
		else
			add[i] = 0;
	}

}

void ovl_get_info(int idx, void *data)
{
	int i = 0;
	OVL_BASIC_STRUCT *pdata = (OVL_BASIC_STRUCT *) data;
	unsigned int idx_off_site = idx * DISP_OVL_INDEX_OFFSET;
	unsigned int layer_off = 0;
	unsigned int src_on = DISP_REG_GET(DISP_REG_OVL_SRC_CON + idx_off_site);
	OVL_BASIC_STRUCT *p = NULL;

	for (i = 0; i < 4; i++) {
		layer_off = i * OVL_LAYER_OFF_SITE + idx_off_site;
		p = &pdata[i];
		p->layer = i;
		p->layer_en = src_on & (0x1 << i);
		if (p->layer_en) {
			p->fmt = (unsigned int)ovl_input_fmt(DISP_REG_GET_FIELD(L_CON_FLD_CFMT,
										layer_off +
										DISP_REG_OVL_L0_CON),
							     DISP_REG_GET_FIELD(L_CON_FLD_BTSW,
										layer_off +
										DISP_REG_OVL_L0_CON));
			p->addr = DISP_REG_GET(layer_off + DISP_REG_OVL_L0_ADDR);
			p->src_w = DISP_REG_GET(layer_off + DISP_REG_OVL_L0_SRC_SIZE) & 0xfff;
			p->src_h =
			    (DISP_REG_GET(layer_off + DISP_REG_OVL_L0_SRC_SIZE) >> 16) & 0xfff;
			p->src_pitch = DISP_REG_GET(layer_off + DISP_REG_OVL_L0_PITCH) & 0xffff;
			p->bpp = ovl_input_fmt_bpp(DISP_REG_GET_FIELD(L_CON_FLD_CFMT,
								      layer_off +
								      DISP_REG_OVL_L0_CON));
		}
		DDPDBG("ovl_get_info:layer%d,en %d,w %d,h %d,bpp %d,addr %lu\n",
		       i, p->layer_en, p->src_w, p->src_h, p->bpp, p->addr);
	}
}

static int ovl_check_input_param(OVL_CONFIG_STRUCT *config)
{
	if (config->addr == 0 || config->dst_w == 0 || config->dst_h == 0) {
		DDPERR("ovl parameter invalidate, addr=%lu, w=%d, h=%d\n",
		       config->addr, config->dst_w, config->dst_h);
		return -1;
	}

	return 0;
}

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
static void ovl_cmdq_insert_wait_frame_done_token_mira(void *handle, int ovl_idx)
{
	cmdqRecWait(handle, ovl_idx + CMDQ_EVENT_MUTEX0_STREAM_EOF);
}
#endif

static int ovl_check_should_reset(DISP_MODULE_ENUM module)
{
	int ovl_idx = ovl_index(module);

	if (ovl_idx == 0) {
		if ((primary_display_is_video_mode() == 0
		     && primary_display_is_decouple_mode() == 0)
		    || primary_display_is_decouple_mode() == 1)
			return 1;
	} else if (ovl_idx == 1) {
#ifdef CONFIG_MTK_HDMI_SUPPORT
		if ((ext_disp_is_decouple_mode() == 1) || (ovl2mem_is_alive() == 1))
			return 1;
#else
		if (ovl2mem_is_alive() == 1)
			return 1;
#endif
	}

	return 0;
}

static int ovl_config_l(DISP_MODULE_ENUM module, disp_ddp_path_config *pConfig, void *handle)
{
	int i = 0;
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	enum CMDQ_DISP_MODE mode = CMDQ_DISP_NON_SUPPORTED_MODE;
	unsigned int layer_min = 0;
	unsigned int layer_max = 4;
	int has_sec_layer = 0;
	int ovl_idx = ovl_index(module);
	enum CMDQ_ENG_ENUM cmdq_engine;
	static int ovl_is_sec[2];
#endif

	/* warm reset ovl every time we use it */
	if (handle) {
		unsigned int offset;

		offset = ovl_index(module) * DISP_OVL_INDEX_OFFSET;
		if (ovl_check_should_reset(module)) {
			DDPDBG("warm reset ovl%d every time we use it\n", ovl_index(module));
			DISP_REG_SET(handle, DISP_REG_OVL_RST + offset, 0x1);
			DISP_REG_SET(handle, DISP_REG_OVL_RST + offset, 0x0);
			cmdqRecPoll(handle, disp_addr_convert(DISP_REG_OVL_STA + offset), 0, 0x1);
		}
	}

	if (pConfig->dst_dirty)
		ovl_roi(module, pConfig->dst_w, pConfig->dst_h, 0xFF000000, handle);

	if (!pConfig->ovl_dirty)
		return 0;

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	/* check if we has sec layer */
	for (i = layer_min; i < layer_max; i++) {
		if (pConfig->ovl_config[i].layer_en &&
		    (pConfig->ovl_config[i].security == DISP_SECURE_BUFFER))
			has_sec_layer = 1;
	}

	if (!ovl_idx)
		cmdq_engine = CMDQ_ENG_DISP_OVL0;
	else
		cmdq_engine = CMDQ_ENG_DISP_OVL1;

	if (has_sec_layer) {
		cmdqRecSetSecure(handle, 1);
		if ((ovl_idx == 0 && !primary_display_is_decouple_mode())
#ifdef CONFIG_MTK_HDMI_SUPPORT
		    || (ovl_idx == 1 && ext_disp_is_alive() && !ext_disp_is_decouple_mode())
#endif
		    )
			mode = CMDQ_DISP_VIDEO_MODE;
		cmdqRecSetSecureMode(handle, mode);

		/* set engine as sec */
		cmdqRecSecureEnablePortSecurity(handle, (1LL << cmdq_engine));
		/* cmdqRecSecureEnableDAPC(handle, (1LL << cmdq_engine)); */
		if (ovl_is_sec[ovl_idx] == 0) {
			DDPMSG("[SVP] switch ovl%d to sec\n", ovl_idx);
#ifdef CONFIG_MTK_HDMI_SUPPORT
			g_wdma_rdma_security = DISP_SECURE_BUFFER;
#endif
			/* disable ovl when switch to sec, avoid m4u translation fault */
			for (i = 0; i < 4; i++)
				ovl_layer_switch(module, i, 0, handle
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
						 , 0
#endif
				    );

		}
		ovl_is_sec[ovl_idx] = 1;
	} else {
		if (ovl_is_sec[ovl_idx] == 1) {
			/* ovl is in sec stat, we need to switch it to nonsec */
			cmdqRecHandle nonsec_switch_handle = NULL;
			int ret;
			enum CMDQ_SCENARIO_ENUM cmdq_scenario =
			    CMDQ_SCENARIO_DISP_PRIMARY_DISABLE_SECURE_PATH;

			if (ovl_idx == 0)
				cmdq_scenario = CMDQ_SCENARIO_DISP_PRIMARY_DISABLE_SECURE_PATH;
			else if (ovl_idx == 1)
				cmdq_scenario = CMDQ_SCENARIO_DISP_SUB_DISABLE_SECURE_PATH;
			ret = cmdqRecCreate(cmdq_scenario, &(nonsec_switch_handle));
			if (ret)
				DDPAEE("[SVP]fail to create disable handle %s ret=%d\n", __func__,
				       ret);

			cmdqRecReset(nonsec_switch_handle);
			if (ovl_idx == 0 && !primary_display_is_decouple_mode()) {
				ovl_cmdq_insert_wait_frame_done_token_mira(nonsec_switch_handle,
									   ovl_idx);
				mode = CMDQ_DISP_VIDEO_MODE;
			}
#ifdef CONFIG_MTK_HDMI_SUPPORT
			if (ovl_idx == 1 && ext_disp_is_alive() && !ext_disp_is_decouple_mode()) {
				ovl_cmdq_insert_wait_frame_done_token_mira(nonsec_switch_handle,
									   ext_disp_get_mutex_id());
				mode = CMDQ_DISP_VIDEO_MODE;
			}
#endif
			cmdqRecSetSecure(nonsec_switch_handle, 1);
			cmdqRecSetSecureMode(nonsec_switch_handle, mode);

			/* we should disable ovl before new (nonsec) setting take effect
			 * or translation fault may happen,
			 * if we switch ovl to nonsec BUT its setting is still sec */
			/* for (i = 0; i < 4; i++) */
			/* ovl_layer_switch(module, i, 0, nonsec_switch_handle, 0); */
			/* add nonsec map to sec for fixing m4u tanslation falut */
			for (i = 0; i < OVL_LAYER_NUM; i++) {
				if (pConfig->ovl_config[i].layer_en != 0) {
					if (ovl_check_input_param(&pConfig->ovl_config[i]))
						continue;
					ovl_layer_config(module,
							 i,
							 pConfig->ovl_config[i].source,
							 pConfig->ovl_config[i].fmt,
							 pConfig->ovl_config[i].addr,
							 pConfig->ovl_config[i].src_x,
							 pConfig->ovl_config[i].src_y,
							 pConfig->ovl_config[i].src_pitch,
							 pConfig->ovl_config[i].dst_x,
							 pConfig->ovl_config[i].dst_y,
							 pConfig->ovl_config[i].dst_w,
							 pConfig->ovl_config[i].dst_h,
							 pConfig->ovl_config[i].keyEn,
							 pConfig->ovl_config[i].key,
							 pConfig->ovl_config[i].aen,
							 pConfig->ovl_config[i].alpha,
							 pConfig->ovl_config[i].sur_aen,
							 pConfig->ovl_config[i].src_alpha,
							 pConfig->ovl_config[i].dst_alpha,
							 pConfig->ovl_config[i].security,
							 1,
							 pConfig->ovl_config[i].yuv_range,
							 nonsec_switch_handle);
				}
				ovl_layer_switch(module, i, pConfig->ovl_config[i].layer_en,
						 nonsec_switch_handle
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
						 , 1
#endif
				    );
			}
			/*in fact, dapc/port_sec will be disabled by cmdq */
			cmdqRecSecureEnablePortSecurity(nonsec_switch_handle, (1LL << cmdq_engine));
			/* cmdqRecSecureEnableDAPC(handle, (1LL << cmdq_engine)); */
			if (ovl_idx == 1)
				cmdqRecFlushAsync(nonsec_switch_handle);
			if (ovl_idx == 0)
				cmdqRecFlush(nonsec_switch_handle);
			cmdqRecDestroy(nonsec_switch_handle);
			DDPMSG("[SVP] switch ovl%d to nonsec\n", ovl_idx);
#ifdef CONFIG_MTK_HDMI_SUPPORT
			g_wdma_rdma_security = DISP_NORMAL_BUFFER;
#endif
		}
		ovl_is_sec[ovl_idx] = 0;
	}
#endif

	for (i = 0; i < OVL_LAYER_NUM; i++) {
		if (pConfig->ovl_config[i].layer_en != 0) {
			if (ovl_check_input_param(&pConfig->ovl_config[i]))
				continue;

			ovl_layer_config(module,
					 i,
					 pConfig->ovl_config[i].source,
					 pConfig->ovl_config[i].fmt,
					 pConfig->ovl_config[i].addr,
					 pConfig->ovl_config[i].src_x,
					 pConfig->ovl_config[i].src_y,
					 pConfig->ovl_config[i].src_pitch,
					 pConfig->ovl_config[i].dst_x,
					 pConfig->ovl_config[i].dst_y,
					 pConfig->ovl_config[i].dst_w,
					 pConfig->ovl_config[i].dst_h,
					 pConfig->ovl_config[i].keyEn,
					 pConfig->ovl_config[i].key,
					 pConfig->ovl_config[i].aen,
					 pConfig->ovl_config[i].alpha,
					 pConfig->ovl_config[i].sur_aen,
					 pConfig->ovl_config[i].src_alpha,
					 pConfig->ovl_config[i].dst_alpha,
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
					 pConfig->ovl_config[i].security, has_sec_layer,
#endif
					 pConfig->ovl_config[i].yuv_range, handle);
		}
		ovl_layer_switch(module, i, pConfig->ovl_config[i].layer_en, handle
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
				 , pConfig->ovl_config[i].security
#endif
		    );
	}
	return 0;
}

int ovl_build_cmdq(DISP_MODULE_ENUM module, void *cmdq_trigger_handle, CMDQ_STATE state)
{
	int ret = 0;
	int reg_pa = DISP_REG_OVL_FLOW_CTRL_DBG;

	if (cmdq_trigger_handle == NULL) {
		DDPERR("cmdq_trigger_handle is NULL\n");
		return -1;
	}

	if (state == CMDQ_CHECK_IDLE_AFTER_STREAM_EOF) {
		if (module == DISP_MODULE_OVL0) {
			ret =
			    cmdqRecPoll(cmdq_trigger_handle, ddp_addr_convert_va2pa(reg_pa), 2,
					0x3ff);
		} else if (module == DISP_MODULE_OVL1) {
			ret =
			    cmdqRecPoll(cmdq_trigger_handle, ddp_addr_convert_va2pa(reg_pa), 2,
					0x3ff);
		} else {
			DDPERR("wrong module: %s\n", ddp_get_module_name(module));
			return -1;
		}
	}

	return 0;
}


/***************** ovl debug info ************/

void ovl_dump_reg(DISP_MODULE_ENUM module)
{
	int idx = ovl_index(module);
	unsigned int off_site = idx * DISP_OVL_INDEX_OFFSET;
	unsigned int src_on = DISP_REG_GET(DISP_REG_OVL_SRC_CON + off_site);

	DDPDUMP("== DISP OVL%d REGS ==\n", idx);
	DDPDUMP("OVL:0x000=0x%08x,0x004=0x%08x,0x008=0x%08x,0x00c=0x%08x\n",
		DISP_REG_GET(DISP_REG_OVL_STA + off_site),
		DISP_REG_GET(DISP_REG_OVL_INTEN + off_site),
		DISP_REG_GET(DISP_REG_OVL_INTSTA + off_site),
		DISP_REG_GET(DISP_REG_OVL_EN + off_site));
	DDPDUMP("OVL:0x010=0x%08x,0x014=0x%08x,0x020=0x%08x,0x024=0x%08x\n",
		DISP_REG_GET(DISP_REG_OVL_TRIG + off_site),
		DISP_REG_GET(DISP_REG_OVL_RST + off_site),
		DISP_REG_GET(DISP_REG_OVL_ROI_SIZE + off_site),
		DISP_REG_GET(DISP_REG_OVL_DATAPATH_CON + off_site));
	DDPDUMP("OVL:0x028=0x%08x,0x02c=0x%08x\n",
		DISP_REG_GET(DISP_REG_OVL_ROI_BGCLR + off_site),
		DISP_REG_GET(DISP_REG_OVL_SRC_CON + off_site));

	if (src_on & 0x1) {
		DDPDUMP("OVL:0x030=0x%08x,0x034=0x%08x,0x038=0x%08x,0x03c=0x%08x\n",
			DISP_REG_GET(DISP_REG_OVL_L0_CON + off_site),
			DISP_REG_GET(DISP_REG_OVL_L0_SRCKEY + off_site),
			DISP_REG_GET(DISP_REG_OVL_L0_SRC_SIZE + off_site),
			DISP_REG_GET(DISP_REG_OVL_L0_OFFSET + off_site));

		DDPDUMP("OVL:0xf40=0x%08x,0x044=0x%08x,0x0c0=0x%08x,0x0d0=0x%08x\n",
			DISP_REG_GET(DISP_REG_OVL_L0_ADDR + off_site),
			DISP_REG_GET(DISP_REG_OVL_L0_PITCH + off_site),
			DISP_REG_GET(DISP_REG_OVL_RDMA0_CTRL + off_site),
			DISP_REG_GET(DISP_REG_OVL_RDMA0_FIFO_CTRL + off_site));

		DDPDUMP("OVL:0x1e0=0x%08x,0x24c=0x%08x\n",
			DISP_REG_GET(DISP_REG_OVL_RDMA0_MEM_GMC_S2 + off_site),
			DISP_REG_GET(DISP_REG_OVL_RDMA0_DBG + off_site));
	}
	if (src_on & 0x2) {
		DDPDUMP("OVL:0x050=0x%08x,0x054=0x%08x,0x058=0x%08x,0x05c=0x%08x\n",
			DISP_REG_GET(DISP_REG_OVL_L1_CON + off_site),
			DISP_REG_GET(DISP_REG_OVL_L1_SRCKEY + off_site),
			DISP_REG_GET(DISP_REG_OVL_L1_SRC_SIZE + off_site),
			DISP_REG_GET(DISP_REG_OVL_L1_OFFSET + off_site));

		DDPDUMP("OVL:0xf60=0x%08x,0x064=0x%08x,0x0e0=0x%08x,0x0f0=0x%08x\n",
			DISP_REG_GET(DISP_REG_OVL_L1_ADDR + off_site),
			DISP_REG_GET(DISP_REG_OVL_L1_PITCH + off_site),
			DISP_REG_GET(DISP_REG_OVL_RDMA1_CTRL + off_site),
			DISP_REG_GET(DISP_REG_OVL_RDMA1_FIFO_CTRL + off_site));

		DDPDUMP("OVL:0x1e4=0x%08x,0x250=0x%08x\n",
			DISP_REG_GET(DISP_REG_OVL_RDMA1_MEM_GMC_S2 + off_site),
			DISP_REG_GET(DISP_REG_OVL_RDMA1_DBG + off_site));
	}
	if (src_on & 0x4) {
		DDPDUMP("OVL:0x070=0x%08x,0x074=0x%08x,0x078=0x%08x,0x07c=0x%08x\n",
			DISP_REG_GET(DISP_REG_OVL_L2_CON + off_site),
			DISP_REG_GET(DISP_REG_OVL_L2_SRCKEY + off_site),
			DISP_REG_GET(DISP_REG_OVL_L2_SRC_SIZE + off_site),
			DISP_REG_GET(DISP_REG_OVL_L2_OFFSET + off_site));

		DDPDUMP("OVL:0xf80=0x%08x,0x084=0x%08x,0x100=0x%08x,0x110=0x%08x\n",
			DISP_REG_GET(DISP_REG_OVL_L2_ADDR + off_site),
			DISP_REG_GET(DISP_REG_OVL_L2_PITCH + off_site),
			DISP_REG_GET(DISP_REG_OVL_RDMA2_CTRL + off_site),
			DISP_REG_GET(DISP_REG_OVL_RDMA2_FIFO_CTRL + off_site));

		DDPDUMP("OVL:0x1e8=0x%08x,0x254=0x%08x\n",
			DISP_REG_GET(DISP_REG_OVL_RDMA2_MEM_GMC_S2 + off_site),
			DISP_REG_GET(DISP_REG_OVL_RDMA2_DBG + off_site));
	}
	if (src_on & 0x8) {
		DDPDUMP("OVL:0x090=0x%08x,0x094=0x%08x,0x098=0x%08x,0x09c=0x%08x\n",
			DISP_REG_GET(DISP_REG_OVL_L3_CON + off_site),
			DISP_REG_GET(DISP_REG_OVL_L3_SRCKEY + off_site),
			DISP_REG_GET(DISP_REG_OVL_L3_SRC_SIZE + off_site),
			DISP_REG_GET(DISP_REG_OVL_L3_OFFSET + off_site));

		DDPDUMP("OVL:0xfa0=0x%08x,0x0a4=0x%08x,0x120=0x%08x,0x130=0x%08x\n",
			DISP_REG_GET(DISP_REG_OVL_L3_ADDR + off_site),
			DISP_REG_GET(DISP_REG_OVL_L3_PITCH + off_site),
			DISP_REG_GET(DISP_REG_OVL_RDMA3_CTRL + off_site),
			DISP_REG_GET(DISP_REG_OVL_RDMA3_FIFO_CTRL + off_site));

		DDPDUMP("OVL:0x1ec=0x%08x,0x258=0x%08x\n",
			DISP_REG_GET(DISP_REG_OVL_RDMA3_MEM_GMC_S2 + off_site),
			DISP_REG_GET(DISP_REG_OVL_RDMA3_DBG + off_site));
	}
	DDPDUMP("OVL:0x1d4=0x%08x,0x200=0x%08x,0x240=0x%08x,0x244=0x%08x\n",
		DISP_REG_GET(DISP_REG_OVL_DEBUG_MON_SEL + off_site),
		DISP_REG_GET(DISP_REG_OVL_DUMMY_REG + off_site),
		DISP_REG_GET(DISP_REG_OVL_FLOW_CTRL_DBG + off_site),
		DISP_REG_GET(DISP_REG_OVL_ADDCON_DBG + off_site));

}

static void ovl_printf_status(int idx, unsigned int status)
{
	DDPDUMP("=OVL%d_FLOW_CONTROL_DEBUG=:\n", idx);
	DDPDUMP("addcon_idle:%d,blend_idle:%d,out_valid:%d,out_ready:%d,out_idle:%d\n",
		(status >> 10) & (0x1),
		(status >> 11) & (0x1),
		(status >> 12) & (0x1), (status >> 13) & (0x1), (status >> 15) & (0x1)
	    );
	DDPDUMP("rdma3_idle:%d,rdma2_idle:%d,rdma1_idle:%d, rdma0_idle:%d,rst:%d\n",
		(status >> 16) & (0x1),
		(status >> 17) & (0x1),
		(status >> 18) & (0x1), (status >> 19) & (0x1), (status >> 20) & (0x1)
	    );
	DDPDUMP("trig:%d,frame_hwrst_done:%d,frame_swrst_done:%d,frame_underrun:%d,frame_done:%d\n",
		(status >> 21) & (0x1),
		(status >> 23) & (0x1),
		(status >> 24) & (0x1), (status >> 25) & (0x1), (status >> 26) & (0x1)
	    );
	DDPDUMP("ovl_running:%d,ovl_start:%d,ovl_clr:%d,reg_update:%d,ovl_upd_reg:%d\n",
		(status >> 27) & (0x1),
		(status >> 28) & (0x1),
		(status >> 29) & (0x1), (status >> 30) & (0x1), (status >> 31) & (0x1)
	    );

	DDPDUMP("ovl%d_fms_state:\n", idx);
	switch (status & 0x3ff) {
	case 0x1:
		DDPDUMP("idle\n");
		break;
	case 0x2:
		DDPDUMP("wait_SOF\n");
		break;
	case 0x4:
		DDPDUMP("prepare\n");
		break;
	case 0x8:
		DDPDUMP("reg_update\n");
		break;
	case 0x10:
		DDPDUMP("eng_clr(internal reset)\n");
		break;
	case 0x20:
		DDPDUMP("eng_act(processing)\n");
		break;
	case 0x40:
		DDPDUMP("h_wait_w_rst\n");
		break;
	case 0x80:
		DDPDUMP("s_wait_w_rst\n");
		break;
	case 0x100:
		DDPDUMP("h_w_rst\n");
		break;
	case 0x200:
		DDPDUMP("s_w_rst\n");
		break;
	default:
		DDPDUMP("ovl_fsm_unknown\n");
		break;
	}

}

static void ovl_print_ovl_rdma_status(unsigned int status)
{
	DDPDUMP
		("wram_rst_cs:0x%x,layer_greq:0x%x,out_data:0x%x",
		status & 0x7,
		(status >> 3) & 0x1,
		(status >> 4) & 0xffffff);
	DDPDUMP
		("out_ready:0x%x,out_valid:0x%x,smi_busy:0x%x,smi_greq:0x%x\n",
		(status >> 28) & 0x1, (status >> 29) & 0x1, (status >> 30) & 0x1, (status >> 31) & 0x1);

}

void ovl_dump_analysis(DISP_MODULE_ENUM module)
{
	int i = 0;
	unsigned int layer_off_site = 0;
	unsigned int rdma_off_site = 0;
	int index = ovl_index(module);
	unsigned int off_site = index * DISP_OVL_INDEX_OFFSET;
	unsigned int src_on = DISP_REG_GET(DISP_REG_OVL_SRC_CON + off_site);

	DDPDUMP("==DISP OVL%d ANALYSIS==\n", index);
	DDPDUMP("ovl_en=%d,layer_enable(%d,%d,%d,%d),bg(w=%d, h=%d),cur_pos(x=%d,y=%d),layer_hit(%d,%d,%d,%d)\n",
		DISP_REG_GET(DISP_REG_OVL_EN + off_site),
		DISP_REG_GET(DISP_REG_OVL_SRC_CON + off_site) & 0x1,
		(DISP_REG_GET(DISP_REG_OVL_SRC_CON + off_site) >> 1) & 0x1,
		(DISP_REG_GET(DISP_REG_OVL_SRC_CON + off_site) >> 2) & 0x1,
		(DISP_REG_GET(DISP_REG_OVL_SRC_CON + off_site) >> 3) & 0x1,
		DISP_REG_GET(DISP_REG_OVL_ROI_SIZE + off_site) & 0xfff,
		(DISP_REG_GET(DISP_REG_OVL_ROI_SIZE + off_site) >> 16) & 0xfff,
		DISP_REG_GET_FIELD(ADDCON_DBG_FLD_ROI_X, DISP_REG_OVL_ADDCON_DBG + off_site),
		DISP_REG_GET_FIELD(ADDCON_DBG_FLD_ROI_Y, DISP_REG_OVL_ADDCON_DBG + off_site),
		DISP_REG_GET_FIELD(ADDCON_DBG_FLD_L0_WIN_HIT, DISP_REG_OVL_ADDCON_DBG + off_site),
		DISP_REG_GET_FIELD(ADDCON_DBG_FLD_L1_WIN_HIT, DISP_REG_OVL_ADDCON_DBG + off_site),
		DISP_REG_GET_FIELD(ADDCON_DBG_FLD_L2_WIN_HIT, DISP_REG_OVL_ADDCON_DBG + off_site),
		DISP_REG_GET_FIELD(ADDCON_DBG_FLD_L3_WIN_HIT, DISP_REG_OVL_ADDCON_DBG + off_site)
	    );
	for (i = 0; i < 4; i++) {
		layer_off_site = i * OVL_LAYER_OFF_SITE + off_site;
		rdma_off_site = i * OVL_RDMA_DEBUG_OFF_SITE + off_site;
		if (src_on & (0x1 << i)) {
			DDPDUMP("layer%d: w=%d,h=%d,off(x=%d,y=%d),pitch=%d,addr=0x%x,fmt=%s\n",
				i,
				DISP_REG_GET(layer_off_site + DISP_REG_OVL_L0_SRC_SIZE) & 0xfff,
				(DISP_REG_GET(layer_off_site + DISP_REG_OVL_L0_SRC_SIZE) >> 16) &
				0xfff,
				DISP_REG_GET(layer_off_site + DISP_REG_OVL_L0_OFFSET) & 0xfff,
				(DISP_REG_GET(layer_off_site + DISP_REG_OVL_L0_OFFSET) >> 16) &
				0xfff,
				DISP_REG_GET(layer_off_site + DISP_REG_OVL_L0_PITCH) & 0xffff,
				DISP_REG_GET(layer_off_site + DISP_REG_OVL_L0_ADDR),
				ovl_intput_format_name(DISP_REG_GET_FIELD
						       (L_CON_FLD_CFMT,
							DISP_REG_OVL_L0_CON + layer_off_site),
						       DISP_REG_GET_FIELD(L_CON_FLD_BTSW,
									  DISP_REG_OVL_L0_CON +
									  layer_off_site)
				));
			DDPDUMP("ovl rdma%d status:(en %d)\n", i,
				DISP_REG_GET(layer_off_site + DISP_REG_OVL_RDMA0_CTRL));
			ovl_print_ovl_rdma_status(DISP_REG_GET
						  (DISP_REG_OVL_RDMA0_DBG + rdma_off_site));

		}
	}
	ovl_printf_status(index, DISP_REG_GET(DISP_REG_OVL_FLOW_CTRL_DBG));
}

int ovl_dump(DISP_MODULE_ENUM module, int level)
{
	ovl_dump_analysis(module);
	ovl_dump_reg(module);

	return 0;
}

/***************** driver************/
DDP_MODULE_DRIVER ddp_driver_ovl = {
	.init = ovl_init,
	.deinit = ovl_deinit,
	.config = ovl_config_l,
	.start = ovl_start,
	.trigger = NULL,
	.stop = ovl_stop,
	.reset = ovl_reset,
	.power_on = ovl_clock_on,
	.power_off = ovl_clock_off,
	.suspend = ovl_suspend,
	.resume = ovl_resume,
	.is_idle = NULL,
	.is_busy = NULL,
	.dump_info = ovl_dump,
	.bypass = NULL,
	.build_cmdq = ovl_build_cmdq,
	.set_lcm_utils = NULL,
};
