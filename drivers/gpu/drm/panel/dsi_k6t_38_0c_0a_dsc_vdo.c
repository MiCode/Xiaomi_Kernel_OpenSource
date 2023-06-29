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
#include <linux/of_gpio.h>

#include <drm/drm_notifier_odm.h>
#include <linux/panel_notifier.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mtk_panel_ext.h"
#include "../mediatek/mtk_log.h"
#include "../mediatek/mtk_drm_graphics_base.h"
#endif

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif

#define REGFLAG_CMD       0xFFFA
#define REGFLAG_DELAY       0xFFFC
#define REGFLAG_UDELAY  0xFFFB
#define REGFLAG_END_OF_TABLE    0xFFFD

#define DELAY_AFTER_AOD_IN_MS     40

static int tp_aod_flag;
static int tp_aod_unprepare_flag;
static int tp_flag;
struct drm_notifier_data g_event_data;

static bool bl_delay_after_aod;

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bias_gpio;
	struct gpio_desc *vio1v8_gpio;

	bool prepared;
	bool enabled;

	int error;
	int hbm_state;
	int doze_brightness_state;

	bool hbm_en;
	bool hbm_wait;
	bool LCM_SLEEP_IN;
};

struct drm_notifier_data g_notify_data;

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[128];
};

static char bl_level[] = {0x51, 0x07, 0xFF};
static unsigned int pre_bl_level;

#define lcm_dcs_write_seq(ctx, seq...) \
({\
	const u8 d[] = { seq };\
	BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64, "DCS sequence too big for stack");\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

#define lcm_dcs_write_seq_static(ctx, seq...) \
({\
	static const u8 d[] = { seq };\
	lcm_dcs_write(ctx, d,  ARRAY_SIZE(d));\
})

/*add for aod notify*/
static BLOCKING_NOTIFIER_HEAD(notifier_aod);

int aod_notifier_chain_register(struct notifier_block *n)
{
        return blocking_notifier_chain_register(&notifier_aod, n);
}

int aod_notifier_call_chain(unsigned long val, void *v)
{
        return blocking_notifier_call_chain(&notifier_aod, val, v);
}

int aod_notifier_chain_unregister(struct notifier_block *n)
{
        return blocking_notifier_chain_unregister(&notifier_aod, n);
}

extern int mtk_ddic_dsi_send_cmd(struct mtk_ddic_dsi_msg *cmd_msg, bool blocking);

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
		pr_info("get dsv_pos fail, error: %d\n", ret);
		return ret;
	}

	disp_bias_neg = regulator_get(NULL, "dsv_neg");
	if (IS_ERR(disp_bias_neg)) { /* handle return value */
		ret = PTR_ERR(disp_bias_neg);
		pr_info("get dsv_neg fail, error: %d\n", ret);
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
		pr_info("set voltage disp_bias_pos fail, ret = %d\n", ret);
	retval |= ret;

	ret = regulator_set_voltage(disp_bias_neg, 5400000, 5400000);
	if (ret < 0)
		pr_info("set voltage disp_bias_neg fail, ret = %d\n", ret);
	retval |= ret;

	/* enable regulator */
	ret = regulator_enable(disp_bias_pos);
	if (ret < 0)
		pr_info("enable regulator disp_bias_pos fail, ret = %d\n", ret);
	retval |= ret;

	ret = regulator_enable(disp_bias_neg);
	if (ret < 0)
		pr_info("enable regulator disp_bias_neg fail, ret = %d\n", ret);
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
		pr_info("disable regulator disp_bias_neg fail, ret = %d\n", ret);
	retval |= ret;

	ret = regulator_disable(disp_bias_pos);
	if (ret < 0)
		pr_info("disable regulator disp_bias_pos fail, ret = %d\n", ret);
	retval |= ret;

	return retval;
}
#endif

static void lcm_panel_init(struct lcm *ctx)
{

	lcm_dcs_write_seq_static(ctx, 0x11);
	msleep(20);

	/* Dimming Control */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB2, 0x01, 0x31); // 11 Bit Dim On
	lcm_dcs_write_seq_static(ctx, 0xDF, 0x09, 0x30, 0x95, 0x46, 0xE9);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);


	/* Compression Enable */
	lcm_dcs_write_seq_static(ctx, 0x9D, 0x01);
	/* PPS Setting */
	lcm_dcs_write_seq_static(ctx, 0x9E, 0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x09, 0x60, 0x04,
								  0x38, 0x00, 0x28, 0x02, 0x1C, 0x02, 0x1C, 0x02, 0x00, 0x02,
								  0x0E, 0x00, 0x20, 0x03, 0xDD, 0x00, 0x07, 0x00, 0x0C, 0x02,
								  0x77, 0x02, 0x8B, 0x18, 0x00, 0x10, 0xF0, 0x03, 0x0C, 0x20,
								  0x00, 0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38, 0x46,
								  0x54 ,0x62, 0x69, 0x70, 0x77, 0x79, 0x7B, 0x7D, 0x7E, 0x01,
								  0x02, 0x01, 0x00, 0x09, 0x40, 0x09, 0xBE, 0x19, 0xFC, 0x19,
								  0xFA, 0x19, 0xF8, 0x1A, 0x38, 0x1A, 0x78, 0x1A, 0xB6, 0x2A,
								  0xF6, 0x2B, 0x34, 0x2B, 0x74, 0x3B, 0x74, 0x6B, 0xF4, 0x00);

	/* 60 Hz */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0x60, 0x21);
	lcm_dcs_write_seq_static(ctx, 0xF7, 0x0B);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x15, 0xF6);
	lcm_dcs_write_seq_static(ctx, 0xF6, 0xF0);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x28, 0xF6);
	lcm_dcs_write_seq_static(ctx, 0xF6, 0xF0);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x3B, 0xF6);
	lcm_dcs_write_seq_static(ctx, 0xF6, 0xF0);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x0A, 0xF4);
	lcm_dcs_write_seq_static(ctx, 0xF4, 0x98);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x11, 0xF4);
	lcm_dcs_write_seq_static(ctx, 0xF4, 0xEE);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0xB2);
	lcm_dcs_write_seq_static(ctx, 0xB2, 0x1C);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

	/* Flat Gamma Control */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x88, 0xB1);
	lcm_dcs_write_seq_static(ctx, 0xB1, 0x27, 0x0D, 0xFB, 0x0F, 0xA4, 0x86, 0x01, 0xFF, 0x10,
					0x33, 0xFF, 0x10, 0xFF, 0x35, 0x34, 0x5A, 0x0A, 0x0A, 0x0A,
					0x29, 0x29, 0x29, 0x37, 0x37, 0x37, 0x41, 0x41, 0x41, 0x45,
					0x45, 0x45, 0x1B, 0x2F, 0xDA, 0x18, 0x74, 0x80, 0x00, 0x00, 0x22); /*Flat mode off*/
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

	/* VINT Voltage control */
	lcm_dcs_write_seq_static(ctx, 0xFC, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x11, 0xFE);
	lcm_dcs_write_seq_static(ctx, 0xFE, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xFC, 0xA5, 0xA5);

	/* ESD Setting */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xFC, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xED, 0x01, 0xCD, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xE1, 0x93);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x06, 0xF4);
	lcm_dcs_write_seq_static(ctx, 0xF4, 0x1F);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
	lcm_dcs_write_seq_static(ctx, 0xFC, 0xA5, 0xA5);


	/* Dimming Setting */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x0D, 0xB2);
	lcm_dcs_write_seq_static(ctx, 0xB2, 0x10); // Dimming Speed: 16 Frames
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x0C, 0xB2);
	lcm_dcs_write_seq_static(ctx, 0xB2, 0x30); // Dimming On
	lcm_dcs_write_seq_static(ctx, 0xF7, 0x0B);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

	lcm_dcs_write_seq_static(ctx, 0x53, 0x20);
	lcm_dcs_write_seq_static(ctx, 0x51, 0x00, 0x00);

	msleep(100);
	lcm_dcs_write_seq_static(ctx, 0x29);

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

	int blank;

	pr_info("%s+\n", __func__);

	if (!ctx->prepared) {
		return 0;
	}

	if (bl_delay_after_aod == true) {
		bl_delay_after_aod = false;
		pr_info("%s: wr bl_delay_after_aod to %d\n", __func__, bl_delay_after_aod);
	}

	if (ctx->LCM_SLEEP_IN) {
		pr_info("%s rstio need pullup", __func__);
		blank = DRM_BLANK_POWERDOWN;
		g_notify_data.data = &blank;
		drm_notifier_call_chain(DRM_EVENT_BLANK, &g_notify_data);
		pr_err("[XMFP] : %s ++++ blank = DRM_BLANK_POWERDOWN ++++", __func__);

		/*tp aod notify*/
		tp_aod_unprepare_flag = 1;

		ctx->error = 0;
		ctx->prepared = false;

		ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->reset_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->reset_gpio);
		msleep(8);

		ctx->bias_gpio = devm_gpiod_get(ctx->dev, "bias", GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->bias_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_gpio);

		ctx->vio1v8_gpio = devm_gpiod_get(ctx->dev, "vio1v8", GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->vio1v8_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->vio1v8_gpio);
		ctx->hbm_state = 0;

		ctx->LCM_SLEEP_IN = 0;
	} else {
		pr_info("%s rstio no need pullup", __func__);
		lcm_dcs_write_seq_static(ctx, 0x28);
		msleep(10);
		lcm_dcs_write_seq_static(ctx, 0x10);
		msleep(110);

		ctx->LCM_SLEEP_IN =  1;
	}

	pr_info("%s+\n", __func__);

	return 0;
}

static int lcm_panel_poweron(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s+\n", __func__);

	if (ctx->prepared)
		return 0;

	ctx->vio1v8_gpio = devm_gpiod_get(ctx->dev,
		"vio1v8", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->vio1v8_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->vio1v8_gpio);
	msleep(1);

	ctx->bias_gpio = devm_gpiod_get(ctx->dev,
		"bias", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->bias_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_gpio);
	msleep(10);

	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	msleep(16);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	pr_info("%s-\n", __func__);

	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;
	int blank;

	pr_info("%s+\n", __func__);

	lcm_panel_poweron(panel);

	if (ctx->prepared)
		return 0;

	lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;

#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif
	blank = DRM_BLANK_UNBLANK;
	g_notify_data.data = &blank;
	drm_notifier_call_chain(DRM_EVENT_BLANK, &g_notify_data);
	pr_err("[XMFP] : %s ++++ blank = DRM_BLANK_UNBLANK ++++", __func__);

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

#define HFP (108)
#define HSA (20)
#define HBP (58)
#define VFP_120 (20)
#define VFP (2452)
#define VSA (2)
#define VBP (10)

static const struct drm_display_mode default_mode = {
	.clock = 369469,
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

static const struct drm_display_mode performance_mode = {
	.clock = 369469,
	.hdisplay = 1080,
	.hsync_start = 1080 + HFP,
	.hsync_end = 1080 + HFP + HSA,
	.htotal = 1080 + HFP + HSA + HBP,
	.vdisplay = 2400,
	.vsync_start = 2400 + VFP_120,
	.vsync_end = 2400 + VFP_120 + VSA,
	.vtotal = 2400 + VFP_120 + VSA + VBP,
	.vrefresh = 120,
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

static int panel_ata_check(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	unsigned char data[3];
	unsigned char id[3] = {0xb3, 0x2, 0x1};
	ssize_t ret;

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

	return 0;
}

static struct LCM_setting_table lcm_aod_high_mode[] = {
	/* aod 50nit*/
	{REGFLAG_CMD, 3, {0xF0, 0x5A, 0x5A} },
	{REGFLAG_CMD, 2, {0x53, 0x22} },
	{REGFLAG_CMD, 3, {0xF0, 0xA5, 0xA5} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static struct LCM_setting_table lcm_aod_low_mode[] = {
	/* aod 10nit*/
	{REGFLAG_CMD, 3, {0xF0, 0x5A, 0x5A} },
	{REGFLAG_CMD, 2, {0x53, 0x23} },
	{REGFLAG_CMD, 3, {0xF0, 0xA5, 0xA5} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static int lcm_setbacklight_cmdq(void *dsi,
		dcs_write_gce cb, void *handle, unsigned int level)
{
	bl_level[1]= level >> 8;
	bl_level[2]= level & 0xFF;
	pre_bl_level = level;
	int blank;

	if (!cb)
		return -1;

	pr_info("%s: level = %d, rd bl_delay_after_aod is %d\n", __func__, level, bl_delay_after_aod);

	if (bl_delay_after_aod == true) {
		bl_delay_after_aod = false;

		msleep(DELAY_AFTER_AOD_IN_MS);
		pr_info("%s: wr bl_delay_after_aod to %d and delay %d ms\n", __func__, bl_delay_after_aod, DELAY_AFTER_AOD_IN_MS);
	}

	cb(dsi, handle, bl_level, ARRAY_SIZE(bl_level));

	if(level == 0) {
		blank = PANEL_AOD_BLANK_POWERDOWN;
		g_event_data.data  = &blank;
		aod_notifier_call_chain(PANEL_AOD_EVENT_BLANK, &g_event_data);
		tp_flag = 1;
		pr_err("lcm setbacklight aod suspend tp\n");
	}else if(level != 0 && tp_flag == 1) {
		blank = PANEL_AOD_BLANK_UNBLANK;
		g_event_data.data  = &blank;
		aod_notifier_call_chain(PANEL_AOD_EVENT_BLANK, &g_event_data);
		tp_flag = 0;
		pr_err("lcm setbacklight aod resume tp\n");
	}

	return 0;
}

static int panel_hbm_set(struct drm_panel *panel, bool en)
{
	int ret = 0;
	char hbm_on[] = {0x53, 0xE8};
	char hbm_off[] = {0x53, 0x28};
	char updtate_key[] = {0xF7, 0x0B};
	char bl_tb[] = {0x51, 0x07, 0xff};

	struct lcm *ctx = panel_to_lcm(panel);
	struct mtk_ddic_dsi_msg *cmd_msg =
		vmalloc(sizeof(struct mtk_ddic_dsi_msg));

	pr_info("%s begin +\n", __func__);

	if (unlikely(!cmd_msg)) {
		pr_err("%s vmalloc mtk_ddic_dsi_msg is failed, return\n", __func__);
		return -1;
	}
	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	cmd_msg->channel = 0;
	cmd_msg->flags = 0;
	cmd_msg->tx_cmd_num = 3;

	if(en)
	{
		cmd_msg->type[0] = 0x15;
		cmd_msg->tx_buf[0] = hbm_on;
		cmd_msg->tx_len[0] = ARRAY_SIZE(hbm_on);

		cmd_msg->type[1] = 0x39;
		cmd_msg->tx_buf[1] = bl_tb;
		cmd_msg->tx_len[1] = ARRAY_SIZE(bl_tb);

		cmd_msg->type[2] = 0x15;
		cmd_msg->tx_buf[2] = updtate_key;
		cmd_msg->tx_len[2] = ARRAY_SIZE(updtate_key);
	}else{
		cmd_msg->type[0] = 0x15;
		cmd_msg->tx_buf[0] = hbm_off;
		cmd_msg->tx_len[0] = ARRAY_SIZE(hbm_on);

		cmd_msg->type[1] = 0x39;
		cmd_msg->tx_buf[1] = bl_level;
		cmd_msg->tx_len[1] = ARRAY_SIZE(bl_level);

		cmd_msg->type[2] = 0x15;
		cmd_msg->tx_buf[2] = updtate_key;
		cmd_msg->tx_len[2] = ARRAY_SIZE(updtate_key);
	}

	ret = mtk_ddic_dsi_send_cmd(cmd_msg, false);
	if (ret != 0) {
		DDPPR_ERR("%s error\n", __func__);
		goto  done;
	}

	ctx->hbm_state = en;
done:
	vfree(cmd_msg);

	pr_info("%s end -\n", __func__);
	return ret;
}

static int panel_hbm_get(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	return ctx->hbm_state;
}

static unsigned long panel_doze_get_mode_flags(struct drm_panel *panel,
	int doze_en)
{
	unsigned long mode_flags;

	if (doze_en) {
		mode_flags = MIPI_DSI_MODE_LPM
		       | MIPI_DSI_MODE_EOT_PACKET
		       | MIPI_DSI_CLOCK_NON_CONTINUOUS;
	} else {
		mode_flags = MIPI_DSI_MODE_VIDEO
		       | MIPI_DSI_MODE_VIDEO_BURST
		       | MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET
		       | MIPI_DSI_CLOCK_NON_CONTINUOUS;
	}

	return mode_flags;
}

static struct LCM_setting_table lcm_normal_to_aod_sam[] = {
	/* Internal VDO Packet generation enable*/
	{REGFLAG_CMD, 3, {0xF0, 0x5A, 0x5A} },
	{REGFLAG_CMD, 3, {0xFC, 0x5A, 0x5A} },
	{REGFLAG_CMD, 3, {0xB0, 0x14, 0xFE} },
	{REGFLAG_CMD, 2, {0xFE, 0x12} },
	{REGFLAG_CMD, 3, {0xF0, 0xA5, 0xA5} },
	{REGFLAG_CMD, 3, {0xFC, 0xA5, 0xA5} },

	/*Sleep out*/
	{REGFLAG_CMD, 1, {0x11} },
	{REGFLAG_DELAY, 10, {} },

	/* MIPI Mode cmd */
	{REGFLAG_CMD, 3, {0xF0, 0x5A, 0x5A} },
	{REGFLAG_CMD, 2, {0xF2, 0x03} },
	{REGFLAG_CMD, 3, {0xF0, 0xA5, 0xA5} },

	/* TE vsync ON */
	{REGFLAG_CMD, 2, {0x35, 0x00} },
	{REGFLAG_DELAY, 10, {} },

	/* PCD setting off */
	{REGFLAG_CMD, 3, {0xF0, 0x5A, 0x5A} },
	{REGFLAG_CMD, 2, {0xEA, 0x48} },
	{REGFLAG_CMD, 3, {0xF0, 0xA5, 0xA5} },

	/* AOD AMP ON */
	{REGFLAG_CMD, 3, {0xFC, 0x5A, 0x5A} },
	{REGFLAG_CMD, 3, {0xB0, 0x06, 0xFD} },
	{REGFLAG_CMD, 2, {0xFD, 0x85} },
	{REGFLAG_CMD, 3, {0xFC, 0xA5, 0xA5} },

	/* AOD Mode On Setting */
	{REGFLAG_CMD, 2, {0x53, 0x22} },

	/* Internal VDO Packet generation enable*/
	{REGFLAG_CMD, 3, {0xF0, 0x5A, 0x5A} },
	{REGFLAG_CMD, 3, {0xFC, 0x5A, 0x5A} },
	{REGFLAG_CMD, 3, {0xB0, 0x14, 0xFE} },
	{REGFLAG_CMD, 3, {0xFE, 0x10} },
	{REGFLAG_CMD, 3, {0xF0, 0xA5, 0xA5} },
	{REGFLAG_CMD, 3, {0xFC, 0xA5, 0xA5} },

	/*AOD IP Setting*/
	{REGFLAG_CMD, 3, {0xF0, 0x5A, 0x5A} },
	{REGFLAG_CMD, 3, {0xB0, 0x03, 0xC2} },
	{REGFLAG_CMD, 3, {0xC2, 0x04} },
	{REGFLAG_CMD, 3, {0xF0, 0xA5, 0xA5} },

	/*seed setting*/
	{REGFLAG_CMD, 3, {0xF0, 0x5A, 0x5A} },
	{REGFLAG_CMD, 2, {0x80, 0x92} },
	{REGFLAG_CMD, 2, {0xB1, 0x00} },
	{REGFLAG_CMD, 3, {0xB0, 0x2B, 0xB1} },
	{REGFLAG_CMD, 22, {0xB1, 0xE0, 0x00, 0x06, 0x10, 0xFF,
			0x00, 0x00, 0x00, 0xFF, 0x2A, 0xFF, 0xE2,
			0xFF, 0x00, 0xEE, 0xFF, 0xF1, 0x00, 0xFF,
			0xFF, 0xFF} },
	{REGFLAG_CMD, 3, {0xB0, 0x55, 0xB1} },
	{REGFLAG_CMD, 2, {0xB1, 0x80} },
	{REGFLAG_CMD, 3, {0xF0, 0xA5, 0xA5} },

	{REGFLAG_DELAY, 100, {} },

	/* Display on */
	{REGFLAG_CMD, 1, {0x29} },

	/* MIPI Video cmd*/
	{REGFLAG_CMD, 3, {0xF0, 0x5A, 0x5A} },
	{REGFLAG_CMD, 2, {0xF2, 0x0F} },
	{REGFLAG_CMD, 3, {0xF0, 0xA5, 0xA5} },

	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static int panel_doze_enable(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	unsigned int i = 0;

	for (i = 0; i < (sizeof(lcm_normal_to_aod_sam) /
			sizeof(struct LCM_setting_table)); i++) {
		unsigned int cmd;

		cmd = lcm_normal_to_aod_sam[i].cmd;
		switch (cmd) {
		case REGFLAG_DELAY:
			msleep(lcm_normal_to_aod_sam[i].count);
			break;
		case REGFLAG_UDELAY:
			udelay(lcm_normal_to_aod_sam[i].count);
			break;
		case REGFLAG_END_OF_TABLE:
			break;
		default:
			cb(dsi, handle, lcm_normal_to_aod_sam[i].para_list,
				lcm_normal_to_aod_sam[i].count);
		}
	}

	return 0;
}

static int panel_doze_enable_start(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	int cmd = 0;

	panel_ext_reset(panel, 0);
	usleep_range(10 * 1000, 15 * 1000);
	panel_ext_reset(panel, 1);

	cmd = 0x28;
	cb(dsi, handle, &cmd, 1);
	cmd = 0x10;
	cb(dsi, handle, &cmd, 1);
	msleep(80);
	return 0;
}

static struct LCM_setting_table lcm_aod_to_normal[] = {

	/* Display off */
	/* MIPI Video cmd*/
	{REGFLAG_CMD, 3, {0xF0, 0x5A, 0x5A} },
	{REGFLAG_CMD, 2, {0xF2, 0x0F} },
	{REGFLAG_CMD, 3, {0xF0, 0xA5, 0xA5} },

	/* AOD Mode off Setting */
	{REGFLAG_CMD, 2, {0x53, 0x20} },

	/*seed setting*/
	{REGFLAG_CMD, 3, {0xF0, 0x5A, 0x5A} },
	{REGFLAG_CMD, 2, {0x80, 0x92} },
	{REGFLAG_CMD, 2, {0xB1, 0x00} },
	{REGFLAG_CMD, 3, {0xB0, 0x2B, 0xB1} },
	{REGFLAG_CMD, 22, {0xB1, 0xE0, 0x00, 0x06, 0x10, 0xFF,
					0x00, 0x00, 0x00, 0xFF, 0x2A, 0xFF,
					0xE2, 0xFF, 0x00, 0xEE, 0xFF, 0xF1,
					0x00, 0xFF, 0xFF, 0xFF} },
	{REGFLAG_CMD, 3, {0xB0, 0x55, 0xB1} },
	{REGFLAG_CMD, 2, {0xB1, 0x80} },
	{REGFLAG_CMD, 3, {0xF0, 0xA5, 0xA5} },
	/*normal mode backlight setting*/
	{REGFLAG_CMD, 3, {0x51, 0x03, 0xff} },
	/* Display on */
	{REGFLAG_CMD, 1, {0x29} },

	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static int panel_doze_disable(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	unsigned int i = 0;

	/* Switch back to VDO mode */
	for (i = 0; i < (sizeof(lcm_aod_to_normal) /
			sizeof(struct LCM_setting_table)); i++) {
		unsigned int cmd;

		cmd = lcm_aod_to_normal[i].cmd;

		switch (cmd) {
		case REGFLAG_DELAY:
				msleep(lcm_aod_to_normal[i].count);
			break;
		case REGFLAG_UDELAY:
			udelay(lcm_aod_to_normal[i].count);
			break;
		case REGFLAG_END_OF_TABLE:
			break;
		default:
			cb(dsi, handle, lcm_aod_to_normal[i].para_list,
				lcm_aod_to_normal[i].count);
		}
	}

	return 0;
}

/* (x,y,w*h) total size = 931200 Bytes */
static char doze_area_cmd[45] = {
	0x81, 0x3C, /* cmd id, enabled area */
	0x0C, 0x07, 0x03, 0x21, 0x90, /* area 0 (192,50,672*350) */
	0x0C, 0x07, 0x19, 0x02, 0xEE, /* area 1 (192,400,672*350) */
	0x18, 0x06, 0x38, 0x45, 0x14, /* area 2 (384,960,576*400) */
	0x00, 0x06, 0x51, 0x46, 0xA4, /* area 3 (0,1300,576*400) */
	0x00, 0x00, 0x00, 0x00, 0x00, /* area 4 - not use */
	0x00, 0x00, 0x00, 0x00, 0x00, /* area 5 - not use */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* color_sel, depth, color */
	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00 /* gray, dummy */
};

static int panel_doze_area(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	cb(dsi, handle, doze_area_cmd, ARRAY_SIZE(doze_area_cmd));

	return 0;
}

static struct mtk_panel_params ext_params = {
	.pll_clk = 550,
	.data_rate = 1100,
	.physical_width_um = 69550,
	.physical_height_um = 154560,
	.cust_esd_check = 1,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A,
		.count = 1,
		.para_list[0] = 0x9F,
		.para_list[1] = 0x9D,
	},
	.mi_esd_check_enable = 1,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.hbm_en_time = 2,
	.hbm_dis_time = 1,
	.doze_delay = 3,
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 120,
		.send_mode = LCM_SEND_IN_VDO,
		.send_cmd_need_delay = 1,
		.dfps_cmd_table[0] = {0, 3, {0xF0, 0x5A, 0x5A} },
		.dfps_cmd_table[1] = {0, 2, {0x60, 0x21} },
		.dfps_cmd_table[2] = {0, 2, {0xF7, 0x0B} },
		.dfps_cmd_table[3] = {0, 3, {0xF0, 0xA5, 0xA5} },
	},
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
	.enable                =  1,
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
	.dec_delay             =  526,
	.scale_value           =  32,
	.increment_interval    =  989,
	.decrement_interval    =  7,
	.line_bpg_offset       =  12,
	.nfl_bpg_offset        =  631,
	.slice_bpg_offset      =  651,
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
	.phy_timcon = {
		.lpx = 10,
	},
};

static struct mtk_panel_params ext_params_120hz = {
	.pll_clk = 550,
	.data_rate = 1100,
	.physical_width_um = 69550,
	.physical_height_um = 154560,
	.cust_esd_check = 1,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A,
		.count = 1,
		.para_list[0] = 0x9F,
		.para_list[1] = 0x9D,
	},
	.mi_esd_check_enable = 1,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.hbm_en_time = 2,
	.hbm_dis_time = 1,
	.doze_delay = 3,
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 120,
		.send_mode = LCM_SEND_IN_VDO,
		.send_cmd_need_delay = 1,
		.dfps_cmd_table[0] = {0, 3, {0xF0, 0x5A, 0x5A} },
		.dfps_cmd_table[1] = {0, 2, {0x60, 0x01} },
		.dfps_cmd_table[2] = {0, 2, {0xF7, 0x0B} },
		.dfps_cmd_table[3] = {0, 3, {0xF0, 0xA5, 0xA5} },
	},
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
	.enable                =  1,
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
	.dec_delay             =  526,
	.scale_value           =  32,
	.increment_interval    =  989,
	.decrement_interval    =  7,
	.line_bpg_offset       =  12,
	.nfl_bpg_offset        =  631,
	.slice_bpg_offset      =  651,
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
	.phy_timcon = {
                 .lpx = 10,
         },
};

static int panel_doze_post_disp_on(struct drm_panel *panel,
		void *dsi, dcs_write_gce cb, void *handle)
{

	int cmd = 0;

#ifdef VENDOR_EDIT
/* Hujie@PSW.MM.DisplayDriver.AOD, 2019/12/10, add for keylog*/
	pr_info("debug for lcm %s\n", __func__);
#endif

	cmd = 0x29;
	cb(dsi, handle, &cmd, 1);
	//msleep(2);

	return 0;
}

static int panel_set_aod_light_mode(void *dsi,
	dcs_write_gce cb, void *handle, unsigned int mode)
{
	int i = 0;

	pr_info("debug for lcm %s\n", __func__);

	if (mode >= 1) {
		for (i = 0; i < sizeof(lcm_aod_high_mode)/sizeof(struct LCM_setting_table); i++)
			cb(dsi, handle, lcm_aod_high_mode[i].para_list, lcm_aod_high_mode[i].count);
	} else {
		for (i = 0; i < sizeof(lcm_aod_low_mode)/sizeof(struct LCM_setting_table); i++)
			cb(dsi, handle, lcm_aod_low_mode[i].para_list, lcm_aod_low_mode[i].count);
	}
	pr_info("%s : %d !\n", __func__, mode);

	//memset(send_cmd, 0, RAMLESS_AOD_PAYLOAD_SIZE);
	return 0;
}

static struct drm_display_mode *get_mode_by_id(struct drm_panel *panel,
	unsigned int mode)
{
	struct drm_display_mode *m;
	unsigned int i = 0;

	list_for_each_entry(m, &panel->connector->modes, head) {
		if (i == mode)
			return m;
		i++;
	}
	return NULL;
}

static int mtk_panel_ext_param_set(struct drm_panel *panel,
			 unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(panel, mode);

	if (m->vrefresh == 60)
		ext->params = &ext_params;
	else if (m->vrefresh == 120)
		ext->params = &ext_params_120hz;
	else
		ret = 1;

	return ret;
}

static char bl_tb[] = {0x51, 0x00, 0x00};

static int panel_set_doze_brightness(struct drm_panel *panel, int doze_brightness)
{
	struct lcm *ctx;
	int ret = 0;
	int event = 0;

	struct mtk_ddic_dsi_msg *cmd_msg = NULL;

	if (!panel) {
		pr_err("invalid params\n");
		return -1;
	}

	ctx = panel_to_lcm(panel);

	if (ctx->doze_brightness_state == doze_brightness) {
		pr_info("%s skip same doze_brightness set:%d\n", __func__, doze_brightness);
		return 0;
	}

	cmd_msg = vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	if (unlikely(!cmd_msg)) {
		pr_err("%s vmalloc mtk_ddic_dsi_msg is failed, return\n", __func__);
		return -1;
	}
	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	pr_info("%s+ doze_brightness:%d\n", __func__, doze_brightness);

	if (1 == doze_brightness) {
		bl_tb[1] = 0x01;
		bl_tb[2] = 0x10;

		bl_delay_after_aod = true;
		pr_err("lcm aod notifier doze_brightness = %d, wr bl_delay_after_aod to %d\n",doze_brightness, bl_delay_after_aod);
	} else if (2 == doze_brightness) {
		bl_tb[1] = 0x00;
		bl_tb[2] = 0x16;

		bl_delay_after_aod = true;
		pr_err("lcm aod notifier doze_brightness = %d, wr bl_delay_after_aod to %d\n",doze_brightness, bl_delay_after_aod);
	} else {
		if (pre_bl_level) {
			bl_tb[1] = bl_level[1];
			bl_tb[2] = bl_level[2];

			pr_err("lcm aod notifier pre_bl_level = %d\n",pre_bl_level);
		} else {
			pr_err("lcm aod notifier pre_bl_level = %d\n",pre_bl_level);
			ctx->doze_brightness_state = doze_brightness;
			goto done;
		}
	}

	cmd_msg->channel = 0;
	cmd_msg->flags = 0;
	cmd_msg->tx_cmd_num = 1;

	cmd_msg->type[0] = 0x39;
	cmd_msg->tx_buf[0] = bl_tb;
	cmd_msg->tx_len[0] = ARRAY_SIZE(bl_tb);

	ret = mtk_ddic_dsi_send_cmd(cmd_msg, false);
	if (ret != 0) {
		DDPPR_ERR("%s error\n", __func__);
		goto  done;
	}
	ctx->doze_brightness_state = doze_brightness;

done:
	vfree(cmd_msg);

	pr_info("%s end -\n", __func__);

	return ret;
}

static int panel_get_doze_brightness(struct drm_panel *panel, u32 *doze_brightness)
{
	struct lcm *ctx = panel_to_lcm(panel);
	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}

	*doze_brightness = ctx->doze_brightness_state;
	return 0;
}

static void panel_set_flat_on(struct drm_panel *panel)
{
	int ret;
	char level_key_en[] = {0xF0, 0x5A, 0x5A};
	char level_key_dis[] = {0xF0, 0xA5, 0xA5};
	char crc_dis[] = {0x80, 0x00};
	char flat_mode_offset[] = {0xB0, 0x00, 0x88, 0xB1};
	char flat_mode_dis[] = {0xB1, 0x27, 0x2D};

	struct mtk_ddic_dsi_msg *cmd_msg =
		vmalloc(sizeof(struct mtk_ddic_dsi_msg));

	pr_info("%s begin +\n", __func__);
	if (unlikely(!cmd_msg)) {
		pr_err("%s vmalloc mtk_ddic_dsi_msg is failed, return\n", __func__);
		return;
	}
	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	cmd_msg->channel = 0;
	cmd_msg->flags = 0;
	cmd_msg->tx_cmd_num = 5;

	cmd_msg->type[0] = 0x39;
	cmd_msg->tx_buf[0] = level_key_en;
	cmd_msg->tx_len[0] = ARRAY_SIZE(level_key_en);

	cmd_msg->type[1] = 0x15;
	cmd_msg->tx_buf[1] = crc_dis;
	cmd_msg->tx_len[1] = ARRAY_SIZE(crc_dis);

	cmd_msg->type[2] = 0x39;
	cmd_msg->tx_buf[2] = flat_mode_offset;
	cmd_msg->tx_len[2] = ARRAY_SIZE(flat_mode_offset);

	cmd_msg->type[3] = 0x39;
	cmd_msg->tx_buf[3] = flat_mode_dis;
	cmd_msg->tx_len[3] = ARRAY_SIZE(flat_mode_dis);

	cmd_msg->type[4] = 0x39;
	cmd_msg->tx_buf[4] = level_key_dis;
	cmd_msg->tx_len[4] = ARRAY_SIZE(level_key_dis);

	ret = mtk_ddic_dsi_send_cmd(cmd_msg, false);
	if (ret != 0) {
		DDPPR_ERR("%s error\n", __func__);
		goto  done;
	}

done:
	vfree(cmd_msg);

	pr_info("%s end -\n", __func__);
}

static void panel_set_flat_off(struct drm_panel *panel)
{
	int ret;
	char level_key_en[] = {0xF0, 0x5A, 0x5A};
	char level_key_dis[] = {0xF0, 0xA5, 0xA5};
	char crc_dis[] = {0x80, 0x00};
	char flat_mode_offset[] = {0xB0, 0x00, 0x88, 0xB1};
	char flat_mode_dis[] = {0xB1, 0x27, 0x0D};

	struct mtk_ddic_dsi_msg *cmd_msg =
		vmalloc(sizeof(struct mtk_ddic_dsi_msg));

	pr_info("%s begin +\n", __func__);
	if (unlikely(!cmd_msg)) {
		pr_err("%s vmalloc mtk_ddic_dsi_msg is failed, return\n", __func__);
		return;
	}
	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	cmd_msg->channel = 0;
	cmd_msg->flags = 0;
	cmd_msg->tx_cmd_num = 5;

	cmd_msg->type[0] = 0x39;
	cmd_msg->tx_buf[0] = level_key_en;
	cmd_msg->tx_len[0] = ARRAY_SIZE(level_key_en);

	cmd_msg->type[1] = 0x15;
	cmd_msg->tx_buf[1] = crc_dis;
	cmd_msg->tx_len[1] = ARRAY_SIZE(crc_dis);

	cmd_msg->type[2] = 0x39;
	cmd_msg->tx_buf[2] = flat_mode_offset;
	cmd_msg->tx_len[2] = ARRAY_SIZE(flat_mode_offset);

	cmd_msg->type[3] = 0x39;
	cmd_msg->tx_buf[3] = flat_mode_dis;
	cmd_msg->tx_len[3] = ARRAY_SIZE(flat_mode_dis);

	cmd_msg->type[4] = 0x39;
	cmd_msg->tx_buf[4] = level_key_dis;
	cmd_msg->tx_len[4] = ARRAY_SIZE(level_key_dis);

	ret = mtk_ddic_dsi_send_cmd(cmd_msg, false);
	if (ret != 0) {
		DDPPR_ERR("%s error\n", __func__);
		goto  done;
	}

done:
	vfree(cmd_msg);

	pr_info("%s end -\n", __func__);
}

static void panel_set_p3_vivid(struct drm_panel *panel)
{
	int ret;
	char level_key_en[] = {0xF0, 0x5A, 0x5A};
	char level_key_dis[] = {0xF0, 0xA5, 0xA5};
	char crc_en[] = {0x80, 0x05};
	char crc_on[] = {0xB1, 0x00};
	char crc_offset[] = {0xB0, 0x00, 0x01, 0xB1};
	char crc_p3[] = {0xB1, 0xC9, 0x00, 0x00, 0x12, 0xCC, 0x02, 0x0A, 0x0B, 0xE9,
			 0x12, 0xE8, 0xE9, 0xE5, 0x02, 0xEE, 0xDE, 0xDC, 0x02, 0xFF,
			 0xFF, 0xFF}; /* 0.3 0.315 DCI-P3 */
	char flat_mode_offset[] = {0xB0, 0x00, 0x88, 0xB1};
	char flat_mode_dis[] = {0xB1, 0x27, 0x0D};

	struct mtk_ddic_dsi_msg *cmd_msg =
			vmalloc(sizeof(struct mtk_ddic_dsi_msg));

	pr_info("%s begin +\n", __func__);
	if (unlikely(!cmd_msg)) {
		pr_err("%s vmalloc mtk_ddic_dsi_msg is failed, return\n", __func__);
		return;
	}
	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	cmd_msg->channel = 0;
	cmd_msg->flags = 0;
	cmd_msg->tx_cmd_num = 8;

	cmd_msg->type[0] = 0x39;
	cmd_msg->tx_buf[0] = level_key_en;
	cmd_msg->tx_len[0] = ARRAY_SIZE(level_key_en);

	cmd_msg->type[1] = 0x15;
	cmd_msg->tx_buf[1] = crc_en;
	cmd_msg->tx_len[1] = ARRAY_SIZE(crc_en);

	cmd_msg->type[2] = 0x15;
	cmd_msg->tx_buf[2] = crc_on;
	cmd_msg->tx_len[2] = ARRAY_SIZE(crc_on);

	cmd_msg->type[3] = 0x39;
	cmd_msg->tx_buf[3] = crc_offset;
	cmd_msg->tx_len[3] = ARRAY_SIZE(crc_offset);

	cmd_msg->type[4] = 0x39;
	cmd_msg->tx_buf[4] = crc_p3;
	cmd_msg->tx_len[4] = ARRAY_SIZE(crc_p3);

	cmd_msg->type[5] = 0x39;
	cmd_msg->tx_buf[5] = flat_mode_offset;
	cmd_msg->tx_len[5] = ARRAY_SIZE(flat_mode_offset);

	cmd_msg->type[6] = 0x39;
	cmd_msg->tx_buf[6] = flat_mode_dis;
	cmd_msg->tx_len[6] = ARRAY_SIZE(flat_mode_dis);

	cmd_msg->type[7] = 0x39;
	cmd_msg->tx_buf[7] = level_key_dis;
	cmd_msg->tx_len[7] = ARRAY_SIZE(level_key_dis);

	ret = mtk_ddic_dsi_send_cmd(cmd_msg, false);
	if (ret != 0) {
		DDPPR_ERR("%s error\n", __func__);
		goto  done;
	}

done:
	vfree(cmd_msg);

	pr_info("%s end -\n", __func__);
}

static void panel_set_srgb_standard(struct drm_panel *panel)
{
	int ret;
	char level_key_en[] = {0xF0, 0x5A, 0x5A};
	char level_key_dis[] = {0xF0, 0xA5, 0xA5};
	char crc_en[] = {0x80, 0x05};
	char crc_on[] = {0xB1, 0x00};
	char crc_offset[] = {0xB0, 0x00, 0x01, 0xB1};
	char crc_srgb[] = {0xB1, 0xBB, 0x08, 0x05, 0x34, 0xE2, 0x13, 0x07, 0x07, 0xAA,
			   0x3A, 0xE6, 0xCC, 0xC1, 0x0E, 0xB9, 0xF2, 0xE7, 0x17, 0xFF,
			   0xF6, 0xD3}; /* 0.3127 0.329 sRGB flat */
	char flat_mode_offset[] = {0xB0, 0x00, 0x88, 0xB1};
	char flat_mode_en[] = {0xB1, 0x27, 0x2D};

	struct mtk_ddic_dsi_msg *cmd_msg =
		vmalloc(sizeof(struct mtk_ddic_dsi_msg));

	pr_info("%s begin +\n", __func__);
	if (unlikely(!cmd_msg)) {
		pr_err("%s vmalloc mtk_ddic_dsi_msg is failed, return\n", __func__);
		return;
	}
	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	cmd_msg->channel = 0;
	cmd_msg->flags = 0;
	cmd_msg->tx_cmd_num = 8;

	cmd_msg->type[0] = 0x39;
	cmd_msg->tx_buf[0] = level_key_en;
	cmd_msg->tx_len[0] = ARRAY_SIZE(level_key_en);

	cmd_msg->type[1] = 0x15;
	cmd_msg->tx_buf[1] = crc_en;
	cmd_msg->tx_len[1] = ARRAY_SIZE(crc_en);

	cmd_msg->type[2] = 0x15;
	cmd_msg->tx_buf[2] = crc_on;
	cmd_msg->tx_len[2] = ARRAY_SIZE(crc_on);

	cmd_msg->type[3] = 0x39;
	cmd_msg->tx_buf[3] = crc_offset;
	cmd_msg->tx_len[3] = ARRAY_SIZE(crc_offset);

	cmd_msg->type[4] = 0x39;
	cmd_msg->tx_buf[4] = crc_srgb;
	cmd_msg->tx_len[4] = ARRAY_SIZE(crc_srgb);

	cmd_msg->type[5] = 0x39;
	cmd_msg->tx_buf[5] = flat_mode_offset;
	cmd_msg->tx_len[5] = ARRAY_SIZE(flat_mode_offset);

	cmd_msg->type[6] = 0x39;
	cmd_msg->tx_buf[6] = flat_mode_en;
	cmd_msg->tx_len[6] = ARRAY_SIZE(flat_mode_en);

	cmd_msg->type[7] = 0x39;
	cmd_msg->tx_buf[7] = level_key_dis;
	cmd_msg->tx_len[7] = ARRAY_SIZE(level_key_dis);

	ret = mtk_ddic_dsi_send_cmd(cmd_msg, false);
	if (ret != 0) {
		DDPPR_ERR("%s error\n", __func__);
		goto  done;
	}

done:
	vfree(cmd_msg);

	pr_info("%s end -\n", __func__);
}

static void lcm_esd_restore_backlight(struct drm_panel *panel, int aod_mode)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if ((aod_mode == 1) || (aod_mode == 2)) {
		lcm_dcs_write(ctx, bl_tb, ARRAY_SIZE(bl_tb));
		pr_info("%s aod high_bl = 0x%x, low_bl = 0x%x \n", __func__, bl_tb[1], bl_tb[2]);
	} else {
		lcm_dcs_write(ctx, bl_level, ARRAY_SIZE(bl_level));
		pr_info("%s high_bl = 0x%x, low_bl = 0x%x \n", __func__, bl_level[1], bl_level[2]);
	}

	return;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.esd_restore_backlight = lcm_esd_restore_backlight,
	.ata_check = panel_ata_check,
	.hbm_set = panel_hbm_set,
	.hbm_get = panel_hbm_get,
	.aod_set = panel_set_doze_brightness,
	.aod_get = panel_get_doze_brightness,
	.ext_param_set = mtk_panel_ext_param_set,
	.panel_set_srgb_standard = panel_set_srgb_standard,
	.panel_set_flat_off = panel_set_flat_off,
	.panel_set_flat_on = panel_set_flat_on,
	.panel_set_p3_vivid = panel_set_p3_vivid,

	/* add for ramless AOD */
	.doze_get_mode_flags = panel_doze_get_mode_flags,
	.doze_enable = panel_doze_enable,
	.doze_enable_start = panel_doze_enable_start,
	.doze_area = panel_doze_area,
	.doze_disable = panel_doze_disable,
	.doze_post_disp_on = panel_doze_post_disp_on,
	.set_aod_light_mode = panel_set_aod_light_mode,
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
	struct drm_display_mode *mode2;

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

	mode2 = drm_mode_duplicate(panel->drm, &performance_mode);
	if (!mode2) {
		dev_info(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
			performance_mode.hdisplay,
			performance_mode.vdisplay,
			performance_mode.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode2);
	mode2->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(panel->connector, mode2);

	return 1;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};

static ssize_t get_aod_area(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i;

	for (i = 0; i < sizeof(doze_area_cmd) / sizeof(char); i++)
		pr_info("%s cmd = %d", __func__, doze_area_cmd[i]);
	return 0;
}

static ssize_t set_aod_area(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	//struct lcm *ctx = mipi_dsi_get_drvdata(dev);
	int i, ret;

	for (i = 0; i < count; i++) {
		ret = sscanf(&buf[i], "%c", &doze_area_cmd[i]);
		pr_info("%s ret = %d, buf[%d]=%d", __func__, ret, i, buf[i]);
	}

	return ret;
}

static DEVICE_ATTR(aod_area, 0644,
		get_aod_area, set_aod_area);

static struct attribute *aod_area_sysfs_attrs[] = {
	&dev_attr_aod_area.attr,
	NULL,
};

static struct attribute_group aod_area_sysfs_attr_group = {
	.attrs = aod_area_sysfs_attrs,
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

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST
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
		dev_info(dev, "cannot get reset-gpios %ld\n",
			PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);

	ext_params.err_flag_irq_gpio = of_get_named_gpio_flags(
			dev->of_node, "mi,esd-err-irq-gpio",
			0, (enum of_gpio_flags *)&(ext_params.err_flag_irq_flags));

	ext_params_120hz.err_flag_irq_gpio = ext_params.err_flag_irq_gpio;
	ext_params_120hz.err_flag_irq_flags = ext_params.err_flag_irq_flags;

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

	ctx->hbm_state = 0;
	ctx->doze_brightness_state = 0;
	ctx->LCM_SLEEP_IN = 0;
	ret = sysfs_create_group(&dev->kobj, &aod_area_sysfs_attr_group);
	if (ret)
		return ret;
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
	{ .compatible = "samsung, dsi_k6t_38_0c_0a_dsc_vdo", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "dsi_k6t_38_0c_0a_dsc_vdo",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);
