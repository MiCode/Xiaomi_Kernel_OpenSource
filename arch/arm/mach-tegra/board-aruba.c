/*
 * arch/arm/mach-tegra/board-aruba.c
 *
 * Copyright (c) 2010-2013, NVIDIA CORPORATION.  All rights reserved.
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

#include <mach/clk.h>
#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/pinmux.h>
#include <mach/iomap.h>
#include <mach/io.h>
#include <mach/i2s.h>
#include <mach/audio.h>
#include <mach/tegra_das.h>

#include <asm/hardware/gic.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <mach/usb_phy.h>
#include <mach/nand.h>
#include <mach/tegra_fiq_debugger.h>

#include "board.h"
#include "clock.h"
#include "board-aruba.h"
#include "devices.h"
#include "gpio-names.h"
#include "fuse.h"

#define ENABLE_OTG 0

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

/* !!!FIXME!!! THESE ARE VENTANA SETTINGS */
static struct tegra_utmip_config utmi_phy_config[] = {
	[0] = {
			.hssync_start_delay = 0,
			.idle_wait_delay = 17,
			.elastic_limit = 16,
			.term_range_adj = 6,
			.xcvr_setup = 15,
			.xcvr_lsfslew = 2,
			.xcvr_lsrslew = 2,
	},
	[1] = {
			.hssync_start_delay = 0,
			.idle_wait_delay = 17,
			.elastic_limit = 16,
			.term_range_adj = 6,
			.xcvr_setup = 8,
			.xcvr_lsfslew = 2,
			.xcvr_lsrslew = 2,
	},
};

/* !!!FIXME!!! THESE ARE VENTANA SETTINGS */
static struct tegra_ulpi_config ulpi_phy_config = {
	.clk = "cdev2",
};

#ifdef CONFIG_BCM4329_RFKILL

static struct resource aruba_bcm4329_rfkill_resources[] = {
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

static struct platform_device aruba_bcm4329_rfkill_device = {
	.name = "bcm4329_rfkill",
	.id             = -1,
	.num_resources  = ARRAY_SIZE(aruba_bcm4329_rfkill_resources),
	.resource       = aruba_bcm4329_rfkill_resources,
};

static noinline void __init aruba_bt_rfkill(void)
{
	/*Add Clock Resource*/
	clk_add_alias("bcm4329_32k_clk", aruba_bcm4329_rfkill_device.name, \
				"blink", NULL);

	platform_device_register(&aruba_bcm4329_rfkill_device);

	return;
}
#else
static inline void aruba_bt_rfkill(void) { }
#endif

static __initdata struct tegra_clk_init_table aruba_clk_init_table[] = {
	/* name		parent		rate		enabled */
	{ "uarta",	"clk_m",	13000000,	true},
	{ "uartb",	"clk_m",	13000000,	true},
	{ "uartc",	"clk_m",	13000000,	true},
	{ "uartd",	"clk_m",	13000000,	true},
	{ "uarte",	"clk_m",	13000000,	true},
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

struct tegra_das_platform_data tegra_das_pdata = {
	.tegra_dap_port_info_table = {
		/* I2S0 <--> NULL */
		[0] = {
			.dac_port = tegra_das_port_none,
			.codec_type = tegra_audio_codec_type_none,
			.device_property = {
				.num_channels = 0,
				.bits_per_sample = 0,
				.rate = 0,
				.master = 0,
				.lrck_high_left = false,
				.dac_dap_data_comm_format = 0,
			},
		},
		/* I2S1 <--> Hifi Codec */
		[1] = {
			.dac_port = tegra_das_port_i2s1,
			.codec_type = tegra_audio_codec_type_hifi,
			.device_property = {
				.num_channels = 2,
				.bits_per_sample = 16,
				.rate = 48000,
				.master = 0,
				.lrck_high_left = false,
				.dac_dap_data_comm_format =
						dac_dap_data_format_i2s,
			},
		},
		/* I2s2 <--> BB */
		[2] = {
			.dac_port = tegra_das_port_i2s2,
			.codec_type = tegra_audio_codec_type_baseband,
			.device_property = {
				.num_channels = 1,
				.bits_per_sample = 16,
				.rate = 16000,
				.master = 0,
				.lrck_high_left = true,
				.dac_dap_data_comm_format =
					dac_dap_data_format_dsp,
			},
		},
		/* I2s3 <--> BT */
		[3] = {
			.dac_port = tegra_das_port_i2s3,
			.codec_type = tegra_audio_codec_type_bluetooth,
			.device_property = {
				.num_channels = 1,
				.bits_per_sample = 16,
				.rate = 8000,
				.master = 0,
				.lrck_high_left = false,
				.dac_dap_data_comm_format =
					dac_dap_data_format_dsp,
			},
		},
		[4] = {
			.dac_port = tegra_das_port_none,
			.codec_type = tegra_audio_codec_type_none,
			.device_property = {
				.num_channels = 0,
				.bits_per_sample = 0,
				.rate = 0,
				.master = 0,
				.lrck_high_left = false,
				.dac_dap_data_comm_format = 0,
			},
		},
	},
};

static struct i2c_board_info __initdata aruba_i2c_bus1_board_info[] = {
	{
		I2C_BOARD_INFO("wm8903", 0x1a),
	},
};

static struct tegra_i2c_platform_data aruba_i2c1_platform_data = {
	.adapter_nr	= 0,
	.bus_count	= 1,
	.bus_clk_rate	= { 100000, 0 },
};

#if 0	/* !!!FIXME!!! THESE ARE VENTANA SETTINGS */
static const struct tegra_pingroup_config i2c2_ddc = {
	.pingroup	= TEGRA_PINGROUP_DDC,
	.func		= TEGRA_MUX_I2C2,
};

static const struct tegra_pingroup_config i2c2_gen2 = {
	.pingroup	= TEGRA_PINGROUP_PTA,
	.func		= TEGRA_MUX_I2C2,
};
#endif

static struct tegra_i2c_platform_data aruba_i2c2_platform_data = {
	.adapter_nr	= 1,
	.bus_count	= 2,
	.bus_clk_rate	= { 100000, 100000 },
#if 0	/* !!!FIXME!!!! TESE ARE VENTANA SETTINGS */
	.bus_mux	= { &i2c2_ddc, &i2c2_gen2 },
	.bus_mux_len	= { 1, 1 },
#endif
};

static struct tegra_i2c_platform_data aruba_i2c3_platform_data = {
	.adapter_nr	= 3,
	.bus_count	= 1,
	.bus_clk_rate	= { 100000, 0 },
};

static struct tegra_i2c_platform_data aruba_i2c4_platform_data = {
	.adapter_nr	= 4,
	.bus_count	= 1,
	.bus_clk_rate	= { 100000, 0 },
};

static struct tegra_i2c_platform_data aruba_i2c5_platform_data = {
	.adapter_nr	= 5,
	.bus_count	= 1,
	.bus_clk_rate	= { 100000, 0 },
};

static void aruba_i2c_init(void)
{
	tegra_i2c_device1.dev.platform_data = &aruba_i2c1_platform_data;
	tegra_i2c_device2.dev.platform_data = &aruba_i2c2_platform_data;
	tegra_i2c_device3.dev.platform_data = &aruba_i2c3_platform_data;
	tegra_i2c_device4.dev.platform_data = &aruba_i2c4_platform_data;
	tegra_i2c_device5.dev.platform_data = &aruba_i2c5_platform_data;

	i2c_register_board_info(0, aruba_i2c_bus1_board_info, 1);

	platform_device_register(&tegra_i2c_device5);
	platform_device_register(&tegra_i2c_device4);
	platform_device_register(&tegra_i2c_device3);
	platform_device_register(&tegra_i2c_device2);
	platform_device_register(&tegra_i2c_device1);
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

// !!!FIXME!!! THESE ARE VENTANA DEFINITIONS
static struct gpio_keys_button aruba_keys[] = {
	[0] = GPIO_KEY(KEY_MENU, PQ0, 0),
	[1] = GPIO_KEY(KEY_HOME, PQ1, 0),
	[2] = GPIO_KEY(KEY_BACK, PQ2, 0),
	[3] = GPIO_KEY(KEY_VOLUMEUP, PQ3, 0),
	[4] = GPIO_KEY(KEY_VOLUMEDOWN, PQ4, 0),
	[5] = GPIO_KEY(KEY_POWER, PV2, 1),
};

static struct gpio_keys_platform_data aruba_keys_platform_data = {
	.buttons	= aruba_keys,
	.nbuttons	= ARRAY_SIZE(aruba_keys),
};

static struct platform_device aruba_keys_device = {
	.name	= "gpio-keys",
	.id	= 0,
	.dev	= {
		.platform_data	= &aruba_keys_platform_data,
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

static struct platform_device *aruba_devices[] __initdata = {
#if ENABLE_OTG
	&tegra_otg_device,
#endif
	&debug_uart,
	&tegra_uartb_device,
	&tegra_uartc_device,
	&tegra_uartd_device,
	&tegra_uarte_device,
	&tegra_pmu_device,
	&tegra_rtc_device,
	&tegra_udc_device,
#if defined(CONFIG_TEGRA_IOVMM_SMMU) || defined(CONFIG_TEGRA_IOMMU_SMMU)
	&tegra_smmu_device,
#endif
	&aruba_keys_device,
	&tegra_wdt0_device,
#if defined(CONFIG_SND_HDA_TEGRA)
	&tegra_hda_device,
#endif
	&tegra_avp_device,
#if defined(CONFIG_MTD_NAND_TEGRA)
	&tegra_nand_device,
#endif
};

static int __init aruba_touch_init(void)
{
	return 0;
}


static struct tegra_ehci_platform_data tegra_ehci_pdata[] = {
	[0] = {
			.phy_config = &utmi_phy_config[0],
			.operating_mode = TEGRA_USB_HOST,
			.power_down_on_bus_suspend = 0,
	},
	[1] = {
			.phy_config = &ulpi_phy_config,
			.operating_mode = TEGRA_USB_HOST,
			.power_down_on_bus_suspend = 1,
	},
	[2] = {
			.phy_config = &utmi_phy_config[1],
			.operating_mode = TEGRA_USB_HOST,
			.power_down_on_bus_suspend = 0,
	},
};


static void aruba_usb_init(void)
{
	tegra_ehci2_device.dev.platform_data=&tegra_ehci_pdata[1];
	platform_device_register(&tegra_ehci2_device);
}

#ifdef CONFIG_SATA_AHCI_TEGRA
static void aruba_sata_init(void)
{
	platform_device_register(&tegra_sata_device);
}
#else
static void aruba_sata_init(void) { }
#endif

static void __init tegra_aruba_init(void)
{
	tegra_clk_init_from_table(aruba_clk_init_table);
	tegra_enable_pinmux();
	aruba_pinmux_init();
	tegra_soc_device_init("aruba");

	platform_add_devices(aruba_devices, ARRAY_SIZE(aruba_devices));

	aruba_sdhci_init();
	aruba_i2c_init();
	aruba_regulator_init();
	aruba_touch_init();
	aruba_usb_init();
	aruba_panel_init();
	aruba_sensors_init();
	aruba_bt_rfkill();
	aruba_sata_init();
	tegra_release_bootloader_fb();
	tegra_serial_debug_init(TEGRA_UARTD_BASE, INT_WDT_CPU, NULL, -1, -1);
}

static void __init tegra_aruba_reserve(void)
{
#if defined(CONFIG_NVMAP_CONVERT_CARVEOUT_TO_IOVMM)
	tegra_reserve(0, SZ_4M, 0);
#else
	tegra_reserve(SZ_32M, SZ_4M, 0);
#endif
}

MACHINE_START(ARUBA, "aruba")
	.boot_params    = 0x80000100,
	.map_io         = tegra_map_common_io,
	.reserve        = tegra_aruba_reserve,
	.init_early	= tegra_init_early,
	.init_irq       = tegra_init_irq,
	.handle_irq	= gic_handle_irq,
	.timer          = &tegra_timer,
	.init_machine   = tegra_aruba_init,
MACHINE_END
