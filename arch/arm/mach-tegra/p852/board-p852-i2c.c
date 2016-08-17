/*
 * arch/arm/mach-tegra/board-p852-i2c.c
 *
 * Copyright (c) 2010-2011, NVIDIA Corporation.
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

#include <linux/resource.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/gpio.h>

#include <asm/mach-types.h>
#include <mach/irqs.h>
#include <mach/iomap.h>
#include <linux/i2c.h>
#include <mach/pinmux.h>
#include <asm/mach-types.h>

#include "board-p852.h"

static struct resource i2c_resource1[] = {
	[0] = {
	       .start = INT_I2C,
	       .end = INT_I2C,
	       .flags = IORESOURCE_IRQ,
	       },
	[1] = {
	       .start = TEGRA_I2C_BASE,
	       .end = TEGRA_I2C_BASE + TEGRA_I2C_SIZE - 1,
	       .flags = IORESOURCE_MEM,
	       },
};

static struct resource i2c_resource2[] = {
	[0] = {
	       .start = INT_I2C2,
	       .end = INT_I2C2,
	       .flags = IORESOURCE_IRQ,
	       },
	[1] = {
	       .start = TEGRA_I2C2_BASE,
	       .end = TEGRA_I2C2_BASE + TEGRA_I2C2_SIZE - 1,
	       .flags = IORESOURCE_MEM,
	       },
};

static struct resource i2c_resource3[] = {
	[0] = {
	       .start = INT_I2C3,
	       .end = INT_I2C3,
	       .flags = IORESOURCE_IRQ,
	       },
	[1] = {
	       .start = TEGRA_I2C3_BASE,
	       .end = TEGRA_I2C3_BASE + TEGRA_I2C3_SIZE - 1,
	       .flags = IORESOURCE_MEM,
	       },
};

static struct resource i2c_resource4[] = {
	[0] = {
	       .start = INT_DVC,
	       .end = INT_DVC,
	       .flags = IORESOURCE_IRQ,
	       },
	[1] = {
	       .start = TEGRA_DVC_BASE,
	       .end = TEGRA_DVC_BASE + TEGRA_DVC_SIZE - 1,
	       .flags = IORESOURCE_MEM,
	       },
};

static const struct tegra_pingroup_config i2c2_ddc = {
	.pingroup = TEGRA_PINGROUP_DDC,
	.func = TEGRA_MUX_I2C2,
};

static const struct tegra_pingroup_config i2c_i2cp = {
	.pingroup = TEGRA_PINGROUP_I2CP,
	.func = TEGRA_MUX_I2C,
};

static struct tegra_i2c_platform_data p852_i2c1_platform_data = {
	.adapter_nr = 0,
	.bus_count = 1,
	.bus_clk_rate = {400000},
};

static struct tegra_i2c_platform_data p852_i2c2_platform_data = {
	.adapter_nr = 1,
	.bus_count = 1,
	.bus_clk_rate = {100000},
	.bus_mux = {&i2c2_ddc},
	.bus_mux_len = {1},
};

static struct tegra_i2c_platform_data p852_i2c3_platform_data = {
	.adapter_nr = 2,
	.bus_count = 1,
	.bus_clk_rate = {400000},
};

static struct tegra_i2c_platform_data p852_dvc_platform_data = {
	.adapter_nr = 3,
	.bus_count = 1,
	.bus_clk_rate = {100000},
	.bus_mux = {&i2c_i2cp},
	.bus_mux_len = {1},
	.is_dvc = true,
};

struct platform_device tegra_i2c_device[] = {
	{
	 .name = "tegra-i2c",
	 .id = 0,
	 .resource = i2c_resource1,
	 .num_resources = ARRAY_SIZE(i2c_resource1),
	 .dev = {
		 .platform_data = &p852_i2c1_platform_data,
		 },
	 },
	{
	 .name = "tegra-i2c",
	 .id = 1,
	 .resource = i2c_resource2,
	 .num_resources = ARRAY_SIZE(i2c_resource2),
	 .dev = {
		 .platform_data = &p852_i2c2_platform_data,
		 },
	 },
	{
	 .name = "tegra-i2c",
	 .id = 2,
	 .resource = i2c_resource3,
	 .num_resources = ARRAY_SIZE(i2c_resource3),
	 .dev = {
		 .platform_data = &p852_i2c3_platform_data,
		 },
	 },
	{
	 .name = "tegra-i2c",
	 .id = 3,
	 .resource = i2c_resource4,
	 .num_resources = ARRAY_SIZE(i2c_resource4),
	 .dev = {
		 .platform_data = &p852_dvc_platform_data,
		 },
	 }
};

void __init p852_i2c_set_default_clock(int adapter, unsigned long clock)
{
	if (adapter >= 0 && adapter < ARRAY_SIZE(tegra_i2c_device))
		((struct tegra_i2c_platform_data *)tegra_i2c_device[adapter].
		 dev.platform_data)->bus_clk_rate[0] = clock;
}

void __init p852_i2c_init(void)
{
	int i;
	unsigned int i2c_config = 0;
	if (p852_sku_peripherals & P852_SKU_I2C_ENABLE) {
		for (i = 0; i < P852_MAX_I2C; i++) {
			i2c_config =
			    (p852_i2c_peripherals >> (P852_I2C_SHIFT * i));
			if (i2c_config & P852_I2C_ENABLE)
				platform_device_register(&tegra_i2c_device[i]);
		}
	}
}
