/*
 * arch/arm/mach-tegra/board-harmony-panel.c
 *
 * Copyright (c) 2010-2012, NVIDIA Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/delay.h>
#include <linux/resource.h>
#include <linux/platform_device.h>
#include <asm/mach-types.h>
#include <linux/nvhost.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/pwm_backlight.h>

#include <mach/dc.h>
#include <mach/irqs.h>
#include <mach/iomap.h>
#include <linux/nvmap.h>
#include <mach/tegra_fb.h>
#include <mach/fb.h>
#include <mach/gpio-tegra.h>

#include "devices.h"
#include "gpio-names.h"
#include "board.h"
#include "tegra2_host1x_devices.h"

#define harmony_bl_enb		TEGRA_GPIO_PB5
#define harmony_lvds_shutdown	TEGRA_GPIO_PB2
#define harmony_en_vdd_pnl	TEGRA_GPIO_PC6
#define harmony_bl_vdd		TEGRA_GPIO_PW0
#define harmony_bl_pwm		TEGRA_GPIO_PB4
#define harmony_hdmi_hpd	TEGRA_GPIO_PN7

/* panel power on sequence timing */
#define harmony_pnl_to_lvds_ms	0
#define harmony_lvds_to_bl_ms	200

static int harmony_backlight_init(struct device *dev)
{
	int ret;

	ret = gpio_request(harmony_bl_enb, "backlight_enb");
	if (ret < 0)
		return ret;

	ret = gpio_direction_output(harmony_bl_enb, 1);
	if (ret < 0)
		gpio_free(harmony_bl_enb);

	return ret;
}

static void harmony_backlight_exit(struct device *dev)
{
	gpio_set_value(harmony_bl_enb, 0);
	gpio_free(harmony_bl_enb);
}

static int harmony_backlight_notify(struct device *unused, int brightness)
{
	gpio_set_value(harmony_en_vdd_pnl, !!brightness);
	gpio_set_value(harmony_lvds_shutdown, !!brightness);
	gpio_set_value(harmony_bl_enb, !!brightness);
	return brightness;
}

static int harmony_disp1_check_fb(struct device *dev, struct fb_info *info);

static struct platform_pwm_backlight_data harmony_backlight_data = {
	.pwm_id		= 0,
	.max_brightness	= 255,
	.dft_brightness	= 224,
	.pwm_period_ns	= 5000000,
	.init		= harmony_backlight_init,
	.exit		= harmony_backlight_exit,
	.notify		= harmony_backlight_notify,
	/* Only toggle backlight on fb blank notifications for disp1 */
	.check_fb	= harmony_disp1_check_fb,
};

static struct platform_device harmony_backlight_device = {
	.name	= "pwm-backlight",
	.id	= -1,
	.dev	= {
		.platform_data = &harmony_backlight_data,
	},
};

static int harmony_panel_enable(struct device *dev)
{
	gpio_set_value(harmony_en_vdd_pnl, 1);
	mdelay(harmony_pnl_to_lvds_ms);
	gpio_set_value(harmony_lvds_shutdown, 1);
	mdelay(harmony_lvds_to_bl_ms);
	return 0;
}

static int harmony_panel_disable(void)
{
	gpio_set_value(harmony_lvds_shutdown, 0);
	gpio_set_value(harmony_en_vdd_pnl, 0);
	return 0;
}

static int harmony_set_hdmi_power(bool enable)
{
	static struct {
		struct regulator *regulator;
		const char *name;
	} regs[] = {
		{ .name = "avdd_hdmi" },
		{ .name = "avdd_hdmi_pll" },
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(regs); i++) {
		if (!regs[i].regulator) {
			regs[i].regulator = regulator_get(NULL, regs[i].name);

			if (IS_ERR(regs[i].regulator)) {
				int ret = PTR_ERR(regs[i].regulator);
				regs[i].regulator = NULL;
				return ret;
			}
		}

		if (enable)
			regulator_enable(regs[i].regulator);
		else
			regulator_disable(regs[i].regulator);
	}

	return 0;
}

static int harmony_hdmi_enable(struct device *dev)
{
	return harmony_set_hdmi_power(true);
}

static int harmony_hdmi_disable(void)
{
	return harmony_set_hdmi_power(false);
}

static struct resource harmony_disp1_resources[] = {
	{
		.name = "irq",
		.start  = INT_DISPLAY_GENERAL,
		.end    = INT_DISPLAY_GENERAL,
		.flags  = IORESOURCE_IRQ,
	},
	{
		.name = "regs",
		.start	= TEGRA_DISPLAY_BASE,
		.end	= TEGRA_DISPLAY_BASE + TEGRA_DISPLAY_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name = "fbmem",
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource harmony_disp2_resources[] = {
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

static struct tegra_dc_mode harmony_panel_modes[] = {
	{
		.pclk = 42430000,
		.h_ref_to_sync = 4,
		.v_ref_to_sync = 2,
		.h_sync_width = 136,
		.v_sync_width = 4,
		.h_back_porch = 138,
		.v_back_porch = 21,
		.h_active = 1024,
		.v_active = 600,
		.h_front_porch = 34,
		.v_front_porch = 4,
	},
};

static struct tegra_fb_data harmony_fb_data = {
	.win		= 0,
	.xres		= 1024,
	.yres		= 600,
	.bits_per_pixel	= 32,
	.flags		= TEGRA_FB_FLIP_ON_PROBE,
};

static struct tegra_fb_data harmony_hdmi_fb_data = {
	.win		= 0,
	.xres		= 1280,
	.yres		= 720,
	.bits_per_pixel	= 16,
};

static struct tegra_dc_out harmony_disp1_out = {
	.type		= TEGRA_DC_OUT_RGB,

	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,
	.depth		= 18,
	.dither		= TEGRA_DC_ORDERED_DITHER,

	.modes		= harmony_panel_modes,
	.n_modes	= ARRAY_SIZE(harmony_panel_modes),

	.enable		= harmony_panel_enable,
	.disable	= harmony_panel_disable,
};

static struct tegra_dc_out harmony_disp2_out = {
	.type		= TEGRA_DC_OUT_HDMI,
	.flags		= TEGRA_DC_OUT_HOTPLUG_HIGH,

	.dcc_bus	= 1,
	.hotplug_gpio	= harmony_hdmi_hpd,

	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,

	.enable		= harmony_hdmi_enable,
	.disable	= harmony_hdmi_disable,
};

static struct tegra_dc_platform_data harmony_disp1_pdata = {
	.flags		= TEGRA_DC_FLAG_ENABLED,
	.default_out	= &harmony_disp1_out,
	.fb		= &harmony_fb_data,
};

static struct tegra_dc_platform_data harmony_disp2_pdata = {
	.flags		= 0,
	.default_out	= &harmony_disp2_out,
	.fb		= &harmony_hdmi_fb_data,
};

static struct platform_device harmony_disp1_device = {
	.name		= "tegradc",
	.id		= 0,
	.resource	= harmony_disp1_resources,
	.num_resources	= ARRAY_SIZE(harmony_disp1_resources),
	.dev = {
		.platform_data = &harmony_disp1_pdata,
	},
};

static int harmony_disp1_check_fb(struct device *dev, struct fb_info *info)
{
	return info->device == &harmony_disp1_device.dev;
}

static struct platform_device harmony_disp2_device = {
	.name		= "tegradc",
	.id		= 1,
	.resource	= harmony_disp2_resources,
	.num_resources	= ARRAY_SIZE(harmony_disp2_resources),
	.dev = {
		.platform_data = &harmony_disp2_pdata,
	},
};

#if defined(CONFIG_TEGRA_NVMAP)
static struct nvmap_platform_carveout harmony_carveouts[] = {
	[0] = NVMAP_HEAP_CARVEOUT_IRAM_INIT,
	[1] = {
		.name		= "generic-0",
		.usage_mask	= NVMAP_HEAP_CARVEOUT_GENERIC,
		.buddy_size	= SZ_32K,
	},
};

static struct nvmap_platform_data harmony_nvmap_data = {
	.carveouts	= harmony_carveouts,
	.nr_carveouts	= ARRAY_SIZE(harmony_carveouts),
};

static struct platform_device harmony_nvmap_device = {
	.name	= "tegra-nvmap",
	.id	= -1,
	.dev	= {
		.platform_data = &harmony_nvmap_data,
	},
};
#endif

static struct platform_device *harmony_gfx_devices[] __initdata = {
#if defined(CONFIG_TEGRA_NVMAP)
	&harmony_nvmap_device,
#endif
	&tegra_pwfm0_device,
	&harmony_backlight_device,
};

int __init harmony_panel_init(void)
{
	int err;
	struct resource *res;
	struct platform_device *phost1x;

	gpio_request(harmony_en_vdd_pnl, "en_vdd_pnl");
	gpio_direction_output(harmony_en_vdd_pnl, 1);

	gpio_request(harmony_bl_vdd, "bl_vdd");
	gpio_direction_output(harmony_bl_vdd, 1);

	gpio_request(harmony_lvds_shutdown, "lvds_shdn");
	gpio_direction_output(harmony_lvds_shutdown, 1);

	gpio_request(harmony_hdmi_hpd, "hdmi_hpd");
	gpio_direction_input(harmony_hdmi_hpd);

#if defined(CONFIG_TEGRA_NVMAP)
	harmony_carveouts[1].base = tegra_carveout_start;
	harmony_carveouts[1].size = tegra_carveout_size;
#endif

	err = platform_add_devices(harmony_gfx_devices,
		ARRAY_SIZE(harmony_gfx_devices));
	if (err)
		return err;

#ifdef CONFIG_TEGRA_GRHOST
	phost1x = tegra2_register_host1x_devices();
	if (!phost1x)
		return -EINVAL;
#endif

	res = platform_get_resource_byname(&harmony_disp1_device,
		IORESOURCE_MEM, "fbmem");
	if (res) {
		res->start = tegra_fb_start;
		res->end = tegra_fb_start + tegra_fb_size - 1;
	}

	res = platform_get_resource_byname(&harmony_disp2_device,
		IORESOURCE_MEM, "fbmem");
	if (res) {
		res->start = tegra_fb2_start;
		res->end = tegra_fb2_start + tegra_fb2_size - 1;
	}

	/* Copy the bootloader fb to the fb. */
	if (tegra_bootloader_fb_start)
		tegra_move_framebuffer(tegra_fb_start,
			tegra_bootloader_fb_start,
			min(tegra_fb_size, tegra_bootloader_fb_size));

	harmony_disp1_device.dev.parent = &phost1x->dev;
	err = platform_device_register(&harmony_disp1_device);
	if (err)
		return err;

	harmony_disp2_device.dev.parent = &phost1x->dev;
	err = platform_device_register(&harmony_disp2_device);
	if (err)
		return err;

	return 0;
}

