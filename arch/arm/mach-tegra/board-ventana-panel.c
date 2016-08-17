/*
 * arch/arm/mach-tegra/board-ventana-panel.c
 *
 * Copyright (c) 2010-2012 NVIDIA Corporation.
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
#include <mach/gpio-tegra.h>
#include <mach/irqs.h>
#include <mach/iomap.h>
#include <mach/dc.h>
#include <mach/fb.h>

#include "devices.h"
#include "gpio-names.h"
#include "board.h"
#include "tegra2_host1x_devices.h"

#define ventana_bl_enb		TEGRA_GPIO_PD4
#define ventana_lvds_shutdown	TEGRA_GPIO_PB2
#define ventana_hdmi_hpd	TEGRA_GPIO_PN7
#define ventana_hdmi_enb	TEGRA_GPIO_PV5

/*panel power on sequence timing*/
#define ventana_pnl_to_lvds_ms	0
#define ventana_lvds_to_bl_ms	200

static struct regulator *pnl_pwr;

#ifdef CONFIG_TEGRA_DC
static struct regulator *ventana_hdmi_reg = NULL;
static struct regulator *ventana_hdmi_pll = NULL;
#endif

static int ventana_backlight_init(struct device *dev) {
	int ret;

	ret = gpio_request(ventana_bl_enb, "backlight_enb");
	if (ret < 0)
		return ret;

	ret = gpio_direction_output(ventana_bl_enb, 1);
	if (ret < 0)
		gpio_free(ventana_bl_enb);

	return ret;
};

static void ventana_backlight_exit(struct device *dev) {
	gpio_set_value(ventana_bl_enb, 0);
	gpio_free(ventana_bl_enb);
}

static int ventana_backlight_notify(struct device *unused, int brightness)
{
	gpio_set_value(ventana_bl_enb, !!brightness);
	return brightness;
}

static int ventana_disp1_check_fb(struct device *dev, struct fb_info *info);

static struct platform_pwm_backlight_data ventana_backlight_data = {
	.pwm_id		= 2,
	.max_brightness	= 255,
	.dft_brightness	= 224,
	.pwm_period_ns	= 5000000,
	.init		= ventana_backlight_init,
	.exit		= ventana_backlight_exit,
	.notify		= ventana_backlight_notify,
	/* Only toggle backlight on fb blank notifications for disp1 */
	.check_fb   = ventana_disp1_check_fb,
};

static struct platform_device ventana_backlight_device = {
	.name	= "pwm-backlight",
	.id	= -1,
	.dev	= {
		.platform_data = &ventana_backlight_data,
	},
};

#ifdef CONFIG_TEGRA_DC
static int ventana_panel_enable(struct device *dev)
{
	struct regulator *reg = regulator_get(dev, "vdd_ldo4");

	if (!reg) {
		regulator_enable(reg);
		regulator_put(reg);
	}

	if (pnl_pwr == NULL) {
		pnl_pwr = regulator_get(dev, "pnl_pwr");
		if (WARN_ON(IS_ERR(pnl_pwr)))
			pr_err("%s: couldn't get regulator pnl_pwr: %ld\n",
				__func__, PTR_ERR(pnl_pwr));
		else
			regulator_enable(pnl_pwr);
	} else {
		regulator_enable(pnl_pwr);
	}

	mdelay(ventana_pnl_to_lvds_ms);
	gpio_set_value(ventana_lvds_shutdown, 1);
	mdelay(ventana_lvds_to_bl_ms);
	return 0;
}

static int ventana_panel_disable(void)
{
	gpio_set_value(ventana_lvds_shutdown, 0);
	regulator_disable(pnl_pwr);
	return 0;
}

static int ventana_hdmi_enable(struct device *dev)
{
	if (!ventana_hdmi_reg) {
		ventana_hdmi_reg = regulator_get(dev, "avdd_hdmi"); /* LD07 */
		if (IS_ERR_OR_NULL(ventana_hdmi_reg)) {
			pr_err("hdmi: couldn't get regulator avdd_hdmi\n");
			ventana_hdmi_reg = NULL;
			return PTR_ERR(ventana_hdmi_reg);
		}
	}
	regulator_enable(ventana_hdmi_reg);

	if (!ventana_hdmi_pll) {
		ventana_hdmi_pll =
			regulator_get(dev, "avdd_hdmi_pll"); /* LD08 */
		if (IS_ERR_OR_NULL(ventana_hdmi_pll)) {
			pr_err("hdmi: couldn't get regulator avdd_hdmi_pll\n");
			ventana_hdmi_pll = NULL;
			regulator_disable(ventana_hdmi_reg);
			ventana_hdmi_reg = NULL;
			return PTR_ERR(ventana_hdmi_pll);
		}
	}
	regulator_enable(ventana_hdmi_pll);
	return 0;
}

static int ventana_hdmi_disable(void)
{
	regulator_disable(ventana_hdmi_reg);
	regulator_disable(ventana_hdmi_pll);
	return 0;
}

static struct resource ventana_disp1_resources[] = {
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

static struct resource ventana_disp2_resources[] = {
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

static struct tegra_dc_mode ventana_panel_modes[] = {
	{
		.pclk = 72072000,
		.h_ref_to_sync = 11,
		.v_ref_to_sync = 1,
		.h_sync_width = 58,
		.v_sync_width = 4,
		.h_back_porch = 58,
		.v_back_porch = 4,
		.h_active = 1366,
		.v_active = 768,
		.h_front_porch = 58,
		.v_front_porch = 4,
	},
};

static struct tegra_fb_data ventana_fb_data = {
	.win		= 0,
	.xres		= 1366,
	.yres		= 768,
	.bits_per_pixel	= 32,
	.flags		= TEGRA_FB_FLIP_ON_PROBE,
};

static struct tegra_fb_data ventana_hdmi_fb_data = {
	.win		= 0,
	.xres		= 640,
	.yres		= 480,
	.bits_per_pixel	= 32,
	.flags		= TEGRA_FB_FLIP_ON_PROBE,
};

static struct tegra_dc_out ventana_disp1_out = {
	.type		= TEGRA_DC_OUT_RGB,

	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,
	.depth		= 18,
	.dither		= TEGRA_DC_ORDERED_DITHER,

	.modes	 	= ventana_panel_modes,
	.n_modes 	= ARRAY_SIZE(ventana_panel_modes),

	.enable		= ventana_panel_enable,
	.disable	= ventana_panel_disable,
};

static struct tegra_dc_out ventana_disp2_out = {
	.type		= TEGRA_DC_OUT_HDMI,
	.flags		= TEGRA_DC_OUT_HOTPLUG_HIGH,

	.dcc_bus	= 1,
	.hotplug_gpio	= ventana_hdmi_hpd,

	.max_pixclock	= KHZ2PICOS(148500),

	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,

	.enable		= ventana_hdmi_enable,
	.disable	= ventana_hdmi_disable,
};

static struct tegra_dc_platform_data ventana_disp1_pdata = {
	.flags		= TEGRA_DC_FLAG_ENABLED,
	.default_out	= &ventana_disp1_out,
	.fb		= &ventana_fb_data,
};

static struct tegra_dc_platform_data ventana_disp2_pdata = {
	.flags		= TEGRA_DC_FLAG_ENABLED,
	.default_out	= &ventana_disp2_out,
	.fb		= &ventana_hdmi_fb_data,
};

static struct platform_device ventana_disp1_device = {
	.name		= "tegradc",
	.id		= 0,
	.resource	= ventana_disp1_resources,
	.num_resources	= ARRAY_SIZE(ventana_disp1_resources),
	.dev = {
		.platform_data = &ventana_disp1_pdata,
	},
};

static int ventana_disp1_check_fb(struct device *dev, struct fb_info *info)
{
	return info->device == &ventana_disp1_device.dev;
}

static struct platform_device ventana_disp2_device = {
	.name		= "tegradc",
	.id		= 1,
	.resource	= ventana_disp2_resources,
	.num_resources	= ARRAY_SIZE(ventana_disp2_resources),
	.dev = {
		.platform_data = &ventana_disp2_pdata,
	},
};
#else
static int ventana_disp1_check_fb(struct device *dev, struct fb_info *info)
{
	return 0;
}
#endif

#if defined(CONFIG_TEGRA_NVMAP)
static struct nvmap_platform_carveout ventana_carveouts[] = {
	[0] = NVMAP_HEAP_CARVEOUT_IRAM_INIT,
	[1] = {
		.name		= "generic-0",
		.usage_mask	= NVMAP_HEAP_CARVEOUT_GENERIC,
		.buddy_size	= SZ_32K,
	},
};

static struct nvmap_platform_data ventana_nvmap_data = {
	.carveouts	= ventana_carveouts,
	.nr_carveouts	= ARRAY_SIZE(ventana_carveouts),
};

static struct platform_device ventana_nvmap_device = {
	.name	= "tegra-nvmap",
	.id	= -1,
	.dev	= {
		.platform_data = &ventana_nvmap_data,
	},
};
#endif

static struct platform_device *ventana_gfx_devices[] __initdata = {
#if defined(CONFIG_TEGRA_NVMAP)
	&ventana_nvmap_device,
#endif
	&tegra_pwfm2_device,
};
static struct platform_device *ventana_backlight_devices[] __initdata = {
	&ventana_backlight_device,
};

int __init ventana_panel_init(void)
{
	int err;
	struct resource __maybe_unused *res;
	struct platform_device *phost1x;

	gpio_request(ventana_lvds_shutdown, "lvds_shdn");
	gpio_direction_output(ventana_lvds_shutdown, 1);

	gpio_request(ventana_hdmi_enb, "hdmi_5v_en");
	gpio_direction_output(ventana_hdmi_enb, 1);

	gpio_request(ventana_hdmi_hpd, "hdmi_hpd");
	gpio_direction_input(ventana_hdmi_hpd);

#if defined(CONFIG_TEGRA_NVMAP)
	ventana_carveouts[1].base = tegra_carveout_start;
	ventana_carveouts[1].size = tegra_carveout_size;
#endif

	err = platform_add_devices(ventana_gfx_devices,
		ARRAY_SIZE(ventana_gfx_devices));

#ifdef CONFIG_TEGRA_GRHOST
	phost1x = tegra2_register_host1x_devices();
	if (!phost1x)
		return -EINVAL;
#endif

#if defined(CONFIG_TEGRA_GRHOST) && defined(CONFIG_TEGRA_DC)
	res = platform_get_resource_byname(&ventana_disp1_device,
		IORESOURCE_MEM, "fbmem");
	res->start = tegra_fb_start;
	res->end = tegra_fb_start + tegra_fb_size - 1;

	res = platform_get_resource_byname(&ventana_disp2_device,
		IORESOURCE_MEM, "fbmem");
	res->start = tegra_fb2_start;
	res->end = tegra_fb2_start + tegra_fb2_size - 1;
#endif

	/* Copy the bootloader fb to the fb. */
	tegra_move_framebuffer(tegra_fb_start, tegra_bootloader_fb_start,
		min(tegra_fb_size, tegra_bootloader_fb_size));

	/*
	 * If the bootloader fb2 is valid, copy it to the fb2, or else
	 * clear fb2 to avoid garbage on dispaly2.
	 */
	if (tegra_bootloader_fb2_size)
		tegra_move_framebuffer(tegra_fb2_start,
			tegra_bootloader_fb2_start,
			min(tegra_fb2_size, tegra_bootloader_fb2_size));
	else
		tegra_clear_framebuffer(tegra_fb2_start, tegra_fb2_size);


#if defined(CONFIG_TEGRA_GRHOST) && defined(CONFIG_TEGRA_DC)
	if (!err) {
		ventana_disp1_device.dev.parent = &phost1x->dev;
		err = platform_device_register(&ventana_disp1_device);
	}

	if (!err) {
		ventana_disp2_device.dev.parent = &phost1x->dev;
		err = platform_device_register(&ventana_disp2_device);
	}
#endif

	err = platform_add_devices(ventana_backlight_devices,
		ARRAY_SIZE(ventana_backlight_devices));

	return err;
}

