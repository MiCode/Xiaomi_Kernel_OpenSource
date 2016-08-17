/*
 * arch/arm/mach-tegra/board-aruba-panel.c
 *
 * Copyright (c) 2010-2012, NVIDIA Corporation.
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
#include <mach/irqs.h>
#include <mach/iomap.h>
#include <mach/dc.h>
#include <mach/fb.h>

#include "board.h"
#include "devices.h"
#include "gpio-names.h"
#include "tegra2_host1x_devices.h"

#define aruba_lvds_shutdown	TEGRA_GPIO_PB2
#define aruba_bl_enb		TEGRA_GPIO_PW1

static int aruba_backlight_init(struct device *dev) {
	int ret;

	ret = gpio_request(aruba_bl_enb, "backlight_enb");
	if (ret < 0)
		return ret;

	ret = gpio_direction_output(aruba_bl_enb, 1);
	if (ret < 0)
		gpio_free(aruba_bl_enb);

	return ret;
};

static void aruba_backlight_exit(struct device *dev) {
	gpio_set_value(aruba_bl_enb, 0);
	gpio_free(aruba_bl_enb);
}

static int aruba_backlight_notify(struct device *unused, int brightness)
{
	gpio_set_value(aruba_bl_enb, !!brightness);
	return brightness;
}

static struct platform_pwm_backlight_data aruba_backlight_data = {
	.pwm_id		= 2,
	.max_brightness	= 255,
	.dft_brightness	= 224,
	.pwm_period_ns	= 5000000,
	.init		= aruba_backlight_init,
	.exit		= aruba_backlight_exit,
	.notify		= aruba_backlight_notify,
};

static struct platform_device aruba_backlight_device = {
	.name	= "pwm-backlight",
	.id	= -1,
	.dev	= {
		.platform_data = &aruba_backlight_data,
	},
};

#ifdef CONFIG_TEGRA_DC
static int aruba_panel_enable(struct device *dev)
{
	static struct regulator *reg = NULL;

	if (reg == NULL) {
		reg = regulator_get(dev, "avdd_lvds");
		if (WARN_ON(IS_ERR(reg)))
			pr_err("%s: couldn't get regulator avdd_lvds: %ld\n",
			       __func__, PTR_ERR(reg));
		else
			regulator_enable(reg);
	}

	gpio_set_value(aruba_lvds_shutdown, 1);
	return 0;
}

static int aruba_panel_disable(void)
{
	gpio_set_value(aruba_lvds_shutdown, 0);
	return 0;
}

static struct resource aruba_disp1_resources[] = {
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
		.start	= 0,	/* Filled in by aruba_panel_init() */
		.end	= 0,	/* Filled in by aruba_panel_init() */
		.flags	= IORESOURCE_MEM,
	},
};

static struct tegra_dc_mode aruba_panel_modes[] = {
	{
		.pclk = 18000000,
		.h_ref_to_sync = 8,
		.v_ref_to_sync = 2,
		.h_sync_width = 4,
		.v_sync_width = 1,
		.h_back_porch = 20,
		.v_back_porch = 7,
		.h_active = 480,
		.v_active = 640,
		.h_front_porch = 8,
		.v_front_porch = 8,
	},
};

static struct tegra_fb_data aruba_fb_data = {
	.win		= 0,
	.xres		= 480,
	.yres		= 640,
	.bits_per_pixel	= 16,
};

static struct tegra_dc_out aruba_disp1_out = {
	.type		= TEGRA_DC_OUT_RGB,

	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,

	.modes	 	= aruba_panel_modes,
	.n_modes 	= ARRAY_SIZE(aruba_panel_modes),

	.enable		= aruba_panel_enable,
	.disable	= aruba_panel_disable,
};

static struct tegra_dc_platform_data aruba_disp1_pdata = {
	.flags		= TEGRA_DC_FLAG_ENABLED,
	.default_out	= &aruba_disp1_out,
	.fb		= &aruba_fb_data,
};

static struct platform_device aruba_disp1_device = {
	.name		= "tegradc",
	.id		= 0,
	.resource	= aruba_disp1_resources,
	.num_resources	= ARRAY_SIZE(aruba_disp1_resources),
	.dev = {
		.platform_data = &aruba_disp1_pdata,
	},
};
#endif

#if defined(CONFIG_TEGRA_NVMAP)
static struct nvmap_platform_carveout aruba_carveouts[] = {
	[0] = NVMAP_HEAP_CARVEOUT_IRAM_INIT,
	[1] = {
		.name		= "generic-0",
		.usage_mask	= NVMAP_HEAP_CARVEOUT_GENERIC,
		.base		= 0,	/* Filled in by aruba_panel_init() */
		.size		= 0,	/* Filled in by aruba_panel_init() */
		.buddy_size	= SZ_32K,
	},
};

static struct nvmap_platform_data aruba_nvmap_data = {
	.carveouts	= aruba_carveouts,
	.nr_carveouts	= ARRAY_SIZE(aruba_carveouts),
};

static struct platform_device aruba_nvmap_device = {
	.name	= "tegra-nvmap",
	.id	= -1,
	.dev	= {
		.platform_data = &aruba_nvmap_data,
	},
};
#endif

static struct platform_device *aruba_gfx_devices[] __initdata = {
#if defined(CONFIG_TEGRA_NVMAP)
	&aruba_nvmap_device,
#endif
	&tegra_pwfm2_device,
	&aruba_backlight_device,
};

int __init aruba_panel_init(void)
{
	int err;
	struct resource __maybe_unused *res;
	struct platform_device *phost1x;

#if defined(CONFIG_TEGRA_NVMAP)
	aruba_carveouts[1].base = tegra_carveout_start;
	aruba_carveouts[1].size = tegra_carveout_size;
#endif

	err = platform_add_devices(aruba_gfx_devices,
		ARRAY_SIZE(aruba_gfx_devices));

#ifdef CONFIG_TEGRA_GRHOST
	phost1x = tegra2_register_host1x_devices();
	if (!phost1x)
		return -EINVAL;
#endif

#if defined(CONFIG_TEGRA_GRHOST) && defined(CONFIG_TEGRA_DC)
	res = platform_get_resource_byname(&aruba_disp1_device,
					 IORESOURCE_MEM, "fbmem");
	res->start = tegra_fb_start;
	res->end = tegra_fb_start + tegra_fb_size - 1;
#endif

	/* Copy the bootloader fb to the fb. */
	__tegra_move_framebuffer(&aruba_nvmap_device,
		tegra_fb_start, tegra_bootloader_fb_start,
				min(tegra_fb_size, tegra_bootloader_fb_size));

#if defined(CONFIG_TEGRA_GRHOST) && defined(CONFIG_TEGRA_DC)
	if (!err) {
		aruba_disp1_device.dev.parent = &phost1x->dev;
		err = platform_device_register(&aruba_disp1_device);
	}
#endif

	return err;
}
