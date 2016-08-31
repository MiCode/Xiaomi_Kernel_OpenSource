/*
 * arch/arm/mach-tegra/panel-j-720p-4-7.c
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/export.h>

#include "gpio-names.h"
#include "board-panel.h"
#include "devices.h"

#define DSI_PANEL_RESET         0

#define DC_CTRL_MODE            (TEGRA_DC_OUT_ONE_SHOT_MODE | \
			 TEGRA_DC_OUT_ONE_SHOT_LP_MODE)

static struct regulator *vdd_lcd_s_1v8;
static struct regulator *vdd_sys_bl_3v7;
static struct regulator *avdd_lcd_3v0_2v8;


static bool dsi_j_720p_4_7_reg_requested;
static bool dsi_j_720p_4_7_gpio_requested;
static bool is_bl_powered;
static struct platform_device *disp_device;

static struct tegra_dc_sd_settings dsi_j_720p_4_7_sd_settings = {
	.enable = 1, /* enabled by default */
	.use_auto_pwm = false,
	.hw_update_delay = 0,
	.bin_width = -1,
	.aggressiveness = 5,
	.use_vid_luma = false,
	.phase_in_adjustments = 0,
	.k_limit_enable = true,
	.k_limit = 180,
	.sd_window_enable = false,
	.soft_clipping_enable = true,
	/* Low soft clipping threshold to compensate for aggressive k_limit */
	.soft_clipping_threshold = 128,
	.smooth_k_enable = true,
	.smooth_k_incr = 4,
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

static tegra_dc_bl_output dsi_j_720p_4_7_bl_response_curve = {
	0, 1, 2, 3, 4, 5, 6, 7,
	8, 9, 10, 11, 12, 13, 14, 15,
	16, 18, 19, 20, 21, 22, 23, 25,
	26, 27, 28, 30, 31, 32, 33, 35,
	36, 38, 39, 41, 42, 43, 45, 46,
	48, 49, 51, 52, 54, 55, 57, 58,
	60, 61, 63, 64, 66, 67, 68, 70,
	71, 72, 74, 75, 77, 78, 79, 80,
	81, 82, 83, 85, 86, 87, 88, 89,
	90, 90, 91, 92, 93, 93, 94, 95,
	96, 96, 96, 97, 97, 97, 97, 98,
	98, 98, 98, 99, 100, 101, 101, 102,
	103, 104, 104, 105, 106, 107, 108, 109,
	110, 112, 113, 114, 115, 116, 117, 119,
	120, 121, 122, 123, 125, 126, 127, 128,
	129, 131, 132, 133, 134, 135, 136, 137,
	138, 140, 141, 142, 142, 143, 144, 145,
	146, 147, 148, 149, 149, 150, 151, 152,
	153, 154, 154, 155, 156, 157, 158, 159,
	160, 162, 163, 164, 165, 167, 168, 169,
	170, 171, 172, 173, 173, 174, 175, 176,
	176, 177, 178, 179, 179, 180, 181, 182,
	182, 183, 184, 184, 185, 186, 186, 187,
	188, 188, 189, 189, 190, 190, 191, 192,
	193, 194, 195, 195, 196, 197, 198, 199,
	200, 201, 202, 203, 203, 204, 205, 206,
	207, 208, 209, 210, 211, 212, 213, 213,
	214, 215, 216, 217, 218, 219, 220, 221,
	222, 224, 225, 226, 227, 228, 229, 230,
	231, 232, 233, 234, 235, 236, 237, 238,
	239, 240, 241, 242, 243, 244, 246, 247,
	248, 249, 250, 251, 252, 253, 254, 255
};

static int __maybe_unused dsi_j_720p_4_7_bl_notify(struct device *unused,
							int brightness)
{
	int cur_sd_brightness = atomic_read(&sd_brightness);

	/* SD brightness is a percentage */
	brightness = (brightness * cur_sd_brightness) / 255;

	/* Apply any backlight response curve */
	if (brightness > 255)
		pr_info("Error: Brightness > 255!\n");
	else
		brightness = dsi_j_720p_4_7_bl_response_curve[brightness];

	return brightness;
}

static int __maybe_unused dsi_j_720p_4_7_check_fb(struct device *dev,
					     struct fb_info *info)
{
	return info->device == &disp_device->dev;
}

/*
	JDI uses Platform blacklight device
*/

static struct platform_pwm_backlight_data dsi_j_720p_4_7_bl_data = {
#ifdef CONFIG_ARCH_TEGRA_11x_SOC
	.pwm_id         = 1,
#else
	.pwm_id         = 0,
#endif
	.max_brightness = 255,
	.dft_brightness = 77,
	.pwm_period_ns  = 40000,
	.pwm_gpio	= TEGRA_GPIO_INVALID,
	.notify         = dsi_j_720p_4_7_bl_notify,
	.check_fb       = dsi_j_720p_4_7_check_fb,
};

static struct platform_device dsi_j_720p_4_7_bl_device = {
	.name   = "pwm-backlight",
	.id     = -1,
	.dev    = {
		.platform_data = &dsi_j_720p_4_7_bl_data,
	},
};

static struct tegra_dsi_out dsi_j_720p_4_7_pdata;

static int dsi_j_720p_4_7_register_bl_dev(void)
{
	int err = 0;
	err = platform_device_register(&tegra_pwfm_device);
	if (err) {
		pr_err("disp1 pwm device registration failed");
		return err;
	}

	err = platform_device_register(&dsi_j_720p_4_7_bl_device);
	if (err) {
		pr_err("disp1 bl device registration failed");
		return err;
	}

	err = gpio_request(dsi_j_720p_4_7_pdata.dsi_panel_bl_pwm_gpio,
		"panel pwm");
	if (err < 0) {
		pr_err("panel backlight pwm gpio request failed\n");
		return err;
	}
	gpio_free(dsi_j_720p_4_7_pdata.dsi_panel_bl_pwm_gpio);
	return err;
}
struct tegra_dc_mode dsi_j_720p_4_7_modes[] = {
	{
		.pclk = 62625000,
		.h_ref_to_sync = 2,
		.v_ref_to_sync = 1,
		.h_sync_width = 2,
		.v_sync_width = 2,
		.h_back_porch = 84,
		.v_back_porch = 2,
		.h_active = 720,
		.v_active = 1280,
		.h_front_porch = 4,
		.v_front_porch = 4,
	},
};
static int dsi_j_720p_4_7_reg_get(void)
{
	int err = 0;

	if (dsi_j_720p_4_7_reg_requested)
		return 0;

	avdd_lcd_3v0_2v8 = regulator_get(NULL, "avdd_lcd");
	if (IS_ERR(avdd_lcd_3v0_2v8)) {
		pr_err("avdd_lcd regulator get failed\n");
		err = PTR_ERR(avdd_lcd_3v0_2v8);
		avdd_lcd_3v0_2v8 = NULL;
		goto fail;
	}

	vdd_lcd_s_1v8 = regulator_get(NULL, "vdd_lcd_1v8_s");
	if (IS_ERR(vdd_lcd_s_1v8)) {
		pr_err("vdd_lcd_1v8_s regulator get failed\n");
		err = PTR_ERR(vdd_lcd_s_1v8);
		vdd_lcd_s_1v8 = NULL;
		goto fail;
	}

	vdd_sys_bl_3v7 = regulator_get(NULL, "vdd_sys_bl");
	if (IS_ERR(vdd_sys_bl_3v7)) {
		pr_err("vdd_sys_bl regulator get failed\n");
		err = PTR_ERR(vdd_sys_bl_3v7);
		vdd_sys_bl_3v7 = NULL;
		goto fail;
	}

	dsi_j_720p_4_7_reg_requested = true;
	return 0;
fail:
	return err;
}

static int dsi_j_720p_4_7_gpio_get(void)
{
	int err = 0;

	if (dsi_j_720p_4_7_gpio_requested)
		return 0;

	err = gpio_request(dsi_j_720p_4_7_pdata.dsi_panel_rst_gpio,
		"panel rst");
	if (err < 0) {
		pr_err("panel reset gpio request failed\n");
		goto fail;
	}

	err = gpio_request(dsi_j_720p_4_7_pdata.dsi_panel_bl_en_gpio,
		"panel backlight");
	if (err < 0) {
		pr_err("panel backlight gpio request failed\n");
		goto fail;
	}



	dsi_j_720p_4_7_gpio_requested = true;
	return 0;
fail:
	return err;
}

static int dsi_j_720p_4_7_enable(struct device *dev)
{
	int err = 0;

	err = dsi_j_720p_4_7_reg_get();
	if (err < 0) {
		pr_err("dsi regulator get failed\n");
		goto fail;
	}

	err = dsi_j_720p_4_7_gpio_get();
	if (err < 0) {
		pr_err("dsi gpio request failed\n");
		goto fail;
	}
	gpio_direction_output(dsi_j_720p_4_7_pdata.dsi_panel_rst_gpio, 0);

	if (avdd_lcd_3v0_2v8) {
		err = regulator_enable(avdd_lcd_3v0_2v8);
		if (err < 0) {
			pr_err("avdd_lcd regulator enable failed\n");
			goto fail;
		}
		regulator_set_voltage(avdd_lcd_3v0_2v8, 3000000, 3000000);
	}

	usleep_range(3000, 5000);

	if (vdd_lcd_s_1v8) {
		err = regulator_enable(vdd_lcd_s_1v8);
		if (err < 0) {
			pr_err("vdd_lcd_1v8_s regulator enable failed\n");
			goto fail;
		}
	}
	usleep_range(3000, 5000);

	if (vdd_sys_bl_3v7) {
		err = regulator_enable(vdd_sys_bl_3v7);
		if (err < 0) {
			pr_err("vdd_sys_bl regulator enable failed\n");
			goto fail;
		}
	}
	usleep_range(3000, 5000);

#if DSI_PANEL_RESET
	gpio_set_value(dsi_j_720p_4_7_pdata.dsi_panel_rst_gpio, 1);
	usleep_range(1000, 5000);
	gpio_set_value(dsi_j_720p_4_7_pdata.dsi_panel_rst_gpio, 0);
	usleep_range(1000, 5000);
	gpio_set_value(dsi_j_720p_4_7_pdata.dsi_panel_rst_gpio, 1);
	msleep(20);
#endif

	gpio_direction_output(dsi_j_720p_4_7_pdata.dsi_panel_bl_en_gpio, 1);
	is_bl_powered = true;
	return 0;
fail:
	return err;
}

static struct tegra_dsi_cmd dsi_j_720p_4_7_init_cmd[] = {
	DSI_CMD_SHORT(DSI_DCS_WRITE_1_PARAM, 0xFF, 0xEE),
	DSI_CMD_SHORT(DSI_DCS_WRITE_1_PARAM, 0x26, 0x08),
	DSI_DLY_MS(10),
	DSI_CMD_SHORT(DSI_DCS_WRITE_1_PARAM, 0x26, 0x00),
	DSI_CMD_SHORT(DSI_DCS_WRITE_1_PARAM, 0xFF, 0x00),
	DSI_DLY_MS(15),
	DSI_GPIO_SET(0, 1), /* use dummy gpio */
	DSI_DLY_MS(10),
	DSI_GPIO_SET(0, 0), /* use dummy gpio */
	DSI_DLY_MS(20),
	DSI_GPIO_SET(0, 1), /* use dummy gpio */
	DSI_DLY_MS(100),
	DSI_CMD_SHORT(DSI_DCS_WRITE_1_PARAM, 0xBA, 0x02),
	DSI_DLY_MS(5),
	DSI_CMD_SHORT(DSI_DCS_WRITE_1_PARAM, 0xC2, 0x08),
	DSI_DLY_MS(5),
	DSI_CMD_SHORT(DSI_DCS_WRITE_1_PARAM, 0xFF, 0x04),
	DSI_DLY_MS(5),
	DSI_CMD_SHORT(DSI_DCS_WRITE_1_PARAM, 0x09, 0x00),
	DSI_DLY_MS(5),
	DSI_CMD_SHORT(DSI_DCS_WRITE_1_PARAM, 0x0A, 0x00),
	DSI_DLY_MS(5),
	DSI_CMD_SHORT(DSI_DCS_WRITE_1_PARAM, 0xFB, 0x01),
	DSI_DLY_MS(5),
	DSI_CMD_SHORT(DSI_DCS_WRITE_1_PARAM, 0xFF, 0xEE),
	DSI_DLY_MS(5),
	DSI_CMD_SHORT(DSI_DCS_WRITE_1_PARAM, 0x12, 0x53),
	DSI_DLY_MS(5),
	DSI_CMD_SHORT(DSI_DCS_WRITE_1_PARAM, 0x13, 0x05),
	DSI_DLY_MS(5),
	DSI_CMD_SHORT(DSI_DCS_WRITE_1_PARAM, 0x6A, 0x60),
	DSI_DLY_MS(5),
	DSI_CMD_SHORT(DSI_DCS_WRITE_1_PARAM, 0xFB, 0x01),
	DSI_DLY_MS(5),
	DSI_CMD_SHORT(DSI_DCS_WRITE_1_PARAM, 0xFF, 0x00),
	DSI_DLY_MS(5),
	DSI_CMD_SHORT(DSI_DCS_WRITE_1_PARAM, 0x3A, 0x77),
	DSI_DLY_MS(5),
	DSI_CMD_SHORT(DSI_DCS_WRITE_0_PARAM, DSI_DCS_EXIT_SLEEP_MODE, 0x00),
	DSI_DLY_MS(100),
#if (DC_CTRL_MODE & TEGRA_DC_OUT_ONE_SHOT_MODE)
	DSI_CMD_SHORT(DSI_DCS_WRITE_1_PARAM, DSI_DCS_SET_TEARING_EFFECT_ON, 0),
#endif
	DSI_CMD_SHORT(DSI_DCS_WRITE_0_PARAM, DSI_DCS_SET_DISPLAY_ON, 0x00),
	DSI_DLY_MS(150),
};

static struct tegra_dsi_out dsi_j_720p_4_7_pdata = {
	.n_data_lanes = 3,

	.rated_refresh_rate = 60,
	.refresh_rate = 60,
	.suspend_aggr = DSI_HOST_SUSPEND_LV2,
	.video_data_type = TEGRA_DSI_VIDEO_TYPE_COMMAND_MODE,
	.video_clock_mode = TEGRA_DSI_VIDEO_CLOCK_TX_ONLY,
	.controller_vs = DSI_VS_1,
	.pixel_format = TEGRA_DSI_PIXEL_FORMAT_24BIT_P,
	.virtual_channel = TEGRA_DSI_VIRTUAL_CHANNEL_0,

	.panel_reset = DSI_PANEL_RESET,
	.power_saving_suspend = true,
	.dsi_init_cmd = dsi_j_720p_4_7_init_cmd,
	.n_init_cmd = ARRAY_SIZE(dsi_j_720p_4_7_init_cmd),
};

static int dsi_j_720p_4_7_disable(void)
{
	gpio_set_value(dsi_j_720p_4_7_pdata.dsi_panel_bl_en_gpio, 0);
	is_bl_powered = false;
	if (vdd_sys_bl_3v7)
		regulator_disable(vdd_sys_bl_3v7);

	if (vdd_lcd_s_1v8)
		regulator_disable(vdd_lcd_s_1v8);

	if (avdd_lcd_3v0_2v8)
		regulator_disable(avdd_lcd_3v0_2v8);

	return 0;
}

static void dsi_j_720p_4_7_set_disp_device(
	struct platform_device *pluto_display_device)
{
	disp_device = pluto_display_device;
}

static void dsi_j_720p_4_7_dc_out_init(struct tegra_dc_out *dc)
{
	dc->dsi = &dsi_j_720p_4_7_pdata;
	dc->parent_clk = "pll_d_out0";
	dc->modes = dsi_j_720p_4_7_modes;
	dc->n_modes = ARRAY_SIZE(dsi_j_720p_4_7_modes);
	dc->enable = dsi_j_720p_4_7_enable;
	dc->disable = dsi_j_720p_4_7_disable;
	dc->width = 58;
	dc->height = 103;
	dc->flags = DC_CTRL_MODE;
}
static void dsi_j_720p_4_7_fb_data_init(struct tegra_fb_data *fb)
{
	fb->xres = dsi_j_720p_4_7_modes[0].h_active;
	fb->yres = dsi_j_720p_4_7_modes[0].v_active;
}

static void dsi_j_720p_4_7_sd_settings_init
(struct tegra_dc_sd_settings *settings)
{
	*settings = dsi_j_720p_4_7_sd_settings;
	settings->bl_device_name = "pwm-backlight";
}


struct tegra_panel __initdata dsi_j_720p_4_7 = {
	.init_sd_settings = dsi_j_720p_4_7_sd_settings_init,
	.init_dc_out = dsi_j_720p_4_7_dc_out_init,
	.init_fb_data = dsi_j_720p_4_7_fb_data_init,
	.set_disp_device = dsi_j_720p_4_7_set_disp_device,
	.register_bl_dev = dsi_j_720p_4_7_register_bl_dev,
};
EXPORT_SYMBOL(dsi_j_720p_4_7);
