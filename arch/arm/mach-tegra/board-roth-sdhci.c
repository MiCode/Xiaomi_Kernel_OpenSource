/*
 * arch/arm/mach-tegra/board-roth-sdhci.c
 *
 * Copyright (C) 2010 Google, Inc.
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
#include <linux/regulator/consumer.h>
#include <linux/mmc/host.h>
#include <linux/wl12xx.h>

#include <asm/mach-types.h>
#include <mach/irqs.h>
#include <mach/iomap.h>
#include <mach/sdhci.h>
#include<mach/gpio-tegra.h>
#include <mach/io_dpd.h>

#include "gpio-names.h"
#include "board.h"
#include "board-roth.h"
#include "dvfs.h"

#define ROTH_WLAN_PWR	TEGRA_GPIO_PCC5
#define ROTH_WLAN_RST	TEGRA_GPIO_INVALID
#define ROTH_WLAN_WOW	TEGRA_GPIO_PU5
#define ROTH_SD_CD		TEGRA_GPIO_PV2
#define WLAN_PWR_STR	"wlan_power"
#define WLAN_WOW_STR	"bcmsdh_sdmmc"

static void (*wifi_status_cb)(int card_present, void *dev_id);
static void *wifi_status_cb_devid;
static int roth_wifi_status_register(void (*callback)(int , void *), void *);

static int roth_wifi_reset(int on);
static int roth_wifi_power(int on);
static int roth_wifi_set_carddetect(int val);

static struct wifi_platform_data roth_wifi_control = {
	.set_power	= roth_wifi_power,
	.set_reset	= roth_wifi_reset,
	.set_carddetect	= roth_wifi_set_carddetect,
};

static struct resource wifi_resource[] = {
	[0] = {
		.name	= "bcm4329_wlan_irq",
		.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL
				| IORESOURCE_IRQ_SHAREABLE,
	},
};

static struct platform_device roth_wifi_device = {
	.name		= "bcm4329_wlan",
	.id		= 1,
	.num_resources	= 1,
	.resource	= wifi_resource,
	.dev		= {
		.platform_data = &roth_wifi_control,
	},
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
		.device	 = 0x4324,
	},
};
#endif

static struct tegra_sdhci_platform_data tegra_sdhci_platform_data0 = {
	.mmc_data = {
		.register_status_notify	= roth_wifi_status_register,
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
	.max_clk_limit = 156000000,
	.uhs_mask = MMC_UHS_MASK_DDR50,
};

static struct tegra_sdhci_platform_data tegra_sdhci_platform_data2 = {
	.cd_gpio = ROTH_SD_CD,
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
		.platform_data = &tegra_sdhci_platform_data0,
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

static int roth_wifi_status_register(
		void (*callback)(int card_present, void *dev_id),
		void *dev_id)
{
	if (wifi_status_cb)
		return -EAGAIN;
	wifi_status_cb = callback;
	wifi_status_cb_devid = dev_id;
	return 0;
}

static int roth_wifi_set_carddetect(int val)
{
	pr_debug("%s: %d\n", __func__, val);
	if (wifi_status_cb)
		wifi_status_cb(val, wifi_status_cb_devid);
	else
		pr_warning("%s: Nobody to notify\n", __func__);
	return 0;
}

static struct regulator *roth_vdd_com_3v3;
static struct regulator *roth_vddio_com_1v8;
#define ROTH_VDD_WIFI_3V3 "vdd_wl_pa"
#define ROTH_VDD_WIFI_1V8 "vddio"


static int roth_wifi_regulator_enable(void)
{
	int ret = 0;

	/* Enable COM's vdd_com_3v3 regulator*/
	if (IS_ERR_OR_NULL(roth_vdd_com_3v3)) {
		roth_vdd_com_3v3 = regulator_get(&roth_wifi_device.dev,
					ROTH_VDD_WIFI_3V3);
		if (IS_ERR_OR_NULL(roth_vdd_com_3v3)) {
			pr_err("Couldn't get regulator "
				ROTH_VDD_WIFI_3V3 "\n");
			return PTR_ERR(roth_vdd_com_3v3);
		}

		ret = regulator_enable(roth_vdd_com_3v3);
		if (ret < 0) {
			pr_err("Couldn't enable regulator "
				ROTH_VDD_WIFI_3V3 "\n");
			regulator_put(roth_vdd_com_3v3);
			roth_vdd_com_3v3 = NULL;
			return ret;
		}
	}

	/* Enable COM's vddio_com_1v8 regulator*/
	if (IS_ERR_OR_NULL(roth_vddio_com_1v8)) {
		roth_vddio_com_1v8 = regulator_get(&roth_wifi_device.dev,
			ROTH_VDD_WIFI_1V8);
		if (IS_ERR_OR_NULL(roth_vddio_com_1v8)) {
			pr_err("Couldn't get regulator "
				ROTH_VDD_WIFI_1V8 "\n");
			regulator_disable(roth_vdd_com_3v3);

			regulator_put(roth_vdd_com_3v3);
			roth_vdd_com_3v3 = NULL;
			return PTR_ERR(roth_vddio_com_1v8);
		}

		ret = regulator_enable(roth_vddio_com_1v8);
		if (ret < 0) {
			pr_err("Couldn't enable regulator "
				ROTH_VDD_WIFI_1V8 "\n");
			regulator_put(roth_vddio_com_1v8);
			roth_vddio_com_1v8 = NULL;

			regulator_disable(roth_vdd_com_3v3);
			regulator_put(roth_vdd_com_3v3);
			roth_vdd_com_3v3 = NULL;
			return ret;
		}
	}

	return ret;
}

static void roth_wifi_regulator_disable(void)
{
	/* Disable COM's vdd_com_3v3 regulator*/
	if (!IS_ERR_OR_NULL(roth_vdd_com_3v3)) {
		regulator_disable(roth_vdd_com_3v3);
		regulator_put(roth_vdd_com_3v3);
		roth_vdd_com_3v3 = NULL;
	}

	/* Disable COM's vddio_com_1v8 regulator*/
	if (!IS_ERR_OR_NULL(roth_vddio_com_1v8)) {
		regulator_disable(roth_vddio_com_1v8);
		regulator_put(roth_vddio_com_1v8);
		roth_vddio_com_1v8 = NULL;
	}
}

static int roth_wifi_power(int on)
{
	struct tegra_io_dpd *sd_dpd;
	int ret = 0;

	pr_debug("%s: %d\n", __func__, on);
	/* Enable COM's regulators on wi-fi poer on*/
	if (on == 1) {
		ret = roth_wifi_regulator_enable();
		if (ret < 0) {
			pr_err("Failed to enable COM regulators\n");
			return ret;
		}
	}

	/*
	 * FIXME : we need to revisit IO DPD code
	 * on how should multiple pins under DPD get controlled
	 *
	 * roth GPIO WLAN enable is part of SDMMC3 pin group
	 */
	sd_dpd = tegra_io_dpd_get(&tegra_sdhci_device2.dev);
	if (sd_dpd) {
		mutex_lock(&sd_dpd->delay_lock);
		tegra_io_dpd_disable(sd_dpd);
		mutex_unlock(&sd_dpd->delay_lock);
	}
	gpio_set_value(ROTH_WLAN_PWR, on);
	mdelay(100);

	if (sd_dpd) {
		mutex_lock(&sd_dpd->delay_lock);
		tegra_io_dpd_enable(sd_dpd);
		mutex_unlock(&sd_dpd->delay_lock);
	}

	/* Disable COM's regulators on wi-fi poer off*/
	if (on != 1) {
		pr_debug("Disabling COM regulators\n");
		roth_wifi_regulator_disable();
	}

	return ret;
}

static int roth_wifi_reset(int on)
{
	pr_debug("%s: do nothing\n", __func__);
	return 0;
}

static int __init roth_wifi_init(void)
{
	int rc = 0;

	/* init wlan_pwr gpio */
	rc = gpio_request(ROTH_WLAN_PWR, WLAN_PWR_STR);
	/* Due to pre-init, during first time boot,
	 * gpio request returns -EBUSY
	 */
	if ((rc < 0) && (rc != -EBUSY)) {
		pr_err("gpio req failed:%d\n", rc);
		return rc;
	}

	rc = gpio_direction_output(ROTH_WLAN_PWR, 0);
	if ((rc < 0) && (rc != -EBUSY)) {
		gpio_free(ROTH_WLAN_PWR);
		return rc;
	}

	/* init wlan_wow gpio */
	rc = gpio_request(ROTH_WLAN_WOW, WLAN_WOW_STR);
	if (rc) {
		pr_err("gpio req failed:%d\n", rc);
		gpio_free(ROTH_WLAN_PWR);
		return rc;
	}

	rc = gpio_direction_input(ROTH_WLAN_WOW);
	if (rc) {
		gpio_free(ROTH_WLAN_WOW);
		gpio_free(ROTH_WLAN_PWR);
		return rc;
	}

	wifi_resource[0].start = wifi_resource[0].end =
		gpio_to_irq(ROTH_WLAN_WOW);

	platform_device_register(&roth_wifi_device);
	return rc;
}

#ifdef CONFIG_TEGRA_PREPOWER_WIFI
static int __init roth_wifi_prepower(void)
{
	if (!machine_is_roth())
		return 0;

	roth_wifi_power(1);

	return 0;
}

subsys_initcall_sync(roth_wifi_prepower);
#endif

int __init roth_sdhci_init(void)
{
	int nominal_core_mv;
	int min_vcore_override_mv;

	nominal_core_mv =
		tegra_dvfs_rail_get_nominal_millivolts(tegra_core_rail);
	if (nominal_core_mv > 0) {
		tegra_sdhci_platform_data0.nominal_vcore_mv = nominal_core_mv;
		tegra_sdhci_platform_data2.nominal_vcore_mv = nominal_core_mv;
		tegra_sdhci_platform_data3.nominal_vcore_mv = nominal_core_mv;
	}
	min_vcore_override_mv =
		tegra_dvfs_rail_get_override_floor(tegra_core_rail);
	if (min_vcore_override_mv) {
		tegra_sdhci_platform_data0.min_vcore_override_mv =
			min_vcore_override_mv;
		tegra_sdhci_platform_data2.min_vcore_override_mv =
			min_vcore_override_mv;
		tegra_sdhci_platform_data3.min_vcore_override_mv =
			min_vcore_override_mv;
	}

	if ((tegra_sdhci_platform_data3.uhs_mask & MMC_MASK_HS200)
		&& (!(tegra_sdhci_platform_data3.uhs_mask &
		MMC_UHS_MASK_DDR50)))
		tegra_sdhci_platform_data3.trim_delay = 0;

	platform_device_register(&tegra_sdhci_device3);
	platform_device_register(&tegra_sdhci_device2);
	platform_device_register(&tegra_sdhci_device0);
	roth_wifi_init();
	return 0;
}
