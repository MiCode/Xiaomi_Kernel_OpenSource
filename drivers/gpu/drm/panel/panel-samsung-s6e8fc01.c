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

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <mt-plat/upmu_common.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mtk_panel_ext.h"
#include "../mediatek/mtk_log.h"
#include "../mediatek/mtk_drm_graphics_base.h"
#endif

#ifdef CONFIG_HWCONF_MANAGER
#include "../mediatek/dsi_panel_mi_count.h"
#endif

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mtk_corner_pattern/mtk_hw_roundedpattern_samsung_s6e8fc01.h"
#endif
#include "panel.h"

#define HSA 14
#define HBP 22
#define HFP 48

#define VSA 2
#define VBP 9
#define VFP 21

#define DC_BACKLIGHT_LEVEL 562

static char bl_tb0[] = {0x51, 0x03, 0xFF};
static char *panel_name = "panel_name=dsi_j7_38_0c_0a_video_display";
struct mtk_ddic_dsi_msg cmd_msg = {0};
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

static void lcm_panel_init(struct lcm *ctx)
{
	pr_info("%s+ \n", __func__);
	gpiod_set_value(ctx->reset_gpio, 0);
	msleep(1);
	gpiod_set_value(ctx->reset_gpio, 1);
	msleep(10);

	lcm_dcs_write_seq_static(ctx, 0x11);
	msleep(20);
	/* TE vsync ON */
	lcm_dcs_write_seq_static(ctx, 0x35, 0x00);

	lcm_dcs_write_seq_static(ctx, 0xFC, 0x5A, 0x5A);
	/* FAIL SAFE Setting */
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x03, 0xED);
	lcm_dcs_write_seq_static(ctx, 0xED, 0x40, 0x04, 0x08, 0xA8, 0x84, 0x4A, 0x73, 0x02, 0x0A);
	/* ESD Err Fg Setting */
	lcm_dcs_write_seq_static(ctx, 0xED, 0x01, 0xCD, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xE1, 0x93);
	/* OFC Setting 91.8M*/
	lcm_dcs_write_seq_static(ctx, 0xDF, 0x09, 0x30, 0x95, 0x43, 0x34, 0x05, 0x00, 0x27, 0x00, 0x2E,
							0x4F, 0x7A, 0x77, 0x10, 0x3D, 0x73, 0x00, 0xFF, 0x01, 0x8B, 0x08);
	lcm_dcs_write_seq_static(ctx, 0xFC, 0xA5, 0xA5);

	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	/* 11Bit Dimming Setting */
	lcm_dcs_write_seq_static(ctx, 0xB3, 0x11, 0x4B);
	/* Fast Discharge Setting */
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xCD, 0x04);
	/* Dynamic elvss setting */
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x04, 0xB3);
	lcm_dcs_write_seq_static(ctx, 0xB3, 0xC0);
	/* Dimming Speed Setting */
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x06, 0xB3);
	lcm_dcs_write_seq_static(ctx, 0xB3, 0x10);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x05, 0xB3);
	lcm_dcs_write_seq_static(ctx, 0xB3, 0x93);
	/* CRC SRGB/P3 Setting */
	lcm_dcs_write_seq_static(ctx, 0x80, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xB1, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x01, 0xB1);
	lcm_dcs_write_seq_static(ctx, 0xB1, 0xA7, 0x02, 0x05, 0x34, 0xCF, 0x14, 0x09, 0x04, 0xAE,
							0x46, 0xEB, 0xD0, 0xC7, 0x06, 0xC1, 0xE6, 0xE2, 0x1D, 0xFF, 0xFA, 0xE1);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x16, 0xB1);
	lcm_dcs_write_seq_static(ctx, 0xB1, 0xD9, 0x00, 0x00, 0x06, 0xCF, 0x02, 0x09, 0x03, 0xC6,
							0x10, 0xF4, 0xD9, 0xFD, 0x00, 0xE0, 0xE8, 0xE1, 0x03, 0xFF, 0xFF, 0xFF);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x55, 0xB1);
	lcm_dcs_write_seq_static(ctx, 0xB1, 0x80);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

	lcm_dcs_write_seq_static(ctx, 0x53, 0x20);
	lcm_dcs_write(ctx, bl_tb0, ARRAY_SIZE(bl_tb0));
	//lcm_dcs_write_seq_static(ctx, 0x51, bl_tb0[1], bl_tb0[2]);
	msleep(60);
	lcm_dcs_write_seq_static(ctx, 0x29);
	pr_info("%s-\n", __func__);
}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("%s+ \n", __func__);
	if (!ctx->enabled)
		return 0;

	lcm_dcs_write_seq_static(ctx, 0x28, 0x00);
	msleep(10);
	lcm_dcs_write_seq_static(ctx, 0x10, 0x00);
	msleep(150);

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;
	pr_info("%s-\n", __func__);
	return 0;
}

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("%s+ \n", __func__);
	if (!ctx->prepared)
		return 0;

	ctx->error = 0;

#if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
	lcm_panel_bias_disable();
#endif

	gpiod_set_value(ctx->reset_gpio, 0);

	gpiod_set_value(ctx->pm_enable_gpio, 0);
	pmic_set_register_value(PMIC_RG_LDO_VIBR_EN, 0);

	ctx->prepared = false;
	ctx->hbm_en = false;
	pr_info("%s-\n", __func__);
	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s+ \n", __func__);
	if (ctx->prepared)
		return 0;

#if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
	lcm_panel_bias_enable();
#endif
	gpiod_set_value(ctx->pm_enable_gpio, 1);
	msleep(10);
	pmic_set_register_value(PMIC_RG_VIBR_VOSEL, 0xB);
	pmic_set_register_value(PMIC_RG_LDO_VIBR_EN, 1);

	lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;
	ctx->hbm_en = false;

#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif
	pr_info("%s-\n", __func__);
	return ret;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("%s+ \n", __func__);
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
	pr_info("%s+ \n", __func__);
	gpiod_set_value(ctx->reset_gpio, on);
	pr_info("%s-\n", __func__);
	return 0;
}

static int lcm_setbacklight_cmdq(void *dsi,
		dcs_write_gce cb, void *handle, unsigned int level)
{
	bl_tb0[1] = (level >> 8) & 0x7;
	bl_tb0[2] = level & 0xFF;
	pr_info("%s+ \n", __func__);
	if (!cb)
		return -1;

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
	pr_info("%s-\n", __func__);
	return 0;
}

static int panel_hbm_set_cmdq(struct drm_panel *panel, void *dsi,
			      dcs_write_gce cb, void *handle, bool en)
{
	char hbm_tb[] = {0x53, 0xe0};
	char bl_tb[] = {0x51, 0x07, 0xff};
	char key2_on_tb[] = {0xF0, 0x5A, 0x5A};
	char key2_off_tb[] = {0xF0, 0xA5, 0xA5};
	char elvss_offset_tb[] = {0xB0, 0x05, 0xB3};
	char elvss_tb[] = {0xB3, 0x93};
	struct lcm *ctx = panel_to_lcm(panel);

	if (!cb)
		return -1;

	if (ctx->hbm_en == en) {
		pr_info("%s-skip hbm set\n", __func__);
		goto done;
	}

	if (en) {
		hbm_tb[1] = 0xe0;
		elvss_tb[1] = 0x13;
	} else {
		bl_tb[1] = bl_tb0[1];
		bl_tb[2] = bl_tb0[2];
		hbm_tb[1] = 0x20;
		elvss_tb[1] = 0x93;
	}
	pr_info("%s+, hbm: %s\n", __func__, en ? "OFF->ON":"ON->OFF");
	cb(dsi, handle, key2_on_tb, ARRAY_SIZE(key2_on_tb));
	cb(dsi, handle, elvss_offset_tb, ARRAY_SIZE(elvss_offset_tb));
	cb(dsi, handle, elvss_tb, ARRAY_SIZE(elvss_tb));
	cb(dsi, handle, key2_off_tb, ARRAY_SIZE(key2_off_tb));
	cb(dsi, handle, hbm_tb, ARRAY_SIZE(hbm_tb));
	cb(dsi, handle, bl_tb, ARRAY_SIZE(bl_tb));

	if (!en)
		ctx->set_backlight_time = ktime_add_ms(ktime_get(), 80);

	ctx->hbm_en = en;
	ctx->hbm_wait = true;

done:
	return 0;
}

static int lcm_setbacklight_control(struct drm_panel *panel, unsigned int level, bool fod_cal_flag)
{
	bool fod_backlight_flag;
	char bl_tb[] = {0x51, 0x00, 0x00};
	int ret;
	static unsigned int  last_backlight;
	struct lcm *ctx = panel_to_lcm(panel);

	if (!panel->connector) {
		pr_err("%s, the connector is null\n", __func__);
		return 0;
	}

	last_backlight = (bl_tb0[1]  << 8) | bl_tb0[2];
	bl_tb[1] = (level >> 8) & 0x7;
	bl_tb[2] = level & 0xFF;
	if ( !((mtk_doze_flag(panel->connector)) &&  ((47 == level) || (255 == level)))) {
		if (level) {
			bl_tb0[1] = bl_tb[1];
			bl_tb0[2] = bl_tb[2];
			pr_debug("%s save,doze_flag = %d, level = %d, high_bl = 0x%x, low_bl = 0x%x \n", __func__, mtk_doze_flag(panel->connector), level, bl_tb0[1], bl_tb0[2]);
		}
		else {
			pr_debug("%s,don't save,doze_flag = %d, level = %d, high_bl = 0x%x, low_bl = 0x%x \n", __func__, mtk_doze_flag(panel->connector), level, bl_tb0[1], bl_tb0[2]);
		}
	}

	fod_backlight_flag = mtk_fod_backlight_flag(panel->connector);
	if (fod_cal_flag || !fod_backlight_flag) {
		cmd_msg.channel = 0;
		cmd_msg.flags = 0;
		cmd_msg.tx_cmd_num = 1;

		cmd_msg.type[0] = 0x39;
		cmd_msg.tx_buf[0] = bl_tb;
		cmd_msg.tx_len[0] = 3;

		ret = mtk_ddic_dsi_send_cmd(&cmd_msg, true, false);
		if (ret != 0) {
			pr_err("%s mtk_ddic_dsi_send_cmd error\n", __func__);
		}
		if (((!last_backlight) && level) || (last_backlight && (!level))) {
			pr_info("%s,fod_backlight_flag = %d, level = %d, high_bl = 0x%x, low_bl = 0x%x \n", __func__, fod_backlight_flag, level, bl_tb0[1], bl_tb0[2]);
		}

		ctx->set_backlight_time = ktime_add_ms(ktime_get(), 20);
	}
	else {
		pr_info("%s,fod_backlight_flag = %d, can't set backlight\n", __func__, fod_backlight_flag);
	}
	if (!fod_cal_flag && !(mtk_dc_flag(panel->connector) && DC_BACKLIGHT_LEVEL == level)) {
		panel->connector->brightness_clone = level;
		sysfs_notify(&panel->connector->kdev->kobj, NULL, "brightness_clone");
	}
	return 0;
}

static void panel_dimming_control(struct drm_panel *panel, bool en)
{
	struct lcm *ctx = panel_to_lcm(panel);

	struct mtk_ddic_dsi_msg *cmd_msg =
			vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	int ret;
	char key2_on_tb[] = {0xF0, 0x5A, 0x5A};
	char key2_off_tb[] = {0xF0, 0xA5, 0xA5};
	char elvss_offset_tb[] = {0xB0, 0x05, 0xB3};
	char elvss_tb[] = {0xB3, 0x93};

	if (unlikely(!cmd_msg)) {
		pr_err("%s vmalloc mtk_ddic_dsi_msg is failed, return\n", __func__);
		return;
	}
	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	if (en) {
		elvss_tb[1] = 0x93;
	}
	else {
		elvss_tb[1] = 0x13;
	}

	cmd_msg->channel = 0;
	cmd_msg->flags = 0;
	cmd_msg->tx_cmd_num = 4;

	cmd_msg->type[0] = 0x39;
	cmd_msg->tx_buf[0] = key2_on_tb;
	cmd_msg->tx_len[0] = 3;

	cmd_msg->type[1] = 0x39;
	cmd_msg->tx_buf[1] = elvss_offset_tb;
	cmd_msg->tx_len[1] = 3;

	cmd_msg->type[2] = 0x15;
	cmd_msg->tx_buf[2] = elvss_tb;
	cmd_msg->tx_len[2] = 2;

	cmd_msg->type[3] = 0x39;
	cmd_msg->tx_buf[3] = key2_off_tb;
	cmd_msg->tx_len[3] = 3;

	pr_info("%s+,elvss_dimming = %d\n", __func__, en);

	ret = mtk_ddic_dsi_send_cmd(cmd_msg, true, false);
	if (ret != 0) {
		pr_err("%s mtk_ddic_dsi_send_cmd error\n", __func__);
	}

	vfree(cmd_msg);
	pr_info("%s-\n", __func__);

	return;
}

static void panel_elvss_control(struct drm_panel *panel, bool en)
{
	struct lcm *ctx = panel_to_lcm(panel);

	struct mtk_ddic_dsi_msg *cmd_msg =
			vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	int ret;
	char key2_on_tb[] = {0xF0, 0x5A, 0x5A};
	char key2_off_tb[] = {0xF0, 0xA5, 0xA5};
	char elvss_offset_tb[] = {0xB0, 0x05, 0xB3};
	char elvss_tb[] = {0xB3, 0x93};

	if (unlikely(!cmd_msg)) {
		pr_err("%s vmalloc mtk_ddic_dsi_msg is failed, return\n", __func__);
		return;
	}
	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	if (en) {
		elvss_tb[1] = 0x93;
	}
	else {
		elvss_tb[1] = 0x13;
	}

	cmd_msg->channel = 0;
	cmd_msg->flags = 0;
	cmd_msg->tx_cmd_num = 4;

	cmd_msg->type[0] = 0x39;
	cmd_msg->tx_buf[0] = key2_on_tb;
	cmd_msg->tx_len[0] = 3;

	cmd_msg->type[1] = 0x39;
	cmd_msg->tx_buf[1] = elvss_offset_tb;
	cmd_msg->tx_len[1] = 3;

	cmd_msg->type[2] = 0x15;
	cmd_msg->tx_buf[2] = elvss_tb;
	cmd_msg->tx_len[2] = 2;

	cmd_msg->type[3] = 0x39;
	cmd_msg->tx_buf[3] = key2_off_tb;
	cmd_msg->tx_len[3] = 3;

	pr_info("%s+,elvss_tb = %d\n", __func__, en);

	ret = mtk_ddic_dsi_send_cmd(cmd_msg, true, false);
	if (ret != 0) {
		pr_err("%s mtk_ddic_dsi_send_cmd error\n", __func__);
	}

	vfree(cmd_msg);
	pr_info("%s-\n", __func__);

	return;
}

static int panel_hbm_fod_control(struct drm_panel *panel, bool en)
{
	struct lcm *ctx = panel_to_lcm(panel);

	struct mtk_ddic_dsi_msg *cmd_msg =
			vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	int ret;
	char hbm_tb[] = {0x53, 0xe0};
	char bl_tb[] = {0x51, 0x07, 0xff};

	if (unlikely(!cmd_msg)) {
		pr_err("%s vmalloc mtk_ddic_dsi_msg is failed, return\n", __func__);
		return 0;
	}
	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	if (en) {
		hbm_tb[1] = 0xe0;
	}
	else {
		bl_tb[1] = bl_tb0[1];
		bl_tb[2] = bl_tb0[2];
		hbm_tb[1] = 0x20;
	}

	cmd_msg->channel = 0;
	cmd_msg->flags = 0;
	cmd_msg->tx_cmd_num = 2;

	cmd_msg->type[0] = 0x15;
	cmd_msg->tx_buf[0] = hbm_tb;
	cmd_msg->tx_len[0] = 2;

	cmd_msg->type[1] = 0x39;
	cmd_msg->tx_buf[1] = bl_tb;
	cmd_msg->tx_len[1] = 3;

	pr_info("%s+,hbm_tb = 0x%x, high_bl = 0x%x, low_bl = 0x%x \n", __func__, hbm_tb[1], bl_tb[1], bl_tb[2]);

	ret = mtk_ddic_dsi_send_cmd(cmd_msg, true, false);
	if (ret != 0) {
		pr_err("%s mtk_ddic_dsi_send_cmd error\n", __func__);
	}

	if (!en)
		ctx->set_backlight_time = ktime_add_ms(ktime_get(), 80);

	vfree(cmd_msg);
	pr_info("%s-\n", __func__);

	return 0;
}

static void panel_hbm_get_state(struct drm_panel *panel, bool *state)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_debug("%s+ \n", __func__);
	*state = ctx->hbm_en;
	pr_debug("%s-\n", __func__);
}

static void panel_hbm_set_state(struct drm_panel *panel, bool *state)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_debug("%s+ \n", __func__);
	ctx->hbm_en = *state;
	pr_debug("%s-hbm_en = %d\n", __func__, ctx->hbm_en);
}

static void panel_hbm_get_wait_state(struct drm_panel *panel, bool *wait)
{
	struct lcm *ctx = panel_to_lcm(panel);

	*wait = ctx->hbm_wait;
	pr_debug("%s-\n", __func__);
}

static bool panel_hbm_set_wait_state(struct drm_panel *panel, bool wait)
{
	struct lcm *ctx = panel_to_lcm(panel);
	bool old = ctx->hbm_wait;
	pr_debug("%s+ \n", __func__);
	ctx->hbm_wait = wait;
	pr_debug("%s-\n", __func__);
	return old;
}

static void panel_hbm_need_delay(struct drm_panel *panel, bool *state)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_debug("%s+ \n", __func__);

	if (ktime_after(ktime_get(), ctx->set_backlight_time)) {
		*state = false;
	} else {
		*state = true;
	}
	pr_debug("%s-\n", __func__);
}

static void panel_get_hbm_solution(struct drm_panel *panel, int *solution)
{
	pr_debug("%s+ solution 1 \n", __func__);
	*solution = 1;
}

static void panel_id_get(struct drm_panel *panel)
{
	unsigned int j = 0;
	unsigned int ret_dlen = 0;
	int ret;
	struct drm_connector *connector = panel->connector;
	struct mtk_ddic_dsi_msg *cmd_msg =
		vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	u8 tx[10] = {0};

	if (unlikely(!cmd_msg)) {
		pr_err("%s vmalloc mtk_ddic_dsi_msg is failed, return\n", __func__);
		return;
	}

	connector->panel_id = 0;
	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));
	/* Read 0xDA = 0x1 */
	cmd_msg->channel = 0;
	cmd_msg->tx_cmd_num = 1;
	cmd_msg->type[0] = 0x06;
	tx[0] = 0xDA;
	cmd_msg->tx_buf[0] = tx;
	cmd_msg->tx_len[0] = 1;

	cmd_msg->rx_cmd_num = 1;
	cmd_msg->rx_buf[0] = vmalloc(4 * sizeof(unsigned char));
	memset(cmd_msg->rx_buf[0], 0, 4);
	cmd_msg->rx_len[0] = 1;
	ret = mtk_ddic_dsi_read_cmd(cmd_msg);
	if (ret != 0) {
		DDPPR_ERR("%s error\n", __func__);
		goto  done;
	}

	ret_dlen = cmd_msg->rx_len[0];
	DDPMSG("read lcm addr:0x%x--dlen:%d\n",
		*(char *)(cmd_msg->tx_buf[0]), ret_dlen);
	for (j = 0; j < ret_dlen; j++) {
		DDPMSG("read lcm addr:0x%x--byte:%d,val:0x%x\n",
			*(char *)(cmd_msg->tx_buf[0]), j,
			*(char *)(cmd_msg->rx_buf[0] + j));
	}
	connector->panel_id = *(unsigned char *)cmd_msg->rx_buf[0];
done:
	vfree(cmd_msg->rx_buf[0]);
	vfree(cmd_msg);

	DDPMSG("%s end -\n", __func__);
}

static void panel_set_crc_off(struct drm_panel *panel)
{
	int ret;
	char level_key_enable_tb[] = {0xF0, 0x5A, 0x5A};
	char level_key_disable_tb[] = {0xF0, 0xA5, 0xA5};
	char crc_enable_tb[] = {0x80, 0x01};
	char crc_bypass_tb[] = {0xB1, 0x01};
	char flat_gamma_tb[] = {0xB0, 0x01, 0xB8};
	char flat_gamma_value_tb[] = {0xB8, 0x09};
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
	cmd_msg->tx_cmd_num = 6;

	cmd_msg->type[0] = 0x39;
	cmd_msg->tx_buf[0] = level_key_enable_tb;
	cmd_msg->tx_len[0] = 3;

	cmd_msg->type[1] = 0x15;
	cmd_msg->tx_buf[1] = crc_enable_tb;
	cmd_msg->tx_len[1] = 2;

	cmd_msg->type[2] = 0x15;
	cmd_msg->tx_buf[2] = crc_bypass_tb;
	cmd_msg->tx_len[2] = 2;

	cmd_msg->type[3] = 0x39;
	cmd_msg->tx_buf[3] = flat_gamma_tb;
	cmd_msg->tx_len[3] = 3;

	cmd_msg->type[4] = 0x15;
	cmd_msg->tx_buf[4] = flat_gamma_value_tb;
	cmd_msg->tx_len[4] = 2;

	cmd_msg->type[5] = 0x39;
	cmd_msg->tx_buf[5] = level_key_disable_tb;
	cmd_msg->tx_len[5] = 3;

	ret = mtk_ddic_dsi_send_cmd(cmd_msg, true, false);
	if (ret != 0) {
		DDPPR_ERR("%s error\n", __func__);
		goto  done;
	}

done:
	vfree(cmd_msg);

	pr_info("%s end -\n", __func__);
}

static void panel_set_crc_srgb(struct drm_panel *panel)
{
	int ret;
	char level_key_enable_tb[] = {0xF0, 0x5A, 0x5A};
	char level_key_disable_tb[] = {0xF0, 0xA5, 0xA5};
	char crc_enable_tb[] = {0x80, 0x90};
	char crc_bypass_tb[] = {0xB1, 0x00};
	char crc_srgb_offset_tb[] = {0xB0, 0x01, 0xB1};
	char crc_srgb_tb[] = {0xB1, 0xA3, 0x00, 0x04, 0x3C, 0xC1, 0x15, 0x08, 0x05, 0xAB,
					0x4C, 0xE0, 0xCF, 0xC2, 0x06, 0xBD, 0xE5, 0xD3, 0x1D, 0xFF, 0xED, 0xE0};
	char flat_gamma_tb[] = {0xB0, 0x01, 0xB8};
	char flat_gamma_value_tb[] = {0xB8, 0x09};
	struct mtk_ddic_dsi_msg *cmd_msg =
		vmalloc(sizeof(struct mtk_ddic_dsi_msg));

	pr_info("%s begin +\n", __func__);
	if (unlikely(!cmd_msg)) {
		pr_err("%s vmalloc mtk_ddic_dsi_msg is failed, return\n", __func__);
		return;
	}
	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	cmd_msg->channel = 0;
	cmd_msg->flags = 1;
	cmd_msg->tx_cmd_num = 8;

	cmd_msg->type[0] = 0x15;
	cmd_msg->tx_buf[0] = crc_enable_tb;
	cmd_msg->tx_len[0] = 2;

	cmd_msg->type[1] = 0x39;
	cmd_msg->tx_buf[1] = level_key_enable_tb;
	cmd_msg->tx_len[1] = 3;

	cmd_msg->type[2] = 0x15;
	cmd_msg->tx_buf[2] = crc_bypass_tb;
	cmd_msg->tx_len[2] = 2;

	cmd_msg->type[3] = 0x39;
	cmd_msg->tx_buf[3] = crc_srgb_offset_tb;
	cmd_msg->tx_len[3] = 3;

	cmd_msg->type[4] = 0x39;
	cmd_msg->tx_buf[4] = crc_srgb_tb;
	cmd_msg->tx_len[4] = ARRAY_SIZE(crc_srgb_tb);

	cmd_msg->type[5] = 0x39;
	cmd_msg->tx_buf[5] = flat_gamma_tb;
	cmd_msg->tx_len[5] = 3;

	cmd_msg->type[6] = 0x15;
	cmd_msg->tx_buf[6] = flat_gamma_value_tb;
	cmd_msg->tx_len[6] = 2;

	cmd_msg->type[7] = 0x39;
	cmd_msg->tx_buf[7] = level_key_disable_tb;
	cmd_msg->tx_len[7] = 3;

	ret = mtk_ddic_dsi_send_cmd(cmd_msg, true, false);
	if (ret != 0) {
		DDPPR_ERR("%s error\n", __func__);
		goto  done;
	}

done:
	vfree(cmd_msg);

	pr_info("%s end -\n", __func__);
}

static void panel_set_crc_p3(struct drm_panel *panel)
{
	int ret;
	char level_key_enable_tb[] = {0xF0, 0x5A, 0x5A};
	char level_key_disable_tb[] = {0xF0, 0xA5, 0xA5};
	char crc_enable_tb[] = {0x80, 0x91};
	char crc_bypass_tb[] = {0xB1, 0x00};
	char crc_p3_offset_tb[] = {0xB0, 0x16, 0xB1};
	char crc_p3_tb[] = {0xB1, 0xD7, 0x00, 0x00, 0x0F, 0xC9, 0x02, 0x0A, 0x06, 0xC2,
					0x1D, 0xF1, 0xDC, 0xFC, 0x00, 0xE1, 0xEC, 0xDA, 0x03, 0xFF, 0xFF, 0xFF};
	//char crc_setting_offset_tb[] = {0xB0, 0x55, 0xB1};
	//char crc_setting_tb[] = {0xB1, 0x80};
	char flat_gamma_tb[] = {0xB0, 0x01, 0xB8};
	char flat_gamma_value_tb[] = {0xB8, 0x09};
	struct mtk_ddic_dsi_msg *cmd_msg =
		vmalloc(sizeof(struct mtk_ddic_dsi_msg));

	pr_info("%s begin +\n", __func__);
	if (unlikely(!cmd_msg)) {
		pr_err("%s vmalloc mtk_ddic_dsi_msg is failed, return\n", __func__);
		return;
	}
	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	cmd_msg->channel = 0;
	cmd_msg->flags = 1;
	cmd_msg->tx_cmd_num = 8;

	cmd_msg->type[0] = 0x15;
	cmd_msg->tx_buf[0] = crc_enable_tb;
	cmd_msg->tx_len[0] = 2;

	cmd_msg->type[1] = 0x39;
	cmd_msg->tx_buf[1] = level_key_enable_tb;
	cmd_msg->tx_len[1] = 3;

	cmd_msg->type[2] = 0x15;
	cmd_msg->tx_buf[2] = crc_bypass_tb;
	cmd_msg->tx_len[2] = 2;

	cmd_msg->type[3] = 0x39;
	cmd_msg->tx_buf[3] = crc_p3_offset_tb;
	cmd_msg->tx_len[3] = 3;

	cmd_msg->type[4] = 0x39;
	cmd_msg->tx_buf[4] = crc_p3_tb;
	cmd_msg->tx_len[4] = ARRAY_SIZE(crc_p3_tb);

	cmd_msg->type[5] = 0x39;
	cmd_msg->tx_buf[5] = flat_gamma_tb;
	cmd_msg->tx_len[5] = 3;

	cmd_msg->type[6] = 0x15;
	cmd_msg->tx_buf[6] = flat_gamma_value_tb;
	cmd_msg->tx_len[6] = 2;

	cmd_msg->type[7] = 0x39;
	cmd_msg->tx_buf[7] = level_key_disable_tb;
	cmd_msg->tx_len[7] = 3;

	ret = mtk_ddic_dsi_send_cmd(cmd_msg, true, false);
	if (ret != 0) {
		DDPPR_ERR("%s error\n", __func__);
		goto  done;
	}

done:
	vfree(cmd_msg);

	pr_info("%s end -\n", __func__);
}

static void panel_set_crc_p3_d65(struct drm_panel *panel)
{
	int ret;
	char level_key_enable_tb[] = {0xF0, 0x5A, 0x5A};
	char level_key_disable_tb[] = {0xF0, 0xA5, 0xA5};
	char crc_enable_tb[] = {0x80, 0x91};
	char crc_bypass_tb[] = {0xB1, 0x00};
	char crc_p3_offset_tb[] = {0xB0, 0x16, 0xB1};
	char crc_p3_tb[] = {0xB1, 0xD7, 0x00, 0x00, 0x0F, 0xC9, 0x02, 0x0A, 0x06, 0xC2,
					0x1D, 0xF1, 0xDC, 0xFC, 0x00, 0xE1, 0xEC, 0xDA, 0x03, 0xFF, 0xED, 0xE0};
	//char crc_setting_offset_tb[] = {0xB0, 0x55, 0xB1};
	//char crc_setting_tb[] = {0xB1, 0x80};
	char flat_gamma_tb[] = {0xB0, 0x01, 0xB8};
	char flat_gamma_value_tb[] = {0xB8, 0x09};
	struct mtk_ddic_dsi_msg *cmd_msg =
		vmalloc(sizeof(struct mtk_ddic_dsi_msg));

	pr_info("%s begin +\n", __func__);
	if (unlikely(!cmd_msg)) {
		pr_err("%s vmalloc mtk_ddic_dsi_msg is failed, return\n", __func__);
		return;
	}
	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	cmd_msg->channel = 0;
	cmd_msg->flags = 1;
	cmd_msg->tx_cmd_num = 8;

	cmd_msg->type[0] = 0x15;
	cmd_msg->tx_buf[0] = crc_enable_tb;
	cmd_msg->tx_len[0] = 2;

	cmd_msg->type[1] = 0x39;
	cmd_msg->tx_buf[1] = level_key_enable_tb;
	cmd_msg->tx_len[1] = 3;

	cmd_msg->type[2] = 0x15;
	cmd_msg->tx_buf[2] = crc_bypass_tb;
	cmd_msg->tx_len[2] = 2;

	cmd_msg->type[3] = 0x39;
	cmd_msg->tx_buf[3] = crc_p3_offset_tb;
	cmd_msg->tx_len[3] = 3;

	cmd_msg->type[4] = 0x39;
	cmd_msg->tx_buf[4] = crc_p3_tb;
	cmd_msg->tx_len[4] = ARRAY_SIZE(crc_p3_tb);

	cmd_msg->type[5] = 0x39;
	cmd_msg->tx_buf[5] = flat_gamma_tb;
	cmd_msg->tx_len[5] = 3;

	cmd_msg->type[6] = 0x15;
	cmd_msg->tx_buf[6] = flat_gamma_value_tb;
	cmd_msg->tx_len[6] = 2;

	cmd_msg->type[7] = 0x39;
	cmd_msg->tx_buf[7] = level_key_disable_tb;
	cmd_msg->tx_len[7] = 3;

	ret = mtk_ddic_dsi_send_cmd(cmd_msg, true, false);
	if (ret != 0) {
		DDPPR_ERR("%s error\n", __func__);
		goto  done;
	}

done:
	vfree(cmd_msg);

	pr_info("%s end -\n", __func__);
}

static struct mtk_panel_params ext_params = {
	.data_rate = 1098,//1098,
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x00,
		.count = 1,
		.para_list[0] = 0x24,
	},
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.hbm_en_pre_time = 0,
	.hbm_en_post_time = 4,
	.hbm_dis_pre_time = 0,
	.hbm_dis_post_time = 0,
	.icon_en_time = 2,
	.icon_dis_time = 0,
	.ssc_range = 4,
	.ssc_disable = 0,
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en=1,
	.corner_pattern_height=ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot=ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size=sizeof(top_rc_pattern),
	.corner_pattern_lt_addr=(void*)top_rc_pattern,
#endif
	.dc_backlight_threhold = 564,
};

static int panel_get_panel_info(struct drm_panel *panel, char *buf)
{
	int count = 0;
	struct lcm *ctx = panel_to_lcm(panel);

	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}

	count = snprintf(buf, PAGE_SIZE, "%s\n", ctx->panel_info);

	return count;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.hbm_set_cmdq = panel_hbm_set_cmdq,
	.hbm_fod_control = panel_hbm_fod_control,
	.normal_hbm_control = panel_hbm_fod_control,
	.hbm_get_state = panel_hbm_get_state,
	.hbm_set_state = panel_hbm_set_state,
	.hbm_get_wait_state = panel_hbm_get_wait_state,
	.hbm_set_wait_state = panel_hbm_set_wait_state,
	.setbacklight_control = lcm_setbacklight_control,
	.panel_id_get = panel_id_get,
	.get_panel_info = panel_get_panel_info,
	.panel_set_crc_off = panel_set_crc_off,
	.panel_set_crc_srgb = panel_set_crc_srgb,
	.panel_set_crc_p3 = panel_set_crc_p3,
	.panel_set_crc_p3_d65 = panel_set_crc_p3_d65,
	.panel_dimming_control = panel_dimming_control,
	.hbm_need_delay = panel_hbm_need_delay,
	.panel_elvss_control = panel_elvss_control,
	.get_hbm_solution = panel_get_hbm_solution,
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
	pr_info("%s+ \n", __func__);
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

	panel->connector->display_info.width_mm = 71;
	panel->connector->display_info.height_mm = 153;
	pr_info("%s-\n", __func__);
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

	ctx->pm_enable_gpio = devm_gpiod_get(dev, "pm-enable", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->pm_enable_gpio)) {
		dev_err(dev, "cannot get pm-enable-gpios %ld\n",
			PTR_ERR(ctx->pm_enable_gpio));
		return PTR_ERR(ctx->pm_enable_gpio);
	}

	ctx->prepared = true;
	ctx->enabled = true;
	ctx->panel_info = panel_name;

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
	{ .compatible = "s6e8fc01,lcm", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "s6e8fc01,lcm",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);
