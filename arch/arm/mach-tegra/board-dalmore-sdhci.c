/*
 * arch/arm/mach-tegra/board-dalmore-sdhci.c
 *
 * Copyright (C) 2010 Google, Inc.
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/regulator/consumer.h>
#include <linux/mmc/host.h>
#include <linux/wl12xx.h>
#include <linux/platform_data/mmc-sdhci-tegra.h>

#include <asm/mach-types.h>
#include <mach/irqs.h>
#include <mach/gpio-tegra.h>

#include "tegra-board-id.h"
#include "gpio-names.h"
#include "board.h"
#include "board-dalmore.h"
#include "dvfs.h"
#include "iomap.h"

#define DALMORE_WLAN_PWR	TEGRA_GPIO_PCC5
#define DALMORE_WLAN_RST	TEGRA_GPIO_PX7
#define DALMORE_WLAN_WOW	TEGRA_GPIO_PU5
#define DALMORE_SD_CD		TEGRA_GPIO_PV2
#define DALMORE_SD_WP		TEGRA_GPIO_PQ4
static void (*wifi_status_cb)(int card_present, void *dev_id);
static void *wifi_status_cb_devid;
static int dalmore_wifi_status_register(void (*callback)(int , void *), void *);

static int dalmore_wifi_reset(int on);
static int dalmore_wifi_power(int on);
static int dalmore_wifi_set_carddetect(int val);

static struct wifi_platform_data dalmore_wifi_control = {
	.set_power	= dalmore_wifi_power,
	.set_reset	= dalmore_wifi_reset,
	.set_carddetect	= dalmore_wifi_set_carddetect,
};

static struct resource wifi_resource[] = {
	[0] = {
		.name	= "bcm4329_wlan_irq",
		.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL
				| IORESOURCE_IRQ_SHAREABLE,
	},
};

static struct platform_device dalmore_wifi_device = {
	.name		= "bcm4329_wlan",
	.id		= 1,
	.num_resources	= 1,
	.resource	= wifi_resource,
	.dev		= {
		.platform_data = &dalmore_wifi_control,
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
		.vendor	 = 0x02d0,
		.device	 = 0x4329,
	},
};
#endif

struct tegra_sdhci_platform_data dalmore_tegra_sdhci_platform_data0 = {
	.mmc_data = {
		.register_status_notify	= dalmore_wifi_status_register,
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
/*FIXME: Enable UHS modes for WiFI */
	.uhs_mask = MMC_UHS_MASK_DDR50 | MMC_UHS_MASK_SDR50 |
		MMC_UHS_MASK_SDR104 | MMC_UHS_MASK_SDR12 | MMC_UHS_MASK_SDR25,
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
	.cd_gpio = DALMORE_SD_CD,
	.wp_gpio = DALMORE_SD_WP,
	.power_gpio = -1,
	.tap_delay = 0x3,
	.trim_delay = 0x3,
	.ddr_clk_limit = 41000000,
	.max_clk_limit = 82000000,
	.uhs_mask = MMC_UHS_MASK_DDR50,
	.power_off_rail = true,
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
		.platform_data = &dalmore_tegra_sdhci_platform_data0,
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

static int dalmore_wifi_status_register(
		void (*callback)(int card_present, void *dev_id),
		void *dev_id)
{
	if (wifi_status_cb)
		return -EAGAIN;
	wifi_status_cb = callback;
	wifi_status_cb_devid = dev_id;
	return 0;
}

static int dalmore_wifi_set_carddetect(int val)
{
	pr_debug("%s: %d\n", __func__, val);
	if (wifi_status_cb)
		wifi_status_cb(val, wifi_status_cb_devid);
	else
		pr_warning("%s: Nobody to notify\n", __func__);
	return 0;
}

static struct regulator *dalmore_vdd_com_3v3;
static struct regulator *dalmore_vddio_com_1v8;
#define DALMORE_VDD_WIFI_3V3 "avdd"
#define DALMORE_VDD_WIFI_1V8 "dvdd"

static int dalmore_wifi_regulator_enable(void)
{
	int ret = 0;

	/* Enable COM's vdd_com_3v3 regulator*/
	if (IS_ERR_OR_NULL(dalmore_vdd_com_3v3)) {
		dalmore_vdd_com_3v3 = regulator_get(&dalmore_wifi_device.dev,
							DALMORE_VDD_WIFI_3V3);
		if (IS_ERR(dalmore_vdd_com_3v3)) {
			pr_err("Couldn't get regulator "
				DALMORE_VDD_WIFI_3V3 "\n");
			return PTR_ERR(dalmore_vdd_com_3v3);
		}

		ret = regulator_enable(dalmore_vdd_com_3v3);
		if (ret < 0) {
			pr_err("Couldn't enable regulator "
				DALMORE_VDD_WIFI_3V3 "\n");
			regulator_put(dalmore_vdd_com_3v3);
			dalmore_vdd_com_3v3 = NULL;
			return ret;
		}
	}

	/* Enable COM's vddio_com_1v8 regulator*/
	if (IS_ERR_OR_NULL(dalmore_vddio_com_1v8)) {
		dalmore_vddio_com_1v8 = regulator_get(&dalmore_wifi_device.dev,
			DALMORE_VDD_WIFI_1V8);
		if (IS_ERR(dalmore_vddio_com_1v8)) {
			pr_err("Couldn't get regulator "
				DALMORE_VDD_WIFI_1V8 "\n");
			regulator_disable(dalmore_vdd_com_3v3);

			regulator_put(dalmore_vdd_com_3v3);
			dalmore_vdd_com_3v3 = NULL;
			return PTR_ERR(dalmore_vddio_com_1v8);
		}

		ret = regulator_enable(dalmore_vddio_com_1v8);
		if (ret < 0) {
			pr_err("Couldn't enable regulator "
				DALMORE_VDD_WIFI_1V8 "\n");
			regulator_put(dalmore_vddio_com_1v8);
			dalmore_vddio_com_1v8 = NULL;

			regulator_disable(dalmore_vdd_com_3v3);
			regulator_put(dalmore_vdd_com_3v3);
			dalmore_vdd_com_3v3 = NULL;
			return ret;
		}
	}

	return ret;
}

static void dalmore_wifi_regulator_disable(void)
{
	/* Disable COM's vdd_com_3v3 regulator*/
	if (!IS_ERR_OR_NULL(dalmore_vdd_com_3v3)) {
		regulator_disable(dalmore_vdd_com_3v3);
		regulator_put(dalmore_vdd_com_3v3);
		dalmore_vdd_com_3v3 = NULL;
	}

	/* Disable COM's vddio_com_1v8 regulator*/
	if (!IS_ERR_OR_NULL(dalmore_vddio_com_1v8)) {
		regulator_disable(dalmore_vddio_com_1v8);
		regulator_put(dalmore_vddio_com_1v8);
		dalmore_vddio_com_1v8 = NULL;
	}
}

static int dalmore_wifi_power(int on)
{
	int ret = 0;

	pr_debug("%s: %d\n", __func__, on);
	/* Enable COM's regulators on wi-fi poer on*/
	if (on == 1) {
		ret = dalmore_wifi_regulator_enable();
		if (ret < 0) {
			pr_err("Failed to enable COM regulators\n");
			return ret;
		}
	}

	gpio_set_value(DALMORE_WLAN_PWR, on);
	mdelay(100);
	gpio_set_value(DALMORE_WLAN_RST, on);
	mdelay(200);

	/* Disable COM's regulators on wi-fi poer off*/
	if (on != 1) {
		pr_debug("Disabling COM regulators\n");
		dalmore_wifi_regulator_disable();
	}

	return ret;
}

static int dalmore_wifi_reset(int on)
{
	pr_debug("%s: do nothing\n", __func__);
	return 0;
}

static int __init dalmore_wifi_init(void)
{
	int rc;

	rc = gpio_request(DALMORE_WLAN_PWR, "wlan_power");
	if (rc)
		pr_err("WLAN_PWR gpio request failed:%d\n", rc);
	rc = gpio_request(DALMORE_WLAN_RST, "wlan_rst");
	if (rc)
		pr_err("WLAN_RST gpio request failed:%d\n", rc);
	rc = gpio_request(DALMORE_WLAN_WOW, "bcmsdh_sdmmc");
	if (rc)
		pr_err("WLAN_WOW gpio request failed:%d\n", rc);

	rc = gpio_direction_output(DALMORE_WLAN_PWR, 0);
	if (rc)
		pr_err("WLAN_PWR gpio direction configuration failed:%d\n", rc);
	gpio_direction_output(DALMORE_WLAN_RST, 0);
	if (rc)
		pr_err("WLAN_RST gpio direction configuration failed:%d\n", rc);
	rc = gpio_direction_input(DALMORE_WLAN_WOW);
	if (rc)
		pr_err("WLAN_WOW gpio direction configuration failed:%d\n", rc);

	wifi_resource[0].start = wifi_resource[0].end =
		gpio_to_irq(DALMORE_WLAN_WOW);

	platform_device_register(&dalmore_wifi_device);
	return 0;
}

#ifdef CONFIG_TEGRA_PREPOWER_WIFI
static int __init dalmore_wifi_prepower(void)
{
	if (!machine_is_dalmore())
		return 0;

	dalmore_wifi_power(1);

	return 0;
}

subsys_initcall_sync(dalmore_wifi_prepower);
#endif

int __init dalmore_sdhci_init(void)
{
	int nominal_core_mv;
	int min_vcore_override_mv;
	int boot_vcore_mv;
	struct board_info board_info;

	nominal_core_mv =
		tegra_dvfs_rail_get_nominal_millivolts(tegra_core_rail);
	if (nominal_core_mv) {
		dalmore_tegra_sdhci_platform_data0.nominal_vcore_mv =
			nominal_core_mv;
		tegra_sdhci_platform_data2.nominal_vcore_mv = nominal_core_mv;
		tegra_sdhci_platform_data3.nominal_vcore_mv = nominal_core_mv;
	}
	min_vcore_override_mv =
		tegra_dvfs_rail_get_override_floor(tegra_core_rail);
	if (min_vcore_override_mv) {
		dalmore_tegra_sdhci_platform_data0.min_vcore_override_mv =
			min_vcore_override_mv;
		tegra_sdhci_platform_data2.min_vcore_override_mv =
			min_vcore_override_mv;
		tegra_sdhci_platform_data3.min_vcore_override_mv =
			min_vcore_override_mv;
	}
	boot_vcore_mv = tegra_dvfs_rail_get_boot_level(tegra_core_rail);
	if (boot_vcore_mv) {
		dalmore_tegra_sdhci_platform_data0.boot_vcore_mv =
			boot_vcore_mv;
		tegra_sdhci_platform_data2.boot_vcore_mv = boot_vcore_mv;
		tegra_sdhci_platform_data3.boot_vcore_mv = boot_vcore_mv;
	}
	if ((tegra_sdhci_platform_data3.uhs_mask & MMC_MASK_HS200)
		&& (!(tegra_sdhci_platform_data3.uhs_mask &
		MMC_UHS_MASK_DDR50)))
		tegra_sdhci_platform_data3.trim_delay = 0;
	tegra_get_board_info(&board_info);
	if (board_info.fab == BOARD_FAB_A05) {
		tegra_sdhci_platform_data2.wp_gpio = -1;
		dalmore_tegra_sdhci_platform_data0.max_clk_limit = 156000000;
		tegra_sdhci_platform_data2.max_clk_limit = 156000000;
	}
	platform_device_register(&tegra_sdhci_device3);
	platform_device_register(&tegra_sdhci_device2);
	platform_device_register(&tegra_sdhci_device0);
	dalmore_wifi_init();
	return 0;
}
