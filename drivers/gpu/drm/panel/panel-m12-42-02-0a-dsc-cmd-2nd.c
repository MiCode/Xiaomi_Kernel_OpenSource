// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <drm/drm_modes.h>
#include <linux/delay.h>
#include <drm/drm_connector.h>
#include <drm/drm_device.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>

#include "mi_dsi_panel_count.h"

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#include "../mediatek/mediatek_v2/mtk_log.h"
#endif

#include <linux/atomic.h>

#include "dsi_iris_api.h"
#include "dsi_iris_mtk_api.h"

#define DATA_RATE0		550
#define DATA_RATE1		1152
#define DATA_RATE2		720
#define DATA_RATE_RAW   1497 //1440

#define VACT_RAW		1660
#define VACT_DSC		2712

#define MODE3_FPS		144
#define MODE3_HFP		4
#define MODE3_HSA		4
#define MODE3_HBP		4
#define MODE3_VFP		424
#define MODE3_VSA		2
#define MODE3_VBP		2

#define MODE2_FPS		90
#define MODE2_HFP		160
#define MODE2_HSA		2
#define MODE2_HBP		4
#define MODE2_VFP		54
#define MODE2_VSA		10
#define MODE2_VBP		10

#define MODE1_FPS		120
#define MODE1_HFP		160
#define MODE1_HSA		2
#define MODE1_HBP		4
#define MODE1_VFP		54
#define MODE1_VSA		10
#define MODE1_VBP		10

#define MODE0_FPS		60
#define MODE0_HFP		408
#define MODE0_HSA		2
#define MODE0_HBP		4
#define MODE0_VFP		54
#define MODE0_VSA		10
#define MODE0_VBP		10

#define FRAME_WIDTH		(1220)
#define	FRAME_HEIGHT		(2712)


#define MAX_BRIGHTNESS_CLONE	16383
#define FPS_INIT_INDEX		11
#define RTE_OFF			0xFFFF
#define RTE_CMD			0xEEEE

#define DSC_ENABLE                  1
#define DSC_VER                     17
#define DSC_SLICE_MODE              1
#define DSC_RGB_SWAP                0
#define DSC_DSC_CFG                 40
#define DSC_RCT_ON                  1
#define DSC_BIT_PER_CHANNEL         10
#define DSC_DSC_LINE_BUF_DEPTH      11
#define DSC_BP_ENABLE               1
#define DSC_BIT_PER_PIXEL           128
#define DSC_SLICE_HEIGHT            12
#define DSC_SLICE_WIDTH             610
#define DSC_CHUNK_SIZE              610
#define DSC_XMIT_DELAY              512
#define DSC_DEC_DELAY               562
#define DSC_SCALE_VALUE             32
#define DSC_INCREMENT_INTERVAL      305
#define DSC_DECREMENT_INTERVAL      8
#define DSC_LINE_BPG_OFFSET         12
#define DSC_NFL_BPG_OFFSET          2235
#define DSC_SLICE_BPG_OFFSET        1915
#define DSC_INITIAL_OFFSET          6144
#define DSC_FINAL_OFFSET            4336
#define DSC_FLATNESS_MINQP          7
#define DSC_FLATNESS_MAXQP          16
#define DSC_RC_MODEL_SIZE           8192
#define DSC_RC_EDGE_FACTOR          6
#define DSC_RC_QUANT_INCR_LIMIT0    15
#define DSC_RC_QUANT_INCR_LIMIT1    15
#define DSC_RC_TGT_OFFSET_HI        3
#define DSC_RC_TGT_OFFSET_LO        3
//static int current_fps = 120;

#define SPR_2D_RENDERING_DUMMY
#ifdef SPR_2D_RENDERING_DUMMY
#define SPR_2D_RENDERING (1)
#endif

unsigned int m12_42_dphy_range_min_qp[15] = {
	0x0, 0x0, 0x5, 0x5, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x9, 0x9, 0x9, 0xb, 0x11};
unsigned int m12_42_dphy_range_max_qp[15] = {
	0x0, 0x2, 0x9, 0xa, 0xb, 0xb, 0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11, 0x11, 0x13};
unsigned int m12_42_dphy_range_bpg_ofs[15] = {
	0x24, 0x4, 0x0, 0x3e, 0x3c, 0x3a, 0x38, 0x38, 0x38, 0x36, 0x36, 0x34, 0x34, 0x34, 0x34};

//static int gDcThreshold = 450;
//static bool gDcEnable;
//static bool doze_had;
//static bool dimming_on;
static struct lcm *panel_ctx;

struct drm_display_mode *get_mode_by_id(struct drm_connector *connector,
	unsigned int mode);
static int mtk_panel_ext_param_get(struct drm_panel *panel,
	struct drm_connector *connector,
	struct mtk_panel_params **ext_param,
	unsigned int mode);


static inline struct lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct lcm, panel);
}


#define REGFLAG_DELAY       0xFFFC
#define REGFLAG_UDELAY  0xFFFB
#define REGFLAG_END_OF_TABLE    0xFFFD
#define REGFLAG_RESET_LOW   0xFFFE
#define REGFLAG_RESET_HIGH  0xFFFF

static void lcm_panel_init(struct lcm *ctx)
{
	pr_info("%s- 2nd\n", __func__);
}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("%s: 2nd \n", __func__);

	if (!ctx->enabled)
		return 0;

	iris_disable_secondary(panel);
	ctx->enabled = false;

	return 0;
}

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("%s: 2nd \n", __func__);

	if (!ctx->prepared)
		return 0;

	ctx->error = 0;
	ctx->prepared = false;

	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s 2nd, ctx->prepared: %d \n", __func__, ctx->prepared);
	if (ctx->prepared)
		return 0;

	lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;
	pr_info("%s: 2nd -\n", __func__);
	return ret;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("%s 2nd, ctx->enabled: %d \n", __func__, ctx->enabled);
	if (ctx->enabled)
		return 0;

	if (ctx->mode_index != -1 && ctx->connector != NULL) {
		// maybe have better solution
		struct mtk_panel_params *panel_params_dst = NULL;
		struct drm_display_mode *mode = get_mode_by_id(ctx->connector, ctx->mode_index);
		mtk_panel_ext_param_get(panel, ctx->connector, &panel_params_dst, ctx->mode_index);

		iris_update_2nd_active_timing(mode->hdisplay, mode->vdisplay,
					drm_mode_vrefresh(mode), panel_params_dst->dsc_params.enable);
	} else {
		iris_enable_secondary(panel);
	}
	ctx->enabled = true;

	return 0;
}

static const struct drm_display_mode default_mode = {
	.clock = 273139,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE0_HFP,
	.hsync_end = FRAME_WIDTH + MODE0_HFP + MODE0_HSA,
	.htotal = FRAME_WIDTH + MODE0_HFP + MODE0_HSA + MODE0_HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE0_VFP,
	.vsync_end = FRAME_HEIGHT + MODE0_VFP + MODE0_VSA,
	.vtotal = FRAME_HEIGHT + MODE0_VFP + MODE0_VSA + MODE0_VBP,
};

static const struct drm_display_mode middle_mode = {
	.clock = 347526,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE2_HFP,
	.hsync_end = FRAME_WIDTH + MODE2_HFP + MODE2_HSA,
	.htotal = FRAME_WIDTH + MODE2_HFP + MODE2_HSA + MODE2_HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE2_VFP,
	.vsync_end = FRAME_HEIGHT + MODE2_VFP + MODE2_VSA,
	.vtotal = FRAME_HEIGHT + MODE2_VFP + MODE2_VSA + MODE2_VBP,
};

static const struct drm_display_mode performence_mode = {
	.clock = 463368,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE1_HFP,
	.hsync_end = FRAME_WIDTH + MODE1_HFP + MODE1_HSA,
	.htotal = FRAME_WIDTH + MODE1_HFP + MODE1_HSA + MODE1_HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE1_VFP,
	.vsync_end = FRAME_HEIGHT + MODE1_VFP + MODE1_VSA,
	.vtotal = FRAME_HEIGHT + MODE1_VFP + MODE1_VSA + MODE1_VBP,
};

static const struct drm_display_mode performence_mode_144 = {
	.clock = 556041,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE3_HFP,
	.hsync_end = FRAME_WIDTH + MODE3_HFP + MODE3_HSA,
	.htotal = FRAME_WIDTH + MODE3_HFP + MODE3_HSA + MODE3_HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE3_VFP,
	.vsync_end = FRAME_HEIGHT + MODE3_VFP + MODE3_VSA,
	.vtotal = FRAME_HEIGHT + MODE3_VFP + MODE3_VSA + MODE3_VBP,
};

static const struct drm_display_mode performance_mode_raw_120hz = {
	.clock = 240065,
	.hdisplay = 1080,
	.hsync_start = 1080 + 4,//HFP
	.hsync_end = 1080 + 4 + 4,//HSA
	.htotal = 1080 + 4 + 4 + 4,//HBP
	.vdisplay = 1660,
	.vsync_start = 1660 + 156,//VFP
	.vsync_end = 1660 + 156 + 8,//VSA
	.vtotal = 1660 + + 156 + 8 + 8,//VBP
};

static struct mtk_panel_params ext_params = {
	.pll_clk = DATA_RATE1 / 2,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x1c,
	},
	//.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 69540,
	.physical_height_um = 154584,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable                =  DSC_ENABLE,
		.ver                   =  DSC_VER,
		.slice_mode            =  DSC_SLICE_MODE,
		.rgb_swap              =  DSC_RGB_SWAP,
		.dsc_cfg               =  DSC_DSC_CFG,
		.rct_on                =  DSC_RCT_ON,
		.bit_per_channel       =  DSC_BIT_PER_CHANNEL,
		.dsc_line_buf_depth    =  DSC_DSC_LINE_BUF_DEPTH,
		.bp_enable             =  DSC_BP_ENABLE,
		.bit_per_pixel         =  DSC_BIT_PER_PIXEL,
		.pic_height            =  FRAME_HEIGHT,
		.pic_width             =  FRAME_WIDTH,
		.slice_height          =  DSC_SLICE_HEIGHT,
		.slice_width           =  DSC_SLICE_WIDTH,
		.chunk_size            =  DSC_CHUNK_SIZE,
		.xmit_delay            =  DSC_XMIT_DELAY,
		.dec_delay             =  DSC_DEC_DELAY,
		.scale_value           =  DSC_SCALE_VALUE,
		.increment_interval    =  DSC_INCREMENT_INTERVAL,
		.decrement_interval    =  DSC_DECREMENT_INTERVAL,
		.line_bpg_offset       =  DSC_LINE_BPG_OFFSET,
		.nfl_bpg_offset        =  DSC_NFL_BPG_OFFSET,
		.slice_bpg_offset      =  DSC_SLICE_BPG_OFFSET,
		.initial_offset        =  DSC_INITIAL_OFFSET,
		.final_offset          =  DSC_FINAL_OFFSET,
		.flatness_minqp        =  DSC_FLATNESS_MINQP,
		.flatness_maxqp        =  DSC_FLATNESS_MAXQP,
		.rc_model_size         =  DSC_RC_MODEL_SIZE,
		.rc_edge_factor        =  DSC_RC_EDGE_FACTOR,
		.rc_quant_incr_limit0  =  DSC_RC_QUANT_INCR_LIMIT0,
		.rc_quant_incr_limit1  =  DSC_RC_QUANT_INCR_LIMIT1,
		.rc_tgt_offset_hi      =  DSC_RC_TGT_OFFSET_HI,
		.rc_tgt_offset_lo      =  DSC_RC_TGT_OFFSET_LO,
		.ext_pps_cfg = {
			.enable = 1,
			.range_min_qp = m12_42_dphy_range_min_qp,
			.range_max_qp = m12_42_dphy_range_max_qp,
			.range_bpg_ofs = m12_42_dphy_range_bpg_ofs,
			},
		},
	.data_rate = DATA_RATE1,
	//.lp_perline_en = 1,
};

#if 0
static struct mtk_panel_params ext_params_90hz = {
	.pll_clk = DATA_RATE1 / 2,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x1c,
	},
	.is_support_od = true,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 69540,
	.physical_height_um = 154584,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 40,
		.rct_on = 1,
		.bit_per_channel = 10,
		.dsc_line_buf_depth = 11,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2712,
		.pic_width = 1220,
		.slice_height = 12,
		.slice_width = 610,
		.chunk_size = 610,
		.xmit_delay = 512,
		.dec_delay = 562,
		.scale_value = 32,
		.increment_interval = 305,
		.decrement_interval = 8,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 2235,
		.slice_bpg_offset = 1915,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 7,
		.flatness_maxqp = 16,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 15,
		.rc_quant_incr_limit1 = 15,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		},
	.data_rate = DATA_RATE1,
	/*Msync 2.0*/
	.msync2_enable = 1,
	.msync_cmd_table = {
		.te_type = REQUEST_TE,
		.msync_max_fps = 120,
		.msync_min_fps = 60,
		.msync_level_num = 20,
		.delay_frame_num = 2,
		.request_te_tb = {

			/* Request-TE level */

			.rte_te_level[0] = {
					.level_id = 1,
					.level_fps = 120,
					.max_fps = 120,
					.min_fps = 60,
			},
/*
			.rte_te_level[0] = {
					.level_id = 1,
					.level_fps = 90,
					.max_fps = 90,
					.min_fps = 60,
			},
			.rte_te_level[1] = {
					.level_id = 2,
					.level_fps = 120,
					.max_fps = 120,
					.min_fps = 90,
			},
*/
		},
	},
	//.lp_perline_en = 1,
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size_l = sizeof(top_rc_pattern_l),
	.corner_pattern_lt_addr_l = (void *)top_rc_pattern_l,
	.corner_pattern_tp_size_r = sizeof(top_rc_pattern_r),
	.corner_pattern_lt_addr_r = (void *)top_rc_pattern_r,

#endif
};

static struct mtk_panel_params ext_params_120hz = {
	.pll_clk = DATA_RATE1 / 2,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x1c,
	},
	.is_support_od = true,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 69540,
	.physical_height_um = 154584,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 40,
		.rct_on = 1,
		.bit_per_channel = 10,
		.dsc_line_buf_depth = 11,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2712,
		.pic_width = 1220,
		.slice_height = 12,
		.slice_width = 610,
		.chunk_size = 610,
		.xmit_delay = 512,
		.dec_delay = 562,
		.scale_value = 32,
		.increment_interval = 305,
		.decrement_interval = 8,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 2235,
		.slice_bpg_offset = 1915,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 7,
		.flatness_maxqp = 16,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 15,
		.rc_quant_incr_limit1 = 15,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		},
	.data_rate = DATA_RATE1,
	/*Msync 2.0*/
	.msync2_enable = 1,
	.msync_cmd_table = {
		.te_type = REQUEST_TE,
		.msync_max_fps = 120,
		.msync_min_fps = 60,
		.msync_level_num = 20,
		.delay_frame_num = 2,
		.request_te_tb = {

			/* Request-TE level */

			.rte_te_level[0] = {
				.level_id = 1,
				.level_fps = 120,
				.max_fps = 120,
				.min_fps = 60,
			},
/*
			.rte_te_level[0] = {
				.level_id = 1,
				.level_fps = 90,
				.max_fps = 90,
				.min_fps = 60,
			},
			.rte_te_level[1] = {
				.level_id = 2,
				.level_fps = 120,
				.max_fps = 120,
				.min_fps = 90,
			},
*/
		},
	},
	//.lp_perline_en = 1,
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size_l = sizeof(top_rc_pattern_l),
	.corner_pattern_lt_addr_l = (void *)top_rc_pattern_l,
	.corner_pattern_tp_size_r = sizeof(top_rc_pattern_r),
	.corner_pattern_lt_addr_r = (void *)top_rc_pattern_r,

#endif
};
#endif 

static struct mtk_panel_params ext_params_raw = {
	.pll_clk = DATA_RATE_RAW/2,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9f,
	},
	//.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 69540,
	.physical_height_um = 154584,
	.output_mode = MTK_PANEL_SINGLE_PORT,
	.data_rate = DATA_RATE_RAW,
};


struct drm_display_mode *get_mode_by_id(struct drm_connector *connector,
	unsigned int mode)
{
	struct drm_display_mode *m = NULL;
	unsigned int i = 0;

	list_for_each_entry(m, &connector->modes, head) {
		if (i == mode)
			return m;
		i++;
	}
	return NULL;
}

static int mtk_panel_ext_param_set(struct drm_panel *panel,
			struct drm_connector *connector, unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(connector, mode);

	if (m->vdisplay == VACT_RAW)
		ext->params = &ext_params_raw;
	else if (m->vdisplay == VACT_DSC)
		ext->params = &ext_params;
	else {
		pr_err("%s cannot get mode: %d, vrefresh = %d, hdisplay = %d, vdisplay = %d",
			__func__, mode, drm_mode_vrefresh(m), m->hdisplay, m->vdisplay);
		return ret;
	}

	pr_info("%s mode: %d, vrefresh = %d, hdisplay = %d, vdisplay = %d, dsc: %d\n",
		__func__, mode, drm_mode_vrefresh(m), m->hdisplay, m->vdisplay, ext->params->dsc_params.enable);
	return ret;
}

static int mtk_panel_ext_param_get(struct drm_panel *panel,
	struct drm_connector *connector,
	struct mtk_panel_params **ext_param,
	unsigned int mode)
{
	int ret = 0;

	struct drm_display_mode *m = get_mode_by_id(connector, mode);

	if (m->vdisplay == VACT_RAW)
		*ext_param = &ext_params_raw;
	else if (m->vdisplay == VACT_DSC)
		*ext_param = &ext_params;
	else {
		pr_err("%s cannot get mode: %d, vrefresh = %d, hdisplay = %d, vdisplay = %d",
			__func__, mode, drm_mode_vrefresh(m), m->hdisplay, m->vdisplay);
		return ret;
	}

	//pr_info("%s mode: %d, vrefresh = %d, hdisplay = %d, vdisplay = %d, dsc: %d\n",
	//	__func__, mode, drm_mode_vrefresh(m), m->hdisplay, m->vdisplay, (*ext_param)->dsc_params.enable);

	//if (*ext_param)
	//	pr_info("data_rate:%d\n", (*ext_param)->data_rate);
	//else
	//	pr_info("ext_param is NULL;\n");

	return ret;
}


static int panel_ext_reset(struct drm_panel *panel, int on)
{
	return 0;
}

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	pr_info("%s 2nd %d %d %d\n", __func__, level);
	return 0;
}

static void mode_switch_to_144(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	pr_info("%s state = %d\n", __func__, stage);
}

static void mode_switch_to_120(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	pr_info("%s state = %d\n", __func__, stage);
}

static void mode_switch_to_90(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	pr_info("%s state = %d\n", __func__, stage);
}

static void mode_switch_to_60(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	pr_info("%s state = %d\n", __func__, stage);
}

static int mode_switch(struct drm_panel *panel,
		struct drm_connector *connector, unsigned int cur_mode,
		unsigned int dst_mode, enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	int ret = 0;
	bool isFpsChange = false;
	bool isResChange = false;
	struct drm_display_mode *m_dst = get_mode_by_id(connector, dst_mode);
	struct drm_display_mode *m_cur = get_mode_by_id(connector, cur_mode);
	struct mtk_panel_params *panel_params_cur = NULL;
	struct mtk_panel_params *panel_params_dst = NULL;
	struct lcm *ctx = panel_to_lcm(panel);

	if (cur_mode == dst_mode)
		return ret;

	isFpsChange = drm_mode_vrefresh(m_dst) == drm_mode_vrefresh(m_cur) ? false : true;
	isResChange = m_dst->vdisplay == m_cur->vdisplay ? false : true;
	mtk_panel_ext_param_get(panel, connector, &panel_params_cur, cur_mode);
	mtk_panel_ext_param_get(panel, connector, &panel_params_dst, dst_mode);

	pr_info("%s isFpsChange = %d, isResChange = %d\n", __func__, isFpsChange, isResChange);
	pr_info("%s dst_mode vrefresh = %d, vdisplay = %d, vdisplay = %d, dsc: %d\n",
		__func__, drm_mode_vrefresh(m_dst), m_dst->vdisplay, m_dst->hdisplay,
		panel_params_dst->dsc_params.enable);
	pr_info("%s cur_mode vrefresh = %d, vdisplay = %d, vdisplay = %d, dsc: %d\n",
		__func__, drm_mode_vrefresh(m_cur), m_cur->vdisplay, m_cur->hdisplay,
		panel_params_cur->dsc_params.enable);

	if (isFpsChange) {
		if (drm_mode_vrefresh(m_dst) == MODE0_FPS)
			mode_switch_to_60(panel, stage);
		else if (drm_mode_vrefresh(m_dst) == MODE1_FPS)
			mode_switch_to_120(panel, stage);
		else if (drm_mode_vrefresh(m_dst) == MODE2_FPS)
			mode_switch_to_90(panel, stage);
		else if (drm_mode_vrefresh(m_dst) == MODE3_FPS)
			mode_switch_to_144(panel, stage);
		else
			ret = 1;
	}

	iris_update_2nd_active_timing(m_dst->hdisplay, m_dst->vdisplay,
								drm_mode_vrefresh(m_dst),
								panel_params_dst->dsc_params.enable);
	ctx->mode_index = dst_mode;
	ctx->connector = connector;

	return ret;
}

static int panel_ata_check(struct drm_panel *panel)
{
	return 1;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.ext_param_set = mtk_panel_ext_param_set,
	.ext_param_get = mtk_panel_ext_param_get,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	//.set_bl_elvss_cmdq = lcm_set_bl_elvss_cmdq,
	.mode_switch = mode_switch,
	//.doze_enable = panel_doze_enable,
	//.doze_disable = panel_doze_disable,
	//.msync_te_level_switch_grp = msync_te_level_switch_grp,
	.ata_check = panel_ata_check,
};


struct panel_desc {
	const struct drm_display_mode *modes;
	unsigned int num_modes;

	unsigned int bpc;

	struct {
		unsigned int width;
		unsigned int height;
	} size;

	struct {
		unsigned int prepare;
		unsigned int enable;
		unsigned int disable;
		unsigned int unprepare;
	} delay;
};

static int lcm_get_modes(struct drm_panel *panel, struct drm_connector *connector)
{
	struct drm_display_mode *mode0, *mode1, *mode2, *mode3, *mode4;
	

	mode0 = drm_mode_duplicate(connector->dev, &default_mode); // 60Hz
	if (!mode0) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}
	drm_mode_set_name(mode0);
	mode0->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode0);

	
	mode2 = drm_mode_duplicate(connector->dev, &middle_mode); //90Hz
	if (!mode2) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			middle_mode.hdisplay, middle_mode.vdisplay,
			drm_mode_vrefresh(&middle_mode));
		return -ENOMEM;
	}
	drm_mode_set_name(mode2);
	mode2->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode2);
	
	mode1 = drm_mode_duplicate(connector->dev, &performence_mode);	//120Hz
	if (!mode1) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			performence_mode.hdisplay, performence_mode.vdisplay,
			drm_mode_vrefresh(&performence_mode));
		return -ENOMEM;
	}
	drm_mode_set_name(mode1);
	mode1->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode1);

	mode3 = drm_mode_duplicate(connector->dev, &performence_mode_144);	//144Hz
	if (!mode1) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			performence_mode.hdisplay, performence_mode_144.vdisplay,
			drm_mode_vrefresh(&performence_mode_144));
		return -ENOMEM;
	}
	drm_mode_set_name(mode3);
	mode3->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode3);

	mode4 = drm_mode_duplicate(connector->dev, &performance_mode_raw_120hz);
	if (!mode4) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 performance_mode_raw_120hz.hdisplay, performance_mode_raw_120hz.vdisplay,
			 drm_mode_vrefresh(&performance_mode_raw_120hz));
		return -ENOMEM;
	}

	drm_mode_set_name(mode4);
	mode4->type = DRM_MODE_TYPE_DRIVER /*| DRM_MODE_TYPE_PREFERRED*/;
	drm_mode_probed_add(connector, mode4);

	connector->display_info.width_mm = 71;
	connector->display_info.height_mm = 153;

	pr_info("%s- m12-42-02-0a-dsc 2nd \n", __func__);

	return 1;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};

static int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct device_node *dsi_node, *remote_node = NULL;
	struct lcm *ctx;
	int ret;
	char secondary_panel[30];

	pr_info("%s m12-42-02-0a-dsc 2nd+\n", __func__);

	if (!is_mi_dev_support_iris()) {
		pr_err("m12-42-02-0a-dsc 2nd : IRIS:mi dev not support iris\n");
		return -ENODEV;
	}

	dsi_node = of_get_parent(dev->of_node);
	if (dsi_node) {
		memset(secondary_panel, '\0', sizeof(secondary_panel));
		iris_find_secondary_panel_name(secondary_panel);
		remote_node = of_find_compatible_node(NULL, NULL, secondary_panel);
		if (!remote_node) {
			pr_err("IRIS:No secondary panel connected\n");
			return -ENODEV;
		}
		if (remote_node != dev->of_node) {
			pr_info("%s+ skip probe due to not current lcm\n", __func__);
			return -ENODEV;
		}
	}

	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	panel_ctx = ctx;
	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_NO_EOT_PACKET
			 | MIPI_DSI_CLOCK_NON_CONTINUOUS;

	ctx->prepared = true;
	
	if (of_property_read_bool(dsi_node, "init-panel-off")) {
		ctx->prepared = false;
		ctx->enabled = false;
		pr_info("2nd display dsi_node:%s set prepared = enabled = false\n",dsi_node->full_name);
	}

	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	//mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	//ret = mtk_panel_ext_create(dev, &ext_params_raw, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif

	pr_info("%s m12-42-02-0a-dsc 2nd DSC 120Hz\n", __func__);
	ctx->mode_index = -1;
	ctx->connector = NULL;
	return ret;
}

static int lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);
#if defined(CONFIG_MTK_PANEL_EXT)
	struct mtk_panel_ctx *ext_ctx;
#endif

	if (!ctx)
		return 0;

#if defined(CONFIG_MTK_PANEL_EXT)
	ext_ctx = find_panel_ctx(&ctx->panel);
#endif

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	if(ext_ctx) {
		mtk_panel_detach(ext_ctx);
		mtk_panel_remove(ext_ctx);
	}
#endif

	return 0;
}

static const struct of_device_id lcm_of_match[] = {
	{ .compatible = "m12_42_02_0a_dsc_cmd,lcm,2nd", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "m12_42_02_0a_dsc_cmd,lcm,2nd",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("yaohui.zeng <yhzeng@pixelworks.com>");
MODULE_DESCRIPTION("m12_42_02_0a_dsc_cmd_2nd oled panel driver");
MODULE_LICENSE("GPL v2");
