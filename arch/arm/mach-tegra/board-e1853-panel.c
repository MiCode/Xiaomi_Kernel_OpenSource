/*
 * arch/arm/mach-tegra/board-e1853-panel.c
 *
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
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
 *
 */

#include <linux/resource.h>
#include <asm/mach-types.h>
#include <linux/platform_device.h>
#include <linux/nvhost.h>
#include <linux/nvmap.h>

#include <mach/irqs.h>
#include <mach/iomap.h>
#include <mach/dc.h>
#include <mach/fb.h>

#include "board.h"
#include "devices.h"
#include "tegra3_host1x_devices.h"
#include "gpio-names.h"

#define E1853_HDMI_HPD TEGRA_GPIO_PB2
#define E1853_LVDS_SER_ADDR  0xd

#define LVDS_SER_REG_CONFIG_1                   0x4
#define LVDS_SER_REG_CONFIG_1_BKWD_OVERRIDE     3
#define LVDS_SER_REG_CONFIG_1_BKWD              2

#define LVDS_SER_REG_DATA_PATH_CTRL             0x12
#define LVDS_SER_REG_DATA_PATH_CTRL_PASS_RGB    6

#define LVDS_SER_REG_CONFIG_0                   0x3
#define LVDS_SER_REG_CONFIG_0_TRFB              0

static int ser_i2c_read(struct i2c_client *client,
						u8 reg_addr, u8 *pval)
{
	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &reg_addr,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = 1,
			.buf = pval,
		},
	};

	return i2c_transfer(client->adapter, msg, 2);
}

static int ser_i2c_write(struct i2c_client *client,
						u8 reg_addr, u8 val)
{
	u8 buffer[] = {reg_addr, val};
	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 2,
			.buf = buffer,
		},
	};

	return i2c_transfer(client->adapter, msg, 1);
}

static int lvds_ser_init(struct i2c_client *client,
						bool is_fpdlinkII,
						bool support_hdcp,
						bool clk_rise_edge)
{
	u8 val;
	int err = 0;

	if (is_fpdlinkII) {
		err = ser_i2c_read(client, LVDS_SER_REG_CONFIG_1, &val);
		if (err < 0)
			return err;

		val |= (1 << LVDS_SER_REG_CONFIG_1_BKWD_OVERRIDE);
		val |= (1 << LVDS_SER_REG_CONFIG_1_BKWD);

		err = ser_i2c_write(client, LVDS_SER_REG_CONFIG_1, val);
		if (err < 0)
			return err;
	} else if (!support_hdcp) {
		err = ser_i2c_read(client, LVDS_SER_REG_DATA_PATH_CTRL, &val);
		if (err < 0)
			return err;

		val |= (1 << LVDS_SER_REG_DATA_PATH_CTRL_PASS_RGB);

		err = ser_i2c_write(client, LVDS_SER_REG_DATA_PATH_CTRL, val);
		if (err < 0)
			return err;
	}

	if (clk_rise_edge) {
		err = ser_i2c_read(client, LVDS_SER_REG_CONFIG_0, &val);
		if (err < 0)
			return err;

		val |= (1 << LVDS_SER_REG_CONFIG_0_TRFB);

		err = ser_i2c_write(client, LVDS_SER_REG_CONFIG_0, val);
	}

	return (err < 0 ? err : 0);
}

/* enable primary LVDS */
static int e1853_lvds_enable(struct device *dev)
{
	struct i2c_adapter *adapter;
	struct i2c_board_info info = { {0} };
	static struct i2c_client *client;
	int err = -1;

	/* Program the serializer */
	if (!client) {
		adapter = i2c_get_adapter(1);
		if (!adapter)
			pr_warning("%s: adapter is null\n", __func__);
		else {
			info.addr = E1853_LVDS_SER_ADDR;
			client = i2c_new_device(adapter, &info);
			i2c_put_adapter(adapter);
		}
	}

	if (!client)
		pr_warning("%s: client is null\n", __func__);
	else {
		err = lvds_ser_init(client,
					true,  /* is_fpdlinkII*/
					false, /* support_hdcp */
					true); /* clk_rise_edge */
		if (err)
			pr_warning("%s: lvds failed\n", __func__);
	}

	return err;
}

/* Disable primary LVDS */
static int e1853_lvds_disable(void)
{
	/* Turn off serializer chip */

	return 0;
}

#ifdef CONFIG_TOUCHSCREEN_ATMEL_MXT

static struct tegra_dc_mode e1853_panel_modes[] = {
	{
		/* 1366x768@60Hz */
		.pclk = 74180000,
		.h_ref_to_sync = 1,
		.v_ref_to_sync = 1,
		.h_sync_width = 30,
		.v_sync_width = 5,
		.h_back_porch = 52,
		.v_back_porch = 20,
		.h_active = 1366,
		.v_active = 768,
		.h_front_porch = 64,
		.v_front_porch = 25,
	},
};

static struct tegra_fb_data e1853_fb_data = {
	.win        = 0,
	.xres       = 1366,
	.yres       = 768,
	.bits_per_pixel = 32,
};

#else

static struct tegra_dc_mode e1853_panel_modes[] = {
	{
		/* 800x480@60 */
		.pclk = 32460000,
		.h_ref_to_sync = 1,
		.v_ref_to_sync = 1,
		.h_sync_width = 64,
		.v_sync_width = 3,
		.h_back_porch = 128,
		.v_back_porch = 22,
		.h_front_porch = 64,
		.v_front_porch = 20,
		.h_active = 800,
		.v_active = 480,
	},
};

static struct tegra_fb_data e1853_fb_data = {
	.win		= 0,
	.xres		= 800,
	.yres		= 480,
	.bits_per_pixel	= 32,
};

#endif

static struct tegra_dc_out e1853_ser_out = {
	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,
	.parent_clk	= "pll_d_out0",
	.type		= TEGRA_DC_OUT_RGB,
	.modes		= e1853_panel_modes,
	.n_modes	= ARRAY_SIZE(e1853_panel_modes),
	.enable		= e1853_lvds_enable,
	.disable	= e1853_lvds_disable,
};

static struct tegra_dc_platform_data e1853_disp1_pdata = {
	.flags		= TEGRA_DC_FLAG_ENABLED,
	.default_out	= &e1853_ser_out,
	.emc_clk_rate	= 300000000,
	.fb		= &e1853_fb_data,
};

static int e1853_hdmi_enable(struct device *dev)
{
	return 0;
}

static int e1853_hdmi_disable(void)
{
	return 0;
}

static struct tegra_fb_data e1853_hdmi_fb_data = {
	.win            = 0,
	.xres           = 800,
	.yres           = 480,
	.bits_per_pixel = 32,
	.flags          = TEGRA_FB_FLIP_ON_PROBE,
};

static struct tegra_dc_out e1853_hdmi_out = {
	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,
	.parent_clk     = "pll_d2_out0",
	.type		= TEGRA_DC_OUT_HDMI,
	.flags          = TEGRA_DC_OUT_HOTPLUG_LOW |
			  TEGRA_DC_OUT_NVHDCP_POLICY_ON_DEMAND,
	.max_pixclock   = KHZ2PICOS(148500),
	.hotplug_gpio   = E1853_HDMI_HPD,
	.enable		= e1853_hdmi_enable,
	.disable	= e1853_hdmi_disable,
	.dcc_bus        = 3,
};

static struct tegra_dc_platform_data e1853_hdmi_pdata = {
	.flags           = 0,
	.default_out     = &e1853_hdmi_out,
	.emc_clk_rate    = 300000000,
	.fb              = &e1853_hdmi_fb_data,
};

static struct nvmap_platform_carveout e1853_carveouts[] = {
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
		.base		= 0,	/* Filled in by e1853_panel_init() */
		.size		= 0,	/* Filled in by e1853_panel_init() */
		.buddy_size	= SZ_32K,
	},
};

static struct nvmap_platform_data e1853_nvmap_data = {
	.carveouts	= e1853_carveouts,
	.nr_carveouts	= ARRAY_SIZE(e1853_carveouts),
};

static struct platform_device *e1853_gfx_devices[] __initdata = {
	&tegra_nvmap_device,
};

int __init e1853_panel_init(void)
{
	int err;
#if defined(CONFIG_TEGRA_GRHOST) && defined(CONFIG_TEGRA_DC)
	struct resource *res;
	struct platform_device *phost1x;
#endif

	e1853_carveouts[1].base = tegra_carveout_start;
	e1853_carveouts[1].size = tegra_carveout_size;
	tegra_nvmap_device.dev.platform_data = &e1853_nvmap_data;
	tegra_disp1_device.dev.platform_data = &e1853_disp1_pdata;
	tegra_disp2_device.dev.platform_data = &e1853_hdmi_pdata;

	err = platform_add_devices(e1853_gfx_devices,
		ARRAY_SIZE(e1853_gfx_devices));

#ifdef CONFIG_TEGRA_GRHOST
	phost1x = tegra3_register_host1x_devices();
	if (!phost1x)
		return -EINVAL;
#endif

#if defined(CONFIG_TEGRA_GRHOST) && defined(CONFIG_TEGRA_DC)
	res = platform_get_resource_byname(&tegra_disp1_device,
					 IORESOURCE_MEM, "fbmem");
	if (res) {
		res->start = tegra_fb_start;
		res->end = tegra_fb_start + tegra_fb_size - 1;
	}

	if (!err) {
		tegra_disp1_device.dev.parent = &phost1x->dev;
		err = platform_device_register(&tegra_disp1_device);
	}

	res = platform_get_resource_byname(&tegra_disp2_device,
					 IORESOURCE_MEM, "fbmem");
	if (res) {
		res->start = tegra_fb2_start;
		res->end = tegra_fb2_start + tegra_fb2_size - 1;
	}

	if (!err) {
		tegra_disp2_device.dev.parent = &phost1x->dev;
		err = platform_device_register(&tegra_disp2_device);
	}
#endif

#if defined(CONFIG_TEGRA_GRHOST) && defined(CONFIG_TEGRA_NVAVP)
	if (!err) {
		nvavp_device.dev.parent = &phost1x->dev;
		err = platform_device_register(&nvavp_device);
	}
#endif
	return err;
}
