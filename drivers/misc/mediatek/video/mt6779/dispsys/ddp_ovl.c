// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
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
#include "ddp_ovl_wcg.h"

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

static unsigned int gOVL_bg_color = 0xff000000;
static unsigned int gOVL_dim_color = 0xff000000;

static unsigned int ovl_bg_w[OVL_NUM];
static unsigned int ovl_bg_h[OVL_NUM];
static enum DISP_MODULE_ENUM next_rsz_module = DISP_MODULE_UNKNOWN;
static enum DISP_MODULE_ENUM prev_rsz_module = DISP_MODULE_UNKNOWN;

struct OVL_CONFIG_STRUCT g_primary_ovl_cfg[TOTAL_OVL_LAYER_NUM];
struct OVL_CONFIG_STRUCT g_second_ovl_cfg[TOTAL_OVL_LAYER_NUM];

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
	if (module == DISP_MODULE_OVL0 || module == DISP_MODULE_OVL0_2L ||
	    module == DISP_MODULE_OVL1_2L)
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
		DDP_PR_ERR("invalid ovl module=%d\n", module);
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
		return 0;
	}
	return 0;
}

unsigned long ovl_layer_num(enum DISP_MODULE_ENUM module)
{
	switch (module) {
	case DISP_MODULE_OVL0:
		return 4;
	case DISP_MODULE_OVL0_2L:
		return 2;
	case DISP_MODULE_OVL1_2L:
		return 2;
	default:
		DDP_PR_ERR("invalid ovl module=%d\n", module);
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
		DDP_PR_ERR("invalid ovl module=%d, %s fail\n",
			   module, __func__);
		ASSERT(0);
		return CMDQ_SYNC_TOKEN_INVALID;
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
		DDP_PR_ERR("invalid ovl module=%d, get cmdq engine fail\n",
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
	DDP_PR_ERR("invalid ovl module=%d, get ovl index fail\n", module);
	ASSERT(0);
	return 0;
}

static inline enum DISP_MODULE_ENUM ovl_index_to_module(int index)
{
	if (index >= OVL_NUM) {
		DDP_PR_ERR("invalid ovl index=%d\n", index);
		ASSERT(0);
	}

	return ovl_index_module[index];
}

int ovl_start(enum DISP_MODULE_ENUM module, void *handle)
{
	unsigned long baddr = ovl_base_addr(module);
	unsigned int value = 0, mask = 0;

	DISP_REG_SET_FIELD(handle, EN_FLD_OVL_EN, baddr + DISP_REG_OVL_EN, 0x1);

	DISP_REG_SET(handle, baddr + DISP_REG_OVL_INTEN,
		     0x1F2 | REG_FLD_VAL(INTEN_FLD_ABNORMAL_SOF, 1) |
		     REG_FLD_VAL(INTEN_FLD_START_INTEN, 1));

#if (defined(CONFIG_TEE) || \
	defined(CONFIG_TRUSTONIC_TEE_SUPPORT)) && \
	defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
	DISP_REG_SET_FIELD(handle, INTEN_FLD_FME_CPL_INTEN,
			   baddr + DISP_REG_OVL_INTEN, 1);
#endif

	DISP_REG_SET_FIELD(handle, FLD_RDMA_BURST_CON1_BURST16_EN,
			   baddr + DISP_REG_OVL_RDMA_BURST_CON1, 1);

	SET_VAL_MASK(value, mask, 1, DATAPATH_CON_FLD_LAYER_SMI_ID_EN);
	SET_VAL_MASK(value, mask, 1, DATAPATH_CON_FLD_GCLAST_EN);
	SET_VAL_MASK(value, mask, 1, DATAPATH_CON_FLD_OUTPUT_CLAMP);
	DISP_REG_MASK(handle, baddr + DISP_REG_OVL_DATAPATH_CON, value, mask);

	value = 0;
	mask = 0;
	SET_VAL_MASK(value, mask, 1, FLD_FBDC_8XE_MODE);
	SET_VAL_MASK(value, mask, 1, FLD_FBDC_FILTER_EN);
	DISP_REG_MASK(handle, baddr + DISP_REG_OVL_FBDC_CFG1, value, mask);
	return 0;
}

int ovl_stop(enum DISP_MODULE_ENUM module, void *handle)
{
	unsigned long baddr = ovl_base_addr(module);

	DISP_REG_SET(handle, baddr + DISP_REG_OVL_INTEN, 0);
	DISP_REG_SET_FIELD(handle, EN_FLD_OVL_EN, baddr + DISP_REG_OVL_EN, 0);
	DISP_REG_SET(handle, baddr + DISP_REG_OVL_INTSTA, 0);

	return 0;
}

int ovl_is_idle(enum DISP_MODULE_ENUM module)
{
	unsigned long baddr = ovl_base_addr(module);
	unsigned int ovl_flow_ctrl_dbg;

	ovl_flow_ctrl_dbg = DISP_REG_GET(baddr + DISP_REG_OVL_FLOW_CTRL_DBG);
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
	unsigned long baddr = ovl_base_addr(module);
	unsigned int ovl_flow_ctrl_dbg;

	DISP_CPU_REG_SET(baddr + DISP_REG_OVL_RST, 1);
	DISP_CPU_REG_SET(baddr + DISP_REG_OVL_RST, 0);
	/* only wait if not cmdq */
	if (handle)
		return ret;

	ovl_flow_ctrl_dbg = DISP_REG_GET(baddr + DISP_REG_OVL_FLOW_CTRL_DBG);
	while (!(ovl_flow_ctrl_dbg & OVL_IDLE)) {
		delay_cnt++;
		udelay(10);
		if (delay_cnt > 2000) {
			DDP_PR_ERR("%s reset timeout!\n",
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
	unsigned long baddr = ovl_base_addr(module);

	if ((bg_w > OVL_MAX_WIDTH) || (bg_h > OVL_MAX_HEIGHT)) {
		DDP_PR_ERR("%s,exceed OVL max size, wh(%ux%u)\n",
			   __func__, bg_w, bg_h);
		ASSERT(0);
	}

	DISP_REG_SET(handle, baddr + DISP_REG_OVL_ROI_SIZE, bg_h << 16 | bg_w);
	DISP_REG_SET(handle, baddr + DISP_REG_OVL_ROI_BGCLR, bg_color);

	DISP_REG_SET(handle, baddr + DISP_REG_OVL_LC_SRC_SIZE,
		((bg_h << 16) + bg_w));

	_store_roi(module, bg_w, bg_h);

	DDPMSG("%s_roi:(%ux%u)\n", ddp_get_module_name(module), bg_w, bg_h);
	return 0;
}

static int _ovl_get_rsz_layer_roi(const struct OVL_CONFIG_STRUCT *const c,
				u32 *dst_x, u32 *dst_y, u32 *dst_w, u32 *dst_h,
				struct disp_rect src_total_roi)
{
	if (c->src_w > c->dst_w || c->src_h > c->dst_h) {
		DDP_PR_ERR("%s:L%u:src(%ux%u)>dst(%ux%u)\n", __func__, c->layer,
			   c->src_w, c->src_h, c->dst_w, c->dst_h);
		return -EINVAL;
	}

	if (c->src_w < c->dst_w || c->src_h < c->dst_h) {
		*dst_x = ((c->dst_x * c->src_w) / c->dst_w) - src_total_roi.x;
		*dst_y = ((c->dst_y * c->src_h) / c->dst_h) - src_total_roi.y;
		*dst_w = c->src_w;
		*dst_h = c->src_h;
	}

	if (c->src_w != c->dst_w || c->src_h != c->dst_h) {
		DDPDBG("%s:L%u:(%u,%u,%ux%u)->(%u,%u,%ux%u)\n", __func__,
		       c->layer, *dst_x, *dst_y, *dst_w, *dst_h,
		       c->dst_x, c->dst_y, c->dst_w, c->dst_h);
	}

	return 0;
}

static int _ovl_get_rsz_roi(enum DISP_MODULE_ENUM module,
			    struct disp_ddp_path_config *pconfig,
			    u32 *bg_w, u32 *bg_h)
{
	struct disp_rect *rsz_src_roi = &pconfig->rsz_src_roi;

	if (pconfig->rsz_enable) {
		*bg_w = rsz_src_roi->width;
		*bg_h = rsz_src_roi->height;
	}

	return 0;
}

static int _ovl_set_rsz_roi(enum DISP_MODULE_ENUM module,
			    struct disp_ddp_path_config *pconfig, void *handle)
{
	u32 bg_w = pconfig->dst_w, bg_h = pconfig->dst_h;

	_ovl_get_rsz_roi(module, pconfig, &bg_w, &bg_h);
	ovl_roi(module, bg_w, bg_h, gOVL_bg_color, handle);

	return 0;
}

static int _ovl_lc_config(enum DISP_MODULE_ENUM module,
			  struct disp_ddp_path_config *pconfig, void *handle)
{
	unsigned long ovl_base = ovl_base_addr(module);
	struct disp_rect rsz_dst_roi = pconfig->rsz_dst_roi;
	u32 lc_x = 0, lc_y = 0, lc_w = pconfig->dst_w, lc_h = pconfig->dst_h;
	int rotate = 0;

#ifdef CONFIG_MTK_LCM_PHYSICAL_ROTATION_HW
	rotate = 1;
#endif

	if (pconfig->rsz_enable) {
		lc_x = rsz_dst_roi.x;
		lc_y = rsz_dst_roi.y;
		lc_w = rsz_dst_roi.width;
		lc_h = rsz_dst_roi.height;
	}

	if (rotate) {
		unsigned int bg_w = 0, bg_h = 0;

		_get_roi(module, &bg_w, &bg_h);
		lc_y = bg_h - lc_h - lc_y;
		lc_x = bg_w - lc_w - lc_x;
	}

	DISP_REG_SET(handle, ovl_base + DISP_REG_OVL_LC_OFFSET,
		((lc_y << 16) | lc_x));

	DISP_REG_SET(handle, ovl_base + DISP_REG_OVL_LC_SRC_SIZE,
		((lc_h << 16) | lc_w));

	DDPMSG("%s_lc:(%d,%d,%dx%d)\n",
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
	if (c->module != module)
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
	unsigned long baddr = ovl_base_addr(module);

	/* physical layer control */
	DISP_REG_SET_FIELD(handle, SRC_CON_FLD_L_EN,
		ovl_base_addr(module) + DISP_REG_OVL_SRC_CON, 0);
	/* ext layer control */
	DISP_REG_SET(handle, baddr + DISP_REG_OVL_DATAPATH_EXT_CON, 0);
	DDPSVPMSG("[SVP] switch %s to nonsec: disable all layers first!\n",
		  ddp_get_module_name(module));
	return 0;
}

int ovl_layer_switch(enum DISP_MODULE_ENUM module, unsigned int layer,
		     unsigned int en, void *handle)
{
	unsigned long baddr = ovl_base_addr(module);

	ASSERT(layer <= 3);
	switch (layer) {
	case 0:
		DISP_REG_SET_FIELD(handle, SRC_CON_FLD_L0_EN, baddr +
				   DISP_REG_OVL_SRC_CON, en);
		DISP_REG_SET(handle, baddr + DISP_REG_OVL_RDMA0_CTRL, en);
		break;
	case 1:
		DISP_REG_SET_FIELD(handle, SRC_CON_FLD_L1_EN, baddr +
				   DISP_REG_OVL_SRC_CON, en);
		DISP_REG_SET(handle, baddr + DISP_REG_OVL_RDMA1_CTRL, en);
		break;
	case 2:
		DISP_REG_SET_FIELD(handle, SRC_CON_FLD_L2_EN, baddr +
				   DISP_REG_OVL_SRC_CON, en);
		DISP_REG_SET(handle, baddr + DISP_REG_OVL_RDMA2_CTRL, en);
		break;
	case 3:
		DISP_REG_SET_FIELD(handle, SRC_CON_FLD_L3_EN, baddr +
				   DISP_REG_OVL_SRC_CON, en);
		DISP_REG_SET(handle, baddr + DISP_REG_OVL_RDMA3_CTRL, en);
		break;
	default:
		DDP_PR_ERR("invalid layer=%d\n", layer);
		ASSERT(0);
		break;
	}

	return 0;
}

static int
ovl_layer_config(enum DISP_MODULE_ENUM module, unsigned int phy_layer,
		 struct disp_rect src_total_roi, unsigned int is_engine_sec,
		 struct OVL_CONFIG_STRUCT *const cfg,
		 const struct disp_rect *const ovl_partial_roi,
		 const struct disp_rect *const layer_partial_roi, void *handle)
{
	unsigned int value = 0, fld = 0;
	unsigned int Bpp, input_swap, input_fmt;
	unsigned int rgb_swap = 0;
	int is_rgb;
	int color_matrix = 0x4;
	int rotate = 0;
	int is_ext_layer = cfg->ext_layer != -1;
	unsigned long baddr = ovl_base_addr(module);
	unsigned long Lx_base = 0;
	unsigned long Lx_clr_base = 0;
	unsigned long Lx_addr_base = 0;
	unsigned int offset = 0;
	enum UNIFIED_COLOR_FMT format = cfg->fmt;
	unsigned int src_x = cfg->src_x;
	unsigned int src_y = cfg->src_y;
	unsigned int dst_x = cfg->dst_x;
	unsigned int dst_y = cfg->dst_y;
	unsigned int dst_w = cfg->dst_w;
	unsigned int dst_h = cfg->dst_h;
	unsigned int dim_color;

	const int len = 120;
	char msg[len];
	int n = 0;

	if (is_ext_layer) {
		Lx_base = baddr + cfg->ext_layer * OVL_LAYER_OFFSET;
		Lx_base += (DISP_REG_OVL_EL0_CON - DISP_REG_OVL_L0_CON);

		Lx_clr_base = baddr + cfg->ext_layer * 0x4;
		Lx_clr_base += (DISP_REG_OVL_EL0_CLR - DISP_REG_OVL_L0_CLR);

		Lx_addr_base = baddr + cfg->ext_layer * 0x4;
		Lx_addr_base += (DISP_REG_OVL_EL0_ADDR - DISP_REG_OVL_L0_ADDR);
	} else {
		Lx_base = baddr + phy_layer * OVL_LAYER_OFFSET;
		Lx_clr_base = baddr + phy_layer * 0x4;
		Lx_addr_base = baddr + phy_layer * OVL_LAYER_OFFSET;
	}

	if (ovl_partial_roi) {
		dst_x = layer_partial_roi->x - ovl_partial_roi->x;
		dst_y = layer_partial_roi->y - ovl_partial_roi->y;
		dst_w = layer_partial_roi->width;
		dst_h = layer_partial_roi->height;
		src_x = cfg->src_x + layer_partial_roi->x - cfg->dst_x;
		src_y = cfg->src_y + layer_partial_roi->y - cfg->dst_y;

		n = snprintf(msg, len, "layer partial (%d,%d)(%d,%d,%dx%d) ",
			     cfg->src_x, cfg->src_y, cfg->dst_x, cfg->dst_y,
			     cfg->dst_w, cfg->dst_h);
		n += snprintf(msg + n, len - n, "to (%d,%d)(%d,%d,%dx%d)\n",
			      src_x, src_y, dst_x, dst_y, dst_w, dst_h);
		DDPDBG("%s", msg);
	}

	/* sbch can use the variable */
	cfg->real_dst_x = dst_x;
	cfg->real_dst_y = dst_y;

	ASSERT(dst_w <= OVL_MAX_WIDTH);
	ASSERT(dst_h <= OVL_MAX_HEIGHT);

	if (!cfg->addr && cfg->source == OVL_LAYER_SOURCE_MEM) {
		DDP_PR_ERR("source from memory, but addr is 0!\n");
		ASSERT(0);
		return -1;
	}

	_ovl_get_rsz_layer_roi(cfg, &dst_x, &dst_y, &dst_w, &dst_h,
			       src_total_roi);

#ifdef CONFIG_MTK_LCM_PHYSICAL_ROTATION_HW
	if (module != DISP_MODULE_OVL1_2L)
		rotate = 1;
#endif

	/* check dim layer fmt */
	if (cfg->source == OVL_LAYER_SOURCE_RESERVED) {
		if (cfg->aen == 0)
			DDP_PR_ERR("dim layer%d alpha enable should be 1!\n",
				   phy_layer);
		format = UFMT_RGB888;
	}
	Bpp = ufmt_get_Bpp(format);
	input_swap = ufmt_get_byteswap(format);
	input_fmt = ufmt_get_format(format);
	is_rgb = ufmt_get_rgb(format);

	if (rotate) {
		unsigned int bg_w = 0, bg_h = 0;

		_get_roi(module, &bg_w, &bg_h);
		value = ((bg_h - dst_h - dst_y) << 16) | (bg_w - dst_w - dst_x);
	} else {
		value = (dst_y << 16) | dst_x;
	}
	DISP_REG_SET(handle, DISP_REG_OVL_L0_OFFSET + Lx_base, value);

	value = 0;
	if (format == UFMT_UYVY || format == UFMT_VYUY ||
	    format == UFMT_YUYV || format == UFMT_YVYU) {
		if (src_x % 2) {
			src_x -= 1; /* make src_x to even */
			dst_w += 1;
			value |= REG_FLD_VAL(OVL_L_CLIP_FLD_LEFT, 1);
		}

		if ((src_x + dst_w) % 2) {
			dst_w += 1; /* make right boundary even */
			value |= REG_FLD_VAL(OVL_L_CLIP_FLD_RIGHT, 1);
		}
	}
	DISP_REG_SET(handle, DISP_REG_OVL_L0_CLIP + Lx_base, value);

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
		DDP_PR_ERR("un-recognized yuv_range=%d!\n", cfg->yuv_range);
		color_matrix = 4;
		break;
	}

	DISP_REG_SET(handle, DISP_REG_OVL_RDMA0_CTRL + Lx_base, 0x1);

	value = (REG_FLD_VAL((L_CON_FLD_LSRC), (cfg->source)) |
		 REG_FLD_VAL((L_CON_FLD_CFMT), (input_fmt)) |
		 REG_FLD_VAL((L_CON_FLD_AEN), (cfg->aen)) |
		 REG_FLD_VAL((L_CON_FLD_APHA), (cfg->alpha)) |
		 REG_FLD_VAL((L_CON_FLD_SKEN), (cfg->keyEn)) |
		 REG_FLD_VAL((L_CON_FLD_BTSW), (input_swap)));
	if (ufmt_is_old_fmt(format)) {
		if (format == UFMT_PARGB8888 || format == UFMT_PABGR8888 ||
		    format == UFMT_PRGBA8888 || format == UFMT_PBGRA8888 ||
		    format == UFMT_PRGBA1010102 || format == UFMT_PRGBA_FP16) {
			rgb_swap = ufmt_get_rgbswap(format);
			value |= REG_FLD_VAL((L_CON_FLD_RGB_SWAP), (rgb_swap));
		}
		value |= REG_FLD_VAL((L_CON_FLD_CLRFMT_MAN), (1));
	}

	if (!is_rgb)
		value |= REG_FLD_VAL((L_CON_FLD_MTX), (color_matrix));

	if (rotate)
		value |= REG_FLD_VAL((L_CON_FLD_VIRTICAL_FLIP), (1)) |
			 REG_FLD_VAL((L_CON_FLD_HORI_FLIP), (1));

	DISP_REG_SET(handle, DISP_REG_OVL_L0_CON + Lx_base, value);

	value = 0;
	if (format == UFMT_RGBA1010102 || format == UFMT_PRGBA1010102)
		value = 1;
	else if (format == UFMT_RGBA_FP16 || format == UFMT_PRGBA_FP16)
		value = 3;

	if (is_ext_layer)
		fld = FLD_ELn_CLRFMT_NB(cfg->ext_layer);
	else
		fld = FLD_Ln_CLRFMT_NB(cfg->phy_layer);
	DISP_REG_SET_FIELD(handle, fld, baddr + DISP_REG_OVL_CLRFMT_EXT, value);

	dim_color = gOVL_dim_color == 0xff000000 ?
		    cfg->dim_color : gOVL_dim_color;
	DISP_REG_SET(handle, DISP_REG_OVL_L0_CLR + Lx_clr_base,
		     0xff000000 | dim_color);

	DISP_REG_SET(handle, DISP_REG_OVL_L0_SRC_SIZE + Lx_base,
		     dst_h << 16 | dst_w);

	if (rotate)
		offset = (src_x + dst_w) * Bpp + (src_y + dst_h - 1) *
						cfg->src_pitch - 1;
	else
		offset = src_x * Bpp + src_y * cfg->src_pitch;

	/* sbch can use the variable */
	cfg->real_addr = cfg->addr + offset;
	cfg->real_dst_w = dst_w;
	cfg->real_dst_h = dst_h;

	if (!is_engine_sec) {
		DISP_REG_SET(handle, DISP_REG_OVL_L0_ADDR + Lx_addr_base,
			     cfg->real_addr);
	} else {
#ifdef MTKFB_M4U_SUPPORT
		unsigned int size;
		int m4u_port;

		size = (dst_h - 1) * cfg->src_pitch + dst_w * Bpp;
		m4u_port = module_to_m4u_port(module);
		if (cfg->security != DISP_SECURE_BUFFER) {
			/*
			 * OVL is sec but this layer is non-sec
			 * we need to tell cmdq to help map non-sec mva
			 * to sec mva
			 */
			cmdqRecWriteSecure(handle,
					disp_addr_convert(DISP_REG_OVL_L0_ADDR +
							  Lx_addr_base),
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
							  Lx_addr_base),
					CMDQ_SAM_H_2_MVA, cfg->addr, offset,
					size, m4u_port);
		}
#endif /* MTKFB_M4U_SUPPORT */
	}
	DISP_REG_SET(handle, DISP_REG_OVL_L0_SRCKEY + Lx_base, cfg->key);

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
	DISP_REG_SET(handle, DISP_REG_OVL_L0_PITCH + Lx_base, value);

	return 0;
}

static int ovl_layer_config_compress(enum DISP_MODULE_ENUM module,
				unsigned int phy_layer,
				struct disp_rect src_total_roi,
				unsigned int is_engine_sec,
				struct OVL_CONFIG_STRUCT *const cfg,
				void *handle)
{
	unsigned int src_x = cfg->src_x, src_y = cfg->src_y;
	unsigned int src_w = cfg->src_w, src_h = cfg->src_h;
	unsigned int dst_x = cfg->dst_x, dst_y = cfg->dst_y;
	unsigned int dst_w = cfg->dst_w, dst_h = cfg->dst_h;
	unsigned int src_pitch = cfg->src_pitch;
	unsigned int src_x_align = 0, src_y_align = 0;
	unsigned int src_w_align = 0, src_h_align = 0;

	enum UNIFIED_COLOR_FMT format = cfg->fmt;
	unsigned int Bpp = ufmt_get_Bpp(format);
	unsigned int input_swap = ufmt_get_byteswap(format);
	unsigned int rgb_swap = 0;
	unsigned int input_fmt = ufmt_get_format(format);
	unsigned int is_rgb = ufmt_get_rgb(format);

	unsigned long baddr = ovl_base_addr(module);
	unsigned long Lx_base = 0;
	unsigned long Lx_clr_base = 0;
	unsigned long Lx_addr_base = 0;
	unsigned long Lx_PVRIC_hdr_base = 0;
	unsigned long Lx_const_clr_base = 0;

	unsigned int tile_offset = 0;

	/* tile size */
	unsigned int tile_w = 16, tile_h = 4;

	unsigned int value = 0, fld = 0;
	int rotate = 0;

	unsigned long buf_addr = 0;
	unsigned int header_offset = 0;

	/* sbch can use the variable */
	cfg->real_dst_x = dst_x;
	cfg->real_dst_y = dst_y;

	/* 1. check params */
	if (dst_w > OVL_MAX_WIDTH || dst_h > OVL_MAX_HEIGHT) {
		DDP_PR_ERR("[PVRIC] invalid size: %u x %u\n", dst_w, dst_h);
		return -EINVAL;
	}

	if (!cfg->addr && cfg->source == OVL_LAYER_SOURCE_MEM) {
		DDP_PR_ERR("[PVRIC] source from memory, but addr is 0\n");
		return -EINVAL;
	}

	if (format == UFMT_RGB888) {
		src_pitch = 4 * src_pitch / 3;
		cfg->src_pitch = 4 * cfg->src_pitch / 3;
		Bpp = 4;
	}

	if (!is_rgb || (Bpp != 4 && Bpp != 8)) {
		DDP_PR_ERR("[PVRIC] OVL no support compressed %s\n",
			unified_color_fmt_name(format));
		return -EINVAL;
	}

	_ovl_get_rsz_layer_roi(cfg, &dst_x, &dst_y, &dst_w, &dst_h,
			       src_total_roi);

	/* 2. calculate register base */
	if (cfg->ext_layer == -1) {
		Lx_base = baddr + phy_layer * OVL_LAYER_OFFSET;
		Lx_clr_base = baddr + phy_layer * 0x4;
		Lx_addr_base = baddr + phy_layer * OVL_LAYER_OFFSET;
		Lx_PVRIC_hdr_base = baddr + phy_layer *
		    (DISP_REG_OVL_L1_HDR_ADDR - DISP_REG_OVL_L0_HDR_ADDR);
		Lx_const_clr_base = baddr + DISP_REG_OVL_L0_FBDC_CNST_CLR0
		    + phy_layer * 0x8;
	} else {
		Lx_base = baddr + cfg->ext_layer * OVL_LAYER_OFFSET;
		Lx_base += (DISP_REG_OVL_EL0_CON - DISP_REG_OVL_L0_CON);

		Lx_clr_base = baddr + cfg->ext_layer * 0x4;
		Lx_clr_base += (DISP_REG_OVL_EL0_CLR - DISP_REG_OVL_L0_CLR);

		Lx_addr_base = baddr + cfg->ext_layer * 0x4;
		Lx_addr_base += (DISP_REG_OVL_EL0_ADDR - DISP_REG_OVL_L0_ADDR);

		Lx_PVRIC_hdr_base = baddr + cfg->ext_layer *
		    (DISP_REG_OVL_EL1_HDR_ADDR - DISP_REG_OVL_EL0_HDR_ADDR);
		Lx_PVRIC_hdr_base += (DISP_REG_OVL_EL0_HDR_ADDR -
			DISP_REG_OVL_L0_HDR_ADDR);
		Lx_const_clr_base = baddr + DISP_REG_OVL_EL0_FBDC_CNST_CLR0
		    + cfg->ext_layer * 0x8;
	}

#ifdef CONFIG_MTK_LCM_PHYSICAL_ROTATION_HW
	if (module != DISP_MODULE_OVL1_2L)
		rotate = 1;
#endif

	header_offset = ALIGN_TO(cfg->src_pitch / 4, tile_w) *
	    ALIGN_TO(cfg->src_v_pitch, tile_h);
	header_offset /= (tile_w * tile_h);
	header_offset = (header_offset + 255) / 256 * 128;
	buf_addr = cfg->addr + header_offset;

	/* 3. Align offset & size to meet PVRIC requirement */
	src_x_align = (src_x / tile_w) * tile_w;
	src_w_align = (1 + (src_x + src_w - 1) / tile_w) * tile_w - src_x_align;
	src_y_align = (src_y / tile_h) * tile_h;
	src_h_align = (1 + (src_y + src_h - 1) / tile_h) * tile_h - src_y_align;

	if (rotate)
		tile_offset = (src_x_align + src_w_align - tile_w) / tile_w +
			(src_pitch / tile_w / 4) *
			(src_y_align + src_h_align - tile_h) /
			tile_h;
	else
		tile_offset = src_x_align / tile_w +
			(src_pitch / tile_w / 4) * src_y_align / tile_h;

	/* 4. config ovl general register */
	/* OVL_Lx_CON 0x30 */
	value = (REG_FLD_VAL((L_CON_FLD_LSRC), (cfg->source)) |
		 REG_FLD_VAL((L_CON_FLD_CFMT), (input_fmt)) |
		 REG_FLD_VAL((L_CON_FLD_AEN), (cfg->aen)) |
		 REG_FLD_VAL((L_CON_FLD_APHA), (cfg->alpha)) |
		 REG_FLD_VAL((L_CON_FLD_SKEN), (cfg->keyEn)) |
		 REG_FLD_VAL((L_CON_FLD_BTSW), (input_swap)));
	if (ufmt_is_old_fmt(format)) {
		if (format == UFMT_PARGB8888 || format == UFMT_PABGR8888 ||
		    format == UFMT_PRGBA8888 || format == UFMT_PBGRA8888 ||
		    format == UFMT_PRGBA1010102 || format == UFMT_PRGBA_FP16) {
			rgb_swap = ufmt_get_rgbswap(format);
			value |= REG_FLD_VAL((L_CON_FLD_RGB_SWAP), (rgb_swap));
		}
		value |= REG_FLD_VAL((L_CON_FLD_CLRFMT_MAN), (1));
	}

	if (rotate)
		value |= REG_FLD_VAL((L_CON_FLD_VIRTICAL_FLIP), (1)) |
			 REG_FLD_VAL((L_CON_FLD_HORI_FLIP), (1));
	DISP_REG_SET(handle, DISP_REG_OVL_L0_CON + Lx_base, value);

	value = 0;
	if (format == UFMT_RGBA1010102 || format == UFMT_PRGBA1010102)
		value = 1;
	else if (format == UFMT_RGBA_FP16 || format == UFMT_PRGBA_FP16)
		value = 3;

	if (cfg->ext_layer != -1)
		fld = FLD_ELn_CLRFMT_NB(cfg->ext_layer);
	else
		fld = FLD_Ln_CLRFMT_NB(cfg->phy_layer);
	DISP_REG_SET_FIELD(handle, fld, baddr + DISP_REG_OVL_CLRFMT_EXT, value);

	/* OVL_Lx_SRCKEY 0x34 */
	DISP_REG_SET(handle, DISP_REG_OVL_L0_SRCKEY + Lx_base, cfg->key);

	/* OVL_Lx_SRC_SIZE 0x38 */
	DISP_REG_SET(handle, DISP_REG_OVL_L0_SRC_SIZE + Lx_base,
		     src_h_align << 16 | src_w_align);

	/* OVL_Lx_SRC_OFFSET 0x3C */
	if (rotate) {
		unsigned int bg_w = 0, bg_h = 0;

		_get_roi(module, &bg_w, &bg_h);
		value = ((bg_h - dst_h - dst_y) << 16) |
		    (bg_w - dst_w - dst_x);
	} else {
		value = (dst_y << 16) | dst_x;
	}

	/* sbch can use the variable */
	cfg->real_addr = buf_addr + tile_offset * 256;
	cfg->real_dst_w = dst_w;
	cfg->real_dst_h = dst_h;

	DISP_REG_SET(handle, DISP_REG_OVL_L0_OFFSET + Lx_base, value);

	/* OVL_Lx_PITCH 0x44 */
	value = REG_FLD_VAL(L_PITCH_FLD_SA_SEL, cfg->src_alpha & 0x3) |
		REG_FLD_VAL(L_PITCH_FLD_SRGB_SEL, cfg->src_alpha & 0x3) |
		REG_FLD_VAL(L_PITCH_FLD_DA_SEL, cfg->dst_alpha & 0x3) |
		REG_FLD_VAL(L_PITCH_FLD_DRGB_SEL, cfg->dst_alpha & 0x3) |
		REG_FLD_VAL(L_PITCH_FLD_SURFL_EN, cfg->src_alpha & 0x1) |
		REG_FLD_VAL(L_PITCH_FLD_SRC_PITCH, cfg->src_pitch * tile_h);

	if (cfg->const_bld)
		value |= REG_FLD_VAL((L_PITCH_FLD_CONST_BLD), (1));
	DISP_REG_SET(handle, DISP_REG_OVL_L0_PITCH + Lx_base, value);

	/* OVL_Lx_CLIP 0x4C */
	value = 0;
	if (!rotate) {
		if (src_x > src_x_align)
			value |= REG_FLD_VAL(OVL_L_CLIP_FLD_LEFT,
				src_x - src_x_align);
		if (src_x + src_w < src_x_align + src_w_align)
			value |= REG_FLD_VAL(OVL_L_CLIP_FLD_RIGHT,
				src_x_align + src_w_align - src_x - src_w);
		if (src_y > src_y_align)
			value |= REG_FLD_VAL(OVL_L_CLIP_FLD_TOP,
				src_y - src_y_align);
		if (src_y + src_h < src_y_align + src_h_align)
			value |= REG_FLD_VAL(OVL_L_CLIP_FLD_BOTTOM,
				src_y_align + src_h_align - src_y - src_h);
	} else {
		if (src_x > src_x_align)
			value |= REG_FLD_VAL(OVL_L_CLIP_FLD_RIGHT,
				src_x - src_x_align);
		if (src_x + src_w < src_x_align + src_w_align)
			value |= REG_FLD_VAL(OVL_L_CLIP_FLD_LEFT,
				src_x_align + src_w_align - src_x - src_w);
		if (src_y > src_y_align)
			value |= REG_FLD_VAL(OVL_L_CLIP_FLD_BOTTOM,
				src_y - src_y_align);
		if (src_y + src_h < src_y_align + src_h_align)
			value |= REG_FLD_VAL(OVL_L_CLIP_FLD_TOP,
				src_y_align + src_h_align - src_y - src_h);
	}

	DISP_REG_SET(handle, DISP_REG_OVL_L0_CLIP + Lx_base, value);

	/* OVL_RDMAx_CTRL 0xC0 */
	DISP_REG_SET(handle, DISP_REG_OVL_RDMA0_CTRL + Lx_base, 0x1);

	/* OVL_Lx_CLR 0x25C */
	DISP_REG_SET(handle, DISP_REG_OVL_L0_CLR + Lx_clr_base, 0xff000000);

	/* OVL_Lx_ADDR 0xF40 */
	if (!is_engine_sec) {
		DISP_REG_SET(handle, DISP_REG_OVL_L0_ADDR + Lx_addr_base,
			     buf_addr + tile_offset * 256);
	} else {

#ifdef MTKFB_M4U_SUPPORT
		unsigned int size;
		int m4u_port;

		size = (dst_h - 1) * cfg->src_pitch + dst_w * Bpp;
		m4u_port = module_to_m4u_port(module);
		if (cfg->security != DISP_SECURE_BUFFER) {
			/*
			 * OVL is sec but this layer is non-sec
			 * we need to tell cmdq to help map non-sec mva
			 * to sec mva
			 */
			cmdqRecWriteSecure(handle,
				disp_addr_convert(DISP_REG_OVL_L0_ADDR +
						  Lx_addr_base),
				CMDQ_SAM_NMVA_2_MVA,
				buf_addr + tile_offset * 256,
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
						  Lx_addr_base),
				CMDQ_SAM_H_2_MVA,
				buf_addr + tile_offset * 256,
				0, size, m4u_port);
		}
#endif /* MTKFB_M4U_SUPPORT */
	}


	/* 5. config PVRIC register */
	/* OVL_Lx_HDR_ADDR 0xF44 */
	DISP_REG_SET(handle, DISP_REG_OVL_L0_HDR_ADDR + Lx_PVRIC_hdr_base,
		buf_addr - tile_offset / 2 - 1);

	value = (cfg->src_pitch / tile_w / 8) |
	    (((cfg->src_pitch / tile_w / 4) % 2) << 16) |
	    (((tile_offset + 1) % 2) << 20);

	/* OVL_Lx_HDR_PITCH 0xF48 */
	DISP_REG_SET(handle, DISP_REG_OVL_L0_HDR_PITCH + Lx_PVRIC_hdr_base,
		value);

	return 0;
}

int ovl_clock_on(enum DISP_MODULE_ENUM module, void *handle)
{
	DDPDBG("%s clock_on\n", ddp_get_module_name(module));
	ddp_clk_enable_by_module(module);

	if (module != DISP_MODULE_OVL1_2L)
		ddp_clk_prepare_enable(CLK_DISP_OVL_FBDC);

	return 0;
}

int ovl_clock_off(enum DISP_MODULE_ENUM module, void *handle)
{
	DDPDBG("%s clock_off\n", ddp_get_module_name(module));
	ddp_clk_disable_by_module(module);

	if (module != DISP_MODULE_OVL1_2L)
		ddp_clk_disable_unprepare(CLK_DISP_OVL_FBDC);

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
	unsigned int value = 0, mask = 0;

	if (!connect) {
		DISP_REG_SET_FIELD(handle, SRC_CON_FLD_LC_EN,
				   ovl_base + DISP_REG_OVL_SRC_CON, 0);
		DISP_REG_SET_FIELD(handle, L_CON_FLD_LSRC,
				   ovl_base + DISP_REG_OVL_LC_CON, 0);
		return 0;
	}

	SET_VAL_MASK(value, mask, 2, L_CON_FLD_LSRC);
	SET_VAL_MASK(value, mask, 0, L_CON_FLD_AEN);
	DISP_REG_MASK(handle, ovl_base + DISP_REG_OVL_LC_CON, value, mask);

	DISP_REG_SET_FIELD(handle, LC_SRC_SEL_FLD_L_SEL,
			   ovl_base + DISP_REG_OVL_LC_SRC_SEL, 0);
	DISP_REG_SET_FIELD(handle, SRC_CON_FLD_LC_EN,
			   ovl_base + DISP_REG_OVL_SRC_CON, 1);

	return 0;
}

int ovl_connect(enum DISP_MODULE_ENUM module, enum DISP_MODULE_ENUM prev,
		enum DISP_MODULE_ENUM next, int connect, void *handle)
{
	unsigned long baddr = ovl_base_addr(module);

	if (connect && is_module_ovl(prev))
		DISP_REG_SET_FIELD(handle, DATAPATH_CON_FLD_BGCLR_IN_SEL,
				   baddr + DISP_REG_OVL_DATAPATH_CON, 1);
	else
		DISP_REG_SET_FIELD(handle, DATAPATH_CON_FLD_BGCLR_IN_SEL,
				   baddr + DISP_REG_OVL_DATAPATH_CON, 0);

	if (connect && is_module_rsz(prev)) {
		_ovl_UFOd_in(module, 1, handle);
		next_rsz_module = module;
	} else {
		_ovl_UFOd_in(module, 0, handle);
	}

	if (connect && is_module_rsz(next))
		prev_rsz_module = module;

	return 0;
}

unsigned int ddp_ovl_get_cur_addr(bool rdma_mode, int layerid)
{
	/* just workaround, should remove this func */
	unsigned long baddr = ovl_base_addr(DISP_MODULE_OVL0);
	unsigned long Lx_base = 0;

	if (rdma_mode)
		return DISP_REG_GET(DISP_REG_RDMA_MEM_START_ADDR);

	Lx_base = layerid * OVL_LAYER_OFFSET + baddr;
	if (DISP_REG_GET(DISP_REG_OVL_RDMA0_CTRL + Lx_base) & 0x1)
		return DISP_REG_GET(DISP_REG_OVL_L0_ADDR + Lx_base);

	return 0;
}

void ovl_get_address(enum DISP_MODULE_ENUM module, unsigned long *addr)
{
	int i = 0;
	unsigned long baddr = ovl_base_addr(module);
	unsigned long Lx_base = 0;
	unsigned int src_on = DISP_REG_GET(DISP_REG_OVL_SRC_CON + baddr);

	for (i = 0; i < 4; i++) {
		Lx_base = i * OVL_LAYER_OFFSET + baddr;
		if (src_on & (0x1 << i))
			addr[i] = DISP_REG_GET(Lx_base + DISP_REG_OVL_L0_ADDR);
		else
			addr[i] = 0;
	}
}

void ovl_get_info(enum DISP_MODULE_ENUM module, void *data)
{
	int i = 0;
	struct OVL_BASIC_STRUCT *pdata = data;
	unsigned long baddr = ovl_base_addr(module);
	unsigned long Lx_base = 0;
	unsigned int src_on = DISP_REG_GET(DISP_REG_OVL_SRC_CON + baddr);
	struct OVL_BASIC_STRUCT *p = NULL;

	for (i = 0; i < ovl_layer_num(module); i++) {
		unsigned int val = 0;

		Lx_base = i * OVL_LAYER_OFFSET + baddr;
		p = &pdata[i];
		p->layer = i;
		p->layer_en = src_on & (0x1 << i);
		if (!p->layer_en) {
			DDPMSG("%s:layer%d,en %d,w %d,h %d,bpp %d,addr %lu\n",
			       __func__, i, p->layer_en, p->src_w, p->src_h,
			       p->bpp, p->addr);
			continue;
		}

		val = DISP_REG_GET(DISP_REG_OVL_L0_CON + Lx_base);
		p->fmt = display_fmt_reg_to_unified_fmt(
				REG_FLD_VAL_GET(L_CON_FLD_CFMT, val),
				REG_FLD_VAL_GET(L_CON_FLD_BTSW, val),
				REG_FLD_VAL_GET(L_CON_FLD_RGB_SWAP, val));
		p->bpp = UFMT_GET_bpp(p->fmt) / 8;
		p->alpha = REG_FLD_VAL_GET(L_CON_FLD_AEN, val);

		p->addr = DISP_REG_GET(Lx_base + DISP_REG_OVL_L0_ADDR);

		val = DISP_REG_GET(DISP_REG_OVL_L0_SRC_SIZE + Lx_base);
		p->src_w = REG_FLD_VAL_GET(SRC_SIZE_FLD_SRC_W, val);
		p->src_h = REG_FLD_VAL_GET(SRC_SIZE_FLD_SRC_H, val);

		val = DISP_REG_GET(DISP_REG_OVL_L0_PITCH + Lx_base);
		p->src_pitch = REG_FLD_VAL_GET(L_PITCH_FLD_SRC_PITCH, val);

		val = DISP_REG_GET(DISP_REG_OVL_DATAPATH_CON + Lx_base);
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

static int ovl_check_input_param(struct OVL_CONFIG_STRUCT *cfg)
{
	if ((cfg->addr == 0 && cfg->source == 0) ||
	    cfg->dst_w == 0 || cfg->dst_h == 0) {
		DDP_PR_ERR("ovl parameter invalidate,addr=0x%08lx,w=%d,h=%d\n",
			   cfg->addr, cfg->dst_w, cfg->dst_h);
		ASSERT(0);
		return -1;
	}

	return 0;
}

/* use noinline to reduce stack size */
static noinline void
print_layer_config_args(int module, struct OVL_CONFIG_STRUCT *cfg,
			struct disp_rect *roi)
{
	DDPDBG("%s:L%d/e%d/%u,source=%s,(%u,%u,%ux%u)(%u,%u,%ux%u),pitch=%u\n",
	       ddp_get_module_name(module), cfg->phy_layer, cfg->ext_layer,
	       cfg->layer, (cfg->source == 0) ? "mem" : "dim",
	       cfg->src_x, cfg->src_y, cfg->src_w, cfg->src_h,
	       cfg->dst_x, cfg->dst_y, cfg->dst_w, cfg->dst_h, cfg->src_pitch);
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
			cmdq_event_nonsec_end = ovl_to_cmdq_event_nonsec_end(
									module);
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
		if (ovl_config->module == module && ovl_config->layer_en &&
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
		DDPAEE("[SVP]fail to %s: ret=%d\n",
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
	int phy_layer = -1, ext_layer = -1, ext_layer_idx = 0;
	struct OVL_CONFIG_STRUCT *cfg = NULL;

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

	/* reset which module belonged to */
	for (local_layer = 0; local_layer < TOTAL_OVL_LAYER_NUM;
	     local_layer++) {
		cfg = &pConfig->ovl_config[local_layer];
		cfg->module = -1;
	}

	/*
	 * layer layout for @module:
	 *   dispatch which module belonged to, phy layer, and ext layer
	 */
	for (local_layer = 0; local_layer <= ovl_layer_num(module);
	     global_layer++) {
		if (global_layer >= TOTAL_OVL_LAYER_NUM)
			break;

		cfg = &pConfig->ovl_config[global_layer];

		/* check if there is any extended layer on last phy layer */
		if (local_layer == ovl_layer_num(module)) {
			if (!disp_helper_get_option(DISP_OPT_OVL_EXT_LAYER) ||
			    cfg->ext_sel_layer == -1)
				break;
		}

		ext_layer = -1;
		cfg->phy_layer = 0;
		cfg->ext_layer = -1;

		pConfig->ovl_layer_scanned |= (1 << global_layer);

		if (cfg->layer_en == 0) {
			local_layer++;
			continue;
		}

		if (disp_helper_get_option(DISP_OPT_OVL_EXT_LAYER) &&
		    cfg->ext_sel_layer != -1) {
			const int len = 100;
			char msg[len];
			int n = 0;

			if (phy_layer != cfg->ext_sel_layer) {
				n = snprintf(msg, len,
					     "L%d layout not match: ");
				n += snprintf(msg + n, len - n,
					      "cur_phy=%d, ext_sel=%d\n",
					      global_layer, phy_layer,
					      cfg->ext_sel_layer);
				DDP_PR_ERR("%s", msg);
				phy_layer++;
				ext_layer = -1;
			} else {
				ext_layer = ext_layer_idx++;
			}
		} else {
			phy_layer = local_layer++;
		}

		cfg->module = module;
		cfg->phy_layer = phy_layer;
		cfg->ext_layer = ext_layer;
		DDPDBG("layout:L%d->%s,phy:%d,ext:%d,ext_sel:%d\n",
		       global_layer, ddp_get_module_name(module),
		       phy_layer, ext_layer,
		       (ext_layer >= 0) ? phy_layer : cfg->ext_sel_layer);
	}

	/* for ASSERT_LAYER, do it specially */
	if (is_DAL_Enabled() && module == DISP_MODULE_OVL0) {
		unsigned int dal = primary_display_get_option("ASSERT_LAYER");

		cfg = &pConfig->ovl_config[dal];
		cfg->module = module;
		cfg->phy_layer = ovl_layer_num(cfg->module) - 1;
		cfg->ext_sel_layer = -1;
		cfg->ext_layer = -1;
	}

	return 1;
}

void config_fake_layer(enum DISP_MODULE_ENUM module, int *phy_layer_en,
		int *ext_layer_en, struct disp_ddp_path_config *pConfig,
		void *handle)
{
	if (fake_layer_mask) {
		int i = 0;

		if (fake_layer_overwrite) {
			*phy_layer_en = 0;
			*ext_layer_en = 0;
		}

		for (i = 0; i < TOTAL_OVL_LAYER_NUM; i++) {
			struct OVL_CONFIG_STRUCT cfg;
			int Bpp;
			int layer_id;
			int local_module;
			struct disp_rect rsz_src_roi;

			if (!(fake_layer_mask & (1 << i)))
				continue;

			/* path: OVL0_2L->RSZ->OVL0_4L */
			if (i < 2) {
				layer_id = i;
				local_module = DISP_MODULE_OVL0_2L;
			} else {
				layer_id = i - 2;
				local_module = DISP_MODULE_OVL0;
			}

			if (local_module != module)
				continue;

			if (*phy_layer_en & (1 << layer_id))
				continue;

			memset(&rsz_src_roi, 0, sizeof(rsz_src_roi));
			memset(&cfg, 0, sizeof(cfg));
			/* make fake layer */
			cfg.layer = layer_id;
			cfg.isDirty = 1;
			cfg.buff_idx = 0;
			cfg.layer_en = 1;
			cfg.fmt = UFMT_RGBA8888;
			Bpp = UFMT_GET_Bpp(cfg.fmt);
			cfg.addr = (unsigned int)get_fake_layer_mva(i);
			cfg.vaddr = 0;
			cfg.src_x = 0;
			cfg.src_y = 0;
			cfg.src_w = pConfig->dst_w;
			cfg.src_h = pConfig->dst_h;
			cfg.src_pitch = pConfig->dst_w * Bpp;
			cfg.dst_x = 0;
			cfg.dst_y = 0;
			cfg.dst_w = pConfig->dst_w;
			cfg.dst_h = pConfig->dst_h;
			cfg.keyEn = 0;
			cfg.key = 0;
			cfg.aen = 1;
			cfg.sur_aen = 0;
			cfg.alpha = 255;
			cfg.src_alpha = 0;
			cfg.dst_alpha = 0;
			cfg.security = DISP_NORMAL_BUFFER;
			cfg.yuv_range = 0;
			cfg.source = OVL_LAYER_SOURCE_MEM; /* from memory */
			cfg.ext_sel_layer = -1;
			cfg.ext_layer = -1;
			cfg.phy_layer = layer_id;
			print_layer_config_args(local_module, &cfg, NULL);
			ovl_layer_config(local_module, layer_id,
				rsz_src_roi, 0, &cfg, NULL, NULL, handle);
			*phy_layer_en |= 1 << layer_id;
		}
	}
}

static void sBCH_enable(enum DISP_MODULE_ENUM module,
			struct OVL_CONFIG_STRUCT *cfg, int *set_reg,
			struct sbch *data,
			struct disp_ddp_path_config *pConfig)
{
	int update = 0;
	int cnst_en = 1;
	int trans_en = 0;

	switch (cfg->fmt) {
	case UFMT_RGBA4444:
	case UFMT_RGBA5551:
	case UFMT_RGBA8888:
	case UFMT_BGRA8888:
	case UFMT_ARGB8888:
	case UFMT_ABGR8888:
	case UFMT_PARGB8888:
	case UFMT_PABGR8888:
	case UFMT_PRGBA8888:
	case UFMT_PBGRA8888:
		trans_en = cfg->aen ? 1 : 0;
		break;
	default:
		trans_en = 0;
		break;
	}

	/* RGBX format, can't set sbch_trans_en to 1 */
	if (cfg->const_bld)
		trans_en = 0;

	if (cfg->layer_disable_by_partial_update) {
		update = 0;
		cnst_en = 0;
		trans_en = 0;
	}

	/* set reg*/
	if (cfg->ext_layer == -1) {
		set_reg[UPDATE] |= update << (cfg->phy_layer * 4);
		set_reg[TRANS_EN] |= trans_en << (16 + cfg->phy_layer * 4);
		set_reg[CNST_EN] |= cnst_en << (17 + cfg->phy_layer * 4);
		data->sbch_en_cnt++;
	} else {
		set_reg[UPDATE] |= update << (cfg->ext_layer * 4);
		set_reg[TRANS_EN] |= trans_en << (16 + cfg->ext_layer * 4);
		set_reg[CNST_EN] |= cnst_en << (17 + cfg->ext_layer * 4);
	}
}

/* note:disable layer isn't same with disable sbch ,so need set update,bch_en */
static void sBCH_disable(struct sbch *bch_info, int ext_layer_num,
			struct OVL_CONFIG_STRUCT *cfg, int *set_reg)
{
	struct sbch *data = bch_info;
	int update = 1;
	int trans_en = 0;
	int cnst_en = 0;

	if (cfg->layer_disable_by_partial_update) {
		update = 0;
		trans_en = 0;
		cnst_en = 0;
	}

	data->pre_addr = cfg->real_addr;
	data->dst_x = cfg->real_dst_x;
	data->dst_y = cfg->real_dst_y;
	data->dst_w = cfg->real_dst_w;
	data->dst_h = cfg->real_dst_h;
	data->height = cfg->src_h;
	data->width = cfg->src_w;
	data->fmt = cfg->fmt;
	data->ext_layer_num = ext_layer_num;
	data->phy_layer = cfg->phy_layer;
	data->const_bld = cfg->const_bld;
	data->sbch_en_cnt = 0;
	data->full_trans_en = 0;
	data->layer_disable_by_partial_update =
		cfg->layer_disable_by_partial_update;

	if (cfg->ext_layer == -1) {
		set_reg[UPDATE] |= update << (cfg->phy_layer * 4);
		set_reg[TRANS_EN] |= trans_en << (16 + cfg->phy_layer * 4);
		set_reg[CNST_EN] |= cnst_en << (17 + cfg->phy_layer * 4);
	} else {
		set_reg[UPDATE] |= update << (cfg->ext_layer * 4);
		set_reg[TRANS_EN] |= trans_en << (16 + cfg->ext_layer * 4);
		set_reg[CNST_EN] |= cnst_en << (17 + cfg->ext_layer * 4);
	}
}

static unsigned long long full_trans_bw_calc(struct sbch *data,
		 enum DISP_MODULE_ENUM module, struct OVL_CONFIG_STRUCT *cfg,
		 struct disp_ddp_path_config *pConfig)
{
	unsigned long long bw_sum = 0;
	unsigned int bpp = 0;
	unsigned int dum_val = 0;
	/* don't check status for cmd mode */
	unsigned int status = 0;

	if (data->sbch_en_cnt == SBCH_EN_NUM) {
		pConfig->read_dum_reg[module] = 1;
	} else if (data->sbch_en_cnt == SBCH_EN_NUM + 1) {

		if (primary_display_is_video_mode())
			cmdqBackupReadSlot(pgc->ovl_status_info,
					0, &status);
		if (!(0x01 & status)) {
			cmdqBackupReadSlot(pgc->ovl_dummy_info,
				module, &dum_val);
			data->full_trans_en =
				((0x01 << cfg->phy_layer) & dum_val);

			if (data->full_trans_en)
				DISPINFO("trans layer %d\n", cfg->phy_layer);
		}
	}

	/* calculate the layer bw */
	if (data->full_trans_en) {
		bpp = ufmt_get_Bpp(cfg->fmt);
		bw_sum = (unsigned long long)cfg->dst_w * cfg->dst_h * bpp;
	}

	return bw_sum;
}

static void check_bch_reg(enum DISP_MODULE_ENUM module, int *phy_reg,
		int *ext_reg, struct disp_ddp_path_config *pConfig)
{

	static int phy_bit_dbg[OVL_NUM] = { 0 };
	static int ext_bit_dbg[OVL_NUM] = { 0 };
	int phy_value =
		DISP_REG_GET(ovl_base_addr(module) + DISP_REG_OVL_SBCH);
	int ext_value =
		DISP_REG_GET(ovl_base_addr(module) + DISP_REG_OVL_SBCH_EXT);

	if (((phy_value != phy_bit_dbg[module]) ||
		(ext_value != ext_bit_dbg[module])) &&
		pConfig->sbch_enable) {
		DDPDBG("sbch reg set fail phy:%x--%x, ext:%x--%x\n",
			phy_value, phy_bit_dbg[module],
			ext_value, ext_bit_dbg[module]);
		mmprofile_log_ex(ddp_mmp_get_events()->sbch_set_error,
			MMPROFILE_FLAG_PULSE, phy_value, phy_bit_dbg[module]);
		/* disp_aee_print("sbch set error ovl%d\n",module); */
	}

	/* store will set reg value */
	phy_bit_dbg[module] =
		(phy_reg[UPDATE]|phy_reg[TRANS_EN]|phy_reg[CNST_EN]);
	ext_bit_dbg[module] =
		(ext_reg[UPDATE]|ext_reg[TRANS_EN]|ext_reg[CNST_EN]);

	DDPDBG("set bch %s reg phy:%x -- %x, ext:%x -- %x\n",
				ddp_get_module_name(module),
				phy_bit_dbg[module], phy_value,
				ext_bit_dbg[module], ext_value);

	if (phy_bit_dbg[module] || phy_value)
		mmprofile_log_ex(ddp_mmp_get_events()->sbch_set,
			MMPROFILE_FLAG_PULSE, phy_bit_dbg[module], phy_value);

}

static void ext_layer_bch_disable(struct sbch *sbch_data,
		int *ext_reg, struct disp_ddp_path_config *pConfig,
		int ext_num, int *layer)
{
	int j;

	for (j = 0; j < ext_num; j++) {
		struct OVL_CONFIG_STRUCT *ext_cfg =
					&pConfig->ovl_config[*layer + 1];

		(*layer)++;
		sBCH_disable(&sbch_data[*layer], -1, ext_cfg, ext_reg);
	}
}

static int get_ext_num(struct disp_ddp_path_config *pConfig, int layer)
{
	int j;
	int ext_num = 0;

	for (j = 0; j < 3; j++) {
		struct OVL_CONFIG_STRUCT *ext_cfg =
					&pConfig->ovl_config[layer + j + 1];

		if (pConfig->ovl_config[layer].phy_layer ==
			ext_cfg->ext_sel_layer &&
			ext_cfg->ext_layer != -1 &&
			pConfig->ovl_config[layer].module ==
			ext_cfg->module)
			ext_num++;
	}
	return ext_num;
}

static int check_ext_update(struct sbch *sbch_data, int ext_num,
		int layer, struct disp_ddp_path_config *pConfig)
{
	int j;

	for (j = 0; j < ext_num; j++) {
		int ext_l = layer + j + 1;
		struct OVL_CONFIG_STRUCT *ext_cfg =
					&pConfig->ovl_config[ext_l];

		if (sbch_data[ext_l].dst_x != ext_cfg->real_dst_x ||
			sbch_data[ext_l].dst_y != ext_cfg->real_dst_y ||
			sbch_data[ext_l].dst_w != ext_cfg->real_dst_w ||
			sbch_data[ext_l].dst_h != ext_cfg->real_dst_h ||
			sbch_data[ext_l].height != ext_cfg->src_h ||
			sbch_data[ext_l].width != ext_cfg->src_w ||
			sbch_data[ext_l].fmt != ext_cfg->fmt ||
			sbch_data[ext_l].pre_addr != ext_cfg->real_addr ||
			sbch_data[ext_l].layer_disable_by_partial_update
			!= ext_cfg->layer_disable_by_partial_update)
			return 1;
	}
	return 0;
}

static void ext_layer_bch_en(enum DISP_MODULE_ENUM module,
		int *ext_reg, int ext_num,
		struct disp_ddp_path_config *pConfig, int *layer,
		struct sbch *data)
{
	int j;

	for (j = 0; j < ext_num; j++) {
		struct OVL_CONFIG_STRUCT *ext_cfg =
					&pConfig->ovl_config[*layer + 1];

		(*layer)++;
		sBCH_enable(module, ext_cfg, ext_reg,
			&data[*layer + 1], pConfig);
	}
}

static void layer_disable_bch(struct sbch *sbch_data, int ext_num,
		struct OVL_CONFIG_STRUCT *ovl_cfg, int *layer,
		struct disp_ddp_path_config *pConfig, int *phy_reg,
		int *ext_reg)
{
	/*update phy layer */
	sBCH_disable(&sbch_data[*layer], ext_num, ovl_cfg, phy_reg);
	/* update all ext layer on this phy */
	ext_layer_bch_disable(sbch_data, ext_reg, pConfig,
					ext_num, layer);
}
static void ext_layer_compare(struct sbch *sbch_data, int *phy_reg,
		int *ext_reg, struct disp_ddp_path_config *pConfig,
		struct OVL_CONFIG_STRUCT *ovl_cfg, int *layer, int ext_num,
		unsigned long long *bw_sum, enum DISP_MODULE_ENUM module)
{
	int ext_update = 0;

	  /* if the phy layer's ext layer num is same with pre frame,
	   * check ext layer addr ,height , format have changed or not,
	   * any ext layer height,addr,fmt changed,
	   * the phy and its ext don't use BCH.
	   */
	if (sbch_data[*layer].ext_layer_num == ext_num) {
		ext_update = check_ext_update(sbch_data, ext_num, (*layer),
							pConfig);
		if (!ext_update) {
			sBCH_enable(module, ovl_cfg, phy_reg,
				&sbch_data[*layer], pConfig);
			/* enable ext layer BCH on this phy */
			ext_layer_bch_en(module, ext_reg, ext_num, pConfig,
				layer, &sbch_data[*layer]);
			return;
		}
	}

	/* the phy layer's ext layer num isn't same with pre frame,
	 * update the phy layer and all ext layer on this phy.
	 */
	layer_disable_bch(sbch_data, ext_num, ovl_cfg,
				layer, pConfig, phy_reg, ext_reg);
}


static void layer_no_update(struct sbch *sbch_data, int *phy_reg,
		int *ext_reg, struct disp_ddp_path_config *pConfig,
		struct OVL_CONFIG_STRUCT *ovl_cfg, int *layer,
		unsigned long long *bw_sum, enum DISP_MODULE_ENUM module)
{
	int ext_num = 0;

	/* check the phy layer have ext layer or not */
	ext_num = get_ext_num(pConfig, *layer);

	/*  the phy layer don't have ext layer, it can use BCH */
	if (!ext_num && !sbch_data[*layer].ext_layer_num) {
		sBCH_enable(module, ovl_cfg, phy_reg,
			&sbch_data[*layer], pConfig);
		*bw_sum += full_trans_bw_calc(&sbch_data[*layer],
					module, ovl_cfg, pConfig);
	} else if (!ext_num && sbch_data[*layer].ext_layer_num) {
		/* phy don't have ext, but pre frame has ext
		 * or pre frame is ext layer.
		 */
		sBCH_disable(&sbch_data[*layer], ext_num,
					ovl_cfg, phy_reg);
	} else {
		ext_layer_compare(sbch_data, phy_reg, ext_reg, pConfig, ovl_cfg,
					layer, ext_num, bw_sum, module);
	}
}
static void layer_update(struct sbch *sbch_data, int *phy_reg,
		int *ext_reg, struct disp_ddp_path_config *pConfig,
		struct OVL_CONFIG_STRUCT *ovl_cfg, int *layer)
{
	int ext_num = 0;

	/* check the phy layer has ext layer or not */
	ext_num = get_ext_num(pConfig, *layer);
	/* update the phy layer and all ext layer on this phy */
	layer_disable_bch(sbch_data, ext_num, ovl_cfg,
				layer, pConfig, phy_reg, ext_reg);
}

static int sbch_calc(enum DISP_MODULE_ENUM module,
		struct sbch *sbch_data, struct disp_ddp_path_config *pConfig,
		void *handle, struct sbch_bw *sbch_bw_data)
{
	int i;
	int phy_bit[BCH_BIT_NUM] = {0};
	int ext_bit[BCH_BIT_NUM] = {0};
	unsigned long long trans_bw = 0;
	unsigned long long trans_fbdc_bw = 0;

	for (i = 0; i < TOTAL_OVL_LAYER_NUM; i++) {
		struct OVL_CONFIG_STRUCT *ovl_cfg = &pConfig->ovl_config[i];

		/*1. limit 18:9 */
		if (ovl_cfg->dst_h > SBCH_HEIGHT ||
			ovl_cfg->dst_w > SBCH_WIDTH || !ovl_cfg->layer_en) {
			ovl_cfg->real_addr = 0;
			ovl_cfg->real_dst_x = 0;
			ovl_cfg->real_dst_y = 0;
			memset(&sbch_data[i], 0, sizeof(struct sbch));
			continue;
		}

		if (ovl_cfg->module != module) {
			memset(&sbch_data[i], 0, sizeof(struct sbch));
			continue;
		}

		/* for Assert_layer config special case, do it specially */
		if (is_DAL_Enabled() && module == DISP_MODULE_OVL0 &&
			i == primary_display_get_option("ASSERT_LAYER"))
			continue;

		DDPDBG("sbch pre L%d,%lx,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
			i, sbch_data[i].pre_addr,
			sbch_data[i].dst_x, sbch_data[i].dst_y,
			sbch_data[i].dst_w, sbch_data[i].dst_h,
			sbch_data[i].height, sbch_data[i].width,
			sbch_data[i].fmt, sbch_data[i].phy_layer,
			sbch_data[i].const_bld);

		DDPDBG("sbch now L%d,%lx,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
			i, ovl_cfg->real_addr,
			ovl_cfg->real_dst_x, ovl_cfg->real_dst_y,
			ovl_cfg->real_dst_w, ovl_cfg->real_dst_h,
			ovl_cfg->src_h, ovl_cfg->src_w,
			ovl_cfg->fmt, ovl_cfg->phy_layer,
			ovl_cfg->const_bld);

		/*
		 * the layer address,height,fmt,const bld all don't change,
		 * maybe use BCH.
		 */
		if (sbch_data[i].pre_addr == ovl_cfg->real_addr &&
			sbch_data[i].dst_x == ovl_cfg->real_dst_x &&
			sbch_data[i].dst_y == ovl_cfg->real_dst_y &&
			sbch_data[i].dst_w == ovl_cfg->real_dst_w &&
			sbch_data[i].dst_h == ovl_cfg->real_dst_h &&
			sbch_data[i].height == ovl_cfg->src_h &&
			sbch_data[i].width == ovl_cfg->src_w &&
			sbch_data[i].fmt == ovl_cfg->fmt &&
			sbch_data[i].phy_layer == ovl_cfg->phy_layer &&
			sbch_data[i].layer_disable_by_partial_update ==
			ovl_cfg->layer_disable_by_partial_update &&
			sbch_data[i].const_bld == ovl_cfg->const_bld) {
			if (ovl_cfg->ext_layer == -1) {
				if (ovl_cfg->compress)
					layer_no_update(sbch_data,
						phy_bit, ext_bit,
						pConfig, ovl_cfg, &i,
						&trans_fbdc_bw, module);
				else
					layer_no_update(sbch_data,
						phy_bit, ext_bit,
						pConfig, ovl_cfg, &i,
						&trans_bw, module);
			}
		} else {
			/* the layer addr or height has changed */
			if (ovl_cfg->ext_layer == -1)
				layer_update(sbch_data, phy_bit, ext_bit,
					pConfig, ovl_cfg, &i);
		}
	}

	/* for debug: check sbch reg is set or not */
	check_bch_reg(module, phy_bit, ext_bit, pConfig);

	/* set bch reg*/
	DISP_REG_SET(handle, ovl_base_addr(module) + DISP_REG_OVL_SBCH,
		phy_bit[UPDATE] | phy_bit[TRANS_EN] | phy_bit[CNST_EN]);
	DISP_REG_SET(handle, ovl_base_addr(module) + DISP_REG_OVL_SBCH_EXT,
		ext_bit[UPDATE] | ext_bit[TRANS_EN] | ext_bit[CNST_EN]);
	/* clear slot */
	cmdqBackupWriteSlot(pgc->ovl_dummy_info, module, 0);

	sbch_bw_data->trans_bw = trans_bw;
	sbch_bw_data->trans_fbdc_bw = trans_fbdc_bw;

	return 0;
}

static int ovl_config_l(enum DISP_MODULE_ENUM module,
			struct disp_ddp_path_config *pConfig, void *handle)
{
	int phy_layer_en = 0;
	int has_sec_layer = 0;
	int layer_id;
	int ovl_layer = 0;
	int ext_layer_en = 0, ext_sel_layers = 0;
	int phy_fbdc_en = 0, ext_fbdc_en = 0;
	unsigned long baddr = ovl_base_addr(module);
	unsigned long Lx_base = 0;
	struct golden_setting_context *golden_setting =
		pConfig->p_golden_setting_context;
	unsigned int Bpp, fps;
	unsigned long long tmp_bw, ovl_bw, ovl_fbdc_bw;
	struct sbch_bw sbch_bw_info;

	if (pConfig->dst_dirty)
		ovl_roi(module, pConfig->dst_w, pConfig->dst_h, gOVL_bg_color,
			handle);

	if (!pConfig->ovl_dirty)
		return 0;

	ovl_layer_layout(module, pConfig);

	/* be careful, turn off all layers */
	for (ovl_layer = 0; ovl_layer < ovl_layer_num(module); ovl_layer++) {
		Lx_base = baddr + ovl_layer * OVL_LAYER_OFFSET;
		DISP_REG_SET(handle, DISP_REG_OVL_RDMA0_CTRL + Lx_base, 0x0);
	}

	has_sec_layer = setup_ovl_sec(module, pConfig, handle);

	if (golden_setting->fps)
		fps = golden_setting->fps;
	else {
		DDPDBG("no fps information, set fps as default 60\n");
		fps = 60;
	}

	ovl_bw = 0;
	ovl_fbdc_bw = 0;

	if (!pConfig->ovl_partial_dirty && module == prev_rsz_module)
		_ovl_set_rsz_roi(module, pConfig, handle);

	for (layer_id = 0; layer_id < TOTAL_OVL_LAYER_NUM; layer_id++) {
		struct OVL_CONFIG_STRUCT *cfg = &pConfig->ovl_config[layer_id];
		int enable = cfg->layer_en;

		/* backup ovl_cfg for primary display */
		if (module != DISP_MODULE_OVL1_2L)
			memcpy(&g_primary_ovl_cfg[layer_id], cfg,
				sizeof(struct OVL_CONFIG_STRUCT));
		else
			memcpy(&g_second_ovl_cfg[layer_id], cfg,
				sizeof(struct OVL_CONFIG_STRUCT));

		if (enable == 0)
			continue;
		if (ovl_check_input_param(cfg)) {
			DDPAEE("invalid layer parameters!\n");
			continue;
		}
		if (cfg->module != module)
			continue;

		if (cfg->compress) {
			ovl_layer_config_compress(module, cfg->phy_layer,
						pConfig->rsz_src_roi,
						has_sec_layer, cfg,
						handle);
			if (cfg->ext_layer == -1)
				phy_fbdc_en |= (1 << cfg->phy_layer);
			else
				ext_fbdc_en |= (1 << (cfg->ext_layer + 4));
		} else if (pConfig->ovl_partial_dirty) {
			struct disp_rect layer_roi = {0, 0, 0, 0};
			struct disp_rect layer_partial_roi = {0, 0, 0, 0};

			layer_roi.x = cfg->dst_x;
			layer_roi.y = cfg->dst_y;
			layer_roi.width = cfg->dst_w;
			layer_roi.height = cfg->dst_h;
			if (rect_intersect(&layer_roi,
					   &pConfig->ovl_partial_roi,
					   &layer_partial_roi)) {
				print_layer_config_args(module, cfg,
							&layer_partial_roi);
				ovl_layer_config(module, cfg->phy_layer,
						pConfig->rsz_src_roi,
						has_sec_layer, cfg,
						&pConfig->ovl_partial_roi,
						&layer_partial_roi, handle);
				cfg->layer_disable_by_partial_update = 0;
			} else {
				/* this layer will not be displayed */
				enable = 0;
				/*update layer_en sbch will skip this layer*/
				cfg->layer_disable_by_partial_update = 1;
			}

			if (cfg->ext_layer == -1)
				phy_fbdc_en &= ~(1 << cfg->phy_layer);
			else
				ext_fbdc_en &= ~(1 << (cfg->ext_layer + 4));
		} else {
			print_layer_config_args(module, cfg, NULL);
			ovl_layer_config(module, cfg->phy_layer,
					 pConfig->rsz_src_roi, has_sec_layer,
					 cfg, NULL, NULL, handle);

			if (cfg->ext_layer == -1)
				phy_fbdc_en &= ~(1 << cfg->phy_layer);
			else
				ext_fbdc_en &= ~(1 << (cfg->ext_layer + 4));
		}

		if (cfg->ext_layer != -1) {
			ext_layer_en |= enable << cfg->ext_layer;
			ext_sel_layers |= cfg->phy_layer <<
						(16 + 4 * cfg->ext_layer);
		} else {
			phy_layer_en |= enable << cfg->phy_layer;
		}

		Bpp = ufmt_get_Bpp(cfg->fmt);
		tmp_bw = (unsigned long long)cfg->dst_h *
			cfg->dst_w * Bpp;
		do_div(tmp_bw, 1000);
		tmp_bw *= 1250;
		do_div(tmp_bw, 1000);
		if (cfg->compress)
			ovl_fbdc_bw = ovl_fbdc_bw + tmp_bw;
		else
			ovl_bw = ovl_bw + tmp_bw;

		DDPDBG("h:%u, w:%u, fps:%u, Bpp:%u, bw:%llu\n",
			cfg->dst_h, cfg->dst_w, fps, Bpp, tmp_bw);
	}

	config_fake_layer(module, &phy_layer_en, &ext_layer_en,
			pConfig, handle);

	if (!pConfig->ovl_partial_dirty && module == next_rsz_module)
		_ovl_lc_config(module, pConfig, handle);

	_rpo_disable_dim_L0(module, pConfig, &phy_layer_en);

	ovl_color_manage(module, pConfig, handle);

	DDPDBG("%s:layer_en=0x%01x,ext_layer_en=0x%01x,ext_sel_layers=0x%04x\n",
	       ddp_get_module_name(module), phy_layer_en, ext_layer_en,
	       ext_sel_layers >> 16);
	DISP_REG_SET_FIELD(handle, SRC_CON_FLD_L_EN,
			   ovl_base_addr(module) + DISP_REG_OVL_SRC_CON,
			   phy_layer_en);
	DISP_REG_SET_FIELD(handle, DATAPATH_CON_FLD_LX_FBDC_EN,
		baddr + DISP_REG_OVL_DATAPATH_CON, phy_fbdc_en);
	/* ext layer control */
	DISP_REG_SET(handle, baddr + DISP_REG_OVL_DATAPATH_EXT_CON,
		     ext_sel_layers | ext_layer_en | ext_fbdc_en);

	memset(&sbch_bw_info, 0x0, sizeof(struct sbch_bw));
	if (disp_helper_get_option(DISP_OPT_OVL_SBCH)) {
		static struct sbch sbch_info[OVL_NUM][TOTAL_OVL_LAYER_NUM];
		int ovl_index = ovl_to_index(module);

		if (!pConfig->sbch_enable) {
			DISPINFO("sbch disable\n");
			memset(sbch_info, 0, sizeof(sbch_info));
			DISP_REG_SET(handle, ovl_base_addr(module) +
					DISP_REG_OVL_SBCH, 0);
			DISP_REG_SET(handle, ovl_base_addr(module) +
					DISP_REG_OVL_SBCH_EXT, 0);
		} else {
			sbch_calc(module, sbch_info[ovl_index],
					pConfig, handle, &sbch_bw_info);
			if (sbch_bw_info.trans_bw) {
				do_div(sbch_bw_info.trans_bw, 1000);
				sbch_bw_info.trans_bw *= 1250;
				do_div(sbch_bw_info.trans_bw, 1000);
			}
			if (sbch_bw_info.trans_fbdc_bw) {
				do_div(sbch_bw_info.trans_fbdc_bw, 1000);
				sbch_bw_info.trans_fbdc_bw *= 1250;
				do_div(sbch_bw_info.trans_fbdc_bw, 1000);
			}

			ovl_fbdc_bw -= sbch_bw_info.trans_fbdc_bw;
			ovl_bw -= sbch_bw_info.trans_bw;
		}
	} else {
	/* if don't enable bch feature, set bch reg to default value(0) */
		DISP_REG_SET(handle,
			ovl_base_addr(module) + DISP_REG_OVL_SBCH, 0);
		DISP_REG_SET(handle,
			ovl_base_addr(module) + DISP_REG_OVL_SBCH_EXT, 0);
	}

	DDPDBG("%s transparent bw:(%llu,%llu), total bw:(%llu,%llu)\n",
		ddp_get_module_name(module),
		sbch_bw_info.trans_bw, sbch_bw_info.trans_fbdc_bw,
		ovl_bw, ovl_fbdc_bw);

	/* bandwidth report */
	if (module == DISP_MODULE_OVL0) {
		DISP_SLOT_SET(handle, DISPSYS_SLOT_BASE,
			DISP_SLOT_IS_DC, golden_setting->is_dc);
		DISP_SLOT_SET(handle, DISPSYS_SLOT_BASE,
			DISP_SLOT_OVL0_BW, (unsigned int)ovl_bw);
		DISP_SLOT_SET(handle, DISPSYS_SLOT_BASE,
			DISP_SLOT_OVL0_FBDC_BW, (unsigned int)ovl_fbdc_bw);
	} else if (module == DISP_MODULE_OVL0_2L) {
		DISP_SLOT_SET(handle, DISPSYS_SLOT_BASE,
			DISP_SLOT_OVL0_2L_BW, (unsigned int)ovl_bw);
		DISP_SLOT_SET(handle, DISPSYS_SLOT_BASE,
			DISP_SLOT_OVL0_2L_FBDC_BW, (unsigned int)ovl_fbdc_bw);
	}

	return 0;
}

int ovl_build_cmdq(enum DISP_MODULE_ENUM module, void *cmdq_trigger_handle,
		   enum CMDQ_STATE state)
{
	int ret = 0;
	/* int reg_pa = DISP_REG_OVL_FLOW_CTRL_DBG & 0x1fffffff; */

	if (!cmdq_trigger_handle) {
		DDP_PR_ERR("cmdq_trigger_handle is NULL\n");
		return -1;
	}

	if (state == CMDQ_CHECK_IDLE_AFTER_STREAM_EOF) {
		if (module == DISP_MODULE_OVL0) {
			ret = cmdqRecPoll(cmdq_trigger_handle, 0x14007240, 2,
					  0x3f);
		} else {
			DDP_PR_ERR("wrong module: %s\n",
				   ddp_get_module_name(module));
			return -1;
		}
	}

	return 0;
}

void ovl_dump_golden_setting(enum DISP_MODULE_ENUM module)
{
	unsigned long base = ovl_base_addr(module);
	unsigned long rg0 = 0, rg1 = 0, rg2 = 0, rg3 = 0, rg4 = 0;
	int i = 0;
	const int len = 160;
	char msg[len];
	int n = 0;

	DDPDUMP("-- %s Golden Setting --\n", ddp_get_module_name(module));
	for (i = 0; i < ovl_layer_num(module); i++) {
		rg0 = DISP_REG_OVL_RDMA0_MEM_GMC_SETTING + i * OVL_LAYER_OFFSET;
		rg1 = DISP_REG_OVL_RDMA0_FIFO_CTRL + i * OVL_LAYER_OFFSET;
		rg2 = DISP_REG_OVL_RDMA0_MEM_GMC_S2 + i * 0x4;
		rg3 = DISP_REG_OVL_RDMAn_BUF_LOW(i);
		rg4 = DISP_REG_OVL_RDMAn_BUF_HIGH(i);
		n = snprintf(msg, len,
			     "0x%03lx:0x%08x 0x%03lx:0x%08x 0x%03lx:0x%08x ",
			     rg0, DISP_REG_GET(rg0 + base),
			     rg1, DISP_REG_GET(rg1 + base),
			     rg2, DISP_REG_GET(rg2 + base));
		n += snprintf(msg + n, len - n,
			      "0x%03lx:0x%08x 0x%03lx:0x%08x\n",
			      rg3, DISP_REG_GET(rg3 + base),
			      rg4, DISP_REG_GET(rg4 + base));
		DDPDUMP("%s", msg);
	}

	rg0 = DISP_REG_OVL_RDMA_BURST_CON1;
	DDPDUMP("0x%03lx:0x%08x\n", rg0, DISP_REG_GET(rg0 + base));

	rg0 = DISP_REG_OVL_RDMA_GREQ_NUM;
	rg1 = DISP_REG_OVL_RDMA_GREQ_URG_NUM;
	rg2 = DISP_REG_OVL_RDMA_ULTRA_SRC;
	DDPDUMP("0x%03lx:0x%08x 0x%03lx:0x%08x 0x%03lx:0x%08x\n",
		rg0, DISP_REG_GET(rg0 + base),
		rg1, DISP_REG_GET(rg1 + base),
		rg2, DISP_REG_GET(rg2 + base));

	rg0 = DISP_REG_OVL_EN;
	rg1 = DISP_REG_OVL_DATAPATH_CON;
	rg2 = DISP_REG_OVL_FBDC_CFG1;
	DDPDUMP("0x%03lx:0x%08x 0x%03lx:0x%08x 0x%03lx:0x%08x\n",
		rg0, DISP_REG_GET(rg0 + base),
		rg1, DISP_REG_GET(rg1 + base),
		rg2, DISP_REG_GET(rg2 + base));

	DDPDUMP("RDMA0_MEM_GMC_SETTING1\n");
	DDPDUMP("[9:0]:%x [25:16]:%x [28]:%x [31]:%x\n",
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_MEM_GMC_ULTRA_THRESHOLD,
		base + DISP_REG_OVL_RDMA0_MEM_GMC_SETTING),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_MEM_GMC_PRE_ULTRA_THRESHOLD,
		base + DISP_REG_OVL_RDMA0_MEM_GMC_SETTING),
		DISP_REG_GET_FIELD(
		    FLD_OVL_RDMA_MEM_GMC_ULTRA_THRESHOLD_HIGH_OFS,
		base + DISP_REG_OVL_RDMA0_MEM_GMC_SETTING),
		DISP_REG_GET_FIELD(
		    FLD_OVL_RDMA_MEM_GMC_PRE_ULTRA_THRESHOLD_HIGH_OFS,
		base + DISP_REG_OVL_RDMA0_MEM_GMC_SETTING));


	DDPDUMP("RDMA0_FIFO_CTRL\n");
	DDPDUMP("[9:0]:%u [25:16]:%u\n",
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_FIFO_THRD,
			base + DISP_REG_OVL_RDMA0_FIFO_CTRL),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_FIFO_SIZE,
			base + DISP_REG_OVL_RDMA0_FIFO_CTRL));

	DDPDUMP("RDMA0_MEM_GMC_SETTING2\n");
	DDPDUMP("[11:0]:%u [27:16]:%u [28]:%u [29]:%u [30]:%u\n",
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_MEM_GMC2_ISSUE_REQ_THRES,
			base + DISP_REG_OVL_RDMA0_MEM_GMC_S2),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_MEM_GMC2_ISSUE_REQ_THRES_URG,
			base + DISP_REG_OVL_RDMA0_MEM_GMC_S2),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_MEM_GMC2_REQ_THRES_PREULTRA,
			base + DISP_REG_OVL_RDMA0_MEM_GMC_S2),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_MEM_GMC2_REQ_THRES_ULTRA,
			base + DISP_REG_OVL_RDMA0_MEM_GMC_S2),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_MEM_GMC2_FORCE_REQ_THRES,
			base + DISP_REG_OVL_RDMA0_MEM_GMC_S2));

	DDPDUMP("OVL_RDMA_BURST_CON1\n");
	DDPDUMP("[28]:%u\n",
		DISP_REG_GET_FIELD(FLD_RDMA_BURST_CON1_BURST16_EN,
		    base + DISP_REG_OVL_RDMA_BURST_CON1));

	DDPDUMP("RDMA_GREQ_NUM\n");
	DDPDUMP("[3:0]%u [7:4]%u [11:8]%u [15:12]%u [23:16]%x [26:24]%u\n",
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_GREQ_LAYER0_GREQ_NUM,
			base + DISP_REG_OVL_RDMA_GREQ_NUM),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_GREQ_LAYER1_GREQ_NUM,
			base + DISP_REG_OVL_RDMA_GREQ_NUM),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_GREQ_LAYER2_GREQ_NUM,
			base + DISP_REG_OVL_RDMA_GREQ_NUM),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_GREQ_LAYER3_GREQ_NUM,
			base + DISP_REG_OVL_RDMA_GREQ_NUM),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_GREQ_OSTD_GREQ_NUM,
			base + DISP_REG_OVL_RDMA_GREQ_NUM),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_GREQ_GREQ_DIS_CNT,
			base + DISP_REG_OVL_RDMA_GREQ_NUM));

	DDPDUMP("[27]%u [28]%u [29]%u [30]%u [31]%u\n",
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_GREQ_STOP_EN,
			base + DISP_REG_OVL_RDMA_GREQ_NUM),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_GREQ_GRP_END_STOP,
			base + DISP_REG_OVL_RDMA_GREQ_NUM),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_GREQ_GRP_BRK_STOP,
			base + DISP_REG_OVL_RDMA_GREQ_NUM),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_GREQ_IOBUF_FLUSH_PREULTRA,
			base + DISP_REG_OVL_RDMA_GREQ_NUM),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_GREQ_IOBUF_FLUSH_ULTRA,
			base + DISP_REG_OVL_RDMA_GREQ_NUM));

	DDPDUMP("RDMA_GREQ_URG_NUM\n");
	DDPDUMP("[3:0]:%u [7:4]:%u [11:8]:%u [15:12]:%u [25:16]:%u [28]:%u\n",
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_GREQ_LAYER0_GREQ_URG_NUM,
			base + DISP_REG_OVL_RDMA_GREQ_URG_NUM),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_GREQ_LAYER1_GREQ_URG_NUM,
			base + DISP_REG_OVL_RDMA_GREQ_URG_NUM),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_GREQ_LAYER2_GREQ_URG_NUM,
			base + DISP_REG_OVL_RDMA_GREQ_URG_NUM),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_GREQ_LAYER3_GREQ_URG_NUM,
			base + DISP_REG_OVL_RDMA_GREQ_URG_NUM),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_GREQ_ARG_GREQ_URG_TH,
			base + DISP_REG_OVL_RDMA_GREQ_URG_NUM),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_GREQ_ARG_URG_BIAS,
				base + DISP_REG_OVL_RDMA_GREQ_URG_NUM));

	DDPDUMP("RDMA_ULTRA_SRC\n");
	DDPDUMP("[1:0]%u [3:2]%u [5:4]%u [7:6]%u [9:8]%u\n",
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_PREULTRA_BUF_SRC,
			base + DISP_REG_OVL_RDMA_ULTRA_SRC),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_PREULTRA_SMI_SRC,
			base + DISP_REG_OVL_RDMA_ULTRA_SRC),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_PREULTRA_ROI_END_SRC,
			base + DISP_REG_OVL_RDMA_ULTRA_SRC),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_PREULTRA_RDMA_SRC,
			base + DISP_REG_OVL_RDMA_ULTRA_SRC),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_ULTRA_BUF_SRC,
			base + DISP_REG_OVL_RDMA_ULTRA_SRC));
	DDPDUMP("[11:10]%u [13:12]%u [15:14]%u\n",
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_ULTRA_SMI_SRC,
			base + DISP_REG_OVL_RDMA_ULTRA_SRC),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_ULTRA_ROI_END_SRC,
			base + DISP_REG_OVL_RDMA_ULTRA_SRC),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_ULTRA_RDMA_SRC,
			base + DISP_REG_OVL_RDMA_ULTRA_SRC));


	DDPDUMP("RDMA0_BUF_LOW\n");
	DDPDUMP("[11:0]:%x [23:12]:%x\n",
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_BUF_LOW_ULTRA_TH,
			base + DISP_REG_OVL_RDMAn_BUF_LOW(0)),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_BUF_LOW_PREULTRA_TH,
			base + DISP_REG_OVL_RDMAn_BUF_LOW(0)));

	DDPDUMP("RDMA0_BUF_HIGH\n");
	DDPDUMP("[23:12]:%x [31]:%x\n",
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_BUF_HIGH_PREULTRA_TH,
			base + DISP_REG_OVL_RDMAn_BUF_HIGH(0)),
		DISP_REG_GET_FIELD(FLD_OVL_RDMA_BUF_HIGH_PREULTRA_DIS,
			base + DISP_REG_OVL_RDMAn_BUF_HIGH(0)));

	DDPDUMP("OVL_EN\n");
	DDPDUMP("[18]:%x [19]:%x\n",
		DISP_REG_GET_FIELD(EN_FLD_BLOCK_EXT_ULTRA,
			base + DISP_REG_OVL_EN),
		DISP_REG_GET_FIELD(EN_FLD_BLOCK_EXT_PREULTRA,
			base + DISP_REG_OVL_EN));


	DDPDUMP("DATAPATH_CON\n");
	DDPDUMP("[0]:%u, [3]:%u [24]:%u [26]:%u\n",
		DISP_REG_GET_FIELD(DATAPATH_CON_FLD_LAYER_SMI_ID_EN,
			base + DISP_REG_OVL_DATAPATH_CON),
		DISP_REG_GET_FIELD(DATAPATH_CON_FLD_OUTPUT_NO_RND,
			base + DISP_REG_OVL_DATAPATH_CON),
		DISP_REG_GET_FIELD(DATAPATH_CON_FLD_GCLAST_EN,
			base + DISP_REG_OVL_DATAPATH_CON),
		DISP_REG_GET_FIELD(DATAPATH_CON_FLD_OUTPUT_CLAMP,
			base + DISP_REG_OVL_DATAPATH_CON));

	DDPDUMP("OVL_FBDC_CFG1\n");
	DDPDUMP("[24]:%u, [28]:%u\n",
		DISP_REG_GET_FIELD(FLD_FBDC_8XE_MODE,
			base + DISP_REG_OVL_FBDC_CFG1),
		DISP_REG_GET_FIELD(FLD_FBDC_FILTER_EN,
			base + DISP_REG_OVL_FBDC_CFG1));
}

void ovl_dump_reg(enum DISP_MODULE_ENUM module)
{
	const int len =  100;
	char msg[len];
	int n = 0;

	if (disp_helper_get_option(DISP_OPT_REG_PARSER_RAW_DUMP)) {
		unsigned long module_base = ovl_base_addr(module);

		DDPDUMP("== START: DISP %s REGS ==\n",
			ddp_get_module_name(module));
		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0x0, INREG32(module_base + 0x0),
			     0x4, INREG32(module_base + 0x4));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0x8, INREG32(module_base + 0x8),
			      0xC, INREG32(module_base + 0xC));
		DDPDUMP("%s", msg);

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0x10, INREG32(module_base + 0x10),
			     0x14, INREG32(module_base + 0x14));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0x20, INREG32(module_base + 0x20),
			      0x24, INREG32(module_base + 0x24));
		DDPDUMP("%s", msg);

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0x28, INREG32(module_base + 0x28),
			     0x2C, INREG32(module_base + 0x2C));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0x30, INREG32(module_base + 0x30),
			      0x34, INREG32(module_base + 0x34));
		DDPDUMP("%s", msg);

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0x38, INREG32(module_base + 0x38),
			     0x3C, INREG32(module_base + 0x3C));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0xF40, INREG32(module_base + 0xF40),
			      0x44, INREG32(module_base + 0x44));
		DDPDUMP("%s", msg);

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0x48, INREG32(module_base + 0x48),
			     0x4C, INREG32(module_base + 0x4C));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0x50, INREG32(module_base + 0x50),
			      0x54, INREG32(module_base + 0x54));
		DDPDUMP("%s", msg);

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0x58, INREG32(module_base + 0x58),
			     0x5C, INREG32(module_base + 0x5C));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0xF60, INREG32(module_base + 0xF60),
			      0x64, INREG32(module_base + 0x64));
		DDPDUMP("%s", msg);

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0x68, INREG32(module_base + 0x68),
			     0x6C, INREG32(module_base + 0x6C));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0x70, INREG32(module_base + 0x70),
			      0x74, INREG32(module_base + 0x74));
		DDPDUMP("%s", msg);

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0x78, INREG32(module_base + 0x78),
			     0x7C, INREG32(module_base + 0x7C));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0xF80, INREG32(module_base + 0xF80),
			      0x84, INREG32(module_base + 0x84));
		DDPDUMP("%s", msg);

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0x88, INREG32(module_base + 0x88),
			     0x8C, INREG32(module_base + 0x8C));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0x90, INREG32(module_base + 0x90),
			      0x94, INREG32(module_base + 0x94));
		DDPDUMP("%s", msg);

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0x98, INREG32(module_base + 0x98),
			     0x9C, INREG32(module_base + 0x9C));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0xFa0, INREG32(module_base + 0xFa0),
			      0xa4, INREG32(module_base + 0xa4));
		DDPDUMP("%s", msg);

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0xa8, INREG32(module_base + 0xa8),
			     0xAC, INREG32(module_base + 0xAC));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0xc0, INREG32(module_base + 0xc0),
			      0xc8, INREG32(module_base + 0xc8));
		DDPDUMP("%s", msg);

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0xcc, INREG32(module_base + 0xcc),
			     0xd0, INREG32(module_base + 0xd0));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0xe0, INREG32(module_base + 0xe0),
			      0xe8, INREG32(module_base + 0xe8));
		DDPDUMP("%s", msg);

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0xec, INREG32(module_base + 0xec),
			     0xf0, INREG32(module_base + 0xf0));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0x100, INREG32(module_base + 0x100),
			      0x108, INREG32(module_base + 0x108));
		DDPDUMP("%s", msg);

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0x10c, INREG32(module_base + 0x10c),
			     0x110, INREG32(module_base + 0x110));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0x120, INREG32(module_base + 0x120),
			      0x128, INREG32(module_base + 0x128));
		DDPDUMP("%s", msg);

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0x12c, INREG32(module_base + 0x12c),
			     0x130, INREG32(module_base + 0x130));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0x134, INREG32(module_base + 0x134),
			      0x138, INREG32(module_base + 0x138));
		DDPDUMP("%s", msg);

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0x13c, INREG32(module_base + 0x13c),
			     0x140, INREG32(module_base + 0x140));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0x144, INREG32(module_base + 0x144),
			      0x148, INREG32(module_base + 0x148));
		DDPDUMP("%s", msg);

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0x14c, INREG32(module_base + 0x14c),
			     0x150, INREG32(module_base + 0x150));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0x154, INREG32(module_base + 0x154),
			      0x158, INREG32(module_base + 0x158));
		DDPDUMP("%s", msg);

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0x15c, INREG32(module_base + 0x15c),
			     0x160, INREG32(module_base + 0x160));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0x164, INREG32(module_base + 0x164),
			      0x168, INREG32(module_base + 0x168));
		DDPDUMP("%s", msg);

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0x16c, INREG32(module_base + 0x16c),
			     0x170, INREG32(module_base + 0x170));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0x174, INREG32(module_base + 0x174),
			      0x178, INREG32(module_base + 0x178));
		DDPDUMP("%s", msg);

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0x17c, INREG32(module_base + 0x17c),
			     0x180, INREG32(module_base + 0x180));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0x184, INREG32(module_base + 0x184),
			      0x188, INREG32(module_base + 0x188));
		DDPDUMP("%s", msg);

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0x18c, INREG32(module_base + 0x18c),
			     0x190, INREG32(module_base + 0x190));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0x194, INREG32(module_base + 0x194),
			      0x198, INREG32(module_base + 0x198));
		DDPDUMP("%s", msg);

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0x19c, INREG32(module_base + 0x19c),
			     0x1a0, INREG32(module_base + 0x1a0));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0x1a4, INREG32(module_base + 0x1a4),
			      0x1a8, INREG32(module_base + 0x1a8));
		DDPDUMP("%s", msg);

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0x1ac, INREG32(module_base + 0x1ac),
			     0x1b0, INREG32(module_base + 0x1b0));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0x1b4, INREG32(module_base + 0x1b4),
			      0x1b8, INREG32(module_base + 0x1b8));
		DDPDUMP("%s", msg);

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0x1bc, INREG32(module_base + 0x1bc),
			     0x1c0, INREG32(module_base + 0x1c0));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0x1c4, INREG32(module_base + 0x1c4),
			      0x1c8, INREG32(module_base + 0x1c8));
		DDPDUMP("%s", msg);

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0x1cc, INREG32(module_base + 0x1cc),
			     0x1d0, INREG32(module_base + 0x1d0));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0x1d4, INREG32(module_base + 0x1d4),
			      0x1e0, INREG32(module_base + 0x1e0));
		DDPDUMP("%s", msg);

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0x1e4, INREG32(module_base + 0x1e4),
			     0x1e8, INREG32(module_base + 0x1e8));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0x1ec, INREG32(module_base + 0x1ec),
			      0x1F0, INREG32(module_base + 0x1F0));
		DDPDUMP("%s", msg);

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0x1F4, INREG32(module_base + 0x1F4),
			     0x1F8, INREG32(module_base + 0x1F8));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0x1FC, INREG32(module_base + 0x1FC),
			      0x200, INREG32(module_base + 0x200));
		DDPDUMP("%s", msg);

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0x208, INREG32(module_base + 0x208),
			     0x20C, INREG32(module_base + 0x20C));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0x210, INREG32(module_base + 0x210),
			      0x214, INREG32(module_base + 0x214));
		DDPDUMP("%s", msg);

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0x218, INREG32(module_base + 0x218),
			     0x21C, INREG32(module_base + 0x21C));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0x230, INREG32(module_base + 0x230),
			      0x234, INREG32(module_base + 0x234));
		DDPDUMP("%s", msg);

		DDPDUMP("OVL0: 0x%04x=0x%08x\n",
			0x23C, INREG32(module_base + 0x23C));

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0x238, INREG32(module_base + 0x238),
			     0x240, INREG32(module_base + 0x240));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0x244, INREG32(module_base + 0x244),
			      0x24c, INREG32(module_base + 0x24c));
		DDPDUMP("%s", msg);

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0x250, INREG32(module_base + 0x250),
			     0x254, INREG32(module_base + 0x254));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0x258, INREG32(module_base + 0x258),
			      0x25c, INREG32(module_base + 0x25c));
		DDPDUMP("%s", msg);

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0x260, INREG32(module_base + 0x260),
			     0x264, INREG32(module_base + 0x264));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0x268, INREG32(module_base + 0x268),
			      0x26C, INREG32(module_base + 0x26C));
		DDPDUMP("%s", msg);

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0x270, INREG32(module_base + 0x270),
			     0x280, INREG32(module_base + 0x280));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0x284, INREG32(module_base + 0x284),
			      0x288, INREG32(module_base + 0x288));
		DDPDUMP("%s", msg);

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0x28C, INREG32(module_base + 0x28C),
			     0x290, INREG32(module_base + 0x290));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0x29C, INREG32(module_base + 0x29C),
			      0x2A0, INREG32(module_base + 0x2A0));
		DDPDUMP("%s", msg);

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0x2A4, INREG32(module_base + 0x2A4),
			     0x2B0, INREG32(module_base + 0x2B0));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0x2B4, INREG32(module_base + 0x2B4),
			      0x2B8, INREG32(module_base + 0x2B8));
		DDPDUMP("%s", msg);

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0x2BC, INREG32(module_base + 0x2BC),
			     0x2C0, INREG32(module_base + 0x2C0));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0x2C4, INREG32(module_base + 0x2C4),
			      0x2C8, INREG32(module_base + 0x2C8));
		DDPDUMP("%s", msg);

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0x324, INREG32(module_base + 0x324),
			     0x330, INREG32(module_base + 0x330));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0x334, INREG32(module_base + 0x334),
			      0x338, INREG32(module_base + 0x338));
		DDPDUMP("%s", msg);

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0x33C, INREG32(module_base + 0x33C),
			     0xFB0, INREG32(module_base + 0xFB0));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0x344, INREG32(module_base + 0x344),
			      0x348, INREG32(module_base + 0x348));
		DDPDUMP("%s", msg);

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0x34C, INREG32(module_base + 0x34C),
			     0x350, INREG32(module_base + 0x350));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0x354, INREG32(module_base + 0x354),
			      0x358, INREG32(module_base + 0x358));
		DDPDUMP("%s", msg);

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0x35C, INREG32(module_base + 0x35C),
			     0xFB4, INREG32(module_base + 0xFB4));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0x364, INREG32(module_base + 0x364),
			      0x368, INREG32(module_base + 0x368));
		DDPDUMP("%s", msg);

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0x36C, INREG32(module_base + 0x36C),
			     0x370, INREG32(module_base + 0x370));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0x374, INREG32(module_base + 0x374),
			      0x378, INREG32(module_base + 0x378));
		DDPDUMP("%s", msg);

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0x37C, INREG32(module_base + 0x37C),
			     0xFB8, INREG32(module_base + 0xFB8));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0x384, INREG32(module_base + 0x384),
			      0x388, INREG32(module_base + 0x388));
		DDPDUMP("%s", msg);

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0x38C, INREG32(module_base + 0x38C),
			     0x390, INREG32(module_base + 0x390));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0x394, INREG32(module_base + 0x394),
			      0x398, INREG32(module_base + 0x398));
		DDPDUMP("%s", msg);

		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x3A0, INREG32(module_base + 0x3A0),
			0x3A4, INREG32(module_base + 0x3A4),
			0x3A8, INREG32(module_base + 0x3A8));

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0xF44, INREG32(module_base + 0xF44),
			     0xF48, INREG32(module_base + 0xF48));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0xF64, INREG32(module_base + 0xF64),
			      0xF68, INREG32(module_base + 0xF68));
		DDPDUMP("%s", msg);

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0xF84, INREG32(module_base + 0xF84),
			     0xF88, INREG32(module_base + 0xF88));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0xFA4, INREG32(module_base + 0xFA4),
			      0xFA8, INREG32(module_base + 0xFA8));
		DDPDUMP("%s", msg);

		n = snprintf(msg, len, "OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, ",
			     0xFD0, INREG32(module_base + 0xFD0),
			     0xFD4, INREG32(module_base + 0xFD4));
		n += snprintf(msg + n, len - n,
			      "0x%04x=0x%08x, 0x%04x=0x%08x\n",
			      0xFD8, INREG32(module_base + 0xFD8),
			      0xFDC, INREG32(module_base + 0xFDC));
		DDPDUMP("%s", msg);

		DDPDUMP("OVL0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0xFE0, INREG32(module_base + 0xFE0),
			0xFE4, INREG32(module_base + 0xFE4),
			0xFC0, INREG32(module_base + 0xFC0));
		DDPDUMP("-- END: DISP %s REGS --\n",
			ddp_get_module_name(module));
	} else {
		unsigned int i = 0;
		unsigned long base = ovl_base_addr(module);
		unsigned int src_on = DISP_REG_GET(DISP_REG_OVL_SRC_CON + base);

		DDPDUMP("== DISP %s REGS ==\n", ddp_get_module_name(module));
		DDPDUMP("0x%03lx:0x%08x 0x%08x 0x%08x 0x%08x\n",
			DISP_REG_OVL_STA,
			DISP_REG_GET(DISP_REG_OVL_STA + base),
			DISP_REG_GET(DISP_REG_OVL_INTEN + base),
			DISP_REG_GET(DISP_REG_OVL_INTSTA + base),
			DISP_REG_GET(DISP_REG_OVL_EN + base));
		DDPDUMP("0x%03lx:0x%08x 0x%08x\n",
			DISP_REG_OVL_TRIG,
			DISP_REG_GET(DISP_REG_OVL_TRIG + base),
			DISP_REG_GET(DISP_REG_OVL_RST + base));
		DDPDUMP("0x%03lx:0x%08x 0x%08x 0x%08x 0x%08x\n",
			DISP_REG_OVL_ROI_SIZE,
			DISP_REG_GET(DISP_REG_OVL_ROI_SIZE + base),
			DISP_REG_GET(DISP_REG_OVL_DATAPATH_CON + base),
			DISP_REG_GET(DISP_REG_OVL_ROI_BGCLR + base),
			DISP_REG_GET(DISP_REG_OVL_SRC_CON + base));

		for (i = 0; i < ovl_layer_num(module); i++) {
			unsigned long l_addr = 0;

			if (!(src_on & (0x1 << i)))
				continue;

			l_addr = i * OVL_LAYER_OFFSET + base;
			DDPDUMP("0x%03x:0x%08x 0x%08x 0x%08x 0x%08x\n",
				0x030 + i * 0x20,
				DISP_REG_GET(DISP_REG_OVL_L0_CON + l_addr),
				DISP_REG_GET(DISP_REG_OVL_L0_SRCKEY + l_addr),
				DISP_REG_GET(DISP_REG_OVL_L0_SRC_SIZE + l_addr),
				DISP_REG_GET(DISP_REG_OVL_L0_OFFSET + l_addr));

			DDPDUMP("0x%03x:0x%08x 0x%03x:0x%08x\n",
				0xf40 + i * 0x20,
				DISP_REG_GET(DISP_REG_OVL_L0_ADDR + l_addr),
				0x044 + i * 0x20,
				DISP_REG_GET(DISP_REG_OVL_L0_PITCH + l_addr));

			DDPDUMP("0x%03x:0x%08x 0x%03x:0x%08x\n",
				0x0c0 + i * 0x20,
				DISP_REG_GET(DISP_REG_OVL_RDMA0_CTRL + l_addr),
				0x24c + i * 0x4,
				DISP_REG_GET(DISP_REG_OVL_RDMA0_DBG +
					     base + i * 0x4));
		}

		DDPDUMP("0x1d4:0x%08x 0x200:0x%08x 0x208:0x%08x\n",
			DISP_REG_GET(DISP_REG_OVL_DEBUG_MON_SEL + base),
			DISP_REG_GET(DISP_REG_OVL_DUMMY_REG + base),
			DISP_REG_GET(DISP_REG_OVL_GDRDY_PRD + base));

		n = snprintf(msg, len, "0x230:0x%08x 0x%08x 0x240:0x%08x ",
			     DISP_REG_GET(DISP_REG_OVL_SMI_DBG + base),
			     DISP_REG_GET(DISP_REG_OVL_GREQ_LAYER_CNT + base),
			     DISP_REG_GET(DISP_REG_OVL_FLOW_CTRL_DBG + base));
		n += snprintf(msg + n, len - n, "0x%08x 0x2a0:0x%08x 0x%08x\n",
			      DISP_REG_GET(DISP_REG_OVL_ADDCON_DBG + base),
			      DISP_REG_GET(DISP_REG_OVL_FUNC_DCM0 + base),
			      DISP_REG_GET(DISP_REG_OVL_FUNC_DCM1 + base));
		DDPDUMP("%s", msg);

		DDPDUMP("0x3A0: 0x%08x, 0x3A4: 0x%08x, 0x3A8: 0x%08x\n",
			DISP_REG_GET(DISP_REG_OVL_SBCH + base),
			DISP_REG_GET(DISP_REG_OVL_SBCH_EXT + base),
			DISP_REG_GET(DISP_REG_OVL_SBCH_STS + base));
	}
	ovl_dump_golden_setting(module);
}

static void ovl_printf_status(unsigned int status)
{
	const int len = 160;
	char msg[len];
	int n = 0;

	DDPDUMP("- OVL_FLOW_CONTROL_DEBUG -\n");
	n = snprintf(msg, len, "addcon_idle:%d,blend_idle:%d,out_valid:%d,",
		     (status >> 10) & (0x1), (status >> 11) & (0x1),
		     (status >> 12) & (0x1));
	n += snprintf(msg + n, len - n, "out_ready:%d,out_idle:%d\n",
		      (status >> 13) & (0x1), (status >> 15) & (0x1));
	DDPDUMP("%s", msg);
	DDPDUMP("rdma_idle3-0:(%d,%d,%d,%d),rst:%d\n",
		(status >> 16) & (0x1), (status >> 17) & (0x1),
		(status >> 18) & (0x1), (status >> 19) & (0x1),
		(status >> 20) & (0x1));
	n = snprintf(msg, len, "trig:%d,frame_hwrst_done:%d,",
		     (status >> 21) & (0x1), (status >> 23) & (0x1));
	n += snprintf(msg + n, len - n,
		      "frame_swrst_done:%d,frame_underrun:%d,frame_done:%d\n",
		      (status >> 24) & (0x1), (status >> 25) & (0x1),
		      (status >> 26) & (0x1));
	DDPDUMP("%s", msg);
	n = snprintf(msg, len, "ovl_running:%d,ovl_start:%d,",
		     (status >> 27) & (0x1), (status >> 28) & (0x1));
	n += snprintf(msg + n, len - n,
		      "ovl_clr:%d,reg_update:%d,ovl_upd_reg:%d\n",
		      (status >> 29) & (0x1), (status >> 30) & (0x1),
		      (status >> 31) & (0x1));
	DDPDUMP("%s", msg);

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
	DDPDUMP("warm_rst_cs:%d,layer_greq:%d,out_data:0x%x\n",
		status & 0x7, (status >> 3) & 0x1, (status >> 4) & 0xffffff);
	DDPDUMP("out_ready:%d,out_valid:%d,smi_busy:%d,smi_greq:%d\n",
		(status >> 28) & 0x1, (status >> 29) & 0x1,
		(status >> 30) & 0x1, (status >> 31) & 0x1);
}


static void ovl_dump_layer_info_compress(enum DISP_MODULE_ENUM module,
			int layer, bool is_ext_layer)
{
	unsigned int compr_en = 0;
	unsigned long baddr = ovl_base_addr(module);
	unsigned long Lx_PVRIC_hdr_base = 0;

	if (is_ext_layer) {
		compr_en = DISP_REG_GET_FIELD(REG_FLD(1, layer + 4),
			baddr + DISP_REG_OVL_DATAPATH_EXT_CON);
		Lx_PVRIC_hdr_base = baddr + layer *
		    (DISP_REG_OVL_EL1_HDR_ADDR - DISP_REG_OVL_EL0_HDR_ADDR);
		Lx_PVRIC_hdr_base += (DISP_REG_OVL_EL0_HDR_ADDR -
			DISP_REG_OVL_L0_HDR_ADDR);
	} else {
		compr_en = DISP_REG_GET_FIELD(REG_FLD(1, layer + 4),
			baddr + DISP_REG_OVL_DATAPATH_CON);
		Lx_PVRIC_hdr_base = baddr + layer *
		    (DISP_REG_OVL_L1_HDR_ADDR - DISP_REG_OVL_L0_HDR_ADDR);
	}

	if (compr_en == 0) {
		DDPDUMP("compr_en:%u\n", compr_en);
		return;
	}

	DDPDUMP("compr_en:%u, hdr_addr:0x%x, hdr_pitch:0x%x\n", compr_en,
		DISP_REG_GET(DISP_REG_OVL_L0_HDR_ADDR + Lx_PVRIC_hdr_base),
		DISP_REG_GET(DISP_REG_OVL_L0_HDR_PITCH + Lx_PVRIC_hdr_base));
}

static void ovl_dump_layer_info(enum DISP_MODULE_ENUM module, int layer,
				bool is_ext_layer)
{
	unsigned int con, src_size, offset, pitch, addr;
	enum UNIFIED_COLOR_FMT fmt;
	unsigned long baddr = ovl_base_addr(module);
	unsigned long Lx_base = 0;
	unsigned long Lx_addr_base = 0;
	const int len = 100;
	char msg[len];
	int n = 0;

	if (is_ext_layer) {
		Lx_base = baddr + layer * OVL_LAYER_OFFSET;
		Lx_base += (DISP_REG_OVL_EL0_CON - DISP_REG_OVL_L0_CON);

		Lx_addr_base = baddr + layer * 0x4;
		Lx_addr_base += (DISP_REG_OVL_EL0_ADDR - DISP_REG_OVL_L0_ADDR);
	} else {
		Lx_base = baddr + layer * OVL_LAYER_OFFSET;
		Lx_addr_base = baddr + layer * OVL_LAYER_OFFSET;
	}

	con = DISP_REG_GET(DISP_REG_OVL_L0_CON + Lx_base);
	offset = DISP_REG_GET(DISP_REG_OVL_L0_OFFSET + Lx_base);
	src_size = DISP_REG_GET(DISP_REG_OVL_L0_SRC_SIZE + Lx_base);
	pitch = DISP_REG_GET(DISP_REG_OVL_L0_PITCH + Lx_base);
	addr = DISP_REG_GET(DISP_REG_OVL_L0_ADDR + Lx_addr_base);

	fmt = display_fmt_reg_to_unified_fmt(
			REG_FLD_VAL_GET(L_CON_FLD_CFMT, con),
			REG_FLD_VAL_GET(L_CON_FLD_BTSW, con),
			REG_FLD_VAL_GET(L_CON_FLD_RGB_SWAP, con));

	n = snprintf(msg, len, "%s_L%d:(%u,%u,%ux%u),pitch=%u,",
		     is_ext_layer ? "ext" : "phy", layer,
		     offset & 0xfff, (offset >> 16) & 0xfff,
		     src_size & 0xfff, (src_size >> 16) & 0xfff,
		     pitch & 0xffff);
	n += snprintf(msg + n, len - n,
		      "addr=0x%08x,fmt=%s,source=%s,aen=%u,alpha=%u\n",
		      addr, unified_color_fmt_name(fmt),
		      (REG_FLD_VAL_GET(L_CON_FLD_LSRC, con) == 0) ?
		      "mem" : "constant_color",
		      REG_FLD_VAL_GET(L_CON_FLD_AEN, con),
		      REG_FLD_VAL_GET(L_CON_FLD_APHA, con));
	DDPDUMP("%s", msg);

	ovl_dump_layer_info_compress(module, layer, is_ext_layer);
}

void ovl_dump_analysis(enum DISP_MODULE_ENUM module)
{
	int i = 0;
	unsigned long Lx_base = 0;
	unsigned long rdma_offset = 0;
	unsigned long baddr = ovl_base_addr(module);
	unsigned int src_con = DISP_REG_GET(DISP_REG_OVL_SRC_CON + baddr);
	unsigned int ext_con = DISP_REG_GET(DISP_REG_OVL_DATAPATH_EXT_CON +
					    baddr);
	unsigned int addcon = DISP_REG_GET(DISP_REG_OVL_ADDCON_DBG + baddr);

	DDPDUMP("== DISP %s ANALYSIS ==\n", ddp_get_module_name(module));
	DDPDUMP("ovl_en=%d,layer_en(%d,%d,%d,%d),bg(%dx%d)\n",
		DISP_REG_GET(DISP_REG_OVL_EN + baddr) & 0x1,
		src_con & 0x1, (src_con >> 1) & 0x1,
		(src_con >> 2) & 0x1, (src_con >> 3) & 0x1,
		DISP_REG_GET(DISP_REG_OVL_ROI_SIZE + baddr) & 0xfff,
		(DISP_REG_GET(DISP_REG_OVL_ROI_SIZE + baddr) >> 16) & 0xfff);
	DDPDUMP("ext_layer:layer_en(%d,%d,%d),attach_layer(%d,%d,%d)\n",
		ext_con & 0x1, (ext_con >> 1) & 0x1, (ext_con >> 2) & 0x1,
		(ext_con >> 16) & 0xf, (ext_con >> 20) & 0xf,
		(ext_con >> 24) & 0xf);
	DDPDUMP("cur_pos(%u,%u),layer_hit(%u,%u,%u,%u),bg_mode=%s,sta=0x%x\n",
		REG_FLD_VAL_GET(ADDCON_DBG_FLD_ROI_X, addcon),
		REG_FLD_VAL_GET(ADDCON_DBG_FLD_ROI_Y, addcon),
		REG_FLD_VAL_GET(ADDCON_DBG_FLD_L0_WIN_HIT, addcon),
		REG_FLD_VAL_GET(ADDCON_DBG_FLD_L1_WIN_HIT, addcon),
		REG_FLD_VAL_GET(ADDCON_DBG_FLD_L2_WIN_HIT, addcon),
		REG_FLD_VAL_GET(ADDCON_DBG_FLD_L3_WIN_HIT, addcon),
		DISP_REG_GET_FIELD(DATAPATH_CON_FLD_BGCLR_IN_SEL,
				   DISP_REG_OVL_DATAPATH_CON + baddr) ?
				   "DL" : "const",
		DISP_REG_GET(DISP_REG_OVL_STA + baddr));

	/* phy layer */
	for (i = 0; i < ovl_layer_num(module); i++) {
		unsigned int rdma_ctrl;

		if (src_con & (0x1 << i))
			ovl_dump_layer_info(module, i, false);
		else
			DDPDUMP("phy_L%d:disabled\n", i);

		Lx_base = i * OVL_LAYER_OFFSET + baddr;
		rdma_ctrl = DISP_REG_GET(Lx_base + DISP_REG_OVL_RDMA0_CTRL);
		DDPDUMP("ovl rdma%d status:(en=%d,fifo_used:%d,GMC=0x%x)\n",
			i, REG_FLD_VAL_GET(RDMA0_CTRL_FLD_RDMA_EN, rdma_ctrl),
			REG_FLD_VAL_GET(RDMA0_CTRL_FLD_RMDA_FIFO_USED_SZ,
					rdma_ctrl),
			DISP_REG_GET(Lx_base +
				     DISP_REG_OVL_RDMA0_MEM_GMC_SETTING));

		rdma_offset = i * OVL_RDMA_DEBUG_OFFSET + baddr;
		ovl_print_ovl_rdma_status(DISP_REG_GET(DISP_REG_OVL_RDMA0_DBG +
						       rdma_offset));
	}

	/* ext layer */
	for (i = 0; i < 3; i++) {
		if (ext_con & (0x1 << i))
			ovl_dump_layer_info(module, i, true);
		else
			DDPDUMP("ext_L%d:disabled\n", i);
	}
	ovl_printf_status(DISP_REG_GET(DISP_REG_OVL_FLOW_CTRL_DBG + baddr));
}

int ovl_dump(enum DISP_MODULE_ENUM module, int level)
{
	ovl_dump_analysis(module);
	ovl_dump_reg(module);

	return 0;
}

void ovl_cal_golden_setting(enum dst_module_type dst_mod_type,
	unsigned int *gs)
{
	/* OVL_RDMA_MEM_GMC_SETTING_1 */
	gs[GS_OVL_RDMA_ULTRA_TH] = 0x3ff;
	gs[GS_OVL_RDMA_PRE_ULTRA_TH] = (dst_mod_type == DST_MOD_REAL_TIME) ?
	    0x3ff : 0xe0;

	/* OVL_RDMA_FIFO_CTRL */
	gs[GS_OVL_RDMA_FIFO_THRD] = 0;
	gs[GS_OVL_RDMA_FIFO_SIZE] = 288;

	/* OVL_RDMA_MEM_GMC_SETTING_2 */
	gs[GS_OVL_RDMA_ISSUE_REQ_TH] = (dst_mod_type == DST_MOD_REAL_TIME) ?
	    191 : 15;
	gs[GS_OVL_RDMA_ISSUE_REQ_TH_URG] =
	    (dst_mod_type == DST_MOD_REAL_TIME) ? 95 : 15;
	gs[GS_OVL_RDMA_REQ_TH_PRE_ULTRA] = 0;
	gs[GS_OVL_RDMA_REQ_TH_ULTRA] = 1;
	gs[GS_OVL_RDMA_FORCE_REQ_TH] = 0;

	/* OVL_RDMA_GREQ_NUM */
	gs[GS_OVL_RDMA_GREQ_NUM] = (dst_mod_type == DST_MOD_REAL_TIME) ?
	    0xF1FF5555 : 0xF1FF0000;

	/* OVL_RDMA_GREQURG_NUM */
	gs[GS_OVL_RDMA_GREQ_URG_NUM] = (dst_mod_type == DST_MOD_REAL_TIME) ?
	    0x5555 : 0x0;

	/* OVL_RDMA_ULTRA_SRC */
	gs[GS_OVL_RDMA_ULTRA_SRC] = (dst_mod_type == DST_MOD_REAL_TIME) ?
	    0x8040 : 0xA040;

	/* OVL_RDMA_BUF_LOW_TH */
	gs[GS_OVL_RDMA_ULTRA_LOW_TH] = 0;
	gs[GS_OVL_RDMA_PRE_ULTRA_LOW_TH] =
	    (dst_mod_type == DST_MOD_REAL_TIME) ? 0 : 0x24;

	/* OVL_RDMA_BUF_HIGH_TH */
	gs[GS_OVL_RDMA_PRE_ULTRA_HIGH_TH] =
	    (dst_mod_type == DST_MOD_REAL_TIME) ? 0 : 0xd8;
	gs[GS_OVL_RDMA_PRE_ULTRA_HIGH_DIS] = 1;

	/* OVL_EN */
	gs[GS_OVL_BLOCK_EXT_ULTRA] =
	    (dst_mod_type == DST_MOD_REAL_TIME) ? 0 : 1;
	gs[GS_OVL_BLOCK_EXT_PRE_ULTRA] =
	    (dst_mod_type == DST_MOD_REAL_TIME) ? 0 : 1;
}

static int ovl_golden_setting(enum DISP_MODULE_ENUM module,
			      enum dst_module_type dst_mod_type, void *cmdq)
{
	unsigned long baddr = ovl_base_addr(module);
	unsigned int regval;
	unsigned int gs[GS_OVL_FLD_NUM];
	int i, layer_num;
	unsigned long Lx_base;

	layer_num = ovl_layer_num(module);

	/* calculate ovl golden setting */
	ovl_cal_golden_setting(dst_mod_type, gs);

	/* OVL_RDMA_MEM_GMC_SETTING_1 */
	regval = gs[GS_OVL_RDMA_ULTRA_TH] +
	    (gs[GS_OVL_RDMA_PRE_ULTRA_TH] << 16);
	for (i = 0; i < layer_num; i++) {
		Lx_base = i * OVL_LAYER_OFFSET + baddr;

		DISP_REG_SET(cmdq, Lx_base +
			     DISP_REG_OVL_RDMA0_MEM_GMC_SETTING, regval);
	}

	/* OVL_RDMA_FIFO_CTRL */
	regval = gs[GS_OVL_RDMA_FIFO_THRD] +
	    (gs[GS_OVL_RDMA_FIFO_SIZE] << 16);
	for (i = 0; i < layer_num; i++) {
		Lx_base = i * OVL_LAYER_OFFSET + baddr;

		DISP_REG_SET(cmdq, Lx_base + DISP_REG_OVL_RDMA0_FIFO_CTRL,
			     regval);
	}

	/* OVL_RDMA_MEM_GMC_SETTING_2 */
	regval = gs[GS_OVL_RDMA_ISSUE_REQ_TH] +
	    (gs[GS_OVL_RDMA_ISSUE_REQ_TH_URG] << 16) +
	    (gs[GS_OVL_RDMA_REQ_TH_PRE_ULTRA] << 28) +
	    (gs[GS_OVL_RDMA_REQ_TH_ULTRA] << 29) +
	    (gs[GS_OVL_RDMA_FORCE_REQ_TH] << 30);
	for (i = 0; i < layer_num; i++)
		DISP_REG_SET(cmdq, baddr + DISP_REG_OVL_RDMA0_MEM_GMC_S2 +
			i * 4, regval);

	/* DISP_REG_OVL_RDMA_GREQ_NUM */
	regval = gs[GS_OVL_RDMA_GREQ_NUM];
	DISP_REG_SET(cmdq, baddr + DISP_REG_OVL_RDMA_GREQ_NUM, regval);

	/* DISP_REG_OVL_RDMA_GREQ_URG_NUM */
	regval = gs[GS_OVL_RDMA_GREQ_URG_NUM];
	DISP_REG_SET(cmdq, baddr + DISP_REG_OVL_RDMA_GREQ_URG_NUM, regval);

	/* DISP_REG_OVL_RDMA_ULTRA_SRC */
	regval = gs[GS_OVL_RDMA_ULTRA_SRC];
	DISP_REG_SET(cmdq, baddr + DISP_REG_OVL_RDMA_ULTRA_SRC, regval);

	/* DISP_REG_OVL_RDMAn_BUF_LOW */
	regval = gs[GS_OVL_RDMA_ULTRA_LOW_TH] +
	    (gs[GS_OVL_RDMA_PRE_ULTRA_LOW_TH] << 12);

	for (i = 0; i < layer_num; i++)
		DISP_REG_SET(cmdq, baddr + DISP_REG_OVL_RDMAn_BUF_LOW(i),
			     regval);

	/* DISP_REG_OVL_RDMAn_BUF_HIGH */
	regval = (gs[GS_OVL_RDMA_PRE_ULTRA_HIGH_TH] << 12) +
	    (gs[GS_OVL_RDMA_PRE_ULTRA_HIGH_DIS] << 31);

	for (i = 0; i < layer_num; i++)
		DISP_REG_SET(cmdq, baddr + DISP_REG_OVL_RDMAn_BUF_HIGH(i),
			     regval);

	/* OVL_EN */
	regval = gs[GS_OVL_BLOCK_EXT_ULTRA] +
	    (gs[GS_OVL_BLOCK_EXT_PRE_ULTRA] << 1);
	DISP_REG_SET_FIELD(cmdq, REG_FLD_MSB_LSB(19, 18), baddr +
		DISP_REG_OVL_EN, regval);

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
	unsigned long baddr = ovl_base_addr(module);

	if ((bg_w > OVL_MAX_WIDTH) || (bg_h > OVL_MAX_HEIGHT)) {
		DDP_PR_ERR("ovl_roi,exceed OVL max size, w=%d, h=%d\n",
			   bg_w, bg_h);
		ASSERT(0);
	}
	DDPDBG("%s partial update\n", ddp_get_module_name(module));
	DISP_REG_SET(handle, baddr + DISP_REG_OVL_ROI_SIZE, bg_h << 16 | bg_w);

	return 0;
}

int ovl_addr_mva_replacement(enum DISP_MODULE_ENUM module,
			     struct ddp_fb_info *p_fb_info, void *handle)
{
	unsigned long base = ovl_base_addr(module);
	unsigned int src_on = DISP_REG_GET(DISP_REG_OVL_SRC_CON + base);
	unsigned int layer_addr, layer_mva;

	if (src_on & 0x1) {
		layer_addr = DISP_REG_GET(DISP_REG_OVL_L0_ADDR + base);
		layer_mva = layer_addr - p_fb_info->fb_pa + p_fb_info->fb_mva;
		DISP_REG_SET(handle, DISP_REG_OVL_L0_ADDR + base, layer_mva);
	}

	if (src_on & 0x2) {
		layer_addr = DISP_REG_GET(DISP_REG_OVL_L1_ADDR + base);
		layer_mva = layer_addr - p_fb_info->fb_pa + p_fb_info->fb_mva;
		DISP_REG_SET(handle, DISP_REG_OVL_L1_ADDR + base, layer_mva);
	}

	if (src_on & 0x4) {
		layer_addr = DISP_REG_GET(DISP_REG_OVL_L2_ADDR + base);
		layer_mva = layer_addr - p_fb_info->fb_pa + p_fb_info->fb_mva;
		DISP_REG_SET(handle, DISP_REG_OVL_L2_ADDR + base, layer_mva);
	}

	if (src_on & 0x8) {
		layer_addr = DISP_REG_GET(DISP_REG_OVL_L3_ADDR + base);
		layer_mva = layer_addr - p_fb_info->fb_pa + p_fb_info->fb_mva;
		DISP_REG_SET(handle, DISP_REG_OVL_L3_ADDR + base, layer_mva);
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

/* for mmpath */
unsigned int MMPathTracePrimaryOVL(char *str, unsigned int strlen,
	unsigned int n)
{
	unsigned int i = 0;
	struct OVL_CONFIG_STRUCT *ovl_cfg;

	for (i = 0; i < TOTAL_OVL_LAYER_NUM; i++) {
		ovl_cfg = &(g_primary_ovl_cfg[i]);
		if (!ovl_cfg->layer_en)
			continue;

		n += scnprintf(str + n, strlen - n,
			"in_%d=0x%lx, ", ovl_cfg->layer, ovl_cfg->addr);
		n += scnprintf(str + n, strlen - n,
			"in_%d_width=%d, ", ovl_cfg->layer, ovl_cfg->src_w);
		n += scnprintf(str + n, strlen - n,
			"in_%d_height=%d, ", ovl_cfg->layer, ovl_cfg->src_h);
		n += scnprintf(str + n, strlen - n,
			"in_%d_fmt=%s, ", ovl_cfg->layer,
			unified_color_fmt_name(ovl_cfg->fmt));
		n += scnprintf(str + n, strlen - n,
			"in_%d_bpp=%u, ", ovl_cfg->layer,
			ufmt_get_Bpp(ovl_cfg->fmt));
		n += scnprintf(str + n, strlen - n,
			"in_%d_compr=%u, ", ovl_cfg->layer,
			ovl_cfg->compress);
	}

	return n;
}

unsigned int MMPathTraceSecondOVL(char *str, unsigned int strlen,
	unsigned int n)
{
	unsigned int i = 0;
	struct OVL_CONFIG_STRUCT *ovl_cfg;

	for (i = 0; i < TOTAL_OVL_LAYER_NUM; i++) {
		ovl_cfg = &(g_second_ovl_cfg[i]);
		if (!ovl_cfg->layer_en)
			continue;

		n += scnprintf(str + n, strlen - n,
			"in_%d=0x%lx, ", ovl_cfg->layer, ovl_cfg->addr);
		n += scnprintf(str + n, strlen - n,
			"in_%d_width=%d, ", ovl_cfg->layer, ovl_cfg->src_w);
		n += scnprintf(str + n, strlen - n,
			"in_%d_height=%d, ", ovl_cfg->layer, ovl_cfg->src_h);
		n += scnprintf(str + n, strlen - n,
			"in_%d_fmt=%s, ", ovl_cfg->layer,
			unified_color_fmt_name(ovl_cfg->fmt));
		n += scnprintf(str + n, strlen - n,
			"in_%d_bpp=%u, ", ovl_cfg->layer,
			ufmt_get_Bpp(ovl_cfg->fmt));
		n += scnprintf(str + n, strlen - n,
			"in_%d_compr=%u, ", ovl_cfg->layer,
			ovl_cfg->compress);
	}

	return n;
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
