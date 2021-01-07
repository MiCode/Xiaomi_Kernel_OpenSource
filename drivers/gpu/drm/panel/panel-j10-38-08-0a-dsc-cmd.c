/*
 * Copyright (c) 2015 MediaTek Inc.
 * Copyright (C) 2020 XiaoMi, Inc.
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
struct mtk_ddic_dsi_msg cmd_msg = {0};

static char bl_tb0[] = {0x51, 0x3, 0xff};

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

       pr_err("%s +\n",__func__);

       lcm_panel_vci_regulator_init(dev);

       /* set voltage with min & max*/
       ret = regulator_set_voltage(disp_vci, 3000000, 3000000);
       if (ret < 0)
               pr_err("set voltage disp_vci fail, ret = %d\n", ret);
       retval |= ret;

       status = regulator_is_enabled(disp_vci);
       pr_err("%s regulator_is_enabled=%d\n",__func__,status);
       if(!status){
               /* enable regulator */
               ret = regulator_enable(disp_vci);
               if (ret < 0)
                       pr_err("enable regulator disp_vci fail, ret = %d\n", ret);
               retval |= ret;
       }

       pr_err("%s -\n",__func__);
       return retval;
}
static unsigned int start_up = 1;
static int lcm_panel_vci_disable(struct device *dev)
{
       int ret = 0;
       int retval = 0;
       int status = 0;
       pr_err("%s +\n",__func__);

       lcm_panel_vci_regulator_init(dev);

       //status = regulator_is_enabled(disp_vci);
       //pr_err("%s regulator_is_enabled=%d\n",__func__,status);
       //if(!status){
       if(start_up) {
               pr_err("%s enable regulator\n",__func__);
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

       pr_err("%s -\n",__func__);

       return retval;
}


static void lcm_panel_init(struct lcm *ctx)
{
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	msleep(12);
	gpiod_set_value(ctx->reset_gpio, 0);
	msleep(2);
	gpiod_set_value(ctx->reset_gpio, 1);
	msleep(10);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	/*sleep out*/
	lcm_dcs_write_seq_static(ctx, 0x11);
	msleep(20);

	/* TE sync on*/
 	lcm_dcs_write_seq_static(ctx, 0x35, 0x00);

 	/*PPS Seting*/
	lcm_dcs_write_seq_static(ctx, 0x9D, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x9E, 0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x09, 0x60,
										0x04, 0x38, 0x00, 0x1E, 0x02, 0x1C, 0x02, 0x1C,
										0x02, 0x00, 0x02, 0x0E, 0x00, 0x20, 0x02, 0xE3,
										0x00, 0x07, 0x00, 0x0C, 0x03, 0x50, 0x03, 0x64,
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

	/* Timming Set */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x03);
 	lcm_dcs_write_seq_static(ctx, 0xD9, 0x40,0x4C,0x40,0x42,0x4E,0x42);
 	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

	/* Err FG Set */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xEC, 0x00,0xC2,0xC2,0x42);
 	lcm_dcs_write_seq_static(ctx, 0xB0, 0x0D);
	lcm_dcs_write_seq_static(ctx, 0xEC, 0x19);
 	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
	msleep(80);

	/* Frequency Select */
 	lcm_dcs_write_seq_static(ctx, 0x60, 0x00);

 	/* Dimming Setting*/
 	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
 	lcm_dcs_write_seq_static(ctx, 0xB0, 0x06);
 	lcm_dcs_write_seq_static(ctx, 0xB7, 0x01);
 	lcm_dcs_write_seq_static(ctx, 0xB0, 0x05);
	lcm_dcs_write_seq_static(ctx, 0xB7, 0x93);
 	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
 	/* HBM Mode OFF(Normal Mode)*/
 	lcm_dcs_write_seq_static(ctx, 0x53, 0x20);
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

	 /*Display on*/
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

	if (!ctx->prepared)
		return 0;

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

	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s\n", __func__);
	if (ctx->prepared)
		return 0;

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
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {

			.cmd = 0x53, .count = 1, .para_list[0] = 0x00,
		},
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 70200,
	.physical_height_um = 152100,
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
		.slice_height = 30,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 739,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 848,
		.slice_bpg_offset = 868,
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
};

static struct mtk_panel_params ext_params_120hz = {
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {

			.cmd = 0x53, .count = 1, .para_list[0] = 0x24,
		},
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 70200,
	.physical_height_um = 152100,
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
		.slice_height = 30,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 739,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 848,
		.slice_bpg_offset = 868,
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

static int lcm_setbacklight_control(struct drm_panel *panel, unsigned int level, bool fod_cal_flag)
{
	bool fod_backlight_flag;
	int ret;
	static unsigned int  last_backlight;
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

		ret = mtk_ddic_dsi_send_cmd(&cmd_msg, true);
		if (ret != 0) {
			pr_err("%s mtk_ddic_dsi_send_cmd error\n", __func__);
		}
		if (((!last_backlight) && level) || (last_backlight && (!level))) {
			pr_info("%s,fod_backlight_flag = %d, level = 0x%x, high_bl = 0x%x, low_bl = 0x%x \n", __func__, fod_backlight_flag, level, bl_tb0[1], bl_tb0[2]);
		}
	}
	else {
		pr_info("%s,fod_backlight_flag = %d,  can't set backlight\n", __func__, fod_backlight_flag);
	}
	if (!fod_cal_flag) {
		panel->connector->brightness_clone = level;
		sysfs_notify(&panel->connector->kdev->kobj, NULL, "brightness_clone");
	}
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
		ext->params = &ext_params_120hz;
	else
		ret = 1;

	return ret;
}

static void mode_switch_60_to_120(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (stage == BEFORE_DSI_POWERDOWN) {
		/* display off */
		lcm_dcs_write_seq_static(ctx, 0x28);
		msleep(10);
		lcm_dcs_write_seq_static(ctx, 0x10);
		usleep_range(200 * 1000, 230 * 1000);
	} else if (stage == AFTER_DSI_POWERON) {
		/* Frequency Select 120HZ*/
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
 		lcm_dcs_write_seq_static(ctx, 0x60, 0x10);
 		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
	}
}

static void mode_switch_120_to_60(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (stage == BEFORE_DSI_POWERDOWN) {
		/* display off */
		lcm_dcs_write_seq_static(ctx, 0x28);
		msleep(10);
		lcm_dcs_write_seq_static(ctx, 0x10);
		usleep_range(200 * 1000, 230 * 1000);
	} else if (stage == AFTER_DSI_POWERON) {
		/* Frequency Select 60HZ*/
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
 		lcm_dcs_write_seq_static(ctx, 0x60, 0x00);
 		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
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
	} else
		ret = 1;

	return ret;
}

static int panel_hbm_fod_control(struct drm_panel *panel, bool en)
{
	struct mtk_ddic_dsi_msg *cmd_msg =
			vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	//struct lcm *ctx = panel_to_lcm(panel);

	int ret;
	char level2_key_enable_tb[] = {0xF0, 0x5A, 0x5A};
	char level2_key_disable_tb[] = {0xF0, 0xA5, 0xA5};
	char elvss_dimming_bypass_tb[] ={0xB0, 0x05};
	char elvss_dimming_setting_tb[] ={0xB7, 0x93};
	char hbm_tb[] = {0x53, 0xe0};
	char bl_tb[] = {0x51, 0x03, 0xff};
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

	cmd_msg->type[4] = 0x15;
	cmd_msg->tx_buf[4] = hbm_tb;
	cmd_msg->tx_len[4] = 2;

	cmd_msg->type[5] = 0x39;
	cmd_msg->tx_buf[5] = bl_tb;
	cmd_msg->tx_len[5] = 3;

	pr_info("%s+,hbm_tb = 0x%x, high_bl = 0x%x, low_bl = 0x%x, elvss dimming state = 0x%x\n",
		__func__, hbm_tb[1], bl_tb[1], bl_tb[2], elvss_dimming_setting_tb[1]);

	ret = mtk_ddic_dsi_send_cmd(cmd_msg, true);
	if (ret != 0) {
		pr_err("%s mtk_ddic_dsi_send_cmd error\n", __func__);
	}

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

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static struct mtk_panel_funcs ext_funcs = {
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.setbacklight_control = lcm_setbacklight_control,
	.ext_param_set = mtk_panel_ext_param_set,
	.mode_switch = mode_switch,
	.hbm_fod_control = panel_hbm_fod_control,
	.hbm_get_state = panel_hbm_get_state,
	.hbm_set_state = panel_hbm_set_state,
	.hbm_get_wait_state = panel_hbm_get_wait_state,
	.hbm_set_wait_state = panel_hbm_set_wait_state,
	.reset = panel_ext_reset,
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
