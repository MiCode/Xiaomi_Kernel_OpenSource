/*
 * arch/arm/mach-tegra/board-loki-sdhci.c
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/resource.h>
#include <linux/platform_device.h>
#include <linux/wlan_plat.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/mmc/host.h>
#include <linux/wl12xx.h>
#include <linux/platform_data/mmc-sdhci-tegra.h>
#include <linux/mfd/max77660/max77660-core.h>
#include <linux/tegra-fuse.h>

#include <asm/mach-types.h>
#include <mach/irqs.h>
#include <mach/gpio-tegra.h>

#include "gpio-names.h"
#include "board.h"
#include "board-loki.h"
#include "iomap.h"
#include "dvfs.h"
#include "tegra-board-id.h"

#define LOKI_WLAN_RST	TEGRA_GPIO_PR3
#define LOKI_WLAN_PWR	TEGRA_GPIO_PCC5
#define LOKI_WLAN_WOW	TEGRA_GPIO_PU5

#define LOKI_SD_CD	TEGRA_GPIO_PV2

#define FUSE_SOC_SPEEDO_0      0x134

static void (*wifi_status_cb)(int card_present, void *dev_id);
static void *wifi_status_cb_devid;
static int loki_wifi_status_register(void (*callback)(int , void *), void *);

static int loki_wifi_reset(int on);
static int loki_wifi_power(int on);
static int loki_wifi_set_carddetect(int val);
static int loki_wifi_get_mac_addr(unsigned char *buf);

static struct wifi_platform_data loki_wifi_control = {
	.set_power	= loki_wifi_power,
	.set_reset	= loki_wifi_reset,
	.set_carddetect	= loki_wifi_set_carddetect,
	.get_mac_addr	= loki_wifi_get_mac_addr,
};

static struct resource wifi_resource[] = {
	[0] = {
		.name	= "bcm4329_wlan_irq",
		.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL
				| IORESOURCE_IRQ_SHAREABLE,
	},
};

static struct platform_device loki_wifi_device = {
	.name		= "bcm4329_wlan",
	.id		= 1,
	.num_resources	= 1,
	.resource	= wifi_resource,
	.dev		= {
		.platform_data = &loki_wifi_control,
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
		.device	 = 0x4329,
	},
};
#endif

static struct tegra_sdhci_platform_data tegra_sdhci_platform_data0 = {
	.mmc_data = {
		.register_status_notify	= loki_wifi_status_register,
#ifdef CONFIG_MMC_EMBEDDED_SDIO
		.embedded_sdio = &embedded_sdio_data0,
#endif
		.built_in = 0,
		.ocr_mask = MMC_OCR_1V8_MASK,
	},
	.cd_gpio = -1,
	.wp_gpio = -1,
	.power_gpio = -1,
	.tap_delay = 0,
	.trim_delay = 0x2,
	.ddr_clk_limit = 41000000,
	.uhs_mask = MMC_UHS_MASK_DDR50 | MMC_UHS_MASK_SDR50,
	.calib_3v3_offsets = 0x7676,
	.calib_1v8_offsets = 0x7676,
};

static struct tegra_sdhci_platform_data tegra_sdhci_platform_data2 = {
	.cd_gpio = LOKI_SD_CD,
	.wp_gpio = -1,
	.power_gpio = -1,
	.tap_delay = 0,
	.trim_delay = 0x3,
	.uhs_mask = MMC_UHS_MASK_DDR50 | MMC_UHS_MASK_SDR50,
	.max_clk_limit = 204000000,
	.calib_3v3_offsets = 0x7676,
	.calib_1v8_offsets = 0x7676,
};

static struct tegra_sdhci_platform_data tegra_sdhci_platform_data3 = {
	.cd_gpio = -1,
	.wp_gpio = -1,
	.power_gpio = -1,
	.is_8bit = 1,
	.tap_delay = 0x4,
	.trim_delay = 0x3,
	.ddr_trim_delay = 0x0,
	.mmc_data = {
		.built_in = 1,
		.ocr_mask = MMC_OCR_1V8_MASK,
	},
	.ddr_clk_limit = 51000000,
	.max_clk_limit = 200000000,
	.calib_3v3_offsets = 0x0202,
	.calib_1v8_offsets = 0x0202,
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

static int loki_wifi_status_register(
		void (*callback)(int card_present, void *dev_id),
		void *dev_id)
{
	if (wifi_status_cb)
		return -EAGAIN;
	wifi_status_cb = callback;
	wifi_status_cb_devid = dev_id;
	return 0;
}

static int loki_wifi_set_carddetect(int val)
{
	pr_debug("%s: %d\n", __func__, val);
	if (wifi_status_cb)
		wifi_status_cb(val, wifi_status_cb_devid);
	else
		pr_warn("%s: Nobody to notify\n", __func__);
	return 0;
}

static struct regulator *loki_vdd_com_3v3;
static struct regulator *loki_vddio_com_1v8;

#define LOKI_VDD_WIFI_3V3 "avdd"
#define LOKI_VDD_WIFI_1V8 "dvdd"

static int loki_wifi_regulator_enable(void)
{
	int ret = 0;

	/* Enable COM's vdd_com_3v3 regulator*/
	if (IS_ERR_OR_NULL(loki_vdd_com_3v3)) {
		loki_vdd_com_3v3 = regulator_get(&loki_wifi_device.dev,
					LOKI_VDD_WIFI_3V3);
		if (IS_ERR(loki_vdd_com_3v3)) {
			pr_err("Couldn't get regulator "
				LOKI_VDD_WIFI_3V3 "\n");
			return PTR_ERR(loki_vdd_com_3v3);
		}

		ret = regulator_enable(loki_vdd_com_3v3);
		if (ret < 0) {
			pr_err("Couldn't enable regulator "
				LOKI_VDD_WIFI_3V3 "\n");
			regulator_put(loki_vdd_com_3v3);
			loki_vdd_com_3v3 = NULL;
			return ret;
		}
	}

	/* Enable COM's vddio_com_1v8 regulator*/
	if (IS_ERR_OR_NULL(loki_vddio_com_1v8)) {
		loki_vddio_com_1v8 = regulator_get(&loki_wifi_device.dev,
			LOKI_VDD_WIFI_1V8);
		if (IS_ERR(loki_vddio_com_1v8)) {
			pr_err("Couldn't get regulator "
				LOKI_VDD_WIFI_1V8 "\n");
			regulator_disable(loki_vdd_com_3v3);

			regulator_put(loki_vdd_com_3v3);
			loki_vdd_com_3v3 = NULL;
			return PTR_ERR(loki_vddio_com_1v8);
		}

		ret = regulator_enable(loki_vddio_com_1v8);
		if (ret < 0) {
			pr_err("Couldn't enable regulator "
				LOKI_VDD_WIFI_1V8 "\n");
			regulator_put(loki_vddio_com_1v8);
			loki_vddio_com_1v8 = NULL;

			regulator_disable(loki_vdd_com_3v3);
			regulator_put(loki_vdd_com_3v3);
			loki_vdd_com_3v3 = NULL;
			return ret;
		}
	}

	return ret;
}

static void loki_wifi_regulator_disable(void)
{
	/* Disable COM's vdd_com_3v3 regulator*/
	if (!IS_ERR_OR_NULL(loki_vdd_com_3v3)) {
		regulator_disable(loki_vdd_com_3v3);
		regulator_put(loki_vdd_com_3v3);
		loki_vdd_com_3v3 = NULL;
	}

	/* Disable COM's vddio_com_1v8 regulator*/
	if (!IS_ERR_OR_NULL(loki_vddio_com_1v8)) {
		regulator_disable(loki_vddio_com_1v8);
		regulator_put(loki_vddio_com_1v8);
		loki_vddio_com_1v8 = NULL;
	}
}

static int loki_wifi_power(int on)
{
	int ret = 0;

	pr_err("%s: %d\n", __func__, on);

	/* Enable COM's regulators on wi-fi poer on*/
	if (on == 1) {
		ret = loki_wifi_regulator_enable();
		if (ret < 0) {
			pr_err("Failed to enable COM regulators\n");
			return ret;
		}
	}

	gpio_set_value(LOKI_WLAN_PWR, on);
	gpio_set_value(LOKI_WLAN_RST, on);
	mdelay(100);

	/* Disable COM's regulators on wi-fi poer off*/
	if (on != 1) {
		pr_debug("Disabling COM regulators\n");
		loki_wifi_regulator_disable();
	}

	return 0;
}

static int loki_wifi_reset(int on)
{
	pr_debug("%s: do nothing\n", __func__);
	return 0;
}

#define LOKI_WIFI_MAC_ADDR_FILE	"/mnt/factory/wifi/wifi_mac.txt"

static int loki_wifi_get_mac_addr(unsigned char *buf)
{
	struct file *fp;
	int rdlen;
	char str[32];
	int mac[6];
	int ret = 0;

	pr_debug("%s\n", __func__);

	/* open wifi mac address file */
	fp = filp_open(LOKI_WIFI_MAC_ADDR_FILE, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		pr_err("%s: cannot open %s\n",
			__func__, LOKI_WIFI_MAC_ADDR_FILE);
		return -ENOENT;
	}

	/* read wifi mac address file */
	memset(str, 0, sizeof(str));
	rdlen = kernel_read(fp, fp->f_pos, str, 17);
	if (rdlen > 0)
		fp->f_pos += rdlen;
	if (rdlen != 17) {
		pr_err("%s: bad mac address file"
			" - len %d < 17",
			__func__, rdlen);
		ret = -ENOENT;
	} else if (sscanf(str, "%x:%x:%x:%x:%x:%x",
		&mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) != 6) {
		pr_err("%s: bad mac address file"
			" - must contain xx:xx:xx:xx:xx:xx\n",
			__func__);
		ret = -ENOENT;
	} else {
		pr_info("%s: using wifi mac %02x:%02x:%02x:%02x:%02x:%02x\n",
			__func__,
			mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		buf[0] = (unsigned char) mac[0];
		buf[1] = (unsigned char) mac[1];
		buf[2] = (unsigned char) mac[2];
		buf[3] = (unsigned char) mac[3];
		buf[4] = (unsigned char) mac[4];
		buf[5] = (unsigned char) mac[5];
	}

	/* close wifi mac address file */
	filp_close(fp, NULL);

	return ret;
}

static int __init loki_wifi_init(void)
{
	int rc;

	rc = gpio_request(LOKI_WLAN_PWR, "wlan_power");
	if (rc)
		pr_err("WLAN_PWR gpio request failed:%d\n", rc);
	rc = gpio_request(LOKI_WLAN_RST, "wlan_rst");
	if (rc)
		pr_err("WLAN_RST gpio request failed:%d\n", rc);
	rc = gpio_request(LOKI_WLAN_WOW, "bcmsdh_sdmmc");
	if (rc)
		pr_err("WLAN_WOW gpio request failed:%d\n", rc);

	rc = gpio_direction_output(LOKI_WLAN_PWR, 0);
	if (rc)
		pr_err("WLAN_PWR gpio direction configuration failed:%d\n", rc);
	rc = gpio_direction_output(LOKI_WLAN_RST, 0);
	if (rc)
		pr_err("WLAN_RST gpio direction configuration failed:%d\n", rc);

	rc = gpio_direction_input(LOKI_WLAN_WOW);
	if (rc)
		pr_err("WLAN_WOW gpio direction configuration failed:%d\n", rc);

	wifi_resource[0].start = wifi_resource[0].end =
		gpio_to_irq(LOKI_WLAN_WOW);

	platform_device_register(&loki_wifi_device);
	return 0;
}

#ifdef CONFIG_TEGRA_PREPOWER_WIFI
static int __init loki_wifi_prepower(void)
{
	if (!of_machine_is_compatible("nvidia,loki"))
		return 0;
	loki_wifi_power(1);

	return 0;
}

subsys_initcall_sync(loki_wifi_prepower);
#endif

int __init loki_sdhci_init(void)
{
	int nominal_core_mv;
	int min_vcore_override_mv;
	int boot_vcore_mv;
	u32 speedo;
	struct board_info bi;

	tegra_get_board_info(&bi);

	if (bi.board_id == BOARD_E2548 && bi.sku == 0x0 && bi.fab == 0x0) {
		tegra_sdhci_platform_data3.uhs_mask |= MMC_MASK_HS200;
		tegra_sdhci_platform_data3.max_clk_limit = 102000000;
	}

	nominal_core_mv =
		tegra_dvfs_rail_get_nominal_millivolts(tegra_core_rail);
	if (nominal_core_mv) {
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
	boot_vcore_mv = tegra_dvfs_rail_get_boot_level(tegra_core_rail);
	if (boot_vcore_mv) {
		tegra_sdhci_platform_data0.boot_vcore_mv = boot_vcore_mv;
		tegra_sdhci_platform_data2.boot_vcore_mv = boot_vcore_mv;
		tegra_sdhci_platform_data3.boot_vcore_mv = boot_vcore_mv;
	}

	tegra_sdhci_platform_data0.max_clk_limit = 136000000;

	speedo = tegra_fuse_readl(FUSE_SOC_SPEEDO_0);
	tegra_sdhci_platform_data0.cpu_speedo = speedo;
	tegra_sdhci_platform_data2.cpu_speedo = speedo;
	tegra_sdhci_platform_data3.cpu_speedo = speedo;


	platform_device_register(&tegra_sdhci_device3);

	if (!is_uart_over_sd_enabled())
		platform_device_register(&tegra_sdhci_device2);

	platform_device_register(&tegra_sdhci_device0);
	loki_wifi_init();

	return 0;
}
