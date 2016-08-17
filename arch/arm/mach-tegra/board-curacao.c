/*
 * arch/arm/mach-tegra/board-curacao.c
 *
 * Copyright (c) 2011-2012, NVIDIA Corporation.
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
#include <linux/tegra_uart.h>
#include <linux/i2c.h>
#include <linux/i2c/panjit_ts.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/i2c-tegra.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/of_platform.h>

#include <mach/clk.h>
#include <mach/gpio-tegra.h>
#include <mach/iomap.h>
#include <mach/io_dpd.h>
#include <mach/irqs.h>
#include <mach/pinmux.h>
#include <mach/iomap.h>
#include <mach/io.h>
#include <mach/i2s.h>
#include <mach/audio.h>
#include <mach/usb_phy.h>
#include <mach/nand.h>
#include <mach/hardware.h>

#include <asm/hardware/gic.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include "board.h"
#include "board-curacao.h"
#include "clock.h"
#include "common.h"
#include "devices.h"
#include "fuse.h"
#include "gpio-names.h"
#include <mach/i2s.h>
#include <mach/tegra_wm8903_pdata.h>
#include <sound/wm8903.h>

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


#ifdef CONFIG_BCM4329_RFKILL

static struct resource curacao_bcm4329_rfkill_resources[] = {
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

static struct platform_device curacao_bcm4329_rfkill_device = {
	.name = "bcm4329_rfkill",
	.id             = -1,
	.num_resources  = ARRAY_SIZE(curacao_bcm4329_rfkill_resources),
	.resource       = curacao_bcm4329_rfkill_resources,
};

static noinline void __init curacao_bt_rfkill(void)
{
	/*Add Clock Resource*/
	clk_add_alias("bcm4329_32k_clk", curacao_bcm4329_rfkill_device.name, \
				"blink", NULL);

	platform_device_register(&curacao_bcm4329_rfkill_device);

	return;
}
#else
static inline void curacao_bt_rfkill(void) { }
#endif

static __initdata struct tegra_clk_init_table curacao_clk_init_table[] = {
	/* name		parent		rate		enabled */
	{ "uarta_dbg",	"clk_m",	13000000,	true},
	{ "uartb_dbg",	"clk_m",	13000000,	true},
	{ "uartc_dbg",	"clk_m",	13000000,	true},
	{ "uartd_dbg",	"clk_m",	13000000,	true},
	{ "uarte_dbg",	"clk_m",	13000000,	true},
	{ "sdmmc1",	"clk_m",	13000000,	false},
	{ "sdmmc3",	"clk_m",	13000000,	false},
	{ "sdmmc4",	"clk_m",	13000000,	false},
	{ "blink",	"clk_32k",	32768,		false},
	{ "pwm",	"clk_32k",	32768,		false},
	{ "blink",	"clk_32k",	32768,		false},
	{ "pll_a",	NULL,		282240000,	true},
	{ "pll_a_out0",	NULL,		11289600,	true},
	{ "i2s0",	"clk_m",	13000000,	true},
	{ "i2s1",	"pll_a_out0",	11289600,	true},
	{ "i2s2",	"pll_a_out0",	11289600,	true},
	{ "d_audio",	"pll_a_out0",	11289600,	false},
	{ "audio",	"pll_a_out0",	11289600,	false},
	{ "audio_2x",	"audio",	22579200,	true},
	{ NULL,		NULL,		0,		0},
};


static struct tegra_i2c_platform_data curacao_i2c1_platform_data = {
	.adapter_nr	= 0,
	.bus_count	= 1,
	.bus_clk_rate	= { 100000, 0 },
};

#if 0	/* !!!FIXME!!! */
static const struct tegra_pingroup_config i2c2_ddc = {
	.pingroup	= TEGRA_PINGROUP_DDC,
	.func		= TEGRA_MUX_I2C2,
};

static const struct tegra_pingroup_config i2c2_gen2 = {
	.pingroup	= TEGRA_PINGROUP_PTA,
	.func		= TEGRA_MUX_I2C2,
};
#endif

static struct tegra_i2c_platform_data curacao_i2c2_platform_data = {
	.adapter_nr	= 2,
	.bus_count	= 1,
	.bus_clk_rate	= { 100000, 0 },
};

static struct tegra_i2c_platform_data curacao_i2c3_platform_data = {
	.adapter_nr	= 3,
	.bus_count	= 1,
	.bus_clk_rate	= { 100000, 0 },
};

static struct tegra_i2c_platform_data curacao_i2c4_platform_data = {
	.adapter_nr	= 4,
	.bus_count	= 1,
	.bus_clk_rate	= { 100000, 0 },
};

static struct tegra_i2c_platform_data curacao_i2c5_platform_data = {
	.adapter_nr	= 5,
	.bus_count	= 1,
	.bus_clk_rate	= { 100000, 0 },
};

static struct wm8903_platform_data curacao_wm8903_pdata = {
	.irq_active_low = 0,
	.micdet_cfg = 0,
	.micdet_delay = 100,
};

static struct i2c_board_info __initdata wm8903_board_info = {
	I2C_BOARD_INFO("wm8903", 0x1a),
	.platform_data = &curacao_wm8903_pdata,
};

static void curacao_i2c_init(void)
{
	tegra11_i2c_device1.dev.platform_data = &curacao_i2c1_platform_data;
	tegra11_i2c_device2.dev.platform_data = &curacao_i2c2_platform_data;
	tegra11_i2c_device3.dev.platform_data = &curacao_i2c3_platform_data;
	tegra11_i2c_device4.dev.platform_data = &curacao_i2c4_platform_data;
	tegra11_i2c_device5.dev.platform_data = &curacao_i2c5_platform_data;

	i2c_register_board_info(0, &wm8903_board_info, 1);

	platform_device_register(&tegra11_i2c_device5);
	platform_device_register(&tegra11_i2c_device4);
	platform_device_register(&tegra11_i2c_device3);
	platform_device_register(&tegra11_i2c_device2);
	platform_device_register(&tegra11_i2c_device1);
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

/* !!!FIXME!!! */
static struct gpio_keys_button curacao_keys[] = {
	[0] = GPIO_KEY(KEY_MENU, PQ0, 0),
	[1] = GPIO_KEY(KEY_HOME, PQ1, 0),
	[2] = GPIO_KEY(KEY_BACK, PQ2, 0),
	[3] = GPIO_KEY(KEY_VOLUMEUP, PQ3, 0),
	[4] = GPIO_KEY(KEY_VOLUMEDOWN, PQ4, 0),
	[5] = GPIO_KEY(KEY_POWER, PV2, 1),
};

static struct gpio_keys_platform_data curacao_keys_platform_data = {
	.buttons	= curacao_keys,
	.nbuttons	= ARRAY_SIZE(curacao_keys),
};

static struct platform_device curacao_keys_device = {
	.name	= "gpio-keys",
	.id	= 0,
	.dev	= {
		.platform_data	= &curacao_keys_platform_data,
	},
};

static struct platform_device tegra_camera = {
	.name = "tegra_camera",
	.id = -1,
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

static struct tegra_wm8903_platform_data curacao_audio_pdata = {
	.gpio_spkr_en		= -1,
	.gpio_hp_det		= -1,
	.gpio_hp_mute		= -1,
	.gpio_int_mic_en	= -1,
	.gpio_ext_mic_en	= -1,
};

static struct platform_device curacao_audio_device = {
	.name	= "tegra-snd-wm8903",
	.id	= 0,
	.dev	= {
		.platform_data  = &curacao_audio_pdata,
	},
};
#if defined(CONFIG_TEGRA_SIMULATION_PLATFORM) && defined(CONFIG_SMC91X)
static struct resource tegra_sim_smc91x_resources[] = {
	[0] = {
		.start		= TEGRA_SIM_ETH_BASE,
		.end		= TEGRA_SIM_ETH_BASE + TEGRA_SIM_ETH_SIZE - 1,
		.flags		= IORESOURCE_MEM,
	},
	[1] = {
		.start		= IRQ_ETH,
		.end		= IRQ_ETH,
		.flags		= IORESOURCE_IRQ,
	},
};

static struct platform_device tegra_sim_smc91x_device = {
	.name		= "smc91x",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(tegra_sim_smc91x_resources),
	.resource	= tegra_sim_smc91x_resources,
};
#endif

static struct platform_device *curacao_devices[] __initdata = {
#if ENABLE_OTG
	&tegra_otg_device,
#endif
	&debug_uart,
	&tegra_pmu_device,
	&tegra_rtc_device,
	&tegra_udc_device,
#if defined(CONFIG_TEGRA_IOVMM_SMMU) || defined(CONFIG_TEGRA_IOMMU_SMMU)
	&tegra_smmu_device,
#endif
	&curacao_keys_device,
#if defined(CONFIG_SND_HDA_TEGRA)
	&tegra_hda_device,
#endif
	&tegra_avp_device,
	&tegra_camera,
#if defined(CONFIG_CRYPTO_DEV_TEGRA_SE)
	&tegra11_se_device,
#endif
	&tegra_ahub_device,
	&tegra_dam_device0,
	&tegra_i2s_device0,
	&tegra_pcm_device,
	&curacao_audio_device,
#if defined(CONFIG_MTD_NAND_TEGRA)
	&tegra_nand_device,
#endif

#if defined(CONFIG_TEGRA_SIMULATION_PLATFORM) && defined(CONFIG_SMC91X)
	&tegra_sim_smc91x_device,
#endif
};

static int __init curacao_touch_init(void)
{
	return 0;
}


#if defined(USB_HOST_ONLY)
static struct tegra_usb_platform_data tegra_ehci1_utmi_pdata = {
	.port_otg = true,
	.has_hostpc = true,
	.unaligned_dma_buf_supported = true,
	.phy_intf = TEGRA_USB_PHY_INTF_UTMI,
	.op_mode = TEGRA_USB_OPMODE_HOST,
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


static void ulpi_link_platform_open(void)
{
	int reset_gpio = TEGRA_GPIO_PV1;

	gpio_request(reset_gpio, "ulpi_phy_reset");
	gpio_direction_output(reset_gpio, 0);
	msleep(5);
	gpio_direction_output(reset_gpio, 1);
}

static struct tegra_usb_phy_platform_ops ulpi_link_plat_ops = {
	.open = ulpi_link_platform_open,
};

static struct tegra_usb_platform_data tegra_ehci2_ulpi_link_pdata = {
	.port_otg = false,
	.has_hostpc = true,
	.unaligned_dma_buf_supported = true,
	.phy_intf = TEGRA_USB_PHY_INTF_ULPI_LINK,
	.op_mode	= TEGRA_USB_OPMODE_HOST,
	.u_data.host = {
		.vbus_gpio = -1,
		.hot_plug = false,
		.remote_wakeup_supported = false,
		.power_off_on_suspend = true,
	},
	.u_cfg.ulpi = {
		.shadow_clk_delay = 10,
		.clock_out_delay = 1,
		.data_trimmer = 4,
		.stpdirnxt_trimmer = 4,
		.dir_trimmer = 4,
		.clk = "cdev2",
	},
	.ops = &ulpi_link_plat_ops,
};
#else

static struct tegra_usb_platform_data tegra_udc_pdata = {
	.port_otg = true,
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

#endif

static void curacao_usb_init(void)
{
#if defined(USB_HOST_ONLY)
	tegra_ehci1_device.dev.platform_data = &tegra_ehci1_utmi_pdata;
	platform_device_register(&tegra_ehci1_device);

	tegra_ehci2_device.dev.platform_data = &tegra_ehci2_ulpi_link_pdata;
	platform_device_register(&tegra_ehci2_device);
#else
	/* setup the udc platform data */
	tegra_udc_device.dev.platform_data = &tegra_udc_pdata;

#endif
}

static struct platform_device *curacao_hs_uart_devices[] __initdata = {
	&tegra_uartb_device,
	&tegra_uartc_device,
	&tegra_uartd_device,
	&tegra_uarte_device,
};

static struct uart_clk_parent uart_parent_clk[] = {
	[0] = {.name = "clk_m"},
};

static struct tegra_uart_platform_data curacao_uart_pdata;
static struct tegra_uart_platform_data curacao_loopback_uart_pdata;

static void __init curacao_hs_uart_init(void)
{
	struct clk *c;
	int i;

	for (i = 0; i < ARRAY_SIZE(uart_parent_clk); ++i) {
		c = tegra_get_clock_by_name(uart_parent_clk[i].name);
		if (IS_ERR_OR_NULL(c)) {
			pr_err("Not able to get the clock for %s\n",
					uart_parent_clk[i].name);
			continue;
		}
		uart_parent_clk[i].parent_clk = c;
		uart_parent_clk[i].fixed_clk_rate = clk_get_rate(c);
	}
	curacao_uart_pdata.parent_clk_list = uart_parent_clk;
	curacao_loopback_uart_pdata.parent_clk_list = uart_parent_clk;
	curacao_uart_pdata.parent_clk_count = ARRAY_SIZE(uart_parent_clk);
	curacao_loopback_uart_pdata.parent_clk_count =
						ARRAY_SIZE(uart_parent_clk);
	curacao_loopback_uart_pdata.is_loopback = true;
	tegra_uartb_device.dev.platform_data = &curacao_uart_pdata;
	tegra_uartc_device.dev.platform_data = &curacao_uart_pdata;
	tegra_uartd_device.dev.platform_data = &curacao_uart_pdata;
	tegra_uarte_device.dev.platform_data = &curacao_loopback_uart_pdata;
	platform_add_devices(curacao_hs_uart_devices,
				ARRAY_SIZE(curacao_hs_uart_devices));
}

static void __init tegra_curacao_init(void)
{
	tegra_clk_init_from_table(curacao_clk_init_table);
	tegra_enable_pinmux();
	curacao_pinmux_init();
	tegra_soc_device_init("curacao");

	if (tegra_revision == TEGRA_REVISION_QT)
		debug_uart_platform_data[0].uartclk = tegra_clk_measure_input_freq();

	platform_add_devices(curacao_devices, ARRAY_SIZE(curacao_devices));

	curacao_power_off_init();
	tegra_io_dpd_init();
	curacao_sdhci_init();
	curacao_i2c_init();
	curacao_regulator_init();
	curacao_emc_init();
	curacao_suspend_init();
	curacao_touch_init();
	curacao_usb_init();
	curacao_panel_init();
	curacao_hs_uart_init();
	curacao_bt_rfkill();
	curacao_sensors_init();
}

static void __init tegra_curacao_dt_init(void)
{
	tegra_curacao_init();

#ifdef CONFIG_USE_OF
	of_platform_populate(NULL,
		of_default_bus_match_table, NULL, NULL);
#endif
}

static void __init tegra_curacao_reserve(void)
{
#if defined(CONFIG_NVMAP_CONVERT_CARVEOUT_TO_IOVMM)
	tegra_reserve(0, SZ_4M, 0);
#else
	tegra_reserve(SZ_32M, SZ_4M, 0);
#endif
}

static const char * const curacao_dt_board_compat[] = {
	"nvidia,curacao",
	NULL
};

MACHINE_START(CURACAO, CURACAO_BOARD_NAME)
	.atag_offset    = 0x80000100,
	.soc		= &tegra_soc_desc,
	.map_io         = tegra_map_common_io,
	.reserve        = tegra_curacao_reserve,
	.init_early	= tegra11x_init_early,
	.init_irq       = tegra_init_irq,
	.handle_irq	= gic_handle_irq,
	.timer          = &tegra_timer,
	.init_machine   = tegra_curacao_dt_init,
	.dt_compat	= curacao_dt_board_compat,
MACHINE_END
