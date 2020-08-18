/*
 * Copyright (c) 2020 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <drm/drmP.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mtk_panel_ext.h"
#include "../mediatek/mtk_log.h"
#include "../mediatek/mtk_drm_graphics_base.h"
#endif

/* enable this to check panel self -bist pattern */
/* #define PANEL_BIST_PATTERN */

/* option function to read data from some panel address */
/* #define PANEL_SUPPORT_READBACK */

struct tianma {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;

	bool prepared;
	bool enabled;

	int error;
};

static char bl_tb0[] = {0x51, 0xf, 0xff};

#define tianma_dcs_write_seq(ctx, seq...)                                     \
	({                                                                     \
		const u8 d[] = {seq};                                          \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		tianma_dcs_write(ctx, d, ARRAY_SIZE(d));                      \
	})

#define tianma_dcs_write_seq_static(ctx, seq...)                              \
	({                                                                     \
		static const u8 d[] = {seq};                                   \
		tianma_dcs_write(ctx, d, ARRAY_SIZE(d));                      \
	})

static inline struct tianma *panel_to_tianma(struct drm_panel *panel)
{
	return container_of(panel, struct tianma, panel);
}

#ifdef PANEL_SUPPORT_READBACK
static int tianma_dcs_read(struct tianma *ctx, u8 cmd, void *data, size_t len)
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

static void tianma_panel_get_data(struct tianma *ctx)
{
	u8 buffer[3] = {0};
	static int ret;

	if (ret == 0) {
		ret = tianma_dcs_read(ctx, 0x0A, buffer, 1);
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}

static struct LCD_setting_table lcm_suspend_setting[] = {
	{0x28, 2, {0x28, 0x00} },
	{REGFLAG_DELAY, 20, {} },
	{0x51, 2, {0x51, 0x00} },
	{0x10, 2, {0x10, 0x00} },
	{REGFLAG_DELAY, 100, {} },
};
}
#endif

static void tianma_dcs_write(struct tianma *ctx, const void *data, size_t len)
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

static void tianma_panel_init(struct tianma *ctx)
{
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(11 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(5 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(5 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(13 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	tianma_dcs_write_seq_static(ctx, 0xFF, 0x10);
	tianma_dcs_write_seq_static(ctx, 0xFB, 0x01);
	tianma_dcs_write_seq_static(ctx, 0x3B, 0x03, 0x14, 0x2E, 0x04, 0x04);
	tianma_dcs_write_seq_static(ctx, 0xB0, 0x00);
	tianma_dcs_write_seq_static(ctx, 0xC1, 0x89, 0x28,
		0x00, 0x08, 0x00, 0xAA, 0x02, 0x0E, 0x00,
		0x2B, 0x00, 0x07, 0x0D, 0xB7, 0x0C, 0xB7);
	tianma_dcs_write_seq_static(ctx, 0xC2, 0x1B, 0xA0);

	tianma_dcs_write_seq_static(ctx, 0xE9, 0x01);
	tianma_dcs_write_seq_static(ctx, 0xFF, 0xF0);
	tianma_dcs_write_seq_static(ctx, 0xFB, 0x01);

	tianma_dcs_write_seq_static(ctx, 0x5A, 0x00);
	tianma_dcs_write_seq_static(ctx, 0x9F, 0x0F);
	tianma_dcs_write_seq_static(ctx, 0xFF, 0x10);
	tianma_dcs_write_seq_static(ctx, 0x51, 0xFF);
	tianma_dcs_write_seq_static(ctx, 0x53, 0x24);
	tianma_dcs_write_seq_static(ctx, 0x55, 0x00);

	tianma_dcs_write_seq_static(ctx, 0x29, 0x00);
	tianma_dcs_write_seq_static(ctx, 0x11, 0x00);

	msleep(20);
}

static int tianma_disable(struct drm_panel *panel)
{
	struct tianma *ctx = panel_to_tianma(panel);

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return 0;
}

static int tianma_unprepare(struct drm_panel *panel)
{
	struct tianma *ctx = panel_to_tianma(panel);

	if (!ctx->prepared)
		return 0;

	tianma_dcs_write_seq_static(ctx, MIPI_DCS_ENTER_SLEEP_MODE);
	tianma_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_OFF);
	msleep(200);

	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	ctx->error = 0;
	ctx->prepared = false;

	return 0;
}

static int tianma_prepare(struct drm_panel *panel)
{
	struct tianma *ctx = panel_to_tianma(panel);
	int ret;

	pr_info("%s\n", __func__);
	if (ctx->prepared)
		return 0;

	tianma_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		tianma_unprepare(panel);

	ctx->prepared = true;

#ifdef PANEL_SUPPORT_READBACK
	tianma_panel_get_data(ctx);
#endif

	return ret;
}

static int tianma_enable(struct drm_panel *panel)
{
	struct tianma *ctx = panel_to_tianma(panel);

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
	.clock = 414831,
	.hdisplay = 1080,
	.hsync_start = 1080 + 72,
	.hsync_end = 1080 + 72 + 4,
	.htotal = 1080 + 72 + 4 + 16,
	.vdisplay = 2408,
	.vsync_start = 2408 + 30,
	.vsync_end = 2408 + 30 + 4,
	.vtotal = 2408 + 30 + 4 + 16,
	.vrefresh = 144,
};

static const struct drm_display_mode performance_mode_90hz = {
	.clock = 414831,
	.hdisplay = 1080,
	.hsync_start = 1080 + 72,
	.hsync_end = 1080 + 72 + 4,
	.htotal = 1080 + 72 + 4 + 16,
	.vdisplay = 2408,
	.vsync_start = 2408 + 1510,
	.vsync_end = 2408 + 1510 + 4,
	.vtotal = 2408 + 1510 + 4 + 16,
	.vrefresh = 90,


};

static const struct drm_display_mode performance_mode_60hz = {
	.clock = 414831,
	.hdisplay = 1080,
	.hsync_start = 1080 + 72,
	.hsync_end = 1080 + 72 + 4,
	.htotal = 1080 + 72 + 4 + 16,
	.vdisplay = 2408,
	.vsync_start = 2408 + 3478,
	.vsync_end = 2408 + 3478 + 4,
	.vtotal = 2408 + 3478 + 4 + 16,
	.vrefresh = 60,

};

#if defined(CONFIG_MTK_PANEL_EXT)

static struct mtk_panel_params ext_params = {
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.physical_width_um = 68364,
	.physical_height_um = 152300,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
	.enable = 1,
	.ver = 17,
	.slice_mode = 1,
	.rgb_swap = 0,
	.dsc_cfg = 34,
	.rct_on = 1,
	.bit_per_channel = 8,
	.dsc_line_buf_depth = 9,
	.bp_enable = 1,
	.bit_per_pixel = 128,  //128
	.pic_height = 2408,
	.pic_width = 1080,
	.slice_height = 8,
	.slice_width = 540,
	.chunk_size = 540,
	.xmit_delay = 170,
	.dec_delay = 526,
	.scale_value = 32,
	.increment_interval = 43,
	.decrement_interval = 7,
	.line_bpg_offset = 12,
	.nfl_bpg_offset = 3511,
	.slice_bpg_offset = 3255,
	.initial_offset = 6144,
	.final_offset = 7072,
	.flatness_minqp = 3,
	.flatness_maxqp = 12,
	.rc_model_size = 8192,
	.rc_edge_factor = 6,
	.rc_quant_incr_limit0 = 11,
	.rc_quant_incr_limit1 = 11,
	.rc_tgt_offset_hi = 3,
	.rc_tgt_offset_lo = 3,
	},
	.dyn = {
		.switch_en = 1,
		.data_rate = 1140,
		.hbp = 84,
	},
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 144,
	},
	.data_rate = 1140,
};


static struct mtk_panel_params ext_params_90hz = {
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.physical_width_um = 68364,
	.physical_height_um = 152300,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
	.enable = 1,
	.ver = 17,
	.slice_mode = 1,
	.rgb_swap = 0,
	.dsc_cfg = 34,
	.rct_on = 1,
	.bit_per_channel = 8,
	.dsc_line_buf_depth = 9,
	.bp_enable = 1,
	.bit_per_pixel = 128,  //128
	.pic_height = 2408,
	.pic_width = 1080,
	.slice_height = 8,
	.slice_width = 540,
	.chunk_size = 540,
	.xmit_delay = 170,
	.dec_delay = 526,
	.scale_value = 32,
	.increment_interval = 43,
	.decrement_interval = 7,
	.line_bpg_offset = 12,
	.nfl_bpg_offset = 3511,
	.slice_bpg_offset = 3255,
	.initial_offset = 6144,
	.final_offset = 7072,
	.flatness_minqp = 3,
	.flatness_maxqp = 12,
	.rc_model_size = 8192,
	.rc_edge_factor = 6,
	.rc_quant_incr_limit0 = 11,
	.rc_quant_incr_limit1 = 11,
	.rc_tgt_offset_hi = 3,
	.rc_tgt_offset_lo = 3,
	},
	.dyn = {
		.switch_en = 1,
		.data_rate = 1140,

		.hbp = 84,
	},
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 90,
	},
	.data_rate = 1140,
};

static struct mtk_panel_params ext_params_60hz = {
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.physical_width_um = 68364,
	.physical_height_um = 152300,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
	.enable = 1,
	.ver = 17,
	.slice_mode = 1,
	.rgb_swap = 0,
	.dsc_cfg = 34,
	.rct_on = 1,
	.bit_per_channel = 8,
	.dsc_line_buf_depth = 9,
	.bp_enable = 1,
	.bit_per_pixel = 128,  //128
	.pic_height = 2408,
	.pic_width = 1080,
	.slice_height = 8,
	.slice_width = 540,
	.chunk_size = 540,
	.xmit_delay = 170,
	.dec_delay = 526,
	.scale_value = 32,
	.increment_interval = 43,
	.decrement_interval = 7,
	.line_bpg_offset = 12,
	.nfl_bpg_offset = 3511,
	.slice_bpg_offset = 3255,
	.initial_offset = 6144,
	.final_offset = 7072,
	.flatness_minqp = 3,
	.flatness_maxqp = 12,
	.rc_model_size = 8192,
	.rc_edge_factor = 6,
	.rc_quant_incr_limit0 = 11,
	.rc_quant_incr_limit1 = 11,
	.rc_tgt_offset_hi = 3,
	.rc_tgt_offset_lo = 3,
	},
	.dyn = {
		.switch_en = 1,
		.data_rate = 1140,
		.hbp = 84,
	},
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 60,
	},
	.data_rate = 1140,
};

static int tianma_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	if (level > 255)
		level = 255;

	level = level * 4095 / 255;
	bl_tb0[1] = ((level >> 8) & 0xf);
	bl_tb0[2] = (level & 0xff);

	if (!cb)
		return -1;

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));

	return 0;
}

static int mtk_panel_ext_param_set(struct drm_panel *panel,
			 unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;

	if (mode == 0)
		ext->params = &ext_params;
	else if (mode == 1)
		ext->params = &ext_params_90hz;
	else if (mode == 2)
		ext->params = &ext_params_60hz;
	else
		ret = 1;

	return ret;
}

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct tianma *ctx = panel_to_tianma(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static struct mtk_panel_funcs ext_funcs = {
	.set_backlight_cmdq = tianma_setbacklight_cmdq,
	.reset = panel_ext_reset,
	.ext_param_set = mtk_panel_ext_param_set,
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

	/**
	 * @prepare: the time (in milliseconds) that it takes for the panel to
	 *           become ready and start receiving video data
	 * @enable: the time (in milliseconds) that it takes for the panel to
	 *          display the first valid frame after starting to receive
	 *          video data
	 * @disable: the time (in milliseconds) that it takes for the panel to
	 *           turn the display off (no content is visible)
	 * @unprepare: the time (in milliseconds) that it takes for the panel
	 *             to power itself down completely
	 */
	struct {
		unsigned int prepare;
		unsigned int enable;
		unsigned int disable;
		unsigned int unprepare;
	} delay;
};

static int tianma_get_modes(struct drm_panel *panel)
{
	struct drm_display_mode *mode;
	struct drm_display_mode *mode_90hz;
	struct drm_display_mode *mode_60hz;

	mode = drm_mode_duplicate(panel->drm, &default_mode);
	if (!mode) {
		dev_err(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			default_mode.vrefresh);
		return -ENOMEM;
	}
	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(panel->connector, mode);

	mode_90hz = drm_mode_duplicate(panel->drm, &performance_mode_90hz);
	if (!mode_90hz) {
		dev_err(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
			performance_mode_90hz.hdisplay,
			performance_mode_90hz.vdisplay,
			performance_mode_90hz.vrefresh);
		return -ENOMEM;
	}
	drm_mode_set_name(mode_90hz);
	mode_90hz->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(panel->connector, mode_90hz);


	mode_60hz = drm_mode_duplicate(panel->drm, &performance_mode_60hz);
	if (!mode_60hz) {
		dev_err(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
			performance_mode_60hz.hdisplay,
			performance_mode_60hz.vdisplay,
			performance_mode_60hz.vrefresh);
		return -ENOMEM;
	}
	drm_mode_set_name(mode_60hz);
	mode_60hz->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(panel->connector, mode_60hz);

	panel->connector->display_info.width_mm = 64;
	panel->connector->display_info.height_mm = 129;

	return 3;
}

static const struct drm_panel_funcs tianma_drm_funcs = {
	.disable = tianma_disable,
	.unprepare = tianma_unprepare,
	.prepare = tianma_prepare,
	.enable = tianma_enable,
	.get_modes = tianma_get_modes,
};

static int tianma_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct tianma *ctx;
	struct device_node *backlight;
	int ret;

	pr_err("%s+\n", __func__);
	ctx = devm_kzalloc(dev, sizeof(struct tianma), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE
			 | MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET
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
		dev_err(dev, "cannot get reset-gpios %ld\n",
			PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);

	ctx->prepared = true;
	ctx->enabled = true;

	drm_panel_init(&ctx->panel);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &tianma_drm_funcs;

	ret = drm_panel_add(&ctx->panel);
	if (ret < 0)
		return ret;

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif

	pr_err("%s-\n", __func__);

	return ret;
}

static int tianma_remove(struct mipi_dsi_device *dsi)
{
	struct tianma *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id tianma_of_match[] = {
	{
		.compatible = "tianma,nt36672c,vdo,144hz",
	},
	{} };

MODULE_DEVICE_TABLE(of, tianma_of_match);

static struct mipi_dsi_driver tianma_driver = {
	.probe = tianma_probe,
	.remove = tianma_remove,
	.driver = {
		.name = "panel-tianma-nt36672c-vdo-144hz",
		.owner = THIS_MODULE,
		.of_match_table = tianma_of_match,
	},
};

module_mipi_dsi_driver(tianma_driver);

MODULE_AUTHOR("Elon Hsu <elon.hsu@mediatek.com>");
MODULE_DESCRIPTION("tianma nt36672C vdo 144hz Panel Driver");
MODULE_LICENSE("GPL v2");

