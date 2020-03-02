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
#include <linux/bug.h>
#include "ddp_log.h"
#include "ddp_clkmgr.h"
#include "ddp_m4u.h"
#include <linux/delay.h>
#include "ddp_info.h"
#include "ddp_hal.h"
#include "ddp_reg.h"
#include "ddp_ovl.h"
#include "primary_display.h"
#include "disp_rect.h"
#include "disp_assert_layer.h"
#include "ddp_mmp.h"
#include "cmdq_record.h"
#include "ddp_dump.h"
#include "disp_drv_platform.h"

#define OVL_REG_BACK_MAX          (40)
#define OVL_LAYER_OFFSET        (0x20)
#define OVL_RDMA_DEBUG_OFFSET   (0x4)

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#define ASSERT_LAYER_OFFSET  (1)
#else
#define ASSERT_LAYER_OFFSET  (0)
#endif

enum OVL_COLOR_SPACE {
	OVL_COLOR_SPACE_RGB = 0,
	OVL_COLOR_SPACE_YUV,
};

struct OVL_REG {
	unsigned long address;
	unsigned int value;
};

static enum DISP_MODULE_ENUM ovl_index_module[OVL_NUM] = {
	DISP_MODULE_OVL0
};

static unsigned int gOVLBackground = 0xFF000000;

static inline int is_module_ovl(enum DISP_MODULE_ENUM module)
{
	if (module == DISP_MODULE_OVL0)
		return 1;
	else
		return 0;
}

unsigned long ovl_base_addr(enum DISP_MODULE_ENUM module)
{
	switch (module) {
	case DISP_MODULE_OVL0:
		return DISPSYS_OVL0_BASE;
	default:
		DDPERR("invalid ovl module=%d\n", module);
		/* BUG(); */
	}
	return 0;
}

static inline unsigned long ovl_layer_num(enum DISP_MODULE_ENUM module)
{
	switch (module) {
	case DISP_MODULE_OVL0:
		return 4;
	default:
		DDPERR("invalid ovl module=%d\n", module);
		/* BUG(); */
	}
	return 0;
}

enum CMDQ_EVENT_ENUM ovl_to_cmdq_event_nonsec_end(enum DISP_MODULE_ENUM module)
{
	switch (module) {
	case DISP_MODULE_OVL0:
		return CMDQ_SYNC_DISP_OVL0_2NONSEC_END;
	default:
		DDPERR("invalid ovl module=%d, get cmdq event nonsecure fail\n",
		       module);
		ASSERT(0);
	}
	return 0;
}

static inline unsigned long ovl_to_cmdq_engine(enum DISP_MODULE_ENUM module)
{
	switch (module) {
	case DISP_MODULE_OVL0:
		return CMDQ_ENG_DISP_OVL0;
	default:
		DDPERR("invalid ovl module=%d, get cmdq engine fail\n", module);
		ASSERT(0);
		return DISP_MODULE_UNKNOWN;
	}
	return 0;
}

unsigned long ovl_to_index(enum DISP_MODULE_ENUM module)
{
	int i;

	for (i = 0; i < OVL_NUM; i++) {
		if (ovl_index_module[i] == module)
			return i;
	}
	DDPERR("invalid ovl module=%d, get ovl index fail\n", module);
	ASSERT(0);
	return 0;
}

static inline enum DISP_MODULE_ENUM ovl_index_to_module(int index)
{
	if (index >= OVL_NUM) {
		DDPERR("invalid ovl index=%d\n", index);
		ASSERT(0);
	}

	return ovl_index_module[index];
}

int ovl_start(enum DISP_MODULE_ENUM module, void *handle)
{
	unsigned long ovl_base = ovl_base_addr(module);
	unsigned int reg_val = 0;

	DISP_REG_SET_FIELD(handle, EN_FLD_OVL_EN,
			   ovl_base + DISP_REG_OVL_EN, 0x1);

	DISP_REG_SET(handle, ovl_base + DISP_REG_OVL_INTEN,
		     0x21F0 | REG_FLD_VAL(INTEN_FLD_FME_CPL_INTEN, 1) |
		     REG_FLD_VAL(INTEN_FLD_START_INTEN, 0));

	reg_val = 0;
	/* if ovl use smi id, emi can do "out-of-order-arbitration",
	 * this aciton will optimize DRAM transaction, and save more power!
	 */
	reg_val |= REG_FLD_VAL(DATAPATH_CON_FLD_LAYER_SMI_ID_EN, 1);
	reg_val |= REG_FLD_VAL(DATAPATH_CON_FLD_OUTPUT_NO_RND, 0);
	reg_val |= REG_FLD_VAL(DATAPATH_CON_FLD_GCLAST_EN, 1);
	reg_val |= REG_FLD_VAL(OVL_WIDE_GAMUT_EN, 1);
	reg_val |= REG_FLD_VAL(DATAPATH_CON_FLD_OUTPUT_CLAMP, 1);
	DISP_REG_SET(handle, ovl_base + DISP_REG_OVL_DATAPATH_CON, reg_val);
	return 0;
}

int ovl_stop(enum DISP_MODULE_ENUM module, void *handle)
{
	unsigned long ovl_base = ovl_base_addr(module);

	DISP_REG_SET(handle, ovl_base + DISP_REG_OVL_INTEN, 0x00);
	DISP_REG_SET_FIELD(handle, EN_FLD_OVL_EN,
			   ovl_base + DISP_REG_OVL_EN, 0x0);
	DISP_REG_SET(handle, ovl_base + DISP_REG_OVL_INTSTA, 0x00);

	return 0;
}

int ovl_is_idle(enum DISP_MODULE_ENUM module)
{
	unsigned long ovl_base = ovl_base_addr(module);
	unsigned int ovl_flow_ctrl_dbg;

	ovl_flow_ctrl_dbg = DISP_REG_GET(ovl_base + DISP_REG_OVL_FLOW_CTRL_DBG);
	if ((ovl_flow_ctrl_dbg & 0x3ff) != 0x1 &&
	    (ovl_flow_ctrl_dbg & 0x3ff) != 0x2)
		return 0;

	return 1;
}

int ovl_reset(enum DISP_MODULE_ENUM module, void *handle)
{
#define OVL_IDLE (0x3)
	int ret = 0;
	unsigned int delay_cnt = 0;
	unsigned long ovl_base = ovl_base_addr(module);
	unsigned int ovl_flow_ctrl_dbg;

	DISP_CPU_REG_SET(ovl_base + DISP_REG_OVL_RST, 0x1);
	DISP_CPU_REG_SET(ovl_base + DISP_REG_OVL_RST, 0x0);
	/*only wait if not cmdq */
	if (handle)
		return ret;

	ovl_flow_ctrl_dbg = DISP_REG_GET(ovl_base + DISP_REG_OVL_FLOW_CTRL_DBG);
	while (!(ovl_flow_ctrl_dbg & OVL_IDLE)) {
		delay_cnt++;
		udelay(10);
		if (delay_cnt > 2000) {
			DDPERR("%s reset timeout!\n",
			       ddp_get_module_name(module));
			ret = -1;
			break;
		}
		ovl_flow_ctrl_dbg = DISP_REG_GET(ovl_base
						 + DISP_REG_OVL_FLOW_CTRL_DBG);
	}
	return ret;
}

int ovl_roi(enum DISP_MODULE_ENUM module, unsigned int bg_w, unsigned int bg_h,
	    unsigned int bg_color, void *handle)
{
	unsigned long ovl_base = ovl_base_addr(module);

	if ((bg_w > OVL_MAX_WIDTH) || (bg_h > OVL_MAX_HEIGHT)) {
		DDPERR("%s,exceed OVL max size, w=%d, h=%d\n",
			__func__, bg_w, bg_h);
		ASSERT(0);
	}

	DISP_REG_SET(handle, ovl_base + DISP_REG_OVL_ROI_SIZE,
		     bg_h << 16 | bg_w);

	DISP_REG_SET(handle, ovl_base + DISP_REG_OVL_ROI_BGCLR, bg_color);

	DDPMSG("%s:(%ux%u)\n", __func__, bg_w, bg_h);
	return 0;
}
int disable_ovl_layers(enum DISP_MODULE_ENUM module, void *handle)
{
	unsigned long ovl_base = ovl_base_addr(module);
	int ovl_idx = ovl_to_index(module);

	/* physical layer control */
	DISP_REG_SET(handle, ovl_base + DISP_REG_OVL_SRC_CON, 0);
	/* ext layer control */
	DISP_REG_SET(handle, ovl_base + DISP_REG_OVL_DATAPATH_EXT_CON, 0);
	DDPSVPMSG("[SVP] switch ovl%d to nonsec: disable all the layers first!\n",
		ovl_idx);
	return 0;
}

int ovl_layer_switch(enum DISP_MODULE_ENUM module, unsigned int layer,
		     unsigned int en, void *handle)
{
	unsigned long ovl_base = ovl_base_addr(module);

	ASSERT(layer <= 3);
	switch (layer) {
	case 0:
		DISP_REG_SET_FIELD(handle, SRC_CON_FLD_L0_EN,
				   ovl_base + DISP_REG_OVL_SRC_CON, en);
		DISP_REG_SET(handle, ovl_base + DISP_REG_OVL_RDMA0_CTRL, en);
		break;
	case 1:
		DISP_REG_SET_FIELD(handle, SRC_CON_FLD_L1_EN,
				   ovl_base + DISP_REG_OVL_SRC_CON, en);
		DISP_REG_SET(handle, ovl_base + DISP_REG_OVL_RDMA1_CTRL, en);
		break;
	case 2:
		DISP_REG_SET_FIELD(handle, SRC_CON_FLD_L2_EN,
				   ovl_base + DISP_REG_OVL_SRC_CON, en);
		DISP_REG_SET(handle, ovl_base + DISP_REG_OVL_RDMA2_CTRL, en);
		break;
	case 3:
		DISP_REG_SET_FIELD(handle, SRC_CON_FLD_L3_EN,
				   ovl_base + DISP_REG_OVL_SRC_CON, en);
		DISP_REG_SET(handle, ovl_base + DISP_REG_OVL_RDMA3_CTRL, en);
		break;
	default:
		DDPERR("invalid layer=%d\n", layer);
		ASSERT(0);
	}

	return 0;
}

static int ovl_layer_config(enum DISP_MODULE_ENUM module, unsigned int layer,
			    unsigned int is_engine_sec,
			    const struct OVL_CONFIG_STRUCT *const cfg,
			    const struct disp_rect *const ovl_partial_roi,
			    const struct disp_rect *const layer_partial_roi,
			    void *handle)
{
	unsigned int value = 0;
	unsigned int Bpp, input_swap, input_fmt;
	unsigned int rgb_swap = 0;
	int is_rgb;
	int color_matrix = 0x4;
	int rotate = 0;
	int is_ext_layer = cfg->ext_layer != -1;
	unsigned long ovl_base = ovl_base_addr(module);
	unsigned long ext_layer_offset = is_ext_layer ?
			(DISP_REG_OVL_EL0_CON - DISP_REG_OVL_L0_CON) : 0;
	unsigned long ext_layer_offset_clr = is_ext_layer ?
			(DISP_REG_OVL_EL0_CLEAR - DISP_REG_OVL_L0_CLR) : 0;
	unsigned long ext_layer_offset_addr = is_ext_layer ?
			(DISP_REG_OVL_EL0_ADDR - DISP_REG_OVL_L0_ADDR) : 0;
	unsigned long layer_offset = ovl_base + ext_layer_offset +
			(is_ext_layer ? cfg->ext_layer : layer) *
			OVL_LAYER_OFFSET;
	unsigned long layer_offset_clr = ovl_base + ext_layer_offset_clr +
			(is_ext_layer ? cfg->ext_layer : layer) * 4;
	unsigned long layer_offset_rdma_ctrl = ovl_base + layer *
			OVL_LAYER_OFFSET;
	unsigned long layer_offset_addr = ovl_base + ext_layer_offset_addr +
			(is_ext_layer ? cfg->ext_layer * 4 : layer *
			OVL_LAYER_OFFSET);
	unsigned int offset = 0;
	enum UNIFIED_COLOR_FMT format = cfg->fmt;
	unsigned int src_x = cfg->src_x;
	unsigned int src_y = cfg->src_y;
	unsigned int dst_x = cfg->dst_x;
	unsigned int dst_y = cfg->dst_y;
	unsigned int dst_w = cfg->dst_w;
	unsigned int dst_h = cfg->dst_h;

	if (ovl_partial_roi != NULL) {
		dst_x = layer_partial_roi->x - ovl_partial_roi->x;
		dst_y = layer_partial_roi->y - ovl_partial_roi->y;
		dst_w = layer_partial_roi->width;
		dst_h = layer_partial_roi->height;
		src_x = cfg->src_x + layer_partial_roi->x - cfg->dst_x;
		src_y = cfg->src_y + layer_partial_roi->y - cfg->dst_y;

		DDPDBG("layer partial (%d,%d)(%d,%d,%d,%d) to (%d,%d)(%d,%d,%d,%d)\n",
		       cfg->src_x, cfg->src_y, cfg->dst_x, cfg->dst_y,
		       cfg->dst_w, cfg->dst_h, src_x, src_y, dst_x, dst_y,
		       dst_w, dst_h);
	}

	if (dst_w > OVL_MAX_WIDTH)
		ASSERT(dst_w < OVL_MAX_WIDTH);
	if (dst_h > OVL_MAX_HEIGHT)
		ASSERT(dst_h < OVL_MAX_HEIGHT);

	if (!cfg->addr && cfg->source == OVL_LAYER_SOURCE_MEM) {
		DDPERR("source from memory, but addr is 0!\n");
		ASSERT(0);
		return -1;
	}

#ifdef CONFIG_MTK_LCM_PHYSICAL_ROTATION_HW
	rotate = 1;
#endif

	if (rotate) {
		if (is_slt_test())
			rotate = 0;
	}

	if (is_slt_test()) {
		DISP_REG_SET_FIELD(handle, DATAPATH_CON_FLD_OUTPUT_CLAMP,
				   ovl_base + DISP_REG_OVL_DATAPATH_CON, 0x0);
	} else {
		DISP_REG_SET_FIELD(handle, DATAPATH_CON_FLD_OUTPUT_CLAMP,
				   ovl_base + DISP_REG_OVL_DATAPATH_CON, 0x1);
	}
	/* check dim layer fmt */
	if (cfg->source == OVL_LAYER_SOURCE_RESERVED) {
		if (cfg->aen == 0)
			DDPERR("dim layer%d ahpha enable should be 1!\n",
			       layer);
		format = UFMT_RGB888;
	}
	Bpp = ufmt_get_Bpp(format);
	input_swap = ufmt_get_byteswap(format);
	input_fmt = ufmt_get_format(format);
	is_rgb = ufmt_get_rgb(format);
	if (rotate) {
		unsigned int bg_h, bg_w;

		bg_h = DISP_REG_GET(ovl_base + DISP_REG_OVL_ROI_SIZE);
		bg_w = bg_h & 0xFFFF;
		bg_h = bg_h >> 16;
		DISP_REG_SET(handle, DISP_REG_OVL_L0_OFFSET + layer_offset,
			     ((bg_h - dst_h - dst_y) << 16) |
				     (bg_w - dst_w - dst_x));
	} else {
		DISP_REG_SET(handle, DISP_REG_OVL_L0_OFFSET + layer_offset,
			     (dst_y << 16) | dst_x);
	}

	if (format == UFMT_UYVY || format == UFMT_VYUY ||
	    format == UFMT_YUYV || format == UFMT_YVYU) {
		unsigned int regval = 0;

		if (src_x % 2) {
			src_x -= 1; /* make src_x to even */
			dst_w += 1;
			regval |= REG_FLD_VAL(OVL_L_CLIP_FLD_LEFT, 1);
		}

		if ((src_x + dst_w) % 2) {
			dst_w += 1; /* make right boundary even */
			regval |= REG_FLD_VAL(OVL_L_CLIP_FLD_RIGHT, 1);
		}
		DISP_REG_SET(handle, DISP_REG_OVL_L0_CLIP + layer_offset,
			     regval);
	} else {
		DISP_REG_SET(handle, DISP_REG_OVL_L0_CLIP + layer_offset, 0);
	}

	switch (cfg->yuv_range) {
	case 0:
		color_matrix = 4;
		break; /* BT601_full */
	case 1:
		color_matrix = 6;
		break; /* BT601 */
	case 2:
		color_matrix = 7;
		break; /* BT709 */
	default:
		DDPERR("un-recognized yuv_range=%d!\n", cfg->yuv_range);
		color_matrix = 4;
		break;
	}

	DISP_REG_SET(handle, DISP_REG_OVL_RDMA0_CTRL +
			layer_offset_rdma_ctrl, 0x1);

	value = (REG_FLD_VAL((L_CON_FLD_LSRC), (cfg->source)) |
		 REG_FLD_VAL((L_CON_FLD_CFMT), (input_fmt)) |
		 REG_FLD_VAL((L_CON_FLD_AEN), (cfg->aen)) |
		 REG_FLD_VAL((L_CON_FLD_APHA), (cfg->alpha)) |
		 REG_FLD_VAL((L_CON_FLD_SKEN), (cfg->keyEn)) |
		 REG_FLD_VAL((L_CON_FLD_BTSW), (input_swap)));
	if (ufmt_is_old_fmt(format)) {
		if (format == UFMT_PARGB8888 || format == UFMT_PABGR8888 ||
		    format == UFMT_PRGBA8888 || format == UFMT_PBGRA8888) {
			rgb_swap = ufmt_get_rgbswap(format);
			value |= REG_FLD_VAL((L_CON_FLD_RGB_SWAP), (rgb_swap));
		}
		value |= REG_FLD_VAL((L_CON_FLD_CLRFMT_MAN), (1));
	}

	if (!is_rgb)
		value = value | REG_FLD_VAL((L_CON_FLD_MTX), (color_matrix));

	if (rotate)
		value |= REG_FLD_VAL((L_CON_FLD_VIRTICAL_FLIP), (1)) |
			 REG_FLD_VAL((L_CON_FLD_HORI_FLIP), (1));

	DISP_REG_SET(handle, DISP_REG_OVL_L0_CON + layer_offset, value);

	DISP_REG_SET(handle, DISP_REG_OVL_L0_CLR + layer_offset_clr,
		     0xff000000 | cfg->dim_color);

	DISP_REG_SET(handle, DISP_REG_OVL_L0_SRC_SIZE + layer_offset,
		     dst_h << 16 | dst_w);

	if (rotate)
		offset = (src_x + dst_w) * Bpp +
			 (src_y + dst_h - 1) * cfg->src_pitch - 1;
	else
		offset = src_x * Bpp + src_y * cfg->src_pitch;

	if (!is_engine_sec) {
		DISP_REG_SET(handle, DISP_REG_OVL_L0_ADDR + layer_offset_addr,
			     cfg->addr + offset);
	} else {
		unsigned int size;
		int m4u_port;

		size = (dst_h - 1) * cfg->src_pitch + dst_w * Bpp;
		m4u_port = module_to_m4u_port(module);
		if (cfg->security != DISP_SECURE_BUFFER) {
			/*
			 * ovl is sec but this layer is non-sec
			 * we need to tell cmdq to help map non-sec
			 * mva to sec mva
			 */
			cmdqRecWriteSecure(handle,
				disp_addr_convert(DISP_REG_OVL_L0_ADDR +
					layer_offset_addr),
				CMDQ_SAM_NMVA_2_MVA, cfg->addr + offset,
					0, size, m4u_port);

		} else {
			/*
			 * for sec layer, addr variable stores sec handle
			 * we need to pass this handle and offset to cmdq driver
			 * cmdq sec driver will help to convert handle to
			 * correct address
			 */
			cmdqRecWriteSecure(handle,
				disp_addr_convert(DISP_REG_OVL_L0_ADDR +
				layer_offset_addr),
				CMDQ_SAM_H_2_MVA, cfg->addr, offset,
				size, m4u_port);
		}
	}
	DISP_REG_SET(handle, DISP_REG_OVL_L0_SRCKEY + layer_offset, cfg->key);

	value = REG_FLD_VAL(L_PITCH_FLD_SA_SEL, cfg->src_alpha & 0x3) |
		REG_FLD_VAL(L_PITCH_FLD_SRGB_SEL, cfg->src_alpha & 0x3) |
		REG_FLD_VAL(L_PITCH_FLD_DA_SEL, cfg->dst_alpha & 0x3) |
		REG_FLD_VAL(L_PITCH_FLD_DRGB_SEL, cfg->dst_alpha & 0x3) |
		REG_FLD_VAL(L_PITCH_FLD_SURFL_EN, cfg->src_alpha & 0x1) |
		REG_FLD_VAL(L_PITCH_FLD_SRC_PITCH, cfg->src_pitch);

	if (cfg->const_bld)
		value |= REG_FLD_VAL((L_PITCH_FLD_CONST_BLD), (1));
	DISP_REG_SET(handle, DISP_REG_OVL_L0_PITCH + layer_offset, value);

	return 0;
}

int ovl_clock_on(enum DISP_MODULE_ENUM module, void *handle)
{
	DDPDBG("%s clock_on\n", ddp_get_module_name(module));
	ddp_clk_prepare_enable(ddp_get_module_clk_id(module));

	return 0;
}

int ovl_clock_off(enum DISP_MODULE_ENUM module, void *handle)
{
	DDPDBG("%s clock_off\n", ddp_get_module_name(module));
	ddp_clk_disable_unprepare(ddp_get_module_clk_id(module));

	return 0;
}

int ovl_init(enum DISP_MODULE_ENUM module, void *handle)
{
	return 0;
}

int ovl_deinit(enum DISP_MODULE_ENUM module, void *handle)
{
	return 0;
}

int ovl_connect(enum DISP_MODULE_ENUM module, enum DISP_MODULE_ENUM prev,
		enum DISP_MODULE_ENUM next, int connect, void *handle)
{
	unsigned long ovl_base = ovl_base_addr(module);

	if (connect && is_module_ovl(prev))
		DISP_REG_SET_FIELD(handle, DATAPATH_CON_FLD_BGCLR_IN_SEL,
				   ovl_base + DISP_REG_OVL_DATAPATH_CON, 1);
	else
		DISP_REG_SET_FIELD(handle, DATAPATH_CON_FLD_BGCLR_IN_SEL,
				   ovl_base + DISP_REG_OVL_DATAPATH_CON, 0);

	return 0;
}

unsigned int ddp_ovl_get_cur_addr(bool rdma_mode, int layerid)
{
	/*just workaround, should remove this func */
	unsigned long ovl_base = ovl_base_addr(DISP_MODULE_OVL0);
	unsigned long Lx_base = 0;

	if (rdma_mode)
		return DISP_REG_GET(DISP_REG_RDMA_MEM_START_ADDR);

	Lx_base = layerid * OVL_LAYER_OFFSET + ovl_base;
	if (DISP_REG_GET(DISP_REG_OVL_RDMA0_CTRL + Lx_base) & 0x1)
		return DISP_REG_GET(DISP_REG_OVL_L0_ADDR + Lx_base);

	return 0;
}

void ovl_get_address(enum DISP_MODULE_ENUM module, unsigned long *add)
{
	int i = 0;
	unsigned long ovl_base = ovl_base_addr(module);
	unsigned long layer_off = 0;
	unsigned int src_on = DISP_REG_GET(DISP_REG_OVL_SRC_CON + ovl_base);

	for (i = 0; i < 4; i++) {
		layer_off = i * OVL_LAYER_OFFSET + ovl_base;
		if (src_on & (0x1 << i))
			add[i] = DISP_REG_GET(layer_off + DISP_REG_OVL_L0_ADDR);
		else
			add[i] = 0;
	}
}

void ovl_get_info(enum DISP_MODULE_ENUM module, void *data)
{
	unsigned int i = 0;
	struct OVL_BASIC_STRUCT *pdata = data;
	unsigned long ovl_base = ovl_base_addr(module);
	unsigned long layer_off = 0;
	unsigned int src_on = DISP_REG_GET(DISP_REG_OVL_SRC_CON + ovl_base);
	struct OVL_BASIC_STRUCT *p = NULL;

	for (i = 0; i < ovl_layer_num(module); i++) {
		unsigned int val = 0;

		layer_off = i * OVL_LAYER_OFFSET + ovl_base;
		p = &pdata[i];
		p->layer = i;
		p->layer_en = src_on & (0x1 << i);
		if (!p->layer_en) {
			DDPDBG("%s:layer%d,en %d,w %d,h %d,bpp %d,addr %lu\n",
			       __func__, i, p->layer_en, p->src_w, p->src_h,
			       p->bpp, p->addr);
			continue;
		}

		val = DISP_REG_GET(DISP_REG_OVL_L0_CON + layer_off);
		p->fmt = display_fmt_reg_to_unified_fmt(
				REG_FLD_VAL_GET(L_CON_FLD_CFMT, val),
				REG_FLD_VAL_GET(L_CON_FLD_BTSW, val),
				REG_FLD_VAL_GET(L_CON_FLD_RGB_SWAP, val));
		p->addr = DISP_REG_GET(layer_off + DISP_REG_OVL_L0_ADDR);

		val = DISP_REG_GET(DISP_REG_OVL_L0_SRC_SIZE + layer_off);
		p->src_w = val & 0xfff;
		p->src_h = (val >> 16) & 0xfff;

		val = DISP_REG_GET(DISP_REG_OVL_L0_PITCH + layer_off);
		p->src_pitch = val & 0xffff;
		p->bpp = UFMT_GET_bpp(p->fmt) / 8;

		val = DISP_REG_GET(DISP_REG_OVL_DATAPATH_CON + layer_off);
		p->gpu_mode = (val & (0x1 << (8 + i))) ? 1 : 0;
		p->adobe_mode = (val & (0x1 << 12)) ? 1 : 0;
		p->ovl_gamma_out = (val & (0x1 << 15)) ? 1 : 0;
		p->alpha = (DISP_REG_GET(layer_off + DISP_REG_OVL_L0_CON +
				      (i * 0x20)) & (0x1 << 8)) ? 1 : 0;

		DDPDBG("ovl_get_info:layer%d,en %d,w %d,h %d,bpp %d,addr %lu\n",
		       i, p->layer_en, p->src_w, p->src_h, p->bpp, p->addr);
	}
}

extern int m4u_query_mva_info(unsigned int mva, unsigned int size,
			      unsigned int *real_mva, unsigned int *real_size);

static int ovl_check_input_param(struct OVL_CONFIG_STRUCT *config)
{
	if ((config->addr == 0 && config->source == 0) ||
	    config->dst_w == 0 || config->dst_h == 0) {
		DDPERR("ovl parameter invalidate, addr=%lu, w=%d, h=%d\n",
		       config->addr, config->dst_w, config->dst_h);
		ASSERT(0);
		return -1;
	}

	return 0;
}

/* use noinline to reduce stack size */
static noinline
void print_layer_config_args(int module, int local_layer,
			     struct OVL_CONFIG_STRUCT *ovl_cfg,
			     struct disp_rect *roi)
{
	DDPDBG("%s, layer=%d(%d), source=%s, off(x=%d, y=%d), dst(%d, %d, %d, %d),pitch=%d,",
	       ddp_get_module_name(module), local_layer, ovl_cfg->layer,
	       (ovl_cfg->source == 0) ? "memory" : "dim", ovl_cfg->src_x,
	       ovl_cfg->src_y, ovl_cfg->dst_x, ovl_cfg->dst_y, ovl_cfg->dst_w,
	       ovl_cfg->dst_h, ovl_cfg->src_pitch);
	DDPDBG("fmt=%s, addr=%lx, keyEn=%d, key=%d, aen=%d, alpha=%d,",
	       unified_color_fmt_name(ovl_cfg->fmt), ovl_cfg->addr,
	       ovl_cfg->keyEn, ovl_cfg->key, ovl_cfg->aen, ovl_cfg->alpha);
	DDPDBG("sur_aen=%d,sur_alpha=0x%x,yuv_range=%d,sec=%d,const_bld=%d\n",
	       ovl_cfg->sur_aen, (ovl_cfg->dst_alpha << 2) | ovl_cfg->src_alpha,
	       ovl_cfg->yuv_range, ovl_cfg->security, ovl_cfg->const_bld);
	if (roi)
		DDPDBG("dirty(%d,%d,%d,%d)\n", roi->x, roi->y, roi->height,
		       roi->width);
}

static int ovl_is_sec[OVL_NUM];

static inline int ovl_switch_to_sec(enum DISP_MODULE_ENUM module, void *handle)
{
	unsigned int ovl_idx = ovl_to_index(module);
	enum CMDQ_ENG_ENUM cmdq_engine;

	cmdq_engine = ovl_to_cmdq_engine(module);
	cmdqRecSetSecure(handle, 1);
	/*
	 * set engine as sec port, it will to access
	 * the sec memory EMI_MPU protected
	 */
	if (cmdq_engine < 64)
		cmdqRecSecureEnablePortSecurity(handle, (1LL << cmdq_engine));
	else
		DDPERR("cmdq engine overflow!:%d\n", cmdq_engine);

	/* Enable DAPC to protect the engine register */
	/* cmdqRecSecureEnableDAPC(handle, (1LL << cmdq_engine)); */
	if (ovl_is_sec[ovl_idx] == 0) {
		DDPSVPMSG("[SVP] switch ovl%d to sec\n", ovl_idx);
		mmprofile_log_ex(ddp_mmp_get_events()->svp_module[module],
				 MMPROFILE_FLAG_START, 0, 0);
	}
	ovl_is_sec[ovl_idx] = 1;

	return 0;
}

int ovl_switch_to_nonsec(enum DISP_MODULE_ENUM module, void *handle)
{
	unsigned int ovl_idx = ovl_to_index(module);
	enum CMDQ_ENG_ENUM cmdq_engine;
	enum CMDQ_EVENT_ENUM cmdq_event_nonsec_end;

	cmdq_engine = ovl_to_cmdq_engine(module);

	if (ovl_is_sec[ovl_idx] == 1) {
		/* ovl is in sec stat, we need to switch it to nonsec */
		struct cmdqRecStruct *nonsec_switch_handle = NULL;
		int ret;

		ret = cmdqRecCreate(
			CMDQ_SCENARIO_DISP_PRIMARY_DISABLE_SECURE_PATH,
			&(nonsec_switch_handle));
		if (ret)
			DDPAEE("[SVP]fail to create disable handle %s ret=%d\n",
			       __func__, ret);

		cmdqRecReset(nonsec_switch_handle);

		if (module != DISP_MODULE_OVL1_2L) {
			/*Primary Mode*/
			if (primary_display_is_decouple_mode())
				cmdqRecWaitNoClear(nonsec_switch_handle,
						   CMDQ_EVENT_DISP_WDMA0_EOF);
			else
				_cmdq_insert_wait_frame_done_token_mira(
					nonsec_switch_handle);
		} else {
			/*External Mode*/
			/*_cmdq_insert_wait_frame_done_token_mira
			 *	(nonsec_switch_handle);
			 */
			cmdqRecWaitNoClear(nonsec_switch_handle,
					   CMDQ_SYNC_DISP_EXT_STREAM_EOF);
		}
		cmdqRecSetSecure(nonsec_switch_handle, 1);

		/*
		 * we should disable OVL before new (nonsec) setting takes
		 * effect, or translation fault may happen.
		 * if we switch OVL to nonsec BUT its setting is still sec
		 */
		disable_ovl_layers(module, nonsec_switch_handle);
		/*in fact, dapc/port_sec will be disabled by cmdq */
		cmdqRecSecureEnablePortSecurity(nonsec_switch_handle,
						(1LL << cmdq_engine));
		/* cmdqRecSecureEnableDAPC(nonsec_switch_handle, (1LL <<
		 * cmdq_engine));
		 */

		if (handle) {
			/* async flush method */
			cmdq_event_nonsec_end =
				ovl_to_cmdq_event_nonsec_end(module);
			cmdqRecSetEventToken(nonsec_switch_handle,
					     cmdq_event_nonsec_end);
			cmdqRecFlushAsync(nonsec_switch_handle);
			cmdqRecWait(handle, cmdq_event_nonsec_end);
		} else {
			/* sync flush method */
			cmdqRecFlush(nonsec_switch_handle);
		}

		cmdqRecDestroy(nonsec_switch_handle);
		DDPSVPMSG("[SVP] switch ovl%d to nonsec\n", ovl_idx);
		mmprofile_log_ex(ddp_mmp_get_events()->svp_module[module],
				 MMPROFILE_FLAG_END, 0, 0);
	}
	ovl_is_sec[ovl_idx] = 0;

	return 0;
}

static int setup_ovl_sec(enum DISP_MODULE_ENUM module,
			 struct disp_ddp_path_config *pConfig, void *handle)
{
	int ret;
	int layer_id;
	int has_sec_layer = 0;
	struct OVL_CONFIG_STRUCT *ovl_config;

	/* check if the OVL module has sec layer */
	for (layer_id = 0; layer_id < TOTAL_OVL_LAYER_NUM; layer_id++) {
		ovl_config = &pConfig->ovl_config[layer_id];
		if (ovl_config->ovl_index == module &&
		    ovl_config->layer_en &&
		    ovl_config->security == DISP_SECURE_BUFFER)
			has_sec_layer = 1;
	}

	if (!handle) {
		DDPDBG("[SVP] bypass ovl sec setting sec=%d,handle=NULL\n",
		       has_sec_layer);
		return 0;
	}

	if (has_sec_layer == 1)
		ret = ovl_switch_to_sec(module, handle);
	else
		ret = ovl_switch_to_nonsec(module, NULL);

	if (ret)
		DDPAEE("[SVP]fail to %s ret=%d\n",
		       __func__, ret);

	return has_sec_layer;
}

/*
 * for enabled layers: layout continuously for each OVL HW engine
 * for disabled layers: ignored
 */
static int ovl_layer_layout(enum DISP_MODULE_ENUM module,
			    struct disp_ddp_path_config *pConfig)
{
	int local_layer, global_layer = 0, assert_layer = 0;
	int ovl_idx = module;
	int phy_layer = -1, ext_layer = -1, ext_layer_idx = 0;

	BUILD_BUG_ON(PRIMARY_SESSION_INPUT_LAYER_COUNT > TOTAL_OVL_LAYER_NUM);
	/*
	 * 1. check if it has been prepared,
	 * just only prepare once for each frame
	 */
#if 0
	for (global_layer = 0; global_layer < TOTAL_OVL_LAYER_NUM;
	     global_layer++) {
		if (!(pConfig->ovl_layer_scanned & (1 << global_layer)))
			break;
	}
	if (global_layer >= TOTAL_OVL_LAYER_NUM)
		return 0;
#else
	if (pConfig->ovl_layer_scanned != 0)
		return 0;
#endif
	/* 2. prepare layer layout */
	for (local_layer = 0; global_layer < PRIMARY_SESSION_INPUT_LAYER_COUNT;
	     local_layer++, global_layer++) {
		struct OVL_CONFIG_STRUCT *ovl_cfg =
			&pConfig->ovl_config[global_layer];

		ext_layer = -1;
		ovl_cfg->phy_layer = 0;
		ovl_cfg->ext_layer = -1;

		pConfig->ovl_layer_scanned |= (1 << global_layer);

		/*
		 * skip disabled layers, but need to decrease
		 * local_layer to make layout continuously
		 * all layers layout continuously by HRT Calc,
		 * so this is not necessary
		 */
		if (ovl_cfg->layer_en == 0) {
			local_layer--;
			continue;
		}

		/* for Assert_layer config special case, do it specially */
		assert_layer = primary_display_get_option("ASSERT_LAYER");
		if (is_DAL_Enabled() && ovl_cfg->layer == assert_layer) {
			ovl_cfg->ovl_index = DISP_MODULE_OVL0;
			ovl_cfg->phy_layer = ovl_layer_num(DISP_MODULE_OVL0) -
					     ASSERT_LAYER_OFFSET - 1;
			continue;
		}

		if (disp_helper_get_option(DISP_OPT_OVL_EXT_LAYER)) {
			if (ovl_cfg->ext_sel_layer != -1) {
				/* always layout from idx=0, so layer_idx
				 * here should be the same as ext_sel_layer
				 */
				if (phy_layer != ovl_cfg->ext_sel_layer) {
					DDPERR("L%d layout not match: cur=%d, in=%d\n",
					       global_layer, phy_layer,
					       ovl_cfg->ext_sel_layer);
					phy_layer++;
					ext_layer = -1;
				} else {
					ext_layer = ext_layer_idx++;
				}
			} else {
				/* all phy layers are layout continuously */
				phy_layer++;
			}
		} else {
			phy_layer = local_layer;
		}
		/* when OVL is full, update ovl index, ext layer capability is
		 * confirmed before
		 */
		if (phy_layer >= ovl_layer_num(ovl_idx)) {
			phy_layer = 0;
			local_layer = 0;
			ext_layer = -1;
			ext_layer_idx = 0;

			if (ovl_idx == DISP_MODULE_OVL0)
				ovl_idx = DISP_MODULE_OVL0_2L;
			else
				DDPERR("unknown module: %d\n", ovl_idx);
		}
		ovl_cfg->ovl_index = ovl_idx;
		ovl_cfg->phy_layer = phy_layer;
		ovl_cfg->ext_layer = ext_layer;
		DDPDBG("layout:L%d->ovl%d,phy:%d,ext:%d,ext_sel:%d\n",
		       global_layer, ovl_idx, phy_layer, ext_layer,
		       (ext_layer >= 0) ? phy_layer : ovl_cfg->ext_sel_layer);
	}
	return 1;
}

static int ovl_config_l(enum DISP_MODULE_ENUM module,
			struct disp_ddp_path_config *pConfig, void *handle)
{
	int enabled_layers = 0;
	int has_sec_layer = 0;
	int layer_id;
	int enabled_ext_layers = 0, ext_sel_layers = 0;
#if 0
	unsigned int tb = 0;
	unsigned int bb = 0;
#endif

	if (pConfig->dst_dirty)
		ovl_roi(module, pConfig->dst_w, pConfig->dst_h, gOVLBackground,
			handle);

	if (!pConfig->ovl_dirty)
		return 0;

	ovl_layer_layout(module, pConfig);

#if 0
	/* check if the ovl module has sec layer */
	for (layer_id = 0; layer_id < TOTAL_OVL_LAYER_NUM; layer_id++) {
		if (pConfig->ovl_config[layer_id].ovl_index == module &&
		    pConfig->ovl_config[layer_id].layer_en &&
		    (pConfig->ovl_config[layer_id].security ==
		     DISP_SECURE_BUFFER))
		has_sec_layer = 1;
	}

	setup_ovl_sec(module, handle, has_sec_layer);
#else
	has_sec_layer = setup_ovl_sec(module, pConfig, handle);
#endif

	for (layer_id = 0; layer_id < TOTAL_OVL_LAYER_NUM; layer_id++) {
		struct OVL_CONFIG_STRUCT *ovl_cfg =
			&pConfig->ovl_config[layer_id];
		int enable = ovl_cfg->layer_en;

		if (enable == 0)
			continue;
		if (ovl_check_input_param(ovl_cfg)) {
			DDPAEE("invalid layer parameters!\n");
			continue;
		}
		if (ovl_cfg->ovl_index != module)
			continue;

		if (pConfig->ovl_partial_dirty) {
			struct disp_rect layer_roi = {0, 0, 0, 0};
			struct disp_rect layer_partial_roi = {0, 0, 0, 0};

			layer_roi.x = ovl_cfg->dst_x;
			layer_roi.y = ovl_cfg->dst_y;
			layer_roi.width = ovl_cfg->dst_w;
			layer_roi.height = ovl_cfg->dst_h;
			if (rect_intersect(&layer_roi,
					   &pConfig->ovl_partial_roi,
					   &layer_partial_roi)) {
				print_layer_config_args(module,
					ovl_cfg->phy_layer, ovl_cfg,
					&layer_partial_roi);
				ovl_layer_config(module, ovl_cfg->phy_layer,
						 has_sec_layer, ovl_cfg,
						 &pConfig->ovl_partial_roi,
						 &layer_partial_roi, handle);
			} else {
				/* this layer will not be displayed */
				enable = 0;
			}
		} else {
			print_layer_config_args(module, ovl_cfg->phy_layer,
						ovl_cfg, NULL);
			ovl_layer_config(module, ovl_cfg->phy_layer,
					 has_sec_layer, ovl_cfg, NULL, NULL,
					 handle);
		}

		if (ovl_cfg->ext_layer != -1) {
			enabled_ext_layers |= enable << ovl_cfg->ext_layer;
			ext_sel_layers |= ovl_cfg->phy_layer
					  << (16 + 4 * ovl_cfg->ext_layer);
		} else {
			enabled_layers |= enable << ovl_cfg->phy_layer;
		}
	}
	DDPDBG("ovl%d enabled_layers=0x%01x, enabled_ext_layers=0x%01x, ext_sel_layers=0x%04x\n",
	       module, enabled_layers, enabled_ext_layers,
	       ext_sel_layers >> 16);
	DISP_REG_SET(handle, ovl_base_addr(module) + DISP_REG_OVL_SRC_CON,
		     enabled_layers);
	/* ext layer control */
	DISP_REG_SET(handle,
		     ovl_base_addr(module) + DISP_REG_OVL_DATAPATH_EXT_CON,
		     enabled_ext_layers | ext_sel_layers);

#if 0
	/* IP ACTIVE ALGORITHM */
	if (pConfig->ovl_partial_dirty)
		ip_active_algorithm(pConfig->ovl_partial_roi->width,
				pConfig->ovl_partial_roi->height);
	else
		ip_active_algorithm(pConfig->dst_w, pConfig->dst_h);


	for (layer_id = 0; layer_id < TOTAL_OVL_LAYER_NUM; layer_id++) {
		struct OVL_CONFIG_STRUCT *ovl_cfg =
				&pConfig->ovl_config[layer_id];
		int enable = ovl_cfg->layer_en;

		if (enable == 0)
			continue;
		if (ovl_check_input_param(ovl_cfg)) {
			DDPAEE("invalid layer parameters!\n");
			continue;
		}
		if (ovl_cfg->ovl_index != module)
			continue;

		if (ovl_cfg->ext_layer == -1) {
			get_ip_active_layer_roi(ovl_cfg->phy_layer, 0, &tb, &bb);

			DISP_REG_SET(handle,
			ovl_base_addr(module) + DISP_REG_OVL_DVFS_L0_ROI +
				ovl_cfg->phy_layer*4,
			bb << 16 | tb);

		} else {
			get_ip_active_layer_roi(ovl_cfg->ext_layer,
				1, &tb, &bb);

			DISP_REG_SET(handle,
			ovl_base_addr(module) + DISP_REG_OVL_DVFS_EL0_ROI +
				ovl_cfg->ext_layer*4,
			bb << 16 | tb);
		}
	}
#endif
	return 0;
}

int ovl_build_cmdq(enum DISP_MODULE_ENUM module, void *cmdq_trigger_handle,
		   enum CMDQ_STATE state)
{
	int ret = 0;
	/* int reg_pa = DISP_REG_OVL_FLOW_CTRL_DBG & 0x1fffffff; */

	if (!cmdq_trigger_handle) {
		DDPERR("cmdq_trigger_handle is NULL\n");
		return -1;
	}

	if (state == CMDQ_CHECK_IDLE_AFTER_STREAM_EOF) {
		if (module == DISP_MODULE_OVL0) {
			ret = cmdqRecPoll(cmdq_trigger_handle, 0x14007240, 2,
					  0x3f);
		} else {
			DDPERR("wrong module: %s\n",
			       ddp_get_module_name(module));
			return -1;
		}
	}

	return 0;
}

static void ovl_printf_status(unsigned int status)
{
	DDPDUMP("- OVL_FLOW_CONTROL_DEBUG -\n");
	DDPDUMP("addcon_idle:%d,blend_idle:%d,out_valid:%d,out_ready:%d,out_idle:%d\n",
		(status >> 10) & (0x1), (status >> 11) & (0x1),
		(status >> 12) & (0x1), (status >> 13) & (0x1),
		(status >> 15) & (0x1));
	DDPDUMP("rdma3_idle:%d,rdma2_idle:%d,rdma1_idle:%d, rdma0_idle:%d,rst:%d\n",
		(status >> 16) & (0x1), (status >> 17) & (0x1),
		(status >> 18) & (0x1), (status >> 19) & (0x1),
		(status >> 20) & (0x1));
	DDPDUMP("trig:%d,frame_hwrst_done:%d,frame_swrst_done:%d,frame_underrun:%d,frame_done:%d\n",
		(status >> 21) & (0x1), (status >> 23) & (0x1),
		(status >> 24) & (0x1), (status >> 25) & (0x1),
		(status >> 26) & (0x1));
	DDPDUMP("ovl_running:%d,ovl_start:%d,ovl_clr:%d,reg_update:%d,ovl_upd_reg:%d\n",
		(status >> 27) & (0x1), (status >> 28) & (0x1),
		(status >> 29) & (0x1), (status >> 30) & (0x1),
		(status >> 31) & (0x1));

	DDPDUMP("ovl_fms_state:\n");
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
	DDPDUMP("wram_rst_cs:0x%x,layer_greq:0x%x,out_data:0x%x,", status & 0x7,
		(status >> 3) & 0x1, (status >> 4) & 0xffffff);
	DDPDUMP("out_ready:0x%x,out_valid:0x%x,smi_busy:0x%x,smi_greq:0x%x\n",
		(status >> 28) & 0x1, (status >> 29) & 0x1,
		(status >> 30) & 0x1, (status >> 31) & 0x1);
}

static void ovl_dump_layer_info(int layer, unsigned long layer_offset)
{
	enum UNIFIED_COLOR_FMT fmt;

	fmt = display_fmt_reg_to_unified_fmt(
		DISP_REG_GET_FIELD(L_CON_FLD_CFMT,
				   DISP_REG_OVL_L0_CON + layer_offset),
		DISP_REG_GET_FIELD(L_CON_FLD_BTSW,
				   DISP_REG_OVL_L0_CON + layer_offset),
		DISP_REG_GET_FIELD(L_CON_FLD_RGB_SWAP,
				   DISP_REG_OVL_L0_CON + layer_offset));

	DDPDUMP("layer%d: w=%d,h=%d,off(x=%d,y=%d),pitch=%d,addr=0x%x,fmt=%s,source=%s,aen=%d,alpha=%d\n",
	     layer, DISP_REG_GET(layer_offset +
			DISP_REG_OVL_L0_SRC_SIZE) & 0xfff,
	     (DISP_REG_GET(layer_offset +
			DISP_REG_OVL_L0_SRC_SIZE) >> 16) & 0xfff,
	     DISP_REG_GET(layer_offset +
			DISP_REG_OVL_L0_OFFSET) & 0xfff,
	     (DISP_REG_GET(layer_offset +
			DISP_REG_OVL_L0_OFFSET) >> 16) & 0xfff,
	     DISP_REG_GET(layer_offset +
			DISP_REG_OVL_L0_PITCH) & 0xffff,
	     DISP_REG_GET(layer_offset +
			DISP_REG_OVL_L0_ADDR), unified_color_fmt_name(fmt),
	     (DISP_REG_GET_FIELD(L_CON_FLD_LSRC,
			DISP_REG_OVL_L0_CON + layer_offset) ==
	      0) ? "memory" : "constant_color",
			DISP_REG_GET_FIELD(L_CON_FLD_AEN,
			   DISP_REG_OVL_L0_CON + layer_offset),
	     DISP_REG_GET_FIELD(L_CON_FLD_APHA,
			DISP_REG_OVL_L0_CON + layer_offset));
}

static void ovl_dump_ext_layer_info(int layer, unsigned long layer_offset)
{
	unsigned long layer_addr_offset;
	enum UNIFIED_COLOR_FMT fmt;

	layer_addr_offset = layer_offset - layer * OVL_LAYER_OFFSET + layer * 4;
	fmt = display_fmt_reg_to_unified_fmt(DISP_REG_GET_FIELD(L_CON_FLD_CFMT,
			DISP_REG_OVL_EL0_CON + layer_offset),
			DISP_REG_GET_FIELD(L_CON_FLD_BTSW,
			DISP_REG_OVL_EL0_CON + layer_offset),
			DISP_REG_GET_FIELD(L_CON_FLD_RGB_SWAP,
			DISP_REG_OVL_EL0_CON + layer_offset));

	DDPDUMP("ext layer%d: w=%d,h=%d,off(x=%d,y=%d),pitch=%d,addr=0x%x,fmt=%s,source=%s,aen=%d,alpha=%d\n",
	     layer,
	     DISP_REG_GET(layer_offset + DISP_REG_OVL_EL0_SRC_SIZE) & 0xfff,
	     (DISP_REG_GET(layer_offset + DISP_REG_OVL_EL0_SRC_SIZE) >> 16) &
		0xfff,
	     DISP_REG_GET(layer_offset + DISP_REG_OVL_EL0_OFFSET) & 0xfff,
	     (DISP_REG_GET(layer_offset + DISP_REG_OVL_EL0_OFFSET) >> 16) &
		0xfff,
	     DISP_REG_GET(layer_offset + DISP_REG_OVL_EL0_PITCH) & 0xffff,
	     DISP_REG_GET(layer_addr_offset + DISP_REG_OVL_EL0_ADDR),
		unified_color_fmt_name(fmt),
	     (DISP_REG_GET_FIELD(
		L_CON_FLD_LSRC, DISP_REG_OVL_EL0_CON + layer_offset) ==
	      0) ? "memory" : "constant_color",
	     DISP_REG_GET_FIELD(L_CON_FLD_AEN, DISP_REG_OVL_EL0_CON +
		layer_offset),
	     DISP_REG_GET_FIELD(L_CON_FLD_APHA, DISP_REG_OVL_EL0_CON +
		layer_offset));
}

void ovl_dump_analysis(enum DISP_MODULE_ENUM module)
{
	int i = 0;
	unsigned long layer_offset = 0;
	unsigned long rdma_offset = 0;
	unsigned long offset = ovl_base_addr(module);
	unsigned int src_con = DISP_REG_GET(DISP_REG_OVL_SRC_CON + offset);
	unsigned int ext_con = DISP_REG_GET(DISP_REG_OVL_DATAPATH_EXT_CON +
			 offset);

	DDPDUMP("== DISP %s ANALYSIS ==\n", ddp_get_module_name(module));
	DDPDUMP("ovl_en=%d,layer_enable(%d,%d,%d,%d),bg(w=%d, h=%d)\n",
		DISP_REG_GET(DISP_REG_OVL_EN + offset) & 0x1,
		src_con & 0x1, (src_con >> 1) & 0x1, (src_con >> 2) & 0x1,
		(src_con >> 3) & 0x1,
		DISP_REG_GET(DISP_REG_OVL_ROI_SIZE + offset) & 0xfff,
		(DISP_REG_GET(DISP_REG_OVL_ROI_SIZE + offset) >> 16) & 0xfff);
	DDPDUMP("ext layer: layer_enable(%d,%d,%d), attach_layer(%d,%d,%d)\n",
		ext_con & 0x1, (ext_con >> 1) & 0x1, (ext_con >> 2) & 0x1,
		(ext_con >> 16) & 0xf, (ext_con >> 20) & 0xf,
		(ext_con >> 24) & 0xf);
	DDPDUMP("cur_pos(x=%d,y=%d),layer_hit(%d,%d,%d,%d),bg_mode=%s,sta=0x%x\n",
		DISP_REG_GET_FIELD(ADDCON_DBG_FLD_ROI_X,
			DISP_REG_OVL_ADDCON_DBG + offset),
		DISP_REG_GET_FIELD(ADDCON_DBG_FLD_ROI_Y,
			DISP_REG_OVL_ADDCON_DBG + offset),
		DISP_REG_GET_FIELD(ADDCON_DBG_FLD_L0_WIN_HIT,
			DISP_REG_OVL_ADDCON_DBG + offset),
		DISP_REG_GET_FIELD(ADDCON_DBG_FLD_L1_WIN_HIT,
			DISP_REG_OVL_ADDCON_DBG + offset),
		DISP_REG_GET_FIELD(ADDCON_DBG_FLD_L2_WIN_HIT,
			DISP_REG_OVL_ADDCON_DBG + offset),
		DISP_REG_GET_FIELD(ADDCON_DBG_FLD_L3_WIN_HIT,
			DISP_REG_OVL_ADDCON_DBG + offset),
		DISP_REG_GET_FIELD(DATAPATH_CON_FLD_BGCLR_IN_SEL,
			DISP_REG_OVL_DATAPATH_CON + offset) ?
			"directlink" : "const",
		DISP_REG_GET(DISP_REG_OVL_STA + offset));

	for (i = 0; i < 4; i++) {
		unsigned int rdma_ctrl;

		layer_offset = i * OVL_LAYER_OFFSET + offset;
		rdma_offset = i * OVL_RDMA_DEBUG_OFFSET + offset;
		if (src_con & (0x1 << i))
			ovl_dump_layer_info(i, layer_offset);
		else
			DDPDUMP("layer%d: disabled\n", i);
		rdma_ctrl =
			DISP_REG_GET(layer_offset + DISP_REG_OVL_RDMA0_CTRL);
		DDPDUMP("ovl rdma%d status:(en=%d, fifo_used %d, GMC=0x%x)\n",
			i, REG_FLD_VAL_GET(RDMA0_CTRL_FLD_RDMA_EN, rdma_ctrl),
			REG_FLD_VAL_GET(RDMA0_CTRL_FLD_RMDA_FIFO_USED_SZ,
				rdma_ctrl),
			DISP_REG_GET(layer_offset +
				DISP_REG_OVL_RDMA0_MEM_GMC_SETTING));
		ovl_print_ovl_rdma_status(
			DISP_REG_GET(DISP_REG_OVL_RDMA0_DBG + rdma_offset));
	}
	/* ext layer detail info */
	for (i = 0; i < 3; i++) {
		layer_offset = i * OVL_LAYER_OFFSET + offset;
		rdma_offset = i * OVL_RDMA_DEBUG_OFFSET + offset;
		if (ext_con & (0x1 << i))
			ovl_dump_ext_layer_info(i, layer_offset);
		else
			DDPDUMP("ext layer%d: disabled\n", i);
	}
	ovl_printf_status(DISP_REG_GET(DISP_REG_OVL_FLOW_CTRL_DBG + offset));
}

int ovl_dump(enum DISP_MODULE_ENUM module, int level)
{
	ovl_dump_analysis(module);
	disp_ovl_dump_reg(module);

	return 0;
}

static int ovl_golden_setting(enum DISP_MODULE_ENUM module,
			      enum dst_module_type dst_mod_type, void *cmdq)
{
	unsigned long ovl_base = ovl_base_addr(module);
	unsigned int regval;
	int i, layer_num;
	int is_large_resolution = 0;
	unsigned int layer_greq_num;
	unsigned int dst_w, dst_h;

	layer_num = ovl_layer_num(module);

	dst_w = primary_display_get_width();
	dst_h = primary_display_get_height();

	if (dst_w > 1260 && dst_h > 2240) {
		/* WQHD */
		is_large_resolution = 1;
	} else {
		/* FHD */
		is_large_resolution = 0;
	}

	/* DISP_REG_OVL_RDMA0_MEM_GMC_SETTING */
	regval = REG_FLD_VAL(FLD_OVL_RDMA_MEM_GMC_ULTRA_THRESHOLD, 0x3ff);
	if (dst_mod_type == DST_MOD_REAL_TIME)
		regval |= REG_FLD_VAL(FLD_OVL_RDMA_MEM_GMC_PRE_ULTRA_THRESHOLD,
				      0x3ff);
	else
		regval |= REG_FLD_VAL(FLD_OVL_RDMA_MEM_GMC_PRE_ULTRA_THRESHOLD,
				      0xe0);

	for (i = 0; i < layer_num; i++) {
		unsigned long layer_offset = i * OVL_LAYER_OFFSET + ovl_base;

		DISP_REG_SET(cmdq, layer_offset +
			     DISP_REG_OVL_RDMA0_MEM_GMC_SETTING, regval);
	}

	/* DISP_REG_OVL_RDMA0_FIFO_CTRL */
	regval = REG_FLD_VAL(FLD_OVL_RDMA_FIFO_SIZE, 128);
	for (i = 0; i < layer_num; i++) {
		unsigned long layer_offset = i * OVL_LAYER_OFFSET + ovl_base;

		DISP_REG_SET(cmdq, layer_offset + DISP_REG_OVL_RDMA0_FIFO_CTRL,
			     regval);
	}

	/* DISP_REG_OVL_RDMA0_MEM_GMC_S2 */
	regval = 0;
	if (dst_mod_type == DST_MOD_REAL_TIME) {
		regval |= REG_FLD_VAL(FLD_OVL_RDMA_MEM_GMC2_ISSUE_REQ_THRES,
				      95);
		regval |= REG_FLD_VAL(FLD_OVL_RDMA_MEM_GMC2_ISSUE_REQ_THRES_URG,
				      47);
	} else {
		/* decouple */
		regval |= REG_FLD_VAL(FLD_OVL_RDMA_MEM_GMC2_ISSUE_REQ_THRES,
				      47);
		regval |= REG_FLD_VAL(FLD_OVL_RDMA_MEM_GMC2_ISSUE_REQ_THRES_URG,
				      47);
	}
	regval |= REG_FLD_VAL(FLD_OVL_RDMA_MEM_GMC2_REQ_THRES_PREULTRA, 0);
	regval |= REG_FLD_VAL(FLD_OVL_RDMA_MEM_GMC2_REQ_THRES_ULTRA, 1);
	regval |= REG_FLD_VAL(FLD_OVL_RDMA_MEM_GMC2_FORCE_REQ_THRES, 0);

	for (i = 0; i < layer_num; i++)
		DISP_REG_SET(cmdq, ovl_base + DISP_REG_OVL_RDMA0_MEM_GMC_S2 +
				i * 4, regval);

	/* DISP_REG_OVL_RDMA_GREQ_NUM */

	layer_greq_num = 5;

	regval = REG_FLD_VAL(FLD_OVL_RDMA_GREQ_LAYER0_GREQ_NUM, layer_greq_num);
	if (layer_num > 1)
		regval |= REG_FLD_VAL(FLD_OVL_RDMA_GREQ_LAYER1_GREQ_NUM,
				      layer_greq_num);
	if (layer_num > 2)
		regval |= REG_FLD_VAL(FLD_OVL_RDMA_GREQ_LAYER2_GREQ_NUM,
				      layer_greq_num);
	if (layer_num > 3)
		regval |= REG_FLD_VAL(FLD_OVL_RDMA_GREQ_LAYER3_GREQ_NUM,
				      layer_greq_num);

	regval |= REG_FLD_VAL(FLD_OVL_RDMA_GREQ_OSTD_GREQ_NUM, 0xff);
	regval |= REG_FLD_VAL(FLD_OVL_RDMA_GREQ_GREQ_DIS_CNT, 1);
	regval |= REG_FLD_VAL(FLD_OVL_RDMA_GREQ_STOP_EN, 0);
	regval |= REG_FLD_VAL(FLD_OVL_RDMA_GREQ_GRP_END_STOP, 0);
	regval |= REG_FLD_VAL(FLD_OVL_RDMA_GREQ_GRP_BRK_STOP, 0);
	regval |= REG_FLD_VAL(FLD_OVL_RDMA_GREQ_IOBUF_FLUSH_PREULTRA, 1);
	regval |= REG_FLD_VAL(FLD_OVL_RDMA_GREQ_IOBUF_FLUSH_ULTRA, 0);
	DISP_REG_SET(cmdq, ovl_base + DISP_REG_OVL_RDMA_GREQ_NUM, regval);

	/* DISP_REG_OVL_RDMA_GREQ_URG_NUM */
	regval = REG_FLD_VAL(FLD_OVL_RDMA_GREQ_LAYER0_GREQ_URG_NUM,
			     layer_greq_num);
	if (layer_num > 0)
		regval |= REG_FLD_VAL(FLD_OVL_RDMA_GREQ_LAYER1_GREQ_URG_NUM,
				      layer_greq_num);
	if (layer_num > 1)
		regval |= REG_FLD_VAL(FLD_OVL_RDMA_GREQ_LAYER2_GREQ_URG_NUM,
				      layer_greq_num);
	if (layer_num > 2)
		regval |= REG_FLD_VAL(FLD_OVL_RDMA_GREQ_LAYER3_GREQ_URG_NUM,
				      layer_greq_num);

	regval |= REG_FLD_VAL(FLD_OVL_RDMA_GREQ_ARG_GREQ_URG_TH, 0);
	regval |= REG_FLD_VAL(FLD_OVL_RDMA_GREQ_ARG_URG_BIAS, 0);
	regval |= REG_FLD_VAL(GREQ_NUM_SHT_VAL, 0);
	regval |= REG_FLD_VAL(GREQ_NUM_SHT, 0);
	DISP_REG_SET(cmdq, ovl_base + DISP_REG_OVL_RDMA_GREQ_URG_NUM, regval);

	/* DISP_REG_OVL_RDMA_ULTRA_SRC */
	regval = REG_FLD_VAL(FLD_OVL_RDMA_PREULTRA_BUF_SRC, 0);
	regval |= REG_FLD_VAL(FLD_OVL_RDMA_PREULTRA_SMI_SRC, 0);
	regval |= REG_FLD_VAL(FLD_OVL_RDMA_PREULTRA_ROI_END_SRC, 0);
	regval |= REG_FLD_VAL(FLD_OVL_RDMA_PREULTRA_RDMA_SRC, 1);
	regval |= REG_FLD_VAL(FLD_OVL_RDMA_ULTRA_BUF_SRC, 0);
	regval |= REG_FLD_VAL(FLD_OVL_RDMA_ULTRA_SMI_SRC, 0);
	if (dst_mod_type == DST_MOD_REAL_TIME)
		regval |= REG_FLD_VAL(FLD_OVL_RDMA_ULTRA_ROI_END_SRC, 0);
	else
		regval |= REG_FLD_VAL(FLD_OVL_RDMA_ULTRA_ROI_END_SRC, 2);
	regval |= REG_FLD_VAL(FLD_OVL_RDMA_ULTRA_RDMA_SRC, 2);

	DISP_REG_SET(cmdq, ovl_base + DISP_REG_OVL_RDMA_ULTRA_SRC, regval);

	/* DISP_REG_OVL_RDMAn_BUF_LOW */
	regval = REG_FLD_VAL(FLD_OVL_RDMA_BUF_LOW_ULTRA_TH, 0);
	if (dst_mod_type == DST_MOD_REAL_TIME)
		regval |= REG_FLD_VAL(FLD_OVL_RDMA_BUF_LOW_PREULTRA_TH, 0);
	else
		regval |= REG_FLD_VAL(FLD_OVL_RDMA_BUF_LOW_PREULTRA_TH, 0x10);

	for (i = 0; i < layer_num; i++)
		DISP_REG_SET(cmdq, ovl_base +
			     DISP_REG_OVL_RDMAn_BUF_LOW(i), regval);

	/* DISP_REG_OVL_RDMAn_BUF_HIGH */
	regval = REG_FLD_VAL(FLD_OVL_RDMA_BUF_HIGH_PREULTRA_DIS, 1);
	if (dst_mod_type == DST_MOD_REAL_TIME)
		regval |= REG_FLD_VAL(FLD_OVL_RDMA_BUF_HIGH_PREULTRA_TH, 0);
	else
		regval |= REG_FLD_VAL(FLD_OVL_RDMA_BUF_HIGH_PREULTRA_TH, 0x60);

	for (i = 0; i < layer_num; i++)
		DISP_REG_SET(cmdq, ovl_base +
			     DISP_REG_OVL_RDMAn_BUF_HIGH(i), regval);

	/* DISP_REG_OVL_FUNC_DCM0 */
	DISP_REG_SET(cmdq, ovl_base + DISP_REG_OVL_FUNC_DCM0, 0x0);
	/* DISP_REG_OVL_FUNC_DCM1 */
	DISP_REG_SET(cmdq, ovl_base + DISP_REG_OVL_FUNC_DCM1, 0x0);

	/* DISP_REG_OVL_DATAPATH_CON */
	/* GCLAST_EN is set @ ovl_start() */

	return 0;
}

int ovl_partial_update(enum DISP_MODULE_ENUM module, unsigned int bg_w,
		       unsigned int bg_h, void *handle)
{
	unsigned long ovl_base = ovl_base_addr(module);

	if ((bg_w > OVL_MAX_WIDTH) || (bg_h > OVL_MAX_HEIGHT)) {
		DDPERR("ovl_roi,exceed OVL max size, w=%d, h=%d\n", bg_w, bg_h);
		ASSERT(0);
	}
	DDPDBG("ovl%d partial update\n", module);
	DISP_REG_SET(handle, ovl_base + DISP_REG_OVL_ROI_SIZE,
		     bg_h << 16 | bg_w);

	return 0;
}

static int ovl_ioctl(enum DISP_MODULE_ENUM module, void *handle,
		     enum DDP_IOCTL_NAME ioctl_cmd, void *params)
{
	int ret = 0;

	if (ioctl_cmd == DDP_OVL_GOLDEN_SETTING) {
		struct ddp_io_golden_setting_arg *gset_arg = params;

		ovl_golden_setting(module, gset_arg->dst_mod_type, handle);
	} else if (ioctl_cmd == DDP_PARTIAL_UPDATE) {
		struct disp_rect *roi = (struct disp_rect *)params;

		ovl_partial_update(module, roi->width, roi->height, handle);
	} else {
		ret = -1;
	}

	return ret;
}

/* ---------------- driver ---------------- */
struct DDP_MODULE_DRIVER ddp_driver_ovl = {
	.init = ovl_init,
	.deinit = ovl_deinit,
	.config = ovl_config_l,
	.start = ovl_start,
	.trigger = NULL,
	.stop = ovl_stop,
	.reset = ovl_reset,
	.power_on = ovl_clock_on,
	.power_off = ovl_clock_off,
	.is_idle = NULL,
	.is_busy = NULL,
	.dump_info = ovl_dump,
	.bypass = NULL,
	.build_cmdq = NULL,
	.set_lcm_utils = NULL,
	.ioctl = ovl_ioctl,
	.connect = ovl_connect,
	.switch_to_nonsec = ovl_switch_to_nonsec,
};
