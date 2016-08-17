/*
 * arch/arm/mach-tegra/board-harmony.c
 *
 * Copyright (C) 2010 Google, Inc.
 * Copyright (C) 2011 NVIDIA, Inc.
 * Copyright (c) 2012, NVIDIA CORPORATION. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/serial_8250.h>
#include <linux/clk.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/dma-mapping.h>
#include <linux/pda_power.h>
#include <linux/input.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/i2c.h>
#include <linux/i2c-tegra.h>
#include <linux/memblock.h>
#include <linux/delay.h>
#include <linux/mfd/tps6586x.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/tegra_uart.h>

#include <sound/wm8903.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <asm/hardware/gic.h>
#include <asm/setup.h>

#include <mach/tegra_asoc_pdata.h>
#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/sdhci.h>
#include <mach/nand.h>
#include <mach/clk.h>
#include <mach/usb_phy.h>
#include <mach/pinmux-tegra20.h>

#include "clock.h"
#include "board.h"
#include "board-harmony.h"
#include "clock.h"
#include "devices.h"
#include "gpio-names.h"
#include "pm.h"
#include "common.h"

/* NVidia bootloader tags */
#define ATAG_NVIDIA		0x41000801

#define ATAG_NVIDIA_RM			0x1
#define ATAG_NVIDIA_DISPLAY		0x2
#define ATAG_NVIDIA_FRAMEBUFFER		0x3
#define ATAG_NVIDIA_CHIPSHMOO		0x4
#define ATAG_NVIDIA_CHIPSHMOOPHYS	0x5
#define ATAG_NVIDIA_PRESERVED_MEM_0	0x10000
#define ATAG_NVIDIA_PRESERVED_MEM_N	2
#define ATAG_NVIDIA_FORCE_32		0x7fffffff

struct tag_tegra {
	__u32 bootarg_key;
	__u32 bootarg_len;
	char bootarg[1];
};

static int __init parse_tag_nvidia(const struct tag *tag)
{

	return 0;
}
__tagtable(ATAG_NVIDIA, parse_tag_nvidia);


static struct tegra_usb_platform_data tegra_udc_pdata = {
	.port_otg = false,
	.has_hostpc = false,
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

static struct tegra_usb_platform_data tegra_ehci3_utmi_pdata = {
	.port_otg = false,
	.has_hostpc = false,
	.phy_intf = TEGRA_USB_PHY_INTF_UTMI,
	.op_mode	= TEGRA_USB_OPMODE_HOST,
	.u_data.host = {
		.vbus_gpio = TEGRA_GPIO_PD3,
		.hot_plug = true,
		.remote_wakeup_supported = false,
		.power_off_on_suspend = true,
	},
	.u_cfg.utmi = {
		.hssync_start_delay = 9,
		.elastic_limit = 16,
		.idle_wait_delay = 17,
		.term_range_adj = 6,
		.xcvr_setup = 8,
		.xcvr_lsfslew = 2,
		.xcvr_lsrslew = 2,
	},
};

static struct tegra_nand_chip_parms nand_chip_parms[] = {
	/* Samsung K5E2G1GACM */
	[0] = {
	       .vendor_id = 0xEC,
	       .device_id = 0xAA,
	       .read_id_fourth_byte = 0x15,
	       .capacity  = 256,
	       .timing = {
			  .trp = 21,
			  .trh = 15,
			  .twp = 21,
			  .twh = 15,
			  .tcs = 31,
			  .twhr = 60,
			  .tcr_tar_trr = 20,
			  .twb = 100,
			  .trp_resp = 30,
			  .tadl = 100,
			  },
	       },
	/* Hynix H5PS1GB3EFR */
	[1] = {
	       .vendor_id = 0xAD,
	       .device_id = 0xDC,
	       .read_id_fourth_byte = 0x95,
	       .capacity  = 512,
	       .timing = {
			  .trp = 12,
			  .trh = 10,
			  .twp = 12,
			  .twh = 10,
			  .tcs = 20,
			  .twhr = 80,
			  .tcr_tar_trr = 20,
			  .twb = 100,
			  .trp_resp = 20,
			  .tadl = 70,
			  },
	       },
};

struct tegra_nand_platform harmony_nand_data = {
	.max_chips = 8,
	.chip_parms = nand_chip_parms,
	.nr_chip_parms = ARRAY_SIZE(nand_chip_parms),
	.wp_gpio = TEGRA_GPIO_PC7,
};

static struct resource resources_nand[] = {
	[0] = {
	       .start = INT_NANDFLASH,
	       .end = INT_NANDFLASH,
	       .flags = IORESOURCE_IRQ,
	       },
};

struct platform_device tegra_nand_device = {
	.name = "tegra_nand",
	.id = -1,
	.num_resources = ARRAY_SIZE(resources_nand),
	.resource = resources_nand,
	.dev = {
		.platform_data = &harmony_nand_data,
		},
};

static struct gpio_keys_button harmony_gpio_keys_buttons[] = {
	{
		.code		= KEY_POWER,
		.gpio		= TEGRA_GPIO_POWERKEY,
		.active_low	= 1,
		.desc		= "Power",
		.type		= EV_KEY,
		.wakeup		= 1,
	},
};

static struct gpio_keys_platform_data harmony_gpio_keys = {
	.buttons	= harmony_gpio_keys_buttons,
	.nbuttons	= ARRAY_SIZE(harmony_gpio_keys_buttons),
};

static struct platform_device harmony_gpio_keys_device = {
	.name		= "gpio-keys",
	.id		= -1,
	.dev		= {
		.platform_data = &harmony_gpio_keys,
	}
};

static struct tegra_asoc_platform_data harmony_audio_pdata = {
	.gpio_spkr_en		= TEGRA_GPIO_SPKR_EN,
	.gpio_hp_det		= TEGRA_GPIO_HP_DET,
	.gpio_hp_mute		= -1,
	.gpio_int_mic_en	= TEGRA_GPIO_INT_MIC_EN,
	.gpio_ext_mic_en	= TEGRA_GPIO_EXT_MIC_EN,
	.i2s_param[HIFI_CODEC]	= {
		.audio_port_id	= 0,
		.is_i2s_master	= 1,
		.i2s_mode	= TEGRA_DAIFMT_I2S,
	},
	.i2s_param[BASEBAND]	= {
		.audio_port_id	= -1,
	},
	.i2s_param[BT_SCO]	= {
		.audio_port_id	= 3,
		.is_i2s_master	= 1,
		.i2s_mode	= TEGRA_DAIFMT_DSP_A,
	},
};

static struct platform_device harmony_audio_device = {
	.name	= "tegra-snd-wm8903",
	.id	= 0,
	.dev	= {
		.platform_data  = &harmony_audio_pdata,
	},
};

static struct tegra_i2c_platform_data harmony_i2c1_platform_data = {
	.adapter_nr     = 0,
	.bus_count      = 1,
	.bus_clk_rate   = { 400000, 0 },
};

static const struct tegra_pingroup_config i2c2_ddc = {
	.pingroup       = TEGRA_PINGROUP_DDC,
	.func           = TEGRA_MUX_I2C2,
};

static const struct tegra_pingroup_config i2c2_gen2 = {
	.pingroup       = TEGRA_PINGROUP_PTA,
	.func           = TEGRA_MUX_I2C2,
};

static struct tegra_i2c_platform_data harmony_i2c2_platform_data = {
	.adapter_nr     = 1,
	.bus_count      = 2,
	.bus_clk_rate   = { 100000, 100000 },
	.bus_mux        = { &i2c2_ddc, &i2c2_gen2 },
	.bus_mux_len    = { 1, 1 },
};

static struct tegra_i2c_platform_data harmony_i2c3_platform_data = {
	.adapter_nr     = 3,
	.bus_count      = 1,
	.bus_clk_rate   = { 400000, 0 },
};

static struct tegra_i2c_platform_data harmony_dvc_platform_data = {
	.adapter_nr     = 4,
	.bus_count      = 1,
	.bus_clk_rate   = { 400000, 0 },
	.is_dvc         = true,
};

static struct wm8903_platform_data harmony_wm8903_pdata = {
	.irq_active_low = 0,
	.micdet_cfg = 0,
	.micdet_delay = 100,
	.gpio_base = HARMONY_GPIO_WM8903(0),
	.gpio_cfg = {
		0,
		0,
		WM8903_GPIO_CONFIG_ZERO,
		0,
		0,
	},
};

static struct i2c_board_info __initdata wm8903_board_info = {
	I2C_BOARD_INFO("wm8903", 0x1a),
	.platform_data = &harmony_wm8903_pdata,
};

static void __init harmony_i2c_init(void)
{
	tegra_i2c_device1.dev.platform_data = &harmony_i2c1_platform_data;
	tegra_i2c_device2.dev.platform_data = &harmony_i2c2_platform_data;
	tegra_i2c_device3.dev.platform_data = &harmony_i2c3_platform_data;
	tegra_i2c_device4.dev.platform_data = &harmony_dvc_platform_data;

	platform_device_register(&tegra_i2c_device1);
	platform_device_register(&tegra_i2c_device2);
	platform_device_register(&tegra_i2c_device3);
	platform_device_register(&tegra_i2c_device4);

	wm8903_board_info.irq = gpio_to_irq(TEGRA_GPIO_CDC_IRQ);
	i2c_register_board_info(0, &wm8903_board_info, 1);
}


/* PDA power */
static struct pda_power_pdata pda_power_pdata = {
};

static struct platform_device pda_power_device = {
	.name   = "pda_power",
	.id     = -1,
	.dev    = {
		.platform_data  = &pda_power_pdata,
	},
};

static struct platform_device *harmony_uart_devices[] __initdata = {
	&tegra_uartd_device,
};

static struct uart_clk_parent uart_parent_clk[] __initdata = {
	[0] = {.name = "pll_p"},
	[1] = {.name = "pll_m"},
	[2] = {.name = "clk_m"},
};

static struct tegra_uart_platform_data harmony_uart_pdata;

static void __init uart_debug_init(void)
{
	unsigned long rate;
	struct clk *c;

	/* UARTD is the debug port. */
	pr_info("Selecting UARTD as the debug console\n");
	harmony_uart_devices[0] = &debug_uartd_device;
	debug_uart_port_base = ((struct plat_serial8250_port *)(
			debug_uartd_device.dev.platform_data))->mapbase;
	debug_uart_clk = clk_get_sys("serial8250.0", "uartd");

	/* Clock enable for the debug channel */
	if (!IS_ERR_OR_NULL(debug_uart_clk)) {
		rate = ((struct plat_serial8250_port *)(
			debug_uartd_device.dev.platform_data))->uartclk;
		pr_info("The debug console clock name is %s\n",
						debug_uart_clk->name);
		c = tegra_get_clock_by_name("pll_p");
		if (IS_ERR_OR_NULL(c))
			pr_err("Not getting the parent clock pll_p\n");
		else
			clk_set_parent(debug_uart_clk, c);

		clk_enable(debug_uart_clk);
		clk_set_rate(debug_uart_clk, rate);
	} else {
		pr_err("Not getting the clock %s for debug console\n",
					debug_uart_clk->name);
	}
}

static void __init harmony_uart_init(void)
{
	int i;
	struct clk *c;

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
	harmony_uart_pdata.parent_clk_list = uart_parent_clk;
	harmony_uart_pdata.parent_clk_count = ARRAY_SIZE(uart_parent_clk);
	tegra_uartd_device.dev.platform_data = &harmony_uart_pdata;

	/* Register low speed only if it is selected */
	if (!is_tegra_debug_uartport_hs())
		uart_debug_init();

	platform_add_devices(harmony_uart_devices,
				ARRAY_SIZE(harmony_uart_devices));
}

static struct platform_device *harmony_devices[] __initdata = {
	&tegra_sdhci_device1,
	&tegra_sdhci_device2,
	&tegra_sdhci_device4,
	&tegra_ehci3_device,
	&tegra_i2s_device1,
	&tegra_i2s_device2,
	&tegra_spdif_device,
	&tegra_das_device,
	&spdif_dit_device,
	&bluetooth_dit_device,
	&tegra_pcm_device,
	&harmony_audio_device,
	&tegra_pmu_device,
	&tegra_nand_device,
	&tegra_udc_device,
	&harmony_gpio_keys_device,
	&pda_power_device,
	&tegra_ehci3_device,
	&tegra_spi_device1,
	&tegra_spi_device2,
	&tegra_spi_device3,
	&tegra_spi_device4,
	&tegra_gart_device,
	&tegra_avp_device,
	&tegra_emc_device,
};

static void __init tegra_harmony_fixup(struct tag *tags, char **cmdline,
	struct meminfo *mi)
{
	mi->nr_banks = 2;
	mi->bank[0].start = PHYS_OFFSET;
	mi->bank[0].size = 448 * SZ_1M;
	mi->bank[1].start = SZ_512M;
	mi->bank[1].size = SZ_512M;
}

static __initdata struct tegra_clk_init_table harmony_clk_init_table[] = {
	/* name		parent		rate		enabled */
	{ "uartd",	"pll_p",	216000000,	true },
	{ "i2s1",	"pll_a_out0",	0,		false},
	{ "usb3",	"clk_m",	12000000,	true },
	{ "spdif_out",	"pll_a_out0",	0,		false},
	{ "sdmmc1",	"clk_m",	48000000,	true },
	{ "sdmmc2",	"clk_m",	48000000,	true },
	{ "sdmmc4",	"clk_m",	48000000,	true },
	{ "ndflash",	"pll_p",	108000000,	true},
	{ "pwm",	"clk_32k",	32768,		false},
	{ NULL,		NULL,		0,		0},
};


static struct tegra_sdhci_platform_data sdhci_pdata1 = {
	.cd_gpio	= -1,
	.wp_gpio	= -1,
	.power_gpio	= -1,
};

static struct tegra_sdhci_platform_data sdhci_pdata2 = {
	.cd_gpio	= TEGRA_GPIO_SD2_CD,
	.wp_gpio	= TEGRA_GPIO_SD2_WP,
	.power_gpio	= TEGRA_GPIO_SD2_POWER,
};

static struct tegra_sdhci_platform_data sdhci_pdata4 = {
	.cd_gpio	= TEGRA_GPIO_SD4_CD,
	.wp_gpio	= TEGRA_GPIO_SD4_WP,
	.power_gpio	= TEGRA_GPIO_SD4_POWER,
	.is_8bit	= 1,
};

#ifdef CONFIG_TEGRA_PREPOWER_WIFI
static int __init harmony_wifi_prepower(void)
{
        int gpio_pwr, gpio_rst;

	if (!machine_is_harmony())
		return 0;

        /* WLAN - Power up (low) and Reset (low) */
        gpio_pwr = gpio_request(TEGRA_GPIO_WLAN_PWR_LOW, "wlan_pwr");
        gpio_rst = gpio_request(TEGRA_GPIO_WLAN_RST_LOW, "wlan_rst");
        if (gpio_pwr < 0 || gpio_rst < 0)
                pr_warning("Unable to get gpio for WLAN Power and Reset\n");
        else {

                /* toggle in this order as per spec */
                gpio_direction_output(TEGRA_GPIO_WLAN_PWR_LOW, 0);
                gpio_direction_output(TEGRA_GPIO_WLAN_RST_LOW, 0);
		udelay(5);
                gpio_direction_output(TEGRA_GPIO_WLAN_PWR_LOW, 1);
                gpio_direction_output(TEGRA_GPIO_WLAN_RST_LOW, 1);
        }

	return 0;
}

/*
 * subsys_initcall_sync is good synch point to call harmony_wifi_init
 * This makes sure that the required regulators (LDO3
 * supply of external PMU and 1.2V regulator) are properly enabled,
 * and mmc driver has not yet probed for a device on SDIO bus.
 */
subsys_initcall_sync(harmony_wifi_prepower);
#endif

static void __init tegra_harmony_init(void)
{
	tegra_clk_init_from_table(harmony_clk_init_table);
	tegra_soc_device_init("harmony");
	harmony_pinmux_init();

	harmony_uart_init();

	tegra_sdhci_device1.dev.platform_data = &sdhci_pdata1;
	tegra_sdhci_device2.dev.platform_data = &sdhci_pdata2;
	tegra_sdhci_device4.dev.platform_data = &sdhci_pdata4;

	tegra_udc_device.dev.platform_data = &tegra_udc_pdata;
	tegra_ehci3_device.dev.platform_data = &tegra_ehci3_utmi_pdata;

	platform_add_devices(harmony_devices, ARRAY_SIZE(harmony_devices));
	harmony_i2c_init();
	harmony_regulator_init();
	harmony_suspend_init();
	harmony_panel_init();
#ifdef CONFIG_KEYBOARD_TEGRA
	harmony_kbc_init();
#endif
#ifdef CONFIG_TEGRA_PCI
	harmony_pcie_init();
#endif
}

void __init tegra_harmony_reserve(void)
{
	if (memblock_reserve(0x0, 4096) < 0)
		pr_warn("Cannot reserve first 4K of memory for safety\n");

	tegra_reserve(SZ_128M, SZ_8M, SZ_16M);
}

MACHINE_START(HARMONY, "harmony")
	.atag_offset	= 0x100,
	.soc		= &tegra_soc_desc,
	.fixup		= tegra_harmony_fixup,
	.map_io         = tegra_map_common_io,
	.reserve        = tegra_harmony_reserve,
	.init_early	= tegra20_init_early,
	.init_irq       = tegra_init_irq,
	.handle_irq	= gic_handle_irq,
	.timer          = &tegra_timer,
	.init_machine   = tegra_harmony_init,
	.restart	= tegra_assert_system_reset,
MACHINE_END
