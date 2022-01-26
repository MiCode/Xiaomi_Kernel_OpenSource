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
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>

#include <drm/drm_notifier_odm.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mtk_drm_graphics_base.h"
#include "../mediatek/mtk_log.h"
#include "../mediatek/mtk_panel_ext.h"
#endif

#include "lcm_cust_common.h"

#include <touch/nova36672c/nt36xxx.h>
extern struct nvt_ts_data *ts;
static int vsp_vsn_status;

#define HFP_SUPPORT 0

#define PHYSICAL_WIDTH              68785
#define PHYSICAL_HEIGHT             152856
static int flag_regulator = 0;
#if HFP_SUPPORT
static int current_fps = 60;
#endif
//static char bl_tb0[] = { 0x51, 0xff };

extern bool g_trigger_disp_esd_recovery;
extern void nvt_bootloader_reset_locked(void);
extern int32_t nvt_esd_vdd_tp_recovery(void);

struct csot {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *lcm_bl_en_gpio;
    struct gpio_desc *pm_gpio;
	struct gpio_desc *bias_pos;
	struct gpio_desc *bias_neg;
	bool prepared;
	bool enabled;
	int error;
};

static struct regulator *lcd_dvdd_ldo;
struct drm_notifier_data g_notify_data1;

#define csot_dcs_write_seq(ctx, seq...)                                         \
	({                                                                     \
		const u8 d[] = { seq };                                        \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		csot_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

#define csot_dcs_write_seq_static(ctx, seq...)                                  \
	({                                                                     \
		static const u8 d[] = { seq };                                 \
		csot_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

extern int lm36273_brightness_set(int level);
extern int get_panel_dead_status(void);

static inline struct csot *panel_to_csot(struct drm_panel *panel)
{
	return container_of(panel, struct csot, panel);
}

#ifdef PANEL_SUPPORT_READBACK
static int csot_dcs_read(struct csot *ctx, u8 cmd, void *data, size_t len)
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

static void csot_panel_get_data(struct csot *ctx)
{
	u8 buffer[3] = { 0 };
	static int ret;

	pr_info("%s+\n", __func__);

	if (ret == 0) {
		ret = csot_dcs_read(ctx, 0x0A, buffer, 1);
		pr_info("%s  0x%08x\n", __func__, buffer[0] | (buffer[1] << 8));
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

static void csot_dcs_write(struct csot *ctx, const void *data, size_t len)
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


static void csot_panel_init(struct csot *ctx)
{
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	usleep_range(10 * 1000, 11 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(10 * 1000, 11 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10 * 1000, 11 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(10 * 1000, 11 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10 * 1000, 15 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	pr_info("%s+\n", __func__);
	//add novatek vdd tp recovery workround.
	if (get_panel_dead_status()) {
		nvt_bootloader_reset_locked();
		csot_dcs_write_seq_static(ctx, 0xFF, 0xC0);
		csot_dcs_write_seq_static(ctx, 0x4B, 0x00);
		msleep(1000);
		nvt_esd_vdd_tp_recovery();
		msleep(1000);
		csot_dcs_write_seq_static(ctx, 0xFF, 0xC0);
		csot_dcs_write_seq_static(ctx, 0x4B, 0x0E);
		msleep(50);
	}
#if HFP_SUPPORT
	pr_info("%s, fps:%d\n", __func__, current_fps);
	csot_dcs_write_seq_static(ctx, 0xFF, 0x25);
	csot_dcs_write_seq_static(ctx, 0xFB, 0x01);
	if (current_fps == 60)
		csot_dcs_write_seq_static(ctx, 0x18, 0x21);
	else if (current_fps == 90)
		csot_dcs_write_seq_static(ctx, 0x18, 0x20);
	else
		csot_dcs_write_seq_static(ctx, 0x18, 0x21);
#endif
	//csot_dcs_write_seq_static(ctx, 0xFF, 0xD0);
	//csot_dcs_write_seq_static(ctx, 0x00, 0x77);
	//csot_dcs_write_seq_static(ctx, 0x02, 0xAA);
	//csot_dcs_write_seq_static(ctx, 0x09, 0xD0);


	csot_dcs_write_seq_static(ctx, 0xFF, 0x10);
	csot_dcs_write_seq_static(ctx, 0xFB, 0x01);

	csot_dcs_write_seq_static(ctx, 0xC0, 0x00);

	csot_dcs_write_seq_static(ctx, 0xFF, 0xE0);
	csot_dcs_write_seq_static(ctx, 0xFB, 0x01);
	csot_dcs_write_seq_static(ctx, 0x35, 0x82);

	csot_dcs_write_seq_static(ctx, 0xFF, 0xF0);
	csot_dcs_write_seq_static(ctx, 0xFB, 0x01);
	csot_dcs_write_seq_static(ctx, 0x1C, 0x01);
	csot_dcs_write_seq_static(ctx, 0x33, 0x01);
	csot_dcs_write_seq_static(ctx, 0x5A, 0x00);

	csot_dcs_write_seq_static(ctx, 0xFF, 0xD0);
	csot_dcs_write_seq_static(ctx, 0xFB, 0x01);
	csot_dcs_write_seq_static(ctx, 0x53, 0x22);
	csot_dcs_write_seq_static(ctx, 0x54, 0x02);

	csot_dcs_write_seq_static(ctx, 0xFF, 0xC0);
	csot_dcs_write_seq_static(ctx, 0xFB, 0x01);
	csot_dcs_write_seq_static(ctx, 0x9C, 0x11);
	csot_dcs_write_seq_static(ctx, 0x9D, 0x11);

	csot_dcs_write_seq_static(ctx, 0xFF, 0x10);
	csot_dcs_write_seq_static(ctx, 0x35, 0x00);

	csot_dcs_write_seq_static(ctx, 0x11);
	usleep_range(70000, 70001);
	/* Display On*/
	csot_dcs_write_seq_static(ctx, 0x29);
	//usleep_range(20000, 20001);
	pr_info("%s-\n", __func__);
}

static int csot_disable(struct drm_panel *panel)
{
	struct csot *ctx = panel_to_csot(panel);

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return 0;
}

static int lcd_get_dvdd(struct device *dev)
{
	int ret = 0;

	lcd_dvdd_ldo = devm_regulator_get_optional(dev, "lcd_dvdd");
	if (IS_ERR(lcd_dvdd_ldo)) {	/* handle return value */
		flag_regulator = 0;
		ret = PTR_ERR(lcd_dvdd_ldo);
		pr_err("get lcd_dvdd_ldo fail:%d\n", ret);
		return ret;
	}
	flag_regulator = 1;

	return 0;
}

static int lcd_enable_dvdd(void)
{
	int ret, retval;

	ret = regulator_set_voltage(lcd_dvdd_ldo, 1300000, 1300000);
	if (ret < 0)
		pr_err("set voltage lcd_dvdd_ldo fail, ret = %d\n", ret);
	retval |= ret;

	/* enable regulator */
	ret = regulator_enable(lcd_dvdd_ldo);
	if (ret < 0)
		pr_err("enable regulator lcd_dvdd_ldo fail, ret = %d\n", ret);
	retval |= ret;

	return retval;
}

static int csot_unprepare(struct drm_panel *panel)
{

	struct csot *ctx = panel_to_csot(panel);
	int ret = 0;
	struct drm_panel_notifier notifier_data;
	int power_status;
	int blank;
	pr_info("nvt : %s++\n", __func__);

	if (!ctx->prepared)
		return 0;

	if((ts != NULL)&&(ts->panel_tp_flag == 1)){
		power_status = DRM_PANEL_BLANK_POWERDOWN;
		blank = DRM_BLANK_POWERDOWN;
		notifier_data.data = &power_status;
		g_notify_data1.data = &blank;
		notifier_data.refresh_rate = 60;
		notifier_data.id = 1;

		drm_panel_notifier_call_chain(panel, DRM_PANEL_EARLY_EVENT_BLANK, &notifier_data);
		drm_notifier_call_chain(DRM_EVENT_BLANK, &g_notify_data1);
		pr_err("[XMFP]-[NVT] : %s ++++ blank = DRM_BLANK_POWERDOWN ++++", __func__);
	}

	ret = wait_for_completion_timeout(&ts->tp_to_lcd, msecs_to_jiffies(800));
	if(ret == 0){
		pr_err("[NVT]: %s unprepare wait for compeltion tp_to_lcd timeout\n", __func__);
	}

	if(ts->gesture_enabled){
		ret = wait_for_completion_timeout(&ts->drm_tp_lcd, msecs_to_jiffies(150));
		if(ret == 0){
			pr_err("[NVT]: %s unprepare wait for compeltion drm_tp_lcd timeout\n", __func__);
		}
	}

	msleep(30);

	csot_dcs_write_seq_static(ctx, 0x28);
	usleep_range(20000, 20001);
	csot_dcs_write_seq_static(ctx, 0x10);
	usleep_range(100000, 100001);

	//ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	//gpiod_set_value(ctx->reset_gpio, 0);
	//devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	//usleep_range(2000, 2001);

	if((ts->gesture_enabled == false) || get_panel_dead_status()){
		ctx->bias_pos = devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->bias_pos, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_pos);

		usleep_range(2000, 2001);

		lm36273_bias_enable(0, 3);
		usleep_range(2000, 2001);
		ctx->bias_neg = devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->bias_neg, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_neg);
		usleep_range(2000, 2001);
		vsp_vsn_status = 0;
	}

	if(flag_regulator) {
		ret = regulator_disable(lcd_dvdd_ldo);
		if (ret < 0)
			pr_err("disable regulator lcd_dvdd_ldo fail, ret = %d\n", ret);
	} else {
		ret = lcd_get_dvdd(ctx->dev);
		if (!ret)
			ret = regulator_disable(lcd_dvdd_ldo);
		if (ret < 0)
			pr_err("disable regulator lcd_dvdd_ldo fail, ret = %d\n", ret);

	}

	usleep_range(2000, 2001);

	ctx->pm_gpio = devm_gpiod_get(ctx->dev, "pm-enable", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->pm_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->pm_gpio);

	ctx->error = 0;
	ctx->prepared = false;

	return 0;
}

static int csot_prepare(struct drm_panel *panel)
{
	struct csot *ctx = panel_to_csot(panel);
	int ret;
	int blank;
	int power_status;
	struct drm_panel_notifier notifier_data;

	pr_info("nvt: %s+\n", __func__);
	if (ctx->prepared)
		return 0;

	// reset  L
	//ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	//gpiod_set_value(ctx->reset_gpio, 0);
	//devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	//usleep_range(10000, 10001);

	//ctx->pm_gpio = devm_gpiod_get(ctx->dev, "pm-enable", GPIOD_OUT_HIGH);
	//gpiod_set_value(ctx->pm_gpio, 1);
	//devm_gpiod_put(ctx->dev, ctx->pm_gpio);
	//usleep_range(10000, 10001);

	/* set voltage with min & max*/
	if(flag_regulator)
		lcd_enable_dvdd();
	else {
		ret = lcd_get_dvdd(ctx->dev);
		if (!ret)
			lcd_enable_dvdd();
	}
	usleep_range(2000, 2001);

	ctx->lcm_bl_en_gpio = devm_gpiod_get_index(ctx->dev, "lcm-bl-enable", 0, GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->lcm_bl_en_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->lcm_bl_en_gpio);
	usleep_range(2000, 2001);

	lm36273_bl_bias_conf();

	if((ts->gesture_enabled == false) || !vsp_vsn_status || get_panel_dead_status()){
		lm36273_bias_enable(1, 1);
		mdelay(10);
		ctx->bias_pos = devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->bias_pos, 1);
		devm_gpiod_put(ctx->dev, ctx->bias_pos);
		usleep_range(2000, 2001);

		ctx->bias_neg = devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->bias_neg, 1);
		devm_gpiod_put(ctx->dev, ctx->bias_neg);
		vsp_vsn_status = 1;
	}

	csot_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0){
		csot_unprepare(panel);
		ctx->prepared = false;
		return ret;
	}
	ctx->prepared = true;
#ifdef PANEL_SUPPORT_READBACK
	csot_panel_get_data(ctx);
#endif

	if((ts != NULL)&&(ts->panel_tp_flag == 1)){
		power_status = DRM_PANEL_BLANK_UNBLANK;
		blank = DRM_BLANK_UNBLANK;
		notifier_data.data = &power_status;
		g_notify_data1.data = &blank;
		notifier_data.refresh_rate = 60;
		notifier_data.id = 1;

		drm_panel_notifier_call_chain(panel, DRM_PANEL_EVENT_BLANK, &notifier_data);
		drm_notifier_call_chain(DRM_EVENT_BLANK, &g_notify_data1);
		pr_err("[XMFP]-[NVT] : %s ++++ blank = DRM_BLANK_UNBLANK ++++", __func__);
	}

	pr_info("%s-\n", __func__);
	return ret;
}

static int csot_enable(struct drm_panel *panel)
{
	struct csot *ctx = panel_to_csot(panel);

	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;

	return 0;
}
#if HFP_SUPPORT
#define HFP_60HZ (256)
#define HFP_90HZ (256)
#define HSA (20)
#define HBP (40)
#define VFP (54)
#define VFP_45HZ (54)
#define VFP_60HZ (54)
#define VSA (2)
#define VBP (18)
#define VAC (2400)
#define HAC (1080)
static const struct drm_display_mode default_mode = {
	.clock = 306826,
	.hdisplay = HAC,
	.hsync_start = HAC + HFP_60HZ,
	.hsync_end = HAC + HFP_60HZ + HSA,
	.htotal = HAC + HFP_60HZ + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + VFP,
	.vsync_end = VAC + VFP + VSA,
	.vtotal = VAC + VFP + VSA + VBP,
	.vrefresh = 90,
};

static const struct drm_display_mode performance_mode = {
	.clock = 306677,
	.hdisplay = HAC,
	.hsync_start = HAC + HFP_90HZ,
	.hsync_end = HAC + HFP_90HZ + HSA,
	.htotal = HAC + HFP_90HZ + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + VFP,
	.vsync_end = VAC + VFP + VSA,
	.vtotal = VAC + VFP + VSA + VBP,
	.vrefresh = 90,
};
#else
#define HFP (228)
#define HSA (20)
#define HBP (36)
#define VFP_50HZ (2070)
#define VFP_60HZ (1290)
#define VFP_90HZ (54)
#define VSA (8)
#define VBP (12)
#define VAC (2400)
#define HAC (1080)
static const struct drm_display_mode default_mode = {
	.clock = 303626,
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,  //1140
	.vdisplay = VAC,
	.vsync_start = VAC + VFP_60HZ,
	.vsync_end = VAC + VFP_60HZ + VSA,
	.vtotal = VAC + VFP_60HZ + VSA + VBP,    //3710
	.vrefresh = 60,
};

static const struct drm_display_mode performance_mode = {
	.clock = 303708,
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,    //1140
	.vdisplay = VAC,
	.vsync_start = VAC + VFP_90HZ,
	.vsync_end = VAC + VFP_90HZ + VSA,
	.vtotal = VAC + VFP_90HZ + VSA + VBP,   //2474
	.vrefresh = 90,
};

static const struct drm_display_mode refresh_50_mode = {
	.clock = 306218,
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,  //1140
	.vdisplay = VAC,
	.vsync_start = VAC + VFP_50HZ,
	.vsync_end = VAC + VFP_50HZ + VSA,
	.vtotal = VAC + VFP_50HZ + VSA + VBP,    //3710
	.vrefresh = 50,
};
#endif

#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params = {
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.pll_clk = 535,
	.vfp_low_power = VFP_50HZ,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.is_cphy = 1,
	.data_rate = 1070,
	.dyn_fps = {
		.switch_en = 1,
#if HFP_SUPPORT
		.dfps_cmd_table[0] = {0, 2, {0xFF, 0x25} },
		.dfps_cmd_table[1] = {0, 2, {0xFB, 0x01} },
		.dfps_cmd_table[2] = {0, 2, {0x18, 0x21} },
		/*switch page for esd check*/
		.dfps_cmd_table[3] = {0, 2, {0xFF, 0x10} },
		.dfps_cmd_table[4] = {0, 2, {0xFB, 0x01} },
#else
		.vact_timing_fps = 90,
#endif
	},
	.phy_timcon = {
		.hs_zero = 35,
		.hs_trail = 26,
		.hs_prpr = 11,
	},
	.dyn = {
		.switch_en = 1,
		.pll_clk = 532,
		.hbp = 28,
		.hfp = HFP,
		.vfp = VFP_60HZ,
		.data_rate = 1064,
		.vfp_lp_dyn = VFP_50HZ,
	},

};

static struct mtk_panel_params ext_params_90hz = {
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.pll_clk = 535,
	.vfp_low_power = VFP_50HZ,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {

		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.is_cphy = 1,
	.data_rate = 1070,
	.dyn_fps = {
		.switch_en = 1,
#if HFP_SUPPORT
		.dfps_cmd_table[0] = {0, 2, {0xFF, 0x25} },
		.dfps_cmd_table[1] = {0, 2, {0xFB, 0x01} },
		.dfps_cmd_table[2] = {0, 2, {0x18, 0x20} },
		/*switch page for esd check*/
		.dfps_cmd_table[3] = {0, 2, {0xFF, 0x10} },
		.dfps_cmd_table[4] = {0, 2, {0xFB, 0x01} },
#else
		.vact_timing_fps = 90,
#endif
	},
	.phy_timcon = {
		.hs_zero = 35,
		.hs_trail = 26,
		.hs_prpr = 11,
	},
	.dyn = {
		.switch_en = 1,
		.pll_clk = 532,
		.hbp = 28,
		.hfp = HFP,
		.vfp = VFP_90HZ,
		.data_rate = 1064,
		.vfp_lp_dyn = VFP_50HZ,
	},

};

static struct mtk_panel_params ext_params_50hz = {
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.pll_clk = 535,
	.vfp_low_power = VFP_50HZ,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.is_cphy = 1,
	.data_rate = 1070,
	.dyn_fps = {
		.switch_en = 1,
#if HFP_SUPPORT
		.dfps_cmd_table[0] = {0, 2, {0xFF, 0x25} },
		.dfps_cmd_table[1] = {0, 2, {0xFB, 0x01} },
		.dfps_cmd_table[2] = {0, 2, {0x18, 0x20} },
		/*switch page for esd check*/
		.dfps_cmd_table[3] = {0, 2, {0xFF, 0x10} },
		.dfps_cmd_table[4] = {0, 2, {0xFB, 0x01} },
#else
		.vact_timing_fps = 90,
#endif
	},
	.phy_timcon = {
		.hs_zero = 35,
		.hs_trail = 26,
		.hs_prpr = 11,
	},
	.dyn = {
		.switch_en = 1,
		.pll_clk = 532,
		.hbp = 28,
		.hfp = HFP,
		.vfp = VFP_50HZ,
		.data_rate = 1064,
		.vfp_lp_dyn = VFP_50HZ,
	},

};

static int csot_panel_ata_check(struct drm_panel *panel)
{
	/* Customer test by own ATA tool */
	return 1;
}

static int last_level;

static int csot_setbacklight_cmdq(void *dsi, dcs_write_gce cb, void *handle,
				 unsigned int level)
{
	char bl_tb0[] = {0xff, 0x10};
	char bl_tb1[] = {0x22, 0x00};
	char bl_tb2[] = {0x13, 0x00};

	if (!level) {
		cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
		cb(dsi, handle, bl_tb1, ARRAY_SIZE(bl_tb1));
		usleep_range(30000, 30010);
	}
	if (!last_level) {
		cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
		cb(dsi, handle, bl_tb2, ARRAY_SIZE(bl_tb2));
	}
	lm36273_brightness_set(level);
	last_level = level;

	return 0;
}

static struct drm_display_mode *get_mode_by_id_hfp(struct drm_panel *panel,
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

static int csot_mtk_panel_ext_param_set(struct drm_panel *panel, unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id_hfp(panel, mode);

	if (m->vrefresh == 60) {
		ext->params = &ext_params;
#if HFP_SUPPORT
		current_fps = 60;
#endif
	} else if (m->vrefresh == 90) {
		ext->params = &ext_params_90hz;
#if HFP_SUPPORT
		current_fps = 90;
#endif
	} else if (m->vrefresh == 50) {
		ext->params = &ext_params_50hz;
#if HFP_SUPPORT
		current_fps = 50;
#endif
	} else
		ret = 1;

	return ret;
}

static int csot_mtk_panel_ext_param_get(struct mtk_panel_params *ext_para,
			 unsigned int mode)
{
	int ret = 0;

	if (mode == 0)
		ext_para = &ext_params;
	else if (mode == 1)
		ext_para = &ext_params_90hz;
	else if (mode == 2)
		ext_para = &ext_params_50hz;
	else
		ret = 1;

	return ret;

}

static void csot_mode_switch_to_90(struct drm_panel *panel)
{
	struct csot *ctx = panel_to_csot(panel);

	//csot_dcs_write_seq_static(ctx, 0xFF, 0x25);
	//csot_dcs_write_seq_static(ctx, 0xFB, 0x01);
	//csot_dcs_write_seq_static(ctx, 0x18, 0x20);//90hz

}

static void csot_mode_switch_to_60(struct drm_panel *panel)
{
	struct csot *ctx = panel_to_csot(panel);

	//csot_dcs_write_seq_static(ctx, 0xFF, 0x25);
	//csot_dcs_write_seq_static(ctx, 0xFB, 0x01);
	//csot_dcs_write_seq_static(ctx, 0x18, 0x21);
}

static int csot_mode_switch(struct drm_panel *panel, unsigned int cur_mode,
		unsigned int dst_mode, enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	int ret = 0;
	//struct drm_display_mode *m = get_mode_by_id(panel, dst_mode);

	pr_info("%s cur_mode = %d dst_mode %d\n", __func__, cur_mode, dst_mode);

	if (dst_mode == 60) { /* 60 switch to 120 */
		csot_mode_switch_to_60(panel);
	} else if (dst_mode == 90) { /* 1200 switch to 60 */
		csot_mode_switch_to_90(panel);
	} else
		ret = 1;

	return ret;
}

static int csot_panel_ext_reset(struct drm_panel *panel, int on)
{
	struct csot *ctx = panel_to_csot(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = csot_panel_ext_reset,
	.set_backlight_cmdq = csot_setbacklight_cmdq,
	.ext_param_set = csot_mtk_panel_ext_param_set,
	.ext_param_get = csot_mtk_panel_ext_param_get,
	.mode_switch = csot_mode_switch,
	.ata_check = csot_panel_ata_check,
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
	 *	   become ready and start receiving video data
	 * @enable: the time (in milliseconds) that it takes for the panel to
	 *	  display the first valid frame after starting to receive
	 *	  video data
	 * @disable: the time (in milliseconds) that it takes for the panel to
	 *	   turn the display off (no content is visible)
	 * @unprepare: the time (in milliseconds) that it takes for the panel
	 *		 to power itself down completely
	 */
	struct {
		unsigned int prepare;
		unsigned int enable;
		unsigned int disable;
		unsigned int unprepare;
	} delay;
};

static int csot_get_modes(struct drm_panel *panel)
{
	struct drm_display_mode *mode;
	struct drm_display_mode *mode2;
	struct drm_display_mode *mode3;

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
			 performance_mode.hdisplay, performance_mode.vdisplay,
			 performance_mode.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode2);
	mode2->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(panel->connector, mode2);

	mode3 = drm_mode_duplicate(panel->drm, &refresh_50_mode);
	if (!mode3) {
		dev_info(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
			 refresh_50_mode.hdisplay, refresh_50_mode.vdisplay,
			 refresh_50_mode.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode3);
	mode3->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(panel->connector, mode3);

	return 1;
}

static const struct drm_panel_funcs csot_drm_funcs = {
	.disable = csot_disable,
	.unprepare = csot_unprepare,
	.prepare = csot_prepare,
	.enable = csot_enable,
	.get_modes = csot_get_modes,
};

static int csot_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct csot *ctx;
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

	pr_err("nvt: %s, dev->of_node->full_name = %s\n",__func__,(dev->of_node)->full_name);
	pr_info("%s+\n", __func__);

	ctx = devm_kzalloc(dev, sizeof(struct csot), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 3;
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
    ctx->pm_gpio = devm_gpiod_get(dev, "pm-enable", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->pm_gpio)) {
		dev_info(dev, "cannot get pm_gpio %ld\n",
			 PTR_ERR(ctx->pm_gpio));
		return PTR_ERR(ctx->pm_gpio);
	}
	devm_gpiod_put(dev, ctx->pm_gpio);

	ctx->lcm_bl_en_gpio = devm_gpiod_get(dev, "lcm-bl-enable", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->lcm_bl_en_gpio)) {
		dev_info(dev, "cannot get lcm-bl-enable-gpio %ld\n",
			 PTR_ERR(ctx->lcm_bl_en_gpio));
		return PTR_ERR(ctx->lcm_bl_en_gpio);
	}
	gpiod_set_value(ctx->lcm_bl_en_gpio, 1);
	devm_gpiod_put(dev, ctx->lcm_bl_en_gpio);

	ctx->bias_pos = devm_gpiod_get_index(dev, "bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_info(dev, "cannot get bias-gpios 0 %ld\n",
			 PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	devm_gpiod_put(dev, ctx->bias_pos);

	ctx->bias_neg = devm_gpiod_get_index(dev, "bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_info(dev, "cannot get bias-gpios 1 %ld\n",
			 PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	devm_gpiod_put(dev, ctx->bias_neg);
	ctx->prepared = true;
	ctx->enabled = true;
	drm_panel_init(&ctx->panel);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &csot_drm_funcs;

	ret = drm_panel_add(&ctx->panel);
	if (ret < 0)
		return ret;


	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;

#endif
	if(dev == NULL)
		pr_err("dev is NULL\n");
	if (0 == lcd_get_dvdd(dev))
		lcd_enable_dvdd();
	pr_info("%s- wt,csot,nt36672c,cphy,vdo,90hz\n", __func__);

	return ret;
}

static int csot_remove(struct mipi_dsi_device *dsi)
{
	struct csot *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id csot_of_match[] = {
	{
	    .compatible = "k16a_42_02_0b_vdo,lcm",
	},
	{}
};

MODULE_DEVICE_TABLE(of, csot_of_match);

static struct mipi_dsi_driver csot_driver = {
	.probe = csot_probe,
	.remove = csot_remove,
	.driver = {
		.name = "k16a_42_02_0b_vdo",
		.owner = THIS_MODULE,
		.of_match_table = csot_of_match,
	},
};

module_mipi_dsi_driver(csot_driver);

MODULE_AUTHOR("samir.liu <samir.liu@mediatek.com>");
MODULE_DESCRIPTION("wt csot nt36672c vdo Panel Driver");
MODULE_LICENSE("GPL v2");
