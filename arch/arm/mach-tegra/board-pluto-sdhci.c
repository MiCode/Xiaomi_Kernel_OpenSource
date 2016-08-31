/*
 * arch/arm/mach-tegra/board-pluto-sdhci.c
 *
 * Copyright (c) 2012-2013 NVIDIA Corporation. All rights reserved.
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
#include <linux/wlan_plat.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/mmc/host.h>
#include <linux/wl12xx.h>
#include <linux/platform_data/mmc-sdhci-tegra.h>

#include "tegra-board-id.h"
#include <asm/mach-types.h>
#include <mach/irqs.h>
#include <mach/gpio-tegra.h>

#include "gpio-names.h"
#include "board.h"
#include "board-pluto.h"
#include "dvfs.h"
#include "iomap.h"

#define PLUTO_WLAN_PWR	TEGRA_GPIO_PCC5
#define PLUTO_WLAN_WOW	TEGRA_GPIO_PU5
#define PLUTO_SD_CD	TEGRA_GPIO_PV2
#define WLAN_PWR_STR	"wlan_power"
#define WLAN_WOW_STR	"bcmsdh_sdmmc"
#if defined(CONFIG_BCMDHD_EDP_SUPPORT)
/* Wifi power levels */
#define ON  1080 /* 1080 mW */
#define OFF 0
static unsigned int wifi_states[] = {ON, OFF};
#endif

static void (*wifi_status_cb)(int card_present, void *dev_id);
static void *wifi_status_cb_devid;
static int pluto_wifi_status_register(void (*callback)(int , void *), void *);

static int pluto_wifi_reset(int on);
static int pluto_wifi_power(int on);
static int pluto_wifi_set_carddetect(int val);

static struct wifi_platform_data pluto_wifi_control = {
	.set_power	= pluto_wifi_power,
	.set_reset	= pluto_wifi_reset,
	.set_carddetect	= pluto_wifi_set_carddetect,
};

static struct resource wifi_resource[] = {
	[0] = {
		.name	= "bcm4329_wlan_irq",
		.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL
				| IORESOURCE_IRQ_SHAREABLE,
	},
};

static struct platform_device pluto_wifi_device = {
	.name		= "bcm4329_wlan",
	.id		= 1,
	.num_resources	= 1,
	.resource	= wifi_resource,
	.dev		= {
		.platform_data = &pluto_wifi_control,
	},
};

#ifdef CONFIG_MMC_EMBEDDED_SDIO
static struct embedded_sdio_data embedded_sdio_data0 = {
	.cccr   = {
		.sdio_vsn       = 2,
		.multi_block    = 1,
		.low_speed      = 0,
		.wide_bus       = 0,
		.high_power     = 1,
		.high_speed     = 1,
	},
	.cis  = {
		.vendor         = 0x02d0,
		.device         = 0x4329,
	},
};
#endif

struct tegra_sdhci_platform_data pluto_tegra_sdhci_platform_data0 = {
	.mmc_data = {
		.register_status_notify	= pluto_wifi_status_register,
#ifdef CONFIG_MMC_EMBEDDED_SDIO
		.embedded_sdio = &embedded_sdio_data0,
#endif
		.built_in = 0,
		.ocr_mask = MMC_OCR_1V8_MASK,
	},
#ifndef CONFIG_MMC_EMBEDDED_SDIO
	.pm_flags = MMC_PM_KEEP_POWER,
#endif
	.cd_gpio = -1,
	.wp_gpio = -1,
	.power_gpio = -1,
	.tap_delay = 0x2,
	.trim_delay = 0x2,
	.ddr_clk_limit = 41000000,
	.max_clk_limit = 82000000,
	.uhs_mask = MMC_UHS_MASK_DDR50,
	.disable_clock_gate = true,
};

static struct resource sdhci_resource0[] = {
	[0] = {
		.start  = INT_SDMMC1,
		.end    = INT_SDMMC1,
		.flags  = IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_SDMMC1_BASE,
		.end	= TEGRA_SDMMC1_BASE + TEGRA_SDMMC1_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource sdhci_resource2[] = {
	[0] = {
		.start  = INT_SDMMC3,
		.end    = INT_SDMMC3,
		.flags  = IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_SDMMC3_BASE,
		.end	= TEGRA_SDMMC3_BASE + TEGRA_SDMMC3_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource sdhci_resource3[] = {
	[0] = {
		.start  = INT_SDMMC4,
		.end    = INT_SDMMC4,
		.flags  = IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_SDMMC4_BASE,
		.end	= TEGRA_SDMMC4_BASE + TEGRA_SDMMC4_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct tegra_sdhci_platform_data tegra_sdhci_platform_data2 = {
	.cd_gpio = PLUTO_SD_CD,
	.wp_gpio = -1,
	.power_gpio = -1,
	.tap_delay = 0x3,
	.trim_delay = 0x3,
	.ddr_clk_limit = 41000000,
	.max_clk_limit = 156000000,
	.uhs_mask = MMC_UHS_MASK_DDR50,
};

static struct tegra_sdhci_platform_data tegra_sdhci_platform_data3 = {
	.cd_gpio = -1,
	.wp_gpio = -1,
	.power_gpio = -1,
	.is_8bit = 1,
	.tap_delay = 0x5,
	.trim_delay = 0xA,
	.ddr_trim_delay = -1,
	.ddr_clk_limit = 41000000,
	.max_clk_limit = 156000000,
	.mmc_data = {
		.built_in = 1,
		.ocr_mask = MMC_OCR_1V8_MASK,
	}
};

static struct platform_device tegra_sdhci_device0 = {
	.name		= "sdhci-tegra",
	.id		= 0,
	.resource	= sdhci_resource0,
	.num_resources	= ARRAY_SIZE(sdhci_resource0),
	.dev = {
		.platform_data = &pluto_tegra_sdhci_platform_data0,
	},
};

static struct platform_device tegra_sdhci_device2 = {
	.name		= "sdhci-tegra",
	.id		= 2,
	.resource	= sdhci_resource2,
	.num_resources	= ARRAY_SIZE(sdhci_resource2),
	.dev = {
		.platform_data = &tegra_sdhci_platform_data2,
	},
};

static struct platform_device tegra_sdhci_device3 = {
	.name		= "sdhci-tegra",
	.id		= 3,
	.resource	= sdhci_resource3,
	.num_resources	= ARRAY_SIZE(sdhci_resource3),
	.dev = {
		.platform_data = &tegra_sdhci_platform_data3,
	},
};

static int pluto_wifi_status_register(
		void (*callback)(int card_present, void *dev_id),
		void *dev_id)
{
	if (wifi_status_cb)
		return -EAGAIN;
	wifi_status_cb = callback;
	wifi_status_cb_devid = dev_id;
	return 0;
}

static int pluto_wifi_set_carddetect(int val)
{
	pr_debug("%s: %d\n", __func__, val);
	if (wifi_status_cb)
		wifi_status_cb(val, wifi_status_cb_devid);
	else
		pr_warning("%s: Nobody to notify\n", __func__);
	return 0;
}

static int pluto_wifi_power(int on)
{
	pr_debug("%s: %d\n", __func__, on);

	gpio_set_value(PLUTO_WLAN_PWR, on);
	mdelay(100);

	return 0;
}

static int pluto_wifi_reset(int on)
{
	pr_debug("%s: do nothing\n", __func__);
	return 0;
}

static int __init pluto_wifi_init(void)
{
	int rc = 0;

	/* init wlan_pwr gpio */
	rc = gpio_request(PLUTO_WLAN_PWR, WLAN_PWR_STR);
	/* Due to pre powering, sometimes gpio req returns EBUSY */
	if ((rc < 0) && (rc != -EBUSY)) {
		pr_err("Wifi init: gpio req failed:%d\n", rc);
		return rc;
	}

	/* Due to pre powering, sometimes gpio req returns EBUSY */
	rc = gpio_direction_output(PLUTO_WLAN_PWR, 0);
	if ((rc < 0) && (rc != -EBUSY)) {
		gpio_free(PLUTO_WLAN_PWR);
		return rc;
	}
	/* init wlan_wow gpio */
	rc = gpio_request(PLUTO_WLAN_WOW, WLAN_WOW_STR);
	if (rc < 0) {
		pr_err("wifi init: gpio req failed:%d\n", rc);
		gpio_free(PLUTO_WLAN_PWR);
		return rc;
	}

	rc = gpio_direction_input(PLUTO_WLAN_WOW);
	if (rc < 0) {
		gpio_free(PLUTO_WLAN_WOW);
		gpio_free(PLUTO_WLAN_PWR);
		return rc;
	}

	wifi_resource[0].start = wifi_resource[0].end =
		gpio_to_irq(PLUTO_WLAN_WOW);

	platform_device_register(&pluto_wifi_device);
	return rc;
}

#ifdef CONFIG_TEGRA_PREPOWER_WIFI
static int __init pluto_wifi_prepower(void)
{
	if (!machine_is_tegra_pluto())
		return 0;

	pluto_wifi_power(1);

	return 0;
}

subsys_initcall_sync(pluto_wifi_prepower);
#endif

int __init pluto_sdhci_init(void)
{
	int nominal_core_mv;
	int min_vcore_override_mv;
	int boot_vcore_mv;

	nominal_core_mv =
		tegra_dvfs_rail_get_nominal_millivolts(tegra_core_rail);
	if (nominal_core_mv > 0) {
		pluto_tegra_sdhci_platform_data0.nominal_vcore_mv =
			nominal_core_mv;
		tegra_sdhci_platform_data2.nominal_vcore_mv = nominal_core_mv;
		tegra_sdhci_platform_data3.nominal_vcore_mv = nominal_core_mv;
	}
	min_vcore_override_mv =
		tegra_dvfs_rail_get_override_floor(tegra_core_rail);
	if (min_vcore_override_mv) {
		pluto_tegra_sdhci_platform_data0.min_vcore_override_mv =
			min_vcore_override_mv;
		tegra_sdhci_platform_data2.min_vcore_override_mv =
			min_vcore_override_mv;
		tegra_sdhci_platform_data3.min_vcore_override_mv =
			min_vcore_override_mv;
	}
	boot_vcore_mv = tegra_dvfs_rail_get_boot_level(tegra_core_rail);
	if (boot_vcore_mv) {
		pluto_tegra_sdhci_platform_data0.boot_vcore_mv = boot_vcore_mv;
		tegra_sdhci_platform_data2.boot_vcore_mv = boot_vcore_mv;
		tegra_sdhci_platform_data3.boot_vcore_mv = boot_vcore_mv;
	}

	if ((tegra_sdhci_platform_data3.uhs_mask & MMC_MASK_HS200)
		&& (!(tegra_sdhci_platform_data3.uhs_mask &
		MMC_UHS_MASK_DDR50)))
		tegra_sdhci_platform_data3.trim_delay = 0;
	platform_device_register(&tegra_sdhci_device3);
	platform_device_register(&tegra_sdhci_device2);
	platform_device_register(&tegra_sdhci_device0);
	pluto_wifi_init();
	return 0;
}
