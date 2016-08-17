/*
 * arch/arm/mach-tegra/board-whistler-panel.c
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
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/pwm_backlight.h>
#include <linux/tegra_pwm_bl.h>
#include <linux/nvhost.h>
#include <linux/nvmap.h>

#include <asm/mach-types.h>

#include <mach/irqs.h>
#include <mach/iomap.h>
#include <mach/dc.h>
#include <mach/fb.h>
#include <mach/gpio-tegra.h>

#include "devices.h"
#include "gpio-names.h"
#include "board.h"
#include "tegra2_host1x_devices.h"

#define whistler_hdmi_hpd	TEGRA_GPIO_PN7

#ifdef CONFIG_TEGRA_DC
static struct regulator *whistler_hdmi_reg = NULL;
static struct regulator *whistler_hdmi_pll = NULL;
#endif

/*
 * In case which_pwm is TEGRA_PWM_PM0,
 * gpio_conf_to_sfio should be TEGRA_GPIO_PW0: set LCD_CS1_N pin to SFIO
 * In case which_pwm is TEGRA_PWM_PM1,
 * gpio_conf_to_sfio should be TEGRA_GPIO_PW1: set LCD_M1 pin to SFIO
 */
static struct platform_tegra_pwm_backlight_data whistler_disp1_backlight_data = {
	.which_dc = 0,
	.which_pwm = TEGRA_PWM_PM1,
	.max_brightness	= 256,
	.dft_brightness	= 77,
	.gpio_conf_to_sfio	= TEGRA_GPIO_PW1,
	.period	= 0x1F,
	.clk_div = 3,
	.clk_select = 2,
};

static struct platform_device whistler_disp1_backlight_device = {
	.name	= "tegra-pwm-bl",
	.id	= -1,
	.dev	= {
		.platform_data = &whistler_disp1_backlight_data,
	},
};

#ifdef CONFIG_TEGRA_DC
static int whistler_hdmi_enable(struct device *dev)
{
	if (!whistler_hdmi_reg) {
		whistler_hdmi_reg = regulator_get(dev, "avdd_hdmi"); /* LD011 */
		if (IS_ERR_OR_NULL(whistler_hdmi_reg)) {
			pr_err("hdmi: couldn't get regulator avdd_hdmi\n");
			whistler_hdmi_reg = NULL;
			return PTR_ERR(whistler_hdmi_reg);
		}
	}
	regulator_enable(whistler_hdmi_reg);

	if (!whistler_hdmi_pll) {
		whistler_hdmi_pll =
			regulator_get(dev, "avdd_hdmi_pll"); /* LD06 */
		if (IS_ERR_OR_NULL(whistler_hdmi_pll)) {
			pr_err("hdmi: couldn't get regulator avdd_hdmi_pll\n");
			whistler_hdmi_pll = NULL;
			regulator_disable(whistler_hdmi_reg);
			whistler_hdmi_reg = NULL;
			return PTR_ERR(whistler_hdmi_pll);
		}
	}
	regulator_enable(whistler_hdmi_pll);
	return 0;
}

static int whistler_hdmi_disable(void)
{
	regulator_disable(whistler_hdmi_reg);
	regulator_disable(whistler_hdmi_pll);
	return 0;
}

static struct resource whistler_disp1_resources[] = {
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
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource whistler_disp2_resources[] = {
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
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "hdmi_regs",
		.start	= TEGRA_HDMI_BASE,
		.end	= TEGRA_HDMI_BASE + TEGRA_HDMI_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct tegra_dc_mode whistler_panel_modes[] = {
	{
		.pclk = 27000000,
		.h_ref_to_sync = 4,
		.v_ref_to_sync = 2,
		.h_sync_width = 10,
		.v_sync_width = 3,
		.h_back_porch = 20,
		.v_back_porch = 3,
		.h_active = 800,
		.v_active = 480,
		.h_front_porch = 70,
		.v_front_porch = 3,
	},
};

static struct tegra_dc_out_pin whistler_dc_out_pins[] = {
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
		.pol	= TEGRA_DC_OUT_PIN_POL_LOW,
	},
};

static u8 whistler_dc_out_pin_sel_config[] = {
	TEGRA_PIN_OUT_CONFIG_SEL_LM1_PM1,
};

static struct tegra_dc_out whistler_disp1_out = {
	.type		= TEGRA_DC_OUT_RGB,

	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,

	.height		= 54, /* mm */
	.width		= 90, /* mm */

	.modes	 	= whistler_panel_modes,
	.n_modes 	= ARRAY_SIZE(whistler_panel_modes),

	.out_pins	= whistler_dc_out_pins,
	.n_out_pins	= ARRAY_SIZE(whistler_dc_out_pins),

	.out_sel_configs   = whistler_dc_out_pin_sel_config,
	.n_out_sel_configs = ARRAY_SIZE(whistler_dc_out_pin_sel_config),
};

static struct tegra_dc_out whistler_disp2_out = {
	.type		= TEGRA_DC_OUT_HDMI,
	.flags		= TEGRA_DC_OUT_HOTPLUG_HIGH,

	.dcc_bus	= 1,
	.hotplug_gpio	= whistler_hdmi_hpd,

	.max_pixclock	= KHZ2PICOS(148500),

	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,

	.enable		= whistler_hdmi_enable,
	.disable	= whistler_hdmi_disable,
};

static struct tegra_fb_data whistler_fb_data = {
	.win		= 0,
	.xres		= 800,
	.yres		= 480,
	.bits_per_pixel	= 32,
	.flags		= TEGRA_FB_FLIP_ON_PROBE,
};

static struct tegra_fb_data whistler_hdmi_fb_data = {
	.win		= 0,
	.xres		= 800,
	.yres		= 480,
	.bits_per_pixel	= 32,
	.flags		= TEGRA_FB_FLIP_ON_PROBE,
};


static struct tegra_dc_platform_data whistler_disp1_pdata = {
	.flags		= TEGRA_DC_FLAG_ENABLED,
	.default_out	= &whistler_disp1_out,
	.fb		= &whistler_fb_data,
};

static struct platform_device whistler_disp1_device = {
	.name		= "tegradc",
	.id		= 0,
	.resource	= whistler_disp1_resources,
	.num_resources	= ARRAY_SIZE(whistler_disp1_resources),
	.dev = {
		.platform_data = &whistler_disp1_pdata,
	},
};

static struct tegra_dc_platform_data whistler_disp2_pdata = {
	.flags		= 0,
	.default_out	= &whistler_disp2_out,
	.fb		= &whistler_hdmi_fb_data,
};

static struct platform_device whistler_disp2_device = {
	.name		= "tegradc",
	.id		= 1,
	.resource	= whistler_disp2_resources,
	.num_resources	= ARRAY_SIZE(whistler_disp2_resources),
	.dev = {
		.platform_data = &whistler_disp2_pdata,
	},
};
#endif

#if defined(CONFIG_TEGRA_NVMAP)
static struct nvmap_platform_carveout whistler_carveouts[] = {
	[0] = NVMAP_HEAP_CARVEOUT_IRAM_INIT,
	[1] = {
		.name		= "generic-0",
		.usage_mask	= NVMAP_HEAP_CARVEOUT_GENERIC,
		.base		= 0x18C00000,
		.size		= SZ_128M - 0xC00000,
		.buddy_size	= SZ_32K,
	},
};

static struct nvmap_platform_data whistler_nvmap_data = {
	.carveouts	= whistler_carveouts,
	.nr_carveouts	= ARRAY_SIZE(whistler_carveouts),
};

static struct platform_device whistler_nvmap_device = {
	.name	= "tegra-nvmap",
	.id	= -1,
	.dev	= {
		.platform_data = &whistler_nvmap_data,
	},
};
#endif

static struct platform_device *whistler_gfx_devices[] __initdata = {
#if defined(CONFIG_TEGRA_NVMAP)
	&whistler_nvmap_device,
#endif
	&whistler_disp1_backlight_device,
};

int __init whistler_panel_init(void)
{
	int err;
	struct resource __maybe_unused *res;
	struct platform_device *phost1x;

	gpio_request(whistler_hdmi_hpd, "hdmi_hpd");
	gpio_direction_input(whistler_hdmi_hpd);

#if defined(CONFIG_TEGRA_NVMAP)
	whistler_carveouts[1].base = tegra_carveout_start;
	whistler_carveouts[1].size = tegra_carveout_size;
#endif

	err = platform_add_devices(whistler_gfx_devices,
		ARRAY_SIZE(whistler_gfx_devices));

#ifdef CONFIG_TEGRA_GRHOST
	phost1x = tegra2_register_host1x_devices();
	if (!phost1x)
		return -EINVAL;
#endif

#if defined(CONFIG_TEGRA_GRHOST) && defined(CONFIG_TEGRA_DC)
	res = platform_get_resource_byname(&whistler_disp1_device,
					 IORESOURCE_MEM, "fbmem");
	res->start = tegra_fb_start;
	res->end = tegra_fb_start + tegra_fb_size - 1;
#endif

	/* Copy the bootloader fb to the fb. */
	tegra_move_framebuffer(tegra_fb_start, tegra_bootloader_fb_start,
		min(tegra_fb_size, tegra_bootloader_fb_size));

#if defined(CONFIG_TEGRA_GRHOST) && defined(CONFIG_TEGRA_DC)
	res = platform_get_resource_byname(&whistler_disp2_device,
					 IORESOURCE_MEM, "fbmem");
	res->start = tegra_fb2_start;
	res->end = tegra_fb2_start + tegra_fb2_size - 1;

	if (!err) {
		whistler_disp1_device.dev.parent = &phost1x->dev;
		err = platform_device_register(&whistler_disp1_device);
	}

	if (!err) {
		whistler_disp2_device.dev.parent = &phost1x->dev;
		err = platform_device_register(&whistler_disp2_device);
	}
#endif

	return err;
}

