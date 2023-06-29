/*
 * Copyright (c) 2015 MediaTek Inc.
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

#include <drm/drmP.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <linux/backlight.h>
#include <linux/delay.h>

#include <linux/gpio/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mtk_drm_graphics_base.h"
#include "../mediatek/mtk_log.h"
#include "../mediatek/mtk_panel_ext.h"
#endif


struct virtual {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bias_gpio;

	bool prepared;
	bool enabled;

	int error;
};

#define virtual_dcs_write_seq(ctx, seq...)                                         \
	({                                                                     \
		const u8 d[] = { seq };                                        \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		virtual_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

#define virtual_dcs_write_seq_static(ctx, seq...)                                  \
	({                                                                     \
		static const u8 d[] = { seq };                                 \
		virtual_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

static inline struct virtual *panel_to_virtual(struct drm_panel *panel)
{
	return container_of(panel, struct virtual, panel);
}

static void virtual_dcs_write(struct virtual *ctx, const void *data, size_t len)
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

static void virtual_panel_init(struct virtual *ctx)
{
	pr_info("%s+\n", __func__);
	virtual_dcs_write_seq_static(ctx, 0x11);
	msleep(180);
	virtual_dcs_write_seq_static(ctx, 0x29);
	msleep(100);
}

static int virtual_disable(struct drm_panel *panel)
{
	struct virtual *ctx = panel_to_virtual(panel);

	pr_info("%s++\n", __func__);
	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return 0;
}

static int virtual_unprepare(struct drm_panel *panel)
{
	struct virtual *ctx = panel_to_virtual(panel);
	pr_info("%s++\n", __func__);
	if (!ctx->prepared)
		return 0;
	virtual_dcs_write_seq_static(ctx, 0x28);
	msleep(50);
	virtual_dcs_write_seq_static(ctx, 0x10);
	msleep(150);

	ctx->error = 0;
	ctx->prepared = false;

	return 0;
}

static int virtual_prepare(struct drm_panel *panel)
{
	struct virtual *ctx = panel_to_virtual(panel);
	int ret;
	pr_info("%s+\n", __func__);
	if (!ctx->prepared)
		return 0;
	virtual_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		virtual_unprepare(panel);
	ctx->prepared = true;

	return ret;
}

static int virtual_enable(struct drm_panel *panel)
{
	struct virtual *ctx = panel_to_virtual(panel);
	pr_info("%s++\n", __func__);
	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;

	return 0;
}

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

#define HFP (36)
#define HSA (20)
#define HBP (36)
#define VFP (20)
#define VSA (2)
#define VBP (10)
#define VAC (2400)
#define HAC (1080)

static const struct drm_display_mode default_mode = {
	.clock = 171019,
	.hdisplay = 1080,
	.hsync_start = 1080 + HFP,
	.hsync_end = 1080 + HFP + HSA,
	.htotal = 1080 + HFP + HSA + HBP,
	.vdisplay = 2400,
	.vsync_start = 2400 + VFP,
	.vsync_end = 2400 + VFP + VSA,
	.vtotal = 2400 + VFP + VSA + VBP,
	.vrefresh = 60,
};

#if defined(CONFIG_MTK_PANEL_EXT)
static int panel_ext_reset(struct drm_panel *panel, int on)
{
	return 0;
}

static int panel_ata_check(struct drm_panel *panel)
{
	return 0;
}

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb, void *handle,
		unsigned int level)
{
	return 0;
}

static int lcm_get_virtual_heigh(void)
{
	return VAC;
}

static int lcm_get_virtual_width(void)
{
	return HAC;
}

static struct mtk_panel_params ext_params = {
	.pll_clk = 550,
	.data_rate = 1100,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
};

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = panel_ata_check,
	.get_virtual_heigh = lcm_get_virtual_heigh,
	.get_virtual_width = lcm_get_virtual_width,
};
#endif

static int virtual_get_modes(struct drm_panel *panel)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(panel->drm, &default_mode);
	if (!mode) {
		pr_notice("failed to add mode %ux%ux@%u\n",
				default_mode.hdisplay, default_mode.vdisplay,
				default_mode.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(panel->connector, mode);

	panel->connector->display_info.width_mm = 70;
	panel->connector->display_info.height_mm = 152;

	return 1;
}

static const struct drm_panel_funcs virtual_drm_funcs = {
	.disable = virtual_disable,
	.unprepare = virtual_unprepare,
	.prepare = virtual_prepare,
	.enable = virtual_enable,
	.get_modes = virtual_get_modes,
};

static int virtual_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct virtual *ctx;
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
		pr_info("%s+ skip probe due to not current lcm %s\n", __func__, dev->of_node);
		return -ENODEV;
	}

	pr_info("%s+\n", __func__);
	ctx = devm_kzalloc(dev, sizeof(struct virtual), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
		MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET |
		MIPI_DSI_CLOCK_NON_CONTINUOUS;

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(dev, "cannot get reset-gpios %ld\n",
				PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);

	ctx->bias_gpio = devm_gpiod_get(ctx->dev, "bias", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->bias_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_gpio);

	ctx->prepared = true;
	ctx->enabled = true;

	drm_panel_init(&ctx->panel);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &virtual_drm_funcs;

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

	pr_info("%s- wt,virtual panel\n", __func__);

	return ret;
}

static int virtual_remove(struct mipi_dsi_device *dsi)
{
	struct virtual *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id virtual_of_match[] = {
	{
		.compatible = "wt,virtual_dsi_vdo_default",
	},
	{}
};

MODULE_DEVICE_TABLE(of, virtual_of_match);

static struct mipi_dsi_driver virtual_driver = {
	.probe = virtual_probe,
	.remove = virtual_remove,
	.driver = {
		.name = "virtual_dsi_vdo_default",
		.owner = THIS_MODULE,
		.of_match_table = virtual_of_match,
	},
};

module_mipi_dsi_driver(virtual_driver);

MODULE_AUTHOR("samir.liu <samir.liu@mediatek.com>");
MODULE_DESCRIPTION("virtual_dsi_vdo_default Panel Driver");
MODULE_LICENSE("GPL v2");
