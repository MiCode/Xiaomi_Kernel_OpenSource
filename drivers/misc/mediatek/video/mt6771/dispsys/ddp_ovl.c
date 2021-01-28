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
#include "debug.h"
#include "disp_drv_platform.h"
#include "mtk_dramc.h"

#define OVL_REG_BACK_MAX	(40)
#define OVL_LAYER_OFFSET	(0x20)
#define OVL_RDMA_DEBUG_OFFSET	(0x4)

enum OVL_COLOR_SPACE {
	OVL_COLOR_SPACE_RGB = 0,
	OVL_COLOR_SPACE_YUV,
};

struct OVL_REG {
	unsigned long address;
	unsigned int value;
};

static enum DISP_MODULE_ENUM ovl_index_module[OVL_NUM] = {
	DISP_MODULE_OVL0, DISP_MODULE_OVL0_2L, DISP_MODULE_OVL1_2L
};

static unsigned int gOVL_bg_color = 0xFF000000;
static unsigned int gOVL_dim_color = 0xFF000000;

static unsigned int ovl_bg_w[OVL_NUM];
static unsigned int ovl_bg_h[OVL_NUM];
static enum DISP_MODULE_ENUM next_rsz_module = DISP_MODULE_UNKNOWN;
static enum DISP_MODULE_ENUM prev_rsz_module = DISP_MODULE_UNKNOWN;

unsigned int ovl_set_bg_color(unsigned int bg_color)
{
	unsigned int old = gOVL_bg_color;

	gOVL_bg_color = bg_color;

	return old;
}

unsigned int ovl_set_dim_color(unsigned int dim_color)
{
	unsigned int old = gOVL_dim_color;

	gOVL_dim_color = dim_color;

	return old;
}

static inline int is_module_ovl(enum DISP_MODULE_ENUM module)
{
	if (module == DISP_MODULE_OVL0 ||
	    module == DISP_MODULE_OVL0_2L || module == DISP_MODULE_OVL1_2L)
		return 1;

	return 0;
}

static inline bool is_module_rsz(enum DISP_MODULE_ENUM module)
{
	if (module == DISP_MODULE_RSZ0)
		return true;

	return false;
}

unsigned long ovl_base_addr(enum DISP_MODULE_ENUM module)
{
	switch (module) {
	case DISP_MODULE_OVL0:
		return DISPSYS_OVL0_BASE;
	case DISP_MODULE_OVL0_2L:
		return DISPSYS_OVL0_2L_BASE;
	case DISP_MODULE_OVL1_2L:
		return DISPSYS_OVL1_2L_BASE;
	default:
		DDPPR_ERR("invalid ovl module=%d\n", module);
		return -1;
	}
	return 0;
}

unsigned long mmsys_ovl_ultra_offset(enum DISP_MODULE_ENUM module)
{
	switch (module) {
	case DISP_MODULE_OVL0:
		return FLD_OVL0_ULTRA_SEL;
	case DISP_MODULE_OVL0_2L:
		return FLD_OVL0_2L_ULTRA_SEL;
	case DISP_MODULE_OVL1_2L:
		return FLD_OVL1_2L_ULTRA_SEL;
	default:
		DDPPR_ERR("invalid ovl module=%d\n", module);
		return -1;
	}
	return 0;
}

static inline int ovl_layer_num(enum DISP_MODULE_ENUM module)
{
	switch (module) {
	case DISP_MODULE_OVL0:
		return 4;
	case DISP_MODULE_OVL0_2L:
		return 2;
	case DISP_MODULE_OVL1_2L:
		return 2;
	default:
		DDPPR_ERR("invalid ovl module=%d\n", module);
		return -1;
	}
	return 0;
}

enum CMDQ_EVENT_ENUM ovl_to_cmdq_event_nonsec_end(enum DISP_MODULE_ENUM module)
{
	switch (module) {
	case DISP_MODULE_OVL0:
		return CMDQ_SYNC_DISP_OVL0_2NONSEC_END;
	case DISP_MODULE_OVL0_2L:
		return CMDQ_SYNC_DISP_2LOVL0_2NONSEC_END;
	case DISP_MODULE_OVL1_2L:
		return CMDQ_SYNC_DISP_2LOVL1_2NONSEC_END;
	default:
		DDPPR_ERR("invalid ovl module=%d, get cmdq event nonsecure fail\n",
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
	case DISP_MODULE_OVL0_2L:
		return CMDQ_ENG_DISP_2L_OVL0;
	case DISP_MODULE_OVL1_2L:
		return CMDQ_ENG_DISP_2L_OVL1;
	default:
		DDPPR_ERR("invalid ovl module=%d, get cmdq engine fail\n",
			module);
		ASSERT(0);
		return DISP_MODULE_UNKNOWN;
	}
	return 0;
}

unsigned int ovl_to_index(enum DISP_MODULE_ENUM module)
{
	unsigned int i;

	for (i = 0; i < OVL_NUM; i++) {
		if (ovl_index_module[i] == module)
			return i;
	}
	DDPPR_ERR("invalid ovl module=%d, get ovl index fail\n", module);
	ASSERT(0);
	return 0;
}

static inline enum DISP_MODULE_ENUM ovl_index_to_module(int index)
{
	if (unlikely((unsigned int)index >= OVL_NUM)) {
		DDPPR_ERR("invalid ovl index=%d\n", index);
		ASSERT(0);
		return -EINVAL;
	}

	return ovl_index_module[index];
}

enum DISP_MODULE_ENUM ovl_index_to_mod_for_debug(int index)
{
	int ovl_index;

	ovl_index = ovl_index_to_module(index);
	return ovl_index;
}

int ovl_layer_num_for_debug(enum DISP_MODULE_ENUM module)
{
	int layer_num;

	layer_num = ovl_layer_num(module);
	return layer_num;
}

int ovl_start(enum DISP_MODULE_ENUM module, void *handle)
{
	unsigned long ovl_base = ovl_base_addr(module);

	if (disp_helper_get_option(DISP_OPT_SHADOW_REGISTER)) {
		if (disp_helper_get_option(DISP_OPT_SHADOW_MODE) == 1) {
			/* force commit: force_commit, read working */
			DISP_REG_SET_FIELD(handle, EN_FLD_FORCE_COMMIT,
				ovl_base + DISP_REG_OVL_EN, 0x1);
			DISP_REG_SET_FIELD(handle, EN_FLD_BYPASS_SHADOW,
				ovl_base + DISP_REG_OVL_EN, 0x0);
			DISP_REG_SET_FIELD(handle, EN_FLD_RD_WRK_REG,
				ovl_base + DISP_REG_OVL_EN, 0x0);
		} else if (disp_helper_get_option(DISP_OPT_SHADOW_MODE) == 2) {
			/* bypass shadow: bypass_shadow, read working */
			DISP_REG_SET_FIELD(handle, EN_FLD_BYPASS_SHADOW,
				ovl_base + DISP_REG_OVL_EN, 0x1);
			DISP_REG_SET_FIELD(handle, EN_FLD_FORCE_COMMIT,
				ovl_base + DISP_REG_OVL_EN, 0x0);
			DISP_REG_SET_FIELD(handle, EN_FLD_RD_WRK_REG,
				ovl_base + DISP_REG_OVL_EN, 0x0);
		}
	}
	DISP_REG_SET_FIELD(handle, EN_FLD_OVL_EN,
			   ovl_base + DISP_REG_OVL_EN, 0x1);

	DISP_REG_SET(handle, ovl_base + DISP_REG_OVL_INTEN,
		     0x1E0 | REG_FLD_VAL(INTEN_FLD_ABNORMAL_SOF, 1) |
		     REG_FLD_VAL(INTEN_FLD_START_INTEN, 1));

#if (defined(CONFIG_MTK_TEE_GP_SUPPORT) || \
	defined(CONFIG_TRUSTONIC_TEE_SUPPORT)) && \
	defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
	DISP_REG_SET_FIELD(handle, INTEN_FLD_FME_CPL_INTEN,
			ovl_base + DISP_REG_OVL_INTEN, 1);
#endif
	DISP_REG_SET_FIELD(handle, DATAPATH_CON_FLD_LAYER_SMI_ID_EN,
			   ovl_base + DISP_REG_OVL_DATAPATH_CON, 1);
	DISP_REG_SET_FIELD(handle, DATAPATH_CON_FLD_OUTPUT_NO_RND,
			   ovl_base + DISP_REG_OVL_DATAPATH_CON, 0x0);
	DISP_REG_SET_FIELD(handle, DATAPATH_CON_FLD_GCLAST_EN,
			   ovl_base + DISP_REG_OVL_DATAPATH_CON, 1);
	DISP_REG_SET_FIELD(handle, DATAPATH_CON_FLD_OUTPUT_CLAMP,
			   ovl_base + DISP_REG_OVL_DATAPATH_CON, 1);
	return 0;
}

void ovl_dump_golden_setting(enum DISP_MODULE_ENUM module)
{
	unsigned long ovl_base = ovl_base_addr(module);
	int i, layer_num;

	layer_num = ovl_layer_num(module);

	DDPDUMP("DUMP %s golden_setting\n", ddp_get_module_name(module));

	for (i = 0; i < layer_num; i++) {
		unsigned long offset = i * OVL_LAYER_OFFSET + ovl_base;

		DDPDUMP("RDMA%d_MEM_GMC_SETTING1\n", i);
		DDPDUMP("[9:0]:%x [25:16]:%x [28]:%x [31]:%x\n",
			DISP_REG_GET_FIELD(
			    FLD_OVL_RDMA_MEM_GMC_ULTRA_THRESHOLD,
			    offset + DISP_REG_OVL_RDMA0_MEM_GMC_SETTING),
			DISP_REG_GET_FIELD(
			    FLD_OVL_RDMA_MEM_GMC_PRE_ULTRA_THRESHOLD,
			    offset + DISP_REG_OVL_RDMA0_MEM_GMC_SETTING),
			DISP_REG_GET_FIELD(
			    FLD_OVL_RDMA_MEM_GMC_ULTRA_THRESHOLD_HIGH_OFS,
			    offset + DISP_REG_OVL_RDMA0_MEM_GMC_SETTING),
			DISP_REG_GET_FIELD(
			    FLD_OVL_RDMA_MEM_GMC_PRE_ULTRA_THRESHOLD_HIGH_OFS,
			    offset + DISP_REG_OVL_RDMA0_MEM_GMC_SETTING));
	}

	for (i = 0; i < layer_num; i++) {
		unsigned long layer_offset = i * OVL_LAYER_OFFSET + ovl_base;

		DDPDUMP("RDMA%d_FIFO_CTRL\n", i);
		DDPDUMP("[9:0]:%u [25:16]:%u\n",
			DISP_REG_GET_FIELD(FLD_OVL_RDMA_FIFO_THRD,
				layer_offset + DISP_REG_OVL_RDMA0_FIFO_CTRL),
			DISP_REG_GET_FIELD(FLD_OVL_RDMA_FIFO_SIZE,
				layer_offset + DISP_REG_OVL_RDMA0_FIFO_CTRL));
	}

	for (i = 0; i < layer_num; i++) {
		unsigned long offset = i * 4 + ovl_base;

		DDPDUMP("RDMA%d_MEM_GMC_SETTING2\n", i);
		DDPDUMP("[11:0]:%u [27:16]:%u [28]:%u [29]:%u [30]:%u\n",
			DISP_REG_GET_FIELD(
				FLD_OVL_RDMA_MEM_GMC2_ISSUE_REQ_THRES,
				DISP_REG_OVL_RDMA0_MEM_GMC_S2 + offset),
			DISP_REG_GET_FIELD(
				FLD_OVL_RDMA_MEM_GMC2_ISSUE_REQ_THRES_URG,
				DISP_REG_OVL_RDMA0_MEM_GMC_S2 + offset),
			DISP_REG_GET_FIELD(
				FLD_OVL_RDMA_MEM_GMC2_REQ_THRES_PREULTRA,
				DISP_REG_OVL_RDMA0_MEM_GMC_S2 + offset),
			DISP_REG_GET_FIELD(
				FLD_OVL_RDMA_MEM_GMC2_REQ_THRES_ULTRA,
				DISP_REG_OVL_RDMA0_MEM_GMC_S2 + offset),
			DISP_REG_GET_FIELD(
				FLD_OVL_RDMA_MEM_GMC2_FORCE_REQ_THRES,
				DISP_REG_OVL_RDMA0_MEM_GMC_S2 + offset));
	}

	DDPDUMP("RDMA_GREQ_NUM\n");
	DDPDUMP("[3:0]%u [7:4]%u [11:8]%u [15:12]%u [23:16]%x [26:24]%u [27]%u [28]%u [29]%u [30]%u [31]%u\n",
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_GREQ_LAYER0_GREQ_NUM,
			ovl_base + DISP_REG_OVL_RDMA_GREQ_NUM),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_GREQ_LAYER1_GREQ_NUM,
			ovl_base + DISP_REG_OVL_RDMA_GREQ_NUM),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_GREQ_LAYER2_GREQ_NUM,
			ovl_base + DISP_REG_OVL_RDMA_GREQ_NUM),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_GREQ_LAYER3_GREQ_NUM,
			ovl_base + DISP_REG_OVL_RDMA_GREQ_NUM),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_GREQ_OSTD_GREQ_NUM,
			ovl_base + DISP_REG_OVL_RDMA_GREQ_NUM),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_GREQ_GREQ_DIS_CNT,
			ovl_base + DISP_REG_OVL_RDMA_GREQ_NUM),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_GREQ_STOP_EN,
			ovl_base + DISP_REG_OVL_RDMA_GREQ_NUM),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_GREQ_GRP_END_STOP,
			ovl_base + DISP_REG_OVL_RDMA_GREQ_NUM),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_GREQ_GRP_BRK_STOP,
			ovl_base + DISP_REG_OVL_RDMA_GREQ_NUM),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_GREQ_IOBUF_FLUSH_PREULTRA,
			ovl_base + DISP_REG_OVL_RDMA_GREQ_NUM),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_GREQ_IOBUF_FLUSH_ULTRA,
				ovl_base + DISP_REG_OVL_RDMA_GREQ_NUM));
	DDPDUMP("RDMA_GREQ_URG_NUM\n");
	DDPDUMP("[3:0]:%u [7:4]:%u [11:8]:%u [15:12]:%u [25:16]:%u [28]:%u [29]:%u [31:30]:%u\n",
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_GREQ_LAYER0_GREQ_URG_NUM,
			ovl_base + DISP_REG_OVL_RDMA_GREQ_URG_NUM),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_GREQ_LAYER1_GREQ_URG_NUM,
			ovl_base + DISP_REG_OVL_RDMA_GREQ_URG_NUM),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_GREQ_LAYER2_GREQ_URG_NUM,
			ovl_base + DISP_REG_OVL_RDMA_GREQ_URG_NUM),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_GREQ_LAYER3_GREQ_URG_NUM,
			ovl_base + DISP_REG_OVL_RDMA_GREQ_URG_NUM),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_GREQ_ARG_GREQ_URG_TH,
			ovl_base + DISP_REG_OVL_RDMA_GREQ_URG_NUM),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_GREQ_ARG_URG_BIAS,
			ovl_base + DISP_REG_OVL_RDMA_GREQ_URG_NUM),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_GREQ_NUM_SHT_VAL,
			ovl_base + DISP_REG_OVL_RDMA_GREQ_URG_NUM),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_GREQ_NUM_SHT,
			ovl_base + DISP_REG_OVL_RDMA_GREQ_URG_NUM));
	DDPDUMP("RDMA_ULTRA_SRC\n");
	DDPDUMP("[1:0]%u [3:2]%u [5:4]%u [7:6]%u [9:8]%u [11:10]%u [13:12]%u [15:14]%u\n",
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_PREULTRA_BUF_SRC,
			ovl_base + DISP_REG_OVL_RDMA_ULTRA_SRC),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_PREULTRA_SMI_SRC,
			ovl_base + DISP_REG_OVL_RDMA_ULTRA_SRC),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_PREULTRA_ROI_END_SRC,
			ovl_base + DISP_REG_OVL_RDMA_ULTRA_SRC),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_PREULTRA_RDMA_SRC,
			ovl_base + DISP_REG_OVL_RDMA_ULTRA_SRC),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_ULTRA_BUF_SRC,
			ovl_base + DISP_REG_OVL_RDMA_ULTRA_SRC),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_ULTRA_SMI_SRC,
			ovl_base + DISP_REG_OVL_RDMA_ULTRA_SRC),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_ULTRA_ROI_END_SRC,
			ovl_base + DISP_REG_OVL_RDMA_ULTRA_SRC),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_ULTRA_RDMA_SRC,
			ovl_base + DISP_REG_OVL_RDMA_ULTRA_SRC));

	for (i = 0; i < layer_num; i++) {
		DDPDUMP("RDMA%d_BUF_LOW\n", i);
		DDPDUMP("[11:0]:%x [23:12]:%x\n",
			DISP_REG_GET_FIELD(FLD_OVL_RDMA_BUF_LOW_ULTRA_TH,
				ovl_base + DISP_REG_OVL_RDMAn_BUF_LOW(i)),
			DISP_REG_GET_FIELD(FLD_OVL_RDMA_BUF_LOW_PREULTRA_TH,
				ovl_base + DISP_REG_OVL_RDMAn_BUF_LOW(i)));
	}

	for (i = 0; i < layer_num; i++) {
		DDPDUMP("RDMA%d_BUF_HIGH\n", i);
		DDPDUMP("[23:12]:%x [31]:%x\n",
			DISP_REG_GET_FIELD(FLD_OVL_RDMA_BUF_HIGH_PREULTRA_TH,
				ovl_base + DISP_REG_OVL_RDMAn_BUF_HIGH(i)),
			DISP_REG_GET_FIELD(FLD_OVL_RDMA_BUF_HIGH_PREULTRA_DIS,
				ovl_base + DISP_REG_OVL_RDMAn_BUF_HIGH(i)));
	}

	DDPDUMP("DATAPATH_CON\n");
	DDPDUMP("[0]:%u, [3]:%u [24]:%u\n",
		DISP_REG_GET_FIELD(DATAPATH_CON_FLD_LAYER_SMI_ID_EN,
			ovl_base + DISP_REG_OVL_DATAPATH_CON),
		DISP_REG_GET_FIELD(DATAPATH_CON_FLD_OUTPUT_NO_RND,
			ovl_base + DISP_REG_OVL_DATAPATH_CON),
		DISP_REG_GET_FIELD(DATAPATH_CON_FLD_GCLAST_EN,
			ovl_base + DISP_REG_OVL_DATAPATH_CON));

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
			DDPPR_ERR("%s reset timeout!\n",
			       ddp_get_module_name(module));
			ret = -1;
			break;
		}
	}
	return ret;
}

static void _store_roi(enum DISP_MODULE_ENUM module,
		       unsigned int bg_w, unsigned int bg_h)
{
	int idx = ovl_to_index(module);

	if (idx >= OVL_NUM)
		return;

	ovl_bg_w[idx] = bg_w;
	ovl_bg_h[idx] = bg_h;
}

static void _get_roi(enum DISP_MODULE_ENUM module,
		     unsigned int *bg_w, unsigned int *bg_h)
{
	int idx = ovl_to_index(module);

	if (idx >= OVL_NUM)
		return;

	*bg_w = ovl_bg_w[idx];
	*bg_h = ovl_bg_h[idx];
}

int ovl_roi(enum DISP_MODULE_ENUM module, unsigned int bg_w, unsigned int bg_h,
	    unsigned int bg_color, void *handle)
{
	unsigned long ovl_base = ovl_base_addr(module);

	if ((bg_w > OVL_MAX_WIDTH) || (bg_h > OVL_MAX_HEIGHT)) {
		DDPPR_ERR("%s,exceed OVL max size, wh(%ux%u)\n",
			__func__, bg_w, bg_h);
		ASSERT(0);
	}

	DISP_REG_SET(handle, ovl_base + DISP_REG_OVL_ROI_SIZE,
			bg_h << 16 | bg_w);

	DISP_REG_SET(handle, ovl_base + DISP_REG_OVL_ROI_BGCLR,
			bg_color);

	DISP_REG_SET_FIELD(handle, FLD_OVL_LC_SRC_W,
			ovl_base + DISP_REG_OVL_LC_SRC_SIZE, bg_w);
	DISP_REG_SET_FIELD(handle, FLD_OVL_LC_SRC_H,
			ovl_base + DISP_REG_OVL_LC_SRC_SIZE, bg_h);

	_store_roi(module, bg_w, bg_h);

	DDPMSG("%s roi:(%ux%u)\n", ddp_get_module_name(module), bg_w, bg_h);
	return 0;
}

static int _ovl_get_rsz_layer_roi(const struct OVL_CONFIG_STRUCT * const oc,
				u32 *dst_x, u32 *dst_y, u32 *dst_w, u32 *dst_h,
				struct disp_rect src_total_roi)
{
	if (oc->src_w > oc->dst_w || oc->src_h > oc->dst_h) {
		DDPERR("%s:L%u:src(%ux%u)>dst(%ux%u)\n", __func__, oc->layer,
		       oc->src_w, oc->src_h, oc->dst_w, oc->dst_h);
		return -EINVAL;
	}

	if (oc->src_w < oc->dst_w || oc->src_h < oc->dst_h) {
		*dst_x =
			((oc->dst_x * oc->src_w) / oc->dst_w) - src_total_roi.x;
		*dst_y =
			((oc->dst_y * oc->src_h) / oc->dst_h) - src_total_roi.y;
		*dst_w = oc->src_w;
		*dst_h = oc->src_h;
	}

	if (oc->src_w != oc->dst_w || oc->src_h != oc->dst_h) {
		DDPDBG("%s:L%u:(%u,%u,%ux%u)->(%u,%u,%ux%u)\n", __func__,
		       oc->layer, *dst_x, *dst_y, *dst_w, *dst_h,
		       oc->dst_x, oc->dst_y, oc->dst_w, oc->dst_h);
	}

	return 0;
}

static int _ovl_set_rsz_roi(enum DISP_MODULE_ENUM module,
			    struct disp_ddp_path_config *pconfig, void *handle)
{
	struct disp_rect rsz_src_roi = pconfig->rsz_src_roi;
	u32 bg_w, bg_h;

	if (pconfig->rsz_enable) {
		bg_w = rsz_src_roi.width;
		bg_h = rsz_src_roi.height;
	} else {
		bg_w = pconfig->dst_w;
		bg_h = pconfig->dst_h;
	}

	ovl_roi(module, bg_w, bg_h, gOVL_bg_color, handle);
	DDPDBG("[RPO] module=%s, bg(w,h)=(%d,%d)\n",
			ddp_get_module_name(module), bg_w, bg_h);

	return 0;
}

static int _ovl_lc_config(enum DISP_MODULE_ENUM module,
			  struct disp_ddp_path_config *pconfig, void *handle)
{
	unsigned long ovl_base = ovl_base_addr(module);
	struct disp_rect rsz_dst_roi = pconfig->rsz_dst_roi;
	u32 lc_x, lc_y, lc_w, lc_h;

	if (pconfig->rsz_enable) {
		lc_x = rsz_dst_roi.x;
		lc_y = rsz_dst_roi.y;
		lc_w = rsz_dst_roi.width;
		lc_h = rsz_dst_roi.height;
	} else {
		lc_x = 0;
		lc_y = 0;
		lc_w = pconfig->dst_w;
		lc_h = pconfig->dst_h;
	}

	DISP_REG_SET_FIELD(handle, FLD_OVL_LC_XOFF,
			   ovl_base + DISP_REG_OVL_LC_OFFSET, lc_x);
	DISP_REG_SET_FIELD(handle, FLD_OVL_LC_YOFF,
			   ovl_base + DISP_REG_OVL_LC_OFFSET, lc_y);
	DISP_REG_SET_FIELD(handle, FLD_OVL_LC_SRC_W,
			   ovl_base + DISP_REG_OVL_LC_SRC_SIZE, lc_w);
	DISP_REG_SET_FIELD(handle, FLD_OVL_LC_SRC_H,
			   ovl_base + DISP_REG_OVL_LC_SRC_SIZE, lc_h);

	DDPDBG("[RPO] module=%s,lc(x,y,w,h)=(%d,%d,%d,%d)\n",
			ddp_get_module_name(module), lc_x, lc_y, lc_w, lc_h);

	return 0;
}

/* only disable L0 dim layer if RPO */
static void _rpo_disable_dim_L0(enum DISP_MODULE_ENUM module,
				struct disp_ddp_path_config *pconfig,
				int *en_layers)
{
	struct OVL_CONFIG_STRUCT *c = NULL;

	c = &pconfig->ovl_config[0];
	if (c->ovl_index != module)
		return;
	if (!(c->layer_en && c->source == OVL_LAYER_SOURCE_RESERVED))
		return;

	c = &pconfig->ovl_config[1];
	if (!(c->layer_en && ((c->src_w < c->dst_w) || (c->src_h < c->dst_h))))
		return;

	c = &pconfig->ovl_config[0];
	*en_layers &= ~(c->layer_en << c->phy_layer);
}

int disable_ovl_layers(enum DISP_MODULE_ENUM module, void *handle)
{
	unsigned long ovl_base = ovl_base_addr(module);

	/* physical layer control */
	DISP_REG_SET_FIELD(handle, SRC_CON_FLD_L_EN,
			ovl_base + DISP_REG_OVL_SRC_CON, 0);
	/* ext layer control */
	DISP_REG_SET(handle, ovl_base + DISP_REG_OVL_DATAPATH_EXT_CON, 0);
	DDPSVPMSG("[SVP] switch %s to nonsec: disable all layers first!\n",
		  ddp_get_module_name(module));
	return 0;
}

int ovl_layer_switch(enum DISP_MODULE_ENUM module, unsigned int layer,
		     unsigned int en, void *handle)
{
	unsigned long ovl_base = ovl_base_addr(module);

	ASSERT(layer <= 3);
	switch (layer) {
	case 0:
		DISP_REG_SET_FIELD(handle, SRC_CON_FLD_L0_EN, ovl_base +
				DISP_REG_OVL_SRC_CON, en);
		DISP_REG_SET(handle, ovl_base + DISP_REG_OVL_RDMA0_CTRL, en);
		break;
	case 1:
		DISP_REG_SET_FIELD(handle, SRC_CON_FLD_L1_EN, ovl_base +
				DISP_REG_OVL_SRC_CON, en);
		DISP_REG_SET(handle, ovl_base + DISP_REG_OVL_RDMA1_CTRL, en);
		break;
	case 2:
		DISP_REG_SET_FIELD(handle, SRC_CON_FLD_L2_EN, ovl_base +
				DISP_REG_OVL_SRC_CON, en);
		DISP_REG_SET(handle, ovl_base + DISP_REG_OVL_RDMA2_CTRL, en);
		break;
	case 3:
		DISP_REG_SET_FIELD(handle, SRC_CON_FLD_L3_EN, ovl_base +
				DISP_REG_OVL_SRC_CON, en);
		DISP_REG_SET(handle, ovl_base + DISP_REG_OVL_RDMA3_CTRL, en);
		break;
	default:
		DDPPR_ERR("invalid layer=%d\n", layer);
		ASSERT(0);
		break;
	}

	return 0;
}

static int ovl_layer_config(enum DISP_MODULE_ENUM module, unsigned int layer,
			    struct disp_rect src_total_roi,
			    unsigned int is_engine_sec,
			    const struct OVL_CONFIG_STRUCT * const cfg,
			    const struct disp_rect * const ovl_partial_roi,
			    const struct disp_rect * const layer_partial_roi,
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

		DDPDBG("layer partial (%d,%d)(%d,%d,%dx%d) to (%d,%d)(%d,%d,%dx%d)\n",
		       cfg->src_x, cfg->src_y, cfg->dst_x, cfg->dst_y,
		       cfg->dst_w, cfg->dst_h, src_x, src_y, dst_x, dst_y,
		       dst_w, dst_h);
	}

	if (dst_w > OVL_MAX_WIDTH)
		ASSERT(dst_w < OVL_MAX_WIDTH);
	if (dst_h > OVL_MAX_HEIGHT)
		ASSERT(dst_h < OVL_MAX_HEIGHT);

	if (!cfg->addr && cfg->source == OVL_LAYER_SOURCE_MEM) {
		DDPPR_ERR("source from memory, but addr is 0!\n");
		ASSERT(0);
		return -1;
	}

	_ovl_get_rsz_layer_roi(cfg, &dst_x, &dst_y, &dst_w,
			&dst_h, src_total_roi);

#ifdef CONFIG_MTK_LCM_PHYSICAL_ROTATION_HW
	if (module != DISP_MODULE_OVL1_2L)
		rotate = 1;
#endif

	/* check dim layer fmt */
	if (cfg->source == OVL_LAYER_SOURCE_RESERVED) {
		if (cfg->aen == 0)
			DDPPR_ERR("dim layer%d ahpha enable should be 1!\n",
				layer);
		format = UFMT_RGB888;
	}
	Bpp = ufmt_get_Bpp(format);
	input_swap = ufmt_get_byteswap(format);
	input_fmt = ufmt_get_format(format);
	is_rgb = ufmt_get_rgb(format);

	if (rotate) {
		unsigned int bg_w = 0, bg_h = 0;

		_get_roi(module, &bg_w, &bg_h);
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
		DISP_REG_SET(handle, DISP_REG_OVL_L0_CLIP + layer_offset,
				0);
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
		DDPPR_ERR("un-recognized yuv_range=%d!\n", cfg->yuv_range);
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

	if (cfg->source != OVL_LAYER_SOURCE_RESERVED)
		DISP_REG_SET(handle, DISP_REG_OVL_L0_CLR + layer_offset_clr,
				0xff000000);
	else {
		unsigned int dim_color;

		dim_color = (gOVL_dim_color == 0xff000000) ?
			cfg->dim_color : gOVL_dim_color;
		DISP_REG_SET(handle, DISP_REG_OVL_L0_CLR + layer_offset_clr,
			0xff000000 | dim_color);
	}

	DISP_REG_SET(handle, DISP_REG_OVL_L0_SRC_SIZE + layer_offset,
			dst_h << 16 | dst_w);

	if (rotate) {
		offset = (src_x + dst_w) * Bpp + (src_y + dst_h - 1) *
			cfg->src_pitch - 1;
	} else
		offset = src_x * Bpp + src_y * cfg->src_pitch;

	if (!is_engine_sec) {
		DISP_REG_SET(handle, DISP_REG_OVL_L0_ADDR + layer_offset_addr,
			     cfg->addr + offset);
	} else {
#ifdef MTKFB_M4U_SUPPORT
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
#endif
	}
	DISP_REG_SET(handle, DISP_REG_OVL_L0_SRCKEY + layer_offset, cfg->key);

	value = REG_FLD_VAL(L_PITCH_FLD_SA_SEL, cfg->src_alpha & 0x3) |
		REG_FLD_VAL(L_PITCH_FLD_SRGB_SEL, cfg->src_alpha & 0x3) |
		REG_FLD_VAL(L_PITCH_FLD_DA_SEL, cfg->dst_alpha & 0x3) |
		REG_FLD_VAL(L_PITCH_FLD_DRGB_SEL, cfg->dst_alpha & 0x3) |
		REG_FLD_VAL(L_PITCH_FLD_SURFL_EN, cfg->src_alpha & 0x1) |
		REG_FLD_VAL(L_PITCH_FLD_SRC_PITCH, cfg->src_pitch);
	if (format == UFMT_RGBA4444) {
		value |= REG_FLD_VAL(L_PITCH_FLD_SRGB_SEL, (1)) |
			REG_FLD_VAL(L_PITCH_FLD_DRGB_SEL, (2)) |
			REG_FLD_VAL(L_PITCH_FLD_SURFL_EN, (1));
	}

	if (cfg->const_bld)
		value |= REG_FLD_VAL((L_PITCH_FLD_CONST_BLD), (1));
	DISP_REG_SET(handle, DISP_REG_OVL_L0_PITCH + layer_offset, value);

	return 0;
}

int ovl_clock_on(enum DISP_MODULE_ENUM module, void *handle)
{
	DDPDBG("%s clock_on\n", ddp_get_module_name(module));
#ifdef ENABLE_CLK_MGR
	ddp_clk_prepare_enable(ddp_get_module_clk_id(module));
#endif
	return 0;
}

int ovl_clock_off(enum DISP_MODULE_ENUM module, void *handle)
{
	DDPDBG("%s clock_off\n", ddp_get_module_name(module));
#ifdef ENABLE_CLK_MGR
	ddp_clk_disable_unprepare(ddp_get_module_clk_id(module));
#endif
	return 0;
}

int ovl_init(enum DISP_MODULE_ENUM module, void *handle)
{
	return ovl_clock_on(module, handle);
}

int ovl_deinit(enum DISP_MODULE_ENUM module, void *handle)
{
	return ovl_clock_off(module, handle);
}

static int _ovl_UFOd_in(enum DISP_MODULE_ENUM module, int connect, void *handle)
{
	unsigned long ovl_base = ovl_base_addr(module);

	if (!connect) {
		DISP_REG_SET_FIELD(handle, SRC_CON_FLD_LC_EN,
			ovl_base + DISP_REG_OVL_SRC_CON, 0);
		DISP_REG_SET_FIELD(handle, L_CON_FLD_LSRC,
			ovl_base + DISP_REG_OVL_LC_CON, 0);
		return 0;
	}

	DISP_REG_SET_FIELD(handle, L_CON_FLD_LSRC,
		ovl_base + DISP_REG_OVL_LC_CON, 2);
	DISP_REG_SET_FIELD(handle, LC_SRC_SEL_FLD_L_SEL,
		ovl_base + DISP_REG_OVL_LC_SRC_SEL, 0);
	DISP_REG_SET_FIELD(handle, L_CON_FLD_AEN,
		ovl_base + DISP_REG_OVL_LC_CON, 0);
	DISP_REG_SET_FIELD(handle, SRC_CON_FLD_LC_EN,
		ovl_base + DISP_REG_OVL_SRC_CON, 1);

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

	if (connect && is_module_rsz(prev)) {
		_ovl_UFOd_in(module, 1, handle);
		next_rsz_module = module;
	} else
		_ovl_UFOd_in(module, 0, handle);

	if (connect && is_module_rsz(next))
		prev_rsz_module = module;

	DISP_REG_SET_FIELD(handle, DATAPATH_CON_FLD_OUTPUT_CLAMP,
			   ovl_base + DISP_REG_OVL_DATAPATH_CON, 1);

	return 0;
}

unsigned int ddp_ovl_get_cur_addr(bool rdma_mode, int layerid)
{
	/* just workaround, should remove this func */
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
	int i = 0;
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
			DDPMSG("%s:layer%d,en %d,w %d,h %d,bpp %d,addr %lu\n",
			       __func__, i, p->layer_en, p->src_w, p->src_h,
			       p->bpp, p->addr);
			continue;
		}

		val = DISP_REG_GET(DISP_REG_OVL_L0_CON + layer_off);
		p->fmt = display_fmt_reg_to_unified_fmt(
				REG_FLD_VAL_GET(L_CON_FLD_CFMT, val),
				REG_FLD_VAL_GET(L_CON_FLD_BTSW, val),
				REG_FLD_VAL_GET(L_CON_FLD_RGB_SWAP, val));
		p->bpp = UFMT_GET_bpp(p->fmt) / 8;
		p->alpha = REG_FLD_VAL_GET(L_CON_FLD_AEN, val);
		p->addr = DISP_REG_GET(layer_off + DISP_REG_OVL_L0_ADDR);
		val = DISP_REG_GET(DISP_REG_OVL_L0_SRC_SIZE + layer_off);
		p->src_w = REG_FLD_VAL_GET(SRC_SIZE_FLD_SRC_W, val);
		p->src_h = REG_FLD_VAL_GET(SRC_SIZE_FLD_SRC_H, val);

		val = DISP_REG_GET(DISP_REG_OVL_L0_PITCH + layer_off);
		p->src_pitch = REG_FLD_VAL_GET(L_PITCH_FLD_SRC_PITCH, val);

		val = DISP_REG_GET(DISP_REG_OVL_DATAPATH_CON + layer_off);
		p->gpu_mode = val & (0x1 << (8 + i));
		p->adobe_mode = REG_FLD_VAL_GET(DATAPATH_CON_FLD_ADOBE_MODE,
						val);
		p->ovl_gamma_out = REG_FLD_VAL_GET(
					DATAPATH_CON_FLD_OVL_GAMMA_OUT, val);

		DDPMSG("%s:layer%d,en %d,w %d,h %d,bpp %d,addr %lu\n",
		       __func__, i, p->layer_en, p->src_w, p->src_h,
			p->bpp, p->addr);
	}
}

extern int m4u_query_mva_info(unsigned int mva, unsigned int size,
			      unsigned int *real_mva, unsigned int *real_size);

static int ovl_check_input_param(struct OVL_CONFIG_STRUCT *cfg)
{
	if ((cfg->addr == 0 && cfg->source == 0) ||
	    cfg->dst_w == 0 || cfg->dst_h == 0) {
		DDPERR("ovl parameter invalidate, addr=0x%08lx, w=%d, h=%d\n",
		       cfg->addr, cfg->dst_w, cfg->dst_h);
		ASSERT(0);
		return -1;
	}

	return 0;
}

/* use noinline to reduce stack size */
static noinline
void print_layer_config_args(int module, int local_layer,
			     struct OVL_CONFIG_STRUCT *cfg,
			     struct disp_rect *roi)
{
	DDPDBG("%s:L%d/%d,source=%s,(%u,%u,%ux%u)->(%u,%u,%ux%u),pitch=%u\n",
		ddp_get_module_name(module), local_layer, cfg->layer,
		(cfg->source == 0) ? "memory" : "dim",
		cfg->src_x, cfg->src_y, cfg->src_w, cfg->src_h,
		cfg->dst_x, cfg->dst_y, cfg->dst_w, cfg->dst_h,
		cfg->src_pitch);
	DDPDBG(" fmt=%s,addr=0x%08lx,keyEn=%d,key=%d,aen=%d,alpha=%d\n",
		unified_color_fmt_name(cfg->fmt), cfg->addr,
		cfg->keyEn, cfg->key, cfg->aen, cfg->alpha);
	DDPDBG(" sur_aen=%d,sur_alpha=0x%x,yuv_range=%d,sec=%d,const_bld=%d\n",
		cfg->sur_aen, (cfg->dst_alpha << 2) | cfg->src_alpha,
		cfg->yuv_range, cfg->security, cfg->const_bld);

	if (roi)
		DDPDBG("dirty(%d,%d,%dx%d)\n", roi->x, roi->y, roi->height,
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
	cmdqRecSecureEnablePortSecurity(handle, (1LL << cmdq_engine));
	/* enable DAPC to protect the engine register */
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
		/* OVL is in sec stat, we need to switch it to nonsec */
		struct cmdqRecStruct *nonsec_switch_handle = NULL;
		int ret;

		ret = cmdqRecCreate(
				CMDQ_SCENARIO_DISP_PRIMARY_DISABLE_SECURE_PATH,
				&(nonsec_switch_handle));
		if (ret)
			DDPAEE("[SVP]fail to create disable_handle %s ret=%d\n",
			       __func__, ret);

		cmdqRecReset(nonsec_switch_handle);

		if (module != DISP_MODULE_OVL1_2L) {
			/* Primary Mode */
			if (primary_display_is_decouple_mode())
				cmdqRecWaitNoClear(nonsec_switch_handle,
						   CMDQ_EVENT_DISP_WDMA0_EOF);
			else
				_cmdq_insert_wait_frame_done_token_mira(
							nonsec_switch_handle);
		} else {
			/* External Mode */
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
		/* in fact, dapc/port_sec will be disabled by cmdq */
		cmdqRecSecureEnablePortSecurity(nonsec_switch_handle,
						(1LL << cmdq_engine));

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

/**
 * for enabled layers: layout continuously for each OVL HW engine
 * for disabled layers: ignored
 */
static int ovl_layer_layout(enum DISP_MODULE_ENUM module,
			    struct disp_ddp_path_config *pConfig)
{
	int local_layer, global_layer = 0;
	int ovl_idx = module;
	int phy_layer = -1, ext_layer = -1, ext_layer_idx = 0;
	struct OVL_CONFIG_STRUCT *ovl_cfg;
	int total_layer_num_ovl = TOTAL_OVL_LAYER_NUM;

	/* need leave the last to layer for show debug disp info */
	if (get_show_info_to_screen_flg())
		total_layer_num_ovl--;

	/*
	 * 1. check if it has been prepared,
	 * just only prepare once for each frame
	 */
	for (global_layer = 0; global_layer < TOTAL_OVL_LAYER_NUM;
	     global_layer++) {
		if (!(pConfig->ovl_layer_scanned & (1 << global_layer)))
			break;
	}
	if (global_layer >= TOTAL_OVL_LAYER_NUM)
		return 0;

	/* 2. prepare layer layout */
	for (local_layer = 0; local_layer < total_layer_num_ovl;
			local_layer++) {
		ovl_cfg = &pConfig->ovl_config[local_layer];
		ovl_cfg->ovl_index = -1;
	}

	for (local_layer = 0; local_layer <= ovl_layer_num(module);
			global_layer++) {
		if (global_layer >= TOTAL_OVL_LAYER_NUM)
			break;

		ovl_cfg = &pConfig->ovl_config[global_layer];

		/* Check if there is any extended layer on last phy layer */
		if (local_layer == ovl_layer_num(module)) {
			if (!disp_helper_get_option(DISP_OPT_OVL_EXT_LAYER) ||
			    ovl_cfg->ext_sel_layer == -1)
				break;
		}

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
			local_layer++;
			continue;
		}

		if (disp_helper_get_option(DISP_OPT_OVL_EXT_LAYER)) {
			if (ovl_cfg->ext_sel_layer != -1) {
				/* always layout from idx=0, so layer_idx
				 * here should be the same as ext_sel_layer
				 */
				if (phy_layer != ovl_cfg->ext_sel_layer) {
					DDPPR_ERR("L%d layout not match: cur=%d, in=%d\n",
					       global_layer, phy_layer,
					       ovl_cfg->ext_sel_layer);
					phy_layer++;
					ext_layer = -1;
				} else {
					ext_layer = ext_layer_idx++;
				}
			} else {
				/* all phy layers are layout continuously */
				phy_layer = local_layer;
				local_layer++;
			}
		} else {
			phy_layer = local_layer;
			local_layer++;
		}

		ovl_cfg->ovl_index = ovl_idx;
		ovl_cfg->phy_layer = phy_layer;
		ovl_cfg->ext_layer = ext_layer;
		DDPDBG("layout:L%d->ovl%d,phy:%d,ext:%d,ext_sel:%d,local_layer:%d\n",
		       global_layer, ovl_idx, phy_layer, ext_layer,
		       (ext_layer >= 0) ? phy_layer :
			ovl_cfg->ext_sel_layer, local_layer);
	}

	/* for Assert_layer config special case, do it specially */
	if (is_DAL_Enabled() && module == DISP_MODULE_OVL0) {
		ovl_cfg = &pConfig->ovl_config[TOTAL_REAL_OVL_LAYER_NUM - 1];
		ovl_cfg->ovl_index = DISP_MODULE_OVL0;
		ovl_cfg->phy_layer = ovl_layer_num(DISP_MODULE_OVL0) - 1;
		ovl_cfg->ext_sel_layer = -1;
		ovl_cfg->ext_layer = -1;
	}

	return 1;
}

static int ovl_config_l(enum DISP_MODULE_ENUM module,
			struct disp_ddp_path_config *pConfig, void *handle)
{
	int enabled_layers = 0;
	int has_sec_layer = 0;
	int layer_id;
	int ovl_layer = 0;
	int enabled_ext_layers = 0, ext_sel_layers = 0;
	struct golden_setting_context *golden_setting =
		pConfig->p_golden_setting_context;
	unsigned int Bpp, fps;
	unsigned long long tmp_bw, ovl_bw;

	unsigned long layer_offset_rdma_ctrl;
	unsigned long ovl_base = ovl_base_addr(module);

	if (pConfig->dst_dirty)
		ovl_roi(module, pConfig->dst_w, pConfig->dst_h, gOVL_bg_color,
			handle);

	if (!pConfig->ovl_dirty)
		return 0;

	ovl_layer_layout(module, pConfig);

	/* be careful, turn off all layers */
	for (ovl_layer = 0; ovl_layer < ovl_layer_num(module); ovl_layer++) {
		layer_offset_rdma_ctrl = ovl_base + ovl_layer *
				OVL_LAYER_OFFSET;
		DISP_REG_SET(handle, DISP_REG_OVL_RDMA0_CTRL +
				layer_offset_rdma_ctrl, 0x0);
	}

	has_sec_layer = setup_ovl_sec(module, pConfig, handle);

	if (!pConfig->ovl_partial_dirty && module == prev_rsz_module)
		_ovl_set_rsz_roi(module, pConfig, handle);

	if (golden_setting->fps)
		fps = golden_setting->fps;
	else {
		DDPDBG("no fps information, set fps as default 60\n");
		fps = 60;
	}

	ovl_bw = 0;
	for (layer_id = 0; layer_id < TOTAL_REAL_OVL_LAYER_NUM; layer_id++) {
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
					pConfig->rsz_src_roi, has_sec_layer,
					ovl_cfg, &pConfig->ovl_partial_roi,
					&layer_partial_roi, handle);
			} else {
				/* this layer will not be displayed */
				enable = 0;
			}
		} else {
			print_layer_config_args(module, ovl_cfg->phy_layer,
					ovl_cfg, NULL);
			ovl_layer_config(module, ovl_cfg->phy_layer,
					pConfig->rsz_src_roi, has_sec_layer,
					ovl_cfg, NULL, NULL, handle);
		}

		if (ovl_cfg->ext_layer != -1) {
			enabled_ext_layers |=
				enable << ovl_cfg->ext_layer;
			ext_sel_layers |=
				ovl_cfg->phy_layer <<
					(16 + 4 * ovl_cfg->ext_layer);
		} else {
			enabled_layers |= enable << ovl_cfg->phy_layer;
		}

		Bpp = ufmt_get_Bpp(ovl_cfg->fmt);
		tmp_bw = (unsigned long long)ovl_cfg->dst_h *
				ovl_cfg->dst_w * fps * Bpp;
		do_div(tmp_bw, 1000);
		tmp_bw *= 1250;
		do_div(tmp_bw, fps * 1000);
		ovl_bw = ovl_bw + tmp_bw;
		DDPMSG("h:%u, w:%u, fps:%u, Bpp:%u, bw:%llu\n",
			pConfig->dst_h, pConfig->dst_w, fps, Bpp, tmp_bw);
	}

	if (!pConfig->ovl_partial_dirty && module == next_rsz_module)
		_ovl_lc_config(module, pConfig, handle);

	_rpo_disable_dim_L0(module, pConfig, &enabled_layers);

	DDPDBG("%s bw:%llu\n", ddp_get_module_name(module), ovl_bw);
	DDPDBG("%s: enabled_layers=0x%01x, enabled_ext_layers=0x%01x, ext_sel_layers=0x%04x\n",
		ddp_get_module_name(module), enabled_layers,
		enabled_ext_layers, ext_sel_layers >> 16);
	DISP_REG_SET_FIELD(handle, SRC_CON_FLD_L_EN,
			   ovl_base_addr(module) + DISP_REG_OVL_SRC_CON,
			   enabled_layers);
	/* ext layer control */
	DISP_REG_SET(handle, ovl_base_addr(module) +
			DISP_REG_OVL_DATAPATH_EXT_CON,
			enabled_ext_layers | ext_sel_layers);

	/* bandwidth report */
	if (module == DISP_MODULE_OVL0) {
		DISP_SLOT_SET(handle, DISPSYS_SLOT_BASE, DISP_SLOT_IS_DC,
				golden_setting->is_dc);
		DISP_SLOT_SET(handle, DISPSYS_SLOT_BASE, DISP_SLOT_OVL0_BW,
				(unsigned int)ovl_bw);
	} else if (module == DISP_MODULE_OVL0_2L)
		DISP_SLOT_SET(handle, DISPSYS_SLOT_BASE, DISP_SLOT_OVL0_2L_BW,
				(unsigned int)ovl_bw);

	return 0;
}

int ovl_build_cmdq(enum DISP_MODULE_ENUM module, void *cmdq_trigger_handle,
		   enum CMDQ_STATE state)
{
	int ret = 0;
	/* int reg_pa = DISP_REG_OVL_FLOW_CTRL_DBG & 0x1fffffff; */

	if (!cmdq_trigger_handle) {
		DDPPR_ERR("cmdq_trigger_handle is NULL\n");
		return -1;
	}

	if (state == CMDQ_CHECK_IDLE_AFTER_STREAM_EOF) {
		if (module == DISP_MODULE_OVL0) {
			ret = cmdqRecPoll(cmdq_trigger_handle, 0x14007240, 2,
					  0x3f);
		} else {
			DDPPR_ERR("wrong module: %s\n",
				ddp_get_module_name(module));
			return -1;
		}
	}

	return 0;
}

/***************** ovl debug info *****************/

void ovl_dump_reg(enum DISP_MODULE_ENUM module)
{
	if (disp_helper_get_option(DISP_OPT_REG_PARSER_RAW_DUMP)) {
		unsigned long module_base = ovl_base_addr(module);

		DDPDUMP("== START: DISP %s REGS ==\n",
			ddp_get_module_name(module));
		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x0, INREG32(module_base + 0x0),
			0x4, INREG32(module_base + 0x4),
			0x8, INREG32(module_base + 0x8),
			0xC, INREG32(module_base + 0xC));
		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x10, INREG32(module_base + 0x10),
			0x14, INREG32(module_base + 0x14),
			0x20, INREG32(module_base + 0x20),
			0x24, INREG32(module_base + 0x24));
		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x28, INREG32(module_base + 0x28),
			0x2C, INREG32(module_base + 0x2C),
			0x30, INREG32(module_base + 0x30),
			0x34, INREG32(module_base + 0x34));
		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x38, INREG32(module_base + 0x38),
			0x3C, INREG32(module_base + 0x3C),
			0xF40, INREG32(module_base + 0xF40),
			0x44, INREG32(module_base + 0x44));
		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x48, INREG32(module_base + 0x48),
			0x4C, INREG32(module_base + 0x4C),
			0x50, INREG32(module_base + 0x50),
			0x54, INREG32(module_base + 0x54));
		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x58, INREG32(module_base + 0x58),
			0x5C, INREG32(module_base + 0x5C),
			0xF60, INREG32(module_base + 0xF60),
			0x64, INREG32(module_base + 0x64));
		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x68, INREG32(module_base + 0x68),
			0x6C, INREG32(module_base + 0x6C),
			0x70, INREG32(module_base + 0x70),
			0x74, INREG32(module_base + 0x74));
		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x78, INREG32(module_base + 0x78),
			0x7C, INREG32(module_base + 0x7C),
			0xF80, INREG32(module_base + 0xF80),
			0x84, INREG32(module_base + 0x84));
		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x88, INREG32(module_base + 0x88),
			0x8C, INREG32(module_base + 0x8C),
			0x90, INREG32(module_base + 0x90),
			0x94, INREG32(module_base + 0x94));
		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x98, INREG32(module_base + 0x98),
			0x9C, INREG32(module_base + 0x9C),
			0xFa0, INREG32(module_base + 0xFa0),
			0xa4, INREG32(module_base + 0xa4));
		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0xa8, INREG32(module_base + 0xa8),
			0xAC, INREG32(module_base + 0xAC),
			0xc0, INREG32(module_base + 0xc0),
			0xc8, INREG32(module_base + 0xc8));
		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0xcc, INREG32(module_base + 0xcc),
			0xd0, INREG32(module_base + 0xd0),
			0xe0, INREG32(module_base + 0xe0),
			0xe8, INREG32(module_base + 0xe8));
		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0xec, INREG32(module_base + 0xec),
			0xf0, INREG32(module_base + 0xf0),
			0x100, INREG32(module_base + 0x100),
			0x108, INREG32(module_base + 0x108));
		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x10c, INREG32(module_base + 0x10c),
			0x110, INREG32(module_base + 0x110),
			0x120, INREG32(module_base + 0x120),
			0x128, INREG32(module_base + 0x128));
		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x12c, INREG32(module_base + 0x12c),
			0x130, INREG32(module_base + 0x130),
			0x134, INREG32(module_base + 0x134),
			0x138, INREG32(module_base + 0x138));
		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x13c, INREG32(module_base + 0x13c),
			0x140, INREG32(module_base + 0x140),
			0x144, INREG32(module_base + 0x144),
			0x148, INREG32(module_base + 0x148));
		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x14c, INREG32(module_base + 0x14c),
			0x150, INREG32(module_base + 0x150),
			0x154, INREG32(module_base + 0x154),
			0x158, INREG32(module_base + 0x158));
		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x15c, INREG32(module_base + 0x15c),
			0x160, INREG32(module_base + 0x160),
			0x164, INREG32(module_base + 0x164),
			0x168, INREG32(module_base + 0x168));
		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x16c, INREG32(module_base + 0x16c),
			0x170, INREG32(module_base + 0x170),
			0x174, INREG32(module_base + 0x174),
			0x178, INREG32(module_base + 0x178));
		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x17c, INREG32(module_base + 0x17c),
			0x180, INREG32(module_base + 0x180),
			0x184, INREG32(module_base + 0x184),
			0x188, INREG32(module_base + 0x188));
		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x18c, INREG32(module_base + 0x18c),
			0x190, INREG32(module_base + 0x190),
			0x194, INREG32(module_base + 0x194),
			0x198, INREG32(module_base + 0x198));
		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x19c, INREG32(module_base + 0x19c),
			0x1a0, INREG32(module_base + 0x1a0),
			0x1a4, INREG32(module_base + 0x1a4),
			0x1a8, INREG32(module_base + 0x1a8));
		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x1ac, INREG32(module_base + 0x1ac),
			0x1b0, INREG32(module_base + 0x1b0),
			0x1b4, INREG32(module_base + 0x1b4),
			0x1b8, INREG32(module_base + 0x1b8));
		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x1bc, INREG32(module_base + 0x1bc),
			0x1c0, INREG32(module_base + 0x1c0),
			0x1c4, INREG32(module_base + 0x1c4),
			0x1c8, INREG32(module_base + 0x1c8));
		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x1cc, INREG32(module_base + 0x1cc),
			0x1d0, INREG32(module_base + 0x1d0),
			0x1d4, INREG32(module_base + 0x1d4),
			0x1e0, INREG32(module_base + 0x1e0));
		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x1e4, INREG32(module_base + 0x1e4),
			0x1e8, INREG32(module_base + 0x1e8),
			0x1ec, INREG32(module_base + 0x1ec),
			0x1F0, INREG32(module_base + 0x1F0));
		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x1F4, INREG32(module_base + 0x1F4),
			0x1F8, INREG32(module_base + 0x1F8),
			0x1FC, INREG32(module_base + 0x1FC),
			0x200, INREG32(module_base + 0x200));
		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x208, INREG32(module_base + 0x208),
			0x20C, INREG32(module_base + 0x20C),
			0x210, INREG32(module_base + 0x210),
			0x214, INREG32(module_base + 0x214));
		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x218, INREG32(module_base + 0x218),
			0x21C, INREG32(module_base + 0x21C),
			0x230, INREG32(module_base + 0x230),
			0x234, INREG32(module_base + 0x234));
		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x238, INREG32(module_base + 0x238),
			0x240, INREG32(module_base + 0x240),
			0x244, INREG32(module_base + 0x244),
			0x24c, INREG32(module_base + 0x24c));
		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x250, INREG32(module_base + 0x250),
			0x254, INREG32(module_base + 0x254),
			0x258, INREG32(module_base + 0x258),
			0x25c, INREG32(module_base + 0x25c));
		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x260, INREG32(module_base + 0x260),
			0x264, INREG32(module_base + 0x264),
			0x268, INREG32(module_base + 0x268),
			0x26C, INREG32(module_base + 0x26C));
		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x270, INREG32(module_base + 0x270),
			0x280, INREG32(module_base + 0x280),
			0x284, INREG32(module_base + 0x284),
			0x288, INREG32(module_base + 0x288));
		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x28C, INREG32(module_base + 0x28C),
			0x290, INREG32(module_base + 0x290),
			0x29C, INREG32(module_base + 0x29C),
			0x2A0, INREG32(module_base + 0x2A0));
		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x2A4, INREG32(module_base + 0x2A4),
			0x2B0, INREG32(module_base + 0x2B0),
			0x2B4, INREG32(module_base + 0x2B4),
			0x2B8, INREG32(module_base + 0x2B8));
		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x2BC, INREG32(module_base + 0x2BC),
			0x2C0, INREG32(module_base + 0x2C0),
			0x2C4, INREG32(module_base + 0x2C4),
			0x2C8, INREG32(module_base + 0x2C8));
		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x324, INREG32(module_base + 0x324),
			0x330, INREG32(module_base + 0x330),
			0x334, INREG32(module_base + 0x334),
			0x338, INREG32(module_base + 0x338));
		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x33C, INREG32(module_base + 0x33C),
			0xFB0, INREG32(module_base + 0xFB0),
			0x344, INREG32(module_base + 0x344),
			0x348, INREG32(module_base + 0x348));
		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x34C, INREG32(module_base + 0x34C),
			0x350, INREG32(module_base + 0x350),
			0x354, INREG32(module_base + 0x354),
			0x358, INREG32(module_base + 0x358));
		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x35C, INREG32(module_base + 0x35C),
			0xFB4, INREG32(module_base + 0xFB4),
			0x364, INREG32(module_base + 0x364),
			0x368, INREG32(module_base + 0x368));
		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x36C, INREG32(module_base + 0x36C),
			0x370, INREG32(module_base + 0x370),
			0x374, INREG32(module_base + 0x374),
			0x378, INREG32(module_base + 0x378));
		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x37C, INREG32(module_base + 0x37C),
			0xFB8, INREG32(module_base + 0xFB8),
			0x384, INREG32(module_base + 0x384),
			0x388, INREG32(module_base + 0x388));
		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x38C, INREG32(module_base + 0x38C),
			0x390, INREG32(module_base + 0x390),
			0x394, INREG32(module_base + 0x394),
			0x398, INREG32(module_base + 0x398));
		DDPDUMP("OVL0: 0x%04x=0x%08x\n",
			0xFC0, INREG32(module_base + 0xFC0));
		DDPDUMP("-- END: DISP %s REGS --\n",
			ddp_get_module_name(module));
	} else {
		unsigned long offset = ovl_base_addr(module);
		unsigned int src_on =
				DISP_REG_GET(DISP_REG_OVL_SRC_CON + offset);

		DDPDUMP("== DISP %s REGS ==\n", ddp_get_module_name(module));
		DDPDUMP("0x000: 0x%08x 0x%08x 0x%08x 0x%08x\n",
			DISP_REG_GET(DISP_REG_OVL_STA + offset),
			DISP_REG_GET(DISP_REG_OVL_INTEN + offset),
			DISP_REG_GET(DISP_REG_OVL_INTSTA + offset),
			DISP_REG_GET(DISP_REG_OVL_EN + offset));
		DDPDUMP("0x010: 0x%08x 0x%08x\n",
			DISP_REG_GET(DISP_REG_OVL_TRIG + offset),
			DISP_REG_GET(DISP_REG_OVL_RST + offset));
		DDPDUMP("0x020: 0x%08x 0x%08x 0x%08x 0x%08x\n",
			DISP_REG_GET(DISP_REG_OVL_ROI_SIZE + offset),
			DISP_REG_GET(DISP_REG_OVL_DATAPATH_CON + offset),
			DISP_REG_GET(DISP_REG_OVL_ROI_BGCLR + offset),
			DISP_REG_GET(DISP_REG_OVL_SRC_CON + offset));

		if (src_on & 0x1) {
			DDPDUMP("0x030: 0x%08x 0x%08x 0x%08x 0x%08x\n",
				DISP_REG_GET(DISP_REG_OVL_L0_CON + offset),
				DISP_REG_GET(DISP_REG_OVL_L0_SRCKEY + offset),
				DISP_REG_GET(DISP_REG_OVL_L0_SRC_SIZE + offset),
				DISP_REG_GET(DISP_REG_OVL_L0_OFFSET + offset));

			DDPDUMP("0xf40=0x%08x,0x044=0x%08x,0x0c0=0x%08x,0x0c8=0x%08x\n",
			    DISP_REG_GET(DISP_REG_OVL_L0_ADDR + offset),
			    DISP_REG_GET(DISP_REG_OVL_L0_PITCH + offset),
			    DISP_REG_GET(DISP_REG_OVL_RDMA0_CTRL + offset),
			    DISP_REG_GET(DISP_REG_OVL_RDMA0_MEM_GMC_SETTING +
					offset));

			DDPDUMP("0x0d0=0x%08x,0x1e0=0x%08x,0x24c=0x%08x\n",
				DISP_REG_GET(DISP_REG_OVL_RDMA0_FIFO_CTRL +
					offset),
				DISP_REG_GET(DISP_REG_OVL_RDMA0_MEM_GMC_S2 +
					offset),
				DISP_REG_GET(DISP_REG_OVL_RDMA0_DBG + offset));
		}
		if (src_on & 0x2) {
			DDPDUMP("0x050: 0x%08x 0x%08x 0x%08x 0x%08x\n",
				DISP_REG_GET(DISP_REG_OVL_L1_CON + offset),
				DISP_REG_GET(DISP_REG_OVL_L1_SRCKEY + offset),
				DISP_REG_GET(DISP_REG_OVL_L1_SRC_SIZE + offset),
				DISP_REG_GET(DISP_REG_OVL_L1_OFFSET + offset));

			DDPDUMP("0xf60=0x%08x,0x064=0x%08x,0x0e0=0x%08x,0x0e8=0x%08x,0x0f0=0x%08x\n",
			    DISP_REG_GET(DISP_REG_OVL_L1_ADDR + offset),
			    DISP_REG_GET(DISP_REG_OVL_L1_PITCH + offset),
			    DISP_REG_GET(DISP_REG_OVL_RDMA1_CTRL + offset),
			    DISP_REG_GET(DISP_REG_OVL_RDMA1_MEM_GMC_SETTING +
					offset),
			    DISP_REG_GET(DISP_REG_OVL_RDMA1_FIFO_CTRL +
					offset));

			DDPDUMP("0x1e4=0x%08x,0x250=0x%08x\n",
				DISP_REG_GET(DISP_REG_OVL_RDMA1_MEM_GMC_S2 +
					offset),
				DISP_REG_GET(DISP_REG_OVL_RDMA1_DBG + offset));
		}
		if (src_on & 0x4) {
			DDPDUMP("0x070: 0x%08x 0x%08x 0x%08x 0x%08x\n",
				DISP_REG_GET(DISP_REG_OVL_L2_CON + offset),
				DISP_REG_GET(DISP_REG_OVL_L2_SRCKEY + offset),
				DISP_REG_GET(DISP_REG_OVL_L2_SRC_SIZE + offset),
				DISP_REG_GET(DISP_REG_OVL_L2_OFFSET + offset));

			DDPDUMP("0xf80=0x%08x,0x084=0x%08x,0x100=0x%08x,0x108=0x%08x,0x110=0x%08x\n",
			    DISP_REG_GET(DISP_REG_OVL_L2_ADDR + offset),
			    DISP_REG_GET(DISP_REG_OVL_L2_PITCH + offset),
			    DISP_REG_GET(DISP_REG_OVL_RDMA2_CTRL + offset),
			    DISP_REG_GET(DISP_REG_OVL_RDMA2_MEM_GMC_SETTING +
					offset),
			    DISP_REG_GET(DISP_REG_OVL_RDMA2_FIFO_CTRL +
					offset));

			DDPDUMP("0x1e8=0x%08x,0x254=0x%08x\n",
			    DISP_REG_GET(DISP_REG_OVL_RDMA2_MEM_GMC_S2 +
					offset),
			    DISP_REG_GET(DISP_REG_OVL_RDMA2_DBG + offset));
		}
		if (src_on & 0x8) {
			DDPDUMP("0x090: 0x%08x 0x%08x 0x%08x 0x%08x\n",
			    DISP_REG_GET(DISP_REG_OVL_L3_CON + offset),
			    DISP_REG_GET(DISP_REG_OVL_L3_SRCKEY + offset),
			    DISP_REG_GET(DISP_REG_OVL_L3_SRC_SIZE + offset),
			    DISP_REG_GET(DISP_REG_OVL_L3_OFFSET + offset));

			DDPDUMP("0xfa0=0x%08x,0x0a4=0x%08x,0x120=0x%08x,0x128=0x%08x,0x130=0x%08x\n",
			    DISP_REG_GET(DISP_REG_OVL_L3_ADDR + offset),
			    DISP_REG_GET(DISP_REG_OVL_L3_PITCH + offset),
			    DISP_REG_GET(DISP_REG_OVL_RDMA3_CTRL + offset),
			    DISP_REG_GET(DISP_REG_OVL_RDMA3_MEM_GMC_SETTING
				+ offset),
			    DISP_REG_GET(DISP_REG_OVL_RDMA3_FIFO_CTRL
				+ offset));

			DDPDUMP("0x1ec=0x%08x,0x258=0x%08x\n",
			    DISP_REG_GET(DISP_REG_OVL_RDMA3_MEM_GMC_S2 +
					offset),
			    DISP_REG_GET(DISP_REG_OVL_RDMA3_DBG + offset));
		}
		if (src_on & 0x10) {
			DDPDUMP("0x280: 0x%08x 0x%08x 0x%08x 0x%08x\n",
				DISP_REG_GET(DISP_REG_OVL_LC_CON + offset),
				DISP_REG_GET(DISP_REG_OVL_LC_SRCKEY + offset),
				DISP_REG_GET(DISP_REG_OVL_LC_SRC_SIZE + offset),
				DISP_REG_GET(DISP_REG_OVL_LC_OFFSET + offset));

			DDPDUMP("0x290=0x%08x,0x29C=0x%08x\n",
				DISP_REG_GET(DISP_REG_OVL_LC_SRC_SEL + offset),
				DISP_REG_GET(DISP_REG_OVL_BANK_CON + offset));
		}
		DDPDUMP("0x1d4=0x%08x,0x1f8=0x%08x,0x1fc=0x%08x,0x200=0x%08x,0x20c=0x%08x\n"
			"0x210: 0x%08x 0x%08x 0x%08x 0x%08x\n"
			"0x230: 0x%08x 0x%08x, 0x240: 0x%08x 0x%08x, 0x2a0: 0x%08x 0x%08x\n",
			DISP_REG_GET(DISP_REG_OVL_DEBUG_MON_SEL + offset),
			DISP_REG_GET(DISP_REG_OVL_RDMA_GREQ_NUM + offset),
			DISP_REG_GET(DISP_REG_OVL_RDMA_GREQ_URG_NUM + offset),
			DISP_REG_GET(DISP_REG_OVL_DUMMY_REG + offset),
			DISP_REG_GET(DISP_REG_OVL_RDMA_ULTRA_SRC + offset),

			DISP_REG_GET(DISP_REG_OVL_RDMAn_BUF_LOW(0) + offset),
			DISP_REG_GET(DISP_REG_OVL_RDMAn_BUF_LOW(1) + offset),
			DISP_REG_GET(DISP_REG_OVL_RDMAn_BUF_LOW(2) + offset),
			DISP_REG_GET(DISP_REG_OVL_RDMAn_BUF_LOW(3) + offset),

			DISP_REG_GET(DISP_REG_OVL_SMI_DBG + offset),
			DISP_REG_GET(DISP_REG_OVL_GREQ_LAYER_CNT + offset),
			DISP_REG_GET(DISP_REG_OVL_FLOW_CTRL_DBG + offset),
			DISP_REG_GET(DISP_REG_OVL_ADDCON_DBG + offset),
			DISP_REG_GET(DISP_REG_OVL_FUNC_DCM0 + offset),
			DISP_REG_GET(DISP_REG_OVL_FUNC_DCM1 + offset));
	}
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
	DDPDUMP("wram_rst_cs:0x%x,layer_greq:0x%x,out_data:0x%x\n",
		status & 0x7, (status >> 3) & 0x1,
		(status >> 4) & 0xffffff);
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

	DDPDUMP("layer%d: w=%d,h=%d,off(x=%d,y=%d),pitch=%d,addr=0x%08x,fmt=%s,source=%s,aen=%d,alpha=%d\n",
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
	DDPDUMP("ovl_en=%d,layer_enable(%d,%d,%d,%d),bg(%dx%d)\n",
		DISP_REG_GET(DISP_REG_OVL_EN + offset) & 0x1,
		src_con & 0x1, (src_con >> 1) & 0x1, (src_con >> 2) & 0x1,
		(src_con >> 3) & 0x1,
		DISP_REG_GET(DISP_REG_OVL_ROI_SIZE + offset) & 0xfff,
		(DISP_REG_GET(DISP_REG_OVL_ROI_SIZE + offset) >> 16) & 0xfff);
	DDPDUMP("ext layer: layer_enable(%d,%d,%d), attach_layer(%d,%d,%d)\n",
		ext_con & 0x1, (ext_con >> 1) & 0x1, (ext_con >> 2) & 0x1,
		(ext_con >> 16) & 0xf, (ext_con >> 20) & 0xf,
		(ext_con >> 24) & 0xf);
	DDPDUMP("cur_pos(%d,%d),layer_hit(%d,%d,%d,%d),bg_mode=%s,sta=0x%x\n",
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
		DDPDUMP("ovl rdma%d status:(en=%d,fifo_used %d,GMC=0x%x)\n",
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

	ovl_dump_golden_setting(module);
}

int ovl_dump(enum DISP_MODULE_ENUM module, int level)
{
	ovl_dump_analysis(module);
	ovl_dump_golden_setting(module);
	ovl_dump_reg(module);

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
	int ddr_type = get_ddr_type();


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
	if (dst_mod_type == DST_MOD_REAL_TIME && ddr_type == TYPE_LPDDR3)
		regval |= REG_FLD_VAL(FLD_OVL_RDMA_MEM_GMC_PRE_ULTRA_THRESHOLD,
				0xe0);
	else
		regval |= REG_FLD_VAL(FLD_OVL_RDMA_MEM_GMC_PRE_ULTRA_THRESHOLD,
				0x3ff);

	for (i = 0; i < layer_num; i++) {
		unsigned long layer_offset = i * OVL_LAYER_OFFSET + ovl_base;

		DISP_REG_SET(cmdq, layer_offset +
			DISP_REG_OVL_RDMA0_MEM_GMC_SETTING, regval);
	}

	/* DISP_REG_OVL_RDMA0_FIFO_CTRL */
	regval = REG_FLD_VAL(FLD_OVL_RDMA_FIFO_SIZE, 192);
	for (i = 0; i < layer_num; i++) {
		unsigned long layer_offset = i * OVL_LAYER_OFFSET + ovl_base;

		DISP_REG_SET(cmdq, layer_offset + DISP_REG_OVL_RDMA0_FIFO_CTRL,
			regval);
	}

	/* DISP_REG_OVL_RDMA0_MEM_GMC_S2 */
	regval = 0;
	if (dst_mod_type == DST_MOD_REAL_TIME) {
		regval |= REG_FLD_VAL(FLD_OVL_RDMA_MEM_GMC2_ISSUE_REQ_THRES,
				127);
		regval |= REG_FLD_VAL(FLD_OVL_RDMA_MEM_GMC2_ISSUE_REQ_THRES_URG,
				63);
	} else {
		/* decouple */
		regval |= REG_FLD_VAL(FLD_OVL_RDMA_MEM_GMC2_ISSUE_REQ_THRES,
				63);
		regval |= REG_FLD_VAL(FLD_OVL_RDMA_MEM_GMC2_ISSUE_REQ_THRES_URG,
				63);
	}
	regval |= REG_FLD_VAL(FLD_OVL_RDMA_MEM_GMC2_REQ_THRES_PREULTRA, 0);
	regval |= REG_FLD_VAL(FLD_OVL_RDMA_MEM_GMC2_REQ_THRES_ULTRA, 1);
	regval |= REG_FLD_VAL(FLD_OVL_RDMA_MEM_GMC2_FORCE_REQ_THRES, 0);

	for (i = 0; i < layer_num; i++)
		DISP_REG_SET(cmdq, ovl_base + DISP_REG_OVL_RDMA0_MEM_GMC_S2 +
				i * 4, regval);

	/* DISP_REG_OVL_RDMA_GREQ_NUM */

	layer_greq_num = 7;

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
	regval |= REG_FLD_VAL(FLD_OVL_RDMA_GREQ_GRP_END_STOP, 1);
	regval |= REG_FLD_VAL(FLD_OVL_RDMA_GREQ_GRP_BRK_STOP, 1);
	regval |= REG_FLD_VAL(FLD_OVL_RDMA_GREQ_IOBUF_FLUSH_PREULTRA, 1);
	regval |= REG_FLD_VAL(FLD_OVL_RDMA_GREQ_IOBUF_FLUSH_ULTRA, 1);
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
	if (dst_mod_type == DST_MOD_REAL_TIME && ddr_type == TYPE_LPDDR3)
		regval |= REG_FLD_VAL(FLD_OVL_RDMA_BUF_LOW_PREULTRA_TH, 0x18);
	else
		regval |= REG_FLD_VAL(FLD_OVL_RDMA_BUF_LOW_PREULTRA_TH, 0x0);

	for (i = 0; i < layer_num; i++)
		DISP_REG_SET(cmdq, ovl_base +
			DISP_REG_OVL_RDMAn_BUF_LOW(i), regval);

	/* DISP_REG_OVL_RDMAn_BUF_HIGH */
	regval = REG_FLD_VAL(FLD_OVL_RDMA_BUF_HIGH_PREULTRA_DIS, 1);
	if (dst_mod_type == DST_MOD_REAL_TIME && ddr_type == TYPE_LPDDR3)
		regval |= REG_FLD_VAL(FLD_OVL_RDMA_BUF_HIGH_PREULTRA_TH, 0x90);
	else
		regval |= REG_FLD_VAL(FLD_OVL_RDMA_BUF_HIGH_PREULTRA_TH, 0x0);

	for (i = 0; i < layer_num; i++)
		DISP_REG_SET(cmdq, ovl_base +
			DISP_REG_OVL_RDMAn_BUF_HIGH(i), regval);

	/* DISP_REG_OVL_FUNC_DCM0 */
	DISP_REG_SET(cmdq, ovl_base + DISP_REG_OVL_FUNC_DCM0, 0x0);
	/* DISP_REG_OVL_FUNC_DCM1 */
	DISP_REG_SET(cmdq, ovl_base + DISP_REG_OVL_FUNC_DCM1, 0x0);

	/* DISP_REG_OVL_DATAPATH_CON */
	/* GCLAST_EN is set @ ovl_start() */

	/* Set ultra_sel of ovl0 & ovl0_2l to RDMA0 if path is DL with rsz
	 * OVL0_2l -> RSZ -> OVL0 -> RDMA0 -> DSI
	 */
	if (dst_mod_type == DST_MOD_REAL_TIME) {
		DISP_REG_SET_FIELD(cmdq, mmsys_ovl_ultra_offset(module),
			DISP_REG_CONFIG_MMSYS_MISC, 1);
	} else {
		DISP_REG_SET_FIELD(cmdq, mmsys_ovl_ultra_offset(module),
			DISP_REG_CONFIG_MMSYS_MISC, 0);
	}

	return 0;
}

int ovl_partial_update(enum DISP_MODULE_ENUM module, unsigned int bg_w,
		       unsigned int bg_h, void *handle)
{
	unsigned long ovl_base = ovl_base_addr(module);

	if ((bg_w > OVL_MAX_WIDTH) || (bg_h > OVL_MAX_HEIGHT)) {
		DDPPR_ERR("ovl_roi,exceed OVL max size, w=%d, h=%d\n",
			bg_w, bg_h);
		ASSERT(0);
	}
	DDPDBG("ovl%d partial update\n", module);
	DISP_REG_SET(handle, ovl_base + DISP_REG_OVL_ROI_SIZE,
			bg_h << 16 | bg_w);

	DISP_REG_SET_FIELD(handle, FLD_OVL_LC_XOFF,
			   ovl_base + DISP_REG_OVL_LC_OFFSET, 0);
	DISP_REG_SET_FIELD(handle, FLD_OVL_LC_YOFF,
			   ovl_base + DISP_REG_OVL_LC_OFFSET, 0);
	DISP_REG_SET_FIELD(handle, FLD_OVL_LC_SRC_W,
			   ovl_base + DISP_REG_OVL_LC_SRC_SIZE, bg_w);
	DISP_REG_SET_FIELD(handle, FLD_OVL_LC_SRC_H,
			   ovl_base + DISP_REG_OVL_LC_SRC_SIZE, bg_h);

	return 0;
}

int ovl_addr_mva_replacement(enum DISP_MODULE_ENUM module,
			     struct ddp_fb_info *p_fb_info, void *handle)
{
	unsigned long offset = ovl_base_addr(module);
	unsigned int src_on = DISP_REG_GET(DISP_REG_OVL_SRC_CON + offset);
	unsigned int layer_addr, layer_mva;

	if (src_on & 0x1) {
		layer_addr = DISP_REG_GET(DISP_REG_OVL_L0_ADDR + offset);
		layer_mva = layer_addr - p_fb_info->fb_pa + p_fb_info->fb_mva;
		DISP_REG_SET(handle, DISP_REG_OVL_L0_ADDR + offset, layer_mva);
	}

	if (src_on & 0x2) {
		layer_addr = DISP_REG_GET(DISP_REG_OVL_L1_ADDR + offset);
		layer_mva = layer_addr - p_fb_info->fb_pa + p_fb_info->fb_mva;
		DISP_REG_SET(handle, DISP_REG_OVL_L1_ADDR + offset, layer_mva);
	}

	if (src_on & 0x4) {
		layer_addr = DISP_REG_GET(DISP_REG_OVL_L2_ADDR + offset);
		layer_mva = layer_addr - p_fb_info->fb_pa + p_fb_info->fb_mva;
		DISP_REG_SET(handle, DISP_REG_OVL_L2_ADDR + offset, layer_mva);
	}

	if (src_on & 0x8) {
		layer_addr = DISP_REG_GET(DISP_REG_OVL_L3_ADDR + offset);
		layer_mva = layer_addr - p_fb_info->fb_pa + p_fb_info->fb_mva;
		DISP_REG_SET(handle, DISP_REG_OVL_L3_ADDR + offset, layer_mva);
	}

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
	} else if (ioctl_cmd == DDP_OVL_MVA_REPLACEMENT) {
		struct ddp_fb_info *p_fb_info = (struct ddp_fb_info *)params;

		ovl_addr_mva_replacement(module, p_fb_info, handle);
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
