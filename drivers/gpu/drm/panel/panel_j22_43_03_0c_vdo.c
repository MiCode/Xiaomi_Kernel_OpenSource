/*
 * Copyright (c) 2015 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
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
#include <drm/drmP.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mtk_panel_ext.h"
#include "../mediatek/mtk_log.h"
#include "../mediatek/mtk_drm_graphics_base.h"
#endif

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek_cannon/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif

#include <linux/double_click.h>

static const char *drm_dsi_ext_panel_name = "panel_j22_43_03_0c_vdo";

#ifdef CONFIG_BACKLIGHT_SUPPORT_LM36273
extern int lm36273_bl_bias_conf(void);
extern int lm36273_bias_enable(int enable, int delayMs);
#endif

extern int get_panel_dead_flag(void);

#define HSA 18
#define HBP 22
#define HFP 114

#define VSA 4
#define VBP 28
#define VFP 120

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;

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
		dev_err(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
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
		dev_err(ctx->dev, "error %d reading dcs seq:(%#x)\n", ret, cmd);
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

#if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
static struct regulator *disp_bias_pos;
static struct regulator *disp_bias_neg;


static int lcm_panel_bias_regulator_init(void)
{
	static int regulator_inited;
	int ret = 0;

	if (regulator_inited)
		return ret;

	/* please only get regulator once in a driver */
	disp_bias_pos = regulator_get(NULL, "dsv_pos");
	if (IS_ERR(disp_bias_pos)) { /* handle return value */
		ret = PTR_ERR(disp_bias_pos);
		pr_err("get dsv_pos fail, error: %d\n", ret);
		return ret;
	}

	disp_bias_neg = regulator_get(NULL, "dsv_neg");
	if (IS_ERR(disp_bias_neg)) { /* handle return value */
		ret = PTR_ERR(disp_bias_neg);
		pr_err("get dsv_neg fail, error: %d\n", ret);
		return ret;
	}

	regulator_inited = 1;
	return ret; /* must be 0 */

}

static int lcm_panel_bias_enable(void)
{
	int ret = 0;
	int retval = 0;

	lcm_panel_bias_regulator_init();

	/* set voltage with min & max*/
	ret = regulator_set_voltage(disp_bias_pos, 5400000, 5400000);
	if (ret < 0)
		pr_err("set voltage disp_bias_pos fail, ret = %d\n", ret);
	retval |= ret;

	ret = regulator_set_voltage(disp_bias_neg, 5400000, 5400000);
	if (ret < 0)
		pr_err("set voltage disp_bias_neg fail, ret = %d\n", ret);
	retval |= ret;

	/* enable regulator */
	ret = regulator_enable(disp_bias_pos);
	if (ret < 0)
		pr_err("enable regulator disp_bias_pos fail, ret = %d\n", ret);
	retval |= ret;

	ret = regulator_enable(disp_bias_neg);
	if (ret < 0)
		pr_err("enable regulator disp_bias_neg fail, ret = %d\n", ret);
	retval |= ret;

	return retval;
}

static int lcm_panel_bias_disable(void)
{
	int ret = 0;
	int retval = 0;

	lcm_panel_bias_regulator_init();

	ret = regulator_disable(disp_bias_neg);
	if (ret < 0)
		pr_err("disable regulator disp_bias_neg fail, ret = %d\n", ret);
	retval |= ret;

	ret = regulator_disable(disp_bias_pos);
	if (ret < 0)
		pr_err("disable regulator disp_bias_pos fail, ret = %d\n", ret);
	retval |= ret;

	return retval;
}
#endif

int get_lockdown_info_for_ft8720(unsigned char* p_lockdown_info) {
	int ret = 0;
	int i = 0;
	char cmd1[] = {0x00, 0x00};
	char cmd2[] = {0xFF, 0x87, 0x20, 0x01};
	char cmd3[] = {0x00, 0x80};
	char cmd4[] = {0xFF, 0x87, 0x20};
	char cmd5[] = {0x00, 0x50};
	u8 tx[10] = {0};
	struct mtk_ddic_dsi_msg *cmd_msg =
		kzalloc(sizeof(struct mtk_ddic_dsi_msg), GFP_KERNEL);

	if (!cmd_msg) {
		goto NOMEM;
	}

	cmd_msg->channel = 0;
	cmd_msg->flags = 1;
	cmd_msg->tx_cmd_num = 5;

	cmd_msg->type[0] = 0x15; // DSI_DCS_WRITE_SHORT_PACKET_1_PARAM
	cmd_msg->tx_buf[0] = cmd1;
	cmd_msg->tx_len[0] = 2;

	cmd_msg->type[1] = 0x39;
	cmd_msg->tx_buf[1] = cmd2;
	cmd_msg->tx_len[1] = 4;

	cmd_msg->type[2] = 0x15;
	cmd_msg->tx_buf[2] = cmd3;
	cmd_msg->tx_len[2] = 2;

	cmd_msg->type[3] = 0x39;
	cmd_msg->tx_buf[3] = cmd4;
	cmd_msg->tx_len[3] = 3;

	cmd_msg->type[4] = 0x15;
	cmd_msg->tx_buf[4] = cmd5;
	cmd_msg->tx_len[4] = 2;

	pr_info("%s begin +\n", __func__);

	ret = mtk_ddic_dsi_send_cmd(cmd_msg, true, false);
	if (ret != 0) {
		pr_err("%s error\n", __func__);
		goto DONE2;
	}

	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	cmd_msg->channel = 0;
	cmd_msg->tx_cmd_num = 1;
	cmd_msg->type[0] = 0x06; //DSI_DCS_READ_NO_PARAM
	tx[0] = 0xF4;
	cmd_msg->tx_buf[0] = tx;
	cmd_msg->tx_len[0] = 1;

	cmd_msg->rx_cmd_num = 1;
	cmd_msg->rx_buf[0] = kzalloc(8 * sizeof(unsigned char), GFP_KERNEL);

	if (!cmd_msg->rx_buf[0]) {
		pr_err("%s: memory allocation failed\n", __func__);
		ret = -ENOMEM;
		goto DONE2;
	}

	cmd_msg->rx_len[0] = 8;
	ret = mtk_ddic_dsi_read_cmd(cmd_msg);

	if (ret != 0) {
		pr_err("%s error\n", __func__);
		goto DONE1;
	}

	pr_info("%s read lcm addr:0x%x--dlen:%d\n", __func__,
		*(unsigned char *)(cmd_msg->tx_buf[0]), cmd_msg->rx_len[0]);
	for (i = 0; i < 8; i++) {
		pr_info("%s read lcm addr:0x%x--byte:%d,val:0x%x\n", __func__,
			*(unsigned char *)(cmd_msg->tx_buf[0]), i,
			*(unsigned char *)(cmd_msg->rx_buf[0] + i));
		p_lockdown_info[i] = *(unsigned char *)(cmd_msg->rx_buf[0] + i);
	}

DONE1:
	kfree(cmd_msg->rx_buf[0]);
DONE2:
	kfree(cmd_msg);

	pr_info("%s end -\n", __func__);

	return ret;

NOMEM:
	pr_err("%s: memory allocation failed\n", __func__);
	return -ENOMEM;

}
EXPORT_SYMBOL(get_lockdown_info_for_ft8720);


static void lcm_panel_init(struct lcm *ctx)
{
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(5 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(1 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(12 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	lcm_dcs_write_seq_static(ctx, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x1C, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x51, 0xFF);
	lcm_dcs_write_seq_static(ctx, 0x53, 0x2C);
	lcm_dcs_write_seq_static(ctx, 0x55, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x11);
	msleep(120);
	lcm_dcs_write_seq_static(ctx, 0x29);
	msleep(50);
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

	pr_info("[%s] 0c begin+\n", __func__);
	if (!ctx->prepared)
		return 0;

	ctx->error = 0;
	ctx->prepared = false;
	lcm_dcs_write_seq_static(ctx, 0x28);
	mdelay(20);
	lcm_dcs_write_seq_static(ctx, 0x10);
	mdelay(120);
	lcm_dcs_write_seq_static(ctx, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x87, 0x20, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x00, 0x80);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x87, 0x20);
	lcm_dcs_write_seq_static(ctx, 0x00, 0X00);
	lcm_dcs_write_seq_static(ctx, 0xF7, 0x5A, 0xA5, 0x95, 0x27);
#if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
	lcm_panel_bias_disable();
#endif
	if (is_tp_doubleclick_enable() == false || get_panel_dead_flag())
		lm36273_bias_enable(0, 0);

	pr_info("[%s] 0c end-\n", __func__);

	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("[%s] 0c begin+\n", __func__);
	if (ctx->prepared)
		return 0;

#if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
	lcm_panel_bias_enable();
#endif
	if (is_tp_doubleclick_enable() == false || get_panel_dead_flag()) {
		ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_LOW);
		gpiod_set_value(ctx->reset_gpio, 0);
		udelay(4 * 1000);
		devm_gpiod_put(ctx->dev, ctx->reset_gpio);
		lm36273_bl_bias_conf();
		lm36273_bias_enable(1, 5);
	}
	mdelay(5);
	lcm_panel_init(ctx);
	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;

#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif
	pr_info("[%s] 0c end-\n", __func__);

	return ret;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (ctx->enabled)
		return 0;

	/*if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}*/

	ctx->enabled = true;

	return 0;
}

static const struct drm_display_mode default_mode = {
	.clock = 184507,
	.hdisplay = 1080,
	.hsync_start = 1080 + HFP,
	.hsync_end = 1080 + HFP + HSA,
	.htotal = 1080 + HFP + HSA + HBP,
	.vdisplay = 2340,
	.vsync_start = 2340 + VFP,
	.vsync_end = 2340 + VFP + VSA,
	.vtotal = 2340 + VFP + VSA + VBP,
	.vrefresh = 60,
};

#if defined(CONFIG_MTK_PANEL_EXT)

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
		void *handle, unsigned int level)
{
/*	char bl_tb0[] = {0x51, 0xFF};

	bl_tb0[1] = level;

	if (!cb)
		return -1;

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));*/

	return 0;
}

static void handle_dsi_read_data(struct drm_panel *panel, unsigned char *pbuf)
{
	int i = 0;
	int write_len = 0;
	unsigned char *pTemp = panel->connector->panel_read_data;

	/* pbuf - buffer size must >= 256 */
	for (i = 0; i < 3; i++) {
		write_len = scnprintf(pTemp, 8, "p%d=%d", i%2, *(pbuf + i));
		pTemp += write_len;
	}
	pr_info("dsi read %s from panel\n", panel->connector->panel_read_data);
}

static int lcm_whitepoint_xylv_get(struct drm_panel *panel) {
	int ret = 0;
	char cmd1[] = {0x00, 0x00};
	char cmd2[] = {0xFF, 0x87, 0x20, 0x01};
	char cmd3[] = {0x00, 0x80};
	char cmd4[] = {0xFF, 0x87, 0x20};
	char cmd5[] = {0x00, 0x60};
	u8 tx[10] = {0};
	struct mtk_ddic_dsi_msg *cmd_msg =
		kzalloc(sizeof(struct mtk_ddic_dsi_msg), GFP_KERNEL);

	if (!cmd_msg) {
		goto NOMEM;
	}

	cmd_msg->channel = 0;
	cmd_msg->flags = 1;
	cmd_msg->tx_cmd_num = 5;

	cmd_msg->type[0] = 0x15; // DSI_DCS_WRITE_SHORT_PACKET_1_PARAM
	cmd_msg->tx_buf[0] = cmd1;
	cmd_msg->tx_len[0] = 2;

	cmd_msg->type[1] = 0x39;
	cmd_msg->tx_buf[1] = cmd2;
	cmd_msg->tx_len[1] = 4;

	cmd_msg->type[2] = 0x15;
	cmd_msg->tx_buf[2] = cmd3;
	cmd_msg->tx_len[2] = 2;

	cmd_msg->type[3] = 0x39;
	cmd_msg->tx_buf[3] = cmd4;
	cmd_msg->tx_len[3] = 3;

	cmd_msg->type[4] = 0x15;
	cmd_msg->tx_buf[4] = cmd5;
	cmd_msg->tx_len[4] = 2;

	pr_info("%s begin +\n", __func__);

	ret = mtk_ddic_dsi_send_cmd(cmd_msg, true, false);
	if (ret != 0) {
		pr_err("%s error\n", __func__);
		goto DONE2;
	}

	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	cmd_msg->channel = 0;
	cmd_msg->tx_cmd_num = 1;
	cmd_msg->type[0] = 0x06; //DSI_DCS_READ_NO_PARAM
	tx[0] = 0xF4;
	cmd_msg->tx_buf[0] = tx;
	cmd_msg->tx_len[0] = 1;

	cmd_msg->rx_cmd_num = 1;
	cmd_msg->rx_buf[0] = kzalloc(3 * sizeof(unsigned char), GFP_KERNEL);

	if (!cmd_msg->rx_buf[0]) {
		pr_err("%s: memory allocation failed\n", __func__);
		ret = -ENOMEM;
		goto DONE2;
	}

	cmd_msg->rx_len[0] = 3;
	ret = mtk_ddic_dsi_read_cmd(cmd_msg);

	if (ret != 0) {
		pr_err("%s error\n", __func__);
		goto DONE1;
	}

	handle_dsi_read_data(panel, (unsigned char *)cmd_msg->rx_buf[0]);

DONE1:
	kfree(cmd_msg->rx_buf[0]);
DONE2:
	kfree(cmd_msg);

	pr_info("%s end -\n", __func__);

	return ret;

NOMEM:
	pr_err("%s: memory allocation failed\n", __func__);
	return -ENOMEM;

}

static struct mtk_panel_params ext_params = {
	.pll_clk = 600,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A,
		.count = 1,
		.para_list[0] = 0x9C,
	},
	.ssc_disable = 1,
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en=1,
	.corner_pattern_height=ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot=ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size=sizeof(top_rc_pattern),
	.corner_pattern_lt_addr=(void*)top_rc_pattern,
#endif
};

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.panel_whitepoint_get = lcm_whitepoint_xylv_get,
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

	strcpy(panel->connector->display_info.name, drm_dsi_ext_panel_name);
	panel->connector->display_info.width_mm = 71;
	panel->connector->display_info.height_mm = 155;

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

	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
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
	ctx->panel.funcs = &lcm_drm_funcs;

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
	{ .compatible = "j22_43_03_0c,vdo", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel_j22_43_03_0c_vdo",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);
