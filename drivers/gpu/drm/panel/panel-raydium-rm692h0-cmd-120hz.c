// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/backlight.h>
#include <drm/drmP.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mtk_panel_ext.h"
#include "../mediatek/mtk_log.h"
#include "../mediatek/mtk_drm_graphics_base.h"
#endif
#define WITH_DSC	1
#define HFP			(32)
#define HSA			(12)
#define HBP			(38)
#define VFP			(16)
#define VSA			(4)
#define VBP			(12)
#define VAC			(1536)
#define HAC			(2560)

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif

#define HW_EVT

#ifdef HW_EVT
#define PANEL_VCI 3300000
#else
#define PANEL_VCI 2800000
#endif

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct regulator *vddi, *vci;

	bool prepared;
	bool enabled;

	int error;
};

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

static int lcm_vddi_enable(struct lcm *ctx)
{
	int ret = 0;
	unsigned int vol = 0;

	if (!ctx->vddi) {
		dev_info(ctx->dev, "vddi connot find\n");
		return -1;
	}

	ret = regulator_set_voltage(ctx->vddi, 1800000, 1800000);
	if (ret) {
		dev_info(ctx->dev, "vddi set voltage fail\n");
		return ret;
	}

	vol = regulator_get_voltage(ctx->vddi);
	if (vol == 1800000)
		dev_info(ctx->dev, "check vol=1800000 pass!\n");
	else
		dev_info(ctx->dev, "check vol=1800000 fail!\n");

	ret = regulator_enable(ctx->vddi);
	if (ret)
		dev_info(ctx->dev, "vddi enable fail\n");

	return ret;
}

static int lcm_vddi_disable(struct lcm *ctx)
{
	int ret = 0;
	int isenable = 0;

	if (!ctx->vddi) {
		dev_info(ctx->dev, "vddi connot find\n");
		return -1;
	}

	isenable = regulator_is_enabled(ctx->vddi);
	if (isenable) {
		ret = regulator_disable(ctx->vddi);
		if (ret)
			dev_info(ctx->dev, "vddi disable fail\n");
	}

	return ret;
}

static int lcm_vci_enable(struct lcm *ctx)
{
	unsigned int vol = 0;
	int ret = 0;

	if (!ctx->vci) {
		dev_info(ctx->dev, "vci connot find\n");
		return -1;
	}

	ret = regulator_set_voltage(ctx->vci, PANEL_VCI, PANEL_VCI);

	if (ret) {
		dev_info(ctx->dev, "vci set voltage fail\n");
		return ret;
	}

	vol = regulator_get_voltage(ctx->vci);
	if (vol == PANEL_VCI)
		dev_info(ctx->dev, "check vol=%d pass!\n", PANEL_VCI);
	else
		dev_info(ctx->dev, "check vol=%d fail!\n", PANEL_VCI);

	ret = regulator_enable(ctx->vci);
	if (ret)
		dev_info(ctx->dev, "vci enable fail\n");

	return ret;
}

static int lcm_vci_disable(struct lcm *ctx)
{
	int ret = 0;
	int isenable = 0;

	if (!ctx->vci) {
		dev_info(ctx->dev, "vci connot find\n");
		return -1;
	}

	isenable = regulator_is_enabled(ctx->vci);
	if (isenable) {
		ret = regulator_disable(ctx->vci);
		if (ret)
			dev_info(ctx->dev, "vci disable fail\n");
	}

	return ret;
}

static void lcm_panel_init(struct lcm *ctx)
{
	pr_info("%s +\n", __func__);

#if WITH_DSC
	lcm_dcs_write_seq_static(ctx, 0xFE, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xFA, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xFE, 0xD2);
	lcm_dcs_write_seq_static(ctx, 0x4F, 0x08);
	lcm_dcs_write_seq_static(ctx, 0x50, 0x11);
	lcm_dcs_write_seq_static(ctx, 0x51, 0xab);
	lcm_dcs_write_seq_static(ctx, 0x52, 0x30);
	lcm_dcs_write_seq_static(ctx, 0x53, 0x06);
	lcm_dcs_write_seq_static(ctx, 0x54, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x55, 0x05);
	lcm_dcs_write_seq_static(ctx, 0x56, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x58, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x59, 0x08);
	lcm_dcs_write_seq_static(ctx, 0x5a, 0x05);
	lcm_dcs_write_seq_static(ctx, 0x5b, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x5c, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x5d, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x5e, 0x20);
	lcm_dcs_write_seq_static(ctx, 0x5f, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x60, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x61, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x62, 0x11);
	lcm_dcs_write_seq_static(ctx, 0x63, 0x0c);
	lcm_dcs_write_seq_static(ctx, 0x64, 0x0d);
	lcm_dcs_write_seq_static(ctx, 0x65, 0xb7);
	lcm_dcs_write_seq_static(ctx, 0x66, 0x05);
	lcm_dcs_write_seq_static(ctx, 0x67, 0x53);
	lcm_dcs_write_seq_static(ctx, 0x68, 0x18);
	lcm_dcs_write_seq_static(ctx, 0x69, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x6a, 0x10);
	lcm_dcs_write_seq_static(ctx, 0x6b, 0xe0);
	lcm_dcs_write_seq_static(ctx, 0x6c, 0x07);
	lcm_dcs_write_seq_static(ctx, 0x6d, 0x10);
	lcm_dcs_write_seq_static(ctx, 0x6e, 0x20);
	lcm_dcs_write_seq_static(ctx, 0x6f, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x70, 0x06);
	lcm_dcs_write_seq_static(ctx, 0x71, 0x0f);
	lcm_dcs_write_seq_static(ctx, 0x72, 0x0f);
	lcm_dcs_write_seq_static(ctx, 0x73, 0x33);
	lcm_dcs_write_seq_static(ctx, 0x74, 0x0e);
	lcm_dcs_write_seq_static(ctx, 0x75, 0x1c);
	lcm_dcs_write_seq_static(ctx, 0x76, 0x2a);
	lcm_dcs_write_seq_static(ctx, 0x77, 0x38);
	lcm_dcs_write_seq_static(ctx, 0x78, 0x46);
	lcm_dcs_write_seq_static(ctx, 0x79, 0x54);
	lcm_dcs_write_seq_static(ctx, 0x7a, 0x62);
	lcm_dcs_write_seq_static(ctx, 0x7b, 0x69);
	lcm_dcs_write_seq_static(ctx, 0x7c, 0x70);
	lcm_dcs_write_seq_static(ctx, 0x7d, 0x77);
	lcm_dcs_write_seq_static(ctx, 0x7e, 0x79);
	lcm_dcs_write_seq_static(ctx, 0x7f, 0x7b);
	lcm_dcs_write_seq_static(ctx, 0x80, 0x7d);
	lcm_dcs_write_seq_static(ctx, 0x81, 0x7e);

	lcm_dcs_write_seq_static(ctx, 0x82, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x83, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x84, 0x22);
	lcm_dcs_write_seq_static(ctx, 0x85, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x86, 0x2a);
	lcm_dcs_write_seq_static(ctx, 0x87, 0x40);
	lcm_dcs_write_seq_static(ctx, 0x88, 0x2a);
	lcm_dcs_write_seq_static(ctx, 0x89, 0xbe);
	lcm_dcs_write_seq_static(ctx, 0x8a, 0x3a);
	lcm_dcs_write_seq_static(ctx, 0x8b, 0xfc);
	lcm_dcs_write_seq_static(ctx, 0x8c, 0x3a);
	lcm_dcs_write_seq_static(ctx, 0x8d, 0xfa);
	lcm_dcs_write_seq_static(ctx, 0x8e, 0x3a);
	lcm_dcs_write_seq_static(ctx, 0x8f, 0xf8);
	lcm_dcs_write_seq_static(ctx, 0x90, 0x3b);
	lcm_dcs_write_seq_static(ctx, 0x91, 0x38);
	lcm_dcs_write_seq_static(ctx, 0x92, 0x3b);
	lcm_dcs_write_seq_static(ctx, 0x93, 0x78);
	lcm_dcs_write_seq_static(ctx, 0x94, 0x3b);
	lcm_dcs_write_seq_static(ctx, 0x95, 0xb6);
	lcm_dcs_write_seq_static(ctx, 0x96, 0x4b);
	lcm_dcs_write_seq_static(ctx, 0x97, 0xf6);
	lcm_dcs_write_seq_static(ctx, 0x98, 0x4c);
	lcm_dcs_write_seq_static(ctx, 0x99, 0x34);
	lcm_dcs_write_seq_static(ctx, 0x9a, 0x4c);
	lcm_dcs_write_seq_static(ctx, 0x9b, 0x74);
	lcm_dcs_write_seq_static(ctx, 0x9c, 0x5c);
	lcm_dcs_write_seq_static(ctx, 0x9d, 0x74);
	lcm_dcs_write_seq_static(ctx, 0x9e, 0x8c);
	lcm_dcs_write_seq_static(ctx, 0x9f, 0xf4);

	lcm_dcs_write_seq_static(ctx, 0xa2, 0x05);
	lcm_dcs_write_seq_static(ctx, 0xa3, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xa4, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xa5, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xa6, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xa7, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xa9, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xaa, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xa0, 0x80);

	lcm_dcs_write_seq_static(ctx, 0xFE, 0x40);
	lcm_dcs_write_seq_static(ctx, 0xBD, 0x00);

	lcm_dcs_write_seq_static(ctx, 0xFE, 0xD4);
	lcm_dcs_write_seq_static(ctx, 0x08, 0x04);
	lcm_dcs_write_seq_static(ctx, 0xFE, 0x94);
	lcm_dcs_write_seq_static(ctx, 0x48, 0x80);
	lcm_dcs_write_seq_static(ctx, 0xFE, 0xD4);
	lcm_dcs_write_seq_static(ctx, 0x3D, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x3F, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x94, 0x0F);
	lcm_dcs_write_seq_static(ctx, 0x97, 0x22);
	lcm_dcs_write_seq_static(ctx, 0x98, 0x22);
	lcm_dcs_write_seq_static(ctx, 0x99, 0x70);
	lcm_dcs_write_seq_static(ctx, 0x9A, 0x90);
	lcm_dcs_write_seq_static(ctx, 0x9B, 0x70);
	lcm_dcs_write_seq_static(ctx, 0x9C, 0x90);
	lcm_dcs_write_seq_static(ctx, 0xFE, 0xD6);
	lcm_dcs_write_seq_static(ctx, 0x00, 0x15);
	lcm_dcs_write_seq_static(ctx, 0x01, 0x70);
	lcm_dcs_write_seq_static(ctx, 0x02, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x03, 0x08);
	lcm_dcs_write_seq_static(ctx, 0x04, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x05, 0xE0);
	lcm_dcs_write_seq_static(ctx, 0x06, 0xFF);
	lcm_dcs_write_seq_static(ctx, 0x07, 0x00);
#else
	lcm_dcs_write_seq_static(ctx, 0xFE, 0x40);
	lcm_dcs_write_seq_static(ctx, 0xBD, 0x05);
#endif
	lcm_dcs_write_seq_static(ctx, 0xFE, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xC2, 0x08);
	lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x51, 0x07, 0xFF);
	mdelay(120);
	lcm_dcs_write_seq_static(ctx, 0x11);
	mdelay(120);
	lcm_dcs_write_seq_static(ctx, 0x29);
	mdelay(20);

	pr_info("%s -\n", __func__);
}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s +\n", __func__);

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	pr_info("%s -\n", __func__);

	return 0;
}

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s +\n", __func__);

	if (!ctx->prepared)
		return 0;

	lcm_dcs_write_seq_static(ctx, 0x28);
	mdelay(120);
	lcm_dcs_write_seq_static(ctx, 0x10);
	mdelay(120);

	ctx->error = 0;
	ctx->prepared = false;

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	lcm_vci_disable(ctx);
	mdelay(5);
	lcm_vddi_disable(ctx);
	mdelay(5);

	ctx->error = 0;
	ctx->prepared = false;

	pr_info("%s -\n", __func__);

	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s +\n", __func__);
	if (ctx->prepared)
		return 0;

	ret = lcm_vddi_enable(ctx);
	if (ret < 0) {
		dev_info(ctx->dev, "vddi enable fail\n");
		return 0;
	}
	mdelay(5);

	ret = lcm_vci_enable(ctx);
	if (ret < 0) {
		dev_info(ctx->dev, "vci enable fail\n");
		return 0;
	}

	mdelay(5);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return 0;
	}

	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(5);
	gpiod_set_value(ctx->reset_gpio, 0);
	mdelay(15);
	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(10);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0) {
		dev_info(ctx->dev, "ctx error\n");
		lcm_unprepare(panel);
	}

	ctx->prepared = true;

#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif

	pr_info("%s -\n", __func__);

	return ret;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s+\n", __func__);

	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}
	pr_info("%s-\n", __func__);

	ctx->enabled = true;

	return 0;
}

static struct drm_display_mode default_mode = {
	.clock = 272000,
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + VFP,
	.vsync_end = VAC + VFP + VSA,
	.vtotal = VAC + VFP + VSA + VBP,
#if WITH_DSC
	.vrefresh = 120,
#else
	.vrefresh = 60,
#endif
};
static const struct drm_display_mode performance_mode_1 = {
	.clock       = 270000,
	.hdisplay    = HAC,
	.hsync_start = HAC  + HFP,
	.hsync_end   = HAC  + HFP + HSA,
	.htotal      = HAC  + HFP + HSA + HBP,
	.vdisplay    = VAC,
	.vsync_start = VAC + VFP,
	.vsync_end   = VAC + VFP + VSA,
	.vtotal      = VAC + VFP + VSA + VBP,
	.vrefresh	 = 60,
};

#if defined(CONFIG_MTK_PANEL_EXT)

static struct mtk_panel_params ext_params = {
	.output_mode = MTK_PANEL_DUAL_PORT,
	.lcm_cmd_if = MTK_PANEL_DUAL_PORT,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x53,
		.count = 1,
		.para_list[0] = 0x24,
	},
#if WITH_DSC
	.dsc_params = {
		.enable = 1,
		.bdg_dsc_enable = 0,
		.ver = 0x11,
		.slice_mode = 0,
		.rgb_swap = 0,
		.dsc_cfg = 0x828,
		.rct_on = 1,
		.bit_per_channel = 10,
		.dsc_line_buf_depth = 11,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 1536,
		.pic_width = 1280,
		.slice_height = 8,
		.slice_width = 1280,
		.chunk_size = 1280,
		.xmit_delay = 512,
		.dec_delay = 897,
		.scale_value = 32,
		.increment_interval = 259,
		.decrement_interval = 17,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 3511,
		.slice_bpg_offset = 1363,
		.initial_offset = 6144,
		.final_offset = 4320,

		.flatness_minqp = 7,
		.flatness_maxqp = 16,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 15,
		.rc_quant_incr_limit1 = 15,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,

		.rc_buf_thresh[0] = 14,
		.rc_buf_thresh[1] = 28,
		.rc_buf_thresh[2] = 42,
		.rc_buf_thresh[3] = 56,
		.rc_buf_thresh[4] = 70,
		.rc_buf_thresh[5] = 84,
		.rc_buf_thresh[6] = 98,
		.rc_buf_thresh[7] = 105,
		.rc_buf_thresh[8] = 112,
		.rc_buf_thresh[9] = 119,
		.rc_buf_thresh[10] = 121,
		.rc_buf_thresh[11] = 123,
		.rc_buf_thresh[12] = 125,
		.rc_buf_thresh[13] = 126,
		.rc_range_parameters[0].range_min_qp = 0,
		.rc_range_parameters[0].range_max_qp = 8,
		.rc_range_parameters[0].range_bpg_offset = 2,
		.rc_range_parameters[1].range_min_qp = 4,
		.rc_range_parameters[1].range_max_qp = 8,
		.rc_range_parameters[1].range_bpg_offset = 0,
		.rc_range_parameters[2].range_min_qp = 5,
		.rc_range_parameters[2].range_max_qp = 9,
		.rc_range_parameters[2].range_bpg_offset = 0,
		.rc_range_parameters[3].range_min_qp = 5,
		.rc_range_parameters[3].range_max_qp = 10,
		.rc_range_parameters[3].range_bpg_offset = -2,
		.rc_range_parameters[4].range_min_qp = 7,
		.rc_range_parameters[4].range_max_qp = 11,
		.rc_range_parameters[4].range_bpg_offset = -4,
		.rc_range_parameters[5].range_min_qp = 7,
		.rc_range_parameters[5].range_max_qp = 11,
		.rc_range_parameters[5].range_bpg_offset = -6,
		.rc_range_parameters[6].range_min_qp = 7,
		.rc_range_parameters[6].range_max_qp = 11,
		.rc_range_parameters[6].range_bpg_offset = -8,
		.rc_range_parameters[7].range_min_qp = 7,
		.rc_range_parameters[7].range_max_qp = 12,
		.rc_range_parameters[7].range_bpg_offset = -8,
		.rc_range_parameters[8].range_min_qp = 7,
		.rc_range_parameters[8].range_max_qp = 13,
		.rc_range_parameters[8].range_bpg_offset = -8,
		.rc_range_parameters[9].range_min_qp = 7,
		.rc_range_parameters[9].range_max_qp = 14,
		.rc_range_parameters[9].range_bpg_offset = -10,
		.rc_range_parameters[10].range_min_qp = 9,
		.rc_range_parameters[10].range_max_qp = 14,
		.rc_range_parameters[10].range_bpg_offset = -10,
		.rc_range_parameters[11].range_min_qp = 9,
		.rc_range_parameters[11].range_max_qp = 15,
		.rc_range_parameters[11].range_bpg_offset = -12,
		.rc_range_parameters[12].range_min_qp = 9,
		.rc_range_parameters[12].range_max_qp = 15,
		.rc_range_parameters[12].range_bpg_offset = -12,
		.rc_range_parameters[13].range_min_qp = 13,
		.rc_range_parameters[13].range_max_qp = 16,
		.rc_range_parameters[13].range_bpg_offset = -12,
		.rc_range_parameters[14].range_min_qp = 16,
		.rc_range_parameters[14].range_max_qp = 17,
		.rc_range_parameters[14].range_bpg_offset = -12,
	},
	.pll_clk = 272,
#else
	.pll_clk = 400,
#endif
};

static int setbacklight_cmdq(void *dsi, dcs_write_gce cb, void *handle,
				 unsigned int level)
{
	int level_mapping = 0;
	char bl_tb0[] = {0x51, 0x07, 0xFF};

	pr_info("%s+\n", __func__);

	if (level > 255)
		level = 255;

	level_mapping = level * 0x7FF / 255;
	pr_info("%s backlight = %d, mapping to 0x%x\n", __func__, level, level_mapping);

	bl_tb0[1] = (u8)((level_mapping >> 8) & 0x7);
	bl_tb0[2] = (u8)(level_mapping & 0xFF);
	pr_info("%s tb0=0x%x,tb1=0x%x\n", __func__, bl_tb0[1], bl_tb0[2]);

	if (!cb)
		return -1;

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
	return 0;
}

struct drm_display_mode *get_mode_by_id(struct drm_panel *panel,
	unsigned int mode)
{
	struct drm_display_mode *m;
	unsigned int i = 0;

	list_for_each_entry(m, &panel->connector->modes, head) {
		if (i == mode)
			return m;
		i++;
	}
	pr_info("%s, %d, failed to get mode:%d, total:%u\n", __func__, __LINE__, mode, i);
	return NULL;
}

static void lcm_mode_switch_to_120(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	if (stage == BEFORE_DSI_POWERDOWN) {
		struct lcm *ctx = panel_to_lcm(panel);

		pr_info("%s\n", __func__);
		lcm_dcs_write_seq_static(ctx, 0xFE, 0x40);
		lcm_dcs_write_seq_static(ctx, 0xBD, 0x00);//00:120HZ,05:60HZ
		lcm_dcs_write_seq_static(ctx, 0xFE, 0x00);
	}
}

static void lcm_mode_switch_to_60(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	if (stage == BEFORE_DSI_POWERDOWN) {
		struct lcm *ctx = panel_to_lcm(panel);

		pr_info("%s\n", __func__);
		lcm_dcs_write_seq_static(ctx, 0xFE, 0x40);
		lcm_dcs_write_seq_static(ctx, 0xBD, 0x05);//00:120HZ,05:60HZ
		lcm_dcs_write_seq_static(ctx, 0xFE, 0x00);
	}
}

static int mode_switch(struct drm_panel *panel, unsigned int cur_mode,
		unsigned int dst_mode, enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(panel, dst_mode);

	if (cur_mode == dst_mode)
		return ret;

	if (m == NULL)
		return -EINVAL;

	if (m->vrefresh == 60) { /*switch to 60 */
		lcm_mode_switch_to_60(panel, stage);
	} else if (m->vrefresh == 120) { /*switch to 120 */
		lcm_mode_switch_to_120(panel, stage);
	} else
		ret = 1;

	return ret;
}

static int mtk_panel_ext_param_set(struct drm_panel *panel,
			 unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;

	pr_info("%s+:mode=%d\n", __func__, mode);
	if (mode == 0)
		ext_params.pll_clk = 272;
	else if (mode == 1)
		ext_params.pll_clk = 136;
	else
		ret = 1;

	ext->params = &ext_params;

	return ret;
}

static struct mtk_panel_funcs ext_funcs = {
	.set_backlight_cmdq = setbacklight_cmdq,
	.ext_param_set = mtk_panel_ext_param_set,
	.mode_switch = mode_switch,
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

static int lcm_get_modes(struct drm_panel *panel)
{
	struct drm_display_mode *mode;
	struct drm_display_mode *mode_2;

	pr_info("%s+\n", __func__);

	mode = drm_mode_duplicate(panel->drm, &default_mode);
	if (!mode) {
		dev_info(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			default_mode.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(panel->connector, mode);

	mode_2 = drm_mode_duplicate(panel->drm, &performance_mode_1);
	if (!mode_2) {
		dev_info(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
			performance_mode_1.hdisplay,
			performance_mode_1.vdisplay,
			performance_mode_1.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode_2);
	mode_2->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(panel->connector, mode_2);
	panel->connector->display_info.width_mm = 129;
	panel->connector->display_info.height_mm = 64;

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
	struct device_node *backlight;
	struct lcm *ctx;
	int ret;

	pr_info("%s+\n", __func__);

	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags =
			 MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET
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

	ctx->vddi = devm_regulator_get(dev, "reg-vddi");
	if (IS_ERR(ctx->vddi)) {
		dev_info(dev, "%s: cannot get vddi %ld\n", PTR_ERR(ctx->vddi));
		return PTR_ERR(ctx->vddi);
	}

	ctx->vci = devm_regulator_get(dev, "reg-vci");
	if (IS_ERR(ctx->vci)) {
		dev_info(dev, "%s: cannot get vci %ld\n", PTR_ERR(ctx->vci));
		return PTR_ERR(ctx->vci);
	}

	ctx->prepared = true;
	ctx->enabled = true;

	drm_panel_init(&ctx->panel);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &lcm_drm_funcs;

	ret = drm_panel_add(&ctx->panel);
	if (ret < 0) {
		dev_info(dev, "drm_panel_add fail\n");
		return ret;
	}

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_info(dev, "mipi_dsi_attach fail\n");
		drm_panel_remove(&ctx->panel);
	}

#if defined(CONFIG_MTK_PANEL_EXT)
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0) {
		dev_info(dev, "mtk_panel_ext_create fail\n");
		return ret;
	}
#endif
	pr_info("%s-\n", __func__);

	return ret;
}

static int lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id lcm_of_match[] = {
	{ .compatible = "edo,rm692h0,cmd", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-rm690h2-cmd",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("Henry Tu <Henry.tu@mediatek.com>");
MODULE_DESCRIPTION("rm690h2 CMD LCD Panel Driver");
MODULE_LICENSE("GPL v2");

