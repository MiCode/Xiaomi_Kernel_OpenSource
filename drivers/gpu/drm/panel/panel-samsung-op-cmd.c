// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
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

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_log.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mediatek_v2/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif

struct lcm_pmic_info {
	struct regulator *reg_vufs18;
	struct regulator *reg_vmch3p0;
};

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bias_pos, *bias_neg;
	struct gpio_desc *bias_gpio;
	struct gpio_desc *vddr1p5_enable_gpio;
	struct gpio_desc *te_switch_gpio, *te_out_gpio;
	bool prepared;
	bool enabled;
	int error;
};

static char bl_tb0[] = { 0x51, 0x07, 0xff};

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
		dev_info(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
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
		dev_info(ctx->dev, "error %d reading dcs seq:(%#x)\n", ret, cmd);
		ctx->error = ret;
	}

	return ret;
}

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

static struct lcm_pmic_info *g_pmic;
static unsigned int lcm_get_reg_vufs18(void)
{
	unsigned int volt = 0;

	if (regulator_is_enabled(g_pmic->reg_vufs18))
		volt = regulator_get_voltage(g_pmic->reg_vufs18);

	return volt;
}

static unsigned int lcm_get_reg_vmch3p0(void)
{
	unsigned int volt = 0;

	if (regulator_is_enabled(g_pmic->reg_vmch3p0))
		volt = regulator_get_voltage(g_pmic->reg_vmch3p0);

	return volt;
}

static unsigned int lcm_enable_reg_vufs18(int en)
{
	unsigned int ret = 0, volt = 0;

	if (en) {
		if (!IS_ERR_OR_NULL(g_pmic->reg_vufs18)) {
			ret = regulator_enable(g_pmic->reg_vufs18);
			pr_info("[lh]Enable the Regulator vufs1p8ret=%d.\n", ret);
			volt = lcm_get_reg_vufs18();
			pr_info("[lh]get the Regulator vufs1p8 =%d.\n", volt);
		}
	} else {
		if (!IS_ERR_OR_NULL(g_pmic->reg_vufs18)) {
			ret = regulator_disable(g_pmic->reg_vufs18);
			pr_info("[lh]disable the Regulator vufs1p8 ret=%d.\n", ret);
		}

	}
	return ret;

}

static unsigned int lcm_enable_reg_vmch3p0(int en)
{
	unsigned int ret = 0, volt = 0;

	if (en) {
		if (!IS_ERR_OR_NULL(g_pmic->reg_vmch3p0)) {
			ret = regulator_enable(g_pmic->reg_vmch3p0);
			pr_info("[lh]Enable the Regulator vmch3p0 ret=%d.\n", ret);
			volt = lcm_get_reg_vmch3p0();
			pr_info("[lh]get the Regulator vmch3p0 =%d.\n", volt);
		}
	} else {
		if (!IS_ERR_OR_NULL(g_pmic->reg_vmch3p0)) {
			ret = regulator_disable(g_pmic->reg_vmch3p0);
			pr_info("[lh]disable the Regulator vmch3p0 ret=%d.\n", ret);
		}
	}
	return ret;

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

	lcm_dcs_write_seq_static(ctx, MIPI_DCS_ENTER_SLEEP_MODE);
	lcm_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_OFF);
	msleep(200);

	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	ctx->vddr1p5_enable_gpio = devm_gpiod_get(ctx->dev, "vddr1p5-enable", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->vddr1p5_enable_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->vddr1p5_enable_gpio);
	msleep(20);
	lcm_enable_reg_vufs18(0);
	msleep(20);
	lcm_enable_reg_vmch3p0(0);

	ctx->error = 0;
	ctx->prepared = false;
	pr_info("[lh]%s-\n", __func__);

	return 0;
}

static void lcm_panel_init(struct lcm *ctx)
{
	//DSC 10bit
	lcm_dcs_write_seq_static(ctx, 0x9E,
		0x11, 0x00, 0x00, 0xAB, 0x30, 0x80, 0x0C, 0x90, 0x05,
		0xA0, 0x00, 0x18, 0x02, 0xD0, 0x02, 0xD0, 0x02, 0x00,
		0x02, 0x86, 0x00, 0x20, 0x02, 0x83, 0x00, 0x0A, 0x00,
		0x0D, 0x04, 0x86, 0x03, 0x2E, 0x18, 0x00, 0x10, 0xF0,
		0x07, 0x10, 0x20, 0x00, 0x06, 0x0F, 0x0F, 0x33, 0x0E,
		0x1C, 0x2A, 0x38, 0x46, 0x54, 0x62, 0x69, 0x70, 0x77,
		0x79, 0x7B, 0x7D, 0x7E, 0x02, 0x02, 0x22, 0x00, 0x2A,
		0x40, 0x2A, 0xBE, 0x3A, 0xFC, 0x3A, 0xFA, 0x3A, 0xF8,
		0x3B, 0x38, 0x3B, 0x78, 0x3B, 0xB6, 0x4B, 0xB6, 0x4B,
		0xF4, 0x4B, 0xF4, 0x6C, 0x34, 0x84, 0x74, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x9D, 0x01);

	lcm_dcs_write_seq_static(ctx, 0x11);
	msleep(120);
	/* TSP_SYNC1 Fixed Setting */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x22, 0xB9);
	lcm_dcs_write_seq_static(ctx, 0xB9, 0xA1, 0xB1);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x3A, 0xB9);
	lcm_dcs_write_seq_static(ctx, 0xB9, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x26, 0xB9);
	lcm_dcs_write_seq_static(ctx, 0xB9, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xF7, 0x0F);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
	/* TSP_SYNC3 Fixed Setting */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x24, 0xB9);
	lcm_dcs_write_seq_static(ctx, 0xB9, 0x21);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x38, 0xB9);
	lcm_dcs_write_seq_static(ctx, 0xB9, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x2A, 0xB9);
	lcm_dcs_write_seq_static(ctx, 0xB9, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xF7, 0x0F);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
	/* 120hz Transition */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x16, 0xF2);
	lcm_dcs_write_seq_static(ctx, 0xF2, 0x1B, 0x50);
	lcm_dcs_write_seq_static(ctx, 0xBD, 0x21, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x60, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x10, 0xBD);
	lcm_dcs_write_seq_static(ctx, 0xBD, 0x10);
	lcm_dcs_write_seq_static(ctx, 0xF7, 0x0F);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
	/* Common Setting */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xF2, 0x00, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
	/* TE(Vsync) ON */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xB9, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
	/* CASET/PASET Setting */
	lcm_dcs_write_seq_static(ctx, 0x2A, 0x00, 0x00, 0x05, 0x9F);
	lcm_dcs_write_seq_static(ctx, 0x2B, 0x00, 0x00, 0x0C, 0x8F);
	/* Scaler Setting */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xC3, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
	/* Pre-charge time setting */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x2B, 0xF6);
	lcm_dcs_write_seq_static(ctx, 0xF6, 0x60, 0x63, 0x69);
	lcm_dcs_write_seq_static(ctx, 0xF7, 0x0F);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
	/* HLPM Power Saving */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x46, 0xF4);
	lcm_dcs_write_seq_static(ctx, 0xF4, 0x08);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
	/* DCDC setting of AOD Sequence */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0xB1);
	lcm_dcs_write_seq_static(ctx, 0xB1, 0x05);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

	/* Brightness Control */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x0D, 0x63);
	lcm_dcs_write_seq_static(ctx, 0x63, 0x01);/* Dimming Setting */
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x0C, 0x63);
	lcm_dcs_write_seq_static(ctx, 0x63, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
	lcm_dcs_write_seq_static(ctx, 0x53, 0x28);
	lcm_dcs_write_seq_static(ctx, 0xF7, 0x0F);
	/* ACL Mode */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0x55, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
	/* OPEC Setting */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x56, 0x1F);
	lcm_dcs_write_seq_static(ctx, 0x1F, 0x01, 0x17, 0x01, 0x18, 0x06, 0x88, 0x06,
			0x89, 0x0A, 0xE2, 0x0A, 0xE3, 0x11, 0xF5, 0x1B, 0xC0);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x6A, 0x1F);
	lcm_dcs_write_seq_static(ctx, 0x1F, 0x01, 0x17, 0x03, 0x44, 0x04, 0x5A, 0x05,
			0x71, 0x06, 0x88, 0x07, 0x9E, 0x08, 0xB5, 0x09, 0xCB, 0x0A,
			0xE2, 0x0B, 0xF9, 0x0D, 0x0F, 0x0E, 0x26, 0x0F, 0x3C, 0x10, 0x53);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x52, 0x1F);
	lcm_dcs_write_seq_static(ctx, 0x1F, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x54, 0x1F);
	lcm_dcs_write_seq_static(ctx, 0x1F, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
	lcm_dcs_write_seq_static(ctx, 0x29);
	lcm_dcs_write_seq_static(ctx, 0x51, 0x03, 0xff);
	lcm_dcs_write_seq_static(ctx, 0x39, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0x39, 0xBF, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x64);
	lcm_dcs_write_seq_static(ctx, 0x39, 0xF0, 0xA5, 0xA5);
	lcm_dcs_write_seq(ctx, bl_tb0[0], bl_tb0[1]);

	pr_info("%s-\n", __func__);
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("[lh]%s +\n", __func__);
	if (ctx->prepared)
		return 0;
	ret = lcm_enable_reg_vufs18(1);

	ctx->vddr1p5_enable_gpio = devm_gpiod_get(ctx->dev, "vddr1p5-enable", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->vddr1p5_enable_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->vddr1p5_enable_gpio);

	msleep(20);

	ret = lcm_enable_reg_vmch3p0(1);

	// lcd reset H -> L -> H
	msleep(20);
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 1);
	msleep(20);
	gpiod_set_value(ctx->reset_gpio, 0);
	msleep(20);
	gpiod_set_value(ctx->reset_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	msleep(20);
	lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;

	pr_info("%s-\n", __func__);
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

#define HFP (40)
#define HSA (10)
#define HBP (20)
#define HACT (1440)
#define VFP (8)
#define VSA (8)
#define VBP (8)
#define VACT (3216)
#define PLL_CLOCK (834)

static const struct drm_display_mode default_mode = {
	.clock		= 307152, //120Hz
	.hdisplay	= HACT,
	.hsync_start	= HACT + HFP,
	.hsync_end	= HACT + HFP + HSA,
	.htotal		= HACT + HFP + HSA + HBP, //1510
	.vdisplay	= VACT,
	.vsync_start	= VACT + VFP,
	.vsync_end	= VACT + VFP + VSA,
	.vtotal		= VACT + VFP + VSA + VBP, //3240
};

#if defined(CONFIG_MTK_PANEL_EXT)
static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static int panel_ata_check(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	unsigned char data[3];
	unsigned char id[3] = {0x00, 0x80, 0x00};
	ssize_t ret;

	pr_info("%s success\n", __func__);

	ret = mipi_dsi_dcs_read(dsi, 0x4, data, 3);
	if (ret < 0)
		pr_info("%s error\n", __func__);

	DDPINFO("ATA read data %x %x %x\n", data[0], data[1], data[2]);

	if (data[0] == id[0] &&
			data[1] == id[1] &&
			data[2] == id[2])
		return 1;

	DDPINFO("ATA expect read data is %x %x %x\n",
			id[0], id[1], id[2]);

	return 1;
}

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	level = 2047;

	bl_tb0[1] = level >> 8;
	bl_tb0[2] = level & 0xFF;
	pr_info("%s bl_tb0[1] = 0x%x,bl_tb0[2]= 0x%x\n", __func__, bl_tb0[1], bl_tb0[2]);

	if (!cb)
		return -1;

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
	return 0;
}

static struct mtk_panel_params ext_params = {
	.data_rate = PLL_CLOCK * 2,
	.pll_clk = PLL_CLOCK,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
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
		.pic_height = 3216,
		.pic_width = 1440,
		.slice_height = 24,
		.slice_width = 720,
		.chunk_size = 720,
		.xmit_delay = 512,
		.dec_delay = 646,
		.scale_value = 32,
		.increment_interval = 643,
		.decrement_interval = 10,
		.line_bpg_offset = 13,
		.nfl_bpg_offset = 1158,
		.slice_bpg_offset = 814,
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

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = panel_ata_check,
	//.ext_param_set = mtk_panel_ext_param_set,
	//.ext_param_get = mtk_panel_ext_param_get,
	//.mode_switch = mode_switch,
	//.msync_te_level_switch = msync_te_level_switch,
	//.msync_cmd_set_min_fps = msync_cmd_set_min_fps,
};
#endif

static int lcm_get_modes(struct drm_panel *panel,
						struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	connector->display_info.width_mm = 67;
	connector->display_info.height_mm = 149;

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
	struct lcm *ctx;
	struct device_node *backlight;
	int ret;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;

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

	g_pmic = kzalloc(sizeof(struct lcm_pmic_info), GFP_KERNEL);

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET
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
		dev_info(dev, "%s: cannot get reset-gpios %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);

	ctx->te_switch_gpio = devm_gpiod_get(dev, "te_switch", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->te_switch_gpio)) {
		dev_info(dev, "%s: cannot get te_switch_gpio %ld\n",
			__func__, PTR_ERR(ctx->te_switch_gpio));
		return PTR_ERR(ctx->te_switch_gpio);
	}
	gpiod_set_value(ctx->te_switch_gpio, 1);
	devm_gpiod_put(dev, ctx->te_switch_gpio);


	ctx->te_out_gpio = devm_gpiod_get(dev, "te_out", GPIOD_IN);
	if (IS_ERR(ctx->te_out_gpio)) {
		dev_info(dev, "%s: cannot get te_out_gpio %ld\n",
			__func__, PTR_ERR(ctx->te_out_gpio));
		return PTR_ERR(ctx->te_out_gpio);
	}

	devm_gpiod_put(dev, ctx->te_out_gpio);
	ctx->vddr1p5_enable_gpio = devm_gpiod_get(dev, "vddr1p5-enable", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vddr1p5_enable_gpio)) {
		dev_info(dev, "%s: cannot get vddr1p5_enable_gpio %ld\n",
			__func__, PTR_ERR(ctx->vddr1p5_enable_gpio));
		return PTR_ERR(ctx->vddr1p5_enable_gpio);
	}
	devm_gpiod_put(dev, ctx->vddr1p5_enable_gpio);

	g_pmic->reg_vufs18 = regulator_get(dev, "1p8");
	if (IS_ERR(g_pmic->reg_vufs18)) {
		dev_info(dev, "%s[lh]: cannot get reg_vufs18 %ld\n",
			__func__, PTR_ERR(g_pmic->reg_vufs18));
	}
	ret = lcm_enable_reg_vufs18(1);

	g_pmic->reg_vmch3p0 = regulator_get(dev, "3p0");
	if (IS_ERR(g_pmic->reg_vmch3p0)) {
		dev_info(dev, "%s[lh]: cannot get reg_vmch3p0 %ld\n",
			__func__, PTR_ERR(g_pmic->reg_vmch3p0));
	}
	msleep(20);
	ret = lcm_enable_reg_vmch3p0(1);

#ifndef CONFIG_MTK_DISP_NO_LK
	ctx->prepared = true;
	ctx->enabled = true;
#endif

	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &lcm_drm_funcs;

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
	pr_info("%s-\n", __func__);

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
	{ .compatible = "samsung,op,cmd", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "samsung_op_cmd",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("MEDIATEK");
MODULE_DESCRIPTION("Samsung ANA6705 AMOLED CMD LCD Panel Driver");
MODULE_LICENSE("GPL v2");
