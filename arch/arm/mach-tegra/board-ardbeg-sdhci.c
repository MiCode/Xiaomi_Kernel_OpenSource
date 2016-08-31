/*
 * arch/arm/mach-tegra/board-ardbeg-sdhci.c
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
#include "board-ardbeg.h"
#include "dvfs.h"
#include "iomap.h"
#include "tegra-board-id.h"

#define ARDBEG_WLAN_RST	TEGRA_GPIO_PCC5
#define ARDBEG_WLAN_PWR	TEGRA_GPIO_PX7
#define ARDBEG_WLAN_WOW	TEGRA_GPIO_PU5
#if defined(CONFIG_BCMDHD_EDP_SUPPORT)
#define ON 3070 /* 3069mW */
#define OFF 0
static unsigned int wifi_states[] = {ON, OFF};
#endif

#define ARDBEG_SD_CD	TEGRA_GPIO_PV2
#define ARDBEG_SD_WP	TEGRA_GPIO_PQ4
#define FUSE_SOC_SPEEDO_0	0x134

static void (*wifi_status_cb)(int card_present, void *dev_id);
static void *wifi_status_cb_devid;
static int ardbeg_wifi_status_register(void (*callback)(int , void *), void *);

static int ardbeg_wifi_reset(int on);
static int ardbeg_wifi_power(int on);
static int ardbeg_wifi_set_carddetect(int val);
static int ardbeg_wifi_get_mac_addr(unsigned char *buf);

static struct wifi_platform_data ardbeg_wifi_control = {
	.set_power	= ardbeg_wifi_power,
	.set_reset	= ardbeg_wifi_reset,
	.set_carddetect	= ardbeg_wifi_set_carddetect,
	.get_mac_addr	= ardbeg_wifi_get_mac_addr,
#if defined (CONFIG_BCMDHD_EDP_SUPPORT)
	/* wifi edp client information */
	.client_info	= {
		.name		= "wifi_edp_client",
		.states		= wifi_states,
		.num_states	= ARRAY_SIZE(wifi_states),
		.e0_index	= 0,
		.priority	= EDP_MAX_PRIO,
	},
#endif
};

static struct resource wifi_resource[] = {
	[0] = {
		.name	= "bcm4329_wlan_irq",
		.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL
				| IORESOURCE_IRQ_SHAREABLE,
	},
};

static struct platform_device ardbeg_wifi_device = {
	.name		= "bcm4329_wlan",
	.id		= 1,
	.num_resources	= 1,
	.resource	= wifi_resource,
	.dev		= {
		.platform_data = &ardbeg_wifi_control,
	},
};

static struct resource mrvl_wifi_resource[] = {
	[0] = {
		.name   = "mrvl_wlan_irq",
		.flags  = IORESOURCE_IRQ | IORESOURCE_IRQ_LOWLEVEL | IORESOURCE_IRQ_SHAREABLE,
	},
};

static struct platform_device marvell_wifi_device = {
	.name           = "mrvl_wlan",
	.id             = 1,
	.num_resources  = 1,
	.resource       = mrvl_wifi_resource,
	.dev            = {
		.platform_data = &ardbeg_wifi_control,
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
		.register_status_notify	= ardbeg_wifi_status_register,
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
	.uhs_mask = MMC_UHS_MASK_DDR50,
	.calib_3v3_offsets = 0x7676,
	.calib_1v8_offsets = 0x7676,
};

static struct tegra_sdhci_platform_data tegra_sdhci_platform_data2 = {
	.cd_gpio = ARDBEG_SD_CD,
	.wp_gpio = -1,
	.power_gpio = -1,
	.tap_delay = 0,
	.trim_delay = 0x3,
	.uhs_mask = MMC_UHS_MASK_DDR50,
	.calib_3v3_offsets = 0x7676,
	.calib_1v8_offsets = 0x7676,
};

static struct tegra_sdhci_platform_data tegra_sdhci_platform_data3 = {
	.cd_gpio = -1,
	.wp_gpio = -1,
	.power_gpio = -1,
	.is_8bit = 1,
	.tap_delay = 0x4,
	.trim_delay = 0x4,
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

static int ardbeg_wifi_status_register(
		void (*callback)(int card_present, void *dev_id),
		void *dev_id)
{
	if (wifi_status_cb)
		return -EAGAIN;
	wifi_status_cb = callback;
	wifi_status_cb_devid = dev_id;
	return 0;
}

static int ardbeg_wifi_set_carddetect(int val)
{
	pr_debug("%s: %d\n", __func__, val);
	if (wifi_status_cb)
		wifi_status_cb(val, wifi_status_cb_devid);
	else
		pr_warn("%s: Nobody to notify\n", __func__);
	return 0;
}

static int ardbeg_wifi_power(int on)
{
	pr_err("%s: %d\n", __func__, on);

	gpio_set_value(ARDBEG_WLAN_PWR, on);
	gpio_set_value(ARDBEG_WLAN_RST, on);
	mdelay(100);

	return 0;
}

static int ardbeg_wifi_reset(int on)
{
	pr_debug("%s: do nothing\n", __func__);
	return 0;
}

#define ARDBEG_WIFI_MAC_ADDR_FILE	"/mnt/factory/wifi/wifi_mac.txt"

static int ardbeg_wifi_get_mac_addr(unsigned char *buf)
{
	struct file *fp;
	int rdlen;
	char str[32];
	int mac[6];
	int ret = 0;

	pr_debug("%s\n", __func__);

	/* open wifi mac address file */
	fp = filp_open(ARDBEG_WIFI_MAC_ADDR_FILE, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		pr_err("%s: cannot open %s\n",
			__func__, ARDBEG_WIFI_MAC_ADDR_FILE);
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

static int __init ardbeg_wifi_init(void)
{
	int rc;

	rc = gpio_request(ARDBEG_WLAN_PWR, "wlan_power");
	if (rc)
		pr_err("WLAN_PWR gpio request failed:%d\n", rc);
	rc = gpio_request(ARDBEG_WLAN_RST, "wlan_rst");
	if (rc)
		pr_err("WLAN_RST gpio request failed:%d\n", rc);
	rc = gpio_request(ARDBEG_WLAN_WOW, "bcmsdh_sdmmc");
	if (rc)
		pr_err("WLAN_WOW gpio request failed:%d\n", rc);

	rc = gpio_direction_output(ARDBEG_WLAN_PWR, 0);
	if (rc)
		pr_err("WLAN_PWR gpio direction configuration failed:%d\n", rc);
	rc = gpio_direction_output(ARDBEG_WLAN_RST, 0);
	if (rc)
		pr_err("WLAN_RST gpio direction configuration failed:%d\n", rc);

	rc = gpio_direction_input(ARDBEG_WLAN_WOW);
	if (rc)
		pr_err("WLAN_WOW gpio direction configuration failed:%d\n", rc);

	wifi_resource[0].start = wifi_resource[0].end =
		gpio_to_irq(ARDBEG_WLAN_WOW);

	platform_device_register(&ardbeg_wifi_device);

	mrvl_wifi_resource[0].start = mrvl_wifi_resource[0].end =
		gpio_to_irq(ARDBEG_WLAN_WOW);
	platform_device_register(&marvell_wifi_device);

	return 0;
}

#ifdef CONFIG_TEGRA_PREPOWER_WIFI
static int __init ardbeg_wifi_prepower(void)
{
	if (!of_machine_is_compatible("nvidia,ardbeg") &&
		!of_machine_is_compatible("nvidia,laguna") &&
		!of_machine_is_compatible("nvidia,ardbeg_sata") &&
		!of_machine_is_compatible("nvidia,tn8") &&
		!of_machine_is_compatible("nvidia,norrin"))
		return 0;
	ardbeg_wifi_power(1);

	return 0;
}

subsys_initcall_sync(ardbeg_wifi_prepower);
#endif

int __init ardbeg_sdhci_init(void)
{
	int nominal_core_mv;
	int min_vcore_override_mv;
	int boot_vcore_mv;
	u32 speedo;
	struct board_info board_info;

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

	if (of_machine_is_compatible("nvidia,laguna"))
		tegra_sdhci_platform_data2.wp_gpio = ARDBEG_SD_WP;

	tegra_get_board_info(&board_info);
	if (board_info.board_id == BOARD_E1780)
		tegra_sdhci_platform_data2.max_clk_limit = 204000000;
	tegra_sdhci_platform_data0.max_clk_limit = 136000000;
	if ((board_info.board_id == BOARD_E1781) ||
		(board_info.board_id == BOARD_PM374) ||
		(board_info.board_id == BOARD_PM359))
		tegra_sdhci_platform_data3.uhs_mask = MMC_MASK_HS200;

	if (board_info.board_id == BOARD_PM374 ||
		board_info.board_id == BOARD_PM358 ||
		board_info.board_id == BOARD_PM363 ||
		board_info.board_id == BOARD_PM359)
			tegra_sdhci_platform_data0.disable_clock_gate = 1;

	speedo = tegra_fuse_readl(FUSE_SOC_SPEEDO_0);
	tegra_sdhci_platform_data0.cpu_speedo = speedo;
	tegra_sdhci_platform_data2.cpu_speedo = speedo;
	tegra_sdhci_platform_data3.cpu_speedo = speedo;

	platform_device_register(&tegra_sdhci_device3);
	platform_device_register(&tegra_sdhci_device2);
	platform_device_register(&tegra_sdhci_device0);
	ardbeg_wifi_init();

	return 0;
}
