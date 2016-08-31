/*
 * arch/arm/mach-tegra/panel-s-edp-wqxgap-15-6.c
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <mach/dc.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/tegra_pwm_bl.h>
#include <linux/regulator/consumer.h>
#include <linux/pwm_backlight.h>
#include <linux/leds.h>
#include <linux/ioport.h>
#include "board.h"
#include "board-panel.h"
#include "devices.h"
#include "gpio-names.h"
#include "tegra12_host1x_devices.h"

#define DC_CTRL_MODE	TEGRA_DC_OUT_CONTINUOUS_MODE

#define EDP_PANEL_BL_PWM	TEGRA_GPIO_PH1

static bool reg_requested;
static bool gpio_requested;
static struct platform_device *disp_device;

static struct regulator *vdd_lcd_bl_en;
static struct regulator *avdd_lcd;
static struct regulator *avdd_3v3_dp;
static struct regulator *vdd_ds_1v8;

static struct tegra_dc_sd_settings edp_s_wqxgap_15_6_sd_settings = {
	.enable = 1, /* enabled by default. */
	.use_auto_pwm = false,
	.hw_update_delay = 0,
	.bin_width = -1,
	.aggressiveness = 5,
	.use_vid_luma = false,
	.phase_in_adjustments = 0,
	.k_limit_enable = true,
	.k_limit = 200,
	.sd_window_enable = false,
	.soft_clipping_enable = true,
	/* Low soft clipping threshold to compensate for aggressive k_limit */
	.soft_clipping_threshold = 128,
	.smooth_k_enable = false,
	.smooth_k_incr = 64,
	/* Default video coefficients */
	.coeff = {5, 9, 2},
	.fc = {0, 0},
	/* Immediate backlight changes */
	.blp = {1024, 255},
	/* Gammas: R: 2.2 G: 2.2 B: 2.2 */
	/* Default BL TF */
	.bltf = {
			{
				{57, 65, 73, 82},
				{92, 103, 114, 125},
				{138, 150, 164, 178},
				{193, 208, 224, 241},
			},
		},
	/* Default LUT */
	.lut = {
			{
				{255, 255, 255},
				{199, 199, 199},
				{153, 153, 153},
				{116, 116, 116},
				{85, 85, 85},
				{59, 59, 59},
				{36, 36, 36},
				{17, 17, 17},
				{0, 0, 0},
			},
		},
	.sd_brightness = &sd_brightness,
	.use_vpulse2 = true,
};

static int shield_edp_regulator_get(struct device *dev)
{
	int err = 0;

	if (reg_requested)
		return 0;

	vdd_ds_1v8 = regulator_get(dev, "vdd_lcd_1v8_s");
	if (IS_ERR(vdd_ds_1v8)) {
		pr_err("vdd_ds_1v8 regulator get failed\n");
		err = PTR_ERR(vdd_ds_1v8);
		vdd_ds_1v8 = NULL;
		goto fail;
	}

	vdd_lcd_bl_en = regulator_get(dev, "vdd_lcd_bl_en");
	if (IS_ERR(vdd_lcd_bl_en)) {
		pr_err("vdd_lcd_bl_en regulator get failed\n");
		err = PTR_ERR(vdd_lcd_bl_en);
		vdd_lcd_bl_en = NULL;
		goto fail;
	}

	avdd_lcd = regulator_get(dev, "avdd_lcd");
	if (IS_ERR(avdd_lcd)) {
		pr_err("avdd_lcd regulator get failed\n");
		err = PTR_ERR(avdd_lcd);
		avdd_lcd = NULL;
		goto fail;
	}

	avdd_3v3_dp = regulator_get(dev, "avdd_3v3_dp");
	if (IS_ERR(avdd_3v3_dp)) {
		pr_err("avdd_3v3_dp regulator get failed\n");
		err = PTR_ERR(avdd_3v3_dp);
		avdd_3v3_dp = NULL;
		goto fail;
	}

	reg_requested = true;
	return 0;
fail:
	return err;
}

static int shield_edp_gpio_get(void)
{
	int err = 0;

	if (gpio_requested)
		return 0;

	gpio_requested = true;

	return err;
}

static int edp_s_wqxgap_15_6_enable(struct device *dev)
{
	int err = 0;

	err = shield_edp_regulator_get(dev);
	if (err < 0) {
		pr_err("edp regulator get failed\n");
		goto fail;
	}

	err = shield_edp_gpio_get();
	if (err < 0) {
		pr_err("edp gpio request failed\n");
		goto fail;
	}

	if (vdd_ds_1v8) {
		err = regulator_enable(vdd_ds_1v8);
		if (err < 0) {
			pr_err("vdd_ds_1v8 regulator enable failed\n");
			goto fail;
		}
	}

	if (avdd_3v3_dp) {
		err = regulator_enable(avdd_3v3_dp);
		if (err < 0) {
			pr_err("avdd_3v3_dp regulator enable failed\n");
			goto fail;
		}
	}

	if (avdd_lcd) {
		err = regulator_enable(avdd_lcd);
		if (err < 0) {
			pr_err("avdd_lcd regulator enable failed\n");
			goto fail;
		}
	}

	msleep(10);

	if (vdd_lcd_bl_en) {
		err = regulator_enable(vdd_lcd_bl_en);
		if (err < 0) {
			pr_err("vdd_lcd_bl_en regulator enable failed\n");
			goto fail;
		}
	}
	msleep(200);

	return 0;
fail:
	return err;
}

static int edp_s_wqxgap_15_6_disable(void)
{
	if (vdd_lcd_bl_en)
		regulator_disable(vdd_lcd_bl_en);

	if (avdd_lcd)
		regulator_disable(avdd_lcd);

	if (avdd_3v3_dp)
		regulator_disable(avdd_3v3_dp);

	if (vdd_ds_1v8)
		regulator_disable(vdd_ds_1v8);

	msleep(50);

	return 0;
}

static int edp_s_wqxgap_15_6_postsuspend(void)
{
	return 0;
}

static struct tegra_dc_out_pin edp_out_pins[] = {
	{
		.name   = TEGRA_DC_OUT_PIN_H_SYNC,
		.pol    = TEGRA_DC_OUT_PIN_POL_LOW,
	},
	{
		.name   = TEGRA_DC_OUT_PIN_V_SYNC,
		.pol    = TEGRA_DC_OUT_PIN_POL_LOW,
	},
	{
		.name   = TEGRA_DC_OUT_PIN_PIXEL_CLOCK,
		.pol    = TEGRA_DC_OUT_PIN_POL_LOW,
	},
	{
		.name   = TEGRA_DC_OUT_PIN_DATA_ENABLE,
		.pol    = TEGRA_DC_OUT_PIN_POL_HIGH,
	},
};

static struct tegra_dc_mode edp_s_wqxgap_15_6_modes[] = {
	{
		.pclk = 373680000,
		.h_ref_to_sync = 1,
		.v_ref_to_sync = 1,
		.h_sync_width = 32,
		.v_sync_width = 5,
		.h_back_porch = 80,
		.v_back_porch = 44,
		.h_active = 3200,
		.v_active = 1800,
		.h_front_porch = 48,
		.v_front_porch = 3,
	},
};

static int edp_s_wqxgap_15_6_bl_notify(struct device *unused, int brightness)
{
	int cur_sd_brightness = atomic_read(&sd_brightness);

	/* SD brightness is a percentage */
	brightness = (brightness * cur_sd_brightness) / 255;

	/* Apply any backlight response curve */
	if (brightness > 255)
		pr_info("Error: Brightness > 255!\n");

	return brightness;
}

static int edp_s_wqxgap_15_6_check_fb(struct device *dev, struct fb_info *info)
{
	return info->device == &disp_device->dev;
}

static struct platform_pwm_backlight_data edp_s_wqxgap_15_6_bl_data = {
	.pwm_id		= 1,
	.max_brightness	= 255,
	.dft_brightness	= 224,
	.pwm_period_ns	= 5000000,
	.pwm_gpio	= EDP_PANEL_BL_PWM,
	.notify		= edp_s_wqxgap_15_6_bl_notify,
	/* Only toggle backlight on fb blank notifications for disp1 */
	.check_fb	= edp_s_wqxgap_15_6_check_fb,
};

static struct platform_device __maybe_unused
		edp_s_wqxgap_15_6_bl_device = {
	.name	= "pwm-backlight",
	.id	= -1,
	.dev	= {
		.platform_data = &edp_s_wqxgap_15_6_bl_data,
	},
};

static struct platform_device __maybe_unused
			*edp_s_wqxgap_15_6_bl_devices[] __initdata = {
	&tegra_pwfm_device,
	&edp_s_wqxgap_15_6_bl_device,
};

static int  __init edp_s_wqxgap_15_6_register_bl_dev(void)
{
	int err = 0;
	err = platform_add_devices(edp_s_wqxgap_15_6_bl_devices,
				ARRAY_SIZE(edp_s_wqxgap_15_6_bl_devices));
	if (err) {
		pr_err("disp1 bl device registration failed");
		return err;
	}
	return err;
}

static void edp_s_wqxgap_15_6_set_disp_device(
	struct platform_device *shield_display_device)
{
	disp_device = shield_display_device;
}

static void edp_s_wqxgap_15_6_dc_out_init(struct tegra_dc_out *dc)
{
	dc->align = TEGRA_DC_ALIGN_MSB;
	dc->order = TEGRA_DC_ORDER_RED_BLUE;
	dc->flags = DC_CTRL_MODE;
	dc->modes = edp_s_wqxgap_15_6_modes;
	dc->n_modes = ARRAY_SIZE(edp_s_wqxgap_15_6_modes);
	dc->out_pins = edp_out_pins;
	dc->n_out_pins = ARRAY_SIZE(edp_out_pins);
	dc->depth = 24;
	dc->parent_clk = "pll_d_out0";
	dc->enable = edp_s_wqxgap_15_6_enable;
	dc->disable = edp_s_wqxgap_15_6_disable;
	dc->postsuspend	= edp_s_wqxgap_15_6_postsuspend;
	dc->width = 346;
	dc->height = 194;
	dc->hotplug_gpio = TEGRA_GPIO_PFF0;
}

static void edp_s_wqxgap_15_6_fb_data_init(struct tegra_fb_data *fb)
{
	fb->xres = edp_s_wqxgap_15_6_modes[0].h_active;
	fb->yres = edp_s_wqxgap_15_6_modes[0].v_active;
}

static void
edp_s_wqxgap_15_6_sd_settings_init(struct tegra_dc_sd_settings *settings)
{
	*settings = edp_s_wqxgap_15_6_sd_settings;
	settings->bl_device_name = "pwm-backlight";
}

struct tegra_panel __initdata edp_s_wqxgap_15_6 = {
	.init_sd_settings = edp_s_wqxgap_15_6_sd_settings_init,
	.init_dc_out = edp_s_wqxgap_15_6_dc_out_init,
	.init_fb_data = edp_s_wqxgap_15_6_fb_data_init,
	.register_bl_dev = edp_s_wqxgap_15_6_register_bl_dev,
	.set_disp_device = edp_s_wqxgap_15_6_set_disp_device,
};
EXPORT_SYMBOL(edp_s_wqxgap_15_6);

