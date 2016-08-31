/*
 * arch/arm/mach-tegra/board-macallan-panel.c
 *
 * Copyright (c) 2013, NVIDIA Corporation. All rights reserved.
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

#include <mach/irqs.h>
#include <mach/dc.h>

#include "board.h"
#include "devices.h"
#include "gpio-names.h"
#include "board-panel.h"
#include "common.h"
#include "iomap.h"
#include "tegra11_host1x_devices.h"

#define DSI_PANEL_RST_GPIO	TEGRA_GPIO_PH5
#define DSI_PANEL_BL_PWM_GPIO	TEGRA_GPIO_PH1

struct platform_device * __init macallan_host1x_init(void)
{
	struct platform_device *pdev = NULL;

#ifdef CONFIG_TEGRA_GRHOST
	if (!of_have_populated_dt())
		pdev = tegra11_register_host1x_devices();
	else
		pdev = to_platform_device(bus_find_device_by_name(
			&platform_bus_type, NULL, "host1x"));
#endif
	return pdev;
}

#ifdef CONFIG_TEGRA_DC

/* HDMI Hotplug detection pin */
#define macallan_hdmi_hpd	TEGRA_GPIO_PN7

static struct regulator *macallan_hdmi_reg;
static struct regulator *macallan_hdmi_pll;
static struct regulator *macallan_hdmi_vddio;

static struct resource macallan_disp1_resources[] = {
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
		.start	= 0, /* Filled in by macallan_panel_init() */
		.end	= 0, /* Filled in by macallan_panel_init() */
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "ganged_dsia_regs",
		.start	= 0, /* Filled in the panel file by init_resources() */
		.end	= 0, /* Filled in the panel file by init_resources() */
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "ganged_dsib_regs",
		.start	= 0, /* Filled in the panel file by init_resources() */
		.end	= 0, /* Filled in the panel file by init_resources() */
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

static struct resource macallan_disp2_resources[] = {
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
		.start	= 0, /* Filled in by macallan_panel_init() */
		.end	= 0, /* Filled in by macallan_panel_init() */
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

static struct tegra_dc_out macallan_disp1_out = {
	.type		= TEGRA_DC_OUT_DSI,
	.sd_settings	= &sd_settings,
};

static int macallan_hdmi_enable(struct device *dev)
{
	int ret;
	if (!macallan_hdmi_reg) {
		macallan_hdmi_reg = regulator_get(dev, "avdd_hdmi");
		if (IS_ERR(macallan_hdmi_reg)) {
			pr_err("hdmi: couldn't get regulator avdd_hdmi\n");
			macallan_hdmi_reg = NULL;
			return PTR_ERR(macallan_hdmi_reg);
		}
	}
	ret = regulator_enable(macallan_hdmi_reg);
	if (ret < 0) {
		pr_err("hdmi: couldn't enable regulator avdd_hdmi\n");
		return ret;
	}
	if (!macallan_hdmi_pll) {
		macallan_hdmi_pll = regulator_get(dev, "avdd_hdmi_pll");
		if (IS_ERR(macallan_hdmi_pll)) {
			pr_err("hdmi: couldn't get regulator avdd_hdmi_pll\n");
			macallan_hdmi_pll = NULL;
			regulator_put(macallan_hdmi_reg);
			macallan_hdmi_reg = NULL;
			return PTR_ERR(macallan_hdmi_pll);
		}
	}
	ret = regulator_enable(macallan_hdmi_pll);
	if (ret < 0) {
		pr_err("hdmi: couldn't enable regulator avdd_hdmi_pll\n");
		return ret;
	}
	return 0;
}

static int macallan_hdmi_disable(void)
{
	if (macallan_hdmi_reg) {
		regulator_disable(macallan_hdmi_reg);
		regulator_put(macallan_hdmi_reg);
		macallan_hdmi_reg = NULL;
	}

	if (macallan_hdmi_pll) {
		regulator_disable(macallan_hdmi_pll);
		regulator_put(macallan_hdmi_pll);
		macallan_hdmi_pll = NULL;
	}

	return 0;
}

static int macallan_hdmi_postsuspend(void)
{
	if (macallan_hdmi_vddio) {
		regulator_disable(macallan_hdmi_vddio);
		regulator_put(macallan_hdmi_vddio);
		macallan_hdmi_vddio = NULL;
	}
	return 0;
}

static int macallan_hdmi_hotplug_init(struct device *dev)
{
	int e = 0;

	if (!macallan_hdmi_vddio) {
		macallan_hdmi_vddio = regulator_get(dev, "vdd_hdmi_5v0");
		if (WARN_ON(IS_ERR(macallan_hdmi_vddio))) {
			e = PTR_ERR(macallan_hdmi_vddio);
			pr_err("%s: couldn't get regulator vdd_hdmi_5v0: %d\n",
				__func__, e);
			macallan_hdmi_vddio = NULL;
		} else {
			e = regulator_enable(macallan_hdmi_vddio);
			mdelay(5);
		}
	}

	return e;
}

static struct tegra_dc_out macallan_disp2_out = {
	.type		= TEGRA_DC_OUT_HDMI,
	.flags		= TEGRA_DC_OUT_HOTPLUG_HIGH,
	.parent_clk	= "pll_d2_out0",

	.dcc_bus	= 3,
	.hotplug_gpio	= macallan_hdmi_hpd,

	.max_pixclock	= KHZ2PICOS(297000),

	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,

	.enable		= macallan_hdmi_enable,
	.disable	= macallan_hdmi_disable,
	.postsuspend	= macallan_hdmi_postsuspend,
	.hotplug_init	= macallan_hdmi_hotplug_init,
};

static struct tegra_fb_data macallan_disp1_fb_data = {
	.win		= 0,
	.bits_per_pixel = 32,
	.flags		= TEGRA_FB_FLIP_ON_PROBE,
};

static struct tegra_dc_platform_data macallan_disp1_pdata = {
	.flags		= TEGRA_DC_FLAG_ENABLED,
	.default_out	= &macallan_disp1_out,
	.fb		= &macallan_disp1_fb_data,
	.emc_clk_rate	= 204000000,
#ifdef CONFIG_TEGRA_DC_CMU
	.cmu_enable	= 1,
#endif
};

static struct tegra_fb_data macallan_disp2_fb_data = {
	.win		= 0,
	.xres		= 1024,
	.yres		= 600,
	.bits_per_pixel = 32,
	.flags		= TEGRA_FB_FLIP_ON_PROBE,
};

static struct tegra_dc_platform_data macallan_disp2_pdata = {
	.flags		= TEGRA_DC_FLAG_ENABLED,
	.default_out	= &macallan_disp2_out,
	.fb		= &macallan_disp2_fb_data,
	.emc_clk_rate	= 300000000,
#ifdef CONFIG_TEGRA_DC_CMU
	.cmu_enable	= 1,
#endif
};

static struct platform_device macallan_disp2_device = {
	.name		= "tegradc",
	.id		= 1,
	.resource	= macallan_disp2_resources,
	.num_resources	= ARRAY_SIZE(macallan_disp2_resources),
	.dev = {
		.platform_data = &macallan_disp2_pdata,
	},
};

static struct platform_device macallan_disp1_device = {
	.name		= "tegradc",
	.id		= 0,
	.resource	= macallan_disp1_resources,
	.num_resources	= ARRAY_SIZE(macallan_disp1_resources),
	.dev = {
		.platform_data = &macallan_disp1_pdata,
	},
};

static struct nvmap_platform_carveout macallan_carveouts[] = {
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
		.base		= 0, /* Filled in by macallan_panel_init() */
		.size		= 0, /* Filled in by macallan_panel_init() */
		.buddy_size	= SZ_32K,
	},
	[2] = {
		.name		= "vpr",
		.usage_mask	= NVMAP_HEAP_CARVEOUT_VPR,
		.base		= 0, /* Filled in by macallan_panel_init() */
		.size		= 0, /* Filled in by macallan_panel_init() */
		.buddy_size	= SZ_32K,
	},
};

static struct nvmap_platform_data macallan_nvmap_data = {
	.carveouts	= macallan_carveouts,
	.nr_carveouts	= ARRAY_SIZE(macallan_carveouts),
};
static struct platform_device macallan_nvmap_device = {
	.name	= "tegra-nvmap",
	.id	= -1,
	.dev	= {
		.platform_data = &macallan_nvmap_data,
	},
};

static void macallan_panel_select(void)
{
	struct tegra_panel *panel = NULL;
	struct board_info board;
	u8 dsi_instance = DSI_INSTANCE_0;

	tegra_get_display_board_info(&board);

	switch (board.board_id) {
	case BOARD_E1639:
		panel = &dsi_s_wqxga_10_1;
		break;
	default:
		panel = &dsi_p_wuxga_10_1;
		break;
	}
	if (panel) {
		if (panel->init_sd_settings)
			panel->init_sd_settings(&sd_settings);

		if (panel->init_dc_out) {
			panel->init_dc_out(&macallan_disp1_out);
			macallan_disp1_out.dsi->dsi_instance = dsi_instance;
			macallan_disp1_out.dsi->dsi_panel_rst_gpio =
				DSI_PANEL_RST_GPIO;
			macallan_disp1_out.dsi->dsi_panel_bl_pwm_gpio =
				DSI_PANEL_BL_PWM_GPIO;
		}

		if (panel->init_fb_data)
			panel->init_fb_data(&macallan_disp1_fb_data);

		if (panel->init_cmu_data)
			panel->init_cmu_data(&macallan_disp1_pdata);

		if (panel->set_disp_device)
			panel->set_disp_device(&macallan_disp1_device);

		tegra_dsi_resources_init(dsi_instance, macallan_disp1_resources,
				 ARRAY_SIZE(macallan_disp1_resources));

		if (panel->register_bl_dev)
			panel->register_bl_dev();

		if (panel->register_i2c_bridge)
			panel->register_i2c_bridge();
	}

}
int __init macallan_panel_init(void)
{
	int err = 0;
	struct resource __maybe_unused *res;
	struct platform_device *phost1x = NULL;

	macallan_panel_select();

#ifdef CONFIG_TEGRA_NVMAP
	macallan_carveouts[1].base = tegra_carveout_start;
	macallan_carveouts[1].size = tegra_carveout_size;
	macallan_carveouts[2].base = tegra_vpr_start;
	macallan_carveouts[2].size = tegra_vpr_size;

	err = platform_device_register(&macallan_nvmap_device);
	if (err) {
		pr_err("nvmap device registration failed\n");
		return err;
	}
#endif

	phost1x = macallan_host1x_init();
	if (!phost1x) {
		pr_err("host1x devices registration failed\n");
		return -EINVAL;
	}

	res = platform_get_resource_byname(&macallan_disp1_device,
					 IORESOURCE_MEM, "fbmem");
	res->start = tegra_fb_start;
	res->end = tegra_fb_start + tegra_fb_size - 1;

	/* Copy the bootloader fb to the fb. */
	__tegra_move_framebuffer(&macallan_nvmap_device,
		tegra_fb_start, tegra_bootloader_fb_start,
			min(tegra_fb_size, tegra_bootloader_fb_size));

	macallan_disp1_device.dev.parent = &phost1x->dev;
	err = platform_device_register(&macallan_disp1_device);
	if (err) {
		pr_err("disp1 device registration failed\n");
		return err;
	}

	err = tegra_init_hdmi(&macallan_disp2_device, phost1x);
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
int __init macallan_panel_init(void)
{
	if (macallan_host1x_init())
		return 0;
	else
		return -EINVAL;
}
#endif
