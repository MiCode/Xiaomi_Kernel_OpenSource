/*
 * arch/arm/mach-tegra/panel-lgd-wxga-7-0.c
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/tegra_pwm_bl.h>
#include <linux/regulator/consumer.h>
#include <linux/pwm_backlight.h>
#include <linux/max8831_backlight.h>
#include <linux/leds.h>
#include <linux/ioport.h>

#include <mach/dc.h>

#include "board.h"
#include "board-panel.h"
#include "devices.h"
#include "gpio-names.h"
#include "tegra11_host1x_devices.h"


#define DSI_PANEL_RESET		0
#define DC_CTRL_MODE	TEGRA_DC_OUT_CONTINUOUS_MODE

static bool reg_requested;
static bool gpio_requested;
static struct platform_device *disp_device;
static struct regulator *avdd_lcd_3v3;
static struct regulator *dvdd_lcd;
static struct regulator *vdd_lcd_bl_en;

static struct tegra_dc_sd_settings dsi_lgd_wxga_7_0_sd_settings = {
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

static struct tegra_dsi_cmd dsi_lgd_wxga_7_0_init_cmd[] = {
	DSI_CMD_SHORT(0x15, 0x01, 0x0),
	DSI_DLY_MS(20),
	DSI_CMD_SHORT(0x15, 0xAE, 0x0B),
	DSI_CMD_SHORT(0x15, 0xEE, 0xEA),
	DSI_CMD_SHORT(0x15, 0xEF, 0x5F),
	DSI_CMD_SHORT(0x15, 0xF2, 0x68),
	DSI_CMD_SHORT(0x15, 0xEE, 0x0),
	DSI_CMD_SHORT(0x15, 0xEF, 0x0),
};

static struct tegra_dsi_cmd dsi_lgd_wxga_7_0_late_resume_cmd[] = {
	DSI_CMD_SHORT(0x15, 0x10, 0x0),
	DSI_DLY_MS(120),
};

static struct tegra_dsi_cmd dsi_lgd_wxga_7_0_early_suspend_cmd[] = {
	DSI_CMD_SHORT(0x15, 0x11, 0x0),
	DSI_DLY_MS(160),
};

static struct tegra_dsi_cmd dsi_lgd_wxga_7_0_suspend_cmd[] = {
	DSI_CMD_SHORT(0x15, 0x11, 0x0),
	DSI_DLY_MS(160),
};

static struct tegra_dsi_out dsi_lgd_wxga_7_0_pdata = {
#ifdef CONFIG_ARCH_TEGRA_3x_SOC
	.n_data_lanes = 2,
	.controller_vs = DSI_VS_0,
#else
	.controller_vs = DSI_VS_1,
#endif

	.n_data_lanes = 4,
	.video_burst_mode = TEGRA_DSI_VIDEO_NONE_BURST_MODE,

	.pixel_format = TEGRA_DSI_PIXEL_FORMAT_24BIT_P,
	.refresh_rate = 60,
	.virtual_channel = TEGRA_DSI_VIRTUAL_CHANNEL_0,

	.dsi_instance = DSI_INSTANCE_0,

	.panel_reset = DSI_PANEL_RESET,
	.power_saving_suspend = true,
	.video_data_type = TEGRA_DSI_VIDEO_TYPE_VIDEO_MODE,

	.video_clock_mode = TEGRA_DSI_VIDEO_CLOCK_TX_ONLY,

	.dsi_init_cmd = dsi_lgd_wxga_7_0_init_cmd,
	.n_init_cmd = ARRAY_SIZE(dsi_lgd_wxga_7_0_init_cmd),

	.dsi_early_suspend_cmd = dsi_lgd_wxga_7_0_early_suspend_cmd,
	.n_early_suspend_cmd = ARRAY_SIZE(dsi_lgd_wxga_7_0_early_suspend_cmd),

	.dsi_late_resume_cmd = dsi_lgd_wxga_7_0_late_resume_cmd,
	.n_late_resume_cmd = ARRAY_SIZE(dsi_lgd_wxga_7_0_late_resume_cmd),

	.dsi_suspend_cmd = dsi_lgd_wxga_7_0_suspend_cmd,
	.n_suspend_cmd = ARRAY_SIZE(dsi_lgd_wxga_7_0_suspend_cmd),

	.phy_timing = {
		.t_clkprepare_ns = 27,
		.t_clkzero_ns = 330,
		.t_hsprepare_ns = 30,
		.t_datzero_ns = 270,
	},
};

static int tegratab_dsi_regulator_get(struct device *dev)
{
	int err = 0;

	if (reg_requested)
		return 0;
	avdd_lcd_3v3 = regulator_get(dev, "avdd_lcd");
	if (IS_ERR(avdd_lcd_3v3)) {
		pr_err("avdd_lcd regulator get failed\n");
		err = PTR_ERR(avdd_lcd_3v3);
		avdd_lcd_3v3 = NULL;
		goto fail;
	}

	vdd_lcd_bl_en = regulator_get(dev, "vdd_lcd_bl_en");
	if (IS_ERR(vdd_lcd_bl_en)) {
		pr_err("vdd_lcd_bl_en regulator get failed\n");
		err = PTR_ERR(vdd_lcd_bl_en);
		vdd_lcd_bl_en = NULL;
		goto fail;
	}

	dvdd_lcd = regulator_get(dev, "dvdd_lcd");
	if (IS_ERR(dvdd_lcd)) {
		pr_err("dvdd_lcd regulator get failed\n");
		err = PTR_ERR(dvdd_lcd);
		dvdd_lcd = NULL;
		goto fail;
	}
	reg_requested = true;
	return 0;
fail:
	return err;
}

static int tegratab_dsi_gpio_get(void)
{
	int err = 0;

	if (gpio_requested)
		return 0;

	/* free pwm GPIO */
	err = gpio_request(dsi_lgd_wxga_7_0_pdata.dsi_panel_bl_pwm_gpio,
		"panel pwm");
	if (err < 0) {
		pr_err("panel pwm gpio request failed\n");
		goto fail;
	}
	gpio_free(dsi_lgd_wxga_7_0_pdata.dsi_panel_bl_pwm_gpio);
	gpio_requested = true;
	return 0;
fail:
	return err;
}

static int dsi_lgd_wxga_7_0_enable(struct device *dev)
{
	int err = 0;

	err = tegratab_dsi_regulator_get(dev);

	if (err < 0) {
		pr_err("dsi regulator get failed\n");
		goto fail;
	}

	err = tegra_panel_gpio_get_dt("lg,wxga-7", &panel_of);
	if (err < 0) {
		/* try to request gpios from board file */
		err = tegratab_dsi_gpio_get();
		if (err < 0) {
			pr_err("dsi gpio request failed\n");
			goto fail;
		}
	}

	/*
	 * Turning on 1.8V then AVDD after 5ms is required per spec.
	 */
	msleep(20);

	if (avdd_lcd_3v3) {
		err = regulator_enable(avdd_lcd_3v3);
		if (err < 0) {
			pr_err("avdd_lcd regulator enable failed\n");
			goto fail;
		}
		regulator_set_voltage(avdd_lcd_3v3, 3300000, 3300000);
	}

	msleep(150);
	if (dvdd_lcd) {
		err = regulator_enable(dvdd_lcd);
		if (err < 0) {
			pr_err("dvdd_lcd regulator enable failed\n");
			goto fail;
		}
	}

	msleep(100);
	if (vdd_lcd_bl_en) {
		err = regulator_enable(vdd_lcd_bl_en);
		if (err < 0) {
			pr_err("vdd_lcd_bl_en regulator enable failed\n");
			goto fail;
		}
	}

	msleep(100);
#if DSI_PANEL_RESET
/*
 * Nothing is requested.
 */
#endif

	return 0;
fail:
	return err;
}

static int dsi_lgd_wxga_7_0_disable(void)
{
	if (vdd_lcd_bl_en)
		regulator_disable(vdd_lcd_bl_en);

	if (dvdd_lcd)
		regulator_disable(dvdd_lcd);

	if (avdd_lcd_3v3)
		regulator_disable(avdd_lcd_3v3);

	return 0;
}

static int dsi_lgd_wxga_7_0_postsuspend(void)
{
	return 0;
}

/*
 * See display standard timings and a few constraints underneath
 * \vendor\nvidia\tegra\core\drivers\hwinc
 *
 * Class: Display Standard Timings
 *
 * Programming of display timing registers must meet these restrictions:
 * Constraint 1: H_REF_TO_SYNC + H_SYNC_WIDTH + H_BACK_PORCH > 11.
 * Constraint 2: V_REF_TO_SYNC + V_SYNC_WIDTH + V_BACK_PORCH > 1.
 * Constraint 3: V_FRONT_PORCH + V_SYNC_WIDTH +
				V_BACK_PORCH > 1 (vertical blank).
 * Constraint 4: V_SYNC_WIDTH >= 1, H_SYNC_WIDTH >= 1
 * Constraint 5: V_REF_TO_SYNC >= 1, H_REF_TO_SYNC >= 0
 * Constraint 6: V_FRONT_PORT >= (V_REF_TO_SYNC + 1),
				H_FRONT_PORT >= (H_REF_TO_SYNC + 1)
 * Constraint 7: H_DISP_ACTIVE >= 16, V_DISP_ACTIVE >= 16
 */

/*
 * how to determine pclk
 * h_total =
 * Horiz_BackPorch + Horiz_SyncWidth + Horiz_DispActive + Horiz_FrontPorch;
 *
 * v_total =
 * Vert_BackPorch + Vert_SyncWidth + Vert_DispActive + Vert_FrontPorch;
 * panel_freq = ( h_total * v_total * refresh_freq );
 */

static struct tegra_dc_mode dsi_lgd_wxga_7_0_modes[] = {
	{
		.pclk = 71000000, /* 890 *1323 *60 = 70648200 */
		.h_ref_to_sync = 10,
		.v_ref_to_sync = 1,
		.h_sync_width = 1,
		.v_sync_width = 1,
		.h_back_porch = 57,
		.v_back_porch = 14,
		.h_active = 800,
		.v_active = 1280,
		.h_front_porch = 32,
		.v_front_porch = 28,
	},
};

static int dsi_lgd_wxga_7_0_bl_notify(struct device *unused, int brightness)
{
	/*
	 * In early panel bring-up, we will
	 * not enable PRISM.
	 * Just use same brightness that is delivered from user side.
	 * TODO...
	 * use PRSIM brightness later.
	 */
	if (brightness > 255) {
		pr_info("Error: Brightness > 255!\n");
		brightness = 255;
	}
	return brightness;
}

static int dsi_lgd_wxga_7_0_check_fb(struct device *dev, struct fb_info *info)
{
	return info->device == &disp_device->dev;
}

static struct platform_pwm_backlight_data dsi_lgd_wxga_7_0_bl_data = {
	.pwm_id		= 1,
	.max_brightness	= 255,
	.dft_brightness	= 224,
	.pwm_period_ns	= 1000000,
	.notify		= dsi_lgd_wxga_7_0_bl_notify,
	/* Only toggle backlight on fb blank notifications for disp1 */
	.check_fb	= dsi_lgd_wxga_7_0_check_fb,
};

static struct platform_device __maybe_unused
		dsi_lgd_wxga_7_0_bl_device = {
	.name	= "pwm-backlight",
	.id	= -1,
	.dev	= {
		.platform_data = &dsi_lgd_wxga_7_0_bl_data,
	},
};

static struct platform_device __maybe_unused
			*dsi_lgd_wxga_7_0_bl_devices[] __initdata = {
	&tegra_pwfm_device,
	&dsi_lgd_wxga_7_0_bl_device,
};

static int  __init dsi_lgd_wxga_7_0_register_bl_dev(void)
{
	int err = 0;

	err = platform_add_devices(dsi_lgd_wxga_7_0_bl_devices,
				ARRAY_SIZE(dsi_lgd_wxga_7_0_bl_devices));
	if (err) {
		pr_err("disp1 bl device registration failed");
		return err;
	}
	return err;
}

static void dsi_lgd_wxga_7_0_set_disp_device(
	struct platform_device *display_device)
{
	disp_device = display_device;
}

static void dsi_lgd_wxga_7_0_dc_out_init(struct tegra_dc_out *dc)
{
	dc->dsi = &dsi_lgd_wxga_7_0_pdata;
	dc->parent_clk = "pll_d_out0";
	dc->modes = dsi_lgd_wxga_7_0_modes;
	dc->n_modes = ARRAY_SIZE(dsi_lgd_wxga_7_0_modes);
	dc->enable = dsi_lgd_wxga_7_0_enable;
	dc->disable = dsi_lgd_wxga_7_0_disable;
	dc->postsuspend	= dsi_lgd_wxga_7_0_postsuspend,
	dc->width = 94;
	dc->height = 150;
	dc->flags = DC_CTRL_MODE;
}

static void dsi_lgd_wxga_7_0_fb_data_init(struct tegra_fb_data *fb)
{
	fb->xres = dsi_lgd_wxga_7_0_modes[0].h_active;
	fb->yres = dsi_lgd_wxga_7_0_modes[0].v_active;
}

static void
dsi_lgd_wxga_7_0_sd_settings_init(struct tegra_dc_sd_settings *settings)
{
	*settings = dsi_lgd_wxga_7_0_sd_settings;
	settings->bl_device_name = "pwm-backlight";
}

static void dsi_lgd_wxga_7_0_cmu_init(struct tegra_dc_platform_data *pdata)
{
	pdata->cmu = NULL; /* will write CMU stuff after calibration */
}

struct tegra_panel_ops dsi_lgd_wxga_7_0_ops = {
	.enable = dsi_lgd_wxga_7_0_enable,
	.disable = dsi_lgd_wxga_7_0_disable,
	.postsuspend = dsi_lgd_wxga_7_0_postsuspend,
};

struct tegra_panel __initdata dsi_lgd_wxga_7_0 = {
	.init_sd_settings = dsi_lgd_wxga_7_0_sd_settings_init,
	.init_dc_out = dsi_lgd_wxga_7_0_dc_out_init,
	.init_fb_data = dsi_lgd_wxga_7_0_fb_data_init,
	.register_bl_dev = dsi_lgd_wxga_7_0_register_bl_dev,
	.init_cmu_data = dsi_lgd_wxga_7_0_cmu_init,
	.set_disp_device = dsi_lgd_wxga_7_0_set_disp_device,
};
EXPORT_SYMBOL(dsi_lgd_wxga_7_0);
