/*
 * arch/arm/mach-tegra/board-ardbeg-panel.c
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/dma-contiguous.h>
#include <linux/clk.h>

#include <mach/irqs.h>
#include <mach/dc.h>
#include <mach/io_dpd.h>

#include "board.h"
#include "tegra-board-id.h"
#include "devices.h"
#include "gpio-names.h"
#include "board-ardbeg.h"
#include "board-panel.h"
#include "common.h"
#include "iomap.h"
#include "tegra12_host1x_devices.h"
#include "dvfs.h"

struct platform_device * __init ardbeg_host1x_init(void)
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

/* hdmi related regulators */
static struct regulator *ardbeg_hdmi_reg;
static struct regulator *ardbeg_hdmi_pll;
static struct regulator *ardbeg_hdmi_vddio;

#ifndef CONFIG_TEGRA_HDMI_PRIMARY
static struct resource ardbeg_disp1_resources[] = {
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
		.start	= 0, /* Filled in by ardbeg_panel_init() */
		.end	= 0, /* Filled in by ardbeg_panel_init() */
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

static struct resource ardbeg_disp1_edp_resources[] = {
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
		.start	= 0, /* Filled in by ardbeg_panel_init() */
		.end	= 0, /* Filled in by ardbeg_panel_init() */
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "mipi_cal",
		.start	= TEGRA_MIPI_CAL_BASE,
		.end	= TEGRA_MIPI_CAL_BASE + TEGRA_MIPI_CAL_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name   = "sor",
		.start  = TEGRA_SOR_BASE,
		.end    = TEGRA_SOR_BASE + TEGRA_SOR_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "dpaux",
		.start  = TEGRA_DPAUX_BASE,
		.end    = TEGRA_DPAUX_BASE + TEGRA_DPAUX_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name	= "irq_dp",
		.start	= INT_DPAUX,
		.end	= INT_DPAUX,
		.flags	= IORESOURCE_IRQ,
	},
};
#endif

static struct resource ardbeg_disp2_resources[] = {
	{
		.name	= "irq",
#ifndef CONFIG_TEGRA_HDMI_PRIMARY
		.start	= INT_DISPLAY_B_GENERAL,
		.end	= INT_DISPLAY_B_GENERAL,
#else
		.start	= INT_DISPLAY_GENERAL,
		.end	= INT_DISPLAY_GENERAL,
#endif
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "regs",
#ifndef CONFIG_TEGRA_HDMI_PRIMARY
		.start	= TEGRA_DISPLAY2_BASE,
		.end	= TEGRA_DISPLAY2_BASE + TEGRA_DISPLAY2_SIZE - 1,
#else
		.start	= TEGRA_DISPLAY_BASE,
		.end	= TEGRA_DISPLAY_BASE + TEGRA_DISPLAY_SIZE - 1,
#endif
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "fbmem",
		.start	= 0, /* Filled in by ardbeg_panel_init() */
		.end	= 0, /* Filled in by ardbeg_panel_init() */
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "hdmi_regs",
		.start	= TEGRA_HDMI_BASE,
		.end	= TEGRA_HDMI_BASE + TEGRA_HDMI_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};


#ifndef CONFIG_TEGRA_HDMI_PRIMARY
static struct tegra_dc_sd_settings sd_settings;

static struct tegra_dc_out ardbeg_disp1_out = {
	.type		= TEGRA_DC_OUT_DSI,
	.sd_settings	= &sd_settings,
};
#endif

static int ardbeg_hdmi_enable(struct device *dev)
{
	int ret;
	if (!ardbeg_hdmi_reg) {
		ardbeg_hdmi_reg = regulator_get(dev, "avdd_hdmi");
		if (IS_ERR(ardbeg_hdmi_reg)) {
			pr_err("hdmi: couldn't get regulator avdd_hdmi\n");
			ardbeg_hdmi_reg = NULL;
			return PTR_ERR(ardbeg_hdmi_reg);
		}
	}
	ret = regulator_enable(ardbeg_hdmi_reg);
	if (ret < 0) {
		pr_err("hdmi: couldn't enable regulator avdd_hdmi\n");
		return ret;
	}
	if (!ardbeg_hdmi_pll) {
		ardbeg_hdmi_pll = regulator_get(dev, "avdd_hdmi_pll");
		if (IS_ERR(ardbeg_hdmi_pll)) {
			pr_err("hdmi: couldn't get regulator avdd_hdmi_pll\n");
			ardbeg_hdmi_pll = NULL;
			regulator_put(ardbeg_hdmi_reg);
			ardbeg_hdmi_reg = NULL;
			return PTR_ERR(ardbeg_hdmi_pll);
		}
	}
	ret = regulator_enable(ardbeg_hdmi_pll);
	if (ret < 0) {
		pr_err("hdmi: couldn't enable regulator avdd_hdmi_pll\n");
		return ret;
	}
	return 0;
}

static int ardbeg_hdmi_disable(void)
{
	if (ardbeg_hdmi_reg) {
		regulator_disable(ardbeg_hdmi_reg);
		regulator_put(ardbeg_hdmi_reg);
		ardbeg_hdmi_reg = NULL;
	}

	if (ardbeg_hdmi_pll) {
		regulator_disable(ardbeg_hdmi_pll);
		regulator_put(ardbeg_hdmi_pll);
		ardbeg_hdmi_pll = NULL;
	}
	return 0;
}

static int ardbeg_hdmi_postsuspend(void)
{
	if (ardbeg_hdmi_vddio) {
		regulator_disable(ardbeg_hdmi_vddio);
		regulator_put(ardbeg_hdmi_vddio);
		ardbeg_hdmi_vddio = NULL;
	}
	return 0;
}

static int ardbeg_hdmi_hotplug_init(struct device *dev)
{
	if (!ardbeg_hdmi_vddio) {
#ifdef CONFIG_TEGRA_HDMI_PRIMARY
		if (of_machine_is_compatible("nvidia,tn8"))
			ardbeg_hdmi_vddio = regulator_get(dev, "vdd-out1-5v0");
		else
			ardbeg_hdmi_vddio = regulator_get(dev, "vdd_hdmi_5v0");
#else
		ardbeg_hdmi_vddio = regulator_get(dev, "vdd_hdmi_5v0");
#endif
		if (WARN_ON(IS_ERR(ardbeg_hdmi_vddio))) {
			pr_err("%s: couldn't get regulator vdd_hdmi_5v0: %ld\n",
				__func__, PTR_ERR(ardbeg_hdmi_vddio));
				ardbeg_hdmi_vddio = NULL;
		} else {
			return regulator_enable(ardbeg_hdmi_vddio);
		}
	}

	return 0;
}

struct tmds_config ardbeg_tmds_config[] = {
	{ /* 480p/576p / 25.2MHz/27MHz modes */
	.version = MKDEV(1, 0),
	.pclk = 27000000,
	.pll0 = 0x01003010,
	.pll1 = 0x00301B00,
	.pe_current = 0x00000000,
	.drive_current = 0x1F1F1F1F,
	.peak_current = 0x03030303,
	.pad_ctls0_mask    = 0xfffff0ff,
	.pad_ctls0_setting = 0x00000400, /* BG_VREF_LEVEL */
	},
	{ /* 720p / 74.25MHz modes */
	.version = MKDEV(1, 0),
	.pclk = 74250000,
	.pll0 = 0x01003110,
	.pll1 = 0x00301500,
	.pe_current = 0x00000000,
	.drive_current = 0x2C2C2C2C,
	.peak_current = 0x07070707,
	.pad_ctls0_mask    = 0xfffff0ff,
	.pad_ctls0_setting = 0x00000400, /* BG_VREF_LEVEL */
	},
	{ /* 1080p / 148.5MHz modes */
	.version = MKDEV(1, 0),
	.pclk = 148500000,
	.pll0 = 0x01003310,
	.pll1 = 0x00301500,
	.pe_current = 0x00000000,
	.drive_current = 0x33333333,
	.peak_current = 0x0C0C0C0C,
	.pad_ctls0_mask    = 0xfffff0ff,
	.pad_ctls0_setting = 0x00000400, /* BG_VREF_LEVEL */
	},
	{
	.version = MKDEV(1, 0),
	.pclk = INT_MAX,
	.pll0 = 0x01003F10,
	.pll1 = 0x00300F00,
	.pe_current = 0x00000000,
	.drive_current = 0x37373737, /* lane3 needs a slightly lower current */
	.peak_current = 0x17171717,
	.pad_ctls0_mask    = 0xfffff0ff,
	.pad_ctls0_setting = 0x00000600, /* BG_VREF_LEVEL */
	},
};

struct tmds_config ardbeg_tn8_tmds_config[] = {
	{ /* 480p/576p / 25.2MHz/27MHz modes */
	.version = MKDEV(1, 0),
	.pclk = 27000000,
	.pll0 = 0x01003010,
	.pll1 = 0x00301b00,
	.pe_current    = 0x00000000,
	.drive_current = 0x1C1C1C1C,
	.peak_current  = 0x00000000,
	.pad_ctls0_mask    = 0xfffff0ff,
	.pad_ctls0_setting = 0x00000400, /* BG_VREF_LEVEL */
	},
	{ /* 720p / 74.25MHz modes */
	.version = MKDEV(1, 0),
	.pclk = 74250000,
	.pll0 = 0x01003110,
	.pll1 = 0x00301500,
	.pe_current    = 0x00000000,
	.drive_current = 0x23232323,
	.peak_current  = 0x00000000,
	.pad_ctls0_mask    = 0xfffff0ff,
	.pad_ctls0_setting = 0x00000400, /* BG_VREF_LEVEL */
	},
	{ /* 1080p / 148.5MHz modes */
	.version = MKDEV(1, 0),
	.pclk = 148500000,
	.pll0 = 0x01003310,
	.pll1 = 0x10300F00,
	.pe_current    = 0x00000000,
	.drive_current = 0x2B2C2D2B,
	.peak_current  = 0x00000000,
	.pad_ctls0_mask    = 0xfffff0ff,
	.pad_ctls0_setting = 0x00000400, /* BG_VREF_LEVEL */
	},
	{
	.version = MKDEV(1, 0),
	.pclk = INT_MAX,
	.pll0 = 0x01003F10,
	.pll1 = 0x10300700,
	.pe_current    = 0x00000000,
	.drive_current = 0x32323131,
	.peak_current  = 0x10101010,
	.pad_ctls0_mask    = 0xfffff0ff,
	.pad_ctls0_setting = 0x00000600, /* BG_VREF_LEVEL */
	},
};

struct tegra_hdmi_out ardbeg_hdmi_out = {
	.tmds_config = ardbeg_tmds_config,
	.n_tmds_config = ARRAY_SIZE(ardbeg_tmds_config),
};


#if defined(CONFIG_FRAMEBUFFER_CONSOLE)
static struct tegra_dc_mode hdmi_panel_modes[] = {
	{
		.pclk =			KHZ2PICOS(25200),
		.h_ref_to_sync =	1,
		.v_ref_to_sync =	1,
		.h_sync_width =		96,	/* hsync_len */
		.v_sync_width =		2,	/* vsync_len */
		.h_back_porch =		48,	/* left_margin */
		.v_back_porch =		33,	/* upper_margin */
		.h_active =		640,	/* xres */
		.v_active =		480,	/* yres */
		.h_front_porch =	16,	/* right_margin */
		.v_front_porch =	10,	/* lower_margin */
	},
};
#elif defined(CONFIG_TEGRA_HDMI_PRIMARY)
static struct tegra_dc_mode hdmi_panel_modes[] = {
	{
		.pclk =			148500000,
		.h_ref_to_sync =	1,
		.v_ref_to_sync =	1,
		.h_sync_width =		44,	/* hsync_len */
		.v_sync_width =		5,	/* vsync_len */
		.h_back_porch =		148,	/* left_margin */
		.v_back_porch =		36,	/* upper_margin */
		.h_active =		1920,	/* xres */
		.v_active =		1080,	/* yres */
		.h_front_porch =	88,	/* right_margin */
		.v_front_porch =	4,	/* lower_margin */
	},
};
#endif /* CONFIG_FRAMEBUFFER_CONSOLE || CONFIG_TEGRA_HDMI_PRIMARY*/

static struct tegra_dc_out ardbeg_disp2_out = {
	.type		= TEGRA_DC_OUT_HDMI,
	.flags		= TEGRA_DC_OUT_HOTPLUG_HIGH |
				TEGRA_DC_OUT_HOTPLUG_WAKE_LP0,
	.parent_clk	= "pll_d2",

	.dcc_bus	= 3,
	.hotplug_gpio	= ardbeg_hdmi_hpd,
	.hdmi_out	= &ardbeg_hdmi_out,

	/* TODO: update max pclk to POR */
	.max_pixclock	= KHZ2PICOS(297000),
#if defined(CONFIG_FRAMEBUFFER_CONSOLE) || defined(CONFIG_TEGRA_HDMI_PRIMARY)
	.modes = hdmi_panel_modes,
	.n_modes = ARRAY_SIZE(hdmi_panel_modes),
	.depth = 24,
#endif /* CONFIG_FRAMEBUFFER_CONSOLE */

	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,

	.enable		= ardbeg_hdmi_enable,
	.disable	= ardbeg_hdmi_disable,
	.postsuspend	= ardbeg_hdmi_postsuspend,
	.hotplug_init	= ardbeg_hdmi_hotplug_init,
};

#ifndef CONFIG_TEGRA_HDMI_PRIMARY
static struct tegra_fb_data ardbeg_disp1_fb_data = {
	.win		= 0,
	.bits_per_pixel = 32,
	.flags		= TEGRA_FB_FLIP_ON_PROBE,
};

static struct tegra_dc_platform_data ardbeg_disp1_pdata = {
	.flags		= TEGRA_DC_FLAG_ENABLED,
	.default_out	= &ardbeg_disp1_out,
	.fb		= &ardbeg_disp1_fb_data,
	.emc_clk_rate	= 204000000,
#ifdef CONFIG_TEGRA_DC_CMU
	.cmu_enable	= 1,
#endif
	.low_v_win	= 0x02,
};
#endif

static struct tegra_fb_data ardbeg_disp2_fb_data = {
	.win		= 0,
	.xres		= 1920,
	.yres		= 1080,
	.bits_per_pixel = 32,
	.flags		= TEGRA_FB_FLIP_ON_PROBE,
};

static struct tegra_dc_platform_data ardbeg_disp2_pdata = {
	.flags		= TEGRA_DC_FLAG_ENABLED,
	.default_out	= &ardbeg_disp2_out,
	.fb		= &ardbeg_disp2_fb_data,
	.emc_clk_rate	= 300000000,
};

static struct platform_device ardbeg_disp2_device = {
	.name		= "tegradc",
#ifndef CONFIG_TEGRA_HDMI_PRIMARY
	.id		= 1,
#else
	.id		= 0,
#endif
	.resource	= ardbeg_disp2_resources,
	.num_resources	= ARRAY_SIZE(ardbeg_disp2_resources),
	.dev = {
		.platform_data = &ardbeg_disp2_pdata,
	},
};

#ifndef CONFIG_TEGRA_HDMI_PRIMARY
static struct platform_device ardbeg_disp1_device = {
	.name		= "tegradc",
	.id		= 0,
	.resource	= ardbeg_disp1_resources,
	.num_resources	= ARRAY_SIZE(ardbeg_disp1_resources),
	.dev = {
		.platform_data = &ardbeg_disp1_pdata,
	},
};
#endif

static struct nvmap_platform_carveout ardbeg_carveouts[] = {
	[0] = {
		.name		= "iram",
		.usage_mask	= NVMAP_HEAP_CARVEOUT_IRAM,
		.base		= TEGRA_IRAM_BASE + TEGRA_RESET_HANDLER_SIZE,
		.size		= TEGRA_IRAM_SIZE - TEGRA_RESET_HANDLER_SIZE,
	},
	[1] = {
		.name		= "generic-0",
		.usage_mask	= NVMAP_HEAP_CARVEOUT_GENERIC,
		.base		= 0, /* Filled in by ardbeg_panel_init() */
		.size		= 0, /* Filled in by ardbeg_panel_init() */
	},
	[2] = {
		.name		= "vpr",
		.usage_mask	= NVMAP_HEAP_CARVEOUT_VPR,
		.base		= 0, /* Filled in by ardbeg_panel_init() */
		.size		= 0, /* Filled in by ardbeg_panel_init() */
	},
};

static struct nvmap_platform_data ardbeg_nvmap_data = {
	.carveouts	= ardbeg_carveouts,
	.nr_carveouts	= ARRAY_SIZE(ardbeg_carveouts),
};
static struct platform_device ardbeg_nvmap_device  = {
	.name	= "tegra-nvmap",
	.id	= -1,
	.dev	= {
		.platform_data = &ardbeg_nvmap_data,
	},
};
static struct tegra_io_dpd dsic_io = {
	.name			= "DSIC",
	.io_dpd_reg_index	= 1,
	.io_dpd_bit		= 8,
};
static struct tegra_io_dpd dsid_io = {
	.name			= "DSID",
	.io_dpd_reg_index	= 1,
	.io_dpd_bit		= 9,
};

static struct tegra_dc_dp_lt_settings ardbeg_edp_lt_data[] = {
	/* DriveCurrent	Preemphasis	PostCursor	tx_pu	load_adj */
	{0x13131313,	0x00000000,	0x00000000,	0x20,	0x3},
	{0x13131313,	0x00000000,	0x00000000,	0x20,	0x4},
	{0x19191919,	0x09090909,	0x00000000,	0x30,	0x6}
};

static struct tegra_dp_out dp_settings = {
	/* Panel can override this with its own LT data */
	.lt_settings = ardbeg_edp_lt_data,
	.n_lt_settings = ARRAY_SIZE(ardbeg_edp_lt_data),
};

#ifndef CONFIG_TEGRA_HDMI_PRIMARY
/* can be called multiple times */
static struct tegra_panel *ardbeg_panel_configure(struct board_info *board_out,
	u8 *dsi_instance_out)
{
	struct tegra_panel *panel = NULL;
	u8 dsi_instance = DSI_INSTANCE_0;
	struct board_info boardtmp;

	if (!board_out)
		board_out = &boardtmp;
	tegra_get_display_board_info(board_out);

	switch (board_out->board_id) {
	case BOARD_E1639:
	case BOARD_E1813:
		panel = &dsi_s_wqxga_10_1;
		break;
	case BOARD_PM354:
		panel = &dsi_a_1080p_14_0;
		break;
	case BOARD_E1627:
		panel = &dsi_p_wuxga_10_1;
		tegra_io_dpd_enable(&dsic_io);
		tegra_io_dpd_enable(&dsid_io);
		break;
	case BOARD_E1549:
		panel = &dsi_lgd_wxga_7_0;
		break;
	case BOARD_PM363:
	case BOARD_E1824:
		panel = &edp_a_1080p_14_0;
		ardbeg_disp1_out.type = TEGRA_DC_OUT_DP;
		ardbeg_disp1_out.dp_out = &dp_settings;
		ardbeg_disp1_device.resource = ardbeg_disp1_edp_resources;
		ardbeg_disp1_device.num_resources =
			ARRAY_SIZE(ardbeg_disp1_edp_resources);
		break;
	case BOARD_PM366:
		panel = &lvds_c_1366_14;
		ardbeg_disp1_out.type = TEGRA_DC_OUT_LVDS;
		ardbeg_disp1_device.resource = ardbeg_disp1_edp_resources;
		ardbeg_disp1_device.num_resources =
			ARRAY_SIZE(ardbeg_disp1_edp_resources);
		break;
	case BOARD_E1807:
		panel = &dsi_a_1200_800_8_0;
		dsi_instance = DSI_INSTANCE_0;
		tegra_io_dpd_enable(&dsic_io);
		tegra_io_dpd_enable(&dsid_io);
		break;
	case BOARD_E1937:
		panel = &dsi_a_1200_1920_7_0;
		dsi_instance = DSI_INSTANCE_0;
		break;
	case BOARD_P1761:
		if (tegra_get_board_panel_id())
			panel = &dsi_a_1200_1920_7_0;
		else
			panel = &dsi_a_1200_800_8_0;
		dsi_instance = DSI_INSTANCE_0;
		tegra_io_dpd_enable(&dsic_io);
		tegra_io_dpd_enable(&dsid_io);
		break;
	default:
		panel = &dsi_p_wuxga_10_1;
		tegra_io_dpd_enable(&dsic_io);
		tegra_io_dpd_enable(&dsid_io);
		break;
	}
	if (dsi_instance_out)
		*dsi_instance_out = dsi_instance;
	return panel;
}

static void ardbeg_panel_select(void)
{
	struct tegra_panel *panel = NULL;
	struct board_info board;
	struct board_info mainboard;
	u8 dsi_instance;

	panel = ardbeg_panel_configure(&board, &dsi_instance);

	if (panel) {
		if (panel->init_sd_settings)
			panel->init_sd_settings(&sd_settings);

		if (panel->init_dc_out) {
			panel->init_dc_out(&ardbeg_disp1_out);
			if (ardbeg_disp1_out.type == TEGRA_DC_OUT_DSI) {
				ardbeg_disp1_out.dsi->dsi_instance =
					dsi_instance;
				ardbeg_disp1_out.dsi->dsi_panel_rst_gpio =
					DSI_PANEL_RST_GPIO;
				ardbeg_disp1_out.dsi->dsi_panel_bl_pwm_gpio =
					DSI_PANEL_BL_PWM_GPIO;
				ardbeg_disp1_out.dsi->te_gpio = TEGRA_GPIO_PR6;
			}

			tegra_get_board_info(&mainboard);
			if ((mainboard.board_id == BOARD_E1784) ||
				(mainboard.board_id == BOARD_P1761)) {

				ardbeg_disp1_out.rotation = 180;

				if ((board.board_id == BOARD_E1937) &&
					(board.sku == 1000))
					ardbeg_disp1_out.dsi->
						dsi_panel_rst_gpio =
						TEGRA_GPIO_PN4;
			}
		}

		if (panel->init_fb_data)
			panel->init_fb_data(&ardbeg_disp1_fb_data);

		if (panel->init_cmu_data)
			panel->init_cmu_data(&ardbeg_disp1_pdata);

		if (panel->set_disp_device)
			panel->set_disp_device(&ardbeg_disp1_device);

		if (ardbeg_disp1_out.type == TEGRA_DC_OUT_DSI) {
			tegra_dsi_resources_init(dsi_instance,
				ardbeg_disp1_resources,
				ARRAY_SIZE(ardbeg_disp1_resources));
		}

		if (panel->register_bl_dev)
			panel->register_bl_dev();

		if (panel->register_i2c_bridge)
			panel->register_i2c_bridge();
	}

}
#endif

int __init ardbeg_panel_init(void)
{
	int err = 0;
	struct resource __maybe_unused *res;
	struct platform_device *phost1x = NULL;
	struct board_info board_info;

	struct device_node *dc1_node = NULL;
	struct device_node *dc2_node = NULL;

	find_dc_node(&dc1_node, &dc2_node);

#ifndef CONFIG_TEGRA_HDMI_PRIMARY
	ardbeg_panel_select();
#endif

#ifdef CONFIG_TEGRA_NVMAP
	ardbeg_carveouts[1].base = tegra_carveout_start;
	ardbeg_carveouts[1].size = tegra_carveout_size;
	ardbeg_carveouts[2].base = tegra_vpr_start;
	ardbeg_carveouts[2].size = tegra_vpr_size;
#ifdef CONFIG_NVMAP_USE_CMA_FOR_CARVEOUT
	carveout_linear_set(&tegra_generic_cma_dev);
	ardbeg_carveouts[1].cma_dev = &tegra_generic_cma_dev;
	ardbeg_carveouts[1].resize = false;
	carveout_linear_set(&tegra_vpr_cma_dev);
	ardbeg_carveouts[2].cma_dev = &tegra_vpr_cma_dev;
	ardbeg_carveouts[2].resize = true;
	ardbeg_carveouts[2].cma_chunk_size = SZ_32M;
#endif

	err = platform_device_register(&ardbeg_nvmap_device);
	if (err) {
		pr_err("nvmap device registration failed\n");
		return err;
	}
#endif

	phost1x = ardbeg_host1x_init();
	if (!phost1x) {
		pr_err("host1x devices registration failed\n");
		return -EINVAL;
	}

	if (!of_have_populated_dt() || !dc1_node ||
		!of_device_is_available(dc1_node)) {
#ifndef CONFIG_TEGRA_HDMI_PRIMARY
		res = platform_get_resource_byname(&ardbeg_disp1_device,
					 IORESOURCE_MEM, "fbmem");
#else
		res = platform_get_resource_byname(&ardbeg_disp2_device,
					 IORESOURCE_MEM, "fbmem");
#endif
		res->start = tegra_fb_start;
		res->end = tegra_fb_start + tegra_fb_size - 1;
	}

	/* Copy the bootloader fb to the fb. */
	if (tegra_bootloader_fb_size)
		__tegra_move_framebuffer(&ardbeg_nvmap_device,
				tegra_fb_start, tegra_bootloader_fb_start,
				min(tegra_fb_size, tegra_bootloader_fb_size));
	else
		__tegra_clear_framebuffer(&ardbeg_nvmap_device,
					  tegra_fb_start, tegra_fb_size);

#ifndef CONFIG_TEGRA_HDMI_PRIMARY
	if (!of_have_populated_dt() || !dc1_node ||
		!of_device_is_available(dc1_node)) {
		ardbeg_disp1_device.dev.parent = &phost1x->dev;
		err = platform_device_register(&ardbeg_disp1_device);
		if (err) {
			pr_err("disp1 device registration failed\n");
			return err;
		}
	}
#endif
	tegra_get_board_info(&board_info);
	if (board_info.board_id == BOARD_P1761) {
		ardbeg_hdmi_out.tmds_config = ardbeg_tn8_tmds_config;
	}

	if (!of_have_populated_dt() || !dc2_node ||
		!of_device_is_available(dc2_node)) {
#ifndef CONFIG_TEGRA_HDMI_PRIMARY
		res = platform_get_resource_byname(&ardbeg_disp2_device,
					IORESOURCE_MEM, "fbmem");
		res->start = tegra_fb2_start;
		res->end = tegra_fb2_start + tegra_fb2_size - 1;
#endif
		ardbeg_disp2_device.dev.parent = &phost1x->dev;
		err = platform_device_register(&ardbeg_disp2_device);
		if (err) {
			pr_err("disp2 device registration failed\n");
			return err;
		}
	}

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

int __init ardbeg_display_init(void)
{
	struct clk *disp1_clk = clk_get_sys("tegradc.0", NULL);
	struct clk *disp2_clk = clk_get_sys("tegradc.1", NULL);
	struct tegra_panel *panel;
	struct board_info board;
	long disp1_rate = 0;
	long disp2_rate = 0;

	/*
	 * TODO
	 * Need to skip ardbeg_display_init
	 * when disp is registered by device_tree
	 */

	if (WARN_ON(IS_ERR(disp1_clk))) {
		if (disp2_clk && !IS_ERR(disp2_clk))
			clk_put(disp2_clk);
		return PTR_ERR(disp1_clk);
	}

	if (WARN_ON(IS_ERR(disp2_clk))) {
		clk_put(disp1_clk);
		return PTR_ERR(disp1_clk);
	}

#ifndef CONFIG_TEGRA_HDMI_PRIMARY
	panel = ardbeg_panel_configure(&board, NULL);

	if (panel && panel->init_dc_out) {
		panel->init_dc_out(&ardbeg_disp1_out);
		if (ardbeg_disp1_out.n_modes && ardbeg_disp1_out.modes)
			disp1_rate = ardbeg_disp1_out.modes[0].pclk;
	} else {
		disp1_rate = 0;
		if (!panel || !panel->init_dc_out)
			printk(KERN_ERR "disp1 panel output not specified!\n");
	}

	printk(KERN_DEBUG "disp1 pclk=%ld\n", disp1_rate);
	if (disp1_rate)
		tegra_dvfs_resolve_override(disp1_clk, disp1_rate);
#endif

	/* set up disp2 */
	if (ardbeg_disp2_out.max_pixclock)
		disp2_rate = PICOS2KHZ(ardbeg_disp2_out.max_pixclock) * 1000;
	else
		disp2_rate = 297000000; /* HDMI 4K */
	printk(KERN_DEBUG "disp2 pclk=%ld\n", disp2_rate);
	if (disp2_rate)
#ifndef CONFIG_TEGRA_HDMI_PRIMARY
		tegra_dvfs_resolve_override(disp2_clk, disp2_rate);
#else
		tegra_dvfs_resolve_override(disp1_clk, disp2_rate);
#endif

	clk_put(disp1_clk);
	clk_put(disp2_clk);
	return 0;
}
