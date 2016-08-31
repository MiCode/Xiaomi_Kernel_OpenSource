/*
 * arch/arm/mach-tegra/board-dalmore-panel.c
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
#include <linux/of.h>

#include <mach/irqs.h>
#include <mach/dc.h>
#include <mach/pinmux.h>
#include <mach/pinmux-t11.h>

#include "board.h"
#include "devices.h"
#include "gpio-names.h"
#include "board-panel.h"
#include "common.h"
#include "iomap.h"
#include "tegra11_host1x_devices.h"

#define DSI_PANEL_RST_GPIO	TEGRA_GPIO_PH3
#define DSI_PANEL_BL_PWM_GPIO	TEGRA_GPIO_PH1

struct platform_device * __init dalmore_host1x_init(void)
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

/* HDMI Hotplug detection pin */
#define dalmore_hdmi_hpd	TEGRA_GPIO_PN7

static struct regulator *dalmore_hdmi_reg;
static struct regulator *dalmore_hdmi_pll;
static struct regulator *dalmore_hdmi_vddio;

static struct resource dalmore_disp1_resources[] = {
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
		.start	= 0, /* Filled in by dalmore_panel_init() */
		.end	= 0, /* Filled in by dalmore_panel_init() */
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

static struct resource dalmore_disp2_resources[] = {
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
		.start	= 0, /* Filled in by dalmore_panel_init() */
		.end	= 0, /* Filled in by dalmore_panel_init() */
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

static struct tegra_dc_out dalmore_disp1_out = {
	.type		= TEGRA_DC_OUT_DSI,
	.sd_settings	= &sd_settings,
};

static int dalmore_hdmi_enable(struct device *dev)
{
	int ret;
	if (!dalmore_hdmi_reg) {
		dalmore_hdmi_reg = regulator_get(dev, "avdd_hdmi");
		if (IS_ERR(dalmore_hdmi_reg)) {
			pr_err("hdmi: couldn't get regulator avdd_hdmi\n");
			dalmore_hdmi_reg = NULL;
			return PTR_ERR(dalmore_hdmi_reg);
		}
	}
	ret = regulator_enable(dalmore_hdmi_reg);
	if (ret < 0) {
		pr_err("hdmi: couldn't enable regulator avdd_hdmi\n");
		return ret;
	}
	if (!dalmore_hdmi_pll) {
		dalmore_hdmi_pll = regulator_get(dev, "avdd_hdmi_pll");
		if (IS_ERR(dalmore_hdmi_pll)) {
			pr_err("hdmi: couldn't get regulator avdd_hdmi_pll\n");
			dalmore_hdmi_pll = NULL;
			regulator_put(dalmore_hdmi_reg);
			dalmore_hdmi_reg = NULL;
			return PTR_ERR(dalmore_hdmi_pll);
		}
	}
	ret = regulator_enable(dalmore_hdmi_pll);
	if (ret < 0) {
		pr_err("hdmi: couldn't enable regulator avdd_hdmi_pll\n");
		return ret;
	}
	return 0;
}

static int dalmore_hdmi_disable(void)
{
	if (dalmore_hdmi_reg) {
		regulator_disable(dalmore_hdmi_reg);
		regulator_put(dalmore_hdmi_reg);
		dalmore_hdmi_reg = NULL;
	}

	if (dalmore_hdmi_pll) {
		regulator_disable(dalmore_hdmi_pll);
		regulator_put(dalmore_hdmi_pll);
		dalmore_hdmi_pll = NULL;
	}

	return 0;
}

static int dalmore_hdmi_postsuspend(void)
{
	if (dalmore_hdmi_vddio) {
		regulator_disable(dalmore_hdmi_vddio);
		regulator_put(dalmore_hdmi_vddio);
		dalmore_hdmi_vddio = NULL;
	}
	return 0;
}

static int dalmore_hdmi_hotplug_init(struct device *dev)
{
	int e = 0;

	if (!dalmore_hdmi_vddio) {
		dalmore_hdmi_vddio = regulator_get(dev, "vdd_hdmi_5v0");
		if (WARN_ON(IS_ERR(dalmore_hdmi_vddio))) {
			e = PTR_ERR(dalmore_hdmi_vddio);
			pr_err("%s: couldn't get regulator vdd_hdmi_5v0: %d\n",
				__func__, e);
			dalmore_hdmi_vddio = NULL;
		} else {
			e = regulator_enable(dalmore_hdmi_vddio);
		}
	}

	return e;
}

static void dalmore_hdmi_hotplug_report(bool state)
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

/* Electrical characteristics for HDMI, all modes must be declared here */
struct tmds_config dalmore_tmds_config[] = {
	{ /* 480p : 27 MHz and below */
		.pclk = 27000000,
		.pll0 = 0x01003010,
		.pll1 = 0x00301b00,
		.drive_current = 0x23232323,
		.pe_current = 0x00000000,
		.peak_current = 0x00000000,
	},
	{ /* 720p : 74.25MHz modes */
		.pclk = 74250000,
		.pll0 = 0x01003110,
		.pll1 = 0x00301b00,
		.drive_current = 0x25252525,
		.pe_current = 0x00000000,
		.peak_current = 0x03030303,
	},
	{ /* 1080p : 148.5MHz modes */
		.pclk = 148500000,
		.pll0 = 0x01003310,
		.pll1 = 0x00301b00,
		.drive_current = 0x27272727,
		.pe_current = 0x00000000,
		.peak_current = 0x03030303,
	},
	{ /* 4K : 297MHz modes */
		.pclk = INT_MAX,
		.pll0 = 0x01003f10,
		.pll1 = 0x00300f00,
		.drive_current = 0x303f3f3f,
		.pe_current = 0x00000000,
		.peak_current = 0x040f0f0f,
	},
};

struct tegra_hdmi_out dalmore_hdmi_out = {
	.tmds_config = dalmore_tmds_config,
	.n_tmds_config = ARRAY_SIZE(dalmore_tmds_config),
};

static struct tegra_dc_out dalmore_disp2_out = {
	.type		= TEGRA_DC_OUT_HDMI,
	.flags		= TEGRA_DC_OUT_HOTPLUG_HIGH,
	.parent_clk	= "pll_d2_out0",

	.dcc_bus	= 3,
	.hotplug_gpio	= dalmore_hdmi_hpd,
	.hdmi_out	= &dalmore_hdmi_out,

	.max_pixclock	= KHZ2PICOS(297000),

	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,

	.enable		= dalmore_hdmi_enable,
	.disable	= dalmore_hdmi_disable,
	.postsuspend	= dalmore_hdmi_postsuspend,
	.hotplug_init	= dalmore_hdmi_hotplug_init,
	.hotplug_report	= dalmore_hdmi_hotplug_report,
};

static struct tegra_fb_data dalmore_disp1_fb_data = {
	.win		= 0,
	.bits_per_pixel = 32,
	.flags		= TEGRA_FB_FLIP_ON_PROBE,
};

static struct tegra_dc_platform_data dalmore_disp1_pdata = {
	.flags		= TEGRA_DC_FLAG_ENABLED,
	.default_out	= &dalmore_disp1_out,
	.fb		= &dalmore_disp1_fb_data,
	.emc_clk_rate	= 204000000,
#ifdef CONFIG_TEGRA_DC_CMU
	.cmu_enable	= 1,
#endif
};

static struct tegra_fb_data dalmore_disp2_fb_data = {
	.win		= 0,
	.xres		= 1280,
	.yres		= 720,
	.bits_per_pixel = 32,
	.flags		= TEGRA_FB_FLIP_ON_PROBE,
};

static struct tegra_dc_platform_data dalmore_disp2_pdata = {
	.flags		= TEGRA_DC_FLAG_ENABLED,
	.default_out	= &dalmore_disp2_out,
	.fb		= &dalmore_disp2_fb_data,
	.emc_clk_rate	= 300000000,
#ifdef CONFIG_TEGRA_DC_CMU
	.cmu_enable	= 1,
#endif
};

static struct platform_device dalmore_disp2_device = {
	.name		= "tegradc",
	.id		= 1,
	.resource	= dalmore_disp2_resources,
	.num_resources	= ARRAY_SIZE(dalmore_disp2_resources),
	.dev = {
		.platform_data = &dalmore_disp2_pdata,
	},
};

static struct platform_device dalmore_disp1_device = {
	.name		= "tegradc",
	.id		= 0,
	.resource	= dalmore_disp1_resources,
	.num_resources	= ARRAY_SIZE(dalmore_disp1_resources),
	.dev = {
		.platform_data = &dalmore_disp1_pdata,
	},
};

static struct nvmap_platform_carveout dalmore_carveouts[] = {
	[0] = {
		.name		= "iram",
		.usage_mask	= NVMAP_HEAP_CARVEOUT_IRAM,
		.base		= TEGRA_IRAM_BASE + TEGRA_RESET_HANDLER_SIZE,
		.size		= TEGRA_IRAM_SIZE - TEGRA_RESET_HANDLER_SIZE,
	},
	[1] = {
		.name		= "generic-0",
		.usage_mask	= NVMAP_HEAP_CARVEOUT_GENERIC,
		.base		= 0, /* Filled in by dalmore_panel_init() */
		.size		= 0, /* Filled in by dalmore_panel_init() */
	},
	[2] = {
		.name		= "vpr",
		.usage_mask	= NVMAP_HEAP_CARVEOUT_VPR,
		.base		= 0, /* Filled in by dalmore_panel_init() */
		.size		= 0, /* Filled in by dalmore_panel_init() */
	},
};

static struct nvmap_platform_data dalmore_nvmap_data = {
	.carveouts	= dalmore_carveouts,
	.nr_carveouts	= ARRAY_SIZE(dalmore_carveouts),
};
static struct platform_device dalmore_nvmap_device = {
	.name	= "tegra-nvmap",
	.id	= -1,
	.dev	= {
		.platform_data = &dalmore_nvmap_data,
	},
};

static void dalmore_panel_select(void)
{
	struct tegra_panel *panel = NULL;
	struct board_info board;
	u8 dsi_instance;

	tegra_get_display_board_info(&board);

	switch (board.board_id) {
	case BOARD_E1639:
		panel = &dsi_s_wqxga_10_1;
		/* FIXME: panel used ganged mode,need to check if
		 * the dsi_instance is useful in this case
		 */
		dsi_instance = DSI_INSTANCE_0;
		break;
	case BOARD_E1631:
		panel = &dsi_a_1080p_11_6;
		dsi_instance = DSI_INSTANCE_0;
		break;
	case BOARD_E1627:
	/* fall through */
	default:
		panel = &dsi_p_wuxga_10_1;
		dsi_instance = DSI_INSTANCE_0;
		break;
	}
	if (panel) {
		if (panel->init_sd_settings)
			panel->init_sd_settings(&sd_settings);

		if (panel->init_dc_out) {
			panel->init_dc_out(&dalmore_disp1_out);
			dalmore_disp1_out.dsi->dsi_instance = dsi_instance;
			dalmore_disp1_out.dsi->dsi_panel_rst_gpio =
				DSI_PANEL_RST_GPIO;
			dalmore_disp1_out.dsi->dsi_panel_bl_pwm_gpio =
				DSI_PANEL_BL_PWM_GPIO;
		}

		if (panel->init_fb_data)
			panel->init_fb_data(&dalmore_disp1_fb_data);

		if (panel->init_cmu_data)
			panel->init_cmu_data(&dalmore_disp1_pdata);

		if (panel->set_disp_device)
			panel->set_disp_device(&dalmore_disp1_device);

		tegra_dsi_resources_init(dsi_instance, dalmore_disp1_resources,
			ARRAY_SIZE(dalmore_disp1_resources));

		if (panel->register_bl_dev)
			panel->register_bl_dev();

		if (panel->register_i2c_bridge)
			panel->register_i2c_bridge();
	}

}
int __init dalmore_panel_init(void)
{
	int err = 0;
	struct resource __maybe_unused *res;
	struct platform_device *phost1x = NULL;

	dalmore_panel_select();

#ifdef CONFIG_TEGRA_NVMAP
	dalmore_carveouts[1].base = tegra_carveout_start;
	dalmore_carveouts[1].size = tegra_carveout_size;
	dalmore_carveouts[2].base = tegra_vpr_start;
	dalmore_carveouts[2].size = tegra_vpr_size;
#ifdef CONFIG_NVMAP_USE_CMA_FOR_CARVEOUT
	dalmore_carveouts[1].cma_dev =  &tegra_generic_cma_dev;
	dalmore_carveouts[1].resize = false;
	dalmore_carveouts[2].cma_dev =  &tegra_vpr_cma_dev;
	dalmore_carveouts[2].resize = true;
	dalmore_carveouts[2].cma_chunk_size = SZ_32M;
#endif

	err = platform_device_register(&dalmore_nvmap_device);
	if (err) {
		pr_err("nvmap device registration failed\n");
		return err;
	}
#endif

	phost1x = dalmore_host1x_init();
	if (!phost1x) {
		pr_err("host1x devices registration failed\n");
		return -EINVAL;
	}

	res = platform_get_resource_byname(&dalmore_disp1_device,
					 IORESOURCE_MEM, "fbmem");
	res->start = tegra_fb_start;
	res->end = tegra_fb_start + tegra_fb_size - 1;

	/* Copy the bootloader fb to the fb. */
	if (tegra_bootloader_fb_size)
		__tegra_move_framebuffer(&dalmore_nvmap_device,
				tegra_fb_start, tegra_bootloader_fb_start,
				min(tegra_fb_size, tegra_bootloader_fb_size));
	else
		__tegra_clear_framebuffer(&dalmore_nvmap_device,
					  tegra_fb_start, tegra_fb_size);

	dalmore_disp1_device.dev.parent = &phost1x->dev;
	err = platform_device_register(&dalmore_disp1_device);
	if (err) {
		pr_err("disp1 device registration failed\n");
		return err;
	}

	err = tegra_init_hdmi(&dalmore_disp2_device, phost1x);
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
