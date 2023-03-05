// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/backlight.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <drm/drm_modes.h>
#include <linux/delay.h>
#include <drm/drm_connector.h>
#include <drm/drm_device.h>
#include <linux/of_graph.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include "panel-boe-nt37705-dphy-cmd-fhd.h"

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif

#define BRIGHTNESS_MAX    4095
#define BRIGHTNESS_HALF   2047
#define MAX_NORMAL_BRIGHTNESS   3515
#define SILKY_MAX_NORMAL_BRIGHTNESS   8191
#define LCM_BRIGHTNESS_TYPE 2

#define FHD_LCM_WIDTH  1080
#define FHD_LCM_HEIGHT 2520

#define lcm_dcs_write_seq(ctx, seq...)			\
	({											\
		const u8 d[] = { seq };					\
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 128,	\
			"DCS sequence too big for stack");	\
		lcm_dcs_write(ctx, d, ARRAY_SIZE(d));	\
	})

#define lcm_dcs_write_seq_static(ctx, seq...)	\
	({											\
		static const u8 d[] = { seq };			\
		lcm_dcs_write(ctx, d, ARRAY_SIZE(d));	\
	})

struct lcm_pmic_info {
	struct regulator *reg_vddi_1v8;
	struct regulator *reg_vci_3v0;
};

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bias_pos, *bias_neg;
	struct gpio_desc *bias_gpio;
	struct gpio_desc *vddr1p5_enable_gpio;
	struct gpio_desc *aod1p3_enable_gpio;
	struct drm_display_mode *m;
	bool prepared;
	bool enabled;

	int error;
};

static inline struct lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct lcm, panel);
}

static void lcm_dcs_write_ext(struct lcm *ctx, const void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return;


	ret = mipi_dsi_dcs_write_buffer(dsi, data, len);

	if (ret < 0) {
		pr_info("error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
}

static void push_table(struct lcm *ctx, struct LCM_setting_table *table,
		unsigned int count)
{
	unsigned int i;
	unsigned int cmd;

	for (i = 0; i < count; i++) {
		cmd = table[i].cmd;
		switch (cmd) {
		case REGFLAG_DELAY:
			msleep(table[i].count);
			break;
		case REGFLAG_UDELAY:
			usleep_range(table[i].count, table[i].count + 1000);
			break;
		case REGFLAG_END_OF_TABLE:
			break;
		default:
			lcm_dcs_write_ext(ctx, table[i].para_list,
				table[i].count);
			break;
		}
	}
}

#ifdef PANEL_SUPPORT_READBACK
static int lcm_dcs_read(struct lcm *ctx, u8 cmd, void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return 0;

	ret = mipi_dsi_dcs_read(dsi, cmd, data, len);
	if (ret < 0) {
		pr_info("error %d reading dcs seq:(%#x)\n", ret,
			 cmd);
		ctx->error = ret;
	}

	return ret;
}

static void lcm_panel_get_data(struct lcm *ctx)
{
	u8 buffer[3] = { 0 };
	static int ret;

	pr_info("%s+\n");

	if (ret == 0) {
		ret = lcm_dcs_read(ctx, 0x0A, buffer, 1);
		pr_debug("return %d data(0x%08x) to dsi engine\n",
				ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

static void lcm_dcs_write(struct lcm *ctx, const void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;
	char *addr;

	if (ctx->error < 0)
		return;

	addr = (char *)data;
	if ((int)*addr < 0xB0)
		ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
	else
		ret = mipi_dsi_generic_write(dsi, data, len);
	if (ret < 0) {
		pr_info("error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
}

static struct lcm_pmic_info *g_pmic;
static unsigned int lcm_get_reg_vddi_1v8(void)
{
	unsigned int volt = 0;

	if (regulator_is_enabled(g_pmic->reg_vddi_1v8))
		/* regulator_get_voltage return volt with uV */
		volt = regulator_get_voltage(g_pmic->reg_vddi_1v8);

	return volt;
}

static unsigned int lcm_get_reg_vci_3v0(void)
{
	unsigned int volt = 0;

	if (regulator_is_enabled(g_pmic->reg_vci_3v0))
		/* regulator_get_voltage return volt with uV */
		volt = regulator_get_voltage(g_pmic->reg_vci_3v0);

	return volt;
}
static unsigned int lcm_enable_reg_vddi_1v8(int en)
{
	unsigned int ret = 0, volt = 0;

	if (en) {
		if (!IS_ERR_OR_NULL(g_pmic->reg_vddi_1v8)) {
			ret = regulator_enable(g_pmic->reg_vddi_1v8);
			pr_info("Enable the reg_vddi_1v8 ret=%d.\n", ret);
			volt = lcm_get_reg_vddi_1v8();
			pr_debug("get the reg_vddi_1v8 = %d.\n", volt);
		}
	} else {
		if (!IS_ERR_OR_NULL(g_pmic->reg_vddi_1v8)) {
			ret = regulator_disable(g_pmic->reg_vddi_1v8);
			volt = lcm_get_reg_vddi_1v8();
			pr_info("disable the reg_vddi_1v8 ret=%d,volt=%d.\n", ret, volt);
		}
	}
	return ret;

}

static unsigned int lcm_enable_reg_vci_3v0(int en)
{
	unsigned int ret = 0, volt = 0;

	if (en) {
		if (!IS_ERR_OR_NULL(g_pmic->reg_vci_3v0)) {
			ret = regulator_enable(g_pmic->reg_vci_3v0);
			pr_info("Enable the reg_vci_3v0 ret=%d.\n", ret);
			volt = lcm_get_reg_vci_3v0();
			pr_debug("get the Regulator vmc3p0 =%d.\n", volt);
		}
	} else if (!IS_ERR_OR_NULL(g_pmic->reg_vci_3v0)) {
		ret = regulator_disable(g_pmic->reg_vci_3v0);
		volt = lcm_get_reg_vci_3v0();
		pr_info("disable the reg_vci_3v0 ret=%d,volt=%d.\n", ret, volt);
	}

	return ret;

}

static struct drm_display_mode *get_mode_by_id(struct drm_connector *connector,
	unsigned int mode)
{
	struct drm_display_mode *m;
	unsigned int i = 0;

	list_for_each_entry(m, &connector->modes, head) {
		if (i == mode)
			return m;
		i++;
	}

	return NULL;
}

static int get_mode_enum(struct drm_display_mode *m)
{
	int ret = 0;
	int m_vrefresh = 0;

	if (m == NULL)
		return -EINVAL;

	m_vrefresh = drm_mode_vrefresh(m);

	if (m_vrefresh == 60)
		ret = FHD_SDC60;
	else if (m_vrefresh == 120)
		ret = FHD_SDC120;
	else
		ret = FHD_SDC60;

	return ret;
}


static void lcm_panel_init(struct lcm *ctx)
{
	int mode_id = -1;
	struct drm_display_mode *m = ctx->m;

	mode_id = get_mode_enum(m);
	switch (mode_id) {
	case FHD_SDC60:
		push_table(ctx, fhd_dsi_on_cmd_sdc60, sizeof(fhd_dsi_on_cmd_sdc60)/
				sizeof(struct LCM_setting_table));
		pr_info("fhd_dsi_on_cmd_sdc60\n");
		break;
	case FHD_SDC120:
		push_table(ctx, fhd_dsi_on_cmd_sdc120, sizeof(fhd_dsi_on_cmd_sdc120)/
				sizeof(struct LCM_setting_table));
		pr_info("fhd_dsi_on_cmd_sdc120\n");
		break;
	default:
		push_table(ctx, fhd_dsi_on_cmd_sdc60, sizeof(fhd_dsi_on_cmd_sdc60)/
				sizeof(struct LCM_setting_table));
		pr_info("default mode_id\n");
		break;
	}

	pr_debug("debug for successful! mode_id=%d\n", mode_id);
}

// static void lcm_panel_seed(struct lcm *ctx){
	// if (seed_mode == 100) {
		// push_table(ctx, lcm_seed_mode0, sizeof(lcm_seed_mode0)/
				// sizeof(struct LCM_setting_table));
	// } else if (seed_mode == 101){
		// push_table(ctx, lcm_seed_mode1, sizeof(lcm_seed_mode1)/
				// sizeof(struct LCM_setting_table));
	// } else if (seed_mode == 102){
		// push_table(ctx, lcm_seed_mode2, sizeof(lcm_seed_mode2)/
				// sizeof(struct LCM_setting_table));
	// }
// pr_info("lcm_panel_seed\n");
// }

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return 0;
}

static int lcm_panel_power_disable(struct lcm *ctx)
{
	int ret = 0;

	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(2000, 2100);

	//set vci 3.0v
	ret = lcm_enable_reg_vci_3v0(0);

	usleep_range(5000, 5100);

	//disable vddr 1.5V
	gpiod_set_value(ctx->vddr1p5_enable_gpio, 0);
	usleep_range(2000, 2100);

	//disable 1.8V
	ret = lcm_enable_reg_vddi_1v8(0);

	//gpiod_set_value(ctx->sm3011_reset_gpio, 0);
	usleep_range(70000, 70100);

	return 0;
}

static int lcm_unprepare(struct drm_panel *panel)
{

	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->prepared)
		return 0;

	lcm_dcs_write_seq_static(ctx, 0x28);
	usleep_range(25000, 25100);
	lcm_dcs_write_seq_static(ctx, 0x10);
	usleep_range(125000, 125100);
	ctx->error = 0;
	ctx->prepared = false;
	lcm_panel_power_disable(ctx);

	pr_info("%s Successful\n", __func__);

	return 0;
}

static int lcm_panel_power_enable(struct lcm *ctx)
{
	int ret = 0;

	//set vddi 1.8v
	ret = regulator_set_voltage(g_pmic->reg_vddi_1v8,
				1800000, 1800000);
	ret = lcm_enable_reg_vddi_1v8(1);
	usleep_range(5000, 5100);

	//enable 1.5V gpio165
	gpiod_set_value(ctx->vddr1p5_enable_gpio, 1);
	usleep_range(1000, 1100);

	//set vci 3.0v
	ret = regulator_set_voltage(g_pmic->reg_vci_3v0,
				3000000, 3000000);
	ret = lcm_enable_reg_vci_3v0(1);
	usleep_range(10000, 10100);

	//gpiod_set_value(ctx->sm3011_reset_gpio, 1);

	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(1000, 1100);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(1000, 1100);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(12000, 12100);

	return ret;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	if (ctx->prepared)
		return 0;

	lcm_panel_power_enable(ctx);
	lcm_panel_init(ctx);
	usleep_range(2000, 2100);
	//lcm_panel_seed(ctx);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;
#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif

	pr_info("%s Successful\n", __func__);
	return ret;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;

	return 0;
}

#define HFP (12)
#define HSA (10)
#define HBP (13)
#define VFP (20)
#define VSA (2)
#define VBP (8)

#define VAC_FHD (2520)
#define HAC_FHD (1080)

static const struct drm_display_mode display_mode[MODE_NUM] = {
	//fhd_sdc_60_mode
	{
		.clock = 170595,
		.hdisplay = HAC_FHD,
		.hsync_start = HAC_FHD + HFP,
		.hsync_end = HAC_FHD + HFP + HSA,
		.htotal = HAC_FHD + HFP + HSA + HBP,
		.vdisplay = VAC_FHD,
		.vsync_start = VAC_FHD + VFP,
		.vsync_end = VAC_FHD + VFP + VSA,
		.vtotal = VAC_FHD + VFP + VSA + VBP,
	},
	//fhd_sdc_120_mode
	{
		.clock = 341190,
		.hdisplay = HAC_FHD,
		.hsync_start = HAC_FHD + HFP,
		.hsync_end = HAC_FHD + HFP + HSA,
		.htotal = HAC_FHD + HFP + HSA + HBP,
		.vdisplay = VAC_FHD,
		.vsync_start = VAC_FHD + VFP,
		.vsync_end = VAC_FHD + VFP + VSA,
		.vtotal = VAC_FHD + VFP + VSA + VBP,
	},
};

#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params[MODE_NUM] = {
	//fhd_sdc_60_mode
	{
		.pll_clk = 413,
		.data_rate = 826,
		.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
		.cmd_null_pkt_en = 0,
		.cmd_null_pkt_len = 110,
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
		},
		.cust_esd_check = 0,
		.esd_check_enable = 0,
		.lcm_esd_check_table[0] = {
			.cmd = 0x0A,
			.count = 1,
			.para_list[0] = 0x9C,
		},
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
		.round_corner_en = 0,
		.corner_pattern_height = ROUND_CORNER_1K_H_TOP,
		.corner_pattern_height_bot = ROUND_CORNER_1K_H_BOT,
		.corner_pattern_tp_size_l = sizeof(top_rc_1k_pattern_l),
		.corner_pattern_lt_addr_l = (void *)top_rc_1k_pattern_l,
		.corner_pattern_tp_size_r =  sizeof(top_rc_1k_pattern_r),
		.corner_pattern_lt_addr_r = (void *)top_rc_1k_pattern_r,
#endif
		.phy_timcon = {
			.hs_trail = 14,
			.clk_trail = 15,
		},
		.dyn_fps = {
			.switch_en = 1,
			.vact_timing_fps = 60,
		},
	},

	//fhd_sdc_120_mode
	{
		.pll_clk = 413,
		.data_rate = 826,
		.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
		.cmd_null_pkt_en = 0,
		.cmd_null_pkt_len = 110,
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
		},
		.cust_esd_check = 0,
		.esd_check_enable = 0,
		.lcm_esd_check_table[0] = {
			.cmd = 0x0A,
			.count = 1,
			.para_list[0] = 0x9C,
		},
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
		.round_corner_en = 0,
		.corner_pattern_height = ROUND_CORNER_1K_H_TOP,
		.corner_pattern_height_bot = ROUND_CORNER_1K_H_BOT,
		.corner_pattern_tp_size_l = sizeof(top_rc_1k_pattern_l),
		.corner_pattern_lt_addr_l = (void *)top_rc_1k_pattern_l,
		.corner_pattern_tp_size_r =  sizeof(top_rc_1k_pattern_r),
		.corner_pattern_lt_addr_r = (void *)top_rc_1k_pattern_r,
#endif
		.phy_timcon = {
			.hs_trail = 14,
			.clk_trail = 15,
		},
		.dyn_fps = {
			.switch_en = 1,
			.vact_timing_fps = 120,
		},
	},
};

static int mtk_panel_ext_param_get(struct drm_panel *panel,
		struct drm_connector *connector,
		struct mtk_panel_params **ext_param,
		unsigned int id)
{
	int ret = 0;
	int mode_id = -1;
	struct drm_display_mode *m = get_mode_by_id(connector, id);

	mode_id = get_mode_enum(m);

	pr_info("mode:%d,mode_id:%d\n", id, mode_id);

	if (mode_id == FHD_SDC60)
		*ext_param = &ext_params[0];
	else if (mode_id == FHD_SDC120)
		*ext_param = &ext_params[1];
	else
		*ext_param = &ext_params[0];

	if (*ext_param)
		pr_debug("data_rate:%d\n", (*ext_param)->data_rate);
	else
		pr_info("ext_param is NULL;\n");

	return ret;
}

static int mtk_panel_ext_param_set(struct drm_panel *panel,
			 struct drm_connector *connector, unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	int mode_id = -1;
	struct drm_display_mode *m = get_mode_by_id(connector, mode);

	mode_id = get_mode_enum(m);

	pr_info("mode:%d,mode_id:%d\n", mode, mode_id);

	if (mode_id == FHD_SDC60)
		ext->params = &ext_params[0];
	else if (mode_id == FHD_SDC120)
		ext->params = &ext_params[1];
	else
		ext->params = &ext_params[0];

	return ret;
}


static int panel_ata_check(struct drm_panel *panel)
{
	/* Customer test by own ATA tool */
	return 1;
}

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb, void *handle,
				 unsigned int level)
{
	char bl_tb0[] = {0x51, 0x07, 0xFF};

	if (!dsi || !cb)
		return -EINVAL;

	bl_tb0[1] = level >> 8;
	bl_tb0[2] = level & 0xFF;

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));

	pr_info("backlight=%d,bl_tb0[1]=0x%x,bl_tb0[2]=0x%x\n",
			level, bl_tb0[1], bl_tb0[2]);
	return 0;
}

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("reset on=%d\n", on);

	gpiod_set_value(ctx->reset_gpio, on);

	return 0;
}

static int panel_send_pack_hs_cmd(void *dsi, struct LCM_setting_table *table,
		unsigned int lcm_cmd_count, dcs_write_gce_pack cb, void *handle)
{
	unsigned int i = 0;
	struct mtk_ddic_dsi_cmd send_cmd_to_ddic;

	if (lcm_cmd_count > MAX_TX_CMD_NUM_PACK) {
		pr_info("out of mtk_ddic_dsi_cmd\n");
		return 0;
	}

	for (i = 0; i < lcm_cmd_count; i++) {
		send_cmd_to_ddic.mtk_ddic_cmd_table[i].cmd_num = table[i].count;
		send_cmd_to_ddic.mtk_ddic_cmd_table[i].para_list = table[i].para_list;
	}
	send_cmd_to_ddic.is_hs = 1;
	send_cmd_to_ddic.is_package = 1;
	send_cmd_to_ddic.cmd_count = lcm_cmd_count;
	cb(dsi, handle, &send_cmd_to_ddic);

	return 0;
}

static int mode_switch_hs(struct drm_panel *panel, struct drm_connector *connector,
		void *dsi_drv, unsigned int cur_mode, unsigned int dst_mode,
			enum MTK_PANEL_MODE_SWITCH_STAGE stage, dcs_write_gce_pack cb)
{
	int ret = 0;
	int mode_id = -1;
	unsigned int lcm_cmd_count = 0;
	struct drm_display_mode *m = get_mode_by_id(connector, dst_mode);

	if (stage == BEFORE_DSI_POWERDOWN) {
		pr_info("cur_mode = %d dst_mode %d\n", cur_mode, dst_mode);
		mode_id = get_mode_enum(m);
		if (mode_id == FHD_SDC60) {
			pr_info("timing switch to FHD_SDC60\n");
			lcm_cmd_count = sizeof(fhd_timing_switch_cmd_sdc60) /
					sizeof(struct LCM_setting_table);
			panel_send_pack_hs_cmd(dsi_drv, fhd_timing_switch_cmd_sdc60,
					lcm_cmd_count, cb, NULL);
		} else if (mode_id == FHD_SDC120) {
			pr_info("timing switch to FHD_SDC120\n");
			lcm_cmd_count = sizeof(fhd_timing_switch_cmd_sdc120) /
					sizeof(struct LCM_setting_table);
			panel_send_pack_hs_cmd(dsi_drv, fhd_timing_switch_cmd_sdc120,
					lcm_cmd_count, cb, NULL);
		} else {
			lcm_cmd_count = sizeof(fhd_timing_switch_cmd_sdc60) /
					sizeof(struct LCM_setting_table);
			panel_send_pack_hs_cmd(dsi_drv, fhd_timing_switch_cmd_sdc60,
					lcm_cmd_count, cb, NULL);
		}
	}
	return ret;
}

static int lcm_get_modes(struct drm_panel *panel,
					struct drm_connector *connector)
{
	struct drm_display_mode *mode[MODE_NUM];
	int i = 0;

	mode[0] = drm_mode_duplicate(connector->dev, &display_mode[0]);
	if (!mode[0]) {
		pr_info("failed to add mode %ux%ux@%u\n",
			display_mode[0].hdisplay, display_mode[0].vdisplay,
				drm_mode_vrefresh(&display_mode[0]));
		return -ENOMEM;
	}

	drm_mode_set_name(mode[0]);
	mode[0]->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode[0]);
	pr_debug("en=%u, clock=%d,htotal=%d,vtotal=%d,hskew=%d,vrefresh=%d\n",
				mode[0], mode[0]->clock, mode[0]->htotal,
		mode[0]->vtotal, mode[0]->hskew, drm_mode_vrefresh(mode[0]));

	for (i = 1; i < MODE_NUM; i++) {
		mode[i] = drm_mode_duplicate(connector->dev, &display_mode[i]);
		pr_debug(" en=%u\n", mode[i]);
		if (!mode[i]) {
			pr_info("not enough memory\n");
			return -ENOMEM;
		}

		drm_mode_set_name(mode[i]);
		mode[i]->type = DRM_MODE_TYPE_DRIVER;
		drm_mode_probed_add(connector, mode[i]);
	}
	connector->display_info.width_mm = 68;   //align x3 panel physical w/h
	connector->display_info.height_mm = 159;

	return 1;
}


static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = panel_ata_check,
	.ext_param_set = mtk_panel_ext_param_set,
	.ext_param_get = mtk_panel_ext_param_get,
	.mode_switch_hs = mode_switch_hs,
};
#endif

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
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;
	struct lcm *ctx;
	struct device_node *backlight;
	int ret;

	pr_info("panel-boe-nt37705-dphy-cmd-fhd %s-\n", __func__);
	dsi_node = of_get_parent(dev->of_node);
	if (dsi_node) {
		endpoint = of_graph_get_next_endpoint(dsi_node, NULL);

		if (endpoint) {
			remote_node = of_graph_get_remote_port_parent(endpoint);
			if (!remote_node) {
				pr_info("No panel connected,skip probe lcm\n");
				return -ENODEV;
			}
			pr_info("device node name:%s\n", remote_node->name);
		}
	}
	if (remote_node != dev->of_node) {
		pr_info("skip probe due to not current lcm\n");
		return -ENODEV;
	}

	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	g_pmic = kzalloc(sizeof(struct lcm_pmic_info), GFP_KERNEL);
	if (!g_pmic)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_LPM | MIPI_DSI_CLOCK_NON_CONTINUOUS;

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight) {
			pr_info("fail to get backlight node\n");
			return -EPROBE_DEFER;
		}
	}

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		pr_info("cannot get reset-gpios %ld\n",
			 PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}

	g_pmic->reg_vddi_1v8 = devm_regulator_get_optional(dev, "1p8");
	if (IS_ERR(g_pmic->reg_vddi_1v8)) {
		pr_info("cannot get reg_vddi_1v8 %ld\n",
			PTR_ERR(g_pmic->reg_vddi_1v8));
	}
	ret = regulator_set_voltage(g_pmic->reg_vddi_1v8,
				1800000, 1800000);
	ret = lcm_enable_reg_vddi_1v8(1);
	usleep_range(5000, 5100);

	ctx->vddr1p5_enable_gpio = devm_gpiod_get(dev, "vddr1p5-enable", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vddr1p5_enable_gpio)) {
		pr_info(" cannot get vddr1p5_enable_gpio %ld\n",
			PTR_ERR(ctx->vddr1p5_enable_gpio));
		return PTR_ERR(ctx->vddr1p5_enable_gpio);
	}
	gpiod_set_value(ctx->vddr1p5_enable_gpio, 1);
	usleep_range(5000, 5100);

	g_pmic->reg_vci_3v0 = devm_regulator_get_optional(dev, "3p0");
	if (IS_ERR(g_pmic->reg_vci_3v0)) {
		pr_info("cannot get reg_vci_3v0 %ld\n",
			PTR_ERR(g_pmic->reg_vci_3v0));
	}
	ret = regulator_set_voltage(g_pmic->reg_vci_3v0,
				3000000, 3000000);
	ret = lcm_enable_reg_vci_3v0(1);

	ctx->prepared = true;
	ctx->enabled = true;
	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		pr_info("fail to mipi_dsi_attach ret = %d\n", ret);
		drm_panel_remove(&ctx->panel);
	}

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params[0], &ext_funcs, &ctx->panel);
	if (ret < 0) {
		pr_info("mtk_panel_ext_create Failed! ret = %d\n", ret);
		return ret;
	}

#endif

	pr_info("%s, end ret = %d\n", __func__, ret);
	return ret;
}

static int lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);
#if defined(CONFIG_MTK_PANEL_EXT)
	struct mtk_panel_ctx *ext_ctx = find_panel_ctx(&ctx->panel);
#endif

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_detach(ext_ctx);
	mtk_panel_remove(ext_ctx);
#endif

	return 0;
}

static const struct of_device_id lcm_of_match[] = {
	{
	    .compatible = "boe,nt37705,dphy,cmd,fhd",
	},
	{}
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-boe-nt37705-dphy-cmd-fhd",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

static int __init lcm_drv_init(void)
{
	int ret = 0;

	pr_notice("%s+\n", __func__);
	ret = mipi_dsi_driver_register(&lcm_driver);
	if (ret < 0)
		pr_notice("%s, Failed to register lcm driver: %d\n",
			__func__, ret);

	pr_notice("%s- ret:%d\n", __func__, ret);
	return 0;
}

static void __exit lcm_drv_exit(void)
{
	pr_notice("%s+\n", __func__);
	mipi_dsi_driver_unregister(&lcm_driver);
	pr_notice("%s-\n", __func__);
}
module_init(lcm_drv_init);
module_exit(lcm_drv_exit);

MODULE_AUTHOR("Castro Dong <castro.dong@mediatek.com>");
MODULE_DESCRIPTION("BOE NT37705 Panel Driver");
MODULE_LICENSE("GPL v2");
