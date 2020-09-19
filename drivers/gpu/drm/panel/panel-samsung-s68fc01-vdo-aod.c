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

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mtk_panel_ext.h"
#include "../mediatek/mtk_log.h"
#include "../mediatek/mtk_drm_graphics_base.h"
#endif

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif

#define HSA 14
#define HBP 22
#define HFP 48

#define VSA 2
#define VBP 9
#define VFP 21

#define REGFLAG_CMD       0xFFFA
#define REGFLAG_DELAY       0xFFFC
#define REGFLAG_UDELAY  0xFFFB
#define REGFLAG_END_OF_TABLE    0xFFFD

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bias_gpio;

	bool prepared;
	bool enabled;

	int error;

	bool hbm_en;
	bool hbm_wait;
};

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static char bl_tb0[] = {0x51, 0x03, 0xFF};

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
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10 * 1000, 15 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	lcm_dcs_write_seq_static(ctx, 0x9F, 0xA5, 0xA5);
	lcm_dcs_write_seq_static(ctx, 0x11);
	msleep(20);
	lcm_dcs_write_seq_static(ctx, 0x9F, 0x5A, 0x5A);

	lcm_dcs_write_seq_static(ctx, 0x9F, 0xA5, 0xA5);
	lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x9F, 0x5A, 0x5A);

	lcm_dcs_write_seq_static(ctx, 0xFC, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xED, 0x00, 0x01, 0x00, 0x40, 0x04, 0x08, 0xA8,
			0x84, 0x4A, 0x73, 0x02, 0x0A);
	lcm_dcs_write_seq_static(ctx, 0xFC, 0xA5, 0xA5);

	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x05);
	lcm_dcs_write_seq_static(ctx, 0xB3, 0x87);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
	lcm_dcs_write_seq_static(ctx, 0x53, 0x20);
	lcm_dcs_write_seq_static(ctx, 0x55, 0x00);
	msleep(100);
	lcm_dcs_write_seq_static(ctx, 0x9F, 0xA5, 0xA5);
	lcm_dcs_write_seq_static(ctx, 0x29);
	lcm_dcs_write_seq_static(ctx, 0x9F, 0x5A, 0x5A);
	lcm_dcs_write_seq(ctx, bl_tb0[0], bl_tb0[1], bl_tb0[2]);
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

	ctx->error = 0;
	ctx->prepared = false;
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(10 * 1000, 15 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	usleep_range(10 * 1000, 15 * 1000);

	ctx->bias_gpio =
	devm_gpiod_get(ctx->dev, "bias", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->bias_gpio, 0);
	usleep_range(10 * 1000, 15 * 1000);
	devm_gpiod_put(ctx->dev, ctx->bias_gpio);

	ctx->hbm_en = false;

	return 0;
}

static int lcm_panel_poweron(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	if (ctx->prepared)
		return 0;

	usleep_range(10 * 1000, 10 * 1000);
	ctx->bias_gpio = devm_gpiod_get(ctx->dev,
		"bias", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->bias_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_gpio);
	usleep_range(10 * 1000, 10 * 1000);
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(5 * 1000, 5 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(5 * 1000, 5 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10 * 1000, 10 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);
	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

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
	.clock = 179222,
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
	bl_tb0[1] = level * 4 >> 8;
	bl_tb0[2] = level * 4 & 0xFF;

	if (!cb)
		return -1;

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));

	return 0;
}

static int panel_hbm_set_cmdq(struct drm_panel *panel, void *dsi,
			      dcs_write_gce cb, void *handle, bool en)
{
	char hbm_tb[] = {0x53, 0xe0};
	struct lcm *ctx = panel_to_lcm(panel);

	if (!cb)
		return -1;

	if (ctx->hbm_en == en)
		goto done;

	if (en)
		hbm_tb[1] = 0xe0;
	else
		hbm_tb[1] = 0x20;

	cb(dsi, handle, hbm_tb, ARRAY_SIZE(hbm_tb));

	ctx->hbm_en = en;
	ctx->hbm_wait = true;

done:
	return 0;
}

static void panel_hbm_get_state(struct drm_panel *panel, bool *state)
{
	struct lcm *ctx = panel_to_lcm(panel);

	*state = ctx->hbm_en;
}

static void panel_hbm_get_wait_state(struct drm_panel *panel, bool *wait)
{
	struct lcm *ctx = panel_to_lcm(panel);

	*wait = ctx->hbm_wait;
}

static bool panel_hbm_set_wait_state(struct drm_panel *panel, bool wait)
{
	struct lcm *ctx = panel_to_lcm(panel);
	bool old = ctx->hbm_wait;

	ctx->hbm_wait = wait;
	return old;
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
		       | MIPI_DSI_MODE_VIDEO_SYNC_PULSE
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
	.data_rate = 1098,
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x00,
		.count = 1,
		.para_list[0] = 0x24,
	},
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.hbm_en_time = 2,
	.hbm_dis_time = 1,
	.doze_delay = 3,
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

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = panel_ata_check,
	.hbm_set_cmdq = panel_hbm_set_cmdq,
	.hbm_get_state = panel_hbm_get_state,
	.hbm_get_wait_state = panel_hbm_get_wait_state,
	.hbm_set_wait_state = panel_hbm_set_wait_state,

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

	panel->connector->display_info.width_mm = 71;
	panel->connector->display_info.height_mm = 153;

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
		dev_info(dev, "cannot get reset-gpios %ld\n",
			PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);
	ctx->bias_gpio = devm_gpiod_get(dev, "bias", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_gpio)) {
		dev_info(dev, "cannot get bias-gpios 0 %ld\n",
			PTR_ERR(ctx->bias_gpio));
		return PTR_ERR(ctx->bias_gpio);
	}
	devm_gpiod_put(dev, ctx->bias_gpio);

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

	ctx->hbm_en = false;
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
	{ .compatible = "s68fc01,vdo,aod", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "s68fc01,vdo,aod",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);
