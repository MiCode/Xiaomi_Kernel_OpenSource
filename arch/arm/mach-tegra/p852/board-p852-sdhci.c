/*
 * arch/arm/mach-tegra/board-p852-sdhci.c
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
#include <mach/sdhci.h>
#include <mach/pinmux.h>
#include <asm/mach-types.h>

#include "board-p852.h"

static struct resource sdhci_resource1[] = {
	[0] = {
	       .start = INT_SDMMC1,
	       .end = INT_SDMMC1,
	       .flags = IORESOURCE_IRQ,
	       },
	[1] = {
	       .start = TEGRA_SDMMC1_BASE,
	       .end = TEGRA_SDMMC1_BASE + TEGRA_SDMMC1_SIZE - 1,
	       .flags = IORESOURCE_MEM,
	       },
};

static struct resource sdhci_resource2[] = {
	[0] = {
	       .start = INT_SDMMC2,
	       .end = INT_SDMMC2,
	       .flags = IORESOURCE_IRQ,
	       },
	[1] = {
	       .start = TEGRA_SDMMC2_BASE,
	       .end = TEGRA_SDMMC2_BASE + TEGRA_SDMMC2_SIZE - 1,
	       .flags = IORESOURCE_MEM,
	       },
};

static struct resource sdhci_resource3[] = {
	[0] = {
	       .start = INT_SDMMC3,
	       .end = INT_SDMMC3,
	       .flags = IORESOURCE_IRQ,
	       },
	[1] = {
	       .start = TEGRA_SDMMC3_BASE,
	       .end = TEGRA_SDMMC3_BASE + TEGRA_SDMMC3_SIZE - 1,
	       .flags = IORESOURCE_MEM,
	       },
};

static struct resource sdhci_resource4[] = {
	[0] = {
	       .start = INT_SDMMC4,
	       .end = INT_SDMMC4,
	       .flags = IORESOURCE_IRQ,
	       },
	[1] = {
	       .start = TEGRA_SDMMC4_BASE,
	       .end = TEGRA_SDMMC4_BASE + TEGRA_SDMMC4_SIZE - 1,
	       .flags = IORESOURCE_MEM,
	       },
};

struct tegra_sdhci_platform_data p852_sdhci_platform_data[] = {
	{
	 .cd_gpio = -1,
	 .wp_gpio = -1,
	 .power_gpio = -1,
	 },
	{
	 .cd_gpio = -1,
	 .wp_gpio = -1,
	 .power_gpio = -1,
	 },
	{
	 .cd_gpio = -1,
	 .wp_gpio = -1,
	 .power_gpio = -1,
	 },
	{
	 .cd_gpio = -1,
	 .wp_gpio = -1,
	 .power_gpio = -1,
	 },
};

static struct platform_device tegra_sdhci_device[] = {
	{
	 .name = "sdhci-tegra",
	 .id = 0,
	 .resource = sdhci_resource1,
	 .num_resources = ARRAY_SIZE(sdhci_resource1),
	 .dev = {
		 .platform_data = &p852_sdhci_platform_data[0],
		 },
	 },
	{
	 .name = "sdhci-tegra",
	 .id = 1,
	 .resource = sdhci_resource2,
	 .num_resources = ARRAY_SIZE(sdhci_resource2),
	 .dev = {
		 .platform_data = &p852_sdhci_platform_data[1],
		 },
	 },
	{
	 .name = "sdhci-tegra",
	 .id = 2,
	 .resource = sdhci_resource3,
	 .num_resources = ARRAY_SIZE(sdhci_resource3),
	 .dev = {
		 .platform_data = &p852_sdhci_platform_data[2],
		 },
	 },
	{
	 .name = "sdhci-tegra",
	 .id = 3,
	 .resource = sdhci_resource4,
	 .num_resources = ARRAY_SIZE(sdhci_resource4),
	 .dev = {
		 .platform_data = &p852_sdhci_platform_data[3],
		 },
	 },

};

void __init p852_sdhci_init(void)
{

	int i, count = 10;
	int cd = 0, wp = 0, pw = 0;
	static char gpio_name[12][10];
	unsigned int sdhci_config = 0;

	if (p852_sku_peripherals & P852_SKU_SDHCI_ENABLE)
		for (i = 0; i < P852_MAX_SDHCI; i++) {
			sdhci_config =
			    (p852_sdhci_peripherals >> (P852_SDHCI_SHIFT * i));
			cd = i * 3;
			wp = cd + 1;
			pw = wp + 1;
			if (sdhci_config & P852_SDHCI_ENABLE) {
				if (sdhci_config & P852_SDHCI_CD_EN) {
					snprintf(gpio_name[cd], count,
						 "sdhci%d_cd", i);
					gpio_request(p852_sdhci_platform_data
						     [i].cd_gpio,
						     gpio_name[cd]);
					gpio_direction_input
					    (p852_sdhci_platform_data[i].
					     cd_gpio);
				}

				if (sdhci_config & P852_SDHCI_WP_EN) {
					snprintf(gpio_name[wp], count,
						 "sdhci%d_wp", i);
					gpio_request(p852_sdhci_platform_data
						     [i].wp_gpio,
						     gpio_name[wp]);
					gpio_direction_input
					    (p852_sdhci_platform_data[i].
					     wp_gpio);
				}

				if (sdhci_config & P852_SDHCI_PW_EN) {
					snprintf(gpio_name[pw], count,
						 "sdhci%d_pw", i);
					gpio_request(p852_sdhci_platform_data
						     [i].power_gpio,
						     gpio_name[pw]);
					gpio_direction_input
					    (p852_sdhci_platform_data[i].
					     power_gpio);
				}

				platform_device_register(&tegra_sdhci_device
							 [i]);
			}
		}
}
