/*
 * arch/arm/mach-tegra/board-bonaire.c
 *
 * Copyright (C) 2013 NVIDIA Corporation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/serial_8250.h>
#include <linux/i2c.h>
#include <linux/i2c/panjit_ts.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/i2c-tegra.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/platform_data/serial-tegra.h>
#include <linux/of_platform.h>
#include <linux/clk/tegra.h>
#include <linux/tegra-soc.h>
#include <linux/usb/tegra_usb_phy.h>
#include <linux/clocksource.h>
#include <linux/irqchip.h>
#include <linux/pci-tegra.h>

#include <mach/gpio-tegra.h>

#include <mach/io_dpd.h>

#include <mach/irqs.h>
#include <mach/pinmux.h>
#include <mach/i2s.h>
#include <mach/audio.h>
#include <mach/nand.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include "board.h"
#include "board-bonaire.h"
#include "clock.h"
#include "common.h"
#include "devices.h"
#include "gpio-names.h"
#include "iomap.h"

#define ENABLE_OTG 0
/*#define USB_HOST_ONLY*/

static struct plat_serial8250_port debug_uart_platform_data[] = {
	{
		.membase	= IO_ADDRESS(TEGRA_UARTA_BASE),
		.mapbase	= TEGRA_UARTA_BASE,
		.irq		= INT_UARTA,
		.flags		= UPF_BOOT_AUTOCONF | UPF_FIXED_TYPE,
		.type		= PORT_TEGRA,
		.iotype		= UPIO_MEM,
		.regshift	= 2,
		.uartclk	= 13000000,
	}, {
		.flags		= 0,
	}
};

static struct platform_device debug_uart = {
	.name = "serial8250",
	.id = PLAT8250_DEV_PLATFORM,
	.dev = {
		.platform_data = debug_uart_platform_data,
	},
};

#ifdef CONFIG_BCM4329_RFKILL

static struct resource bonaire_bcm4329_rfkill_resources[] = {
	{
		.name   = "bcm4329_nreset_gpio",
		.start  = TEGRA_GPIO_PU0,
		.end    = TEGRA_GPIO_PU0,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "bcm4329_nshutdown_gpio",
		.start  = TEGRA_GPIO_PK2,
		.end    = TEGRA_GPIO_PK2,
		.flags  = IORESOURCE_IO,
	},
};

static struct platform_device bonaire_bcm4329_rfkill_device = {
	.name = "bcm4329_rfkill",
	.id             = -1,
	.num_resources  = ARRAY_SIZE(bonaire_bcm4329_rfkill_resources),
	.resource       = bonaire_bcm4329_rfkill_resources,
};

static noinline void __init bonaire_bt_rfkill(void)
{
	/*Add Clock Resource*/
	clk_add_alias("bcm4329_32k_clk", bonaire_bcm4329_rfkill_device.name, \
				"blink", NULL);

	platform_device_register(&bonaire_bcm4329_rfkill_device);

	return;
}
#else
static inline void bonaire_bt_rfkill(void) { }
#endif

static __initdata struct tegra_clk_init_table bonaire_clk_init_table[] = {
	/* name		parent		rate		enabled */
	{ "uarta",	"clk_m",	13000000,	true},
	{ "uartb",	"clk_m",	13000000,	true},
	{ "uartc",	"clk_m",	13000000,	true},
	{ "uartd",	"clk_m",	13000000,	true},
	{ "uarte",	"clk_m",	13000000,	true},
	{ "sdmmc1",     "clk_m",        26000000,       false},
	{ "sdmmc3",     "clk_m",        26000000,       false},
	{ "sdmmc4",     "clk_m",        26000000,       false},
	{ "pll_m",	NULL,		0,		true},
	{ "blink",      "clk_32k",      32768,          false},
	{ "pll_p_out4",	"pll_p",	24000000,	true },
	{ "pwm",	"clk_32k",	32768,		false},
	{ "blink",	"clk_32k",	32768,		false},
	{ "pll_a",	NULL,		56448000,	true},
	{ "pll_a_out0",	NULL,		11289600,	true},
	{ "i2s1",	"pll_a_out0",	11289600,	true},
	{ "i2s2",	"pll_a_out0",	11289600,	true},
	{ "d_audio",	"pll_a_out0",	11289600,	false},
	{ "audio_2x",	"audio",	22579200,	true},
	{ NULL,		NULL,		0,		0},
};

static struct i2c_board_info __initdata bonaire_i2c_bus1_board_info[] = {
	{
		I2C_BOARD_INFO("wm8903", 0x1a),
	},
};

static void bonaire_i2c_init(void)
{
	i2c_register_board_info(0, bonaire_i2c_bus1_board_info, 1);
}

static void bonaire_apbdma_init(void)
{
	platform_device_register(&tegra_apbdma);
}

#define GPIO_KEY(_id, _gpio, _iswake)		\
	{					\
		.code = _id,			\
		.gpio = TEGRA_GPIO_##_gpio,	\
		.active_low = 1,		\
		.desc = #_id,			\
		.type = EV_KEY,			\
		.wakeup = _iswake,		\
		.debounce_interval = 10,	\
	}

/* !!!FIXME!!! THESE ARE VENTANA DEFINITIONS */
static struct gpio_keys_button bonaire_keys[] = {
	[0] = GPIO_KEY(KEY_MENU, PQ0, 0),
	[1] = GPIO_KEY(KEY_HOME, PQ1, 0),
	[2] = GPIO_KEY(KEY_BACK, PQ2, 0),
	[3] = GPIO_KEY(KEY_VOLUMEUP, PQ3, 0),
	[4] = GPIO_KEY(KEY_VOLUMEDOWN, PQ4, 0),
	[5] = GPIO_KEY(KEY_POWER, PV2, 1),
};

static struct gpio_keys_platform_data bonaire_keys_platform_data = {
	.buttons	= bonaire_keys,
	.nbuttons	= ARRAY_SIZE(bonaire_keys),
};

static struct platform_device bonaire_keys_device = {
	.name	= "gpio-keys",
	.id	= 0,
	.dev	= {
		.platform_data	= &bonaire_keys_platform_data,
	},
};

static struct resource tegra_rtc_resources[] = {
	[0] = {
		.start = TEGRA_RTC_BASE,
		.end = TEGRA_RTC_BASE + TEGRA_RTC_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = INT_RTC,
		.end = INT_RTC,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device tegra_rtc_device = {
	.name = "tegra_rtc",
	.id   = -1,
	.resource = tegra_rtc_resources,
	.num_resources = ARRAY_SIZE(tegra_rtc_resources),
};

#if defined(CONFIG_MTD_NAND_TEGRA)
static struct resource nand_resources[] = {
	[0] = {
		.start = INT_NANDFLASH,
		.end   = INT_NANDFLASH,
		.flags = IORESOURCE_IRQ
	},
	[1] = {
		.start = TEGRA_NAND_BASE,
		.end = TEGRA_NAND_BASE + TEGRA_NAND_SIZE - 1,
		.flags = IORESOURCE_MEM
	}
};

static struct tegra_nand_chip_parms nand_chip_parms[] = {
	/* Samsung K5E2G1GACM */
	[0] = {
		.vendor_id   = 0xEC,
		.device_id   = 0xAA,
		.capacity    = 256,
		.timing      = {
			.trp		= 21,
			.trh		= 15,
			.twp		= 21,
			.twh		= 15,
			.tcs		= 31,
			.twhr		= 60,
			.tcr_tar_trr	= 20,
			.twb		= 100,
			.trp_resp	= 30,
			.tadl		= 100,
		},
	},
	/* Hynix H5PS1GB3EFR */
	[1] = {
		.vendor_id   = 0xAD,
		.device_id   = 0xDC,
		.capacity    = 512,
		.timing      = {
			.trp		= 12,
			.trh		= 10,
			.twp		= 12,
			.twh		= 10,
			.tcs		= 20,
			.twhr		= 80,
			.tcr_tar_trr	= 20,
			.twb		= 100,
			.trp_resp	= 20,
			.tadl		= 70,
		},
	},
};

struct tegra_nand_platform nand_data = {
	.max_chips	= 8,
	.chip_parms	= nand_chip_parms,
	.nr_chip_parms  = ARRAY_SIZE(nand_chip_parms),
};

struct platform_device tegra_nand_device = {
	.name          = "tegra_nand",
	.id            = -1,
	.resource      = nand_resources,
	.num_resources = ARRAY_SIZE(nand_resources),
	.dev            = {
		.platform_data = &nand_data,
	},
};
#endif

static struct platform_device *bonaire_devices[] __initdata = {
#if ENABLE_OTG
	&tegra_otg_device,
#endif
	&debug_uart,
	&tegra_pmu_device,
	&tegra_rtc_device,
#if !defined(USB_HOST_ONLY)
	&tegra_udc_device,
#endif
#if defined(CONFIG_CRYPTO_DEV_TEGRA_SE)
	&tegra12_se_device,
#endif
	&bonaire_keys_device,
#if defined(CONFIG_SND_HDA_TEGRA)
	&tegra_hda_device,
#endif
	&tegra_avp_device,
#if defined(CONFIG_MTD_NAND_TEGRA)
	&tegra_nand_device,
#endif
};

static int __init bonaire_touch_init(void)
{
	return 0;
}

static struct tegra_usb_platform_data tegra_udc_pdata = {
	.port_otg = false,
	.has_hostpc = true,
	.phy_intf = TEGRA_USB_PHY_INTF_UTMI,
	.op_mode = TEGRA_USB_OPMODE_DEVICE,
	.u_data.dev = {
		.vbus_pmu_irq = 0,
		.vbus_gpio = -1,
		.charging_supported = false,
		.remote_wakeup_supported = false,
	},
	.u_cfg.utmi = {
		.hssync_start_delay = 0,
		.elastic_limit = 16,
		.idle_wait_delay = 17,
		.term_range_adj = 6,
		.xcvr_setup = 8,
		.xcvr_lsfslew = 2,
		.xcvr_lsrslew = 2,
		.xcvr_setup_offset = 0,
		.xcvr_use_fuses = 1,
	},
};

static struct tegra_usb_platform_data tegra_ehci1_utmi_pdata = {
	.port_otg = false,
	.has_hostpc = true,
	.phy_intf = TEGRA_USB_PHY_INTF_UTMI,
	.op_mode = TEGRA_USB_OPMODE_HOST,
	.u_data.host = {
		.vbus_gpio = -1,
		/*.vbus_reg = "vdd_vbus_micro_usb",*/
		.hot_plug = true,
		.remote_wakeup_supported = true,
		.power_off_on_suspend = true,
	},
	.u_cfg.utmi = {
		.hssync_start_delay = 0,
		.elastic_limit = 16,
		.idle_wait_delay = 17,
		.term_range_adj = 6,
		.xcvr_setup = 15,
		.xcvr_lsfslew = 2,
		.xcvr_lsrslew = 2,
		.xcvr_setup_offset = 0,
		.xcvr_use_fuses = 1,
	},
};

static struct tegra_usb_platform_data tegra_ehci2_utmi_pdata = {
	.port_otg = false,
	.has_hostpc = true,
	.phy_intf = TEGRA_USB_PHY_INTF_UTMI,
	.op_mode	 = TEGRA_USB_OPMODE_HOST,
	.u_data.host = {
		.vbus_gpio = -1,
		.hot_plug = true,
		.remote_wakeup_supported = true,
		.power_off_on_suspend = true,
	},
	.u_cfg.utmi = {
		.hssync_start_delay = 0,
		.elastic_limit = 16,
		.idle_wait_delay = 17,
		.term_range_adj = 6,
		.xcvr_setup = 15,
		.xcvr_lsfslew = 2,
		.xcvr_lsrslew = 2,
		.xcvr_setup_offset = 0,
		.xcvr_use_fuses = 1,
	},
};

static struct tegra_usb_platform_data tegra_ehci3_utmi_pdata = {
	.port_otg = false,
	.has_hostpc = true,
	.phy_intf = TEGRA_USB_PHY_INTF_UTMI,
	.op_mode	 = TEGRA_USB_OPMODE_HOST,
	.u_data.host = {
		.vbus_gpio = -1,
		/*.vbus_reg = "vdd_vbus_typea_usb",*/
		.hot_plug = true,
		.remote_wakeup_supported = true,
		.power_off_on_suspend = true,
	},
	.u_cfg.utmi = {
		.hssync_start_delay = 0,
		.elastic_limit = 16,
		.idle_wait_delay = 17,
		.term_range_adj = 6,
		.xcvr_setup = 8,
		.xcvr_lsfslew = 2,
		.xcvr_lsrslew = 2,
		.xcvr_setup_offset = 0,
		.xcvr_use_fuses = 1,
	},
};

static void bonaire_usb_init(void)
{
#if defined(USB_HOST_ONLY)
	tegra_ehci1_device.dev.platform_data = &tegra_ehci1_utmi_pdata;
	platform_device_register(&tegra_ehci1_device);
#else
	/* setup the udc platform data */
	tegra_udc_device.dev.platform_data = &tegra_udc_pdata;

#endif
}

static struct platform_device *bonaire_hs_uart_devices[] __initdata = {
	&tegra_uartd_device, &tegra_uartb_device, &tegra_uartc_device,
};

static struct tegra_serial_platform_data bonaire_uartb_pdata = {
	.dma_req_selector = 9,
	.modem_interrupt = false,
};
static struct tegra_serial_platform_data bonaire_uartc_pdata = {
	.dma_req_selector = 10,
	.modem_interrupt = false,
};
static struct tegra_serial_platform_data bonaire_uartd_pdata = {
	.dma_req_selector = 19,
	.modem_interrupt = false,
};

static void __init bonaire_hs_uart_init(void)
{
	tegra_uartb_device.dev.platform_data = &bonaire_uartb_pdata;
	tegra_uartc_device.dev.platform_data = &bonaire_uartc_pdata;
	tegra_uartd_device.dev.platform_data = &bonaire_uartd_pdata;
	platform_add_devices(bonaire_hs_uart_devices,
			ARRAY_SIZE(bonaire_hs_uart_devices));
}

static struct tegra_pci_platform_data bonaire_pcie_platform_data = {
	.port_status[0]	= 1,
	.port_status[1]	= 1,
	.gpio_hot_plug	= TEGRA_GPIO_PO1,
};

static void bonaire_pcie_init(void)
{
	tegra_pci_device.dev.platform_data = &bonaire_pcie_platform_data;
	platform_device_register(&tegra_pci_device);
}

static void __init tegra_bonaire_init(void)
{
	tegra_clk_init_from_table(bonaire_clk_init_table);
	tegra_enable_pinmux();
	bonaire_pinmux_init();
	tegra_soc_device_init("bonaire");
	bonaire_apbdma_init();

	if (tegra_platform_is_fpga() && tegra_platform_is_qt())
		debug_uart_platform_data[0].uartclk =
						tegra_clk_measure_input_freq();

	platform_add_devices(bonaire_devices, ARRAY_SIZE(bonaire_devices));

	if (tegra_cpu_is_asim())
		bonaire_power_off_init();
	tegra_io_dpd_init();
	bonaire_hs_uart_init();
	bonaire_sdhci_init();
	bonaire_i2c_init();
	bonaire_regulator_init();
	bonaire_suspend_init();
	bonaire_touch_init();
	bonaire_usb_init();
	bonaire_panel_init();
	bonaire_sensors_init();
	bonaire_bt_rfkill();
	bonaire_pcie_init();
	tegra_register_fuse();
}

#ifdef CONFIG_USE_OF
struct of_dev_auxdata tegra_bonaire_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA("nvidia,tegra124-host1x", TEGRA_HOST1X_BASE, "host1x",
		NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-gk20a", TEGRA_GK20A_BAR0_BASE,
		"gk20a.0", NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-vic", TEGRA_VIC_BASE, "vic03.0", NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-msenc", TEGRA_MSENC_BASE, "msenc",
		NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-vi", TEGRA_VI_BASE, "vi.0", NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-isp", TEGRA_ISP_BASE, "isp.0", NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-isp", TEGRA_ISPB_BASE, "isp.1", NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-tsec", TEGRA_TSEC_BASE, "tsec", NULL),
	T124_I2C_OF_DEV_AUXDATA,
	{}
};
#endif

static void __init tegra_bonaire_dt_init(void)
{
	of_platform_populate(NULL, of_default_bus_match_table,
		tegra_bonaire_auxdata_lookup, &platform_bus);

	tegra_bonaire_init();
}

static void __init tegra_bonaire_reserve(void)
{
#if defined(CONFIG_NVMAP_CONVERT_CARVEOUT_TO_IOVMM)
	tegra_reserve(0, SZ_16M + SZ_2M, SZ_16M);
#else
	if (tegra_cpu_is_asim() && tegra_split_mem_active())
		tegra_reserve(0, 0, 0);
	else
		tegra_reserve(SZ_128M, SZ_16M + SZ_2M, SZ_16M);
#endif
}

static const char * const bonaire_dt_board_compat[] = {
	"nvidia,bonaire",
	NULL
};

MACHINE_START(BONAIRE, "bonaire")
	.atag_offset    = 0x80000100,
	.map_io		= tegra_map_common_io,
	.reserve	= tegra_bonaire_reserve,
	.init_early	= tegra12x_init_early,
	.init_irq	= irqchip_init,
	.init_machine	= tegra_bonaire_dt_init,
	.init_time      = clocksource_of_init,
	.dt_compat	= bonaire_dt_board_compat,
MACHINE_END
