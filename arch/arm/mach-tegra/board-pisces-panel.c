/*
 * arch/arm/mach-tegra/board-pisces-panel.c
 *
 * Copyright (C) 2012-2013 NVIDIA Corporation. All rights reserved.
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <linux/ioport.h>
#include <linux/fb.h>
#include <linux/nvmap.h>
#include <linux/nvhost.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/tegra_pwm_bl.h>
#include <linux/regulator/consumer.h>
#include <linux/pwm_backlight.h>
#include <linux/mfd/lm3533.h>
#include <linux/i2c/pca953x.h>
#include <mach/irqs.h>
#include <mach/iomap.h>
#include <mach/dc.h>
#if defined(CONFIG_TEGRA_HDMI_MHL_9244)
#include <linux/mhl_api.h>
#endif
#include <linux/hwinfo.h>

#include "board.h"
#include "devices.h"
#include "gpio-names.h"
#include "board-panel.h"
#include "board-pisces.h"
#include "common.h"

#include "tegra11_host1x_devices.h"

#define DSI_PANEL_BL_EN_GPIO	TEGRA_GPIO_PH2

#define LCD_ID_DET0 TEGRA_GPIO_PI7
static int lcd_id_det0;

static int __maybe_unused get_panel_id(void)
{
	int err = 0;

	err = gpio_request(LCD_ID_DET0, "lcd id det0");
	if (err < 0) {
		pr_err("lcd id det0 gpio request failed\n");
		goto fail;
	}

	err = gpio_direction_input(LCD_ID_DET0);
	if (err < 0) {
		pr_err("lcd id det0 gpio direction input failed\n");
		goto fail;
	}

	lcd_id_det0 = gpio_get_value(LCD_ID_DET0);
	pr_info("panel: id is %d (0-sharp 1-lgd)\n", lcd_id_det0);

	gpio_free(LCD_ID_DET0);
	return 0;

fail:
	return err;
}

static struct lm3533_bl_platform_data pisces_lm3533_bl_data[] = {
	{
		.name = "lm3533-backlight0",
		.max_current = 19400,
		.default_brightness = 30,
		.pwm = 0x3f,
		.linear = 1,
		.edp_states = { 425, 383, 340, 299, 255, 213, 170, 128, 85, 43, 0 },
		.edp_brightness = {255, 230, 204, 179, 153, 128, 102, 77, 51, 26, 0},
	},
	{
		.name = "lm3533-backlight1",
		.max_current = 19400,
		.default_brightness = 30,
		.pwm = 0x3f,
		.linear = 1,
		.edp_states = { 425, 383, 340, 299, 255, 213, 170, 128, 85, 43, 0 },
		.edp_brightness = {255, 230, 204, 179, 153, 128, 102, 77, 51, 26, 0},
	}
};

static struct lm3533_led_platform_data pisces_lm3533_led_data[] = {
	{
		.name = "red",
		.max_current = 5000,
		.pwm = 0x38,
		.default_trigger = "max170xx_battery-charging",
		.delay_on = 650,
		.delay_off = 200,
	},
	{
		.name = "blue",
		.max_current = 5000,
		.pwm = 0x38,
		.delay_on = 650,
		.delay_off = 200,
	},
	{
		.name = "green",
		.max_current = 5000,
		.pwm = 0x38,
		.default_trigger = "max170xx_battery-full",
		.delay_on = 650,
		.delay_off = 200,
	},
	{
		.name = "button-backlight",
		.max_current = 5000,
		.pwm = 0x38
	},
};

static struct lm3533_platform_data pisces_lm3533 = {
	.gpio_hwen = DSI_PANEL_BL_EN_GPIO,
	.boost_ovp = LM3533_BOOST_OVP_40V,
	.boost_freq = LM3533_BOOST_FREQ_500KHZ,
	.backlights = pisces_lm3533_bl_data,
	.num_backlights = ARRAY_SIZE(pisces_lm3533_bl_data),
	.leds = pisces_lm3533_led_data,
	.num_leds = ARRAY_SIZE(pisces_lm3533_led_data),
};

static struct i2c_board_info __maybe_unused pluto_i2c_led_info = {
	.type		= "lm3533",
	.addr		= 0x38,
	.platform_data	= &pisces_lm3533,
};

#if defined(CONFIG_TEGRA_HDMI_MHL_9244)
#define MI3_GPIO_MHL_RESET		TEGRA_GPIO_PH7
#define MI3_GPIO_MHL_INT		TEGRA_GPIO_PC7
#define MI3_GPIO_MHL_WAKEUP		TEGRA_GPIO_PH6
#define MI3_GPIO_MHL_1V8		TEGRA_GPIO_PK1
#define MI3_GPIO_MHL_3V3		TEGRA_GPIO_PW5
#define MI3_GPIO_HDMI_1V8_3V3		TEGRA_GPIO_PS1
#define MI3_GPIO_MHL_SEL0		TEGRA_GPIO_PR3

static int sii9244_power_setup(int on)
{
	int rc;
	static bool mhl_power_on;
	int mhl_1v8_gpio = MI3_GPIO_MHL_1V8;
	int mhl_3v3_gpio = MI3_GPIO_MHL_3V3;
	int hdmi_1v8_3v3_gpio = MI3_GPIO_HDMI_1V8_3V3;

	if (!mhl_power_on) {
		rc = gpio_request(mhl_1v8_gpio, "mhl_1v8_gpio");
		if (rc) {
			pr_err("request pm8921 gpio 14 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		rc = gpio_request(mhl_3v3_gpio, "mhl_3v3_gpio");
		if (rc) {
			pr_err("request pm8921 gpio 19 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		rc = gpio_request(hdmi_1v8_3v3_gpio, "hdmi_1v8_3v3_gpio");
		if (rc) {
			pr_err("request pm8921 gpio 21 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		mhl_power_on = true;
	}

	if (on) {
		gpio_direction_output(mhl_1v8_gpio, 1);
		gpio_direction_output(mhl_3v3_gpio, 1);
		gpio_direction_output(hdmi_1v8_3v3_gpio, 1);
	} else {
		gpio_direction_output(mhl_1v8_gpio, 0);
		gpio_direction_output(mhl_3v3_gpio, 0);
		gpio_direction_output(hdmi_1v8_3v3_gpio, 0);
	}

	return 0;
}

static void sii9244_reset(int on)
{
	int rc;
	static bool mhl_first_reset;
	int mhl_gpio_reset = MI3_GPIO_MHL_RESET;

	if (!mhl_first_reset) {
		rc = gpio_request(mhl_gpio_reset, "mhl_rst");
		if (rc) {
			pr_err("request pm8921 gpio 22 failed, rc=%d\n", rc);
			return;
		}
		mhl_first_reset = true;
	}

	if (on) {
		gpio_direction_output(mhl_gpio_reset, 0);
		msleep(10);
		gpio_direction_output(mhl_gpio_reset, 1);
	} else
		gpio_direction_output(mhl_gpio_reset, 0);

}

static int sii9244_set_vbuspower(struct device *dev, int on)
{
	static struct regulator *vbus_reg = NULL;
	int err = 0;

	printk("set vbuspower on %d\n", on);
	if (on) {
		if (!vbus_reg) {
			vbus_reg = regulator_get(dev, "usb_vbus");
			if (IS_ERR_OR_NULL(vbus_reg)) {
				pr_err("Couldn't get regulator usb_vbus: %ld\n",
						PTR_ERR(vbus_reg));
				err = PTR_ERR(vbus_reg);
				vbus_reg = NULL;
				goto fail_vbus_reg;
			}
		}
		regulator_enable(vbus_reg);
	} else {
		if (vbus_reg) {
			regulator_disable(vbus_reg);
			regulator_put(vbus_reg);
			vbus_reg = NULL;
		}
	}
	return 0;
fail_vbus_reg:
	return -EPERM;
}

#if defined(CONFIG_FB_MSM_HDMI_MHL_RCP)
static int sii9244_key_codes[] = {
	KEY_1, KEY_2, KEY_3, KEY_4, KEY_5,
	KEY_6, KEY_7, KEY_8, KEY_9, KEY_0,
	KEY_SELECT, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
	KEY_MENU, KEY_EXIT, KEY_DOT, KEY_ENTER,
	KEY_CLEAR, KEY_SOUND,
	KEY_PLAY, KEY_PAUSE, KEY_STOP, KEY_FASTFORWARD, KEY_REWIND,
	KEY_EJECTCD, KEY_FORWARD, KEY_BACK,
	KEY_PLAYCD, KEY_PAUSECD, KEY_STOP,
};
#endif

static struct mhl_platform_data mhl_sii9244_pdata = {
	.mhl_gpio_reset =       MI3_GPIO_MHL_RESET,
	.mhl_gpio_wakeup =      MI3_GPIO_MHL_WAKEUP,
	.power_setup =          sii9244_power_setup,
	.reset =                sii9244_reset,
	.set_vbuspower = 	sii9244_set_vbuspower,
#if defined(CONFIG_FB_MSM_HDMI_MHL_RCP)
	.mhl_key_codes =        sii9244_key_codes,
	.mhl_key_num =          ARRAY_SIZE(sii9244_key_codes),
#endif
};

static struct i2c_board_info __maybe_unused pluto_i2c_mhl_info[] = {
	{
	I2C_BOARD_INFO("mhl_Sii9244_page0", 0x39),
	.platform_data = &mhl_sii9244_pdata,
	},
	{
	I2C_BOARD_INFO("mhl_Sii9244_page1", 0x3D),
	},
	{
	I2C_BOARD_INFO("mhl_Sii9244_page2", 0x49),
	},
	{
	I2C_BOARD_INFO("mhl_Sii9244_cbus", 0x64),
	},
};
#endif

struct platform_device * __init pluto_host1x_init(void)
{
	struct platform_device *pdev = NULL;
#ifdef CONFIG_TEGRA_GRHOST
	pdev = tegra11_register_host1x_devices();
	if (!pdev) {
		pr_err("host1x devices registration failed\n");
		return NULL;
	}
#endif
	return pdev;
}

#ifdef CONFIG_TEGRA_DC

/* hdmi pins for hotplug */
#define pluto_hdmi_hpd		TEGRA_GPIO_PN7

/* hdmi related regulators */
static struct regulator *pluto_hdmi_vddio;
static struct regulator *pluto_hdmi_reg;
static struct regulator *pluto_hdmi_pll;

static struct resource pluto_disp1_resources[] = {
	{
		.name	= "irq",
		.start	= INT_DISPLAY_GENERAL,
		.end	= INT_DISPLAY_GENERAL,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "regs",
		.start	= TEGRA_DISPLAY_BASE,
		.end	= TEGRA_DISPLAY_BASE + TEGRA_DISPLAY_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "fbmem",
		.start	= 0, /* Filled in by pluto_panel_init() */
		.end	= 0, /* Filled in by pluto_panel_init() */
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "dsi_regs",
		.start	= 0, /* Filled in the panel file by init_resources() */
		.end	= 0, /* Filled in the panel file by init_resources() */
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "mipi_cal",
		.start	= TEGRA_MIPI_CAL_BASE,
		.end	= TEGRA_MIPI_CAL_BASE + TEGRA_MIPI_CAL_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource pluto_disp2_resources[] = {
	{
		.name	= "irq",
		.start	= INT_DISPLAY_B_GENERAL,
		.end	= INT_DISPLAY_B_GENERAL,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "regs",
		.start	= TEGRA_DISPLAY2_BASE,
		.end	= TEGRA_DISPLAY2_BASE + TEGRA_DISPLAY2_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "fbmem",
		.start	= 0, /* Filled in by pluto_panel_init() */
		.end	= 0, /* Filled in by pluto_panel_init() */
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "hdmi_regs",
		.start	= TEGRA_HDMI_BASE,
		.end	= TEGRA_HDMI_BASE + TEGRA_HDMI_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct tegra_dc_sd_settings sd_settings;

static struct tegra_dc_out pluto_disp1_out = {
	.type		= TEGRA_DC_OUT_DSI,
	.sd_settings	= NULL,
};

static int pluto_hdmi_enable(struct device *dev)
{
	int ret;
	if (!pluto_hdmi_reg) {
		pluto_hdmi_reg = regulator_get(dev, "avdd_hdmi");
		if (IS_ERR_OR_NULL(pluto_hdmi_reg)) {
			pr_err("hdmi: couldn't get regulator avdd_hdmi\n");
			pluto_hdmi_reg = NULL;
			return PTR_ERR(pluto_hdmi_reg);
		}
	}
	ret = regulator_enable(pluto_hdmi_reg);
	if (ret < 0) {
		pr_err("hdmi: couldn't enable regulator avdd_hdmi\n");
		return ret;
	}
	if (!pluto_hdmi_pll) {
		pluto_hdmi_pll = regulator_get(dev, "avdd_hdmi_pll");
		if (IS_ERR_OR_NULL(pluto_hdmi_pll)) {
			pr_err("hdmi: couldn't get regulator avdd_hdmi_pll\n");
			pluto_hdmi_pll = NULL;
			regulator_put(pluto_hdmi_reg);
			pluto_hdmi_reg = NULL;
			return PTR_ERR(pluto_hdmi_pll);
		}
	}
	ret = regulator_enable(pluto_hdmi_pll);
	if (ret < 0) {
		pr_err("hdmi: couldn't enable regulator avdd_hdmi_pll\n");
		return ret;
	}
	return 0;
}

static int pluto_hdmi_disable(void)
{
	if (pluto_hdmi_reg) {
		regulator_disable(pluto_hdmi_reg);
		regulator_put(pluto_hdmi_reg);
		pluto_hdmi_reg = NULL;
	}

	if (pluto_hdmi_pll) {
		regulator_disable(pluto_hdmi_pll);
		regulator_put(pluto_hdmi_pll);
		pluto_hdmi_pll = NULL;
	}

	return 0;
}

static int pluto_hdmi_postsuspend(void)
{
	if (pluto_hdmi_vddio) {
		regulator_disable(pluto_hdmi_vddio);
		regulator_put(pluto_hdmi_vddio);
		pluto_hdmi_vddio = NULL;
	}
	return 0;
}

static int pluto_hdmi_hotplug_init(struct device *dev)
{
	int ret = 0;
	if (!pluto_hdmi_vddio) {
		pluto_hdmi_vddio = regulator_get(dev, "vdd_hdmi_5v0");
		if (IS_ERR_OR_NULL(pluto_hdmi_vddio)) {
			ret = PTR_ERR(pluto_hdmi_vddio);
			pr_err("hdmi: couldn't get regulator vdd_hdmi_5v0\n");
			pluto_hdmi_vddio = NULL;
			return ret;
		}
	}
	ret = regulator_enable(pluto_hdmi_vddio);
	if (ret < 0) {
		pr_err("hdmi: couldn't enable regulator vdd_hdmi_5v0\n");
		regulator_put(pluto_hdmi_vddio);
		pluto_hdmi_vddio = NULL;
		return ret;
	}
	return ret;
}

static struct tegra_dc_out pluto_disp2_out = {
	.type		= TEGRA_DC_OUT_HDMI,
	.flags		= TEGRA_DC_OUT_HOTPLUG_HIGH,
	.parent_clk	= "pll_d2_out0",

	.dcc_bus	= 3,
	.hotplug_gpio	= pluto_hdmi_hpd,

	.max_pixclock	= KHZ2PICOS(297000),

	.enable		= pluto_hdmi_enable,
	.disable	= pluto_hdmi_disable,
	.postsuspend	= pluto_hdmi_postsuspend,
	.hotplug_init	= pluto_hdmi_hotplug_init,
};

static struct tegra_fb_data pluto_disp1_fb_data = {
	.win = 0,
	.bits_per_pixel = 32,
	.flags = TEGRA_FB_FLIP_ON_PROBE,
};

static struct tegra_dc_platform_data pluto_disp1_pdata = {
	.flags		= TEGRA_DC_FLAG_ENABLED,
	.default_out	= &pluto_disp1_out,
	.fb		= &pluto_disp1_fb_data,
	.emc_clk_rate	= 204000000,
#ifdef CONFIG_TEGRA_DC_CMU
	.cmu_enable = 1,
#endif
};

static struct tegra_fb_data pluto_disp2_fb_data = {
	.win		= 0,
	.xres		= 1024,
	.yres		= 600,
	.bits_per_pixel = 32,
	.flags		= TEGRA_FB_FLIP_ON_PROBE,
};

static struct tegra_dc_platform_data pluto_disp2_pdata = {
	.flags		= TEGRA_DC_FLAG_ENABLED,
	.default_out	= &pluto_disp2_out,
	.fb		= &pluto_disp2_fb_data,
	.emc_clk_rate	= 300000000,
};

static struct platform_device pluto_disp2_device = {
	.name		= "tegradc",
	.id		= 1,
	.resource	= pluto_disp2_resources,
	.num_resources	= ARRAY_SIZE(pluto_disp2_resources),
	.dev = {
		.platform_data = &pluto_disp2_pdata,
	},
};

static struct platform_device pluto_disp1_device = {
	.name		= "tegradc",
	.id		= 0,
	.resource	= pluto_disp1_resources,
	.num_resources	= ARRAY_SIZE(pluto_disp1_resources),
	.dev = {
		.platform_data = &pluto_disp1_pdata,
	},
};

static struct nvmap_platform_carveout pluto_carveouts[] = {
	[0] = {
		.name		= "iram",
		.usage_mask	= NVMAP_HEAP_CARVEOUT_IRAM,
		.base		= TEGRA_IRAM_BASE + TEGRA_RESET_HANDLER_SIZE,
		.size		= TEGRA_IRAM_SIZE - TEGRA_RESET_HANDLER_SIZE,
		.buddy_size	= 0, /* no buddy allocation for IRAM */
	},
	[1] = {
		.name		= "generic-0",
		.usage_mask	= NVMAP_HEAP_CARVEOUT_GENERIC,
		.base		= 0, /* Filled in by pluto_panel_init() */
		.size		= 0, /* Filled in by pluto_panel_init() */
		.buddy_size	= SZ_32K,
	},
	[2] = {
		.name		= "vpr",
		.usage_mask	= NVMAP_HEAP_CARVEOUT_VPR,
		.base		= 0, /* Filled in by pluto_panel_init() */
		.size		= 0, /* Filled in by pluto_panel_init() */
		.buddy_size	= SZ_32K,
	},
};

static struct nvmap_platform_data pluto_nvmap_data = {
	.carveouts	= pluto_carveouts,
	.nr_carveouts	= ARRAY_SIZE(pluto_carveouts),
};

static struct platform_device pluto_nvmap_device = {
	.name	= "tegra-nvmap",
	.id	= -1,
	.dev	= {
		.platform_data = &pluto_nvmap_data,
	},
};

static struct tegra_dc_sd_settings pluto_sd_settings = {
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

static struct tegra_panel *panel = NULL;
static void __init pluto_panel_select(void)
{
	struct board_info board;

	tegra_get_display_board_info(&board);

	switch (board.board_id) {
	case BOARD_E1605:
		panel = &dsi_j_720p_4_7;
		break;
	case BOARD_E1577:
		panel = &dsi_s_1080p_5;
		break;
	case BOARD_E1582:
	default:
		if (tegra_get_board_panel_id())
			panel = &dsi_s_1080p_5;
		else
			panel = &dsi_l_720p_5;
		break;
	}

	get_panel_id();
	if (1 == lcd_id_det0)
		panel = &dsi_l_1080p_5_mi3;
	else
		panel = &dsi_s_1080p_5_mi3;

	update_hardware_info(TYPE_PANEL, lcd_id_det0);

	if (panel->init_sd_settings)
		panel->init_sd_settings(&sd_settings);

	if (panel->init_dc_out)
		panel->init_dc_out(&pluto_disp1_out);

	if (panel->init_fb_data)
		panel->init_fb_data(&pluto_disp1_fb_data);
#ifdef CONFIG_TEGRA_DC_CMU
	if (panel->init_cmu_data)
		panel->init_cmu_data(&pluto_disp1_pdata);
#endif
	if (panel->set_disp_device)
		panel->set_disp_device(&pluto_disp1_device);

	if (panel->init_resources)
		panel->init_resources(pluto_disp1_resources,
			ARRAY_SIZE(pluto_disp1_resources));

	if (panel->register_bl_dev)
		panel->register_bl_dev();

	i2c_register_board_info(0, &pluto_i2c_led_info, 1);

#if defined(CONFIG_TEGRA_HDMI_MHL_9244)
	pluto_i2c_mhl_info[0].irq = gpio_to_irq(MI3_GPIO_MHL_INT);
	i2c_register_board_info(0, pluto_i2c_mhl_info, ARRAY_SIZE(pluto_i2c_mhl_info));
#endif
}

void panel_set_param(int param)
{
	if (panel == NULL) {
		pr_err("panel: error in %s(): panel point is NULL ! \n", __func__);
	} else {
		if (panel->set_dispparam)
			panel->set_dispparam(param);
		else
			pr_err("panel: error in %s(): panel->set_dispparam() point is NULL ! \n", __func__);
	}
	return;
}
EXPORT_SYMBOL(panel_set_param);

void panel_config_gamma(void)
{
	if (panel != NULL) {
		if (panel->panel_gamma_select)
			panel->panel_gamma_select();
	} else
		pr_err("panel: +++ %s: panel point is NULL ! \n", __func__);

	return;
}
EXPORT_SYMBOL(panel_config_gamma);

int __init pluto_panel_init(void)
{
	int err = 0;
	struct resource __maybe_unused *res;
	struct platform_device *phost1x;

	sd_settings = pluto_sd_settings;

	pluto_panel_select();

#ifdef CONFIG_TEGRA_NVMAP
	pluto_carveouts[1].base = tegra_carveout_start;
	pluto_carveouts[1].size = tegra_carveout_size;
	pluto_carveouts[2].base = tegra_vpr_start;
	pluto_carveouts[2].size = tegra_vpr_size;

	err = platform_device_register(&pluto_nvmap_device);
	if (err) {
		pr_err("nvmap device registration failed\n");
		return err;
	}
#endif
	gpio_request(pluto_hdmi_hpd, "hdmi_hpd");
	gpio_direction_input(pluto_hdmi_hpd);

	phost1x = pluto_host1x_init();
	if (!phost1x) {
		pr_err("host1x devices registration failed\n");
		return -EINVAL;
	}

	res = platform_get_resource_byname(&pluto_disp1_device,
		IORESOURCE_MEM, "fbmem");
	res->start = tegra_fb_start;
	res->end = tegra_fb_start + tegra_fb_size - 1;

	/* Copy the bootloader fb to the fb. */
	__tegra_move_framebuffer(&pluto_nvmap_device,
		tegra_fb_start, tegra_bootloader_fb_start,
			min(tegra_fb_size, tegra_bootloader_fb_size));

	pluto_disp1_device.dev.parent = &phost1x->dev;
	err = platform_device_register(&pluto_disp1_device);
	if (err) {
		pr_err("disp1 device registration failed\n");
		return err;
	}

	err = tegra_init_hdmi(&pluto_disp2_device, phost1x);
	if (err) {
		pr_err("hdmi device init failed (%d)\n", err);
		return err;
	}

#ifdef CONFIG_TEGRA_NVAVP
	nvavp_device.dev.parent = &phost1x->dev;
	err = platform_device_register(&nvavp_device);
	if (err) {
		pr_err("nvavp device registration failed\n");
		return err;
	}
#endif
	return err;
}
#else
int __init pluto_panel_init(void)
{
	if (pluto_host1x_init())
		return 0;
	else
		return -EINVAL;
}
#endif
