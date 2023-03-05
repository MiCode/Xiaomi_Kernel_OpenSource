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
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_log.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mediatek_v2/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif

#define _DSC_ENABLE_      0

#if _DSC_ENABLE_
#define pll_lcm0 (260) //panel 1080*2400
#else
#define pll_lcm0 (260) //panel 1080*2400
#endif
#define pll_lcm1 (423) ////panel 2250*2088

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *avdden_gpio;
	struct gpio_desc *bias_pos, *bias_neg;
	struct regulator *vddio_1v8;                      /*power vddio 1.8v*/

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

#define REGFLAG_CMD				0xFFFA
#define REGFLAG_DELAY			0xFFFC
#define REGFLAG_UDELAY			0xFFFB
#define REGFLAG_END_OF_TABLE	0xFFFD
static int esd_brightness;
static unsigned int last_backlight;
//extern unsigned int dsi1_id3_val;

struct LCM_setting_table {
	unsigned int cmd;
	unsigned int count;
	unsigned char para_list[256];
};

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
	ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
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

static void transsion_lcm0_panel_init(struct lcm *ctx)
{
	pr_info("%s+\n", __func__);

	usleep_range(2000, 2100);
	lcm_dcs_write_seq_static(ctx, 0xFE, 0x20);
	lcm_dcs_write_seq_static(ctx, 0x0C, 0x84);
	lcm_dcs_write_seq_static(ctx, 0xFE, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x53, 0x20);
	lcm_dcs_write_seq_static(ctx, 0x2A, 0x00, 0x08, 0x01, 0x85);
	lcm_dcs_write_seq_static(ctx, 0x2B, 0x00, 0x00, 0x02, 0xCF);

	lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x51, 0xFF);

	lcm_dcs_write_seq_static(ctx, 0x11);

	usleep_range(120000, 120100);
	lcm_dcs_write_seq_static(ctx, 0x29);	//display on
	lcm_dcs_write_seq_static(ctx, 0xFE, 0x10);
	usleep_range(50000, 50100);
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

static int lcm_panel_power_disable(struct lcm *ctx)
{
	int ret = 0;

	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(2000, 2100);
	gpiod_set_value(ctx->avdden_gpio, 0);
	usleep_range(25000, 25100);

	regulator_disable(ctx->vddio_1v8);

	usleep_range(42000, 42100);
	return ret;
}

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->prepared)
		return 0;

	pr_info("%s, panel_0 unprepare\n", __func__);
	lcm_dcs_write_seq_static(ctx, 0xFE, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x28);
	//usleep_range(20000, 20100);
	lcm_dcs_write_seq_static(ctx, 0x10);
	lcm_dcs_write_seq_static(ctx, 0xFE, 0x10);
	usleep_range(83000, 83100);

	ctx->prepared = false;
	ctx->error = 0;

	lcm_panel_power_disable(ctx);

	return 0;
}

static int lcm_panel_power_enable(struct lcm *ctx)
{
	int ret = 0;

	regulator_set_voltage(ctx->vddio_1v8, 1800000, 1800000);
	regulator_set_load(ctx->vddio_1v8, 200000);
	ret = regulator_enable(ctx->vddio_1v8);

	usleep_range(5000, 5100);
	gpiod_set_value(ctx->avdden_gpio, 1);
	usleep_range(12000, 12100);

	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(21000, 21100);

	return ret;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret = 0;

	if (ctx->prepared)
		return 0;
	pr_info("bf130 ctx->prepared=%d\n", ctx->prepared);

	lcm_panel_power_enable(ctx);
	transsion_lcm0_panel_init(ctx);
	usleep_range(2000, 2100);
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

#define HFP (40)
#define HSA (2)
#define HBP (20)
#define VFP (22)
#define VSA (2)
#define VBP (24)

#define VAC_FHD (720)
#define HAC_FHD (382)

static struct drm_display_mode default_mode = {
	.clock		= 20460, //60Hz
	.hdisplay	= HAC_FHD,
	.hsync_start	= HAC_FHD + HFP,
	.hsync_end	= HAC_FHD + HFP + HSA,
	.htotal		= HAC_FHD + HFP + HSA + HBP,
	.vdisplay	= VAC_FHD,
	.vsync_start	= VAC_FHD + VFP,
	.vsync_end	= VAC_FHD + VFP + VSA,
	.vtotal		= VAC_FHD + VFP + VSA + VBP,
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

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb, void *handle, unsigned int level)
{
	unsigned char bl_page1[] = {0xFE, 0x00};
	unsigned char bl_level[] = {0x51, 0xFF};
	unsigned char hbm_cmd[] = {0x66, 0x00};
	unsigned char bl_page2[] = {0xFE, 0x10};

	static bool hbm_state;

	pr_debug("[LCM][DEBUG][%s:%d]start\n", __func__, __LINE__);

	if (!dsi || !cb)
		return -EINVAL;

	esd_brightness = level;
	last_backlight = level;

	if (level == 1 || level == 2 || level == 3) {
		pr_info("[%s:%d]filter backlight %u setting\n",
				__func__, __LINE__, level);
		return 0;
	}

	/* min DBV level is 8; max DBV is 287 */
	if (level > 1 && level <= 8)
		level = 8;
	else if (level > 287)
		level = 287;

	if (level > 255 && level <= 287) {
		level = level - 32;
		hbm_cmd[1] = 0x02;
		if (!hbm_state) {
			cb(dsi, handle, bl_page1, ARRAY_SIZE(bl_page1));
			cb(dsi, handle, hbm_cmd, ARRAY_SIZE(hbm_cmd));
			cb(dsi, handle, bl_page2, ARRAY_SIZE(bl_page2));
		}
		hbm_state = true;
	} else {
		hbm_cmd[1] = 0x00;
		if (hbm_state) {
			cb(dsi, handle, bl_page1, ARRAY_SIZE(bl_page1));
			cb(dsi, handle, hbm_cmd, ARRAY_SIZE(hbm_cmd));
			cb(dsi, handle, bl_page2, ARRAY_SIZE(bl_page2));
		}
		hbm_state = false;
	}

	bl_level[1] = level & 0xFF;
	cb(dsi, handle, bl_page1, ARRAY_SIZE(bl_page1));
	cb(dsi, handle, bl_level, ARRAY_SIZE(bl_level));
	cb(dsi, handle, bl_page2, ARRAY_SIZE(bl_page2));
	pr_debug("[LCM][DEBUG][%s:%d]end\n", __func__, __LINE__);

	return 0;
}


static struct mtk_panel_params ext_params = {
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
			.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
		},
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
#if _DSC_ENABLE_
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable                =  0,
		.ver                   =  17,
		.slice_mode            =  1,
		.rgb_swap              =  0,
		.dsc_cfg               =  34,
		.rct_on                =  1,
		.bit_per_channel       =  8,
		.dsc_line_buf_depth    =  9,
		.bp_enable             =  1,
		.bit_per_pixel         =  128,
		.pic_height            =  2400,
		.pic_width             =  1080,
		.slice_height          =  40,
		.slice_width           =  540,
		.chunk_size            =  540,
		.xmit_delay            =  512,
		.dec_delay             =  526,//796,
		.scale_value           =  32,
		.increment_interval    =  989,//1325,
		.decrement_interval    =  7,//15,
		.line_bpg_offset       =  12,
		.nfl_bpg_offset        =  631,
		.slice_bpg_offset      =  651,//326,
		.initial_offset        =  6144,
		.final_offset          =  4336,
		.flatness_minqp        =  3,
		.flatness_maxqp        =  12,
		.rc_model_size         =  8192,
		.rc_edge_factor        =  6,
		.rc_quant_incr_limit0  =  11,
		.rc_quant_incr_limit1  =  11,
		.rc_tgt_offset_hi      =  3,
		.rc_tgt_offset_lo      =  3,
	},
#endif
	.dyn_fps = {
		.switch_en = 0,
	},
	.data_rate = 512,
	.pll_clk = 256,
	.phy_timcon = {
		.hs_trail = 10,
		.clk_trail = 11,
	},
};

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = panel_ata_check,
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

static int lcm_get_modes(struct drm_panel *panel,
			struct drm_connector *connector)
{
	struct drm_display_mode *mode0;

	mode0 = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode0) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode0);
	mode0->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode0);

	connector->display_info.width_mm = 39;
	connector->display_info.height_mm = 73;

	return 2;
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

	pr_info("panel-boe-bf130-dphy-cmd-hd %s-\n", __func__);

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

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 1;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_LPM | MIPI_DSI_CLOCK_NON_CONTINUOUS;

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
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(2000, 2100);
	devm_gpiod_put(dev, ctx->reset_gpio);


	ctx->vddio_1v8 = devm_regulator_get_optional(ctx->dev, "vddio-1v8");
	if (IS_ERR_OR_NULL(ctx->vddio_1v8))
		pr_info("Regulator get failed vddio_1v8\n");
	else {
		ret = regulator_set_voltage(ctx->vddio_1v8, 1800000, 1800000);
		if (ret) {
			pr_info("Regulator set_vtg failed vddio_i2c rc = %d\n", ret);
			return -EPROBE_DEFER;
		}

		ret = regulator_set_load(ctx->vddio_1v8, 200000);
		if (ret < 0) {
			pr_info("Failed to set vddio_1v8 mode(rc:%d)\n", ret);
			return -EPROBE_DEFER;
		}
		ret = regulator_enable(ctx->vddio_1v8);
		if (ret) {
			pr_info("Regulator vddi_i2c enable failed ret = %d\n", ret);
			return -EPROBE_DEFER;
		}
	}
	usleep_range(2000, 2100);

	ctx->avdden_gpio = devm_gpiod_get(dev, "avdden", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->avdden_gpio)) {
		pr_info("%s: cannot get avdden-gpios %ld\n",
			__func__, PTR_ERR(ctx->avdden_gpio));
		return PTR_ERR(ctx->avdden_gpio);
	}
	gpiod_set_value(ctx->avdden_gpio, 1);
	usleep_range(2000, 2100);

#ifndef CONFIG_MTK_DISP_NO_LK
	ctx->prepared = true;
	ctx->enabled = true;
#endif

	if (of_property_read_bool(dsi_node, "init-panel-off")) {
		ctx->prepared = false;
		ctx->enabled = false;
		pr_info("flip dsi_node:%s set prepare = enable = false\n",
								dsi_node->full_name);
	}
	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &lcm_drm_funcs;

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		drm_panel_remove(&ctx->panel);
		pr_info("%s, L-%d\n", __func__, __LINE__);
	}

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif

	pr_info("%s+\n", __func__);
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
	{ .compatible = "boe,bf130,dphy,cmd,hd", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-boe-bf130-dphy-cmd-hd",
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
MODULE_DESCRIPTION("BOE BF130 Panel Driver");
MODULE_LICENSE("GPL v2");
