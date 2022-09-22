// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/backlight.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <drm/drm_modes.h>
#include <drm/drm_dsc.h>
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
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif

//static atomic_t current_backlight;
#define LCM_USED_2PORT_4SLICE 0

enum CMDID_TYPE {
	TYPE_GENERIC,
	TYPE_DCS,
	TYPE_PPS
};

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *mt6373_io11;
	struct gpio_desc *switch_en;
	struct gpio_desc *tp_1v8_en;
	bool prepared;
	bool enabled;

	int error;
};

static inline struct lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct lcm, panel);
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
		dev_info(ctx->dev, "error %d reading dcs seq:(%#x)\n", ret,
			 cmd);
		ctx->error = ret;
	}

	return ret;
}

static void lcm_panel_get_data(struct lcm *ctx)
{
	u8 buffer[3] = { 0 };
	static int ret;

	pr_info("%s+\n", __func__);

	if (ret == 0) {
		ret = lcm_dcs_read(ctx, 0x0A, buffer, 1);
		pr_info("%s  0x%08x\n", __func__, buffer[0] | (buffer[1] << 8));
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

static void lcm_dsi_write(struct lcm *ctx, enum CMDID_TYPE type, const void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return;

	switch (type) {
	case TYPE_DCS:
		ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
		break;
	case TYPE_GENERIC:
		ret = mipi_dsi_generic_write(dsi, data, len);
		break;
	case TYPE_PPS:
		ret = mipi_dsi_picture_parameter_set(dsi,
				(struct drm_dsc_picture_parameter_set *)data);
		break;
	default:
		dev_info(ctx->dev, "%s: donnot support this cmd(%d)!\n", __func__, type);
		break;
	}

	if (ret < 0) {
		dev_info(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
}

static void lcm_panel_init(struct lcm *ctx)
{
	unsigned char sleep_out[] = {0x11};
	unsigned char disp_on[] = {0x29};
	unsigned char pre_bl[] = {0x80, 0x40, 0x00, 0x80, 0xE7, 0x70, 0x00, 0x00, 0x00};
#if LCM_USED_2PORT_4SLICE
	int i = 0;
	unsigned char dsc_4slice[6][9] = {
		{0x80, 0x0c, 0x00, 0x81, 0xe7, 0x02, 0xEE, 0x02, 0xEE},
		{0x80, 0x10, 0x00, 0x81, 0xe7, 0x02, 0x00, 0x01, 0xBC},
		{0x80, 0x14, 0x00, 0x81, 0xe7, 0x00, 0x20, 0x00, 0x90},
		{0x80, 0x18, 0x00, 0x81, 0xe7, 0x00, 0x0A, 0x00, 0x06},
		{0x80, 0x1c, 0x00, 0x81, 0xe7, 0x10, 0x00, 0x12, 0x4E},
		{0x80, 0xe8, 0x00, 0x81, 0xe7, 0xE4, 0xEE, 0x00, 0x00}};
	unsigned char pre_pps[] = {0x80, 0x90, 0x00, 0x81, 0xe7, 0x03, 0x00, 0x00, 0x00};
	unsigned char pps[] = {0x11, 0x00, 0x00, 0xAB, 0x30, 0x80, 0x07, 0x54,
		0x0B, 0xB8, 0x00, 0x14, 0x02, 0xEE, 0x02, 0xEE,
		0x02, 0x00, 0x01, 0xBC, 0x00, 0x20, 0x00, 0x90,
		0x00, 0x0A, 0x00, 0x06, 0x10, 0x00, 0x12, 0x4E,
		0x18, 0x00, 0x10, 0xF0, 0x07, 0x10, 0x20, 0x00,
		0x06, 0x0F, 0x0F, 0x33, 0x0E, 0x1C, 0x2A, 0x38,
		0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7B,
		0x7D, 0x7E, 0x02, 0x02, 0x22, 0x00, 0x2A, 0x40,
		0x2A, 0xBE, 0x3A, 0xFC, 0x3A, 0xFA, 0x3A, 0xF8,
		0x3B, 0x38, 0x3B, 0x78, 0x3B, 0xB6, 0x4B, 0xB6,
		0x4B, 0xF4, 0x4B, 0xF4, 0x6C, 0x34, 0x84, 0x74,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
#endif
	pr_info("%s++++\n", __func__);

	lcm_dsi_write(ctx, TYPE_GENERIC, pre_bl,
			sizeof(pre_bl)/sizeof(unsigned char)); //backlight pre setting
#if LCM_USED_2PORT_4SLICE
	lcm_dsi_write(ctx, TYPE_GENERIC, pre_pps,
			sizeof(pre_pps)/sizeof(unsigned char)); //pps pre setting
	lcm_dsi_write(ctx, TYPE_PPS, pps,
			sizeof(pps)/sizeof(unsigned char)); //pps pre setting
	msleep(20);
	for (i = 0; i < 6; i++)
		lcm_dsi_write(ctx, TYPE_GENERIC, dsc_4slice[i],
			sizeof(dsc_4slice[i])/sizeof(unsigned char));
	msleep(20);
#endif
	/* Sleep Out */
	lcm_dsi_write(ctx, TYPE_DCS, sleep_out, 1);
	msleep(120);
	/* Display On */
	//himax need move display_on to bl function,
	lcm_dsi_write(ctx, TYPE_DCS, disp_on, 1);

	pr_info("%s----\n", __func__);
}

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	unsigned char sleep_in[] = {0x10};
	unsigned char disp_off[] = {0x28};

	pr_info("%s+\n", __func__);

	if (!ctx->prepared)
		return 0;

	lcm_dsi_write(ctx, TYPE_DCS, disp_off, 1);
	msleep(20);
	lcm_dsi_write(ctx, TYPE_DCS, sleep_in, 1);
	msleep(120);
	gpiod_direction_output(ctx->tp_1v8_en, 0);//1.8v
	msleep(20);
	gpiod_direction_output(ctx->mt6373_io11, 0);//3.3v
	ctx->error = 0;
	ctx->prepared = false;

	pr_info("%s-\n", __func__);

	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s+\n", __func__);
	if (ctx->prepared)
		return 0;

	gpiod_direction_output(ctx->switch_en, 0);
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_direction_output(ctx->tp_1v8_en, 1);//1.8v
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_direction_output(ctx->mt6373_io11, 1);//3.3v

	gpiod_direction_output(ctx->reset_gpio, 1);
	usleep_range(5 * 1000, 5 * 1000);
	gpiod_direction_output(ctx->reset_gpio, 0);
	usleep_range(15 * 1000, 15 * 1000);
	gpiod_direction_output(ctx->reset_gpio, 1);
	usleep_range(60 * 1000, 60 * 1000);

	lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;
#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif

	pr_info("%s-\n", __func__);
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

	ctx->enabled = true;

	pr_info("%s-\n", __func__);

	return 0;
}

#define HFP (236)
#define HSA (4)
#define HBP 128
#define HACT (3000)
#define VFP (23)
#define VSA 1
#define VBP (8)
#define VACT (1876)
#define PLL_CLOCK (450)

static const struct drm_display_mode default_mode = {
	.clock			= 771138,
	.hdisplay		= HACT,
	.hsync_start	= HACT + HFP,
	.hsync_end		= HACT + HFP + HSA,
	.htotal			= HACT + HFP + HSA + HBP,
	.vdisplay		= VACT,
	.vsync_start	= VACT + VFP,
	.vsync_end		= VACT + VFP + VSA,
	.vtotal			= VACT + VFP + VSA + VBP,
};

#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params = {
	.pll_clk = PLL_CLOCK,
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.ssc_enable = 0,
	.output_mode = MTK_PANEL_DUAL_PORT,
	.lcm_cmd_if = MTK_PANEL_DUAL_PORT,
	.lp_perline_en = 1,
	.set_area_before_trigger = 1,
	.dsc_params = {
		.enable = 1,
		.ver = 1,
#if LCM_USED_2PORT_4SLICE
		.dual_dsc_enable = 1,
		.slice_mode = 1,
#else
		.dual_dsc_enable = 0,
		.slice_mode = 0,
#endif
		.rgb_swap = 0,
#if LCM_USED_2PORT_4SLICE
		.dsc_cfg = 0x828,
#else
		.dsc_cfg = 0x808,
#endif
		.rct_on = 1,
		.bit_per_channel = 10,
		.dsc_line_buf_depth = 11,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 1876,
		.pic_width = 1500,
#if LCM_USED_2PORT_4SLICE
		.slice_height = 20,
		.slice_width = 750,
		.chunk_size = 750,
#else
		.slice_height = 4,
		.slice_width = 1500,
		.chunk_size = 1500,
#endif
		.xmit_delay = 512,
#if LCM_USED_2PORT_4SLICE
		.dec_delay = 444,
#else
		.dec_delay = 512,
#endif
		.scale_value = 32,
#if LCM_USED_2PORT_4SLICE
		.increment_interval = 144,
		.decrement_interval = 10,
#else
		.increment_interval = 197,
		.decrement_interval = 20,
#endif
		.line_bpg_offset = 6,
		.nfl_bpg_offset = 4096,
#if LCM_USED_2PORT_4SLICE
		.slice_bpg_offset = 4686,
#else
		.slice_bpg_offset = 2343,
#endif
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
};

static int panel_ata_check(struct drm_panel *panel)
{
	/* Customer test by own ATA tool */
	return 1;
}

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb, void *handle,
				 unsigned int level)
{
	int level_mapping = 0;
	char bl_tb0[] = {0x51, 0x1f, 0xFF};

	pr_info("%s+ level=%d\n", __func__, level);
	if (!cb)
		return -1;

	if (level > 1023)
		level = 1023;

	level_mapping = level * 0x1FFF / 1023;
	pr_info("%s backlight = %d, mapping to 0x%x\n", __func__, level, level_mapping);

	bl_tb0[1] = (u8)((level_mapping >> 8) & 0x1F);
	bl_tb0[2] = (u8)(level_mapping & 0xFF);
	pr_info("%s tb0=0x%x,tb1=0x%x\n", __func__, bl_tb0[1], bl_tb0[2]);

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));

	return 0;
}

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

	gpiod_set_value(ctx->reset_gpio, on);

	return 0;
}

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

static int mtk_panel_ext_param_set(struct drm_panel *panel,
			struct drm_connector *connector, unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(connector, mode);

	pr_info("%s+\n", __func__);
	if (!m) {
		pr_info("%s:%d invalid display_mode\n", __func__, __LINE__);
		return ret;
	}
	if (drm_mode_vrefresh(m) == 120)
		ext->params = &ext_params;
//	else if (drm_mode_vrefresh(m) == 60)
//		ext->params = &ext_params_60hz;
	else
		ret = 1;

	pr_info("%s-\n", __func__);

	return ret;
}

static int lcm_ext_cmd_set(void *dsi, dcs_write_gce cb,
		void *handle)
{
	unsigned char update_x[] = {0x2a, 0x00, 0x00, 0x05, 0xdb};
	unsigned char update_y[] = {0x2b, 0x00, 0x00, 0x07, 0x53};

	cb(dsi, handle, update_x, ARRAY_SIZE(update_x));
	cb(dsi, handle, update_y, ARRAY_SIZE(update_y));
	return 0;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = panel_ata_check,
	.ext_param_set = mtk_panel_ext_param_set,
	.ext_cmd_set = lcm_ext_cmd_set,
};
#endif

static int lcm_get_modes(struct drm_panel *panel,
					struct drm_connector *connector)
{
	struct drm_display_mode *mode_1;

	pr_info("%s+\n", __func__);

	mode_1 = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode_1) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode_1);
	mode_1->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_1);

	connector->display_info.width_mm = 64;
	connector->display_info.height_mm = 129;

	pr_info("%s-\n", __func__);

	return 1;
}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s+\n", __func__);

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	pr_info("%s-\n", __func__);

	return 0;
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

	pr_info("%s+\n", __func__);

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
		dev_info(dev, "cannot get reset-gpios %ld\n",
			 PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	ctx->switch_en = devm_gpiod_get(dev, "switch_en", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->switch_en)) {
		dev_info(dev, "cannot get switch_en-gpios %ld\n",
			 PTR_ERR(ctx->switch_en));
		return PTR_ERR(ctx->switch_en);
	}
	ctx->tp_1v8_en = devm_gpiod_get(dev, "tp_1v8_en", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->tp_1v8_en)) {
		dev_info(dev, "cannot get tp_1v8_en-gpios %ld\n",
			 PTR_ERR(ctx->tp_1v8_en));
		return PTR_ERR(ctx->tp_1v8_en);
	}
	ctx->mt6373_io11 = devm_gpiod_get_optional(dev,
						"mt6373_io11", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->mt6373_io11)) {
		dev_info(dev, "cannot get mt6373_io11-gpios %ld\n",
			 PTR_ERR(ctx->mt6373_io11));
		return -EPROBE_DEFER;
	}

	ctx->prepared = true;
	ctx->enabled = true;
	ctx->panel.dev = dev;
	ctx->panel.funcs = &lcm_drm_funcs;
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

	pr_info("%s- lcm,nt37701,cmd,120hz\n", __func__);

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
	    .compatible = "edo,hx5329,cmd,120hz",
	},
	{}
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-edo-hx5329-cmd-120hz",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("MEDIATEK");
MODULE_DESCRIPTION("edo hx5329 AMOLED CMD SPR LCD Panel Driver");

MODULE_LICENSE("GPL v2");
