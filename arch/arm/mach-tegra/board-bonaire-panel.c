/*
 * arch/arm/mach-tegra/board-bonaire-panel.c
 *
 * Copyright (c) 2013, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/resource.h>
#include <asm/mach-types.h>
#include <linux/platform_device.h>
#include <linux/pwm_backlight.h>
#include <linux/nvhost.h>
#include <linux/nvmap.h>
#include <linux/tegra-soc.h>
#include <mach/gpio-tegra.h>
#include <mach/irqs.h>
#include <mach/dc.h>
#include <mach/fb.h>

#include "board.h"
#include "board-bonaire.h"
#include "devices.h"
#include "gpio-names.h"
#include "tegra12_host1x_devices.h"
#include "iomap.h"

/* Select panel to be used. */
#define DSI_PANEL_219 0
#define DSI_PANEL_218 1
#define AVDD_LCD PMU_TCA6416_GPIO_PORT17
#define DSI_PANEL_RESET 0

#define bonaire_lvds_shutdown	TEGRA_GPIO_PB2
#define bonaire_bl_enb		TEGRA_GPIO_PW1
#define bonaire_bl_pwm		TEGRA_GPIO_PH0

#if defined(DSI_PANEL_219) || defined(DSI_PANEL_218)
#define bonaire_dsia_bl_enb	TEGRA_GPIO_PW1
#define bonaire_dsib_bl_enb	TEGRA_GPIO_PW0
#define bonaire_dsi_panel_reset	TEGRA_GPIO_PD2
#endif

struct platform_device * __init bonaire_host1x_init(void)
{
	struct platform_device *pdev = NULL;

#ifdef CONFIG_TEGRA_GRHOST
	if (!of_have_populated_dt())
		pdev = tegra12_register_host1x_devices();
	else
		pdev = to_platform_device(bus_find_device_by_name(
			&platform_bus_type, NULL, "host1x"));

	if (!pdev) {
		pr_err("host1x devices registration failed\n");
		return NULL;
	}
#endif
	return pdev;
}

static struct regulator *bonaire_dsi_reg;

static int bonaire_backlight_init(struct device *dev)
{
	int ret;

#ifndef CONFIG_TEGRA_BONAIRE_DSI

	ret = gpio_request(bonaire_bl_enb, "backlight_enb");
	if (ret < 0)
		return ret;

	ret = gpio_direction_output(bonaire_bl_enb, 1);
	if (ret < 0)
		gpio_free(bonaire_bl_enb);

	return ret;
#endif

#if DSI_PANEL_219 || DSI_PANEL_218
	/* Enable back light for DSIa panel */
	printk("bonaire_dsi_backlight_init\n");
	ret = gpio_request(bonaire_dsia_bl_enb, "dsia_bl_enable");
	if (ret < 0)
		return ret;

	ret = gpio_direction_output(bonaire_dsia_bl_enb, 1);
	if (ret < 0)
		gpio_free(bonaire_dsia_bl_enb);

	/* Enable back light for DSIb panel */
	ret = gpio_request(bonaire_dsib_bl_enb, "dsib_bl_enable");
	if (ret < 0)
		return ret;

	ret = gpio_direction_output(bonaire_dsib_bl_enb, 1);
	if (ret < 0)
		gpio_free(bonaire_dsib_bl_enb);

#endif

	return ret;
};

static void bonaire_backlight_exit(struct device *dev)
{
#ifndef CONFIG_TEGRA_BONAIRE_DSI
	gpio_set_value(bonaire_bl_enb, 0);
	gpio_free(bonaire_bl_enb);

	return;
#endif
#if DSI_PANEL_219 || DSI_PANEL_218
	/* Disable back light for DSIa panel */
	gpio_set_value(bonaire_dsia_bl_enb, 0);
	gpio_free(bonaire_dsia_bl_enb);

	/* Disable back light for DSIb panel */
	gpio_set_value(bonaire_dsib_bl_enb, 0);
	gpio_free(bonaire_dsib_bl_enb);

	gpio_set_value(bonaire_lvds_shutdown, 1);
	mdelay(20);
#endif
}

static int bonaire_backlight_notify(struct device *unused, int brightness)
{
	int cur_sd_brightness = atomic_read(&sd_brightness);
	int orig_brightness = brightness;

#ifndef CONFIG_TEGRA_BONAIRE_DSI
	/* Set the backlight GPIO pin mode to 'backlight_enable' */
	gpio_request(bonaire_bl_enb, "backlight_enb");
	gpio_set_value(bonaire_bl_enb, !!brightness);
	goto final;
#endif
#if DSI_PANEL_219 || DSI_PANEL_218
	/* DSIa */
	gpio_set_value(bonaire_dsia_bl_enb, !!brightness);

	/* DSIb */
	gpio_set_value(bonaire_dsib_bl_enb, !!brightness);
#endif

final:
	/* SD brightness is a percentage, 8-bit value. */
	brightness = (brightness * cur_sd_brightness) / 255;
	if (cur_sd_brightness != 255) {
		printk("NVSD BL - in: %d, sd: %d, out: %d\n",
			orig_brightness, cur_sd_brightness, brightness);
	}

	return brightness;
}

static struct platform_pwm_backlight_data bonaire_backlight_data = {
	.pwm_id		= 2,
	.max_brightness	= 255,
	.dft_brightness	= 224,
	.pwm_period_ns	= 5000000,
	.pwm_gpio	= TEGRA_GPIO_INVALID,
	.init		= bonaire_backlight_init,
	.exit		= bonaire_backlight_exit,
	.notify		= bonaire_backlight_notify,
};

static struct platform_device bonaire_backlight_device = {
	.name	= "pwm-backlight",
	.id	= -1,
	.dev	= {
		.platform_data = &bonaire_backlight_data,
	},
};

static int bonaire_panel_enable(struct device *dev)
{
	static struct regulator *reg;

	if (reg == NULL) {
		reg = regulator_get(NULL, "avdd_lvds");
		if (WARN_ON(IS_ERR(reg)))
			pr_err("%s: couldn't get regulator avdd_lvds: %ld\n",
			       __func__, PTR_ERR(reg));
		else
			regulator_enable(reg);
	}

	gpio_set_value(bonaire_lvds_shutdown, 1);
	return 0;
}

static int bonaire_panel_disable(void)
{
	gpio_set_value(bonaire_lvds_shutdown, 0);
	return 0;
}

#if defined(CONFIG_TEGRA_DP)
static struct tegra_dc_out_pin edp_out_pins[] = {
	{
		.name	= TEGRA_DC_OUT_PIN_H_SYNC,
		.pol	= TEGRA_DC_OUT_PIN_POL_HIGH,
	},
	{
		.name	= TEGRA_DC_OUT_PIN_V_SYNC,
		.pol	= TEGRA_DC_OUT_PIN_POL_LOW,
	},
	{
		.name	= TEGRA_DC_OUT_PIN_PIXEL_CLOCK,
		.pol	= TEGRA_DC_OUT_PIN_POL_LOW,
	},
	{
		.name   = TEGRA_DC_OUT_PIN_DATA_ENABLE,
		.pol    = TEGRA_DC_OUT_PIN_POL_HIGH,
	},
};
#endif

#if defined(CONFIG_TEGRA_LVDS)
static struct tegra_dc_out_pin lvds_out_pins[] = {
	{
		.name	= TEGRA_DC_OUT_PIN_H_SYNC,
		.pol	= TEGRA_DC_OUT_PIN_POL_LOW,
	},
	{
		.name	= TEGRA_DC_OUT_PIN_V_SYNC,
		.pol	= TEGRA_DC_OUT_PIN_POL_LOW,
	},
	{
		.name	= TEGRA_DC_OUT_PIN_PIXEL_CLOCK,
		.pol	= TEGRA_DC_OUT_PIN_POL_HIGH,
	},
	{
		.name   = TEGRA_DC_OUT_PIN_DATA_ENABLE,
		.pol    = TEGRA_DC_OUT_PIN_POL_HIGH,
	},
};
#endif

static struct resource bonaire_disp1_resources[] = {
	{
		.name	= "irq",
		.start	= INT_DISPLAY_GENERAL,
		.end	= INT_DISPLAY_GENERAL,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "regs",
		.start	= TEGRA_DISPLAY_BASE,
		.end	= TEGRA_DISPLAY_BASE + TEGRA_DISPLAY_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "fbmem",
		.start	= 0,	/* Filled in by bonaire_panel_init() */
		.end	= 0,	/* Filled in by bonaire_panel_init() */
		.flags	= IORESOURCE_MEM,
	},
#ifdef CONFIG_TEGRA_DSI_INSTANCE_1
	{
		.name	= "dsi_regs",
		.start	= TEGRA_DSIB_BASE,
		.end	= TEGRA_DSIB_BASE + TEGRA_DSIB_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
#else
	{
		.name	= "dsi_regs",
		.start	= TEGRA_DSI_BASE,
		.end	= TEGRA_DSI_BASE + TEGRA_DSI_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
#endif
#if defined(CONFIG_TEGRA_DP) || defined(CONFIG_TEGRA_LVDS)
	{
		.name	= "sor",
		.start	= TEGRA_SOR_BASE,
		.end	= TEGRA_SOR_BASE + TEGRA_SOR_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
#endif
#if defined(CONFIG_TEGRA_DP)
	{
		.name	= "dpaux",
		.start	= TEGRA_DPAUX_BASE,
		.end	= TEGRA_DPAUX_BASE + TEGRA_DPAUX_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
#endif
};

static struct resource bonaire_disp2_resources[] = {
	{
		.name	= "irq",
		.start	= INT_DISPLAY_B_GENERAL,
		.end	= INT_DISPLAY_B_GENERAL,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "regs",
		.start	= TEGRA_DISPLAY2_BASE,
		.end	= TEGRA_DISPLAY2_BASE + TEGRA_DISPLAY2_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "fbmem",
		.start	= 0,	/* Filled in by bonaire_panel_init() */
		.end	= 0,	/* Filled in by bonaire_panel_init() */
		.flags	= IORESOURCE_MEM,
	},
#ifdef CONFIG_TEGRA_DSI_INSTANCE_1
	{
		.name	= "dsi_regs",
		.start	= TEGRA_DSIB_BASE,
		.end	= TEGRA_DSIB_BASE + TEGRA_DSIB_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
#else
	{
		.name	= "dsi_regs",
		.start	= TEGRA_DSI_BASE,
		.end	= TEGRA_DSI_BASE + TEGRA_DSI_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
#endif
#if defined(CONFIG_TEGRA_DP) || defined(CONFIG_TEGRA_LVDS)
	{
		.name	= "sor",
		.start	= TEGRA_SOR_BASE,
		.end	= TEGRA_SOR_BASE + TEGRA_SOR_SIZE - 1,

		.flags	= IORESOURCE_MEM,
	},
#endif
#if defined(CONFIG_TEGRA_DP)
	{
		.name	= "dpaux",
		.start	= TEGRA_DPAUX_BASE,
		.end	= TEGRA_DPAUX_BASE + TEGRA_DPAUX_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
#endif
};

static struct tegra_dc_mode bonaire_panel_modes[] = {
	{
		.pclk = 27000000,
		.h_ref_to_sync = 1,
		.v_ref_to_sync = 1,
		.h_sync_width = 34,
		.v_sync_width = 6,
		.h_back_porch = 64,
		.v_back_porch = 4,
		.h_active = 1366,
		.v_active = 768,
		.h_front_porch = 16,
		.v_front_porch = 2,
	},
};

#if defined(CONFIG_TEGRA_LVDS)
static struct tegra_dc_mode bonaire_lvds_panel_modes[] = {
	{
		.pclk	       = 27000000,
		.h_ref_to_sync = 1,
		.v_ref_to_sync = 1,
		.h_sync_width  = 32,
		.v_sync_width  = 5,
		.h_back_porch  = 20,
		.v_back_porch  = 12,
		.h_active      = 1366,
		.v_active      = 768,
		.h_front_porch = 48,
		.v_front_porch = 3,
	},
};
#endif

static struct tegra_fb_data bonaire_fb_data_linsim = {
	.win            = 0,
	.xres           = 120,
	.yres           = 160,
	.bits_per_pixel = 16,
	.flags          = 0,
};

static struct tegra_fb_data bonaire_fb_data = {
	.win		= 0,
	.xres		= 1366,
	.yres		= 768,
	.bits_per_pixel	= 16,
	.flags		= TEGRA_FB_FLIP_ON_PROBE,
};

static int bonaire_dsi_panel_enable(void)
{
	int ret;

	if (bonaire_dsi_reg == NULL) {
		bonaire_dsi_reg = regulator_get(NULL, "avdd_dsi_csi");
		if (IS_ERR(bonaire_dsi_reg)) {
		pr_err("dsi: Could not get regulator avdd_dsi_csi\n");
			bonaire_dsi_reg = NULL;
			return PTR_ERR(bonaire_dsi_reg);
		}
	}
	regulator_enable(bonaire_dsi_reg);

	ret = gpio_request(TEGRA_GPIO_PJ1, "DSI TE");
	if (ret < 0)
		return ret;

	ret = gpio_direction_input(TEGRA_GPIO_PJ1);
	if (ret < 0) {
		gpio_free(TEGRA_GPIO_PJ1);
		return ret;
	}


#if DSI_PANEL_219

	ret = gpio_request(TEGRA_GPIO_PH0, "ph0");
	if (ret < 0)
		return ret;
	ret = gpio_direction_output(TEGRA_GPIO_PH0, 0);
	if (ret < 0) {
		gpio_free(TEGRA_GPIO_PH0);
		return ret;
	}

	ret = gpio_request(TEGRA_GPIO_PH2, "ph2");
	if (ret < 0)
		return ret;
	ret = gpio_direction_output(TEGRA_GPIO_PH2, 0);
	if (ret < 0) {
		gpio_free(TEGRA_GPIO_PH2);
		return ret;
	}

	ret = gpio_request(TEGRA_GPIO_PU2, "pu2");
	if (ret < 0)
		return ret;
	ret = gpio_direction_output(TEGRA_GPIO_PU2, 0);
	if (ret < 0) {
		gpio_free(TEGRA_GPIO_PU2);
		return ret;
	}

	gpio_set_value(bonaire_lvds_shutdown, 1);
	mdelay(20);
	gpio_set_value(TEGRA_GPIO_PH0, 1);
	mdelay(10);
	gpio_set_value(TEGRA_GPIO_PH2, 1);
	mdelay(15);
	gpio_set_value(TEGRA_GPIO_PU2, 0);
	gpio_set_value(TEGRA_GPIO_PU2, 1);
	mdelay(10);
	gpio_set_value(TEGRA_GPIO_PU2, 0);
	mdelay(10);
	gpio_set_value(TEGRA_GPIO_PU2, 1);
	mdelay(15);
#endif

#if DSI_PANEL_218
	printk("DSI_PANEL_218 is enabled\n");
	ret = gpio_request(AVDD_LCD, "avdd_lcd");
	if (ret < 0)
		gpio_free(AVDD_LCD);
	ret = gpio_direction_output(AVDD_LCD, 1);
	if (ret < 0)
		gpio_free(AVDD_LCD);


#if DSI_PANEL_RESET
	ret = gpio_request(TEGRA_GPIO_PD2, "pd2");
	if (ret < 0) {
		return ret;
	}
	ret = gpio_direction_output(TEGRA_GPIO_PD2, 0);
	if (ret < 0) {
		gpio_free(TEGRA_GPIO_PD2);
		return ret;
	}

	gpio_set_value(TEGRA_GPIO_PD2, 1);
	gpio_set_value(TEGRA_GPIO_PD2, 0);
	mdelay(2);
	gpio_set_value(TEGRA_GPIO_PD2, 1);
	mdelay(2);
#endif
#endif

	return 0;
}

static int bonaire_dsi_panel_disable(void)
{
	int err;

	err = 0;
	printk(KERN_INFO "DSI panel disable\n");

#if DSI_PANEL_219
	gpio_free(TEGRA_GPIO_PU2);
	gpio_free(TEGRA_GPIO_PH2);
	gpio_free(TEGRA_GPIO_PH0);
	gpio_free(TEGRA_GPIO_PL2);
#endif

#if DSI_PANEL_218
	gpio_free(TEGRA_GPIO_PD2);
#endif

	return err;
}

static int bonaire_dsi_panel_postsuspend(void)
{
	int err;

	err = 0;
	printk(KERN_INFO "DSI panel postsuspend\n");

	if (bonaire_dsi_reg) {
		err = regulator_disable(bonaire_dsi_reg);
		if (err < 0)
			printk(KERN_ERR
			"DSI regulator avdd_dsi_csi disable failed\n");
		regulator_put(bonaire_dsi_reg);
		bonaire_dsi_reg = NULL;
	}

#if DSI_PANEL_218
	gpio_free(AVDD_LCD);
#endif

	return err;
}

static struct tegra_dsi_cmd dsi_init_cmd[] = {
	DSI_CMD_SHORT(0x05, 0x11, 0x00),
	DSI_DLY_MS(150),
	DSI_CMD_SHORT(0x05, 0x29, 0x00),
	DSI_DLY_MS(20),
};

static struct tegra_dsi_cmd dsi_suspend_cmd[] = {
	DSI_CMD_SHORT(0x05, 0x28, 0x00),
	DSI_DLY_MS(20),
	DSI_CMD_SHORT(0x05, 0x10, 0x00),
	DSI_DLY_MS(5),
};

struct tegra_dsi_out bonaire_dsi = {
	.n_data_lanes = 2,
	.pixel_format = TEGRA_DSI_PIXEL_FORMAT_24BIT_P,
	.refresh_rate = 60,
	.virtual_channel = TEGRA_DSI_VIRTUAL_CHANNEL_0,

	.panel_has_frame_buffer = true,
#ifdef CONFIG_TEGRA_DSI_INSTANCE_1
	.dsi_instance = 1,
#else
	.dsi_instance = 0,
#endif
	.panel_reset = DSI_PANEL_RESET,

	.n_init_cmd = ARRAY_SIZE(dsi_init_cmd),
	.dsi_init_cmd = dsi_init_cmd,

	.n_suspend_cmd = ARRAY_SIZE(dsi_suspend_cmd),
	.dsi_suspend_cmd = dsi_suspend_cmd,

	.video_data_type = TEGRA_DSI_VIDEO_TYPE_COMMAND_MODE,
	.lp_cmd_mode_freq_khz = 430000,
};

static struct tegra_dc_mode bonaire_dsi_modes[] = {
#if DSI_PANEL_219
	{
		.pclk = 10000000,
		.h_ref_to_sync = 4,
		.v_ref_to_sync = 1,
		.h_sync_width = 16,
		.v_sync_width = 1,
		.h_back_porch = 32,
		.v_back_porch = 1,
		.h_active = 540,
		.v_active = 960,
		.h_front_porch = 32,
		.v_front_porch = 2,
	},
#endif

#if DSI_PANEL_218
	{
		.pclk = 323000000,
		.h_ref_to_sync = 11,
		.v_ref_to_sync = 1,
		.h_sync_width = 16,
		.v_sync_width = 4,
		.h_back_porch = 16,
		.v_back_porch = 4,
		.h_active = 864,
		.v_active = 480,
		.h_front_porch = 16,
		.v_front_porch = 4,
	},
#endif

};

static struct tegra_fb_data bonaire_dsi_fb_data = {
#if DSI_PANEL_219
	.win		= 0,
	.xres		= 540,
	.yres		= 960,
	.bits_per_pixel	= 32,
#endif

#if DSI_PANEL_218
	.win		= 0,
	.xres		= 864,
	.yres		= 480,
	.bits_per_pixel	= 32,
#endif
};

static struct tegra_dc_out bonaire_disp_out = {
	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,
	.flags		= TEGRA_DC_OUT_CONTINUOUS_MODE,

#if defined(CONFIG_TEGRA_DP)
	.type		= TEGRA_DC_OUT_DP,

	.modes		= bonaire_panel_modes,
	.n_modes	= ARRAY_SIZE(bonaire_panel_modes),
	.out_pins       = edp_out_pins,
	.n_out_pins     = ARRAY_SIZE(edp_out_pins),
	.depth		= 18,
#elif defined(CONFIG_TEGRA_LVDS)
	.type		= TEGRA_DC_OUT_LVDS,

	.modes	 	= bonaire_lvds_panel_modes,
	.n_modes 	= ARRAY_SIZE(bonaire_lvds_panel_modes),
	.out_pins       = lvds_out_pins,
	.n_out_pins     = ARRAY_SIZE(lvds_out_pins),
#elif defined(CONFIG_TEGRA_BONAIRE_DSI)
	.type		= TEGRA_DC_OUT_DSI,

	.modes	 	= bonaire_dsi_modes,
	.n_modes 	= ARRAY_SIZE(bonaire_dsi_modes),

	.dsi		= &bonaire_dsi,

	.enable		= bonaire_dsi_panel_enable,
	.disable	= bonaire_dsi_panel_disable,
	.postsuspend	= bonaire_dsi_panel_postsuspend,
#else
	.type		= TEGRA_DC_OUT_RGB,

	.modes	 	= bonaire_panel_modes,
	.n_modes 	= ARRAY_SIZE(bonaire_panel_modes),

	.enable		= bonaire_panel_enable,
	.disable	= bonaire_panel_disable,
#endif
};

static struct tegra_dc_platform_data bonaire_disp_pdata = {
	.flags		= TEGRA_DC_FLAG_ENABLED,
	.default_out	= &bonaire_disp_out,
	.emc_clk_rate	= 300000000,
#ifndef CONFIG_TEGRA_BONAIRE_DSI
	.fb		= &bonaire_fb_data,
#else
	.fb		= &bonaire_dsi_fb_data,
#endif
};

static struct platform_device bonaire_disp1_device = {
	.name		= "tegradc",
	.id		= 0,
	.resource	= bonaire_disp1_resources,
	.num_resources	= ARRAY_SIZE(bonaire_disp1_resources),
	.dev = {
		.platform_data = &bonaire_disp_pdata,
	},
};

static struct platform_device bonaire_disp2_device = {
	.name		= "tegradc",
	.id		= 1,
	.resource	= bonaire_disp2_resources,
	.num_resources	= ARRAY_SIZE(bonaire_disp2_resources),
	.dev = {
		.platform_data = &bonaire_disp_pdata,
	},
};

static struct nvmap_platform_carveout bonaire_carveouts[] = {
	[0] = {
		.name		= "iram",
		.usage_mask	= NVMAP_HEAP_CARVEOUT_IRAM,
		.base		= TEGRA_IRAM_BASE,
		.size		= TEGRA_IRAM_SIZE,
		.buddy_size	= 0, /* no buddy allocation for IRAM */
	},
	[1] = {
		.name		= "generic-0",
		.usage_mask	= NVMAP_HEAP_CARVEOUT_GENERIC,
		.base		= 0,	/* Filled in by bonaire_panel_init() */
		.size		= 0,	/* Filled in by bonaire_panel_init() */
		.buddy_size	= SZ_32K,
	},
	[2] = {
		.name		= "vpr",
		.usage_mask	= NVMAP_HEAP_CARVEOUT_VPR,
		.base		= 0,	/* Filled in by bonaire_panel_init() */
		.size		= 0,	/* Filled in by bonaire_panel_init() */
		.buddy_size	= SZ_32K,
	},
};

static struct nvmap_platform_data bonaire_nvmap_data = {
	.carveouts	= bonaire_carveouts,
	.nr_carveouts	= ARRAY_SIZE(bonaire_carveouts),
};

static struct platform_device bonaire_nvmap_device = {
	.name	= "tegra-nvmap",
	.id	= -1,
	.dev	= {
		.platform_data = &bonaire_nvmap_data,
	},
};

static struct platform_device *bonaire_gfx_devices[] __initdata = {
	&bonaire_nvmap_device,
	&tegra_pwfm_device,
	&bonaire_backlight_device,
};

int __init bonaire_panel_init(void)
{
	int err;
	struct resource *res;
#if defined(CONFIG_TEGRA_GRHOST)
	struct platform_device *phost1x;
#endif

#ifdef CONFIG_TEGRA_PRE_SILICON_SUPPORT
	if (tegra_platform_is_linsim()) {
		bonaire_panel_modes[0].h_active = 120;
		bonaire_panel_modes[0].v_active = 160;
		bonaire_fb_data = bonaire_fb_data_linsim;
		bonaire_dsi.pixel_format = TEGRA_DSI_PIXEL_FORMAT_16BIT_P;
		bonaire_dsi.video_data_type = TEGRA_DSI_VIDEO_TYPE_VIDEO_MODE;
#if DSI_PANEL_218
		bonaire_dsi_modes[0].h_active = 320;
		bonaire_dsi_modes[0].v_active = 240;
		bonaire_dsi_fb_data.xres = 320;
		bonaire_dsi_fb_data.yres = 240;
		bonaire_dsi_fb_data.bits_per_pixel = 16;
#endif
		bonaire_carveouts[0].size = 0;
	}
#endif

	bonaire_carveouts[1].base = tegra_carveout_start;
	bonaire_carveouts[1].size = tegra_carveout_size;
	bonaire_carveouts[2].base = tegra_vpr_start;
	bonaire_carveouts[2].size = tegra_vpr_size;

	err = platform_add_devices(bonaire_gfx_devices,
				   ARRAY_SIZE(bonaire_gfx_devices));

#ifdef CONFIG_TEGRA_GRHOST
	phost1x = bonaire_host1x_init();
	if (!phost1x)
		return -EINVAL;
#endif

#if defined(CONFIG_TEGRA_GRHOST) && defined(CONFIG_TEGRA_DC)
	if (!tegra_platform_is_fpga()) {
		res = platform_get_resource_byname(&bonaire_disp1_device,
			IORESOURCE_MEM, "fbmem");
		res->start = tegra_fb_start;
		res->end = tegra_fb_start + tegra_fb_size - 1;

#ifdef CONFIG_TEGRA_IOMMU_SMMU
		/* Copy the bootloader fb to the fb. */
		__tegra_move_framebuffer(&bonaire_nvmap_device,
			tegra_fb_start, tegra_bootloader_fb_start,
			tegra_fb_size);
#endif

		if (!err) {
			bonaire_disp1_device.dev.parent = &phost1x->dev;
			err = platform_device_register(&bonaire_disp1_device);
		}
	} else {
		/* FPGA only has disp2 support */
		res = platform_get_resource_byname(&bonaire_disp2_device,
			IORESOURCE_MEM, "fbmem");
		res->start = tegra_fb_start;
		res->end = tegra_fb_start + tegra_fb_size - 1;

#ifdef CONFIG_TEGRA_IOMMU_SMMU
		/* Copy the bootloader fb to the fb. */
		__tegra_move_framebuffer(&bonaire_nvmap_device,
			tegra_fb_start, tegra_bootloader_fb_start,
			tegra_fb_size);
#endif

		if (!err) {
			bonaire_disp2_device.dev.parent = &phost1x->dev;
			err = platform_device_register(&bonaire_disp2_device);
		}
	}

#endif

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
