// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/backlight.h>
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

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#include "../mediatek/mediatek_v2/mtk_log.h"
#endif

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mediatek_v2/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif

#include <linux/atomic.h>

#define DATA_RATE0		550
#define DATA_RATE1		1100
#define DATA_RATE2		720

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

static int current_fps = 120;

#define SPR_2D_RENDERING_DUMMY
#ifdef SPR_2D_RENDERING_DUMMY
#define SPR_2D_RENDERING (1)
#endif

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;

	bool prepared;
	bool enabled;
	bool gir_status;
	bool dc_status;
	bool doze_suspend;
	bool wqhd_en;

	unsigned int dynamic_fps;
	unsigned int gate_ic;
	unsigned int crc_level;

	int error;
};

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[200];
};

static atomic_t doze_enable = ATOMIC_INIT(0);
static int gDcThreshold = 450;
static bool gDcEnable;
static bool doze_had;
static bool dimming_on;
static struct lcm *panel_ctx;

#define lcm_dcs_write_seq(ctx, seq...) \
({\
	const u8 d[] = { seq };\
	BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64, "DCS sequence too big for stack");\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

#define lcm_dcs_write_seq_static(ctx, seq...) \
({\
	static const u8 d[] = { seq };\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

static int panel_doze_disable(struct drm_panel *panel, void *dsi, dcs_write_gce cb, void *handle);
static int panel_doze_disable_grp(struct drm_panel *panel,
	void *dsi, dcs_grp_write_gce cb, void *handle);


static inline struct lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct lcm, panel);
}

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
		dev_err(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
}

static int lcm_dcs_read(struct lcm *ctx, u8 cmd, void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return 0;

	ret = mipi_dsi_dcs_read(dsi, cmd, data, len);
	if (ret < 0) {
		dev_err(ctx->dev, "error %d reading dcs seq:(%#x)\n", ret, cmd);
		ctx->error = ret;
	}

	return ret;
}

static bool is_aod_mode(void)
{
	u8 buffer[3] = {0};
	int val;
	bool ret;

	if (!panel_ctx) {
		pr_err("panel_ctx is null\n");
		return false;
	}
	lcm_dcs_read(panel_ctx,  0x0A, buffer, 1);
	val = buffer[0] | (buffer[1] << 8);
	pr_info("%s val_oa =%x\n", __func__, val);

	ret = (val == 0x9c) ? false : true;

	return ret;
}

#ifdef PANEL_SUPPORT_READBACK
static void lcm_panel_get_data(struct lcm *ctx)
{
	u8 buffer[3] = {0};
	static int ret;

	if (ret == 0) {
		ret = lcm_dcs_read(ctx,  0x0A, buffer, 1);
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

#define REGFLAG_DELAY       0xFFFC
#define REGFLAG_UDELAY  0xFFFB
#define REGFLAG_END_OF_TABLE    0xFFFD
#define REGFLAG_RESET_LOW   0xFFFE
#define REGFLAG_RESET_HIGH  0xFFFF

static struct LCM_setting_table init_setting[] = {
	//enter aod with no black
	{0xFF,  04, {0xAA, 0x55, 0xA5, 0x80}},
	{0x6F,  01, {0x61}},
	{0xF3,  01, {0x80}},
	//dimming setting
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x00}},
	{0xB2, 01, {0x98}},
	{0x6F, 01, {0x02}},
	{0xB2, 02, {0x10, 0x10}},
	// AVDD EL power setting
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x00}},
	{0xB5, 01, {0x80}},
	{0x6F, 01, {0x05}},
	{0xB5, 04, {0x7F, 0x00, 0x28, 0x00}},
	{0x6F, 01, {0x0B}},
	{0xB5, 03, {0x00, 0x28, 0x00}},
	{0x6F, 01, {0x10}},
	{0xB5, 05, {0x28, 0x28, 0x28, 0x28, 0x28}},
	{0x6F, 01, {0x16}},
	{0xB5, 02, {0x0D, 0x19}},
	{0x6F, 01, {0x1B}},
	{0xB5, 02, {0x0D, 0x1A}},
	//RCN setting
	{0xFF, 04, {0xAA, 0x55, 0xA5, 0x80}},
	{0x6F, 01, {0x1D}},
	{0xF2, 01, {0x05}},
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x00}},
	{0x6F, 01, {0x1C}},
	//Demura gain mapping
	{0xC0, 03, {0x03, 0x12, 0x11}},
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x00}},
	{0x03, 01, {0x01}},
	// DSC setting
	{0x3B, 04, {0x00, 0x14, 0x00, 0x14}},
	{0x90, 01, {0x11}},
	{0x91, 18, {0xAB, 0x28, 0x00, 0x0C, 0xC2, 0x00, 0x02, 0x32,
	0x01, 0x31, 0x00, 0x08, 0x08, 0xBB, 0x07, 0x7B, 0x10, 0xF0}},
	{0x2C, 01, {0x00}},
	{0x51, 04, {0x07, 0xFF, 0x0F, 0xFE}},
	{0x53, 01, {0x20}},
	{0x35, 01, {0x00}},
	{0x2A, 04, {0x00, 0x00, 0x04, 0xC3}},
	{0x2B, 04, {0x00, 0x00, 0x0A, 0x97}},
	// FCON1(HFR1,60Hz)
	{0x2F, 01, {0x02}},
	{0x5F, 01, {0x01}},
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x00}},
	{0x6F, 01, {0x09}},
	{0xC0, 01, {0x00}},
	// Video Trim mipi speed = 1100Mbps @ osc 127.9525Mhz
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x01}},
	{0xCC, 07, {0x9B, 0x01, 0x94, 0xD0, 0x22, 0x02, 0x00}},
	// Gamma update
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x02}},
	{0xCC, 01, {0x30}},
	{0xCE, 01, {0x01}},
	{REGFLAG_DELAY, 20, {}},
	{0xCC, 01, {0x00}},
	//ddic round corner on
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x07}},
	{0xC0, 01, {0x01}},
	// ESD active low
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x00}},
	{0x6F, 01, {0x05}},
	{0xBE, 01, {0x88}},
	// Source-on-test
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x00}},
	{0x6F, 01, {0x02}},
	{0xC1, 01, {0xD8}},
	//demura
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x04}},
	{0x6F, 01, {0x0D}},
	{0xCA, 01, {0x05}},

	{0x11, 01, {0x00}},
	{REGFLAG_DELAY, 120, {}},
	{0x29, 01, {0x00}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table base_120hz[] = {
	/* TE width = 40H */
	{0x6F, 01, {0x01}},
	{0x35, 01, {0x28}},
	/* Page 0 */
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x00}},
	/* MSYNC_MAX_AFP_HFR2 = 2751 */
	{0x6F, 01, {0x18}},
	{0xC0, 02, {0x0A, 0xBF}},
	/* MSYNC_MAX_AFP_HFR3 = 1839 */
	{0x6F, 01, {0x1A}},
	{0xC0, 02, {0x07, 0x2F}},
	/* Valid TE start = 2712 */
	{0x6F, 01, {0x01}},
	{0x40, 02, {0x0A, 0x98}},
	/* Valid TE end = 5443 */
	{0x6F, 01, {0x03}},
	{0x40, 02, {0x15, 0x43}},
	/* Msync off */
	{0x40, 01, {0x00}},
};

static void push_table(struct lcm *ctx, struct LCM_setting_table *table, unsigned int count)
{
	unsigned int i, j;
	unsigned char temp[255] = {0};

	for (i = 0; i < count; i++) {
		unsigned int cmd;

		cmd = table[i].cmd;
		memset(temp, 0, sizeof(temp));
		switch (cmd) {
		case REGFLAG_DELAY:
			msleep(table[i].count);
			break;
		case REGFLAG_END_OF_TABLE:
			break;
		default:
			temp[0] = cmd;
			for (j = 0; j < table[i].count; j++)
				temp[j+1] = table[i].para_list[j];
			lcm_dcs_write(ctx, temp, table[i].count+1);
		}
	}
}

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

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->prepared)
		return 0;

	lcm_dcs_write_seq_static(ctx, 0x28);
	msleep(20);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(125);

	ctx->error = 0;
	ctx->prepared = false;
	ctx->crc_level = 0;
	ctx->doze_suspend = false;

#if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
	lcm_panel_bias_disable();
#else
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
#endif

	return 0;
}

static void lcm_panel_init(struct lcm *ctx)
{
	pr_info("%s: +\n", __func__);
	if (ctx->prepared) {
		pr_info("%s: panel has been prepared, nothing to do!\n", __func__);
		goto err;
	}

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		goto err;
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(12 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(1 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(10 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(12 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	push_table(ctx, init_setting,
		sizeof(init_setting) / sizeof(struct LCM_setting_table));
	ctx->prepared = true;
err:
	pr_info("%s: -\n", __func__);
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s: +\n", __func__);
	if (ctx->prepared)
		return 0;

#if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
	lcm_panel_bias_enable();
#endif

	lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_rst(panel);
#endif
#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif

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

#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params = {
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
		.ver = 1,
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
	.lp_perline_en = 1,
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
	.lp_perline_en = 1,
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
	.lp_perline_en = 1,
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

struct drm_display_mode *get_mode_by_id(struct drm_connector *connector,
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

static int mtk_panel_ext_param_get(struct drm_panel *panel,
	struct drm_connector *connector,
	struct mtk_panel_params **ext_param,
	unsigned int mode)
{
	int ret = 0;
	int dst_fps = 0;
	struct drm_display_mode *m_dst = get_mode_by_id(connector, mode);

	dst_fps = m_dst ? drm_mode_vrefresh(m_dst) : -EINVAL;

	if (dst_fps == MODE0_FPS)
		*ext_param = &ext_params;
	else if (dst_fps == MODE2_FPS)
		*ext_param = &ext_params_90hz;
	else if (dst_fps == MODE1_FPS)
		*ext_param = &ext_params_120hz;
	else {
		pr_err("%s, dst_fps %d\n", __func__, dst_fps);
		ret = -EINVAL;
	}

	if (!ret)
		current_fps = dst_fps;

	return ret;
}

static int mtk_panel_ext_param_set(struct drm_panel *panel,
			struct drm_connector *connector, unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	int dst_fps = 0;
	struct drm_display_mode *m_dst = get_mode_by_id(connector, mode);

	dst_fps = m_dst ? drm_mode_vrefresh(m_dst) : -EINVAL;

	if (dst_fps == MODE0_FPS)
		ext->params = &ext_params;
	else if (dst_fps == MODE2_FPS)
		ext->params = &ext_params_90hz;
	else if (dst_fps == MODE1_FPS)
		ext->params = &ext_params_120hz;
	else {
		pr_err("%s, dst_fps %d\n", __func__, dst_fps);
		ret = -EINVAL;
	}

	if (!ret)
		current_fps = dst_fps;

	return ret;
}

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	char bl_tb[] = {0x51, 0x07, 0xff};
	char dimmingon_tb[] = {0x53, 0x28};
	char dimmingoff_tb[] = {0x53, 0x20};

	if (gDcEnable && level < gDcThreshold)
		level = gDcThreshold;

	bl_tb[1] = (level >> 8) & 0xFF;
	bl_tb[2] = level & 0xFF;
	if (!cb)
		return -1;
	if (atomic_read(&doze_enable)) {
		pr_info("%s: Return it when aod on, %d %d %d\n",
			__func__, level, bl_tb[1], bl_tb[2]);
		return 0;
	}
	if (doze_had && is_aod_mode()) {
		pr_info("%s is in AOD mode need to call doze disable\n", __func__);
		panel_doze_disable(&panel_ctx->panel, dsi, cb, handle);
	}
	doze_had = false;
	pr_info("%s %d %d %d\n", __func__, level, bl_tb[1], bl_tb[2]);
	cb(dsi, handle, bl_tb, ARRAY_SIZE(bl_tb));

	if (!dimming_on && level) {
		cb(dsi, handle, dimmingon_tb, ARRAY_SIZE(dimmingon_tb));
		dimming_on = true;
		pr_info("%s dimming status:%d\n", __func__, dimming_on);
	} else if (dimming_on && !level) {
		cb(dsi, handle, dimmingoff_tb, ARRAY_SIZE(dimmingoff_tb));
		dimming_on = false;
		pr_info("%s dimming status:%d\n", __func__, dimming_on);
	}

	return 0;
}

static int lcm_set_bl_elvss_cmdq(void *dsi, dcs_grp_write_gce cb, void *handle,
		struct mtk_bl_ext_config *bl_ext_config)
{
	int pulses;
	int level;
	bool need_cfg_elvss = false;

	static struct mtk_panel_para_table bl_tb[] = {
			{3, {0x51, 0x0f, 0xff}},
		};
	static struct mtk_panel_para_table bl_elvss_tb[] = {
			{3, { 0x51, 0x0f, 0xff}},
			{2, { 0x83, 0xff}},
		};
	static struct mtk_panel_para_table elvss_tb[] = {
			{2, { 0x83, 0xff}},
		};
	static struct mtk_panel_para_table dimmingon_tb[] = {
			{3, {0x53, 0x28}},
		};
	static struct mtk_panel_para_table dimmingoff_tb[] = {
			{3, {0x53, 0x20}},
		};

	if (!cb)
		return -1;

	pulses = bl_ext_config->elvss_pn;
	level = bl_ext_config->backlight_level;

	if (bl_ext_config->cfg_flag & (0x1<<SET_BACKLIGHT_LEVEL)) {

		if (bl_ext_config->cfg_flag & (0x1<<SET_ELVSS_PN))
			need_cfg_elvss = true;

		if (gDcEnable && level < gDcThreshold)
			level = gDcThreshold;

		if (atomic_read(&doze_enable)) {
			pr_info("%s: Return it when aod on, %d %d %d\n",
				__func__, level, bl_tb[0].para_list[1], bl_tb[0].para_list[2]);
			if (need_cfg_elvss) {
				pr_info("%s elvss = -%d\n", __func__, pulses);
				elvss_tb[0].para_list[1] = (u8)((1<<7)|pulses);
				cb(dsi, handle, elvss_tb, ARRAY_SIZE(elvss_tb));
			}
			return 0;
		}

		if (doze_had && is_aod_mode()) {
			pr_info("%s is in AOD mode need to call doze disable\n", __func__);
			panel_doze_disable_grp(&panel_ctx->panel, dsi, cb, handle);
		}

		doze_had = false;

		if (need_cfg_elvss) {
			bl_elvss_tb[0].para_list[1] = (level >> 8) & 0xf;
			bl_elvss_tb[0].para_list[2] = (level) & 0xFF;
			pr_info("%s backlight = -%d\n", __func__, level);
			pr_info("%s elvss = -%d\n", __func__, pulses);
			bl_elvss_tb[1].para_list[1] = (u8)((1<<7)|pulses);
			cb(dsi, handle, bl_elvss_tb, ARRAY_SIZE(bl_elvss_tb));
		} else {
			pr_info("%s backlight = -%d\n", __func__, level);
			bl_tb[0].para_list[1] = (level >> 8) & 0xf;
			bl_tb[0].para_list[2] = (level) & 0xFF;
			cb(dsi, handle, bl_tb, ARRAY_SIZE(bl_tb));
		}

		if (!dimming_on && level) {
			cb(dsi, handle, dimmingon_tb, ARRAY_SIZE(dimmingon_tb));
			dimming_on = true;
			pr_info("%s dimming status:%d\n", __func__, dimming_on);
		} else if (dimming_on && !level) {
			cb(dsi, handle, dimmingoff_tb, ARRAY_SIZE(dimmingoff_tb));
			dimming_on = false;
			pr_info("%s dimming status:%d\n", __func__, dimming_on);
		}

	} else if (bl_ext_config->cfg_flag & (0x1<<SET_ELVSS_PN)) {
		pr_info("%s elvss = -%d\n", __func__, pulses);
		elvss_tb[0].para_list[1] = (u8)((1<<7)|pulses);
		cb(dsi, handle, elvss_tb, ARRAY_SIZE(elvss_tb));
	}
	return 0;
}


static struct LCM_setting_table mode_120hz_setting_gir_off[] = {
	/* frequency select 120hz */
	{0x2F, 01, {0x03}},
	{0x5F, 01, {0x01}},	//gir off
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x00}},
	{0x6F, 01, {0x0A}},
	{0xC0, 01, {0x20}},	//Preset1 gamma
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table mode_120hz_setting_gir_on[] = {
	/* frequency select 120hz */
	{0x2F, 01, {0x03}},
	{0x5F, 01, {0x00}},	//gir on
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x00}},
	{0x6F, 01, {0x0A}},
	{0xC0, 01, {0x60}},	//Preset3 gamma
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table mode_90hz_setting_gir_off[] = {
	/* frequency select 90hz */
	{0x2F, 01, {0x04}},
	{0x5F, 01, {0x01}},	//gir off
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x00}},
	{0x6F, 01, {0x0B}},
	{0xC0, 01, {0x20}},	//Preset1 gamma
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table mode_90hz_setting_gir_on[] = {
	/* frequency select 90hz */
	{0x2F, 01, {0x04}},
	{0x5F, 01, {0x00}},	//gir on
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x00}},
	{0x6F, 01, {0x0B}},
	{0xC0, 01, {0x60}},	//Preset3 gamma
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table mode_60hz_setting_gir_off[] = {
	/* frequency select 60hz */
	{0x2F, 01, {0x02}},
	{0x5F, 01, {0x01}},	//gir off
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x00}},
	{0x6F, 01, {0x09}},
	{0xC0, 01, {0x00}},	//Normal gamma
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table mode_60hz_setting_gir_on[] = {
	/* frequency select 60hz */
	{0x2F, 01, {0x02}},
	{0x5F, 01, {0x00}},	//gir on
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x00}},
	{0x6F, 01, {0x09}},
	{0xC0, 01, {0x40}},	//Preset2 gamma
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static void mode_switch_to_120(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s state = %d\n", __func__, stage);
	if (stage == BEFORE_DSI_POWERDOWN) {
		if (ctx->gir_status)
			push_table(ctx, mode_120hz_setting_gir_on,
			sizeof(mode_120hz_setting_gir_on) / sizeof(struct LCM_setting_table));
		else
			push_table(ctx, mode_120hz_setting_gir_off,
			sizeof(mode_120hz_setting_gir_off) / sizeof(struct LCM_setting_table));

		push_table(ctx, base_120hz,
			sizeof(base_120hz) / sizeof(struct LCM_setting_table));
		ctx->dynamic_fps = 120;
	}
}

static void mode_switch_to_90(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s state = %d\n", __func__, stage);
	if (stage == BEFORE_DSI_POWERDOWN) {
		if (ctx->gir_status)
			push_table(ctx, mode_90hz_setting_gir_on,
			sizeof(mode_90hz_setting_gir_on) / sizeof(struct LCM_setting_table));
		else
			push_table(ctx, mode_90hz_setting_gir_off,
			sizeof(mode_90hz_setting_gir_off) / sizeof(struct LCM_setting_table));
		ctx->dynamic_fps = 90;
	}
}

static void mode_switch_to_60(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s state = %d\n", __func__, stage);
	if (stage == BEFORE_DSI_POWERDOWN) {
		if (ctx->gir_status)
			push_table(ctx, mode_60hz_setting_gir_on,
			sizeof(mode_60hz_setting_gir_on) / sizeof(struct LCM_setting_table));
		else
			push_table(ctx, mode_60hz_setting_gir_off,
			sizeof(mode_60hz_setting_gir_off) / sizeof(struct LCM_setting_table));
		ctx->dynamic_fps = 60;
	}
}

static int mode_switch(struct drm_panel *panel,
		struct drm_connector *connector, unsigned int cur_mode,
		unsigned int dst_mode, enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	int ret = 0;
	int dst_fps = 0, cur_fps = 0;
	int dst_vdisplay = 0, dst_hdisplay = 0;
	int cur_vdisplay = 0, cur_hdisplay = 0;
	bool isFpsChange = false;
	struct drm_display_mode *m_dst = get_mode_by_id(connector, dst_mode);
	struct drm_display_mode *m_cur = get_mode_by_id(connector, cur_mode);

	if (cur_mode == dst_mode)
		return ret;

	dst_fps = m_dst ? drm_mode_vrefresh(m_dst) : -EINVAL;
	dst_vdisplay = m_dst ? m_dst->vdisplay : -EINVAL;
	dst_hdisplay = m_dst ? m_dst->hdisplay : -EINVAL;
	cur_fps = m_cur ? drm_mode_vrefresh(m_cur) : -EINVAL;
	cur_vdisplay = m_cur ? m_cur->vdisplay : -EINVAL;
	cur_hdisplay = m_cur ? m_cur->hdisplay : -EINVAL;

	isFpsChange = ((dst_fps == cur_fps) && (dst_fps != -EINVAL)
			&& (cur_fps != -EINVAL)) ? false : true;

	pr_info("%s isFpsChange = %d\n", __func__, isFpsChange);
	pr_info("%s dst_mode vrefresh = %d, vdisplay = %d, vdisplay = %d\n",
		__func__, dst_fps, dst_vdisplay, dst_hdisplay);
	pr_info("%s cur_mode vrefresh = %d, vdisplay = %d, vdisplay = %d\n",
		__func__, cur_fps, cur_vdisplay, cur_hdisplay);

	if (isFpsChange) {
		if (dst_fps == MODE0_FPS)
			mode_switch_to_60(panel, stage);
		else if (dst_fps == MODE2_FPS)
			mode_switch_to_90(panel, stage);
		else if (dst_fps == MODE1_FPS)
			mode_switch_to_120(panel, stage);
		else
			ret = 1;
	}

	return ret;
}

struct mtk_panel_para_table msync_level_90[] = {
	/* frequency select 90hz */
	{2, {0x2F, 0x04}},
	{2, {0x5F, 0x01}},     //gir off
	/* Page 0 */
	{6, {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00}},
	/* Valid TE start = 3640 */
	{2, {0x6F, 0x01}},
	{3, {0x40, 0x0E, 0x38}},
	/* Msync on */
	{2, {0x40, 0x07}},
};

struct mtk_panel_para_table msync_level_120[] = {
	/* frequency select 120hz */
	{2, {0x2F, 0x03}},
	{2, {0x5F, 0x01}},     //gir off
	/* Page 0 */
	{6, {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00}},
	/* Valid TE start = 2712 */
	{2, {0x6F, 0x01}},
	{3, {0x40, 0x0A, 0x98}},
	/* Msync on */
	{2, {0x40, 0x07}},
};

struct mtk_panel_para_table request_te_cmd[] = {
	{2, {0x3F, 0x00}},
};

struct mtk_panel_para_table rte_off[] = {
	/* Msync off */
	{2, {0x40, 0x00}},
};

static int msync_te_level_switch_grp(void *dsi, dcs_grp_write_gce cb,
		void *handle, struct drm_panel *panel, unsigned int fps_level)
{
	int ret = 0;
	struct lcm *ctx = NULL;

	ctx = panel_to_lcm(panel);

	pr_info("[Msync2.0]%s:%d fps_level:%d\n", __func__, __LINE__, fps_level);
	if (fps_level == RTE_CMD) { /*send request TE*/
		pr_info("[Msync2.0]%s:%d send request TE\n", __func__, __LINE__);
		cb(dsi, handle, request_te_cmd, ARRAY_SIZE(request_te_cmd));
	} else if (fps_level == RTE_OFF) { /*disable request te */
		pr_info("[Msync2.0]%s:%d RTE off, current fps:%d, fps_level:%d\n",
			__func__, __LINE__, current_fps, fps_level);
		cb(dsi, handle, rte_off, ARRAY_SIZE(rte_off));
	} else {
		pr_info("[Msync2.0]%s:%d switch to 120fps\n", __func__, __LINE__);
		cb(dsi, handle, msync_level_120, ARRAY_SIZE(msync_level_120));
	}

	pr_info("[Msync2.0]%s:%d fps_level:%d\n", __func__, __LINE__, fps_level);
	return ret;
}

static int panel_doze_enable(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	atomic_set(&doze_enable, 1);
	doze_had = true;
	pr_info("%s !-\n", __func__);
	return 0;
}

static int panel_doze_disable(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	char page0_tb[] = {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00};
	char data_remap_enable_tb[] = {0xB2, 0x98};
	char cmd0_tb[] = {0x65, 0x00};
	char cmd1_tb[] = {0x38, 0x00};
	char page_51[] = {0x51, 0x00, 0x08};
	char cmd2_tb[] = {0x2C, 0x00};

	pr_info("%s +\n", __func__);
	cb(dsi, handle, page0_tb, ARRAY_SIZE(page0_tb));
	cb(dsi, handle, data_remap_enable_tb, ARRAY_SIZE(data_remap_enable_tb));
	cb(dsi, handle, cmd0_tb, ARRAY_SIZE(cmd0_tb));
	cb(dsi, handle, cmd1_tb, ARRAY_SIZE(cmd1_tb));
	cb(dsi, handle, page_51, ARRAY_SIZE(page_51));
	usleep_range(50 * 1000, 50 * 1000 + 10);
	cb(dsi, handle, cmd2_tb, ARRAY_SIZE(cmd2_tb));

	atomic_set(&doze_enable, 0);
	pr_info("%s -\n", __func__);

	return 0;
}

static int panel_doze_disable_grp(struct drm_panel *panel,
	void *dsi, dcs_grp_write_gce cb, void *handle)
{
	static struct mtk_panel_para_table cmd0_tb[] = {
			{6, {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00}},
			{2, {0xB2, 0x98}},
			{2, {0x65, 0x00}},
			{2, {0x38, 0x00}},
			{3, {0x51, 0x00, 0x08}},
		};
	static struct mtk_panel_para_table cmd1_tb[] = {
			{2, {0x2C, 0x00}},
		};
	cb(dsi, handle, cmd0_tb, ARRAY_SIZE(cmd0_tb));

	usleep_range(50 * 1000, 50 * 1000 + 10);
	cb(dsi, handle, cmd1_tb, ARRAY_SIZE(cmd1_tb));

	atomic_set(&doze_enable, 0);
	pr_info("%s -\n", __func__);

	return 0;
}



static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.ext_param_set = mtk_panel_ext_param_set,
	.ext_param_get = mtk_panel_ext_param_get,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.set_bl_elvss_cmdq = lcm_set_bl_elvss_cmdq,
	.mode_switch = mode_switch,
	.doze_enable = panel_doze_enable,
	.doze_disable = panel_doze_disable,
	.msync_te_level_switch_grp = msync_te_level_switch_grp,
};
#endif

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
	struct drm_display_mode *mode0, *mode1/*, *mode2*/;

	mode0 = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode0) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}
	drm_mode_set_name(mode0);
	mode0->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode0);

	/*
	mode2 = drm_mode_duplicate(connector->dev, &middle_mode);
	if (!mode2) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			middle_mode.hdisplay, middle_mode.vdisplay,
			drm_mode_vrefresh(&middle_mode));
		return -ENOMEM;
	}
	drm_mode_set_name(mode2);
	mode2->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode2);
	*/
	mode1 = drm_mode_duplicate(connector->dev, &performence_mode);
	if (!mode1) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			performence_mode.hdisplay, performence_mode.vdisplay,
			drm_mode_vrefresh(&performence_mode));
		return -ENOMEM;
	}
	drm_mode_set_name(mode1);
	mode1->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode1);

	connector->display_info.width_mm = 71;
	connector->display_info.height_mm = 153;

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
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;
	struct lcm *ctx;
	struct device_node *backlight;
	int ret;

	pr_info("%s l12a-42-02-0a-dsc +\n", __func__);

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
		pr_info("%s+ skip probe due to not current lcm\n", __func__);
		return -ENODEV;
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

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(dev, "%s: cannot get reset-gpios %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);

	ctx->prepared = true;
	ctx->enabled = true;
	ctx->dynamic_fps = 60;
	ctx->wqhd_en = true;
	ctx->dc_status = false;
	ctx->crc_level = 0;

	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif

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
	{ .compatible = "l12a_42_02_0a_dsc_cmd,lcm", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "l12a_42_02_0a_dsc_cmd,lcm",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("neil.yu <neil.yu@mediatek.com>");
MODULE_DESCRIPTION("l12a_42_02_0a_dsc_cmd oled panel driver");
MODULE_LICENSE("GPL v2");
