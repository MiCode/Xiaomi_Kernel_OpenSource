/*
 * arch/arm/mach-tegra/board-roth-panel.c
 *
 * Copyright (c) 2011-2013, NVIDIA Corporation. All rights reserved.
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
#include <mach/iomap.h>
#include <mach/dc.h>
#include <mach/pinmux.h>
#include <mach/pinmux-t11.h>
#include <asm/mach-types.h>

#include "board.h"
#include "board-panel.h"
#include "devices.h"
#include "gpio-names.h"
#include "tegra11_host1x_devices.h"

struct platform_device * __init roth_host1x_init(void)
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

#define IS_EXTERNAL_PWM		1

#define DSI_PANEL_RESET		1

#define DSI_PANEL_RST_GPIO	TEGRA_GPIO_PH3
#define DSI_PANEL_BL_PWM	TEGRA_GPIO_PH1

#define DC_CTRL_MODE	TEGRA_DC_OUT_CONTINUOUS_MODE

/* HDMI Hotplug detection pin */
#define roth_hdmi_hpd	TEGRA_GPIO_PN7

static bool reg_requested;
static bool gpio_requested;

static struct regulator *vdd_lcd_s_1v8;
static struct regulator *vdd_lcd_bl;
static struct regulator *vdd_lcd_bl_en;
static struct regulator *avdd_lcd_3v0_2v8;

static struct regulator *roth_hdmi_reg;
static struct regulator *roth_hdmi_pll;
static struct regulator *roth_hdmi_vddio;

static struct resource roth_disp1_resources[] = {
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
		.start	= 0, /* Filled in by roth_panel_init() */
		.end	= 0, /* Filled in by roth_panel_init() */
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "dsi_regs",
		.start	= TEGRA_DSI_BASE,
		.end	= TEGRA_DSI_BASE + TEGRA_DSI_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "mipi_cal",
		.start	= TEGRA_MIPI_CAL_BASE,
		.end	= TEGRA_MIPI_CAL_BASE + TEGRA_MIPI_CAL_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource roth_disp2_resources[] = {
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
		.start	= 0, /* Filled in by roth_panel_init() */
		.end	= 0, /* Filled in by roth_panel_init() */
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "hdmi_regs",
		.start	= TEGRA_HDMI_BASE,
		.end	= TEGRA_HDMI_BASE + TEGRA_HDMI_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static u8 panel_dsi_config[] = {0xe0, 0x43, 0x0, 0x80, 0x0, 0x0};
static u8 panel_disp_ctrl1[] = {0xb5, 0x34, 0x20, 0x40, 0x0, 0x20};
static u8 panel_disp_ctrl2[] = {0xb6, 0x04, 0x74, 0x0f, 0x16, 0x13};
static u8 panel_internal_clk[] = {0xc0, 0x01, 0x08};
static u8 panel_pwr_ctrl3[] = {
	0xc3, 0x0, 0x09, 0x10, 0x02, 0x0, 0x66, 0x20, 0x13, 0x0};
static u8 panel_pwr_ctrl4[] = {0xc4, 0x23, 0x24, 0x17, 0x17, 0x59};
static u8 panel_positive_gamma_red[] = {
	0xd0, 0x21, 0x13, 0x67, 0x37, 0x0c, 0x06, 0x62, 0x23, 0x03};
static u8 panel_negetive_gamma_red[] = {
	0xd1, 0x32, 0x13, 0x66, 0x37, 0x02, 0x06, 0x62, 0x23, 0x03};
static u8 panel_positive_gamma_green[] = {
	0xd2, 0x41, 0x14, 0x56, 0x37, 0x0c, 0x06, 0x62, 0x23, 0x03};
static u8 panel_negetive_gamma_green[] = {
	0xd3, 0x52, 0x14, 0x55, 0x37, 0x02, 0x06, 0x62, 0x23, 0x03};
static u8 panel_positive_gamma_blue[] = {
	0xd4, 0x41, 0x14, 0x56, 0x37, 0x0c, 0x06, 0x62, 0x23, 0x03};
static u8 panel_negetive_gamma_blue[] = {
	0xd5, 0x52, 0x14, 0x55, 0x37, 0x02, 0x06, 0x62, 0x23, 0x03};
static u8 panel_ce2[] = {0x71, 0x0, 0x0, 0x01, 0x01};
static u8 panel_ce3[] = {0x72, 0x01, 0x0e};
static u8 panel_ce4[] = {0x73, 0x34, 0x52, 0x0};
static u8 panel_ce5[] = {0x74, 0x05, 0x0, 0x06};
static u8 panel_ce6[] = {0x75, 0x03, 0x0, 0x07};
static u8 panel_ce7[] = {0x76, 0x07, 0x0, 0x06};
static u8 panel_ce8[] = {0x77, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f};
static u8 panel_ce9[] = {0x78, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40};
static u8 panel_ce10[] = {
	0x79, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40};
static u8 panel_ce11[] = {0x7a, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
static u8 panel_ce12[] = {0x7b, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
static u8 panel_ce13[] = {0x7c, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};

static struct tegra_dsi_cmd dsi_init_cmd[] = {
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_dsi_config),

	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_disp_ctrl1),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_disp_ctrl2),

	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_internal_clk),

	/*  panel power control 1 */
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0xc1, 0x0),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_pwr_ctrl3),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_pwr_ctrl4),

	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_positive_gamma_red),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_negetive_gamma_red),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_positive_gamma_green),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_negetive_gamma_green),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_positive_gamma_blue),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_negetive_gamma_blue),

	DSI_CMD_SHORT(DSI_DCS_WRITE_1_PARAM, DSI_DCS_SET_ADDR_MODE, 0x08),

	/* panel OTP 2 */
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0xf9, 0x0),

	/* panel CE 1 */
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x70, 0x0),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_ce2),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_ce3),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_ce4),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_ce5),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_ce6),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_ce7),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_ce8),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_ce9),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_ce10),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_ce11),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_ce12),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_ce13),

	/* panel power control 2 */
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0xc2, 0x02),
	DSI_DLY_MS(20),

	/* panel power control 2 */
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0xc2, 0x06),
	DSI_DLY_MS(20),

	/* panel power control 2 */
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0xc2, 0x4e),
	DSI_DLY_MS(100),

	DSI_CMD_SHORT(DSI_DCS_WRITE_0_PARAM, DSI_DCS_EXIT_SLEEP_MODE, 0x0),
	DSI_DLY_MS(20),

	/* panel OTP 2 */
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0xf9, 0x80),
	DSI_DLY_MS(20),

	DSI_CMD_SHORT(DSI_DCS_WRITE_0_PARAM, DSI_DCS_SET_DISPLAY_ON, 0x0),
};

static struct tegra_dsi_out roth_dsi = {
	.n_data_lanes = 4,
	.controller_vs = DSI_VS_1,
	.pixel_format = TEGRA_DSI_PIXEL_FORMAT_24BIT_P,
	.refresh_rate = 60,
	.virtual_channel = TEGRA_DSI_VIRTUAL_CHANNEL_0,

	.dsi_instance = DSI_INSTANCE_0,

	.panel_reset = DSI_PANEL_RESET,
	.power_saving_suspend = true,
	.video_data_type = TEGRA_DSI_VIDEO_TYPE_VIDEO_MODE,
	.video_clock_mode = TEGRA_DSI_VIDEO_CLOCK_CONTINUOUS,
	.video_burst_mode = TEGRA_DSI_VIDEO_NONE_BURST_MODE_WITH_SYNC_END,
	.dsi_init_cmd = dsi_init_cmd,
	.n_init_cmd = ARRAY_SIZE(dsi_init_cmd),
};

static int roth_dsi_regulator_get(struct device *dev)
{
	int err = 0;

	if (reg_requested)
		return 0;

	avdd_lcd_3v0_2v8 = regulator_get(dev, "avdd_lcd");
	if (IS_ERR_OR_NULL(avdd_lcd_3v0_2v8)) {
		pr_err("avdd_lcd regulator get failed\n");
		err = PTR_ERR(avdd_lcd_3v0_2v8);
		avdd_lcd_3v0_2v8 = NULL;
		goto fail;
	}
	vdd_lcd_s_1v8 = regulator_get(dev, "dvdd_lcd");
	if (IS_ERR_OR_NULL(vdd_lcd_s_1v8)) {
		pr_err("vdd_lcd_1v8_s regulator get failed\n");
		err = PTR_ERR(vdd_lcd_s_1v8);
		vdd_lcd_s_1v8 = NULL;
		goto fail;
	}

	if (machine_is_dalmore()) {
		vdd_lcd_bl = regulator_get(dev, "vdd_lcd_bl");
		if (IS_ERR_OR_NULL(vdd_lcd_bl)) {
			pr_err("vdd_lcd_bl regulator get failed\n");
			err = PTR_ERR(vdd_lcd_bl);
			vdd_lcd_bl = NULL;
			goto fail;
		}
	}

	vdd_lcd_bl_en = regulator_get(dev, "vdd_lcd_bl_en");
	if (IS_ERR_OR_NULL(vdd_lcd_bl_en)) {
		pr_err("vdd_lcd_bl_en regulator get failed\n");
		err = PTR_ERR(vdd_lcd_bl_en);
		vdd_lcd_bl_en = NULL;
		goto fail;
	}
	reg_requested = true;
	return 0;
fail:
	return err;
}

static int roth_dsi_gpio_get(void)
{
	int err = 0;

	if (gpio_requested)
		return 0;

	err = gpio_request(DSI_PANEL_RST_GPIO, "panel rst");
	if (err < 0) {
		pr_err("panel reset gpio request failed\n");
		goto fail;
	}

	gpio_requested = true;
	return 0;
fail:
	return err;
}

static struct tegra_dc_out roth_disp1_out;

static int roth_dsi_panel_enable(struct device *dev)
{
	int err = 0;

	err = roth_dsi_regulator_get(dev);
	if (err < 0) {
		pr_err("dsi regulator get failed\n");
		goto fail;
	}
	err = roth_dsi_gpio_get();
	if (err < 0) {
		pr_err("dsi gpio request failed\n");
		goto fail;
	}

	if (avdd_lcd_3v0_2v8) {
		err = regulator_enable(avdd_lcd_3v0_2v8);
		if (err < 0) {
			pr_err("avdd_lcd_3v0_2v8 regulator enable failed\n");
			goto fail;
		}
		regulator_set_voltage(avdd_lcd_3v0_2v8, 2800000, 2800000);
	}
	usleep_range(3000, 5000);

	if (vdd_lcd_s_1v8) {
		err = regulator_enable(vdd_lcd_s_1v8);
		if (err < 0) {
			pr_err("vdd_lcd_1v8_s regulator enable failed\n");
			goto fail;
		}
	}
	usleep_range(3000, 5000);

	if (vdd_lcd_bl) {
		err = regulator_enable(vdd_lcd_bl);
		if (err < 0) {
			pr_err("vdd_lcd_bl regulator enable failed\n");
			goto fail;
		}
	}

	if (vdd_lcd_bl_en) {
		err = regulator_enable(vdd_lcd_bl_en);
		if (err < 0) {
			pr_err("vdd_lcd_bl_en regulator enable failed\n");
			goto fail;
		}
	}

	/* Skip panel programming if in initialized mode */
	if (roth_disp1_out.flags & TEGRA_DC_OUT_INITIALIZED_MODE)
		return 0;

#if DSI_PANEL_RESET
	gpio_direction_output(DSI_PANEL_RST_GPIO, 1);
	usleep_range(1000, 5000);
	gpio_set_value(DSI_PANEL_RST_GPIO, 0);
	usleep_range(1000, 5000);
	gpio_set_value(DSI_PANEL_RST_GPIO, 1);
	msleep(20);
#endif

	return 0;
fail:
	return err;
}

static int roth_dsi_panel_disable(void)
{
	if (vdd_lcd_bl)
		regulator_disable(vdd_lcd_bl);

	if (vdd_lcd_bl_en)
		regulator_disable(vdd_lcd_bl_en);

	if (vdd_lcd_s_1v8)
		regulator_disable(vdd_lcd_s_1v8);

	if (avdd_lcd_3v0_2v8)
		regulator_disable(avdd_lcd_3v0_2v8);

	return 0;
}

static int roth_dsi_panel_postsuspend(void)
{
	/* TODO */
	return 0;
}

static struct tegra_dc_mode roth_dsi_modes[] = {
	{
		.pclk = 66700000,
		.h_ref_to_sync = 4,
		.v_ref_to_sync = 1,
		.h_sync_width = 4,
		.v_sync_width = 4,
		.h_back_porch = 112,
		.v_back_porch = 7,
		.h_active = 720,
		.v_active = 1280,
		.h_front_porch = 12,
		.v_front_porch = 20,
	},
};

static struct tegra_dc_sd_settings sd_settings;

static struct tegra_dc_out roth_disp1_out = {
	.type		= TEGRA_DC_OUT_DSI,
	.dsi		= &roth_dsi,

	.flags		= DC_CTRL_MODE,
	.sd_settings	= &sd_settings,

	.modes		= roth_dsi_modes,
	.n_modes	= ARRAY_SIZE(roth_dsi_modes),

	.enable		= roth_dsi_panel_enable,
	.disable	= roth_dsi_panel_disable,
	.postsuspend	= roth_dsi_panel_postsuspend,
	.width		= 62,
	.height		= 110,
};

static int roth_hdmi_enable(struct device *dev)
{
	int ret;
	if (!roth_hdmi_reg) {
		roth_hdmi_reg = regulator_get(dev, "avdd_hdmi");
		if (IS_ERR_OR_NULL(roth_hdmi_reg)) {
			pr_err("hdmi: couldn't get regulator avdd_hdmi\n");
			roth_hdmi_reg = NULL;
			return PTR_ERR(roth_hdmi_reg);
		}
	}
	ret = regulator_enable(roth_hdmi_reg);
	if (ret < 0) {
		pr_err("hdmi: couldn't enable regulator avdd_hdmi\n");
		return ret;
	}
	if (!roth_hdmi_pll) {
		roth_hdmi_pll = regulator_get(dev, "avdd_hdmi_pll");
		if (IS_ERR_OR_NULL(roth_hdmi_pll)) {
			pr_err("hdmi: couldn't get regulator avdd_hdmi_pll\n");
			roth_hdmi_pll = NULL;
			regulator_put(roth_hdmi_reg);
			roth_hdmi_reg = NULL;
			return PTR_ERR(roth_hdmi_pll);
		}
	}
	ret = regulator_enable(roth_hdmi_pll);
	if (ret < 0) {
		pr_err("hdmi: couldn't enable regulator avdd_hdmi_pll\n");
		return ret;
	}
	return 0;
}

static int roth_hdmi_disable(void)
{
	if (roth_hdmi_reg) {
		regulator_disable(roth_hdmi_reg);
		regulator_put(roth_hdmi_reg);
		roth_hdmi_reg = NULL;
	}

	if (roth_hdmi_pll) {
		regulator_disable(roth_hdmi_pll);
		regulator_put(roth_hdmi_pll);
		roth_hdmi_pll = NULL;
	}

	return 0;
}

static int roth_hdmi_postsuspend(void)
{
	if (roth_hdmi_vddio) {
		regulator_disable(roth_hdmi_vddio);
		regulator_put(roth_hdmi_vddio);
		roth_hdmi_vddio = NULL;
	}
	return 0;
}

static int roth_hdmi_hotplug_init(struct device *dev)
{
	if (!roth_hdmi_vddio) {
		roth_hdmi_vddio = regulator_get(dev, "vdd_hdmi_5v0");
		if (WARN_ON(IS_ERR(roth_hdmi_vddio))) {
			pr_err("%s: couldn't get regulator vdd_hdmi_5v0: %ld\n",
					__func__, PTR_ERR(roth_hdmi_vddio));
			roth_hdmi_vddio = NULL;
		} else
			regulator_enable(roth_hdmi_vddio);
	}
	return 0;
}

static void roth_hdmi_hotplug_report(bool state)
{
	if (state) {
		tegra_pinmux_set_pullupdown(TEGRA_PINGROUP_DDC_SDA,
						TEGRA_PUPD_PULL_DOWN);
		tegra_pinmux_set_pullupdown(TEGRA_PINGROUP_DDC_SCL,
						TEGRA_PUPD_PULL_DOWN);
	} else {
		tegra_pinmux_set_pullupdown(TEGRA_PINGROUP_DDC_SDA,
						TEGRA_PUPD_NORMAL);
		tegra_pinmux_set_pullupdown(TEGRA_PINGROUP_DDC_SCL,
						TEGRA_PUPD_NORMAL);
	}
}

static struct tegra_dc_out roth_disp2_out = {
	.type		= TEGRA_DC_OUT_HDMI,
	.flags		= TEGRA_DC_OUT_HOTPLUG_HIGH,
	.parent_clk	= "pll_d2_out0",

	.dcc_bus	= 3,
	.hotplug_gpio	= roth_hdmi_hpd,

	.max_pixclock	= KHZ2PICOS(297000),

	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,

	.enable		= roth_hdmi_enable,
	.disable	= roth_hdmi_disable,
	.postsuspend	= roth_hdmi_postsuspend,
	.hotplug_init	= roth_hdmi_hotplug_init,
	.hotplug_report = roth_hdmi_hotplug_report,
};

static struct tegra_fb_data roth_disp1_fb_data = {
	.win		= 0,
	.bits_per_pixel = 32,
	.flags		= TEGRA_FB_FLIP_ON_PROBE,
	.xres		= 720,
	.yres		= 1280,
};

static struct tegra_dc_platform_data roth_disp1_pdata = {
	.flags		= TEGRA_DC_FLAG_ENABLED,
	.default_out	= &roth_disp1_out,
	.fb		= &roth_disp1_fb_data,
	.emc_clk_rate	= 204000000,
#ifdef CONFIG_TEGRA_DC_CMU
	.cmu_enable	= 1,
#endif
};

static struct tegra_fb_data roth_disp2_fb_data = {
	.win		= 0,
	.xres		= 1024,
	.yres		= 600,
	.bits_per_pixel = 32,
	.flags		= TEGRA_FB_FLIP_ON_PROBE,
};

static struct tegra_dc_platform_data roth_disp2_pdata = {
	.flags		= TEGRA_DC_FLAG_ENABLED,
	.default_out	= &roth_disp2_out,
	.fb		= &roth_disp2_fb_data,
	.emc_clk_rate	= 300000000,
};

static struct platform_device roth_disp2_device = {
	.name		= "tegradc",
	.id		= 1,
	.resource	= roth_disp2_resources,
	.num_resources	= ARRAY_SIZE(roth_disp2_resources),
	.dev = {
		.platform_data = &roth_disp2_pdata,
	},
};

static struct platform_device roth_disp1_device = {
	.name		= "tegradc",
	.id		= 0,
	.resource	= roth_disp1_resources,
	.num_resources	= ARRAY_SIZE(roth_disp1_resources),
	.dev = {
		.platform_data = &roth_disp1_pdata,
	},
};

static struct nvmap_platform_carveout roth_carveouts[] = {
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
		.base		= 0, /* Filled in by roth_panel_init() */
		.size		= 0, /* Filled in by roth_panel_init() */
		.buddy_size	= SZ_32K,
	},
	[2] = {
		.name		= "vpr",
		.usage_mask	= NVMAP_HEAP_CARVEOUT_VPR,
		.base		= 0, /* Filled in by roth_panel_init() */
		.size		= 0, /* Filled in by roth_panel_init() */
		.buddy_size	= SZ_32K,
	},
};

static struct nvmap_platform_data roth_nvmap_data = {
	.carveouts	= roth_carveouts,
	.nr_carveouts	= ARRAY_SIZE(roth_carveouts),
};

static struct platform_device roth_nvmap_device = {
	.name	= "tegra-nvmap",
	.id	= -1,
	.dev	= {
		.platform_data = &roth_nvmap_data,
	},
};

static int roth_disp1_bl_notify(struct device *unused, int brightness)
{
	int cur_sd_brightness = atomic_read(&sd_brightness);

	/* SD brightness is a percentage */
	brightness = (brightness * cur_sd_brightness) / 255;

	/* Apply any backlight response curve */
	if (brightness > 255)
		pr_info("Error: Brightness > 255!\n");

	return brightness;
}

static int roth_disp1_check_fb(struct device *dev, struct fb_info *info)
{
	return info->device == &roth_disp1_device.dev;
}

static struct platform_pwm_backlight_data roth_disp1_bl_data = {
	.pwm_id		= 1,
	.max_brightness	= 255,
	.dft_brightness	= 77,
	.pwm_period_ns	= 40000,
	.pwm_gpio	= DSI_PANEL_BL_PWM,
	.notify		= roth_disp1_bl_notify,
	/* Only toggle backlight on fb blank notifications for disp1 */
	.check_fb	= roth_disp1_check_fb,
};

static struct platform_device __maybe_unused
		roth_disp1_bl_device = {
	.name	= "pwm-backlight",
	.id	= -1,
	.dev	= {
		.platform_data = &roth_disp1_bl_data,
	},
};

static struct tegra_dc_sd_settings roth_sd_settings = {
	.enable = 1, /* enabled by default. */
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
	.bl_device_name = "pwm-backlight",
};

static struct platform_device __maybe_unused
			*roth_bl_device[] __initdata = {
	&tegra_pwfm1_device,
	&roth_disp1_bl_device,
};

int __init roth_panel_init(int board_id)
{
	int err = 0;
	struct resource __maybe_unused *res;
	struct platform_device *phost1x;

	sd_settings = roth_sd_settings;
#ifdef CONFIG_TEGRA_NVMAP
	roth_carveouts[1].base = tegra_carveout_start;
	roth_carveouts[1].size = tegra_carveout_size;
	roth_carveouts[2].base = tegra_vpr_start;
	roth_carveouts[2].size = tegra_vpr_size;

	err = platform_device_register(&roth_nvmap_device);
	if (err) {
		pr_err("nvmap device registration failed\n");
		return err;
	}
#endif

	phost1x = roth_host1x_init();
	if (!phost1x) {
		pr_err("host1x devices registration failed\n");
		return -EINVAL;
	}

	gpio_request(roth_hdmi_hpd, "hdmi_hpd");
	gpio_direction_input(roth_hdmi_hpd);
	res = platform_get_resource_byname(&roth_disp1_device,
					 IORESOURCE_MEM, "fbmem");
	res->start = tegra_fb_start;
	res->end = tegra_fb_start + tegra_fb_size - 1;

	/* Copy the bootloader fb to the fb. */
	__tegra_move_framebuffer(&roth_nvmap_device,
		tegra_fb_start, tegra_bootloader_fb_start,
			min(tegra_fb_size, tegra_bootloader_fb_size));

	/*
	 * only roth supports initialized mode.
	 */
	if (!board_id)
		roth_disp1_out.flags |= TEGRA_DC_OUT_INITIALIZED_MODE;

	roth_disp1_device.dev.parent = &phost1x->dev;
	err = platform_device_register(&roth_disp1_device);
	if (err) {
		pr_err("disp1 device registration failed\n");
		return err;
	}

	err = tegra_init_hdmi(&roth_disp2_device, phost1x);
	if (err)
		return err;

#if IS_EXTERNAL_PWM
	err = platform_add_devices(roth_bl_device,
				ARRAY_SIZE(roth_bl_device));
	if (err) {
		pr_err("disp1 bl device registration failed");
		return err;
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
#else
int __init roth_panel_init(void)
{
	if (roth_host1x_init())
		return 0;
	else
		return -EINVAL;
}
#endif
