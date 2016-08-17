/*
 * arch/arm/mach-tegra/board-pluto-panel.c
 *
 * Copyright (C) 2012-2013 NVIDIA Corporation. All rights reserved.
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
#include <linux/i2c/pca953x.h>
#include <mach/irqs.h>
#include <mach/iomap.h>
#include <mach/dc.h>

#include "board.h"
#include "devices.h"
#include "gpio-names.h"
#include "board-pluto.h"
#include "board-panel.h"
#include "common.h"

#include "tegra11_host1x_devices.h"

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
	.sd_settings	= &sd_settings,
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
	.win		= 0,
	.bits_per_pixel = 32,
	.flags		= TEGRA_FB_FLIP_ON_PROBE,
};

static struct tegra_dc_platform_data pluto_disp1_pdata = {
	.flags		= TEGRA_DC_FLAG_ENABLED,
	.default_out	= &pluto_disp1_out,
	.fb		= &pluto_disp1_fb_data,
	.emc_clk_rate	= 204000000,
#ifdef CONFIG_TEGRA_DC_CMU
	.cmu_enable	= 1,
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

static void __init pluto_panel_select(void)
{
	struct tegra_panel *panel;
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

	if (panel->init_sd_settings)
		panel->init_sd_settings(&sd_settings);

	if (panel->init_dc_out)
		panel->init_dc_out(&pluto_disp1_out);

	if (panel->init_fb_data)
		panel->init_fb_data(&pluto_disp1_fb_data);

	if (panel->init_cmu_data)
		panel->init_cmu_data(&pluto_disp1_pdata);

	if (panel->set_disp_device)
		panel->set_disp_device(&pluto_disp1_device);

	if (panel->init_resources)
		panel->init_resources(pluto_disp1_resources,
			ARRAY_SIZE(pluto_disp1_resources));

	if (panel->register_bl_dev)
		panel->register_bl_dev();

}
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
	if (err)
		return err;

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
