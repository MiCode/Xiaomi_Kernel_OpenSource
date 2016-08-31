/*
 * arch/arm/mach-tegra/panel-c-lvds-1366-4.c
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/max8831_backlight.h>
#include <linux/leds.h>
#include <linux/ioport.h>
#include "board.h"
#include "board-panel.h"
#include "devices.h"
#include "gpio-names.h"
#include "tegra12_host1x_devices.h"

#define DC_CTRL_MODE	TEGRA_DC_OUT_CONTINUOUS_MODE

#define LVDS_PANEL_BL_PWM	TEGRA_GPIO_PH1

static bool reg_requested;
static bool gpio_requested;
static struct platform_device *disp_device;

static struct regulator *vdd_lcd_bl;
static struct regulator *vdd_lcd_bl_en;
static struct regulator *avdd_lcd;
static struct regulator *vdd_ds_1v8;
static struct regulator	*dvdd_lcd_1v8;

static struct tegra_dc_sd_settings lvds_c_1366_14_sd_settings = {
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

static tegra_dc_bl_output lvds_c_1366_14_bl_output_measured = {
	/*  TODO: linear backlight calibration */
	0,   1,	  2,   3,   4,	 5,   6,   7,
	8,   9,	 10,  11,  12,	13,  14,  15,
	16,  17,  18,  19,  20,	 21,  22,  23,
	24,  25,  26,  27,  28,	 29,  30,  31,
	32,  33,  34,  35,  36,	 37,  38,  39,
	40,  41,  42,  43,  44,	 45,  46,  47,
	48,  49,  50,  51,  52,	 53,  54,  55,
	56,  57,  58,  59,  60,	 61,  62,  63,
	64,  65,  66,  67,  68,	 69,  70,  71,
	72,  73,  74,  75,  76,	 77,  78,  79,
	80,  81,  82,  83,  84,	 85,  86,  87,
	88,  89,  90,  91,  92,	 93,  94,  95,
	96,  97,  98,  99, 100, 101, 102, 103,
	104, 105, 106, 107, 108, 109, 110, 111,
	112, 113, 114, 115, 116, 117, 118, 119,
	120, 121, 122, 123, 124, 125, 126, 127,
	128, 129, 130, 131, 132, 133, 134, 135,
	136, 137, 138, 139, 140, 141, 142, 143,
	144, 145, 146, 147, 148, 149, 150, 151,
	152, 153, 154, 155, 156, 157, 158, 159,
	160, 161, 162, 163, 164, 165, 166, 167,
	168, 169, 170, 171, 172, 173, 174, 175,
	176, 177, 178, 179, 180, 181, 182, 183,
	184, 185, 186, 187, 188, 189, 190, 191,
	192, 193, 194, 195, 196, 197, 198, 199,
	200, 201, 202, 203, 204, 205, 206, 207,
	208, 209, 210, 211, 212, 213, 214, 215,
	216, 217, 218, 219, 220, 221, 222, 223,
	224, 225, 226, 227, 228, 229, 230, 231,
	232, 233, 234, 235, 236, 237, 238, 239,
	240, 241, 242, 243, 244, 245, 246, 247,
	248, 249, 250, 251, 252, 253, 254, 255
};

static int laguna_lvds_regulator_get(struct device *dev)
{
	int err = 0;

	if (reg_requested)
		return 0;

	vdd_ds_1v8 = regulator_get(dev, "vdd_ds_1v8");
	if (IS_ERR(vdd_ds_1v8)) {
		pr_err("vdd_ds_1v8 regulator get failed\n");
		err = PTR_ERR(vdd_ds_1v8);
		vdd_ds_1v8 = NULL;
		goto fail;
	}

	vdd_lcd_bl = regulator_get(dev, "vdd_lcd_bl");
	if (IS_ERR(vdd_lcd_bl)) {
		pr_err("vdd_lcd_bl regulator get failed\n");
		err = PTR_ERR(vdd_lcd_bl);
		vdd_lcd_bl = NULL;
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

	dvdd_lcd_1v8 = regulator_get(dev, "dvdd_lcd");
	if (IS_ERR(dvdd_lcd_1v8)) {
		pr_err("dvdd_lcd regulator get failed\n");
		err = PTR_ERR(dvdd_lcd_1v8);
		dvdd_lcd_1v8 = NULL;
		goto fail;
	}

	reg_requested = true;
	return 0;
fail:
	return err;
}

static int laguna_lvds_gpio_get(void)
{
	int err = 0;

	if (gpio_requested)
		return 0;

	err = gpio_request(LVDS_PANEL_BL_PWM, "panel pwm");
	if (err < 0) {
		pr_err("panel pwm gpio request failed\n");
		goto fail;
	}
	gpio_free(LVDS_PANEL_BL_PWM);

	gpio_requested = true;
	return 0;
fail:
	return err;
}

static int lvds_c_1366_14_enable(struct device *dev)
{
	int err = 0;

	err = laguna_lvds_regulator_get(dev);
	if (err < 0) {
		pr_err("lvds regulator get failed\n");
		goto fail;
	}

	err = laguna_lvds_gpio_get();
	if (err < 0) {
		pr_err("lvds gpio request failed\n");
		goto fail;
	}

	if (vdd_lcd_bl) {
		err = regulator_enable(vdd_lcd_bl);
		if (err < 0) {
			pr_err("vdd_lcd_bl regulator enable failed\n");
			goto fail;
		}
	}

	msleep(20);

	if (vdd_ds_1v8) {
		err = regulator_enable(vdd_ds_1v8);
		if (err < 0) {
			pr_err("vdd_ds_1v8 regulator enable failed\n");
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

	if (dvdd_lcd_1v8) {
		err = regulator_enable(dvdd_lcd_1v8);
		if (err < 0) {
			pr_err("dvdd_lcd_1v8 regulator enable failed\n");
			goto fail;
		}
	}

	return 0;
fail:
	return err;
}

static int lvds_c_1366_14_disable(void)
{
	if (vdd_lcd_bl_en)
		regulator_disable(vdd_lcd_bl_en);

	msleep(10);

	if (avdd_lcd)
		regulator_disable(avdd_lcd);

	if (vdd_ds_1v8)
		regulator_disable(vdd_ds_1v8);

	msleep(10);

	if (vdd_lcd_bl)
		regulator_disable(vdd_lcd_bl);

	msleep(500);

	return 0;
}

static int lvds_c_1366_14_postsuspend(void)
{
	return 0;
}

static struct tegra_dc_out_pin lvds_out_pins[] = {
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

static struct tegra_dc_mode lvds_c_1366_14_modes[] = {
	{
		.pclk = 74720100, /* 1366 x 768 @ 60hz */
		.h_ref_to_sync = 1,
		.v_ref_to_sync = 1,
		.h_sync_width = 45,
		.v_sync_width = 7,
		.h_back_porch = 113,
		.v_back_porch = 21,
		.h_active = 1366,
		.v_active = 768,
		.h_front_porch = 68,
		.v_front_porch = 4,
	},
};

static int lvds_c_1366_14_bl_notify(struct device *unused, int brightness)
{
	int cur_sd_brightness = atomic_read(&sd_brightness);

	/* SD brightness is a percentage */
	brightness = (brightness * cur_sd_brightness) / 255;

	/* Apply any backlight response curve */
	if (brightness > 255)
		pr_info("Error: Brightness > 255!\n");
	else
		brightness = lvds_c_1366_14_bl_output_measured[brightness];

	return brightness;
}

static int lvds_c_1366_14_check_fb(struct device *dev, struct fb_info *info)
{
	return info->device == &disp_device->dev;
}

static struct platform_pwm_backlight_data lvds_c_1366_14_bl_data = {
	.pwm_id		= 1,
	.max_brightness	= 255,
	.dft_brightness	= 224,
	.pwm_period_ns	= 1000000,
	.pwm_gpio	= TEGRA_GPIO_INVALID,
	.notify		= lvds_c_1366_14_bl_notify,
	/* Only toggle backlight on fb blank notifications for disp1 */
	.check_fb	= lvds_c_1366_14_check_fb,
};

static struct platform_device __maybe_unused
		lvds_c_1366_14_bl_device = {
	.name	= "pwm-backlight",
	.id	= -1,
	.dev	= {
		.platform_data = &lvds_c_1366_14_bl_data,
	},
};

static struct platform_device __maybe_unused
			*lvds_c_1366_14_bl_devices[] __initdata = {
	&tegra_pwfm_device,
	&lvds_c_1366_14_bl_device,
};

static int  __init lvds_c_1366_14_register_bl_dev(void)
{
	int err = 0;
	err = platform_add_devices(lvds_c_1366_14_bl_devices,
				ARRAY_SIZE(lvds_c_1366_14_bl_devices));
	if (err) {
		pr_err("disp1 bl device registration failed");
		return err;
	}
	return err;
}

static void lvds_c_1366_14_set_disp_device(
	struct platform_device *laguna_display_device)
{
	disp_device = laguna_display_device;
}

static void lvds_c_1366_14_dc_out_init(struct tegra_dc_out *dc)
{
	dc->align = TEGRA_DC_ALIGN_MSB,
	dc->order = TEGRA_DC_ORDER_RED_BLUE,
	dc->flags = DC_CTRL_MODE;
	dc->modes = lvds_c_1366_14_modes;
	dc->n_modes = ARRAY_SIZE(lvds_c_1366_14_modes);
	dc->out_pins = lvds_out_pins,
	dc->n_out_pins = ARRAY_SIZE(lvds_out_pins),
	dc->depth = 18,
	dc->parent_clk = "pll_d_out0";
	dc->enable = lvds_c_1366_14_enable;
	dc->disable = lvds_c_1366_14_disable;
	dc->postsuspend	= lvds_c_1366_14_postsuspend,
	dc->width = 309;
	dc->height = 174;
}

static void lvds_c_1366_14_fb_data_init(struct tegra_fb_data *fb)
{
	fb->xres = lvds_c_1366_14_modes[0].h_active;
	fb->yres = lvds_c_1366_14_modes[0].v_active;
}

static void
lvds_c_1366_14_sd_settings_init(struct tegra_dc_sd_settings *settings)
{
	*settings = lvds_c_1366_14_sd_settings;
	settings->bl_device_name = "pwm-backlight";
}

struct tegra_panel __initdata lvds_c_1366_14 = {
	.init_sd_settings = lvds_c_1366_14_sd_settings_init,
	.init_dc_out = lvds_c_1366_14_dc_out_init,
	.init_fb_data = lvds_c_1366_14_fb_data_init,
	.register_bl_dev = lvds_c_1366_14_register_bl_dev,
	.set_disp_device = lvds_c_1366_14_set_disp_device,
};
EXPORT_SYMBOL(lvds_c_1366_14);

