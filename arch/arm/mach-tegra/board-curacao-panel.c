/*
 * arch/arm/mach-tegra/board-curacao-panel.c
 *
 * Copyright (c) 2011-2012, NVIDIA Corporation.
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
#include <linux/pwm_backlight.h>
#include <linux/nvhost.h>
#include <linux/nvmap.h>

#include <mach/irqs.h>
#include <mach/iomap.h>
#include <mach/dc.h>
#include <mach/fb.h>

#include <asm/mach-types.h>

#include "board.h"
#include "devices.h"
#include "gpio-names.h"
#include "tegra11_host1x_devices.h"

#define TEGRA_DSI_GANGED_MODE 0
#define PANEL_ENABLE	1

#if PANEL_ENABLE

#if TEGRA_DSI_GANGED_MODE
#define DSI_PANEL_218	1
#else
#define DSI_PANEL_218	0
#endif

#define DSI_PANEL_RESET	1
#define DC_CTRL_MODE	TEGRA_DC_OUT_CONTINUOUS_MODE

#if defined(CONFIG_TEGRA_GRHOST) && defined(CONFIG_TEGRA_DC)
static atomic_t sd_brightness = ATOMIC_INIT(255);
#endif

static int curacao_backlight_init(struct device *dev)
{
#if DSI_PANEL_218
	/* TODO: Enable backlight for dsi panel */
#endif
	return -ENODEV;
}

static void curacao_backlight_exit(struct device *dev)
{
#if DSI_PANEL_218
	/* TODO: Exit backlight for dsi panel */
#endif
}

static int curacao_backlight_notify(struct device *unused, int brightness)
{
#if DSI_PANEL_218
	/* TODO: Backlight notify for dsi panel */
#endif
	return -ENODEV;
}

static struct platform_pwm_backlight_data curacao_backlight_data = {
	.pwm_id		= 2,
	.max_brightness	= 255,
	.dft_brightness	= 224,
	.pwm_period_ns	= 5000000,
	.init		= curacao_backlight_init,
	.exit		= curacao_backlight_exit,
	.notify		= curacao_backlight_notify,
};

static struct platform_device curacao_backlight_device = {
	.name	= "pwm-backlight",
	.id	= -1,
	.dev	= {
		.platform_data = &curacao_backlight_data,
	},
};

#if defined(CONFIG_TEGRA_GRHOST) && defined(CONFIG_TEGRA_DC)
static int curacao_panel_enable(struct device *dev)
{
#if DSI_PANEL_218
	/* TODO: DSI panel enable */
#endif
	return -ENODEV;
}

static int curacao_panel_disable(void)
{
#if DSI_PANEL_218
	/* TODO: DSI panel disable */
#endif
	return -ENODEV;
}

#if TEGRA_DSI_GANGED_MODE
static struct resource curacao_disp1_resources[] = {
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
		.start	= 0,	/* Filled in by curacao_panel_init() */
		.end	= 0,	/* Filled in by curacao_panel_init() */
		.flags	= IORESOURCE_MEM,
	},

#if TEGRA_DSI_GANGED_MODE
	{
		.name	= "ganged_dsia_regs",
		.start	= TEGRA_DSI_BASE,
		.end	= TEGRA_DSI_BASE + TEGRA_DSI_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "ganged_dsib_regs",
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
};
#endif

static struct tegra_dc_sd_settings curacao_sd_settings = {
	.enable = 0, /* Normal mode operation */
	.use_auto_pwm = false,
	.hw_update_delay = 0,
	.bin_width = -1,
	.aggressiveness = 1,
	.use_vid_luma = false,
	.k_limit_enable = false,
	.sd_window_enable = false,
	.soft_clipping_enable = false,
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
			}
		},
	.sd_brightness = &sd_brightness,
};

static struct tegra_dc_mode curacao_panel_modes[] = {
#ifdef CONFIG_TEGRA_SIMULATION_PLATFORM
	{
		.pclk = 18000000,
		.h_ref_to_sync = 11,
		.v_ref_to_sync = 1,
		.h_sync_width = 16,
		.v_sync_width = 4,
		.h_back_porch = 16,
		.v_back_porch = 4,
		.h_active = 240,
		.v_active = 320,
		.h_front_porch = 16,
		.v_front_porch = 4,
	},
#else
#if !defined(CONFIG_TEGRA_SILICON_PLATFORM) && \
			TEGRA_DSI_GANGED_MODE
	{
		.pclk = 27000000,
		.h_ref_to_sync = 1,
		.v_ref_to_sync = 1,
		.h_sync_width = 64,
		.v_sync_width = 2,
		.h_back_porch = 16,
		.v_back_porch = 33,
		.h_active = 640,
		.v_active = 480,
		.h_front_porch = 132,
		.v_front_porch = 10,
	},
#else
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
#endif
};

static struct tegra_fb_data curacao_fb_data = {
	.win		= 0,
#if defined(CONFIG_TEGRA_SIMULATION_PLATFORM)
	.xres		= 240,
	.yres		= 320,
	.bits_per_pixel = 16,
	.flags		= 0,
#else
#if !defined(CONFIG_TEGRA_SILICON_PLATFORM) && \
		TEGRA_DSI_GANGED_MODE
	.xres		= 640,
	.yres		= 480,
#else
	.xres		= 864,
	.yres		= 480,
#endif
	.bits_per_pixel = 32,
	.flags		= TEGRA_FB_FLIP_ON_PROBE,
#endif
};

static struct tegra_dsi_cmd dsi_init_cmd[] = {
#if DSI_PANEL_218
	DSI_CMD_SHORT(0x05, 0x11, 0x00),
	DSI_DLY_MS(20),
#if (DC_CTRL_MODE & TEGRA_DC_OUT_ONE_SHOT_MODE)
	DSI_CMD_SHORT(0x15, 0x35, 0x00),
#endif
	DSI_CMD_SHORT(0x05, 0x29, 0x00),
	DSI_DLY_MS(20),
#endif
};

static struct tegra_dsi_cmd dsi_early_suspend_cmd[] = {
#if DSI_PANEL_218
	DSI_CMD_SHORT(0x05, 0x28, 0x00),
	DSI_DLY_MS(20),
#if (DC_CTRL_MODE & TEGRA_DC_OUT_ONE_SHOT_MODE)
	DSI_CMD_SHORT(0x05, 0x34, 0x00),
#endif
#endif
};

static struct tegra_dsi_cmd dsi_late_resume_cmd[] = {
#if DSI_PANEL_218
#if (DC_CTRL_MODE & TEGRA_DC_OUT_ONE_SHOT_MODE)
	DSI_CMD_SHORT(0x15, 0x35, 0x00),
#endif
	DSI_CMD_SHORT(0x05, 0x29, 0x00),
	DSI_DLY_MS(20),
#endif
};

static struct tegra_dsi_cmd dsi_suspend_cmd[] = {
#if DSI_PANEL_218
	DSI_CMD_SHORT(0x05, 0x28, 0x00),
	DSI_DLY_MS(20),
#if (DC_CTRL_MODE & TEGRA_DC_OUT_ONE_SHOT_MODE)
	DSI_CMD_SHORT(0x05, 0x34, 0x00),
#endif
	DSI_CMD_SHORT(0x05, 0x10, 0x00),
	DSI_DLY_MS(5),
#endif
};

static struct tegra_dsi_out curacao_dsi = {
#if DSI_PANEL_218
	.n_data_lanes = 2,
#else
#if TEGRA_DSI_GANGED_MODE
	.n_data_lanes = 8,
#else
	.n_data_lanes = 4,
#endif
#endif
	.pixel_format = TEGRA_DSI_PIXEL_FORMAT_24BIT_P,
	.refresh_rate = 60,
	.virtual_channel = TEGRA_DSI_VIRTUAL_CHANNEL_0,

	.dsi_instance = 0,
	.controller_vs = DSI_VS_1,

	.panel_reset = DSI_PANEL_RESET,
	.power_saving_suspend = true,

	.n_init_cmd = ARRAY_SIZE(dsi_init_cmd),
	.dsi_init_cmd = dsi_init_cmd,

	.n_early_suspend_cmd = ARRAY_SIZE(dsi_early_suspend_cmd),
	.dsi_early_suspend_cmd = dsi_early_suspend_cmd,

	.n_late_resume_cmd = ARRAY_SIZE(dsi_late_resume_cmd),
	.dsi_late_resume_cmd = dsi_late_resume_cmd,

	.n_suspend_cmd = ARRAY_SIZE(dsi_suspend_cmd),
	.dsi_suspend_cmd = dsi_suspend_cmd,

	.lp_cmd_mode_freq_khz = 20000,

#if TEGRA_DSI_GANGED_MODE
	.video_data_type = TEGRA_DSI_VIDEO_TYPE_VIDEO_MODE,
	.video_burst_mode = TEGRA_DSI_VIDEO_NONE_BURST_MODE,
	.ganged_type = TEGRA_DSI_GANGED_SYMMETRIC_LEFT_RIGHT,
#else
	.video_data_type = TEGRA_DSI_VIDEO_TYPE_COMMAND_MODE,
#endif

	/* TODO: Get the vender recommended freq */
	.lp_read_cmd_mode_freq_khz = 200000,

	.fpga_freq_khz = 162000,
};

#if TEGRA_DSI_GANGED_MODE
static struct tegra_dc_out curacao_disp1_out = {
	.sd_settings	= &curacao_sd_settings,

#ifdef CONFIG_TEGRA_SIMULATION_PLATFORM
	.type		= TEGRA_DC_OUT_RGB,
#else
	.type		= TEGRA_DC_OUT_DSI,
#endif
	.dsi		= &curacao_dsi,

	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,

	.flags		= DC_CTRL_MODE,

	.modes		= curacao_panel_modes,
	.n_modes	= ARRAY_SIZE(curacao_panel_modes),

	.enable		= curacao_panel_enable,
	.disable	= curacao_panel_disable,
};

static struct tegra_dc_platform_data curacao_disp1_pdata = {
	.flags		= TEGRA_DC_FLAG_ENABLED,
	.default_out	= &curacao_disp1_out,
	.fb		= &curacao_fb_data,
};

static struct platform_device curacao_disp1_device = {
	.name		= "tegradc",
	.id		= 0,
	.resource	= curacao_disp1_resources,
	.num_resources	= ARRAY_SIZE(curacao_disp1_resources),
	.dev = {
		.platform_data = &curacao_disp1_pdata,
	},
};
#endif

static struct resource curacao_disp2_resources[] = {
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
		.start	= 0,	/* Filled in by curacao_panel_init() */
		.end	= 0,	/* Filled in by curacao_panel_init() */
		.flags	= IORESOURCE_MEM,
	},
#if TEGRA_DSI_GANGED_MODE
	{
		.name	= "ganged_dsia_regs",
		.start	= TEGRA_DSI_BASE,
		.end	= TEGRA_DSI_BASE + TEGRA_DSI_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "ganged_dsib_regs",
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
};

static struct tegra_dc_out curacao_disp2_out = {
	.sd_settings	= &curacao_sd_settings,

#ifdef CONFIG_TEGRA_SIMULATION_PLATFORM
	.type		= TEGRA_DC_OUT_RGB,
#else
	.type		= TEGRA_DC_OUT_DSI,
#endif
	.dsi		= &curacao_dsi,

	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,

	.flags		= DC_CTRL_MODE,

	.modes		= curacao_panel_modes,
	.n_modes	= ARRAY_SIZE(curacao_panel_modes),

	.enable		= curacao_panel_enable,
	.disable	= curacao_panel_disable,
};

static struct tegra_dc_platform_data curacao_disp2_pdata = {
	.flags		= TEGRA_DC_FLAG_ENABLED,
	.default_out	= &curacao_disp2_out,
	.fb		= &curacao_fb_data,
};

static struct platform_device curacao_disp2_device = {
	.name		= "tegradc",
	.id		= 0,
	.resource	= curacao_disp2_resources,
	.num_resources	= ARRAY_SIZE(curacao_disp2_resources),
	.dev = {
		.platform_data = &curacao_disp2_pdata,
	},
};
#endif

static struct nvmap_platform_carveout curacao_carveouts[] = {
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
		.base		= 0,	/* Filled in by curacao_panel_init() */
		.size		= 0,	/* Filled in by curacao_panel_init() */
		.buddy_size	= SZ_32K,
	},
	[2] = {
		.name		= "vpr",
		.usage_mask	= NVMAP_HEAP_CARVEOUT_VPR,
		.base		= 0,	/* Filled in by curacao_panel_init() */
		.size		= 0,	/* Filled in by curacao_panel_init() */
		.buddy_size	= SZ_32K,
	},
};

static struct nvmap_platform_data curacao_nvmap_data = {
	.carveouts	= curacao_carveouts,
	.nr_carveouts	= ARRAY_SIZE(curacao_carveouts),
};

static struct platform_device curacao_nvmap_device = {
	.name	= "tegra-nvmap",
	.id	= -1,
	.dev	= {
		.platform_data = &curacao_nvmap_data,
	},
};

static struct platform_device *curacao_gfx_devices[] __initdata = {
	&curacao_nvmap_device,
	&tegra_pwfm2_device,
	&curacao_backlight_device,
};

int __init curacao_panel_init(void)
{
	int err;
#if defined(CONFIG_TEGRA_GRHOST) && defined(CONFIG_TEGRA_DC)
	struct resource *res;
#endif
#if defined(CONFIG_TEGRA_GRHOST)
	struct platform_device *phost1x;
#endif
	curacao_carveouts[1].base = tegra_carveout_start;
	curacao_carveouts[1].size = tegra_carveout_size;
	curacao_carveouts[2].base = tegra_vpr_start;
	curacao_carveouts[2].size = tegra_vpr_size;

	err = platform_add_devices(curacao_gfx_devices,
		ARRAY_SIZE(curacao_gfx_devices));

#ifdef CONFIG_TEGRA_GRHOST
	phost1x = tegra11_register_host1x_devices();
	if (!phost1x)
		return -EINVAL;
#endif

#if TEGRA_DSI_GANGED_MODE
#if defined(CONFIG_TEGRA_GRHOST) && defined(CONFIG_TEGRA_DC)
	res = platform_get_resource_byname(&curacao_disp1_device,
					 IORESOURCE_MEM, "fbmem");
	res->start = tegra_fb_start;
	res->end = tegra_fb_start + tegra_fb_size - 1;

	if (!err) {
		curacao_disp1_device.dev.parent = &phost1x->dev;
		err = platform_device_register(&curacao_disp1_device);
	}
#endif
#else
#if defined(CONFIG_TEGRA_GRHOST) && defined(CONFIG_TEGRA_DC)
	res = platform_get_resource_byname(&curacao_disp2_device,
					 IORESOURCE_MEM, "fbmem");
	res->start = tegra_fb_start;
	res->end = tegra_fb_start + tegra_fb_size - 1;

	if (!err) {
		curacao_disp2_device.dev.parent = &phost1x->dev;
		err = platform_device_register(&curacao_disp2_device);
	}
#endif
#endif

#if defined(CONFIG_TEGRA_GRHOST) && defined(CONFIG_TEGRA_NVAVP)
	if (!err) {
		nvavp_device.dev.parent = &phost1x->dev;
		err = platform_device_register(&nvavp_device);
	}
#endif
	return err;
}
#else
int __init curacao_panel_init(void)
{
	return -ENODEV;
}
#endif
