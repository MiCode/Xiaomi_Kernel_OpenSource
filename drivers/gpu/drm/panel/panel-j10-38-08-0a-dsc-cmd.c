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
#include <linux/delay.h>
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
#include <linux/workqueue.h>

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
#include "../mediatek/mtk_corner_pattern/mtk_hw_roundedpattern_j10_38_08_0a.h"
#endif
#include "panel.h"

#define REGFLAG_CMD			0xFFFA
#define REGFLAG_DELAY			0xFFFC
#define REGFLAG_UDELAY		0xFFFB
#define REGFLAG_END_OF_TABLE	0xFFFD

/* enable this to check panel self -bist pattern */
/* #define PANEL_BIST_PATTERN */

/* option function to read data from some panel address */
/* #define PANEL_SUPPORT_READBACK */

#define DC_BACKLIGHT_LEVEL 486
#define PANEL_DIMMINGON_DELAY 120

struct mtk_ddic_dsi_msg cmd_msg = {0};

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static char bl_tb0[] = {0x51, 0x3, 0xff};
char *panel_name = "panel_name=dsi_j10_38_08_0a_dsc_cmd";

#define lcm_dcs_write_seq(ctx, seq...)                                     \
	({                                                                     \
		const u8 d[] = {seq};                                          \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		lcm_dcs_write(ctx, d, ARRAY_SIZE(d));                      \
	})

#define lcm_dcs_write_seq_static(ctx, seq...)                              \
	({                                                                     \
		static const u8 d[] = {seq};                                   \
		lcm_dcs_write(ctx, d, ARRAY_SIZE(d));                      \
	})

static void panel_dimming_on_delayed_work(struct work_struct* work);

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
		ret = lcm_dcs_read(ctx, 0x0A, buffer, 1);
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

static void push_panel_table(struct LCM_setting_table *table, struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle, int len)
{
	unsigned int i = 0;
	pr_info("%s +\n", __func__);

	for (i = 0; i < len; i++) {
		unsigned int cmd;

		cmd = table[i].cmd;
		switch (cmd) {
		case REGFLAG_DELAY:
			msleep(table[i].count);
			break;
		case REGFLAG_UDELAY:
			udelay(table[i].count);
			break;
		case REGFLAG_END_OF_TABLE:
			break;
		default:
			cb(dsi, handle, table[i].para_list,
				table[i].count);
		}
	}
	pr_info("%s -\n", __func__);
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

static struct regulator *disp_vci;

static int lcm_panel_vci_regulator_init(struct device *dev)
{
       static int regulator_inited;
       int ret = 0;

       if (regulator_inited)
               return ret;

       /* please only get regulator once in a driver */
       disp_vci = regulator_get(dev, "vibr");
       if (IS_ERR(disp_vci)) { /* handle return value */
               ret = PTR_ERR(disp_vci);
               pr_err("get disp_vci fail, error: %d\n", ret);
               return ret;
       }

       regulator_inited = 1;
       return ret; /* must be 0 */

}

static int lcm_panel_vci_enable(struct device *dev)
{
       int ret = 0;
       int retval = 0;
       int status = 0;

       pr_info("%s +\n",__func__);

       lcm_panel_vci_regulator_init(dev);

       /* set voltage with min & max*/
       ret = regulator_set_voltage(disp_vci, 3000000, 3000000);
       if (ret < 0)
               pr_err("set voltage disp_vci fail, ret = %d\n", ret);
       retval |= ret;

       status = regulator_is_enabled(disp_vci);
       pr_info("%s regulator_is_enabled=%d\n",__func__,status);
       if(!status){
               /* enable regulator */
               ret = regulator_enable(disp_vci);
               if (ret < 0)
                       pr_err("enable regulator disp_vci fail, ret = %d\n", ret);
               retval |= ret;
       }

       pr_info("%s -\n",__func__);
       return retval;
}
static unsigned int start_up = 1;
static int lcm_panel_vci_disable(struct device *dev)
{
       int ret = 0;
       int retval = 0;
       pr_info("%s +\n",__func__);

       lcm_panel_vci_regulator_init(dev);

       //status = regulator_is_enabled(disp_vci);
       //pr_err("%s regulator_is_enabled=%d\n",__func__,status);
       //if(!status){
       if(start_up) {
               pr_info("%s enable regulator\n",__func__);
               /* enable regulator */
               ret = regulator_enable(disp_vci);
               if (ret < 0)
                       pr_err("enable regulator disp_vci fail, ret = %d\n", ret);
               retval |= ret;

               start_up = 0;
       }
       //}

       ret = regulator_disable(disp_vci);
       if (ret < 0)
               pr_err("disable regulator disp_vci fail, ret = %d\n", ret);
       retval |= ret;

       pr_info("%s -\n",__func__);

       return retval;
}

static void lcm_panel_init(struct lcm *ctx)
{
	pr_info("%s \n",__func__);
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	msleep(10);
	gpiod_set_value(ctx->reset_gpio, 0);
	msleep(1);
	gpiod_set_value(ctx->reset_gpio, 1);
	msleep(10);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	/*sleep out*/
	lcm_dcs_write_seq_static(ctx, 0x11);
	msleep(10);

	/* TE sync on*/
 	lcm_dcs_write_seq_static(ctx, 0x35, 0x00);

 	/*PPS Seting*/
	lcm_dcs_write_seq_static(ctx, 0x9D, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x9E, 0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x09, 0x60,
										0x04, 0x38, 0x00, 0x08, 0x02, 0x1C, 0x02, 0x1C,
										0x02, 0x00, 0x02, 0x0E, 0x00, 0x20, 0x00, 0xBB,
										0x00, 0x07, 0x00, 0x0C, 0x0D, 0xB7, 0x0C, 0xB7,
										0x18, 0x00, 0x10, 0xF0, 0x03, 0x0C, 0x20, 0x00,
										0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38,
										0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7B,
										0x7D, 0x7E, 0x01, 0x02, 0x01, 0x00, 0x09, 0x40,
										0x09, 0xBE, 0x19, 0xFC, 0x19, 0xFA, 0x19, 0xF8,
										0x1A, 0x38, 0x1A, 0x78, 0x1A, 0xB6, 0x2A, 0xF6,
										0x2B, 0x34, 0x2B, 0x74, 0x3B, 0x74, 0x6B, 0xF4,
										0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
										0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
										0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
										0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
										0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 );

 	/* PAGE ADDRESS SET*/
 	lcm_dcs_write_seq_static(ctx, 0x2A, 0x00,0x00,0x04,0x37);
	lcm_dcs_write_seq_static(ctx, 0x2B, 0x00,0x00,0x09,0x5F);

	/* 11bit dimming setting */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xB7, 0x4F);
 	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

	/* Err FG Set */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xEC, 0x00,0xC2,0xC2,0x42);
 	lcm_dcs_write_seq_static(ctx, 0xB0, 0x0D);
	lcm_dcs_write_seq_static(ctx, 0xEC, 0x19);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x06);
	lcm_dcs_write_seq_static(ctx, 0xE4, 0x10);
 	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

	/* TE frequency change(to 121.4Hz/60.7hz) */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xFC, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x16);
	lcm_dcs_write_seq_static(ctx, 0xD1, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xFC, 0xA5, 0xA5);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

	/* Ripple improvement setting */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x36);
	lcm_dcs_write_seq_static(ctx, 0xD3, 0x0F);
	lcm_dcs_write_seq_static(ctx, 0xF7, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

	/* OFC setting(MIPI 820Mbps OSC 91.8MHz) */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xFC, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xE4, 0xDF, 0x4C, 0xA2);
	lcm_dcs_write_seq_static(ctx, 0xE9, 0x11, 0x75, 0xDF, 0x4C, 0xA2, 0xA6, 0x37, 0xBE,
										 0xA6, 0x37, 0xBE, 0x00, 0x32, 0x32);
	lcm_dcs_write_seq_static(ctx, 0xFC, 0xA5, 0xA5);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
	msleep(80);

	/* Frequency Select */
	pr_info("%s ctx->high_refresh_rate = %d\n", __func__, ctx->high_refresh_rate);
	if (ctx->high_refresh_rate)
		lcm_dcs_write_seq_static(ctx, 0x60, 0x10, 0x00);
	else
		lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x00);

 	/* Dimming Setting*/
 	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
 	lcm_dcs_write_seq_static(ctx, 0xB0, 0x06);
	lcm_dcs_write_seq_static(ctx, 0xB7, 0x10);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x05);
	lcm_dcs_write_seq_static(ctx, 0xB7, 0x13);
 	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
 	/* HBM Mode OFF(Normal Mode)*/
	lcm_dcs_write_seq_static(ctx, 0x53, 0x20, 0x00);
 	/* Normal Mode Backlight Setting*/
	lcm_dcs_write_seq_static(ctx, 0x51, 0x00,0x00);

 	/* ACL Mode(ACL OFF) */
 	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
 	lcm_dcs_write_seq_static(ctx, 0x55, 0x00);
 	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
 	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
 	lcm_dcs_write_seq_static(ctx, 0xB0, 0xD9);
 	lcm_dcs_write_seq_static(ctx, 0xBB, 0x00);
 	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

	/* CRC SRGB/P3 Setting */
	lcm_dcs_write_seq_static(ctx, 0x81, 0x92);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB1, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x01, 0xB1);
	lcm_dcs_write_seq_static(ctx, 0xB1, 0x9C, 0x07, 0x04, 0x3E, 0xC1, 0x13, 0x07, 0x07, 0xA5,
								  0x52, 0xE2, 0xD3, 0xB9, 0x11, 0xBD, 0xE4, 0xD8, 0x1B, 0xFF, 0xF5, 0xE0);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x2B, 0xB1);
	lcm_dcs_write_seq_static(ctx, 0xB1, 0xB7, 0x00, 0x00, 0x15, 0xC2, 0x00, 0x08, 0x08, 0xB9,
								  0x26, 0xEE, 0xDB, 0xDC, 0x05, 0xCC, 0xE4, 0xD7, 0x00, 0xFF, 0xFF, 0xFF);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

	 /*Display on*/
 	lcm_dcs_write_seq_static(ctx, 0x29);
	pr_info("%s -\n", __func__);
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
	ctx->skip_dimmingon = STATE_NONE;

	return 0;
}

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->prepared)
		return 0;

	pr_info("%s\n", __func__);
	if (ctx->hbm_en)
		lcm_dcs_write_seq_static(ctx, 0x53, 0x20, 0x00);
	msleep(20);
	lcm_dcs_write_seq_static(ctx, 0x28);
	msleep(10);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(120);

	ctx->bias_gpio = devm_gpiod_get(ctx->dev, "bias", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->bias_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->bias_gpio);

	lcm_panel_vci_disable(ctx->dev);

	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	ctx->error = 0;
	ctx->prepared = false;
	ctx->hbm_en = false;
	ctx->in_aod = false;
	ctx->doze_enable = false;
	ctx->crc_state = CRC_P3;
	atomic_set(&fod_hbm_on_flag, 0);

	mtk_clear_fod_status(panel->connector);
	//ctx->doze_brightness_state = DOZE_TO_NORMAL;

#ifdef CONFIG_HWCONF_MANAGER
		/* add for display fps cpunt */
		dsi_panel_fps_count(panel, 0, 0);
		/* add for display state count*/
		if (atomic_read(&panel_active_count_enable))
			dsi_panel_state_count(panel, 0);
		/* add for display hbm count */
		dsi_panel_HBM_count(panel, 0, 1);
#endif

	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	if (ctx->prepared)
		return 0;

	pr_info("%s\n", __func__);
	lcm_panel_vci_enable(ctx->dev);
	ctx->bias_gpio = devm_gpiod_get(ctx->dev, "bias", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->bias_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_gpio);
	lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;
	ctx->hbm_en = false;
	ctx->in_aod = false;
	ctx->doze_enable = false;
	ctx->crc_state = CRC_P3;
	//ctx->doze_brightness_state = DOZE_TO_NORMAL;
	atomic_set(&fod_hbm_on_flag, 0);

#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif

#ifdef CONFIG_HWCONF_MANAGER
	/* add for display fps cpunt */
	dsi_panel_fps_count(panel, 0, 1);
	/* add for display state count*/
	if (!atomic_read(&panel_active_count_enable))
		dsi_panel_state_count(panel, 1);
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
	ctx->skip_dimmingon = STATE_NONE;

	return 0;
}

static const struct drm_display_mode default_mode = {
	.clock = 163530,
	.hdisplay = 1080,
	.hsync_start = 1080 + 40,
	.hsync_end = 1080 + 40 + 10,
	.htotal = 1080 + 40 + 10 + 20,
	.vdisplay = 2400,
	.vsync_start = 2400 + 20,
	.vsync_end = 2400 + 20 + 2,
	.vtotal = 2400 + 20 + 2 + 8,
	.vrefresh = 60,
};

static const struct drm_display_mode performance_mode = {
	.clock = 327060,
	.hdisplay = 1080,
	.hsync_start = 1080 + 40,
	.hsync_end = 1080 + 40 + 10,
	.htotal = 1080 + 40 + 10 + 20,
	.vdisplay = 2400,
	.vsync_start = 2400 + 20,
	.vsync_end = 2400 + 20 + 2,
	.vtotal = 2400 + 20 + 2 + 8,
	.vrefresh = 120,
};

#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params = {
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {

			.cmd = 0x53, .count = 1, .para_list[0] = 0x00,
		},
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 69500,
	.physical_height_um = 154500,
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
		.bit_per_pixel = 128,
		.pic_height = 2400,
		.pic_width = 1080,
		.slice_height = 8,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 187,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 3511,
		.slice_bpg_offset = 3255,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		},
	.data_rate = 820,
	.hbm_en_pre_time = 1,
	.hbm_en_post_time = 0,
	.hbm_dis_pre_time = 1,
	.hbm_dis_post_time = 0,
	.icon_en_time = 3,
	.icon_dis_time = 0,
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en=1,
	.corner_pattern_height=ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot=ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size=sizeof(top_rc_pattern),
	.corner_pattern_lt_addr=(void*)top_rc_pattern,
#endif
	.dc_backlight_threhold = 486,
};

static struct mtk_panel_params ext_params_120hz = {
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {

			.cmd = 0x53, .count = 1, .para_list[0] = 0x24,
		},
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 69500,
	.physical_height_um = 154500,
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
		.bit_per_pixel = 128,
		.pic_height = 2400,
		.pic_width = 1080,
		.slice_height = 8,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 187,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 3511,
		.slice_bpg_offset = 3255,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		},
	.data_rate = 820,
	.hbm_en_pre_time = 1,
	.hbm_en_post_time = 1,
	.hbm_dis_pre_time = 1,
	.hbm_dis_post_time = 1,
	.icon_en_time = 2,
	.icon_dis_time = 0,
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
        .round_corner_en=1,
        .corner_pattern_height=ROUND_CORNER_H_TOP,
        .corner_pattern_height_bot=ROUND_CORNER_H_BOT,
        .corner_pattern_tp_size=sizeof(top_rc_pattern),
        .corner_pattern_lt_addr=(void*)top_rc_pattern,
#endif
	.dc_backlight_threhold = 486,
};

static int lcm_setbacklight_cmdq(void *dsi,
		dcs_write_gce cb, void *handle, unsigned int level)
{
	bl_tb0[1] = (level >> 8) & 0x7;
	bl_tb0[2] = level & 0xFF;
	pr_info("%s+ \n", __func__);
	pr_info("%s level = %d\n",  __func__, level);

	if (!cb)
		return -1;

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
	pr_info("%s-\n", __func__);
	return 0;
}

static int panel_hbm_set_cmdq(struct drm_panel *panel, void *dsi,
			      dcs_write_gce cb, void *handle, bool en)
{
	char hbm_tb[] = {0x53, 0xe0, 0x00};
	char bl_tb[] = {0x51, 0x07, 0xff};
	struct lcm *ctx = panel_to_lcm(panel);

	if (!cb)
		return -1;

	if (ctx->hbm_en == en) {
		pr_info("%s-skip hbm set\n", __func__);
		goto done;
	}

	if (en) {
		hbm_tb[1] = 0xe0;
		//ctx->unset_doze_brightness = ctx->doze_brightness_state;
		//ctx->doze_brightness_state = DOZE_TO_NORMAL;
		//pr_info("set unset_doze_brightness = %d", ctx->unset_doze_brightness);
		ctx->skip_dimmingon = STATE_DIM_BLOCK;

		atomic_set(&fod_hbm_on_flag, 1);
	} else {
		bl_tb[1] = bl_tb0[1];
		bl_tb[2] = bl_tb0[2];
		hbm_tb[1] = 0x20;

		if (ctx->doze_enable) {
			if (ctx->doze_brightness_state == DOZE_BRIGHTNESS_HBM)
				hbm_tb[1] = 0x22;
			if (ctx->doze_brightness_state == DOZE_BRIGHTNESS_LBM)
				hbm_tb[1] = 0x23;
			pr_info("%s hbm off in doze, ctx->doze_brightness_state = %d\n",
					__func__, ctx->doze_brightness_state);
		} else {
			ctx->skip_dimmingon = STATE_DIM_RESTORE;
		}

		atomic_set(&fod_hbm_on_flag, 0);
	}
	pr_info("%s+, hbm: %s\n", __func__, en ? "OFF->ON":"ON->OFF");

	cb(dsi, handle, hbm_tb, ARRAY_SIZE(hbm_tb));
	cb(dsi, handle, bl_tb, ARRAY_SIZE(bl_tb));

	if (!en) {
		ctx->set_backlight_time = ktime_add_ms(ktime_get(), 80);
		schedule_delayed_work(&ctx->dimmingon_delayed_work,
					msecs_to_jiffies(PANEL_DIMMINGON_DELAY));
		if(ctx->skip_dimmingon == STATE_DIM_RESTORE)
			ctx->skip_dimmingon = STATE_NONE;
	} else {
		cancel_delayed_work(&ctx->dimmingon_delayed_work);
	}

	ctx->hbm_en = en;
	ctx->hbm_wait = true;

done:
	return 0;
}

static void panel_hbm_set_state(struct drm_panel *panel, bool *state);

static int lcm_setbacklight_control(struct drm_panel *panel, unsigned int level, bool fod_cal_flag)
{
	bool fod_backlight_flag;
	bool hbm_flag = 0;
	int ret;
	static unsigned int  last_backlight;
	struct lcm *ctx = panel_to_lcm(panel);

	last_backlight = (bl_tb0[1]  << 8) | bl_tb0[2];
	bl_tb0[1] = (level >> 8) & 0x7;
	bl_tb0[2] = level & 0xFF;
	if (!panel->connector) {
		pr_err("%s, the connector is null\n", __func__);
		return 0;
	}
	fod_backlight_flag = mtk_fod_backlight_flag(panel->connector);
	if (fod_cal_flag || !fod_backlight_flag) {
		cmd_msg.channel = 0;
		cmd_msg.flags = 0;
		cmd_msg.tx_cmd_num = 1;

		cmd_msg.type[0] = 0x39;
		cmd_msg.tx_buf[0] = bl_tb0;
		cmd_msg.tx_len[0] = 3;

		ret = mtk_ddic_dsi_send_cmd(&cmd_msg, false, false);
		if (ret != 0) {
			pr_err("%s mtk_ddic_dsi_send_cmd error\n", __func__);
		} else {
			panel_hbm_set_state(panel, &hbm_flag);
		}
		if(((last_backlight == 0 && !ctx->doze_enable) || ctx->skip_dimmingon == STATE_DIM_RESTORE) && level) {
			pr_info("%s,last_backlight = %d, doze_enable = %d, skip_dimmingon = %d\n", __func__, last_backlight, ctx->doze_enable, ctx->skip_dimmingon);
			schedule_delayed_work(&ctx->dimmingon_delayed_work,
					msecs_to_jiffies(PANEL_DIMMINGON_DELAY));

			if(ctx->skip_dimmingon == STATE_DIM_RESTORE)
				ctx->skip_dimmingon = STATE_NONE;
		}

		if (((!last_backlight) && level) || (last_backlight && (!level))) {
			pr_info("%s,fod_backlight_flag = %d, level = 0x%x, high_bl = 0x%x, low_bl = 0x%x \n", __func__, fod_backlight_flag, level, bl_tb0[1], bl_tb0[2]);
		}

		ctx->set_backlight_time = ktime_add_ms(ktime_get(), 267);
	}
	else {
		pr_info("%s,fod_backlight_flag = %d,  can't set backlight\n", __func__, fod_backlight_flag);
	}
	if (!fod_cal_flag && !(mtk_dc_flag(panel->connector) && DC_BACKLIGHT_LEVEL == level) && level != 1 && level != 2) {
		panel->connector->brightness_clone = level;
		sysfs_notify(&panel->connector->kdev->kobj, NULL, "brightness_clone");
	}
	return 0;
}

static struct LCM_setting_table panel_esd_recovery_in_doze_table[] = {
	/* display_off */
	{REGFLAG_CMD, 1, {0x28} },
	/* level2_key_enable */
	{REGFLAG_CMD, 3, {0xF0, 0x5A, 0x5A} },
	/* frame_60hz */
	{REGFLAG_CMD, 3, {0x60, 0x00, 0x00} },
	/* vfp_diff_off_bypass */
	{REGFLAG_CMD, 2, {0xB0, 0x0B} },
	/* vfp_diff_off_setting */
	{REGFLAG_CMD, 3, {0xD8, 0x50, 0x00} },
	/* aod_mode */
	{REGFLAG_CMD, 3, {0x53, 0x22, 0x00} },
	/* level2_key_disable */
	{REGFLAG_CMD, 3, {0xF0, 0xA5, 0xA5} },

	{REGFLAG_DELAY, 32, {} },
	/* display_on */
	{REGFLAG_CMD, 1, {0x29} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static int panel_esd_recovery_in_doze(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle, int doze_brightness)
{
	struct lcm *ctx = panel_to_lcm(panel);
	bool fod_backlight_flag;
	bool normal_hbm_flag;
	bool fod_hbm_flag;

	if (!panel) {
		pr_err("panel_aod_set_state invalid panel\n");
		return -EAGAIN;
	}

	pr_info("%s+ \n", __func__);

	if (!panel->connector) {
		pr_err("panel_aod_set_state invalid panel->connector\n");
		return -EAGAIN;
	}

	fod_backlight_flag = mtk_fod_backlight_flag(panel->connector);
	normal_hbm_flag = mtk_normal_hbm_flag(panel->connector);
	fod_hbm_flag = mtk_fod_hbm_flag(panel->connector);

	if (ctx->hbm_en || fod_backlight_flag || normal_hbm_flag || fod_hbm_flag) {
		pr_info("%s hbm en, can't set aod cmd\n", __func__);
		goto exit;
	}

	if (doze_brightness == DOZE_BRIGHTNESS_HBM)
		panel_esd_recovery_in_doze_table[5].para_list[1] = 0x22;
	else if (doze_brightness == DOZE_BRIGHTNESS_LBM)
		panel_esd_recovery_in_doze_table[5].para_list[1] = 0x23;
	pr_info("aod 0x53 = 0x%x\n", panel_esd_recovery_in_doze_table[5].para_list[1]);

	push_panel_table(panel_esd_recovery_in_doze_table, panel, dsi, cb, handle, ARRAY_SIZE(panel_esd_recovery_in_doze_table));
	ctx->doze_enable = true;
	ctx->skip_dimmingon = STATE_DIM_BLOCK;

exit:
	pr_info("%s-\n", __func__);

	return 0;
}

static void panel_esd_restore_backlight(struct drm_panel *panel,
		void *dsi_drv, dcs_write_gce cb, void *handle)
{
	struct lcm *ctx = panel_to_lcm(panel);
	const char *doze_brightness_str[] = {
		[DOZE_TO_NORMAL] = "DOZE_TO_NORMAL",
		[DOZE_BRIGHTNESS_HBM] = "DOZE_BRIGHTNESS_HBM",
		[DOZE_BRIGHTNESS_LBM] = "DOZE_BRIGHTNESS_LBM",
	};

	if (!panel->connector) {
		pr_err("%s, the connector is null\n", __func__);
		return ;
	}
	pr_info("%s doze_enable = %d, unset_doze_brightness = %d, doze_brightness_state = %d\n",
		__func__, ctx->doze_enable, ctx->unset_doze_brightness, ctx->doze_brightness_state);

	/* for esd recovery in aod */
	if (ctx->doze_enable) {
		pr_info("%s trigger in aod, save unset_doze_brightness = %s\n", __func__, doze_brightness_str[ctx->doze_brightness_state]);
		ctx->unset_doze_brightness = ctx->doze_brightness_state;
		ctx->doze_brightness_state = DOZE_TO_NORMAL;
		ctx->in_aod = true;
		panel_esd_recovery_in_doze(panel, dsi_drv, cb, handle, ctx->unset_doze_brightness);
		lcm_dcs_write(ctx, bl_tb0, ARRAY_SIZE(bl_tb0));
	} else {
		lcm_dcs_write(ctx, bl_tb0, ARRAY_SIZE(bl_tb0));
	}
	pr_info("%s high_bl = 0x%x, low_bl = 0x%x \n", __func__, bl_tb0[1], bl_tb0[2]);
}

static int mtk_panel_ext_param_set(struct drm_panel *panel,
			 unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;

	if (mode == 0)
		ext->params = &ext_params;
	else if (mode == 1)
		ext->params = &ext_params_120hz;
	else
		ret = 1;

	return ret;
}

static void mode_switch_60_to_120(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);
	ctx->high_refresh_rate = true;

	pr_info("%s +", __func__);
	if (stage == AFTER_DSI_POWERON) {
		/* Frequency Select 120HZ*/
		lcm_dcs_write_seq_static(ctx, 0x60, 0x10, 0x00);

#ifdef CONFIG_HWCONF_MANAGER
		/* add for display fps cpunt */
		dsi_panel_fps_count(panel, 120, 1);
#endif
	}
}

static void mode_switch_120_to_60(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);
	ctx->high_refresh_rate = false;

	pr_info("%s +", __func__);
	if (stage == AFTER_DSI_POWERON) {
		/* Frequency Select 60HZ*/
		lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x00);

#ifdef CONFIG_HWCONF_MANAGER
		/* add for display fps cpunt */
		dsi_panel_fps_count(panel, 60, 1);
#endif

	}
}

static int mode_switch(struct drm_panel *panel, unsigned int cur_mode,
		unsigned int dst_mode, enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	int ret = 0;

	if (cur_mode == 0 && dst_mode == 1) { /* 60 switch to 120 */
		mode_switch_60_to_120(panel, stage);
	} else if (cur_mode == 1 && dst_mode == 0) { /* 120 switch to 60 */
		mode_switch_120_to_60(panel, stage);
	} else {
		ret = 1;
	}

	return ret;
}

static void panel_set_display_on(struct drm_panel *panel)
{
	struct mtk_ddic_dsi_msg *cmd_msg =
			vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s+ display on\n", __func__);

	int ret = 0;
	char display_on_tb[] = {0x29};

	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	cmd_msg->channel = 0;
	cmd_msg->flags = 0;
	cmd_msg->tx_cmd_num = 1;

	cmd_msg->type[0] = 0x05;
	cmd_msg->tx_buf[0] = display_on_tb;
	cmd_msg->tx_len[0] = 1;

	ret = mtk_ddic_dsi_send_cmd(cmd_msg, true, false);
	if (ret != 0) {
		pr_err("%s mtk_ddic_dsi_send_cmd error\n", __func__);
	}

	vfree(cmd_msg);
}

static int panel_doze_aod_control(struct drm_panel *panel, bool is_aod_hbm)
{
	struct mtk_ddic_dsi_msg *cmd_msg =
			vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	struct lcm *ctx = panel_to_lcm(panel);
	bool fod_backlight_flag;
	bool normal_hbm_flag;
	bool fod_hbm_flag;
	bool hbm_flag = 0;

	int ret = 0;
	char aod_mode_tb[] = {0x53, 0x2A, 0x00};

	pr_info("%s+ is_aod_hbm = %d\n", __func__, is_aod_hbm);
	fod_backlight_flag = mtk_fod_backlight_flag(panel->connector);
	normal_hbm_flag = mtk_normal_hbm_flag(panel->connector);
	fod_hbm_flag = mtk_fod_hbm_flag(panel->connector);

	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	if (ctx->hbm_en || fod_backlight_flag || normal_hbm_flag || fod_hbm_flag) {
		pr_info("%s hbm en, can't set aod cmd\n", __func__);
		goto exit;
	}

	if (is_aod_hbm) {
		aod_mode_tb[1] = 0x2A;
	}
	else {
		aod_mode_tb[1] = 0x2B;
	}

	cmd_msg->channel = 0;
	cmd_msg->flags = 0;
	cmd_msg->tx_cmd_num = 1;

	cmd_msg->type[0] = 0x39;
	cmd_msg->tx_buf[0] = aod_mode_tb;
	cmd_msg->tx_len[0] = 3;

	ret = mtk_ddic_dsi_send_cmd(cmd_msg, true, true);
	if (ret != 0) {
		pr_err("%s mtk_ddic_dsi_send_cmd error line = %d\n", __func__, __LINE__);
	} else {
		panel_hbm_set_state(panel, &hbm_flag);
	}
	//msleep(40);

exit:
	panel_set_display_on(panel);
	vfree(cmd_msg);
	pr_info("%s-\n", __func__);

	return ret;
}

int panel_set_doze_brightness(struct drm_panel *panel, int doze_brightness)
{
	int rc = 0;
	bool fod_backlight_flag;
	bool normal_hbm_flag;
	bool fod_hbm_flag;
	const char *doze_brightness_str[] = {
		[DOZE_TO_NORMAL] = "DOZE_TO_NORMAL",
		[DOZE_BRIGHTNESS_HBM] = "DOZE_BRIGHTNESS_HBM",
		[DOZE_BRIGHTNESS_LBM] = "DOZE_BRIGHTNESS_LBM",
	};

	struct lcm *ctx = panel_to_lcm(panel);
	fod_backlight_flag = mtk_fod_backlight_flag(panel->connector);
	normal_hbm_flag = mtk_normal_hbm_flag(panel->connector);
	fod_hbm_flag = mtk_fod_hbm_flag(panel->connector);

	if (!ctx->prepared) {
		ctx->unset_doze_brightness = doze_brightness;
		pr_info("Panel not initialized! save unset_doze_brightness = %s\n",
				doze_brightness_str[ctx->unset_doze_brightness]);
		goto exit;
	}

	pr_info("%s +\n", __func__);
	if (ctx->hbm_en || fod_backlight_flag || normal_hbm_flag || fod_hbm_flag) {
		ctx->unset_doze_brightness = doze_brightness;
		if (ctx->unset_doze_brightness == DOZE_TO_NORMAL) {
			ctx->doze_brightness_state = DOZE_TO_NORMAL;
		}
		pr_info("fod_hbm_enabled set, save unset_doze_brightness = %s\n",
				doze_brightness_str[ctx->unset_doze_brightness]);
		panel_set_display_on(panel);
		goto exit;
	}

	pr_info("doze_brightness_state = %d, doze_brightness = %d, unset_doze_brightness = %d\n",
			ctx->doze_brightness_state, doze_brightness, ctx->unset_doze_brightness);
	if (ctx->doze_brightness_state != doze_brightness ||
		ctx->unset_doze_brightness != DOZE_TO_NORMAL) {

		if (doze_brightness == DOZE_BRIGHTNESS_HBM ||
			ctx->unset_doze_brightness == DOZE_BRIGHTNESS_HBM) {
			rc = panel_doze_aod_control(panel, true);
			if (rc) {
				pr_err("[%s] failed to send DSI_CMD_SET_MI_DOZE_HBM cmd, rc=%d\n",
					panel->connector->name, rc);
			} else {
				pr_info("set doze mode to DOZE_BRIGHTNESS_HBM");
			}
		} else if (doze_brightness == DOZE_BRIGHTNESS_LBM ||
			ctx->unset_doze_brightness == DOZE_BRIGHTNESS_LBM) {
			rc = panel_doze_aod_control(panel, false);
			if (rc) {
				pr_err("[%s] failed to send DSI_CMD_SET_MI_DOZE_LBM cmd, rc=%d\n",
					panel->connector->name, rc);
			} else {
				pr_info("set doze mode to DOZE_BRIGHTNESS_LBM");
			}
		}

		ctx->unset_doze_brightness = DOZE_TO_NORMAL;
		ctx->doze_brightness_state = doze_brightness;
		pr_info("set doze brightness to %s\n", doze_brightness_str[doze_brightness]);
	} else {
		pr_info("%s has been set, skip\n", doze_brightness_str[doze_brightness]);
	}

exit:
	return rc;
}

static int panel_get_doze_brightness(struct drm_panel *panel, char *buf)
{
	int count = 0;
	struct lcm *ctx = panel_to_lcm(panel);

	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}

	count = snprintf(buf, PAGE_SIZE, "%d\n", ctx->doze_brightness_state);

	return count;
}

static struct LCM_setting_table panel_doze_aod_table[] = {
	/* display_off */
	{REGFLAG_CMD, 1, {0x28} },
	/* level2_key_enable */
	{REGFLAG_CMD, 3, {0xF0, 0x5A, 0x5A} },
	/* close checksum */
	{REGFLAG_CMD, 2, {0xB0, 0x05} },
	{REGFLAG_CMD, 2, {0xEC, 0x20} },
	/* frame_60hz */
	{REGFLAG_CMD, 3, {0x60, 0x00, 0x00} },
	/* vfp_diff_off_bypass */
	{REGFLAG_CMD, 2, {0xB0, 0x0B} },
	/* vfp_diff_off_setting */
	{REGFLAG_CMD, 3, {0xD8, 0x50, 0x00} },
	/* aod_mode */
	{REGFLAG_CMD, 3, {0x53, 0x2A, 0x00} },
	/* level2_key_disable */
	{REGFLAG_CMD, 3, {0xF0, 0xA5, 0xA5} },

	{REGFLAG_DELAY, 40, {} },
	/* display_on */
	//{REGFLAG_CMD, 1, {0x29} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static int panel_doze_enable(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	struct lcm *ctx = panel_to_lcm(panel);
	bool fod_backlight_flag;
	bool normal_hbm_flag;
	bool fod_hbm_flag;

	if (!panel) {
		pr_err("%s invalid panel\n", __func__);
		return -EAGAIN;
	}
	if (!panel->connector) {
		pr_err("%s invalid panel->connector\n", __func__);
		return -EAGAIN;
	}

	pr_info("%s+ \n", __func__);

	fod_backlight_flag = mtk_fod_backlight_flag(panel->connector);
	normal_hbm_flag = mtk_normal_hbm_flag(panel->connector);
	fod_hbm_flag = mtk_fod_hbm_flag(panel->connector);

	if (ctx->hbm_en || fod_backlight_flag || normal_hbm_flag || fod_hbm_flag) {
		pr_info("%s hbm en, can't set aod cmd\n");
		goto exit;
	}

	push_panel_table(panel_doze_aod_table, panel, dsi, cb, handle, ARRAY_SIZE(panel_doze_aod_table));
	if (mtk_get_esd_status(panel->connector))
		mtk_clear_esd_status(panel->connector);
	ctx->doze_enable = true;
	ctx->skip_dimmingon = STATE_DIM_BLOCK;

exit:

#ifdef CONFIG_HWCONF_MANAGER
	/* add for display fps cpunt */
	dsi_panel_fps_count(panel, 0, 0);
	/* add for display state count*/
	if (atomic_read(&panel_active_count_enable))
		dsi_panel_state_count(panel, 0);
#endif
	pr_info("%s-\n", __func__);

	return 0;
}

static struct LCM_setting_table panel_set_nolp_table[] = {
	/* display_off */
	{REGFLAG_CMD, 1, {0x28} },
	/* level2_key_enable */
	{REGFLAG_CMD, 3, {0xF0, 0x5A, 0x5A} },
	/* black_insertion_bypass */
	{REGFLAG_CMD, 2, {0xB0, 0x0A} },
	/* black_insertion_setting */
	{REGFLAG_CMD, 2, {0xEE, 0x02} },
	/*vfp_diff_on_bypass*/
	{REGFLAG_CMD, 2, {0xB0, 0x0B} },
	/* vfp_diff_off_setting */
	{REGFLAG_CMD, 3, {0xD8, 0x59, 0x70} },
	/* delay 32ms */
	{REGFLAG_DELAY, 40, {} },
	/* aod_mode */
	{REGFLAG_CMD, 3, {0x53, 0x28, 0x00} },
	/* open checksum */
	{REGFLAG_CMD, 2, {0xB0, 0x05} },
	{REGFLAG_CMD, 2, {0xEC, 0x42} },
	/* level2_key_disable */
	{REGFLAG_CMD, 3, {0xF0, 0xA5, 0xA5} },
	/* display on */
	//{REGFLAG_CMD, 1, {0x29} },

	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static struct LCM_setting_table panel_set_nolp_skip_table[] = {
	/* display_off */
	//{REGFLAG_CMD, 1, {0x28} },
	/* level2_key_enable */
	{REGFLAG_CMD, 3, {0xF0, 0x5A, 0x5A} },
	/* black_insertion_bypass */
	//{REGFLAG_CMD, 2, {0xB0, 0x0A} },
	/* black_insertion_setting */
	//{REGFLAG_CMD, 2, {0xEE, 0x02} },
	/*vfp_diff_on_bypass*/
	{REGFLAG_CMD, 2, {0xB0, 0x0B} },
	/* vfp_diff_off_setting */
	{REGFLAG_CMD, 3, {0xD8, 0x59, 0x70} },
	/* open checksum */
	{REGFLAG_CMD, 2, {0xB0, 0x05} },
	{REGFLAG_CMD, 2, {0xEC, 0x42} },
	/* level2_key_disable */
	{REGFLAG_CMD, 3, {0xF0, 0xA5, 0xA5} },
	/* display on */
	//{REGFLAG_CMD, 1, {0x29} },

	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static int panel_doze_disable(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	struct lcm *ctx = panel_to_lcm(panel);
	bool fod_backlight_flag;
	bool fod_hbm_flag;

	if (!panel) {
		pr_err("panel_aod_set_state invalid panel\n");
		return -EAGAIN;
	}

	fod_backlight_flag = mtk_fod_backlight_flag(panel->connector);
	fod_hbm_flag = mtk_fod_hbm_flag(panel->connector);

	pr_info("%s fod_backlight_flag = %d, fod_hbm_flag = %d, fod_hbm_enabled set\n",
			__func__, fod_backlight_flag, fod_hbm_flag);

	if (fod_backlight_flag || fod_hbm_flag) {
		pr_info("fod_backlight_flag/fod_hbm_flag set, skip nolp\n");
		push_panel_table(panel_set_nolp_skip_table, panel, dsi, cb, handle, ARRAY_SIZE(panel_set_nolp_skip_table));
	} else {
		pr_info("%s+ \n", __func__);
		push_panel_table(panel_set_nolp_table, panel, dsi, cb, handle, ARRAY_SIZE(panel_set_nolp_table));
		ctx->hbm_en = false;
		ctx->hbm_wait = false;
		ctx->skip_dimmingon = STATE_NONE;
	}
	ctx->doze_brightness_state = DOZE_TO_NORMAL;
	ctx->doze_enable = false;

#ifdef CONFIG_HWCONF_MANAGER
		/* add for display fps cpunt */
		dsi_panel_fps_count(panel, 0, 1);
		/* add for display state count*/
		if (!atomic_read(&panel_active_count_enable))
			dsi_panel_state_count(panel, 1);
#endif

	return 0;
}
static void panel_set_nolp(struct drm_panel *panel)
{
	int ret = 0;
	char display_on_tb[] = {0x29};
	struct mtk_ddic_dsi_msg *cmd_msg =
			vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s+ display on\n", __func__);

	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	cmd_msg->channel = 0;
	cmd_msg->flags = 0;
	cmd_msg->tx_cmd_num = 1;

	cmd_msg->type[0] = 0x05;
	cmd_msg->tx_buf[0] = display_on_tb;
	cmd_msg->tx_len[0] = 1;

	ret = mtk_ddic_dsi_send_cmd(cmd_msg, true, false);
	if (ret != 0) {
		pr_err("%s mtk_ddic_dsi_send_cmd error\n", __func__);
	}
	ctx->set_backlight_time = ktime_add_ms(ktime_get(), 80);

	vfree(cmd_msg);
}

static struct LCM_setting_table panel_set_aod_backlight_table[] = {
	/* vfp_diff_off_setting */
	{REGFLAG_CMD, 3, {0x53, 0x2A, 0x00} },
	/* display on */
	{REGFLAG_CMD, 1, {0x29} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static int panel_set_aod_light_mode(struct drm_panel *panel, void *dsi, dcs_write_gce cb,
		void *handle, unsigned int mode)
{
	struct lcm *ctx = panel_to_lcm(panel);
	bool fod_backlight_flag;
	bool normal_hbm_flag;
	bool fod_hbm_flag;

	if (!panel) {
		pr_err("%s invalid panel\n", __func__);
		return -EAGAIN;
	}
	if (!panel->connector) {
		pr_err("%s invalid panel->connector\n", __func__);
		return -EAGAIN;
	}

	pr_info("%s+ \n", __func__);

	fod_backlight_flag = mtk_fod_backlight_flag(panel->connector);
	normal_hbm_flag = mtk_normal_hbm_flag(panel->connector);
	fod_hbm_flag = mtk_fod_hbm_flag(panel->connector);

	if (ctx->hbm_en || fod_backlight_flag || normal_hbm_flag || fod_hbm_flag) {
		pr_info("%s hbm en, can't set aod cmd\n", __func__);
		goto exit;
	}

	if (mode == DOZE_BRIGHTNESS_HBM)
		panel_set_aod_backlight_table[0].para_list[1] = 0x2A;
	else if (mode == DOZE_BRIGHTNESS_LBM)
		panel_set_aod_backlight_table[0].para_list[1] = 0x2B;

	push_panel_table(panel_set_aod_backlight_table,
		panel, dsi, cb, handle, ARRAY_SIZE(panel_set_aod_backlight_table));

exit:
	pr_info("%s-\n", __func__);

	return 0;
}

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

void panel_get_unset_doze_brightness(struct drm_panel *panel, int *state)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!panel) {
		pr_err("panel_get_unset_doze_brightness invalid panel\n");
		return;
	}

	*state = ctx->unset_doze_brightness;
	pr_info("%s unset_doze_brightness = %d\n", __func__, *state);
}

static void panel_aod_get_state(struct drm_panel *panel, bool *state)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!panel) {
		pr_err("panel_aod_get_state invalid panel\n");
		return;
	}

	*state = ctx->in_aod;
	pr_info("%s+ in_aod = %b\n", __func__, *state);
}

static void panel_aod_set_state(struct drm_panel *panel, bool *state)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!panel) {
		pr_err("panel_aod_set_state invalid panel\n");
		return;
	}

	ctx->in_aod = *state;
	pr_info("%s in_aod = %d\n", __func__, *state);
}

static int panel_hbm_fod_control(struct drm_panel *panel, bool en)
{
	struct mtk_ddic_dsi_msg *cmd_msg =
			vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	struct lcm *ctx = panel_to_lcm(panel);

	int ret;
	char level2_key_enable_tb[] = {0xF0, 0x5A, 0x5A};
	char elvss_dimming_bypass_tb[] = {0xB0, 0x05};
	char elvss_dimming_setting_tb[] = {0xB7, 0x13};
	char level2_key_disable_tb[] = {0xF0, 0xA5, 0xA5};
	char hbm_tb[] = {0x53, 0xe0, 0x00};
	char bl_tb[] = {0x51, 0x07, 0xff};
	pr_info("%s+ en = %d\n", __func__, en);
	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	if (en) {
		hbm_tb[1] = 0xe0;
		elvss_dimming_setting_tb[1] = 0x13;
	}
	else {
		bl_tb[1] = bl_tb0[1];
		bl_tb[2] = bl_tb0[2];
		hbm_tb[1] = 0x20;
		elvss_dimming_setting_tb[1] = 0x93;
	}

	if (en) {
		/* fod hbm on */
		cmd_msg->channel = 0;
		cmd_msg->flags = 0;
		cmd_msg->tx_cmd_num = 6;

		cmd_msg->type[0] = 0x39;
		cmd_msg->tx_buf[0] = level2_key_enable_tb;
		cmd_msg->tx_len[0] = 3;

		cmd_msg->type[1] = 0x15;
		cmd_msg->tx_buf[1] = elvss_dimming_bypass_tb;
		cmd_msg->tx_len[1] = 2;

		cmd_msg->type[2] = 0x15;
		cmd_msg->tx_buf[2] = elvss_dimming_setting_tb;
		cmd_msg->tx_len[2] = 2;

		cmd_msg->type[3] = 0x39;
		cmd_msg->tx_buf[3] = level2_key_disable_tb;
		cmd_msg->tx_len[3] = 3;

		cmd_msg->type[4] = 0x39;
		cmd_msg->tx_buf[4] = hbm_tb;
		cmd_msg->tx_len[4] = 3;

		cmd_msg->type[5] = 0x39;
		cmd_msg->tx_buf[5] = bl_tb;
		cmd_msg->tx_len[5] = 3;
	} else {
		/* fod hbm off */
		cmd_msg->channel = 0;
		cmd_msg->flags = 0;
		cmd_msg->tx_cmd_num = 6;

		cmd_msg->type[0] = 0x39;
		cmd_msg->tx_buf[0] = hbm_tb;
		cmd_msg->tx_len[0] = 3;

		cmd_msg->type[1] = 0x39;
		cmd_msg->tx_buf[1] = bl_tb;
		cmd_msg->tx_len[1] = 3;

		cmd_msg->type[2] = 0x39;
		cmd_msg->tx_buf[2] = level2_key_enable_tb;
		cmd_msg->tx_len[2] = 3;

		cmd_msg->type[3] = 0x15;
		cmd_msg->tx_buf[3] = elvss_dimming_bypass_tb;
		cmd_msg->tx_len[3] = 2;

		cmd_msg->type[4] = 0x15;
		cmd_msg->tx_buf[4] = elvss_dimming_setting_tb;
		cmd_msg->tx_len[4] = 2;

		cmd_msg->type[5] = 0x39;
		cmd_msg->tx_buf[5] = level2_key_disable_tb;
		cmd_msg->tx_len[5] = 3;
	}

	pr_info("%s+,hbm_tb = 0x%x, high_bl = 0x%x, low_bl = 0x%x, elvss dimming state = %d\n",
		__func__, hbm_tb[1], bl_tb[1], bl_tb[2], en);

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

static int panel_normal_hbm_control(struct drm_panel *panel, bool en)
{
	struct mtk_ddic_dsi_msg *cmd_msg =
			vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	struct lcm *ctx = panel_to_lcm(panel);

	int ret;
	char level2_key_enable_tb[] = {0xF0, 0x5A, 0x5A};
	char elvss_dimming_bypass_tb[] = {0xB0, 0x05};
	char elvss_dimming_setting_tb[] = {0xB7, 0x13};
	char dly_bypass_tb[] = {0xB0, 0x01};
	char dly_setting_tb[] = {0xB7, 0x4C};
	char level2_key_disable_tb[] = {0xF0, 0xA5, 0xA5};
	char hbm_tb[] = {0x53, 0xe0, 0x00};
	char bl_tb[] = {0x51, 0x07, 0xff};
	pr_info("%s+ en = %d\n", __func__, en);
	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	if (en) {
		hbm_tb[1] = 0xe8;
		elvss_dimming_setting_tb[1] = 0x13;
		dly_setting_tb[1] = 0x4C;
		ctx->skip_dimmingon = STATE_DIM_BLOCK;
	}
	else {
		bl_tb[1] = bl_tb0[1];
		bl_tb[2] = bl_tb0[2];
		hbm_tb[1] = 0x28;
		elvss_dimming_setting_tb[1] = 0x93;
		dly_setting_tb[1] = 0x44;
		ctx->skip_dimmingon = STATE_DIM_RESTORE;
	}

	if (en) {
		/* fod hbm on */
		cmd_msg->channel = 0;
		cmd_msg->flags = 0;
		cmd_msg->tx_cmd_num = 6;

		cmd_msg->type[0] = 0x39;
		cmd_msg->tx_buf[0] = level2_key_enable_tb;
		cmd_msg->tx_len[0] = 3;

		cmd_msg->type[1] = 0x15;
		cmd_msg->tx_buf[1] = elvss_dimming_bypass_tb;
		cmd_msg->tx_len[1] = 2;

		cmd_msg->type[2] = 0x15;
		cmd_msg->tx_buf[2] = elvss_dimming_setting_tb;
		cmd_msg->tx_len[2] = 2;

		cmd_msg->type[3] = 0x39;
		cmd_msg->tx_buf[3] = level2_key_disable_tb;
		cmd_msg->tx_len[3] = 3;

		cmd_msg->type[4] = 0x39;
		cmd_msg->tx_buf[4] = hbm_tb;
		cmd_msg->tx_len[4] = 3;

		cmd_msg->type[5] = 0x39;
		cmd_msg->tx_buf[5] = bl_tb;
		cmd_msg->tx_len[5] = 3;
	} else {
		/* fod hbm off */
		cmd_msg->channel = 0;
		cmd_msg->flags = 0;
		cmd_msg->tx_cmd_num = 6;

		cmd_msg->type[0] = 0x39;
		cmd_msg->tx_buf[0] = hbm_tb;
		cmd_msg->tx_len[0] = 3;

		cmd_msg->type[1] = 0x39;
		cmd_msg->tx_buf[1] = bl_tb;
		cmd_msg->tx_len[1] = 3;

		cmd_msg->type[2] = 0x39;
		cmd_msg->tx_buf[2] = level2_key_enable_tb;
		cmd_msg->tx_len[2] = 3;

		cmd_msg->type[3] = 0x15;
		cmd_msg->tx_buf[3] = elvss_dimming_bypass_tb;
		cmd_msg->tx_len[3] = 2;

		cmd_msg->type[4] = 0x15;
		cmd_msg->tx_buf[4] = elvss_dimming_setting_tb;
		cmd_msg->tx_len[4] = 2;

		cmd_msg->type[5] = 0x39;
		cmd_msg->tx_buf[5] = level2_key_disable_tb;
		cmd_msg->tx_len[5] = 3;
	}

	pr_info("%s+,hbm_tb = 0x%x, high_bl = 0x%x, low_bl = 0x%x, elvss dimming state = %d\n",
		__func__, hbm_tb[1], bl_tb[1], bl_tb[2], en);

	ret = mtk_ddic_dsi_send_cmd(cmd_msg, true, false);
	if (ret != 0) {
		pr_err("%s mtk_ddic_dsi_send_cmd error\n", __func__);
	}

	vfree(cmd_msg);
	pr_info("%s-\n", __func__);

	return 0;
}

static void panel_set_crc_off(struct drm_panel *panel)
{
	int ret;
	char level_key_enable_tb[] = {0xF0, 0x5A, 0x5A};
	char level_key_disable_tb[] = {0xF0, 0xA5, 0xA5};
	char crc_enable_tb[] = {0x81, 0x92};
	char crc_bypass_tb[] = {0xB1, 0x00};
	char crc_p3_offset_tb[] = {0xB0, 0x2B, 0xB1};
	char crc_p3_off_tb[] = {0xB1, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF,
			0x00, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0xFF, 0xFF};
	//FLAT MODE OFF
	char flat_gamma_tb[] = {0xC2, 0x2D, 0x07, 0xFE, 0x82, 0xC6, 0x70, 0x82, 0x70, 0xCA, 0x0A, 0x0A, 0x0A, 0x28,
					0x28, 0x28, 0x35, 0x35, 0x35, 0x3F, 0x3F, 0x3F, 0x42, 0x42, 0x42, 0x1B, 0xDA, 0x2F, 0x48};
	char flat_gamma_value_tb[] = {0xF7, 0x03};
	struct lcm *ctx = panel_to_lcm(panel);
	struct mtk_ddic_dsi_msg *cmd_msg =
		vmalloc(sizeof(struct mtk_ddic_dsi_msg));

	if (ctx && ctx->crc_state == CRC_OFF) {
		pr_info("%s crc_state is already in CRC_OFF,skip \n", __func__);
		goto done;
	}

	pr_info("%s begin +\n", __func__);
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
	cmd_msg->tx_buf[4] = crc_p3_off_tb;
	cmd_msg->tx_len[4] = ARRAY_SIZE(crc_p3_off_tb);

	cmd_msg->type[5] = 0x39;
	cmd_msg->tx_buf[5] = flat_gamma_tb;
	cmd_msg->tx_len[5] = ARRAY_SIZE(flat_gamma_tb);

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
	} else {
		ctx->crc_state = CRC_OFF;
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
	char crc_enable_tb[] = {0x81, 0x90};
	char crc_bypass_tb[] = {0xB1, 0x00};
	char crc_srgb_offset_tb[] = {0xB0, 0x01, 0xB1};
	char crc_srgb_tb[] = {0xB1, 0xC0, 0x09, 0x04, 0x3C, 0xE9, 0x10, 0x09, 0x02, 0xC6,
			0x45, 0xEA, 0xD8, 0xC7, 0x0C, 0xCA, 0xFB, 0xF0, 0x14, 0xFF, 0xF3, 0xDB};
	//FLAT MODE ON
	char flat_gamma_tb[] = {0xC2, 0x2D, 0x27, 0xFE, 0x82, 0xC6, 0x70, 0x82, 0x70, 0xCA, 0x0A, 0x0A, 0x0A, 0x28,
					0x28, 0x28, 0x35, 0x35, 0x35, 0x3F, 0x3F, 0x3F, 0x42, 0x42, 0x42, 0x1B, 0xDA, 0x2F, 0x48};
	char flat_gamma_value_tb[] = {0xF7, 0x03};
	struct lcm *ctx = panel_to_lcm(panel);
	struct mtk_ddic_dsi_msg *cmd_msg =
		vmalloc(sizeof(struct mtk_ddic_dsi_msg));

	if (ctx && ctx->crc_state == CRC_SRGB) {
			pr_info("%s crc_state is already in SRGB,skip \n", __func__);
			goto done;
	}

	pr_info("%s begin +\n", __func__);
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
	cmd_msg->tx_len[5] = ARRAY_SIZE(flat_gamma_tb);

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
	} else {
		ctx->crc_state = CRC_SRGB;
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
	char crc_enable_tb[] = {0x81, 0x92};
	char crc_bypass_tb[] = {0xB1, 0x00};
	char crc_p3_offset_tb[] = {0xB0, 0x2B, 0xB1};
	char crc_p3_tb[] = {0xB1, 0xB7, 0x00, 0x00, 0x15, 0xC2, 0x00, 0x08, 0x08, 0xB9,
			0x26, 0xEE, 0xDB, 0xDC, 0x05, 0xCC, 0xE4, 0xD7, 0x00, 0xFF, 0xFF, 0xFF};
	//FLAT MODE OFF
	char flat_gamma_tb[] = {0xC2, 0x2D, 0x07, 0xFE, 0x82, 0xC6, 0x70, 0x82, 0x70, 0xCA, 0x0A, 0x0A, 0x0A, 0x28,
					0x28, 0x28, 0x35, 0x35, 0x35, 0x3F, 0x3F, 0x3F, 0x42, 0x42, 0x42, 0x1B, 0xDA, 0x2F, 0x48};
	char flat_gamma_value_tb[] = {0xF7, 0x03};
	struct lcm *ctx = panel_to_lcm(panel);

	struct mtk_ddic_dsi_msg *cmd_msg =
		vmalloc(sizeof(struct mtk_ddic_dsi_msg));

	if (ctx && ctx->crc_state == CRC_P3) {
		pr_info("%s crc_state is already in P3,skip \n", __func__);
		goto done;
	}

	pr_info("%s begin +\n", __func__);
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
	cmd_msg->tx_len[5] = ARRAY_SIZE(flat_gamma_tb);

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
	} else {
		ctx->crc_state = CRC_P3;
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
	char crc_enable_tb[] = {0x81, 0x92};
	char crc_bypass_tb[] = {0xB1, 0x00};
	char crc_p3_offset_tb[] = {0xB0, 0x2B, 0xB1};
	char crc_p3_tb[] = {0xB1, 0xB7, 0x00, 0x00, 0x15, 0xC2, 0x00, 0x08, 0x08, 0xB9,
			0x26, 0xEE, 0xDB, 0xDC, 0x05, 0xCC, 0xE4, 0xD7, 0x00, 0xFF, 0xF5, 0xE0};
	//FLAT MODE OFF
	char flat_gamma_tb[] = {0xC2, 0x2D, 0x07, 0xFE, 0x82, 0xC6, 0x70, 0x82, 0x70, 0xCA, 0x0A, 0x0A, 0x0A, 0x28,
					0x28, 0x28, 0x35, 0x35, 0x35, 0x3F, 0x3F, 0x3F, 0x42, 0x42, 0x42, 0x1B, 0xDA, 0x2F, 0x48};
	char flat_gamma_value_tb[] = {0xF7, 0x03};
	struct lcm *ctx = panel_to_lcm(panel);

	struct mtk_ddic_dsi_msg *cmd_msg =
		vmalloc(sizeof(struct mtk_ddic_dsi_msg));

	if (ctx && ctx->crc_state == CRC_P3_D65) {
		pr_info("%s crc_state is already in CRC_P3_D65, skip \n", __func__);
		goto done;
	}

	pr_info("%s begin +\n", __func__);
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
	cmd_msg->tx_len[5] = ARRAY_SIZE(flat_gamma_tb);

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
	} else {
		ctx->crc_state = CRC_P3_D65;
	}

done:
	vfree(cmd_msg);

	pr_info("%s end -\n", __func__);
}

static void panel_set_flat_crc_p3(struct drm_panel *panel)
{
	int ret;
	char level_key_enable_tb[] = {0xF0, 0x5A, 0x5A};
	char level_key_disable_tb[] = {0xF0, 0xA5, 0xA5};
	char crc_enable_tb[] = {0x81, 0x92};
	char crc_bypass_tb[] = {0xB1, 0x00};
	char crc_p3_offset_tb[] = {0xB0, 0x2B, 0xB1};
	char crc_p3_tb[] = {0xB1, 0xEE, 0x01, 0x00, 0x11, 0xFC, 0x00, 0x0B, 0x02, 0xE5,
			0x1E, 0xFD, 0xE3, 0xF6, 0x05, 0xDF, 0xFF, 0xF6, 0x00, 0xFF, 0xF3, 0xDB};
	//FLAT MODE ON
	char flat_gamma_tb[] = {0xC2, 0x2D, 0x27, 0xFE, 0x82, 0xC6, 0x70, 0x82, 0x70, 0xCA, 0x0A, 0x0A, 0x0A, 0x28,
					0x28, 0x28, 0x35, 0x35, 0x35, 0x3F, 0x3F, 0x3F, 0x42, 0x42, 0x42, 0x1B, 0xDA, 0x2F, 0x48};
	char flat_gamma_value_tb[] = {0xF7, 0x03};

	struct lcm *ctx = panel_to_lcm(panel);
	struct mtk_ddic_dsi_msg *cmd_msg =
		vmalloc(sizeof(struct mtk_ddic_dsi_msg));

	if (ctx && ctx->crc_state == CRC_P3_FLAT) {
		pr_info("%s crc_state is already in CRC_P3_FLAT, skip \n", __func__);
		goto done;
	}

	pr_info("%s begin +\n", __func__);
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
	cmd_msg->tx_len[5] = ARRAY_SIZE(flat_gamma_tb);

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
	} else {
		ctx->crc_state = CRC_P3_FLAT;
	}

done:
	vfree(cmd_msg);

	pr_info("%s end -\n", __func__);
}

static void panel_dimming_control(struct drm_panel *panel, bool en)
{
	struct mtk_ddic_dsi_msg *cmd_msg =
			vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	int ret;
	bool hbm_flag = 0;
	char dimming_tb[] = {0x53, 0x28, 0x00};
	pr_info("%s+ en = %d\n", __func__, en);
	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	if (en) {
		dimming_tb[1] = 0x28;
	}
	else {
		dimming_tb[1] = 0x20;
	}

	cmd_msg->channel = 0;
	cmd_msg->flags = 0;
	cmd_msg->tx_cmd_num = 1;

	cmd_msg->type[0] = 0x39;
	cmd_msg->tx_buf[0] = dimming_tb;
	cmd_msg->tx_len[0] = 3;

	pr_info("%s+,dimming = %d \n", __func__, en);

	ret = mtk_ddic_dsi_send_cmd(cmd_msg, true, true);
	if (ret != 0) {
		pr_err("%s mtk_ddic_dsi_send_cmd error\n", __func__);
	} else {
		panel_hbm_set_state(panel, &hbm_flag);
	}
	vfree(cmd_msg);
	pr_info("%s-\n", __func__);
}

static int panel_freq_switch(struct drm_panel *panel, unsigned int cur_mode, unsigned int dst_mode)
{
	int ret = 0;
	struct mtk_ddic_dsi_msg *cmd_msg =
			vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	char freq_tb[] = {0x60, 0x00, 0x00};
	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	if (cur_mode == 0 && dst_mode == 1) { /* 60 switch to 120 */
		freq_tb[1] = 0x10;
		pr_info("%s_60_to_120+,freq_tb = 0x%x\n", __func__, freq_tb[1]);
#ifdef CONFIG_HWCONF_MANAGER
		/* add for display fps cpunt */
		dsi_panel_fps_count(panel, 120, 1);
#endif
	} else if (cur_mode == 1 && dst_mode == 0) { /* 120 switch to 60 */
		freq_tb[1] = 0x00;
		pr_info("%s_120_to_60+,freq_tb = 0x%x\n", __func__, freq_tb[1]);
#ifdef CONFIG_HWCONF_MANAGER
		/* add for display fps cpunt */
		dsi_panel_fps_count(panel, 60, 1);
#endif

	} else {
		ret = 1;
	}

	cmd_msg->channel = 0;
	cmd_msg->flags = 0;
	cmd_msg->tx_cmd_num = 1;

	cmd_msg->type[0] = 0x39;
	cmd_msg->tx_buf[0] = freq_tb;
	cmd_msg->tx_len[0] = 3;

	ret = mtk_ddic_dsi_send_cmd(cmd_msg, true, false);
	if (ret != 0) {
		DDPPR_ERR("%s error\n", __func__);
		goto  done;
	}

done:
	vfree(cmd_msg);
	pr_info("%s end -\n", __func__);
	return ret;
}

static void panel_dimming_on_delayed_work(struct work_struct* work)
{
	struct lcm *ctx = container_of(work, struct lcm, dimmingon_delayed_work.work);

	if(&ctx->panel != NULL && !ctx->hbm_en && !ctx->doze_enable)
		panel_dimming_control(&ctx->panel, true);
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
		*state = true;
	} else {
		if (ctx->hbm_en)
			*state = true;
		else
			*state = false;
	}
	pr_debug("%s-\n", __func__);
}

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static void panel_get_hbm_solution(struct drm_panel *panel, int *solution)
{
	pr_debug("%s+ solution 2 \n", __func__);
	*solution = 2;
}

static struct mtk_panel_funcs ext_funcs = {
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.setbacklight_control = lcm_setbacklight_control,
	.hbm_set_cmdq = panel_hbm_set_cmdq,
	.ext_param_set = mtk_panel_ext_param_set,
	.mode_switch = mode_switch,
	.hbm_fod_control = panel_hbm_fod_control,
	.normal_hbm_control = panel_normal_hbm_control,
	.hbm_get_state = panel_hbm_get_state,
	.hbm_set_state = panel_hbm_set_state,
	.hbm_get_wait_state = panel_hbm_get_wait_state,
	.hbm_set_wait_state = panel_hbm_set_wait_state,
	.esd_restore_backlight = panel_esd_restore_backlight,
	.set_doze_brightness = panel_set_doze_brightness,
	.get_doze_brightness = panel_get_doze_brightness,
	.doze_enable = panel_doze_enable,
	.doze_disable = panel_doze_disable,
	.set_nolp = panel_set_nolp,
	.set_aod_light_mode = panel_set_aod_light_mode,
	.aod_set_state = panel_aod_set_state,
	.aod_get_state = panel_aod_get_state,
	.get_unset_doze_brightness = panel_get_unset_doze_brightness,
	.get_panel_info = panel_get_panel_info,
	.panel_set_crc_off = panel_set_crc_off,
	.panel_set_crc_srgb = panel_set_crc_srgb,
	.panel_set_crc_p3 = panel_set_crc_p3,
	.panel_set_crc_p3_d65 = panel_set_crc_p3_d65,
	.panel_set_flat_crc_p3 = panel_set_flat_crc_p3,
	.panel_dimming_control = panel_dimming_control,
	.panel_freq_switch = panel_freq_switch,
	.reset = panel_ext_reset,
	.hbm_need_delay = panel_hbm_need_delay,
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

static int lcm_get_modes(struct drm_panel *panel)
{
	struct drm_display_mode *mode;
	struct drm_display_mode *mode2;


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

	mode2 = drm_mode_duplicate(panel->drm, &performance_mode);
	if (!mode2) {
		dev_err(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
			performance_mode.hdisplay,
			performance_mode.vdisplay,
			performance_mode.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode2);
	mode2->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(panel->connector, mode2);

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

static int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct lcm *ctx;
	struct device_node *backlight;
	int ret;

	pr_info("%s+\n", __func__);
	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET |
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
		dev_err(dev, "cannot get reset-gpios %ld\n",
			PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);

	ctx->prepared = true;
	ctx->enabled = true;
	ctx->doze_brightness_state = DOZE_TO_NORMAL;
	ctx->unset_doze_brightness = DOZE_TO_NORMAL;
	ctx->in_aod = false;
	ctx->high_refresh_rate = false;
	ctx->panel_info = panel_name;
	ctx->doze_enable = false;
	ctx->crc_state = CRC_NONE;

	drm_panel_init(&ctx->panel);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &lcm_drm_funcs;

	atomic_set(&fod_hbm_on_flag, 0);

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

	ctx->skip_dimmingon = STATE_NONE;
	INIT_DELAYED_WORK(&ctx->dimmingon_delayed_work, panel_dimming_on_delayed_work);

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
	{
		.compatible = "j10_38_08_0a_dsc_cmd,lcm",
	},
	{} };

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {

			.name = "j10_38_08_0a_dsc_cmd,lcm",
			.owner = THIS_MODULE,
			.of_match_table = lcm_of_match,
		},
};

module_mipi_dsi_driver(lcm_driver);
